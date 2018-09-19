#!/bin/sh
rm -rf ../mingw-build
mkdir ../mingw-build
cd ../mingw-build

unzip /tmp/pd.zip
cd pd/lib
tar xzf /home/msp/work/asio/asio2.3-sdk-src.tgz
mv ASIOSDK2.3 ASIOSDK
cd ../src
rm semaphore.h pthread.h sched.h pthreadVC.lib pthreadVC.dll
cp ~/pd/src/pd.rc ~/pd/tcl/pd.ico .
cp /usr/i686-w64-mingw32/sys-root/mingw/bin/pthreadGC2.dll ../bin

if make -f makefile.mingw CC=i686-w64-mingw32-gcc CXX=i686-w64-mingw32-c++ \
    WINDRES=i686-w64-mingw32-windres
        then echo -n ; else exit 1; fi

if cp ../../../drive_c/users/msp/pd/bin/pd.lib ../bin/
        then echo -n ; else exit 1; fi

cd ../extra
for i in  bonk~ choice fiddle~ loop~ lrshift~ pique sigmund~ stdout pd~\
   bob~;
do
  echo extern ----------------- $i -----------------
  cd $i
  if make \
    MSCC="wine 'c:\Program Files\Microsoft Visual Studio 9.0\VC\bin\cl'" \
    MSLN="wine 'c:\Program Files\Microsoft Visual Studio 9.0\VC\bin\link'" \
    COPY=echo pd_nt
        then echo -n ; else exit 1; fi
   cd ..
done

cd ../..
mv pd/lib/ASIOSDK /tmp/ASIOSDK-save
rm -f /tmp/pd-out.zip
zip -r /tmp/pd-out.zip pd
mv /tmp/ASIOSDK-save pd/lib/ASIOSDK
echo ++++++++++++++++ /tmp/pd-out.zip ++++++++++++++++++
exit 0
