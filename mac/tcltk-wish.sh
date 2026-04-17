#! /bin/sh
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
tcltk_macos_min=10.6
macos_version_min="${tcltk_macos_min}"
override_macos_version_min="no"

# Help message
#----------------------------------------------------------
help() {
cat <<EOF
Usage: $0 [OPTIONS] VERSION

  Downloads and builds a Wish-VERSION.app for macOS
  with the chosen Tcl/Tk framework version,
  optionally applies custom patches as needed by Pd

Options:
  -h,--help           display this help message

  --arch ARCH         choose a specific arch ie. ppc, i386, x86_64, arm64

  --universal         "universal" multi-arch build based on detected macOS SDK
                      10.6:          ppc i386 x86_64
                      10.7  - 10.13: i386 x86_64
                      10.14 - 10.15: x86_64
                      11.0+:         arm64 x86_64

  --macosx-version-min VER
		      compile for the given macOS VERsion onwards (DEFAULT: ${macos_version_min})

  --git               clone from tcl/tk git repos at https://github.com/tcltk,
                      any arguments after VERSION are passed to git

  -d,--download       download source to tcl{$VERSION}/tk{$VERSION} paths,
                      do not build

  --no-build          do not build

  -b,--build          (re)build from tcl{$VERSION}/tk{$VERSION} source paths,
                      do not download

  --no-download       do not download

  -k,--keep           keep source paths, do not delete after building

  --no-patches        do not apply any local patches to the tcl or tk sources

Arguments:

  VERSION             Tcl/Tk version (required)

Examples:

    # build Wish-8.5.19.app with embedded Tcl/Tk 8.5.19
    $0 8.5.19

    # build 32bit Wish-8.6.6.app with embedded Tcl/Tk 8.6.6
    $0 --arch i386 8.6.6

    # build Wish-8.6.6.app with embedded Tcl/Tk 8.6.6
    # and universal archs (detected from Xcode macOS SDK)
    $0 --universal 8.6.6

    # build Wish-master-git.app with the latest master branch from git
    $0 --git master-git

    # build Wish-8.6.6-git.app with embedded Tcl/Tl 8.6.6
    # from git using the core_8_6_6 tag in the master branch
    $0 --git 8.6.6-git -b master core_8_6_6

    # download the tcl8.5.19 and tk8.5.19 source paths, do not build
    $0 --download 8.5.19

    # build from existing tcl8.5.19 and tk8.5.19 source paths, do not download
    # note: --keep ensures the source trees are not deleted after building
    $0 --build --keep 8.5.19

EOF
}

buildinfo() {
    cat <<EOF
# Tcl/Tk
${TCLTK}

# macOS
EOF
sw_vers

cat <<EOF

# XCode
EOF
pkgutil --pkg-info=com.apple.pkg.CLTools_Executables


cat <<EOF

# buildflags
EOF
echo "CFLAGS: ${CFLAGS}"
}

# Parse command line arguments
#----------------------------------------------------------
while [ "$1" != "" ] ; do
    case "$1" in
        -h|--help)
            help
            exit 0
            ;;
        --arch)
            if [ $# = 0 ] ; then
                echo "--arch option requires an ARCH argument"
                exit 1
            fi
            shift 1
            ARCH=$1
            ;;
        --universal)
            UNIVERSAL=true
            ;;
        --macosx-version-min|--macos-version-min)
            if [ $# = 0 ] ; then
                echo "--macosx-version-min option requires a version argument"
                exit 1
            fi
            shift 1
            macos_version_min="${1}"
            override_macos_version_min="yes"
            ;;
        --git)
            GIT=true
            ;;
        -d|--download)
            DOWNLOAD=true
            BUILD=false
            ;;
        --no-download)
            DOWNLOAD=false
            ;;
        -b|--build)
            DOWNLOAD=false
            BUILD=true
            ;;
        --no-build)
            BUILD=false
            ;;
        -l|--leave|-k|--keep)
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
if [ "$1" = "" ] ; then
    echo "Usage: tcltk-wish.sh [OPTIONS] VERSION"
    exit 1
fi
TCLTK="$1"
shift 1


# TCLTK>=9 needs a macos_version_min>=10.10(?)
if { echo "${TCLTK}"; echo 9; } | sort -Vr | head -1 | grep -Fwq "${TCLTK}"; then
    # TCLTK>=9
    tcltk_macos_min=10.9
fi
if [ "${override_macos_version_min}" = "no" ]; then
    macos_version_min="${tcltk_macos_min}"
elif [ -n "${macos_version_min}" ]; then
    if { echo "${macos_version_min}"; echo "${tcltk_macos_min}"; } | sort -Vr | head -1 | grep -Fwq "${macos_version_min}"; then
        echo "TclTk-${TCLTK} needs at least macosx_version_min=${tcltk_macos_min} ...all good!"
    else
        echo "TclTk-${TCLTK} needs at least macosx_version_min=${tcltk_macos_min} ...proceeding with ${macos_version_min} at your own risk!"
    fi
fi

# set deployment target to enable weak-linking for older macOS version support
if [ -n "${macos_version_min}" ]; then
	CFLAGS="-mmacosx-version-min=${macos_version_min} $CFLAGS"
fi

# Go
#----------------------------------------------------------

WISH="$(pwd)/Wish-${TCLTK}.app"

# change to the dir of this script
cd "$(dirname "$0")"

