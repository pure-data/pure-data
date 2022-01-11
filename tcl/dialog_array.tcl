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
set ::dialog_array::listview_pagesize 1000
# this stores the state of the "save me" check button
array set ::dialog_array::saveme_button {}
# this stores the state of the "draw as" radio buttons
array set ::dialog_array::drawas_button {}
# this stores the state of the "in new graph"/"in last graph" radio buttons
# and the "delete array" checkbutton
array set ::dialog_array::otherflag_button {}

############ pdtk_array_dialog -- dialog window for arrays #########
proc ::dialog_array::listview_windowname {arrayName} {
    set id $::dialog_array::listview_id($arrayName)
    return "${id}_listview"
}
proc ::dialog_array::listview_setpage {arrayName page} {
    set ::dialog_array::listview_page($arrayName) $page
}
proc ::dialog_array::listview_setdata {arrayName startIndex args} {
    set lb [::dialog_array::listview_windowname $arrayName].lb
    ${lb} delete 0 end
    set idx 0
    foreach x $args {
        ${lb} insert $idx "[expr $startIndex + $idx]) $x"
        incr idx
    }
}
proc ::dialog_array::listview_focus {arrayName item} {
    set lb [::dialog_array::listview_windowname $arrayName].lb
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
    set lb [listview_windowname ${arrayName}].lb
    if {[winfo exists $lb]} {
        set topItem [expr [lindex [$lb yview] 0] * \
                         [$lb size]]

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
    # FIXME
    set font 12
    set lb $windowName.lb
    set sb ${lb}.sb
    listbox $lb -height 20 -width 25 \
        -selectmode extended \
        -relief solid -background white -borderwidth 1 \
        -font [format {{%s} %d %s} $::font_family $font $::font_weight]\
        -yscrollcommand "$sb set"
    scrollbar $sb \
        -command "$lb yview" -orient vertical
    place configure $sb -relheight 1 -relx 0.9 -relwidth 0.1
    pack $lb -expand 1 -fill both
    bind $lb <Double-ButtonPress-1> \
        "::dialog_array::listview_edit \{$arrayName\} $page $font"
    # handle copy/paste
    switch -- $::windowingsystem {
        "x11" {selection handle $lb \
                   "::dialog_array::listview_lbselection \{$arrayName\}"}
        "win32" {bind $lb <ButtonPress-3> \
                     "::dialog_array::listview_popup \{$arrayName\}"}
    }
    button $windowName.prevBtn -text "<-" \
        -command "::dialog_array::listview_changepage \{$arrayName\} -1"
    button $windowName.nextBtn -text "->" \
        -command "::dialog_array::listview_changepage \{$arrayName\} 1"
    pack $windowName.prevBtn -side left -ipadx 20 -pady 10 -anchor s
    pack $windowName.nextBtn -side right -ipadx 20 -pady 10 -anchor s
    focus $windowName
}

proc ::dialog_array::listview_lbselection {arrayName off size} {
    set lb [listview_windowname ${arrayName}].lb
    set itemNums [$lb curselection]
    set cbString ""
    for {set i 0} {$i < [expr [llength $itemNums] - 1]} {incr i} {
        set listItem [$lb get [lindex $itemNums $i]]
        append cbString [string range $listItem \
                             [expr [string first ") " $listItem] + 2] \
                             end]
        append cbString "\n"
    }
    set listItem [$lb get [lindex $itemNums $i]]
    append cbString [string range $listItem \
                         [expr [string first ") " $listItem] + 2] \
                         end]
    set last $cbString
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

# Win32 uses a popup menu for copy/paste
proc ::dialog_array::listview_popup {arrayName} {
    set windowName [listview_windowname ${arrayName}]
    set popup ${windowName}.popup
    destroy $popup
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

proc ::dialog_array::listview_copy {arrayName} {
    set lb [listview_windowname ${arrayName}].lb
    set itemNums [$lb curselection]
    set cbString ""
    for {set i 0} {$i < [expr [llength $itemNums] - 1]} {incr i} {
        set listItem [$lb get [lindex $itemNums $i]]
        append cbString [string range $listItem \
                             [expr [string first ") " $listItem] + 2] \
                             end]
        append cbString "\n"
    }
    set listItem [$lb get [lindex $itemNums $i]]
    append cbString [string range $listItem \
                         [expr [string first ") " $listItem] + 2] \
                         end]
    clipboard clear
    clipboard append $cbString
}

proc ::dialog_array::listview_paste {arrayName} {
    set sel [selection get -selection CLIPBOARD]
    set lb [listview_windowname ${arrayName}].lb
    set itemNum [lindex [$lb curselection] 0]
    ::dialog_array::listview_edit+paste $arrayName $itemNum $sel
}

proc ::dialog_array::listview_edit {arrayName page font} {
    set lb [listview_windowname ${arrayName}].lb
    set entry ${lb}.entry
    if {[winfo exists $entry]} {
        ::dialog_array::listview_update_entry \
            $arrayName $::dialog_array::listview_entry($arrayName)
        unset ::dialog_array::listview_entry($arrayName)
    }
    set itemNum [$lb index active]
    set ::dialog_array::listview_entry($arrayName) $itemNum
    set bbox [$lb bbox $itemNum]
    set y [expr [lindex $bbox 1] - 4]
    entry $entry \
        -font [format {{%s} %d %s} $::font_family $font $::font_weight]
    $entry insert 0 []
    place configure $entry -relx 0 -y $y -relwidth 1
    lower $entry
    focus $entry
    bind $entry <Return> \
        "::dialog_array::listview_update_entry \{$arrayName\} $itemNum;"
}

proc ::dialog_array::listview_update_entry {arrayName itemNum} {
    set entry [::dialog_array::listview_windowname $arrayName].lb.entry
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

proc ::dialog_array::apply {mytoplevel} {
    pdsend "$mytoplevel arraydialog \
            [::dialog_gatom::escape [$mytoplevel.array.name.entry get]] \
            [$mytoplevel.array.size.entry get] \
            [expr $::dialog_array::saveme_button($mytoplevel) + (2 * $::dialog_array::drawas_button($mytoplevel))] \
            $::dialog_array::otherflag_button($mytoplevel)"
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

proc ::dialog_array::pdtk_array_dialog {mytoplevel name size flags newone} {
    if {[winfo exists $mytoplevel]} {
        wm deiconify $mytoplevel
        raise $mytoplevel
        focus $mytoplevel
    } else {
        create_dialog $mytoplevel $newone
    }

    $mytoplevel.array.name.entry insert 0 [::dialog_gatom::unescape $name]
    $mytoplevel.array.size.entry insert 0 $size
    set ::dialog_array::saveme_button($mytoplevel) [expr $flags & 1]
    set ::dialog_array::drawas_button($mytoplevel) [expr ( $flags & 6 ) >> 1]
    set ::dialog_array::otherflag_button($mytoplevel) 0
# pd -> tcl
#  2 * (int)(template_getfloat(template_findbyname(sc->sc_template), gensym("style"), x->x_scalar->sc_vec, 1)));

# tcl->pd
#    int style = ((flags & 6) >> 1);
}

proc ::dialog_array::create_dialog {mytoplevel newone} {
    toplevel $mytoplevel -class DialogWindow
    wm title $mytoplevel [_ "Array Properties"]
    wm group $mytoplevel .
    wm resizable $mytoplevel 0 0
    wm transient $mytoplevel $::focused_window
    $mytoplevel configure -menu $::dialog_menubar
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
    entry $mytoplevel.array.size.entry -width 17
    pack $mytoplevel.array.size.entry $mytoplevel.array.size.label -side right

    checkbutton $mytoplevel.array.saveme -text [_ "Save contents"] \
        -variable ::dialog_array::saveme_button($mytoplevel) -anchor w
    pack $mytoplevel.array.saveme -side top

    # draw as
    labelframe $mytoplevel.drawas -text [_ "Draw as:"] -padx 20 -borderwidth 1
    pack $mytoplevel.drawas -side top -fill x
    radiobutton $mytoplevel.drawas.points -value 0 \
        -variable ::dialog_array::drawas_button($mytoplevel) -text [_ "Polygon"]
    radiobutton $mytoplevel.drawas.polygon -value 1 \
        -variable ::dialog_array::drawas_button($mytoplevel) -text [_ "Points"]
    radiobutton $mytoplevel.drawas.bezier -value 2 \
        -variable ::dialog_array::drawas_button($mytoplevel) -text [_ "Bezier curve"]
    pack $mytoplevel.drawas.points -side top -anchor w
    pack $mytoplevel.drawas.polygon -side top -anchor w
    pack $mytoplevel.drawas.bezier -side top -anchor w

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
    } else {
        labelframe $mytoplevel.options -text [_ "Options"] -padx 20 -borderwidth 1
        pack $mytoplevel.options -side top -fill x
        button $mytoplevel.options.listview -text [_ "Open List View..."] \
            -command "::dialog_array::openlistview $mytoplevel [$mytoplevel.array.name.entry get]"
        pack $mytoplevel.options.listview -side top
        checkbutton $mytoplevel.options.deletearray -text [_ "Delete array"] \
            -variable ::dialog_array::otherflag_button($mytoplevel) -anchor w
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
