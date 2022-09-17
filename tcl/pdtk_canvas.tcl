
package provide pdtk_canvas 0.1

package require pd_bindings

namespace eval ::pdtk_canvas:: {

    # the untitled name prefix pd checks for using a macro in g_canvas.h,
    # a saveas panel is shown when saving a file with this name
    variable untitled_name "PDUNTITLED"
    variable untitled_len 10

    namespace export pdtk_canvas_popup
    namespace export pdtk_canvas_editmode
    namespace export pdtk_canvas_getscroll
    namespace export pdtk_canvas_setparents
    namespace export pdtk_canvas_reflecttitle
    namespace export pdtk_canvas_menuclose
}

# store the filename associated with this window,
# so we can use it during menuclose
array set ::pdtk_canvas::::window_fullname {}

array set ::pdtk_canvas::geometry_needs_init {}

# One thing that is tricky to understand is the difference between a Tk
# 'canvas' and a 'canvas' in terms of Pd's implementation.  They are similar,
# but not the same thing.  In Pd code, a 'canvas' is basically a patch, while
# the Tk 'canvas' is the backdrop for drawing everything that is in a patch.
# The Tk 'canvas' is contained in a 'toplevel' window. That window has a Tk
# class of 'PatchWindow'.

# TODO figure out weird frameless window when you open a graph


#TODO: http://wiki.tcl.tk/11502
# MS Windows
#wm geometry . returns contentswidthxcontentsheight+decorationTop+decorationLeftEdge.
#and
#winfo rooty . returns contentsTop
#winfo rootx . returns contentsLeftEdge

if {[tk windowingsystem] eq "win32" || \
    $::tcl_version < 8.5 || \
        ($::tcl_version == 8.5 && \
             [tk windowingsystem] eq "aqua" && \
             [lindex [split [info patchlevel] "."] 2] < 13) } {
    # fit the geometry onto screen for Tk 8.4 or win32,
    # also check for Tk Cocoa backend on macOS which is only stable in 8.5.13+;
    # newer versions of Tk can handle multiple monitors so allow negative pos
    proc pdtk_canvas_wrap_window {x y w h} {
        set width [lindex [wm maxsize .] 0]
        set height [lindex [wm maxsize .] 1]

        if {$w > $width} {
            set w $width
            set x 0
        }
        if {$h > $height} {
            # 30 for window framing
            set h [expr $height - $::menubarsize]
            set y $::menubarsize
        }

        set x [ expr $x % $width]
        set y [ expr $y % $height]
        if {$x < 0} {set x 0}
        if {$y < 0} {set y 0}

        return [list ${x} ${y} ${w} ${h}]
    }
} {
    proc pdtk_canvas_wrap_window {x y w h} {
        return [list ${x} ${y} ${w} ${h}]
    }
}

# this proc is split out on its own to make it easy to override. This makes it
# easy for people to customize these calculations based on their Window
# Manager, desires, etc.
proc pdtk_canvas_place_window {width height geometry} {
    # read back the current geometry +posx+posy into variables
    set w $width
    set h $height
    set xypos ""
    if { "" != ${geometry} } {
        scan $geometry {%[+]%d%[+]%d} - x - y
        foreach {x y w h} [pdtk_canvas_wrap_window $x $y $width $height] {break}
        set xypos +${x}+${y}
    }
    return [list ${w} ${h} ${w}x${h}${xypos}]
}


#------------------------------------------------------------------------------#
# canvas new/saveas

