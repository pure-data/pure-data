#!/bin/sh
# This line continues for Tcl, but is a single line for 'sh' \
    exec wish "$0" -- ${1+"$@"}
# For information on usage and redistribution, and for a DISCLAIMER OF ALL
# WARRANTIES, see the file, "LICENSE.txt," in this distribution.
# Copyright (c) 1997-2009 Miller Puckette.

# "." automatically gets a window, we don't want it.  Withdraw it before doing
# anything else, so that we don't get the automatic window flashing for a
# second while pd loads.
if { [catch {wm withdraw .} fid] } { exit 2 }

# This is mainly for OSX as older versions only
# have 8.4 while newer versions have 8.5.
if { [catch {package provide Tcl 8.5}] } {
    # Tcl 8.5 not available
    package require Tcl 8.4
    package require Tk
} else {
    # Tcl 8.5 is available
    package require Tcl 8.5
    package require Tk

    # replace Tk widgets with Ttk widgets on 8.5
    namespace import -force ttk::*
}

package require msgcat
# TODO create a constructor in each package to create things at startup, that
#  way they can be easily be modified by startup scripts
# TODO create alt-Enter/Cmd-I binding to bring up Properties panels

# Pd's packages are stored in the same directory as the main script (pd-gui.tcl)
set auto_path [linsert $auto_path 0 [file dirname [info script]]]
package require pd_connect
package require pd_menus
package require pd_bindings
package require pdwindow
package require dialog_array
package require dialog_audio
package require dialog_canvas
package require dialog_data
package require dialog_font
package require dialog_gatom
package require dialog_iemgui
package require dialog_message
package require dialog_midi
package require dialog_path
package require dialog_startup
package require helpbrowser
package require pd_menucommands
package require opt_parser
package require pdtk_canvas
package require pdtk_text
package require pdtk_textwindow
package require pd_guiprefs
# TODO eliminate this kludge:
package require wheredoesthisgo

#------------------------------------------------------------------------------#
# import functions into the global namespace

# gui preferences
namespace import ::pd_guiprefs::init
namespace import ::pd_guiprefs::update_recentfiles
namespace import ::pd_guiprefs::write_recentfiles

# make global since they are used throughout
namespace import ::pd_menucommands::*

# import into the global namespace for backwards compatibility
namespace import ::pd_connect::pdsend
namespace import ::pdwindow::pdtk_post
namespace import ::pdwindow::pdtk_pd_dio
namespace import ::pdwindow::pdtk_pd_audio
namespace import ::pdwindow::pdtk_pd_dsp
namespace import ::pdwindow::pdtk_pd_meters
namespace import ::pdtk_canvas::pdtk_canvas_popup
namespace import ::pdtk_canvas::pdtk_canvas_editmode
namespace import ::pdtk_canvas::pdtk_canvas_getscroll
namespace import ::pdtk_canvas::pdtk_canvas_setparents
namespace import ::pdtk_canvas::pdtk_canvas_reflecttitle
namespace import ::pdtk_canvas::pdtk_canvas_menuclose
namespace import ::dialog_array::pdtk_array_dialog
namespace import ::dialog_audio::pdtk_audio_dialog
namespace import ::dialog_canvas::pdtk_canvas_dialog
namespace import ::dialog_data::pdtk_data_dialog
namespace import ::dialog_find::pdtk_showfindresult
namespace import ::dialog_font::pdtk_canvas_dofont
namespace import ::dialog_gatom::pdtk_gatom_dialog
namespace import ::dialog_iemgui::pdtk_iemgui_dialog
namespace import ::dialog_midi::pdtk_midi_dialog
namespace import ::dialog_midi::pdtk_alsa_midi_dialog
namespace import ::dialog_path::pdtk_path_dialog
namespace import ::dialog_startup::pdtk_startup_dialog

# hack - these should be better handled in the C code
namespace import ::dialog_array::pdtk_array_listview_new
namespace import ::dialog_array::pdtk_array_listview_fillpage
namespace import ::dialog_array::pdtk_array_listview_setpage
namespace import ::dialog_array::pdtk_array_listview_closeWindow

#------------------------------------------------------------------------------#
# global variables

# this is a wide array of global variables that are used throughout the GUI.
# they can be used in plugins to check the status of various things since they
# should all have been properly initialized by the time startup plugins are
# loaded.

