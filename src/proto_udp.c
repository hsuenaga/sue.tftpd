/*
   sue.tftpd:
   $Id: proto_udp.c,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pkt_buff.h"
#include "proto_udp.h"
#include "proto_tftp.h"
#include "util.h"
#include "debug.h"

#ifndef DEBUG_PROTO_UDP
#  undef P_DEBUG(fmt...)
#  define P_DEBUG(fmt...) /* null */
#endif

/*
 * file scope variables
 */
static unsigned int InputCounter = 0;
static unsigned int OutputCounter = 0;

/* forward declarations of private function */
struct pkt_buff *udp_recv(int sockfd);

/*
 * Exported functions
 */
#ifdef HAVE_GETADDRINFO
int
open_portal(char *host, char *serv)
{
    int gai_errno = 0;
    int sockfd = 0, sock_err = 0;
    int bind_err = 0, bind_ok = -1;
    struct addrinfo *res, *walk;
    struct addrinfo hints = {
        AI_PASSIVE, /* ai_flags */
        PF_INET,    /* ai_family */
        SOCK_DGRAM, /* ai_socktype */
        0,          /* ai_protocol */
        0,          /* ai_addrlen : DON'T CHANGE */
        NULL,       /* ai_addr : DON'T CHANGE */
        NULL,       /* ai_canonname : DON'T CHANGE */
        NULL,       /* ai_next : DON'T CHANGE */
    };
#ifdef DSTADDR_OPT
    int on = 1;
#endif

    /* get server address infomation */
    P_DEBUG("Performing getaddrinfo...\n");
    gai_errno = getaddrinfo(host, serv, &hints, &res);
    if (gai_errno != 0) {
        if (gai_errno == EAI_SYSTEM) {
            P_WARNING("getaddrinfo() failed: %s\n", strerror(errno));
        } else {
            P_WARNING("getaddrinfo() failed: %s\n", gai_strerror(gai_errno));
        }
        P_WARNING("host name = %s\n", host);
        P_WARNING("servive name = %s\n", serv); 

        return -1;
    }

    /* search usefull server address */
    for(walk=res; walk != NULL; walk = walk->ai_next) {
        sockfd = socket(walk->ai_family, walk->ai_socktype, walk->ai_protocol);
        if (sockfd < 0) {
            P_DEBUG("socket() failed: %s.\n", strerror(errno));
            sock_err = errno;
            continue;
        } 
        bind_ok = bind(sockfd, walk->ai_addr, walk->ai_addrlen);
        if (bind_ok < 0) {
            P_DEBUG("bind() failed: %s.\n", strerror(errno));
            bind_err = errno;
            close(sockfd);
            continue;
        } else {
            bind_ok = 1;
            break;
        }
    }
    freeaddrinfo(res);

    if (sockfd < 0) {
        P_WARNING("Cannot create socket: %s.\n", strerror(sock_err));
        return -1;
    }
    else if (bind_ok < 0) {
        P_WARNING("Cannot create socket: %s\n", strerror(bind_err));
        return -1;
    }

#ifdef DSTADDR_OPT
    setsockopt(sockfd, IPPROTO_IP, DSTADDR_OPT, &on, sizeof(on));
    /* XXX: error handling */
#endif

    P_DEBUG("Socket created successfully.\n");
    P_DEBUG("Addres=%s\n", strsockaddr(walk->ai_addr, walk->ai_addrlen));
    return sockfd;
}
#else
int
open_portal(char *host, char *serv)
{
    int error;
    int af = AF_UNSPEC;
    int sockfd;
    struct sockaddr_in sin;
    struct in_addr addr4;
#if defined(HAVE_INET_PTON) && defined(HAVE_SOCKADDR_IN6)
    struct sockaddr_in6 sin6;
    struct in6_addr addr6;
#endif
    struct sockaddr* sa;
    socklen_t sa_len;
    uint16_t port;
    long n;
    char *end;
#ifdef DSTADDR_OPT
    int on = 1;
#endif

    if (host == NULL)
        host = "0.0.0.0";
    if (serv == NULL)
        serv = "69";

    P_DEBUG("opening socket %s:%s\n", host, serv);
    error = inet_aton(host, &addr4);
    if (error == 1) {
        af = AF_INET;
        memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
        sin.sin_len = sizeof(sin);
#endif
        memcpy(&sin.sin_addr, &addr4, sizeof(sin.sin_addr));
        sa = (struct sockaddr *)&sin;
        sa_len = sizeof(sin);
        goto get_port;
    }

#if defined(HAVE_INET_PTON) && defined(HAVE_SOCKADDR_IN6)
    error = inet_pton(AF_INET6, host, &addr6);
    if (error == 1) {
        af = AF_INET6;
        memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
#ifdef HAVE_SA_LEN
        sin6.sin6_len = sizeof(sin6);
#endif
        memcpy(&sin6.sin6_addr, &addr6, sizeof(sin6.sin6_addr));
        sa = (struct sockaddr *)&sin6;
        sa_len = sizeof(sin6);
        goto get_port;
    }
#endif

    P_WARNING("%s is not a valid address.\n", host);
    return -1;

get_port:
    n = strtol(serv, &end, 10);
    if (n < 0 || n > 0xffff) {
        P_WARNING("%s is not a valid port number(out of range)\n", serv);
        return -1;
    }
    port = (uint16_t)n;

    sockfd = socket(af, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        P_WARNING("socket() failed: %s.\n", strerror(errno));
        return -1;
    }

    error = bind(sockfd, sa, sa_len);
    if (error < 0) {
        P_WARNING("bind() failed: %s.\n", strerror(errno));
        return -1;
    }

#ifdef DSTADDR_OPT
    setsockopt(sockfd, IPPROTO_IP, DSTADDR_OPT, &on, sizeof(on));
#endif

    P_DEBUG("Socket created successfully.\n");
    P_DEBUG("Addres=%s\n", strsockaddr(sa, sa_len));
    return sockfd;
}
#endif /* HAVE_GETADDRINFO */

