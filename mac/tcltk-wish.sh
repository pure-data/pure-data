#! /bin/bash
#
# Downloads and builds a Wish.app with with a
# chosen Tcl/TK framework version.
#
# References:
# * http://www.tcl.tk/software/tcltk
# * tk distribution macosx/README
#
# Dan Wilcox danomatika.com
#

# stop on error
set -e

WD=$(dirname $0)

UNIVERSAL=false
ARCH=

# Help message
#----------------------------------------------------------
help() {
echo -e "
Usage: tcltk-wish.sh [OPTIONS] VERSION

  Downloads and builds a Wish-VERSION.app with the
  chosen Tcl/Tk framework version

Options:
  -h,--help           display this help message

  --arch ARCH         choose a specific arch ie. i386, x86_64
  
  --universal         \"universal\" multi-arch build:
                      i386 & x86_64 (& ppc if 10.6 SDK found)

Arguments:

  VERSION             Tcl/TK version (required)

Examples:

    # build Wish-8.5.19.app with embedded Tcl/Tk 8.5.19
    tcltk-wish.sh 8.5.19

    # build Wish-8.6.5.app with embedded Tcl/Tk 8.6.5
    # and universal archs
    tcltk-wish.sh --universal 8.6.5
"
}

# Parse command line arguments
#----------------------------------------------------------
while [ "$1" != "" ] ; do
    case $1 in
        -h|--help)
            help
            exit 0
            ;;
        --arch)
            if [ $# == 0 ] ; then
                echo "--arch option requires an ARCH argument"
                exit 1
            fi
            shift 1
            ARCH=$1
            ;;
        --universal)
            UNIVERSAL=true
            ;;
        *)
            break ;;
    esac
    shift 1
done

# check for required version argument
if [ "$1" == "" ] ; then
    echo "Usage: tcltk-wish.sh [OPTIONS] VERSION"
    exit 1
fi

# Go
#----------------------------------------------------------

TCLTK=$1

cd $WD

# remove old app if found
if [ -d Wish-${TCLTK}.app ] ; then
    rm -rf Wish-${TCLTK}.app
fi

echo "==== Creating Tcl/Tk $TCLTK Wish.app"

# download source packages
curl -LO http://prdownloads.sourceforge.net/tcl/tcl${TCLTK}-src.tar.gz
curl -LO http://prdownloads.sourceforge.net/tcl/tk${TCLTK}-src.tar.gz

# unpack
tar -xzf tcl${TCLTK}-src.tar.gz
tar -xzf tk${TCLTK}-src.tar.gz

# set a specific arch
if [ "$ARCH" != "" ] ; then
    CFLAGS="-arch $ARCH $CFLAGS"
    export CFLAGS
fi

# try a universal build
if [ $UNIVERSAL == true ] ; then
    CFLAGS="-arch i386 -arch x86_64 -mmacosx-version-min=10.5 $CFLAGS"
    # check if the 10.6 SDK is available, if so we can build for ppc
    if [ xcodebuild -version -sdk macosx10.6 Path >/dev/null 2>&1 ] ; then
        CFLAGS="-arch ppc $CFLAGS"
    fi
    export CFLAGS
fi

# build Tcl and Tk
# outputs into a local "build" & "embedded" directories 
make -C tcl${TCLTK}/macosx embedded
make -C tk${TCLTK}/macosx embedded
make -C tcl${TCLTK}/macosx install-embedded INSTALL_ROOT=`pwd`/embedded
make -C tk${TCLTK}/macosx  install-embedded INSTALL_ROOT=`pwd`/embedded

# move Wish.app located in "embedded"
mv embedded/Applications/Utilities/Wish.app ./Wish-${TCLTK}.app

# finish up
rm -rf tcl${TCLTK}* tk${TCLTK}*
rm -rf build embedded

echo  "==== Finished Tcl/Tk $TCLTK Wish.app"
