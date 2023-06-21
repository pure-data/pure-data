package provide dialog_midi 0.1

package require pdtcl_compat

namespace eval ::dialog_midi:: {
    namespace export pdtk_midi_dialog
    namespace export pdtk_alsa_midi_dialog

    variable referenceconfig ""
}

# reference configuration string (to detect whether the config has changed)
set ::dialog_midi::referenceconfig ""

# the length of the 'currently used' lists is currently hardcoded to 9
# (to match Pd's "midi-dialog")
set ::dialog_midi::max_devices 9

# selected devices
# a value of '0' indicates no-device; a value of '1' is the first valid MIDI device,...
#::dialog_midi::indev1 -- ::dialog_midi::indev${max_devices}
#::dialog_midi::outdev1 -- ::dialog_midi::outdev${max_devices}

## midi devices (available and used)
# ::midi_*devlist: a list of available input devices (type:string)
# ::midi_*devices: currently used input devices (type:index)
# ::midi_*devcechannels: currently used channels per input device (type:int)
# the length of the 'currently used' lists is $max_devices
# '-1' indicates no-device; '0' is the first valid MIDI device...
# (WARNING: this is different from ::dialog_midi::iodevN!!!)
set ::midi_indevlist {}
set ::midi_indevices {-1 -1 -1 -1 -1 -1 -1 -1 -1}
set ::midi_outdevlist {}
set ::midi_outdevices {-1 -1 -1 -1 -1 -1 -1 -1 -1}


####################### midi dialog ##################

proc ::dialog_midi::config2string { } {
    set result {}
    foreach dir {in out} {
        upvar ::midi_${dir}devlist devlist
        upvar ::dialog_midi::ports${dir} ports
        for {set i 1} {$i <= $::dialog_midi::max_devices} {incr i} {
            upvar ::dialog_midi::${dir}dev${i} dev
            if { $dev < 0 || $dev > [llength $devlist] } {
                set dev 0
            }
            lappend result $dev
        }
        set ports [expr $ports + 0]
    }
    lappend result $::dialog_midi::portsin $::dialog_midi::portsout
    return $result
}

proc ::dialog_midi::apply {mytoplevel {force ""}} {
    set config [config2string]
    if { $force ne "" || $config ne $::dialog_midi::referenceconfig} {
        pdsend "pd midi-dialog ${config}"
    }
    set ::dialog_midi::referenceconfig $config
}

proc ::dialog_midi::cancel {mytoplevel} {
    destroy $mytoplevel
}

proc ::dialog_midi::ok {mytoplevel} {
    ::dialog_midi::apply $mytoplevel 1
    ::dialog_midi::cancel $mytoplevel
}

proc ::dialog_midi::fill_frame_device {frame direction index} {
    upvar ::dialog_midi::${direction}dev${index} selected
    upvar ::midi_${direction}devlist devlist

    set x [expr $selected - 1]
    set device [lindex $devlist $x]
    if { "$x" < 0 || $x >= [llength $devlist ] || "${device}" eq "" } {
        set device [format "(%s)" [_ "no device" ]]
    }

    label $frame.l1 -text "${index}:"
    menubutton $frame.x1 -indicatoron 1 -menu $frame.x1.menu \
        -relief raised -highlightthickness 1 -anchor c \
        -direction flush \
        -text ${device}
    menu $frame.x1.menu
    set idx 1
    foreach dev $devlist {
        $frame.x1.menu add radiobutton \
            -label "${dev}" \
            -value ${idx} -variable ::dialog_midi::${direction}dev${index} \
            -command "$frame.x1 configure -text \"${dev}\""
        incr idx
    }
    set dev [_ "(no device)" ]
    $frame.x1.menu add radiobutton \
        -label "$dev" \
        -value 0 -variable ::dialog_midi::${direction}dev${index} \
        -command "$frame.x1 configure -text \"${dev}\""
    pack $frame.l1 -side left
    pack $frame.x1 -side left -fill x -expand 1
}
proc ::dialog_midi::fill_frame_devices {frame direction maxdevs} {
    # side effects:
    ## ro: devlist_midi_${direction}devlist

    upvar ::midi_${direction}devlist devlist
    set numdevs [llength $devlist]
    if { $maxdevs > $numdevs } { set maxdevs $numdevs }
    for {set i 1} {$i <= $maxdevs} {incr i} {
        set w ${frame}.${i}f
        frame $w
        pack $w -side top -fill x
        fill_frame_device $w ${direction} ${i}
    }
    if {$maxdevs < 1} {
        label $frame.label -text [_ "no input devices" ]
        pack $frame.label -side top -fill x
    }
}

