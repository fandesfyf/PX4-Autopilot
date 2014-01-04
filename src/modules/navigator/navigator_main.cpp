/****************************************************************************
 *
 *   Copyright (c) 2013 PX4 Development Team. All rights reserved.
 *   Author: @author Lorenz Meier <lm@inf.ethz.ch>
 *           @author Jean Cyr <jean.m.cyr@gmail.com>
 *           @author Julian Oes <joes@student.ethz.ch>
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
 * @file navigator_main.c
 * Implementation of the main navigation state machine.
 *
 * Handles missions, geo fencing and failsafe navigation behavior.
 * Published the mission item triplet for the position controller.
 */

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <time.h>
#include <sys/ioctl.h>
#include <drivers/device/device.h>
#include <drivers/drv_hrt.h>
#include <arch/board/board.h>
#include <uORB/uORB.h>
#include <uORB/topics/vehicle_global_position.h>
#include <uORB/topics/home_position.h>
#include <uORB/topics/mission_item_triplet.h>
#include <uORB/topics/mission_result.h>
#include <uORB/topics/vehicle_status.h>
#include <uORB/topics/vehicle_control_mode.h>
#include <uORB/topics/parameter_update.h>
#include <uORB/topics/mission.h>
#include <uORB/topics/fence.h>
#include <uORB/topics/navigation_capabilities.h>
#include <systemlib/param/param.h>
#include <systemlib/err.h>
#include <systemlib/state_table.h>
#include <systemlib/perf_counter.h>
#include <systemlib/systemlib.h>
#include <geo/geo.h>
#include <mathlib/mathlib.h>
#include <dataman/dataman.h>
#include <mavlink/mavlink_log.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "navigator_mission.h"
#include "mission_feasibility_checker.h"
#include "geofence.h"


/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

/**
 * navigator app start / stop handling function
 *
 * @ingroup apps
 */
extern "C" __EXPORT int navigator_main(int argc, char *argv[]);

class Navigator : public StateTable
{
public:
	/**
	 * Constructor
	 */
	Navigator();

	/**
	 * Destructor, also kills the navigators task.
	 */
	~Navigator();

	/**
	* Start the navigator task.
	 *
	 * @return		OK on success.
	 */
	int		start();

	/**
	 * Display the navigator status.
	 */
	void		status();

	/**
	 * Add point to geofence
	 */
	void add_fence_point(int argc, char *argv[]);

	/**
	 * Load fence from file
	 */
	void load_fence_from_file(const char *filename);

private:

	bool		_task_should_exit;		/**< if true, sensor task should exit */
	int		_navigator_task;		/**< task handle for sensor task */

	int		_mavlink_fd;

	int		_global_pos_sub;		/**< global position subscription */
	int		_home_pos_sub;			/**< home position subscription */
	int		_vstatus_sub;			/**< vehicle status subscription */
	int		_params_sub;			/**< notification of parameter updates */
	int		_offboard_mission_sub;		/**< notification of offboard mission updates */
	int 		_onboard_mission_sub;		/**< notification of onboard mission updates */
	int		_capabilities_sub;		/**< notification of vehicle capabilities updates */

	orb_advert_t	_triplet_pub;			/**< publish position setpoint triplet */
	orb_advert_t	_mission_result_pub;		/**< publish mission result topic */
	orb_advert_t	_control_mode_pub;			/**< publish vehicle control mode topic */

	struct vehicle_status_s				_vstatus;		/**< vehicle status */
	struct vehicle_control_mode_s		_control_mode;		/**< vehicle control mode */
	struct vehicle_global_position_s		_global_pos;		/**< global vehicle position */
	struct home_position_s				_home_pos;		/**< home position for RTL */
	struct mission_item_triplet_s			_mission_item_triplet;	/**< triplet of mission items */
	struct mission_result_s				_mission_result;	/**< mission result for commander/mavlink */

	perf_counter_t	_loop_perf;			/**< loop performance counter */
	
	Geofence					_geofence;
	bool						_fence_valid;		/**< flag if fence is valid */
        bool						_inside_fence;		/**< vehicle is inside fence */

	struct navigation_capabilities_s		_nav_caps;

	class 		Mission				_mission;

	bool		_reset_loiter_pos;				/**< if true then loiter position should be set to current position */
	bool		_waypoint_position_reached;
	bool		_waypoint_yaw_reached;
	uint64_t	_time_first_inside_orbit;
	bool		_need_takeoff;		/**< if need to perform vertical takeoff before going to waypoint (only for MISSION mode and VTOL vehicles) */
	bool		_do_takeoff;		/**< vertical takeoff state, current mission item is generated by navigator (only for MISSION mode and VTOL vehicles) */

	MissionFeasibilityChecker missionFeasiblityChecker;

	uint64_t	_set_nav_state_timestamp;		/**< timestamp of last handled navigation state request */

	struct {
		float min_altitude;
		float acceptance_radius;
		float loiter_radius;
		int onboard_mission_enabled;
		float takeoff_alt;
		float land_alt;
		float rtl_alt;
	}		_parameters;			/**< local copies of parameters */

	struct {
		param_t min_altitude;
		param_t acceptance_radius;
		param_t loiter_radius;
		param_t onboard_mission_enabled;
		param_t takeoff_alt;
		param_t land_alt;
		param_t rtl_alt;
	}		_parameter_handles;		/**< handles for parameters */

	enum Event {
		EVENT_NONE_REQUESTED,
		EVENT_READY_REQUESTED,
		EVENT_LOITER_REQUESTED,
		EVENT_MISSION_REQUESTED,
		EVENT_RTL_REQUESTED,
		EVENT_MISSION_CHANGED,
		EVENT_HOME_POSITION_CHANGED,
		MAX_EVENT
	};

	/**
	 * State machine transition table
	 */
	static StateTable::Tran const myTable[NAV_STATE_MAX][MAX_EVENT];

	enum RTLState {
		RTL_STATE_NONE = 0,
		RTL_STATE_CLIMB,
		RTL_STATE_RETURN,
		RTL_STATE_DESCEND,
		RTL_STATE_LAND
	};

	enum RTLState _rtl_state;

	/**
	 * Update our local parameter cache.
	 */
	void		parameters_update();

	/**
	 * Retrieve global position
	 */
	void		global_position_update();

	/**
	 * Retrieve home position
	 */
	void		home_position_update();

	/**
	 * Retreive navigation capabilities
	 */
	void		navigation_capabilities_update();

	/**
	 * Retrieve offboard mission.
	 */
	void		offboard_mission_update(bool isrotaryWing);

	/**
	 * Retrieve onboard mission.
	 */
	void		onboard_mission_update();

	/**
	 * Retrieve vehicle status
	 */
	void		vehicle_status_update();


	/**
	 * Shim for calling task_main from task_create.
	 */
	static void	task_main_trampoline(int argc, char *argv[]);

	/**
	 * Main sensor collection task.
	 */
	void		task_main() __attribute__((noreturn));

	void		publish_safepoints(unsigned points);

