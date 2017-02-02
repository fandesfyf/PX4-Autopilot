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

#include "send_event.h"
#include "temperature_calibration/temperature_calibration.h"

#include <px4_log.h>
#include <drivers/drv_hrt.h>

static SendEvent *send_event_obj = nullptr;

// Run it at 30 Hz.
const unsigned SEND_EVENT_INTERVAL_US = 33000;

int SendEvent::start()
{
	if (_task_is_running) {
		return 0;
	}

	_task_is_running = true;
	_task_should_exit = false;

	/* Schedule a cycle to start things. */
	return work_queue(LPWORK, &_work, (worker_t)&SendEvent::cycle_trampoline, this, 0);
}

void SendEvent::stop()
{
	if (!_task_is_running) {
		return;
	}

	_task_should_exit = true;
	// Wait for task to exit
	int i = 0;

	do {
		/* wait up to 3s */
		usleep(100000);

	} while (_task_is_running && ++i < 30);

	if (i == 30) {
		PX4_ERR("failed to stop");
	}
}

void
SendEvent::cycle_trampoline(void *arg)
{
	SendEvent *obj = reinterpret_cast<SendEvent *>(arg);

	obj->cycle();
}

void SendEvent::cycle()
{
	if (_task_should_exit) {
		if (_vehicle_command_sub >= 0) {
			orb_unsubscribe(_vehicle_command_sub);
			_vehicle_command_sub = -1;
		}

		_task_is_running = false;
		return;
	}

	// check if not yet initialized. we have to do it here, because it's running in a different context than initialisation
	if (_vehicle_command_sub < 0) {
		_vehicle_command_sub = orb_subscribe(ORB_ID(vehicle_command));
	}

	process_commands();

	work_queue(LPWORK, &_work, (worker_t)&SendEvent::cycle_trampoline, this,
		   USEC2TICK(SEND_EVENT_INTERVAL_US));
}

void SendEvent::process_commands()
{
	struct vehicle_command_s cmd;
	bool updated;
	orb_check(_vehicle_command_sub, &updated);

	if (!updated) {
		return;
	}

	orb_copy(ORB_ID(vehicle_command), _vehicle_command_sub, &cmd);

	switch (cmd.command) {
	case vehicle_command_s::VEHICLE_CMD_PREFLIGHT_CALIBRATION:
		if ((int)(cmd.param1) == 2) { //TODO: this needs to be specified in mavlink (and adjust commander accordingly)...

			if (run_temperature_gyro_calibration() == 0) {
				answer_command(cmd, vehicle_command_s::VEHICLE_CMD_RESULT_ACCEPTED);

			} else {
				answer_command(cmd, vehicle_command_s::VEHICLE_CMD_RESULT_FAILED);
			}

		} else if ((int)(cmd.param1) ==
			   3) {  //TODO: this needs to be specified in mavlink (and adjust commander accordingly)...

			if (run_temperature_accel_calibration() == 0) {
				answer_command(cmd, vehicle_command_s::VEHICLE_CMD_RESULT_ACCEPTED);

			} else {
				answer_command(cmd, vehicle_command_s::VEHICLE_CMD_RESULT_FAILED);
			}

		} else if ((int)(cmd.param1) ==
			   4) {  //TODO: this needs to be specified in mavlink (and adjust commander accordingly)...

			if (run_temperature_baro_calibration() == 0) {
				answer_command(cmd, vehicle_command_s::VEHICLE_CMD_RESULT_ACCEPTED);

			} else {
				answer_command(cmd, vehicle_command_s::VEHICLE_CMD_RESULT_FAILED);
			}
		}

		break;
	}

}

void SendEvent::answer_command(const vehicle_command_s &cmd, unsigned result)
{
	struct vehicle_command_ack_s command_ack;

	/* publish ACK */
	command_ack.command = cmd.command;
	command_ack.result = result;
	command_ack.timestamp = hrt_absolute_time();

	if (_command_ack_pub != nullptr) {
		orb_publish(ORB_ID(vehicle_command_ack), _command_ack_pub, &command_ack);

	} else {
		_command_ack_pub = orb_advertise_queue(ORB_ID(vehicle_command_ack), &command_ack,
						       vehicle_command_ack_s::ORB_QUEUE_LENGTH);
	}
}


