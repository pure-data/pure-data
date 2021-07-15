# Copyright (c) 1997-2009 Miller Puckette.
#(c) 2008 WordTech Communications LLC. License: standard Tcl license, http://www.tcl.tk/software/tcltk/license.html

package provide pd_menus 0.1

package require pd_menucommands

# TODO figure out Undo/Redo/Cut/Copy/Paste state changes for menus

# since there is one menubar that is used for all windows, the menu -commands
# use {} quotes so that $::focused_window is interpreted when the menu item
# is called, not when the command is mapped to the menu item.  This is the
# opposite of the 'bind' commands in pd_bindings.tcl

namespace eval ::pd_menus:: {
    variable accelerator
    variable menubar ".menubar"

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
    variable menubar
    if {$::windowingsystem eq "aqua"} {
        set accelerator "Cmd"
    } else {
        set accelerator "Ctrl"
    }
    menu $menubar
    if {$::windowingsystem eq "aqua"} {create_apple_menu $menubar}
    set menulist "file edit put find media window help"
    foreach mymenu $menulist {
        if {$mymenu eq "find"} {
            set underlined 3
        } {
            set underlined 0
        }
        menu $menubar.$mymenu
        $menubar add cascade -label [_ [string totitle $mymenu]] \
            -underline $underlined -menu $menubar.$mymenu
        [format build_%s_menu $mymenu] $menubar.$mymenu
    }
    if {$::windowingsystem eq "win32"} {create_system_menu $menubar}
    . configure -menu $menubar
}

proc ::pd_menus::configure_for_pdwindow {} {
    variable menubar
    # these are meaningless for the Pd window, so disable them
    # File menu
    $menubar.file entryconfigure [_ "Close"] -state disabled
    $menubar.file entryconfigure [_ "Save"] -state disabled
    $menubar.file entryconfigure [_ "Save As..."] -state normal
    $menubar.file entryconfigure [_ "Print..."] -state disabled

    # Edit menu
    $menubar.edit entryconfigure [_ "Paste Replace"] -state disabled
    $menubar.edit entryconfigure [_ "Duplicate"] -state disabled
    $menubar.edit entryconfigure [_ "Font"] -state normal
    $menubar.edit entryconfigure [_ "Zoom In"] -state disabled
    $menubar.edit entryconfigure [_ "Zoom Out"] -state disabled
    $menubar.edit entryconfigure [_ "Tidy Up"] -state disabled
    $menubar.edit entryconfigure [_ "(Dis)Connect Selection"] -state disabled
    $menubar.edit entryconfigure [_ "Triggerize"] -state disabled
    $menubar.edit entryconfigure [_ "Edit Mode"] -state disabled
    pdtk_canvas_editmode .pdwindow 0
    # Undo/Redo change names, they need to have the asterisk (*) after
    $menubar.edit entryconfigure 0 -state disabled -label [_ "Undo"]
    $menubar.edit entryconfigure 1 -state disabled -label [_ "Redo"]
    # disable everything on the Put menu
    for {set i 0} {$i <= [$menubar.put index end]} {incr i} {
        # catch errors that happen when trying to disable separators
        catch {$menubar.put entryconfigure $i -state disabled }
    }
    # Help menu
    if {$::windowingsystem eq "aqua"} {
        ::pd_menus::reenable_help_items_aqua $menubar
    }
}