proc pdtk_canvas_new {mytoplevel width height geometry editable} {
    if { "" eq $geometry } {
        # no position set: this is a new window (rather than one loaded from file)
        # we set a flag here, so we can query (and report) the actual geometry,
        # once the window is fully created
        set ::pdtk_canvas::geometry_needs_init($mytoplevel) 1
    }

    foreach {width height geometry} [pdtk_canvas_place_window $width $height $geometry] {break;}
    set ::undo_actions($mytoplevel) no
    set ::redo_actions($mytoplevel) no

    # release the window grab here so that the new window will
    # properly get the Map and FocusIn events when its created
    ::pdwindow::busyrelease
    # set the loaded array for this new window so things can track state
    set ::loaded($mytoplevel) 0
    toplevel $mytoplevel -width $width -height $height -class PatchWindow
    wm group $mytoplevel .
    $mytoplevel configure -menu $::patch_menubar

    # we have to wait until $mytoplevel exists before we can generate
    # a <<Loading>> event for it, that's why this is here and not in the
    # started_loading_file proc.  Perhaps this doesn't make sense tho
    event generate $mytoplevel <<Loading>>

    if { "" != ${geometry} } {
        wm geometry $mytoplevel $geometry
    }
    wm minsize $mytoplevel $::canvas_minwidth $::canvas_minheight

    set tkcanvas [tkcanvas_name $mytoplevel]
    canvas $tkcanvas -width $width -height $height \
        -highlightthickness 0 -scrollregion [list 0 0 $width $height] \
        -xscrollcommand "$mytoplevel.xscroll set" \
        -yscrollcommand "$mytoplevel.yscroll set" \
        -background white
    scrollbar $mytoplevel.xscroll -orient horizontal -command "$tkcanvas xview"
    scrollbar $mytoplevel.yscroll -orient vertical -command "$tkcanvas yview"
    pack $tkcanvas -side left -expand 1 -fill both

    # for some crazy reason, win32 mousewheel scrolling is in units of
    # 120, and this forces Tk to interpret 120 to mean 1 scroll unit
    if {$::windowingsystem eq "win32"} {
        $tkcanvas configure -xscrollincrement 1 -yscrollincrement 1
    }

    ::pd_bindings::patch_bindings $mytoplevel

    # give focus to the canvas so it gets the events rather than the window
    focus $tkcanvas

    # let the scrollbar logic determine if it should make things scrollable
    set ::xscrollable($tkcanvas) 0
    set ::yscrollable($tkcanvas) 0

    # init patch properties arrays
    set ::editingtext($mytoplevel) 0
    set ::childwindows($mytoplevel) {}

    # this should be at the end so that the window and canvas are all ready
    # before this variable changes.
    set ::editmode($mytoplevel) $editable
}

# if the patch canvas window already exists, then make it come to the front
proc pdtk_canvas_raise {mytoplevel} {
    wm deiconify $mytoplevel
    raise $mytoplevel
    set mycanvas $mytoplevel.c
    focus $mycanvas
}