void SendEvent::print_status()
{
	PX4_INFO("running");
}



static void print_usage(const char *reason = nullptr)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PX4_INFO("usage: send_event {start_listening|stop_listening|status|temperature_calibration}\n"
		 "\tstart_listening: start background task to listen to events\n"
		 "\tstart_temp_gyro_cal: start gyro temperature calibration task\n"
		 "\tstart_temp_accel_cal: start accelerometer temperature calibration task\n"
		 "\tstart_temp_baro_cal: start barometer temperature calibration task\n"
		);
}



int send_event_main(int argc, char *argv[])
{
	if (argc < 2) {
		print_usage();
		return 1;
	}

	if (!strcmp(argv[1], "start_listening")) {

		if (send_event_obj) {
			PX4_INFO("already running");
			return -1;

		} else {
			send_event_obj = new SendEvent();

			if (!send_event_obj) {
				PX4_ERR("alloc failed");
				return -1;
			}

			return send_event_obj->start();
		}

	} else if (!strcmp(argv[1], "stop_listening")) {
		if (send_event_obj) {
			send_event_obj->stop();
			delete send_event_obj;
			send_event_obj = nullptr;
		}

	} else if (!strcmp(argv[1], "status")) {

		if (send_event_obj) {
			send_event_obj->print_status();

		} else {
			PX4_INFO("not running");
		}

	} else if (!strcmp(argv[1], "start_temp_gyro_cal")) {

		if (!send_event_obj) {
			PX4_ERR("background task not running");
			return -1;
		}

		vehicle_command_s cmd = {};
		cmd.target_system = -1;
		cmd.target_component = -1;

		cmd.command = vehicle_command_s::VEHICLE_CMD_PREFLIGHT_CALIBRATION;
		cmd.param1 = 2;
		cmd.param2 = NAN;
		cmd.param3 = NAN;
		cmd.param4 = NAN;
		cmd.param5 = NAN;
		cmd.param6 = NAN;
		cmd.param7 = NAN;

		orb_advert_t h = orb_advertise_queue(ORB_ID(vehicle_command), &cmd, vehicle_command_s::ORB_QUEUE_LENGTH);
		(void)orb_unadvertise(h);

	} else if (!strcmp(argv[1], "start_temp_accel_cal")) {

		if (!send_event_obj) {
			PX4_ERR("background task not running");
			return -1;
		}

		vehicle_command_s cmd = {};
		cmd.target_system = -1;
		cmd.target_component = -1;

		cmd.command = vehicle_command_s::VEHICLE_CMD_PREFLIGHT_CALIBRATION;
		cmd.param1 = 3;
		cmd.param2 = NAN;
		cmd.param3 = NAN;
		cmd.param4 = NAN;
		cmd.param5 = NAN;
		cmd.param6 = NAN;
		cmd.param7 = NAN;

		orb_advert_t h = orb_advertise_queue(ORB_ID(vehicle_command), &cmd, vehicle_command_s::ORB_QUEUE_LENGTH);
		(void)orb_unadvertise(h);

	}  else if (!strcmp(argv[1], "start_temp_baro_cal")) {

		if (!send_event_obj) {
			PX4_ERR("background task not running");
			return -1;
		}

		vehicle_command_s cmd = {};
		cmd.target_system = -1;
		cmd.target_component = -1;

		cmd.command = vehicle_command_s::VEHICLE_CMD_PREFLIGHT_CALIBRATION;
		cmd.param1 = 4;
		cmd.param2 = NAN;
		cmd.param3 = NAN;
		cmd.param4 = NAN;
		cmd.param5 = NAN;
		cmd.param6 = NAN;
		cmd.param7 = NAN;

		orb_advert_t h = orb_advertise_queue(ORB_ID(vehicle_command), &cmd, vehicle_command_s::ORB_QUEUE_LENGTH);
		(void)orb_unadvertise(h);

	} else {
		print_usage("unrecognized command");
	}

	return 0;
}
