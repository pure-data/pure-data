#!/bin/sh

# this file is included for reference only.  It shows how it might be possible
# to compile Pd in Microsift Visual Studio Version 9.0.  This is useful
# as a test of the portability of Pd's code, although the resulting binaries
# are never used.  Run this in .../pd directory

BUILDDIR=$HOME/bis/work/wine/drive_c/users/msp
PDDIR=`pwd`

if [ ! -d $BUILDDIR ]
  then echo $BUILDDIR: no such directory; exit 1
  else echo -n
fi

git archive --prefix=pd/ HEAD | gzip > /tmp/pd-src.tgz

cd $BUILDDIR
pwd
rm -rf pd
tar xzf $PDDIR/msw/pdprototype.tgz
tar xzf /tmp/pd-src.tgz
cd pd/src
if make -f makefile.msvc \
    MSCC="wine 'c:\Program Files\Microsoft Visual Studio 9.0\VC\bin\cl'" \
    MSLN="wine 'c:\Program Files\Microsoft Visual Studio 9.0\VC\bin\link'" \
    COPY=cp DELETE=rm \
    SRCASIO= ASIOLIB=/NODEFAULTLIB:ole32 PAAPI=-DPA_USE_WMME PAASIO=
 then echo -n ; else exit 1; fi
cd ../extra
for i in  bonk~ choice fiddle~ loop~ lrshift~ pique sigmund~ stdout pd~;
do
  echo extern ----------------- $i -----------------
  cd $i
  if make \
    MSCC="wine 'c:\Program Files\Microsoft Visual Studio 9.0\VC\bin\cl'" \
    MSLN="wine 'c:\Program Files\Microsoft Visual Studio 9.0\VC\bin\link'" \
    COPY=cp DELETE=rm pd_nt
        then echo -n ; else exit 1; fi
   cd ..
done

echo to run: wine $BUILDDIR/pd/bin/pd.exe

exit 0