	/**
	 * Functions that are triggered when a new state is entered.
	 */
	void		start_none();
	void		start_ready();
	void		start_loiter();
	void		start_mission();
	void		start_rtl();
	void		finish_rtl();

	/**
	 * Guards offboard mission
	 */
	bool		offboard_mission_available(unsigned relative_index);

	/**
	 * Guards onboard mission
	 */
	bool		onboard_mission_available(unsigned relative_index);

	/**
	 * Check if current mission item has been reached.
	 */
	bool		check_mission_item_reached();

	/**
	 * Perform actions when current mission item reached.
	 */
	void		on_mission_item_reached();

	/**
	 * Move to next waypoint
	 */
	void		advance_mission();

	/**
	 * Switch to next RTL state
	 */
	void		set_rtl_item();

	/**
	 * Helper function to get a loiter item
	 */
	void		get_loiter_item(mission_item_s *item);

	/**
	 * Helper function to get a takeoff item
	 */
	void		get_takeoff_item(mission_item_s *item);

	/**
	 * Publish a new mission item triplet for position controller
	 */
	void		publish_mission_item_triplet();

	/**
	 * Publish vehicle_control_mode topic for controllers
	 */
	void		publish_control_mode();


	/**
	 * Compare two mission items if they are equivalent
	 * Two mission items can be considered equivalent for the purpose of the navigator even if some fields differ.
	 *
	 * @return true if equivalent, false otherwise
	 */
	bool		cmp_mission_item_equivalent(const struct mission_item_s a, const struct mission_item_s b);

	void		add_home_pos_to_rtl(struct mission_item_s *new_mission_item);
};

namespace navigator
{

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

Navigator	*g_navigator;
}

Navigator::Navigator() : 

/* state machine transition table */
	StateTable(&myTable[0][0], NAV_STATE_MAX, MAX_EVENT),

	_task_should_exit(false),
	_navigator_task(-1),
	_mavlink_fd(-1),

/* subscriptions */
	_global_pos_sub(-1),
	_home_pos_sub(-1),
	_vstatus_sub(-1),
	_params_sub(-1),
	_offboard_mission_sub(-1),
	_onboard_mission_sub(-1),
	_capabilities_sub(-1),

/* publications */
	_triplet_pub(-1),
	_mission_result_pub(-1),
	_control_mode_pub(-1),

/* performance counters */
	_loop_perf(perf_alloc(PC_ELAPSED, "navigator")),

/* states */
	_rtl_state(RTL_STATE_NONE),
	_fence_valid(false),
	_inside_fence(true),
	_mission(),
	_reset_loiter_pos(true),
	_waypoint_position_reached(false),
	_waypoint_yaw_reached(false),
	_time_first_inside_orbit(0),
	_set_nav_state_timestamp(0),
	_need_takeoff(true),
	_do_takeoff(false)
{
	_parameter_handles.min_altitude = param_find("NAV_MIN_ALT");
	_parameter_handles.acceptance_radius = param_find("NAV_ACCEPT_RAD");
	_parameter_handles.loiter_radius = param_find("NAV_LOITER_RAD");
	_parameter_handles.onboard_mission_enabled = param_find("NAV_ONB_MIS_EN");
	_parameter_handles.takeoff_alt = param_find("NAV_TAKEOFF_ALT");
	_parameter_handles.land_alt = param_find("NAV_LAND_ALT");
	_parameter_handles.rtl_alt = param_find("NAV_RTL_ALT");

	_mission_item_triplet.previous_valid = false;
	_mission_item_triplet.current_valid = false;
	_mission_item_triplet.next_valid = false;
	memset(&_mission_item_triplet.previous, 0, sizeof(struct mission_item_s));
	memset(&_mission_item_triplet.current, 0, sizeof(struct mission_item_s));
	memset(&_mission_item_triplet.next, 0, sizeof(struct mission_item_s));

	memset(&_mission_result, 0, sizeof(struct mission_result_s));

	/* Initialize state machine */
	myState = NAV_STATE_NONE;
	start_none();
}

Navigator::~Navigator()
{
	if (_navigator_task != -1) {

		/* task wakes up every 100ms or so at the longest */
		_task_should_exit = true;

		/* wait for a second for the task to quit at our request */
		unsigned i = 0;

		do {
			/* wait 20ms */
			usleep(20000);

			/* if we have given up, kill it */
			if (++i > 50) {
				task_delete(_navigator_task);
				break;
			}
		} while (_navigator_task != -1);
	}

	navigator::g_navigator = nullptr;
}

void
Navigator::parameters_update()
{
	/* read from param to clear updated flag */
	struct parameter_update_s update;
	orb_copy(ORB_ID(parameter_update), _params_sub, &update);

	param_get(_parameter_handles.min_altitude, &(_parameters.min_altitude));
	param_get(_parameter_handles.acceptance_radius, &(_parameters.acceptance_radius));
	param_get(_parameter_handles.loiter_radius, &(_parameters.loiter_radius));
	param_get(_parameter_handles.onboard_mission_enabled, &(_parameters.onboard_mission_enabled));
	param_get(_parameter_handles.takeoff_alt, &(_parameters.takeoff_alt));
	param_get(_parameter_handles.land_alt, &(_parameters.land_alt));
	param_get(_parameter_handles.rtl_alt, &(_parameters.rtl_alt));

	_mission.set_onboard_mission_allowed((bool)_parameter_handles.onboard_mission_enabled);
}

void
Navigator::global_position_update()
{
	/* load local copies */
	orb_copy(ORB_ID(vehicle_global_position), _global_pos_sub, &_global_pos);
}

void
Navigator::home_position_update()
{
	orb_copy(ORB_ID(home_position), _home_pos_sub, &_home_pos);
}

void
Navigator::navigation_capabilities_update()
{
	orb_copy(ORB_ID(navigation_capabilities), _capabilities_sub, &_nav_caps);
}


void
Navigator::offboard_mission_update(bool isrotaryWing)
{
	struct mission_s offboard_mission;
	if (orb_copy(ORB_ID(mission), _offboard_mission_sub, &offboard_mission) == OK) {

		/* Check mission feasibility, for now do not handle the return value,
		 * however warnings are issued to the gcs via mavlink from inside the MissionFeasiblityChecker */
		dm_item_t dm_current;
		if (offboard_mission.dataman_id == 0) {
			dm_current = DM_KEY_WAYPOINTS_OFFBOARD_0;
		} else {
			dm_current = DM_KEY_WAYPOINTS_OFFBOARD_1;
		}
		missionFeasiblityChecker.checkMissionFeasible(isrotaryWing, dm_current, (size_t)offboard_mission.count, _geofence);

		_mission.set_offboard_dataman_id(offboard_mission.dataman_id);
		_mission.set_current_offboard_mission_index(offboard_mission.current_index);
		_mission.set_offboard_mission_count(offboard_mission.count);

	} else {
		_mission.set_current_offboard_mission_index(0);
		_mission.set_offboard_mission_count(0);
	}
}

