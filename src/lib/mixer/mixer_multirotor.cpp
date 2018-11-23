/****************************************************************************
 *
 *   Copyright (c) 2012-2018 PX4 Development Team. All rights reserved.
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
 * @file mixer_multirotor.cpp
 *
 * Multi-rotor mixers.
 */

#include "mixer.h"

#include <cfloat>
#include <cstring>

#include <mathlib/mathlib.h>

// This file is generated by the px_generate_mixers.py script which is invoked during the build process
// #include "mixer_multirotor.generated.h"
#include "mixer_multirotor_normalized.generated.h"

#define debug(fmt, args...)	do { } while(0)
//#define debug(fmt, args...)	do { printf("[mixer] " fmt "\n", ##args); } while(0)
//#include <debug.h>
//#define debug(fmt, args...)	syslog(fmt "\n", ##args)

/*
 * Clockwise: 1
 * Counter-clockwise: -1
 */

MultirotorMixer::MultirotorMixer(ControlCallback control_cb,
				 uintptr_t cb_handle,
				 MultirotorGeometry geometry,
				 float roll_scale,
				 float pitch_scale,
				 float yaw_scale,
				 float idle_speed) :
	Mixer(control_cb, cb_handle),
	_roll_scale(roll_scale),
	_pitch_scale(pitch_scale),
	_yaw_scale(yaw_scale),
	_idle_speed(-1.0f + idle_speed * 2.0f),	/* shift to output range here to avoid runtime calculation */
	_delta_out_max(0.0f),
	_thrust_factor(0.0f),
	_airmode(Airmode::disabled),
	_rotor_count(_config_rotor_count[(MultirotorGeometryUnderlyingType)geometry]),
	_rotors(_config_index[(MultirotorGeometryUnderlyingType)geometry]),
	_outputs_prev(new float[_rotor_count])
{
	for (unsigned i = 0; i < _rotor_count; ++i) {
		_outputs_prev[i] = _idle_speed;
	}
}

MultirotorMixer::~MultirotorMixer()
{
	if (_outputs_prev != nullptr) {
		delete[] _outputs_prev;
	}
}

MultirotorMixer *
MultirotorMixer::from_text(Mixer::ControlCallback control_cb, uintptr_t cb_handle, const char *buf, unsigned &buflen)
{
	MultirotorGeometry geometry = MultirotorGeometry::MAX_GEOMETRY;
	char geomname[8];
	int s[4];
	int used;

	/* enforce that the mixer ends with a new line */
	if (!string_well_formed(buf, buflen)) {
		return nullptr;
	}

	if (sscanf(buf, "R: %7s %d %d %d %d%n", geomname, &s[0], &s[1], &s[2], &s[3], &used) != 5) {
		debug("multirotor parse failed on '%s'", buf);
		return nullptr;
	}

	if (used > (int)buflen) {
		debug("OVERFLOW: multirotor spec used %d of %u", used, buflen);
		return nullptr;
	}

	buf = skipline(buf, buflen);

	if (buf == nullptr) {
		debug("no line ending, line is incomplete");
		return nullptr;
	}

	debug("remaining in buf: %d, first char: %c", buflen, buf[0]);

	for (MultirotorGeometryUnderlyingType i = 0; i < (MultirotorGeometryUnderlyingType)MultirotorGeometry::MAX_GEOMETRY;
	     i++) {
		if (!strcmp(geomname, _config_key[i])) {
			geometry = (MultirotorGeometry)i;
			break;
		}
	}

	if (geometry == MultirotorGeometry::MAX_GEOMETRY) {
		debug("unrecognised geometry '%s'", geomname);
		return nullptr;
	}

	debug("adding multirotor mixer '%s'", geomname);

	return new MultirotorMixer(
		       control_cb,
		       cb_handle,
		       geometry,
		       s[0] / 10000.0f,
		       s[1] / 10000.0f,
		       s[2] / 10000.0f,
		       s[3] / 10000.0f);
}