proc ::pd_menus::configure_for_canvas {mytoplevel} {
    variable menubar
    # File menu
    $menubar.file entryconfigure [_ "Close"] -state normal
    $menubar.file entryconfigure [_ "Save"] -state normal
    $menubar.file entryconfigure [_ "Save As..."] -state normal
    $menubar.file entryconfigure [_ "Print..."] -state normal
    # Edit menu
    $menubar.edit entryconfigure [_ "Paste Replace"] -state normal
    $menubar.edit entryconfigure [_ "Duplicate"] -state normal
    $menubar.edit entryconfigure [_ "Font"] -state normal
    $menubar.edit entryconfigure [_ "Zoom In"] -state normal
    $menubar.edit entryconfigure [_ "Zoom Out"] -state normal
    $menubar.edit entryconfigure [_ "Tidy Up"] -state normal
    $menubar.edit entryconfigure [_ "(Dis)Connect Selection"] -state normal
    $menubar.edit entryconfigure [_ "Triggerize"] -state normal
    $menubar.edit entryconfigure [_ "Edit Mode"] -state normal
    pdtk_canvas_editmode $mytoplevel $::editmode($mytoplevel)
    # Put menu
    for {set i 0} {$i <= [$menubar.put index end]} {incr i} {
        # catch errors that happen when trying to disable separators
        if {[$menubar.put type $i] ne "separator"} {
            $menubar.put entryconfigure $i -state normal
        }
    }
    update_undo_on_menu $mytoplevel $::undo_actions($mytoplevel) $::redo_actions($mytoplevel)
    # Help menu
    if {$::windowingsystem eq "aqua"} {
        ::pd_menus::reenable_help_items_aqua $menubar
    }
}

proc ::pd_menus::configure_for_dialog {mytoplevel} {
    variable menubar
    # these are meaningless for the dialog panels, so disable them except for
    # the ones that make sense in the Find dialog panel and it's canvas
    # File menu
    $menubar.file entryconfigure [_ "Close"] -state disabled
    if {$mytoplevel eq ".find"} {
        # these bindings are passed through Find to it's target search window
        $menubar.file entryconfigure [_ "Save As..."] -state disabled
        if {$mytoplevel ne ".pdwindow"} {
            # these don't do anything in the pdwindow
            $menubar.file entryconfigure [_ "Save"] -state disabled
            $menubar.file entryconfigure [_ "Print..."] -state disabled
        }
    } else {
        $menubar.file entryconfigure [_ "Save"] -state disabled
        $menubar.file entryconfigure [_ "Save As..."] -state disabled
        $menubar.file entryconfigure [_ "Print..."] -state disabled
    }

    # Edit menu
    $menubar.edit entryconfigure [_ "Font"] -state disabled
    $menubar.edit entryconfigure [_ "Paste Replace"] -state disabled
    $menubar.edit entryconfigure [_ "Duplicate"] -state disabled
    $menubar.edit entryconfigure [_ "Zoom In"] -state disabled
    $menubar.edit entryconfigure [_ "Zoom Out"] -state disabled
    $menubar.edit entryconfigure [_ "Tidy Up"] -state disabled
    $menubar.edit entryconfigure [_ "(Dis)Connect Selection"] -state disabled
    $menubar.edit entryconfigure [_ "Triggerize"] -state disabled
    $menubar.edit entryconfigure [_ "Edit Mode"] -state disabled
    pdtk_canvas_editmode $mytoplevel 0
    # Undo/Redo change names, they need to have the asterisk (*) after
    $menubar.edit entryconfigure 0 -state disabled -label [_ "Undo"]
    $menubar.edit entryconfigure 1 -state disabled -label [_ "Redo"]
    # disable everything on the Put menu
    for {set i 0} {$i <= [$menubar.put index end]} {incr i} {
        # catch errors that happen when trying to disable separators
        catch {$menubar.put entryconfigure $i -state disabled }
    }
    # Help menu
    if {$::windowingsystem eq "aqua"} {
        ::pd_menus::reenable_help_items_aqua $menubar
    }
}


