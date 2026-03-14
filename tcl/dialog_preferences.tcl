
package provide dialog_preferences 0.1
package require pd_guiprefs
package require pd_i18n
package require preferencewindow

namespace eval ::dialog_preferences:: {
}

namespace eval ::category_menu {}

set ::dialog_preferences::use_ttknotebook {}
after idle ::dialog_preferences::read
# allow updating the audio resp MIDI frame if the backend changes
set ::dialog_preferences::audio_frame {}
set ::dialog_preferences::midi_frame {}
set ::dialog_preferences::gui_color_radio 0
set ::dialog_preferences::gui_colors_changed 0


proc ::dialog_preferences::cancel {mytoplevel} {
    set ::dialog_preferences::audio_frame {}
    set ::dialog_preferences::midi_frame {}
    destroy $mytoplevel
}
proc ::dialog_preferences::do_applycolors {} {
    set saved_bg [::pd_guiprefs::read color_bg]
    set saved_fg [::pd_guiprefs::read color_fg]
    set saved_sel [::pd_guiprefs::read color_sel]
    set saved_gop [::pd_guiprefs::read color_gop]
    set changed 0
    if {$saved_bg ne $::dialog_preferences::gui_color_bg} { set changed 1 }
    if {$saved_fg ne $::dialog_preferences::gui_color_fg} { set changed 1 }
    if {$saved_sel ne $::dialog_preferences::gui_color_sel} { set changed 1 }
    if {$saved_gop ne $::dialog_preferences::gui_color_gop} { set changed 1 }

    ::pd_guiprefs::write "color_bg" $::dialog_preferences::gui_color_bg
    ::pd_guiprefs::write "color_fg" $::dialog_preferences::gui_color_fg
    ::pd_guiprefs::write "color_sel" $::dialog_preferences::gui_color_sel
    ::pd_guiprefs::write "color_gop" $::dialog_preferences::gui_color_gop

    if {$changed} {
        pdsend "pd colors $::dialog_preferences::gui_color_fg $::dialog_preferences::gui_color_bg $::dialog_preferences::gui_color_sel $::dialog_preferences::gui_color_gop"
    }
}
proc ::dialog_preferences::do_apply {mytoplevel} {
    ::pd_guiprefs::write "gui_language" $::pd_i18n::language
    ::pd_guiprefs::write "use_ttknotebook" $::dialog_preferences::use_ttknotebook
    ::pd_guiprefs::write "cords_to_foreground" $::pdtk_canvas::enable_cords_to_foreground
    pdsend "pd zoom-open $::sys_zoom_open"
    ::dialog_preferences::do_applycolors
}
proc ::dialog_preferences::apply_colors_if_mac {} {
    if {$::windowingsystem eq "aqua"} {
        ::dialog_preferences::do_applycolors
    }
}
proc ::dialog_preferences::apply {mytoplevel} {
    ::preferencewindow::apply $mytoplevel
}
proc ::dialog_preferences::ok {mytoplevel} {
    ::preferencewindow::ok $mytoplevel
}
proc ::dialog_preferences::read {} {
    set ::dialog_preferences::gui_color_bg [::pd_guiprefs::read color_bg]
    if {$::dialog_preferences::gui_color_bg eq ""} {
        set ::dialog_preferences::gui_color_bg  "#FFFFFF"
    }
    set ::dialog_preferences::gui_color_fg [::pd_guiprefs::read color_fg]
    if {$::dialog_preferences::gui_color_fg eq ""} {
        set ::dialog_preferences::gui_color_fg  "#000000"
    }
    set ::dialog_preferences::gui_color_sel [::pd_guiprefs::read color_sel]
    if {$::dialog_preferences::gui_color_sel eq ""} {
        set ::dialog_preferences::gui_color_sel  "#0000FF"
    }
    set ::dialog_preferences::gui_color_gop [::pd_guiprefs::read color_gop]
    if {$::dialog_preferences::gui_color_gop eq ""} {
        set ::dialog_preferences::gui_color_gop  "#FF0000"
    }
    set x [::pd_guiprefs::read cords_to_foreground]
    if {[catch { if { $x } { set x 1 } else { set x 0 } }]} {
        set x 0
    }
    set ::pdtk_canvas::enable_cords_to_foreground $x
    ::dialog_preferences::read_usettknotebook
}
proc ::dialog_preferences::read_usettknotebook {} {
    set ::dialog_preferences::use_ttknotebook [::pd_guiprefs::read use_ttknotebook]
}
proc ::dialog_preferences::write_usettknotebook {} {
    ::pd_guiprefs::write "use_ttknotebook" $::dialog_preferences::use_ttknotebook
}

