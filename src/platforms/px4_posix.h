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

/**
 * @file px4_posix.h
 *
 * Includes POSIX-like functions for virtual character devices
 */

#pragma once

#include <px4_defines.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>
#include <semaphore.h>

#define  PX4_F_RDONLY 1
#define  PX4_F_WRONLY 2

#ifdef __PX4_NUTTX

typedef struct pollfd px4_pollfd_struct_t;

#if defined(__cplusplus)
#define _GLOBAL ::
#else
#define _GLOBAL 
#endif
#define px4_open 	_GLOBAL open
#define px4_close 	_GLOBAL close
#define px4_ioctl 	_GLOBAL ioctl
#define px4_write 	_GLOBAL write
#define px4_read 	_GLOBAL read
#define px4_poll 	_GLOBAL poll

#elif defined(__PX4_POSIX)

#define  PX4_F_RDONLY 1
#define  PX4_F_WRONLY 2

typedef short pollevent_t;

typedef struct {
  /* This part of the struct is POSIX-like */
  int		fd;       /* The descriptor being polled */
  pollevent_t 	events;   /* The input event flags */
  pollevent_t 	revents;  /* The output event flags */

  /* Required for PX4 compatability */
  sem_t   *sem;  	/* Pointer to semaphore used to post output event */
  void   *priv;     	/* For use by drivers */
} px4_pollfd_struct_t;

__BEGIN_DECLS

__EXPORT int 		px4_open(const char *path, int flags);
__EXPORT int 		px4_close(int fd);
__EXPORT ssize_t	px4_read(int fd, void *buffer, size_t buflen);
__EXPORT ssize_t	px4_write(int fd, const void *buffer, size_t buflen);
__EXPORT int		px4_ioctl(int fd, int cmd, unsigned long arg);
__EXPORT int		px4_poll(px4_pollfd_struct_t *fds, nfds_t nfds, int timeout);

__END_DECLS
#else
#error "No TARGET OS Provided"
#endif

__BEGIN_DECLS
extern int px4_errno;

__EXPORT void		px4_show_devices(void);
__EXPORT const char *	px4_get_device_names(unsigned int *handle);

__EXPORT void		px4_show_topics(void);
__EXPORT const char *	px4_get_topic_names(unsigned int *handle);
__END_DECLS
