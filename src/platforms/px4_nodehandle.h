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
 * @file px4_nodehandle.h
 *
 * PX4 Middleware Wrapper Node Handle
 */
#pragma once

/* includes for all platforms */
#include <px4_subscriber.h>
#include <px4_publisher.h>

#if defined(__linux) || (defined(__APPLE__) && defined(__MACH__))
/* includes when building for ros */
#include "ros/ros.h"
#include <list>
#include <inttypes.h>
#else
/* includes when building for NuttX */
#include <containers/List.hpp>
#endif

namespace px4
{
#if defined(__linux) || (defined(__APPLE__) && defined(__MACH__))
class NodeHandle :
	private ros::NodeHandle
{
public:
	NodeHandle() :
		ros::NodeHandle(),
		_subs(),
		_pubs()
	{}

	~NodeHandle() {
		//XXX empty lists
	};

	template<typename M>
	Subscriber subscribe(const char *topic, void(*fp)(M)) {
		ros::Subscriber ros_sub = ros::NodeHandle::subscribe(topic, kQueueSizeDefault, fp);
		Subscriber sub(ros_sub);
		_subs.push_back(sub);
		return sub;
	}

	template<typename M>
	Publisher advertise(const char *topic) {
		ros::Publisher ros_pub = ros::NodeHandle::advertise<M>(topic, kQueueSizeDefault);
		Publisher pub(ros_pub);
		_pubs.push_back(pub);
		return pub;
	}
private:
	static const uint32_t kQueueSizeDefault = 1000;
	std::list<Subscriber> _subs;
	std::list<Publisher> _pubs;
};
#else
class NodeHandle
{
public:
	NodeHandle() :
		_subs(),
		_pubs()
	{}

	~NodeHandle() {};

	template<typename M>
	Subscriber subscribe(const char *topic, void(*fp)(M)) {
		Subscriber sub(&_subs, , interval);
		return sub;
	}

	template<typename M>
	Publisher advertise(const char *topic) {
		Publisher pub(ros_pub);
		_pubs.push_back(pub);
		return pub;
	}
private:
	List<Subscriber> _subs;
	List<Publisher> _pubs;

};
#endif
}
