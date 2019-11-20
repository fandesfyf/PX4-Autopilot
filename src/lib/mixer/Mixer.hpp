/****************************************************************************
 *
 *   Copyright (C) 2012-2019 PX4 Development Team. All rights reserved.
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

#pragma once

/** simple channel scaler */
struct mixer_scaler_s {
	float			negative_scale;
	float			positive_scale;
	float			offset;
	float			min_output;
	float			max_output;
};

/**
 * Abstract class defining a mixer mixing zero or more inputs to
 * one or more outputs.
 */
class Mixer
{
public:
	enum class Airmode : int32_t {
		disabled = 0,
		roll_pitch = 1,
		roll_pitch_yaw = 2
	};

	/** next mixer in a list */
	Mixer				*_next{nullptr};

	/**
	 * Fetch a control value.
	 *
	 * @param handle		Token passed when the callback is registered.
	 * @param control_group		The group to fetch the control from.
	 * @param control_index		The group-relative index to fetch the control from.
	 * @param control		The returned control
	 * @return			Zero if the value was fetched, nonzero otherwise.
	 */
	typedef int	(* ControlCallback)(uintptr_t handle, uint8_t control_group, uint8_t control_index, float &control);

	/**
	 * Constructor.
	 *
	 * @param control_cb		Callback invoked when reading controls.
	 */
	Mixer(ControlCallback control_cb, uintptr_t cb_handle);
	virtual ~Mixer() = default;

	/**
	 * Perform the mixing function.
	 *
	 * @param outputs		Array into which mixed output(s) should be placed.
	 * @param space			The number of available entries in the output array;
	 * @return			The number of entries in the output array that were populated.
	 */
	virtual unsigned		mix(float *outputs, unsigned space) = 0;

	/**
	 * Get the saturation status.
	 *
	 * @return			Integer bitmask containing saturation_status from multirotor_motor_limits.msg.
	 */
	virtual uint16_t		get_saturation_status() { return 0; }

	/**
	 * Analyses the mix configuration and updates a bitmask of groups
	 * that are required.
	 *
	 * @param groups		A bitmask of groups (0-31) that the mixer requires.
	 */
	virtual void			groups_required(uint32_t &groups) {};

	/**
	 * @brief      Empty method, only implemented for MultirotorMixer and MixerGroup class.
	 *
	 * @param[in]  delta_out_max  Maximum delta output.
	 *
	 */
	virtual void 			set_max_delta_out_once(float delta_out_max) {}

	/**
	 * @brief Set trim offset for this mixer
	 *
	 * @return the number of outputs this mixer feeds to
	 */
	virtual unsigned		set_trim(float trim) { return 0; }

	/**
	 * @brief Get trim offset for this mixer
	 *
	 * @return the number of outputs this mixer feeds to
	 */
	virtual unsigned		get_trim(float *trim) { return 0; }

	/*
	 * @brief      Sets the thrust factor used to calculate mapping from desired thrust to motor control signal output.
	 *
	 * @param[in]  val   The value
	 */
	virtual void 			set_thrust_factor(float val) {}

	/**
	 * @brief Set airmode. Airmode allows the mixer to increase the total thrust in order to unsaturate the motors.
	 *
	 * @param[in]  airmode   Select airmode type (0 = disabled, 1 = roll/pitch, 2 = roll/pitch/yaw)
	 */
	virtual void			set_airmode(Airmode airmode) {};

	virtual unsigned		get_multirotor_count()  { return 0; }

protected:
	/** client-supplied callback used when fetching control values */
	ControlCallback			_control_cb;
	uintptr_t			_cb_handle;

	/**
	 * Invoke the client callback to fetch a control value.
	 *
	 * @param group			Control group to fetch from.
	 * @param index			Control index to fetch.
	 * @return			The control value.
	 */
	float				get_control(uint8_t group, uint8_t index);

	/**
	 * Perform simpler linear scaling.
	 *
	 * @param scaler		The scaler configuration.
	 * @param input			The value to be scaled.
	 * @return			The scaled value.
	 */
	static float			scale(const mixer_scaler_s &scaler, float input);

	/**
	 * Validate a scaler
	 *
	 * @param scaler		The scaler to be validated.
	 * @return			Zero if good, nonzero otherwise.
	 */
	static int			scale_check(struct mixer_scaler_s &scaler);

	/**
	 * Find a tag
	 *
	 * @param buf			The buffer to operate on.
	 * @param buflen		length of the buffer.
	 * @param tag			character to search for.
	 */
	static const char 		*findtag(const char *buf, unsigned &buflen, char tag);

	/**
	 * Find next tag and return it (0 is returned if no tag is found)
	 *
	 * @param buf			The buffer to operate on.
	 * @param buflen		length of the buffer.
	 */
	static char 			findnexttag(const char *buf, unsigned buflen);

	/**
	 * Skip a line
	 *
	 * @param buf			The buffer to operate on.
	 * @param buflen		length of the buffer.
	 * @return			0 / OK if a line could be skipped, 1 else
	 */
	static const char 		*skipline(const char *buf, unsigned &buflen);

	/**
	 * Check wether the string is well formed and suitable for parsing
	 */
	static bool			string_well_formed(const char *buf, unsigned &buflen);
};
