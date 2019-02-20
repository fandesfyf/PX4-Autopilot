/****************************************************************************
 *
 *   Copyright (c) 2017 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef COMMANDER_HPP_
#define COMMANDER_HPP_

#include "state_machine_helper.h"
#include "failure_detector/FailureDetector.hpp"

#include <lib/controllib/blocks.hpp>
#include <lib/mathlib/mathlib.h>
#include <px4_module.h>
#include <px4_module_params.h>
#include <systemlib/hysteresis/hysteresis.h>

// publications
#include <uORB/Publication.hpp>
#include <uORB/topics/actuator_armed.h>
#include <uORB/topics/home_position.h>
#include <uORB/topics/vehicle_status.h>

// subscriptions
#include <uORB/Subscription.hpp>
#include <uORB/topics/estimator_status.h>
#include <uORB/topics/iridiumsbd_status.h>
#include <uORB/topics/mission_result.h>
#include <uORB/topics/vehicle_command.h>
#include <uORB/topics/vehicle_global_position.h>
#include <uORB/topics/vehicle_local_position.h>

using math::constrain;
using uORB::Publication;
using uORB::Subscription;

using namespace time_literals;

class Commander : public ModuleBase<Commander>, public ModuleParams
{
public:
	Commander();
	~Commander();

	/** @see ModuleBase */
	static int task_spawn(int argc, char *argv[]);

	/** @see ModuleBase */
	static Commander *instantiate(int argc, char *argv[]);

	/** @see ModuleBase */
	static int custom_command(int argc, char *argv[]);

	/** @see ModuleBase */
	static int print_usage(const char *reason = nullptr);

	/** @see ModuleBase::run() */
	void run() override;

	void enable_hil();

	// TODO: only temporarily static until low priority thread is removed
	static bool preflight_check(bool report);

