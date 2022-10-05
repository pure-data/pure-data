## infrastructure for drawing Pd "widgets" (graphical objects)
package provide pdwidget_object 0.1


namespace eval ::pd::widget::object:: {

}

# stack order
# - boundingrectangle
# - outlets
# - inlets
# - text

proc ::pd::widget::object::create {obj cnv posX posY} {
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} RECT]
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag $posX $posY

    ::pd::widget::widgetbehavior $obj config ::pd::widget::object::config
    ::pd::widget::widgetbehavior $obj select ::pd::widget::object::select
    ::pd::widget::widgetbehavior $obj textselect ::pd::widget::object::textselect
}
proc ::pd::widget::object::config {obj args} {
    set options [::pd::widget::parseargs \
                     {
                         -size 2
                         -text 1
                     } $args]
    set tag [::pd::widget::base_tag $obj]
    set tmargin 3
    set lmargin 2
    set sizechanged 0

    foreach cnv [::pd::widget::get_canvases $obj] {
        set zoom [::pd::canvas::get_zoom $cnv]
        dict for {k v} $options {
            foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
            switch -exact -- $k {
                default {
                } "-size" {
                    set xnew [lindex $v 0]
                    set ynew [lindex $v 1]
                    set sizechanged 1
                    $cnv coords "${tag}" \
                        $xpos                        $ypos                  \
                        [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                    $cnv coords "${tag}&&RECT" \
                        $xpos                        $ypos                  \
                        [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                    $cnv coords "${tag}&&text" \
                        [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
                } "-text" {
                    pdtk_text_set $cnv "${tag}&&text" "$v"
                }

            }
            $cnv itemconfigure "${tag}&&RECT" -width $zoom
        }
    }
    if { $sizechanged } {
        ::pd::widget::refresh_iolets $obj
    }
}
proc ::pd::widget::object::select {obj state} {
    # if selection is activated, use a different color
    # TODO: label

    if {$state != 0} {
        set color #0000FF
    } else {
        set color #000000
    }
    set tag "[::pd::widget::base_tag $obj]"
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv itemconfigure "${tag}&&RECT" -outline $color
    }
}

proc ::pd::widget::object::textselect {obj {start {}} {length {}}} {
    set tag "[::pd::widget::base_tag $obj]&&text"
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv select clear
        if { $start ne {} } {
            if { $length > 0 } {
                $cnv select from $tag $start
                $cnv select to $tag [expr $start + $length - 1]
                $cnv focus {}
            } else {
                $cnv icursor $tag $start
                focus $cnv
                $cnv focus $tag
            }
        }
    }
}


::pd::widget::register object ::pd::widget::object::create
