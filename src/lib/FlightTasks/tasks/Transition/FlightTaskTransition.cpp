/****************************************************************************
 *
 *   Copyright (c) 2018 PX4 Development Team. All rights reserved.
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
 * @file FlightTaskTranstion.cpp
 */

#include "FlightTaskTransition.hpp"

bool FlightTaskTransition::updateInitialize()
{
	return FlightTask::updateInitialize();
}

bool FlightTaskTransition::activate(vehicle_local_position_setpoint_s state_prev)
{
	checkSetpoints(state_prev);
	_transition_altitude = state_prev.z;
	_transition_yaw = state_prev.yaw;
	_acceleration_setpoint.setAll(0.f);
	_velocity_prev = _velocity;
	return FlightTask::activate(state_prev);
}

void FlightTaskTransition::updateAccelerationEstimate()
{
	// Estimate the acceleration by filtering the raw derivative of the velocity estimate
	// This is done to provide a good estimate of the current acceleration to the next flight task after back-transition
	_acceleration_setpoint = 0.9f * _acceleration_setpoint + 0.1f * (_velocity - _velocity_prev) / _deltatime;

	if (!PX4_ISFINITE(_acceleration_setpoint(0)) ||
	    !PX4_ISFINITE(_acceleration_setpoint(1)) ||
	    !PX4_ISFINITE(_acceleration_setpoint(2))) {
		_acceleration_setpoint.setAll(0.f);
	}

	_velocity_prev = _velocity;
}

bool FlightTaskTransition::update()
{
	// level wings during the transition, altitude should be controlled
	_thrust_setpoint(0) = _thrust_setpoint(1) = 0.0f;
	_thrust_setpoint(2) = NAN;
	_position_setpoint *= NAN;
	_velocity_setpoint *= NAN;
	_position_setpoint(2) = _transition_altitude;

	updateAccelerationEstimate();

	_yaw_setpoint = _transition_yaw;
	return true;
}
