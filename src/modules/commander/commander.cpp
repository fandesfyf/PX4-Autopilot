/****************************************************************************
 *
 *   Copyright (C) 2013 PX4 Development Team. All rights reserved.
 *   Author: Petri Tanskanen <petri.tanskanen@inf.ethz.ch>
 *           Lorenz Meier <lm@inf.ethz.ch>
 *           Thomas Gubler <thomasgubler@student.ethz.ch>
 *           Julian Oes <joes@student.ethz.ch>
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
 * @file commander.cpp
 * Main system state machine implementation.
 *
 */

#include <nuttx/config.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <debug.h>
#include <sys/prctl.h>
#include <string.h>
#include <math.h>
#include <poll.h>

#include <uORB/uORB.h>
#include <uORB/topics/sensor_combined.h>
#include <uORB/topics/battery_status.h>
#include <uORB/topics/manual_control_setpoint.h>
#include <uORB/topics/offboard_control_setpoint.h>
#include <uORB/topics/home_position.h>
#include <uORB/topics/vehicle_global_position.h>
#include <uORB/topics/vehicle_local_position.h>
#include <uORB/topics/vehicle_gps_position.h>
#include <uORB/topics/vehicle_command.h>
#include <uORB/topics/vehicle_control_mode.h>
#include <uORB/topics/subsystem_info.h>
#include <uORB/topics/actuator_controls.h>
#include <uORB/topics/actuator_armed.h>
#include <uORB/topics/parameter_update.h>
#include <uORB/topics/differential_pressure.h>
#include <uORB/topics/safety.h>

#include <drivers/drv_led.h>
#include <drivers/drv_hrt.h>
#include <drivers/drv_tone_alarm.h>

#include <mavlink/mavlink_log.h>
#include <systemlib/param/param.h>
#include <systemlib/systemlib.h>
#include <systemlib/err.h>
#include <systemlib/cpuload.h>

#include "px4_custom_mode.h"
#include "commander_helper.h"
#include "state_machine_helper.h"
#include "calibration_routines.h"
#include "accelerometer_calibration.h"
#include "gyro_calibration.h"
#include "mag_calibration.h"
#include "baro_calibration.h"
#include "rc_calibration.h"
#include "airspeed_calibration.h"

/* oddly, ERROR is not defined for c++ */
#ifdef ERROR
# undef ERROR
#endif
static const int ERROR = -1;

extern struct system_load_s system_load;

#define LOW_VOLTAGE_BATTERY_HYSTERESIS_TIME_MS 1000.0f
#define CRITICAL_VOLTAGE_BATTERY_HYSTERESIS_TIME_MS 100.0f

/* Decouple update interval and hysteris counters, all depends on intervals */
#define COMMANDER_MONITORING_INTERVAL 50000
#define COMMANDER_MONITORING_LOOPSPERMSEC (1/(COMMANDER_MONITORING_INTERVAL/1000.0f))
#define LOW_VOLTAGE_BATTERY_COUNTER_LIMIT (LOW_VOLTAGE_BATTERY_HYSTERESIS_TIME_MS*COMMANDER_MONITORING_LOOPSPERMSEC)
#define CRITICAL_VOLTAGE_BATTERY_COUNTER_LIMIT (CRITICAL_VOLTAGE_BATTERY_HYSTERESIS_TIME_MS*COMMANDER_MONITORING_LOOPSPERMSEC)

#define STICK_ON_OFF_LIMIT 0.75f
#define STICK_THRUST_RANGE 1.0f
#define STICK_ON_OFF_HYSTERESIS_TIME_MS 1000
#define STICK_ON_OFF_COUNTER_LIMIT (STICK_ON_OFF_HYSTERESIS_TIME_MS*COMMANDER_MONITORING_LOOPSPERMSEC)

#define GPS_FIX_TYPE_2D 2
#define GPS_FIX_TYPE_3D 3
#define GPS_QUALITY_GOOD_HYSTERIS_TIME_MS 5000
#define GPS_QUALITY_GOOD_COUNTER_LIMIT (GPS_QUALITY_GOOD_HYSTERIS_TIME_MS*COMMANDER_MONITORING_LOOPSPERMSEC)

#define LOCAL_POSITION_TIMEOUT 1000000 /**< consider the local position estimate invalid after 1s */

#define PRINT_INTERVAL	5000000
#define PRINT_MODE_REJECT_INTERVAL	2000000

enum MAV_MODE_FLAG {
	MAV_MODE_FLAG_CUSTOM_MODE_ENABLED = 1, /* 0b00000001 Reserved for future use. | */
	MAV_MODE_FLAG_TEST_ENABLED = 2, /* 0b00000010 system has a test mode enabled. This flag is intended for temporary system tests and should not be used for stable implementations. | */
	MAV_MODE_FLAG_AUTO_ENABLED = 4, /* 0b00000100 autonomous mode enabled, system finds its own goal positions. Guided flag can be set or not, depends on the actual implementation. | */
	MAV_MODE_FLAG_GUIDED_ENABLED = 8, /* 0b00001000 guided mode enabled, system flies MISSIONs / mission items. | */
	MAV_MODE_FLAG_STABILIZE_ENABLED = 16, /* 0b00010000 system stabilizes electronically its attitude (and optionally position). It needs however further control inputs to move around. | */
	MAV_MODE_FLAG_HIL_ENABLED = 32, /* 0b00100000 hardware in the loop simulation. All motors / actuators are blocked, but internal software is full operational. | */
	MAV_MODE_FLAG_MANUAL_INPUT_ENABLED = 64, /* 0b01000000 remote control input is enabled. | */
	MAV_MODE_FLAG_SAFETY_ARMED = 128, /* 0b10000000 MAV safety set to armed. Motors are enabled / running / can start. Ready to fly. | */
	MAV_MODE_FLAG_ENUM_END = 129, /*  | */
};

/* Mavlink file descriptors */
static int mavlink_fd;

/* flags */
static bool commander_initialized = false;
static bool thread_should_exit = false;		/**< daemon exit flag */
static bool thread_running = false;		/**< daemon status flag */
static int daemon_task;				/**< Handle of daemon task / thread */

/* timout until lowlevel failsafe */
static unsigned int failsafe_lowlevel_timeout_ms;
static unsigned int leds_counter;
/* To remember when last notification was sent */
static uint64_t last_print_mode_reject_time = 0;


/* tasks waiting for low prio thread */
typedef enum {
	LOW_PRIO_TASK_NONE = 0,
	LOW_PRIO_TASK_PARAM_SAVE,
	LOW_PRIO_TASK_PARAM_LOAD,
	LOW_PRIO_TASK_GYRO_CALIBRATION,
	LOW_PRIO_TASK_MAG_CALIBRATION,
	LOW_PRIO_TASK_ALTITUDE_CALIBRATION,
	LOW_PRIO_TASK_RC_CALIBRATION,
	LOW_PRIO_TASK_ACCEL_CALIBRATION,
	LOW_PRIO_TASK_AIRSPEED_CALIBRATION
} low_prio_task_t;

static low_prio_task_t low_prio_task = LOW_PRIO_TASK_NONE;

/**
 * The daemon app only briefly exists to start
 * the background job. The stack size assigned in the
 * Makefile does only apply to this management task.
 *
 * The actual stack size should be set in the call
 * to task_create().
 *
 * @ingroup apps
 */
extern "C" __EXPORT int commander_main(int argc, char *argv[]);

