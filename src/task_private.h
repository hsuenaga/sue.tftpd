/*
   sue.tftpd:
   $Id: task_private.h,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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
#ifndef __TASK_PRIVATE_H__
#define __TASK_PRIVATE_H__
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef __TASK_H__
#  error "task_private.h" must included before "task.h".
#endif

#define RETRANS_BUFSIZE 600         /* max payload size (not include header) */
                                    /* MUST lager than TFTP_DATA_MAX_SIZE */
struct int_list {
    int value;
    struct int_list *prev;
    struct int_list *next;
};

typedef struct __tftp_task TASK;

struct __tftp_task {
    int sockfd;                     /* socket fd (used for identifire) */
    int type;                       /* task type(portal, read, write) */
    int state;                      /* task status */
    size_t rbuf_size;               /* length of retransmit data */
    struct timeval retrans;         /* retrans timer start time */
    long retrans_interval;          /* retrnas interval */
    int  retrans_counter;           /* number of retrans tryed */
    FILE *file;                     /* file to read/write */
    u_int16_t BlockN;               /* Block number of TFTP */
    char rbuf[RETRANS_BUFSIZE];     /* buffer for retransmit */
};

#ifndef FD_COPY
#  define FD_COPY(src, dest) (void)memcpy((dest), (src), sizeof(*(src)))
#endif /* FD_COPY */

enum wait_list_flags {
    WLST_LOOKUP,
    WLST_CREATE,
};

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __TASK_PRIVATE_H__ */
