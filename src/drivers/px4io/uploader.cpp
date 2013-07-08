/****************************************************************************
 *
 *   Copyright (c) 2012, 2013 PX4 Development Team. All rights reserved.
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
 * @file uploader.cpp
 * Firmware uploader for PX4IO
 */

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <sys/stat.h>

#include <crc32.h>

#include "uploader.h"

#ifdef CONFIG_ARCH_BOARD_PX4FMU_V2
#include <drivers/boards/px4fmuv2/px4fmu_internal.h>
#endif
#ifdef CONFIG_ARCH_BOARD_PX4FMU_V1
#include <drivers/boards/px4fmu/px4fmu_internal.h>
#endif

// define for comms logging
//#define UDEBUG

PX4IO_Uploader::PX4IO_Uploader() :
	_io_fd(-1),
	_fw_fd(-1)
{
}

PX4IO_Uploader::~PX4IO_Uploader()
{
}

int
PX4IO_Uploader::upload(const char *filenames[])
{
	int	ret;
	const char *filename = NULL;
	size_t fw_size;

#ifndef PX4IO_SERIAL_DEVICE
#error Must define PX4IO_SERIAL_DEVICE in board configuration to support firmware upload
#endif

	_io_fd = open(PX4IO_SERIAL_DEVICE, O_RDWR);

	if (_io_fd < 0) {
		log("could not open interface");
		return -errno;
	}

	/* adjust line speed to match bootloader */
	struct termios t;
	tcgetattr(_io_fd, &t);
	cfsetspeed(&t, 115200);
	tcsetattr(_io_fd, TCSANOW, &t);

	/* look for the bootloader */
	ret = sync();

	if (ret != OK) {
		/* this is immediately fatal */
		log("bootloader not responding");
		close(_io_fd);
		_io_fd = -1;
		return -EIO;
	}

	for (unsigned i = 0; filenames[i] != nullptr; i++) {
		_fw_fd = open(filenames[i], O_RDONLY);

		if (_fw_fd < 0) {
			log("failed to open %s", filenames[i]);
			continue;
		}

		log("using firmware from %s", filenames[i]);
		filename = filenames[i];
		break;
	}

	if (filename == NULL) {
		log("no firmware found");
		close(_io_fd);
		_io_fd = -1;
		return -ENOENT;
	}

	struct stat st;
	if (stat(filename, &st) != 0) {
		log("Failed to stat %s - %d\n", filename, (int)errno);
		close(_io_fd);
		_io_fd = -1;
		return -errno;
	}
	fw_size = st.st_size;

	if (_fw_fd == -1) {
		close(_io_fd);
		_io_fd = -1;
		return -ENOENT;
	}

	/* do the usual program thing - allow for failure */
	for (unsigned retries = 0; retries < 1; retries++) {
		if (retries > 0) {
			log("retrying update...");
			ret = sync();

			if (ret != OK) {
				/* this is immediately fatal */
				log("bootloader not responding");
				close(_io_fd);
				_io_fd = -1;
				return -EIO;
			}
		}

		ret = get_info(INFO_BL_REV, bl_rev);

		if (ret == OK) {
			if (bl_rev <= BL_REV) {
				log("found bootloader revision: %d", bl_rev);
			} else {
				log("found unsupported bootloader revision %d, exiting", bl_rev);
				close(_io_fd);
				_io_fd = -1;
				return OK;
			}
		}

		ret = erase();

		if (ret != OK) {
			log("erase failed");
			continue;
		}

		ret = program(fw_size);

		if (ret != OK) {
			log("program failed");
			continue;
		}

		if (bl_rev <= 2)
			ret = verify_rev2(fw_size);
		else if(bl_rev == 3) {
			ret = verify_rev3(fw_size);
		}

		if (ret != OK) {
			log("verify failed");
			continue;
		}

		ret = reboot();

		if (ret != OK) {
			log("reboot failed");
			return ret;
		}

		log("update complete");

		ret = OK;
		break;
	}

	close(_fw_fd);
	close(_io_fd);
	_io_fd = -1;
	return ret;
}

