# For information on usage and redistribution, and for a DISCLAIMER OF ALL
# WARRANTIES, see the file, "LICENSE.txt," in this distribution.
# Copyright (c) 1997-2009 Miller Puckette.

package provide dialog_iemgui 0.1

namespace eval ::dialog_iemgui:: {
    namespace export pdtk_iemgui_dialog
}
# some constants
set ::dialog_iemgui::min_flashhold 50
set ::dialog_iemgui::min_flashbreak 10
set ::dialog_iemgui::min_fontsize 4

# arrays to store per-dialog values
array set ::dialog_iemgui::var_width {} ;# var_iemgui_wdt
array set ::dialog_iemgui::var_height {} ;# var_iemgui_hgt
array set ::dialog_iemgui::var_minwidth {} ;# var_iemgui_min_wdt
array set ::dialog_iemgui::var_minheight {} ;# var_iemgui_min_hgt

array set ::dialog_iemgui::var_range_max {} ;# var_iemgui_max_rng
array set ::dialog_iemgui::var_range_min {} ;# var_iemgui_min_rng
array set ::dialog_iemgui::var_range_checkmode {} ;# var_iemgui_rng_sch

array set ::dialog_iemgui::var_mode {} ;# var_iemgui_lin0_log1
array set ::dialog_iemgui::var_loadbang {} ;# var_iemgui_loadbang
array set ::dialog_iemgui::var_steady {} ;# var_iemgui_steady
array set ::dialog_iemgui::var_number {} ;# var_iemgui_num

array set ::dialog_iemgui::var_snd {} ;# var_send
array set ::dialog_iemgui::var_rcv {} ;# var_receive
array set ::dialog_iemgui::var_label {} ;# var_iemgui_gui_nam

array set ::dialog_iemgui::var_label_dx {} ;# var_iemgui_gn_dx
array set ::dialog_iemgui::var_label_dy {} ;# var_iemgui_gn_dy
array set ::dialog_iemgui::var_label_font {} ;# var_iemgui_gn_f
array set ::dialog_iemgui::var_label_fontsize {} ;# var_iemgui_gn_fs

array set ::dialog_iemgui::var_color_background {} ;# var_iemgui_bcol
array set ::dialog_iemgui::var_color_foreground {} ;# var_iemgui_fcol
array set ::dialog_iemgui::var_color_label {} ;# var_iemgui_lcol
array set ::dialog_iemgui::var_colortype {} ;# var_iemgui_l2_f1_b0

# TODO convert Init/No Init and Steady on click/Jump on click to checkbuttons

proc ::dialog_iemgui::tonumber val {
    set x 0
    catch {
        set x [expr $val]
    }
    return $x
}

proc ::dialog_iemgui::clip {val min {max {}}} {
    set val [tonumber $val]
    if {$min ne {} && $val < $min} {return $min}
    if {$max ne {} && $val > $max} {return $max}
    return $val
}

proc ::dialog_iemgui::clip_dim {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    set ::dialog_iemgui::var_width($vid) [clip $::dialog_iemgui::var_width($vid) $::dialog_iemgui::var_minwidth($vid)]
    set ::dialog_iemgui::var_height($vid) [clip $::dialog_iemgui::var_height($vid) $::dialog_iemgui::var_minheight($vid)]
}

proc ::dialog_iemgui::clip_num {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    set ::dialog_iemgui::var_number($vid) [clip $::dialog_iemgui::var_number($vid) 1 2000]
}

proc ::dialog_iemgui::sched_rng {mytoplevel} {
    # TODO: rename this to 'range_check'
    set vid [string trimleft $mytoplevel .]
    switch -- $::dialog_iemgui::var_range_checkmode($vid) {
        2 {
            # 'range-check' is in 'flash' mode
            # make sure that min/max are sorted properly and are not smaller than the resp. min values
            foreach {flashbreak flashhold} [lsort -real [list [tonumber $::dialog_iemgui::var_range_min($vid)] [tonumber $::dialog_iemgui::var_range_max($vid)]]] {break;}
            set ::dialog_iemgui::var_range_min($vid) [clip $flashbreak $::dialog_iemgui::min_flashbreak]
            set ::dialog_iemgui::var_range_max($vid) [clip $flashhold $::dialog_iemgui::min_flashhold]
        }
        1 {
            # 'range-check' is in 'toggle' mode
            if {[tonumber $::dialog_iemgui::var_range_min($vid)] == 0} {
                #  there's little use toggling between 0 and 0, so force it to 1...
                set ::dialog_iemgui::var_range_min($vid) 1
            }
        }
    }
}

