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
pdwindow::post "Platform: $tcl_platform(os)-$tcl_platform(machine)-$tcl_platform(bits)bit\n"

namespace eval ::dialog_externals_search:: {
    variable searchfont [list {DejaVu Sans}]
}

# this function gets called when the menu is clicked
proc ::dialog_externals_search::open_searchui {mytoplevel} {
    #if {[winfo exists $mytoplevel]} {
    #    wm deiconify $mytoplevel
    #    raise $mytoplevel
    #} else {
    #    create_dialog $mytoplevel
    #}
    search_for "" $mytoplevel.f.resultstext
}

# build the externals search dialog window
proc ::dialog_externals_search::create_dialog {mytoplevel} {
    # much of this is lifted from Jonathan's search plugin.
    variable searchfont
    toplevel $mytoplevel -class DialogWindow
    wm title $mytoplevel [_ "Finding Pd externals on puredata.info"]
    wm geometry $mytoplevel 670x550+0+30
    wm minsize $mytoplevel 230 360
    ttk::style configure Entry.TCombobox -arrowsize 0
    ttk::style configure Genre.TCombobox
    ttk::style configure Search.TButton -font menufont
    ttk::style configure Search.TCheckbutton -font menufont
    
    ttk::frame $mytoplevel.f -padding 3
    ttk::combobox $mytoplevel.f.searchtextentry -textvar ::dialog_externals_search::searchtext -font "$searchfont 12" -style "Entry.TCombobox" -cursor "xterm"
    ttk::button $mytoplevel.f.searchbutton -text [_ "Search"] -takefocus 0 -command ::dialog_helpbrowser2::search -style Search.TButton
    text $mytoplevel.resultstext -yscrollcommand "$mytoplevel.yscrollbar set" -bg white -highlightcolor blue -height 30 -wrap word -state disabled -padx 8 -pady 3 -spacing3 2 -bd 0 -highlightthickness 0 -fg black
    ttk::scrollbar $mytoplevel.yscrollbar -command "$mytoplevel.resultstext yview" -takefocus 0
    
    grid $mytoplevel.f.searchtextentry -column 0 -columnspan 3 -row 0 -padx 3 -pady 2 -sticky ew
    grid $mytoplevel.f.searchbutton -column 3 -columnspan 2 -row 0 -padx 3 -sticky ew
    grid $mytoplevel.resultstext -column 0 -columnspan 4 -row 3 -sticky nsew -ipady 0 -pady 0
    grid $mytoplevel.yscrollbar -column 4 -row 3 -sticky nsew
    
    grid columnconfigure $mytoplevel.f 0 -weight 0
    grid columnconfigure $mytoplevel.f 1 -weight 0
    grid columnconfigure $mytoplevel.f 2 -weight 1
    grid columnconfigure $mytoplevel.f 3 -weight 0
    grid columnconfigure $mytoplevel 0 -weight 1
    grid columnconfigure $mytoplevel 4 -weight 0
    grid rowconfigure    $mytoplevel 2 -weight 0
    grid rowconfigure    $mytoplevel 3 -weight 1
    
    $mytoplevel.f.searchtextentry insert 0 [_ "Pd external name..."]
    $mytoplevel.f.searchtextentry selection range 0 end
    focus $mytoplevel.f.searchtextentry
    # ::dialog_externals_search::intro $mytoplevel.resultstext
    search_for "" $mytoplevel.f.resultstext
}

# make a remote HTTP call and parse and display the results
proc ::dialog_externals_search::search_for {term destination} {
    ::pdwindow::post "Searching for externals...\n"
    set token [http::geturl "http://puredata.info/search_rss?SearchableText=xtrnl-"]
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