/**
 * Print the correct usage.
 */
void usage(const char *reason);

/**
 * React to commands that are sent e.g. from the mavlink module.
 */
void handle_command(struct vehicle_status_s *status, struct vehicle_control_mode_s *control_mode, struct vehicle_command_s *cmd, struct actuator_armed_s *armed);

/**
 * Mainloop of commander.
 */
int commander_thread_main(int argc, char *argv[]);

void toggle_status_leds(vehicle_status_s *status, actuator_armed_s *armed, vehicle_gps_position_s *gps_position);

void check_mode_switches(struct manual_control_setpoint_s *sp_man, struct vehicle_status_s *current_status);

transition_result_t check_main_state_machine(struct vehicle_status_s *current_status);

void print_reject_mode(const char *msg);
void print_reject_arm(const char *msg);

transition_result_t check_navigation_state_machine(struct vehicle_status_s *current_status, struct vehicle_control_mode_s *control_mode);

/**
 * Loop that runs at a lower rate and priority for calibration and parameter tasks.
 */
void *commander_low_prio_loop(void *arg);


int commander_main(int argc, char *argv[])
{
	if (argc < 1)
		usage("missing command");

	if (!strcmp(argv[1], "start")) {

		if (thread_running) {
			warnx("commander already running\n");
			/* this is not an error */
			exit(0);
		}

		thread_should_exit = false;
		daemon_task = task_spawn_cmd("commander",
					     SCHED_DEFAULT,
					     SCHED_PRIORITY_MAX - 40,
					     3000,
					     commander_thread_main,
					     (argv) ? (const char **)&argv[2] : (const char **)NULL);
		exit(0);
	}

	if (!strcmp(argv[1], "stop")) {
		thread_should_exit = true;
		exit(0);
	}

	if (!strcmp(argv[1], "status")) {
		if (thread_running) {
			warnx("\tcommander is running\n");

		} else {
			warnx("\tcommander not started\n");
		}

		exit(0);
	}

	usage("unrecognized command");
	exit(1);
}

void usage(const char *reason)
{
	if (reason)
		fprintf(stderr, "%s\n", reason);

	fprintf(stderr, "usage: daemon {start|stop|status} [-p <additional params>]\n\n");
	exit(1);
}

void handle_command(struct vehicle_status_s *status, const struct safety_s *safety, struct vehicle_control_mode_s *control_mode, struct vehicle_command_s *cmd, struct actuator_armed_s *armed)
{
	/* result of the command */
	uint8_t result = VEHICLE_CMD_RESULT_UNSUPPORTED;

	/* request to set different system mode */
	switch (cmd->command) {
	case VEHICLE_CMD_DO_SET_MODE: {
			uint8_t base_mode = (uint8_t) cmd->param1;
			uint32_t custom_mode = (uint32_t) cmd->param2;

			// TODO remove debug code
			mavlink_log_critical(mavlink_fd, "[cmd] command setmode: %d %d", base_mode, custom_mode);
			/* set arming state */
			transition_result_t arming_res = TRANSITION_NOT_CHANGED;

			if (base_mode & MAV_MODE_FLAG_SAFETY_ARMED) {
				arming_res = arming_state_transition(status, safety, ARMING_STATE_ARMED, armed);

				if (arming_res == TRANSITION_CHANGED) {
					mavlink_log_info(mavlink_fd, "[cmd] ARMED by command");
				}

			} else {
				if (status->arming_state == ARMING_STATE_ARMED || status->arming_state == ARMING_STATE_ARMED_ERROR) {
					arming_state_t new_arming_state = (status->arming_state == ARMING_STATE_ARMED ? ARMING_STATE_STANDBY : ARMING_STATE_STANDBY_ERROR);
					arming_res = arming_state_transition(status, safety, new_arming_state, armed);

					if (arming_res == TRANSITION_CHANGED) {
						mavlink_log_info(mavlink_fd, "[cmd] DISARMED by command");
					}

				} else {
					arming_res = TRANSITION_NOT_CHANGED;
				}
			}

			/* set main state */
			transition_result_t main_res = TRANSITION_DENIED;

			if (base_mode & MAV_MODE_FLAG_CUSTOM_MODE_ENABLED) {
				/* use autopilot-specific mode */
				if (custom_mode == PX4_CUSTOM_MODE_MANUAL) {
					/* MANUAL */
					main_res = main_state_transition(status, MAIN_STATE_MANUAL);

				} else if (custom_mode == PX4_CUSTOM_MODE_SEATBELT) {
					/* SEATBELT */
					main_res = main_state_transition(status, MAIN_STATE_SEATBELT);

				} else if (custom_mode == PX4_CUSTOM_MODE_EASY) {
					/* EASY */
					main_res = main_state_transition(status, MAIN_STATE_EASY);

				} else if (custom_mode == PX4_CUSTOM_MODE_AUTO) {
					/* AUTO */
					main_res = main_state_transition(status, MAIN_STATE_AUTO);
				}

			} else {
				/* use base mode */
				if (base_mode & MAV_MODE_FLAG_AUTO_ENABLED) {
					/* AUTO */
					main_res = main_state_transition(status, MAIN_STATE_AUTO);

				} else if (base_mode & MAV_MODE_FLAG_MANUAL_INPUT_ENABLED) {
					if (base_mode & MAV_MODE_FLAG_GUIDED_ENABLED) {
						/* EASY */
						main_res = main_state_transition(status, MAIN_STATE_EASY);

					} else if (base_mode & MAV_MODE_FLAG_STABILIZE_ENABLED) {
						/* MANUAL */
						main_res = main_state_transition(status, MAIN_STATE_MANUAL);
					}
				}
			}

			if (arming_res != TRANSITION_DENIED && main_res != TRANSITION_DENIED) {
				result = VEHICLE_CMD_RESULT_ACCEPTED;

			} else {
				result = VEHICLE_CMD_RESULT_TEMPORARILY_REJECTED;
			}

			break;
		}

	case VEHICLE_CMD_COMPONENT_ARM_DISARM:
		break;

	case VEHICLE_CMD_PREFLIGHT_REBOOT_SHUTDOWN:
			if (is_safe(status, safety, armed)) {

				if (((int)(cmd->param1)) == 1) {
					/* reboot */
					up_systemreset();
				} else if (((int)(cmd->param1)) == 3) {
					/* reboot to bootloader */

					// XXX implement
					result = VEHICLE_CMD_RESULT_UNSUPPORTED;
				} else {
					result = VEHICLE_CMD_RESULT_DENIED;
				}
			
			} else {
				result = VEHICLE_CMD_RESULT_DENIED;
			}
		break;

	case VEHICLE_CMD_PREFLIGHT_CALIBRATION: {
			low_prio_task_t new_low_prio_task = LOW_PRIO_TASK_NONE;

			if ((int)(cmd->param1) == 1) {
				/* gyro calibration */
				new_low_prio_task = LOW_PRIO_TASK_GYRO_CALIBRATION;

			} else if ((int)(cmd->param2) == 1) {
				/* magnetometer calibration */
				new_low_prio_task = LOW_PRIO_TASK_MAG_CALIBRATION;

			} else if ((int)(cmd->param3) == 1) {
				/* zero-altitude pressure calibration */
				//new_low_prio_task = LOW_PRIO_TASK_ALTITUDE_CALIBRATION;
			} else if ((int)(cmd->param4) == 1) {
				/* RC calibration */
				new_low_prio_task = LOW_PRIO_TASK_RC_CALIBRATION;

			} else if ((int)(cmd->param5) == 1) {
				/* accelerometer calibration */
				new_low_prio_task = LOW_PRIO_TASK_ACCEL_CALIBRATION;

			} else if ((int)(cmd->param6) == 1) {
				/* airspeed calibration */
				new_low_prio_task = LOW_PRIO_TASK_AIRSPEED_CALIBRATION;
			}

			/* check if we have new task and no other task is scheduled */
			if (low_prio_task == LOW_PRIO_TASK_NONE && new_low_prio_task != LOW_PRIO_TASK_NONE) {
				/* try to go to INIT/PREFLIGHT arming state */
				if (TRANSITION_DENIED != arming_state_transition(status, safety, ARMING_STATE_INIT, armed)) {
					result = VEHICLE_CMD_RESULT_ACCEPTED;
					low_prio_task = new_low_prio_task;

				} else {
					result = VEHICLE_CMD_RESULT_DENIED;
				}

			} else {
				result = VEHICLE_CMD_RESULT_TEMPORARILY_REJECTED;
			}

			break;
		}

	case VEHICLE_CMD_PREFLIGHT_STORAGE: {
			low_prio_task_t new_low_prio_task = LOW_PRIO_TASK_NONE;

			if (((int)(cmd->param1)) == 0) {
				new_low_prio_task = LOW_PRIO_TASK_PARAM_LOAD;

			} else if (((int)(cmd->param1)) == 1) {
				new_low_prio_task = LOW_PRIO_TASK_PARAM_SAVE;
			}

			/* check if we have new task and no other task is scheduled */
			if (low_prio_task == LOW_PRIO_TASK_NONE && new_low_prio_task != LOW_PRIO_TASK_NONE) {
				result = VEHICLE_CMD_RESULT_ACCEPTED;
				low_prio_task = new_low_prio_task;

			} else {
				result = VEHICLE_CMD_RESULT_TEMPORARILY_REJECTED;
			}

			break;
		}

	default:
		break;
	}

	/* supported command handling stop */
	if (result == VEHICLE_CMD_RESULT_ACCEPTED) {
		tune_positive();

	} else {
		tune_negative();

		if (result == VEHICLE_CMD_RESULT_DENIED) {
			mavlink_log_critical(mavlink_fd, "[cmd] command denied: %u", cmd->command);

		} else if (result == VEHICLE_CMD_RESULT_FAILED) {
			mavlink_log_critical(mavlink_fd, "[cmd] command failed: %u", cmd->command);

		} else if (result == VEHICLE_CMD_RESULT_TEMPORARILY_REJECTED) {
			mavlink_log_critical(mavlink_fd, "[cmd] command temporarily rejected: %u", cmd->command);

		} else if (result == VEHICLE_CMD_RESULT_UNSUPPORTED) {
			mavlink_log_critical(mavlink_fd, "[cmd] command unsupported: %u", cmd->command);
		}
	}

	/* send any requested ACKs */
	if (cmd->confirmation > 0) {
		/* send acknowledge command */
		// XXX TODO
	}

}

