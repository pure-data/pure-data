# This is a makefile to build "test_libpd".  It assumes that libpd is in the 
# source directory "../" and that libpd is already built.

# detect platform
UNAME = $(shell uname)
SOLIB_PREFIX = lib

ifeq ($(UNAME), Darwin) # Mac
  SOLIB_EXT = dylib
  STATICLIB_EXT = a
  PLATFORM = mac
else
  ifeq ($(OS), Windows_NT) # Windows, use Mingw
    SOLIB_PREFIX =
    SOLIB_EXT = dll
    STATICLIB_EXT = lib
    PLATFORM = windows
  else # assume Linux
    SOLIB_EXT = so
    STATICLIB_EXT = a
    PLATFORM = linux
  endif
endif

PD_DIR = ../..
LIBPD_DIR = ..
LIBPD = $(SOLIB_PREFIX)pd.$(SOLIB_EXT)
LIBPD_STATIC = $(LIBPD_DIR)/libpd.$(STATICLIB_EXT)

SRC_FILES = test_libpd.c
TARGET = test_libpd

CFLAGS = -I$(PD_DIR)/src -O3

.PHONY: libs clean-libs clean clobber

all: $(TARGET)

##### libs

$(LIBPD):
	cp $(LIBPD_DIR)/$(LIBPD) .

# on windows, copy libpd and MinGW winpthread dll to here
ifeq ($(PLATFORM), windows)

LDFLAGS = $(LIBPD)

PTHREAD_DIR = ${MINGW_PREFIX}/bin
PTHREAD = libwinpthread-1.dll

$(PTHREAD):
	cp $(PTHREAD_DIR)/$(PTHREAD) .

libs: $(LIBPD) $(PTHREAD)

clean-libs:
	rm -f $(LIBPD) $(PTHREAD)

# mac & linux
else

# force static linking? make STATIC=true
ifeq ($(STATIC), true)
LDFLAGS = $(LIBPD_STATIC)
else # copy libpd to here by default
LDFLAGS = $(LIBPD)
libs: $(LIBPD)
endif

clean-libs:
	rm -f $(LIBPD)

endif

##### target

$(TARGET): ${SRC_FILES:.c=.o} libs
	$(CC) -o $@ ${SRC_FILES:.c=.o} $(LDFLAGS)

##### clean

clean: clean-libs
	rm -f $(TARGET) *.o
