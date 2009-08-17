
package provide dialog_find 0.1

package require pd_bindings

namespace eval ::dialog_find:: {
    namespace export menu_dialog_find
}

# TODO figure out findagain
# TODO make targetlabel into a popup menu
# TODO make panel go away after a find

proc find_ok {mytoplevel} {::dialog_find::ok $mytoplevel} ;# TODO temp kludge
proc ::dialog_find::ok {mytoplevel} {
    # find will be on top, so use the previous window that was on top
    set search_window [lindex [wm stackorder .] end-1]
    if {$search_window eq "."} {
        puts "search pd window not implemented yet"
    } else {
        puts "search_window $search_window"
        set find_string [.find.entry get]
        if {$find_string ne ""} {
            pdsend "$search_window find $find_string"
        }
    }
}

proc find_cancel {mytoplevel} {::dialog_find::cancel $mytoplevel} ;# TODO temp kludge
proc ::dialog_find::cancel {mytoplevel} {
    wm withdraw .find
}

proc ::dialog_find::set_canvas_to_search {mytoplevel} {
    if {[winfo exists .find.frame.targetlabel]} {
        set focusedtoplevel [winfo toplevel [lindex [wm stackorder .] end]]
        if {$focusedtoplevel eq ".find"} {
            set focusedtoplevel [winfo toplevel [lindex [wm stackorder .] end-1]]
        }
        # TODO this text should be based on $::menu_windowlist
        if {$focusedtoplevel eq "."} {  
            .find.frame.targetlabel configure -text [wm title .]
        } else {
            foreach window $::menu_windowlist {
                if {[lindex $window 1] eq $focusedtoplevel} {
                    .find.frame.targetlabel configure -text [lindex $window 0]
                }
            }
        }
    }
}

# the find panel is opened from the menu and key bindings
proc ::dialog_find::menu_dialog_find {mytoplevel} {
    if {[winfo exists .find]} {
        wm deiconify .find
        raise .find
    } else {
        create_panel $mytoplevel
    }
}

proc ::dialog_find::create_panel {mytoplevel} {
    toplevel .find
    wm title .find [_ "Find"]
    wm geometry .find =475x125+150+150
    wm resizable .find 0 0
    if {[catch {wm attributes .find -topmost}]} {puts stderr ".find -topmost failed"}
    .find configure
    ::pd_bindings::panel_bindings .find "find"
    
    frame .find.frame
    pack .find.frame -side top -fill x -pady 7
    label .find.frame.searchin -text [_ "Search in"]
    label .find.frame.targetlabel -font "TkTextFont 14" 
    label .find.frame.for -text [_ "for:"]
    pack .find.frame.searchin .find.frame.targetlabel .find.frame.for -side left
    entry .find.entry -width 54 -font 18 -relief sunken \
        -highlightthickness 3 -highlightcolor blue
    focus .find.entry
    pack .find.entry -side top -padx 10
    
    frame .find.buttonframe -background yellow
    button .find.button -text [_ "Find"] -default active -width 9 \
        -command "::dialog_find::ok $mytoplevel"
    if {$::windowingsystem eq "x11"} {
        button .find.close -text [_ "Close"] -default normal -width 9 \
            -command "::dialog_find::cancel $mytoplevel"
        pack .find.buttonframe .find.button .find.close -side right -padx 10 -pady 15
    } else {
        pack .find.buttonframe .find.button -side right -padx 10 -pady 15
    }
    ::dialog_find::set_canvas_to_search $mytoplevel
}
