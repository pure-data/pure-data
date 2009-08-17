
package provide dialog_font 0.1

namespace eval ::dialog_font:: {
    variable fontsize 0
    variable dofont_fontsize 0
    variable stretchval 0
    variable whichstretch 0

    namespace export pdtk_canvas_dofont
}

proc ::dialog_font::apply {mytoplevel myfontsize} {
    pdsend "$mytoplevel font $myfontsize $stretchval $whichstretch"
}

proc ::dialog_font::close {mytoplevel} {
    pdsend "$mytoplevel cancel"
}

proc ::dialog_font::cancel {mytoplevel} {
    ::dialog_font::apply $mytoplevel $fontsize ;# reinstate previous font size
    pdsend "$mytoplevel cancel"
}

proc ::dialog_font::ok {mytoplevel} {
    set fontsize $::dialog_font::fontsize
    ::dialog_font::apply $mytoplevel $fontsize
    ::dialog_font::close $mytoplevel
}

# this should be called pdtk_font_dialog like the rest of the panels, but it
# is called from the C side, so we'll leave it be
proc ::dialog_font::pdtk_canvas_dofont {mytoplevel initsize} {
	create_panel $mytoplevel $initsize
}

proc ::dialog_font::create_panel {mytoplevel initsize} {
    set fontsize $initsize
    set dofont_fontsize $initsize
    set stretchval 100
    set whichstretch 1
    
    toplevel $mytoplevel
    wm title $mytoplevel  {Patch Font}
    wm protocol $mytoplevel WM_DELETE_WINDOW "::dialog_font::cancel $mytoplevel"

    pdtk_panelkeybindings $mytoplevel font
    
    frame $mytoplevel.buttonframe
    pack $mytoplevel.buttonframe -side bottom -fill x -pady 2m
    button $mytoplevel.buttonframe.cancel -text "Cancel" \
        -command "::dialog_font::cancel $mytoplevel"
    button $mytoplevel.buttonframe.ok -text "OK" \
        -command "::dialog_font::ok $mytoplevel"
    pack $mytoplevel.buttonframe.cancel -side left -expand 1
    pack $mytoplevel.buttonframe.ok -side left -expand 1
    
    frame $mytoplevel.radiof
    pack $mytoplevel.radiof -side left
    
    label $mytoplevel.radiof.label -text {Font Size:}
    pack $mytoplevel.radiof.label -side top
	
    radiobutton $mytoplevel.radiof.radio8 -value 8 -variable ::dialog_font::fontsize -text "8" \
        -command "::dialog_font::apply $mytoplevel 8"
    radiobutton $mytoplevel.radiof.radio10 -value 10 -variable ::dialog_font::fontsize -text "10" \
        -command "::dialog_font::apply $mytoplevel 10"
    radiobutton $mytoplevel.radiof.radio12 -value 12 -variable ::dialog_font::fontsize -text "12" \
        -command "::dialog_font::apply $mytoplevel 12"
    radiobutton $mytoplevel.radiof.radio16 -value 16 -variable ::dialog_font::fontsize -text "16" \
        -command "::dialog_font::apply $mytoplevel 16"
    radiobutton $mytoplevel.radiof.radio24 -value 24 -variable ::dialog_font::fontsize -text "24" \
        -command "::dialog_font::apply $mytoplevel 24"
    radiobutton $mytoplevel.radiof.radio36 -value 36 -variable ::dialog_font::fontsize -text "36" \
        -command "::dialog_font::apply $mytoplevel 36"
    pack $mytoplevel.radiof.radio8 -side top -anchor w
    pack $mytoplevel.radiof.radio10 -side top -anchor w
    pack $mytoplevel.radiof.radio12 -side top -anchor w
    pack $mytoplevel.radiof.radio16 -side top -anchor w
    pack $mytoplevel.radiof.radio24 -side top -anchor w
    pack $mytoplevel.radiof.radio36 -side top -anchor w

    set current_radiobutton [format "$mytoplevel.radiof.radio%d" $initsize]
    $current_radiobutton select

    frame $mytoplevel.stretchf
    pack $mytoplevel.stretchf -side left
    
    label $mytoplevel.stretchf.label -text "Stretch:"
    pack $mytoplevel.stretchf.label -side top
    
    entry $mytoplevel.stretchf.entry -textvariable stretchval -width 5
    pack $mytoplevel.stretchf.entry -side left

    radiobutton $mytoplevel.stretchf.radio1 \
        -value 1 -variable whichstretch -text "X and Y"
    radiobutton $mytoplevel.stretchf.radio2 \
        -value 2 -variable whichstretch -text "X only"
    radiobutton $mytoplevel.stretchf.radio3 \
        -value 3 -variable whichstretch -text "Y only"

    pack $mytoplevel.stretchf.radio1 -side top -anchor w
    pack $mytoplevel.stretchf.radio2 -side top -anchor w
    pack $mytoplevel.stretchf.radio3 -side top -anchor w

}
