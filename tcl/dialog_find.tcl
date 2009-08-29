
package provide dialog_find 0.1

package require pd_bindings

namespace eval ::dialog_find:: {
    # store the state of the "Match whole word only" check box
    variable wholeword_button 0
    # if the search hasn't changed, then the Find button sends "findagain"
    variable previous_wholeword_button 0
    variable previous_findstring ""

    namespace export menu_dialog_find
}

# TODO make find panel as small as possible, being topmost means its findable
# TODO (GNOME/Windows) find panel should retain focus after a find
# TODO (Mac OS X) hide panel after success, but stay if the find was unsuccessful

proc ::dialog_find::ok {mytoplevel} {
    variable wholeword_button
    variable previous_wholeword_button
    variable previous_findstring
    # find will be on top, so use the previous window that was on top
    set search_window [lindex [wm stackorder .] end-1]
    puts "search_window $search_window"
    set findstring [.find.entry get]
    if {$findstring eq ""} {return}
    if {$search_window eq ".pdwindow"} {
        set matches [.pdwindow.text search -all -nocase -- $findstring 0.0]
        .pdwindow.text tag delete sel
        foreach match $matches {
            .pdwindow.text tag add sel $match "$match wordend"
        }
        .pdwindow.text see [lindex $matches 0]
    } else {
        if {$findstring eq $previous_findstring \
                && $wholeword_button == $previous_wholeword_button} {
            pdsend "$search_window findagain"
        } else {
            # TODO switch back to this for 0.43:
            #pdsend "$search_window find $findstring $wholeword_button"
            pdsend "$search_window find $findstring"
            set previous_findstring $findstring
            set previous_wholeword_button $wholeword_button
        }
    }
}

proc ::dialog_find::cancel {mytoplevel} {
    wm withdraw .find
}

proc ::dialog_find::set_canvas_to_search {mytoplevel} {
    # TODO rewrite using global $::focused_window
    if {[winfo exists .find.frame.targetlabel]} {
        set focusedtoplevel [winfo toplevel [lindex [wm stackorder .] end]]
        if {$focusedtoplevel eq ".find"} {
            set focusedtoplevel [winfo toplevel [lindex [wm stackorder .] end-1]]
        }
        if {$focusedtoplevel eq ".pdwindow"} {    
            .find.frame.targetlabel configure -text [wm title .pdwindow]
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
proc ::dialog_find::menu_find_dialog {mytoplevel} {
    if {[winfo exists .find]} {
        wm deiconify .find
        raise .find
    } else {
        create_dialog $mytoplevel
    }
}

proc ::dialog_find::create_dialog {mytoplevel} {
    toplevel .find -class DialogWindow
    wm title .find [_ "Find"]
    wm geometry .find =475x125+150+150
    .find configure
    if {$::windowingsystem eq "aqua"} {$mytoplevel configure -menu .menubar}
    ::pd_bindings::dialog_bindings .find "find"
    
    frame .find.frame
    pack .find.frame -side top -fill x -pady 1
    label .find.frame.searchin -text [_ "Search in"]
    label .find.frame.targetlabel -font "TkTextFont 14" 
    label .find.frame.for -text [_ "for:"]
    pack .find.frame.searchin .find.frame.targetlabel .find.frame.for -side left
    entry .find.entry -width 54 -font 18 -relief sunken \
        -highlightthickness 3 -highlightcolor blue
    focus .find.entry
    pack .find.entry -side top -padx 10
    
    checkbutton .find.wholeword -variable ::dialog_find::wholeword_button \
        -text [_ "Match whole word only"] -anchor w
    pack .find.wholeword -side top -padx 30 -pady 3 -fill x
    
    frame .find.buttonframe -background yellow
    button .find.button -text [_ "Find"] -default active -width 9 \
        -command "::dialog_find::ok $mytoplevel"
    if {$::windowingsystem eq "x11"} {
        button .find.close -text [_ "Close"] -default normal -width 9 \
            -command "::dialog_find::cancel $mytoplevel"
        pack .find.buttonframe .find.button .find.close -side right -padx 10 -pady 3
    } else {
        pack .find.buttonframe .find.button -side right -padx 10 -pady 3
    }
    ::dialog_find::set_canvas_to_search $mytoplevel
}