set PD_MAJOR_VERSION 0
set PD_MINOR_VERSION 0
set PD_BUGFIX_VERSION 0
set PD_TEST_VERSION ""
set done_init 0

# for testing which platform we are running on ("aqua", "win32", or "x11")
set windowingsystem ""

# args about how much and where to log
set loglevel 2
set stderr 0

# connection between 'pd' and 'pd-gui'
set host ""
set port 0

# canvas font, received from pd in pdtk_pd_startup, set in s_main.c
set font_family "courier"
set font_weight "normal"
# sizes of chars for each of the Pd fixed font sizes:
# width(pixels)  height(pixels)
set font_metrics {
    5 11
    6 13
    7 16
    10 19
    14 29
    22 44
}

# sizes as above for zoomed-in view
set font_zoom2_metrics {
    10 22
    12 26
    14 32
    20 38
    28 58
    44 88
}
set font_measured {}
set font_zoom2_measured {}

# root path to lib of Pd's files, see s_main.c for more info
set sys_libdir {}
# root path where the pd-gui.tcl GUI script is located
set sys_guidir {}
# user-specified search paths for objects, help, fonts, etc.
set sys_searchpath {}
# user-specified search paths from the commandline -path option
set sys_temppath {}
# hard-coded search paths for objects, help, plugins, etc.
set sys_staticpath {}
# the path to the folder where the current plugin is being loaded from
set current_plugin_loadpath {}
# a list of plugins that were loaded
set loaded_plugins {}
# list of command line flags set at startup
set startup_flags {}
# list of libraries loaded on startup
set startup_libraries {}
# start dirs for new files and open panels
set filenewdir [pwd]
set fileopendir [pwd]

# lists of audio/midi devices and APIs for prefs dialogs
set audio_apilist {}
set audio_indevlist {}
set audio_outdevlist {}
set midi_apilist {}
set midi_indevlist {}
set midi_outdevlist {}
set pd_whichapi 0
set pd_whichmidiapi 0

# current state of the DSP
set dsp 0
# state of the peak meters in the Pd window
set meters 0
# the toplevel window that currently is on top and has focus
set focused_window .
# store that last 5 files that were opened
set recentfiles_list {}
set total_recentfiles 5
# keep track of the location of popup menu for PatchWindows, in canvas coords
set popup_xcanvas 0
set popup_ycanvas 0
# modifier for key commands (Ctrl/Control on most platforms, Cmd/Mod1 on MacOSX)
set modifier ""
# current state of the Edit Mode menu item
set editmode_button 0


## per toplevel/patch data
# window location modifiers
set menubarsize 0           ;# Mac OS X and other platforms have a menubar on top
set windowframex 0      ;# different platforms have different window frames
set windowframey 0      ;# different platforms have different window frames
# patch properties
array set editmode {}   ;# store editmode for each open patch canvas
array set editingtext {};# if an obj, msg, or comment is being edited, per patch
array set loaded {}     ;# store whether a patch has completed loading
array set xscrollable {};# keep track of whether the scrollbars are present
array set yscrollable {}
# patch window tree, these might contain patch IDs without a mapped toplevel
array set windowname {}    ;# window names based on mytoplevel IDs
array set childwindows {}  ;# all child windows based on mytoplevel IDs
array set parentwindows {} ;# topmost parent window ID based on mytoplevel IDs

# variables for holding the menubar to allow for configuration by plugins
set ::pdwindow_menubar ".menubar"
set ::patch_menubar   ".menubar"
set ::dialog_menubar   ""

# minimum size of the canvas window of a patch
set canvas_minwidth 50
set canvas_minheight 20

# undo states
array set undo_actions {}
array set redo_actions {}
# unused legacy undo states
set undo_action no
set redo_action no

namespace eval ::pdgui:: {
    variable scriptname [ file normalize [ info script ] ]
    variable pd_exec [ file normalize [file join [file dirname $scriptname] ../bin/pd] ]
}

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
#
## Names for Common Variables
#----------------------------
# variables named after the Tk widgets they represent
#   $window     = any kind of Tk widget that can be a Tk 'window'
#   $mytoplevel = a window id made by a 'toplevel' command
#   $gfxstub  = a 'toplevel' window id for dialogs made in gfxstub/x_gui.c
#   $menubar    = the 'menu' attached to each 'toplevel'
#   $mymenu     = 'menu' attached to the menubar, like the File menu
#   $tkcanvas   = a Tk 'canvas', which is the root of each patch
#
#
## Dialog Panel Types
#----------------------------
#   global (only one):   find, sendmessage, prefs, helpbrowser
#   per-canvas:          font, canvas properties (created with a message from pd)
#   per object:          gatom, iemgui, array, data structures (created with a message from pd)
#
#
## Prefix Names for procs
#----------------------------
# pdtk_     pd -> pd-gui API (i.e. called from 'pd')
# pdsend    pd-gui -> pd API (sends a message to 'pd' using pdsend)

