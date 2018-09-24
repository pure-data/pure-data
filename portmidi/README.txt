The cross-platform PortMidi library is included with the PureData distribution
so that Pd can be built without any main external dependencies.

It is built as a libtool convenience library when using autotools to build Pd:

    ./configure
    make

If you want to update to a newer version of PortAudio, simply use the
"update.sh" script:

    cd portmidi
    
    # update to latest PortMidi svn trunk
    ./update.sh

    # update to a specific trunk revision
    ./update.sh 239

You *may* need to add or remove source file or header lines in the Makefile.am,
if the source or include files have changed in PortMidi.

As a second option, you can build Pd using a system-installed PortMidi via:

    ./configure --without-local-portmidi
    make

For more info on PortMidi, see http://portmedia.sourceforge.net/portmidi

There may be custom patches to apply to the PortMidi sources in the "patches"
directory which are applied by the "update.sh" script automatically using:

    patch -p0 < ../patches/some_unreleased_fix.patch

To generate a patch file from a PortMidi svn checkout:

    svn diff > some_newfix.patch
