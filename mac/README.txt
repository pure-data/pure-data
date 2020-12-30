# Pure Data macOS resources

This directory contains support files for building a Pure Data macOS application
bundle and supplementary build scripts for compiling Pd on Macintosh systems, as
it is built for the 'vanilla' releases on msp.ucsd.edu.

## Pd macOS app

In a nutshell, a monolithic macOS "application" is simply a directory structure
treated as a single object by the OS. Inside this bundle are the compiled
binaries, resource files, and contextual information. You can look inside any
application by either navigating inside it from the commandline or by
right-clicking on it in Finder and choosing "Show Package Contents."

The basic layout is:

Pd-0.47-1.app/Contents
  Info.plist  <- contextual info: version string, get info string, etc
  /Frameworks <- embedded Tcl/Tk frameworks (optional)
  /MacOS/Pd   <- renamed Wish bundle launcher
  /Resources
    /bin      <- pd binaries
    /doc      <- built in docs & help files
    /extra    <- core externals
    /font     <- included fonts
    /po       <- text translation files
    /src      <- Pd source header files
    /tcl      <- Pd GUI scripts

The Pure Data GUI utilizes the Tk windowing shell aka "Wish" at runtime.
Creating a Pure Data .app involves using a precompiled Wish.app as a wrapper
by copying the Pd binaries and resources inside of it.

## App Bundle Helpers

* osx-app.sh: creates a Pd .app bundle for macOS using a Tk Wish.app wrapper
* tcltk-wish.sh: downloads and builds a Tcl/Tk Wish.app for macOS

These scripts complement the autotools build system described in INSTALL.txt and
are meant to be run after Pd is configured and built. The following usage, for
example, downloads and builds a 32 bit Tk 8.6.6 Wish.app which is used to create
a macOS Pd-0.47-1.app:

    mac/tcltk-wish.sh --arch i386 8.6.6
    mac/osx-app.sh --wish Wish-8.6.6.app 0.47-1

Both osx-app.sh & tcltck-wish.sh have extensive help output using the --help
commandline option:

    mac/osx-app.sh --help
    mac/tcltk-wish.sh --help

The osx-app.sh script automates building the Pd .app bundle and is used in the
"make app" makefile target. This default action can be invoked manually after
Pd is built:

    mac/osx-app.sh 0.47-1

This builds a "Pd-0.47-1.app" using the included Wish. If you omit the version
argument, a "Pd.app" is built. The version argument is only used as a suffix to
the file name and contextual version info is pulled from configure script
output.

A pre-built universal (32/64 bit) Tk 8.6.10 Wish with patches applied is
included with the Pd source distribution and works across the majority of macOS
versions up to 10.15. This is the default Wish.app when using osx-app.sh. If you
want to use a different Wish.app (a newer version, a custom build, a system
version), you can specify the donor via commandline options, for example:

    # build Pd-0.47-1.app using Tk 8.6 installed to the system
    mac/osx-app.sh --system-tk 8.6 0.47-1

If you want Pd to use a newer version of Tcl/Tk, but do not want to install to
it to your system, you can build Tcl/Tk as embedded frameworks inside of the Pd
.app bundle. This has the advantage of portability to other systems.

The tcltk-wish.sh script automates building a Wish.app with embedded Tcl/Tk,
either from the release distributions or from a git clone:

    # build Wish-8.6.6.app with embedded Tcl/Tk 8.6.6
    mac/tcltk-wish.sh 8.6.6

    # build Wish-master-git.app from the latest Tcl/Tk master branch from git
    mac/tcltk-wish.sh --git master-git

You can also specify which architectures to build (32 bit, 64 bit, or both):

    # build 32 bit Wish-8.6.6.app with embedded Tcl/Tk 8.6.6
    mac/tcltk-wish.sh --arch i386 8.6.6

    # build universal (32 & 64 bit)
    mac/tcltk-wish.sh --universal 8.6.6

Once your custom Wish.app is built, you can use it as the .app source for
osx-app.sh with the -w/--wish option:

    # build Pd with a custom Tcl/Tk 8.6.6 Wish
    mac/osx-app.sh -w Wish-8.6.6.app

Downloading and building Tcl/Tk takes some time. If you are doing lots of builds
of Pd and/or are experimenting with different versions of Tcl/Tk, building the
embedded Wish.apps you need with tcltk-wish.sh can save you some time as they
can be reused when (re)making the Pd .app bundle.

Usually, it's best to use stable releases of Tcl/Tk. However, there are times
when building from the current development version is useful. For instance, 
if there is a bug in the Tcl/Tk sources and the generated Wish.app crashes on
your system, you can then see if there is a fix for this in the Tcl/Tk
development version on GitHub. If so, then you can test by using the
tcltk-wish.sh --git commandline option. Oftentimes, these kinds of issues will
appear with a newer version of macOS before they have been fixed by the open
source community.

