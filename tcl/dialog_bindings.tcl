package provide dialog_bindings 0.1

package require pd_menucommands
package require pd_guiprefs
package require pd_bindings

namespace eval ::dialog_bindings:: {
    # the current shortcut as displayed (e.g. 'Control+Shift+M')
    variable currentshortcut
    # the treeview cell ({<treeid> <item> <column>}, e.g. {.editor.f.tree File|New #1}
    # or empty
    variable currentID {}

    # whether we allow single-char shortcuts, like 'a' or 'Shift-A'
    variable allow1char 0

    # a sorted list of events
    variable events {}
    # an array that maps events to a list of keyboard shortcuts
    # e.g. events(Edit|ZoomIn) == {{Ctrl plus} {Ctrl KP_Add} {Control equal}}
    array set bindlist {}
    # an array that maps keyboard shortcuts back to events
    # e.g. usedshortcuts(Control+plus) == Edit|ZoomIn
    # useful for finding already used bindings
    array set usedshortcuts {}

    # tree structure maps events to their position in the tree
    # e.g. treeleaves(Edit|ZoomIn) == {Edit 15}
    # this stays persistent even when items are detached
    array set treeleaves {}
    # and vice versa: mapping a tree node to all it's children
    # e.g. treechildren(Edit) == {Edit|Undo Edit|Redo Edit|Cut ...}
    array set treechildren {}

    # the search field
    variable searchtext {}
}


proc ::dialog_bindings::reattach2tree {treeid item} {
    catch {
        foreach {parent index} $::dialog_bindings::treeleaves(${item}) {break}
        ${treeid} move ${item} ${parent} ${index}
        if { ${parent} ne {} } {
            ::dialog_bindings::reattach2tree ${treeid} ${parent}
        }
    }
}

# https://wiki.tcl-lang.org/page/Combinatorial%20mathematics%20functions
proc ::dialog_bindings::combinations {myList size {prefix {}}} {
    ;# End recursion when size is 0 or equals our list size
    if {$size == 0} {return [list $prefix]}
    if {$size == [llength $myList]} {return [list [concat $prefix $myList]]}

    set first [lindex $myList 0]
    set rest [lrange $myList 1 end]

    ;# Combine solutions w/ first element and solutions w/o first element
    set ans1 [combinations $rest [expr {$size-1}] [concat $prefix $first]]
    set ans2 [combinations $rest $size $prefix]
    return [concat $ans1 $ans2]
}

proc ::dialog_bindings::getshortcuts {treeid} {
    # get a list of <event> <shortcuts> tuples, as stored in the current treeview
    # as a side-effect, this updates the 'usedshortcuts' array
    array unset ::dialog_bindings::usedshortcuts

    set result {}
    foreach ev $::dialog_bindings::events {
        set keys [${treeid} item ${ev} -values]
        set shortcuts {}
        foreach k ${keys} {
            lappend shortcuts [split ${k} +]
            set ::dialog_bindings::usedshortcuts(${k}) ${ev}
        }
        set ::dialog_bindings::bindlist(${ev}) ${shortcuts}

        lappend result ${ev}
        lappend result ${shortcuts}
    }
    return ${result}
}


proc ::dialog_bindings::make_treecolumns {treeid numcolumns} {
    set columns {}
    for {set i 0} {$i < $numcolumns} {incr i} {
        lappend columns [_ "Shortcut#%d" $i]
    }
    $treeid configure -columns $columns
    foreach c $columns {
        $treeid heading $c -text $c
    }
}

proc ::dialog_bindings::filltree {treeid bindlist {labelroot {}}} {
    variable treeleaves
    variable treechildren
    variable events
    array set labels {}
    set events {}
    foreach {event _} ${bindlist} {lappend events ${event}}
    array set ::dialog_bindings::bindlist ${bindlist}
    if { ${labelroot} != {} } {
        array set labels [::pd_menus::get_events ${labelroot} label]
    }
    set numshortcuts 0
    foreach event ${events} {
        set shortcuts $::dialog_bindings::bindlist(${event})
        set evs {}
        foreach e [split ${event} "|"] {
            set evs2 [concat ${evs} ${e}]
            set ev [join ${evs2} "|"]
            if { ![${treeid} exists ${ev}] } {
                set name [_ ${e}]
                if { [info exists labels(<<${ev}>>)] } {
                    foreach name $labels(<<${ev}>>) {break}
                }
                set parent [join ${evs} "|"]
                ${treeid} insert ${parent} end -id ${ev} -text ${name} -open 1
                set treeleaves(${ev}) [list ${parent} [${treeid} index ${ev}]]
                set treechildren(${parent}) [${treeid} children ${parent}]
            }
            set evs ${evs2}
        }
        if { [llength ${shortcuts}] > ${numshortcuts} } {
            set numshortcuts [llength ${shortcuts}]
        }
        set values {}
        foreach shortcut ${shortcuts} {
            lappend values [join ${shortcut} +]
        }
        ${treeid} item ${event} -values ${values}
    }
    make_treecolumns ${treeid} ${numshortcuts}
    getshortcuts ${treeid}
}

