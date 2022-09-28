package provide pd_canvaszoom 0.1

namespace eval ::pd_canvaszoom:: {
    # exported procedures
    namespace export zoominit
    namespace export canvasxy
    namespace export scalescript
    namespace export getzdepth
    namespace export setzdepth

    # exported variables
    variable zsteps
    variable zdepth
    variable font_measure
    variable default_zoom
}

proc ::pd_canvaszoom::steps2depth {steps} {
    return [expr pow(2, $steps/100.0)]
}

proc ::pd_canvaszoom::set_default_zoom {steps} {
    set ::pd_canvaszoom::default_zoom $steps
    ::pd_guiprefs::write default_zoom $::pd_canvaszoom::default_zoom
}

proc ::pd_canvaszoom::init_default_zoom {} {
    variable default_zoom
    if {[info exists default_zoom]} return;

    set default_zoom [::pd_guiprefs::read default_zoom]
    if { $default_zoom == {}} { set default_zoom 0 }

    # update "File/Preferences/Zoom New Windows" menu entry
    # for now, "Zoom New Windows" is a checkbox;
    # LATER: allow other default_zoom values than 0(100%) or 100(200%)
    set ::zoom_open [expr $default_zoom >= 100]
}

proc ::pd_canvaszoom::zoominit {mytoplevel} {
    # read or define default_zoom if not already done
    ::pd_canvaszoom::init_default_zoom

    # init zoom state for this canvas, if it didn't exist
    set c [tkcanvas_name $mytoplevel]
    if { ! [info exists ::pd_canvaszoom::zsteps($c)]} {
        set ::pd_canvaszoom::zsteps($c) $::pd_canvaszoom::default_zoom
        set ::pd_canvaszoom::zdepth($c) [pd_canvaszoom::steps2depth $::pd_canvaszoom::zsteps($c)]
    }

    # add mousewheel bindings to canvas
    if {$::windowingsystem eq "x11"} {
        bind all <Control-Button-4> \
            {event generate [focus -displayof %W] <Control-MouseWheel> -delta  1}
        bind all <Control-Button-5> \
            {event generate [focus -displayof %W] <Control-MouseWheel> -delta -1}
    }
    bind $c <Control-MouseWheel> [list ::pd_canvaszoom::stepzoom $c %D]

    # add button-2 bindings to scroll the canvas
    bind $c <ButtonPress-2> {%W scan mark %x %y}
    bind $c <B2-Motion> {+ %W scan dragto %x %y 1}
}

# scroll so that the point (xcanvas, ycanvas) moves to the window-relative position (xwin, ywin)
proc ::pd_canvaszoom::scroll_point_to {c xcanvas ycanvas xwin ywin} {
    set zdepth $::pd_canvaszoom::zdepth($c)
    set scrollregion [$c cget -scrollregion]
    set x0 [lindex $scrollregion 0]
    set y0 [lindex $scrollregion 1]
    set W [expr [lindex $scrollregion 2] - [lindex $scrollregion 0]]
    set H [expr [lindex $scrollregion 3] - [lindex $scrollregion 1]]
    set scrollx [expr { ($xcanvas * $zdepth - $xwin - $x0) / $W}]
    set scrolly [expr { ($ycanvas * $zdepth - $ywin - $y0) / $H}]
    $c xview moveto $scrollx
    $c yview moveto $scrolly
}

# zoom in (steps>0) or zoom out (steps<0)
proc ::pd_canvaszoom::stepzoom {c steps} {
    variable zsteps
    # don't zoom if not initialized
    if { ! [info exists zsteps($c)] } { return  }
    set newsteps [expr $zsteps($c) + $steps * 20]
    set newsteps [expr min(max($newsteps, -400), 400)]
    ::pd_canvaszoom::setzoom $c $newsteps
}

