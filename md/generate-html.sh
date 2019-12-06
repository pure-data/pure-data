#!/bin/sh

for i in *.md
    do pandoc -s -o ../doc/8.topics/`basename $i .md`.htm $i
done
