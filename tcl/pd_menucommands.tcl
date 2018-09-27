
package provide pd_menucommands 0.1

namespace eval ::pd_menucommands:: {
    variable untitled_number "1"

    namespace export menu_*
}

# ------------------------------------------------------------------------------
# functions called from File menu

proc ::pd_menucommands::menu_new {} {
    variable untitled_number
    if { ! [file isdirectory $::filenewdir]} {set ::filenewdir $::env(HOME)}
    # to localize "Untitled" there will need to be changes in g_canvas.c and
    # g_readwrite.c, where it tests for the string "Untitled"
    set untitled_name "Untitled"
    pdsend "pd menunew $untitled_name-$untitled_number [enquote_path $::filenewdir]"
    incr untitled_number
}

proc ::pd_menucommands::menu_open {} {
    if { ! [file isdirectory $::fileopendir]} {set ::fileopendir $::env(HOME)}
    set files [tk_getOpenFile -defaultextension .pd \
                       -multiple true \
                       -filetypes $::filetypes \
                       -initialdir $::fileopendir]
    if {$files ne ""} {
        foreach filename $files {
            open_file $filename
        }
        set ::fileopendir [file dirname $filename]
    }
}

# TODO set the current font family & size via the -fontmap option:
# http://wiki.tcl.tk/41871
proc ::pd_menucommands::menu_print {mytoplevel} {
    set initialfile "[file rootname [lookup_windowname $mytoplevel]].ps"
    set filename [tk_getSaveFile -initialfile $initialfile \
                      -defaultextension .ps \
                      -filetypes { {{Postscript} {.ps}} }]
    if {$filename ne ""} {
        set tkcanvas [tkcanvas_name $mytoplevel]
        if {$::font_family eq "DejaVu Sans Mono"} {
            # FIXME hack to fix incorrect PS font naming,
            # this could be removed in the future
            set ps [$tkcanvas postscript]
            regsub -all "DejavuSansMono" $ps "DejaVuSansMono" ps
            set f [open $filename w]
            puts $f $ps
            close $f
        } else {
            $tkcanvas postscript -file $filename
        }
    }
}

# ------------------------------------------------------------------------------
# functions called from Edit menu

proc ::pd_menucommands::menu_undo {} {
    if { $::focused_window ne ".pdwindow" } {
        pdsend "$::focused_window undo"
    }
}

proc ::pd_menucommands::menu_redo {} {
    if { $::focused_window ne ".pdwindow" } {
        pdsend "$::focused_window redo"
    }
}

proc ::pd_menucommands::menu_editmode {state} {
    if {[winfo class $::focused_window] ne "PatchWindow"} {return}
    set ::editmode_button $state
# this shouldn't be necessary because 'pd' will reply with pdtk_canvas_editmode
#    set ::editmode($::focused_window) $state
    pdsend "$::focused_window editmode $state"
}

proc ::pd_menucommands::menu_toggle_editmode {} {
    menu_editmode [expr {! $::editmode_button}]
}

# ------------------------------------------------------------------------------
# generic procs for sending menu events

# send a message to a pd canvas receiver
proc ::pd_menucommands::menu_send {window message} {
    set mytoplevel [winfo toplevel $window]
    if {[winfo class $mytoplevel] eq "PatchWindow"} {
        pdsend "$mytoplevel $message"
    } elseif {$mytoplevel eq ".pdwindow"} {
        if {$message eq "copy"} {
            tk_textCopy .pdwindow.text
        } elseif {$message eq "selectall"} {
            .pdwindow.text tag add sel 1.0 end
        } elseif {$message eq "menusaveas"} {
            ::pdwindow::save_logbuffer_to_file
        }
    }
}

# send a message to a pd canvas receiver with a float arg
proc ::pd_menucommands::menu_send_float {window message float} {
    set mytoplevel [winfo toplevel $window]
    if {[winfo class $mytoplevel] eq "PatchWindow"} {
        pdsend "$mytoplevel $message $float"
    }
}

# ------------------------------------------------------------------------------
# open the dialog panels

proc ::pd_menucommands::menu_message_dialog {} {
    ::dialog_message::open_message_dialog $::focused_window
}

proc ::pd_menucommands::menu_find_dialog {} {
    ::dialog_find::open_find_dialog $::focused_window
}

proc ::pd_menucommands::menu_font_dialog {} {
    if {[winfo exists .font]} {
        raise .font
        focus .font
    } elseif {$::focused_window eq ".pdwindow"} {
        pdtk_canvas_dofont .pdwindow [lindex [.pdwindow.text cget -font] 1]
    } else {
        pdsend "$::focused_window menufont"
    }
}

proc ::pd_menucommands::menu_path_dialog {} {
    if {[winfo exists .path]} {
        raise .path
        focus .path
    } else {
        pdsend "pd start-path-dialog"
    }
}

