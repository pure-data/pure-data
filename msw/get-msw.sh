#!/bin/tcsh

rm -rf /tmp/image
mkdir /tmp/image
cd /tmp/image
unzip -q /tmp/pdout.zip
find . \( -name "*.lib" -o -name "*.exe" -o -name "*.dll" -o -name "*.obj" \
    -o -name "*.exp"  \) \
    -exec rm {} \;

echo "****************** NEW FILES *******************"

find . -type f -newer /tmp/pd.zip

foreach i (`find . -name "*.c" -o -name "*.h"  -o -name "*.cpp" \
	-o -name "make*" -o -name "*.txt" -o -name "*.pd" -o -name "*.htm" \
	-o -name "*.html" -o -name "*.tk"`)
	textconvert w u < $i > /tmp/xxx
	mv /tmp/xxx $i
end