# ------------------------------------------------------------------------------
# init functions

# root paths to find Pd's files where they are installed
proc set_pd_paths {} {
    set ::sys_guidir [file normalize [file dirname [info script]]]
    set ::sys_libdir [file normalize [file join $::sys_guidir ".."]]
}

proc init_for_platform {} {
    # we are not using Tk scaling, so fix it to 1 on all platforms.  This
    # guarantees that patches will be pixel-exact on every platform
    # 2013.07.19 msp - trying without this to see what breaks - it's having
    # deleterious effects on dialog window font sizes.
    # tk scaling 1

    switch -- $::windowingsystem {
        "x11" {
            set ::modifier "Control"
            option add *PatchWindow*Canvas.background "white" startupFile
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
            # some platforms have a menubar on the top, so place below them
            set ::menubarsize 0
            # Tk handles the window placement differently on each
            # platform. With X11, the x,y placement refers to the window
            # frame's upper left corner. http://wiki.tcl.tk/11502
            set ::windowframex 3
            set ::windowframey 53
            # trying loading icon in the GUI directory
            if {$::tcl_version >= 8.5} {
                set icon [file join $::sys_guidir pd.gif]
                if {[file readable $icon]} {
                    catch {
                        wm iconphoto . -default [image create photo -file "$icon"]
                    }
                }
            }
            # mouse cursors for all the different modes
            set ::cursor_runmode_nothing "left_ptr"
            set ::cursor_runmode_clickme "arrow"
            set ::cursor_runmode_thicken "sb_v_double_arrow"
            set ::cursor_runmode_addpoint "plus"
            set ::cursor_editmode_nothing "hand2"
            set ::cursor_editmode_connect "circle"
            set ::cursor_editmode_disconnect "X_cursor"
            set ::cursor_editmode_resize "sb_h_double_arrow"
        }
        "aqua" {
            # load tk::mac event callbacks here, this way launching pd
            # from the commandline incorporates the special mac event handling
            package require apple_events
            set ::modifier "Mod1"
            if {$::tcl_version < 8.5} {
                # old default font for Tk 8.4 on macOS
                # since font detection requires 8.5+
                set ::font_family "Monaco"
            } else {
                # hack until DVSM bug is fixed on macOS 10.15+
                set ::font_family "Menlo"
            }
            option add *DialogWindow*background "#E8E8E8" startupFile
            option add *DialogWindow*Entry.highlightBackground "#E8E8E8" startupFile
            option add *DialogWindow*Button.highlightBackground "#E8E8E8" startupFile
            option add *DialogWindow*Entry.background "white" startupFile
            option add *DialogWindow*Menu.foreground "black" startupFile
            # Mac OS X needs a menubar all the time
            set ::dialog_menubar ".menubar"
            # set file types that open/save recognize
            set ::filetypes \
                [list \
                     [list [_ "Associated Files"]       {.pd .pat .mxt} ] \
                     [list [_ "Pd Files"]               {.pd}  ] \
                     [list [_ "Max Patch Files (.pat)"] {.pat} ] \
                     [list [_ "Max Text Files (.mxt)"]  {.mxt} ] \
                ]
            # some platforms have a menubar on the top, so place below them
            set ::menubarsize 22
            # Tk handles the window placement differently on each platform, on
            # Mac OS X, the x,y placement refers to the content window's upper
            # left corner (not of the window frame) http://wiki.tcl.tk/11502
            set ::windowframex 0
            set ::windowframey 0
            # mouse cursors for all the different modes
            set ::cursor_runmode_nothing "arrow"
            set ::cursor_runmode_clickme "center_ptr"
            set ::cursor_runmode_thicken "sb_v_double_arrow"
            set ::cursor_runmode_addpoint "plus"
            set ::cursor_editmode_nothing "hand2"
            set ::cursor_editmode_connect "circle"
            set ::cursor_editmode_disconnect "X_cursor"
            set ::cursor_editmode_resize "sb_h_double_arrow"
        }
        "win32" {
            set ::modifier "Control"
            option add *PatchWindow*Canvas.background "white" startupFile
            # fix menu font size on Windows with tk scaling = 1
            font create menufont -family Tahoma -size -11
            option add *Menu.font menufont startupFile
            option add *HelpBrowser*font menufont startupFile
            option add *DialogWindow*font menufont startupFile
            option add *PdWindow*font menufont startupFile
            option add *ErrorDialog*font menufont startupFile
            # set file types that open/save recognize
            set ::filetypes \
                [list \
                     [list [_ "Associated Files"]  {.pd .pat .mxt} ] \
                     [list [_ "Pd Files"]          {.pd}  ] \
                     [list [_ "Max Patch Files"]   {.pat} ] \
                     [list [_ "Max Text Files"]    {.mxt} ] \
                    ]
            # some platforms have a menubar on the top, so place below them
            set ::menubarsize 0
            # Tk handles the window placement differently on each platform, on
            # Mac OS X, the x,y placement refers to the content window's upper
            # left corner. http://wiki.tcl.tk/11502
            # TODO this probably needs a script layer: http://wiki.tcl.tk/11291
            set ::windowframex 0
            set ::windowframey 0
            # TODO use 'winico' package for full, hicolor icon support
            wm iconbitmap . -default [file join $::sys_guidir pd.ico]
            # add local fonts to Tk's font list using pdfontloader
            if {[file exists [file join "$::sys_libdir" "font"]]} {
                catch {
                    load [file join "$::sys_libdir" "bin/pdfontloader.dll"]
                    set localfonts {"DejaVuSansMono.ttf" "DejaVuSansMono-Bold.ttf"}
                    foreach font $localfonts {
                        set path [file join "$::sys_libdir" "font/$font"]
                        pdfontloader::load $path
                        ::pdwindow::verbose 0 "pdfontloader loaded [file tail $path]\n"
                    }
                }
            }
            # mouse cursors for all the different modes
            set ::cursor_runmode_nothing "right_ptr"
            set ::cursor_runmode_clickme "arrow"
            set ::cursor_runmode_thicken "sb_v_double_arrow"
            set ::cursor_runmode_addpoint "plus"
            set ::cursor_editmode_nothing "hand2"
            set ::cursor_editmode_connect "circle"
            set ::cursor_editmode_disconnect "X_cursor"
            set ::cursor_editmode_resize "sb_h_double_arrow"
        }
    }
}

