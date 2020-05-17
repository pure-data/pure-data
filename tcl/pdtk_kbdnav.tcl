package provide pdtk_kbdnav 0.1

namespace eval ::pdtk_kbdnav:: {
    #namespace export pdtk_canvas_kbdnavmode
    namespace export pdtk_canvas_is_kbdnav_active
}

# store the patches that are "keyboard navigating" so we can
# process key bindings selectively
set ::pdtk_kbdnav::active {}


proc ::pdtk_kbdnav::pdtk_canvas_kbdnavmode {mytoplevel flag} {
    if {$flag eq 0} {
        set idx [lsearch $::pdtk_kbdnav::active $mytoplevel]
        if {$idx ne -1} {
            set ::pdtk_kbdnav::active [lreplace $::pdtk_kbdnav::active $idx $idx]
            ::pdtk_kbdnav::kbdnav_unbind $mytoplevel
            ::pdwindow::post "kbdnav OFF $mytoplevel\n"
        }
    } else {
        lappend ::pdtk_kbdnav::active $mytoplevel
            ::pdtk_kbdnav::kbdnav_bind $mytoplevel
            ::pdwindow::post "kbdnav ON $mytoplevel\n"
    }
}

proc ::pdtk_kbdnav::pdtk_canvas_is_kbdnav_active {mytoplevel} {
    return [lsearch $::pdtk_kbdnav::active $mytoplevel]
}

# this proc creates the bindings for the key commands that
# will be used to activate keyboard navigation
proc ::pdtk_kbdnav::bind_activators {} {
    bind all <$::modifier-Up>    {pdsend "%W kbdnav_arrow up 0"}
    bind all <$::modifier-Down>  {pdsend "%W kbdnav_arrow down 0"}
    bind all <$::modifier-Left>  {pdsend "%W kbdnav_arrow left 0"}
    bind all <$::modifier-Right> {pdsend "%W kbdnav_arrow right 0"}
}

# we use kbdnav_bind instead of bind to avoid conflicts with
# tcl's bind inside this file
proc ::pdtk_kbdnav::kbdnav_bind { mytoplevel } {

    bind mytoplevel <$::modifier-Shift-Up> {pdsend "%W kbdnav_arrow up 1"}
    bind mytoplevel <$::modifier-Shift-Down> {pdsend "%W kbdnav_arrow down 1"}
    # [...]
}

proc ::pdtk_kbdnav::kbdnav_unbind { mytoplevel } {
    bind mytoplevel <$::modifier-Up> {}
    bind mytoplevel <$::modifier-Down> {}

    bind mytoplevel <$::modifier-Shift-Up> {}
    bind mytoplevel <$::modifier-Shift-Down> {}
    # [...]
}