# ------------------------------------------------------------------------------
# menu building functions
proc ::pd_menus::build_file_menu {mymenu} {
    # run the platform-specific build_file_menu_* procs first, and config them
    [format build_file_menu_%s $::windowingsystem] $mymenu
    $mymenu entryconfigure [_ "New"]        -command {menu_new}
    $mymenu entryconfigure [_ "Open"]       -command {menu_open}
    $mymenu entryconfigure [_ "Save"]       -command {menu_send $::focused_window menusave}
    $mymenu entryconfigure [_ "Save As..."] -command {menu_send $::focused_window menusaveas}
    #$mymenu entryconfigure [_ "Revert*"]    -command {menu_revert $::focused_window}
    $mymenu entryconfigure [_ "Close"]      -command {::pd_bindings::window_close $::focused_window}
    $mymenu entryconfigure [_ "Message..."] -command {menu_message_dialog}
    $mymenu entryconfigure [_ "Print..."]   -command {menu_print $::focused_window}
    # update recent files
    if {[llength $::recentfiles_list] > 0} {
        ::pd_menus::update_recentfiles_menu false
    }
}

proc ::pd_menus::build_edit_menu {mymenu} {
    variable accelerator
    $mymenu add command -label [_ "Undo"]       -accelerator "$accelerator+Z" \
        -command {menu_undo}
    $mymenu add command -label [_ "Redo"]       -accelerator "Shift+$accelerator+Z" \
        -command {menu_redo}
    $mymenu add  separator
    $mymenu add command -label [_ "Cut"]        -accelerator "$accelerator+X" \
        -command {menu_send $::focused_window cut}
    $mymenu add command -label [_ "Copy"]       -accelerator "$accelerator+C" \
        -command {menu_send $::focused_window copy}
    $mymenu add command -label [_ "Paste"]      -accelerator "$accelerator+V" \
        -command {menu_send $::focused_window paste}
    $mymenu add command -label [_ "Duplicate"]  -accelerator "$accelerator+D" \
        -command {menu_send $::focused_window duplicate}
    $mymenu add command -label [_ "Paste Replace" ]  \
        -command {menu_send $::focused_window paste-replace}
    $mymenu add command -label [_ "Select All"] -accelerator "$accelerator+A" \
        -command {menu_send $::focused_window selectall}
    $mymenu add  separator
    $mymenu add command -label [_ "Font"] \
        -command {menu_font_dialog}
    $mymenu add command -label [_ "Zoom In"]    -accelerator "$accelerator++" \
        -command {menu_send_float $::focused_window zoom 2}
    $mymenu add command -label [_ "Zoom Out"]   -accelerator "$accelerator+-" \
        -command {menu_send_float $::focused_window zoom 1}
    $mymenu add command -label [_ "Tidy Up"]    -accelerator "$accelerator+Shift+R" \
        -command {menu_send $::focused_window tidy}
    $mymenu add command -label [_ "(Dis)Connect Selection"]    -accelerator "$accelerator+K" \
        -command {menu_send $::focused_window connect_selection}
    $mymenu add command -label [_ "Triggerize"] -accelerator "$accelerator+T" \
        -command {menu_send $::focused_window triggerize}
    $mymenu add command -label [_ "Clear Console"] \
        -accelerator "Shift+$accelerator+L" -command {menu_clear_console}
    $mymenu add  separator
    #TODO madness! how to set the state of the check box without invoking the menu!
    $mymenu add check -label [_ "Edit Mode"]    -accelerator "$accelerator+E" \
        -variable ::editmode_button \
        -command {menu_editmode $::editmode_button}
}