proc ::pd_canvaszoom::setzoom {c steps} {
    variable zdepth
    variable zsteps

    # compute the position of the pointer, relatively to the window and to the canvas
    set xwin [expr {[winfo pointerx $c] - [winfo rootx $c]}]
    set ywin [expr {[winfo pointery $c] - [winfo rooty $c]}]
    set scrollregion [$c cget -scrollregion]
    set x0 [lindex $scrollregion 0]
    set y0 [lindex $scrollregion 1]
    set W [expr [lindex $scrollregion 2] - [lindex $scrollregion 0]]
    set H [expr [lindex $scrollregion 3] - [lindex $scrollregion 1]]
    set left_xview_pix [expr $x0 + [lindex [$c xview] 0] * $W]
    set top_yview_pix [expr $y0 + [lindex [$c yview] 0] * $H]
    set xcanvas [expr ($xwin + $left_xview_pix) / $zdepth($c)]
    set ycanvas [expr ($ywin + $top_yview_pix) / $zdepth($c)]

    set zsteps($c) $steps
    # save old zoom depth
    set oldzdepth $zdepth($c)
    # set new zoom depth
    set zdepth($c) [::pd_canvaszoom::steps2depth $steps]
    # compute scaling factor
    set fact [expr $zdepth($c) / $oldzdepth]
    # scale the canvas
    $c scale all 0 0 $fact $fact
    # update fonts and linewidth
    zoom_text_and_lines $c $oldzdepth $zdepth($c)
    # check new visibility of scrollbars
    ::pdtk_canvas::pdtk_canvas_getscroll $c
    # adjust scrolling to keep the (xcanvas, ycanvas) point at the same (xwin, ywin) position on the screen
    scroll_point_to $c $xcanvas $ycanvas $xwin $ywin
}


# compute the width of "M" for every size of the font.
# "fontname" here is [list $family $weight]
proc ::pd_canvaszoom::measure_font {fontname} {
    variable font_measure
    set family [lindex $fontname 0]
    set weight [lindex $fontname 1]
    set font_measure($fontname) 0
    for {set fsize 1} {$fsize < 120} {incr fsize} {
        set foo [list $family -$fsize $weight]
        set width [font measure $foo M]
        lappend font_measure($fontname) $width
    }
}

# scale a font so that it's not wider than the original one scaled by zdepth
proc ::pd_canvaszoom::scalefont {font fontsize zdepth} {
    variable font_measure
    set fontsize [expr int(abs($fontsize))]
    set fontname [list [lindex $font 0] [lindex $font 2]]
    if {! [info exist font_measure($fontname)]} {
        measure_font $fontname
    }
    if {$fontsize >= [llength $font_measure($fontname)]} {
        set fontsize [expr [llength $font_measure($fontname)] - 1]
    }
    set target_width [expr [lindex $font_measure($fontname) $fontsize] * $zdepth]
    set new_fontsize [expr {int($fontsize * $zdepth)}]
    while {[lindex $font_measure($fontname) $new_fontsize] > $target_width} {
        incr new_fontsize -1
    }
    return [lreplace $font 1 1 -$new_fontsize];
}

proc ::pd_canvaszoom::zoom_text_and_lines {c oldzdepth zdepth} {
    foreach {i} [$c find all] {
        if {[string equal [$c type $i] text]} { # adjust fonts of text items
            set fontsize 0
            set text {}
            # get original fontsize and text from tags
            #   if they were previously recorded
            foreach {tag} [$c gettags $i] {
                scan $tag {_f%d} fontsize
                scan $tag "_t%\[^\0\]" text
            }
            # if not, then record current fontsize and text
            #   and use them
            set font [$c itemcget $i -font]
            if {!$fontsize} {
                set fontsize [expr int([lindex $font 1] / $oldzdepth)]
                $c addtag _f$fontsize withtag $i
            }
            if {[string length $text] == 0} {
                set text [$c itemcget $i -text]
                $c addtag _t$text withtag $i
            }
            # scale font
            if {[expr {abs($fontsize * $zdepth)}] >= 4} {
                set font [scalefont $font $fontsize $zdepth];
                $c itemconfigure $i -font $font -text $text
            } {
                # suppress text if too small
                $c itemconfigure $i -text {}
            }
        } else { # adjust linewidth of non-text items
            set linewidth 0
            # get original linewidth from tags if it was previously recorded
            foreach {tag} [$c gettags $i] {
                scan $tag {_lw%d} linewidth
            }
            # if not, then record current linewidth and use it
            catch { # protect the case the item doesn't have "-width"
                if {!$linewidth} {
                    set linewidth [expr ([$c itemcget $i -width] / $oldzdepth)]
                    $c addtag _lw$linewidth withtag $i
                }
                # scale
                set newwitdth [expr {$linewidth * $zdepth}]
                if {$newwitdth < 1} {set newwitdth 1}
                $c itemconfigure $i -width $newwitdth
            }
        }
    }
}

