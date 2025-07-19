#!/bin/sh
#usage: ./make-w32.sh 0.38-0 or 0.38-0test4
#does the build in /tmp/

# This file documents how Miller builds the 32-bit windows binaries for Pd.
# It has not been tested on machines other than his.  Most people wishing to
# compile Pd will want to use Dan Wilcox's scripts, tcltk-dir.sh and msw-app.sh
# directly, perhaps using this file as a guide.

if test x$1 == x
then
   echo usage: ./build-msw-32.sh 0.38-0 or 0.38-0test4
   exit 1
fi

pdversion=$1-i386
tkversion=8.5.19

cd /tmp/

rm -rf pd-$pdversion
git clone $HOME/pd pd-$pdversion
cd pd-$pdversion
(cd asio; unzip $HOME/work/asio/ASIOSDK2.3.zip;
    rm -rf _MACOSX/; mv ASIOSDK2.3 ASIOSDK)
./autogen.sh
if ./configure --host=i686-w64-mingw32 --with-wish=wish85.exe \
    CPPFLAGS='-DWISH=\"wish85.exe\"'
        then echo -n ; else exit 1; fi

if make
    then echo -n ; else exit 1; fi

cd msw
cp -a $HOME/bis/work/pd-versions/tcltk-$tkversion .
/home/msp/pd/msw/msw-app.sh \
   --builddir ..  --tk tcltk-$tkversion $pdversion

# 0.56 and onward - separately copy in thread lib which was thrown out of
# the prototype because it conflicted with something else
cp -p ~/pd/msw/libwinpthread-1.dll32 pd-$pdversion/bin/libwinpthread-1.dll

# also pdfontloader.dll for wish85 and 32bit (was removed from prototype)
cp -p ~/pd/msw/pdfontloader.dll-wish86-32bit pd-$pdversion/bin/pdfontloader.dll

# make zip archive
zip -r -q /tmp/pd-$pdversion.msw.zip  pd-$pdversion

# make installer
~/pd/msw/build-nsi.sh  `pwd`/pd-$pdversion $pdversion  wish85.exe

echo /tmp/pd-$pdversion.msw.zip
echo /tmp/pd-$pdversion.windows-installer.exe

echo wine `pwd`/pd-$pdversion/bin/wish85.exe `pwd`/pd-$pdversion/tcl/pd-gui.tcl
