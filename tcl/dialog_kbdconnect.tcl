package provide dialog_kbdconnect 0.1

package require pd_bindings

namespace eval ::dialog_kbdconnect:: {
    namespace export pdtk_kbdconnect_open
    namespace export pdtk_kbdconnect_prefilled
}

# the kbdconnect panel is opened/closed from the menu and key bindings
proc ::dialog_kbdconnect::pdtk_kbdconnect_open {mytoplevel} {
    set canvas [tkcanvas_name $mytoplevel]
    if { $mytoplevel eq ".pdwindow"} {
        return
    }
    if {[winfo exists $canvas.kbdconn]} {
        focus $canvas.kbdconn
    } else {
        create_kbdconnect_dialog $mytoplevel
        # beware the bug:
        # https://stackoverflow.com/questions/60438845/cant-change-focus-after-first-focus-call
        # We call it here instead of inside "create_kbdconnect_dialog" because the bug wouldn't
        # let "pdtk_kbdconnect_prefilled" change the focus
        set_focus $mytoplevel 0
    }
}

# creates the entry for a patch window
# TODO: use tkcanvas_name instead of $canvas
proc ::dialog_kbdconnect::create_kbdconnect_dialog {mytoplevel} {

    set canvas [tkcanvas_name $mytoplevel]

    set w 5

    frame $canvas.kbdconn
    label $canvas.kbdconn.l -text "Connect:" -padx 2m

    label $canvas.kbdconn.lab_obj1_idx -text "obj"
    label $canvas.kbdconn.lab_obj1_out -text "outlet"
    label $canvas.kbdconn.lab_obj2_idx -text "obj"
    label $canvas.kbdconn.lab_obj2_in -text "inlet"
    label $canvas.kbdconn.arrow -text "->"

    spinbox $canvas.kbdconn.obj1_index -from 0 -to 999 -width $w -relief \
        solid -highlightthickness 1 -highlightcolor blue -borderwidth 1 \
        -vcmd "::dialog_kbdconnect::validate_spinbox %P"
    spinbox $canvas.kbdconn.obj1_outlet -from 0 -to 999 -width $w -relief \
        solid -highlightthickness 1 -highlightcolor blue -borderwidth 1 \
        -vcmd "::dialog_kbdconnect::validate_spinbox %P"
    spinbox $canvas.kbdconn.obj2_index -from 0 -to 999 -width $w -relief \
        solid -highlightthickness 1 -highlightcolor blue -borderwidth 1 \
        -vcmd "::dialog_kbdconnect::validate_spinbox %P"
    spinbox $canvas.kbdconn.obj2_inlet -from 0 -to 999 -width $w -relief \
        solid -highlightthickness 1 -highlightcolor blue -borderwidth 1 \
        -vcmd "::dialog_kbdconnect::validate_spinbox %P"

    # We configure validation *after* declaring the spinboxes otherwise the validate command
    # will be run *before* the spinbox is created
    $canvas.kbdconn.obj1_index configure -validate key
    $canvas.kbdconn.obj1_outlet configure -validate key
    $canvas.kbdconn.obj2_index configure -validate key
    $canvas.kbdconn.obj2_inlet configure -validate key

    # help button
    button $canvas.kbdconn.help -text "?"  -width -2 -height -2 \
        -command {::pd_menucommands::menu_doc_open doc/7.stuff/patching/ keyboard.patching-help.pd}

    # grid labels
    grid $canvas.kbdconn.l -column 0 -row 1
    grid $canvas.kbdconn.lab_obj1_idx -column 1 -row 0
    grid $canvas.kbdconn.lab_obj1_out -column 2 -row 0
    grid $canvas.kbdconn.lab_obj2_idx -column 4 -row 0
    grid $canvas.kbdconn.lab_obj2_in -column 5 -row 0
    # grid spinboxes
    grid $canvas.kbdconn.obj1_index -column 1 -row 1
    grid $canvas.kbdconn.obj1_outlet -column 2 -row 1
    grid $canvas.kbdconn.arrow -column 3 -row 1
    grid $canvas.kbdconn.obj2_index -column 4 -row 1
    grid $canvas.kbdconn.obj2_inlet -column 5 -row 1
    # grid help button
    grid $canvas.kbdconn.help -column 6 -row 0 -rowspan 2
    grid configure $canvas.kbdconn.help -sticky ne -padx 4 -pady 2
    # hack for anchoring to the east
    grid columnconfigure $canvas.kbdconn 6 -weight 100


    pack $canvas.kbdconn -anchor s -fill x -expand 1

    # left/right arrow keys move through the spinboxes
    bind $canvas.kbdconn.obj1_index <Key> { if {"%K" eq "Right"} { focus ".[lindex [split %W .] 1].c.kbdconn.obj1_outlet" } }
    bind $canvas.kbdconn.obj1_outlet <Key> { if {"%K" eq "Left"} { focus ".[lindex [split %W .] 1].c.kbdconn.obj1_index" } }
    bind $canvas.kbdconn.obj1_outlet <Key> {+ if {"%K" eq "Right"} { focus ".[lindex [split %W .] 1].c.kbdconn.obj2_index" } }
    bind $canvas.kbdconn.obj2_index <Key> { if {"%K" eq "Left"} { focus ".[lindex [split %W .] 1].c.kbdconn.obj1_outlet" } }
    bind $canvas.kbdconn.obj2_index <Key> {+ if {"%K" eq "Right"} { focus ".[lindex [split %W .] 1].c.kbdconn.obj2_inlet" } }
    bind $canvas.kbdconn.obj2_inlet <Key> { if {"%K" eq "Left"} { focus ".[lindex [split %W .] 1].c.kbdconn.obj2_index" } }

    # bindings for accepting / canceling the operation
    bind $canvas.kbdconn.obj1_index <KeyPress-Return> "::dialog_kbdconnect::ok $mytoplevel 1"
    bind $canvas.kbdconn.obj1_outlet <KeyPress-Return> "::dialog_kbdconnect::ok $mytoplevel 1"
    bind $canvas.kbdconn.obj2_index <KeyPress-Return> "::dialog_kbdconnect::ok $mytoplevel 1"
    bind $canvas.kbdconn.obj2_inlet <KeyPress-Return> "::dialog_kbdconnect::ok $mytoplevel 1"
    # Shift+Enter: do the connection but doesn't close the dialog
    bind $canvas.kbdconn.obj1_index <Shift-Return> "::dialog_kbdconnect::ok $mytoplevel 0"
    bind $canvas.kbdconn.obj1_outlet <Shift-Return> "::dialog_kbdconnect::ok $mytoplevel 0"
    bind $canvas.kbdconn.obj2_index <Shift-Return> "::dialog_kbdconnect::ok $mytoplevel 0"
    bind $canvas.kbdconn.obj2_inlet <Shift-Return> "::dialog_kbdconnect::ok $mytoplevel 0"

    # open help patch with Return (default is only Space)
    bind $canvas.kbdconn.help <KeyPress-Return> "$canvas.kbdconn.help invoke; break"

    # Add a break statement to stop the "<KeyPress-Escape>"
    # in the "all" bindtags from executing (bound in pd_bindings.tcl)
    bind $mytoplevel <KeyPress-Escape> "dialog_kbdconnect::cancel $mytoplevel; break"

    # Stop the "bind all <KeyPress>" in pd_bindings.tcl from sending
    # key presses to the canvas window
    bind $mytoplevel <KeyPress> {break}

    # Override the <Key> { break } binding on the toplevel
    bind $mytoplevel <KeyPress-Tab> {#nothing}

    $canvas.kbdconn.obj1_index set ""
    $canvas.kbdconn.obj1_outlet set ""
    $canvas.kbdconn.obj2_index set ""
    $canvas.kbdconn.obj2_inlet set ""

    pdsend "$mytoplevel set_indices_visibility 1"
}

# when the user presses Return
proc ::dialog_kbdconnect::ok {mytoplevel {close 1}} {
    set canvas [tkcanvas_name $mytoplevel]
    set obj1_index [$canvas.kbdconn.obj1_index get]
    set obj1_outlet [$canvas.kbdconn.obj1_outlet get]
    set obj2_index [$canvas.kbdconn.obj2_index get]
    set obj2_inlet [$canvas.kbdconn.obj2_inlet get]
    if { $obj1_index eq "" || \
         $obj1_outlet eq "" || \
         $obj2_index eq "" || \
         $obj2_inlet eq "" } {
        return
     }
    set msg "$mytoplevel connect_with_undo $obj1_index $obj1_outlet $obj2_index $obj2_inlet"
    pdsend "$msg"
    if {$close} {
        ::dialog_kbdconnect::cancel $mytoplevel
    } else {
        $canvas.kbdconn.obj1_index set ""
        $canvas.kbdconn.obj1_outlet set ""
        $canvas.kbdconn.obj2_index set ""
        $canvas.kbdconn.obj2_inlet set ""
        focus $canvas.kbdconn.obj1_index
    }
}

proc ::dialog_kbdconnect::cancel {mytoplevel} {
    set canvas [tkcanvas_name $mytoplevel]
    # first we remove our binding with the break statement so the
    # toplevel will process keypresses again
    bind $mytoplevel <KeyPress> {}
    # remove the others
    bind $mytoplevel <KeyPress-Return> {}
    bind $mytoplevel <KeyPress-Escape> {}
    bind $mytoplevel <KeyPress-Tab> {}
    destroy $canvas.kbdconn
    pdsend "$mytoplevel set_indices_visibility 0"
}

proc ::dialog_kbdconnect::pdtk_kbdconnect_prefilled {mytoplevel obj1 outlet obj2 inlet} {
    set canvas [tkcanvas_name $mytoplevel]
    ::dialog_kbdconnect::create_kbdconnect_dialog $mytoplevel
    $canvas.kbdconn.obj1_index set $obj1
    $canvas.kbdconn.obj1_outlet set $outlet
    $canvas.kbdconn.obj2_index set $obj2
    $canvas.kbdconn.obj2_inlet set $inlet
}

# used by the core to let the gui know in which spinbox
# to start typing.
# values for argument "i"
# 0 = obj1_index
# 1 = obj1_outlet
# 2 = obj2_index
# 3 = obj2_inlet
proc ::dialog_kbdconnect::set_focus {mytoplevel i} {
    set canvas [tkcanvas_name $mytoplevel]
    switch -- $i {
        0 { focus $canvas.kbdconn.obj1_index }
        1 { focus $canvas.kbdconn.obj1_outlet }
        2 { focus $canvas.kbdconn.obj2_index }
        3 { focus $canvas.kbdconn.obj2_inlet }
        default { ::pdwindow::post "\[::dialog_kbdconnect::set_focus error\] Invalid argument: $i" }
    }
}

proc ::dialog_kbdconnect::validate_spinbox {value} {
    return [string is integer $value]
}
