/****************************************************************************
 *
 *   Copyright (c) 2014 PX4 Development Team. All rights reserved.
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
 * @file px4_defines.h
 *
 * Generally used magic defines
 */

#pragma once
/* Get the name of the default value fiven the param name */
#define PX4_PARAM_DEFAULT_VALUE_NAME(_name) PARAM_##_name##_DEFAULT

/* Shortcuts to define parameters when the default value is defined according to PX4_PARAM_DEFAULT_VALUE_NAME */
#define PX4_PARAM_DEFINE_INT32(_name) PARAM_DEFINE_INT32(_name, PX4_PARAM_DEFAULT_VALUE_NAME(_name))
#define PX4_PARAM_DEFINE_FLOAT(_name) PARAM_DEFINE_FLOAT(_name, PX4_PARAM_DEFAULT_VALUE_NAME(_name))


#if defined(__linux) || (defined(__APPLE__) && defined(__MACH__))
/*
 * Building for running within the ROS environment
 */
#define __EXPORT
#include "ros/ros.h"
/* Main entry point */
#define PX4_MAIN_FUNCTION(_prefix) int main(int argc, char **argv)

/* Print/output wrappers */
#define PX4_WARN ROS_WARN
#define PX4_INFO ROS_INFO

/* Topic Handle */
#define PX4_TOPIC(_name) #_name

/* Topic type */
#define PX4_TOPIC_T(_name) _name

/* Subscribe and providing a class method as callback (do not use directly, use PX4_SUBSCRIBE instead) */
#define PX4_SUBSCRIBE_CBMETH(_nodehandle, _name, _cbf, _obj, _interval) _nodehandle.subscribe(PX4_TOPIC(_name), &_cbf, &_obj);
/* Subscribe and providing a function as callback (do not use directly, use PX4_SUBSCRIBE instead) */
#define PX4_SUBSCRIBE_CBFUNC(_nodehandle, _name, _cbf, _interval) _nodehandle.subscribe(PX4_TOPIC(_name), _cbf);

/* Parameter handle datatype */
typedef const char *px4_param_t;

/* Helper fucntions to set ROS params, only int and float supported */
static inline px4_param_t PX4_ROS_PARAM_SET(const char *name, int value)
{
	ros::param::set(name, value);
	return (px4_param_t)name;
};
static inline px4_param_t PX4_ROS_PARAM_SET(const char *name, float value)
{
	ros::param::set(name, value);
	return (px4_param_t)name;
};

/* Initialize a param, in case of ROS the parameter is sent to the parameter server here */
#define PX4_PARAM_INIT(_name) PX4_ROS_PARAM_SET(#_name, PX4_PARAM_DEFAULT_VALUE_NAME(_name))

/* Get value of parameter */
#define PX4_PARAM_GET(_handle, _destpt) ros::param::get(_handle, *_destpt)

#else
/*
 * Building for NuttX
 */
#include <platforms/px4_includes.h>
#ifdef __cplusplus
#include <functional>
#endif
/* Main entry point */
#define PX4_MAIN_FUNCTION(_prefix) int _prefix##_task_main(int argc, char *argv[])

/* Print/output wrappers */
#define PX4_WARN warnx
#define PX4_INFO warnx

/* Topic Handle */
#define PX4_TOPIC(_name) ORB_ID(_name)

/* Topic type */
#define PX4_TOPIC_T(_name) _name##_s

/* Subscribe and providing a class method as callback (do not use directly, use PX4_SUBSCRIBE instead) */
#define PX4_SUBSCRIBE_CBMETH(_nodehandle, _name, _cbf, _obj, _interval) _nodehandle.subscribe<PX4_TOPIC_T(_name)>(PX4_TOPIC(_name), std::bind(&_cbf, _obj, std::placeholders::_1), _interval)
/* Subscribe and providing a function as callback (do not use directly, use PX4_SUBSCRIBE instead) */
#define PX4_SUBSCRIBE_CBFUNC(_nodehandle, _name, _cbf, _interval) _nodehandle.subscribe<PX4_TOPIC_T(_name)>(PX4_TOPIC(_name), std::bind(&_cbf, std::placeholders::_1), _interval)
#define PX4_SUBSCRIBE_NOCB(_nodehandle, _name, _interval) _nodehandle.subscribe<PX4_TOPIC_T(_name)>(PX4_TOPIC(_name), nullptr, _interval)

/* Parameter handle datatype */
#include <systemlib/param/param.h>
typedef param_t px4_param_t;

/* Initialize a param, get param handle */
#define PX4_PARAM_INIT(_name) param_find(#_name)

/* Get value of parameter */
#define PX4_PARAM_GET(_handle, _destpt) param_get(_handle, _destpt)
#endif

/* Shortcut for subscribing to topics
 * Overload the PX4_SUBSCRIBE macro to suppport methods, pure functions as callback and no callback at all
 */
#define PX4_GET_SUBSCRIBE(_1, _2, _3, _4, _5, NAME, ...) NAME
#define PX4_SUBSCRIBE(...) PX4_GET_SUBSCRIBE(__VA_ARGS__, PX4_SUBSCRIBE_CBMETH, PX4_SUBSCRIBE_CBFUNC, PX4_SUBSCRIBE_NOCB)(__VA_ARGS__)

/* shortcut for advertising topics */
#define PX4_ADVERTISE(_nodehandle, _name) _nodehandle.advertise<PX4_TOPIC_T(_name)>(PX4_TOPIC(_name))

/* wrapper for 2d matrices */
#define PX4_ARRAY2D(_array, _ncols, _x, _y) (_array[_x * _ncols + _y])

/* wrapper for rotation matrices stored in arrays */
#define PX4_R(_array, _x, _y) PX4_ARRAY2D(_array, 3, _x, _y)

#define isspace(c) \
  ((c) == ' '  || (c) == '\t' || (c) == '\n' || \
   (c) == '\r' || (c) == '\f' || c== '\v')
