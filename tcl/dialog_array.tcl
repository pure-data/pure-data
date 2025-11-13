package provide dialog_array 0.1

namespace eval ::dialog_array:: {
    namespace export pdtk_array_dialog
    namespace export pdtk_array_listview_new
    namespace export pdtk_array_listview_fillpage
    namespace export pdtk_array_listview_setpage
    namespace export pdtk_array_listview_closeWindow
}

# global variables for the listview
array set ::dialog_array::listview_entry {}
array set ::dialog_array::listview_id {}
array set ::dialog_array::listview_page {}
array set ::dialog_array::listview_numpages {}
set ::dialog_array::listview_pagesize 1000
# whether the array stores its values
array set ::dialog_array::saveme_button {}
# whether we can draw the array with the mouse
array set ::dialog_array::editable_button {}
# draw style: polygon(0), points(1), bezier(2)
array set ::dialog_array::drawas_button {}
# line width
array set ::dialog_array::linewidth_entry {}
# line color
array set ::dialog_array::linecolor_button {}
# show data
array set ::dialog_array::vis_button {}
# show name
array set ::dialog_array::visname_button {}
# this stores the state of the "in new graph"/"in last graph" radio buttons
# and the "delete array" checkbutton
array set ::dialog_array::otherflag_button {}

############ pdtk_array_dialog -- dialog window for arrays #########
proc ::dialog_array::listview_windowname {arrayName} {
    set id $::dialog_array::listview_id($arrayName)
    return "${id}_listview"
}
proc ::dialog_array::listview_lbname {arrayName} {
    set id $::dialog_array::listview_id($arrayName)
    return "${id}_listview.data.lb"
}

proc ::dialog_array::listview_setpage {arrayName page {numpages {}} {pagesize {}}} {
    set ::dialog_array::listview_page($arrayName) $page
    set ::dialog_array::listview_numpages($arrayName) $numpages
    if {$pagesize ne {} && [string is double $pagesize]} {
        set ::dialog_array::listview_pagesize $pagesize
    }
}
proc ::dialog_array::listview_setdata {arrayName startIndex args} {
    set lb [listview_lbname $arrayName]
    if { [catch {
        # treeview
        ${lb} delete [${lb} children {}]
        set idx $startIndex
        foreach x $args {
            ${lb} insert {} end -values [list $idx $x]
            incr idx
        }
    } ] } {
        # listbox
        ${lb} delete 0 end
        set idx 0
        foreach x $args {
            ${lb} insert $idx "[expr $startIndex + $idx]) $x"
            incr idx
        }
    }
}
proc ::dialog_array::listview_focus {arrayName item} {
    set lb [listview_lbname $arrayName]
    ${lb} yview $item
}

proc ::dialog_array::pdtk_array_listview_setpage {arrayName page} {
    listview_setpage $arrayName $page
}

proc ::dialog_array::listview_changepage {arrayName np} {
    pdtk_array_listview_setpage \
        $arrayName [expr $::dialog_array::listview_page($arrayName) + $np]
    pdtk_array_listview_fillpage $arrayName
}

proc ::dialog_array::pdtk_array_listview_fillpage {arrayName} {
    set lb [listview_lbname ${arrayName}]

    # get the index of the topmost visible element
    # (so the scroll does not change after updating the elements)
    if {[winfo exists $lb]} {
        if { [catch {
            # treeview

            # this is index of the 'selected' element
            # (not what we want, but a good fallback...)
            set topItem [$lb index [$lb focus]]

            # search for the first visible cell
            set xy 0
            for { set xy 0 } { $xy < 500 } { incr xy } {
                if { [$lb identify region $xy $xy ] eq "cell" } {
                    # usually the first cell we find is still hidden
                    # increment by one more pixel to get a valid one
                    incr xy

                    set item [$lb identify item $xy $xy]
                    set topItem [$lb index $item]
                    break
                }
            }
        } ] } {
            # listbox (much simpler)
            set topItem [expr [lindex [$lb yview] 0] * [$lb size]]
        }
        set cmd "$::dialog_array::listview_id($arrayName) \
               arrayviewlistfillpage \
               $::dialog_array::listview_page($arrayName) \
               $topItem"

        pdsend $cmd
    }
}

