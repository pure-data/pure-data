# Copyright (c) 1997-2009 Miller Puckette.
#(c) 2008 WordTech Communications LLC. License: standard Tcl license, http://www.tcl.tk/software/tcltk/license.html

package provide pd_menus 0.1

package require pd_menucommands
package require pd_connect

# TODO figure out Undo/Redo/Cut/Copy/Paste state changes for menus

# since there is one menubar that is used for all windows, the menu -commands
# use {} quotes so that $::focused_window is interpreted when the menu item
# is called, not when the command is mapped to the menu item.  This is the
# opposite of the 'bind' commands in pd_bindings.tcl

namespace eval ::pd_menus:: {
    variable accelerator "Ctrl"
    if {[tk windowingsystem] eq "aqua"} {
        set accelerator "Cmd"
    }

    namespace export create_menubar
    namespace export configure_for_pdwindow
    namespace export configure_for_canvas
    namespace export configure_for_dialog

    # turn off tearoff menus globally
    option add *tearOff 0
}

# ------------------------------------------------------------------------------
#
proc ::pd_menus::create_menubar {} {
    variable accelerator
    # a menu for PatchWindows
    menu $::patch_menubar
    # a menu for the PdWindow
    menu $::pdwindow_menubar
    # a shared menu for preferences
    build_preferences_menu .preferences
    # a shared menu for recentfiles
    ::pd_menus::update_openrecent_menu .openrecent 0

    if {$::windowingsystem eq "aqua"} {
        create_apple_menu $::patch_menubar
        create_apple_menu $::pdwindow_menubar
    }

    foreach mymenu "file edit" {
        [format build_%s_menu $mymenu] [menu $::patch_menubar.$mymenu] 1
        $::patch_menubar add cascade -label [_ [string totitle $mymenu]] \
            -underline 0 -menu $::patch_menubar.$mymenu
        [format build_%s_menu $mymenu] [menu $::pdwindow_menubar.$mymenu] 0
        $::pdwindow_menubar add cascade -label [_ [string totitle $mymenu]] \
            -underline 0 -menu $::pdwindow_menubar.$mymenu
    }

    build_put_menu [menu $::patch_menubar.put]
    $::patch_menubar add cascade -label [_ Put] \
            -underline 0 -menu $::patch_menubar.put

    foreach mymenu "find media window tools help" {
        if {$mymenu eq "find"} {
            set underlined 3
        } {
            set underlined 0
        }
        # some GUI-plugins do expect that common menus are available under
        # ${::patch_menubar}.${mymenu} (e.g. ".menubar.help")
        set m [menu $::patch_menubar.$mymenu]
        [format build_%s_menu $mymenu] ${m}
        $::patch_menubar add cascade -label [_ [string totitle $mymenu]] \
            -underline $underlined -menu $m
        $::pdwindow_menubar add cascade -label [_ [string totitle $mymenu]] \
            -underline $underlined -menu $m
    }

    if {$::windowingsystem eq "win32"} {create_system_menu $::patch_menubar}
    . configure -menu $::patch_menubar
}

proc ::pd_menus::configure_for_pdwindow {} {
    pdtk_canvas_editmode .pdwindow 0
    # Help menu
    if {$::windowingsystem eq "aqua"} {
        ::pd_menus::reenable_help_items_aqua $::patch_menubar
    }
}

proc ::pd_menus::configure_for_canvas {mytoplevel} {
    pdtk_canvas_editmode $mytoplevel $::editmode($mytoplevel)
    update_undo_on_menu $mytoplevel $::undo_actions($mytoplevel) $::redo_actions($mytoplevel)
    # Help menu
    if {$::windowingsystem eq "aqua"} {
        ::pd_menus::reenable_help_items_aqua $::patch_menubar
    }
}

proc ::pd_menus::configure_for_dialog {mytoplevel} {
    pdtk_canvas_editmode $mytoplevel 0

    # Help menu
    if {$::windowingsystem eq "aqua"} {
        ::pd_menus::reenable_help_items_aqua $::patch_menubar
    }
}

