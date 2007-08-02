
current: expr.pd_linux expr~.pd_linux fexpr~.pd_linux \
     ../expr.pd_linux ../expr~.pd_linux ../fexpr~.pd_linux

install: install_linux

clean: clean_linux

PDEXTERN=/usr/local/lib/pd/externs

# ----------------------- NT -----------------------

pd_nt: expr.dll

NTOBJ =  vexp.obj vexp_fun.obj vexp_if.obj

PDNTCFLAGS = /W3 /WX /DNT /DPD /nologo
VC="C:\Program Files\Microsoft Visual Studio\Vc98"

PDNTINCLUDE = /I. /I..\..\src /I$(VC)\include

PDNTLDIR = $(VC)\lib
PDNTLIB = $(PDNTLDIR)\libc.lib \
	$(PDNTLDIR)\oldnames.lib \
	$(PDNTLDIR)\kernel32.lib \
	..\..\bin\pd.lib 

.c.obj:
	cl $(PDNTCFLAGS) $(PDNTINCLUDE) /c $*.c

expr.dll: $(NTOBJ)
	link /dll /export:expr_setup /export:expr_tilde_setup \
	    /export:fexpr_tilde_setup $(NTOBJ) $(PDNTLIB)
	ren vexp.dll expr.dll
	copy expr.dll ..\expr.dll
	copy expr.dll ..\expr~.dll
	copy expr.dll ..\fexpr~.dll
	copy help-expr.pd ..\help-expr.pd

# ----------------------- IRIX 5.x -----------------------

pd_irix5: 

.SUFFIXES: .pd_irix5

SGICFLAGS5 = -o32 -DPD -DSGI -O2


SGIINCLUDE =  -I/usr/people/msp/pd/pd/src

.c.pd_irix5:
	$(CC) $(SGICFLAGS5) $(SGIINCLUDE) -o $*.o -c $*.c
	ld -elf -shared -rdata_shared -o $*.pd_irix5 $*.o
	rm $*.o

# ----------------------- IRIX 6.x -----------------------

pd_irix6: 

.SUFFIXES: .pd_irix6

SGICFLAGS6 = -DPD -DSGI -n32 \
	-OPT:roundoff=3 -OPT:IEEE_arithmetic=3 -OPT:cray_ivdep=true \
	-Ofast=ip32

SGICFLAGS5 = -DPD -O2 -DSGI

SGIINCLUDE =  -I/usr/people/msp/pd/pd/src

.c.pd_irix6:
	$(CC) $(SGICFLAGS6) $(SGIINCLUDE) -o $*.o -c $*.c
	ld -elf -shared -rdata_shared -o $*.pd_irix6 $*.o
	rm $*.o

# ----------------------- LINUX i386 -----------------------

LINUXOBJ =  vexp.pd_linux_o vexp_fun.pd_linux_o vexp_if.pd_linux_o
.SUFFIXES: .pd_linux_o

LINUXCFLAGS = -DPD -O2 -funroll-loops -fomit-frame-pointer -fPIC \
    -fno-strict-aliasing -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch 

LINUXINCLUDE =  -I../../src

.c.pd_linux_o:
	$(CC) -g $(LINUXCFLAGS) $(LINUXINCLUDE) -o $*.pd_linux_o -c $*.c

expr.pd_linux: $(LINUXOBJ)
	$(CC) -export_dynamic  -shared -o expr.pd_linux $(LINUXOBJ) -lc -lm
	strip --strip-unneeded expr.pd_linux

expr~.pd_linux: expr.pd_linux
	-ln -s expr.pd_linux expr~.pd_linux

fexpr~.pd_linux: expr.pd_linux
	-ln -s expr.pd_linux fexpr~.pd_linux

../expr.pd_linux: expr.pd_linux
	-ln -s expr~/expr.pd_linux ../expr.pd_linux

../expr~.pd_linux: expr.pd_linux
	-ln -s expr~/expr.pd_linux ../expr~.pd_linux

