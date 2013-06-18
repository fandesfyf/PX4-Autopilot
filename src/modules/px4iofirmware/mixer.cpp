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
 * @file mixer.cpp
 *
 * Control channel input/output mixer and failsafe.
 */

#include <nuttx/config.h>
#include <syslog.h>

#include <sys/types.h>
#include <stdbool.h>
#include <string.h>

#include <drivers/drv_pwm_output.h>
#include <drivers/drv_hrt.h>

#include <systemlib/mixer/mixer.h>

extern "C" {
//#define DEBUG
#include "px4io.h"
}

/*
 * Maximum interval in us before FMU signal is considered lost
 */
#define FMU_INPUT_DROP_LIMIT_US		200000

/*
 * Time that the ESCs need to initialize
 */
 #define ESC_INIT_TIME_US		1000000

/* XXX need to move the RC_CHANNEL_FUNCTION out of rc_channels.h and into systemlib */
#define ROLL     0
#define PITCH    1
#define YAW      2
#define THROTTLE 3
#define OVERRIDE 4

/* current servo arm/disarm state */
static bool mixer_servos_armed = false;

static uint64_t esc_init_time;
static bool esc_init_active = false;
static bool esc_init_done = false;

/* selected control values and count for mixing */
enum mixer_source {
	MIX_NONE,
	MIX_FMU,
	MIX_OVERRIDE,
	MIX_FAILSAFE
};
static mixer_source source;

static int	mixer_callback(uintptr_t handle,
			       uint8_t control_group,
			       uint8_t control_index,
			       float &control);

static MixerGroup mixer_group(mixer_callback, 0);

/* Set the failsafe values of all mixed channels (based on zero throttle, controls centered) */
static void mixer_set_failsafe();

