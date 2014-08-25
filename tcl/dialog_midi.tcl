package provide dialog_midi 0.1

namespace eval ::dialog_midi:: {
    namespace export pdtk_midi_dialog
    namespace export pdtk_alsa_midi_dialog
}

# TODO this panel really needs some reworking, it works but the code is
# very unreadable


####################### midi dialog ##################

proc ::dialog_midi::apply {mytoplevel} {
    global midi_indev1 midi_indev2 midi_indev3 midi_indev4 midi_indev5 \
        midi_indev6 midi_indev7 midi_indev8 midi_indev9
    global midi_outdev1 midi_outdev2 midi_outdev3 midi_outdev4 midi_outdev5 \
        midi_outdev6 midi_outdev7 midi_outdev8 midi_outdev9
    global midi_alsain midi_alsaout

    pdsend "pd midi-dialog \
        $midi_indev1 \
        $midi_indev2 \
        $midi_indev3 \
        $midi_indev4 \
        $midi_indev5 \
        $midi_indev6 \
        $midi_indev7 \
        $midi_indev8 \
        $midi_indev9 \
        $midi_outdev1 \
        $midi_outdev2 \
        $midi_outdev3 \
        $midi_outdev4 \
        $midi_outdev5 \
        $midi_outdev6 \
        $midi_outdev7 \
        $midi_outdev8 \
        $midi_outdev9 \
        $midi_alsain \
        $midi_alsaout"
}

proc ::dialog_midi::cancel {mytoplevel} {
    pdsend "$mytoplevel cancel"
}

proc ::dialog_midi::ok {mytoplevel} {
    ::dialog_midi::apply $mytoplevel
    ::dialog_midi::cancel $mytoplevel
}

# callback from popup menu
proc midi_popup_action {buttonname varname devlist index} {
    global midi_indevlist midi_outdevlist $varname
    $buttonname configure -text [lindex $devlist $index]
    set $varname $index
}

# create a popup menu
proc midi_popup {name buttonname varname devlist} {
    if [winfo exists $name.popup] {destroy $name.popup}
    menu $name.popup -tearoff false
    if {$::windowingsystem eq "win32"} {
        $name.popup configure -font menuFont
    }
#    puts stderr [concat $devlist ]
    for {set x 0} {$x<[llength $devlist]} {incr x} {
        $name.popup add command -label [lindex $devlist $x] \
            -command [list midi_popup_action \
                $buttonname $varname $devlist $x] 
    }
    tk_popup $name.popup [winfo pointerx $name] [winfo pointery $name] 0
}

