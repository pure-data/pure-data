
package provide pd_menucommands 0.1

namespace eval ::pd_menucommands:: {
    variable untitled_number "1"
    variable menu_new_dir [pwd]
    variable menu_open_dir [pwd]

    namespace export menu_*
}

# ------------------------------------------------------------------------------
# functions called from File menu

proc ::pd_menucommands::menu_new {} {
    variable untitled_number
    variable menu_new_dir
    if { ! [file isdirectory $menu_new_dir]} {set menu_new_dir $::env(HOME)}
    set untitled_name [_ "Untitled"]
    pdsend "pd filename $untitled_name-$untitled_number [enquote_path $menu_new_dir]"
    pdsend "#N canvas"
    pdsend "#X pop 1"
    incr untitled_number
}

proc ::pd_menucommands::menu_open {} {
    variable menu_open_dir
    if { ! [file isdirectory $menu_open_dir]} {set menu_open_dir $::env(HOME)}
    set files [tk_getOpenFile -defaultextension .pd \
                       -multiple true \
                       -filetypes $::filetypes \
                       -initialdir $menu_open_dir]
    if {$files ne ""} {
        foreach filename $files { 
            open_file $filename
        }
        set menu_open_dir [file dirname $filename]
    }
}

proc ::pd_menucommands::menu_print {mytoplevel} {
    set filename [tk_getSaveFile -initialfile pd.ps \
                      -defaultextension .ps \
                      -filetypes { {{postscript} {.ps}} }]
    if {$filename ne ""} {
        $mytoplevel.c postscript -file $filename 
    }
}

# dialog types:
#   global (only one):   find, sendmessage, prefs, helpbrowser
#   per-canvas:          font, canvas properties (created with a message from pd)
#   per object:          gatom, iemgui, array, data structures (created with a message from pd)


# ------------------------------------------------------------------------------
# functions called from Edit menu

proc menu_undo {mytoplevel} {
    # puts stderr "menu_undo $mytoplevel not implemented yet"
}

proc menu_redo {mytoplevel} {
    # puts stderr "menu_redo $mytoplevel not implemented yet"
}

# ------------------------------------------------------------------------------
# open the dialog panels

proc ::pd_menucommands::menu_message_dialog {} {
    if {[winfo exists .send_message]} {
        wm deiconify .send_message
        raise .message
    } else {
        # TODO insert real message panel here
        toplevel .send_message
        wm group .send_message .
        wm title .send_message [_ "Send Message..."]
        wm resizable .send_message 0 0
        ::pd_bindings::dialog_bindings .send_message "send_message"
        frame .send_message.frame
        label .send_message.label -text [_ "Message"] -width 30 -height 15
        pack .send_message.label .send_message.frame -side top -expand yes -fill both
    }
}

proc ::pd_menucommands::menu_font_dialog {mytoplevel} {
    if {[winfo exists .font]} {
        raise .font
    } elseif {$mytoplevel eq ".pdwindow"} {
        pdtk_canvas_dofont .pdwindow [lindex [.pdwindow.text cget -font] 1]
    } else {
        pdsend "$mytoplevel menufont"
    }
}

proc ::pd_menucommands::menu_path_dialog {} {
    if {[winfo exists .path]} {
        raise .path
    } else {
        pdsend "pd start-path-dialog"
    }
}

proc ::pd_menucommands::menu_startup_dialog {} {
    if {[winfo exists .startup]} {
        raise .startup
    } else {
        pdsend "pd start-startup-dialog"
    }
}

# ------------------------------------------------------------------------------
# window management functions

proc ::pd_menucommands::menu_minimize {mytoplevel} {
    wm iconify $mytoplevel
}

proc ::pd_menucommands::menu_maximize {mytoplevel} {
    wm state $mytoplevel zoomed
}

proc menu_raise_pdwindow {} {
    set top_window [lindex [wm stackorder .pdwindow] end]
    if {.pdwindow eq $top_window} {
        lower .pdwindow
    } else {
        wm deiconify .pdwindow
        raise .pdwindow
    }
}

# ------------------------------------------------------------------------------
# manage the saving of the directories for the new commands

# this gets the dir from the path of a window's title
proc ::pd_menucommands::set_menu_new_dir {mytoplevel} {
    variable menu_new_dir
    variable menu_open_dir
    # TODO add Aqua specifics once g_canvas.c has [wm attributes -titlepath]
    if {$mytoplevel eq ".pdwindow"} {
        # puts "set_menu_new_dir $mytoplevel"
        set menu_new_dir $menu_open_dir
    } else {
        regexp -- ".+ - (.+)" [wm title $mytoplevel] ignored menu_new_dir
    }
}

# ------------------------------------------------------------------------------
# opening docs as menu items (like the Test Audio and MIDI patch and the manual)
proc ::pd_menucommands::menu_doc_open {subdir basename} {
    set dirname "$::sys_libdir/$subdir"
    
    switch -- [string tolower [file extension $basename]] {
        ".txt"    {::pd_menucommands::menu_opentext "$dirname/$basename"
        } ".c"    {::pd_menucommands::menu_opentext "$dirname/$basename"
        } ".htm"  {::pd_menucommands::menu_openhtml "$dirname/$basename"
        } ".html" {::pd_menucommands::menu_openhtml "$dirname/$basename"
        } default {
            pdsend "pd open [enquote_path $basename] [enquote_path $dirname]"
        }
    }
}

# open text docs in a Pd window
proc ::pd_menucommands::menu_opentext {filename} {
    global pd_myversion
    set mytoplevel [format ".help%d" [clock seconds]]
    toplevel $mytoplevel -class TextWindow
    text $mytoplevel.text -relief flat -borderwidth 0 \
        -yscrollcommand "$mytoplevel.scroll set" -background white
    scrollbar $mytoplevel.scroll -command "$mytoplevel.text yview"
    pack $mytoplevel.scroll -side right -fill y
    pack $mytoplevel.text -side left -fill both -expand 1
    ::pd_bindings::window_bindings $mytoplevel
    
    set textfile [open $filename]
    while {![eof $textfile]} {
        set bigstring [read $textfile 1000]
        regsub -all PD_BASEDIR $bigstring $::sys_guidir bigstring2
        regsub -all PD_VERSION $bigstring2 $pd_myversion bigstring3
        $mytoplevel.text insert end $bigstring3
    }
    close $textfile
}

# open HTML docs from the menu using the OS-default HTML viewer
proc ::pd_menucommands::menu_openhtml {filename} {
    if {$::tcl_platform(os) eq "Darwin"} {
        exec sh -c [format "open '%s'" $filename]
    } elseif {$::tcl_platform(platform) eq "windows"} {
        exec rundll32 url.dll,FileProtocolHandler [format "%s" $filename] &
    } else {
        foreach candidate { gnome-open xdg-open sensible-browser iceweasel firefox \
                                mozilla galeon konqueror netscape lynx } {
            set browser [lindex [auto_execok $candidate] 0]
            if {[string length $browser] != 0} {
                exec -- sh -c [format "%s '%s'" $browser $filename] &
                break
            }
        }
    }
}

# ------------------------------------------------------------------------------
# Mac OS X specific functions

proc ::pd_menucommands::menu_bringalltofront {} {
    # use [winfo children .] here to include windows that are minimized
    foreach item [winfo children .] {
        # get all toplevel windows, exclude menubar windows
        if { [string equal [winfo toplevel $item] $item] && \
                 [catch {$item cget -tearoff}]} {
            wm deiconify $item
        }
    }
    wm deiconify .
}