void
mixer_tick(void)
{
	/* check that we are receiving fresh data from the FMU */
	if (hrt_elapsed_time(&system_state.fmu_data_received_time) > FMU_INPUT_DROP_LIMIT_US) {

		/* too long without FMU input, time to go to failsafe */
		if (r_status_flags & PX4IO_P_STATUS_FLAGS_FMU_OK) {
			isr_debug(1, "AP RX timeout");
		}
		r_status_flags &= ~(PX4IO_P_STATUS_FLAGS_FMU_OK);
		r_status_alarms |= PX4IO_P_STATUS_ALARMS_FMU_LOST;

	} else {
		r_status_flags |= PX4IO_P_STATUS_FLAGS_FMU_OK;
	}

	/* default to failsafe mixing */
	source = MIX_FAILSAFE;

	/*
	 * Decide which set of controls we're using.
	 */

	/* do not mix if RAW_PWM mode is on and FMU is good */
	if ((r_status_flags & PX4IO_P_STATUS_FLAGS_RAW_PWM) &&
	        (r_status_flags & PX4IO_P_STATUS_FLAGS_FMU_OK)) {

		/* don't actually mix anything - we already have raw PWM values */
		source = MIX_NONE;

	} else {

		if (!(r_status_flags & PX4IO_P_STATUS_FLAGS_OVERRIDE) &&
		     (r_status_flags & PX4IO_P_STATUS_FLAGS_FMU_OK) &&
		     (r_status_flags & PX4IO_P_STATUS_FLAGS_MIXER_OK)) {

			/* mix from FMU controls */
			source = MIX_FMU;
		}

		if ( (r_status_flags & PX4IO_P_STATUS_FLAGS_OVERRIDE) &&
		     (r_status_flags & PX4IO_P_STATUS_FLAGS_RC_OK) &&
		     (r_status_flags & PX4IO_P_STATUS_FLAGS_MIXER_OK)) {

		 	/* if allowed, mix from RC inputs directly */
			source = MIX_OVERRIDE;
		}
	}

	/*
	 * Set failsafe status flag depending on mixing source
	 */
	if (source == MIX_FAILSAFE) {
		r_status_flags |= PX4IO_P_STATUS_FLAGS_FAILSAFE;
	} else {
		r_status_flags &= ~(PX4IO_P_STATUS_FLAGS_FAILSAFE);
	}

	/*
	 * Run the mixers.
	 */
	if (source == MIX_FAILSAFE) {

		/* copy failsafe values to the servo outputs */
		for (unsigned i = 0; i < IO_SERVO_COUNT; i++) {
			r_page_servos[i] = r_page_servo_failsafe[i];

			/* safe actuators for FMU feedback */
			r_page_actuators[i] = (r_page_servos[i] - 1500) / 600.0f;
		}


	} else if (source != MIX_NONE) {

		float	outputs[IO_SERVO_COUNT];
		unsigned mixed;

		/* after arming, some ESCs need an initalization period, count the time from here */
		if (mixer_servos_armed && !esc_init_done && !esc_init_active) {
			esc_init_time = hrt_absolute_time();
			esc_init_active = true;
			isr_debug(1, "start counting now");
		}

		/* after waiting long enough for the ESC initialization, we can disable the ESC initialization phase */
		if (!esc_init_done && esc_init_active && mixer_servos_armed && (hrt_elapsed_time(&esc_init_time) > ESC_INIT_TIME_US)) {
			esc_init_active = false;
			esc_init_done = true;
			isr_debug(1, "time is up");
		}

		/* mix */
		mixed = mixer_group.mix(&outputs[0], IO_SERVO_COUNT);

		/* scale to PWM and update the servo outputs as required */
		for (unsigned i = 0; i < mixed; i++) {

			/* save actuator values for FMU readback */
			r_page_actuators[i] = FLOAT_TO_REG(outputs[i]);

			/* XXX maybe this check for an armed FMU could be achieved a little less messy */
			if (source == MIX_FMU && !(r_setup_arming & PX4IO_P_SETUP_ARMING_FMU_ARMED) && (r_setup_arming & PX4IO_P_SETUP_ARMING_FAILSAFE_CUSTOM)) {
				r_page_servos[i] = r_page_servo_failsafe[i];
			}
			/* during ESC initialization, use low PWM */
			else if (esc_init_active) {
				r_page_servos[i] = (outputs[i] * 600 + 1500);
			
			/* afterwards use min and max values */
			} else {
				r_page_servos[i] = (outputs[i]
					* (r_page_servo_control_max[i] - r_page_servo_control_min[i])/2
					+ (r_page_servo_control_max[i] + r_page_servo_control_min[i])/2);
			}

		}
		for (unsigned i = mixed; i < IO_SERVO_COUNT; i++)
			r_page_servos[i] = 0;
	}

	/*
	 * Decide whether the servos should be armed right now.
	 *
	 * We must be armed, and we must have a PWM source; either raw from
	 * FMU or from the mixer.
	 *
	 * XXX correct behaviour for failsafe may require an additional case
	 * here.
	 */
	bool should_arm = (
		/* IO initialised without error */  (r_status_flags & PX4IO_P_STATUS_FLAGS_INIT_OK) &&
		/* IO is armed */  (r_status_flags & PX4IO_P_STATUS_FLAGS_SAFETY_OFF) && 
			/* and either FMU is armed */ ( ( (r_setup_arming & PX4IO_P_SETUP_ARMING_FMU_ARMED) && 
				/* and there is valid input via direct PWM or mixer */ (r_status_flags & (PX4IO_P_STATUS_FLAGS_RAW_PWM | PX4IO_P_STATUS_FLAGS_MIXER_OK))) ||
			
			/* or failsafe was set manually */ (r_setup_arming & PX4IO_P_SETUP_ARMING_FAILSAFE_CUSTOM) )
	);


	if (should_arm && !mixer_servos_armed) {
		/* need to arm, but not armed */
		up_pwm_servo_arm(true);
		mixer_servos_armed = true;
		r_status_flags |= PX4IO_P_STATUS_FLAGS_OUTPUTS_ARMED;
		isr_debug(5, "> armed");

	} else if (!should_arm && mixer_servos_armed) {
		/* armed but need to disarm */
		up_pwm_servo_arm(false);
		mixer_servos_armed = false;
		r_status_flags &= ~(PX4IO_P_STATUS_FLAGS_OUTPUTS_ARMED);
		isr_debug(5, "> disarmed");

		esc_init_active = false;
		isr_debug(1, "disarming, and init aborted");
	}

	if (mixer_servos_armed) {
		/* update the servo outputs. */
		for (unsigned i = 0; i < IO_SERVO_COUNT; i++)
			up_pwm_servo_set(i, r_page_servos[i]);
	}
}

