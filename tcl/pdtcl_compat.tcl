####### tcl compatibility layer
## this package provides implementations of newer Tcl-features
## that are not found in old one implementations (like Tcl8.4)
## features are only provided if they are missing

## currently these are the functions provided:
## - 'dict'

## to use this do something like:
## > package require pdtcl_compat
## > catch {namespace import ::pdtcl_compat::dict}

package provide pdtcl_compat 0.1
namespace eval ::pdtcl_compat {
    namespace export dict
    namespace export lmap
}

## poor man's 'dict' implementation (for Tcl8.4)
# this only provides a limited set of sub-commands
# it is also slower than the built-in
proc ::pdtcl_compat::dict {command args} {
    #puts "<dict:${command}> $args"
    switch -- "${command}" {
        create {
            return {}
        }
        exist -
        exists {
            set dictionary [lindex ${args} 0]
            set key [lindex ${args} 1]
            foreach {k v} ${dictionary} {
                if { $k eq ${key} } {return 1}
            }
            return 0
        }
        get {
            set key [lindex ${args} 1]
            foreach {k v} [lindex ${args} 0] {
                if { $k eq ${key} } {return $v}
            }
            return {}
        }
        lappend {
            upvar [lindex ${args} 0] dictionary
            set key [lindex ${args} 1]
            set value [lrange ${args} 2 end]
            set index 0
            foreach {k v} ${dictionary} {
                if { $k eq ${key} } {
                    incr index
                    set dictionary \
                        [lreplace ${dictionary} ${index} ${index} \
                             [concat $v ${value}]]
                    return ${dictionary}

                }
                incr index 2
            }
            return [lappend dictionary ${key} ${value}]
        }
        set {
            upvar [lindex ${args} 0] dictionary
            set key [lindex ${args} 1]
            set value [lindex ${args} 2]
            set index 0
            foreach {k v} ${dictionary} {
                if { $k eq ${key} } {
                    incr index
                    set dictionary \
                        [lreplace ${dictionary} ${index} ${index} \
                             [concat $v ${value}]]
                    return ${dictionary}

                }
                incr index 2
            }
            return [lappend dictionary ${key} ${value}]
        }
        unset {
            upvar [lindex ${args} 0] dictionary
            set key [lindex ${args} 1]
            set index 0
            foreach {k value} ${dictionary} {
                if { $k eq ${key} } {
                    set dictionary \
                        [lreplace ${dictionary} ${index} ${index}+1]
                    return ${dictionary}

                }
                incr index 2
            }
            return ${dictionary}
        }
    }
}

## poor man's 'lmap' implementation (for Tcl8.4, Tcl8.5)
# https://wiki.tcl-lang.org/page/lmap
proc ::pdtcl_compat::lmap args {
    set body [lindex $args end]
    set args [lrange $args 0 end-1]
    set n 0
    set pairs [list]
    foreach {varname listval} $args {
        upvar 1 $varname var$n
        lappend pairs var$n $listval
        incr n
    }
    set temp [list]
    eval foreach $pairs [list {
        lappend temp [uplevel 1 $body]
    }]
    set temp
}

if { [catch {dict create} ] } {
    namespace import ::pdtcl_compat::dict
}
if { [catch {lmap _ {1 2 3} {list $_}} ] } {
    namespace import ::pdtcl_compat::lmap
}
