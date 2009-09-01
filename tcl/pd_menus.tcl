# Copyright (c) 1997-2009 Miller Puckette.
#(c) 2008 WordTech Communications LLC. License: standard Tcl license, http://www.tcl.tk/software/tcltk/license.html

package provide pd_menus 0.1

package require pd_menucommands
package require Tk
#package require tile
## replace Tk widgets with Ttk widgets on 8.5
#namespace import -force ttk::*

# TODO figure out Undo/Redo/Cut/Copy/Paste state changes for menus
# TODO figure out parent window/window list for Window menu
# TODO what is the Tcl package constructor or init()?
# TODO $::pd_menus::menubar or .menubar globally?

# since there is one menubar that is used for all windows, the menu -commands
# use {} quotes so that $::focused_window is interpreted when the menu item
# is called, not when the command is mapped to the menu item.  This is the
# opposite of the 'bind' commands in pd_bindings.tcl
    

# ------------------------------------------------------------------------------
# global variables

# TODO this should properly be inside the pd_menus namespace, now it is global
namespace import ::pd_menucommands::* 

namespace eval ::pd_menus:: {
    variable accelerator
    variable menubar ".menubar"
    variable current_toplevel ".pdwindow"
    
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
    set menulist "file edit put find media window help"
    if { $::windowingsystem eq "aqua" } {create_apple_menu $menubar}
    # FIXME why does the following (if uncommented) kill my menubar?
    # if { $::windowingsystem eq "win32" } {create_system_menu $menubar}
    foreach mymenu $menulist {    
        menu $menubar.$mymenu
        $menubar add cascade -label [_ [string totitle $mymenu]] \
            -menu $menubar.$mymenu
        [format build_%s_menu $mymenu] $menubar.$mymenu .
        if {$::windowingsystem eq "win32"} {
            # fix menu font size on Windows with tk scaling = 1
            $menubar.$mymenu configure -font menufont
        }
    }
}

proc ::pd_menus::configure_for_pdwindow {} {
    variable menubar
    # these are meaningless for the Pd window, so disable them
    set file_items_to_disable {"Save" "Save As..." "Print..." "Close"}
    foreach menuitem $file_items_to_disable {
        $menubar.file entryconfigure [_ $menuitem] -state disabled
    }
    set edit_items_to_disable {"Undo" "Redo" "Duplicate" "Tidy Up" "Edit Mode"}
    foreach menuitem $edit_items_to_disable {
        $menubar.edit entryconfigure [_ $menuitem] -state disabled
    }
    # disable everything on the Put menu
    for {set i 0} {$i <= [$menubar.put index end]} {incr i} {
        # catch errors that happen when trying to disable separators
        catch {$menubar.put entryconfigure $i -state disabled }
    }
}

proc ::pd_menus::configure_for_canvas {mytoplevel} {
    variable menubar
    set file_items_to_disable {"Save" "Save As..." "Print..." "Close"}
    foreach menuitem $file_items_to_disable {
        $menubar.file entryconfigure [_ $menuitem] -state normal
    }
    set edit_items_to_disable {"Undo" "Redo" "Duplicate" "Tidy Up" "Edit Mode"}
    foreach menuitem $edit_items_to_disable {
        $menubar.edit entryconfigure [_ $menuitem] -state normal
    }
    for {set i 0} {$i <= [$menubar.put index end]} {incr i} {
        # catch errors that happen when trying to disable separators
        catch {$menubar.put entryconfigure $i -state normal }
    }
    # TODO set "Edit Mode" state using editmode($mytoplevel)
}

proc ::pd_menus::configure_for_dialog {mytoplevel} {
    variable menubar
    # these are meaningless for the dialog panels, so disable them
    set file_items_to_disable {"Save" "Save As..." "Print..." "Close"}
    foreach menuitem $file_items_to_disable {
        $menubar.file entryconfigure [_ $menuitem] -state disabled
    }
    set edit_items_to_disable {"Undo" "Redo" "Duplicate" "Tidy Up" "Edit Mode"}
    foreach menuitem $edit_items_to_disable {
        $menubar.edit entryconfigure [_ $menuitem] -state disabled
    }
    # disable everything on the Put menu
    for {set i 0} {$i <= [$menubar.put index end]} {incr i} {
        # catch errors that happen when trying to disable separators
        catch {$menubar.put entryconfigure $i -state disabled }
    }
}


