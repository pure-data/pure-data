
package provide wheredoesthisgo 0.1

namespace eval ::pd::private:: {
    # these are some private variables for which there is
    # no more specific namespace yet...
    variable lastsavedir ""
    variable lastopendir ""
}

# a place to temporarily store things until they find a home or go away

proc open_file {filename} {
    if {$filename == ""} { return }
    set directory [file normalize [file dirname $filename]]
    set basename [file tail $filename]
    if { ! [file exists $filename]} {
        ::pdwindow::post [_ "ignoring '%s': doesn't exist" $filename]
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
        ::pdwindow::post [_ "ignoring '%s': doesn't look like a Pd file" $filename]
        ::pdwindow::post "\n"
    }
}

# ------------------------------------------------------------------------------
# procs for panels (openpanel, savepanel)
proc pdtk_openpanel {target localdir {mode 0} {parent .pdwindow}} {
    set localdir [file normalize $localdir]
    if { $::pd::private::lastopendir == "" } {
        if { ! [file isdirectory $::fileopendir]} {
            set ::fileopendir $::env(HOME)
        }
        set ::pd::private::lastopendir $::fileopendir
    }
    if {! [file isdirectory $localdir]} {
        set localdir $::pd::private::lastopendir
    }
    if { ! [winfo exists parent] } {
        set parent .pdwindow
    }

    set result ""
    if { [winfo exists parent] } {
        # 0: file, 1: directory, 2: multiple files
        switch $mode {
            0 { set result [tk_getOpenFile -initialdir $localdir \
                                -parent $parent] }
            1 { set result [tk_chooseDirectory -initialdir $localdir \
                                -parent $parent] }
            2 { set result [tk_getOpenFile -multiple 1 -initialdir $localdir \
                                -parent $parent] }
            default { ::pdwindow::error "bad value for 'mode' argument" }
        }
    } else {
        switch $mode {
            0 { set result [tk_getOpenFile -initialdir $localdir] }
            1 { set result [tk_chooseDirectory -initialdir $localdir] }
            2 { set result [tk_getOpenFile -multiple 1 -initialdir $localdir] }
            default { ::pdwindow::error "bad value for 'mode' argument" }
        }
    }
    if {$result ne ""} {
        if { $mode == 2 } {
            # 'result' is a list
            set ::pd::private::lastopendir [file dirname [lindex $result 0]]
            set ::fileopendir $::pd::private::lastopendir
            set args {}
            foreach path $result {
                lappend args [enquote_path $path]
            }
            pdsend "$target callback [join $args]"
        } else {
            set ::pd::private::lastopendir [expr {$mode == 0 ? [file dirname $result] : $result}]
            set ::fileopendir $::pd::private::lastopendir
            pdsend "$target callback [enquote_path $result]"
        }
    }
}


proc pdtk_savepanel {target localdir {parent .pdwindow}} {
    set localdir [file normalize $localdir]
    if { $::pd::private::lastsavedir == "" } {
        if { ! [file isdirectory $::filenewdir]} {
            set ::filenewdir $::env(HOME)
        }
        set ::pd::private::lastsavedir $::filenewdir
    }
    if {! [file isdirectory $localdir]} {
        set localdir $::pd::private::lastsavedir
    }
    if { ! [winfo exists parent] } {
        set parent .pdwindow
    }
    if { [winfo exists parent] } {
        set filename [tk_getSaveFile -initialdir $localdir -parent $parent]
    } else {
        set filename [tk_getSaveFile -initialdir $localdir]
    }
    if {$filename ne ""} {
        set ::pd::private::lastsavedir [file dirname $filename]
        pdsend "$target callback [enquote_path $filename]"
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
    }
    if { [winfo exists $mytoplevel] } {
        return [wm title [winfo toplevel $mytoplevel]]
    }
    return ERROR
}

proc tkcanvas_name {mytoplevel} {
    return "$mytoplevel.c"
}

# ------------------------------------------------------------------------------
# window helpers

# position one window over another
proc position_over_window {child parent} {
    if {![winfo exists $parent]} {return}
    # use internal tk::PlaceWindow http://wiki.tcl.tk/534
    # so fallback if not available
    if {[catch {tk::PlaceWindow $child widget $parent}]} {
        set x [expr [winfo x $parent] + ([winfo width $parent] / 2) - ([winfo reqwidth $child] / 2)]
        set y [expr [winfo y $parent] + ([winfo height $parent] / 2) - ([winfo reqheight $child] / 2)]
        wm geometry $child "+${x}+${y}"
    }
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

# 'set' a variable with escaped value(s) unescaped
# (to be used over the wire)
proc set_escaped {varname args} {
    upvar ${varname} localvar
    set localvar {}
    foreach arg ${args} {
        set x [subst -nocommands -novariables ${arg}]
        lappend localvar $x
    }
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
