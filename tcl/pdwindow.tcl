
package provide pdwindow 0.1

namespace eval ::pdwindow:: {
    variable printout_buffer ""
    variable pdwindow_search_index
    variable tclentry {}
    variable tclentry_history {"console show"}
    variable history_position 0
    variable linecolor 0 ;# is toggled to alternate text line colors
    variable maxverbosity 5
    variable defaultverbosity 4

    variable lasttag "line0"

    namespace export create_window
    namespace export pdtk_post
    namespace export pdtk_pd_dsp
    namespace export pdtk_pd_dio
}

# TODO make the Pd window save its size and location between running

proc ::pdwindow::set_layout {} {
    #fatal
    .pdwindow.text tag configure verbosetext0
    #error
    .pdwindow.text tag configure verbosetext1
    #warning
    .pdwindow.text tag configure verbosetext2
    #info
    .pdwindow.text tag configure verbosetext3

    #verbose
    for { set i 4 } { $i <= $::pdwindow::maxverbosity } { incr i } {
        set j [ expr 100 - int($i * 50 / $::pdwindow::maxverbosity ) ]
  
        .pdwindow.text tag configure verbosetext${i} -background grey${j}
    }
}


# grab focus on part of the Pd window when Pd is busy
proc ::pdwindow::busygrab {} {
    # set the mouse cursor to look busy and grab focus so it stays that way    
    .pdwindow.text configure -cursor watch
    grab set .pdwindow.text
}

# release focus on part of the Pd window when Pd is finished
proc ::pdwindow::busyrelease {} {
    .pdwindow.text configure -cursor xterm
    grab release .pdwindow.text
}

# ------------------------------------------------------------------------------
# pdtk functions for 'pd' to send data to the Pd window

# if there is no console, this stores the current message in a buffer and returns true
# if there is a console, the buffer is sent to the console and "false" is returned

proc ::pdwindow::buffer_message { message } {
    variable printout_buffer
   if { ! [winfo exists .pdwindow.text]} {
        set printout_buffer "$printout_buffer\n$message"
       return true
    } else {
        if {$printout_buffer ne ""} {
            .pdwindow.text insert end "$printout_buffer\n"
            set printout_buffer ""
        }
    }
    return false
}

proc ::pdwindow::do_post {message {tag verbosetext3}} {
    if { ! [buffer_message $message ] } {
        variable lasttag
        .pdwindow.text insert end $message $tag
        .pdwindow.text yview end
        set lasttag $tag
    }
    # -stderr only sets $::stderr if 'pd-gui' is started before 'pd'
    if {$::stderr} {puts stderr $message}
}

proc ::pdwindow::post {level message} {
# log4X levels
#    0 fatal (not yet used)
#    1 error (error)
#    2 warning (bug)
#    3 info (post)
#    4... debug
    if { $level <= $::verbose } {
        do_post "$message" verbosetext$level
    }
}

proc ::pdwindow::endpost {} {
    variable linecolor
    do_post "\n" $::pdwindow::lasttag
    set linecolor [expr ! $linecolor]
}

# convenience functions
proc ::pdwindow::fatal {message} {
    post 0 "$message"
}
proc ::pdwindow::error {message} {
    post 1 "$message"
}
proc ::pdwindow::warn {message} {
    post 2 "$message"
}
proc ::pdwindow::info {message} {
    post 3 "$message"
}
proc ::pdwindow::verbose {level message} {
    incr level 4
    post $level "$message"
}


## for backwards compatibility
proc ::pdwindow::pdtk_post {message} {
    post 3 $message
}

# set the checkbox on the "Compute Audio" menuitem and checkbox
proc ::pdwindow::pdtk_pd_dsp {value} {
    # TODO canvas_startdsp/stopdsp should really send 1 or 0, not "ON" or "OFF"
    if {$value eq "ON"} {
        set ::dsp 1
    } else {
        set ::dsp 0
    }
}

proc ::pdwindow::pdtk_pd_dio {red} {
    if {$red == 1} {
        .pdwindow.header.dio configure -foreground red
    } else {
        .pdwindow.header.dio configure -foreground lightgrey
    }
        
}

