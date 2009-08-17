
package provide wheredoesthisgo 0.1

# a place to temporarily store things until they find a home or go away

set help_top_directory ""


proc post_tclinfo {} {
    pdtk_post "Tcl library: [info library]"
    pdtk_post "executable: [info nameofexecutable]"
    pdtk_post "tclversion: [info tclversion]"
    pdtk_post "patchlevel: [info patchlevel]"
    pdtk_post "sharedlibextension: [info sharedlibextension]"
}


proc placeholder {args} {
    # PLACEHOLDER
    pdtk_post "PLACEHOLDER $args"
}


proc open_file {filename} {
    set directory [file dirname $filename]
    set basename [file tail $filename]
    if {[regexp -nocase -- "\.(pd|pat|mxt)$" $filename]} {
    pdsend "pd open [enquote_path $basename] [enquote_path $directory]"
    }
}

# ------------------------------------------------------------------------------
# quoting functions

# enquote a filename to send it to pd, " isn't handled properly tho...
proc enquote_path {message} {
    string map {"," "\\," ";" "\\;" " " "\\ "} $message
}

#enquote a string to send it to Pd.  Blow off semi and comma; alias spaces
#we also blow off "{", "}", "\" because they'll just cause bad trouble later.
proc unspace_text {x} {
    set y [string map {" " "_" ";" "" "," "" "{" "" "}" "" "\\" ""} $x]
    if {$y == ""} {set y "empty"}
    concat $y
}


#------------------------------------------------------------------------------#
# key usage

proc pdsend_key {mycanvas state key iso shift} {
    # TODO canvas_key on the C side should be refactored with this proc as well
    switch -- $key {
        "BackSpace" { set iso ""; set key 8 }
        "Tab"       { set iso ""; set key 9 }
        "Return"    { set iso ""; set key 10 }
        "Escape"    { set iso ""; set key 27 }
        "Space"     { set iso ""; set key 32 }
        "Delete"    { set iso ""; set key 127 }
        "KP_Delete" { set iso ""; set key 127 }
    }
    if {$iso != ""} {
        scan $iso %c key
    }
    pdsend "[winfo toplevel $mycanvas] key $state $key $shift"
}

# ------------------------------------------------------------------------------
# lost pdtk functions...

# set the checkbox on the "Compute Audio" menuitem and checkbox
proc pdtk_pd_dsp {value} {
    if {$value eq "ON"} {
        #TODO
    } else {
    }
}

proc pdtk_pd_dio {red} {
    # puts stderr [concat pdtk_pd_dio $red]
}


proc pdtk_watchdog {} {
   pdsend "pd watchdog"
   after 2000 {pdtk_watchdog}
}


proc pdtk_ping {} {
    pdsend "pd ping"
}

# ------------------------------------------------------------------------------
# kludges to avoid changing C code

proc .mbar.find {command number} {
    # this should be changed in g_canvas.c, around line 800
    .menubar.find $command $number
}

# ------------------------------------------------------------------------------
# stuff Miller added to get up and running...

proc menu_doc_open {dirname basename} {
    global argv0
    set slashed $argv0
    if {[tk windowingsystem] eq "win32"} {
    	set slashed [string map {"\\" "/"} $slashed]
    }

    set pddir [string range $slashed 0 [expr [string last / $slashed ] - 1]]

    if {[regexp ".*\.(txt|c)$" $basename]} {
        menu_opentext $pddir/../$dirname/$basename
    } elseif {[regexp ".*\.html?$" $basename]} {
                  menu_openhtml $pddir/../$dirname/$basename
    } else {
        pdsend [concat pd open [enquote_path $basename] \
                [enquote_path $pddir/../$dirname] \;]
    }
}

set pd_window_exists 0

proc create_pdwindow {} {
    global pd_window_exists
    set pd_window_exists 1
    wm title . [_ "Pd window"]
    wm geometry . +500+50

    frame .printout
    text .printout.text -relief raised -bd 2 -font console_font \
        -yscrollcommand ".printout.scroll set" -width 80
    # .printout.text insert end "\n\n\n\n\n\n\n\n\n\n"
    scrollbar .printout.scroll -command ".printout.text yview"
    pack .printout.scroll -side right -fill y
    pack .printout.text -side left -fill both -expand 1
    pack .printout -side bottom -fill both -expand 1

    ::pd_menus::create_menubar .menubar .
    . configure -menu .menubar -width 400 -height 250
    ::pd_menus::configure_pdwindow .menubar
    ::pd_bindings::pdwindow_bindings .
}

