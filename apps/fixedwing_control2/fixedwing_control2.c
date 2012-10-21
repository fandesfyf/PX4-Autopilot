/****************************************************************************
 *
 *   Copyright (C) 2012 PX4 Development Team. All rights reserved.
 *   Author: @author Ivan Ovinnikov <oivan@ethz.ch>
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
 * @file fixedwing_control2.c
 * Implementation of a fixed wing attitude controller.
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
#include <arch/board/up_hrt.h>
#include <arch/board/board.h>
#include <uORB/uORB.h>
#include <uORB/topics/vehicle_global_position.h>
#include <uORB/topics/vehicle_global_position_setpoint.h>
#include <uORB/topics/vehicle_attitude.h>
#include <uORB/topics/vehicle_status.h>
#include <uORB/topics/vehicle_attitude_setpoint.h>
#include <uORB/topics/manual_control_setpoint.h>
#include <uORB/topics/actuator_controls.h>
#include <uORB/topics/vehicle_rates_setpoint.h>
#include <systemlib/param/param.h>
#include <systemlib/pid/pid.h>
#include <systemlib/geo/geo.h>
#include <systemlib/systemlib.h>

#include <fixedwing_control2_rate.h>

/* Prototypes */
/**
 * Deamon management function.
 */
__EXPORT int fixedwing_control2_main(int argc, char *argv[]);

/**
 * Mainloop of deamon.
 */
int fixedwing_control2_thread_main(int argc, char *argv[]);

/**
 * Print the correct usage.
 */
static void usage(const char *reason);

/* Variables */
static bool thread_should_exit = false;		/**< Deamon exit flag */
static bool thread_running = false;		/**< Deamon status flag */
static int deamon_task;				/**< Handle of deamon task / thread */

/* Main Thread */
int fixedwing_control2_thread_main(int argc, char *argv[])
{
	/* read arguments */
		bool verbose = false;

		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
				verbose = true;
			}
		}

		/* welcome user */
		printf("[fixedwing control2] started\n");

		/* declare and safely initialize all structs */
		struct vehicle_attitude_s att;
		memset(&att, 0, sizeof(att));
		struct vehicle_rates_setpoint_s rates_sp;
		memset(&rates_sp, 0, sizeof(rates_sp));

		/* output structs */
		struct actuator_controls_s actuators;
		memset(&actuators, 0, sizeof(actuators));

		/* publish actuator controls */
		for (unsigned i = 0; i < NUM_ACTUATOR_CONTROLS; i++)
			actuators.control[i] = 0.0f;
		orb_advert_t actuator_pub = orb_advertise(ORB_ID_VEHICLE_ATTITUDE_CONTROLS, &actuators);



		/* subscribe to attitude (for attitude rate) and rate septoint */
		int att_sub = orb_subscribe(ORB_ID(vehicle_attitude));

		/* Setup of loop */
		float gyro[3] = {0.0f, 0.0f, 0.0f};
		struct pollfd fds = { .fd = att_sub, .events = POLLIN };

		while(!thread_should_exit)
		{
			/* wait for a sensor update, check for exit condition every 500 ms */
			poll(&fds, 1, 500);

			/*Get Local Copies */
			/* get a local copy of attitude */
			orb_copy(ORB_ID(vehicle_attitude), att_sub, &att);

			gyro[0] = att.rollspeed;
			gyro[1] = att.pitchspeed;
			gyro[2] = att.yawspeed;

			/* Control */

			/* Attitude Control */
			rates_sp.roll = 0.0f;
			rates_sp.pitch = 0.0f;
			rates_sp.yaw = 0.0f;

			/* Attitude Rate Control */
			fixedwing_control2_rates(&rates_sp, gyro, &actuators);

			//REMOVEME XXX
			actuators.control[3] = 0.7f;

			orb_publish(ORB_ID_VEHICLE_ATTITUDE_CONTROLS, actuator_pub, &actuators);
		}

		printf("[fixedwing_control2] exiting, stopping all motors.\n");
		thread_running = false;

		/* kill all outputs */
		for (unsigned i = 0; i < NUM_ACTUATOR_CONTROLS; i++)
			actuators.control[i] = 0.0f;
		orb_publish(ORB_ID_VEHICLE_ATTITUDE_CONTROLS, actuator_pub, &actuators);


		close(att_sub);
		close(actuator_pub);

		fflush(stdout);
		exit(0);

		return 0;

}

/* Startup Functions */

static void
usage(const char *reason)
{
	if (reason)
		fprintf(stderr, "%s\n", reason);
	fprintf(stderr, "usage: fixedwing_control2 {start|stop|status}\n\n");
	exit(1);
}

/**
 * The deamon app only briefly exists to start
 * the background job. The stack size assigned in the
 * Makefile does only apply to this management task.
 *
 * The actual stack size should be set in the call
 * to task_create().
 */
int fixedwing_control2_main(int argc, char *argv[])
{
	if (argc < 1)
		usage("missing command");

	if (!strcmp(argv[1], "start")) {

		if (thread_running) {
			printf("fixedwing_control2 already running\n");
			/* this is not an error */
			exit(0);
		}

		thread_should_exit = false;
		deamon_task = task_spawn("fixedwing_control2",
					 SCHED_DEFAULT,
					 SCHED_PRIORITY_MAX - 20,
					 4096,
					 fixedwing_control2_thread_main,
					 (argv) ? (const char **)&argv[2] : (const char **)NULL);
		thread_running = true;
		exit(0);
	}

	if (!strcmp(argv[1], "stop")) {
		thread_should_exit = true;
		exit(0);
	}

	if (!strcmp(argv[1], "status")) {
		if (thread_running) {
			printf("\tfixedwing_control2 is running\n");
		} else {
			printf("\tfixedwing_control2 not started\n");
		}
		exit(0);
	}

	usage("unrecognized command");
	exit(1);
}



