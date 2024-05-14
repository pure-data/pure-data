#! /bin/sh
#
# this script automatically downloads and copies the portmidi library for Pd
#
# uses git master version by default, set checkout branch/tag/commit using
# first argument:
#
#    ./update.sh c0d239712d
#
# Dan Wilcox danomatika.com 2018,2024
#

# exit on error
set -e

##### VARIABLES

SRC=portmidi-git
DEST=portmidi

# commandline arg: version can be a tag, commit, etc
# uses git master by default
VERSION=""

##### FUNCTIONS

# copy .h & .c files from $SRC to $DEST, ignore missing file errors
function copysrc {
    mkdir -p "${DEST}/${1}"
    cp -v "${SRC}/${1}"/*.h "${DEST}/${1}" 2>/dev/null || :
    cp -v "${SRC}/${1}"/*.c "${DEST}/${1}" 2>/dev/null || :
}

usage() {
cat <<EOF 1>&2
Usage: $0 [OPTIONS] [VERSION]

Options:
  -h       display this help message

  -f       force (do not abort if '${DEST}/' exists)
           this updates existing files but keeps removed files,
           only use this if you know what you are doing!

Arguments:

VERSION    tag/commit/branch of portaudio to import
EOF
}

#### ARGUMENTS

force=no
OPTIND=1
while getopts ":hf" opt; do
    case "$opt" in
        h)
            usage
            exit 0
            ;;
        f)
            force=yes
            ;;
        '?')
            usage
            exit 1
            ;;
    esac
done
shift "$((OPTIND-1))" # shift off the options and optional --

if [ $# -gt 0 ] ; then
    VERSION="$1"
fi

##### GO

# move to this scripts dir
scriptdir=$(dirname "$0")
cd "${scriptdir}"

# check if target exists
if [ -d "${DEST}" ]; then
    echo "Destination '${DEST}' exists" 1>&2
    if [ "${force}" = "yes" ]; then
        echo "  forced to update existing files" 1>&2
    else
        echo "  Please remove '$(pwd)/${DEST}' before running $0" 1>&2
        echo "  Or check the '-h' flag" 1>&2
        exit 1
    fi
fi

# clone source
echo "==== Downloading portmidi $VERSION"
if [ -d "${SRC}" ] ; then
    rm -rf "${SRC}"
fi
git clone https://github.com/PortMidi/portmidi.git "${SRC}"
if [ "$VERSION" != "" ] ; then
    cd "${SRC}"
    git checkout "${VERSION}"
    cd -
fi

# apply patches, note: this probably can't handle filenames with spaces
# temp disable exit on error since the exit value of patch --dry-run is used
echo "==== Applying any patches"
for p in $(find ./patches -type f -name "*.patch") ; do
    cd "${SRC}"
    set +e
    (patch -p1 -N --silent --dry-run --input "../${p}" > /dev/null 2>&1)
    set -e
    if [[ $? == 0 ]] ; then
        echo "==== Applying ${p}"
        patch -p1 < "../${p}"
    fi
    cd ../
done

echo "==== Copying"

# copy what we need, namely the main headers and relevant sources
copysrc pm_common
copysrc pm_linux
copysrc pm_mac
copysrc pm_win
copysrc porttime
cp -v "${SRC}"/license.txt "${DEST}/"

# make sure the source-code is not executable
#find "${DEST}" -type f -perm -u+x "(" -name "*.h" -or -name "*-c" ")" -exec chmod -x {} +

# cleanup
#rm -rf $SRC