proc pdtk_post {message} {
    global pd_window_exists
    if {$pd_window_exists} {
        .printout.text insert end $message
        .printout.text yview end-2char
    } else {
        puts stderr $message
    }
}

proc pdtk_standardkeybindings {id} {
    bind $id <Control-Key> {pdtk_pd_ctrlkey %W %K 0}
    bind $id <Control-Shift-Key> {pdtk_pd_ctrlkey %W %K 1}
    if {[tk windowingsystem] eq "win32"} {
        bind $id <Mod1-Key> {pdtk_canvas_ctrlkey %W %K 0}
        bind $id <Mod1-Shift-Key> {pdtk_canvas_ctrlkey %W %K 1}
    }
}

proc pdtk_encodedialog {x} {
    concat +[string map {" " "+_" "$" "+d" ";" "+s" "," "+c" "+" "++"} $x]
}

####################### audio dialog ##################3

proc audio_apply {id} {
    global audio_indev1 audio_indev2 audio_indev3 audio_indev4 
    global audio_inchan1 audio_inchan2 audio_inchan3 audio_inchan4
    global audio_inenable1 audio_inenable2 audio_inenable3 audio_inenable4
    global audio_outdev1 audio_outdev2 audio_outdev3 audio_outdev4 
    global audio_outchan1 audio_outchan2 audio_outchan3 audio_outchan4
    global audio_outenable1 audio_outenable2 audio_outenable3 audio_outenable4
    global audio_sr audio_advance audio_callback

    pdsend [concat pd audio-dialog \
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
        \;]
}

proc audio_cancel {id} {
    pdsend [concat $id cancel \;]
}

proc audio_ok {id} {
    audio_apply $id
    audio_cancel $id
}

# callback from popup menu
proc audio_popup_action {buttonname varname devlist index} {
    global audio_indevlist audio_outdevlist $varname
    $buttonname configure -text [lindex $devlist $index]
#    puts stderr [concat popup_action $buttonname $varname $index]
    set $varname $index
}

