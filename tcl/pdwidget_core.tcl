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

    # -outline: rectangle, oval, polygon
    # -fill: line, text

    $cnv itemconfigure "${tag}_seloutline" -outline $color
    $cnv itemconfigure "${tag}_selfill" -fill $color
}
proc ::pdwidget::core::textselect {obj cnv {start {}} {length {}}} {
    set tag "[::pdwidget::base_tag $obj]_text"

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

proc ::pdwidget::core::create_obj {obj cnv posX posY args} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}_seloutline ${tag}_iolets]
    # the 'iolet' tag is used to properly place iolets in the display list
    # (inlets and outlets are to be placed directly above this tag)
    pdtk_text_new $cnv [list ${tag} ${tag}_text ${tag}_selfill text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_obj
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    ::pdwidget::widgetbehavior $obj textselect ::pdwidget::core::textselect
    if { $args ne {} } {::pdwidget::core::config_obj $obj $cnv {*}$args }
}

proc ::pdwidget::core::config_obj {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2
    set sizechanged 0

    set zoom [::pd::canvas::get_zoom $cnv]
    foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
    foreach {k v} $args {
        switch -exact -- $k {
            default {
            } "-size" {
                set xnew [lindex $v 0]
                set ynew [lindex $v 1]
                set sizechanged 1
                $cnv coords "${tag}" \
                    $xpos                        $ypos                  \
                    [expr $xpos + $xnew * $zoom] [expr $ypos + $ynew * $zoom]
                $cnv coords "${tag}_text" \
                    [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
            } "-text" {
                pdtk_text_set $cnv "${tag}_text" "$v"
            } "-state" {
                switch -exact -- $v {
                    "normal" {
                        $cnv itemconfigure "${tag}_seloutline" -dash ""
                    } "broken" {
                        $cnv itemconfigure "${tag}_seloutline" -dash "-"
                    } "edit" {
                        $cnv itemconfigure "${tag}_seloutline" -dash "."
                    }
                }
            }

        }
    }
    $cnv itemconfigure "${tag}_seloutline" -width $zoom
    if { $sizechanged } {
        ::pdwidget::refresh_iolets $obj
    }
}

########################################################################
# message

proc ::pdwidget::core::create_msg {obj cnv posX posY args} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create polygon 0 0 -tags [list ${tag} ${tag}_seloutline ${tag}_iolets iolets] -fill {} -outline black
    pdtk_text_new $cnv [list ${tag} ${tag}_text ${tag}_selfill text] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_msg
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    ::pdwidget::widgetbehavior $obj textselect ::pdwidget::core::textselect
    if { $args ne {} } {::pdwidget::core::config_msg $obj $cnv {*}$args }
}
proc ::pdwidget::core::config_msg {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2

    set zoom [::pd::canvas::get_zoom $cnv]
    foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
    foreach {k v} $args {
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
                $cnv coords "${tag}_seloutline"                                   \
                    $xpos                                $ypos                    \
                    [expr $xpos + $xnew + $corner] [expr $ypos                  ] \
                    [expr $xpos + $xnew          ] [expr $ypos         + $corner] \
                    [expr $xpos + $xnew          ] [expr $ypos + $ynew - $corner] \
                    [expr $xpos + $xnew + $corner] [expr $ypos + $ynew          ] \
                    $xpos                    [expr $ypos + $ynew          ] \
                    $xpos                                $ypos

                $cnv coords "${tag}_text" \
                    [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
            } "-text" {
                pdtk_text_set $cnv "${tag}_text" "$v"
            }

        }
    }
    $cnv itemconfigure "${tag}_seloutline" -width $zoom
}

