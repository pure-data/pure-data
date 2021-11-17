package provide pd_bindings 0.1

package require pd_menucommands
package require dialog_find
package require pd_connect

namespace eval ::pd_bindings:: {
    namespace export global_bindings
    namespace export dialog_bindings
    namespace export patch_bindings
    variable key2iso
}
set ::pd_bindings::key2iso ""


# wrapper around bind(3tk)to deal with CapsLock
# the actual bind-sequence is is build as '<${seq_prefix}-${seq_nocase}',
# with the $seq_nocase part bing bound both as upper-case and lower-case
proc ::pd_bindings::bind_capslock {tag seq_prefix seq_nocase script} {
    bind $tag <${seq_prefix}-[string tolower ${seq_nocase}]> "::pd_menucommands::scheduleAction $script"
    bind $tag <${seq_prefix}-[string toupper ${seq_nocase}]> "::pd_menucommands::scheduleAction $script"
}

# TODO rename pd_bindings to window_bindings after merge is done

# Some commands are bound using "" quotations so that the $mytoplevel is
# interpreted immediately.  Since the command is being bound to $mytoplevel,
# it makes sense to have value of $mytoplevel already in the command.  This is
# the opposite of most menu/bind commands here and in pd_menus.tcl, which use
# {} to force execution of any variables (i.e. $::focused_window) until later

# binding by class is not recursive, so its useful for window events
proc ::pd_bindings::class_bindings {} {
    # and the Pd window is in a class to itself
    bind PdWindow <FocusIn>           "::pd_bindings::window_focusin %W"
    # bind to all the windows dedicated to patch canvases
    bind PatchWindow <FocusIn>        "::pd_bindings::window_focusin %W"
    bind PatchWindow <Map>            "::pd_bindings::patch_map %W"
    bind PatchWindow <Unmap>          "::pd_bindings::patch_unmap %W"
    bind PatchWindow <Configure>      "::pd_bindings::patch_configure %W %w %h %x %y"
    # dialog panel windows bindings, which behave differently than PatchWindows
    bind DialogWindow <Configure>     "::pd_bindings::dialog_configure %W"
    bind DialogWindow <FocusIn>       "::pd_bindings::dialog_focusin %W"
    # help browser bindings
    bind HelpBrowser <Configure>      "::pd_bindings::dialog_configure %W"
    bind HelpBrowser <FocusIn>        "::pd_bindings::dialog_focusin %W"
}

