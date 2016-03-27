#!/bin/sh
#usage: ./send-mw.sh 0.38-0 or 0.38-0test4

if test x$1 == x
then
   echo usage: ./build 0.38-0 or 0.38-0test4
   exit 1
fi

PDDIR=`pwd`/..
MSWDIR=`pwd`

cd $PDDIR
find . -name ".[a-zA-Z]*" -o -name core -ok rm {} \;
rm -rf /tmp/pd /tmp/pd.zip
cd /tmp
tar xzf $MSWDIR/pdprototype.tgz

cd $PDDIR
cp src/*.{c,h} src/notes.txt src/makefile.mingw /tmp/pd/src
cp src/makefile.msvc /tmp/pd/src/makefile
textconvert u w < $MSWDIR/build-nt.bat > /tmp/pd/src/build.bat

cp tcl/*  /tmp/pd/tcl
cp -a portaudio  /tmp/pd/portaudio
cp -a portmidi /tmp/pd/portmidi
cp -a doc/ INSTALL.txt LICENSE.txt /tmp/pd/
cp -a extra/ /tmp/pd/extra

cd /tmp/pd
find . -name "*.pd_linux" -exec rm {} \;

for i in `find . -name "*.c" -o -name "*.h"  -o -name "*.cpp" \
    -o -name "make*" -o -name "*.txt" -o -name "*.pd" -o -name "*.htm" \
    -o -name "*.html" -o -name "*.tcl" \
    | grep -v asio | grep -v portmidi | grep -v portaudio \
    | grep -v include/X11` ; do
	textconvert u w < $i > /tmp/xxx
	mv /tmp/xxx $i
done

cd ..
rm -f pd.zip
zip -q -r pd.zip pd
ls -l /tmp/pd.zip

cd ~/bis/work/wine/script
if  ./build-msw.sh
    then echo -n ; else exit 1; fi

if  ./mingw-compile.sh
    then echo -n ; else exit 1; fi

# that has put a zip file in /tmp/pd-out.zip.  Now make the self-extracting
# installer using nsis.  Thanks to Roman Haefeli.

cd $MSWDIR
rm -f /tmp/pd-$1.windows-installer.exe /tmp/pd-$1.msw.zip

rm -rf /tmp/zz
mkdir /tmp/zz
(cd /tmp/zz; unzip ../pd-out.zip)
./build-nsi.sh  /tmp/zz/pd $1

mv /tmp/pd-out.zip /tmp/pd-$1.msw.zip
ls -l /tmp/pd-$1.windows-installer.exe /tmp/pd-$1.msw.zip

exit 0
