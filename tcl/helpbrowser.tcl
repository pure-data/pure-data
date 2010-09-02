
package provide helpbrowser 0.1

namespace eval ::helpbrowser:: {
    variable libdirlist
    variable helplist
    variable reference_count
    variable reference_paths
    variable doctypes "*.{pd,pat,mxb,mxt,help,txt,htm,html,pdf}"

    namespace export open_helpbrowser
}

# TODO remove the doc_ prefix on procs where its not needed
# TODO rename .help_browser to .helpbrowser
# TODO enter and up/down/left/right arrow key bindings for nav

################## help browser and support functions #########################
proc ::helpbrowser::open_helpbrowser {} {
    if { [winfo exists .help_browser.frame] } {
        wm deiconify .help_browser
        raise .help_browser
    } else {
        toplevel .help_browser -class HelpBrowser
        wm group .help_browser .
        wm transient .help_browser
        wm title .help_browser [_ "Help Browser"]
        bind .help_browser <$::modifier-Key-w> "wm withdraw .help_browser"

        if {$::windowingsystem eq "aqua"} {
            .help_browser configure -menu $::dialog_menubar
        }

        wm resizable .help_browser 0 0
        frame .help_browser.frame
        pack .help_browser.frame -side top -fill both
        build_references
        make_rootlistbox .help_browser.frame
    }
}

# make the root listbox of the help browser using the pre-built lists
proc ::helpbrowser::make_rootlistbox {base} {
    variable libdirlist
    variable helplist
    # exportselection 0 looks good, but selection gets easily out-of-sync
	set current_listbox [listbox "[set b $base.root]" -yscrollcommand "$b-scroll set" \
                             -highlightbackground white -highlightthickness 5 \
                             -highlightcolor "#D6E5FC" -selectborderwidth 0 \
                             -height 20 -width 23 -exportselection 0 -bd 0]
	pack $current_listbox [scrollbar "$b-scroll" -command [list $current_listbox yview]] \
        -side left -fill both -expand 1
    foreach item [concat [lsort [concat $libdirlist $helplist]]] {
		$current_listbox insert end $item
	}
	bind $current_listbox <Button-1> \
        [list ::helpbrowser::root_navigate %W %x %y]
    bind $current_listbox <Key-Return> \
        [list ::helpbrowser::root_navigate %W %x %y]
	bind $current_listbox <Double-ButtonRelease-1> \
        [list ::helpbrowser::root_doubleclick %W %x %y]
	bind $current_listbox <$::modifier-Key-o> \
        [list ::helpbrowser::root_doubleclick %W %x %y]
}

# navigate into a library/directory from the root
proc ::helpbrowser::root_navigate {window x y} {
    variable reference_paths
    if {[set item [$window get [$window index "@$x,$y"]]] eq {}} {
        return
    }
    set filename $reference_paths($item)
    if {[file isdirectory $filename]} {
        make_liblistbox [winfo parent $window] $filename
    }
}

# double-click action to open the folder
proc ::helpbrowser::root_doubleclick {window x y} {
    variable reference_paths
    if {[set listname [$window get [$window index "@$x,$y"]]] eq {}} {
        return
    }
    set dir [file dirname $reference_paths($listname)]
    set filename [file tail $reference_paths($listname)]
    ::pdwindow::verbose 0 "menu_doc_open $dir $filename"
    if { [catch {menu_doc_open $dir $filename} fid] } {
        ::pdwindow::warn "Could not open $dir/$filename\n"
    }
}

# make the listbox to show the first level contents of a libdir
proc ::helpbrowser::make_liblistbox {base dir} {
    variable doctypes
    catch { eval destroy [lrange [winfo children $base] 2 end] } errorMessage
    # exportselection 0 looks good, but selection gets easily out-of-sync
	set current_listbox [listbox "[set b $base.listbox0]" -yscrollcommand "$b-scroll set" \
                             -highlightbackground white -highlightthickness 5 \
                             -highlightcolor "#D6E5FC" -selectborderwidth 0 \
                             -height 20 -width 23 -exportselection 0 -bd 0]
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
	bind $current_listbox <Button-1> \
        [list ::helpbrowser::dir_navigate $dir 1 %W %x %y]
	bind $current_listbox <Double-ButtonRelease-1> \
        [list ::helpbrowser::dir_doubleclick $dir 1 %W %x %y]
	bind $current_listbox <Key-Return> \
        [list ::helpbrowser::dir_doubleclick $dir 1 %W %x %y]
}