proc pdtk_canvas_saveas {mytoplevel initialfile initialdir destroyflag} {
    if { ! [file isdirectory $initialdir]} {set initialdir $::filenewdir}
    set filename [tk_getSaveFile -initialdir $initialdir \
                      -initialfile [::pdtk_canvas::cleanname "$initialfile"] \
                      -defaultextension .pd -filetypes $::filetypes]
    if {$filename eq ""} return; # they clicked cancel

    set extension [file extension $filename]
    set oldfilename $filename
    set filename [regsub -- "$extension$" $filename [string tolower $extension]]
    if { ! [regexp -- "\.(pd|pat|mxt)$" $filename]} {
        # we need the file extension even on Mac OS X
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
    pdsend "$mytoplevel savetofile [enquote_path $basename] [enquote_path \
         $dirname] $destroyflag"
    set ::filenewdir $dirname
    # add to recentfiles
    ::pd_guiprefs::update_recentfiles $filename
}

##### ask user Save? Discard? Cancel?, and if so, send a message on to Pd ######
proc ::pdtk_canvas::pdtk_canvas_menuclose {mytoplevel reply_to_pd} {
    raise $mytoplevel
    set filename [lindex [array get ::pdtk_canvas::::window_fullname $mytoplevel] 1]
    set message [format {Do you want to save the changes you made in "%s"?} $filename]
    set answer [tk_messageBox -message $message -type yesnocancel -default "yes" \
                    -parent $mytoplevel -icon question]
    switch -- $answer {
        yes {pdsend "$mytoplevel menusave 1"}
        no {pdsend $reply_to_pd}
        cancel {}
    }
}

#------------------------------------------------------------------------------#
# mouse usage

# TODO put these procs into the pdtk_canvas namespace
proc pdtk_canvas_motion {tkcanvas x y mods} {
    set mytoplevel [winfo toplevel $tkcanvas]
    pdsend "$mytoplevel motion [$tkcanvas canvasx $x] [$tkcanvas canvasy $y] $mods"
}

proc pdtk_canvas_mouse {tkcanvas x y b f} {
    set mytoplevel [winfo toplevel $tkcanvas]
    pdsend "$mytoplevel mouse [$tkcanvas canvasx $x] [$tkcanvas canvasy $y] $b $f"
}

proc pdtk_canvas_mouseup {tkcanvas x y b {f 0}} {
    set mytoplevel [winfo toplevel $tkcanvas]
    pdsend "$mytoplevel mouseup [$tkcanvas canvasx $x] [$tkcanvas canvasy $y] $b $f"
}

proc pdtk_canvas_rightclick {tkcanvas x y b} {
    set mytoplevel [winfo toplevel $tkcanvas]
    pdsend "$mytoplevel mouse [$tkcanvas canvasx $x] [$tkcanvas canvasy $y] $b 8"
}

# on X11, button 2 pastes from X11 clipboard, so simulate normal paste actions
proc pdtk_canvas_clickpaste {tkcanvas x y b} {
    pdtk_canvas_mouse $tkcanvas $x $y $b 0
    pdtk_canvas_mouseup $tkcanvas $x $y $b 0
    if { [catch {set pdtk_pastebuffer [selection get]}] } {
        # no selection... do nothing
    } else {
        for {set i 0} {$i < [string length $pdtk_pastebuffer]} {incr i 1} {
            set cha [string index $pdtk_pastebuffer $i]
            scan $cha %c keynum
            pdsend "[winfo toplevel $tkcanvas] key 1 $keynum 0"
        }
    }
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
            -command {::pdtk_canvas::done_popup $::focused_window 0}
        .popup add command -label [_ "Open"]       \
            -command {::pdtk_canvas::done_popup $::focused_window 1}
        .popup add command -label [_ "Help"]       \
            -command {::pdtk_canvas::done_popup $::focused_window 2}
    }
}

proc ::pdtk_canvas::done_popup {mytoplevel action} {
    pdsend "$mytoplevel done-popup $action $::popup_xcanvas $::popup_ycanvas"
}

proc ::pdtk_canvas::pdtk_canvas_popup {mytoplevel xcanvas ycanvas hasproperties hasopen} {
    set ::popup_xcanvas $xcanvas
    set ::popup_ycanvas $ycanvas
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
    set tkcanvas [tkcanvas_name $mytoplevel]
    set scrollregion [$tkcanvas cget -scrollregion]
    # get the canvas location that is currently the top left corner in the window
    set left_xview_pix [expr [lindex [$tkcanvas xview] 0] * [lindex $scrollregion 2]]
    set top_yview_pix [expr [lindex [$tkcanvas yview] 0] * [lindex $scrollregion 3]]
    # take the mouse clicks in canvas coords, add the root of the canvas
    # window, and subtract the area that is obscured by scrolling
    set xpopup [expr int($xcanvas + [winfo rootx $tkcanvas] - $left_xview_pix)]
    set ypopup [expr int($ycanvas + [winfo rooty $tkcanvas] - $top_yview_pix)]
    tk_popup .popup $xpopup $ypopup 0
}