void
Navigator::onboard_mission_update()
{
	struct mission_s onboard_mission;
	if (orb_copy(ORB_ID(mission), _onboard_mission_sub, &onboard_mission) == OK) {

		_mission.set_current_onboard_mission_index(onboard_mission.current_index);
		_mission.set_onboard_mission_count(onboard_mission.count);

	} else {
		_mission.set_current_onboard_mission_index(0);
		_mission.set_onboard_mission_count(0);
	}
}

void
Navigator::vehicle_status_update()
{
	/* try to load initial states */
	if (orb_copy(ORB_ID(vehicle_status), _vstatus_sub, &_vstatus) != OK) {
		_vstatus.arming_state = ARMING_STATE_STANDBY; /* in case the commander is not be running */
	}
}

void
Navigator::task_main_trampoline(int argc, char *argv[])
{
	navigator::g_navigator->task_main();
}

void
Navigator::task_main()
{
	/* inform about start */
	warnx("Initializing..");
	fflush(stdout);

	_mavlink_fd = open(MAVLINK_LOG_DEVICE, 0);

	mavlink_log_info(_mavlink_fd, "[navigator] started");

	_fence_valid = _geofence.loadFromDm(GEOFENCE_MAX_VERTICES);

	/* Try to load the geofence:
	 * if /fs/microsd/etc/geofence.txt load from this file
	 * else clear geofence data in datamanager
	 */
	struct stat buffer;
	if( stat (GEOFENCE_FILENAME, &buffer) == 0 ) {
		warnx("Try to load geofence.txt");
		_geofence.loadFromFile(GEOFENCE_FILENAME);
	} else {
		if (_geofence.clearDm() > 0 )
			warnx("Geofence cleared");
		else
			warnx("Could not clear geofence");
	}

	/*
	 * do subscriptions
	 */
	_global_pos_sub = orb_subscribe(ORB_ID(vehicle_global_position));
	_offboard_mission_sub = orb_subscribe(ORB_ID(mission));
	_onboard_mission_sub = orb_subscribe(ORB_ID(onboard_mission));
	_capabilities_sub = orb_subscribe(ORB_ID(navigation_capabilities));
	_vstatus_sub = orb_subscribe(ORB_ID(vehicle_status));
	_params_sub = orb_subscribe(ORB_ID(parameter_update));
	_home_pos_sub = orb_subscribe(ORB_ID(home_position));
	
	/* copy all topics first time */
	vehicle_status_update();
	parameters_update();
	global_position_update();
	home_position_update();
	navigation_capabilities_update();
	offboard_mission_update(_vstatus.is_rotary_wing);
	onboard_mission_update();

	/* rate limit position updates to 50 Hz */
	orb_set_interval(_global_pos_sub, 20);

	unsigned prevState = NAV_STATE_NONE;
	bool pub_control_mode = true;
	hrt_abstime mavlink_open_time = 0;
	const hrt_abstime mavlink_open_interval = 500000;

	/* wakeup source(s) */
	struct pollfd fds[7];

	/* Setup of loop */
	fds[0].fd = _params_sub;
	fds[0].events = POLLIN;
	fds[1].fd = _global_pos_sub;
	fds[1].events = POLLIN;
	fds[2].fd = _home_pos_sub;
	fds[2].events = POLLIN;
	fds[3].fd = _capabilities_sub;
	fds[3].events = POLLIN;
	fds[4].fd = _offboard_mission_sub;
	fds[4].events = POLLIN;
	fds[5].fd = _onboard_mission_sub;
	fds[5].events = POLLIN;
	fds[6].fd = _vstatus_sub;
	fds[6].events = POLLIN;

	while (!_task_should_exit) {

		/* wait for up to 100ms for data */
		int pret = poll(&fds[0], (sizeof(fds) / sizeof(fds[0])), 100);

		/* timed out - periodic check for _task_should_exit, etc. */
		if (pret == 0) {
			continue;
		}

		/* this is undesirable but not much we can do - might want to flag unhappy status */
		if (pret < 0) {
			warn("poll error %d, %d", pret, errno);
			continue;
		}

		perf_begin(_loop_perf);

		if (_mavlink_fd < 0 && hrt_absolute_time() > mavlink_open_time) {
			/* try to reopen the mavlink log device with specified interval */
			mavlink_open_time = hrt_abstime() + mavlink_open_interval;
			_mavlink_fd = open(MAVLINK_LOG_DEVICE, 0);
		}

		/* vehicle status updated */
		if (fds[6].revents & POLLIN) {
			vehicle_status_update();
			pub_control_mode = true;

			/* Evaluate state machine from commander and set the navigator mode accordingly */
			if (_vstatus.main_state == MAIN_STATE_AUTO &&
					(_vstatus.arming_state == ARMING_STATE_ARMED || _vstatus.arming_state == ARMING_STATE_ARMED_ERROR)) {
				bool stick_mode = false;
				if (!_vstatus.rc_signal_lost) {
					/* RC signal available, use control switches to set mode */
					/* RETURN switch, overrides MISSION switch */
					if (_vstatus.return_switch == RETURN_SWITCH_RETURN) {
						if (myState != NAV_STATE_READY || _rtl_state != RTL_STATE_LAND) {
							dispatch(EVENT_RTL_REQUESTED);
						}
						stick_mode = true;
					} else {
						/* MISSION switch */
						if (_vstatus.mission_switch == MISSION_SWITCH_LOITER) {
							dispatch(EVENT_LOITER_REQUESTED);
							stick_mode = true;
						} else if (_vstatus.mission_switch == MISSION_SWITCH_MISSION) {
							/* switch to mission only if available */
							if (_mission.current_mission_available()) {
								dispatch(EVENT_MISSION_REQUESTED);
							} else {
								dispatch(EVENT_LOITER_REQUESTED);
							}
							stick_mode = true;
						}
						if (!stick_mode && _vstatus.return_switch == RETURN_SWITCH_NORMAL && myState == NAV_STATE_RTL) {
							/* RETURN switch is in normal mode, no MISSION switch mapped, interrupt if in RTL state */
							dispatch(EVENT_LOITER_REQUESTED);
							stick_mode = true;
						}
					}
				}

				if (!stick_mode) {
					if (_vstatus.set_nav_state_timestamp != _set_nav_state_timestamp) {
						/* commander requested new navigation mode, try to set it */
						_set_nav_state_timestamp = _vstatus.set_nav_state_timestamp;

						switch (_vstatus.set_nav_state) {
						case NAV_STATE_NONE:
							/* nothing to do */
							break;

						case NAV_STATE_LOITER:
							dispatch(EVENT_LOITER_REQUESTED);
							break;

						case NAV_STATE_MISSION:
							if (_mission.current_mission_available()) {
								dispatch(EVENT_MISSION_REQUESTED);
							} else {
								dispatch(EVENT_LOITER_REQUESTED);
							}
							break;

						case NAV_STATE_RTL:
							if (myState != NAV_STATE_READY || _rtl_state != RTL_STATE_LAND) {
								dispatch(EVENT_RTL_REQUESTED);
							}
							break;

						default:
							warnx("ERROR: Requested navigation state not supported");
							break;
						}

					} else {
						/* on first switch to AUTO try mission by default, if none is available fallback to loiter */
						if (myState == NAV_STATE_NONE) {
							if (_mission.current_mission_available()) {
								dispatch(EVENT_MISSION_REQUESTED);
							} else {
								dispatch(EVENT_LOITER_REQUESTED);
							}
						}
					}
				}

			} else {
				/* not in AUTO */
				dispatch(EVENT_NONE_REQUESTED);
			}
		}

		/* parameters updated */
		if (fds[0].revents & POLLIN) {
			parameters_update();
			/* note that these new parameters won't be in effect until a mission triplet is published again */
		}

		/* navigation capabilities updated */
		if (fds[3].revents & POLLIN) {
			navigation_capabilities_update();
		}

		/* offboard mission updated */
		if (fds[4].revents & POLLIN) {
			offboard_mission_update(_vstatus.is_rotary_wing);
			// XXX check if mission really changed 
			dispatch(EVENT_MISSION_CHANGED);
		}

		/* onboard mission updated */
		if (fds[5].revents & POLLIN) {
			onboard_mission_update();
			// XXX check if mission really changed
			dispatch(EVENT_MISSION_CHANGED);
		}

		/* home position updated */
		if (fds[2].revents & POLLIN) {
			home_position_update();
			// XXX check if home position really changed
			dispatch(EVENT_HOME_POSITION_CHANGED);
		}

		/* global position updated */
		if (fds[1].revents & POLLIN) {
			global_position_update();
			/* only check if waypoint has been reached in MISSION and RTL modes */
			if (myState == NAV_STATE_MISSION || myState == NAV_STATE_RTL) {
				if (check_mission_item_reached()) {
					on_mission_item_reached();
				}
			}
		}

		/* notify user about state changes */
		if (myState != prevState) {
			mavlink_log_info(_mavlink_fd, "[navigator] nav state %d -> %d", prevState, myState);
			prevState = myState;
			pub_control_mode = true;
		}

		/* publish control mode if updated */
		if (pub_control_mode) {
			publish_control_mode();
		}

		perf_end(_loop_perf);
	}

	warnx("exiting.");

	_navigator_task = -1;
	_exit(0);
}

