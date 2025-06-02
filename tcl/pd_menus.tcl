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

    array set keynames { \
                             "Key" "" \
                             "Mod1" "Cmd" \
                             "Mod2" "Option" \
                             "Command" "Cmd" \
                             "Control" "Ctrl" \
                             "ISO_Level3_Shift" "AltGr" \
                             "Next" "Page Down" \
                             "Prior" "Page Up" \
                             "exclam" "!" \
                             "quotedbl" "\"" \
                             "numbersign" "#" \
                             "dollar" "$" \
                             "percent" "%" \
                             "ampersand" "&" \
                             "apostrophe" "'" \
                             "parenleft" "(" \
                             "parenright" ")" \
                             "asterisk" "*" \
                             "plus" "+" \
                             "comma" "," \
                             "minus" "-" \
                             "period" "." \
                             "slash" "/" \
                             "colon" ":" \
                             "semicolon" ";" \
                             "less" "<" \
                             "equal" "=" \
                             "greater" ">" \
                             "question" "?" \
                             "at" "@" \
                             "bracketleft" "[" \
                             "backslash" "\\" \
                             "bracketright" "]" \
                             "asciicircum" "^" \
                             "underscore" "_" \
                             "grave" "`" \
                             "braceleft" "{" \
                             "bar" "|" \
                             "braceright" "}" \
                             "asciitilde" "~" \
                             "section" "§" \
                             "acute" "´" \
                             "degree" "°" \
    }
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

    # create separate help menu for pdwindow on macOS (to get "Pd Help" entry)
    if {$::windowingsystem eq "aqua" && $::tcl_version >= 8.5} {
        build_help_menu [menu $::pdwindow_menubar.help]
        $::pdwindow_menubar entryconfigure [_ Help] -menu $::pdwindow_menubar.help
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

# ------------------------------------------------------------------------------
# menu building functions
proc ::pd_menus::get_accelerator_for_event {event} {
    foreach shortcut [event info $event] {
        set accel {}
        foreach k [split [string trim $shortcut <>] -] {
            foreach {longname k} [array get ::pd_menus::keynames $k] {break}
            if { $k != "" } {
                lappend accel $k
            }
        }

        if { $accel != {} } {
            return [join $accel +]
        }
    }
    return ""
}
proc ::pd_menus::add_menu {menu type label event args} {
    $menu add $type \
        -label $label \
        -command "event generate \[focus\] $event"
    set accel [::pd_menus::get_accelerator_for_event $event]
    if { $accel != {} } {
        $menu entryconfigure end -accelerator $accel
    }
    foreach {k v} $args {
        $menu entryconfigure end $k $v
    }
}

proc ::pd_menus::menubar_for_dialog {mytoplevel} {
    set menubar $::dialog_menubar
    if {$::windowingsystem eq "aqua"} {
        set menubar $::pdwindow_menubar
    }
    $mytoplevel configure -menu $menubar
}



proc ::pd_menus::build_file_menu {mymenu {patchwindow true}} {
    # run the platform-specific build_file_menu_* procs first, and config them
    add_menu $mymenu command [_ "New" ] "<<File|New>>"
    add_menu $mymenu command [_ "Open" ] "<<File|Open>>"
    $mymenu add cascade -label [_ "Open Recent"] -menu .openrecent
    $mymenu add separator
    add_menu $mymenu command [_ "Close" ] "<<File|Close>>"
    if { $patchwindow } {
        add_menu $mymenu command [_ "Save" ] "<<File|Save>>"
    }
    add_menu $mymenu command [_ "Save As..." ] "<<File|SaveAs>>"
    #add_menu $mymenu command [_ "Save All" ] "<<File|SaveAll>>"
    #add_menu $mymenu command [_ "Revert to Saved" ] "<<File|Revert>>"
    #$mymenu entryconfigure [_ "Revert*"]    -command {event generate [focus] <<File|Revert>>}

    $mymenu add  separator
    if {$::windowingsystem ne "aqua"} {
        $mymenu add cascade -label [_ "Preferences"] -menu .preferences
    }
    if { $patchwindow } {
        add_menu $mymenu command [_ "Print..." ] "<<File|Print>>"
    }
    if {$::windowingsystem ne "aqua"} {
        $mymenu add  separator
        add_menu $mymenu command [_ "Quit" ] "<<File|Quit>>"
    }

    # update recent files
    ::pd_menus::update_recentfiles_menu false
}

proc ::pd_menus::build_edit_menu {mymenu {patchwindow true}} {
    variable accelerator
    if { $patchwindow } {
    add_menu $mymenu command [_ "Undo" ] "<<Edit|Undo>>"
    add_menu $mymenu command [_ "Redo" ] "<<Edit|Redo>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Cut" ] "<<Edit|Cut>>"
    add_menu $mymenu command [_ "Copy" ] "<<Edit|Copy>>"
    add_menu $mymenu command [_ "Paste" ] "<<Edit|Paste>>"
    add_menu $mymenu command [_ "Paste Replace" ] "<<Edit|PasteReplace>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Select All" ] "<<Edit|SelectAll>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Duplicate" ] "<<Edit|Duplicate>>"
    add_menu $mymenu command [_ "Tidy Up" ] "<<Edit|TidyUp>>"
    add_menu $mymenu command [_ "(Dis)Connect Selection" ] "<<Edit|ConnectSelection>>"
    add_menu $mymenu command [_ "Triggerize" ] "<<Edit|Triggerize>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Font" ] "<<Edit|Font>>"
    add_menu $mymenu command [_ "Zoom In" ] "<<Edit|ZoomIn>>"
    add_menu $mymenu command [_ "Zoom Out" ] "<<Edit|ZoomOut>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Clear Console" ] "<<Pd|ClearConsole>>"
    $mymenu add  separator
    add_menu $mymenu check [_ "Edit Mode"] <<Edit|EditMode>> -variable ::editmode_button
    } else {
        add_menu $mymenu command [_ "Cut" ] "<<Edit|Cut>>" -state disabled
        add_menu $mymenu command [_ "Copy" ] "<<Edit|Copy>>"
        add_menu $mymenu command [_ "Paste" ] "<<Edit|Paste>>" -state disabled
        $mymenu add  separator
        add_menu $mymenu command [_ "Select All" ] "<<Edit|SelectAll>>"
        $mymenu add  separator
        add_menu $mymenu command [_ "Font" ] "<<Edit|Font>>"
        $mymenu add  separator
        add_menu $mymenu command [_ "Clear Console" ] "<<Pd|ClearConsole>>"
    }
}

