## various window-related functions
# (that don't fit into the pdwindow or pdtk_canvas)

package provide pd_windows 0.1
package require Tcl 8.5

namespace eval ::pd::window { }

# position one window over another
proc ::pd::window::position_over {child parent} {
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
# procs for panels (openpanel, savepanel)

proc ::pd::window::openpanel {target localdir {mode 0}} {
    if {! [file isdirectory $localdir]} {
        if { ! [file isdirectory $::fileopendir]} {
            set ::fileopendir $::env(HOME)
        }
        set localdir $::fileopendir
    }
    # 0: file, 1: directory, 2: multiple files
    switch $mode {
        0 { set result [tk_getOpenFile -initialdir $localdir] }
        1 { set result [tk_chooseDirectory -initialdir $localdir] }
        2 { set result [tk_getOpenFile -multiple 1 -initialdir $localdir] }
        default { ::pdwindow::error "bad value for 'mode' argument" }
    }
    if {$result ne ""} {
        if { $mode == 2 } {
            # 'result' is a list
            set ::fileopendir [file dirname [lindex $result 0]]
            set args {}
            foreach path $result {
                lappend args [enquote_path $path]
            }
            pdsend "$target callback [join $args]"
        } else {
            set ::fileopendir [expr {$mode == 0 ? [file dirname $result] : $result}]
            pdsend "$target callback [enquote_path $result]"
        }
    }
}

proc ::pd::window::savepanel {target localdir} {
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




# #######################################################################
# compat aliases of the above
proc position_over_window {args} { ::pd::window::position_over {*}$args }
proc pdtk_openpanel {args} {::pd::window::openpanel {*}$args}
proc pdtk_savepanel {args} {::pd::window::savepanel {*}$args}
