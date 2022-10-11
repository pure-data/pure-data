## core Pd "widgets" (graphical objects)
# - objects
# - messages
# - gatoms
# - comments
# - connections



package provide pdwidget_core 0.1
namespace eval ::pdwidget::core:: { }

########################################################################
# common procedures
proc ::pdwidget::core::select {obj cnv state} {
    # if selection is activated, use a different color
    # TODO: label

    if {$state != 0} {
        set color #0000FF
    } else {
        set color #000000
    }
    set tag "[::pdwidget::base_tag $obj]"

    $cnv itemconfigure "${tag}&&OUTLINE" -outline $color
    $cnv itemconfigure "${tag}&&text" -fill $color
    $cnv itemconfigure "${tag}&&cord" -fill $color
}
proc ::pdwidget::core::textselect {obj cnv {start {}} {length {}}} {
    set tag "[::pdwidget::base_tag $obj]&&text"

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


########################################################################
# object

proc ::pdwidget::core::create_obj {obj cnv posX posY} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} OUTLINE]
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_obj
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    ::pdwidget::widgetbehavior $obj textselect ::pdwidget::core::textselect
}

proc ::pdwidget::core::config_obj {obj cnv args} {
    set options [::pdwidget::parseargs \
                     {
                         -size 2
                         -text 1
                         -state 1
                     } $args]
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2
    set sizechanged 0

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
                        $cnv itemconfigure "${tag}&&OUTLINE" -dash "-"
                    } "edit" {
                        $cnv itemconfigure "${tag}&&OUTLINE" -dash "."
                    }
                }
            }

        }
        $cnv itemconfigure "${tag}&&OUTLINE" -width $zoom
    }
    if { $sizechanged } {
        ::pdwidget::refresh_iolets $obj
    }
}

########################################################################
# message

proc ::pdwidget::core::create_msg {obj cnv posX posY} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create polygon 0 0 -tags [list ${tag} OUTLINE] -fill {} -outline black
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_msg
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    ::pdwidget::widgetbehavior $obj textselect ::pdwidget::core::textselect
}
proc ::pdwidget::core::config_msg {obj cnv args} {
    set options [::pdwidget::parseargs \
                     {
                         -size 2
                         -text 1
                     } $args]
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2

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

namespace eval ::pdwidget::message:: { }
proc ::pdwidget::message::_activate {obj width} {
    set tag "[::pdwidget::base_tag $obj]&&OUTLINE"
    foreach cnv [::pdwidget::get_canvases $obj] {
        set zoom [::pd::canvas::get_zoom $cnv]
        $cnv itemconfigure $tag -width [expr $width * $zoom]
    }
}

proc ::pdwidget::message::activate {obj flashtime} {
    ::pdwidget::message::_activate $obj 5
    after 120 [list ::pdwidget::message::_activate $obj 1]
}


########################################################################
# gatom

proc ::pdwidget::core::create_gatom {obj cnv posX posY} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create polygon 0 0 -tags [list ${tag} OUTLINE] -fill {} -outline black
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} label] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_gatom
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    ::pdwidget::widgetbehavior $obj textselect ::pdwidget::core::textselect
}
proc ::pdwidget::core::config_gatom {obj cnv args} {
    set options [::pdwidget::parseargs \
                     {
                         -size 2
                         -fontsize 1
                         -labelpos 1
                         -label 1
                         -text 1
                     } $args]
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2
    set sizechanged 0

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
            } "-fontsize" {
                set font [$cnv itemcget "${tag}&&text" -font]
                lset font 1 -${v}
                set font [get_font_for_size [expr $v * $zoom] ]
                puts $font
                $cnv itemconfigure "${tag}&&text" -font $font
                $cnv itemconfigure "${tag}&&label" -font $font
            } "-label" {
                pdtk_text_set $cnv "${tag}&&label" "$v"
            } "-labelpos" {
                set txt [$cnv itemcget "${tag}&&label" -text]
                set txtlen [string length $txt]
                foreach {x1 y1 x2 y2} [$cnv coords "${tag}"] {break}
                switch -exact -- $v {
                    "left" {
                        $cnv itemconfigure "${tag}&&label" -anchor ne
                        $cnv coords "${tag}&&label" [expr $x1 - 3 * $zoom] [expr $y1 + 2 * $zoom]
                    } "right" {
                        $cnv itemconfigure "${tag}&&label" -anchor nw
                        $cnv coords "${tag}&&label" [expr $x2 + 2 * $zoom] [expr $y1 + 2 * $zoom]
                    } "top" {
                        $cnv itemconfigure "${tag}&&label" -anchor sw
                        $cnv coords "${tag}&&label" [expr $x1 - 1 * $zoom] [expr $y1 - 1 * $zoom]
                    } "bottom" {
                        $cnv itemconfigure "${tag}&&label" -anchor nw
                        $cnv coords "${tag}&&label" [expr $x1 - 1 * $zoom] [expr $y2 + 3 * $zoom]
                    }
                }
            } "-text" {
                pdtk_text_set $cnv "${tag}&&text" "$v"
            }

        }
        $cnv itemconfigure "${tag}&&OUTLINE" -width $zoom
    }
    if { $sizechanged } {
        ::pdwidget::refresh_iolets $obj
    }
}

