## infrastructure for drawing Pd "widgets" (graphical objects)
package provide pdwidget_iemgui 0.1

namespace eval ::pdwidget::iemgui:: { }


proc ::pdwidget::iemgui::create_bang {obj cnv posX posY args} {
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}_BASE]
    $cnv create oval 0 0 0 0 -tags [list ${tag} ${tag}_BUTTON ${tag}_iolets]
    $cnv create text 0 0 -anchor w -tags [list ${tag} ${tag}_label]
    $cnv move $tag $posX $posY

    ::pdwidget::widgetbehavior $obj config ::pdwidget::iemgui::config_bang
    ::pdwidget::widgetbehavior $obj select ::pdwidget::iemgui::select
    if { $args ne {} } {::pdwidget::iemgui::config_bang $obj $cnv {*}$args }
}


########################################################################
# common procedures

proc ::pdwidget::iemgui::select {obj cnv state} {
    # if selection is activated, use a different color
    # TODO: label

    if {$state != 0} {
        set color #0000FF
    } else {
        set color #000000
    }
    set tag "[::pdwidget::base_tag $obj]"
    $cnv itemconfigure "${tag}_BASE" -outline $color
    $cnv itemconfigure "${tag}_BUTTON" -outline $color
}

# this is a helper to be called from the object-specific 'config' procs
# that deals with the labels (it's the same with every iemgui)
proc ::pdwidget::iemgui::_config_common {tag cnv options} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set fontname {}
    set fontsize {}
    foreach {k v} $options {
        switch -exact -- $k {
            default {
            } "-labelpos" {
                foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
                set xnew [lindex $v 0]
                set ynew [lindex $v 1]
                $cnv coords "${tag}_label" \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
            } "-lcolor" {
                $cnv itemconfigure "${tag}_label" -fill $v
            } "-label" {
                pdtk_text_set $cnv "${tag}_label" $v
            } "-font" {
                set fontname $v
            } "-fontsize" {
                set fontsize [expr -int($v) * $zoom]
            }
        }
    }
    if { $fontname ne {} || $fontsize ne {} } {
        set font [$cnv itemcget "${tag}_label" -font]
        if { $fontname ne {} } { lset font 0 $fontname }
        if { $fontsize ne {} } { lset font 1 $fontsize }
        lset font 2 $::font_weight
        $cnv itemconfigure "${tag}_label" -font $font
    }
}

########################################################################
# [bng]
proc ::pdwidget::iemgui::config_bang {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]
    set recreate_iolets 0

    set zoom [::pd::canvas::get_zoom $cnv]
    set iow $::pdwidget::IOWIDTH
    set ih [expr $::pdwidget::IHEIGHT - 0.5]
    set oh [expr $::pdwidget::OHEIGHT - 1]
    ::pdwidget::iemgui::_config_common $tag $cnv $args
    foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
    foreach {k v} $args {
        switch -exact -- $k {
            default {
            } "-size" {
                foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
                set xnew [lindex $v 0]
                set ynew [lindex $v 1]
                $cnv coords "${tag}" \
                    $xpos                        $ypos                  \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                $cnv coords "${tag}_BASE" \
                    $xpos                        $ypos                  \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                $cnv coords "${tag}_BUTTON" \
                    [expr $xpos +                 $zoom] [expr $ypos +                 $zoom] \
                    [expr $xpos + ($xnew - 1.5) * $zoom] [expr $ypos + ($ynew - 1.5) * $zoom]
                set recreate_iolets 1
            } "-bcolor" {
                $cnv itemconfigure "${tag}_BASE" -fill $v
            } "-fcolor" {
                if { [$cnv itemcget "${tag}_BUTTON" -fill] ne {} } {
                    $cnv itemconfigure "${tag}_BUTTON" -fill $v
                }
            }
        }
        $cnv itemconfigure "${tag}_BASE" -width $zoom
        $cnv itemconfigure "${tag}_BUTTON" -width $zoom
    }

    if {$recreate_iolets} {
        ::pdwidget::refresh_iolets $obj
    }
}

