/****************************************************************************
 *
 *   Copyright (C) 2012, 2014 PX4 Development Team. All rights reserved.
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
 * @file geo.c
 *
 * Geo / math functions to perform geodesic calculations
 *
 * @author Thomas Gubler <thomasgubler@student.ethz.ch>
 * @author Julian Oes <joes@student.ethz.ch>
 * @author Lorenz Meier <lm@inf.ethz.ch>
 * @author Anton Babushkin <anton.babushkin@me.com>
 */

#include <geo/geo.h>
#include <nuttx/config.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

/*
 * Azimuthal Equidistant Projection
 * formulas according to: http://mathworld.wolfram.com/AzimuthalEquidistantProjection.html
 */

static struct map_projection_reference_s mp_ref = {0};

__EXPORT bool map_projection_initialized()
{
	return mp_ref.init_done;
}

__EXPORT uint64_t map_projection_timestamp()
{
	return mp_ref.timestamp;
}

__EXPORT int map_projection_init(double lat_0, double lon_0, uint64_t timestamp) //lat_0, lon_0 are expected to be in correct format: -> 47.1234567 and not 471234567
{
	if (strcmp("navigator", getprogname() == 0)) {

		mp_ref.lat = lat_0 / 180.0 * M_PI;
		mp_ref.lon = lon_0 / 180.0 * M_PI;

		mp_ref.sin_lat = sin(mp_ref.lat);
		mp_ref.cos_lat = cos(mp_ref.lat);

		mp_ref.timestamp = timestamp;
		mp_ref.init_done = true;

		return 0;
	} else {
		return -1;
	}
}

__EXPORT int map_projection_reference(double *ref_lat, double *ref_lon)
{
	if (!map_projection_initialized()) {
		return -1;
	}

	*ref_lat = mp_ref.lat;
	*ref_lon = mp_ref.lon;

	return 0;
}

__EXPORT int map_projection_project(double lat, double lon, float *x, float *y)
{
	if (!map_projection_initialized()) {
		return -1;
	}

	double lat_rad = lat / 180.0 * M_PI;
	double lon_rad = lon / 180.0 * M_PI;

	double sin_lat = sin(lat_rad);
	double cos_lat = cos(lat_rad);
	double cos_d_lon = cos(lon_rad - mp_ref.lon);

	double c = acos(mp_ref.sin_lat * sin_lat + mp_ref.cos_lat * cos_lat * cos_d_lon);
	double k = (c == 0.0) ? 1.0 : (c / sin(c));

	*x = k * (mp_ref.cos_lat * sin_lat - mp_ref.sin_lat * cos_lat * cos_d_lon) * CONSTANTS_RADIUS_OF_EARTH;
	*y = k * cos_lat * sin(lon_rad - mp_ref.lon) * CONSTANTS_RADIUS_OF_EARTH;

	return 0;
}

__EXPORT int map_projection_reproject(float x, float y, double *lat, double *lon)
{
	if (!map_projection_initialized()) {
		return -1;
	}

	float x_rad = x / CONSTANTS_RADIUS_OF_EARTH;
	float y_rad = y / CONSTANTS_RADIUS_OF_EARTH;
	double c = sqrtf(x_rad * x_rad + y_rad * y_rad);
	double sin_c = sin(c);
	double cos_c = cos(c);

	double lat_rad;
	double lon_rad;

	if (c != 0.0) {
		lat_rad = asin(cos_c * mp_ref.sin_lat + (x_rad * sin_c * mp_ref.cos_lat) / c);
		lon_rad = (mp_ref.lon + atan2(y_rad * sin_c, c * mp_ref.cos_lat * cos_c - x_rad * mp_ref.sin_lat * sin_c));

	} else {
		lat_rad = mp_ref.lat;
		lon_rad = mp_ref.lon;
	}

	*lat = lat_rad * 180.0 / M_PI;
	*lon = lon_rad * 180.0 / M_PI;

	return 0;
}


__EXPORT float get_distance_to_next_waypoint(double lat_now, double lon_now, double lat_next, double lon_next)
{
	double lat_now_rad = lat_now / 180.0d * M_PI;
	double lon_now_rad = lon_now / 180.0d * M_PI;
	double lat_next_rad = lat_next / 180.0d * M_PI;
	double lon_next_rad = lon_next / 180.0d * M_PI;


	double d_lat = lat_next_rad - lat_now_rad;
	double d_lon = lon_next_rad - lon_now_rad;

	double a = sin(d_lat / 2.0d) * sin(d_lat / 2.0d) + sin(d_lon / 2.0d) * sin(d_lon / 2.0d) * cos(lat_now_rad) * cos(lat_next_rad);
	double c = 2.0d * atan2(sqrt(a), sqrt(1.0d - a));

	return CONSTANTS_RADIUS_OF_EARTH * c;
}

