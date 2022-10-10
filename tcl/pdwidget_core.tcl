## core Pd "widgets" (graphical objects)
# - objects
# - messages
# - gatoms
# - comments
# - connections



package provide pdwidget_core 0.1
namespace eval ::pd::widget::core:: { }

########################################################################
# common procedures
proc ::pd::widget::core::select {obj state} {
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
        $cnv itemconfigure "${tag}&&text" -fill $color
        $cnv itemconfigure "${tag}&&cord" -fill $color
    }
}
proc ::pd::widget::core::textselect {obj {start {}} {length {}}} {
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


########################################################################
# object

proc ::pd::widget::core::create_obj {obj cnv posX posY} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} OUTLINE]
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pd::widget::widgetbehavior $obj config ::pd::widget::core::config_obj
    ::pd::widget::widgetbehavior $obj select ::pd::widget::core::select
    ::pd::widget::widgetbehavior $obj textselect ::pd::widget::core::textselect
}

proc ::pd::widget::core::config_obj {obj args} {
    set options [::pd::widget::parseargs \
                     {
                         -size 2
                         -text 1
                         -state 1
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
                    $cnv coords "${tag}&&OUTLINE" \
                        $xpos                        $ypos                  \
                        [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                    $cnv coords "${tag}&&text" \
                        [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
                } "-text" {
                    pdtk_text_set $cnv "${tag}&&text" "$v"
                } "-state" {
                    switch -exact -- $v {
                        "normal" {
                            $cnv itemconfigure "${tag}&&OUTLINE" -dash ""
                        } "broken" {
                            $cnv itemconfigure "${tag}&&OUTLINE" -dash "."
                        } "edit" {
                            $cnv itemconfigure "${tag}&&OUTLINE" -dash "-"
                        }
                    }
                }

            }
            $cnv itemconfigure "${tag}&&OUTLINE" -width $zoom
        }
    }
    if { $sizechanged } {
        ::pd::widget::refresh_iolets $obj
    }
}

########################################################################
# message

proc ::pd::widget::core::create_msg {obj cnv posX posY} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create polygon 0 0 -tags [list ${tag} OUTLINE] -fill {} -outline black
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pd::widget::widgetbehavior $obj config ::pd::widget::core::config_msg
    ::pd::widget::widgetbehavior $obj select ::pd::widget::core::select
}
proc ::pd::widget::core::config_msg {obj args} {
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

namespace eval ::pd::widget::message:: { }
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


########################################################################
# gatom

proc ::pd::widget::core::create_gatom {obj cnv posX posY} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create polygon 0 0 -tags [list ${tag} OUTLINE] -fill {} -outline black
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pd::widget::widgetbehavior $obj config ::pd::widget::core::config_gatom
    ::pd::widget::widgetbehavior $obj select ::pd::widget::core::select
    ::pd::widget::widgetbehavior $obj textselect ::pd::widget::core::textselect
}
proc ::pd::widget::core::config_gatom {obj args} {
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
                    set sizechanged 1
                    set xnew [lindex $v 0]
                    set ynew [lindex $v 1]
                    set corner [expr $ynew/4]
                    set xpos2 [expr $xpos + $xnew * $zoom]
                    set ypos2 [expr $ypos + $ynew * $zoom]
                    set corner [expr $corner * $zoom]


                    $cnv coords "${tag}" \
                        $xpos $ypos  $xpos2 $ypos2
                    $cnv coords "${tag}&&OUTLINE" \
                              $xpos                   $ypos \
                        [expr $xpos2 - $corner]       $ypos \
                              $xpos2            [expr $ypos + $corner] \
                              $xpos2                  $ypos2 \
                              $xpos                   $ypos2 \
                              $xpos                   $ypos
                    $cnv coords "${tag}&&text" \
                        [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
                } "-text" {
                    pdtk_text_set $cnv "${tag}&&text" "$v"
                }

            }
            $cnv itemconfigure "${tag}&&OUTLINE" -width $zoom
        }
    }
    if { $sizechanged } {
        ::pd::widget::refresh_iolets $obj
    }
}