########################################################################
# [cnv]
proc ::pdwidget::iemgui::create_canvas {obj cnv posX posY args} {
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}_RECT]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}_BASE]
    $cnv create text 0 0 -anchor w -tags [list ${tag} ${tag}_label]
    $cnv move $tag $posX $posY

    ::pdwidget::widgetbehavior $obj config ::pdwidget::iemgui::config_canvas
    ::pdwidget::widgetbehavior $obj select ::pdwidget::iemgui::select_canvas
    if { $args ne {} } {::pdwidget::iemgui::config_canvas $obj $cnv {*}$args }
}
proc ::pdwidget::iemgui::config_canvas {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]

    set zoom [::pd::canvas::get_zoom $cnv]
    set offset 0
    if {$zoom > 1} {
        set offset $zoom
    }

    ::pdwidget::iemgui::_config_common $tag $cnv $args
    foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
    foreach {k v} $args {
        switch -exact -- $k {
            default {
            } "-size" {
                set xnew [lindex $v 0]
                set ynew [lindex $v 1]
                $cnv coords "${tag}_BASE" \
                    [expr $xpos + $offset]                 [expr $ypos + $offset] \
                    [expr $xpos + $offset + $xnew * $zoom] [expr $ypos + $offset + $ynew * $zoom]
            } "-visible" {
                set w [lindex $v 0]
                set h [lindex $v 1]
                $cnv coords "${tag}"       $xpos $ypos [expr $xpos + $w * $zoom] [expr $ypos + $h * $zoom]
                $cnv coords "${tag}_RECT" $xpos $ypos [expr $xpos + $w * $zoom] [expr $ypos + $h * $zoom]
            } "-bcolor" {
                $cnv itemconfigure "${tag}_RECT" -fill $v -outline $v
                $cnv itemconfigure "${tag}_BASE" -width 1 -outline {}
            }
        }
    }
    $cnv itemconfigure "${tag}_BASE" -width $zoom
    # TODO: in the original code, BASE was shifted by $offset to 'keep zoomed border inside visible area'
}
proc ::pdwidget::iemgui::select_canvas {obj cnv state} {
    # get the normal color from the object's state
    # if selection is activated, use a different color
    if {$state != 0} {
        set color #0000FF
    } else {
        set color {}
    }
    set tag "[::pdwidget::base_tag $obj]_BASE"
    $cnv itemconfigure ${tag} -outline $color
}

