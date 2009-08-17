# This file is for the Wish.app on Mac OS X.  It is only used when a Wish.app
# is loading embedded pd code on Mac OS X.  It is completely unused on any
# other configuration, like when 'pd' launches Wish.app or when 'pd' is using
# an X11 wish on Mac OS X.  GNU/Linux and Windows will never use this file.


puts --------------------------AppMain.tcl-----------------------------------
catch {console show}

# FIXME apple_events must require a newer tcl than 8.4?
# package require apple_events

puts "AppMain.tcl"
puts "argv0: $argv0"
puts "executable: [info nameofexecutable]"
puts "argc: $argc argv: $argv"

# TODO is there anything useful to do with the psn (Process Serial Number)?
if {[string first "-psn" [lindex $argv 0]] == 0} { 
    set argv [lrange $argv 1 end]
    set argc [expr $argc - 1]
}

# launch pd.tk here
if [catch {source [file join [file dirname [info script]] ../tcl/pd.tcl]}] { 
    puts stderr $errorInfo
}
