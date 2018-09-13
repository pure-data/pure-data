#########################################
##### Defaults & Paths #####

AUTOMAKE_OPTIONS = foreign
CLEANFILES=

bin_SCRIPTS =
noinst_SCRIPTS =

pd_CFLAGS = -DPD -DINSTALL_PREFIX=\"$(prefix)\" -DPD_INTERNAL
pd_LDFLAGS =
pd_LDADD =

# there are pd_* and pd_*_core variables as we need different flags on Windows
# for the DLL and the EXE, other OSes simply set pd_* = $(pd_*_core) later
# also, the "_core" suffix is used as this keeps automake from thinking these
# are library or binary variables since we only need them as placeholders
pd_LDFLAGS_core =
pd_LDADD_core =

pdsend_CFLAGS =
pdreceive_CFLAGS =
pd_watchdog_CFLAGS =
LIBS = @LIBS@

SUFFIXES = .@EXTENSION@ .@SHARED_LIB@

#########################################
##### Files, Binaries, & Libs #####

# pd core & utils
bin_PROGRAMS = pd pdsend pdreceive

pdsend_SOURCES = u_pdsend.c
pdreceive_SOURCES = u_pdreceive.c
pd_watchdog_SOURCES = s_watchdog.c

# on Windows, pd.exe contains only s_entry.c and links against pd.dll
# (where all the logic resides), that's why we have to split the sources
# (only to later merge them again for non-Windows systems)
pd_SOURCES = s_entry.c
pd_SOURCES_core = \
    d_arithmetic.c \
    d_array.c \
    d_ctl.c \
    d_dac.c \
    d_delay.c \
    d_fft.c \
    d_filter.c \
    d_global.c \
    d_math.c \
    d_misc.c \
    d_osc.c \
    d_resample.c \
    d_soundfile.c \
    d_ugen.c \
    g_all_guis.c \
    g_array.c \
    g_bang.c \
    g_canvas.c \
    g_clone.c \
    g_editor.c \
    g_editor_extras.c \
    g_graph.c \
    g_guiconnect.c \
    g_hdial.c \
    g_hslider.c \
    g_io.c \
    g_mycanvas.c \
    g_numbox.c \
    g_readwrite.c \
    g_rtext.c \
    g_scalar.c \
    g_template.c \
    g_text.c \
    g_toggle.c \
    g_traversal.c \
    g_undo.c \
    g_vdial.c \
    g_vslider.c \
    g_vumeter.c \
    m_atom.c \
    m_binbuf.c \
    m_class.c \
    m_conf.c \
    m_glob.c \
    m_memory.c \
    m_obj.c \
    m_pd.c \
    m_sched.c \
    s_audio.c \
    s_file.c \
    s_inter.c \
    s_loader.c \
    s_main.c \
    s_midi.c \
    s_path.c \
    s_print.c \
    s_utf8.c \
    x_acoustics.c \
    x_arithmetic.c \
    x_array.c \
    x_connective.c \
    x_gui.c \
    x_interface.c \
    x_list.c \
    x_midi.c \
    x_misc.c \
    x_net.c \
    x_scalar.c \
    x_text.c \
    x_time.c \
    x_vexp.c \
    x_vexp_fun.c \
    x_vexp_if.c

# pd-watchdog and "local" pd symlink location for tcl scripts
libpdbindir = $(pkglibdir)/bin

# these install to ${includedir}/pd
pkginclude_HEADERS = m_pd.h m_imp.h g_canvas.h g_undo.h g_all_guis.h s_stuff.h x_vexp.h
# compatibility: m_pd.h also goes into ${includedir}/
include_HEADERS = m_pd.h
noinst_HEADERS = s_audio_alsa.h s_audio_paring.h s_utf8.h

# we want these in the dist tarball
EXTRA_DIST = CHANGELOG.txt notes.txt pd.rc \
    makefile.gnu  makefile.mac  makefile.mingw  makefile.msvc \
    s_audio_audiounit.c s_audio_esd.c

# add WISH define if it's set
WISH=@WISH@
WISHDEFINE=$(if $(WISH),-DWISH='"$(WISH)"')
pd_CFLAGS += $(WISHDEFINE)

#########################################
##### Configurations Per Library #####

##### Advanced Linux Sound Architecture #####
if ALSA
pd_CFLAGS += -DUSEAPI_ALSA
pd_LDADD_core += @ALSA_LIBS@
pd_SOURCES_core += s_audio_alsa.c s_audio_alsamm.c s_midi_alsa.c
endif

##### OSX CoreAudio #####
# needed by PortAudio on OSX
if COREAUDIO
LIBS += -framework CoreAudio -framework CoreMIDI \
        -framework AudioUnit -framework AudioToolbox
endif

