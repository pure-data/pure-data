## various window-related functions
# (that don't fit into the pdwindow or pdtk_canvas)

package provide pd_windows 0.1

namespace eval ::pd::window { }


proc ::pd::window::set_cursor {winid cursor} {
    set cur {}
    switch -exact -- $cursor {
        "runmode_nothing" {set cur $::cursor_runmode_nothing}
        "runmode_clickme" {set cur $::cursor_runmode_clickme}
        "runmode_thicken" {set cur $::cursor_runmode_thicken}
        "runmode_addpoint" {set cur $::cursor_runmode_addpoint}

        "editmode_nothing" {set cur $::cursor_editmode_nothing}
        "editmode_connect" {set cur $::cursor_editmode_connect}
        "editmode_disconnect" {set cur $::cursor_editmode_disconnect}
        "editmode_resize" {set cur $::cursor_editmode_resize}

        default {
            ::pdwindow::error "Unknown cursor '${cursor}'\n"
        }
    }
    if { $cur ne {} } {
        $winid configure -cursor $cur
    }
}

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
# compat alias of the above
proc position_over_window {child parent} { ::pd::window::position_over $child $parent }
