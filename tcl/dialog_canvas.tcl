
# TODO offset this panel so it doesn't overlap the pdtk_array panel

package provide dialog_canvas 0.1

namespace eval ::dialog_canvas:: {
    namespace export pdtk_canvas_dialog
}

# global variables to store checkbox state on canvas properties window.  These
# are only used in the context of getting data from the checkboxes, so they
# aren't really useful elsewhere.  It would be nice to have them globally
# useful, but that would mean changing the C code.
array set graphme_button {}
array set hidetext_button {}

############# pdtk_canvas_dialog -- dialog window for canvases #################

proc ::dialog_canvas::apply {mytoplevel} {
    pdsend "$mytoplevel donecanvasdialog \
            [$mytoplevel.scale.x.entry get] \
            [$mytoplevel.scale.y.entry get] \
            [expr $::graphme_button($mytoplevel) + 2 * $::hidetext_button($mytoplevel)] \
            [$mytoplevel.range.x.from_entry get] \
            [$mytoplevel.range.y.from_entry get] \
            [$mytoplevel.range.x.to_entry get] \
            [$mytoplevel.range.y.to_entry get] \
            [$mytoplevel.range.x.size_entry get] \
            [$mytoplevel.range.y.size_entry get] \
            [$mytoplevel.range.x.margin_entry get] \
            [$mytoplevel.range.y.margin_entry get]"
}

proc ::dialog_canvas::cancel {mytoplevel} {
    pdsend "$mytoplevel cancel"
}

proc ::dialog_canvas::ok {mytoplevel} {
    ::dialog_canvas::apply $mytoplevel
    ::dialog_canvas::cancel $mytoplevel
}

proc ::dialog_canvas::checkcommand {mytoplevel} {
    if { $::graphme_button($mytoplevel) != 0 } {
        $mytoplevel.scale.x.entry configure -state disabled
        $mytoplevel.scale.y.entry configure -state disabled
        $mytoplevel.parent.hidetext configure -state normal
        $mytoplevel.range.x.from_entry configure -state normal
        $mytoplevel.range.x.to_entry configure -state normal
        $mytoplevel.range.x.size_entry configure -state normal
        $mytoplevel.range.x.margin_entry configure -state normal
        $mytoplevel.range.y.from_entry configure -state normal
        $mytoplevel.range.y.to_entry configure -state normal
        $mytoplevel.range.y.size_entry configure -state normal
        $mytoplevel.range.y.margin_entry configure -state normal
        if { [$mytoplevel.range.x.from_entry get] == 0 \
                 && [$mytoplevel.range.y.from_entry get] == 0 \
                 && [$mytoplevel.range.x.to_entry get] == 0 \
                 && [$mytoplevel.range.y.to_entry get] == 0 } {
            $mytoplevel.range.y.to_entry insert 0 1
            $mytoplevel.range.y.to_entry insert 0 1
        }
        if { [$mytoplevel.range.x.size_entry get] == 0 } {
            $mytoplevel.range.x.size_entry delete 0 end
            $mytoplevel.range.x.margin_entry delete 0 end
            $mytoplevel.range.x.size_entry insert 0 85
            $mytoplevel.range.x.margin_entry insert 0 100
        }
        if { [$mytoplevel.range.y.size_entry get] == 0 } {
            $mytoplevel.range.y.size_entry delete 0 end
            $mytoplevel.range.y.margin_entry delete 0 end
            $mytoplevel.range.y.size_entry insert 0 60
            $mytoplevel.range.y.margin_entry insert 0 100
       }
    } else {
        $mytoplevel.scale.x.entry configure -state normal
        $mytoplevel.scale.y.entry configure -state normal
        $mytoplevel.parent.hidetext configure -state disabled
        $mytoplevel.range.x.from_entry configure -state disabled
        $mytoplevel.range.x.to_entry configure -state disabled
        $mytoplevel.range.x.size_entry configure -state disabled
        $mytoplevel.range.x.margin_entry configure -state disabled
        $mytoplevel.range.y.from_entry configure -state disabled
        $mytoplevel.range.y.to_entry configure -state disabled
        $mytoplevel.range.y.size_entry configure -state disabled
        $mytoplevel.range.y.margin_entry configure -state disabled
        if { [$mytoplevel.scale.x.entry get] == 0 } {
            $mytoplevel.scale.x.entry delete 0 end
            $mytoplevel.scale.x.entry insert 0 1
        }
        if { [$mytoplevel.scale.y.entry get] == 0 } {
            $mytoplevel.scale.y.entry delete 0 end
            $mytoplevel.scale.y.entry insert 0 1
        }
    }
}

proc ::dialog_canvas::pdtk_canvas_dialog {mytoplevel xscale yscale graphmeflags \
                                             xfrom yfrom xto yto \
                                             xsize ysize xmargin ymargin} {
    if {[winfo exists $mytoplevel]} {
        wm deiconify $mytoplevel
        raise $mytoplevel
    } else {
        create_dialog $mytoplevel
    }
    switch -- $graphmeflags {
        0 {
            $mytoplevel.parent.graphme deselect
            $mytoplevel.parent.hidetext deselect
        } 1 {
            $mytoplevel.parent.graphme select
            $mytoplevel.parent.hidetext deselect
        } 2 {
            $mytoplevel.parent.graphme deselect
            $mytoplevel.parent.hidetext select
        } 3 {
            $mytoplevel.parent.graphme select
            $mytoplevel.parent.hidetext select
        } default {
            ::pdwindow::error [_ "WARNING: unknown graphme flags received in pdtk_canvas_dialog"]
        }
    }

    $mytoplevel.scale.x.entry insert 0 $xscale
    $mytoplevel.scale.y.entry insert 0 $yscale
    $mytoplevel.range.x.from_entry insert 0 $xfrom
    $mytoplevel.range.y.from_entry insert 0 $yfrom
    $mytoplevel.range.x.to_entry insert 0 $xto
    $mytoplevel.range.y.to_entry insert 0 $yto
    $mytoplevel.range.x.size_entry insert 0 $xsize
    $mytoplevel.range.y.size_entry insert 0 $ysize
    $mytoplevel.range.x.margin_entry insert 0 $xmargin
    $mytoplevel.range.y.margin_entry insert 0 $ymargin

   ::dialog_canvas::checkcommand $mytoplevel
}

