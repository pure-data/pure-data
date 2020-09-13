# This is a kind of console that sends messages directly to a canvas

package provide entry_canvasmsg 0.1

package require pd_bindings

namespace eval ::entry_canvasmsg:: {
    namespace export openclose_canvasmsg
}


# the canvasmsg panel is opened/closed from the menu and key bindings
proc ::entry_canvasmsg::openclose_canvasmsg {mytoplevel} {
    if { $mytoplevel eq ".pdwindow"} {
        return
    }
    if {[winfo exists $mytoplevel.c.canvas_message]} {
        focus $mytoplevel.c.canvas_message
    } else {
        create_canvasmsg_dialog $mytoplevel
    }
}

# creates the entry for a patch window
# TODO: use tkcanvas_name instead of $mytoplevel.c
proc ::entry_canvasmsg::create_canvasmsg_dialog {mytoplevel} {

    set canvasmsg "[tkcanvas_name $mytoplevel].canvas_message"

    entry $mytoplevel.c.canvas_message -width 300 -font {$::font_family 9} -relief solid \
        -highlightthickness 1 -highlightcolor blue \
        -borderwidth 1 \
        -textvariable ::canvasmsg_entry_text($mytoplevel)

    #$mytoplevel.c.canvas_message insert 0 "goto "
    # change that
    # use the same syntax as
    # set tkcanvas [tkcanvas_name $mytoplevel]

    pack $mytoplevel.c.canvas_message -anchor s -fill x -expand 1
    focus $mytoplevel.c.canvas_message

    bind $mytoplevel.c.canvas_message <KeyPress-Return> "::entry_canvasmsg::ok $mytoplevel"
    bind $mytoplevel.c.canvas_message <KeyPress-Escape> "::entry_canvasmsg::cancel $mytoplevel"

    # UNBIND otherwise the patch would get the key presses

    # we stop it from happening by including the break statement
    # see:
    # https://stackoverflow.com/questions/19342875/overriding-binding-in-tcl-tk-text-widget
    bind $mytoplevel <KeyPress> {break}
    # then it will receive from the canvas
}

# when the user presses Return
proc ::entry_canvasmsg::ok {mytoplevel} {
    set msg [$mytoplevel.c.canvas_message get]

    # ::pdwindow::post "sending message to PD: $msg\n"

    if { [lindex $msg 0] eq "connect"} {
        lset msg 0 "connect_with_undo"
    }
    pdsend "$mytoplevel $msg"

    # bind again
    # we do this just by removing our toplevel binding which had a break statement
    bind $mytoplevel <KeyPress> {}
    destroy $mytoplevel.c.canvas_message
}

proc ::entry_canvasmsg::cancel {mytoplevel} {
    bind $mytoplevel <KeyPress> {}
    destroy $mytoplevel.c.canvas_message
}

proc ::entry_canvasmsg::open_canvas_msg_dialog_with_text {mytoplevel text} {
    ::entry_canvasmsg::create_canvasmsg_dialog $mytoplevel
    set ::canvasmsg_entry_text($mytoplevel) "$text"
    $mytoplevel.c.canvas_message icursor end
    if {$text eq "goto "} {
        pdsend "$mytoplevel toggleindices 1"
    }
}

proc ::entry_canvasmsg::select_range {mytoplevel start finish} {
    $mytoplevel.c.canvas_message selection range $start $finish
    $mytoplevel.c.canvas_message icursor $start
}

proc ::entry_canvasmsg::select_last_n_chars {mytoplevel n} {
    set l [string length [$mytoplevel.c.canvas_message get]]
    $mytoplevel.c.canvas_message selection range [expr $l-$n] end
}

