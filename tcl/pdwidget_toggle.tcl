## infrastructure for drawing Pd "widgets" (graphical objects)
package provide pdwidget_toggle 0.1


namespace eval ::pd::widget::toggle:: {

}

proc ::pd::widget::toggle::create {obj cnv posX posY} {
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}BASE]
    $cnv create line 0 0 0 0 -tags [list ${tag} ${tag}X ${tag}X1] -fill {}
    $cnv create line 0 0 0 0 -tags [list ${tag} ${tag}X ${tag}X2] -fill {}
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}INLET] -outline {} -fill {}
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}OUTLET] -outline {} -fill {}
    $cnv create text 0 0 -anchor w -tags [list ${tag} ${tag}LABEL label text]
    $cnv move $tag $posX $posY

    ::pd::widget::widgetbehaviour $obj config ::pd::widget::toggle::config
    ::pd::widget::widgetbehaviour $obj select ::pd::widget::toggle::select
}
proc ::pd::widget::toggle::config {obj args} {
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
                $cnv coords "${tag}LABEL" \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
            } "-size" {
                set xnew [lindex $v 0]
                set ynew [lindex $v 1]
                $cnv coords "${tag}BASE" \
                          $xpos                        $ypos                  \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]

                set crossw [expr abs(int($xnew/30)) + 2]
                set x0 [expr $xpos +          $crossw  * $zoom]
                set y0 [expr $ypos +          $crossw  * $zoom]
                set x1 [expr $xpos + ($xnew - $crossw) * $zoom]
                set y1 [expr $ypos + ($ynew - $crossw) * $zoom]
                $cnv coords "${tag}X1" $x0 $y0 $x1 $y1
                $cnv coords "${tag}X2" $x0 $y1 $x1 $y0
                $cnv itemconfigure ${tag}X -width [expr $crossw - 1]

                $cnv coords "${tag}INLET" \
                          $xpos                       $ypos                          \
                    [expr $xpos + $iow * $zoom] [expr $ypos +           $ih * $zoom]
                $cnv coords "${tag}OUTLET" \
                          $xpos                 [expr $ypos + ($ynew - $oh) * $zoom] \
                    [expr $xpos + $iow * $zoom] [expr $ypos + ($ynew      ) * $zoom]
            } "-colors" {
                set color [lindex $v 0]
                $cnv itemconfigure "${tag}BASE" -fill $color
                if { [$cnv itemcget "${tag}X" -fill] ne {} } {
                    set color [lindex $v 1]
                    $cnv itemconfigure "${tag}X" -fill $color
                }
                set color [lindex $v 2]
                $cnv itemconfigure "${tag}LABEL" -fill $color
            } "-font" {
                set fontweight $::font_weight
                set font [lindex $v 0]
                set fontsize [lindex $v 1]
                set fontsize [expr -int($fontsize) * $zoom]
                $cnv itemconfigure "${tag}LABEL" -font [list $font $fontsize $fontweight]
            } "-label" {
                pdtk_text_set $cnv "${tag}LABEL" $v
            }

        }
        $cnv itemconfigure "${tag}BASE" -width $zoom
    }
}
}
proc ::pd::widget::toggle::select {obj state} {
    # if selection is activated, use a different color
    # TODO: label

    if {$state != 0} {
        set color #0000FF
    } else {
        set color #000000
    }
    set tag "[::pd::widget::base_tag $obj]"
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv itemconfigure "${tag}BASE" -outline $color
    }
}

proc ::pd::widget::toggle::activate {obj state activecolor} {
    # LATER: have the timer work on the GUI side!
    set tag "[::pd::widget::base_tag $obj]"
    set tag "${tag}X"
    if {! $state} {
        set activecolor {}
    }
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv itemconfigure $tag -fill $activecolor
    }
}



::pd::widget::register toggle ::pd::widget::toggle::create
