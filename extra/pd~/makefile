NAME=pd~
CSYM=pd_tilde

include ../makefile

pd_linux: pdsched.pd_linux

d_fat: pdsched.d_fat
d_ppc: pdsched.d_ppc

pdsched.pd_linux: pdsched.c
	$(CC) $(LINUXCFLAGS) $(LINUXINCLUDE) -o $*.o -c $*.c
	$(CC) -shared -o $*.pd_linux $*.o -lc -lm
	strip --strip-unneeded $*.pd_linux
	rm -f $*.o
