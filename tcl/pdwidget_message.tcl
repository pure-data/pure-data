## infrastructure for drawing Pd "widgets" (graphical messages)
package provide pdwidget_message 0.1


namespace eval ::pd::widget::message:: {

}

# stack order
# - boundingrectangle
# - outlets
# - inlets
# - text

proc ::pd::widget::message::create {obj cnv posX posY} {
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create polygon 0 0 -tags [list ${tag} OUTLINE] -fill {} -outline black
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag $posX $posY

    ::pd::widget::widgetbehaviour $obj config ::pd::widget::message::config
    ::pd::widget::widgetbehaviour $obj select ::pd::widget::message::select
}
proc ::pd::widget::message::config {obj args} {
    set options [::pd::widget::parseargs \
                     {
                         -size 2
                         -text 1
                     } $args]
    set tag [::pd::widget::base_tag $obj]
    set tmargin 3
    set lmargin 2

    foreach cnv [::pd::widget::get_canvases $obj] {
        set zoom [::pd::canvas::get_zoom $cnv]
        dict for {k v} $options {
            foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
            switch -exact -- $k {
                default {
                } "-size" {
                    set xnew [lindex $v 0]
                    set ynew [lindex $v 1]
                    set corner [expr $ynew/4]
                    if {$corner > 10} {set corner 10}
                    set xnew [expr $xnew * $zoom]
                    set ynew [expr $ynew * $zoom]
                    set corner [expr $corner * $zoom]

                    $cnv coords "${tag}" \
                              $xpos                $ypos          \
                        [expr $xpos + $xnew] [expr $ypos + $ynew]
                    $cnv coords "${tag}&&OUTLINE"                                     \
                        $xpos                                $ypos                    \
                        [expr $xpos + $xnew + $corner] [expr $ypos                  ] \
                        [expr $xpos + $xnew          ] [expr $ypos         + $corner] \
                        [expr $xpos + $xnew          ] [expr $ypos + $ynew - $corner] \
                        [expr $xpos + $xnew + $corner] [expr $ypos + $ynew          ] \
                              $xpos                    [expr $ypos + $ynew          ] \
                        $xpos                                $ypos

                    $cnv coords "${tag}&&text" \
                        [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
                } "-text" {
                    pdtk_text_set $cnv "${tag}&&text" "$v"
                }

            }
            $cnv itemconfigure "${tag}&&OUTLINE" -width $zoom
        }
    }
}
proc ::pd::widget::message::select {obj state} {
    # if selection is activated, use a different color
    # TODO: label

    if {$state != 0} {
        set color #0000FF
    } else {
        set color #000000
    }
    set tag "[::pd::widget::base_tag $obj]"
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv itemconfigure "${tag}&&OUTLINE" -outline $color
    }
}

proc ::pd::widget::message::_activate {obj width} {
    set tag "[::pd::widget::base_tag $obj]&&OUTLINE"
    foreach cnv [::pd::widget::get_canvases $obj] {
        set zoom [::pd::canvas::get_zoom $cnv]
        $cnv itemconfigure $tag -width [expr $width * $zoom]
    }
}

proc ::pd::widget::message::activate {obj flashtime} {
    ::pd::widget::message::_activate $obj 5
    after 120 [list ::pd::widget::message::_activate $obj 1]
}


::pd::widget::register message ::pd::widget::message::create
