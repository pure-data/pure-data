package provide pd_bindings 0.1

package require pd_menucommands
package require dialog_find
package require pd_connect
package require pd_guiprefs

namespace eval ::pd_bindings:: {
    namespace export global_bindings
    namespace export dialog_bindings
    namespace export patch_bindings
    variable key2iso ""

    variable alt "Alt"
    variable control "Control"

    # on Mac OS X/Aqua, the Alt/Option key is called Option in Tcl
    if {[tk windowingsystem] eq "aqua"} {
        set alt "Option"
        set control "Command"
    }

    # note: we avoid CMD-H & CMD+Shift-H as it hides Pd on macOS
    variable bindlist {}
    lappend bindlist "<<File|New>>"               [list <${control}-Key-N> <${control}-Key-n>]
    lappend bindlist "<<File|Open>>"              [list <${control}-Key-O> <${control}-Key-o>]
    lappend bindlist "<<File|Save>>"              [list <${control}-Key-S> <${control}-Key-s>]
    lappend bindlist "<<File|SaveAs>>"            [list <${control}-Shift-Key-S> <${control}-Shift-Key-s>]
    lappend bindlist "<<File|Print>>"             [list <${control}-Key-P> <${control}-Key-p>]
    lappend bindlist "<<File|ClearRecentFiles>>"  {}
    lappend bindlist "<<File|Close>>"             [list <${control}-Key-W> <${control}-Key-w>]
    lappend bindlist "<<File|CloseNow>>"          [list <${control}-Shift-Key-W> <${control}-Shift-Key-w>]
    if {[tk windowingsystem] eq "aqua"} {
        # TK 8.5+ Cocoa handles quit, minimize, & raise next window for us
        if {$::tcl_version < 8.5} {
            lappend bindlist <<File|Quit>>       [list <${control}-Key-Q> <${control}-Key-q>]
        }
    } else {
        lappend bindlist <<File|Quit>>          [list <${control}-Key-Q> <${control}-Key-q>]
    }
    lappend bindlist "<<File|QuitNow>>"           [list <${control}-Shift-Key-Q> <${control}-Shift-Key-q>]

    # (at least on X11) the built-in events Undo/Redo/Cut/Copy/Paste are already bound to the usual shortcuts
    # this is somewhat problematic, as we cannot unbind these events from the keys
    lappend bindlist "<<Edit|Undo>>"              [list <${control}-Key-Z> <${control}-Key-z>]
    lappend bindlist "<<Edit|Redo>>"              [list <${control}-Shift-Key-Z> <${control}-Shift-Key-z>]
    lappend bindlist "<<Edit|Cut>>"               [list <${control}-Key-X> <${control}-Key-x>]
    lappend bindlist "<<Edit|Copy>>"              [list <${control}-Key-C> <${control}-Key-c>]
    # TclTk says:
    # lappend bindlist "<<Paste>>"   Replace the currently selected widget contents with the contents of the clipboard.
    # lappend bindlist "<<PasteSelection>>" Insert the contents of the selection at the mouse location.
    lappend bindlist "<<Edit|Paste>>"             [list <${control}-Key-V> <${control}-Key-v>]
    lappend bindlist "<<Edit|PasteReplace>>"      {}
    lappend bindlist "<<Edit|Duplicate>>"         [list <${control}-Key-D> <${control}-Key-d>]
    lappend bindlist "<<Edit|SelectAll>>"         [list <${control}-Key-A> <${control}-Key-a>]
    lappend bindlist "<<Edit|SelectNone>>"        [list <KeyPress-Escape>]
    lappend bindlist "<<Edit|Font>>"              {}
    # take the '=' key as a zoom-in accelerator, because '=' is the non-shifted
    # "+" key... this only makes sense on US keyboards but some users
    # expected it... go figure.
    lappend bindlist "<<Edit|ZoomIn>>"            [list <${control}-Key-plus> <${control}-Key-KP_Add> <${control}-Key-equal>]
    lappend bindlist "<<Edit|ZoomOut>>"           [list <${control}-Key-minus> <${control}-Key-KP_Subtract>]
    lappend bindlist "<<Edit|TidyUp>>"            [list <${control}-Shift-Key-R> <${control}-Shift-Key-r>]
    lappend bindlist "<<Edit|ConnectSelection>>"  [list <${control}-Key-K> <${control}-Key-k>]
    lappend bindlist "<<Edit|Triggerize>>"        [list <${control}-Key-T> <${control}-Key-t>]
    lappend bindlist "<<Edit|EditMode>>"          [list <${control}-Key-E> <${control}-Key-e>]

