package provide pd_canvaszoom 0.1

namespace eval ::pd_canvaszoom:: {
    # exported procedures
    namespace export zoominit
    namespace export canvasxy
    namespace export getzdepth
    namespace export setzdepth

    # exported variables
    variable zsteps
    variable zdepth
    variable font_measure
    variable default_zoom
}

namespace eval ::pd_canvaszoom::canvas:: {
    # a namespace for the renamed canvas-procs
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
}

after idle ::pd_canvaszoom::init_default_zoom

proc ::pd_canvaszoom::default_zoom_callback {widget value} {
    set value [expr 20 * int($value / 20.)]
    ::pd_canvaszoom::set_default_zoom $value
    set zdepth [expr int([::pd_canvaszoom::steps2depth $value] * 100)]
    ${widget}.l configure -text [_ "Default zoom level: %d%%" ${zdepth}]
}

proc ::pd_canvaszoom::default_zoom_pref_widget {widget} {
    frame $widget
    label ${widget}.l

    if [catch {::ttk::scale ${widget}.z} ] {
        scale ${widget}.z -showvalue false
    }
    ${widget}.z configure \
        -from -100 -to 200 -orient horizontal \
        -length 300 \
        -variable ::pd_canvaszoom::default_zoom \
        -command [list ::pd_menucommands::scheduleAction ::pd_canvaszoom::default_zoom_callback ${widget}]

    pack ${widget}.l ${widget}.z -anchor w
    ::pd_canvaszoom::default_zoom_callback ${widget} ${::pd_canvaszoom::default_zoom}
}

# multiplies by "zdepth" all consecutive numbers from the "from"th element.
# process maximum "max_elements" elements, and round the result if "int" is not null.
proc ::pd_canvaszoom::scale_consecutive_numbers {from zdepth int max_elements args} {
    set i $from
    set result {}
    set maxi [expr min([llength $args], [expr $from + $max_elements])]
    while {$i < $maxi && [string is double -strict [lindex $args $i]]} {
        if {$int} {
            lset args $i [expr int([lindex $args $i] * $zdepth)]
        } else {
            lset args $i [expr [lindex $args $i] * $zdepth]
        }
        incr i
    }
    return $args
}

