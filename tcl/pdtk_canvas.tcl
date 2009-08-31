
package provide pdtk_canvas 0.1

package require pd_bindings

namespace eval ::pdtk_canvas:: {
}

# TODO figure out weird frameless window when you open a graph

#------------------------------------------------------------------------------#
# canvas new/saveas

proc pdtk_canvas_new {mytoplevel width height geometry editable} {
    # TODO check size of window
    toplevel $mytoplevel -width $width -height $height -class CanvasWindow
    wm group $mytoplevel .
    $mytoplevel configure -menu .menubar

    # TODO slide off screen windows into view
    wm geometry $mytoplevel $geometry
    if {$::windowingsystem eq "aqua"} { # no menubar, it can be small
        wm minsize $mytoplevel 50 20
    } else { # leave room for the menubar
        wm minsize $mytoplevel 310 30
    }
    
    set ::editmode($mytoplevel) $editable

    set mycanvas $mytoplevel.c
    canvas $mycanvas -width $width -height $height -background white \
        -highlightthickness 0
    # TODO add scrollbars here
    pack $mycanvas -side left -expand 1 -fill both

    ::pd_bindings::canvas_bindings $mytoplevel

    # give focus to the canvas so it gets the events rather than the window
    focus $mycanvas
}

proc pdtk_canvas_saveas {name initialfile initialdir} {
    if { ! [file isdirectory $initialdir]} {set initialdir $::env(HOME)}
    set filename [tk_getSaveFile -initialfile $initialfile -initialdir $initialdir \
                      -defaultextension .pd -filetypes $::filetypes]
    if {$filename eq ""} return; # they clicked cancel

    set extension [file extension $filename]
    set oldfilename $filename
    set filename [regsub -- "$extension$" $filename [string tolower $extension]]
    if { ! [regexp -- "\.(pd|pat|mxt)$" $filename]} {
        # we need the file extention even on Mac OS X
        set filename $filename.pd
    }
    # test again after downcasing and maybe adding a ".pd" on the end
    if {$filename ne $oldfilename && [file exists $filename]} {
        set answer [tk_messageBox -type okcancel -icon question -default cancel\
                        -message [_ "\"$filename\" already exists. Do you want to replace it?"]]
        if {$answer eq "cancel"} return; # they clicked cancel
    }
    set dirname [file dirname $filename]
    set basename [file tail $filename]
    pdsend "$name savetofile [enquote_path $basename] [enquote_path $dirname]"
    set ::pd_menucommands::menu_new_dir $dirname
}

#------------------------------------------------------------------------------#
# mouse usage

proc pdtk_canvas_motion {mycanvas x y mods} {
    set mytoplevel [winfo toplevel $mycanvas]
    pdsend "$mytoplevel motion [$mycanvas canvasx $x] [$mycanvas canvasy $y] $mods"
}

proc pdtk_canvas_mouse {mycanvas x y b f} {
    set mytoplevel [winfo toplevel $mycanvas]
    pdsend "$mytoplevel mouse [$mycanvas canvasx $x] [$mycanvas canvasy $y] $b $f"
}

proc pdtk_canvas_mouseup {mycanvas x y b} {
    set mytoplevel [winfo toplevel $mycanvas]
    pdsend "$mytoplevel mouseup [$mycanvas canvasx $x] [$mycanvas canvasy $y] $b"
}

proc pdtk_canvas_rightclick {mycanvas x y b} {
    set mytoplevel [winfo toplevel $mycanvas]
    pdsend "$mytoplevel mouse [$mycanvas canvasx $x] [$mycanvas canvasy $y] $b 8"
}

# on X11, button 2 pastes from X11 clipboard, so simulate normal paste actions
proc pdtk_canvas_clickpaste {mycanvas x y b} {
    pdtk_canvas_mouse $mycanvas $x $y $b 0
    pdtk_canvas_mouseup $mycanvas $x $y $b
    pdtk_pastetext
}

#------------------------------------------------------------------------------#
# canvas popup menu

# since there is one popup that is used for all canvas windows, the menu
# -commands use {} quotes so that $::focused_window is interpreted when the
# menu item is called, not when the command is mapped to the menu item.  This
# is the same as the menubar in pd_menus.tcl but the opposite of the 'bind'
# commands in pd_bindings.tcl
proc ::pdtk_canvas::create_popup {} {
    if { ! [winfo exists .popup]} {
        # the popup menu for the canvas
        menu .popup -tearoff false
        .popup add command -label [_ "Properties"] \
            -command {popup_action $::focused_window 0}
        .popup add command -label [_ "Open"]       \
            -command {popup_action $::focused_window 1}
        .popup add command -label [_ "Help"]       \
            -command {popup_action $::focused_window 2}
    }
}

proc popup_action {mytoplevel action} {
    pdsend "$mytoplevel done-popup $action $::popup_xpix $::popup_ypix"
}

proc pdtk_canvas_popup {mytoplevel xpix ypix hasproperties hasopen} {
    set ::popup_xpix $xpix
    set ::popup_ypix $ypix
    if {$hasproperties} {
        .popup entryconfigure [_ "Properties"] -state normal
    } else {
        .popup entryconfigure [_ "Properties"] -state disabled
    }
    if {$hasopen} {
        .popup entryconfigure [_ "Open"] -state normal
    } else {
        .popup entryconfigure [_ "Open"] -state disabled
    }
    set mycanvas "$mytoplevel.c"
    tk_popup .popup [expr $xpix + [winfo rootx $mycanvas]] \
        [expr $ypix + [winfo rooty $mycanvas]] 0
}


#------------------------------------------------------------------------------#
# procs for canvas events

# check or uncheck the "edit" menu item
proc pdtk_canvas_editval {mytoplevel value} {
    set ::editmode($mytoplevel) $value
# TODO figure how to change Edit Mode/Interact Mode text and have menu
# enabling and disabling working still in pd_menus.tcl
#    if {$value == 0} {
#        $::pd_menus::menubar.edit entryconfigure [_ "Interact Mode"] -label [_ "Edit Mode"]
#    } else {
#        $::pd_menus::menubar.edit entryconfigure [_ "Edit Mode"] -label [_ "Interact Mode"]
#    }
    #$mytoplevel.menubar.edit entryconfigure [_ "Edit Mode"] -indicatoron $value
    # TODO make this work, probably with a proc in pd_menus, or maybe the menu
    # item can track the editmode variable
}

proc pdtk_undomenu {args} {
    # TODO make this work, probably with a proc in pd_menus    
    puts "pdtk_undomenu $args"
}

proc pdtk_canvas_getscroll {mycanvas} {
    # TODO make this work
    # the C code still sends a .c canvas, so get the toplevel
    set mytoplevel [winfo toplevel $mycanvas]
    # puts stderr "pdtk_canvas_getscroll $mycanvas"
}
