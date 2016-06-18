# META NAME PdExternalsSearch
# META DESCRIPTION Search for externals zipfiles on puredata.info
# META AUTHOR <Chris McCormick> chris@mccormick.cx
# META AUTHOR <IOhannes m zmölnig> zmoelnig@iem.at
# ex: set setl sw=2 sts=2 et

# Search URL:
# http://deken.puredata.info/search?name=foobar

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
    variable version
}

## only register this plugin if there isn't any newer version already registered
## (if ::deken::version is defined and is higher than our own version)
proc ::deken::versioncheck {version} {
    if { [info exists ::deken::version ] } {
        set v0 [split $::deken::version "."]
        set v1 [split $version "."]
        foreach x $v0 y $v1 {
            if { $x > $y } {
                ::pdwindow::debug "\[deken\]: installed version \[$::deken::version\] > $version...skipping!\n"
                return 0
            }
            if { $x < $y } {
                ::pdwindow::debug "\[deken\]: installed version \[$::deken::version\] < $version...overwriting!\n"
                set ::deken::version $version
                return 1
            }
        }
        ::pdwindow::debug "\[deken\]: installed version \[$::deken::version\] == $version...skipping!\n"
        return 0
    }
    set ::deken::version $version
    return 1
}

## put the current version of this package here:
if { [::deken::versioncheck 0.2.1] } {

namespace eval ::deken:: {
    namespace export open_searchui
    variable mytoplevelref
    variable platform
    variable architecture_substitutes
    variable installpath
    variable statustext
    variable statustimer
    variable backends
    namespace export register
}
namespace eval ::deken::search:: { }

set ::deken::statustimer ""

set ::deken::backends [list]
proc ::deken::register {fun} {
    set ::deken::backends [linsert $::deken::backends 0 $fun]
}

proc ::deken::gettmpdir {} {
    proc _iswdir {d} { expr [file isdirectory $d] * [file writable $d] }
    set tmpdir ""
    catch {set tmpdir $::env(TRASH_FOLDER)} ;# very old Macintosh. Mac OS X doesn't have this.
    if {[_iswdir $tmpdir]} {return $tmpdir}
    catch {set tmpdir $::env(TMP)}
    if {[_iswdir $tmpdir]} {return $tmpdir}
    catch {set tmpdir $::env(TEMP)}
    if {[_iswdir $tmpdir]} {return $tmpdir}
    set tmpdir "/tmp"
    set tmpdir [pwd]
    if {[_iswdir $tmpdir]} {return $tmpdir}
}
proc ::deken::vbs_unzipper {zipfile {path .}} {
    ## this is w32 only
    if { "Windows" eq "$::deken::platform(os)" } { } { return 0 }
    if { "" eq $::deken::_vbsunzip } {
        set ::deken::_vbsunzip [ file join [::deken::gettmpdir] unzip.vbs ]
    }

    if {[file exists $::deken::_vbsunzip]} {} {
        ## no script yet, create one
        set script {
Set fso = CreateObject("Scripting.FileSystemObject")

'The location of the zip file.
ZipFile = fso.GetAbsolutePathName(WScript.Arguments.Item(0))
'The folder the contents should be extracted to.
ExtractTo = fso.GetAbsolutePathName(WScript.Arguments.Item(1))

'If the extraction location does not exist create it.
If NOT fso.FolderExists(ExtractTo) Then
   fso.CreateFolder(ExtractTo)
End If

'Extract the contants of the zip file.
set objShell = CreateObject("Shell.Application")
set FilesInZip=objShell.NameSpace(ZipFile).items
objShell.NameSpace(ExtractTo).CopyHere(FilesInZip)
Set fso = Nothing
Set objShell = Nothing
}
        if {![catch {set fileId [open $::deken::_vbsunzip "w"]}]} {
            puts $fileId $script
            close $fileId
        }
    }
    if {[file exists $::deken::_vbsunzip]} {} {
        ## still no script, give up
        return 0
    }
    ## try to call the script
    if { [ catch { exec cscript $::deken::_vbsunzip $zipfile .} stdout ] } {
        ::pdwindow::debug "\[deken\] VBS-unzip: $::deken::_vbsunzip\n$stdout\n"
        return 0
    }
    return 1
}
set ::deken::_vbsunzip ""

proc ::deken::get_writable_dir {paths} {
    set fs [file separator]
    set access [list RDWR CREAT EXCL TRUNC]
    foreach p $paths {
        #if { [ catch { file mkdir $p } ] } {}
        for {set i 0} {True} {incr i} {
            set tmpfile "${p}${fs}dekentmp.${i}"
            if {![file exists $tmpfile]} {
                break
            }
        }
        # try creating tmpfile
        if {![catch {open $tmpfile $access} channel]} {
            close $channel
            file delete $tmpfile
            return $p
        }
    }
    return
}

# list-reverter (compat for tcl<8.5)
if {[info command lreverse] == ""} {
    proc lreverse list {
        set res {}
        set i [llength $list]
        while {$i} {
            lappend res [lindex $list [incr i -1]]
        }
        set res
    } ;# RS
}


## where to look for the config-files:
## - near the deken-plugin.tcl file
## + at some user-specific place (e.g. ~/pd-externals/deken-plugin/)
## it's probably easiest to iterate through curdir and ::sys_staticpath (in reverse order)
## and read all (existing) configurations
#
## the configfile's format is simple:
# the first element of a line is the variable name, the rest the value. e.g.
#    foo bar baz
# will create a variable '::deken::foo' with value [list bar baz]
## LATER: this is rather insecure, as it allows people to overwrite
##        virtually everything... (e.g. ::deken::search_for)
proc ::deken::readconfig {paths filename} {
    proc doreadconfig {fname} {
        if {[file exists $fname]} {
            set fp [open $fname r]
            while {![eof $fp]} {
                set data [gets $fp]
                if { [string is list $data ] } {
                    if { [llength $data ] > 1 } {
                        set ::deken::[lindex $data 0] [lrange $data 1 end]
                    }
                }
            }
            return True
        }
        return False
    }
    set fs [file separator]
    doreadconfig "$::current_plugin_loadpath${fs}${filename}"
    foreach p0 [lreverse $paths] {
        foreach p1 [ list "" "${fs}deken-plugin" ] {
            doreadconfig "${p0}${p1}${fs}${filename}"
        }
    }
}


::deken::readconfig $::sys_staticpath deken-plugin.conf

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
    if { "intel" eq "$::deken::platform(machine)" } { set ::deken::platform(machine) "i686" }
    # ... and all 64bit CPUs are manufactured by amd
    #if { "amd64" eq "$::deken::platform(machine)" } { set ::deken::platform(machine) "x86_64" }
}

