
package provide helpbrowser 0.2

namespace eval ::helpbrowser:: {
    variable libdirlist
    variable helplist
    variable reference_count
    variable reference_paths
    variable doctypes "*.{pd,pat,mxb,mxt,help,txt,htm,html,pdf,c}"

    namespace export open_helpbrowser
}

################## help browser and support functions #########################

proc ::helpbrowser::open_helpbrowser {} {
    if { [winfo exists .helpbrowser.frame] } {
        wm deiconify .helpbrowser
        raise .helpbrowser
    } else {
        toplevel .helpbrowser -class HelpBrowser
        wm group .helpbrowser .
        wm transient .helpbrowser
        wm title .helpbrowser [_ "Help Browser"]
        bind .helpbrowser <$::modifier-Key-w> "wm withdraw .helpbrowser"

        if {$::windowingsystem eq "aqua"} {
            .helpbrowser configure -menu $::dialog_menubar
        }

        wm resizable .helpbrowser 0 1
        frame .helpbrowser.frame
        pack .helpbrowser.frame -side top -fill both -expand 1
        build_references
        make_rootlistbox
    }
}

# check for deleting old listboxes
proc ::helpbrowser::check_destroy {level} {

    set winlist list
    set winlevel 0
    foreach child [winfo children .helpbrowser.frame] {
        regexp \\d+ $child winlevel
        if {$winlevel >= $level} {
            lappend winlist $child
        }
        unset -nocomplain winlevel
    }
    # check for [file readable]?
    # requires Tcl 8.5 but probably deals with special chars better:
    #        destroy {*}[lrange [winfo children .helpbrowser.frame] [expr {2 * $count}] end]

    if { [catch { eval destroy $winlist } errorMessage] } {
        ::pdwindow::error "helpbrowser: error destroying listbox\n"
    }
}

# make the root listbox of the help browser using the pre-built lists
proc ::helpbrowser::make_rootlistbox {} {
    variable libdirlist
    variable helplist

    # exportselection 0 looks good, but selection gets easily out-of-sync
    set current_listbox [listbox "[set b .helpbrowser.frame.root0]" -yscrollcommand "$b-scroll set" \
                             -highlightbackground white -highlightthickness 5 \
                             -highlightcolor white -selectborderwidth 0 \
                             -height 20 -width 24 -exportselection 0 -bd 0]
    pack $current_listbox [scrollbar "$b-scroll" -command [list $current_listbox yview]] \
        -side left -fill both -expand 1
    # first show the directories (for easier navigation)
    foreach item [lsort  $libdirlist] {
        $current_listbox insert end $item
    }
    # then show the (potentially) long list of patches
    foreach item [lsort $helplist] {
        $current_listbox insert end $item
    }

    if {[$current_listbox size] != "0"} {
        $current_listbox selection set 0
    }

    bind $current_listbox <Button-1> \
        "::helpbrowser::root_navigate %W %x %y"
    bind $current_listbox <Double-ButtonRelease-1> \
        "::helpbrowser::root_doubleclick %W %x %y"
    bind $current_listbox <Key-Return> \
        "::helpbrowser::root_return %W"
    bind $current_listbox <Key-Right> \
        "::helpbrowser::root_right %W"
    bind $current_listbox <$::modifier-Key-o> \
        "::helpbrowser::root_doubleclick %W %x %y"
    bind $current_listbox <FocusIn> \
        "::helpbrowser::scroll_destroy %W 2"
    focus $current_listbox
}

# destroy a column
proc ::helpbrowser::scroll_destroy {window level} {
    $window xview 0
    check_destroy $level
}

# try to open a file or dir
proc ::helpbrowser::open_path {dir filename} {
    if {[file exists [file join $dir $filename]] eq 0} {
        return
    }
    ::pdwindow::verbose 0 "menu_doc_open $dir $filename\n"
    if { [catch {menu_doc_open $dir $filename} fid] } {
        ::pdwindow::error "Could not open $dir/$filename\n"
    }
}

# navigate from one column to the right
proc ::helpbrowser::root_right {window} {
    variable reference_paths
    if {[set item [$window get active]] eq {}} {
        return
    }
    set filename $reference_paths($item)
    if {[file isdirectory $filename]} {
        focus [make_liblistbox $filename]
    }
}

# open current file
proc ::helpbrowser::root_return {window} {
    variable reference_paths
    if {[set item [$window get active]] eq {}} {
        return
    }
    set filename $reference_paths($item)
    if {[file isfile $filename]} {
        set dir [file dirname $reference_paths($item)]
        set filename [file tail $reference_paths($item)]
        open_path $dir $filename
    }
}

