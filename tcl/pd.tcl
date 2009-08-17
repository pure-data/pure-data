#!/bin/sh
# This line continues for Tcl, but is a single line for 'sh' \
    exec wish "$0" -- ${1+"$@"}
# For information on usage and redistribution, and for a DISCLAIMER OF ALL
# WARRANTIES, see the file, "LICENSE.txt," in this distribution.
# Copyright (c) 1997-2009 Miller Puckette.

# puts -------------------------------pd.tcl-----------------------------------

package require Tcl 8.3
package require Tk
if {[tk windowingsystem] ne "win32"} {package require msgcat}

# Pd's packages are stored in the same directory as the main script (pd.tcl)
set auto_path [linsert $auto_path 0 [file dirname [info script]]]
package require pd_connect
package require pd_menus
package require pd_bindings
package require dialog_font
package require dialog_gatom
package require dialog_iemgui
package require pdtk_array
package require pdtk_canvas
package require pdtk_text
# TODO eliminate this kludge:
package require wheredoesthisgo

# import into the global namespace for backwards compatibility
namespace import ::pd_connect::pdsend
namespace import ::dialog_font::pdtk_canvas_dofont
namespace import ::dialog_gatom::pdtk_gatom_dialog
namespace import ::dialog_iemgui::pdtk_iemgui_dialog

#------------------------------------------------------------------------------#
# global variables

# for testing which platform we are running on ("aqua", "win32", or "x11")
set windowingsystem ""

# canvas font, received from pd in pdtk_pd_startup, set in s_main.c
set font_family "Courier"
set font_weight "bold"
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

# store list of parent windows for Window menu
set menu_windowlist {}

#------------------------------------------------------------------------------#
# coding style
#
# these are preliminary ideas, we'll change them as we work things out:
# - when possible use "" doublequotes to delimit messages
# - use '$::myvar' instead of 'global myvar' 
# - for the sake of clarity, there should not be any inline code, everything 
#   should be in a proc that is ultimately triggered from main()
# - if a menu_* proc opens a panel, that proc is called menu_*_panel
# - use "eq/ne" for string comparison, NOT "==/!="
#
## Names for Common Variables
#----------------------------
#
# variables named after the Tk widgets they represent
#   $mytoplevel = 'toplevel'
#   $mymenubar = the 'menu' attached to the 'toplevel'
#   $mymenu = 'menu' attached to the menubar 'menu'
#   $menuitem = 'menu' item
#   $mycanvas = 'canvas'
#   $canvasitem = 'canvas' item
#
#
## Prefix Names for procs
#----------------------------
# pdtk      pd -> pd-gui API (i.e. called from 'pd')
# pdsend    pd-gui -> pd API (sends a message to 'pd' using pdsend)
# canvas    manipulates a canvas
# text      manipulates a Tk 'text' widget

# ------------------------------------------------------------------------------
# init functions

proc init {} {
    # we are not using Tk scaling, so fix it to 1 on all platforms.  This
    # guarantees that patches will be pixel-exact on every platform
    tk scaling 1

    # TODO Tcl/Tk 8.3 doesn't have [tk windowingsystem]
    set ::windowingsystem [tk windowingsystem]
    # get the versions for later testing
    regexp {([0-9])\.([0-9])\.([0-9]+)} [info patchlevel] \
        wholematch ::tcl_major ::tcl_minor ::tcl_patch
    switch -- $::windowingsystem {
        "x11" {
            # add control to show/hide hidden files in the open panel (load
            # the tk_getOpenFile dialog once, otherwise it will not work)
            catch {tk_getOpenFile -with-invalid-argument} 
            set ::tk::dialog::file::showHiddenBtn 1
            set ::tk::dialog::file::showHiddenVar 0
            # set file types that open/save recognize
            set ::filetypes {
                {{pd files}         {.pd}  }
                {{max patch files}  {.pat} }
                {{max text files}   {.mxt} }
            }
        }
        "aqua" {
            # set file types that open/save recognize
            set ::filetypes {
                {{Pd Files}               {.pd}  }
                {{Max Patch Files (.pat)} {.pat} }
                {{Max Text Files (.mxt)}  {.mxt} }
            }
        }
        "win32" {
            font create menufont -family Tahoma -size -11
            # set file types that open/save recognize
            set ::filetypes {
                {{Pd Files}         {.pd}  }
                {{Max Patch Files}  {.pat} }
                {{Max Text Files}   {.mxt} }
            }
        }
    }
}

