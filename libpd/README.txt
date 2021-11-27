This directory contains files for building and testing libpd.so from vanilla
Pd sources.  It assumes that the Pd source directory "../src" is parallel to
this directory in order to find the Pd sources.

To build libpd, in this directory, type "make".  This is tested in linux but
not in macOS or in Windows.

This directory also contains a test program "pdtest_multi" with its own
separate makefile.  To build and run the test program:
$ make -f makefile.example
$ ./test_libpd

