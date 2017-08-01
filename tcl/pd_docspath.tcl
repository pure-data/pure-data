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

namespace eval ::pd_docspath:: {
    variable docspath
    namespace export init
    namespace export get_defaultpath
    namespace export createpath
    namespace export setpath
    namespace export is_valid
    namespace export get_externalspath
    namespace export externalspath_is_valid
}

# if empty, the user is prompted about creating this
# if set to "DISABLED", the docs path functionality is disabled
set ::pd_docspath::docspath ""

# self-init after loading
after 1000 {::pd_docspath::init}

# check the Pd documents directory path & prompt user to create if it's empty,
# otherwise ignore all of this if the user cancelled it
proc ::pd_docspath::init {} {
    set ::pd_docspath::docspath [::pd_guiprefs::read docspath]
    set docsdir $::pd_docspath::docspath
    if { "$docsdir" eq "DISABLED" } {
        # respect prev decision
        return
    } elseif { "$docsdir" eq "" } {
        # sanity check, Pd might be running on a non-writable install
        if {![file exists $::env(HOME)] || ![file writable $::env(HOME)]} {
            return
        }
        # default location: Documents dir if it exists, otherwise use home
        set docsdir_default [file join $::env(HOME) "Documents"]
        if {![file exists $docsdir_default]} {
            set docsdir_default $::env(HOME)
        }
        if {![file writable $docsdir_default]} {
            # sanity check
            set docsdir "DISABLED"
            return
        }
        set docsdir_default [file join $docsdir_default "Pd"]
        # prompt
        set msg [_ "Welcome to Pure Data!\n\nDo you want Pd to create a documents directory for patches and external libraries?\n\nLocation: %s\n\nYou can change or disable this later in the Path preferences." ]
        set _args "-message \"[format $msg $docsdir_default]\" -type yesnocancel -default yes -icon question -parent .pdwindow"
        switch -- [eval tk_messageBox ${_args}] {
            yes {
                set docsdir $docsdir_default
                # set the new docs path
                if {![::pd_docspath::createpath $docsdir]} {
                    # didn't work
                    ::pdwindow::error [format [_ "Couldn't create Pd documents directory: %s"] $docsdir]
                    return
                }
            }
            no {
                # bug off
                set docsdir "DISABLED"
            }
            cancel {
                # defer
                return
            }
        }
        ::pd_docspath::setpath $docsdir
        focus .pdwindow
    } else {
        # check saved setting
        set fullpath [file normalize "$docsdir"]
        if {![file exists $fullpath]} {
            set msg [_ "Pd documents directory cannot be found:\n\n%s\n\nChoose a new location?" ]
            set _args "-message \"[format $msg $::pd_docspath::docspath]\" -type yesno -default yes -icon warning"
            switch -- [eval tk_messageBox ${_args}] {
                yes {
                    # set the new docs path
                    set newpath [tk_chooseDirectory -title [_ "Choose Pd documents directory:"] \
                                                    -initialdir $::env(HOME)]
                    if {$newpath ne ""} {
                        if {[::pd_docspath::createpath $docsdir]} {
                            # set the new docs path
                            ::pd_docspath::setpath $docsdir
                        } else {
                            # didn't work
                            ::pdwindow::error [format [_ "Couldn't create Pd documents directory: %s"] $docsdir]
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
    if {[::pd_docspath::is_valid]} {
        if {"$::filenewdir" eq "$::env(HOME)"} {
            set ::filenewdir $::pd_docspath::docspath
        }
        if {"$::fileopendir" eq "$::env(HOME)"} {
            set ::fileopendir $::pd_docspath::docspath
        }
    }
    ::pdwindow::verbose 0 "Pd documents directory: $::pd_docspath::docspath\n"
}

# try to find a default docs path, returns an empty string if not possible
proc ::pd_docspath::get_defaultpath {} {
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
        return  [file join $path "Pd"]
    }
    return ""
}

# create the Pd documents directory path and it's "externals" subdir,
# does nothing if paths already exist
proc ::pd_docspath::createpath {path} {
    if {"$path" eq "" || "$path" eq "DISABLED"} {return 0}
    set path [file join [file normalize "$path"] "externals"]
    if {[file mkdir "$path" ] eq ""} {
        return 1
    }
    return 0
}

# set the Pd documents directory path, adds "externals" subdir to search paths
proc ::pd_docspath::setpath {path} {
    set ::pd_docspath::docspath $path
    ::pd_guiprefs::write docspath "$path"
    if {"$path" ne "" && "$path" ne "DISABLED"} {
        set externalspath [file join "$path" "externals"]
        # set deken to use it
        if {[namespace exists ::deken]} {
            set ::deken::installpath ""
            :::deken::set_installpath $externalspath
        }
        add_to_searchpaths $externalspath
    }
}

# reset the Pd documents directory path to the default location on this system
# returns 1 on success
proc ::pd_docspath::reset {} {
    set path [::pd_docspath::get_defaultpath]
    if {[::pd_docspath::createpath $path]} {
        ::pd_docspath::setpath $path
        return 1
    }
    return 0
}

# disable the Pd documents directory feature
proc ::pd_docspath::disable {} {
    # specific "no docs path please" keyword
    ::pd_docspath::setpath "DISABLED"
    return 1
}

# returns 1 if the docspath is set, not disabled, & a directory
proc ::pd_docspath::is_valid {} {
    if {"$::pd_docspath::docspath" eq "" ||
        "$::pd_docspath::docspath" eq "DISABLED" ||
        ![file isdirectory "$::pd_docspath::docspath"]} {
        return 0
    }
    return 1
}

# get the externals subdir within the current docs path,
# returns an empty string if docs path is not valid
proc ::pd_docspath::get_externalspath {} {
    if {[::pd_docspath::is_valid]} {
        return [file join $::pd_docspath::docspath "externals"]
    }
    return ""
}

# returns 1 if the docspath externals subdir exists
proc ::pd_docspath::externalspath_is_valid {} {
    set path [::pd_docspath::get_externalspath]
    if {"$path" ne "" && [file writable [file normalize "$path"]]} {
        return 1
    }
    return 0
}
