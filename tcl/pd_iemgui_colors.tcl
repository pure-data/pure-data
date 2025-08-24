# IEMGUI Colors Management Plugin for Pure Data
# Adds a submenu to the "Tools" menu for bulk setting IEMGUI object colors of a patch window

namespace eval ::iemgui_colors:: {}

proc ::iemgui_colors::add_colors_menu {toolsmenu} {
    if {![winfo exists $toolsmenu]} {
        return
    }
    $toolsmenu add separator
    set colorsmenu [menu $toolsmenu.colors -tearoff 0]
    $toolsmenu add cascade -label [_ "Set GUI object colors"] -menu $colorsmenu
    $colorsmenu add command -label [_ "adaptive"] \
        -command {::iemgui_colors::set_colors adaptive}
    $colorsmenu add command -label [_ "standard"] \
        -command {::iemgui_colors::set_colors standard}
}

proc ::iemgui_colors::set_colors {color_type} {
    if {[info exists ::focused_window] && [winfo class $::focused_window] eq "PatchWindow"} {
        raise $::focused_window
        set color_type_label [expr {$color_type eq "adaptive" ? [_ "adaptive"] : [_ "standard"]}]
        set result [tk_messageBox -type okcancel -icon question -default "ok" \
            -message [_ "Set %s colors for all GUI objects in this patch?" $color_type_label] \
            -parent $::focused_window]
        if {$result eq "ok"} {
            ::pdsend "$::focused_window iemgui_set_colors $color_type"
        }
    } else {
        tk_messageBox -type ok -icon info \
            -message [_ "Select a patch window first to change colors."]
    }
}

proc ::iemgui_colors::initialize {} {
    set toolsmenu {}
    set pdmenu [.pdwindow cget -menu]
    for {set m 0} {$m <= [$pdmenu index end]} {incr m} {
        set _m [$pdmenu entrycget $m -menu]
        switch -glob $_m {
            *.tools { set toolsmenu $_m }
        }
    }
    if { [winfo exists ${toolsmenu}] } {
        ::iemgui_colors::add_colors_menu ${toolsmenu}
    }
}

::iemgui_colors::initialize
