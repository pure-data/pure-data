# Pure Data macOS custom Tcl/Tk patches

This directory contains custom patches for the Tcl/Tk source trees.

As Pd uses an older version of Tcl/Tk for backwards compatibility on macOS, small bugfixes from newer versions may need to be backported for the Pd GUI. This is handled in the mac/tcltk-wish.sh script by applying custom patches in this directory to either the Tcl and/or Tk source trees.

A simple filename match is used:

* tcl${VERSION}*.patch -> applied to tcl${VERSION} source tree
* tk${VERSION}*.patch  -> applied to tk${VERSION} source tree

For example, "tcl8.5.19_somefix.patch" is applied to the "tcl8.5.19" source directory when building Tcl/Tk 8.5.19.

To create a patch using git within a cloned tcl or tk source tree:

    git diff > somefix.patch

Note: make sure the changes *only* include your own by running "git diff" first.

Patches are then applied within the relevant source tree directory using:

    patch -p1 < somefix.patch

To skip applying patches, use the tcltk-wish.sh --no-patches commandline option.
