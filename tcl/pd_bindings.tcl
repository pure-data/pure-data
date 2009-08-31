package provide pd_bindings 0.1

package require pd_menucommands
package require dialog_find

namespace eval ::pd_bindings:: {
    variable modifier

    namespace export window_bindings
    namespace export dialog_bindings
    namespace export canvas_bindings
}

# the commands are bound using "" quotations so that the $mytoplevel is
# interpreted immediately.  Since the command is being bound to $mytoplevel,
# it makes sense to have value of $mytoplevel already in the command.  This is
# the opposite of the menu commands in pd_menus.tcl

# binding by class is not recursive, so its useful for certain things
proc ::pd_bindings::class_bindings {} {
    # and the Pd window is in a class to itself
    bind PdWindow <Configure>              "::pd_bindings::window_configure %W"
    bind PdWindow <FocusIn>                "::pd_bindings::window_focusin %W"
    # bind to all the canvas windows
    bind CanvasWindow <Map>                    "::pd_bindings::map %W"
    bind CanvasWindow <Unmap>                  "::pd_bindings::unmap %W"
    bind CanvasWindow <Configure>              "::pd_bindings::window_configure %W"
    bind CanvasWindow <FocusIn>                "::pd_bindings::window_focusin %W"
    # bindings for dialog windows, which behave differently than canvas windows
    bind DialogWindow <Configure>              "::pd_bindings::dialog_configure %W"
    bind DialogWindow <FocusIn>                "::pd_bindings::dialog_focusin %W"
}

proc ::pd_bindings::window_bindings {mytoplevel} {
    variable modifier

    # for key bindings
    if {$::windowingsystem eq "aqua"} {
        set modifier "Mod1"
    } else {
        set modifier "Control"
    }

    # File menu
    bind $mytoplevel <$modifier-Key-b>        "menu_helpbrowser"
    bind $mytoplevel <$modifier-Key-f>        "::dialog_find::menu_find_dialog $mytoplevel"
    bind $mytoplevel <$modifier-Key-n>        "menu_new"
    bind $mytoplevel <$modifier-Key-o>        "menu_open"
    bind $mytoplevel <$modifier-Key-p>        "menu_print $mytoplevel"
    bind $mytoplevel <$modifier-Key-q>        "pdsend \"pd verifyquit\""
    bind $mytoplevel <$modifier-Key-r>        "menu_raise_pdwindow"
    bind $mytoplevel <$modifier-Shift-Key-L>  "menu_clear_console"
    bind $mytoplevel <$modifier-Shift-Key-Q>  "pdsend \"pd quit\""
    bind $mytoplevel <$modifier-Shift-Key-R>  "menu_toggle_console"

    # DSP control
    bind $mytoplevel <$modifier-Key-slash>    "pdsend \"pd dsp 1\""
    bind $mytoplevel <$modifier-Key-period>   "pdsend \"pd dsp 0\""
}

proc ::pd_bindings::pdwindow_bindings {mytoplevel} {
    variable modifier

    window_bindings $mytoplevel

    # TODO update this to work with the console, if it is used
    bind $mytoplevel <$modifier-Key-a> ".pdwindow.text tag add sel 1.0 end"
    bind $mytoplevel <$modifier-Key-x> "tk_textCut .pdwindow.text"
    bind $mytoplevel <$modifier-Key-c> "tk_textCopy .pdwindow.text"
    bind $mytoplevel <$modifier-Key-v> "tk_textPaste .pdwindow.text"
    bind $mytoplevel <$modifier-Key-w> "wm iconify $mytoplevel"

    if {$::windowingsystem eq "aqua"} {
        bind $mytoplevel <$modifier-Key-m>   "menu_minimize $mytoplevel"
        bind $mytoplevel <$modifier-Key-t>   "menu_font_dialog $mytoplevel"
        bind $mytoplevel <$modifier-quoteleft> "menu_raisenextwindow"
    } else {
        bind $mytoplevel <$modifier-Key-m>    "menu_message_dialog"
        bind $mytoplevel <$modifier-Key-t>    "menu_texteditor"
    }

    # Tcl event bindings
    wm protocol $mytoplevel WM_DELETE_WINDOW "pdsend \"pd verifyquit\""
}

# this is for the dialogs: find, font, sendmessage, gatom properties, array
# properties, iemgui properties, canvas properties, data structures
# properties, Audio setup, and MIDI setup
proc ::pd_bindings::dialog_bindings {mytoplevel dialogname} {
    variable modifier

    window_bindings $mytoplevel

    bind $mytoplevel <KeyPress-Escape> "dialog_${dialogname}::cancel $mytoplevel"
    bind $mytoplevel <KeyPress-Return> "dialog_${dialogname}::ok $mytoplevel"
    bind $mytoplevel <$modifier-Key-w> "dialog_${dialogname}::cancel $mytoplevel"

    $mytoplevel configure -padx 10 -pady 5
    wm group $mytoplevel .
    wm resizable $mytoplevel 0 0
    wm protocol $mytoplevel WM_DELETE_WINDOW "dialog_${dialogname}::cancel $mytoplevel"
    catch { # not all platforms/Tcls versions have these options
        wm attributes $mytoplevel -topmost 1
        #wm attributes $mytoplevel -transparent 1
        #$mytoplevel configure -highlightthickness 1
    }
}