proc ::dialog_array::pdtk_array_listview_new {id arrayName page} {
    set ::dialog_array::listview_page($arrayName) $page
    set ::dialog_array::listview_id($arrayName) $id
    set windowName [listview_windowname ${arrayName}]
    destroy $windowName

    toplevel $windowName -class DialogWindow
    wm group $windowName .
    wm protocol $windowName WM_DELETE_WINDOW \
        "::dialog_array::listview_close $id \{$arrayName\}"
    wm title $windowName [concat $arrayName "(list view)"]


    frame $windowName.data
    pack $windowName.data -fill "both" -side top -expand 1
    frame $windowName.buttons
    pack $windowName.buttons -fill "x" -side bottom

    set lb $windowName.data.lb
    set sb $windowName.data.sb
    if { [ catch {
        # treeview
        ttk::treeview $lb \
            -columns {index value} -show headings \
            -height 20 \
            -selectmode extended \
            -yscrollcommand "$sb set"
        $lb heading index -text "#" -anchor center
        $lb heading value -text $arrayName -anchor center
        $lb column index -width 75 -anchor e
    } stderr ] } {
        # listview
        listbox $lb -height 20 -width 25 \
            -selectmode extended \
            -relief solid -background white -borderwidth 1 \
            -yscrollcommand "$sb set"
    }
    scrollbar $sb \
        -command "$lb yview" -orient vertical
    pack $lb -expand 1 -fill both -side left
    pack $sb -fill y -side right
    bind $lb <Double-ButtonPress-1> \
        "::dialog_array::listview_edit \{$arrayName\} $page"
    # handle copy/paste
    catch {
        # this probably only works on X11
        selection handle $lb \
            "::dialog_array::listview_lbselection \{$arrayName\}"
    }
    # a Copy/Paste popup menu
    bind $lb <ButtonPress-3> \
        "::dialog_array::listview_popup \{$arrayName\}"
    bind $lb <<Paste>> \
        "::dialog_array::listview_paste \{$arrayName\}; break"
    bind $lb <<Copy>> \
        "::dialog_array::listview_copy \{$arrayName\}; break"

    button $windowName.buttons.prev -text "\u2190" \
        -command "::dialog_array::listview_changepage \{$arrayName\} -1"
    button $windowName.buttons.next -text "\u2192" \
        -command "::dialog_array::listview_changepage \{$arrayName\} 1"

    frame $windowName.buttons.page

    entry $windowName.buttons.page.current -textvariable ::dialog_array::listview_page($arrayName) \
        -validate key -validatecommand "string is double %P" \
        -justify "right" -width 5
    bind $windowName.buttons.page.current <Return> \
        "::dialog_array::listview_changepage \{$arrayName\} 0"
    label $windowName.buttons.page.slash -text "/"
    label $windowName.buttons.page.total -text 0 -textvariable ::dialog_array::listview_numpages($arrayName)
    pack $windowName.buttons.page.current -side left
    pack $windowName.buttons.page.slash -side left
    pack $windowName.buttons.page.total -side left


    pack $windowName.buttons.prev -side left -ipadx 20 -pady 10 -anchor s
    pack $windowName.buttons.page -side left -padx 20 -pady 10 -anchor s
    pack $windowName.buttons.next -side right -ipadx 20 -pady 10 -anchor s
    focus $windowName
}

proc ::dialog_array::listview_lbselection {arrayName off size} {
    set lb [listview_lbname ${arrayName}]
    set items {}
    if { [catch {
        foreach idx [$lb selection] {
            lappend items [lindex [$lb item $idx -values] 1]
        }
    } ] } {
        foreach idx [$lb curselection] {
            set v [$lb get $idx]
            lappend items [string range $v [string first ") " $v]+2 end]
        }
    }

    return [join $items "\n"]
}

