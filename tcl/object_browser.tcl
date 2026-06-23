# generate menu tree for native objects for the canvas right click popup
# code by Porres and Seb Shader

package require pd_menus

namespace eval category_menu {
    variable uid 0
}

proc category_menu::send_item {w x y item} {
    switch $item {
        "Message" {
            pdsend "$w msg $x $y"
        }
        "Number atom box" {
            pdsend "$w floatatom $x $y 5 0 0 0 - - - 0"
        }
        "Symbol atom box" {
            pdsend "$w symbolatom $x $y 10 0 0 0 - - - 0"
        }
        "List box" {
            pdsend "$w listbox $x $y 20 0 0 0 - - - 0"
        }
        "Comment" {
            pdsend "$w text $x $y"
        }
        "Bang" {
            pdsend "$w obj $x $y bng"
        }
        "Toggle" {
            pdsend "$w obj $x $y tgl"
        }
        "Number2" {
            pdsend "$w obj $x $y nbx"
        }
        "Vertical slider" {
            pdsend "$w obj $x $y vsl"
        }
        "Horizontal slider" {
            pdsend "$w obj $x $y hsl"
        }
        "Vertical radio" {
            pdsend "$w obj $x $y vradio"
        }
        "Horizontal radio" {
            pdsend "$w obj $x $y hradio"
        }
        "VU meter" {
            pdsend "$w obj $x $y vu"
        }
        "Canvas" {
            pdsend "$w obj $x $y cnv"
        }
        "Graph" {
            pdsend "$w graph $x $y"
        }
        "Array" {
            pdsend "$w menuarray $x $y"
        }
        default {
            pdsend "$w obj $x $y $item"
        }
    }
}

proc category_menu::load_menutree {} {
    set menutree {
        {
            guis
            {
                {
                    iemguis
                    {
                        Bang
                        Toggle
                        Number2
                        Vertical\ slider
                        Horizontal\ slider
                        Vertical\ radio
                        Horizontal\ radio
                        VU\ meter
                        Canvas
                    }
                }
                Message
                Number\ atom\ box
                Symbol\ atom\ box
                List\ box
                Comment
                Graph
                Array
            }
        }
        {
            patch/subpatch
            {
                inlet
                inlet~
                outlet
                outlet~
                pd
                block~
                switch~
                clone
                namecanvas
                loadbang
                declare
                savestate
                pdcontrol
            }
        }
        {
            control
            {
                {
                    general\ tools
                    {
                        {
                            data\ management
                            {
                                bang
                                trigger
                                route
                                swap
                                print
                                float
                                int
                                value
                                symbol
                                makefilename
                            }
                        }
                        send
                        receive
                        openpanel
                        savepanel
                        key
                        keyup
                        keyname
                        trace
                    }
                }
                {
                    list\ management
                    {
                        pack
                        unpack
                        list\ append
                        list\ prepend
                        list\ store
                        list\ split
                        list\ trim
                        list\ length
                        list\ fromsymbol
                        list\ tosymbol
                    }
                }
                {
                    text\ management
                    {
                        text\ define
                        text\ get
                        text\ set
                        text\ insert
                        text\ delete
                        text\ size
                        text\ tolist
                        text\ fromlist
                        text\ search
                        text\ sequence
                        qlist
                        textfile
                    }
                }
                {
                    file\ management
                    {
                        file\ handle
                        file\ define
                        file\ mkdir
                        file\ cwd
                        file\ patchpath
                        file\ which
                        file\ glob
                        file\ stat
                        file\ isfile
                        file\ isdirectory
                        file\ size
                        file\ copy
                        file\ move
                        file\ delete
                        file\ split
                        file\ join
                        file\ splitex
                        file\ splitname
                        file\ normalize
                        file\ isabsolute
                    }
                }
                {
                    arrays/tables
                    {
                        tabread
                        tabread4
                        tabwrite
                        soundfiler
                        table
                        array\ define
                        array\ size
                        array\ sum
                        array\ get
                        array\ set
                        array\ quantile
                        array\ random
                        array\ max
                        array\ min
                    }
                }
                {
                    time
                    {
                        delay
                        pipe
                        metro
                        line
                        timer
                        cputime
                        realtime
                    }
                }
                {
                    logic
                    {
                        select
                        change
                        spigot
                        moses
                        until
                    }
                }
                {
                    math
                    {
                        {
                            trig
                            {
                                sin
                                cos
                                tan
                                atan
                                atan2
                            }
                        }
                        {
                            unops
                            {
                                wrap
                                abs
                                sqrt
                                exp
                            }
                        }
                        {
                            binops
                            {
                                +
                                -
                                *
                                /
                                max
                                min
                                log
                                pow
                                >
                                >=
                                <
                                <=
                                ==
                                !=
                                div
                                mod
                                &&
                                ||
                                &
                                |
                                <<
                                >>
                            }
                        }
                        expr
                        clip
                        random
                    }
                }
                {
                    acoustic\ conversions
                    {
                        mtof
                        ftom
                        rmstodb
                        dbtorms
                        powtodb
                        dbtopow
                    }
                }
                {
                    midi
                    {
                        {
                            in/out
                            {
                                midiin
                                midiout
                                notein
                                noteout
                                ctlin
                                ctlout
                                pgmin
                                pgmout
                                bendin
                                bendout
                                touchin
                                touchout
                                polytouchin
                                polytouchout
                                sysexin
                                midirealtimein
                            }
                        }
                        makenote
                        stripnote
                        bag
                        poly
                    }
                }
                {
                    network/osc
                    {
                        netsend
                        netreceive
                        fudiparse
                        fudiformat
                        oscparse
                        oscformat
                    }
                }
            }
        }
        {
            audio
            {
                {
                    general\ tools
                    {
                        snake~\ in
                        snake~\ out
                        adc~
                        dac~
                        sig~
                        line~
                        vline~
                        threshold~
                        env~
                        snapshot~
                        vsnapshot~
                        bang~
                        samphold~
                        samplerate~
                        send~
                        receive~
                        throw~
                        catch~
                        readsf~
                        writesf~
                        print~
                    }
                }
                {
                    math
                    {
                        {
                            unops
                            {
                                abs~
                                sqrt~
                                rsqrt~
                                wrap~
                                exp~
                            }
                        }
                        {
                            binops
                            {
                                +~
                                -~
                                *~
                                /~
                                max~
                                min~
                                log~
                                pow~
                            }
                        }
                        {
                            fft
                            {
                                fft~
                                ifft~
                                rfft~
                                rifft~
                            }
                        }
                        expr~
                        fexpr~
                        clip~
                    }
                }
                {
                    acoustic\ conversions
                    {
                        mtof~
                        ftom~
                        rmstodb~
                        dbtorms~
                        powtodb~
                        dbtopow~
                    }
                }
                {
                    audio\ generators/tables
                    {
                        noise~
                        phasor~
                        cos~
                        osc~
                        tabosc4~
                        tabplay~
                        tabwrite~
                        tabread~
                        tabread4~
                        tabsend~
                        tabreceive~
                    }
                }
                {
                    filters
                    {
                        vcf~
                        hip~
                        lop~
                        slop~
                        bp~
                        biquad~
                        rpole~
                        rzero~
                        rzero_rev~
                        cpole~
                        czero~
                        czero_rev~
                    }
                }
                {
                    delay
                    {
                        delwrite~
                        delread~
                        delread4~
                    }
                }
            }
        }
        {
            data\ structures
            {
                struct
                drawpolygon
                filledpolygon
                drawcurve
                filledcurve
                drawnumber
                drawsymbol
                drawtext
                plot
                scalar
                pointer
                vpointer
                get
                set
                element
                getsize
                setsize
                append
            }
        }
        {
            extra
            {
                sigmund~
                bonk~
                choice
                hilbert~
                complex-mod~
                loop~
                lrshift~
                pd~
                stdout
                rev1~
                rev2~
                rev3~
                bob~
                output~
            }
        }
    }
    return $menutree
}