void
close_portal(int sockfd)
{
    if (is_socket(sockfd) == 0) {
        P_WARNING("fd %d is not a socket.\n", sockfd);
        return;
    }

    close(sockfd);
    P_DEBUG("Closing socket %d.\n", sockfd);
    return;
}

int
open_transfer(struct sockaddr *local, struct sockaddr *dest, socklen_t addrlen)
{
    int sockfd;
#ifdef DSTADDR_OPT
    int on = 1;
#endif

    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        P_WARNING("sockfd() failed: %s.\n", strerror(errno));
        return -1;
    }

#ifdef DSTADDR_OPT
    if ( setsockopt(sockfd, IPPROTO_IP, DSTADDR_OPT, &on, sizeof(on)) < 0) {
        P_WARNING("setsockopt() failed: %s.\n", strerror(errno));
        close(sockfd);
        return -1;
    }
#endif
    if (((struct sockaddr_in *)local)->sin_addr.s_addr != INADDR_ANY) {
        if ( bind(sockfd, local, addrlen) < 0) {
            P_WARNING("bind() failed: %s.\n", strerror(errno));
            close(sockfd);
            return -1;
        }
    }
    if ( connect(sockfd, dest, addrlen) < 0) {
        P_WARNING("connect() failed: %s.\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    return sockfd;
}

void
close_transfer(int sockfd)
{
    if (is_socket(sockfd) == 0) {
        P_WARNING("fd %d is not a socket.\n", sockfd);
        return;
    }

    close(sockfd);
    P_DEBUG("Closing socket %d.\n", sockfd);
    return;
}

int
udp_output(int sockfd, struct pkt_buff *pkb)
{
    int sendto_ok, retval = 1;

    if (is_socket(sockfd) == 0) {
        P_WARNING("fd %d is not a socket.\n", sockfd);
        return 0;
    }
    if (pkb == NULL) {
        P_WARNING("no packet specified.\n");
        return 0;
    }
    
    /* XXX: sockfd may be not task id */
    P_DEBUG("Task %d: Sending UDP %d octed packet.\n",
            sockfd, pkb->size);
    sendto_ok = send(sockfd, pkb->payload, pkb->size, 0);
    if (sendto_ok < 0) {
        P_WARNING("sendto() failed: %s\n", strerror(errno));
        retval = 0;
    }

    OutputCounter++;

    pkb_free(pkb);
    return retval;
}

int
udp_input(TASK *task)
{
    int input_ok, retval = 1;
    u_int16_t opcode;
    struct pkt_buff *pkb;
    struct tftp_pkt *tpkt;

    if (task == NULL) {
        P_WARNING("Invalid task specified.\n");
        return 0;
    }

    P_DEBUG("Task %d: Receiving packet.\n", task_get_id(task));
    pkb = udp_recv(task_get_sockfd(task));
    if (pkb == NULL) {
        P_WARNING("udp_recv() failed.\n");
        return 0;
    }

    if (pkb->size < TFTP_HDLEN) {
        P_WARNING("Pakcet too small.\n");
        P_WARNING("Packet discarded.\n");
        pkb_free(pkb);
        return 0;
    }

    tpkt = PKB_TO_TFTP(pkb);
    opcode = ntohs(tpkt->Opcode);

    if (opcode >= TFTP_LAST) {
        P_WARNING("Unknown protocol received. Packet discarded.\n");
        retval = 0;
    }
    input_ok = tftp_input(task, pkb);
    if (input_ok == 0) {
        P_WARNING("tftp_input() failed.\n");
        retval = 0;
    }

    if (retval == 0) pkb_free(pkb);
    return retval;
}

void
udp_report(void)
{

    P_INFO("--- UDP statics ---\n");
    P_INFO(" Input    Counter = %d\n", InputCounter);
    P_INFO(" Outout   Counter = %d\n", OutputCounter);

    return;
}


/*
 * Private functions
 */
struct pkt_buff *
udp_recv(int sockfd)
{
    int nrecv;
    struct pkt_buff *pkb;
    struct sockaddr_in *laddr, *caddr;
    socklen_t caddr_len;
#ifdef DSTADDR_OPT
    struct iovec iov[1];
    struct msghdr msg;
    struct cmsghdr *cmsgp;
    struct in_pktinfo pktinfo;
    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(struct in_pktinfo))];
    } control_un;
