#!/bin/sh
# This line continues for Tcl, but is a single line for 'sh' \
    exec wish "$0" -- ${1+"$@"}
# For information on usage and redistribution, and for a DISCLAIMER OF ALL
# WARRANTIES, see the file, "LICENSE.txt," in this distribution.
# Copyright (c) 1997-2009 Miller Puckette.

# "." automatically gets a window, we don't want it.  Withdraw it before doing
# anything else, so that we don't get the automatic window flashing for a
# second while pd loads.
wm withdraw . 

puts -------------------------------pd-gui.tcl-----------------------------------

package require Tcl 8.3
package require Tk
package require Tk
if {[tk windowingsystem] ne "win32"} {package require msgcat}
# TODO figure out msgcat issue on Windows

# Pd's packages are stored in the same directory as the main script (pd-gui.tcl)
set auto_path [linsert $auto_path 0 [file dirname [info script]]]
package require pd_connect
package require pd_menus
package require pd_bindings
package require pdwindow
package require dialog_array
package require dialog_audio
package require dialog_canvas
package require dialog_font
package require dialog_gatom
package require dialog_iemgui
package require dialog_midi
package require pdtk_canvas
package require pdtk_text
# TODO eliminate this kludge:
package require wheredoesthisgo

# import into the global namespace for backwards compatibility
namespace import ::pd_connect::pdsend
namespace import ::pdwindow::pdtk_post
namespace import ::dialog_array::pdtk_array_dialog
namespace import ::dialog_audio::pdtk_audio_dialog
namespace import ::dialog_canvas::pdtk_canvas_dialog
namespace import ::dialog_font::pdtk_canvas_dofont
namespace import ::dialog_gatom::pdtk_gatom_dialog
namespace import ::dialog_iemgui::pdtk_iemgui_dialog
namespace import ::dialog_midi::pdtk_midi_dialog
namespace import ::dialog_midi::pdtk_alsa_midi_dialog

# hack - these should be better handled in the C code
namespace import ::dialog_array::pdtk_array_listview_new
namespace import ::dialog_array::pdtk_array_listview_fillpage
namespace import ::dialog_array::pdtk_array_listview_setpage
namespace import ::dialog_array::pdtk_array_listview_closeWindow

#------------------------------------------------------------------------------#
# global variables

set PD_MAJOR_VERSION 0
set PD_MINOR_VERSION 0
set PD_BUGFIX_VERSION 0
set PD_TEST_VERSION ""

set TCL_MAJOR_VERSION 0
set TCL_MINOR_VERSION 0
set TCL_BUGFIX_VERSION 0

# for testing which platform we are running on ("aqua", "win32", or "x11")
set windowingsystem ""

# variable for vwait so that 'pd-gui' will timeout if 'pd' never shows up
set wait4pd "init"

# canvas font, received from pd in pdtk_pd_startup, set in s_main.c
set font_family "courier"
set font_weight "normal"
# sizes of chars for each of the Pd fixed font sizes:
#  fontsize  width(pixels)  height(pixels)
set font_fixed_metrics {
    8 5 10
    9 6 11
    10 6 13
    12 7 15
    14 8 17
    16 10 20
    18 11 22
    24 14 30
    30 18 37
    36 22 45
}

# root path to lib of Pd's files, see s_main.c for more info
set sys_libdir {}
# root path where the pd-gui.tcl GUI script is located
set sys_guidir {}

set audioapi_list {}
set midiapi_list {}
set pd_whichapi 0
set pd_whichmidiapi 0

# current state of the DSP
set dsp 0
# the toplevel window that currently is on top and has focus
set focused_window .
# TODO figure out how to get all windows into the menu_windowlist
# store list of parent windows for Window menu
set menu_windowlist {}
# store that last 10 files that were opened
set recentfiles_list {}
set total_recentfiles 10
# keep track of the location of popup menu for CanvasWindows
set popup_xpix 0
set popup_ypix 0

## per toplevel/patch data
# store editmode for each open canvas, starting with a blank array
array set editmode {}