__EXPORT float get_bearing_to_next_waypoint(double lat_now, double lon_now, double lat_next, double lon_next)
{
	double lat_now_rad = lat_now * M_DEG_TO_RAD;
	double lon_now_rad = lon_now * M_DEG_TO_RAD;
	double lat_next_rad = lat_next * M_DEG_TO_RAD;
	double lon_next_rad = lon_next * M_DEG_TO_RAD;

	double d_lon = lon_next_rad - lon_now_rad;

	/* conscious mix of double and float trig function to maximize speed and efficiency */
	float theta = atan2f(sin(d_lon) * cos(lat_next_rad) , cos(lat_now_rad) * sin(lat_next_rad) - sin(lat_now_rad) * cos(lat_next_rad) * cos(d_lon));

	theta = _wrap_pi(theta);

	return theta;
}

__EXPORT void get_vector_to_next_waypoint(double lat_now, double lon_now, double lat_next, double lon_next, float *v_n, float *v_e)
{
	double lat_now_rad = lat_now * M_DEG_TO_RAD;
	double lon_now_rad = lon_now * M_DEG_TO_RAD;
	double lat_next_rad = lat_next * M_DEG_TO_RAD;
	double lon_next_rad = lon_next * M_DEG_TO_RAD;

	double d_lat = lat_next_rad - lat_now_rad;
	double d_lon = lon_next_rad - lon_now_rad;

	/* conscious mix of double and float trig function to maximize speed and efficiency */
	*v_n = CONSTANTS_RADIUS_OF_EARTH * (cos(lat_now_rad) * sin(lat_next_rad) - sin(lat_now_rad) * cos(lat_next_rad) * cos(d_lon));
	*v_e = CONSTANTS_RADIUS_OF_EARTH * sin(d_lon) * cos(lat_next_rad);
}

__EXPORT void get_vector_to_next_waypoint_fast(double lat_now, double lon_now, double lat_next, double lon_next, float *v_n, float *v_e)
{
	double lat_now_rad = lat_now * M_DEG_TO_RAD;
	double lon_now_rad = lon_now * M_DEG_TO_RAD;
	double lat_next_rad = lat_next * M_DEG_TO_RAD;
	double lon_next_rad = lon_next * M_DEG_TO_RAD;

	double d_lat = lat_next_rad - lat_now_rad;
	double d_lon = lon_next_rad - lon_now_rad;

	/* conscious mix of double and float trig function to maximize speed and efficiency */
	*v_n = CONSTANTS_RADIUS_OF_EARTH * d_lat;
	*v_e = CONSTANTS_RADIUS_OF_EARTH * d_lon * cos(lat_now_rad);
}

__EXPORT void add_vector_to_global_position(double lat_now, double lon_now, float v_n, float v_e, double *lat_res, double *lon_res)
{
	double lat_now_rad = lat_now * M_DEG_TO_RAD;
	double lon_now_rad = lon_now * M_DEG_TO_RAD;

	*lat_res = (lat_now_rad + v_n / CONSTANTS_RADIUS_OF_EARTH) * M_RAD_TO_DEG;
	*lon_res = (lon_now_rad + v_e / (CONSTANTS_RADIUS_OF_EARTH * cos(lat_now_rad))) * M_RAD_TO_DEG;
}

// Additional functions - @author Doug Weibel <douglas.weibel@colorado.edu>