proc category_menu::build_menu {parent_menu node x y} {
    # node = {label {children}}, where children can include subnodes
    variable uid
    set name [lindex $node 0]
    incr uid
    set safe_name [string tolower [regsub -all {[^a-z0-9_]} $name {_}]]
    set current_menu $parent_menu.m${uid}_${safe_name}
    menu $current_menu
    $parent_menu add cascade \
        -label $name \
        -menu $current_menu
    set children [lindex $node 1]
    foreach element $children {
        # detect submenu (2 elements where the 2nd is a list)
        if {[llength $element] == 2 && [llength [lindex $element 1]] > 1} {
            build_menu $current_menu $element $x $y
        } else {
            $current_menu add command \
                -label [regsub -all {^\-$} $element {−}] \
                -command "category_menu::send_item \$::focused_window $x $y {$element}"
        }
    }
}

proc category_menu::create {cmdstring code result op} {
    if {!$::category_menu::enable_object_browser} { return }
    set mymenu [lindex $cmdstring 1]
    set x [lindex $cmdstring 3]
    set y [lindex $cmdstring 4]
    set menutree [load_menutree]
    $mymenu add separator
    set category "object-browser"
    menu $mymenu.$category
    $mymenu add cascade -label $category -menu $mymenu.$category
    foreach item $menutree {
        build_menu $mymenu.$category $item $x $y
    }
}

proc category_menu::read_browser_cfg {} {
    set ::category_menu::enable_object_browser [::pd_guiprefs::read object_browser_enabled]
    if {$::category_menu::enable_object_browser eq ""} {
        set ::category_menu::enable_object_browser 0
    }
}

proc category_menu::write_config {{filename browser.cfg}} {
    ::pd_guiprefs::write object_browser_enabled $::category_menu::enable_object_browser
}

trace add execution ::pdtk_canvas::create_popup leave category_menu::create

category_menu::read_browser_cfg