# console message to let them know we're loaded
## but only if we are being called as a plugin (not as built-in)
if { "" != "$::current_plugin_loadpath" } {
    ::pdwindow::post "deken-plugin.tcl (Pd externals search) in $::current_plugin_loadpath loaded.\n"
    ::pdwindow::post "Platform detected: $::deken::platform(os)-$::deken::platform(machine)-$::deken::platform(bits)bit\n"
}

# architectures that can be substituted for eachother
array set ::deken::architecture_substitutes {}
set ::deken::architecture_substitutes(x86_64) [list "amd64" "i386" "i586" "i686"]
set ::deken::architecture_substitutes(amd64) [list "x86_64" "i386" "i586" "i686"]
set ::deken::architecture_substitutes(i686) [list "i586" "i386"]
set ::deken::architecture_substitutes(i586) [list "i386"]
set ::deken::architecture_substitutes(armv6l) [list "armv6" "arm"]
set ::deken::architecture_substitutes(armv7l) [list "armv7" "armv6l" "armv6" "arm"]
set ::deken::architecture_substitutes(PowerPC) [list "ppc"]
set ::deken::architecture_substitutes(ppc) [list "PowerPC"]

proc ::deken::status {msg} {
    #variable mytoplevelref
    #$mytoplevelref.results insert end "$msg\n"
    #$mytoplevelref.status.label -text "$msg"
    after cancel $::deken::statustimer
    if {"" ne $msg} {
        set ::deken::statustext "STATUS: $msg"
        set ::deken::statustimer [after 5000 [list set ::deken::statustext ""]]
    } {
        set ::deken::statustext ""
    }
}
proc ::deken::scrollup {} {
    variable mytoplevelref
    $mytoplevelref.results see 0.0
}
proc ::deken::post {msg {tag ""}} {
    variable mytoplevelref
    $mytoplevelref.results insert end "$msg\n" $tag
    $mytoplevelref.results see end
}
proc ::deken::clearpost {} {
    variable mytoplevelref
    $mytoplevelref.results delete 1.0 end
}
proc ::deken::bind_posttag {tag key cmd} {
    variable mytoplevelref
    $mytoplevelref.results tag bind $tag $key $cmd
}
proc ::deken::highlightable_posttag {tag} {
    variable mytoplevelref
    ::deken::bind_posttag $tag <Enter> \
        "$mytoplevelref.results tag add highlight [ $mytoplevelref.results tag ranges $tag ]"
    ::deken::bind_posttag $tag <Leave> \
        "$mytoplevelref.results tag remove highlight [ $mytoplevelref.results tag ranges $tag ]"
    # make sure that the 'highlight' tag is topmost
    $mytoplevelref.results tag raise highlight
}
proc ::deken::prompt_installdir {} {
    set installdir [tk_chooseDirectory -title "Install libraries to directory:"]
    if { "$installdir" != "" } {
        set ::deken::installpath $installdir
        return 1
    }
    return 0
}


