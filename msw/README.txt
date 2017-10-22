notes about compiling on Microsoft windows:

the release procedure for making vanilla releases is a mess for several
reasons.  Because of licensing restrictions, the ASIO support files are not
included in the Pd source tree.  For this and other reasons, Pd is compiled
in another directory in a filesystem maintained by wine (the linux windows
emulator).  Compilation is done both using Microsoft's compiler and
separately using mingw.  The release contains binaries built with mingw but
a .lib file created by the Microsoft compile chain.

The main script is "send-msw.sh" in this directory; this calls two other
scripts in the non-distributed directory ~/bis/work/wine/script/, copies
of which are included here (build-msw.sh and mingw-compile.sh).

There are also random support files here, some of which are probably no longer
used.
