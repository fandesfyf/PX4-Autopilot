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
 * @file FlightAutoLine.cpp
 */

#include "FlightTaskAutoLineSmoothVel.hpp"
#include <mathlib/mathlib.h>
#include <float.h>

using namespace matrix;

bool FlightTaskAutoLineSmoothVel::activate()
{
	bool ret = FlightTaskAutoMapper2::activate();

	for (int i = 0; i < 3; ++i) {
		_trajectory[i].reset(0.f, _velocity(i), _position(i));
	}

	_yaw_sp_prev = _yaw;
	_updateTrajConstraints();

	return ret;
}

void FlightTaskAutoLineSmoothVel::reActivate()
{
	// Don't reset during takeoff TODO: Find a proper solution
	// The issue here is that with a small increment of velocity setpoint (generated by this flight task), the
	// land detector doesn't detect takeoff and without takeoff detection, the
	// flight task is always reset.
}

void FlightTaskAutoLineSmoothVel::_setDefaultConstraints()
{
	FlightTaskAuto::_setDefaultConstraints();

	_constraints.speed_xy = MPC_XY_VEL_MAX.get(); // TODO : Should be computed using heading
}

void FlightTaskAutoLineSmoothVel::_generateSetpoints()
{
	_prepareSetpoints();
	_generateTrajectory();

	if (!PX4_ISFINITE(_yaw_setpoint)) {
		// no valid heading -> generate heading in this flight task
		_generateHeading();
	}
}

void FlightTaskAutoLineSmoothVel::_generateHeading()
{
	// Generate heading along trajectory if possible, otherwise hold the previous yaw setpoint
	if (!_generateHeadingAlongTraj()) {
		_yaw_setpoint = _yaw_sp_prev;
	}

	_yaw_sp_prev = _yaw_setpoint;
}

bool FlightTaskAutoLineSmoothVel::_generateHeadingAlongTraj()
{
	bool res = false;
	Vector2f vel_sp_xy(_velocity_setpoint);

	if (vel_sp_xy.length() > .01f) {
		// Generate heading from velocity vector, only if it is long enough
		_compute_heading_from_2D_vector(_yaw_setpoint, vel_sp_xy);
		res = true;
	}

	return res;
}

/* Constrain some value vith a constrain depending on the sign of the constrain
 * Example: 	- if the constrain is -5, the value will be constrained between -5 and 0
 * 		- if the constrain is 5, the value will be constrained between 0 and 5
 */
inline float FlightTaskAutoLineSmoothVel::constrain_one_side(float val, float constrain)
{
	const float min = (constrain < FLT_EPSILON) ? constrain : 0.f;
	const float max = (constrain > FLT_EPSILON) ? constrain : 0.f;

	return math::constrain(val, min, max);
}

void FlightTaskAutoLineSmoothVel::_prepareSetpoints()
{
	// Interface: A valid position setpoint generates a velocity target using a P controller. If a velocity is specified
	// that one is used as a velocity limit.
	// If the position setpoints are set to NAN, the values in the velocity setpoints are used as velocity targets: nothing to do here.

	if (PX4_ISFINITE(_position_setpoint(0)) &&
	    PX4_ISFINITE(_position_setpoint(1))) {
		// Use position setpoints to generate velocity setpoints

		// Get various path specific vectors. */
		Vector2f pos_traj;
		pos_traj(0) = _trajectory[0].getCurrentPosition();
		pos_traj(1) = _trajectory[1].getCurrentPosition();
		Vector2f pos_sp_xy(_position_setpoint);
		Vector2f pos_traj_to_dest(pos_sp_xy - pos_traj);
		Vector2f u_prev_to_dest = Vector2f(pos_sp_xy - Vector2f(_prev_wp)).unit_or_zero();
		Vector2f prev_to_pos(pos_traj - Vector2f(_prev_wp));
		Vector2f closest_pt = Vector2f(_prev_wp) + u_prev_to_dest * (prev_to_pos * u_prev_to_dest);
		Vector2f u_pos_traj_to_dest_xy(Vector2f(pos_traj_to_dest).unit_or_zero());

		float speed_sp_track = Vector2f(pos_traj_to_dest).length() * MPC_XY_TRAJ_P.get();
		speed_sp_track = math::constrain(speed_sp_track, 0.0f, MPC_XY_CRUISE.get());
		Vector2f vel_sp_xy = u_pos_traj_to_dest_xy * speed_sp_track;

		for (int i = 0; i < 2; i++) {
			// If available, constrain the velocity using _velocity_setpoint(.)
			if (PX4_ISFINITE(_velocity_setpoint(i))) {
				_velocity_setpoint(i) = constrain_one_side(vel_sp_xy(i), _velocity_setpoint(i));

			} else {
				_velocity_setpoint(i) = vel_sp_xy(i);
			}

			_velocity_setpoint(i) += (closest_pt(i) - _trajectory[i].getCurrentPosition()) *
						 MPC_XY_TRAJ_P.get();  // Along-track setpoint + cross-track P controller
		}

	} else if (!PX4_ISFINITE(_velocity_setpoint(0)) &&
		   !PX4_ISFINITE(_velocity_setpoint(1))) {
		// No position nor velocity setpoints available, set the velocity targer to zero

		_velocity_setpoint(0) = 0.f;
		_velocity_setpoint(1) = 0.f;
	}

	if (PX4_ISFINITE(_position_setpoint(2))) {
		const float vel_sp_z = (_position_setpoint(2) - _trajectory[2].getCurrentPosition()) *
				       MPC_Z_TRAJ_P.get(); // Generate a velocity target for the trajectory using a simple P loop

		// If available, constrain the velocity using _velocity_setpoint(.)
		if (PX4_ISFINITE(_velocity_setpoint(2))) {
			_velocity_setpoint(2) = constrain_one_side(vel_sp_z, _velocity_setpoint(2));

		} else {
			_velocity_setpoint(2) = vel_sp_z;
		}

	} else if (!PX4_ISFINITE(_velocity_setpoint(2))) {
		// No position nor velocity setpoints available, set the velocity targer to zero
		_velocity_setpoint(2) = 0.f;
	}
}