# ------------------------------------------------------------------------------
# locale handling

# official GNU gettext msgcat shortcut
proc _ {s} {return [::msgcat::mc $s]}

proc load_locale {} {
    # on any UNIX-like environment, Tcl should automatically use LANG, LC_ALL,
    # etc. otherwise we need to dig it up.  Mac OS X only uses LANG, etc. from
    # the Terminal, and Windows doesn't have LANG, etc unless you manually set
    # it up yourself.  Windows apps don't use the locale env vars usually.
    if {$::tcl_platform(os) eq "Darwin" && ! [info exists ::env(LANG)]} {
        # http://thread.gmane.org/gmane.comp.lang.tcl.mac/5215
        # http://thread.gmane.org/gmane.comp.lang.tcl.mac/6433
        if {![catch "exec defaults read com.apple.dock loc" lang]} {
            ::msgcat::mclocale $lang
        } elseif {![catch "exec defaults read NSGlobalDomain AppleLocale" lang]} {
            ::msgcat::mclocale $lang
        }
    } elseif {$::tcl_platform(platform) eq "windows"} {
        # using LANG on Windows is useful for easy debugging
        if {[info exists ::env(LANG)] && $::env(LANG) ne "C" && $::env(LANG) ne ""} {
            ::msgcat::mclocale $::env(LANG)
        } elseif {![catch {package require registry}]} {
            ::msgcat::mclocale [string tolower \
                                    [string range \
                                         [registry get {HKEY_CURRENT_USER\Control Panel\International} sLanguage] 0 1] ]
        }
    }
    ::msgcat::mcload [file join [file dirname [info script]] .. po]

    ##--moo: force default system and stdio encoding to UTF-8
    encoding system utf-8
    fconfigure stderr -encoding utf-8
    fconfigure stdout -encoding utf-8
    ##--/moo
}

# ------------------------------------------------------------------------------
# font handling

