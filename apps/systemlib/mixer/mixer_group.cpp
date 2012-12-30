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
 * @file mixer_group.cpp
 *
 * Mixer collection.
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

#include "mixer.h"

#define debug(fmt, args...)	do { } while(0)
//#define debug(fmt, args...)	do { printf("[mixer] " fmt "\n", ##args); } while(0)

MixerGroup::MixerGroup(ControlCallback control_cb, uintptr_t cb_handle) :
	Mixer(control_cb, cb_handle),
	_first(nullptr)
{
}

MixerGroup::~MixerGroup()
{
	reset();
}

void
MixerGroup::add_mixer(Mixer *mixer)
{
	Mixer **mpp;

	mpp = &_first;

	while (*mpp != nullptr)
		mpp = &((*mpp)->_next);

	*mpp = mixer;
	mixer->_next = nullptr;
}

void
MixerGroup::reset()
{
	Mixer *mixer;

	/* discard sub-mixers */
	while (_first != nullptr) {
		mixer = _first;
		_first = mixer->_next;
		delete mixer;
	}
}

unsigned
MixerGroup::mix(float *outputs, unsigned space)
{
	Mixer	*mixer = _first;
	unsigned index = 0;

	while ((mixer != nullptr) && (index < space)) {
		index += mixer->mix(outputs + index, space - index);
		mixer = mixer->_next;
	}

	return index;
}

void
MixerGroup::groups_required(uint32_t &groups)
{
	Mixer	*mixer = _first;

	while (mixer != nullptr) {
		mixer->groups_required(groups);
		mixer = mixer->_next;
	}
}

int
MixerGroup::load_from_buf(const char *buf, unsigned &buflen)
{
	int ret = -1;
	const char *end = buf + buflen;

	/* loop until we have consumed the buffer */
	while (buflen > 0) {
		Mixer *m = nullptr;
		const char *p = end - buflen;

		/* use the next character as a hint to decide which mixer class to construct */
		switch (*p) {
		case 'Z':
			m = NullMixer::from_text(p, buflen);
			break;

		case 'M':
			m = SimpleMixer::from_text(_control_cb, _cb_handle, p, buflen);
			break;

		case 'R':
			m = MultirotorMixer::from_text(_control_cb, _cb_handle, p, buflen);
			break;

		default:
			/* it's probably junk or whitespace */
			break;
		}

		if (m != nullptr) {
			add_mixer(m);
			ret = 0;

		} else {
			/* skip whitespace or junk in the buffer */
			buflen--;
		}
	}

	return ret;
}
