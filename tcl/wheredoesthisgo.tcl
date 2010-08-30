
package provide wheredoesthisgo 0.1

# a place to temporarily store things until they find a home or go away

proc open_file {filename} {
    set directory [file normalize [file dirname $filename]]
    set basename [file tail $filename]
    if {[regexp -nocase -- "\.(pd|pat|mxt)$" $filename]} {
        ::pdtk_canvas::started_loading_file [format "%s/%s" $basename $filename]
        pdsend "pd open [enquote_path $basename] [enquote_path $directory]"
        # remove duplicates first, then the duplicate added after to the top
        set index [lsearch -exact $::recentfiles_list $filename]
        set ::recentfiles_list [lreplace $::recentfiles_list $index $index]
        lappend ::recentfiles_list $filename
        set ::recentfiles_list [lrange $::recentfiles_list 0 $::total_recentfiles]
        ::pd_menus::update_recentfiles_menu
    } {
        ::pdwindow::post 2 [format [_ "Ignoring '%s': doesn't look like a Pd-file" ] $filename ]
    }
}
    
# ------------------------------------------------------------------------------
# procs for panels (openpanel, savepanel)

proc pdtk_openpanel {target localdir} {
    if {! [file isdirectory $localdir]} {
        if { ! [file isdirectory $::fileopendir]} {
            set ::fileopendir $::env(HOME)
        }
        set localdir $::fileopendir
    }
    set filename [tk_getOpenFile -initialdir $localdir]
    if {$filename ne ""} {
        set ::fileopendir [file dirname $filename]
        pdsend "$target callback [enquote_path $filename]"
    }
}

proc pdtk_savepanel {target localdir} {
    if {! [file isdirectory $localdir]} {
        if { ! [file isdirectory $::filenewdir]} {
            set ::filenewdir $::env(HOME)
        }
        set localdir $::filenewdir
    }
    set filename [tk_getSaveFile -initialdir $localdir]
    if {$filename ne ""} {
        pdsend "$target callback [enquote_path $filename]"
    }
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
    set y [string map {" " "_" ";" "" "," "" "{" "" "}" "" "\\" ""} $x]
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
