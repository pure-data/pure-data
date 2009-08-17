# Copyright (c) 1997-2009 Miller Puckette.
#(c) 2008 WordTech Communications LLC. License: standard Tcl license, http://www.tcl.tk/software/tcltk/license.html

package provide pd_menus 0.1

package require pd_menucommands
package require Tk
#package require tile
## replace Tk widgets with Ttk widgets on 8.5
#namespace import -force ttk::*

# TODO figure out Undo/Redo/Cut/Copy/Paste/DSP state changes for menus
# TODO figure out parent window/window list for Window menu
# TODO what is the Tcl package constructor or init()?

    

# ------------------------------------------------------------------------------
# global variables

# TODO this should properly be inside the pd_menus namespace, now it is global
namespace import ::pd_menucommands::* 

namespace eval ::pd_menus:: {
    variable accelerator
    
    namespace export create_menubar
    namespace export configure_pdwindow

    # turn off tearoff menus globally
    option add *tearOff 0
}

# ------------------------------------------------------------------------------
# 
proc ::pd_menus::create_menubar {mymenubar mytoplevel} {
    variable accelerator
    if {$::windowingsystem eq "aqua"} {
        set accelerator "Cmd"
    } else {
        set accelerator "Ctrl"
    }
    menu $mymenubar
    set menulist "file edit put find media window help"
    if { $::windowingsystem eq "aqua" } {create_apple_menu $mymenubar}
#TODO figure out why this took my menubars out? -msp
#    if { $::windowingsystem eq "win32" } {create_system_menu $mymenubar}
    foreach mymenu $menulist {  
        menu $mymenubar.$mymenu
        $mymenubar add cascade -label [_ [string totitle $mymenu]] \
            -menu $mymenubar.$mymenu
        [format build_%s_menu $mymenu] $mymenubar.$mymenu $mytoplevel
        if {$::windowingsystem eq "win32"} {
            # fix menu font size on Windows with tk scaling = 1
            $mymenubar.$mymenu configure -font menufont
        }
    }
}

proc ::pd_menus::configure_pdwindow {mymenubar} {
    # these are meaningless for the Pd window, so disable them
    set file_items_to_disable {"Save" "Save As..." "Print..." "Close"}
    foreach menuitem $file_items_to_disable {
        $mymenubar.file entryconfigure [_ $menuitem] -state disabled
    }
    set edit_items_to_disable {"Undo" "Redo" "Duplicate" "Tidy Up" "Edit Mode"}
    foreach menuitem $edit_items_to_disable {
        $mymenubar.edit entryconfigure [_ $menuitem] -state disabled
    }
    # disable everything on the Put menu
    for {set i 0} {$i <= [$mymenubar.put index end]} {incr i} {
        # catch errors by trying to disable separators
        catch {$mymenubar.put entryconfigure $i -state disabled }
    }
}

# ------------------------------------------------------------------------------
# menu building functions
proc ::pd_menus::build_file_menu {mymenu mytoplevel} {
    [format build_file_menu_%s $::windowingsystem] $mymenu
    $mymenu entryconfigure [_ "New"]        -command "menu_new"
    $mymenu entryconfigure [_ "Open"]       -command "menu_open"
    $mymenu entryconfigure [_ "Save"]       -command "pdsend \"$mytoplevel menusave\""
    $mymenu entryconfigure [_ "Save As..."] -command "pdsend \"$mytoplevel menusaveas\""
    #   $mymenu entryconfigure "Revert*"    -command "menu_revert $mytoplevel"
    $mymenu entryconfigure [_ "Close"]      -command "pdsend \"$mytoplevel menuclose 0\""
    $mymenu entryconfigure [_ "Message"]    -command "menu_message_panel"
    $mymenu entryconfigure [_ "Print..."]   -command "menu_print $mytoplevel"
}