#------------------------------------------------------------------------------#
# coding style
#
# these are preliminary ideas, we'll change them as we work things out:
# - when possible use "" doublequotes to delimit messages
# - use '$::myvar' instead of 'global myvar' 
# - for the sake of clarity, there should not be any inline code, everything 
#   should be in a proc that is ultimately triggered from main()
# - if a menu_* proc opens a dialog panel, that proc is called menu_*_dialog
# - use "eq/ne" for string comparison, NOT "==/!=" (http://wiki.tcl.tk/15323)
#
## Names for Common Variables
#----------------------------
#
# variables named after the Tk widgets they represent
#   $mytoplevel = a window id made by a 'toplevel' command
#   $mygfxstub = a window id made by a 'toplevel' command via gfxstub/x_gui.c
#   $menubar = the 'menu' attached to each 'toplevel'
#   $mymenu = 'menu' attached to the menubar
#   $menuitem = 'menu' item
#   $mycanvas = 'canvas'
#   $canvasitem = 'canvas' item
#
#
## Prefix Names for procs
#----------------------------
# pdtk_     pd -> pd-gui API (i.e. called from 'pd')
# pdsend    pd-gui -> pd API (sends a message to 'pd' using pdsend)

# ------------------------------------------------------------------------------
# init functions

proc set_pd_version {versionstring} {
    regexp -- {.*([0-9])\.([0-9]+)[\.\-]([0-9]+)([^0-9]?.*)} $versionstring \
        wholematch \
        ::PD_MAJOR_VERSION ::PD_MINOR_VERSION ::PD_BUGFIX_VERSION ::PD_TEST_VERSION
}

proc set_tcl_version {} {
    regexp {([0-9])\.([0-9])\.([0-9]+)} [info patchlevel] \
        wholematch \
        ::TCL_MAJOR_VERSION ::TCL_MINOR_VERSION ::TCL_BUGFIX_VERSION
}

# root paths to find Pd's files where they are installed
proc set_pd_paths {} {
    set ::sys_guidir [file normalize [file dirname [info script]]]
    set ::sys_libdir [file normalize [file join $::sys_guidir ".."]]
}

proc init_for_platform {} {
    # we are not using Tk scaling, so fix it to 1 on all platforms.  This
    # guarantees that patches will be pixel-exact on every platform
    tk scaling 1

    switch -- $::windowingsystem {
        "x11" {
            # add control to show/hide hidden files in the open panel (load
            # the tk_getOpenFile dialog once, otherwise it will not work)
            catch {tk_getOpenFile -with-invalid-argument} 
            set ::tk::dialog::file::showHiddenBtn 1
            set ::tk::dialog::file::showHiddenVar 0
            # set file types that open/save recognize
            set ::filetypes \
                [list \
                     [list [_ "Associated Files"]  {.pd .pat .mxt} ] \
                     [list [_ "Pd Files"]          {.pd}  ] \
                     [list [_ "Max Patch Files"]   {.pat} ] \
                     [list [_ "Max Text Files"]    {.mxt} ] \
                    ]
        }
        "aqua" {
            # set file types that open/save recognize
            set ::filetypes \
                [list \
                     [list [_ "Associated Files"]       {.pd .pat .mxt} ] \
                     [list [_ "Pd Files"]               {.pd}  ] \
                     [list [_ "Max Patch Files (.pat)"] {.pat} ] \
                     [list [_ "Max Text Files (.mxt)"]  {.mxt} ] \
                    ]
        }
        "win32" {
            font create menufont -family Tahoma -size -11
            # set file types that open/save recognize
            set ::filetypes \
                [list \
                     [list [_ "Associated Files"]  {.pd .pat .mxt} ] \
                     [list [_ "Pd Files"]          {.pd}  ] \
                     [list [_ "Max Patch Files"]   {.pat} ] \
                     [list [_ "Max Text Files"]    {.mxt} ] \
                    ]
        }
    }
}

# ------------------------------------------------------------------------------
# locale handling

# official GNU gettext msgcat shortcut
if {[tk windowingsystem] ne "win32"} {
    proc _ {s} {return [::msgcat::mc $s]}
} else {
    proc _ {s} {return $s}
}

proc load_locale {} {
    if {[tk windowingsystem] ne "win32"} {
        ::msgcat::mcload [file join [file dirname [info script]] .. po]
    }

    # for Windows
    #set locale "en"  ;# Use whatever is right for your app
    #if {[catch {package require registry}]} {
    #        tk_messageBox -icon error -message "Could not get locale from registry"
    #} else {
    #    set locale [string tolower \
    #        [string range \
    #        [registry get {HKEY_CURRENT_USER\Control Panel\International} sLanguage] 0 1] ]
    #}

    ##--moo: force default system and stdio encoding to UTF-8
    encoding system utf-8
    fconfigure stderr -encoding utf-8
    fconfigure stdout -encoding utf-8
    ##--/moo
}