# parses 'data' into numbers, and sends them to the Pd-core so it
# can set the values in 'arrayName' starting from 'startIndex'
proc ::dialog_array::listview_edit+paste {arrayName startIndex data} {
    set values {}
    set offset [expr $startIndex \
                    + $::dialog_array::listview_pagesize \
                    * $::dialog_array::listview_page($arrayName)]
    foreach value [split $data ", \n"] {
        if {$value eq {}} {continue}
        if {! [string is double $value]} {continue}
        lappend values $value
    }
    if { $values ne {} } {
        pdsend "$::dialog_array::listview_id($arrayName) $offset $values"
        pdtk_array_listview_fillpage $arrayName
    }
}

# a popup menu for copy/paste
proc ::dialog_array::listview_popup {arrayName} {
    set windowName [listview_windowname ${arrayName}]
    set lb [listview_lbname ${arrayName}]
    set popup ${lb}.popup
    destroy $popup

    # check if there's no selection, disable the popup
    set cur {}
    if { [catch {
        set cur [$lb selection]
    } ] } {
        set cur [$lb curselection]
    }
    if { $cur eq {} } {
        return
    }

    menu $popup -tearoff false
    $popup add command -label [_ "Copy"] \
        -command "::dialog_array::listview_copy \{$arrayName\}; \
                  destroy $popup"
    $popup add command -label [_ "Paste"] \
        -command "::dialog_array::listview_paste \{$arrayName\}; \
                  destroy $popup"
    tk_popup $popup [winfo pointerx $windowName] \
        [winfo pointery $windowName] 0
}

# copy current selection to clipboard (called from the copy/paste popup)
proc ::dialog_array::listview_copy {arrayName} {
    set sel [listview_lbselection $arrayName {} {}]
    clipboard clear
    clipboard append $sel
}

# when data is pasted (called from the copy/paste popup), update the values
proc ::dialog_array::listview_paste {arrayName} {
    set sel {}
    set itemNum {}
    # get data from CLIPBOARD
    if { $sel eq {} } {catch { set sel [selection get -selection CLIPBOARD] }}
    # if that failed, get it from the PRIMARY copy buffer
    if { $sel eq {} } {catch { set sel [selection get -selection PRIMARY] }}

    if { $sel eq {} } {
        # giving up
        return
    }

    # get the selection start, so we know where to paste to
    set lb [::dialog_array::listview_lbname $arrayName]
    if { [catch {
        set itemId [lindex [$lb selection] 0]
        if { $itemId ne {} } {
            set itemNum [$lb index ${itemId} ]
        }
    } ] } {
        set itemNum [lindex [$lb curselection] 0]
    }

    if { $itemNum ne {} } {
        ::dialog_array::listview_edit+paste $arrayName $itemNum $sel
    }

}

proc ::dialog_array::listview_edit {arrayName page {font {}}} {
    set lb [listview_lbname ${arrayName}]
    set entry ${lb}.entry
    if {[winfo exists $entry]} {
        ::dialog_array::listview_update_entry \
            $arrayName $::dialog_array::listview_entry($arrayName)
        unset ::dialog_array::listview_entry($arrayName)
    }
    destroy $entry
    if { [catch {
        set focus [$lb focus]
        foreach {x y w h} [$lb bbox $focus 1] {break}
        entry $entry
        place configure ${lb}.entry -x ${x} -y ${y} -width ${w} -height ${h}
        set itemNum [$lb index $focus]
    } stderr ] } {
        set itemNum [$lb index active]

        set bbox [$lb bbox $itemNum]
        set y [expr [lindex $bbox 1] - 4]
        entry $entry
        place configure $entry -relx 0 -y $y -relwidth 1
    }
    set ::dialog_array::listview_entry($arrayName) $itemNum

    $entry insert 0 []
    lower $entry
    focus $entry
    bind $entry <Return> \
        "::dialog_array::listview_update_entry \{$arrayName\} $itemNum; break"
    bind $entry <Escape> \
        "destroy $entry; break"
}