if {[tk windowingsystem] eq "aqua" } {
    # I don't know how to move the mouse on OSX, so skip it
    proc ::pdtk_canvas::setmouse {tkcanvas x y} { }
} else {
    proc ::pdtk_canvas::setmouse {tkcanvas x y} {
        # set the mouse to the given position
        # (same coordinate system as reported by pdtk_canvas_motion)
        event generate $tkcanvas <Motion> -warp 1 -x $x -y $y
    }
}

#------------------------------------------------------------------------------#
# procs for when file loading starts/finishes

proc ::pdtk_canvas::started_loading_file {patchname} {
    ::pdwindow::busygrab
}

# things to run when a patch is finished loading.  This is called when
# the OS sends the "Map" event for this window.
proc ::pdtk_canvas::finished_loading_file {mytoplevel} {
    # ::pdwindow::busyrelease is in pdtk_canvas_new so that the grab
    # is released before the new toplevel window gets created.
    # Otherwise the grab blocks the new window from getting the
    # FocusIn event on creation.

    # set editmode to make sure the menu item is in the right state
    pdtk_canvas_editmode $mytoplevel $::editmode($mytoplevel)
    set ::loaded($mytoplevel) 1
    # send the virtual events now that everything is loaded
    event generate $mytoplevel <<Loaded>>

    # if the window was created without a position (that is: a new window),
    # we have the opportunity to query the actual position now
    if { "" ne [array names ::pdtk_canvas::geometry_needs_init $mytoplevel ] } {
        array unset ::pdtk_canvas::geometry_needs_init $mytoplevel
        scan [wm geometry $mytoplevel] {%dx%d%[+]%d%[+]%d} width height - x - y
        # on X11, 'wm geometry' won't report a useful position until the window was moved
        # but 'winfo geometry' does (though slightly off, but we ignore this offset
        # for newly created, never moved windows)
        # other windowingsystems will already report a useful position, and luckily
        # they report the same for 'wm geometry' and 'winfo geometry'
        if { "+$x+$y" eq "+0+0" } {
            scan [winfo geometry $mytoplevel] {%dx%d%[+]%d%[+]%d} width height - x - y
            pdsend "$mytoplevel setbounds $x $y [expr $x + $width] [expr $y + $height]"
        }
    }
}

#------------------------------------------------------------------------------#
# procs for canvas events

# check or uncheck the "edit" menu item
proc ::pdtk_canvas::pdtk_canvas_editmode {mytoplevel state} {
    set ::editmode_button $state
    set ::editmode($mytoplevel) $state
    event generate $mytoplevel <<EditMode>>
}

# message from Pd to update the currently available undo/redo action
proc pdtk_undomenu {mytoplevel undoaction redoaction} {
    set ::undo_actions($mytoplevel) $undoaction
    set ::redo_actions($mytoplevel) $redoaction
    if {$mytoplevel ne "nobody"} {
        ::pd_menus::update_undo_on_menu $mytoplevel $undoaction $redoaction
    }
}

