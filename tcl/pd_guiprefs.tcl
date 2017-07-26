#
# Copyright (c) 1997-2009 Miller Puckette.
# Copyright (c) 2011 Yvan Volochine.
# Copyright (c) 2017 IOhannes m zmölnig.
# Copyright (c) 2008 WordTech Communications LLC.
#               License: standard Tcl license, http://www.tcl.tk/software/tcltk/license.html

package provide pd_guiprefs 0.1

namespace eval ::pd_guiprefs:: {
    namespace export init
    namespace export write_recentfiles
    namespace export update_recentfiles
    namespace export write_loglevel
}

# FIXME should these be globals ?
set ::recentfiles_key ""
set ::loglevel_key "loglevel"
set ::pd_guiprefs::domain ""
set ::pd_guiprefs::configdir ""
set ::recentfiles_is_array false

#################################################################
# perferences storage locations
#
# legacy
#   registry
#    HKEY_CURRENT_USER\Software\Pure-Data <key>:<value>
#    domain: HKEY_CURRENT_USER\Software\Pure-Data
#   plist
#    org.puredata <key> <value>
#    domain: org.puredata
#   linux:
#    ~/.config/pure-data/<key>.conf
#    domain: ~/.config/pure-data/
#
# new
#   plist
#    org.puredata.pd.pd-gui <key> <value>
#    domain: org.puredata.pd-gui
#   registry
#    HKEY_CURRENT_USER\Software\Pure-Data\org.puredata <key>:<value>
#    domain: org.puredata.pd-gui
#   file
#    Linux: ~/.config/pd/org.puredata/<key>.conf
#       - env(XDG_CONFIG_HOME)=~/.config/
#       - env(PD_CONFIG_DIR)=~/.config/pd/
#       - domain=org.puredata.pd-gui
#    OSX  : ~/Library/Preferences/Pd/org.puredata/<key>.conf
#       - env(PD_CONFIG_DIR)=~/Library/Preferences/Pd/
#       - domain=org.puredata.pd-gui
#    W32  : %AppData%\Pd\.config\org.puredata\<key>.conf
#       - env(PD_CONFIG_DIR)=%AppData%\Pd\.config
#       - domain=org.puredata.pd-gui
#
#################################################################

