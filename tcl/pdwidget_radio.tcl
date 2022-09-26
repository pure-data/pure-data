## infrastructure for drawing Pd "widgets" (graphical objects)
package provide pdwidget_radio 0.1


namespace eval ::pd::widget::radio:: {

}

proc ::pd::widget::radio::create {obj cnv posX posY} {
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} BASE0]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}INLET] -outline {} -fill {}
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}OUTLET] -outline {} -fill {}
    $cnv create text 0 0 -anchor w -tags [list ${tag} label text]
    $cnv move $tag $posX $posY

    ::pd::widget::widgetbehaviour $obj config ::pd::widget::radio::config
    ::pd::widget::widgetbehaviour $obj select ::pd::widget::radio::select
}
proc ::pd::widget::radio::_recreate_buttons {cnv obj numX numY} {
    # create an array of numX*numY buttons
    set tag [::pd::widget::base_tag $obj]
    $cnv delete ${tag}&&RADIO
    for {set x 0} {$x < $numX} {incr x} {
        for {set y 0} {$y < $numY} {incr y} {
            $cnv create rectangle 0 0 0 0 -fill {} -outline {} \
                -tags [list ${tag} RADIO BASE GRID${x}x${y}]
            $cnv create rectangle 0 0 0 0 -fill {} -outline {} \
                -tags [list ${tag} RADIO KNOB GRID${x}x${y}]
        }
    }
    $cnv lower "${tag}&&RADIO" "${tag}&&BASE0"
    $cnv raise "${tag}&&RADIO" "${tag}&&BASE0"
}

proc ::pd::widget::radio::_getactive {cnv tag} {
    set t {}
    foreach t [$cnv find withtag ${tag}&&X] {break}
    if { $t eq {} } {return}
    # we have an active button, but which one is it?
    set i 0
    foreach t [$cnv find withtag ${tag}&&KNOB] {
        if {[lsearch -exact [$cnv gettags $t] X] >= 0} {
            # found it
            return [list $i [$cnv itemcget $t -fill]]
        }
        incr i
    }
}

proc ::pd::widget::radio::_reconfigure_buttons {cnv obj zoom} {
    set tag [::pd::widget::base_tag $obj]
    foreach {xpos ypos w h} [$cnv coords "${tag}&&BASE0"] {break}
    set w [expr $w - $xpos]
    set h [expr $h - $ypos]

    set knob_x0 [expr $w / 4 - 0.5]
    set knob_y0 [expr $h / 4 - 0.5]
    set knob_x1 [expr $w - $knob_x0]
    set knob_y1 [expr $h - $knob_y0]

    # get the coord of the outlet...
    set ylast $ypos

    # resize/reposition the buttons
    foreach id [$cnv find withtag "${tag}&&KNOB"] {
        set x {}
        set y {}
        foreach t [$cnv gettags $id] {
            if { [regexp {^GRID(.+)x(.+)} $t _ x y] } {
                break;
            }
        }
        if { $x eq {} || $y eq {} } { break }
        set xy "${x}x${y}"
        set ylast [expr $h * $y]
        $cnv coords "${tag}&&BASE&&GRID${xy}" 0 0 $w $h
        $cnv coords "${tag}&&KNOB&&GRID${xy}" $knob_x0 $knob_y0 $knob_x1 $knob_y1
        $cnv move "${tag}&&GRID${xy}" [expr $w * $x] $ylast
    }
    $cnv itemconfigure "${tag}&&BASE" -outline black
    $cnv move "${tag}&&RADIO" $xpos $ypos

    ## reposition the iolets

    # these need to be scaled
    set iow [expr $::pd::widget::IOWIDTH * $zoom]
    set ih [expr ($::pd::widget::IHEIGHT - 0.5) * $zoom]
    set oh [expr ($::pd::widget::OHEIGHT -   1) * $zoom]

    $cnv coords "${tag}INLET" 0 0 $iow $ih
    $cnv coords "${tag}OUTLET" 0 0 $iow $oh
    $cnv move "${tag}INLET" $xpos $ypos
    $cnv move "${tag}OUTLET" $xpos [expr $ypos + $ylast + $h - $oh]

}

