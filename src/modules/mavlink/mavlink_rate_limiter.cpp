/****************************************************************************
 *
 *   Copyright (c) 2014 PX4 Development Team. All rights reserved.
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
 * @file mavlink_rate_limiter.cpp
 * Message rate limiter implementation.
 *
 * @author Anton Babushkin <anton.babushkin@me.com>
 */

#include "mavlink_rate_limiter.h"

MavlinkRateLimiter::MavlinkRateLimiter() : _last_sent(0), _interval(1000000)
{
}

MavlinkRateLimiter::MavlinkRateLimiter(unsigned int interval) : _last_sent(0), _interval(interval)
{
}

MavlinkRateLimiter::~MavlinkRateLimiter()
{
}

void
MavlinkRateLimiter::set_interval(unsigned int interval)
{
	_interval = interval;
}

bool
MavlinkRateLimiter::check(hrt_abstime t)
{
	uint64_t dt = t - _last_sent;

	if (dt > 0 && dt >= _interval) {
		_last_sent = (t / _interval) * _interval;
		return true;
	}

	return false;
}
