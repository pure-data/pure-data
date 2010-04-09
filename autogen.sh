#!/bin/sh

PATH=/sw/bin:$PATH

case `uname -s` in
    MINGW*)
# autoreconf doesn't always work on MinGW
        libtoolize --install --force \
	        && aclocal \
	        && automake --add-missing --force-missing \
	        && autoconf
        ;;
    *)
        autoreconf --install --force --verbose
    ;;
esac