proc ::pd_menus::build_put_menu {mymenu} {
    variable accelerator
    # The trailing 0 in menu_send_float basically means leave the object box
    # sticking to the mouse cursor. The iemguis always do that when created
    # from the menu, as defined in canvas_iemguis()
    $mymenu add command -label [_ "Object"]   -accelerator "$accelerator+1" \
        -command {menu_send_float $::focused_window obj 0}
    $mymenu add command -label [_ "Message"]  -accelerator "$accelerator+2" \
        -command {menu_send_float $::focused_window msg 0}
    $mymenu add command -label [_ "Number"]   -accelerator "$accelerator+3" \
        -command {menu_send_float $::focused_window floatatom 0}
    $mymenu add command -label [_ "Symbol"]   -accelerator "$accelerator+4" \
        -command {menu_send_float $::focused_window symbolatom 0}
    $mymenu add command -label [_ "Comment"]  -accelerator "$accelerator+5" \
        -command {menu_send_float $::focused_window text 0}
    $mymenu add  separator
    $mymenu add command -label [_ "Bang"]     -accelerator "Shift+$accelerator+B" \
        -command {menu_send $::focused_window bng}
    $mymenu add command -label [_ "Toggle"]   -accelerator "Shift+$accelerator+T" \
        -command {menu_send $::focused_window toggle}
    $mymenu add command -label [_ "Number2"]  -accelerator "Shift+$accelerator+N" \
        -command {menu_send $::focused_window numbox}
    $mymenu add command -label [_ "Vslider"]  -accelerator "Shift+$accelerator+V" \
        -command {menu_send $::focused_window vslider}
    $mymenu add command -label [_ "Hslider"]  -accelerator "Shift+$accelerator+J" \
        -command {menu_send $::focused_window hslider}
    $mymenu add command -label [_ "Vradio"]   -accelerator "Shift+$accelerator+D" \
        -command {menu_send $::focused_window vradio}
    $mymenu add command -label [_ "Hradio"]   -accelerator "Shift+$accelerator+I" \
        -command {menu_send $::focused_window hradio}
    $mymenu add command -label [_ "VU Meter"] -accelerator "Shift+$accelerator+U" \
        -command {menu_send $::focused_window vumeter}
    $mymenu add command -label [_ "Canvas"]   -accelerator "Shift+$accelerator+C" \
        -command {menu_send $::focused_window mycnv}
    $mymenu add  separator
    $mymenu add command -label [_ "Graph"]    -accelerator "Shift+$accelerator+G" \
        -command {menu_send $::focused_window graph}
    $mymenu add command -label [_ "Array"]    -accelerator "Shift+$accelerator+A" \
        -command {menu_send $::focused_window menuarray}
}

proc ::pd_menus::build_find_menu {mymenu} {
    variable accelerator
    $mymenu add command -label [_ "Find..."]    -accelerator "$accelerator+F" \
        -command {menu_find_dialog}
    $mymenu add command -label [_ "Find Again"] -accelerator "$accelerator+G" \
        -command {menu_send $::focused_window findagain}
    $mymenu add command -label [_ "Find Last Error"] \
        -command {pdsend {pd finderror}}
}

proc ::pd_menus::build_media_menu {mymenu} {
    variable accelerator
    $mymenu add radiobutton -label [_ "DSP On"]  -accelerator "$accelerator+/" \
        -variable ::dsp -value 1 -command {pdsend "pd dsp 1"}
    $mymenu add radiobutton -label [_ "DSP Off"] -accelerator "$accelerator+." \
        -variable ::dsp -value 0 -command {pdsend "pd dsp 0"}

    $mymenu add  separator
    $mymenu add command -label [_ "Test Audio and MIDI..."] \
        -command {menu_doc_open doc/7.stuff/tools testtone.pd}
    $mymenu add command -label [_ "Load Meter"] \
        -command {menu_doc_open doc/7.stuff/tools load-meter.pd}

    set audio_apilist_length [llength $::audio_apilist]
    if {$audio_apilist_length > 0} {$mymenu add separator}
    for {set x 0} {$x<$audio_apilist_length} {incr x} {
        $mymenu add radiobutton -label [lindex [lindex $::audio_apilist $x] 0] \
            -command {menu_audio 0} -variable ::pd_whichapi \
            -value [lindex [lindex $::audio_apilist $x] 1]\
            -command {pdsend "pd audio-setapi $::pd_whichapi"}
    }

    set midi_apilist_length [llength $::midi_apilist]
    if {$midi_apilist_length > 0} {$mymenu add separator}
    for {set x 0} {$x<$midi_apilist_length} {incr x} {
        $mymenu add radiobutton -label [lindex [lindex $::midi_apilist $x] 0] \
            -command {menu_midi 0} -variable ::pd_whichmidiapi \
            -value [lindex [lindex $::midi_apilist $x] 1]\
            -command {pdsend "pd midi-setapi $::pd_whichmidiapi"}
    }

    $mymenu add  separator
    $mymenu add command -label [_ "Audio Settings..."] \
        -command {pdsend "pd audio-properties"}
    $mymenu add command -label [_ "MIDI Settings..."] \
        -command {pdsend "pd midi-properties"}
}

