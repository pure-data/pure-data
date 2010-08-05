#!/bin/sh

PATH=/sw/bin:$PATH

PWD=${0%/*}


## git cannot really handle empty directories
## so let's create the missing ones
mkdir -p ${PWD}/m4/generated

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
