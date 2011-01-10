/*
   sue.tftpd:
   $Id: proto_tftp.c,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include "pkt_buff.h"
#include "proto_udp.h"
#include "proto_tftp.h"
#include "task.h"
#include "util.h"
#include "debug.h"

#ifndef DEBUG_PROTO_TFTP
#  undef P_DEBUG
#  define P_DEBUG(fmt...) /* null */
#endif

/*
 * file scope variables
 */
static const char *err_msgs[] = {
    "Unknown error.",
    "File not found.",
    "Access violation.",
    "Disk full or allocation exceeded.",
    "Illegal TFTP operation.",
    "Unknown transfer ID.",
    "File already exists.",
    "No such user.",
};

static unsigned int AcceptCounter = 0;
static unsigned int RejectCounter = 0;
static unsigned int RetransCounter = 0;
static unsigned int TimeoutCounter = 0;

/*
 * Constatns.
 */
enum FILE_STATE {
    FILE_ST_UNKNOWN, /* unkonwn error */
    FILE_ST_OK,      /* no problem */
    FILE_ST_TOOBIG,  /* file too big to transfer using TFTP */
    FILE_ST_EACCES,  /* access denied */
    FILE_ST_ENOENT,  /* file not exist */
    FILE_ST_EREG,    /* not regular file */
    FILE_ST_LAST
};

enum buffer_params {
    FILE_BUFSIZE = 600,
};

/*
 * forward declaration of private functions.
 */
static struct tftp_req_str *parse_req(struct pkt_buff *pkb);
static int rrq_input(TASK *task, struct pkt_buff *pkb);
static int ack_input(TASK *task, struct pkt_buff *pkb);
static int error_output(TASK *task, u_int16_t errcode);
static int data_output(TASK *task);
static int check_filest(char *fname);
static int check_pktlen(int pkt_type, int size);

/*
 * Exported functions
 */
int
tftp_input(TASK *task, struct pkt_buff *pkb)
{
    int size_ok, task_ok, retval;
    int opcode;
    TASK *new_task;
    struct tftp_pkt *tpkt;
    struct sockaddr *caddr;
    struct sockaddr *daddr;
    socklen_t addrlen;

    P_DEBUG("Task id is %d.\n", task_get_id(task));

    /* error check */
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }
    if (pkb == NULL) {
        P_WARNING("Invalid packet specified.\n");
        return 0;
    }

    /* debug ... */
#if defined(DEBUG) && defined(DEBUG_PROTO_TFTP)
    pkb_dump(pkb);
#endif

    /* initialize variables */
    daddr = pkb->laddr;
    caddr = pkb->caddr;
    addrlen = pkb->addrlen;
    tpkt = PKB_TO_TFTP(pkb);
    opcode = ntohs(tpkt->Opcode);
    retval = 0;

    /* check consistency */
    size_ok = check_pktlen(opcode, pkb->size);
    if (size_ok == 0) {
        P_WARNING("Packet too small.\n");
        return 0;
    }
    
    /* create new task for request*/
    switch(opcode) {
        case TFTP_RRQ:
            P_INFO("Task %d: Read request received from interface %s.\n",
                   task_get_id(task), strsockaddr(daddr, addrlen));
            new_task = task_create(TASK_TYPE_READ, daddr, caddr, addrlen);
            if (new_task == NULL) {
                P_WARNING("task_new() failed.\n");
                break;
            }
            task_ok = rrq_input(new_task, pkb);
            if (task_ok == 0) {
                P_WARNING("unable to start new task.\n");
                break;
            }
            retval = 1;
            break;
        case TFTP_WRQ:
            P_DEBUG("Opcode is WRQ.\n");
            break;
        case TFTP_DATA:
            P_DEBUG("Opcode is DATA.\n");
            break;
        case TFTP_ACK:
            P_DEBUG("Opcode is ACK.\n");
            ack_input(task, pkb);
            retval = 1;
            break;
        case TFTP_ERROR:
            P_DEBUG("Opcode is ERROR.\n");
            break;
        default:
            P_DEBUG("Unknown Opcode %d.\n", opcode);
            break;
    }

    return retval;
}

