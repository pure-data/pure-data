# For information on usage and redistribution, and for a DISCLAIMER OF ALL
# WARRANTIES, see the file, "LICENSE.txt," in this distribution.
# Copyright (c) 1997-2009 Miller Puckette.

proc ::dialog_iemgui::knob_wpopup {mytoplevel} {
    $mytoplevel.wiperpopup unpost
    set button $mytoplevel.para.knbstyle.wiper.ent
    set x [expr [winfo rootx $button] + ( [winfo width $button] / 2 )]
    set y [expr [winfo rooty $button] + ( [winfo height $button] / 2 )]
    tk_popup $mytoplevel.wiperpopup $x $y 0
}

proc ::dialog_iemgui::toggle_knob_wpopup {mytoplevel gn_f} {
    set vid [string trimleft $mytoplevel .]

    set var_iemgui_wiper_style [concat iemgui_wiper_style_$vid]
    global $var_iemgui_wiper_style
    set $var_iemgui_wiper_style [$mytoplevel.wiperpopup entrycget $gn_f -label]
    $mytoplevel.para.knbstyle.wiper.ent configure -text [eval concat $$var_iemgui_wiper_style]
}

proc ::dialog_iemgui::create_properties_knb {mytoplevel ticks wiper_style arc_width start_angle end_angle} {
    #puts "start=$start_angle end=$end_angle wiper_style=$wiper_style arc_style=$arc_style"
    set vid [string trimleft $mytoplevel .]

    menu $mytoplevel.wiperpopup
    $mytoplevel.wiperpopup add command \
        -label "none" \
        -command "::dialog_iemgui::toggle_knob_wpopup $mytoplevel 0"
    $mytoplevel.wiperpopup add command \
        -label "line" \
        -command "::dialog_iemgui::toggle_knob_wpopup $mytoplevel 1"

    set var_iemgui_ticks [concat iemgui_ticks_$vid]
    global $var_iemgui_ticks
    set $var_iemgui_ticks $ticks
    set var_iemgui_wiper_style [concat iemgui_wiper_style_$vid]
    global $var_iemgui_wiper_style
    set $var_iemgui_wiper_style $wiper_style
    set var_iemgui_arc_width [concat iemgui_arc_width_$vid]
    global $var_iemgui_arc_width
    set $var_iemgui_arc_width $arc_width
    set var_iemgui_start_angle [concat iemgui_start_angle_$vid]
    global $var_iemgui_start_angle
    set $var_iemgui_start_angle $start_angle
    set var_iemgui_end_angle [concat iemgui_end_angle_$vid]
    global $var_iemgui_end_angle
    set $var_iemgui_end_angle $end_angle

    frame $mytoplevel.para.knbstyle -padx 20 -pady 1
    pack $mytoplevel.para.knbstyle -side top -fill x
    
    frame $mytoplevel.para.knbstyle.ticks
    label $mytoplevel.para.knbstyle.ticks.lab -text [_ "Ticks: "]
    entry $mytoplevel.para.knbstyle.ticks.ent -textvariable $var_iemgui_ticks -width 5
    pack $mytoplevel.para.knbstyle.ticks.ent $mytoplevel.para.knbstyle.ticks.lab -side right -anchor e

    label $mytoplevel.para.knbstyle.dummy1 -text " " -width 1

    frame $mytoplevel.para.knbstyle.wiper
    label $mytoplevel.para.knbstyle.wiper.lab -text [_ "Wiper style: "]
    button $mytoplevel.para.knbstyle.wiper.ent -text [_ [eval concat $$var_iemgui_wiper_style]] \
        -command "::dialog_iemgui::knob_wpopup $mytoplevel"
    pack $mytoplevel.para.knbstyle.wiper.ent $mytoplevel.para.knbstyle.wiper.lab -side right -anchor e

    label $mytoplevel.para.knbstyle.dummy2 -text " " -width 1
    
    frame $mytoplevel.para.knbstyle.arc
    label $mytoplevel.para.knbstyle.arc.lab -text [_ "Arc width: "]
    entry $mytoplevel.para.knbstyle.arc.ent -textvariable $var_iemgui_arc_width -width 5
    pack $mytoplevel.para.knbstyle.arc.ent $mytoplevel.para.knbstyle.arc.lab -side right -anchor e

    pack $mytoplevel.para.knbstyle.ticks $mytoplevel.para.knbstyle.dummy1 \
        $mytoplevel.para.knbstyle.wiper $mytoplevel.para.knbstyle.dummy2 \
        $mytoplevel.para.knbstyle.arc -side left


    frame $mytoplevel.para.knbangle -padx 20 -pady 1
    pack $mytoplevel.para.knbangle -side top -fill x

    frame $mytoplevel.para.knbangle.start
    label $mytoplevel.para.knbangle.start.lab -text [_ "Start angle: "]
    entry $mytoplevel.para.knbangle.start.ent -textvariable $var_iemgui_start_angle -width 5
    pack $mytoplevel.para.knbangle.start.ent $mytoplevel.para.knbangle.start.lab -side right -anchor e

    label $mytoplevel.para.knbangle.dummy1 -text " " -width 1

    frame $mytoplevel.para.knbangle.end
    label $mytoplevel.para.knbangle.end.lab -text [_ "End angle: "]
    entry $mytoplevel.para.knbangle.end.ent -textvariable $var_iemgui_end_angle -width 5
    pack $mytoplevel.para.knbangle.end.ent $mytoplevel.para.knbangle.end.lab -side right -anchor e

    pack $mytoplevel.para.knbangle.start $mytoplevel.para.knbangle.dummy1 $mytoplevel.para.knbangle.end -side left
}