# remove old app if found
if [ -d "$WISH" ] ; then
    rm -rf "$WISH"
fi

tcldir=tcl${TCLTK}
tkdir=tk${TCLTK}

if [ $DOWNLOAD = true ] ; then
    echo "==== Downloading Tcl/Tk $TCLTK"

    if [ $GIT = true ] ; then

        # shallow clone sources, pass remaining args to git
        git clone --depth 1 "https://github.com/tcltk/tcl.git tcl${TCLTK}" "$@"
        git clone --depth 1 "https://github.com/tcltk/tk.git tk${TCLTK}" "$@"
    else

        # download source packages
        curl -LO "http://prdownloads.sourceforge.net/tcl/tcl${TCLTK}-src.tar.gz"
        curl -LO "http://prdownloads.sourceforge.net/tcl/tk${TCLTK}-src.tar.gz"

        # unpack
        tar -xzf "tcl${TCLTK}-src.tar.gz"
        tar -xzf "tk${TCLTK}-src.tar.gz"
    fi
else
    # check source directories
    if [ ! -d "$tcldir" ] && [ -e "tcl${TCLTK}-src.tar.gz" ] ; then
        echo "---- Extracting tcl${TCLTK}-src.tar.gz ---"
        tar -xzf "tcl${TCLTK}-src.tar.gz" || exit 1
    fi
    if [ ! -d "$tkdir" ] && [ -e "tk${TCLTK}-src.tar.gz" ] ; then
        echo "---- Extracting tk${TCLTK}-src.tar.gz --- "
        tar -xzf "tk${TCLTK}-src.tar.gz" || exit 1
    fi

    echo  "==== Using sources in $tcldir $tkdir"

    # check source directories
    if [ ! -d "$tcldir" ] ; then
        echo "$tcldir not found"
        exit 1
    fi
    if [ ! -d "$tkdir" ] ; then
        echo "$tkdir not found"
        exit 1
    fi
fi

if [ $BUILD = false ] ; then
    echo  "==== Downloaded sources to $tcldir $tkdir"
    exit 0
fi

# apply patches, note: this probably can't handle filenames with spaces
# temp disable exit on error since the exit value of patch --dry-run is used
if [ $PATCHES = true ] ; then
    set +e
    for p in $(find ./patches -type f -name "tcl${TCLTK}*.patch") ; do
        cd "tcl${TCLTK}"
        (patch -p1 -N --silent --dry-run --input "../${p}" > /dev/null 2>&1)
        if [ $? = 0 ] ; then
            echo "==== Applying $p"
            patch -p1 < "../${p}"
        fi
        cd ../
    done
    for p in $(find ./patches -type f -name "tk${TCLTK}*.patch") ; do
        cd "tk${TCLTK}"
        (patch -p1 -N --silent --dry-run --input "../${p}" > /dev/null 2>&1)
        if [ $? = 0 ] ; then
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
if [ $UNIVERSAL = true ] ; then

    # detect macOS SDK from Xcode toolchain version & deduce archs
    XCODE_VER=$(pkgutil --pkg-info=com.apple.pkg.CLTools_Executables | \
                grep "version" | sed "s/[^0-9]*\([0-9]*\).*/\1/")
    if [ "$XCODE_VER" = "" ] ; then
        # no CLTools, try xcodebuild
        XCODE_VER=$(xcodebuild -version | grep "Xcode" | \
                    sed "s/[^0-9]*\([0-9]*\).*/\1/")
    fi
    if [ "${XCODE_VER}" -gt 11 ] ; then
        # Xcode 12+: 11.0+
        CFLAGS="-arch x86_64 -arch arm64 $CFLAGS"
    elif [ "${XCODE_VER}" -gt 9 ] ; then
        # Xcode 10 - 11: 10.14 - 10.15
        echo "warning: Xcode version $XCODE_VER only builds x86_64"
        CFLAGS="-arch x86_64 $CFLAGS"
    elif [ "${XCODE_VER}" -gt 3 ] ; then
        # Xcode 4 - 9: 10.7 - 10.13
        CFLAGS="-arch i386 -arch x86_64 $CFLAGS"
    elif [ "${XCODE_VER}" = "3" ] ; then
        # Xcode 3: 10.6
        CFLAGS="-arch ppc -arch i386 -arch x86_64 $CFLAGS"
    else
        echo "warning: unknown or unsupported Xcode version, trying i386 x86_64"
        CFLAGS="-arch i386 -arch x86_64 $CFLAGS"
    fi
fi

# set any custom flags
export CFLAGS

# build Tcl and Tk
# outputs into local "build" & "embedded" directories
make -C "${tcldir}/macosx" embedded
make -C "${tkdir}/macosx" embedded
make -C "${tcldir}/macosx" install-embedded INSTALL_ROOT="$(pwd)/embedded"
make -C "${tkdir}/macosx"  install-embedded INSTALL_ROOT="$(pwd)/embedded"

# move Wish.app located in "embedded"
mv embedded/Applications/Utilities/Wish.app "${WISH}"
buildinfo > "${WISH}/Contents/Resources/tcltck_buildinfo.txt"

# finish up
if [ $LEAVE = false ] ; then
    rm -rf "${tcldir}" "${tkdir}" "${tcldir}-src.tar.gz" "${tkdir}-src.tar.gz"
    rm -rf build embedded
fi

echo  "==== Finished Tcl/Tk Wish-$TCLTK.app"
