/****************************************************************************
 *
 *   Copyright (C) 2012 PX4 Development Team. All rights reserved.
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
 * @file actuator_controls.h
 *
 * Actuator control topics - mixer inputs.
 *
 * Values published to these topics are the outputs of the vehicle control
 * system, and are expected to be mixed and used to drive the actuators
 * (servos, speed controls, etc.) that operate the vehicle.
 *
 * Each topic can be published by a single controller
 */

#ifndef TOPIC_ACTUATOR_SAFETY_H
#define TOPIC_ACTUATOR_SAFETY_H

#include <stdint.h>
#include "../uORB.h"

/** global 'actuator output is live' control. */
struct actuator_safety_s {

	uint64_t	timestamp;

	bool	safety_off;	/**< Set to true if safety is off */
	bool	armed;		/**< Set to true if system is armed */
	bool	ready_to_arm;	/**< Set to true if system is ready to be armed */
	bool	lockdown;	/**< Set to true if actuators are forced to being disabled (due to emergency or HIL) */
	bool	hil_enabled;	/**< Set to true if hardware-in-the-loop (HIL) is enabled */
};

ORB_DECLARE(actuator_safety);

#endif