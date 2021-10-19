/****************************************************************************
 *
 *   Copyright (c) 2021 PX4 Development Team. All rights reserved.
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

#include "ManualControl.hpp"

#include <drivers/drv_hrt.h>
#include <commander/px4_custom_mode.h>
#include <uORB/topics/vehicle_command.h>
#include <uORB/topics/landing_gear.h>
#include <uORB/topics/vtol_vehicle_status.h>
#include <uORB/topics/commander_state.h>

namespace manual_control
{

ManualControl::ManualControl() :
	ModuleParams(nullptr),
	ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::hp_default)
{
}

ManualControl::~ManualControl()
{
	perf_free(_loop_perf);
	perf_free(_loop_interval_perf);
}

bool ManualControl::init()
{
	ScheduleNow();
	return true;
}

void ManualControl::Run()
{
	if (should_exit()) {
		ScheduleClear();
		exit_and_cleanup();
		return;
	}

	perf_begin(_loop_perf);
	perf_count(_loop_interval_perf);

	// Check if parameters have changed
	if (_parameter_update_sub.updated()) {
		// clear update
		parameter_update_s param_update;
		_parameter_update_sub.copy(&param_update);

		updateParams();

		_stick_arm_hysteresis.set_hysteresis_time_from(false, _param_rc_arm_hyst.get() * 1_ms);
		_stick_disarm_hysteresis.set_hysteresis_time_from(false, _param_rc_arm_hyst.get() * 1_ms);
		_button_hysteresis.set_hysteresis_time_from(false, _param_rc_arm_hyst.get() * 1_ms);

		_selector.setRcInMode(_param_com_rc_in_mode.get());
		_selector.setTimeout(_param_com_rc_loss_t.get() * 1_s);
	}

	const hrt_abstime now = hrt_absolute_time();
	_selector.updateValidityOfChosenInput(now);

	for (int i = 0; i < MAX_MANUAL_INPUT_COUNT; i++) {
		manual_control_input_s manual_control_input;

		if (_manual_control_input_subs[i].update(&manual_control_input)) {
			_selector.updateWithNewInputSample(now, manual_control_input, i);
		}
	}

	bool switches_updated = false;
	manual_control_switches_s switches;

	if (_manual_control_switches_sub.update(&switches)) {
		switches_updated = true;
	}

	if (_selector.setpoint().valid) {
		_published_invalid_once = false;

		// Arm gesture
		const bool right_stick_centered = (fabsf(_selector.setpoint().chosen_input.x) < 0.1f)
						  && (fabsf(_selector.setpoint().chosen_input.y) < 0.1f);
		const bool stick_lower_right = (_selector.setpoint().chosen_input.z < 0.1f)
					       && (_selector.setpoint().chosen_input.r > 0.9f);

		const bool previous_stick_arm_hysteresis = _stick_arm_hysteresis.get_state();
		_stick_arm_hysteresis.set_state_and_update(stick_lower_right && right_stick_centered, _selector.setpoint().timestamp);

		if (!previous_stick_arm_hysteresis && _stick_arm_hysteresis.get_state()) {
			sendActionRequest(action_request_s::ACTION_ARM, action_request_s::SOURCE_RC_STICK_GESTURE);
		}

		// Disarm gesture
		const bool stick_lower_left = (_selector.setpoint().chosen_input.z < 0.1f)
					      && (_selector.setpoint().chosen_input.r < -0.9f);

		const bool previous_stick_disarm_hysteresis = _stick_disarm_hysteresis.get_state();
		_stick_disarm_hysteresis.set_state_and_update(stick_lower_left && right_stick_centered, _selector.setpoint().timestamp);

		if (!previous_stick_disarm_hysteresis && _stick_disarm_hysteresis.get_state()) {
			sendActionRequest(action_request_s::ACTION_DISARM, action_request_s::SOURCE_RC_STICK_GESTURE);
		}

		// User override by stick
		const float dt_s = (now - _last_time) / 1e6f;
		_x_diff.update(_selector.setpoint().chosen_input.x, dt_s);
		_y_diff.update(_selector.setpoint().chosen_input.y, dt_s);
		_z_diff.update(_selector.setpoint().chosen_input.z, dt_s);
		_r_diff.update(_selector.setpoint().chosen_input.r, dt_s);

		const float minimum_stick_change = 0.01f * _param_com_rc_stick_ov.get();

		const bool rpy_moved = (fabsf(_x_diff.diff()) > minimum_stick_change)
				       || (fabsf(_y_diff.diff()) > minimum_stick_change)
				       || (fabsf(_r_diff.diff()) > minimum_stick_change);

		// Throttle change value doubled to achieve the same scaling even though the range is [0,1] instead of [-1,1]
		const bool throttle_moved = (fabsf(_z_diff.diff()) * 2.f) > minimum_stick_change;

		_selector.setpoint().user_override = rpy_moved || throttle_moved;

		if (switches_updated) {
			// Only use switches if current source is RC as well.
			if (_selector.setpoint().chosen_input.data_source == manual_control_input_s::SOURCE_RC) {
				if (_previous_switches_initialized) {
					if (switches.mode_slot != _previous_switches.mode_slot) {
						evaluateModeSlot(switches.mode_slot);
					}

					if (_param_com_arm_swisbtn.get()) {
						// Arming button
						const bool previous_button_hysteresis = _button_hysteresis.get_state();
						_button_hysteresis.set_state_and_update(switches.arm_switch == manual_control_switches_s::SWITCH_POS_ON, now);

						if (!previous_button_hysteresis && _button_hysteresis.get_state()) {
							sendActionRequest(action_request_s::ACTION_TOGGLE_ARMING, action_request_s::SOURCE_RC_BUTTON);
						}

					} else {
						// Arming switch
						if (switches.arm_switch != _previous_switches.arm_switch) {
							if (switches.arm_switch == manual_control_switches_s::SWITCH_POS_ON) {
								sendActionRequest(action_request_s::ACTION_ARM, action_request_s::SOURCE_RC_SWITCH);

							} else if (switches.arm_switch == manual_control_switches_s::SWITCH_POS_OFF) {
								sendActionRequest(action_request_s::ACTION_DISARM, action_request_s::SOURCE_RC_SWITCH);
							}
						}
					}

					if (switches.return_switch != _previous_switches.return_switch) {
						if (switches.return_switch == manual_control_switches_s::SWITCH_POS_ON) {
							sendActionRequest(action_request_s::ACTION_SWITCH_MODE, action_request_s::SOURCE_RC_MODE_SLOT,
									  commander_state_s::MAIN_STATE_AUTO_RTL);

						} else if (switches.return_switch == manual_control_switches_s::SWITCH_POS_OFF) {
							evaluateModeSlot(switches.mode_slot);
						}
					}

					if (switches.loiter_switch != _previous_switches.loiter_switch) {
						if (switches.loiter_switch == manual_control_switches_s::SWITCH_POS_ON) {
							sendActionRequest(action_request_s::ACTION_SWITCH_MODE, action_request_s::SOURCE_RC_MODE_SLOT,
									  commander_state_s::MAIN_STATE_AUTO_LOITER);

						} else if (switches.loiter_switch == manual_control_switches_s::SWITCH_POS_OFF) {
							evaluateModeSlot(switches.mode_slot);
						}
					}

					if (switches.offboard_switch != _previous_switches.offboard_switch) {
						if (switches.offboard_switch == manual_control_switches_s::SWITCH_POS_ON) {
							sendActionRequest(action_request_s::ACTION_SWITCH_MODE, action_request_s::SOURCE_RC_MODE_SLOT,
									  commander_state_s::MAIN_STATE_OFFBOARD);

						} else if (switches.offboard_switch == manual_control_switches_s::SWITCH_POS_OFF) {
							evaluateModeSlot(switches.mode_slot);
						}
					}

					if (switches.kill_switch != _previous_switches.kill_switch) {
						if (switches.kill_switch == manual_control_switches_s::SWITCH_POS_ON) {
							sendActionRequest(action_request_s::ACTION_KILL, action_request_s::SOURCE_RC_SWITCH);

						} else if (switches.kill_switch == manual_control_switches_s::SWITCH_POS_OFF) {
							sendActionRequest(action_request_s::ACTION_UNKILL, action_request_s::SOURCE_RC_SWITCH);
						}
					}

					if (switches.gear_switch != _previous_switches.gear_switch
					    && _previous_switches.gear_switch != manual_control_switches_s::SWITCH_POS_NONE) {

						if (switches.gear_switch == manual_control_switches_s::SWITCH_POS_ON) {
							publish_landing_gear(landing_gear_s::GEAR_UP);

						} else if (switches.gear_switch == manual_control_switches_s::SWITCH_POS_OFF) {
							publish_landing_gear(landing_gear_s::GEAR_DOWN);
						}
					}

					if (switches.transition_switch != _previous_switches.transition_switch) {
						if (switches.transition_switch == manual_control_switches_s::SWITCH_POS_ON) {
							sendActionRequest(action_request_s::ACTION_VTOL_TRANSITION_TO_FIXEDWING, action_request_s::SOURCE_RC_SWITCH);

						} else if (switches.transition_switch == manual_control_switches_s::SWITCH_POS_OFF) {
							sendActionRequest(action_request_s::ACTION_VTOL_TRANSITION_TO_MULTICOPTER, action_request_s::SOURCE_RC_SWITCH);
						}
					}

				} else {
					// Send an initial request to switch to the mode requested by RC
					evaluateModeSlot(switches.mode_slot);
				}

				_previous_switches_initialized = true;
				_previous_switches = switches;

			} else {
				_previous_switches_initialized = false;
			}
		}

		_selector.setpoint().timestamp = now;
		_manual_control_setpoint_pub.publish(_selector.setpoint());

		// If it's valid, this should really be valid but better safe than sorry.
		const int instance = _selector.instance();

		if (instance >= 0 && instance < MAX_MANUAL_INPUT_COUNT) {
			_manual_control_input_subs[instance].registerCallback();
		}

		_manual_control_switches_sub.registerCallback();

	} else {
		if (!_published_invalid_once) {
			_published_invalid_once = true;
			_manual_control_setpoint_pub.publish(_selector.setpoint());
		}

		_x_diff.reset();
		_y_diff.reset();
		_z_diff.reset();
		_r_diff.reset();
		_stick_arm_hysteresis.set_state_and_update(false, now);
		_stick_disarm_hysteresis.set_state_and_update(false, now);
		_button_hysteresis.set_state_and_update(false, now);
	}

	_last_time = now;

	// reschedule timeout
	ScheduleDelayed(200_ms);

	perf_end(_loop_perf);
}

void ManualControl::evaluateModeSlot(uint8_t mode_slot)
{
	switch (mode_slot) {
	case manual_control_switches_s::MODE_SLOT_NONE:
		break;

	case manual_control_switches_s::MODE_SLOT_1:
		sendActionRequest(action_request_s::ACTION_SWITCH_MODE, action_request_s::SOURCE_RC_MODE_SLOT, _param_fltmode_1.get());
		break;

	case manual_control_switches_s::MODE_SLOT_2:
		sendActionRequest(action_request_s::ACTION_SWITCH_MODE, action_request_s::SOURCE_RC_MODE_SLOT, _param_fltmode_2.get());
		break;

	case manual_control_switches_s::MODE_SLOT_3:
		sendActionRequest(action_request_s::ACTION_SWITCH_MODE, action_request_s::SOURCE_RC_MODE_SLOT, _param_fltmode_3.get());
		break;

	case manual_control_switches_s::MODE_SLOT_4:
		sendActionRequest(action_request_s::ACTION_SWITCH_MODE, action_request_s::SOURCE_RC_MODE_SLOT, _param_fltmode_4.get());
		break;

	case manual_control_switches_s::MODE_SLOT_5:
		sendActionRequest(action_request_s::ACTION_SWITCH_MODE, action_request_s::SOURCE_RC_MODE_SLOT, _param_fltmode_5.get());
		break;

	case manual_control_switches_s::MODE_SLOT_6:
		sendActionRequest(action_request_s::ACTION_SWITCH_MODE, action_request_s::SOURCE_RC_MODE_SLOT, _param_fltmode_6.get());
		break;

	default:
		PX4_WARN("mode slot overflow");
		break;
	}
}

void ManualControl::sendActionRequest(int8_t action, int8_t source, int8_t mode)
{
	action_request_s action_request{};
	action_request.action = action;
	action_request.source = source;
	action_request.mode = mode;
	action_request.timestamp = hrt_absolute_time();
	_action_request_pub.publish(action_request);
}

void ManualControl::publish_landing_gear(int8_t action)
{
	landing_gear_s landing_gear{};
	landing_gear.landing_gear = action;
	landing_gear.timestamp = hrt_absolute_time();
	uORB::Publication<landing_gear_s> landing_gear_pub{ORB_ID(landing_gear)};
	landing_gear_pub.publish(landing_gear);
}

int ManualControl::task_spawn(int argc, char *argv[])
{
	ManualControl *instance = new ManualControl();

	if (instance) {
		_object.store(instance);
		_task_id = task_id_is_work_queue;

		if (instance->init()) {
			return PX4_OK;
		}

	} else {
		PX4_ERR("alloc failed");
	}

	delete instance;
	_object.store(nullptr);
	_task_id = -1;

	return PX4_ERROR;
}

int ManualControl::print_status()
{
	perf_print_counter(_loop_perf);
	perf_print_counter(_loop_interval_perf);
	return 0;
}

int ManualControl::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int ManualControl::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Module consuming manual_control_inputs publishing one manual_control_setpoint.

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("manual_control", "system");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

}; // namespace manual_control

extern "C" __EXPORT int manual_control_main(int argc, char *argv[])
{
	return manual_control::ManualControl::main(argc, argv);
}
