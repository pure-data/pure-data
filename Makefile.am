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

# subdirs that are built,
# mac before src due to uninstall target order kludge (see mac/Makfile.am)
SUBDIRS = $(EXTRA_SUBDIRS) mac src man doc tcl extra font

# subdirs that are always included in the dist, etc.
DIST_SUBDIRS = asio doc extra font mac man po portmidi portaudio src tcl

#########################################
##### Targets #####

.PHONY: app

# optionaly build localizations
if MSGFMT
locales:
	make -C po all
else
locales:
endif

# forward target to mac/Makefile
app:
	make -C mac app

# startup location for GUI plugins and/or patches,
# leftover from Pd-extended & not currently used
install-data-local:
	$(INSTALL) -d $(DESTDIR)$(libdir)/pd/startup
	$(INSTALL) -d $(DESTDIR)$(libdir)/pd/startup/disabled

# remove the leftover $(libdir)/pd dir, fail silently on any non-empty dirs
# The "|| true" ensures that if the dir is *not* empty, then rmdir does not throw
# an error and stop make.
uninstall-hook:
	if test -d $(DESTDIR)$(pkglibdir) ; then \
	    find $(DESTDIR)$(pkglibdir) -type d -depth | xargs rmdir 2>/dev/null || true ; \
	fi