#--bindings specific to the Pd window------------------------------------------#

proc ::pdwindow::pdwindow_bindings {} {
    # these bindings are for the whole Pd window, minus the Tcl entry
    foreach window {.pdwindow.text .pdwindow.header} {
        bind $window <$::modifier-Key-x> "tk_textCut .pdwindow.text"
        bind $window <$::modifier-Key-c> "tk_textCopy .pdwindow.text"
        bind $window <$::modifier-Key-v> "tk_textPaste .pdwindow.text"
    }
    # Select All doesn't seem to work unless its applied to the whole window
    bind .pdwindow <$::modifier-Key-a> ".pdwindow.text tag add sel 1.0 end"
    # the "; break" part stops executing another binds, like from the Text class

    # these don't do anything in the Pd window, so alert the user, then break
    # so no more bindings run
    bind .pdwindow <$::modifier-Key-s> "bell; break"
    bind .pdwindow <$::modifier-Shift-Key-S> "bell; break"
    bind .pdwindow <$::modifier-Key-p> "bell; break"

    # ways of hiding/closing the Pd window
    if {$::windowingsystem eq "aqua"} {
        # on Mac OS X, you can close the Pd window, since the menubar is there
        bind .pdwindow <$::modifier-Key-w>   "wm withdraw .pdwindow"
        wm protocol .pdwindow WM_DELETE_WINDOW "wm withdraw .pdwindow"
    } else {
        # TODO should it possible to close the Pd window and keep Pd open?
        bind .pdwindow <$::modifier-Key-w>   "wm iconify .pdwindow"
        wm protocol .pdwindow WM_DELETE_WINDOW "pdsend \"pd verifyquit\""
    }
}

#--Tcl entry procs-------------------------------------------------------------#

