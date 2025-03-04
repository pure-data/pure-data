
package provide pdwindow 0.1

package require pd_connect

namespace eval ::pdwindow:: {
    variable maxlogbuffer 21000 ;# if the logbuffer grows beyond this number, cut it
    variable keeplogbuffer 1000 ;# if the logbuffer gets automatically cut, keep this many elements
    variable logbuffer {}
    variable history_position 0
    variable linecolor 0 ;# is toggled to alternate text line colors
    variable logmenuitems
    variable maxloglevel 4
    variable font_size 12

    # private variables
    variable _lastlevel 0       ;# loglevel of last post (for automatic endpost level)
    variable _curlogbuffer 0    ;# number of \n currently in the logbuffer

    namespace export create_window
    namespace export pdtk_post
    namespace export pdtk_pd_dsp
    namespace export pdtk_pd_dio
    namespace export pdtk_pd_audio
}
array set ::pdwindow::missingobjects {}

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
    variable maxlogbuffer
    variable keeplogbuffer
    variable _curlogbuffer
    lappend logbuffer [list $object_id $level $message]
    set lfi 0
    while { [set lfi [string first "\n" $message $lfi]] >= 0 } {
        incr lfi
        incr _curlogbuffer
    }
    # what we are actually counting here is not the number of *lines* in the logbuffer,
    # but the number of buffer_messages, which is much higher
    # e.g. printing a 10 element list ([1 2 3 4 5 6 7 8 9 10( -> [print])
    # will add 22 messages (one prefix, one per atom, one per space-between-atoms, one LF)
    # LATER we could try to track "\n"
    # buffer-size limiting is only done if maxlogbuffer is > 0
    if {$maxlogbuffer > 0 && $_curlogbuffer > $maxlogbuffer} {
        # so we now have more lines (counting "\n") in the buffer than we actually want
        set keeplines ${keeplogbuffer}
        if {$keeplines > $maxlogbuffer} {set keeplines $maxlogbuffer}
        set count 0
        set keepitems 0
        # check how many elements we need to save to keep ${keeplines} lines
        foreach x [lreverse $logbuffer] {
            set x [lindex $x 2]
            set lfi 0
            while { [set lfi [string first "\n" $x $lfi] ] >= 0} { incr lfi
                incr count
            }
            if { $count >= $keeplines } {
                break
            }
            incr keepitems
        }
        set logbuffer [lrange $logbuffer end-$keepitems end]
        set msg [_ "dropped %d lines from the Pd window" [expr $_curlogbuffer - $count]]
        set _curlogbuffer 0
        ::pdwindow::verbose 10 "$msg\n"
        ::pdwindow::filter_logbuffer
    }
}

proc ::pdwindow::insert_log_line {object_id level message} {
    set win .pdwindow.text.internal
    set message [subst -nocommands -novariables $message]
    if {$object_id eq "" || $object_id eq "obj:(nil)"} {
        $win insert end $message log$level
    } else {
        set tag obj$object_id
        $win insert end $message [list log$level $tag]
        $win tag bind $tag <$::modifier-ButtonRelease-1> \
            "::pdwindow::select_by_id $object_id; break"
        $win tag bind $tag <Key-Return> \
            "::pdwindow::select_by_id $object_id; break"
        $win tag bind $tag <Key-KP_Enter> \
            "::pdwindow::select_by_id $object_id; break"
        $win tag bind $tag <Enter> "::pdwindow::set_findinstance_cursor %W Control_L 1"
        $win tag bind $tag <Leave> "::pdwindow::set_findinstance_cursor %W Control_L 0"
        if {$::windowingsystem eq "aqua"} {
            set rightbtn [expr {$::tcl_version < 9.0 ? 2 : 3}]
            set rightclicks [list <$rightbtn> <Control-Button-1>]
        } else {
            set rightclicks {<3>}
        }
        foreach event $rightclicks {
            $win tag bind $tag $event [list ::pdwindow::message_contextmenu %W %x %y $object_id]
        }
    }
}

proc ::pdwindow::filter_logbuffer {} {
    variable logbuffer
    variable maxloglevel
    .pdwindow.text.internal delete 0.0 end
    set i 0
    foreach logentry $logbuffer {
        foreach {object_id level message} $logentry {
            if { $level <= $::loglevel || $maxloglevel == $::loglevel} {
                insert_log_line $object_id $level $message
            }
        }
        # this could take a while, so update the GUI every 10000 lines
        if { [expr $i % 10000] == 0} {update idletasks}
        incr i
    }
    .pdwindow.text.internal yview end
    return $i
}
# this has 'args' to satisfy trace, but its not used
proc ::pdwindow::filter_buffer_to_text {args} {
    set i [::pdwindow::filter_logbuffer]
    set msg [_ "the Pd window filtered %d lines" $i ]
    ::pdwindow::verbose 10 "$msg\n"
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
    variable _lastlevel $level

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
    if {$::stderr} {puts -nonewline stderr $message}
}