proc ::dialog_midi::make_frame_iodevices {frame maxdevs longform} {
    # side effects: none

    if {[winfo exists $frame]} {
        destroy $frame
    }
    frame $frame
    pack  $frame -side top -fill x -anchor n -expand 1

    set showdevs $maxdevs
    if {$longform == 0} {
        set showdevs 1
    }

    # input devices
    labelframe $frame.inputs -text [_ "Input Devices"] -padx 5 -pady 5 -borderwidth 1
    pack $frame.inputs -side top -fill x -pady 5
    ::dialog_midi::fill_frame_devices $frame.inputs in ${showdevs}

    # output devices
    labelframe $frame.outputs -text [_ "Output Devices"] -padx 5 -pady 5 -borderwidth 1
    pack $frame.outputs -side top -fill x -pady 5
    ::dialog_midi::fill_frame_devices $frame.outputs out ${showdevs}

    # If not the "long form" but if "multi" is 2, make a button to
    # restart with longform set.
    if {$longform == 0 && $maxdevs > 1} {
        frame $frame.longbutton
        pack $frame.longbutton -side right -fill x
        button $frame.longbutton.b -text [_ "Use Multiple Devices"] \
            -command  "::dialog_midi::make_frame_iodevices $frame $maxdevs 1"
        pack $frame.longbutton.b -expand 1 -ipadx 10 -pady 5
    }
}

proc ::dialog_midi::make_frame_ports {frame inportsvar outportsvar} {
    if {[winfo exists $frame]} {
        destroy $frame
    }
    frame $frame
    pack  $frame -side top -fill x -anchor n -expand 1

    label $frame.l1 -text [_ "In Ports:"]
    entry $frame.x1 -textvariable $inportsvar -width 4
    pack $frame.l1 $frame.x1 -side left
    label $frame.l2 -text [_ "Out Ports:"]
    entry $frame.x2 -textvariable $outportsvar -width 4
    pack $frame.l2 $frame.x2 -side left
}


proc ::dialog_midi::fill_frame {frame {include_backends 1}} {
    set longform 0
    init_devicevars

    if { $include_backends != 0 && [llength $::midi_apilist] > 1} {
        labelframe $frame.backend -text [_ "MIDI system"] -padx 5 -pady 5 -borderwidth 1
        pack $frame.backend -side top -fill x -pady 5

        menubutton $frame.backend.api -indicatoron 1 -menu $frame.backend.api.menu \
            -relief raised -highlightthickness 1 -anchor c \
            -direction flush \
            -text [_ "MIDI system" ]
        menu $frame.backend.api.menu -tearoff 0

        foreach api $::midi_apilist {
            foreach {api_name api_id} $api {
                $frame.backend.api.menu add radiobutton \
                    -label "${api_name}" \
                    -value ${api_id} -variable ::pd_whichmidiapi \
                    -command {pdsend "pd midi-setapi $::pd_whichmidiapi"}
                if { ${api_id} == ${::pd_whichmidiapi} } {
                    $frame.backend.api configure -text "${api_name}"
                }
            }
        }
        pack $frame.backend.api -side left -fill x -expand 1
    }


    if { $::dialog_midi::portsin > 1 || $::dialog_midi::portsout > 1 } {
        set longform 1
    }

    switch -- $::pd_whichmidiapi {
        1       {::dialog_midi::make_frame_ports $frame.contentf ::dialog_midi::portsin ::dialog_midi::portsout }
        default {::dialog_midi::make_frame_iodevices $frame.contentf $::dialog_midi::max_devices $longform}
    }
}