int commander_thread_main(int argc, char *argv[])
{
	/* not yet initialized */
	commander_initialized = false;
	bool home_position_set = false;

	bool battery_tune_played = false;
	bool arm_tune_played = false;

	/* set parameters */
	failsafe_lowlevel_timeout_ms = 0;
	param_get(param_find("SYS_FAILSAVE_LL"), &failsafe_lowlevel_timeout_ms);

	param_t _param_sys_type = param_find("MAV_TYPE");
	param_t _param_system_id = param_find("MAV_SYS_ID");
	param_t _param_component_id = param_find("MAV_COMP_ID");

	/* welcome user */
	warnx("[commander] starting");

	/* pthread for slow low prio thread */
	pthread_t commander_low_prio_thread;

	/* initialize */
	if (led_init() != 0) {
		warnx("ERROR: Failed to initialize leds");
	}

	if (buzzer_init() != OK) {
		warnx("ERROR: Failed to initialize buzzer");
	}

	mavlink_fd = open(MAVLINK_LOG_DEVICE, 0);

	if (mavlink_fd < 0) {
		warnx("ERROR: Failed to open MAVLink log stream, start mavlink app first.");
	}

	/* Main state machine */
	struct vehicle_status_s status;
	orb_advert_t status_pub;
	/* make sure we are in preflight state */
	memset(&status, 0, sizeof(status));

	/* armed topic */
	struct actuator_armed_s armed;
	orb_advert_t armed_pub;
	/* Initialize armed with all false */
	memset(&armed, 0, sizeof(armed));

	/* flags for control apps */
	struct vehicle_control_mode_s control_mode;
	orb_advert_t control_mode_pub;

	/* Initialize all flags to false */
	memset(&control_mode, 0, sizeof(control_mode));

	status.main_state = MAIN_STATE_MANUAL;
	status.navigation_state = NAVIGATION_STATE_STANDBY;
	status.arming_state = ARMING_STATE_INIT;
	status.hil_state = HIL_STATE_OFF;

	/* neither manual nor offboard control commands have been received */
	status.offboard_control_signal_found_once = false;
	status.rc_signal_found_once = false;

	/* mark all signals lost as long as they haven't been found */
	status.rc_signal_lost = true;
	status.offboard_control_signal_lost = true;

	/* allow manual override initially */
	control_mode.flag_external_manual_override_ok = true;

	/* set battery warning flag */
	status.battery_warning = VEHICLE_BATTERY_WARNING_NONE;
	status.condition_battery_voltage_valid = false;

	// XXX for now just set sensors as initialized
	status.condition_system_sensors_initialized = true;

	// XXX just disable offboard control for now
	control_mode.flag_control_offboard_enabled = false;

	/* advertise to ORB */
	status_pub = orb_advertise(ORB_ID(vehicle_status), &status);
	/* publish current state machine */

	/* publish initial state */
	status.counter++;
	status.timestamp = hrt_absolute_time();
	orb_publish(ORB_ID(vehicle_status), status_pub, &status);

	armed_pub = orb_advertise(ORB_ID(actuator_armed), &armed);

	control_mode_pub = orb_advertise(ORB_ID(vehicle_control_mode), &control_mode);

	/* home position */
	orb_advert_t home_pub = -1;
	struct home_position_s home;
	memset(&home, 0, sizeof(home));

	if (status_pub < 0) {
		warnx("ERROR: orb_advertise for topic vehicle_status failed (uorb app running?).\n");
		warnx("exiting.");
		exit(ERROR);
	}

	mavlink_log_info(mavlink_fd, "[cmd] started");

	pthread_attr_t commander_low_prio_attr;
	pthread_attr_init(&commander_low_prio_attr);
	pthread_attr_setstacksize(&commander_low_prio_attr, 2048);

	struct sched_param param;
	/* low priority */
	param.sched_priority = SCHED_PRIORITY_DEFAULT - 50;
	(void)pthread_attr_setschedparam(&commander_low_prio_attr, &param);
	pthread_create(&commander_low_prio_thread, &commander_low_prio_attr, commander_low_prio_loop, NULL);

	/* Start monitoring loop */
	unsigned counter = 0;
	unsigned low_voltage_counter = 0;
	unsigned critical_voltage_counter = 0;
	unsigned stick_off_counter = 0;
	unsigned stick_on_counter = 0;

	/* To remember when last notification was sent */
	uint64_t last_print_control_time = 0;

	enum VEHICLE_BATTERY_WARNING battery_warning_previous = VEHICLE_BATTERY_WARNING_NONE;
	bool armed_previous = false;

	bool low_battery_voltage_actions_done = false;
	bool critical_battery_voltage_actions_done = false;

	uint64_t last_idle_time = 0;

	uint64_t start_time = 0;

	bool status_changed = true;
	bool param_init_forced = true;

	bool updated = false;

	/* Subscribe to safety topic */
	int safety_sub = orb_subscribe(ORB_ID(safety));
	struct safety_s safety;
	memset(&safety, 0, sizeof(safety));
	safety.safety_switch_available = false;
	safety.safety_off = false;

	/* Subscribe to manual control data */
	int sp_man_sub = orb_subscribe(ORB_ID(manual_control_setpoint));
	struct manual_control_setpoint_s sp_man;
	memset(&sp_man, 0, sizeof(sp_man));

	/* Subscribe to offboard control data */
	int sp_offboard_sub = orb_subscribe(ORB_ID(offboard_control_setpoint));
	struct offboard_control_setpoint_s sp_offboard;
	memset(&sp_offboard, 0, sizeof(sp_offboard));

	/* Subscribe to global position */
	int global_position_sub = orb_subscribe(ORB_ID(vehicle_global_position));
	struct vehicle_global_position_s global_position;
	memset(&global_position, 0, sizeof(global_position));

	/* Subscribe to local position data */
	int local_position_sub = orb_subscribe(ORB_ID(vehicle_local_position));
	struct vehicle_local_position_s local_position;
	memset(&local_position, 0, sizeof(local_position));

	/*
	 * The home position is set based on GPS only, to prevent a dependency between
	 * position estimator and commander. RAW GPS is more than good enough for a
	 * non-flying vehicle.
	 */

	/* Subscribe to GPS topic */
	int gps_sub = orb_subscribe(ORB_ID(vehicle_gps_position));
	struct vehicle_gps_position_s gps_position;
	memset(&gps_position, 0, sizeof(gps_position));

	/* Subscribe to sensor topic */
	int sensor_sub = orb_subscribe(ORB_ID(sensor_combined));
	struct sensor_combined_s sensors;
	memset(&sensors, 0, sizeof(sensors));

	/* Subscribe to differential pressure topic */
	int diff_pres_sub = orb_subscribe(ORB_ID(differential_pressure));
	struct differential_pressure_s diff_pres;
	memset(&diff_pres, 0, sizeof(diff_pres));
	uint64_t last_diff_pres_time = 0;

	/* Subscribe to command topic */
	int cmd_sub = orb_subscribe(ORB_ID(vehicle_command));
	struct vehicle_command_s cmd;
	memset(&cmd, 0, sizeof(cmd));

	/* Subscribe to parameters changed topic */
	int param_changed_sub = orb_subscribe(ORB_ID(parameter_update));
	struct parameter_update_s param_changed;
	memset(&param_changed, 0, sizeof(param_changed));

	/* Subscribe to battery topic */
	int battery_sub = orb_subscribe(ORB_ID(battery_status));
	struct battery_status_s battery;
	memset(&battery, 0, sizeof(battery));
	battery.voltage_v = 0.0f;

	/* Subscribe to subsystem info topic */
	int subsys_sub = orb_subscribe(ORB_ID(subsystem_info));
	struct subsystem_info_s info;
	memset(&info, 0, sizeof(info));

	/* now initialized */
	commander_initialized = true;
	thread_running = true;

	start_time = hrt_absolute_time();

	while (!thread_should_exit) {
		hrt_abstime t = hrt_absolute_time();

		/* update parameters */
		orb_check(param_changed_sub, &updated);

		if (updated || param_init_forced) {
			param_init_forced = false;
			/* parameters changed */
			orb_copy(ORB_ID(parameter_update), param_changed_sub, &param_changed);

			/* update parameters */
			if (!armed.armed) {
				if (param_get(_param_sys_type, &(status.system_type)) != OK) {
					warnx("failed getting new system type");
				}

				/* disable manual override for all systems that rely on electronic stabilization */
				if (status.system_type == VEHICLE_TYPE_COAXIAL ||
				    status.system_type == VEHICLE_TYPE_HELICOPTER ||
				    status.system_type == VEHICLE_TYPE_TRICOPTER ||
				    status.system_type == VEHICLE_TYPE_QUADROTOR ||
				    status.system_type == VEHICLE_TYPE_HEXAROTOR ||
				    status.system_type == VEHICLE_TYPE_OCTOROTOR) {
					control_mode.flag_external_manual_override_ok = false;
					status.is_rotary_wing = true;

				} else {
					control_mode.flag_external_manual_override_ok = true;
					status.is_rotary_wing = false;
				}

				/* check and update system / component ID */
				param_get(_param_system_id, &(status.system_id));
				param_get(_param_component_id, &(status.component_id));
				status_changed = true;
			}
		}

		orb_check(sp_man_sub, &updated);

		if (updated) {
			orb_copy(ORB_ID(manual_control_setpoint), sp_man_sub, &sp_man);
		}

		orb_check(sp_offboard_sub, &updated);

		if (updated) {
			orb_copy(ORB_ID(offboard_control_setpoint), sp_offboard_sub, &sp_offboard);
		}

		orb_check(sensor_sub, &updated);

		if (updated) {
			orb_copy(ORB_ID(sensor_combined), sensor_sub, &sensors);
		}

		orb_check(diff_pres_sub, &updated);

		if (updated) {
			orb_copy(ORB_ID(differential_pressure), diff_pres_sub, &diff_pres);
			last_diff_pres_time = diff_pres.timestamp;
		}

		orb_check(cmd_sub, &updated);

		if (updated) {
			/* got command */
			orb_copy(ORB_ID(vehicle_command), cmd_sub, &cmd);

			/* handle it */
			handle_command(&status, &safety, &control_mode, &cmd, &armed);
		}

		/* update safety topic */
		orb_check(safety_sub, &updated);

		if (updated) {
			orb_copy(ORB_ID(safety), safety_sub, &safety);
		}

		/* update global position estimate */
		orb_check(global_position_sub, &updated);

		if (updated) {
			/* position changed */
			orb_copy(ORB_ID(vehicle_global_position), global_position_sub, &global_position);
		}

		/* update local position estimate */
		orb_check(local_position_sub, &updated);

		if (updated) {
			/* position changed */
			orb_copy(ORB_ID(vehicle_local_position), local_position_sub, &local_position);
		}

		/* set the condition to valid if there has recently been a local position estimate */
		if (t - local_position.timestamp < LOCAL_POSITION_TIMEOUT) {
			if (!status.condition_local_position_valid) {
				status.condition_local_position_valid = true;
				status_changed = true;
			}

		} else {
			if (status.condition_local_position_valid) {
				status.condition_local_position_valid = false;
				status_changed = true;
			}
		}

		/* update battery status */
		orb_check(battery_sub, &updated);

		if (updated) {
			orb_copy(ORB_ID(battery_status), battery_sub, &battery);

			warnx("bat v: %2.2f", battery.voltage_v);

			/* only consider battery voltage if system has been running 2s and battery voltage is not 0 */
			if ((t - start_time) > 2000000 && battery.voltage_v > 0.001f) {
				status.battery_voltage = battery.voltage_v;
				status.condition_battery_voltage_valid = true;
				status.battery_remaining = battery_remaining_estimate_voltage(status.battery_voltage);
			}
		}

		/* update subsystem */
		orb_check(subsys_sub, &updated);

		if (updated) {
			orb_copy(ORB_ID(subsystem_info), subsys_sub, &info);

			warnx("subsystem changed: %d\n", (int)info.subsystem_type);

			/* mark / unmark as present */
			if (info.present) {
				status.onboard_control_sensors_present |= info.subsystem_type;

			} else {
				status.onboard_control_sensors_present &= ~info.subsystem_type;
			}

			/* mark / unmark as enabled */
			if (info.enabled) {
				status.onboard_control_sensors_enabled |= info.subsystem_type;

			} else {
				status.onboard_control_sensors_enabled &= ~info.subsystem_type;
			}

			/* mark / unmark as ok */
			if (info.ok) {
				status.onboard_control_sensors_health |= info.subsystem_type;

			} else {
				status.onboard_control_sensors_health &= ~info.subsystem_type;
			}

			status_changed = true;
		}

		toggle_status_leds(&status, &armed, &gps_position);

		if (counter % (1000000 / COMMANDER_MONITORING_INTERVAL) == 0) {
			/* compute system load */
			uint64_t interval_runtime = system_load.tasks[0].total_runtime - last_idle_time;

			if (last_idle_time > 0)
				status.load = 1000 - (interval_runtime / 1000);	//system load is time spent in non-idle

			last_idle_time = system_load.tasks[0].total_runtime;
		}

		/* if battery voltage is getting lower, warn using buzzer, etc. */
		if (status.condition_battery_voltage_valid && status.battery_remaining < 0.15f && !low_battery_voltage_actions_done) {
			//TODO: add filter, or call emergency after n measurements < VOLTAGE_BATTERY_MINIMAL_MILLIVOLTS
			if (low_voltage_counter > LOW_VOLTAGE_BATTERY_COUNTER_LIMIT) {
				low_battery_voltage_actions_done = true;
				mavlink_log_critical(mavlink_fd, "[cmd] WARNING: LOW BATTERY");
				status.battery_warning = VEHICLE_BATTERY_WARNING_WARNING;
				status_changed = true;
			}

			low_voltage_counter++;

		} else if (status.condition_battery_voltage_valid && status.battery_remaining < 0.1f && !critical_battery_voltage_actions_done && low_battery_voltage_actions_done) {
			/* critical battery voltage, this is rather an emergency, change state machine */
			if (critical_voltage_counter > CRITICAL_VOLTAGE_BATTERY_COUNTER_LIMIT) {
				critical_battery_voltage_actions_done = true;
				mavlink_log_critical(mavlink_fd, "[cmd] EMERGENCY: CRITICAL BATTERY");
				status.battery_warning = VEHICLE_BATTERY_WARNING_ALERT;
				arming_state_transition(&status, &safety, ARMING_STATE_ARMED_ERROR, &armed);
				status_changed = true;
			}

			critical_voltage_counter++;

		} else {
			low_voltage_counter = 0;
			critical_voltage_counter = 0;
		}

		/* End battery voltage check */

		/* If in INIT state, try to proceed to STANDBY state */
		if (status.arming_state == ARMING_STATE_INIT && low_prio_task == LOW_PRIO_TASK_NONE) {
			// XXX check for sensors
			arming_state_transition(&status, &safety, ARMING_STATE_STANDBY, &armed);

		} else {
			// XXX: Add emergency stuff if sensors are lost
		}


		/*
		 * Check for valid position information.
		 *
		 * If the system has a valid position source from an onboard
		 * position estimator, it is safe to operate it autonomously.
		 * The flag_vector_flight_mode_ok flag indicates that a minimum
		 * set of position measurements is available.
		 */

		/* store current state to reason later about a state change */
		// bool vector_flight_mode_ok = current_status.flag_vector_flight_mode_ok;
		bool global_pos_valid = status.condition_global_position_valid;
		bool local_pos_valid = status.condition_local_position_valid;
		bool airspeed_valid = status.condition_airspeed_valid;


		/* check for global or local position updates, set a timeout of 2s */
		if (t - global_position.timestamp < 2000000 && t > 2000000 && global_position.valid) {
			status.condition_global_position_valid = true;

		} else {
			status.condition_global_position_valid = false;
		}

		if (t - local_position.timestamp < 2000000 && t > 2000000 && local_position.valid) {
			status.condition_local_position_valid = true;

		} else {
			status.condition_local_position_valid = false;
		}

		/* Check for valid airspeed/differential pressure measurements */
		if (t - last_diff_pres_time < 2000000 && t > 2000000) {
			status.condition_airspeed_valid = true;

		} else {
			status.condition_airspeed_valid = false;
		}

		orb_check(gps_sub, &updated);

		if (updated) {
			orb_copy(ORB_ID(vehicle_gps_position), gps_sub, &gps_position);

			/* check for first, long-term and valid GPS lock -> set home position */
			float hdop_m = gps_position.eph_m;
			float vdop_m = gps_position.epv_m;

			/* check if GPS fix is ok */
			float hdop_threshold_m = 4.0f;
			float vdop_threshold_m = 8.0f;

			/*
			 * If horizontal dilution of precision (hdop / eph)
			 * and vertical diluation of precision (vdop / epv)
			 * are below a certain threshold (e.g. 4 m), AND
			 * home position is not yet set AND the last GPS
			 * GPS measurement is not older than two seconds AND
			 * the system is currently not armed, set home
			 * position to the current position.
			 */

			if (!home_position_set && gps_position.fix_type == GPS_FIX_TYPE_3D &&
			    (hdop_m < hdop_threshold_m) && (vdop_m < vdop_threshold_m) &&	// XXX note that vdop is 0 for mtk
			    (t - gps_position.timestamp_position < 2000000)
			    && !armed.armed) {
				/* copy position data to uORB home message, store it locally as well */
				// TODO use global position estimate
				home.lat = gps_position.lat;
				home.lon = gps_position.lon;
				home.alt = gps_position.alt;

				home.eph_m = gps_position.eph_m;
				home.epv_m = gps_position.epv_m;

				home.s_variance_m_s = gps_position.s_variance_m_s;
				home.p_variance_m = gps_position.p_variance_m;

				double home_lat_d = home.lat * 1e-7;
				double home_lon_d = home.lon * 1e-7;
				warnx("home: lat = %.7f, lon = %.7f", home_lat_d, home_lon_d);
				mavlink_log_info(mavlink_fd, "[cmd] home: %.7f, %.7f", home_lat_d, home_lon_d);

				/* announce new home position */
				if (home_pub > 0) {
					orb_publish(ORB_ID(home_position), home_pub, &home);

				} else {
					home_pub = orb_advertise(ORB_ID(home_position), &home);
				}

				/* mark home position as set */
				home_position_set = true;
				tune_positive();
			}
		}

		/* ignore RC signals if in offboard control mode */
		if (!status.offboard_control_signal_found_once && sp_man.timestamp != 0) {
			/* start RC input check */
			if ((t - sp_man.timestamp) < 100000) {
				/* handle the case where RC signal was regained */
				if (!status.rc_signal_found_once) {
					status.rc_signal_found_once = true;
					mavlink_log_critical(mavlink_fd, "[cmd] detected RC signal first time");
					status_changed = true;

				} else {
					if (status.rc_signal_lost) {
						mavlink_log_critical(mavlink_fd, "[cmd] RC signal regained");
						status_changed = true;
					}
				}

				status.rc_signal_cutting_off = false;
				status.rc_signal_lost = false;
				status.rc_signal_lost_interval = 0;

				transition_result_t res;	// store all transitions results here

				/* arm/disarm by RC */
				res = TRANSITION_NOT_CHANGED;

				/* check if left stick is in lower left position and we are in MANUAL or AUTO mode -> disarm
				 * do it only for rotary wings */
				if (status.is_rotary_wing &&
				    (status.arming_state == ARMING_STATE_ARMED || status.arming_state == ARMING_STATE_ARMED_ERROR) &&
				    (status.main_state == MAIN_STATE_MANUAL || status.navigation_state == NAVIGATION_STATE_AUTO_READY)) {
					if (sp_man.yaw < -STICK_ON_OFF_LIMIT && sp_man.throttle < STICK_THRUST_RANGE * 0.1f) {
						if (stick_off_counter > STICK_ON_OFF_COUNTER_LIMIT) {
							/* disarm to STANDBY if ARMED or to STANDBY_ERROR if ARMED_ERROR */
							arming_state_t new_arming_state = (status.arming_state == ARMING_STATE_ARMED ? ARMING_STATE_STANDBY : ARMING_STATE_STANDBY_ERROR);
							res = arming_state_transition(&status, &safety, new_arming_state, &armed);
							stick_off_counter = 0;

						} else {
							stick_off_counter++;
						}

						stick_on_counter = 0;

					} else {
						stick_off_counter = 0;
					}
				}

				/* check if left stick is in lower right position and we're in manual mode -> arm */
				if (status.arming_state == ARMING_STATE_STANDBY &&
				    status.main_state == MAIN_STATE_MANUAL) {
					if (sp_man.yaw > STICK_ON_OFF_LIMIT && sp_man.throttle < STICK_THRUST_RANGE * 0.1f) {
						if (stick_on_counter > STICK_ON_OFF_COUNTER_LIMIT) {
							res = arming_state_transition(&status, &safety, ARMING_STATE_ARMED, &armed);
							stick_on_counter = 0;

						} else {
							stick_on_counter++;
						}

						stick_off_counter = 0;

					} else {
						stick_on_counter = 0;
					}
				}

				if (res == TRANSITION_CHANGED) {
					if (status.arming_state == ARMING_STATE_ARMED) {
						mavlink_log_info(mavlink_fd, "[cmd] ARMED by RC");

					} else {
						mavlink_log_info(mavlink_fd, "[cmd] DISARMED by RC");
					}
				} else if (res == TRANSITION_DENIED) {
					/* DENIED here indicates safety switch not pressed */

					if (!(!safety.safety_switch_available || safety.safety_off)) {
						print_reject_arm("NOT ARMING: Press safety switch first.");

					} else {
						warnx("ERROR: main denied: arm %d main %d mode_sw %d", status.arming_state, status.main_state, status.mode_switch);
						mavlink_log_critical(mavlink_fd, "[cmd] ERROR: main denied: arm %d main %d mode_sw %d", status.arming_state, status.main_state, status.mode_switch);
					}
				}

				/* fill current_status according to mode switches */
				check_mode_switches(&sp_man, &status);

				/* evaluate the main state machine */
				res = check_main_state_machine(&status);

				if (res == TRANSITION_CHANGED) {
					mavlink_log_info(mavlink_fd, "[cmd] main state: %d", status.main_state);
					tune_positive();

				} else if (res == TRANSITION_DENIED) {
					/* DENIED here indicates bug in the commander */
					warnx("ERROR: main denied: arm %d main %d mode_sw %d", status.arming_state, status.main_state, status.mode_switch);
					mavlink_log_critical(mavlink_fd, "[cmd] ERROR: main denied: arm %d main %d mode_sw %d", status.arming_state, status.main_state, status.mode_switch);
				}

			} else {

				/* print error message for first RC glitch and then every 5s */
				if (!status.rc_signal_cutting_off || (t - last_print_control_time) > PRINT_INTERVAL) {
					// TODO remove debug code
					if (!status.rc_signal_cutting_off) {
						warnx("Reason: not rc_signal_cutting_off\n");

					} else {
						warnx("last print time: %llu\n", last_print_control_time);
					}

					/* only complain if the offboard control is NOT active */
					status.rc_signal_cutting_off = true;
					mavlink_log_critical(mavlink_fd, "[cmd] CRITICAL: NO RC CONTROL");

					last_print_control_time = t;
				}

				/* flag as lost and update interval since when the signal was lost (to initiate RTL after some time) */
				status.rc_signal_lost_interval = t - sp_man.timestamp;

				/* if the RC signal is gone for a full second, consider it lost */
				if (status.rc_signal_lost_interval > 1000000) {
					status.rc_signal_lost = true;
					status.failsave_lowlevel = true;
					status_changed = true;
				}
			}
		}

		/* END mode switch */
		/* END RC state check */

		// TODO check this
		/* state machine update for offboard control */
		if (!status.rc_signal_found_once && sp_offboard.timestamp != 0) {
			if ((t - sp_offboard.timestamp) < 5000000) {	// TODO 5s is too long ?

				// /* decide about attitude control flag, enable in att/pos/vel */
				// bool attitude_ctrl_enabled = (sp_offboard.mode == OFFBOARD_CONTROL_MODE_DIRECT_ATTITUDE ||
				// 				  sp_offboard.mode == OFFBOARD_CONTROL_MODE_DIRECT_VELOCITY ||
				// 				  sp_offboard.mode == OFFBOARD_CONTROL_MODE_DIRECT_POSITION);

				// /* decide about rate control flag, enable it always XXX (for now) */
				// bool rates_ctrl_enabled = true;

				// /* set up control mode */
				// if (current_status.flag_control_attitude_enabled != attitude_ctrl_enabled) {
				// 	current_status.flag_control_attitude_enabled = attitude_ctrl_enabled;
				// 	state_changed = true;
				// }

				// if (current_status.flag_control_rates_enabled != rates_ctrl_enabled) {
				// 	current_status.flag_control_rates_enabled = rates_ctrl_enabled;
				// 	state_changed = true;
				// }

				// /* handle the case where offboard control signal was regained */
				// if (!current_status.offboard_control_signal_found_once) {
				// 	current_status.offboard_control_signal_found_once = true;
				// 	/* enable offboard control, disable manual input */
				// 	current_status.flag_control_manual_enabled = false;
				// 	current_status.flag_control_offboard_enabled = true;
				// 	state_changed = true;
				// 	tune_positive();

				// 	mavlink_log_critical(mavlink_fd, "DETECTED OFFBOARD SIGNAL FIRST");

				// } else {
				// 	if (current_status.offboard_control_signal_lost) {
				// 		mavlink_log_critical(mavlink_fd, "RECOVERY OFFBOARD CONTROL");
				// 		state_changed = true;
				// 		tune_positive();
				// 	}
				// }

				status.offboard_control_signal_weak = false;
				status.offboard_control_signal_lost = false;
				status.offboard_control_signal_lost_interval = 0;

				// XXX check if this is correct
				/* arm / disarm on request */
				if (sp_offboard.armed && !armed.armed) {
					arming_state_transition(&status, &safety, ARMING_STATE_ARMED, &armed);

				} else if (!sp_offboard.armed && armed.armed) {
					arming_state_transition(&status, &safety, ARMING_STATE_STANDBY, &armed);
				}

			} else {

				/* print error message for first offboard signal glitch and then every 5s */
				if (!status.offboard_control_signal_weak || ((t - last_print_control_time) > PRINT_INTERVAL)) {
					status.offboard_control_signal_weak = true;
					mavlink_log_critical(mavlink_fd, "[cmd] CRITICAL: NO OFFBOARD CONTROL");
					last_print_control_time = t;
				}

				/* flag as lost and update interval since when the signal was lost (to initiate RTL after some time) */
				status.offboard_control_signal_lost_interval = t - sp_offboard.timestamp;

				/* if the signal is gone for 0.1 seconds, consider it lost */
				if (status.offboard_control_signal_lost_interval > 100000) {
					status.offboard_control_signal_lost = true;
					status.failsave_lowlevel_start_time = t;
					tune_positive();

					/* kill motors after timeout */
					if (t - status.failsave_lowlevel_start_time > failsafe_lowlevel_timeout_ms * 1000) {
						status.failsave_lowlevel = true;
						status_changed = true;
					}
				}
			}
		}

		/* evaluate the navigation state machine */
		transition_result_t res = check_navigation_state_machine(&status, &control_mode);

		if (res == TRANSITION_DENIED) {
			/* DENIED here indicates bug in the commander */
			warnx("ERROR: nav denied: arm %d main %d nav %d", status.arming_state, status.main_state, status.navigation_state);
			mavlink_log_critical(mavlink_fd, "[cmd] ERROR: nav denied: arm %d main %d nav %d", status.arming_state, status.main_state, status.navigation_state);
		}

		/* check which state machines for changes, clear "changed" flag */
		bool arming_state_changed = check_arming_state_changed();
		bool main_state_changed = check_main_state_changed();
		bool navigation_state_changed = check_navigation_state_changed();

		if (arming_state_changed || main_state_changed || navigation_state_changed) {
			mavlink_log_info(mavlink_fd, "[cmd] state: arm %d, main %d, nav %d", status.arming_state, status.main_state, status.navigation_state);
			status_changed = true;
		}

		/* publish arming state */
		if (arming_state_changed) {
			armed.timestamp = t;
			orb_publish(ORB_ID(actuator_armed), armed_pub, &armed);
		}

		/* publish control mode */
		if (navigation_state_changed) {
			/* publish new navigation state */
			control_mode.counter++;
			control_mode.timestamp = t;
			orb_publish(ORB_ID(vehicle_control_mode), control_mode_pub, &control_mode);
		}

		/* publish vehicle status at least with 1 Hz */
		if (counter % (1000000 / COMMANDER_MONITORING_INTERVAL) == 0 || status_changed) {
			status.counter++;
			status.timestamp = t;
			orb_publish(ORB_ID(vehicle_status), status_pub, &status);
			status_changed = false;
		}

		/* play arming and battery warning tunes */
		if (!arm_tune_played && armed.armed) {
			/* play tune when armed */
			if (tune_arm() == OK)
				arm_tune_played = true;

		} else if (status.battery_warning == VEHICLE_BATTERY_WARNING_WARNING) {
			/* play tune on battery warning */
			if (tune_low_bat() == OK)
				battery_tune_played = true;

		} else if (status.battery_remaining == VEHICLE_BATTERY_WARNING_ALERT) {
			/* play tune on battery critical */
			if (tune_critical_bat() == OK)
				battery_tune_played = true;

		} else if (battery_tune_played) {
			tune_stop();
			battery_tune_played = false;
		}

		/* reset arm_tune_played when disarmed */
		if (!(armed.armed && (!safety.safety_switch_available || (safety.safety_off && safety.safety_switch_available)))) {
			arm_tune_played = false;
		}

		/* store old modes to detect and act on state transitions */
		battery_warning_previous = status.battery_warning;
		armed_previous = armed.armed;

		fflush(stdout);
		counter++;
		usleep(COMMANDER_MONITORING_INTERVAL);
	}

	/* wait for threads to complete */
	pthread_join(commander_low_prio_thread, NULL);

	/* close fds */
	led_deinit();
	buzzer_deinit();
	close(sp_man_sub);
	close(sp_offboard_sub);
	close(local_position_sub);
	close(global_position_sub);
	close(gps_sub);
	close(sensor_sub);
	close(safety_sub);
	close(cmd_sub);
	close(subsys_sub);
	close(diff_pres_sub);
	close(param_changed_sub);
	close(battery_sub);

	warnx("exiting");
	fflush(stdout);

	thread_running = false;

	return 0;
}

