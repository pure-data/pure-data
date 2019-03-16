#! /bin/bash
#
# this script automatically downloads and copies the portaudio library for Pd
#
# uses git master version by default, set checkout branch/tag/commit using
# first argument:
#
#    ./update.sh c0d239712d
#
# to see which git revision the portaudio code included with Pd is,
# see portaudio/src/common/pa_gitrevision.h
#
# Dan Wilcox danomatika.com 2018
#

# exit on error
set -e

##### VARIABLES

SRC=portaudio-git
DEST=portaudio

# commandline arg: version can be a tag, commit, etc
# uses git master by default
VERSION=""

##### FUNCTIONS

# copy .h & .c files from $SRC to $DEST, ignore missing file errors
function copysrc {
    mkdir -p $DEST/$1
    cp -v $SRC/$1/*.h $DEST/$1 2>/dev/null || :
    cp -v $SRC/$1/*.c $DEST/$1 2>/dev/null || :
}

##### GO

if [ $# -gt 0 ] ; then
    VERSION="$1"
fi

# move to this scripts dir
cd $(dirname $0)

# clone source
echo "==== Downloading portaudio $VERSION"
if [ -d $SRC ] ; then
    rm -rf $SRC
fi
git clone https://git.assembla.com/portaudio.git $SRC
if [[ "$VERSION" != "" ]] ; then
    cd $SRC && git checkout $VERSION && cd -
fi

# set git revision
if [ -f $SRC/update_gitrevision.sh ] ; then
	echo "==== Set git revision"
	cd $SRC && ./update_gitrevision.sh && cd -
fi

echo "==== Copying"

# remove stuff we don't need
rm $SRC/include/pa_jack.h
rm $SRC/include/pa_win_ds.h
rm $SRC/include/pa_win_wasapi.h
rm $SRC/include/pa_win_wdmks.h
rm $SRC/src/hostapi/coreaudio/pa_mac_core_old.c
rm $SRC/src/os/win/pa_win_wdmks_utils.*
rm $SRC/src/os/win/pa_x86_plain_converters.*

# copy what we need, namely the main headers and relevant sources
copysrc include
copysrc src/common
copysrc src/hostapi/alsa
copysrc src/hostapi/asio
copysrc src/hostapi/coreaudio
copysrc src/hostapi/wmme
copysrc src/os/unix
copysrc src/os/win
cp -v $SRC/LICENSE.txt $DEST/

# cleanup
rm -rf $SRC
