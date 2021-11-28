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

.PHONY: clean clobber

# move libpd dylib to local folder on mac
$(TARGET): ${SRC_FILES:.c=.o} $(LIBPD_DIR)/$(LIBPD)
	$(CC) -o $@ $^ $(LDFLAGS)
	if [ $(PLATFORM) = "mac" ]; then cp $(LIBPD_DIR)/$(LIBPD) ./; fi

clean:
	rm -f $(TARGET) *.o $(LIBPD)
