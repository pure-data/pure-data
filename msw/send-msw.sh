#!/bin/tcsh
set PDDIR=`pwd`/..
set MSWDIR=`pwd`

cd $PDDIR
find . -name ".[a-zA-Z]*" -o -name core -ok rm {} \;
rm -rf /tmp/pd /tmp/pd.zip
cd /tmp
tar xzf $MSWDIR/pdprototype.tgz
cd $PDDIR
cp src/*.{c,h} src/notes.txt /tmp/pd/src
textconvert u w < src/u_main.tk > /tmp/pd/src/u_main.tk
cp src/makefile.nt /tmp/pd/src/makefile
textconvert u w < $MSWDIR/build-nt.bat > /tmp/pd/src/build.bat
cp -a portaudio  /tmp/pd/portaudio
cp -a portmidi /tmp/pd/portmidi
cp -a doc/ INSTALL.txt LICENSE.txt /tmp/pd/
cp -a extra/ /tmp/pd/extra

cd /tmp/pd
find . -name "*.pd_linux" -exec rm {} \;

foreach i (`find . -name "*.c" -o -name "*.h"  -o -name "*.cpp" \
    -o -name "make*" -o -name "*.txt" -o -name "*.pd" -o -name "*.htm" \
    -o -name "*.html" | grep -v asio | grep -v portmidi | grep -v portaudio \
    | grep -v include/X11`)
	textconvert u w < $i > /tmp/xxx
	mv /tmp/xxx $i
end
foreach i (`find lib/asio -name "*.c" -o -name "*.h"  -o -name "*.cpp" -o -name "make*" -o -name "*.txt" -o -name "*.pd" -o -name "*.htm" -o -name "*.html"`)
        echo FOO----- $i
	textconvert w u < $i > /tmp/xxx
	textconvert u w < /tmp/xxx > $i
end


cd ..
rm -f pd.zip
zip -q -r pd.zip pd
ls -l /tmp/pd.zip