proc ::dialog_preferences::tab_changed {mytoplevel} {
    set tab [::preferencewindow::selected $mytoplevel]
    ::pd_guiprefs::write preferences_tab $tab
}

proc ::dialog_preferences::fill_frame {prefs} {
    # patch-window settings
    labelframe $prefs.extraframe -text [_ "Patch Windows" ] -padx 5 -pady 5 -borderwidth 1
    checkbutton $prefs.extraframe.zoom -text [_ "Zoom New Windows"] \
        -variable ::sys_zoom_open -anchor w
    pack $prefs.extraframe.zoom -side left -expand 1
    pack $prefs.extraframe -side top -anchor n -fill x

    labelframe $prefs.guiframe -text [_ "GUI Settings" ] -padx 5 -pady 5 -borderwidth 1
    pack $prefs.guiframe -side top -anchor n -fill x

    labelframe $prefs.guiframe.prefsnavi -padx 5 -pady 5 -borderwidth 0 \
        -text [_ "Preference layout (reopen the preferences to see the effect)" ]
    pack $prefs.guiframe.prefsnavi -side top -anchor w -expand 1
    radiobutton $prefs.guiframe.prefsnavi.tab \
        -text [_ "Use tabs" ] \
        -value 1 -variable ::dialog_preferences::use_ttknotebook
    radiobutton $prefs.guiframe.prefsnavi.scroll \
        -text [_ "Single page" ] \
        -value 0 -variable ::dialog_preferences::use_ttknotebook
    pack $prefs.guiframe.prefsnavi.tab -side left -anchor w
    pack $prefs.guiframe.prefsnavi.scroll -side left -anchor w

    labelframe $prefs.guiframe.patching -padx 5 -pady 5 -borderwidth 0 \
        -text [_ "Patching helpers" ]
    pack $prefs.guiframe.patching -side top -anchor w -expand 1
    checkbutton $prefs.guiframe.patching.highlight_connections -text [_ "Highlight active cord while connecting"] \
        -variable ::pdtk_canvas::enable_cords_to_foreground -anchor w
    pack $prefs.guiframe.patching.highlight_connections -side left -anchor w

    # Colors section
    labelframe $prefs.guiframe.colors -padx 5 -pady 5 -borderwidth 0 \
        -text [_ "Colors" ]
    pack $prefs.guiframe.colors -side top -anchor w -expand 1 -fill x

    # Row 1: Dropdown menu and Compose button
    frame $prefs.guiframe.colors.toprow
    pack $prefs.guiframe.colors.toprow -side top -fill x -pady 5
    label $prefs.guiframe.colors.toprow.leftspacer -text "" -width 2
    pack $prefs.guiframe.colors.toprow.leftspacer -side left -expand 1
    label $prefs.guiframe.colors.toprow.lab -text [_ "Presets:"]
    menubutton $prefs.guiframe.colors.toprow.mb \
        -text "Default" \
        -menu $prefs.guiframe.colors.toprow.mb.menu \
        -width 12 -relief raised
    menu $prefs.guiframe.colors.toprow.mb.menu -tearoff 0
    $prefs.guiframe.colors.toprow.mb configure \
        -menu $prefs.guiframe.colors.toprow.mb.menu
    set presets { "Default" "Dark Mode" "Light Mode" "Pastel" "Dark Contrasted" "Purple Haze" "Misty Rose" "C64" "Strongbad" "Solarized" "Solarized Inverted"}
    foreach preset $presets {
        $prefs.guiframe.colors.toprow.mb.menu add command \
            -label $preset \
            -command "::dialog_preferences::apply_color_preset $prefs \"$preset\""
    }
    pack $prefs.guiframe.colors.toprow.lab \
         $prefs.guiframe.colors.toprow.mb \
         -side left
    label $prefs.guiframe.colors.toprow.spacer -text "" -width 2
    pack $prefs.guiframe.colors.toprow.spacer -side left
    button $prefs.guiframe.colors.toprow.compose -text [_ "Compose Color"] \
        -command "::dialog_preferences::gui_compose_color $prefs"
    pack $prefs.guiframe.colors.toprow.compose -side left
    label $prefs.guiframe.colors.toprow.rightspacer -text "" -width 2
    pack $prefs.guiframe.colors.toprow.rightspacer -side left -expand 1
    # Row 2: Radio buttons
    frame $prefs.guiframe.colors.radio
    pack $prefs.guiframe.colors.radio -side top -fill x -pady 2
    radiobutton $prefs.guiframe.colors.radio.bg -value 0 \
        -variable ::dialog_preferences::gui_color_radio \
        -text [_ "Background"]
    radiobutton $prefs.guiframe.colors.radio.fg -value 1 \
        -variable ::dialog_preferences::gui_color_radio \
        -text [_ "Foreground"]
    radiobutton $prefs.guiframe.colors.radio.sel -value 2 \
        -variable ::dialog_preferences::gui_color_radio \
        -text [_ "Select"]
    radiobutton $prefs.guiframe.colors.radio.gop -value 3 \
        -variable ::dialog_preferences::gui_color_radio \
        -text [_ "GOP"]
    pack $prefs.guiframe.colors.radio.bg \
         $prefs.guiframe.colors.radio.fg \
         $prefs.guiframe.colors.radio.sel \
         $prefs.guiframe.colors.radio.gop \
         -side left
    # Row 3: Preset colors
    frame $prefs.guiframe.colors.swatches
    pack $prefs.guiframe.colors.swatches -fill x
    foreach r {r1 r2 r3} hexcols {
        { "#FFFFFF" "#DFDFDF" "#BBBBBB" "#FFC7C6" "#FFE3C6" "#FEFFC6" "#C6FFC7" "#C6FEFF" "#C7C6FF" "#E3C6FF" }
        { "#9F9F9F" "#7C7C7C" "#606060" "#FF0400" "#FF8300" "#FAFF00" "#00FF04" "#00FAFF" "#0400FF" "#9C00FF" }
        { "#404040" "#202020" "#000000" "#551312" "#553512" "#535512" "#0F4710" "#0E4345" "#131255" "#2F004D" }
    } {
        frame $prefs.guiframe.colors.swatches.$r
        pack $prefs.guiframe.colors.swatches.$r -side top
        foreach i {0 1 2 3 4 5 6 7 8 9} hexcol $hexcols {
            label $prefs.guiframe.colors.swatches.$r.c$i \
                -background $hexcol \
                -activebackground $hexcol \
                -relief ridge -padx 7 -pady 0 -width 1
            bind $prefs.guiframe.colors.swatches.$r.c$i <Button> \
                "::dialog_preferences::gui_set_color $prefs $hexcol"
        }
        pack $prefs.guiframe.colors.swatches.$r.c0 \
             $prefs.guiframe.colors.swatches.$r.c1 \
             $prefs.guiframe.colors.swatches.$r.c2 \
             $prefs.guiframe.colors.swatches.$r.c3 \
             $prefs.guiframe.colors.swatches.$r.c4 \
             $prefs.guiframe.colors.swatches.$r.c5 \
             $prefs.guiframe.colors.swatches.$r.c6 \
             $prefs.guiframe.colors.swatches.$r.c7 \
             $prefs.guiframe.colors.swatches.$r.c8 \
             $prefs.guiframe.colors.swatches.$r.c9 \
             -side left
    }
}