# ------------------------------------------------------------------------------
# menu building functions
proc ::pd_menus::build_file_menu {mymenu mytoplevel} {
    [format build_file_menu_%s $::windowingsystem] $mymenu
    $mymenu entryconfigure [_ "New"]        -command {menu_new}
    $mymenu entryconfigure [_ "Open"]       -command {menu_open}
    $mymenu entryconfigure [_ "Save"]       -command {pdsend "$::focused_window menusave"}
    $mymenu entryconfigure [_ "Save As..."] -command {pdsend "$::focused_window menusaveas"}
    #$mymenu entryconfigure [_ "Revert*"]    -command {menu_revert $current_toplevel}
    $mymenu entryconfigure [_ "Close"]      -command {pdsend "$::focused_window menuclose 0"}
    $mymenu entryconfigure [_ "Message"]    -command {menu_message_dialog}
    $mymenu entryconfigure [_ "Print..."]   -command {menu_print $::focused_window}
}

proc ::pd_menus::build_edit_menu {mymenu mytoplevel} {
    variable accelerator
    $mymenu add command -label [_ "Undo"]       -accelerator "$accelerator+Z" \
        -command {menu_undo $::focused_window}
    $mymenu add command -label [_ "Redo"]       -accelerator "Shift+$accelerator+Z" \
        -command {menu_redo $::focused_window}
    $mymenu add  separator
    $mymenu add command -label [_ "Cut"]        -accelerator "$accelerator+X" \
        -command {pdsend "$::focused_window cut"}
    $mymenu add command -label [_ "Copy"]       -accelerator "$accelerator+C" \
        -command {pdsend "$::focused_window copy"}
    $mymenu add command -label [_ "Paste"]      -accelerator "$accelerator+V" \
        -command {pdsend "$::focused_window paste"}
    $mymenu add command -label [_ "Duplicate"]  -accelerator "$accelerator+D" \
        -command {pdsend "$::focused_window duplicate"}
    $mymenu add command -label [_ "Select All"] -accelerator "$accelerator+A" \
        -command {pdsend "$::focused_window selectall"}
    $mymenu add  separator
    if {$::windowingsystem eq "aqua"} {
        $mymenu add command -label [_ "Text Editor"] \
            -command {menu_texteditor $::focused_window}
        $mymenu add command -label [_ "Font"]  -accelerator "$accelerator+T" \
            -command {menu_font_dialog $::focused_window}
    } else {
        $mymenu add command -label [_ "Text Editor"] -accelerator "$accelerator+T"\
            -command {menu_texteditor $::focused_window}
        $mymenu add command -label [_ "Font"] \
            -command {menu_font_dialog $::focused_window}
    }
    $mymenu add command -label [_ "Tidy Up"] \
        -command {pdsend "$::focused_window tidy"}
    $mymenu add command -label [_ "Toggle Console"] -accelerator "Shift+$accelerator+R" \
        -command {.controls.switches.console invoke}
    $mymenu add command -label [_ "Clear Console"] -accelerator "Shift+$accelerator+L" \
        -command {menu_clear_console}
    $mymenu add  separator
    #TODO madness! how to do set the state of the check box without invoking the menu!
    $mymenu add check -label [_ "Edit Mode"] -accelerator "$accelerator+E" \
        -selectcolor grey85 \
        -command {pdsend "$::focused_window editmode 0"}
    #if { ! [catch {console hide}]} { 
    # TODO set up menu item to show/hide the Tcl/Tk console, if it available
    #}

    if {$::windowingsystem ne "aqua"} {
        $mymenu add  separator
        $mymenu add command -label [_ "Preferences"] \
            -command {menu_preferences_dialog}
    }
}

