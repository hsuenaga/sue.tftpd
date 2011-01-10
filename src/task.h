/*
   sue.tftpd:
   $Id: task.h,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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
#ifndef __TASK_H__
#define __TASK_H__
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#ifndef __TASK_PRIVATE_H__
typedef struct __tftp_task TASK;

struct __tftp_task {
    /* task structure is protected. Only task.c can read actual value. */
    char *__dummy[0];
};
#endif

/*
 * task status operations.
 */
int task_init(void);
TASK *task_create(int type, struct sockaddr *daddr, 
                              struct sockaddr *caddr,socklen_t caddrlen);
int task_join(TASK *task, int state);
int task_main(void);

/*
 * task attribute operations
 */
/* ID */
int task_get_id(TASK *task);
/* Sockfd */
int task_get_sockfd(TASK *task);
/* State */
int task_set_state(TASK *task, int state);
int task_get_state(TASK *task);
/* Retrans buffer */
char *task_get_rbuf(TASK *task);
int task_set_rbufsize(TASK *task, size_t size);
size_t task_get_rbufsize(TASK *task);
size_t task_get_maxrbufsize(TASK *task);
/* Type */
int task_set_type(TASK *task, int type);
int task_get_type(TASK *task);
/* File */
int task_set_file(TASK *task, FILE *file);
FILE *task_get_file(TASK *task);
/* Retrans timer state time */
int task_set_rsec(TASK *task, long sec);
long task_get_rsec(TASK *task);
int task_set_rusec(TASK *task, long usec);
long task_get_rusec(TASK *task);
/* Retrans counter */
int task_set_rcounter(TASK *task, int count);
int task_get_rcounter(TASK *task);
int task_inc_rcounter(TASK *task);
/* Retrans interval */
long task_get_rinterval(TASK *task);
int task_set_rinterval(TASK *task, long interval);
/* Block number */
int task_set_blockn(TASK *task, u_int16_t blockn);
u_int16_t task_get_blockn(TASK *task);
int task_inc_blockn(TASK *task);

/*
 * Constant value and parameters
 */
enum task_type {
    TASK_TYPE_INVAL,  /* for error report */
    TASK_TYPE_PORTAL, /* task for wait on port 69 */
    TASK_TYPE_READ,   /* task for RRQ */
    TASK_TYPE_WRITE,  /* task for WRQ */
    TASK_TYPE_CWAIT,  /* task for Waiting Normal Close */
    TASK_TYPE_ERROR,  /* task for Waiting Premature Close */
    TASK_TYPE_LAST
};

enum task_state {
    TASK_ST_INVAL,  /* for error report */
    TASK_ST_INIT,   /* initial state */
    TASK_ST_PORTAL, /* wait on port 69 */
    TASK_ST_WACK,   /* wait for Ack */
    TASK_ST_SEND,   /* sending packet */
    TASK_ST_ERROR,  /* sending error */
    TASK_ST_LAST
};

enum task_exit_state {
    TASK_EXIT_NORMAL,
    TASK_EXIT_ERROR,
    TASK_EXIT_LAST
};

enum task_params {
    TASK_ID_MAX = 500, /* MUST smaller than FD_SETSIZE */
    SELECT_TIMEOUT = 500, /* [ms] MUST smaller than RETRANS_INIT_INTERVAL */
};

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __TASK_H__ */
