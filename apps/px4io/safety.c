/****************************************************************************
 *
 *   Copyright (C) 2012 PX4 Development Team. All rights reserved.
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
  * @file Safety button logic.
  */

#include <nuttx/config.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <debug.h>
#include <stdlib.h>
#include <errno.h>

#include <nuttx/clock.h>

#include <drivers/drv_hrt.h>

#include "px4io.h"

static struct hrt_call arming_call;
static struct hrt_call heartbeat_call;
static struct hrt_call failsafe_call;

/*
 * Count the number of times in a row that we see the arming button
 * held down.
 */
static unsigned counter;

/*
 * Used to coordinate a special blink pattern wheb both the FMU and IO are armed.
 */
static unsigned blink_count = 0;

#define ARM_COUNTER_THRESHOLD	10
#define DISARM_COUNTER_THRESHOLD	2

static bool safety_led_state;
static bool safety_button_pressed;

static void safety_check_button(void *arg);
static void heartbeat_blink(void *arg);
static void failsafe_blink(void *arg);

void
safety_init(void)
{
	/* arrange for the button handler to be called at 10Hz */
	hrt_call_every(&arming_call, 1000, 100000, safety_check_button, NULL);

	/* arrange for the heartbeat handler to be called at 4Hz */
	hrt_call_every(&heartbeat_call, 1000, 250000, heartbeat_blink, NULL);

	/* arrange for the failsafe blinker to be called at 8Hz */
	hrt_call_every(&failsafe_call, 1000, 125000, failsafe_blink, NULL);
}

static void
safety_check_button(void *arg)
{
	/* 
	 * Debounce the safety button, change state if it has been held for long enough.
	 *
	 */
	safety_button_pressed = BUTTON_SAFETY;

	if(safety_button_pressed) {
		//printf("Pressed, Arm counter: %d, Disarm counter: %d\n", arm_counter, disarm_counter);
	}

	/* Keep pressed for a while to arm */
	if (safety_button_pressed && !system_state.armed) {
		if (counter < ARM_COUNTER_THRESHOLD) {
			counter++;
		} else if (counter == ARM_COUNTER_THRESHOLD) {
			/* change to armed state and notify the FMU */
			system_state.armed = true;
			counter++;
			system_state.fmu_report_due = true;
		}
	/* Disarm quickly */
	} else if (safety_button_pressed && system_state.armed) {
		if (counter < DISARM_COUNTER_THRESHOLD) {
			counter++;
		} else if (counter == DISARM_COUNTER_THRESHOLD) {
			/* change to disarmed state and notify the FMU */
			system_state.armed = false;
			counter++;
			system_state.fmu_report_due = true;
		}
	} else {
		counter = 0;
	}

	/* 
	 * When the IO is armed, toggle the LED; when IO and FMU armed use aircraft like 
	 * pattern (long pause then 2 fast blinks); when safe, leave it on. 
	 */
	if (system_state.armed) {
		if (system_state.arm_ok) {
			/* FMU and IO are armed */
			if (blink_count > 9) {
				safety_led_state = !safety_led_state;
			} else {
				safety_led_state = false;
			}
			if (blink_count++ == 12) {
				blink_count = 0;
			}
		} else {
			/* Only the IO is armed so use a constant blink rate */
			safety_led_state = !safety_led_state;
		}
	} else {
		safety_led_state = true;
	}
	LED_SAFETY(safety_led_state);
}


static void
heartbeat_blink(void *arg)
{
	static bool heartbeat = false;

	/* XXX add flags here that need to be frobbed by various loops */

	LED_BLUE(heartbeat = !heartbeat);
}

static void
failsafe_blink(void *arg)
{
	static bool failsafe = false;

	/* blink the failsafe LED if we don't have FMU input */
	if (!system_state.mixer_use_fmu) {
		failsafe = !failsafe;
	} else {
		failsafe = false;
	}
	LED_AMBER(failsafe);
}