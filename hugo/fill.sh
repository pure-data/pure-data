#! /bin/bash

rm ./GNUmakefile.am

cp ./fill.pre  ./GNUmakefile.am

find . \
 -type f -not -path "./GNUmakefile*" \
 -type f -not -path "./Makefile*" \
 -type f -not -path "./automakehugo.yaml" \
 -type f -not -path "./hugo.stamp" \
 -type f -not -path "./.hugo_build.lock" \
 -type f -not -path "./.gitignore" \
 -type f -not -path "./fill.pre" \
 -type f -not -path "./fill.sh" \
 | sort | awk '{print "    ", $1, "\\"}' >> ./GNUmakefile.am; echo '     $(empty)' >> ./GNUmakefile.am

truncate -s-1 ./GNUmakefile.am
