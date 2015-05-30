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

# console message to let them know we're loaded
pdwindow::post  "deken-plugin.tcl (Pd externals search) in $::current_plugin_loadpath loaded.\n"
set ::tcl_platform(bits) [ expr [ string length [ format %X -1 ] ] * 4 ]
pdwindow::post "Platform detected: $tcl_platform(os)-$tcl_platform(machine)-$tcl_platform(bits)bit\n"

namespace eval ::dialog_externals_search:: {
    namespace export open_searchui
    variable mytoplevelref
}

# architectures that can be substituted for eachother
array set architecture_substitutes {}
set architecture_substitutes(x86_64) [list "amd64" "i386" "i586" "i686"]
set architecture_substitutes(amd64) [list "x86_64" "i386" "i586" "i686"]
set architecture_substitutes(i686) [list "i586" "i386"]
set architecture_substitutes(i586) [list "i386"]
set architecture_substitutes(armv6l) [list]
set architecture_substitutes(armv7l) [list "armv6l"]

# this function gets called when the menu is clicked
proc ::dialog_externals_search::open_searchui {mytoplevel} {
    if {[winfo exists $mytoplevel]} {
        wm deiconify $mytoplevel
        raise $mytoplevel
    } else {
        create_dialog $mytoplevel
    }
    #search_for "freeverb" $mytoplevel.f.resultstext
}

# build the externals search dialog window
proc ::dialog_externals_search::create_dialog {mytoplevel} {
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
    pack $mytoplevel.searchbit.entry -side left -padx 10 -fill x -expand true
    bind $mytoplevel.searchbit.entry <Key-Return> "::dialog_externals_search::initiate_search $mytoplevel"
    focus $mytoplevel.searchbit.entry

    button $mytoplevel.searchbit.button -text [_ "Search"] -default active -width 9 -command "dialog_externals_search::initiate_search $mytoplevel"
    pack $mytoplevel.searchbit.button -side right -padx 6 -pady 3

    text $mytoplevel.results -takefocus 0 -cursor hand2 -height 100 -yscrollcommand "$mytoplevel.results.ys set"
    scrollbar $mytoplevel.results.ys -orient vertical -command "$mytoplevel.results yview"
    pack $mytoplevel.results.ys -side right -fill y
    pack $mytoplevel.results -side left -padx 6 -pady 3 -fill both -expand true
}

proc ::dialog_externals_search::initiate_search {mytoplevel} {
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
            if {[regexp "$::tcl_platform(os)" $title]} {
                set tag ch$counter
                set readable_date [regsub -all {[TZ]} $date { }]
                $mytoplevel.results insert end "$title\n\tBy $creator - $readable_date\n\n" $tag
                $mytoplevel.results tag bind $tag <Enter> "$mytoplevel.results tag configure $tag -foreground blue"
                if {[dialog_externals_search::architecture_match $title]} {
                    $mytoplevel.results tag bind $tag <Leave> "$mytoplevel.results tag configure $tag -foreground black"
                    $mytoplevel.results tag configure $tag -foreground black
                } else {
                    $mytoplevel.results tag bind $tag <Leave> "$mytoplevel.results tag configure $tag -foreground gray"
                    $mytoplevel.results tag configure $tag -foreground gray
                }
                # have to decode the URL here because otherwise percent signs cause tcl to bug out - not sure why - scripting languages...
                $mytoplevel.results tag bind $tag <1> [list dialog_externals_search::clicked_link $mytoplevel [urldecode $URL] $title]
                incr counter
            }
        }
    } else {
        $mytoplevel.results insert end "No matching externals found. Try using the full name e.g. 'freeverb'.\n"
    }
}

# handle a clicked link
proc ::dialog_externals_search::clicked_link {mytoplevel URL title} {
    set destination "$::current_plugin_loadpath/$title"
    $mytoplevel.results delete 1.0 end
    $mytoplevel.results insert end "Commencing downloading of:\n$URL\nInto $::current_plugin_loadpath...\n"
    dialog_externals_search::download_file $URL $destination
    # Open both the destination folder and the zipfile itself
    # NOTE: in tcl 8.5 it should be possible to use the zlib interface to actually do the unzip
    pd_menucommands::menu_openfile $::current_plugin_loadpath
    pd_menucommands::menu_openfile $destination
    # destroy $mytoplevel
    $mytoplevel.results insert end "1. Unzip $destination.\n"
    $mytoplevel.results insert end "2. Copy the files into $::current_plugin_loadpath.\n\n"
}

# download a file to a location
# http://wiki.tcl.tk/15303
proc ::dialog_externals_search::download_file {URL outputfilename} {
    set f [open $outputfilename w]
    set status ""
    set errorstatus ""
    fconfigure $f -translation binary
    set httpresult [http::geturl $URL -binary true -progress "dialog_externals_search::download_progress" -channel $f]
    set status [::http::status $httpresult]
    set errorstatus [::http::error $httpresult]
    flush $f
    close $f
    http::cleanup $httpresult
    return status errorstatus
}

# print the download progress to the results window
proc ::dialog_externals_search::download_progress {token total current} {
    variable mytoplevelref
    set computed [expr {round(100 * (1.0 * $current / $total))}]
    $mytoplevelref.results insert end "= $computed%\n"
}

# test for platform match with our current platform
proc ::dialog_externals_search::architecture_match {title} {
    # if the word size doesn't match, return false
    if {![regexp "$::tcl_platform(bits)bit" $title]} {
        return 0
    }
    # see if the exact architecture string matches
    if {[regexp "$::tcl_platform(machine)" $title]} {
        return 1
    }
    # see if any substitute architectures match
    foreach arch $::architecture_substitutes($::tcl_platform(machine)) {
        if {[regexp "$arch" $title]} {
            return 1
        }
    }
    return 0
}

# make a remote HTTP call and parse and display the results
proc ::dialog_externals_search::search_for {term} {
    set searchresults [list]
    set token [http::geturl "http://puredata.info/search_rss?SearchableText=$term+xtrnl+.zip&portal_type%3Alist=IAEMFile"]
    set contents [http::data $token]
    set splitCont [split $contents "\n"]
    # loop through the resulting XML parsing out entries containing results with a regular expression
    foreach ele $splitCont {
        if {[regexp {<title>(.*?)</title>(.*?)<link>(.*?)</link>(.*?)<dc:creator>(.*?)</dc:creator>(.*?)<dc:date>(.*?)</dc:date>} $ele -> title junk URL junk creator junk date]} {
            set result [list $title $URL $creator $date]
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
$mymenu insert $inserthere command -label [_ "Find externals"] -command {::dialog_externals_search::open_searchui .externals_searchui}
# bind all <$::modifier-Key-s> {::dialog_externals_search::open_helpbrowser .helpbrowser2}

# http://rosettacode.org/wiki/URL_decoding#Tcl
proc urldecode {str} {
    set specialMap {"[" "%5B" "]" "%5D"}
    set seqRE {%([0-9a-fA-F]{2})}
    set replacement {[format "%c" [scan "\1" "%2x"]]}
    set modStr [regsub -all $seqRE [string map $specialMap $str] $replacement]
    return [encoding convertfrom utf-8 [subst -nobackslash -novariable $modStr]]
}
