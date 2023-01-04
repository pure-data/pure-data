This directory contains files for building and testing libpd from vanilla
Pd sources.  It assumes that the Pd source directory "../src" is parallel to
this directory in order to find the Pd sources.

To build libpd, in this directory, type "make".  This is tested on Linux, macOS,
and on Windows using Msys2/MinGW.

    cd libpd
    make

This directory also contains a test program "test_libpd" with its own separate
makefile.  To build and run the test program:

    cd test_libpd
    make
    ./test_libpd

On Windows, the test_libpd makefile will try to find the libwinpthread-1.dll
included with MinGW and copy it to the test_libpd directory to run the example.

On macOS and Linux, the test_libpd makefile can statically link libpd by using:

    make STATIC=true