int
tftp_output(int sockfd, struct pkt_buff *pkb)
{
    int send_ok;

    /* XXX: Under construction ... */
    if (is_socket(sockfd) == 0) {
        P_WARNING("fd %d is not a socket.\n");
        return 0;
    }
    if (pkb == NULL) {
        P_WARNING("No pakcet specified.\n");
        return 0;
    }

    /* XXX: maybe filetering is inserted here. */

    send_ok = udp_output(sockfd, pkb);
    if (send_ok == 0) {
        P_WARNING("udp_output() failed.\n");
        return 0;
    }

    return 1;
}

int
tftp_retrans(TASK *task)
{
    int send_ok;
    int type, state, retc, rbufsize;
    struct pkt_buff *pkb;
    unsigned char *rbuf;
    long interval;

    /* error check */
    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    /* initialize variables */
    type = task_get_type(task);
    state = task_get_state(task);
    retc = task_get_rcounter(task);
    rbuf = task_get_rbuf(task);
    rbufsize = task_get_rbufsize(task);
    interval = task_get_rinterval(task);

    /* consistency check */
    if (type == TASK_TYPE_ERROR) {
        /* RFC1350 Page 8: Error pakcet never retransmitted. */
        task_join(task, TASK_EXIT_ERROR);
        return 0;
    }
    if (state != TASK_ST_WACK) {
        P_WARNING("tftp_retrans() called, but task %d not in wait state.",
                  task_get_id(task));
        return 0;
    }
    if (retc >= RETRANS_MAX) {
        P_WARNING("Task %d: Timeout during transfer.\n", task_get_sockfd(task));
        TimeoutCounter++;
        task_join(task, TASK_EXIT_ERROR);
        return 0;
    }

    /* set up packet buffer */
    pkb = pkb_alloc(rbufsize);
    memcpy(pkb->payload, rbuf, pkb->size);

    /* output */
    send_ok = tftp_output(task_get_sockfd(task), pkb);
    if (send_ok == 0) {
        P_WARNING("tftp_output() failed.\n");
        return 0;
    }

    /* renew timer parameter */
    interval *= RETRANS_BACKOFF_FACTOR;
    task_set_rinterval(task, interval);
    task_inc_rcounter(task);

    RetransCounter++;

    P_DEBUG("last packet retransed %d times.\n", retc);

    return 1;
}

void
tftp_report(void)
{
    P_INFO("--- TFTP statics ---\n");
    P_INFO(" Accept   Counter = %d\n", AcceptCounter);
    P_INFO(" Reject   Counter = %d\n", RejectCounter);
    P_INFO(" Retrans  Counter = %d\n", RetransCounter);
    P_INFO(" Timeout  Counter = %d\n", TimeoutCounter);

    return;
}

/*
 * Private fuctions
 */