void
toggle_status_leds(vehicle_status_s *status, actuator_armed_s *armed, vehicle_gps_position_s *gps_position)
{
	if (leds_counter % 2 == 0) {
		/* run at 10Hz, full cycle is 16 ticks = 10/16Hz */
		if (armed->armed) {
			/* armed, solid */
			led_on(LED_AMBER);

		} else if (armed->ready_to_arm) {
			/* ready to arm, blink at 2.5Hz */
			if (leds_counter & 8) {
				led_on(LED_AMBER);

			} else {
				led_off(LED_AMBER);
			}

		} else {
			/* not ready to arm, blink at 10Hz */
			led_toggle(LED_AMBER);
		}

		if (status->condition_global_position_valid) {
			/* position estimated, solid */
			led_on(LED_BLUE);

		} else if (leds_counter == 0) {
			/* waiting for position estimate, short blink at 1.25Hz */
			led_on(LED_BLUE);

		} else {
			/* no position estimator available, off */
			led_off(LED_BLUE);
		}
	}

	leds_counter++;

	if (leds_counter >= 16)
		leds_counter = 0;
}

void
check_mode_switches(struct manual_control_setpoint_s *sp_man, struct vehicle_status_s *current_status)
{
	/* main mode switch */
	if (!isfinite(sp_man->mode_switch)) {
		warnx("mode sw not finite");
		current_status->mode_switch = MODE_SWITCH_MANUAL;

	} else if (sp_man->mode_switch > STICK_ON_OFF_LIMIT) {
		current_status->mode_switch = MODE_SWITCH_AUTO;

	} else if (sp_man->mode_switch < -STICK_ON_OFF_LIMIT) {
		current_status->mode_switch = MODE_SWITCH_MANUAL;

	} else {
		current_status->mode_switch = MODE_SWITCH_ASSISTED;
	}

	/* land switch */
	if (!isfinite(sp_man->return_switch)) {
		current_status->return_switch = RETURN_SWITCH_NONE;

	} else if (sp_man->return_switch > STICK_ON_OFF_LIMIT) {
		current_status->return_switch = RETURN_SWITCH_RETURN;

	} else {
		current_status->return_switch = RETURN_SWITCH_NONE;
	}

	/* assisted switch */
	if (!isfinite(sp_man->assisted_switch)) {
		current_status->assisted_switch = ASSISTED_SWITCH_SEATBELT;

	} else if (sp_man->assisted_switch > STICK_ON_OFF_LIMIT) {
		current_status->assisted_switch = ASSISTED_SWITCH_EASY;

	} else {
		current_status->assisted_switch = ASSISTED_SWITCH_SEATBELT;
	}

	/* mission switch  */
	if (!isfinite(sp_man->mission_switch)) {
		current_status->mission_switch = MISSION_SWITCH_MISSION;

	} else if (sp_man->mission_switch > STICK_ON_OFF_LIMIT) {
		current_status->mission_switch = MISSION_SWITCH_NONE;

	} else {
		current_status->mission_switch = MISSION_SWITCH_MISSION;
	}
}

