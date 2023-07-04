
package provide dialog_path 0.1

package require scrollboxwindow

namespace eval ::dialog_path:: {
    variable use_standard_paths_button 1
    variable verbose_button 0
    variable docspath ""
    variable installpath ""
    namespace export pdtk_path_dialog
}

############ pdtk_path_dialog -- run a path dialog #########

proc ::dialog_path::cancel {mytoplevel} {
    ::scrollboxwindow::cancel $mytoplevel
}

proc ::dialog_path::ok {mytoplevel} {
    ::scrollboxwindow::ok $mytoplevel dialog_path::commit
}

# set up the panel with the info from pd
proc ::dialog_path::pdtk_path_dialog {mytoplevel extrapath verbose} {
    set ::sys_use_stdpath $extrapath
    set ::sys_verbose $verbose
    if {[winfo exists $mytoplevel]} {
        # this doesn't seem to be called...
        wm deiconify $mytoplevel
        raise $mytoplevel
        focus $mytoplevel
    } else {
        create_dialog $mytoplevel
    }
}


proc ::dialog_path::fill_frame {frame {with_verbose 0}} {
    # 'frame' is a frame, rather than a toplevel window
    set readonly_color [lindex [$frame configure -background] end]

    if {[namespace exists ::deken]} {set ::dialog_path::installpath $::deken::installpath}

    # scrollbox
    ::scrollbox::make ${frame} $::sys_searchpath \
        ::dialog_path::add dialog_path::edit \
        [_ "Pd search path for objects, help, audio, text and other files"]

    # path options
    frame $frame.extraframe
    pack $frame.extraframe -side top -anchor s -fill x
    checkbutton $frame.extraframe.extra -text [_ "Use standard paths"] \
        -variable ::sys_use_stdpath -anchor w
    pack $frame.extraframe.extra -side left -expand 1
    if { ${with_verbose} ne 0 } {
        checkbutton $frame.extraframe.verbose -text [_ "Verbose"] \
            -variable ::sys_verbose -anchor w
        pack $frame.extraframe.verbose -side right -expand 1
    }

    # add docsdir path widgets if pd_docsdir is loaded
    if {[namespace exists ::pd_docsdir]} {
        set ::dialog_path::docspath $::pd_docsdir::docspath
        labelframe $frame.docspath -text [_ "Pd Documents Directory"] \
            -borderwidth 1 -padx 5 -pady 5
        pack $frame.docspath -side top -anchor s -fill x -padx {2m 4m} -pady 2m

        frame $frame.docspath.path
        pack $frame.docspath.path -fill x
        entry $frame.docspath.path.entry -textvariable ::dialog_path::docspath \
            -takefocus 0 -state readonly -readonlybackground $readonly_color
        button $frame.docspath.path.browse -text [_ "Browse"] \
            -command "::dialog_path::browse_docspath $frame"
        pack $frame.docspath.path.browse -side right -fill x -ipadx 8
        pack $frame.docspath.path.entry -side right -expand 1 -fill x

        frame $frame.docspath.buttons
        pack $frame.docspath.buttons -fill x
        button $frame.docspath.buttons.reset -text [_ "Reset"] \
            -command "::dialog_path::reset_docspath $frame"
        button $frame.docspath.buttons.disable -text [_ "Disable"] \
            -command "::dialog_path::disable_docspath $frame"
        pack $frame.docspath.buttons.reset -side left -ipadx 8
        pack $frame.docspath.buttons.disable -side left -ipadx 8

        # scroll to right for long paths
        $frame.docspath.path.entry xview moveto 1
    }

    # add deken path widgets if deken is loaded
    if {[namespace exists ::deken]} {
        labelframe $frame.installpath -text [_ "Externals Install Directory"] \
            -borderwidth 1 -padx 5 -pady 5
        pack $frame.installpath -fill x -anchor s -padx {2m 4m} -pady 2m

        frame $frame.installpath.path
        pack $frame.installpath.path -fill x
        entry $frame.installpath.path.entry -textvariable ::dialog_path::installpath \
            -takefocus 0 -state readonly -readonlybackground $readonly_color
        button $frame.installpath.path.browse -text [_ "Browse"] \
            -command "::dialog_path::browse_installpath $frame"
        pack $frame.installpath.path.browse -side right -fill x -ipadx 8
        pack $frame.installpath.path.entry -side right -expand 1 -fill x

        frame $frame.installpath.buttons
        pack $frame.installpath.buttons -fill x
        button $frame.installpath.buttons.reset -text [_ "Reset"] \
            -command "::dialog_path::reset_installpath $frame"
        button $frame.installpath.buttons.clear -text [_ "Clear"] \
            -command "::dialog_path::clear_installpath $frame"
        pack $frame.installpath.buttons.reset -side left -ipadx 8
        pack $frame.installpath.buttons.clear -side left -ipadx 8

        # scroll to right for long paths
        $frame.installpath.path.entry xview moveto 1
    }
    ::preferencewindow::simplefocus $frame.listbox.box
    return
}

