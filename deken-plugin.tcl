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

    frame $mytoplevel.frame
    pack $mytoplevel.frame -side top -fill both
    # build_references
    # make_rootlistbox .help_browser.frame
    # search_for "" $mytoplevel.f.resultstext
}

proc ::dialog_externals_search::initiate_search {mytoplevel} {
    set results [search_for [$mytoplevel.entry get]]
    parray results
}

# make a remote HTTP call and parse and display the results
proc ::dialog_externals_search::search_for {term} {
    ::pdwindow::post "Searching for externals...\n"
    set token [http::geturl "http://puredata.info/search_rss?SearchableText=$term+xtrnl+.zip&portal_type%3Alist=IAEMFile"]
    set contents [http::data $token]
    set splitCont [split $contents "\n"]
    foreach ele $splitCont {
        if {[regexp {<title>(.*?)</title>(.*?)<link>(.*?)</link>(.*?)<dc:creator>(.*?)</dc:creator>(.*?)<dc:date>(.*?)</dc:date>} $ele -> title junk link junk creator junk date]} {
            ::pdwindow::post "\n$title\n"
            ::pdwindow::post "\t$link\n"
            ::pdwindow::post "$creator $date\n"
            # $destination insert end "$title" hello
        }
    }
    http::cleanup $token
    ::pdwindow::post "Done.\n"
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