proc ::deken::update_searchbutton {mytoplevel} {
    if { [$mytoplevel.searchbit.entry get] == "" } {
        $mytoplevel.searchbit.button configure -text [_ "Show all" ]
    } {
        $mytoplevel.searchbit.button configure -text [_ "Search" ]
    }
}

# this function gets called when the menu is clicked
proc ::deken::open_searchui {mytoplevel} {
    if {[winfo exists $mytoplevel]} {
        wm deiconify $mytoplevel
        raise $mytoplevel
    } else {
        ::deken::create_dialog $mytoplevel
        $mytoplevel.results tag configure error -foreground red
        $mytoplevel.results tag configure warn -foreground orange
        $mytoplevel.results tag configure info -foreground grey
        $mytoplevel.results tag configure highlight -foreground blue
        $mytoplevel.results tag configure archmatch
        $mytoplevel.results tag configure noarchmatch -foreground grey
    }
    ::deken::post "To get a list of all available externals, try an empty search." info
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
    bind $mytoplevel.searchbit.entry <KeyRelease> "::deken::update_searchbutton $mytoplevel"
    focus $mytoplevel.searchbit.entry
    button $mytoplevel.searchbit.button -text [_ "Show all"] -default active -width 9 -command "::deken::initiate_search $mytoplevel"
    pack $mytoplevel.searchbit.button -side right -padx 6 -pady 3

    frame $mytoplevel.warning
    pack $mytoplevel.warning -side top -fill x
    label $mytoplevel.warning.label -text "Only install externals uploaded by people you trust."
    pack $mytoplevel.warning.label -side left -padx 6

    frame $mytoplevel.status
    pack $mytoplevel.status -side bottom -fill x
    label $mytoplevel.status.label -textvariable ::deken::statustext
    pack $mytoplevel.status.label -side left -padx 6

    text $mytoplevel.results -takefocus 0 -cursor hand2 -height 100 -yscrollcommand "$mytoplevel.results.ys set"
    scrollbar $mytoplevel.results.ys -orient vertical -command "$mytoplevel.results yview"
    pack $mytoplevel.results.ys -side right -fill y
    pack $mytoplevel.results -side left -padx 6 -pady 3 -fill both -expand true
}

proc ::deken::initiate_search {mytoplevel} {
    # let the user know what we're doing
    ::deken::clearpost
    ::deken::post "Searching for externals..."
    # make the ajax call
    if { [ catch {
        set results [::deken::search_for [$mytoplevel.searchbit.entry get]]
    } stdout ] } {
        ::pdwindow::debug "\[deken\]: online? $stdout\n"
        ::deken::status "Unable to perform search. Are you online?"
    } else {
    # delete all text in the results
    ::deken::clearpost
    if {[llength $results] != 0} {
        set counter 0
        # build the list UI of results
        foreach r $results {
            ::deken::show_result $mytoplevel $counter $r 1
            incr counter
        }
        foreach r $results {
            ::deken::show_result $mytoplevel $counter $r 0
            incr counter
        }
	::deken::scrollup
    } else {
        ::deken::post "No matching externals found. Try using the full name e.g. 'freeverb'."
    }
}}

# display a single found entry
proc ::deken::show_result {mytoplevel counter result showmatches} {
            foreach {title cmd match comment status} $result {break}

            set tag ch$counter
    #if { [ ($match) ] } { set matchtag archmatch } { set matchtag noarchmatch }
            set matchtag [expr $match?"archmatch":"noarchmatch" ]
            if {($match == $showmatches)} {
                set comment [string map {"\n" "\n\t"} $comment]
                ::deken::post "$title\n\t$comment\n" [list $tag $matchtag]
                ::deken::highlightable_posttag $tag
                ::deken::bind_posttag $tag <Enter> "+::deken::status $status"
                ::deken::bind_posttag $tag <1> "$cmd"
            }
}