#.menubar add cascade -label Tools -underline 0 -menu [set m [menu .menubar.tools]]
#::pd_menus::add_menu $m command [_ "Install externals..." ] "<<Tools|Deken>>"
proc ::dialog_bindings::get_extra_bindings {bindlist} {
    # tries to find additional bindings from the menu, and returns [concat $bindlist $newbindings]
    # with duplicates removed
    array set seen {}

    foreach {ev binding} $bindlist {
        set seen($ev) 1
    }
    foreach event [::pd_menus::get_events] {
        set ev [string trim $event <>]
        if { [info exists seen($ev)] } {
            continue
        }
        set shortcuts {}
        foreach b [event info ${event}] {
            lappend shortcuts [split [string map {-Key- -} [string trim $b <>]]  -]
        }
        lappend bindlist $ev
        lappend bindlist $shortcuts
        set seen($ev) 1
    }
    return $bindlist
}


proc ::dialog_bindings::create {winid} {
    label ${winid}.help -justify left \
        -text [_ "To edit a keyboard shortcut, double-click on the corresponding event (or shortcut) and type a new accelerator.\nUse right-click for a context-menu to remove shortcuts." ]

    frame ${winid}.interact
    checkbutton ${winid}.interact.allow1char \
        -variable ::dialog_bindings::allow1char \
        -justify left \
        -text [_ "Allow single-character shortcuts.\nDANGER: this can seriously impact the patching workflow." ]

    frame ${winid}.interact.search -borderwidth 10
    label ${winid}.interact.search.label -text [_ "Search shortcuts:" ]
    entry ${winid}.interact.search.entry -textvariable ::dialog_bindings::searchtext
    bind ${winid}.interact.search.entry <KeyRelease> [list ::dialog_bindings::typeinsearch ${winid}.tree ::dialog_bindings::searchtext]
    pack ${winid}.interact.search.label ${winid}.interact.search.entry -anchor w

    set treeid ${winid}.tree
    ::ttk::treeview ${treeid} -selectmode browse
    $treeid column #0 -stretch
    $treeid heading #0 -text [_ "Event" ]

    pack ${winid}.help ${winid}.interact -anchor w
    pack ${winid}.interact.allow1char -anchor w -padx 5 -pady 5 -side left
    pack ${winid}.interact.search -anchor e -padx 5 -pady 5 -side right
    pack ${treeid} -expand 1 -fill both

    set ::pd_bindings::bindlist [get_extra_bindings $::pd_bindings::bindlist]

    filltree $treeid $::pd_bindings::bindlist .

    set f [frame $winid.but]
    pack $f -side left -pady 5 -padx 5 -ipadx 5
    button $f.previous -text [_ "Reset to previous state" ] -command [list ::dialog_bindings::filltree $treeid $::pd_bindings::bindlist]
    button $f.defaults -text [_ "Reset to defaults" ] -command [list ::dialog_bindings::filltree $treeid $::pd_bindings::default_bindlist]
    pack $f.previous -side left
    pack $f.defaults -side left


    bind $treeid <Double-ButtonRelease-1> "::dialog_bindings::doubleclick %W %x %y"
    if {$::windowingsystem eq "aqua"} {
        set rightbutton <2>
    } else {
        set rightbutton <3>
    }
    bind $treeid $rightbutton "::dialog_bindings::rightclick %W %x %y"

    # order of modifier keys:
    # - TheGIMP  : Shift-Control-Alt
    # - Apple    : Control-Option-Shift-Command
    # - Microsoft: Control-Alt-Shift
    # - JMZ      : i would type either Ctrl-Alt-Shift or Ctrl-Shift-Alt

    # guidelines:
    # - https://developer.apple.com/design/human-interface-guidelines/macos/user-interaction/keyboard/
    # - http://msdn.microsoft.com/en-us/library/ms971323.aspx#atg_keyboardshortcuts_windows_shortcut_keys

    # we *really* want the default shortcutsto yield the same results as a recorded shortcut.
    # (e.g. '${control} Shift S'), so duplicate detection is easier
    set metakeys {Control Alt Shift}
    if {$::windowingsystem eq "aqua"} {
        # macOS displays accelerators in this order: Control-Option-Shift-Command
        # with 'Alt' a synonym for 'Option', and 'Control' being distinct from 'Command'
        # we would like the recorded shortcuts match closely with the shown accelerator.
        # however, 'Command' needs to go before 'Shift' (for dupe detection)

        set metakeys {Control Command Option Shift}
    }

    set tag ::dialog_bindings::popup
    for {set i 1} {$i <= [llength $metakeys]} {incr i} {
        foreach modifiers [combinations $metakeys $i] {
            if {$modifiers  == "Shift" } {continue}
            set binding [join [concat $modifiers Key] -]
            bind ${tag} <${binding}> [list ::dialog_bindings::shortcut %W %K ${modifiers} 1]
            set binding [join [concat $modifiers KeyRelease] -]
            bind ${tag} <${binding}> [list ::dialog_bindings::shortcut %W %K ${modifiers} 0]
        }
    }
    # the KeyRelease events are intentionally bound to shortcut,
    # to properly detect KeyRelease on macOS if the modifiers are released before the actual key
    bind ${tag} <Shift-Key> [list ::dialog_bindings::shortcut1 %W %K Shift 1]
    bind ${tag} <Shift-KeyRelease> [list ::dialog_bindings::shortcut %W %K Shift 0]
    bind ${tag} <KeyPress> [list ::dialog_bindings::shortcut1 %W %K {} 1]
    bind ${tag} <KeyRelease> [list ::dialog_bindings::shortcut %W %K {} 0]

    # update the list of used shortcuts:
    getshortcuts $treeid
}
proc ::dialog_bindings::doubleclick {treeid x y} {
    set popup ${treeid}.popup
    destroy $popup
    set item [$treeid identify item $x $y]
    set col [$treeid identify column $x $y]
    set type [$treeid identify region $x $y]
    if { [$treeid children $item] != {} } {
        # we are only interested in leaves
        return
    }
    if { $type == "cell" } {
        foreach {x y w h} [$treeid bbox $item $col] {break;}
        set ::dialog_bindings::currentshortcut [$treeid set $item $col]
    } elseif { $type == "tree" } {
        foreach {x0 y w h} [$treeid bbox $item] {break;}
        foreach x [$treeid bbox $item #1] {break;}
        set w [expr $w + $x0 - $x]
        set ::dialog_bindings::currentshortcut {}
    } else {
        # we are only interested in the shortcut cells
        return
    }
    entry ${popup} -textvariable ::dialog_bindings::currentshortcut -state readonly
    place $popup -x $x -y $y -w $w -h $h
    set ::dialog_bindings::currentID [list $treeid $item $col]

    bind $popup <FocusOut> [list ::dialog_bindings::popup_destroy %W]

    # make need to bind to '::dialog_bindings::popup' as this is where the shortcut-keys go
    # we also must make sure to *not* bind to 'all', so the normal keybindings do not work for us
    bindtags $popup [list $popup [winfo class $popup] ::dialog_bindings::popup]

    focus $popup
}
proc ::dialog_bindings::rightclick {treeid X Y} {
    set m ${treeid}.contextmenu
    destroy $m
    set item [$treeid identify item $X $Y]
    set col [$treeid identify column $X $Y]
    set type [$treeid identify region $X $Y]
    if { [$treeid children $item] != {} } {
        # we are only interested in leaves
        return
    }

    menu $m
    if { $type == "cell" } {
        foreach {x y w h} [$treeid bbox $item $col] {break;}
        $m add command -label [_ "Edit shortcut" ] \
            -command [list ::dialog_bindings::doubleclick $treeid $X $Y]
        $m add command -label [_ "Delete shortcut" ] \
            -command [list ::dialog_bindings::shortcut_clear %W]
    } elseif { $type == "tree" } {
        foreach {x y w h} [$treeid bbox $item] {break;}
        #foreach x [$treeid bbox $item #1] {break;}
        $m add command -label [_ "Add shortcut" ] \
            -command [list ::dialog_bindings::doubleclick $treeid $X $Y]
        $m add command -label [_ "Delete shortcuts" ] \
            -command [list ::dialog_bindings::shortcut_clear %W]
    } else {
        # we are only interested in the shortcut cells
        destroy $m
        return
    }

    set ::dialog_bindings::currentID [list $treeid $item $col]
    bindtags $m [list $m [winfo class $m] ::dialog_bindings::m]
    tk_popup $m [expr [winfo rootx $treeid] + $x] [expr [winfo rooty $treeid] + $y]
}

proc ::dialog_bindings::popup_destroy {popid} {
    # cleanup the popup
    destroy $popid
    set ::dialog_bindings::currentID {}
    set ::dialog_bindings::currentshortcut {}
}
proc ::dialog_bindings::shortcut_set {shortcut} {
    foreach {treeid item col} $::dialog_bindings::currentID {break}
    if { ${col} == {} } {return}

    # try to normalize the keyboard shortcut (titlecasing the actual key)
    set keys [split $shortcut "+"]
    set Keys [lreplace $keys end end [string totitle [lindex $keys end]]]

    set testevent <<::pd_binding::editor::shortcut::test>>
    catch {
        event add ${testevent} <[join $Keys -]>
        set keys $Keys
    }
    event delete ${testevent}

    # check if shortcut is already taken
    set sc [join $keys "+"]
    set oldev [shortcut_check $sc]
    if { $oldev != "" && $oldev != $item } {
        tk_messageBox \
            -title [_ "Shortcut in use!"] \
            -message [_ "The shortcut '%1\$s' is already used for the '%2\$s' event." $sc $oldev] \
            -detail [_ "Please remove the old shortcut before assigning it to a new event." ] \
            -icon error -type ok
        return
    }

    if { $sc !=  {} } {
        set ::dialog_bindings::usedshortcuts($sc) $item
    }

    set shortcuts {}

    if { $col != "#0" } {
        set oldsc [$treeid set $item $col]
        set ::dialog_bindings::usedshortcuts($oldsc) ""
        $treeid set $item $col $sc
    } else {
        if { $sc != {} } {
            # *add* a new shortcut (rather than replace an existing one
            dict set shortcuts $sc 1
        } else {
            # remove all shortcuts
            foreach col [$treeid cget -columns] {
                set oldsc [$treeid set $item $col]
                set ::dialog_bindings::usedshortcuts($oldsc) ""
                $treeid set $item $col $sc
            }

        }
    }

    # remove duplicate and empty entries for the same event
    foreach {it sc} [$treeid set $item] {
        if { $sc != {} } {
            dict set shortcuts $sc 1
        }
    }
    set shortcuts [dict keys $shortcuts]
    set numshortcuts [llength $shortcuts]
    if { $numshortcuts > [llength [$treeid cget -columns]] } {
        make_treecolumns $treeid $numshortcuts
    }
    $treeid item $item -values $shortcuts
}

proc ::dialog_bindings::shortcut_clear {popid} {
    shortcut_set ""
    after idle ::dialog_bindings::popup_destroy $popid
}
proc ::dialog_bindings::shortcut_check {shortcut} {
    if { [info exists ::dialog_bindings::usedshortcuts($shortcut) ] } {
        return $::dialog_bindings::usedshortcuts($shortcut)
    }
    return ""
}

proc ::dialog_bindings::shortcut {popid key modifier state} {
    # the user pressed a new shortcut
    # - ideally, prevent combination that only consist of modifier keys (e.g. "Control+Shift")
    # - if they are releasing (the top-level combination), assign
    if { ! [winfo exists $popid ] } {
        return
    }
    set treeid [winfo parent $popid]
    if { ! $state } {
        # releasing and the user selected a shortcut
        shortcut_set ${::dialog_bindings::currentshortcut}
        popup_destroy $popid
        return
    }
    set ::dialog_bindings::currentshortcut [join [concat $modifier $key] +]
}
proc ::dialog_bindings::shortcut1 {popid key modifier state} {
    set allowed  $::dialog_bindings::allow1char
    if { $allowed } {
        return [::dialog_bindings::shortcut $popid $key $modifier $state]
    }
}

proc ::dialog_bindings::filter {treeid needle} {
    set filtered {}
    set needle [string tolower [string trim ${needle}]]
    if { ${needle} eq {} } {
        set filtered [array names ::dialog_bindings::treeleaves]
    } else {
        foreach item [array names ::dialog_bindings::treeleaves] {
            foreach txt [list ${item} [${treeid} item ${item} -text]] {
                set txt [string tolower [string trim ${txt}]]
                if {[string first ${needle} ${txt}] >= 0} {
                    lappend filtered ${item}
                    break
                }
            }
            ${treeid} detach ${item}
        }
    }

    foreach item ${filtered} {
        ::dialog_bindings::reattach2tree ${treeid} ${item}
        foreach {parent children} [array get ::dialog_bindings::treechildren ${item}] {
            foreach child ${children} {
                ::dialog_bindings::reattach2tree ${treeid} ${child}
            }
        }
    }
}
proc ::dialog_bindings::typeinsearch {treeid varname} {
    upvar ${varname} x
    ::dialog_bindings::filter ${treeid} ${x}
}

proc ::dialog_bindings::reset {winid} {
    set treeid ${winid}.tree
    ::pdwindow::error "TODO: ::dialog_bindings::reset\n"
}
proc ::dialog_bindings::apply {winid} {
    set data [getshortcuts ${winid}.tree]
    set ::pd_bindings::bindlist $data
    ::pd_guiprefs::write KeyBindings $data
    ::pd_bindings::make_events ${data}
    ::pd_menus::update_accelerators
}
