# This is a makefile to build "test_libpd".  It assumes that libpd is in the 
# source directory "../" and that libpd is already built.

# detect platform
UNAME = $(shell uname)
SOLIB_PREFIX = lib

ifeq ($(UNAME), Darwin) # Mac
  SOLIB_EXT = dylib
  PLATFORM = mac
else
  ifeq ($(OS), Windows_NT) # Windows, use Mingw
    SOLIB_PREFIX =
    SOLIB_EXT = dll
    PLATFORM = windows
  else # assume Linux
    SOLIB_EXT = so
    PLATFORM = linux
  endif
endif

PD_DIR = ../..
LIBPD_DIR = ..
LIBPD = $(SOLIB_PREFIX)pd.$(SOLIB_EXT)

SRC_FILES = test_libpd.c
TARGET = test_libpd

CFLAGS = -I$(PD_DIR)/src -O3
LDFLAGS = $(LIBPD)

.PHONY: libs clean-libs clean clobber

##### all

$(TARGET): ${SRC_FILES:.c=.o} libs
	$(CC) -o $@ ${SRC_FILES:.c=.o} $(LDFLAGS)

##### libs

$(LIBPD):
	cp $(LIBPD_DIR)/$(LIBPD) .

# on windowes, copy libpd and MinGW winpthread dll to here
ifeq ($(PLATFORM), windows)

PTHREAD_DIR = ${MINGW_PREFIX}/bin
PTHREAD = libwinpthread-1.dll

$(PTHREAD):
	cp $(PTHREAD_DIR)/$(PTHREAD) .

libs: $(LIBPD) $(PTHREAD)

clean-libs:
	rm -f $(LIBPD) $(PTHREAD)

# mac & linux, copy libpd to here
else

libs: $(LIBPD)

clean-libs:
	rm -f $(LIBPD)

endif

##### clean

clean: clean-libs
	rm -f $(TARGET) *.o
