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

# preference keys
set ::pd_guiprefs::recentfiles_key ""
set ::pd_guiprefs::loglevel_key "loglevel"

# platform specific
set ::pd_guiprefs::domain ""
set ::pd_guiprefs::configdir ""
set ::pd_guiprefs::recentfiles_is_array false

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
            # macOS has a "Open Recent" menu with 10 recent files (others have 5 inlined)
            set ::pd_guiprefs::recentfiles_key "NSRecentDocuments"
            set ::total_recentfiles 10
            # store recent files as an array, not a string
            set ::pd_guiprefs::recentfiles_is_array true

            # ------------------------------------------------------------------------------
            # macOS: read a plist file using the 'defaults' command
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
                        set conf ""
                    }
                }
                return $conf
            }
            # ------------------------------------------------------------------------------
            # macOS: write configs to plist file using the 'defaults' command
            # if $arr is true, we write an array
            #
            proc ::pd_guiprefs::write_config {data {adomain} {akey} {arr false}} {
                if {$arr} {
                    # FIXME empty and write again so we don't lose the order
                    if {[catch {exec defaults write $adomain $akey -array} errorMsg]} {
                        puts "write_config $akey: $errorMsg\n"
                    }
                    foreach item $data {
                        set escaped [escape_for_plist $item]
                        if {[catch {eval exec defaults write $adomain $akey -array-add $escaped} errorMsg]} {
                            puts "write_config $akey: $errorMsg\n"
                        }
                    }
                } else {
                    set escaped [escape_for_plist $data]
                    if {[catch {exec defaults write $adomain $akey $escaped} errorMsg]} {
                        puts "write_config $akey: $errorMsg\n"
                    }
                }
                return
            }

            # Disable window state saving by default for 10.7+ as there is a chance
            # pd will hang on start due to conflicting patch resources until the state
            # is purged. State saving will still work, it just has to be explicitly
            # asked for by holding the Option/Alt button when quitting via the File
            # menu or with the Cmd+Q key binding.
            exec defaults write $::pd_guiprefs::domain NSQuitAlwaysKeepsWindows -bool false

            # Disable Character Accent Selector popup so key repeat works for all keys.
            exec defaults write $::pd_guiprefs::domain ApplePressAndHoldEnabled -bool false
        }
        "registry" {
            # windows uses registry
            set ::pd_guiprefs::registrypath "HKEY_CURRENT_USER\\Software\\Pure-Data"
            set ::pd_guiprefs::recentfiles_key "RecentDocs"

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
            set ::pd_guiprefs::recentfiles_key "recentfiles"
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
            ::pdwindow::error "unknown gui preferences backend '$backend'.\n"
        }

    }
    # init gui preferences
    set ::recentfiles_list [::pd_guiprefs::init_config $::pd_guiprefs::domain \
                                                       $::pd_guiprefs::recentfiles_key \
                                                       $::recentfiles_list \
                                                       $::pd_guiprefs::recentfiles_is_array]
    set ::loglevel [::pd_guiprefs::init_config $::pd_guiprefs::domain \
                                               $::pd_guiprefs::loglevel_key \
                                               $::loglevel]
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
        }
    }]} {
        set absconfdir ${::pd_guiprefs::configdir}
        catch { set absconfdir [file normalize ${::pd_guiprefs::configdir} ] }

        ::pdwindow::error [format [_ "Couldn't create preferences \"%1\$s\" in %2\$s" ] $domain $absconfdir]
        ::pdwindow::error "\n"
    }
    return $domain
}

# ------------------------------------------------------------------------------
# convenience proc to init prefs value, returns default if not found
#
proc ::pd_guiprefs::init_config {adomain akey {default ""} {arr false}} {
    set conf ""
    catch {set conf [::pd_guiprefs::get_config $adomain $akey $arr]}
    if {$conf eq ""} {set conf $default}
    return $conf
}

# ------------------------------------------------------------------------------
# convert array returned by macOS 'defaults' command into a tcl list (thanks hc)
#
# ie. defaults output of
#     (
#        "/path1/hello.pd",
#        "/path2/world.pd",
#        "/foo/bar/baz.pd"
#     )
#
# becomes: "/path1/hello.pd /path2/world.pd /foo/bar/baz.pd"
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
        # trim any enclosing single quotes that
        # might have been saved previously
        set filename [string trim $file ']
        lappend result $filename
    }
    return $result
}

# escape tcl characters & quote with single quotes for macOS 'defaults' command
#
# strings that don't need major quoting pass through & don't need to be quoted
# while others strangely do (found via trial and error), this is ensures the
# single quotes do not pass through and are saved with the string
#
# FIXME:
#   * " are not escaped
#   * \ seem to be swallowed
#   * mixing ' & parens doesn't work
#
# at this point, we hope people don't have too many exotic filenames...
#
proc ::pd_guiprefs::escape_for_plist {str} {
    set quote 0
    set result $str
    regsub -all -- { } $result {\\ } result
    set quote [expr [regsub -all -- {'} $result {\\'} result] || $quote]
    set quote [expr [regsub -all -- {\(} $result {\\(} result] || $quote]
    set quote [expr [regsub -all -- {\)} $result {\\)} result] || $quote]
    set quote [expr [regsub -all -- {\[} $result {\\[} result] || $quote]
    set quote [expr [regsub -all -- {\]} $result {\\]} result] || $quote]
    set quote [expr [regsub -all -- {\{} $result {\\\{} result] || $quote]
    set quote [expr [regsub -all -- {\}} $result {\\\}} result] || $quote]
    if {$quote} {
        return '$result'
    } else {
        return $result
    }
}

#################################################################
# recent files
#################################################################

# ------------------------------------------------------------------------------
# write recent files
#
proc ::pd_guiprefs::write_recentfiles {} {
    write_config $::recentfiles_list $::pd_guiprefs::domain \
                 $::pd_guiprefs::recentfiles_key \
                 $::pd_guiprefs::recentfiles_is_array
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
    write_config $::loglevel $::pd_guiprefs::domain $::pd_guiprefs::loglevel_key
}