proc ::dialog_iemgui::verify_rng {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    if {$::dialog_iemgui::var_mode($vid) == 1} {
        if {$::dialog_iemgui::var_range_max($vid) == 0.0 && $::dialog_iemgui::var_range_min($vid) == 0.0} {
            set ::dialog_iemgui::var_range_max($vid) 1.0
        }
        if {$::dialog_iemgui::var_range_max($vid) > 0} {
            if {$::dialog_iemgui::var_range_min($vid) <= 0} {
                set ::dialog_iemgui::var_range_min($vid) [expr $::dialog_iemgui::var_range_max($vid) * 0.01]
            }
        } else {
            if {$::dialog_iemgui::var_range_min($vid) > 0} {
                set ::dialog_iemgui::var_range_max($vid) [expr $::dialog_iemgui::var_range_min($vid) * 0.01]
            }
        }
    }
}

proc ::dialog_iemgui::clip_fontsize {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    set ::dialog_iemgui::var_label_fontsize($vid) [clip $::dialog_iemgui::var_label_fontsize($vid) $::dialog_iemgui::min_fontsize]
}

proc ::dialog_iemgui::set_col_example {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    set fgcol $::dialog_iemgui::var_color_label($vid)
    $mytoplevel.colors.sections.exp.lb_bk configure \
        -background $::dialog_iemgui::var_color_background($vid) \
        -activebackground $::dialog_iemgui::var_color_background($vid) \
        -foreground $fgcol -activeforeground $fgcol

    set fgcol $::dialog_iemgui::var_color_foreground($vid)
    if { $fgcol eq "none" } {
        set fgcol $::dialog_iemgui::var_color_background($vid)
    }
    $mytoplevel.colors.sections.exp.fr_bk configure \
        -background $::dialog_iemgui::var_color_background($vid) \
        -activebackground $::dialog_iemgui::var_color_background($vid) \
        -foreground $fgcol -activeforeground $fgcol

    # for OSX live updates
    if {$::windowingsystem eq "aqua"} {
        ::dialog_iemgui::apply_and_rebind_return $mytoplevel
    }
}

proc ::dialog_iemgui::preset_col {mytoplevel presetcol} {
    set vid [string trimleft $mytoplevel .]

    switch -- $::dialog_iemgui::var_colortype($vid) {
        0 { set ::dialog_iemgui::var_color_background($vid) $presetcol }
        1 { set ::dialog_iemgui::var_color_foreground($vid) $presetcol }
        2 { set ::dialog_iemgui::var_color_label($vid) $presetcol }
    }

    ::dialog_iemgui::set_col_example $mytoplevel
}

proc ::dialog_iemgui::choose_col_bkfrlb {mytoplevel} {
    # TODO rename this
    set vid [string trimleft $mytoplevel .]

    switch -- $::dialog_iemgui::var_colortype($vid) {
        0 {
            set title [_ "Background color" ]
            set color $::dialog_iemgui::var_color_background($vid)
        }
        1 {
            set title [_ "Foreground color" ]
            set color $::dialog_iemgui::var_color_foreground($vid)
        }
        2 {
            set title [_ "Label color" ]
            set color $::dialog_iemgui::var_color_label($vid)
        }
    }
    set color [tk_chooseColor -title $title -initialcolor $color]
    if { $color ne "" } {
        ::dialog_iemgui::preset_col $color
    }
}

proc ::dialog_iemgui::popupmenu {path varname labels {command {}}} {
    upvar 1 $varname var

    menubutton ${path} -menu ${path}.menu -indicatoron 1 -relief raised -text [lindex $labels $var]
    menu ${path}.menu -tearoff 0
    set idx 0
    foreach l $labels {
        $path.menu add radiobutton -label "$l" -variable $varname -value $idx
        $path.menu entryconfigure last -command "\{$path\} configure -text \{$l\}; $command"
        incr idx
    }
}

proc ::dialog_iemgui::toggle_mode {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    if {$::dialog_iemgui::var_mode($vid) != 0} {
        ::dialog_iemgui::verify_rng $mytoplevel
        ::dialog_iemgui::sched_rng $mytoplevel
    }
}

