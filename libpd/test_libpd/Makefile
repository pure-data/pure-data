# This is a makefile to build "test_libpd".  It assumes that libpd is in the 
# source directory "../" and that libpd is already built before. 

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

.PHONY: libs clean-libs clean clobber

##### all

$(TARGET): ${SRC_FILES:.c=.o} libs
	$(CC) -o $@ ${SRC_FILES:.c=.o} $(LIBPD) $(LDFLAGS)

##### libs

$(LIBPD):
	cp $(LIBPD_DIR)/$(LIBPD) ./

ifeq ($(PLATFORM), windows)

# on windows, copy libpd and MinGW pthread.dll from the following to ./
# * windows:             $MINGW_PREFIX/bin
# * linux cross-compile: $MINGW_PREFIX/lib

$(PDTHREAD_DIR) = ${MINGW_PREFIX}/bin
$(PTHREAD) = libwinpthread-1.dll

$(PTHREAD):
	cp $(PTHREAD_DIR))/$(PTHREAD) ./

libs: $(LIBPD) $(PTHREAD)

clean-libs:
	rm -f $(LIBPD) $(PTHREAD)

else

libs: $(LIBPD)

clean-libs:
	rm -f $(LIBPD)

endif

##### clean

clean: clean-libs
	rm -f $(TARGET) *.o
