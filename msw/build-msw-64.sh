#!/bin/sh
#usage: ./build-msw-64.sh 0.38-0 or 0.38-0test4

# This file documents how Miller builds the 64-bit windows binaries for Pd.
# It has not been tested on machines other than his.  Most people wishing to
# compile Pd will want to use Dan Wilcox's scripts, tcltk-dir.sh and msw-app.sh
# directly, perhaps using this file as a guide.

if test x$1 = x
then
   echo usage: ./build-msw-64.sh 0.38-0 or 0.38-0test4
   exit 1
fi

pdversion=$1
installerversion=$1.windows-installer.exe
tkversion=8.6.8


cd /tmp
rm -rf pd-$pdversion
git clone ~/pd pd-$pdversion
cd pd-$pdversion

#copy in the ASIO SDK and rename it ASIOSDK
(cd asio; unzip $HOME/work/asio/ASIOSDK2.3.zip;
    rm -rf __MACOSX/; mv ASIOSDK2.3 ASIOSDK)

#do an autotools build
./autogen.sh
./configure --host=x86_64-w64-mingw32 --with-wish=wish86.exe \
    CPPFLAGS='-DWISH=\"wish86.exe\" -DWINVER=0x0600 -D_WIN32_WINNT=0x0600'

# for some reason, the generated libtool file has a reference to the 'msvcrt'
# lib which breaks compilation... so get rid of it.  This might no longer be
# necessary.
sed -i '/-lmsvcrt/s/-lmsvcrt//g' libtool

make

# prepare to run Dan Wilcox's app-builcding script
cd msw
# copy in a pre-compiled version of TK (this was previously built using
# Dan's tk-building script here)
cp -a $HOME/bis/work/pd-versions/build-tk-on-win64/msw/tcltk-$tkversion .

# run the app building script itself
./msw-app.sh  --builddir ..  --tk tcltk-$tkversion $pdversion

# On my machine at least, the wrong winpthread library gets loaded (the
# 32 bit one I think) - manually copy the correct one in.  This too is
# probably no longer necessary.

cp /usr/x86_64-w64-mingw32/sys-root/mingw/bin/libwinpthread-1.dll \
   pd-$pdversion/bin/
   
# use pdfontloader for wish86 and 64bit Windows
rm -f pd-$pdversion/bin/pdfontloader.dll
unzip pdfontloader-64bit-for-wish86.zip -d pd-$pdversion/bin/


# make the zip archive
zip -r /tmp/pd-$pdversion.msw.zip  pd-$pdversion

# make an installer
~/pd/msw/build-nsi.sh  `pwd`/pd-$pdversion $pdversion wish86.exe

# for convenience echo the finished filename and a command to quickly test
# it in wine
echo /tmp/pd-$pdversion.msw.zip
echo /tmp/pd-$installerversion
echo wine `pwd`/pd-$pdversion/bin/wish86.exe `pwd`/pd-$pdversion/tcl/pd-gui.tcl