# shortcuts for posting to the Pd window
proc ::pdwindow::fatal {message} {logpost {} 0 $message}
proc ::pdwindow::error {message} {logpost {} 1 $message}
proc ::pdwindow::post {message} {logpost {} 2 $message}
proc ::pdwindow::debug {message} {logpost {} 3 $message}
# for backwards compatibility
proc ::pdwindow::bug {message} {logpost {} 1 \
    [concat consistency check failed: $message]}
proc ::pdwindow::pdtk_post {message} {post $message}

proc ::pdwindow::endpost {} {
    variable linecolor
    variable _lastlevel
    logpost {} $_lastlevel "\n"
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
    variable _curlogbuffer 0
    .pdwindow.text.internal delete 0.0 end
}

# save the contents of the pdwindow::logbuffer to a file
proc ::pdwindow::save_logbuffer_to_file {} {
    variable logbuffer
    set filename [tk_getSaveFile -initialfile "pdwindow.txt" -defaultextension .txt -parent .pdwindow]
    if {$filename eq ""} return; # they clicked cancel
    set f [open $filename w]
    puts $f "Pd $::PD_MAJOR_VERSION.$::PD_MINOR_VERSION-$::PD_BUGFIX_VERSION$::PD_TEST_VERSION on $::tcl_platform(os) $::tcl_platform(machine)"
    puts $f "--------------------------------------------------------------------------------"
    foreach logentry $logbuffer {
        foreach {object_id level message} $logentry {
            puts -nonewline $f $message
        }
    }
    ::pdwindow::post "saved console to: $filename\n"
    close $f
}
# this has 'args' to satisfy trace, but its not used
proc ::pdwindow::loglevel_updated {args} {
    ::pdwindow::filter_buffer_to_text $args
    ::pd_guiprefs::write_loglevel
}

#--compute audio/DSP checkbutton-----------------------------------------------#

# set the checkbox on the "DSP" menuitems and checkbox
proc ::pdwindow::pdtk_pd_dsp {value} {
    # TODO canvas_startdsp/stopdsp should really send 1 or 0, not "ON" or "OFF"
    if {$value eq "ON"} {
        set ::dsp 1
    } else {
        set ::dsp 0
    }
}

proc ::pdwindow::pdtk_pd_dio {red} {
    set dio .pdwindow.header.ioframe.dio
    if {$red == 1} {
        $dio configure -foreground red
    } else {
        $dio configure -foreground [[winfo parent $dio] cget -background]
    }
}

