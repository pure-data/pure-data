
package provide dialog_startup 0.1

package require scrollboxwindow

namespace eval dialog_startup {
    namespace export pdtk_startup_dialog
}

########## pdtk_startup_dialog -- dialog window for startup options #########
# Create a simple modal window with an entry widget
# for editing/adding a startup command
# (the next-best-thing to in-place editing)
proc ::dialog_startup::chooseCommand { prompt initialValue } {
    global cmd
    set cmd $initialValue

    toplevel .inputbox -class DialogWindow
    wm title .inputbox $prompt
    wm group .inputbox .
    wm minsize .inputbox 450 30
    wm resizable .inputbox 0 0
    wm geom .inputbox "450x30"

    button .inputbox.button -text [_ "OK"] -command { destroy .inputbox } \
        -width [::msgcat::mcmax [_ "OK"]]

    entry .inputbox.entry -width 50 -textvariable cmd
    pack .inputbox.button -side right
    bind .inputbox.entry <KeyPress-Return> { destroy .inputbox }
    bind .inputbox.entry <KeyPress-Escape> { destroy .inputbox }
    pack .inputbox.entry -side right -expand 1 -fill x -padx 2m

    raise .inputbox
    focus .inputbox.entry
    wm transient .inputbox
    grab .inputbox
    tkwait window .inputbox

    return $cmd
}

proc ::dialog_startup::cancel {mytoplevel} {
    ::scrollboxwindow::cancel $mytoplevel
}

proc ::dialog_startup::ok {mytoplevel} {
    ::scrollboxwindow::ok $mytoplevel dialog_startup::commit
}

proc ::dialog_startup::add {} {
    return [chooseCommand [_ "Add new library"] ""]
}

proc ::dialog_startup::edit { current_library } {
    return [chooseCommand [_ "Edit library"] $current_library]
}

proc ::dialog_startup::commit { new_startup } {
    ::pd_guiprefs::write "gui_language" $::pd_i18n::language
    set ::startup_libraries $new_startup
    pdsend "pd startup-dialog $::sys_defeatrt [pdtk_encodedialog $::startup_flags] [pdtk_encode $::startup_libraries]"
}

# set up the panel with the info from pd
proc ::dialog_startup::pdtk_startup_dialog {mytoplevel defeatrt flags} {
    set ::sys_defeatrt $defeatrt
    if {$flags ne ""} {variable ::startup_flags [subst -nocommands $flags]}

    if {[winfo exists $mytoplevel]} {
        wm deiconify $mytoplevel
        raise $mytoplevel
        focus $mytoplevel
    } else {
        create_dialog $mytoplevel
    }
}

proc ::dialog_startup::fill_frame {mytoplevel} {
    # 'mytoplevel' is a frame, rather than a toplevel window

    label $mytoplevel.restart_required -text [_ "Settings below require a restart of Pd!" ]
    pack $mytoplevel.restart_required -side top -fill x

    # scrollbox
    ::scrollbox::make ${mytoplevel} $::startup_libraries \
        ::dialog_startup::add dialog_startup::edit \
        [_ "Pd libraries to load on startup"]

    labelframe $mytoplevel.optionframe -text [_ "Startup options" ]
    pack $mytoplevel.optionframe -side top -anchor s -fill x -padx 2m -pady 5

    checkbutton $mytoplevel.optionframe.verbose  -anchor w \
        -text [_ "Verbose"] \
        -variable verbose_button
    pack $mytoplevel.optionframe.verbose -side top -anchor w -expand 1

    if {$::windowingsystem ne "win32"} {
        checkbutton $mytoplevel.optionframe.defeatrt -anchor w \
            -text [_ "Defeat real-time scheduling"] \
            -variable ::sys_defeatrt
        pack $mytoplevel.optionframe.defeatrt -side top -anchor w -expand 1
    }

    # language selection
    frame $mytoplevel.optionframe.langframe
    set w $mytoplevel.optionframe.langframe.language
    menubutton $w -indicatoron 1 -menu $w.menu \
        -text [_ "language" ] \
            -relief raised -highlightthickness 1 -anchor c \
            -direction flush
    menu $w.menu -tearoff 0
    foreach lang [::pd_i18n::get_available_languages] {
        foreach {langname langcode} $lang {
            $w.menu add radiobutton \
                -label ${langname} -command "$w configure -text \"${langname}\"" \
                -value ${langcode} -variable ::pd_i18n::language
            if { ${langcode} == ${::pd_i18n::language} } {
                $w configure -text "${langname}"
            }
        }
    }
    pack $w -side left

    set w $mytoplevel.optionframe.langframe.langlabel
    label $w -text [_ "Menu language" ]
    pack $w -side right
    pack $mytoplevel.optionframe.langframe -side top -anchor w -expand 1



    labelframe $mytoplevel.flags -text [_ "Startup flags:" ]
    pack $mytoplevel.flags -side top -anchor s -fill x -padx 2m
    entry $mytoplevel.flags.entry -textvariable ::startup_flags
    pack $mytoplevel.flags.entry -side right -expand 1 -fill x



    # focus handling on OSX
    if {$::windowingsystem eq "aqua"} {

        # unbind ok button when in listbox
        bind $mytoplevel.listbox.box <FocusIn> "::dialog_startup::unbind_return $mytoplevel"
        bind $mytoplevel.listbox.box <FocusOut> "::dialog_startup::rebind_return $mytoplevel"

        # call apply on Return in entry boxes that are in focus & rebind Return to ok button
        bind $mytoplevel.flags.entry <KeyPress-Return> "::dialog_startup::rebind_return $mytoplevel"

        # unbind Return from ok button when an entry takes focus
        $mytoplevel.flags.entry config -validate focusin -vcmd "::dialog_startup::unbind_return $mytoplevel"
    }

}

proc ::dialog_startup::create_dialog {mytoplevel} {
    ::preferencewindow::create ${mytoplevel} [_ "Pd libraries to load on startup"] {450 300}
    wm withdraw $mytoplevel
    wm resizable $mytoplevel 0 0

    set my [::preferencewindow::add_frame ${mytoplevel} [_ "startup preferences" ]]

    # add widgets
    fill_frame $my
    pack $my -side top -fill x -expand 1

    # add actions
    ::preferencewindow::add_cancel ${mytoplevel} "::scrollboxwindow::cancel ${mytoplevel}"
    ::preferencewindow::add_apply ${mytoplevel} "::scrollboxwindow::apply ${my} ::dialog_startup::commit"

    ::pd_bindings::dialog_bindings $mytoplevel "startup"

    # set min size based on widget sizing
    update
    wm minsize $mytoplevel [winfo width $mytoplevel] [winfo reqheight $mytoplevel]

    position_over_window $mytoplevel .pdwindow
    raise $mytoplevel
}

# for focus handling on OSX
proc ::dialog_startup::rebind_return {mytoplevel} {
    bind $mytoplevel <KeyPress-Escape> "::dialog_startup::cancel $mytoplevel"
    bind $mytoplevel <KeyPress-Return> "::dialog_startup::ok $mytoplevel"
    focus $mytoplevel.nb.buttonframe.ok
    return 0
}

# for focus handling on OSX
proc ::dialog_startup::unbind_return {mytoplevel} {
    bind $mytoplevel <KeyPress-Escape> break
    bind $mytoplevel <KeyPress-Return> break
    return 1
}