# substituted commands for hijacking canvas
proc ::pd_canvaszoom::canvas_command {c method args} {
    set zdepth [getzdepth $c]
    # puts "canvas_command: $c $method $args"

    # itemconfig[ure] always needs to be filtered (even when zdepth==1.0),
    # otherwise outdated _f or _t tags could be left untouched
    if {[string first "itemconfig" $method] == 0} {
        # scale width
        set widthindex [lsearch -start 1 $args "-width"]
        if {$widthindex != -1} {
            incr widthindex
            set args [scale_consecutive_numbers $widthindex $zdepth 0 1 {*}$args]
        }
        # scale font
        set fontindex [lsearch -start 1 $args "-font"]
        if {$fontindex != -1} {
            incr fontindex
            set item [lindex $args 0]
            set font [lindex $args $fontindex]
            set newfont [scalefont $font [lindex $font 1] $zdepth]
            lset args $fontindex $newfont
            # remove font tag
            foreach {tag} [::pd_canvaszoom::canvas::$c gettags $item] {
                if {"_f" in [string range $tag 0 1]} {
                    ::pd_canvaszoom::canvas::$c dtag $item $tag
                }
            }
            # add the new font tag
            ::pd_canvaszoom::canvas::$c addtag _f[lindex $font 1] withtag $item"
        }
        # if changing the text content, remove text tag
        if {[lsearch -start 1 $args "-text"] != -1} {
            set item [lindex $args 0]
            foreach {tag} [::pd_canvaszoom::canvas::$c gettags $item] {
                if {"_t" in [string range $tag 0 1]} {
                    ::pd_canvaszoom::canvas::$c dtag $item $tag
                }
            }
        }
    }

    if { $zdepth == 1.0 } { return [::pd_canvaszoom::canvas::$c $method {*}$args] }
    switch $method {
        "create" {
            # scale coordinates
            set args [scale_consecutive_numbers 1 $zdepth 0 1e6 {*}$args]

            set widthindex [lsearch -start 2 $args "-width"]
            # for non-text, default linewidth to 1.0
            if {$widthindex == -1 && [lindex $args 0] != "text"} {
                set tagsindex [lsearch -start 2 $args "-tags"]
                incr tagsindex
                set tags [lindex $args $tagsindex]
                # don't scale rect selection outline width (tagged "x")
                if {{x} ni $tags} {
                    set args [linsert $args $tagsindex+1 -width 1.0]
                    set widthindex [lsearch -start 2 $args "-width"]
                }
            }
            # now scale 'width' value, if any
            if {$widthindex != -1} {
                incr widthindex
                set args [scale_consecutive_numbers $widthindex $zdepth 0 1e6 {*}$args]
            }

            # scale font if any
            if {[set fontindex [lsearch -start 2 $args "-font"]] != -1} {
                incr fontindex
                set font [lindex $args $fontindex]
                set realfontsize [lindex $font 1]
                set font [scalefont $font $realfontsize $zdepth]
                lset args $fontindex $font
                # add font tag
                set tagsindex [lsearch -start 2 $args "-tags"]
                incr tagsindex
                set tags [lindex $args $tagsindex]
                lset args $tagsindex [list {*}$tags _f$realfontsize]
            }
        }
        "move" {
            set args [scale_consecutive_numbers 1 $zdepth 0 2 {*}$args]
        }
        "coords" {
            set args [scale_consecutive_numbers 1 $zdepth 0 1e6 {*}$args]
        }
    }
    return [::pd_canvaszoom::canvas::$c $method {*}$args]
}

proc ::pd_canvaszoom::cleanup {canvas} {
    foreach c [list ::${canvas} ::pd_canvaszoom::canvas::${canvas}] {
        catch {
            rename ${c} {}
            unset ::pd_canvaszoom::zsteps($c)
            unset ::pd_canvaszoom::zdepth($c)
        }
    }
}

