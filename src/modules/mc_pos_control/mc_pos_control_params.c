/****************************************************************************
 *
 *   Copyright (c) 2013 PX4 Development Team. All rights reserved.
 *   Author: @author Anton Babushkin <anton.babushkin@me.com>
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
 * @file mc_pos_control_params.c
 * Multicopter position controller parameters.
 */

#include <systemlib/param/param.h>

PARAM_DEFINE_FLOAT(MPC_THR_MIN, 0.0f);
PARAM_DEFINE_FLOAT(MPC_THR_MAX, 1.0f);
PARAM_DEFINE_FLOAT(MPC_Z_P, 1.0f);
PARAM_DEFINE_FLOAT(MPC_Z_VEL_P, 0.1f);
PARAM_DEFINE_FLOAT(MPC_Z_VEL_I, 0.02f);
PARAM_DEFINE_FLOAT(MPC_Z_VEL_D, 0.0f);
PARAM_DEFINE_FLOAT(MPC_Z_VEL_MAX, 5.0f);
PARAM_DEFINE_FLOAT(MPC_Z_FF, 0.5f);
PARAM_DEFINE_FLOAT(MPC_XY_P, 1.0f);
PARAM_DEFINE_FLOAT(MPC_XY_VEL_P, 0.1f);
PARAM_DEFINE_FLOAT(MPC_XY_VEL_I, 0.02f);
PARAM_DEFINE_FLOAT(MPC_XY_VEL_D, 0.01f);
PARAM_DEFINE_FLOAT(MPC_XY_VEL_MAX, 5.0f);
PARAM_DEFINE_FLOAT(MPC_XY_FF, 0.5f);
PARAM_DEFINE_FLOAT(MPC_TILT_MAX, 1.0f);
PARAM_DEFINE_FLOAT(MPC_LAND_SPEED, 1.0f);
PARAM_DEFINE_FLOAT(MPC_LAND_TILT_MAX, 0.4f);