    lappend bindlist "<<Put|Object>>"             [list <${control}-Key-1>]
    lappend bindlist "<<Put|Message>>"            [list <${control}-Key-2>]
    lappend bindlist "<<Put|Number>>"             [list <${control}-Key-3>]
    lappend bindlist "<<Put|List>>"               [list <${control}-Key-4>]
    lappend bindlist "<<Put|Symbol>>"             {}
    lappend bindlist "<<Put|Comment>>"            [list <${control}-Key-5>]
    lappend bindlist "<<Put|Bang>>"               [list <${control}-Shift-Key-B> <${control}-Shift-Key-b>]
    lappend bindlist "<<Put|Toggle>>"             [list <${control}-Shift-Key-T> <${control}-Shift-Key-t>]
    lappend bindlist "<<Put|Number2>>"            [list <${control}-Shift-Key-N> <${control}-Shift-Key-n>]
    lappend bindlist "<<Put|VerticalSlider>>"     [list <${control}-Shift-Key-V> <${control}-Shift-Key-v>]
    lappend bindlist "<<Put|HorizontalSlider>>"   [list <${control}-Shift-Key-J> <${control}-Shift-Key-j>]
    lappend bindlist "<<Put|VerticalRadio>>"      [list <${control}-Shift-Key-D> <${control}-Shift-Key-d>]
    lappend bindlist "<<Put|HorizontalRadio>>"    [list <${control}-Shift-Key-I> <${control}-Shift-Key-i>]
    lappend bindlist "<<Put|VUMeter>>"            [list <${control}-Shift-Key-U> <${control}-Shift-Key-u>]
    lappend bindlist "<<Put|Canvas>>"             [list <${control}-Shift-Key-C> <${control}-Shift-Key-c>]
    lappend bindlist "<<Put|Graph>>"              [list <${control}-Shift-Key-G> <${control}-Shift-Key-g>]
    lappend bindlist "<<Put|Array>>"              [list <${control}-Shift-Key-A> <${control}-Shift-Key-a>]

    lappend bindlist "<<Find|Find>>"              [list <${control}-Key-F> <${control}-Key-f>]
    lappend bindlist "<<Find|FindAgain>>"         [list <${control}-Key-G> <${control}-Key-g>]
    lappend bindlist "<<Find|FindLastError>>"     {}

    lappend bindlist "<<Media|DSPOn>>"            [list <${control}-Key-slash>]
    lappend bindlist "<<Media|DSPOff>>"           [list <${control}-Key-period>]
    lappend bindlist "<<Media|TestAudioMIDI>>"    {}
    lappend bindlist "<<Media|LoadMeter>>"        {}

    lappend bindlist "<<Window|Maximize>>"        {}
    lappend bindlist "<<Window|AllToFront>>"      {}
    lappend bindlist "<<Window|PdWindow>>"        [list <${control}-Key-R> <${control}-Key-r>]
    lappend bindlist "<<Window|Parent>>"          {}

    if {[tk windowingsystem] eq "aqua"} {
        # TK 8.5+ Cocoa handles quit, minimize, & raise next window for us
        if {$::tcl_version < 8.5} {
            lappend bindlist <<Window|Minimize>> [list <${control}-Key-M> <${control}-Key-m>]
            lappend bindlist <<Window|Next>>     [list <${control}-quoteleft>]
        }
    } else {
        lappend bindlist <<Window|Minimize>>    [list <${control}-Key-M> <${control}-Key-m>]
        lappend bindlist <<Window|Next>>        [list <${control}-Next> <${control}-greater>]
        lappend bindlist <<Window|Previous>>    [list <${control}-Prior> <${control}-less>]
    }

    lappend bindlist "<<Help|About>>"             {}
    lappend bindlist "<<Help|Manual>>"            {}
    lappend bindlist "<<Help|Browser>>"           [list <${control}-Key-B> <${control}-Key-b>]
    lappend bindlist "<<Help|ListObjects>>"       {}
    lappend bindlist "<<Help|puredata.info>>"     {}
    lappend bindlist "<<Help|CheckUpdates>>"      {}
    lappend bindlist "<<Help|ReportBug>>"         {}

    lappend bindlist "<<Pd|Message>>"             [list <${control}-Shift-Key-M> <${control}-Shift-Key-m>]
    lappend bindlist "<<Pd|ClearConsole>>"        [list <${control}-Shift-Key-L> <${control}-Shift-Key-l>]

    lappend bindlist "<<Preferences|Edit>>"       {}
    lappend bindlist "<<Preferences|Audio>>"      {}
    lappend bindlist "<<Preferences|MIDI>>"       {}
    lappend bindlist "<<Preferences|Save>>"       {}
    lappend bindlist "<<Preferences|SaveTo>>"     {}
    lappend bindlist "<<Preferences|Load>>"       {}
    lappend bindlist "<<Preferences|Forget>>"     {}

}

