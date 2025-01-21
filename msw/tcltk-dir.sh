#! /bin/sh
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
# * disable building bundled Tcl packages Pd doesn't need: iTcl, Sqlite, TDBC
#
# Dan Wilcox danomatika.com
#

# stop on error
set -e

# cloning with git?
GIT=false

# force 64 bit build?
FORCE64BIT=false

# configure options
if [ -z "${OPTIONS}" ] ; then
    OPTIONS="--enable-threads"
fi

# Help message
#----------------------------------------------------------
help() {
cat <<EOF

Usage: $0 [OPTIONS] VERSION

  Downloads and builds a directory for Windows
  with the chosen Tcl/Tk version using:

      configure ${OPTIONS}

Options:
  -h,--help           display this help message

  --git               clone from tcl/tk git repos at https://github.com/tcltk,
                      any arguments after VERSION are passed to git

  --64bit             force building for 64 bit Windows, use this if 64 bit
                      auto detection does not work (default: ${FORCE64BIT})

Arguments:

  VERSION             Tcl/Tk version (required)

Examples:

    # build tcltk-8.5.19 directory with Tcl/Tk 8.5.19
    $0 8.5.19

    # build tcltk-master-git with the latest master branch from git
    $0 --git master-git

    # build tcltk-8.6.6-git with Tcl/Tl 8.6.6
    # from git using the core_8_6_6 tag in the master branch
    $0 --git 8.6.6-git -b master core_8_6_6

    # override default Tcl/Tk configure options
    OPTIONS="--enable-foo --with-bar" $0 8.5.19

EOF
}

# Parse command line arguments
#----------------------------------------------------------
while [ "$1" != "" ] ; do
    case "$1" in
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
if [ "$1" = "" ] ; then
    echo "Usage: $0 [OPTIONS] VERSION" 1>&2
    exit 1
fi
TCLTK="$1"
shift 1

# Go
#----------------------------------------------------------

DEST="$(pwd)/tcltk-${TCLTK}"

# change to the dir of this script
cd "$(dirname "$0")"

# remove old dir if found
if [ -d "${DEST}" ] ; then
    rm -rf "${DEST}"
fi
mkdir -p "${DEST}"


# 64 bit support
if [ "${FORCE64BIT}" = true ] ; then
    echo "==== Forcing 64 bit"
    OPTIONS="${OPTIONS} --enable-64bit"
else
    # try to detect 64 bit MinGW using MSYSTEM env var
    case "${MSYSTEM}" in
        MINGW64 | UCRT64 | CLANG64)
            echo "==== Detected 64 bit"
            OPTIONS="${OPTIONS} --enable-64bit"
            ;;
        CLANGARM64)
            echo "==== Detected 64 bit (arm)"
            OPTIONS="${OPTIONS} --enable-64bit=arm64"
            ;;
    esac
fi


echo "==== Creating tcltk-${TCLTK}"

# fetch Tcl and Tk
if [ "${GIT}" = true ] ; then
    echo "---- Cloning TclTk"

    # shallow clone sources, pass remaining args to git
    git clone --depth 1 https://github.com/tcltk/tcl.git "$@"
    git clone --depth 1 https://github.com/tcltk/tk.git "$@"

    tcldir=tcl
    tkdir=tk
else
    echo "---- Downloading TclTk-${TCLTK}"

    # download source packages
    curl -LO "http://prdownloads.sourceforge.net/tcl/tcl${TCLTK}-src.tar.gz"
    curl -LO "http://prdownloads.sourceforge.net/tcl/tk${TCLTK}-src.tar.gz"

    # unpack
    tar -xzf "tcl${TCLTK}-src.tar.gz"
    tar -xzf "tk${TCLTK}-src.tar.gz"

    tcldir="tcl${TCLTK}"
    tkdir="tk${TCLTK}"
fi

# build Tcl and Tk
echo "---- Building Tcl-${TCLTK}"
cd "${tcldir}/win"
./configure ${OPTIONS} --prefix="${DEST}"
make
make install
cd -
echo "---- Building Tk-${TCLTK}"
cd "${tkdir}/win"
./configure ${OPTIONS} --prefix="${DEST}"
make
make install
cd -

# finish up
echo "---- Cleaning up TclTk-${TCLTK}"
rm -rf "${tcldir}" "${tkdir}" "${tcldir}-src.tar.gz" "${tkdir}-src.tar.gz"

echo  "==== Finished tcltk-${TCLTK}"
