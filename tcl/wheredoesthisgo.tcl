
package provide wheredoesthisgo 0.1

# a place to temporarily store things until they find a home or go away

set help_top_directory ""


proc post_tclinfo {} {
    pdtk_post "Tcl library: [file normalize [info library]]"
    pdtk_post "executable: [file normalize [info nameofexecutable]]"
    pdtk_post "tclversion: [info tclversion]"
    pdtk_post "patchlevel: [info patchlevel]"
    pdtk_post "sharedlibextension: [info sharedlibextension]"
}


proc placeholder {args} {
    # PLACEHOLDER
    ::pdwindow::pdtk_post "PLACEHOLDER $args"
}


proc open_file {filename} {
    set directory [file dirname $filename]
    set basename [file tail $filename]
    if {[regexp -nocase -- "\.(pd|pat|mxt)$" $filename]} {
        pdsend "pd open [enquote_path $basename] [enquote_path $directory]"
        # remove duplicates first, then the duplicate added after to the top
        set index [lsearch -exact $::recentfiles_list $filename]
        set ::recentfiles_list [lreplace $::recentfiles_list $index $index]
        set ::recentfiles_list \
            "$filename [lrange $::recentfiles_list 0 $::total_recentfiles]"
        ::pd_menus::update_recentfiles_menu
    }
}

proc lookup_windowname {mytoplevel} {
    foreach window $::menu_windowlist {
        if {[lindex $window 1] eq $mytoplevel} {
            return [lindex $window 0]
        }
    }
}
    
# ------------------------------------------------------------------------------
# quoting functions

# TODO enquote a filename to send it to pd, " isn't handled properly tho...
proc enquote_path {message} {
    string map {"," "\\," ";" "\\;" " " "\\ "} $message
}

#enquote a string to send it to Pd.  Blow off semi and comma; alias spaces
#we also blow off "{", "}", "\" because they'll just cause bad trouble later.
proc unspace_text {x} {
    set y [string map {" " "_" ";" "" "," "" "{" "" "}" "" "\\" ""} $x]
    if {$y eq ""} {set y "empty"}
    concat $y
}


# ------------------------------------------------------------------------------
# lost pdtk functions...

# set the checkbox on the "Compute Audio" menuitem and checkbox
proc pdtk_pd_dsp {value} {
    # TODO canvas_startdsp/stopdsp should really send 1 or 0, not "ON" or "OFF"
    if {$value eq "ON"} {
        set ::dsp 1
    } else {
        set ::dsp 0
    }
}

proc pdtk_pd_dio {red} {
    # puts stderr [concat pdtk_pd_dio $red]
}


proc pdtk_watchdog {} {
   pdsend "pd watchdog"
   after 2000 {pdtk_watchdog}
}


proc pdtk_ping {} {
    pdsend "pd ping"
}

# ------------------------------------------------------------------------------
# kludges to avoid changing C code

proc .mbar.find {command number} {
    # this should be changed in g_canvas.c, around line 800
    .menubar.find $command $number
}
