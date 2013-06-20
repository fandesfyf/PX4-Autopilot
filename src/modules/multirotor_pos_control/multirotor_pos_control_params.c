/****************************************************************************
 *
 *   Copyright (C) 2012 PX4 Development Team. All rights reserved.
 *   Author: Tobias Naegeli <naegelit@student.ethz.ch>
 *           Lorenz Meier <lm@inf.ethz.ch>
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

/*
 * @file multirotor_pos_control_params.c
 * 
 * Parameters for multirotor_pos_control
 */

#include "multirotor_pos_control_params.h"

/* controller parameters */
PARAM_DEFINE_FLOAT(MPC_THR_MIN, 0.3f);
PARAM_DEFINE_FLOAT(MPC_THR_MAX, 0.7f);
PARAM_DEFINE_FLOAT(MPC_ALT_P, 0.1f);
PARAM_DEFINE_FLOAT(MPC_ALT_I, 0.1f);
PARAM_DEFINE_FLOAT(MPC_ALT_D, 0.1f);
PARAM_DEFINE_FLOAT(MPC_ALT_RATE_MAX, 3.0f);
PARAM_DEFINE_FLOAT(MPC_POS_P, 0.1f);
PARAM_DEFINE_FLOAT(MPC_POS_I, 0.0f);
PARAM_DEFINE_FLOAT(MPC_POS_D, 0.2f);
PARAM_DEFINE_FLOAT(MPC_POS_RATE_MAX, 10.0f);
PARAM_DEFINE_FLOAT(MPC_SLOPE_MAX, 0.5f);
PARAM_DEFINE_INT32(MPC_HARD, 0);

int parameters_init(struct multirotor_position_control_param_handles *h)
{
	h->thr_min 	=	param_find("MPC_THR_MIN");
	h->thr_max 	=	param_find("MPC_THR_MAX");
	h->alt_p 	=	param_find("MPC_ALT_P");
	h->alt_i 	=	param_find("MPC_ALT_I");
	h->alt_d 	=	param_find("MPC_ALT_D");
	h->alt_rate_max 	=	param_find("MPC_ALT_RATE_MAX");
	h->pos_p 	=	param_find("MPC_POS_P");
	h->pos_i 	=	param_find("MPC_POS_I");
	h->pos_d 	=	param_find("MPC_POS_D");
	h->pos_rate_max 	=	param_find("MPC_POS_RATE_MAX");
	h->slope_max 	=	param_find("MPC_SLOPE_MAX");
	h->hard 	=	param_find("MPC_HARD");

	h->rc_scale_pitch    =   param_find("RC_SCALE_PITCH");
	h->rc_scale_roll    =   param_find("RC_SCALE_ROLL");
	h->rc_scale_yaw      =   param_find("RC_SCALE_YAW");

	return OK;
}

int parameters_update(const struct multirotor_position_control_param_handles *h, struct multirotor_position_control_params *p)
{
	param_get(h->thr_min, &(p->thr_min));
	param_get(h->thr_max, &(p->thr_max));
	param_get(h->alt_p, &(p->alt_p));
	param_get(h->alt_i, &(p->alt_i));
	param_get(h->alt_d, &(p->alt_d));
	param_get(h->alt_rate_max, &(p->alt_rate_max));
	param_get(h->pos_p, &(p->pos_p));
	param_get(h->pos_i, &(p->pos_i));
	param_get(h->pos_d, &(p->pos_d));
	param_get(h->pos_rate_max, &(p->pos_rate_max));
	param_get(h->slope_max, &(p->slope_max));
	param_get(h->hard, &(p->hard));

	param_get(h->rc_scale_pitch, &(p->rc_scale_pitch));
	param_get(h->rc_scale_roll, &(p->rc_scale_roll));
	param_get(h->rc_scale_yaw, &(p->rc_scale_yaw));

	return OK;
}
