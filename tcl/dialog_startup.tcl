
package provide dialog_startup 0.1

package require scrollboxwindow

namespace eval dialog_startup {
    namespace export pdtk_startup_dialog
}

set ::dialog_startup::language ""
set ::dialog_startup::precision_binary ""


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
    if {$::dialog_startup::language eq "default" } {
        set ::dialog_startup::language ""
    }
    set ::pd_i18n::language $::dialog_startup::language
    ::pd_guiprefs::write "gui_language" $::dialog_startup::language
    if { $::dialog_startup::precision_binary != "" } {
        ::pd_guiprefs::write "pdcore_precision_binary" $::dialog_startup::precision_binary
    }
    set ::startup_libraries $new_startup
    pdsend "pd startup-dialog $::sys_defeatrt [pdtk_encodedialog $::sys_flags] [pdtk_encode $::startup_libraries]"
}

# set up the panel with the info from pd
proc ::dialog_startup::pdtk_startup_dialog {mytoplevel defeatrt flags} {
    set ::sys_defeatrt $defeatrt
    if {$flags ne ""} {variable ::sys_flags [subst -nocommands $flags]}

    if {[winfo exists $mytoplevel]} {
        wm deiconify $mytoplevel
        raise $mytoplevel
        focus $mytoplevel
    } else {
        create_dialog $mytoplevel
    }
}

proc ::dialog_startup::fill_frame {frame} {
    # 'frame' is a frame, rather than a toplevel window

    label $frame.restart_required -text [_ "Settings below require a restart of Pd!" ]
    pack $frame.restart_required -side top -fill x

    # scrollbox
    ::scrollbox::make ${frame} $::startup_libraries \
        {} {} \
        [_ "Pd libraries to load on startup"]

    ## GUI options
    labelframe $frame.guiframe -text [_ "GUI options" ]
    set f $frame.guiframe
    pack $f -side top -anchor s -fill x -padx 2m -pady 5

    # language selection
    frame $f.langframe
    set w $f.langframe.language
    menubutton $w -indicatoron 1 -menu $w.menu \
        -text [_ "language" ] \
            -relief raised -highlightthickness 1 -anchor c \
            -direction flush
    menu $w.menu -tearoff 0
    set ::dialog_startup::language [::pd_guiprefs::read "gui_language" ]
    if { $::dialog_startup::language eq "" } {
        set ::dialog_startup::language default
    }
    foreach lang [::pd_i18n::get_available_languages] {
        foreach {langname langcode} $lang {
            $w.menu add radiobutton \
                -label ${langname} -command "$w configure -text \"${langname}\"" \
                -value ${langcode} -variable ::dialog_startup::language
            if { ${langcode} == $::dialog_startup::language } {
                $w configure -text "${langname}"
            }
        }
    }
    pack $w -side right

    set w $f.langframe.langlabel
    label $w -text [_ "Menu language" ]
    pack $w -side left
    pack $f.langframe -side top -anchor w -expand 1


    # precision selection
    set basedir [file normalize [file join [file dirname ${::pdgui::scriptname}] ..]]
    set pdexecs {}
    foreach {bindir exe} {
        bin32 pd32
        bin32 pd
        bin   pd32
        bin   pd} {
        foreach ext {{} .exe .com} {
            set x [file join $basedir $bindir ${exe}${ext}]
            if {[file executable $x]} {
                lappend pdexecs [list [_ "float (32bit)"] 32]
                break
            }
        }
    }
    foreach {bindir exe} {
             bin64 pd64
             bin64 pd
             bin pd64} {
        foreach ext {{} .exe .com} {
            set x [file join $basedir $bindir ${exe}${ext}]
            if {[file executable $x]} {
                lappend pdexecs [list [_ "double (64bit) EXPERIMENTAL"] 64]
                # the next line is just for the translations
                set x [_ "double (64bit)" ]
                break
            }
        }
    }
    set precbin [::pd_guiprefs::read "pdcore_precision_binary" ]
    if {[info exists ::deken::platform(floatsize)]} {
        switch -- ${::deken::platform(floatsize)} {
            64 {
                set precbin 64
            }
            32 {
                set precbin 32
            }
        }
    }

    if { [llength $pdexecs] > 1 } {
        if { $precbin == "" } {
            set precbin [lindex [lindex $pdexecs 0] 1]
        }

        frame $f.floatsize

        set w $f.floatsize.floatsize
        menubutton $w -indicatoron 1 -menu $w.menu \
            -text [_ "float size" ] \
            -relief raised -highlightthickness 1 -anchor c \
            -direction flush

        set var ::dialog_startup::precision_binary
        if { $::host ne "" } {
            $w configure -state disabled
            set var ::dialog_startup::precision_binary_dummy
            set ::dialog_startup::precision_binary_dummy ${::dialog_startup::precision_binary}
        }
        set $var $precbin

        menu $w.menu -tearoff 0
        foreach pdx $pdexecs {
            foreach {precision bin} ${pdx} {break}
            $w.menu add radiobutton \
                -label ${precision} -command "$w configure -text \"${precision}\"" \
                -value ${bin} -variable ${var}
            if { ${bin} == $precbin } {
                $w configure -text "${precision}"
            }
        }
        pack $w -side right
        if { $::host ne "" } {
            $w configure -state disabled
        }


        set w $f.floatsize.label
        label $w -text [_ "Numeric precision of Pd-core" ]
        pack $w -side left
        pack $f.floatsize -side top -anchor w -expand 1
    }

    # Startup options and flags
    labelframe $frame.optionframe -text [_ "Startup options" ]
    pack $frame.optionframe -side top -anchor s -fill x -padx 2m -pady 5

    checkbutton $frame.optionframe.verbose  -anchor w \
        -text [_ "Verbose"] \
        -variable ::sys_verbose
    pack $frame.optionframe.verbose -side top -anchor w -expand 1

    if {$::windowingsystem ne "win32"} {
        checkbutton $frame.optionframe.defeatrt -anchor w \
            -text [_ "Defeat real-time scheduling"] \
            -variable ::sys_defeatrt
        pack $frame.optionframe.defeatrt -side top -anchor w -expand 1
    }

    labelframe $frame.flags -text [_ "Startup flags:" ]
    pack $frame.flags -side top -anchor s -fill x -padx 2m
    entry $frame.flags.entry -textvariable ::sys_flags
    pack $frame.flags.entry -side right -expand 1 -fill x

    ::preferencewindow::entryfocus $frame.flags.entry
    ::preferencewindow::simplefocus $frame.listbox.box
}

proc ::dialog_startup::create_dialog {mytoplevel} {
    ::preferencewindow::create ${mytoplevel} [_ "Pd libraries to load on startup"] {450 300}
    wm withdraw $mytoplevel
    wm resizable $mytoplevel 0 0

    set my [::preferencewindow::add_frame ${mytoplevel} [_ "startup preferences" ]]

    # add widgets
    fill_frame $my
    ::preferencewindow::entryfocus $my.flags.entry $mytoplevel.nb.buttonframe.ok "::dialog_startup::ok $mytoplevel" "::dialog_startup::cancel $mytoplevel"
    ::preferencewindow::simplefocus $my.listbox.box $mytoplevel.nb.buttonframe.ok "::dialog_path::ok $mytoplevel" "::dialog_path::cancel $mytoplevel"

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

# procs for setting variables from the Pd-core
proc ::dialog_startup::set_flags {flags} {
    set ::sys_flags [subst -nocommands -novariables ${flags}]
}
proc ::dialog_startup::set_libraries {libraries} {
    set ::startup_libraries $libraries
}
