########### internationalization (i18n) handling
## this package provides ways to
## - set the language of the GUI (menus,...),
##   loading the necessary localization (l10n) files
## - the standard '_' command for translating strings

package provide pd_i18n 0.1
package require msgcat
package require pdtcl_compat

namespace eval ::pd_i18n:: {
    variable podir [file join [file dirname [info script]] .. po]
    variable language ""
    variable systemlanguage [::msgcat::mcpreferences]
    catch {namespace import ::pdtcl_compat::dict}

    namespace export _
}

#### official GNU gettext msgcat shortcut
## initialization code:
# > package require pd_i18n
# > namespace import ::pd_i18n::_
# > ::pd_i18n::init
## usage
# > puts [_ "Quit" ]

## make `::pd_i18n::_` (and thus `_`) an alias for `::msgcat::mc` (with varargs)
if {$::tcl_version < 8.5} {
    # uplevel works an all versions of Tcl-8.x
    proc ::pd_i18n::_ args {return [uplevel 0 [concat ::msgcat::mc $args]]}
} else {
    # the `{*}` operator was introduced with Tcl-8.5
    proc ::pd_i18n::_ {args} {return [::msgcat::mc {*}$args]}
}

proc ::pd_i18n::load_locale {{lang ""}} {
    # loads the current locale, can only be called once!
    # <lang>=""  : use the preferred language from the system
    # <lang>="default": use the preferred language from the system (legacy)
    # <lang>="." : no translation
    # <lang>="fr": french

    ::msgcat::mcload $::pd_i18n::podir

    if { $lang == "default" } {
        set lang ""
    }
    if { $lang != "" } {
        ::msgcat::mclocale $lang
    }
    set ::pd_i18n::language $lang

    # we can only change the language once
    # (otherwise the menus will play havoc)
    # so replace *this* proc with a dummy
    proc [info level 0] {{lang ""}} {
        ::msgcat::mcload $::pd_i18n::podir
        if { $lang != "" && $lang != $::pd_i18n::language} {
            set msg [_ "The language can only be set during startup."]
            ::pdwindow::error "${msg}\n"
        }
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

    # TODO: get default language
    #set lang [lindex [split [ ::pd_i18n::get_system_language ] ":" ] 0]
    #if [ dict exists $polanguages $lang ] {
    #    set lang [ dict get $polanguages $lang ]
    #}
    foreach lang $::pd_i18n::systemlanguage {
        if [ dict exists $polanguages $lang ] {
            set lang [ dict get $polanguages $lang ]
            break
        }
    }
    if { $lang == "" } {
        # the system language was not in our list.
        # either it's a not-yet-supported language (in which case we print its short name)
        # or it is the default
        set lang [lindex $::pd_i18n::systemlanguage 0]
        if { $lang == "" || [string tolower $lang] == "c" } {
            set lang [_ "(no translation)" ]
        }
    }
    lappend havelanguages [list [_ "(default language: %s)" $lang] "default" ]
    lappend havelanguages [list [_ "(no translation)" ] "." ]
    return $havelanguages
}

proc ::pd_i18n::init {} {
    set language [::pd_guiprefs::read "gui_language" ]
    load_locale ${language}
}
