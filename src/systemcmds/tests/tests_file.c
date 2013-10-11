/****************************************************************************
 *
 *   Copyright (C) 2012 PX4 Development Team. All rights reserved.
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
 * @file tests_file.c
 *
 * File write test.
 */

#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <systemlib/err.h>
#include <systemlib/perf_counter.h>
#include <string.h>

#include <drivers/drv_hrt.h>

#include "tests.h"

int
test_file(int argc, char *argv[])
{
	const iterations = 200;

	/* check if microSD card is mounted */
	struct stat buffer;
	if (stat("/fs/microsd/", &buffer)) {
		warnx("no microSD card mounted, aborting file test");
		return 1;
	}

	uint8_t write_buf[512 + 64] __attribute__((aligned(64)));

	/* fill write buffer with known values */
	for (int i = 0; i < sizeof(write_buf); i++) {
		/* this will wrap, but we just need a known value with spacing */
		write_buf[i] = i+11;
	}

	uint8_t read_buf[512 + 64] __attribute__((aligned(64)));
	hrt_abstime start, end;
	perf_counter_t wperf = perf_alloc(PC_ELAPSED, "SD writes (aligned)");

	int fd = open("/fs/microsd/testfile", O_TRUNC | O_WRONLY | O_CREAT);

	warnx("testing unaligned writes - please wait..");

	start = hrt_absolute_time();
	for (unsigned i = 0; i < iterations; i++) {
		perf_begin(wperf);
		int wret = write(fd, write_buf + 1/*+ (i % 64)*/, 512);

		if (wret != 512) {
			warn("WRITE ERROR!");

			if ((0x3 & (uintptr_t)(write_buf + 1 /* (i % 64)*/)))
				warnx("memory is unaligned, align shift: %d", 1/*(i % 64)*/);

		}

		fsync(fd);
		perf_end(wperf);

	}
	end = hrt_absolute_time();

	warnx("%dKiB in %llu microseconds", iterations / 2, end - start);

	perf_print_counter(wperf);
	perf_free(wperf);

	close(fd);
	fd = open("/fs/microsd/testfile", O_RDONLY);

	/* read back data for validation */
	for (unsigned i = 0; i < iterations; i++) {
		int rret = read(fd, read_buf, 512);

		if (rret != 512) {
			warn("READ ERROR!");
			break;
		}
		
		/* compare value */
		bool compare_ok = true;

		for (int j = 0; j < 512; j++) {
			if (read_buf[j] != write_buf[j + 1/*+ (i % 64)*/]) {
				warnx("COMPARISON ERROR: byte %d, align shift: %d", j, 1/*(i % 64)*/);
				compare_ok = false;
				break;
			}
		}

		if (!compare_ok) {
			warnx("ABORTING FURTHER COMPARISON DUE TO ERROR");
			break;
		}

	}

	/*
	 * ALIGNED WRITES AND UNALIGNED READS
	 */

	int ret = unlink("/fs/microsd/testfile");
	fd = open("/fs/microsd/testfile", O_TRUNC | O_WRONLY | O_CREAT);

	warnx("testing aligned writes - please wait..");

	start = hrt_absolute_time();
	for (unsigned i = 0; i < iterations; i++) {
		perf_begin(wperf);
		int wret = write(fd, write_buf, 512);

		if (wret != 512) {
			warn("WRITE ERROR!");
		}

		perf_end(wperf);

	}

	fsync(fd);

	warnx("reading data aligned..");

	close(fd);
	fd = open("/fs/microsd/testfile", O_RDONLY);

	bool align_read_ok = true;

	/* read back data unaligned */
	for (unsigned i = 0; i < iterations; i++) {
		int rret = read(fd, read_buf, 512);

		if (rret != 512) {
			warn("READ ERROR!");
			break;
		}
		
		/* compare value */
		bool compare_ok = true;

		for (int j = 0; j < 512; j++) {
			if (read_buf[j] != write_buf[j]) {
				warnx("COMPARISON ERROR: byte %d: %u != %u", j, (unsigned int)read_buf[j], (unsigned int)write_buf[j]);
				align_read_ok = false;
				break;
			}
		}

		if (!align_read_ok) {
			warnx("ABORTING FURTHER COMPARISON DUE TO ERROR");
			break;
		}

	}

	warnx("align read result: %s\n", (align_read_ok) ? "OK" : "ERROR");

	warnx("reading data unaligned..");

	close(fd);
	fd = open("/fs/microsd/testfile", O_RDONLY);

	bool unalign_read_ok = true;
	int unalign_read_err_count = 0;

	memset(read_buf, 0, sizeof(read_buf));
	read_buf[0] = 0;

	warnx("printing first 10 pairs:");

	uint8_t *read_ptr = &read_buf[1];

	/* read back data unaligned */
	for (unsigned i = 0; i < iterations; i++) {
		int rret = read(fd, read_ptr, 512);

		warnx("first byte: %u (should be zero)", (unsigned int)read_buf[0]);

		if (rret != 512) {
			warn("READ ERROR!");
			break;
		}

		for (int j = 0; j < 512; j++) {

			if (i == 0 && j < 10) {
				warnx("read: %u, expect: %u", (unsigned int)(read_buf + 1)[j], (unsigned int)write_buf[j]);
			}

			if ((read_buf + 1)[j] != write_buf[j]) {
				warnx("COMPARISON ERROR: byte %d, align shift: %d: %u != %u", j, 1/*(i % 64)*/, (unsigned int)read_ptr[j], (unsigned int)write_buf[j]);
				unalign_read_ok = false;
				unalign_read_err_count++;
				
				if (unalign_read_err_count > 10)
					break;
			}
		}

		if (!unalign_read_ok) {
			warnx("ABORTING FURTHER COMPARISON DUE TO ERROR");
			break;
		}

	}

	ret = unlink("/fs/microsd/testfile");
	close(fd);

	if (ret)
		err(1, "UNLINKING FILE FAILED");

	/* list directory */
	DIR           *d;
	struct dirent *dir;
	d = opendir("/fs/microsd");
	if (d) {

		while ((dir = readdir(d)) != NULL) {
			//printf("%s\n", dir->d_name);
		}

		closedir(d);

		warnx("directory listing ok (FS mounted and readable)");

	} else {
		/* failed opening dir */
		err(1, "FAILED LISTING MICROSD ROOT DIRECTORY");
	}

	return 0;
}
#if 0
int
test_file(int argc, char *argv[])
{
	const iterations = 1024;

	/* check if microSD card is mounted */
	struct stat buffer;
	if (stat("/fs/microsd/", &buffer)) {
		warnx("no microSD card mounted, aborting file test");
		return 1;
	}

	uint8_t buf[512];
	hrt_abstime start, end;
	perf_counter_t wperf = perf_alloc(PC_ELAPSED, "SD writes");

	int fd = open("/fs/microsd/testfile", O_TRUNC | O_WRONLY | O_CREAT);
	memset(buf, 0, sizeof(buf));

	start = hrt_absolute_time();
	for (unsigned i = 0; i < iterations; i++) {
		perf_begin(wperf);
		write(fd, buf, sizeof(buf));
		perf_end(wperf);
	}
	end = hrt_absolute_time();

	close(fd);

	unlink("/fs/microsd/testfile");

	warnx("%dKiB in %llu microseconds", iterations / 2, end - start);
	perf_print_counter(wperf);
	perf_free(wperf);

	warnx("running unlink test");

	/* ensure that common commands do not run against file count limits */
	for (unsigned i = 0; i < 64; i++) {

		warnx("unlink iteration #%u", i);

		int fd = open("/fs/microsd/testfile", O_TRUNC | O_WRONLY | O_CREAT);
		if (fd < 0)
			errx(1, "failed opening test file before unlink()");
		int ret = write(fd, buf, sizeof(buf));
		if (ret < 0)
			errx(1, "failed writing test file before unlink()");
		close(fd);

		ret = unlink("/fs/microsd/testfile");
		if (ret != OK)
			errx(1, "failed unlinking test file");

		fd = open("/fs/microsd/testfile", O_TRUNC | O_WRONLY | O_CREAT);
		if (fd < 0)
			errx(1, "failed opening test file after unlink()");
		ret = write(fd, buf, sizeof(buf));
		if (ret < 0)
			errx(1, "failed writing test file after unlink()");
		close(fd);
	}

	return 0;
}
#endif
