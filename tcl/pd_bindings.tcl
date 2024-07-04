package provide pd_bindings 0.1

package require pd_menucommands
package require dialog_find
package require pd_connect

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
        set control "Mod1"
    }
}

proc ::pd_bindings::event_set {vevent args} {
    event delete $vevent
    foreach binding $args {
        if { "${binding}" != "" } {
            event add $vevent $binding
        }
    }
}

proc ::pd_bindings::setup {} {
    variable control
    # defines all the virtual events for menu interaction

    # note: we avoid CMD-H & CMD+Shift-H as it hides Pd on macOS

    #event_set  <<VirtualEvent>>           ${*}bindings
    event_set  <<File|New>>               <${control}-Key-n> <${control}-Key-N>
    event_set  <<File|Open>>              <${control}-Key-o> <${control}-Key-O>
    event_set  <<File|Save>>              <${control}-Key-s> <${control}-Key-S>
    event_set  <<File|SaveAs>>            <${control}-Shift-Key-s> <${control}-Shift-Key-S>
    event_set  <<File|Print>>             <${control}-Key-p> <${control}-Key-P>
    event_set  <<File|ClearRecentFiles>>  {}
    event_set  <<File|Close>>             <${control}-Key-w> <${control}-Key-W>
    event_set  <<File|CloseNow>>          <${control}-Shift-Key-w> <${control}-Shift-Key-W>
    event_set  <<File|QuitNow>>           <${control}-Shift-Key-q> <${control}-Shift-Key-Q>

    # (at least on X11) the built-in events Undo/Redo/Cut/Copy/Paste are already bound to the usual shortcuts
    # this is somewhat problematic, as we cannot unbind these events from the keys
    event_set  <<Edit|Undo>>              <${control}-Key-z> <${control}-Key-Z>
    event_set  <<Edit|Redo>>              <${control}-Shift-Key-z> <${control}-Shift-Key-Z>
    event_set  <<Edit|Cut>>               <${control}-Key-x> <${control}-Key-X>
    event_set  <<Edit|Copy>>              <${control}-Key-c> <${control}-Key-C>
    # TclTk says:
    # <<Paste>>   Replace the currently selected widget contents with the contents of the clipboard.
    # <<PasteSelection>> Insert the contents of the selection at the mouse location.
    event_set  <<Edit|Paste>>             <${control}-Key-v> <${control}-Key-V>

    event_set  <<Edit|PasteReplace>>      {}
    event_set  <<Edit|Duplicate>>         <${control}-Key-d> <${control}-Key-D>
    event_set  <<Edit|SelectAll>>         <${control}-Key-a> <${control}-Key-A>
    event_set  <<Edit|SelectNone>>        <KeyPress-Escape>
    event_set  <<Edit|Font>>              {}
    # take the '=' key as a zoom-in accelerator, because '=' is the non-shifted
    # "+" key... this only makes sense on US keyboards but some users
    # expected it... go figure.
    event_set  <<Edit|ZoomIn>>            <${control}-Key-equal> <${control}-Key-plus> <${control}-Key-KP_Add>
    event_set  <<Edit|ZoomOut>>           <${control}-Key-minus> <${control}-Key-KP_Subtract>
    event_set  <<Edit|TidyUp>>            <${control}-Shift-Key-r> <${control}-Shift-Key-R>
    event_set  <<Edit|ConnectSelection>>  <${control}-Key-k> <${control}-Key-K>
    event_set  <<Edit|Triggerize>>        <${control}-Key-t> <${control}-Key-T>
    event_set  <<Edit|EditMode>>          <${control}-Key-e> <${control}-Key-E>

    event_set  <<Put|Object>>             <${control}-Key-1>
    event_set  <<Put|Message>>            <${control}-Key-2>
    event_set  <<Put|Number>>             <${control}-Key-3>
    event_set  <<Put|List>>               <${control}-Key-4>
    event_set  <<Put|Symbol>>             {}
    event_set  <<Put|Comment>>            <${control}-Key-5>

    event_set  <<Put|Bang>>               <${control}-Shift-Key-b> <${control}-Shift-Key-B>
    event_set  <<Put|Toggle>>             <${control}-Shift-Key-t> <${control}-Shift-Key-T>
    event_set  <<Put|Number2>>            <${control}-Shift-Key-n> <${control}-Shift-Key-N>
    event_set  <<Put|VerticalSlider>>     <${control}-Shift-Key-v> <${control}-Shift-Key-V>
    event_set  <<Put|HorizontalSlider>>   <${control}-Shift-Key-j> <${control}-Shift-Key-J>
    event_set  <<Put|VerticalRadio>>      <${control}-Shift-Key-d> <${control}-Shift-Key-D>
    event_set  <<Put|HorizontalRadio>>    <${control}-Shift-Key-i> <${control}-Shift-Key-I>
    event_set  <<Put|VUMeter>>            <${control}-Shift-Key-u> <${control}-Shift-Key-U>
    event_set  <<Put|Canvas>>             <${control}-Shift-Key-c> <${control}-Shift-Key-C>
    event_set  <<Put|Graph>>              <${control}-Shift-Key-g> <${control}-Shift-Key-G>
    event_set  <<Put|Array>>              <${control}-Shift-Key-a> <${control}-Shift-Key-A>

    event_set  <<Find|Find>>              <${control}-Key-f> <${control}-Key-F>
    event_set  <<Find|FindAgain>>         <${control}-Key-g> <${control}-Key-G>
    event_set  <<Find|FindLastError>>     {}

    event_set  <<Media|DSPOn>>            <${control}-Key-slash>
    event_set  <<Media|DSPOff>>           <${control}-Key-period>
    event_set  <<Media|TestAudioMIDI>>    {}
    event_set  <<Media|LoadMeter>>        {}

    event_set  <<Window|Maximize>>        {}
    event_set  <<Window|AllToFront>>      {}
    if {[tk windowingsystem] eq "aqua"} {
         # TK 8.5+ Cocoa handles quit, minimize, & raise next window for us
        if {$::tcl_version < 8.5} {
            event_set <<File|Quit>>       <${control}-Key-q> <${control}-Key-Q>
            event_set <<Window|Minimize>> <${control}-Key-m> <${control}-Key-M>
            event_set <<Window|Next>>     <${control}-quoteleft>
        }
    } else {
        event_set  <<File|Quit>>          <${control}-Key-q> <${control}-Key-Q>
        event_set  <<Window|Minimize>>    <${control}-Key-m> <${control}-Key-M>
        event_set  <<Window|Next>>        <${control}-Next> <${control}-greater>
        event_set  <<Window|Previous>>    <${control}-Prior> <${control}-less>
    }

    event_set  <<Window|PdWindow>>        <${control}-Key-r> <${control}-Key-R>
    event_set  <<Window|Parent>>          {}

    event_set  <<Help|About>>             {}
    event_set  <<Help|Manual>>            {}
    event_set  <<Help|Browser>>           <${control}-Key-b> <${control}-Key-B>
    event_set  <<Help|ListObjects>>       {}
    event_set  <<Help|puredata.info>>     {}
    event_set  <<Help|CheckUpdates>>      {}
    event_set  <<Help|ReportBug>>         {}

    event_set  <<Pd|Message>>             <${control}-Shift-Key-m> <${control}-Shift-Key-M>
    event_set  <<Pd|ClearConsole>>        <${control}-Shift-Key-l> <${control}-Shift-Key-L>

    event_set  <<Preferences|Edit>>       {}
    event_set  <<Preferences|Audio>>      {}
    event_set  <<Preferences|MIDI>>       {}
    event_set  <<Preferences|Save>>       {}
    event_set  <<Preferences|SaveTo>>     {}
    event_set  <<Preferences|Load>>       {}
    event_set  <<Preferences|Forget>>     {}
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
    bind  all  <<File|Print>>             {::pd_menucommands::scheduleAction menu_print $::focused_window}
    bind  all  <<File|Close>>             {::pd_menucommands::scheduleAction ::pd_bindings::window_close %W}
    bind  all  <<File|CloseNow>>          {::pd_menucommands::scheduleAction ::pd_bindings::window_close %W 1}
    bind  all  <<File|Quit>>              {::pd_menucommands::scheduleAction ::pd_connect::menu_quit}
    bind  all  <<File|QuitNow>>           {::pd_menucommands::scheduleAction pdsend "pd quit"}


    bind  all  <<Edit|Undo>>              {::pd_menucommands::scheduleAction menu_undo}
    bind  all  <<Edit|Redo>>              {::pd_menucommands::scheduleAction menu_redo}
    bind  all  <<Edit|Cut>>               {::pd_menucommands::scheduleAction menu_send %W cut}
    bind  all  <<Edit|Copy>>              {::pd_menucommands::scheduleAction menu_send %W copy}
    bind  all  <<Edit|Paste>>             {::pd_menucommands::scheduleAction menu_send %W paste}
    bind  all  <<Edit|Duplicate>>         {::pd_menucommands::scheduleAction menu_send %W duplicate}
    bind  all  <<Edit|PasteReplace>>      {::pd_menucommands::scheduleAction puts "TODO bind PasteReplace"}
    bind  all  <<Edit|SelectAll>>         {::pd_menucommands::scheduleAction menu_send %W selectall}

    bind  all  <<Edit|Font>>              {::pd_menucommands::scheduleAction puts "TODO bind Font"}
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
    bind  all  <<Find|FindLastError>>     {::pd_menucommands::scheduleAction puts "TODO bind FindLastError"}

    bind  all  <<Media|DSPOn>>            {::pd_menucommands::scheduleAction pdsend "pd dsp 1"}
    bind  all  <<Media|DSPOff>>           {::pd_menucommands::scheduleAction pdsend "pd dsp 0"}
    bind  all  <<Preferences|Audio>>      {::pd_menucommands::scheduleAction puts "TODO bind AudioSettings"}
    bind  all  <<Preferences|MIDI>>       {::pd_menucommands::scheduleAction puts "TODO bind MIDISettings"}
    bind  all  <<Media|TestAudioMIDI>>    {::pd_menucommands::scheduleAction puts "TODO bind TestAudioMIDI"}
    bind  all  <<Media|LoadMeter>>        {::pd_menucommands::scheduleAction puts "TODO bind LoadMeter"}

    bind  all  <<Window|Minimize>>        {::pd_menucommands::scheduleAction puts "TODO bind Window|Minimize"}
    bind  all  <<Window|Maximize>>        {::pd_menucommands::scheduleAction puts "TODO bind Window|Maximize"}
    bind  all  <<Window|AllToFront>>      {::pd_menucommands::scheduleAction puts "TODO bind Window|AllToFront"}
    bind  all  <<Window|Next>>            {::pd_menucommands::scheduleAction puts "TODO bind Window|Next"}
    bind  all  <<Window|Previous>>        {::pd_menucommands::scheduleAction puts "TODO bind Window|Previous"}
    bind  all  <<Window|PdWindow>>        {::pd_menucommands::scheduleAction menu_raise_pdwindow}
    bind  all  <<Window|Parent>>          {::pd_menucommands::scheduleAction puts "TODO bind Window|Parent"}

    bind  all  <<Help|About>>             {::pd_menucommands::scheduleAction puts "TODO bind Help|About"}
    bind  all  <<Help|Manual>>            {::pd_menucommands::scheduleAction puts "TODO bind Help|Manual"}
    bind  all  <<Help|Browser>>           {::pd_menucommands::scheduleAction menu_helpbrowser}
    bind  all  <<Help|ListObjects>>       {::pd_menucommands::scheduleAction puts "TODO bind Help|ListObjects"}
    bind  all  <<Help|puredata.info>>     {::pd_menucommands::scheduleAction puts "TODO bind Help|puredata.info"}
    bind  all  <<Help|CheckUpdates>>      {::pd_menucommands::scheduleAction puts "TODO bind Help|CheckUpdates"}
    bind  all  <<Help|ReportBug>>         {::pd_menucommands::scheduleAction puts "TODO bind Help|ReportBug"}


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



::pd_bindings::setup
