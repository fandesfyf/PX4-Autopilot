/****************************************************************************
 *
 *   Copyright (c) 2012-2015 PX4 Development Team. All rights reserved.
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
 * @file Subscription.cpp
 *
 */

#include "Subscription.hpp"
#include "topics/actuator_armed.h"
#include "topics/actuator_controls.h"
#include "topics/att_pos_mocap.h"
#include "topics/battery_status.h"
#include "topics/control_state.h"
#include "topics/distance_sensor.h"
#include "topics/hil_sensor.h"
#include "topics/home_position.h"
#include "topics/log_message.h"
#include "topics/manual_control_setpoint.h"
#include "topics/mavlink_log.h"
#include "topics/optical_flow.h"
#include "topics/parameter_update.h"
#include "topics/position_setpoint_triplet.h"
#include "topics/rc_channels.h"
#include "topics/satellite_info.h"
#include "topics/sensor_combined.h"
#include "topics/vehicle_attitude.h"
#include "topics/vehicle_attitude_setpoint.h"
#include "topics/vehicle_control_mode.h"
#include "topics/vehicle_global_position.h"
#include "topics/vehicle_gps_position.h"
#include "topics/vehicle_land_detected.h"
#include "topics/vehicle_local_position.h"
#include "topics/vehicle_local_position_setpoint.h"
#include "topics/vehicle_rates_setpoint.h"
#include "topics/vehicle_status.h"

#include <px4_defines.h>

namespace uORB
{

SubscriptionBase::SubscriptionBase(const struct orb_metadata *meta,
				   unsigned interval, unsigned instance) :
	_meta(meta),
	_instance(instance),
	_handle()
{
	if (_instance > 0) {
		_handle =  orb_subscribe_multi(
				   getMeta(), instance);

	} else {
		_handle =  orb_subscribe(getMeta());
	}

	if (_handle < 0) { PX4_ERR("sub failed"); }

	if (interval > 0) {
		orb_set_interval(getHandle(), interval);
	}
}

bool SubscriptionBase::updated()
{
	bool isUpdated = false;
	int ret = orb_check(_handle, &isUpdated);

	if (ret != PX4_OK) { PX4_ERR("orb check failed"); }

	return isUpdated;
}

void SubscriptionBase::update(void *data)
{
	if (updated()) {
		int ret = orb_copy(_meta, _handle, data);

		if (ret != PX4_OK) { PX4_ERR("orb copy failed"); }
	}
}

SubscriptionBase::~SubscriptionBase()
{
	int ret = orb_unsubscribe(_handle);

	if (ret != PX4_OK) { PX4_ERR("orb unsubscribe failed"); }
}

} // namespace uORB
