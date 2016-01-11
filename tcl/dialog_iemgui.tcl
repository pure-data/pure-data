# For information on usage and redistribution, and for a DISCLAIMER OF ALL
# WARRANTIES, see the file, "LICENSE.txt," in this distribution.
# Copyright (c) 1997-2009 Miller Puckette.

package provide dialog_iemgui 0.1

namespace eval ::dialog_iemgui:: {
    variable define_min_flashhold 50
    variable define_min_flashbreak 10
    variable define_min_fontsize 4
    
    namespace export pdtk_iemgui_dialog
}

# TODO convert Init/No Init and Steady on click/Jump on click to checkbuttons

proc ::dialog_iemgui::clip_dim {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_wdt [concat iemgui_wdt_$vid]
    global $var_iemgui_wdt
    set var_iemgui_min_wdt [concat iemgui_min_wdt_$vid]
    global $var_iemgui_min_wdt
    set var_iemgui_hgt [concat iemgui_hgt_$vid]
    global $var_iemgui_hgt
    set var_iemgui_min_hgt [concat iemgui_min_hgt_$vid]
    global $var_iemgui_min_hgt
    
    if {[eval concat $$var_iemgui_wdt] < [eval concat $$var_iemgui_min_wdt]} {
        set $var_iemgui_wdt [eval concat $$var_iemgui_min_wdt]
        $mytoplevel.dim.w_ent configure -textvariable $var_iemgui_wdt
    }
    if {[eval concat $$var_iemgui_hgt] < [eval concat $$var_iemgui_min_hgt]} {
        set $var_iemgui_hgt [eval concat $$var_iemgui_min_hgt]
        $mytoplevel.dim.h_ent configure -textvariable $var_iemgui_hgt
    }
}

proc ::dialog_iemgui::clip_num {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_num [concat iemgui_num_$vid]
    global $var_iemgui_num
    
    if {[eval concat $$var_iemgui_num] > 2000} {
        set $var_iemgui_num 2000
        $mytoplevel.para.num.ent configure -textvariable $var_iemgui_num
    }
    if {[eval concat $$var_iemgui_num] < 1} {
        set $var_iemgui_num 1
        $mytoplevel.para.num.ent configure -textvariable $var_iemgui_num
    }
}

proc ::dialog_iemgui::sched_rng {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_min_rng [concat iemgui_min_rng_$vid]
    global $var_iemgui_min_rng
    set var_iemgui_max_rng [concat iemgui_max_rng_$vid]
    global $var_iemgui_max_rng
    set var_iemgui_rng_sch [concat iemgui_rng_sch_$vid]
    global $var_iemgui_rng_sch
    
    variable define_min_flashhold
    variable define_min_flashbreak
    
    if {[eval concat $$var_iemgui_rng_sch] == 2} {
        if {[eval concat $$var_iemgui_max_rng] < [eval concat $$var_iemgui_min_rng]} {
            set hhh [eval concat $$var_iemgui_min_rng]
            set $var_iemgui_min_rng [eval concat $$var_iemgui_max_rng]
            set $var_iemgui_max_rng $hhh
            $mytoplevel.rng.max_ent configure -textvariable $var_iemgui_max_rng
            $mytoplevel.rng.min.ent configure -textvariable $var_iemgui_min_rng }
        if {[eval concat $$var_iemgui_max_rng] < $define_min_flashhold} {
            set $var_iemgui_max_rng $define_min_flashhold
            $mytoplevel.rng.max_ent configure -textvariable $var_iemgui_max_rng
        }
        if {[eval concat $$var_iemgui_min_rng] < $define_min_flashbreak} {
            set $var_iemgui_min_rng $define_min_flashbreak
            $mytoplevel.rng.min.ent configure -textvariable $var_iemgui_min_rng
        }
    }
    if {[eval concat $$var_iemgui_rng_sch] == 1} {
        if {[eval concat $$var_iemgui_min_rng] == 0.0} {
            set $var_iemgui_min_rng 1.0
            $mytoplevel.rng.min.ent configure -textvariable $var_iemgui_min_rng
        }
    }
}

proc ::dialog_iemgui::verify_rng {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_min_rng [concat iemgui_min_rng_$vid]
    global $var_iemgui_min_rng
    set var_iemgui_max_rng [concat iemgui_max_rng_$vid]
    global $var_iemgui_max_rng
    set var_iemgui_lin0_log1 [concat iemgui_lin0_log1_$vid]
    global $var_iemgui_lin0_log1
    
    if {[eval concat $$var_iemgui_lin0_log1] == 1} {
        if {[eval concat $$var_iemgui_max_rng] == 0.0 && [eval concat $$var_iemgui_min_rng] == 0.0} {
            set $var_iemgui_max_rng 1.0
            $mytoplevel.rng.max_ent configure -textvariable $var_iemgui_max_rng
        }
        if {[eval concat $$var_iemgui_max_rng] > 0} {
            if {[eval concat $$var_iemgui_min_rng] <= 0} {
                set $var_iemgui_min_rng [expr [eval concat $$var_iemgui_max_rng] * 0.01]
                $mytoplevel.rng.min.ent configure -textvariable $var_iemgui_min_rng
            }
        } else {
            if {[eval concat $$var_iemgui_min_rng] > 0} {
                set $var_iemgui_max_rng [expr [eval concat $$var_iemgui_min_rng] * 0.01]
                $mytoplevel.rng.max_ent configure -textvariable $var_iemgui_max_rng
            }
        }
    }
}

