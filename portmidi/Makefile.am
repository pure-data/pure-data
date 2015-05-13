# this is built as a "libtool convenience library"
# http://sources.redhat.com/automake/automake.html#Libtool-Convenience-Libraries
AUTOMAKE_OPTIONS = foreign
AM_CFLAGS = -DNEWBUFFER
INCLUDES = -Ipm_common -Iporttime

noinst_LTLIBRARIES = libportmidi.la

libportmidi_la_SOURCES = pm_common/pmutil.c pm_common/portmidi.c

if LINUX
INCLUDES +=  -Ipm_linux
libportmidi_la_SOURCES += porttime/ptlinux.c \
		pm_linux/pmlinux.c \
		pm_linux/pmlinuxalsa.c
endif

if MACOSX
INCLUDES +=  -Ipm_mac
libportmidi_la_SOURCES += porttime/ptmacosx_cf.c \
        pm_mac/pmmac.c \
        pm_mac/pmmacosxcm.c \
        pm_mac/finddefault.c \
        pm_mac/readbinaryplist.c
endif

if WINDOWS
INCLUDES +=  -Ipm_win
libportmidi_la_SOURCES += porttime/porttime.c \
        porttime/ptwinmm.c \
        pm_win/pmwin.c \
        pm_win/pmwinmm.c
endif

# include the headers in the dist so you can build
nobase_noinst_HEADERS = \
	pm_common/pminternal.h \
	pm_common/pmutil.h \
	pm_common/portmidi.h \
	pm_linux/pmlinux.h \
	pm_linux/pmlinuxalsa.h \
	pm_mac/pmmac.h \
	pm_mac/pmmacosxcm.h \
	pm_win/pmdll.h \
	pm_win/pmwinmm.h \
	porttime/porttime.h
