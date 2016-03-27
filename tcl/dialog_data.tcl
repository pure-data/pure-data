
package provide dialog_data 0.1

namespace eval ::dialog_data:: {
    namespace export pdtk_data_dialog
}

############ pdtk_data_dialog -- run a data dialog #########

proc ::dialog_data::send {mytoplevel} {
    for {set i 1} {[$mytoplevel.text compare [concat $i.0 + 3 chars] < end]} \
        {incr i 1} {
            pdsend "$mytoplevel data [$mytoplevel.text get $i.0 [expr $i + 1].0]"
        }
    pdsend "$mytoplevel end"
}

proc ::dialog_data::cancel {mytoplevel} {
    pdsend "$mytoplevel cancel"
}

proc ::dialog_data::ok {mytoplevel} {
    ::dialog_data::send $mytoplevel
    ::dialog_data::cancel $mytoplevel
}

proc ::dialog_data::pdtk_data_dialog {mytoplevel stuff} {
    variable modifier
    set modkeyname "Ctrl"
    if {$::windowingsystem eq "aqua"} {
        set modkeyname "Cmd"
    }

    toplevel $mytoplevel -class DialogWindow
    wm title $mytoplevel [_ "Data Properties"]
    wm group $mytoplevel $::focused_window
    wm transient $mytoplevel $::focused_window
    $mytoplevel configure -menu $::dialog_menubar
    $mytoplevel configure -padx 0 -pady 0

    frame $mytoplevel.buttonframe
    pack $mytoplevel.buttonframe -side bottom -fill x -pady 2m
    button $mytoplevel.buttonframe.send -text [_ "Send ($modkeyname-S)"] \
        -command "::dialog_data::send $mytoplevel"
    button $mytoplevel.buttonframe.ok -text [_ "Done ($modkeyname-D)"] \
        -command "::dialog_data::ok $mytoplevel"
    pack $mytoplevel.buttonframe.send -side left -expand 1
    pack $mytoplevel.buttonframe.ok -side left -expand 1

    text $mytoplevel.text -relief raised -highlightthickness 0 -bd 2 -height 40 -width 60 \
        -yscrollcommand "$mytoplevel.scroll set" -background white
    scrollbar $mytoplevel.scroll -command "$mytoplevel.text yview"
    pack $mytoplevel.scroll -side right -fill y
    pack $mytoplevel.text -side left -fill both -expand 1
    $mytoplevel.text insert end $stuff
    bind $mytoplevel.text <$::modifier-Key-s> "::dialog_data::send $mytoplevel"
    bind $mytoplevel.text <$::modifier-Key-d> "::dialog_data::ok $mytoplevel"
    bind $mytoplevel.text <$::modifier-Key-w> "::dialog_data::cancel $mytoplevel"
    focus $mytoplevel.text
}
