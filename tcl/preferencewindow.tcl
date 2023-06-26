####### preferencewindow -- preferences window with default bindings #########
## This is the base dialog behind the Path and Startup dialogs
##
## This creates a dialog centered on the viewing area of the screen
## with cancel, apply, and OK buttons
## you can then create new frames with `::preferencewindow::add_frame` to put
## preference widgets on.
## when calling `add_frame` multiple times, this will give you tabbed frames
## (on systems that support it)
## you can also register apply/cancel callbacks with the `add_apply` and
## `add_cancel` procs

package provide preferencewindow 0.1
package require pdtcl_compat 0.1

namespace eval ::preferencewindow {
    variable apply_procs
    variable cancel_procs

    catch {namespace import ::pdtcl_compat::dict}
    set apply_procs [dict create]
    set cancel_procs [dict create]
}
# Apply button action
proc ::preferencewindow::apply {winid} {
    # send the data to the GUI
    if {[dict exists ${::preferencewindow::apply_procs} ${winid}]} {
        foreach apply [dict get ${::preferencewindow::apply_procs} ${winid}] {
            eval ${apply}
        }
    }
}

# Cancel button action
proc ::preferencewindow::cancel {winid} {
    # close the window
    if { [dict exists ${::preferencewindow::cancel_procs} ${winid}] } {
        foreach cancel [dict get ${::preferencewindow::cancel_procs} ${winid}] {
            eval ${cancel}
        }
    }
    dict unset ::preferencewindow::cancel_procs ${winid}
    dict unset ::preferencewindow::apply_procs ${winid}
    destroy ${winid}
}

# OK button action
# The "commit" action can take a second or more,
# long enough to be noticeable, so we only write
# the changes after closing the dialog
proc ::preferencewindow::ok {winid} {
    wm withdraw $winid
    apply $winid
    cancel $winid
}

proc ::preferencewindow::mapped {winid} {
    # if there's no support for tabbed notebooks, we must update our scrollregion
    set cnv ${winid}.content.cnv
    catch {
        set bbox [$cnv bbox all]
        if { "$bbox" != "" } {
            foreach {_ _ w _} $bbox {break}
            $cnv configure -scrollregion $bbox -width $w
        }
    }
}

# simple helper to raise an error that can then be 'catch'ed
proc ::preferencewindow::fail {args} {
    return -code error ${args}
}