proc ::dialog_midi::init_devicevars {} {
    foreach direction {in out} {
        # input vars
        upvar ::midi_${direction}devices devices
        # output vars
        upvar ::dialog_midi::ports${direction} ports

        for {set i 1} {$i <= $::dialog_midi::max_devices} {incr i} {
            # output vars
            upvar ::dialog_midi::${direction}dev${i} dev
            set d ""
            if { [llength $devices] > 0 } {
                set d [lindex $devices ${i}-1]
            }
            if { "${d}" eq "" } {
                set dev 0
            } else {
                set dev [expr $d + 1]
            }

        }

        set ports 0
        set i 0
        foreach x $devices {
            if { $x >= 0 } {
                set ports [incr i]
            }
        }
    }
    set ::dialog_midi::referenceconfig [config2string]
}


proc ::dialog_midi::set_configuration {API inputdevices indevs outputdevices  outdevs} {
    set ::pd_whichmidiapi $API

    set ::midi_indevlist ${inputdevices}
    # decrement indices by 1 (and filter out invalid(=negative) values)
    set ::midi_indevices [lmap _ $indevs {set _ [expr int($_) - 1]; if {$_ < 0} continue; set _}]

    set ::midi_outdevlist ${outputdevices}
    set ::midi_outdevices [lmap _ $outdevs {set _ [expr int($_) - 1]; if {$_ < 0} continue; set _}]
}

# start a dialog window to select midi devices.  "longform" asks us to make
# controls for opening several devices; if not, we get an extra button to
# turn longform on and restart the dialog.
# iodev==0 indicates that it is disabled
proc ::dialog_midi::pdtk_midi_dialog {id \
      indev1 indev2 indev3 indev4 indev5 indev6 indev7 indev8 indev9 \
      outdev1 outdev2 outdev3 outdev4 outdev5 outdev6 outdev7 outdev8 outdev9 \
      {longform ignored}} {

    ::dialog_midi::set_configuration ${::pd_whichmidiapi} \
        ${::midi_indevlist} \
        [list $indev1 $indev2 $indev3 $indev4 $indev5 $indev6 $indev7 $indev8 $indev9] \
        ${::midi_outdevlist} \
        [list $outdev1 $outdev2 $outdev3 $outdev4 $outdev5 $outdev6 $outdev7 $outdev8 $outdev9]

    ::dialog_midi::create $id
}

