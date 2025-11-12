package provide dialog_theme 0.1
package require pd_guiprefs

namespace eval ::dialog_theme:: {
}

package provide dialog_theme 0.1
package require pd_guiprefs
package require pd_i18n

namespace eval ::dialog_theme:: {
}

proc ::dialog_theme::set_colors {fg bg sel gop} {
    # called from C to report current colors as hex strings
    if {[string length $fg]} { set ::dialog_theme::foreground $fg }
    if {[string length $bg]} { set ::dialog_theme::background $bg }
    if {[string length $sel]} { set ::dialog_theme::selection $sel }
    if {[string length $gop]} { set ::dialog_theme::gop $gop }
}

proc ::dialog_theme::color_to_hex {color} {
    if {[string match "#*" $color]} {
        return $color
    } else {
        return [format "#%06x" [winfo rgb . $color]]
    }
}

proc ::dialog_theme::choose_color {var} {
    set color [tk_chooseColor -initialcolor [set $var] -title "Choose color"]
    if {$color ne ""} {
        set $var $color
    }
}

proc ::dialog_theme::apply {mytoplevel} {
    set fg [::dialog_theme::color_to_hex $::dialog_theme::foreground]
    set bg [::dialog_theme::color_to_hex $::dialog_theme::background]
    set sel [::dialog_theme::color_to_hex $::dialog_theme::selection]
    set gop [::dialog_theme::color_to_hex $::dialog_theme::gop]
    pdsend "pd colors $fg $bg $sel $gop"
    # persist gui preferences so colors survive restart
    ::pd_guiprefs::write "foregroundcolor" $fg
    ::pd_guiprefs::write "backgroundcolor" $bg
    ::pd_guiprefs::write "selectioncolor" $sel
    ::pd_guiprefs::write "gopcolor" $gop
}

proc ::dialog_theme::ok {mytoplevel} {
    ::dialog_theme::apply $mytoplevel
    pdsend "pd save-preferences"
    destroy $mytoplevel
}

proc ::dialog_theme::cancel {mytoplevel} {
    destroy $mytoplevel
}

proc ::dialog_theme::create_dialog {{mytoplevel .theme}} {
    destroy $mytoplevel

    toplevel $mytoplevel -class DialogWindow
    ::pd_menus::menubar_for_dialog $mytoplevel
    wm title $mytoplevel [_ "Theme Editor"]
    wm group $mytoplevel .
    wm transient $mytoplevel $::focused_window
    ::pd_bindings::dialog_bindings $mytoplevel "theme"

    # try to read persisted colors from preferences, fall back to core defaults
    set fg [::pd_guiprefs::read foregroundcolor]
    set bg [::pd_guiprefs::read backgroundcolor]
    set sel [::pd_guiprefs::read selectioncolor]
    set gop [::pd_guiprefs::read gopcolor]

    if {$fg eq ""} { set fg "#000000" }
    if {$bg eq ""} { set bg "#ffffff" }
    if {$sel eq ""} { set sel "#0000ff" }
    if {$gop eq ""} { set gop "#ff0000" }

    set ::dialog_theme::foreground $fg
    set ::dialog_theme::background $bg
    set ::dialog_theme::selection $sel
    set ::dialog_theme::gop $gop

    # request actual current values from Pd core (C) -- may override above
    pdsend "pd request-theme-colors"

    frame $mytoplevel.colors
    pack $mytoplevel.colors -side top -fill both -expand 1 -padx 10 -pady 10

    labelframe $mytoplevel.colors.fg -text [_ "Foreground"] -padx 5 -pady 5
    pack $mytoplevel.colors.fg -side top -fill x
    entry $mytoplevel.colors.fg.entry -textvariable ::dialog_theme::foreground -width 10
    button $mytoplevel.colors.fg.button -text [_ "Choose..."] -command "::dialog_theme::choose_color ::dialog_theme::foreground"
    pack $mytoplevel.colors.fg.entry $mytoplevel.colors.fg.button -side left

    labelframe $mytoplevel.colors.bg -text [_ "Background"] -padx 5 -pady 5
    pack $mytoplevel.colors.bg -side top -fill x
    entry $mytoplevel.colors.bg.entry -textvariable ::dialog_theme::background -width 10
    button $mytoplevel.colors.bg.button -text [_ "Choose..."] -command "::dialog_theme::choose_color ::dialog_theme::background"
    pack $mytoplevel.colors.bg.entry $mytoplevel.colors.bg.button -side left

    labelframe $mytoplevel.colors.sel -text [_ "Selection"] -padx 5 -pady 5
    pack $mytoplevel.colors.sel -side top -fill x
    entry $mytoplevel.colors.sel.entry -textvariable ::dialog_theme::selection -width 10
    button $mytoplevel.colors.sel.button -text [_ "Choose..."] -command "::dialog_theme::choose_color ::dialog_theme::selection"
    pack $mytoplevel.colors.sel.entry $mytoplevel.colors.sel.button -side left

    labelframe $mytoplevel.colors.gop -text [_ "Graph on Parent"] -padx 5 -pady 5
    pack $mytoplevel.colors.gop -side top -fill x
    entry $mytoplevel.colors.gop.entry -textvariable ::dialog_theme::gop -width 10
    button $mytoplevel.colors.gop.button -text [_ "Choose..."] -command "::dialog_theme::choose_color ::dialog_theme::gop"
    pack $mytoplevel.colors.gop.entry $mytoplevel.colors.gop.button -side left

    frame $mytoplevel.buttons
    pack $mytoplevel.buttons -side bottom -fill x -padx 10 -pady 10
    button $mytoplevel.buttons.apply -text [_ "Apply"] -command "::dialog_theme::apply $mytoplevel"
    button $mytoplevel.buttons.ok -text [_ "OK"] -command "::dialog_theme::ok $mytoplevel"
    button $mytoplevel.buttons.cancel -text [_ "Cancel"] -command "::dialog_theme::cancel $mytoplevel"
    pack $mytoplevel.buttons.apply $mytoplevel.buttons.ok $mytoplevel.buttons.cancel -side left -expand 1

    position_over_window $mytoplevel $::focused_window
}