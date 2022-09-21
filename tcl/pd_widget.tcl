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
array set ::pd::widget::_canvas2obj {}

proc ::pd::widget::_lremove {list value} {
    set result {}
    foreach x $list {
        if {$x ne $value} {
            lappend result $x
        }
    }
    return $result
}

# public interface for Pd->GUI communication

# register a new GUI-object by name
# e.g. '::pd::widget::register "bang" ::pd::widget::bang::create'
# the <ctor> takes a handle to an object and the associated canvas
# and is responsible for registering per-object widgetfunctions
proc ::pd::widget::register {type ctor} {
    puts "registering widget '$type' with $ctor"
    set ::pd::widget::_procs::constructor($type) $ctor
}
proc ::pd::widget::widgetbehaviour {obj behaviour behaviourproc} {
    if {$obj eq {} } {
        ::pdwindow::error "refusing to add ${behaviour} proc without an object\n"
    } else {
        array set ::pd::widget::_procs::widget_$behaviour [list $obj $behaviourproc]
    }
}
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

#
proc ::pd::widget::create {type obj cnv} {
    if {[catch {$::pd::widget::_procs::constructor($type) $obj $cnv} stdout]} {
        ::pdwindow::error "Unknown widget type '$type': $stdout\n"
    } else {
        # associate this obj with the cnv
        lappend ::pd::widget::_canvas2obj($cnv) $obj
        lappend ::pd::widget::_obj2canvas($obj) $cnv
    }
}
proc ::pd::widget::delete {obj} {
    ::pd::widget::_call delete $obj
    # always clean up our internal state
    ::pd::widget::_delete $obj
}
proc ::pd::widget::config {obj args} {
    ::pd::widget::_call config $obj {*}$args
}
proc ::pd::widget::select {obj state} {
    ::pd::widget::_call select $obj $state
}
proc ::pd::widget::move {obj dx dy} {
    ::pd::widget::_call move $obj $dx $dy
}
proc ::pd::widget::moveto {obj cnv x y} {
    ::pd::widget::_call moveto $obj $cnv $x $y
}
proc ::pd::widget::show_iolets {obj show_inlets show_outlets} {
    ::pd::widget::_call show_iolets $obj $show_inlets $show_outlets
}

# fallback widget behaviours (private, DO NOT CALL DIRECTLY)
proc ::pd::widget::_defaultproc {id arguments body} {
    proc ::pd::widget::_${id} $arguments $body
    array set ::pd::widget::_procs::widget_${id} [list {} ::pd::widget::_${id}]
}

::pd::widget::_defaultproc delete {obj} {
    set tag [::pd::widget::base_tag $obj]
    if {[info exists ::pd::widget::_obj2canvas($obj)]} {
        foreach cnv $::pd::widget::_obj2canvas($obj) {
            $cnv delete $tag
            if {[info exists ::pd::widget::_canvas2obj($cnv)]} {
                set ::pd::widget::_canvas2obj($cnv) [_lremove $::pd::widget::_canvas2obj($cnv) $obj]
            }
        }
        unset ::pd::widget::_obj2canvas($obj)
    }
}


::pd::widget::_defaultproc move {obj dx dy} {
    set tag [::pd::widget::base_tag $obj]
    if {[info exists ::pd::widget::_obj2canvas($obj)]} {
        foreach cnv $::pd::widget::_obj2canvas($obj) {
            $cnv move $tag $dx $dy
        }
    }
}

::pd::widget::_defaultproc moveto {obj cnv x y} {
    set zoom [::pdtk_canvas::get_zoom $cnv]
    set x [expr $x * $zoom]
    set y [expr $y * $zoom]
    set tag [::pd::widget::base_tag $obj]
    if {[catch {$cnv moveto $tag $x $y}]} {
        foreach {oldx oldy} [$cnv coords $tag] {
            puts $cnv move $tag [expr $x - $oldx] [expr $y - $oldy]
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