__EXPORT int get_distance_to_line(struct crosstrack_error_s *crosstrack_error, double lat_now, double lon_now, double lat_start, double lon_start, double lat_end, double lon_end)
{
// This function returns the distance to the nearest point on the track line.  Distance is positive if current
// position is right of the track and negative if left of the track as seen from a point on the track line
// headed towards the end point.

	float dist_to_end;
	float bearing_end;
	float bearing_track;
	float bearing_diff;

	int return_value = ERROR;	// Set error flag, cleared when valid result calculated.
	crosstrack_error->past_end = false;
	crosstrack_error->distance = 0.0f;
	crosstrack_error->bearing = 0.0f;

	// Return error if arguments are bad
	if (lat_now == 0.0d || lon_now == 0.0d || lat_start == 0.0d || lon_start == 0.0d || lat_end == 0.0d || lon_end == 0.0d) { return return_value; }

	bearing_end = get_bearing_to_next_waypoint(lat_now, lon_now, lat_end, lon_end);
	bearing_track = get_bearing_to_next_waypoint(lat_start, lon_start, lat_end, lon_end);
	bearing_diff = bearing_track - bearing_end;
	bearing_diff = _wrap_pi(bearing_diff);

	// Return past_end = true if past end point of line
	if (bearing_diff > M_PI_2_F || bearing_diff < -M_PI_2_F) {
		crosstrack_error->past_end = true;
		return_value = OK;
		return return_value;
	}

	dist_to_end = get_distance_to_next_waypoint(lat_now, lon_now, lat_end, lon_end);
	crosstrack_error->distance = (dist_to_end) * sin(bearing_diff);

	if (sin(bearing_diff) >= 0) {
		crosstrack_error->bearing = _wrap_pi(bearing_track - M_PI_2_F);

	} else {
		crosstrack_error->bearing = _wrap_pi(bearing_track + M_PI_2_F);
	}

	return_value = OK;

	return return_value;

}


__EXPORT int get_distance_to_arc(struct crosstrack_error_s *crosstrack_error, double lat_now, double lon_now, double lat_center, double lon_center,
				 float radius, float arc_start_bearing, float arc_sweep)
{
	// This function returns the distance to the nearest point on the track arc.  Distance is positive if current
	// position is right of the arc and negative if left of the arc as seen from the closest point on the arc and
	// headed towards the end point.

	// Determine if the current position is inside or outside the sector between the line from the center
	// to the arc start and the line from the center to the arc end
	float	bearing_sector_start;
	float	bearing_sector_end;
	float	bearing_now = get_bearing_to_next_waypoint(lat_now, lon_now, lat_center, lon_center);
	bool	in_sector;

	int return_value = ERROR;		// Set error flag, cleared when valid result calculated.
	crosstrack_error->past_end = false;
	crosstrack_error->distance = 0.0f;
	crosstrack_error->bearing = 0.0f;

	// Return error if arguments are bad
	if (lat_now == 0.0d || lon_now == 0.0d || lat_center == 0.0d || lon_center == 0.0d || radius == 0.0d) { return return_value; }


	if (arc_sweep >= 0) {
		bearing_sector_start = arc_start_bearing;
		bearing_sector_end = arc_start_bearing + arc_sweep;

		if (bearing_sector_end > 2.0f * M_PI_F) { bearing_sector_end -= M_TWOPI_F; }

	} else {
		bearing_sector_end = arc_start_bearing;
		bearing_sector_start = arc_start_bearing - arc_sweep;

		if (bearing_sector_start < 0.0f) { bearing_sector_start += M_TWOPI_F; }
	}

	in_sector = false;

	// Case where sector does not span zero
	if (bearing_sector_end >= bearing_sector_start && bearing_now >= bearing_sector_start && bearing_now <= bearing_sector_end) { in_sector = true; }

	// Case where sector does span zero
	if (bearing_sector_end < bearing_sector_start && (bearing_now > bearing_sector_start || bearing_now < bearing_sector_end)) { in_sector = true; }

	// If in the sector then calculate distance and bearing to closest point
	if (in_sector) {
		crosstrack_error->past_end = false;
		float dist_to_center = get_distance_to_next_waypoint(lat_now, lon_now, lat_center, lon_center);

		if (dist_to_center <= radius) {
			crosstrack_error->distance = radius - dist_to_center;
			crosstrack_error->bearing = bearing_now + M_PI_F;

		} else {
			crosstrack_error->distance = dist_to_center - radius;
			crosstrack_error->bearing = bearing_now;
		}

		// If out of the sector then calculate dist and bearing to start or end point

	} else {

		// Use the approximation  that 111,111 meters in the y direction is 1 degree (of latitude)
		// and 111,111 * cos(latitude) meters in the x direction is 1 degree (of longitude) to
		// calculate the position of the start and end points.  We should not be doing this often
		// as this function generally will not be called repeatedly when we are out of the sector.

		// TO DO - this is messed up and won't compile
		float start_disp_x = radius * sin(arc_start_bearing);
		float start_disp_y = radius * cos(arc_start_bearing);
		float end_disp_x = radius * sin(_wrapPI(arc_start_bearing + arc_sweep));
		float end_disp_y = radius * cos(_wrapPI(arc_start_bearing + arc_sweep));
		float lon_start = lon_now + start_disp_x / 111111.0d;
		float lat_start = lat_now + start_disp_y * cos(lat_now) / 111111.0d;
		float lon_end = lon_now + end_disp_x / 111111.0d;
		float lat_end = lat_now + end_disp_y * cos(lat_now) / 111111.0d;
		float dist_to_start = get_distance_to_next_waypoint(lat_now, lon_now, lat_start, lon_start);
		float dist_to_end = get_distance_to_next_waypoint(lat_now, lon_now, lat_end, lon_end);


		if (dist_to_start < dist_to_end) {
			crosstrack_error->distance = dist_to_start;
			crosstrack_error->bearing = get_bearing_to_next_waypoint(lat_now, lon_now, lat_start, lon_start);

		} else {
			crosstrack_error->past_end = true;
			crosstrack_error->distance = dist_to_end;
			crosstrack_error->bearing = get_bearing_to_next_waypoint(lat_now, lon_now, lat_end, lon_end);
		}

	}

	crosstrack_error->bearing = _wrapPI(crosstrack_error->bearing);
	return_value = OK;
	return return_value;
}

