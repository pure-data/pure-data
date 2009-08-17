package provide pd_bindings 0.1

package require pd_menucommands
package require dialog_find

namespace eval ::pd_bindings:: {
    variable modifier

    namespace export window_bindings
    namespace export panel_bindings
    namespace export canvas_bindings
}

proc ::pd_bindings::class_bindings {} {
    # binding by class is not recursive, so its useful for certain things
    bind CanvasWindow <Map>                    "::pd_bindings::map %W"
    bind CanvasWindow <Unmap>                  "::pd_bindings::unmap %W"
    bind CanvasWindow <Configure>              "::pd_bindings::window_configure %W"
    bind CanvasWindow <FocusIn>                "::pd_bindings::window_focusin %W"
    bind CanvasWindow <Activate>               "::pd_bindings::window_focusin %W"
}

proc ::pd_bindings::window_bindings {mytoplevel} {
    variable modifier

    # for key bindings
    # puts "::windowingsystem $::windowingsystem"
    if {$::windowingsystem eq "aqua"} {
        set modifier "Mod1"
    } else {
        set modifier "Control"
    }

    # File menu
    bind $mytoplevel <$modifier-Key-b>        "menu_helpbrowser"
    bind $mytoplevel <$modifier-Key-f>        "::dialog_find::menu_dialog_find $mytoplevel"
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
    bind $mytoplevel <$modifier-Key-a> ".printout.text tag add sel 1.0 end"
    bind $mytoplevel <$modifier-Key-x> "tk_textCut .printout.text"
    bind $mytoplevel <$modifier-Key-c> "tk_textCopy .printout.text"
    bind $mytoplevel <$modifier-Key-v> "tk_textPaste .printout.text"
    bind $mytoplevel <$modifier-Key-w> { }

    # Tcl event bindings
    wm protocol $mytoplevel WM_DELETE_WINDOW "pdsend \"pd verifyquit\""

    # do window maintenance when entering the Pd window (Window menu, scrollbars, etc)
    # bind $mytoplevel <FocusIn>                "::pd_bindings::window_focusin %W"
}

# this is for the panels: find, font, sendmessage, gatom properties, array
# properties, iemgui properties, canvas properties, data structures
# properties, Audio setup, and MIDI setup
proc ::pd_bindings::panel_bindings {mytoplevel panelname} {
    variable modifier

    window_bindings $mytoplevel

    bind $mytoplevel <KeyPress-Escape> [format "%s_cancel %s" $panelname $mytoplevel]
    bind $mytoplevel <KeyPress-Return> [format "%s_ok %s" $panelname $mytoplevel]
    bind $mytoplevel <$modifier-Key-w> [format "%s_cancel %s" $panelname $mytoplevel]

    wm protocol $mytoplevel WM_DELETE_WINDOW "${panelname}_cancel $mytoplevel"

    bind $mytoplevel <FocusIn>                "::pd_bindings::panel_focusin %W"
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
    bind $mytoplevel <$modifier-Key-slash>    "pdsend \"pd dsp 1\""
    bind $mytoplevel <$modifier-Key-period>   "pdsend \"pd dsp 0\""

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
        bind $mytoplevel <$modifier-Key-t>   "menu_dialog_font $mytoplevel"
    bind $mytoplevel <$modifier-quoteleft> "menu_raisenextwindow"
    } else {
        bind $mytoplevel <$modifier-Key-m>    "menu_message_panel"
        bind $mytoplevel <$modifier-Key-t>    "menu_texteditor"
    }

    bind $mycanvas <Key>        "pdsend_key %W 1 %K %A 0"
    bind $mycanvas <Shift-Key>  "pdsend_key %W 1 %K %A 1"
    bind $mycanvas <KeyRelease> "pdsend_key %W 0 %K %A 0"

    # mouse bindings -----------------------------------------------------------
    # these need to be bound to $mytoplevel.c because %W will return $mytoplevel for
    # events over the window frame and $mytoplevel.c for events over the canvas
    bind $mycanvas <Motion>               "pdtk_canvas_motion %W %x %y 0"
    bind $mycanvas <Button-1>             "pdtk_canvas_mouse %W %x %y %b 0"
    bind $mycanvas <ButtonRelease-1>      "pdtk_canvas_mouseup %W %x %y %b"
    bind $mycanvas <$modifier-Button-1>   "pdtk_canvas_mouse %W %x %y %b 2"
    # TODO look into "virtual events' for a means for getting Shift-Button, etc.
    switch -- $::windowingsystem { 
        "aqua" {
            bind $mycanvas <Button-2>      "pdtk_canvas_rightclick %W %x %y %b"
            # on Mac OS X, make a rightclick with Ctrl-click for 1 button mice
            bind $mycanvas <Control-Button-1> "pdtk_canvas_rightclick %W %x %y %b"
            # TODO try replacing the above with this
            #bind all <Control-Button-1> {event generate %W <Button-2> \
            #                                     -x %x -y %y -rootx %X -rooty %Y \
            #                                     -button 2 -time %t}
        } "x11" {
            bind $mycanvas <Button-3>      "pdtk_canvas_rightclick %W %x %y %b"
            # on X11, button 2 "pastes" from the X windows clipboard
            bind $mycanvas <Button-2>      "pdtk_canvas_clickpaste %W %x %y %b"
        } "win32" {
            bind $mycanvas <Button-3>      "pdtk_canvas_rightclick %W %x %y %b"
        }
    }
    #TODO bind $mytoplevel <MouseWheel>

    # window protocol bindings
    wm protocol $mytoplevel WM_DELETE_WINDOW "pdsend \"$mytoplevel menuclose 0\""
}


#------------------------------------------------------------------------------#
# event handlers

proc ::pd_bindings::window_configure {mytoplevel} {
    pdtk_canvas_getscroll $mytoplevel
}
    
# do tasks when changing focus (Window menu, scrollbars, etc.)
proc ::pd_bindings::window_focusin {mytoplevel} {
    ::dialog_find::set_canvas_to_search $mytoplevel
    ::pd_menucommands::set_menu_new_dir $mytoplevel
    # TODO handle enabling/disabling the Undo and Redo menu items in Edit
    # TODO handle enabling/disabling the Cut/Copy/Paste menu items in Edit
    # TODO enable menu items that the Pd window or panels might have disabled
}

proc ::pd_bindings::panel_focusin {mytoplevel} {
    # TODO disable things on the menus that don't work for panels
}

# "map" event tells us when the canvas becomes visible, and "unmap",
# invisible.  Invisibility means the Window Manager has minimized us.  We
# don't get a final "unmap" event when we destroy the window.
proc ::pd_bindings::map {mytoplevel} {
    # puts "map $mytoplevel [wm title $mytoplevel]"
    pdsend "$mytoplevel map 1"
}

proc ::pd_bindings::unmap {mytoplevel} {
    pdsend "$mytoplevel map 0"
}