private:

	DEFINE_PARAMETERS(

		(ParamInt<px4::params::NAV_DLL_ACT>) _datalink_loss_action,
		(ParamInt<px4::params::COM_DL_LOSS_T>) _datalink_loss_threshold,
		(ParamInt<px4::params::COM_DL_REG_T>) _datalink_regain_threshold,

		(ParamInt<px4::params::COM_HLDL_LOSS_T>) _high_latency_datalink_loss_threshold,
		(ParamInt<px4::params::COM_HLDL_REG_T>) _high_latency_datalink_regain_threshold,

		(ParamInt<px4::params::NAV_RCL_ACT>) _rc_loss_action,
		(ParamFloat<px4::params::COM_RC_LOSS_T>) _rc_loss_threshold,

		(ParamFloat<px4::params::COM_HOME_H_T>) _home_eph_threshold,
		(ParamFloat<px4::params::COM_HOME_V_T>) _home_epv_threshold,

		(ParamFloat<px4::params::COM_POS_FS_EPH>) _eph_threshold,
		(ParamFloat<px4::params::COM_POS_FS_EPV>) _epv_threshold,
		(ParamFloat<px4::params::COM_VEL_FS_EVH>) _evh_threshold,

		(ParamInt<px4::params::COM_POS_FS_DELAY>) _failsafe_pos_delay,
		(ParamInt<px4::params::COM_POS_FS_PROB>) _failsafe_pos_probation,
		(ParamInt<px4::params::COM_POS_FS_GAIN>) _failsafe_pos_gain,

		(ParamInt<px4::params::COM_LOW_BAT_ACT>) _low_bat_action,
		(ParamFloat<px4::params::COM_DISARM_LAND>) _disarm_when_landed_timeout,

		(ParamInt<px4::params::COM_OBS_AVOID>) _obs_avoid
	)

	const int64_t POSVEL_PROBATION_MIN = 1_s;	/**< minimum probation duration (usec) */
	const int64_t POSVEL_PROBATION_MAX = 100_s;	/**< maximum probation duration (usec) */

	hrt_abstime	_last_gpos_fail_time_us{0};	/**< Last time that the global position validity recovery check failed (usec) */
	hrt_abstime	_last_lpos_fail_time_us{0};	/**< Last time that the local position validity recovery check failed (usec) */
	hrt_abstime	_last_lvel_fail_time_us{0};	/**< Last time that the local velocity validity recovery check failed (usec) */

	// Probation times for position and velocity validity checks to pass if failed
	hrt_abstime	_gpos_probation_time_us = POSVEL_PROBATION_MIN;
	hrt_abstime	_lpos_probation_time_us = POSVEL_PROBATION_MIN;
	hrt_abstime	_lvel_probation_time_us = POSVEL_PROBATION_MIN;

	/* class variables used to check for navigation failure after takeoff */
	hrt_abstime	_time_at_takeoff{0};		/**< last time we were on the ground */
	hrt_abstime	_time_last_innov_pass{0};	/**< last time velocity or position innovations passed */
	bool		_nav_test_passed{false};	/**< true if the post takeoff navigation test has passed */
	bool		_nav_test_failed{false};	/**< true if the post takeoff navigation test has failed */

	FailureDetector _failure_detector;
	bool _failure_detector_termination_printed{false};

	bool handle_command(vehicle_status_s *status, const vehicle_command_s &cmd, actuator_armed_s *armed,
			    orb_advert_t *command_ack_pub, bool *changed);

	bool set_home_position();
	bool set_home_position_alt_only();

	// Set the main system state based on RC and override device inputs
	transition_result_t set_main_state(const vehicle_status_s &status, bool *changed);

	// Enable override (manual reversion mode) on the system
	transition_result_t set_main_state_override_on(const vehicle_status_s &status, bool *changed);

	// Set the system main state based on the current RC inputs
	transition_result_t set_main_state_rc(const vehicle_status_s &status, bool *changed);

	void check_valid(const hrt_abstime &timestamp, const hrt_abstime &timeout, const bool valid_in, bool *valid_out,
			 bool *changed);

	bool check_posvel_validity(const bool data_valid, const float data_accuracy, const float required_accuracy,
				   const hrt_abstime &data_timestamp_us, hrt_abstime *last_fail_time_us, hrt_abstime *probation_time_us, bool *valid_state,
				   bool *validity_changed);

	void reset_posvel_validity(bool *changed);

	void mission_init();

	/**
	 * Checks the status of all available data links and handles switching between different system telemetry states.
	 */
	void		data_link_check(bool &status_changed);
	int		_telemetry_status_sub[ORB_MULTI_MAX_INSTANCES] {};

	uint64_t	_datalink_last_heartbeat_gcs{0};

	uint64_t	_datalink_last_heartbeat_onboard_controller{0};
	uint64_t	_onboard_controller_lost{0};

	uint64_t	_datalink_last_heartbeat_avoidance_system{0};
	uint64_t	_avoidance_system_lost{0};
	uint64_t	_avoidance_system_not_started{0};
	bool		_avoidance_system_status_change{0};
	uint64_t	_datalink_last_status_avoidance_system{9};

	int			_iridiumsbd_status_sub{-1};
	uint64_t	_high_latency_datalink_heartbeat{0};
	uint64_t	_high_latency_datalink_lost{0};

	void estimator_check(bool *status_changed);

	int _battery_sub{-1};
	uint8_t _battery_warning{battery_status_s::BATTERY_WARNING_NONE};
	float _battery_current{0.0f};

	void battery_status_check();

	systemlib::Hysteresis	_auto_disarm_landed{false};
	systemlib::Hysteresis	_auto_disarm_killed{false};

	// Subscriptions
	Subscription<estimator_status_s>		_estimator_status_sub{ORB_ID(estimator_status)};
	Subscription<mission_result_s>			_mission_result_sub{ORB_ID(mission_result)};
	Subscription<vehicle_global_position_s>		_global_position_sub{ORB_ID(vehicle_global_position)};
	Subscription<vehicle_local_position_s>		_local_position_sub{ORB_ID(vehicle_local_position)};

	Publication<home_position_s>			_home_pub{ORB_ID(home_position)};

	orb_advert_t					_status_pub{nullptr};
};

#endif /* COMMANDER_HPP_ */
