
package provide dialog_preferences 0.1
package require pd_guiprefs
package require pd_i18n
package require preferencewindow

namespace eval ::dialog_preferences:: {
}

set ::dialog_preferences::use_ttknotebook {}
after idle ::dialog_preferences::read
# allow updating the audio resp MIDI frame if the backend changes
set ::dialog_preferences::audio_frame {}
set ::dialog_preferences::midi_frame {}
set ::dialog_preferences::gui_color_radio 0


proc ::dialog_preferences::cancel {mytoplevel} {
    set ::dialog_preferences::audio_frame {}
    set ::dialog_preferences::midi_frame {}
    destroy $mytoplevel
}
proc ::dialog_preferences::do_apply {mytoplevel} {
    ::pd_guiprefs::write "gui_language" $::pd_i18n::language
    ::pd_guiprefs::write "use_ttknotebook" $::dialog_preferences::use_ttknotebook
    ::pd_guiprefs::write "cords_to_foreground" $::pdtk_canvas::enable_cords_to_foreground
    ::pd_guiprefs::write "color_bg" $::dialog_preferences::gui_color_bg
    ::pd_guiprefs::write "color_fg" $::dialog_preferences::gui_color_fg
    ::pd_guiprefs::write "color_sel" $::dialog_preferences::gui_color_sel
    ::pd_guiprefs::write "color_gop" $::dialog_preferences::gui_color_gop

    pdsend "pd zoom-open $::sys_zoom_open"
    pdsend "pd colors $::dialog_preferences::gui_color_fg $::dialog_preferences::gui_color_bg $::dialog_preferences::gui_color_sel $::dialog_preferences::gui_color_gop"
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
        set ::dialog_preferences::gui_color_sek  "#0000FF"
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

    # Radio buttons and Compose button
    frame $prefs.guiframe.colors.radio
    pack $prefs.guiframe.colors.radio -side top

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

    label $prefs.guiframe.colors.radio.dummy -text "" -width 1
    button $prefs.guiframe.colors.radio.but -text [_ "Compose"] \
        -command "::dialog_preferences::gui_compose_color $prefs"

    pack $prefs.guiframe.colors.radio.bg \
         $prefs.guiframe.colors.radio.fg \
         $prefs.guiframe.colors.radio.sel \
         $prefs.guiframe.colors.radio.gop \
         $prefs.guiframe.colors.radio.dummy \
         $prefs.guiframe.colors.radio.but \
         -side left

    # Container frame for preset-colors-rows + dropdown
    frame $prefs.guiframe.colors.container -pady 8
    pack $prefs.guiframe.colors.container -fill x

    # Left side: Preset colors (3 rows)
    frame $prefs.guiframe.colors.container.left
    pack $prefs.guiframe.colors.container.left -side left -fill x -expand 1

    foreach r {r1 r2 r3} hexcols {
        { "#FFFFFF" "#DFDFDF" "#BBBBBB" "#FFC7C6" "#FFE3C6" "#FEFFC6" "#C6FFC7" "#C6FEFF" "#C7C6FF" "#E3C6FF" }
        { "#9F9F9F" "#7C7C7C" "#606060" "#FF0400" "#FF8300" "#FAFF00" "#00FF04" "#00FAFF" "#0400FF" "#9C00FF" }
        { "#404040" "#202020" "#000000" "#551312" "#553512" "#535512" "#0F4710" "#0E4345" "#131255" "#2F004D" }
    } {
        frame $prefs.guiframe.colors.container.left.$r
        pack $prefs.guiframe.colors.container.left.$r -side top

        foreach i {0 1 2 3 4 5 6 7 8 9} hexcol $hexcols {
            label $prefs.guiframe.colors.container.left.$r.c$i \
                -background $hexcol \
                -activebackground $hexcol \
                -relief ridge -padx 7 -pady 0 -width 1
            bind $prefs.guiframe.colors.container.left.$r.c$i <Button> \
                "::dialog_preferences::gui_set_color $prefs $hexcol"
        }

        pack $prefs.guiframe.colors.container.left.$r.c0 \
             $prefs.guiframe.colors.container.left.$r.c1 \
             $prefs.guiframe.colors.container.left.$r.c2 \
             $prefs.guiframe.colors.container.left.$r.c3 \
             $prefs.guiframe.colors.container.left.$r.c4 \
             $prefs.guiframe.colors.container.left.$r.c5 \
             $prefs.guiframe.colors.container.left.$r.c6 \
             $prefs.guiframe.colors.container.left.$r.c7 \
             $prefs.guiframe.colors.container.left.$r.c8 \
             $prefs.guiframe.colors.container.left.$r.c9 \
             -side left
    }

    # Right side: Dropdown menu
    frame $prefs.guiframe.colors.container.right
    pack $prefs.guiframe.colors.container.right -side left -padx 5

    label $prefs.guiframe.colors.container.right.lab -text [_ "Presets:"]
    menubutton $prefs.guiframe.colors.container.right.mb \
        -text "Default" \
        -menu $prefs.guiframe.colors.container.right.mb.menu \
        -width 12 -relief raised
    menu $prefs.guiframe.colors.container.right.mb.menu -tearoff 0
    $prefs.guiframe.colors.container.right.mb configure \
        -menu $prefs.guiframe.colors.container.right.mb.menu

    # Add preset options
    set presets { "Default" "High Contrast" "Pastel" "Dark Mode" "C64" "Strongbad" "Solarized" "Solarized Inverted"}
    foreach preset $presets {
        $prefs.guiframe.colors.container.right.mb.menu add command \
            -label $preset \
            -command "::dialog_preferences::apply_color_preset $prefs \"$preset\""
    }

    pack $prefs.guiframe.colors.container.right.lab \
         $prefs.guiframe.colors.container.right.mb \
         -side top -anchor w
}

