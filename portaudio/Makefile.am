# this is built as a static convenience library

#########################################
##### Defaults & Paths #####

AUTOMAKE_OPTIONS = foreign
AM_CFLAGS = -DNEWBUFFER
AM_CPPFLAGS += \
    -I$(top_srcdir)/portaudio/portaudio/include \
    -I$(top_srcdir)/portaudio/portaudio/src/common

#########################################
##### Files, Binaries, & Libs #####

noinst_LIBRARIES = libportaudio.a

libportaudio_a_SOURCES = \
    portaudio/src/common/pa_allocation.c \
    portaudio/src/common/pa_converters.c \
    portaudio/src/common/pa_cpuload.c \
    portaudio/src/common/pa_debugprint.c \
    portaudio/src/common/pa_dither.c \
    portaudio/src/common/pa_front.c \
    portaudio/src/common/pa_process.c \
    portaudio/src/common/pa_ringbuffer.c \
    portaudio/src/common/pa_stream.c \
    portaudio/src/common/pa_trace.c

if LINUX
AM_CPPFLAGS += -I$(top_srcdir)/portaudio/portaudio/src/os/unix
libportaudio_a_SOURCES += \
    portaudio/src/os/unix/pa_unix_hostapis.c \
    portaudio/src/os/unix/pa_unix_util.c \
    portaudio/src/hostapi/alsa/pa_linux_alsa.c
endif

if MACOSX
AM_CFLAGS += -DPA_USE_COREAUDIO
AM_CPPFLAGS += -I$(top_srcdir)/portaudio/portaudio/src/os/unix
libportaudio_a_SOURCES += \
    portaudio/src/os/unix/pa_unix_hostapis.c \
    portaudio/src/os/unix/pa_unix_util.c \
    portaudio/src/hostapi/coreaudio/pa_mac_core.c \
    portaudio/src/hostapi/coreaudio/pa_mac_core_blocking.c \
    portaudio/src/hostapi/coreaudio/pa_mac_core_utilities.c
# required for PortAudio to build on newer versions of macOS as it
# disables deprecation warnings stopping the build
AM_CFLAGS += -Wno-error -Wno-deprecated
endif

if WINDOWS
AM_CFLAGS += -DPA_USE_WMME
AM_CPPFLAGS += -I$(top_srcdir)/portaudio/portaudio/src/os/win
libportaudio_a_SOURCES += \
    portaudio/src/os/win/pa_win_coinitialize.c \
    portaudio/src/os/win/pa_win_hostapis.c \
    portaudio/src/os/win/pa_win_util.c \
    portaudio/src/os/win/pa_win_waveformat.c \
    portaudio/src/hostapi/wmme/pa_win_wmme.c
if ASIO
AM_CFLAGS += -DPA_USE_ASIO
if MINGW
# hack for /asio/ASIOSDK/common/combase.h
AM_CPPFLAGS += -DWINVER=0x0502 -D_WIN32_WINNT=0x0502
endif
AM_CPPFLAGS += \
    -I$(top_srcdir)/asio/ASIOSDK/common -I$(top_srcdir)/asio/ASIOSDK/host \
	-I$(top_srcdir)/asio/ASIOSDK/host/pc
libportaudio_a_SOURCES += \
    portaudio/src/hostapi/asio/iasiothiscallresolver.cpp \
    portaudio/src/hostapi/asio/pa_asio.cpp
endif
endif

# empty var for headers list footer
empty=

# include the headers in the dist so you can build
# find portaudio -type file -name *.h | sort | awk '{print "   ", $1, "\\"}'; echo '     $(empty)'
nobase_noinst_HEADERS = \
    portaudio/include/pa_asio.h \
    portaudio/include/pa_jack.h \
    portaudio/include/pa_linux_alsa.h \
    portaudio/include/pa_mac_core.h \
    portaudio/include/pa_win_waveformat.h \
    portaudio/include/pa_win_wmme.h \
    portaudio/include/portaudio.h \
    portaudio/src/common/pa_allocation.h \
    portaudio/src/common/pa_converters.h \
    portaudio/src/common/pa_cpuload.h \
    portaudio/src/common/pa_debugprint.h \
    portaudio/src/common/pa_dither.h \
    portaudio/src/common/pa_endianness.h \
    portaudio/src/common/pa_gitrevision.h \
    portaudio/src/common/pa_hostapi.h \
    portaudio/src/common/pa_memorybarrier.h \
    portaudio/src/common/pa_process.h \
    portaudio/src/common/pa_ringbuffer.h \
    portaudio/src/common/pa_stream.h \
    portaudio/src/common/pa_trace.h \
    portaudio/src/common/pa_types.h \
    portaudio/src/common/pa_util.h \
    portaudio/src/hostapi/asio/iasiothiscallresolver.h \
    portaudio/src/hostapi/coreaudio/pa_mac_core_blocking.h \
    portaudio/src/hostapi/coreaudio/pa_mac_core_internal.h \
    portaudio/src/hostapi/coreaudio/pa_mac_core_utilities.h \
    portaudio/src/os/unix/pa_unix_util.h \
    portaudio/src/os/win/pa_win_coinitialize.h \
     $(empty)