# This proc configures the scrollbars whenever anything relevant has
# been updated.  It should always receive a tkcanvas, which is then
# used to generate the mytoplevel, needed to address the scrollbars.
proc ::pdtk_canvas::pdtk_canvas_getscroll {tkcanvas} {
    if {! [winfo exists $tkcanvas]} {
        return
    }
    set mytoplevel [winfo toplevel $tkcanvas]
    set height [winfo height $tkcanvas]
    set width [winfo width $tkcanvas]

    set bbox [$tkcanvas bbox all]
    if {$bbox eq "" || [llength $bbox] != 4} {return}
    set xupperleft [lindex $bbox 0]
    set yupperleft [lindex $bbox 1]
    if {$xupperleft > 0} {set xupperleft 0}
    if {$yupperleft > 0} {set yupperleft 0}
    set xlowerright [lindex $bbox 2]
    set ylowerright [lindex $bbox 3]
    if {$xlowerright < $width} {set xlowerright $width}
    if {$ylowerright < $height} {set ylowerright $height}
    set scrollregion [concat $xupperleft $yupperleft $xlowerright $ylowerright]
    $tkcanvas configure -scrollregion $scrollregion
    # X scrollbar
    if {[lindex [$tkcanvas xview] 0] == 0.0 && [lindex [$tkcanvas xview] 1] == 1.0} {
        set ::xscrollable($tkcanvas) 0
        pack forget $mytoplevel.xscroll
    } else {
        set ::xscrollable($tkcanvas) 1
        pack $mytoplevel.xscroll -side bottom -fill x -before $tkcanvas
    }
    # Y scrollbar, it gets touchy at the limit, so say > 0.995
    if {[lindex [$tkcanvas yview] 0] == 0.0 && [lindex [$tkcanvas yview] 1] > 0.995} {
        set ::yscrollable($tkcanvas) 0
        pack forget $mytoplevel.yscroll
    } else {
        set ::yscrollable($tkcanvas) 1
        pack $mytoplevel.yscroll -side right -fill y -before $tkcanvas
    }
}

proc ::pdtk_canvas::scroll {tkcanvas axis amount} {
    if {$axis eq "x" && $::xscrollable($tkcanvas) == 1} {
        $tkcanvas xview scroll [expr {- ($amount)}] units
    }
    if {$axis eq "y" && $::yscrollable($tkcanvas) == 1} {
        $tkcanvas yview scroll [expr {- ($amount)}] units
    }
}

#------------------------------------------------------------------------------#
# get patch window child/parent relationships

# add a child window ID to the list of children, if it isn't already there
proc ::pdtk_canvas::addchild {mytoplevel child} {
    # if either ::childwindows($mytoplevel) does not exist, or $child does not
    # exist inside of the ::childwindows($mytoplevel list
    if { [lsearch -exact [array names ::childwindows $mytoplevel]] == -1 \
             || [lsearch -exact $::childwindows($mytoplevel) $child] == -1} {
        set ::childwindows($mytoplevel) [lappend ::childwindows($mytoplevel) $child]
    }
}

# receive a list of all my parent windows from 'pd'
proc ::pdtk_canvas::pdtk_canvas_setparents {mytoplevel args} {
    # check if the user passed a list (instead of multiple arguments)
    if { [llength $args] == 1 } {set args [lindex $args 0]}
    set parents {}
    foreach parent $args {
        if { [catch {set parent [winfo toplevel $parent]}] } {
            if { [file extension $parent] eq ".c" } {set parent [file rootname $parent]}
        }
        lappend parents $parent
        addchild $parent $mytoplevel
    }
    set ::parentwindows($mytoplevel) $parents
}

# receive information for setting the info in the title bar of the window
proc ::pdtk_canvas::pdtk_canvas_reflecttitle {mytoplevel \
                                              path name arguments dirty} {
    set path [::pdtk_text::unescape $path]
    set name [::pdtk_text::unescape $name]
    set arguments [::pdtk_text::unescape $arguments]
    set name [::pdtk_canvas::cleanname "$name"]
    set ::windowname($mytoplevel) $name
    set ::pdtk_canvas::::window_fullname($mytoplevel) "$path/$name"
    if {$::windowingsystem eq "aqua"} {
        wm attributes $mytoplevel -modified $dirty
        if {[file exists "$path/$name"]} {
            # for some reason -titlepath can still fail so just catch it
            if [catch {wm attributes $mytoplevel -titlepath "$path/$name"}] {
                wm title $mytoplevel "$path/$name"
            }
        }
        wm title $mytoplevel "$name$arguments"
    } else {
        if {$dirty} {set dirtychar "*"} else {set dirtychar " "}
        wm title $mytoplevel "$name$dirtychar$arguments - $path"
    }
}

#------------------------------------------------------------------------------#
# utils

