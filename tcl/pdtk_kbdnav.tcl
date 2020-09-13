package provide pdtk_kbdnav 0.1

namespace eval ::pdtk_kbdnav:: {
    namespace export pdtk_canvas_is_kbdnav_active
}

# store the patches that are "keyboard navigating" so we can
# process key bindings selectively
set ::pdtk_kbdnav::active {}

# enable/disable keyboard navigation globally
proc ::pdtk_kbdnav::set_enabled {state} {
    set ::kbdnav_enabled $state
    if {$state} {
        ::pdtk_kbdnav::bind_activators
        ::pdtk_kbdnav::bind_context
    } else {
        ::pdtk_kbdnav::unbind_activators
        ::pdtk_kbdnav::unbind_context
        foreach canvas $::pdtk_kbdnav::active {
            # Note that in TCL you can remove while iterating
            # (the core will call pdtk_canvas_kbdnavmode)
            pdsend "$canvas kbdnav_deactivate"
        }
    }
}

# activate/deactivate keyboard navigation for a given patch window
proc ::pdtk_kbdnav::pdtk_canvas_kbdnavmode {mytoplevel flag} {
    if {$flag eq 0} {
        set idx [lsearch $::pdtk_kbdnav::active $mytoplevel]
        if {$idx ne -1} {
            set ::pdtk_kbdnav::active [lreplace $::pdtk_kbdnav::active $idx $idx]
            ::pdtk_kbdnav::kbdnav_remove_tag $mytoplevel
        }
    } else {
        lappend ::pdtk_kbdnav::active $mytoplevel
            ::pdtk_kbdnav::kbdnav_add_tag $mytoplevel
    }
}

proc ::pdtk_kbdnav::pdtk_canvas_is_kbdnav_active {mytoplevel} {
    return [lsearch $::pdtk_kbdnav::active $mytoplevel]
}

# Creates the bindings for the key commands that
# will be used to activate keyboard navigation
#
# It also contains kbdnav related bindings that might be used
# when kbdnav is inactive.
proc ::pdtk_kbdnav::bind_activators {} {
    bind all <$::modifier-Up>    {pdsend "[winfo toplevel %W] kbdnav_arrow up 0"}
    bind all <$::modifier-Down>  {pdsend "[winfo toplevel %W] kbdnav_arrow down 0"}
    bind all <$::modifier-Left>  {pdsend "[winfo toplevel %W] kbdnav_arrow left 0"}
    bind all <$::modifier-Right> {pdsend "[winfo toplevel %W] kbdnav_arrow right 0"}

    # toggle displaying object indices
    bind all <$::modifier-Key-i> {pdsend "[winfo toplevel %W] kbdnav_toggle_indices_visibility";break}

    # virtual click
    bind all <Shift-Key-Return> {pdsend "[winfo toplevel %W] kbdnav_virtual_click";break}

    # canvas reselect (toggle editing rtext)
    bind all <Control-Key-Return> {pdsend "[winfo toplevel %W] kbdnav_reselect";break}

    # goto
    bind all <$::modifier-Key-g> {::dialog_goto::pdtk_goto_open "$::focused_window"}
}

proc ::pdtk_kbdnav::unbind_activators {} {
    bind all <$::modifier-Up>    {}
    bind all <$::modifier-Down>  {}
    bind all <$::modifier-Left>  {}
    bind all <$::modifier-Right> {}

    # toggle displaying object indices
    bind all <$::modifier-Key-i> {}

    # virtual click
    bind all <Shift-Key-Return> {}

    # canvas reselect (toggle editing rtext)
    bind all <Control-Key-Return> {}

    # goto
    bind all <$::modifier-Key-g> {}
}