########################################################################
# [hradio], [vradio]
proc ::pdwidget::iemgui::create_radio {obj cnv posX posY args} {
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}_BASE0 ${tag}_iolets]
    $cnv create text 0 0 -anchor w -tags [list ${tag} ${tag}_label]
    $cnv move $tag $posX $posY

    ::pdwidget::widgetbehavior $obj config ::pdwidget::iemgui::config_radio
    ::pdwidget::widgetbehavior $obj select ::pdwidget::iemgui::select
    if { $args ne {} } {::pdwidget::iemgui::config_radio $obj $cnv {*}$args }
}
proc ::pdwidget::iemgui::_radio_recreate_buttons {cnv obj numX numY} {
    # create an array of numX*numY buttons
    set tag [::pdwidget::base_tag $obj]
    $cnv delete ${tag}_RADIO
    for {set x 0} {$x < $numX} {incr x} {
        for {set y 0} {$y < $numY} {incr y} {
            set xy "${x}x${y}"

            $cnv create rectangle 0 0 0 0 -fill {} -outline {} \
                -tags [list ${tag} ${tag}_RADIO ${tag}_BASE ${tag}_BASE_${xy} ${tag}_GRID${xy} GRID${xy}]
            $cnv create rectangle 0 0 0 0 -fill {} -outline {} \
                -tags [list ${tag} ${tag}_RADIO ${tag}_KNOB ${tag}_KNOB_${xy} ${tag}_GRID${xy} GRID${xy}]
        }
    }
    $cnv lower "${tag}_RADIO" "${tag}_BASE0"
    $cnv raise "${tag}_RADIO" "${tag}_BASE0"
}
proc ::pdwidget::iemgui::_radio_getactive {cnv tag} {
    set t {}
    foreach t [$cnv find withtag ${tag}_X] {break}
    if { $t eq {} } {return}
    # we have an active button, but which one is it?
    set i 0
    foreach t [$cnv find withtag ${tag}_KNOB] {
        if {[lsearch -exact [$cnv gettags $t] X] >= 0} {
            # found it
            return [list $i [$cnv itemcget $t -fill]]
        }
        incr i
    }
}
proc ::pdwidget::iemgui::_radio_reconfigure_buttons {cnv obj zoom} {
    set tag [::pdwidget::base_tag $obj]
    foreach {xpos ypos w h} [$cnv coords "${tag}_BASE0"] {break}
    set w [expr $w - $xpos]
    set h [expr $h - $ypos]

    set knob_x0 [expr $w / 4 - 0.5]
    set knob_y0 [expr $h / 4 - 0.5]
    set knob_x1 [expr $w - $knob_x0]
    set knob_y1 [expr $h - $knob_y0]

    # get the coord of the outlet...
    set ylast $ypos
    set numX 0
    set numY 0

    # resize/reposition the buttons
    foreach id [$cnv find withtag "${tag}_KNOB"] {
        set x {}
        set y {}
        foreach t [$cnv gettags $id] {
            if { [regexp {^GRID(.+)x(.+)} $t _ x y] } {
                break;
            }
        }
        if { $x eq {} || $y eq {} } { break }
        if {$x > $numX} {set numX $x}
        if {$y > $numY} {set numY $y}

        set xy "${x}x${y}"
        set ylast [expr $h * $y]
        $cnv coords "${tag}_BASE_${xy}" 0 0 $w $h
        $cnv coords "${tag}_KNOB_${xy}" $knob_x0 $knob_y0 $knob_x1 $knob_y1
        $cnv move "${tag}_GRID${xy}" [expr $w * $x] $ylast
    }
    $cnv itemconfigure "${tag}_BASE" -outline black
    $cnv move "${tag}_RADIO" $xpos $ypos

    $cnv coords "${tag}" $xpos $ypos [expr $xpos + ($numX + 1) * $w * $zoom] [expr $ypos + ($numY + 1) * $h * $zoom]

}
proc ::pdwidget::iemgui::config_radio {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]
    set recreate_iolets 0

    # which button is activated?
    set active {}
    set activecolor {}

    foreach {active activecolor} [::pdwidget::iemgui::_radio_getactive $cnv $tag] {break}
    set zoom [::pd::canvas::get_zoom $cnv]
    ::pdwidget::iemgui::_config_common $tag $cnv $args
    foreach {k v} $args {
        switch -exact -- $k {
            default {
            } "-bcolor" {
                $cnv itemconfigure "${tag}_BASE" -fill $v
            } "-fcolor" {
                set activecolor $v
            } "-size" {
                set w [lindex $v 0]
                set h [lindex $v 1]
                foreach {x y} [$cnv coords "${tag}_BASE0"] {break}
                $cnv coords "${tag}_BASE0" $x $y [expr $x + $w * $zoom] [expr $y + $h * $zoom]
                ::pdwidget::iemgui::_radio_reconfigure_buttons $cnv $obj $zoom
                set recreate_iolets 1
            } "-number" {
                set xnew [lindex $v 0]
                set ynew [lindex $v 1]
                ::pdwidget::iemgui::_radio_recreate_buttons $cnv $obj $xnew $ynew
                ::pdwidget::iemgui::_radio_reconfigure_buttons $cnv $obj $zoom
                set recreate_iolets 1
            }
        }
    }
    $cnv itemconfigure "${tag}_BASE" -width $zoom

    if { $active ne {} } {
        ::pdwidget::radio::activate $obj $active $activecolor
    }
    if { $recreate_iolets } {
        ## FIXXME cnv handling is ugly
        set iolets [::pdwidget::get_iolets $obj $cnv inlet]
        ::pdwidget::create_inlets $obj {*}$iolets
        set iolets [::pdwidget::get_iolets $obj $cnv outlet]
        ::pdwidget::create_outlets $obj {*}$iolets
    }
}