proc ::dialog_array::listview_update_entry {arrayName itemNum} {
    set entry [listview_lbname $arrayName].entry
    ::dialog_array::listview_edit+paste $arrayName $itemNum [$entry get]
    destroy $entry
}

proc ::dialog_array::pdtk_array_listview_closeWindow {arrayName} {
    destroy [listview_windowname ${arrayName}]
}

proc ::dialog_array::listview_close {mytoplevel arrayName} {
    pdtk_array_listview_closeWindow $arrayName
    pdsend "$mytoplevel arrayviewclose"
}

proc ::dialog_array::apply_old {mytoplevel} {
    set otherflag $::dialog_array::otherflag_button($mytoplevel)
    if { $otherflag eq "keep" } {set otherflag 0}
    if { $otherflag eq "delete" } {set otherflag 1}
    pdsend "$mytoplevel arraydialog \
            [::dialog_gatom::escape [$mytoplevel.array.name.entry get]] \
            [$mytoplevel.array.size.entry get] \
            [expr $::dialog_array::saveme_button($mytoplevel) + (2 * $::dialog_array::drawas_button($mytoplevel))] \
            ${otherflag}"
}
proc ::dialog_array::apply_new {mytoplevel} {
    set data "$mytoplevel arraydialog"
    lappend data [::dialog_gatom::escape [$mytoplevel.array.name.entry get]] \
        [$mytoplevel.array.size.entry get]
    lappend data "-keep" $::dialog_array::saveme_button($mytoplevel)
    lappend data "-edit" $::dialog_array::editable_button($mytoplevel)
    lappend data "-style" $::dialog_array::drawas_button($mytoplevel)
    lappend data "-width" $::dialog_array::linewidth_entry($mytoplevel)
    lappend data "-color" $::dialog_array::linecolor_button($mytoplevel)
    lappend data "-vis" $::dialog_array::vis_button($mytoplevel)
    lappend data "-visname" $::dialog_array::visname_button($mytoplevel)

    set otherflag $::dialog_array::otherflag_button($mytoplevel)
    if { $otherflag eq "keep" } {lappend data -delete 0} \
        elseif { $otherflag eq "delete" } {lappend data -delete 1} \
        else {lappend data -new [expr ! $otherflag]}

    pdsend $data
}

proc ::dialog_array::apply {mytoplevel} {
    if { 1 } {
        ::dialog_array::apply_new ${mytoplevel}
    } else {
        ::dialog_array::apply_old ${mytoplevel}
    }
}


proc ::dialog_array::openlistview {mytoplevel} {
    pdsend "$mytoplevel arrayviewlistnew"
}

proc ::dialog_array::cancel {mytoplevel} {
    pdsend "$mytoplevel cancel"
}

proc ::dialog_array::ok {mytoplevel} {
    ::dialog_array::apply $mytoplevel
    ::dialog_array::cancel $mytoplevel
}

proc ::dialog_array::array_dialog {mytoplevel name size args} {
    # ::dialog_array::array_dialog {.gfxstub55eb1bb15ba0} {array97} 100 -keep 0 -edit 1 -style 2 -color 835
    puts [list ::dialog_array::array_dialog ${mytoplevel} $name $size $args]

    set keep 0
    set edit 1
    set style 0
    set width 1
    set color 0
    set vis 1
    set visname 1
    set newone 0

    foreach {flag value} ${args} {
        switch -glob ${flag} {
            -new {set newone ${value}}
            -keep {set keep ${value}}
            -edit {set edit ${value}}
            -style {set style ${value}}
            -width {set width ${value}}
            -color {set color ${value}}
            -vis {set vis ${value}}
            -visname {set visname ${value}}
            * {
                ::pdwindow::error "::dialog_array::array_dialog: invalid flag ${flag}\n"
                return
            }
        }
    }

    if {[winfo exists $mytoplevel]} {
        wm deiconify $mytoplevel
        raise $mytoplevel
        focus $mytoplevel
    } else {
        create_dialog $mytoplevel $newone
    }

    $mytoplevel.array.name.entry insert 0 [::dialog_gatom::unescape ${name}]
    $mytoplevel.array.size.entry insert 0 ${size}

    set ::dialog_array::saveme_button($mytoplevel) ${keep}
    set ::dialog_array::editable_button($mytoplevel) ${edit}
    set ::dialog_array::drawas_button($mytoplevel) ${style}
    set ::dialog_array::linewidth_entry($mytoplevel) ${width}
    set ::dialog_array::linecolor_button($mytoplevel) ${color}
    set ::dialog_array::vis_button($mytoplevel) ${vis}
    set ::dialog_array::visname_button($mytoplevel) ${visname}

    ::dialog_array::set_linecolor ${mytoplevel} ${color}
}