__EXPORT float get_distance_to_point_global_wgs84(double lat_now, double lon_now, float alt_now,
		double lat_next, double lon_next, float alt_next,
		float *dist_xy, float *dist_z)
{
	double current_x_rad = lat_next / 180.0 * M_PI;
	double current_y_rad = lon_next / 180.0 * M_PI;
	double x_rad = lat_now / 180.0 * M_PI;
	double y_rad = lon_now / 180.0 * M_PI;

	double d_lat = x_rad - current_x_rad;
	double d_lon = y_rad - current_y_rad;

	double a = sin(d_lat / 2.0) * sin(d_lat / 2.0) + sin(d_lon / 2.0) * sin(d_lon / 2.0f) * cos(current_x_rad) * cos(x_rad);
	double c = 2 * atan2(sqrt(a), sqrt(1 - a));

	float dxy = CONSTANTS_RADIUS_OF_EARTH * c;
	float dz = alt_now - alt_next;

	*dist_xy = fabsf(dxy);
	*dist_z = fabsf(dz);

	return sqrtf(dxy * dxy + dz * dz);
}


__EXPORT float mavlink_wpm_distance_to_point_local(float x_now, float y_now, float z_now,
		float x_next, float y_next, float z_next,
		float *dist_xy, float *dist_z)
{
	float dx = x_now - x_next;
	float dy = y_now - y_next;
	float dz = z_now - z_next;

	*dist_xy = sqrtf(dx * dx + dy * dy);
	*dist_z = fabsf(dz);

	return sqrtf(dx * dx + dy * dy + dz * dz);
}

__EXPORT float _wrap_pi(float bearing)
{
	/* value is inf or NaN */
	if (!isfinite(bearing)) {
		return bearing;
	}

	int c = 0;
	while (bearing >= M_PI_F) {
		bearing -= M_TWOPI_F;

		if (c++ > 3) {
			return NAN;
		}
	}

	c = 0;
	while (bearing < -M_PI_F) {
		bearing += M_TWOPI_F;

		if (c++ > 3) {
			return NAN;
		}
	}

	return bearing;
}

__EXPORT float _wrap_2pi(float bearing)
{
	/* value is inf or NaN */
	if (!isfinite(bearing)) {
		return bearing;
	}

	int c = 0;
	while (bearing >= M_TWOPI_F) {
		bearing -= M_TWOPI_F;

		if (c++ > 3) {
			return NAN;
		}
	}

	c = 0;
	while (bearing < 0.0f) {
		bearing += M_TWOPI_F;

		if (c++ > 3) {
			return NAN;
		}
	}

	return bearing;
}

__EXPORT float _wrap_180(float bearing)
{
	/* value is inf or NaN */
	if (!isfinite(bearing)) {
		return bearing;
	}

	int c = 0;
	while (bearing >= 180.0f) {
		bearing -= 360.0f;

		if (c++ > 3) {
			return NAN;
		}
	}

	c = 0;
	while (bearing < -180.0f) {
		bearing += 360.0f;

		if (c++ > 3) {
			return NAN;
		}
	}

	return bearing;
}

__EXPORT float _wrap_360(float bearing)
{
	/* value is inf or NaN */
	if (!isfinite(bearing)) {
		return bearing;
	}

	int c = 0;
	while (bearing >= 360.0f) {
		bearing -= 360.0f;

		if (c++ > 3) {
			return NAN;
		}
	}

	c = 0;
	while (bearing < 0.0f) {
		bearing += 360.0f;

		if (c++ > 3) {
			return NAN;
		}
	}

	return bearing;
}