proc ::dialog_iemgui::clip_fontsize {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_gn_fs [concat iemgui_gn_fs_$vid]
    global $var_iemgui_gn_fs
    
    variable define_min_fontsize
    
    if {[eval concat $$var_iemgui_gn_fs] < $define_min_fontsize} {
        set $var_iemgui_gn_fs $define_min_fontsize
        $mytoplevel.label.fs_ent configure -textvariable $var_iemgui_gn_fs
    }
}

proc ::dialog_iemgui::set_col_example {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_bcol [concat iemgui_bcol_$vid]
    global $var_iemgui_bcol
    set var_iemgui_fcol [concat iemgui_fcol_$vid]
    global $var_iemgui_fcol
    set var_iemgui_lcol [concat iemgui_lcol_$vid]
    global $var_iemgui_lcol
    
    $mytoplevel.colors.sections.exp.lb_bk configure \
        -background [eval concat $$var_iemgui_bcol] \
        -activebackground [eval concat $$var_iemgui_bcol] \
        -foreground [eval concat $$var_iemgui_lcol] \
        -activeforeground [eval concat $$var_iemgui_lcol]
    
    if { [eval concat $$var_iemgui_fcol] ne "none" } {
        $mytoplevel.colors.sections.exp.fr_bk configure \
            -background [eval concat $$var_iemgui_bcol] \
            -activebackground [eval concat $$var_iemgui_bcol] \
            -foreground [eval concat $$var_iemgui_fcol] \
            -activeforeground [eval concat $$var_iemgui_fcol]
    } else {
        $mytoplevel.colors.sections.exp.fr_bk configure \
            -background [eval concat $$var_iemgui_bcol] \
            -activebackground [eval concat $$var_iemgui_bcol] \
            -foreground [eval concat $$var_iemgui_bcol] \
            -activeforeground [eval concat $$var_iemgui_bcol]}
    
    # for OSX live updates
    if {$::windowingsystem eq "aqua"} {
        ::dialog_iemgui::apply_and_rebind_return $mytoplevel
    }
}

proc ::dialog_iemgui::preset_col {mytoplevel presetcol} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_l2_f1_b0 [concat iemgui_l2_f1_b0_$vid]
    global $var_iemgui_l2_f1_b0
    set var_iemgui_bcol [concat iemgui_bcol_$vid]
    global $var_iemgui_bcol
    set var_iemgui_fcol [concat iemgui_fcol_$vid]
    global $var_iemgui_fcol
    set var_iemgui_lcol [concat iemgui_lcol_$vid]
    global $var_iemgui_lcol
    
    if { [eval concat $$var_iemgui_l2_f1_b0] == 0 } { set $var_iemgui_bcol $presetcol }
    if { [eval concat $$var_iemgui_l2_f1_b0] == 1 } { set $var_iemgui_fcol $presetcol }
    if { [eval concat $$var_iemgui_l2_f1_b0] == 2 } { set $var_iemgui_lcol $presetcol }
    ::dialog_iemgui::set_col_example $mytoplevel
}

proc ::dialog_iemgui::choose_col_bkfrlb {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_l2_f1_b0 [concat iemgui_l2_f1_b0_$vid]
    global $var_iemgui_l2_f1_b0
    set var_iemgui_bcol [concat iemgui_bcol_$vid]
    global $var_iemgui_bcol
    set var_iemgui_fcol [concat iemgui_fcol_$vid]
    global $var_iemgui_fcol
    set var_iemgui_lcol [concat iemgui_lcol_$vid]
    global $var_iemgui_lcol
    
    if {[eval concat $$var_iemgui_l2_f1_b0] == 0} {
        set $var_iemgui_bcol [eval concat $$var_iemgui_bcol]
        set helpstring [tk_chooseColor -title [_ "Background color"] -initialcolor [eval concat $$var_iemgui_bcol]]
        if { $helpstring ne "" } {
            set $var_iemgui_bcol $helpstring }
    }
    if {[eval concat $$var_iemgui_l2_f1_b0] == 1} {
        set $var_iemgui_fcol [eval concat $$var_iemgui_fcol]
        set helpstring [tk_chooseColor -title [_ "Foreground color"] -initialcolor [eval concat $$var_iemgui_fcol]]
        if { $helpstring ne "" } {
            set $var_iemgui_fcol $helpstring }
    }
    if {[eval concat $$var_iemgui_l2_f1_b0] == 2} {
        set helpstring [tk_chooseColor -title [_ "Label color"] -initialcolor [eval concat $$var_iemgui_lcol]]
        if { $helpstring ne "" } {
            set $var_iemgui_lcol $helpstring }
    }
    ::dialog_iemgui::set_col_example $mytoplevel
}

