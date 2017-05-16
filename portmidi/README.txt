The cross-platform PortMidi library is included with the PureData distribution so that Pd can be built without any main external dependencies.

It is built as a libtool convenience library when using autotools to build Pd, aka:

    ./configure
	make

If you want to update to a newer version of PortMidi, simply update the subfolders in the `portmidi` folder with their contents from the PortMidi distribution. You *may* need to add or remove source file or header lines in the Makefile.am, if the source or include files have changed in PortMidi.

As a second option, you can build Pd using a system installed PortMidi via:

    ./configure --without-local-portmidi
	make

For more info on PortMidi, see http://portmedia.sourceforge.net/portmidi

