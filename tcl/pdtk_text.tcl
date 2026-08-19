
package provide pdtk_text 0.1

# these procs are currently all in the global namespace because all of them
# are used by 'pd' and therefore need to be in the global namespace.

namespace eval ::pdtk_text:: {
# proc to sanitize the 'text'
    proc unescape {text} {
        return [subst -nocommands -novariables $text]
    }
}

# create a new text object (ie. obj, msg, comment)
# the initializing string ends in an extra space.  This is done in case
# the last character should have been a backslash ('\') which would have
# had the effect of escaping the closing brace.  We trim off the last
# character in the string to compensate via [string range].
proc pdtk_text_new {tkcanvas tags x y text font_size color} {
    $tkcanvas create text $x $y -tags $tags \
        -text [::pdtk_text::unescape $text] \
            -fill $color -anchor nw -font [get_font_for_size $font_size]
    set mytag [lindex $tags 0]
    $tkcanvas bind $mytag <Home> "$tkcanvas icursor $mytag 0"
    $tkcanvas bind $mytag <End>  "$tkcanvas icursor $mytag end"
    # select all
    $tkcanvas bind $mytag <Triple-ButtonRelease-1>  \
        "pdtk_text_selectall $tkcanvas $mytag"
    if {$::windowingsystem eq "aqua"} { # emacs bindings for Mac OS X
        $tkcanvas bind $mytag <Control-a> "$tkcanvas icursor $mytag 0"
        $tkcanvas bind $mytag <Control-e> "$tkcanvas icursor $mytag end"
    }
}

# change the text in an existing text box
proc pdtk_text_set {tkcanvas tag text} {
    $tkcanvas itemconfig $tag -text [::pdtk_text::unescape $text]
}


# send "pastetext" messages to send a properly escaped (UTF-8 encoded)
# text to the patch.
# follow the text with a terminating "-2" at which point the patch carries
# out the paste.
proc ::pdtk_text::pasteutf8chars {w buf} {
    set command "$w pastechars"
    pdsend "${command} -1"
    set maxlen [expr 75 - [string length $command] - 1]
    set chars {}
    for {set i 0} {$i < [expr [string length $buf] - 1]} {incr i 1} {
        scan [string index $buf $i] %c keynum
        lappend chars ${keynum}
        if { [string length ${chars}] > 75 } {
            pdsend "$command ${chars}"
            set chars {}
        }
    }
    if [llength $chars] {
        pdsend "${command} ${chars}"
    }
    pdsend "$w pastechars -2"
}

# paste into an existing text box by literally "typing" the contents of the
# clipboard, i.e. send the contents one character at a time via 'pd key'
proc pdtk_pastetext {tkcanvas} {
    set buf [::pdtk_clipboard_get]
    if { ${buf} ne {} }  {
        # turn unicode-encoded stuff (\u...) into unicode characters
        # 'unescape' needs a trailing space...
        set buf [::pdtk_text::unescape "${buf} " ]
        for {set i 0} {$i < [expr [string length $buf] - 1]} {incr i 1} {
            set cha [string index $buf $i]
            scan $cha %c keynum
            pdsend "[winfo toplevel $tkcanvas] key 1 $keynum 0"
        }
    }
}

# fetch the clipboard and send it to the patch
proc pdtk_pasteany {tkcanvas} {
    set buf [::pdtk_clipboard_get]
    if { ${buf} ne {} }  {
        # the clipboard is unicode-encoded; we need the raw utf-8 bytes
        set utf8buf [encoding convertto utf-8 ${buf}]
        ::pdtk_text::pasteutf8chars [winfo toplevel $tkcanvas] ${utf8buf}
    }
}

## receive a text from Pd and put it into the clipboard
proc pdtk_clipboard_set {text} {
    set escaped [::pdtk_text::unescape ${text}]
    clipboard clear
    clipboard append $escaped
}
## get the current text in the clipboard
proc pdtk_clipboard_get {} {
    # 1st try to get the text as an UTF-8 string
    set buf {}

    if { ${buf} eq {} } {
        catch {
            set buf [clipboard get -type UTF8_STRING]
        }
    }
    # if that failed (e.g. on Windows),
    # try to get the text as an ordinary string
    if { ${buf} eq {} } {
        catch {
            set buf [clipboard get]
        }
    }

    return ${buf}
}

# select all of the text in an existing text box
proc pdtk_text_selectall {tkcanvas mytag} {
    if {$::editmode([winfo toplevel $tkcanvas])} {
        $tkcanvas select from $mytag 0
        $tkcanvas select to $mytag end
    }
}

# de/activate a text box for editing based on $editing flag
proc pdtk_text_editing {mytoplevel tag editing} {
    set tkcanvas [tkcanvas_name $mytoplevel]
    if {$editing == 0} {selection clear $tkcanvas}
    $tkcanvas focus $tag
    set ::editingtext($mytoplevel) $editing
}