proc ::dialog_path::create_dialog {mytoplevel} {
    ::preferencewindow::create ${mytoplevel} [_ "Pd search path for objects, help, audio, text and other files"] {450 300}
    wm withdraw $mytoplevel

    set my [::preferencewindow::add_frame ${mytoplevel}  [_ "path preferences"]]

    # add widgets
    fill_frame $my 1
    # focus handling on OSX
    ::preferencewindow::simplefocus $my.listbox.box $mytoplevel.nb.buttonframe.ok "::dialog_path::ok $mytoplevel" "::dialog_path::cancel $mytoplevel"
    if {$::windowingsystem eq "aqua"} {
        # unbind ok button when in listbox
        bind $my.listbox.box <FocusIn> "::dialog_path::unbind_return $mytoplevel"
        bind $my.listbox.box <FocusOut> "::dialog_path::rebind_return $mytoplevel"
    }

    pack $my -side top -fill x -expand 1

    # add actions
    ::preferencewindow::add_cancel ${mytoplevel} "::scrollboxwindow::cancel ${mytoplevel}"
    ::preferencewindow::add_apply ${mytoplevel} "::scrollboxwindow::apply ${my} ::dialog_path::commit"

    ::pd_bindings::dialog_bindings $mytoplevel "path"

    # re-adjust height based on optional sections
    update
    wm minsize $mytoplevel [winfo width $mytoplevel] [winfo reqheight $mytoplevel]

    position_over_window $mytoplevel .pdwindow
    raise $mytoplevel
}

# browse for a new Pd user docs path
proc ::dialog_path::browse_docspath {mytoplevel} {
    # set the new docs dir
    set newpath [tk_chooseDirectory -initialdir $::env(HOME) \
                                    -title [_ "Choose Pd documents directory:"]]
    if {$newpath ne ""} {
        set ::dialog_path::docspath $newpath
        set ::dialog_path::installpath [::pd_docsdir::get_externals_path "$::dialog_path::docspath"]
        $mytoplevel.docspath.path.entry xview moveto 1
        return 1
    }
    return 0
}

# ignore the Pd user docs path
proc ::dialog_path::disable_docspath {mytoplevel} {
    set ::dialog_path::docspath [::pd_docsdir::get_disabled_path]
    return 1
}

# reset to the default Pd user docs path
proc ::dialog_path::reset_docspath {mytoplevel} {
    set ::dialog_path::docspath [::pd_docsdir::get_default_path]
    set ::dialog_path::installpath [::pd_docsdir::get_externals_path "$::dialog_path::docspath"]
    $mytoplevel.docspath.path.entry xview moveto 1
    return 1
}

# browse for a new deken installpath, this assumes deken is available
proc ::dialog_path::browse_installpath {mytoplevel} {
    if {![file isdirectory $::dialog_path::installpath]} {
        set initialdir $::env(HOME)
    } else {
        set initialdir $::dialog_path::installpath
    }
    set newpath [tk_chooseDirectory -initialdir $initialdir \
                                    -title [_ "Install externals to directory:"]]
    if {$newpath ne ""} {
        set ::dialog_path::installpath $newpath
        $mytoplevel.installpath.path.entry xview moveto 1
        return 1
    }
    return 0
}

# reset to default deken installpath
proc ::dialog_path::reset_installpath {mytoplevel} {
    set ::dialog_path::installpath [::deken::find_installpath true]
    $mytoplevel.installpath.path.entry xview moveto 1
}

# clear the deken installpath
proc ::dialog_path::clear_installpath {mytoplevel} {
    set ::dialog_path::installpath ""
}

# for focus handling on OSX
proc ::dialog_path::rebind_return {mytoplevel} {
    bind $mytoplevel <KeyPress-Escape> "::dialog_path::cancel $mytoplevel"
    bind $mytoplevel <KeyPress-Return> "::dialog_path::ok $mytoplevel"
    focus $mytoplevel.nb.buttonframe.ok
    return 0
}

# for focus handling on OSX
proc ::dialog_path::unbind_return {mytoplevel} {
    bind $mytoplevel <KeyPress-Escape> break
    bind $mytoplevel <KeyPress-Return> break
    return 1
}

############ pdtk_path_dialog -- dialog window for search path #########
proc ::dialog_path::choosePath {currentpath title} {
    if {$currentpath == ""} {
        set currentpath $::env(HOME)
    }
    return [tk_chooseDirectory -initialdir $currentpath -title $title]
}

proc ::dialog_path::add {} {
    return [::dialog_path::choosePath "" [_ "Add a new path"]]
}

proc ::dialog_path::edit {currentpath} {
    return [::dialog_path::choosePath $currentpath [format [_ "Edit existing path \[%s\]" ] $currentpath] ]
}

proc ::dialog_path::commit {new_path} {
    # save buttons and search paths
    set changed false
    if {"$new_path" ne "$::sys_searchpath"} {set changed true}
    set ::sys_searchpath $new_path
    pdsend "pd path-dialog $::sys_use_stdpath $::sys_verbose [pdtk_encode $::sys_searchpath]"
    if {$changed} {::helpbrowser::refresh}

    # save installpath
    if {[namespace exists ::deken]} {
        # clear first so set_installpath doesn't pick up prev value from guiprefs
        set ::deken::installpath ""
        ::deken::set_installpath $::dialog_path::installpath
    }

    # save docspath
    if {[namespace exists ::pd_docsdir]} {
        # run this after since it checks ::deken::installpath value
        ::pd_docsdir::update_path $::dialog_path::docspath
    }
}


# procs for setting variables from the Pd-core
proc ::dialog_path::set_paths {searchpath temppath staticpath} {
    set ::sys_searchpath $searchpath
    set ::sys_temppath $temppath
    set ::sys_staticpath $staticpath
}