unsigned
MultirotorMixer::mix(float *outputs, unsigned space)
{
	/* Summary of mixing strategy:
	1) mix roll, pitch and thrust without yaw.
	2) if some outputs violate range [0,1] then try to shift all outputs to minimize violation ->
		increase or decrease total thrust (boost). The total increase or decrease of thrust is limited
		(max_thrust_diff). If after the shift some outputs still violate the bounds then scale roll & pitch.
		In case there is violation at the lower and upper bound then try to shift such that violation is equal
		on both sides.
	3) mix in yaw and scale if it leads to limit violation.
	4) scale all outputs to range [idle_speed,1]
	*/

	float		roll    = math::constrain(get_control(0, 0) * _roll_scale, -1.0f, 1.0f);
	float		pitch   = math::constrain(get_control(0, 1) * _pitch_scale, -1.0f, 1.0f);
	float		yaw     = math::constrain(get_control(0, 2) * _yaw_scale, -1.0f, 1.0f);
	float		thrust  = math::constrain(get_control(0, 3), 0.0f, 1.0f);
	float		min_out = 1.0f;
	float		max_out = 0.0f;

	// clean out class variable used to capture saturation
	_saturation_status.value = 0;

	/* perform initial mix pass yielding unbounded outputs, ignore yaw */
	for (unsigned i = 0; i < _rotor_count; i++) {
		float out = roll * _rotors[i].roll_scale +
			    pitch * _rotors[i].pitch_scale +
			    thrust * _rotors[i].thrust_scale;

		/* calculate min and max output values */
		if (out < min_out) {
			min_out = out;
		}

		if (out > max_out) {
			max_out = out;
		}

		outputs[i] = out;
	}

	float boost = 0.0f;		// value added to demanded thrust (can also be negative)
	float roll_pitch_scale = 1.0f;	// scale for demanded roll and pitch
	float delta_out_max = max_out - min_out; // distance between the two extrema

	// If the difference between the to extrema is smaller than 1.0, the boost can safely unsaturate a motor if needed
	// without saturating another one.
	// Otherwise, a scaler is computed to make the distance between the two extrema exacly 1.0 and the boost
	// value is computed to maximize the roll-pitch control.
	//
	// Note: thrust boost is computed assuming thrust_scale==1 for all motors.
	// On asymmetric platforms, some motors have thrust_scale<1,
	// which may result in motor saturation after thrust boost is applied
	// TODO: revise the saturation/boosting strategy
	if (delta_out_max <= 1.0f) {
		if (min_out < 0.0f) {
			boost = -min_out;

		} else if (max_out > 1.0f) {
			boost = -(max_out - 1.0f);
		}

	} else {
		roll_pitch_scale = 1.0f / (delta_out_max);
		boost = 1.0f - ((max_out - thrust) * roll_pitch_scale + thrust);
	}

	if (_airmode == Airmode::disabled) {
		// disable positive boosting if not in air-mode
		// boosting can only be positive when min_out < 0.0
		// roll_pitch_scale is reduced accordingly
		if (boost > 0.0f) {
			roll_pitch_scale = thrust / (thrust - min_out);
			boost = 0.0f;
		}
	}

	// capture saturation
	if (min_out < 0.0f) {
		_saturation_status.flags.motor_neg = true;
	}

	if (max_out > 1.0f) {
		_saturation_status.flags.motor_pos = true;
	}

	// Thrust reduction is used to reduce the collective thrust if we hit
	// the upper throttle limit
	float thrust_reduction = 0.0f;

	// mix again but now with thrust boost, scale roll/pitch and also add yaw
	for (unsigned i = 0; i < _rotor_count; i++) {
		float out = (roll * _rotors[i].roll_scale +
			     pitch * _rotors[i].pitch_scale) * roll_pitch_scale +
			    yaw * _rotors[i].yaw_scale +
			    (thrust + boost) * _rotors[i].thrust_scale;

		// scale yaw if it violates limits. inform about yaw limit reached
		if (out < 0.0f) {
			if (fabsf(_rotors[i].yaw_scale) <= FLT_EPSILON) {
				yaw = 0.0f;

			} else {
				yaw = -((roll * _rotors[i].roll_scale + pitch * _rotors[i].pitch_scale) *
					roll_pitch_scale + thrust + boost) / _rotors[i].yaw_scale;
			}

		} else if (out > 1.0f) {
			// allow to reduce thrust to get some yaw response
			float prop_reduction = fminf(0.15f, out - 1.0f);
			// keep the maximum requested reduction
			thrust_reduction = fmaxf(thrust_reduction, prop_reduction);

			if (fabsf(_rotors[i].yaw_scale) <= FLT_EPSILON) {
				yaw = 0.0f;

			} else {
				yaw = (1.0f - ((roll * _rotors[i].roll_scale + pitch * _rotors[i].pitch_scale) *
					       roll_pitch_scale + (thrust - thrust_reduction) + boost)) / _rotors[i].yaw_scale;
			}
		}
	}

	// Apply collective thrust reduction, the maximum for one prop
	thrust -= thrust_reduction;

	// add yaw and scale outputs to range idle_speed...1
	for (unsigned i = 0; i < _rotor_count; i++) {
		outputs[i] = (roll * _rotors[i].roll_scale +
			      pitch * _rotors[i].pitch_scale) * roll_pitch_scale +
			     yaw * _rotors[i].yaw_scale +
			     (thrust + boost) * _rotors[i].thrust_scale;

		/*
			implement simple model for static relationship between applied motor pwm and motor thrust
			model: thrust = (1 - _thrust_factor) * PWM + _thrust_factor * PWM^2
			this model assumes normalized input / output in the range [0,1] so this is the right place
			to do it as at this stage the outputs are in that range.
		 */
		if (_thrust_factor > 0.0f) {
			outputs[i] = -(1.0f - _thrust_factor) / (2.0f * _thrust_factor) + sqrtf((1.0f - _thrust_factor) *
					(1.0f - _thrust_factor) / (4.0f * _thrust_factor * _thrust_factor) + (outputs[i] < 0.0f ? 0.0f : outputs[i] /
							_thrust_factor));
		}

		outputs[i] = math::constrain(_idle_speed + (outputs[i] * (1.0f - _idle_speed)), _idle_speed, 1.0f);

	}

	/* slew rate limiting and saturation checking */
	for (unsigned i = 0; i < _rotor_count; i++) {
		bool clipping_high = false;
		bool clipping_low = false;

		// check for saturation against static limits
		if (outputs[i] > 0.99f) {
			clipping_high = true;

		} else if (outputs[i] < _idle_speed + 0.01f) {
			clipping_low = true;

		}

		// check for saturation against slew rate limits
		if (_delta_out_max > 0.0f) {
			float delta_out = outputs[i] - _outputs_prev[i];

			if (delta_out > _delta_out_max) {
				outputs[i] = _outputs_prev[i] + _delta_out_max;
				clipping_high = true;

			} else if (delta_out < -_delta_out_max) {
				outputs[i] = _outputs_prev[i] - _delta_out_max;
				clipping_low = true;

			}
		}

		_outputs_prev[i] = outputs[i];

		// update the saturation status report
		update_saturation_status(i, clipping_high, clipping_low);
	}

	// this will force the caller of the mixer to always supply new slew rate values, otherwise no slew rate limiting will happen
	_delta_out_max = 0.0f;

	return _rotor_count;
}

