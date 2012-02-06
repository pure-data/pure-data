# this is built as a "libtool convenience library"
# http://sources.redhat.com/automake/automake.html#Libtool-Convenience-Libraries
AUTOMAKE_OPTIONS = foreign
AM_CPPFLAGS = 
INCLUDES = -Iinclude -Isrc/common

noinst_LTLIBRARIES = libportaudio.la

libportaudio_la_SOURCES = src/common/pa_allocation.c  \
        src/common/pa_converters.c  \
        src/common/pa_cpuload.c     \
        src/common/pa_dither.c      \
        src/common/pa_front.c       \
        src/common/pa_process.c     \
        src/common/pa_skeleton.c    \
        src/common/pa_stream.c      \
        src/common/pa_trace.c       \
        src/common/pa_debugprint.c  \
        src/common/pa_ringbuffer.c 


if ALSA
AM_CPPFLAGS += -DPA_USE_ALSA
libportaudio_la_SOURCES += src/hostapi/alsa/pa_linux_alsa.c
endif

if ASIO
libportaudio_la_CPPFLAGS = -DWINVER=0x0501 #kludge to work around Portaudio/ASIO bug
libportaudio_la_SOURCES += src/hostapi/asio/pa_asio.cpp \
	src/hostapi/asio/iasiothiscallresolver.cpp
INCLUDES += -I../asio/ASIOSDK2/host -I../asio/ASIOSDK2/host/pc -I../asio/ASIOSDK2/common
# Some old code in asio/ASIOSDK2/common/combase.h needs to be ignored,
# we do this by setting the WINVER macro to min Windows XP aka 5.1.
AM_CPPFLAGS += -DPA_USE_ASIO -DWINVER=0x0501
else
AM_CPPFLAGS += -DPA_NO_ASIO
endif

if COREAUDIO
AM_CPPFLAGS += -DPA_USE_COREAUDIO
INCLUDES += -Isrc/os/mac_osx
libportaudio_la_SOURCES += src/os/unix/pa_unix_util.c     \
        src/os/mac_osx/pa_mac_hostapis.c   \
        src/hostapi/coreaudio/pa_mac_core.c  \
        src/hostapi/coreaudio/pa_mac_core_blocking.c  \
        src/hostapi/coreaudio/pa_mac_core_utilities.c
endif

# Cygwin has a function OSS /dev/dsp, but not MIDI, and Pd is only set up to
# handle a single MIDI API
if OSS
if !WINDOWS
libportaudio_la_SOURCES += src/hostapi/oss/pa_unix_oss.c
endif
endif

if LINUX
INCLUDES +=  -Isrc/os/unix
libportaudio_la_SOURCES += src/os/unix/pa_unix_hostapis.c \
        src/os/unix/pa_unix_util.c
endif

if WINDOWS
AM_CPPFLAGS += -DPA_USE_WMM
INCLUDES += -Isrc/os/win
libportaudio_la_SOURCES += src/os/win/pa_win_hostapis.c \
        src/os/win/pa_win_util.c \
        src/os/win/pa_win_waveformat.c \
        src/hostapi/wmme/pa_win_wmme.c
endif

# include the headers in the dist so you can build
nobase_noinst_HEADERS = \
	include/pa_asio.h \
	include/pa_jack.h \
	include/pa_linux_alsa.h \
	include/pa_mac_core.h \
	include/pa_win_waveformat.h \
	include/pa_win_wmme.h \
	include/portaudio.h \
	src/common/pa_allocation.h \
	src/common/pa_converters.h \
	src/common/pa_cpuload.h \
	src/common/pa_debugprint.h \
	src/common/pa_dither.h \
	src/common/pa_endianness.h \
	src/common/pa_hostapi.h \
	src/common/pa_process.h \
	src/common/pa_ringbuffer.h \
	src/common/pa_memorybarrier.h \
	src/common/pa_stream.h \
	src/common/pa_trace.h \
	src/common/pa_types.h \
	src/common/pa_util.h \
	src/hostapi/asio/iasiothiscallresolver.h \
	src/hostapi/coreaudio/pa_mac_core_blocking.h \
	src/hostapi/coreaudio/pa_mac_core_internal.h \
	src/hostapi/coreaudio/pa_mac_core_utilities.h \
	src/os/unix/pa_unix_util.h \
	src/os/win/pa_win_wdmks_utils.h \
	src/os/win/pa_x86_plain_converters.h

