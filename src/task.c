/*
   sue.tftpd:
   $Id: task.c,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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

#ifdef FD_SETSIZE
#  undef FD_SETSIZE
#  define FD_SETSIZE 1024
#endif /* FD_SETSIZE */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#include "task_private.h"
#include "proto_udp.h"
#include "proto_tftp.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "debug.h"

#include "globals.h"

#ifndef DEBUG_TASK
#  undef P_DEBUG
#  define P_DEBUG(fmt...) /* null */
#endif
/* file scope variables */
static TASK *TaskVector[FD_SETSIZE];
static fd_set ActiveFds;
static int ActiveFdMax = -1;
static struct int_list *WaitFdList = NULL;

static unsigned int TaskCounter = 0;

/* forward declarations of private functions */
static TASK *task_alloc(int type, int sockfd);
static void task_free(TASK *task);
static TASK *ttbl_add(TASK *task);
static int ttbl_del(TASK *task);
static int renew_task_fds(fd_set *temp);

static int wlst_lookup(int value, int flag);
static int wlst_delete(int value);
static int wlst_vector(TASK *vector[]);
static int do_retrans(TASK *taskv[]);

/*
 * Exported functions
 */
int
task_init(void)
{
    P_DEBUG("---> Initializing task table ...\n");
    P_DEBUG(" Number of table entry = %d.\n", FD_SETSIZE);
    P_DEBUG(" Maximum number of tasks = %d.\n", TASK_ID_MAX);
    memset(TaskVector, 0, sizeof(TASK *) * FD_SETSIZE);
    FD_ZERO(&ActiveFds);
    P_DEBUG("<--- Initializing task table Done.\n");

    return 1;
}

TASK *
task_create(int type, struct sockaddr *laddr, struct sockaddr *caddr,
         socklen_t addrlen)
{
    int sockfd;
    TASK *task;

    if (type == TASK_TYPE_PORTAL) {
        P_DEBUG("Open wild card socket....\n");
        sockfd = open_portal(TFTP_Address, TFTP_Port);
        if (sockfd < 0) {
            P_WARNING("open_portal() failed.\n");
            return NULL;
        }
    } else { 
        sockfd = open_transfer(laddr, caddr, addrlen);
        if (sockfd < 0) {
            P_WARNING("open_transfer() failed.\n");
            return NULL;
        }
    }
    P_DEBUG("<--- sockfd=%d.\n", sockfd);

    task = task_alloc(type, sockfd);
    if (task == NULL) {
        P_WARNING("task_alloc() failed.\n");
        return NULL;
    }

    if (ttbl_add(task) == NULL) {
        P_WARNING("ttbl_add() failed.\n");
        task_free(task);
        return NULL;
    }

    FD_SET(task->sockfd, &ActiveFds);
    if (task->sockfd > ActiveFdMax) ActiveFdMax = task->sockfd;

    P_DEBUG("Task %d started.\n", task->sockfd);
    P_INFO("Active Task [%d/%d]\n", TaskCounter, TASK_ID_MAX);

    return task;
}

int
task_join(TASK *task, int state)
{
    int del_ok;

    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    FD_CLR(task->sockfd, &ActiveFds);
    wlst_delete(task->sockfd);
    del_ok = ttbl_del(task);
    if (del_ok == 0) {
        P_WARNING("ttbl_del() failed.\n");
        return 0;
    }
    if (task->sockfd == ActiveFdMax)
        ActiveFdMax = renew_task_fds(&ActiveFds);

    if (state == TASK_EXIT_NORMAL) {
        P_INFO("Task %d: Exit normally.\n", task_get_id(task));
    } else {
        P_INFO("Task %d: Exit prematurely.\n", task_get_id(task));
    }
    task_free(task);
    P_INFO("Active Task [%d/%d]\n", TaskCounter, TASK_ID_MAX);

    return 1;
}

