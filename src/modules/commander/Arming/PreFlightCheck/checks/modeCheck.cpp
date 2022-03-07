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

#include "../PreFlightCheck.hpp"

#include <systemlib/mavlink_log.h>

using namespace time_literals;

bool PreFlightCheck::modeCheck(orb_advert_t *mavlink_log_pub, const bool report_fail,
			       const vehicle_status_s &vehicle_status)
{
	bool success = true;

	switch (vehicle_status.nav_state) {
	case vehicle_status_s::NAVIGATION_STATE_MANUAL:
	case vehicle_status_s::NAVIGATION_STATE_ALTCTL:
	case vehicle_status_s::NAVIGATION_STATE_POSCTL:
	case vehicle_status_s::NAVIGATION_STATE_AUTO_MISSION:
	case vehicle_status_s::NAVIGATION_STATE_AUTO_LOITER:
	case vehicle_status_s::NAVIGATION_STATE_ACRO:
	case vehicle_status_s::NAVIGATION_STATE_OFFBOARD:
	case vehicle_status_s::NAVIGATION_STATE_STAB:
	case vehicle_status_s::NAVIGATION_STATE_AUTO_TAKEOFF:
	case vehicle_status_s::NAVIGATION_STATE_AUTO_VTOL_TAKEOFF:
		break;

	default:
		success = false;

		if (report_fail) {
			mavlink_log_critical(mavlink_log_pub, "Mode not suitable for takeoff");
		}

		break;
	}

	return success;
}
