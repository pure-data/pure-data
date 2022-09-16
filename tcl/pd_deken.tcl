# META NAME PdExternalsSearch
# META DESCRIPTION Search for externals zipfiles on puredata.info
# META AUTHOR <Chris McCormick> chris@mccormick.cx
# META AUTHOR <IOhannes m zmölnig> zmoelnig@iem.at
# ex: set setl sw=2 sts=2 et

# Search URL:
# http://deken.puredata.info/search?name=foobar

# TODOs
## + open embedded README
## - open README on homepage (aka "More info...")
## + remove library before unzipping
## + only show valid arch
## - only show most recent version (of each arch)
## - check whether the "cd" thing during unzip works on w32 and multiple drives
## - redirect ::deken::post to ::pdwindow::post (that is: use the results pane only for results)
## + make the "add to path" thingy configurable

# The minimum version of TCL that allows the plugin to run
package require Tcl 8.4
# If Tk or Ttk is needed
#package require Ttk
# Any elements of the Pd GUI that are required
# + require everything and all your script needs.
#   If a requirement is missing,
#   Pd will load, but the script will not.

package require http 2
# try enabling https if possible
if { [catch {package require tls} ] } {} else {
    ::tls::init -ssl2 false -ssl3 false -tls1 true
    ::http::register https 443 ::tls::socket
}
# try enabling PROXY support if possible
if { [catch {package require autoproxy} ] } {} else {
    ::autoproxy::init
    if { ! [catch {package present tls} stdout] } {
        ::http::register https 443 ::autoproxy::tls_socket
    }
}

package require pdwindow 0.1
package require pd_menucommands 0.1
package require pd_guiprefs

namespace eval ::deken:: {
    variable version

    variable installpath
    variable userplatform
    variable hideforeignarch
    variable hideoldversions

    # whether to use http:// or https://
    variable protocol

    # results
    variable results
    variable selected {}
}

namespace eval ::deken::preferences {
    variable installpath
    variable userinstallpath
    # automatically detected platform
    variable platform
    # user specified platform
    variable userplatform
    # boolean whether non-matching archs should be hidden
    variable hideforeignarch
    variable hideoldversions
}
namespace eval ::deken::utilities { }


## only register this plugin if there isn't any newer version already registered
## (if ::deken::version is defined and is higher than our own version)
proc ::deken::versioncheck {version} {
    if { [info exists ::deken::version ] } {
        set v0 [split $::deken::version "."]
        set v1 [split $version "."]
        foreach x $v0 y $v1 {
            if { $x > $y } {
                set msg [format [_ "\[deken\] installed version \[%1\$s\] > %2\$s...skipping!" ] $::deken::version $version ]
                ::pdwindow::debug "${msg}\n"
                return 0
            }
            if { $x < $y } {
                set msg [format [_ "\[deken\] installed version \[%1\$s\] < %2\$s...overwriting!" ] $::deken::version $version ]
                ::pdwindow::debug "$msg\n"
                set ::deken::version $version
                return 1
            }
        }
        set msg [format [_ "\[deken\] installed version \[%1\$s\] == %2\$s...skipping!" ] $::deken::version $version ]
        ::pdwindow::debug "${msg}\n"
        return 0
    }
    set ::deken::version $version
    return 1
}

