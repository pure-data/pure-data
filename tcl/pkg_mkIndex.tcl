#!/usr/bin/tclsh

puts stdout "Watch out, this doesn't work on packages with namespace import"
pkg_mkIndex -verbose -- [pwd] *.tcl *.[info sharedlibextension]

## this currently needs to be added to pkg_mkIndex manually, ug
#package ifneeded pd_menus 0.1 [list source [file join $dir pd_menus.tcl]]


