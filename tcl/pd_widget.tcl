## infrastructure for drawing Pd "widgets" (graphical objects)

package provide pd_widget 0.1

# we currently require Tcl/Tk-8.5 for
# - {*} argument expansion
# - dict
package require Tcl 8.5

namespace eval ::pd::widget {
    variable IOWIDTH 7
    variable IHEIGHT 2
    variable OHEIGHT 3
}


namespace eval ::pd::widget::_procs { }
array set ::pd::widget::_procs::constructor {}
array set ::pd::widget::_obj2canvas {}

# GUI widget interface (for writing your own widgets)

# public widget functions (to be called from Pd-core)
# a widget MUST implement at least the 'create' procedure,
# and needs to register this constructor via '::pd::widget::register'
# any additional widget-procs can then be registered within the
# constructor with '::pd::widget::widgetbehaviour'

# ::pd::widget::create      | create an object on a given canvas
# ::pd::widget::destroy     | destroy the object
# ::pd::widget::config      | re-configure the object (using a parameter syntax)
# ::pd::widget::select      | show that an object is (de)selected
# ::pd::widget::displace    | relative move
# ::pd::widget::moveto      | absolute move
# ::pd::widget::show_iolets | show/hide inlets resp. outlets
# ::pd::widget::create_inlets  | create inlets (arguments are inlet types: 0=message, 1=signal)
# ::pd::widget::create_outlets | create outlets for obj (see above for arguments)

# register a new GUI-object by name
# e.g. '::pd::widget::register "bang" ::pd::widget::bang::create'
# the <ctor> takes a handle to an object and the associated canvas
# and is responsible for registering per-object widgetfunctions
proc ::pd::widget::register {type ctor} {
    set ::pd::widget::_procs::constructor($type) $ctor
}

# register a proc for a given (common) widget behaviour
# to be called from the constructor
# the behaviour proc is associated with a single object
#
# if a given behaviour is not implemented, a default implementation
# is used, which makes a few assumptions about how the widget is written
# the most important assumption is, that all components of an object are bound
# to the tag that can be obtained with [::pd::widget::base_tag obj],
# and that the first item (in the stack order) with this tag defines the
# object's position (and bounding rectangle)
proc ::pd::widget::widgetbehaviour {obj behaviour behaviourproc} {
    if {$obj eq {} } {
        ::pdwindow::error "refusing to add ${behaviour} proc without an object\n"
    } else {
        array set ::pd::widget::_procs::widget_$behaviour [list $obj $behaviourproc]
    }
}


# widgetbehaviour on the core-side (and how this relates to us)
# - getrect: get the bounding rectangle (NOT our business, LATER send the rect back to the core if needed)
# - displace: relative move of object (::pd::widget::displace)
# - select: (de)select and object (::pd::widget::select)
# - activate: (de)activate the object's editor
# - delete: delete an object (::pd::widget::destroy)
# - vis: show/hide an object (handled via ::pd::widget::create and ::pd::widget::destroy)
# - click: called on hitbox detection with mouse-click (NOT our business, for now)

proc ::pd::widget::_call {behaviour obj args} {
    set proc {}
    set wb ::pd::widget::_procs::widget_${behaviour}
    if { [info exists $wb] } {
        # get widget-behaviour for this object
        if {$proc eq {} } {
            foreach {_ proc} [array get $wb $obj] {break}
        }
        # if there's no specific widget-behaviour, check if there's a default one
        if {$proc eq {} } {
            foreach {_ proc} [array get $wb {}] {break}
        }
    }
    if { $proc eq {} } {
        ::pdwindow::error "NOT IMPLEMENTED: ::pd::widget::$behaviour $obj $args\n"
    } else {
        # {*} requires Tcl-8.5
        $proc $obj {*}$args
    }
}

# fallback widget behaviours (private, DO NOT CALL DIRECTLY)
proc ::pd::widget::_defaultproc {id arguments body} {
    proc ::pd::widget::_${id} $arguments $body
    array set ::pd::widget::_procs::widget_${id} [list {} ::pd::widget::_${id}]
}