transition_result_t
check_main_state_machine(struct vehicle_status_s *current_status)
{
	/* evaluate the main state machine */
	transition_result_t res = TRANSITION_DENIED;

	switch (current_status->mode_switch) {
	case MODE_SWITCH_MANUAL:
		res = main_state_transition(current_status, MAIN_STATE_MANUAL);
		// TRANSITION_DENIED is not possible here
		break;

	case MODE_SWITCH_ASSISTED:
		if (current_status->assisted_switch == ASSISTED_SWITCH_EASY) {
			res = main_state_transition(current_status, MAIN_STATE_EASY);

			if (res != TRANSITION_DENIED)
				break;	// changed successfully or already in this state

			// else fallback to SEATBELT
			print_reject_mode("EASY");
		}

		res = main_state_transition(current_status, MAIN_STATE_SEATBELT);

		if (res != TRANSITION_DENIED)
			break;	// changed successfully or already in this mode

		if (current_status->assisted_switch != ASSISTED_SWITCH_EASY)	// don't print both messages
			print_reject_mode("SEATBELT");

		// else fallback to MANUAL
		res = main_state_transition(current_status, MAIN_STATE_MANUAL);
		// TRANSITION_DENIED is not possible here
		break;

	case MODE_SWITCH_AUTO:
		res = main_state_transition(current_status, MAIN_STATE_AUTO);

		if (res != TRANSITION_DENIED)
			break;	// changed successfully or already in this state

		// else fallback to SEATBELT (EASY likely will not work too)
		print_reject_mode("AUTO");
		res = main_state_transition(current_status, MAIN_STATE_SEATBELT);

		if (res != TRANSITION_DENIED)
			break;	// changed successfully or already in this state

		// else fallback to MANUAL
		res = main_state_transition(current_status, MAIN_STATE_MANUAL);
		// TRANSITION_DENIED is not possible here
		break;

	default:
		break;
	}

	return res;
}

