# this is built as a static convenience library

#########################################
##### Defaults & Paths #####

AUTOMAKE_OPTIONS = foreign
AM_CFLAGS = -DNEWBUFFER
AM_CPPFLAGS += \
    -I$(top_srcdir)/portmidi/portmidi/pm_common \
    -I$(top_srcdir)/portmidi/portmidi/porttime

#########################################
##### Files, Binaries, & Libs #####

noinst_LIBRARIES = libportmidi.a

libportmidi_a_SOURCES = \
    portmidi/pm_common/pmutil.c \
    portmidi/pm_common/portmidi.c

if LINUX
AM_CPPFLAGS += -I$(top_srcdir)/portmidi/portmidi/pm_linux
libportmidi_a_SOURCES += \
    portmidi/porttime/ptlinux.c \
    portmidi/pm_linux/pmlinux.c \
    portmidi/pm_linux/pmlinuxalsa.c
endif

if MACOSX
AM_CPPFLAGS += -I$(top_srcdir)/portmidi/portmidi/pm_mac
libportmidi_a_SOURCES += \
    portmidi/porttime/ptmacosx_mach.c \
    portmidi/pm_mac/pmmac.c \
    portmidi/pm_mac/pmmacosxcm.c \
    portmidi/pm_mac/finddefault.c \
    portmidi/pm_mac/readbinaryplist.c
endif

if WINDOWS
AM_CPPFLAGS += -I$(top_srcdir)/portmidi/portmidi/pm_win
libportmidi_a_SOURCES += \
    portmidi/porttime/ptwinmm.c \
    portmidi/pm_win/pmwin.c \
    portmidi/pm_win/pmwinmm.c
endif

# empty var for headers list footer
empty=

# include the headers in the dist so you can build
# find portmidi -type file -name *.h | sort | awk '{print "   ", $1, "\\"}'; echo '     $(empty)'
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
    portmidi/porttime/porttime.h \
     $(empty)
