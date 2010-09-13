
package provide apple_events 0.1

package require pdwindow
package require wheredoesthisgo

# from http://wiki.tcl.tk/12987

set ::tk::mac::CGAntialiasLimit 0 ;# min line thickness to anti-alias (default: 3)
set ::tk::mac::antialiasedtext  1 ;# enable anti-aliased text

# kAEOpenDocuments
proc ::tk::mac::OpenDocument {args} {
    foreach filename $args {
        if {$::done_init} {
            open_file $filename
        } else {
            lappend ::filestoopen_list $filename
        }
    }
    set ::pd_menucommands::menu_open_dir [file dirname $filename]
}

# kEventAppHidden
proc ::tk::mac::OnHide {args} {
    ::pdwindow::verbose 1 "::tk::mac::OnHide $args +++++++++++++++++++++"
}

# kEventAppShown
proc ::tk::mac::OnShow {args} {
    ::pdwindow::verbose 1 "::tk::mac::OnShow $args +++++++++++++++++++++"
}

# open About Pd... in Tk/Cocoa
proc tkAboutDialog {} {
    menu_aboutpd
}

# kAEShowPreferences
proc ::tk::mac::ShowPreferences {args} {
    ::pdwindow::verbose 1 "::tk::mac::ShowPreferences $args ++++++++++++"
    pdsend "pd start-path-dialog"
}

# kAEQuitApplication
proc ::tk::mac::Quit {args} {
    pdsend "pd verifyquit"
}

# on Tk/Cocoa, override the Apple Help menu
#proc tk::mac::ShowHelp {args} {
#}

# these I gleaned by reading the source (tkMacOSXHLEvents.c)
proc ::tk::mac::PrintDocument {args} {
    menu_print $::focused_window
}

proc ::tk::mac::OpenApplication {args} {
    ::pdwindow::verbose 1 "::tk::mac::OpenApplication $args ++++++++++++"
}

proc ::tk::mac::ReopenApplication {args} {
    ::pdwindow::verbose 1 "::tk::mac::ReopenApplication $args ++++++++++"
}
