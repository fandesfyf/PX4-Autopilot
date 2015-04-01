/****************************************************************************
 *
 * Copyright (c) 2015 Mark Charlebois. All rights reserved.
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
 * @file vcdev_posix.cpp
 *
 * POSIX-like API for virtual character device
 */

#include "px4_posix.h"
#include "device.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

using namespace device;

extern "C" {

struct timerData {
	sem_t &sem;
	struct timespec &ts;
 
	timerData(sem_t &s, struct timespec &t) : sem(s), ts(t) {}
	~timerData() {}
};

static void *timer_handler(void *data)
{
	struct timerData *td = (struct timerData *)data;

	if (td->ts.tv_sec) {
		sleep(td->ts.tv_sec);
	}
	usleep(td->ts.tv_nsec/1000);
	sem_post(&(td->sem));

	PX4_DEBUG("timer_handler: Timer expired\n");
	return 0;
}

#define PX4_MAX_FD 100
static px4_dev_handle_t *filemap[PX4_MAX_FD] = {};

int px4_errno;

inline bool valid_fd(int fd)
{
	return (fd < PX4_MAX_FD && fd >= 0 && filemap[fd] != NULL);
}

int px4_open(const char *path, int flags)
{
	VDev *dev = VDev::getDev(path);
	int ret = 0;
	int i;

	if (!dev) {
		ret = -EINVAL;
	}
	else {
		for (i=0; i<PX4_MAX_FD; ++i) {
			if (filemap[i] == 0) {
				filemap[i] = new px4_dev_handle_t(flags,dev,i);
				break;
			}
		}
		if (i < PX4_MAX_FD) {
			ret = dev->open(filemap[i]);
		}
		else {
			ret = -ENOENT;
		}
	}
	if (ret < 0) {
		px4_errno = -ret;
		return -1;
	}
	PX4_DEBUG("px4_open fd = %d", filemap[i]->fd);
	return filemap[i]->fd;
}

int px4_close(int fd)
{
	int ret;
	if (valid_fd(fd)) {
		VDev *dev = (VDev *)(filemap[fd]->cdev);
		PX4_DEBUG("px4_close fd = %d\n", fd);
		ret = dev->close(filemap[fd]);
		filemap[fd] = NULL;
	}
	else { 
                ret = -EINVAL;
        }
	if (ret < 0) {
		px4_errno = -ret;
	}
	return ret;
}

ssize_t px4_read(int fd, void *buffer, size_t buflen)
{
	int ret;
	if (valid_fd(fd)) {
		VDev *dev = (VDev *)(filemap[fd]->cdev);
		PX4_DEBUG("px4_read fd = %d\n", fd);
		ret = dev->read(filemap[fd], (char *)buffer, buflen);
	}
	else { 
                ret = -EINVAL;
        }
	if (ret < 0) {
		px4_errno = -ret;
	}
	return ret;
}

ssize_t px4_write(int fd, const void *buffer, size_t buflen)
{
	int ret = PX4_ERROR;
        if (valid_fd(fd)) {
		VDev *dev = (VDev *)(filemap[fd]->cdev);
		PX4_DEBUG("px4_write fd = %d\n", fd);
		ret = dev->write(filemap[fd], (const char *)buffer, buflen);
	}
	else { 
                ret = -EINVAL;
        }
	if (ret < 0) {
		px4_errno = -ret;
	}
        return ret;
}

int px4_ioctl(int fd, int cmd, unsigned long arg)
{
	int ret = PX4_ERROR;
        if (valid_fd(fd)) {
		VDev *dev = (VDev *)(filemap[fd]->cdev);
		PX4_DEBUG("px4_ioctl fd = %d\n", fd);
		ret = dev->ioctl(filemap[fd], cmd, arg);
	}
	else { 
                ret = -EINVAL;
        }
	if (ret < 0) {
		px4_errno = -ret;
	}
	else { 
                px4_errno = -EINVAL;
        }
        return ret;
}

int px4_poll(px4_pollfd_struct_t *fds, nfds_t nfds, int timeout)
{
	sem_t sem;
	int count = 0;
	int ret;
	unsigned int i;
	struct timespec ts;

	PX4_DEBUG("Called px4_poll timeout = %d\n", timeout);
	sem_init(&sem, 0, 0);

	// For each fd 
	for (i=0; i<nfds; ++i)
	{
		fds[i].sem     = &sem;
		fds[i].revents = 0;
		fds[i].priv    = NULL;

		// If fd is valid
		if (valid_fd(fds[i].fd))
		{
			VDev *dev = (VDev *)(filemap[fds[i].fd]->cdev);;
			PX4_DEBUG("px4_poll: VDev->poll(setup) %d\n", fds[i].fd);
			ret = dev->poll(filemap[fds[i].fd], &fds[i], true);

			if (ret < 0)
				break;
		}
	}

	if (ret >= 0)
	{
		if (timeout >= 0)
		{
			pthread_t pt;
			void *res;

			ts.tv_sec = timeout/1000;
			ts.tv_nsec = (timeout % 1000)*1000;

			// Create a timer to unblock
			struct timerData td(sem, ts);
			int rv = pthread_create(&pt, NULL, timer_handler, (void *)&td);
			if (rv != 0) {
				count = -1;
				goto cleanup;
			}
			sem_wait(&sem);

			// Make sure timer thread is killed before sem goes
			// out of scope
			(void)pthread_cancel(pt);
			(void)pthread_join(pt, &res);
        	}
		else
		{
			sem_wait(&sem);
		}

		// For each fd 
		for (i=0; i<nfds; ++i)
		{
			// If fd is valid
			if (valid_fd(fds[i].fd))
			{
				VDev *dev = (VDev *)(filemap[fds[i].fd]->cdev);;
				PX4_DEBUG("px4_poll: VDev->poll(teardown) %d\n", fds[i].fd);
				ret = dev->poll(filemap[fds[i].fd], &fds[i], false);
	
				if (ret < 0)
					break;

				if (fds[i].revents)
				count += 1;
			}
		}
	}

cleanup:
	sem_destroy(&sem);

	return count;
}

}

