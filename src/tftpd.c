/*
   sue.tftpd:
   $Id: tftpd.c,v 1.1.1.1 2008/08/01 05:31:04 hsuenaga Exp $

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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "proto_udp.h"
#include "proto_tftp.h"
#include "task.h"
#include "util.h"
#include "tftpd.h"
#include "debug.h"
#define GLOBAL_DEFINE
#include "globals.h"
#undef GLOBAL_DEFINE

#ifndef DEBUG_TFTPD
#  undef P_DEBUG
#  define P_DEBUG(fmt...) /* null */
#endif
/* forward delarations of private functions */
void print_version(void);
void print_help(void);

/*
 * Exported functions.
 */
int
main(int argc, char *argv[])
{
    int chroot_ok, chdir_ok, daemon_ok;
    int task_err, nodaemon = 0, nochroot = 1, noclose = 1;
    pid_t pid;
    TASK *portal_task;
    FILE *flog = NULL, *fpid = NULL;
    char *Root_dir = NULL, *Log_file = LOGFILE, *Pid_file = PIDFILE;
    extern char *optarg;
    extern int optind, opterr, optopt;


    printf("hogehgoe\n");
    
    /* environment setup */
    TFTP_Address = NULL;
    TFTP_Port = "69";
    for (;;) {
        int c;

        c = getopt(argc, argv, "l:p:P:r:Dhv");

        if (c == -1) break;

        switch (c) {
            case 'l':
                Log_file = optarg;
                break;
            case 'p':
                TFTP_Port = optarg;
                break;
            case 'P':
                Pid_file = optarg;
                break;
            case 'r':
                Root_dir = optarg;
                break;
            case 'D':
                nodaemon = 1;
                break;
            case 'h':
                print_help();
                return 0;
                break;
            case 'v':
                print_version();
                return 0;
                break;
            default:
                fprintf(stderr, "Error. Unkown option %c\n\n", c);
                print_help();
                return 1;
                break;
        }
    }

    /* -r MUST specified. */
    if (nodaemon == 0) {
        if (Root_dir == NULL) {
            print_help();
            return 1;
        }
    }

    if (nodaemon == 1) Log_file = NULL;

    if (Log_file == NULL) {
        flog = NULL;
    } else {
        flog = fopen(Log_file, "a");
        if (flog == NULL) {
            fprintf(stderr, "Can't open logfile: %s.\n", strerror(errno));
        }
    }
    if (flog == NULL) noclose = 1; /* XXX: noclose alway 1 */
    log_init(flog); /* At now, logging function is usable. */

    P_INFO("%s version %s\n", PACKAGE, VERSION);

    if (nodaemon == 0) {
        fpid = fopen(Pid_file, "w");
        if (fpid == NULL) {
            P_ERROR("Can't open pidfile: %s.\n", strerror(errno));
            goto Error;
        }
    }

    /* environment setup done. */

    P_DEBUG("Initializeing TASK\n");
    task_init();
    /*
     * task_new MUST called before chroot() because it refers
     * /etc/services.
     */
    P_DEBUG("Add portal task\n");
    portal_task = task_create(TASK_TYPE_PORTAL, NULL, NULL, 0);
    if (portal_task == NULL) {
        P_ERROR("task_new() failed. Unable to start service. \n");
        goto Error;
    }

    if (nodaemon == 0) {
        chdir_ok = chdir(Root_dir);
        if (chdir_ok < 0) {
            P_ERROR("chdir() failed: %s\n", strerror(errno));
            goto Error;
        }
        chroot_ok = chroot(".");
        if (chroot_ok < 0) {
            P_ERROR("chroot() failed: %s\n", strerror(errno));
            goto Error;
        }
    }

    if (nodaemon == 0) {
        daemon_ok = daemon(nochroot, noclose);
        if (daemon_ok < 0) {
            P_ERROR("daemon() failed: %s.\n", strerror(errno));
            goto Error;
        }
        pid = getpid();
        fprintf(fpid, "%d", pid);
        fclose(fpid);
    }

    P_INFO("Starting service...\n");
    task_err = task_main(); /* infinite loop */
    
    if (task_err == 0) {
        P_ERROR("task_switch() returns error.\n");
        task_join(portal_task, TASK_EXIT_ERROR);
        goto Error;
    }

Error:
    fprintf(stderr, "Error occured. see log file for more infomation.\n");
    fprintf(stderr, "exitting..\n");
    return 1;
}

/*
 * Private functions
 */
void
print_version(void)
{
    printf("%s version %s\n", PACKAGE, VERSION);

    return;
};

void
print_help(void)
{
    print_version();

    printf(
           "Usage:\n"
           "  %s -r <directory> [options...]\n" 
           "\n"
           "Options:\n"
           "  -l <logfile>   ... specify logfile.\n"
           "  -P <pidfile>   ... specify pidfile.\n"
           "  -r <directory> ... directory to chdir()\n"
           "  -p <port>      ... specify port number.\n"
           "  -D             ... debug mode. don't daemon().\n"
           "  -h             ... print help (this)\n"
           "  -v             ... print version\n" 
           "\n"
           "  -r MUST specified because of security reason.\n",
           PACKAGE
           );

    return;
};