proc ::pd_bindings::global_bindings {} {
    # we use 'bind all' everywhere to get as much of Tk's automatic binding
    # behaviors as possible, things like not sending an event for 'O' when
    # 'Control-O' is pressed
    bind_capslock all $::modifier-Key a {menu_send %W selectall}
    bind_capslock all $::modifier-Key b {menu_helpbrowser}
    bind_capslock all $::modifier-Key c {menu_send %W copy}
    bind_capslock all $::modifier-Key d {menu_send %W duplicate}
    bind_capslock all $::modifier-Key e {menu_toggle_editmode}
    bind_capslock all $::modifier-Key f {menu_find_dialog}
    bind_capslock all $::modifier-Key g {menu_send %W findagain}
    bind_capslock all $::modifier-Key k {menu_send %W connect_selection}
    bind_capslock all $::modifier-Key n {menu_new}
    bind_capslock all $::modifier-Key o {menu_open}
    bind_capslock all $::modifier-Key p {menu_print $::focused_window}
    bind_capslock all $::modifier-Key r {menu_raise_pdwindow}
    bind_capslock all $::modifier-Key s {menu_send %W menusave}
    bind_capslock all $::modifier-Key t {menu_send %W triggerize}
    bind_capslock all $::modifier-Key v {menu_send %W paste}
    bind_capslock all $::modifier-Key w {::pd_bindings::window_close %W}
    bind_capslock all $::modifier-Key x {menu_send %W cut}
    bind_capslock all $::modifier-Key z {menu_undo}
    bind all <$::modifier-Key-1>        {::pd_menucommands::scheduleAction menu_send_float %W obj 0}
    bind all <$::modifier-Key-2>        {::pd_menucommands::scheduleAction menu_send_float %W msg 0}
    bind all <$::modifier-Key-3>        {::pd_menucommands::scheduleAction menu_send_float %W floatatom 0}
    bind all <$::modifier-Key-4>        {::pd_menucommands::scheduleAction menu_send_float %W listbox 0}
    bind all <$::modifier-Key-5>        {::pd_menucommands::scheduleAction menu_send_float %W text 0}
    bind all <$::modifier-Key-slash>    {::pd_menucommands::scheduleAction pdsend "pd dsp 1"}
    bind all <$::modifier-Key-period>   {::pd_menucommands::scheduleAction pdsend "pd dsp 0"}

    # take the '=' key as a zoom-in accelerator, because '=' is the non-shifted
    # "+" key... this only makes sense on US keyboards but some users
    # expected it... go figure.
    bind all <$::modifier-Key-equal>       {::pd_menucommands::scheduleAction menu_send_float %W zoom 2}
    bind all <$::modifier-Key-plus>        {::pd_menucommands::scheduleAction menu_send_float %W zoom 2}
    bind all <$::modifier-Key-minus>       {::pd_menucommands::scheduleAction menu_send_float %W zoom 1}
    bind all <$::modifier-Key-KP_Add>      {::pd_menucommands::scheduleAction menu_send_float %W zoom 2}
    bind all <$::modifier-Key-KP_Subtract> {::pd_menucommands::scheduleAction menu_send_float %W zoom 1}

    # note: we avoid CMD-H & CMD+Shift-H as it hides Pd on macOS

    # annoying, but Tk's bind needs uppercase letter to get the Shift
    bind_capslock all $::modifier-Shift-Key A {menu_send %W menuarray}
    bind_capslock all $::modifier-Shift-Key B {menu_send %W bng}
    bind_capslock all $::modifier-Shift-Key C {menu_send %W mycnv}
    bind_capslock all $::modifier-Shift-Key D {menu_send %W vradio}
    bind_capslock all $::modifier-Shift-Key G {menu_send %W graph}
    bind_capslock all $::modifier-Shift-Key J {menu_send %W hslider}
    bind_capslock all $::modifier-Shift-Key I {menu_send %W hradio}
    bind_capslock all $::modifier-Shift-Key L {menu_clear_console}
    bind_capslock all $::modifier-Shift-Key M {menu_message_dialog}
    bind_capslock all $::modifier-Shift-Key N {menu_send %W numbox}
    bind_capslock all $::modifier-Shift-Key Q {pdsend "pd quit"}
    bind_capslock all $::modifier-Shift-Key R {menu_send %W tidy}
    bind_capslock all $::modifier-Shift-Key S {menu_send %W menusaveas}
    bind_capslock all $::modifier-Shift-Key T {menu_send %W toggle}
    bind_capslock all $::modifier-Shift-Key U {menu_send %W vumeter}
    bind_capslock all $::modifier-Shift-Key V {menu_send %W vslider}
    bind_capslock all $::modifier-Shift-Key W {::pd_bindings::window_close %W 1}
    bind_capslock all $::modifier-Shift-Key Z {menu_redo}
    bind all <KeyPress-Escape>         {menu_send %W deselectall; ::pd_bindings::sendkey %W 1 %K %A 1 %k}

    # OS-specific bindings
    if {$::windowingsystem eq "aqua"} {
         # TK 8.5+ Cocoa handles quit, minimize, & raise next window for us
        if {$::tcl_version < 8.5} {
            bind_capslock all $::modifier-Key q       {::pd_menucommands::scheduleAction ::pd_connect::menu_quit}
            bind_capslock all $::modifier-Key m       {::pd_menucommands::scheduleAction menu_minimize %W}
            bind all <$::modifier-quoteleft>   {::pd_menucommands::scheduleAction menu_raisenextwindow}
        }
        # BackSpace/Delete report the wrong isos (unicode representations) on OSX,
        # so we set them to the empty string and let ::pd_bindings::sendkey guess the correct values
        bind all <KeyPress-BackSpace>      {::pd_bindings::sendkey %W 1 %K "" 1 %k}
        bind all <KeyRelease-BackSpace>    {::pd_bindings::sendkey %W 0 %K "" 1 %k}
        bind all <KeyPress-Delete>         {::pd_bindings::sendkey %W 1 %K "" 1 %k}
        bind all <KeyRelease-Delete>       {::pd_bindings::sendkey %W 0 %K "" 1 %k}
        bind all <KeyPress-KP_Enter>       {::pd_bindings::sendkey %W 1 %K "" 1 %k}
        bind all <KeyRelease-KP_Enter>     {::pd_bindings::sendkey %W 0 %K "" 1 %k}
        bind all <KeyPress-Clear>          {::pd_bindings::sendkey %W 1 %K "" 1 %k}
        bind all <KeyRelease-Clear>        {::pd_bindings::sendkey %W 0 %K "" 1 %k}
    } else {
        bind_capslock all $::modifier-Key q       {::pd_connect::menu_quit}
        bind_capslock all $::modifier-Key m       {menu_minimize %W}

        bind all <$::modifier-Next>        {menu_raisenextwindow}    ;# PgUp
        bind all <$::modifier-Prior>       {menu_raisepreviouswindow};# PageDown
        # these can conflict with CMD+comma & CMD+period bindings in Tk Cococa
        bind all <$::modifier-greater>     {menu_raisenextwindow}
        bind all <$::modifier-less>        {menu_raisepreviouswindow}
    }

    bind all <KeyPress>         {::pd_bindings::sendkey %W 1 %K %A 0 %k}
    bind all <KeyRelease>       {::pd_bindings::sendkey %W 0 %K %A 0 %k}
    bind all <Shift-KeyPress>   {::pd_bindings::sendkey %W 1 %K %A 1 %k}
    bind all <Shift-KeyRelease> {::pd_bindings::sendkey %W 0 %K %A 1 %k}
}

