## various canvas-related functions
# this is really just a reboot of pdtk_canvas with a nicer namespace
# gradually migrate procs over to here.
# new procs should probably be added in here...
# (that don't fit into the pdwindow or pdtk_canvas)

package provide pd_canvas 0.1

namespace eval ::pd::canvas { }


proc ::pd::canvas::set_zoom {cnv zoom} {
    set ::pdtk_canvas::_zoom($cnv) $zoom
}
proc ::pd::canvas::get_zoom {cnv} {
    if {[info exists ::pdtk_canvas::_zoom($cnv)]} {
        return $::pdtk_canvas::_zoom($cnv)
    }
    return 1
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
