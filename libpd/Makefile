# This is a makefile to build libpd from vanilla Pd sources.  It assumes that
# the Pd source directory "../src" is parallel to this directory in order to
# find the Pd sources.

UNAME = $(shell uname)
SOLIB_PREFIX = lib
STATICLIB_EXT = a
LIBPD_IMPLIB =
LIBPD_DEF =
PLATFORM_ARCH ?= $(shell $(CC) -dumpmachine | sed -e 's,-.*,,')

ifeq ($(UNAME), Darwin)  # Mac
  SOLIB_EXT = dylib
  PLATFORM_CFLAGS = -DHAVE_LIBDL
  PLATFORM_LDFLAGS = -dynamiclib -ldl -Wl,-no_compact_unwind
  ifeq ($(FAT_LIB), true)
    # macOS universal "fat" lib compilation
    MAC_VER = $(shell sw_vers -productVersion | cut -f1 -f2 -d.)
    ifeq ($(shell expr $(MAC_VER) \<= 10.13), 1)
      # universal1: macOS 10.6 - 10.13
      FAT_ARCHS ?= -arch i386 -arch x86_64
    endif
    ifeq ($(shell expr $(MAC_VER) \>= 11.0), 1)
      # universal2: macOS 11.0+
      FAT_ARCHS ?= -arch arm64 -arch x86_64
    endif
    FAT_ARCHS ?= -arch $(PLATFORM_ARCH)
    PLATFORM_CFLAGS += $(FAT_ARCHS)
    PLATFORM_LDFLAGS += $(FAT_ARCHS)
  endif
else
  ifeq ($(OS), Windows_NT)  # Windows, use Mingw
    CC ?= gcc
    SOLIB_EXT = dll
    SOLIB_PREFIX =
    LIBPD_IMPLIB = libpd.lib
    LIBPD_DEF = libpd.def
    PLATFORM_CFLAGS = -DWINVER=0x502 -DWIN32 -D_WIN32 -DPD_INTERNAL
    MINGW_LDFLAGS = -shared -Wl,--export-all-symbols -lws2_32 -lkernel32 \
                    -static-libgcc
    PLATFORM_LDFLAGS = $(MINGW_LDFLAGS) -Wl,--output-def=$(LIBPD_DEF) \
                       -Wl,--out-implib=$(LIBPD_IMPLIB)
  else  # Linux or *BSD
    SOLIB_EXT = so
    PLATFORM_CFLAGS = -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -fPIC
    PLATFORM_LDFLAGS = -shared -Wl,-Bsymbolic
    ifeq ($(UNAME), Linux)
      PLATFORM_CFLAGS += -DHAVE_LIBDL
      PLATFORM_LDFLAGS += -ldl
    endif
  endif
endif

VPATH = ../src:\
../extra/bob~:../extra/bonk~:../extra/choice:../extra/fiddle~:../extra/loop~:\
../extra/lrshift~:../extra/pd~:../extra/pique:../extra/sigmund~:../extra/stdout

# The C flags are separated into CPPFLAGS, CODECFLAGS, and MORECFLAGS
# to allow easy overriding of CODECFLAGS and to allow adding MORECFLAGS:

# C preprocessor flags, and flags controlling errors and warnings
CPPFLAGS = -I../src -DPD -DHAVE_UNISTD_H -DUSEAPI_DUMMY -DLIBPD_EXTRA \
    -DPDINSTANCE

# code generation flags (e.g., optimization).
CODECFLAGS = -fPIC -ffast-math -funroll-loops -fomit-frame-pointer -O3

# anything else you want to specify.
MORECFLAGS =

# additional linker flags to override to add more (like "-g").
MORELDFLAGS =

CFLAGS = $(CPPFLAGS) $(PLATFORM_CFLAGS) $(CODECFLAGS) $(MORECFLAGS)
LDFLAGS = $(PLATFORM_LDFLAGS) $(MORELDFLAGS)

PDSRC = g_canvas.c g_graph.c g_text.c g_rtext.c g_array.c g_template.c g_io.c \
    g_scalar.c g_traversal.c g_guiconnect.c g_readwrite.c g_editor.c g_clone.c \
    g_all_guis.c g_bang.c g_hdial.c g_hslider.c g_mycanvas.c g_numbox.c \
    g_toggle.c g_undo.c g_vdial.c g_vslider.c g_vumeter.c \
    g_editor_extras.c \
    m_pd.c m_class.c m_obj.c m_atom.c m_memory.c m_binbuf.c \
    m_conf.c m_glob.c m_sched.c \
    s_main.c s_inter.c s_print.c \
    s_loader.c s_path.c s_entry.c s_audio.c s_midi.c s_net.c s_utf8.c \
    s_audio_paring.c \
    d_ugen.c d_ctl.c d_arithmetic.c d_osc.c d_filter.c d_dac.c d_misc.c \
    d_math.c d_fft.c d_fft_fftsg.c d_array.c d_global.c \
    d_delay.c d_resample.c d_soundfile.c d_soundfile_aiff.c d_soundfile_caf.c \
    d_soundfile_next.c d_soundfile_wave.c \
    x_arithmetic.c x_connective.c x_interface.c x_midi.c x_misc.c \
    x_time.c x_acoustics.c x_net.c x_text.c x_gui.c x_list.c x_array.c \
    x_file.c x_scalar.c  x_vexp.c x_vexp_if.c x_vexp_fun.c \

SRC = $(PDSRC) \
    x_libpdreceive.o s_audio_dummy.o s_midi_dummy.o \
    z_hooks.o z_libpd.o z_print_util.o z_queued.o z_ringbuffer.o \
    bob~.o bonk~.o choice.o fiddle~.o loop~.o lrshift~.o pique.o sigmund~.o \
    pd~.o stdout.o

OBJ = $(SRC:.c=.o)

LIBPD = $(SOLIB_PREFIX)pd.$(SOLIB_EXT)
LIBPD_STATIC = libpd.$(STATICLIB_EXT)

.PHONY: libpd clean

libpd: $(LIBPD) $(LIBPD_STATIC)

$(LIBPD): $(OBJ)
	$(CC) -o $@ $^ -shared -lm -lpthread $(LDFLAGS)

$(LIBPD_STATIC): $(OBJ)
	ar rcs $@ $^

clean:
	-rm -f *.o $(LIBPD) $(LIBPD_STATIC) $(LIBPD_IMPLIB) $(LIBPD_DEF)
