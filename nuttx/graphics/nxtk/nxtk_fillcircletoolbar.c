/****************************************************************************
 * graphics/nxtk/nxtk_fillcircletoolbar.c
 *
 *   Copyright (C) 2011 Gregory Nutt. All rights reserved.
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

#include <sys/types.h>
#include <debug.h>
#include <errno.h>

#include <nuttx/nx/nxglib.h>
#include <nuttx/nx/nxtk.h>

/****************************************************************************
 * Pre-Processor Definitions
 ****************************************************************************/

#define NCIRCLE_TRAPS 8

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nxtk_fillcircletoolbar
 *
 * Description:
 *  Fill a circular region using the specified color.
 *
 * Input Parameters:
 *   hfwnd  - The window handle returned by nxtk_openwindow()
 *   center - A pointer to the point that is the center of the circle
 *   radius - The radius of the circle in pixels.
 *   color  - The color to use to fill the circle.
 *
 * Return:
 *   OK on success; ERROR on failure with errno set appropriately
 *
 ****************************************************************************/

int nxtk_fillcircletoolbar(NXWINDOW hfwnd, FAR const struct nxgl_point_s *center,
                           nxgl_coord_t radius,
                           nxgl_mxpixel_t color[CONFIG_NX_NPLANES])
{
  FAR struct nxgl_trapezoid_s traps[NCIRCLE_TRAPS];
  int i;
  int ret;

  /* Describe the circular region as a sequence of 8 trapezoids */

  nxgl_circletraps(center, radius, traps);

  /* Then rend those trapezoids */

  for (i = 0; i < NCIRCLE_TRAPS; i++)
    {
      ret = nxtk_filltraptoolbar(hfwnd, &traps[i], color);
      if (ret != OK)
        {
          return ret;
        }
    }
  return OK;
}
