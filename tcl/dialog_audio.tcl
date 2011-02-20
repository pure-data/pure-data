package provide dialog_audio 0.1

namespace eval ::dialog_audio:: {
    namespace export pdtk_audio_dialog
}

# TODO this panel really needs some reworking, it works but the code is very
# unreadable.  The panel could look a lot better too, like using menubuttons
# instead of regular buttons with tk_popup for pulldown menus.

####################### audio dialog ##################3

proc ::dialog_audio::apply {mytoplevel} {
    global audio_indev1 audio_indev2 audio_indev3 audio_indev4 
    global audio_inchan1 audio_inchan2 audio_inchan3 audio_inchan4
    global audio_inenable1 audio_inenable2 audio_inenable3 audio_inenable4
    global audio_outdev1 audio_outdev2 audio_outdev3 audio_outdev4 
    global audio_outchan1 audio_outchan2 audio_outchan3 audio_outchan4
    global audio_outenable1 audio_outenable2 audio_outenable3 audio_outenable4
    global audio_sr audio_advance audio_callback

    pdsend "pd audio-dialog \
        $audio_indev1 \
        $audio_indev2 \
        $audio_indev3 \
        $audio_indev4 \
        [expr $audio_inchan1 * ( $audio_inenable1 ? 1 : -1 ) ]\
        [expr $audio_inchan2 * ( $audio_inenable2 ? 1 : -1 ) ]\
        [expr $audio_inchan3 * ( $audio_inenable3 ? 1 : -1 ) ]\
        [expr $audio_inchan4 * ( $audio_inenable4 ? 1 : -1 ) ]\
        $audio_outdev1 \
        $audio_outdev2 \
        $audio_outdev3 \
        $audio_outdev4 \
        [expr $audio_outchan1 * ( $audio_outenable1 ? 1 : -1 ) ]\
        [expr $audio_outchan2 * ( $audio_outenable2 ? 1 : -1 ) ]\
        [expr $audio_outchan3 * ( $audio_outenable3 ? 1 : -1 ) ]\
        [expr $audio_outchan4 * ( $audio_outenable4 ? 1 : -1 ) ]\
        $audio_sr \
        $audio_advance \
        $audio_callback"
}

proc ::dialog_audio::cancel {mytoplevel} {
    pdsend "$mytoplevel cancel"
}

proc ::dialog_audio::ok {mytoplevel} {
    ::dialog_audio::apply $mytoplevel
    ::dialog_audio::cancel $mytoplevel
}

# callback from popup menu
proc audio_popup_action {buttonname varname devlist index} {
    global audio_indevlist audio_outdevlist $varname
    $buttonname configure -text [lindex $devlist $index]
    set $varname $index
}

# create a popup menu
proc audio_popup {name buttonname varname devlist} {
    if [winfo exists $name.popup] {destroy $name.popup}
    menu $name.popup -tearoff false
    if {$::windowingsystem eq "win32"} {
        $name.popup configure -font menuFont
    }
    for {set x 0} {$x<[llength $devlist]} {incr x} {
        $name.popup add command -label [lindex $devlist $x] \
            -command [list audio_popup_action \
                          $buttonname $varname $devlist $x] 
    }
    tk_popup $name.popup [winfo pointerx $name] [winfo pointery $name] 0
}

# start a dialog window to select audio devices and settings.  "multi"
# is 0 if only one device is allowed; 1 if one apiece may be specified for
# input and output; and 2 if we can select multiple devices.  "longform"
# (which only makes sense if "multi" is 2) asks us to make controls for
# opening several devices; if not, we get an extra button to turn longform
# on and restart the dialog.

