/****************************************************************************
 * include/nuttx/binfmt/builtin.h
 *
 * Originally by:
 *
 *   Copyright (C) 2011 Uros Platise. All rights reserved.
 *   Author: Uros Platise <uros.platise@isotel.eu>
 *
 * With subsequent updates, modifications, and general maintenance by:
 *
 *   Copyright (C) 2012-2013 Gregory Nutt.  All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
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
 * 3. Neither the name NuttX nor the names of its contributors may be
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

#ifndef __INCLUDE_NUTTX_BINFMT_BUILTIN_H
#define __INCLUDE_NUTTX_BINFMT_BUILTIN_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <sys/types.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct builtin_s
{
  const char *name;         /* Invocation name and as seen under /sbin/ */
  int         priority;     /* Use: SCHED_PRIORITY_DEFAULT */
  int         stacksize;    /* Desired stack size */
  main_t      main;         /* Entry point: main(int argc, char *argv[]) */
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C" {
#else
#define EXTERN extern
#endif

/* The g_builtins[] array holds information about each builtin function. If
 * support for builtin functions is enabled in the NuttX configuration, then
 * this arrary (along with the number_builtins() function) must be provided
 * by the application code.
 */

EXTERN const struct builtin_s g_builtins[];

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Utility Functions Provided to Applications by binfmt/libbuiltin
 ****************************************************************************/
/****************************************************************************
 * Name: builtin_isavail
 *
 * Description:
 *   Checks for availabiliy of application registerred during compile time.
 *
 * Input Parameter:
 *   filename - Name of the linked-in binary to be started.
 *
 * Returned Value:
 *   This is an end-user function, so it follows the normal convention:
 *   Returns index of builtin application. If it is not found then it
 *   returns -1 (ERROR) and sets errno appropriately.
 *
 ****************************************************************************/

int builtin_isavail(FAR const char *appname);

/****************************************************************************
 * Name: builtin_getname
 *
 * Description:
 *   Returns pointer to a name of built-in application pointed by the
 *   index.
 *
 * Input Parameter:
 *   index, from 0 and on ...
 *
 * Returned Value:
 *   Returns valid pointer pointing to the app name if index is valid.
 *   Otherwise NULL is returned.
 *
 ****************************************************************************/

FAR const char *builtin_getname(int index);

/****************************************************************************
 * Data Set Access Functions Provided to Applications by binfmt/libbuiltin
 ****************************************************************************/
/****************************************************************************
 * Name: number_builtins
 *
 * Description:
 *   Returns the number of builtin functions in the g_builtins[] array.  If
 *   support for builtin functions is enabled in the NuttX configuration,
 *   then this function (along with g_builtins[]) must be provided by the
 *   application code.
 *
 * Input Parameter:
 *   None
 *
 * Returned Value:
 *   The number of entries in the g_builtins[] array.  This function does
 *   not return failures.
 *
 ****************************************************************************/

int number_builtins(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __INCLUDE_NUTTX_BINFMT_BUILTIN_H */