proc ::pd::widget::radio::config {obj args} {
    set options [::pd::widget::parseargs \
                     {
                         -labelpos 2
                         -size 2
                         -colors 3
                         -font 2
                         -label 1
                         -number 2
                     } $args]
    set tag [::pd::widget::base_tag $obj]
    set canvases [::pd::widget::get_canvases $obj]

    # which button is activated?
    set active {}
    set activecolor {}
    set bgcolor {}
    foreach cnv $canvases {
        foreach {active activecolor} [::pd::widget::radio::_getactive $cnv $tag] {break}
        set bgcolor [$cnv itemcget "${tag}&&BASE" -fill]
        # we only check a single canvas (they are all the same)
        break;
    }
    foreach cnv $canvases {
        set zoom [::pd::canvas::get_zoom $cnv]
        dict for {k v} $options {
            foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
            switch -exact -- $k {
                default {
                } "-labelpos" {
                    set xnew [lindex $v 0]
                    set ynew [lindex $v 1]
                    $cnv coords "${tag}&&label" \
                        [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                } "-font" {
                    set fontweight $::font_weight
                    set font [lindex $v 0]
                    set fontsize [lindex $v 1]
                    set fontsize [expr -int($fontsize) * $zoom]
                    $cnv itemconfigure "${tag}&&label" -font [list $font $fontsize $fontweight]
                } "-label" {
                    pdtk_text_set $cnv "${tag}&&label" $v
                } "-colors" {
                    set bgcolor [lindex $v 0]
                    set activecolor [lindex $v 1]
                    set labelcolor [lindex $v 2]
                    $cnv itemconfigure "${tag}&&label" -fill $labelcolor
                } "-size" {
                    set w [lindex $v 0]
                    set h [lindex $v 1]
                    foreach {x y} [$cnv coords "${tag}&&BASE0"] {break}
                    $cnv coords "${tag}&&BASE0" $x $y [expr $x + $w * $zoom] [expr $y + $h * $zoom]
                    ::pd::widget::radio::_reconfigure_buttons $cnv $obj $zoom
                } "-number" {
                    set xnew [lindex $v 0]
                    set ynew [lindex $v 1]
                    ::pd::widget::radio::_recreate_buttons $cnv $obj $xnew $ynew
                    ::pd::widget::radio::_reconfigure_buttons $cnv $obj $zoom
                }

            }
        }
        $cnv itemconfigure "${tag}&&BASE" -width $zoom -fill $bgcolor
    }
    if { $active ne {} } {
        ::pd::widget::radio::activate $obj $active $activecolor
    }
}
proc ::pd::widget::radio::select {obj state} {
    # if selection is activated, use a different color
    # TODO: label

    if {$state != 0} {
        set color #0000FF
    } else {
        set color #000000
    }
    set tag "[::pd::widget::base_tag $obj]&&BASE"
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv itemconfigure "${tag}" -outline $color
    }
}

proc ::pd::widget::radio::activate {obj state activecolor} {
    set tag "[::pd::widget::base_tag $obj]"
    foreach cnv [::pd::widget::get_canvases $obj] {
        # pass the 'activated' tag to the newly selected button
        $cnv dtag "${tag}" "X"
        set id [lindex [$cnv find withtag ${tag}&&KNOB] $state]
        if { $id ne {} } {
            $cnv addtag "X" withtag $id
        }
        # show (de)activation
        $cnv itemconfigure "${tag}&&KNOB" -fill {} -outline {}
        $cnv itemconfigure "${tag}&&X" -fill $activecolor -outline $activecolor
    }
}



::pd::widget::register radio ::pd::widget::radio::create
