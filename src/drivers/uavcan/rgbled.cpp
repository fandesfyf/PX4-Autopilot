/****************************************************************************
 *
 *   Copyright (C) 2020 PX4 Development Team. All rights reserved.
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

#include "rgbled.hpp"

UavcanRGBController::UavcanRGBController(uavcan::INode &node) :
	_node(node),
	_uavcan_pub_lights_cmd(node),
	_timer(node)
{
	_uavcan_pub_lights_cmd.setPriority(uavcan::TransferPriority::Lowest);
}

int UavcanRGBController::init()
{
	// Setup timer and call back function for periodic updates
	_timer.setCallback(TimerCbBinder(this, &UavcanRGBController::periodic_update));
	_timer.startPeriodic(uavcan::MonotonicDuration::fromMSec(1000 / MAX_RATE_HZ));
	return 0;
}

void UavcanRGBController::periodic_update(const uavcan::TimerEvent &)
{
	LedControlData led_control_data;

	if (_led_controller.update(led_control_data) == 1) {
		// RGB color in the standard 5-6-5 16-bit palette.
		// Monocolor lights should interpret this as brightness setpoint: from zero (0, 0, 0) to full brightness (31, 63, 31).
		uavcan::equipment::indication::SingleLightCommand cmd;

		uint8_t brightness = led_control_data.leds[0].brightness;

		switch (led_control_data.leds[0].color) {
		case led_control_s::COLOR_RED:
			cmd.color.red = brightness >> 3;
			cmd.color.green = 0;
			cmd.color.blue = 0;
			break;

		case led_control_s::COLOR_GREEN:
			cmd.color.red = 0;
			cmd.color.green = brightness >> 2;
			cmd.color.blue = 0;
			break;

		case led_control_s::COLOR_BLUE:
			cmd.color.red = 0;
			cmd.color.green = 0;
			cmd.color.blue = brightness >> 3;
			break;

		case led_control_s::COLOR_AMBER: // make it the same as yellow

		// FALLTHROUGH
		case led_control_s::COLOR_YELLOW:
			cmd.color.red = brightness >> 3;
			cmd.color.green = brightness >> 2;
			cmd.color.blue = 0;
			break;

		case led_control_s::COLOR_PURPLE:
			cmd.color.red = brightness >> 3;
			cmd.color.green = 0;
			cmd.color.blue = brightness >> 3;
			break;

		case led_control_s::COLOR_CYAN:
			cmd.color.red = 0;
			cmd.color.green = brightness >> 2;
			cmd.color.blue = brightness >> 3;
			break;

		case led_control_s::COLOR_WHITE:
			cmd.color.red = brightness >> 3;
			cmd.color.green = brightness >> 2;
			cmd.color.blue = brightness >> 3;
			break;

		default: // led_control_s::COLOR_OFF
			cmd.color.red = 0;
			cmd.color.green = 0;
			cmd.color.blue = 0;
			break;
		}

		uavcan::equipment::indication::LightsCommand cmds;
		cmds.commands.push_back(cmd);

		_uavcan_pub_lights_cmd.broadcast(cmds);
	}
}
