#! /bin/bash

rm ./GNUmakefile.am

cat fill.pre | while read; do echo "$REPLY" >> GNUmakefile.am; done

find . \
 -type f -not -path "./GNUmakefile*" \
 -type f -not -path "./Makefile" \
 | sort | awk '{print "    ", $1, "\\"}' >> GNUmakefile.am; echo '     $(empty)' >> GNUmakefile.am

truncate -s-1 GNUmakefile.am
