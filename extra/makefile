current: pd_linux

# ----------------------- NT -----------------------

pd_nt: $(NAME).dll

.SUFFIXES: .dll

PDNTCFLAGS = /W3 /WX /DNT /DPD /nologo
VC="C:\Program Files\Microsoft Visual Studio\Vc98"

PDNTINCLUDE = /I. /I..\..\src /I$(VC)\include

PDNTLDIR = $(VC)\lib
PDNTLIB = $(PDNTLDIR)\libc.lib \
	$(PDNTLDIR)\oldnames.lib \
	$(PDNTLDIR)\kernel32.lib \
	..\..\bin\pd.lib 

.c.dll:
	cl $(PDNTCFLAGS) $(PDNTINCLUDE) /c $*.c
	link /dll /export:$(CSYM)_setup $*.obj $(PDNTLIB)

# ----------------------- IRIX 5.x -----------------------

pd_irix5: $(NAME).pd_irix5

.SUFFIXES: .pd_irix5

SGICFLAGS5 = -o32 -DPD -DUNIX -DIRIX -O2

SGIINCLUDE =  -I../../src

.c.pd_irix5:
	$(CC) $(SGICFLAGS5) $(SGIINCLUDE) -o $*.o -c $*.c
	ld -elf -shared -rdata_shared -o $*.pd_irix5 $*.o
	rm $*.o

# ----------------------- IRIX 6.x -----------------------

pd_irix6: $(NAME).pd_irix6

.SUFFIXES: .pd_irix6

SGICFLAGS6 = -n32 -DPD -DUNIX -DIRIX -DN32 -woff 1080,1064,1185 \
	-OPT:roundoff=3 -OPT:IEEE_arithmetic=3 -OPT:cray_ivdep=true \
	-Ofast=ip32

.c.pd_irix6:
	$(CC) $(SGICFLAGS6) $(SGIINCLUDE) -o $*.o -c $*.c
	ld -n32 -IPA -shared -rdata_shared -o $*.pd_irix6 $*.o
	rm $*.o

# ----------------------- LINUX i386 -----------------------

pd_linux: $(NAME).pd_linux

.SUFFIXES: .pd_linux

LINUXCFLAGS = -DPD -O2 -funroll-loops -fomit-frame-pointer -fPIC \
    -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch $(CFLAGS)

LINUXINCLUDE =  -I../../src

.c.pd_linux:
	$(CC) $(LINUXCFLAGS) $(LINUXINCLUDE) -o $*.o -c $*.c
	$(CC) -export_dynamic -shared -o $*.pd_linux $*.o -lc -lm
	strip --strip-unneeded $*.pd_linux
	rm -f $*.o

# ----------------------- Mac OSX -----------------------

d_ppc: $(NAME).d_ppc
d_fat: $(NAME).d_fat

.SUFFIXES: .d_ppc .d_fat

DARWINCFLAGS = -DPD -O2 -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch

.c.d_ppc:
	$(CC) $(DARWINCFLAGS) $(LINUXINCLUDE) -o $*.o -c $*.c
	$(CC) -bundle -undefined suppress -flat_namespace -o $*.pd_darwin $*.o 
	rm -f $*.o

.c.d_fat:
	$(CC) -arch i386 -arch ppc $(DARWINCFLAGS) $(LINUXINCLUDE) -o $*.o -c $*.c
	$(CC) -arch i386 -arch ppc -bundle -undefined suppress -flat_namespace \
	    -o $*.d_fat $*.o 
	rm -f $*.o

# ----------------------------------------------------------

clean:
	rm -f *.o *.pd_* so_locations
