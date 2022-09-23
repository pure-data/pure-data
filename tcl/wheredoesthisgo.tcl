
package provide wheredoesthisgo 0.1

# a place to temporarily store things until they find a home or go away

proc open_file {filename} {
    if {$filename == ""} { return }
    set directory [file normalize [file dirname $filename]]
    set basename [file tail $filename]
    if { ! [file exists $filename]} {
        ::pdwindow::post [format [_ "ignoring '%s': doesn't exist"] $filename]
        ::pdwindow::post "\n"
        # remove from recent files
        ::pd_guiprefs::update_recentfiles $filename true
        return
    }
    set ::fileopendir $directory
    if {[regexp -nocase -- "\.(pd|pat|mxt)$" $filename]} {
        ::pdtk_canvas::started_loading_file [format "%s/%s" $basename $filename]
        pdsend "pd open [enquote_path $basename] [enquote_path $directory]"
        # now this is done in pd_guiprefs
        ::pd_guiprefs::update_recentfiles $filename
    } else {
        ::pdwindow::post [format [_ "ignoring '%s': doesn't look like a Pd file"] $filename]
        ::pdwindow::post "\n"
    }
}


# ------------------------------------------------------------------------------
# path helpers

# adds to the sys_searchpath user search paths directly
proc add_to_searchpaths {path {save true}} {
    # try not to add duplicates
    foreach searchpath $::sys_searchpath {
        set dir [string trimright $searchpath [file separator]]
        if {"$dir" eq "$path"} {
            return
        }
    }
    # tell pd about the new path
    if {$save} {set save 1} else {set save 0}
    pdsend "pd add-to-path [pdtk_encodedialog ${path}] $save"
    # append to search paths as this won't be
    # updated from the pd core until a restart
    lappend ::sys_searchpath "$path"
}

# ------------------------------------------------------------------------------
# window info (name, path, parents, children, etc.)

proc lookup_windowname {mytoplevel} {
    set window [array get ::windowname $mytoplevel]
    if { $window ne ""} {
        return [lindex $window 1]
    } else {
        return ERROR
    }
}

proc tkcanvas_name {mytoplevel} {
    return "$mytoplevel.c"
}

# ------------------------------------------------------------------------------
# quoting functions

# enquote a string for find, path, and startup dialog panels, to be decoded by
# sys_decodedialog()
proc pdtk_encodedialog {x} {
    concat +[string map {" " "+_" "$" "+d" ";" "+s" "," "+c" "+" "++"} $x]
}

# encode a list with pdtk_encodedialog
proc pdtk_encode { listdata } {
    set outlist {}
    foreach this_path $listdata {
        if {0==[string match "" $this_path]} {
            lappend outlist [pdtk_encodedialog $this_path]
        }
    }
    return $outlist
}

# TODO enquote a filename to send it to pd, " isn't handled properly tho...
proc enquote_path {message} {
    string map {"," "\\," ";" "\\;" " " "\\ "} $message
}

#enquote a string to send it to Pd.  Blow off semi and comma; alias spaces
#we also blow off "{", "}", "\" because they'll just cause bad trouble later.
proc unspace_text {x} {
    set y [string map {" " {\ } ";" "" "," "" "{" "" "}" "" "\\" ""} $x]
    if {$y eq ""} {set y "empty"}
    concat $y
}

#dequote a string received from Pd.
proc respace_text {x} {
    set y [string map {{\ } " "} $x]
    if {$y eq ""} {set y "empty"}
    concat $y
}

# ------------------------------------------------------------------------------
# watchdog functions

proc pdtk_watchdog {} {
   pdsend "pd watchdog"
   after 2000 {pdtk_watchdog}
}

proc pdtk_ping {} {
    pdsend "pd ping"
}

# ------------------------------------------------------------------------------
# debugging functions

# print the function with params before it gets called
proc DDD {args} {
    ::pdwindow::error "${args}\n"
    {*}$args
}
