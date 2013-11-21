/****************************************************************************
 *
 *   Copyright (c) 2013 Estimation and Control Library (ECL). All rights reserved.
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
 * 3. Neither the name ECL nor the names of its contributors may be
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
 * @file ecl_l1_pos_controller.h
 * Implementation of L1 position control.
 * Authors and acknowledgements in header.
 *
 */

#include "ecl_l1_pos_controller.h"

float ECL_L1_Pos_Controller::nav_roll()
{
	float ret = atanf(_lateral_accel * 1.0f / CONSTANTS_ONE_G);
	ret = math::constrain(ret, -_roll_lim_rad, _roll_lim_rad);
	return ret;
}

float ECL_L1_Pos_Controller::nav_lateral_acceleration_demand()
{
	return _lateral_accel;
}

float ECL_L1_Pos_Controller::nav_bearing()
{
	return _wrap_pi(_nav_bearing);
}

float ECL_L1_Pos_Controller::bearing_error()
{
	return _bearing_error;
}

float ECL_L1_Pos_Controller::target_bearing()
{
	return _target_bearing;
}

float ECL_L1_Pos_Controller::switch_distance(float wp_radius)
{
	/* following [2], switching on L1 distance */
	return math::min(wp_radius, _L1_distance);
}

bool ECL_L1_Pos_Controller::reached_loiter_target(void)
{
	return _circle_mode;
}

float ECL_L1_Pos_Controller::crosstrack_error(void)
{
	return _crosstrack_error;
}

void ECL_L1_Pos_Controller::navigate_waypoints(const math::Vector2f &vector_A, const math::Vector2f &vector_B, const math::Vector2f &vector_curr_position,
				       const math::Vector2f &ground_speed_vector)
{

	/* this follows the logic presented in [1] */

	float eta;
	float xtrack_vel;
	float ltrack_vel;

	/* get the direction between the last (visited) and next waypoint */
	_target_bearing = get_bearing_to_next_waypoint(vector_curr_position.getX(), vector_curr_position.getY(), vector_B.getX(), vector_B.getY());

	/* enforce a minimum ground speed of 0.1 m/s to avoid singularities */
	float ground_speed = math::max(ground_speed_vector.length(), 0.1f);

	/* calculate the L1 length required for the desired period */
	_L1_distance = _L1_ratio * ground_speed;

	/* calculate vector from A to B */
	math::Vector2f vector_AB = get_local_planar_vector(vector_A, vector_B);

	/*
	 * check if waypoints are on top of each other. If yes,
	 * skip A and directly continue to B
	 */
	if (vector_AB.length() < 1.0e-6f) {
		vector_AB = get_local_planar_vector(vector_curr_position, vector_B);
	}

	vector_AB.normalize();

	/* calculate the vector from waypoint A to the aircraft */
	math::Vector2f vector_A_to_airplane = get_local_planar_vector(vector_A, vector_curr_position);

	/* calculate crosstrack error (output only) */
	_crosstrack_error = vector_AB % vector_A_to_airplane;

	/*
	 * If the current position is in a +-135 degree angle behind waypoint A
	 * and further away from A than the L1 distance, then A becomes the L1 point.
	 * If the aircraft is already between A and B normal L1 logic is applied.
	 */
	float distance_A_to_airplane = vector_A_to_airplane.length();
	float alongTrackDist = vector_A_to_airplane * vector_AB;

	/* estimate airplane position WRT to B */
	math::Vector2f vector_B_to_P_unit = get_local_planar_vector(vector_B, vector_curr_position).normalized();
	
	/* calculate angle of airplane position vector relative to line) */

	// XXX this could probably also be based solely on the dot product
	float AB_to_BP_bearing = atan2f(vector_B_to_P_unit % vector_AB, vector_B_to_P_unit * vector_AB);

	/* extension from [2], fly directly to A */
	if (distance_A_to_airplane > _L1_distance && alongTrackDist / math::max(distance_A_to_airplane , 1.0f) < -0.7071f) {

		/* calculate eta to fly to waypoint A */

		/* unit vector from waypoint A to current position */
		math::Vector2f vector_A_to_airplane_unit = vector_A_to_airplane.normalized();
		/* velocity across / orthogonal to line */
		xtrack_vel = ground_speed_vector % (-vector_A_to_airplane_unit);
		/* velocity along line */
		ltrack_vel = ground_speed_vector * (-vector_A_to_airplane_unit);
		eta = atan2f(xtrack_vel, ltrack_vel);
		/* bearing from current position to L1 point */
		_nav_bearing = atan2f(-vector_A_to_airplane_unit.getY() , -vector_A_to_airplane_unit.getX());

	/*
	 * If the AB vector and the vector from B to airplane point in the same
	 * direction, we have missed the waypoint. At +- 90 degrees we are just passing it.
	 */
	} else if (fabsf(AB_to_BP_bearing) < math::radians(100.0f)) {
		/*
		 * Extension, fly back to waypoint.
		 * 
		 * This corner case is possible if the system was following
		 * the AB line from waypoint A to waypoint B, then is
		 * switched to manual mode (or otherwise misses the waypoint)
		 * and behind the waypoint continues to follow the AB line.
		 */

		/* calculate eta to fly to waypoint B */
		
		/* velocity across / orthogonal to line */
		xtrack_vel = ground_speed_vector % (-vector_B_to_P_unit);
		/* velocity along line */
		ltrack_vel = ground_speed_vector * (-vector_B_to_P_unit);
		eta = atan2f(xtrack_vel, ltrack_vel);
		/* bearing from current position to L1 point */
		_nav_bearing = atan2f(-vector_B_to_P_unit.getY() , -vector_B_to_P_unit.getX());

	} else {

		/* calculate eta to fly along the line between A and B */

		/* velocity across / orthogonal to line */
		xtrack_vel = ground_speed_vector % vector_AB;
		/* velocity along line */
		ltrack_vel = ground_speed_vector * vector_AB;
		/* calculate eta2 (angle of velocity vector relative to line) */
		float eta2 = atan2f(xtrack_vel, ltrack_vel);
		/* calculate eta1 (angle to L1 point) */
		float xtrackErr = vector_A_to_airplane % vector_AB;
		float sine_eta1 = xtrackErr / math::max(_L1_distance , 0.1f);
		/* limit output to 45 degrees */
		sine_eta1 = math::constrain(sine_eta1, -0.7071f, 0.7071f); //sin(pi/4) = 0.7071
		float eta1 = asinf(sine_eta1);
		eta = eta1 + eta2;
		/* bearing from current position to L1 point */
		_nav_bearing = atan2f(vector_AB.getY(), vector_AB.getX()) + eta1;

	}

	/* limit angle to +-90 degrees */
	eta = math::constrain(eta, (-M_PI_F) / 2.0f, +M_PI_F / 2.0f);
	_lateral_accel = _K_L1 * ground_speed * ground_speed / _L1_distance * sinf(eta);

	/* flying to waypoints, not circling them */
	_circle_mode = false;

	/* the bearing angle, in NED frame */
	_bearing_error = eta;
}

