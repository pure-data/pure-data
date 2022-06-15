# For information on usage and redistribution, and for a DISCLAIMER OF ALL
# WARRANTIES, see the file, "LICENSE.txt," in this distribution.
# Copyright (c) 2020 Antoine Rousseau.

namespace eval ::dialog_iemgui_knb:: {}

array set ::dialog_iemgui_knb::var_color_arc {} ;
array set ::dialog_iemgui_knb::var_ticks {} ;
array set ::dialog_iemgui_knb::var_arc_width {} ;
array set ::dialog_iemgui_knb::var_start_angle {} ;
array set ::dialog_iemgui_knb::var_end_angle {} ;

proc ::dialog_iemgui_knb::preset_col {mytoplevel presetcol} {
    set vid [string trimleft $mytoplevel .]

    if { $::dialog_iemgui::var_colortype($vid) == 3 } {
        set ::dialog_iemgui_knb::var_color_arc($vid) $presetcol
    }
}

proc ::dialog_iemgui_knb::choose_col {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    if {$::dialog_iemgui::var_colortype($vid) == 3} {
        set helpstring [tk_chooseColor -title [_ "Arc color"] -initialcolor $::dialog_iemgui_knb::var_color_arc($vid)]
        if { $helpstring ne "" } {
            set ::dialog_iemgui_knb::var_color_arc($vid) $helpstring
        }
    }
}

proc ::dialog_iemgui_knb::set_col_example {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    $mytoplevel.colors.sections.exp.arc_bk configure \
        -background $::dialog_iemgui::var_color_background($vid) \
        -activebackground $::dialog_iemgui::var_color_background($vid) \
        -foreground $::dialog_iemgui_knb::var_color_arc($vid) \
        -activeforeground $::dialog_iemgui_knb::var_color_arc($vid)
}

proc ::dialog_iemgui_knb::toggle_font {mytoplevel current_font_spec} {
    $mytoplevel.colors.sections.exp.arc_bk configure -font $current_font_spec
}

proc ::dialog_iemgui_knb::create_properties {mytoplevel current_font ticks arc_color arc_width \
        start_angle end_angle} {
    set vid [string trimleft $mytoplevel .]

    set ::dialog_iemgui_knb::var_ticks($vid) $ticks
    set ::dialog_iemgui_knb::var_color_arc($vid) $arc_color
    set ::dialog_iemgui_knb::var_arc_width($vid) $arc_width
    set ::dialog_iemgui_knb::var_start_angle($vid) $start_angle
    set ::dialog_iemgui_knb::var_end_angle($vid) $end_angle

    $mytoplevel.dim.h_lab configure -text [_ "Sensitivity:"]
    set applycmd ""
    if {$::windowingsystem eq "aqua"} {
        set applycmd "::dialog_iemgui::apply $mytoplevel"
    }
    destroy $mytoplevel.para.std.stdy_jmp
    label $mytoplevel.para.std.move_label -text [_ "Move:"]
    ::dialog_iemgui::popupmenu $mytoplevel.para.std.move_mode \
        ::dialog_iemgui::var_steady($vid) [list [_ "X"] [_ "Y"] [_ "X+Y"] [_ "Angle"] ] \
        $applycmd
    pack $mytoplevel.para.std.move_label -side left -expand 0 -ipadx 4
    pack $mytoplevel.para.std.move_mode -side left -expand 0 -ipadx 10

    $mytoplevel.rng.min.lab configure -text [_ "Lower:"]
    $mytoplevel.rng.max_lab configure -text [_ "Upper:"]
    # style
    frame $mytoplevel.para.knbstyle -padx 20 -pady 1

    frame $mytoplevel.para.knbstyle.ticks
    label $mytoplevel.para.knbstyle.ticks.lab -text [_ "Ticks: "]
    entry $mytoplevel.para.knbstyle.ticks.ent -textvariable ::dialog_iemgui_knb::var_ticks($vid) -width 5
    pack $mytoplevel.para.knbstyle.ticks.ent $mytoplevel.para.knbstyle.ticks.lab -side right -anchor e

    frame $mytoplevel.para.knbstyle.arc
    label $mytoplevel.para.knbstyle.arc.lab -text [_ "Arc thickness: "]
    entry $mytoplevel.para.knbstyle.arc.ent -textvariable ::dialog_iemgui_knb::var_arc_width($vid) -width 5
    pack $mytoplevel.para.knbstyle.arc.ent $mytoplevel.para.knbstyle.arc.lab -side right -anchor e

    frame $mytoplevel.para.knbstyle.start
    label $mytoplevel.para.knbstyle.start.lab -text [_ "Start angle: "]
    entry $mytoplevel.para.knbstyle.start.ent -textvariable ::dialog_iemgui_knb::var_start_angle($vid) -width 5
    pack $mytoplevel.para.knbstyle.start.ent $mytoplevel.para.knbstyle.start.lab -side right -anchor e

    frame $mytoplevel.para.knbstyle.end
    label $mytoplevel.para.knbstyle.end.lab -text [_ "End angle: "]
    entry $mytoplevel.para.knbstyle.end.ent -textvariable ::dialog_iemgui_knb::var_end_angle($vid) -width 5
    pack $mytoplevel.para.knbstyle.end.ent $mytoplevel.para.knbstyle.end.lab -side right -anchor w

    grid $mytoplevel.para.knbstyle.ticks -row 0 -column 0 -sticky e
    grid $mytoplevel.para.knbstyle.arc -row 1 -column 0 -sticky e
    grid $mytoplevel.para.knbstyle.start -row 0 -column 1 -sticky e -padx {20 0}
    grid $mytoplevel.para.knbstyle.end -row 1 -column 1 -sticky e -padx {20 0}

    pack $mytoplevel.para.knbstyle -side top -fill x

    # arc color
    radiobutton $mytoplevel.colors.select.radio3 -value 3 -variable \
        ::dialog_iemgui::var_colortype($vid) -text [_ "Arc"] -justify left
    pack $mytoplevel.colors.select.radio3 $mytoplevel.colors.select.radio2 -side left

    $mytoplevel.colors.sections.exp.lb_bk configure -text [_ "Label"]
    label $mytoplevel.colors.sections.exp.arc_bk -text [_ "Arc"] \
        -background $::dialog_iemgui::var_color_background($vid) \
        -activebackground $::dialog_iemgui::var_color_background($vid) \
        -foreground $::dialog_iemgui_knb::var_color_arc($vid) \
        -activeforeground $::dialog_iemgui_knb::var_color_arc($vid) \
        -font [list $current_font 14 $::font_weight] -padx 2 -pady 2 -relief ridge
    pack $mytoplevel.colors.sections.exp.arc_bk \
        -side right -anchor e -expand yes -fill both -pady 7 \
        -before $mytoplevel.colors.sections.exp.lb_bk

    # live widget updates on OSX in lieu of Apply button
    if {$::windowingsystem eq "aqua"} {

        # call apply on Return in entry boxes that are in focus & rebind Return to ok button
        bind $mytoplevel.para.knbstyle.ticks.ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.para.knbstyle.arc.ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.para.knbstyle.start.ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.para.knbstyle.end.ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"

        # unbind Return from ok button when an entry takes focus
        $mytoplevel.para.knbstyle.ticks.ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.para.knbstyle.arc.ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.para.knbstyle.start.ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.para.knbstyle.end.ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
    }
}

