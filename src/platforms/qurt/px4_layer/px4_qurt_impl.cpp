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
 * @file px4_linux_impl.cpp
 *
 * PX4 Middleware Wrapper Linux Implementation
 */

#include <px4_defines.h>
#include <px4_tasks.h>
#include <px4_middleware.h>
#include <px4_workqueue.h>
#include <dataman/dataman.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include "systemlib/param/param.h"
#include "hrt_work.h"
#include "px4_log.h"


//extern pthread_t _shell_task_id;


__BEGIN_DECLS
extern uint64_t get_ticks_per_us();

//long PX4_TICKS_PER_SEC = 1000L;

unsigned int sleep(unsigned int sec)
{
	for (unsigned int i = 0; i < sec; i++) {
		usleep(1000000);
	}

	return 0;
}

extern void hrt_init(void);
extern void init_params();

#if 0
void qurt_log(const char *fmt, ...)
{
	va_list	args;
	va_start(args, fmt);
	printf(fmt, args);
	printf("n");
}
#endif

//extern int _posix_init(void);

__END_DECLS

extern struct wqueue_s gwork[NWORKERS];


namespace px4
{

void init_once(void);

void init_once(void)
{
	// Required for QuRT
	//_posix_init();
	PX4_WARN("Before calling work_queue_init");

//	_shell_task_id = pthread_self();
//	PX4_INFO("Shell id is %lu", _shell_task_id);

	work_queues_init();
	PX4_WARN("Before calling hrt_init");
	hrt_work_queue_init();
	hrt_init();
	PX4_WARN("after calling hrt_init");

	/* Shared memory param sync*/
	init_params();
}

void init(int argc, char *argv[], const char *app_name)
{
	PX4_DEBUG("App name: %s\n", app_name);
}

}

/** Retrieve from the data manager store */
ssize_t
dm_read(
	dm_item_t item,                 /* The item type to retrieve */
	unsigned char index,            /* The index of the item */
	void *buffer,                   /* Pointer to caller data buffer */
	size_t buflen                   /* Length in bytes of data to retrieve */
)
{
	return 0;
}

/** write to the data manager store */
ssize_t
dm_write(
	dm_item_t  item,                /* The item type to store */
	unsigned char index,            /* The index of the item */
	dm_persitence_t persistence,    /* The persistence level of this item */
	const void *buffer,             /* Pointer to caller data buffer */
	size_t buflen                   /* Length in bytes of data to retrieve */
)
{
	return 0;
}

size_t strnlen(const char *s, size_t maxlen)
{
	size_t i = 0;

	while (s[i] != '\0' && i < maxlen) {
		++i;
	}

	return i;
}

int write(int a, char const *b, int c)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}

int fprintf(FILE *stream, const char *format, ...)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return 0;
}

int fputc(int c, FILE *stream)
{
	return c;
}

FILE _Stdin;
FILE _Stdout;
FILE _Stderr;

static void block_indefinite(void)
{
	for (;;) {
		volatile int x = 0;
		++x;
	}
}

int ungetc(int c, FILE *stream)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}

int fgetc(FILE *stream)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}

int fseek(FILE *stream, long offset, int whence)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}

int fgetpos(FILE *stream, fpos_t *pos)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}
int fsetpos(FILE *stream, const fpos_t *pos)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}

int setvbuf(FILE *stream, char *buf, int mode, size_t size)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}

#include <wchar.h>

wint_t fputwc(wchar_t wc, FILE *stream)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}

wint_t ungetwc(wint_t wc, FILE *stream)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}

wint_t fgetwc(FILE *stream)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}

#include <stdlib.h>

size_t _Getmbcurmax()
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return 2;
}

#include <ctype.h>

_Ctype_t _Getptolower(void)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return nullptr;
}

_Ctype_t _Getptoupper(void)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return nullptr;
}

#include <xwchar.h>

_Statab *_Getpmbstate(void)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return nullptr;
}

_Statab *_Getpwcstate(void)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return nullptr;
}

int _Mbtowcx(wchar_t *, const char *, size_t, mbstate_t *, _Statab *)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
	return -1;
}

int _Wctombx(char *, wchar_t, mbstate_t *, _Statab *, _Statab *)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	return -1;
}

void _Locksyslock(int x)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
}

void _Unlocksyslock(int x)
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
}

void _Atexit(void (*)(void))
{
	PX4_ERR("Error: Calling unresolved symbol stub[%s]", __FUNCTION__);
	block_indefinite();
}