## put the current version of this package here:
if { [::deken::versioncheck 0.8.0] } {

namespace eval ::deken:: {
    namespace export open_searchui
    variable winid .externals_searchui
    variable platform
    variable architecture_substitutes
    variable installpath
    variable statustext
    variable statustimer
    variable backends
    variable progressvar
    variable progresstext
    variable progresstimer
    namespace export register
}
namespace eval ::deken::search:: { }

## FIXXXXME only initialize vars if not yet set
set ::deken::backends {}
set ::deken::installpath {}
set ::deken::userplatform {}
set ::deken::hideforeignarch false
set ::deken::hideoldversions false
set ::deken::show_readme 1
set ::deken::remove_on_install 1
set ::deken::add_to_path 0
set ::deken::keep_package 0
set ::deken::verify_sha256 1
set ::deken::searchtype name
set ::deken::statustimer {}
set ::deken::progresstimer {}
set ::deken::preferences::installpath {}
set ::deken::preferences::userinstallpath {}
set ::deken::preferences::platform {}
set ::deken::preferences::userplatform {}
set ::deken::preferences::hideforeignarch {}
set ::deken::preferences::hideoldversions {}
set ::deken::preferences::show_readme {}
set ::deken::preferences::remove_on_install {}
set ::deken::preferences::add_to_path {}
set ::deken::preferences::add_to_path_temp {}
set ::deken::preferences::keep_package {}
set ::deken::preferences::verify_sha256 {}

set ::deken::platform(os) $::tcl_platform(os)
set ::deken::platform(machine) $::tcl_platform(machine)
set ::deken::platform(bits) [ expr [ string length [ format %X -1 ] ] * 4 ]
set ::deken::platform(floatsize) 32

# architectures that can be substituted for each other
array set ::deken::architecture_substitutes {}
set ::deken::architecture_substitutes(x86_64) [list "amd64" ]
set ::deken::architecture_substitutes(amd64) [list "x86_64" ]
set ::deken::architecture_substitutes(i686) [list "i586" "i386"]
set ::deken::architecture_substitutes(i586) [list "i386"]
set ::deken::architecture_substitutes(armv6) [list "armv6l" "arm"]
set ::deken::architecture_substitutes(armv6l) [list "armv6" "arm"]
set ::deken::architecture_substitutes(armv7) [list "armv7l" "armv6l" "armv6" "arm"]
set ::deken::architecture_substitutes(armv7l) [list "armv7" "armv6l" "armv6" "arm"]
set ::deken::architecture_substitutes(PowerPC) [list "ppc"]
set ::deken::architecture_substitutes(ppc) [list "PowerPC"]

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


set ::deken::protocol "http"
if { ! [catch {package present tls} stdout] } {
    set ::deken::protocol "https"
}


# ######################################################################
# ################ compatibility #######################################
# ######################################################################

# list-reverter (compat for tcl<8.5)
if {[info commands lreverse] == ""} {
    proc lreverse list {
        set res {}
        set i [llength $list]
        while {$i} {
            lappend res [lindex $list [incr i -1]]
        }
        set res
    } ;# RS
}

# ######################################################################
# ################ utilities ##########################################
# ######################################################################

proc ::deken::utilities::bool {value {fallback 0}} {
    catch {set fallback [expr bool($value) ] } stdout
    return $fallback
}

proc ::deken::utilities::tristate {value {offset 0} {fallback 0} } {
    catch {set fallback [expr (int($value) + int($offset))% 3 ]} stdout
    return $fallback
}

proc ::deken::utilities::expandpath {path} {
    set map "@PD_PATH@"
    lappend map $::sys_libdir
    string map $map $path
}

proc ::deken::utilities::get_tmpfilename {{path ""} {ext ""}} {
    for {set i 0} {true} {incr i} {
        set tmpfile [file join ${path} dekentmp.${i}${ext}]
        if {![file exists $tmpfile]} {
            return $tmpfile
        }
    }
}

proc ::deken::utilities::get_tmpdir {} {
    proc _iswdir {d} { "expr" [file isdirectory $d] * [file writable $d] }
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

proc ::deken::utilities::is_writabledir {path} {
    set fs [file separator]
    set access [list RDWR CREAT EXCL TRUNC]
    set tmpfile [::deken::utilities::get_tmpfilename $path]
    # try creating tmpfile
    if {![catch {open $tmpfile $access} channel]} {
        close $channel
        file delete $tmpfile
        return true
    }
    return false
}

proc ::deken::utilities::get_writabledir {paths} {
    foreach p $paths {
        set xp [ ::deken::utilities::expandpath $p ]
        if { [ ::deken::utilities::is_writabledir $xp ] } { return $p }
    }
    return
}

proc ::deken::utilities::rmrecursive {path} {
    # recursively remove ${path} if it exists, traversing into each directory
    # to delete single items (rather than just unlinking the parent directory)
    set errors 0
    set myname [lindex [info level 0] 0]
    set children [glob -nocomplain -directory $path -types hidden *]
    lappend children {*}[glob -nocomplain -directory $path *]
    foreach child $children[set children {}] {
        if {[file tail $child] in {. ..}} {
            continue
        }
        if {[file isdirectory $child]} {
            if {[file type $child] ne "link"} {
                incr errors [$myname $child]
            }
        }
        if { [ catch { file delete -force $child } ] } {
            incr errors
        }
    }
    return $errors
}

# http://rosettacode.org/wiki/URL_decoding#Tcl
proc ::deken::utilities::urldecode {str} {
    set specialMap {"[" "%5B" "]" "%5D"}
    set seqRE {%([0-9a-fA-F]{2})}
    set replacement {[format "%c" [scan "\1" "%2x"]]}
    set modStr [regsub -all $seqRE [string map $specialMap $str] $replacement]
    encoding convertfrom utf-8 [subst -nobackslashes -novariables $modStr]
}

proc ::deken::utilities::verbose {level message} {
    ::pdwindow::verbose ${level} "\[deken\] ${message}\n"
}
proc ::deken::utilities::debug {message} {
    ::pdwindow::debug "${message}\n"
}

if { [catch {package require tkdnd} ] } {
    proc ::deken::utilities::dnd_init {windowid} { }
} else {
proc ::deken::utilities::dnd_init {windowid} {
    ::tkdnd::drop_target register $windowid DND_Files
    bind $windowid <<Drop:DND_Files>> {::deken::utilities::dnd_drop_files %D}
}
proc ::deken::utilities::dnd_drop_files {files} {
    foreach f $files {
        if { [regexp -all -nocase "\.(zip|dek|tgz|tar\.gz)$" ${f} ] } {
            set msg [format [_ "installing deken package '%s'" ] $f]
            ::deken::status ${msg}
            ::deken::utilities::verbose -1 ${msg}
            ::deken::install_package_from_file $f
        } else {
            set msg [format [_ "ignoring '%s': doesn't look like a deken package" ] $f]
            ::deken::status ${msg}
            ::deken::utilities::verbose -1 ${msg}
        }
    }
    return "link"
}
}

if { [catch {package require zipfile::decode} ] } {
proc ::deken::utilities::unzipper {zipfile {path .}} {
    ## this is w32 only
    if {$::tcl_platform(platform) ne "windows"} { return 0 }

    ## create script-file
    set vbsscript [::deken::utilities::get_tmpfilename [::deken::utilities::get_tmpdir] ".vbs" ]
    set script {
On Error Resume Next
Set fso = CreateObject("Scripting.FileSystemObject")

'The location of the zip file.
ZipFile = fso.GetAbsolutePathName(WScript.Arguments.Item(0))
'The folder the contents should be extracted to.
ExtractTo = fso.GetAbsolutePathName(WScript.Arguments.Item(1))

'If the extraction location does not exist create it.
If NOT fso.FolderExists(ExtractTo) Then
   fso.CreateFolder(ExtractTo)
End If

'Extract the contents of the zip file.
set objShell = CreateObject("Shell.Application")
set FilesInZip=objShell.NameSpace(ZipFile).items
objShell.NameSpace(ExtractTo).CopyHere(FilesInZip)
'In case of an error, exit
If Err.Number <> 0 Then
  Err.Clear
  WScript.Quit 1
End If

Set fso = Nothing
Set objShell = Nothing
    }
    if {![catch {set fileId [open $vbsscript "w"]}]} {
        puts $fileId $script
        close $fileId
    }

    if {![file exists $vbsscript]} {
        ## still no script, give up
        return 0
    }
    ## try to call the script
    ## (and windows requires the file to have a .zip extension!!!)
    if { [ catch {
	set zipfilezip ${zipfile}.zip
	file rename ${zipfile} ${zipfilezip}
	exec cscript "${vbsscript}" "${zipfilezip}" .
	file rename ${zipfilezip} ${zipfile}
    } stdout ] } {
        catch { file rename ${zipfilezip} ${zipfile} }
        catch { file delete "${vbsscript}" }
        ::deken::utilities::debug "\[deken\] VBS-unzip($vbsscript): $stdout"
        return 0
    }
    catch { file delete "${vbsscript}" }
    return 1
}
} else { # successfully imported zipfile::decode
proc ::deken::utilities::unzipper {zipfile {path .}} {
    if { [catch {
        ::zipfile::decode::unzipfile "${zipfile}" "${path}"
    } stdout ] } {
        ::deken::utilities::debug "\[deken\] unzip: $stdout"
        return 0
    }
    return 1
}
}

proc ::deken::utilities::extract {installdir filename fullpkgfile {keep_package 1}} {
    if { ! [ file isdirectory "${installdir}" ] } {
        return 0
    }
    ::deken::status [format [_ "Installing %s" ] $filename ]
    set PWD [ pwd ]
    cd $installdir
    set success 1
    if { [ string match *.dek $fullpkgfile ] } then {
        if { ! [ ::deken::utilities::unzipper $fullpkgfile  $installdir ] } {
            if { [ catch { exec unzip -uo $fullpkgfile } stdout ] } {
                ::deken::utilities::debug "$stdout"
                set success 0
            }
        }
    } elseif { [ string match *.zip $fullpkgfile ] } then {
        if { ! [ ::deken::utilities::unzipper $fullpkgfile  $installdir ] } {
            if { [ catch { exec unzip -uo $fullpkgfile } stdout ] } {
                ::deken::utilities::debug "$stdout"
                set success 0
            }
        }
    } elseif { [ string match *.tar.* $fullpkgfile ]
                || [ string match *.tgz $fullpkgfile ]
              } then {
        if { [ catch { exec tar xf $fullpkgfile } stdout ] } {
            ::deken::utilities::debug "$stdout"
            set success 0
        }
    }
    cd $PWD

    if { $success > 0 } {
        ::deken::utilities::debug [format [_ "\[deken\] Successfully unzipped %1\$s into %2\$s."] $filename $installdir ]
        if { ! "${keep_package}" } {
            catch { file delete $fullpkgfile }
        }
    } else {
        # Open both the fullpkgfile folder and the zipfile itself
        # NOTE: in tcl 8.6 it should be possible to use the zlib interface to actually do the unzip
        set msg [_ "\[deken\] Unable to extract package automatically." ]
        ::pdwindow::error "${msg}\n"
        set msg ""
        append msg [_ "Please perform the following steps manually:" ]
        append msg "\n"
        append msg [format [_ "1. Unzip %s." ]  $fullpkgfile ]
        append msg "\n"
        if { [string match "*.dek" $fullpkgfile] } {
            append msg "  "
            append msg [_ "You might need to change the file-extension from .dek to .zip" ]
            append msg "\n"
        }
        append msg [format [_ "2. Copy the contents into %s." ] $installdir]
        append msg "\n"
        append msg [format [_ "3. Remove %s. (optional)" ]  $fullpkgfile ]
        append msg "\n"

        ::pdwindow::post "$msg"

        pd_menucommands::menu_openfile $fullpkgfile
        pd_menucommands::menu_openfile $installdir
    }
    return $success
}

proc ::deken::utilities::uninstall {path library} {
    # recursively remove ${path}/${library} if it exists
    set fullpath [file join ${path} ${library}]
    if {[file exists ${fullpath}]} {
        ::deken::utilities::debug [format [_ "\[deken\] uninstalling '%s'" ] ${fullpath} ]
        if { [catch {
            file delete -force "${fullpath}"
        } stdout ] } {
            set msg [format [_ "Uninstalling %1\$s from %2\$s failed!"] ${library} ${path}]
            ::deken::utilities::debug "$msg\n    $stdout"
            return 0
        }
    }
    return 1
}

if { [catch {package require sha256} ] } {
    proc ::deken::utilities::verify_sha256 {url pkgfile} {
        set msg [format [_ "skipping SHA256 verification of %s." ] $url ]
        ::deken::status $msg
        ::deken::utilities::verbose 0 $msg
        return 1
    }
} else {  # successfully imported sha256
    proc ::deken::utilities::verify_sha256 {url pkgfile} {
        ::deken::status [format [_ "SHA256 verification of %s" ] $pkgfile ]
        ::deken::syncgui

        set retval 1
        set isremote 1
        if { [ catch {
            # check if $url really is a local file
            if { [file normalize $url] eq $url } {
                # $url is really an absolute filename
                # use it, if it exists
                set hashfile "${url}.sha256"
                set isremote 0
                if { [file isfile $url ] && [file readable $url] } { } else {
                    set hashfile ""
                }
            } else {
                # otherwise fetch it from the internet
                set hashfile [::deken::utilities::download_file ${url}.sha256 [::deken::utilities::get_tmpfilename [::deken::utilities::get_tmpdir] ".sha256" ] ]
            }
            if { "$hashfile" eq "" } {
                ::deken::utilities::verbose 0 [format [_ "unable to fetch reference SHA256 for %s." ] $url ]
                set retval 1
            } else {
                set fp [open $hashfile r]
                set reference [string trim [string tolower [read $fp] ] ]
                close $fp
                if { $isremote } {
                    catch { file delete $hashfile }
                }

                set hash [string trim [string tolower [ ::sha2::sha256 -hex -filename $pkgfile ] ] ]
                if { "${hash}" eq "${reference}" } {
                    set retval 1
                } else {
                    # SHA256 verification failed...
                    set retval 0
                }
            }
        } stdout ] } {
            ::deken::utilities::verbose 0 "${stdout}"
            # unable to verify
            ::deken::utilities::verbose 0 [format [_ "unable to perform SHA256 verification for %s." ] $url ]
            set retval 1
        }
        return ${retval}
    }
}

# download a file to a location
# http://wiki.tcl.tk/15303
proc ::deken::utilities::download_file {url outputfilename {progressproc {}}} {
    set URL [string map {{[} "%5b" {]} "%5d"} $url]
    set downloadfilename [::deken::utilities::get_tmpfilename [file dirname $outputfilename] ]
    set f [open $downloadfilename w]
    fconfigure $f -translation binary

    if { [catch {
        if { $progressproc eq {} } {
            set httpresult [::http::geturl $URL -binary true -channel $f]
        } else {
            set httpresult [::http::geturl $URL -binary true -progress ${progressproc} -channel $f]
        }
        set ncode [::http::ncode $httpresult]
        if {$ncode != 200} {
            ## FIXXME: we probably should handle redirects correctly (following them...)
            set err [::http::code $httpresult]
            set msg [format [_ "\[deken\] Unable to download from %1\$s \[%2\$s\]" ] $url $err ]
            ::pdwindow::error "$msg\n"
            set outputfilename ""
        }
        ::http::cleanup $httpresult
    } stdout ] } {
        set msg [format [_ "Unable to download from '%s'!" ] $url ]
        tk_messageBox \
            -title [_ "Download failed" ] \
            -message "${msg}\n$stdout" \
            -icon error -type ok \
            -parent .externals_searchui
        set outputfilename ""
    }

    flush $f
    close $f

    if { "$outputfilename" != "" } {
        catch { file delete $outputfilename }
        if {[file exists $outputfilename]} {
            ::deken::utilities::debug [format [_ "\[deken\] Unable to remove stray file %s" ] $outputfilename ]
            set outputfilename ""
        }
    }
    if { $outputfilename != "" && "$outputfilename" != "$downloadfilename" } {
        if {[catch { file rename $downloadfilename $outputfilename}]} {
            ::deken::utilities::debug [format [_ "\[deken\] Unable to rename downloaded file to %s" ] $outputfilename ]
            set outputfilename ""
        }
    }
    if { "$outputfilename" eq "" } {
        file delete $downloadfilename
    }

    return $outputfilename
}


# parse a deken-packagefilename into it's components:
# v0:: <pkgname>[-v<version>-]?{(<arch>)}-externals.<ext>
# v1:: <pkgname>[\[<version\]]?{(<arch>)}
# return: list <pkgname> <version> [list <arch> ...]
proc ::deken::utilities::parse_filename {filename} {
    set pkgname $filename
    set archs [list]
    set version ""
    if { [ string match "*.dek" $filename ] } {
        ## deken filename v1: <library>[v<version>](<arch1>)(<arch2>).dek
        set archstring ""
        regexp {^([^\[\]\(\)]+)((\[[^\[\]\(\)]+\])*)((\([^\[\]\(\)]+\))*)\.dek$} $filename _ pkgname optionstring _ archstring
        foreach {o _} [lreplace [split $optionstring {[]}] 0 0] {
            if {![string first v ${o}]} {
                set version [string range $o 1 end]
            } else { # ignoring unknown option...
            }
        }
        foreach {a _} [lreplace [split $archstring "()"] 0 0] { lappend archs $a }
    } elseif { [ regexp {(.*)-externals\..*} $filename _ basename] } {
        ## deken filename v0
        set pkgname $basename
        # basename <pkgname>[-v<version>-]?{(<arch>)}
        ## strip off the archs
        set baselist [split $basename () ]
        # get pkgname + version
        set pkgver [lindex $baselist 0]
        if { ! [ regexp "(.*)-v(.*)-" $pkgver _ pkgname version ] } {
            set pkgname $pkgver
            set version ""
        }
        # get archs
        foreach {a _} [lreplace $baselist 0 0] {
            # in filename.v0 the semantics of the last arch field ("bits") was unclear
            # since this format predates float64 builds, we just force it to 32
            regsub -- {-[0-9]+$} $a {-32} a
            lappend archs $a
        }
        if { "x$archs$version" == "x" } {
            # try again as <pkgname>-v<version>
            if { ! [ regexp "(.*)-v(.*)" $pkgver _ pkgname version ] } {
                set pkgname $pkgver
                set version ""
            }
        }
    }
    list $pkgname $version $archs
}


# ######################################################################
# ################ preferences #########################################
# ######################################################################

proc ::deken::preferences::newwidget {basename} {
    # calculate a widget name that has not yet been taken
    set i 0
    while {[winfo exists ${basename}${i}]} {incr i}
    return ${basename}${i}
}

proc ::deken::preferences::create_pathpad {toplevel row {padx 2} {pady 2}} {
    set pad [::deken::preferences::newwidget ${toplevel}.pad]
    frame $pad -relief groove -borderwidth 2 -width 2 -height 2
    grid ${pad} -sticky ew -row ${row} -column 0 -columnspan 3 -padx ${padx}  -pady ${pady}
}
proc ::deken::preferences::create_packpad {toplevel {padx 2} {pady 2} } {
    set mypad [::deken::preferences::newwidget ${toplevel}.pad]

    frame $mypad
    pack $mypad -padx ${padx} -pady ${pady} -expand 1 -fill "y"

    return $mypad
}

proc ::deken::preferences::userpath_doit { winid } {
    set installdir [::deken::do_prompt_installdir ${::deken::preferences::userinstallpath} $winid]
    if { "${installdir}" != "" } {
        set ::deken::preferences::userinstallpath "${installdir}"
    }
}

proc ::deken::preferences::path_doit {rdb ckb path {mkdir true}} {
    # handler for the check/create button
    # if the path does not exist, disable the radiobutton and suggest to Create it
    # if the path exists, check whether it is writable
    #  if it is writable, enable the radiobutton and disable the check/create button
    #  if it is not writable, keep the radiobutton disabled and suggest to (Re)Check
    ${ckb} configure -state normal
    ${rdb} configure -state disabled

    if { ! [file exists ${path}] } {
        ${ckb} configure -text [_ "Create"]
        if { $mkdir } {
            catch { file mkdir $path }
        }
    }

    if { [file exists ${path}] } {
        ${ckb} configure -text [_ "Check"]
    }

    if { [::deken::utilities::is_writabledir ${path} ] } {
        ${ckb} configure -state disabled
        ${rdb} configure -state normal
    }
}

proc ::deken::preferences::create_pathentry {toplevel row var path {generic false}} {
    # only add absolute paths to the pathentries
    set xpath [ ::deken::utilities::expandpath $path ]
    if {! $generic} {
        if { [file pathtype $xpath] != "absolute"} { return }
    }

    set rdb [::deken::preferences::newwidget ${toplevel}.path]
    set chk [::deken::preferences::newwidget ${toplevel}.doit]
    set pad [::deken::preferences::newwidget ${toplevel}.pad]

    radiobutton ${rdb} -value ${path} -text "${path}" -variable $var
    frame ${pad}
    button ${chk} -text "..." -command "::deken::preferences::path_doit ${rdb} ${chk} ${xpath}"

    grid ${rdb} -sticky "w"  -row ${row} -column 2
    grid ${pad} -sticky ""   -row ${row} -column 1 -padx 10
    grid ${chk} -sticky nsew -row ${row} -column 0


    if {! $generic} {
        ::deken::preferences::path_doit ${rdb} ${chk} ${xpath} false
    }
    list ${rdb} ${chk}
}

proc ::deken::preferences::create {winid} {
    # urgh...we want to know when the window gets drawn,
    # so we can query the size of the pathentries canvas
    # in order to get the scrolling-region right!!!
    # this seems to be so wrong...
    bind $winid <Map> "::deken::preferences::mapped %W"
    ::deken::bind_globalshortcuts $winid

    set ::deken::preferences::installpath $::deken::installpath
    set ::deken::preferences::hideforeignarch $::deken::hideforeignarch
    set ::deken::preferences::hideoldversions $::deken::hideoldversions
    if { $::deken::userplatform == "" } {
        set ::deken::preferences::platform DEFAULT
        set ::deken::preferences::userplatform [ ::deken::platform2string ]
    } else {
        set ::deken::preferences::platform USER
        set ::deken::preferences::userplatform $::deken::userplatform
    }

    set ::deken::preferences::installpath USER
    set ::deken::preferences::userinstallpath $::deken::installpath

    set ::deken::preferences::show_readme $::deken::show_readme
    set ::deken::preferences::keep_package $::deken::keep_package
    set ::deken::preferences::verify_sha256 $::deken::verify_sha256
    set ::deken::preferences::remove_on_install $::deken::remove_on_install
    set ::deken::preferences::add_to_path $::deken::add_to_path
    set ::deken::preferences::add_to_path_temp $::deken::preferences::add_to_path

    # this dialog allows us to select:
    #  - which directory to extract to
    #    - including all (writable) elements from $::sys_staticpath
    #      and option to create each of them
    #    - a directory chooser
    #  - whether to delete directories before re-extracting
    #  - whether to filter-out non-matching architectures
    labelframe $winid.installdir -text [_ "Install externals to directory:" ] -padx 5 -pady 5 -borderwidth 1
    canvas $winid.installdir.cnv \
        -confine true
    scrollbar $winid.installdir.scrollv \
        -command "$winid.installdir.cnv yview"
    scrollbar $winid.installdir.scrollh \
        -orient horizontal \
        -command "$winid.installdir.cnv xview"
    $winid.installdir.cnv configure \
        -xscrollincrement 0 \
        -xscrollcommand " $winid.installdir.scrollh set"
    $winid.installdir.cnv configure \
        -yscrollincrement 0 \
        -yscrollcommand " $winid.installdir.scrollv set" \

    pack $winid.installdir.cnv -side left -fill both -expand 1
    pack $winid.installdir.scrollv -side right -fill "y"
    pack $winid.installdir.scrollh -side bottom -fill "x" -before  $winid.installdir.cnv
    pack $winid.installdir -fill both

    set pathsframe [frame $winid.installdir.cnv.f]
    set row 0
    ### dekenpath: directory-chooser
    # FIXME: should we ask user to add chosen directory to PATH?
    set pathdoit [::deken::preferences::create_pathentry ${pathsframe} ${row} ::deken::preferences::installpath "USER" true]
    incr row
    [lindex $pathdoit 0] configure \
        -foreground blue \
        -value "USER" \
        -textvariable ::deken::preferences::userinstallpath \
        -variable ::deken::preferences::installpath
    [lindex $pathdoit 1] configure \
        -text "..." \
        -command "::deken::preferences::userpath_doit $winid"
    ::deken::preferences::create_pathpad ${pathsframe} ${row}
    incr row

    ### dekenpath: default directories
    if {[namespace exists ::pd_docsdir] && [::pd_docsdir::externals_path_is_valid]} {
        foreach p [::pd_docsdir::get_externals_path] {
            ::deken::preferences::create_pathentry ${pathsframe} ${row} ::deken::preferences::installpath $p
            incr row
        }
        ::deken::preferences::create_pathpad ${pathsframe} ${row}
        incr row
    }
    set extradir [file join ${::sys_libdir} extra ]
    foreach p $::sys_staticpath {
        if { [file normalize $p] == $extradir } {
            set p [file join @PD_PATH@ extra]
        }
        ::deken::preferences::create_pathentry ${pathsframe} ${row} ::deken::preferences::installpath $p
        incr row
    }
    ::deken::preferences::create_pathpad ${pathsframe} ${row}
    incr row

    foreach p $::sys_searchpath {
        ::deken::preferences::create_pathentry ${pathsframe} ${row} ::deken::preferences::installpath $p
        incr row
    }

    pack $pathsframe -fill "x"
    $winid.installdir.cnv create window 0 0 -anchor "nw" -window $pathsframe

    ## installation options
    labelframe $winid.install -text [_ "Installation options:" ] -padx 5 -pady 5 -borderwidth 1
    pack $winid.install -side top -fill "x" -anchor "w"

    checkbutton $winid.install.verify256 -text [_ "Try to verify the libraries' checksum before (re)installing them"] \
        -variable ::deken::preferences::verify_sha256
    pack $winid.install.verify256 -anchor "w"

    checkbutton $winid.install.remove -text [_ "Try to remove libraries before (re)installing them"] \
        -variable ::deken::preferences::remove_on_install
    pack $winid.install.remove -anchor "w"

    checkbutton $winid.install.readme -text [_ "Show README of newly installed libraries (if present)"] \
        -variable ::deken::preferences::show_readme
    pack $winid.install.readme -anchor "w"

    checkbutton $winid.install.keeppackage -text [_ "Keep package files after installation"] \
        -variable ::deken::preferences::keep_package
    pack $winid.install.keeppackage -anchor "w"


    checkbutton $winid.install.add_to_path -text [_ "Add newly installed libraries to Pd's search path"] \
        -variable ::deken::preferences::add_to_path
    catch { $winid.install.add_to_path configure \
                -tristatevalue 1 \
                -onvalue 2 \
                -command {set ::deken::preferences::add_to_path \
                              [set ::deken::preferences::add_to_path_temp \
                                   [::deken::utilities::tristate $::deken::preferences::add_to_path_temp 1 0]]}
    } stdout

    pack $winid.install.add_to_path -anchor "w"


    ## platform filter settings
    labelframe $winid.platform -text [_ "Platform settings:" ] -padx 5 -pady 5 -borderwidth 1
    pack $winid.platform -side top -fill "x" -anchor "w"

    # default architecture vs user-defined arch
    radiobutton $winid.platform.default -value "DEFAULT" \
        -text [format [_ "Default platform: %s" ] [::deken::platform2string ] ] \
        -variable ::deken::preferences::platform \
        -command "$winid.platform.userarch.entry configure -state disabled"
    pack $winid.platform.default -anchor "w"

    frame $winid.platform.userarch
    radiobutton $winid.platform.userarch.radio -value "USER" \
        -text [_ "User-defined platform:" ] \
        -variable ::deken::preferences::platform \
        -command "$winid.platform.userarch.entry configure -state normal"
    entry $winid.platform.userarch.entry -textvariable ::deken::preferences::userplatform
    if { "$::deken::preferences::platform" == "DEFAULT" } {
        $winid.platform.userarch.entry configure -state disabled
    }

    pack $winid.platform.userarch -anchor "w"
    pack $winid.platform.userarch.radio -side left
    pack $winid.platform.userarch.entry -side right -fill "x"

    # hide non-matching architecture?
    ::deken::preferences::create_packpad $winid.platform 2 10

    checkbutton $winid.platform.hide_foreign -text [_ "Hide foreign architectures"] \
        -variable ::deken::preferences::hideforeignarch
    pack $winid.platform.hide_foreign -anchor "w"
    checkbutton $winid.platform.only_newest -text [_ "Only show the newest version of a library\n(treats other versions like foreign architectures)"] \
        -variable ::deken::preferences::hideoldversions -justify "left"
    pack $winid.platform.only_newest -anchor "w"


    # Use two frames for the buttons, since we want them both bottom and right
    frame $winid.nb
    pack $winid.nb -side bottom -fill "x" -pady 2m

    # buttons
    frame $winid.nb.buttonframe
    pack $winid.nb.buttonframe -side right -fill "x" -padx 2m

    button $winid.nb.buttonframe.cancel -text [_ "Cancel"] \
        -command "::deken::preferences::cancel $winid"
    pack $winid.nb.buttonframe.cancel -side left -expand 1 -fill "x" -padx 15 -ipadx 10
    if {$::windowingsystem ne "aqua"} {
        button $winid.nb.buttonframe.apply -text [_ "Apply"] \
            -command "::deken::preferences::apply $winid"
        pack $winid.nb.buttonframe.apply -side left -expand 1 -fill "x" -padx 15 -ipadx 10
    }
    button $winid.nb.buttonframe.ok -text [_ "OK"] \
        -command "::deken::preferences::ok $winid"
    pack $winid.nb.buttonframe.ok -side left -expand 1 -fill "x" -padx 15 -ipadx 10
}

proc ::deken::preferences::mapped {winid} {
    set cnv $winid.installdir.cnv
    catch {
        set bbox [$cnv bbox all]
        if { "$bbox" != "" } {
            $cnv configure -scrollregion $bbox
        }
    } stdout
}

proc ::deken::preferences::show {{winid .deken_preferences}} {
    if {[winfo exists $winid]} {
        wm deiconify $winid
        raise $winid
    } else {
        toplevel $winid -class DialogWindow
        wm title $winid [format [_ "Deken %s Preferences"] $::deken::version]

        frame $winid.frame
        pack $winid.frame -side top -padx 6 -pady 3 -fill both -expand true

        ::deken::preferences::create $winid.frame
    }
}

proc ::deken::preferences::apply {winid} {
    set installpath "${::deken::preferences::installpath}"
    if { "$installpath" == "USER" } {
        set installpath "${::deken::preferences::userinstallpath}"
    }

    ::deken::set_installpath "$installpath"
    set plat ""
    if { "${::deken::preferences::platform}" == "USER" } {
        set plat "${::deken::preferences::userplatform}"
    }
    ::deken::set_platform_options ${plat} ${::deken::preferences::hideforeignarch} ${::deken::preferences::hideoldversions}
    ::deken::set_install_options \
        "${::deken::preferences::remove_on_install}" \
        "${::deken::preferences::show_readme}" \
        "${::deken::preferences::add_to_path}" \
        "${::deken::preferences::keep_package}" \
        "${::deken::preferences::verify_sha256}"
}
proc ::deken::preferences::cancel {winid} {
    ## FIXXME properly close the window/frame (for re-use in a tabbed pane)
    if {[winfo exists .deken_preferences]} {destroy .deken_preferences}
    #destroy $winid
}
proc ::deken::preferences::ok {winid} {
    ::deken::preferences::apply $winid
    ::deken::preferences::cancel $winid
}

# ######################################################################
# ################ core ################################################
# ######################################################################

if { [ catch { set ::deken::installpath [::pd_guiprefs::read dekenpath] } stdout ] } {
    # this is a Pd without the new GUI-prefs
    proc ::deken::set_installpath {installdir} {
        set ::deken::installpath $installdir
    }
    proc ::deken::set_platform_options {platform hideforeignarch {hideoldversions 0}} {
        set ::deken::userplatform $platform
        set ::deken::hideforeignarch [::deken::utilities::bool $hideforeignarch ]
        set ::deken::hideoldversions [::deken::utilities::bool $hideoldversions ]
    }
    proc ::deken::set_install_options {remove readme add keep verify256} {
        set ::deken::remove_on_install [::deken::utilities::bool $remove]
        set ::deken::show_readme [::deken::utilities::bool $readme]
        set ::deken::add_to_path [::deken::utilities::tristate $add 0 0]
        set ::deken::keep_package [::deken::utilities::bool $keep]
        set ::deken::verify_sha256 [::deken::utilities::bool $verify256]
    }
} else {
    catch {set ::deken::installpath [lindex ${::deken::installpath} 0]}
    # Pd has a generic preferences system, that we can use
    proc ::deken::set_installpath {installdir} {
        set ::deken::installpath $installdir
        ::pd_guiprefs::write dekenpath [list $installdir]
    }
    # user requested platform (empty = DEFAULT)
    set ::deken::userplatform [::pd_guiprefs::read deken_platform]
    catch {set ::deken::userplatform [lindex ${deken::userplatform} 0 ]}
    set ::deken::hideforeignarch [::deken::utilities::bool [::pd_guiprefs::read deken_hide_foreign_archs] 1]
    set ::deken::hideoldversions [::deken::utilities::bool [::pd_guiprefs::read deken_hide_old_versions] 1]
    proc ::deken::set_platform_options {platform hideforeignarch {hideoldversions 0}} {
        set ::deken::userplatform $platform
        set ::deken::hideforeignarch [::deken::utilities::bool $hideforeignarch ]
        set ::deken::hideoldversions [::deken::utilities::bool $hideoldversions ]
        ::pd_guiprefs::write deken_platform [list $platform]
        ::pd_guiprefs::write deken_hide_foreign_archs $::deken::hideforeignarch
        ::pd_guiprefs::write deken_hide_old_versions $::deken::hideoldversions
    }
    set ::deken::remove_on_install [::deken::utilities::bool [::pd_guiprefs::read deken_remove_on_install] 1]
    set ::deken::show_readme [::deken::utilities::bool [::pd_guiprefs::read deken_show_readme] 1]
    set ::deken::keep_package [::deken::utilities::bool [::pd_guiprefs::read deken_keep_package] 0]
    set ::deken::verify_sha256 [::deken::utilities::bool [::pd_guiprefs::read deken_verify_sha256] 1]
    set ::deken::add_to_path [::deken::utilities::tristate [::pd_guiprefs::read deken_add_to_path] ]

    proc ::deken::set_install_options {remove readme path keep verify256} {
        set ::deken::remove_on_install [::deken::utilities::bool $remove]
        set ::deken::show_readme [::deken::utilities::bool $readme]
        set ::deken::add_to_path [::deken::utilities::tristate $path]
        set ::deken::keep_package [::deken::utilities::bool $keep]
        set ::deken::verify_sha256 [::deken::utilities::bool $verify256]
        ::pd_guiprefs::write deken_remove_on_install "$::deken::remove_on_install"
        ::pd_guiprefs::write deken_show_readme "$::deken::show_readme"
        ::pd_guiprefs::write deken_add_to_path "$::deken::add_to_path"
        ::pd_guiprefs::write deken_keep_package "$::deken::keep_package"
        ::pd_guiprefs::write deken_verify_sha256 "$::deken::verify_sha256"
    }
}

proc ::deken::normalize_result {title
                                cmd
                                {match 1}
                                {subtitle ""}
                                {statusline ""}
                                {contextcmd {}}
                                {pkgname ""}
                                args} {
    ## normalize a search-result
    # the function parameters are guaranteed to be a stable API (with the exception or args)
    # but the value returned by this function is an implementation detail
    # <title> the primary line displayed for the search-result
    # - <cmd> the full command to run to install the library
    # - <match> boolean value to indicate whether this entry matches the current architecture
    # - <subtitle> additional text to be shown under the <name>
    # - <statusline> additional text to be shown in the STATUS line if the mouse hovers over the result
    # - <contextcmd> the full command to be executed when the user right-clicks the menu-entry
    # - <pkgname> the library name (typically this gets parsed from the package filename)
    # - <args> RESERVED FOR FUTURE USE (this is a variadic placeholder. do not use!)

    list "" $title $cmd $match $subtitle $statusline $contextcmd $pkgname
}


# find an install path, either from prefs or on the system
# returns an empty string if nothing was found
proc ::deken::find_installpath {{ignoreprefs false}} {
    set installpath ""
    if { [ info exists ::deken::installpath ] && !$ignoreprefs } {
        ## any previous choice?
        set installpath [ ::deken::utilities::get_writabledir [list $::deken::installpath ] ]
    }
    if { "$installpath" == "" } {
        ## search the default paths
        set installpath [ ::deken::utilities::get_writabledir $::sys_staticpath ]
    }
    if { "$installpath" == "" } {
        # let's use the first of $::sys_staticpath, if it does not exist yet
        set userdir [lindex $::sys_staticpath 0]
        if { ! [file exists ${userdir} ] } {
            set installpath $userdir
        }
    }
    return $installpath
}

proc ::deken::platform2string {{verbose 0}} {
    if { $verbose } {
        return $::deken::platform(os)-$::deken::platform(machine)-float$::deken::platform(floatsize)
    } else {
        return $::deken::platform(os)-$::deken::platform(machine)-$::deken::platform(floatsize)
    }
}

# allow overriding deken platform from Pd-core
proc ::deken::set_platform {os machine bits floatsize} {
    if { $os != $::deken::platform(os) ||
         $machine != $::deken::platform(machine) ||
         $bits != $::deken::platform(bits) ||
         $floatsize != $::deken::platform(floatsize)
     } {
        set ::deken::platform(os) ${os}
        set ::deken::platform(machine) ${machine}
        set ::deken::platform(bits) ${bits}
        set ::deken::platform(floatsize) ${floatsize}

        ::deken::utilities::verbose 1 [format [_ "Platform re-detected: %s" ] [::deken::platform2string 1] ]
    }
}

proc ::deken::versioncompare {a b} {
    # compares two versions, the Debian way
    # each version string is split into numeric and non-numeric elements
    # the elements are compared pairwise
    # "~" sorts before everything else

    # sidenote: in practice the version we get here are of the form "<date>/<library>/<version>/<date>"
    # we probably should only use this version-comparision for the <version> part,
    # and use 'string compare' for the other parts
    foreach x [regexp -all -inline {\d+|\D+} [string map {~ \t} $a]] y [regexp -all -inline {\d+|\D+} [string map {~ \t} $b]] {
        if { "$x" == "" } { set x " " }
        if { "$y" == "" } { set y " " }
        if { [catch {
           set c [dict get {1 0 {0 1} -1 {1 0} 1} [lsort -indices -dictionary -unique [list $x $y]]]
        } stdout ] } {
          # Tcl<8.5 (as found the PowerPC builds) lacks 'dict' and 'lsort -indices'
          if { [catch {
            # "string compare" does not sort numerically
            set c [expr 2 * ($x > $y) + ($x == $y) - 1]
          } stdout] } {
            set c [string compare $x $y]
          }
        }
        if { $c != "0" } {return $c}
    }
    return 0
}

proc ::deken::install_package_from_file {{pkgfile ""}} {
    set types {}
    lappend types [list [_ "Deken Packages" ] .dek]
    lappend types [list [_ "ZIP Files" ] .zip]
    if {$::tcl_platform(platform) ne "windows"} {
        lappend types [list [_ "TAR Files" ] {.tar.gz .tgz} ]
    }
    lappend types [list [_ "All Files" ]  *  ]
    if { "${pkgfile}" eq ""} {
        set pkgfile [tk_getOpenFile -defaultextension dek -filetypes $types]
    }
    if { "${pkgfile}" eq "" } { return }

    # user picked one
    # perform checks and install it
    set pkgfile [file normalize $pkgfile]

    if { "$::deken::verify_sha256" } {
        if { ! [::deken::utilities::verify_sha256 ${pkgfile} ${pkgfile}] } {
            ::deken::status [format [_ "Checksum mismatch for '%s'" ] $pkgfile] 0
            set msg [format [_ "SHA256 verification of %s failed!" ] $pkgfile ]

            set result "retry"
            while { "$result" eq "retry" } {
                set result [tk_messageBox \
                                -title [_ "SHA256 verification failed" ] \
                                -message "${msg}" \
                                -icon error -type abortretryignore \
                                -parent .externals_searchui]
                # we don't have a good strategy if the user selects 'retry'
                # so we just display the dialog again...
            }
            if { "$result" eq "abort" } { return }
        }
    }
    ::deken::install_package ${pkgfile} "" "" 1
}
proc ::deken::install_package {fullpkgfile {filename ""} {installdir ""} {keep 1}}  {
    # fullpkgfile: the file to extract
    # filename   : the package file name (usually the basename of $fullpkgfile)
    #              but might be different depending on the download)
    # installdir : where to put stuff into
    # keep       : whether to remove the fullpkgfile after successful extraction
    if { "${filename}" == "" } {
        set filename [file tail ${fullpkgfile}]
    }
    set installdir [::deken::ensure_installdir ${installdir} ${filename}]
    set parsedname [::deken::utilities::parse_filename $filename]
    set extname [lindex $parsedname 0]
    set extpath [file join $installdir $extname]


    set deldir ""
    if { "$::deken::remove_on_install" } {
        ::deken::status [format [_ "Removing %s" ] $extname ]

        if { ! [::deken::utilities::uninstall $installdir $extname] } {
            # ouch uninstalling failed.
            # on msw, lets assume this is because some of the files in the folder are locked.
            # so move the folder out of the way and proceed
            set deldir [::deken::utilities::get_tmpfilename $installdir]
            if { [ catch {
                file mkdir $deldir
                file rename [file join ${installdir} ${extname}] [file join ${deldir} ${extname}]
            } ] } {
                ::deken::utilities::debug [format [_ "\[deken\] temporarily moving %1\$s into %2\$s failed." ] $extname $deldir ]
                set deldir ""
            }
        }
    }

    ::deken::status [format [_ "Installing package %s" ] $extname ] 0
    ::deken::syncgui
    ::deken::progress 0
    if { [::deken::utilities::extract $installdir $filename $fullpkgfile $keep] > 0 } {
        ::deken::progressstatus [_ "Installation completed!" ]
        set msg [format [_ "Successfully installed %s!" ] $extname ]
        ::deken::status "${msg}" 0
        ::pdwindow::post "\[deken\] $msg\n"
        set install_failed 0
    } else {
        ::deken::progressstatus [_ "Installation failed!" ]
        set msg [format [_ "Failed to install %s!" ] $extname ]
        ::deken::status ${msg} 0
        tk_messageBox \
            -title [_ "Package installation failed" ] \
            -message "${msg}" \
            -icon error -type ok \
            -parent .externals_searchui
        set install_failed 1
    }

    if { "$deldir" != "" } {
        # try getting rid of the directory to be deleted
        # we already tried once (and failed), so this time we iterate over each file
        set rmerrors [::deken::utilities::rmrecursive $deldir]
        # and if there are still files around, ask the user to delete them.
        if { $rmerrors > 0 } {
            set result [tk_messageBox \
                            -message [format [_ "Failed to completely remove %1\$s.\nPlease manually remove the directory %2\$s after quitting Pd." ] $extname $deldir] \
                            -icon warning -type okcancel -default ok \
                            -parent .externals_searchui]
            switch -- $result {
                ok {
                    ::pd_menucommands::menu_openfile $deldir
                }
            }
        }
        set deldir ""
    }

    if { ${install_failed} } { return }

    if { "$::deken::show_readme" } {
        foreach ext {pd html pdf txt} {
            set r [file join $extpath "README.deken.$ext"]
            if {[file exists $r]} {
                if { "$ext" == "pd" } {
                    set directory [file normalize [file dirname $r]]
                    set basename [file tail $r]
                    pdsend "pd open [enquote_path $basename] [enquote_path $directory]"
                } else {
                    pd_menucommands::menu_openfile $r
                }
                break
            }
        }
    }

    if { "$::deken::add_to_path" } {
        # add to the search paths? bail if the version of pd doesn't support it
        if {[uplevel 1 info procs add_to_searchpaths] eq ""} {return}
        if {![file exists $extpath]} {
            ::deken::utilities::debug [format [_ "\[deken\] Unable to add %s to search paths"] $extname]
            return
        }
        set result yes
        if { $::deken::add_to_path > 1 } {
            set result yes
        } else {
            set result [tk_messageBox \
                            -message [format [_ "Add %s to the Pd search paths?" ]  $extname] \
                            -icon question -type yesno -default yes \
                            -parent .externals_searchui]
        }
        switch -- "${result}" {
            yes {
                add_to_searchpaths [file join $installdir $extname]
                ::deken::utilities::debug [format [_ "\[deken\] Added %s to search paths"] $extname]
                # if this version of pd supports it, try refreshing the helpbrowser
                if {[uplevel 1 info procs ::helpbrowser::refresh] ne ""} {
                    ::helpbrowser::refresh
                }
            }
            no {
                return
            }
        }
    }
}

##### GUI ########
proc ::deken::bind_globalshortcuts {toplevel} {
    set closescript "destroy $toplevel"
    bind $toplevel <$::modifier-Key-w> $closescript
}

proc ::deken::status {{msg ""} {timeout 5000}} {
    after cancel $::deken::statustimer
    if {"" ne $msg} {
        set ::deken::statustext "$msg"
        if { $timeout != "0" } {
            set ::deken::statustimer [after $timeout [list set "::deken::statustext" ""]]
        }
    } else {
        set ::deken::statustext ""
    }
}
proc ::deken::progressstatus {{msg ""} {timeout 5000}} {
   after cancel $::deken::progresstimer
    if {"" ne $msg} {
        set ::deken::progresstext "$msg"
        if { $timeout != "0" } {
            set ::deken::progresstimer [after $timeout [list set "::deken::progresstext" ""]]
        }
    } else {
        set ::deken::progresstext ""
    }
}
proc ::deken::syncgui {} {
    update idletasks
}
proc ::deken::scrollup {} {
    variable winid
    if { [winfo exists $winid] } {
        $winid.results see 0.0
    }
}
proc ::deken::post {msg {tag ""}} {
    variable winid
    if { [winfo exists $winid] } {
        $winid.results insert end "$msg\n" $tag
        $winid.results see end
    }
}
proc ::deken::clearpost {} {
    variable winid
    if { [winfo exists $winid] } {
        $winid.results delete 1.0 end
    }
    set ::deken::selected {}
}
proc ::deken::bind_posttag {tagname key cmd} {
    variable winid
    if { [winfo exists $winid] } {
        $winid.results tag bind $tagname $key $cmd
    }
}
proc ::deken::highlightable_posttag {tagname} {
    variable winid
    if { [winfo exists $winid] } {
        ::deken::bind_posttag $tagname <Enter> \
            "$winid.results tag add highlight [ $winid.results tag ranges $tagname ]"
        ::deken::bind_posttag $tagname <Leave> \
            "$winid.results tag remove highlight [ $winid.results tag ranges $tagname ]"
        # make sure that the 'highlight' tag is topmost
        $winid.results tag raise highlight
    }
}
proc ::deken::bind_contextmenu {winid tagname cmd} {
    if { [winfo exists $winid] } {
        if {$::windowingsystem eq "aqua"} {
            $winid.results tag bind $tagname <2> $cmd
        } else {
            $winid.results tag bind $tagname <3> $cmd
        }
    }
}

proc ::deken::menu_selectpackage {winid pkgname installcmd} {
    set results ${winid}.results

    # set/unset the selection in a "dict"
    set state {}
    set counter 1
    foreach {k v} $::deken::selected {
        if { $k eq $pkgname } {
            if { $v ne $installcmd } {
                set state 1
                lset ::deken::selected $counter $installcmd
            } else {
                set state 0
                lset ::deken::selected $counter {}
            }
            break
        }
        incr counter 2
    }
    if { ${state} eq {} } {
        # not found in the dict; just add it
        lappend ::deken::selected $pkgname $installcmd
        set state 1
    }

    # set/unset the visual representation (via tags)
    set counter 0
    foreach {a b} [$results tag ranges /$pkgname] {$results tag remove sel $a $b}
    if { $state } {
        foreach r $::deken::results {
            if { [lindex $r 1] eq ${installcmd} } {
                foreach {a b} [$results tag ranges ch$counter] {$results tag add sel $a $b}
            }
            incr counter
        }
    }
}
proc ::deken::menu_installselected {winid} {
    set counter 0
    foreach {k v} $::deken::selected {
        if { $v ne {} } {
            eval $v
            incr counter
        }
    }
    if { $counter == 0 } {
        ::deken::status [_ "No packages selected for installation."]
    } elseif { $counter > 1 } {
        ::deken::status [format [_ "Processed %d packages selected for installation."] $counter ]
    }
    # clear the selection
    set ::deken::selected {}
    foreach {a b} [${winid}.results tag ranges sel] {${winid}.results tag remove sel $a $b}
}


proc ::deken::do_prompt_installdir {path {winid {}}} {
    set msg [_ "Install externals to directory:"]
    if { $winid eq {} } {
        set winid $::deken::winid
    }
    if {[winfo exists $winid]} {
        tk_chooseDirectory -title "${msg}" -initialdir ${path} -parent $winid
    } else {
        tk_chooseDirectory -title "${msg}" -initialdir ${path}
    }
}

proc ::deken::prompt_installdir {} {
    set installdir [::deken::do_prompt_installdir $::fileopendir]
    if { "$installdir" != "" } {
        ::deken::set_installpath $installdir
        return 1
    }
    return 0
}


proc ::deken::update_searchbutton {winid} {
    if { [$winid.searchbit.entry get] == "" } {
        $winid.searchbit.button configure -text [_ "Show all" ]
    } else {
        $winid.searchbit.button configure -text [_ "Search" ]
    }
}

proc ::deken::progress {x} {
    ::deken::status [format [_ "%s%% of download completed"] ${x}]
}

# this function gets called when the menu is clicked
proc ::deken::open_searchui {winid} {
    if {[winfo exists $winid]} {
        wm deiconify $winid
        raise $winid
    } else {
        ::deken::create_dialog $winid
        ::deken::bind_globalshortcuts $winid
        ::deken::utilities::dnd_init $winid.results
        $winid.results tag configure error -foreground red
        $winid.results tag configure warn -foreground orange
        $winid.results tag configure info -foreground grey
        $winid.results tag configure highlight -foreground blue
        $winid.results tag configure archmatch
        $winid.results tag configure noarchmatch -foreground grey
    }
    ::deken::clearpost
    ::deken::post [_ "Enter an exact library or object name."] info
    set msg [_ "e.g. 'freeverb~'"]
    ::deken::post "\t${msg}" info
    ::deken::post [_ "Use the '*' wildcard to match any number of characters."] info
    set msg [_ "e.g. '*-plugin' will match 'deken-plugin' (and more)."]
    ::deken::post "\t${msg}" info
    ::deken::post [_ "You can restrict the search to only-libraries or only-objects."] info
    ::deken::post [_ "To get a list of all available externals, try an empty search."] info
    ::deken::post "" info
    ::deken::post [_ "Right-clicking a search result will give you more options..." ] info
}

# build the externals search dialog window
proc ::deken::create_dialog {winid} {
    toplevel $winid -class DialogWindow
    set ::deken::winid $winid
    wm title $winid [_ "Find externals"]
    wm geometry $winid 670x550
    wm minsize $winid 230 360
    wm transient $winid
    $winid configure -padx 10 -pady 5

    if {$::windowingsystem eq "aqua"} {
        $winid configure -menu $::dialog_menubar
    }

    frame $winid.searchbit
    pack $winid.searchbit -side top -fill "x"

    entry $winid.searchbit.entry -font 18 -relief sunken -highlightthickness 1 -highlightcolor blue
    pack $winid.searchbit.entry -side left -padx 6 -fill "x" -expand true
    bind $winid.searchbit.entry <Key-Return> "::deken::initiate_search $winid"
    bind $winid.searchbit.entry <KeyRelease> "::deken::update_searchbutton $winid"
    focus $winid.searchbit.entry
    button $winid.searchbit.button -text [_ "Show all"] -default active -command "::deken::initiate_search $winid"
    pack $winid.searchbit.button -side right -padx 6 -pady 3 -ipadx 10

    frame $winid.objlib
    pack $winid.objlib -side top -fill "x"
    label $winid.objlib.label -text [_ "Search for: "]
    radiobutton $winid.objlib.libraries -text [_ "libraries"] -variable ::deken::searchtype -value libraries
    radiobutton $winid.objlib.objects -text [_ "objects"] -variable ::deken::searchtype -value objects
    radiobutton $winid.objlib.both -text [_ "both"] -variable ::deken::searchtype -value name
    foreach x {label libraries objects both} {
        pack $winid.objlib.$x -side left -padx 6
    }
    # for Pd that supports it, add a 'translation' radio
    if {[uplevel 2 info procs add_to_helppaths] ne ""} {
        radiobutton $winid.objlib.translations -text [_ "translations"] -variable ::deken::searchtype -value translations
        pack $winid.objlib.translations -side left -padx 6
    }
    frame $winid.warning
    pack $winid.warning -side top -fill "x"
    label $winid.warning.label -text [_ "Only install externals uploaded by people you trust."]
    pack $winid.warning.label -side left -padx 6

    text $winid.results -takefocus 0 -cursor hand2 -height 100 -yscrollcommand "$winid.results.ys set"
    scrollbar $winid.results.ys -orient vertical -command "$winid.results yview"
    pack $winid.results.ys -side right -fill "y"
    pack $winid.results -side top -padx 6 -pady 3 -fill both -expand true

    frame $winid.progress
    pack $winid.progress -side top -fill "x"
    if { ! [ catch {
        ttk::progressbar $winid.progress.bar -orient horizontal -length 640 -maximum 100 -mode determinate -variable ::deken::progressvar } stdout ] } {
        pack $winid.progress.bar -side top -fill "x"
        proc ::deken::progress {x} { set ::deken::progressvar $x }
        label ${winid}.progress.label -textvariable ::deken::progresstext -padx 0 -borderwidth 0
        place ${winid}.progress.label -in ${winid}.progress.bar -x 1
    }

    frame $winid.status
    pack $winid.status -side bottom -fill "x" -pady 3
    label $winid.status.label -textvariable ::deken::statustext -relief sunken -anchor "w"
    pack $winid.status.label -side bottom -fill "x"

    set m .deken_moremenu
    if { [winfo exists $m] } {
        destroy $m
    }
    set m [menu $m]
    $m add command -label [_ "Preferences..." ]  -command "::deken::preferences::show"
    $m add command -label [_ "Install DEK file..." ]  -command "::deken::install_package_from_file"

    button $winid.status.installdek -text [_ "More..." ] -command "tk_popup $m \[winfo pointerx $winid\] \[winfo pointery $winid\]"
    pack $winid.status.installdek -side right -padx 6 -pady 3 -ipadx 10
}

proc ::deken::open_search_xxx {searchtype xxx}  {
    set winid .externals_searchui
    ::deken::open_searchui $winid
    ::deken::clearpost

    set searchterm {}
    if { $::deken::searchtype eq "${searchtype}" } {
        append searchterm [$winid.searchbit.entry get]
    }
    if { ${searchterm} ne {} } {  append searchterm " " }
    foreach xx $xxx {
        foreach x $xx {
            lappend searchterm $x
        }
    }

    ${winid}.searchbit.entry delete 0 end
    ${winid}.searchbit.entry insert end ${searchterm}

    set ::deken::searchtype "${searchtype}"
    ::deken::update_searchbutton $winid
}

proc ::deken::open_search_objects {args}  {
    ::deken::open_search_xxx "objects" $args
}

proc ::deken::open_search_libraries {args}  {
    ::deken::open_search_xxx "libraries" $args
}
proc ::deken::open_search_translations {args}  {
    ::deken::open_search_xxx "translations" $args
}


proc ::deken::open_search_missing_libraries {args}  {
    # LATER this should only display not-installed libraries
    ::deken::open_search_xxx "libraries" $args
}

proc ::deken::initiate_search {winid} {
    set searchterm [$winid.searchbit.entry get]
    # let the user know what we're doing
    ::deken::clearpost
    ::deken::utilities::debug [format [_ "\[deken\] Start searching for externals... %s" ] ${searchterm}]
    set ::deken::progressvar 0
    ::deken::progressstatus ""
    if { [ catch {
        set results [::deken::search_for ${searchterm}]
    } stdout ] } {
        ::deken::utilities::debug [format [_ "\[deken\] online? %s" ] $stdout ]
        ::deken::status [_ "Unable to perform search. Are you online?" ]
    } else {
        # delete all text in the results
        ::deken::clearpost

        set ::deken::results $results
        if {[llength $results] != 0} {
            ::deken::show_results $winid
        } else {
            ::pdwindow::post [_ "\[deken\] No matching externals found." ]
            set msg [_ "Try using the full name e.g. 'freeverb~'." ]
            ::pdwindow::debug " ${msg}"
            ::pdwindow::post "\n"
            ::deken::status [_ "No matching externals found." ]
        }
    }
}

# display a single found entry
proc ::deken::show_result {winid counter result showmatches} {
    foreach {title cmd match comment status contextcmd pkgname} $result {break}
    set tag ch$counter
    set tags [list $tag [expr ${match}?"archmatch":"noarchmatch" ] ]
    if { "$pkgname" ne "" } {lappend tags "/$pkgname"}

    if {($match == $showmatches)} {
        set comment [string map {"\n" "\n\t"} $comment]
        ::deken::post "$title\n\t$comment\n" $tags
        ::deken::highlightable_posttag $tag
        ::deken::bind_posttag $tag <Enter> "+::deken::status {$status}"
        ::deken::bind_posttag $tag <1> "$cmd"
        if { "" ne $contextcmd } {
            ::deken::bind_contextmenu $winid $tag $contextcmd
        }
    }
}

# display all found entry
proc ::deken::show_results {winid} {
    set counter 0
    # build the list UI of results
    foreach r $::deken::results {
        ::deken::show_result $winid $counter $r 1
        incr counter
    }
    if { "${::deken::hideforeignarch}" } {
        # skip display of non-matching archs
    } else {
        set counter 0
        foreach r $::deken::results {
            ::deken::show_result $winid $counter $r 0
            incr counter
        }
    }
    ::deken::scrollup
    $winid.results tag raise sel
    $winid.results tag raise highlight
}

proc ::deken::ensure_installdir {{installdir ""} {extname ""}} {
    ## make sure that the destination path exists
    ### if ::deken::installpath is set, use the first writable item
    ### if not, get a writable item from one of the searchpaths
    ### if this still doesn't help, ask the user
    if { "$installdir" != "" } {return $installdir}

    set installdir [::deken::find_installpath]
    if { "$installdir" != "" } {return $installdir}

    if {[namespace exists ::pd_docsdir] && [::pd_docsdir::externals_path_is_valid]} {
        # if the docspath is set, try the externals subdir
        set installdir [::pd_docsdir::get_externals_path]
    }
    if { "$installdir" != "" } {return $installdir}

    # ask the user (and remember the decision)
    ::deken::prompt_installdir
    set installdir [ ::deken::utilities::get_writabledir [list $::deken::installpath ] ]

    set installdir [::deken::utilities::expandpath $installdir ]
    while {1} {
        if { "$installdir" == "" } {
            set result [tk_messageBox \
                            -message [_ "Please select a (writable) installation directory!"] \
                            -icon warning -type retrycancel -default retry \
                            -parent .externals_searchui]
            switch -- "${result}" {
                cancel {return}
                retry {
                    if {[::deken::prompt_installdir]} {
                        set installdir $::deken::installpath
                    } else {
                        continue
                    }
                }
            }
        } else {
            set result [tk_messageBox \
                            -message [format [_ "Install %1\$s to %2\$s?" ] $extname $installdir] \
                            -icon question -type yesnocancel -default yes \
                            -parent .externals_searchui]
            switch -- "${result}" {
                cancel {return}
                yes { }
                no {
                    set prevpath $::deken::installpath
                    if {[::deken::prompt_installdir]} {
                        set keepprevpath 1
                        set installdir $::deken::installpath
                        # if docsdir is set & the install path is valid,
                        # saying "no" is temporary to ensure the docsdir
                        # hierarchy remains, use the Path dialog to override
                        if {[namespace exists ::pd_docsdir] && [::pd_docsdir::path_is_valid] &&
                            [file writable [file normalize $prevpath]] } {
                            set keepprevpath 0
                        }
                        if {$keepprevpath} {
                            set ::deken::installpath $prevpath
                        }
                    } else {
                        continue
                    }
                }
            }
        }

        if { "$installdir" != "" } {
            # try creating the installdir...just in case
            catch { file mkdir $installdir }
        }
        # check whether this is a writable directory
        set installdir [ ::deken::utilities::get_writabledir [list $installdir ] ]
        if { "$installdir" != "" } {
            # stop looping if we've found our dir
            break
        }
    }
    return $installdir
}

# handle a clicked link
proc ::deken::clicked_link {URL filename} {
    ## make sure that the destination path exists
    ### if ::deken::installpath is set, use the first writable item
    ### if not, get a writable item from one of the searchpaths
    ### if this still doesn't help, ask the user
    set installdir [::deken::ensure_installdir "" ${filename}]
    if { "${installdir}" == "" } {
        ::deken::utilities::debug [format [_ "Cancelling download of %s: No installation directory given." ] $filename]
        ::deken::status [format [_ "Installing to non-existent directory failed" ] $filename]
        return
    }
    set fullpkgfile [file join $installdir $filename]
    ::pdwindow::debug [format [_ "Commencing downloading of:\n%1\$s\nInto %2\$s..." ] $URL $installdir]
    ::deken::status [format [_ "Downloading '%s'" ] $filename] 0
    ::deken::syncgui
    set fullpkgfile [::deken::utilities::download_file $URL $fullpkgfile "::deken::download_progress"]
    if { "$fullpkgfile" eq "" } {
        ::deken::utilities::debug [_ "aborting."]
        ::deken::status [format [_ "Downloading '%s' failed" ] $filename]
        ::deken::progressstatus [_ "Download failed!" ]
        ::deken::progress 0
        return
    }
    ::deken::progressstatus [_ "Download completed! Verifying..." ]
    ::deken::utilities::debug ""
    if { ! [::deken::utilities::verify_sha256 ${URL} $fullpkgfile] } {
        ::deken::status [format [_ "Checksum mismatch for '%s'" ] $filename] 0
        if { "$::deken::verify_sha256" } {
            if { ! "$::deken::keep_package" } {
                catch { file delete $fullpkgfile }
            }
            tk_messageBox \
                -title [_ "SHA256 verification failed" ] \
                -message [format [_ "SHA256 verification of %s failed!" ] $fullpkgfile ] \
                -icon error -type ok \
                -parent .externals_searchui
            ::deken::progress 0
            return
        }
    }
    ::deken::install_package ${fullpkgfile} ${filename} ${installdir} ${::deken::keep_package}
}

# print the download progress to the results window
proc ::deken::download_progress {token total current} {
    if { $total > 0 } {
        ::deken::progress [expr {round(100 * (1.0 * $current / $total))}]
    }
}

# test for platform match with our current platform
proc ::deken::architecture_match {archs} {
    if { "translations" eq "${::deken::searchtype}" } {
        foreach arch $archs {
            if { "i18n" eq "${arch}" } {
                return 1
            }
            if {[string match "i18n-*" ${arch}] } {
                return 1
            }
        }
        return 0
    }
    # if there are no architecture sections this must be arch-independent
    if { ! [llength $archs] } { return 1}
    set OS "$::deken::platform(os)"
    set MACHINE "$::deken::platform(machine)"
    set BITS "$::deken::platform(bits)"
    set FLOATSIZE "$::deken::platform(floatsize)"
    if { "$::deken::userplatform" != "" } {
        ## FIXXME what if the user-supplied input isn't valid?
        regexp -- {(.*)-(.*)-(.*)} $::deken::userplatform _ OS MACHINE FLOATSIZE
    }
    # strip the little-endian indicator from arm-archs, it's the default
    regexp -- {(armv[0-9]*)[lL]} $MACHINE _ MACHINE

    # check each architecture in our list against the current one
    foreach arch $archs {
        if { [ regexp -- {(.*)-(.*)-(.*)} $arch _ os machine floatsize ] } {
            # normalize arm-architectures by stripping away sub-architectures
            # TODO: leave any big-endian indicator in place
            regexp -- {(armv[0-9]*)[^0-9]*} $machine _ machine
            if { ("${os}" eq "${OS}") && (("${floatsize}" eq "${FLOATSIZE}") || ("${floatsize}" eq "0"))} {
                ## so OS and floatsize match...
                ## check whether the CPU matches as well
                if { "${machine}" eq "${MACHINE}" } {return 1}
                ## not exactly; see whether it is in the list of compat CPUs
                if {[llength [array names ::deken::architecture_substitutes -exact "${MACHINE}"]]} {
                    foreach cpu $::deken::architecture_substitutes(${MACHINE}) {
                        if { "${machine}" eq "${cpu}" } {return 1}
                    }
                }
            }
        }
    }
    return 0
}

proc ::deken::search_for {term} {
    ::deken::status [format [_ "searching for '%s'" ] $term ]

    set result [list]
    foreach searcher $::deken::backends {
        if {[catch {
            foreach r [ $searcher $term ] {
                if { "" eq [lindex $r 0] } {
                    # data is already normalized
                } else {
                    # legacy data format
                    foreach {title cmd match comment status} $r {break}
                    set r [::deken::normalize_result $title $cmd $match $comment $status]
                }
                lappend result [lrange $r 1 end]
            }
        } stdout] } {
            ::deken::utilities::debug "\[deken\] $searcher: $stdout"
        }
    }
    return $result
}


proc ::deken::initialize {} {
    # console message to let them know we're loaded
    ## but only if we are being called as a plugin (not as built-in)
    if { "" != "$::current_plugin_loadpath" } {
        ::deken::utilities::debug [format [_ "\[deken\] deken-plugin.tcl (Pd externals search) loaded from %s." ] $::current_plugin_loadpath ]
    }
    set msg [format [_ "\[deken\] Platform detected: %s" ] [::deken::platform2string 1] ]
    ::pdwindow::verbose 0 "${msg}\n"

    # try to set install path when plugin is loaded
    set ::deken::installpath [::deken::find_installpath]


    # create an entry for our search in the "help" menu (or re-use an existing one)
    set mymenu .menubar.help
    if { [catch {
        $mymenu entryconfigure [_ "Find externals"] -command {::deken::open_searchui .externals_searchui}
    } _ ] } {
        $mymenu add separator
        $mymenu add command -label [_ "Find externals"] -command {::deken::open_searchui .externals_searchui}
    }
    # bind all <$::modifier-Key-s> {::deken::open_helpbrowser .helpbrowser2}
}


# ######################################################################
# ################ search backends #####################################
# ######################################################################

proc ::deken::register {fun} {
    # register a searchfunction with deken.
    # the searchfunction <fun> will be called with a <searchterm>,
    # and must return a list of <result>.
    # <searchterm> is a list of (whitespace separated) words.
    # each word denotes a library or library-object to search for and may
    # contain wildcards ("*").
    # the <result> should be normalized via ::deken::search::normalize_result
    # failing to do so, a <result> is a list <name> <cmd> <match> <comment> <status> <args...>
    # - <title> non-empty name of the library (to be shown to the user as search-result)
    # - <cmd>  the full command to run to install the library
    # - <match> boolean value to indicate whether this entry matches the current architecture
    # - <subtitle> additional text to be shown under the <name>
    # - <status> additional text to be shown in the STATUS line if the mouse hovers over the result
    # - <args>... additional args (ignored)
    # the library <name> must be non-empty (and empty value is reserved for normalized results)

    set ::deken::backends [linsert $::deken::backends 0 $fun]
}


## API draft

# each backend is implemented via a single proc
## that takes a single argument "term", the term to search for
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
namespace eval ::deken::search::puredata.info { }
proc ::deken::search::puredata.info::search {term} {
    set searchresults [list]
    set dekenserver "${::deken::protocol}://deken.puredata.info/search"
    catch {set dekenserver $::env(DEKENSERVER)} stdout
    set queryterm {}
    if { ${::deken::searchtype} eq "translations" && ${term} eq "" } {
        # special handling of searching for all translations (so we ONLY get translations)
        set term {*}
    }
    foreach x $term {lappend queryterm ${::deken::searchtype} $x}
    if { [ catch {set queryterm [::http::formatQuery {*}$queryterm ] } stdout ] } {
        set queryterm [ join $term "&${::deken::searchtype}=" ]
        set queryterm "${::deken::searchtype}=${queryterm}"
    }

    # deken-specific socket config
    set httpaccept [::http::config -accept]
    set httpagent [::http::config -useragent]
    set pdversion "Pd/$::PD_MAJOR_VERSION.$::PD_MINOR_VERSION.$::PD_BUGFIX_VERSION$::PD_TEST_VERSION"
    ::http::config -accept text/tab-separated-values
    ::http::config -useragent "Deken/${::deken::version} ([::deken::platform2string]) ${pdversion} Tcl/[info patchlevel]"

    # fetch search result
    if { [catch {
        set token [::http::geturl "${dekenserver}?${queryterm}"]
    } stdout ] } {
        set msg [format [_ "Searching for '%s' failed!" ] $term ]
        tk_messageBox \
            -title [_ "Search failed" ] \
            -message "${msg}\n$stdout" \
            -icon error -type ok \
            -parent .externals_searchui
        return
    }

    # restore http settings
    ::http::config -accept $httpaccept
    ::http::config -useragent $httpagent

    set ncode [::http::ncode $token]
    if { $ncode != 200 } {
        set err [::http::code $token]
        set msg [_ "\[deken\] Unable to perform search."]
        ::deken::utilities::debug "$msg\n   ${err}"
        return {}
    }
    set contents [::http::data $token]
    ::http::cleanup $token

    set splitCont [split $contents "\n"]
    # loop through the resulting tab-delimited table
    if { [catch {
        set latestrelease0 [dict create]
        set latestrelease1 [dict create]
        set newestversion [dict create]
        foreach ele $splitCont {
            set ele [ string trim $ele ]
            if { "" ne $ele } {
                foreach {name URL creator date} [ split $ele "\t" ] {break}
                set filename [ file tail $URL ]
                foreach {pkgname version archs} [ ::deken::utilities::parse_filename $filename ] {break}

                #if { $version eq "0.0.extended" } { set date "0000-00-00 00:02:00" }
                set olddate {}
                set match [::deken::architecture_match "$archs" ]

                if { ${match} } {
                    catch { set olddate [dict get ${latestrelease1} $pkgname] }

                    set oldversion {}
                    catch { set oldversion [dict get ${newestversion} $pkgname]}
                    if { [::deken::versioncompare $version $oldversion] > 0 } {
                        dict set newestversion $pkgname $version
                    }
                } else {
                    catch { set olddate [dict get ${latestrelease0} $pkgname] }
                }

                if { $date > $olddate } {
                    dict set latestrelease${match} $pkgname $date
                }
            }
        }
    } stdout ] } {
        set latestrelease0 {}
        set latestrelease1 {}
        set newestversion {}
    }
    foreach ele $splitCont {
        set ele [ string trim $ele ]
        if { "" ne $ele } {
            foreach {name URL creator date} [ split $ele "\t" ] {break}
            set decURL [::deken::utilities::urldecode $URL]
            set filename [ file tail $URL ]
            set cmd [list ::deken::clicked_link $decURL $filename]
            set pkgverarch [ ::deken::utilities::parse_filename $filename ]
            set pkgname [lindex $pkgverarch 0]
            set version [lindex $pkgverarch 1]
            set archs [lindex $pkgverarch 2]

            set match [::deken::architecture_match "$archs" ]
            set comment [format [_ "Uploaded by %1\$s @ %2\$s" ] $creator $date ]
            set status $URL
            set sortprefix "0000-00-00 00:01:00"
            if { ${match} == 0 } {
                catch { set sortprefix [dict get ${latestrelease0} $pkgname] }
            } else {
                if { "${::deken::hideoldversions}" } {
                    # If this version is not the newest one, mark it as unmatched
                    catch {
                        set oldversion [dict get ${newestversion} $pkgname]
                        if { [::deken::versioncompare $version $oldversion] < 0 } {
                            set match 0
                        }
                    }
                }
            }
            catch { set sortprefix [dict get ${latestrelease1} $pkgname] }
            set sortname "${sortprefix}/${pkgname}/${version}/${date}"
            set contextcmd [list ::deken::search::puredata.info::contextmenu %W %x %y $URL]
            set res [list $sortname $filename $name $cmd $match $comment $status $contextcmd $pkgname]
            lappend searchresults $res
        }
    }
    set sortedresult []
    foreach r [lsort -command ::deken::versioncompare -decreasing -index 0 $searchresults ] {
        foreach {sortname filename title cmd match comment status menus pkgname} $r {
            lappend sortedresult [::deken::normalize_result $title $cmd $match $comment $status $menus $pkgname]
            break
        }
    }
    return $sortedresult
}
proc ::deken::search::puredata.info::contextmenu {widget theX theY URL} {
    set m .dekenresults_contextMenu
    if { [winfo exists $m] } {
        destroy $m
    }
    menu $m

    set saveURL [string map {"[" "%5B" "]" "%5D"} $URL]
    set decURL [::deken::utilities::urldecode $URL]
    set filename [ file tail $URL ]
    set pkgverarch [ ::deken::utilities::parse_filename $filename ]
    set pkgname [lindex $pkgverarch 0]
    set cmd [list ::deken::clicked_link $decURL $filename]

    set selcount 0
    set selected 0
    foreach {k v} $::deken::selected {
        if { ${v} != {} } {incr selcount}
        if { ($k eq $pkgname) && ($v eq $cmd) } {
            set selected 1
            break
        }
    }

    if { $selected } {
        set selmsg [_ "Deselect package" ]
    } else {
        set selmsg [_ "Select package for installation" ]
    }
    $m add command -label "${selmsg}" -command "::deken::menu_selectpackage .externals_searchui $pkgname {$cmd}"
    $m add separator
    if { $selcount } {
        $m add command -label [_ "Install selected packages" ] -command "::deken::menu_installselected .externals_searchui"
    } else {
        $m add command -label [_ "Install package" ] -command $cmd
    }
    $m add separator
    $m add command -label [_ "Open package webpage" ] -command "pd_menucommands::menu_openfile [file dirname ${URL}]"
    $m add command -label [_ "Copy package URL" ] -command "clipboard clear; clipboard append $saveURL"
    $m add command -label [_ "Copy SHA256 checksum URL" ] -command "clipboard clear; clipboard append ${saveURL}.sha256"
    $m add command -label [_ "Copy OpenGPG signature URL" ] -command "clipboard clear; clipboard append ${saveURL}.asc"
    tk_popup $m [expr [winfo rootx $widget] + $theX] [expr [winfo rooty $widget] + $theY]
}


::deken::initialize
::deken::register ::deken::search::puredata.info::search
}
