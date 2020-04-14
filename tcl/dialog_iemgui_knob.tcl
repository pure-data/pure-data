# For information on usage and redistribution, and for a DISCLAIMER OF ALL
# WARRANTIES, see the file, "LICENSE.txt," in this distribution.
# Copyright (c) 2020 Antoine Rousseau.

proc ::dialog_iemgui::preset_col_knb {mytoplevel presetcol} {
    set vid [string trimleft $mytoplevel .]
    set var_iemgui_l2_f1_b0 [concat iemgui_l2_f1_b0_$vid]
    global $var_iemgui_l2_f1_b0
    set var_iemgui_acol [concat iemgui_acol_$vid]
    global $var_iemgui_acol

    if { [eval concat $$var_iemgui_l2_f1_b0] == 3 } { set $var_iemgui_acol $presetcol }
}

proc ::dialog_iemgui::choose_col_knb {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    set var_iemgui_l2_f1_b0 [concat iemgui_l2_f1_b0_$vid]
    global $var_iemgui_l2_f1_b0
    set var_iemgui_acol [concat iemgui_acol_$vid]
    global $var_iemgui_acol

    if {[eval concat $$var_iemgui_l2_f1_b0] == 3} {
        set $var_iemgui_acol [eval concat $$var_iemgui_acol]
        set helpstring [tk_chooseColor -title [_ "Arc color"] -initialcolor [eval concat $$var_iemgui_acol]]
        if { $helpstring ne "" } {
            set $var_iemgui_acol $helpstring }
    }
}

proc ::dialog_iemgui::set_col_example_knb {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    set var_iemgui_bcol [concat iemgui_bcol_$vid]
    global $var_iemgui_bcol
    set var_iemgui_acol [concat iemgui_acol_$vid]
    global $var_iemgui_acol

    $mytoplevel.colors.sections.exp.arc_bk configure \
        -background [eval concat $$var_iemgui_bcol] \
        -activebackground [eval concat $$var_iemgui_bcol] \
        -foreground [eval concat $$var_iemgui_acol] \
        -activeforeground [eval concat $$var_iemgui_acol]
}

proc ::dialog_iemgui::create_properties_knb {mytoplevel ticks arc_color arc_width \
        start_angle end_angle} {
    set vid [string trimleft $mytoplevel .]

    set var_iemgui_ticks [concat iemgui_ticks_$vid]
    global $var_iemgui_ticks
    set $var_iemgui_ticks $ticks
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

    frame $mytoplevel.para.knbstyle.arc
    label $mytoplevel.para.knbstyle.arc.lab -text [_ "Arc width: "]
    entry $mytoplevel.para.knbstyle.arc.ent -textvariable $var_iemgui_arc_width -width 5
    pack $mytoplevel.para.knbstyle.arc.ent $mytoplevel.para.knbstyle.arc.lab -side right -anchor e

    pack $mytoplevel.para.knbstyle.ticks $mytoplevel.para.knbstyle.dummy1 \
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

    set var_iemgui_l2_f1_b0 [concat iemgui_l2_f1_b0_$vid]
    global $var_iemgui_l2_f1_b0
    radiobutton $mytoplevel.colors.select.radio3 -value 3 -variable \
        $var_iemgui_l2_f1_b0 -text [_ "Arc"] -justify left
    pack $mytoplevel.colors.select.radio3 $mytoplevel.colors.select.radio2 -side left
    
    set var_iemgui_bcol [concat iemgui_bcol_$vid]
    global $var_iemgui_bcol
    set var_iemgui_fcol [concat iemgui_fcol_$vid]
    global $var_iemgui_fcol
    set var_iemgui_lcol [concat iemgui_lcol_$vid]
    global $var_iemgui_lcol
    set var_iemgui_acol [concat iemgui_acol_$vid]
    global $var_iemgui_acol
    set $var_iemgui_acol $arc_color
    $mytoplevel.colors.sections.exp.lb_bk configure -text [_ "Label"]
    label $mytoplevel.colors.sections.exp.arc_bk -text [_ "Arc"] \
        -background [eval concat $$var_iemgui_bcol] \
        -activebackground [eval concat $$var_iemgui_bcol] \
        -foreground [eval concat $$var_iemgui_lcol] \
        -activeforeground [eval concat $$var_iemgui_lcol] \
        -font [list $::font_family 14 $::font_weight] -padx 2 -pady 2 -relief ridge
    pack $mytoplevel.colors.sections.exp.arc_bk \
        -side right -anchor e -expand yes -fill both -pady 7
}

proc ::dialog_iemgui::apply_knb {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    set var_iemgui_ticks [concat iemgui_ticks_$vid]
    global $var_iemgui_ticks
    set var_iemgui_arc_width [concat iemgui_arc_width_$vid]
    global $var_iemgui_arc_width
    set var_iemgui_start_angle [concat iemgui_start_angle_$vid]
    global $var_iemgui_start_angle
    set var_iemgui_end_angle [concat iemgui_end_angle_$vid]
    global $var_iemgui_end_angle
    set var_iemgui_wdt [concat iemgui_wdt_$vid]
    global $var_iemgui_wdt
    set var_iemgui_acol [concat iemgui_acol_$vid]
    global $var_iemgui_acol

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

    set max_arc_width [expr ([eval concat $$var_iemgui_wdt] - 1) / 2]
    set min_arc_width [expr -(([eval concat $$var_iemgui_wdt] - 1) / 2 + 1)]
    if {[eval concat $$var_iemgui_arc_width] < $min_arc_width } {
        set $var_iemgui_arc_width $min_arc_width
    }
    if {[eval concat $$var_iemgui_arc_width] > $max_arc_width } {
        set $var_iemgui_arc_width $max_arc_width
    }

    return [list [eval concat $$var_iemgui_ticks] \
        [eval concat $$var_iemgui_acol] [eval concat $$var_iemgui_arc_width] \
        [eval concat $$var_iemgui_start_angle] [eval concat $$var_iemgui_end_angle]]
}