Additionally, Pd uses an older version of Tcl/Tk for backwards compatibility on
macOS. As such, small bugfixes from newer versions may need to be backported for
the Pd GUI. Currently, this is handled in the tcltk-wish.sh script by applying
custom patches to either the Tcl and/or Tk source trees. To skip applying
patches, use the tcltk-wish.sh --no-patches commandline option. See
mac/patches/README.txt for more info.

## Supplementary Build Scripts

* build-macosx: builds a 32 bit Pd .app bundle using src/makefile.mac
* build-mac64: builds a 64 bit Pd .app bundle using src/makefile.mac

These scripts automate building Pd with the fallback makefiles in the src
directory.

To build a 32 bit Pd, copy this "mac" directory somewhere like ~/mac. Also copy
a source tarball there, such as pd-0.47-1.src.tar.gz. Then cd to ~/mac and type:

    ./build-macosx 0.47-1

If all goes well, you'll soon see a new app appear named Pd-0.47-1.app.

If you want to build a 64 bit Pd, perform the same steps and use the build-mac64
script:

    ./build-mac64 0.47-1

Note: The "wish-shell.tgz" is an archive of this app I found on my mac:
/System/Library/Frameworks/Tk.framework/Versions/8.4/Resources/Wish Shell.app

A smarter version of the scripts ought to be able to find that file
automatically on your system so I wouldn't have to include it here.

## Preferences

The Pure Data preferences are saved in the macOS "defaults" preference system
using the following domains:

* org.puredata.pd: core settings (audio devices, search paths, etc)
* org.puredata.pd.pd-gui: GUI settings (recent files, last opened location, etc)

The files themselves live in your user home folder and use the .plist extension:

    ~/Library/Preferences/org.puredata.pd.plist
    ~/Library/Preferences/org.puredata.pd.pd-gui.plist

These files use the Apple Property List XML format and shouldn't be edited
directly. You can look inside, edit, and/or delete these using the "defaults"
commandline utility in Terminal:

    # print the contents of the core settings
    defaults read org.puredata.pd

    # delete the current GUI settings
    defaults delete org.puredata.pd.pd-gui

    # set the startup flag in the core settings
    defaults write org.puredata.pd -array-add flags '-lib Gem'

Some important per-application settings required by the GUI include:

* NSRecentDocuments: string array, list of recently opened files
* NSQuitAlwaysKeepsWindows: false, disables default 10.7+ window state saving
* ApplePressAndHoldEnabled: false, disables character compose popup,
                                   enables key repeat for all keys
* NSRequiresAquaSystemAppearance: true, disables dark mode for Pd GUI on 10.14+

These are set in `tcl/pd_guiprefs.tcl`.

## Code Signing

As of Pd 0.51, the mac/osx-app.sh script performs "ad-hoc code signing" in order
to set entitlements to open un-validated dynamic libraries on macOS 10.15+. This
is required due to the new security settings. Note: ad-hoc signing doesn't
actually sign the .app bundle with an account certificate, so the unidentified
developer warning is still shown when the downloaded .app is run for the first
time.

## Privacy Permissions

macOS 10.14 introduced system privacy permissions for actions applications can
undertake on a user account, such as accessing files or reading microphone or
camera input. When an application is started for the first time and tries to
access something that is covered by the privacy settings, a permissions prompt
is displayed by the system requesting access. The action is then allowed or
denied and this setting is saved and applied when the application is run again
in the future.

As of macOS 10.15, running Pd will request access for the following:

* Files and Folders: Documents
* Files and Folders: Desktop
* Microphone

Additionally, using an external such as Gem for camera input will request
access to the Camera.

The current permissions can be changed in Privacy panel in System Preferences.
They can also be reset on the commandline using the "tccutil" command and the
Pd .app bundle id:

    # reset Pd's Microphone privacy setting
    tccutil reset Microphone org.puredata.pd.pd-gui

    # reset all of Pd's privacy settings
    tccutil reset All org.puredata.pd.pd-gui

## Font Issues with macOS 10.15+

macOS 10.15 furthered changes to font rendering begin with 10.14 with the weird
result that Pd's default font, DejaVu Sans Mono, renders thin and closer
together than system fonts. This results in objects on the patch canvas that are 
longer their inner text and text selection positioning is off.

To remedy this for now, Pd 0.51-3 changed Pd's default font for macOS to Menlo
which is included with the system since 10.6. Menlo is based on Bitstream Vera
Mono and DejaVu Sans Mono, so there should be no issues with patch sizing or
positioning.

## Dark Mode

Pd currently disables Dark Mode support by setting the
NSRequiresAquaSystemAppearance key to true in both the app bundle's Info.plist
and the GUI defaults preference file. This restruction may be removed in the
future once Dark Mode is handled in the GUI.
