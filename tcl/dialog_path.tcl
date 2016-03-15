
package provide dialog_path 0.1

namespace eval ::dialog_path:: {
    variable use_standard_extensions_button 1
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
    global use_standard_extensions_button
    global verbose_button
    set use_standard_extensions_button $extrapath
    set verbose_button $verbose

    if {[winfo exists $mytoplevel]} {
        wm deiconify $mytoplevel
        raise $mytoplevel
    } else {
        create_dialog $mytoplevel
    }
}

proc ::dialog_path::create_dialog {mytoplevel} {

    scrollboxwindow::make $mytoplevel $::sys_searchpath \
        dialog_path::add dialog_path::edit dialog_path::commit \
        [_ "Pd search path for objects, help, fonts, and other files"] \
        400 300 1
    ::pd_bindings::dialog_bindings $mytoplevel "path"
    
    frame $mytoplevel.extraframe
    pack $mytoplevel.extraframe -side bottom -fill x -pady 2m
    checkbutton $mytoplevel.extraframe.extra -text [_ "Use standard extensions"] \
        -variable use_standard_extensions_button -anchor w 
    checkbutton $mytoplevel.extraframe.verbose -text [_ "Verbose"] \
        -variable verbose_button -anchor w 
    pack $mytoplevel.extraframe.extra -side left -expand 1
    pack $mytoplevel.extraframe.verbose -side right -expand 1

    # focus handling on OSX
    if {$::windowingsystem eq "aqua"} {

        # unbind ok button when in listbox
        bind $mytoplevel.listbox.box <FocusIn> "::dialog_path::unbind_return $mytoplevel"
        bind $mytoplevel.listbox.box <FocusOut> "::dialog_path::rebind_return $mytoplevel"

        # can't see focus for buttons, so disable it
        $mytoplevel.actions.delete_path config -takefocus 0
        $mytoplevel.actions.edit_path config -takefocus 0
        $mytoplevel.actions.add_path config -takefocus 0
        $mytoplevel.extraframe.extra config -takefocus 0
        $mytoplevel.extraframe.verbose config -takefocus 0

        # remove cancel button from focus list since it's not activated on Return
        $mytoplevel.nb.buttonframe.cancel config -takefocus 0

        # show active focus on the ok button as it *is* activated on Return
        $mytoplevel.nb.buttonframe.ok config -default normal
        bind $mytoplevel.nb.buttonframe.ok <FocusIn> "$mytoplevel.nb.buttonframe.ok config -default active"
        bind $mytoplevel.nb.buttonframe.ok <FocusOut> "$mytoplevel.nb.buttonframe.ok config -default normal"
    }
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
proc ::dialog_path::choosePath { currentpath title } {
    if {$currentpath == ""} {
        set currentpath "~"
    }
    return [tk_chooseDirectory -initialdir $currentpath -title $title]
}

proc ::dialog_path::add {} {
    return [::dialog_path::choosePath "" {Add a new path}]
}

proc ::dialog_path::edit { currentpath } {
    return [::dialog_path::choosePath $currentpath "Edit existing path \[$currentpath\]"]
}

proc ::dialog_path::commit { new_path } {
    global use_standard_extensions_button
    global verbose_button

    set ::sys_searchpath $new_path
    pdsend "pd path-dialog $use_standard_extensions_button $verbose_button $::sys_searchpath"
}