proc ::pd_menus::menubar_for_dialog {mytoplevel} {
    set menubar $::dialog_menubar
    if {$::windowingsystem eq "aqua"} {
        set menubar $::pdwindow_menubar
    }
    $mytoplevel configure -menu $menubar
}


# ------------------------------------------------------------------------------
# menu building functions
proc ::pd_menus::build_file_menu {mymenu {patchwindow true}} {
    variable accelerator
    $mymenu add command -label [_ "New"]         -accelerator "$accelerator+N" -command {::pd_menucommands::scheduleAction menu_new}
    $mymenu add command -label [_ "Open"]        -accelerator "$accelerator+O" -command {::pd_menucommands::scheduleAction menu_open}

    $mymenu add cascade -label [_ "Open Recent"] -menu .openrecent
    $mymenu add separator
    $mymenu add command -label [_ "Close"]       -accelerator "$accelerator+W" -command {::pd_menucommands::scheduleAction ::pd_bindings::window_close $::focused_window}
    if { $patchwindow } {
    $mymenu add command -label [_ "Save"]        -accelerator "$accelerator+S" -command {::pd_menucommands::scheduleAction menu_send $::focused_window menusave}
    }
    $mymenu add command -label [_ "Save As..."]  -accelerator "Shift+$accelerator+S" -command {::pd_menucommands::scheduleAction menu_send $::focused_window menusaveas}
    #$mymenu add command -label [_ "Save All"]
    #$mymenu add command -label [_ "Revert to Saved"] -command {::pd_menucommands::scheduleAction menu_revert $::focused_window}
    $mymenu add  separator
    if {$::windowingsystem ne "aqua"} {
        $mymenu add cascade -label [_ "Preferences"] -menu .preferences
    }
    if { $patchwindow } {
    $mymenu add command -label [_ "Print..."]    -accelerator "$accelerator+P" -command {::pd_menucommands::scheduleAction menu_print $::focused_window}
    }
    $mymenu add  separator
    if {$::windowingsystem ne "aqua"} {
        $mymenu add command -label [_ "Quit"]    -accelerator "$accelerator+Q" \
            -command {::pd_connect::menu_quit}
    }

    # update recent files
    ::pd_menus::update_recentfiles_menu false
}

proc ::pd_menus::build_edit_menu {mymenu {patchwindow true}} {
    variable accelerator
    if { $patchwindow } {
    $mymenu add command -label [_ "Undo"]       -accelerator "$accelerator+Z" \
        -command {::pd_menucommands::scheduleAction menu_undo}
    $mymenu add command -label [_ "Redo"]       -accelerator "Shift+$accelerator+Z" \
        -command {::pd_menucommands::scheduleAction menu_redo}
    $mymenu add  separator
    $mymenu add command -label [_ "Cut"]        -accelerator "$accelerator+X" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window cut}
    $mymenu add command -label [_ "Copy"]       -accelerator "$accelerator+C" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window copy}
    $mymenu add command -label [_ "Paste"]      -accelerator "$accelerator+V" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window paste}
    $mymenu add command -label [_ "Paste Replace" ]  \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window paste-replace}
    $mymenu add  separator
    $mymenu add command -label [_ "Select All"] -accelerator "$accelerator+A" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window selectall}
    $mymenu add  separator
    $mymenu add command -label [_ "Duplicate"]  -accelerator "$accelerator+D" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window duplicate}
    $mymenu add command -label [_ "Tidy Up"]    -accelerator "$accelerator+Shift+R" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window tidy}
    $mymenu add command -label [_ "(Dis)Connect Selection"]    -accelerator "$accelerator+K" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window connect_selection}
    $mymenu add command -label [_ "Triggerize"] -accelerator "$accelerator+T" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window triggerize}
    $mymenu add  separator
    $mymenu add command -label [_ "Font"] \
        -command {::pd_menucommands::scheduleAction menu_font_dialog}
    $mymenu add command -label [_ "Zoom In"]    -accelerator "$accelerator++" \
        -command {::pd_menucommands::scheduleAction menu_send_float $::focused_window zoom 2}
    $mymenu add command -label [_ "Zoom Out"]   -accelerator "$accelerator+-" \
        -command {::pd_menucommands::scheduleAction menu_send_float $::focused_window zoom 1}
    $mymenu add  separator
    $mymenu add command -label [_ "Clear Console"] \
        -accelerator "Shift+$accelerator+L" -command {::pd_menucommands::scheduleAction menu_clear_console}
    $mymenu add  separator
    #TODO madness! how to set the state of the check box without invoking the menu!
    $mymenu add check -label [_ "Edit Mode"]    -accelerator "$accelerator+E" \
        -variable ::editmode_button \
        -command {::pd_menucommands::scheduleAction menu_editmode $::editmode_button}

    } else {
    $mymenu add command -label [_ "Cut"]        -accelerator "$accelerator+X" \
        -state disabled \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window cut}
    $mymenu add command -label [_ "Copy"]       -accelerator "$accelerator+C" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window copy}
    $mymenu add command -label [_ "Paste"]      -accelerator "$accelerator+V" \
        -state disabled \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window paste}
    $mymenu add  separator
    $mymenu add command -label [_ "Select All"] -accelerator "$accelerator+A" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window selectall}
    $mymenu add  separator
    $mymenu add command -label [_ "Font"] \
        -command {::pd_menucommands::scheduleAction menu_font_dialog}
    $mymenu add  separator
    $mymenu add command -label [_ "Clear Console"] \
        -accelerator "Shift+$accelerator+L" -command {::pd_menucommands::scheduleAction menu_clear_console}
    }
}