namespace eval ::pdwidget::message:: { }
proc ::pdwidget::message::_activate {obj width} {
    set tag "[::pdwidget::base_tag $obj]_seloutline"
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

proc ::pdwidget::core::create_gatom {obj cnv posX posY args} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create polygon 0 0 -tags [list ${tag} ${tag}_seloutline ${tag}_iolets] \
        -fill {} -outline black
    pdtk_text_new $cnv [list ${tag} ${tag}_label ${tag}_selfill] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    pdtk_text_new $cnv [list ${tag} ${tag}_text ${tag}_selfill] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_gatom
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    ::pdwidget::widgetbehavior $obj textselect ::pdwidget::core::textselect

    if { $args ne {} } {::pdwidget::core::config_gatom $obj $cnv {*}$args }
}
proc ::pdwidget::core::config_gatom {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2
    set sizechanged 0

    set zoom [::pd::canvas::get_zoom $cnv]
    foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
    foreach {k v} $args {
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
                $cnv coords "${tag}_seloutline" \
                    $xpos                   $ypos \
                    [expr $xpos2 - $corner]       $ypos \
                    $xpos2            [expr $ypos + $corner] \
                    $xpos2                  $ypos2 \
                    $xpos                   $ypos2 \
                    $xpos                   $ypos
                $cnv coords "${tag}_text" \
                    [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
                # TODO label
            } "-fontsize" {
                set font [$cnv itemcget "${tag}_text" -font]
                lset font 1 -${v}
                set font [get_font_for_size [expr $v * $zoom] ]
                $cnv itemconfigure "${tag}_selfill" -font $font
            } "-label" {
                pdtk_text_set $cnv "${tag}_label" "$v"
            } "-labelpos" {
                set txt [$cnv itemcget "${tag}_label" -text]
                set txtlen [string length $txt]
                foreach {x1 y1 x2 y2} [$cnv coords "${tag}"] {break}
                switch -exact -- $v {
                    "left" {
                        $cnv itemconfigure "${tag}_label" -anchor ne
                        $cnv coords "${tag}_label" [expr $x1 - 3 * $zoom] [expr $y1 + 2 * $zoom]
                    } "right" {
                        $cnv itemconfigure "${tag}_label" -anchor nw
                        $cnv coords "${tag}_label" [expr $x2 + 2 * $zoom] [expr $y1 + 2 * $zoom]
                    } "top" {
                        $cnv itemconfigure "${tag}_label" -anchor sw
                        $cnv coords "${tag}_label" [expr $x1 - 1 * $zoom] [expr $y1 - 1 * $zoom]
                    } "bottom" {
                        $cnv itemconfigure "${tag}_label" -anchor nw
                        $cnv coords "${tag}_label" [expr $x1 - 1 * $zoom] [expr $y2 + 3 * $zoom]
                    }
                }
            } "-text" {
                pdtk_text_set $cnv "${tag}_text" "$v"
            }

        }
    }
    $cnv itemconfigure "${tag}_seloutline" -width $zoom
    if { $sizechanged } {
        ::pdwidget::refresh_iolets $obj
    }
}

# list atoms
proc ::pdwidget::core::create_latom {obj cnv posX posY args} {
    # stack order
    # - boundingrectangle
    # - outlets
    # - inlets
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create polygon 0 0 -tags [list ${tag} ${tag}_seloutline ${tag}_iolets] \
        -fill {} -outline black
    pdtk_text_new $cnv [list ${tag} ${tag}_label ${tag}_selfill] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    pdtk_text_new $cnv [list ${tag} ${tag}_text ${tag}_selfill] 0 0 {} [::pd::canvas::get_fontsize $cnv] black

    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_latom
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    ::pdwidget::widgetbehavior $obj textselect ::pdwidget::core::textselect
    if { $args ne {} } {::pdwidget::core::config_latom $obj $cnv {*}$args }
}
proc ::pdwidget::core::config_latom {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2
    set sizechanged 0

    set zoom [::pd::canvas::get_zoom $cnv]
    foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
    foreach {k v} $args {
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
                $cnv coords "${tag}_seloutline" \
                    $xpos                   $ypos \
                    [expr $xpos2 - $corner]       $ypos \
                    $xpos2            [expr $ypos + $corner] \
                    $xpos2            [expr $ypos2 - $corner] \
                    [expr $xpos2 - $corner]       $ypos2 \
                    $xpos                   $ypos2 \
                    $xpos                   $ypos
                $cnv coords "${tag}_text" \
                    [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
                # TODO label
            } "-text" {
                pdtk_text_set $cnv "${tag}_text" "$v"
            }

        }
    }
    $cnv itemconfigure "${tag}_seloutline" -width $zoom
    if { $sizechanged } {
        ::pdwidget::refresh_iolets $obj
    }
}
########################################################################
# comment
## TODO: some text-cursor appears on the left-hand-side after editing
proc ::pdwidget::core::create_comment {obj cnv posX posY args} {
    # stack order
    # - boundingrectangle
    # - text
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    $cnv create line 0 0 0 0 -tags [list ${tag} ${tag}_commentbar] -fill {}
    pdtk_text_new $cnv [list ${tag} ${tag}_text ${tag}_selfill ${tag}_iolets] 0 0 {} [::pd::canvas::get_fontsize $cnv] black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_comment
    ::pdwidget::widgetbehavior $obj editmode ::pdwidget::core::edit_comment
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    ::pdwidget::widgetbehavior $obj textselect ::pdwidget::core::textselect
    if { $args ne {} } {::pdwidget::core::config_comment $obj $cnv {*}$args }
}

