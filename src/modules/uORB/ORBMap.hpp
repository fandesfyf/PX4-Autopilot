/****************************************************************************
 *
 *   Copyright (c) 2015 Mark Charlebois. All rights reserved.
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

#include <string.h>
#include <stdlib.h>


namespace uORB
{
class DeviceNode;
class ORBMap;
}

class uORB::ORBMap
{
public:
	struct Node {
		struct Node *next;
		const char *node_name;
		uORB::DeviceNode *node;
	};

	ORBMap() :
		_top(nullptr),
		_end(nullptr)
	{ }
	~ORBMap()
	{
		while (_top != nullptr) {
			unlinkNext(_top);

			if (_top->next == nullptr) {
				free((void *)_top->node_name);
				free(_top);
				_top = nullptr;
				_end = nullptr;
			}
		}
	}
	void insert(const char *node_name, uORB::DeviceNode *node)
	{
		Node **p;

		if (_top == nullptr) {
			p = &_top;

		} else {
			p = &_end->next;
		}

		*p = (Node *)malloc(sizeof(Node));

		if (_end) {
			_end = _end->next;

		} else {
			_end = _top;
		}

		_end->next = nullptr;
		_end->node_name = strdup(node_name);
		_end->node = node;
	}

	bool find(const char *node_name)
	{
		Node *p = _top;

		while (p) {
			if (strcmp(p->node_name, node_name) == 0) {
				return true;
			}

			p = p->next;
		}

		return false;
	}

	uORB::DeviceNode *get(const char *node_name)
	{
		Node *p = _top;

		while (p) {
			if (strcmp(p->node_name, node_name) == 0) {
				return p->node;
			}

			p = p->next;
		}

		return nullptr;
	}

	void unlinkNext(Node *a)
	{
		Node *b = a->next;

		if (b != nullptr) {
			if (_end == b) {
				_end = a;
			}

			a->next = b->next;
			free((void *)b->node_name);
			free(b);
		}
	}

	Node *top() const
	{
		return _top;
	}

private:
	Node *_top;
	Node *_end;
};

