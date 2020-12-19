# Pure Data macOS custom Tcl/Tk patches

This directory contains custom patches for the Tcl/Tk source trees.

As Pd uses an older version of Tcl/Tk for backwards compatibility on macOS,
small bugfixes from newer versions may need to be backported for the Pd GUI.
This is handled in the mac/tcltk-wish.sh script by applying custom patches in
this directory to either the Tcl and/or Tk source trees.

A simple filename match is used:

* tcl${VERSION}*.patch -> applied to tcl${VERSION} source tree
* tk${VERSION}*.patch  -> applied to tk${VERSION} source tree

For example, "tcl8.5.19_somefix.patch" is applied to the "tcl8.5.19" source
directory when building Tcl/Tk 8.5.19.

To create a patch using git within a cloned tcl or tk source tree:

    git diff > somefix.patch

Note: make sure the changes *only* include your own by running "git diff" first.

Patches are then applied within the relevant source tree directory using:

    patch -p1 < somefix.patch

To skip applying patches, use the tcltk-wish.sh --no-patches commandline option.

## Current Patches

### tk8.6.10_zombiewindows.patch

Backport fix for zombie windows on systems with the Touchbar which could cause
Pd to crash when closing the Font dialog due to a nil window pointer.

### tk8.6.10_scrollbars.patch

Backport commit which fixes scrollbars not (re)drawing, see

    https://core.tcl-lang.org/tk/info/71433282feea6ae9

### tk8.6.10_helpmenu.patch:

Fixes help menu items disabling after switching windows, see

	https://core.tcl-lang.org/tk/info/ad7edbfafdda63e6
	https://core.tcl-lang.org/tk/vinfo/8c9db659a45831ee

### tk8.6.10_keyfix.patch

The tk8.5.19_keyfix.patch updated for Tk 8.6.10.

### tk8.5.19_keyfix.patch

Fixes key release events and adds repeat handling, see

    https://github.com/pure-data/pure-data/issues/213