#endif

    /* error check */
    if (is_socket(sockfd) == 0) {
        P_WARNING("fd %d is not a socket.\n", sockfd);
        return NULL;
    }

    /* set up buffer */
    pkb = pkb_alloc(RECV_BUFSIZE);
    if (pkb == NULL) {
        P_WARNING("pkb_alloc() failed.\n");
        return NULL;
    }

    /* initialize variables */
    laddr = (struct sockaddr_in *) pkb->laddr;
    caddr = (struct sockaddr_in *) pkb->caddr;
#ifdef HAVE_SA_LEN
    laddr->sin_len = sizeof(struct sockaddr_in);
    caddr->sin_len = sizeof(struct sockaddr_in);
#endif
    laddr->sin_family = AF_INET;
    caddr->sin_family = AF_INET;
    caddr_len = sizeof(struct sockaddr_in);

#ifdef DSTADDR_OPT
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);
    msg.msg_flags = 0;
    msg.msg_name = (caddr_t) caddr;
    msg.msg_namelen = pkb->addrlen;
    iov[0].iov_base = pkb->payload;
    iov[0].iov_len = pkb->size;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    /* recv: to know destination addr of the packet, use recvmsg */
    nrecv = recvmsg(sockfd, &msg, 0);
    if (nrecv < 0) {
        P_WARNING("recvmsg() failed: %s.\n", strerror(errno));
        pkb_free(pkb);
        return NULL;
    }
#else
    nrecv = recvfrom(sockfd, pkb->payload, pkb->size, 0,
                     (struct sockaddr *)caddr, &caddr_len);
    if (nrecv < 0) {
        P_WARNING("recvfrom() failed: %s.\n", strerror(errno));
        pkb_free(pkb);
        return NULL;
    }
    if (caddr_len != sizeof(struct sockaddr_in)) {
        P_WARNING("cannot get client address.\n");
        pkb_free(pkb);
        return NULL;
    }
    laddr->sin_addr.s_addr = INADDR_ANY;
    laddr->sin_port = 0;
#endif

#ifdef DSTADDR_OPT
    /* parse control message */
    pkb->size = nrecv;
    pkb->addrlen = msg.msg_namelen;
    if (msg.msg_controllen < sizeof(struct cmsghdr)) {
        P_WARNING("msg buffer maybe exhauted.\n");
        pkb_free(pkb);
        return NULL;
    } 
    if  (msg.msg_flags & MSG_CTRUNC) {
        P_WARNING("msg controll header trancated.\n");
        pkb_free(pkb);
        return NULL;
    }

    for (cmsgp=CMSG_FIRSTHDR(&msg);cmsgp!=NULL;cmsgp=CMSG_NXTHDR(&msg,cmsgp)) {
        if (cmsgp->cmsg_level == IPPROTO_IP && cmsgp->cmsg_type==DSTADDR_OPT) {
#ifdef IP_PKTINFO
            memcpy(&pktinfo, CMSG_DATA(cmsgp), sizeof(struct in_pktinfo));
else
            memcpy(&pktinfo.ipi_addr, CMSG_DATA(cmsgp), sizeof(struct in_addr));
#endif
            memcpy(&laddr->sin_addr, &pktinfo.ipi_addr, sizeof(struct in_addr));
            P_DEBUG("msg controll header found.(%s)\n",
                    strin_addr(&laddr->sin_addr));
            break;
        }
    }
#endif

    InputCounter++;
    P_DEBUG("Task %d: UDP %d octets packet received.\n",
            sockfd, pkb->size);

    return pkb;
}
