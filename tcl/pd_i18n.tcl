########### internationalization (i18n) handling
## this package provides ways to
## - set the language of the GUI (menus,...),
##   loading the necessary localization (l10n) files
## - the standard '_' command for translating strings

package provide pd_i18n 0.1
package require msgcat
package require pdtcl_compat

namespace eval ::pd_i18n:: {
    variable podir
    variable language
    catch {namespace import ::pdtcl_compat::dict}

    namespace export _
}
set ::pd_i18n::language ""

set ::pd_i18n::podir  [file join [file dirname [info script]] .. po]


#### official GNU gettext msgcat shortcut
## initialization code:
# > package require pd_i18n
# > namespace import ::pd_i18n::_
# > ::pd_i18n::init
## usage
# > puts [_ "Quit" ]
proc ::pd_i18n::_ {s} {return [::msgcat::mc $s]}

proc ::pd_i18n::get_system_language {} {
    # on any UNIX-like environment, Tcl should automatically use LANG, LC_ALL,
    # etc. otherwise we need to dig it up.  Mac OS X only uses LANG, etc. from
    # the Terminal, and Windows doesn't have LANG, etc unless you manually set
    # it up yourself.  Windows apps don't use the locale env vars usually.
    if {$::tcl_platform(os) eq "Darwin" && ! [info exists ::env(LANG)]} {
        # http://thread.gmane.org/gmane.comp.lang.tcl.mac/5215
        # http://thread.gmane.org/gmane.comp.lang.tcl.mac/6433
        if {![catch "exec defaults read com.apple.dock loc" lang]} {
            # ...
        } elseif {![catch "exec defaults read NSGlobalDomain AppleLocale" lang]} {
            # ...
        }
    } elseif {$::tcl_platform(platform) eq "windows"} {
        # using LANG on Windows is useful for easy debugging
        if {[info exists ::env(LANG)] && $::env(LANG) ne "C" && $::env(LANG) ne ""} {
            set lang $::env(LANG)
        } elseif {![catch {package require registry}]} {
            set lang [string tolower \
                          [string range \
                               [registry get {HKEY_CURRENT_USER\Control Panel\International} sLanguage] 0 1] ]
        }
    } elseif {[info exists ::env(LANG)]} {
        set lang $::env(LANG)
    } else {
        set lang ""
    }
    set lang [file rootname [string tolower $lang]]
    if { $lang == "c" } {
        set lang "C"
    }
    return $lang
}

proc ::pd_i18n::load_locale {{lang ""}} {
    if { $lang == "default" } {
        set lang ""
    }
    if { $lang == "" } {
        set lang [lindex [split [ ::pd_i18n::get_system_language ] ":" ] 0]
    }

    if { $::pd_i18n::language == "" || $lang == $::pd_i18n::language } {
        if { $lang != "" } {
            ::msgcat::mclocale $lang
        }
        ::msgcat::mcload $::pd_i18n::podir
        set ::pd_i18n::language $lang
    }
}


proc ::pd_i18n::get_available_languages {{podir ""}} {
    if { $podir == "" } {
        set podir $::pd_i18n::podir
    }
    set ::pd_i18n::podir $podir
    # translate language-codes
    set polanguagelist [list \
        af "Afrikaans" [_ "Afrikaans" ] \
        ar "Arabic" [_ "Arabic" ] \
        az "Azerbaijani" [_ "Azerbaijani" ] \
        be "Belarusian" [_ "Belarusian" ] \
        bg "Bulgarian" [_ "Bulgarian" ] \
        de "German" [_ "German" ] \
        de_at "German (Austria)" [_ "German (Austria)" ] \
        el "Greek" [_ "Greek" ] \
        en "English" [_ "English" ] \
        en_ca "English (Canada)" [_ "English (Canada)" ] \
        en_gb "English (UK)" [_ "English (UK)" ] \
        en_us "English (USA)" [_ "English (USA)" ] \
        es "Spanish" [_ "Spanish" ] \
        eu "Basque" [_ "Basque" ] \
        fi "Finnish" [_ "Finnish" ] \
        fr "French" [_ "French" ] \
        gu "Gujarati" [_ "Gujarati" ] \
        he "Hebrew" [_ "Hebrew" ] \
        hi "Hindi" [_ "Hindi" ] \
        hu "Hungarian" [_ "Hungarian" ] \
        hy "Armenian" [_ "Armenian" ] \
        it "Italian" [_ "Italian" ] \
        id "Indonesian" [_ "Indonesian" ] \
        ja "Japanese" [_ "Japanese" ] \
        ko "Korean" [_ "Korean" ] \
        nl "Dutch" [_ "Dutch" ] \
        pa "Panjabi" [_ "Panjabi" ] \
        pl "Polish" [_ "Polish" ] \
        pt "Portuguese" [_ "Portuguese" ] \
        pt_br "Portuguese (Brazil)" [_ "Portuguese (Brazil)" ] \
        pt_pt "Portuguese (Portugal)" [_ "Portuguese (Portugal)" ] \
        ru "Russian" [_ "Russian" ] \
        sq "Albanian" [_ "Albanian" ] \
        sv "Swedish" [_ "Swedish" ] \
        tr "Turkish" [_ "Turkish" ] \
        uk "Ukrainian" [_ "Ukrainian" ] \
        vi "Vietnamese" [_ "Vietnamese" ] \
        zh_tw "Chinese (Traditional)" [_ "Chinese (Traditional)" ] \
    ]

    set polanguages [dict create]
    foreach {k v1 v2} $polanguagelist {
        if { $v1 == $v2 } {
            set v $v1
        } else {
            set v "$v1 - $v2"
        }
        dict set polanguages $k $v
    }
    set havelanguages {}
    foreach pofile [glob -directory $podir -nocomplain -types {f} -- *.msg] {
        set polang [ file rootname [file tail $pofile] ]
        if [ dict exists $polanguages $polang ] {
            lappend havelanguages [ list [ dict get $polanguages $polang ] $polang ]
        } {
            lappend havelanguages [ list [_ "$polang" ] $polang ]
        }
    }
    set havelanguages [lsort -index 0 $havelanguages]

    set lang [lindex [split [ ::pd_i18n::get_system_language ] ":" ] 0]
    if [ dict exists $polanguages $lang ] {
        set lang [ dict get $polanguages $lang ]
    }
    lappend havelanguages [list [format [_ "(default language: %s)" ] $lang] "default" ]
    lappend havelanguages [list [_ "(no translation)" ] "." ]
    return $havelanguages
}

proc ::pd_i18n::init {} {
    set language [::pd_guiprefs::read "gui_language" ]
    load_locale ${language}
}