proc ::pd_menus::build_edit_menu {mymenu mytoplevel} {
    variable accelerator
    $mymenu add command -label [_ "Undo"]       -accelerator "$accelerator+Z" \
        -command "menu_undo $mytoplevel"
    $mymenu add command -label [_ "Redo"]       -accelerator "Shift+$accelerator+Z" \
        -command "menu_redo $mytoplevel"
    $mymenu add  separator
    $mymenu add command -label [_ "Cut"]        -accelerator "$accelerator+X" \
        -command "pdsend \"$mytoplevel cut\""
    $mymenu add command -label [_ "Copy"]       -accelerator "$accelerator+C" \
        -command "pdsend \"$mytoplevel copy\""
    $mymenu add command -label [_ "Paste"]      -accelerator "$accelerator+V" \
        -command "pdsend \"$mytoplevel paste\""
    $mymenu add command -label [_ "Duplicate"]  -accelerator "$accelerator+D" \
        -command "pdsend \"$mytoplevel duplicate\""
    $mymenu add command -label [_ "Select All"] -accelerator "$accelerator+A" \
        -command "pdsend \"$mytoplevel selectall\""
    $mymenu add  separator
    if {$::windowingsystem eq "aqua"} {
        $mymenu add command -label [_ "Text Editor"] \
            -command "menu_texteditor $mytoplevel"
        $mymenu add command -label [_ "Font"]  -accelerator "$accelerator+T" \
            -command "menu_dialog_font $mytoplevel"
    } else {
        $mymenu add command -label [_ "Text Editor"] -accelerator "$accelerator+T"\
            -command "menu_texteditor $mytoplevel"
        $mymenu add command -label [_ "Font"] \
            -command "menu_dialog_font $mytoplevel"
    }
    $mymenu add command -label [_ "Tidy Up"] \
        -command "pdsend \"$mytoplevel tidy\""
    # $mymenu add command -label [_ "Toggle Console"] -accelerator "Shift+$accelerator+R" \
    #   -command {.controls.switches.console invoke}
    # $mymenu add command -label [_ "Clear Console"] -accelerator "Shift+$accelerator+L" \
    #   -command "menu_clear_console"
    $mymenu add  separator
    $mymenu add radiobutton -label [_ "Edit Mode"] -accelerator "$accelerator+E" \
        -indicatoron true -selectcolor grey85 \
        -command "pdsend \"$mytoplevel editmode 0\""
    #    if { $editable == 0 } {
    #        $mymenu entryconfigure "Edit Mode" -indicatoron false 
    #    }

    #if { ! [catch {console hide}]} { 
    # TODO set up menu item to show/hide the Tcl/Tk console, if it available
    #}

    if {$::windowingsystem ne "aqua"} {
        $mymenu add  separator
        $mymenu add command -label [_ "Path..."] \
            -command "menu_path_panel"
        $mymenu add command -label [_ "Startup..."] \
            -command "menu_startup_panel"
    }
}

proc ::pd_menus::build_put_menu {mymenu mytoplevel} {
    variable accelerator
    $mymenu add command -label [_ "Object"] -accelerator "$accelerator+1" \
        -command "pdsend \"$mytoplevel obj 0\"" 
    $mymenu add command -label [_ "Message"] -accelerator "$accelerator+2" \
        -command "pdsend \"$mytoplevel msg 0\""
    $mymenu add command -label [_ "Number"] -accelerator "$accelerator+3" \
        -command "pdsend \"$mytoplevel floatatom  0\""
    $mymenu add command -label [_ "Symbol"] -accelerator "$accelerator+4" \
        -command "pdsend \"$mytoplevel symbolatom  0\""
    $mymenu add command -label [_ "Comment"] -accelerator "$accelerator+5" \
        -command "pdsend \"$mytoplevel text  0\""
    $mymenu add  separator
    $mymenu add command -label [_ "Bang"]    -accelerator "Shift+$accelerator+B" \
        -command "pdsend \"$mytoplevel bng  0\""
    $mymenu add command -label [_ "Toggle"]  -accelerator "Shift+$accelerator+T" \
        -command "pdsend \"$mytoplevel toggle  0\""
    $mymenu add command -label [_ "Number2"] -accelerator "Shift+$accelerator+N" \
        -command "pdsend \"$mytoplevel numbox  0\""
    $mymenu add command -label [_ "Vslider"] -accelerator "Shift+$accelerator+V" \
        -command "pdsend \"$mytoplevel vslider  0\""
    $mymenu add command -label [_ "Hslider"] -accelerator "Shift+$accelerator+H" \
        -command "pdsend \"$mytoplevel hslider  0\""
    $mymenu add command -label [_ "Vradio"]  -accelerator "Shift+$accelerator+D" \
        -command "pdsend \"$mytoplevel vradio  0\""
    $mymenu add command -label [_ "Hradio"]  -accelerator "Shift+$accelerator+I" \
        -command "pdsend \"$mytoplevel hradio  0\""
    $mymenu add command -label [_ "VU Meter"] -accelerator "Shift+$accelerator+U"\
        -command "pdsend \"$mytoplevel vumeter  0\""
    $mymenu add command -label [_ "Canvas"]  -accelerator "Shift+$accelerator+C" \
        -command "pdsend \"$mytoplevel mycnv  0\""
    $mymenu add  separator
    $mymenu add command -label Graph -command "pdsend \"$mytoplevel graph\"" 
    $mymenu add command -label Array -command "pdsend \"$mytoplevel menuarray\""
}

proc ::pd_menus::build_find_menu {mymenu mytoplevel} {
    variable accelerator
    $mymenu add command -label [_ "Find..."]    -accelerator "$accelerator+F" \
        -command "::dialog_find::menu_dialog_find $mytoplevel"
    $mymenu add command -label [_ "Find Again"] -accelerator "$accelerator+G" \
        -command "pdsend \"$mytoplevel findagain\""
    $mymenu add command -label [_ "Find Last Error"] \
        -command "pdsend \"$mytoplevel finderror\"" 
}