# list atoms
proc ::pdwidget::core::create_latom {obj cnv posX posY} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create polygon 0 0 -tags [list ${tag} OUTLINE] -fill {} -outline black
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_latom
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    ::pdwidget::widgetbehavior $obj textselect ::pdwidget::core::textselect
}
proc ::pdwidget::core::config_latom {obj cnv args} {
    set options [::pdwidget::parseargs \
                     {
                         -size 2
                         -text 1
                     } $args]
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2
    set sizechanged 0

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
    if { $sizechanged } {
        ::pdwidget::refresh_iolets $obj
    }
}
########################################################################
# comment

proc ::pdwidget::core::create_comment {obj cnv posX posY} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create line 0 0 0 0 -tags [list ${tag} commentbar] -fill {}
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    $cnv create rectangle 0 0 0 0 -tags [list $tag iolets] -outline {} -fill {} -width 0
    pdtk_text_new $cnv [list ${tag} text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_comment
    ::pdwidget::widgetbehavior $obj editmode ::pdwidget::core::edit_comment
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    ::pdwidget::widgetbehavior $obj textselect ::pdwidget::core::textselect
}

proc ::pdwidget::core::config_comment {obj cnv args} {
    set options [::pdwidget::parseargs \
                     {
                         -size 2
                         -text 1
                     } $args]
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2
    set sizechanged 0

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
    if { $sizechanged } {
        ::pdwidget::refresh_iolets $obj
    }
}
proc ::pdwidget::core::edit_comment {obj cnv state} {
    if {$state != 0} {
        set color #000000
    } else {
        set color ""
    }
    set tag "[::pdwidget::base_tag $obj]&&commentbar"
    $cnv itemconfigure "${tag}" -fill $color
}

########################################################################
# connection

proc ::pdwidget::core::create_conn {obj cnv posX posY} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    # the 'iolets' tag might be expected by some helper scripts, but it not really needed...
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} iolets] -outline {} -fill {} -width 0
    $cnv create line 0 0 0 0 -tags [list ${tag} cord] -fill black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_conn
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
}
proc ::pdwidget::core::config_conn {obj cnv args} {
    set options [::pdwidget::parseargs \
                     {
                         -position 4
                         -text 1
                         -type 1
                     } $args]
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2

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


########################################################################
# selection lasso

proc ::pdwidget::core::create_sel {obj cnv posX posY} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    # the 'iolets' tag might be expected by some helper scripts, but it not really needed...
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} lasso iolets] -outline black -fill {} -width 1

    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_sel
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
}
proc ::pdwidget::core::config_sel {obj cnv args} {
    set options [::pdwidget::parseargs \
                     {
                         -size 2
                     } $args]
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2

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


########################################################################
## register objects
::pdwidget::register object ::pdwidget::core::create_obj
::pdwidget::register message ::pdwidget::core::create_msg
::pdwidget::register floatatom ::pdwidget::core::create_gatom
::pdwidget::register symbolatom ::pdwidget::core::create_gatom
::pdwidget::register listatom ::pdwidget::core::create_latom
::pdwidget::register comment ::pdwidget::core::create_comment
::pdwidget::register connection ::pdwidget::core::create_conn
::pdwidget::register selection ::pdwidget::core::create_sel