proc ::pd_menus::build_put_menu {mymenu mytoplevel} {
    variable accelerator
    $mymenu add command -label [_ "Object"] -accelerator "$accelerator+1" \
        -command {pdsend "$::focused_window obj 0"} 
    $mymenu add command -label [_ "Message"] -accelerator "$accelerator+2" \
        -command {pdsend "$::focused_window msg 0"}
    $mymenu add command -label [_ "Number"] -accelerator "$accelerator+3" \
        -command {pdsend "$::focused_window floatatom  0"}
    $mymenu add command -label [_ "Symbol"] -accelerator "$accelerator+4" \
        -command {pdsend "$::focused_window symbolatom  0"}
    $mymenu add command -label [_ "Comment"] -accelerator "$accelerator+5" \
        -command {pdsend "$::focused_window text  0"}
    $mymenu add  separator
    $mymenu add command -label [_ "Bang"]    -accelerator "Shift+$accelerator+B" \
        -command {pdsend "$::focused_window bng  0"}
    $mymenu add command -label [_ "Toggle"]  -accelerator "Shift+$accelerator+T" \
        -command {pdsend "$::focused_window toggle  0"}
    $mymenu add command -label [_ "Number2"] -accelerator "Shift+$accelerator+N" \
        -command {pdsend "$::focused_window numbox  0"}
    $mymenu add command -label [_ "Vslider"] -accelerator "Shift+$accelerator+V" \
        -command {pdsend "$::focused_window vslider  0"}
    $mymenu add command -label [_ "Hslider"] -accelerator "Shift+$accelerator+H" \
        -command {pdsend "$::focused_window hslider  0"}
    $mymenu add command -label [_ "Vradio"]  -accelerator "Shift+$accelerator+D" \
        -command {pdsend "$::focused_window vradio  0"}
    $mymenu add command -label [_ "Hradio"]  -accelerator "Shift+$accelerator+I" \
        -command {pdsend "$::focused_window hradio  0"}
    $mymenu add command -label [_ "VU Meter"] -accelerator "Shift+$accelerator+U"\
        -command {pdsend "$::focused_window vumeter  0"}
    $mymenu add command -label [_ "Canvas"]  -accelerator "Shift+$accelerator+C" \
        -command {pdsend "$::focused_window mycnv  0"}
    $mymenu add  separator
    $mymenu add command -label [_ "Graph"] -command {pdsend "$::focused_window graph"} 
    $mymenu add command -label [_ "Array"] -command {pdsend "$::focused_window menuarray"}
}

proc ::pd_menus::build_find_menu {mymenu mytoplevel} {
    variable accelerator
    $mymenu add command -label [_ "Find..."]    -accelerator "$accelerator+F" \
        -command {::dialog_find::menu_find_dialog $::focused_window}
    $mymenu add command -label [_ "Find Again"] -accelerator "$accelerator+G" \
        -command {pdsend "$::focused_window findagain"}
    $mymenu add command -label [_ "Find Last Error"] \
        -command {pdsend "$::focused_window finderror"} 
}

proc ::pd_menus::build_media_menu {mymenu mytoplevel} {
    variable accelerator
    $mymenu add radiobutton -label [_ "DSP On"] -accelerator "$accelerator+/" \
        -variable ::dsp -value 1 -command {pdsend "pd dsp 1"}
    $mymenu add radiobutton -label [_ "DSP Off"] -accelerator "$accelerator+." \
        -variable ::dsp -value 0 -command {pdsend "pd dsp 0"}
    $mymenu add  separator

    set audioapi_list_length [llength $::audioapi_list]
    for {set x 0} {$x<$audioapi_list_length} {incr x} {
        # pdtk_post "audio [lindex [lindex $::audioapi_list $x] 0]"
        $mymenu add radiobutton -label [lindex [lindex $::audioapi_list $x] 0] \
            -command {menu_audio 0} -variable ::pd_whichapi \
            -value [lindex [lindex $::audioapi_list $x] 1]\
            -command {pdsend "pd audio-setapi $::pd_whichapi"}
    }
    if {$audioapi_list_length > 0} {$mymenu add separator}

    set midiapi_list_length [llength $::midiapi_list]
    for {set x 0} {$x<$midiapi_list_length} {incr x} {
        # pdtk_post "midi [lindex [lindex $::midiapi_list $x] 0]"
        $mymenu add radiobutton -label [lindex [lindex $::midiapi_list $x] 0] \
            -command {menu_midi 0} -variable ::pd_whichmidiapi \
            -value [lindex [lindex $::midiapi_list $x] 1]\
            -command {pdsend "pd midi-setapi $::pd_whichmidiapi"}
    }
    if {$midiapi_list_length > 0} {$mymenu add separator}

    if {$::windowingsystem ne "aqua"} {
        $mymenu add command -label [_ "Audio settings..."] \
            -command {pdsend "pd audio-properties"}
        $mymenu add command -label [_ "MIDI settings..."] \
            -command {pdsend "pd midi-properties"} 
        $mymenu add  separator
    }
    $mymenu add command -label [_ "Test Audio and MIDI..."] \
        -command {menu_doc_open doc/7.stuff/tools testtone.pd} 
    $mymenu add command -label [_ "Load Meter"] \
        -command {menu_doc_open doc/7.stuff/tools load-meter.pd} 
}

