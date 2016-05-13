#########################################
##### Defaults & Paths #####

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4/generated -I m4

CFLAGS += @DEFS@
EXTRA_SUBDIRS = 

# pkg-config support
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pd.pc

#########################################
##### Files, Binaries, & Libs #####

##### ASIO #####
if ASIO
EXTRA_SUBDIRS += asio
endif

##### PortAudio #####
if LOCAL_PORTAUDIO
EXTRA_SUBDIRS += portaudio
endif

##### PortMidi #####
if LOCAL_PORTMIDI
EXTRA_SUBDIRS += portmidi
endif

##### Localizations #####
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

#########################################
##### Targets #####

.PHONY: app

# startup lock files (?)
install-data-local:
	$(INSTALL) -d $(DESTDIR)/$(libdir)/pd/startup
	$(INSTALL) -d $(DESTDIR)/$(libdir)/pd/startup/disabled

uninstall-local:
	rm -rf $(DESTDIR)/$(libdir)/pd/startup

# build localizations
locales:
	make -C po all

# forward target to mac/Makefile
app: 
	make -C mac app

# remove left over folders, does *not* remove anything that still has contents
# Essentially builds a depth-first list of dirs in $(libdir)/pd, then tries to
# remove them if they are empty. The "|| true" ensures that if the dir is *not*
# empty, then rmdir does not throw an error and stop make.
uninstall-hook:
	if test -d $(DESTDIR)/$(libdir)/pd ; then \
	    find $(DESTDIR)/$(libdir)/pd -type d -depth | xargs rmdir 2>/dev/null || true ; \
	fi