int
PX4IO_Uploader::recv(uint8_t &c, unsigned timeout)
{
	struct pollfd fds[1];

	fds[0].fd = _io_fd;
	fds[0].events = POLLIN;

	/* wait 100 ms for a character */
	int ret = ::poll(&fds[0], 1, timeout);

	if (ret < 1) {
#ifdef UDEBUG
		log("poll timeout %d", ret);
#endif
		return -ETIMEDOUT;
	}

	read(_io_fd, &c, 1);
#ifdef UDEBUG
	log("recv 0x%02x", c);
#endif
	return OK;
}

int
PX4IO_Uploader::recv(uint8_t *p, unsigned count)
{
	while (count--) {
		int ret = recv(*p++, 5000);

		if (ret != OK)
			return ret;
	}

	return OK;
}

void
PX4IO_Uploader::drain()
{
	uint8_t c;
	int ret;

	do {
		ret = recv(c, 1000);

#ifdef UDEBUG
		if (ret == OK) {
			log("discard 0x%02x", c);
		}
#endif
	} while (ret == OK);
}

int
PX4IO_Uploader::send(uint8_t c)
{
#ifdef UDEBUG
	log("send 0x%02x", c);
#endif
	if (write(_io_fd, &c, 1) != 1)
		return -errno;

	return OK;
}

int
PX4IO_Uploader::send(uint8_t *p, unsigned count)
{
	while (count--) {
		int ret = send(*p++);

		if (ret != OK)
			return ret;
	}

	return OK;
}

int
PX4IO_Uploader::get_sync(unsigned timeout)
{
	uint8_t c[2];
	int ret;

	ret = recv(c[0], timeout);

	if (ret != OK)
		return ret;

	ret = recv(c[1], timeout);

	if (ret != OK)
		return ret;

	if ((c[0] != PROTO_INSYNC) || (c[1] != PROTO_OK)) {
		log("bad sync 0x%02x,0x%02x", c[0], c[1]);
		return -EIO;
	}

	return OK;
}

int
PX4IO_Uploader::sync()
{
	drain();

	/* complete any pending program operation */
	for (unsigned i = 0; i < (PROG_MULTI_MAX + 6); i++)
		send(0);

	send(PROTO_GET_SYNC);
	send(PROTO_EOC);
	return get_sync();
}

int
PX4IO_Uploader::get_info(int param, uint32_t &val)
{
	int ret;

	send(PROTO_GET_DEVICE);
	send(param);
	send(PROTO_EOC);

	ret = recv((uint8_t *)&val, sizeof(val));

	if (ret != OK)
		return ret;

	return get_sync();
}

int
PX4IO_Uploader::erase()
{
	log("erase...");
	send(PROTO_CHIP_ERASE);
	send(PROTO_EOC);
	return get_sync(10000);		/* allow 10s timeout */
}


static int read_with_retry(int fd, void *buf, size_t n)
{
	int ret;
	uint8_t retries = 0;
	do {
		ret = read(fd, buf, n);
	} while (ret == -1 && retries++ < 100);
	if (retries != 0) {
		printf("read of %u bytes needed %u retries\n",
		       (unsigned)n,
		       (unsigned)retries);
	}
	return ret;
}

int
PX4IO_Uploader::program(size_t fw_size)
{
	uint8_t	file_buf[PROG_MULTI_MAX];
	ssize_t count;
	int ret;
	size_t sent = 0;

	log("programming %u bytes...", (unsigned)fw_size);

	ret = lseek(_fw_fd, 0, SEEK_SET);

	while (sent < fw_size) {
		/* get more bytes to program */
		size_t n = fw_size - sent;
		if (n > sizeof(file_buf)) {
			n = sizeof(file_buf);
		}
		count = read_with_retry(_fw_fd, file_buf, n);

		if (count != (ssize_t)n) {
			log("firmware read of %u bytes at %u failed -> %d errno %d", 
			    (unsigned)n,
			    (unsigned)sent,
			    (int)count,
			    (int)errno);
		}

		if (count == 0)
			return OK;

		sent += count;

		if (count < 0)
			return -errno;

		ASSERT((count % 4) == 0);

		send(PROTO_PROG_MULTI);
		send(count);
		send(&file_buf[0], count);
		send(PROTO_EOC);

		ret = get_sync(1000);

		if (ret != OK)
			return ret;
	}
	return OK;
}

