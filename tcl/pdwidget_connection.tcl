## infrastructure for drawing Pd "widgets" (graphical connections)
package provide pdwidget_connection 0.1


namespace eval ::pd::widget::connection:: {

}

# stack order
# - boundingrectangle
# - outlets
# - inlets
# - text

proc ::pd::widget::connection::create {obj cnv posX posY} {
    set tag [::pd::widget::base_tag $obj]
    # the 'iolets' tag might be expected by some helper scripts, but it not really needed...
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} iolets] -outline {} -fill {} -width 0
    $cnv create line 0 0 0 0 -tags [list ${tag} cord] -fill black
    $cnv move $tag $posX $posY

    ::pd::widget::widgetbehaviour $obj config ::pd::widget::connection::config
    ::pd::widget::widgetbehaviour $obj select ::pd::widget::connection::select
}
proc ::pd::widget::connection::config {obj args} {
    set options [::pd::widget::parseargs \
                     {
                         -position 4
                         -text 1
                         -type 1
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
                } "-position" {
                    foreach {x0 y0 x1 y1} $v {break}
                    $cnv coords "${tag}&&cord" \
                        [expr $x0 * $zoom] [expr $y0 * $zoom] \
                        [expr $x1 * $zoom] [expr $y1 * $zoom]
                    $cnv coords "${tag}" [$cnv coords "${tag}&&cord"]
                } "-text" {
                    pdtk_text_set $cnv "${tag}&&text" "$v"
                } "-type" {
                    $cnv dtag "${tag}&&cord" signal
                    switch -exact $v {
                        "signal" {
                            $cnv addtag signal withtag "${tag}&&cord"
                        }
                    }
                }
            }

            $cnv itemconfigure "${tag}&&cord&&!signal" -width $zoom
            $cnv itemconfigure "${tag}&&cord&&signal" -width [expr $zoom * 2]
        }
    }
}
proc ::pd::widget::connection::select {obj state} {
    # if selection is activated, use a different color
    # TODO: label

    if {$state != 0} {
        set color #0000FF
    } else {
        set color #000000
    }
    set tag "[::pd::widget::base_tag $obj]"
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv itemconfigure "${tag}&&cord" -fill $color
    }
}

::pd::widget::register connection ::pd::widget::connection::create