# this proc gets the internal font name associated with each size
proc get_font_for_size {fsize} {
    return [list $::font_family -$fsize $::font_weight]
}

# searches for a font to use as the default.  Tk automatically assigns a
# monospace font to the name "Courier" (see Tk 'font' docs), but it doesn't
# always do a good job of choosing in respect to Pd's needs.  So this chooses
# from a list of fonts that are known to work well with Pd.
proc find_default_font {} {
    set testfonts {
        "DejaVu Sans Mono" "Bitstream Vera Sans Mono" "Menlo" "Monaco" \
        "Inconsolata" "Courier 10 Pitch" "Andale Mono" "Droid Sans Mono"
    }
    foreach family $testfonts {
        if {[lsearch -exact -nocase [font families] $family] > -1} {
            set ::font_family $family
            break
        }
    }
    ::pdwindow::verbose 0 "detected font: $::font_family\n"
}

proc set_base_font {family weight} {
    if {[lsearch -exact [font families] $family] > -1} {
        set ::font_family $family
    } else {
        ::pdwindow::post [format \
            [_ "WARNING: font family '%s' not found, using default (%s)\n"] \
                $family $::font_family]
    }
    if {[lsearch -exact {bold normal} $weight] > -1} {
        set ::font_weight $weight
        set using_defaults 0
    } else {
        ::pdwindow::post [format \
            [_ "WARNING: font weight '%s' not found, using default (%s)\n"] \
                $weight $::font_weight]
    }
    ::pdwindow::verbose 0 "using font: $::font_family $::font_weight\n"
}

# finds sizes of the chosen font that just fit into the requried metrics
# e.g. if the metric requires the 'M' to be 15x10 pixels,
# and the given font at size 12 is 15x7 and at size 16 it is 19x10,
# then we would pick size 12.
# <wantmetrics> is a list of <w> <h> pairs for which we are seeking font-sizes.
# the proc returns a list of matching <size0> <width0> <height0> <size1> ...
proc fit_font_into_metrics {family weight wantmetrics} {
    set lastsize 0
    set lastwidth 0
    set lastheight 0
    set result {}
    if { [llength $wantmetrics] < 2} {
        return $result
    }
    for {set fsize 1} {$fsize < 120} {incr fsize} {
        set foo [list $family -$fsize $weight]
        set height [font metrics $foo -linespace]
        set width [font measure $foo M]
        if { $lastsize < 1 } {
            # just in case even the smallest font does not fit,
            # we just use it nevertheless
            set lastsize $fsize
            set lastwidth $width
            set lastheight $height
        }

        if { $width > [lindex $wantmetrics 0] || $height > [lindex $wantmetrics 1] } {
            # oops, this font is already too big; use the last one
            lappend result $lastsize $lastwidth $lastheight
            # and search for the next one
            set wantmetrics [lrange $wantmetrics 2 end]
            if { [llength $wantmetrics] < 2} {
                break
            }
        }
        set lastsize $fsize
        set lastwidth $width
        set lastheight $height
    }
    return $result
}

# ------------------------------------------------------------------------------
# procs called directly by pd

proc pdtk_pd_startup {major minor bugfix test
                      audio_apis midi_apis sys_font sys_fontweight} {
    set ::PD_MAJOR_VERSION $major
    set ::PD_MINOR_VERSION $minor
    set ::PD_BUGFIX_VERSION $bugfix
    set ::PD_TEST_VERSION $test
    set oldtclversion 0
    set ::audio_apilist $audio_apis
    set ::midi_apilist $midi_apis
    ::pdwindow::verbose 0 "Tk [info patchlevel]\n"
    if {$::tcl_version >= 8.5} {find_default_font}
    set_base_font $sys_font $sys_fontweight
    set ::font_measured [fit_font_into_metrics $::font_family $::font_weight $::font_metrics]
    set ::font_zoom2_measured [fit_font_into_metrics $::font_family $::font_weight $::font_zoom2_metrics]
    ::pd_guiprefs::init
    pdsend "pd init [enquote_path [pwd]] $oldtclversion \
        $::font_measured $::font_zoom2_measured"
    ::pd_bindings::class_bindings
    ::pd_bindings::global_bindings
    ::pd_menus::create_menubar
    ::pdwindow::create_window
    ::pdwindow::configure_menubar
    ::pd_menus::configure_for_pdwindow
    ::pdwindow::create_window_finalize
    ::pdtk_canvas::create_popup
    load_startup_plugins
    open_filestoopen
    set ::done_init 1
}

