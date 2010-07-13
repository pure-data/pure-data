AUTOMAKE_OPTIONS = foreign

pd_CFLAGS = -DPD -DINSTALL_PREFIX=\"$(prefix)\"
pd_LDFLAGS =

pdsend_CFLAGS = 
pdreceive_CFLAGS = 
pd_watchdog_CFLAGS = 
LIBS = 
INCLUDES = @INCLUDES@

SUFFIXES = .@EXTENSION@ .@SHARED_LIB@

bin_PROGRAMS = pd pdsend pdreceive pd-watchdog
pdsend_SOURCES = u_pdsend.c
pdreceive_SOURCES = u_pdreceive.c
pd_watchdog_SOURCES = s_watchdog.c
pd_LDADD = 
pd_SOURCES = g_canvas.c g_graph.c g_text.c g_rtext.c g_array.c g_template.c \
	g_io.c g_scalar.c g_traversal.c g_guiconnect.c g_readwrite.c g_editor.c \
    g_all_guis.c g_bang.c g_hdial.c g_hslider.c g_mycanvas.c g_numbox.c \
    g_toggle.c g_vdial.c g_vslider.c g_vumeter.c \
    m_pd.c m_class.c m_obj.c m_atom.c m_memory.c m_binbuf.c \
    m_conf.c m_glob.c m_sched.c \
    s_main.c s_inter.c s_file.c s_print.c \
    s_loader.c s_path.c s_entry.c s_audio.c s_midi.c \
    d_ugen.c d_ctl.c d_arithmetic.c d_osc.c d_filter.c d_dac.c d_misc.c \
    d_math.c d_fft.c d_array.c d_global.c \
    d_delay.c d_resample.c \
    x_arithmetic.c x_connective.c x_interface.c x_midi.c x_misc.c \
    x_time.c x_acoustics.c x_net.c x_qlist.c x_gui.c x_list.c d_soundfile.c

TCL_FILES = AppMain.tcl apple_events.tcl dialog_array.tcl dialog_audio.tcl \
	dialog_canvas.tcl dialog_data.tcl dialog_find.tcl dialog_font.tcl \
	dialog_gatom.tcl dialog_iemgui.tcl dialog_message.tcl dialog_midi.tcl \
	dialog_path.tcl dialog_startup.tcl helpbrowser.tcl opt_parser.tcl \
	pd-gui.tcl pd_bindings.tcl pd_connect.tcl pd_menucommands.tcl pd_menus.tcl \
	pdtk_canvas.tcl pdtk_text.tcl pdwindow.tcl pkgIndex.tcl scrollbox.tcl \
	scrollboxwindow.tcl wheredoesthisgo.tcl

include_HEADERS = m_pd.h m_imp.h g_canvas.h s_stuff.h
noinst_HEADERS = g_all_guis.h s_audio_alsa.h s_audio_pablio.h s_audio_paring.h


# we want these in the dist tarball
EXTRA_DIST = CHANGELOG.txt notes.txt makefile.mingw $(TCL_FILES)

# configurations per library
if ALSA
pd_CFLAGS += -DUSEAPI_ALSA
pd_SOURCES += s_audio_alsa.c s_audio_alsamm.c s_midi_alsa.c
endif

if COREAUDIO
LIBS += -framework CoreAudio -framework CoreMIDI \
	-framework AudioUnit -framework AudioToolbox
endif

if FFTW
pd_SOURCES +=  d_fft_fftw.c d_fftroutine.c
else
pd_SOURCES +=  d_fft_mayer.c d_fftroutine.c
endif

# TODO support Jack xrun
if JACK
pd_CFLAGS += -DUSEAPI_JACK -DJACK_XRUN
pd_SOURCES += s_audio_jack.c
if MACOSX
LIBS += -weak_framework Jackmp
else
LIBS += -ljack
endif
endif

# Cygwin has a function OSS /dev/dsp, but not MIDI, and Pd is only set up to
# handle a single MIDI API
if OSS
if !WINDOWS
pd_CFLAGS += -DUSEAPI_OSS -DUNIX
pd_SOURCES += s_audio_oss.c s_midi_oss.c
endif
endif

if PORTAUDIO
pd_CFLAGS += -DUSEAPI_PORTAUDIO  -I../portaudio/include
pd_LDADD += ../portaudio/libportaudio.la
pd_SOURCES += s_audio_pa.c s_audio_pablio.c s_audio_paring.c
endif

# ASIO needs to go after PORTAUDIO in order for it to link properly
if ASIO
pd_LDADD += ../asio/libasio.la
endif

if PORTMIDI
INCLUDES += -I../portmidi/pm_common -I../portmidi/porttime
pd_LDADD += ../portmidi/libportmidi.la
pd_SOURCES += s_midi_pm.c
endif

if IPHONEOS
pd_SOURCES += s_midi_dummy.c
endif

if LINUX 
# kludge, should use auto macros like __linux__. The UNIX macro seems
# to mean "things that have been implemented with GNU/Linux in mind, but the
# watchdog should work on Mac OS X and Cygwin, and OSS can work on FreeBSD,
# Cygwin, etc.
pd_CFLAGS += -DUNIX 

libpdbindir = $(pkglibdir)/bin
libpdbin_DATA = $(TCL_FILES)
# this flag has to have a single leading "-" for libtool, even though ld uses
# --export-dynamic, and libtool sends -Wl,--export-dynamic to ld...
pd_LDFLAGS += -export-dynamic
# on Ubuntu/Karmic 9.10, it doesn't seem to find libm, so force it
pd_LDFLAGS += $(LIBM)
endif

if MACOSX
LIBS += -framework Carbon
pd_CFLAGS += -DMACOSX #kludge, should use auto macro __APPLE__
bin_SCRIPTS = $(TCL_FILES)
endif

if WINDOWS
LIBS += -lwsock32 -lwinmm -lole32
pd_CFLAGS +=  -DUSEAPI_MMIO -DPD_INTERNAL
pd_SOURCES += s_audio_mmio.c s_midi_mmio.c
bin_SCRIPTS = $(TCL_FILES)
endif

# Cygwin is not _WIN32 and MSW vaguely means the same thing, so MINGW only
if MINGW
pd_CFLAGS += -DWISHAPP='"wish85.exe"' -DMSW #kludge, MSW should be _WIN32
pdsend_CFLAGS += -DMSW #kludge, should use _WIN32
pdreceive_CFLAGS += -DMSW #kludge, should use _WIN32
endif

etags: TAGS
	etags --append --language=none --regex="/proc[ \t]+\([^ \t]+\)/\1/" *.tcl