# Create all the bindings for the tag "kbdnav"
# Those will only work while keyboard-navigating
proc ::pdtk_kbdnav::bind_context {} {
    # deactivating kbdnav
    bind "kbdnav" <KeyPress-Escape> {pdsend "[winfo toplevel %W] kbdnav_deactivate";break}
    # arrow keys
    bind "kbdnav" <$::modifier-Shift-Up> {pdsend "[winfo toplevel %W] kbdnav_arrow up 1";break}
    bind "kbdnav" <$::modifier-Shift-Down> {pdsend "[winfo toplevel %W] kbdnav_arrow down 1";break}

    # magnetic connector
    bind "kbdnav" <c> {pdsend "[winfo toplevel %W] kbdnav_magnetic_connect 0";break}
        # Caps Lock Off
    bind "kbdnav" <Shift-C> {pdsend "[winfo toplevel %W] kbdnav_magnetic_connect 1";break}
        # Caps Lock On
    bind "kbdnav" <Shift-c> {pdsend "[winfo toplevel %W] kbdnav_magnetic_connect 1";break}

    # digits connector
    bind "kbdnav" <Key-0> {pdsend "[winfo toplevel %W] kbdnav_digit 0 1";break}
    bind "kbdnav" <Key-1> {pdsend "[winfo toplevel %W] kbdnav_digit 1 1";break}
    bind "kbdnav" <Key-2> {pdsend "[winfo toplevel %W] kbdnav_digit 2 1";break}
    bind "kbdnav" <Key-3> {pdsend "[winfo toplevel %W] kbdnav_digit 3 1";break}
    bind "kbdnav" <Key-4> {pdsend "[winfo toplevel %W] kbdnav_digit 4 1";break}
    bind "kbdnav" <Key-5> {pdsend "[winfo toplevel %W] kbdnav_digit 5 1";break}
    bind "kbdnav" <Key-6> {pdsend "[winfo toplevel %W] kbdnav_digit 6 1";break}
    bind "kbdnav" <Key-7> {pdsend "[winfo toplevel %W] kbdnav_digit 7 1";break}
    bind "kbdnav" <Key-8> {pdsend "[winfo toplevel %W] kbdnav_digit 8 1";break}
    bind "kbdnav" <Key-9> {pdsend "[winfo toplevel %W] kbdnav_digit 9 1";break}

    # Those bindings override the "kbdnav" <Key-N> bindings above
    # for the case where the modifier is held.
    #
    # This way the less-specific "all <$::modifier-Key-N>" bindings
    # defined in ::pd_bindings::global_bindings can run
    # Thus allowing the "put obj", "put msg", etc bindings to be
    # fired while kbd-navigating
    bind "kbdnav" <$::modifier-Key-1> { continue }
    bind "kbdnav" <$::modifier-Key-2> { continue }
    bind "kbdnav" <$::modifier-Key-3> { continue }
    bind "kbdnav" <$::modifier-Key-4> { continue }
    bind "kbdnav" <$::modifier-Key-5> { continue }

    bind "kbdnav" <Shift-KeyPress> {::pdtk_kbdnav::digits "[winfo toplevel %W]" "%k"}

    # delete
    bind "kbdnav" <Key-Delete> {pdsend "[winfo toplevel %W] kbdnav_delete";break}
}

proc ::pdtk_kbdnav::unbind_context {} {
    # deactivating kbdnav
    bind "kbdnav" <KeyPress-Escape> {}

    # arrow keys
    bind "kbdnav" <$::modifier-Shift-Up> {}
    bind "kbdnav" <$::modifier-Shift-Down> {}

    # magnetic connector
    bind "kbdnav" <c> {}
        # Caps Lock Off
    bind "kbdnav" <Shift-c> {}
        # Caps Lock On
    bind "kbdnav" <Shift-C> {}

    #digits connector
    bind "kbdnav" <Key-0> {}
    bind "kbdnav" <Key-1> {}
    bind "kbdnav" <Key-2> {}
    bind "kbdnav" <Key-3> {}
    bind "kbdnav" <Key-4> {}
    bind "kbdnav" <Key-5> {}
    bind "kbdnav" <Key-6> {}
    bind "kbdnav" <Key-7> {}
    bind "kbdnav" <Key-8> {}
    bind "kbdnav" <Key-9> {}

    bind "kbdnav" <Shift-KeyPress> {}

    # delete
    bind "kbdnav" <Key-Delete> {}
}

proc ::pdtk_kbdnav::kbdnav_add_tag { mytoplevel } {
    set bt [linsert [bindtags $mytoplevel] 0 "kbdnav"]
    bindtags $mytoplevel $bt
}

proc ::pdtk_kbdnav::kbdnav_remove_tag { mytoplevel } {
    set bt [bindtags $mytoplevel]
    set idx [lsearch $bt "kbdnav"]
    if {$idx ne -1} {
        set bt [lreplace $bt $idx $idx]
        bindtags $mytoplevel $bt
    } else {
        ::pdwindow::error "Cannot remove kbdnav bindtag"
    }
}

# see
# https://stackoverflow.com/questions/61806427/keyboard-independent-way-of-detecting-shiftdigits/
proc ::pdtk_kbdnav::digits { mytoplevel keycode } {
    if {$keycode >= 48 && $keycode <= 57} {
        pdsend "$mytoplevel kbdnav_digit [expr {$keycode-48}] 0"
    }
}