# 'destroy' an object (removing it from the canvas(es))
::pd::widget::_defaultproc destroy {obj} {
    set tag [::pd::widget::base_tag $obj]
    if {[info exists ::pd::widget::_obj2canvas($obj)]} {
        foreach cnv $::pd::widget::_obj2canvas($obj) {
            foreach id [$cnv find withtag "connection&&src:${tag}"] {
                $cnv delete $id
            }
            foreach id [$cnv find withtag "connection&&dst:${tag}"] {
                $cnv delete $id
            }
            $cnv delete $tag
            ::pd::canvas::remove_object $cnv $obj
        }
        unset ::pd::widget::_obj2canvas($obj)
    }
}

proc ::pd::widget::_update_iolets_on_canvas {cnv objtag} {
    set numio 0
    foreach anchor [$cnv find withtag "${objtag}&&outlet${numio}&&anchor"] {
        foreach {x y} [$cnv coords $anchor] {
            foreach id [$cnv find withtag "connection&&src:${objtag}:${numio}" ] {
                foreach {_ _ x1 y1} [$cnv coords $id] {
                    $cnv coords $id $x $y $x1 $y1
                    break
                }
            }
            break
        }
    }
    set numio 0
    foreach anchor [$cnv find withtag "${objtag}&&inlet${numio}&&anchor"] {
        foreach {x y} [$cnv coords $anchor] {
            foreach id [$cnv find withtag "connection&&dst:${objtag}:${numio}" ] {
                foreach {x0 y0 _ _} [$cnv coords $id] {
                    $cnv coords $id $x0 $y0 $x $y
                    break
                }
            }
            break
        }
    }
}
::pd::widget::_defaultproc displace {obj dx dy} {
    set tag [::pd::widget::base_tag $obj]
    foreach cnv [::pd::widget::get_canvases $obj] {
        $cnv move $tag $dx $dy
        ::pd::widget::_update_iolets_on_canvas $cnv $tag
    }
}

::pd::widget::_defaultproc moveto {obj cnv x y} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set x [expr $x * $zoom]
    set y [expr $y * $zoom]
    set tag [::pd::widget::base_tag $obj]
    if {[catch {$cnv moveto $tag $x $y}]} {
        foreach {oldx oldy} [$cnv coords $tag] {
            $cnv move $tag [expr $x - $oldx] [expr $y - $oldy]
        }
    }
    ::pd::widget::_update_iolets_on_canvas $cnv $tag
}

