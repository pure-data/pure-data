# The ASIO sources are not free, so they are not included here.  You can get
# them for free (as in beer) from Steinberg:

# https://www.steinberg.net/en/company/developer.html

# First, download the ASIO SDK (asiosdk2.3 or higher), then unzip it right into
# the /asio folder. You should have all of the ASIO files in pure-data/asio/ASIOSDK
# when you are done unzipping. (You'll probably have to remove the version number
# from the folder name.)

# this is built as a static convenience library

#########################################
##### Defaults & Paths #####

AUTOMAKE_OPTIONS = foreign
AM_CFLAGS =
AM_CPPFLAGS += -I$(srcdir)/ASIOSDK/host -I$(srcdir)/ASIOSDK/host/pc -I$(srcdir)/ASIOSDK/common
if MINGW
# hack for /asio/ASIOSDK/common/combase.h
AM_CPPFLAGS += -DWINVER=0x0502 -D_WIN32_WINNT=0x0502
endif

#########################################
##### Files, Binaries, & Libs #####

noinst_LIBRARIES = libasio.a

nodist_libasio_a_SOURCES = \
    ASIOSDK/common/asio.cpp \
    ASIOSDK/host/asiodrivers.cpp \
    ASIOSDK/host/pc/asiolist.cpp

EXTRA_DIST = README.txt
