/****************************************************************************
 *
 *   Copyright (c) 2019 PX4 Development Team. All rights reserved.
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
 * @file Publication.hpp
 *
 */

#pragma once

#include <px4_platform_common/defines.h>
#include <systemlib/err.h>
#include <uORB/uORB.h>

namespace uORB
{

/**
 * Base publication multi wrapper class
 */
template<typename T>
class PublicationMulti
{
public:

	/**
	 * Constructor
	 *
	 * @param meta The uORB metadata (usually from the ORB_ID() macro) for the topic.
	 * @param priority The priority for multi pub/sub, 0 means don't publish as multi
	 */
	PublicationMulti(const orb_metadata *meta, uint8_t priority = ORB_PRIO_DEFAULT) :
		_meta(meta),
		_priority(priority)
	{}

	~PublicationMulti() { orb_unadvertise(_handle); }

	/**
	 * Publish the struct
	 * @param data The uORB message struct we are updating.
	 */
	bool publish(const T &data)
	{
		if (_handle != nullptr) {
			return (orb_publish(_meta, _handle, &data) == PX4_OK);

		} else {
			int instance = 0;
			orb_advert_t handle = orb_advertise_multi(_meta, &data, &instance, _priority);

			if (handle != nullptr) {
				_handle = handle;
				return true;
			}
		}

		return false;
	}

protected:
	const orb_metadata *_meta;

	orb_advert_t _handle{nullptr};

	const uint8_t _priority;
};

/**
 * The publication multi class with data embedded.
 */
template<typename T>
class PublicationMultiData : public PublicationMulti<T>
{
public:
	/**
	 * Constructor
	 *
	 * @param meta The uORB metadata (usually from the ORB_ID() macro) for the topic.
	 * @param priority The priority for multi pub
	 */
	PublicationMultiData(const orb_metadata *meta, uint8_t priority = ORB_PRIO_DEFAULT) :
		PublicationMulti<T>(meta, priority)
	{}

	~PublicationMultiData() = default;

	T	&get() { return _data; }
	void	set(const T &data) { _data = data; }

	// Publishes the embedded struct.
	bool	update() { return PublicationMulti<T>::publish(_data); }
	bool	update(const T &data)
	{
		_data = data;
		return PublicationMulti<T>::publish(_data);
	}

private:
	T _data{};
};

} // namespace uORB