proc ::dialog_midi::create {id} {
    # check if there's already an open gui-preference
    # where we can splat the new midi preferences into
    if {[winfo exists ${::dialog_preferences::midi_frame}]} {
        ::preferencewindow::removechildren ${::dialog_preferences::midi_frame}
        ::dialog_midi::fill_frame ${::dialog_preferences::midi_frame}
        return {}
    }

    # destroy leftover dialogs
    destroy $id

    # create a dialog window
    toplevel $id -class DialogWindow
    wm withdraw $id
    wm title $id [_ "MIDI Settings"]
    wm group $id .
    wm resizable $id 0 0
    wm transient $id
    wm minsize $id 240 260
    $id configure -menu $::dialog_menubar
    $id configure -padx 10 -pady 5
    ::pd_bindings::dialog_bindings $id "midi"

    frame $id.midi
    pack $id.midi -side top -anchor n -fill x -expand 1
    ::dialog_midi::fill_frame $id.midi 0

    # save all settings button
    button $id.saveall -text [_ "Save All Settings"] \
        -command "::dialog_midi::apply $id 1; pdsend \"pd save-preferences\""
    pack $id.saveall -side top -expand 1 -ipadx 10 -pady 5

    # buttons
    frame $id.buttonframe
    pack $id.buttonframe -side top -after $id.saveall -pady 2m
    button $id.buttonframe.cancel -text [_ "Cancel"] \
        -command "::dialog_midi::cancel $id"
    pack $id.buttonframe.cancel -side left -expand 1 -fill x -padx 15 -ipadx 10
    if {$::windowingsystem ne "aqua"} {
        button $id.buttonframe.apply -text [_ "Apply"] \
            -command "::dialog_midi::apply $id 1"
        pack $id.buttonframe.apply -side left -expand 1 -fill x -padx 15 -ipadx 10
    }
    button $id.buttonframe.ok -text [_ "OK"] \
        -command "::dialog_midi::ok $id" -default active
    pack $id.buttonframe.ok -side left -expand 1 -fill x -padx 15 -ipadx 10

    # set focus
    focus $id.buttonframe.ok

    # for focus handling on OSX
    if {$::windowingsystem eq "aqua"} {

        # remove cancel button from focus list since it's not activated on Return
        $id.buttonframe.cancel config -takefocus 0

        # show active focus on multiple device button
        if {[winfo exists $id.longbutton.b]} {
            bind $id.longbutton.b <KeyPress-Return> "$id.longbutton.b invoke"
            bind $id.longbutton.b <FocusIn> "::dialog_midi::unbind_return $id; $id.longbutton.b config -default active"
            bind $id.longbutton.b <FocusOut> "::dialog_midi::rebind_return $id; $id.longbutton.b config -default normal"
        }

        # show active focus on save settings button
        bind $id.saveall <KeyPress-Return> "$id.saveall invoke"
        bind $id.saveall <FocusIn> "::dialog_midi::unbind_return $id; $id.saveall config -default active"
        bind $id.saveall <FocusOut> "::dialog_midi::rebind_return $id; $id.saveall config -default normal"

        # show active focus on the ok button as it *is* activated on Return
        $id.buttonframe.ok config -default normal
        bind $id.buttonframe.ok <FocusIn> "$id.buttonframe.ok config -default active"
        bind $id.buttonframe.ok <FocusOut> "$id.buttonframe.ok config -default normal"

        # since we show the active focus, disable the highlight outline
        if {[winfo exists $id.longbutton.b]} {
            $id.longbutton.b config -highlightthickness 0
        }
        $id.saveall config -highlightthickness 0
        $id.buttonframe.ok config -highlightthickness 0
        $id.buttonframe.cancel config -highlightthickness 0
    }

    # set min size based on widget sizing & pos over pdwindow
    wm minsize $id [winfo reqwidth $id] [winfo reqheight $id]
    position_over_window $id .pdwindow
    raise $id
}

proc ::dialog_midi::pdtk_alsa_midi_dialog {id \
        indev1 indev2 indev3 indev4 \
        outdev1 outdev2 outdev3 outdev4 \
        longform alsa} {
    if {$alsa} {
        set ::pd_whichmidiapi 1
    }
    ::dialog_midi::pdtk_midi_dialog $id \
        $indev1 $indev2 $indev3 $indev4 0 0 0 0 0 \
        $outdev1 $outdev2 $outdev3 $outdev4 0 0 0 0 0 \
        $longform
    if {[winfo exists $id]} {
        wm title $id [_ "ALSA MIDI Settings"]
    }
}

# for focus handling on OSX
proc ::dialog_midi::rebind_return {mytoplevel} {
    bind $mytoplevel <KeyPress-Return> "::dialog_midi::ok $mytoplevel"
    focus $mytoplevel.buttonframe.ok
    return 0
}

# for focus handling on OSX
proc ::dialog_midi::unbind_return {mytoplevel} {
    bind $mytoplevel <KeyPress-Return> break
    return 1
}
