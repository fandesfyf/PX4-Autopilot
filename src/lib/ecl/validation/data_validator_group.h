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
 * @file data_validator_group.h
 *
 * A data validation group to identify anomalies in data streams
 *
 * @author Lorenz Meier <lorenz@px4.io>
 */

#pragma once

#include "data_validator.h"

class DataValidatorGroup {
public:
	DataValidatorGroup(unsigned siblings);
	virtual ~DataValidatorGroup();

	/**
	 * Put an item into the validator group.
	 *
	 * @param x		X Item to put
	 * @param y		Y Item to put
	 * @param z		Z Item to put
	 */
	void			put(unsigned index, uint64_t timestamp,
					float val[3], uint64_t error_count);

	/**
	 * Get the best data triplet of the group
	 *
	 * @return		pointer to the array of best values
	 */
	float*			get_best(uint64_t timestamp, int *index);

	/**
	 * Get the RMS / vibration factor
	 *
	 * @return		float value representing the RMS, which a valid indicator for vibration
	 */
	float			get_vibration_factor(uint64_t timestamp);

	/**
	 * Get the number of failover events
	 *
	 * @return		the number of failovers
	 */
	unsigned		failover_count();

	/**
	 * Print the validator value
	 *
	 */
	void			print();

private:
	DataValidator *_first;		/**< sibling in the group */
	int _curr_best;		/**< currently best index */
	int _prev_best;		/**< the previous best index */
	uint64_t _first_failover_time;	/**< timestamp where the first failover occured or zero if none occured */
	unsigned _toggle_count;		/**< number of back and forth switches between two sensors */

	/* we don't want this class to be copied */
	DataValidatorGroup(const DataValidatorGroup&);
	DataValidatorGroup operator=(const DataValidatorGroup&);
};

DataValidatorGroup::DataValidatorGroup(unsigned siblings) :
	_first(nullptr),
	_curr_best(-1),
	_prev_best(-1),
	_first_failover_time(0),
	_toggle_count(0)
{
	DataValidator *next = _first;

	for (unsigned i = 0; i < siblings; i++) {
		next = new DataValidator(next);
	}

	_first = next;
}

DataValidatorGroup::~DataValidatorGroup()
{

}

void
DataValidatorGroup::put(unsigned index, uint64_t timestamp, float val[3], uint64_t error_count)
{
	DataValidator *next = _first;
	unsigned i = 0;

	while (next != nullptr) {
		if (i == index) {
			next->put(timestamp, val, error_count);
			break;
		}
		next = next->sibling();
		i++;
	}
}

float*
DataValidatorGroup::get_best(uint64_t timestamp, int *index)
{
	DataValidator *next = _first;

	// XXX This should eventually also include voting
	int pre_check_best = _curr_best;
	float max_confidence = 0.0f;
	int max_index = -1;
	uint64_t min_error_count = 30000;
	DataValidator *best = nullptr;

	unsigned i = 0;

	while (next != nullptr) {
		float confidence = next->confidence(timestamp);
		if (confidence > max_confidence ||
			(fabsf(confidence - max_confidence) < 0.01f && next->error_count() < min_error_count)) {
			max_index = i;
			max_confidence = confidence;
			min_error_count = next->error_count();
			best = next;
		}

		next = next->sibling();
		i++;
	}

	/* the current best sensor is not matching the previous best sensor */
	if (max_index != _curr_best) {

		/* if we're no initialized, initialize the bookkeeping but do not count a failsafe */
		if (_curr_best < 0) {
			_prev_best = max_index;
		} else {
			/* we were initialized before, this is a real failsafe */
			_prev_best = pre_check_best;
			_toggle_count++;

			/* if this is the first time, log when we failed */
			if (_first_failover_time == 0) {
				_first_failover_time = timestamp;
			}
		}

		/* for all cases we want to keep a record of the best index */
		_curr_best = max_index;
	}
	*index = max_index;
	return (best) ? best->value() : nullptr;
}

float
DataValidatorGroup::get_vibration_factor(uint64_t timestamp)
{
	DataValidator *next = _first;

	float vibe = 0.0f;

	/* find the best RMS value of a non-timed out sensor */
	while (next != nullptr) {

		if (next->confidence(timestamp) > 0.5f) {
			float* rms = next->rms();

			for (unsigned j = 0; j < 3; j++) {
				if (rms[j] > vibe) {
					vibe = rms[j];
				}
			}
		}

		next = next->sibling();
	}

	return vibe;
}

void
DataValidatorGroup::print()
{
	/* print the group's state */
	warnx("validator: best: %d, prev best: %d, failsafe: %s (# %u)",
		_curr_best, _prev_best, (_toggle_count > 0) ? "YES" : "NO",
		_toggle_count);


	DataValidator *next = _first;
	unsigned i = 0;

	while (next != nullptr) {
		printf("sensor #%u:\n", i);
		next->print();
		next = next->sibling();
		i++;
	}
}

unsigned
DataValidatorGroup::failover_count()
{
	return _toggle_count;
}
