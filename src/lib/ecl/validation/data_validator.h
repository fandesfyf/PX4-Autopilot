/****************************************************************************
 *
 *   Copyright (c) 2015 PX4 Development Team. All rights reserved.
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
 * @file data_validator.h
 *
 * A data validation class to identify anomalies in data streams
 *
 * @author Lorenz Meier <lorenz@px4.io>
 */

#pragma once

#include <cmath>

class DataValidator {
public:
	DataValidator(DataValidator *prev_sibling = nullptr);
	virtual ~DataValidator();

	/**
	 * Put an item into the validator.
	 *
	 * @param val		Item to put
	 */
	void			put(uint64_t timestamp, float val[3], uint64_t error_count);

	/**
	 * Get the next sibling in the group
	 *
	 * @return		the next sibling
	 */
	DataValidator*		sibling() { return _sibling; }

	/**
	 * Get the confidence of this validator
	 * @return		the confidence between 0 and 1
	 */
	float			confidence(uint64_t timestamp);

	/**
	 * Get the error count of this validator
	 * @return		the error count
	 */
	uint64_t		error_count() { return _error_count; }

	/**
	 * Get the values of this validator
	 * @return		the stored value
	 */
	float*			value() { return _value; }

	/**
	 * Get the RMS values of this validator
	 * @return		the stored RMS
	 */
	float*			rms() { return _rms; }

	/**
	 * Print the validator value
	 *
	 */
	void			print();

	/**
	 * Set the timeout value
	 *
	 * @param timeout_interval_us The timeout interval in microseconds
	 */
	void			set_timeout(uint64_t timeout_interval_us) { _timeout_interval = timeout_interval_us; }

private:
	static const unsigned _dimensions = 3;
	uint64_t _time_last;			/**< last timestamp */
	uint64_t _timeout_interval;		/**< interval in which the datastream times out in us */
	uint64_t _event_count;			/**< total data counter */
	uint64_t _error_count;			/**< error count */
	float _mean[_dimensions];				/**< mean of value */
	float _lp[3];				/**< low pass value */
	float _M2[3];				/**< RMS component value */
	float _rms[3];				/**< root mean square error */
	float _value[3];			/**< last value */
	float _value_equal_count;		/**< equal values in a row */
	DataValidator *_sibling;		/**< sibling in the group */
	const unsigned NORETURN_ERRCOUNT = 1000;	/**< if the error count reaches this value, return sensor as invalid */
	const unsigned VALUE_EQUAL_COUNT_MAX = 100;	/**< if the sensor value is the same (accumulated also between axes) this many times, flag it */

	/* we don't want this class to be copied */
	DataValidator(const DataValidator&);
	DataValidator operator=(const DataValidator&);
};
