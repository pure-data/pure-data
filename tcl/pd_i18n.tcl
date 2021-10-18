########### internationalization (i18n) handling
## this package provides ways to
## - set the language of the GUI (menus,...),
##   loading the necessary localization (l10n) files
## - the standard '_' command for translating strings

package provide pd_i18n 0.1
package require msgcat


namespace eval ::pd_i18n:: {
    variable podir
    variable language
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

proc ::pd_i18n::load_locale {{lang ""}} {
    if { $lang == "default" } {
        set lang ""
    }
    if { $lang == "" } {
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
        }
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
    set polanguages [list \
        af [_ "Afrikaans" ] \
        az [_ "Azerbaijani" ] \
        be [_ "Belarusian" ] \
        bg [_ "Bulgarian" ] \
        de [_ "German" ] \
        de_at [_ "German (Austria)" ] \
        el [_ "Greek" ] \
        en [_ "English" ] \
        en_ca [_ "English (Canada)" ] \
        en_gb [_ "English (UK)" ] \
        en_us [_ "English (USA)" ] \
        es [_ "Spanish" ] \
        eu [_ "Basque" ] \
        fr [_ "French" ] \
        gu [_ "Gujarati" ] \
        he [_ "Hebrew" ] \
        hi [_ "Hindi" ] \
        hu [_ "Hungarian" ] \
        it [_ "Italian" ] \
        pa [_ "Panjabi" ] \
        pt [_ "Portuguese" ] \
        pt_br [_ "Portuguese (Brazil)" ] \
        pt_pt [_ "Portuguese (Portugal)" ] \
        sq [_ "Albanian" ] \
        sv [_ "Swedish" ] \
        vi [_ "Vietnamese" ] \
    ]

    dict create polanguages $polanguages
    set havelanguages {}
    foreach pofile [glob -directory $podir -nocomplain -types {f} -- *.msg] {
        set polang [ file rootname [file tail $pofile] ]
        if [ dict exist $polanguages $polang ] {
            lappend havelanguages [ list [ dict get $polanguages $polang ] $polang ]
        } {
            lappend havelanguages [ list [_ "$polang" ] $polang ]
        }
    }
    set havelanguages [lsort -index 0 $havelanguages]
    lappend havelanguages [list [format "(%s)" [_ "default language" ] ] "default" ]
    lappend havelanguages [list [format "(%s)" [_ "no language" ] ] "." ]
}

proc ::pd_i18n::init {} {
    set language [::pd_guiprefs::read "gui_language" ]
    load_locale ${language}
}