proc ::pd_bindings::canvas_bindings {mytoplevel} {
    variable modifier
    set mycanvas $mytoplevel.c

    window_bindings $mytoplevel

    # key bindings -------------------------------------------------------------
    bind $mytoplevel <$modifier-Key-1>        "pdsend \"$mytoplevel obj\""
    bind $mytoplevel <$modifier-Key-2>        "pdsend \"$mytoplevel msg\""
    bind $mytoplevel <$modifier-Key-3>        "pdsend \"$mytoplevel floatatom\""
    bind $mytoplevel <$modifier-Key-4>        "pdsend \"$mytoplevel symbolatom\""
    bind $mytoplevel <$modifier-Key-5>        "pdsend \"$mytoplevel text\""
    bind $mytoplevel <$modifier-Key-a>        "pdsend \"$mytoplevel selectall\""
    bind $mytoplevel <$modifier-Key-c>        "pdsend \"$mytoplevel copy\""
    bind $mytoplevel <$modifier-Key-d>        "pdsend \"$mytoplevel duplicate\""
    bind $mytoplevel <$modifier-Key-e>        "pdsend \"$mytoplevel editmode 0\""
    bind $mytoplevel <$modifier-Key-g>        "pdsend \"$mytoplevel findagain\""
    bind $mytoplevel <$modifier-Key-s>        "pdsend \"$mytoplevel menusave\""
    bind $mytoplevel <$modifier-Key-v>        "pdsend \"$mytoplevel paste\""
    bind $mytoplevel <$modifier-Key-w>        "pdsend \"$mytoplevel menuclose 0\""
    bind $mytoplevel <$modifier-Key-x>        "pdsend \"$mytoplevel cut\""
    bind $mytoplevel <$modifier-Key-z>        "menu_undo $mytoplevel"

    # annoying, but Tk's bind needs uppercase letter to get the Shift
    bind $mytoplevel <$modifier-Shift-Key-B> "pdsend \"$mytoplevel bng 1\""
    bind $mytoplevel <$modifier-Shift-Key-C> "pdsend \"$mytoplevel mycnv 1\""
    bind $mytoplevel <$modifier-Shift-Key-D> "pdsend \"$mytoplevel vradio 1\""
    bind $mytoplevel <$modifier-Shift-Key-H> "pdsend \"$mytoplevel hslider 1\""
    bind $mytoplevel <$modifier-Shift-Key-I> "pdsend \"$mytoplevel hradio 1\""
    bind $mytoplevel <$modifier-Shift-Key-N> "pdsend \"$mytoplevel numbox 1\""
    bind $mytoplevel <$modifier-Shift-Key-S> "pdsend \"$mytoplevel menusaveas\""
    bind $mytoplevel <$modifier-Shift-Key-T> "pdsend \"$mytoplevel toggle 1\""
    bind $mytoplevel <$modifier-Shift-Key-U> "pdsend \"$mytoplevel vumeter 1\""
    bind $mytoplevel <$modifier-Shift-Key-V> "pdsend \"$mytoplevel vslider 1\""
    bind $mytoplevel <$modifier-Shift-Key-W> "pdsend \"$mytoplevel menuclose 1\""
    bind $mytoplevel <$modifier-Shift-Key-Z> "menu_redo $mytoplevel"
    
    if {$::windowingsystem eq "aqua"} {
        bind $mytoplevel <$modifier-Key-m>   "menu_minimize $mytoplevel"
        bind $mytoplevel <$modifier-Key-t>   "menu_font_dialog $mytoplevel"
        bind $mytoplevel <$modifier-quoteleft> "menu_raisenextwindow"
    } else {
        bind $mytoplevel <$modifier-Key-m>    "menu_message_dialog"
        bind $mytoplevel <$modifier-Key-t>    "menu_texteditor"
    }
    bind $mytoplevel <KeyPress>         "::pd_bindings::sendkey %W 1 %K %A 0"
    bind $mytoplevel <KeyRelease>       "::pd_bindings::sendkey %W 0 %K %A 0"
    bind $mytoplevel <Shift-KeyPress>   "::pd_bindings::sendkey %W 1 %K %A 1"
    bind $mytoplevel <Shift-KeyRelease> "::pd_bindings::sendkey %W 0 %K %A 1"

    # mouse bindings -----------------------------------------------------------
    # these need to be bound to $mycanvas because %W will return $mytoplevel for
    # events over the window frame and $mytoplevel.c for events over the canvas
    bind $mycanvas <Motion>                   "pdtk_canvas_motion %W %x %y 0"
    bind $mycanvas <$modifier-Motion>         "pdtk_canvas_motion %W %x %y 2"
    bind $mycanvas <ButtonPress-1>            "pdtk_canvas_mouse %W %x %y %b 0"
    bind $mycanvas <ButtonRelease-1>          "pdtk_canvas_mouseup %W %x %y %b"
    bind $mycanvas <$modifier-ButtonPress-1>  "pdtk_canvas_mouse %W %x %y %b 2"
    # TODO look into "virtual events' for a means for getting Shift-Button, etc.
    switch -- $::windowingsystem { 
        "aqua" {
            bind $mycanvas <ButtonPress-2>      "pdtk_canvas_rightclick %W %x %y %b"
            # on Mac OS X, make a rightclick with Ctrl-click for 1 button mice
            bind $mycanvas <Control-Button-1> "pdtk_canvas_rightclick %W %x %y %b"
            # TODO try replacing the above with this
            #bind all <Control-Button-1> {event generate %W <Button-2> \
            #                                      -x %x -y %y -rootx %X -rooty %Y \
            #                                      -button 2 -time %t}
        } "x11" {
            bind $mycanvas <ButtonPress-3>    "pdtk_canvas_rightclick %W %x %y %b"
            # on X11, button 2 "pastes" from the X windows clipboard
            bind $mycanvas <ButtonPress-2>   "pdtk_canvas_clickpaste %W %x %y %b"
        } "win32" {
            bind $mycanvas <ButtonPress-3>   "pdtk_canvas_rightclick %W %x %y %b"
        }
    }
    #TODO bind $mytoplevel <MouseWheel>

    # window protocol bindings
    wm protocol $mytoplevel WM_DELETE_WINDOW "pdsend \"$mytoplevel menuclose 0\""
    bind $mycanvas <Destroy> "::pd_bindings::window_destroy %W"
}


