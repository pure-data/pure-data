# META NAME PdExternalsSearch
# META DESCRIPTION Search for externals zipfiles on puredata.info
# META AUTHOR <Chris McCormick> chris@mccormick.cx
# ex: set setl sw=2 sts=2 et

# Search URL:
# http://puredata.info/search_rss?SearchableText=xtrnl-

# The minimum version of TCL that allows the plugin to run
package require Tcl 8.4
# If Tk or Ttk is needed
#package require Ttk
# Any elements of the Pd GUI that are required
# + require everything and all your script needs.
#   If a requirement is missing,
#   Pd will load, but the script will not.
package require http 2
package require pdwindow 0.1
package require pd_menucommands 0.1

namespace eval ::deken:: {
    namespace export open_searchui
    variable mytoplevelref
    variable platform
    variable architecture_substitutes
    variable installpath
}

set ::deken::installpath [ lindex $::sys_staticpath 0 ]

# console message to let them know we're loaded
pdwindow::post  "deken-plugin.tcl (Pd externals search) in $::current_plugin_loadpath loaded.\n"

set ::deken::platform(os) $::tcl_platform(os)
set ::deken::platform(machine) $::tcl_platform(machine)
set ::deken::platform(bits) [ expr [ string length [ format %X -1 ] ] * 4 ]

# normalize W32 OSs
if { [ string match "Windows *" "$::deken::platform(os)" ] > 0 } {
    # we are not interested in the w32 flavour, so we just use 'Windows' for all of them
    set ::deken::platform(os) "Windows"
}
# normalize W32 CPUs
if { "Windows" eq "$::deken::platform(os)" } {
    # in redmond, intel only produces 32bit CPUs,...
    if { "intel" eq "$::deken::platform(machine)" } { set ::deken::platform(machine) "i386" }
    # ... and all 64bit CPUs are manufactured by amd
    #if { "amd64" eq "$::deken::platform(machine)" } { set ::deken::platform(machine) "x86_64" }
}

pdwindow::post "Platform detected: $::deken::platform(os)-$::deken::platform(machine)-$::deken::platform(bits)bit\n"

# architectures that can be substituted for eachother
array set ::deken::architecture_substitutes {}
set ::deken::architecture_substitutes(x86_64) [list "amd64" "i386" "i586" "i686"]
set ::deken::architecture_substitutes(amd64) [list "x86_64" "i386" "i586" "i686"]
set ::deken::architecture_substitutes(i686) [list "i586" "i386"]
set ::deken::architecture_substitutes(i586) [list "i386"]
set ::deken::architecture_substitutes(armv6l) [list "armv6" "arm"]
set ::deken::architecture_substitutes(armv7l) [list "armv7" "armv6l" "armv6" "arm"]

# this function gets called when the menu is clicked
proc ::deken::open_searchui {mytoplevel} {
    if {[winfo exists $mytoplevel]} {
        wm deiconify $mytoplevel
        raise $mytoplevel
    } else {
        create_dialog $mytoplevel
        $mytoplevel.results tag configure warn -foreground orange
    }
    #search_for "freeverb" $mytoplevel.f.resultstext
}