static int
rrq_input(TASK *task, struct pkt_buff *pkb)
{
    int send_ok;
    int state, errcode, filest;
    struct tftp_req_str *reqs;
    FILE *file = NULL;

    P_DEBUG("Task id is %d.\n", task_get_id(task));

    /* initialize variables */
    state = task_get_state(task);

    /* check consistency */
    if (state != TASK_ST_INIT) {
        P_WARNING("Task %d is not a new task. BUG?\n", task_get_id(task));
        pkb_free(pkb);
        task_join(task, TASK_EXIT_ERROR);
        return 0;
    }

    /* parse request */
    reqs = parse_req(pkb);
    if (reqs == NULL) {
        P_WARNING("parse_req() failed.\n");
        pkb_free(pkb);
        task_join(task, TASK_EXIT_ERROR);
        return 0;
    }
    P_INFO("Task %d: file=\"%s\" client addr=%s.\n",
           task_get_id(task), reqs->Filename,
           strsockaddr(pkb->caddr, pkb->addrlen));

    /* setup task */
    errcode = TFTP_ENONE; /* TFTP_ENONE = no error */
    if (strncasecmp("octet", reqs->Mode, reqs->mode_len) != 0) {
        P_WARNING("Only \"octet\" transfer mode is supported.\n");
        /* XXX: netascii MUST supported. */
        errcode = TFTP_EILLEGAL;
        goto Check_Done;
    }
    filest = check_filest(reqs->Filename);
    if (filest != FILE_ST_OK) {
        switch (filest) {
            case FILE_ST_TOOBIG:
                P_INFO("Task %d: Read request rejected. File too big.\n",
                        task_get_id(task));
                errcode = TFTP_EILLEGAL;
                break;
            case FILE_ST_EACCES:
                P_INFO("Task %d: Read request rejected. Access denied.\n",
                        task_get_id(task));
                errcode = TFTP_ENDEF;
                break;
            case FILE_ST_ENOENT:
                P_INFO("Task %d: Read request rejected. File not found.\n",
                        task_get_id(task));
                errcode = TFTP_ENOENT;
                break;
            case FILE_ST_EREG:
                P_INFO("Task %d: Read request rejected. File isn't regular file.\n",
                        task_get_id(task));
                errcode = TFTP_EILLEGAL;
                break;
            default:
                P_INFO("Task %d: Read request rejected. Unknown error.\n",
                       task_get_id(task)); 
                errcode = TFTP_ENDEF;
                break;
        }
        goto Check_Done;
    }
    file = fopen(reqs->Filename, "r");
    if (file == NULL) {
        switch(errno) {
            case EACCES:
                P_INFO("Task %d: Read request rejected. Access denied.\n",
                       task_get_id(task));
                errcode = TFTP_EACCES;
                break;
            case ENOENT:
                P_INFO("Task %d: Read request rejected. File not found.\n",
                       task_get_id(task));
                errcode = TFTP_ENOENT;
                break;
            default:
                P_INFO("Task %d: Read request rejected. Unknown error.\n",
                       task_get_id(task));
                errcode = TFTP_ENDEF;
                break;
        }
        goto Check_Done;
    }
Check_Done:
    safe_free(reqs);
    pkb_free(pkb);
    if (errcode != TFTP_ENONE) {
        send_ok = error_output(task, errcode);
        if (send_ok == 0) {
            P_WARNING("error_output() failed.\n");
            task_join(task, TASK_EXIT_ERROR);
            return 0;
        }
        /* error task is a valid task. */
        RejectCounter++;
        return 1;
    }
    task_set_file(task, file);

    /* From RFC1350 Page4: Ack for RRQ is 1st data packet */
    send_ok = data_output(task);
    if (send_ok == 0) {
        P_WARNING("data_output() failed. cannot initialize connection.\n");
        task_join(task, TASK_EXIT_ERROR);
        return 0;
    }

    AcceptCounter++;
    P_INFO("Task %d: Request accepted.\n", task_get_id(task));

    return 1;
}

static int
ack_input(TASK *task, struct pkt_buff *pkb)
{
    int send_ok;
    int type, state, retval;
    u_int16_t expect, received;
    struct tftp_pkt *tpkt;
    struct tftp_ack *apkt;

    P_DEBUG("Task id is %d.\n", task_get_id(task));

    /* initialize variables */
    type = task_get_type(task);
    state = task_get_state(task);
    tpkt = PKB_TO_TFTP(pkb);
    apkt = TFTP_TO_ACK(tpkt);
    expect = task_get_blockn(task); /* XXX */
    received = ntohs(apkt->BlockN);
    P_DEBUG("Ack: Received Block = %d, Wait Ack = %d.\n", received, expect);

    /* free received packet */
    pkb_free(pkb);

    /* check consistency */
    if (state != TASK_ST_WACK) {
        P_WARNING("Ack received, but task not wait for ack.\n");
        return 1;
    }
    if (expect != received) {
        /*
         * NOTE: Don't run retrans code when dupilicate ack received.
         *       If do so, dupilicate packet goes multiplied ....
         */
        /* XXX: Ack for error packet may ignore block number? */
        P_DEBUG("Task %d: Block number mismatch. Expect %d, received %d\n",
                  task_get_id(task), expect, received);
        return 1;
    }
    
    /* OK, transaction go forward */
    switch (type) {
        case TASK_TYPE_READ:
            P_DEBUG("Read next block from file.\n");
            task_inc_blockn(task);
            send_ok = data_output(task);
            if (send_ok == 0) {
                P_WARNING("tftp_send_data() failed.\n");
                retval = 1;
            }
            break;
        case TASK_TYPE_CWAIT:
            task_join(task, TASK_EXIT_NORMAL);
            retval = 0;
            break;
        case TASK_TYPE_ERROR:
            task_join(task, TASK_EXIT_ERROR);
            retval = 0;
            break;
        default:
            P_WARNING("Task %d: Unknown task type %d. BUG?\n",
                      task_get_id(task), type);
            break;
    }

    return 1;
}

