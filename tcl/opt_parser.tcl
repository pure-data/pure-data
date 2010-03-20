package provide opt_parser 0.1

namespace eval opt_parser {
    # list of option vars (keys are long option names)
    variable optlist
    # option behavior <set|lappend>
    variable optbehavior
    variable optprefix {-}
}

proc opt_parser::init {optdata} {
    variable optlist
    variable optbehavior
    array unset optlist ; array set optlist {}
    array unset optbehavior ; array set optbehavior {}
    foreach item $optdata {
        foreach {optName behavior varlist} $item {
            if {[llength $varlist] < 1 || [lsearch -exact {set lappend} $behavior] == -1} {
                return -code error "usage: init { {optname <set|lappend> {var1 var2 ...}} ... }"
            }
            set optlist($optName) $varlist
            set optbehavior($optName) $behavior
        }
    }
}

proc opt_parser::get_options {argv {opts {}}} {
    # second argument are internal options
    # (like 'ignore_unknown_flags <0|1>')
    foreach {k v} $opts {set $k $v}
    set ignore_unknown_flags 0

    variable optlist
    variable optbehavior
    variable optprefix

    # zero all the options 1st var
    foreach optName [array names optlist] {
        uplevel [list set [lindex $optlist($optName) 0] 0]
        if {$optbehavior($optName) == {lappend}} {
            for {set i 1} {$i < [llength $optlist($optName)]} {incr i} {
                uplevel [list set [lindex $optlist($optName) $i] [list]]
            }
        }
    }

    # here will be appended non-options arguments
    set residualArgs {}

    set argc [llength $argv]
    for {set i 0} {$i < $argc} {} {
        # get i-th arg
        set optName [lindex $argv $i]
        incr i

        # if it's not an option, stop here, and add to residualArgs
        if {![regexp ^$optprefix $optName]} {
            lappend residualArgs $optName
            continue
        }

        if {[info exists optlist($optName)]} {
            set varlist $optlist($optName)
            uplevel [list set [lindex $optlist($optName) 0] 1]
            set n_required_opt_args [expr {-1+[llength $varlist]}]
            set j 1
            while {$n_required_opt_args > 0} {
                incr n_required_opt_args -1
                if {$i >= $argc} {
                    return -code error "not enough arguments for option $optName"
                }
                uplevel [list $optbehavior($optName) [lindex $varlist $j] [lindex $argv $i]]
                incr j
                incr i
            }
        } else {
            if {$ignore_unknown_flags} {
                lappend residualArgs $argv_i
                continue
            } else {
                return -code error "unknown option: $optName"
            }
        }
    }
    return $residualArgs
}
