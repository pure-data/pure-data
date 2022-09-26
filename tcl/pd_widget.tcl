## infrastructure for drawing Pd "widgets" (graphical objects)

package provide pd_widget 0.1

# we currently require Tcl/Tk-8.5 for
# - {*} argument expansion
# - dict
package require Tcl 8.5

namespace eval ::pd::widget {
    variable IOWIDTH 7
    variable IHEIGHT 3
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
# TODO:
# ::pd::widget::create_inlets  |
# ::pd::widget::create_outlets |

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
            $cnv delete $tag
        }
        unset ::pd::widget::_obj2canvas($obj)
    }
}

::pd::widget::_defaultproc displace {obj dx dy} {
    set tag [::pd::widget::base_tag $obj]
    if {[info exists ::pd::widget::_obj2canvas($obj)]} {
        foreach cnv $::pd::widget::_obj2canvas($obj) {
            $cnv move $tag $dx $dy
        }
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
}

::pd::widget::_defaultproc show_iolets {obj show_inlets show_outlets} {
    set tag [::pd::widget::base_tag $obj]
    set icolor {}
    set ocolor {}
    if {$show_inlets } {set icolor black}
    if {$show_outlets} {set ocolor black}
    if {[info exists ::pd::widget::_obj2canvas($obj)]} {
        foreach cnv $::pd::widget::_obj2canvas($obj) {
            $cnv itemconfigure ${tag}INLET  -fill $icolor
            $cnv itemconfigure ${tag}OUTLET -fill $ocolor
        }
    }
}



#
proc ::pd::widget::create {type obj cnv posX posY} {
    $cnv delete $obj
    if {[catch {$::pd::widget::_procs::constructor($type) $obj $cnv $posX $posY} stdout]} {
        ::pdwindow::error "Unknown widget type '$type': $stdout\n"
    } else {
        # associate this obj with the cnv
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
package require pdwidget_canvas
package require pdwidget_bang
package require pdwidget_radio
package require pdwidget_toggle