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
 * @file controls.c
 *
 * R/C inputs and servo outputs.
 */

#include <nuttx/config.h>
#include <stdbool.h>
#include <poll.h>

#include <drivers/drv_hrt.h>
#include <systemlib/perf_counter.h>
#include <systemlib/ppm_decode.h>

//#define DEBUG
#include "px4io.h"

#define RC_FAILSAFE_TIMEOUT		2000000		/**< two seconds failsafe timeout */
#define RC_CHANNEL_HIGH_THRESH		5000
#define RC_CHANNEL_LOW_THRESH		-5000

static bool	ppm_input(uint16_t *values, uint16_t *num_values);

void
controls_main(void)
{
	struct pollfd fds[2];

	/* DSM input */
	fds[0].fd = dsm_init("/dev/ttyS0");
	fds[0].events = POLLIN;

	/* S.bus input */
	fds[1].fd = sbus_init("/dev/ttyS2");
	fds[1].events = POLLIN;

	ASSERT(fds[0].fd >= 0);
	ASSERT(fds[1].fd >= 0);

	/* default to a 1:1 input map */
	for (unsigned i = 0; i < MAX_CONTROL_CHANNELS; i++) {
		unsigned base = PX4IO_P_RC_CONFIG_STRIDE * i;

		r_page_rc_input_config[base + PX4IO_P_RC_CONFIG_OPTIONS]    = 0;
		r_page_rc_input_config[base + PX4IO_P_RC_CONFIG_MIN]        = 1000;
		r_page_rc_input_config[base + PX4IO_P_RC_CONFIG_CENTER]     = 1500;
		r_page_rc_input_config[base + PX4IO_P_RC_CONFIG_MAX]        = 2000;
		r_page_rc_input_config[base + PX4IO_P_RC_CONFIG_DEADZONE]   = 30;
		r_page_rc_input_config[base + PX4IO_P_RC_CONFIG_ASSIGNMENT] = i;
	}

	for (;;) {
		/* run this loop at ~100Hz */
		int result = poll(fds, 2, 10);

		ASSERT(result >= 0);

		/*
		 * Gather R/C control inputs from supported sources.
		 *
		 * Note that if you're silly enough to connect more than
		 * one control input source, they're going to fight each
		 * other.  Don't do that.
		 */

		bool dsm_updated = dsm_input(r_raw_rc_values, &r_raw_rc_count);
		if (dsm_updated)
			r_status_flags |= PX4IO_P_STATUS_FLAGS_RC_DSM;

		bool sbus_updated = sbus_input(r_raw_rc_values, &r_raw_rc_count);
		if (sbus_updated)
			r_status_flags |= PX4IO_P_STATUS_FLAGS_RC_SBUS;

		/*
		 * XXX each S.bus frame will cause a PPM decoder interrupt
		 * storm (lots of edges).  It might be sensible to actually
		 * disable the PPM decoder completely if we have S.bus signal.
		 */
		bool ppm_updated = ppm_input(r_raw_rc_values, &r_raw_rc_count);
		if (ppm_updated)
			r_status_flags |= PX4IO_P_STATUS_FLAGS_RC_PPM;

		ASSERT(r_raw_rc_count <= MAX_CONTROL_CHANNELS);

		/*
		 * In some cases we may have received a frame, but input has still
		 * been lost.
		 */
		bool rc_input_lost = false;

		/*
		 * If we received a new frame from any of the RC sources, process it.
		 */
		if (dsm_updated | sbus_updated | ppm_updated) {

			/* update RC-received timestamp */
			system_state.rc_channels_timestamp = hrt_absolute_time();

			/* record a bitmask of channels assigned */
			unsigned assigned_channels = 0;

			/* map raw inputs to mapped inputs */
			/* XXX mapping should be atomic relative to protocol */
			for (unsigned i = 0; i < r_raw_rc_count; i++) {

				/* map the input channel */
				uint16_t *conf = &r_page_rc_input_config[i * PX4IO_P_RC_CONFIG_STRIDE];

				if (conf[PX4IO_P_RC_CONFIG_OPTIONS] && PX4IO_P_RC_CONFIG_OPTIONS_ENABLED) {

					uint16_t raw = r_raw_rc_values[i];

					/* implement the deadzone */
					if (raw < conf[PX4IO_P_RC_CONFIG_CENTER]) {
						raw += conf[PX4IO_P_RC_CONFIG_DEADZONE];
						if (raw > conf[PX4IO_P_RC_CONFIG_CENTER])
							raw = conf[PX4IO_P_RC_CONFIG_CENTER];
					}
					if (raw > conf[PX4IO_P_RC_CONFIG_CENTER]) {
						raw -= conf[PX4IO_P_RC_CONFIG_DEADZONE];
						if (raw < conf[PX4IO_P_RC_CONFIG_CENTER])
							raw = conf[PX4IO_P_RC_CONFIG_CENTER];
					}

					/* constrain to min/max values */
					if (raw < conf[PX4IO_P_RC_CONFIG_MIN])
						raw = conf[PX4IO_P_RC_CONFIG_MIN];
					if (raw > conf[PX4IO_P_RC_CONFIG_MAX])
						raw = conf[PX4IO_P_RC_CONFIG_MAX];

					int16_t scaled = raw;

					/* adjust to zero-relative (-500..500) */
					scaled -= 1500;

					/* scale to fixed-point representation (-10000..10000) */
					scaled *= 20;

					ASSERT(scaled >= -15000);
					ASSERT(scaled <= 15000);

					if (conf[PX4IO_P_RC_CONFIG_OPTIONS] & PX4IO_P_RC_CONFIG_OPTIONS_REVERSE)
						scaled = -scaled;

					/* and update the scaled/mapped version */
					unsigned mapped = conf[PX4IO_P_RC_CONFIG_ASSIGNMENT];
					ASSERT(mapped < MAX_CONTROL_CHANNELS);

					r_rc_values[mapped] = SIGNED_TO_REG(scaled);
					assigned_channels |= (1 << mapped);
				}
			}

			/* set un-assigned controls to zero */
			for (unsigned i = 0; i < MAX_CONTROL_CHANNELS; i++) {
				if (!(assigned_channels & (1 << i)))
					r_rc_values[i] = 0;
			}

			/*
			 * If we got an update with zero channels, treat it as 
			 * a loss of input.
			 */
			if (assigned_channels == 0)
				rc_input_lost = true;

			/*
			 * Export the valid channel bitmap
			 */
			r_rc_valid = assigned_channels;
		}

		/*
		 * If we haven't seen any new control data in 200ms, assume we
		 * have lost input.
		 */
		if ((hrt_absolute_time() - system_state.rc_channels_timestamp) > 200000) {
			rc_input_lost = true;
		}

		/*
		 * Handle losing RC input
		 */
		if (rc_input_lost) {

			/* Clear the RC input status flags, clear manual override control */
			r_status_flags &= ~(
				PX4IO_P_STATUS_FLAGS_OVERRIDE |
				PX4IO_P_STATUS_FLAGS_RC_OK |
				PX4IO_P_STATUS_FLAGS_RC_PPM |
				PX4IO_P_STATUS_FLAGS_RC_DSM |
				PX4IO_P_STATUS_FLAGS_RC_SBUS);

			/* Set the RC_LOST alarm */
			r_status_alarms |= PX4IO_P_STATUS_ALARMS_RC_LOST;

			/* Mark the arrays as empty */
			r_raw_rc_count = 0;
			r_rc_valid = 0;
		}

		/*
		 * Check for manual override.
		 *
		 * The OVERRIDE_OK feature must be set, and we must have R/C input.
		 * Override is enabled if either the hardcoded channel / value combination
		 * is selected, or the AP has requested it.
		 */
		if ((r_setup_features & PX4IO_P_FEAT_ARMING_MANUAL_OVERRIDE_OK) && 
			(r_status_flags & PX4IO_P_STATUS_FLAGS_RC_OK)) {

			bool override = false;

			/*
			 * Check mapped channel 5; if the value is 'high' then the pilot has
			 * requested override.
			 */
			if ((r_rc_valid & (1 << 4)) && (r_rc_values[4] > RC_CHANNEL_HIGH_THRESH))
				override = true;

			/*
			 * Check for an explicit manual override request from the AP.
			 */
			if ((r_status_flags & PX4IO_P_STATUS_FLAGS_FMU_OK) &&
				(r_setup_arming & PX4IO_P_SETUP_ARMING_MANUAL_OVERRIDE))
				override = true;

			if (override) {

				r_status_flags |= PX4IO_P_STATUS_FLAGS_OVERRIDE;

				/* mix new RC input control values to servos */
				if (dsm_updated | sbus_updated | ppm_updated)
					mixer_tick();

			} else {
				r_status_flags &= ~PX4IO_P_STATUS_FLAGS_OVERRIDE;
			}
		}

	}
}

static bool
ppm_input(uint16_t *values, uint16_t *num_values)
{
	bool result = false;

	/* avoid racing with PPM updates */
	irqstate_t state = irqsave();

	/*
	 * Look for recent PPM input.
	 */
	if ((hrt_absolute_time() - ppm_last_valid_decode) < 200000) {

		/* PPM data exists, copy it */
		result = true;
		*num_values = ppm_decoded_channels;
		if (*num_values > MAX_CONTROL_CHANNELS)
			*num_values = MAX_CONTROL_CHANNELS;

		for (unsigned i = 0; i < *num_values; i++)
			values[i] = ppm_buffer[i];

		/* clear validity */
		ppm_last_valid_decode = 0;
	}

	irqrestore(state);

	return result;
}