proc ::pd_menus::build_window_menu {mymenu} {
    variable accelerator
    if {$::windowingsystem eq "aqua"} {
        # Tk 8.5+ automatically adds default Mac window menu items
        if {$::tcl_version < 8.5} {
            $mymenu add command -label [_ "Minimize"] -accelerator "$accelerator+M" \
                -command {menu_minimize $::focused_window}
            $mymenu add command -label [_ "Zoom"] \
                -command {menu_maximize $::focused_window}
            $mymenu add  separator
            $mymenu add command -label [_ "Bring All to Front"] \
                -command {menu_bringalltofront}
        }
    } else {
        $mymenu add command -label [_ "Minimize"] -accelerator "$accelerator+M" \
                -command {menu_minimize $::focused_window}
        $mymenu add command -label [_ "Next Window"] \
            -command {menu_raisenextwindow} \
            -accelerator [_ "$accelerator+Page Down"]
        $mymenu add command -label [_ "Previous Window"] \
            -command {menu_raisepreviouswindow} \
            -accelerator [_ "$accelerator+Page Up"]
    }
    $mymenu add  separator
    $mymenu add command -label [_ "Pd window"] -command {menu_raise_pdwindow} \
        -accelerator "$accelerator+R"
    $mymenu add command -label [_ "Parent Window"] \
        -command {menu_send $::focused_window findparent}
    $mymenu add  separator
}