proc ::dialog_preferences::apply_color_preset {prefs presetname} {
    switch -- $presetname {
        "Default" {
            set ::dialog_preferences::gui_color_bg "#FFFFFF"
            set ::dialog_preferences::gui_color_fg "#000000"
            set ::dialog_preferences::gui_color_sel "#0000FF"
            set ::dialog_preferences::gui_color_gop "#FF0000"
        }
        "Dark Mode" {
            set ::dialog_preferences::gui_color_bg "#333333"
            set ::dialog_preferences::gui_color_fg "#DDDDDD"
            set ::dialog_preferences::gui_color_sel "#AA88AA"
            set ::dialog_preferences::gui_color_gop "#cc5555"
        }
        "Light Mode" {
            set ::dialog_preferences::gui_color_bg "#EEEEEE"
            set ::dialog_preferences::gui_color_fg "#202020"
            set ::dialog_preferences::gui_color_sel "#880088"
            set ::dialog_preferences::gui_color_gop "#DD0000"
        }
        "Pastel" {
            set ::dialog_preferences::gui_color_bg "#FFF0F0"
            set ::dialog_preferences::gui_color_fg "#505050"
            set ::dialog_preferences::gui_color_sel "#4080B0"
            set ::dialog_preferences::gui_color_gop "#C04060"
        }
        "Dark Contrasted" {
            set ::dialog_preferences::gui_color_bg "#000000"
            set ::dialog_preferences::gui_color_fg "#FFFFFF"
            set ::dialog_preferences::gui_color_sel "#4A8A9C"
            set ::dialog_preferences::gui_color_gop "#B04A6B"
        }
        "Purple Haze" {
            set ::dialog_preferences::gui_color_bg "#800080"
            set ::dialog_preferences::gui_color_fg "#cccccc"
            set ::dialog_preferences::gui_color_sel "#ff00ff"
            set ::dialog_preferences::gui_color_gop "#ff0000"
        }
        "Misty Rose" {
            set ::dialog_preferences::gui_color_bg "#FFE4E1"
            set ::dialog_preferences::gui_color_fg "#8B7765"
            set ::dialog_preferences::gui_color_sel "#D2691E"
            set ::dialog_preferences::gui_color_gop "#B22222"
        }
        "C64" {
            set ::dialog_preferences::gui_color_bg "#3e32a2"
            set ::dialog_preferences::gui_color_fg "#a49aea"
            set ::dialog_preferences::gui_color_sel "#cc9933"
            set ::dialog_preferences::gui_color_gop "#ff9933"
        }
        "Strongbad" {
            set ::dialog_preferences::gui_color_bg "#000000"
            set ::dialog_preferences::gui_color_fg "#4bd046"
            set ::dialog_preferences::gui_color_sel "#00a0b0"
            set ::dialog_preferences::gui_color_gop "#cc0000"
        }
        "Solarized" {
            set ::dialog_preferences::gui_color_bg "#fdf6e3"
            set ::dialog_preferences::gui_color_fg "#657b83"
            set ::dialog_preferences::gui_color_sel "#268bd2"
            set ::dialog_preferences::gui_color_gop "#dc322f"
        }
        "Solarized Inverted" {
            set ::dialog_preferences::gui_color_bg "#002b36"
            set ::dialog_preferences::gui_color_fg "#839496"
            set ::dialog_preferences::gui_color_sel "#b58900"
            set ::dialog_preferences::gui_color_gop "#dc322f"
        }
    }
    $prefs.guiframe.colors.toprow.mb configure -text $presetname
    ::dialog_preferences::apply_colors_if_mac
}