void
print_reject_mode(const char *msg)
{
	hrt_abstime t = hrt_absolute_time();

	if (t - last_print_mode_reject_time > PRINT_MODE_REJECT_INTERVAL) {
		last_print_mode_reject_time = t;
		char s[80];
		sprintf(s, "[cmd] WARNING: reject %s", msg);
		mavlink_log_critical(mavlink_fd, s);
		tune_negative();
	}
}

void
print_reject_arm(const char *msg)
{
	hrt_abstime t = hrt_absolute_time();

	if (t - last_print_mode_reject_time > PRINT_MODE_REJECT_INTERVAL) {
		last_print_mode_reject_time = t;
		char s[80];
		sprintf(s, "[cmd] %s", msg);
		mavlink_log_critical(mavlink_fd, s);
		tune_negative();
	}
}

transition_result_t
check_navigation_state_machine(struct vehicle_status_s *current_status, struct vehicle_control_mode_s *control_mode)
{
	transition_result_t res = TRANSITION_DENIED;

	if (current_status->arming_state == ARMING_STATE_ARMED || current_status->arming_state == ARMING_STATE_ARMED_ERROR) {
		/* ARMED */
		switch (current_status->main_state) {
		case MAIN_STATE_MANUAL:
			res = navigation_state_transition(current_status, current_status->is_rotary_wing ? NAVIGATION_STATE_STABILIZE : NAVIGATION_STATE_DIRECT, control_mode);
			break;

		case MAIN_STATE_SEATBELT:
			res = navigation_state_transition(current_status, NAVIGATION_STATE_ALTHOLD, control_mode);
			break;

		case MAIN_STATE_EASY:
			res = navigation_state_transition(current_status, NAVIGATION_STATE_VECTOR, control_mode);
			break;

		case MAIN_STATE_AUTO:
			if (current_status->navigation_state != NAVIGATION_STATE_AUTO_TAKEOFF) {
				/* don't act while taking off */
				if (current_status->condition_landed) {
					/* if landed: transitions only to AUTO_READY are allowed */
					res = navigation_state_transition(current_status, NAVIGATION_STATE_AUTO_READY, control_mode);
					// TRANSITION_DENIED is not possible here
					break;

				} else {
					/* if not landed: act depending on switches */
					if (current_status->return_switch == RETURN_SWITCH_RETURN) {
						/* RTL */
						res = navigation_state_transition(current_status, NAVIGATION_STATE_AUTO_RTL, control_mode);

					} else {
						if (current_status->mission_switch == MISSION_SWITCH_MISSION) {
							/* MISSION */
							res = navigation_state_transition(current_status, NAVIGATION_STATE_AUTO_MISSION, control_mode);

						} else {
							/* LOITER */
							res = navigation_state_transition(current_status, NAVIGATION_STATE_AUTO_LOITER, control_mode);
						}
					}
				}
			}

			break;

		default:
			break;
		}

	} else {
		/* DISARMED */
		res = navigation_state_transition(current_status, NAVIGATION_STATE_STANDBY, control_mode);
	}

	return res;
}