# provide a clean filename to avoid saving files with the untitled name prefix
proc ::pdtk_canvas::cleanname {name} {
    variable untitled_name
    variable untitled_len
    if {[string compare -length $untitled_len "$name" "$untitled_name"] == 0} {
        # replace untitled prefix with a display name
        # TODO localize "Untitled" & make sure translations do not contain spaces
        return [string replace "$name" 0 [expr $untitled_len - 1] "Untitled"]
    }
    return $name
}

set enable_cords_to_foreground false

proc ::pdtk_canvas::cords_to_foreground {mytoplevel {state 1}} {
    global enable_cords_to_foreground
    if {$enable_cords_to_foreground eq "true"} {
        set col black
        if { $state == 0 } {
            set col lightgrey
        }
        foreach id [$mytoplevel find withtag {cord && !selected}] {
            # don't apply backgrouding on selected (blue) lines
            if { [lindex [$mytoplevel itemconfigure $id -fill] 4 ] ne "blue" } {
                $mytoplevel itemconfigure $id -fill $col
            }
        }
    }
}

proc _check_argc {count argc type mode} {
    set should_throw false
    if {"exact" eq $mode} {
        if {$argc != $count} {
            set should_throw true
        }
    } else {
        if {$argc < $count} {
            set should_throw true
        }
    }
    if {$should_throw} {
        throw "pd<->gui comm" "[info level -2] >> only $argc arguments for $type"
    }
}

proc check_argc_exact {count argc {type {}} } {
    _check_argc $count $argc $type exact
}

proc check_argc_least {count argc {type {}} } {
    _check_argc $count $argc $$type least
}

# expand array to variables named after the array members in the caller's scope
proc array_to_vars {arrPtr} {
    upvar 1 $arrPtr arr
    foreach name [array names arr] {
        upvar 1 $name $name
        set $name $arr($name)
    }
}

proc parse_cnv_coords {args argc outPtr} {
    check_argc_least 5 $argc
    upvar 1 $outPtr out
    set out(cnv) [lindex $args 0]
    set out(x1) [lindex $args 1]
    set out(y1) [lindex $args 2]
    set out(x2) [lindex $args 3]
    set out(y2) [lindex $args 4]
}

proc parse_obj_atom_args {args argc type outPtr} {
    check_argc_exact 8 $argc
    upvar 1 $outPtr out
    if { "atom" eq $type } {
        set out(corner) [lindex $args 5]
        set out(pattern) ""
        set out(width) "-width [lindex $args 6]"
        set out(tag) [lindex $args 7]
    } elseif { "obj" eq $type } {
        set out(pattern) "-dash \"[lindex $args 5]\""
        set out(width) "-width [lindex $args 6]"
        set out(tag) [lindex $args 7]
    }
}

proc get_poly_coords {pPtr} {
    upvar 1 $pPtr p
    array_to_vars p
    if [info exists corner] {
        return "$x1 $y1 [expr $x2 - $corner] $y1 $x2 [expr $y1 + $corner] $x2 $y2 $x1 $y2 $x1 $y1"
    } else {
        return "$x1 $y1 $x2 $y1 $x2 $y2 $x1 $y2 $x1 $y1"
    }
}

proc bang_get_tags { obj outPtr } {
    upvar 1 $outPtr out
    #The C backend sends $obj with trailing 0x, however the legacy version
    #would create tags with sprintf(...%lx..)(i.e.: no leading 0x). Here we
    #remove the trailing 0x.
    #TODO: better options would be to find a a better type from C instead
    set obj [string replace $obj 0 1 "" ];
    append out(object) $obj OBJ
    append out(base) $obj BASE
    append out(button) $obj BUT
    append out(label) $obj LABEL
}

set iolets {outlet inlet iemgui_outlet iemgui_inlet}
append cnv_coords_types "obj atom $::iolets"

