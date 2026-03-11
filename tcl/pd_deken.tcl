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
package require Tcl 8.4 9
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
package require scrollboxwindow
package require scrollbox


namespace eval ::deken:: {
    variable version

    variable installpath
    variable userplatform
    variable hideforeignarch
    variable hideoldversions

    # results: {{title} {cmd} {description} {url} {ctxmenu}}
    variable results
    # selected: {library} {cmd} ...
    variable selected {}
}
namespace eval ::deken::preferences {
    variable installpath
    variable installpath_x
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
        set v0 [split ${::deken::version} "."]
        set v1 [split ${version} "."]
        foreach x ${v0} y ${v1} {
            if { ${x} > ${y} } {
                set msg [format [_ "\[deken\] installed version \[%1\$s\] > %2\$s...skipping!" ] ${::deken::version} ${version} ]
                ::pdwindow::debug "${msg}\n"
                return 0
            }
            if { ${x} < ${y} } {
                set msg [format [_ "\[deken\] installed version \[%1\$s\] < %2\$s...overwriting!" ] ${::deken::version} ${version} ]
                ::pdwindow::debug "${msg}\n"
                set ::deken::version ${version}
                return 1
            }
        }
        set msg [format [_ "\[deken\] installed version \[%1\$s\] == %2\$s...skipping!" ] ${::deken::version} ${version} ]
        ::pdwindow::debug "${msg}\n"
        return 0
    }
    set ::deken::version ${version}
    return 1
}

