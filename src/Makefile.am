AUTOMAKE_OPTIONS = foreign

pd_CFLAGS = -DPD -DINSTALL_PREFIX=\"$(prefix)\"
pd_LDFLAGS =

pdsend_CFLAGS = 
pdreceive_CFLAGS = 
pd_watchdog_CFLAGS = 
LIBS = @LIBS@
INCLUDES = @INCLUDES@

SUFFIXES = .@EXTENSION@ .@SHARED_LIB@

bin_PROGRAMS = pd pdsend pdreceive
pdsend_SOURCES = u_pdsend.c
pdreceive_SOURCES = u_pdreceive.c
pd_watchdog_SOURCES = s_watchdog.c
pd_LDADD = 
pd_SOURCES = g_canvas.c g_graph.c g_text.c g_rtext.c g_array.c g_template.c \
    g_io.c g_scalar.c g_traversal.c g_guiconnect.c g_readwrite.c g_editor.c \
    g_clone.c \
    g_all_guis.c g_bang.c g_hdial.c g_hslider.c g_mycanvas.c g_numbox.c \
    g_toggle.c g_vdial.c g_vslider.c g_vumeter.c \
    m_pd.c m_class.c m_obj.c m_atom.c m_memory.c m_binbuf.c \
    m_conf.c m_glob.c m_sched.c \
    s_main.c s_inter.c s_file.c s_print.c \
    s_loader.c s_path.c s_entry.c s_audio.c s_midi.c \
    s_utf8.c \
    d_ugen.c d_ctl.c d_arithmetic.c d_osc.c d_filter.c d_dac.c d_misc.c \
    d_math.c d_fft.c d_array.c d_global.c \
    d_delay.c d_resample.c  d_soundfile.c \
    x_arithmetic.c x_connective.c x_interface.c x_midi.c x_misc.c \
    x_time.c x_acoustics.c x_net.c x_text.c x_gui.c x_list.c x_array.c \
    x_scalar.c  x_vexp.c x_vexp_if.c x_vexp_fun.c
nodist_EXTRA_pd_SOURCES =

pd_includedir = ${includedir}/pd
pd_include_HEADERS = m_pd.h m_imp.h g_canvas.h s_stuff.h g_all_guis.h
# compatibility: m_pd.h also goes into ${includedir}/
include_HEADERS = m_pd.h
noinst_HEADERS = g_all_guis.h s_audio_alsa.h s_audio_paring.h s_utf8.h

# we want these in the dist tarball
EXTRA_DIST = CHANGELOG.txt notes.txt \
	pd.ico pd.rc \
	makefile.gnu  makefile.mac  makefile.mingw  makefile.msvc \
	s_audio_dummy.c s_audio_audiounit.c s_audio_esd.c


# configurations per library
if ALSA
pd_CFLAGS += -DUSEAPI_ALSA
pd_LDADD  += @ALSA_LIBS@
pd_SOURCES += s_audio_alsa.c s_audio_alsamm.c s_midi_alsa.c
endif

if COREAUDIO
LIBS += -framework CoreAudio -framework CoreMIDI \
	-framework AudioUnit -framework AudioToolbox
endif

if FFTW
pd_SOURCES +=  d_fft_fftw.c
else
pd_SOURCES +=  d_fft_fftsg.c
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

# Cygwin has a function OSS /dev/dsp, but not MIDI, and Pd is only set up to
# handle a single MIDI API
if OSS
if !WINDOWS
pd_CFLAGS += -DUSEAPI_OSS
pd_SOURCES += s_audio_oss.c s_midi_oss.c
endif
endif

if PORTAUDIO
pd_CFLAGS += -DUSEAPI_PORTAUDIO  -I$(top_srcdir)/portaudio/include
pd_LDADD += $(top_builddir)/portaudio/lib/libportaudio.la
pd_SOURCES += s_audio_pa.c s_audio_paring.c
endif

# ASIO needs to go after PORTAUDIO in order for it to link properly
if ASIO
pd_LDADD += $(top_builddir)/asio/libasio.la
nodist_EXTRA_pd_SOURCES += dummy.cxx
endif

if PORTMIDI
INCLUDES += -I$(top_srcdir)/portmidi/pm_common -I$(top_srcdir)/portmidi/porttime
pd_LDADD += $(top_builddir)/portmidi/libportmidi.la
pd_SOURCES += s_midi_pm.c
endif


# FIXXXME
# GNU/HURD, IPHONEOS, ... have no MIDI (not even OSS)
#  i think it would be better to add s_midi_dummy.c only if no other midi API can be found
#  (without OS-specific checks)
# even better would be, to allow Pd to have simply have no MIDI (nor AUDIO)
if IPHONEOS
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
LIBS += -framework Carbon
pd_CFLAGS += -DMACOSX #kludge, should use auto macro __APPLE__
bin_SCRIPTS = 
bin_PROGRAMS += pd-watchdog
endif

if WINDOWS
LIBS += -lwsock32 -lwinmm -lole32
pd_CFLAGS +=  -DUSEAPI_MMIO -DPD_INTERNAL
pd_SOURCES += s_audio_mmio.c s_midi_mmio.c
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
