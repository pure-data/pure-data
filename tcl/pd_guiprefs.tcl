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
set ::recentfiles_domain ""


#################################################################
# global procedures
#################################################################
# ------------------------------------------------------------------------------
# init preferences
#
proc ::pd_guiprefs::init {} {
    switch -- $::windowingsystem {
        "aqua"  { init_aqua }
        "win32" { init_win }
        "x11"   { init_x11 }
    }
    # assign gui preferences
    # osx special case for arrays
    set arr [expr { $::windowingsystem eq "aqua" }]
    set ::recentfiles_list ""
    catch {set ::recentfiles_list [get_config $::recentfiles_domain \
        $::recentfiles_key $arr]}
}

proc ::pd_guiprefs::init_aqua {} {
    # osx has a "Open Recent" menu with 10 recent files (others have 5 inlined)
    set ::recentfiles_domain org.puredata
    set ::recentfiles_key "NSRecentDocuments"
    set ::total_recentfiles 10
}

proc ::pd_guiprefs::init_win {} {
    # windows uses registry
    set ::recentfiles_domain "HKEY_CURRENT_USER\\Software\\Pure-Data"
    set ::recentfiles_key "RecentDocs"
}

proc ::pd_guiprefs::init_x11 {} {
    # linux uses ~/.config/pure-data dir
    set ::recentfiles_domain "~/.config/pure-data"
    set ::recentfiles_key "recentfiles.conf"
    prepare_configdir
}

# ------------------------------------------------------------------------------
# write recent files
#
proc ::pd_guiprefs::write_recentfiles {} {
    write_config $::recentfiles_list $::recentfiles_domain $::recentfiles_key true
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

# ------------------------------------------------------------------------------
# get configs from a file or the registry
#
proc ::pd_guiprefs::get_config {adomain {akey} {arr}} {
    switch -- $::windowingsystem {
        "aqua"  { set conf [get_config_aqua $adomain $akey $arr] }
        "win32" { set conf [get_config_win $adomain $akey $arr] }
        "x11"   { set conf [get_config_x11 $adomain $akey $arr] }
    }
    return $conf
}

# ------------------------------------------------------------------------------
# write configs to a file or to the registry
# $arr is true if the data needs to be written in an array
#
proc ::pd_guiprefs::write_config {data {adomain} {akey} {arr false}} {
    switch -- $::windowingsystem {
        "aqua"  { write_config_aqua $data $adomain $akey $arr }
        "win32" { write_config_win $data $adomain $akey $arr }
        "x11"   { write_config_x11 $data $adomain $akey }
    }
}

#################################################################
# os specific procedures
#################################################################

# ------------------------------------------------------------------------------
# osx: read a plist file
#
proc ::pd_guiprefs::get_config_aqua {adomain {akey} {arr false}} {
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
# win: read in the registry
#
proc ::pd_guiprefs::get_config_win {adomain {akey} {arr false}} {
    package require registry
    if {![catch {registry get $adomain $akey} conf]} {
        return [expr {$conf}]
    } else {
        return {}
    }
}

# ------------------------------------------------------------------------------
# linux: read a config file and return its lines splitted.
#
proc ::pd_guiprefs::get_config_x11 {adomain {akey} {arr false}} {
    set filename [file join $adomain $akey]
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
# osx: write configs to plist file
# if $arr is true, we write an array
#
proc ::pd_guiprefs::write_config_aqua {data {adomain} {akey} {arr false}} {
    # FIXME empty and write again so we don't loose the order
    if {[catch {exec defaults write $adomain $akey -array} errorMsg]} {
        ::pdwindow::error "write_config_aqua $akey: $errorMsg"
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
}

# ------------------------------------------------------------------------------
# win: write configs to registry
# if $arr is true, we write an array
#
proc ::pd_guiprefs::write_config_win {data {adomain} {akey} {arr false}} {
    package require registry
    # FIXME: ugly
    if {$arr} {
        if {[catch {registry set $adomain $akey $data multi_sz} errorMsg]} {
            ::pdwindow::error "write_config_win $data $akey: $errorMsg"
        }
    } else {
        if {[catch {registry set $adomain $akey $data sz} errorMsg]} {
            ::pdwindow::error "write_config_win $data $akey: $errorMsg"
        }
    }
}

# ------------------------------------------------------------------------------
# linux: write configs to USER_APP_CONFIG_DIR
#
proc ::pd_guiprefs::write_config_x11 {data {adomain} {akey}} {
    # right now I (yvan) assume that data are just \n separated, i.e. no keys
    set data [join $data "\n"]
    set filename [file join $adomain $akey]
    if {[catch {set fl [open $filename w]} errorMsg]} {
        ::pdwindow::error "write_config_x11 $data $akey: $errorMsg"
    } else {
        puts -nonewline $fl $data
        close $fl
    }
}

#################################################################
# utils
#################################################################

# ------------------------------------------------------------------------------
# linux only! : look for pd config directory and create it if needed
#
proc ::pd_guiprefs::prepare_configdir {} {
    if { [catch {
        if {[file isdirectory $::recentfiles_domain] != 1} {
            file mkdir $::recentfiles_domain
            ::pdwindow::debug "$::recentfiles_domain was created.\n"
            }
    }]} {
                ::pdwindow::error "$::recentfiles_domain was *NOT* created.\n"
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

    foreach file $filelist {
        set filename [regsub -- {,$} $file {}]
        lappend result $filename
    }
    return $result
}

# the Mac OS X 'defaults' command uses single quotes to quote things,
# so they need to be escaped
proc ::pd_guiprefs::escape_for_plist {str} {
    return [regsub -all -- {'} $str {\\'}]
}