# bindings for .pdwindow are found in ::pdwindow::pdwindow_bindings in pdwindow.tcl

# this is for the dialogs: find, font, sendmessage, gatom properties, array
# properties, iemgui properties, canvas properties, data structures
# properties, Audio setup, and MIDI setup
proc ::pd_bindings::dialog_bindings {mytoplevel dialogname} {
    variable modifier

    bind $mytoplevel <KeyPress-Escape>          "dialog_${dialogname}::cancel $mytoplevel"
    bind $mytoplevel <KeyPress-Return>          "dialog_${dialogname}::ok $mytoplevel"
    bind_capslock $mytoplevel $::modifier-Key w "dialog_${dialogname}::cancel $mytoplevel"
    # these aren't supported in the dialog, so alert the user, then break so
    # that no other key bindings are run
    if {$mytoplevel ne ".find"} {
        bind_capslock $mytoplevel $::modifier-Key s       {bell; break}
        bind_capslock $mytoplevel $::modifier-Shift-Key s {bell; break}
        bind_capslock $mytoplevel $::modifier-Key p       {bell; break}
    } else {
        # find may may allow passthrough to it's target search patch
        ::dialog_find::update_bindings
    }
    bind_capslock $mytoplevel $::modifier-Key t           {bell; break}

    wm protocol $mytoplevel WM_DELETE_WINDOW "dialog_${dialogname}::cancel $mytoplevel"
}