proc ::pd_menus::build_help_menu {mymenu} {
    variable accelerator
    if {$::windowingsystem ne "aqua"} {
        # Tk creates this automatically on Mac
        $mymenu add command -label [_ "About Pd"] -command {menu_aboutpd}
    }
    if {$::windowingsystem ne "aqua" || $::tcl_version < 8.5} {
        # TK 8.5+ on Mac creates this automatically, other platforms do not
        $mymenu add command -label [_ "HTML Manual..."] \
                -command {menu_manual}
    }
    $mymenu add command -label [_ "Browser..."] -accelerator "$accelerator+B" \
        -command {menu_helpbrowser}
    $mymenu add command -label [_ "List of objects..."] \
        -command {menu_objectlist}
    $mymenu add  separator
    $mymenu add command -label [_ "puredata.info"] \
        -command {menu_openfile {http://puredata.info}}
    $mymenu add command -label [_ "Report a bug"] -command {menu_openfile \
        {http://bugs.puredata.info}}
}

#------------------------------------------------------------------------------#
# undo/redo menu items

proc ::pd_menus::update_undo_on_menu {mytoplevel undo redo} {
    variable menubar
    if {$undo eq "no"} { set undo "" }
    if {$redo eq "no"} { set redo "" }

    if {$undo ne ""} {
        $menubar.edit entryconfigure 0 -state normal \
            -label [_ "Undo $undo"]
    } else {
        $menubar.edit entryconfigure 0 -state disabled -label [_ "Undo"]
    }
    if {$redo ne ""} {
        $menubar.edit entryconfigure 1 -state normal \
            -label [_ "Redo $redo"]
    } else {
        $menubar.edit entryconfigure 1 -state disabled -label [_ "Redo"]
    }
}

# ------------------------------------------------------------------------------
# update the menu entries for opening recent files (write arg should always be true except the first time when pd is opened)
proc ::pd_menus::update_recentfiles_menu {{write true}} {
    variable menubar
    switch -- $::windowingsystem {
        "aqua"  {::pd_menus::update_openrecent_menu_aqua .openrecent $write}
        "win32" {::pd_menus::update_recentfiles_on_menu $menubar.file $write}
        "x11"   {::pd_menus::update_recentfiles_on_menu $menubar.file $write}
    }
}

proc ::pd_menus::clear_recentfiles_menu {} {
    # empty recentfiles in preferences (write empty array)
    set ::recentfiles_list {}
    ::pd_menus::update_recentfiles_menu
}

proc ::pd_menus::update_openrecent_menu_aqua {mymenu {write}} {
    if {! [winfo exists $mymenu]} {menu $mymenu}
    $mymenu delete 0 end

    # now the list is last first so we just add
    foreach filename $::recentfiles_list {
        $mymenu add command -label [file tail $filename] \
            -command "open_file {$filename}"
    }
    # clear button
    $mymenu add  separator
    $mymenu add command -label [_ "Clear Menu"] \
        -command "::pd_menus::clear_recentfiles_menu"
    # write to config file
    if {$write == true} { ::pd_guiprefs::write_recentfiles }
}

# ------------------------------------------------------------------------------
# this expects to be run on the File menu, and to insert above the last separator
proc ::pd_menus::update_recentfiles_on_menu {mymenu {write}} {
    set lastitem [$mymenu index end]
    set i 1
    while {[$mymenu type [expr $lastitem-$i]] ne "separator"} {incr i}
    set bottom_separator [expr $lastitem-$i]
    incr i

    while {[$mymenu type [expr $lastitem-$i]] ne "separator"} {incr i}
    set top_separator [expr $lastitem-$i]
    if {$top_separator < [expr $bottom_separator-1]} {
        $mymenu delete [expr $top_separator+1] [expr $bottom_separator-1]
    }
    # insert the list from the end because we insert each element on the top
    set i [llength $::recentfiles_list]
    while {[incr i -1] > -1} {
        set filename [lindex $::recentfiles_list $i]
        set j [expr $i + 1]
        if {$::windowingsystem eq "aqua"} {
            set label [file tail $filename]
        } else {
            set label [concat "$j. " [file tail $filename]]
        }
        $mymenu insert [expr $top_separator+1] command \
            -label $label -command "open_file {$filename}" -underline 0
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
        if {$currentcommand eq "raise $entry"} {return} ;# it exists already
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
    $mymenu insert $insertat command -label $label -command "raise $entry"
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

proc ::pd_menus::create_preferences_menu {mymenu} {
    menu $mymenu
    $mymenu add command -label [_ "Path..."] \
        -command {pdsend "pd start-path-dialog"}
    $mymenu add command -label [_ "Startup..."] \
        -command {pdsend "pd start-startup-dialog"}
    $mymenu add command -label [_ "Audio..."] \
        -command {pdsend "pd audio-properties"}
    $mymenu add command -label [_ "MIDI..."] \
        -command {pdsend "pd midi-properties"}
    $mymenu add check -label [_ "Zoom New Windows"] \
        -variable ::zoom_open \
        -command {pdsend "pd zoom-open $zoom_open"}
    $mymenu add  separator
    $mymenu add command -label [_ "Save All Preferences"] \
        -command {pdsend "pd save-preferences"}
    $mymenu add command -label [_ "Save to..."] \
        -command {::pd_menus::savepreferences}
    $mymenu add command -label [_ "Load from..."] \
        -command {::pd_menus::loadpreferences}
    $mymenu add command -label [_ "Forget All..."] \
        -command {::pd_menus::forgetpreferences}
		
		
    switch -- $::windowingsystem {
        "win32" {
            package require registry
            if { [registry get HKEY_CLASSES_ROOT\\PureData\\shell\\open\\command ""] != [::pd_menus::mswindows_file_associationcmd] } {
                set regstate normal
            } {
                set regstate disabled
            }
            $mymenu add command -label [_ "Always open .pd files with this Pd"] \
                -state $regstate \
                -command { ::pd_menus::mswindows_file_association }
        }
    }
}

# ------------------------------------------------------------------------------
# menu building functions for Mac OS X/aqua

# for Mac OS X only
proc ::pd_menus::create_apple_menu {mymenu} {
    # TODO this should open a Pd patch called about.pd
    menu $mymenu.apple
    $mymenu.apple add command -label [_ "About Pd"] -command {menu_aboutpd}
    $mymenu.apple add separator
    create_preferences_menu $mymenu.apple.preferences
    $mymenu.apple add cascade -label [_ "Preferences"] \
        -menu $mymenu.apple.preferences
    # this needs to be last for things to function properly
    $mymenu add cascade -label "Apple" -menu $mymenu.apple
}

proc ::pd_menus::build_file_menu_aqua {mymenu} {
    variable accelerator
    $mymenu add command -label [_ "New"]        -accelerator "$accelerator+N"
    $mymenu add command -label [_ "Open"]       -accelerator "$accelerator+O"
    # this is now done in main ::pd_menus::build_file_menu
    #::pd_menus::update_openrecent_menu_aqua .openrecent
    $mymenu add cascade -label [_ "Open Recent"] -menu .openrecent
    $mymenu add  separator
    $mymenu add command -label [_ "Close"]      -accelerator "$accelerator+W"
    $mymenu add command -label [_ "Save"]       -accelerator "$accelerator+S"
    $mymenu add command -label [_ "Save As..."] -accelerator "$accelerator+Shift+S"
    #$mymenu add command -label [_ "Save All"]
    #$mymenu add command -label [_ "Revert to Saved"]
    $mymenu add  separator
    $mymenu add command -label [_ "Message..."] -accelerator "$accelerator+Shift+M"
    $mymenu add  separator
    $mymenu add command -label [_ "Print..."]   -accelerator "$accelerator+P"
}

# the "Edit", "Put", and "Find" menus do not have cross-platform differences

proc ::pd_menus::build_media_menu_aqua {mymenu} {
}

proc ::pd_menus::build_window_menu_aqua {mymenu} {
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
# menu building functions for UNIX/X11

proc ::pd_menus::build_file_menu_x11 {mymenu} {
    variable accelerator
    $mymenu add command -label [_ "New"]         -accelerator "$accelerator+N"
    $mymenu add command -label [_ "Open"]        -accelerator "$accelerator+O"
    $mymenu add  separator
    $mymenu add command -label [_ "Save"]        -accelerator "$accelerator+S"
    $mymenu add command -label [_ "Save As..."]  -accelerator "Shift+$accelerator+S"
    #    $mymenu add command -label "Revert"
    $mymenu add  separator
    $mymenu add command -label [_ "Message..."]  -accelerator "$accelerator+Shift+M"
    create_preferences_menu $mymenu.preferences
    $mymenu add cascade -label [_ "Preferences"] -menu $mymenu.preferences
    $mymenu add command -label [_ "Print..."]    -accelerator "$accelerator+P"
    $mymenu add  separator
    # the recent files get inserted in here by update_recentfiles_on_menu
    $mymenu add  separator
    $mymenu add command -label [_ "Close"]       -accelerator "$accelerator+W"
    $mymenu add command -label [_ "Quit"]        -accelerator "$accelerator+Q" \
        -command {pdsend "pd verifyquit"}
}

# the "Edit", "Put", and "Find" menus do not have cross-platform differences

proc ::pd_menus::build_media_menu_x11 {mymenu} {
}

proc ::pd_menus::build_window_menu_x11 {mymenu} {
}

# the "Help" does not have cross-platform differences

# ------------------------------------------------------------------------------
# menu building functions for Windows/Win32

# for Windows only
proc ::pd_menus::create_system_menu {mymenubar} {
    set mymenu $mymenubar.system
    $mymenubar add cascade -label System -menu $mymenu
    menu $mymenu -tearoff 0
    # placeholders
    $mymenu add command -label [_ "Edit Mode"] -command "::pdwindow::verbose 0 systemmenu"
    # TODO add Close, Minimize, etc and whatever else is on the little menu
    # that is on the top left corner of the window frame
    # http://wiki.tcl.tk/1006
    # TODO add Edit Mode here
}

proc ::pd_menus::build_file_menu_win32 {mymenu} {
    variable accelerator
    $mymenu add command -label [_ "New"]         -accelerator "$accelerator+N"
    $mymenu add command -label [_ "Open"]        -accelerator "$accelerator+O"
    $mymenu add  separator
    $mymenu add command -label [_ "Save"]        -accelerator "$accelerator+S"
    $mymenu add command -label [_ "Save As..."]  -accelerator "Shift+$accelerator+S"
    #    $mymenu add command -label "Revert"
    $mymenu add  separator
    $mymenu add command -label [_ "Message..."]  -accelerator "$accelerator+Shift+M"
    create_preferences_menu $mymenu.preferences
    $mymenu add cascade -label [_ "Preferences"] -menu $mymenu.preferences
    $mymenu add command -label [_ "Print..."]    -accelerator "$accelerator+P"
    $mymenu add  separator
    # the recent files get inserted in here by update_recentfiles_on_menu
    $mymenu add  separator
    $mymenu add command -label [_ "Close"]       -accelerator "$accelerator+W"
    $mymenu add command -label [_ "Quit"]        -accelerator "$accelerator+Q" \
        -command {pdsend "pd verifyquit"}
}

proc ::pd_menus::mswindows_file_associationcmd {} {
    set wish [regsub -all "/" [file normalize [info nameofexecutable]] "\\"]
    set script [regsub -all "/" ${::pdgui::scriptname} "\\"]
    return "${wish} \"${script}\" %1"
}

proc ::pd_menus::mswindows_file_association {} {
    # file with icons: either ${::pdgui::pd_exec}.exe or ${script}/../pd.ico
    set icon ""
    foreach f [list ${::pdgui::pd_exec}.exe [file join [file dirname ${::pdgui::scriptname}] pd.ico ] ] {
        if {[file exists $f]} {
            set icon  [regsub -all "/" $f "\\"]
            break
        }
    }
    package require registry
    if {[catch {registry set HKEY_CLASSES_ROOT\\.pd "" "PureData"}  errmsg ]  } {
        ::pdwindow::error "${errmsg}\n"
        ::pdwindow::post "\"Administrator Permissions\" are required to set the file associations.\nSave your work and close Pd.\nThen right-click on Pd's icon and select \"Run as Administrator\".\nFinally try again \"Always open .pd files with this Pd.\".\n"
    } else {
        registry set HKEY_CLASSES_ROOT\\.pd "" "PureData"
        registry set HKEY_CLASSES_ROOT\\PureData "" ""
        if {"${icon}" != ""} {
            registry set HKEY_CLASSES_ROOT\\PureData\\DefaultIcon "" "${icon}"
        }
        registry set HKEY_CLASSES_ROOT\\PureData\\shell "" ""
        registry set HKEY_CLASSES_ROOT\\PureData\\shell\\open "" ""
        registry set HKEY_CLASSES_ROOT\\PureData\\shell\\open\\command "" [::pd_menus::mswindows_file_associationcmd]
        ::pdwindow::post "Registry updated successfully to open .pd files with \"${::pdgui::pd_exec}\"\n"
    }
}

# the "Edit", "Put", and "Find" menus do not have cross-platform differences

proc ::pd_menus::build_media_menu_win32 {mymenu} {
}

proc ::pd_menus::build_window_menu_win32 {mymenu} {
}

# the "Help" does not have cross-platform differences