void ECL_L1_Pos_Controller::navigate_loiter(const math::Vector2f &vector_A, const math::Vector2f &vector_curr_position, float radius, int8_t loiter_direction,
				       const math::Vector2f &ground_speed_vector)
{
	/* the complete guidance logic in this section was proposed by [2] */

	/* calculate the gains for the PD loop (circle tracking) */
	float omega = (2.0f * M_PI_F / _L1_period);
	float K_crosstrack = omega * omega;
	float K_velocity = 2.0f * _L1_damping * omega;

	/* update bearing to next waypoint */
	_target_bearing = get_bearing_to_next_waypoint(vector_curr_position.getX(), vector_curr_position.getY(), vector_A.getX(), vector_A.getY());

	/* ground speed, enforce minimum of 0.1 m/s to avoid singularities */
	float ground_speed = math::max(ground_speed_vector.length() , 0.1f);

	/* calculate the L1 length required for the desired period */
	_L1_distance = _L1_ratio * ground_speed;

	/* calculate the vector from waypoint A to current position */
	math::Vector2f vector_A_to_airplane = get_local_planar_vector(vector_A, vector_curr_position);

	/* store the normalized vector from waypoint A to current position */
	math::Vector2f vector_A_to_airplane_unit = (vector_A_to_airplane).normalized();

	/* calculate eta angle towards the loiter center */

	/* velocity across / orthogonal to line from waypoint to current position */
	float xtrack_vel_center = vector_A_to_airplane_unit % ground_speed_vector;
	/* velocity along line from waypoint to current position */
	float ltrack_vel_center = - (ground_speed_vector * vector_A_to_airplane_unit);
	float eta = atan2f(xtrack_vel_center, ltrack_vel_center);
	/* limit eta to 90 degrees */
	eta = math::constrain(eta, -M_PI_F / 2.0f, +M_PI_F / 2.0f);

	/* calculate the lateral acceleration to capture the center point */
	float lateral_accel_sp_center = _K_L1 * ground_speed * ground_speed / _L1_distance * sinf(eta);

	/* for PD control: Calculate radial position and velocity errors */

	/* radial velocity error */
	float xtrack_vel_circle = -ltrack_vel_center;
	/* radial distance from the loiter circle (not center) */
	float xtrack_err_circle = vector_A_to_airplane.length() - radius;

	/* cross track error for feedback */
	_crosstrack_error = xtrack_err_circle;

	/* calculate PD update to circle waypoint */
	float lateral_accel_sp_circle_pd = (xtrack_err_circle * K_crosstrack + xtrack_vel_circle * K_velocity);

	/* calculate velocity on circle / along tangent */
	float tangent_vel = xtrack_vel_center * loiter_direction;

	/* prevent PD output from turning the wrong way */
	if (tangent_vel < 0.0f) {
		lateral_accel_sp_circle_pd = math::max(lateral_accel_sp_circle_pd , 0.0f);
	}

	/* calculate centripetal acceleration setpoint */
	float lateral_accel_sp_circle_centripetal = tangent_vel * tangent_vel / math::max((0.5f * radius) , (radius + xtrack_err_circle));

	/* add PD control on circle and centripetal acceleration for total circle command */
	float lateral_accel_sp_circle = loiter_direction * (lateral_accel_sp_circle_pd + lateral_accel_sp_circle_centripetal);

	/*
	 * Switch between circle (loiter) and capture (towards waypoint center) mode when
	 * the commands switch over. Only fly towards waypoint if outside the circle.
	 */

	// XXX check switch over
	if ((lateral_accel_sp_center < lateral_accel_sp_circle && loiter_direction > 0 && xtrack_err_circle > 0.0f) ||
		(lateral_accel_sp_center > lateral_accel_sp_circle && loiter_direction < 0 && xtrack_err_circle > 0.0f)) {
		_lateral_accel = lateral_accel_sp_center;
		_circle_mode = false;
		/* angle between requested and current velocity vector */
		_bearing_error = eta;
		/* bearing from current position to L1 point */
		_nav_bearing = atan2f(-vector_A_to_airplane_unit.getY() , -vector_A_to_airplane_unit.getX());

	} else {
		_lateral_accel = lateral_accel_sp_circle;
		_circle_mode = true;
		_bearing_error = 0.0f;
		/* bearing from current position to L1 point */
		_nav_bearing = atan2f(-vector_A_to_airplane_unit.getY() , -vector_A_to_airplane_unit.getX());
	}
}


