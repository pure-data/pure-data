
package provide apple_events 0.1

package require wheredoesthisgo

# from http://wiki.tcl.tk/12987

set ::tk::mac::CGAntialiasLimit 0 ;# min line thickness to anti-alias (default: 3)
set ::tk::mac::antialiasedtext  1 ;# enable/disable anti-aliased text

# kAEOpenDocuments
proc ::tk::mac::OpenDocument {args} {
    foreach filename $args { 
        puts "open_file $filename"
        open_file $filename
    }
    set ::pd_menucommands::menu_open_dir [file dirname $filename]
}

# kEventAppHidden
proc ::tk::mac::OnHide {} {
    # TODO
}

# kEventAppShown
proc ::tk::mac::OnShow {} {
    # TODO
}

# kAEShowPreferences
proc ::tk::mac::ShowPreferences {} {
     menu_preferences_dialog
}

# kAEQuitApplication
#proc ::tk::mac::Quit {} {
#    # TODO sort this out... how to quit pd-gui after sending the message
#    puts stderr "Custom exit proc"
#    pdsend "pd verifyquit"
#}

# these I gleaned by reading the source (tkMacOSXHLEvents.c)
proc ::tk::mac::PrintDocument {args} {
    # TODO what's $mytoplevel here?.  I am guessing args would be the same as
    # ::tk::mac::OpenDocument
    #menu_print $mytoplevel
}

proc ::tk::mac::OpenApplication {} {
}

proc ::tk::mac::ReopenApplication {} {
}
