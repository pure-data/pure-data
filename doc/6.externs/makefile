current:
	@echo make pd_linux, pd_nt, or pd_darwin

clean:
	rm -f *.o
	rm -f *.pd_linux
	rm -f *.pd_darwin

# ----------------------- Windows-----------------------
# note: you will certainly have to edit the definition of VC to agree with
# whatever you've got installed on your machine

VC="D:\Program Files\Microsoft Visual Studio\Vc98"

pd_nt: obj1.dll obj2.dll obj3.dll obj4.dll obj5.dll dspobj~.dll

.SUFFIXES: .obj .dll

PDNTCFLAGS = /W3 /WX /DNT /DPD /nologo

PDNTINCLUDE = /I. /I\tcl\include /I..\..\src /I$(VC)\include

PDNTLDIR = $(VC)\lib
PDNTLIB = $(PDNTLDIR)\libc.lib \
	$(PDNTLDIR)\oldnames.lib \
	$(PDNTLDIR)\kernel32.lib \
	..\..\bin\pd.lib 

.c.dll:
	cl $(PDNTCFLAGS) $(PDNTINCLUDE) $(CFLAGS) /c $*.c
	link /dll /export:$*_setup $*.obj $(PDNTLIB)

# override explicitly for tilde objects like this:
dspobj~.dll: dspobj~.c; 
	cl $(PDNTCFLAGS) $(PDNTINCLUDE) $(CFLAGS) /c $*.c
	link /dll /export:dspobj_tilde_setup $*.obj $(PDNTLIB)

# ----------------------- LINUX i386 -----------------------

pd_linux: obj1.pd_linux obj2.pd_linux obj3.pd_linux obj4.pd_linux \
    obj5.pd_linux dspobj~.pd_linux

pd_linux32: obj1.l_i386 obj2.l_i386 obj3.l_i386 obj4.l_i386 \
    obj5.l_i386 dspobj~.l_i386

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

pd_darwin: obj1.pd_darwin obj2.pd_darwin \
     obj3.pd_darwin obj4.pd_darwin obj5.pd_darwin dspobj~.pd_darwin

.SUFFIXES: .pd_darwin

DARWINCFLAGS = -DPD -O2 -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch

.c.pd_darwin:
	cc $(DARWINCFLAGS) $(LINUXINCLUDE) $(CFLAGS) -o $*.o -c $*.c
	cc $(CFLAGS) -bundle -undefined suppress \
            -flat_namespace -o $*.pd_darwin $*.o 
	rm -f $*.o
