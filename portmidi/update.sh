#! /bin/bash
#
# this script automatically downloads and copies the portmidi library for Pd
#
# uses svn trunk version by default, set checkout revision # using
# first argument:
#
#    ./update.sh 239
#
# Dan Wilcox danomatika.com 2018
#

# exit on error
set -e

##### VARIABLES

SRC=portmidi-svn
DEST=portmidi

# commandline arg: version is svn revision #
VERSION=""

##### FUNCTIONS

# copy .h & .c files from $SRC to $DEST, ignore missing file errors
function copysrc {
    mkdir -p $DEST/$1
    cp -v $SRC/$1/*.h $DEST/$1 2>/dev/null || :
    cp -v $SRC/$1/*.c $DEST/$1 2>/dev/null || :
}

##### GO

# append revision number to checkout rule ala @###
if [ $# -gt 0 ] ; then
    VERSION="@$1"
fi

# move to this scripts dir
cd $(dirname $0)

# checkout source
echo "==== Downloading portmidi $VERSION"
if [ -d $SRC ] ; then
    rm -rf $SRC
fi
svn checkout https://svn.code.sf.net/p/portmedia/code/portmidi/trunk${VERSION} $SRC

# apply patches, note: this probably can't handle filenames with spaces
# temp disable exit on error since the exit value of patch --dry-run is used
echo "==== Applying any patches"
for p in $(find ./patches -type f -name "*.patch") ; do
    cd $SRC
    set +e
    (patch -p0 -N --silent --dry-run --input "../${p}" > /dev/null 2>&1)
    set -e
    if [[ $? == 0 ]] ; then
        patch -p0 < "../${p}"
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
cp -v $SRC/license.txt $DEST/

# cleanup
rm -rf $SRC