proc ::pd_menus::build_put_menu {mymenu} {
    variable accelerator
    # The trailing 0 in menu_send_float basically means leave the object box
    # sticking to the mouse cursor. The iemguis always do that when created
    # from the menu, as defined in canvas_iemguis()
    $mymenu add command -label [_ "Object"]   -accelerator "$accelerator+1" \
        -command {::pd_menucommands::scheduleAction menu_send_float $::focused_window obj 0}
    $mymenu add command -label [_ "Message"]  -accelerator "$accelerator+2" \
        -command {::pd_menucommands::scheduleAction menu_send_float $::focused_window msg 0}
    $mymenu add command -label [_ "Number"]   -accelerator "$accelerator+3" \
        -command {::pd_menucommands::scheduleAction menu_send_float $::focused_window floatatom 0}
    $mymenu add command -label [_ "List"]   -accelerator "$accelerator+4" \
        -command {::pd_menucommands::scheduleAction menu_send_float $::focused_window listbox 0}
    $mymenu add command -label [_ "Symbol"]  \
        -command {::pd_menucommands::scheduleAction menu_send_float $::focused_window symbolatom 0}
    $mymenu add command -label [_ "Comment"]  -accelerator "$accelerator+5" \
        -command {::pd_menucommands::scheduleAction menu_send_float $::focused_window text 0}
    $mymenu add  separator
    $mymenu add command -label [_ "Bang"]     -accelerator "Shift+$accelerator+B" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window bng}
    $mymenu add command -label [_ "Toggle"]   -accelerator "Shift+$accelerator+T" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window toggle}
    $mymenu add command -label [_ "Number2"]  -accelerator "Shift+$accelerator+N" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window numbox}
    $mymenu add command -label [_ "Vslider"]  -accelerator "Shift+$accelerator+V" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window vslider}
    $mymenu add command -label [_ "Hslider"]  -accelerator "Shift+$accelerator+J" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window hslider}
    $mymenu add command -label [_ "Vradio"]   -accelerator "Shift+$accelerator+D" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window vradio}
    $mymenu add command -label [_ "Hradio"]   -accelerator "Shift+$accelerator+I" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window hradio}
    $mymenu add command -label [_ "VU Meter"] -accelerator "Shift+$accelerator+U" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window vumeter}
    $mymenu add command -label [_ "Canvas"]   -accelerator "Shift+$accelerator+C" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window mycnv}
    $mymenu add  separator
    $mymenu add command -label [_ "Graph"]    -accelerator "Shift+$accelerator+G" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window graph}
    $mymenu add command -label [_ "Array"]    -accelerator "Shift+$accelerator+A" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window menuarray}
}