proc ::pdwidget::core::config_comment {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2
    set sizechanged 0

    set zoom [::pd::canvas::get_zoom $cnv]
    foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
    foreach {k v} $args {
        switch -exact -- $k {
            default {
            } "-size" {
                set xnew [lindex $v 0]
                set ynew [lindex $v 1]
                set xpos1 [expr $xpos + $xnew * $zoom]
                set ypos1 [expr $ypos + $ynew * $zoom]
                set sizechanged 1
                $cnv coords "${tag}" $xpos  $ypos $xpos1 $ypos
                $cnv coords "${tag}_commentbar" \
                    $xpos1 $ypos \
                    $xpos1 $ypos1
                $cnv coords "${tag}_text" \
                    [expr $xpos + $lmargin * $zoom] [expr $ypos + $tmargin * $zoom]
            } "-text" {
                pdtk_text_set $cnv "${tag}_text" "$v"
            } "-state" {
                switch -exact -- $v {
                    "edit" {
                        $cnv itemconfigure "${tag}_commentbar" -fill black
                    } default {
                        $cnv itemconfigure "${tag}_commentbar" -fill {}
                    }
                }
            }

        }
    }
    $cnv itemconfigure "${tag}_commentbar" -width $zoom
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
    set tag "[::pdwidget::base_tag $obj]_commentbar"
    $cnv itemconfigure ${tag} -fill $color
}

########################################################################
# connection

proc ::pdwidget::core::create_conn {obj cnv posX posY args} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    # the 'iolets' tag might be expected by some helper scripts, but it not really needed...
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline {} -fill {} -width 0
    $cnv create line 0 0 0 0 -tags [list ${tag} ${tag}_iolets ${tag}_cord] -fill black
    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_conn
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    if { $args ne {} } {::pdwidget::core::config_conn $obj $cnv {*}$args }
}
proc ::pdwidget::core::config_conn {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2

    set zoom [::pd::canvas::get_zoom $cnv]
    foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
    foreach {k v} $args {
        switch -exact -- $k {
            default {
            } "-position" {
                foreach {x0 y0 x1 y1} $v {break}
                $cnv coords "${tag}_cord" \
                    [expr $x0 * $zoom] [expr $y0 * $zoom] \
                    [expr $x1 * $zoom] [expr $y1 * $zoom]
                $cnv coords "${tag}" [$cnv coords "${tag}_cord"]
                foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
            } "-text" {
                pdtk_text_set $cnv "${tag}_text" "$v"
            } "-type" {
                switch -exact $v {
                    "signal" {
                        $cnv dtag "${tag}_cord" "${tag}_message"
                        $cnv addtag "${tag}_signal" withtag "${tag}_cord"
                    } default {
                        $cnv dtag "${tag}_cord" "${tag}_signal"
                        $cnv addtag "${tag}_message" withtag "${tag}_cord"
                    }
                }
            }
        }
    }
    $cnv itemconfigure "${tag}_message" -width $zoom
    $cnv itemconfigure "${tag}_signal" -width [expr $zoom * 2]
}


########################################################################
# selection lasso

proc ::pdwidget::core::create_sel {obj cnv posX posY args} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set tag [::pdwidget::base_tag $obj]
    # we need this dummy rectangle to keep the anchor point
    $cnv create rectangle 0 0 0 0 -tags [list ${tag}] -outline black -fill {} -width 1
    # the 'iolets' tag might be expected by some helper scripts, but it not really needed...
    $cnv create rectangle 0 0 0 0 -tags [list ${tag} ${tag}_iolets] -outline black -fill {} -width 1

    $cnv move $tag [expr $zoom * $posX] [expr $zoom * $posY]

    ::pdwidget::widgetbehavior $obj config ::pdwidget::core::config_sel
    ::pdwidget::widgetbehavior $obj select ::pdwidget::core::select
    if { $args ne {} } {::pdwidget::core::config_sel $obj $cnv {*}$args }
}
proc ::pdwidget::core::config_sel {obj cnv args} {
    set tag [::pdwidget::base_tag $obj]
    set tmargin 3
    set lmargin 2

    set zoom [::pd::canvas::get_zoom $cnv]
    foreach {xpos ypos _ _} [$cnv coords "${tag}"] {break}
    dict for {k v} $args {
        switch -exact -- $k {
            default {
            } "-size" {
                foreach {xnew ynew} $v {break}
                $cnv coords "${tag}_iolets" \
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
