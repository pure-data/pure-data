#!/bin/sh

PATH=/sw/bin:$PATH

PWD=${0%/*}


## git cannot really handle empty directories
## so let's create the missing ones
mkdir -p ${PWD}/m4/generated

case `uname -s` in
    MINGW*)
# autoreconf doesn't always work on MinGW
	aclocal --force -I m4/generated -I m4 && \
        libtoolize --install --force && \
	autoconf --force && \
	automake --add-missing --copy --force-missing && \
	true
        ;;
    *)
        autoreconf --install --force --verbose
    ;;
esac
