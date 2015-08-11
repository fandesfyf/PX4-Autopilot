/****************************************************************************
 *
 *   Copyright (c) 2015 PX4 Development Team. All rights reserved.
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
 * @file airframe.cpp
 *
 * @author Roman Bapst 		<bapstroman@gmail.com>
 *
 */

#include "vtol_type.h"
#include "drivers/drv_pwm_output.h"
#include <nuttx/fs/ioctl.h>
#include "vtol_att_control_main.h"

VtolType::VtolType(VtolAttitudeControl *att_controller) :
_attc(att_controller),
_vtol_mode(ROTARY_WING)
{
	_v_att = _attc->get_att();
	_v_att_sp = _attc->get_att_sp();
	_v_rates_sp = _attc->get_rates_sp();
	_mc_virtual_v_rates_sp = _attc->get_mc_virtual_rates_sp();
	_fw_virtual_v_rates_sp = _attc->get_fw_virtual_rates_sp();
	_manual_control_sp = _attc->get_manual_control_sp();
	_v_control_mode = _attc->get_control_mode();
	_vtol_vehicle_status = _attc->get_vehicle_status();
	_actuators_out_0 = _attc->get_actuators_out0();
	_actuators_out_1 = _attc->get_actuators_out1();
	_actuators_mc_in = _attc->get_actuators_mc_in();
	_actuators_fw_in = _attc->get_actuators_fw_in();
	_armed = _attc->get_armed();
	_local_pos = _attc->get_local_pos();
	_airspeed = _attc->get_airspeed();
	_batt_status = _attc->get_batt_status();
	_vehicle_transition_cmd = _attc->get_vehicle_transition_cmd();
	_params = _attc->get_params();

	flag_idle_mc = true;
}

VtolType::~VtolType()
{
	
}

/**
* Adjust idle speed for mc mode.
*/
void VtolType::set_idle_mc()
{
	int ret;
	unsigned servo_count;
	char *dev = PWM_OUTPUT0_DEVICE_PATH;
	int fd = open(dev, 0);

	if (fd < 0) {err(1, "can't open %s", dev);}

	ret = ioctl(fd, PWM_SERVO_GET_COUNT, (unsigned long)&servo_count);
	unsigned pwm_value = _params->idle_pwm_mc;
	struct pwm_output_values pwm_values;
	memset(&pwm_values, 0, sizeof(pwm_values));

	for (int i = 0; i < _params->vtol_motor_count; i++) {
		pwm_values.values[i] = pwm_value;
		pwm_values.channel_count++;
	}

	ret = ioctl(fd, PWM_SERVO_SET_MIN_PWM, (long unsigned int)&pwm_values);

	if (ret != OK) {errx(ret, "failed setting min values");}

	close(fd);

	flag_idle_mc = true;
}

/**
* Adjust idle speed for fw mode.
*/
void VtolType::set_idle_fw()
{
	int ret;
	char *dev = PWM_OUTPUT0_DEVICE_PATH;
	int fd = open(dev, 0);

	if (fd < 0) {err(1, "can't open %s", dev);}

	unsigned pwm_value = PWM_LOWEST_MIN;
	struct pwm_output_values pwm_values;
	memset(&pwm_values, 0, sizeof(pwm_values));

	for (int i = 0; i < _params->vtol_motor_count; i++) {

		pwm_values.values[i] = pwm_value;
		pwm_values.channel_count++;
	}

	ret = ioctl(fd, PWM_SERVO_SET_MIN_PWM, (long unsigned int)&pwm_values);

	if (ret != OK) {errx(ret, "failed setting min values");}

	close(fd);
}

/*
 * Returns true if fixed-wing mode is requested.
 * Changed either via switch or via command.
 */
bool VtolType::is_fixed_wing_requested()
{
	bool to_fw = _manual_control_sp->aux1 > 0.0f;
 	if (_v_control_mode->flag_control_offboard_enabled) {
 		to_fw = fabsf(_vehicle_transition_cmd->param1 - vehicle_status_s::VEHICLE_VTOL_STATE_FW) < FLT_EPSILON;
 	}
 	return to_fw;
}