void *commander_low_prio_loop(void *arg)
{
	/* Set thread name */
	prctl(PR_SET_NAME, "commander_low_prio", getpid());

	low_prio_task = LOW_PRIO_TASK_NONE;

	while (!thread_should_exit) {

		switch (low_prio_task) {
		case LOW_PRIO_TASK_PARAM_LOAD:

			if (0 == param_load_default()) {
				mavlink_log_info(mavlink_fd, "[cmd] parameters loaded");

			} else {
				mavlink_log_critical(mavlink_fd, "[cmd] parameters load ERROR");
				tune_error();
			}

			low_prio_task = LOW_PRIO_TASK_NONE;
			break;

		case LOW_PRIO_TASK_PARAM_SAVE:

			if (0 == param_save_default()) {
				mavlink_log_info(mavlink_fd, "[cmd] parameters saved");

			} else {
				mavlink_log_critical(mavlink_fd, "[cmd] parameters save error");
				tune_error();
			}

			low_prio_task = LOW_PRIO_TASK_NONE;
			break;

		case LOW_PRIO_TASK_GYRO_CALIBRATION:

			do_gyro_calibration(mavlink_fd);
			low_prio_task = LOW_PRIO_TASK_NONE;
			break;

		case LOW_PRIO_TASK_MAG_CALIBRATION:

			do_mag_calibration(mavlink_fd);
			low_prio_task = LOW_PRIO_TASK_NONE;
			break;

		case LOW_PRIO_TASK_ALTITUDE_CALIBRATION:

			// do_baro_calibration(mavlink_fd);
			low_prio_task = LOW_PRIO_TASK_NONE;
			break;

		case LOW_PRIO_TASK_RC_CALIBRATION:

			// do_rc_calibration(mavlink_fd);
			low_prio_task = LOW_PRIO_TASK_NONE;
			break;

		case LOW_PRIO_TASK_ACCEL_CALIBRATION:

			do_accel_calibration(mavlink_fd);
			low_prio_task = LOW_PRIO_TASK_NONE;
			break;

		case LOW_PRIO_TASK_AIRSPEED_CALIBRATION:

			do_airspeed_calibration(mavlink_fd);
			low_prio_task = LOW_PRIO_TASK_NONE;
			break;

		case LOW_PRIO_TASK_NONE:
		default:
			/* slow down to 10Hz */
			usleep(100000);
			break;
		}

	}

	return 0;
}
