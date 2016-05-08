# the find dialog panel is a bit unusual in that it is created directly by the
# Tcl 'pd-gui'. Most dialog panels are created by sending a message to 'pd',
# which then sends a message to 'pd-gui' to create the panel.

package provide dialog_find 0.1

package require pd_bindings

namespace eval ::dialog_find:: {
    variable find_in_toplevel ".pdwindow"
    # store the state of the "Match whole word only" check box
    variable wholeword_button 0
    # if the search hasn't changed, then the Find button sends "findagain"
    variable previous_wholeword_button 0
    variable previous_findstring ""
    variable find_in_window ""
    variable window_changed 0
    variable find_history {}
    variable history_position 0

    namespace export pdtk_showfindresult
}

proc ::dialog_find::get_history {direction} {
    variable find_history
    variable history_position
    
    incr history_position $direction
    if {$history_position < 0} {set history_position 0}
    if {$history_position > [llength $find_history]} {
        set history_position [llength $find_history]
    }
    .find.entry delete 0 end
    .find.entry insert 0 [lindex $find_history end-[expr $history_position - 1]]
}

# mytoplevel isn't used here, but is kept for compatibility with other dialog ok procs
proc ::dialog_find::ok {mytoplevel} {
    variable find_in_window
    variable wholeword_button
    variable previous_wholeword_button
    variable previous_findstring
    variable window_changed
    variable find_history

    set findstring [.find.entry get]
    if {$findstring eq ""} {
        if {$::windowingsystem eq "aqua"} {bell}
        return
    }
    if {$find_in_window eq ".pdwindow"} {
        if {$::tcl_version < 8.5} {
            # TODO implement in 8.4 style, without -all
            set matches [.pdwindow.text search -nocase -- $findstring 0.0]
        } else {
            set matches [.pdwindow.text search -all -nocase -- $findstring 0.0]
        }
        .pdwindow.text tag delete sel
        if {[llength $matches] > 0} {
            foreach match $matches {
                .pdwindow.text tag add sel $match "$match wordend"
            }
            .pdwindow.text see [lindex $matches 0]
            lappend find_history $findstring
        }
    } else {
        if {$findstring eq $previous_findstring \
                && $wholeword_button == $previous_wholeword_button \
                    && !$window_changed} {
            pdsend "$find_in_window findagain"
        } else {
            pdsend [concat $find_in_window find [pdtk_encodedialog $findstring] \
                        $wholeword_button]
            set previous_findstring $findstring
            set previous_wholeword_button $wholeword_button
            lappend find_history $findstring
        }
        set window_changed 0
    }
    if {$::windowingsystem eq "aqua"} {
        # (Mac OS X) hide panel after success, but keep it if unsuccessful by
        # having the couldnotfind proc reopen it
        cancel $mytoplevel
    } else {
        # (GNOME/Windows) find panel should retain focus after a find
        # (yes, a bit of a kludge)
        after 100 "raise .find; focus .find.entry"
    }
}

# mytoplevel isn't used here, but is kept for compatibility with other dialog cancel procs
proc ::dialog_find::cancel {mytoplevel} {
    wm withdraw .find
}

proc ::dialog_find::set_window_to_search {mytoplevel} {
    variable find_in_window
    variable window_changed
    if {$find_in_window eq $mytoplevel} {
        set window_changed 0
    } else {
        set window_changed 1
    }
    set find_in_window $mytoplevel
    if {[winfo exists $find_in_window]} {
        if {$find_in_window eq ".find"} {
            set find_in_window [winfo toplevel [lindex [wm stackorder .] end-1]]
        }
            # this has funny side effects in tcl 8.4 ???
        if {$::tcl_version >= 8.5} {
            wm transient .find $find_in_window
        }
        .find.searchin configure -text \
            [concat [_ "Search in"] [lookup_windowname $find_in_window] \
                [_ "for:"] ]
            
    }
}

proc ::dialog_find::pdtk_showfindresult {mytoplevel success which total} {
    if {$success eq 0} {
        if {$::windowingsystem eq "aqua"} {bell}
        if {$total eq 0} {
            .find.searchin configure -text \
                [format [_ "Couldn't find '%s' in %s"] \
                [.find.entry get] [lookup_windowname $mytoplevel] ]
        } else {
            .find.searchin configure -text \
                [format [_ "Showed last '%s' in %s"] \
                [.find.entry get] [lookup_windowname $mytoplevel] ]
        }
    } else {
        .find.searchin configure -text \
        [format [_ "Showing '%d' out of %d items in %s"] \
            $which $total \
            [.find.entry get] [lookup_windowname $mytoplevel] ]
    }
    if {$::windowingsystem eq "aqua"} {open_find_dialog $mytoplevel}
}

# the find panel is opened from the menu and key bindings
proc ::dialog_find::open_find_dialog {mytoplevel} {
    if {[winfo exists .find]} {
        wm deiconify .find
        raise .find
        ::dialog_find::set_window_to_search $mytoplevel
    } else {
        create_dialog $mytoplevel
    }
    .find.entry selection range 0 end
}

proc ::dialog_find::create_dialog {mytoplevel} {
    toplevel .find -class DialogWindow
    wm title .find [_ "Find"]
    wm geometry .find =475x125+150+150
    wm group .find .
    wm resizable .find 0 0
    wm transient .find
    .find configure -menu $::dialog_menubar
    .find configure -padx 10 -pady 5
    ::pd_bindings::dialog_bindings .find "find"
    # sending these commands to the Find Dialog Panel should forward them to
    # the currently focused patch
    bind .find <$::modifier-Key-s> \
        {menu_send $::focused_window menusave; break}
    bind .find <$::modifier-Shift-Key-S> \
        {menu_send $::focused_window menusaveas; break}
    bind .find <$::modifier-Key-p> \
        {menu_print $::focused_window; break}
    
    label .find.searchin -text \
        [concat [_ "Search in"] [_ "Pd window"] [_ "for:"] ]
    pack .find.searchin -side top -fill x -pady 1

    entry .find.entry -width 54 -font 18 -relief sunken \
        -highlightthickness 1 -highlightcolor blue
    pack .find.entry -side top -padx 10

    bind .find.entry <Up> "::dialog_find::get_history 1"
    bind .find.entry <Down> "::dialog_find::get_history -1"
    
    checkbutton .find.wholeword -variable ::dialog_find::wholeword_button \
        -text [_ "Match whole word only"] -anchor w
    pack .find.wholeword -side top -padx 30 -pady 3 -fill x
    
    frame .find.buttonframe -background yellow
    pack .find.buttonframe -side right -pady 3
    if {$::windowingsystem eq "win32"} {
        button .find.cancel -text [_ "Cancel"] -default normal -width 9 \
            -command "::dialog_find::cancel $mytoplevel"
        pack .find.cancel -side right -padx 6 -pady 3
    }
    button .find.button -text [_ "Find"] -default active -width 9 \
        -command "::dialog_find::ok $mytoplevel"
    pack .find.button -side right -padx 6 -pady 3
    if {$::windowingsystem eq "x11"} {
        button .find.close -text [_ "Close"] -default normal -width 9 \
            -command "::dialog_find::cancel $mytoplevel"
        pack .find.close -side right -padx 6 -pady 3
    }
    # on Mac OS X, the buttons shouldn't get Tab/keyboard focus
    if {$::windowingsystem eq "aqua"} {
        .find.wholeword configure -takefocus 0
        .find.button configure -takefocus 0
    }
    ::dialog_find::set_window_to_search $mytoplevel
    focus .find.entry
}
