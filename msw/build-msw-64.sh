#!/bin/sh
#usage: ./build-msw-64.sh 0.38-0 or 0.38-0test4

if test x$1 = x
then
   echo usage: ./build-msw-64.sh 0.38-0 or 0.38-0test4
   exit 1
fi

v=$1-64bit.msw
tkversion=8.6.8


cd ~/tmp
git clone ~/pd pd-$v
cd pd-$v

#copy in the ASIO SDK and rename it ASIOSDK
(cd asio; unzip $HOME/work/asio/ASIOSDK2.3.zip;
    rm -rf __MACOSX/; mv ASIOSDK2.3 ASIOSDK)

#do an autotools build
./autogen.sh
./configure --host=x86_64-w64-mingw32 --with-wish=wish86.exe \
    CPPFLAGS='-DWISH=\"wish86.exe\"'

#for some reason, the generated libtool file has a reference to the 'msvcrt'
#lib which breaks compilation... so get rid of it.
sed -i '/-lmsvcrt/s/-lmsvcrt//g' libtool

make

#prepare to run Dan Wilcox's app-builcding script
cd msw
#copy in a pre-compiled version of TK (this was previously built using
#Dan's tk-building script here)
cp -a $HOME/bis/work/pd-versions/build-tk-on-win64/msw/tcltk-$tkversion .

#run the app building script itself
/home/msp/bis/work/pd-versions/build-tk-on-win64/msw/msw-app.sh \
   --builddir ..  --tk tcltk-$tkversion $v

#On my machine at least, the wrong winpthread lirary gets loaded (the
#32 bit one I think - manually copy the correct one in.
cp /usr/x86_64-w64-mingw32/sys-root/mingw/bin/libwinpthread-1.dll \
   pd-$v/bin/

#make the zip archive
zip -r /tmp/pd-$v.zip  pd-$v

#for convenience echo teh finished filename and a command to quicly test
#it in wine
echo /tmp/pd-$v.zip
echo wine `pwd`/pd-$v/bin/wish86.exe `pwd`/pd-$v/tcl/pd-gui.tcl