proc ::dialog_array::pdtk_array_dialog {mytoplevel name size flags newone} {
    set keep [expr $flags & 1]
    set style [expr ( $flags & 6 ) >> 1]
    ::dialog_array::array_dialog ${mytoplevel} ${name} ${size} \
        -keep ${keep} -style ${style} -new ${newone}
    # pd -> tcl
#  2 * (int)(template_getfloat(template_findbyname(sc->sc_template), gensym("style"), x->x_scalar->sc_vec, 1)));

# tcl->pd
#    int style = ((flags & 6) >> 1);
}

proc ::dialog_array::color2pd {tclcolor} {
    scan $tclcolor "#%2x%2x%2x" r g b
    return [expr $r*8/255 * 100 + $g*8/255 * 10 + $b*8/255 * 1]
}
proc ::dialog_array::pd2color {pdcolor} {
    set pdcolor [expr max(0, min(999, int($pdcolor)))]
    set R [expr $pdcolor / 100]
    set G [expr ($pdcolor / 10)%10]
    set B [expr $pdcolor % 10]
    set r [expr min(255, max(0, min(8, $R)) << 5)]
    set g [expr min(255, max(0, min(8, $G)) << 5)]
    set b [expr min(255, max(0, min(8, $B)) << 5)]
    return [format "#%02x%02x%02x" $r $g $b]
}
proc ::dialog_array::set_linecolor {mytoplevel pdcolor} {
    set ::dialog_array::linecolor_button($mytoplevel) $pdcolor
    set tclcolor [pd2color $pdcolor]

    ${mytoplevel}.drawas.right.color configure -background $tclcolor -activebackground $tclcolor
}
proc ::dialog_array::choose_linecolor {mytoplevel} {
    set color "#000000"
    if {[info exists ::dialog_array::linecolor_button($mytoplevel)]} {
        set color [pd2color $::dialog_array::linecolor_button($mytoplevel)]
    }
    if { "${color}" eq "" } {set color "#000000" }
    set color [tk_chooseColor -initialcolor $color -title [_ "Graph color" ]]
    if { $color ne "" } {
        ::dialog_array::set_linecolor $mytoplevel [color2pd $color]
    }
}
proc ::dialog_array::create_dialog {mytoplevel newone} {
    toplevel $mytoplevel -class DialogWindow
    wm title $mytoplevel [_ "Array Properties"]
    wm group $mytoplevel .
    wm resizable $mytoplevel 0 0
    wm transient $mytoplevel $::focused_window
    ::pd_menus::menubar_for_dialog $mytoplevel
    $mytoplevel configure -padx 0 -pady 0
    ::pd_bindings::dialog_bindings $mytoplevel "array"

    # array
    labelframe $mytoplevel.array -borderwidth 1 -text [_ "Array"] -padx 5
    pack $mytoplevel.array -side top -fill x
    frame $mytoplevel.array.name -height 7 -padx 5
    pack $mytoplevel.array.name -side top -anchor e
    label $mytoplevel.array.name.label -text [_ "Name:"]
    entry $mytoplevel.array.name.entry -width 17
    pack $mytoplevel.array.name.entry $mytoplevel.array.name.label -side right

    frame $mytoplevel.array.size -height 7 -padx 5
    pack $mytoplevel.array.size -side top -anchor e
    label $mytoplevel.array.size.label -text [_ "Size:"]
    entry $mytoplevel.array.size.entry -width 17 \
        -validate key -validatecommand "string is digit %P"
    pack $mytoplevel.array.size.entry $mytoplevel.array.size.label -side right

    frame $mytoplevel.array.editsave
    pack $mytoplevel.array.editsave -side top
    checkbutton $mytoplevel.array.editsave.saveme -text [_ "Save contents"] \
        -variable ::dialog_array::saveme_button($mytoplevel) -anchor w
    pack $mytoplevel.array.editsave.saveme -side left
    checkbutton $mytoplevel.array.editsave.editable -text [_ "Allow mouse editing"] \
        -variable ::dialog_array::editable_button($mytoplevel) -anchor w
    pack $mytoplevel.array.editsave.editable -side left

    # draw as
    labelframe $mytoplevel.drawas -text [_ "Display:"] -padx 20 -pady 10 -borderwidth 1
    pack $mytoplevel.drawas -side top -fill x

    set f $mytoplevel.drawas.left
    labelframe ${f} -text [_ "Draw as:"] -padx 20  -borderwidth 1 -relief flat
    pack ${f} -side left -fill x
    radiobutton ${f}.points  -variable ::dialog_array::drawas_button($mytoplevel) \
        -value 1 -text [_ "Polygon"]
    radiobutton ${f}.polygon -variable ::dialog_array::drawas_button($mytoplevel) \
        -value 0 -text [_ "Points"]
    radiobutton ${f}.bezier  -variable ::dialog_array::drawas_button($mytoplevel) \
        -value 2 -text [_ "Bezier curve"]
    pack ${f}.points -side top -anchor w
    pack ${f}.polygon -side top -anchor w
    pack ${f}.bezier -side top -anchor w

    set f $mytoplevel.drawas.right
    frame ${f} -padx 20 -borderwidth 1
    pack ${f} -side left -fill x

    set color #000

    checkbutton $f.vis -text [_ "Show array"] \
        -variable ::dialog_array::vis_button($mytoplevel) -anchor w
    checkbutton $f.visname -text [_ "Show name"] \
        -variable ::dialog_array::visname_button($mytoplevel) -anchor w
    label ${f}.wlabel -text [_ "Width:"]
    entry ${f}.width -width 17 \
        -textvariable ::dialog_array::linewidth_entry($mytoplevel) \
        -validate key -validatecommand "string is digit %P"

    label ${f}.clabel -text [_ "Color:"]
    label ${f}.color \
        -background $color -activebackground $color \
        -relief ridge -padx 7 -pady 0 -width 1
    bind ${f}.color <Button> "::dialog_array::choose_linecolor $mytoplevel"
    grid ${f}.wlabel -row 0 -column 0
    grid ${f}.width  -row 0 -column 1
    grid ${f}.clabel -row 1 -column 0
    grid ${f}.color  -row 1 -column 1 -sticky we
    grid ${f}.vis    -row 2 -column 0 -columnspan 2 -sticky w
    grid ${f}.visname -row 3 -column 0 -columnspan 2 -sticky w

    # options
    if {$newone == 1} {
        labelframe $mytoplevel.options -text [_ "Put array into:"] -padx 20 -borderwidth 1
        pack $mytoplevel.options -side top -fill x
        radiobutton $mytoplevel.options.radio0 -value 0 \
            -variable ::dialog_array::otherflag_button($mytoplevel) -text [_ "New graph"]
        radiobutton $mytoplevel.options.radio1 -value 1 \
            -variable ::dialog_array::otherflag_button($mytoplevel) -text [_ "Last graph"]
        pack $mytoplevel.options.radio0 -side top -anchor w
        pack $mytoplevel.options.radio1 -side top -anchor w
        set ::dialog_array::otherflag_button($mytoplevel) 0
    } else {
        labelframe $mytoplevel.options -text [_ "Options"] -padx 20 -borderwidth 1
        pack $mytoplevel.options -side top -fill x
        button $mytoplevel.options.listview -text [_ "Open List View..."] \
            -command "::dialog_array::openlistview $mytoplevel [$mytoplevel.array.name.entry get]"
        pack $mytoplevel.options.listview -side top
        checkbutton $mytoplevel.options.deletearray -text [_ "Delete array"] \
            -onvalue "delete" -offvalue "keep" \
            -variable ::dialog_array::otherflag_button($mytoplevel) -anchor w
        set ::dialog_array::otherflag_button($mytoplevel) keep
        pack $mytoplevel.options.deletearray -side top
    }

    # buttons
    frame $mytoplevel.buttonframe
    pack $mytoplevel.buttonframe -side bottom -pady 2m
    button $mytoplevel.buttonframe.cancel -text [_ "Cancel"] \
        -command "::dialog_array::cancel $mytoplevel"
    pack $mytoplevel.buttonframe.cancel -side left -expand 1 -fill x -padx 15 -ipadx 10
    if {$newone == 0 && $::windowingsystem ne "aqua"} {
        button $mytoplevel.buttonframe.apply -text [_ "Apply"] \
            -command "::dialog_array::apply $mytoplevel"
        pack $mytoplevel.buttonframe.apply -side left -expand 1 -fill x -padx 15 -ipadx 10
    }
    button $mytoplevel.buttonframe.ok -text [_ "OK"]\
        -command "::dialog_array::ok $mytoplevel" -default active
    pack $mytoplevel.buttonframe.ok -side left -expand 1 -fill x -padx 15 -ipadx 10

    # live widget updates on OSX in lieu of Apply button
    if {$::windowingsystem eq "aqua"} {

        # only bind if there is an existing array to edit
        if {$newone == 0} {

            # call apply on button changes
            $mytoplevel.array.saveme config -command [ concat ::dialog_array::apply $mytoplevel ]
            $mytoplevel.drawas.points config -command [ concat ::dialog_array::apply $mytoplevel ]
            $mytoplevel.drawas.polygon config -command [ concat ::dialog_array::apply $mytoplevel ]
            $mytoplevel.drawas.bezier config -command [ concat ::dialog_array::apply $mytoplevel ]

            # call apply on Return in entry boxes that are in focus & rebind Return to ok button
            bind $mytoplevel.array.name.entry <KeyPress-Return> "::dialog_array::apply_and_rebind_return $mytoplevel"
            bind $mytoplevel.array.size.entry <KeyPress-Return> "::dialog_array::apply_and_rebind_return $mytoplevel"

            # unbind Return from ok button when an entry takes focus
            $mytoplevel.array.name.entry config -validate focusin -vcmd "::dialog_array::unbind_return $mytoplevel"
            $mytoplevel.array.size.entry config -validate focusin -vcmd "::dialog_array::unbind_return $mytoplevel"
        }

        # remove cancel button from focus list since it's not activated on Return
        $mytoplevel.buttonframe.cancel config -takefocus 0

        # show active focus on the ok button as it *is* activated on Return
        $mytoplevel.buttonframe.ok config -default normal
        bind $mytoplevel.buttonframe.ok <FocusIn> "$mytoplevel.buttonframe.ok config -default active"
        bind $mytoplevel.buttonframe.ok <FocusOut> "$mytoplevel.buttonframe.ok config -default normal"

        # since we show the active focus, disable the highlight outline
        $mytoplevel.buttonframe.ok config -highlightthickness 0
        $mytoplevel.buttonframe.cancel config -highlightthickness 0
    }

    position_over_window ${mytoplevel} ${::focused_window}
}

# for live widget updates on OSX
proc ::dialog_array::apply_and_rebind_return {mytoplevel} {
    ::dialog_array::apply $mytoplevel
    bind $mytoplevel <KeyPress-Return> "::dialog_array::ok $mytoplevel"
    focus $mytoplevel.buttonframe.ok
    return 0
}

# for live widget updates on OSX
proc ::dialog_array::unbind_return {mytoplevel} {
    bind $mytoplevel <KeyPress-Return> break
    return 1
}
