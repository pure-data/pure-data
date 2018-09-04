#!/bin/sh
if [ ! -d ../drive_c/users/msp ]
  then echo ../drive_c/users/msp: no such directory; exit 1
  else echo -n
fi

cd ../drive_c/users/msp/
pwd
rm -rf pd
unzip /tmp/pd.zip
cd pd/src
if make \
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
exit 0
