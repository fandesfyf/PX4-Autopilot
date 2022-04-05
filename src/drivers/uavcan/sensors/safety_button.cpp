/****************************************************************************
 *
 *   Copyright (c) 2022 PX4 Development Team. All rights reserved.
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

#include <drivers/drv_hrt.h>
#include <parameters/param.h>
#include "safety_button.hpp"
#include <math.h>

const char *const UavcanSafetyButtonBridge::NAME = "safety_button";

UavcanSafetyButtonBridge::UavcanSafetyButtonBridge(uavcan::INode &node) :
	UavcanSensorBridgeBase("uavcan_safety_button",
			       ORB_ID(safety)), // TODO: either multiple publishers or `button_event` uORB topic
	_sub_button(node)
{ }

int UavcanSafetyButtonBridge::init()
{
	int res = _sub_button.start(ButtonCbBinder(this, &UavcanSafetyButtonBridge::button_sub_cb));

	if (res < 0) {
		DEVICE_LOG("failed to start uavcan sub: %d", res);
		return res;
	}

	return 0;
}

void UavcanSafetyButtonBridge::button_sub_cb(const
		uavcan::ReceivedDataStructure<ardupilot::indication::Button> &msg)
{
	bool is_safety = msg.button == ardupilot::indication::Button::BUTTON_SAFETY;
	bool pressed = msg.press_time >= 10; // 0.1s increments

	if (is_safety && pressed) {
		safety_s safety = {};
		safety.timestamp = hrt_absolute_time();
		safety.safety_switch_available = true;
		safety.safety_off = true;
		publish(msg.getSrcNodeID().get(), &safety);
	}
}

int UavcanSafetyButtonBridge::init_driver(uavcan_bridge::Channel *channel)
{
	return PX4_OK;
}
