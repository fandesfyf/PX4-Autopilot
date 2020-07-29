/***************************************************************************
 *
 *   Copyright (c) 2013-2020 PX4 Development Team. All rights reserved.
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
/**
 * @file rtl.h
 *
 * Helper class for RTL
 *
 * @author Julian Oes <julian@oes.ch>
 * @author Anton Babushkin <anton.babushkin@me.com>
 */

#pragma once

#include <px4_platform_common/module_params.h>

#include "navigator_mode.h"
#include "mission_block.h"

#include <uORB/Subscription.hpp>
#include <uORB/topics/home_position.h>
#include <uORB/topics/rtl_flight_time.h>
#include <uORB/topics/wind_estimate.h>
#include <matrix/math.hpp>
#include <lib/ecl/geo/geo.h>

class Navigator;

class RTL : public MissionBlock, public ModuleParams
{
public:
	enum RTLType {
		RTL_HOME = 0,
		RTL_LAND,
		RTL_MISSION,
		RTL_CLOSEST,
	};

	enum RTLDestinationType {
		RTL_DESTINATION_HOME = 0,
		RTL_DESTINATION_MISSION_LANDING,
		RTL_DESTINATION_SAFE_POINT,
	};

	RTL(Navigator *navigator);

	~RTL() = default;

	void on_inactivation() override;
	void on_inactive() override;
	void on_activation() override;
	void on_active() override;

	void find_RTL_destination();

	void set_return_alt_min(bool min) { _rtl_alt_min = min; }

	int rtl_type() const { return _param_rtl_type.get(); }

	int rtl_destination();

	void setClimbAndReturnDone(bool done) { _climb_and_return_done = done; }

	bool getClimbAndReturnDone() { return _climb_and_return_done; }

	bool denyMissionLanding() { return _deny_mission_landing; }

private:
	/**
	 * Set the RTL item
	 */
	void set_rtl_item();

	/**
	 * Move to next RTL item
	 */
	void advance_rtl();

	void get_rtl_xy_z_speed(uint8_t vehicle_type, float &xy, float &z);
	matrix::Vector2f get_wind();

	float calculate_return_alt_from_cone_half_angle(float cone_half_angle_deg);

	enum RTLState {
		RTL_STATE_NONE = 0,
		RTL_STATE_CLIMB,
		RTL_STATE_RETURN,
		RTL_STATE_TRANSITION_TO_MC,
		RTL_STATE_DESCEND,
		RTL_STATE_LOITER,
		RTL_MOVE_TO_LAND_HOVER_VTOL,
		RTL_STATE_LAND,
		RTL_STATE_LANDED,
	} _rtl_state{RTL_STATE_NONE};

	struct RTLPosition {
		double lat;
		double lon;
		float alt;
		float yaw;
		uint8_t safe_point_index; ///< 0 = home position, 1 = mission landing, >1 = safe landing points (rally points)
		RTLDestinationType type{RTL_DESTINATION_HOME};

		void set(const home_position_s &home_position)
		{
			lat = home_position.lat;
			lon = home_position.lon;
			alt = home_position.alt;
			yaw = home_position.yaw;
			safe_point_index = 0;
			type = RTL_DESTINATION_HOME;
		}
	};

	RTLPosition _destination{}; ///< the RTL position to fly to (typically the home position or a safe point)

	hrt_abstime _destination_check_time{0};

	float _rtl_alt{0.0f};	// AMSL altitude at which the vehicle should return to the home position
	bool _rtl_alt_min{false};
	bool _climb_and_return_done{false};	// this flag is set to true if RTL is active and we are past the climb state and return state
	bool _deny_mission_landing{false};

	DEFINE_PARAMETERS(
		(ParamFloat<px4::params::RTL_RETURN_ALT>) _param_rtl_return_alt,
		(ParamFloat<px4::params::RTL_DESCEND_ALT>) _param_rtl_descend_alt,
		(ParamFloat<px4::params::RTL_LAND_DELAY>) _param_rtl_land_delay,
		(ParamFloat<px4::params::RTL_MIN_DIST>) _param_rtl_min_dist,
		(ParamInt<px4::params::RTL_TYPE>) _param_rtl_type,
		(ParamInt<px4::params::RTL_CONE_ANG>) _param_rtl_cone_half_angle_deg,
		(ParamFloat<px4::params::RTL_FLT_TIME>) _param_rtl_flt_time,
		(ParamInt<px4::params::RTL_PLD_MD>) _param_rtl_pld_md
	)

	// These need to point at different parameters depending on vehicle type.
	// Can't hard-code them because we have non-MC/FW/Rover builds
	uint8_t _rtl_vehicle_type{255};
	param_t _rtl_xy_speed;
	param_t _rtl_descent_speed;

	uORB::SubscriptionData<wind_estimate_s>		_wind_estimate_sub{ORB_ID(wind_estimate)};
	uORB::PublicationData<rtl_flight_time_s>	_rtl_flight_time_pub{ORB_ID(rtl_flight_time)};

	map_projection_reference_s _projection_reference = {}; ///< reference to convert (lon, lat) to local [m]
};

float time_to_home(const matrix::Vector3f &vehicle_local_pos,
		   const matrix::Vector3f &rtl_point_local_pos,
		   const matrix::Vector2f &wind_velocity, float vehicle_speed_m_s,
		   float vehicle_descent_speed_m_s);
