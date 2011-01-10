/*
   sue.tftpd:
   $Id: pkt_buff.h,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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
#ifndef __PKT_BUFF_H__
#define __PKT_BUFF_H__
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* Packet buffer including dest/src infomation */
struct pkt_buff {
    size_t size;                /* size of payload */
    socklen_t addrlen;          /* length of sockaddr structure */
    struct sockaddr *laddr;     /* sockaddr for local address */
    struct sockaddr *caddr;     /* sockaddr for client if not connect()'ed */

    u_int8_t payload[0];        /* data gram body */
};
#define PKB_HDLEN (sizeof(size_t) + sizeof(socklen_t) + \
                    sizeof(struct sockaddr *))

/* exported functions */
struct pkt_buff *pkb_alloc(size_t size);
void pkb_free(struct pkt_buff *pkb);
int pkb_setaddr(struct pkt_buff *pkb,
                 struct sockaddr *caddr, socklen_t caddrlen);
void pkb_dump(struct pkt_buff *pkb);
void pkb_report(void);

enum pkb_dump_param {
    MAX_COLUMN = 8,
    PRINT_BUFSIZE = MAX_COLUMN * 3 + 1,
};

#if 0
#  define SALEN_MAX (sizeof(struct sockaddr_in6))
#else
#  define SALEN_MAX 1024
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __PKT_BUFF_H__ */
