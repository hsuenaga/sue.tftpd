/*
   sue.tftpd:
   $Id: pkt_buff.c,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "pkt_buff.h"
#include "proto_tftp.h"
#include "util.h"
#include "debug.h"

#ifndef DEBUG_PKT_BUFF
#  undef P_DEBUG
#  define P_DEBUG(fmt...) /* null */
#endif
/*
 * file scope variables
 */
static unsigned int PkbCounter = 0;

/*
 * Exported functions
 */
struct pkt_buff *
pkb_alloc(size_t size)
{
    struct pkt_buff *pkb;

    pkb = (struct pkt_buff *)safe_malloc(PKB_HDLEN + size);
    if (pkb == NULL) {
        P_WARNING("safe_malloc() failed : %s.\n", strerror(errno));
        return NULL;
    }
    pkb->size = size;
    /* Don't forget memset.
       TCPv2 pp731-731:
       ... ifa_ifwithaddr does a binary comparison of the entire structure...
     */
    pkb->laddr = (struct sockaddr *) safe_malloc(SALEN_MAX);
    if (pkb->laddr == NULL) {
        P_WARNING("safe_malloc() failed.");
        return NULL;
    }
    memset(pkb->laddr, 0, SALEN_MAX);
    pkb->caddr = (struct sockaddr *) safe_malloc(SALEN_MAX);
    if (pkb->caddr == NULL) {
        P_WARNING("safe_malloc() failed.");
        safe_free(pkb);
        return NULL;
    }
    memset(pkb->caddr, 0, SALEN_MAX);
    pkb->addrlen = SALEN_MAX;

    PkbCounter++;

    P_DEBUG("PKB alloc. Total %d block(s).\n", PkbCounter);

    return pkb;
}

void
pkb_free(struct pkt_buff *pkb)
{
    if (pkb == NULL) return;

    if (PkbCounter == 0) {
        P_WARNING("pkb_free() called, but no buffer allocated. BUG?\n");
        return;
    }

    if (pkb->laddr != NULL) safe_free(pkb->laddr);
    if (pkb->caddr != NULL) safe_free(pkb->caddr);
    safe_free(pkb);

    PkbCounter--;
    P_DEBUG("PKB free. Total %d block(s).\n", PkbCounter);

    return;
}

int
pkb_setaddr(struct pkt_buff *pkb, struct sockaddr *caddr, socklen_t addrlen)
{
    if (pkb == NULL) {
        P_WARNING("Invalid packet buffer specified.\n");
        return 0;
    }
    if (caddr == NULL) {
        P_WARNING("Invalid addr specified.\n");
        return 0;
    }
    if (addrlen == 0) {
        P_WARNING("Invalid addrlen specified.\n");
        return 0;
    }
    if (addrlen > SALEN_MAX) {
        P_WARNING("addr length too long.\n");
        return 0;
    }

    memcpy(pkb->caddr, caddr, addrlen);
    pkb->addrlen = addrlen;

    return 1;
}

void
pkb_dump(struct pkt_buff *pkb)
{
    int written, rest;
    size_t off;
    unsigned char buf[PRINT_BUFSIZE];
    unsigned char *p;

    P_DEBUG("---> Dumping Pakcet ....\n");

    p = pkb->payload;
    off = 0;

    while (off < pkb->size) {
        int i, j;

        rest = PRINT_BUFSIZE;
        written = 0;

        for(i = 0; i < MAX_COLUMN; i++) {
            j = sprintf(buf + written, "%02x ", *(p + off));
            if (j < 0) {
                P_DEBUG("sprintf() failed: %s.", strerror(errno));
                return;
            }

            written += j;
            rest -= j;
            off++;

            if (rest < 0) {
                P_DEBUG("Buffer exhausted.\n");
                return;
            }
            if (off >= pkb->size) break;
        }

        *(buf + written) = '\0';
        P_DEBUG("%s\n", buf);
    }

    P_DEBUG("<--- Dumping Pakcet Done\n");

    return;
}

void
pkb_report(void)
{
    P_INFO("--- pkb statics ---\n");
    P_INFO(" PKB      Counter = %d\n", PkbCounter);

    return;
}