# open popup over source button
proc ::dialog_iemgui::font_popup {mytoplevel} {
    $mytoplevel.popup unpost
    set button $mytoplevel.label.fontpopup_label
    set x [expr [winfo rootx $button] + ( [winfo width $button] / 2 )]
    set y [expr [winfo rooty $button] + ( [winfo height $button] / 2 )]
    tk_popup $mytoplevel.popup $x $y 0
}

proc ::dialog_iemgui::toggle_font {mytoplevel gn_f} {
    set vid [string trimleft $mytoplevel .]
    set ::dialog_iemgui::var_label_font($vid) $gn_f

    switch -- $gn_f {
        0 { set current_font $::font_family}
        1 { set current_font "Helvetica" }
        2 { set current_font "Times" }
    }
    set current_font_spec "{$current_font} 14 $::font_weight"

    $mytoplevel.label.fontpopup_label configure -text $current_font \
        -font [list $current_font 16 $::font_weight]
    $mytoplevel.label.name_entry configure -font $current_font_spec
    $mytoplevel.colors.sections.exp.fr_bk configure -font $current_font_spec
    $mytoplevel.colors.sections.exp.lb_bk configure -font $current_font_spec
}

proc ::dialog_iemgui::apply {mytoplevel} {
    set vid [string trimleft $mytoplevel .]

    ::dialog_iemgui::clip_dim $mytoplevel
    ::dialog_iemgui::clip_num $mytoplevel
    ::dialog_iemgui::sched_rng $mytoplevel
    ::dialog_iemgui::verify_rng $mytoplevel
    ::dialog_iemgui::sched_rng $mytoplevel
    ::dialog_iemgui::clip_fontsize $mytoplevel


    # TODO wrap the name-mangling ('empty', unspace_text, map) into a helper-proc
    set sendname empty
    set receivename empty
    set labelname empty

    if {$::dialog_iemgui::var_snd($vid) ne ""} {set sendname $::dialog_iemgui::var_snd($vid)}
    if {$::dialog_iemgui::var_rcv($vid) ne ""} {set receivename $::dialog_iemgui::var_rcv($vid)}
    if {$::dialog_iemgui::var_label($vid) ne ""} {set labelname $::dialog_iemgui::var_label($vid)}

    set labelname [string map { "\\" {\\} {$} {\$} { } {\ } {,} {\,} {;} {\;}  "{" "\{" "}" "\}" } $labelname]

    # make sure the offset boxes have a value
    if {$::dialog_iemgui::var_label_dx($vid) eq ""} {set ::dialog_iemgui::var_label_dx($vid) 0}
    if {$::dialog_iemgui::var_label_dy($vid) eq ""} {set ::dialog_iemgui::var_label_dy($vid) 0}

    pdsend [concat $mytoplevel dialog \
                $::dialog_iemgui::var_width($vid) \
                $::dialog_iemgui::var_height($vid) \
                $::dialog_iemgui::var_range_min($vid) \
                $::dialog_iemgui::var_range_max($vid) \
                $::dialog_iemgui::var_mode($vid) \
                $::dialog_iemgui::var_loadbang($vid) \
                $::dialog_iemgui::var_number($vid) \
                [string map {"$" {\$}} [unspace_text $sendname]] \
                [string map {"$" {\$}} [unspace_text $receivename]] \
                $labelname \
                $::dialog_iemgui::var_label_dx($vid) \
                $::dialog_iemgui::var_label_dy($vid) \
                $::dialog_iemgui::var_label_font($vid) \
                $::dialog_iemgui::var_label_fontsize($vid) \
                [string tolower $::dialog_iemgui::var_color_background($vid)] \
                [string tolower $::dialog_iemgui::var_color_foreground($vid)] \
                [string tolower $::dialog_iemgui::var_color_label($vid)] \
                $::dialog_iemgui::var_steady($vid) \
               ]
}


proc ::dialog_iemgui::cancel {mytoplevel} {
    pdsend "$mytoplevel cancel"
}

proc ::dialog_iemgui::ok {mytoplevel} {
    ::dialog_iemgui::apply $mytoplevel
    ::dialog_iemgui::cancel $mytoplevel
}