# this is for canvas windows
proc ::pd_bindings::patch_bindings {mytoplevel} {
    variable modifier
    set tkcanvas [tkcanvas_name $mytoplevel]

    # on Mac OS X/Aqua, the Alt/Option key is called Option in Tcl
    if {$::windowingsystem eq "aqua"} {
        set alt "Option"
    } else {
        set alt "Alt"
    }

    # TODO move mouse bindings to global and bind to 'all'

    # mouse bindings -----------------------------------------------------------
    # these need to be bound to $tkcanvas because %W will return $mytoplevel for
    # events over the window frame and $tkcanvas for events over the canvas
    bind $tkcanvas <Motion>                    "pdtk_canvas_motion %W %x %y 0"
    bind $tkcanvas <Shift-Motion>              "pdtk_canvas_motion %W %x %y 1"
    bind $tkcanvas <$::modifier-Motion>        "pdtk_canvas_motion %W %x %y 2"
    bind $tkcanvas <$::modifier-Shift-Motion>  "pdtk_canvas_motion %W %x %y 3"
    bind $tkcanvas <$alt-Motion>               "pdtk_canvas_motion %W %x %y 4"
    bind $tkcanvas <$alt-Shift-Motion>         "pdtk_canvas_motion %W %x %y 5"
    bind $tkcanvas <$::modifier-$alt-Motion>   "pdtk_canvas_motion %W %x %y 6"
    bind $tkcanvas <$::modifier-$alt-Shift-Motion> "pdtk_canvas_motion %W %x %y 7"

    bind $tkcanvas <ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 0"
    bind $tkcanvas <Shift-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 1"
    bind $tkcanvas <$::modifier-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 2"
    bind $tkcanvas <$::modifier-Shift-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 3"
    bind $tkcanvas <$alt-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 4"
    bind $tkcanvas <$alt-Shift-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 5"
    bind $tkcanvas <$::modifier-$alt-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 6"
    bind $tkcanvas <$::modifier-$alt-Shift-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 7"

    bind $tkcanvas <ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 0"
    bind $tkcanvas <Shift-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 1"
    bind $tkcanvas <$::modifier-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 2"
    bind $tkcanvas <$::modifier-Shift-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 3"
    bind $tkcanvas <$alt-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 4"
    bind $tkcanvas <$alt-Shift-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 5"
    bind $tkcanvas <$::modifier-$alt-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 6"
    bind $tkcanvas <$::modifier-$alt-Shift-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 7"

    if {$::windowingsystem eq "x11"} {
        # from http://wiki.tcl.tk/3893
        bind all <Button-4> \
            {event generate [focus -displayof %W] <MouseWheel> -delta  1}
        bind all <Button-5> \
            {event generate [focus -displayof %W] <MouseWheel> -delta -1}
        bind all <Shift-Button-4> \
            {event generate [focus -displayof %W] <Shift-MouseWheel> -delta  1}
        bind all <Shift-Button-5> \
            {event generate [focus -displayof %W] <Shift-MouseWheel> -delta -1}
    }
    bind $tkcanvas <MouseWheel>       {::pdtk_canvas::scroll %W y %D}
    bind $tkcanvas <Shift-MouseWheel> {::pdtk_canvas::scroll %W x %D}

    # clear interim compose character by sending a virtual BackSpace,
    # these events are pulled from Tk library/entry.tcl
    # should we try to keep track of the "marked text" selection which may be
    # more than a single character? ie. start & length of marked text
    catch {
        # bind $tkcanvas <<TkStartIMEMarkedText>> {
        #     ::pdwindow::post "%W start marked text\n"
        # }
        # bind $tkcanvas <<TkEndIMEMarkedText>> {
        #     ::pdwindow::post "%W end marked text\n"
        # }
        bind $tkcanvas <<TkClearIMEMarkedText>> {
            # ::pdwindow::post "%W clear marked text\n"
            ::pd_bindings::sendkey %W 1 BackSpace "" 0
            ::pd_bindings::sendkey %W 0 BackSpace "" 0
        }
        bind $tkcanvas <<TkAccentBackspace>> {
            # ::pdwindow::post "%W accent backspace\n"
            ::pd_bindings::sendkey %W 1 BackSpace "" 0
            ::pd_bindings::sendkey %W 0 BackSpace "" 0
        }
    } stderr

    # "right clicks" are defined differently on each platform
    switch -- $::windowingsystem {
        "aqua" {
            bind $tkcanvas <ButtonPress-2>    "pdtk_canvas_rightclick %W %x %y %b"
            # on Mac OS X, make a rightclick with Ctrl-click for 1 button mice
            bind $tkcanvas <Control-Button-1> "pdtk_canvas_rightclick %W %x %y %b"
        } "x11" {
            bind $tkcanvas <ButtonPress-3>    "pdtk_canvas_rightclick %W %x %y %b"
            # on X11, button 2 "pastes" from the X windows clipboard
            bind $tkcanvas <ButtonPress-2>    "pdtk_canvas_clickpaste %W %x %y %b"
        } "win32" {
            bind $tkcanvas <ButtonPress-3>    "pdtk_canvas_rightclick %W %x %y %b"
        }
    }

    # <Tab> key to cycle through selection
    bind $tkcanvas <KeyPress-Tab>        "::pd_bindings::canvas_cycle %W  1 %K %A 0 %k"
    bind $tkcanvas <Shift-Tab>           "::pd_bindings::canvas_cycle %W -1 %K %A 1 %k"
    # on X11, <Shift-Tab> is a different key by the name 'ISO_Left_Tab'...
    # other systems (at least aqua) do not like this name, so we 'catch' any errors
    catch {bind $tkcanvas <KeyPress-ISO_Left_Tab> "::pd_bindings::canvas_cycle %W -1 %K %A 1 %k" } stderr

    # window protocol bindings
    wm protocol $mytoplevel WM_DELETE_WINDOW "pdsend \"$mytoplevel menuclose 0\""
    bind $tkcanvas <Destroy> "::pd_bindings::patch_destroy %W"
}


