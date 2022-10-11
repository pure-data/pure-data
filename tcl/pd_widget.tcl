## infrastructure for drawing Pd "widgets" (graphical objects)

package provide pd_widget 0.1

# we currently require Tcl/Tk-8.5 for
# - {*} argument expansion
# - dict
package require Tcl 8.5

namespace eval ::pdwidget {
    variable IOWIDTH 7
    variable IHEIGHT 2
    variable OHEIGHT 3
}


namespace eval ::pdwidget::_procs { }
array set ::pdwidget::_procs::constructor {}
array set ::pdwidget::_obj2canvas {}

# GUI widget interface (for writing your own widgets)

# public widget functions (to be called from Pd-core)
# a widget MUST implement at least the 'create' procedure,
# and needs to register this constructor via '::pdwidget::register'
# any additional widget-procs can then be registered within the
# constructor with '::pdwidget::widgetbehavior'

# ::pdwidget::create      | create an object on a given canvas
# ::pdwidget::destroy     | destroy the object
# ::pdwidget::config      | re-configure the object (using a parameter syntax)
# ::pdwidget::select      | show that an object is (de)selected
# ::pdwidget::editmode    | show that an object is in editmode
# ::pdwidget::displace    | relative move
# ::pdwidget::moveto      | absolute move
# ::pdwidget::show_iolets | show/hide inlets resp. outlets
# ::pdwidget::create_inlets  | create inlets (arguments are inlet types: 0=message, 1=signal)
# ::pdwidget::create_outlets | create outlets for obj (see above for arguments)

# register a new GUI-object by name
# e.g. '::pdwidget::register "bang" ::pdwidget::bang::create'
# the <ctor> takes a handle to an object and the associated canvas
# and is responsible for registering per-object widgetfunctions
proc ::pdwidget::register {type ctor} {
    set ::pdwidget::_procs::constructor($type) $ctor
}

# register a proc for a given (common) widget behavior
# to be called from the constructor
# the behavior proc is associated with a single object
#
# if a given behavior is not implemented, a default implementation
# is used, which makes a few assumptions about how the widget is written
# the most important assumption is, that all components of an object are bound
# to the tag that can be obtained with [::pdwidget::base_tag obj],
# and that the first item (in the stack order) with this tag defines the
# object's position (and bounding rectangle)
proc ::pdwidget::widgetbehavior {obj behavior behaviorproc} {
    if {$obj eq {} } {
        ::pdwindow::error "refusing to add ${behavior} proc without an object\n"
    } else {
        array set ::pdwidget::_procs::widget_$behavior [list $obj $behaviorproc]
    }
}


# widgetbehavior on the core-side (and how this relates to us)
# - getrect: get the bounding rectangle (NOT our business, LATER send the rect back to the core if needed)
# - displace: relative move of object (::pdwidget::displace)
# - select: (de)select and object (::pdwidget::select)
# - activate: (de)activate the object's editor
# - delete: delete an object (::pdwidget::destroy)
# - vis: show/hide an object (handled via ::pdwidget::create and ::pdwidget::destroy)
# - click: called on hitbox detection with mouse-click (NOT our business, for now)

proc ::pdwidget::_call {canvases behavior obj args} {
    set proc {}
    set wb ::pdwidget::_procs::widget_${behavior}
    if { [info exists $wb] } {
        # get widget-behavior for this object
        if {$proc eq {} } {
            foreach {_ proc} [array get $wb $obj] {break}
        }
        # if there's no specific widget-behavior, check if there's a default one
        if {$proc eq {} } {
            foreach {_ proc} [array get $wb {}] {break}
        }
    }
    if { $proc eq {} } {
        ::pdwindow::error "NOT IMPLEMENTED: ::pdwidget::$behavior $obj $args\n"
    } else {
        # {*} requires Tcl-8.5
        if { $canvases eq {} } {set canvases [::pdwidget::get_canvases $obj] }
        foreach cnv $canvases {
            $proc $obj $cnv {*}$args
        }
    }
}

# fallback widget behaviors (private, DO NOT CALL DIRECTLY)
proc ::pdwidget::_defaultproc {id arguments body} {
    proc ::pdwidget::_${id} $arguments $body
    array set ::pdwidget::_procs::widget_${id} [list {} ::pdwidget::_${id}]
}

