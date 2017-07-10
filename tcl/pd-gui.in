#!/bin/sh
#
# script to launch the Pure Data gui

# this will be replaced with a full path during `make`
prefix=@prefix@
exec_prefix=@exec_prefix@
PD_PREFIX=@libdir@/@PACKAGE@

# for convenience, a symlink is created from this (unexpanded) file
# to ${builddir}/bin/pd-gui
# when called as such, we want to launch the local (non-installed) pd-gui.tcl
if [ "x${PD_PREFIX#@}" != "x${PD_PREFIX}" ]; then
# the PD_PREFIX starts with '@', so it hasn't been replaced...
 PD_PREFIX=${0%/*}/..
fi

exec ${PD_PREFIX}/tcl/pd-gui.tcl "$@"
