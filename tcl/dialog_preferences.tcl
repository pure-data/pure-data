
package provide dialog_preferences 0.1
package require pd_guiprefs
package require pd_i18n
package require preferencewindow

namespace eval ::dialog_preferences:: {
}


proc ::dialog_preferences::cancel {mytoplevel} {
    destroy $mytoplevel
}
proc ::dialog_preferences::do_apply {mytoplevel} {
    ::pd_guiprefs::write "gui_language" $::pd_i18n::language
    pdsend "pd zoom-open $::sys_zoom_open"
}
proc ::dialog_preferences::apply {mytoplevel} {
    ::preferencewindow::apply $mytoplevel
}
proc ::dialog_preferences::ok {mytoplevel} {
    ::preferencewindow::ok $mytoplevel
}

proc ::dialog_preferences::fill_frame {prefs} {
    # patch-window settings
    labelframe $prefs.extraframe -text [_ "Patch windows" ] -padx 5 -pady 5 -borderwidth 1
    checkbutton $prefs.extraframe.zoom -text [_ "Zoom New Windows"] \
        -variable ::sys_zoom_open -anchor w
    pack $prefs.extraframe.zoom -side left -expand 1
    pack $prefs.extraframe -side top -anchor n -fill x
}

proc ::dialog_preferences::create_dialog {{mytoplevel .gui_preferences}} {
    if { [winfo exists $mytoplevel] } {
        destroy $mytoplevel
    }
    ::preferencewindow::create $mytoplevel "Preferences"

    #wm title $mytoplevel "GUI preferences"
    #wm group $mytoplevel .
    #wm withdraw $mytoplevel
    #::pd_bindings::dialog_bindings $mytoplevel "guiprefs"


    # path options
    set prefs [::preferencewindow::add_frame $mytoplevel [_ "path preferences"]]
    ::dialog_path::fill_frame $prefs
    ::preferencewindow::add_apply ${mytoplevel} "::scrollboxwindow::apply ${prefs} ::dialog_path::commit"


    # startup options
    set prefs [::preferencewindow::add_frame $mytoplevel [_ "startup preferences"]]
    ::dialog_startup::fill_frame $prefs
    ::preferencewindow::add_apply ${mytoplevel} "::scrollboxwindow::apply ${prefs} ::dialog_startup::commit"


    # audio options
    set prefs [::preferencewindow::add_frame $mytoplevel  [_ "audio preferences"]]
    ::dialog_audio::fill_frame $prefs
    ::preferencewindow::add_apply ${mytoplevel} "::dialog_audio::apply ${prefs}"


    # midi options
    set prefs [::preferencewindow::add_frame $mytoplevel [_ "MIDI preferences"]]
    ::dialog_midi::fill_frame $prefs
    ::preferencewindow::add_apply ${mytoplevel} "::dialog_midi::apply ${prefs}"

    # deken options
    if {[uplevel 1 info procs ::deken::preferences::fill_frame] ne ""} {
        set prefs [::preferencewindow::add_frame $mytoplevel [_ "deken preferences"]]
        ::deken::preferences::fill_frame $prefs
        ::preferencewindow::add_apply ${mytoplevel} "::deken::preferences::apply ${prefs}"
    }


    # misc options
    set prefs [::preferencewindow::add_frame $mytoplevel [_ "misc preferences"]]
    ::preferencewindow::add_apply $mytoplevel "::dialog_preferences::do_apply ${mytoplevel}"
    ::dialog_preferences::fill_frame $prefs


    ::preferencewindow::add_apply $mytoplevel {pdsend "pd save-preferences"}
    ::pd_bindings::dialog_bindings $mytoplevel "preferences"


    pack $mytoplevel.content

    # re-adjust height based on optional sections
    update
    wm minsize $mytoplevel [winfo width $mytoplevel] [winfo reqheight $mytoplevel]

    raise $mytoplevel
}

proc ::dialog_preferences::create {} {
    ::dialog_preferences::create_dialog
}