proc ::pd_menus::build_media_menu {mymenu mytoplevel} {
    variable accelerator
    $mymenu add radiobutton -label [_ "Audio ON"] -accelerator "$accelerator+/" \
        -command "pdsend \"pd dsp 1\""
    $mymenu add radiobutton -label [_ "Audio OFF"] -accelerator "$accelerator+." \
        -command "pdsend \"pd dsp 0\"" -indicatoron true 
    $mymenu add  separator    
    $mymenu add command -label [_ "Audio settings..."] \
        -command "pdsend \"pd audio-properties\"" 
    $mymenu add command -label [_ "MIDI settings..."] \
        -command "pdsend \"pd midi-properties\"" 
    $mymenu add  separator    
    $mymenu add command -label [_ "Test Audio and MIDI..."] \
        -command "menu_doc_open doc/7.stuff/tools testtone.pd" 
    $mymenu add command -label [_ "Load Meter"] \
        -command "menu_doc_open doc/7.stuff/tools load-meter.pd" 
}

proc ::pd_menus::build_window_menu {mymenu mytoplevel} {
    variable accelerator
    if {$::windowingsystem eq "aqua"} {
        $mymenu add command -label [_ "Minimize"] -command "menu_minimize ." \
            -accelerator "$accelerator+M"
        $mymenu add command -label [_ "Zoom"] -command "menu_zoom ."
        $mymenu add  separator
    }
    $mymenu add command -label [_ "Parent Window"] \
        -command "pdsend \"$mytoplevel findparent\""
    $mymenu add command -label [_ "Pd window"] -command "menu_raise_pdwindow" \
        -accelerator "$accelerator+R"
    $mymenu add  separator
    if {$::windowingsystem eq "aqua"} {
        $mymenu add command -label [_ "Bring All to Front"] \
            -command "menu_bringalltofront"
        $mymenu add  separator
    }
}

proc ::pd_menus::build_help_menu {mymenu mytoplevel} {
    if {$::windowingsystem ne "aqua"} {
        $mymenu add command -label {About Pd} \
            -command "placeholder menu_doc_open doc/1.manual 1.introduction.txt" 
    }
    $mymenu add command -label {HTML ...} \
        -command "placeholder menu_doc_open doc/1.manual index.htm"
    $mymenu add command -label {Browser ...} \
        -command "placeholder menu_helpbrowser \$help_top_directory" 
}

# ------------------------------------------------------------------------------
# menu building functions for Mac OS X/aqua

# for Mac OS X only
proc ::pd_menus::create_apple_menu {mymenu} {
    puts stderr BUILD_APPLE_MENU
    # TODO this should open a Pd patch called about.pd
    menu $mymenu.apple
    $mymenu.apple add command -label [_ "About Pd"] \
        -command "menu_doc_open doc/1.manual 1.introduction.txt" 
    $mymenu add cascade -label "Apple" -menu $mymenu.apple
    $mymenu.apple add  separator
    # starting in 8.4.14, this is created automatically
    set patchlevel [split [info patchlevel] .]
    if {[lindex $patchlevel 1] < 5 && [lindex $patchlevel 2] < 14} {
        $mymenu.apple add command -label [_ "Preferences..."] \
            -command "menu_preferences_panel" -accelerator "Cmd+,"
    }
}

proc ::pd_menus::build_file_menu_aqua {mymenu} {
    variable accelerator
    $mymenu add command -label [_ "New"]       -accelerator "$accelerator+N"
    $mymenu add command -label [_ "Open"]      -accelerator "$accelerator+O"
    $mymenu add cascade -label [_ "Open Recent"]
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
    #   $mymenu add command -label "Revert"
    $mymenu add  separator
    $mymenu add command -label [_ "Message"]    -accelerator "$accelerator+M"
    $mymenu add command -label [_ "Print..."]   -accelerator "$accelerator+P"
    $mymenu add  separator
    $mymenu add command -label [_ "Close"]      -accelerator "$accelerator+W"
    $mymenu add command -label [_ "Quit"]       -accelerator "$accelerator+Q" \
        -command "pdsend \"pd verifyquit\""
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
    #   $mymenu add command -label "Revert"
    $mymenu add  separator
    $mymenu add command -label [_ "Message"]  -accelerator "$accelerator+M"
    $mymenu add command -label [_ "Print..."] -accelerator "$accelerator+P"
    $mymenu add  separator
    $mymenu add command -label [_ "Close"]    -accelerator "$accelerator+W"
    $mymenu add command -label [_ "Quit"]     -accelerator "$accelerator+Q"\
        -command "pdsend \"pd verifyquit\""
}

# the "Edit", "Put", and "Find" menus do not have cross-platform differences

proc ::pd_menus::build_media_menu_win32 {mymenu} {
}

proc ::pd_menus::build_window_menu_win32 {mymenu} {
}

# the "Help" does not have cross-platform differences

