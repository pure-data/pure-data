The cross-platform PortAudio library is included with the PureData distribution
so that Pd can be built without any main external dependencies.

It is built as a libtool convenience library when using autotools to build Pd:

    ./configure
    make

If you want to update to a newer version of PortAudio, simply use the
"update.sh" script:

    cd portaudio
    
    # update to latest PortAudio git master
    ./update.sh

    # update to a specific tag/branch/commit
    ./update.sh c0d239712d

You *may* need to add or remove source file or header lines in the Makefile.am,
if the source or include files have changed in PortAudio. To check the current
git revision, see "portaudio/src/common/pa_gitrevision.h"

As a second option, you can build Pd using a system-installed PortAudio via:

    ./configure --without-local-portaudio
    make

For more info on PortAudio, see http://portaudio.com