../fexpr~.pd_linux: expr.pd_linux
	-ln -s expr~/expr.pd_linux ../fexpr~.pd_linux

install_linux:
	install expr.pd_linux $(PDEXTERN)
	rm -f $(PDEXTERN)/expr~.pd_linux
	rm -f $(PDEXTERN)/fexpr~.pd_linux
	cd $(PDEXTERN); \
	-ln -s expr.pd_linux expr~.pd_linux
	-ln -s expr.pd_linux fexpr~.pd_linux


linux_clean:
	rm -f *.pd_linux_o *.o

# ----------------------- MAC OSX -----------------------

MACOSXOBJ3 =  vexp.d_ppc_o vexp_fun.d_ppc_o vexp_if.d_ppc_o
MACOSXOBJ =  vexp.d_fat_o vexp_fun.d_fat_o vexp_if.d_fat_o
.SUFFIXES: .d_ppc_o .d_fat_o

MACOSXCFLAGS = -DMACOSX -DPD -O2 -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch 

MACOSXINCLUDE =  -I../../src

d_ppc: expr.d_ppc expr~.d_ppc fexpr~.d_ppc
.c.d_ppc_o:
	$(CC) -g $(MACOSXCFLAGS) $(MACOSXINCLUDE) -o $*.d_ppc_o -c $*.c

expr.d_ppc: $(MACOSXOBJ3)
	$(CC) -bundle -undefined suppress  -flat_namespace \
		-o expr.d_ppc $(MACOSXOBJ3)  -lm
	rm -f ../expr.d_ppc
	-ln -s expr~/expr.d_ppc ..

expr~.d_ppc: expr.d_ppc
	-ln -s expr.d_ppc expr~.d_ppc
	rm -f ../expr~.d_ppc
	-ln -s expr~/expr~.d_ppc ..

fexpr~.d_ppc: expr.d_ppc
	-ln -s expr.d_ppc fexpr~.d_ppc
	rm -f ../fexpr~.d_ppc
	-ln -s expr~/fexpr~.d_ppc ..

install_d_fat:
	install expr.d_fat $(PDEXTERN)
	rm -f $(PDEXTERN)/expr~.d_fat
	rm -f $(PDEXTERN)/fexpr~.d_fat
	cd $(PDEXTERN); \
	-ln -s expr.d_fat expr~.d_fat; \
	-ln -s expr.d_fat fexpr~.d_fat

d_fat: expr.d_fat expr~.d_fat fexpr~.d_fat
.c.d_fat_o:
	$(CC) -g $(MACOSXCFLAGS) $(MACOSXINCLUDE)  -arch i386 -arch ppc \
	-o $*.d_fat_o -c $*.c

expr.d_fat: $(MACOSXOBJ)
	$(CC) -arch i386 -arch ppc -bundle -undefined suppress  -flat_namespace \
		-o expr.d_fat $(MACOSXOBJ)  -lm
	rm -f ../expr.d_fat
	-ln -s expr~/expr.d_fat ..

expr~.d_fat: expr.d_fat
	-ln -s expr.d_fat expr~.d_fat
	rm -f ../expr~.d_fat
	-ln -s expr~/expr~.d_fat ..

fexpr~.d_fat: expr.d_fat
	-ln -s expr.d_fat fexpr~.d_fat
	rm -f ../fexpr~.d_fat
	-ln -s expr~/fexpr~.d_fat ..

install_d_ppc:
	install expr.d_ppc $(PDEXTERN)
	rm -f $(PDEXTERN)/expr~.d_ppc
	rm -f $(PDEXTERN)/fexpr~.d_ppc
	cd $(PDEXTERN); \
	-ln -s expr.d_ppc expr~.d_ppc; \
	-ln -s expr.d_ppc fexpr~.d_ppc

clean_d_ppc:
	rm -f *.d_ppc_o *.o