# start a dialog window to select midi devices.  "longform" asks us to make
# controls for opening several devices; if not, we get an extra button to
# turn longform on and restart the dialog.
proc ::dialog_midi::pdtk_midi_dialog {id \
      indev1 indev2 indev3 indev4 indev5 indev6 indev7 indev8 indev9 \
      outdev1 outdev2 outdev3 outdev4 outdev5 outdev6 outdev7 outdev8 outdev9 \
      longform} {
    global midi_indev1 midi_indev2 midi_indev3 midi_indev4 midi_indev5 \
         midi_indev6 midi_indev7 midi_indev8 midi_indev9
    global midi_outdev1 midi_outdev2 midi_outdev3 midi_outdev4 midi_outdev5 \
         midi_outdev6 midi_outdev7 midi_outdev8 midi_outdev9
    global midi_indevlist midi_outdevlist
    global midi_alsain midi_alsaout

    set midi_indev1 $indev1
    set midi_indev2 $indev2
    set midi_indev3 $indev3
    set midi_indev4 $indev4
    set midi_indev5 $indev5
    set midi_indev6 $indev6
    set midi_indev7 $indev7
    set midi_indev8 $indev8
    set midi_indev9 $indev9
    set midi_outdev1 $outdev1
    set midi_outdev2 $outdev2
    set midi_outdev3 $outdev3
    set midi_outdev4 $outdev4
    set midi_outdev5 $outdev5
    set midi_outdev6 $outdev6
    set midi_outdev7 $outdev7
    set midi_outdev8 $outdev8
    set midi_outdev9 $outdev9
    set midi_alsain [llength $midi_indevlist]
    set midi_alsaout [llength $midi_outdevlist]

    toplevel $id -class DialogWindow
    wm title $id [_ "MIDI Settings"]
    wm group $id .
    wm resizable $id 0 0
    wm transient $id
    $id configure -menu $::dialog_menubar
    $id configure -padx 10 -pady 5
    ::pd_bindings::dialog_bindings $id "midi"
    # not all Tcl/Tk versions or platforms support -topmost, so catch the error
    catch {wm attributes $id -topmost 1}

    frame $id.buttonframe
    pack $id.buttonframe -side bottom -fill x -pady 2m
    button $id.buttonframe.cancel -text [_ "Cancel"]\
        -command "::dialog_midi::cancel $id"
    button $id.buttonframe.apply -text [_ "Apply"]\
        -command "::dialog_midi::apply $id"
    button $id.buttonframe.ok -text [_ "OK"]\
        -command "::dialog_midi::ok $id"
    pack $id.buttonframe.cancel -side left -expand 1
    pack $id.buttonframe.apply -side left -expand 1
    pack $id.buttonframe.ok -side left -expand 1
    
        # input device 1
    frame $id.in1f
    pack $id.in1f -side top

    label $id.in1f.l1 -text [_ "Input device 1:"]
    button $id.in1f.x1 -text [lindex $midi_indevlist $midi_indev1] \
        -command [list midi_popup $id $id.in1f.x1 midi_indev1 $midi_indevlist]
    pack $id.in1f.l1 $id.in1f.x1 -side left

        # input device 2
    if {$longform && [llength $midi_indevlist] > 2} {
        frame $id.in2f
        pack $id.in2f -side top

        label $id.in2f.l1 -text [_ "Input device 2:"]
        button $id.in2f.x1 -text [lindex $midi_indevlist $midi_indev2] \
            -command [list midi_popup $id $id.in2f.x1 midi_indev2 \
                $midi_indevlist]
        pack $id.in2f.l1 $id.in2f.x1 -side left
    }

        # input device 3
    if {$longform && [llength $midi_indevlist] > 3} {
        frame $id.in3f
        pack $id.in3f -side top

        label $id.in3f.l1 -text [_ "Input device 3:"]
        button $id.in3f.x1 -text [lindex $midi_indevlist $midi_indev3] \
            -command [list midi_popup $id $id.in3f.x1 midi_indev3 \
                $midi_indevlist]
        pack $id.in3f.l1 $id.in3f.x1 -side left
    }

        # input device 4
    if {$longform && [llength $midi_indevlist] > 4} {
        frame $id.in4f
        pack $id.in4f -side top

        label $id.in4f.l1 -text [_ "Input device 4:"]
        button $id.in4f.x1 -text [lindex $midi_indevlist $midi_indev4] \
            -command [list midi_popup $id $id.in4f.x1 midi_indev4 \
                $midi_indevlist]
        pack $id.in4f.l1 $id.in4f.x1 -side left
    }

        # input device 5
    if {$longform && [llength $midi_indevlist] > 5} {
        frame $id.in5f
        pack $id.in5f -side top

        label $id.in5f.l1 -text [_ "Input device 5:"]
        button $id.in5f.x1 -text [lindex $midi_indevlist $midi_indev5] \
            -command [list midi_popup $id $id.in5f.x1 midi_indev5 \
                $midi_indevlist]
        pack $id.in5f.l1 $id.in5f.x1 -side left
    }

        # input device 6
    if {$longform && [llength $midi_indevlist] > 6} {
        frame $id.in6f
        pack $id.in6f -side top

        label $id.in6f.l1 -text [_ "Input device 6:"]
        button $id.in6f.x1 -text [lindex $midi_indevlist $midi_indev6] \
            -command [list midi_popup $id $id.in6f.x1 midi_indev6 \
                $midi_indevlist]
        pack $id.in6f.l1 $id.in6f.x1 -side left
    }

        # input device 7
    if {$longform && [llength $midi_indevlist] > 7} {
        frame $id.in7f
        pack $id.in7f -side top

        label $id.in7f.l1 -text [_ "Input device 7:"]
        button $id.in7f.x1 -text [lindex $midi_indevlist $midi_indev7] \
            -command [list midi_popup $id $id.in7f.x1 midi_indev7 \
                $midi_indevlist]
        pack $id.in7f.l1 $id.in7f.x1 -side left
    }

        # input device 8
    if {$longform && [llength $midi_indevlist] > 8} {
        frame $id.in8f
        pack $id.in8f -side top

        label $id.in8f.l1 -text [_ "Input device 8:"]
        button $id.in8f.x1 -text [lindex $midi_indevlist $midi_indev8] \
            -command [list midi_popup $id $id.in8f.x1 midi_indev8 \
                $midi_indevlist]
        pack $id.in8f.l1 $id.in8f.x1 -side left
    }

        # input device 9
    if {$longform && [llength $midi_indevlist] > 9} {
        frame $id.in9f
        pack $id.in9f -side top

        label $id.in9f.l1 -text [_ "Input device 9:"]
        button $id.in9f.x1 -text [lindex $midi_indevlist $midi_indev9] \
            -command [list midi_popup $id $id.in9f.x1 midi_indev9 \
                $midi_indevlist]
        pack $id.in9f.l1 $id.in9f.x1 -side left
    }

        # output device 1

    frame $id.out1f
    pack $id.out1f -side top
    label $id.out1f.l1 -text [_ "Output device 1:"]
    button $id.out1f.x1 -text [lindex $midi_outdevlist $midi_outdev1] \
        -command [list midi_popup $id $id.out1f.x1 midi_outdev1 \
            $midi_outdevlist]
    pack $id.out1f.l1 $id.out1f.x1 -side left

        # output device 2
    if {$longform && [llength $midi_outdevlist] > 2} {
        frame $id.out2f
        pack $id.out2f -side top
        label $id.out2f.l1 -text [_ "Output device 2:"]
        button $id.out2f.x1 -text [lindex $midi_outdevlist $midi_outdev2] \
            -command \
            [list midi_popup $id $id.out2f.x1 midi_outdev2 $midi_outdevlist]
        pack $id.out2f.l1 $id.out2f.x1 -side left
    }

        # output device 3
    if {$longform && [llength $midi_outdevlist] > 3} {
        frame $id.out3f
        pack $id.out3f -side top
        label $id.out3f.l1 -text [_ "Output device 3:"]
        button $id.out3f.x1 -text [lindex $midi_outdevlist $midi_outdev3] \
            -command \
            [list midi_popup $id $id.out3f.x1 midi_outdev3 $midi_outdevlist]
        pack $id.out3f.l1 $id.out3f.x1 -side left
    }

        # output device 4
    if {$longform && [llength $midi_outdevlist] > 4} {
        frame $id.out4f
        pack $id.out4f -side top
        label $id.out4f.l1 -text [_ "Output device 4:"]
        button $id.out4f.x1 -text [lindex $midi_outdevlist $midi_outdev4] \
            -command \
            [list midi_popup $id $id.out4f.x1 midi_outdev4 $midi_outdevlist]
        pack $id.out4f.l1 $id.out4f.x1 -side left
    }

        # output device 5
    if {$longform && [llength $midi_outdevlist] > 5} {
        frame $id.out5f
        pack $id.out5f -side top
        label $id.out5f.l1 -text [_ "Output device 5:"]
        button $id.out5f.x1 -text [lindex $midi_outdevlist $midi_outdev5] \
            -command \
            [list midi_popup $id $id.out5f.x1 midi_outdev5 $midi_outdevlist]
        pack $id.out5f.l1 $id.out5f.x1 -side left
    }

        # output device 6
    if {$longform && [llength $midi_outdevlist] > 6} {
        frame $id.out6f
        pack $id.out6f -side top
        label $id.out6f.l1 -text [_ "Output device 6:"]
        button $id.out6f.x1 -text [lindex $midi_outdevlist $midi_outdev6] \
            -command \
            [list midi_popup $id $id.out6f.x1 midi_outdev6 $midi_outdevlist]
        pack $id.out6f.l1 $id.out6f.x1 -side left
    }

        # output device 7
    if {$longform && [llength $midi_outdevlist] > 7} {
        frame $id.out7f
        pack $id.out7f -side top
        label $id.out7f.l1 -text [_ "Output device 7:"]
        button $id.out7f.x1 -text [lindex $midi_outdevlist $midi_outdev7] \
            -command \
            [list midi_popup $id $id.out7f.x1 midi_outdev7 $midi_outdevlist]
        pack $id.out7f.l1 $id.out7f.x1 -side left
    }

        # output device 8
    if {$longform && [llength $midi_outdevlist] > 8} {
        frame $id.out8f
        pack $id.out8f -side top
        label $id.out8f.l1 -text [_ "Output device 8:"]
        button $id.out8f.x1 -text [lindex $midi_outdevlist $midi_outdev8] \
            -command \
            [list midi_popup $id $id.out8f.x1 midi_outdev8 $midi_outdevlist]
        pack $id.out8f.l1 $id.out8f.x1 -side left
    }

        # output device 9
    if {$longform && [llength $midi_outdevlist] > 9} {
        frame $id.out9f
        pack $id.out9f -side top
        label $id.out9f.l1 -text [_ "Output device 9:"]
        button $id.out9f.x1 -text [lindex $midi_outdevlist $midi_outdev9] \
            -command \
            [list midi_popup $id $id.out9f.x1 midi_outdev9 $midi_outdevlist]
        pack $id.out9f.l1 $id.out9f.x1 -side left
    }

        # if not the "long form" make a button to
        # restart with longform set. 
    
    if {$longform == 0} {
        frame $id.longbutton
        pack $id.longbutton -side top
        button $id.longbutton.b -text [_ "Use multiple devices"] \
            -command  {pdsend "pd midi-properties 1"}
        pack $id.longbutton.b
    }
}

