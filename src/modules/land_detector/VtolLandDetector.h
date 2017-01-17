/****************************************************************************
 *
 *   Copyright (c) 2013-2016 PX4 Development Team. All rights reserved.
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
 * @file VtolLandDetector.h
 * Land detection implementation for VTOL also called hybrids.
 *
 * @author Roman Bapst <bapstr@gmail.com>
 * @author Julian Oes <julian@oes.ch>
 */

#pragma once

#include <uORB/topics/airspeed.h>

#include "MulticopterLandDetector.h"

namespace land_detector
{

class VtolLandDetector : public MulticopterLandDetector
{
public:
	VtolLandDetector();

protected:
	virtual void _initialize_topics() override;

	virtual void _update_params() override;

	virtual void _update_topics() override;

	virtual bool _get_landed_state() override;

	bool _get_ground_contact_state();

	virtual bool _get_freefall_state() override;

private:
	struct {
		param_t maxAirSpeed;
	} _paramHandle;

	struct {
		float maxAirSpeed;
	} _params;

	int _airspeedSub;

	struct airspeed_s _airspeed;

	bool _was_in_air; /**< indicates whether the vehicle was in the air in the previous iteration */
	float _airspeed_filtered; /**< low pass filtered airspeed */
};


} // namespace land_detector
