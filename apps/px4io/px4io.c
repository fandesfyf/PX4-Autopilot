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
 * @file px4io.c
 * Top-level logic for the PX4IO module.
 */

#include <nuttx/config.h>

#include <stdio.h>	// required for task_create
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

#include <drivers/drv_pwm_output.h>
#include <drivers/drv_hrt.h>

#include <systemlib/perf_counter.h>

#include <stm32_uart.h>

#define DEBUG
#include "px4io.h"

__EXPORT int user_start(int argc, char *argv[]);

extern void up_cxxinitialize(void);

struct sys_state_s 	system_state;

static struct hrt_call serial_dma_call;

// global debug level for isr_debug()
volatile uint8_t debug_level = 0;
volatile uint32_t i2c_resets = 0;
volatile uint32_t i2c_loop_resets = 0;


/*
  a set of debug buffers to allow us to send debug information from ISRs
 */

static volatile uint32_t msg_counter;
static volatile uint32_t last_msg_counter;
static volatile uint8_t msg_next_out, msg_next_in;
#define NUM_MSG 6
static char msg[NUM_MSG][60];

/*
  add a debug message to be printed on the console
 */
void isr_debug(uint8_t level, const char *fmt, ...)
{
	if (level > debug_level) {
		return;
	}
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg[msg_next_in], sizeof(msg[0]), fmt, ap);
	va_end(ap);
	msg_next_in = (msg_next_in+1) % NUM_MSG;
	msg_counter++;
}

/*
  show all pending debug messages
 */
static void show_debug_messages(void)
{
	if (msg_counter != last_msg_counter) {
		uint32_t n = msg_counter - last_msg_counter;
		if (n > NUM_MSG) n = NUM_MSG;
		last_msg_counter = msg_counter;
		while (n--) {
			debug("%s", msg[msg_next_out]);
			msg_next_out = (msg_next_out+1) % NUM_MSG;
		}
	}
}

int user_start(int argc, char *argv[])
{
	/* run C++ ctors before we go any further */
	up_cxxinitialize();

	/* reset all to zero */
	memset(&system_state, 0, sizeof(system_state));

	/* configure the high-resolution time/callout interface */
	hrt_init();

	/*
	 * Poll at 1ms intervals for received bytes that have not triggered
	 * a DMA event.
	 */
	hrt_call_every(&serial_dma_call, 1000, 1000, (hrt_callout)stm32_serial_dma_poll, NULL);

	/* print some startup info */
	debug("\nPX4IO: starting\n");

	/* default all the LEDs to off while we start */
	LED_AMBER(false);
	LED_BLUE(false);
	LED_SAFETY(false);

	/* turn on servo power */
	POWER_SERVO(true);

	/* start the safety switch handler */
	safety_init();

	/* configure the first 8 PWM outputs (i.e. all of them) */
	up_pwm_servo_init(0xff);

	/* start the flight control signal handler */
	task_create("FCon",
		    SCHED_PRIORITY_DEFAULT,
		    1024,
		    (main_t)controls_main,
		    NULL);


	struct mallinfo minfo = mallinfo();
	debug("free %u largest %u\n", minfo.mxordblk, minfo.fordblks);

	debug("debug_level=%u\n", (unsigned)debug_level);

	/* start the i2c handler */
	i2c_init();

	/* add a performance counter for mixing */
	perf_counter_t mixer_perf = perf_alloc(PC_ELAPSED, "mix");

	/* run the mixer at 100Hz (for now...) */
	/* XXX we should use CONFIG_IDLE_CUSTOM and take over the idle thread instead of running two additional tasks */
	uint8_t counter=0;
	for (;;) {
		poll(NULL, 0, 10);
		perf_begin(mixer_perf);
		mixer_tick();
		perf_end(mixer_perf);
		show_debug_messages();
		if (counter++ == 200) {
			counter = 0;
			isr_debug(0, "tick debug=%u status=0x%x resets=%u loop_resets=%u", 
				  (unsigned)debug_level,
				  (unsigned)r_status_flags,
				  (unsigned)i2c_resets,
				  (unsigned)i2c_loop_resets);
		}
	}
}
