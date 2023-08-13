
####### scrollboxwindow -- scrollbox window with default bindings #########
## This is the base dialog behind the Path and Startup dialogs
## This namespace specifies everything the two dialogs have in common,
## with arguments specifying the differences
##
## By default, this creates a dialog centered on the viewing area of the screen
## with cancel, apply, and OK buttons
## which contains a scrollbox widget populated with the given data

package provide scrollboxwindow 0.1

package require preferencewindow
package require scrollbox

namespace eval scrollboxwindow {
}

proc ::scrollboxwindow::get_listdata {mytoplevel} {
    return [$mytoplevel.listbox.box get 0 end]
}

proc ::scrollboxwindow::do_apply {mytoplevel commit_method listdata} {
    $commit_method $listdata
}

# Cancel button action
proc ::scrollboxwindow::cancel {mytoplevel} {
    # in case Pd keeps a receiver objects, we need to notify it
    if { [string first .gfxstub $mytoplevel ] == 0 } {
        pdsend "$mytoplevel cancel"
    }
}

# Apply button action
proc ::scrollboxwindow::apply {mytoplevel commit_method } {
    do_apply $mytoplevel $commit_method [get_listdata $mytoplevel]
}

# OK button action
# The "commit" action can take a second or more,
# long enough to be noticeable, so we only write
# the changes after closing the dialog
proc ::scrollboxwindow::ok {mytoplevel commit_method } {
    set listdata [get_listdata $mytoplevel]
    cancel $mytoplevel
    do_apply $mytoplevel $commit_method $listdata
}

# "Constructor" function for building the window
# mytoplevel -- the window id to use
# listdata -- the data used to populate the scrollbox
# add_method -- a reference to a proc to be called when the user adds a new item
# edit_method -- same as above, for editing and existing item
# commit_method -- same as above, to commit during the "apply" action
# title -- top-level title for the dialog
# width, height -- initial width and height dimensions for the window, also minimum size
# resizable -- 0 or 1, set to 1 for dialog to be resizeable
proc ::scrollboxwindow::make {mytoplevel listdata add_method edit_method commit_method title width height resizable } {
    ::preferencewindow::create ${mytoplevel} ${title} [list $width $height]
    if {$resizable == 0} {
        wm resizable $winid 0 0
    }

    set my [::preferencewindow::add_frame ${mytoplevel} "${title}" ]
    ::preferencewindow::add_cancel ${mytoplevel} "::scrollboxwindow::cancel ${mytoplevel}"
    ::preferencewindow::add_apply ${mytoplevel} "::scrollboxwindow::apply ${my} ${commit_method}"
    ::preferencewindow::add_apply ${mytoplevel} {pdsend "pd save-preferences"}


    # Add the scrollbox widget
    ::scrollbox::make ${my} ${listdata} ${add_method} ${edit_method}
}