# list atoms
proc ::pd::widget::core::create_latom {obj cnv posX posY} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create polygon 0 0 -tags [list ${tag} OUTLINE] -fill {} -outline black
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pd::widget::widgetbehavior $obj config ::pd::widget::core::config_latom
    ::pd::widget::widgetbehavior $obj select ::pd::widget::core::select
    ::pd::widget::widgetbehavior $obj textselect ::pd::widget::core::textselect
}
proc ::pd::widget::core::config_latom {obj args} {
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
                    set sizechanged 1
                    set xnew [lindex $v 0]
                    set ynew [lindex $v 1]
                    set corner [expr $ynew/4]
                    set xpos2 [expr $xpos + $xnew * $zoom]
                    set ypos2 [expr $ypos + $ynew * $zoom]
                    set corner [expr $corner * $zoom]


                    $cnv coords "${tag}" \
                        $xpos $ypos  $xpos2 $ypos2
                    $cnv coords "${tag}&&OUTLINE" \
                              $xpos                   $ypos \
                        [expr $xpos2 - $corner]       $ypos \
                              $xpos2            [expr $ypos + $corner] \
                              $xpos2            [expr $ypos2 - $corner] \
                        [expr $xpos2 - $corner]       $ypos2 \
                              $xpos                   $ypos2 \
                              $xpos                   $ypos
                    $cnv coords "${tag}&&text" \
                        [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
                } "-text" {
                    pdtk_text_set $cnv "${tag}&&text" "$v"
                }

            }
            $cnv itemconfigure "${tag}&&OUTLINE" -width $zoom
        }
    }
    if { $sizechanged } {
        ::pd::widget::refresh_iolets $obj
    }
}
########################################################################
# comment

proc ::pd::widget::core::create_comment {obj cnv posX posY} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pd::widget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create line 0 0 0 0 -tags [list ${tag} commentbar] -fill {}
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pd::widget::widgetbehavior $obj config ::pd::widget::core::config_comment
    ::pd::widget::widgetbehavior $obj editmode ::pd::widget::core::edit_comment
    ::pd::widget::widgetbehavior $obj select ::pd::widget::core::select
    ::pd::widget::widgetbehavior $obj textselect ::pd::widget::core::textselect
}

proc ::pd::widget::core::config_comment {obj args} {
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
                    $cnv coords "${tag}&&commentbar" \
                        [expr $xpos + $xnew * $zoom] $ypos \
                        [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                    $cnv coords "${tag}&&text" \
                        [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
                } "-text" {
                    pdtk_text_set $cnv "${tag}&&text" "$v"
                }

            }
            $cnv itemconfigure "${tag}&&commentbar" -width $zoom
        }
    }
    if { $sizechanged } {
        ::pd::widget::refresh_iolets $obj
    }
}
proc ::pd::widget::core::edit_comment {obj state} {
    if {$state != 0} {
        set color #000000
    } else {
        set color ""
    }
    set tag "[::pd::widget::base_tag $obj]&&commentbar"
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv itemconfigure "${tag}" -fill $color
    }

}

########################################################################
# connection

proc ::pd::widget::core::create_conn {obj cnv posX posY} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pd::widget::base_tag $obj]
    # the 'iolets' tag might be expected by some helper scripts, but it not really needed...
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} iolets] -outline {} -fill {} -width 0
    $cnv create line 0 0 0 0 -tags [list ${tag} cord] -fill black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pd::widget::widgetbehavior $obj config ::pd::widget::core::config_conn
    ::pd::widget::widgetbehavior $obj select ::pd::widget::core::select
}
proc ::pd::widget::core::config_conn {obj args} {
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


########################################################################
# selection lasso

proc ::pd::widget::core::create_sel {obj cnv posX posY} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pd::widget::base_tag $obj]
    # the 'iolets' tag might be expected by some helper scripts, but it not really needed...
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} lasso iolets] -outline black -fill {} -width 1

    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pd::widget::widgetbehavior $obj config ::pd::widget::core::config_sel
    ::pd::widget::widgetbehavior $obj select ::pd::widget::core::select
}
proc ::pd::widget::core::config_sel {obj args} {
    set options [::pd::widget::parseargs \
                     {
                         -size 2
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
                    foreach {xnew ynew} $v {break}
                    $cnv coords "${tag}&&lasso" \
                        $xpos $ypos \
                        [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                }
            }
        }
    }
}


########################################################################
## register objects
::pd::widget::register object ::pd::widget::core::create_obj
::pd::widget::register message ::pd::widget::core::create_msg
::pd::widget::register floatatom ::pd::widget::core::create_gatom
::pd::widget::register symbolatom ::pd::widget::core::create_gatom
::pd::widget::register listatom ::pd::widget::core::create_latom
::pd::widget::register comment ::pd::widget::core::create_comment
::pd::widget::register connection ::pd::widget::core::create_conn
::pd::widget::register selection ::pd::widget::core::create_sel