proc ::pdtk_canvas::create {args} {
    #puts "pdtk_canvas::create got: $args"
    set docmds ""
    check_argc_least 2 $args
    set type [lindex $args 0]
    set args [lrange $args 1 end]
    set argc [llength $args]
    if { $type in $::cnv_coords_types} {
        parse_cnv_coords $args $argc p
        array_to_vars p
    }
    if {"obj" eq $type || "atom" eq $type} {
        parse_obj_atom_args $args $argc $type p
        set docmds "$p(cnv) create line [get_poly_coords p] $p(pattern) $p(width) -capstyle projecting -tags {{$p(tag)} {$type}}"
    }
    if { $type in $::iolets } {
        if {[string match "iemgui_*" $type]} {
            set is_iemgui True
        } else {
            set is_iemgui False
        }
        if {$is_iemgui} {
            check_argc_exact 7 $argc $type
            set tag_label [lindex $args 6]
        } else {
            check_argc_exact 6 $argc $type
        }
        set tags [lindex $args 5]
        set fill "-fill black"
        # this is for backwards compatibility in order to ensure the exact
        # same command as before get executed.
        # iemgui_draw_iolets() and glist_drawiofor() have a different ordering
        # of the -tags and -fill attributes.
        if {$is_iemgui} {
            # this is iemgui_draw_iolets, so -fill goes before -tags
            set fill0 $fill
            set fill1 ""
        } else {
            # this is glist_drawiofor, so -tags goes before -fill
            set fill0 ""
            set fill1 $fill
        }
        append docmds "$cnv create rectangle $x1 $y1 $x2 $y2 $fill0 -tags {$tags} $fill1;\n"
        if {$is_iemgui} {
            append docmds "$cnv lower [lindex $tags 1] $tag_label;\n"
        }
    }
    if {"bang" eq $type} {
        check_argc_exact 2 $argc
        set cnv [lindex $args 0]
        set obj [lindex $args 1]
        #The C backend sends $obj with trailing 0x, however the legacy version
        #would create tags with sprintf(...%lx..)(i.e.: no leading 0x). Here we
        #remove the trailing 0x.
        #TODO: better options would be to find a a better type from C instead
        set obj [string replace $obj 0 1 "" ];
        append tag_object $obj OBJ
        append tag_base $obj BASE
        append tag_button $obj BUT
        append tag_label $obj LABEL
        #bang_draw_new()
        set cmd "$cnv create rectangle 0 0 0 0 -tags {$tag_object $tag_base}"
        append docmds "$cmd;\n"
        set cmd "$cnv create oval 0 0 0 0 -tags {$tag_object $tag_button}"
        append docmds "$cmd;\n"
        set cmd "$cnv create text 0 0 -anchor w -tags {$tag_object $tag_label label text}"
        append docmds "$cmd;\n"
    }
    if { [string length $docmds] > 0 } {
        ::pd_connect::pd_docmds "$docmds"
    }
}

proc ::pdtk_canvas::select {args} {
    #puts "pdtk_canvas::select got: $args"
    set docmds ""
    check_argc_least 2 [llength $args]
    set type [lindex $args 0]
    set args [lrange $args 1 end]
    set argc [llength $args]
    if {$type eq "tag"} {
        check_argc_exact 3 $argc
        set glist [lindex $args 0]
        set buf [lindex $args 1]
        set state [lindex $args 2]
        set docmds "$glist itemconfigure $buf -fill $state"
    }
    if {"bang" eq $type} {
        check_argc_exact 4 $argc
        set cnv [lindex $args 0]
        set obj [lindex $args 1]
        set col [lindex $args 2]
        set lcol [lindex $args 3]
        bang_get_tags $obj tags
        append docmds "$cnv itemconfigure $tags(base) -outline $col;\n"
        append docmds "$cnv itemconfigure $tags(button) -outline $col;\n"
        append docmds "$cnv itemconfigure $tags(label) -fill $lcol;\n"
    }
    if { [string length $docmds] > 0 } {
        ::pd_connect::pd_docmds "$docmds"
    }
}

