
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
    namespace export pdtk_pd_meters
    namespace export pdtk_pd_dio
}

# TODO make the Pd window save its size and location between running

proc ::pdwindow::set_layout {} {
    #fatal
    .pdwindow.text tag configure verbosetext0 -background red
    #error
    .pdwindow.text tag configure verbosetext1 -background red
    #warning
    .pdwindow.text tag configure verbosetext2 -background orange
    #info
    .pdwindow.text tag configure verbosetext3

    #verbose
    for { set i 4 } { $i <= $::pdwindow::maxverbosity } { incr i } {
        set j [ expr 100 - int($i * 50 / $::pdwindow::maxverbosity ) ]
  
        .pdwindow.text tag configure verbosetext${i} -background grey${j}
    }
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
    puts "level $level     max $::verbose"
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
        .pdwindow.header.dsp configure -background green
    } else {
        set ::dsp 0
        .pdwindow.header.dsp configure -background grey
    }
}

proc ::pdwindow::pdtk_pd_dio {red} {
    if {$red == 1} {
        .pdwindow.header.dio configure -background red \
            -highlightbackground red
    } else {
        .pdwindow.header.dio configure -background lightgrey \
            -highlightbackground grey
    }
        
}

#--VU meter procedures---------------------------------------------------------#

proc ::pdwindow::calc_vu_color {position} {
    if {$position > 32} {
        return "#DF3A32"
    } elseif {$position > 30} {
        return "#EEAD54"
    } elseif {$position > 25} {
        return "#E9E448"
    } elseif {$position > 0} {
        return "#65DF37"
    } else {
        return "#6CEFF1"
    }
}

proc ::pdwindow::create_vu {tkcanvas name} {
    for {set i 0} {$i<40} {incr i} {
        set x [expr $i * 4]
        $tkcanvas create line $x 2 $x 18 -width 3 -tag "$name$i" \
            -fill [calc_vu_color $i] -state hidden
    }
}

proc ::pdwindow::set_vu {name db} {
    # TODO figure out the actual math here to properly map the incoming values
    set elements [expr ($db + 1) / 3]
    for {set i 0} {$i<$elements} {incr i} {
        .pdwindow.header.vu.$name itemconfigure "$name$i" -state normal
    }
    for {set i $elements} {$i<40} {incr i} {
        .pdwindow.header.vu.$name itemconfigure "$name$i" -state hidden
    }
}