proc ::dialog_iemgui::lilo {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_lin0_log1 [concat iemgui_lin0_log1_$vid]
    global $var_iemgui_lin0_log1
    set var_iemgui_lilo0 [concat iemgui_lilo0_$vid]
    global $var_iemgui_lilo0
    set var_iemgui_lilo1 [concat iemgui_lilo1_$vid]
    global $var_iemgui_lilo1
    
    ::dialog_iemgui::sched_rng $mytoplevel
    
    if {[eval concat $$var_iemgui_lin0_log1] == 0} {
        set $var_iemgui_lin0_log1 1
        $mytoplevel.para.lilo configure -text [eval concat $$var_iemgui_lilo1]
        ::dialog_iemgui::verify_rng $mytoplevel
        ::dialog_iemgui::sched_rng $mytoplevel
    } else {
        set $var_iemgui_lin0_log1 0
        $mytoplevel.para.lilo configure -text [eval concat $$var_iemgui_lilo0]
    }
}

proc ::dialog_iemgui::toggle_font {mytoplevel gn_f} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_gn_f [concat iemgui_gn_f_$vid]
    global $var_iemgui_gn_f
    
    set $var_iemgui_gn_f $gn_f
    
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

proc ::dialog_iemgui::lb {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_loadbang [concat iemgui_loadbang_$vid]
    global $var_iemgui_loadbang
    
    if {[eval concat $$var_iemgui_loadbang] == 0} {
        set $var_iemgui_loadbang 1
        $mytoplevel.para.lb configure -text [_ "Init"]
    } else {
        set $var_iemgui_loadbang 0
        $mytoplevel.para.lb configure -text [_ "No init"]
    }
}

proc ::dialog_iemgui::stdy_jmp {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_steady [concat iemgui_steady_$vid]
    global $var_iemgui_steady
    
    if {[eval concat $$var_iemgui_steady]} {
        set $var_iemgui_steady 0
        $mytoplevel.para.stdy_jmp configure -text [_ "Jump on click"]
    } else {
        set $var_iemgui_steady 1
        $mytoplevel.para.stdy_jmp configure -text [_ "Steady on click"]
    }
}