proc ::pdtk_canvas::config {args} {
    set docmds ""
    check_argc_least 2 [llength $args]
    set type [lindex $args 0]
    set args [lrange $args 1 end]
    set argc [llength $args]
    if { "bang" eq $type} {
        check_argc_exact 13 $argc $type
        parse_cnv_coords $args $argc p
        array_to_vars p
        set obj [lindex $args 5]
        set zoom [lindex $args 6]
        set bcol [lindex $args 7]
        set fcol [lindex $args 8]
        set ldx [lindex $args 9]
        set ldy [lindex $args 10]
        set fontatoms [lindex $args 11]
        set lcol [lindex $args 12]

        bang_get_tags $obj tags
        set inset $zoom
        #bng_draw_config()
        append docmds "$cnv coords $tags(base) $x1 $y1 $x2 $y2;\n"
        append docmds "$cnv itemconfigure $tags(base) -width $zoom -fill $bcol;\n"
        append docmds "$cnv coords $tags(button) [expr $x1 + $inset] \
            [expr $y1 + $inset] [expr $x2 - $inset] [expr $y2 - $inset];\n"
        append docmds "$cnv itemconfigure $tags(button) -width $zoom -fill $fcol;\n"
        append docmds "$cnv coords $tags(label) $ldx $ldy;\n"
        append docmds "$cnv itemconfigure $tags(label) -font {$fontatoms} -fill $lcol;\n"
    }
    if { [string length $docmds] > 0 } {
        ::pd_connect::pd_docmds "$docmds"
    }
}

proc ::pdtk_canvas::update {args} {
    set docmds ""
    check_argc_least 2 [llength $args]
    set type [lindex $args 0]
    set args [lrange $args 1 end]
    set argc [llength $args]
    if { "bang" eq $type} {
        check_argc_exact 3 $argc $type
        set cnv [lindex $args 0]
        set obj [lindex $args 1]
        set col [lindex $args 2]
        set obj [string replace $obj 0 1 "" ]; # same as above
        append tag_button $obj BUT
        set docmds "$cnv itemconfigure $tag_button -fill $col"
    }
    if { [string length $docmds] > 0 } {
        ::pd_connect::pd_docmds "$docmds"
    }
}

proc ::pdtk_canvas::move {args} {
    set docmds ""
    check_argc_least 2 [llength $args]
    set type [lindex $args 0]
    set args [lrange $args 1 end]
    set argc [llength $args]
    if { $type in [list atom obj]} {
        parse_cnv_coords $args $argc p
        parse_obj_atom_args $args $argc $type p
        append docmds "$p(cnv) coords $p(tag) [get_poly_coords p];\n"
        if { "obj" eq $type } {
            # this is for backwards compatibility in order to ensure the exact
            # same command as before get executed. It can be removed later, as
            # there is no effect in an additional `-width` property with the
            # same width as in create()
            set p(width) ""
        }
        append docmds "$p(cnv) itemconfigure $p(tag) $p(pattern) $p(width);\n"
    }
    if { "tag" eq $type} { ;#meta-type for those commands that call tk's "move" directly
        check_argc_exact 4 $argc $type
        set cnv [lindex $args 0]
        set tag [lindex $args 1]
        set dx [lindex $args 2]
        set dy [lindex $args 3]
        set docmds "$cnv move $tag $dx $dy"
    }
    if { [string length $docmds] > 0 } {
        ::pd_connect::pd_docmds "$docmds"
    }
}

proc ::pdtk_canvas::delete {args} {
    set docmds ""
    check_argc_exact 2 [llength $args]
    set cnv [lindex $args 0]
    set tag [lindex $args 1]
    set docmds "$cnv delete $tag"
    if { [string length $docmds] > 0 } {
        ::pd_connect::pd_docmds "$docmds"
    }
}
