/****************************************************************************
 *
 *   Copyright (c) 2014, 2015 PX4 Development Team. All rights reserved.
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
 * @file land_detector.c
 * Land detector algorithm parameters.
 *
 * @author Johan Jansen <jnsn.johan@gmail.com>
 */

#include <systemlib/param/param.h>

/**
 * Multicopter max climb rate
 *
 * Maximum vertical velocity allowed to trigger a land (m/s up and down)
 *
 * @unit m/s
 *
 * @group Land Detector
 */
PARAM_DEFINE_FLOAT(LNDMC_Z_VEL_MAX, 0.30f);

/**
 * Multicopter max horizontal velocity
 *
 * Maximum horizontal velocity allowed to trigger a land (m/s)
 *
 * @unit m/s
 *
 * @group Land Detector
 */
PARAM_DEFINE_FLOAT(LNDMC_XY_VEL_MAX, 1.00f);

/**
 * Multicopter max rotation
 *
 * Maximum allowed around each axis to trigger a land (degrees per second)
 *
 * @unit deg/s
 *
 * @group Land Detector
 */
PARAM_DEFINE_FLOAT(LNDMC_ROT_MAX, 20.0f);

/**
 * Multicopter max throttle
 *
 * Maximum actuator output on throttle before triggering a land
 *
 * @min 0.1
 * @max 0.5
 *
 * @group Land Detector
 */
PARAM_DEFINE_FLOAT(LNDMC_THR_MAX, 0.20f);

/**
 * Fixedwing max horizontal velocity
 *
 * Maximum horizontal velocity allowed to trigger a land (m/s)
 *
 * @min 0.5
 * @max 10
 * @unit m/s
 *
 * @group Land Detector
 */
PARAM_DEFINE_FLOAT(LNDFW_VEL_XY_MAX, 5.0f);

/**
 * Fixedwing max climb rate
 *
 * Maximum vertical velocity allowed to trigger a land (m/s up and down)
 *
 * @min 5
 * @max 20
 * @unit m/s
 *
 * @group Land Detector
 */
PARAM_DEFINE_FLOAT(LNDFW_VEL_Z_MAX, 10.0f);

/**
 * Airspeed max
 *
 * Maximum airspeed allowed to trigger a land (m/s)
 *
 * @min 4
 * @max 20
 * @unit m/s
 *
 * @group Land Detector
 */
PARAM_DEFINE_FLOAT(LNDFW_AIRSPD_MAX, 8.00f);