##### routine to ask user if OK and, if so return '1' (or else '0')
# (this really should be in some other file)
proc pdtk_yesnodialog {mytoplevel message default} {
    wm deiconify $mytoplevel
    raise $mytoplevel
    if {$::windowingsystem eq "win32"} {
           set answer [tk_messageBox -message [_ $message] -type yesno \
                                     -default $default -icon question \
                                     -title [wm title $mytoplevel]]
       } {
           set answer [tk_messageBox -message [_ $message] -type yesno \
                                     -default $default -icon question \
                                     -parent $mytoplevel]
       }
    if {$answer eq "yes"} {
           return 1
    }
    return 0
}
##### routine to ask user if OK and, if so, send a message on to Pd ######
proc pdtk_check {mytoplevel message reply_to_pd default} {
    if {[ pdtk_yesnodialog $mytoplevel $message $default ]} {
        pdsend $reply_to_pd
    }
}

# dispatch a message from running Pd patches to the intended plugin receiver
proc pdtk_plugin_dispatch { args } {
    set receiver [ lindex $args 0 ]
    if [ info exists ::pd_connect::plugin_dispatch_receivers($receiver) ] {
       foreach callback $::pd_connect::plugin_dispatch_receivers($receiver) {
               $callback [ lrange $args 1 end ]
       }
    }
}

# ------------------------------------------------------------------------------
# parse command line args when Wish/pd-gui.tcl is started first

proc parse_args {argc argv} {
    opt_parser::init {
        {-stderr    set {::stderr}}
        {-open      lappend {- ::filestoopen_list}}
    }
    set unflagged_files [opt_parser::get_options $argv]
    # if we have a single arg that is not a file, its a port or host:port combo
    if {$argc == 1 && ! [file exists [ lindex $argv 0 ]]} {
           set arg1 [ lindex $argv 0 ]
        if { [string is int $arg1] && $arg1 > 0} {
            # 'pd-gui' got the port number from 'pd'
            set ::host "localhost"
            set ::port $arg1
        } else {
            set hostport [split $arg1 ":"]
            set ::port [lindex $hostport 1]
            if { [string is int $::port] && $::port > 0} {
                set ::host [lindex $hostport 0]
            } else {
                set ::port 0
            }

        }
    } elseif {$unflagged_files ne ""} {
        foreach filename $unflagged_files {
            lappend ::filestoopen_list $filename
        }
    }
}

proc open_filestoopen {} {
    foreach filename $::filestoopen_list {
        open_file $filename
    }
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
## the next 2 lines raise the focus to the given window (and change desktop)
#    wm deiconify .pdwindow
#    raise .pdwindow
    return [tk appname]
}

proc first_lost {} {
    receive_args [selection get -selection ${::pdgui::scriptname} ]
    selection own -command first_lost -selection ${::pdgui::scriptname} .
 }

proc others_lost {} {
    set ::singleton_state "exit"
    destroy .
    exit
}

# all other instances
proc send_args {offset maxChars} {
    set sendargs {}
    foreach filename $::filestoopen_list {
        lappend sendargs [file normalize $filename]
    }
    return [string range $sendargs $offset [expr {$offset+$maxChars}]]
}

# this command will open files received from a 2nd instance of Pd
proc receive_args {filelist} {
    raise .
    wm deiconify .pdwindow
    raise .pdwindow
    foreach filename $filelist {
        open_file $filename
    }
}

proc dde_open_handler {cmd} {
    open_file [file normalize $cmd]
}