# copied from ::pd_connect::pd_readsocket, so perhaps it could be merged
proc ::pdwindow::eval_tclentry {} {
    variable tclentry
    variable tclentry_history
    variable history_position 0
    if {[catch {uplevel #0 $tclentry} errorname]} {
        global errorInfo
        switch -regexp -- $errorname { 
            "missing close-brace" {
                post 0 [concat [_ "(Tcl) MISSING CLOSE-BRACE '\}': "] $errorInfo]
            } "missing close-bracket" {
                post 0 [concat [_ "(Tcl) MISSING CLOSE-BRACKET '\]': "] $errorInfo]
            } "^invalid command name" {
                post 0 [concat [_ "(Tcl) INVALID COMMAND NAME: "] $errorInfo]
            } default {
                post 0 [concat [_ "(Tcl) UNHANDLED ERROR: "] $errorInfo]
            }
        }
    }    
    lappend tclentry_history $tclentry
    set tclentry {}
}

proc ::pdwindow::get_history {direction} {
    variable tclentry_history
    variable history_position

    incr history_position $direction
    if {$history_position < 0} {set history_position 0}
    if {$history_position > [llength $tclentry_history]} {
        set history_position [llength $tclentry_history]
    }
    .pdwindow.tcl.entry delete 0 end
    .pdwindow.tcl.entry insert 0 \
        [lindex $tclentry_history end-[expr $history_position - 1]]
}

proc ::pdwindow::validate_tcl {} {
    variable tclentry
    if {[::info complete $tclentry]} {
        .pdwindow.tcl.entry configure -background "white"
    } else {
        .pdwindow.tcl.entry configure -background "#FFF0F0"
    }
}

#--create tcl entry-----------------------------------------------------------#

proc ::pdwindow::create_tcl_entry {} {
# Tcl entry box frame
    label .pdwindow.tcl.label -text [_ "Tcl:"] -anchor e
    pack .pdwindow.tcl.label -side left
    entry .pdwindow.tcl.entry -width 200 \
       -exportselection 1 -insertwidth 2 -insertbackground blue \
       -textvariable ::pdwindow::tclentry -font {$::font_family 12}
    pack .pdwindow.tcl.entry -side left -fill x
# bindings for the Tcl entry widget
    bind .pdwindow.tcl.entry <$::modifier-Key-a> "%W selection range 0 end; break"
    bind .pdwindow.tcl.entry <Return> "::pdwindow::eval_tclentry"
    bind .pdwindow.tcl.entry <Up>     "::pdwindow::get_history 1"
    bind .pdwindow.tcl.entry <Down>   "::pdwindow::get_history -1"
    bind .pdwindow.tcl.entry <KeyRelease> +"::pdwindow::validate_tcl"

    bind .pdwindow.text <Key-Tab> "focus .pdwindow.tcl.entry; break"
}


#--create the window-----------------------------------------------------------#

proc ::pdwindow::create_window {} {
    set ::loaded(.pdwindow) 0

    # colorize by class before creating anything
    option add *PdWindow*Entry.highlightBackground "grey" startupFile
    option add *PdWindow*Frame.background "grey" startupFile
    option add *PdWindow*Label.background "grey" startupFile
    option add *PdWindow*Checkbutton.background "grey" startupFile
    option add *PdWindow*Menubutton.background "grey" startupFile
    option add *PdWindow*Text.background "white" startupFile
    option add *PdWindow*Entry.background "white" startupFile

    toplevel .pdwindow -class PdWindow
    wm title .pdwindow [_ "Pd"]
    set ::windowname(.pdwindow) [_ "Pd"]
    if {$::windowingsystem eq "x11"} {
        wm minsize .pdwindow 400 75
    } else {
        wm minsize .pdwindow 400 51
    }
    wm geometry .pdwindow =500x400+20+50
    .pdwindow configure -menu .menubar

    frame .pdwindow.header -borderwidth 1 -relief flat -background lightgray
    pack .pdwindow.header -side top -fill x -ipady 5

    frame .pdwindow.header.pad1
    pack .pdwindow.header.pad1 -side left -padx 12

    checkbutton .pdwindow.header.dsp -text [_ "DSP"] -variable ::dsp \
        -font {$::font_family 18 bold} -takefocus 1 -background lightgray \
        -borderwidth 0  -command {pdsend "pd dsp $::dsp"}
    pack .pdwindow.header.dsp -side right -fill y -anchor e -padx 5 -pady 0
# DIO button
    label .pdwindow.header.dio -text [_ "audio I/O error"] -borderwidth 0 \
        -background lightgrey -foreground lightgray \
        -takefocus 0 \
        -font {$::font_family 14}
    pack .pdwindow.header.dio -side right -fill y -padx 30 -pady 0

    label .pdwindow.header.loglabel -text [_ "Log:"] -anchor e \
        -background lightgray
    pack .pdwindow.header.loglabel -side left
    for { set i 0 } { $i <=  $::pdwindow::maxverbosity } { incr i}  {
        lappend vlist $i }
    set logmenu [eval tk_optionMenu .pdwindow.header.logmenu ::verbose $vlist]

    set ::verbose  $::pdwindow::defaultverbosity


    # TODO figure out how to make the menu traversable with the keyboard
    #.pdwindow.header.logmenu configure -takefocus 1
    pack .pdwindow.header.logmenu -side left
    frame .pdwindow.tcl -borderwidth 0
    pack .pdwindow.tcl -side bottom -fill x
# TODO this should use the pd_font_$size created in pd-gui.tcl    
    text .pdwindow.text -relief raised -bd 2 -font {-size 10} \
        -highlightthickness 0 -borderwidth 1 -relief flat \
        -yscrollcommand ".pdwindow.scroll set" -width 60 \
        -undo true -autoseparators true -maxundo -1 -takefocus 0   
    scrollbar .pdwindow.scroll -command ".pdwindow.text yview"
    pack .pdwindow.scroll -side right -fill y
    pack .pdwindow.text -side right -fill both -expand 1
    raise .pdwindow
    focus .pdwindow.text
    # run bindings last so that .pdwindow.tcl.entry exists
    pdwindow_bindings

    # print whatever is in the queue
    ::pdwindow::do_post {}    

    set ::loaded(.pdwindow) 1

    # set some layout variables
    ::pdwindow::set_layout

    # wait until .pdwindow.tcl.entry is visible before opening files so that
    # the loading logic can grab it and put up the busy cursor
    tkwait visibility .pdwindow.text
#    create_tcl_entry
}

