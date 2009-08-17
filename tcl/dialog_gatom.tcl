
package provide dialog_gatom 0.1

package require wheredoesthisgo

namespace eval ::dialog_gatom:: {
	namespace export pdtk_gatom_dialog
}

# hashtable for communicating the position of the radiobuttons (Tk's
# radiobutton widget requires this to be global)
global gatomlabel_position

############ pdtk_gatom_dialog -- run a gatom dialog #########

# dialogs like this one can come up in many copies; but in TK the easiest
# way to get data from an "entry", etc., is to set an associated variable
# name.  This is especially true for grouped "radio buttons".  So we have
# to synthesize variable names for each instance of the dialog.  The dialog
# gets a TK pathname $id, from which it strips the leading "." to make a
# variable suffix $vid.  Then you can get the actual value out by asking for
# [eval concat $$variablename].  There should be an easier way but I don't see
# it yet.

proc ::dialog_gatom::escape {sym} {
    if {[string length $sym] == 0} {
        set ret "-"
    } else {
        if {[string equal -length 1 $sym "-"]} {
            set ret [string replace $sym 0 0 "--"]
        } else {
            set ret [string map {"$" "#"} $sym]
        }
    }
    return [unspace_text $ret]
}

proc ::dialog_gatom::unescape {sym} {
    if {[string equal -length 1 $sym "-"]} {
        set ret [string replace $sym 0 0 ""]
    } else {
        set ret [string map {"#" "$"} $sym]
    }
    return $ret
}

proc gatom_apply {mytoplevel} {
    # TODO kludge!! until a common approach to ::pd_bindings::panel_bindings
    # is sorted out
    ::dialog_gatom::apply $mytoplevel
}

proc ::dialog_gatom::apply {mytoplevel} {
	global gatomlabel_position
	
    pdsend "$mytoplevel param \
                 [$mytoplevel.width.entry get] \
                 [$mytoplevel.limits.lower.entry get] \
                 [$mytoplevel.limits.upper.entry get] \
                 [::dialog_gatom::escape [$mytoplevel.gatomlabel.name.entry get]] \
                 $gatomlabel_position($mytoplevel) \
                 [::dialog_gatom::escape [$mytoplevel.s_r.send.entry get]] \
                 [::dialog_gatom::escape [$mytoplevel.s_r.receive.entry get]]"
}


proc gatom_cancel {mytoplevel} {
    # TODO kludge!! until a common approach to ::pd_bindings::panel_bindings
    # is sorted out
    ::dialog_gatom::cancel $mytoplevel
}

proc ::dialog_gatom::cancel {mytoplevel} {
    pdsend "$mytoplevel cancel"
}


proc gatom_ok {mytoplevel} {
    # TODO kludge!! until a common approach to ::pd_bindings::panel_bindings
    # is sorted out
    ::dialog_gatom::ok $mytoplevel
}
proc ::dialog_gatom::ok {mytoplevel} {
    ::dialog_gatom::apply $mytoplevel
    ::dialog_gatom::cancel $mytoplevel
}

# set up the panel with the info from pd
proc ::dialog_gatom::pdtk_gatom_dialog {mytoplevel initwidth initlower \
        initupper initgatomlabel_position initgatomlabel initsend initreceive} {
    global gatomlabel_position
    set gatomlabel_position($mytoplevel) $initgatomlabel_position

    if {[winfo exists $mytoplevel]} {
	    wm deiconify $mytoplevel
	    raise $mytoplevel
    } else {
	    create_panel $mytoplevel
    }

    $mytoplevel.width.entry insert 0 $initwidth
    $mytoplevel.limits.lower.entry insert 0 $initlower
    $mytoplevel.limits.upper.entry insert 0 $initupper
    if {$initgatomlabel ne "-"} {
	    $mytoplevel.gatomlabel.name.entry insert 0 $initgatomlabel
    }
    set gatomlabel_position($mytoplevel) $initgatomlabel_position
	    if {$initsend ne "-"} {
	    $mytoplevel.s_r.send.entry insert 0 $initsend
    }
    if {$initreceive ne "-"} {
	    $mytoplevel.s_r.receive.entry insert 0 $initreceive
    }
}