int
Navigator::start()
{
	ASSERT(_navigator_task == -1);

	/* start the task */
	_navigator_task = task_spawn_cmd("navigator",
					 SCHED_DEFAULT,
					 SCHED_PRIORITY_MAX - 5,
					 2048,
					 (main_t)&Navigator::task_main_trampoline,
					 nullptr);

	if (_navigator_task < 0) {
		warn("task start failed");
		return -errno;
	}

	return OK;
}

void
Navigator::status() 
{
	warnx("Global position is %svalid", _global_pos.valid ? "" : "in");
	if (_global_pos.valid) {
		warnx("Longitude %5.5f degrees, latitude %5.5f degrees", _global_pos.lon / 1e7d, _global_pos.lat / 1e7d);
		warnx("Altitude %5.5f meters, altitude above home %5.5f meters",
			(double)_global_pos.alt, (double)_global_pos.relative_alt);
		warnx("Ground velocity in m/s, x %5.5f, y %5.5f, z %5.5f",
			(double)_global_pos.vx, (double)_global_pos.vy, (double)_global_pos.vz);
		warnx("Compass heading in degrees %5.5f", (double)(_global_pos.yaw * M_RAD_TO_DEG_F));
	}
	if (_fence_valid) {
		warnx("Geofence is valid");
//		warnx("Vertex longitude latitude");
//		for (unsigned i = 0; i < _fence.count; i++)
//		warnx("%6u %9.5f %8.5f", i, (double)_fence.vertices[i].lon, (double)_fence.vertices[i].lat);
	} else {
		warnx("Geofence not set");
	}

	switch (myState) {
		case NAV_STATE_NONE:
			warnx("State: None");
			break;
		case NAV_STATE_LOITER:
			warnx("State: Loiter");
			break;
		case NAV_STATE_MISSION:
			warnx("State: Mission");
			break;
		case NAV_STATE_RTL:
			warnx("State: RTL");
			break;
		default:
			warnx("State: Unknown");
			break;
	}
}

StateTable::Tran const Navigator::myTable[NAV_STATE_MAX][MAX_EVENT] = {
	{	
							/* STATE_NONE */
		/* EVENT_NONE_REQUESTED */		{NO_ACTION, NAV_STATE_NONE},
		/* EVENT_READY_REQUESTED */		{ACTION(&Navigator::start_ready), NAV_STATE_READY},
		/* EVENT_LOITER_REQUESTED */		{ACTION(&Navigator::start_loiter), NAV_STATE_LOITER},
		/* EVENT_MISSION_REQUESTED */		{ACTION(&Navigator::start_mission), NAV_STATE_MISSION},
		/* EVENT_RTL_REQUESTED */		{ACTION(&Navigator::start_rtl), NAV_STATE_RTL},
		/* EVENT_MISSION_CHANGED */		{NO_ACTION, NAV_STATE_NONE},
		/* EVENT_HOME_POSITION_CHANGED */	{NO_ACTION, NAV_STATE_NONE},
	},
	{	
							/* STATE_READY */
		/* EVENT_NONE_REQUESTED */		{NO_ACTION, NAV_STATE_NONE},
		/* EVENT_READY_REQUESTED */		{NO_ACTION, NAV_STATE_READY},
		/* EVENT_LOITER_REQUESTED */		{NO_ACTION, NAV_STATE_READY},
		/* EVENT_MISSION_REQUESTED */		{ACTION(&Navigator::start_mission), NAV_STATE_MISSION},
		/* EVENT_RTL_REQUESTED */		{ACTION(&Navigator::start_rtl), NAV_STATE_RTL},
		/* EVENT_MISSION_CHANGED */		{NO_ACTION, NAV_STATE_READY},
		/* EVENT_HOME_POSITION_CHANGED */	{NO_ACTION, NAV_STATE_READY},
	},
	{
							/* STATE_LOITER */
		/* EVENT_NONE_REQUESTED */		{ACTION(&Navigator::start_none), NAV_STATE_NONE},
		/* EVENT_READY_REQUESTED */		{NO_ACTION, NAV_STATE_LOITER},
		/* EVENT_LOITER_REQUESTED */		{NO_ACTION, NAV_STATE_LOITER},
		/* EVENT_MISSION_REQUESTED */		{ACTION(&Navigator::start_mission), NAV_STATE_MISSION},
		/* EVENT_RTL_REQUESTED */		{ACTION(&Navigator::start_rtl), NAV_STATE_RTL},
		/* EVENT_MISSION_CHANGED */		{NO_ACTION, NAV_STATE_LOITER},
		/* EVENT_HOME_POSITION_CHANGED */	{NO_ACTION, NAV_STATE_LOITER},
	},
	{	
							/* STATE_MISSION */
		/* EVENT_NONE_REQUESTED */		{ACTION(&Navigator::start_none), NAV_STATE_NONE},
		/* EVENT_READY_REQUESTED */		{ACTION(&Navigator::start_ready), NAV_STATE_READY},
		/* EVENT_LOITER_REQUESTED */		{ACTION(&Navigator::start_loiter), NAV_STATE_LOITER},
		/* EVENT_MISSION_REQUESTED */		{NO_ACTION, NAV_STATE_MISSION},
		/* EVENT_RTL_REQUESTED */		{ACTION(&Navigator::start_rtl), NAV_STATE_RTL},
		/* EVENT_MISSION_CHANGED */		{ACTION(&Navigator::start_mission), NAV_STATE_MISSION},
		/* EVENT_HOME_POSITION_CHANGED */	{NO_ACTION, NAV_STATE_MISSION},
	},
	{	
							/* STATE_RTL */
		/* EVENT_NONE_REQUESTED */		{ACTION(&Navigator::start_none), NAV_STATE_NONE},
		/* EVENT_READY_REQUESTED */		{ACTION(&Navigator::start_ready), NAV_STATE_READY},
		/* EVENT_LOITER_REQUESTED */		{ACTION(&Navigator::start_loiter), NAV_STATE_LOITER},
		/* EVENT_MISSION_REQUESTED */		{ACTION(&Navigator::start_mission), NAV_STATE_MISSION},
		/* EVENT_RTL_REQUESTED */		{NO_ACTION, NAV_STATE_RTL},
		/* EVENT_MISSION_CHANGED */		{NO_ACTION, NAV_STATE_RTL},
		/* EVENT_HOME_POSITION_CHANGED */	{ACTION(&Navigator::start_rtl), NAV_STATE_RTL},
	},
};

