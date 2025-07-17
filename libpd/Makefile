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
  PLATFORM_CFLAGS = -DHAVE_ALLOCA_H -DHAVE_LIBDL -DHAVE_MACHINE_ENDIAN_H
  PLATFORM_LDFLAGS = -dynamiclib -ldl -Wl,-no_compact_unwind
  # helps for machine/endian.h to be found
  PLATFORM_CFLAGS += -D_DARWIN_C_SOURCE
  # increase max allowed file descriptors
  PLATFORM_CFLAGS += -D_DARWIN_UNLIMITED_SELECT -DFD_SETSIZE=10240
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
    PLATFORM_CFLAGS = -DWINVER=0x502 -DWIN32 -D_WIN32
    MINGW_LDFLAGS = -shared -Wl,--export-all-symbols -lws2_32 -lkernel32 \
                    -static-libgcc
    PLATFORM_LDFLAGS = $(MINGW_LDFLAGS) -Wl,--output-def=$(LIBPD_DEF) \
                       -Wl,--out-implib=$(LIBPD_IMPLIB)
  else  # Linux or *BSD
    SOLIB_EXT = so
    PLATFORM_CFLAGS = -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -fPIC \
                      -DHAVE_ENDIAN_H
    PLATFORM_LDFLAGS = -shared -Wl,-Bsymbolic
    ifeq ($(UNAME), Linux)
      PLATFORM_CFLAGS += -DHAVE_ALLOCA_H -DHAVE_LIBDL
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
CPPFLAGS = -DPD -DUSEAPI_DUMMY -DPD_INTERNAL -DHAVE_UNISTD_H \
           -I../src -DLIBPD_EXTRA -DPDINSTANCE

# code generation flags (e.g., optimization).
CODECFLAGS = -fPIC -ffast-math -funroll-loops -fomit-frame-pointer -O3

# anything else you want to specify.
MORECFLAGS =

# additional linker flags to override to add more (like "-g").
MORELDFLAGS =

# additional source files you might want
MORESRC =

CFLAGS = $(CPPFLAGS) $(PLATFORM_CFLAGS) $(CODECFLAGS) $(MORECFLAGS)
LDFLAGS = $(PLATFORM_LDFLAGS) $(MORELDFLAGS)

PDSRC = d_arithmetic.c d_array.c d_ctl.c d_dac.c d_delay.c d_fft.c \
        d_fft_fftsg.c d_filter.c d_global.c d_math.c d_misc.c d_osc.c \
        d_resample.c d_soundfile.c d_soundfile_aiff.c d_soundfile_caf.c \
        d_soundfile_next.c d_soundfile_wave.c d_ugen.c \
        g_all_guis.c g_array.c g_bang.c g_canvas.c g_clone.c g_editor.c \
        g_editor_extras.c g_graph.c g_guiconnect.c g_io.c g_mycanvas.c \
        g_numbox.c g_radio.c g_readwrite.c g_rtext.c g_scalar.c g_slider.c \
        g_template.c g_text.c g_toggle.c g_traversal.c g_undo.c g_vumeter.c \
        m_atom.c m_binbuf.c m_class.c m_conf.c m_glob.c m_memory.c m_obj.c \
        m_pd.c m_sched.c \
        s_audio.c s_audio_dummy.c s_inter.c s_inter_gui.c s_loader.c s_main.c \
        s_net.c s_path.c  s_print.c s_utf8.c \
        x_acoustics.c x_arithmetic.c x_array.c x_connective.c x_file.c x_gui.c \
        x_interface.c x_list.c x_midi.c x_misc.c x_net.c x_scalar.c x_text.c \
        x_time.c x_vexp.c x_vexp_if.c x_vexp_fun.c

EXTRASRC = bob~.c bonk~.c choice.c fiddle~.c loop~.c lrshift~.c pique.c \
           pdsched.c pd~.c sigmund~.c stdout.c

LIBPDSRC = s_libpdmidi.c x_libpdreceive.c z_hooks.c z_libpd.c \
           z_print_util.c z_queued.c z_ringbuffer.c

SRC = $(PDSRC) $(EXTRASRC) $(LIBPDSRC) $(MORESRC)

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
