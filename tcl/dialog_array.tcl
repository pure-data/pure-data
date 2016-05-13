package provide dialog_array 0.1

namespace eval ::dialog_array:: {
    namespace export pdtk_array_dialog
    namespace export pdtk_array_listview_new
    namespace export pdtk_array_listview_fillpage
    namespace export pdtk_array_listview_setpage
    namespace export pdtk_array_listview_closeWindow
}

# global variables for the listview
array set pd_array_listview_entry {}
array set pd_array_listview_id {}
array set pd_array_listview_page {}
set pd_array_listview_pagesize 0
# this stores the state of the "save me" check button
array set saveme_button {}
# this stores the state of the "draw as" radio buttons
array set drawas_button {}
# this stores the state of the "in new graph"/"in last graph" radio buttons
# and the "delete array" checkbutton
array set otherflag_button {}

############ pdtk_array_dialog -- dialog window for arrays #########

proc ::dialog_array::pdtk_array_listview_setpage {arrayName page} {
    set ::pd_array_listview_page($arrayName) $page
}

proc ::dialog_array::listview_changepage {arrayName np} {
    pdtk_array_listview_setpage \
        $arrayName [expr $::pd_array_listview_page($arrayName) + $np]
    pdtk_array_listview_fillpage $arrayName
}

proc ::dialog_array::pdtk_array_listview_fillpage {arrayName} {
    set windowName [format ".%sArrayWindow" $arrayName]
    set topItem [expr [lindex [$windowName.lb yview] 0] * \
                     [$windowName.lb size]]
    
    if {[winfo exists $windowName]} {
        set cmd "$::pd_array_listview_id($arrayName) \
               arrayviewlistfillpage \
               $::pd_array_listview_page($arrayName) \
               $topItem"
        
        pdsend $cmd
    }
}

proc ::dialog_array::pdtk_array_listview_new {id arrayName page} {
    set ::pd_array_listview_page($arrayName) $page
    set ::pd_array_listview_id($arrayName) $id
    set windowName [format ".%sArrayWindow" $arrayName]
    if [winfo exists $windowName] then [destroy $windowName]
    toplevel $windowName -class DialogWindow
    wm group $windowName .
    wm protocol $windowName WM_DELETE_WINDOW \
        "::dialog_array::listview_close $id $arrayName"
    wm title $windowName [concat $arrayName "(list view)"]
    # FIXME
    set font 12
    set $windowName.lb [listbox $windowName.lb -height 20 -width 25\
                            -selectmode extended \
                            -relief solid -background white -borderwidth 1 \
                            -font [format {{%s} %d %s} $::font_family $font $::font_weight]\
                            -yscrollcommand "$windowName.lb.sb set"]
    set $windowName.lb.sb [scrollbar $windowName.lb.sb \
                               -command "$windowName.lb yview" -orient vertical]
    place configure $windowName.lb.sb -relheight 1 -relx 0.9 -relwidth 0.1
    pack $windowName.lb -expand 1 -fill both
    bind $windowName.lb <Double-ButtonPress-1> \
        "::dialog_array::listview_edit $arrayName $page $font"
    # handle copy/paste
    switch -- $::windowingsystem {
        "x11" {selection handle $windowName.lb \
                   "::dialog_array::listview_lbselection $arrayName"}
        "win32" {bind $windowName.lb <ButtonPress-3> \
                     "::dialog_array::listview_popup $arrayName"} 
    }
    set $windowName.prevBtn [button $windowName.prevBtn -text "<-" \
                                 -command "::dialog_array::listview_changepage $arrayName -1"]
    set $windowName.nextBtn [button $windowName.nextBtn -text "->" \
                                 -command "::dialog_array::listview_changepage $arrayName 1"]
    pack $windowName.prevBtn -side left -ipadx 20 -pady 10 -anchor s
    pack $windowName.nextBtn -side right -ipadx 20 -pady 10 -anchor s
    focus $windowName
}

proc ::dialog_array::listview_lbselection {arrayName off size} {
    set windowName [format ".%sArrayWindow" $arrayName]
    set itemNums [$windowName.lb curselection]
    set cbString ""
    for {set i 0} {$i < [expr [llength $itemNums] - 1]} {incr i} {
        set listItem [$windowName.lb get [lindex $itemNums $i]]
        append cbString [string range $listItem \
                             [expr [string first ") " $listItem] + 2] \
                             end]
        append cbString "\n"
    }
    set listItem [$windowName.lb get [lindex $itemNums $i]]
    append cbString [string range $listItem \
                         [expr [string first ") " $listItem] + 2] \
                         end]
    set last $cbString
}