proc ::dialog_iemgui_knb::apply {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    set ::dialog_iemgui_knb::var_end_angle($vid) \
        [::dialog_iemgui::clip $::dialog_iemgui_knb::var_end_angle($vid) -360 360]
    set ::dialog_iemgui_knb::var_start_angle($vid) \
        [::dialog_iemgui::clip $::dialog_iemgui_knb::var_start_angle($vid) -360 360]

    if {$::dialog_iemgui_knb::var_end_angle($vid) < $::dialog_iemgui_knb::var_start_angle($vid) } {
        set endangle $::dialog_iemgui_knb::var_end_angle($vid)
        set ::dialog_iemgui_knb::var_end_angle($vid) $::dialog_iemgui_knb::var_start_angle($vid)
        set ::dialog_iemgui_knb::var_start_angle($vid) $endangle
    }

    if {$::dialog_iemgui_knb::var_end_angle($vid) - $::dialog_iemgui_knb::var_start_angle($vid) > 360 } {
        set ::dialog_iemgui_knb::var_end_angle($vid) [expr $::dialog_iemgui_knb::var_start_angle($vid) + 360]
    }

    if {$::dialog_iemgui_knb::var_end_angle($vid) == $::dialog_iemgui_knb::var_start_angle($vid)} {
        set ::dialog_iemgui_knb::var_end_angle($vid) [expr $::dialog_iemgui_knb::var_start_angle($vid) + 1]
    }

    set ::dialog_iemgui_knb::var_ticks($vid) [::dialog_iemgui::clip $::dialog_iemgui_knb::var_ticks($vid) 0 360]

    set max_arc_width [expr ($::dialog_iemgui::var_width($vid) - 1) / 2]
    set min_arc_width [expr -(($::dialog_iemgui::var_width($vid) - 1) / 2 + 1)]
    set ::dialog_iemgui_knb::var_arc_width($vid) \
        [::dialog_iemgui::clip $::dialog_iemgui_knb::var_arc_width($vid) $min_arc_width $max_arc_width]

    return [list $::dialog_iemgui_knb::var_ticks($vid) \
        $::dialog_iemgui_knb::var_color_arc($vid) $::dialog_iemgui_knb::var_arc_width($vid) \
        $::dialog_iemgui_knb::var_start_angle($vid) $::dialog_iemgui_knb::var_end_angle($vid) \
    ]
}