::pd::widget::_defaultproc show_iolets {obj show_inlets show_outlets} {
    set tag [::pd::widget::base_tag $obj]
    set icolor {}
    set ocolor {}
    if {$show_inlets } {set icolor black}
    if {$show_outlets} {set ocolor black}
    if {[info exists ::pd::widget::_obj2canvas($obj)]} {
        foreach cnv $::pd::widget::_obj2canvas($obj) {
            $cnv itemconfigure ${tag}&&inlet  -fill $icolor
            $cnv itemconfigure ${tag}&&outlet -fill $ocolor
        }
    }
}
proc ::pd::widget::_do_create_iolets {obj iotag iolets iowidth ioheight} {
    set tag [::pd::widget::base_tag $obj]
    set numiolets [llength $iolets]
    if {$numiolets < 2} {set numiolets 2}
    if {[info exists ::pd::widget::_obj2canvas($obj)]} {
        foreach cnv $::pd::widget::_obj2canvas($obj) {
            $cnv delete "${tag}&&${iotag}"
            set zoom [::pd::canvas::get_zoom $cnv]
            foreach {x0 y0 x1 y1} [$cnv coords $tag] {break}
            set w [expr $x1 - $x0]
            set h [expr $y1 - $y0]
            set iw [expr $iowidth * $zoom]
            set ih [expr ($ioheight - 0.5) * $zoom]
            set delta [expr ($w - $iw)/($numiolets - 1.) ]
            set objheight 0
            if {$iotag eq "outlet"} {
                set objheight [expr $h - $ih]
            }

            set numin 0
            foreach iolet $iolets {
                set iotype message
                switch -exact $iolet {
                    default {
                        set iotype message
                    } 1 {
                        set iotype signal
                    }
                }
                # create an iolet (with an 'anchor' where the connection starts)
                set centerX [expr $iw / 2]
                set centerY [expr $ih / 2]
                $cnv create rectangle $centerX $centerY $centerX $centerY -tags [list $tag ${iotag} ${iotag}${numin} anchor] -outline {} -fill {} -width 0
                $cnv create rectangle 0 0 $iw $ih -tags [list $tag ${iotag} ${iotag}${numin} $iotype] -fill black
                # move the iolet in place
                $cnv move "${tag}&&${iotag}${numin}" [expr $x0 + $numin * $delta] [expr $y0 + $objheight]

                incr numin
            }
            $cnv lower "${tag}&&${iotag}" "${tag}&&iolets"
            $cnv raise "${tag}&&${iotag}" "${tag}&&iolets"
        }
    }
}
::pd::widget::_defaultproc create_inlets {obj inlets} {
    ::pd::widget::_do_create_iolets $obj inlet $inlets $::pd::widget::IOWIDTH $::pd::widget::IHEIGHT
}
::pd::widget::_defaultproc create_outlets {obj outlets} {
    ::pd::widget::_do_create_iolets $obj outlet $outlets $::pd::widget::IOWIDTH $::pd::widget::OHEIGHT
}
::pd::widget::_defaultproc create_iolets {obj {inlets {}} {outlets {}}} {
    if {$inlets eq {} } {
        set inlets [::pd::widget::get_iolets $obj inlet]
    }
    if {$outlets eq {} } {
        set outlets [::pd::widget::get_iolets $obj outlet]
    }
    ::pd::widget::_do_create_iolets $obj inlet $inlets $::pd::widget::IOWIDTH $::pd::widget::IHEIGHT
    ::pd::widget::_do_create_iolets $obj outlet $outlets $::pd::widget::IOWIDTH $::pd::widget::OHEIGHT
}

# get inlet types
proc ::pd::widget::get_iolets {obj type} {
    set cnv {}
    if {[info exists ::pd::widget::_obj2canvas($obj)]} {
        foreach cnv $::pd::widget::_obj2canvas($obj) {break}
    }
    if { $cnv eq {} } {return}
    set tag [::pd::widget::base_tag $obj]

    set result {}
    foreach id [$cnv find withtag "${tag}&&${type}&&!anchor" ] {
        set tags [$cnv gettags $id]
        if { [lsearch -exact $tags signal] >= 0 } {
            lappend result 1
        } else {
            lappend result 0
        }
    }
    return $result
}



#
proc ::pd::widget::create {type obj cnv posX posY} {
    $cnv delete [base_tag $obj]
    if {[catch {$::pd::widget::_procs::constructor($type) $obj $cnv $posX $posY} stdout]} {
        ::pdwindow::error "Unknown widget type '$type': $stdout\n"
    } else {
        # associate this obj with the cnv
        ::pd::canvas::add_object $cnv $obj
        lappend ::pd::widget::_obj2canvas($obj) $cnv
    }
}
proc ::pd::widget::destroy {obj} {
    ::pd::widget::_call destroy $obj
    # always clean up our internal state
    ::pd::widget::_destroy $obj
}
proc ::pd::widget::config {obj args} {
    ::pd::widget::_call config $obj {*}$args
}
proc ::pd::widget::select {obj state} {
    ::pd::widget::_call select $obj $state
}
proc ::pd::widget::displace {obj dx dy} {
    ::pd::widget::_call displace $obj $dx $dy
}
proc ::pd::widget::moveto {obj cnv x y} {
    ::pd::widget::_call moveto $obj $cnv $x $y
}
proc ::pd::widget::connect {src outlet dst inlet} {
    set srctag [::pd::widget::base_tag $src]
    set dsttag [::pd::widget::base_tag $dst]
    if {![info exists ::pd::widget::_obj2canvas($src)]} {return}
    foreach cnv $::pd::widget::_obj2canvas($src) {
        set zoom [::pd::canvas::get_zoom $cnv]
        foreach {x0 y0 x1 y1} {{} {} {} {}} {break}
        foreach {x0 y0} [$cnv coords "${srctag}&&anchor&&outlet${outlet}"] {break}
        foreach {x1 y1} [$cnv coords "${dsttag}&&anchor&&inlet${inlet}"] {break}
        if {$x0 eq {} || $x1 eq {}} {
            ::pdwindow::error "Unable to connect ${src}:${outlet} with ${dst}:${inlet} on $cnv : $x0 $x1\n"
            continue
        }
        set cordwidth $zoom
        foreach id [$cnv find withtag "${srctag}&&!anchor&&outlet${outlet}"] {
            if { [lsearch -exact [$cnv gettags $id] signal] >= 0 } {
                set cordwidth [expr 2 * $zoom]
            }
        }
        $cnv create line $x0 $y0 $x1 $y1 -tags [list "connection" "src:$srctag" "src:$srctag:${outlet}" "dst:$dsttag" "dst:$dsttag:${inlet}"] -width $cordwidth
    }
}
proc ::pd::widget::disconnect {src outlet dst inlet} {
    set srctag [::pd::widget::base_tag $src]
    set dsttag [::pd::widget::base_tag $dst]
    if {![info exists ::pd::widget::_obj2canvas($src)]} {return}
    foreach cnv $::pd::widget::_obj2canvas($src) {
        foreach id [$cnv find withtag "connection&&src:${srctag}:${outlet}&&dst:$dsttag:${inlet}"] {
            $cnv delete $id
        }
    }
}