# handle a clicked link
proc ::deken::clicked_link {URL filename} {
    ## make sure that the destination path exists
    ### if ::deken::installpath is set, use the first writable item
    ### if not, get a writable item from one of the searchpaths
    ### if this still doesn't help, ask the user
    set installdir ""
    if { [ info exists ::deken::installpath ] } {
        ## any previous choice?
        set installdir [ ::deken::get_writable_dir [list $::deken::installpath ] ]
    }
    if { "$installdir" == "" } {
        ## search the default paths
        set installdir [ ::deken::get_writable_dir $::sys_staticpath ]
    }
    if { "$installdir" == "" } {
        ## ask the user (and remember the decision)
        ::deken::prompt_installdir
        set installdir [ ::deken::get_writable_dir [list $::deken::installpath ] ]
    }
    if { "$installdir" == "" } {
        #::deken::clearpost
        ::deken::post "No writeable directory found in:" warn
        foreach p $::sys_staticpath { ::deken::post "\t- $p" warn }
        ::deken::post "Cannot download/install libraries!" warn
        return
    }
    switch -- [tk_messageBox -message \
                   "Install to directory $installdir?" \
                   -type yesnocancel -default "yes" \
                   -icon question] {
                       no {set installdir ""
                           if {[::deken::prompt_installdir]} {
                               set installdir [ ::deken::get_writable_dir [list $::deken::installpath ] ] }
                           if { "$installdir" eq "" } return}
                       cancel return}

    set fullpkgfile "$installdir/$filename"
    ::deken::clearpost
    ::deken::post "Commencing downloading of:\n$URL\nInto $installdir..."
    ::deken::download_file $URL $fullpkgfile
    set PWD [ pwd ]
    cd $installdir
    set success 1
    if { [ string match *.zip $fullpkgfile ] } then {
        if { [ ::deken::vbs_unzipper $fullpkgfile  $installdir ] } { } {
            if { [ catch { exec unzip -uo $fullpkgfile } stdout ] } {
                ::pdwindow::debug "$stdout\n"
                set success 0
            }
        }
    } elseif  { [ string match *.tar.gz $fullpkgfile ]
                || [ string match *.tgz $fullpkgfile ]
              } then {
        if { [ catch { exec tar xzf $fullpkgfile } stdout ] } {
            ::pdwindow::debug "$stdout\n"
            set success 0
        }
    }
    cd $PWD
    if { $success > 0 } {
        ::deken::post "Successfully unzipped $filename into $installdir.\n"
        catch { exec rm $fullpkgfile }
    } else {
        # Open both the fullpkgfile folder and the zipfile itself
        # NOTE: in tcl 8.6 it should be possible to use the zlib interface to actually do the unzip
        ::deken::post "Unable to extract package automatically." warn
        ::deken::post "Please perform the following steps manually:"
        ::deken::post "1. Unzip $fullpkgfile."
        pd_menucommands::menu_openfile $fullpkgfile
        ::deken::post "2. Copy the contents into $installdir.\n"
        pd_menucommands::menu_openfile $installdir
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
        ::deken::post "= $computed%"
    }
}

# parse a deken-packagefilename into it's components: <pkgname>[-v<version>-]?{(<arch>)}-externals.zip
# return: list <pkgname> <version> [list <arch> ...]
proc ::deken::parse_filename {filename} {
    set pkgname $filename
    set archs [list]
    set version ""
    if { [ regexp {(.*)-externals\..*} $filename _ basename] } {
        set pkgname $basename
        # basename <pkgname>[-v<version>-]?{(<arch>)}
        ## strip off the archs
        set baselist [split $basename () ]

        # get pkgname + version
        set pkgver [lindex $baselist 0]
        if { ! [ regexp "(.*)-(.*)-" $pkgver _ pkgname version ] } {
            set pkgname $pkgver
            set $version ""
        }
        # get archs
        foreach {a _} [lreplace $baselist 0 0] { lappend archs $a }
    }
    return [list $pkgname $version $archs]
}

