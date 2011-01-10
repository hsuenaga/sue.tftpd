/*
   sue.tftpd:
   $Id: timer.c,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

   This file is part of sue.tftpd.

   Copyright 2002 SUENAGA Hiroki <hsuenaga@jaist.ac.jp>.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without 
   modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
   3. All advertising materials mentioning features or use of this software
      must display the following acknowledgement:

        This product includes software developed by
        SUENAGA Hiroki <hsuenaga@jaist.ac.jp> and its contributors.

   4. Neither the name of authors nor the names of its contributors may be used
      to endorse or promote products derived from this software without
      specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/time.h>

#include "task.h"
#include "timer.h"
#include "util.h"
#include "debug.h"

#ifndef DEBUG_TIMER
#  undef P_DEBUG
#  define P_DEBUG(fmt...) /* null */
#endif

/*
 * file scope variables
 */
static unsigned int ExpireCounter = 0;
static unsigned int ResetCounter = 0;

/*
 * Exported functions
 */
int
timer_update(TASK *wait_tasks[], TASK *retrans_tasks[])
{
    int i=0;
    struct timeval tv;
    TASK **walk;
    long sdiff, udiff, diff, interval;

    gettimeofday(&tv, NULL);

    for(walk=wait_tasks; *walk != NULL; walk++) {

        interval = task_get_rinterval(*walk);
        sdiff = tv.tv_sec - task_get_rsec(*walk);
        udiff = tv.tv_usec - task_get_rusec(*walk);
        diff = sdiff * 1000 * 1000 + udiff;
        
        if (diff < 0) {
            P_WARNING("Clock skew detected. Resetting timer.\n");
            task_set_rsec(*walk, tv.tv_sec);
            task_set_rusec(*walk, tv.tv_usec);
            ResetCounter++;
            continue;
        }

        if (diff > interval) {
            P_DEBUG("Timer expired for task %d.\n", task_get_sockfd(*walk));
            task_set_rsec(*walk, tv.tv_sec);
            task_set_rusec(*walk, tv.tv_usec);
            retrans_tasks[i] = *walk;
            ExpireCounter++;
            i++;
        }
    }
    
    retrans_tasks[i] = NULL;

    return i;
}

void
timer_report(void)
{
    P_INFO("--- timer statics ---\n");
    P_INFO(" Expire   Counter = %d\n", ExpireCounter);
    P_INFO(" Reset    Counter = %d\n", ResetCounter);

    return;
}