# navigate into a library/directory from the root
proc ::helpbrowser::root_navigate {window x y} {
    variable reference_paths
    ::pdwindow::debug "root_navigate\n"
    if {[set item [$window get [$window index "@$x,$y"]]] eq {}} {
        return
    }
    set filename $reference_paths($item)
    if {[file isdirectory $filename]} {
        focus [make_liblistbox $filename]
    }
}

# double-click action to open the file or folder
proc ::helpbrowser::root_doubleclick {window x y} {
    variable reference_paths
    ::pdwindow::debug "root_doubleclick"
    if {[set item [$window get [$window index "@$x,$y"]]] eq {}} {
        return
    }
    set dir [file dirname $reference_paths($item)]
    set filename [file tail $reference_paths($item)]
    open_path $dir $filename
}

# make the listbox to show the first level contents of a libdir
proc ::helpbrowser::make_liblistbox {dir} {
    variable doctypes

    check_destroy 1
    # exportselection 0 looks good, but selection gets easily out-of-sync
    set current_listbox [listbox "[set b .helpbrowser.frame.root1]" -yscrollcommand "$b-scroll set" \
                             -highlightbackground white -highlightthickness 5 \
                             -highlightcolor white -selectborderwidth 0 \
                             -height 20 -width 24 -exportselection 0 -bd 0]
    pack $current_listbox [scrollbar "$b-scroll" -command [list $current_listbox yview]] \
        -side left -fill both -expand 1
    foreach item [lsort -dictionary [glob -directory $dir -nocomplain -types {d} -- *]] {
        if {[glob -directory $item -nocomplain -types {f} -- $doctypes] ne "" ||
            [glob -directory $item -nocomplain -types {d} -- *] ne ""} {
            $current_listbox insert end "[file tail $item]/"
        }
    }
    foreach item [lsort -dictionary [glob -directory $dir -nocomplain -types {f} -- \
                                         *-{help,meta}.pd]]  {
        $current_listbox insert end [file tail $item]
    }
    $current_listbox insert end "___________________________"
    foreach item [lsort -dictionary [glob -directory $dir -nocomplain -types {f} -- \
                                         *.txt]]  {
        $current_listbox insert end [file tail $item]
    }

    # select first entry
    if {[$current_listbox size] != "0"} {
        $current_listbox selection set 0
    }
    
    bind $current_listbox <Button-1> \
        "::helpbrowser::dir_navigate {$dir} 2 %W %x %y"
    bind $current_listbox <Double-ButtonRelease-1> \
        "::helpbrowser::dir_doubleclick {$dir} 2 %W %x %y"
    bind $current_listbox <Key-Return> \
        "::helpbrowser::dir_return {$dir} 2 %W"
    bind $current_listbox <Key-Right> \
        "::helpbrowser::dir_right {$dir} 2 %W"
    bind $current_listbox <Key-Left> \
        "::helpbrowser::dir_left 0 %W"
    bind $current_listbox <FocusIn> \
        "::helpbrowser::scroll_destroy %W 3"

    return $current_listbox
}

proc ::helpbrowser::make_doclistbox {dir count} {
    variable doctypes

    check_destroy $count
    # exportselection 0 looks good, but selection gets easily out-of-sync
    set current_listbox [listbox "[set b .helpbrowser.frame.root$count]" \
                             -yscrollcommand "$b-scroll set" \
                             -highlightbackground white -highlightthickness 5 \
                             -highlightcolor white -selectborderwidth 0 \
                             -height 20 -width 24 -exportselection 0 -bd 0]
    pack $current_listbox [scrollbar "$b-scroll" -command "$current_listbox yview"] \
        -side left -fill both -expand 1
    
    foreach item [lsort -dictionary [glob -directory $dir -nocomplain -types {d} -- *]] {
        $current_listbox insert end "[file tail $item]/"
    }
    foreach item [lsort -dictionary [glob -directory $dir -nocomplain -types {f} -- \
                                         $doctypes]]  {
        $current_listbox insert end [file tail $item]
    }
    incr count

    # select first entry
    if {[$current_listbox size] != "0"} {
        $current_listbox selection set 0
    }

    bind $current_listbox <Button-1> \
        "::helpbrowser::dir_navigate {$dir} $count %W %x %y"
    bind $current_listbox <Double-ButtonRelease-1> \
        "::helpbrowser::dir_doubleclick {$dir} $count %W %x %y"
    bind $current_listbox <Key-Right> \
        "::helpbrowser::dir_right {$dir} $count %W"
    bind $current_listbox <Key-Left> \
        "::helpbrowser::dir_left [expr $count - 2] %W"
    bind $current_listbox <Key-Return> \
        "::helpbrowser::dir_return {$dir} $count %W"
    bind $current_listbox <FocusIn> \
        "::helpbrowser::scroll_destroy %W [expr $count + 1]"

    return $current_listbox
}

