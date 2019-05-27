# PortMidi

The cross-platform PortMidi library is included with the PureData distribution
so that Pd can be built without any main external dependencies.

It is built as a libtool convenience library when using autotools to build Pd:

    ./configure
    make

For more info on PortMidi, see http://portmedia.sourceforge.net/portmidi

## Updating

If you want to update to a newer version of PortMidi, simply use the
"update.sh" script:

    cd portmidi
    
    # update to latest PortMidi svn trunk
    ./update.sh

    # update to a specific trunk revision
    ./update.sh 239

You *may* need to add or remove source file or header lines in the Makefile.am,
if the source or include files have changed in PortMidi.

## System-installed PortMidi

As a second option, you can build Pd using a system-installed PortMidi via:

    ./configure --without-local-portmidi
    make

## Patches

There may be custom patches to apply to the PortMidi sources in the "patches"
directory which are applied by the "update.sh" script automatically using:

    patch -p0 < ../patches/some_unreleased_fix.patch

To generate a patch file from a PortMidi svn checkout:

    svn diff > some_newfix.patch

## macOS LIMIT_RATE

Newer versions of Portmidi include automatic message rate limiting for macOS
which has a rather low buffer size when using the internal IAC virtual device
for message routing and could lead to messages being dropped. As this limit
may affect timing and cause issues when sending *large* amounts of MIDI data,
it is disabled when building the Portmidi sources included with Pd.

If you want to re-enable the rate limit, set the LIMIT_RATE define before
building:

    ./configure CFLAGS="-DLIMIT_RATE=1"