proc ::pd_menus::build_put_menu {mymenu} {
    variable accelerator
    # The trailing 0 in menu_send_float basically means leave the object box
    # sticking to the mouse cursor. The iemguis always do that when created
    # from the menu, as defined in canvas_iemguis()
    add_menu $mymenu command [_ "Object" ] "<<Put|Object>>"
    add_menu $mymenu command [_ "Message" ] "<<Put|Message>>"
    add_menu $mymenu command [_ "Number" ] "<<Put|Number>>"
    add_menu $mymenu command [_ "List" ] "<<Put|List>>"
    add_menu $mymenu command [_ "Symbol" ] "<<Put|Symbol>>"
    add_menu $mymenu command [_ "Comment" ] "<<Put|Comment>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Bang" ] "<<Put|Bang>>"
    add_menu $mymenu command [_ "Toggle" ] "<<Put|Toggle>>"
    add_menu $mymenu command [_ "Number2" ] "<<Put|Number2>>"
    add_menu $mymenu command [_ "Vslider" ] "<<Put|VerticalSlider>>"
    add_menu $mymenu command [_ "Hslider" ] "<<Put|HorizontalSlider>>"
    add_menu $mymenu command [_ "Vradio" ] "<<Put|VerticalRadio>>"
    add_menu $mymenu command [_ "Hradio" ] "<<Put|HorizontalRadio>>"
    add_menu $mymenu command [_ "VU Meter" ] "<<Put|VUMeter>>"
    add_menu $mymenu command [_ "Canvas" ] "<<Put|Canvas>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Graph" ] "<<Put|Graph>>"
    add_menu $mymenu command [_ "Array" ] "<<Put|Array>>"
}

proc ::pd_menus::build_find_menu {mymenu {patchwindow true}} {
    variable accelerator
    add_menu $mymenu command [_ "Find..."] "<<Find|Find>>"
    add_menu $mymenu command [_ "Find Again"] "<<Find|FindAgain>>"
    add_menu $mymenu command [_ "Find Last Error"] "<<Find|FindLastError>>"
}