int
task_main(void)
{
    int sockfd, sel_err, nwait, retrans;
    TASK *wait_tasks[TASK_ID_MAX], *retrans_tasks[TASK_ID_MAX];
    fd_set rfds;
    struct timeval tv;
    struct timeval *tout;

    for(;;) {
        tv.tv_sec = 0;           /* [s] */
        tv.tv_usec = 500 * 1000; /* [ms] */

        if (ActiveFdMax < 0) {
            P_WARNING("No task.\n");
            break;
        }

        P_DEBUG("Check tasks in wait state.\n");
        nwait = wlst_vector(wait_tasks);
        if (nwait > 0) {
            P_DEBUG("Waiting task found. Timer enabled.\n");
            tout = &tv;
        } else {
            P_DEBUG("Wainting task not found. Timer disabled.\n");
            udp_report();
            tftp_report();
            timer_report();
            pkb_report();
            util_report();
            tout = NULL;
        }


        P_DEBUG("Switching task...\n");
        FD_COPY(&ActiveFds, &rfds);
        sel_err = select(ActiveFdMax + 1, &rfds, NULL, NULL, tout);
        if (sel_err < 0) {
            P_WARNING("select() failed: %s.\n", strerror(errno));
            break;
        }
        else if (sel_err == 0) {
            if (nwait > 0) {
                retrans = timer_update(wait_tasks, retrans_tasks);
                if (retrans > 0) {
                    P_DEBUG("Retrans packet.\n");
                    do_retrans(retrans_tasks);
                }
            }
            else {
                P_WARNING("Timer enabled, but no wait task found. BUG?\n");
            }
        }
        else {
            P_DEBUG("Running active tasks.\n");
            for(sockfd=0; sockfd <= ActiveFdMax; sockfd++) {
                if (FD_ISSET(sockfd, &rfds)) {
                    udp_input(TaskVector[sockfd]);
                }
            }
            
            wlst_vector(wait_tasks); /* CWAIT tasks may be done in above. */

            P_DEBUG("Update retrans timer.\n");
            retrans = timer_update(wait_tasks, retrans_tasks);
            if (retrans > 0) {
                P_DEBUG("Retrans packet.\n");
                do_retrans(retrans_tasks);
            }
        }
    }

    /* end of infinite loop */
    return -1;
}

/*
 * task attribute operations.
 */
int
task_get_sockfd(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    return task->sockfd;
}

int
task_get_id(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    return task_get_sockfd(task); /* sockfd is used for id */
}

int
task_set_state(TASK *task, int state)
{
#ifdef DEBUG
    int old_state;
#endif
    struct timeval tv;

    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }
    if (state < 0 || state >= TASK_ST_LAST) {
        P_WARNING("Invalid state specified.\n");
        return 0;
    }

    if (state == TASK_ST_WACK) {
        gettimeofday(&tv, NULL);
        task->retrans.tv_sec = tv.tv_sec;
        task->retrans.tv_usec = tv.tv_usec;
        wlst_lookup(task->sockfd, WLST_CREATE);
    } else {
        task->retrans.tv_sec = 0;
        task->retrans.tv_usec = 0;
        task->retrans_interval = RETRANS_INIT_INTERVAL; /* [us] */
        task->retrans_counter = 0;
        wlst_delete(task->sockfd);
    }

#ifdef DEBUG
    old_state = task->state;
#endif
    task->state = state;
    P_DEBUG("Task %d state %d -> %d\n", task->sockfd, old_state, state);

    return 1;
}

int
task_get_state(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return TASK_ST_INVAL;
    }

    return task->state;
}

char *
task_get_rbuf(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return NULL;
    }

    return task->rbuf;
}

size_t
task_get_rbufsize(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    return task->rbuf_size;
}

size_t
task_get_maxrbufsize(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    return RETRANS_BUFSIZE;
}

int
task_set_rbufsize(TASK *task, size_t size)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }
    if (size > RETRANS_BUFSIZE) {
        P_WARNING("Too big size (%d bytes) specified.\n", size);
        return 0;
    }

    task->rbuf_size = size;

    return 1;
}

int
task_set_type(TASK *task, int type)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return TASK_TYPE_INVAL;
    }

    if (type <= TASK_TYPE_INVAL || type >= TASK_TYPE_LAST) {
        P_WARNING("Invalid type specified.\n");
        return TASK_TYPE_INVAL;
    }

    task->type = type;

    return task->type;
}

int
task_get_type(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return TASK_TYPE_INVAL;
    }

    return task->type;
}