proc ::dialog_iemgui::pdtk_iemgui_dialog {mytoplevel mainheader dim_header_UNUSED \
                                       wdt min_wdt label_width \
                                       hgt min_hgt label_height \
                                       label_range min_rng label_range_min max_rng \
                                       label_range_max rng_sched \
                                       lin0_log1 lilo0_label lilo1_label \
                                       loadbang steady label_number num \
                                       snd rcv \
                                       gui_name \
                                       gn_dx gn_dy gn_f gn_fs \
                                       bcol fcol lcol} {

    set vid [string trimleft $mytoplevel .]
    set snd [::pdtk_text::unescape $snd]
    set rcv [::pdtk_text::unescape $rcv]
    set gui_name [::pdtk_text::unescape $gui_name]

    # initialize the array
    set ::dialog_iemgui::var_width($vid) $wdt
    set ::dialog_iemgui::var_height($vid) $hgt
    set ::dialog_iemgui::var_minwidth($vid) $min_wdt
    set ::dialog_iemgui::var_minheight($vid) $min_hgt

    set ::dialog_iemgui::var_range_max($vid) $max_rng
    set ::dialog_iemgui::var_range_min($vid) $min_rng
    set ::dialog_iemgui::var_range_checkmode($vid) $rng_sched

    set ::dialog_iemgui::var_mode($vid) $lin0_log1
    set ::dialog_iemgui::var_loadbang($vid) $loadbang
    set ::dialog_iemgui::var_steady($vid) $steady
    set ::dialog_iemgui::var_number($vid) $num

    set ::dialog_iemgui::var_snd($vid) $snd
    set ::dialog_iemgui::var_rcv($vid) $rcv
    set ::dialog_iemgui::var_label($vid) $gui_name

    set ::dialog_iemgui::var_label_dx($vid) $gn_dx
    set ::dialog_iemgui::var_label_dy($vid) $gn_dy
    set ::dialog_iemgui::var_label_font($vid) $gn_f
    set ::dialog_iemgui::var_label_fontsize($vid) $gn_fs

    set ::dialog_iemgui::var_color_background($vid) $bcol
    set ::dialog_iemgui::var_color_foreground($vid) $fcol
    set ::dialog_iemgui::var_color_label($vid) $lcol
    set ::dialog_iemgui::var_colortype($vid) 0

    # Override incoming values for known iem guis.
    set iemgui_type [_ $mainheader]

    switch -- $mainheader {
        "|bang|" {
            set iemgui_type [_ "Bang"]
            set label_width "Size:"
            set label_range [_ "Flash Time (msec)"]
            set label_range_min [_ "Min:"]
            set label_range_max [_ "Max:"] }
        "|tgl|" {
            set iemgui_type [_ "Toggle"]
            set label_width [_ "Size:"]
            set label_range [_ "Non Zero Value"]
            set label_range_min [_ "Value:"] }
        "|nbx|" {
            set iemgui_type [_ "Number2"]
            set label_width [_ "Width (digits):"]
            set label_height [_ "Height:"]
            set label_range [_ "Output Range"]
            set label_range_min [_ "Lower:"]
            set label_range_max [_ "Upper:"]
            set label_number [_ "Log height:"] }
        "|vsl|" {
            set iemgui_type [_ "Vslider"]
            set label_width [_ "Width:"]
            set label_height [_ "Height:"]
            set label_range [_ "Output Range"]
            set label_range_min [_ "Lower:"]
            set label_range_max [_ "Upper:"] }
        "|hsl|" {
            set iemgui_type [_ "Hslider"]
            set label_width [_ "Width:"]
            set label_height [_ "Height:"]
            set label_range [_ "Output Range"]
            set label_range_min [_ "Lower:"]
            set label_range_max [_ "Upper:"] }
        "|vradio|" {
            set iemgui_type [_ "Vradio"]
            set label_width [_ "Size:"]
            set label_number [_ "Num cells:"] }
        "|hradio|" {
            set iemgui_type [_ "Hradio"]
            set label_width [_ "Size:"]
            set label_number [_ "Num cells:"] }
        "|vu|" {
            set ::dialog_iemgui::var_color_foreground($vid) none
            set iemgui_type [_ "VU Meter"]
            set label_width [_ "Width:"]
            set label_height [_ "Height:"] }
        "|cnv|" {
            set ::dialog_iemgui::var_color_foreground($vid) none
            set iemgui_type [_ "Canvas"]
            set label_width [_ "Size:"]
            set label_range [_ "Visible Rectangle (pix)"]
            set label_range_min [_ "Width:"]
            set label_range_max [_ "Height:"] }
    }

    toplevel $mytoplevel -class DialogWindow
    wm title $mytoplevel [format [_ "%s Properties"] $iemgui_type]
    wm group $mytoplevel .
    wm resizable $mytoplevel 0 0
    wm transient $mytoplevel $::focused_window
    $mytoplevel configure -menu $::dialog_menubar
    $mytoplevel configure -padx 0 -pady 0
    ::pd_bindings::dialog_bindings $mytoplevel "iemgui"

    # dimensions
    frame $mytoplevel.dim -height 7
    pack $mytoplevel.dim -side top
    label $mytoplevel.dim.w_lab -text [_ $label_width]
    entry $mytoplevel.dim.w_ent -textvariable ::dialog_iemgui::var_width($vid) -width 4
    label $mytoplevel.dim.dummy1 -text "" -width 1
    label $mytoplevel.dim.h_lab -text [_ $label_height]
    entry $mytoplevel.dim.h_ent -textvariable ::dialog_iemgui::var_height($vid) -width 4
    pack $mytoplevel.dim.w_lab $mytoplevel.dim.w_ent -side left
    if { $label_height ne "" } {
        pack $mytoplevel.dim.dummy1 $mytoplevel.dim.h_lab $mytoplevel.dim.h_ent -side left }

    # range
    labelframe $mytoplevel.rng
    pack $mytoplevel.rng -side top -fill x
    frame $mytoplevel.rng.min
    label $mytoplevel.rng.min.lab -text $label_range_min
    entry $mytoplevel.rng.min.ent -textvariable ::dialog_iemgui::var_range_min($vid) -width 7
    label $mytoplevel.rng.dummy1 -text "" -width 1
    label $mytoplevel.rng.max_lab -text [_ $label_range_max]
    entry $mytoplevel.rng.max_ent -textvariable ::dialog_iemgui::var_range_max($vid) -width 7
    if { $label_range ne "" } {
        $mytoplevel.rng config -borderwidth 1 -pady 4 -text [_ $label_range]
        if { $label_range_min ne "" } {
            pack $mytoplevel.rng.min
            pack $mytoplevel.rng.min.lab $mytoplevel.rng.min.ent -side left }
        if { $label_range_max ne "" } {
            $mytoplevel.rng config -padx 26
            pack configure $mytoplevel.rng.min -side left
            pack $mytoplevel.rng.dummy1 $mytoplevel.rng.max_lab $mytoplevel.rng.max_ent -side left}
    }

    # parameters
    labelframe $mytoplevel.para -borderwidth 1 -padx 5 -pady 5 -text [_ "Parameters"]
    pack $mytoplevel.para -side top -fill x -pady 5

    frame $mytoplevel.para.num
    label $mytoplevel.para.num.lab -text [_ $label_number]
    entry $mytoplevel.para.num.ent -textvariable ::dialog_iemgui::var_number($vid) -width 4
    pack $mytoplevel.para.num.ent $mytoplevel.para.num.lab -side right -anchor e

    set applycmd ""
    if {$::windowingsystem eq "aqua"} {
        set applycmd "::dialog_iemgui::apply $mytoplevel"
    }


    if {$::dialog_iemgui::var_mode($vid) >= 0} {
        ::dialog_iemgui::popupmenu $mytoplevel.para.lilo \
            ::dialog_iemgui::var_mode($vid) [list [_ $lilo0_label] [_ $lilo1_label] ] \
            "::dialog_iemgui::toggle_mode $mytoplevel; $applycmd"
        pack $mytoplevel.para.lilo -side left -expand 1 -ipadx 10
    }
    if {$::dialog_iemgui::var_loadbang($vid) >= 0} {
        ::dialog_iemgui::popupmenu $mytoplevel.para.lb \
            ::dialog_iemgui::var_loadbang($vid) [list [_ "No init"] [_ "Init"] ] \
            $applycmd
        pack $mytoplevel.para.lb -side left -expand 1 -ipadx 10
    }
    if {$::dialog_iemgui::var_number($vid) > 0} {
        pack $mytoplevel.para.num -side left -expand 1 -ipadx 10
    }
    if {$::dialog_iemgui::var_steady($vid) >= 0} {
        ::dialog_iemgui::popupmenu $mytoplevel.para.stdy_jmp \
            ::dialog_iemgui::var_steady($vid) [list [_ "Jump on click"] [_ "Steady on click"] ] \
            $applycmd
        pack $mytoplevel.para.stdy_jmp -side left -expand 1 -ipadx 10
    }

    # messages
    labelframe $mytoplevel.s_r -borderwidth 1 -padx 5 -pady 5 -text [_ "Messages"]
    pack $mytoplevel.s_r -side top -fill x
    frame $mytoplevel.s_r.send
    pack $mytoplevel.s_r.send -side top -anchor e -padx 5
    label $mytoplevel.s_r.send.lab -text [_ "Send symbol:"]
    entry $mytoplevel.s_r.send.ent -textvariable ::dialog_iemgui::var_snd($vid) -width 21
    if { $snd ne "nosndno" } {
        pack $mytoplevel.s_r.send.lab $mytoplevel.s_r.send.ent -side left \
            -fill x -expand 1
    }

    frame $mytoplevel.s_r.receive
    pack $mytoplevel.s_r.receive -side top -anchor e -padx 5
    label $mytoplevel.s_r.receive.lab -text [_ "Receive symbol:"]
    entry $mytoplevel.s_r.receive.ent -textvariable ::dialog_iemgui::var_rcv($vid) -width 21
    if { $rcv ne "norcvno" } {
        pack $mytoplevel.s_r.receive.lab $mytoplevel.s_r.receive.ent -side left \
            -fill x -expand 1
    }

    # get the current font name from the int given from C-space (gn_f)
    set current_font $::font_family
    if {$::dialog_iemgui::var_label_font($vid) == 1} \
        { set current_font "Helvetica" }
    if {$::dialog_iemgui::var_label_font($vid) == 2} \
        { set current_font "Times" }

    # label
    labelframe $mytoplevel.label -borderwidth 1 -text [_ "Label"] -padx 5 -pady 5
    pack $mytoplevel.label -side top -fill x -pady 5
    entry $mytoplevel.label.name_entry -textvariable ::dialog_iemgui::var_label($vid) \
        -width 30 -font [list $current_font 14 $::font_weight]
    pack $mytoplevel.label.name_entry -side top -fill both -padx 5

    frame $mytoplevel.label.xy -padx 20 -pady 1
    pack $mytoplevel.label.xy -side top
    label $mytoplevel.label.xy.x_lab -text [_ "X offset:"]
    entry $mytoplevel.label.xy.x_entry -textvariable ::dialog_iemgui::var_label_dx($vid) -width 5
    label $mytoplevel.label.xy.dummy1 -text " " -width 1
    label $mytoplevel.label.xy.y_lab -text [_ "Y offset:"]
    entry $mytoplevel.label.xy.y_entry -textvariable ::dialog_iemgui::var_label_dy($vid) -width 5
    pack $mytoplevel.label.xy.x_lab $mytoplevel.label.xy.x_entry $mytoplevel.label.xy.dummy1 \
        $mytoplevel.label.xy.y_lab $mytoplevel.label.xy.y_entry -side left

    button $mytoplevel.label.fontpopup_label -text $current_font \
        -font [list $current_font 16 $::font_weight] -pady 4 \
        -command "::dialog_iemgui::font_popup $mytoplevel"
    pack $mytoplevel.label.fontpopup_label -side left -anchor w \
        -expand 1 -fill x -padx 5
    frame $mytoplevel.label.fontsize
    pack $mytoplevel.label.fontsize -side right -padx 5 -pady 5
    label $mytoplevel.label.fontsize.label -text [_ "Size:"]
    entry $mytoplevel.label.fontsize.entry -textvariable ::dialog_iemgui::var_label_fontsize($vid) -width 4
    pack $mytoplevel.label.fontsize.entry $mytoplevel.label.fontsize.label \
        -side right -anchor e
    menu $mytoplevel.popup
    $mytoplevel.popup add command \
        -label $::font_family \
        -font [format {{%s} 16 %s} $::font_family $::font_weight] \
        -command "::dialog_iemgui::toggle_font $mytoplevel 0"
    $mytoplevel.popup add command \
        -label "Helvetica" \
        -font [format {Helvetica 16 %s} $::font_weight] \
        -command "::dialog_iemgui::toggle_font $mytoplevel 1"
    $mytoplevel.popup add command \
        -label "Times" \
        -font [format {Times 16 %s} $::font_weight] \
        -command "::dialog_iemgui::toggle_font $mytoplevel 2"

    # colors
    labelframe $mytoplevel.colors -borderwidth 1 -text [_ "Colors"] -padx 5 -pady 5
    pack $mytoplevel.colors -fill x

    frame $mytoplevel.colors.select
    pack $mytoplevel.colors.select -side top
    radiobutton $mytoplevel.colors.select.radio0 \
        -value 0 -variable ::dialog_iemgui::var_colortype($vid) \
        -text [_ "Background"] -justify left
    radiobutton $mytoplevel.colors.select.radio1 \
        -value 1 -variable ::dialog_iemgui::var_colortype($vid) \
        -text [_ "Front"] -justify left
    radiobutton $mytoplevel.colors.select.radio2 \
        -value 2 -variable ::dialog_iemgui::var_colortype($vid) \
        -text [_ "Label"] -justify left
    if { $::dialog_iemgui::var_color_foreground($vid) ne "none" } {
        pack $mytoplevel.colors.select.radio0 $mytoplevel.colors.select.radio1 \
            $mytoplevel.colors.select.radio2 -side left
    } else {
        pack $mytoplevel.colors.select.radio0 $mytoplevel.colors.select.radio2 -side left
    }

    frame $mytoplevel.colors.sections
    pack $mytoplevel.colors.sections -side top
    button $mytoplevel.colors.sections.but -text [_ "Compose color"] \
        -command "::dialog_iemgui::choose_col_bkfrlb $mytoplevel"
    pack $mytoplevel.colors.sections.but -side left -anchor w -pady 5 \
        -expand yes -fill x
    frame $mytoplevel.colors.sections.exp
    pack $mytoplevel.colors.sections.exp -side right -padx 5
    if { $::dialog_iemgui::var_color_foreground($vid) ne "none" } {
        label $mytoplevel.colors.sections.exp.fr_bk -text "o=||=o" -width 6 \
            -background $::dialog_iemgui::var_color_background($vid) \
            -activebackground $::dialog_iemgui::var_color_background($vid) \
            -foreground $::dialog_iemgui::var_color_foreground($vid) \
            -activeforeground $::dialog_iemgui::var_color_foreground($vid) \
            -font [list $current_font 14 $::font_weight] -padx 2 -pady 2 -relief ridge
    } else {
        label $mytoplevel.colors.sections.exp.fr_bk -text "o=||=o" -width 6 \
            -background $::dialog_iemgui::var_color_background($vid) \
            -activebackground $::dialog_iemgui::var_color_background($vid) \
            -foreground $::dialog_iemgui::var_color_background($vid) \
            -activeforeground $::dialog_iemgui::var_color_background($vid) \
            -font [list $current_font 14 $::font_weight] -padx 2 -pady 2 -relief ridge
    }
    label $mytoplevel.colors.sections.exp.lb_bk -text [_ "Test label"] \
        -background $::dialog_iemgui::var_color_background($vid) \
        -activebackground $::dialog_iemgui::var_color_background($vid) \
        -foreground $::dialog_iemgui::var_color_label($vid) \
        -activeforeground $::dialog_iemgui::var_color_label($vid) \
        -font [list $current_font 14 $::font_weight] -padx 2 -pady 2 -relief ridge
    pack $mytoplevel.colors.sections.exp.lb_bk $mytoplevel.colors.sections.exp.fr_bk \
        -side right -anchor e -expand yes -fill both -pady 7

    # color scheme by Mary Ann Benedetto http://piR2.org
    foreach r {r1 r2 r3} hexcols {
       { "#FFFFFF" "#DFDFDF" "#BBBBBB" "#FFC7C6" "#FFE3C6" "#FEFFC6" "#C6FFC7" "#C6FEFF" "#C7C6FF" "#E3C6FF" }
       { "#9F9F9F" "#7C7C7C" "#606060" "#FF0400" "#FF8300" "#FAFF00" "#00FF04" "#00FAFF" "#0400FF" "#9C00FF" }
       { "#404040" "#202020" "#000000" "#551312" "#553512" "#535512" "#0F4710" "#0E4345" "#131255" "#2F004D" } } \
    {
       frame $mytoplevel.colors.$r
       pack $mytoplevel.colors.$r -side top
       foreach i { 0 1 2 3 4 5 6 7 8 9} hexcol $hexcols \
           {
               label $mytoplevel.colors.$r.c$i -background $hexcol -activebackground $hexcol -relief ridge -padx 7 -pady 0 -width 1
               bind $mytoplevel.colors.$r.c$i <Button> "::dialog_iemgui::preset_col $mytoplevel $hexcol"
           }
       pack $mytoplevel.colors.$r.c0 $mytoplevel.colors.$r.c1 $mytoplevel.colors.$r.c2 $mytoplevel.colors.$r.c3 \
           $mytoplevel.colors.$r.c4 $mytoplevel.colors.$r.c5 $mytoplevel.colors.$r.c6 $mytoplevel.colors.$r.c7 \
           $mytoplevel.colors.$r.c8 $mytoplevel.colors.$r.c9 -side left
    }

    # buttons
    frame $mytoplevel.cao -pady 10
    pack $mytoplevel.cao -side top
    button $mytoplevel.cao.cancel -text [_ "Cancel"] \
        -command "::dialog_iemgui::cancel $mytoplevel"
    pack $mytoplevel.cao.cancel -side left -expand 1 -fill x -padx 15 -ipadx 10
    if {$::windowingsystem ne "aqua"} {
        button $mytoplevel.cao.apply -text [_ "Apply"] \
            -command "::dialog_iemgui::apply $mytoplevel"
        pack $mytoplevel.cao.apply -side left -expand 1 -fill x -padx 15 -ipadx 10
    }
    button $mytoplevel.cao.ok -text [_ "OK"] \
        -command "::dialog_iemgui::ok $mytoplevel" -default active
    pack $mytoplevel.cao.ok -side left -expand 1 -fill x -padx 15 -ipadx 10

    $mytoplevel.dim.w_ent select from 0
    $mytoplevel.dim.w_ent select adjust end
    focus $mytoplevel.dim.w_ent

    # live widget updates on OSX in lieu of Apply button
    if {$::windowingsystem eq "aqua"} {

        # call apply on Return in entry boxes that are in focus & rebind Return to ok button
        bind $mytoplevel.dim.w_ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.dim.h_ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.rng.min.ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.rng.max_ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.para.num.ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.label.name_entry <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.s_r.send.ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.s_r.receive.ent <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.label.xy.x_entry <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.label.xy.y_entry <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"
        bind $mytoplevel.label.fontsize.entry <KeyPress-Return> "::dialog_iemgui::apply_and_rebind_return $mytoplevel"

        # unbind Return from ok button when an entry takes focus
        $mytoplevel.dim.w_ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.dim.h_ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.rng.min.ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.rng.max_ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.para.num.ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.label.name_entry config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.s_r.send.ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.s_r.receive.ent config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.label.xy.x_entry config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.label.xy.y_entry config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"
        $mytoplevel.label.fontsize.entry config -validate focusin -vcmd "::dialog_iemgui::unbind_return $mytoplevel"

        # remove cancel button from focus list since it's not activated on Return
        $mytoplevel.cao.cancel config -takefocus 0

        # show active focus on the ok button as it *is* activated on Return
        $mytoplevel.cao.ok config -default normal
        bind $mytoplevel.cao.ok <FocusIn> "$mytoplevel.cao.ok config -default active"
        bind $mytoplevel.cao.ok <FocusOut> "$mytoplevel.cao.ok config -default normal"

        # since we show the active focus, disable the highlight outline
        $mytoplevel.cao.ok config -highlightthickness 0
        $mytoplevel.cao.cancel config -highlightthickness 0
    }

    position_over_window $mytoplevel $::focused_window
}

# for live widget updates on OSX
proc ::dialog_iemgui::apply_and_rebind_return {mytoplevel} {
    ::dialog_iemgui::apply $mytoplevel
    bind $mytoplevel <KeyPress-Return> "::dialog_iemgui::ok $mytoplevel"
    focus $mytoplevel.cao.ok
    return 0
}

# for live widget updates on OSX
proc ::dialog_iemgui::unbind_return {mytoplevel} {
    bind $mytoplevel <KeyPress-Return> break
    return 1
}
