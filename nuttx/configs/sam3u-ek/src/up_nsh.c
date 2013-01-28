/****************************************************************************
 * config/sam3u-ek/src/up_nsh.c
 * arch/arm/src/board/up_nsh.c
 *
 *   Copyright (C) 2010 Gregory Nutt. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdio.h>
#include <debug.h>
#include <errno.h>

#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>

#include "sam3u_internal.h"
#include "sam3uek_internal.h"

#ifdef CONFIG_SAM3U_HSMCI

/****************************************************************************
 * Pre-Processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/* PORT and SLOT number probably depend on the board configuration */

#ifdef CONFIG_ARCH_BOARD_SAM3UEK
#  define NSH_HAVEUSBDEV 1
#  define NSH_HAVEMMCSD  1
#  if defined(CONFIG_NSH_MMCSDSLOTNO) && CONFIG_NSH_MMCSDSLOTNO != 0
#    error "Only one MMC/SD slot"
#    undef CONFIG_NSH_MMCSDSLOTNO
#  endif
#  ifndef CONFIG_NSH_MMCSDSLOTNO
#    define CONFIG_NSH_MMCSDSLOTNO 0
#  endif
#else
   /* Add configuration for new SAM3U boards here */
#  error "Unrecognized SAM3U board"
#  undef NSH_HAVEUSBDEV
#  undef NSH_HAVEMMCSD
#endif

/* Can't support USB features if USB is not enabled */

#ifndef CONFIG_USBDEV
#  undef NSH_HAVEUSBDEV
#endif

/* Can't support MMC/SD features if mountpoints are disabled or if SDIO support
 * is not enabled.
 */

#if defined(CONFIG_DISABLE_MOUNTPOINT) || !defined(CONFIG_SAM3U_HSMCI)
#  undef NSH_HAVEMMCSD
#endif

#ifndef CONFIG_NSH_MMCSDMINOR
#  define CONFIG_NSH_MMCSDMINOR 0
#endif

/* Debug ********************************************************************/

#ifdef CONFIG_CPP_HAVE_VARARGS
#  ifdef CONFIG_DEBUG
#    define message(...) syslog(__VA_ARGS__)
#  else
#    define message(...) printf(__VA_ARGS__)
#  endif
#else
#  ifdef CONFIG_DEBUG
#    define message syslog
#  else
#    define message printf
#  endif
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nsh_archinitialize
 *
 * Description:
 *   Perform architecture specific initialization
 *
 ****************************************************************************/

int nsh_archinitialize(void)
{
#ifdef NSH_HAVEMMCSD
  FAR struct sdio_dev_s *sdio;
  int ret;

  /* Mount the SDIO-based MMC/SD block driver */
  /* First, get an instance of the SDIO interface */

  message("nsh_archinitialize: Initializing SDIO slot %d\n",
          CONFIG_NSH_MMCSDSLOTNO);
  sdio = sdio_initialize(CONFIG_NSH_MMCSDSLOTNO);
  if (!sdio)
    {
      message("nsh_archinitialize: Failed to initialize SDIO slot %d\n",
              CONFIG_NSH_MMCSDSLOTNO);
      return -ENODEV;
    }

  /* Now bind the SDIO interface to the MMC/SD driver */

  message("nsh_archinitialize: Bind SDIO to the MMC/SD driver, minor=%d\n",
          CONFIG_NSH_MMCSDMINOR);
  ret = mmcsd_slotinitialize(CONFIG_NSH_MMCSDMINOR, sdio);
  if (ret != OK)
    {
      message("nsh_archinitialize: Failed to bind SDIO to the MMC/SD driver: %d\n", ret);
      return ret;
    }
  message("nsh_archinitialize: Successfully bound SDIO to the MMC/SD driver\n");
  
  /* Then inform the HSMCI driver if there is or is not a card in the slot. */

   sdio_mediachange(sdio, sam3u_cardinserted(0));
#endif
  return OK;
}
#endif
