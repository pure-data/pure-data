package provide pd_canvaszoom 0.1

namespace eval ::pd_canvaszoom:: {
    namespace export zoominit
}

proc ::pd_canvaszoom::zoominit {mytoplevel {zfact {1.1}}} {
    set c [tkcanvas_name $mytoplevel]
    # save zoom state in a global variable with the same name as the canvas handle
    upvar #0 $c data
    set data(zdepth) 1.0
    set data(idle) {}
    # add mousewheel bindings to canvas
    bind all <Control-Button-4> \
        {event generate [focus -displayof %W] <Control-MouseWheel> -delta  1}
    bind all <Control-Button-5> \
        {event generate [focus -displayof %W] <Control-MouseWheel> -delta -1}
    bind $c <Control-MouseWheel> "if {%D > 0} {zoom $c $zfact} else {zoom $c [expr {1.0/$zfact}]}"
}

proc zoom {c fact} {
    upvar #0 $c data
    # zoom at the current mouse position
    set x [$c canvasx [expr {[winfo pointerx $c] - [winfo rootx $c]}]]
    set y [$c canvasy [expr {[winfo pointery $c] - [winfo rooty $c]}]]
    $c scale all $x $y $fact $fact
    # save new zoom depth
    set data(zdepth) [expr {$data(zdepth) * $fact}]
    # update fonts only after main zoom activity has ceased
    after cancel $data(idle)
    set data(idle) [after idle "zoomtext $c"]
    set data(idle) [after idle "setwitdth $c"]
}

proc zoomtext {c} {
    upvar #0 $c data
    # adjust fonts
    foreach {i} [$c find all] {
        if { ! [string equal [$c type $i] text]} {continue}
        set fontsize 0
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
            set text [$c itemcget $i -text]
            if {[llength $font] < 2} {
                #new font API
                set fontsize [font actual $font -size]
            } {
                #old font API
                set fontsize [lindex $font 1]
            }
            $c addtag _f$fontsize withtag $i
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
    }
    # update canvas scrollregion
    set bbox [$c bbox all]
    if {[llength $bbox]} {
        $c configure -scrollregion $bbox
    } {
        $c configure -scrollregion [list -4 -4 \
            [expr {[winfo width $c]-4}] \
            [expr {[winfo height $c]-4}]]
    }
}

proc setwitdth {c} {
    upvar #0 $c data
    # adjust line width
    set newwitdth $data(zdepth)
    if {$newwitdth < 1} { set newwitdth 1 }
    foreach {i} [$c find all] {
        if { [string equal [$c type $i] text]} {continue}
        $c itemconfigure $i -width $newwitdth
    }
}
