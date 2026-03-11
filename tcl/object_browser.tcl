# generate menu tree for native objects for the canvas right click popup
# code by Porres and Seb Shader

package require pd_menus

namespace eval category_menu {
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
        {guis
            {Message Number\ atom\ box Symbol\ atom\ box List\ box Comment Bang Toggle Number2 Vertical\ slider Horizontal\ slider Vertical\ radio Horizontal\ radio VU\ meter Canvas Graph Array}}
        {patch/subpatch
            {inlet inlet~ outlet outlet~ pd block~ switch~ clone namecanvas loadbang declare savestate pdcontrol}}
        {control
            {general\ data\ management
                {bang trigger route swap print float int value symbol makefilename send receive}}
            {list\ management
                {pack unpack list\ append list\ prepend list\ store list\ split list\ trim list\ length list\ fromsymbol list\ tosymbol}}
            {arrays/tables
                {tabread tabread4 tabwrite soundfiler table array\ define array\ size array\ sum array\ get array\ set array\ quantile array\ random array\ max array\ min}}
            {text\ management
                {qlist textfile text\ define text\ get text\ set text\ insert text\ delete text\ size text\ tolist text\ fromlist text\ search text\ sequence}}
            {file\ management
                {file\ handle file\ define file\ mkdir  file\ cwd file\ patchpath file\ which file\ glob file\ stat file\ isfile file\ isdirectory file\ size file\ copy file\ move file\ delete file\ split file\ join file\ splitex file\ splitname file\ normalize file\ isabsolute}}
            {time
                {delay pipe metro line timer cputime realtime}}
            {logic
                {select change spigot moses until}}
            {math
                {expr clip random + - * / max min > >= < <= == != div mod && || & | << >> wrap abs sqrt exp log pow sin cos tan atan atan2}}
            {acoustic\ conversions
                {mtof ftom rmstodb dbtorms powtodb dbtopow}}
            {midi/osc
                {midiin midiout notein noteout ctlin ctlout pgmin pgmout bendin bendout touchin touchout polytouchin polytouchout sysexin midirealtimein makenote stripnote poly oscparse oscformat}}
            {misc
                {openpanel savepanel key keyup keyname netsend netreceive fudiparse fudiformat bag trace}}
        }
        {audio
            {general\ audio\ tools
                {snake~\ in snake~\ out adc~ dac~ sig~ line~ vline~ threshold~ env~ snapshot~ vsnapsot~ bang~ samphold~ samplerate~ send~ receive~ throw~ catch~ readsf~ writesf~ print~}}
            {signal\ math
                {fft~ ifft~ rfft~ rifft~ expr~ fexpr~ +~ -~ *~ /~ max~ min~ log~ pow~ abs~ sqrt~ rsqrt~ wrap~ exp~ clip~}}
            {signal\ acoustic\ conversions
                {mtof~ ftom~ rmstodb~ dbtorms~ powtodb~ dbtopow~}}
            {audio\ generators/tables
                {noise~ phasor~ cos~ osc~ tabosc4~ tabplay~ tabwrite~ tabread~ tabread4~ tabsend~ tabreceive~}}
            {audio\ filters
                {vcf~ hip~ lop~ slop~ bp~ biquad~ rpole~ rzero~ rzero_rev~ cpole~ czero~ czero_rev~}}
            {audio\ delay
                {delwrite~ delread~ delread4~}}
        }
        {data\ structures
            {struct drawpolygon filledpolygon drawcurve filledcurve drawnumber drawsymbol drawtext plot scalar pointer vpointer get set element getsize setsize append}}
        {extra
            {sigmund~ bonk~ choice hilbert~ complex-mod~ loop~ lrshift~ pd~ stdout rev1~ rev2~ rev3~ bob~ output~}}
    }
    return $menutree
}

proc category_menu::build_menu {parent_menu node x y} {
    set name [lindex $node 0]
    # Create menu for this node
    set current_menu $parent_menu.$name
    menu $current_menu
    $parent_menu add cascade -label $name -menu $current_menu
    # Process each remaining element directly (these are submenus or item lists)
    set remaining [lrange $node 1 end]
    set count 0
    foreach element $remaining {
        set second [lindex $element 1]
        # Determine if this element is a submenu or item list
        # A submenu has: first part is a name (string), remaining parts are lists
        set is_submenu 0
        if {[llength $element] >= 2} {
            set is_submenu 1
            for {set j 1} {$j < [llength $element]} {incr j} {
                if {[llength [lindex $element $j]] <= 1} {
                    set is_submenu 0
                    break
                }
            }
        }
        if {$is_submenu} {
            build_menu $current_menu $element $x $y
        } elseif {[llength $element] > 1} {
            # Element is a list of items - add each as command
            foreach item $element {
                $current_menu add command \
                    -label [regsub -all {^\-$} $item {−}] \
                    -command "menu_send_else_obj \$::focused_window $x $y {$item}"
            }
        } else {
            # Element is a single item
            $current_menu add command \
                -label [regsub -all {^\-$} $element {−}] \
                -command "menu_send_else_obj \$::focused_window $x $y {$element}"
        }
        incr count
    }
}

proc category_menu::create {cmdstring code result op} {
    if {!$::category_menu::enabled} { return }
    set mymenu [lindex $cmdstring 1]
    set x [lindex $cmdstring 3]
    set y [lindex $cmdstring 4]
    set menutree [load_menutree]
    $mymenu add separator
    set category "object-browser"
    menu $mymenu.$category
    $mymenu add cascade -label $category -menu $mymenu.$category
    # Process each top-level item
    foreach item $menutree {
        build_menu $mymenu.$category $item $x $y
    }
}

proc category_menu::read_browser_cfg {} {
    set ::category_menu::enabled [::pd_guiprefs::read object_browser_enabled]
    if {$::category_menu::enabled eq ""} {
        set ::category_menu::enabled 1
    }
}

proc category_menu::write_config {{filename browser.cfg}} {
    ::pd_guiprefs::write object_browser_enabled $::category_menu::enabled
}

trace add execution ::pdtk_canvas::create_popup leave category_menu::create

proc category_menu::add_menu_entry {} {
    .preferences add separator
    .preferences add checkbutton \
        -label [_ "Enable Object Browser"] \
        -variable ::category_menu::enabled \
        -command {category_menu::write_config}
}

category_menu::read_browser_cfg
category_menu::add_menu_entry
