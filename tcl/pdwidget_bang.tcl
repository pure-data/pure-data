## infrastructure for drawing Pd "widgets" (graphical objects)
package provide pdwidget_bang 0.1


namespace eval ::pd::widget::bang:: {

}

proc ::pd::widget::bang::create {obj cnv posX posY} {
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} BASE]
    $cnv create oval 0 0 0 0 -tags [list ${tag} BUTTON]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}INLET] -outline {} -fill {}
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}OUTLET] -outline {} -fill {}
    $cnv create text 0 0 -anchor w -tags [list ${tag} label text]
    $cnv move $tag $posX $posY

    ::pd::widget::widgetbehaviour $obj config ::pd::widget::bang::config
    ::pd::widget::widgetbehaviour $obj select ::pd::widget::bang::select
}
proc ::pd::widget::bang::config {obj args} {
    set options [::pd::widget::parseargs \
                     {
                         -labelpos 2
                         -size 2
                         -colors 3
                         -font 2
                         -label 1
                     } $args]
    set tag [::pd::widget::base_tag $obj]

foreach cnv [::pd::widget::get_canvases $obj] {
    set zoom [::pd::canvas::get_zoom $cnv]
    set iow $::pd::widget::IOWIDTH
    set ih [expr $::pd::widget::IHEIGHT - 0.5]
    set oh [expr $::pd::widget::OHEIGHT - 1]
    dict for {k v} $options {
        foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
        switch -exact -- $k {
            default {
            } "-labelpos" {
                set xnew [lindex $v 0]
                set ynew [lindex $v 1]
                $cnv coords "${tag}&&label" \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
            } "-size" {
                set xnew [lindex $v 0]
                set ynew [lindex $v 1]
                $cnv coords "${tag}" \
                          $xpos                        $ypos                  \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                $cnv coords "${tag}&&BASE" \
                          $xpos                        $ypos                  \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                $cnv coords "${tag}&&BUTTON" \
                    [expr $xpos +                 $zoom] [expr $ypos +                 $zoom] \
                    [expr $xpos + ($xnew - 1.5) * $zoom] [expr $ypos + ($ynew - 1.5) * $zoom]

                $cnv coords "${tag}INLET" \
                          $xpos                       $ypos                          \
                    [expr $xpos + $iow * $zoom] [expr $ypos +           $ih * $zoom]
                $cnv coords "${tag}OUTLET" \
                          $xpos                 [expr $ypos + ($ynew - $oh) * $zoom] \
                    [expr $xpos + $iow * $zoom] [expr $ypos + ($ynew      ) * $zoom]
            } "-colors" {
                set color [lindex $v 0]
                $cnv itemconfigure "${tag}&&BASE" -fill $color
                if { [$cnv itemcget "${tag}&&BUTTON" -fill] ne {} } {
                    set color [lindex $v 1]
                    $cnv itemconfigure "${tag}&&BUTTON" -fill $color
                }
                set color [lindex $v 2]
                $cnv itemconfigure "${tag}&&label" -fill $color
            } "-font" {
                set fontweight $::font_weight
                set font [lindex $v 0]
                set fontsize [lindex $v 1]
                set fontsize [expr -int($fontsize) * $zoom]
                $cnv itemconfigure "${tag}&&label" -font [list $font $fontsize $fontweight]
            } "-label" {
                pdtk_text_set $cnv "${tag}&&label" $v
            }

        }
        $cnv itemconfigure "${tag}&&BASE" -width $zoom
        $cnv itemconfigure "${tag}&&BUTTON" -width $zoom
    }
}
}
proc ::pd::widget::bang::select {obj state} {
    # if selection is activated, use a different color
    # TODO: label

    if {$state != 0} {
        set color #0000FF
    } else {
        set color #000000
    }
    set tag "[::pd::widget::base_tag $obj]"
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv itemconfigure "${tag}&&BASE" -outline $color
        $cnv itemconfigure "${tag}&&BUTTON" -outline $color
    }
}

proc ::pd::widget::bang::activate {obj state activecolor} {
    # LATER: have the timer work on the GUI side!
    set tag "[::pd::widget::base_tag $obj]&&BUTTON"
    if {! $state} {
        set activecolor {}
    }
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv itemconfigure $tag -fill $activecolor
    }
}



::pd::widget::register bang ::pd::widget::bang::create