# "Constructor" function for building the window
# winid -- the window id to use
# title -- top-level title for the dialog
# width, height -- initial width and height dimensions for the window, also minimum size
# resizable -- 0 or 1, set to 1 for dialog to be resizeable
proc ::preferencewindow::create {winid title {dimen {0 0}}} {
    wm deiconify .pdwindow
    raise .pdwindow

    toplevel $winid -class DialogWindow

    wm title $winid $title
    wm group $winid .
    wm transient $winid .pdwindow
    wm protocol $winid WM_DELETE_WINDOW "::preferencewindow::cancel $winid"

    bind $winid <Map> "::preferencewindow::mapped %W"

    foreach {width height} ${dimen} { }
    # Enforce a minimum size for the window
    wm minsize $winid $width $height

    # Set the current dimensions of the window
    # LATER check >0 rather than ==0, and handle the two axes separately
    if { "${width}x${height}" ne "0x0" } {
        wm geometry $winid "${width}x${height}"
    }

    # holds either the tabbed notebook, or a scrollable canvas
    frame ${winid}.content
    pack $winid.content -side top -fill both -expand 1

    # put the preference-widgets on the .content.frames frame
    if { [catch {
        # should/can we do tabs?
        # LATER: put these checks in some common place, where all potential users
        #        of ttk::notebook (preferencewindow, deken,...) can use it
        set wanttabs [::pd_guiprefs::read use_ttknotebook]
        if { ${wanttabs} == 0 } { fail "prefs request to not use ttk::notebook" }
        ::deken::versioncompare 8.6 [info patchlevel]
        if { $::windowingsystem eq "aqua" &&
            [::deken::versioncompare 8.6 [info patchlevel]] < 0 &&
            [::deken::versioncompare 8.6.12 [info patchlevel]] > 0
         } {
            # some versions of Tcl/Tk on macOS just crash with ttk::notebook
            set msg [_ "Disabling tabbed view: incompatible Tcl/Tk detected"]
            ::pdwindow::error "${msg}\n"
            fail "Tcl/Tk request to not use ttk::notebook"
        }
        ttk::notebook ${winid}.content.frames

        catch {ttk::notebook::enableTraversal ${winid}.content.frames}
        pack $winid.content.frames -side top -fill both -pady 2m
    } ] } {
        canvas ${winid}.content.cnv \
            -yscrollincrement 0 \
            -yscrollcommand " ${winid}.content.yscroll set" \
            -xscrollincrement 0 \
            -xscrollcommand " ${winid}.content.xscroll set" \
            -confine true

        frame $winid.content.frames
        scrollbar $winid.content.yscroll -command "${winid}.content.cnv yview"
        scrollbar $winid.content.xscroll -command "${winid}.content.cnv xview" -orient horizontal

        $winid.content.cnv create window 0 0 -anchor nw -window $winid.content.frames

        pack $winid.content.xscroll -side bottom -fill x
        pack $winid.content.yscroll -side right -fill y
        pack $winid.content.cnv -side left -fill both -expand 1
    }


    # Use two frames for the buttons, since we want them both bottom and right
    frame $winid.nb
    pack $winid.nb -side bottom -fill x -pady 2m

    # buttons
    frame $winid.nb.buttonframe
    pack $winid.nb.buttonframe -side right -fill x -padx 2m

    button $winid.nb.buttonframe.cancel -text [_ "Cancel"] \
        -command "::preferencewindow::cancel $winid"
    pack $winid.nb.buttonframe.cancel -side left -expand 1 -fill x -padx 15 -ipadx 10
    if {$::windowingsystem ne "aqua"} {
        button $winid.nb.buttonframe.apply -text [_ "Apply"] \
            -command "::preferencewindow::apply $winid"
        pack $winid.nb.buttonframe.apply -side left -expand 1 -fill x -padx 15 -ipadx 10
    }
    button $winid.nb.buttonframe.ok -text [_ "OK"] \
        -command "::preferencewindow::ok $winid"
    pack $winid.nb.buttonframe.ok -side left -expand 1 -fill x -padx 15 -ipadx 10

    # focus handling on OSX
    if {$::windowingsystem eq "aqua"} {
        # remove cancel button from focus list since it's not activated on Return
        $winid.nb.buttonframe.cancel config -takefocus 0

        # show active focus on the ok button as it *is* activated on Return
        $winid.nb.buttonframe.ok config -default normal
        bind $winid.nb.buttonframe.ok <FocusIn> "$winid.nb.buttonframe.ok config -default active"
        bind $winid.nb.buttonframe.ok <FocusOut> "$winid.nb.buttonframe.ok config -default normal"

        # since we show the active focus, disable the highlight outline
        $winid.nb.buttonframe.ok config -highlightthickness 0
        $winid.nb.buttonframe.cancel config -highlightthickness 0
    }

    return $winid
}

proc ::preferencewindow::add_frame {winid subtitle} {
    set i 0
    while {[winfo exists ${winid}.content.frames.f${i}]} {incr i}
    set frame ${winid}.content.frames.f${i}
    labelframe ${frame} -borderwidth 0 -padx 5 -pady 5
    if { [catch {
        ${winid}.content.frames add ${frame} -text "${subtitle}"
    } ] } {
        ${frame} configure -text "${subtitle}" -borderwidth 1 -relief solid
        pack ${frame} -side top -fill x -padx 2m -pady 2m
    }

    ::preferencewindow::mapped $winid
    return ${frame}
}
proc ::preferencewindow::add_apply {winid apply_proc} {
    if { {} ne ${apply_proc} } {
        dict lappend ::preferencewindow::apply_procs $winid ${apply_proc}
    }
}
proc ::preferencewindow::add_cancel {winid cancel_proc} {
    if { {} ne ${cancel_proc} } {
        dict lappend ::preferencewindow::cancel_procs $winid ${cancel_proc}
    }
}