void
Navigator::start_none()
{
	_mission_item_triplet.previous_valid = false;
	_mission_item_triplet.current_valid = false;
	_mission_item_triplet.next_valid = false;

	_reset_loiter_pos = true;
	_do_takeoff = false;
	_rtl_state = RTL_STATE_NONE;

	publish_mission_item_triplet();
}

void
Navigator::start_ready()
{
	_mission_item_triplet.previous_valid = false;
	_mission_item_triplet.current_valid = false;
	_mission_item_triplet.next_valid = false;

	_reset_loiter_pos = true;
	_do_takeoff = false;

	// TODO check
	if (_rtl_state != RTL_STATE_LAND) {
		_rtl_state = RTL_STATE_NONE;
	}

	publish_mission_item_triplet();
}

void
Navigator::start_loiter()
{
	_do_takeoff = false;

	/* set loiter position if needed */
	if (_reset_loiter_pos || !_mission_item_triplet.current_valid) {
		_reset_loiter_pos = false;

		_mission_item_triplet.current.lat = (double)_global_pos.lat / 1e7d;
		_mission_item_triplet.current.lon = (double)_global_pos.lon / 1e7d;
		_mission_item_triplet.current.yaw = NAN;	// NAN means to use current yaw

		_mission_item_triplet.current.altitude_is_relative = false;
		float min_alt_amsl = _parameters.min_altitude + _home_pos.altitude;

		/* use current altitude if above min altitude set by parameter */
		if (_global_pos.alt < min_alt_amsl) {
			_mission_item_triplet.current.altitude = min_alt_amsl;
			mavlink_log_info(_mavlink_fd, "[navigator] loiter %.1fm higher", (double)(min_alt_amsl - _global_pos.alt));
		} else {
			_mission_item_triplet.current.altitude = _global_pos.alt;
			mavlink_log_info(_mavlink_fd, "[navigator] loiter at current altitude");
		}
	}

	_mission_item_triplet.previous_valid = false;
	_mission_item_triplet.current_valid = true;
	_mission_item_triplet.next_valid = false;

	get_loiter_item(&_mission_item_triplet.current);

	publish_mission_item_triplet();
}

void
Navigator::start_mission()
{
	_need_takeoff = true;

	mavlink_log_info(_mavlink_fd, "[navigator] mission started");
	advance_mission();
}

void
Navigator::advance_mission()
{
	/* copy current mission to previous item */
	memcpy(&_mission_item_triplet.previous, &_mission_item_triplet.current, sizeof(mission_item_s));
	_mission_item_triplet.previous_valid = _mission_item_triplet.current_valid;

	_reset_loiter_pos = true;
	_do_takeoff = false;

	int ret;
	bool onboard;
	unsigned index;

	ret = _mission.get_current_mission_item(&_mission_item_triplet.current, &onboard, &index);

	if (ret == OK) {
		_mission_item_triplet.current_valid = true;
		add_home_pos_to_rtl(&_mission_item_triplet.current);

		if (_mission_item_triplet.current.nav_cmd != NAV_CMD_RETURN_TO_LAUNCH &&
			_mission_item_triplet.current.nav_cmd != NAV_CMD_LOITER_TIME_LIMIT &&
			_mission_item_triplet.current.nav_cmd != NAV_CMD_LOITER_TURN_COUNT &&
			_mission_item_triplet.current.nav_cmd != NAV_CMD_LOITER_UNLIMITED) {
			/* don't reset RTL state on RTL or LOITER items */
			_rtl_state = RTL_STATE_NONE;
		}

		if (_vstatus.is_rotary_wing) {
			if (_need_takeoff && (
				_mission_item_triplet.current.nav_cmd == NAV_CMD_TAKEOFF ||
				_mission_item_triplet.current.nav_cmd == NAV_CMD_WAYPOINT ||
				_mission_item_triplet.current.nav_cmd == NAV_CMD_RETURN_TO_LAUNCH ||
				_mission_item_triplet.current.nav_cmd == NAV_CMD_LOITER_TIME_LIMIT ||
				_mission_item_triplet.current.nav_cmd == NAV_CMD_LOITER_TURN_COUNT ||
				_mission_item_triplet.current.nav_cmd == NAV_CMD_LOITER_UNLIMITED
				)) {
				/* do special TAKEOFF handling for VTOL */
				_need_takeoff = false;

				/* calculate desired takeoff altitude AMSL */
				float takeoff_alt_amsl = _mission_item_triplet.current.altitude;
				if (_mission_item_triplet.current.altitude_is_relative)
					takeoff_alt_amsl += _home_pos.altitude;

				if (_vstatus.condition_landed) {
					/* takeoff to at least NAV_TAKEOFF_ALT from ground if landed */
					takeoff_alt_amsl = fmaxf(takeoff_alt_amsl, _global_pos.alt + _parameters.takeoff_alt);
				}

				/* check if we really need takeoff */
				if (_vstatus.condition_landed || _global_pos.alt < takeoff_alt_amsl - _mission_item_triplet.current.acceptance_radius) {
					/* force TAKEOFF if landed or waypoint altitude is more than current */
					_do_takeoff = true;

					/* move current mission item to next */
					memcpy(&_mission_item_triplet.next, &_mission_item_triplet.current, sizeof(mission_item_s));
					_mission_item_triplet.next_valid = true;

					/* set current item to TAKEOFF */
					get_takeoff_item(&_mission_item_triplet.current);

					_mission_item_triplet.current.lat = (double)_global_pos.lat / 1e7d;
					_mission_item_triplet.current.lon = (double)_global_pos.lon / 1e7d;
					_mission_item_triplet.current.altitude = takeoff_alt_amsl;
					_mission_item_triplet.current.altitude_is_relative = false;
				}
			} else if (_mission_item_triplet.current.nav_cmd == NAV_CMD_LAND) {
				/* will need takeoff after landing */
				_need_takeoff = true;
			}
		}

		if (_do_takeoff) {
			mavlink_log_info(_mavlink_fd, "[navigator] takeoff to %.1fm", _mission_item_triplet.current.altitude);
		} else {
			if (onboard) {
				mavlink_log_info(_mavlink_fd, "[navigator] heading to onboard WP %d", index);
			} else {
				mavlink_log_info(_mavlink_fd, "[navigator] heading to offboard WP %d", index);
			}
		}
	} else {
		/* since a mission is not advanced without WPs available, this is not supposed to happen */
		_mission_item_triplet.current_valid = false;
		warnx("ERROR: current WP can't be set");
	}

	if (!_do_takeoff) {
		ret = _mission.get_next_mission_item(&_mission_item_triplet.next);

		if (ret == OK) {
			add_home_pos_to_rtl(&_mission_item_triplet.next);
			_mission_item_triplet.next_valid = true;
		} else {
			/* this will fail for the last WP */
			_mission_item_triplet.next_valid = false;
		}
	}

	publish_mission_item_triplet();
}