proc ::pd_canvaszoom::canvasxy {tkcanvas x y} {
    set zdepth $::pd_canvaszoom::zdepth($tkcanvas)
    return [list [expr int([$tkcanvas canvasx $x] / $zdepth)] [expr int([$tkcanvas canvasy $y] / $zdepth)]]
}

# compute the position of the first character of each element of the string seen as a list
proc ::pd_canvaszoom::elements_position {instring {max_elements 1e6}} {
    set positions 0
    set pos 0
    set nextpos 0
    set element 0
    while {[set nextpos [string first " " $instring $nextpos]] >= 0} {
        catch {
            if {[lindex $instring $element] in [string range $instring $pos [expr $nextpos - 1]]} {
                set pos $nextpos
                lappend positions $pos
                if {[incr element] == [expr $max_elements - 1]} break
            }
        }
        incr nextpos
    }
    lappend positions end
    return $positions
}

# multiplies by "zdepth" all consecutive numbers from the "from"th element.
# process maximum "max_elements" elements, and round the result if "int" is not null.
# "lset" isn't usable here, since it modifies the other parts of the string, which breaks Tcl commands,
# that's why "elements_position" is needed.
proc ::pd_canvaszoom::scale_consecutive_numbers {instring from zdepth {int 0} {max_elements 1e6}} {
    set i $from
    set numbers {}
    set positions [elements_position $instring [expr $from + $max_elements + 1]]
    set maxi [expr min([llength $positions], [expr $from + $max_elements])]
    while {$i < $maxi && [string is double -strict [lindex $instring $i]]} {
        if {$int} {
            lappend numbers [expr int([lindex $instring $i] * $zdepth)]
        } else {
            lappend numbers [expr [lindex $instring $i] * $zdepth]
        }
        incr i
    }
    set startstring [string range $instring 0 [lindex $positions $from]]
    set endstring [string range $instring [lindex $positions $i] end]
    return [string cat $startstring $numbers $endstring]
}

# like "lset", but without changing the other parts of the string
proc ::pd_canvaszoom::string_lset {instring index newvalue} {
    set positions [elements_position $instring [expr $index + 1]]
    set startstring [string range $instring 0 [lindex $positions $index]]
    set endstring [string range $instring [lindex $positions [expr $index + 1]] end]
    return [string cat $startstring $newvalue $endstring]
}

proc ::pd_canvaszoom::unescape {text} {
    return [string range [subst -nocommands -novariables $text] 0 end]
}

proc ::pd_canvaszoom::getzdepth tkcanvas {
    if [info exists ::pd_canvaszoom::zdepth($tkcanvas)] {
        return $::pd_canvaszoom::zdepth($tkcanvas)
    } {
        return 0
    }
}