########################################################################
# [tgl]
proc ::pdwidget::iemgui::create_toggle {obj cnv posX posY args} {
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}_BASE]
    $cnv create line 0 0 0 0 -tags [list ${tag} ${tag}_X ${tag}_X1] -fill {}
    $cnv create line 0 0 0 0 -tags [list ${tag} ${tag}_X ${tag}_X2 ${tag}_iolets] -fill {}
    $cnv create text 0 0 -anchor w -tags [list ${tag} ${tag}_label]
    $cnv move $tag $posX $posY

    ::pdwidget::widgetbehavior $obj config ::pdwidget::iemgui::config_toggle
    ::pdwidget::widgetbehavior $obj select ::pdwidget::iemgui::select
    if { $args ne {} } {::pdwidget::iemgui::config_toggle $obj $cnv {*}$args }
}
proc ::pdwidget::iemgui::config_toggle {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]
    set recreate_iolets 0

    set zoom [::pd::canvas::get_zoom $cnv]
    set iow $::pdwidget::IOWIDTH
    set ih [expr $::pdwidget::IHEIGHT - 0.5]
    set oh [expr $::pdwidget::OHEIGHT - 1]
    ::pdwidget::iemgui::_config_common $tag $cnv $args
    foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
    foreach {k v} $args {
        switch -exact -- $k {
            default {
            } "-size" {
                set xnew [lindex $v 0]
                set ynew [lindex $v 1]
                $cnv coords "${tag}" \
                    $xpos                        $ypos                  \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                $cnv coords "${tag}_BASE" \
                    $xpos                        $ypos                  \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]

                set crossw [expr abs(int($xnew/30)) + 2]
                set x0 [expr $xpos +          $crossw  * $zoom]
                set y0 [expr $ypos +          $crossw  * $zoom]
                set x1 [expr $xpos + ($xnew - $crossw) * $zoom]
                set y1 [expr $ypos + ($ynew - $crossw) * $zoom]
                $cnv coords "${tag}_X1" $x0 $y0 $x1 $y1
                $cnv coords "${tag}_X2" $x0 $y1 $x1 $y0
                $cnv itemconfigure ${tag}_X -width [expr $crossw - 1]
                set recreate_iolets 1
            } "-bcolor" {
                $cnv itemconfigure "${tag}_BASE" -fill $v
            } "-fcolor" {
                if { [$cnv itemcget "${tag}_X" -fill] ne {} } {
                    $cnv itemconfigure "${tag}_X" -fill $v
                }
            }

        }
    }
    $cnv itemconfigure "${tag}_BASE" -width $zoom

    if {$recreate_iolets} {
        ::pdwidget::refresh_iolets $obj
    }
}

########################################################################
# object-specific procs from the core
#
# for whatever reasons, we use per-object namespaces
namespace eval ::pdwidget::radio:: { }
proc ::pdwidget::radio::activate {obj state activecolor} {
    set tag "[::pdwidget::base_tag $obj]"
    foreach cnv [::pdwidget::get_canvases $obj] {
        # pass the 'activated' tag to the newly selected button
        $cnv dtag "${tag}" "X"
        set id [lindex [$cnv find withtag ${tag}_KNOB] $state]
        if { $id ne {} } {
            $cnv addtag "${tag}_X" withtag $id
        }
        # show (de)activation
        $cnv itemconfigure "${tag}_KNOB" -fill {} -outline {}
        $cnv itemconfigure "${tag}_X" -fill $activecolor -outline $activecolor
    }
}

namespace eval ::pdwidget::bang:: { }
proc ::pdwidget::bang::activate {obj state activecolor} {
    # LATER: have the timer work on the GUI side!
    set tag "[::pdwidget::base_tag $obj]_BUTTON"
    if {! $state} {
        set activecolor {}
    }
    foreach cnv [::pdwidget::get_canvases $obj] {
        $cnv itemconfigure $tag -fill $activecolor
    }
}

namespace eval ::pdwidget::toggle:: { }
proc ::pdwidget::toggle::activate {obj state activecolor} {
    set tag "[::pdwidget::base_tag $obj]"
    set tag "${tag}_X"
    if {! $state} {
        set activecolor {}
    }
    foreach cnv [::pdwidget::get_canvases $obj] {
        $cnv itemconfigure $tag -fill $activecolor
    }
}



########################################################################
# register the new objects
::pdwidget::register bang ::pdwidget::iemgui::create_bang
::pdwidget::register canvas ::pdwidget::iemgui::create_canvas
::pdwidget::register radio ::pdwidget::iemgui::create_radio
::pdwidget::register toggle ::pdwidget::iemgui::create_toggle
