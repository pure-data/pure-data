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
        {add\ object
            {guis
                {Message Number\ atom\ box Symbol\ atom\ box List\ box Comment Bang Toggle Number2 Vertical\ slider Horizontal\ slider Vertical\ radio Horizontal\ radio VU\ meter Canvas Graph Array}}
            {general\ data\ management
                {bang trigger route swap print float int value symbol makefilename send receive}}
            {list\ management
                {pack unpack list\ append list\ prepend list\ store list\ split list\ trim list\ length list\ fromsymbol list\ tosymbol}}
            {arrays/tables
                {tabread tabread4 tabwrite soundfiler table array\ define array\ size array\ sum array\ get array\ set array\ quantile array\ random array\ max array\ min}}
            {text\ management
                {qlist textfile text\ define text\ get text\ set text\ insert text\ delete text\ size text\ tolist text\ fromlist text\ search text\ sequence}}
            {file\ management
                {file\ handle file\ define file\ mkdir file\ which file\ glob file\ stat file\ isdirectory file\ isfile file\ size file\ copy file\ move file\ delete file\ split file\ splitex file\ join file\ splitname}}
            {time
                {delay pipe metro line timer cputime realtime}}
            {logic
                {select change spigot moses until}}
            {math
                {expr clip random + - * / max min > >= < <= == != div mod && || & | << >> sin cos tan atan atan2 wrap abs sqrt exp log pow}}
            {acoustic\ conversions
                {mtof ftom rmstodb dbtorms powtodb dbtopow}}
            {midi/osc
                {midiin midiout notein noteout ctlin ctlout pgmin pgmout bendin bendout touchin touchout polytouchin polytouchout sysexin midirealtimein makenote stripnote poly oscparse oscformat}}
            {misc
                {openpanel savepanel key keyup keyname netsend netreceive fudiparse fudiformat bag trace}}
            {general\ audio\ tools
                {snake~\ in snake~\ out adc~ dac~ sig~ line~ vline~ threshold~ env~ snapshot~ vsnapsot~ bang~ samphold~ samplerate~ send~ receive~ throw~ catch~ readsf~ writesf~ print~}}
            {signal\ math
                {fft~ ifft~ rfft~ irfft~ expr~ fexpr~ +~ -~ *~ /~ max~ min~ clip~ sqrt~ rsqrt~ wrap~ pow~ exp~ log~ abs~}}
            {signal\ acoustic\ conversions
                {mtof~ ftom~ rmstodb~ dbtorms~ powtodb~ dbtopow~}}
            {audio\ generators/tables
                {noise~ phasor~ cos~ osc~ tabosc4~ tabplay~ tabwrite~ tabread~ tabread4~ tabsend~ tabreceive~}}
            {audio\ filters
                {vcf~ hip~ lop~ slop~ bp~ biquad~ rpole~ rzero~ rzero_rev~ cpole~ czero~ czero_rev~}}
            {audio\ delay
                {delwrite~ delread~ delread4~}}
            {patch/subpatch
                {loadbang declare savestate clone pdcontrol pd inlet inlet~ outlet outlet~ namecanvas block~ switch~}}
            {data\ structures
                {struct drawpolygon filledpolygon drawcurve filledcurve drawnumber drawsymbol drawtext plot scalar pointer get set element getsize setsize append}}
            {extra
                {sigmund~ bonk~ choice hilbert~ complex-mod~ loop~ lrshift~ pd~ stdout rev1~ rev2~ rev3~ bob~ output~}}
        }
    }
    return $menutree
}

proc category_menu::create {cmdstring code result op} {
    set mymenu [lindex $cmdstring 1]
    set x [lindex $cmdstring 3]
    set y [lindex $cmdstring 4]
    set menutree [load_menutree]
    $mymenu add separator
    foreach categorylist $menutree {
        set category [lindex $categorylist 0]
        menu $mymenu.$category
        $mymenu add cascade -label $category -menu $mymenu.$category
        foreach subcategorylist [lrange $categorylist 1 end] {
            set subcategory [lindex $subcategorylist 0]
            menu $mymenu.$category.$subcategory
            $mymenu.$category add cascade -label $subcategory -menu $mymenu.$category.$subcategory
            foreach item [lindex $subcategorylist end] {
                # replace the normal dash with a Unicode minus so that Tcl does not
                # interpret the dash in the -label to make it a separator
                $mymenu.$category.$subcategory add command \
                    -label [regsub -all {^\-$} $item {−}] \
                    -command "::category_menu::send_item \$::focused_window $x $y {$item}"
            }
        }
    }
}

trace add execution ::pdtk_canvas::create_popup leave category_menu::create