proc ::preferencewindow::select {winid id} {
    catch { ${winid}.content.frames select $id }
    return {}
}
proc ::preferencewindow::selected {winid} {
    if { [ catch { set id [ ${winid}.content.frames index current ] } ] } {
        set id ""
    }
    return $id
}



# ############## helpers ###############

proc ::preferencewindow::removechildren {id} {
    if {[winfo exists ${id}]} {
        set children [winfo children ${id}]
        foreach c ${children} {
            destroy $c
        }
    }
}

if {[tk windowingsystem] eq "aqua"} {
    proc ::preferencewindow::simplefocus {id {okbutton {}} {okaction {}} {cancelaction {}}} {
        set mytoplevel [winfo toplevel $id]
        if { "$okbutton" eq "" } {
            set okbutton $mytoplevel.nb.buttonframe.ok
        }
        if { "$okaction" eq "" } {
            set okaction "::preferencewindow::ok ${mytoplevel}"
        }
        if { "$cancelaction" eq "" } {
            set cancelaction "::preferencewindow::cancel ${mytoplevel}"
        }

        bind $id <FocusIn> "::preferencewindow::unbind_return $mytoplevel"
        bind $id <FocusOut> "::preferencewindow::rebind_return $mytoplevel $okbutton \"$okaction\" \"$cancelaction\""
    }
    proc ::preferencewindow::buttonfocus {id {okbutton {}} {okaction {}} {cancelaction {}}} {
        bind $id <KeyPress-Return> "$id invoke"
        ::preferencewindow::simplefocus $id $okbutton $okaction $cancelaction
        set cmd "$id config -default active"
        bind $id <FocusIn> +$cmd
        set cmd "$id config -default normal"
        bind $id <FocusOut> +$cmd
        $id config -highlightthickness 0
    }
    proc ::preferencewindow::entryfocus {id {okbutton {}} {okaction {}} {cancelaction {}}} {
        set mytoplevel [winfo toplevel $id]
        if { "$okbutton" eq "" } {
            set okbutton $mytoplevel.nb.buttonframe.ok
        }
        if { "$okaction" eq "" } {
            set okaction "::preferencewindow::ok ${mytoplevel}"
        }
        if { "$cancelaction" eq "" } {
            set cancelaction "::preferencewindow::cancel ${mytoplevel}"
        }

        # call apply on Return in entry boxes that are in focus & rebind Return to ok button
        bind $id <KeyPress-Return> "::preferencewindow::rebind_return $mytoplevel $okbutton \"$okaction\" \"cancelaction\""
        # unbind Return from ok button when an entry takes focus
        $id config -validate focusin -vcmd "::preferencewindow::unbind_return $mytoplevel"
    }

    # for focus handling on OSX
    proc ::preferencewindow::rebind_return {mytoplevel {okbutton {}} {okaction {}} {cancelaction {}}} {
        if { "$okbutton" eq "" } {
            set okbutton $mytoplevel.nb.buttonframe.ok
        }
        if { "$okaction" eq "" } {
            set okaction "::preferencewindow::ok ${mytoplevel}"
        }
        if { "$cancelaction" eq "" } {
            set cancelaction "::preferencewindow::cancel ${mytoplevel}"
        }

        bind $mytoplevel <KeyPress-Return> $okaction
        bind $mytoplevel <KeyPress-Escape> $cancelaction
        if {[winfo exists $okbutton]} {
            focus $okbutton
        }
        return 0
    }

    # for focus handling on OSX
    proc ::preferencewindow::unbind_return {mytoplevel} {
        bind $mytoplevel <KeyPress-Return> break
        bind $mytoplevel <KeyPress-Escape> break
        return 1
    }
} else {
    proc ::preferencewindow::simplefocus {id {okbutton {}} {okaction {}} {cancelaction {}}} {
    }
    proc ::preferencewindow::buttonfocus {id {okbutton {}} {okaction {}} {cancelaction {}}} {
    }
    proc ::preferencewindow::entryfocus {id {okbutton {}} {okaction {}} {cancelaction {}}} {
    }
    proc ::preferencewindow::rebind_return {mytoplevel {okbutton {}} {okaction {}} {cancelaction {}}} {
    }
    proc ::preferencewindow::unbind_return {mytoplevel} {
    }

}