#################################################################
# global procedures
#################################################################
# ------------------------------------------------------------------------------
# init preferences
#
proc ::pd_guiprefs::init {} {
    set ::pd_guiprefs::domain org.puredata.pd.pd-gui

    switch -- $::platform {
        "Darwin" {
            set backend "plist"
        }
        "W32" {
            set backend "registry"
        }
        default {
            set backend "file"
        }
    }
    # let the user force the cross-platform 'file' backend
    if {[info exists ::env(PD_CONFIG_DIR)]} {
        set backend "file"
    }

    switch -- $backend {
        "plist" {
            # osx has a "Open Recent" menu with 10 recent files (others have 5 inlined)
            set ::recentfiles_key "NSRecentDocuments"
            set ::total_recentfiles 10
            # osx special case for arrays
            set ::recentfiles_is_array true

            # ------------------------------------------------------------------------------
            # osx: read a plist file
            #
            proc ::pd_guiprefs::get_config {adomain {akey} {arr false}} {
                if {![catch {exec defaults read $adomain $akey} conf]} {
                    if {$arr} {
                        set conf [plist_array_to_tcl_list $conf]
                    }
                } else {
                    # value not found, so set empty value
                    if {$arr} {
                        # initialize w/ empty array for NSRecentDocuments, etc
                        exec defaults write $adomain $akey -array
                        set conf {}
                    } else {
                        # not an array
                        exec defaults write $adomain $akey ""
                        set ""
                    }
                }
                return $conf
            }
            # ------------------------------------------------------------------------------
            # osx: write configs to plist file
            # if $arr is true, we write an array
            #
            proc ::pd_guiprefs::write_config {data {adomain} {akey} {arr false}} {
                # FIXME empty and write again so we don't lose the order
                if {[catch {exec defaults write $adomain $akey -array} errorMsg]} {
                    ::pdwindow::error "write_config $akey: $errorMsg\n"
                }
                # invoke /bin/sh -c to pass command as a string. This ensures
                # the shell interprets the single quotes around the value so the
                # quotes don't end up being saved as part of the string itself.
                # Also use the -r for a restricted shell to be safe.
                if {$arr} {
                    foreach filepath $data {
                        set escaped [escape_for_plist $filepath]
                        exec /bin/sh -rc "defaults write $adomain $akey -array-add $escaped"
                    }
                } else {
                    set escaped [escape_for_plist $data]
                    exec /bin/sh -rc "defaults write $adomain $akey $escaped"
                }
                return
            }

            # Disable window state saving by default for 10.7+ as there is a chance
            # pd will hang on start due to conflicting patch resources until the state
            # is purged. State saving will still work, it just has to be explicitly
            # asked for by holding the Option/Alt button when quitting via the File
            # menu or with the Cmd+Q key binding.
            exec defaults write $::pd_guiprefs::domain NSQuitAlwaysKeepsWindows -bool false
        }
        "registry" {
            # windows uses registry
            set ::pd_guiprefs::registrypath "HKEY_CURRENT_USER\\Software\\Pure-Data"
            set ::recentfiles_key "RecentDocs"

            # ------------------------------------------------------------------------------
            # w32: read in the registry
            #
            proc ::pd_guiprefs::get_config {adomain {akey} {arr false}} {
                package require registry
                set adomain [join [list ${::pd_guiprefs::registrypath} ${adomain}] \\]
                if {![catch {registry get ${adomain} $akey} conf]} {
                    return [expr {$conf}]
                }
                return {}
            }
            # ------------------------------------------------------------------------------
            # w32: write configs to registry
            # if $arr is true, we write an array
            #
            proc ::pd_guiprefs::write_config {data {adomain} {akey} {arr false}} {
                package require registry
                # FIXME: ugly
                set adomain [join [list ${::pd_guiprefs::registrypath} ${adomain}] \\]
                if {$arr} {
                    if {[catch {registry set ${adomain} $akey $data multi_sz} errorMsg]} {
                        ::pdwindow::error "write_config $data $akey: $errorMsg\n"
                    }
                } else {
                    if {[catch {registry set ${adomain} $akey $data sz} errorMsg]} {
                        ::pdwindow::error "write_config $data $akey: $errorMsg\n"
                    }
                }
                return
            }
        }
        "file" {
            set ::recentfiles_key "recentfiles"
            prepare_configdir ${::pd_guiprefs::domain}

            # ------------------------------------------------------------------------------
            # linux: read a config file and return its lines splitted.
            #
            proc ::pd_guiprefs::get_config {adomain {akey} {arr false}} {
                return [::pd_guiprefs::get_config_file $adomain $akey $arr]
            }
            # ------------------------------------------------------------------------------
            # linux: write configs to USER_APP_CONFIG_DIR
            # $arr is true if the data needs to be written in an array
            #
            proc ::pd_guiprefs::write_config {data {adomain} {akey} {arr false}} {
                return [::pd_guiprefs::write_config_file $data $adomain $akey $arr]
            }
        }
        default {
            ::pdwindow::error "Unknown configuration backend '$backend'.\n"
        }

    }
    # assign gui preferences
    set ::recentfiles_list {}
    catch {set ::recentfiles_list [get_config $::pd_guiprefs::domain \
                                       $::recentfiles_key $::recentfiles_is_array]}
    catch {set ::loglevel [get_config $::pd_guiprefs::domain $::loglevel_key]}
    # reset default if value was not found
    if {$::loglevel == ""} {set ::loglevel 2}
}

# ------------------------------------------------------------------------------
# read a config file and return its lines splitted.
#
proc ::pd_guiprefs::get_config_file {adomain {akey} {arr false}} {
    set filename [file join ${::pd_guiprefs::configdir} ${adomain} ${akey}.conf]
    set conf {}
    if {
        [file exists $filename] == 1
        && [file readable $filename]
    } {
        set fl [open $filename r]
        while {[gets $fl line] >= 0} {
            lappend conf $line
        }
        close $fl
    }
    return $conf
}
# ------------------------------------------------------------------------------
# write configs to USER_APP_CONFIG_DIR
# $arr is true if the data needs to be written in an array
#
proc ::pd_guiprefs::write_config_file {data {adomain} {akey} {arr false}} {
    ::pd_guiprefs::prepare_domain ${adomain}
    # right now I (yvan) assume that data are just \n separated, i.e. no keys
    set data [join $data "\n"]
    set filename [file join ${::pd_guiprefs::configdir} ${adomain} ${akey}.conf]
    if {[catch {set fl [open $filename w]} errorMsg]} {
        ::pdwindow::error "write_config $data $akey: $errorMsg\n"
    } else {
        puts -nonewline $fl $data
        close $fl
    }
}

#################################################################
# main read/write procedures
#################################################################

## these are stubs that will be overwritten in ::pd_guiprefs::init()
proc ::pd_guiprefs::write_config {data {adomain} {akey} {arr false}} {
    ::pdwindow::error "::pd_guiprefs::write_config not implemented for $::platform\n"
}
proc ::pd_guiprefs::get_config {adomain {akey} {arr false}} {
    ::pdwindow::error "::pd_guiprefs::get_config not implemented for $::platform\n"
}