proc ::pd_menus::build_media_menu {mymenu} {
    variable accelerator
    add_menu $mymenu radiobutton [_ "DSP On" ] "<<Media|DSPOn>>" -variable ::dsp -value 1
    add_menu $mymenu radiobutton [_ "DSP Off" ] "<<Media|DSPOff>>" -variable ::dsp -value 0

    $mymenu add  separator
    add_menu $mymenu command [_ "Test Audio and MIDI..."] "<<Media|TestAudioMIDI>>"
    add_menu $mymenu command [_ "Load Meter"] "<<Media|LoadMeter>>"

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
    add_menu $mymenu command [_ "Audio Settings..."] "<<Preferences|Audio>>"
    add_menu $mymenu command [_ "MIDI Settings..."] "<<Preferences|MIDI>>"
}

proc ::pd_menus::build_window_menu {mymenu} {
    variable accelerator
    if {$::windowingsystem eq "aqua"} {
        # Tk 8.5+ automatically adds default Mac window menu items
        if {$::tcl_version < 8.5} {
            add_menu $mymenu command [_ "Minimize" ] "<<Window|Minimize>>"
            add_menu $mymenu command [_ "Zoom" ] "<<Window|Maximize>>"
            $mymenu add  separator
            add_menu $mymenu command [_ "Bring All to Front" ] "<<Window|AllToFront>>"
        }
    } else {
        add_menu $mymenu command [_ "Minimize" ] "<<Window|Minimize>>"
        add_menu $mymenu command [_ "Next Window" ] "<<Window|Next>>"
        add_menu $mymenu command [_ "Previous Window" ] "<<Window|Previous>>"
    }
    $mymenu add command -label [_ "Close subwindows"] \
        -command {pdsend "pd close-subwindows"}
    $mymenu add  separator
    add_menu $mymenu command [_ "Close subwindows" ] "<<Window|CloseSubwindows>>"
    add_menu $mymenu command [_ "Pd window" ] "<<Window|PdWindow>>"
    add_menu $mymenu command [_ "Parent Window" ] "<<Window|Parent>>"
    $mymenu add  separator
}

proc ::pd_menus::build_tools_menu {mymenu} {
    variable accelerator
    add_menu $mymenu command [_ "Message..."] <<Pd|Message>>
}

proc ::pd_menus::build_help_menu {mymenu} {
    variable accelerator
    if {$::windowingsystem ne "aqua"} {
        # Tk creates this automatically on Mac
        add_menu $mymenu command [_ "About Pd" ] "<<Help|About>>"
    }
    if {$::windowingsystem ne "aqua" || $::tcl_version < 8.5} {
        # TK 8.5+ on Mac creates this automatically, other platforms do not
        add_menu $mymenu command [_ "HTML Manual..." ] "<<Help|Manual>>"
    }
    add_menu $mymenu command [_ "Browser..." ] "<<Help|Browser>>"
    add_menu $mymenu command [_ "List of objects..." ] "<<Help|ListObjects>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "puredata.info" ] "<<Help|puredata.info>>"
    add_menu $mymenu command [_ "Check for updates" ] "<<Help|CheckUpdates>>"
    add_menu $mymenu command [_ "Report a bug" ] "<<Help|ReportBug>>"
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
    $mymenu add command -label [_ "Clear Recent Files"] \
        -command {event generate [focus] <<File|ClearRecentFiles>>}
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
    if { "${insertat}" eq "none" } { set insertat 0 }
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
    $mymenu insert $insertat command -label $label \
        -command "::pd_menucommands::scheduleAction raise $entry"
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
    set lastmenu [$mymenu index end]
    if { "${lastmenu}" eq "none" } { set lastmenu 0 }
    for {set i 0} {$i <= $lastmenu} {incr i} {
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
    add_menu $mymenu command [_ "Edit Preferences..." ] "<<Preferences|Edit>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Save All Preferences" ] "<<Preferences|Save>>"
    add_menu $mymenu command [_ "Save to..." ] "<<Preferences|SaveTo>>"
    add_menu $mymenu command [_ "Load from..." ] "<<Preferences|Load>>"
    add_menu $mymenu command [_ "Forget All..." ] "<<Preferences|Forget>>"
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
    $mymenu.apple add command -label [_ "About Pd"] \
        -command {event generate [focus] <<Help|About>>}
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