void
Navigator::start_rtl()
{
	_reset_loiter_pos = true;
	_do_takeoff = false;
	if (_rtl_state == RTL_STATE_NONE)
		_rtl_state = RTL_STATE_CLIMB;

	mavlink_log_info(_mavlink_fd, "[navigator] RTL started");
	set_rtl_item();
}

void
Navigator::set_rtl_item()
{
	switch (_rtl_state) {
	case RTL_STATE_CLIMB: {
		memcpy(&_mission_item_triplet.previous, &_mission_item_triplet.current, sizeof(mission_item_s));
		_mission_item_triplet.previous_valid = _mission_item_triplet.current_valid;

		_mission_item_triplet.current_valid = true;
		_mission_item_triplet.next_valid = false;

		float climb_alt = _home_pos.altitude + _parameters.rtl_alt;
		if (_vstatus.condition_landed)
			climb_alt = fmaxf(climb_alt, _global_pos.alt + _parameters.rtl_alt);

		_mission_item_triplet.current.altitude_is_relative = false;
		_mission_item_triplet.current.lat = (double)_global_pos.lat / 1e7d;
		_mission_item_triplet.current.lon = (double)_global_pos.lon / 1e7d;
		_mission_item_triplet.current.altitude = climb_alt;
		_mission_item_triplet.current.yaw = NAN;
		_mission_item_triplet.current.loiter_radius = _parameters.loiter_radius;
		_mission_item_triplet.current.loiter_direction = 1;
		_mission_item_triplet.current.nav_cmd = NAV_CMD_TAKEOFF;
		_mission_item_triplet.current.acceptance_radius = _parameters.acceptance_radius;
		_mission_item_triplet.current.time_inside = 0.0f;
		_mission_item_triplet.current.pitch_min = 0.0f;
		_mission_item_triplet.current.autocontinue = true;
		_mission_item_triplet.current.origin = ORIGIN_ONBOARD;
		mavlink_log_info(_mavlink_fd, "[navigator] RTL: climb to %.1fm", climb_alt - _home_pos.altitude);
		break;
	}
	case RTL_STATE_RETURN: {
		memcpy(&_mission_item_triplet.previous, &_mission_item_triplet.current, sizeof(mission_item_s));
		_mission_item_triplet.previous_valid = _mission_item_triplet.current_valid;

		_mission_item_triplet.current_valid = true;
		_mission_item_triplet.next_valid = false;

		_mission_item_triplet.current.altitude_is_relative = false;
		_mission_item_triplet.current.lat = _home_pos.lat;
		_mission_item_triplet.current.lon = _home_pos.lon;
		// don't change altitude setpoint
		_mission_item_triplet.current.yaw = NAN;
		_mission_item_triplet.current.loiter_radius = _parameters.loiter_radius;
		_mission_item_triplet.current.loiter_direction = 1;
		_mission_item_triplet.current.nav_cmd = NAV_CMD_WAYPOINT;
		_mission_item_triplet.current.acceptance_radius = _parameters.acceptance_radius;
		_mission_item_triplet.current.time_inside = 0.0f;
		_mission_item_triplet.current.pitch_min = 0.0f;
		_mission_item_triplet.current.autocontinue = true;
		_mission_item_triplet.current.origin = ORIGIN_ONBOARD;
		mavlink_log_info(_mavlink_fd, "[navigator] RTL: return");
		break;
	}
	case RTL_STATE_DESCEND: {
		memcpy(&_mission_item_triplet.previous, &_mission_item_triplet.current, sizeof(mission_item_s));
		_mission_item_triplet.previous_valid = _mission_item_triplet.current_valid;

		_mission_item_triplet.current_valid = true;
		_mission_item_triplet.next_valid = false;

		float descend_alt = _home_pos.altitude + _parameters.land_alt;

		_mission_item_triplet.current.altitude_is_relative = false;
		_mission_item_triplet.current.lat = _home_pos.lat;
		_mission_item_triplet.current.lon = _home_pos.lon;
		_mission_item_triplet.current.altitude = descend_alt;
		_mission_item_triplet.current.yaw = NAN;
		_mission_item_triplet.current.loiter_radius = _parameters.loiter_radius;
		_mission_item_triplet.current.loiter_direction = 1;
		_mission_item_triplet.current.nav_cmd = NAV_CMD_WAYPOINT;
		_mission_item_triplet.current.acceptance_radius = _parameters.acceptance_radius;
		_mission_item_triplet.current.time_inside = 0.0f;
		_mission_item_triplet.current.pitch_min = 0.0f;
		_mission_item_triplet.current.autocontinue = true;
		_mission_item_triplet.current.origin = ORIGIN_ONBOARD;
		mavlink_log_info(_mavlink_fd, "[navigator] RTL: descend to %.1fm", descend_alt - _home_pos.altitude);
		break;
	}
	case RTL_STATE_LAND: {
		memcpy(&_mission_item_triplet.previous, &_mission_item_triplet.current, sizeof(mission_item_s));
		_mission_item_triplet.previous_valid = _mission_item_triplet.current_valid;

		_mission_item_triplet.current_valid = true;
		_mission_item_triplet.next_valid = false;

		_mission_item_triplet.current.altitude_is_relative = false;
		_mission_item_triplet.current.lat = _home_pos.lat;
		_mission_item_triplet.current.lon = _home_pos.lon;
		_mission_item_triplet.current.altitude = _home_pos.altitude;
		_mission_item_triplet.current.yaw = NAN;
		_mission_item_triplet.current.loiter_radius = _parameters.loiter_radius;
		_mission_item_triplet.current.loiter_direction = 1;
		_mission_item_triplet.current.nav_cmd = NAV_CMD_LAND;
		_mission_item_triplet.current.acceptance_radius = _parameters.acceptance_radius;
		_mission_item_triplet.current.time_inside = 0.0f;
		_mission_item_triplet.current.pitch_min = 0.0f;
		_mission_item_triplet.current.autocontinue = true;
		_mission_item_triplet.current.origin = ORIGIN_ONBOARD;
		mavlink_log_info(_mavlink_fd, "[navigator] RTL: land");
		break;
	}
	default: {
		mavlink_log_critical(_mavlink_fd, "[navigator] error: unknown RTL state: %d", _rtl_state);
		start_loiter();
		break;
	}
	}

	publish_mission_item_triplet();
}

