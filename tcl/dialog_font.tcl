
package provide dialog_font 0.1

namespace eval ::dialog_font:: {
    variable fontsize 10
    variable stretchval 100
    variable whichstretch 1
    variable canvaswindow
    variable sizes {8 10 12 16 24 36}
    
    namespace export pdtk_canvas_dofont
}

# TODO this should use the pd_font_$size fonts created in pd-gui.tcl
# TODO change pdtk_canvas_dofont to pdtk_font_dialog here and g_editor.c

# TODO this should really be changed on the C side so that it doesn't have to
# work around gfxstub/x_gui.c.  The gfxstub stuff assumes that there are
# multiple panels, for properties panels like this, its much easier to use if
# there is a single properties panel that adjusts based on which PatchWindow
# has focus

proc ::dialog_font::apply {mytoplevel myfontsize} {
    if {$mytoplevel eq ".pdwindow"} {
        if {[lsearch [font names] TkTextFont] >= 0} {
            font configure TkTextFont -size -$myfontsize
        }
        if {[lsearch [font names] TkDefaultFont] >= 0} {
            font configure TkDefaultFont -size -$myfontsize
        }
        if {[lsearch [font names] TkMenuFont] >= 0} {
            font configure TkMenuFont -size -$myfontsize
        }
# repeat a "pack" command so the font dialog can resize itself
        pack .font.buttonframe -side bottom -fill x -pady 2m

    } else {
        variable stretchval
        variable whichstretch
        pdsend "$mytoplevel font $myfontsize $stretchval $whichstretch"
    }
}

proc ::dialog_font::cancel {gfxstub} {
    if {$gfxstub ne ".pdwindow"} {
        pdsend "$gfxstub cancel"
    }
    destroy .font
}

proc ::dialog_font::ok {gfxstub} {
    variable fontsize
    apply $gfxstub $fontsize
    cancel $gfxstub
}

proc ::dialog_font::update_font_dialog {mytoplevel} {
    variable canvaswindow $mytoplevel
    if {[winfo exists .font]} {
        wm title .font [format [_ "%s Font"] [lookup_windowname $mytoplevel]]
    }
}

proc ::dialog_font::arrow_fontchange {change} {
    variable sizes
    variable fontsize
    variable canvaswindow
    set position [expr [lsearch $sizes $fontsize] + $change]
    if {$position < 0} {set position 0}
    set max [llength $sizes]
    if {$position >= $max} {set position [expr $max-1]}
    set fontsize [lindex $sizes $position]
    ::dialog_font::apply $canvaswindow $fontsize
}

# this should be called pdtk_font_dialog like the rest of the panels, but it
# is called from the C side, so we'll leave it be
proc ::dialog_font::pdtk_canvas_dofont {gfxstub initsize} {
    variable fontsize $initsize
    variable whichstretch 1
    variable stretchval 100
    if {$fontsize < 8} {set fontsize 12}
    if {[winfo exists .font]} {
        wm deiconify .font
        raise .font
        # the gfxstub stuff expects multiple font windows, we only have one,
        # so kill the new gfxstub requests as the come in.  We'll save the
        # original gfxstub for when the font panel gets closed
        pdsend "$gfxstub cancel"
    } else {
        create_dialog $gfxstub
    }
}

proc ::dialog_font::create_dialog {gfxstub} {
    toplevel .font -class DialogWindow
    .font configure -menu $::dialog_menubar
    .font configure -padx 10 -pady 5
    wm group .font .
#    wm resizable .font 0 0
    wm transient .font $::focused_window
    ::pd_bindings::dialog_bindings .font "font"
    # replace standard bindings to work around the gfxstub stuff and use
    # break to prevent the close window command from going to other bindings.
    # .font won't exist anymore, so it'll cause errors down the line...
    bind .font <KeyPress-Return> "::dialog_font::ok $gfxstub; break"
    bind .font <KeyPress-Escape> "::dialog_font::cancel $gfxstub; break"
    bind .font <$::modifier-Key-w> "::dialog_font::cancel $gfxstub; break"
    wm protocol .font WM_DELETE_WINDOW "dialog_font::cancel $gfxstub"
    bind .font <Up> "::dialog_font::arrow_fontchange -1"
    bind .font <Down> "::dialog_font::arrow_fontchange 1"
    
    frame .font.buttonframe
    pack .font.buttonframe -side bottom -fill x -pady 2m
    button .font.buttonframe.ok -text [_ "OK"] \
        -command "::dialog_font::ok $gfxstub" -default active
    pack .font.buttonframe.ok -side left -expand 1
    
    labelframe .font.fontsize -text [_ "Font Size"] -padx 5 -pady 4 -borderwidth 1 \
        -width [::msgcat::mcmax "Font Size"] -labelanchor n
    pack .font.fontsize -side left -padx 5
    
    # this is whacky Tcl at its finest, but I couldn't resist...
    foreach size $::dialog_font::sizes {
        radiobutton .font.fontsize.radio$size -value $size -text $size \
            -variable ::dialog_font::fontsize \
            -command [format {::dialog_font::apply $::dialog_font::canvaswindow %s} $size]
        pack .font.fontsize.radio$size -side top -anchor w
    }

    labelframe .font.stretch -text [_ "Stretch"] -padx 5 -pady 5 -borderwidth 1 \
        -width [::msgcat::mcmax "Stretch"] -labelanchor n
    pack .font.stretch -side left -padx 5 -fill y

    entry .font.stretch.entry -textvariable ::dialog_font::stretchval -width 5
    pack .font.stretch.entry -side top -pady 5

    radiobutton .font.stretch.radio1 -text [_ "X and Y"] \
        -value 1 -variable ::dialog_font::whichstretch
    radiobutton .font.stretch.radio2 -text [_ "X only"] \
        -value 2 -variable ::dialog_font::whichstretch
    radiobutton .font.stretch.radio3 -text [_ "Y only"] \
        -value 3 -variable ::dialog_font::whichstretch

    pack .font.stretch.radio1 -side top -anchor w
    pack .font.stretch.radio2 -side top -anchor w
    pack .font.stretch.radio3 -side top -anchor w
}
