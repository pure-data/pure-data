This directory contains the support files for compiling Pd on Macintosh
systems, as it is built for the 'vanilla' releases on msp.ucsd.edu.  To
build Pd, copy this ('mac') directory somewhere like ~/mac.  Also copy
a source tarball there, such as pd-0.45-4.src.tar.gz .  then cd to ~/mac
and type,

./build-macosx 0.45-3

and if all goes well, you'll soon see a new app appear named Pd-0.45-4.app

Note: The "wish-shell.tgz" is an archive of this app I found on my mac:
System/Library/Frameworks/Tk.framework/Versions/8.4/Resources/Wish Shell.app
A smarter version of the scripts ought to be able to find that file
automatically on your system so I wouldn't have to include it here.