int
PX4IO_Uploader::verify_rev2(size_t fw_size)
{
	uint8_t	file_buf[4];
	ssize_t count;
	int ret;
	size_t sent = 0;

	log("verify...");
	lseek(_fw_fd, 0, SEEK_SET);

	send(PROTO_CHIP_VERIFY);
	send(PROTO_EOC);
	ret = get_sync();

	if (ret != OK)
		return ret;

	while (sent < fw_size) {
		/* get more bytes to verify */
		size_t n = fw_size - sent;
		if (n > sizeof(file_buf)) {
			n = sizeof(file_buf);
		}
		count = read_with_retry(_fw_fd, file_buf, n);

		if (count != (ssize_t)n) {
			log("firmware read of %u bytes at %u failed -> %d errno %d", 
			    (unsigned)n,
			    (unsigned)sent,
			    (int)count,
			    (int)errno);
		}

		if (count == 0)
			break;

		sent += count;

		if (count < 0)
			return -errno;

		ASSERT((count % 4) == 0);

		send(PROTO_READ_MULTI);
		send(count);
		send(PROTO_EOC);

		for (ssize_t i = 0; i < count; i++) {
			uint8_t c;

			ret = recv(c, 5000);

			if (ret != OK) {
				log("%d: got %d waiting for bytes", sent + i, ret);
				return ret;
			}

			if (c != file_buf[i]) {
				log("%d: got 0x%02x expected 0x%02x", sent + i, c, file_buf[i]);
				return -EINVAL;
			}
		}

		ret = get_sync();

		if (ret != OK) {
			log("timeout waiting for post-verify sync");
			return ret;
		}
	}

	return OK;
}

int
PX4IO_Uploader::verify_rev3(size_t fw_size_local)
{
	int ret;
	uint8_t	file_buf[4];
	ssize_t count;
	uint32_t sum = 0;
	uint32_t bytes_read = 0;
	uint32_t crc = 0;
	uint32_t fw_size_remote;
	uint8_t fill_blank = 0xff;

	log("verify...");
	lseek(_fw_fd, 0, SEEK_SET);

	ret = get_info(INFO_FLASH_SIZE, fw_size_remote);
	send(PROTO_EOC);

	if (ret != OK) {
		log("could not read firmware size");
		return ret;
	}

	/* read through the firmware file again and calculate the checksum*/
	while (bytes_read < fw_size_local) {
		size_t n = fw_size_local - bytes_read;
		if (n > sizeof(file_buf)) {
			n = sizeof(file_buf);
		}
		count = read_with_retry(_fw_fd, file_buf, n);

		if (count != (ssize_t)n) {
			log("firmware read of %u bytes at %u failed -> %d errno %d", 
			    (unsigned)n,
			    (unsigned)bytes_read,
			    (int)count,
			    (int)errno);
		}

		/* set the rest to ff */
		if (count == 0) {
			break;
		}
		/* stop if the file cannot be read */
		if (count < 0)
			return -errno;

		/* calculate crc32 sum */
		sum = crc32part((uint8_t *)&file_buf, sizeof(file_buf), sum);

		bytes_read += count;
	}

	/* fill the rest with 0xff */
	while (bytes_read < fw_size_remote) {
		sum = crc32part(&fill_blank, sizeof(fill_blank), sum);
		bytes_read += sizeof(fill_blank);
	}

	/* request CRC from IO */
	send(PROTO_GET_CRC);
	send(PROTO_EOC);

	ret = recv((uint8_t*)(&crc), sizeof(crc));

	if (ret != OK) {
		log("did not receive CRC checksum");
		return ret;
	}

	/* compare the CRC sum from the IO with the one calculated */
	if (sum != crc) {
		log("CRC wrong: received: %d, expected: %d", crc, sum);
		return -EINVAL;
	}

	return OK;
}

int
PX4IO_Uploader::reboot()
{
	send(PROTO_REBOOT);
	send(PROTO_EOC);

	return OK;
}

void
PX4IO_Uploader::log(const char *fmt, ...)
{
	va_list	ap;

	printf("[PX4IO] ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
	fflush(stdout);
}
