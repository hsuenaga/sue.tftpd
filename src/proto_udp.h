/*
   sue.tftpd:
   $Id: proto_udp.h,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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
#ifndef __PROTO_UDP_H__
#define __PROTO_UDP_H__
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "pkt_buff.h"
#include "task.h"

#ifdef IP_PKTINFO
#  ifdef HAVE_IP_PKTINFO
#    define DSTADDR_OPT IP_PKTINFO
#  else
#    undef DSTADDR_OPT
#  endif
#elif defined(IP_RECVDSTADDR)
struct in_pktinfo {
    struct in_addr ipi_addr;
    struct in_addr ipi_spec_dst;
    unsigned int ipi_ifindex;
};
#  define DSTADDR_OPT IP_RECVDSTADDR
#else
#  undef DSTADDR_OPT
#endif

int open_portal(char *host, char *serv);
void close_portal(int sockfd);
int
open_transfer(struct sockaddr *local, struct sockaddr *dest, socklen_t addrlen);
void close_transfer(int sockfd);

int udp_input(TASK *task);
int udp_output(int sockfd, struct pkt_buff *pkb);
void udp_report(void);

enum udp_recv_params {
    RECV_BUFSIZE = 1500,
};

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __PROTO_UDP_H__ */
