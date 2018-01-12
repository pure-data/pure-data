#! /bin/bash
#
# Downloads and builds a directory with the chosen Tcl/Tk version.
#
# Check available versions at https://sourceforge.net/projects/tcl/files/Tcl
#
# References:
# * http://www.tcl.tk/software/tcltk
# * tk distribution macosx/README
#
# TODO:
# * install layout does not *exactly* match pdprototype, does it need to?
# * find a way to convert tcl & tk static .a libraries to VS .lib
#
# Dan Wilcox danomatika.com
#

# stop on error
set -e

# default configure options
OPTIONS="--enable-threads"

# cloning with git?
GIT=false

# force 64 bit build?
FORCE64BIT=false

# Help message
#----------------------------------------------------------
help() {
echo -e "
Usage: tcltk-dir.sh [OPTIONS] VERSION

  Downloads and builds a directory for Windows
  with the chosen Tcl/Tk version.

Options:
  -h,--help           display this help message

  --git               clone from tcl/tk git repos at https://github.com/tcltk,
                      any arguments after VERSION are passed to git
                      
  --64bit             force building for 64 bit Windows, use this if 64 bit
                      auto detection does not work

Arguments:

  VERSION             Tcl/Tk version (required)

Examples:

    # build tcltk-8.5.19 directory with Tcl/Tk 8.5.19
    tcltk-dir.sh 8.5.19

    # build tcltk-master-git with the latest master branch from git
    tcltk-dir.sh --git master-git 

    # build tcltk-8.6.6-git with Tcl/Tl 8.6.6
    # from git using the core_8_6_6 tag in the master branch
    tcltk-dir.sh --git 8.6.6-git -b master core_8_6_6
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
        --git)
            GIT=true
            ;;
        --64bit)
            FORCE64BIT=true
            ;;
        *)
            break ;;
    esac
    shift 1
done

# check for required version argument
if [ "$1" == "" ] ; then
    echo "Usage: tcltk-dir.sh [OPTIONS] VERSION"
    exit 1
fi
TCLTK=$1
shift 1

# Go
#----------------------------------------------------------

DEST=$(pwd)/tcltk-${TCLTK}

# change to the dir of this script
cd $(dirname $0)

# remove old dir if found
if [ -d "$DEST" ] ; then
    rm -rf $DEST
fi
mkdir -p $DEST

echo "==== Creating tcltk-$TCLTK"

if [ "x$GIT" = xtrue ] ; then

    # shallow clone sources, pass remaining args to git
    git clone --depth 1 https://github.com/tcltk/tcl.git $@
    git clone --depth 1 https://github.com/tcltk/tk.git $@

    tcldir=tcl
    tkdir=tk
else

    # download source packages
    curl -LO http://prdownloads.sourceforge.net/tcl/tcl${TCLTK}-src.tar.gz
    curl -LO http://prdownloads.sourceforge.net/tcl/tk${TCLTK}-src.tar.gz

    # unpack
    tar -xzf tcl${TCLTK}-src.tar.gz
    tar -xzf tk${TCLTK}-src.tar.gz

    tcldir=tcl${TCLTK}
    tkdir=tk${TCLTK}
fi


if [ "x${FORCE64BIT}" = xtrue ] ; then
  echo "Forcing 64 bit"
  OPTIONS="$OPTIONS --enable-64bit"
else
  # try to detect 64 bit MinGW: uname -s -> MINGW64_NT-10.0
  # don't use uname -m as that returns the *system* archtecture
  # which may be different, ie. MinGW 32 running on 64 bit Windows  
  if [ $(uname -s | grep 64) ] ; then
    echo "Detected 64 bit"
    OPTIONS="$OPTIONS --enable-64bit"
  fi
fi

# build Tcl and Tk
cd ${tcldir}/win
./configure $OPTIONS --prefix=$DEST
make
make install
cd -
cd ${tkdir}/win
./configure $OPTIONS --prefix=$DEST
make
make install
cd -

# finish up
rm -rf ${tcldir} ${tkdir} ${tcldir}-src.tar.gz ${tkdir}-src.tar.gz

echo  "==== Finished tcltk-$TCLTK"
