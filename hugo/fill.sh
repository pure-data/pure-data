#! /bin/bash

rm ./GNUmakefile.am

while read line; do echo $line >> GNUmakefile.am ; done < fill.pre

find . -type f -not -path "./GNUmakefile*" | sort | awk '{print "    ", $1, "\\"}' >> GNUmakefile.am; echo '     $(empty)' >> GNUmakefile.am