# build the externals search dialog window
proc ::deken::create_dialog {mytoplevel} {
    toplevel $mytoplevel -class DialogWindow
    variable mytoplevelref $mytoplevel
    wm title $mytoplevel [_ "Find externals"]
    wm geometry $mytoplevel 670x550
    wm minsize $mytoplevel 230 360
    wm transient $mytoplevel
    $mytoplevel configure -padx 10 -pady 5

    if {$::windowingsystem eq "aqua"} {
        $mytoplevel configure -menu $::dialog_menubar
    }

    frame $mytoplevel.searchbit
    pack $mytoplevel.searchbit -side top -fill x
    
    entry $mytoplevel.searchbit.entry -font 18 -relief sunken -highlightthickness 1 -highlightcolor blue
    pack $mytoplevel.searchbit.entry -side left -padx 6 -fill x -expand true
    bind $mytoplevel.searchbit.entry <Key-Return> "::deken::initiate_search $mytoplevel"
    focus $mytoplevel.searchbit.entry

    frame $mytoplevel.warning
    pack $mytoplevel.warning -side top -fill x
    label $mytoplevel.warning.label -text "Only install externals uploaded by people you trust."
    pack $mytoplevel.warning.label -side left -padx 6

    button $mytoplevel.searchbit.button -text [_ "Search"] -default active -width 9 -command "::deken::initiate_search $mytoplevel"
    pack $mytoplevel.searchbit.button -side right -padx 6 -pady 3

    text $mytoplevel.results -takefocus 0 -cursor hand2 -height 100 -yscrollcommand "$mytoplevel.results.ys set"
    scrollbar $mytoplevel.results.ys -orient vertical -command "$mytoplevel.results yview"
    pack $mytoplevel.results.ys -side right -fill y
    pack $mytoplevel.results -side left -padx 6 -pady 3 -fill both -expand true
}

proc ::deken::initiate_search {mytoplevel} {
    # let the user know what we're doing
    $mytoplevel.results delete 1.0 end
    $mytoplevel.results insert end "Searching for externals...\n"
    # make the ajax call
    set results [search_for [$mytoplevel.searchbit.entry get]]
    # delete all text in the results
    $mytoplevel.results delete 1.0 end
    if {[llength $results] != 0} {
        # sort the results by reverse date - latest upload first
        set sorted [lsort -index 3 $results]
        for {set i [llength $sorted]} {[incr i -1] >= 0} {} {lappend reversed [lindex $sorted $i]}
        set counter 0
        # build the list UI of results
        foreach r $reversed {
            foreach {title URL creator date} $r {break}
            # sanity check - is this the same OS
            if {[regexp -- "$::deken::platform(os)" $title]} {
                set tag ch$counter
                set readable_date [regsub -all {[TZ]} $date { }]
                $mytoplevel.results insert end "$title\n\tUploaded by $creator $readable_date\n\n" $tag
                $mytoplevel.results tag bind $tag <Enter> "$mytoplevel.results tag configure $tag -foreground blue"
                if {[::deken::architecture_match $title]} {
                    $mytoplevel.results tag bind $tag <Leave> "$mytoplevel.results tag configure $tag -foreground black"
                    $mytoplevel.results tag configure $tag -foreground black
                } else {
                    $mytoplevel.results tag bind $tag <Leave> "$mytoplevel.results tag configure $tag -foreground gray"
                    $mytoplevel.results tag configure $tag -foreground gray
                }
                # have to decode the URL here because otherwise percent signs cause tcl to bug out - not sure why - scripting languages...
                $mytoplevel.results tag bind $tag <1> [list ::deken::clicked_link $mytoplevel [urldecode $URL] $title]
                incr counter
            }
        }
    } else {
        $mytoplevel.results insert end "No matching externals found. Try using the full name e.g. 'freeverb'.\n"
    }
}

# handle a clicked link
proc ::deken::clicked_link {mytoplevel URL filename} {
    ## make sure that the destination path exists
    file mkdir $::deken::installpath
    set fullzipfile "$::deken::installpath/$filename"
    $mytoplevel.results delete 1.0 end
    $mytoplevel.results insert end "Commencing downloading of:\n$URL\nInto $::deken::installpath...\n"
    ::deken::download_file $URL $fullzipfile
    set PWD [ pwd ]
    cd $::deken::installpath
    set success 1
    if { [ catch { exec unzip $fullzipfile } stdout ] } {
        puts $stdout
        set success 0
        # Open both the fullzipfile folder and the zipfile itself
        # NOTE: in tcl 8.6 it should be possible to use the zlib interface to actually do the unzip
        $mytoplevel.results insert end "Unable to extract package automatically.\n" warn
        $mytoplevel.results insert end "Please perform the following steps manually:\n"
        $mytoplevel.results insert end "1. Unzip $fullzipfile.\n"
        pd_menucommands::menu_openfile $fullzipfile
        $mytoplevel.results insert end "2. Copy the contents into $::deken::installpath.\n\n"
        pd_menucommands::menu_openfile $::deken::installpath
        # destroy $mytoplevel
    }
    cd $PWD
    if { $success > 0 } {
        $mytoplevel.results insert end "Successfully unzipped $filename into $::deken::installpath.\n\n"
    }
}