# ------------------------------------------------------------------------------
# font handling

# this proc gets the internal font name associated with each size
proc get_font_for_size {size} {
    return "::pd_font_${size}"
}

# searches for a font to use as the default.  Tk automatically assigns a
# monospace font to the name "Courier" (see Tk 'font' docs), but it doesn't
# always do a good job of choosing in respect to Pd's needs.  So this chooses
# from a list of fonts that are known to work well with Pd.
proc find_default_font {} {
    set testfonts {Inconsolata "Courier New" "Liberation Mono" FreeMono \
                       "DejaVu Sans Mono" "Bitstream Vera Sans Mono"}
    foreach family $testfonts {
        if {[lsearch -exact -nocase [font families] $family] > -1} {
            set ::font_family $family
            break
        }
    }
    puts "DEFAULT FONT: $::font_family"
}

proc set_base_font {family weight} {
    if {[lsearch -exact [font families] $family] > -1} {
        set ::font_family $family
    } else {
        pdtk_post [format \
                       [_ "WARNING: Font family '%s' not found, using default (%s)"] \
                       $family $::font_family]
    }
    if {[lsearch -exact {bold normal} $weight] > -1} {
        set ::font_weight $weight
        set using_defaults 0
    } else {
        pdtk_post [format \
                       [_ "WARNING: Font weight '%s' not found, using default (%s)"] \
                       $weight $::font_weight]
    }
}

# creates all the base fonts (i.e. pd_font_8 thru pd_font_36) so that they fit
# into the metrics given by $::font_fixed_metrics for any given font/weight
proc fit_font_into_metrics {} {
# TODO the fonts picked seem too small, probably on fixed width
    foreach {size width height} $::font_fixed_metrics {
        set myfont [get_font_for_size $size]
        font create $myfont -family $::font_family -weight $::font_weight \
            -size [expr {-$height}]
        set height2 $height
        set giveup 0
        while {[font measure $myfont M] > $width} {
            incr height2 -1
            font configure $myfont -size [expr {-$height2}]
            if {$height2 * 2 <= $height} {
                set giveup 1
                break
            }
        }
        if {$giveup} {
            pdtk_post [format \
               [_ "ERROR: %s failed to find font size (%s) that fits into %sx%s!"]\
               [lindex [info level 0] 0] $size $width $height]
            continue
        }
    }
}


# ------------------------------------------------------------------------------
# procs called directly by pd

# this is only called when 'pd' starts 'pd-gui', not the other way around
proc pdtk_pd_startup {versionstring audio_apis midi_apis sys_font sys_fontweight} {
#    pdtk_post "-------------- pdtk_pd_startup ----------------"
#    pdtk_post "version: $versionstring"
#    pdtk_post "audio_apis: $audio_apis"
#    pdtk_post "midi_apis: $midi_apis"
#    pdtk_post "sys_font: $sys_font"
#    pdtk_post "sys_fontweight: $sys_fontweight"
    set oldtclversion 0
    pdsend "pd init [enquote_path [pwd]] $oldtclversion $::font_fixed_metrics"
    set_pd_version $versionstring
    set ::audioapi_list $audio_apis
    set ::midiapi_list $midi_apis
    if {$::tcl_version >= 8.5} {find_default_font}
    set_base_font $sys_font $sys_fontweight
    fit_font_into_metrics
    # TODO what else is needed from the original?
    set ::wait4pd "started"
}

##### routine to ask user if OK and, if so, send a message on to Pd ######
# TODO add 'mytoplevel' once merged to 0.43, with -parent 
proc pdtk_check {message reply_to_pd default} {
    # TODO this should use -parent and -title, but the hard part is figuring
    # out how to get the values for those without changing g_editor.c
    set answer [tk_messageBox -type yesno -icon question -default $default \
                    -message [_ $message]]
    if {$answer eq "yes"} {
        pdsend $reply_to_pd
    }
}

proc pdtk_fixwindowmenu {} {
    # TODO canvas_updatewindowlist() sets up the menu_windowlist with all of
    # the parent CanvasWindows, we should then use [wm stackorder .] to get
    # the rest of the CanvasWindows to make sure that all CanvasWindows are in
    # the menu.  This would probably be better handled on the C side of
    # things, since then, the menu_windowlist could be built with the proper
    # parent/child relationships.
    # pdtk_post "Running pdtk_fixwindowmenu"
}