##### Jack Audio Connection Kit #####
# TODO support Jack xrun
if JACK
pd_CFLAGS += -DUSEAPI_JACK -DJACK_XRUN
pd_SOURCES_core += s_audio_jack.c

if JACK_FRAMEWORK
# link to Jackmp.framework on macOS
pd_LDFLAGS_core += -weak_framework Jackmp
else
# link to Jack discovered by configure
pd_LDADD_core += @JACK_LIBS@
endif

endif

##### GNU/Linux Open Sound System #####
if OSS
pd_CFLAGS += -DUSEAPI_OSS
pd_SOURCES_core += s_audio_oss.c s_midi_oss.c
endif

##### Windows MultiMedia (File) I/O #####
if MMIO
pd_CFLAGS += -DUSEAPI_MMIO
pd_SOURCES_core += s_audio_mmio.c
endif

##### PortAudio #####
if PORTAUDIO
pd_CFLAGS += -DUSEAPI_PORTAUDIO
pd_SOURCES_core += s_audio_pa.c s_audio_paring.c

if LOCAL_PORTAUDIO
# link the included portaudio which is built as a static lib
AM_CPPFLAGS += -I$(top_srcdir)/portaudio/portaudio/include
pd_LDADD_core += $(top_builddir)/portaudio/libportaudio.a
else
# link the system's portaudio
pd_LDADD_core += -lportaudio
endif

# need Carbon framework for PA on Mac
if MACOSX
LIBS += -framework Carbon
endif

endif

##### ASIO #####
# ASIO needs to go after PORTAUDIO in order for it to link properly
if ASIO
# always Windows
pd_LDADD_core += $(top_builddir)/asio/libasio.a
endif

##### PortMidi #####
if PORTMIDI
pd_SOURCES_core += s_midi_pm.c

if LOCAL_PORTMIDI
# link the included portmidi which is built as a static lib
AM_CPPFLAGS += \
    -I$(top_srcdir)/portmidi/portmidi/pm_common \
    -I$(top_srcdir)/portmidi/portmidi/porttime
pd_LDADD_core += $(top_builddir)/portmidi/libportmidi.a
else
# link the system's portmidi
pd_LDADD_core += -lportmidi
endif

# need Carbon framework for PM on Mac
if MACOSX
LIBS += -framework CoreFoundation -framework Carbon
endif

endif

##### NO API? #####
# if no audio or midi api was detected/specified, fall back to dummy apis
# ie. GNU/HURD, IPHONEOS, ... have no MIDI (not even OSS)
if AUDIO_DUMMY
pd_CFLAGS += -DUSEAPI_DUMMY
pd_SOURCES_core += s_audio_dummy.c
endif
if MIDI_DUMMY
pd_CFLAGS += -DUSEAPI_MIDIDUMMY
pd_SOURCES_core += s_midi_dummy.c
endif

##### FFTW fft library #####
if FFTW
pd_SOURCES_core += d_fft_fftw.c
else
pd_SOURCES_core += d_fft_fftsg.c
endif

#########################################
##### Configurations Per Platform #####

##### GNU/Hurd #####
if HURD

# install watchdog to $(libdir)/pd/bin as it's not a user facing program
libpdbin_PROGRAMS = pd-watchdog

# this flag has to have a single leading "-" for libtool, even though ld uses
# --export-dynamic, and libtool sends -Wl,--export-dynamic to ld...
pd_LDFLAGS_core += -export-dynamic

# on Ubuntu/Karmic 9.10, it doesn't seem to find libm, so force it
pd_LDFLAGS_core += $(LIBM)

# force linking to pthread, which should really be done with some autotools way
pd_LDFLAGS_core += -lpthread

# force linking to dl, which should really be done with some autotools way
pd_LDFLAGS_core += -ldl

endif

##### GNU/Linux #####
if LINUX

# install watchdog to $(libdir)/pd/bin as it's not a user facing program
libpdbin_PROGRAMS = pd-watchdog

# this flag has to have a single leading "-" for libtool, even though ld uses
# --export-dynamic, and libtool sends -Wl,--export-dynamic to ld...
pd_LDFLAGS_core += -export-dynamic

# on Ubuntu/Karmic 9.10, it doesn't seem to find libm, so force it
pd_LDFLAGS_core += $(LIBM)

endif

##### Apple Mac OSX #####
if MACOSX

# install watchdog to $(libdir)/pd/bin as it's not a user facing program
libpdbin_PROGRAMS = pd-watchdog

# kludge, should use auto macro __APPLE__
# but who knows what externals rely on this
pd_CFLAGS += -DMACOSX

# for dynamic loading & threading
LIBS += -ldl -lm -lpthread

endif

##### Windows #####
if WINDOWS