proc ::dialog_preferences::apply_color_preset {prefs presetname} {
    switch -- $presetname {
        "Default" {
            set ::dialog_preferences::gui_color_bg "#FFFFFF"
            set ::dialog_preferences::gui_color_fg "#000000"
            set ::dialog_preferences::gui_color_sel "#0000FF"
            set ::dialog_preferences::gui_color_gop "#FF0000"
        }
        "High Contrast" {
            set ::dialog_preferences::gui_color_bg "#000000"
            set ::dialog_preferences::gui_color_fg "#FFFFFF"
            set ::dialog_preferences::gui_color_sel "#FFFF00"
            set ::dialog_preferences::gui_color_gop "#FF00FF"
        }
        "Pastel" {
            set ::dialog_preferences::gui_color_bg "#FFF0F0"
            set ::dialog_preferences::gui_color_fg "#808080"
            set ::dialog_preferences::gui_color_sel "#A0D0A0"
            set ::dialog_preferences::gui_color_gop "#D0A0D0"
        }
        "Dark Mode" {
            set ::dialog_preferences::gui_color_bg "#333333"
            set ::dialog_preferences::gui_color_fg "#DDDDDD"
            set ::dialog_preferences::gui_color_sel "#88AA88"
            set ::dialog_preferences::gui_color_gop "#AA88AA"
        }
        "C64" {
            set ::dialog_preferences::gui_color_bg "#3e32a2"
            set ::dialog_preferences::gui_color_fg "#a49aea"
            set ::dialog_preferences::gui_color_sel "#7c71da"
            set ::dialog_preferences::gui_color_gop "#ff9933"
        }
        "Strongbad" {
            set ::dialog_preferences::gui_color_bg "#000000"
            set ::dialog_preferences::gui_color_fg "#4bd046"
            set ::dialog_preferences::gui_color_sel "#53b83b"
            set ::dialog_preferences::gui_color_gop "#cc0000"
        }
        "Solarized" {
            set ::dialog_preferences::gui_color_bg "#fdf6e3"
            set ::dialog_preferences::gui_color_fg "#657b83"
            set ::dialog_preferences::gui_color_sel "#073642"
            set ::dialog_preferences::gui_color_gop "#dc322f"
        }
        "Solarized Inverted" {
            set ::dialog_preferences::gui_color_bg "#002b36"
            set ::dialog_preferences::gui_color_fg "#839496"
            set ::dialog_preferences::gui_color_sel "#b58900"
            set ::dialog_preferences::gui_color_gop "#dc322f"
        }
    }
    # Update menubutton text
    $prefs.guiframe.colors.container.right.mb configure -text $presetname
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
            set ::dialog_preferences::gui_color_select $presetcol
        }
        3 {
            set ::dialog_preferences::gui_color_gop $presetcol
        }
    }
}

proc ::dialog_preferences::gui_compose_color {prefs} {
    switch -- $::dialog_preferences::gui_color_radio {
        0 {
            set title [_ "Background color"]
            # We'll need variables for each color - assuming these exist
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
