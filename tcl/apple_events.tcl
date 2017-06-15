
package provide apple_events 0.1

package require pdwindow
package require wheredoesthisgo

# references:
# https://www.tcl.tk/man/tcl/TkCmd/tk_mac.htm
# http://www.tkdocs.com/tutorial/menus.html
# http://wiki.tcl.tk/12987

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
    ::pdwindow::verbose 1 "::tk::mac::OnHide $args +++++++++++++++++++++\n"
}

# kEventAppShown
proc ::tk::mac::OnShow {args} {
    ::pdwindow::verbose 1 "::tk::mac::OnShow $args +++++++++++++++++++++\n"
}

# open About Pd... in Tk/Cocoa
proc tkAboutDialog {} {
    menu_aboutpd
}

# kAEShowPreferences
proc ::tk::mac::ShowPreferences {args} {
    ::pdwindow::verbose 1 "::tk::mac::ShowPreferences $args ++++++++++++\n"
    pdsend "pd start-path-dialog"
}

# kAEQuitApplication
proc ::tk::mac::Quit {args} {
    pdsend "pd verifyquit"
}

# on Tk/Cocoa, respond to the "Pd Help" option in the Help menu which
# is provided by default and cannot disabled or removed 
proc ::tk::mac::ShowHelp {args} {
    ::pdwindow::verbose 1 "::tk::mac::ShowHelp $args ++++++++++++\n"
    ::pd_menucommands::menu_helpbrowser
}

# these I gleaned by reading the source (tkMacOSXHLEvents.c)
proc ::tk::mac::PrintDocument {args} {
    menu_print $::focused_window
}

proc ::tk::mac::OpenApplication {args} {
    ::pdwindow::verbose 1 "::tk::mac::OpenApplication $args ++++++++++++\n"
}

proc ::tk::mac::ReopenApplication {args} {
    ::pdwindow::verbose 1 "::tk::mac::ReopenApplication $args ++++++++++\n"
}