# simple API (with a default domain)
proc ::pd_guiprefs::write {key data {arr false} {domain {}}} {
    if {"" eq $domain} { set domain ${::pd_guiprefs::domain} }
    set result [::pd_guiprefs::write_config $data $domain $key $arr]
    return $result
}
proc ::pd_guiprefs::read {key {arr false} {domain {}}} {
    if {"" eq $domain} { set domain ${::pd_guiprefs::domain} }
    set result [::pd_guiprefs::get_config $domain $key $arr]
    return $result
}

#################################################################
# utils
#################################################################

# ------------------------------------------------------------------------------
# file-backend only! : look for pd config directory and create it if needed
#
proc ::pd_guiprefs::prepare_configdir {domain} {
    set confdir ""
    switch -- $::platform {
        "W32" {
            # W32 uses %AppData%/Pd/.config dir
            # FIXXME: how to create hidden directories on W32??
            set confdir [file join $::env(AppData) Pd .config]
        }
        "OSX" {
            set confdir [file join ~ Library Preferences Pd]
        }
        default {
            # linux uses ~/.config/pure-data dir
            set confdir [file join ~ .config Pd]
            if {[info exists ::env(XDG_CONFIG_HOME)]} {
                set confdir [file join $::env(XDG_CONFIG_HOME) pd]
            }
        }
    }
    # let the user override the Pd-config-path
    if {[info exists ::env(PD_CONFIG_DIR)]} {
        if { "$::env(PD_CONFIG_DIR)" != "" } {
            set confdir $::env(PD_CONFIG_DIR)
        }
    }

    set ::pd_guiprefs::configdir $confdir
    set ::pd_guiprefs::domain $domain

    return [::pd_guiprefs::prepare_domain ${::pd_guiprefs::domain}]
}
proc ::pd_guiprefs::prepare_domain {{domain {}}} {
    if { "${domain}" == "" } {
        set domain ${::pd_guiprefs::domain}
    }
    if { [catch {
        set fullconfigdir [file join ${::pd_guiprefs::configdir} ${domain}]
        if {[file isdirectory $fullconfigdir] != 1} {
            file mkdir $fullconfigdir
            #::pdwindow::debug "$::pd_guiprefs::domain was created in $confdir.\n"
        }
    }]} {
        ::pdwindow::error "$::pd_guiprefs::domain was *NOT* created in $confdir.\n"
    }
    return $domain
}

# ------------------------------------------------------------------------------
# osx: handles arrays in plist files (thanks hc)
#
proc ::pd_guiprefs::plist_array_to_tcl_list {arr} {
    set result {}
    set filelist $arr
    regsub -all -- {("?),\s+("?)} $filelist {\1 \2} filelist
    regsub -all -- {\n} $filelist {} filelist
    regsub -all -- {^\(} $filelist {} filelist
    regsub -all -- {\)$} $filelist {} filelist
    regsub -line -- {^'(.*)'$} $filelist {\1} filelist
    regsub -all -- {\\\\U} $filelist {\\u} filelist

    foreach file $filelist {
        set filename [regsub -- {,$} $file {}]
        lappend result $filename
    }
    return $result
}

# the Mac OS X 'defaults' command uses single quotes to quote things,
# so they need to be escaped
proc ::pd_guiprefs::escape_for_plist {str} {
    return '[regsub -all -- {'} $str {\\'}]'
}

#################################################################
# recent files
#################################################################

# ------------------------------------------------------------------------------
# write recent files
#
proc ::pd_guiprefs::write_recentfiles {} {
    write_config $::recentfiles_list $::pd_guiprefs::domain $::recentfiles_key \
                 $::recentfiles_is_array
}

# ------------------------------------------------------------------------------
# this is called when opening a document (wheredoesthisshouldgo.tcl)
#
proc ::pd_guiprefs::update_recentfiles {afile {remove false}} {
    # remove duplicates first
    set index [lsearch -exact $::recentfiles_list $afile]
    set ::recentfiles_list [lreplace $::recentfiles_list $index $index]
    if {! $remove} {
        # insert new one in the beginning and crop the list
        set ::recentfiles_list [linsert $::recentfiles_list 0 $afile]
        set ::recentfiles_list [lrange $::recentfiles_list 0 $::total_recentfiles]
    }
    ::pd_menus::update_recentfiles_menu
}

#################################################################
# log level
#################################################################

# ------------------------------------------------------------------------------
# write log level
#
proc ::pd_guiprefs::write_loglevel {} {
    write_config $::loglevel $::pd_guiprefs::domain $::loglevel_key
}