# official GNU gettext msgcat shortcut
if {[tk windowingsystem] ne "win32"} {
    proc _ {s} {return [::msgcat::mc $s]}
} else {
    proc _ {s} {return $s}
}

proc load_locale {} {
    ::msgcat::mcload [file join [file dirname [info script]] locale]

    # for Windows
    #set locale "en"  ;# Use whatever is right for your app
    #if {[catch {package require registry}]} {
    #       tk_messageBox -icon error -message "Could not get locale from registry"
    #} else {
    #   set locale [string tolower \
    #       [string range \
    #       [registry get {HKEY_CURRENT_USER\Control Panel\International} sLanguage] 0 1] ]
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
    return "pd_font_${size}"
}

proc set_base_font {family weight} {
    if {[lsearch -exact [font families] $family] > -1} {
        set ::font_family $family
    } else {
        puts stderr "Error: Font family \"$family\" not found, using default: $::font_family"
    }
    if {[lsearch -exact {bold normal} $weight] > -1} {
        set ::font_weight $weight
        set using_defaults 0
    } else {
        puts stderr "Error: Font weight \"$weight\" not found, using default: $::font_weight"
    }
    puts stderr "Using FONT $::font_family $::font_weight"
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
        puts "error: [lindex [info level 0] 0] failed to find a font of size $size fitting into a $width x $height cell! this system sucks"
        break
        }
    }
    if {$giveup} {continue}
    }
}


# ------------------------------------------------------------------------------
# procs called directly by pd

proc pdtk_pd_startup {version {args ""}} {
    # pdtk_post "pdtk_pd_startup $version $args"
    # pdtk_post "\tversion: $version"
    # pdtk_post "\targs: $args"
    set oldtclversion 0
    pdsend "pd init [enquote_path [pwd]] $oldtclversion $::font_fixed_metrics"
    set_base_font [lindex $args 2] [lindex $args 3]
    fit_font_into_metrics
    # TODO what else is needed from the original?
}

##### routine to ask user if OK and, if so, send a message on to Pd ######
proc pdtk_check {ignoredarg message reply_to_pd default} {
    # TODO this should use -parent and -title, but the hard part is figuring
    # out how to get the values for those without changing g_editor.c
    set answer [tk_messageBox -type yesno -icon question \
                    -default $default -message $message]
    if {$answer eq "yes"} {
        pdsend $reply_to_pd
    }
}

proc pdtk_fixwindowmenu {} {
    #TODO figure out how to do this cleanly
    puts stderr "Running pdtk_fixwindowmenu"
}

# ------------------------------------------------------------------------------
# procs called directly by pd

proc check_for_running_instances {} {
## http://tcl.tk/man/tcl8.4/TkCmd/send.htm
## This script fragment can be used to make an application that only  
## runs once on a particular display.
#
#if {[tk appname FoobarApp] ne "FoobarApp"} {
#   send -async FoobarApp RemoteStart $argv
#   exit
#}
## The command that will be called remotely, which raises
## the application main window and opens the requested files
#proc RemoteStart args {
#   raise .
#   foreach filename $args {
#       OpenFile $filename
#   }
#}
}

proc load_startup {} {
    global errorInfo
    set pd_guidir "[pwd]/../startup"
    # puts stderr "load_startup $pd_guidir"
    if { ! [file isdirectory $pd_guidir]} { return }
    foreach filename [glob -directory $pd_guidir -nocomplain -types {f} -- *.tcl] {
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
    catch {console show} ;# Not all platforms have the console command
    post_tclinfo
    pdtk_post "Starting pd.tcl with main($argc $argv)"
    check_for_running_instances
    if {[tk windowingsystem] ne "win32"} {load_locale}
    init

    # TODO check args for -stderr and set pdtk_post accordingly
    if { $argc == 1 && [string is int [lindex $argv 0]]} {
    # 'pd' started first and launched us, so get the port to connect to
        ::pd_connect::to_pd [lindex $argv 0]
    } else {
        # the GUI is starting first, so create socket and exec 'pd'
        set portnumber [::pd_connect::create_socket]
        set pd_exec [file join [file dirname [info script]] ../bin/pd]
        exec -- $pd_exec -guiport $portnumber &
        #TODO add vwait so that pd-gui will exit if pd never shows up
    }
    ::pd_bindings::class_bindings
    create_pdwindow
    load_startup
}

main $::argc $::argv