proc ::dialog_iemgui::apply_knb {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    set var_iemgui_ticks [concat iemgui_ticks_$vid]
    global $var_iemgui_ticks
    set var_iemgui_wiper_style [concat iemgui_wiper_style_$vid]
    global $var_iemgui_wiper_style
    set var_iemgui_arc_width [concat iemgui_arc_width_$vid]
    global $var_iemgui_arc_width
    set var_iemgui_start_angle [concat iemgui_start_angle_$vid]
    global $var_iemgui_start_angle
    set var_iemgui_end_angle [concat iemgui_end_angle_$vid]
    global $var_iemgui_end_angle
    set var_iemgui_wdt [concat iemgui_wdt_$vid]
    global $var_iemgui_wdt

    if {[eval concat $$var_iemgui_end_angle] < -360 } { set $var_iemgui_end_angle -360 }
    if {[eval concat $$var_iemgui_end_angle] > 360 } { set $var_iemgui_end_angle 360 }
    if {[eval concat $$var_iemgui_start_angle] < -360 } { set $var_iemgui_start_angle -360 }
    if {[eval concat $$var_iemgui_start_angle] > 360 } { set $var_iemgui_start_angle 360 }

    if {[eval concat $$var_iemgui_end_angle] < [eval concat $$var_iemgui_start_angle] } {
        set endangle [eval concat $$var_iemgui_end_angle]
        set $var_iemgui_end_angle [eval concat $$var_iemgui_start_angle]
        set $var_iemgui_start_angle $endangle
    }

    if {[eval concat $$var_iemgui_end_angle] - [eval concat $$var_iemgui_start_angle] > 360 } {
        set $var_iemgui_end_angle [expr [eval concat $$var_iemgui_start_angle] + 360]
    }

    if {[eval concat $$var_iemgui_end_angle] == [eval concat $$var_iemgui_start_angle]} {
        set $var_iemgui_end_angle [expr [eval concat $$var_iemgui_start_angle] + 1]
    }

    if {[eval concat $$var_iemgui_ticks] < 0 } { set $var_iemgui_ticks 0 }
    if {[eval concat $$var_iemgui_ticks] > 360 } { set $var_iemgui_ticks 360 }

    set max_arc_width [expr ([eval concat $$var_iemgui_wdt ] - 1) / 2]
    set min_arc_width [expr -([eval concat $$var_iemgui_wdt ] / 2 + 1)]
    if {[eval concat $$var_iemgui_arc_width] < $min_arc_width } {
        set $var_iemgui_arc_width $min_arc_width
    }
    if {[eval concat $$var_iemgui_arc_width] > $max_arc_width } {
        set $var_iemgui_arc_width $max_arc_width
    }

    return [list [eval concat $$var_iemgui_ticks] \
        [eval concat $$var_iemgui_wiper_style] [eval concat $$var_iemgui_arc_width] \
        [eval concat $$var_iemgui_start_angle] [eval concat $$var_iemgui_end_angle]]
}