/*
 * This function update the control saturation status report using the following inputs:
 *
 * index: 0 based index identifying the motor that is saturating
 * clipping_high: true if the motor demand is being limited in the positive direction
 * clipping_low: true if the motor demand is being limited in the negative direction
*/
void
MultirotorMixer::update_saturation_status(unsigned index, bool clipping_high, bool clipping_low)
{
	// The motor is saturated at the upper limit
	// check which control axes and which directions are contributing
	if (clipping_high) {
		if (_rotors[index].roll_scale > 0.0f) {
			// A positive change in roll will increase saturation
			_saturation_status.flags.roll_pos = true;

		} else if (_rotors[index].roll_scale < 0.0f) {
			// A negative change in roll will increase saturation
			_saturation_status.flags.roll_neg = true;
		}

		// check if the pitch input is saturating
		if (_rotors[index].pitch_scale > 0.0f) {
			// A positive change in pitch will increase saturation
			_saturation_status.flags.pitch_pos = true;

		} else if (_rotors[index].pitch_scale < 0.0f) {
			// A negative change in pitch will increase saturation
			_saturation_status.flags.pitch_neg = true;
		}

		// check if the yaw input is saturating
		if (_rotors[index].yaw_scale > 0.0f) {
			// A positive change in yaw will increase saturation
			_saturation_status.flags.yaw_pos = true;

		} else if (_rotors[index].yaw_scale < 0.0f) {
			// A negative change in yaw will increase saturation
			_saturation_status.flags.yaw_neg = true;
		}

		// A positive change in thrust will increase saturation
		_saturation_status.flags.thrust_pos = true;

	}

	// The motor is saturated at the lower limit
	// check which control axes and which directions are contributing
	if (clipping_low) {
		// check if the roll input is saturating
		if (_rotors[index].roll_scale > 0.0f) {
			// A negative change in roll will increase saturation
			_saturation_status.flags.roll_neg = true;

		} else if (_rotors[index].roll_scale < 0.0f) {
			// A positive change in roll will increase saturation
			_saturation_status.flags.roll_pos = true;
		}

		// check if the pitch input is saturating
		if (_rotors[index].pitch_scale > 0.0f) {
			// A negative change in pitch will increase saturation
			_saturation_status.flags.pitch_neg = true;

		} else if (_rotors[index].pitch_scale < 0.0f) {
			// A positive change in pitch will increase saturation
			_saturation_status.flags.pitch_pos = true;
		}

		// check if the yaw input is saturating
		if (_rotors[index].yaw_scale > 0.0f) {
			// A negative change in yaw will increase saturation
			_saturation_status.flags.yaw_neg = true;

		} else if (_rotors[index].yaw_scale < 0.0f) {
			// A positive change in yaw will increase saturation
			_saturation_status.flags.yaw_pos = true;
		}

		// A negative change in thrust will increase saturation
		_saturation_status.flags.thrust_neg = true;
	}

	_saturation_status.flags.valid = true;
}

void
MultirotorMixer::set_airmode(Airmode airmode)
{
	_airmode = airmode;
}

void
MultirotorMixer::groups_required(uint32_t &groups)
{
	/* XXX for now, hardcoded to indexes 0-3 in control group zero */
	groups |= (1 << 0);
}

uint16_t MultirotorMixer::get_saturation_status()
{
	return _saturation_status.value;
}
