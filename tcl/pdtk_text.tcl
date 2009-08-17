
package provide pdtk_text 0.1

############ pdtk_text_new -- create a new text object #2###########
proc pdtk_text_new {mycanvas canvasitem x y text font_size color} {
    $mycanvas create text $x $y -tags $canvasitem -text $text -fill $color \
        -anchor nw -font [get_font_for_size $font_size]
    $mycanvas bind $canvasitem <Home> "$mycanvas icursor $canvasitem 0"
    $mycanvas bind $canvasitem <End> "$mycanvas icursor $canvasitem end"
    if {$::windowingsystem eq "aqua"} { # emacs bindings for Mac OS X
        $mycanvas bind $canvasitem <Control-a> "$mycanvas icursor $canvasitem 0"
        $mycanvas bind $canvasitem <Control-e> "$mycanvas icursor $canvasitem end"
    }
}

################ pdtk_text_set -- change the text ##################
proc pdtk_text_set {mycanvas canvasitem text} {
    $mycanvas itemconfig $canvasitem -text $text
}

