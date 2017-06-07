/****************************************************************************
 *
 *   Copyright (C) 2012-2017 PX4 Development Team. All rights reserved.
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
 * @file BlockParam.h
 *
 * Controller library code
 */

#pragma once

#include <systemlib/param/param.h>

#include "Block.hpp"
#include <containers/List.hpp>

namespace control
{

class Block;

// A base class for block params that enables traversing linked list.
class BlockParamBase : public ListNode<BlockParamBase *>
{
public:
	/**
	 * Instantiate a block param base.
	 *
	 * @param parent_prefix Set to true to include the parent name in the parameter name
	 */
	BlockParamBase(Block *parent, const char *name, bool parent_prefix = true);
	virtual ~BlockParamBase() {};

	virtual void update() = 0;
	const char *getName() { return param_name(_handle); }

protected:
	param_t _handle{PARAM_INVALID};
};

// Parameters that are tied to blocks for updating and naming.
template <class T>
class __EXPORT BlockParam : public BlockParamBase
{
public:
	BlockParam(Block *block, const char *name, bool parent_prefix = true);
	BlockParam(Block *block, const char *name, bool parent_prefix, T &extern_val);

	~BlockParam() = default;

	// no copy, assignment, move, move assignment
	BlockParam(const BlockParam &) = delete;
	BlockParam &operator=(const BlockParam &) = delete;
	BlockParam(BlockParam &&) = delete;
	BlockParam &operator=(BlockParam &&) = delete;

	T get() const { return _val; }

	// Store the parameter value to the parameter storage (@see param_set())
	void commit() { param_set(_handle, &_val); };

	// Store the parameter value to the parameter storage, w/o notifying the system (@see param_set_no_notification())
	void commit_no_notification() { param_set_no_notification(_handle, &_val); };

	void set(T val) { _val = val; };
	void update() override { param_get(_handle, &_val); };

protected:
	T _val;
};

typedef BlockParam<float> BlockParamFloat;
typedef BlockParam<int32_t> BlockParamInt;
typedef BlockParam<float &> BlockParamExtFloat;
typedef BlockParam<int32_t &> BlockParamExtInt;

template class __EXPORT BlockParam<float>;
template class __EXPORT BlockParam<int32_t>;
template class __EXPORT BlockParam<float &>;
template class __EXPORT BlockParam<int32_t &>;

} // namespace control
