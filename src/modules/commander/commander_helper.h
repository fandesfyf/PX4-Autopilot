/****************************************************************************
 *
 *   Copyright (C) 2013 PX4 Development Team. All rights reserved.
 *   Author: Thomas Gubler <thomasgubler@student.ethz.ch>
 *           Julian Oes <joes@student.ethz.ch>
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
 * @file commander_helper.h
 * Commander helper functions definitions
 */

#ifndef COMMANDER_HELPER_H_
#define COMMANDER_HELPER_H_

#include <uORB/uORB.h>
#include <uORB/topics/vehicle_status.h>
#include <uORB/topics/actuator_armed.h>
#include <uORB/topics/vehicle_control_mode.h>
#include <drivers/drv_rgbled.h>


bool is_multirotor(const struct vehicle_status_s *current_status);
bool is_rotary_wing(const struct vehicle_status_s *current_status);

int buzzer_init(void);
void buzzer_deinit(void);

void tune_error(void);
void tune_positive(void);
void tune_neutral(void);
void tune_negative(void);
int tune_arm(void);
int tune_low_bat(void);
int tune_critical_bat(void);
void tune_stop(void);
int blink_msg_state();

int led_init(void);
void led_deinit(void);
int led_toggle(int led);
int led_on(int led);
int led_off(int led);

void rgbled_set_color(rgbled_color_t color);
void rgbled_set_mode(rgbled_mode_t mode);
void rgbled_set_pattern(rgbled_pattern_t *pattern);

/**
 * Provides a coarse estimate of remaining battery power.
 *
 * The estimate is very basic and based on decharging voltage curves.
 *
 * @return the estimated remaining capacity in 0..1
 */
float battery_remaining_estimate_voltage(float voltage);

#endif /* COMMANDER_HELPER_H_ */
