/****************************************************************************
 *
 *   Copyright (C) 2008-2012 PX4 Development Team. All rights reserved.
 *   Author: @author Laurens Mackay <mackayl@student.ethz.ch>
 *           @author Tobias Naegeli <naegelit@student.ethz.ch>
 *           @author Martin Rutschmann <rutmarti@student.ethz.ch>
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
 * @file pid.c
 * Implementation of generic PID control interface
 */

#include "pid.h"
#include <math.h>

__EXPORT void pid_init(PID_t *pid, float kp, float ki, float kd, float intmax,
		       float limit, float diff_filter_factor, uint8_t mode)
{
	pid->kp = kp;
	pid->ki = ki;
	pid->kd = kd;
	pid->intmax = intmax;
	pid->limit = limit;
	pid->mode = mode;
	pid->diff_filter_factor = diff_filter_factor;
	pid->count = 0.0f;
	pid->saturated = 0.0f;
	pid->last_output = 0.0f;

	pid->sp = 0.0f;
	pid->error_previous_filtered = 0.0f;
	pid->control_previous = 0.0f;
	pid->integral = 0.0f;
}
__EXPORT int pid_set_parameters(PID_t *pid, float kp, float ki, float kd, float intmax, float limit, float diff_filter_factor)
{
	int ret = 0;

	if (isfinite(kp)) {
		pid->kp = kp;

	} else {
		ret = 1;
	}

	if (isfinite(ki)) {
		pid->ki = ki;

	} else {
		ret = 1;
	}

	if (isfinite(kd)) {
		pid->kd = kd;

	} else {
		ret = 1;
	}

	if (isfinite(intmax)) {
		pid->intmax = intmax;

	}  else {
		ret = 1;
	}

	if (isfinite(limit)) {
		pid->limit = limit;

	}  else {
		ret = 1;
	}

	if (isfinite(diff_filter_factor)) {
		pid->diff_filter_factor = diff_filter_factor;

	}  else {
		ret = 1;
	}

	return ret;
}

//void pid_set(PID_t *pid, float sp)
//{
//	pid->sp = sp;
//	pid->error_previous = 0;
//	pid->integral = 0;
//}

/**
 *
 * @param pid
 * @param val
 * @param dt
 * @return
 */
__EXPORT float pid_calculate(PID_t *pid, float sp, float val, float val_dot, float dt, float *ctrl_p, float *ctrl_i, float *ctrl_d)
{
	/*  error = setpoint - actual_position
	 integral = integral + (error*dt)
	 derivative = (error - previous_error)/dt
	 output = (Kp*error) + (Ki*integral) + (Kd*derivative)
	 previous_error = error
	 wait(dt)
	 goto start
	 */

	if (!isfinite(sp) || !isfinite(val) || !isfinite(val_dot) || !isfinite(dt)) {
		return pid->last_output;
	}

	float i, d;
	pid->sp = sp;

	// Calculated current error value
	float error = pid->sp - val;
	float error_filtered;

	// Calculate or measured current error derivative

	if (pid->mode == PID_MODE_DERIVATIV_CALC) {

		error_filtered = pid->error_previous_filtered + (error - pid->error_previous_filtered) * pid->diff_filter_factor;
		d = (error_filtered - pid->error_previous_filtered) / fmaxf(dt, 0.003f);		// fail-safe for too low dt
		pid->error_previous_filtered = error_filtered;
	} else if (pid->mode == PID_MODE_DERIVATIV_CALC_NO_SP) {

		error_filtered = pid->error_previous_filtered + (- val - pid->error_previous_filtered) * pid->diff_filter_factor;
		d = (error_filtered - pid->error_previous_filtered) / fmaxf(dt, 0.003f);		// fail-safe for too low dt
		pid->error_previous_filtered = error_filtered;
	} else if (pid->mode == PID_MODE_DERIVATIV_SET) {
		d = -val_dot;

	} else {
		d = 0.0f;
	}

	// Calculate the error integral and check for saturation
	i = pid->integral + (error * dt);

	if (fabsf((error * pid->kp) + (i * pid->ki) + (d * pid->kd)) > pid->limit ||
	    fabsf(i) > pid->intmax) {
		i = pid->integral;		// If saturated then do not update integral value
		pid->saturated = 1;

	} else {
		if (!isfinite(i)) {
			i = 0;
		}

		pid->integral = i;
		pid->saturated = 0;
	}

	// Calculate the output.  Limit output magnitude to pid->limit
	float output = (error * pid->kp) + (i * pid->ki) + (d * pid->kd);
	if (output > pid->limit) output = pid->limit;

	if (output < -pid->limit) output = -pid->limit;

	if (isfinite(output)) {
		pid->last_output = output;
	}

	pid->control_previous = pid->last_output;

	// if (isfinite(error)) {				// Why is this necessary?  DEW
	// 	pid->error_previous = error;
	// }

	*ctrl_p = (error * pid->kp);
	*ctrl_i = (i * pid->ki);
	*ctrl_d = (d * pid->kd);

	return pid->last_output;
}


__EXPORT void pid_reset_integral(PID_t *pid)
{
	pid->integral = 0;
}