proc check_for_running_instances { } {
    # if pd-gui gets called from pd ('pd-gui 5400') or is told otherwise
    # to connect to a running instance of Pd (by providing [<host>:]<port>)
    # then we don't want to connect to a running instance
    if { $::port > 0 && $::host ne "" } { return }

    switch -- $::windowingsystem {
        "aqua" {
            # handled by ::tk::mac::OpenDocument in apple_events.tcl
        } "x11" {
            # http://wiki.tcl.tk/1558
            # TODO replace PUREDATA name with path so this code is a singleton
            # based on install location rather than this hard-coded name
            if {![singleton ${::pdgui::scriptname}_MANAGER ]} {
                selection handle -selection ${::pdgui::scriptname} . "send_args"
                selection own -command others_lost -selection ${::pdgui::scriptname} .
                after 5000 set ::singleton_state "timeout"
                vwait ::singleton_state
                exit
            } else {
                # first instance
                selection own -command first_lost -selection ${::pdgui::scriptname} .
            }
        } "win32" {
            ## http://wiki.tcl.tk/8940
            package require dde ;# 1.4 or later needed for full unicode support
            set topic "Pure_Data_DDE_Open ${::pdgui::scriptname}"
            # if no DDE service is running, start one and claim the name
            if { [dde services TclEval $topic] == {} } {
                # registers the interpreter as a DDE server with the service name 'TclEval' and the topic name specified by 'topic'
                dde servername -handler dde_open_handler $topic
            } else {
                # DDE is already running: use it to open the file with the running instance
                # we only open a single file (assuming that this is called by double-clicking)
                set filename [lindex ${::argv} 0]
                dde eval $topic $filename
                exit 0
            }
        }
    }
}


# ------------------------------------------------------------------------------
# load plugins on startup

proc load_plugin_script {filename} {
    global errorInfo

    set basename [file tail $filename]
    if {[lsearch $::loaded_plugins $basename] > -1} {
        ::pdwindow::post [ format [_ "'%1\$s' already loaded, ignoring: '%2\$s'"] $basename $filename]
        ::pdwindow::post "\n"
        return
    }

    ::pdwindow::debug [ format [_ "Loading plugin: %s"] $filename ]
    ::pdwindow::debug "\n"
    set tclfile [open $filename]
    set tclcode [read $tclfile]
    close $tclfile
    if {[catch {uplevel #0 $tclcode} errorname]} {
        ::pdwindow::error "-----------\n"
        ::pdwindow::error [ format [_ "UNHANDLED ERROR: %s"] $errorInfo ]
        ::pdwindow::error "\n"
        ::pdwindow::error [ format [_ "FAILED TO LOAD %s"] $filename ]
        ::pdwindow::error "\n-----------\n"
    } else {
        lappend ::loaded_plugins $basename
    }
}

proc load_startup_plugins {} {
    # load built-in plugins
    load_plugin_script [file join $::sys_guidir pd_deken.tcl]
    load_plugin_script [file join $::sys_guidir pd_docsdir.tcl]

    # load other installed plugins
    foreach pathdir [concat $::sys_temppath $::sys_searchpath $::sys_staticpath] {
        set dir [file normalize $pathdir]
        if { ! [file isdirectory $dir]} {continue}
        if { [ catch {
                   foreach filename [glob -directory $dir -nocomplain -types {f} -- \
                                          *-plugin/*-plugin.tcl *-plugin.tcl] {
                       set ::current_plugin_loadpath [file dirname $filename]
                       if { [ catch {
                                  load_plugin_script $filename
                              } ] } {
                              ::pdwindow::debug [ format [ _ "Failed to read plugin %s ...skipping!"] $filename ]
                              ::pdwindow::debug "\n"
                          }
                   }
               } ] } {
               # this is triggered in weird cases, e.g. when ~/Documents/Pd/externals is not readable,
               # but ~/Documents/Pd/ is...
               ::pdwindow::debug [ format [_ "Failed to find plugins in %s ...skipping!" ] $dir ]
               ::pdwindow::debug "\n"
           }
    }
}

# ------------------------------------------------------------------------------
# main
proc main {argc argv} {
    set ::windowingsystem [tk windowingsystem]
    set ::platform $::tcl_platform(os)
    if { $::tcl_platform(platform) eq "windows"} {
       set ::platform W32
    }

    tk appname pd-gui
    load_locale
    parse_args $argc $argv
    check_for_running_instances
    set_pd_paths
    init_for_platform

    # ::host and ::port are parsed from argv by parse_args
    if { $::port > 0 && $::host ne "" } {
        # 'pd' started first and launched us, so get the port to connect to
        ::pd_connect::to_pd $::port $::host
    } else {
        # the GUI is starting first, so create socket and exec 'pd'
        set ::port [::pd_connect::create_socket]
        exec -- ${::pdgui::pd_exec} -guiport $::port &
        # if 'pd-gui' first, then initial dir is home
        set ::filenewdir $::env(HOME)
        set ::fileopendir $::env(HOME)
    }
    ::pdwindow::verbose 0 "------------------ done with main ----------------------\n"
}

main $::argc $::argv
