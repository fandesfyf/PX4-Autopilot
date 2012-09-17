/************************************************************
 * lib/queue/sq_rem.c
 *
 *   Copyright (C) 2007, 2011 Gregory Nutt. All rights reserved.
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
 ************************************************************/

/************************************************************
 * Compilation Switches
 ************************************************************/

/************************************************************
 * Included Files
 ************************************************************/

#include <queue.h>

/************************************************************
 * public Functions
 ************************************************************/

/************************************************************
 * Name: sq_rem
 *
 * Description:
 *   sq_rem removes a 'node' for 'queue.'
 *
 ************************************************************/

void sq_rem(FAR sq_entry_t *node, sq_queue_t *queue)
{
  if (queue->head && node)
    {
      if (node == queue->head)
        {
          queue->head = node->flink;
          if (node == queue->tail)
            {
              queue->tail = NULL;
            }
        }
      else
        {
          FAR sq_entry_t *prev;
          for(prev = (FAR sq_entry_t*)queue->head;
              prev && prev->flink != node;
              prev = prev->flink);

          if (prev)
            {
              sq_remafter(prev, queue);
            }
        }
    }
}
