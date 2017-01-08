# this is built as a "libtool convenience library"
# http://sources.redhat.com/automake/automake.html#Libtool-Convenience-Libraries

#########################################
##### Defaults & Paths #####

AUTOMAKE_OPTIONS = foreign
AM_CFLAGS = -DNEWBUFFER
AM_CPPFLAGS += \
    -I$(top_srcdir)/portmidi/portmidi/pm_common \
    -I$(top_srcdir)/portmidi/portmidi/porttime

#########################################
##### Files, Binaries, & Libs #####

noinst_LTLIBRARIES = libportmidi.la

libportmidi_la_SOURCES = \
    portmidi/pm_common/pmutil.c \
    portmidi/pm_common/portmidi.c \
    portmidi/porttime/porttime.c

if LINUX
AM_CPPFLAGS += -I$(top_srcdir)/portmidi/portmidi/pm_linux
libportmidi_la_SOURCES += \
    portmidi/porttime/ptlinux.c \
    portmidi/pm_linux/pmlinux.c \
    portmidi/pm_linux/pmlinuxalsa.c
endif

if MACOSX
AM_CPPFLAGS += -I$(top_srcdir)/portmidi/portmidi/pm_mac
libportmidi_la_SOURCES += \
    portmidi/porttime/ptmacosx_cf.c \
    portmidi/pm_mac/pmmac.c \
    portmidi/pm_mac/pmmacosxcm.c \
    portmidi/pm_mac/finddefault.c \
    portmidi/pm_mac/readbinaryplist.c
endif

if WINDOWS
AM_CPPFLAGS += -I$(top_srcdir)/portmidi/portmidi/pm_win
libportmidi_la_SOURCES += \
    portmidi/porttime/ptwinmm.c \
    portmidi/pm_win/pmwin.c \
    portmidi/pm_win/pmwinmm.c
endif

# include the headers in the dist so you can build
nobase_noinst_HEADERS = \
    portmidi/pm_common/pminternal.h \
    portmidi/pm_common/pmutil.h \
    portmidi/pm_common/portmidi.h \
    portmidi/pm_linux/pmlinux.h \
    portmidi/pm_linux/pmlinuxalsa.h \
    portmidi/pm_mac/pmmac.h \
    portmidi/pm_mac/pmmacosxcm.h \
    portmidi/pm_mac/readbinaryplist.h \
    portmidi/pm_win/pmwinmm.h \
    portmidi/porttime/porttime.h