proc ::dialog_canvas::create_dialog {mytoplevel} {
    toplevel $mytoplevel -class DialogWindow
    wm title $mytoplevel [_ "Canvas Properties"]
    wm group $mytoplevel .
    wm resizable $mytoplevel 0 0
    wm transient $mytoplevel $::focused_window
    $mytoplevel configure -menu $::dialog_menubar
    $mytoplevel configure -padx 0 -pady 0
    ::pd_bindings::dialog_bindings $mytoplevel "canvas"
    
    labelframe $mytoplevel.scale -text [_ "Scale"] -borderwidth 1
    pack $mytoplevel.scale -side top -fill x
    frame $mytoplevel.scale.x -pady 2 -borderwidth 1
    pack $mytoplevel.scale.x -side top
    label $mytoplevel.scale.x.label -text [_ "X units per pixel:"]
    entry $mytoplevel.scale.x.entry -width 10
    pack $mytoplevel.scale.x.label $mytoplevel.scale.x.entry -side left
    frame $mytoplevel.scale.y -pady 2
    pack $mytoplevel.scale.y -side top
    label $mytoplevel.scale.y.label -text [_ "Y units per pixel:"]
    entry $mytoplevel.scale.y.entry -width 10
    pack $mytoplevel.scale.y.label $mytoplevel.scale.y.entry -side left

    labelframe $mytoplevel.parent -text [_ "Appearance on parent patch"] -borderwidth 1
    pack $mytoplevel.parent -side top -fill x
    checkbutton $mytoplevel.parent.graphme -text [_ "Graph-On-Parent"] \
        -anchor w -variable graphme_button($mytoplevel) \
        -command [concat ::dialog_canvas::checkcommand $mytoplevel]
    pack $mytoplevel.parent.graphme -side top -fill x -padx 40
    checkbutton $mytoplevel.parent.hidetext -text [_ "Hide object name and arguments"] \
        -anchor w -variable hidetext_button($mytoplevel) \
        -command [concat ::dialog_canvas::checkcommand $mytoplevel]
    pack $mytoplevel.parent.hidetext -side top -fill x -padx 40

    labelframe $mytoplevel.range -text [_ "Range and size"] -borderwidth 1
    pack $mytoplevel.range -side top -fill x
    frame $mytoplevel.range.x -padx 2 -pady 2
    pack $mytoplevel.range.x -side top
    label $mytoplevel.range.x.from_label -text [_ "X range, from"]
    entry $mytoplevel.range.x.from_entry -width 6
    label $mytoplevel.range.x.to_label -text [_ "to"]
    entry $mytoplevel.range.x.to_entry -width 6
    label $mytoplevel.range.x.size_label -text [_ "Size:"]
    entry $mytoplevel.range.x.size_entry -width 4
    label $mytoplevel.range.x.margin_label -text [_ "Margin:"]
    entry $mytoplevel.range.x.margin_entry -width 4
    pack $mytoplevel.range.x.from_label $mytoplevel.range.x.from_entry \
        $mytoplevel.range.x.to_label $mytoplevel.range.x.to_entry \
        $mytoplevel.range.x.size_label $mytoplevel.range.x.size_entry \
        $mytoplevel.range.x.margin_label $mytoplevel.range.x.margin_entry \
        -side left
    frame $mytoplevel.range.y -padx 2 -pady 2
    pack $mytoplevel.range.y -side top
    label $mytoplevel.range.y.from_label -text [_ "Y range, from"]
    entry $mytoplevel.range.y.from_entry -width 6
    label $mytoplevel.range.y.to_label -text [_ "to"]
    entry $mytoplevel.range.y.to_entry -width 6
    label $mytoplevel.range.y.size_label -text [_ "Size:"]
    entry $mytoplevel.range.y.size_entry -width 4
    label $mytoplevel.range.y.margin_label -text [_ "Margin:"]
    entry $mytoplevel.range.y.margin_entry -width 4
    pack $mytoplevel.range.y.from_label $mytoplevel.range.y.from_entry \
        $mytoplevel.range.y.to_label $mytoplevel.range.y.to_entry \
        $mytoplevel.range.y.size_label $mytoplevel.range.y.size_entry \
        $mytoplevel.range.y.margin_label $mytoplevel.range.y.margin_entry \
        -side left

    frame $mytoplevel.buttons
    pack $mytoplevel.buttons -side bottom -fill x -expand 1 -pady 2m
    button $mytoplevel.buttons.cancel -text [_ "Cancel"] \
        -command "::dialog_canvas::cancel $mytoplevel"
    pack $mytoplevel.buttons.cancel -side left -expand 1 -fill x -padx 10
    if {$::windowingsystem ne "aqua"} {
        button $mytoplevel.buttons.apply -text [_ "Apply"] \
            -command "::dialog_canvas::apply $mytoplevel"
        pack $mytoplevel.buttons.apply -side left -expand 1 -fill x -padx 10
    }
    button $mytoplevel.buttons.ok -text [_ "OK"] \
        -command "::dialog_canvas::ok $mytoplevel"
    pack $mytoplevel.buttons.ok -side left -expand 1 -fill x -padx 10
 }
