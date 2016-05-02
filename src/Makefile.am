AUTOMAKE_OPTIONS = foreign
AM_CPPFLAGS =

pd_CFLAGS = -DPD -DINSTALL_PREFIX=\"$(prefix)\" -DPD_INTERNAL

pd_LDFLAGS =

pdsend_CFLAGS =
pdreceive_CFLAGS =
pd_watchdog_CFLAGS =
LIBS = @LIBS@

SUFFIXES = .@EXTENSION@ .@SHARED_LIB@

bin_PROGRAMS = pd pdsend pdreceive
pdsend_SOURCES = u_pdsend.c
pdreceive_SOURCES = u_pdreceive.c
pd_watchdog_SOURCES = s_watchdog.c
pd_LDADD =
pd_SOURCES = \
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
    s_entry.c \
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
nodist_EXTRA_pd_SOURCES =

pd_includedir = ${includedir}/pd
pd_include_HEADERS = m_pd.h m_imp.h g_canvas.h s_stuff.h g_all_guis.h x_vexp.h
# compatibility: m_pd.h also goes into ${includedir}/
include_HEADERS = m_pd.h
noinst_HEADERS = g_all_guis.h s_audio_alsa.h s_audio_paring.h s_utf8.h x_vexp.h

# we want these in the dist tarball
EXTRA_DIST = CHANGELOG.txt notes.txt pd.ico pd.rc \
    makefile.gnu  makefile.mac  makefile.mingw  makefile.msvc \
    s_audio_audiounit.c s_audio_esd.c

# configurations per library
if ALSA
pd_CFLAGS += -DUSEAPI_ALSA
pd_LDADD += @ALSA_LIBS@
pd_SOURCES += s_audio_alsa.c s_audio_alsamm.c s_midi_alsa.c
endif

# needed by PortAudio on OSX
if COREAUDIO
LIBS += -framework CoreAudio -framework CoreMIDI \
        -framework AudioUnit -framework AudioToolbox
endif

if FFTW
pd_SOURCES += d_fft_fftw.c
else
pd_SOURCES += d_fft_fftsg.c
endif

# TODO support Jack xrun
if JACK
pd_CFLAGS += -DUSEAPI_JACK -DJACK_XRUN
pd_SOURCES += s_audio_jack.c
if MACOSX
pd_LDFLAGS += -weak_framework Jackmp
else
pd_LDADD += @JACK_LIBS@
endif
endif

if OSS
pd_CFLAGS += -DUSEAPI_OSS
pd_SOURCES += s_audio_oss.c s_midi_oss.c
endif

if PORTAUDIO
AM_CPPFLAGS += -I$(top_srcdir)/portaudio/include
pd_CFLAGS += -DUSEAPI_PORTAUDIO
pd_LDADD += $(top_builddir)/portaudio/lib/libportaudio.la
pd_SOURCES += s_audio_pa.c s_audio_paring.c
if MACOSX
LIBS += -framework Carbon
endif
endif

# ASIO needs to go after PORTAUDIO in order for it to link properly
if ASIO
pd_LDADD += $(top_builddir)/asio/libasio.la
nodist_EXTRA_pd_SOURCES += dummy.cxx
endif

if PORTMIDI
AM_CPPFLAGS += -I$(top_srcdir)/portmidi/pm_common -I$(top_srcdir)/portmidi/porttime
pd_LDADD += $(top_builddir)/portmidi/libportmidi.la
pd_SOURCES += s_midi_pm.c
endif

if MMIO
pd_CFLAGS += -DUSEAPI_MMIO
pd_SOURCES += s_audio_mmio.c
endif

# if no audio or midi api was detected/specified, fall back to dummy apis
# ie. GNU/HURD, IPHONEOS, ... have no MIDI (not even OSS)
if AUDIO_DUMMY
pd_CFLAGS += -DUSEAPI_DUMMY
pd_SOURCES += s_audio_dummy.c
endif
if MIDI_DUMMY
pd_SOURCES += s_midi_dummy.c
endif

if HURD
libpdbindir = $(pkglibdir)/bin
libpdbin_DATA =
libpdbin_PROGRAMS = pd-watchdog pd

# this flag has to have a single leading "-" for libtool, even though ld uses
# --export-dynamic, and libtool sends -Wl,--export-dynamic to ld...
pd_LDFLAGS += -export-dynamic

# on Ubuntu/Karmic 9.10, it doesn't seem to find libm, so force it
pd_LDFLAGS += $(LIBM)

# force linking to pthread, which should really be done with some autotools way
pd_LDFLAGS += -lpthread

# force linking to dl, which should really be done with some autotools way
pd_LDFLAGS += -ldl
endif

if LINUX
libpdbindir = $(pkglibdir)/bin
libpdbin_DATA =
libpdbin_PROGRAMS = pd-watchdog pd

# this flag has to have a single leading "-" for libtool, even though ld uses
# --export-dynamic, and libtool sends -Wl,--export-dynamic to ld...
pd_LDFLAGS += -export-dynamic

# on Ubuntu/Karmic 9.10, it doesn't seem to find libm, so force it
pd_LDFLAGS += $(LIBM)
endif

if MACOSX

# required for dlopen & weak linking
pd_CFLAGS += -mmacosx-version-min=10.4

# kludge, should use auto macro __APPLE__
# but who knows what externals rely on this
pd_CFLAGS += -DMACOSX

LIBS += -ldl -lm -lpthread
bin_SCRIPTS =
bin_PROGRAMS += pd-watchdog
endif

if WINDOWS
LIBS += -lwsock32 -lwinmm -lole32
bin_SCRIPTS =
endif

if MINGW
# To use SetDllDirectory() in s_loader.c, we need a minium of Windows
# XP SP1.  WINVER isnt' fine-grained enough for that, so we use the
# next minor version of Windows, 5.2.  That gives us -DWINVER=0x0502
pd_CFLAGS += -DWISHAPP='"wish85.exe"' -DWINVER=0x0502
endif

etags: TAGS
	etags --append --language=none --regex="/proc[ \t]+\([^ \t]+\)/\1/" *.tcl