# Win32 uses a popup menu for copy/paste
proc ::dialog_array::listview_popup {arrayName} {
    set windowName [format ".%sArrayWindow" $arrayName]
    if [winfo exists $windowName.popup] then [destroy $windowName.popup]
    menu $windowName.popup -tearoff false
    $windowName.popup add command -label [_ "Copy"] \
        -command "::dialog_array::listview_copy $arrayName; \
                  destroy $windowName.popup"
    $windowName.popup add command -label [_ "Paste"] \
        -command "::dialog_array::listview_paste $arrayName; \
                  destroy $windowName.popup"
    tk_popup $windowName.popup [winfo pointerx $windowName] \
        [winfo pointery $windowName] 0
}

proc ::dialog_array::listview_copy {arrayName} {
    set windowName [format ".%sArrayWindow" $arrayName]
    set itemNums [$windowName.lb curselection]
    set cbString ""
    for {set i 0} {$i < [expr [llength $itemNums] - 1]} {incr i} {
        set listItem [$windowName.lb get [lindex $itemNums $i]]
        append cbString [string range $listItem \
                             [expr [string first ") " $listItem] + 2] \
                             end]
        append cbString "\n"
    }
    set listItem [$windowName.lb get [lindex $itemNums $i]]
    append cbString [string range $listItem \
                         [expr [string first ") " $listItem] + 2] \
                         end]
    clipboard clear
    clipboard append $cbString
}

proc ::dialog_array::listview_paste {arrayName} {
    set cbString [selection get -selection CLIPBOARD]
    set lbName [format ".%sArrayWindow.lb" $arrayName]
    set itemNum [lindex [$lbName curselection] 0]
    set splitChars ", \n"
    set itemString [split $cbString $splitChars]
    set flag 1
    for {set i 0; set counter 0} {$i < [llength $itemString]} {incr i} {
        if {[lindex $itemString $i] ne {}} {
            pdsend "$arrayName [expr $itemNum + \
                                       [expr $counter + \
                                            [expr $::pd_array_listview_pagesize \
                                                 * $::pd_array_listview_page($arrayName)]]] \
                    [lindex $itemString $i]"
            incr counter
            set flag 0
        }
    }
}

proc ::dialog_array::listview_edit {arrayName page font} {
    set lbName [format ".%sArrayWindow.lb" $arrayName]
    if {[winfo exists $lbName.entry]} {
        ::dialog_array::listview_update_entry \
            $arrayName $::pd_array_listview_entry($arrayName)
        unset ::pd_array_listview_entry($arrayName)
    }
    set itemNum [$lbName index active]
    set ::pd_array_listview_entry($arrayName) $itemNum
    set bbox [$lbName bbox $itemNum]
    set y [expr [lindex $bbox 1] - 4]
    set $lbName.entry [entry $lbName.entry \
                           -font [format {{%s} %d %s} $::font_family $font $::font_weight]]
    $lbName.entry insert 0 []
    place configure $lbName.entry -relx 0 -y $y -relwidth 1
    lower $lbName.entry
    focus $lbName.entry
    bind $lbName.entry <Return> \
        "::dialog_array::listview_update_entry $arrayName $itemNum;"
}

proc ::dialog_array::listview_update_entry {arrayName itemNum} {
    set lbName [format ".%sArrayWindow.lb" $arrayName]
    set splitChars ", \n"
    set itemString [split [$lbName.entry get] $splitChars]
    set flag 1
    for {set i 0; set counter 0} {$i < [llength $itemString]} {incr i} {
        if {[lindex $itemString $i] ne {}} {
            pdsend "$arrayName [expr $itemNum + \
                                       [expr $counter + \
                                            [expr $::pd_array_listview_pagesize \
                                                 * $::pd_array_listview_page($arrayName)]]] \
                    [lindex $itemString $i]"
            incr counter
            set flag 0
        }
    }
    pdtk_array_listview_fillpage $arrayName
    destroy $lbName.entry
}

proc ::dialog_array::pdtk_array_listview_closeWindow {arrayName} {
    set mytoplevel [format ".%sArrayWindow" $arrayName]
    destroy $mytoplevel
}

proc ::dialog_array::listview_close {mytoplevel arrayName} {
    pdtk_array_listview_closeWindow $arrayName
    pdsend "$mytoplevel arrayviewclose"
}