#------------------------------------------------------------------------------#
# event handlers

# do tasks when changing focus (Window menu, scrollbars, etc.)
proc ::pd_bindings::window_focusin {mytoplevel} {
    # focused_window is used throughout for sending bindings, menu commands,
    # etc. to the correct patch receiver symbol.  MSP took out a line that
    # confusingly redirected the "find" window which might be in mid search
    set ::focused_window $mytoplevel
    ::pd_menucommands::set_filenewdir $mytoplevel
    ::dialog_font::update_font_dialog $mytoplevel
    if {$mytoplevel eq ".pdwindow"} {
        ::pd_menus::configure_for_pdwindow
    } else {
        ::pd_menus::configure_for_canvas $mytoplevel
    }
    if {[winfo exists .font]} {wm transient .font $mytoplevel}
    # if we regain focus from another app, make sure to editmode cursor is right
    if {$::editmode($mytoplevel)} {
        $mytoplevel configure -cursor hand2
    }
}

# global window close event, patch windows are closed by pd
# while other window types are closed via their own bindings
# force argument:
#   0: request to close, verifying whether clean or dirty
#   1: request to close, no verification
proc ::pd_bindings::window_close {mytoplevel {force 0}} {
    # catch any non-existent windows
    # ie. the helpbrowser after it's been
    # closed by it's own binding
    if {$force eq 0 && ![winfo exists $mytoplevel]} {
        return
    }
    menu_send_float $mytoplevel menuclose $force
}

# "map" event tells us when the canvas becomes visible, and "unmap",
# invisible.  Invisibility means the Window Manager has minimized us.  We
# don't get a final "unmap" event when we destroy the window.
proc ::pd_bindings::patch_map {mytoplevel} {
    pdsend "$mytoplevel map 1"
    ::pdtk_canvas::finished_loading_file $mytoplevel
}

proc ::pd_bindings::patch_unmap {mytoplevel} {
    pdsend "$mytoplevel map 0"
}

proc ::pd_bindings::patch_configure {mytoplevel width height x y} {
    if {$width == 1 || $height == 1} {
        # make sure the window is fully created
        update idletasks
    }
    # the geometry we receive from the callback is really for the frame
    # however, we need position including the border decoration
    # as this is how we restore the position
    scan [wm geometry $mytoplevel] {%dx%d%[+]%d%[+]%d} width height - x - y
    pdtk_canvas_getscroll [tkcanvas_name $mytoplevel]
    # send the size/location of the window and canvas to 'pd' in the form of:
    #    left top right bottom
    pdsend "$mytoplevel setbounds $x $y [expr $x + $width] [expr $y + $height]"
}