proc ::pd_canvaszoom::scale_command {cmd} {
    set cmd [regsub -lineanchor ";$" $cmd ""]
    set cmd [regsub -all "\}" $cmd "\} "]
    set cmd [regsub -all "\"\]" $cmd "\" \]"]
    set cmd [regsub -all {\\\n} $cmd " "]
    switch [lindex $cmd 0] {
        "image" {return $cmd}
        "pdtk_text_new" {
            if {! [set zdepth [getzdepth [lindex $cmd 1]]]} {return $cmd}
            set font [get_font_for_size [lindex $cmd 6]]
            set fontsize [lindex $font 1]
            # scale position
            set cmd [scale_consecutive_numbers $cmd 3 $zdepth 0 2]
            # scale font
            set displayed_fontsize [lindex [scalefont $font $fontsize $zdepth] 1]
            set cmd [string_lset $cmd 6 [expr abs($displayed_fontsize)]]
            # init tags
            set text [unescape [lindex $cmd 5]]
            set cmd [string_lset $cmd 2 [concat "\{ " [lindex $cmd 2] _f$fontsize [list _t$text] " \}"]]
            return $cmd
        }
        "pdtk_text_set" {
            if {! [set zdepth [getzdepth [lindex $cmd 1]]]} {return $cmd}
            # remove text tag
            set c [lindex $cmd 1]
            set i [lindex $cmd 2]
            set str {foreach {tag} [$c gettags $i] {if {"_t" in [string range $tag 0 1]} {$c dtag $i $tag}}}
            set str [string map [list {$c} $c {$i} $i] $str]
            append cmd $str
            return $cmd
        }
    }
    switch [lindex $cmd 1] {
        "create" {
            if {! [set zdepth [getzdepth [lindex $cmd 0]]]} {return $cmd}
            set cmd [scale_consecutive_numbers $cmd 3 $zdepth]
            set widthindex [lsearch -start 3 $cmd "-width"]
            if {$widthindex != -1} {
                incr widthindex
                set cmd [scale_consecutive_numbers $cmd $widthindex $zdepth]
            }
            if {[set fontindex [lsearch -start 3 $cmd "-font"]] != -1} {
                incr fontindex
                set c [lindex $cmd 0]
                set i [lindex $cmd 2]
                set font [lindex $cmd $fontindex]
                set font [scalefont $font [lindex $font 1] $zdepth]
                set cmd [string_lset $cmd $fontindex "\{$font\}"]
            }
            return $cmd
        }
        "coords" {
            if {! [set zdepth [getzdepth [lindex $cmd 0]]]} {return $cmd}
            return [scale_consecutive_numbers $cmd 3 $zdepth]
        }
        "move" {
            if {! [set zdepth [getzdepth [lindex $cmd 0]]]} {return $cmd}
            return [scale_consecutive_numbers $cmd 3 $zdepth]
        }
        "itemconfigure" {
            if {! [set zdepth [getzdepth [lindex $cmd 0]]]} {return $cmd}
            set widthindex [lsearch -start 3 $cmd "-width"]
            if {$widthindex != -1} {
                incr widthindex
                set cmd [scale_consecutive_numbers $cmd $widthindex $zdepth]
            }
            if {[set fontindex [lsearch -start 3 $cmd "-font"]] != -1} {
                incr fontindex
                set c [lindex $cmd 0]
                set i [lindex $cmd 2]
                set font [lindex $cmd $fontindex]
                set font [scalefont $font [lindex $font 1] $zdepth]
                lset cmd $fontindex $font
                # remove font tag
                set str {foreach {tag} [$c gettags $i] {if {"_f" in [string range $tag 0 1]} {$c dtag $i $tag}}}
                set str [string map [list {$c} $c {$i} $i] $str]
                append cmd \n $str
            }
            if {[lsearch -start 3 $cmd "-text"] != -1} {
                # remove text tag
                set c [lindex $cmd 0]
                set i [lindex $cmd 2]
                set str {foreach {tag} [$c gettags $i] {if {"_t" in [string range $tag 0 1]} {$c dtag $i $tag}}}
                set str [string map [list {$c} $c {$i} $i] $str]
                append cmd \n $str
            }
            return $cmd
        }
    }
    return $cmd
}

proc ::pd_canvaszoom::scalescript {incmds} {
    set outcmds ""
    set start_index 0
    set end_index 0
    # split "complete" lines:
    while {$end_index >= 0} {
        set end_index [string first "\n" $incmds $end_index]
        if {$end_index == -1} {break}
        set line [string range $incmds $start_index $end_index]
        incr end_index
        if {[info complete $line]} {
            append outcmds [scale_command $line] "\n"
            set start_index $end_index
        }
    }
    return $outcmds
}
