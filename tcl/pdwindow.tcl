
package provide pdwindow 0.1

namespace eval ::pdwindow:: {
    variable consolefont
    variable printout_buffer ""
    variable pdwindow_search_index

    namespace export pdtk_post
}



proc ::pdwindow::pdtk_post {message} {
    variable printout_buffer
    # TODO this should be switchable between Pd window and stderr
    if { ! [winfo exists .pdwindow.text]} {
        set printout_buffer "$printout_buffer\n$message"
    } else {
        if {$printout_buffer ne ""} {
            .pdwindow.text insert end "$printout_buffer\n"
            set printout_buffer ""
        }
        .pdwindow.text insert end "$message\n"
        .pdwindow.text yview end
    }
    puts stderr $message
}

proc ::pdwindow::create_window {} {
    variable consolefont
    toplevel .pdwindow -class PdWindow
    wm title .pdwindow [_ "Pd window"]
    wm geometry .pdwindow =500x450+20+50
    .pdwindow configure -menu .menubar
    ::pd_menus::configure_for_pdwindow
    ::pd_bindings::pdwindow_bindings .pdwindow

    frame .pdwindow.header
    pack .pdwindow.header -side top -fill x -padx 30 -ipady 10
    # label .pdwindow.header.label -text "The Pd window wants you to make it look nice!"
    # pack .pdwindow.header.label -side left -fill y -anchor w
    checkbutton .pdwindow.header.dsp -text [_ "DSP"] -variable ::dsp \
        -command "pdsend \"pd dsp 0\""
    pack .pdwindow.header.dsp -side right -fill y -anchor e
# TODO this should use the pd_font_$size created in pd-gui.tcl    
    text .pdwindow.text -relief raised -bd 2 -font {-size 10} \
        -yscrollcommand ".pdwindow.scroll set" -width 60
    scrollbar .pdwindow.scroll -command ".pdwindow.text yview"
    pack .pdwindow.scroll -side right -fill y
    pack .pdwindow.text -side bottom -fill both -expand 1
    raise .pdwindow
}
