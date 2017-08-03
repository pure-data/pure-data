# ------------------------------------------------------------------------------
# optional documents directory, implemented as a GUI plugin
#
# this feature provides a beginner-friendly location for patches & externals and
# is created by a dialog when first running Pd
#
# a subfolder for externals is automatically added to the user search paths and
# file dialogs will open in the docs path by default
#
# Dan Wilcox <danomatika.com> 2017
#

# The minimum version of TCL that allows the plugin to run
package require Tcl 8.4

package require pdwindow 0.1
package require pd_guiprefs 0.1
package require wheredoesthisgo

namespace eval ::pd_docsdir:: {
    variable docspath
    namespace export init
    namespace export get_default_path
    namespace export get_disabled_path
    namespace export create_path
    namespace export set_path
    namespace export path_is_valid
    namespace export get_default_externals_path
    namespace export get_externals_path
    namespace export externals_path_is_valid
}

# if empty, the user is prompted about creating this
# if set to "DISABLED", the docs dit functionality is disabled
set ::pd_docsdir::docspath ""

# self-init after loading
after 1000 {::pd_docsdir::init}

# check the Pd documents directory path & prompt user to create if it's empty,
# otherwise ignore all of this if the user cancelled it
proc ::pd_docsdir::init {} {
    set ::pd_docsdir::docspath [::pd_guiprefs::read docspath]
    set docspath $::pd_docsdir::docspath
    if { "$docspath" eq "DISABLED" } {
        # respect prev decision
        return
    } elseif { "$docspath" eq "" } {
        # sanity check, Pd might be running on a non-writable install
        if {![file exists $::env(HOME)] || ![file writable $::env(HOME)]} {
            return
        }
        # default location: Documents dir if it exists, otherwise use home
        set docspath_default [file join $::env(HOME) "Documents"]
        if {![file exists $docspath_default]} {
            set docspath_default $::env(HOME)
        }
        if {![file writable $docspath_default]} {
            # sanity check
            set docspath "DISABLED"
            return
        }
        set docspath_default [file join $docspath_default "Pd"]
        # prompt
        set msg [_ "Welcome to Pure Data!\n\nDo you want Pd to create a documents directory for patches and external libraries?\n\nLocation: %s\n\nYou can change or disable this later in the Path preferences." ]
        set _args "-message \"[format $msg $docspath_default]\" -type yesnocancel -default yes -icon question -parent .pdwindow"
        switch -- [eval tk_messageBox ${_args}] {
            yes {
                set docspath $docspath_default
                # set the new docs path
                if {![::pd_docsdir::create_path $docspath]} {
                    # didn't work
                    ::pdwindow::error [format [_ "Couldn't create Pd documents directory: %s"] $docspath]
                    return
                }
            }
            no {
                # bug off
                set docspath "DISABLED"
            }
            cancel {
                # defer
                return
            }
        }
        ::pd_docsdir::set_path $docspath
        focus .pdwindow
    } else {
        # check saved setting
        set fullpath [file normalize "$docspath"]
        if {![file exists $fullpath]} {
            set msg [_ "Pd documents directory cannot be found:\n\n%s\n\nChoose a new location?" ]
            set _args "-message \"[format $msg $::pd_docsdir::docspath]\" -type yesno -default yes -icon warning"
            switch -- [eval tk_messageBox ${_args}] {
                yes {
                    # set the new docs path
                    set newpath [tk_chooseDirectory -title [_ "Choose Pd documents directory:"] \
                                                    -initialdir $::env(HOME)]
                    if {$newpath ne ""} {
                        if {[::pd_docsdir::create_path $docspath]} {
                            # set the new docs path
                            ::pd_docsdir::set_path $docspath
                        } else {
                            # didn't work
                            ::pdwindow::error [format [_ "Couldn't create Pd documents directory: %s"] $docspath]
                            return
                        }
                    }
                }
                no {
                    # defer
                    return
                }
            }
            focus .pdwindow
        }
    }
    # open dialogs in docs dir if GUI was started first
    if {[::pd_docsdir::path_is_valid]} {
        if {"$::filenewdir" eq "$::env(HOME)"} {
            set ::filenewdir $::pd_docsdir::docspath
        }
        if {"$::fileopendir" eq "$::env(HOME)"} {
            set ::fileopendir $::pd_docsdir::docspath
        }
    }
    ::pdwindow::verbose 0 "Pd documents directory: $::pd_docsdir::docspath\n"
}

# try to find a default docs path, returns an empty string if not possible
proc ::pd_docsdir::get_default_path {} {
     # sanity check, Pd might be running on a non-writable install
    if {![file exists $::env(HOME)] || ![file writable $::env(HOME)]} {
        return ""
    }
    # default location: Documents dir if it exists, otherwise use home
    set path [file join $::env(HOME) "Documents"]
    if {![file exists $path]} {
        set path $::env(HOME)
    }
    if {[file writable $path]} {
        return [file join $path "Pd"]
    }
    return ""
}

# get the default disabled path value
proc ::pd_docsdir::get_disabled_path {} {
    return "DISABLED"
}

# create the Pd documents directory path and it's "externals" subdir,
# does nothing if paths already exist
proc ::pd_docsdir::create_path {path} {
    if {"$path" eq "" || "$path" eq "DISABLED"} {return 0}
    set path [file join [file normalize "$path"] "externals"]
    if {[file mkdir "$path" ] eq ""} {
        return 1
    }
    return 0
}

# set the Pd documents directory path, adds "externals" subdir to search paths
proc ::pd_docsdir::set_path {path {setdekenpath true}} {
    set ::pd_docsdir::docspath $path
    ::pd_guiprefs::write docspath "$path"
    if {"$path" ne "" && "$path" ne "DISABLED"} {
        set externalspath [file join "$path" "externals"]
        # set deken to use it
        if {[namespace exists ::deken] && $setdekenpath} {
            set ::deken::installpath ""
            :::deken::set_installpath $externalspath
        }
        add_to_searchpaths $externalspath
    }
}

# returns 1 if the docspath is set, not disabled, & a directory
proc ::pd_docsdir::path_is_valid {} {
    if {"$::pd_docsdir::docspath" eq "" ||
        "$::pd_docsdir::docspath" eq "DISABLED" ||
        ![file isdirectory "$::pd_docsdir::docspath"]} {
        return 0
    }
    return 1
}

# get the externals subdir within the current docs path or a given path,
# returns an empty string if docs path is not valid
proc ::pd_docsdir::get_externals_path {{path ""}} {
    if {"$path" eq ""} {
        if {[::pd_docsdir::path_is_valid]} {
            return [file join $::pd_docsdir::docspath "externals"]
        } else {
            return ""
        }
    }
    return [file join $path "externals"]
}

# returns 1 if the docspath externals subdir exists
proc ::pd_docsdir::externals_path_is_valid {} {
    set path [::pd_docsdir::get_externals_path]
    if {"$path" ne "" && [file writable [file normalize "$path"]]} {
        return 1
    }
    return 0
}