static int
mixer_callback(uintptr_t handle,
	       uint8_t control_group,
	       uint8_t control_index,
	       float &control)
{
	if (control_group != 0)
		return -1;

	switch (source) {
	case MIX_FMU:
		if (control_index < PX4IO_CONTROL_CHANNELS) {
			control = REG_TO_FLOAT(r_page_controls[control_index]);
			break;
		}
		return -1;

	case MIX_OVERRIDE:
		if (r_page_rc_input[PX4IO_P_RC_VALID] & (1 << control_index)) {
			control = REG_TO_FLOAT(r_page_rc_input[PX4IO_P_RC_BASE + control_index]);
			break;
		}
		return -1;

	case MIX_FAILSAFE:
	case MIX_NONE:
		control = 0.0f;
		return -1;
	}

	return 0;
}

/*
 * XXX error handling here should be more aggressive; currently it is
 * possible to get STATUS_FLAGS_MIXER_OK set even though the mixer has
 * not loaded faithfully.
 */

static char mixer_text[256];		/* large enough for one mixer */
static unsigned mixer_text_length = 0;

void
mixer_handle_text(const void *buffer, size_t length)
{
	/* do not allow a mixer change while outputs armed */
	if ((r_status_flags & PX4IO_P_STATUS_FLAGS_OUTPUTS_ARMED)) {
		return;
	}

	px4io_mixdata	*msg = (px4io_mixdata *)buffer;

	isr_debug(2, "mix txt %u", length);

	if (length < sizeof(px4io_mixdata))
		return;

	unsigned	text_length = length - sizeof(px4io_mixdata);

	switch (msg->action) {
	case F2I_MIXER_ACTION_RESET:
		isr_debug(2, "reset");

		/* FIRST mark the mixer as invalid */
		r_status_flags &= ~PX4IO_P_STATUS_FLAGS_MIXER_OK;
		/* THEN actually delete it */
		mixer_group.reset();
		mixer_text_length = 0;

		/* FALLTHROUGH */
	case F2I_MIXER_ACTION_APPEND:
		isr_debug(2, "append %d", length);

		/* check for overflow - this would be really fatal */
		if ((mixer_text_length + text_length + 1) > sizeof(mixer_text)) {
			r_status_flags &= ~PX4IO_P_STATUS_FLAGS_MIXER_OK;
			return;
		}

		/* append mixer text and nul-terminate */
		memcpy(&mixer_text[mixer_text_length], msg->text, text_length);
		mixer_text_length += text_length;
		mixer_text[mixer_text_length] = '\0';
		isr_debug(2, "buflen %u", mixer_text_length);

		/* process the text buffer, adding new mixers as their descriptions can be parsed */
		unsigned resid = mixer_text_length;
		mixer_group.load_from_buf(&mixer_text[0], resid);

		/* if anything was parsed */
		if (resid != mixer_text_length) {

			/* only set mixer ok if no residual is left over */
			if (resid == 0) {
				r_status_flags |= PX4IO_P_STATUS_FLAGS_MIXER_OK;
			} else {
				/* not yet reached the end of the mixer, set as not ok */
				r_status_flags &= ~PX4IO_P_STATUS_FLAGS_MIXER_OK;
			}

			isr_debug(2, "used %u", mixer_text_length - resid);

			/* copy any leftover text to the base of the buffer for re-use */
			if (resid > 0)
				memcpy(&mixer_text[0], &mixer_text[mixer_text_length - resid], resid);

			mixer_text_length = resid;

			/* update failsafe values */
			mixer_set_failsafe();
		}

		break;
	}
}

static void
mixer_set_failsafe()
{
	/* 
	 * Check if a custom failsafe value has been written,
	 * or if the mixer is not ok and bail out.
	 */

	if ((r_setup_arming & PX4IO_P_SETUP_ARMING_FAILSAFE_CUSTOM) ||
		!(r_status_flags & PX4IO_P_STATUS_FLAGS_MIXER_OK))
		return;

	/* set failsafe defaults to the values for all inputs = 0 */
	float	outputs[IO_SERVO_COUNT];
	unsigned mixed;

	/* mix */
	mixed = mixer_group.mix(&outputs[0], IO_SERVO_COUNT);

	/* scale to PWM and update the servo outputs as required */
	for (unsigned i = 0; i < mixed; i++) {

		/* scale to servo output */
		r_page_servo_failsafe[i] = (outputs[i] * 600.0f) + 1500;

	}

	/* disable the rest of the outputs */
	for (unsigned i = mixed; i < IO_SERVO_COUNT; i++)
		r_page_servo_failsafe[i] = 0;

}