proc ::pdwindow::pdtk_pd_meters {indb outdb inclip outclip} {
#    puts stderr [concat meters $indb $outdb $inclip $outclip]
    set_vu in $indb
    if {$inclip == 1} {
        .pdwindow.header.cliplabel.in configure -background red
    } else {
        .pdwindow.header.cliplabel.in configure -background grey
    }
    set_vu out $outdb
    if {$outclip == 1} {
        .pdwindow.header.cliplabel.out configure -background red
    } else {
        .pdwindow.header.cliplabel.out configure -background grey
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
    bind .pdwindow.text <Key-Tab> "focus .pdwindow.tcl.entry; break"

    # bindings for the Tcl entry widget
    bind .pdwindow.tcl.entry <$::modifier-Key-a> "%W selection range 0 end; break"
    bind .pdwindow.tcl.entry <Return> "::pdwindow::eval_tclentry"
    bind .pdwindow.tcl.entry <Up>     "::pdwindow::get_history 1"
    bind .pdwindow.tcl.entry <Down>   "::pdwindow::get_history -1"
    bind .pdwindow.tcl.entry <KeyRelease> +"::pdwindow::validate_tcl"

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
    wm title .pdwindow [_ "Pd window"]
    set ::windowname(.pdwindow) [_ "Pd window"]
    if {$::windowingsystem eq "x11"} {
        wm minsize .pdwindow 400 75
    } else {
        wm minsize .pdwindow 400 51
    }
    wm geometry .pdwindow =500x450+20+50
    .pdwindow configure -menu .menubar
    ::pd_menus::configure_for_pdwindow

    frame .pdwindow.header -borderwidth 1 -relief flat
    pack .pdwindow.header -side top -fill x -ipady 5
    checkbutton .pdwindow.header.meters -variable ::meters -takefocus 1 \
        -command {pdsend "pd meters $::meters"}
    pack .pdwindow.header.meters -side left -fill y -anchor w -pady 10
    frame .pdwindow.header.vu -borderwidth 0
    pack .pdwindow.header.vu -side left
    canvas .pdwindow.header.vu.in -width 150 -height 20 -background "#3F3F3F" \
        -highlightthickness 1 -highlightbackground grey
    create_vu .pdwindow.header.vu.in "in"
    canvas .pdwindow.header.vu.out -width 150 -height 20 -background "#3F3F3F" \
        -highlightthickness 1 -highlightbackground grey
    create_vu .pdwindow.header.vu.out "out"
    pack .pdwindow.header.vu.in -side top
    pack .pdwindow.header.vu.out -side top
    frame .pdwindow.header.cliplabel -borderwidth 0
    pack .pdwindow.header.cliplabel -side left
    label .pdwindow.header.cliplabel.in -text [_ "IN"] -width 4
    label .pdwindow.header.cliplabel.out -text [_ "OUT"] -width 4
    pack .pdwindow.header.cliplabel.in .pdwindow.header.cliplabel.out -side top
    checkbutton .pdwindow.header.dsp -text [_ "DSP"] -variable ::dsp \
        -font {$::font_family 18 bold} -takefocus 1 \
        -command {pdsend "pd dsp $::dsp"}
    pack .pdwindow.header.dsp -side right -fill y -anchor e -padx 5 -pady 10
# DIO button
    button .pdwindow.header.dio -text [_ "DIO"] -borderwidth 0 -width 4 \
        -highlightbackground grey -highlightthickness 0 -foreground grey \
        -takefocus 0 \
        -font {$::font_family 14} -command {pdsend "pd audiostatus"} 
    pack .pdwindow.header.dio -side right -fill y -padx 30 -pady 10
# Tcl entry box and log level frame
    frame .pdwindow.tcl -borderwidth 0
    pack .pdwindow.tcl -side bottom -fill x
    frame .pdwindow.tcl.pad
    pack .pdwindow.tcl.pad -side right -padx 12

    for { set i 0 } { $i <=  $::pdwindow::maxverbosity } { incr i} {lappend vlist $i }
    set logmenu [eval tk_optionMenu .pdwindow.tcl.logmenu ::verbose $vlist]

    set ::verbose  $::pdwindow::defaultverbosity


    # TODO figure out how to make the menu traversable with the keyboard
    #.pdwindow.tcl.logmenu configure -takefocus 1
    pack .pdwindow.tcl.logmenu -side right
    label .pdwindow.tcl.loglabel -text [_ "Log:"] -anchor e
    pack .pdwindow.tcl.loglabel -side right
    label .pdwindow.tcl.label -text [_ "Tcl:"] -anchor e
    pack .pdwindow.tcl.label -side left
    entry .pdwindow.tcl.entry -width 200 \
        -exportselection 1 -insertwidth 2 -insertbackground blue \
        -textvariable ::pdwindow::tclentry -font {$::font_family 12}
    pack .pdwindow.tcl.entry -side left -fill x
# TODO this should use the pd_font_$size created in pd-gui.tcl    
    text .pdwindow.text -relief raised -bd 2 -font {-size 10} \
        -highlightthickness 0 -borderwidth 1 -relief flat \
        -yscrollcommand ".pdwindow.scroll set" -width 60 \
        -undo true -autoseparators true -maxundo -1 -takefocus 0   
    scrollbar .pdwindow.scroll -command ".pdwindow.text yview"
    pack .pdwindow.scroll -side right -fill y
    pack .pdwindow.text -side right -fill both -expand 1
    raise .pdwindow
    focus .pdwindow.tcl.entry
    # run bindings last so that .pdwindow.tcl.entry exists
    pdwindow_bindings

    # print whatever is in the queue
    ::pdwindow::do_post {}    

    set ::loaded(.pdwindow) 1

    # set some layout variables
    ::pdwindow::set_layout

    # wait until .pdwindow.tcl.entry is visible before opening files so that
    # the loading logic can grab it and put up the busy cursor
    tkwait visibility .pdwindow.tcl.entry
}

