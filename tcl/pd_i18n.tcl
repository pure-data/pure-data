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
        aa "Afar" [_ "Afar" ] \
        ab "Abkhazian" [_ "Abkhazian" ] \
        ae "Avestan" [_ "Avestan" ] \
        af "Afrikaans" [_ "Afrikaans" ] \
        ak "Akan" [_ "Akan" ] \
        am "Amharic" [_ "Amharic" ] \
        an "Aragonese" [_ "Aragonese" ] \
        ar "Arabic" [_ "Arabic" ] \
        as "Assamese" [_ "Assamese" ] \
        av "Avaric" [_ "Avaric" ] \
        ay "Aymara" [_ "Aymara" ] \
        az "Azerbaijani" [_ "Azerbaijani" ] \
        ba "Bashkir" [_ "Bashkir" ] \
        be "Belarusian" [_ "Belarusian" ] \
        bg "Bulgarian" [_ "Bulgarian" ] \
        bh "Bihari" [_ "Bihari" ] \
        bi "Bislama" [_ "Bislama" ] \
        bm "Bambara" [_ "Bambara" ] \
        bn "Bengali" [_ "Bengali" ] \
        bo "Tibetan" [_ "Tibetan" ] \
        br "Breton" [_ "Breton" ] \
        bs "Bosnian" [_ "Bosnian" ] \
        ca "Catalan" [_ "Catalan" ] \
        ce "Chechen" [_ "Chechen" ] \
        ch "Chamorro" [_ "Chamorro" ] \
        co "Corsican" [_ "Corsican" ] \
        cr "Cree" [_ "Cree" ] \
        cs "Czech" [_ "Czech" ] \
        cv "Chuvash" [_ "Chuvash" ] \
        cy "Welsh" [_ "Welsh" ] \
        da "Danish" [_ "Danish" ] \
        de "German" [_ "German" ] \
        de_at "German (Austria)" [_ "German (Austria)" ] \
        dv "Dhivehi" [_ "Dhivehi" ] \
        dz "Dzongkha" [_ "Dzongkha" ] \
        ee "Ewe" [_ "Ewe" ] \
        el "Greek" [_ "Greek" ] \
        en "English" [_ "English" ] \
        en_ca "English (Canada)" [_ "English (Canada)" ] \
        en_gb "English (UK)" [_ "English (UK)" ] \
        en_us "English (USA)" [_ "English (USA)" ] \
        eo "Esperanto" [_ "Esperanto" ] \
        es "Spanish" [_ "Spanish" ] \
        et "Estonian" [_ "Estonian" ] \
        eu "Basque" [_ "Basque" ] \
        fa "Persian" [_ "Persian" ] \
        ff "Fulah" [_ "Fulah" ] \
        fi "Finnish" [_ "Finnish" ] \
        fj "Fijian" [_ "Fijian" ] \
        fo "Faroese" [_ "Faroese" ] \
        fr "French" [_ "French" ] \
        fy "Frisian" [_ "Frisian" ] \
        ga "Irish" [_ "Irish" ] \
        gd "Gaelic" [_ "Gaelic" ] \
        gl "Galician" [_ "Galician" ] \
        gn "Guarani" [_ "Guarani" ] \
        gu "Gujarati" [_ "Gujarati" ] \
        gv "Manx" [_ "Manx" ] \
        ha "Hausa" [_ "Hausa" ] \
        he "Hebrew" [_ "Hebrew" ] \
        hi "Hindi" [_ "Hindi" ] \
        ho "Hiri Motu" [_ "Hiri Motu" ] \
        hr "Croatian" [_ "Croatian" ] \
        ht "Haitian" [_ "Haitian" ] \
        hu "Hungarian" [_ "Hungarian" ] \
        hy "Armenian" [_ "Armenian" ] \
        hz "Herero" [_ "Herero" ] \
        ia "Interlingua" [_ "Interlingua" ] \
        id "Indonesian" [_ "Indonesian" ] \
        ie "Occidental" [_ "Occidental" ] \
        ig "Igbo" [_ "Igbo" ] \
        ii "Nuosu" [_ "Nuosu" ] \
        ik "Inupiaq" [_ "Inupiaq" ] \
        io "Ido" [_ "Ido" ] \
        is "Icelandic" [_ "Icelandic" ] \
        it "Italian" [_ "Italian" ] \
        iu "Inuktitut" [_ "Inuktitut" ] \
        ja "Japanese" [_ "Japanese" ] \
        jv "Javanese" [_ "Javanese" ] \
        ka "Georgian" [_ "Georgian" ] \
        kg "Kongo" [_ "Kongo" ] \
        ki "Gikuyu" [_ "Gikuyu" ] \
        kj "Kwanyama" [_ "Kwanyama" ] \
        kk "Kazakh" [_ "Kazakh" ] \
        kl "Greenlandic" [_ "Greenlandic" ] \
        kn "Kannada" [_ "Kannada" ] \
        ko "Korean" [_ "Korean" ] \
        kr "Kanuri" [_ "Kanuri" ] \
        ks "Kashmiri" [_ "Kashmiri" ] \
        ku "Kurdish" [_ "Kurdish" ] \
        kv "Komi" [_ "Komi" ] \
        kw "Cornish" [_ "Cornish" ] \
        ky "Kyrgyz" [_ "Kyrgyz" ] \
        la "Latin" [_ "Latin" ] \
        lb "Luxembourgish" [_ "Luxembourgish" ] \
        lg "Luganda" [_ "Luganda" ] \
        li "Limburgish" [_ "Limburgish" ] \
        ln "Lingala" [_ "Lingala" ] \
        lo "Lao" [_ "Lao" ] \
        lt "Lithuanian" [_ "Lithuanian" ] \
        lu "Luba-Katanga" [_ "Luba-Katanga" ] \
        lv "Latvian" [_ "Latvian" ] \
        mg "Malagasy" [_ "Malagasy" ] \
        mh "Marshallese" [_ "Marshallese" ] \
        mi "Maori" [_ "Maori" ] \
        mk "Macedonian" [_ "Macedonian" ] \
        ml "Malayalam" [_ "Malayalam" ] \
        mn "Mongolian" [_ "Mongolian" ] \
        mr "Marathi" [_ "Marathi" ] \
        ms "Malay" [_ "Malay" ] \
        mt "Maltese" [_ "Maltese" ] \
        my "Burmese" [_ "Burmese" ] \
        na "Nauru" [_ "Nauru" ] \
        ne "Nepali" [_ "Nepali" ] \
        ng "Ndonga" [_ "Ndonga" ] \
        nl "Dutch" [_ "Dutch" ] \
        nn "Norwegian Nynorsk" [_ "Norwegian Nynorsk" ] \
        nv "Navaho" [_ "Navaho" ] \
        ny "Nyanja" [_ "Nyanja" ] \
        oc "Occitan" [_ "Occitan" ] \
        oj "Ojibwe" [_ "Ojibwe" ] \
        om "Oromo" [_ "Oromo" ] \
        or "Odia" [_ "Odia" ] \
        os "Ossetian" [_ "Ossetian" ] \
        pa "Panjabi" [_ "Panjabi" ] \
        pi "Pali" [_ "Pali" ] \
        pl "Polish" [_ "Polish" ] \
        ps "Pashto" [_ "Pashto" ] \
        pt "Portuguese" [_ "Portuguese" ] \
        pt_br "Portuguese (Brazil)" [_ "Portuguese (Brazil)" ] \
        pt_pt "Portuguese (Portugal)" [_ "Portuguese (Portugal)" ] \
        qu "Quechua" [_ "Quechua" ] \
        rm "Romansh" [_ "Romansh" ] \
        rn "Rundi" [_ "Rundi" ] \
        ro "Romanian" [_ "Romanian" ] \
        ru "Russian" [_ "Russian" ] \
        rw "Kinyarwanda" [_ "Kinyarwanda" ] \
        sa "Sanskrit" [_ "Sanskrit" ] \
        sc "Sardinian" [_ "Sardinian" ] \
        sd "Sindhi" [_ "Sindhi" ] \
        sg "Sango" [_ "Sango" ] \
        si "Sinhala" [_ "Sinhala" ] \
        sk "Slovak" [_ "Slovak" ] \
        sl "Slovenian" [_ "Slovenian" ] \
        sm "Samoan" [_ "Samoan" ] \
        sn "Shona" [_ "Shona" ] \
        so "Somali" [_ "Somali" ] \
        sq "Albanian" [_ "Albanian" ] \
        sr "Serbian" [_ "Serbian" ] \
        ss "Swati" [_ "Swati" ] \
        su "Sundanese" [_ "Sundanese" ] \
        sv "Swedish" [_ "Swedish" ] \
        sw "Swahili" [_ "Swahili" ] \
        ta "Tamil" [_ "Tamil" ] \
        te "Telugu" [_ "Telugu" ] \
        tg "Tajik" [_ "Tajik" ] \
        th "Thai" [_ "Thai" ] \
        ti "Tigrinya" [_ "Tigrinya" ] \
        tk "Turkmen" [_ "Turkmen" ] \
        tl "Tagalog" [_ "Tagalog" ] \
        tn "Tswana" [_ "Tswana" ] \
        to "Tongan" [_ "Tongan" ] \
        tr "Turkish" [_ "Turkish" ] \
        ts "Tsonga" [_ "Tsonga" ] \
        tt "Tatar" [_ "Tatar" ] \
        tw "Twi" [_ "Twi" ] \
        ty "Tahitian" [_ "Tahitian" ] \
        ug "Uyghur" [_ "Uyghur" ] \
        uk "Ukrainian" [_ "Ukrainian" ] \
        ur "Urdu" [_ "Urdu" ] \
        uz "Uzbek" [_ "Uzbek" ] \
        ve "Venda" [_ "Venda" ] \
        vi "Vietnamese" [_ "Vietnamese" ] \
        vo "Volapük" [_ "Volapük" ] \
        wa "Walloon" [_ "Walloon" ] \
        wo "Wolof" [_ "Wolof" ] \
        xh "Xhosa" [_ "Xhosa" ] \
        yi "Yiddish" [_ "Yiddish" ] \
        yo "Yoruba" [_ "Yoruba" ] \
        za "Zhuang" [_ "Zhuang" ] \
        zh_tw "Chinese (Traditional)" [_ "Chinese (Traditional)" ] \
        zu "Zulu" [_ "Zulu" ] \
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
