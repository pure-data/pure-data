## infrastructure for drawing Pd "widgets" (graphical objects)


# TODO: ZOOM
# zoom is currently ignored (the canvas should now the zoom state).
#   even better if https://github.com/pure-data/pure-data/pull/1659 gets accepted,
#   and the core doesn't know anything about zoom any more
#
#   until then:
#   - apart from coordinate scaling, the BASE should adjust it's 'width' with the zoom level
#     this also involves a slight repositioning of BASE

package provide pdwidget_canvas 0.1


namespace eval ::pd::widget::canvas:: {

}

proc printme {args} {
    ::pdwindow::error "${args}\n"
}
proc debug {args} {
    ::pdwindow::error "${args}\n"
    {*}$args
}
proc ::pd::widget::canvas::create {obj cnv} {
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}RECT]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}BASE]
    $cnv create text 0 0 -anchor w -tags [list ${tag} ${tag}LABEL label text]

    ::pd::widget::widgetbehaviour $obj config ::pd::widget::canvas::config
    ::pd::widget::widgetbehaviour $obj select ::pd::widget::canvas::select
}
proc ::pd::widget::canvas::config {obj args} {
    set options [::pd::widget::parseargs \
                     {
                         -labelpos 2
                         -size 2
                         -visible 2
                         -colors 3
                         -font 2
                         -label 1
                     } $args]
    set tag [::pd::widget::base_tag $obj]

foreach cnv [::pd::widget::get_canvases $obj] {
    set zoom [::pdtk_canvas::get_zoom $cnv]
    set offset 0
    if {$zoom > 1} {
        set offset $zoom
    }

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
                    [expr $xpos + $offset] [expr $ypos + $offset] \
                    [expr $xpos + $xnew * $zoom + $offset] [expr $ypos + $ynew * $zoom + $offset]
            } "-visible" {
                set w [lindex $v 0]
                set h [lindex $v 1]
                $cnv coords "${tag}RECT" $xpos $ypos [expr $xpos + $w * $zoom] [expr $ypos + $h * $zoom]
            } "-colors" {
                set color [lindex $v 0]
                $cnv itemconfigure "${tag}RECT" -fill $color -outline $color
                $cnv itemconfigure "${tag}BASE" -width 1 -outline {}
                # set unused_color [lindex $v 1]
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
    }
    $cnv itemconfigure "${tag}BASE" -width $zoom
    # TODO: in the original code, BASE was shifted by $offset to 'keep zoomed border inside visible area'
}
}
proc ::pd::widget::canvas::select {obj state} {
    # get the normal color from the object's state
    # if selection is activated, use a different color
    if {$state != 0} {
        set color #0000FF
    } else {
        set color {}
    }
    set tag "[::pd::widget::base_tag $obj]BASE"
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv itemconfigure ${tag} -outline $color
    }
}


::pd::widget::register canvas ::pd::widget::canvas::create