# test for platform match with our current platform
proc ::deken::architecture_match {archs} {
    # if there are no architecture sections this must be arch-independent
    if { ! [llength $archs] } { return 1}

    # check each architecture in our list against the current one
    foreach arch $archs {
        if { [ regexp -- {(.*)-(.*)-(.*)} $arch _ os machine bits ] } {
            if { "${os}" eq "$::deken::platform(os)" &&
                 "${bits}" eq "$::deken::platform(bits)"
             } {
                ## so OS and word size match
                ## check whether the CPU matches as well
                if { "${machine}" eq "$::deken::platform(machine)" } {return 1}
                ## not exactly; see whether it is in the list of compat CPUs
                if {[llength [array names ::deken::architecture_substitutes -exact $::deken::platform(machine)]]} {
                    foreach cpu $::deken::architecture_substitutes($::deken::platform(machine)) {
                        if { "${machine}" eq "${cpu}" } {return 1}
                    }
                }
            }
        }
    }
    return 0
}

proc ::deken::search_for {term} {
    ::deken::status "searching for '$term'"

    set result [list]
    foreach searcher $::deken::backends {
        set result [concat $result [ $searcher $term ] ]
    }
    return $result
}

# create an entry for our search in the "help" menu (or re-use an existing one)
set mymenu .menubar.help
if { [catch {
    $mymenu entryconfigure [_ "Find externals"] -command {::deken::open_searchui .externals_searchui}
} _ ] } {
    $mymenu add command -label [_ "Find externals"] -command {::deken::open_searchui .externals_searchui}
}
# bind all <$::modifier-Key-s> {::deken::open_helpbrowser .helpbrowser2}

# http://rosettacode.org/wiki/URL_decoding#Tcl
proc urldecode {str} {
    set specialMap {"[" "%5B" "]" "%5D"}
    set seqRE {%([0-9a-fA-F]{2})}
    set replacement {[format "%c" [scan "\1" "%2x"]]}
    set modStr [regsub -all $seqRE [string map $specialMap $str] $replacement]
    return [encoding convertfrom utf-8 [subst -nobackslash -novariable $modStr]]
}


# ####################################################################
# search backends
# ####################################################################

## API draft

# each backend is implemented via a single proc
## that takes a single argument "term", the term to search fo
## an empty term indicates "search for all"
# the backend then returns a list of results
## each result is a list of the following elements:
##   <title> <cmd> <match> <comment> <status>
## title: the primary name to display
##        (the user will select the element by this name)
##        e.g. "frobscottle-1.10 (Linux/amd64)"
## cmd  : a command that will install the selected library
##        e.g. "[list ::deken::clicked_link http://bfg.org/frobscottle-1.10.zip frobscottle-1.10.zip]"
## match: an integer indicating whether this entry is actually usable
##        on this host (1) or not (0)
## comment: secondary line to display
##        e.g. "uploaded by the BFG in 1982"
## status: line to display in the status-line
##        e.g. "http://bfg.org/frobscottle-1.10.zip"
# note on sorting:
## the results ought to be sorted with most up-to-date first
##  (filtering based on architecture-matches should be ignored when sorting!)
# note on helper-functions:
## you can put whatever you like into <cmd>, even your own proc


# registration
## to register a new search function, call `::deken::register $myfun`

# namespace
## you are welcome to use the ::deken::search:: namespace



## ####################################################################
## searching puredata.info
proc ::deken::search::puredata.info {term} {
    set searchresults [list]

    set term [ join $term "&name=" ]
    set token [http::geturl "http://deken.puredata.info/search?name=$term"]
    set contents [http::data $token]
    set splitCont [split $contents "\n"]
    # loop through the resulting tab-delimited table
    foreach ele $splitCont {
        set ele [ string trim $ele ]
        if { "" ne $ele } {
            set sele [ split $ele "\t" ]

            set name  [ string trim [ lindex $sele 0 ]]
            set URL   [ string trim [ lindex $sele 1 ]]
            set creator [ string trim [ lindex $sele 2 ]]
            set date    [regsub -all {[TZ]} [ string trim [ lindex $sele 3 ] ] { }]
            set decURL [urldecode $URL]
            set filename [ file tail $URL ]
            set cmd [list ::deken::clicked_link $decURL $filename]
            set pkgverarch [ ::deken::parse_filename $filename ]
            set archs [lindex $pkgverarch 2]

            set match [::deken::architecture_match "$archs" ]

            set comment "Uploaded by $creator @ $date"
            set status $URL
            set sortname [lindex $pkgverarch 0]--[lindex $pkgverarch 1]--$date
            set res [list $name $cmd $match $comment $status $filename]
            lappend searchresults $res
        }
    }
    http::cleanup $token
    return [lsort -dictionary -decreasing -index 5 $searchresults ]
}

::deken::register ::deken::search::puredata.info
}
