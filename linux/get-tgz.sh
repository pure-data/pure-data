#!/bin/sh

#usage: ./get-tgz /tmp/x.tgz /tmp/sent-one.tgz
if test x$1 == x
then
   echo usage: ./get-tgz /tmp/x.tgz  /tmp/sent-one.tgz
   exit 1
fi

rm -rf /tmp/image
mkdir /tmp/image
cd /tmp/image
tar xzf $1

find . -type f \( -name "*.o" -o -name "*.d_fat" -o -name "*.d_ppc" \
    -o -perm -0100 \) \
    -exec echo rm {} \;

echo "****************** NEW FILES *******************"

find . -type f -newer $2

