
package provide pdwindow 0.1

namespace eval ::pdwindow:: {
    variable logbuffer {}
    variable tclentry {}
    variable tclentry_history {"console show"}
    variable history_position 0
    variable linecolor 0 ;# is toggled to alternate text line colors
    variable logmenuitems
    variable maxloglevel 4

    variable lastlevel 0

    namespace export create_window
    namespace export pdtk_post
    namespace export pdtk_pd_dsp
    namespace export pdtk_pd_dio
}

# TODO make the Pd window save its size and location between running

proc ::pdwindow::set_layout {} {
    variable maxloglevel
    .pdwindow.text.internal tag configure log0 -foreground "#d00" -background "#ffe0e8"
    .pdwindow.text.internal tag configure log1 -foreground "#d00"
    # log2 messages are normal black on white
    .pdwindow.text.internal tag configure log3 -foreground "#484848"

    # 0-20(4-24) is a rough useful range of 'verbose' levels for impl debugging
    set start 4
    set end 25
    for {set i $start} {$i < $end} {incr i} {
        set B [expr int(($i - $start) * (40 / ($end - $start))) + 50]
        .pdwindow.text.internal tag configure log${i} -foreground grey${B}
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

proc ::pdwindow::buffer_message {object_id level message} {
    variable logbuffer
    lappend logbuffer $object_id $level $message
}

proc ::pdwindow::insert_log_line {object_id level message} {
    if {$object_id eq ""} {
        .pdwindow.text.internal insert end $message log$level
    } else {
        .pdwindow.text.internal insert end $message [list log$level obj$object_id]
        .pdwindow.text.internal tag bind obj$object_id <$::modifier-ButtonRelease-1> \
            "::pdwindow::select_by_id $object_id; break"
        .pdwindow.text.internal tag bind obj$object_id <Key-Return> \
            "::pdwindow::select_by_id $object_id; break"
        .pdwindow.text.internal tag bind obj$object_id <Key-KP_Enter> \
            "::pdwindow::select_by_id $object_id; break"
    }
}

# this has 'args' to satisfy trace, but its not used
proc ::pdwindow::filter_buffer_to_text {args} {
    variable logbuffer
    variable maxloglevel
    .pdwindow.text.internal delete 0.0 end
    set i 0
    foreach {object_id level message} $logbuffer {
        if { $level <= $::loglevel || $maxloglevel == $::loglevel} {
            insert_log_line $object_id $level $message
        }
        # this could take a while, so update the GUI every 10000 lines
        if { [expr $i % 10000] == 0} {update idletasks}
        incr i
    }
    .pdwindow.text.internal yview end
    ::pdwindow::verbose 10 "The Pd window filtered $i lines\n"
}

proc ::pdwindow::select_by_id {args} {
    if [llength $args] { # Is $args empty?
        pdsend "pd findinstance $args"
    }
}

# logpost posts to Pd window with an object to trace back to and a
# 'log level'. The logpost and related procs are for generating
# messages that are useful for debugging patches.  They are messages
# that are meant for the Pd programmer to see so that they can get
# information about the patches they are building
proc ::pdwindow::logpost {object_id level message} {
    variable maxloglevel
    variable lastlevel $level

    buffer_message $object_id $level $message
    if {[llength [info commands .pdwindow.text.internal]] && 
        ($level <= $::loglevel || $maxloglevel == $::loglevel)} {
        # cancel any pending move of the scrollbar, and schedule it
        # after writing a line. This way the scrollbar is only moved once
        # when the inserting has finished, greatly speeding things up
        after cancel .pdwindow.text.internal yview end
        insert_log_line $object_id $level $message
        after idle .pdwindow.text.internal yview end
    }
    # -stderr only sets $::stderr if 'pd-gui' is started before 'pd'
    if {$::stderr} {puts stderr $message}
}

# shortcuts for posting to the Pd window
proc ::pdwindow::fatal {message} {logpost {} 0 $message}
proc ::pdwindow::error {message} {logpost {} 1 $message}
proc ::pdwindow::post {message} {logpost {} 2 $message}
proc ::pdwindow::debug {message} {logpost {} 3 $message}
# for backwards compatibility
proc ::pdwindow::bug {message} {logpost {} 3 $message}
proc ::pdwindow::pdtk_post {message} {post $message}

proc ::pdwindow::endpost {} {
    variable linecolor
    variable lastlevel
    logpost {} $lastlevel "\n"
    set linecolor [expr ! $linecolor]
}

# this verbose proc has a separate numbering scheme since its for
# debugging implementations, and therefore falls outside of the 0-3
# numbering on the Pd window.  They should only be shown in ALL mode.
proc ::pdwindow::verbose {level message} {
    incr level 4
    logpost {} $level $message
}

# clear the log and the buffer
proc ::pdwindow::clear_console {} {
    variable logbuffer {}
    .pdwindow.text.internal delete 0.0 end
}

#--compute audio/DSP checkbutton-----------------------------------------------#

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
        .pdwindow.header.dio configure -foreground lightgray
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
    if {$tclentry eq ""} {return} ;# no need to do anything if empty
    if {[catch {uplevel #0 $tclentry} errorname]} {
        global errorInfo
        switch -regexp -- $errorname { 
            "missing close-brace" {
                ::pdwindow::error [concat [_ "(Tcl) MISSING CLOSE-BRACE '\}': "] $errorInfo]\n
            } "missing close-bracket" {
                ::pdwindow::error [concat [_ "(Tcl) MISSING CLOSE-BRACKET '\]': "] $errorInfo]\n
            } "^invalid command name" {
                ::pdwindow::error [concat [_ "(Tcl) INVALID COMMAND NAME: "] $errorInfo]\n
            } default {
                ::pdwindow::error [concat [_ "(Tcl) UNHANDLED ERROR: "] $errorInfo]\n
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
    if {[info complete $tclentry]} {
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

proc ::pdwindow::set_findinstance_cursor {widget key state} {
    set triggerkeys [list Control_L Control_R Meta_L Meta_R]
    if {[lsearch -exact $triggerkeys $key] > -1} {
        if {$state == 0} {
            $widget configure -cursor xterm
        } else {
            $widget configure -cursor based_arrow_up
        }
    }
}

#--create the window-----------------------------------------------------------#

proc ::pdwindow::create_window {} {
    variable logmenuitems
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
        -background lightgray -foreground lightgray \
        -takefocus 0 \
        -font {$::font_family 14}
    pack .pdwindow.header.dio -side right -fill y -padx 30 -pady 0

    label .pdwindow.header.loglabel -text [_ "Log:"] -anchor e \
        -background lightgray
    pack .pdwindow.header.loglabel -side left

    set loglevels {0 1 2 3 4}
    lappend logmenuitems "0 [_ fatal]"
    lappend logmenuitems "1 [_ error]"
    lappend logmenuitems "2 [_ normal]"
    lappend logmenuitems "3 [_ debug]"
    lappend logmenuitems "4 [_ all]"
    set logmenu \
        [eval tk_optionMenu .pdwindow.header.logmenu ::loglevel $loglevels]
    .pdwindow.header.logmenu configure -background lightgray
    foreach i $loglevels {
        $logmenu entryconfigure $i -label [lindex $logmenuitems $i]
    }
    trace add variable ::loglevel write ::pdwindow::filter_buffer_to_text

    # TODO figure out how to make the menu traversable with the keyboard
    #.pdwindow.header.logmenu configure -takefocus 1
    pack .pdwindow.header.logmenu -side left
    frame .pdwindow.tcl -borderwidth 0
    pack .pdwindow.tcl -side bottom -fill x
# TODO this should use the pd_font_$size created in pd-gui.tcl    
    text .pdwindow.text -relief raised -bd 2 -font {-size 10} \
        -highlightthickness 0 -borderwidth 1 -relief flat \
        -yscrollcommand ".pdwindow.scroll set" -width 60 \
        -undo false -autoseparators false -maxundo 1 -takefocus 0
    scrollbar .pdwindow.scroll -command ".pdwindow.text.internal yview"
    pack .pdwindow.scroll -side right -fill y
    pack .pdwindow.text -side right -fill both -expand 1
    raise .pdwindow
    focus .pdwindow.text
    # run bindings last so that .pdwindow.tcl.entry exists
    pdwindow_bindings
    # set cursor to show when clicking in 'findinstance' mode
    bind .pdwindow <KeyPress> "+::pdwindow::set_findinstance_cursor %W %K %s"
    bind .pdwindow <KeyRelease> "+::pdwindow::set_findinstance_cursor %W %K %s"

    # hack to make a good read-only text widget from http://wiki.tcl.tk/1152
    rename ::.pdwindow.text ::.pdwindow.text.internal
    proc ::.pdwindow.text {args} {
        switch -exact -- [lindex $args 0] {
            "insert" {}
            "delete" {}
            "default" { return [eval ::.pdwindow.text.internal $args] }
        }
    }
    
    # print whatever is in the queue
    filter_buffer_to_text

    set ::loaded(.pdwindow) 1

    # set some layout variables
    ::pdwindow::set_layout

    # wait until .pdwindow.tcl.entry is visible before opening files so that
    # the loading logic can grab it and put up the busy cursor
    tkwait visibility .pdwindow.text
#    create_tcl_entry
}