void FlightTaskAutoLineSmoothVel::_updateTrajConstraints()
{
	// Update the constraints of the trajectories
	_trajectory[0].setMaxAccel(MPC_ACC_HOR_MAX.get()); // TODO : Should be computed using heading
	_trajectory[1].setMaxAccel(MPC_ACC_HOR_MAX.get());
	_trajectory[0].setMaxVel(_constraints.speed_xy);
	_trajectory[1].setMaxVel(_constraints.speed_xy);
	_trajectory[0].setMaxJerk(MPC_JERK_MIN.get()); // TODO : Should be computed using heading
	_trajectory[1].setMaxJerk(MPC_JERK_MIN.get());
	_trajectory[2].setMaxJerk(MPC_JERK_MIN.get());

	if (_velocity_setpoint(2) < 0.f) { // up
		_trajectory[2].setMaxAccel(MPC_ACC_UP_MAX.get());
		_trajectory[2].setMaxVel(MPC_Z_VEL_MAX_UP.get());

	} else { // down
		_trajectory[2].setMaxAccel(MPC_ACC_DOWN_MAX.get());
		_trajectory[2].setMaxVel(MPC_Z_VEL_MAX_DN.get());
	}
}

void FlightTaskAutoLineSmoothVel::_generateTrajectory()
{
	/* Slow down the trajectory by decreasing the integration time based on the position error.
	 * This is only performed when the drone is behind the trajectory
	 */
	Vector2f position_trajectory_xy(_trajectory[0].getCurrentPosition(), _trajectory[1].getCurrentPosition());
	Vector2f position_xy(_position);
	Vector2f vel_traj_xy(_trajectory[0].getCurrentVelocity(), _trajectory[1].getCurrentVelocity());
	Vector2f drone_to_trajectory_xy(position_trajectory_xy - position_xy);
	float position_error = drone_to_trajectory_xy.length();

	float time_stretch = 1.f - math::constrain(position_error * 0.5f, 0.f, 1.f);

	// Don't stretch time if the drone is ahead of the position setpoint
	if (drone_to_trajectory_xy.dot(vel_traj_xy) < 0.f) {
		time_stretch = 1.f;
	}

	Vector3f jerk_sp_smooth;
	Vector3f accel_sp_smooth;
	Vector3f vel_sp_smooth;
	Vector3f pos_sp_smooth;

	for (int i = 0; i < 3; ++i) {
		_trajectory[i].integrate(_deltatime, time_stretch, accel_sp_smooth(i), vel_sp_smooth(i), pos_sp_smooth(i));
		jerk_sp_smooth(i) = _trajectory[i].getCurrentJerk();
	}

	_updateTrajConstraints();

	for (int i = 0; i < 3; ++i) {
		_trajectory[i].updateDurations(_deltatime, _velocity_setpoint(i));
	}

	VelocitySmoothing::timeSynchronization(_trajectory, 2); // Synchronize x and y only

	_jerk_setpoint = jerk_sp_smooth;
	_acceleration_setpoint = accel_sp_smooth;
	_velocity_setpoint = vel_sp_smooth;
	_position_setpoint = pos_sp_smooth;
}
