
package provide dialog_path 0.1

namespace eval ::dialog_path:: {
    variable use_standard_paths_button 1
    variable verbose_button 0

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
    global use_standard_paths_button
    global verbose_button
    set use_standard_paths_button $extrapath
    set verbose_button $verbose
    if {[winfo exists $mytoplevel]} {
        # this doesn't seem to be called...
        wm deiconify $mytoplevel
        raise $mytoplevel
        focus $mytoplevel
    } else {
        create_dialog $mytoplevel
    }
}

proc ::dialog_path::create_dialog {mytoplevel} {
    scrollboxwindow::make $mytoplevel $::sys_searchpath \
        dialog_path::add dialog_path::edit dialog_path::commit \
        [_ "Pd search path for objects, help, fonts, and other files"] \
        450 450 1
    ::pd_bindings::dialog_bindings $mytoplevel "path"
    set readonly_color [lindex [$mytoplevel configure -background] end]
    # path options
    frame $mytoplevel.extraframe
    pack $mytoplevel.extraframe -side top -fill x
    checkbutton $mytoplevel.extraframe.extra -text [_ "Use standard paths"] \
        -variable use_standard_paths_button -anchor w
    checkbutton $mytoplevel.extraframe.verbose -text [_ "Verbose"] \
        -variable verbose_button -anchor w
    pack $mytoplevel.extraframe.extra -side left -expand 1
    pack $mytoplevel.extraframe.verbose -side right -expand 1

    # docs path
    labelframe $mytoplevel.docspath -text [_ "Pd Documents Directory"] \
        -borderwidth 1 -padx 5 -pady 5
    pack $mytoplevel.docspath -side top -expand 1 -fill x -padx {2m 4m} -pady 2m

    frame $mytoplevel.docspath.path
    pack $mytoplevel.docspath.path -fill x
    entry $mytoplevel.docspath.path.entry -textvariable ::docspath \
        -takefocus 0 -state readonly -readonlybackground $readonly_color
    button $mytoplevel.docspath.path.browse -text [_ "Browse"] \
        -command "::dialog_path::browse_docspath %W"
    pack $mytoplevel.docspath.path.browse -side right -fill x -ipadx 8
    pack $mytoplevel.docspath.path.entry -side right -expand 1 -fill x

    frame $mytoplevel.docspath.buttons
    pack $mytoplevel.docspath.buttons -fill x
    button $mytoplevel.docspath.buttons.reset -text [_ "Reset"] \
        -command "::dialog_path::reset_docspath %W"
    button $mytoplevel.docspath.buttons.disable -text [_ "Disable"] \
        -command "::dialog_path::disable_docspath %W"
    pack $mytoplevel.docspath.buttons.reset -side left -ipadx 8
    pack $mytoplevel.docspath.buttons.disable -side left -ipadx 8

    # add deken path widgets if deken is available, increase window height to make room
    if {[namespace exists ::deken]} {
        wm geometry $mytoplevel "450x490"
        wm minsize $mytoplevel 450 490
        labelframe $mytoplevel.installpath -text [_ "Externals Install Directory"] \
            -borderwidth 1 -padx 5 -pady 5
        pack $mytoplevel.installpath -expand 1 -fill x -padx {2m 4m} -pady 2m

        frame $mytoplevel.installpath.path
        pack $mytoplevel.installpath.path -fill x
        entry $mytoplevel.installpath.path.entry -textvariable ::deken::installpath \
            -takefocus 0 -state readonly -readonlybackground $readonly_color
        button $mytoplevel.installpath.path.browse -text [_ "Browse"] \
            -command "::dialog_path::browse_installpath %W"
        pack $mytoplevel.installpath.path.browse -side right -fill x -ipadx 8
        pack $mytoplevel.installpath.path.entry -side right -expand 1 -fill x

        frame $mytoplevel.installpath.buttons
        pack $mytoplevel.installpath.buttons -fill x
        button $mytoplevel.installpath.buttons.reset -text [_ "Reset"] \
            -command "::dialog_path::reset_installpath %W"
        pack $mytoplevel.installpath.buttons.reset -side left -ipadx 8
    }

    # focus handling on OSX
    if {$::windowingsystem eq "aqua"} {

        # unbind ok button when in listbox
        bind $mytoplevel.listbox.box <FocusIn> "::dialog_path::unbind_return $mytoplevel"
        bind $mytoplevel.listbox.box <FocusOut> "::dialog_path::rebind_return $mytoplevel"

        # remove cancel button from focus list since it's not activated on Return
        $mytoplevel.nb.buttonframe.cancel config -takefocus 0

        # show active focus on the ok button as it *is* activated on Return
        $mytoplevel.nb.buttonframe.ok config -default normal
        bind $mytoplevel.nb.buttonframe.ok <FocusIn> "$mytoplevel.nb.buttonframe.ok config -default active"
        bind $mytoplevel.nb.buttonframe.ok <FocusOut> "$mytoplevel.nb.buttonframe.ok config -default normal"

        # since we show the active focus, disable the highlight outline
        $mytoplevel.nb.buttonframe.ok config -highlightthickness 0
        $mytoplevel.nb.buttonframe.cancel config -highlightthickness 0
    }
}

# browse for a new Pd user docs path
proc ::dialog_path::browse_docspath {mytoplevel} {
    # set the new docs path
    set newpath [tk_chooseDirectory -initialdir [get_dialog_initialdir true] \
         -title [_ "Choose Pd documents directory:"]]
    if {$newpath ne ""} {
        if {[create_docspath $newpath]} {
            set_docspath $newpath
            return 1
        } else {
            # didn't work
            ::pdwindow::error [format [_ "Couldn't create Pd documents directory: %s"] $newpath]
        }
    }
    return 0
}

# ignore the Pd user docs path
proc ::dialog_path::disable_docspath {mytoplevel} {
    # specific "no docs path please" keyword
    set_docspath "DISABLED"
    return 1
}

# reset to the default Pd user docs path
proc ::dialog_path::reset_docspath {mytoplevel} {
    set path [get_default_docspath]
    if {[create_docspath $path]} {
        set_docspath $path
        return 1
    }
    return 0
}

# browse for a new deken installpath, this assumes deken is available
proc ::dialog_path::browse_installpath {mytoplevel} {
    if {![file isdirectory $::deken::installpath]} {
        set path [get_dialog_initialdir]
    } else {
        set path $::deken::installpath
    }
    set newpath [tk_chooseDirectory -initialdir $path -title [_ "Install externals to directory:"]]
    if {$newpath ne ""} {
        ::deken::set_installpath $newpath
        return 1
    }
    return 0
}

# reset to default deken installpath
proc ::dialog_path::reset_installpath {mytoplevel} {
    set ::deken::installpath ""
    ::deken::set_installpath [::deken::find_installpath]
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
        set currentpath "~"
    }
    return [tk_chooseDirectory -initialdir $currentpath -title $title]
}

proc ::dialog_path::add {} {
    return [::dialog_path::choosePath "" {Add a new path}]
}

proc ::dialog_path::edit {currentpath} {
    return [::dialog_path::choosePath $currentpath "Edit existing path \[$currentpath\]"]
}

proc ::dialog_path::commit {new_path} {
    global use_standard_paths_button
    global verbose_button
    set ::sys_searchpath $new_path
    pdsend "pd path-dialog $use_standard_paths_button $verbose_button [pdtk_encode $::sys_searchpath]"
}