proc ::helpbrowser::doc_make_listbox {base dir count} {
    variable doctypes
    # check for [file readable]?
	# requires Tcl 8.5 but probably deals with special chars better:
	#        destroy {*}[lrange [winfo children $base] [expr {2 * $count}] end]
	if { [catch { eval destroy [lrange [winfo children $base] \
									[expr { 2 * $count }] end] } errorMessage] } {
		::pdwindow::error "doc_make_listbox: error listing $dir\n"
	}
    # exportselection 0 looks good, but selection gets easily out-of-sync
	set current_listbox [listbox "[set b "$base.listbox$count"]-list" \
                             -yscrollcommand "$b-scroll set" \
                             -highlightbackground white -highlightthickness 5 \
                             -highlightcolor "#D6E5FC" -selectborderwidth 0 \
                             -height 20 -width 23 -exportselection 0 -bd 0]
	pack $current_listbox [scrollbar "$b-scroll" -command "$current_listbox yview"] \
        -side left -fill both -expand 1
	foreach item [lsort -dictionary [glob -directory $dir -nocomplain -types {d} -- *]] {
		$current_listbox insert end "[file tail $item]/"
    }
	foreach item [lsort -dictionary [glob -directory $dir -nocomplain -types {f} -- \
                                         $doctypes]]  {
		$current_listbox insert end [file tail $item]
	}
	bind $current_listbox <Button-1> \
        "::helpbrowser::dir_navigate {$dir} $count %W %x %y"
    bind $current_listbox <Key-Right> \
        "::helpbrowser::dir_navigate {$dir} $count %W %x %y"
	bind $current_listbox <Double-ButtonRelease-1> \
        "::helpbrowser::dir_doubleclick {$dir} $count %W %x %y"
    bind $current_listbox <Key-Return> \
        "::helpbrowser::dir_doubleclick {$dir} $count %W %x %y"
}

# navigate into an actual directory
proc ::helpbrowser::dir_navigate {dir count window x y} {
    if {[set newdir [$window get [$window index "@$x,$y"]]] eq {}} {
        return
    }
    set dir_to_open [file join $dir $newdir]
    if {[file isdirectory $dir_to_open]} {
        doc_make_listbox [winfo parent $window] $dir_to_open [incr count]
    }
}

proc ::helpbrowser::dir_doubleclick {dir count window x y} {
    if {[set filename [$window get [$window index "@$x,$y"]]] eq {}} {
        return
    }
    if { [catch {menu_doc_open $dir $filename} fid] } {
        ::pdwindow::error "Could not open $dir/$filename\n"
    }
}

proc ::helpbrowser::rightclickmenu {dir count window x y} {
    if {[set filename [$window get [$window index "@$x,$y"]]] eq {}} {
        return
    }
    if { [catch {menu_doc_open $dir $filename} fid] } {
        ::pdwindow::error "Could not open $dir/$filename\n"
    }
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
    variable libdirlist {" Pure Data/" "-----------------------"}
    variable helplist {}
    variable reference_count
    variable reference_paths

    array set reference_count {}
    array set reference_paths [list \
                                   " Pure Data/" $::sys_libdir/doc \
                                   "-----------------------" "" \
                                  ]
    foreach pathdir [concat $::sys_searchpath $::sys_staticpath] {
        if { ! [file isdirectory $pathdir]} {continue}
        # Fix the directory name, this ensures the directory name is in the
        # native format for the platform and contains a final directory seperator
        set dir [string trimright [file join [file normalize $pathdir] { }]]
        ## find the libdirs
        foreach filename [glob -nocomplain -type d -path $dir "*"] {
            add_entry libdirlist $filename
        }
        ## find the stray help patches
        foreach filename [glob -nocomplain -type f -path $dir "*-help.pd"] {
            add_entry helplist $filename
        }
    }
}




 	  	 