int
task_set_file(TASK *task, FILE *file)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }
    if (file == NULL) {
        P_WARNING("Invalid file specified.\n");
        return 0;
    }

    task->file = file;
    
    return 1;
}

FILE *
task_get_file(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return NULL;
    }

    return task->file;
}

long
task_get_rsec(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    return task->retrans.tv_sec;
}

int
task_set_rsec(TASK *task, long sec)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }
    if (sec < 0) {
        P_WARNING("Invalid sec sepecified.\n");
        return 0;
    }

    task->retrans.tv_sec = sec;

    return 1;
}

long
task_get_rusec(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    return task->retrans.tv_usec;
}

int
task_set_rusec(TASK *task, long usec)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }
    if (usec < 0) {
        P_WARNING("Invalid usec specified.\n");
        return 0;
    }

    task->retrans.tv_usec = usec;

    return 1;
}

int
task_set_rcounter(TASK *task, int count)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }
    if (count < 0) {
        P_WARNING("Invalid counter value specified.\n");
        return 0;
    }


    task->retrans_counter = count;

    return 1;
}

int
task_get_rcounter(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return -1;
    }

    return task->retrans_counter;
}

int
task_inc_rcounter(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    task->retrans_counter++;

    return 1;
}

long
task_get_rinterval(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    return task->retrans_interval;
}

int
task_set_rinterval(TASK *task, long interval)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    task->retrans_interval = interval;

    P_DEBUG("task %d: retrans interval is %ld [us].\n",
            task->sockfd, task->retrans_interval);

    return 1;
}

int
task_set_blockn(TASK *task, u_int16_t blockn)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    task->BlockN = blockn;

    return 1;
}

u_int16_t
task_get_blockn(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invlid task specified.\n");
        return 0;
    }

    return task->BlockN;
}

int
task_inc_blockn(TASK *task)
{
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    task->BlockN++;

    return 1;
}

/*
 * private functions
 */
static TASK *
task_alloc(int type, int sockfd)
{
    TASK *task;

    if (sockfd < 0 || sockfd > TASK_ID_MAX) {
        P_WARNING("Invalid sockfd specified.\n");
        return NULL;
    }
    if (TaskVector[sockfd] != NULL) {
        P_WARNING("Task allocation failed. sockfd %d already used.\n", sockfd);
        return NULL;
    }
    task = (TASK *)safe_malloc(sizeof(TASK));
    if (task == 0) {
        P_WARNING("safe_malloc() failed: %s.\n", strerror(errno));
        return NULL;
    }
    task->sockfd = sockfd;
    task->type = type;
    task->state = TASK_ST_INIT;
    /* rbuf is static array in TASK */
    task->rbuf_size = 0;
    task->retrans.tv_sec = 0;
    task->retrans.tv_usec = 0;
    task->retrans_interval = RETRANS_INIT_INTERVAL;
    task->retrans_counter = 0;
    task->file = NULL;
    if (type == TASK_TYPE_READ) {
        task->BlockN = 1;
    } else {
        task->BlockN = 0;
    }

    TaskCounter++;
    P_DEBUG("Task entry allocated. id %d, total %d.\n",
            task->sockfd, TaskCounter);

    return task;
}

static void
task_free(TASK *task)
{
    int sockfd;

    if (task == NULL) {
        P_WARNING("No task specified.\n");
        return;
    }

    if (TaskCounter == 0) {
        P_WARNING("task_free() called but no task allocated.\n");
        return;
    }

    sockfd = task->sockfd;
    close(task->sockfd);
    if (task->file != NULL) fclose(task->file);
    safe_free(task);

    TaskCounter--;
    P_DEBUG("Task entry freed. id %d, total %d.\n", sockfd, TaskCounter);
    
    return;
}

static TASK *
ttbl_add(TASK *task)
{
    if (TaskVector[task->sockfd] != NULL) {
        P_WARNING("task %d is already exist. BUG?", task->sockfd);
        return NULL;
    }
    TaskVector[task->sockfd] = task;

    P_DEBUG("Task %d is added to the task table.\n", task->sockfd);

    return task;
}