proc ::pd_menucommands::menu_startup_dialog {} {
    if {[winfo exists .startup]} {
        raise .startup
        focus .startup
    } else {
        pdsend "pd start-startup-dialog"
    }
}

proc ::pd_menucommands::menu_manual {} {
    ::pd_menucommands::menu_doc_open doc/1.manual index.htm
}

proc ::pd_menucommands::menu_helpbrowser {} {
    ::helpbrowser::open_helpbrowser
}

# ------------------------------------------------------------------------------
# window management functions

proc ::pd_menucommands::menu_minimize {window} {
    wm iconify [winfo toplevel $window]
}

proc ::pd_menucommands::menu_maximize {window} {
    wm state [winfo toplevel $window] zoomed
}

proc ::pd_menucommands::menu_raise_pdwindow {} {
    # explicitly raise/lower & focus relative to the current window stack for Tk Cocoa
    if {$::focused_window eq ".pdwindow" && [winfo viewable .pdwindow]} {
        lower .pdwindow [lindex [wm stackorder .] 0]
        focus [lindex [wm stackorder .] end]
    } else {
        wm deiconify .pdwindow
        raise .pdwindow [lindex [wm stackorder .] end]
        focus .pdwindow
    }
}

# used for cycling thru windows of an app
proc ::pd_menucommands::menu_raisepreviouswindow {} {
    set mytoplevel [lindex [wm stackorder .] end]
    lower $mytoplevel [lindex [wm stackorder .] 0]
    focus $mytoplevel
}

# used for cycling thru windows of an app the other direction
proc ::pd_menucommands::menu_raisenextwindow {} {
    set mytoplevel [lindex [wm stackorder .] 0]
    raise $mytoplevel [lindex [wm stackorder .] end]
    focus $mytoplevel
}

# ------------------------------------------------------------------------------
# Pd window functions
proc menu_clear_console {} {
    ::pdwindow::clear_console
}

# ------------------------------------------------------------------------------
# manage the saving of the directories for the new commands

# this gets the dir from the path of a window's title
proc ::pd_menucommands::set_filenewdir {mytoplevel} {
    # TODO add Aqua specifics once g_canvas.c has [wm attributes -titlepath]
    if {$mytoplevel eq ".pdwindow"} {
        set ::filenewdir $::fileopendir
    } else {
        regexp -- ".+ - (.+)" [wm title $mytoplevel] ignored ::filenewdir
    }
}

# parse the textfile for the About Pd page
proc ::pd_menucommands::menu_aboutpd {} {
    set versionstring "Pd $::PD_MAJOR_VERSION.$::PD_MINOR_VERSION.$::PD_BUGFIX_VERSION$::PD_TEST_VERSION"
    set filename "$::sys_libdir/doc/1.manual/1.introduction.txt"
    if {[winfo exists .aboutpd]} {
        wm deiconify .aboutpd
        raise .aboutpd
        focus .aboutpd
    } else {
        toplevel .aboutpd -class TextWindow
        wm title .aboutpd [_ "About Pd"]
        wm group .aboutpd .
        .aboutpd configure -menu $::dialog_menubar
        text .aboutpd.text -relief flat -borderwidth 0 -highlightthickness 0 \
            -yscrollcommand ".aboutpd.scroll set" -background white
        scrollbar .aboutpd.scroll -command ".aboutpd.text yview"
        pack .aboutpd.scroll -side right -fill y
        pack .aboutpd.text -side left -fill both -expand 1
        bind .aboutpd <$::modifier-Key-w> "destroy .aboutpd"

        set textfile [open $filename]
        while {![eof $textfile]} {
            set bigstring [read $textfile 1000]
            regsub -all PD_BASEDIR $bigstring $::sys_libdir bigstring2
            regsub -all PD_VERSION $bigstring2 $versionstring bigstring3
            .aboutpd.text insert end $bigstring3
        }
        close $textfile
    }
}

# ------------------------------------------------------------------------------
# opening docs as menu items (like the Test Audio and MIDI patch and the manual)
proc ::pd_menucommands::menu_doc_open {dir basename} {
    if {[file pathtype $dir] eq "relative"} {
        set dirname "$::sys_libdir/$dir"
    } else {
        set dirname $dir
    }
    set textextension "[string tolower [file extension $basename]]"
    if {[lsearch -exact [lindex $::filetypes 0 1]  $textextension] > -1} {
        set fullpath [file normalize [file join $dirname $basename]]
        set dirname [file dirname $fullpath]
        set basename [file tail $fullpath]
        pdsend "pd open [enquote_path $basename] [enquote_path $dirname] 1"
    } else {
        ::pd_menucommands::menu_openfile "$dirname/$basename"
    }
}

# open HTML docs from the menu using the OS-default HTML viewer
proc ::pd_menucommands::menu_openfile {filename} {
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
# open the help-intro.pd patch which provides a list of core objects
proc ::pd_menucommands::menu_objectlist {} {
    pdsend "pd help-intro"
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
