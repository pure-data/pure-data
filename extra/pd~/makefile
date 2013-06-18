NAME=pd~
CSYM=pd_tilde

include ../makefile.subdir

pd_linux: pdsched.pd_linux
pd_nt: pdsched.dll

d_fat: pdsched.d_fat
d_ppc: pdsched.d_ppc

pdsched.pd_linux: pdsched.c
	$(CC) $(LINUXCFLAGS) $(LINUXINCLUDE) -o $*.o -c $*.c
	$(CC) -shared -o $*.pd_linux $*.o -lc -lm
	rm -f $*.o

pdsched.dll: pdsched.c
	$(MSCC) $(PDNTCFLAGS) $(PDNTINCLUDE) /c $*.c
	$(MSLN) /nologo /dll /export:pd_extern_sched $*.obj $(PDNTLIB)
	rm -f $*.obj
