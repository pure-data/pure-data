## various canvas-related functions
# this is really just a reboot of pdtk_canvas with a nicer namespace
# gradually migrate procs over to here.
# new procs should probably be added in here...
# (that don't fit into the pdwindow or pdtk_canvas)

package provide pd_canvas 0.1

namespace eval ::pd::canvas { }


# private helpers
proc ::pd::canvas::_lremove {list value} {
    set result {}
    foreach x $list {
        if {$x ne $value} {
            lappend result $x
        }
    }
    return $result
}




proc ::pd::canvas::set_zoom {cnv zoom} {
    set ::pdtk_canvas::_zoom($cnv) $zoom
}
proc ::pd::canvas::get_zoom {cnv} {
    if {[info exists ::pdtk_canvas::_zoom($cnv)]} {
        return $::pdtk_canvas::_zoom($cnv)
    }
    return 1
}

array set ::pd::canvas::_fontsize {}
proc ::pd::canvas::set_fontsize {cnv fontsize} {
    set ::pdtk_canvas::_fontsize($cnv) $fontsize
}
proc ::pd::canvas::get_fontsize {cnv} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set fontsize 12
    if {[info exists ::pdtk_canvas::_fontsize($cnv)]} {
        set fontsize $::pdtk_canvas::_fontsize($cnv)
    }
    return [expr $fontsize * $zoom]
}

proc ::pd::canvas::set_editmode {cnv state} {
    set ::editmode_button $state
    set ::editmode($cnv) $state
    if { [winfo exists $cnv] } {
        if { ! $state && [winfo toplevel $cnv] ne $cnv} {
            catch {$cnv delete commentbar}
        }
        event generate $cnv <<EditMode>>
    }
}

proc ::pd::canvas::set_cursor {cnv cursor} {
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
        $cnv configure -cursor $cur
    }
}


# keep track of objects on this canvas
array set ::pd::canvas::_objects {}
proc ::pd::canvas::add_object {cnv obj} {
    if {[info exists ::pd::canvas::_objects($cnv)] && [lsearch $::pd::canvas::_objects($cnv) $obj] >= 0} {
    } else {
        lappend ::pd::canvas::_objects($cnv) $obj
    }
}

proc ::pd::canvas::remove_object {cnv obj} {
    if {[info exists ::pd::canvas::_objects($cnv)]} {
        set ::pd::canvas::_objects($cnv) [_lremove $::pd::canvas::_objects($cnv) $obj]
    }
}
