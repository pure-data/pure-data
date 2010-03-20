# the message dialog panel is a bit unusual in that it is created directly by
# the Tcl 'pd-gui'. Most dialog panels are created by sending a message to
# 'pd', which then sends a message to 'pd-gui' to create the panel.  This is
# similar to the Find dialog panel.

package provide dialog_message 0.1

package require pd_bindings

namespace eval ::dialog_message:: {
    variable message_history {"pd dsp 1"}
    variable history_position 0

    namespace export open_message_dialog
}

proc ::dialog_message::get_history {direction} {
    variable message_history
    variable history_position
    
    incr history_position $direction
    if {$history_position < 0} {set history_position 0}
    if {$history_position > [llength $message_history]} {
        set history_position [llength $message_history]
    }
    .message.f.entry delete 0 end
    .message.f.entry insert 0 \
        [lindex $message_history end-[expr $history_position - 1]]
}

# mytoplevel isn't used here, but is kept for compatibility with other dialog ok procs
proc ::dialog_message::ok {mytoplevel} {
    variable message_history
    set message [.message.f.entry get]
    if {$message ne ""} {
        pdsend $message
        lappend message_history $message
        .message.f.entry delete 0 end
    }
}

# mytoplevel isn't used here, but is kept for compatibility with other dialog cancel procs
proc ::dialog_message::cancel {mytoplevel} {
    wm withdraw .message
}

# the message panel is opened from the menu and key bindings
proc ::dialog_message::open_message_dialog {mytoplevel} {
    if {[winfo exists .message]} {
        wm deiconify .message
        raise .message
    } else {
        create_dialog $mytoplevel
    }
}

proc ::dialog_message::create_dialog {mytoplevel} {
    toplevel .message -class DialogWindow
    wm group .message .
    wm transient .message
    wm title .message [_ "Send a Pd message"]
    wm geometry .message =400x80+150+150
    wm resizable .message 1 0
    wm minsize .message 250 80
    .message configure -menu $::dialog_menubar
    .message configure -padx 10 -pady 5
    ::pd_bindings::dialog_bindings .message "message"
    # not all Tcl/Tk versions or platforms support -topmost, so catch the error
    catch {wm attributes $id -topmost 1}

    # TODO this should use something like 'dialogfont' for the font
    frame .message.f
    pack .message.f -side top -fill x -expand 1
    entry .message.f.entry -width 54 -font {Helvetica 18} -relief sunken \
        -highlightthickness 1 -highlightcolor blue
    label .message.f.semicolon -text ";" -font {Helvetica 24}
    pack .message.f.semicolon -side left
    pack .message.f.entry -side left -padx 10 -fill x -expand 1
    focus .message.f.entry
    label .message.label -text [_ "(use arrow keys for history)"]
    pack .message.label -side bottom

    bind .message.f.entry <Up> "::dialog_message::get_history 1"
    bind .message.f.entry <Down> "::dialog_message::get_history -1"
}
