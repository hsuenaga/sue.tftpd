/*
   sue.tftpd:
   $Id: util.h,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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
#ifndef __UTIL_H__
#define __UTIL_H__
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 * Functions for logging
 */
int log_init(FILE *fp);

#if (__GNUC__ >= 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96))
#pragma GCC system_header
#endif

#ifdef __GNUC__
#  define GNUC_VA_ARGS
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#  define C99_VA_ARGS
#else
#  define NO_VA_ARGS
#endif

#if defined(GNUC_VA_ARGS) || defined(C99_VA_ARGS)
int __vprefix_p(const char *prefix, const char *fname, const char *func, unsigned int line, const char *fmt, ...);
#endif

#ifdef GNUC_VA_ARGS
/* gcc's variable arguments macros */
#  define P_INFO(fmt...) \
          __vprefix_p(NULL, NULL, NULL, 0, fmt)
#  define P_WARNING(fmt...) \
          __vprefix_p("WARNING", NULL, __FUNCTION__, 0, fmt)
#  define P_ERROR(fmt...) \
          __vprefix_p("ERROR  ", NULL, __FUNCTION__, 0, fmt)
#  ifdef DEBUG
#    define P_DEBUG(fmt...) \
        __vprefix_p("DEBUG  ", __FILE__, __FUNCTION__, __LINE__, fmt)
#  else /* DEBUG */
#    define P_DEBUG(fmt...) /* null */
#  endif /* DEBUG */
#elif defined(C99_VA_ARGS)
/* C99's variable arguments macros ( __inline__ is better? ) */
#  define P_INFO(...) \
          __vprefix_p(NULL, NULL, NULL, 0, __VA_ARGS__)
#  define P_WARNING(...) \
          __vprefix_p("WARNING", NULL, NULL, 0, __VA_ARGS__)
#  define P_ERROR(...) \
          __vprefix_p("ERROR  ", NULL, NULL, 0, __VA_ARGS__)
#  ifdef DEBUG
#    define P_DEBUG(...) \
        __vprefix_p("DEBUG  ", __FILE__, NULL, __LINE__, __VA_ARGS__)
#  else /* DEBUG */
#    define P_DEBUG(...) /* null */
#  endif /* DEBUG */
#else /* defined(C99_VA_ARGS) */
/* We can't use variable arguments macro, so declare those functions */
int P_INFO(const char *fmt, ...);
int P_WARNING(const char *fmt, ...);
int P_ERROR(const char *fmt, ...);
int P_DEBUG(const char *fmt, ...);
#endif

int is_socket(int sockfd);
void *safe_malloc(size_t size);
void safe_free(void *ptr);
char *strsockaddr(struct sockaddr *sa, socklen_t salen);
char *strsockfd(int sockfd);
char *strin_addr(struct in_addr *addr);
void util_report(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __UTIL_H__ */