proc ::pd_menus::build_window_menu {mymenu mytoplevel} {
    variable accelerator
    if {$::windowingsystem eq "aqua"} {
        $mymenu add command -label [_ "Minimize"] -command {menu_minimize .} \
            -accelerator "$accelerator+M"
        $mymenu add command -label [_ "Zoom"] -command {menu_zoom .}
        $mymenu add  separator
    }
    $mymenu add command -label [_ "Parent Window"] \
        -command {pdsend "$::focused_window findparent"}
    $mymenu add command -label [_ "Pd window"] -command {menu_raise_pdwindow} \
        -accelerator "$accelerator+R"
    $mymenu add  separator
    if {$::windowingsystem eq "aqua"} {
        $mymenu add command -label [_ "Bring All to Front"] \
            -command {menu_bringalltofront}
        $mymenu add  separator
    }
}

proc ::pd_menus::build_help_menu {mymenu mytoplevel} {
    if {$::windowingsystem ne "aqua"} {
        $mymenu add command -label [_ "About Pd"] \
            -command {menu_doc_open doc/1.manual 1.introduction.txt} 
    }
    $mymenu add command -label [_ "HTML Manual..."] \
        -command {menu_doc_open doc/1.manual index.htm}
    $mymenu add command -label [_ "Browser..."] \
        -command {placeholder menu_helpbrowser \$help_top_directory} 
}

# ------------------------------------------------------------------------------
# update the menu entries for opening recent files
proc ::pd_menus::update_recentfiles_menu {} {
    variable menubar
    switch -- $::windowingsystem {
        "aqua" {::pd_menus::update_openrecent_menu_aqua .openrecent}
        "win32" {update_recentfiles_on_menu $menubar.file}
        "x11" {update_recentfiles_on_menu $menubar.file}
    }
}

proc ::pd_menus::clear_recentfiles_menu {} {
    set ::recentfiles_list {}
    ::pd_menus::update_recentfiles_menu
}

proc ::pd_menus::update_openrecent_menu_aqua {mymenu} {
    if {! [winfo exists $mymenu]} {menu $mymenu}
    $mymenu delete 0 end
    foreach filename $::recentfiles_list {
        puts "creating menu item for $filename"
        $mymenu add command -label [file tail $filename] \
            -command "open_file $filename"
    }
    $mymenu add  separator
    $mymenu add command -label [_ "Clear Menu"] \
        -command "::pd_menus::clear_recentfiles_menu"
}

# this expects to be run on the File menu, and to insert above the last separator
proc ::pd_menus::update_recentfiles_on_menu {mymenu} {
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
    set i 0
    foreach filename $::recentfiles_list {
        $mymenu insert [expr $top_separator+$i+1] command \
            -label [file tail $filename] -command "open_file $filename"
        incr i
    }
}

# ------------------------------------------------------------------------------
# menu building functions for Mac OS X/aqua

# for Mac OS X only
proc ::pd_menus::create_apple_menu {mymenu} {
    # TODO this should open a Pd patch called about.pd
    menu $mymenu.apple
    $mymenu.apple add command -label [_ "About Pd"] \
        -command {menu_doc_open doc/1.manual 1.introduction.txt} 
    $mymenu add cascade -label "Apple" -menu $mymenu.apple
    $mymenu.apple add  separator
    # starting in 8.4.14, this is created automatically
    set patchlevel [split [info patchlevel] .]
    if {[lindex $patchlevel 1] < 5 && [lindex $patchlevel 2] < 14} {
        $mymenu.apple add command -label [_ "Preferences..."] \
            -command {menu_preferences_dialog" -accelerator "Cmd+,}
    }
}

