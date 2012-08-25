/****************************************************************************
 *
 *   Copyright (C) 2012 PX4 Development Team. All rights reserved.
 *   Author: @author Lorenz Meier <lm@inf.ethz.ch>
 *           @author Thomas Gubler <thomasgubler@student.ethz.ch>
 *           @author Julian Oes <joes@student.ethz.ch>
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
 * @file sensor_params.c
 *
 * Parameters defined by the sensors task.
 */

#include <nuttx/config.h>

#include <systemlib/param/param.h>

PARAM_DEFINE_FLOAT(SENSOR_GYRO_XOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_GYRO_YOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_GYRO_ZOFF, 0.0f);

PARAM_DEFINE_FLOAT(SENSOR_MAG_XOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_MAG_YOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_MAG_ZOFF, 0.0f);

PARAM_DEFINE_FLOAT(SENSOR_ACC_XOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_ACC_YOFF, 0.0f);
PARAM_DEFINE_FLOAT(SENSOR_ACC_ZOFF, 0.0f);

PARAM_DEFINE_FLOAT(RC1_MIN, 1000.0f);
PARAM_DEFINE_FLOAT(RC1_TRIM, 1500.0f);
PARAM_DEFINE_FLOAT(RC1_MAX, 2000.0f);
PARAM_DEFINE_FLOAT(RC1_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC2_MIN, 1000);
PARAM_DEFINE_FLOAT(RC2_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC2_MAX, 2000);
PARAM_DEFINE_FLOAT(RC2_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC3_MIN, 1000);
PARAM_DEFINE_FLOAT(RC3_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC3_MAX, 2000);
PARAM_DEFINE_FLOAT(RC3_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC4_MIN, 1000);
PARAM_DEFINE_FLOAT(RC4_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC4_MAX, 2000);
PARAM_DEFINE_FLOAT(RC4_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC5_MIN, 1000);
PARAM_DEFINE_FLOAT(RC5_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC5_MAX, 2000);
PARAM_DEFINE_FLOAT(RC5_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC6_MIN, 1000);
PARAM_DEFINE_FLOAT(RC6_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC6_MAX, 2000);
PARAM_DEFINE_FLOAT(RC6_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC7_MIN, 1000);
PARAM_DEFINE_FLOAT(RC7_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC7_MAX, 2000);
PARAM_DEFINE_FLOAT(RC7_REV, 1.0f);

PARAM_DEFINE_FLOAT(RC8_MIN, 1000);
PARAM_DEFINE_FLOAT(RC8_TRIM, 1500);
PARAM_DEFINE_FLOAT(RC8_MAX, 2000);
PARAM_DEFINE_FLOAT(RC8_REV, 1.0f);

PARAM_DEFINE_INT32(RC_TYPE, 1); // 1 = FUTABA

/* default is conversion factor for the PX4IO / PX4IOAR board, the factor for PX4FMU standalone is different */
PARAM_DEFINE_FLOAT(BAT_V_SCALING, (3.3f * 52.0f / 5.0f / 4095.0f));

PARAM_DEFINE_INT32(RC_MAP_ROLL, 1);
PARAM_DEFINE_INT32(RC_MAP_PITCH, 2);
PARAM_DEFINE_INT32(RC_MAP_THROTTLE, 3);
PARAM_DEFINE_INT32(RC_MAP_YAW, 4);
PARAM_DEFINE_INT32(RC_MAP_MODE_SW, 5);
