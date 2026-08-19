current:
	@echo make pd_linux, pd_mingw, or pd_darwin

clean:
	rm -f *.o
	rm -f *.dll
	rm -f *.pd_linux
	rm -f *.pd_darwin

# ----------------------- Windows (MinGW) -----------------------

pd_mingw: obj1.dll obj2.dll obj3.dll obj4.dll obj5.dll taskobj.dll dspobj~.dll

.SUFFIXES: .o .dll

MINGWFLAGS = -DPD -g -O2 -funroll-loops -fomit-frame-pointer -mms-bitfields \
    -Wall -W -Wshadow -Wstrict-prototypes -Wno-unused -Wno-parentheses -Wno-switch

MINGWINCLUDE =  -I../../src

.c.dll:
	$(CC) $(MINGWFLAGS) $(MINGWINCLUDE) $(CFLAGS) -o $*.o -c $*.c
	$(CC) -shared -Wl,--enable-auto-import -o $*.dll $*.o -L../../src -lpd
	rm $*.o

# ----------------------- LINUX i386 -----------------------

pd_linux: obj1.pd_linux obj2.pd_linux obj3.pd_linux obj4.pd_linux \
    obj5.pd_linux dspobj~.pd_linux taskobj.pd_linux

pd_linux32: obj1.l_i386 obj2.l_i386 obj3.l_i386 obj4.l_i386 \
    obj5.l_i386 taskobj.l_i386 dspobj~.l_i386

.SUFFIXES: .l_i386 .pd_linux

LINUXCFLAGS = -DPD -g -O2 -fPIC -funroll-loops -fomit-frame-pointer \
    -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch

LINUXINCLUDE =  -I../../src

.c.pd_linux:
	$(CC) $(LINUXCFLAGS) $(LINUXINCLUDE) $(CFLAGS) -o $*.o -c $*.c
	$(LD) -shared -o $*.pd_linux $*.o -lc -lm
	strip --strip-unneeded $*.pd_linux
	rm $*.o

# ----------------------- macOS -----------------------
# By default, builds on macOS will use the current system architecture. For
# non-native or multi-arch builds, pass the archs via CFLAGS:
#     make pd_darwin CFLAGS="-arch arm64 -arch x86_64"

pd_darwin: obj1.pd_darwin obj2.pd_darwin obj3.pd_darwin \
	obj4.pd_darwin obj5.pd_darwin taskobj.pd_darwin dspobj~.pd_darwin

.SUFFIXES: .pd_darwin

DARWINCFLAGS = -DPD -O2 -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch

.c.pd_darwin:
	cc $(DARWINCFLAGS) $(LINUXINCLUDE) $(CFLAGS) -o $*.o -c $*.c
	cc $(CFLAGS) -bundle -undefined suppress \
            -flat_namespace -o $*.pd_darwin $*.o 
	rm -f $*.o