proc ::dialog_gatom::create_panel {mytoplevel} {
	global gatomlabel_position

    toplevel $mytoplevel
    wm title $mytoplevel "atom box properties"
    wm resizable $mytoplevel 0 0
	catch { # not all platforms/Tcls versions have these options
		wm attributes $mytoplevel -topmost 1
		#wm attributes $mytoplevel -transparent 1
		#$mytoplevel configure -highlightthickness 1
	}
    wm protocol $mytoplevel WM_DELETE_WINDOW "::dialog_gatom::cancel $mytoplevel"

    ::pd_bindings::panel_bindings $mytoplevel "gatom"

    frame $mytoplevel.width -height 7
    pack $mytoplevel.width -side top
    label $mytoplevel.width.label -text "width"
    entry $mytoplevel.width.entry -width 4
	pack $mytoplevel.width.label $mytoplevel.width.entry -side left

    labelframe $mytoplevel.limits -text "limits" -padx 15 -pady 4 -borderwidth 1 \
        -font highlight_font
    pack $mytoplevel.limits -side top -fill x
    frame $mytoplevel.limits.lower
    pack $mytoplevel.limits.lower -side left
    label $mytoplevel.limits.lower.label -text "lower"
    entry $mytoplevel.limits.lower.entry -width 8
    pack $mytoplevel.limits.lower.label $mytoplevel.limits.lower.entry -side left
    frame $mytoplevel.limits.upper
    pack $mytoplevel.limits.upper -side left
    frame $mytoplevel.limits.upper.spacer -width 20
    label $mytoplevel.limits.upper.label -text "upper"
    entry $mytoplevel.limits.upper.entry -width 8
    pack  $mytoplevel.limits.upper.spacer $mytoplevel.limits.upper.label \
        $mytoplevel.limits.upper.entry -side left

    frame $mytoplevel.spacer1 -height 7
    pack $mytoplevel.spacer1 -side top

    labelframe $mytoplevel.gatomlabel -text "label" -padx 5 -pady 4 -borderwidth 1 \
        -font highlight_font
    pack $mytoplevel.gatomlabel -side top -fill x
    frame $mytoplevel.gatomlabel.name
    pack $mytoplevel.gatomlabel.name -side top
    entry $mytoplevel.gatomlabel.name.entry -width 33
    pack $mytoplevel.gatomlabel.name.entry -side left
    frame $mytoplevel.gatomlabel.radio
    pack $mytoplevel.gatomlabel.radio -side top
    radiobutton $mytoplevel.gatomlabel.radio.left -value 0 -text "left   " \
        -variable gatomlabel_position($mytoplevel) -justify left -takefocus 0
    radiobutton $mytoplevel.gatomlabel.radio.right -value 1 -text "right" \
        -variable gatomlabel_position($mytoplevel) -justify left -takefocus 0
    radiobutton $mytoplevel.gatomlabel.radio.top -value 2 -text "top" \
        -variable gatomlabel_position($mytoplevel) -justify left -takefocus 0
    radiobutton $mytoplevel.gatomlabel.radio.bottom -value 3 -text "bottom" \
        -variable gatomlabel_position($mytoplevel) -justify left -takefocus 0
    pack $mytoplevel.gatomlabel.radio.left -side left -anchor w
    pack $mytoplevel.gatomlabel.radio.right -side right -anchor w
    pack $mytoplevel.gatomlabel.radio.top -side top -anchor w
    pack $mytoplevel.gatomlabel.radio.bottom -side bottom -anchor w

    frame $mytoplevel.spacer2 -height 7
    pack $mytoplevel.spacer2 -side top

    labelframe $mytoplevel.s_r -text "messages" -padx 5 -pady 4 -borderwidth 1 \
        -font highlight_font
    pack $mytoplevel.s_r -side top -fill x
    frame $mytoplevel.s_r.send
    pack $mytoplevel.s_r.send -side top -anchor e
    label $mytoplevel.s_r.send.label -text "send symbol"
    entry $mytoplevel.s_r.send.entry -width 21
    pack $mytoplevel.s_r.send.entry $mytoplevel.s_r.send.label -side right

    frame $mytoplevel.s_r.receive
    pack $mytoplevel.s_r.receive -side top -anchor e
    label $mytoplevel.s_r.receive.label -text "receive symbol"
    entry $mytoplevel.s_r.receive.entry -width 21
    pack $mytoplevel.s_r.receive.entry $mytoplevel.s_r.receive.label -side right
    
    frame $mytoplevel.buttonframe -pady 5
    pack $mytoplevel.buttonframe -side top -fill x -pady 2m
    button $mytoplevel.buttonframe.cancel -text {Cancel} \
        -command "::dialog_gatom::cancel $mytoplevel"
    pack $mytoplevel.buttonframe.cancel -side left -expand 1
    button $mytoplevel.buttonframe.apply -text {Apply} \
        -command "::dialog_gatom::apply $mytoplevel"
    pack $mytoplevel.buttonframe.apply -side left -expand 1
    button $mytoplevel.buttonframe.ok -text {OK} \
        -command "::dialog_gatom::ok $mytoplevel"
    pack $mytoplevel.buttonframe.ok -side left -expand 1

    $mytoplevel.width.entry select from 0
    $mytoplevel.width.entry select adjust end
    focus $mytoplevel.width.entry
}
