package provide opt_parser 0.1

namespace eval opt_parser {
    # list of option vars (keys are long option names)
    variable optlist
    variable optprefix {-}
}

proc opt_parser::init {optdata} {
    variable optlist
    array unset optlist
    array set optlist {}
    foreach item $optdata {
        foreach {longname varlist} $item {
            if {[llength $varlist] < 1} {
                return -code error "usage: init { {optname {var1 var2 ...}} ... }"
            }
            set optlist($longname) $varlist
        }
    }
}

proc opt_parser::get_options {argv {opts {}}} {
    set ignore_unknown_flags 0
    foreach {k v} $opts {set $k $v}

    variable optlist
    variable optprefix

    # zero all the options 1st var
    foreach optName [array names optlist] {
        uplevel [list set [lindex $optlist($optName) 0] 0]
        for {set i 1} {$i < [llength $optlist($optName)]} {incr i} {
            uplevel [list set [lindex $optlist($optName) $i] [list]]
        }
    }

    # here will be appended non-options arguments
    set residualArgs {}

    set argc [llength $argv]
    for {set i 0} {$i < $argc} {} {
        # get i-th arg
        set argv_i [lindex $argv $i]
        incr i

        # if it's not an option, stop here, and add to residualArgs
        if {![regexp ^$optprefix $argv_i]} {
            lappend residualArgs $argv_i
            continue
        }

        set optName [regsub ^$optprefix $argv_i {}]
        if {[info exists optlist($optName)]} {
            set varlist $optlist($optName)
            uplevel [list set [lindex $optlist($optName) 0] 1]
            set n_required_opt_args [expr {-1+[llength $varlist]}]
            set j 1
            while {$n_required_opt_args > 0} {
                incr n_required_opt_args -1
                if {$i >= $argc} {
                    return -code error "not enough arguments for option $optprefix$optName"
                }
                uplevel [list lappend [lindex $varlist $j] [lindex $argv $i]]
                incr j
                incr i
            }
        } else {
            if {$ignore_unknown_flags} {
                lappend residualArgs $argv_i
                continue
            } else {
                return -code error "unknown option: $optprefix$optName"
            }
        }
    }
    return $residualArgs
}
