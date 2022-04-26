/****************************************************************************
 *
 *   Copyright (C) 2014-2020 PX4 Development Team. All rights reserved.
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
 * UAVCAN Node ID.
 *
 * Read the specs at http://uavcan.org to learn more about Node ID.
 *
 * @min 1
 * @max 125
 * @group UAVCAN
 */
PARAM_DEFINE_INT32(CANNODE_NODE_ID, 120);

/**
 * UAVCAN CAN bus bitrate.
 *
 * @min 20000
 * @max 1000000
 * @group UAVCAN
 */
PARAM_DEFINE_INT32(CANNODE_BITRATE, 1000000);

/**
 * CAN built-in bus termination
 *
 * @boolean
 * @max 1
 * @group UAVCAN
 */
PARAM_DEFINE_INT32(CANNODE_TERM, 0);

/**
 * Cannode flow board rotation
 *
 * This parameter defines the yaw rotation of the Cannode flow board relative to the vehicle body frame.
 * Zero rotation is defined as X on flow board pointing towards front of vehicle.
 *
 * @value 0 No rotation
 * @value 1 Yaw 45°
 * @value 2 Yaw 90°
 * @value 3 Yaw 135°
 * @value 4 Yaw 180°
 * @value 5 Yaw 225°
 * @value 6 Yaw 270°
 * @value 7 Yaw 315°
 *
 * @reboot_required true
 *
 * @group UAVCAN
 */
PARAM_DEFINE_INT32(CANNODE_FLOW_ROT, 0);
