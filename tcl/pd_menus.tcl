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
    variable accelerator
    variable menubar ".menubar"

    namespace export create_menubar
    namespace export configure_for_pdwindow
    namespace export configure_for_canvas
    namespace export configure_for_dialog

    # turn off tearoff menus globally
    option add *tearOff 0

    array set keynames { \
                             "Key" "" \
                             "Mod1" "Cmd" \
                             "Command" "Cmd" \
                             "Control" "Ctrl" \
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

proc ::pd_menus::update_accelerators {{menu .}} {
    if { ! [winfo exists $menu] } {return}
    if { [winfo class $menu] == "Menu" } {
        # this is a real menu, so update it
        set lastmenu [$menu index end]
        for {set i 0} {$i <= $lastmenu} {incr i} {
            set cmd {}
            catch {
                set cmd [$menu entrycget $i -command]
            }
            if { $cmd == "" } {continue}
            if { [regexp -all {event generate \[focus\] <<([^>]*|[^>]*)>>} $cmd _ ev] } {
                set accel [ ::pd_menus::get_accelerator_for_event <<${ev}>>]
                $menu entryconfigure $i -accelerator $accel
            }
        }
    }
    foreach m [winfo children $menu] {
        ::pd_menus::update_accelerators $m
    }
}
proc ::pd_menus::get_events {{menu .} args} {
    # recursively search the given menu for any commands that generate a virtual event
    # and return a list of these events:
    # e.g. '::pd_menus::get_events .menu.edit' -> '{<<Copy>> <<Paste>>}'
    # the optional <args> is a list of additional options to query:
    # e.g. '::pd_menus::get_events .menu.edit label accelerator' -> '{<<Copy>> {{Copy} {Ctrl-C}} <<Paste>> {{Paste} {Ctrl-V}}}'
    if { ! [winfo exists $menu] } {return}

    # flat list of events (for order preservation)
    set events {}
    # array with args-options of each event
    array set seen {}

    if { [winfo class $menu] == "Menu" } {
        # this is a real menu, so update it
        set lastmenu [$menu index end]
        for {set i 0} {$i <= $lastmenu} {incr i} {
            set cmd {}
            catch {
                # some menu types (e.g. Seperators) do not have a command, so beware
                set cmd [$menu entrycget $i -command]
            }
            if { $cmd == "" } {continue}
            if { [regexp -all {event generate .* (<<[^>]*|[^>]*>>)} $cmd _ ev] } {
                set options {}
                foreach a $args {
                    set o {}
                    catch {
                        set o [$menu entrycget $i -$a]
                    }
                    lappend options $o
                }
                if { [info exists seen($ev) ] } {
                    # already seen, check if there are some new options
                    set newoptions {}
                    foreach o0 $seen($ev) o1 $options {
                        if { $o0 != "" } {
                            lappend newoptions $o0
                        } else {
                            lappend newoptions $o1
                        }
                    }
                    set seen($ev) $newoptions
                } else {
                    lappend events $ev
                    set seen($ev) $options
                }
            }
        }
    }

    foreach m [winfo children $menu] {
        foreach {ev options} [::pd_menus::get_events $m $args] {
            if { [info exists seen($ev) ] } {
                # already seen, check if there are some new options
                set newoptions {}
                foreach o0 $seen($ev) o1 $options {
                    if { $o0 != "" } {
                        lappend newoptions $o0
                    } else {
                        lappend newoptions $o1
                    }
                }
                set seen($ev) $newoptions
            } else {
                lappend events $ev
                set seen($ev) $options
            }
        }
    }

    set result {}
    foreach ev $events {
        lappend result $ev
        if { $args != {} } {
            lappend result $seen($ev)
        }
    }
    return $result
}


proc ::pd_menus::build_file_menu {mymenu} {
    # run the platform-specific build_file_menu_* procs first, and config them
    [format build_file_menu_%s $::windowingsystem] $mymenu
    $mymenu entryconfigure [_ "New"]        -command {event generate [focus] <<File|New>>}
    $mymenu entryconfigure [_ "Open"]       -command {event generate [focus] <<File|Open>>}
    $mymenu entryconfigure [_ "Save"]       -command {event generate [focus] <<File|Save>>}
    $mymenu entryconfigure [_ "Save As..."] -command {event generate [focus] <<File|SaveAs>>}
    #$mymenu entryconfigure [_ "Revert*"]    -command {event generate [focus] <<File|Revert>>}
    $mymenu entryconfigure [_ "Close"]      -command {event generate [focus] <<File|Close>>}
    $mymenu entryconfigure [_ "Message..."] -command {event generate [focus] <<Pd|Message>>}
    $mymenu entryconfigure [_ "Print..."]   -command {event generate [focus] <<File|Print>>}
    # update recent files
    if {[llength $::recentfiles_list] > 0} {
        ::pd_menus::update_recentfiles_menu false
    }
}

proc ::pd_menus::build_edit_menu {mymenu} {
    variable accelerator
    add_menu $mymenu command [_ "Undo" ] "<<Edit|Undo>>"
    add_menu $mymenu command [_ "Redo" ] "<<Edit|Redo>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Cut" ] "<<Edit|Cut>>"
    add_menu $mymenu command [_ "Copy" ] "<<Edit|Copy>>"
    add_menu $mymenu command [_ "Paste" ] "<<Edit|Paste>>"
    add_menu $mymenu command [_ "Duplicate" ] "<<Edit|Duplicate>>"
    add_menu $mymenu command [_ "Paste Replace" ] "<<Edit|PasteReplace>>"
    add_menu $mymenu command [_ "Select All" ] "<<Edit|SelectAll>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Font" ] "<<Edit|Font>>"
    add_menu $mymenu command [_ "Zoom In" ] "<<Edit|ZoomIn>>"
    add_menu $mymenu command [_ "Zoom Out" ] "<<Edit|ZoomOut>>"

    add_menu $mymenu command [_ "Tidy Up" ] "<<Edit|TidyUp>>"
    add_menu $mymenu command [_ "(Dis)Connect Selection" ] "<<Edit|ConnectSelection>>"
    add_menu $mymenu command [_ "Triggerize" ] "<<Edit|Triggerize>>"
    add_menu $mymenu command [_ "Clear Console" ] "<<Pd|ClearConsole>>"
    $mymenu add  separator
    #TODO madness! how to set the state of the check box without invoking the menu!
    $mymenu add check -label [_ "Edit Mode"] \
        -variable ::editmode_button \
        -command {::pd_menucommands::scheduleAction menu_editmode $::editmode_button}
    set accel [::pd_menus::get_accelerator_for_event  <<Edit|EditMode>>]
    if { $accel != "" } {
        $mymenu entryconfigure end -accelerator $accel
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

proc ::pd_menus::build_find_menu {mymenu} {
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
    $mymenu add  separator
    add_menu $mymenu command [_ "Pd window" ] "<<Window|PdWindow>>"
    add_menu $mymenu command [_ "Parent Window" ] "<<Window|Parent>>"
    $mymenu add  separator
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
            -command "::pd_menucommands::scheduleAction open_file {$filename}"
    }
    # clear button
    $mymenu add  separator
    $mymenu add command -label [_ "Clear Menu"] \
        -command {event generate [focus] <<File|ClearRecentFiles>>}
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
            -label $label -command "::pd_menucommands::scheduleAction open_file {$filename}" -underline 0
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
    create_preferences_menu $mymenu.apple.preferences
    $mymenu.apple add cascade -label [_ "Preferences"] \
        -menu $mymenu.apple.preferences
    # this needs to be last for things to function properly
    $mymenu add cascade -label "Apple" -menu $mymenu.apple
}