static int
ttbl_del(TASK *task)
{
    TaskVector[task->sockfd] = NULL;

    P_DEBUG("Task %d is deleted from the task table.\n", task->sockfd);

    return 1;
}

static int
renew_task_fds(fd_set *fds) {
    int sockfd, fdmax = -1;

    FD_ZERO(fds);
    for (sockfd=0; sockfd < FD_SETSIZE; sockfd++) {
        if (TaskVector[sockfd] != NULL) {
            P_DEBUG("fd %d is added to select() list.\n", sockfd);
            fdmax = sockfd;
            FD_SET(sockfd, fds);
        }
    }

    return fdmax;
}

static int
wlst_lookup(int value, int flag)
{
    struct int_list *listp, *tailp;

    for (listp=WaitFdList; listp != NULL; listp=listp->next) {
        if (listp->value == value) {
            if (flag == WLST_LOOKUP) {
                P_DEBUG("value %d is found.\n", value);
                return 1;
            }
            else if (flag == WLST_CREATE) {
                P_DEBUG("WLST_CREATE specified, but value %d is found. BUG?\n",
                        value);
                return 0;
            }
        }
    }

    /* not found */
    if (flag == WLST_LOOKUP) {
        P_DEBUG("value %d is not found.\n", value);
        return 0;
    }

    /* create new entry */
    listp = (struct int_list *) safe_malloc(sizeof(struct int_list));
    if (listp == NULL) {
        P_WARNING("safe_malloc() failed: %s.", strerror(errno));
        return 0;
    }
    listp->value = value;
    listp->next = NULL;
    listp->prev = NULL;

    if (WaitFdList == NULL) {
        /*
         * 1st entry
         *
         * Note: WaitFdList->prev = tail
         *       tail->next = NULL
         */
        WaitFdList = listp;
        WaitFdList->value = value;
        WaitFdList->next = NULL;
        WaitFdList->prev = WaitFdList;
        P_DEBUG("value %d added to list(1st entry).\n", value);
    } else {
        tailp = WaitFdList->prev;
        tailp->next = listp;
        listp->prev = tailp;
        WaitFdList->prev = listp;
        P_DEBUG("value %d added to list.\n", value);
    }
    return 1;
}

static int
wlst_delete(int value)
{
    int found = 0;
    struct int_list *listp, *prevp, *nextp;

    for (listp=WaitFdList; listp != NULL; listp=listp->next) {
        if (listp->value == value) {
            found = 1;
            break;
        }
    }

    if (found == 0) {
        P_DEBUG("Requested sockfd not found.\n");
        return 0;
    }

    prevp = listp->prev;
    nextp = listp->next;

    if (prevp == NULL) {
        P_WARNING("WaitFdList is broken. BUG?\n");
        return 0;
    }

    if (prevp == listp) {
        /* last one entry */
        P_DEBUG("value %d is deleted(last entry).\n", value);
        WaitFdList = NULL;
    }
    else if (nextp == NULL) {
        /* tail entry */
        P_DEBUG("value %d is deleted(tail entry).\n", value);
        prevp->next = NULL;
        WaitFdList->prev = prevp;
    }
    else if (prevp->next == NULL) {
        /* head entry */
        P_DEBUG("value %d is deleted(head entry).\n", value);
        nextp->prev = prevp;
        WaitFdList = nextp;
    }
    else {
        /* other */
        P_DEBUG("value %d is deleted.\n", value);
        prevp->next = nextp;
        nextp->prev = prevp;
    }
    safe_free(listp);

    return 1;
}

static int
wlst_vector(TASK *vector[])
{
    int i = 0;
    struct int_list *listp;

    for(listp=WaitFdList; listp != NULL; listp=listp->next) {
        vector[i++] = TaskVector[listp->value];
    }

    /* end marker */
    vector[i] = NULL;

    return i;
}

static int
do_retrans(TASK *taskv[])
{
    TASK **taskp;
    int err = 0;

    P_DEBUG("Retrans packet.\n");
    for (taskp=taskv; *taskp != NULL; taskp++) {
        if ( tftp_retrans(*taskp) == 0) {
            P_WARNING("task_retrans() failed.\n");
            err = 1;
        }
    }

    if (err == 1) return 0;

    return 1;
}
