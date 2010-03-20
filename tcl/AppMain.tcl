# This file is for the Wish.app on Mac OS X.  It is only used when a Wish.app
# is loading embedded pd code on Mac OS X.  It is completely unused on any
# other configuration, like when 'pd' launches Wish.app or when 'pd' is using
# an X11 wish on Mac OS X.  GNU/Linux and Windows will never use this file.

package require apple_events

# TODO is there anything useful to do with the psn (Process Serial Number)?
if {[string first "-psn" [lindex $argv 0]] == 0} { 
    set argv [lrange $argv 1 end]
    set argc [expr $argc - 1]
}

# launch pd-gui.tcl here
if [catch {source [file join [file dirname [info script]] pd-gui.tcl]}] { 
    puts stderr $errorInfo
}
