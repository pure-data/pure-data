package provide pdtk_array 0.1

#### jsarlo #####
proc pdtk_array_listview_setpage {arrayName page} {
    global pd_array_listview_page
    set pd_array_listview_page($arrayName) $page
}

proc pdtk_array_listview_changepage {arrayName np} {
    global pd_array_listview_page
    pdtk_array_listview_setpage \
        $arrayName [expr $pd_array_listview_page($arrayName) + $np]
    pdtk_array_listview_fillpage $arrayName
}

proc pdtk_array_listview_fillpage {arrayName} {
    global pd_array_listview_page
    global pd_array_listview_id
    set windowName [format ".%sArrayWindow" $arrayName]
    set topItem [expr [lindex [$windowName.lb yview] 0] * \
                     [$windowName.lb size]]
    
    if {[winfo exists $windowName]} {
        set cmd "$pd_array_listview_id($arrayName) \
               arrayviewlistfillpage \
               $pd_array_listview_page($arrayName) \
               $topItem"
        
        pdsend $cmd
    }
}

proc pdtk_array_listview_new {id arrayName page} {
    global pd_array_listview_page
    global pd_array_listview_id
    global fontname fontweight
    set pd_array_listview_page($arrayName) $page
    set pd_array_listview_id($arrayName) $id
    set windowName [format ".%sArrayWindow" $arrayName]
    if [winfo exists $windowName] then [destroy $windowName]
    toplevel $windowName
    wm protocol $windowName WM_DELETE_WINDOW \
        "pdtk_array_listview_close $id $arrayName"
    wm title $windowName [concat $arrayName "(list view)"]
    # FIXME
    set font 12
    set $windowName.lb [listbox $windowName.lb -height 20 -width 25\
                            -selectmode extended \
                            -relief solid -background white -borderwidth 1 \
                            -font [format {{%s} %d %s} $fontname $font $fontweight]\
                            -yscrollcommand "$windowName.lb.sb set"]
    set $windowName.lb.sb [scrollbar $windowName.lb.sb \
                               -command "$windowName.lb yview" -orient vertical]
    place configure $windowName.lb.sb -relheight 1 -relx 0.9 -relwidth 0.1
    pack $windowName.lb -expand 1 -fill both
    bind $windowName.lb <Double-ButtonPress-1> \
        "pdtk_array_listview_edit $arrayName $page $font"
    # handle copy/paste
    if {[tk windowingsystem] eq "x11"} {
        selection handle $windowName.lb \
            "pdtk_array_listview_lbselection $arrayName"
    } else {
        if {[tk windowingsystem] eq "win32"} {
            bind $windowName.lb <ButtonPress-3> \
                "pdtk_array_listview_popup $arrayName"
        } 
    }
    set $windowName.prevBtn [button $windowName.prevBtn -text "<-" \
                                 -command "pdtk_array_listview_changepage $arrayName -1"]
    set $windowName.nextBtn [button $windowName.nextBtn -text "->" \
                                 -command "pdtk_array_listview_changepage $arrayName 1"]
    pack $windowName.prevBtn -side left -ipadx 20 -pady 10 -anchor s
    pack $windowName.nextBtn -side right -ipadx 20 -pady 10 -anchor s
    focus $windowName
}