proc ::pd_menus::build_file_menu_aqua {mymenu} {
    variable accelerator
    add_menu $mymenu command [_ "New" ] "<<File|New>>"
    add_menu $mymenu command [_ "Open" ] "<<File|Open>>"
    # this is now done in main ::pd_menus::build_file_menu
    #::pd_menus::update_openrecent_menu_aqua .openrecent
    $mymenu add cascade -label [_ "Open Recent"] -menu .openrecent
    $mymenu add  separator
    add_menu $mymenu command [_ "Close" ] "<<File|Close>>"
    add_menu $mymenu command [_ "Save" ] "<<File|Save>>"
    add_menu $mymenu command [_ "Save As..." ] "<<File|SaveAs>>"
    #add_menu $mymenu command [_ "Save All" ] "<<File|SaveAll>>"
    #add_menu $mymenu command [_ "Revert to Saved" ] "<<File|Revert>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Message..." ] "<<Pd|Message>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Print..." ] "<<File|Print>>"
}

# the other menus do not have cross-platform differences

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
    add_menu $mymenu command [_ "New" ] "<<File|New>>"
    add_menu $mymenu command [_ "Open" ] "<<File|Open>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Save" ] "<<File|Save>>"
    add_menu $mymenu command [_ "Save As..." ] "<<File|SaveAs>>"
    #    $mymenu add command -label "Revert"
    $mymenu add  separator
    add_menu $mymenu command [_ "Message..." ] "<<Pd|Message>>"
    create_preferences_menu $mymenu.preferences
    $mymenu add cascade -label [_ "Preferences"] -menu $mymenu.preferences
    add_menu $mymenu command [_ "Print..." ] "<<File|Print>>"
    $mymenu add  separator
    # the recent files get inserted in here by update_recentfiles_on_menu
    $mymenu add  separator
    add_menu $mymenu command [_ "Close" ] "<<File|Close>>"
    add_menu $mymenu command [_ "Quit" ] "<<File|Quit>>"
}

# the other menus do not have cross-platform differences

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

proc ::pd_menus::build_file_menu_win32 {mymenu} {
    variable accelerator
    add_menu $mymenu command [_ "New" ] "<<File|New>>"
    add_menu $mymenu command [_ "Open" ] "<<File|Open>>"
    $mymenu add  separator
    add_menu $mymenu command [_ "Save" ] "<<File|Save>>"
    add_menu $mymenu command [_ "Save As..." ] "<<File|SaveAs>>"
    #    $mymenu add command -label "Revert"
    $mymenu add  separator
    add_menu $mymenu command [_ "Message..." ] "<<Pd|Message>>"
    create_preferences_menu $mymenu.preferences
    $mymenu add cascade -label [_ "Preferences"] -menu $mymenu.preferences
    add_menu $mymenu command [_ "Print..." ] "<<File|Print>>"
    $mymenu add  separator
    # the recent files get inserted in here by update_recentfiles_on_menu
    $mymenu add  separator
    add_menu $mymenu command [_ "Close" ] "<<File|Close>>"
    add_menu $mymenu command [_ "Quit" ] "<<File|Quit>>"
}

# the other menus do not have cross-platform differences