proc ::pd_canvaszoom::zoominit {mytoplevel} {
    # read or define default_zoom if not already done
    ::pd_canvaszoom::init_default_zoom

    set c [tkcanvas_name $mytoplevel]

    # hijack canvas
    rename $c ::pd_canvaszoom::canvas::$c
    bind $c <Destroy> {+::pd_canvaszoom::cleanup %W}
    proc ::$c {method args} {
        # retreive canvas name from 'info'
        set c [lindex [info level 0] 0]
        return [::pd_canvaszoom::canvas_command $c $method {*}$args]
    }

    # init zoom state to the default zoom level
    set ::pd_canvaszoom::zsteps($c) $::pd_canvaszoom::default_zoom
    set ::pd_canvaszoom::zdepth($c) [pd_canvaszoom::steps2depth $::pd_canvaszoom::zsteps($c)]

    # canvas bindings for mousewheel and mousewheel-button are OS dependent
    # LATER: probably move this to pd_bindings.tcl
    switch -- $::windowingsystem {
        "x11" {
            bind all <${::modifier}-Button-4> \
                {event generate [focus -displayof %W] <${::modifier}-MouseWheel> -delta  120}
            bind all <${::modifier}-Button-5> \
                {event generate [focus -displayof %W] <${::modifier}-MouseWheel> -delta -120}
            bind ${c} <${::modifier}-MouseWheel> {::pd_canvaszoom::delayed_stepzoom %W %D}
            bind ${c} <ButtonPress-2> {%W scan mark %x %y}
            bind ${c} <B2-Motion> {%W scan dragto %x %y 1}
        }
        "aqua" {
            # on MacOS, mousewheel is upside down and scaled differently
            bind ${c} <${::modifier}-MouseWheel> {::pd_canvaszoom::delayed_stepzoom %W [expr %D*-120]}
            # on MacOS mousewheel-button is button-3
            bind ${c} <ButtonPress-3> {%W scan mark %x %y}
            bind ${c} <B3-Motion> {%W scan dragto %x %y 1}
        }
        "win32" {
            bind ${c} <${::modifier}-MouseWheel> {::pd_canvaszoom::delayed_stepzoom %W %D}
            bind ${c} <ButtonPress-2> {%W scan mark %x %y}
            bind ${c} <B2-Motion> {%W scan dragto %x %y 1}
        }
    }
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

proc ::pd_canvaszoom::delete_toastzoom {w} {
    if [winfo exists $w] {
        wm withdraw $w
    }
}

proc ::pd_canvaszoom::toastzoom {c} {
    variable zdepth
    if { ! [info exists zdepth($c)] } {return}
    set ::pd_canvaszoom::zoomtext($c) "[expr int($zdepth($c) * 100)]%"

    set zwindow "${c}.canvaszoom"
    if {![winfo exists ${zwindow} ]} {
        toplevel ${zwindow}
        wm overrideredirect ${zwindow} 1
        label ${zwindow}.label \
            -highlightthick 0 -relief solid -borderwidth 1 \
            -font TkFixedFont \
            -textvariable ::pd_canvaszoom::zoomtext($c)
        pack ${zwindow}.label -expand 1 -fill x
    }
    wm deiconify ${zwindow}

    set geometry [format +%d+%d [expr [winfo rootx ${c}] + 3] [expr [winfo rooty ${c}] + 3]]
    wm geometry ${zwindow} ${geometry}
    after idle "[list wm geometry ${zwindow} ${geometry}]; raise ${zwindow}"

    raise ${zwindow}

    after cancel ::pd_canvaszoom::delete_toastzoom ${zwindow}
    after 1200   ::pd_canvaszoom::delete_toastzoom ${zwindow}
}

set ::pd_canvaszoom::stepzoom_task {}
set ::pd_canvaszoom::accum_steps 0
proc ::pd_canvaszoom::delayed_stepzoom {c steps} {
    variable stepzoom_task
    variable accum_steps
    after cancel $stepzoom_task
    set accum_steps [expr $accum_steps + $steps]
    set stepzoom_task [after idle ::pd_canvaszoom::stepzoom $c $accum_steps]
}

# zoom in (steps>0) or zoom out (steps<0)
proc ::pd_canvaszoom::stepzoom {c steps} {
    set ::pd_canvaszoom::accum_steps 0
    variable zsteps
    # don't zoom if not initialized
    if { ! [info exists zsteps($c)] } { return  }
    set newsteps [expr $zsteps($c) + $steps / 6.]
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
    ::pdtk_canvas::pdtk_canvas_getscroll $c 1
    # adjust scrolling to keep the (xcanvas, ycanvas) point at the same (xwin, ywin) position on the screen
    scroll_point_to $c $xcanvas $ycanvas $xwin $ywin

    ::pd_canvaszoom::toastzoom $c
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
                ::pd_canvaszoom::canvas::$c itemconfigure $i -font $font -text $text
            } {
                # suppress text if too small
                ::pd_canvaszoom::canvas::$c itemconfigure $i -text {}
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
                set newwidth [expr {$linewidth * $zdepth}]
                if {$newwidth < 1} {set newwidth 1}
                ::pd_canvaszoom::canvas::$c itemconfigure $i -width $newwidth
            }
        }
    }
}

proc ::pd_canvaszoom::canvasxy {c x y} {
    set zdepth $::pd_canvaszoom::zdepth($c)
    return [list [expr int([$c canvasx $x] / $zdepth)] [expr int([$c canvasy $y] / $zdepth)]]
}

proc ::pd_canvaszoom::getzdepth c {
    if [info exists ::pd_canvaszoom::zdepth($c)] {
        return $::pd_canvaszoom::zdepth($c)
    } {
        return 0
    }
}