# win32 sockets, multimedia, and all that
LIBS += -lwsock32 -lwinmm -lole32 -static-libgcc -static-libstdc++

bin_SCRIPTS += pd.dll pd.lib pd.def pd.com
CLEANFILES += pd.dll pd.lib pd.def pd.com
noinst_SCRIPTS += libpd.a
CLEANFILES += libpd.a
CLEANFILES += pd.res

# hide the console
pd_LDFLAGS += -mwindows
# link with ressources and pd.dll import library
pd_LDADD += pd.res pd.lib

pd_OBJECTS_core = $(pd_SOURCES_core:.c=.o)

# the variables are taken from the automake Makefile
$(pd_OBJECTS_core) : %.o : %.c
	$(AM_V_CC)$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(pd_CFLAGS) $(CFLAGS) -c -o $@ $<

pd.res: pd.rc
	$(WINDRES) $< -O coff -o $@

# import library for pd.dll. also works with MSVC!
pd.lib: pd.dll

# another import library (when linking with -lpd), the same as pd.lib.
libpd.a: pd.lib
	cp $< $@

pd.def: pd.dll

pd.dll: $(pd_OBJECTS_core)
	$(CXX) -shared $(pd_LDFLAGS_core) -o pd.dll \
	$(pd_OBJECTS_core) $(pd_LDADD_core) $(LIBS) \
	-Wl,--export-all-symbols -Wl,--out-implib=pd.lib -Wl,--output-def=pd.def

# same as pd.exe but without -mwindows and resources
# NOTE: this is a bit ugly. I couldn't figure out how to get automake to build
# two programs with the same basename but different suffix.
pd.com: pd-s_entry.o pd.lib
	$(CXX) $(LDFLAGS) -o pd.com pd-s_entry.o $(LIBS) pd.lib

else
# for other OS, join pd_* with pd_*_core
pd_SOURCES += $(pd_SOURCES_core)
pd_LDADD += $(pd_LDADD_core)
pd_LDFLAGS += $(pd_LDFLAGS_core)
endif

##### Windows MinGW #####
if MINGW
# To use SetDllDirectory() in s_loader.c, we need a minimum of Windows
# XP SP1. WINVER isnt' fine-grained enough for that, so we use the
# next minor version of Windows, 5.2.  That gives us -DWINVER=0x0502
pd_CFLAGS += -DWINVER=0x0502 -D_WIN32_WINNT=0x0502
endif

#########################################
##### Targets #####

.PHONY: convenience-links

all-local: convenience-links

# create a bin folder & symlinks to the binaries in order to
# replicate the src/makefile.* build result
convenience-links: $(libpdbin_PROGRAMS) $(bin_PROGRAMS)
	$(MKDIR_P) $(top_builddir)/bin
	rm -rf $(top_builddir)/bin/pd*
	$(LN_S) $(top_builddir)/src/pd$(EXEEXT) $(top_builddir)/bin/pd$(EXEEXT)
	$(LN_S) $(top_builddir)/src/pdsend$(EXEEXT) $(top_builddir)/bin/pdsend$(EXEEXT)
	$(LN_S) $(top_builddir)/src/pdreceive$(EXEEXT) $(top_builddir)/bin/pdreceive$(EXEEXT)
	$(LN_S) $(top_srcdir)/tcl/pd-gui.in $(top_builddir)/bin/pd-gui
	test -e $(top_builddir)/src/pd-watchdog$(EXEEXT) && $(LN_S) $(top_builddir)/src/pd-watchdog$(EXEEXT) $(top_builddir)/bin/pd-watchdog$(EXEEXT) || true

clean-local:
	rm -rf $(top_builddir)/bin

# link to $(libdir)/pd/bin so the tcl scripts can
# launch the core if the gui is started first
#
# We have to make the dir just in case as it may not exist yet &
# we remove any existing symlink if doing a repeated install.
install-exec-hook:
	$(MKDIR_P) $(DESTDIR)$(libpdbindir)
	rm -f $(DESTDIR)$(libpdbindir)/pd
	$(LN_S) $(DESTDIR)$(bindir)/pd $(DESTDIR)$(libpdbindir)/pd

# remove the $(libdir)/pd/bin link
# &
# remove the leftover $(includedir)/pd dir, fail silently on any non-empty dirs
#
# The "|| true" ensures that if the dir is *not* empty, then rmdir does not throw
# an error and stop make.
uninstall-hook:
	rm -f $(DESTDIR)$(libpdbindir)/pd
	if test -d $(DESTDIR)$(pkgincludedir) ; then \
	    rmdir $(DESTDIR)$(pkgincludedir) 2>/dev/null || true ; \
	fi

# generate Emacs tags
etags: TAGS
	etags --append --language=none --regex="/proc[ \t]+\([^ \t]+\)/\1/" *.tcl
