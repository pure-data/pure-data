package provide pd_canvaszoom 0.1

namespace eval ::pd_canvaszoom:: {
    namespace export zoominit
    namespace export canvasxy
    namespace export scalescript
}

proc ::pd_canvaszoom::zoominit {mytoplevel {zfact {1.1}}} {
    set c [tkcanvas_name $mytoplevel]
    # save zoom state in a global variable with the same name as the canvas handle
    upvar #0 $c data
    set data(zdepth) 1.0
    set data(oldzdepth) 1.0
    set data(idle) {}

    # add mousewheel bindings to canvas
    if {$::windowingsystem eq "x11"} {
        bind all <Control-Button-4> \
            {event generate [focus -displayof %W] <Control-MouseWheel> -delta  1}
        bind all <Control-Button-5> \
            {event generate [focus -displayof %W] <Control-MouseWheel> -delta -1}
    }
    bind $c <Control-MouseWheel> "if {%D > 0} {zoom $c $zfact} else {zoom $c [expr {1.0/$zfact}]}"

    # add button-2 bindings to scroll the canvas
    bind $c <ButtonPress-2> {+ %W scan mark %x %y}
    bind $c <B2-Motion> {+ %W scan dragto %x %y 1}
}

# scroll so that the point (xcanvas, ycanvas) moves to the window-relative position (xwin, ywin)
proc scroll_point_to {c xcanvas ycanvas xwin ywin} {
    upvar #0 $c data
    set zdepth $data(zdepth)
    set scrollregion [$c cget -scrollregion]
    set scrollx [expr { ($xcanvas * $zdepth - $xwin) / [lindex $scrollregion 2]}]
    set scrolly [expr { ($ycanvas * $zdepth - $ywin) / [lindex $scrollregion 3]}]
    $c xview moveto $scrollx
    $c yview moveto $scrolly
}

proc zoom {c fact} {
    upvar #0 $c data
    # don't use "origin" point of "scale" command to help mouse handling
    $c scale all 0 0 $fact $fact
    # save old zoom depth
    set data(oldzdepth) $data(zdepth)

    # compute the (xcanvas, ycanvas) point of the (xwin, ywin) position on the window
    set xwin [expr {[winfo pointerx $c] - [winfo rootx $c]}]
    set ywin [expr {[winfo pointery $c] - [winfo rooty $c]}]
    set scrollregion [$c cget -scrollregion]
    set left_xview_pix [expr [lindex [$c xview] 0] * [lindex $scrollregion 2]]
    set top_yview_pix [expr [lindex [$c yview] 0] * [lindex $scrollregion 3]]
    set xcanvas [expr ($xwin + $left_xview_pix) / $data(zdepth)]
    set ycanvas [expr ($ywin + $top_yview_pix) / $data(zdepth)]

    # compute new zoom depth
    set data(zdepth) [expr {$data(zdepth) * $fact}]

    # adjust scrolling to keep the (xcanvas, ycanvas) point at the same (xwin, ywin) position on the screen
    ::pdtk_canvas::pdtk_canvas_getscroll $c
    scroll_point_to $c $xcanvas $ycanvas $xwin $ywin

    # update fonts and linewidth and fix canvas scollbars again, only after main zoom activity has ceased
    after cancel $data(idle)
    set data(idle) [after idle "zoomtext $c ;\
        ::pdtk_canvas::pdtk_canvas_getscroll $c ;\
        scroll_point_to $c $xcanvas $ycanvas $xwin $ywin"]
}

proc zoomtext {c} {
    upvar #0 $c data
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
                if {[llength $font] < 2} {
                    #new font API
                    set fontsize [font actual $font -size]
                } {
                    #old font API
                    set fontsize [lindex $font 1]
                }
                set fontsize [expr $fontsize / $data(oldzdepth)]
                $c addtag _f$fontsize withtag $i
            }
            if {[string length $text] == 0} {
                set text [$c itemcget $i -text]
                $c addtag _t$text withtag $i
            }
            # scale font
            set newsize [expr {int($fontsize * $data(zdepth))}]
            if {abs($newsize) >= 4} {
                if {[llength $font] < 2} {
                    #new font api
                    font configure $font -size $newsize
                } {
                    #old font api
                    set font [lreplace $font 1 1 $newsize] ; # Save modified font! [ljl]
                }
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
                    set linewidth [expr ([$c itemcget $i -width] / $data(oldzdepth))]
                    $c addtag _lw$linewidth withtag $i
                }
                # scale
                set newwitdth [expr {$linewidth * $data(zdepth)}]
                if {$newwitdth < 1} {set newwitdth 1}
                $c itemconfigure $i -width $newwitdth
            }
        }
    }
}

proc ::pd_canvaszoom::canvasxy {tkcanvas x y} {
    set mytoplevel [winfo toplevel $tkcanvas]
    upvar #0 $tkcanvas data
    set zdepth $data(zdepth)
    return [list [expr int([$tkcanvas canvasx $x] / $zdepth)] [expr int([$tkcanvas canvasy $y] / $zdepth)]]
}

# compute the position of the first character of each element of the string seen as a list
proc elements_position {instring {max_elements 1e6}} {
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
proc scale_consecutive_numbers {instring from zdepth {int 0} {max_elements 1e6}} {
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
proc string_lset {instring index newvalue} {
    set positions [elements_position $instring [expr $index + 1]]
    set startstring [string range $instring 0 [lindex $positions $index]]
    set endstring [string range $instring [lindex $positions [expr $index + 1]] end]
    return [string cat $startstring $newvalue $endstring]
}

proc unescape {text} {
    return [string range [subst -nocommands -novariables $text] 0 end-1]
}

proc getactualfontsize {fontsize} {
    set font [get_font_for_size $fontsize]
    if {[llength $font] < 2} {
        #new font API
        return [font actual $font -size]
    } {
        #old font API
        return [lindex $font 1]
    }
}

proc getzdepth tkcanvas {
    upvar #0 $tkcanvas data
    if [info exists data] {
        return $data(zdepth)
    } {
        return 0
    }
}

proc scale_command {cmd} {
    set cmd [regsub -all "\}" $cmd "\} "]
    set cmd [regsub -all "\"\]" $cmd "\" \]"]
    set cmd [regsub -all {\\\n} $cmd " "]
    switch [lindex $cmd 0] {
        "image" {return $cmd}
        "pdtk_text_new" {
            if {! [set zdepth [getzdepth [lindex $cmd 1]]]} {return $cmd}
            set actualfontsize [getactualfontsize [lindex $cmd 6]]
            set cmd [scale_consecutive_numbers $cmd 3 $zdepth 0 2]
            set cmd [scale_consecutive_numbers $cmd 6 $zdepth true]
            set text [unescape [lindex $cmd 5]]
            lset cmd 2 [concat [lindex $cmd 2] _f$actualfontsize [list _t$text]]
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
                if {[llength $font] < 2} {
                    #new font API
                    set font [get_font_for_size [expr int([getactualfontsize [font actual $font -size]] * $zdepth])]
                } {
                    #old font API
                    lset font 1 [expr int([lindex $font 1] * $zdepth)]
                }
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
                if {[llength $font] < 2} {
                    #new font API
                    set font [get_font_for_size [expr int([getactualfontsize [font actual $font -size]] * $zdepth])]
                } {
                    #old font API
                    lset font 1 [expr int([lindex $font 1] * $zdepth)]
                }
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
