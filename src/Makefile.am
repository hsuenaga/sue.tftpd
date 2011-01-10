## Process this file with automake to produce Makefile.in

sbin_PROGRAMS = sue.tftpd

sue_tftpd_SOURCES = \
    tftpd.c tftpd.h \
    pkt_buff.c pkt_buff.h \
    proto_udp.c proto_udp.h \
    proto_tftp.c proto_tftp.h \
    task.c task.h task_private.h \
    timer.c timer.h \
    util.c util.h \
    debug.h globals.h

sue_tftpd_LDFLAGS= -Wl,--enable-auto-import
