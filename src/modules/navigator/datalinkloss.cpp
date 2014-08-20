/****************************************************************************
 *
 *   Copyright (c) 2013-2014 PX4 Development Team. All rights reserved.
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
 * @file datalinkloss.cpp
 * Helper class for Data Link Loss Mode according to the OBC rules
 *
 * @author Thomas Gubler <thomasgubler@gmail.com>
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>

#include <mavlink/mavlink_log.h>
#include <systemlib/err.h>
#include <geo/geo.h>

#include <uORB/uORB.h>
#include <uORB/topics/mission.h>
#include <uORB/topics/home_position.h>

#include "navigator.h"
#include "datalinkloss.h"

#define DELAY_SIGMA	0.01f

DataLinkLoss::DataLinkLoss(Navigator *navigator, const char *name) :
	MissionBlock(navigator, name),
	_param_commsholdwaittime(this, "CH_T"),
	_param_commsholdlat(this, "CH_LAT"),
	_param_commsholdlon(this, "CH_LON"),
	_param_commsholdalt(this, "CH_ALT"),
	_param_airfieldhomelat(this, "AH_LAT"),
	_param_airfieldhomelon(this, "AH_LON"),
	_param_airfieldhomealt(this, "AH_ALT"),
	_param_numberdatalinklosses(this, "N"),
	_dll_state(DLL_STATE_NONE)
{
	/* load initial params */
	updateParams();
	/* initial reset */
	on_inactive();
}

DataLinkLoss::~DataLinkLoss()
{
}

void
DataLinkLoss::on_inactive()
{
	/* reset DLL state only if setpoint moved */
	if (!_navigator->get_can_loiter_at_sp()) {
		_dll_state = DLL_STATE_NONE;
	}
}

void
DataLinkLoss::on_activation()
{
	_dll_state = DLL_STATE_NONE;
	updateParams();
	advance_dll();
	set_dll_item();
}

void
DataLinkLoss::on_active()
{
	if (is_mission_item_reached()) {
		updateParams();
		advance_dll();
		set_dll_item();
	}
}

void
DataLinkLoss::set_dll_item()
{
	struct position_setpoint_triplet_s *pos_sp_triplet = _navigator->get_position_setpoint_triplet();

	set_previous_pos_setpoint();
	_navigator->set_can_loiter_at_sp(false);

	switch (_dll_state) {
	case DLL_STATE_FLYTOCOMMSHOLDWP: {
		_mission_item.lat = (double)(_param_commsholdlat.get()) * 1.0e-7;
		_mission_item.lon = (double)(_param_commsholdlon.get()) * 1.0e-7;
		_mission_item.altitude_is_relative = false;
		_mission_item.altitude = _param_commsholdalt.get();
		_mission_item.yaw = NAN;
		_mission_item.loiter_radius = _navigator->get_loiter_radius();
		_mission_item.loiter_direction = 1;
		_mission_item.nav_cmd = NAV_CMD_LOITER_TIME_LIMIT;
		_mission_item.acceptance_radius = _navigator->get_acceptance_radius();
		_mission_item.time_inside = _param_commsholdwaittime.get() < 0.0f ? 0.0f : _param_commsholdwaittime.get();
		_mission_item.pitch_min = 0.0f;
		_mission_item.autocontinue = true;
		_mission_item.origin = ORIGIN_ONBOARD;

		_navigator->set_can_loiter_at_sp(true);
		break;
	}
	case DLL_STATE_FLYTOAIRFIELDHOMEWP: {
		_mission_item.lat = (double)(_param_airfieldhomelat.get()) * 1.0e-7;
		_mission_item.lon = (double)(_param_airfieldhomelon.get()) * 1.0e-7;
		_mission_item.altitude_is_relative = false;
		_mission_item.altitude = _param_airfieldhomealt.get();
		_mission_item.yaw = NAN;
		_mission_item.loiter_radius = _navigator->get_loiter_radius();
		_mission_item.loiter_direction = 1;
		_mission_item.nav_cmd = NAV_CMD_LOITER_UNLIMITED;
		_mission_item.acceptance_radius = _navigator->get_acceptance_radius();
		_mission_item.pitch_min = 0.0f;
		_mission_item.autocontinue = true;
		_mission_item.origin = ORIGIN_ONBOARD;

		_navigator->set_can_loiter_at_sp(true);
		break;
	}
	default:
		break;
	}

	reset_mission_item_reached();

	/* convert mission item to current position setpoint and make it valid */
	mission_item_to_position_setpoint(&_mission_item, &pos_sp_triplet->current);
	pos_sp_triplet->next.valid = false;

	_navigator->set_position_setpoint_triplet_updated();
}

void
DataLinkLoss::advance_dll()
{
	switch (_dll_state) {
	case DLL_STATE_NONE:
		/* Check the number of data link losses. If above home fly home directly */
		if (_navigator->get_vstatus()->data_link_lost_counter > _param_numberdatalinklosses.get()) {
			warnx("%d data link losses, limit is %d, fly to airfield home", _navigator->get_vstatus()->data_link_lost_counter, _param_numberdatalinklosses.get());
			mavlink_log_info(_navigator->get_mavlink_fd(), "#audio: too many DL losses, fly to home");
			_dll_state = DLL_STATE_FLYTOAIRFIELDHOMEWP;
		} else {
			warnx("fly to comms hold, datalink loss counter: %d", _navigator->get_vstatus()->data_link_lost_counter);
			mavlink_log_info(_navigator->get_mavlink_fd(), "#audio: fly to comms hold");
			_dll_state = DLL_STATE_FLYTOCOMMSHOLDWP;
		}
		break;
	case DLL_STATE_FLYTOCOMMSHOLDWP:
		warnx("fly to airfield home");
			mavlink_log_info(_navigator->get_mavlink_fd(), "#audio: fly to airfield home");
		_dll_state = DLL_STATE_FLYTOAIRFIELDHOMEWP;
		_navigator->get_mission_result()->stay_in_failsafe = true;
		_navigator->publish_mission_result();
		break;
	default:
		break;
	}
}