#------------------------------------------------------------------------------#
# event handlers

proc ::pd_bindings::window_configure {mytoplevel} {
    pdtk_canvas_getscroll $mytoplevel
}
    
proc ::pd_bindings::window_destroy {mycanvas} {
    set mytoplevel [winfo toplevel $mycanvas]
    unset ::editmode($mytoplevel)
}

# do tasks when changing focus (Window menu, scrollbars, etc.)
proc ::pd_bindings::window_focusin {mytoplevel} {
    # pdtk_post "::pd_bindings::window_focusin $mytoplevel"
    set ::focused_window $mytoplevel
    ::dialog_find::set_canvas_to_search $mytoplevel
    ::pd_menucommands::set_menu_new_dir $mytoplevel
    ::dialog_font::update_font_dialog $mytoplevel
    if {$mytoplevel eq ".pdwindow"} {
        ::pd_menus::configure_for_pdwindow 
    } else {
        ::pd_menus::configure_for_canvas $mytoplevel
    }
    # TODO handle enabling/disabling the Undo and Redo menu items in Edit
    # TODO handle enabling/disabling the Cut/Copy/Paste menu items in Edit
    # TODO enable menu items that the Pd window or dialogs might have disabled
    # TODO update "Open Recent" menu
}

proc ::pd_bindings::dialog_configure {mytoplevel} {
}

proc ::pd_bindings::dialog_focusin {mytoplevel} {
    # TODO disable things on the menus that don't work for dialogs
    ::pd_menus::configure_for_dialog $mytoplevel
}

# "map" event tells us when the canvas becomes visible, and "unmap",
# invisible.  Invisibility means the Window Manager has minimized us.  We
# don't get a final "unmap" event when we destroy the window.
proc ::pd_bindings::map {mytoplevel} {
    pdsend "$mytoplevel map 1"
}

proc ::pd_bindings::unmap {mytoplevel} {
    pdsend "$mytoplevel map 0"
}


#------------------------------------------------------------------------------#
# key usage

proc ::pd_bindings::sendkey {mycanvas state key iso shift} {
    # TODO canvas_key on the C side should be refactored with this proc as well
    switch -- $key {
        "BackSpace" { set iso ""; set key 8    }
        "Tab"       { set iso ""; set key 9 }
        "Return"    { set iso ""; set key 10 }
        "Escape"    { set iso ""; set key 27 }
        "Space"     { set iso ""; set key 32 }
        "Delete"    { set iso ""; set key 127 }
        "KP_Delete" { set iso ""; set key 127 }
    }
    if {$iso ne ""} {
        scan $iso %c key
    }
    puts "::pd_bindings::sendkey {%W:$mycanvas $state %K$key %A$iso $shift}"
    # $mycanvas might be a toplevel, but [winfo toplevel] does the right thing
    pdsend "[winfo toplevel $mycanvas] key $state $key $shift"
}