# create a popup menu
proc audio_popup {name buttonname varname devlist} {
    if [winfo exists $name.popup] {destroy $name.popup}
    menu $name.popup -tearoff false
    if {[tk windowingsystem] eq "win32"} {
    $name.popup configure -font menuFont
    }
#    puts stderr [concat $devlist ]
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

proc pdtk_audio_dialog {id indev1 indev2 indev3 indev4 \
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
    toplevel $id
    wm title $id {audio}
    wm protocol $id WM_DELETE_WINDOW [concat audio_cancel $id]

    frame $id.buttonframe
    pack $id.buttonframe -side bottom -fill x -pady 2m
    button $id.buttonframe.cancel -text {Cancel}\
        -command "audio_cancel $id"
    button $id.buttonframe.apply -text {Apply}\
        -command "audio_apply $id"
    button $id.buttonframe.ok -text {OK}\
        -command "audio_ok $id"
    button $id.buttonframe.save -text {Save all settings}\
        -command "audio_apply $id \; pdsend \"pd save-preferences\""
    pack $id.buttonframe.cancel $id.buttonframe.apply $id.buttonframe.ok \
        $id.buttonframe.save -side left -expand 1
    
        # sample rate and advance
    frame $id.srf
    pack $id.srf -side top
    
    label $id.srf.l1 -text "sample rate:"
    entry $id.srf.x1 -textvariable audio_sr -width 7
    label $id.srf.l2 -text "delay (msec):"
    entry $id.srf.x2 -textvariable audio_advance -width 4
    pack $id.srf.l1 $id.srf.x1 $id.srf.l2 $id.srf.x2 -side left
    if {$audio_callback >= 0} {
        checkbutton $id.srf.x3 -variable audio_callback \
            -text {use callbacks} -anchor e
        pack $id.srf.x3 -side left
    }
        # input device 1
    frame $id.in1f
    pack $id.in1f -side top

    checkbutton $id.in1f.x0 -variable audio_inenable1 \
        -text {input device 1} -anchor e
    button $id.in1f.x1 -text [lindex $audio_indevlist $audio_indev1] \
        -command [list audio_popup $id $id.in1f.x1 audio_indev1 $audio_indevlist]
    label $id.in1f.l2 -text "channels:"
    entry $id.in1f.x2 -textvariable audio_inchan1 -width 3
    pack $id.in1f.x0 $id.in1f.x1 $id.in1f.l2 $id.in1f.x2 -side left

        # input device 2
    if {$longform && $multi > 1 && [llength $audio_indevlist] > 1} {
        frame $id.in2f
        pack $id.in2f -side top

        checkbutton $id.in2f.x0 -variable audio_inenable2 \
            -text {input device 2} -anchor e
        button $id.in2f.x1 -text [lindex $audio_indevlist $audio_indev2] \
            -command [list audio_popup $id $id.in2f.x1 audio_indev2 \
                $audio_indevlist]
        label $id.in2f.l2 -text "channels:"
        entry $id.in2f.x2 -textvariable audio_inchan2 -width 3
        pack $id.in2f.x0 $id.in2f.x1 $id.in2f.l2 $id.in2f.x2 -side left
    }

        # input device 3
    if {$longform && $multi > 1 && [llength $audio_indevlist] > 2} {
        frame $id.in3f
        pack $id.in3f -side top

        checkbutton $id.in3f.x0 -variable audio_inenable3 \
            -text {input device 3} -anchor e
        button $id.in3f.x1 -text [lindex $audio_indevlist $audio_indev3] \
            -command [list audio_popup $id $id.in3f.x1 audio_indev3 \
                $audio_indevlist]
        label $id.in3f.l2 -text "channels:"
        entry $id.in3f.x2 -textvariable audio_inchan3 -width 3
        pack $id.in3f.x0 $id.in3f.x1 $id.in3f.l2 $id.in3f.x2 -side left
    }

        # input device 4
    if {$longform && $multi > 1 && [llength $audio_indevlist] > 3} {
        frame $id.in4f
        pack $id.in4f -side top

        checkbutton $id.in4f.x0 -variable audio_inenable4 \
            -text {input device 4} -anchor e
        button $id.in4f.x1 -text [lindex $audio_indevlist $audio_indev4] \
            -command [list audio_popup $id $id.in4f.x1 audio_indev4 \
                $audio_indevlist]
        label $id.in4f.l2 -text "channels:"
        entry $id.in4f.x2 -textvariable audio_inchan4 -width 3
        pack $id.in4f.x0 $id.in4f.x1 $id.in4f.l2 $id.in4f.x2 -side left
    }

        # output device 1
    frame $id.out1f
    pack $id.out1f -side top

    checkbutton $id.out1f.x0 -variable audio_outenable1 \
        -text {output device 1} -anchor e
    if {$multi == 0} {
        label $id.out1f.l1 \
            -text "(same as input device) ..............      "
    } else {
        button $id.out1f.x1 -text [lindex $audio_outdevlist $audio_outdev1] \
            -command  [list audio_popup $id $id.out1f.x1 audio_outdev1 \
                $audio_outdevlist]
    }
    label $id.out1f.l2 -text "channels:"
    entry $id.out1f.x2 -textvariable audio_outchan1 -width 3
    if {$multi == 0} {
        pack $id.out1f.x0 $id.out1f.l1 $id.out1f.x2 -side left
    } else {
        pack $id.out1f.x0 $id.out1f.x1 $id.out1f.l2 $id.out1f.x2 -side left
    }

        # output device 2
    if {$longform && $multi > 1 && [llength $audio_outdevlist] > 1} {
        frame $id.out2f
        pack $id.out2f -side top

        checkbutton $id.out2f.x0 -variable audio_outenable2 \
            -text {output device 2} -anchor e
        button $id.out2f.x1 -text [lindex $audio_outdevlist $audio_outdev2] \
            -command \
            [list audio_popup $id $id.out2f.x1 audio_outdev2 $audio_outdevlist]
        label $id.out2f.l2 -text "channels:"
        entry $id.out2f.x2 -textvariable audio_outchan2 -width 3
        pack $id.out2f.x0 $id.out2f.x1 $id.out2f.l2 $id.out2f.x2 -side left
    }

        # output device 3
    if {$longform && $multi > 1 && [llength $audio_outdevlist] > 2} {
        frame $id.out3f
        pack $id.out3f -side top

        checkbutton $id.out3f.x0 -variable audio_outenable3 \
            -text {output device 3} -anchor e
        button $id.out3f.x1 -text [lindex $audio_outdevlist $audio_outdev3] \
            -command \
            [list audio_popup $id $id.out3f.x1 audio_outdev3 $audio_outdevlist]
        label $id.out3f.l2 -text "channels:"
        entry $id.out3f.x2 -textvariable audio_outchan3 -width 3
        pack $id.out3f.x0 $id.out3f.x1 $id.out3f.l2 $id.out3f.x2 -side left
    }

        # output device 4
    if {$longform && $multi > 1 && [llength $audio_outdevlist] > 3} {
        frame $id.out4f
        pack $id.out4f -side top

        checkbutton $id.out4f.x0 -variable audio_outenable4 \
            -text {output device 4} -anchor e
        button $id.out4f.x1 -text [lindex $audio_outdevlist $audio_outdev4] \
            -command \
            [list audio_popup $id $id.out4f.x1 audio_outdev4 $audio_outdevlist]
        label $id.out4f.l2 -text "channels:"
        entry $id.out4f.x2 -textvariable audio_outchan4 -width 3
        pack $id.out4f.x0 $id.out4f.x1 $id.out4f.l2 $id.out4f.x2 -side left
    }

        # if not the "long form" but if "multi" is 2, make a button to
        # restart with longform set. 
    
    if {$longform == 0 && $multi > 1} {
        frame $id.longbutton
        pack $id.longbutton -side top
        button $id.longbutton.b -text {use multiple devices} \
            -command  {pdsend "pd audio-properties 1"}
        pack $id.longbutton.b
    }
    bind $id.srf.x1 <KeyPress-Return> [concat audio_ok $id]
    bind $id.srf.x2 <KeyPress-Return> [concat audio_ok $id]
    bind $id.in1f.x2 <KeyPress-Return> [concat audio_ok $id]
    bind $id.out1f.x2 <KeyPress-Return> [concat audio_ok $id]
    $id.srf.x1 select from 0
    $id.srf.x1 select adjust end
    focus $id.srf.x1
    pdtk_standardkeybindings $id.srf.x1
    pdtk_standardkeybindings $id.srf.x2
    pdtk_standardkeybindings $id.in1f.x2
    pdtk_standardkeybindings $id.out1f.x2
}

####################### midi dialog ##################

proc midi_apply {id} {
    global midi_indev1 midi_indev2 midi_indev3 midi_indev4 
    global midi_outdev1 midi_outdev2 midi_outdev3 midi_outdev4
    global midi_alsain midi_alsaout

    pdsend [concat pd midi-dialog \
        $midi_indev1 \
        $midi_indev2 \
        $midi_indev3 \
        $midi_indev4 \
        $midi_outdev1 \
        $midi_outdev2 \
        $midi_outdev3 \
        $midi_outdev4 \
        $midi_alsain \
        $midi_alsaout \
        \;]
}

proc midi_cancel {id} {
    pdsend [concat $id cancel \;]
}

proc midi_ok {id} {
    midi_apply $id
    midi_cancel $id
}

# callback from popup menu
proc midi_popup_action {buttonname varname devlist index} {
    global midi_indevlist midi_outdevlist $varname
    $buttonname configure -text [lindex $devlist $index]
#    puts stderr [concat popup_action $buttonname $varname $index]
    set $varname $index
}

# create a popup menu
proc midi_popup {name buttonname varname devlist} {
    if [winfo exists $name.popup] {destroy $name.popup}
    menu $name.popup -tearoff false
    if {[tk windowingsystem] eq "win32"} {
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
proc pdtk_midi_dialog {id indev1 indev2 indev3 indev4 \
        outdev1 outdev2 outdev3 outdev4 longform} {
    global midi_indev1 midi_indev2 midi_indev3 midi_indev4 
    global midi_outdev1 midi_outdev2 midi_outdev3 midi_outdev4
    global midi_indevlist midi_outdevlist
    global midi_alsain midi_alsaout

    set midi_indev1 $indev1
    set midi_indev2 $indev2
    set midi_indev3 $indev3
    set midi_indev4 $indev4
    set midi_outdev1 $outdev1
    set midi_outdev2 $outdev2
    set midi_outdev3 $outdev3
    set midi_outdev4 $outdev4
    set midi_alsain [llength $midi_indevlist]
    set midi_alsaout [llength $midi_outdevlist]

    toplevel $id
    wm title $id {midi}
    wm protocol $id WM_DELETE_WINDOW [concat midi_cancel $id]

    frame $id.buttonframe
    pack $id.buttonframe -side bottom -fill x -pady 2m
    button $id.buttonframe.cancel -text {Cancel}\
        -command "midi_cancel $id"
    button $id.buttonframe.apply -text {Apply}\
        -command "midi_apply $id"
    button $id.buttonframe.ok -text {OK}\
        -command "midi_ok $id"
    pack $id.buttonframe.cancel -side left -expand 1
    pack $id.buttonframe.apply -side left -expand 1
    pack $id.buttonframe.ok -side left -expand 1
    
        # input device 1
    frame $id.in1f
    pack $id.in1f -side top

    label $id.in1f.l1 -text "input device 1:"
    button $id.in1f.x1 -text [lindex $midi_indevlist $midi_indev1] \
        -command [list midi_popup $id $id.in1f.x1 midi_indev1 $midi_indevlist]
    pack $id.in1f.l1 $id.in1f.x1 -side left

        # input device 2
    if {$longform && [llength $midi_indevlist] > 2} {
        frame $id.in2f
        pack $id.in2f -side top

        label $id.in2f.l1 -text "input device 2:"
        button $id.in2f.x1 -text [lindex $midi_indevlist $midi_indev2] \
            -command [list midi_popup $id $id.in2f.x1 midi_indev2 \
                $midi_indevlist]
        pack $id.in2f.l1 $id.in2f.x1 -side left
    }

        # input device 3
    if {$longform && [llength $midi_indevlist] > 3} {
        frame $id.in3f
        pack $id.in3f -side top

        label $id.in3f.l1 -text "input device 3:"
        button $id.in3f.x1 -text [lindex $midi_indevlist $midi_indev3] \
            -command [list midi_popup $id $id.in3f.x1 midi_indev3 \
                $midi_indevlist]
        pack $id.in3f.l1 $id.in3f.x1 -side left
    }

        # input device 4
    if {$longform && [llength $midi_indevlist] > 4} {
        frame $id.in4f
        pack $id.in4f -side top

        label $id.in4f.l1 -text "input device 4:"
        button $id.in4f.x1 -text [lindex $midi_indevlist $midi_indev4] \
            -command [list midi_popup $id $id.in4f.x1 midi_indev4 \
                $midi_indevlist]
        pack $id.in4f.l1 $id.in4f.x1 -side left
    }

        # output device 1

    frame $id.out1f
    pack $id.out1f -side top
    label $id.out1f.l1 -text "output device 1:"
    button $id.out1f.x1 -text [lindex $midi_outdevlist $midi_outdev1] \
        -command [list midi_popup $id $id.out1f.x1 midi_outdev1 \
            $midi_outdevlist]
    pack $id.out1f.l1 $id.out1f.x1 -side left

        # output device 2
    if {$longform && [llength $midi_outdevlist] > 2} {
        frame $id.out2f
        pack $id.out2f -side top
        label $id.out2f.l1 -text "output device 2:"
        button $id.out2f.x1 -text [lindex $midi_outdevlist $midi_outdev2] \
            -command \
            [list midi_popup $id $id.out2f.x1 midi_outdev2 $midi_outdevlist]
        pack $id.out2f.l1 $id.out2f.x1 -side left
    }

        # output device 3
    if {$longform && [llength $midi_midi_outdevlist] > 3} {
        frame $id.out3f
        pack $id.out3f -side top
        label $id.out3f.l1 -text "output device 3:"
        button $id.out3f.x1 -text [lindex $midi_outdevlist $midi_outdev3] \
            -command \
            [list midi_popup $id $id.out3f.x1 midi_outdev3 $midi_outdevlist]
        pack $id.out3f.l1 $id.out3f.x1 -side left
    }

        # output device 4
    if {$longform && [llength $midi_midi_outdevlist] > 4} {
        frame $id.out4f
        pack $id.out4f -side top
        label $id.out4f.l1 -text "output device 4:"
        button $id.out4f.x1 -text [lindex $midi_outdevlist $midi_outdev4] \
            -command \
            [list midi_popup $id $id.out4f.x1 midi_outdev4 $midi_outdevlist]
        pack $id.out4f.l1 $id.out4f.x1 -side left
    }

        # if not the "long form" make a button to
        # restart with longform set. 
    
    if {$longform == 0} {
        frame $id.longbutton
        pack $id.longbutton -side top
        button $id.longbutton.b -text {use multiple devices} \
            -command  {pdsend "pd midi-properties 1"}
        pack $id.longbutton.b
    }
}

proc pdtk_alsa_midi_dialog {id indev1 indev2 indev3 indev4 \
        outdev1 outdev2 outdev3 outdev4 longform alsa} {
    global midi_indev1 midi_indev2 midi_indev3 midi_indev4 
    global midi_outdev1 midi_outdev2 midi_outdev3 midi_outdev4
    global midi_indevlist midi_outdevlist
    global midi_alsain midi_alsaout

    set midi_indev1 $indev1
    set midi_indev2 $indev2
    set midi_indev3 $indev3
    set midi_indev4 $indev4
    set midi_outdev1 $outdev1
    set midi_outdev2 $outdev2
    set midi_outdev3 $outdev3
    set midi_outdev4 $outdev4
    set midi_alsain [llength $midi_indevlist]
    set midi_alsaout [llength $midi_outdevlist]
    
    toplevel $id
    wm title $id {midi}
    wm protocol $id WM_DELETE_WINDOW [concat midi_cancel $id]

    frame $id.buttonframe
    pack $id.buttonframe -side bottom -fill x -pady 2m
    button $id.buttonframe.cancel -text {Cancel}\
        -command "midi_cancel $id"
    button $id.buttonframe.apply -text {Apply}\
        -command "midi_apply $id"
    button $id.buttonframe.ok -text {OK}\
        -command "midi_ok $id"
    pack $id.buttonframe.cancel -side left -expand 1
    pack $id.buttonframe.apply -side left -expand 1
    pack $id.buttonframe.ok -side left -expand 1

    frame $id.in1f
    pack $id.in1f -side top

  if {$alsa == 0} {
        # input device 1
    label $id.in1f.l1 -text "input device 1:"
    button $id.in1f.x1 -text [lindex $midi_indevlist $midi_indev1] \
        -command [list midi_popup $id $id.in1f.x1 midi_indev1 $midi_indevlist]
    pack $id.in1f.l1 $id.in1f.x1 -side left

        # input device 2
    if {$longform && [llength $midi_indevlist] > 2} {
        frame $id.in2f
        pack $id.in2f -side top

        label $id.in2f.l1 -text "input device 2:"
        button $id.in2f.x1 -text [lindex $midi_indevlist $midi_indev2] \
            -command [list midi_popup $id $id.in2f.x1 midi_indev2 \
                $midi_indevlist]
        pack $id.in2f.l1 $id.in2f.x1 -side left
    }

        # input device 3
    if {$longform && [llength $midi_indevlist] > 3} {
        frame $id.in3f
        pack $id.in3f -side top

        label $id.in3f.l1 -text "input device 3:"
        button $id.in3f.x1 -text [lindex $midi_indevlist $midi_indev3] \
            -command [list midi_popup $id $id.in3f.x1 midi_indev3 \
                $midi_indevlist]
        pack $id.in3f.l1 $id.in3f.x1 -side left
    }

        # input device 4
    if {$longform && [llength $midi_indevlist] > 4} {
        frame $id.in4f
        pack $id.in4f -side top

        label $id.in4f.l1 -text "input device 4:"
        button $id.in4f.x1 -text [lindex $midi_indevlist $midi_indev4] \
            -command [list midi_popup $id $id.in4f.x1 midi_indev4 \
                $midi_indevlist]
        pack $id.in4f.l1 $id.in4f.x1 -side left
    }

        # output device 1

    frame $id.out1f
    pack $id.out1f -side top
    label $id.out1f.l1 -text "output device 1:"
    button $id.out1f.x1 -text [lindex $midi_outdevlist $midi_outdev1] \
        -command [list midi_popup $id $id.out1f.x1 midi_outdev1 \
            $midi_outdevlist]
    pack $id.out1f.l1 $id.out1f.x1 -side left

        # output device 2
    if {$longform && [llength $midi_outdevlist] > 2} {
        frame $id.out2f
        pack $id.out2f -side top
        label $id.out2f.l1 -text "output device 2:"
        button $id.out2f.x1 -text [lindex $midi_outdevlist $midi_outdev2] \
            -command \
            [list midi_popup $id $id.out2f.x1 midi_outdev2 $midi_outdevlist]
        pack $id.out2f.l1 $id.out2f.x1 -side left
    }

        # output device 3
    if {$longform && [llength $midi_outdevlist] > 3} {
        frame $id.out3f
        pack $id.out3f -side top
        label $id.out3f.l1 -text "output device 3:"
        button $id.out3f.x1 -text [lindex $midi_outdevlist $midi_outdev3] \
            -command \
            [list midi_popup $id $id.out3f.x1 midi_outdev3 $midi_outdevlist]
        pack $id.out3f.l1 $id.out3f.x1 -side left
    }

        # output device 4
    if {$longform && [llength $midi_outdevlist] > 4} {
        frame $id.out4f
        pack $id.out4f -side top
        label $id.out4f.l1 -text "output device 4:"
        button $id.out4f.x1 -text [lindex $midi_outdevlist $midi_outdev4] \
            -command \
            [list midi_popup $id $id.out4f.x1 midi_outdev4 $midi_outdevlist]
        pack $id.out4f.l1 $id.out4f.x1 -side left
    }

        # if not the "long form" make a button to
        # restart with longform set. 
    
    if {$longform == 0} {
        frame $id.longbutton
        pack $id.longbutton -side top
        button $id.longbutton.b -text {use multiple alsa devices} \
            -command  {pdsend "pd midi-properties 1"}
        pack $id.longbutton.b
    }
    }
    if {$alsa} {
        label $id.in1f.l1 -text "In Ports:"
        entry $id.in1f.x1 -textvariable midi_alsain -width 4
        pack $id.in1f.l1 $id.in1f.x1 -side left
        label $id.in1f.l2 -text "Out Ports:"
        entry $id.in1f.x2 -textvariable midi_alsaout -width 4
        pack $id.in1f.l2 $id.in1f.x2 -side left
    }
}

############ pdtk_path_dialog -- dialog window for search path #########

proc path_apply {id} {
    global pd_extrapath pd_verbose
    global pd_path_count
    set pd_path {}

    for {set x 0} {$x < $pd_path_count} {incr x} {
        global pd_path$x
        set this_path [set pd_path$x]
        if {0==[string match "" $this_path]} {
            lappend pd_path [pdtk_encodedialog $this_path]
        }
    }

    pdsend [concat pd path-dialog $pd_extrapath $pd_verbose $pd_path \;]
}

proc path_cancel {id} {
    pdsend [concat $id cancel \;]
}

proc path_ok {id} {
    path_apply $id
    path_cancel $id
}

proc pdtk_path_dialog {id extrapath verbose} {
    global pd_extrapath pd_verbose
    global pd_path
    global pd_path_count

    set pd_path_count [expr [llength $pd_path] + 2]
    if { $pd_path_count < 10 } { set pd_path_count 10 }

    for {set x 0} {$x < $pd_path_count} {incr x} {
        global pd_path$x
        set pd_path$x [lindex $pd_path $x]
    }

    set pd_extrapath $extrapath
    set pd_verbose $verbose
    toplevel $id
    wm title $id {PD search path for patches and other files}
    wm protocol $id WM_DELETE_WINDOW [concat path_cancel $id]

    frame $id.buttonframe
    pack $id.buttonframe -side bottom -fill x -pady 2m
    button $id.buttonframe.cancel -text {Cancel}\
        -command "path_cancel $id"
    button $id.buttonframe.apply -text {Apply}\
        -command "path_apply $id"
    button $id.buttonframe.ok -text {OK}\
        -command "path_ok $id"
    pack $id.buttonframe.cancel -side left -expand 1
    pack $id.buttonframe.apply -side left -expand 1
    pack $id.buttonframe.ok -side left -expand 1
    
    frame $id.extraframe
    pack $id.extraframe -side bottom -fill x -pady 2m
    checkbutton $id.extraframe.extra -text {use standard extensions} \
        -variable pd_extrapath -anchor w 
    checkbutton $id.extraframe.verbose -text {verbose} \
        -variable pd_verbose -anchor w 
    button $id.extraframe.save -text {Save all settings}\
        -command "path_apply $id \; pdsend \"pd save-preferences\""
    pack $id.extraframe.extra $id.extraframe.verbose $id.extraframe.save \
        -side left -expand 1

    for {set x 0} {$x < $pd_path_count} {incr x} {
        entry $id.f$x -textvariable pd_path$x -width 80
        bind $id.f$x <KeyPress-Return> [concat path_ok $id]
        pdtk_standardkeybindings $id.f$x
        pack $id.f$x -side top
    }

    focus $id.f0
}

proc pd_set {var value} {
        global $var
        set $var $value
}

########## pdtk_startup_dialog -- dialog window for startup options #########

proc startup_apply {id} {
    global pd_nort pd_flags
    global pd_startup_count

    set pd_startup {}
    for {set x 0} {$x < $pd_startup_count} {incr x} {
        global pd_startup$x
        set this_startup [set pd_startup$x]
        if {0==[string match "" $this_startup]} {lappend pd_startup [pdtk_encodedialog $this_startup]}
    }

    pdsend [concat pd startup-dialog $pd_nort [pdtk_encodedialog $pd_flags] $pd_startup \;]
}

proc startup_cancel {id} {
    pdsend [concat $id cancel \;]
}

proc startup_ok {id} {
    startup_apply $id
    startup_cancel $id
}

proc pdtk_startup_dialog {id nort flags} {
    global pd_nort pd_flags
    global pd_startup
    global pd_startup_count

    set pd_startup_count [expr [llength $pd_startup] + 2]
    if { $pd_startup_count < 10 } { set pd_startup_count 10 }

    for {set x 0} {$x < $pd_startup_count} {incr x} {
        global pd_startup$x
        set pd_startup$x [lindex $pd_startup $x]
    }

    set pd_nort $nort
    set pd_flags $flags
    toplevel $id
    wm title $id {Pd binaries to load (on next startup)}
    wm protocol $id WM_DELETE_WINDOW [concat startup_cancel $id]

    frame $id.buttonframe
    pack $id.buttonframe -side bottom -fill x -pady 2m
    button $id.buttonframe.cancel -text {Cancel}\
        -command "startup_cancel $id"
    button $id.buttonframe.apply -text {Apply}\
        -command "startup_apply $id"
    button $id.buttonframe.ok -text {OK}\
        -command "startup_ok $id"
    pack $id.buttonframe.cancel -side left -expand 1
    pack $id.buttonframe.apply -side left -expand 1
    pack $id.buttonframe.ok -side left -expand 1
    
    frame $id.flags
    pack $id.flags -side bottom
    label $id.flags.entryname -text {startup flags}
    entry $id.flags.entry -textvariable pd_flags -width 80
    bind $id.flags.entry <KeyPress-Return> [concat startup_ok $id]
    pdtk_standardkeybindings $id.flags.entry
    pack $id.flags.entryname $id.flags.entry -side left

    frame $id.nortframe
    pack $id.nortframe -side bottom -fill x -pady 2m
    if {[tk windowingsystem] ne "win32"} {
        checkbutton $id.nortframe.nort -text {defeat real-time scheduling} \
            -variable pd_nort -anchor w
    }
    button $id.nortframe.save -text {Save all settings}\
        -command "startup_apply $id \; pdsend \"pd save-preferences\""
    if {[tk windowingsystem] ne "win32"} {
        pack $id.nortframe.nort $id.nortframe.save -side left -expand 1
    } else {
        pack $id.nortframe.save -side left -expand 1
    }



    for {set x 0} {$x < $pd_startup_count} {incr x} {
        entry $id.f$x -textvariable pd_startup$x -width 80
        bind $id.f$x <KeyPress-Return> [concat startup_ok $id]
        pdtk_standardkeybindings $id.f$x
        pack $id.f$x -side top
    }

    focus $id.f0
}

########## data-driven dialog -- convert others to this someday? ##########

proc ddd_apply {id} {
    set vid [string trimleft $id .]
    set var_count [concat ddd_count_$vid]
    global $var_count
    set count [eval concat $$var_count]
    set values {}

    for {set x 0} {$x < $count} {incr x} {
        set varname [concat ddd_var_$vid$x]
        global $varname
        lappend values [eval concat $$varname]
    }
    set cmd [concat $id done $values \;]

#    puts stderr $cmd
    pd $cmd
}

proc ddd_cancel {id} {
    set cmd [concat $id cancel \;]
#    puts stderr $cmd
    pd $cmd
}

proc ddd_ok {id} {
    ddd_apply $id
    ddd_cancel $id
}

proc ddd_dialog {id dialogname} {
    global ddd_fields
    set vid [string trimleft $id .]
    set count [llength $ddd_fields]

    set var_count [concat ddd_count_$vid]
    global $var_count
    set $var_count $count

    toplevel $id
    label $id.label -text $dialogname
    pack $id.label -side top
    wm title $id "Pd dialog"
    wm resizable $id 0 0
    wm protocol $id WM_DELETE_WINDOW [concat ddd_cancel $id]

    for {set x 0} {$x < $count} {incr x} {
        set varname [concat ddd_var_$vid$x]
        global $varname
        set fieldname [lindex $ddd_fields $x 0]
        set $varname [lindex $ddd_fields $x 1]
        frame $id.frame$x
        pack $id.frame$x -side top -anchor e
        label $id.frame$x.label -text $fieldname
        entry $id.frame$x.entry -textvariable $varname -width 20
        bind $id.frame$x.entry <KeyPress-Return> [concat ddd_ok $id]
        pdtk_standardkeybindings $id.frame$x.entry
        pack $id.frame$x.entry $id.frame$x.label -side right
    }
            
    frame $id.buttonframe -pady 5
    pack $id.buttonframe -side top -fill x -pady 2
    button $id.buttonframe.cancel -text {Cancel}\
        -command "ddd_cancel $id"
    button $id.buttonframe.apply -text {Apply}\
        -command "ddd_apply $id"
    button $id.buttonframe.ok -text {OK}\
        -command "ddd_ok $id"
    pack $id.buttonframe.cancel $id.buttonframe.apply \
        $id.buttonframe.ok -side left -expand 1

#    $id.params.entry select from 0
#    $id.params.entry select adjust end
#    focus $id.params.entry
}