proc ::dialog_preferences::gui_set_color {prefs presetcol} {
    switch -- $::dialog_preferences::gui_color_radio {
        0 {
            set ::dialog_preferences::gui_color_bg $presetcol
        }
        1 {
            set ::dialog_preferences::gui_color_fg $presetcol
        }
        2 {
            set ::dialog_preferences::gui_color_sel $presetcol
        }
        3 {
            set ::dialog_preferences::gui_color_gop $presetcol
        }
    }
    ::dialog_preferences::apply_colors_if_mac
}

proc ::dialog_preferences::gui_compose_color {prefs} {
    # Check which color field is currently active
    switch -- $::dialog_preferences::gui_color_radio {
        0 {
            set title [_ "Background color"]
            set color $::dialog_preferences::gui_color_bg
        }
        1 {
            set title [_ "Foreground color"]
            set color $::dialog_preferences::gui_color_fg
        }
        2 {
            set title [_ "Select color"]
            set color $::dialog_preferences::gui_color_sel
        }
        3 {
            set title [_ "GOP color"]
            set color $::dialog_preferences::gui_color_gop
        }
    }
    set color [tk_chooseColor -title $title -initialcolor $color]
    if { $color ne "" } {
        ::dialog_preferences::gui_set_color $prefs $color
        ::dialog_preferences::apply_colors_if_mac
    }
}

proc ::dialog_preferences::create_dialog {{mytoplevel .gui_preferences}} {
    destroy $mytoplevel
    destroy .audio_preferences
    destroy .midi_preferences

    ::preferencewindow::create $mytoplevel [_ "Preferences" ]

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
    set ::dialog_preferences::audio_frame $prefs
    ::preferencewindow::add_apply ${mytoplevel} "::dialog_audio::apply ${prefs}"


    # midi options
    set prefs [::preferencewindow::add_frame $mytoplevel [_ "MIDI preferences"]]
    ::dialog_midi::fill_frame $prefs
    set ::dialog_preferences::midi_frame $prefs
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
    ::preferencewindow::select $mytoplevel [::pd_guiprefs::read preferences_tab]
    catch { bind ${mytoplevel}.content.frames <<NotebookTabChanged>> "::dialog_preferences::tab_changed $mytoplevel" }

    # re-adjust height based on optional sections
    update
    wm minsize $mytoplevel [winfo width $mytoplevel] [winfo reqheight $mytoplevel]

    raise $mytoplevel
}

proc ::dialog_preferences::create {} {
    ::dialog_preferences::create_dialog
}
