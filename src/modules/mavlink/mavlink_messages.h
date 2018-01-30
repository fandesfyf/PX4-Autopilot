/****************************************************************************
 *
 *   Copyright (c) 2012-2014 PX4 Development Team. All rights reserved.
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
 * @file mavlink_messages.h
 * MAVLink 1.0 message formatters definition.
 *
 * @author Anton Babushkin <anton.babushkin@me.com>
 */

#ifndef MAVLINK_MESSAGES_H_
#define MAVLINK_MESSAGES_H_

#include "mavlink_stream.h"

#include <limits>

class StreamListItem
{

public:
	MavlinkStream *(*new_instance)(Mavlink *mavlink);
	const char *(*get_name)();
	uint16_t (*get_id)();

	StreamListItem(MavlinkStream * (*inst)(Mavlink *mavlink), const char *(*name)(), uint16_t (*id)()) :
		new_instance(inst),
		get_name(name),
		get_id(id) {}

};

const char *get_stream_name(const uint16_t msg_id);
MavlinkStream *create_mavlink_stream(const char *stream_name, Mavlink *mavlink);

class SimpleAnalyzer
{
public:
	enum Mode {
		AVERAGE = 0,
		MIN,
		MAX,
	};

	SimpleAnalyzer(Mode mode, float window = 60.0f);
	virtual ~SimpleAnalyzer();

	void reset();
	void add_value(float val, float update_rate);
	bool valid() const;
	float get() const;
	float get_scaled(float scalingfactor) const;

	template <typename T>
	void get_scaled(T &ret, float scalingfactor) const
	{
		float avg = get_scaled(scalingfactor);
		int_round(avg);
		check_limits(avg, std::numeric_limits<T>::min(), std::numeric_limits<T>::max());

		ret = static_cast<T>(avg);
	}

private:
	unsigned int _n = 0;
	float _window = 60.0f;
	Mode _mode = AVERAGE;
	float _result = 0.0f;

	void check_limits(float &x, float min, float max) const;
	void int_round(float &x) const;
};

template<typename Tin, typename Tout>
void convert_limit_safe(Tin in, Tout *out)
{
	if (in > std::numeric_limits<Tout>::max()) {
		*out = std::numeric_limits<Tout>::max();

	} else if (in < std::numeric_limits<Tout>::min()) {
		*out = std::numeric_limits<Tout>::min();

	} else {
		*out = static_cast<Tout>(in);
	}
}

#endif /* MAVLINK_MESSAGES_H_ */
