# Pure Data Windows resources

This directory contains support files for building a Pure Data Windows
application, as it is built for the 'vanilla' releases on msp.ucsd.edu.

## Notes about compiling on Microsoft Windows

As of Pd version 0.50, all releases are compiled using Dan Wilcox's scripts,
msw-app.sh, and optionally tcltk-dir.sh (which builds a version of TCL/TK in
case you don't already have one).

The Pd sources aren't completely self-contained: Because of licensing
restrictions, the ASIO support files are not included in the Pd source tree.
The msw-app.sh script presumes you've downloaded the ASIO SDK and can point
msw-app.sh to it.

It's also possible to compile Pd using Visual C.  This is not managed
automatically but you can look in "how-to-use-msvc.sh" to see how it can be
done.  This compiler gives different warnings which are sometimes useful, and
refuses to compile code that has variable declarations in the middle of a
block.  It's a good idea to test pull requests against MSVC if you can.  The
file "pdprototype.zip" contains all the garbage that Pd needs in addition to its
own files, including tcl/tk.  MSVC compilation works in 32 bits only.

The scripts build-msw-32.sh and build-msw-64.sh are the ones used by Miller to
make Pd releases.  These files work on Linux only and will not work out of
the box unless your file tree resembles Miller's in some ways (pd source is in
~/pd for instance) but you can presumably make your own version if you need to.

But to first get things working, it's best to use msw-app.sh and tcltk-dir.sh
directly.

## Pd Application Directory

Pd for Windows is essentially a stand-alone application directory which contains
the compiled binaries, resource files, and contextual information.

The basic layout is:

pd
  /bin      <- pd binaries
  /doc      <- built in docs & help files
  /extra    <- core externals
  /font     <- included fonts
  /lib      <- embedded Tcl/Tk frameworks
  /po       <- text translation files
  /src      <- Pd source header files
  /tcl      <- Pd GUI scripts

The Pure Data GUI utilizes the Tk windowing shell aka "wish##.exe" at runtime
which is included with Pd in the /bin directory. A Pure Data app directory
includes both the Pd binaries and resources as well as a precompiled Tk.

## App Bundle Helpers

* msw-app.sh: creates a Pd app directory for Windows using precompiled Tcl/Tk
* tcltk-dir.sh: downloads and builds Tcl/Tk for Windows

These scripts complement the autotools build system described in INSTALL.txt and
are meant to be run after Pd is configured and built. The following usage, for
example, downloads and builds 32 bit Tk 8.6.8 which is used to create
a Windows pd-0.48-1 directory:

    msw/tcltk-dir.sh 8.6.8
    msw/msw-app.sh --tk tcltk-8.6.8 0.48-1

Both msw-app.sh & tcltck-dir.sh have extensive help output using the --help
commandline option:

    msw/msw-app.sh --help
    msw/tcltk-dir.sh --help

The msw-app.sh script automates building the Pd app directory and is used in the
"make app" makefile target. This default action can be invoked manually after
Pd is built:

    msw/msw-app.sh 0.47-1

This builds a "pd-0.47-1" directory using the default copy of Tk. If you omit
the version argument, a "pd" directory is built. The version argument is only
used as a suffix to the directory name.

The "msw/pdprototype.tgz" archive contains the basic requirements for running Pd
on Windows: a precompiled copy of Tcl/Tk and various .dll library files. This is
the default Tcl/Tk when using msw-app.

If you want to use a newer Tcl/Tk version or a custom build, you can specify the
version or directory via commandline options, for example:

    # create pd-0.48-1 directory, download and build Tcl/Tk 8.5.19
    msw/msw-app.sh --tk 8.5.19 0.48-1

    # create pd-0.48-1 directory, use Tcl/Tk 8.5.19 built with tcltk-dir.sh
    msw/msw-app.sh --tk tcltk-8.5.19 0.48-1

Note: Depending on which version of Tcl/Tk you want to use, you may need to set
the Tk Wish command when configuring Pd. To build Pd to use Tk 8.6:

    ./configure --with-wish=wish86.exe
    make

The tcltk-dir.sh script automates building Tcl/Tk for Windows, either from the
release distributions or from a git clone:

    # build tcltk-8.5.19 directory with Tcl/Tk 8.5.19
    msw/tcltk-dir.sh 8.5.19

    # build tcltk-master-git with the latest master branch from git
    msw/tcltk-dir.sh --git master-git

Once your custom Tcl/Tk is built, you can use it as the Tk directory source for
msw-app.sh with the -t/--tk option:

    # build Pd with a custom Tcl/Tk 8.6.8 directory
    msw/msw-app.sh -t tcltk-8.6.8

Downloading and building Tcl/Tk takes some time. If you are doing lots of builds
of Pd and/or are experimenting with different versions of Tcl/Tk, building the
tcltk directories you need with tcltk-dir.sh can save you some time as they
can be reused when (re)making the Pd app directory.

Usually, it's best to use stable releases of Tcl/Tk. However, there are times
when building from the current development version is useful. For instance,
if there is a bug in the Tcl/Tk sources, you can then see if there is a fix for
this in the Tcl/Tk development version on GitHub. If so, then you can test by
using the tcltk-dir.sh --git commandline option.

The tcltk-dir.sh script tries to detect if it's building in a 64 bit
environment, ie. MinGW 64. If this detection fails, you can force 64 bit with
the --64bit option:

    # force 64 bit Tcl/Tk 8.6.8 build
    tcltk-dir.sh --64bit 8.6.8

## pdfontloader

Tk cannot load local font files by default on Windows. Pd accomplishes this
through a tiny, custom Tcl extension, pdfontloader.dll. On initialization, the
Pd GUI tries to load pdfontloader and, if successful, tries to load the included
Pd font.

Currently, pdfontloader.dll is pre-built and included within the pdprototype.tgz
tarball. To build pdfontloader, see https://github.com/pure-data/pdfontloader
source.