proc ::pd_bindings::setup {} {
    set data [::pd_guiprefs::read KeyBindings true]
    if { ${data} != {} } {
        set ::pd_bindings::bindlist $data
    }
    if {[catch {
        foreach {event shortcuts} $::pd_bindings::bindlist {
            event delete $event
            foreach keys $shortcuts {
                if { "$keys" != "" } {
                    if {[catch {event add $event $keys} stderr1]} {
                        ::pdwindow::error "Failed to bind $event to $keys: $stderr1\n"
                    }
                }
            }
        }
    } stderr]} {
        ::pdwindow::error "Failed to add bind: $stderr\n"
    }
}

# wrapper around bind(3tk)to deal with CapsLock
# the actual bind-sequence is is build as '<${seq_prefix}-${seq_nocase}',
# with the $seq_nocase part bing bound both as upper-case and lower-case
proc ::pd_bindings::bind_capslock {tag seq_prefix seq_nocase script} {
    bind $tag <${seq_prefix}-[string tolower ${seq_nocase}]> "::pd_menucommands::scheduleAction $script"
    bind $tag <${seq_prefix}-[string toupper ${seq_nocase}]> "::pd_menucommands::scheduleAction $script"
}

# TODO rename pd_bindings to window_bindings after merge is done

# Some commands are bound using "" quotations so that the $mytoplevel is
# interpreted immediately.  Since the command is being bound to $mytoplevel,
# it makes sense to have value of $mytoplevel already in the command.  This is
# the opposite of most menu/bind commands here and in pd_menus.tcl, which use
# {} to force execution of any variables (i.e. $::focused_window) until later

# binding by class is not recursive, so its useful for window events
proc ::pd_bindings::class_bindings {} {
    # and the Pd window is in a class to itself
    bind PdWindow     <FocusIn>       "::pd_bindings::window_focusin %W"
    bind PdWindow     <Destroy>       "::pd_bindings::window_destroy %W"
    # bind to all the windows dedicated to patch canvases
    bind PatchWindow  <FocusIn>       "::pd_bindings::window_focusin %W"
    bind PatchWindow  <Map>           "::pd_bindings::patch_map %W"
    bind PatchWindow  <Unmap>         "::pd_bindings::patch_unmap %W"
    bind PatchWindow  <Configure>     "::pd_bindings::patch_configure %W %w %h %x %y"
    bind PatchWindow  <Destroy>       "::pd_bindings::window_destroy %W"
    # dialog panel windows bindings, which behave differently than PatchWindows
    bind DialogWindow <Configure>     "::pd_bindings::dialog_configure %W"
    bind DialogWindow <FocusIn>       "::pd_bindings::dialog_focusin %W"
    bind DialogWindow <Destroy>       "::pd_bindings::window_destroy %W"
    # help browser bindings
    bind HelpBrowser  <Configure>     "::pd_bindings::dialog_configure %W"
    bind HelpBrowser  <FocusIn>       "::pd_bindings::dialog_focusin %W"
    bind HelpBrowser  <Destroy>       "::pd_bindings::window_destroy %W"
}