/* output related */
static int
error_output(TASK *task, u_int16_t errcode)
{
    int send_ok;
    int msglen;
    size_t bufsize;
    struct pkt_buff *pkb;
    struct tftp_pkt *tpkt;
    struct tftp_err *terr;

    P_DEBUG("Sending error 0x%02x(%s).\n", errcode, err_msgs[errcode]);

    task_set_type(task, TASK_TYPE_ERROR);
    task_set_state(task, TASK_ST_ERROR);

    /* initialize variables */
    msglen = strlen(err_msgs[errcode]) + 1;
    bufsize = TFTP_HDLEN + TFTP_ERR_HDLEN + msglen;

    /* setup packet buffer */
    pkb = pkb_alloc(bufsize);
    if (pkb == NULL) {
        P_WARNING("pkb_alloc() failed.\n");
        return 0;
    }
    tpkt = PKB_TO_TFTP(pkb);
    terr = TFTP_TO_ERR(tpkt);

    /* encapsulation */
    tpkt->Opcode = htons(TFTP_ERROR);
    terr->ErrorCode = htons(errcode);
    memcpy(terr->ErrMsg, err_msgs[errcode], msglen);

    /* Error packet doesn't retransmitted at this time.
     * So rbuf set up is not required.
     */ 

    /* output */
    send_ok = tftp_output(task_get_sockfd(task), pkb);
    if (send_ok == 0) {
        P_WARNING("tftp_output() failed.\n");
        return 0;
    }

    task_set_state(task, TASK_ST_WACK);

    return 1;
}

static int
data_output(TASK *task)
{
    int output_ok, fread_err, rbufsize, bufsize, max;
    char *rbuf;
    char buf[FILE_BUFSIZE];
    struct pkt_buff *pkb;
    struct tftp_pkt *tpkt;
    struct tftp_data *tdata;
    FILE *fp;

    task_set_state(task, TASK_ST_SEND);

    /* initialize variables */
    fp = task_get_file(task);
    max = TFTP_DATA_MAX_SIZE;
    bufsize = 0;
    rbuf = task_get_rbuf(task);
    rbufsize = 0;

    /* renew retransmit buffer */
    bufsize = fread(buf, sizeof(char), max, fp);
    fread_err = errno;
    P_DEBUG("read %d bytes from file.\n", bufsize);
    if (bufsize < max) {
        if (feof(fp) == 0) {
            P_WARNING("fread() failed: %s.\n", strerror(fread_err));
            return 0;
        }
        task_set_type(task, TASK_TYPE_CWAIT);
    }

    /* set up packet buffer */
    pkb = pkb_alloc(TFTP_HDLEN + TFTP_DATA_HDLEN + bufsize);
    if (pkb == NULL) {
        P_WARNING("pkb_alloc() failed.\n");
        return 0;
    }
    tpkt = PKB_TO_TFTP(pkb);
    tdata = TFTP_TO_DATA(tpkt);

    /* encapsulation data */
    tpkt->Opcode = htons(TFTP_DATA);
    tdata->BlockN = htons(task_get_blockn(task));
    memcpy(tdata->Data, buf, bufsize);

    /* store packet in retrans buffer */
    rbufsize = TFTP_HDLEN + TFTP_DATA_HDLEN + bufsize;
    task_set_rbufsize(task, rbufsize);
    memcpy(rbuf, pkb->payload, rbufsize);

    /* output packet */
    output_ok = tftp_output(task_get_sockfd(task), pkb);
    if (output_ok == 0) {
        P_WARNING("tftp_output() failed.\n");
        return 0;
    }
    P_DEBUG("Task %d output block %d.\n", task_get_id(task),
            task_get_blockn(task));

    task_set_state(task, TASK_ST_WACK);

    return 1;
}

