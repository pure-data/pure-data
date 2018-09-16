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
# if set to "DISABLED", the docs dir functionality is disabled
set ::pd_docsdir::docspath ""

# self-init after loading
after 1000 {::pd_docsdir::init}

# check the Pd documents directory path & prompt user to create if it's empty,
# otherwise ignore all of this if the user cancelled it
# set reset to true to force the welcome prompt
proc ::pd_docsdir::init {{reset false}} {
    if {$reset} {
        set ::pd_docsdir::docspath ""
    } else {
        set ::pd_docsdir::docspath [::pd_guiprefs::read docspath]
    }
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
        set msg [_ "Do you want Pd to create a documents directory for patches and external libraries?\n\nLocation: %s\n\nYou can change or disable this later in the Path preferences." ]
        set _args "-message \"[format $msg $docspath_default]\" -type yesnocancel -default yes -icon question -parent .pdwindow"
        switch -- [eval tk_messageBox ${_args}] {
            yes {
                set docspath $docspath_default
                # set the new docs path
                if {![::pd_docsdir::create_path $docspath]} {
                    # didn't work
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
        if {[::pd_docsdir::create_externals_path]} {
            ::pd_docsdir::add_externals_path
            if {[namespace exists ::deken]} {
                # clear first so set_installpath doesn't pick up prev value from guiprefs
                set ::deken::installpath ""
                :::deken::set_installpath [::pd_docsdir::get_externals_path]
            }
        }
        focus .pdwindow
    } else {
        # check saved setting
        set fullpath [file normalize "$docspath"]
        if {![file exists $fullpath]} {
            set msg [_ "Pd documents directory cannot be found:\n\n%s\n\nChoose a new location?" ]
            set _args "-message \"[format $msg $::pd_docsdir::docspath]\" -type yesno -default yes -icon warning -parent .pdwindow"
            switch -- [eval tk_messageBox ${_args}] {
                yes {
                    # set the new docs path
                    set newpath [tk_chooseDirectory -title [_ "Choose Pd documents directory:"] \
                                                    -initialdir $::env(HOME)]
                    if {$newpath ne ""} {
                        if{![::pd_docsdir::update_path $newpath]} {
                            # didn't work
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

# update the Pd documents directory path & externals path, creates if needed
# higher level complement to create_path & path_set
# checks ::deken::installpath value
# returns 1 if the update was successful
proc ::pd_docsdir::update_path {path} {
    if {"$path" eq "" || "$path" eq "DISABLED"} {
        ::pd_docsdir::set_path "$path"
    } else {
        if {![::pd_docsdir::create_path "$path"]} {return 0}
        ::pd_docsdir::set_path "$path"
        # only try creating the externals path if it is the deken installpath,
        # this keeps the installpath from changing arbitrarily
        if {![namespace exists ::deken] || [::pd_docsdir::is_externals_path_deken_installpath]} {
            if {[::pd_docsdir::create_externals_path]} {
                ::pd_docsdir::add_externals_path
            }
        }
    }
    return 1
}

# create the Pd documents directory path and it's "externals" subdir,
# set externalsdir to false to create the given path only
# returns true if the path already exists
proc ::pd_docsdir::create_path {path} {
    if {"$path" eq "" || "$path" eq "DISABLED"} {return 0}
    if {[file mkdir [file normalize "$path"]] eq ""} {
        return 1
    }
    ::pdwindow::error [format [_ "Couldn't create Pd documents directory: %s\n"] $path]
    return 0
}

# set the Pd documents directory path
proc ::pd_docsdir::set_path {path} {
    set ::pd_docsdir::docspath $path
    ::pd_guiprefs::write docspath "$path"
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

# create the externals subdir within the current docs path or a given path,
# returns 1 on success
proc ::pd_docsdir::create_externals_path {{path ""}} {
    if {"$path" eq ""} {
        if {"$::pd_docsdir::docspath" eq "" ||
            "$::pd_docsdir::docspath" eq "DISABLED"} {
            return 0
        }
        set path $::pd_docsdir::docspath
    }
    set newpath [file join [file normalize "$path"] "externals"]
    if {[file mkdir "$newpath" ] eq ""} {
        return 1
    }
    ::pdwindow::error [format [_ "Couldn't create \"externals\" directory in: %s\n"] $path]
    return 0
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
    return [file join "$path" "externals"]
}

# returns 1 if the docspath externals subdir exists
proc ::pd_docsdir::externals_path_is_valid {} {
    set path [::pd_docsdir::get_externals_path]
    if {"$path" ne "" && [file writable [file normalize "$path"]]} {
        return 1
    }
    return 0
}

# add the externals subdir within the current docs path or a given path to the
# the user search paths
proc ::pd_docsdir::add_externals_path {{path ""}} {
    set externalspath [::pd_docsdir::get_externals_path $path]
    if {$externalspath ne ""} {
        add_to_searchpaths $externalspath
    }
}

# is the deken installpath the externals subdir within the current docs path?
proc ::pd_docsdir::is_externals_path_deken_installpath {} {
    if {![namespace exists ::deken]} {return 0}
    if {[file normalize $::deken::installpath] eq
        [file normalize [::pd_docsdir::get_externals_path]]} {
        return 1
    }
    return 0
}
