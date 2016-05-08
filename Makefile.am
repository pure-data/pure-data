AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4/generated -I m4

CFLAGS += @DEFS@
EXTRA_SUBDIRS = 

# pkg-config support
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pd.pc

# optional apis
if ASIO
EXTRA_SUBDIRS += asio
endif

if LOCAL_PORTAUDIO
EXTRA_SUBDIRS += portaudio
endif

if LOCAL_PORTMIDI
EXTRA_SUBDIRS += portmidi
endif

# optionally build localizations
BUILT_SOURCES = 
if MSGFMT
EXTRA_SUBDIRS += po
BUILT_SOURCES += locales
endif

# files that are included but not built
EXTRA_DIST = LICENSE.txt README.txt INSTALL.txt

# subdirs that are built
SUBDIRS = $(EXTRA_SUBDIRS) src mac man doc tcl extra

# subdirs that are always included in the dist, etc.
DIST_SUBDIRS = asio doc extra mac man po portmidi portaudio src tcl

.PHONY: app

# build localizations
locales:
	make -C po all

# forward target to mac/Makefile
app: 
	make -C mac $(AM_MAKEFLAGS) app

# remove left over folders
uninstall-hook:
	if test -d $(libdir)/pd; then rm -rf $(libdir)/pd; fi