proc ::pd_menus::build_find_menu {mymenu {patchwindow true}} {
    variable accelerator
    $mymenu add command -label [_ "Find..."]    -accelerator "$accelerator+F" \
        -command {::pd_menucommands::scheduleAction menu_find_dialog}
    if { $patchwindow } {
    $mymenu add command -label [_ "Find Again"] -accelerator "$accelerator+G" \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window findagain}
    }
    $mymenu add command -label [_ "Find Last Error"] \
        -command {::pd_menucommands::scheduleAction pdsend {pd finderror}}
}

proc ::pd_menus::build_media_menu {mymenu} {
    variable accelerator
    $mymenu add radiobutton -label [_ "DSP On"]  -accelerator "$accelerator+/" \
        -variable ::dsp -value 1 -command {::pd_menucommands::scheduleAction pdsend "pd dsp 1"}
    $mymenu add radiobutton -label [_ "DSP Off"] -accelerator "$accelerator+." \
        -variable ::dsp -value 0 -command {::pd_menucommands::scheduleAction pdsend "pd dsp 0"}

    $mymenu add  separator
    $mymenu add command -label [_ "Test Audio and MIDI..."] \
        -command {::pd_menucommands::scheduleAction menu_doc_open doc/7.stuff/tools testtone.pd}
    $mymenu add command -label [_ "Load Meter"] \
        -command {::pd_menucommands::scheduleAction menu_doc_open doc/7.stuff/tools load-meter.pd}

    set audio_apilist_length [llength $::audio_apilist]
    if {$audio_apilist_length > 0} {$mymenu add separator}
    for {set x 0} {$x<$audio_apilist_length} {incr x} {
        $mymenu add radiobutton -label [lindex [lindex $::audio_apilist $x] 0] \
            -command {::pd_menucommands::scheduleAction menu_audio 0} -variable ::pd_whichapi \
            -value [lindex [lindex $::audio_apilist $x] 1]\
            -command {::pd_menucommands::scheduleAction pdsend "pd audio-setapi $::pd_whichapi"}
    }

    set midi_apilist_length [llength $::midi_apilist]
    if {$midi_apilist_length > 0} {$mymenu add separator}
    for {set x 0} {$x<$midi_apilist_length} {incr x} {
        $mymenu add radiobutton -label [lindex [lindex $::midi_apilist $x] 0] \
            -command {::pd_menucommands::scheduleAction menu_midi 0} -variable ::pd_whichmidiapi \
            -value [lindex [lindex $::midi_apilist $x] 1]\
            -command {::pd_menucommands::scheduleAction pdsend "pd midi-setapi $::pd_whichmidiapi"}
    }

    $mymenu add  separator
    $mymenu add command -label [_ "Audio Settings..."] \
        -command {::pd_menucommands::scheduleAction pdsend "pd audio-properties"}
    $mymenu add command -label [_ "MIDI Settings..."] \
        -command {::pd_menucommands::scheduleAction pdsend "pd midi-properties"}
}