# ------------------------------------------------------------------------------
# X11 procs for handling singleton state and getting args from other instances

# first instance
proc singleton {key} {
    if {![catch { selection get -selection $key }]} {
        return 0
    }
    selection handle -selection $key . "singleton_request"
    selection own -command first_lost -selection $key .
    return 1
}

proc singleton_request {offset maxbytes} {
    wm deiconify .pdwindow
    raise .pdwindow
    return [tk appname]
}

proc first_lost {} {
    receive_args [selection get -selection PUREDATA]
    selection own -command first_lost -selection PUREDATA .
 }

# all other instances
proc send_args {offset maxChars} {
    return [string range $::argv $offset [expr {$offset+$maxChars}]]
}

proc others_lost {} {
    set ::singleton_state "exit"
    destroy .
    exit
}


# ------------------------------------------------------------------------------
# various startup related procs

proc check_for_running_instances {argc argv} {
    # pdtk_post "check_for_running_instances $argc $argv"
    switch -- $::windowingsystem {
        "aqua" {
            # handled by ::tk::mac::OpenDocument in apple_events.tcl
        } "x11" {
            # http://wiki.tcl.tk/1558
            if {![singleton PUREDATA_MANAGER]} {
                # other instances called by wish/pd-gui (exempt 'pd' by 5400 arg)
                if {$argc == 1 && [string is int $argv] && $argv >= 5400} {return}
                selection handle -selection PUREDATA . "send_args"
                selection own -command others_lost -selection PUREDATA .
                after 5000 set ::singleton_state "timeout"
                vwait ::singleton_state
                exit
            } else {
                # first instance
                selection own -command first_lost -selection PUREDATA .
            }
        } "win32" {
            ## http://wiki.tcl.tk/1558
            # TODO on Win: http://tcl.tk/man/tcl8.4/TclCmd/dde.htm
        }
    }
}

# this command will open files received from a 2nd instance of Pd
proc receive_args args {
    # pdtk_post "receive_files $args"
    raise .
    foreach filename $args {
        open_file $filename
    }
}

proc load_startup {} {
    global errorInfo
# TODO search all paths for startup.tcl
    set startupdir [file normalize "$::sys_libdir/startup"]
    # pdtk_post "load_startup $startupdir"
    puts stderr "load_startup $startupdir"
    if { ! [file isdirectory $startupdir]} { return }
    foreach filename [glob -directory $startupdir -nocomplain -types {f} -- *.tcl] {
        puts "Loading $filename"
        set tclfile [open $filename]
        set tclcode [read $tclfile]
        close $tclfile
        if {[catch {uplevel #0 $tclcode} errorname]} {
            puts stderr "------------------------------------------------------"
            puts stderr "UNHANDLED ERROR: $errorInfo"
            puts stderr "FAILED TO LOAD $filename"
            puts stderr "------------------------------------------------------"
        }
    }
}

# ------------------------------------------------------------------------------
# main
proc main {argc argv} {
    # TODO Tcl/Tk 8.3 doesn't have [tk windowingsystem]
    set ::windowingsystem [tk windowingsystem]
    tk appname pd-gui
    load_locale
    check_for_running_instances $argc $argv
    set_pd_paths
    init_for_platform
    # post_tclinfo

    # set a timeout for how long 'pd-gui' should wait for 'pd' to start
    after 20000 set ::wait4pd "timeout"        
    # TODO check args for -stderr and set pdtk_post accordingly
    if {$argc == 1 && [string is int $argv] && $argv >= 5400} {
        # 'pd' started first and launched us, so get the port to connect to
        ::pd_connect::to_pd [lindex $argv 0]
    } else {
        # the GUI is starting first, so create socket and exec 'pd'
        set portnumber [::pd_connect::create_socket]
        set pd_exec [file join [file dirname [info script]] ../bin/pd]
        exec -- $pd_exec -guiport $portnumber &
    }
    # wait for 'pd' to call pdtk_pd_startup, or exit on timeout
    vwait ::wait4pd
    if {$::wait4pd eq "timeout"} {
        puts stderr [_ "ERROR: 'pd' never showed up, 'pd-gui' quitting!"]
        exit 2
    }
    ::pd_bindings::class_bindings
    ::pd_menus::create_menubar
    ::pdtk_canvas::create_popup
    ::pdwindow::create_window
    ::pd_menus::configure_for_pdwindow
    load_startup
    # pdtk_post "------------------ done with main ----------------------"
}

main $::argc $::argv






