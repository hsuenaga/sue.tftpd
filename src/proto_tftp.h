/*
   sue.tftpd:
   $Id: proto_tftp.h,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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
#ifndef __PROTO_TFTP_H__
#define __PROTO_TFTP_H__
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <sys/types.h>

#include "pkt_buff.h"
#include "task.h"

/* From RFC1350 Page 6 */ 
struct tftp_req {
    u_int8_t string[0];         /* We must find out the separater '\0' */
};
#define TFTP_REQ_HDLEN (0)

struct tftp_req_str {
    size_t fname_len;
    size_t mode_len;
    char *Filename;
    char *Mode;
};

/* From RFC1350 Page 7 */ 
struct tftp_data {
    u_int16_t BlockN;
    u_int8_t  Data[0];          /* Max 512 bytes long */
};
#define TFTP_DATA_HDLEN (sizeof(u_int16_t))

/* From RFC1350 Page 7 */
struct tftp_ack {
    u_int16_t BlockN;
};
#define TFTP_ACK_HDLEN (sizeof(u_int16_t))

/* From RFC1350 Page 8 */
struct tftp_err {
    u_int16_t ErrorCode;
    u_int8_t ErrMsg[0];           /* null terminated. */
};
#define TFTP_ERR_HDLEN (sizeof(u_int16_t))

/* Generic tftp packet structure */
struct tftp_pkt {
    u_int16_t Opcode;
    u_int8_t payload[0];          /* tftp payload */
};
#define TFTP_HDLEN (sizeof(u_int16_t))

/* useful macros */
/* Cast pkt_buff to tftp packet type */
#define PKB_TO_TFTP(pkb)  ((struct tftp_pkt *) &(pkb->payload))

/* Cast tftp_pkt to message type */
#define TFTP_TO_REQ(tpkt)  ((struct tftp_req *)  &(tpkt->payload))
#define TFTP_TO_DATA(tpkt) ((struct tftp_data *) &(tpkt->payload))
#define TFTP_TO_ACK(tpkt)  ((struct tftp_ack *)  &(tpkt->payload))
#define TFTP_TO_ERR(tpkt)  ((struct tftp_err *)  &(tpkt->payload))

/* From RFC1350 Page 4 */
enum tftp_opcode {
    TFTP_RRQ   = 0x01,      /* Read ReQuest */
    TFTP_WRQ   = 0x02,      /* Write ReQuest */
    TFTP_DATA  = 0x03,      /* Data */
    TFTP_ACK   = 0x04,      /* Ack */
    TFTP_ERROR = 0x05,      /* Error */
    TFTP_LAST
};

/* From RFC1350 Page 10 */
enum tftp_errcode {
    TFTP_ENDEF       = 0x00, /* Not defined, see error message(if any). */
    TFTP_ENOENT      = 0x01, /* File not found */
    TFTP_EACCES      = 0x02, /* Access violation */
    TFTP_ENOSPC      = 0x03, /* Disk full or allocation exceeded */
    TFTP_EILLEGAL    = 0x04, /* Illegal TFTP operation */
    TFTP_ETID        = 0x05, /* Unknown tranfer ID */
    TFTP_EEXIST      = 0x06, /* File already exists */
    TFTP_ENOUSER     = 0x07, /* No such user. */

    /* following definition is internal use only */
    TFTP_ENONE,              /* No error detected. */
    TFTP_ELAST
};

int tftp_input(TASK *task, struct pkt_buff *pkb);
int tftp_output(int sockfd, struct pkt_buff *pkb);
int tftp_retrans(TASK *task);
void tftp_report(void);

enum tftp_params {
    TFTP_DATA_MAX_SIZE = 512,            /* max size of data packet's payload */
    TFTP_FILE_MAX_SIZE = 65535 * 512 - 1,/* Max file size */
    RETRANS_MAX = 5,                     /* max retransmit packet */
    RETRANS_INIT_INTERVAL = 500 * 1000,  /* initial retrans interval [us] */
    RETRANS_BACKOFF_FACTOR = 2,          /* backoff factor */
};

/*
 * NOTE:
 *
 * - TFTP_DATA_MAX_SIZE
 *     In RFC1350, Payload size of TFTP data packet is 512 [bytes].
 *   So you never change this except you build your own protocol.
 *     proto_private.h define Max size of retrans buffer. If you change
 *   TFTP_DATA_MAX_SIZE you should change it.
 *
 * - TFTP_FILE_MAX_SIZE
 *     In RFC1350, block number of TFTP data packet is 16bit width.
 *   And block number MUST unique. So max file size is limited to
 *   65535 * 512 - 1 [bytes]. Remember that block number start from 1 
 *   and last block size < 512.
 *
 * - Retransmit timing
 *     In RFC1123, TFTP MUST support exponential back off.
 *   But RFC1123 doesn't specify actual parameters.
 *   Any users should redefine retransmit timings to fit to
 *   there site.
 *     task.h specify select() timeout parameter. If you change
 *   retransmit timigs, you should also change it.
 *
 *
 * [Refernces]
 *   RFC764  Telenet Protocol Specification
 *   RFC1123 Requirements for Internet Hosts -- Applications and Support
 *   RFC1350 TFTP Revision 2
 */
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __PROTO_TFTP_H__ */