proc ::pd_menus::build_window_menu {mymenu} {
    variable accelerator
    if {$::windowingsystem eq "aqua"} {
        # Tk 8.5+ automatically adds default Mac window menu items
        if {$::tcl_version < 8.5} {
            $mymenu add command -label [_ "Minimize"] -accelerator "$accelerator+M" \
                -command {::pd_menucommands::scheduleAction menu_minimize $::focused_window}
            $mymenu add command -label [_ "Zoom"] \
                -command {::pd_menucommands::scheduleAction menu_maximize $::focused_window}
            $mymenu add  separator
            $mymenu add command -label [_ "Bring All to Front"] \
                -command {::pd_menucommands::scheduleAction menu_bringalltofront}
        }
    } else {
        $mymenu add command -label [_ "Minimize"] -accelerator "$accelerator+M" \
                -command {::pd_menucommands::scheduleAction menu_minimize $::focused_window}
        $mymenu add command -label [_ "Next Window"] \
            -command {::pd_menucommands::scheduleAction menu_raisenextwindow} \
            -accelerator [_ "$accelerator+Page Down"]
        $mymenu add command -label [_ "Previous Window"] \
            -command {::pd_menucommands::scheduleAction menu_raisepreviouswindow} \
            -accelerator [_ "$accelerator+Page Up"]
    }
    $mymenu add command -label [_ "Close subwindows"] \
        -command {pdsend "pd close-subwindows"}
    $mymenu add  separator
    $mymenu add command -label [_ "Pd window"] -command {::pd_menucommands::scheduleAction menu_raise_pdwindow} \
        -accelerator "$accelerator+R"
    $mymenu add command -label [_ "Parent Window"] \
        -command {::pd_menucommands::scheduleAction menu_send $::focused_window findparent}
    $mymenu add  separator
}

proc ::pd_menus::build_tools_menu {mymenu} {
    variable accelerator

    $mymenu add command -label [_ "Message..."] \
        -accelerator "$accelerator+Shift+M" \
        -command {::pd_menucommands::scheduleAction menu_message_dialog}
}

