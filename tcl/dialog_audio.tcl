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
    global audio_sr audio_advance audio_callback audio_blocksize

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
        $audio_callback \
        $audio_blocksize"
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
        longform blocksize} {
    global audio_indev1 audio_indev2 audio_indev3 audio_indev4 
    global audio_inchan1 audio_inchan2 audio_inchan3 audio_inchan4
    global audio_inenable1 audio_inenable2 audio_inenable3 audio_inenable4
    global audio_outdev1 audio_outdev2 audio_outdev3 audio_outdev4
    global audio_outchan1 audio_outchan2 audio_outchan3 audio_outchan4
    global audio_outenable1 audio_outenable2 audio_outenable3 audio_outenable4
    global audio_sr audio_advance audio_callback audio_blocksize
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
    set audio_blocksize $blocksize

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
    
    # settings
    labelframe $mytoplevel.settings -text [_ "Settings"] -padx 5 -pady 5 -borderwidth 1
    pack $mytoplevel.settings -side top -fill x -pady 5
    
    frame $mytoplevel.settings.srd
    pack $mytoplevel.settings.srd -side top -fill x
    label $mytoplevel.settings.srd.sr_label -text [_ "Sample rate:"]
    entry $mytoplevel.settings.srd.sr_entry -textvariable audio_sr -width 7
    label $mytoplevel.settings.srd.d_label -text [_ "Delay (msec):"]
    entry $mytoplevel.settings.srd.d_entry -textvariable audio_advance -width 4
    pack $mytoplevel.settings.srd.sr_label $mytoplevel.settings.srd.sr_entry -side left
    pack $mytoplevel.settings.srd.d_entry $mytoplevel.settings.srd.d_label -side right

    frame $mytoplevel.settings.bsc
    pack $mytoplevel.settings.bsc -side top -fill x
    label $mytoplevel.settings.bsc.bs_label -text [_ "Block size:"]
    tk_optionMenu $mytoplevel.settings.bsc.bs_popup audio_blocksize 64 128 256 512 1024 2048

    pack $mytoplevel.settings.bsc.bs_label $mytoplevel.settings.bsc.bs_popup -side left
    if {$audio_callback >= 0} {
        checkbutton $mytoplevel.settings.bsc.c_button -variable audio_callback \
            -text [_ "Use callbacks"] -anchor e
        pack $mytoplevel.settings.bsc.c_button -side right
    }
    
    # input devices
    labelframe $mytoplevel.inputs -text [_ "Input Devices"] -padx 5 -pady 5 -borderwidth 1
    pack $mytoplevel.inputs -side top -fill x -pady 5

    # input device 1
    frame $mytoplevel.inputs.in1f
    pack $mytoplevel.inputs.in1f -side top -fill x

    checkbutton $mytoplevel.inputs.in1f.x0 -variable audio_inenable1 \
        -text [_ "1:"] -anchor e
    button $mytoplevel.inputs.in1f.x1 -text [lindex $audio_indevlist $audio_indev1] -width 20 \
        -command [list audio_popup $mytoplevel $mytoplevel.inputs.in1f.x1 audio_indev1 $audio_indevlist]
    label $mytoplevel.inputs.in1f.l2 -text [_ "Channels:"]
    entry $mytoplevel.inputs.in1f.x2 -textvariable audio_inchan1 -width 3
    pack $mytoplevel.inputs.in1f.x0 $mytoplevel.inputs.in1f.x1 -side left
    pack $mytoplevel.inputs.in1f.x2 $mytoplevel.inputs.in1f.l2 -side right

    # input device 2
    if {$longform && $multi > 1 && [llength $audio_indevlist] > 1} {
        frame $mytoplevel.inputs.in2f
        pack $mytoplevel.inputs.in2f -side top

        checkbutton $mytoplevel.inputs.in2f.x0 -variable audio_inenable2 \
            -text [_ "2:"] -anchor e
        button $mytoplevel.inputs.in2f.x1 -text [lindex $audio_indevlist $audio_indev2] -width 20 \
            -command [list audio_popup $mytoplevel $mytoplevel.inputs.in2f.x1 audio_indev2 \
                $audio_indevlist]
        label $mytoplevel.inputs.in2f.l2 -text [_ "Channels:"]
        entry $mytoplevel.inputs.in2f.x2 -textvariable audio_inchan2 -width 3
        pack $mytoplevel.inputs.in2f.x0 $mytoplevel.inputs.in2f.x1 -side left
        pack $mytoplevel.inputs.in2f.x2 $mytoplevel.inputs.in2f.l2 -side right
    }

    # input device 3
    if {$longform && $multi > 1 && [llength $audio_indevlist] > 2} {
        frame $mytoplevel.inputs.in3f
        pack $mytoplevel.inputs.in3f -side top

        checkbutton $mytoplevel.inputs.in3f.x0 -variable audio_inenable3 \
            -text [_ "3:"] -anchor e
        button $mytoplevel.inputs.in3f.x1 -text [lindex $audio_indevlist $audio_indev3] -width 20 \
            -command [list audio_popup $mytoplevel $mytoplevel.inputs.in3f.x1 audio_indev3 \
                $audio_indevlist]
        label $mytoplevel.inputs.in3f.l2 -text [_ "Channels:"]
        entry $mytoplevel.inputs.in3f.x2 -textvariable audio_inchan3 -width 3
        pack $mytoplevel.inputs.in3f.x0 $mytoplevel.inputs.in3f.x1 -side left
        pack $mytoplevel.inputs.in3f.x2 $mytoplevel.inputs.in3f.l2 -side right
    }

    # input device 4
    if {$longform && $multi > 1 && [llength $audio_indevlist] > 3} {
        frame $mytoplevel.inputs.in4f
        pack $mytoplevel.inputs.in4f -side top

        checkbutton $mytoplevel.inputs.in4f.x0 -variable audio_inenable4 \
            -text [_ "4:"] -anchor e
        button $mytoplevel.inputs.in4f.x1 -text [lindex $audio_indevlist $audio_indev4] -width 20 \
            -command [list audio_popup $mytoplevel $mytoplevel.inputs.in4f.x1 audio_indev4 \
                $audio_indevlist]
        label $mytoplevel.inputs.in4f.l2 -text [_ "Channels:"]
        entry $mytoplevel.inputs.in4f.x2 -textvariable audio_inchan4 -width 3
        pack $mytoplevel.inputs.in4f.x0 $mytoplevel.inputs.in4f.x1 -side left
        pack $mytoplevel.inputs.in4f.x2 $mytoplevel.inputs.in4f.l2 -side right
    }

    # output devices
    labelframe $mytoplevel.outputs -text [_ "Output Devices"] -padx 5 -pady 5 -borderwidth 1
    pack $mytoplevel.outputs -side top -fill x -pady 5

    # output device 1
    frame $mytoplevel.outputs.out1f
    pack $mytoplevel.outputs.out1f -side top -fill x

    checkbutton $mytoplevel.outputs.out1f.x0 -variable audio_outenable1 \
        -text [_ "1:"] -anchor e
    if {$multi == 0} {
        label $mytoplevel.outputs.out1f.l1 \
            -text [_ "(same as input device)..."]
    } else {
        button $mytoplevel.outputs.out1f.x1 -text [lindex $audio_outdevlist $audio_outdev1] -width 20 \
            -command  [list audio_popup $mytoplevel $mytoplevel.outputs.out1f.x1 audio_outdev1 \
                $audio_outdevlist]
    }
    label $mytoplevel.outputs.out1f.l2 -text [_ "Channels:"]
    entry $mytoplevel.outputs.out1f.x2 -textvariable audio_outchan1 -width 3
    if {$multi == 0} {
        pack $mytoplevel.outputs.out1f.x0 $mytoplevel.outputs.out1f.l1 -side left
        pack $mytoplevel.outputs.out1f.x2 -side right
    } else {
        pack $mytoplevel.outputs.out1f.x0 $mytoplevel.outputs.out1f.x1 -side left
        pack $mytoplevel.outputs.out1f.x2 $mytoplevel.outputs.out1f.l2 -side right
    }

    # output device 2
    if {$longform && $multi > 1 && [llength $audio_outdevlist] > 1} {
        frame $mytoplevel.outputs.out2f
        pack $mytoplevel.outputs.out2f -side top

        checkbutton $mytoplevel.outputs.out2f.x0 -variable audio_outenable2 \
            -text [_ "2:"] -anchor e
        button $mytoplevel.outputs.out2f.x1 -text [lindex $audio_outdevlist $audio_outdev2] -width 20 \
            -command \
            [list audio_popup $mytoplevel $mytoplevel.outputs.out2f.x1 audio_outdev2 $audio_outdevlist]
        label $mytoplevel.outputs.out2f.l2 -text [_ "Channels:"]
        entry $mytoplevel.outputs.out2f.x2 -textvariable audio_outchan2 -width 3
        pack $mytoplevel.outputs.out2f.x0 $mytoplevel.outputs.out2f.x1 -side left
        pack $mytoplevel.outputs.out2f.x2 $mytoplevel.outputs.out2f.l2 -side right
    }

    # output device 3
    if {$longform && $multi > 1 && [llength $audio_outdevlist] > 2} {
        frame $mytoplevel.outputs.out3f
        pack $mytoplevel.outputs.out3f -side top

        checkbutton $mytoplevel.outputs.out3f.x0 -variable audio_outenable3 \
            -text [_ "3:"] -anchor e
        button $mytoplevel.outputs.out3f.x1 -text [lindex $audio_outdevlist $audio_outdev3] -width 20 \
            -command \
            [list audio_popup $mytoplevel $mytoplevel.outputs.out3f.x1 audio_outdev3 $audio_outdevlist]
        label $mytoplevel.outputs.out3f.l2 -text [_ "Channels:"]
        entry $mytoplevel.outputs.out3f.x2 -textvariable audio_outchan3 -width 3
        pack $mytoplevel.outputs.out3f.x0 $mytoplevel.outputs.out3f.x1 -side left
        pack $mytoplevel.outputs.out3f.x2 $mytoplevel.outputs.out3f.l2 -side right
    }

    # output device 4
    if {$longform && $multi > 1 && [llength $audio_outdevlist] > 3} {
        frame $mytoplevel.outputs.out4f
        pack $mytoplevel.outputs.out4f -side top

        checkbutton $mytoplevel.outputs.out4f.x0 -variable audio_outenable4 \
            -text [_ "4:"] -anchor e
        button $mytoplevel.outputs.out4f.x1 -text [lindex $audio_outdevlist $audio_outdev4] -width 20 \
            -command \
            [list audio_popup $mytoplevel $mytoplevel.outputs.out4f.x1 audio_outdev4 $audio_outdevlist]
        label $mytoplevel.outputs.out4f.l2 -text [_ "Channels:"]
        entry $mytoplevel.outputs.out4f.x2 -textvariable audio_outchan4 -width 3
        pack $mytoplevel.outputs.out4f.x0 $mytoplevel.outputs.out4f.x1 -side left
        pack $mytoplevel.outputs.out4f.x2 $mytoplevel.outputs.out4f.l2 -side right
    }

    # If not the "long form" but if "multi" is 2, make a button to
    # restart with longform set. 
    if {$longform == 0 && $multi > 1} {
        frame $mytoplevel.longbutton
        pack $mytoplevel.longbutton -side top
        button $mytoplevel.longbutton.b -text [_ "Use multiple devices"] \
            -command  {pdsend "pd audio-properties 1"}
        pack $mytoplevel.longbutton.b
    }

    # save all settings button
    button $mytoplevel.saveall -text [_ "Save All Settings"] \
        -command "::dialog_audio::ok $mytoplevel; pdsend \"pd save-preferences\""
    pack $mytoplevel.saveall -side top -expand 1 -pady 5

    # buttons
    frame $mytoplevel.buttonframe
    pack $mytoplevel.buttonframe -side top -after $mytoplevel.saveall -fill x -pady 2m
    button $mytoplevel.buttonframe.cancel -text [_ "Cancel"] \
        -command "::dialog_audio::cancel $mytoplevel"
    pack $mytoplevel.buttonframe.cancel -side left -expand 1 -fill x -padx 15
    if {$::windowingsystem ne "aqua"} {
        button $mytoplevel.buttonframe.apply -text [_ "Apply"] \
            -command "::dialog_audio::apply $mytoplevel"
        pack $mytoplevel.buttonframe.apply -side left -expand 1 -fill x -padx 15
    }
    button $mytoplevel.buttonframe.ok -text [_ "OK"] \
        -command "::dialog_audio::ok $mytoplevel" -default active
    pack $mytoplevel.buttonframe.ok -side left -expand 1 -fill x -padx 15

    # set focus
    $mytoplevel.settings.srd.sr_entry select from 0
    $mytoplevel.settings.srd.sr_entry select adjust end
    focus $mytoplevel.settings.srd.sr_entry

    # for focus handling on OSX
    if {$::windowingsystem eq "aqua"} {

        # call apply on Return in entry boxes that are in focus & rebind Return to ok button
        bind $mytoplevel.settings.srd.sr_entry <KeyPress-Return> "::dialog_audio::rebind_return $mytoplevel"
        bind $mytoplevel.settings.srd.d_entry <KeyPress-Return> "::dialog_audio::rebind_return $mytoplevel"
        bind $mytoplevel.outputs.out1f.x2 <KeyPress-Return> "::dialog_audio::rebind_return $mytoplevel"

        # unbind Return from ok button when an entry takes focus
        $mytoplevel.settings.srd.sr_entry config -validate focusin -vcmd "::dialog_audio::unbind_return $mytoplevel"
        $mytoplevel.settings.srd.d_entry config -validate focusin -vcmd "::dialog_audio::unbind_return $mytoplevel"
        $mytoplevel.outputs.out1f.x2 config -validate focusin -vcmd "::dialog_audio::unbind_return $mytoplevel"

        # remove cancel button from focus list since it's not activated on Return
        $mytoplevel.buttonframe.cancel config -takefocus 0

        # can't see focus for buttons, so disable it
        $mytoplevel.settings.bsc.bs_popup config -takefocus 0
        $mytoplevel.settings.bsc.c_button config -takefocus 0
        if {[winfo exists $mytoplevel.inputs.in1f.x1]} {
            $mytoplevel.inputs.in1f.x1 config -takefocus 0
            $mytoplevel.inputs.in1f.x0 config -takefocus 0
        }
        if {[winfo exists $mytoplevel.inputs.in2f.x1]} {
            $mytoplevel.inputs.in2f.x1 config -takefocus 0
            $mytoplevel.inputs.in2f.x0 config -takefocus 0
        }
        if {[winfo exists $mytoplevel.inputs.in3f.x1]} {
            $mytoplevel.inputs.in3f.x1 config -takefocus 0
            $mytoplevel.inputs.in3f.x0 config -takefocus 0
        }
        if {[winfo exists $mytoplevel.inputs.in4f.x1]} {
            $mytoplevel.inputs.in4f.x1 config -takefocus 0
            $mytoplevel.inputs.in4f.x0 config -takefocus 0
        }
        if {[winfo exists $mytoplevel.outputs.out1f.x1]} {
            $mytoplevel.outputs.out1f.x1 config -takefocus 0
            $mytoplevel.outputs.out1f.x0 config -takefocus 0
        }
        if {[winfo exists $mytoplevel.outputs.out2f.x1]} {
            $mytoplevel.outputs.out2f.x1 config -takefocus 0
            $mytoplevel.outputs.out2f.x0 config -takefocus 0
        }
        if {[winfo exists $mytoplevel.outputs.out3f.x1]} {
            $mytoplevel.outputs.out3f.x1 config -takefocus 0
            $mytoplevel.outputs.out3f.x0 config -takefocus 0
        }
        if {[winfo exists $mytoplevel.outputs.out4f.x1]} {
            $mytoplevel.outputs.out4f.x1 config -takefocus 0
            $mytoplevel.outputs.out4f.x0 config -takefocus 0
        }

        # show active focus on save settings button
        bind $mytoplevel.saveall <KeyPress-Return> "$mytoplevel.saveall invoke"
        bind $mytoplevel.saveall <FocusIn> "::dialog_audio::unbind_return $mytoplevel; $mytoplevel.saveall config -default active"
        bind $mytoplevel.saveall <FocusOut> "::dialog_audio::rebind_return $mytoplevel; $mytoplevel.saveall config -default normal"

        # show active focus on the ok button as it *is* activated on Return
        $mytoplevel.buttonframe.ok config -default normal
        bind $mytoplevel.buttonframe.ok <FocusIn> "$mytoplevel.buttonframe.ok config -default active"
        bind $mytoplevel.buttonframe.ok <FocusOut> "$mytoplevel.buttonframe.ok config -default normal"
    }
}

# for focus handling on OSX
proc ::dialog_audio::rebind_return {mytoplevel} {
    bind $mytoplevel <KeyPress-Return> "::dialog_audio::ok $mytoplevel"
    focus $mytoplevel.buttonframe.ok
    return 0
}

# for focus handling on OSX
proc ::dialog_audio::unbind_return {mytoplevel} {
    bind $mytoplevel <KeyPress-Return> break
    return 1
}