bool
Navigator::check_mission_item_reached()
{
	/* only check if there is actually a mission item to check */
	if (!_mission_item_triplet.current_valid) {
		return false;
	}

	if (_mission_item_triplet.current.nav_cmd == NAV_CMD_LAND) {
		return _vstatus.condition_landed;
	}

	/* XXX TODO count turns */
	if ((_mission_item_triplet.current.nav_cmd == NAV_CMD_LOITER_TURN_COUNT ||
	     _mission_item_triplet.current.nav_cmd == NAV_CMD_LOITER_TIME_LIMIT ||
	     _mission_item_triplet.current.nav_cmd == NAV_CMD_LOITER_UNLIMITED) &&
	    _mission_item_triplet.current.loiter_radius > 0.01f) {

		return false;
	}	

	uint64_t now = hrt_absolute_time();

	if (!_waypoint_position_reached) {
		float acceptance_radius;

		if (_mission_item_triplet.current.nav_cmd == NAV_CMD_WAYPOINT && _mission_item_triplet.current.acceptance_radius > 0.01f) {
			acceptance_radius = _mission_item_triplet.current.acceptance_radius;

		} else {
			acceptance_radius = _parameters.acceptance_radius;
		}

		float dist = -1.0f;
		float dist_xy = -1.0f;
		float dist_z = -1.0f;

		/* current relative or AMSL altitude depending on mission item altitude_is_relative flag */
		float wp_alt_amsl = _mission_item_triplet.current.altitude;
		if (_mission_item_triplet.current.altitude_is_relative)
			_mission_item_triplet.current.altitude += _home_pos.altitude;

		dist = get_distance_to_point_global_wgs84(_mission_item_triplet.current.lat, _mission_item_triplet.current.lon, wp_alt_amsl,
										  (double)_global_pos.lat / 1e7d, (double)_global_pos.lon / 1e7d, _global_pos.alt,
										  &dist_xy, &dist_z);

		if (_do_takeoff) {
			if (_global_pos.alt > wp_alt_amsl - acceptance_radius) {
				/* require only altitude for takeoff */
				_waypoint_position_reached = true;
			}
		} else {
			if (dist >= 0.0f && dist <= acceptance_radius) {
				_waypoint_position_reached = true;
			}
		}
	}

	if (!_waypoint_yaw_reached) {
		if (_vstatus.is_rotary_wing && !_do_takeoff && isfinite(_mission_item_triplet.current.yaw)) {
			/* check yaw if defined only for rotary wing except takeoff */
			float yaw_err = _wrap_pi(_mission_item_triplet.current.yaw - _global_pos.yaw);
			if (fabsf(yaw_err) < 0.05f) { /* XXX get rid of magic number */
				_waypoint_yaw_reached = true;
			}
		} else {
			_waypoint_yaw_reached = true;
		}
	}

	/* check if the current waypoint was reached */
	if (_waypoint_position_reached && _waypoint_yaw_reached) {
		if (_time_first_inside_orbit == 0) {
			_time_first_inside_orbit = now;
			if (_mission_item_triplet.current.time_inside > 0.01f) {
				mavlink_log_info(_mavlink_fd, "[navigator] waypoint reached, wait for %.1fs", _mission_item_triplet.current.time_inside);
			}
		}
		
		/* check if the MAV was long enough inside the waypoint orbit */
		if ((now - _time_first_inside_orbit >= (uint64_t)_mission_item_triplet.current.time_inside * 1e6)
		     || _mission_item_triplet.current.nav_cmd == NAV_CMD_TAKEOFF) {
			_time_first_inside_orbit = 0;
			_waypoint_yaw_reached = false;
			_waypoint_position_reached = false;
			return true;
		}
	}
	return false;

}

void
Navigator::on_mission_item_reached()
{
	if (myState == NAV_STATE_MISSION) {
		if (_do_takeoff) {
			/* takeoff completed */
			_do_takeoff = false;
			mavlink_log_info(_mavlink_fd, "[navigator] takeoff completed");
		} else {
			/* advance by one mission item */
			_mission.move_to_next();
		}

		if (_mission.current_mission_available()) {
			advance_mission();
		} else {
			/* if no more mission items available then finish mission */
			/* loiter at last waypoint */
			_reset_loiter_pos = false;
			mavlink_log_info(_mavlink_fd, "[navigator] mission completed");
			if (_vstatus.condition_landed) {
				dispatch(EVENT_READY_REQUESTED);
			} else {
				dispatch(EVENT_LOITER_REQUESTED);
			}
		}
	} else {
		/* RTL finished */
		if (_rtl_state == RTL_STATE_LAND) {
			/* landed at home position */
			mavlink_log_info(_mavlink_fd, "[navigator] RTL completed, landed");
			dispatch(EVENT_READY_REQUESTED);
		} else {
			/* next RTL step */
			_rtl_state = (RTLState)(_rtl_state + 1);
			set_rtl_item();
		}
	}
}

void
Navigator::get_loiter_item(struct mission_item_s *item)
{
	//item->altitude_is_relative
	//item->lat
	//item->lon
	//item->altitude
	//item->yaw
	item->loiter_radius = _parameters.loiter_radius;
	item->loiter_direction = 1;
	item->nav_cmd = NAV_CMD_LOITER_UNLIMITED;
	item->acceptance_radius = _parameters.acceptance_radius;
	item->time_inside = 0.0f;
	item->pitch_min = 0.0f;
	item->autocontinue = false;
	item->origin = ORIGIN_ONBOARD;

}

void
Navigator::get_takeoff_item(mission_item_s *item)
{
	//item->altitude_is_relative
	//item->lat
	//item->lon
	//item->altitude
	item->yaw = NAN;
	item->loiter_radius = _parameters.loiter_radius;
	item->loiter_direction = 1;
	item->nav_cmd = NAV_CMD_TAKEOFF;
	item->acceptance_radius = _parameters.acceptance_radius;
	item->time_inside = 0.0f;
	item->pitch_min = 0.0;
	item->autocontinue = true;
	item->origin = ORIGIN_ONBOARD;
}