proc ::pd_menus::build_help_menu {mymenu} {
    variable accelerator
    if {$::windowingsystem ne "aqua"} {
        # Tk creates this automatically on Mac
        $mymenu add command -label [_ "About Pd"] -command {::pd_menucommands::scheduleAction menu_aboutpd}
    }
    if {$::windowingsystem ne "aqua" || $::tcl_version < 8.5} {
        # TK 8.5+ on Mac creates this automatically, other platforms do not
        $mymenu add command -label [_ "HTML Manual..."] \
                -command {::pd_menucommands::scheduleAction menu_manual}
    }
    $mymenu add command -label [_ "Browser..."] -accelerator "$accelerator+B" \
        -command {::pd_menucommands::scheduleAction menu_helpbrowser}
    $mymenu add command -label [_ "List of objects..."] \
        -command {::pd_menucommands::scheduleAction menu_objectlist}
    $mymenu add  separator
    $mymenu add command -label [_ "puredata.info"] \
        -command {::pd_menucommands::scheduleAction menu_openfile {https://puredata.info}}
    $mymenu add command -label [_ "Check for updates"] -command {::pd_menucommands::scheduleAction menu_openfile \
        {https://pdlatest.puredata.info}}
    $mymenu add command -label [_ "Report a bug"] -command {::pd_menucommands::scheduleAction menu_openfile \
        {https://bugs.puredata.info}}
}

#------------------------------------------------------------------------------#
# undo/redo menu items

proc ::pd_menus::update_undo_on_menu {mytoplevel undo redo} {
    if {$undo eq "no"} { set undo "" }
    if {$redo eq "no"} { set redo "" }

    if {$undo ne ""} {
        $::patch_menubar.edit entryconfigure 0 -state normal \
            -label [_ "Undo $undo"]
    } else {
        $::patch_menubar.edit entryconfigure 0 -state disabled -label [_ "Undo"]
    }
    if {$redo ne ""} {
        $::patch_menubar.edit entryconfigure 1 -state normal \
            -label [_ "Redo $redo"]
    } else {
        $::patch_menubar.edit entryconfigure 1 -state disabled -label [_ "Redo"]
    }
}

# ------------------------------------------------------------------------------
# update the menu entries for opening recent files (write arg should always be true except the first time when pd is opened)
proc ::pd_menus::update_recentfiles_menu {{write true}} {
    ::pd_menus::update_openrecent_menu .openrecent $write
}

proc ::pd_menus::clear_recentfiles_menu {} {
    # empty recentfiles in preferences (write empty array)
    set ::recentfiles_list {}
    ::pd_menus::update_recentfiles_menu
}

proc ::pd_menus::update_openrecent_menu {mymenu {write}} {
    if {! [winfo exists $mymenu]} {
        menu $mymenu
    }
    $mymenu delete  0 end

    set i 1
    foreach filename $::recentfiles_list {
        set label [file tail $filename]
        if {$::windowingsystem ne "aqua"} {
            set label "$i. $label"
        }
        $mymenu add command \
            -label $label -underline 0 \
            -command [list ::pd_menucommands::scheduleAction open_file ${filename}]
        incr i
    }
    $mymenu add separator
    $mymenu add command -label [_ "Clear Menu"] \
        -command {::pd_menucommands::scheduleAction ::pd_menus::clear_recentfiles_menu}

    if {[llength $::recentfiles_list] == 0} {
        $mymenu entryconfigure end -state disabled
    }

    # write to config file
    if {$write == true} { ::pd_guiprefs::write_recentfiles }
}


# ------------------------------------------------------------------------------
# lots of crazy recursion to update the Window menu

# find the first parent patch that has a mapped window
proc ::pd_menus::find_mapped_parent {parentlist} {
    if {[llength $parentlist] == 0} {return "none"}
    set firstparent [lindex $parentlist 0]
    if {[winfo exists $firstparent]} {
        return $firstparent
    } elseif {[llength $parentlist] > 1} {
        return [find_mapped_parent [lrange $parentlist 1 end]]
    } else {
        # we must be the first menu item to be inserted
        return "none"
    }
}

# find the first parent patch that has a mapped window
proc ::pd_menus::insert_into_menu {mymenu entry parent} {
    set insertat [$mymenu index end]
    for {set i 0} {$i <= [$mymenu index end]} {incr i} {
        if {[$mymenu type $i] ne "command"} {continue}
        set currentcommand [$mymenu entrycget $i -command]
        if {$currentcommand eq "::pd_menucommands::scheduleAction raise $entry"} {return} ;# it exists already
        if {$currentcommand eq "raise $parent"} {
            set insertat $i
        }
    }
    incr insertat
    set label ""
    for {set i 0} {$i < [llength $::parentwindows($entry)]} {incr i} {
        append label " "
    }
    append label $::windowname($entry)
    $mymenu insert $insertat command -label $label -command "::pd_menucommands::scheduleAction raise $entry"
}

# recurse through a list of parent windows and add to the menu
proc ::pd_menus::add_list_to_menu {mymenu window parentlist} {
    if {[llength $parentlist] == 0} {
        insert_into_menu $mymenu $window {}
    } else {
        set entry [lindex $parentlist end]
        if {[winfo exists $entry]} {
            insert_into_menu $mymenu $entry \
                [find_mapped_parent $::parentwindows($entry)]
        }
    }
    if {[llength $parentlist] > 1} {
        add_list_to_menu $mymenu $window [lrange $parentlist 0 end-1]
    }
}

# update the list of windows on the Window menu. This expects run on the
# Window menu, and to insert below the last separator
proc ::pd_menus::update_window_menu {} {

    # TK 8.5+ Cocoa on Mac handles the window list for us
    if {$::windowingsystem eq "aqua" && $::tcl_version >= 8.5} {
        return 0
    }

    set mymenu $::patch_menubar.window
    # find the last separator and delete everything after that
    for {set i 0} {$i <= [$mymenu index end]} {incr i} {
        if {[$mymenu type $i] eq "separator"} {
            set deleteat $i
        }
    }
    $mymenu delete $deleteat end
    $mymenu add  separator
    foreach window [array names ::parentwindows] {
        set parentlist $::parentwindows($window)
        add_list_to_menu $mymenu $window $parentlist
        insert_into_menu $mymenu $window [find_mapped_parent $parentlist]
    }
}

# ------------------------------------------------------------------------------
# submenu for Preferences, now used on all platforms
proc ::pd_menus::savepreferences {} {
    # TODO localize "Untitled" & make sure translations do not contain spaces
    set filename [tk_getSaveFile \
                  -initialdir $::fileopendir \
                  -defaultextension .pdsettings \
                  -initialfile Untitled.pdsettings]
    if {$filename ne ""} {pdsend "pd save-preferences $filename"}
}

proc ::pd_menus::loadpreferences {} {
    set filename [tk_getOpenFile \
                  -initialdir $::fileopendir \
                  -defaultextension .pdsettings]
    if {$filename ne ""} {pdsend "pd load-preferences $filename"}
}

proc ::pd_menus::forgetpreferences {} {
    if {[pdtk_yesnodialog .pdwindow \
             [_ "Delete all preferences?\n(takes effect when Pd is restarted)"] \
             yes]} {
        pdsend "pd forget-preferences"
        if {[::pd_guiprefs::delete_config]} {
            ::pdwindow::post [_ "removed GUI settings" ]
        } {
            ::pdwindow::post [_ "no Pd-GUI settings to clear" ]
        }
        ::pdwindow::post "\n"

    }
}

proc ::pd_menus::build_preferences_menu {mymenu} {
    menu $mymenu
    $mymenu add command -label [_ "Edit Preferences..."] \
        -command {menu_preference_dialog}
    $mymenu add  separator
    $mymenu add command -label [_ "Save All Preferences"] \
        -command {::pd_menucommands::scheduleAction pdsend "pd save-preferences"}
    $mymenu add command -label [_ "Save to..."] \
        -command {::pd_menucommands::scheduleAction ::pd_menus::savepreferences}
    $mymenu add command -label [_ "Load from..."] \
        -command {::pd_menucommands::scheduleAction ::pd_menus::loadpreferences}
    $mymenu add command -label [_ "Forget All..."] \
        -command {::pd_menucommands::scheduleAction ::pd_menus::forgetpreferences}
    $mymenu add  separator
    $mymenu add check -label [_ "Tabbed preferences"] \
        -variable ::dialog_preferences::use_ttknotebook \
        -command {::pd_menucommands::scheduleAction ::dialog_preferences::write_usettknotebook}
}

# ------------------------------------------------------------------------------
# menu building functions for Mac OS X/aqua

# for Mac OS X only
proc ::pd_menus::create_apple_menu {mymenu} {
    # TODO this should open a Pd patch called about.pd
    menu $mymenu.apple
    $mymenu.apple add command -label [_ "About Pd"] -command {::pd_menucommands::scheduleAction menu_aboutpd}
    $mymenu.apple add separator
    $mymenu.apple add cascade -label [_ "Preferences"] \
        -menu .preferences
    # this needs to be last for things to function properly
    $mymenu add cascade -label "Apple" -menu $mymenu.apple
}

# FIXME: remove this when it is no longer necessary
# there is a Tk Cocoa bug where Help menu items after separators may be
# disabled after windows are cycled, as of Nov 2020 this is fixed via a fresh
# upstream patch in our Tk 8.6.10 build distributed with Pd but we keep this in
# case other versions of Tk are used such as those installed to the system
proc ::pd_menus::reenable_help_items_aqua {mymenu} {
    #if {$::tcl_version < 8.6} {
        $mymenu.help entryconfigure [_ "List of objects..."] -state normal
        $mymenu.help entryconfigure [_ "Report a bug"] -state normal
    #}
}

# ------------------------------------------------------------------------------
# menu building functions for Windows/Win32

# for Windows only
proc ::pd_menus::create_system_menu {mymenubar} {
    set mymenu $mymenubar.system
    $mymenubar add cascade -label System -menu $mymenu
    menu $mymenu -tearoff 0
    # placeholders
    $mymenu add command -label [_ "Edit Mode"] -command {::pd_menucommands::scheduleAction ::pdwindow::verbose 0 systemmenu}
    # TODO add Close, Minimize, etc and whatever else is on the little menu
    # that is on the top left corner of the window frame
    # http://wiki.tcl.tk/1006
    # TODO add Edit Mode here
}