proc ::pd_menus::build_file_menu_aqua {mymenu} {
    variable accelerator
    $mymenu add command -label [_ "New"]       -accelerator "$accelerator+N"
    $mymenu add command -label [_ "Open"]      -accelerator "$accelerator+O"
    ::pd_menus::update_openrecent_menu_aqua .openrecent
    $mymenu add cascade -label [_ "Open Recent"] -menu .openrecent
    $mymenu add  separator
    $mymenu add command -label [_ "Close"]     -accelerator "$accelerator+W"
    $mymenu add command -label [_ "Save"]      -accelerator "$accelerator+S"
    $mymenu add command -label [_ "Save As..."] -accelerator "$accelerator+Shift+S"
    #$mymenu add command -label [_ "Save All"]
    #$mymenu add command -label [_ "Revert to Saved"]
    $mymenu add  separator
    $mymenu add command -label [_ "Message"]
    $mymenu add  separator
    $mymenu add command -label [_ "Print..."]   -accelerator "$accelerator+P"
}

# the "Edit", "Put", and "Find" menus do not have cross-platform differences

proc ::pd_menus::build_media_menu_aqua {mymenu} {
}

proc ::pd_menus::build_window_menu_aqua {mymenu} {
}

# the "Help" does not have cross-platform differences
 
# ------------------------------------------------------------------------------
# menu building functions for UNIX/X11

proc ::pd_menus::build_file_menu_x11 {mymenu} {
    variable accelerator
    $mymenu add command -label [_ "New"]        -accelerator "$accelerator+N"
    $mymenu add command -label [_ "Open"]       -accelerator "$accelerator+O"
    $mymenu add  separator
    $mymenu add command -label [_ "Save"]       -accelerator "$accelerator+S"
    $mymenu add command -label [_ "Save As..."] -accelerator "Shift+$accelerator+S"
    #    $mymenu add command -label "Revert"
    $mymenu add  separator
    $mymenu add command -label [_ "Message"]    -accelerator "$accelerator+M"
    $mymenu add command -label [_ "Print..."]   -accelerator "$accelerator+P"
    $mymenu add  separator
    # the recent files get inserted in here by update_recentfiles_on_menu
    $mymenu add  separator
    $mymenu add command -label [_ "Close"]      -accelerator "$accelerator+W"
    $mymenu add command -label [_ "Quit"]       -accelerator "$accelerator+Q" \
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
proc ::pd_menus::create_system_menu {mymenu} {
    $mymenu add cascade -menu [menu $mymenu.system]
    # TODO add Close, Minimize, etc and whatever else is on the little menu
    # that is on the top left corner of the window frame
}

proc ::pd_menus::build_file_menu_win32 {mymenu} {
    variable accelerator
    $mymenu add command -label [_ "New"]      -accelerator "$accelerator+N"
    $mymenu add command -label [_ "Open"]     -accelerator "$accelerator+O"
    $mymenu add  separator
    $mymenu add command -label [_ "Save"]      -accelerator "$accelerator+S"
    $mymenu add command -label [_ "Save As..."] -accelerator "Shift+$accelerator+S"
    #    $mymenu add command -label "Revert"
    $mymenu add  separator
    $mymenu add command -label [_ "Message"]  -accelerator "$accelerator+M"
    $mymenu add command -label [_ "Print..."] -accelerator "$accelerator+P"
    $mymenu add  separator
    # the recent files get inserted in here by update_recentfiles_on_menu
    $mymenu add  separator
    $mymenu add command -label [_ "Close"]    -accelerator "$accelerator+W"
    $mymenu add command -label [_ "Quit"]     -accelerator "$accelerator+Q"\
        -command {pdsend "pd verifyquit"}
}

# the "Edit", "Put", and "Find" menus do not have cross-platform differences

proc ::pd_menus::build_media_menu_win32 {mymenu} {
}

proc ::pd_menus::build_window_menu_win32 {mymenu} {
}

# the "Help" does not have cross-platform differences

