The cross-platform PortAudio library is included with the PureData distribution so that Pd can be built without any main external dependencies.

It is built as a libtool convenience library when using autotools to build Pd, aka:

    ./configure
	make

If you want to update to a newer version of PortAudio, simply update the subfolders in the `portaudio` folder with their contents from the PortAudio distribution. You *may* need to add or remove source file or header lines in the Makefile.am, if the source or include files have changed in PortAudio.

As a second option, you can build Pd using a system installed PortAudio via:

    ./configure --without-local-portaudio
	make

For more info on PortAudio, see http://portaudio.com