# clear current selection & navigate one column to the left
proc ::helpbrowser::dir_left {count window} {
    $window selection clear 0 end
    focus .helpbrowser.frame.root$count
}

# navigate one column to the right
proc ::helpbrowser::dir_right {dir count window} {
    if {[set newdir [$window get active]] eq {}} {
        return
    }
    set dir_to_open [file join $dir $newdir]
    if {[file isdirectory $dir_to_open]} {
        focus [make_doclistbox $dir_to_open $count]
    }
}

# open current file
proc ::helpbrowser::dir_return {dir count window} {
    if {[set newdir [$window get active]] eq {}} {
        return
    }
    set dir_to_open [file join $dir $newdir]
    if {[file isfile $dir_to_open]} {
        open_path $dir $newdir
    }
}

# navigate into an actual directory
proc ::helpbrowser::dir_navigate {dir count window x y} {
    if {[set newdir [$window get [$window index "@$x,$y"]]] eq {}} {
        return
    }
    set dir_to_open [file join $dir $newdir]
    if {[file isdirectory $dir_to_open]} {
        focus [make_doclistbox $dir_to_open $count]
    }
}

# double-click action to open the file or folder
proc ::helpbrowser::dir_doubleclick {dir count window x y} {
    if {[set filename [$window get [$window index "@$x,$y"]]] eq {}} {
        return
    }
    open_path $dir $filename
}

#------------------------------------------------------------------------------#
# build help browser trees

# TODO check file timestamp against timestamp of when tree was built

proc ::helpbrowser::findfiles {basedir pattern} {
    set basedir [string trimright [file join [file normalize $basedir] { }]]
    set filelist {}

    # Look in the current directory for matching files, -type {f r}
    # means ony readable normal files are looked at, -nocomplain stops
    # an error being thrown if the returned list is empty
    foreach filename [glob -nocomplain -type {f r} -path $basedir $pattern] {
        lappend filelist $filename
    }

    foreach dirName [glob -nocomplain -type {d  r} -path $basedir *] {
        set subdirlist [findfiles $dirName $pattern]
        if { [llength $subdirlist] > 0 } {
            foreach subdirfile $subdirlist {
                lappend filelist $subdirfile
            }
        }
    }
    return $filelist
}

proc ::helpbrowser::add_entry {reflist entry} {
    variable libdirlist
    variable helplist
    variable reference_paths
    variable reference_count
    set entryname [file tail $entry]
    # if we are checking libdirs, then check to see if there is already a
    # libdir with that name that has been discovered in the path.  If so, dump
    # a warning. The trailing slash on $entryname is added below when
    # $entryname is a dir
    if {$reflist eq "libdirlist" && [lsearch -exact $libdirlist $entryname/] > -1} {
        ::pdwindow::error "WARNING: duplicate '$entryname' library found!\n"
        ::pdwindow::error "  '$reference_paths($entryname/)' is active\n"
        ::pdwindow::error "  '$entry' is duplicate\n"
        incr reference_count($entryname)
        append entryname "/ ($reference_count($entryname))"
    } else {
        set reference_count($entryname) 1
        if {[file isdirectory $entry]} {
            append entryname "/"
        }
    }
    lappend $reflist $entryname
    set reference_paths($entryname) $entry
}

proc ::helpbrowser::build_references {} {
    variable libdirlist {" Pure Data/" "---- libraries: -----"}
    variable helplist {}
    variable reference_count
    variable reference_paths

    array set reference_count {}
    array set reference_paths [list \
                                   " Pure Data/" $::sys_libdir/doc \
                                   "---- libraries: -----" "" \
                                  ]
    foreach pathdir $::sys_staticpath {
        if { ! [file isdirectory $pathdir]} {continue}

        # Fix the directory name, this ensures the directory name is in the
        # native format for the platform and contains a final directory seperator
        set dir [string trimright [file join [file normalize $pathdir] { }]]

        ## don't find libdirs anymore: supported libs should be in search path
        # Directory comes from sys_staticpath (aka hardcoded)
        # Then add an entry for each subdir of this directory in Help browser's root column :

        foreach filename [glob -nocomplain -type d -path $dir "*"] {
            add_entry libdirlist $filename
        }
        ## find the stray help patches
        foreach filename [glob -nocomplain -type f -path $dir "*-help.pd"] {
            add_entry helplist $filename
        }
    }
}

