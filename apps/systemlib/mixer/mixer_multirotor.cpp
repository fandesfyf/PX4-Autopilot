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
 * @file mixer_multirotor.cpp
 *
 * Multi-rotor mixers.
 */

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <math.h>

#include "mixer.h"

#define		CW		(-1.0f)
#define		CCW		(1.0f)

namespace 
{

/*
 * These tables automatically generated by multi_tables - do not edit.
 */
const MultirotorMixer::Rotor _config_quad_x[] = {
	{ -0.707107,  0.707107, -1.00 },
	{  0.707107, -0.707107, -1.00 },
	{  0.707107,  0.707107,  1.00 },
	{ -0.707107, -0.707107,  1.00 },
};
const MultirotorMixer::Rotor _config_quad_plus[] = {
	{ -1.000000,  0.000000, -1.00 },
	{  1.000000,  0.000000, -1.00 },
	{  0.000000,  1.000000,  1.00 },
	{ -0.000000, -1.000000,  1.00 },
};
const MultirotorMixer::Rotor _config_hex_x[] = {
	{ -1.000000,  0.000000,  1.00 },
	{  1.000000,  0.000000, -1.00 },
	{  0.500000,  0.866025,  1.00 },
	{ -0.500000, -0.866025, -1.00 },
	{ -0.500000,  0.866025, -1.00 },
	{  0.500000, -0.866025,  1.00 },
};
const MultirotorMixer::Rotor _config_hex_plus[] = {
	{  0.000000,  1.000000,  1.00 },
	{ -0.000000, -1.000000, -1.00 },
	{  0.866025, -0.500000,  1.00 },
	{ -0.866025,  0.500000, -1.00 },
	{  0.866025,  0.500000, -1.00 },
	{ -0.866025, -0.500000,  1.00 },
};
const MultirotorMixer::Rotor _config_octa_x[] = {
	{ -0.382683,  0.923880,  1.00 },
	{  0.382683, -0.923880,  1.00 },
	{ -0.923880,  0.382683, -1.00 },
	{ -0.382683, -0.923880, -1.00 },
	{  0.382683,  0.923880, -1.00 },
	{  0.923880, -0.382683, -1.00 },
	{  0.923880,  0.382683,  1.00 },
	{ -0.923880, -0.382683,  1.00 },
};
const MultirotorMixer::Rotor _config_octa_plus[] = {
	{  0.000000,  1.000000,  1.00 },
	{ -0.000000, -1.000000,  1.00 },
	{ -0.707107,  0.707107, -1.00 },
	{ -0.707107, -0.707107, -1.00 },
	{  0.707107,  0.707107, -1.00 },
	{  0.707107, -0.707107, -1.00 },
	{  1.000000,  0.000000,  1.00 },
	{ -1.000000,  0.000000,  1.00 },
};
const MultirotorMixer::Rotor *_config_index[MultirotorMixer::Geometry::MAX_GEOMETRY] = {
	&_config_quad_x[0],
	&_config_quad_plus[0],
	&_config_hex_x[0],
	&_config_hex_plus[0],
	&_config_octa_x[0],
	&_config_octa_plus[0],
};
const unsigned _config_rotor_count[MultirotorMixer::Geometry::MAX_GEOMETRY] = {
	4, /* quad_x */
	4, /* quad_plus */
	6, /* hex_x */
	6, /* hex_plus */
	8, /* octa_x */
	8, /* octa_plus */
};

}

MultirotorMixer::MultirotorMixer(ControlCallback control_cb,
				 uintptr_t cb_handle,
				 Geometry geometry,
				 float roll_scale,
				 float pitch_scale,
				 float yaw_scale,
				 float deadband) :
	Mixer(control_cb, cb_handle),
	_roll_scale(roll_scale),
	_pitch_scale(pitch_scale),
	_yaw_scale(yaw_scale),
	_deadband(-1.0f + deadband),	/* shift to output range here to avoid runtime calculation */
	_rotor_count(_config_rotor_count[geometry]),
	_rotors(_config_index[geometry])
{
}

MultirotorMixer::~MultirotorMixer()
{
}

unsigned
MultirotorMixer::mix(float *outputs, unsigned space)
{
	float		roll    = get_control(0, 0) * _roll_scale;
	float		pitch   = get_control(0, 1) * _pitch_scale;
	float		yaw     = get_control(0, 2) * _yaw_scale;
	float		thrust  = get_control(0, 3);
	float		max     = 0.0f;
	float		fixup_scale;

	/* perform initial mix pass yielding un-bounded outputs */
	for (unsigned i = 0; i < _rotor_count; i++) {
		float tmp = roll  * _rotors[i].roll_scale +
			    pitch * _rotors[i].pitch_scale +
			    yaw   * _rotors[i].yaw_scale +
			    thrust;
		if (tmp > max)
			max = tmp;
		outputs[i] = tmp;
	}

	/* scale values into the -1.0 - 1.0 range */
	if (max > 1.0f) {
		fixup_scale = 2.0f / max;
	} else {
		fixup_scale = 2.0f;
	}
	for (unsigned i = 0; i < _rotor_count; i++)
		outputs[i] = -1.0f + (outputs[i] * fixup_scale);

	/* ensure outputs are out of the deadband */
	for (unsigned i = 0; i < _rotor_count; i++)
		if (outputs[i] < _deadband)
			outputs[i] = _deadband;

	return 0;
}

void
MultirotorMixer::groups_required(uint32_t &groups)
{
	/* XXX for now, hardcoded to indexes 0-3 in control group zero */
	groups |= (1 << 0);
}