proc pdtk_array_listview_lbselection {arrayName off size} {
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
proc pdtk_array_listview_popup {arrayName} {
    set windowName [format ".%sArrayWindow" $arrayName]
    if [winfo exists $windowName.popup] then [destroy $windowName.popup]
    menu $windowName.popup -tearoff false
    $windowName.popup add command -label {Copy} \
        -command "pdtk_array_listview_copy $arrayName; \
                  destroy $windowName.popup"
    $windowName.popup add command -label {Paste} \
        -command "pdtk_array_listview_paste $arrayName; \
                  destroy $windowName.popup"
    tk_popup $windowName.popup [winfo pointerx $windowName] \
        [winfo pointery $windowName] 0
}

proc pdtk_array_listview_copy {arrayName} {
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

proc pdtk_array_listview_paste {arrayName} {
    global pd_array_listview_page
    global pd_array_listview_pagesize
    set cbString [selection get -selection CLIPBOARD]
    set lbName [format ".%sArrayWindow.lb" $arrayName]
    set itemNum [lindex [$lbName curselection] 0]
    set splitChars ", \n"
    set itemString [split $cbString $splitChars]
    set flag 1
    for {set i 0; set counter 0} {$i < [llength $itemString]} {incr i} {
        if {[lindex $itemString $i] != {}} {
            pdsend "$arrayName [expr $itemNum + \
                                       [expr $counter + \
                                            [expr $pd_array_listview_pagesize \
                                                 * $pd_array_listview_page($arrayName)]]] \
                    [lindex $itemString $i]"
            incr counter
            set flag 0
        }
    }
}

proc pdtk_array_listview_edit {arrayName page font} {
    global pd_array_listview_entry
    global fontname fontweight
    set lbName [format ".%sArrayWindow.lb" $arrayName]
    if {[winfo exists $lbName.entry]} {
        pdtk_array_listview_update_entry \
            $arrayName $pd_array_listview_entry($arrayName)
        unset pd_array_listview_entry($arrayName)
    }
    set itemNum [$lbName index active]
    set pd_array_listview_entry($arrayName) $itemNum
    set bbox [$lbName bbox $itemNum]
    set y [expr [lindex $bbox 1] - 4]
    set $lbName.entry [entry $lbName.entry \
                           -font [format {{%s} %d %s} $fontname $font $fontweight]]
    $lbName.entry insert 0 []
    place configure $lbName.entry -relx 0 -y $y -relwidth 1
    lower $lbName.entry
    focus $lbName.entry
    bind $lbName.entry <Return> \
        "pdtk_array_listview_update_entry $arrayName $itemNum;"
}

proc pdtk_array_listview_update_entry {arrayName itemNum} {
    global pd_array_listview_page
    global pd_array_listview_pagesize
    set lbName [format ".%sArrayWindow.lb" $arrayName]
    set splitChars ", \n"
    set itemString [split [$lbName.entry get] $splitChars]
    set flag 1
    for {set i 0; set counter 0} {$i < [llength $itemString]} {incr i} {
        if {[lindex $itemString $i] != {}} {
            pdsend [concat $arrayName [expr $itemNum + \
                                       [expr $counter + \
                                            [expr $pd_array_listview_pagesize \
                                                 * $pd_array_listview_page($arrayName)]]] \
                    [lindex $itemString $i] \;]
            incr counter
            set flag 0
        }
    }
    pdtk_array_listview_fillpage $arrayName
    destroy $lbName.entry
}

proc pdtk_array_listview_closeWindow {arrayName} {
    set windowName [format ".%sArrayWindow" $arrayName]
    destroy $windowName
}

proc pdtk_array_listview_close {id arrayName} {
    pdtk_array_listview_closeWindow $arrayName
    pdsend "$id arrayviewclose"
}
##### end jsarlo #####

############ pdtk_array_dialog -- dialog window for arrays #########
# see comments above (pdtk_gatom_dialog) about variable name handling 

proc array_apply {id} {
    # strip "." from the TK id to make a variable name suffix 
    set vid [string trimleft $id .]
    # for each variable, make a local variable to hold its name...
    set var_array_name [concat array_name_$vid]
    global $var_array_name
    set var_array_n [concat array_n_$vid]
    global $var_array_n
    set var_array_saveit [concat array_saveit_$vid]
    global $var_array_saveit
    set var_array_drawasrects [concat array_drawasrects_$vid]
    global $var_array_drawasrects
    set var_array_otherflag [concat array_otherflag_$vid]
    global $var_array_otherflag
    set mofo [eval concat $$var_array_name]
    if {[string index $mofo 0] == "$"} {
        set mofo [string replace $mofo 0 0 #] }

    set saveit [eval concat $$var_array_saveit]
    set drawasrects [eval concat $$var_array_drawasrects]

    pdsend "$id arraydialog $mofo [eval concat $$var_array_n] \
            [expr $saveit + 2 * $drawasrects] [eval concat $$var_array_otherflag]"
}

# jsarlo
proc array_viewlist {id} {
    pdsend "$id arrayviewlistnew"
}
# end jsarlo

proc array_cancel {id} {
    pdsend "$id cancel"
}

proc array_ok {id} {
    array_apply $id
    array_cancel $id
}

proc pdtk_array_dialog {id name n flags newone} {
    set vid [string trimleft $id .]

    set var_array_name [concat array_name_$vid]
    global $var_array_name
    set var_array_n [concat array_n_$vid]
    global $var_array_n
    set var_array_saveit [concat array_saveit_$vid]
    global $var_array_saveit
    set var_array_drawasrects [concat array_drawasrects_$vid]
    global $var_array_drawasrects
    set var_array_otherflag [concat array_otherflag_$vid]
    global $var_array_otherflag

    set $var_array_name $name
    set $var_array_n $n
    set $var_array_saveit [expr ( $flags & 1 ) != 0]
    set $var_array_drawasrects [expr ( $flags & 2 ) != 0]
    set $var_array_otherflag 0

    toplevel $id
    wm title $id {array}
    wm resizable $id 0 0
    wm protocol $id WM_DELETE_WINDOW [concat array_cancel $id]

    ::pd_bindings::panel_bindings $id "array"

    frame $id.name
    pack $id.name -side top
    label $id.name.label -text "name"
    entry $id.name.entry -textvariable $var_array_name
    pack $id.name.label $id.name.entry -side left

    frame $id.n
    pack $id.n -side top
    label $id.n.label -text "size"
    entry $id.n.entry -textvariable $var_array_n
    pack $id.n.label $id.n.entry -side left

    checkbutton $id.saveme -text {save contents} -variable $var_array_saveit \
        -anchor w
    pack $id.saveme -side top

    frame $id.drawasrects
    pack $id.drawasrects -side top
    radiobutton $id.drawasrects.drawasrects0 -value 0 \
        -variable $var_array_drawasrects \
        -text "draw as points"
    radiobutton $id.drawasrects.drawasrects1 -value 1 \
        -variable $var_array_drawasrects \
        -text "polygon"
    radiobutton $id.drawasrects.drawasrects2 -value 2 \
        -variable $var_array_drawasrects \
        -text "bezier curve"
    pack $id.drawasrects.drawasrects0 -side top -anchor w
    pack $id.drawasrects.drawasrects1 -side top -anchor w
    pack $id.drawasrects.drawasrects2 -side top -anchor w

    if {$newone != 0} {
        frame $id.radio
        pack $id.radio -side top
        radiobutton $id.radio.radio0 -value 0 \
            -variable $var_array_otherflag \
            -text "in new graph"
        radiobutton $id.radio.radio1 -value 1 \
            -variable $var_array_otherflag \
            -text "in last graph"
        pack $id.radio.radio0 -side top -anchor w
        pack $id.radio.radio1 -side top -anchor w
    } else {    
        checkbutton $id.deleteme -text {delete me} \
            -variable $var_array_otherflag -anchor w
        pack $id.deleteme -side top
    }
    # jsarlo
    if {$newone == 0} {
        button $id.listview -text {View list}\
            -command "array_viewlist $id $name 0"
        pack $id.listview -side left
    }
    # end jsarlo
    frame $id.buttonframe
    pack $id.buttonframe -side bottom -fill x -pady 2m
    button $id.buttonframe.cancel -text {Cancel}\
        -command "array_cancel $id"
    if {$newone == 0} {button $id.buttonframe.apply -text {Apply}\
                           -command "array_apply $id"}
    button $id.buttonframe.ok -text {OK}\
        -command "array_ok $id"
    pack $id.buttonframe.cancel -side left -expand 1
    if {$newone == 0} {pack $id.buttonframe.apply -side left -expand 1}
    pack $id.buttonframe.ok -side left -expand 1
    
    $id.name.entry select from 0
    $id.name.entry select adjust end
    focus $id.name.entry
}
