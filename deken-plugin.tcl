# META NAME PdExternalsSearch
# META DESCRIPTION Search for externals zipfiles on puredata.info
# META AUTHOR <Chris McCormick> chris@mccormick.cx

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
package require pdwindow 0.1
package require http 2

# console message to let them know we're loaded
pdwindow::post  "deken-plugin.tcl (Pd externals search) in $::current_plugin_loadpath loaded.\n"
set ::tcl_platform(bits) [ expr [ string length [ format %X -1 ] ] * 4 ]
pdwindow::post "Platform detected: $tcl_platform(os)-$tcl_platform(machine)-$tcl_platform(bits)bit\n"

namespace eval ::dialog_externals_search:: {
    namespace export open_searchui
}

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
    wm title $mytoplevel [_ "Find externals"]
    wm geometry $mytoplevel 670x550+0+30
    wm minsize $mytoplevel 230 360
    wm transient $mytoplevel
    $mytoplevel configure -padx 10 -pady 5

    if {$::windowingsystem eq "aqua"} {
        $mytoplevel configure -menu $::dialog_menubar
    }

    entry $mytoplevel.entry -font 18 -relief sunken \
        -highlightthickness 1 -highlightcolor blue
    pack $mytoplevel.entry -side top -padx 10 -fill x

    button $mytoplevel.button -text [_ "Search"] -default active -width 9 \
        -command "dialog_externals_search::initiate_search $mytoplevel"
    pack $mytoplevel.button -side top  -padx 6 -pady 3 -fill x

    text $mytoplevel.results -takefocus 0
    pack $mytoplevel.results -side top -padx 6 -pady 3 -fill x

}

proc ::dialog_externals_search::initiate_search {mytoplevel} {
    # let the user know what we're doing
    $mytoplevel.results delete 1.0 end
    $mytoplevel.results insert end "Searching for externals...\n"
    # make the ajax call
    set results [search_for [$mytoplevel.entry get]]
    # sort the results by reverse date - latest upload first
    set sorted [lsort -index 3 $results]
    for {set i [llength $sorted]} {[incr i -1] >= 0} {} {lappend reversed [lindex $sorted $i]}
    # delete all text in the results
    $mytoplevel.results delete 1.0 end
    set counter 0
    # build the list UI of results
    foreach r $reversed {
        # $current_listbox insert end "$creator - $date\n\t$title\n\t$URL"
        foreach {title URL creator date} $r {break}
        set tag ch$counter
        $mytoplevel.results insert end "$title\n\tBy $creator - $date\n\n" $tag
        $mytoplevel.results tag bind $tag <Enter> "$mytoplevel.results tag configure $tag -foreground blue"
        $mytoplevel.results tag bind $tag <Leave> "$mytoplevel.results tag configure $tag -foreground black"
        $mytoplevel.results tag bind $tag <1> [list "dialog_externals_search::clicked_link" $mytoplevel $URL $title]
        incr counter
    }
}

# handle a clicked link
proc ::dialog_externals_search::clicked_link {mytoplevel URL title} {
    $mytoplevel.results delete 1.0 end
    $mytoplevel.results insert end "Unzip $title\n"
    $mytoplevel.results insert end "Copy the files into $::current_plugin_loadpath.\n"
    $mytoplevel.results insert end "Downloading:\n$URL\nInto $::current_plugin_loadpath...\n"
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

