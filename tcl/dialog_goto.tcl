package provide dialog_goto 0.1

package require pd_bindings

namespace eval ::dialog_goto:: {
    namespace export pdtk_goto_open
}


# the goto panel is opened/closed from the menu and key bindings
proc ::dialog_goto::pdtk_goto_open {mytoplevel} {
    set canvas [tkcanvas_name $mytoplevel]
    if { $mytoplevel eq ".pdwindow"} {
        return
    }
    if {[winfo exists $canvas.goto]} {
        focus $canvas.goto
    } else {
        create_goto_dialog $mytoplevel
    }
}

# creates the entry for a patch window
proc ::dialog_goto::create_goto_dialog {mytoplevel} {

    set canvas [tkcanvas_name $mytoplevel]
    set w 5
    set max_objects 999

    frame $canvas.goto

    label $canvas.goto.label -text "go to object:" -padx 2m

    # Spinbox to select the object index
    spinbox $canvas.goto.index -from 0 -to $max_objects -width $w -relief solid \
        -highlightthickness 1 -highlightcolor blue -borderwidth 1 -textvariable ::index \
        -vcmd "::dialog_goto::validate_spinbox %P"

    # We configure validation *after* declaring the spinbox otherwise the validate command
    # will be run *before* the spinbox is created
    $canvas.goto.index configure -validate key

    pack $canvas.goto -anchor s -fill x -expand 1
    focus $canvas.goto.index

    # help button
    button $canvas.goto.help -text "?"  -width -2 -height -2 \
        -command {::pd_menucommands::menu_doc_open doc/7.stuff/patching/ keyboard.patching-help.pd}


    grid $canvas.goto.label -column 0 -row 0
    grid $canvas.goto.index -column 1 -row 0
    grid $canvas.goto.help -column 2 -row 0 -rowspan 2
    grid configure $canvas.goto.help -sticky ne -padx 4 -pady 2
    # hack for anchoring to the east
    grid columnconfigure $canvas.goto 2 -weight 100

    pack $canvas.goto -anchor s -fill x -expand 1

    # bindings for accepting / canceling the operation
    bind $canvas.goto.index <KeyPress-Return> "dialog_goto::ok $mytoplevel"

    # open help patch with Return (default is only Space)
    bind $canvas.goto.help <KeyPress-Return> "$canvas.goto.help invoke; break"

    # Add a break statement to stop the "<KeyPress-Escape>"
    # in the kbdnav "$mytoplevel" bindtags from executing
    bind $canvas.goto.index <KeyPress-Escape> "dialog_goto::cancel $mytoplevel; break"
    bind $canvas.goto.help <KeyPress-Escape> "dialog_goto::cancel $mytoplevel; break"

    # remove the toplevel and "all" bindtags so key presses don't
    # end up being sent to the canvas
    bindtags $canvas.goto.index [lrange [bindtags $canvas.goto.index] 0 1]
    bindtags $canvas.goto.help [lrange [bindtags $canvas.goto.help] 0 1]

    # consequently we have to manually bind the Tab as focus traversal
    bind $canvas.goto.index <KeyPress-Tab> "focus $canvas.goto.help"
    bind $canvas.goto.help <KeyPress-Tab> "focus $canvas.goto.index"


    $canvas.goto.index selection range 0 end
    pdsend "$mytoplevel set_indices_visibility 1"
}

# when the user presses Return
proc ::dialog_goto::ok {mytoplevel} {
    set canvas [tkcanvas_name $mytoplevel]
    set i [$canvas.goto.index get]
    if { $i eq "" } {
        return
    }
    pdsend "$mytoplevel goto $i"
    ::dialog_goto::cancel $mytoplevel
}

proc ::dialog_goto::cancel {mytoplevel} {
    set canvas [tkcanvas_name $mytoplevel]
    destroy $canvas.goto
    pdsend "$mytoplevel set_indices_visibility 0"
}

proc ::dialog_goto::validate_spinbox {value} {
    return [string is integer $value]
}