# download a file to a location
# http://wiki.tcl.tk/15303
proc ::deken::download_file {URL outputfilename} {
    set f [open $outputfilename w]
    set status ""
    set errorstatus ""
    fconfigure $f -translation binary
    set httpresult [http::geturl $URL -binary true -progress "::deken::download_progress" -channel $f]
    set status [::http::status $httpresult]
    set errorstatus [::http::error $httpresult]
    flush $f
    close $f
    http::cleanup $httpresult
    return [list $status $errorstatus ]
}

# print the download progress to the results window
proc ::deken::download_progress {token total current} {
    if { $total > 0 } {
        variable mytoplevelref
        set computed [expr {round(100 * (1.0 * $current / $total))}]
        $mytoplevelref.results insert end "= $computed%\n"
    }
}

# test for platform match with our current platform
proc ::deken::architecture_match {title} {
    # if the word size doesn't match, return false
    if {![regexp -- "-$::deken::platform(bits)\\\)" $title]} {
        return 0
    }
    # see if the exact architecture string matches
    if {[regexp -- "-$::deken::platform(machine)-" $title]} {
        return 1
    }
    # see if any substitute architectures match
    if {[llength [array names ::deken::architecture_substitutes -exact $::deken::platform(machine)]] == 1} {
        foreach arch $::deken::architecture_substitutes($::deken::platform(machine)) {
            if {[regexp -- "-$arch-" $title]} {
                return 1
            }
        }
    }
    return 0
}

# make a remote HTTP call and parse and display the results
proc ::deken::search_for {term} {
    set searchresults [list]
    #set token [http::geturl "http://puredata.info/search_rss?SearchableText=$term+externals.zip&portal_type%3Alist=IAEMFile&portal_type%3Alist=PSCfile"]
    set token [http::geturl "http://puredata.info/dekenpackages?name=$term"]
    set contents [http::data $token]
    set splitCont [split $contents "\n"]
    # loop through the resulting XML parsing out entries containing results with a regular expression
    foreach ele $splitCont {
	set ele [ string trim $ele ]
	if { "" ne $ele } {
	    set sele [ split $ele "\t" ]
	    set result [list [ string trim [ lindex $sele 0 ]] [ string trim [ lindex $sele 1 ]] [ string trim [ lindex $sele 2 ]] [ string trim [ lindex $sele 0 ]]]
            #set result [list $title $URL $creator $date]
            lappend searchresults $result
        }
    }
    http::cleanup $token
    return $searchresults
}

# create an entry for our search in the "help" menu
set mymenu .menubar.help
if {$::windowingsystem eq "aqua"} {
    set inserthere 3    
} else {
    set inserthere 4
}
$mymenu insert $inserthere separator
$mymenu insert $inserthere command -label [_ "Find externals"] -command {::deken::open_searchui .externals_searchui}
# bind all <$::modifier-Key-s> {::deken::open_helpbrowser .helpbrowser2}

# http://rosettacode.org/wiki/URL_decoding#Tcl
proc urldecode {str} {
    set specialMap {"[" "%5B" "]" "%5D"}
    set seqRE {%([0-9a-fA-F]{2})}
    set replacement {[format "%c" [scan "\1" "%2x"]]}
    set modStr [regsub -all $seqRE [string map $specialMap $str] $replacement]
    return [encoding convertfrom utf-8 [subst -nobackslash -novariable $modStr]]
}