proc ::dialog_array::apply {mytoplevel} {
    pdsend "$mytoplevel arraydialog \
            [::dialog_gatom::escape [$mytoplevel.array.name.entry get]] \
            [$mytoplevel.array.size.entry get] \
            [expr $::saveme_button($mytoplevel) + (2 * $::drawas_button($mytoplevel))] \
            $::otherflag_button($mytoplevel)"
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
    } else {
        create_dialog $mytoplevel $newone
    }

    $mytoplevel.array.name.entry insert 0 [::dialog_gatom::unescape $name]
    $mytoplevel.array.size.entry insert 0 $size
    set ::saveme_button($mytoplevel) [expr $flags & 1]
    set ::drawas_button($mytoplevel) [expr ( $flags & 6 ) >> 1]
    set ::otherflag_button($mytoplevel) 0
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
        -variable ::saveme_button($mytoplevel) -anchor w
    pack $mytoplevel.array.saveme -side top

    # draw as
    labelframe $mytoplevel.drawas -text [_ "Draw as:"] -padx 20 -borderwidth 1
    pack $mytoplevel.drawas -side top -fill x
    radiobutton $mytoplevel.drawas.points -value 0 \
        -variable ::drawas_button($mytoplevel) -text [_ "Polygon"]
    radiobutton $mytoplevel.drawas.polygon -value 1 \
        -variable ::drawas_button($mytoplevel) -text [_ "Points"]
    radiobutton $mytoplevel.drawas.bezier -value 2 \
        -variable ::drawas_button($mytoplevel) -text [_ "Bezier curve"]
    pack $mytoplevel.drawas.points -side top -anchor w
    pack $mytoplevel.drawas.polygon -side top -anchor w
    pack $mytoplevel.drawas.bezier -side top -anchor w

    # options
    if {$newone == 1} {
        labelframe $mytoplevel.options -text [_ "Put array into:"] -padx 20 -borderwidth 1
        pack $mytoplevel.options -side top -fill x
        radiobutton $mytoplevel.options.radio0 -value 0 \
            -variable ::otherflag_button($mytoplevel) -text [_ "New graph"]
        radiobutton $mytoplevel.options.radio1 -value 1 \
            -variable ::otherflag_button($mytoplevel) -text [_ "Last graph"]
        pack $mytoplevel.options.radio0 -side top -anchor w
        pack $mytoplevel.options.radio1 -side top -anchor w
    } else {
        labelframe $mytoplevel.options -text [_ "Options"] -padx 20 -borderwidth 1
        pack $mytoplevel.options -side top -fill x
        button $mytoplevel.options.listview -text [_ "Open List View..."] \
            -command "::dialog_array::openlistview $mytoplevel [$mytoplevel.array.name.entry get]"
        pack $mytoplevel.options.listview -side top
        checkbutton $mytoplevel.options.deletearray -text [_ "Delete array"] \
            -variable ::otherflag_button($mytoplevel) -anchor w
        pack $mytoplevel.options.deletearray -side top
    }

    # buttons
    frame $mytoplevel.buttonframe
    pack $mytoplevel.buttonframe -side bottom -expand 1 -fill x -pady 2m
    button $mytoplevel.buttonframe.cancel -text [_ "Cancel"] \
        -command "::dialog_array::cancel $mytoplevel"
    pack $mytoplevel.buttonframe.cancel -side left -expand 1 -fill x -padx 10
    if {$newone == 0 && $::windowingsystem ne "aqua"} {
        button $mytoplevel.buttonframe.apply -text [_ "Apply"] \
            -command "::dialog_array::apply $mytoplevel"
        pack $mytoplevel.buttonframe.apply -side left -expand 1 -fill x -padx 10
    }
    button $mytoplevel.buttonframe.ok -text [_ "OK"]\
        -command "::dialog_array::ok $mytoplevel" -default active
    pack $mytoplevel.buttonframe.ok -side left -expand 1 -fill x -padx 10

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

        # can't see focus for buttons, so disable it
        $mytoplevel.array.saveme config -takefocus 0
        $mytoplevel.drawas.points config -takefocus 0
        $mytoplevel.drawas.polygon config -takefocus 0
        $mytoplevel.drawas.bezier config -takefocus 0
        if {[winfo exists $mytoplevel.options.radio0]} {
            $mytoplevel.options.radio0 config -takefocus 0
        }
        if {[winfo exists $mytoplevel.options.radio1]} {
            $mytoplevel.options.radio1 config -takefocus 0
        }
        if {[winfo exists $mytoplevel.options.deletearray]} {
            $mytoplevel.options.deletearray config -takefocus 0
        }
        if {[winfo exists $mytoplevel.options.listview]} {
            $mytoplevel.options.listview config -takefocus 0
        }

        # show active focus on the ok button as it *is* activated on Return
        $mytoplevel.buttonframe.ok config -default normal
        bind $mytoplevel.buttonframe.ok <FocusIn> "$mytoplevel.buttonframe.ok config -default active"
        bind $mytoplevel.buttonframe.ok <FocusOut> "$mytoplevel.buttonframe.ok config -default normal"
    }
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