## put the current version of this package here:
if { [::deken::versioncheck 0.10.6] } {

namespace eval ::deken:: {
    namespace export open_searchui
    variable winid .externals_searchui
    variable resultsid ${winid}.results
    variable infoid ${winid}.results

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
namespace eval ::deken::search::dekenserver { }

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
set ::deken::preferences::installpath_x {}
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
set ::deken::preferences::use_url_primary 1
set ::deken::preferences::use_urls_secondary 0
set ::deken::preferences::urls_secondary {}
set ::deken::preferences::use_urls_ephemeral 0
set ::deken::preferences::urls_ephemeral {}

set ::deken::platform(os) ${::tcl_platform(os)}
set ::deken::platform(machine) [string tolower ${::tcl_platform(machine)}]
set ::deken::platform(bits) [ expr {[ string length [ format %X -1 ] ] * 4} ]
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
set ::deken::architecture_substitutes(powerpc) [list "ppc"]
set ::deken::architecture_substitutes(ppc) [list "powerpc"]

set ::deken::architecture_normalize(x86_64) "amd64"
set ::deken::architecture_normalize(i686) "i386"
set ::deken::architecture_normalize(i586) "i386"
set ::deken::architecture_normalize(i486) "i386"
set ::deken::architecture_normalize(armv6l) "armv6"
set ::deken::architecture_normalize(armv7l) "armv7"
set ::deken::architecture_normalize(powerpc) "ppc"

# normalize W32 OSs
if { [ string match "Windows *" "${::deken::platform(os)}" ] > 0 } {
    # we are not interested in the w32 flavour, so we just use 'Windows' for all of them
    set ::deken::platform(os) "Windows"
}
# normalize W32 CPUs
if { "Windows" eq "${::deken::platform(os)}" } {
    # in redmond, intel only produces 32bit CPUs,...
    if { "intel" eq "${::deken::platform(machine)}" } { set ::deken::platform(machine) "i686" }
    # ... and all 64bit CPUs are manufactured by amd
    #if { "amd64" eq "$::deken::platform(machine)" } { set ::deken::platform(machine) "x86_64" }
}

catch {
    set ::deken::platform(machine) $::deken::architecture_normalize($::deken::platform(machine))
}

# ######################################################################
# ################ compatibility #######################################
# ######################################################################

# list-reverter (compat for tcl<8.5)
if {[info commands lreverse] == ""} {
    proc lreverse list {
        set res {}
        set i [llength ${list}]
        while {${i}} {
            lappend res [lindex ${list} [incr i -1]]
        }
        set res
    } ;# RS
}

# ######################################################################
# ################ utilities ##########################################
# ######################################################################
proc ::deken::utilities::setdefault {key value} {
    upvar ${key} k
    if { [info exists k] } {
        set k
    } else {
        set k ${value}
    }
}

proc ::deken::utilities::bool {value {fallback 0}} {
    catch {set fallback [expr {bool(${value})} ] } stdout
    return ${fallback}
}
proc ::deken::utilities::int {value {fallback 0}} {
    catch {set fallback [expr {int(${value})} ] } stdout
    return ${fallback}
}
proc ::deken::utilities::tristate {value {offset 0} {fallback 0} } {
    catch {set fallback [expr {(int(${value}) + int(${offset}))% 3} ]} stdout
    return ${fallback}
}

proc ::deken::utilities::list_unique {lst} {
    array set cache {}
    set result {}
    foreach element ${lst} {
        if { ! [info exists cache(${element})]} {
            lappend result ${element}
        }
        set cache(${element}) 1
    }
    return ${result}
}
proc ::deken::utilities::lists_intersect {args} {
    set numlists [llength ${args}]
    if {${numlists} < 1} {return {}}

    set cache("") 0
    set elements {}
    foreach lst ${args} {
        set lst [::deken::utilities::list_unique ${lst}]
        foreach element ${lst} {
            if { ! [info exists cache(${element})]} {
                lappend elements ${element}
            }
            incr cache(${element})
        }
    }
    # ${elements} holds a list of unique elements (as they appeared)
    # so filter out those that were not in all lists
    set cache("") 0
    set result {}
    foreach element ${elements} {
        if { ${numlists} == $cache(${element}) } {
            lappend result ${element}
        }
    }
    return ${result}
}

proc ::deken::utilities::expandpath {path} {
    set map "@PD_PATH@"
    lappend map ${::sys_libdir}
    string map ${map} ${path}
}

proc ::deken::utilities::get_tmpfilename {{path ""} {ext ""} {prefix dekentmp}} {
    for {set i 0} {true} {incr i} {
        set tmpfile [file join ${path} ${prefix}.${i}${ext}]
        if {![file exists ${tmpfile}]} {
            return ${tmpfile}
        }
    }
}

proc ::deken::utilities::get_tmpdir {} {
    proc _iswdir {d} { return [expr {[file isdirectory ${d}] * [file writable ${d}]}] }
    set tmpdir ""
    # TRASH_FOLDER: very old Macintosh. Mac OS X doesn't have this.
    # TMPDIR: unices
    # TMP, TEMP: windows
    # TEPMDIR: for symmetry :-)
    foreach {d} {TRASH_FOLDER TMPDIR TEMPDIR TEMP TMP} {
        if { [info exists ::env(${d}) ] } {
            set tmpdir $::env(${d})
            if {[_iswdir ${tmpdir}]} {return ${tmpdir}}
        }
    }
    set tmpdir "/tmp"
    if {[_iswdir ${tmpdir}]} {return ${tmpdir}}
    set tmpdir [pwd]
    if {[_iswdir ${tmpdir}]} {return ${tmpdir}}
}

proc ::deken::utilities::is_writabledir {path} {
    set fs [file separator]
    set access [list RDWR CREAT EXCL TRUNC]
    set tmpfile [::deken::utilities::get_tmpfilename ${path}]
    # try creating tmpfile
    if {![catch {open ${tmpfile} ${access}} channel]} {
        close ${channel}
        file delete ${tmpfile}
        return true
    }
    return false
}

proc ::deken::utilities::get_writabledir {paths} {
    foreach p ${paths} {
        set xp [ ::deken::utilities::expandpath ${p} ]
        if { [ ::deken::utilities::is_writabledir ${xp} ] } { return ${p} }
    }
    return
}

proc ::deken::utilities::rmrecursive {path} {
    # recursively remove ${path} if it exists, traversing into each directory
    # to delete single items (rather than just unlinking the parent directory)
    set errors 0
    set myname [lindex [info level 0] 0]
    set children [glob -nocomplain -directory ${path} -types hidden *]
    lappend children {*}[glob -nocomplain -directory ${path} *]
    foreach child $children[set children {}] {
        if {[file tail ${child}] in {. ..}} {
            continue
        }
        if {[file isdirectory ${child}]} {
            if {[file type ${child}] ne "link"} {
                incr errors [${myname} ${child}]
            }
        }
        if { [ catch { file delete -force ${child} } ] } {
            incr errors
        }
    }
    return ${errors}
}

# http://rosettacode.org/wiki/URL_decoding#Tcl
proc ::deken::utilities::urldecode {str} {
    set specialMap {"[" "%5B" "]" "%5D"}
    set seqRE {%([0-9a-fA-F]{2})}
    set replacement {[format "%c" [scan "\1" "%2x"]]}
    set modStr [regsub -all ${seqRE} [string map ${specialMap} ${str}] ${replacement}]
    encoding convertfrom utf-8 [subst -nobackslashes -novariables ${modStr}]
}

proc ::deken::utilities::verbose {level message} {
    ::pdwindow::verbose ${level} "\[deken\] ${message}\n"
}
proc ::deken::utilities::debug {message} {
    set winid ${::deken::winid}
    if {[winfo exists ${winid}.tab.info]} {
        ::deken::post ${message} debug
    } else {
        ::pdwindow::debug "\[deken\] ${message}\n"
    }
}

if { [catch {package require tkdnd} ] } {
    proc ::deken::utilities::dnd_init {windowid} { }
} else {
proc ::deken::utilities::dnd_init {windowid} {
    ::tkdnd::drop_target register ${windowid} DND_Files
    bind ${windowid} <<Drop:DND_Files>> {::deken::utilities::dnd_drop_files %D}
}
proc ::deken::utilities::dnd_drop_files {files} {
    foreach f ${files} {
        if { [regexp -all -nocase "\.(zip|dek|tgz|tar\.gz)$" ${f} ] } {
            set msg [format [_ "installing deken package '%s'" ] ${f}]
            ::deken::statuspost ${msg}
            ::deken::install_package_from_file ${f}
        } else {
            set msg [format [_ "ignoring '%s': doesn't look like a deken package" ] ${f}]
            ::deken::statuspost ${msg}
        }
    }
    return "link"
}
}

namespace eval ::deken::utilities::unzipper:: {
    # we put a number of unzip methods in this namespace.
    # they are tried in in alphabetical(!) order


    # zipfs from Tcl>=8.7 (it seems this only works with Tcl>=9.0)
    catch {
        zipfs root
        proc builtin_zipfs {zipfile path} {
            if { [catch {
                set base [file join [zipfs root] deken]
                if { [catch  {
                    package require Tcl 8
                    # yikes! Tcl8.7 uses 'zipfs mount <mountpoint> <zipfile>'
                    zipfs mount ${base} ${zipfile}
                } ] } {
                    # and Tcl9 uses 'zipfs mount <zipfile> <mountpoint>'
                    zipfs mount ${zipfile} ${base}
                }
                foreach x [glob [file join ${base} *]] {
                    file copy -force -- ${x} ${path}
                }
                zipfs unmount ${base}
            } stdout ] } {
                ::deken::utilities::debug "unzipper::zipfs: ${stdout}"
                return 0
            }
            return 1
        }
    }

    # ::zipfile::decode from tcllib
    catch {
        package require zipfile::decode
        proc tcllib_zipfile {zipfile path} {
            if { [catch {
                ::zipfile::decode::unzipfile "${zipfile}" "${path}"
                ::zipfile::decode::open "${zipfile}"
                set zdict [::zipfile::decode::archive]
                ::zipfile::decode::close
                foreach zfile [::zipfile::decode::files ${zdict}] {
                    set zfile [file join ${path} ${zfile}]
                    catch {
                        if { ! [file attributes ${zfile} -permissions] } {
                            if [file isdirectory ${zfile} ] {
                                set perms 0700
                            } else {
                                set perms 0600
                            }
                            file attributes ${zfile} -permissions ${perms}
                        }
                    }
                }
            } stdout ] } {
                ::deken::utilities::debug "unzipper::zipfile::decode: ${stdout}"
                return 0
            }
            return 1
        }
    }

    proc untar {zipfile path} {
        if { [catch {
            exec tar -xf "${zipfile}" -C "${path}"
        } stdout ] } {
            ::deken::utilities::debug "unzipper::tar ${stdout}"
            return 0
        }
        return 1
    }

    proc unzip {zipfile path} {
        set result 0
        if { [catch {
            exec unzip -uo "${zipfile}" -d "${path}"
            set result 1
        } stdout ] } {
            ::deken::utilities::debug "unzipper::unzip ${stdout}"
        }
        return ${result}
    }

    if {$::tcl_platform(platform) eq "windows"} {
        ## VisualBasic is w32 only
        proc windows_visualbasic {zipfile path} {
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
            if {![catch {set fileId [open ${vbsscript} "w"]}]} {
                puts ${fileId} ${script}
                close ${fileId}
            }

            if {![file exists ${vbsscript}]} {
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
                ::deken::utilities::debug "unzipper::VBS-unzip(${vbsscript}): ${stdout}"
                return 0
            }
            catch { file delete "${vbsscript}" }
            return 1
        }
    }
}

proc ::deken::utilities::unzipper {zipfile {path .}} {
    set unzippers [lsort -dictionary [info procs ::deken::utilities::unzipper::*]]
    foreach unzip ${unzippers} {
        set result 0
        catch {
            set result [ ${unzip} ${zipfile} ${path} ]
        }
        if {${result} ne 0} {
            return 1
        }
    }
    return 0
}

proc ::deken::utilities::extract {installdir filename fullpkgfile {keep_package 1}} {
    if { ! [ file isdirectory "${installdir}" ] } {
        return 0
    }
    ::deken::statuspost [format [_ "Installing '%s'" ] ${filename} ] debug
    set PWD [ pwd ]
    cd ${installdir}
    set success 1
    if { [ string match *.dek ${fullpkgfile} ] } then {
        if { ! [ ::deken::utilities::unzipper ${fullpkgfile}  ${installdir} ] } {
            if { [ catch { exec unzip -uo ${fullpkgfile} } stdout ] } {
                ::deken::utilities::debug "${stdout}"
                set success 0
            }
        }
    } elseif { [ string match *.zip ${fullpkgfile} ] } then {
        if { ! [ ::deken::utilities::unzipper ${fullpkgfile}  ${installdir} ] } {
            if { [ catch { exec unzip -uo ${fullpkgfile} } stdout ] } {
                ::deken::utilities::debug "${stdout}"
                set success 0
            }
        }
    } elseif { [ string match *.tar.* ${fullpkgfile} ]
                || [ string match *.tgz ${fullpkgfile} ]
              } then {
        if { [ catch { exec tar xf ${fullpkgfile} } stdout ] } {
            ::deken::utilities::debug "${stdout}"
            set success 0
        }
    }
    cd ${PWD}

    if { ${success} > 0 } {
        ::deken::post [format [_ "Successfully unzipped %1\$s into %2\$s."] ${filename} ${installdir} ] debug
        if { ! "${keep_package}" } {
            catch { file delete ${fullpkgfile} }
        }
    } else {
        # Open both the fullpkgfile folder and the zipfile itself
        # NOTE: in tcl 8.6 it should be possible to use the zlib interface to actually do the unzip
        set msg [_ "Unable to extract package automatically." ]
        ::deken::post "${msg}" warn
        ::pdwindow::error "${msg}\n"
        set msg ""
        append msg [_ "Please perform the following steps manually:" ]
        append msg "\n"
        append msg [format [_ "1. Unzip %s." ]  ${fullpkgfile} ]
        append msg "\n"
        if { [string match "*.dek" ${fullpkgfile}] } {
            append msg "  "
            append msg [_ "You might need to change the file-extension from .dek to .zip" ]
            append msg "\n"
        }
        append msg [format [_ "2. Copy the contents into %s." ] ${installdir}]
        append msg "\n"
        append msg [format [_ "3. Remove %s. (optional)" ]  ${fullpkgfile} ]
        append msg "\n"

        ::deken::post "${msg}"

        pd_menucommands::menu_openfile ${fullpkgfile}
        pd_menucommands::menu_openfile ${installdir}
    }
    return ${success}
}

proc ::deken::utilities::uninstall {path library} {
    # recursively remove ${path}/${library} if it exists
    set fullpath [file join ${path} ${library}]
    if {[file exists ${fullpath}]} {
        ::deken::post [format [_ "Removing '%s'" ] ${fullpath} ] debug
        if { [catch {
            file delete -force "${fullpath}"
        } stdout ] } {
            set msg [format [_ "Uninstalling %1\$s from %2\$s failed!"] ${library} ${path}]
            ::deken::utilities::debug "${msg}\n    ${stdout}"
            return 0
        }
    }
    return 1
}
namespace eval ::deken::utilities::sha256:: {
    # we put a number of sha256sum methods in this namespace
    # they are tried in alphabetical order

    proc sha256sum {filename} {
        set hash {}
        catch { set hash [lindex [exec sha256sum ${filename}] 0] }
        return ${hash}
    }
    proc shasum {filename} {
        set hash {}
        catch { set hash [lindex [exec shasum -a 256 ${filename}] 0] }
        return ${hash}
    }
    if {$::tcl_platform(platform) eq "windows"} {
    proc winpowershell {filename} {
        set batscript [::deken::utilities::get_tmpfilename [::deken::utilities::get_tmpdir] ".bat" ]
        set script {
            @echo off
            powershell -Command " & {Get-FileHash -Algorithm SHA256 -LiteralPath \"%1\"  | Select-Object -ExpandProperty Hash}"
        }
        if {![catch {set fileId [open ${batscript} "w"]}]} {
            puts ${fileId} ${script}
            close ${fileId}
        }

        if {![file exists ${batscript}]} {
            ## still no script, give up
            return ""
        }

        if { [ catch {
            set hash [exec "${batscript}" "${filename}"]
        } stdout ] } {
            # ouch, couldn't run powershell script
            ::deken::utilities::verbose 1 "sha256.ps1 error: ${stdout}"
            set hash ""
        }

        catch { file delete "${batscript}" }

        return ${hash}
    }
    proc windows {filename} {
        set hash {}
        catch {
            regexp {([a-fA-F0-9]{64})} [exec certUtil -hashfile ${filename} SHA256] hash
        }
        return ${hash}
    }
    }
    catch {
        package require sha256
        proc ZZZtcllib {filename} {
            # the tcllib implementation comes last, as it is really slow...
            set hash {}
            catch { set hash [::sha2::sha256 -hex -filename ${filename}] }
            return ${hash}
        }
    }
}


proc ::deken::utilities::verify_sha256 {url pkgfile} {
    set msg [format [_ "Skipping SHA256 verification of '%s'." ] ${url} ]
    ::deken::statuspost ${msg}
    return -100
}

foreach impl [lsort -dictionary [info procs ::deken::utilities::sha256::*]] {
    # skip any $impl that does not return a valid sha256 (or even throws an error)
    if { [catch {
        if { [${impl} ${::argv0}] eq "" } {
            continue
        }
    }] } {
        continue
    }

    # short name for the actually used interpreter
    interp alias {} ::deken::utilities::sha256 {} ${impl}

    proc ::deken::utilities::verify_sha256 {url pkgfile} {
        ::deken::statuspost [format [_ "SHA256 verification of '%s'" ] ${pkgfile} ] debug
        ::deken::syncgui

        set retval 1
        set isremote 1
        set hashfile ""

        # check if ${url} really is a local file
        if { [file normalize ${url}] eq ${url} } {
            # ${url} is really an absolute filename
            # use it, if it exists
            set hashfile "${url}.sha256"
            set isremote 0
            if { [file isfile ${hashfile} ] && [file readable ${hashfile}] } { } else {
                set msg [format [_ "Unable to fetch reference SHA256 for '%s'." ] ${url} ]
                ::deken::utilities::verbose 0 ${msg}
                ::deken::statuspost ${msg} warn 0
                return -10
            }
        } else {
            # otherwise fetch it from the internet
            if { [ catch {
                set hashfile [::deken::utilities::download_file ${url}.sha256 [::deken::utilities::get_tmpfilename [::deken::utilities::get_tmpdir] ".sha256" ] ]
            } stdout ] } {
                ::deken::utilities::verbose 0 "${stdout}"
                # unable to download
                set msg [format [_ "Unable to fetch reference SHA256 for '%s'." ] ${url} ]
                ::deken::utilities::verbose 0 ${msg}
                ::deken::statuspost ${msg} warn 0
                return -10
            }
        }
        if { "${hashfile}" eq "" } {
            set retval -10
        }

        if { [ catch {
            set fp [open ${hashfile} r]
            set reference [string trim [string tolower [read ${fp}] ] ]
            close ${fp}
            if { ${isremote} } {
                catch { file delete ${hashfile} }
            }

            # get hash of file
            set hash [::deken::utilities::sha256 ${pkgfile} ]
            set hash [string trim [string tolower ${hash} ] ]

            if { ${hash} == "" } {
                set msg [format [_ "Skipping SHA256 verification of '%s'." ] ${url} ]
                ::deken::statuspost ${msg}
                return -100
            }

            # check if hash is sane
            if { [string length ${hash}] != 64 || ! [string is xdigit ${hash}] } {
                ::deken::statuspost [format [_ "File checksum looks invalid: '%s'." ] ${hash}] warn 0
            }
            # check if reference is sane
            if { [string length ${reference}] != 64 || ! [string is xdigit ${reference}] } {
                # this is more grave than the sanity check for the file hash
                # (since for the file hash we depend on the user's machine being able to
                # produce a proper SHA256 hash)
                ::deken::statuspost [format [_ "Reference checksum looks invalid: '%s'." ] ${reference}] error 0
            }

            if { [string first ${reference} ${hash}] >= 0 } {
                set retval 1
            } else {
                # SHA256 verification failed...
                set retval 0
            }
        } stdout ] } {
            ::deken::utilities::verbose 0 "${stdout}"
            # unable to verify
            set msg [format [_ "Unable to perform SHA256 verification for '%s'." ] ${url} ]
            ::deken::utilities::verbose 0 ${msg}
            ::deken::statuspost ${msg} warn 0
            set retval -20
        }
        return ${retval}
    }
    # it seems we found a working sha256 implementation, don't try the other ones...
    break
}

proc ::deken::utilities::httpuseragent {} {
    set httpagent [::http::config -useragent]
    set appname "Pd"
    if {[info exists ::PD_APPLICATION_NAME]} {
        set appname ${::PD_APPLICATION_NAME}
    }
    set pdversion "${appname}/${::PD_MAJOR_VERSION}.${::PD_MINOR_VERSION}.${::PD_BUGFIX_VERSION}${::PD_TEST_VERSION}"
    set platformstring [::deken::platform2string]
    set tclversion "Tcl/[info patchlevel]"
    ::http::config -useragent "Deken/${::deken::version} (${platformstring}) ${pdversion} ${tclversion}"
    return ${httpagent}
}


# download a file to a location
# http://wiki.tcl.tk/15303
proc ::deken::utilities::download_file {url outputfilename {progressproc {}}} {
    set URL [string map {{[} "%5b" {]} "%5d"} ${url}]
    set downloadfilename [::deken::utilities::get_tmpfilename [file dirname ${outputfilename}] ]
    set f [open ${downloadfilename} w]
    fconfigure ${f} -translation binary

    set httpagent [::deken::utilities::httpuseragent]
    if { [catch {
        if { ${progressproc} eq {} } {
            set httpresult [::http::geturl ${URL} -binary true -channel ${f}]
        } else {
            set httpresult [::http::geturl ${URL} -binary true -progress ${progressproc} -channel ${f}]
        }
        set ncode [::http::ncode ${httpresult}]
        if {${ncode} != 200} {
            ## FIXXME: we probably should handle redirects correctly (following them...)
            set err [::http::code ${httpresult}]
            set msg [format [_ "Unable to download from %1\$s \[%2\$s\]" ] ${url} ${err} ]
            ::deken::post "${msg}" debug
            set outputfilename ""
        }
        ::http::cleanup ${httpresult}
    } stdout ] } {
        set msg [format [_ "Unable to download from '%s'!" ] ${url} ]
        tk_messageBox \
            -title [_ "Download failed" ] \
            -message "${msg}\n${stdout}" \
            -icon error -type ok \
            -parent ${::deken::winid}
        set outputfilename ""
    }
    ::http::config -useragent ${httpagent}

    flush ${f}
    close ${f}

    if { "${outputfilename}" != "" } {
        catch { file delete ${outputfilename} }
        if {[file exists ${outputfilename}]} {
            ::deken::utilities::debug [format [_ "Unable to remove stray file '%s'" ] ${outputfilename} ]
            set outputfilename ""
        }
    }
    if { ${outputfilename} != "" && "${outputfilename}" != "${downloadfilename}" } {
        if {[catch { file rename ${downloadfilename} ${outputfilename}}]} {
            ::deken::utilities::debug [format [_ "Unable to rename downloaded file to '%s'" ] ${outputfilename} ]
            set outputfilename ""
        }
    }
    if { "${outputfilename}" eq "" } {
        file delete ${downloadfilename}
    }

    return ${outputfilename}
}


# parse a deken-packagefilename into it's components:
# v0:: <pkgname>[-v<version>-]?{(<arch>)}-externals.<ext>
# v1:: <pkgname>[\[<version\]]?{(<arch>)}
# return: list <pkgname> <version> [list <arch> ...]
proc ::deken::utilities::parse_filename {filename} {
    set pkgname ${filename}
    set archs [list]
    set version ""
    if { [ string match "*.dek" ${filename} ] } {
        ## deken filename v1: <library>[v<version>](<arch1>)(<arch2>).dek
        set archstring ""
        if { ! [regexp {^([^\[\]\(\)]+)((\[[^\[\]\(\)]+\])*)((\([^\[\]\(\)]+\))*).*\.dek$} ${filename} _ pkgname optionstring _ archstring] } {
            # oops, somewhat unparseable (e.g. a copy)

        } else {
            foreach {o _} [lreplace [split ${optionstring} {[]}] 0 0] {
                if {![string first v ${o}]} {
                    set version [string range ${o} 1 end]
                } else { # ignoring unknown option...
                    return [list {} {} {}]
                }
            }
            foreach {a _} [lreplace [split ${archstring} "()"] 0 0] { lappend archs ${a} }
        }
    } elseif { [ regexp {(.*)-externals\..*} ${filename} _ basename] } {
        ## deken filename v0
        set pkgname ${basename}
        # basename <pkgname>[-v<version>-]?{(<arch>)}
        ## strip off the archs
        set baselist [split ${basename} () ]
        # get pkgname + version
        set pkgver [lindex ${baselist} 0]
        if { ! [ regexp "(.*)-v(.*)-" ${pkgver} _ pkgname version ] } {
            set pkgname ${pkgver}
            set version ""
        }
        # get archs
        foreach {a _} [lreplace ${baselist} 0 0] {
            # in filename.v0 the semantics of the last arch field ("bits") was unclear
            # since this format predates float64 builds, we just force it to 32
            regsub -- {-[0-9]+$} ${a} {-32} a
            lappend archs ${a}
        }
        if { "x${archs}${version}" == "x" } {
            # try again as <pkgname>-v<version>
            if { ! [ regexp "(.*)-v(.*)" ${pkgver} _ pkgname version ] } {
                set pkgname ${pkgver}
                set version ""
            }
        }
    }
    return [list ${pkgname} ${version} ${archs}]
}

# split filename extension from deken-packagefilename
proc ::deken::utilities::get_filenameextension {filename} {
    if { [ regexp {.*(\.tar\.[^.]*)$} ${filename} _ ext ] } {
        return ${ext}
    }
    return [file extension ${filename}]
}



# ######################################################################
# ################ preferences #########################################
# ######################################################################

proc ::deken::preferences::create_sources_entry {toplevel} {
    # 3 panels
    # - [ ] primary server
    # - [ ] secondary URLs (list editable)
    #       (all URLs are searched if checked)
    # - [ ] ephemeral URLs (list multiselectable)
    #       (if checked, and URLs are selected, only these are searched)
    #       (if checked, and no URLs are selected, all are searched)
    set frame [::deken::preferences::newwidget ${toplevel}.servers]
    labelframe ${frame} -text [_ "Search URLs:" ] -padx 5 -pady 5 -borderwidth 1
    set numframes 0

    if {1} {
    set f [::deken::preferences::newwidget ${frame}.primary]
    # primary URL
    labelframe ${f} -borderwidth 0
    checkbutton ${f}.use -text "${::deken::search::dekenserver::url_primary}" \
        -variable ::deken::preferences::use_url_primary
    ${f} configure -labelwidget ${f}.use
    pack ${f} -anchor "w" -fill both -expand 1 -side top
    pack ${f}.use -anchor "w" -side left
    incr numframes
    }

    if {0} {
    set f [::deken::preferences::newwidget ${frame}.secondary]
    # secondary URLs
    labelframe ${f} -borderwidth 0
    checkbutton ${f}.use \
        -variable ::deken::preferences::use_urls_secondary
        button ${f}.open \
        -command [list ::deken::preferences::urls2_frame_create ${toplevel}] \
        -text [_ "Additional search URLs" ]
    ${f} configure -labelwidget ${f}.use
    pack ${f} -anchor "w" -fill both -expand 1 -side left
    pack ${f}.use -anchor "w" -side left
    pack ${f}.open -anchor "w" -side left
    incr numframes
    }

    if {0} {
    set f [::deken::preferences::newwidget ${frame}.ephemeral]
    # ephemeral URLs
    labelframe ${f} -borderwidth 0
    checkbutton ${f}.use  \
        -variable ::deken::preferences::use_urls_ephemeral
    button ${f}.open -text [_ "Ephemeral search URLs" ]
    ${f} configure -labelwidget ${f}.use
    pack ${f} -anchor "w" -fill both -expand 1 -side left
    pack ${f}.use -anchor "w" -side left
    pack ${f}.open -anchor "w" -side left
    incr numframes
    }
    if { ${numframes} > 1 } {
        return ${frame}
    }
    return ""
}

proc ::deken::preferences::newwidget {basename} {
    # calculate a widget name that has not yet been taken
    set i 0
    while {[winfo exists ${basename}${i}]} {incr i}
    return ${basename}${i}
}

proc ::deken::preferences::urls2_frame_create {toplevel} {
    set title [_ "Additional search URLs"]
    set win ${toplevel}.urls2
    destroy $win
    toplevel $win
    wm title $win "deken: ${title}"
    ::scrollbox::make ${win} ${::deken::preferences::urls_secondary} {} {}
    #::deken::preferences::make_applybutton_frame ${win} \
    #    [list ::deken::preferences::urls2_frame_apply ${win}]

    set cmd [list ::scrollboxwindow::apply ${win} ::deken::preferences::set_urls_secondary]
    ::deken::preferences::make_applybutton_frame ${win} \
        "${cmd}; destroy ${win}"

    bind ${win} <Escape> [list after idle [list ::deken::preferences::cancel $win]]

}
proc ::deken::preferences::set_urls_secondary {urls} {
    set ::deken::preferences::urls_secondary ${urls}
}

proc ::deken::preferences::create_pathpad {toplevel row {padx 2} {pady 2}} {
    set pad [::deken::preferences::newwidget ${toplevel}.pad]
    frame ${pad} -relief groove -borderwidth 2 -width 2 -height 2
    grid ${pad} -sticky ew -row ${row} -column 0 -columnspan 3 -padx ${padx}  -pady ${pady}
}
proc ::deken::preferences::create_packpad {toplevel {padx 2} {pady 2} } {
    set mypad [::deken::preferences::newwidget ${toplevel}.pad]

    frame ${mypad}
    pack ${mypad} -padx ${padx} -pady ${pady} -expand 1 -fill "y"

    return ${mypad}
}

proc ::deken::preferences::userpath_doit { winid } {
    set installdir [::deken::do_prompt_installdir ${::deken::preferences::userinstallpath} ${winid}]
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
        if { ${mkdir} } {
            catch { file mkdir ${path} }
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

proc ::deken::preferences::update_displaypath {var value} {
    set ${var} ${value}
}
proc ::deken::preferences::create_pathentry {toplevel row var path {generic false}} {
    # only add absolute paths to the pathentries
    set xpath [ ::deken::utilities::expandpath ${path} ]
    if {! ${generic}} {
        if { [file pathtype ${xpath}] != "absolute"} { return }
    }

    set rdb [::deken::preferences::newwidget ${toplevel}.path]
    set chk [::deken::preferences::newwidget ${toplevel}.doit]
    set pad [::deken::preferences::newwidget ${toplevel}.pad]

    radiobutton ${rdb} -value ${path} -text "${path}" -variable ${var} \
        -command [list ::deken::preferences::update_displaypath ${var}_x ${path}]
    frame ${pad}
    button ${chk} -text [_ "Browse" ] -command "::deken::preferences::path_doit ${rdb} ${chk} ${xpath}"

    grid ${rdb} -sticky "w"  -row ${row} -column 2
    grid ${pad} -sticky ""   -row ${row} -column 1 -padx 10
    grid ${chk} -sticky nsew -row ${row} -column 0


    if {! ${generic}} {
        ::deken::preferences::path_doit ${rdb} ${chk} ${xpath} false
    }
    return [list ${rdb} ${chk}]
}

proc ::deken::preferences::fill_frame {winid} {
    ::deken::preferences::create ${winid}
}
proc ::deken::preferences::create_pathframe {cnv winid} {
    canvas ${cnv}.cnv \
        -confine true
    scrollbar ${cnv}.scrollv \
        -command "${cnv}.cnv yview"
    scrollbar ${cnv}.scrollh \
        -orient horizontal \
        -command "${cnv}.cnv xview"
    ${cnv}.cnv configure \
        -xscrollincrement 0 \
        -xscrollcommand " ${cnv}.scrollh set"
    ${cnv}.cnv configure \
        -yscrollincrement 0 \
        -yscrollcommand " ${cnv}.scrollv set" \

    pack ${cnv}.cnv -side left -fill both -expand 1
    pack ${cnv}.scrollv -side right -fill "y"
    pack ${cnv}.scrollh -side bottom -fill "x" -before  ${cnv}.cnv

    set pathsframe [frame ${cnv}.cnv.f]
    set row 0
    ### dekenpath: directory-chooser
    # FIXME: should we ask user to add chosen directory to PATH?
    set pathdoit [::deken::preferences::create_pathentry ${pathsframe} ${row} ::deken::preferences::installpath "USER" true]
    incr row
    [lindex ${pathdoit} 0] configure \
        -foreground blue \
        -value "USER" \
        -textvariable ::deken::preferences::userinstallpath \
        -variable ::deken::preferences::installpath \
        -command {::deken::preferences::update_displaypath ::deken::preferences::installpath_x ${::deken::preferences::userinstallpath}}

    [lindex ${pathdoit} 1] configure \
        -text [_ "Browse" ] \
        -command "::deken::preferences::userpath_doit ${winid}"

    ### dekenpath: default directories
    if {[namespace exists ::pd_docsdir] && [::pd_docsdir::externals_path_is_valid]} {
        set xpath [::pd_docsdir::get_externals_path]
        if { [llength ${xpath}] } {
            ::deken::preferences::create_pathpad ${pathsframe} ${row}
            incr row
        }

        foreach p ${xpath} {
            ::deken::preferences::create_pathentry ${pathsframe} ${row} ::deken::preferences::installpath ${p}
            incr row
        }
    }
    if { [llength ${::sys_staticpath}] } {
        ::deken::preferences::create_pathpad ${pathsframe} ${row}
        incr row
    }
    set extradir [file join ${::sys_libdir} extra ]
    foreach p ${::sys_staticpath} {
        if { [file normalize ${p}] == ${extradir} } {
            set p [file join @PD_PATH@ extra]
        }
        ::deken::preferences::create_pathentry ${pathsframe} ${row} ::deken::preferences::installpath ${p}
        incr row
    }

    if {[llength ${::sys_searchpath}]} {
        ::deken::preferences::create_pathpad ${pathsframe} ${row}
        incr row
    }

    foreach p ${::sys_searchpath} {
        ::deken::preferences::create_pathentry ${pathsframe} ${row} ::deken::preferences::installpath ${p}
        incr row
    }

    pack ${pathsframe} -fill "x"
    ${cnv}.cnv create window 0 0 -anchor "nw" -window ${pathsframe}
}
proc ::deken::preferences::create_pathwindow {parentwin} {
    set winid ${parentwin}.pathwindow
    if {[winfo exists ${winid}]} {
        wm deiconify ${winid}
        raise ${winid}
    } else {
        toplevel ${winid} -class DialogWindow
        bind ${winid} <Escape> {after idle {::deken::preferences::cancel %W}}
        wm title ${winid} [_ "Deken Installation Target"]

        frame ${winid}.frame
        pack ${winid}.frame -side top -padx 6 -pady 3 -fill both -expand true

        ::deken::preferences::create_pathframe ${winid}.frame ${parentwin}

        frame ${winid}.bframe
        pack ${winid}.bframe -side bottom -fill "x" -pady 2m
        button ${winid}.bframe.close -text [_ "Close" ] \
            -command [list ::deken::preferences::cancel ${winid}]
        pack ${winid}.bframe.close -side right -expand 1 -fill "x" -padx 15 -ipadx 10
    }
}



proc ::deken::preferences::create {winid} {
    # urgh...we want to know when the window gets drawn,
    # so we can query the size of the pathentries canvas
    # in order to get the scrolling-region right!!!
    # this seems to be so wrong...
    bind ${winid} <Map> "::deken::preferences::mapped %W"
    ::deken::bind_globalshortcuts ${winid}

    set ::deken::preferences::installpath ${::deken::installpath}
    set ::deken::preferences::installpath_x ${::deken::installpath}
    set ::deken::preferences::hideforeignarch ${::deken::hideforeignarch}
    set ::deken::preferences::hideoldversions ${::deken::hideoldversions}
    if { ${::deken::userplatform} == "" } {
        set ::deken::preferences::platform DEFAULT
        set ::deken::preferences::userplatform [ ::deken::platform2string ]
    } else {
        set ::deken::preferences::platform USER
        set ::deken::preferences::userplatform ${::deken::userplatform}
    }

    set ::deken::preferences::installpath USER
    set ::deken::preferences::userinstallpath ${::deken::installpath}

    set ::deken::preferences::show_readme ${::deken::show_readme}
    set ::deken::preferences::keep_package ${::deken::keep_package}
    set ::deken::preferences::verify_sha256 ${::deken::verify_sha256}
    set ::deken::preferences::remove_on_install ${::deken::remove_on_install}
    set ::deken::preferences::add_to_path ${::deken::add_to_path}
    set ::deken::preferences::add_to_path_temp ${::deken::preferences::add_to_path}

    set ::deken::preferences::urls_secondary ${::deken::search::dekenserver::urls_secondary}
    set ::deken::preferences::urls_ephemeral ${::deken::search::dekenserver::urls_ephemeral}

    set ::deken::preferences::use_url_primary ${::deken::search::dekenserver::use_url_primary}
    set ::deken::preferences::use_urls_secondary ${::deken::search::dekenserver::use_urls_secondary}
    set ::deken::preferences::use_urls_ephemeral ${::deken::search::dekenserver::use_urls_ephemeral}


    # this dialog allows us to select:
    #  - which directory to extract to
    #    - including all (writable) elements from ${::sys_staticpath}
    #      and option to create each of them
    #    - a directory chooser
    #  - whether to delete directories before re-extracting
    #  - whether to filter-out non-matching architectures
    labelframe ${winid}.installdir -text [_ "Install externals to directory:" ] \
        -borderwidth 1 -padx 5 -pady 5
    if { 1 } {
        set readonly_color [lindex [${winid} configure -background] end]
        pack ${winid}.installdir -fill x -anchor s
        # -padx {2m 4m} -pady 2m

        frame ${winid}.installdir.path
        pack ${winid}.installdir.path -fill x
        entry ${winid}.installdir.path.entry -textvariable ::deken::preferences::installpath_x \
            -takefocus 0 -state readonly -readonlybackground $readonly_color
        button ${winid}.installdir.path.browse -text [_ "Browse"] \
            -command [list ::deken::preferences::create_pathwindow ${winid}]
        pack ${winid}.installdir.path.browse -side right -fill x -ipadx 8
        pack ${winid}.installdir.path.entry -side right -expand 1 -fill x
        # scroll to right for long paths
        ${winid}.installdir.path.entry xview moveto 1
    } else {
        ::deken::preferences::create_pathframe ${winid}.installdir ${winid}
    }
    pack ${winid}.installdir -fill both


    ## installation options
    labelframe ${winid}.install -text [_ "Installation options:" ] -padx 5 -pady 5 -borderwidth 1
    pack ${winid}.install -side top -fill "x" -anchor "w"

    checkbutton ${winid}.install.verify256 -text [_ "Try to verify the libraries' checksum before (re)installing them"] \
        -variable ::deken::preferences::verify_sha256
    pack ${winid}.install.verify256 -anchor "w"

    checkbutton ${winid}.install.remove -text [_ "Try to remove libraries before (re)installing them"] \
        -variable ::deken::preferences::remove_on_install
    pack ${winid}.install.remove -anchor "w"

    checkbutton ${winid}.install.readme -text [_ "Show README of newly installed libraries (if present)"] \
        -variable ::deken::preferences::show_readme
    pack ${winid}.install.readme -anchor "w"

    checkbutton ${winid}.install.keeppackage -text [_ "Keep package files after installation"] \
        -variable ::deken::preferences::keep_package
    pack ${winid}.install.keeppackage -anchor "w"


    checkbutton ${winid}.install.add_to_path -text [_ "Add newly installed libraries to Pd's search path"] \
        -variable ::deken::preferences::add_to_path
    catch { ${winid}.install.add_to_path configure \
                -tristatevalue 1 \
                -onvalue 2 \
                -command {set ::deken::preferences::add_to_path \
                              [set ::deken::preferences::add_to_path_temp \
                                   [::deken::utilities::tristate ${::deken::preferences::add_to_path_temp} 1 0]]}
        set msg "- Always add to search path\n- Never add to search path\n- Prompt before adding"
        bind ${winid}.install.add_to_path <Enter> "::deken::balloon::show ${winid}.install_balloon %X \[winfo rooty %W\] \{${msg}\} 0 30"
        bind ${winid}.install.add_to_path <Leave> [list ::deken::balloon::hide ${winid}.install_balloon]

    } stdout

    pack ${winid}.install.add_to_path -anchor "w"


    ## platform filter settings
    labelframe ${winid}.platform -text [_ "Platform settings:" ] -padx 5 -pady 5 -borderwidth 1
    pack ${winid}.platform -side top -fill "x" -anchor "w"

    # default architecture vs user-defined arch
    radiobutton ${winid}.platform.default -value "DEFAULT" \
        -text [format [_ "Default platform: %s" ] [::deken::platform2string ] ] \
        -variable ::deken::preferences::platform \
        -command "${winid}.platform.userarch.entry configure -state disabled"
    pack ${winid}.platform.default -anchor "w"

    frame ${winid}.platform.userarch
    radiobutton ${winid}.platform.userarch.radio -value "USER" \
        -text [_ "User-defined platform:" ] \
        -variable ::deken::preferences::platform \
        -command "${winid}.platform.userarch.entry configure -state normal"
    entry ${winid}.platform.userarch.entry -textvariable ::deken::preferences::userplatform
    if { "${::deken::preferences::platform}" == "DEFAULT" } {
        ${winid}.platform.userarch.entry configure -state disabled
    }

    pack ${winid}.platform.userarch -anchor "w"
    pack ${winid}.platform.userarch.radio -side left
    pack ${winid}.platform.userarch.entry -side right -fill "x"

    # hide non-matching architecture?
    ::deken::preferences::create_packpad ${winid}.platform 2 10

    checkbutton ${winid}.platform.hide_foreign -text [_ "Hide foreign architectures"] \
        -variable ::deken::preferences::hideforeignarch
    pack ${winid}.platform.hide_foreign -anchor "w"
    checkbutton ${winid}.platform.only_newest -text [_ "Only show the newest version of a library\n(treats other versions like foreign architectures)"] \
        -variable ::deken::preferences::hideoldversions -justify "left"
    pack ${winid}.platform.only_newest -anchor "w"

    # search URLs
    set sourceframe [::deken::preferences::create_sources_entry ${winid}]
    if { ${sourceframe} ne {} } {
        pack ${sourceframe} -anchor "w" -fill both -expand 1
    }
}

proc ::deken::preferences::mapped {winid} {
    set cnv ${winid}.installdir.cnv
    catch {
        set bbox [${cnv} bbox all]
        if { "${bbox}" != "" } {
            ${cnv} configure -scrollregion ${bbox}
        }
    } stdout
}

proc ::deken::preferences::make_applybutton_frame {winid okproc {cancelproc {}} {applyproc {}}} {
    # the Apply/OK/Cancel buttons
    if {${::windowingsystem} eq "aqua"} {
        # no "Apply" on macOS...
        set applyproc {}
    }
    if { ${cancelproc} eq {} } {
        set cancelproc [list destroy ${winid}]
    }

    # Use two frames for the buttons, since we want them both bottom and right
    frame ${winid}.nb
    pack ${winid}.nb -side bottom -fill "x" -pady 2m

    # buttons
    frame ${winid}.nb.buttonframe
    pack ${winid}.nb.buttonframe -side right -fill "x" -padx 2m

    button ${winid}.nb.buttonframe.cancel -text [_ "Cancel"] \
        -command ${cancelproc}
    pack ${winid}.nb.buttonframe.cancel -side left -expand 1 -fill "x" -padx 15 -ipadx 10

    if { ${applyproc} ne {} } {
        button ${winid}.nb.buttonframe.apply -text [_ "Apply"] \
            -command ${applyproc}
        pack ${winid}.nb.buttonframe.apply -side left -expand 1 -fill "x" -padx 15 -ipadx 10
    }
    if { ${okproc} ne {} } {
        button ${winid}.nb.buttonframe.ok -text [_ "OK"] \
            -command ${okproc}
        pack ${winid}.nb.buttonframe.ok -side left -expand 1 -fill "x" -padx 15 -ipadx 10
    }
}

proc ::deken::preferences::show {{winid .deken_preferences}} {
    if {[winfo exists ${winid}]} {
        wm deiconify ${winid}
        raise ${winid}
    } else {
        toplevel ${winid} -class DialogWindow
        wm title ${winid} [format [_ "Deken %s Preferences"] ${::deken::version}]

        frame ${winid}.frame
        pack ${winid}.frame -side top -padx 6 -pady 3 -fill both -expand true

        bind ${winid} <Escape> {after idle {::deken::preferences::cancel %W}}
        ::deken::preferences::create ${winid}.frame


        # the Apply/OK/Cancel buttons
        ::deken::preferences::make_applybutton_frame ${winid} \
            [list ::deken::preferences::ok ${winid}] \
            [list ::deken::preferences::cancel ${winid}] \
            [list ::deken::preferences::apply ${winid}]
    }
}

proc ::deken::preferences::apply {winid} {
    set installpath "${::deken::preferences::installpath}"
    if { "${installpath}" == "USER" } {
        set installpath "${::deken::preferences::userinstallpath}"
    }

    ::deken::set_installpath "${installpath}"
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

    ::deken::set_search_urls \
        ${::deken::preferences::use_url_primary} \
        ${::deken::preferences::use_urls_secondary} \
        ${::deken::preferences::urls_secondary} \
        ${::deken::preferences::use_urls_ephemeral} \
        ${::deken::preferences::urls_ephemeral}
}
proc ::deken::preferences::cancel {winid} {
    ## FIXXME properly close the window/frame (for reuse in a tabbed pane)
    destroy ${winid}
}
proc ::deken::preferences::ok {winid} {
    ::deken::preferences::apply ${winid}
    ::deken::preferences::cancel ${winid}
}

# ######################################################################
# ################ core ################################################
# ######################################################################

if { [ catch { set ::deken::installpath [::pd_guiprefs::read dekenpath] } stdout ] } {
    # this is a Pd without the new GUI-prefs
    proc ::deken::set_installpath {installdir} {
        set ::deken::installpath ${installdir}
    }
    proc ::deken::set_platform_options {platform hideforeignarch {hideoldversions 0}} {
        set ::deken::userplatform ${platform}
        set ::deken::hideforeignarch [::deken::utilities::bool ${hideforeignarch} ]
        set ::deken::hideoldversions [::deken::utilities::bool ${hideoldversions} ]
    }
    proc ::deken::set_install_options {remove readme add keep verify256} {
        set ::deken::remove_on_install [::deken::utilities::bool ${remove}]
        set ::deken::show_readme [::deken::utilities::bool ${readme}]
        set ::deken::add_to_path [::deken::utilities::tristate ${add} 0 0]
        set ::deken::keep_package [::deken::utilities::bool ${keep}]
        set ::deken::verify_sha256 [::deken::utilities::bool ${verify256}]
    }
    proc ::deken::set_search_urls {use_primary use_secondaries secondaries use_ephemerals ephemerals} {
        set ::deken::search::dekenserver::use_url_primary [::deken::utilities::bool ${use_primary}]

        set ::deken::search::dekenserver::use_urls_secondary [::deken::utilities::bool ${use_secondaries}]
        set ::deken::search::dekenserver::urls_secondary ${secondaries}

        set ::deken::search::dekenserver::use_urls_ephemeral [::deken::utilities::bool ${use_ephemerals}]
        set ::deken::search::dekenserver::urls_ephemeral ${ephemerals}
    }
} else {
    catch {set ::deken::installpath [lindex ${::deken::installpath} 0]}
    # Pd has a generic preferences system, that we can use
    proc ::deken::set_installpath {installdir} {
        set ::deken::installpath ${installdir}
        ::pd_guiprefs::write dekenpath [list ${installdir}]
    }
    # user requested platform (empty = DEFAULT)
    set ::deken::userplatform [::pd_guiprefs::read deken_platform]
    catch {set ::deken::userplatform [lindex ${deken::userplatform} 0 ]}
    # urgh, on macOS an empty :deken::userplatform ({}, which is promoted to [list {}] on save)
    # got saved as a literal "{}" (actually "\\\\{\\\\}")
    # which then gets restored as "\\{\\}"...
    # the bogus write behaviour was fixed with v0.9.8, but we need to handle old prefs...
    set ::deken::userplatform [string trim [string trim ${::deken::userplatform} "\\\{\}" ] ]
    set ::deken::hideforeignarch [::deken::utilities::bool [::pd_guiprefs::read deken_hide_foreign_archs] 1]
    set ::deken::hideoldversions [::deken::utilities::bool [::pd_guiprefs::read deken_hide_old_versions] 1]
    proc ::deken::set_platform_options {platform hideforeignarch {hideoldversions 0}} {
        set ::deken::userplatform ${platform}
        if { ${platform} == "" } {
            set platformlist [list]
        } else {
            set platformlist [list ${platform}]
        }
        set ::deken::hideforeignarch [::deken::utilities::bool ${hideforeignarch} ]
        set ::deken::hideoldversions [::deken::utilities::bool ${hideoldversions} ]
        ::pd_guiprefs::write deken_platform ${platformlist}
        ::pd_guiprefs::write deken_hide_foreign_archs ${::deken::hideforeignarch}
        ::pd_guiprefs::write deken_hide_old_versions ${::deken::hideoldversions}
    }
    set ::deken::remove_on_install [::deken::utilities::bool [::pd_guiprefs::read deken_remove_on_install] 1]
    set ::deken::show_readme [::deken::utilities::bool [::pd_guiprefs::read deken_show_readme] 1]
    set ::deken::keep_package [::deken::utilities::bool [::pd_guiprefs::read deken_keep_package] 0]
    set ::deken::verify_sha256 [::deken::utilities::bool [::pd_guiprefs::read deken_verify_sha256] 1]
    set ::deken::add_to_path [::deken::utilities::tristate [::pd_guiprefs::read deken_add_to_path] ]

    proc ::deken::set_install_options {remove readme path keep verify256} {
        set ::deken::remove_on_install [::deken::utilities::bool ${remove}]
        set ::deken::show_readme [::deken::utilities::bool ${readme}]
        set ::deken::add_to_path [::deken::utilities::tristate ${path}]
        set ::deken::keep_package [::deken::utilities::bool ${keep}]
        set ::deken::verify_sha256 [::deken::utilities::bool ${verify256}]
        ::pd_guiprefs::write deken_remove_on_install "${::deken::remove_on_install}"
        ::pd_guiprefs::write deken_show_readme "${::deken::show_readme}"
        ::pd_guiprefs::write deken_add_to_path "${::deken::add_to_path}"
        ::pd_guiprefs::write deken_keep_package "${::deken::keep_package}"
        ::pd_guiprefs::write deken_verify_sha256 "${::deken::verify_sha256}"
    }
    set ::deken::search::dekenserver::use_url_primary [::deken::utilities::bool [pd_guiprefs::read dekensearch_useprimaryurl] 1]
    set ::deken::search::dekenserver::use_urls_secondary [::deken::utilities::bool [pd_guiprefs::read dekensearch_usesecondaryurls] 0]
    set ::deken::search::dekenserver::urls_secondary [pd_guiprefs::read dekensearch_secondaryurls]
    set ::deken::search::dekenserver::use_urls_ephemeral [::deken::utilities::bool [pd_guiprefs::read dekensearch_useephemeralurls] 0]
    set ::deken::search::dekenserver::urls_ephemeral [pd_guiprefs::read dekensearch_ephemeralurls]

    proc ::deken::set_search_urls {use_primary use_secondaries secondaries use_ephemerals ephemerals} {
        set ::deken::search::dekenserver::use_url_primary [::deken::utilities::bool ${use_primary}]

        set ::deken::search::dekenserver::use_urls_secondary [::deken::utilities::bool ${use_secondaries}]
        set ::deken::search::dekenserver::urls_secondary ${secondaries}

        set ::deken::search::dekenserver::use_urls_ephemeral [::deken::utilities::bool ${use_ephemerals}]
        set ::deken::search::dekenserver::urls_ephemeral ${ephemerals}

        ::pd_guiprefs::write dekensearch_useprimaryurl "${::deken::search::dekenserver::use_url_primary}"
        ::pd_guiprefs::write dekensearch_usesecondaryurls "${::deken::search::dekenserver::use_urls_secondary}"
        ::pd_guiprefs::write dekensearch_secondaryurls "${::deken::search::dekenserver::urls_secondary}"
        ::pd_guiprefs::write dekensearch_useephemeralurls "${::deken::search::dekenserver::use_urls_ephemeral}"
        ::pd_guiprefs::write dekensearch_ephemeralurls "${::deken::search::dekenserver::urls_ephemeral}"
    }
}

proc ::deken::normalize_result {title
                                cmd
                                {match 1}
                                {subtitle ""}
                                {statusline ""}
                                {contextcmd {}}
                                {pkgname ""}
                                {version ""}
                                {uploader ""}
                                {timestamp ""}
                                args} {
    ## normalize a search-result
    # the function parameters are guaranteed to be a stable API (with the exception of <args>)
    # but the value returned by this function is an implementation detail
    # <title> the primary line displayed for the search-result
    # - <cmd> the full command to run to install the library
    # - <match> boolean value to indicate whether this entry matches the current architecture
    # - <subtitle> additional text to be shown under the <name>
    # - <statusline> additional text to be shown in the STATUS line if the mouse hovers over the result
    # - <contextcmd> the full command to be executed when the user right-clicks the menu-entry
    # - <pkgname> the library name (typically this gets parsed from the package filename)
    # - <uploader> who provided the package
    # - <timestamp> the upload date of the package
    # - <args> RESERVED FOR FUTURE USE (this is a variadic placeholder. do not use!)

    return [list "" ${title} ${cmd} ${match} ${subtitle} ${statusline} ${contextcmd} ${pkgname} ${version} ${uploader} ${timestamp}]
}


# find an install path, either from prefs or on the system
# returns an empty string if nothing was found
proc ::deken::find_installpath {{ignoreprefs false}} {
    set installpath ""
    if { [ info exists ::deken::installpath ] && !${ignoreprefs} } {
        ## any previous choice?
        return ${::deken::installpath}
    }
    if { "${installpath}" == "" } {
        ## search the default paths
        set installpath [ ::deken::utilities::get_writabledir ${::sys_staticpath} ]
    }
    if { "${installpath}" == "" } {
        # let's use the first of ${::sys_staticpath}, if it does not exist yet
        set userdir [lindex ${::sys_staticpath} 0]
        if { ! [file exists ${userdir} ] } {
            set installpath ${userdir}
        }
    }
    return ${installpath}
}

proc ::deken::platform2string {{verbose 0}} {
    if { ${verbose} } {
        return $::deken::platform(os)-$::deken::platform(machine)-float$::deken::platform(floatsize)
    } else {
        return $::deken::platform(os)-$::deken::platform(machine)-$::deken::platform(floatsize)
    }
}

# allow overriding deken platform from Pd-core
proc ::deken::set_platform {os machine bits floatsize} {
    set machine [string tolower ${machine}]
    set bits [::deken::utilities::int ${bits} $::deken::platform(bits)]
    set floatsize [::deken::utilities::int ${floatsize} $::deken::platform(floatsize)]
    if { ${os} != $::deken::platform(os) ||
         ${machine} != $::deken::platform(machine) ||
         ${bits} != $::deken::platform(bits) ||
         ${floatsize} != $::deken::platform(floatsize)
     } {
        set ::deken::platform(os) ${os}
        set ::deken::platform(machine) ${machine}
        set ::deken::platform(bits) ${bits}
        set ::deken::platform(floatsize) ${floatsize}
        set msg [format [_ "Platform re-detected: %s" ] [::deken::platform2string 1] ]
        ::pdwindow::verbose 0 "\[deken\] ${msg}\n"
    }
    if { [info procs ::pdwindow::update_title] ne ""} {
        after idle {::pdwindow::update_title .pdwindow}
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
    foreach x [regexp -all -inline {\d+|\D+} [string map {~ \t} ${a}]] y [regexp -all -inline {\d+|\D+} [string map {~ \t} ${b}]] {
        if { "${x}" == "" } { set x " " }
        if { "${y}" == "" } { set y " " }
        if { [catch {
           set c [dict get {1 0 {0 1} -1 {1 0} 1} [lsort -indices -dictionary -unique [list ${x} ${y}]]]
        } stdout ] } {
          # Tcl<8.5 (as found the PowerPC builds) lacks 'dict' and 'lsort -indices'
          if { [catch {
            # "string compare" does not sort numerically
            set c [expr {2 * (${x} > ${y}) + (${x} == ${y}) - 1}]
          } stdout] } {
            set c [string compare ${x} ${y}]
          }
        }
        if { ${c} != "0" } {return ${c}}
    }
    return 0
}

proc ::deken::verify_sha256_gui {url pkgfile} {
    ## verify that the SHA256 of the ${pkgfile} matches that from ${url}
    ## in case of failure, this displays a dialog asking the user how to proceed
    ## (if the preferences indicate we require checking)
    ## returns
    ## - 1 on success
    ## - 0 on failure
    ## - negative numbers indicate failures to be ignored
    ## - one digit: user requested ignore
    ## - -1 user requested ignore via prefs
    ## - -2 user requested ignore via dialog
    ## - two digits: unable to verify
    ## - -10 reference could not be read
    ## - -20 an exception occurred while verifying
    ## - three digits:
    ## - -100 no sha256 verifier implemented
    set err_msg [format [_ "SHA256 verification of '%s' failed!" ] ${pkgfile} ]
    set err_title [_ "SHA256 verification failed" ]
    set err_status [format [_ "Checksum mismatch for '%s'" ] ${url}]

    while 1 {
        set hash_ok [::deken::utilities::verify_sha256 ${url} ${pkgfile}]
        if { ${hash_ok} } {
            return ${hash_ok}
        }
        ::deken::statuspost ${err_status} warn 0
        if { ! ${::deken::verify_sha256} } {
            return -1
        }
        set result [tk_messageBox \
                        -title ${err_title} \
                        -message ${err_msg} \
                        -icon error -type abortretryignore \
                        -parent ${::deken::winid}]
        switch -- ${result} {
            abort {
                return 0
            }
            ignore {
                return -2
            }
        }
    }
    return 0
}

proc ::deken::install_package_from_file {{pkgfile ""}} {
    set types {}
    lappend types [list [_ "Deken Packages" ] .dek]
    lappend types [list [_ "ZIP Files" ] .zip]
    if {$::tcl_platform(platform) ne "windows"} {
        lappend types [list [_ "TAR Files" ] {.tgz} ]
        if {${::windowingsystem} eq "aqua"} {
            # stupid bug on macOS>=12: an extension with two dots crashes the fileselector
            lappend types [list [_ "TAR Files" ] {.gz} ]
        } else {
            lappend types [list [_ "TAR Files" ] {.tar.gz} ]
        }
    }
    lappend types [list [_ "All Files" ]  *  ]
    if { "${pkgfile}" eq ""} {
        set pkgfile [tk_getOpenFile -defaultextension dek -filetypes ${types}]
    }
    if { "${pkgfile}" eq "" } { return }

    # user picked one
    # perform checks and install it
    set pkgfile [file normalize ${pkgfile}]

    set result [::deken::verify_sha256_gui ${pkgfile} ${pkgfile}]
    if { ! ${result} } {
        return
    }
    ::deken::install_package ${pkgfile} "" "" 1
}
proc ::deken::install_package {fullpkgfile {filename ""} {installdir ""} {keep 1}}  {
    # fullpkgfile: the file to extract
    # filename   : the package file name (usually the basename of ${fullpkgfile})
    #              but might be different depending on the download)
    # installdir : where to put stuff into
    # keep       : whether to remove the fullpkgfile after successful extraction
    if { "${filename}" == "" } {
        set filename [file tail ${fullpkgfile}]
    }
    set installdir [::deken::ensure_installdir ${installdir} ${filename}]
    set parsedname [::deken::utilities::parse_filename ${filename}]
    foreach {extname version archs} ${parsedname} {break}
    set showextname ${extname}
    set deldir ""
    set extpath [file join ${installdir} ${extname}]

    if { ${extname} eq {} } {
        # cannot remove previous installation, as we couldn't guess the library name
        set showextname ${filename}
    } else {
        set match [::deken::architecture_match "${archs}" ]
        if { ! ${match} } {
            set msg [_ "Installing incompatible architecture of '%s'." ${showextname} ]
            ::deken::post "${msg}" warn
            if { "${::deken::remove_on_install}"  && [file exists ${extpath}] } {
                set result [tk_messageBox \
                                -title [_ "Replacing library with incompatible architecture!" ] \
                                -message [_ "Do you want to replace the library '%1\$s' in '%2\$s' with a version that is incompatible with your computer?" ${showextname} ${installdir}] \
                                -icon error -type yesnocancel \
                                -parent ${::deken::winid}]
                switch -- "${result}" {
                    cancel {return}
                    yes { }
                    no {
                        set installdir [::deken::do_prompt_installdir ${installdir}]
                        if { "${installdir}" == "" } {
                            return
                        }
                        set extpath [file join ${installdir} ${extname}]
                    }
                }
            }
        }

        if { "${::deken::remove_on_install}" } {
            ::deken::statuspost [format [_ "Uninstalling previous installation of '%s'" ] ${showextname} ] info

            if { ! [::deken::utilities::uninstall ${installdir} ${extname}] } {
                # ouch uninstalling failed.
                # on msw, lets assume this is because some of the files in the folder are locked.
                # so move the folder out of the way and proceed
                set deldir [::deken::utilities::get_tmpfilename ${installdir}]
                if { [ catch {
                    file mkdir ${deldir}
                    file rename [file join ${installdir} ${extname}] [file join ${deldir} ${extname}]
                } ] } {
                    ::deken::utilities::debug [format [_ "Temporarily moving %1\$s into %2\$s failed." ] ${showextname} ${deldir} ]
                    set deldir ""
                }
            }
        }
    }

    ::deken::statuspost [format [_ "Installing package '%s'" ] ${showextname} ] {} 0
    ::deken::syncgui
    ::deken::progress 0
    if { [::deken::utilities::extract ${installdir} ${filename} ${fullpkgfile} ${keep}] > 0 } {
        ::deken::progressstatus [_ "Installation completed!" ]
        set msg [format [_ "Successfully installed '%s'!" ] ${showextname} ]
        ::deken::statuspost "${msg}" {} 0
        ::deken::post ""
        ::pdwindow::post "\[deken\] ${msg}\n"
        set install_failed 0
    } else {
        ::deken::progressstatus [_ "Installation failed!" ]
        set msg [format [_ "Failed to install '%s'!" ] ${showextname} ]
        ::deken::statuspost ${msg} error 0
        tk_messageBox \
            -title [_ "Package installation failed" ] \
            -message "${msg}" \
            -icon error -type ok \
            -parent ${::deken::winid}
        set install_failed 1
    }

    if { "${deldir}" != "" } {
        # try getting rid of the directory to be deleted
        # we already tried once (and failed), so this time we iterate over each file
        set rmerrors [::deken::utilities::rmrecursive ${deldir}]
        # and if there are still files around, ask the user to delete them.
        if { ${rmerrors} > 0 } {
            set result [tk_messageBox \
                            -message [format [_ "Failed to completely remove %1\$s.\nPlease manually remove the directory %2\$s after quitting Pd." ] ${showextname} ${deldir}] \
                            -icon warning -type okcancel -default ok \
                            -parent ${::deken::winid}]
            switch -- ${result} {
                ok {
                    ::pd_menucommands::menu_openfile ${deldir}
                }
            }
        }
        set deldir ""
    }

    if { ${install_failed} } { return }

    if { ${extname} eq {} } {
        # failed to properly parse packagefile, so we do not know the libraryname
        # therefore we cannot actually show the readme nor add the path
        return
    }


    if { "${::deken::show_readme}" } {
        foreach ext {pd html pdf txt} {
            set r [file join ${extpath} "README.deken.${ext}"]
            if {[file exists ${r}]} {
                if { "${ext}" == "pd" } {
                    set directory [file normalize [file dirname ${r}]]
                    set basename [file tail ${r}]
                    pdsend "pd open [enquote_path ${basename}] [enquote_path ${directory}]"
                } else {
                    pd_menucommands::menu_openfile ${r}
                }
                break
            }
        }
    }

    if { "${::deken::add_to_path}" } {
        # add to the search paths? bail if the version of pd doesn't support it
        if {[uplevel 1 info procs add_to_searchpaths] eq ""} {return}
        if {![file exists ${extpath}]} {
            ::deken::utilities::debug [format [_ "Unable to add %s to search paths"] ${extname}]
            return
        }
        set result yes
        if { ${::deken::add_to_path} > 1 } {
            set result yes
        } else {
            set result [tk_messageBox \
                            -message [format [_ "Add %s to the Pd search paths?" ]  ${extname}] \
                            -icon question -type yesno -default yes \
                            -parent ${::deken::winid}]
        }
        switch -- "${result}" {
            yes {
                add_to_searchpaths [file join ${installdir} ${extname}]
                ::deken::utilities::debug [format [_ "Added %s to search paths"] ${extname}]
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
    # this should probably only be called if toplevel is indeed a toplevel
    if { ${toplevel} eq [winfo toplevel ${toplevel}] } {
        bind ${toplevel} <${::modifier}-Key-w> [list destroy ${toplevel}]
        bind ${toplevel} <Escape> [list after idle [list destroy ${toplevel}]]
    }
}

proc ::deken::status {{msg ""} {timeout 5000}} {
    after cancel ${::deken::statustimer}
    if {"" ne ${msg}} {
        set ::deken::statustext "${msg}"
        if { ${timeout} != "0" } {
            set ::deken::statustimer [after ${timeout} [list set "::deken::statustext" ""]]
        }
    } else {
        set ::deken::statustext ""
    }
}
proc ::deken::progressstatus {{msg ""} {timeout 5000}} {
   after cancel ${::deken::progresstimer}
    if {"" ne ${msg}} {
        set ::deken::progresstext "${msg}"
        if { ${timeout} != "0" } {
            set ::deken::progresstimer [after ${timeout} [list set "::deken::progresstext" ""]]
        }
    } else {
        set ::deken::progresstext ""
    }
}
proc ::deken::syncgui {} {
    update idletasks
}
proc ::deken::scrollup {} {
    variable infoid
    if { [winfo exists ${infoid}] } {
        ${infoid} see 0.0
    }
}
proc ::deken::post {msg args} {
    variable infoid
    if { [winfo exists ${infoid}] } {
        ${infoid} insert end "${msg}\n" ${args}
        ${infoid} see end
    }
}
proc ::deken::statuspost {msg {tag info} {timeout 5000}} {
    post ${msg} ${tag}
    status ${msg} ${timeout}
}

proc ::deken::clearpost {} {
    variable infoid
    if { [winfo exists ${infoid}] } {
        ${infoid} delete 1.0 end
    }
    set ::deken::selected {}
}

proc ::deken::post_result {msg {tag ""}} {
    variable resultsid
    if { [winfo exists ${resultsid}] } {
        ${resultsid} insert end "${msg}\n" ${tag}
        ${resultsid} see end
    }
}
proc ::deken::bind_resulttag {tagname key cmd} {
    variable resultsid
    if { [winfo exists ${resultsid}] } {
        ${resultsid} tag bind ${tagname} ${key} ${cmd}
    }
}
proc ::deken::highlightable_resulttag {tagname} {
    variable resultsid
    if { [winfo exists ${resultsid}] } {
        ::deken::bind_resulttag ${tagname} <Enter> \
            "${resultsid} tag add highlight [ ${resultsid} tag ranges ${tagname} ]"
        ::deken::bind_resulttag ${tagname} <Leave> \
            "${resultsid} tag remove highlight [ ${resultsid} tag ranges ${tagname} ]"
        # make sure that the 'highlight' tag is topmost
        ${resultsid} tag raise sel
        ${resultsid} tag raise highlight
    }
}
proc ::deken::bind_contextmenu {resultsid tagname cmd} {
    if { [winfo exists ${resultsid}] } {
        if {${::windowingsystem} eq "aqua"} {
            ${resultsid} tag bind ${tagname} <2> ${cmd}
        } else {
            ${resultsid} tag bind ${tagname} <3> ${cmd}
        }
    }
}

proc ::deken::menu_installselected {resultsid} {
    set counter 0
    foreach {k v} ${::deken::selected} {
        if { ${v} ne {} } {
            eval ${v}
            incr counter
        }
    }
    if { ${counter} == 0 } {
        ::deken::statuspost [_ "No packages selected for installation."]
    } elseif { ${counter} > 1 } {
        ::deken::statuspost [format [_ "Processed %d packages selected for installation."] ${counter} ]
    }
    # clear the selection
    set ::deken::selected {}
    ::deken::clear_selection ${resultsid}
    ::deken::update_installbutton ${::deken::winid}
}

proc ::deken::menu_uninstall_package {winid pkgname installpath} {
    ::deken::show_tab ${winid} info
    ::deken::statuspost [format [_ "Uninstalling previous installation of '%s'" ] ${pkgname} ] info
    ::deken::utilities::uninstall ${installpath} ${pkgname}
}



proc ::deken::do_prompt_installdir {path {winid {}}} {
    set msg [_ "Install externals to directory:"]
    if { ${winid} eq {} } {
        set winid ${::deken::winid}
    }
    if {[winfo exists ${winid}]} {
        tk_chooseDirectory -title "${msg}" -initialdir ${path} -parent ${winid}
    } else {
        tk_chooseDirectory -title "${msg}" -initialdir ${path}
    }
}

proc ::deken::prompt_installdir {} {
    set installdir [::deken::do_prompt_installdir ${::fileopendir}]
    if { "${installdir}" != "" } {
        ::deken::set_installpath ${installdir}
        return 1
    }
    return 0
}


proc ::deken::update_searchbutton {winid} {
    if { [${winid}.searchbit.entry get] == "" } {
        ${winid}.searchbit.button configure -text [_ "Show all" ]
    } else {
        ${winid}.searchbit.button configure -text [_ "Search" ]
    }
}

proc ::deken::update_installbutton {winid} {
    set installbutton ${winid}.status.install
    if { ! [winfo exists ${installbutton}] } { return }
    set counter 0
    foreach {a b} ${::deken::selected} {
        if {${b} ne {} } {
            incr counter
        }
    }
    if { ${counter} > 0 } {
        ${installbutton} configure -state normal -text [format [_ "Install (%d)" ] ${counter}]
    } else {
        ${installbutton} configure -state disabled -text [_ "Install" ]
    }
}

proc ::deken::progress {x} {
    ::deken::statuspost [format [_ "%s%% of download completed"] ${x}]
}

# this function gets called when the menu is clicked
proc ::deken::open_searchui {winid} {
    if {[winfo exists ${winid}]} {
        wm deiconify ${winid}
        raise ${winid}
    } else {
        variable resultsid
        variable infoid
        ::deken::create_dialog ${winid}
        ::deken::bind_globalshortcuts ${winid}
        foreach dndid [list ${winid}.tab ${winid}.results] {
            if { [winfo exists ${dndid}] } {
                ::deken::utilities::dnd_init ${dndid}
            }
        }
        ${infoid} tag configure error -foreground red
        ${infoid} tag configure warn -foreground orange
        ${infoid} tag configure info -foreground black
        ${infoid} tag configure debug -foreground grey
        ${infoid} tag configure dekenurl -foreground blue
        ${infoid} tag bind dekenurl <1> "pd_menucommands::menu_openfile https://deken.puredata.info/"
        ${infoid} tag bind dekenurl <Enter> "${infoid} tag configure dekenurl -underline 1"
        ${infoid} tag bind dekenurl <Leave> "${infoid} tag configure dekenurl -underline 0"
        ${resultsid} tag configure highlight -foreground blue
        ${resultsid} tag configure archmatch
        ${resultsid} tag configure noarchmatch -foreground grey
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
    ::deken::post "" info
    ::deken::post [_ "You can also search for libraries & objects via your web browser:" ] info
    ::deken::post "https://deken.puredata.info" dekenurl
}

# build the externals search dialog window
proc ::deken::create_dialog {winid} {
    variable resultsid
    toplevel ${winid} -class DialogWindow
    set ::deken::winid ${winid}
    set title  [_ "Find externals"]
    wm title ${winid} "deken - ${title}"
    wm geometry ${winid} 670x550
    wm minsize ${winid} 230 360
    wm transient ${winid}
    ${winid} configure -padx 10 -pady 5

    set m ${winid}_menu
    destroy ${m}
    menu ${m}
    menu ${m}.file
    ${m} add cascade -label [_ [string totitle "file"]] -underline 0 -menu ${m}.file
    ${m}.file add command -label [_ "Install DEK file..." ] -command "::deken::install_package_from_file"
    menu ${m}.edit
    ${m} add cascade -label [_ [string totitle "edit"]] -underline 0 -menu ${m}.edit
    ${m}.edit add command -label [_ "Preferences..." ] -command "::deken::preferences::show"

    ${winid} configure -menu ${m}

    frame ${winid}.searchbit
    pack ${winid}.searchbit -side top -fill "x"

    entry ${winid}.searchbit.entry -font 18 -relief sunken -highlightthickness 1 -highlightcolor blue
    pack ${winid}.searchbit.entry -side left -padx 6 -fill "x" -expand true
    bind ${winid}.searchbit.entry <Key-Return> "::deken::initiate_search ${winid}"
    bind ${winid}.searchbit.entry <KeyRelease> "::deken::update_searchbutton ${winid}"
    focus ${winid}.searchbit.entry
    button ${winid}.searchbit.button -text [_ "Show all"] -default active -command "::deken::initiate_search ${winid}"
    pack ${winid}.searchbit.button -side right -padx 6 -pady 3 -ipadx 10

    frame ${winid}.objlib
    pack ${winid}.objlib -side top -fill "x"
    label ${winid}.objlib.label -text [_ "Search for: "]
    radiobutton ${winid}.objlib.libraries -text [_ "libraries"] -variable ::deken::searchtype -value libraries
    radiobutton ${winid}.objlib.objects -text [_ "objects"] -variable ::deken::searchtype -value objects
    radiobutton ${winid}.objlib.both -text [_ "both"] -variable ::deken::searchtype -value name
    foreach x {label libraries objects both} {
        pack ${winid}.objlib.${x} -side left -padx 6
    }
    # for Pd that supports it, add a 'translation' radio
    if {[uplevel 2 info procs add_to_helppaths] ne ""} {
        radiobutton ${winid}.objlib.translations -text [_ "translations"] -variable ::deken::searchtype -value translations
        pack ${winid}.objlib.translations -side left -padx 6
    }
    frame ${winid}.warning
    pack ${winid}.warning -side top -fill "x"
    label ${winid}.warning.label -text [_ "Only install externals uploaded by people you trust."]
    pack ${winid}.warning.label -side left -padx 6

    if { [catch {
        if {${::windowingsystem} eq "aqua"
            && [::deken::versioncompare 8.6 [info patchlevel]] < 0
            && [::deken::versioncompare 8.6.12 [info patchlevel]] > 0
        } {
            ::deken::utilities::debug [_ "Disabling tabbed view: incompatible Tcl/Tk detected"]
            error [_ "Disabling tabbed view: incompatible Tcl/Tk detected"]
        }

        ttk::notebook ${winid}.tab
        pack ${winid}.tab -side top -padx 6 -pady 3 -fill both -expand true

        text ${winid}.tab.info -takefocus 0 -cursor hand2 -height 100 -yscrollcommand "${winid}.tab.info.ys set"
        scrollbar ${winid}.tab.info.ys -orient vertical -command "${winid}.tab.info yview"
        pack ${winid}.tab.info.ys -side right -fill "y"

        if { [catch {
            set treeid ${winid}.tab.results
            ttk::treeview ${treeid} \
                -height 10 \
                -selectmode browse \
                -columns {version title uploader date} \
                -displaycolumns {version uploader date} \
                -yscrollcommand "${winid}.tab.results.ys set"
            ${treeid} heading #0 -text [_ "Library" ] -anchor center -command "::deken::treeresults::columnsort ${treeid}"
            ${treeid} heading version -text [_ "Version" ] -anchor center -command "::deken::treeresults::columnsort ${treeid} version"
            ${treeid} heading title -text [_ "Description" ] -anchor center -command "::deken::treeresults::columnsort ${treeid} title"
            ${treeid} heading uploader -text [_ "Uploader" ] -anchor center -command "::deken::treeresults::columnsort ${treeid} uploader"
            ${treeid} heading date -text [_ "Date" ] -anchor center -command "::deken::treeresults::columnsort ${treeid} date"
            ${treeid} column #0 -stretch 0
            ${treeid} tag configure library -background lightgrey
            ${treeid} tag configure noarchmatch -foreground lightgrey
            ${treeid} tag configure selpkg -background lightblue

            bind ${treeid} <<TreeviewSelect>> "::deken::treeresults::selection_changed %W"
            bind ${treeid} <<TreeviewOpen>> "::deken::treeresults::selection_skip %W 1"
            bind ${treeid} <<TreeviewClose>> "::deken::treeresults::selection_skip %W 1"
            bind ${treeid} <Motion> "::deken::treeresults::motionevent %W %x %y"
            bind ${treeid} <Leave> "::deken::treeresults::leaveevent %W"
            bind ${treeid} <Double-ButtonRelease-1> "::deken::treeresults::doubleclick %W %x %y"

            proc ::deken::show_results {resultsid} { ::deken::treeresults::show ${resultsid}}
            proc ::deken::clear_results {resultsid} { ::deken::treeresults::clear ${resultsid}}
            proc ::deken::clear_selection {resultsid} { ::deken::treeresults::clear_selection ${resultsid} }

            scrollbar ${winid}.tab.results.ys -orient vertical -command "${winid}.tab.results yview"
            pack ${winid}.tab.results.ys -side right -fill "y"

        } ] } {
            text ${winid}.tab.results -takefocus 0 -cursor hand2 -height 100 -yscrollcommand "${winid}.tab.results.ys set"
            scrollbar ${winid}.tab.results.ys -orient vertical -command "${winid}.tab.results yview"
            pack ${winid}.tab.results.ys -side right -fill "y"
        }

        ${winid}.tab add ${winid}.tab.results -text [_ "Search Results"]
        ${winid}.tab add ${winid}.tab.info -text [_ "Log"]
        ::deken::show_tab ${winid} info

        variable infoid
        set resultsid ${winid}.tab.results
        set infoid ${winid}.tab.info
    } ] } {
        text ${winid}.results -takefocus 0 -cursor hand2 -height 100 -yscrollcommand "${winid}.results.ys set"
        scrollbar ${winid}.results.ys -orient vertical -command "${winid}.results yview"
        pack ${winid}.results.ys -side right -fill "y"
        pack ${winid}.results -side top -padx 6 -pady 3 -fill both -expand true
    }

    frame ${winid}.progress
    pack ${winid}.progress -side top -fill "x"
    if { ! [ catch {
        ttk::progressbar ${winid}.progress.bar -orient horizontal -length 640 -maximum 100 -mode determinate -variable ::deken::progressvar } stdout ] } {
        pack ${winid}.progress.bar -side top -fill "x"
        proc ::deken::progress {x} { set ::deken::progressvar ${x} }
        label ${winid}.progress.label -textvariable ::deken::progresstext -padx 0 -borderwidth 0
        place ${winid}.progress.label -in ${winid}.progress.bar -x 1
    }

    frame ${winid}.status
    pack ${winid}.status -side bottom -fill "x" -pady 3
    label ${winid}.status.label -textvariable ::deken::statustext -relief sunken -anchor "w"
    pack ${winid}.status.label -side bottom -fill "x"

    button ${winid}.status.install -text [_ "Install" ] \
        -state disabled \
        -command "::deken::menu_installselected ${resultsid}"

    pack ${winid}.status.install -side right -padx 6 -pady 3 -ipadx 10
}

proc ::deken::show_tab {winid tab} {
    if { [winfo exists ${winid}.tab.${tab}] } {
        ${winid}.tab select ${winid}.tab.${tab}
    }
}

proc ::deken::open_search_xxx {searchtype xxx}  {
    set winid ${::deken::winid}
    ::deken::open_searchui ${winid}
    ::deken::clearpost

    set searchterm {}
    if { ${::deken::searchtype} eq "${searchtype}" } {
        append searchterm [${winid}.searchbit.entry get]
    }
    if { ${searchterm} ne {} } {  append searchterm " " }
    foreach xx ${xxx} {
        foreach x ${xx} {
            lappend searchterm ${x}
        }
    }

    ${winid}.searchbit.entry delete 0 end
    ${winid}.searchbit.entry insert end ${searchterm}

    set ::deken::searchtype "${searchtype}"
    ::deken::update_searchbutton ${winid}
}

proc ::deken::open_search_objects {args}  {
    ::deken::open_search_xxx "objects" ${args}
}

proc ::deken::open_search_libraries {args}  {
    ::deken::open_search_xxx "libraries" ${args}
}
proc ::deken::open_search_translations {args}  {
    ::deken::open_search_xxx "translations" ${args}
}


proc ::deken::open_search_missing_libraries {args}  {
    # LATER this should only display not-installed libraries
    ::deken::open_search_xxx "libraries" ${args}
}

proc ::deken::initiate_search {winid} {
    set searchterm [${winid}.searchbit.entry get]
    # let the user know what we're doing
    ::deken::show_tab ${winid} info

    ::deken::clearpost
    ::deken::statuspost [format [_ "Searching for \"%s\"..." ] ${searchterm} ]
    set ::deken::progressvar 0
    ::deken::progressstatus ""
    if { [ catch {
        set results [::deken::search_for ${searchterm}]
    } stdout ] } {
        ::deken::utilities::debug [format [_ "online? %s" ] ${stdout} ]
        ::deken::statuspost [_ "Unable to perform search. Are you online?" ] error
    } else {
        # delete all text in the results
        variable resultsid
        ::deken::clear_results ${resultsid}
        set ::deken::selected {}

        set ::deken::results ${results}
        set matchcount 0
        foreach r ${results} {
            foreach {_ _ match} ${r} {break}
            if { ${match} } {
                incr matchcount
            }
        }
        if {[llength ${results}] != 0} {
            ::deken::show_results ${resultsid}
            set msg [format [_ "Found %1\$d usable packages (of %2\$d packages in total)." ] ${matchcount} [llength ${results}]]
            ::deken::statuspost [format {"%s": %s} ${searchterm} ${msg}]
            if { ${matchcount} } {
                ::deken::show_tab ${winid} results
            } else {
                ::deken::post [_ "It appears that there are no matching packages for your architecture." ] warn
            }
        } else {
            ::deken::statuspost [_ "No matching externals found." ]
            set msg [_ "Try using the full name e.g. 'freeverb~'." ]
            ::deken::post " ${msg}"
            set msg [_ "Or use wildcards like 'freeverb*'." ]
            ::deken::post " ${msg}"
        }
    }
}

## deken::textresults: show versions of libraries in a simple text widget
namespace eval ::deken::textresults:: {
}

# display a single found entry in a simple text widget
proc ::deken::textresults::show_result {resultsid counter result showmatches} {
    foreach {title cmd match comment status contextcmd pkgname} ${result} {break}
    set tag ch${counter}
    set tags [list ${tag} [expr {${match}?"archmatch":"noarchmatch"} ] ]
    if { "${pkgname}" ne "" } {lappend tags "/${pkgname}"}

    if {(${match} == ${showmatches})} {
        set comment [string map {"\n" "\n\t"} ${comment}]
        ::deken::post_result "${title}\n\t${comment}\n" ${tags}
        ::deken::highlightable_resulttag ${tag}
        ::deken::bind_resulttag ${tag} <Enter> "+::deken::status {${status}}"
        ::deken::bind_resulttag ${tag} <1> "${cmd}"
        if { "" ne ${contextcmd} } {
            ::deken::bind_contextmenu ${resultsid} ${tag} ${contextcmd}
        }
    }
}

# display all found entries in a simple text widget
proc ::deken::textresults::show {resultsid} {
    set counter 0
    # build the list UI of results
    foreach r ${::deken::results} {
        ::deken::textresults::show_result ${resultsid} ${counter} ${r} 1
        incr counter
    }
    if { "${::deken::hideforeignarch}" } {
        # skip display of non-matching archs
    } else {
        set counter 0
        foreach r ${::deken::results} {
            ::deken::textresults::show_result ${resultsid} ${counter} ${r} 0
            incr counter
        }
    }
    if { [winfo exists ${resultsid}] } {
        ${resultsid} see 0.0
    }
}

proc ::deken::textresults::clear {resultsid} {
    if { [winfo exists ${resultsid}] } {
        ${resultsid} delete 1.0 end
    }
}

proc ::deken::textresults::selectpackage {resultsid pkgname installcmd} {
    # set/unset the selection in a "dict"
    set state {}
    set counter 1
    foreach {k v} ${::deken::selected} {
        if { ${k} eq ${pkgname} } {
            if { ${v} ne ${installcmd} } {
                set state 1
                lset ::deken::selected ${counter} ${installcmd}
            } else {
                set state 0
                lset ::deken::selected ${counter} {}
            }
            break
        }
        incr counter 2
    }
    if { ${state} eq {} } {
        # not found in the dict; just add it
        lappend ::deken::selected ${pkgname} ${installcmd}
        set state 1
    }

    # set/unset the visual representation (via tags)
    set counter 0
    foreach {a b} [${resultsid} tag ranges /${pkgname}] {${resultsid} tag remove sel ${a} ${b}}
    if { ${state} } {
        foreach r ${::deken::results} {
            if { [lindex ${r} 1] eq ${installcmd} } {
                foreach {a b} [${resultsid} tag ranges ch${counter}] {${resultsid} tag add sel ${a} ${b}}
            }
            incr counter
        }
    }
    ::deken::update_installbutton [winfo toplevel ${resultsid}]
}

proc ::deken::textresults::clear_selection {resultsid} {
    if { [winfo exists ${resultsid}] } {
        foreach {a b} [${resultsid} tag ranges sel] {${resultsid} tag remove sel ${a} ${b}}
    }
}


## deken::treeresults: show versions of libraries in a tree-view
# TASKs
# - each library (distinguished by name) is a separate (expandable/collapsible) node
# - expanding a library node shows all versions
# - the library node shows which version of the library is going to be installed (if any)
# - the tree can be sorted in both directions by clicking on any of the headings
# SELECTING which library to install
# - clicking on a version
#   - if the version was currently selected for installation, it is now deselected
#   - otherwise select this version to be installed
# - clicking on a library node
#   - if no version of the given library has been selected, this selects the most recent compatible version
#   - otherwise the library is deselected from installation
# - multiple selections
#   - ideally we would just forbid ctrl-clicking for multiple selections
#   - otherwise, this would select the the most recent compatible version
#
# CAVEATs
# - interaccting with the selection for library 'x' should not interfere with the selection of library 'y'
# - incompatible archs should be marked somehow
# - incompatible archs must always be explicitly selected
# - TODO: what about multi-selecting incompatible archs of only a single library?
# - TODO: what about multi-selecting a couple of libraries where some only have incompatible archs?

namespace eval ::deken::treeresults:: {
}
array set ::deken::treeresults::colsort {}
array set ::deken::treeresults::skipclick {}
array set ::deken::treeresults::activecell {}

proc ::deken::treeresults::columnsort {treeid {col "#0"}} {
    # do we want to sort increasing or decreasing?
    variable colsort
    if {! [info exists colsort(${col}) ] } {
        set colsort(${col}) 1
    }
    set colsort(${col}) [expr { ! $colsort(${col}) }]

    set dir -increasing
    if { $colsort(${col}) } {
        set dir -decreasing
    }

    # do the actual sorting
    if { ${col} eq "#0" } {
        set sortable {}
        foreach lib [${treeid} children {}] {
            lappend sortable [list [${treeid} item ${lib} -text] ${lib}]
        }
        set pkgs {}
        foreach x [lsort -nocase ${dir} -index 0 ${sortable}] {
            lappend pkgs [lindex ${x} 1]
        }
        ${treeid} children {} ${pkgs}
    } else {
        foreach lib [${treeid} children {}] {
            set sortable {}
            foreach pkg [${treeid} children ${lib}]  {
                lappend sortable [list [${treeid} set ${pkg} ${col}] ${pkg}]
            }
            set pkgs {}
            foreach x [lsort -nocase ${dir} -index 0 -command ::deken::versioncompare ${sortable}] {
                lappend pkgs [lindex ${x} 1]
            }
            ${treeid} children ${lib} ${pkgs}
        }
    }

    ## add some decoration to the header indicating the sort-direction
    set label_incr "\u2b07"
    set label_decr "\u2b06"

    set dirsym "${label_incr}"
    if { ${dir} eq "-decreasing" } {
        set dirsym "${label_decr}"
    }
    # clear all the increasing/decreasing indicators from the headings
    foreach c [${treeid} cget -columns] {
        ${treeid} heading ${c} -text [regsub "(${label_decr}|${label_incr})$" [${treeid} heading ${c} -text] {}]
    }
    set c "#0"
    ${treeid} heading ${c} -text [regsub "(${label_decr}|${label_incr})$" [${treeid} heading ${c} -text] {}]

    # and finally set the increasing/decreasing indicator for the sorted column
    ${treeid} heading ${col} -text [${treeid} heading ${col} -text]${dirsym}
}


proc ::deken::treeresults::focusbyindex {treeid index} {
    # make sure that the entry <index> is visible
    ${treeid} yview ${index}
}

proc ::deken::treeresults::getselected {treeid} {
    set sel {}
    foreach id [${treeid} children {}] {
        set data [${treeid} item ${id} -values]
        if { "${data}" eq {} } { continue }
        lappend sel [linsert ${data} 0 [${treeid} item ${id} -text]]
    }
    return ${sel}
}

proc ::deken::treeresults::selection_skip {treeid {state 1}} {
    # expanding/collapsing a node results in a selection message
    # so we set a flag to skip it
    variable skipclick
    if { ! [info exists skipclick(${treeid})] } {
        set skipclick(${treeid}) 0
    }
    set skip $skipclick(${treeid})
    set skipclick(${treeid}) ${state}
    return ${skip}
}

proc ::deken::treeresults::selection_changed {treeid} {
    if { [::deken::treeresults::selection_skip ${treeid} 0] } { return }

    ${treeid} tag remove selpkg
    foreach sel [${treeid} selection] {
        set lib [${treeid} parent ${sel}]
        if { ${lib} eq {} } {
            # library node
            set lib ${sel}
            if { [${treeid} item ${sel} -values] eq {} } {
                # currently no data, find the best match!
                set children {}
                foreach child [${treeid} children ${lib}] {
                    set data [${treeid} item ${child} -values]
                    if {[lindex ${data} 4]} {
                        lappend children [list [lindex ${data} 0] ${child}]
                    }
                }
                set children [lsort -decreasing -index 0 -command ::deken::versioncompare ${children}]
                set sel {}
                foreach child ${children} {
                    foreach {version sel} ${child} {break}
                    break
                }
                if { ${sel} != {}} {
                    ${treeid} item ${lib} -values [${treeid} item ${sel} -values]
                    ${treeid} tag add selpkg ${sel}
                }
            } else {
                ${treeid} item ${lib} -values {}
            }

        } else {
            # package (leaf)
            set data [${treeid} item ${sel} -values]
            if { ${data} eq [${treeid} item ${lib} -values] } {
                # we were already selected, so deselect us
                ${treeid} item ${lib} -values {}
            } else {
                ${treeid} item ${lib} -values ${data}
                ${treeid} tag add selpkg ${sel}
            }
        }
    }

    ## fixup the selection
    set bound [bind ${treeid} <<TreeviewSelect>>]
    bind ${treeid} <<TreeviewSelect>> {}
    # unselect the old ones, and select the new ones
    ${treeid} selection remove [${treeid} selection]
    set counter 0

    set ::deken::selected {}
    foreach id [${treeid} children {}] {
        set data [${treeid} item ${id} -values]
        if { ${data} eq {} } { continue }
        ${treeid} selection add ${id}
        lappend ::deken::selected [${treeid} item ${id} -text] [lindex ${data} 5]
    }
    ::deken::update_installbutton [winfo toplevel ${treeid}]
    after idle "bind ${treeid} <<TreeviewSelect>> \{${bound}\}"
}

proc ::deken::treeresults::presorter {A B} {
    # <a>, <b>: [<pkgname> <version> <title> <uploader> <timestamp> <match> <cmd> <contextcmd>]
    # compare to library lists: <pkgname> (ascending), <match> (descending), <version> (descending), <date> (descending)
    foreach {a_name a_ver _ _ a_time a_match} ${A} {break}
    foreach {b_name b_ver _ _ b_time b_match} ${B} {break}

    if {${a_name} < ${b_name}} { return 1 } elseif {${a_name} > ${b_name}} { return -1 }

    if {${a_match} < ${b_match}} { return -1 } elseif {${a_match} > ${b_match}} { return 1 }

    set v [::deken::versioncompare ${a_ver} ${b_ver}]
    if { ${v} != "0" } {return ${v}}

    if {${a_time} < ${b_time}} { return -1 } elseif {${a_time} > ${b_time}} { return 1 }

    return 0
}

proc ::deken::treeresults::motionevent {treeid x y} {
    set item [${treeid} identify item ${x} ${y}]
    set data [${treeid} item ${item} -values]
    if {! [info exists ::deken::treeresults::activecell(${treeid}) ] } {
        set ::deken::treeresults::activecell(${treeid}) {}
    }

    set title [lindex ${data} 1]
    set status [lindex ${data} 7]
    set subtitle [lindex ${data} 8]

    # the status bar
    if { "${status}" != "" } {
        ::deken::status ${status}
    }

    # the balloon
    if { $::deken::treeresults::activecell(${treeid}) != ${item} } {
        set ::deken::treeresults::activecell(${treeid}) ${item}
        set X [expr {[winfo rootx ${treeid}] + 10}]
        set Y [expr {[winfo rooty ${treeid}] + ${y} + 10}]

        ::deken::balloon::show ${treeid}_balloon ${X} ${Y} [string trim "${title}\n${subtitle}"]
    }
}
proc ::deken::treeresults::leaveevent {treeid} {
    set ::deken::treeresults::activecell(${treeid}) {}
    ::deken::balloon::hide ${treeid}_balloon
}

proc ::deken::treeresults::doubleclick {treeid x y} {
    set item [${treeid} identify item ${x} ${y}]
    set installitem ${item}
    if { [${treeid} bbox ${item}] eq {} } {
        set installitem {}
    }

    if { ${installitem} eq {} } {
        # the user double-clicked on a column heading
        set column [${treeid} identify column ${x} ${y}]
        if { ${column} eq "#0" } {
            # we don't want to sort by column#0
            # instead we open/close the items
            set have_open 0
            set have_close 0
            foreach lib [${treeid} children {}] {
                if { [${treeid} item ${lib} -open] } {
                    incr have_open
                } else {
                    incr have_close
                }
            }
            set do_open [expr {${have_close} > ${have_open}}]
            foreach lib [${treeid} children {}] {
                ${treeid} item ${lib} -open ${do_open}
            }
            return
        }
        set column [${treeid} column ${column} -id]

        # do we want to sort increasing or decreasing?
        variable colsort
        if {! [info exists colsort(${column}) ] } {
            set colsort(${column}) 1
        }
        set dir -increasing
        if { $colsort(${column}) } {
            set dir -decreasing
        }

        set sortable {}
        foreach lib [${treeid} children {}] {
            foreach pkg [${treeid} children ${lib}] {
                lappend sortable [list [${treeid} set ${pkg} ${column}] ${lib}]
                break
            }
        }
        set pkgs {}
        foreach x [lsort -nocase ${dir} -index 0 -command ::deken::versioncompare ${sortable}] {
            lappend pkgs [lindex ${x} 1]
        }
        ${treeid} children {} ${pkgs}
        if { ${item} eq {} } {
            # yikes: if we are not scrolled down, the double-click will trigger columnsort twice
            # so we do an extra round here...
            ::deken::treeresults::columnsort ${treeid} ${column}
        }

        return
    }
    set data [${treeid} item ${item} -values]
    set cmd [lindex ${data} 5]
    if { ${cmd} != "" } {
        ::deken::post ""
        eval ${cmd}
    }
}


proc ::deken::treeresults::show {treeid} {
    # shown: library, version, title, uploader, date
    set libraries {}
    foreach r ${::deken::results} {
        foreach {title cmd match subtitle statusline contextcmd pkgname version uploader timestamp} ${r} {break}
        if { "${::deken::hideforeignarch}" }  {
            if { ! ${match} } {
                continue
            }
        }

        lappend libraries [list ${pkgname} ${version} ${title} ${uploader} ${timestamp} ${match} ${cmd} ${contextcmd} ${statusline} ${subtitle}]
    }
    # sort the libraries
    set libraries [lsort -decreasing -command ::deken::treeresults::presorter ${libraries}]

    set lastlib {}
    set index {}

    ##foreach v {"#0" version title uploader date} {
    ##    set width(${v}) 0
    ##}

    #puts [time {
    foreach lib ${libraries} {
        set l [lindex ${lib} 0]
        set data [lrange ${lib} 1 end]
        if {${l} ne ${lastlib}} {
            set lastlib ${l}
            set index [${treeid} insert {} end -text ${l} -open 0 -tags {library}]
            ##set w [font measure {-underline false} -displayof ${treeid} ${l}]
            ##if {${w} > $width(#0)} {set width(#0) ${w}}
        }
        set archtag noarchmatch
        if { [lindex ${lib} 5] } {
            set archtag archmatch
        }
        set x [${treeid} insert ${index} end -values ${data} -tags [list package ${archtag}]]
        ##set vidx 0
        ##foreach v {version title uploader date} {
        ##    set w [font measure {-underline false} -displayof ${treeid} [lindex ${data} ${vidx}]]
        ##    incr vidx
        ##    if { ${w} > $width(${v}) } {set width($v) ${w} }
        ##}
        ${treeid} tag add ${x} ${x}
        ::deken::bind_contextmenu ${treeid} ${x} [lindex ${lib} 7]
    }
    #}]

    ### setting the width as a few caveats
    ## - we end up with cut off texts anyhow
    ##   (i guess this is mostly a problem with requiring too much text)
    ## - the widths don#t get fully applied automatically
    ##   (as soon as you drag one of the column delimiters, the other snap into place)
    ## - it takes forever.
    ##   (simply calculating the required widths for 148 entries takes ~7200ms,
    ##   as opposed to ~1.2ms for not calculating them)
    ##
    #foreach v {version title uploader date} {
    #    incr width(${v}) 10
    #    ${treeid} column ${v} -width $width(${v})
    #}

}


proc ::deken::treeresults::clear {resultsid} {
    ${resultsid} delete [${resultsid} children {}]
}
proc ::deken::treeresults::clear_selection {treeid} {
    if { ! [winfo exists ${treeid}] } { return }

    set bound [bind ${treeid} <<TreeviewSelect>>]
    bind ${treeid} <<TreeviewSelect>> {}
    # unselect the old ones, and select the new ones
    ${treeid} selection remove [${treeid} selection]
    ${treeid} tag remove selpkg
    foreach item [${treeid} children {}] {
        ${treeid} item ${item} -values {}
    }
    after idle "bind ${treeid} <<TreeviewSelect>> \{${bound}\}"
}


########################################################
proc ::deken::show_results {resultsid} {
    ::deken::textresults::show ${resultsid}
}
proc ::deken::clear_results {resultsid} {
    ::deken::textresults::clear ${resultsid}
}
proc ::deken::clear_selection {resultsid} {
    ::deken::textresults::clear_selection ${resultsid}
}
########################################################


## tooltips
# code based on example by Donal Fellows in 2001
# https://groups.google.com/g/comp.lang.tcl/c/IhNlXBxL1_I/m/sF4sNhpi7XQJ
namespace eval ::deken::balloon {
}

proc ::deken::balloon::show {winid x y msg {x_offset 0} {y_offset 0}} {
    set ::deken::balloon::message(${winid}) ${msg}
    if {![winfo exist ${winid}]} {
        toplevel ${winid}
        wm overrideredirect ${winid} 1
        label ${winid}.label \
            -highlightthick 0 -relief solid -borderwidth 1 \
            -textvariable ::deken::balloon::message(${winid})
        pack ${winid}.label -expand 1 -fill x
    }

    if { ${msg} == {} } {
        wm withdraw ${winid}
        return
    }

    set g [format +%d+%d [expr {${x} + ${x_offset}}] [expr {${y} + ${y_offset}}]]
    # This is probably overdoing it, but better too much than too little
    wm geometry ${winid} ${g}
    wm deiconify ${winid}
    wm geometry ${winid} ${g}
    raise ${winid}
    after idle "[list wm geometry ${winid} ${g}]; raise ${winid}"
}
proc ::deken::balloon::hide {winid} {
    if {[winfo exist ${winid}]} {
        wm withdraw ${winid}
    }
}

########################################################

proc ::deken::ask_installdir {{installdir ""} {extname ""}} {
    while {1} {
        if { "${installdir}" == "" } {
            set result [tk_messageBox \
                            -message [_ "Please select a (writable) installation directory!"] \
                            -icon warning -type retrycancel -default retry \
                            -parent ${::deken::winid}]
            switch -- "${result}" {
                cancel {return}
                retry {
                    if {[::deken::prompt_installdir]} {
                        set installdir ${::deken::installpath}
                    } else {
                        continue
                    }
                }
            }
        } else {
            set result [tk_messageBox \
                            -message [format [_ "Install %1\$s to %2\$s?" ] ${extname} ${installdir}] \
                            -icon question -type yesnocancel -default yes \
                            -parent ${::deken::winid}]
            switch -- "${result}" {
                cancel {return}
                yes { }
                no {
                    set prevpath ${::deken::installpath}
                    if {[::deken::prompt_installdir]} {
                        set keepprevpath 1
                        set installdir ${::deken::installpath}
                        # if docsdir is set & the install path is valid,
                        # saying "no" is temporary to ensure the docsdir
                        # hierarchy remains, use the Path dialog to override
                        if {[namespace exists ::pd_docsdir] && [::pd_docsdir::path_is_valid] &&
                            [file writable [file normalize ${prevpath}]] } {
                            set keepprevpath 0
                        }
                        if {${keepprevpath}} {
                            set ::deken::installpath ${prevpath}
                        }
                    } else {
                        continue
                    }
                }
            }
        }

        if { "${installdir}" != "" } {
            # try creating the installdir...just in case
            catch { file mkdir ${installdir} }
        }
        # check whether this is a writable directory
        set installdir [ ::deken::utilities::get_writabledir [list ${installdir} ] ]
        if { "${installdir}" != "" } {
            # stop looping if we've found our dir
            break
        }
    }
    return ${installdir}
}

proc ::deken::ensure_installdir {{installdir ""} {extname ""}} {
    ## make sure that the destination path exists
    ### if ::deken::installpath is set, use the first writable item
    ### if not, get a writable item from one of the searchpaths
    ### if this still doesn't help, ask the user
    if { "${installdir}" != "" } {return ${installdir}}

    set installdir [::deken::find_installpath]
    if { "${installdir}" != "" } {return ${installdir}}

    if {[namespace exists ::pd_docsdir] && [::pd_docsdir::externals_path_is_valid]} {
        # if the docspath is set, try the externals subdir
        set installdir [::pd_docsdir::get_externals_path]
    }
    if { "${installdir}" != "" } {return ${installdir}}

    # ask the user (and remember the decision)
    ::deken::prompt_installdir
    set installdir [ ::deken::utilities::get_writabledir [list ${::deken::installpath} ] ]

    return [::deken::ask_installdir [::deken::utilities::expandpath ${installdir} ] ${extname}]
}

# handle a clicked link
proc ::deken::install_link {URL filename} {
    ## make sure that the destination path exists
    ### if ::deken::installpath is set, use the first writable item
    ### if not, get a writable item from one of the searchpaths
    ### if this still doesn't help, ask the user
    variable winid
    set installbutton ${winid}.status.install
    if {[winfo exists ${installbutton}]} {
        ${installbutton} configure -state disabled
    }
    ::deken::show_tab ${winid} info

    set installdir [::deken::ensure_installdir "" ${filename}]
    if { "${installdir}" == "" } {
        ::deken::utilities::debug [format [_ "Cancelling download of '%s': No installation directory given." ] ${filename}]
        ::deken::statuspost [format [_ "Installing to non-existent directory failed" ] ${filename}] error
        return
    }
    if { ! [file exists ${installdir}] } {
        catch { file mkdir ${installdir} }
    }
    if { ! [file isdirectory ${installdir}] } {
        set msg [_ "Directory does not exist!" ]
        ::deken::post [format [_ "Unable to install to '%s'" ]  ${installdir} ] error
        ::deken::post "\t${msg}" error
        set installdir [::deken::do_prompt_installdir ${installdir}]
        if { "${installdir}" == "" } {
            #::deken::update_installbutton ${winid}
            return
        }
    }
    if { ! [file isdirectory ${installdir}] } {
        ::deken::post [format [_ "Unable to install to '%s'" ]  ${installdir} ] error
        set msg [_ "Directory does not exist!" ]
        ::deken::post "\t${msg}" error
        return
    }
    if { [::deken::utilities::get_writabledir [list ${installdir}]] == "" } {
        ::deken::post [format [_ "Unable to install to '%s'" ]  ${installdir} ] error
        set msg [_ "Directory is not writable!" ]
        ::deken::post "\t${msg}" error
        return
    }

    set parsedfilename [::deken::utilities::parse_filename ${filename}]
    set fullpkgfile [::deken::utilities::get_tmpfilename ${installdir} [::deken::utilities::get_filenameextension ${filename}] "[lindex ${parsedfilename} 0]\[[lindex ${parsedfilename} 1]\]" ]
    ::deken::statuspost [format [_ "Downloading '%s'" ] ${filename}] info 0
    ::deken::utilities::debug [format [_ "Commencing download of '%1\$s' into '%2\$s'..." ] ${URL} ${installdir}]
    ::deken::syncgui
    set fullpkgfile [::deken::utilities::download_file ${URL} ${fullpkgfile} "::deken::download_progress"]
    if { "${fullpkgfile}" eq "" } {
        ::deken::utilities::debug [_ "aborting."]
        ::deken::statuspost [format [_ "Downloading '%s' failed" ] ${filename}] error
        ::deken::progressstatus [_ "Download failed!" ]
        ::deken::progress 0
        return
    }
    set msg [_ "Download completed! Verifying..." ]
    ::deken::progressstatus ${msg}
    ::deken::post "${msg}" info

    set result [::deken::verify_sha256_gui ${URL} ${fullpkgfile}]
    if { ! ${result} } {
        # verification failed
        if { ! "${::deken::keep_package}" } {
            catch { file delete ${fullpkgfile} }
        }
        ::deken::progress 0
        return
    }
    if { ${result} < 0 } {
        # verification failed, but we ignore it
        if { ${result} > -10 } {
            ::deken::statuspost [_ "Ignoring checksum mismatch" ] info 0
        } elseif  { ${result} > -100 } {
            ::deken::statuspost [_ "Ignoring checksum errors" ] info 0
        }
    }
    ::deken::install_package ${fullpkgfile} ${filename} ${installdir} ${::deken::keep_package}
    ::deken::update_installbutton ${winid}
}

# print the download progress to the results window
proc ::deken::download_progress {token total current} {
    if { ${total} > 0 } {
        ::deken::progress [expr {round(100 * (1.0 * ${current} / ${total}))}]
    }
}

# test for platform match with our current platform
proc ::deken::architecture_match {archs} {
    if { "translations" eq "${::deken::searchtype}" } {
        foreach arch ${archs} {
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
    if { ! [llength ${archs}] } { return 1}
    set OS "$::deken::platform(os)"
    set MACHINE "$::deken::platform(machine)"
    set BITS "$::deken::platform(bits)"
    set FLOATSIZE "$::deken::platform(floatsize)"
    if { "${::deken::userplatform}" != "" } {
        ## FIXXME what if the user-supplied input isn't valid?
        regexp -- {(.*)-(.*)-(.*)} ${::deken::userplatform} _ OS MACHINE FLOATSIZE
    }
    # strip the little-endian indicator from arm-archs, it's the default
    regexp -- {(armv[0-9]*)[lL]} ${MACHINE} _ MACHINE
    set MACHINE [string tolower ${MACHINE}]
    set OS [string tolower ${OS}]

    # check each architecture in our list against the current one
    foreach arch ${archs} {
        if { [ regexp -- {(.*)-(.*)-(.*)} ${arch} _ os machine floatsize ] } {
            # normalize arm-architectures by stripping away sub-architectures
            # TODO: leave any big-endian indicator in place
            regexp -- {(armv[0-9]*)[^0-9]*} ${machine} _ machine
            set machine [string tolower ${machine}]
            set os [string tolower ${os}]
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
    set result [list]
    foreach searcher ${::deken::backends} {
        if {[catch {
            foreach r [ ${searcher} ${term} ] {
                if { "" eq [lindex ${r} 0] } {
                    # data is already normalized
                } else {
                    # legacy data format
                    foreach {title cmd match comment status} ${r} {break}
                    set r [::deken::normalize_result ${title} ${cmd} ${match} ${comment} ${status}]
                }
                lappend result [lrange ${r} 1 end]
            }
        } stdout] } {
            ::deken::utilities::debug "${searcher:} ${stdout}"
        }
    }
    return ${result}
}


proc ::deken::initialize {} {
    set label [_ "Find externals"]
    # console message to let them know we're loaded
    ## but only if we are being called as a plugin (not as built-in)
    if { "" != "${::current_plugin_loadpath}" } {
        ::pdwindow::debug [format [_ "\[deken\] deken-plugin.tcl (Pd externals search) loaded from %s." ] ${::current_plugin_loadpath} ]
        ::pdwindow::debug "\n"
    }
    set msg [format [_ "\[deken\] Platform detected: %s" ] [::deken::platform2string 1] ]
    ::pdwindow::verbose 0 "${msg}\n"

    # try to set install path when plugin is loaded
    set ::deken::installpath [::deken::find_installpath]


    # create an entry for our search in the menu (or reuse an existing one)

    # check which menus are there (for the pdwindow)
    set helpmenu {}
    set toolsmenu {}

    set pdmenu [.pdwindow cget -menu]
    for {set m 0} {$m <= [$pdmenu index end]} {incr m} {
        set _m [$pdmenu entrycget $m -menu]
        switch -glob $_m {
            *.tools { set toolsmenu $_m }
            *.help { set helpmenu $_m }
        }
    }

    # if there's a 'tools' menu, use that, otherwise use the 'help' menu
    if { [winfo exists ${toolsmenu}]} {
        # we got Tools->, so if there's Help->Find externals... entry, drop it
        catch {
            $helpmenu delete ${label}
        }
        set mymenu ${toolsmenu}
    } else {
        set mymenu ${helpmenu}
    }
    if { [winfo exists ${mymenu}] } {
        if { [catch {
            # if there's already an entry, make sure to use our 'open_searchui' rather than the built-in
            ${mymenu} entryconfigure ${label} -command {::deken::open_searchui ${::deken::winid}}
        } _ ] } {
            # otherwise create a new menu entry
            if { ${mymenu} eq ".menubar.help" } {
                ${mymenu} add separator
            }
            ${mymenu} add command -label ${label} -command {::deken::open_searchui ${::deken::winid}}
        }
    } else {
        set msg [_ "Could not find a menu for adding '%s'" ${label}]
        ::pdwindow::fatal "\[deken\] ${msg}\n"
    }
    # bind all <${::modifier}-Key-s> {::deken::open_helpbrowser .helpbrowser2}
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

    set ::deken::backends [linsert ${::deken::backends} 0 ${fun}]
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
##        e.g. "[list ::deken::install_link http://bfg.org/frobscottle-1.10.zip frobscottle-1.10.zip]"
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
## to register a new search function, call `::deken::register ${myfun}`

# namespace
## you are welcome to use the ::deken::search:: namespace



## ####################################################################
## searching puredata.info
namespace eval ::deken::search::dekenserver {
    # deken servers to use
    variable url_primary
    variable urls_secondary
    variable urls_ephemeral
    # should we actually use them?
    variable use_url_primary
    variable use_urls_secondary
    variable use_urls_ephemeral
}

# the main deken-url
::deken::utilities::setdefault ::deken::search::dekenserver::use_url_primary 1
set ::deken::search::dekenserver::url_primary "http://deken.puredata.info/search"
if { ! [catch {package present tls} stdout] } {
    set ::deken::search::dekenserver::url_primary "https://deken.puredata.info/search"
}
catch {set ::deken::search::dekenserver::url_primary $::env(DEKENSERVER)}
catch {set ::deken::search::dekenserver::url_primary $::env(DEKEN_SEARCH_URL)}


# additional (fixed) deken-servers
::deken::utilities::setdefault ::deken::search::dekenserver::use_urls_secondary 0
::deken::utilities::setdefault ::deken::search::dekenserver::urls_secondary {}

# additional (ephemeral) deken-servers
::deken::utilities::setdefault ::deken::search::dekenserver::use_urls_ephemeral 0
## those we expect
::deken::utilities::setdefault ::deken::search::dekenserver::urls_ephemeral {}
## those that are there
array set ::deken::search::dekenserver::urls_ephemeral_existing {}


proc ::deken::search::dekenserver::search {term} {
    set tmpurls {}
    foreach {k v} [array get ::deken::search::dekenserver::urls_ephemeral_existing] {
        lappend tmpurls ${v}
    }
    # all the search URLs
    set urls {}
    if { ${::deken::search::dekenserver::use_url_primary} } {
    lappend urls ${::deken::search::dekenserver::url_primary}
    }
    if { ${::deken::search::dekenserver::use_urls_secondary} } {
        set urls [concat ${urls} ${::deken::search::dekenserver::urls_secondary}]
    }
    if { ${::deken::search::dekenserver::use_urls_ephemeral} } {
        set urls [concat \
                     ${urls} \
                     [::deken::utilities::lists_intersect ${::deken::search::dekenserver::urls_ephemeral} ${tmpurls}] \
                     ]
    }

    # remove duplicate entries
    set urls [::deken::utilities::list_unique ${urls}]

    # search all the urls
    array set results {}
    set urlcount 0
    foreach s ${urls} {
        # skip empty urls
        if { ${s} eq {} } { continue }
        ::deken::post [format [_ "Searching on %s..."] ${s} ] debug
        set resultcount 0
        # get the results from the given url, and add them to our results set
        foreach r [::deken::search::dekenserver::search_server ${term} ${s}] {
            set results(${r}) {}
            incr resultcount
        }
        ::deken::post [format [_ "Searching on %1\$s returned %2\$d results"] ${s} ${resultcount}] debug
        incr urlcount
    }
    if { ${urlcount} == 0 } {
        ::deken::post [format [_ "No usable servers for searching found..."] ${urls} ] debug
    }
    set splitCont [array names results]
    if { [llength ${splitCont}] == 0 } {
        return ${splitCont}
    }

    set searchresults [list]
    # loop through the resulting tab-delimited table
    if { [catch {
        set latestrelease0 [dict create]
        set latestrelease1 [dict create]
        set newestversion [dict create]
        foreach ele ${splitCont} {
            set ele [ string trim ${ele} ]
            if { "" ne ${ele} } {
                foreach {name URL creator date} [ split ${ele} "\t" ] {break}
                set filename [ file tail ${URL} ]
                foreach {pkgname version archs} [ ::deken::utilities::parse_filename ${filename} ] {break}

                #if { ${version} eq "0.0.extended" } { set date "0000-00-00 00:02:00" }
                set olddate {}
                set match [::deken::architecture_match "${archs}" ]

                if { ${match} } {
                    catch { set olddate [dict get ${latestrelease1} ${pkgname}] }

                    set oldversion {}
                    catch { set oldversion [dict get ${newestversion} ${pkgname}]}
                    if { [::deken::versioncompare ${version} ${oldversion}] > 0 } {
                        dict set newestversion ${pkgname} ${version}
                    }
                } else {
                    catch { set olddate [dict get ${latestrelease0} ${pkgname}] }
                }

                if { ${date} > ${olddate} } {
                    dict set latestrelease${match} ${pkgname} ${date}
                }
            }
        }
    } stdout ] } {
        set latestrelease0 {}
        set latestrelease1 {}
        set newestversion {}
    }
    set vsep "\u0001"

    foreach ele ${splitCont} {
        set ele [ string trim ${ele} ]
        if { "" ne ${ele} } {
            foreach {name URL creator date} [ split ${ele} "\t" ] {break}
            set decURL [::deken::utilities::urldecode ${URL}]
            set filename [ file tail ${URL} ]
            set cmd [list ::deken::install_link ${decURL} ${filename}]
            set pkgverarch [ ::deken::utilities::parse_filename ${filename} ]
            set pkgname [lindex ${pkgverarch} 0]
            set version [lindex ${pkgverarch} 1]
            set archs [lindex ${pkgverarch} 2]

            set match [::deken::architecture_match "${archs}" ]
            set comment [format [_ "Uploaded by %1\$s @ %2\$s" ] ${creator} ${date} ]
            set status ${URL}
            set sortprefix "0000-00-00 00:01:00"
            if { ${match} == 0 } {
                catch { set sortprefix [dict get ${latestrelease0} ${pkgname}] }
            } else {
                if { "${::deken::hideoldversions}" } {
                    # If this version is not the newest one, mark it as unmatched
                    catch {
                        set oldversion [dict get ${newestversion} ${pkgname}]
                        if { [::deken::versioncompare ${version} ${oldversion}] < 0 } {
                            set match 0
                        }
                    }
                }
            }
            catch { set sortprefix [dict get ${latestrelease1} ${pkgname}] }
            # the ${vsep} should sort before all other characters that might appear in version strings,
            #   as it unsures that "1.2" sorts before "1.2-1"
            # the space (or some other character that sorts after "\t") after the ${version} is important,
            #   as it ensures that "0.2~1" sorts before "1.2"
            set sortname "${sortprefix}${vsep}${pkgname}${vsep}${version} ${vsep}${date}"
            set contextcmd [list ::deken::search::dekenserver::contextmenu %W %x %y ${pkgname} ${URL}]
            set res [list ${sortname} ${filename} ${name} ${cmd} ${match} ${comment} ${status} ${contextcmd} ${pkgname} ${version} ${creator} ${date}]
            lappend searchresults ${res}
        }
    }
    set sortedresult []
    foreach r [lsort -command ::deken::versioncompare -decreasing -index 0 ${searchresults} ] {
        foreach {sortname filename title cmd match comment status menus pkgname version creator date} ${r} {
            lappend sortedresult [::deken::normalize_result ${title} ${cmd} ${match} ${comment} ${status} ${menus} ${pkgname} ${version} ${creator} ${date}]
            break
        }
    }
    return ${sortedresult}
}

proc ::deken::search::dekenserver::search_server {term dekenurl} {
    set queryterm {}
    if { ${::deken::searchtype} eq "translations" && ${term} eq "" } {
        # special handling of searching for all translations (so we ONLY get translations)
        set term {*}
    }
    foreach x ${term} {lappend queryterm ${::deken::searchtype} ${x}}
    if { [ catch {set queryterm [::http::formatQuery {*}${queryterm} ] } stdout ] } {
        set queryterm [ join ${term} "&${::deken::searchtype}=" ]
        set queryterm "${::deken::searchtype}=${queryterm}"
    }

    # deken-specific socket config
    set httpaccept [::http::config -accept]
    set httpagent [::deken::utilities::httpuseragent]
    ::http::config -accept text/tab-separated-values

    # fetch search result
    if { [catch {
        set token [::http::geturl "${dekenurl}?${queryterm}"]
    } stdout ] } {
        set msg [format [_ "Searching for '%s' failed!" ] ${term} ]
        tk_messageBox \
            -title [_ "Search failed" ] \
            -message "${msg}\n(${dekenurl})\n${stdout}" \
            -icon error -type ok \
            -parent ${::deken::winid}
        return
    }

    # restore http settings
    ::http::config -accept ${httpaccept}
    ::http::config -useragent ${httpagent}

    set ncode [::http::ncode ${token}]
    if { ${ncode} != 200 } {
        set err [::http::code ${token}]
        set msg [_ "Unable to perform search."]
        ::deken::utilities::debug "${msg}\n   ${err}"
        return {}
    }
    set contents [::http::data ${token}]
    ::http::cleanup ${token}

    return [split ${contents} "\n"]
}

proc ::deken::search::dekenserver::contextmenu {widget theX theY pkgname URL} {
    set winid ${::deken::winid}
    set resultsid ${::deken::resultsid}
    set with_installmenu 1

    catch {
        # don't show the select/install entries when using a treeview
        ${resultsid} identify item 0 0
        set with_installmenu 0
    }

    set m .dekenresults_contextMenu
    destroy ${m}
    menu ${m}

    set saveURL [string map {"[" "%5B" "]" "%5D"} ${URL}]

    if { ${with_installmenu} } {
        set decURL [::deken::utilities::urldecode ${URL}]
        set filename [ file tail ${URL} ]
        set pkgverarch [ ::deken::utilities::parse_filename ${filename} ]
        set pkgname [lindex ${pkgverarch} 0]

        set cmd [list ::deken::install_link ${decURL} ${filename}]

        set selcount 0
        set selected 0
        foreach {k v} ${::deken::selected} {
            if { ${v} != {} } {incr selcount}
            if { (${k} eq ${pkgname}) && (${v} eq ${cmd}) } {
                set selected 1
                break
            }
        }

        set msg [_ "Select package for installation" ]
        if { ${selected} } {
            set msg [_ "Deselect package" ]
        }

        ${m} add command -label "${msg}" -command "::deken::textresults::selectpackage ${resultsid} ${pkgname} {${cmd}}"
        ${m} add separator
    }

    set infoq "url=${URL}"
    if {$::tcl_platform(platform) ne "windows"} {
        set infoq [::http::formatQuery url ${URL}]
    }

    ${m} add command -label [_ "Open package webpage" ] -command "pd_menucommands::menu_openfile \{https://deken.puredata.info/info?${infoq}\}"
    ${m} add command -label [_ "Copy package URL" ] -command "clipboard clear; clipboard append ${saveURL}"
    ${m} add command -label [_ "Copy SHA256 checksum URL" ] -command "clipboard clear; clipboard append ${saveURL}.sha256"
    ${m} add command -label [_ "Copy OpenGPG signature URL" ] -command "clipboard clear; clipboard append ${saveURL}.asc"

    set installpath [::deken::find_installpath]
    if { "${installpath}" ne {} } {
        if { [file isdir [file join ${installpath} ${pkgname}]] } {
            ${m} add separator
            ${m} add command -label [format [_ "Uninstall '%s'" ] ${pkgname}] -command [list ::deken::menu_uninstall_package ${winid} ${pkgname} ${installpath}]
        }
    }
    tk_popup ${m} [expr {[winfo rootx ${widget}] + ${theX}}] [expr {[winfo rooty ${widget}] + ${theY}}]
}


::deken::initialize
::deken::register ::deken::search::dekenserver::search
}