# 'destroy' an object (removing it from the canvas(es))
::pdwidget::_defaultproc destroy {obj cnv} {
    # dummy fallback to not throw an error
}

proc ::pdwidget::_update_connections_on_canvas {cnv objtag} {
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

::pdwidget::_defaultproc editmode {obj cnv state} {
    # just ignore
}


::pdwidget::_defaultproc displace {obj cnv dx dy} {
    set tag [::pdwidget::base_tag $obj]
    $cnv move $tag $dx $dy
    ::pdwidget::_update_connections_on_canvas $cnv $tag
}

::pdwidget::_defaultproc moveto {obj cnv x y} {
    set zoom [::pd::canvas::get_zoom $cnv]
    set x [expr $x * $zoom]
    set y [expr $y * $zoom]
    set tag [::pdwidget::base_tag $obj]
    if {[catch {$cnv moveto $tag $x $y}]} {
        foreach {oldx oldy} [$cnv coords $tag] {
            $cnv move $tag [expr $x - $oldx] [expr $y - $oldy]
        }
    }
    ::pdwidget::_update_connections_on_canvas $cnv $tag
}

::pdwidget::_defaultproc show_iolets {obj cnv show_inlets show_outlets} {
    set tag [::pdwidget::base_tag $obj]
    set icolor {}
    set ocolor {}
    if {$show_inlets } {set icolor black}
    if {$show_outlets} {set ocolor black}
    $cnv itemconfigure ${tag}&&inlet  -fill $icolor
    $cnv itemconfigure ${tag}&&outlet -fill $ocolor
}
proc ::pdwidget::_do_create_iolets {obj cnv iotag iolets iowidth ioheight} {
    set tag [::pdwidget::base_tag $obj]
    set numiolets [llength $iolets]
    if {$numiolets < 2} {set numiolets 2}

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
::pdwidget::_defaultproc create_inlets {obj cnv inlets} {
    ::pdwidget::_do_create_iolets $obj $cnv inlet $inlets $::pdwidget::IOWIDTH $::pdwidget::IHEIGHT
}
::pdwidget::_defaultproc create_outlets {obj cnv outlets} {
    ::pdwidget::_do_create_iolets $obj $cnv outlet $outlets $::pdwidget::IOWIDTH $::pdwidget::OHEIGHT
}
::pdwidget::_defaultproc create_iolets {obj cnv {inlets {}} {outlets {}}} {
    ::pdwidget::_do_create_iolets $obj $cnv inlet $inlets $::pdwidget::IOWIDTH $::pdwidget::IHEIGHT
    ::pdwidget::_do_create_iolets $obj $cnv outlet $outlets $::pdwidget::IOWIDTH $::pdwidget::OHEIGHT
}
::pdwidget::_defaultproc refresh_iolets {obj cnv} {
    set tag [::pdwidget::base_tag $obj]
    set inlets [::pdwidget::get_iolets $obj $cnv inlet]
    set outlets [::pdwidget::get_iolets $obj $cnv outlet]
    ::pdwidget::_do_create_iolets $obj $cnv inlet $inlets $::pdwidget::IOWIDTH $::pdwidget::IHEIGHT
    ::pdwidget::_do_create_iolets $obj $cnv outlet $outlets $::pdwidget::IOWIDTH $::pdwidget::OHEIGHT

    ::pdwidget::_update_connections_on_canvas $cnv $tag
}


# get inlet types
proc ::pdwidget::get_iolets {obj cnv type} {
    if { $cnv eq {} } {
        foreach cnv [::pdwidget::get_canvases $obj] {break}
    }
    if { $cnv eq {} } {return}
    set tag [::pdwidget::base_tag $obj]

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
proc ::pdwidget::create {type obj cnv posX posY} {
    $cnv delete [base_tag $obj]
    if {[catch {$::pdwidget::_procs::constructor($type) $obj $cnv $posX $posY} stdout]} {
        ::pdwindow::error "Unknown widget type '$type': $stdout\n"
    } else {
        # associate this obj with the cnv
        ::pd::canvas::add_object $cnv $obj
        if {[info exists ::pdwidget::_obj2canvas($obj) ] && [lsearch -exact $::pdwidget::_obj2canvas($obj) $cnv ] >= 0 } { } else {
            lappend ::pdwidget::_obj2canvas($obj) $cnv
        }
    }
}
proc ::pdwidget::destroy {obj} {
    ::pdwidget::_call {} destroy $obj

    # always clean up our internal state
    set tag [::pdwidget::base_tag $obj]
    foreach cnv [::pdwidget::get_canvases $obj] {
        foreach id [$cnv find withtag "connection&&src:${tag}"] {
            $cnv delete $id
        }
        foreach id [$cnv find withtag "connection&&dst:${tag}"] {
            $cnv delete $id
        }
        $cnv delete $tag
        ::pd::canvas::remove_object $cnv $obj
    }
    # finally get rid of the obj2canvas mapping
    if {[info exists ::pdwidget::_obj2canvas($obj)]} {
        unset ::pdwidget::_obj2canvas($obj)
    }
}
proc ::pdwidget::config {obj args} {
    ::pdwidget::_call {} config $obj {*}$args
}
proc ::pdwidget::select {obj state} {
    ::pdwidget::_call {} select $obj $state
}
proc ::pdwidget::editmode {obj state} {
    ::pdwidget::_call {} editmode $obj $state
}
proc ::pdwidget::displace {obj dx dy} {
    ::pdwidget::_call {} displace $obj $dx $dy
}
proc ::pdwidget::moveto {obj cnv x y} {
    ::pdwidget::_call $cnv moveto $obj $x $y
}
proc ::pdwidget::textselect {obj {index {}} {selectionlength {}}} {
    set args $obj
    if { $index ne {} } {
        lappend args $index
        if { $selectionlength ne {} } {
            lappend args $selectionlength
        }
    }
    ::pdwidget::_call {} textselect {*}$args
}



# 'connect' and 'disconnect' show how connection handling might actually word
# in practice, this would break compatibility with GUI externals
#
# a backward-compatible approach is to use the 'connection' widget
proc ::pdwidget::connect {src outlet dst inlet} {
    set srctag [::pdwidget::base_tag $src]
    set dsttag [::pdwidget::base_tag $dst]
    foreach cnv [::pdwidget::get_canvases $obj] {
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
proc ::pdwidget::disconnect {src outlet dst inlet} {
    set srctag [::pdwidget::base_tag $src]
    set dsttag [::pdwidget::base_tag $dst]
    foreach cnv [::pdwidget::get_canvases $obj] {
        foreach id [$cnv find withtag "connection&&src:${srctag}:${outlet}&&dst:$dsttag:${inlet}"] {
            $cnv delete $id
        }
    }
}

proc ::pdwidget::create_inlets {obj inlets} {
    ::pdwidget::_call {} create_inlets $obj $inlets
}
proc ::pdwidget::create_outlets {obj outlets} {
    ::pdwidget::_call {} create_outlets $obj $outlets
}
proc ::pdwidget::show_iolets {obj show_inlets show_outlets} {
    ::pdwidget::_call {} show_iolets $obj $show_inlets $show_outlets
}

# these are actually used internally (not by Pd-core)
proc ::pdwidget::create_iolets {obj {inlets {}} {outlets {}}} {
## TODO add cnv (?)
    ::pdwidget::_call {} create_iolets $obj $inlets $outlets
}
proc ::pdwidget::refresh_iolets {obj} {
## TODO add cnv (?)
    ::pdwidget::_call {} refresh_iolets $obj
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
#   % puts [::pdwidget::parseargs $options $arguments]
#   -help {} -position {10 20} {} {30 40}
#   %
# call '::pdwidget::parseargs {-position 2 -label 1} {-position 10 20}'
# and it will return a dictionary {-position {10 20}}
proc ::pdwidget::parseargs {optionspec argv} {
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
proc ::pdwidget::base_tag {obj} {
    set tag [string trimleft $obj 0x]
    return "${tag}OBJ"
    # this one is much nicer...
    return "_$obj"
}
proc ::pdwidget::get_canvases {obj} {
    if {[info exists ::pdwidget::_obj2canvas($obj)]} {
        return $::pdwidget::_obj2canvas($obj)
    }
}




# finally import the actual widget implementations
package require pdwidget_core
package require pdwidget_iemgui