proc ::pdwindow::pdtk_pd_audio {state} {
    # set strings so these can be translated
    # state values are "on" or "off"
    if {$state eq "on"} {
        set labeltext [_ "Audio on"]
    } elseif {$state eq "off"} {
        set labeltext [_ "Audio off"]
    } else {
        # fallback in case the $state values change in the future
        set labeltext [concat Audio $state]
    }
    .pdwindow.header.ioframe.iostate configure -text $labeltext
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
    bind .pdwindow <$::modifier-Key-s> {bell; break}
    bind .pdwindow <$::modifier-Key-p> {bell; break}

    # ways of hiding/closing the Pd window
    if {$::windowingsystem eq "aqua"} {
        # on Mac OS X, you can close the Pd window, since the menubar is there
        bind .pdwindow <$::modifier-Key-w>   "wm withdraw .pdwindow"
        wm protocol .pdwindow WM_DELETE_WINDOW "wm withdraw .pdwindow"
    } else {
        # TODO should it possible to close the Pd window and keep Pd open?
        bind .pdwindow <$::modifier-Key-w>   "wm iconify .pdwindow"
        wm protocol .pdwindow WM_DELETE_WINDOW "::pd_connect::menu_quit"
    }
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

proc ::pdwindow::add_missingobject {obj name} {
    set ::pdwindow::missingobjects($obj) $name
}


proc ::pdwindow::message_contextmenu {widget theX theY obj} {
    set m .pdwindow.message_contextmenu
    destroy $m
    menu $m
    $m add command -label [_ "Find source" ]  -command [list ::pdwindow::select_by_id $obj]
    if { [info exists ::pdwindow::missingobjects($obj)] } {
        set cmd [list ::deken::open_search_objects $::pdwindow::missingobjects($obj)]
        $m add command -label [_ "Find externals" ]  -command $cmd
    }

    tk_popup $m [expr [winfo rootx $widget] + $theX] [expr [winfo rooty $widget] + $theY]
}

#--create the window-----------------------------------------------------------#
proc ::pdwindow::update_title {w} {
    set title [_ "Pd" ]
    set version "${::PD_MAJOR_VERSION}.${::PD_MINOR_VERSION}.${::PD_BUGFIX_VERSION}${::PD_TEST_VERSION}"
    set fulltitle "${title} ${version}"
    if { [info exists ::deken::platform(floatsize)] } {
        switch -- ${::deken::platform(floatsize)} {
            32 { set floatsize "" }
            64 { set floatsize [_ "EXPERIMENTAL double (64bit) precision"] }
            default  { set floatsize [_ "%dbit-floats EXPERIMENTAL" ${::deken::platform(floatsize)}]}
        }
        if { ${floatsize} ne "" } {
            set fulltitle "${fulltitle} - ${floatsize}"
        }
    }

    if { [winfo exists ${w} ] } {
        wm title ${w} "${fulltitle}"
    }
    set ::windowname($w) $title

    return ${fulltitle}
}

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
    ::pdwindow::update_title .pdwindow

    if {$::windowingsystem eq "x11"} {
        wm minsize .pdwindow 400 75
    } else {
        wm minsize .pdwindow 400 51
    }
    wm geometry .pdwindow =500x400

    frame .pdwindow.header -borderwidth 1 -relief flat -background lightgray
    pack .pdwindow.header -side top -fill x -ipady 5

    frame .pdwindow.header.pad1
    pack .pdwindow.header.pad1 -side left -padx 12

    checkbutton .pdwindow.header.dsp -text [_ "DSP"] -variable ::dsp \
        -takefocus 1 -background lightgray \
        -borderwidth 0  -command {pdsend "pd dsp $::dsp"}
    pack .pdwindow.header.dsp -side right -fill y -anchor e -padx 5 -pady 0

# frame for DIO error and audio in/out labels
    frame .pdwindow.header.ioframe -background lightgray
    pack .pdwindow.header.ioframe -side right -padx 30

# I/O state label (shows I/O on/off/in-only/out-only)
    label .pdwindow.header.ioframe.iostate \
        -text [_ "Audio off"] -borderwidth 1 \
        -background lightgray -foreground black \
        -takefocus 0

# DIO error label
    label .pdwindow.header.ioframe.dio \
        -text [_ "Audio I/O error"] -borderwidth 1 \
        -background lightgray -foreground lightgray \
        -takefocus 0

    pack .pdwindow.header.ioframe.iostate .pdwindow.header.ioframe.dio \
        -side top

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
    trace add variable ::loglevel write ::pdwindow::loglevel_updated

    # TODO figure out how to make the menu traversable with the keyboard
    #.pdwindow.header.logmenu configure -takefocus 1
    pack .pdwindow.header.logmenu -side left
    text .pdwindow.text -relief raised -bd 2 -font [list $::font_family $::pdwindow::font_size] \
        -highlightthickness 0 -borderwidth 1 -relief flat \
        -yscrollcommand ".pdwindow.scroll set" -width 80 \
        -undo false -autoseparators false -maxundo 1 -takefocus 0
    scrollbar .pdwindow.scroll -command ".pdwindow.text.internal yview"
    pack .pdwindow.scroll -side right -fill y
    pack .pdwindow.text -side right -fill both -expand 1
    raise .pdwindow
    focus .pdwindow.text
    pdwindow_bindings

    # hack to make a good read-only text widget from http://wiki.tcl.tk/1152
    rename ::.pdwindow.text ::.pdwindow.text.internal
    proc ::.pdwindow.text {args} {
        switch -exact -- [lindex $args 0] {
            "insert" {}
            "delete" {}
            "default" { return [eval ::.pdwindow.text.internal $args] }
        }
    }

    # print whatever is in the queue after the event loop finishes
    after idle [list after 0 ::pdwindow::filter_buffer_to_text]

    set ::loaded(.pdwindow) 1

    # set some layout variables
    ::pdwindow::set_layout
}

#--configure the window menu---------------------------------------------------#

proc ::pdwindow::create_window_finalize {} {
    # this ought to be called after all elements of the window (including the
    # menubar!) have been created!
    if {![winfo viewable .pdwindow.text]} { tkwait visibility .pdwindow.text }
    set fontsize [::pd_guiprefs::read menu-fontsize]
    if {$fontsize != ""} {
        ::dialog_font::apply .pdwindow $fontsize
    }
}

# this needs to happen *after* the main menu is created, otherwise the default Wish
# menu is not replaced by the custom Apple menu on OSX
proc ::pdwindow::configure_menubar {} {
    .pdwindow configure -menu .menubar
}