proc ::dialog_iemgui::apply {mytoplevel} {
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_wdt [concat iemgui_wdt_$vid]
    global $var_iemgui_wdt
    set var_iemgui_min_wdt [concat iemgui_min_wdt_$vid]
    global $var_iemgui_min_wdt
    set var_iemgui_hgt [concat iemgui_hgt_$vid]
    global $var_iemgui_hgt
    set var_iemgui_min_hgt [concat iemgui_min_hgt_$vid]
    global $var_iemgui_min_hgt
    set var_iemgui_min_rng [concat iemgui_min_rng_$vid]
    global $var_iemgui_min_rng
    set var_iemgui_max_rng [concat iemgui_max_rng_$vid]
    global $var_iemgui_max_rng
    set var_iemgui_lin0_log1 [concat iemgui_lin0_log1_$vid]
    global $var_iemgui_lin0_log1
    set var_iemgui_lilo0 [concat iemgui_lilo0_$vid]
    global $var_iemgui_lilo0
    set var_iemgui_lilo1 [concat iemgui_lilo1_$vid]
    global $var_iemgui_lilo1
    set var_iemgui_loadbang [concat iemgui_loadbang_$vid]
    global $var_iemgui_loadbang
    set var_iemgui_num [concat iemgui_num_$vid]
    global $var_iemgui_num
    set var_iemgui_steady [concat iemgui_steady_$vid]
    global $var_iemgui_steady
    set var_iemgui_snd [concat iemgui_snd_$vid]
    global $var_iemgui_snd
    set var_iemgui_rcv [concat iemgui_rcv_$vid]
    global $var_iemgui_rcv
    set var_iemgui_gui_nam [concat iemgui_gui_nam_$vid]
    global $var_iemgui_gui_nam
    set var_iemgui_gn_dx [concat iemgui_gn_dx_$vid]
    global $var_iemgui_gn_dx
    set var_iemgui_gn_dy [concat iemgui_gn_dy_$vid]
    global $var_iemgui_gn_dy
    set var_iemgui_gn_f [concat iemgui_gn_f_$vid]
    global $var_iemgui_gn_f
    set var_iemgui_gn_fs [concat iemgui_gn_fs_$vid]
    global $var_iemgui_gn_fs
    set var_iemgui_bcol [concat iemgui_bcol_$vid]
    global $var_iemgui_bcol
    set var_iemgui_fcol [concat iemgui_fcol_$vid]
    global $var_iemgui_fcol
    set var_iemgui_lcol [concat iemgui_lcol_$vid]
    global $var_iemgui_lcol
    
    ::dialog_iemgui::clip_dim $mytoplevel
    ::dialog_iemgui::clip_num $mytoplevel
    ::dialog_iemgui::sched_rng $mytoplevel
    ::dialog_iemgui::verify_rng $mytoplevel
    ::dialog_iemgui::sched_rng $mytoplevel
    ::dialog_iemgui::clip_fontsize $mytoplevel
    
    if {[eval concat $$var_iemgui_snd] == ""} {set hhhsnd "empty"} else {set hhhsnd [eval concat $$var_iemgui_snd]}
    if {[eval concat $$var_iemgui_rcv] == ""} {set hhhrcv "empty"} else {set hhhrcv [eval concat $$var_iemgui_rcv]}
    if {[eval concat $$var_iemgui_gui_nam] == ""} {set hhhgui_nam "empty"
    } else {
        set hhhgui_nam [eval concat $$var_iemgui_gui_nam]}
    
    if {[string index $hhhsnd 0] == "$"} {
        set hhhsnd [string replace $hhhsnd 0 0 #] }
    if {[string index $hhhrcv 0] == "$"} {
        set hhhrcv [string replace $hhhrcv 0 0 #] }
    if {[string index $hhhgui_nam 0] == "$"} {
        set hhhgui_nam [string replace $hhhgui_nam 0 0 #] }
    
    set hhhsnd [unspace_text $hhhsnd]
    set hhhrcv [unspace_text $hhhrcv]
    set hhhgui_nam [unspace_text $hhhgui_nam]

    # make sure the offset boxes have a value
    if {[eval concat $$var_iemgui_gn_dx] eq ""} {set $var_iemgui_gn_dx 0}
    if {[eval concat $$var_iemgui_gn_dy] eq ""} {set $var_iemgui_gn_dy 0}

    pdsend [concat $mytoplevel dialog \
            [eval concat $$var_iemgui_wdt] \
            [eval concat $$var_iemgui_hgt] \
            [eval concat $$var_iemgui_min_rng] \
            [eval concat $$var_iemgui_max_rng] \
            [eval concat $$var_iemgui_lin0_log1] \
            [eval concat $$var_iemgui_loadbang] \
            [eval concat $$var_iemgui_num] \
            $hhhsnd \
            $hhhrcv \
            $hhhgui_nam \
            [eval concat $$var_iemgui_gn_dx] \
            [eval concat $$var_iemgui_gn_dy] \
            [eval concat $$var_iemgui_gn_f] \
            [eval concat $$var_iemgui_gn_fs] \
            [eval concat $$var_iemgui_bcol] \
            [eval concat $$var_iemgui_fcol] \
            [eval concat $$var_iemgui_lcol] \
            [eval concat $$var_iemgui_steady]]
}


proc ::dialog_iemgui::cancel {mytoplevel} {
    pdsend "$mytoplevel cancel"
}

proc ::dialog_iemgui::ok {mytoplevel} {
    ::dialog_iemgui::apply $mytoplevel
    ::dialog_iemgui::cancel $mytoplevel
}

proc ::dialog_iemgui::pdtk_iemgui_dialog {mytoplevel mainheader dim_header \
                                       wdt min_wdt wdt_label \
                                       hgt min_hgt hgt_label \
                                       rng_header min_rng min_rng_label max_rng \
                                       max_rng_label rng_sched \
                                       lin0_log1 lilo0_label lilo1_label \
                                       loadbang steady num_label num \
                                       snd rcv \
                                       gui_name \
                                       gn_dx gn_dy gn_f gn_fs \
                                       bcol fcol lcol} {
    
    set vid [string trimleft $mytoplevel .]
    
    set var_iemgui_wdt [concat iemgui_wdt_$vid]
    global $var_iemgui_wdt
    set var_iemgui_min_wdt [concat iemgui_min_wdt_$vid]
    global $var_iemgui_min_wdt
    set var_iemgui_hgt [concat iemgui_hgt_$vid]
    global $var_iemgui_hgt
    set var_iemgui_min_hgt [concat iemgui_min_hgt_$vid]
    global $var_iemgui_min_hgt
    set var_iemgui_min_rng [concat iemgui_min_rng_$vid]
    global $var_iemgui_min_rng
    set var_iemgui_max_rng [concat iemgui_max_rng_$vid]
    global $var_iemgui_max_rng
    set var_iemgui_rng_sch [concat iemgui_rng_sch_$vid]
    global $var_iemgui_rng_sch
    set var_iemgui_lin0_log1 [concat iemgui_lin0_log1_$vid]
    global $var_iemgui_lin0_log1
    set var_iemgui_lilo0 [concat iemgui_lilo0_$vid]
    global $var_iemgui_lilo0
    set var_iemgui_lilo1 [concat iemgui_lilo1_$vid]
    global $var_iemgui_lilo1
    set var_iemgui_loadbang [concat iemgui_loadbang_$vid]
    global $var_iemgui_loadbang
    set var_iemgui_num [concat iemgui_num_$vid]
    global $var_iemgui_num
    set var_iemgui_steady [concat iemgui_steady_$vid]
    global $var_iemgui_steady
    set var_iemgui_snd [concat iemgui_snd_$vid]
    global $var_iemgui_snd
    set var_iemgui_rcv [concat iemgui_rcv_$vid]
    global $var_iemgui_rcv
    set var_iemgui_gui_nam [concat iemgui_gui_nam_$vid]
    global $var_iemgui_gui_nam
    set var_iemgui_gn_dx [concat iemgui_gn_dx_$vid]
    global $var_iemgui_gn_dx
    set var_iemgui_gn_dy [concat iemgui_gn_dy_$vid]
    global $var_iemgui_gn_dy
    set var_iemgui_gn_f [concat iemgui_gn_f_$vid]
    global $var_iemgui_gn_f
    set var_iemgui_gn_fs [concat iemgui_gn_fs_$vid]
    global $var_iemgui_gn_fs
    set var_iemgui_l2_f1_b0 [concat iemgui_l2_f1_b0_$vid]
    global $var_iemgui_l2_f1_b0
    set var_iemgui_bcol [concat iemgui_bcol_$vid]
    global $var_iemgui_bcol
    set var_iemgui_fcol [concat iemgui_fcol_$vid]
    global $var_iemgui_fcol
    set var_iemgui_lcol [concat iemgui_lcol_$vid]
    global $var_iemgui_lcol
    
    set $var_iemgui_wdt $wdt
    set $var_iemgui_min_wdt $min_wdt
    set $var_iemgui_hgt $hgt
    set $var_iemgui_min_hgt $min_hgt
    set $var_iemgui_min_rng $min_rng
    set $var_iemgui_max_rng $max_rng
    set $var_iemgui_rng_sch $rng_sched
    set $var_iemgui_lin0_log1 $lin0_log1
    set $var_iemgui_lilo0 $lilo0_label
    set $var_iemgui_lilo1 $lilo1_label
    set $var_iemgui_loadbang $loadbang
    set $var_iemgui_num $num
    set $var_iemgui_steady $steady
    if {$snd == "empty"} {set $var_iemgui_snd [format ""]
    } else {set $var_iemgui_snd [format "%s" $snd]}
    if {$rcv == "empty"} {set $var_iemgui_rcv [format ""]
    } else {set $var_iemgui_rcv [format "%s" $rcv]}
    if {$gui_name == "empty"} {set $var_iemgui_gui_nam [format ""]
    } else {set $var_iemgui_gui_nam [format "%s" $gui_name]}
    
    if {[string index [eval concat $$var_iemgui_snd] 0] == "#"} {
        set $var_iemgui_snd [string replace [eval concat $$var_iemgui_snd] 0 0 $] }
    if {[string index [eval concat $$var_iemgui_rcv] 0] == "#"} {
        set $var_iemgui_rcv [string replace [eval concat $$var_iemgui_rcv] 0 0 $] }
    if {[string index [eval concat $$var_iemgui_gui_nam] 0] == "#"} {
        set $var_iemgui_gui_nam [string replace [eval concat $$var_iemgui_gui_nam] 0 0 $] }
    set $var_iemgui_gn_dx $gn_dx
    set $var_iemgui_gn_dy $gn_dy
    set $var_iemgui_gn_f $gn_f
    set $var_iemgui_gn_fs $gn_fs
    
    set $var_iemgui_bcol $bcol
    set $var_iemgui_fcol $fcol
    set $var_iemgui_lcol $lcol
    
    set $var_iemgui_l2_f1_b0 0

    # Override incoming values for known iem guis.
    set iemgui_type [_ $mainheader] 
    set iemgui_range_header [_ $rng_header]
    set lilo_width 5
    switch -- $mainheader {
        "|bang|" {
            set iemgui_type [_ "Bang"]
            set wdt_label "Size:"
            set iemgui_range_header [_ "Flash Time (msec)"]
            set min_rng_label [_ "Intrrpt:"]
            set max_rng_label [_ "Hold:"] }
        "|tgl|" {
            set iemgui_type [_ "Toggle"]
            set wdt_label [_ "Size:"]
            set iemgui_range_header [_ "Non Zero Value"]
            set min_rng_label [_ "Value:"] }
        "|nbx|" {
            set iemgui_type [_ "Number2"]
            set wdt_label [_ "Width (digits):"]
            set hgt_label [_ "Height:"]
            set iemgui_range_header [_ "Output Range"]
            set min_rng_label [_ "Lower:"]
            set max_rng_label [_ "Upper:"]
            set num_label [_ "Log height:"] }
        "|vsl|" {
            set iemgui_type [_ "Vslider"]
            set wdt_label [_ "Width:"]
            set hgt_label [_ "Height:"]
            set iemgui_range_header [_ "Output Range"]
            set min_rng_label [_ "Lower:"]
            set max_rng_label [_ "Upper:"] }
        "|hsl|" {
            set iemgui_type [_ "Hslider"]
            set wdt_label [_ "Width:"]
            set hgt_label [_ "Height:"]
            set iemgui_range_header [_ "Output Range"]
            set min_rng_label [_ "Lower:"]
            set max_rng_label [_ "Upper:"] }
        "|vradio|" {
            set iemgui_type [_ "Vradio"]
            set wdt_label [_ "Size:"]
            set num_label [_ "Num cells:"] }
        "|hradio|" {
            set iemgui_type [_ "Hradio"]
            set wdt_label [_ "Size:"]
            set num_label [_ "Num cells:"] }
        "|vu|" {
            set iemgui_type [_ "VU Meter"]
            set wdt_label [_ "Width:"]
            set hgt_label [_ "Height:"]
            # VU needs more room to display "no_scale"
            set lilo_width 8 }
        "|cnv|" {
            set iemgui_type [_ "Canvas"]
            set wdt_label [_ "Size:"]
            set iemgui_range_header [_ "Visible Rectangle (pix)"]
            set min_rng_label [_ "Width:"]
            set max_rng_label [_ "Height:"] }
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
    label $mytoplevel.dim.w_lab -text [_ $wdt_label]
    entry $mytoplevel.dim.w_ent -textvariable $var_iemgui_wdt -width 4
    label $mytoplevel.dim.dummy1 -text "" -width 1
    label $mytoplevel.dim.h_lab -text [_ $hgt_label]
    entry $mytoplevel.dim.h_ent -textvariable $var_iemgui_hgt -width 4
    pack $mytoplevel.dim.w_lab $mytoplevel.dim.w_ent -side left
    if { $hgt_label ne "empty" } {
        pack $mytoplevel.dim.dummy1 $mytoplevel.dim.h_lab $mytoplevel.dim.h_ent -side left }
    
    # range
    labelframe $mytoplevel.rng
    pack $mytoplevel.rng -side top -fill x
    frame $mytoplevel.rng.min
    label $mytoplevel.rng.min.lab -text [_ $min_rng_label]
    entry $mytoplevel.rng.min.ent -textvariable $var_iemgui_min_rng -width 7
    label $mytoplevel.rng.dummy1 -text "" -width 1
    label $mytoplevel.rng.max_lab -text [_ $max_rng_label]
    entry $mytoplevel.rng.max_ent -textvariable $var_iemgui_max_rng -width 7
    if { $rng_header ne "empty" } {
        $mytoplevel.rng config -borderwidth 1 -pady 4 -text [_ $iemgui_range_header]
        if { $min_rng_label ne "empty" } {
            pack $mytoplevel.rng.min
            pack $mytoplevel.rng.min.lab $mytoplevel.rng.min.ent -side left }
        if { $max_rng_label ne "empty" } {
            $mytoplevel.rng config -padx 26
            pack configure $mytoplevel.rng.min -side left
            pack $mytoplevel.rng.dummy1 $mytoplevel.rng.max_lab $mytoplevel.rng.max_ent -side left}
    }

    # parameters
    labelframe $mytoplevel.para -borderwidth 1 -padx 5 -pady 5 -text [_ "Parameters"]
    pack $mytoplevel.para -side top -fill x -pady 5
    if {[eval concat $$var_iemgui_lin0_log1] == 0} {
        button $mytoplevel.para.lilo -text [_ [eval concat $$var_iemgui_lilo0]] -width $lilo_width \
            -command "::dialog_iemgui::lilo $mytoplevel" }
    if {[eval concat $$var_iemgui_lin0_log1] == 1} {
        button $mytoplevel.para.lilo -text [_ [eval concat $$var_iemgui_lilo1]] -width $lilo_width \
            -command "::dialog_iemgui::lilo $mytoplevel" }
    if {[eval concat $$var_iemgui_loadbang] == 0} {
        button $mytoplevel.para.lb -text [_ "No init"] \
            -command "::dialog_iemgui::lb $mytoplevel" -width 7 }
    if {[eval concat $$var_iemgui_loadbang] == 1} {
        button $mytoplevel.para.lb -text [_ "Init"] \
            -command "::dialog_iemgui::lb $mytoplevel" -width 7 }
    frame $mytoplevel.para.num
    label $mytoplevel.para.num.lab -text [_ $num_label]
    entry $mytoplevel.para.num.ent -textvariable $var_iemgui_num -width 4
    pack $mytoplevel.para.num.ent $mytoplevel.para.num.lab -side right -anchor e

    if {[eval concat $$var_iemgui_steady] == 0} {
        button $mytoplevel.para.stdy_jmp -command "::dialog_iemgui::stdy_jmp $mytoplevel" \
            -text [_ "Jump on click"] }
    if {[eval concat $$var_iemgui_steady] == 1} {
        button $mytoplevel.para.stdy_jmp -command "::dialog_iemgui::stdy_jmp $mytoplevel" \
            -text [_ "Steady on click"] }
    if {[eval concat $$var_iemgui_lin0_log1] >= 0} {
        pack $mytoplevel.para.lilo -side left -expand 1}
    if {[eval concat $$var_iemgui_loadbang] >= 0} {
        pack $mytoplevel.para.lb -side left -expand 1}
    if {[eval concat $$var_iemgui_num] > 0} {
        pack $mytoplevel.para.num -side left -expand 1}
    if {[eval concat $$var_iemgui_steady] >= 0} {
        pack $mytoplevel.para.stdy_jmp -side left -expand 1}
    
    # messages
    labelframe $mytoplevel.s_r -borderwidth 1 -padx 5 -pady 5 -text [_ "Messages"]
    pack $mytoplevel.s_r -side top -fill x
    frame $mytoplevel.s_r.send
    pack $mytoplevel.s_r.send -side top -anchor e -padx 5
    label $mytoplevel.s_r.send.lab -text [_ "Send symbol:"]
    entry $mytoplevel.s_r.send.ent -textvariable $var_iemgui_snd -width 21
    if { $snd ne "nosndno" } {
        pack $mytoplevel.s_r.send.lab $mytoplevel.s_r.send.ent -side left \
            -fill x -expand 1
    }
    
    frame $mytoplevel.s_r.receive
    pack $mytoplevel.s_r.receive -side top -anchor e -padx 5
    label $mytoplevel.s_r.receive.lab -text [_ "Receive symbol:"]
    entry $mytoplevel.s_r.receive.ent -textvariable $var_iemgui_rcv -width 21
    if { $rcv ne "norcvno" } {
        pack $mytoplevel.s_r.receive.lab $mytoplevel.s_r.receive.ent -side left \
            -fill x -expand 1
    }
    
    # get the current font name from the int given from C-space (gn_f)
    set current_font $::font_family
    if {[eval concat $$var_iemgui_gn_f] == 1} \
        { set current_font "Helvetica" }
    if {[eval concat $$var_iemgui_gn_f] == 2} \
        { set current_font "Times" }
    
    # label
    labelframe $mytoplevel.label -borderwidth 1 -text [_ "Label"] -padx 5 -pady 5
    pack $mytoplevel.label -side top -fill x -pady 5
    entry $mytoplevel.label.name_entry -textvariable $var_iemgui_gui_nam \
        -width 30 -font [list $current_font 14 $::font_weight]
    pack $mytoplevel.label.name_entry -side top -fill both -padx 5
    
    frame $mytoplevel.label.xy -padx 20 -pady 1
    pack $mytoplevel.label.xy -side top
    label $mytoplevel.label.xy.x_lab -text [_ "X offset:"]
    entry $mytoplevel.label.xy.x_entry -textvariable $var_iemgui_gn_dx -width 5
    label $mytoplevel.label.xy.dummy1 -text " " -width 1
    label $mytoplevel.label.xy.y_lab -text [_ "Y offset:"]
    entry $mytoplevel.label.xy.y_entry -textvariable $var_iemgui_gn_dy -width 5
    pack $mytoplevel.label.xy.x_lab $mytoplevel.label.xy.x_entry $mytoplevel.label.xy.dummy1 \
        $mytoplevel.label.xy.y_lab $mytoplevel.label.xy.y_entry -side left
    
    button $mytoplevel.label.fontpopup_label -text $current_font \
        -font [list $current_font 16 $::font_weight] -pady 4
    pack $mytoplevel.label.fontpopup_label -side left -anchor w \
        -expand 1 -fill x -padx 5
    frame $mytoplevel.label.fontsize
    pack $mytoplevel.label.fontsize -side right -padx 5 -pady 5
    label $mytoplevel.label.fontsize.label -text [_ "Size:"]
    entry $mytoplevel.label.fontsize.entry -textvariable $var_iemgui_gn_fs -width 4
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
    bind $mytoplevel.label.fontpopup_label <Button> \
        [list tk_popup $mytoplevel.popup %X %Y]
    
    # colors
    labelframe $mytoplevel.colors -borderwidth 1 -text [_ "Colors"] -padx 5 -pady 5
    pack $mytoplevel.colors -fill x
    
    frame $mytoplevel.colors.select
    pack $mytoplevel.colors.select -side top
    radiobutton $mytoplevel.colors.select.radio0 -value 0 -variable \
        $var_iemgui_l2_f1_b0 -text [_ "Background"] -justify left
    radiobutton $mytoplevel.colors.select.radio1 -value 1 -variable \
        $var_iemgui_l2_f1_b0 -text [_ "Front"] -justify left
    radiobutton $mytoplevel.colors.select.radio2 -value 2 -variable \
        $var_iemgui_l2_f1_b0 -text [_ "Label"] -justify left
    if { [eval concat $$var_iemgui_fcol] ne "none" } {
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
    if { [eval concat $$var_iemgui_fcol] ne "none" } {
        label $mytoplevel.colors.sections.exp.fr_bk -text "o=||=o" -width 6 \
            -background [eval concat $$var_iemgui_bcol] \
            -activebackground [eval concat $$var_iemgui_bcol] \
            -foreground [eval concat $$var_iemgui_fcol] \
            -activeforeground [eval concat $$var_iemgui_fcol] \
            -font [list $current_font 14 $::font_weight] -padx 2 -pady 2 -relief ridge
    } else {
        label $mytoplevel.colors.sections.exp.fr_bk -text "o=||=o" -width 6 \
            -background [eval concat $$var_iemgui_bcol] \
            -activebackground [eval concat $$var_iemgui_bcol] \
            -foreground [eval concat $$var_iemgui_bcol] \
            -activeforeground [eval concat $$var_iemgui_bcol] \
            -font [list $current_font 14 $::font_weight] -padx 2 -pady 2 -relief ridge
    }
    label $mytoplevel.colors.sections.exp.lb_bk -text [_ "Test label"] \
        -background [eval concat $$var_iemgui_bcol] \
        -activebackground [eval concat $$var_iemgui_bcol] \
        -foreground [eval concat $$var_iemgui_lcol] \
        -activeforeground [eval concat $$var_iemgui_lcol] \
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
               label $mytoplevel.colors.$r.c$i -background $hexcol -activebackground $hexcol -relief ridge -padx 7 -pady 0
               bind $mytoplevel.colors.$r.c$i <Button> "::dialog_iemgui::preset_col $mytoplevel $hexcol"
           }
       pack $mytoplevel.colors.$r.c0 $mytoplevel.colors.$r.c1 $mytoplevel.colors.$r.c2 $mytoplevel.colors.$r.c3 \
           $mytoplevel.colors.$r.c4 $mytoplevel.colors.$r.c5 $mytoplevel.colors.$r.c6 $mytoplevel.colors.$r.c7 \
           $mytoplevel.colors.$r.c8 $mytoplevel.colors.$r.c9 -side left
    }
    
    # buttons
    frame $mytoplevel.cao -pady 10
    pack $mytoplevel.cao -side top -expand 1 -fill x
    button $mytoplevel.cao.cancel -text [_ "Cancel"] \
        -command "::dialog_iemgui::cancel $mytoplevel"
    pack $mytoplevel.cao.cancel -side left -padx 10 -expand 1 -fill x
    if {$::windowingsystem ne "aqua"} {
        button $mytoplevel.cao.apply -text [_ "Apply"] \
            -command "::dialog_iemgui::apply $mytoplevel"
        pack $mytoplevel.cao.apply -side left -padx 10 -expand 1 -fill x
    }
    button $mytoplevel.cao.ok -text [_ "OK"] \
        -command "::dialog_iemgui::ok $mytoplevel" -default active
    pack $mytoplevel.cao.ok -side left -padx 10 -expand 1 -fill x
    
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

        # can't see focus for buttons, so disable it
        if {[winfo exists $mytoplevel.para.lilo]} {
            $mytoplevel.para.lilo config -takefocus 0
        }
        if {[winfo exists $mytoplevel.para.lb]} {
            $mytoplevel.para.lb config -takefocus 0
        }
        if {[winfo exists $mytoplevel.para.stdy_jmp]} {
            $mytoplevel.para.stdy_jmp config -takefocus 0
        }
        $mytoplevel.label.fontpopup_label config -takefocus 0
        $mytoplevel.colors.select.radio0 config -takefocus 0
        $mytoplevel.colors.select.radio1 config -takefocus 0
        $mytoplevel.colors.select.radio2 config -takefocus 0
        $mytoplevel.colors.sections.but config -takefocus 0

        # show active focus on the ok button as it *is* activated on Return
        $mytoplevel.cao.ok config -default normal
        bind $mytoplevel.cao.ok <FocusIn> "$mytoplevel.cao.ok config -default active"
        bind $mytoplevel.cao.ok <FocusOut> "$mytoplevel.cao.ok config -default normal"
    }
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
