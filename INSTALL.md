**Building Pure Data**
======================

If you don't already have the Pd sources, then read GettingPdSource. The rsync method is the easiest way to get started, or for more detail on the possible options for getting the source, see WorkingWithPdExtendedSources for how to put the sources together from scratch.

If you want to build only Pd without all of the libraries included in Pd-extended, then read BuildingPd. You will also need to install all of the dependencies before trying to build. That is specific to each OS, see the OS-specific pages in the Setting Up Your Machine for Building section of the Developer's Documentation. Once you have all of the sources and dependencies, you can start compiling.

Try building the whole thing and see if it works for you. If you run into included libraries that don't build on your system, you can remove them from the build. Look for LIB_TARGETS in externals/Makefile and remove the library names you do not want to build.

**GNU/Linux**
======================

For building on any GNU/Linux distro, the basic commands are currently the same. The Makefile will create a tarball unless it finds the dpkg-dev tools. If the dpkg-dev tools are found, then the end result will be a .deb.

cd pure-data/packages/linux_make
make install && make package
It will then build for a while, then you'll see a .tar.bz2 or .deb in pure-data/packages/linux_make/build. The Makefile checks for the Debian utility dpkg-shlibdeps to determine whether it should build a .deb package or a tarball. If you want to force building the tarball, you can run the build like this:

$ cd pure-data/packages/linux_make
$ make PACKAGE_TYPE=tarbz2 install && make PACKAGE_TYPE=tarbz2 package

you may also install it on linux by running:

$ yum install alsa-lib-devel
$ sudo apt-get install libasound2-dev
$ yum install jack-audio-connection-kit-devel
$ sudo apt-get install libjack-dev
$ sudo apt-get install libjack-jackd2-dev)
$ sudo apt-get install libtool

and finally:

$ sudo add-apt-repository "deb http://apt.puredata.info/releases `lsb_release -c | awk '{print $2}'` main"
$ sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-key 9f0fe587374bbe81
$ sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-key D63D3D09C39F5EEB
$ sudo apt-get update
$ sudo apt-get install pd-extended
 
$ sudo add-apt-repository ppa:eighthave/pd-extended
$ sudo apt-get update
$ sudo apt-get install pd-extended

**Mac OS X**
======================

Once you have all of the sources, then you can start compiling. Try building the whole thing and see if it works for you:

$ cd pure-data/packages/darwin_app
$ make install && make package
$ It will then build for a while, and it should end up with a Pd-extended.dmg package in pure-data/packages/darwin_app/build.

**Windows**
======================

Once you have all of the sources, then you can start compiling. Try building the whole thing and see if it works for you:

$ cd pure-data/packages/win32_inno
$ make install && make package
It will then build for a while, and it should end up with a Pd-extended.exe package in pure-data/packages/win32_inno/Output.

**Auto-Build Scripts**
======================

You can also use the same scripts that the PdLab build farm use to build Pd every night.

first get the source: GettingPdSource

then run the scripts: AutoBuildScripts

So in the Terminal, do something like this for Pd-extended:

$ mkdir ~/auto-build
$ cd ~/auto-build
https://pure-data.svn.sourceforge.net/svnroot/pure-data/branches/pd-extended/0.43 pd-extended
$ ~/auto-build/pd-extended/scripts/auto-build/pd-extended-auto-builder.sh
Building Pd-Extended For Development
See WorkingWithPdExtendedSources In order to build Pd-Extended from sources under version control in a manner that enables an effective development cycle.