proc ::dialog_midi::pdtk_alsa_midi_dialog {id indev1 indev2 indev3 indev4 \
        outdev1 outdev2 outdev3 outdev4 longform alsa} {

    global midi_indev1 midi_indev2 midi_indev3 midi_indev4 midi_indev5 \
         midi_indev6 midi_indev7 midi_indev8 midi_indev9
    global midi_outdev1 midi_outdev2 midi_outdev3 midi_outdev4 midi_outdev5 \
         midi_outdev6 midi_outdev7 midi_outdev8 midi_outdev9
    global midi_indevlist midi_outdevlist
    global midi_alsain midi_alsaout

    set midi_indev1 $indev1
    set midi_indev2 $indev2
    set midi_indev3 $indev3
    set midi_indev4 $indev4
    set midi_indev5 0
    set midi_indev6 0
    set midi_indev7 0
    set midi_indev8 0
    set midi_indev9 0
    set midi_outdev1 $outdev1
    set midi_outdev2 $outdev2
    set midi_outdev3 $outdev3
    set midi_outdev4 $outdev4
    set midi_outdev5 0
    set midi_outdev6 0
    set midi_outdev7 0
    set midi_outdev8 0
    set midi_outdev9 0
    set midi_alsain [expr [llength $midi_indevlist] - 1]
    set midi_alsaout [expr [llength $midi_outdevlist] - 1]
    
    toplevel $id
    wm title $id [_ "ALSA MIDI Settings"]
    if {$::windowingsystem eq "aqua"} {$id configure -menu .menubar}
    ::pd_bindings::dialog_bindings $id "midi"

    frame $id.buttonframe
    pack $id.buttonframe -side bottom -fill x -pady 2m
    button $id.buttonframe.cancel -text [_ "Cancel"]\
        -command "::dialog_midi::cancel $id"
    button $id.buttonframe.apply -text [_ "Apply"]\
        -command "::dialog_midi::apply $id"
    button $id.buttonframe.ok -text [_ "OK"]\
        -command "::dialog_midi::ok $id"
    pack $id.buttonframe.cancel -side left -expand 1
    pack $id.buttonframe.apply -side left -expand 1
    pack $id.buttonframe.ok -side left -expand 1

    frame $id.in1f
    pack $id.in1f -side top

    if {$alsa} {
        label $id.in1f.l1 -text [_ "In Ports:"]
        entry $id.in1f.x1 -textvariable midi_alsain -width 4
        pack $id.in1f.l1 $id.in1f.x1 -side left
        label $id.in1f.l2 -text [_ "Out Ports:"]
        entry $id.in1f.x2 -textvariable midi_alsaout -width 4
        pack $id.in1f.l2 $id.in1f.x2 -side left
    }
}
