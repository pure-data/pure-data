######### scrollbox -- utility scrollbar with default bindings #######
# scrollbox is used in the Path and Startup dialogs to edit lists of options

package provide scrollbox 0.1

namespace eval scrollbox {
    # This variable keeps track of the last list element we clicked on,
    # used to implement drag-drop reordering of list items
    variable lastIdx 0
}

proc ::scrollbox::get_curidx { mytoplevel } {
    set idx [$mytoplevel.listbox.box index active]
    if {$idx < 0 || \
            $idx == [$mytoplevel.listbox.box index end]} {
        return [expr {[$mytoplevel.listbox.box index end] + 1}]
    }
    return [expr $idx]
}

proc ::scrollbox::insert_item { mytoplevel idx name } {
    if {$name != ""} {
        $mytoplevel.listbox.box insert $idx $name
        set activeIdx [expr {[$mytoplevel.listbox.box index active] + 1}]
        $mytoplevel.listbox.box see $activeIdx
        $mytoplevel.listbox.box activate $activeIdx
        $mytoplevel.listbox.box selection clear 0 end
        $mytoplevel.listbox.box selection set active
        focus $mytoplevel.listbox.box
    }
}

proc ::scrollbox::add_item { mytoplevel add_method } {
    set dir [$add_method]
    insert_item $mytoplevel [expr {[get_curidx $mytoplevel] + 1}] $dir
}

proc ::scrollbox::edit_item { mytoplevel edit_method } {
    set idx [expr {[get_curidx $mytoplevel]}]
    set initialValue [$mytoplevel.listbox.box get $idx]
    if {$initialValue != ""} {
        set dir [$edit_method $initialValue]

        if {$dir != ""} {
            $mytoplevel.listbox.box delete $idx
            insert_item $mytoplevel $idx $dir
        }
        $mytoplevel.listbox.box activate $idx
        $mytoplevel.listbox.box selection clear 0 end
        $mytoplevel.listbox.box selection set active
        focus $mytoplevel.listbox.box
    }
}

proc ::scrollbox::delete_item { mytoplevel } {
    set cursel [$mytoplevel.listbox.box curselection]
    foreach idx $cursel {
        $mytoplevel.listbox.box delete $idx
    }
}

# Double-clicking on the listbox should edit the current item,
# or add a new one if there is no current
proc ::scrollbox::dbl_click { mytoplevel edit_method add_method x y } {
    if { $x == "" || $y == "" } {
        return
    }

    set curBB [$mytoplevel.listbox.box bbox @$x,$y]

    # listbox bbox returns an array of 4 items in the order:
    # left, top, width, height
    set height [lindex $curBB 3]
    set top [lindex $curBB 1]
    if { $height == "" || $top == "" } {
        # If for some reason we didn't get valid bbox info,
        # we want to default to adding a new item
        set height 0
        set top 0
        set y 1
    }

    set bottom [expr {$height + $top}]

    if {$y > $bottom} {
        add_item $mytoplevel $add_method
    } else {
        edit_item $mytoplevel $edit_method
    }
}

proc ::scrollbox::click { mytoplevel x y } {
    # record the index of the current element being
    # clicked on
    variable ::lastIdx [$mytoplevel.listbox.box index @$x,$y]

    focus $mytoplevel.listbox.box
}

# For drag-and-drop reordering, recall the last-clicked index
# and move it to the position of the item currently under the mouse
proc ::scrollbox::release { mytoplevel x y } {
    variable lastIdx
    set curIdx [$mytoplevel.listbox.box index @$x,$y]

    if { $curIdx != $::lastIdx } {
        # clear any current selection
        $mytoplevel.listbox.box selection clear 0 end

        set oldIdx $::lastIdx
        set newIdx [expr {$curIdx+1}]
        set selIdx $curIdx

        if { $curIdx < $::lastIdx } {
            set oldIdx [expr {$::lastIdx + 1}]
            set newIdx $curIdx
            set selIdx $newIdx
        }

        $mytoplevel.listbox.box insert $newIdx [$mytoplevel.listbox.box get $::lastIdx]
        $mytoplevel.listbox.box delete $oldIdx
        $mytoplevel.listbox.box activate $newIdx
        $mytoplevel.listbox.box selection set $selIdx
    }
}

# Make a scrollbox widget in a given window and set of data.
#
# id - the parent window for the scrollbox
# listdata - array of data to populate the scrollbox
# add_method - method to be called when we add a new item
# edit_method - method to be called when we edit an existing item
proc ::scrollbox::make { mytoplevel listdata add_method edit_method } {
    frame $mytoplevel.listbox
    listbox $mytoplevel.listbox.box -relief raised -highlightthickness 0 \
        -selectmode browse -activestyle dotbox \
        -yscrollcommand [list "$mytoplevel.listbox.scrollbar" set]

    # Create a scrollbar and keep it in sync with the current
    # listbox view
    pack $mytoplevel.listbox.box [scrollbar "$mytoplevel.listbox.scrollbar" \
                              -command [list $mytoplevel.listbox.box yview]] \
        -side left -fill y -anchor w 

    # Populate the listbox widget
    foreach item $listdata {
        $mytoplevel.listbox.box insert end $item
    }

    # Standard listbox key/mouse bindings
    event add <<Delete>> <Delete>
    if { $::windowingsystem eq "aqua" } { event add <<Delete>> <BackSpace> }

    bind $mytoplevel.listbox.box <ButtonPress> "::scrollbox::click $mytoplevel %x %y"
    bind $mytoplevel.listbox.box <Double-1> "::scrollbox::dbl_click $mytoplevel $edit_method $add_method %x %y"
    bind $mytoplevel.listbox.box <ButtonRelease> "::scrollbox::release $mytoplevel %x %y"
    bind $mytoplevel.listbox.box <Return> "::scrollbox::edit_item $mytoplevel $edit_method"
    bind $mytoplevel.listbox.box <<Delete>> "::scrollbox::delete_item $mytoplevel"

    # <Configure> is called when the user modifies the window
    # We use it to capture resize events, to make sure the
    # currently selected item in the listbox is always visible
    bind $mytoplevel <Configure> "$mytoplevel.listbox.box see active"

    # The listbox should expand to fill its containing window
    # the "-fill" option specifies which direction (x, y or both) to fill, while
    # the "-expand" option (false by default) specifies whether the widget
    # should fill
    pack $mytoplevel.listbox.box -side left -fill both -expand 1
    pack $mytoplevel.listbox -side top -pady 2m -padx 2m -fill both -expand 1

    # All widget interactions can be performed without buttons, but
    # we still need a "New..." button since the currently visible window
    # might be full (even though the user can still expand it)
    frame $mytoplevel.actions 
    pack $mytoplevel.actions -side top -padx 2m -fill x 
    button $mytoplevel.actions.add_path -text {New...} \
        -command "::scrollbox::add_item $mytoplevel $add_method"
    button $mytoplevel.actions.edit_path -text {Edit...} \
        -command "::scrollbox::edit_item $mytoplevel $edit_method"
    button $mytoplevel.actions.delete_path -text {Delete} \
        -command "::scrollbox::delete_item $mytoplevel"

    pack $mytoplevel.actions.delete_path -side right -pady 2m -padx 5
    pack $mytoplevel.actions.edit_path -side right -pady 2m -padx 5
    pack $mytoplevel.actions.add_path -side right -pady 2m -padx 5

    $mytoplevel.listbox.box activate end
    $mytoplevel.listbox.box selection set end
    focus $mytoplevel.listbox.box
}