void ECL_L1_Pos_Controller::navigate_heading(float navigation_heading, float current_heading, const math::Vector2f &ground_speed_vector)
{
	/* the complete guidance logic in this section was proposed by [2] */

	float eta;

	/* 
	 * As the commanded heading is the only reference
	 * (and no crosstrack correction occurs),
	 * target and navigation bearing become the same
	 */
	_target_bearing = _nav_bearing = _wrap_pi(navigation_heading);
	eta = _target_bearing - _wrap_pi(current_heading);
	eta = _wrap_pi(eta);

	/* consequently the bearing error is exactly eta: */
	_bearing_error = eta; 

	/* ground speed is the length of the ground speed vector */
	float ground_speed = ground_speed_vector.length();

	/* adjust L1 distance to keep constant frequency */
	_L1_distance = ground_speed / _heading_omega;
	float omega_vel = ground_speed * _heading_omega;

	/* not circling a waypoint */
	_circle_mode = false;

	/* navigating heading means by definition no crosstrack error */
	_crosstrack_error = 0;

	/* limit eta to 90 degrees */
	eta = math::constrain(eta, (-M_PI_F) / 2.0f, +M_PI_F / 2.0f);
	_lateral_accel = 2.0f * sinf(eta) * omega_vel;
}

void ECL_L1_Pos_Controller::navigate_level_flight(float current_heading)
{
	/* the logic in this section is trivial, but originally proposed by [2] */

	/* reset all heading / error measures resulting in zero roll */
	_target_bearing = current_heading;
	_nav_bearing = current_heading;
	_bearing_error = 0;
	_crosstrack_error = 0;
	_lateral_accel = 0;

	/* not circling a waypoint when flying level */
	_circle_mode = false;

}


math::Vector2f ECL_L1_Pos_Controller::get_local_planar_vector(const math::Vector2f &origin, const math::Vector2f &target) const
{
	/* this is an approximation for small angles, proposed by [2] */

	math::Vector2f out;

	out.setX(math::radians((target.getX() - origin.getX())));
	out.setY(math::radians((target.getY() - origin.getY())*cosf(math::radians(origin.getX()))));

	return out * static_cast<float>(CONSTANTS_RADIUS_OF_EARTH);
}