void
Navigator::add_home_pos_to_rtl(struct mission_item_s *new_mission_item)
{
	if (new_mission_item->nav_cmd == NAV_CMD_RETURN_TO_LAUNCH) {
		/* append the home position to RTL item */
		new_mission_item->lat = _home_pos.lat;
		new_mission_item->lon = _home_pos.lon;
		new_mission_item->altitude = _home_pos.altitude + _parameters.rtl_alt;
		new_mission_item->altitude_is_relative = false;
		new_mission_item->loiter_radius = _parameters.loiter_radius;
		new_mission_item->acceptance_radius = _parameters.acceptance_radius;
	}
}

void
Navigator::publish_mission_item_triplet()
{
	/* lazily publish the mission triplet only once available */
	if (_triplet_pub > 0) {
		/* publish the mission triplet */
		orb_publish(ORB_ID(mission_item_triplet), _triplet_pub, &_mission_item_triplet);

	} else {
		/* advertise and publish */
		_triplet_pub = orb_advertise(ORB_ID(mission_item_triplet), &_mission_item_triplet);
	}
}

void
Navigator::publish_control_mode()
{
	/* update vehicle_control_mode topic*/
	_control_mode.main_state = _vstatus.main_state;
	_control_mode.nav_state = static_cast<nav_state_t>(myState);
	_control_mode.flag_armed = _vstatus.arming_state == ARMING_STATE_ARMED || _vstatus.arming_state == ARMING_STATE_ARMED_ERROR;
	_control_mode.flag_external_manual_override_ok = !_vstatus.is_rotary_wing;
	_control_mode.flag_system_hil_enabled = _vstatus.hil_state == HIL_STATE_ON;

	_control_mode.flag_control_offboard_enabled = false;
	_control_mode.flag_control_flighttermination_enabled = false;

	switch (_vstatus.main_state) {
	case MAIN_STATE_MANUAL:
		_control_mode.flag_control_manual_enabled = true;
		_control_mode.flag_control_rates_enabled = _vstatus.is_rotary_wing;
		_control_mode.flag_control_attitude_enabled = _vstatus.is_rotary_wing;
		_control_mode.flag_control_altitude_enabled = false;
		_control_mode.flag_control_climb_rate_enabled = false;
		_control_mode.flag_control_position_enabled = false;
		_control_mode.flag_control_velocity_enabled = false;
		break;

	case MAIN_STATE_SEATBELT:
		_control_mode.flag_control_manual_enabled = true;
		_control_mode.flag_control_rates_enabled = true;
		_control_mode.flag_control_attitude_enabled = true;
		_control_mode.flag_control_altitude_enabled = true;
		_control_mode.flag_control_climb_rate_enabled = true;
		_control_mode.flag_control_position_enabled = false;
		_control_mode.flag_control_velocity_enabled = false;
		break;

	case MAIN_STATE_EASY:
		_control_mode.flag_control_manual_enabled = true;
		_control_mode.flag_control_rates_enabled = true;
		_control_mode.flag_control_attitude_enabled = true;
		_control_mode.flag_control_altitude_enabled = true;
		_control_mode.flag_control_climb_rate_enabled = true;
		_control_mode.flag_control_position_enabled = true;
		_control_mode.flag_control_velocity_enabled = true;
		break;

	case MAIN_STATE_AUTO:
		_control_mode.flag_control_manual_enabled = false;
		if (myState == NAV_STATE_READY) {
			/* disable all controllers, armed but idle */
			_control_mode.flag_control_rates_enabled = false;
			_control_mode.flag_control_attitude_enabled = false;
			_control_mode.flag_control_position_enabled = false;
			_control_mode.flag_control_velocity_enabled = false;
			_control_mode.flag_control_altitude_enabled = false;
			_control_mode.flag_control_climb_rate_enabled = false;
		} else {
			_control_mode.flag_control_rates_enabled = true;
			_control_mode.flag_control_attitude_enabled = true;
			_control_mode.flag_control_position_enabled = true;
			_control_mode.flag_control_velocity_enabled = true;
			_control_mode.flag_control_altitude_enabled = true;
			_control_mode.flag_control_climb_rate_enabled = true;
		}
		break;

	default:
		break;
	}

	_control_mode.timestamp = hrt_absolute_time();

	/* lazily publish the mission triplet only once available */
	if (_control_mode_pub > 0) {
		/* publish the mission triplet */
		orb_publish(ORB_ID(vehicle_control_mode), _control_mode_pub, &_control_mode);

	} else {
		/* advertise and publish */
		_control_mode_pub = orb_advertise(ORB_ID(vehicle_control_mode), &_control_mode);
	}
}

bool Navigator::cmp_mission_item_equivalent(const struct mission_item_s a, const struct mission_item_s b) {
	if (a.altitude_is_relative == b.altitude_is_relative &&
	    fabs(a.lat - b.lat) < FLT_EPSILON &&
	    fabs(a.lon - b.lon) < FLT_EPSILON &&
	    fabsf(a.altitude - b.altitude) < FLT_EPSILON &&
	    fabsf(a.yaw - b.yaw) < FLT_EPSILON &&
	    fabsf(a.loiter_radius - b.loiter_radius) < FLT_EPSILON &&
	    a.loiter_direction == b.loiter_direction &&
	    a.nav_cmd == b.nav_cmd &&
	    fabsf(a.acceptance_radius - b.acceptance_radius) < FLT_EPSILON &&
	    fabsf(a.time_inside - b.time_inside) < FLT_EPSILON &&
	    a.autocontinue == b.autocontinue) {
		return true;
	} else {
		return false;
	}
}

void Navigator::add_fence_point(int argc, char *argv[])
{
	_geofence.addPoint(argc, argv);
}

void Navigator::load_fence_from_file(const char *filename)
{
	_geofence.loadFromFile(filename);
}


static void usage()
{
	errx(1, "usage: navigator {start|stop|status|fence}");
}

int navigator_main(int argc, char *argv[])
{
	if (argc < 2) {
		usage();
	}

	if (!strcmp(argv[1], "start")) {

		if (navigator::g_navigator != nullptr) {
			errx(1, "already running");
		}

		navigator::g_navigator = new Navigator;

		if (navigator::g_navigator == nullptr) {
			errx(1, "alloc failed");
		}

		if (OK != navigator::g_navigator->start()) {
			delete navigator::g_navigator;
			navigator::g_navigator = nullptr;
			err(1, "start failed");
		}

		return 0;
	}

	if (navigator::g_navigator == nullptr)
		errx(1, "not running");

	if (!strcmp(argv[1], "stop")) {
		delete navigator::g_navigator;
		navigator::g_navigator = nullptr;

	} else if (!strcmp(argv[1], "status")) {
		navigator::g_navigator->status();

	} else if (!strcmp(argv[1], "fence")) {
		navigator::g_navigator->add_fence_point(argc - 2, argv + 2);
	} else if (!strcmp(argv[1], "fencefile")) {
			navigator::g_navigator->load_fence_from_file(GEOFENCE_FILENAME);

	} else {
		usage();
	}

	return 0;
}