proc ::pd_bindings::global_bindings {} {
    # we use 'bind all' everywhere to get as much of Tk's automatic binding
    # behaviors as possible, things like not sending an event for 'O' when
    # 'Control-O' is pressed

    bind  all  <<File|New>>               {::pd_menucommands::scheduleAction menu_new}
    bind  all  <<File|Open>>              {::pd_menucommands::scheduleAction menu_open}
    bind  all  <<File|Save>>              {::pd_menucommands::scheduleAction menu_send %W menusave}
    bind  all  <<File|SaveAs>>            {::pd_menucommands::scheduleAction menu_send %W menusaveas}
    bind  all  <<Pd|Message>>             {::pd_menucommands::scheduleAction menu_message_dialog}
    bind  all  <<File|Print>>             {::pd_menucommands::scheduleAction menu_print %W}
    bind  all  <<File|ClearRecentFiles>>  {::pd_menucommands::scheduleAction ::pd_menus::clear_recentfiles_menu}
    bind  all  <<File|Close>>             {::pd_menucommands::scheduleAction ::pd_bindings::window_close %W}
    bind  all  <<File|CloseNow>>          {::pd_menucommands::scheduleAction ::pd_bindings::window_close %W 1}
    bind  all  <<File|Quit>>              {::pd_menucommands::scheduleAction ::pd_connect::menu_quit}
    bind  all  <<File|QuitNow>>           {::pd_menucommands::scheduleAction pdsend "pd quit"}

    bind  all  <<Preferences|Edit>>       {::pd_menucommands::scheduleAction menu_preference_dialog}
    bind  all  <<Preferences|Save>>       {::pd_menucommands::scheduleAction pdsend "pd save-preferences"}
    bind  all  <<Preferences|SaveTo>>     {::pd_menucommands::scheduleAction ::pd_menus::savepreferences}
    bind  all  <<Preferences|Load>>       {::pd_menucommands::scheduleAction ::pd_menus::loadpreferences}
    bind  all  <<Preferences|Forget>>     {::pd_menucommands::scheduleAction ::pd_menus::forgetpreferences}

    bind  all  <<Edit|Undo>>              {::pd_menucommands::scheduleAction menu_undo}
    bind  all  <<Edit|Redo>>              {::pd_menucommands::scheduleAction menu_redo}
    bind  all  <<Edit|Cut>>               {::pd_menucommands::scheduleAction menu_send %W cut}
    bind  all  <<Edit|Copy>>              {::pd_menucommands::scheduleAction menu_send %W copy}
    bind  all  <<Edit|Paste>>             {::pd_menucommands::scheduleAction menu_send %W paste}
    bind  all  <<Edit|Duplicate>>         {::pd_menucommands::scheduleAction menu_send %W duplicate}
    bind  all  <<Edit|PasteReplace>>      {::pd_menucommands::scheduleAction menu_send %W paste-replace}
    bind  all  <<Edit|SelectAll>>         {::pd_menucommands::scheduleAction menu_send %W selectall}

    bind  all  <<Edit|Font>>              {::pd_menucommands::scheduleAction menu_font_dialog}
    bind  all  <<Edit|ZoomIn>>            {::pd_menucommands::scheduleAction menu_send_float %W zoom 2}
    bind  all  <<Edit|ZoomOut>>           {::pd_menucommands::scheduleAction menu_send_float %W zoom 1}
    bind  all  <<Edit|TidyUp>>            {::pd_menucommands::scheduleAction menu_send %W tidy}
    bind  all  <<Edit|ConnectSelection>>  {::pd_menucommands::scheduleAction menu_send %W connect_selection}
    bind  all  <<Edit|Triggerize>>        {::pd_menucommands::scheduleAction menu_send %W triggerize}
    bind  all  <<Pd|ClearConsole>>        {::pd_menucommands::scheduleAction menu_clear_console}
    bind  all  <<Edit|EditMode>>          {::pd_menucommands::scheduleAction menu_toggle_editmode}
    bind  all  <<Edit|SelectNone>>        {::pd_menucommands::scheduleAction menu_send %W deselectall; ::pd_bindings::sendkey %W 1 %K %A 1 %k}

    bind  all  <<Put|Object>>             {::pd_menucommands::scheduleAction menu_send_float %W obj 0}
    bind  all  <<Put|Message>>            {::pd_menucommands::scheduleAction menu_send_float %W msg 0}
    bind  all  <<Put|Number>>             {::pd_menucommands::scheduleAction menu_send_float %W floatatom 0}
    bind  all  <<Put|Symbol>>             {::pd_menucommands::scheduleAction menu_send_float %W symbolatom 0}
    bind  all  <<Put|List>>               {::pd_menucommands::scheduleAction menu_send_float %W listbox 0}
    bind  all  <<Put|Comment>>            {::pd_menucommands::scheduleAction menu_send_float %W text 0}
    bind  all  <<Put|Array>>              {::pd_menucommands::scheduleAction menu_send %W menuarray}
    bind  all  <<Put|Bang>>               {::pd_menucommands::scheduleAction menu_send %W bng}
    bind  all  <<Put|Canvas>>             {::pd_menucommands::scheduleAction menu_send %W mycnv}
    bind  all  <<Put|VerticalRadio>>      {::pd_menucommands::scheduleAction menu_send %W vradio}
    bind  all  <<Put|Graph>>              {::pd_menucommands::scheduleAction menu_send %W graph}
    bind  all  <<Put|HorizontalSlider>>   {::pd_menucommands::scheduleAction menu_send %W hslider}
    bind  all  <<Put|HorizontalRadio>>    {::pd_menucommands::scheduleAction menu_send %W hradio}
    bind  all  <<Put|Number2>>            {::pd_menucommands::scheduleAction menu_send %W numbox}
    bind  all  <<Put|Toggle>>             {::pd_menucommands::scheduleAction menu_send %W toggle}
    bind  all  <<Put|VUMeter>>            {::pd_menucommands::scheduleAction menu_send %W vumeter}
    bind  all  <<Put|VerticalSlider>>     {::pd_menucommands::scheduleAction menu_send %W vslider}

    bind  all  <<Find|Find>>              {::pd_menucommands::scheduleAction menu_find_dialog}
    bind  all  <<Find|FindAgain>>         {::pd_menucommands::scheduleAction menu_send %W findagain}
    bind  all  <<Find|FindLastError>>     {::pd_menucommands::scheduleAction pdsend {pd finderror}}

    bind  all  <<Media|DSPOn>>            {::pd_menucommands::scheduleAction pdsend "pd dsp 1"}
    bind  all  <<Media|DSPOff>>           {::pd_menucommands::scheduleAction pdsend "pd dsp 0"}
    bind  all  <<Media|TestAudioMIDI>>    {::pd_menucommands::scheduleAction menu_doc_open doc/7.stuff/tools testtone.pd}
    bind  all  <<Media|LoadMeter>>        {::pd_menucommands::scheduleAction menu_doc_open doc/7.stuff/tools load-meter.pd}
    bind  all  <<Preferences|Audio>>      {::pd_menucommands::scheduleAction pdsend "pd audio-properties"}
    bind  all  <<Preferences|MIDI>>       {::pd_menucommands::scheduleAction pdsend "pd midi-properties"}

    bind  all  <<Window|Minimize>>        {::pd_menucommands::scheduleAction menu_minimize %W}
    bind  all  <<Window|Maximize>>        {::pd_menucommands::scheduleAction menu_maximize %W}
    bind  all  <<Window|AllToFront>>      {::pd_menucommands::scheduleAction menu_bringalltofront}
    bind  all  <<Window|Next>>            {::pd_menucommands::scheduleAction menu_raisenextwindow}
    bind  all  <<Window|Previous>>        {::pd_menucommands::scheduleAction menu_raisepreviouswindow}
    bind  all  <<Window|PdWindow>>        {::pd_menucommands::scheduleAction menu_raise_pdwindow}
    bind  all  <<Window|Parent>>          {::pd_menucommands::scheduleAction menu_send %W findparent}

    bind  all  <<Help|About>>             {::pd_menucommands::scheduleAction menu_aboutpd}
    bind  all  <<Help|Manual>>            {::pd_menucommands::scheduleAction menu_manual}
    bind  all  <<Help|Browser>>           {::pd_menucommands::scheduleAction menu_helpbrowser}
    bind  all  <<Help|ListObjects>>       {::pd_menucommands::scheduleAction menu_objectlist}
    bind  all  <<Help|puredata.info>>     {::pd_menucommands::scheduleAction menu_openfile {https://puredata.info}}
    bind  all  <<Help|CheckUpdates>>      {::pd_menucommands::scheduleAction menu_openfile {https://pdlatest.puredata.info}}
    bind  all  <<Help|ReportBug>>         {::pd_menucommands::scheduleAction menu_openfile {https://bugs.puredata.info}}


    # JMZ: shouldn't the keybindings only apply to PatchWindows?

    # OS-specific bindings
    if {$::windowingsystem eq "aqua"} {
        # BackSpace/Delete report the wrong isos (unicode representations) on OSX,
        # so we set them to the empty string and let ::pd_bindings::sendkey guess the correct values
        bind all <KeyPress-BackSpace>      {::pd_bindings::sendkey %W 1 %K "" 1 %k}
        bind all <KeyRelease-BackSpace>    {::pd_bindings::sendkey %W 0 %K "" 1 %k}
        bind all <KeyPress-Delete>         {::pd_bindings::sendkey %W 1 %K "" 1 %k}
        bind all <KeyRelease-Delete>       {::pd_bindings::sendkey %W 0 %K "" 1 %k}
        bind all <KeyPress-KP_Enter>       {::pd_bindings::sendkey %W 1 %K "" 1 %k}
        bind all <KeyRelease-KP_Enter>     {::pd_bindings::sendkey %W 0 %K "" 1 %k}
        bind all <KeyPress-Clear>          {::pd_bindings::sendkey %W 1 %K "" 1 %k}
        bind all <KeyRelease-Clear>        {::pd_bindings::sendkey %W 0 %K "" 1 %k}
    }

    bind all <KeyPress>         {::pd_bindings::sendkey %W 1 %K %A 0 %k}
    bind all <KeyRelease>       {::pd_bindings::sendkey %W 0 %K %A 0 %k}
    bind all <Shift-KeyPress>   {::pd_bindings::sendkey %W 1 %K %A 1 %k}
    bind all <Shift-KeyRelease> {::pd_bindings::sendkey %W 0 %K %A 1 %k}
}

# bindings for .pdwindow are found in ::pdwindow::pdwindow_bindings in pdwindow.tcl

# this is for the dialogs: find, font, sendmessage, gatom properties, array
# properties, iemgui properties, canvas properties, data structures
# properties, Audio setup, and MIDI setup
proc ::pd_bindings::dialog_bindings {mytoplevel dialogname} {
    variable control

    bind $mytoplevel <KeyPress-Escape>          "dialog_${dialogname}::cancel $mytoplevel; break"
    bind_capslock $mytoplevel ${control}-Key w "dialog_${dialogname}::cancel $mytoplevel; break"
    bind $mytoplevel <KeyPress-Return>          "dialog_${dialogname}::ok $mytoplevel; break"
    # these aren't supported in the dialog, so alert the user, then break so
    # that no other key bindings are run
    if {$mytoplevel ne ".find"} {
        bind_capslock $mytoplevel ${control}-Key s       {bell; break}
        bind_capslock $mytoplevel ${control}-Shift-Key s {bell; break}
        bind_capslock $mytoplevel ${control}-Key p       {bell; break}
    } else {
        # find may may allow passthrough to it's target search patch
        ::dialog_find::update_bindings
    }
    bind_capslock $mytoplevel ${control}-Key t           {bell; break}

    wm protocol $mytoplevel WM_DELETE_WINDOW "dialog_${dialogname}::cancel $mytoplevel"
}

# this is for canvas windows
proc ::pd_bindings::patch_bindings {mytoplevel} {
    variable control
    variable alt
    set tkcanvas [tkcanvas_name $mytoplevel]

    # TODO move mouse bindings to global and bind to 'all'

    # mouse bindings -----------------------------------------------------------
    # these need to be bound to $tkcanvas because %W will return $mytoplevel for
    # events over the window frame and $tkcanvas for events over the canvas
    bind $tkcanvas <Motion>                    "pdtk_canvas_motion %W %x %y 0"
    bind $tkcanvas <Shift-Motion>              "pdtk_canvas_motion %W %x %y 1"
    bind $tkcanvas <${control}-Motion>        "pdtk_canvas_motion %W %x %y 2"
    bind $tkcanvas <${control}-Shift-Motion>  "pdtk_canvas_motion %W %x %y 3"
    bind $tkcanvas <${alt}-Motion>               "pdtk_canvas_motion %W %x %y 4"
    bind $tkcanvas <${alt}-Shift-Motion>         "pdtk_canvas_motion %W %x %y 5"
    bind $tkcanvas <${control}-${alt}-Motion>   "pdtk_canvas_motion %W %x %y 6"
    bind $tkcanvas <${control}-${alt}-Shift-Motion> "pdtk_canvas_motion %W %x %y 7"

    bind $tkcanvas <ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 0"
    bind $tkcanvas <Shift-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 1"
    bind $tkcanvas <${control}-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 2"
    bind $tkcanvas <${control}-Shift-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 3"
    bind $tkcanvas <${alt}-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 4"
    bind $tkcanvas <${alt}-Shift-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 5"
    bind $tkcanvas <${control}-${alt}-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 6"
    bind $tkcanvas <${control}-${alt}-Shift-ButtonPress-1> \
        "pdtk_canvas_mouse %W %x %y %b 7"

    bind $tkcanvas <ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 0"
    bind $tkcanvas <Shift-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 1"
    bind $tkcanvas <${control}-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 2"
    bind $tkcanvas <${control}-Shift-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 3"
    bind $tkcanvas <${alt}-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 4"
    bind $tkcanvas <${alt}-Shift-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 5"
    bind $tkcanvas <${control}-${alt}-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 6"
    bind $tkcanvas <${control}-${alt}-Shift-ButtonRelease-1> \
        "pdtk_canvas_mouseup %W %x %y %b 7"

    if {$::windowingsystem eq "x11"} {
        # from http://wiki.tcl.tk/3893
        bind all <Button-4> \
            {event generate [focus -displayof %W] <MouseWheel> -delta  1}
        bind all <Button-5> \
            {event generate [focus -displayof %W] <MouseWheel> -delta -1}
        bind all <Shift-Button-4> \
            {event generate [focus -displayof %W] <Shift-MouseWheel> -delta  1}
        bind all <Shift-Button-5> \
            {event generate [focus -displayof %W] <Shift-MouseWheel> -delta -1}
    }
    bind $tkcanvas <MouseWheel>       {::pdtk_canvas::scroll %W y %D}
    bind $tkcanvas <Shift-MouseWheel> {::pdtk_canvas::scroll %W x %D}

    # clear interim compose character by sending a virtual BackSpace,
    # these events are pulled from Tk library/entry.tcl
    # should we try to keep track of the "marked text" selection which may be
    # more than a single character? ie. start & length of marked text
    catch {
        # bind $tkcanvas <<TkStartIMEMarkedText>> {
        #     ::pdwindow::post "%W start marked text\n"
        # }
        # bind $tkcanvas <<TkEndIMEMarkedText>> {
        #     ::pdwindow::post "%W end marked text\n"
        # }
        bind $tkcanvas <<TkClearIMEMarkedText>> {
            # ::pdwindow::post "%W clear marked text\n"
            ::pd_bindings::sendkey %W 1 BackSpace "" 0
            ::pd_bindings::sendkey %W 0 BackSpace "" 0
        }
        bind $tkcanvas <<TkAccentBackspace>> {
            # ::pdwindow::post "%W accent backspace\n"
            ::pd_bindings::sendkey %W 1 BackSpace "" 0
            ::pd_bindings::sendkey %W 0 BackSpace "" 0
        }
    } stderr

    # "right clicks" are defined differently on each platform
    switch -- $::windowingsystem {
        "aqua" {
            bind $tkcanvas <ButtonPress-2>    "pdtk_canvas_rightclick %W %x %y %b"
            # on Mac OS X, make a rightclick with Ctrl-click for 1 button mice
            bind $tkcanvas <Control-Button-1> "pdtk_canvas_rightclick %W %x %y %b"
        } "x11" {
            bind $tkcanvas <ButtonPress-3>    "pdtk_canvas_rightclick %W %x %y %b"
            # on X11, button 2 "pastes" from the X windows clipboard
            bind $tkcanvas <ButtonPress-2>    "pdtk_canvas_clickpaste %W %x %y %b"
        } "win32" {
            bind $tkcanvas <ButtonPress-3>    "pdtk_canvas_rightclick %W %x %y %b"
        }
    }

    # <Tab> key to cycle through selection
    bind $tkcanvas <KeyPress-Tab>        "::pd_bindings::canvas_cycle %W  1 %K %A 0 %k"
    bind $tkcanvas <Shift-Tab>           "::pd_bindings::canvas_cycle %W -1 %K %A 1 %k"
    # on X11, <Shift-Tab> is a different key by the name 'ISO_Left_Tab'...
    # other systems (at least aqua) do not like this name, so we 'catch' any errors
    catch {bind $tkcanvas <KeyPress-ISO_Left_Tab> "::pd_bindings::canvas_cycle %W -1 %K %A 1 %k" } stderr

    # window protocol bindings
    wm protocol $mytoplevel WM_DELETE_WINDOW "pdsend \"$mytoplevel menuclose 0\""
    bind $tkcanvas <Destroy> "::pd_bindings::patch_destroy %W"
}


#------------------------------------------------------------------------------#
# event handlers

# do tasks when changing focus (Window menu, scrollbars, etc.)
proc ::pd_bindings::window_focusin {mytoplevel} {
    # focused_window is used throughout for sending bindings, menu commands,
    # etc. to the correct patch receiver symbol.  MSP took out a line that
    # confusingly redirected the "find" window which might be in mid search
    set ::focused_window $mytoplevel
    ::pd_menucommands::set_filenewdir $mytoplevel
    ::dialog_font::update_font_dialog $mytoplevel
    if {$mytoplevel eq ".pdwindow"} {
        ::pd_menus::configure_for_pdwindow
    } else {
        ::pd_menus::configure_for_canvas $mytoplevel
    }
    if {[winfo exists .font]} {wm transient .font $mytoplevel}
    # if we regain focus from another app, make sure to editmode cursor is right
    if {$::editmode($mytoplevel)} {
        $mytoplevel configure -cursor hand2
    }
}

# global window close event, patch windows are closed by pd
# while other window types are closed via their own bindings
# force argument:
#   0: request to close, verifying whether clean or dirty
#   1: request to close, no verification
proc ::pd_bindings::window_close {mytoplevel {force 0}} {
    # catch any non-existent windows
    # ie. the helpbrowser after it's been
    # closed by it's own binding
    if {$force eq 0 && ![winfo exists $mytoplevel]} {
        return
    }
    menu_send_float $mytoplevel menuclose $force
}

# "map" event tells us when the canvas becomes visible, and "unmap",
# invisible.  Invisibility means the Window Manager has minimized us.  We
# don't get a final "unmap" event when we destroy the window.
proc ::pd_bindings::patch_map {mytoplevel} {
    pdsend "$mytoplevel map 1"
    ::pdtk_canvas::finished_loading_file $mytoplevel
}

proc ::pd_bindings::patch_unmap {mytoplevel} {
    pdsend "$mytoplevel map 0"
}

proc ::pd_bindings::patch_configure {mytoplevel width height x y} {
    if {$width == 1 || $height == 1} {
        # make sure the window is fully created
        update idletasks
    }
    # the geometry we receive from the callback is really for the frame
    # however, we need position including the border decoration
    # as this is how we restore the position
    scan [wm geometry $mytoplevel] {%dx%d%[+]%d%[+]%d} width height - x - y
    pdtk_canvas_getscroll [tkcanvas_name $mytoplevel]
    # send the size/location of the window and canvas to 'pd' in the form of:
    #    left top right bottom
    pdsend "$mytoplevel setbounds $x $y [expr $x + $width] [expr $y + $height]"
}

proc ::pd_bindings::patch_destroy {window} {
    set mytoplevel [winfo toplevel $window]
    unset ::editmode($mytoplevel)
    unset ::undo_actions($mytoplevel)
    unset ::redo_actions($mytoplevel)
    unset ::editingtext($mytoplevel)
    unset ::loaded($mytoplevel)
    # unset my entries all of the window data tracking arrays
    array unset ::windowname $mytoplevel
    array unset ::parentwindows $mytoplevel
    array unset ::childwindows $mytoplevel
}

proc ::pd_bindings::dialog_configure {mytoplevel} {
}

proc ::pd_bindings::dialog_focusin {mytoplevel} {
    set ::focused_window $mytoplevel
    ::pd_menus::configure_for_dialog $mytoplevel
    if {$mytoplevel eq ".find"} {::dialog_find::focus_find}
}

proc ::pd_bindings::window_destroy {winid} {
    # make sure that ::focused_window does not refer to a non-existing window
    if {$winid eq $::focused_window} {
        set ::focused_window [wm transient $winid]
    }
    if {![winfo exists $::focused_window]} {
        set ::focused_window [focus]
        if {$::focused_window ne ""} {
            set ::focused_window [winfo toplevel $::focused_window]
        }
    }
}


# (Shift-)Tab for cycling through selection
proc ::pd_bindings::canvas_cycle {mytoplevel cycledir key iso shift {keycode ""}} {
    menu_send_float $mytoplevel cycleselect $cycledir
    ::pd_bindings::sendkey $mytoplevel 1 $key $iso $shift $keycode
}

#------------------------------------------------------------------------------#
# key usage

# canvas_key() expects to receive the patch's mytoplevel because key messages
# are local to each patch.  Therefore, key messages are not send for the
# dialog panels, the Pd window, help browser, etc. so we need to filter those
# events out.
proc ::pd_bindings::sendkey {window state key iso shift {keycode ""} } {
    #::pdwindow::error "::pd_bindings::sendkey .${state}. .${key}. .${iso}. .${shift}. .${keycode}.\n"

    # state: 1=keypress, 0=keyrelease
    # key (%K): the keysym corresponding to the event, substituted as a textual string
    # iso (%A): substitutes the UNICODE character corresponding to the event, or the empty string if the event does not correspond to a UNICODE character (e.g. the shift key was pressed)
    # shift: 1=shift, 0=no-shift
    # keycode (%k): keyboard code

    if { "$keycode" eq "" } {
        # old fashioned code fails to add %k-parameter; substitute...
        set keycode $key
    }
    # iso:
    # - 1-character: use it!
    # - multi-character: can happen with dead-keys (where an accent turns out to not be an accent after all...; e.g. "^" + "1" -> "^1")
    #              : turn it into multiple key-events (one per character)!
    # - 0-character: we need to calculate iso
    #              : if there's one stored in key2iso, use it (in the case of KeyRelease)
    #              : do some substitution based on $key
    #              : else use $key

    if { [string length $iso] == 0 && $state == 0 } {
        catch {set iso [dict get $::pd_bindings::key2iso $keycode] }
    }

    switch -- [string length $iso] {
        0 {
            switch -- $key {
                "BackSpace" { set key   8 }
                "Tab"       { set key   9 }
                "Return"    { set key  10 }
                "Escape"    { set key  27 }
                "Space"     { set key  32 }
                "space"     { set key  32 }
                "Delete"    { set key 127 }
                "KP_Delete" { set key 127 }
                "KP_Enter"  { set key  10 }
                default     {             }
            }
        }
        1 {
            scan $iso %c key
            if { "$key" eq "13" } { set key 10 }
            catch {
                if { "" eq "${::pd_bindings::key2iso}" } {
                    set ::pd_bindings::key2iso [dict create]
                }
                # store the key2iso mapping
                dict set ::pd_bindings::key2iso $keycode $iso
            }
        }
        default {
            # split a multi-char $iso in single chars
            foreach k [split $iso {}] {
                ::pd_bindings::sendkey $window $state $key $k $shift $keycode
            }
            return
        }
    }

    # some pop-up panels also bind to keys like the enter, but then disappear,
    # so ignore their events.  The inputbox in the Startup dialog does this.
    if {! [winfo exists $window]} {return}

    # $window might be a toplevel or canvas, [winfo toplevel] does the right thing
    set mytoplevel [winfo toplevel $window]
    if {[winfo class $mytoplevel] ne "PatchWindow"} {
        set mytoplevel pd
    }
    pdsend "$mytoplevel key $state $key $shift"
}