proc ::dialog_audio::pdtk_audio_dialog {mytoplevel \
        indev1 indev2 indev3 indev4 \
        inchan1 inchan2 inchan3 inchan4 \
        outdev1 outdev2 outdev3 outdev4 \
        outchan1 outchan2 outchan3 outchan4 sr advance multi callback \
        longform} {
    global audio_indev1 audio_indev2 audio_indev3 audio_indev4 
    global audio_inchan1 audio_inchan2 audio_inchan3 audio_inchan4
    global audio_inenable1 audio_inenable2 audio_inenable3 audio_inenable4
    global audio_outdev1 audio_outdev2 audio_outdev3 audio_outdev4
    global audio_outchan1 audio_outchan2 audio_outchan3 audio_outchan4
    global audio_outenable1 audio_outenable2 audio_outenable3 audio_outenable4
    global audio_sr audio_advance audio_callback
    global audio_indevlist audio_outdevlist
    global pd_indev pd_outdev
    global audio_longform

    set audio_indev1 $indev1
    set audio_indev2 $indev2
    set audio_indev3 $indev3
    set audio_indev4 $indev4

    set audio_inchan1 [expr ( $inchan1 > 0 ? $inchan1 : -$inchan1 ) ]
    set audio_inenable1 [expr $inchan1 > 0 ]
    set audio_inchan2 [expr ( $inchan2 > 0 ? $inchan2 : -$inchan2 ) ]
    set audio_inenable2 [expr $inchan2 > 0 ]
    set audio_inchan3 [expr ( $inchan3 > 0 ? $inchan3 : -$inchan3 ) ]
    set audio_inenable3 [expr $inchan3 > 0 ]
    set audio_inchan4 [expr ( $inchan4 > 0 ? $inchan4 : -$inchan4 ) ]
    set audio_inenable4 [expr $inchan4 > 0 ]

    set audio_outdev1 $outdev1
    set audio_outdev2 $outdev2
    set audio_outdev3 $outdev3
    set audio_outdev4 $outdev4

    set audio_outchan1 [expr ( $outchan1 > 0 ? $outchan1 : -$outchan1 ) ]
    set audio_outenable1 [expr $outchan1 > 0 ]
    set audio_outchan2 [expr ( $outchan2 > 0 ? $outchan2 : -$outchan2 ) ]
    set audio_outenable2 [expr $outchan2 > 0 ]
    set audio_outchan3 [expr ( $outchan3 > 0 ? $outchan3 : -$outchan3 ) ]
    set audio_outenable3 [expr $outchan3 > 0 ]
    set audio_outchan4 [expr ( $outchan4 > 0 ? $outchan4 : -$outchan4 ) ]
    set audio_outenable4 [expr $outchan4 > 0 ]

    set audio_sr $sr
    set audio_advance $advance
    set audio_callback $callback

    toplevel $mytoplevel -class DialogWindow
    wm title $mytoplevel [_ "Audio Settings"]
    wm group $mytoplevel .
    wm resizable $mytoplevel 0 0
    wm transient $mytoplevel
    $mytoplevel configure -menu $::dialog_menubar
    $mytoplevel configure -padx 10 -pady 5
    ::pd_bindings::dialog_bindings $mytoplevel "audio"
    # not all Tcl/Tk versions or platforms support -topmost, so catch the error
    catch {wm attributes $mytoplevel -topmost 1}

    frame $mytoplevel.buttonframe
    pack $mytoplevel.buttonframe -side bottom -fill x -pady 2m
    button $mytoplevel.buttonframe.cancel -text [_ "Cancel"]\
        -command "::dialog_audio::cancel $mytoplevel"
    pack $mytoplevel.buttonframe.cancel -side left -expand 1 -fill x -padx 15
    button $mytoplevel.buttonframe.apply -text [_ "Apply"]\
        -command "::dialog_audio::apply $mytoplevel"
    pack $mytoplevel.buttonframe.apply -side left -expand 1 -fill x -padx 15
    button $mytoplevel.buttonframe.ok -text [_ "OK"] \
        -command "::dialog_audio::ok $mytoplevel"
    pack $mytoplevel.buttonframe.ok -side left -expand 1 -fill x -padx 15

    button $mytoplevel.saveall -text [_ "Save All Settings"]\
        -command "::dialog_audio::apply $mytoplevel; pdsend {pd save-preferences}"
    pack $mytoplevel.saveall -side bottom -expand 1 -pady 5
    
        # sample rate and advance
    frame $mytoplevel.srf
    pack $mytoplevel.srf -side top
    
    label $mytoplevel.srf.l1 -text [_ "Sample rate:"]
    entry $mytoplevel.srf.x1 -textvariable audio_sr -width 7
    label $mytoplevel.srf.l2 -text [_ "Delay (msec):"]
    entry $mytoplevel.srf.x2 -textvariable audio_advance -width 4
    pack $mytoplevel.srf.l1 $mytoplevel.srf.x1 $mytoplevel.srf.l2 \
        $mytoplevel.srf.x2 -side left
    if {$audio_callback >= 0} {
        checkbutton $mytoplevel.srf.x3 -variable audio_callback \
            -text [_ "Use callbacks"] -anchor e
        pack $mytoplevel.srf.x3 -side left
    }
        # input device 1
    frame $mytoplevel.in1f
    pack $mytoplevel.in1f -side top

    checkbutton $mytoplevel.in1f.x0 -variable audio_inenable1 \
        -text [_ "Input device 1:"] -anchor e
    button $mytoplevel.in1f.x1 -text [lindex $audio_indevlist $audio_indev1] \
        -command [list audio_popup $mytoplevel $mytoplevel.in1f.x1 audio_indev1 $audio_indevlist]
    label $mytoplevel.in1f.l2 -text [_ "Channels:"]
    entry $mytoplevel.in1f.x2 -textvariable audio_inchan1 -width 3
    pack $mytoplevel.in1f.x0 $mytoplevel.in1f.x1 $mytoplevel.in1f.l2 \
        $mytoplevel.in1f.x2 -side left -fill x

        # input device 2
    if {$longform && $multi > 1 && [llength $audio_indevlist] > 1} {
        frame $mytoplevel.in2f
        pack $mytoplevel.in2f -side top

        checkbutton $mytoplevel.in2f.x0 -variable audio_inenable2 \
            -text [_ "Input device 2:"] -anchor e
        button $mytoplevel.in2f.x1 -text [lindex $audio_indevlist $audio_indev2] \
            -command [list audio_popup $mytoplevel $mytoplevel.in2f.x1 audio_indev2 \
                $audio_indevlist]
        label $mytoplevel.in2f.l2 -text [_ "Channels:"]
        entry $mytoplevel.in2f.x2 -textvariable audio_inchan2 -width 3
        pack $mytoplevel.in2f.x0 $mytoplevel.in2f.x1 $mytoplevel.in2f.l2 \
            $mytoplevel.in2f.x2 -side left -fill x
    }

        # input device 3
    if {$longform && $multi > 1 && [llength $audio_indevlist] > 2} {
        frame $mytoplevel.in3f
        pack $mytoplevel.in3f -side top

        checkbutton $mytoplevel.in3f.x0 -variable audio_inenable3 \
            -text [_ "Input device 3:"] -anchor e
        button $mytoplevel.in3f.x1 -text [lindex $audio_indevlist $audio_indev3] \
            -command [list audio_popup $mytoplevel $mytoplevel.in3f.x1 audio_indev3 \
                $audio_indevlist]
        label $mytoplevel.in3f.l2 -text [_ "Channels:"]
        entry $mytoplevel.in3f.x2 -textvariable audio_inchan3 -width 3
        pack $mytoplevel.in3f.x0 $mytoplevel.in3f.x1 $mytoplevel.in3f.l2 $mytoplevel.in3f.x2 -side left
    }

        # input device 4
    if {$longform && $multi > 1 && [llength $audio_indevlist] > 3} {
        frame $mytoplevel.in4f
        pack $mytoplevel.in4f -side top

        checkbutton $mytoplevel.in4f.x0 -variable audio_inenable4 \
            -text [_ "Input device 4:"] -anchor e
        button $mytoplevel.in4f.x1 -text [lindex $audio_indevlist $audio_indev4] \
            -command [list audio_popup $mytoplevel $mytoplevel.in4f.x1 audio_indev4 \
                $audio_indevlist]
        label $mytoplevel.in4f.l2 -text [_ "Channels:"]
        entry $mytoplevel.in4f.x2 -textvariable audio_inchan4 -width 3
        pack $mytoplevel.in4f.x0 $mytoplevel.in4f.x1 $mytoplevel.in4f.l2 \
            $mytoplevel.in4f.x2 -side left
    }

        # output device 1
    frame $mytoplevel.out1f
    pack $mytoplevel.out1f -side top

    checkbutton $mytoplevel.out1f.x0 -variable audio_outenable1 \
        -text [_ "Output device 1:"] -anchor e
    if {$multi == 0} {
        label $mytoplevel.out1f.l1 \
            -text [_ "(same as input device) ..............      "]
    } else {
        button $mytoplevel.out1f.x1 -text [lindex $audio_outdevlist $audio_outdev1] \
            -command  [list audio_popup $mytoplevel $mytoplevel.out1f.x1 audio_outdev1 \
                $audio_outdevlist]
    }
    label $mytoplevel.out1f.l2 -text [_ "Channels:"]
    entry $mytoplevel.out1f.x2 -textvariable audio_outchan1 -width 3
    if {$multi == 0} {
        pack $mytoplevel.out1f.x0 $mytoplevel.out1f.l1 $mytoplevel.out1f.x2 -side left -fill x
    } else {
        pack $mytoplevel.out1f.x0 $mytoplevel.out1f.x1 $mytoplevel.out1f.l2\
            $mytoplevel.out1f.x2 -side left -fill x
    }

        # output device 2
    if {$longform && $multi > 1 && [llength $audio_outdevlist] > 1} {
        frame $mytoplevel.out2f
        pack $mytoplevel.out2f -side top

        checkbutton $mytoplevel.out2f.x0 -variable audio_outenable2 \
            -text [_ "Output device 2:"] -anchor e
        button $mytoplevel.out2f.x1 -text [lindex $audio_outdevlist $audio_outdev2] \
            -command \
            [list audio_popup $mytoplevel $mytoplevel.out2f.x1 audio_outdev2 $audio_outdevlist]
        label $mytoplevel.out2f.l2 -text [_ "Channels:"]
        entry $mytoplevel.out2f.x2 -textvariable audio_outchan2 -width 3
        pack $mytoplevel.out2f.x0 $mytoplevel.out2f.x1 $mytoplevel.out2f.l2\
            $mytoplevel.out2f.x2 -side left
    }

        # output device 3
    if {$longform && $multi > 1 && [llength $audio_outdevlist] > 2} {
        frame $mytoplevel.out3f
        pack $mytoplevel.out3f -side top

        checkbutton $mytoplevel.out3f.x0 -variable audio_outenable3 \
            -text [_ "Output device 3:"] -anchor e
        button $mytoplevel.out3f.x1 -text [lindex $audio_outdevlist $audio_outdev3] \
            -command \
            [list audio_popup $mytoplevel $mytoplevel.out3f.x1 audio_outdev3 $audio_outdevlist]
        label $mytoplevel.out3f.l2 -text [_ "Channels:"]
        entry $mytoplevel.out3f.x2 -textvariable audio_outchan3 -width 3
        pack $mytoplevel.out3f.x0 $mytoplevel.out3f.x1 $mytoplevel.out3f.l2 \
            $mytoplevel.out3f.x2 -side left
    }

        # output device 4
    if {$longform && $multi > 1 && [llength $audio_outdevlist] > 3} {
        frame $mytoplevel.out4f
        pack $mytoplevel.out4f -side top

        checkbutton $mytoplevel.out4f.x0 -variable audio_outenable4 \
            -text [_ "Output device 4:"] -anchor e
        button $mytoplevel.out4f.x1 -text [lindex $audio_outdevlist $audio_outdev4] \
            -command \
            [list audio_popup $mytoplevel $mytoplevel.out4f.x1 audio_outdev4 $audio_outdevlist]
        label $mytoplevel.out4f.l2 -text [_ "Channels:"]
        entry $mytoplevel.out4f.x2 -textvariable audio_outchan4 -width 3
        pack $mytoplevel.out4f.x0 $mytoplevel.out4f.x1 $mytoplevel.out4f.l2 \
            $mytoplevel.out4f.x2 -side left
    }

        # if not the "long form" but if "multi" is 2, make a button to
        # restart with longform set. 
    
    if {$longform == 0 && $multi > 1} {
        frame $mytoplevel.longbutton
        pack $mytoplevel.longbutton -side top
        button $mytoplevel.longbutton.b -text [_ "Use multiple devices"] \
            -command  {pdsend "pd audio-properties 1"}
        pack $mytoplevel.longbutton.b
    }
    $mytoplevel.srf.x1 select from 0
    $mytoplevel.srf.x1 select adjust end
    focus $mytoplevel.srf.x1
}
