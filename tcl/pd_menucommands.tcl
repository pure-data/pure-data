
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
    pdsend "pd filename Untitled-$untitled_number [enquote_path $menu_new_dir]"
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
            puts "open_file $filename"
            open_file $filename
        }
        set menu_open_dir [file dirname $filename]
    }
}

proc ::pd_menucommands::menu_print {mytoplevel} {
    set filename [tk_getSaveFile -initialfile pd.ps \
              -defaultextension .ps \
              -filetypes { {{postscript} {.ps}} }]
    if {$filename != ""} {
    $mytoplevel.c postscript -file $filename 
    }
}

# panel types:
#   global (only one):   find, sendmessage, prefs, helpbrowser
#   per-canvas:          font, canvas properties (created with a message from pd)
#   per object:          gatom, iemgui, array, data structures (created with a message from pd)


# ------------------------------------------------------------------------------
# functions called from Edit menu

proc menu_undo {mytoplevel} {
    puts stderr "menu_undo $mytoplevel not implemented yet"
}

proc menu_redo {mytoplevel} {
    puts stderr "menu_redo $mytoplevel not implemented yet"
}

# ------------------------------------------------------------------------------
# open the panels

proc ::pd_menucommands::menu_message_panel {} {
    if {[winfo exists .send_message]} {
        wm deiconify .send_message
        raise .message
    } else {
        # TODO insert real message panel here
        toplevel .send_message
        wm title .send_message [_ "Send Message..."]
        wm resizable .send_message 0 0
        ::pd_bindings::panel_bindings .send_message "send_message"
        frame .send_message.frame
        label .send_message.label -text "message" -width 30 -height 15
        pack .send_message.label .send_message.frame -side top -expand yes -fill both
    }
}


proc ::pd_menucommands::menu_dialog_font {mytoplevel} {
    if {[winfo exists .font]} {
        wm deiconify .font
        raise .font
    } else {
        # TODO insert real preference panel here
        toplevel .font
        wm title .font [_ "Font"]
        ::pd_bindings::panel_bindings .font "font"
        frame .font.frame
        label .font.label -text "font" -width 30 -height 15
        pack .font.label .font.frame -side top -expand yes -fill both
    }
}

proc ::pd_menucommands::menu_path_panel {} {
    if {[winfo exists .path]} {
    raise .path
    } else {
    pdsend "pd start-path-dialog"
    }
}

proc ::pd_menucommands::menu_startup_panel {} {
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
    set pd_window .
    set top_window [lindex [wm stackorder $pd_window] end]
    if {$pd_window eq $top_window} {
        lower $pd_window
    } else {
        wm deiconify $pd_window
        raise $pd_window
    }
}

# ------------------------------------------------------------------------------
# manage the saving of the directories for the new commands

# this gets the dir from the path of a window's title
proc ::pd_menucommands::set_menu_new_dir {mytoplevel} {
    variable menu_new_dir
    # TODO add Aqua specifics once g_canvas.c has [wm attributes -titlepath]
    if {$mytoplevel eq "."} {
        set menu_new_dir [pwd]
    } else {
        regexp -- ".+ - (.+)" [wm title $mytoplevel] ignored menu_new_dir
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
