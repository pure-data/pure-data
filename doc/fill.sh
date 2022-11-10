#! /bin/bash

rm ./Makefile.am

cp ./fill.pre  ./Makefile.am

find . \
 -type f -not -path "./Makefile*" \
 -type f -not -path "./fill.pre" \
 -type f -not -path "./fill.sh" \
 | sort | awk '{print "    ", $1, "\\"}' >> ./Makefile.am; \
 echo '     $(empty)' >> ./Makefile.am

truncate -s-1 ./Makefile.am
