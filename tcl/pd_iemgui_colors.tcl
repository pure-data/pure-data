# IEMGUI Colors Management Plugin for Pure Data
# Adds a submenu to the patch window's "Edit" menu for bulk setting IEMGUI object colors

package provide pd_iemgui_colors 0.1

namespace eval ::iemgui_colors:: {}

proc ::iemgui_colors::initialize {} {
    set editmenu $::patch_menubar.edit
    if {![winfo exists $editmenu]} {
        return
    }
    $editmenu add separator
    set colorsmenu [menu $editmenu.colors -tearoff 0]
    $editmenu add cascade -label [_ "Set GUI Object Colors"] -menu $colorsmenu
    $colorsmenu add command -label [_ "Adaptive Colors"] \
        -command {::iemgui_colors::set_colors adaptive}
    $colorsmenu add command -label [_ "Standard Colors"] \
        -command {::iemgui_colors::set_colors standard}
}

proc ::iemgui_colors::set_colors {color_type} {
    raise $::focused_window
    set message [expr {$color_type eq "adaptive" ? \
        [_ "Set adaptive colors for all GUI objects in this patch?"] : \
        [_ "Set standard colors for all GUI objects in this patch?"]}]
    set result [tk_messageBox -type okcancel -icon question -default "ok" \
        -message $message \
        -parent $::focused_window]
    if {$result eq "ok"} {
        ::pdsend "$::focused_window iemgui_set_colors $color_type"
    }
}

::iemgui_colors::initialize