/* misc */
static struct tftp_req_str *
parse_req(struct pkt_buff *pkb)
{
    int i, pkt_ok;
    unsigned char *walk;
    struct tftp_req_str *reqs;
    struct tftp_pkt *tpkt;
    struct tftp_req *treq;

    /* initialize variables */
    tpkt = PKB_TO_TFTP(pkb);
    treq = TFTP_TO_REQ(tpkt);
    reqs = (struct tftp_req_str *)safe_malloc(sizeof(struct tftp_req_str));
    if (reqs == NULL) {
        P_WARNING("safe_malloc() failed: %s.\n", strerror(errno));
        return NULL;
    }
    reqs->Filename = reqs->Mode = treq->string;
    reqs->fname_len = 0;
    reqs->mode_len = 0;

    /* Search the end of Filename */
    for (pkt_ok=0, i=pkb->size; i > 0; i--) {
        if (*(reqs->Mode) == '\0') {
            pkt_ok = 1;
            reqs->Mode++; /* next octed is the start of Mode */
            break;
        }
        reqs->fname_len++;
        reqs->Mode++;
    }
    if (pkt_ok == 0) {
        P_WARNING("The packet seemed to broken.\n");
        safe_free(reqs);
        return NULL;
    }

    /* check pkt->Mode is really terminated. */
    for (pkt_ok=0, walk=reqs->Mode ;i > 0; i--) {
        if (*(walk) == '\0') {
            pkt_ok = 1;
            break;
        }
        reqs->mode_len++;
        walk++;
    }
    if (pkt_ok == 0) {
        P_WARNING("The packet seemed to broken.\n");
        safe_free(reqs);
        return NULL;
    }

    P_DEBUG("Requsted file name is \"%s\".\n", reqs->Filename);
    P_DEBUG("Transfer mode is \"%s\".\n", reqs->Mode);

    return reqs;
}

static int
check_filest(char *fname)
{
    int st_ok, retval;
    struct stat st;

    st_ok = stat(fname, &st);

    if (st_ok < 0) {
        switch (errno) {
            case ENOENT:
                retval = FILE_ST_ENOENT;
                break;
            case EACCES:
                retval = FILE_ST_EACCES;
                break;
            default:
                retval = FILE_ST_UNKNOWN;
                break;
        }

        return retval;
    }

    P_DEBUG("file size is %d bytes.\n", st.st_size);
    if (st.st_size > TFTP_FILE_MAX_SIZE) {
        return FILE_ST_TOOBIG;
    }
    else if (S_ISREG(st.st_mode) == 0) {
        return FILE_ST_EREG;
    }

    return FILE_ST_OK;
}

static int check_pktlen(int opcode, int size)
{
    /* see proto_tftp.h: tftp_opcode */
    int min_table[] = {
        0,                               /* null */
        TFTP_HDLEN + TFTP_REQ_HDLEN + 2, /* RRQ */
        TFTP_HDLEN + TFTP_REQ_HDLEN + 2, /* WRQ */
        TFTP_HDLEN + TFTP_DATA_HDLEN,    /* DATA */
        TFTP_HDLEN + TFTP_ACK_HDLEN,     /* ACK */
        TFTP_HDLEN + TFTP_ERR_HDLEN + 1, /* ERR */
    };

    if (size < min_table[opcode]) {
        P_DEBUG("Packet len %d < minimam len %d.\n", 
                size, min_table[opcode]);
        return 0;
    }

    return 1;
}
