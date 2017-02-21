#
# Copyright (c) 1997-2009 Miller Puckette.
# Copyright (c) 2011 Yvan Volochine.
#(c) 2008 WordTech Communications LLC. License: standard Tcl license, http://www.tcl.tk/software/tcltk/license.html

package provide pd_guiprefs 0.1


namespace eval ::pd_guiprefs:: {
    namespace export init
    namespace export write_recentfiles
    namespace export update_recentfiles
}

# FIXME should these be globals ?
set ::recentfiles_key ""
set ::pd_guiprefs::domain ""


#################################################################
# global procedures
#################################################################
# ------------------------------------------------------------------------------
# init preferences
#
proc ::pd_guiprefs::init {} {
    switch -- $::windowingsystem {
        "aqua" {
            init_aqua
            # ------------------------------------------------------------------------------
            # osx: read a plist file
            #
            proc ::pd_guiprefs::get_config {adomain {akey} {arr false}} {
                if {![catch {exec defaults read $adomain $akey} conf]} {
                    if {$arr} {
                        set conf [plist_array_to_tcl_list $conf]
                    }
                } else {
                    # initialize NSRecentDocuments with an empty array
                    exec defaults write $adomain $akey -array
                    set conf {}
                }
                return $conf
            }
            # ------------------------------------------------------------------------------
            # write configs to plist file
            # if $arr is true, we write an array
            #
            proc ::pd_guiprefs::write_config {data {adomain} {akey} {arr false}} {
                # FIXME empty and write again so we don't loose the order
                if {[catch {exec defaults write $adomain $akey -array} errorMsg]} {
                    ::pdwindow::error "write_config $akey: $errorMsg\n"
                }
                if {$arr} {
                    foreach filepath $data {
                        set escaped [escape_for_plist $filepath]
                        exec defaults write $adomain $akey -array-add "$escaped"
                    }
                } else {
                    set escaped [escape_for_plist $data]
                    exec defaults write $adomain $akey '$escaped'
                }

                # Disable window state saving by default for 10.7+ as there is a chance
                # pd will hang on start due to conflicting patch resources until the state
                # is purged. State saving will still work, it just has to be explicitly
                # asked for by holding the Option/Alt button when quitting via the File
                # menu or with the Cmd+Q key binding.
                exec defaults write $adomain NSQuitAlwaysKeepsWindows -bool false
                return
            }

        }
        "win32" {
            init_win
            # ------------------------------------------------------------------------------
            # w32: read in the registry
            #
            proc ::pd_guiprefs::get_config {adomain {akey} {arr false}} {
                package require registry
                if {![catch {registry get $adomain $akey} conf]} {
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
                if {$arr} {
                    if {[catch {registry set $adomain $akey $data multi_sz} errorMsg]} {
                        ::pdwindow::error "write_config $data $akey: $errorMsg\n"
                    }
                } else {
                    if {[catch {registry set $adomain $akey $data sz} errorMsg]} {
                        ::pdwindow::error "write_config $data $akey: $errorMsg\n"
                    }
                }
                return
            }

        }
        "x11" {
            init_x11
            # ------------------------------------------------------------------------------
            # linux: read a config file and return its lines splitted.
            #
            proc ::pd_guiprefs::get_config {adomain {akey} {arr false}} {
                set filename [file join $adomain ${akey}.conf]
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
            # linux: write configs to USER_APP_CONFIG_DIR
            # $arr is true if the data needs to be written in an array
            #
            proc ::pd_guiprefs::write_config {data {adomain} {akey} {arr false}} {
                # right now I (yvan) assume that data are just \n separated, i.e. no keys
                set data [join $data "\n"]
                set filename [file join $adomain ${akey}.conf]
                if {[catch {set fl [open $filename w]} errorMsg]} {
                    ::pdwindow::error "write_config $data $akey: $errorMsg\n"
                } else {
                    puts -nonewline $fl $data
                    close $fl
                }
            }
        }
    }
    # assign gui preferences
    # osx special case for arrays
    set arr [expr { $::windowingsystem eq "aqua" }]
    set ::recentfiles_list ""
    catch {set ::recentfiles_list [get_config $::pd_guiprefs::domain \
        $::recentfiles_key $arr]}
}

proc ::pd_guiprefs::init_aqua {} {
    # osx has a "Open Recent" menu with 10 recent files (others have 5 inlined)
    set ::pd_guiprefs::domain org.puredata
    set ::recentfiles_key "NSRecentDocuments"
    set ::total_recentfiles 10
}

proc ::pd_guiprefs::init_win {} {
    # windows uses registry
    set ::pd_guiprefs::domain "HKEY_CURRENT_USER\\Software\\Pure-Data"
    set ::recentfiles_key "RecentDocs"
}

proc ::pd_guiprefs::init_x11 {} {
    # linux uses ~/.config/pure-data dir
    set ::pd_guiprefs::domain "~/.config/pure-data"
    set ::recentfiles_key "recentfiles"
    prepare_configdir
}

# ------------------------------------------------------------------------------
# write recent files
#
proc ::pd_guiprefs::write_recentfiles {} {
    write_config $::recentfiles_list $::pd_guiprefs::domain $::recentfiles_key true
}

# ------------------------------------------------------------------------------
# this is called when opening a document (wheredoesthisshouldgo.tcl)
#
proc ::pd_guiprefs::update_recentfiles {afile} {
    # remove duplicates first
    set index [lsearch -exact $::recentfiles_list $afile]
    set ::recentfiles_list [lreplace $::recentfiles_list $index $index]
    # insert new one in the beginning and crop the list
    set ::recentfiles_list [linsert $::recentfiles_list 0 $afile]
    set ::recentfiles_list [lrange $::recentfiles_list 0 $::total_recentfiles]
    ::pd_menus::update_recentfiles_menu
}

#################################################################
# main read/write procedures
#################################################################

## these are stubs that will be overwritten in ::pd_guiprefs::init()
proc ::pd_guiprefs::write_config {data {adomain} {akey} {arr false}} {
    ::pdwindow::error "::pd_guiprefs::write_config not implemented for $::windowingsystem\n"
}
proc ::pd_guiprefs::get_config {adomain {akey} {arr false}} {
    ::pdwindow::error "::pd_guiprefs::get_config not implemented for $::windowingsystem\n"
}

# the new API
proc ::pd_guiprefs::write {key data {arr false} {domain $::pd_guiprefs::domain}} {
    puts "::pd_guiprefs::write '${key}' '${data}' '${arr}' '${domain}'"
}

#################################################################
# utils
#################################################################

# ------------------------------------------------------------------------------
# linux only! : look for pd config directory and create it if needed
#
proc ::pd_guiprefs::prepare_configdir {} {
    if { [catch {
        if {[file isdirectory $::pd_guiprefs::domain] != 1} {
            file mkdir $::pd_guiprefs::domain
            ::pdwindow::debug "$::pd_guiprefs::domain was created.\n"
            }
    }]} {
        ::pdwindow::error "$::pd_guiprefs::domain was *NOT* created.\n"
    }
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
    return \"[regsub -all -- {"} $str {\\"}]\"
}
