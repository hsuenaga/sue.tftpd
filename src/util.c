/*
   sue.tftpd:
   $Id: util.c,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>

#include "pkt_buff.h"
#include "util.h"
#include "debug.h"

#define IN_ADDR_STRLEN 16 /* strlen("xxx.xxx.xxx.xxx") + 1 */

/*
 * file scope variables
 */
static FILE *Log_fp = NULL;
static int MallocCounter = 0;

/*
 * forward delarations of private functions.
 */ 
static int
__vprefix_p0(const char *prefix, const char *fname, const char *func, unsigned int line, const char *fmt, va_list ap);

/*
 * Exported functions
 */
int
log_init(FILE *fp) {

    if (fp == NULL) {
         Log_fp = stderr;
        P_INFO("Logging to stdout ...\n");
        return 0;
    }

    Log_fp = fp;

    return 1;
}

#if defined(GNUC_VA_ARGS) || defined(C99_VA_ARGS)
int __vprefix_p(const char *prefix, const char *fname, const char *func,
		unsigned int line, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    __vprefix_p0(prefix, fname, func, line, fmt, ap);
    va_end(ap);

    return 1;
}
#else
int P_INFO(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    __vprefix_p0(NULL, NULL, NULL, 0, fmt, ap);
    va_end(ap);

    return 1;
}

int P_WARNING(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    __vprefix_p0("WARNING", NULL, NULL, 0, fmt, ap);
    va_end(ap);

    return 1;
}

int P_ERROR(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    __vprefix_p0("ERROR  ", NULL, NULL, 0, fmt, ap);
    va_end(ap);

    return 1;
}

int P_DEBUG(const char *fmt, ...)
{
    va_list ap;

#ifdef DEBUG
    va_start(ap, fmt);
    __vprefix_p0("DEBUG  ", NULL, NULL, 0, fmt, ap);
    va_end(ap);
#endif

    return 1;
}
#endif /* !defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L */

int
is_socket(int sockfd)
{
    int err;
    struct stat st;

    if (sockfd < 0) return 0;

    err = fstat(sockfd, &st);
    if (err < 0) {
        P_WARNING("fstat() failed: %s.\n", err);
        return 0;
    }

    if (S_ISSOCK(st.st_mode) == 1) {
        return 1;
    } else {
        return 0;
    }
}

void *
safe_malloc(size_t size)
{
    void *p;

    p = malloc(size);
    if (p == NULL) return NULL;
    MallocCounter++;
    P_DEBUG("safe_malloc() called. total %d blocks.\n", MallocCounter);

    return p;
}

void
safe_free(void *ptr)
{
    if (MallocCounter <= 0) {
        P_DEBUG("safe_free() called, but buffer not allocated. BUG?\n");
        return;
    }

    free(ptr);
    MallocCounter--;
    P_DEBUG("safe_free() called. total %d blocks.\n", MallocCounter);

    return;
}

char *
strsockaddr(struct sockaddr *sa, socklen_t salen)
{
    static char buffer[512];
    char host[256];
#ifndef HAVE_GETADDRINFO
    void *addrp;
    const char *p;
#else
    int result;
#endif

#ifdef HAVE_GETADDRINFO
    result = getnameinfo(sa, salen, host, 256, NULL, 0, NI_NUMERICHOST);
    if (result != 0) {
        if (result == EAI_SYSTEM) 
            P_WARNING("getnameinfo() failed: %s.\n", strerror(errno));
        else
            P_WARNING("getnameinfo() failed: %s.\n", gai_strerror(result));
        
        return NULL;
    }
#elif defined(HAVE_INET_PTON)
    if (sa->sa_family == AF_INET)
        addrp = (void *) &((struct sockaddr_in *)sa)->sin_addr;
#ifdef AF_INET6
    else if (sa->sa_family == AF_INET6)
        addrp = (void *) &((struct sockaddr_in6 *)sa)->sin6_addr;
#endif
    else {
        P_WARNING("unknown family %d.\n", sa->sa_family);
        return NULL;
    }

    p = inet_ntop(sa->sa_family, addrp, host, sizeof(host));
    if (p == NULL) {
        P_WARNING("inet_ntop() failed: %s.\n", strerror(errno));
        return NULL;
    }
#else
    if (sa->sa_family != AF_INET) {
        P_WARNING("unknown family %d.\n", sa->sa_family);
        return NULL;
    }
    addrp = (void *) &((struct sockaddr_in *)sa)->sin_addr;
    p = inet_ntoa(addrp);
    if (p == NULL) {
        P_WARNING("inet_ntoa() failed: %s.\n", strerror(errno));
        return NULL;
    }
    strncpy(host, p, sizeof(host));
#endif
    snprintf(buffer, 512, "%s", host);

    return buffer;
}

char *
strsockfd(int sockfd)
{
    char *p;
    socklen_t salen;
    struct sockaddr *sa;

    if (is_socket(sockfd) == 0) {
        P_WARNING("sockfd %d is not a socket.\n");
        return NULL;
    }

    salen = SALEN_MAX;
    sa = (struct sockaddr *)malloc(salen);
    if (sa == NULL) {
        P_WARNING("malloc() failed.\n");
        return NULL;
    }

    if (getsockname(sockfd, sa, &salen) < 0) {
        P_WARNING("getsockname() failed: %s.\n", strerror(errno));
        return NULL;
    }

    p = strsockaddr(sa, salen);
    if (p == NULL) {
        P_WARNING("strsockaddr() returns NULL.\n");
    }

    return p;
}

char *
strin_addr(struct in_addr *addr)
{
    static char buf[IN_ADDR_STRLEN];
    u_int8_t *p;

    p = (u_int8_t *) addr;

    snprintf(buf, IN_ADDR_STRLEN, "%3d.%3d.%3d.%3d",
            p[0], p[1], p[2], p[3]);

    return buf;
}

void
util_report(void)
{
    P_INFO("--- util statics ---\n");
    P_INFO(" Malloc   Counter = %d\n", MallocCounter);

    return;
}

/*
 * Private functions
 */
static int
__vprefix_p0(const char *prefix, const char *fname, const char *func,
		unsigned int line, const char *fmt, va_list ap)
{
    int p_success;
    struct tm *t;
    struct timeval tv;
    char *mstr[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    gettimeofday(&tv, NULL);
    t = localtime((time_t *)(&tv.tv_sec));

    p_success = fprintf(Log_fp, "%3s %02d %02d:%02d:%02d ", mstr[t->tm_mon], t->tm_mday,
                        t->tm_hour, t->tm_min, t->tm_sec);
    if (p_success <= 0) {
        fprintf(stderr, "fpirntf() failed: %s.", strerror(errno));
    }

    if (prefix) fprintf(Log_fp, "%s ", prefix);

    if (fname) {
        fprintf(Log_fp, "[%s", fname);
        if (line) fprintf(Log_fp, ":%d", line);
        fprintf(Log_fp, "] ");
    }

    if (func) fprintf(Log_fp, "%s(): ", func);

    vfprintf(Log_fp, fmt, ap);

    fflush(Log_fp);

    return 1;
}