proc ::pd_bindings::patch_destroy {window} {
    set mytoplevel [winfo toplevel $window]
    unset ::editmode($mytoplevel)
    unset ::undo_actions($mytoplevel)
    unset ::redo_actions($mytoplevel)
    unset ::editingtext($mytoplevel)
    unset ::loaded($mytoplevel)
    # unset my entries all of the window data tracking arrays
    array unset ::windowname $mytoplevel
    array unset ::parentwindows $mytoplevel
    array unset ::childwindows $mytoplevel
}

proc ::pd_bindings::dialog_configure {mytoplevel} {
}

proc ::pd_bindings::dialog_focusin {mytoplevel} {
    set ::focused_window $mytoplevel
    ::pd_menus::configure_for_dialog $mytoplevel
    if {$mytoplevel eq ".find"} {::dialog_find::focus_find}
}

# (Shift-)Tab for cycling through selection
proc ::pd_bindings::canvas_cycle {mytoplevel cycledir key iso shift {keycode ""}} {
    menu_send_float $mytoplevel cycleselect $cycledir
    ::pd_bindings::sendkey $mytoplevel 1 $key $iso $shift $keycode
}

#------------------------------------------------------------------------------#
# key usage

# canvas_key() expects to receive the patch's mytoplevel because key messages
# are local to each patch.  Therefore, key messages are not send for the
# dialog panels, the Pd window, help browser, etc. so we need to filter those
# events out.
proc ::pd_bindings::sendkey {window state key iso shift {keycode ""} } {
    #::pdwindow::error "::pd_bindings::sendkey .${state}. .${key}. .${iso}. .${shift}. .${keycode}.\n"

    # state: 1=keypress, 0=keyrelease
    # key (%K): the keysym corresponding to the event, substituted as a textual string
    # iso (%A): substitutes the UNICODE character corresponding to the event, or the empty string if the event does not correspond to a UNICODE character (e.g. the shift key was pressed)
    # shift: 1=shift, 0=no-shift
    # keycode (%k): keyboard code

    if { "$keycode" eq "" } {
        # old fashioned code fails to add %k-parameter; substitute...
        set keycode $key
    }
    # iso:
    # - 1-character: use it!
    # - multi-character: can happen with dead-keys (where an accent turns out to not be an accent after all...; e.g. "^" + "1" -> "^1")
    #              : turn it into multiple key-events (one per character)!
    # - 0-character: we need to calculate iso
    #              : if there's one stored in key2iso, use it (in the case of KeyRelease)
    #              : do some substitution based on $key
    #              : else use $key

    if { [string length $iso] == 0 && $state == 0 } {
        catch {set iso [dict get $::pd_bindings::key2iso $keycode] }
    }

    switch -- [string length $iso] {
        0 {
            switch -- $key {
                "BackSpace" { set key   8 }
                "Tab"       { set key   9 }
                "Return"    { set key  10 }
                "Escape"    { set key  27 }
                "Space"     { set key  32 }
                "space"     { set key  32 }
                "Delete"    { set key 127 }
                "KP_Delete" { set key 127 }
                "KP_Enter"  { set key  10 }
                default     {             }
            }
        }
        1 {
            scan $iso %c key
            if { "$key" eq "13" } { set key 10 }
            catch {
                if { "" eq "${::pd_bindings::key2iso}" } {
                    set ::pd_bindings::key2iso [dict create]
                }
                # store the key2iso mapping
                dict set ::pd_bindings::key2iso $keycode $iso
            }
        }
        default {
            # split a multi-char $iso in single chars
            foreach k [split $iso {}] {
                ::pd_bindings::sendkey $window $state $key $k $shift $keycode
            }
            return
        }
    }

    # some pop-up panels also bind to keys like the enter, but then disappear,
    # so ignore their events.  The inputbox in the Startup dialog does this.
    if {! [winfo exists $window]} {return}

    # $window might be a toplevel or canvas, [winfo toplevel] does the right thing
    set mytoplevel [winfo toplevel $window]
    if {[winfo class $mytoplevel] ne "PatchWindow"} {
        set mytoplevel pd
    }
    pdsend "$mytoplevel key $state $key $shift"
}
