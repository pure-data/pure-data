#! /bin/bash
#
# Downloads and builds a Wish.app with with a
# chosen Tcl/TK framework version.
#
# Check available versions at https://sourceforge.net/projects/tcl/files/Tcl
#
# This script relies on the unpacked source paths using the following naming:
# * tcl: tcl${VERSION}, ie. tcl8.5.19
# * tk:   tk${VERSION}, ie. tk8.5.19
#
# Custom tcl/tk source patches needed by Pd are found in the mac/patches
# directory and are applied based on their initial naming: for example,
# "tcl8.5.19_somefix.patch" is applied to the tcl8.5.19 source path
#
# References:
# * http://www.tcl.tk/software/tcltk
# * tk distribution macosx/README
#
# Dan Wilcox danomatika.com
#

# stop on error
set -e

UNIVERSAL=false
ARCH=
GIT=false
DOWNLOAD=true
BUILD=true
LEAVE=false
PATCHES=true

# set deployment target to enable weak-linking for older macOS version support
CFLAGS="-mmacosx-version-min=10.6 $CFLAGS"

# Help message
#----------------------------------------------------------
help() {
echo -e "
Usage: tcltk-wish.sh [OPTIONS] VERSION

  Downloads and builds a Wish-VERSION.app for macOS
  with the chosen Tcl/Tk framework version,
  optionally applies custom patches as needed by Pd

Options:
  -h,--help           display this help message

  --arch ARCH         choose a specific arch ie. i386, x86_64
  
  --universal         \"universal\" multi-arch build:
                      i386 & x86_64 (& ppc if 10.6 SDK found)

  --git               clone from tcl/tk git repos at https://github.com/tcltk,
                      any arguments after VERSION are passed to git

  -d,--download       download source to tcl{$VERSION}/tk{$VERSION} paths,
                      do not build

  -b,--build          (re)build from tcl{$VERSION}/tk{$VERSION} source paths,
                      do not download

  -l,--leave          leave source paths, do not delete after building

  --no-patches        do not apply any local patches to the tcl or tk sources

Arguments:

  VERSION             Tcl/Tk version (required)

Examples:

    # build Wish-8.5.19.app with embedded Tcl/Tk 8.5.19
    tcltk-wish.sh 8.5.19

    # build 32bit Wish-8.6.6.app with embedded Tcl/Tk 8.6.6
    tcltk-wish.sh --arch i386 8.6.6

    # build Wish-8.6.6.app with embedded Tcl/Tk 8.6.6
    # and universal archs
    tcltk-wish.sh --universal 8.6.6

    # build Wish-master-git.app with the latest master branch from git
    tcltk-wish.sh --git master-git 

    # build Wish-8.6.6-git.app with embedded Tcl/Tl 8.6.6
    # from git using the core_8_6_6 tag in the master branch
    tcltk-wish.sh --git 8.6.6-git -b master core_8_6_6

    # download the tcl8.5.19 and tk8.5.19 source paths, do not build
    tcltk-wish.sh --download 8.5.19

    # build from existing tcl8.5.19 and tk8.5.19 source paths, do not download
    # note: --leave ensures the source trees are not deleted after building
    tcltk-wish.sh --build --leave 8.5.19
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
        --git)
            GIT=true
            ;;
        -d|--download)
            DOWNLOAD=true
            BUILD=false
            ;;
        -b|--build)
            DOWNLOAD=false
            BUILD=true
            ;;
        -l|--leave)
            LEAVE=true
            ;;
        --no-patches)
            PATCHES=false
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
TCLTK=$1
shift 1

# Go
#----------------------------------------------------------

WISH=$(pwd)/Wish-${TCLTK}.app

# change to the dir of this script
cd $(dirname $0)

# remove old app if found
if [ -d "$WISH" ] ; then
    rm -rf "$WISH"
fi

tcldir=tcl${TCLTK}
tkdir=tk${TCLTK}

if [[ $DOWNLOAD == true ]] ; then
    echo "==== Downloading Tcl/Tk $TCLTK"

    if [[ $GIT == true ]] ; then

        # shallow clone sources, pass remaining args to git
        git clone --depth 1 https://github.com/tcltk/tcl.git tcl${TCLTK} $@
        git clone --depth 1 https://github.com/tcltk/tk.git tk${TCLTK} $@
    else

        # download source packages
        curl -LO http://prdownloads.sourceforge.net/tcl/tcl${TCLTK}-src.tar.gz
        curl -LO http://prdownloads.sourceforge.net/tcl/tk${TCLTK}-src.tar.gz

        # unpack
        tar -xzf tcl${TCLTK}-src.tar.gz
        tar -xzf tk${TCLTK}-src.tar.gz
    fi
else
    echo  "==== Using sources in $tcldir $tkdir"

    # check source directories
    if [ ! -e "$tcldir" ] ; then
        echo "$tcldir not found"
        exit 1
    fi
    if [ ! -e "$tkdir" ] ; then
        echo "$tkdir not found"
        exit 1
    fi
fi

if [[ $BUILD == false ]] ; then
    echo  "==== Downloaded sources to $tcldir $tkdir"
    exit 0
fi

# apply patches, note: this probably can't handle filenames with spaces
# temp disable exit on error since the exit value of patch --dry-run is used
if [[ $PATCHES == true ]] ; then
    set +e
    for p in $(find ./patches -type f -name "tcl${TCLTK}*.patch") ; do
        cd tcl${TCLTK}
        (patch -p1 -N --silent --dry-run --input "../${p}" > /dev/null 2>&1)
        if [[ $? == 0 ]] ; then
            echo "==== Applying $p"
            patch -p1 < "../${p}"
        fi
        cd ../
    done
    for p in $(find ./patches -type f -name "tk${TCLTK}*.patch") ; do
        cd tk${TCLTK}
        (patch -p1 -N --silent --dry-run --input "../${p}" > /dev/null 2>&1)
        if [[ $? == 0  ]] ; then
            echo "==== Applying $p"
            patch -p1 < "../${p}"
        fi
        cd ../
    done
    set -e
fi

echo "==== Building Tcl/Tk Wish-$TCLTK.app"

# set a specific arch
if [ "$ARCH" != "" ] ; then
    CFLAGS="-arch $ARCH $CFLAGS"
fi

# try a universal build
if [ $UNIVERSAL == true ] ; then
    CFLAGS="-arch i386 -arch x86_64 $CFLAGS"
    # check if the 10.6 SDK is available, if so we can build for ppc
    if [ xcodebuild -version -sdk macosx10.6 Path >/dev/null 2>&1 ] ; then
        CFLAGS="-arch ppc $CFLAGS"
    fi
fi

# set any custom flags
export CFLAGS

# build Tcl and Tk
# outputs into local "build" & "embedded" directories 
make -C ${tcldir}/macosx embedded
make -C ${tkdir}/macosx embedded
make -C ${tcldir}/macosx install-embedded INSTALL_ROOT=`pwd`/embedded
make -C ${tkdir}/macosx  install-embedded INSTALL_ROOT=`pwd`/embedded

# move Wish.app located in "embedded"
mv embedded/Applications/Utilities/Wish.app $WISH

# finish up
if [[ $LEAVE == false ]] ; then
    rm -rf ${tcldir} ${tkdir} ${tcldir}-src.tar.gz ${tkdir}-src.tar.gz
    rm -rf build embedded
fi

echo  "==== Finished Tcl/Tk Wish-$TCLTK.app"