proc ::pd::widget::create_inlets {obj args} {
    ::pd::widget::_call create_inlets $obj $args
}
proc ::pd::widget::create_outlets {obj args} {
    ::pd::widget::_call create_outlets $obj $args
}

# these are actually used internally (not by Pd-core)
proc ::pd::widget::create_iolets {obj {inlets {}} {outlets {}}} {
    ::pd::widget::_call create_iolets $obj $inlets $outlets
}
proc ::pd::widget::show_iolets {obj show_inlets show_outlets} {
    ::pd::widget::_call show_iolets $obj $show_inlets $show_outlets
}



############## helpers

# simplistic options parsers
# <optionspec> is a dict of <name,length>
# <argv> is a list of arguments
# whenever the 1st arg of <argv> matches a <name> in <optionspec>,
# the following <length> elements are stored in the result under <name>
# the consumed elements are discarded, and we start anew.
# leftover arguments are stored under the empty key {}
# NOTE: whenever an unknown argument is encountered, the algorithm stops
#       and the remaining args are stored as leftovers
#
# example:
#   % set options {-position 2 -help 0}
#   % set arguments {-help -position 10 20 30 40}
#   % puts [::pd::widget::parseargs $options $arguments]
#   -help {} -position {10 20} {} {30 40}
#   %
# call '::pd::widget::parseargs {-position 2 -label 1} {-position 10 20}'
# and it will return a dictionary {-position {10 20}}
proc ::pd::widget::parseargs {optionspec argv} {
    set result [dict create]
    set numargs [llength $argv]
    set argc 1

    for {set i 0} {$i < $numargs} {incr i} {
        set opt [lindex $argv $i]
        if {[dict exists $optionspec $opt]} {
            set argc [dict get $optionspec $opt]
            set start [expr $i + 1]
            if {$argc == 1} {
                dict set result $opt [lindex $argv $start]
            } else {
                dict set result $opt [lrange $argv $start $i+$argc]
            }
            incr i $argc
        } else {
            dict set result {} [lrange $argv $i end]
            break
        }
    }
    return $result
}

# get a string to be used as a base-tag for $obj
proc ::pd::widget::base_tag {obj} {
    set tag [string trimleft $obj 0x]
    return "${tag}OBJ"
    # this one is much nicer...
    return "_$obj"
}
proc ::pd::widget::get_canvases {obj} {
    if {[info exists ::pd::widget::_obj2canvas($obj)]} {
        return $::pd::widget::_obj2canvas($obj)
    }
}




# finally import the actual widget implementations
package require pdwidget_object
package require pdwidget_message

package require pdwidget_bang
package require pdwidget_canvas
package require pdwidget_radio
package require pdwidget_toggle
