---
title: current status
---

## Pd Manual: current status

[back to table of contents](index.html)


This section tracks changes in Pd's current implementation.

### release notes

------------------ 0.52-2 ------------------------------

Many small bug fixes, and a few larger ones: under jack, Pd's delay
adjusted upward to the jack blocksize if needed (to prevent a hang).

Many documentation fixes and improvements by Alexandre Porres

Reverted behavior when selecting and dragging a single text box, so that
it now activates the text as traditional until 0.51. Sometimes one
behavior is wanted, sometines the other, so there should be an
easy-to-understand way for users to control this. At the moment you can
rectangle-select an object to displace it without selecting the text,
which is perhaps adequate, perhaps not.

added an indirect way to open Pd's gui on one machine and the real-time
program on another without using X windows (so that it can be easily
done on MacOS and Windows). Requires ssh expertise - see commit
c0e320f2142. (This was already included in pd-0.51-1 but not
documented).

------------------ 0.52-0 and 0.52-1 ------------------------------

The Macintosh compiled version is now compiled at IEM (as part of the
continuous-integration setup available at
git.iem.at/pd/pure-data/pipelines). This should make Pd appear to be
"safely" signed. Added compilation for arm64 target (but not sure if
the released binary will include that or not).

New "list" box, like number and symbol but allows lists in addition to
single atoms. You can scroll up and down on any of the numbers in a list
as if it were a number box. All boxes (number, symbol, list) can now set
different sizes and you can also double-click on them to activate a
"real" editor you can copy/paste from/to.

A new "trace" object allows you to get backward and forward traces of
message passing.

New "file" object by Iohannes with contributions from Jean-Michael
Celerier - general file operations.

Support for uncompressed CAF and AIFC soundfiles (by Dan Wilcox) - the
soundfile code reorganized to make it more modular.

Improvements to the audio implementation - the most noticeable
improvement is better control of latency under jack.

Pd's GUI checks that Pd actually exits when asked to and if not, kills
it to prevent rogue invisible pd instances from collecting behind the
scenes.

Bugfix in bob~ to output "correct" signal (old behavior still
available by setting pd compatibility < 52).

libpd included as part of the Pd distribution. A test libpd project is
included in ../pd/libpd.

Raised the limit on the size of incoming UDP packets in netreceive.

Some improvements to label handling in IEMguis.

Improved window positioning.

New method for garrays to set colors, line width, visibility, display
style and disable mouse editing (documented in
2.control.examples/16.more.arrays.pd). Also added more flags to the
"plot" object that plots arrays in data structures (by Christof
Ressi).

"set", "send", "delete" and "insert" methods added to "list
store" object, and new features added to "get" method (by Christof
Ressi).

The output~ abstraction, widely used in Pd's documentation is now part
of 'extra' - plus several documentation updates/improvements to the
help files and chapter 4 of the manual (by Alexandre Porres). Also,
other updates to the manual by Lucas Cordiviola.

As usual, numerous bug fixes.

------------------ 0.51-4 ------------------------------

Bug fixes: infinite loop on Windows if writesf~ encountered a write
error; another hack at TCL/TK window size confusion; and better libpd
and build support

------------------ 0.51-3 ------------------------------

New functions for expr: mtof, ftom, rmstodb, dbtorms, powtodb, dbtopow,
and two bug fixes.

The "use callbacks" switch in the audio dialog only appears if it's
already on (via the command line or app default settings). It was
crashing and hanging Pd in several situations. It might still be useful
for reducing audio latency by a block or so, but won't buy you much in
any case. I'll try to fix it later.

More white-space fixes (primarily affecting pd~ on Windows).

Much work dealing with upgrade issues in MACOS 10.15 (thanks Dan Wilcox)

------------------ 0.51-2 ------------------------------

Better support for white space in file path to pd, especially for pd~
object. Also fixed an annoyamce in which pd~ started up a command
prompt window on PCs. The pd~ source is also now updated to work
(again) in Max/MSP, on Mac or PC.

------------------ 0.51-1 ------------------------------

Bug fixes, mostly from the git updates branch

(long overdue) Buttons in the audio dialog to set some standard sample
rates. You can still run Pd at 3.14159 Hz if you want to but you'll
have to type that one in.

On Macintosh tcl/tk is updgraded to 8.6.10 (should work on MacOS from
10.6 to 11.0 aka "Bug Sur").

You can send "pd fast-forward " to make Pd run its scheduler at full
speed, without waiting for A/D/A or the real-time clock. Useful for
running batch-style jobs interactively. You can advance a variable
amount of time by specifying a large value and then later sending "pd
fast-forward 0" - but note that outside input is ignored while in fast
forward.

------------------ 0.51-0 ------------------------------

Many bug fixes and some enhancements to network objects. Many thanks to
Dan Wilcox, Christof Ressi, and Iohannes Zmoelnig who did almost all the
work.

(Also Dan) some fix I don't understand about code signing for MacOS
10.15, without which externals refused to load.

The "inlet~" object can now also optionally forward messages via an
extra outlet.

New "mode" argument to openpanel object to allow selecting directories
or multiple files

New "equal" message to pointer object to test if two pointers are
equal.

Pd should now function OK if recompiled with float size set to 64 bits.

------------------ 0.50-2 ------------------------------

fix backspace problem on MacOS

------------------ 0.50-1 ------------------------------

improved handling of tilde chartacters for Portuguese and Spanish
keyboard maps

handling spaces in path to HTML file open message to pdcontrol object

------------------ 0.50-0 ------------------------------

new pdcontrol object allows getting patch's owning directory and
calling arguments, and opening files in web browser under patch control
(useful in help files).

new nonlinear low-pass filter, slop~, for use in designing envelope
followers and dynamics processors.

attempted to fix limitations on incoming MIDI and network packets.
Things seem to be improved but the improvements aren't exhaustively
tested yet.

improved HTML style in documentation and more info on managing externs.

updates to Iohannes's smart patching features

sort method for text object. Also text searching can deal with
restricted range of lines (so that, for instance, you can iteratively
get all the matches, not just the first one).

internal changes to improve scheduler behavior and memory management.

"zoom" feature updates to handle data structs better.

------------------ 0.49-0 ------------------------------

"Undo" and "Redo" menu items now may be repeated to undo/redo more
than one successive edit. This feature was introduced by Ivica Ico
Bukvic and ported to Pd vanilla by Johannes Zmoelnig.

Various editing shortcuts, also from Johannes.

"Declare" -path & -lib flags now search user search paths.

Search path handling made easier and more consistent between user
preferences, "declare" objects and command line flags. See help file
for the "declare" object.

A "savestate" object allows abstractions to be designed so that they
can save state information as part of the calling patch.

"Pointer" object has a "delete" method.

"Text", "array", and "scalar" objects have the same methods (bang
and "send") for outputting pointers to themselves

An optional argument was added to the "pd open" message, to allow
opening a file only if it is not currently open; to get this behavior
send "pd open 1". It is still under discussion whether this would be
the appropriate response when the user clicks on a patch to open it in
the file browser.

4 million point default limit on resizing soundfile read to array
changed to 2^31-1 (about 2 billion) samples. If invoked without an
array name, soundfiler still outputs the file's sample rate, etc.,
although it still reads the samples only to throw them away; so if you
really just want the soundfile info, consider reading into an unused
one-sample table instead.

The "log" object has an optional argument to specify the laogarythm's
base, and a new inlet, to match "log~", and "pow" and "pow~" can
now raise negative numbers to integer powers, like the C math library
"pow()" function.

Changes to audio I/O handling to time out if device freezes (as when you
unplug a USB device in MACOSX).

Backslashes may now be typed into object/message boxes. They escape
following "$", "," and ";" characters.

------------------ 0.48-2 ------------------------------

More bug fixes. Audio on linux improved; various improvements to the
pd~ object, -no-gui windows patches able to use netsend/netreceive;
loadbang order fix for clone object; soundfiler no longer limiting size
of soundfiles to 4million points by default; updates to deken (external
extension search tool); seed message added to noise~; many code-level
enhancements.

------------------ 0.48-1 ------------------------------

Many bug fixes and a re-working of font handling that might make font
sizes resemble 0.47 more closely. most of the work was done by Dan
Wilcox and Iohannes Zmoelnig.

------------------ 0.48-0 ------------------------------

It's possible to save and recall "settings" to files, and/or to erase
system settings. On Pcs, the settings are now stored in the "user"
resource area (before, it was per-machine, and users who weren't
administrators apparently couldn't save their settings.)

On the first startup, Pd queries the user as to whether to create a
directory such as ~/Documents/Pd and set path to point there. Many
updates were also made to the "path" dialog and deken to better
integrate downloading stuff and path maintenance.

The expr family (expr, expr~, fexpr~) got an update from Shahrokh
Yadegari, and the help file was reorganized and updated by Alexandre
Porres. Many more math functions are supported, and the parser was
updated so that expressions using "if" skip evaluating the argument
that isn't used.

New "fudiparse", "fudiformat" objects.

New "list store" object for faster and easier concatenation and
splitting apart of lists.

New notification outlet in "text define" so the patch can know when
the text is changed.

New "text insert" object.

"delwrite~" now has a "clear" message.

"soundfiler" now has a right outlet which outputs a list with the
sample rate, header size, number of channels, byte depth, and
endianness.

"declare -path" inside abstractions was changed so that, if the
declaration isn't an absolute filename, it's relative to the
abstraction's directory, not to the owning patch's directory. This
might be considered an incompatible change; but since the situation with
paths declared inside abstractions has always been so confused, I think
nobody will have got away without editing patches that relied on it as
things changed in the past.

"float" now understands symbol messages that look like floats. The
help files for "float", "value" and "send" were updated to reflect
this.

But the biggest changes are under the hood. Pd now can manage more than
one running instance, so that it is possible to make plug-ins of various
sorts out of Pd and they can coexist with each other. This will allow
people to write much better VST plug-ins in Pd. I'm grateful to Carlos
Eduardo Batista for much help on this.

The Pd API allows starting and/or shutting down the GUI dynamically
while Pd is running.

Another internal change: the pd~ object now communicates with
sub-processes in binary by default. This increases the numerical
accuracy of signal passing (it should be exact now) and makes the
passing back and forth of audio signals much more efficient.

Thanks to much effort by Dan Wilcox and IOhannes zmölnig, the
compile-and-build system is much improved, especially for Macintosh
computers. One visible effect is that language support is finally
working in the compiled versions on msp.ucsd.edu.

------------------ 0.47-1 ------------------------------

Improvements to deken plug-in ("find externals" menu item). Now
unzipping works automatically in windows.

improved "clone" argument parsing and added option to suppress
numbering of clones as first argument

fixed a bug in initbang (didn't work in subpatches)

"improved" zoom-in menu binding again (now '+' or '=' zooms in).

restored a function needed by GUI externs such as 'knob'

------------------ 0.47-0 ------------------------------

The "deken" plug-in is integrated into the Pd help menu - you can
download and install Pd libraries using the "Find Externals" menu
command.

New clone object that opens multiple copies of an abstraction and routes
messages and signals to/from them, for making banks of voices or whatnot

"Zoom" feature for dealing with high-resolution displays. IEM GUIs are
only somewhat dodgily handled (font sizes of labels aren't managed
well).

Controllable font sizes for menus and dialogs: setting the "pd window"
font size also sets font sizes for menus/dialogs.

The expr, expr~, and fexpr~ objects are included in Pd proper, not
loaded as externs as before. This reflects their new license (lgpl) and
should allow them to be used in IOS apps via libpd. Shahrokh Yadegari
has updated the source and made manifold improvements in the objects.
Notably, they now allow access to variables in Pd defined via the
"value" object.

Backward messaging to netsend now works in UDP as well as TCP.

Dialogs now work more Apple-ishly (changes taking place without the need
to hit an "apply" button in many cases). Thanks to Dan Wilcox.

API support for "initbang" and "closebang" objects (from IEM library
I think, but anyhow you can now get them in Pd Vanilla via deken (help
menu "Find externals").

"Declare" object path settings now take effect immediately when you
edit the declare object.

(IOhannes) Abstractions, externs, and stuff written in other languages
(python, Lua, etc) are now loaded logically, that is, if you have one
patch that loads an external named X, you can still load abstractions
named X in other patches. Miller now officially Does Not Know How This
Works (DNKHTW).

Many bug fixes.

------------------ 0.46-7 ------------------------------

Fixed non-working declare -stdpath on windows

Fixed bug in which [array min/max] ignored onset argument

Fixed jack audio back-ed not to auto-start the daemon

Various compilation and architecture fixes

------------------ 0.46-6 ------------------------------

fixed bug selecting and saving/restoring MIDI output devices.

new bob~ object (Moog filter emulation) in extra. (This was intended
for 0.47 but it was easiest to leave it in place for this bug-fix
release).

------------------ 0.46-5 ------------------------------

fixed crasher bug (cutting all text in an object then cutting again to
delete the object sometimes crashed Pd)

added two checks to re-sort DSP when arrays are created or renamed

patched Makefile.am for correct list of objects (fixes broken make
install)

fixed configure.ac to use fftw3

------------------ 0.46-4 ------------------------------

fixed crasher bug in 0.364-3

------------------ 0.46-3 ------------------------------

changed priority settings to agree with new restrictions on linux --
apparently non-root processes can't set to any of the top 5 or so
priority levels.

three minor fixes for array graphics

new "set" messages and wildcard '-' template names for "set",
"get", "elem", "setsize", "getsize", and "append" objects

updated Makefile.am to reflect new and corrected help file names

took out bogus object in rev3~

------------------ 0.46-2 ------------------------------

fixed bug calculating widths of pre-version-0.39 GOPs

added a pd_getdspstate() function to replace dspstate external variable
which was moved to the pd instance structure. Strictly speaking this
doesn't restore binary compatibility but I believe only one extern
writer was using dspstate (and he's actively maintaining the externs)
so I think this is OK.

The old standard-error "realtime priority" debug message under linux
only appears if Pd is started 'verbose'. OTOH, if realtime
priority-setting fails, the user is notified in the Pd window.

fixed debian compile bug.

fixed "array" object to allow a size of 1.

------------------ 0.46-1 ------------------------------

Bug fix in oscparse (truncated some messages)

Fixed ALSA (on linux) to open devices with fewer channels than requested

Other small bug fixes.

------------------ 0.46-0 ------------------------------

The biggest change in 0.46 was to make possible loading multiple Pd
instances into a single address space. The instances share symbols
(there are no namespaces) but have different schedulers, DSP networks,
and MIDI I/O structures. In principle it should be possible to design Pd
plug-ins that don't interact with each other using libpd.

New oscparse and oscformat objects to facilitate OSC I/O. No support for
streaming; UDP only so far.

At long last, Pd vanilla compiled for Macintosh computers is distributed
with Jack support.

Bug fixes to IEM GUIs: sliders and radio buttons pass floating-point
numbers through without limiting them to the GUI's range or resolution.
Also, the toggle no longer resets its "value" (the nonzero number it
outputs) when receiving numbers in messages. Set compatibility to 0.45
or earlier to get the old behavior (using "pd compatibility" message
or a startup flag).

Audio and MIDI settings are now saved by device name, not number, so
that, hopefully, adding or reordering devices doesn't change the
devices Pd opens as saved in preferences. You can also specify them by
name on Pd's command line with new flags such as "-audioadddev".

New [text tosymbol] and [text fromsymbol] to allow string
manipulation.

Netsend/netreceive can send messages bidirectionally if using TCP
protocol.

Added some startup flags (mostly from IOhannes) - -nostderr, etc. Also
added a flag (-noautopatch) to suppress autoconnect.

improved the sound of rev2~ and rev3~ reverberators

Made stdlib and stdpath flags and declarations follow standard search
paths (IOhannes again).

Dozens of small bug fixes.

------------------ 0.45-3 ---------------------------

fixed a bug pasting text into Pd and an audio problem in Mac OSX
(non-default sample rates with built-in hardware needed huge buffering)

------------------ 0.45-1,2 ---------------------------

fixed bug in which backspace to dialog windows got sent to the last
edited patch

------------------ 0.45 ---------------------------

multi-purpose "array" and "text" objects. "Array" is a more
general replacement for the "table", "tabread" and "tabwrite"
objects. "text" is sort of like Max's "coll" but simpler and
hopefully more powerful. "text"s are also available as fields in data
structures.

"tempo" messages for delay, metro, timer, and "test sequence". In
particular you can specify or measure time in samples, but you can also
use this for changing the speed of an ensemble of delay loops while
keeping them in sync.

binary netsend/netreceive (so you should no longer need an extern for
OSC).

pd~ (multiprocessing) works on windows and is less likely to deadlock
although not yet perfect.

Objects/messages/comments have settable box widths. By default they're
auto, but you can change that by dragging on the right-hand-side border.
You can set it back to the default just by dragging or to whatever width
the default would have been. If a box's width isn't "auto" and you
open the patch back in Pd version 0.44 you'll see a warning; if you
open it in earlier versions you'll get pages of error messages but no
real harm. You can open and save a patch in 0.44 if you'd like to reset
all box widths to auto.

The Pd window now tells you whether PD has an audio device open or not,
and various error and status messages that used to go to standard error
now appear on the Pd window (this improvement was way overdue!)

Fixed hangups exiting when using jack.

Got ASIO working again on PC (it was apparently broken; I don't know
for how long.)

Various other small improvements in audio and midi handling: Notably,
except when using Jack, Pd doesn't open audio on startup; sometimes you
want to get to the dialog panel before audio ever starts (for instance,
on Mac, to prevent Pd from hanging when someone walks in the building
with an apple TV -- gee, thanks, Apple!)

Took a patch from IOhannes zmölnig to make Pd source conform to strict
aliasing rules. The vanilla release is still compiled with
-fno-strict-aliasing but this will be dropped after more testing.

The vanilla release now includes a 64 bit binary for MacOS. This is a
separate app and it won't load externs that haven't been compiled with
-arch x86_64. (Externs can be future-proofed by including both i386 and
i86_64 binaries by compiling with -arch i386 -arch x86_64).

The soundfiler now writes correct sample rate to AIFF files (was
previously hard-coded to 44100). Still no way to detect the sample rate
of a file when reading it.

------------------ 0.44-3 ---------------------------

Fix hip~ AC gain once more (still didn't have it right).

Small improvements in MIDI handling (midiin and sysexin device
numbering; two linux-specific fixes).

"open recent" fixed for linux

------------------ 0.44-2 ---------------------------

Fix underflow problem for Raspberry Pi (no effect on other platforms)

------------------ 0.44-1 ---------------------------

Fixed default API settings to prefer MMIO over ASIO for MS Windows and
anything over jack (so that first-time users are most likely to get
audio out).

------------------ 0.44 ---------------------------

many, many changes (hopefully improvements) in audio and MIDI I/O and in
scheduling.

got rid of the "old" (cd src; ./configure etc) build system. src now
contains fallback makefiles for gnu/linux and mac as well as windows.
But the "official" way to build Pd is now to invoke automake.

better handling of reading and writing files with non-ASCII filenames in
Microsoft Windows.

fixed a bug in hip~ (incorrect gain; noticeably different when the
cutoff frequency is set about 5K or higher.) To get the old behavior you
can set Pd to be 0.43 compatible; see the hip~ help window for details.

inlet~ and outlet~, if configured to do upsampling, now use
sample/hold instead of zero padding by default; arguably this is a bug
fix as the DC gain isn't one for zero padding. Also undoable by setting
compatibility to 0.43.

------------------ 0.43.2-3 ---------------------------

bug fixes, notably allowing an increase in incoming MIDI bandwidth.

added an "f" message to canvases which might in the future be used to
specify formatting info (width; font size) box by box.

------------------ 0.43.1 ---------------------------

bug fix: in "perf mode" (having sent pd the "perf" message to
prevent undesired patch closure) Pd crashed (oops!) when one asked for
the patch to close after all.

Improvements to Mac version of bonk~ -- "learn" parameter handled
correctly and compilation fixes.

Default font size is the same on all platforms.

Bug fixes reading and writing AIFF files, particularly 24 bit ones

"edit mode" menu checkbox should work now.

X Pasting text into boxes now works in linux.

fixed bug that hung Pd when closing more than one pd~ subpatch out of
order.

fixed segfault if DSP loop found and main patch had signal
inlets/outlets

miscellaneous cleanups.

STILL BUGGY: when you change the contents of a graph-on-parent subpatch
the old stuff often doesn't get erased correctly.

------------------ 0.43 ---------------------------

Completely new TCL front end, thanks to Hans-Christophe Steiner,
IOhannes zmölnig, and others.

------------------ 0.42-5 ---------------------------

broken abs~ and log~ fixed

pd~ -ninsig 0 hang fixed

testtone updated and 16ch version added

lrshift~ bug fix

32 channel limit removed for portaudio (ASIO/Windows and Mac)

------------------ 0.42-4 ---------------------------

added -noautopatch startup argument to defeat auto-connecting to new
objects (some folks like it and others hate it)

gfxstub bug fix

fixed crash on deleting "s" objects with no args

re-fixed seteuid(0 problem

fixed crash on "find $1" (still not useful though)

------------------ 0.42.1-3 ---------------------------

Bug fix on Windows(cancelling window close deactivated window).

Bug fix running Pd from command line in MacOS.

"Select all" fixed to select none if everything already selected.

------------------ 0.42 ---------------------------

The 'struct' object can now be used to catch a small but growing
variety of events (mouse clicks on data, selection/deselection).

The 'tabread4~' object was fixed to allow message-time onset into the
table. This makes it possible (at last) to use tabread4~ effectively
with large (> 1 second) samples. New help files 3.audio.examples/B15
and B16 show how to use it. It's not pretty but it's at least
possible.

Took patches from IOhannes zmölnig to make Pd work when compiled to use
64-bit floating-point audio. I didn't test this; doubtless there will
still be some problems. (This isn't the same thing as running in '64
bit mode' which already works fine and is the default when compiled on
a 64-bit linux machine.)

New objects in extra, 'pd~', and 'stdout', allow running a separate
Pd instance in a sub-process. Theoretically this should make it possible
to use multiprocessors efficiently in {d. It's still somewhat
experimental; it might not work well to pass large numbers of messages
and/or signals back and forth.

A -batch flag allows Pd to be run efficiently from scripts. You should
probably also specify -nosound. With these flags Pd runs as quickly as
it can without waiting for real time. It's up to the patch to arrange
to exit at the appropriate time via a 'pd quit' message.

Large and sweeping improvements have been made to bonk~ and sigmund~.
The new bonk~ features are documented in its help window; sigmund~
works better but has the same old features.

Closing and quitting Pd now act more politely, querying the user only if
there are edited windows. There's also a query if the user types into a
box holding an edited abstraction to prevent the changes from getting
lost accidentally. A new 'pd perf' message re-enables the old "really
close this window" behavior, which is appropriate for performances
where you don't want the wrong keyclick to stop Pd brusquely.

The process of reloading multiple copies of a modified abstraction was
sped up.

The 'find' dialog permits searching for substrings. This is useful for
strings containing "$" arguments, where it's unclear what 'find'
does or should do.

New pow~, log~, exp~, abs~, and 'wrap' objects.

The 'print' object takes "-n" flag to suppress "print:" in output

"clear" button in Pd output window

".pdrc" loading suppressed if pd is started with "-noprefs".

Bug fix in pipe object: if sending a list to pipe, it didn't update the
delay time when asked to.

Binbufs fixed to handle arbitrary length messages. (This fixed a problem
reloading data structures with huge arrays).

various fixes to

------------------ 0.41-3,4 ---------------------------

2 fixes for PC: no bonk~, and the audio device selection dialogs
didn't show all the devices.

------------------ 0.41-2 ----------------------------

More bug fixes: large netsends dropping messages, and crash bug when
turning DSP on and off repeatedly in MS windows

------------------ 0.41-1 ----------------------------

Fixed a startup problem for Mac OSX 10.5.1 (other platforms should not
be affected)

------------------ 0.41 ----------------------------

Pd may be compiled in 64 bit address spaces; this is well tested on
linux and at least seems to work on Microsoft and MacOS. On linux, in
fact, if you compile Pd in a 64-bit version of the OS it's
automatically in the 64 bit address space.

In linux, a "-nosleep" flag causes Pd to poll instead of sleeping as
it waits for dac and adc; this is useful on multiprocessors when you
don't mind having Pd occupy 100% of one processor and want to get
latency as low as possible. (If you don't use this, latency on
multiprocessors - Intel Core 2 at least - is much worse for some reason
than it is on uniprocessors.) It's "experimental" but I use it every
day.

added an experimental callback scheduler. This might reduce latency in
MacOS and/or Microsoft, although I haven't seen that actually happen
yet.

removed limitation on huge messages from text files; this prevented long
"data" arrays from reloading from disk correctly.

fixed crash bug closing patches with open GOPs.

changed selection strategy so that, right after duplicating a collection
of objects, clicking tends to select an already-selected object.

the cursor changes slightly more appropriately when switching between
edit to run modes.

got really confused about the proper role of "declare 'path" in
abstractions; for the moment they are ignored. I the future, it may be
worthwhile to allow them but to have them act only locally to the
abstraction; but this might mean a lot more computation overhead when
opening files.

limited window sizes to that of the screen (otherwise, on Mac OS, it's
impossible to resize them.)

fixed "startup" dialogs to allow unlimited "path" and "lib"
entries

started, but didn't finish, the process of getting Pd to compile with
t_float as 64 bits. This would slow Pd down but improve tabread4~'s
accuracy.

made IEM Guis respect "-font" flag and friends. New startup flags:
"-font-face" (badly named); "-font-size" synonym of "-font".
(Large patch from HC).

String overflow protection here and there.

migrated to ".net" compiler ("VC 2005", a free download).

------------------ 0.40-1 --------------------------

Fixed "declare" which wasn't working properly yet in 0.40-0, and made
more objects (notably "soundfiler") respect "declared" paths. Path
entries are relative to the parent patch. Declares inside abstractions
are ignored.

"drawnumbers" are draggable now, even if they're in arrays.

Bug fix opening "html" help from windows.

Changed MACOSX to __APPLE__ in 4 places.

------------------ 0.40 -----------------------------

A new object, "declare", allows patches to control where Pd looks for
resources such as abstractions and libraries.

Symbols can now be built using multiple dollar sign variables, as in
"$1-$2.$3". Meanwhile, naming subpatches as in "pd $1-foo" now
seems to work correctly.

The switch~ object takes a "bang" message to compute one block of DSP
on demand. Also, block sizes are no longer required to be powers of two
(although reblocking only works between powers of two.)

Externs may have characters in their names that aren't part of valid C
variable names (i.e., outsize the range a-z,A-Z,0-9, _).

Openpanel and savepanel take an argument to set the initial directory to
search.

Support for large (>2G) soundfiles, in operating systems that offer it.

Text copy/paste fixed (I hope) for Macintosh

Intel Mac binaries. New naming scheme for externs to allow
disambiguation (although Pd falls back to the old names for
compatibility.)

Templates can get notified when data are selected/deselected. I want to
provide this for mouse actions on locked canvases too, but haven't yet.

Characters like "", "[", etc., typed into dialogs for labelling
number boxes or other GUI objects, get filtered out so that they at
least don't hang Pd up.

Drawcurve (and the others, filledcurve, drawpolygon, filledpolygon) have
a new "-x" flag that inhibits a scalar getting selected when the curve
is clicked on. This is useful for drawing stuff in the background
(grids, clefs, etc).

6 or so patches adopted from sourceforge.

A "list length" object

IEM Gui labels (except for the VU meter) default to 10 point fonts
(typically 8 before).

"send" without an argument gets an inlet to set the destination

A -noprefs flag defeats loading startup preferences. This gives you a
way to rescue things if Pd's settings somehow crash Pd on startup.

tabwrite~ takes a "start" message to allow writing into the middle of
the table.

------------------ 0.39.2 --------------------------

Bug fixes: memory leak in OSX version; problem printing numbers as
symbols.

------------------ 0.39.1 --------------------------

Bug fixes: compatibility problems with older version of TK

------------------ 0.39.0 --------------------------

At the source level, "regular" arrays and arrays within data
structures are now the same thing. This will mean that, in the future,
features introduced to one array type will become available on the other
one too. Array elements are "scalars" (i.e., data structures) and if
they have drawing instructions, each point of the array is drawn
according to them; they can be clicked on, etc., just like any other
scalars. "Regular" arrays have points which are of a special, built-in
template, "float".

Drawing instructions now can use variable ranges for screen coordinates;
for instance, specifying an offset of "a(0:10)(100:200)(0.2)"
specifies that the member "a", which should range from 0 to 10, should
be graphed at locations ranging from 100 to 200 (relative to the
scalar's base location) and should have a "grain" of 0.2, i.e., steps
of 2 pixels each.

Drawing instructions can be turned on and off, either globally (for all
data of the given structure) or by a data field.

The "struct" object has an outlet to notify you when a datum is
selected or deselected.

Graph-on-parent subpatches and abstractions no longer scale the GUI
objects to fit the parent rectangle; instead you get a sub-rectangle in
the subpatch, of the same size as the parent object, to place GUI
objects in. GUI objects that don't fit inside aren't shown on the
parent, and the parent objects no longer stretches itself to show things
that wouldn't otherwise fit. Older patches work as before until you try
to edit them - at which point you have no choice but to use the new
functionality.

The font size of a Graph-on-parent abstraction is that of the
abstraction itself, not the calling patch.

Message boxes now take "addcomma" and similar messages.

A "list" object is provided for joining and splitting lists, and
converting between lists and non-list messages.

Pd extension is now added automatically to files on Macintish when you
do a "save as". The tcl/tk version is updated to 8.4.5. This should
run on OSX version 10.2 and later. Also on Mac, drag-and-drop startups
read "libraries" (specified in "startup" dialog) before opening the
file.

The "pointer" object has a method to rewind to the beginning of a
list. A "sendwindow" message forwards any message to the window
continuing the scalar currently pointed to.

Abstractions don't produce visible windows, even if subwindows of the
abstraction were visible when the abstraction was saved.

MIDI sysex messages should now work on all platforms.

Bug fixes:

sending lists to arrays now correctly interprets the first number of the
list as the starting index (following values are then stored
sequentially in the array.)

The rfft~ object's imaginary part had the wrong sign. Also, the
Nyquist bin is now supplied correctly.

Fixed problems writing aiff files using the writesf~ and soundfiler
objects. Writesf, if sent an "open" while a file was previously being
written, closes the previous file first.

Bug fix in number2 which sometimes crashed Pd.

Stale-pointer protection made more robust.

Some of Pd's tcl/tk error messages have been tracked down, but probably
not all of them yet.

"Find" crashed Pd when the found object was in a GOP.

Mouse motion over arrays no longer is quite so CPU-consuming (but is
still somewhat so.)

samplerate~ now reflects up/downsampling.

Tilde objects in blocked, overlapped subpatches no longer adjust their
internal sample rate to reflect the overlap.

Fixed a thread-safety problem in sys_microsleep().

------------------ 0.38.1 -------------------------- 

Fixed two bugs that
crashed Pd when deleting number boxes in certain situations.

------------------ 0.38.0 --------------------------

The big change is queued graphics updates, which apply (so far) to
tables and number/symbol boxes. The IEM GUIS aren't enqueued yet. This
along with a better graphics update buffering scheme makes Pd's
graphics run much better.

Support for cutting/copying/pasting text between boxes and between Pd
and other applications.

Dialogs for setting and saving path, libs-to-load-on-startup, and some
other things. This and the audio settings can be saved automatically to
the appropriate repository (.pdsettings on linux; registry on MS
windows; "Preferences" on Mac.)

"Print" printout goes to the Pd window by default. You can revert to
the old (standard error) behavior with the "-stderr" startup flag.

The "gui" TK script can now start Pd up (previously Pd had to be
started first.) This is needed for Pd to work as an "App" on Mac.

new filter objects: cpole~, fpole~, etc... these will get used in the
upcoming Techniques chapter 8.

Objects whose creation failed get a distinctive outline; if they are
already inside a patch they sprout inlets and outlets as necessary to
preserve connections.

Filenames in the "search path", etc., now may contain spaces, commas,
and semicolons.

bug fix: click on miniaturized subpatch failed to "vis" it

bug fix: font size change crash reported by CK

Key bindings like control-Q now work even from within most dialogs.

The audio settings dialog now permits turning audio input and/or output
off without forgetting how many channels it should be when on.

RME Hammerfall ALSA support from Winfried -- but specify the number of
channels correctly or else Pd crashes.

portaudio (e.g., Mac) audio support fixed for inchans != outchans, so
the emi emagic can now be used 2-in. 6-out, for example.

(linux) The configure script can set the setuid flag on "make
install". The "-enable" flags to ./configure should now work
correctly too.

atan2 had its inlets switched to conform to standard usage

------------------ 0.37.3 --------------------------

Oops- added __i386__ macro to windows makefile so it would test for
underflows correctly. This affects only Microsoft Windows; the other two
platforms are fine as 0.37.2. Thanks to Thomas Musil...

------------------ 0.37.2 --------------------------

fixed a bug in soundfile reading (soundfiles now default to wav better.)

fixed gfx update problem in hradio and vradio

minor changes to built-in Max import feature (but you should still use
cyclone's instead.)

colors for scalars fixed (probably never worked before!)

added a "set" message to the line object

aliased spaces to underscores in GUI labels so that at least they won't
destroy the object.

------------------ 0.37.1 --------------------------

fixed the apple key on OSX so it does key accelerators

fixed bug in -inchannels/-outchannels arg parsing

major editions to the IEM GUIs to fix bugs in how "$" variables are
handled. The code still isn't pretty but hopefully at least works now.

bug fix in vd~ for very small delays

fixed MSW version not to make windows grow by 2 pixels on save/restore

added an "nrt" flag for OSX to defeat real-time prioritization (useful
when running Gem.)

on some platforms, audio open failures are handled more gracefully.

added a "changelog" file in the source directory to document
source-level changes.

------------------ 0.37 --------------------------

Pd is finally fixed so that it can open and close audio and MIDI devices
on-the-fly (previously it opened them once at startup and hogged them
until Pd quit). Starting DSP causes audio devices to be opened, and
stopping it closes them. There are dialog panels in the "Media" menu
(which used to be called "Audio") for choosing audio and MIDI
settings. The "path" also can be changed on the fly via a dialog in
the "File" menu.

A "vline" object acts like "line" but to sub-sample accuracy. See
the audio example, C04.control.to.signal.pd (and/or chapter 3 of [Theory
and Techniques of Electronic Music
).](http://msp.ucsd.edu/techniques.htm)

The block~/switch~ object now takes a "set" message to dynamically
change block size, etc.

The makefilename object takes a "set" message to set the "pattern".
You can use this to kludge multiple substitutions (as shown in the help
file).

The writesf~ object got an update and a better help window. It now
should be able to write 32bit floating-point WAV soundfiles. The file's
sample rate is now set "correctly".

Various improvements were made in audio I/O to improve stability and
reduce latency.

Jack support should work for Mac OSX (it appears as a separate API).
Linux is offering experimental portaudio V19 support (but Mac and
Window/ASIO are still based on PA V18.)

The fiddle~ object (in extra) has an "npoints" method to set the
analysis window size dynamically.

(windows) Pd is now distributed as a self-extracting archive.

(windows) url files in the help directories are opened correctly.

(Mac) the arrow keys should now be fixed.

(linux) The "configure" script should be better at finding TK in
various distributions (debian users previously had to use a special
configure script.)

(developers) Pd now exits cleanly from its main loop instead of bailing
out. A mutex protects Pd's data so it can be accessed from other
threads. (Thomas Grill's improvements.)

(developers) The "savefunction" and "dialog" widget behaviors were
replaced by a better mechanism (class_setsavefn() and
class_setpropertiesfn()). They're declared in m_pd.h so you don't have
to include the (unstable) g_canvas.h to get them.

(developers) Better flag handling in the IEM GUIs (g_toggle.c, etc)
should compile with fewer warnings and be more portable.

------------------ 0.37-test 1 --------------------------

The MacOSX version now prioritizes itself effectively (thanks to
gert@test.at (v93r)) via Adam Lindsay). Adam also made a proper MacOSX
"package" for Pd.

A bug was fixed in readsf~/writesf~ (things were coming out in the
wrong number of channels.)

A problem compiling Pd with TK8.4 (the latest version) was fixed.

Large numbers of GUI improvements by Adam Lindsay, especially relevant
to Mac OSX.

For externs, the binary may now be included in a subdirectory of the
same name (e.g., "choice/choice.pd_linux" and "choicechoice.dll").
So now you can pack multiple binaries for the same extern, along with
the source, in one convenient place. (Note that "expr~" is an
exception, since it goes by three different names, so this trick fails
for that example.)

"Help" files renamed "help-xxx.pd", so that help files are now
possible for abstractions. The "help path" feature from CVS (I forgot
who contributed that) is also included but should now not be needed: Pd
remembers where it got externs and abstractions and looks back in the
same directory for a help file. See the way "extras" is organized.

Pd refuses to connect signal outlets to non-signal inlets.

When you save any patch, Pd looks for all invocations of that patch as
an abstraction and reloads them. This unfortunately has the side effect
of making all the containing windows visible, but it's better than
nothing.

------------------ 0.36-1 -------------------------------

"print" now queries you for a file to save the postscript to.

"expr" brought up to date (0.4) -- a bug was fixed involving
expressions like "max($f1, 100)" which had erroneously output an
integer.

a bug fix in the 4-point interpolation formula, which affects tabosc4~,
tabread4~, tabread4, and vd~. These should have significantly lower
distortion than before.

bug fix: vradio, hradio "send symbol" feature didn't work

------------------ 0.36 -------------------------------

There's now an "undo" for most editing operations. Undoing is only
available in the window that was most recently edited. (One gotcha
remains, that "stretching" (in the font menu) affects all windows and
you can't undo any but the last one it touched.) Also, there's no
"undo" for run-time operations, only editing ones. That might be worth
thinking about.

Some bugs were fixed that affected "flipped" canvases (ones whose
"properties show a positive "y" increment per pixel.) Also, the
coordinates are now saved and restored correctly. "text" objects
(comments) now stick to the bottom of the window for flipped canvases.

Signal lines now show up fatter than control lines. (Now I have to go
through the figures in the HTML doc again... drat)

"Classic" number boxes now can have labels and send/receive signals,
which work in the same way as the IEMGUI controls do. I think "$1"
style label/send/receive names work too. I fixed a related bug in the
IEM code (typing at boxes sometimes crashed Pd).

"vdial" and "hdial" were renamed "vradio" and "hradio", and
fixed to output numbers, not lists, like the other GUIs. The old ones
are still around for compatibility with old patches.

"Make install" should now actually make Pd before trying to install
it.

"expr" is updated to Shahrokh's 0.4test3 version (which I modified
somewhat to get it to compile.)

The problem of CPU usage skyrocketing on underflows in P4s should be
fixed.

Compiled "pdsend" and "pdreceive" for Windows.

"PD_VERSION" macro added to m_pd.h

------------------ 0.35 -------------------------------

An experimental new feature called graph-on-parent allows subpatches and
abstractions to show GUI features; so, for instance, you can make an
oscillator with a number box to control the frequency. This is described
in section 2.7.2 of the HTML documentation and an example is shown in
7.stuff/synth1/.

Spaces are allowed in pathnames to Pd and to patches; however, the
"path" variable still can't have spaces. (You can address path
directories using relative pathnames as in "../sound" (or ..sound on
Windows), even if there are spaces further "up" the path to the patch.
See 3.7, "dealing with files."

The soundfile reading routine (used in readsf~ and soundfiler) is much
better at opening wav files with different header sizes and odd chunks.
You can now read floating-point "wav" files -- although you can't
write them yet.

Templates and data structures are extensively reworked. A "struct"
object replaces "template", so that you specify the name of the
structure as the first argument to "struct" (previously it was derived
from the window name.) You can now have multiple "structs" of the same
name; the oldest one is the "real" one, but if you delete that, the
structures are all conformed to the next-oldest one, and so on. You can
alter the contents of a "struct" and all the associated data will be
modified to fit the new structure definition. Data are persistent, i.e.,
saved with the containing patch. You can copy and paste data between
patches. If you save data to a file explicitly, you can read it into
another patch and the data are conformed automatically to the new data
structures.

A new version of Thomas Musil's GUI objects was merged in.

The testtone patch works for up to 6 channels of audio input and output.

Lots of improvements got made to audio I/O in general. In NT you may
specify "-asio" to use ASIO drivers; see HTML documentation section
3.2. You may specify lists of audio input and output devices. In Linux,
Pd will now attempt to open each /dev/dsp* only once, even if it's
requested for reading and writing.

The "extra" directory is now searched after the directories in the
search path, not before (so now you can override objects like
"fiddle~").

A bug in paf~ is fixed.

In Linux, the ".pdrc" is now read before the command line arguments,
so that command line arguments override the .pdrc (it was backwards
before.)

In Linux, "help" now can invoke either mozilla or netscape to start up
the HTML documentation. This doesn't work in Windows or Mac land yet.

In Linux, the "-32bit" flag was added, which you must now use if
running Guenter's OSS RME Hammerfall driver. (This was necessary
because OSS went and used the same "bit" for a different purpose, so
that Pd tried to open some other cards in 32bit mode inappropriately.)

In Linux, MIDI is now opened "-NODELAY" ... this makes the OSS
Creative driver take MIDI input correctly which it didn't before.

In MS windows, you can now use "readsf~/writesf~" for spooling
sounds to and from disk.

MS Windows bug fixes: -nosound was ignored, and now works. Also,
clicking to open abstractions, when they were already open anyway, used
to lose the keyboard; this should be fixed now. Finally, "netreceive"
didn't work when running "-nogui". This is fixed, and moreover, you
should definitely include a netreceive object in any -nogui patch in
MSW, otherwise it eats up all available CPU time gratuitously.

The outlet is removed from the "table" object.

In MS Windows, Pd now has "-resync" and "-noresync" flags so that
you can specify how to deal with audio input and output blocksize
nonsense in MMIO. If "resync" is on, whenever the audio input and
output seem out of whack the audio driver resynchronizes all input and
output devices; otherwise the situation is simply ignored. "Noresync"
is probably best for consumer stereo cards (and is the default if
you're running only 2 channels in and out). If you're running more
than 2 channels in either direction, the default is "resync".

In soundfiler's read method, if you specify "-maxsize", that implies
"-resize" (as it ought to.)

You can use $1-style names for arrays and tables.

Pd will now refuse to make duplicate connections between objects.

Pd is (somewhat shakily) running on Macintosh OS/X. See section 3.4 of
the HTML doc. For Macs with one-button mice, you can double-click in
edit mode to simulate a right click. Unfortunately, the "alt" key
doesn't work yet.

In Linux, ALSA audio is now fixed to clip, not wrap around, on output
overflows.

Various problems were fixed with objects changing size. Number boxes
never wrap to two lines (as they used to), and lines are reconnected
appropriately when objects are resized.

A function call is added to retrieve a unique event-dependent number, so
that objects like "buddy" can be written.

All the "sound" command-line flags now have "audio" equivalents.

The "-listdev" flag now works on Mac and MSW/ASIO.

Help file updates for env~, route, and pointer

------------------ 0.34.3 -------------------------------

fixed a bug in "udp" netreceive that crashed pd

fixed a bug in tabosc4~ that caused gritty sound

changed "specfile" for RPM releases (thanks Fernando)

adopted Krzysztof's glob_setfilename bug fix

bug fixes from "the joy of global variables" thread in Pd list

made a help window for "table".

------------------ 0.34.2 -------------------------------

fixed ".pdrc" bug

added an experimental "pd restart-audio" feature for (new) Alsa

------------------ 0.34.1 -------------------------------

Bug fixes:

1. Closing a window with objects selected crashed Pd.

2. "find" when it opened a window to show the found object crashed
Pd.

3. (Linux only) Oversized .pdrc files crashed pd...

Also, I updated Thomas Musil's IEM GUI objects and their help files.

------------------ 0.34 -------------------------------

NEW FEATURES:

I incorporated Thomas Musil's GUI objects (slider, button, etc.) into
the Pd release so Thomas won't have to publish patches to Pd anymore. I
didn't take the graphical inlets and outlets for reasons explained
elsewhere, but Thomas might decide to continue supplying them on a patch
basis.

Many new examples were added to the "2.control" and especially
"3.audio" example patches. A list of differences between Max/MSP and
Pd now appears at the end of this section.

Finally, I fixed Pd to notice window iconification and suspend graphical
updates for iconified windows.

Numbering of versions of Pd will now be as in "0.34.2" instead of
"0.34PATCH2" which was confusing.

BUGS FIXED:

I incorporated Krzysztof Czaja's menuclose bug fix in g_canvas.c.

(Linux) the configure script is more rational.

the qlist and pack objects were fixed to handle reentrancy correctly.

Pd now complains about running out of memory (before it dies.) I intend
to provide advance warning and automatically back out of loading patches
that would run out of memory, but that's not in place yet.

Typing into a message box sometimes left you with lines from the output
pointing to the wrong location. Fixed.

Reading of "wav" and nextstep soundfiles now handles the headers
better.

------------------ 0.33 -------------------------------

AUDIO AND MIDI:

MIDI time jitter is reduced. Theoretically, it could now be as low as
the audio blocksize (and so if you care about MIDI timing, keep your
audio blocksize low.) If you run Pd with audio in stream mode or without
audio at all, and perhaps in some cases in block more too (?), the
controlling parameter for MIDI jitter is "-sleepgrain", which
specifies the interval of time Pd sleeps when it believes it's idle.

You can now specify multiple MIDI input and output devices. For example,
"pd -midiindev 3 -midioutdev 4,2" asks for the third MIDI input device
and the fourth and second MIDI output device. The "channel message"
midi objects in Pd such as notein or pgmout will take channels 1-16 to
mean the first open MIDI port, 17-32 the second one, and so on. The
midiin, sysexin, midiout objects give you a separate inlet to specify
which of the open MIDI port numbers you want.

(Linux only) By default, Pd now reads and write audio in "block mode."
Previously you have to specify "-frags" and/or "-fragsize" to get
this. As of this version you have to specify "-streammode" to get the
opposite, streaming mode. This mode seems only to work with a small
number of sound cards, notably Ensoniq ens1370 and ens1371.

(Linux only) Also, "-fragsize" is replaced with a more convenient
"-blocksize" which you specify in sample frames. It defaults to 64
which is Pd's audio computation block size but may be larger or
smaller. Typically you would specify "-audiobuf" and "-blocksize"
and Pd will compute "-frags" for you; but you can also specify
"-frags" explicitly.

(Linux only) OSS and ALSA audio support are improved. You can now talk
to RME9652 using Guenter's OSS driver; this is different from the
"-RME" support which uses Winfried's older driver. Other multichannel
OSS drivers might now work as well. Pd also seems to work with ALSA 0.9
Beta 4; I've tested this with Midiman Delta 66 and Soundblaster live. I
plan to update the linux audio setup documentation accordingly.

NEW FEATURES:

I've put in Shahrokh's new expr, expr~, and fexpr~ objects. The
latter allows you to make expressions referring to prior input and
output samples in case you're interested in writing your own recursive
filters, oscillators, or chaotic sound generators...

In support of expr, you can now use commas in "object" boxes; they
just become symbols.

sqrt~ is fixed so that it apparently has 24-bit accurate mantissas. It
turned out to be easier to just make it accurate than to confront the
question of how a reduced-accuracy version should be named.

The bizarre framp~ object which does phase vocoder analysis got a help
window. The phase vocoder example doesn't use framp~ and I had
forgotten what it did until Guenter dug it back up.

(Linux only) I finally got around to incorporating Guenter's autoconf
stuff, and learned about RPM. Major new Linux releases will probably be
in .tar.gz and .rpm formats; "test" releases will probably just be in
.tar.gz. I also fixed it so that the installation prefix is overridden
if you invoke pd by its full pathname, so that you can still use
compilations with installation prefixes before you actually install
them.

(NT only) I added support for directX using the portaudio package by
Ross Bencina and Phil Burk. I couldn't discover any way this would ever
outperform the old "multimedia" API Pd uses. So the release contains
the sources, but you have to recompile Pd to use directX. Use
"makefile.nt.portaudio". Only 1 or 2 channels of audio are supported.
The interesting thing is that the same code will run on Macintosh. There
are a couple of other obstacles to a MacOS port of Pd though; it's hard
to predict when this will be feasible.

BUG FIXES:

"drawnumber" was broken in 0.32 -- fixed.

new arrays in 0.32p6 got ill-fitting graphs -- fixed.

------------------ 0.32 PATCH 6 -------------------

Got array and graph dialogs to behave better when there are more than
one.

put in mtof~, etc.

made Pd search the "extra" directory without having to specify it in
"path."

bug fix in exporting patches to Max

------------------ 0.32 PATCH 5 -------------------

Reversed the order of these release notes so that the newest appear
first.

Arrays can save their content with containing patch; the properties
dialog selects this. The dialog shows up when you create a new array
from the menu, and allows you to set the name and size. Only floating
point arrays can be created and edited this way.

Bug fix: the figures in the NT web doc were garbage.

Bug fix: large tables (> 800 pixels and points) no longer crash the
GUI. A related problem remains; large arrays are truncated to either
1000 points or 1000 pixels.

Bug fix: doing "save as" on an instantiated abstraction no longer sets
the window title.

in linux, a couple of status messages on opening /dev/dsp only appear
now if Pd is run "-verbose".


------------------ 0.32 PATCH 2, 3, 4 -------------------

Hassled more with font size differences between NT and Linux, and
updated many help files. Minor bug fixes here and there.

the table object now takes a second argument to set size in points.

Improved underflow protection in some DSP objects.

pointer now has a "vnext" traversal method which goes forward to the
next SELECTED object.

improvements to throw~ (it now sums) and receive~ fixed to be
settable.

bug fix in which RME driver always thought sample rate was 44100.


------------------ 0.32 PATCH 1 -------------------

bug fixes (bugs flagged by mik): vcf~ help window crashed; writesf~
only wrote 1 channel soundfiles; "table" object didn't open when
clicked on;

new object: tabosc4~ -- finally, a real wavetable oscillator for Pd.

much work on "data" editing; go to 7.stuff/data-structures, open
patches 5 and 7, and try clicking on things. Alt clicks delete or add
points; regular clicks drag values around. The cursor changes to show
you what will happen if you click.

------------------- 0.32 -----------------

**New objects:**

midiin, sysexin, midiout. (I don't think MIDI sysex is working in
Windows yet though.)

threshold~ as in Jmax, triggers from audio level.

value as in Max and Jmax.

writesf as in Jmax.

**New startup flags:**

-sleepgrain: if you aren't using audio I/O, this can reduce time jitter
in MIDI I/O. Otherwise, MIDI I/O jitter is limited by the audio buffer
size.

-noloadbang: cancels loadbangs.

-nogui: suppress starting the GUI. You can then still talk to Pd using,
perhaps among other possibilities, the new network connection programs
now included in the release.

-guicmd: lets you specify the command string Pd calls to start the GUI,
in case you've written your own GUI to replace the TK one Pd comes
with.

-send: after loading all the patches specified in the command line, you
can specify "startup" messages to send. For example, if you want to
use Pd just to play 50-channel soundfiles from a shell, this is how you
can specify the soundfile name on the command line.

**bug fixes.**

A readsf~ problem got fixed.

hitting the tab key used to cause Pd windows to relinquish the keyboard.

The $0 feature appears now to work.

Inlets and outlets of subpatches sometimes got out of left-to-right
order.

Scrollbars are less out of whack than they were before.

Pd now knows to de-iconify windows if you "vis" them from the parent.

**in general:**

In Linux the treatment of MIDI input is now much more efficient. Also,
bugs were fixed in notein and (for SGI) bendin.

You can "select all" from the Edit menu.

standalone programs "pd-send" and "pd-receive" are provided that can
send messages to Pd or receive messages from Pd via the netsend~ and
netreceive~ objects. This should allow you to interface a wide variety
of other programs with Pd either on the same machine or over the
network. Also you should be able to hack the code into your own programs
to make them interoperate with Pd and/or each other. The underlying
protocol is called FUDI.

"Properties" for scalars, graphs, and number boxes: left click on
them. In particular, number boxes can have fixed widths and finite
ranges; if you make them one character wide they act as toggles. Later
you'll be able to configure them as sliders.

As to scalars, the properties dialog lets you edit the data in the raw.
Don't try to edit the template though; you can't.

You can now type into a "pd" object to change its name without losing
the contents.

An experimental "scalar" _text_ object now allows abstractions to
draw primitive control panels on their parents when you invoke them, as
if they were Moog or Buchla modules. See the "7.stuff/data" examples.

New help windows for the "data" classes (pointer, append, template,
etc.) and for send/receive which somehow I had neglected.

When you hit "copy" with nothing selected, the copy buffer used to be
cleared. This is fixed to do nothing.

------------------- 0.31 -----------------

ALSA support in Linux has been completely overhauled. It now works with
Midiman (up to 10 in/12 out!) and es1370. There are problems with SBLive
under ALSA but it works in OSS emulation with a "-frags" setting. See
the "getting started" documentation.

In NT, the default is now "noresync" if you're running stereo. You
can override this with the "-resync" flag. If you're running more
than 2 channels it's the opposite (as it was before.)

"symbol" boxes now display symbols and let you type them in.

There was a bug when you renamed a patch from outside Pd; the old
filename still showed in the title bar (and there were other bad side
effects.) Fixed.

Protection was added against patches opening themselves as abstractions.

The "route" object's handling of leading symbols was improved. I'm
not sure whether it's Max compatible or not.

You can draw into arrays with the mouse, at least in the case where
there's at least one pixel per point. (I'm not sure if the other case
even makes sense.)

Abstractions display their "$1", etc., arguments in the window title
bar.

A "sort" method was added for lists to make them easier to use as
sequencers.

The "save as" dialog makes a more reasonable choice of start-up
directory.

"Trigger i" is now disallowed (it used to crash Pd.)

Getbytes and resizebytes now zero out new memory.

A memory leak reported by Hannes has been partly, hopefully mostly,
fixed.

The "signal_free 2" bug reported by Fogar is fixed.

New graphs now reliably avoid using already-taken "graph%d" names.

The old bug which showed up as ".xxxxxxxxx: no such object" is fixed.

The FFT examples have been reworked and the "pique" and "shift"
objects are moved to "extra".

------------------- 0.30 -----------------

in Linux, you can get Pd to promote itself to "real time" priority. A
"watchdog" process protects you from having Pd lock your machine up.
You must request real time by running "pd -rt" or "pd -realtime".
You must either be superuser or make Pd a root-owned SETUID program
(chown root .../pd/bin/pd; chmod 4755 .../pd/bin/pd). For security
reasons, Pd relinquishes root privilege immediately after setting its
priority, before loading any patches or externs.

Protection was added against message loops.

loadbang was fixed so that loadbangs in abstractions go off before
loadbangs in the owner patch. Within each patch, loadbangs go off first
in subpatches.

new object: tabplay~, a non-interpolating sample reader.

new objects (in "extra" library): loop~; rev1~.

The "toys" library was renamed "extra" and incorporated in the Pd
release.

In Linux, timeouts were added to the driver opening and closing code
(which used to hang under some conditions.)

the "field" object was replaced by "template"; see
"data.structures" examples in 7.stuff. Data lists can be read from and
written to files now.

You can invoke an external object by pathname, as in
"../../extra/loop~".

hip~, etc. should no longer get stuck when they get a NAN on input.

a bug was fixed in expanding symbols such as "$1-foo".

------------------- 0.29 -----------------

readsf~ - a MAX/FTS style soundfile player, which reads multichannel
soundfiles in wave, aiff, or next formats. The files must be 16 or 24
bit fixed point or 32 bit floating point (only nextstep headers
understand the latter.) You can also override the header. A "skip"
flag lets you read starting anywhere in the file. (Sorry: linux only for
now; I can't find Posix threads packages for the other platforms.)

soundfiler - support for reading and writing soundfiles (wave, aiff,
nextstep) to and from arrays. Multichannel soundfiles can be read into
or written from several arrays at once. When reading you can ask that
the tables be automatically resized; in any event the object obligingly
outputs the number of samples actually read. When writing you can
specify a sub-segment of the arrays, and/or request that the
soundfile's maximum amplitude be normalized to one.

tabplay~ - a non-interpolating sample player

Garry Kling reports having compiled Pd for "yellowdog" linux on
Macintosh computers. One "fix" has been made to s_linux.c to
facilitate this. I don't have access to a Mac running linux at the
moment so I can't verify whether any particular release of mine
actually works there.

Signal objects now automatically convert scalars to vectors, so that you
can just run a number box into a signal input. One caveat is that the
binops "+~", "-~", "*~", "/~", "max~", "min~" run
slightly faster if you give them an argument to tell them that their
right inlet will be scalar; so the construction "+~ 0" is still
meaningful. This will get fixed at some later date...

Font sizes work in what I hope will be a more machine-portable way. On
any machine, the point sizes 8, 10, 12, 14, 16, 24 are DEFINED to be the
largest fonts Pd can find that don't exceed their size on my linux
machine. This way I can write patches that everyone else can read, and
others will at least have fewer portability problems than before. The
downside is that your old patches may appear with a different type size
than you want; use the "font" menu item to fix them.

The OSS support no longer asks the audio driver whether full duplex is
needed; it just tries to open it. Apparently some drivers (such as
ALSA's OSS emulation) might do full duplex but not implement the call
Pd used to query for it.

You can give "-nomidi" as a flag (previously you had to type
"-nomidiin -nomidiout".)

A GUI bug reported by Iain Mott was fixed.

You can now type symbols such as "$3-poodle" and the "$3" portion
gets expanded properly. Someone was also asking about the FTS-style #0
feature, but I couldn't figure out how to reconcile it with Pd's usage
of "$" for "#" in abstractions. So I'm still searching for a good
way to provide local symbols.

the GUI now protects itself from "", "{" and "}" characters by
dropping them. I wonder how many NT users have crashed Pd trying to type
in filenames with backslashes...

samphold_set and tabwrite_stop methods added. There turned out to be no
help window for samphold~ so one was supplied.

------------------- 0.28 -----------------

Version 0.28 has a primitive in-box text editor... about time!

the "front panel" now gives you information on audio levels and sync
errors.

Message boxes flash, sort of, when you click them.

Support has been added for RME 9652 soundcards; see the Linux soundcard
section of the documentation. Support files for RME and PCI128 (Ensoniq
es1370) cards are released separately from Pd.

The delete and backspace keys clear the current selection. There is
unfortunately no "undo" though; I'm not sure this is a good thing to
have put in.

The "until" object has a "float" method which limits the number of
bangs it will output.

The audio setup is better documented for NT and Linux.

The externs in 4.fft and 6.externs got recompiled and tested.

BUG FIX: the "read16" message to tables was broken on NT and is now
fixed.

BUG FIX: In Linux, starting Pd up sometimes changed the audio mixer
setting.

BUG FIX: sending "floats" to inlets expecting lists now works
correctly.

BUG FIX: "route" on symbols now deals better with symbols, floats and
lists.

------------------- 0.27 -----------------

The main new feature is the "find" menu stuff. You can search for
boxes containing specified atoms, including semicolons or commas. Most
errors are now trackable, allowing you to "find last error". Look in
the "Find" menu.

New objects written: change, max, max~, min, min~, and swap.

I looked in 0.INTRO.txt in 5.reference, and found that the objects bag,
cputime, realtime, pipe, symbol, poly, and bang were missing.

Five or six bug fixes.

Some audio problems in 0.25 were addresses. In Linux, audio drivers that
don't support the GETISPACE/GETOSPACE ioctl calls can be called using
the (inferior) "-frags/-fragsize" mechanism. If you specify either a
"-frags" or a "-fragsize" option, the GETIOSPACE calls are
cancelled.

Under NT, for some audio drivers the 0.26 release gave a constant stream
of "resync" events. I don't know what causes this but I added a
"-noresync" option which simply never resyncs at all.

------------------- 0.26 -----------------

phasor~ and osc~ can be configured to take floating point messages to
set their frequencies, as an alternative to having an input signal to do
the same. Also, +~, etc, can take floating point arguments (and
messages) to add or multiply scalars. The +~, etc, loops were unrolled
to make them run faster.

A switch~ object is provided to let you switch sub-patches on and off.
The inlet~ and outlet~ objects were re-written to avoid adding any
overhead when moving signals in or out of sub patches.

In Linux at least, the audio latency is much reduced. It's possible to
poll for audio I/O lateness errors by sending "pd audiostatus".

When reading a sample using tabread4~, you can switch between sample
tables using the "set" message.

A new "textfile" object is like qlist but more flexible.

Many help windows got updated (but at least a dozen more need work
urgently).

A dsp_addv function was added to allow variable-length DSP calls (for
writers of tilde externs.)

It's possible for a tilde extern to have a name ending in "tilde"
now. Name the setup routine "foo_tilde" for "foo~", etc.

The dac~ object was fixed to clip its output when out of range (before
it wrapped around.)

A first line of protection was added against getting numerical underflow
in delay feedback loops. Before, when a reverberator tailed out there
was a sudden jump in CPU usage because the numerical underflows would
trap to the kernel. Now, if any delwrite~ is given a value less than
1e-20 or so, it records a true zero to avoid this.

Signal division checks for divide by zero.

A "Font bomb" feature is provided for resizing fonts and stretching
and contracting patches to fit.

Pds now bind themselves to the symbol pd-<window-name).

IN Linux, if Pd is called as root it tries to promote its run-time
priority. You can make pd a setuid root owned program if you want this
behavior for non-root users who start pd. (Don't make pd-gui setuid
though. That would make a security hole in your system.)

The Pd command line can take multiple "open" arguments.

The file search path feature was fixed and generalized.

Alt-clicking a table gives you a dialog to set its x and y range and
pixel size.

------------------- 0.25 -----------------

Lots of minor, under-the-hood improvements and bug fixes...

The Netsend/netreceive objects were improved; you can now choose between
UDP and TCP and there's an outlet to tell you whether they're
connected.

You can now alt click on an object to get its help window (and the help
windows got a fair amount of work.)

multichannel audio I/O -- you can get up to 8 audio channels in and
out. On SGI this is done correctly; on NT it's done using sequential
"stereo" devices. I'm not sure of the status of multichannel in
linux...

The "text" window got new accelerators and a bigger font size

there are 3 "tool" patches in 7.stuff: filtering, pvoc, ring mod.

In NT, command-line backslashes are converted to forward slashes.

There's a load measurement tool in the "help" menu.

The SGI version contains an n32 binary (look at the "bin" directory).

------------------- 0.24 ---------------

new objects:
- bang - convert any message to a "bang"
- qlist - message sequencer
- textfile - file to message converter
- makefilename - format a name with a variable field
- openpanel - "Open" dialog
- savepanel - "Save as" dialog

Bug fixes:
- Fixed a bug in "const" message to arrays
- "exp" was broken on NT, now fixed
- phase vocoder example improved
- "read" message to arrays now zero out unread samples
- bug fix in "key" object
- bug fix in ifft~ (thanks to Peter Lunden)
- "print" object fixed to distinguish between lists starting with
symbols and other messages
- polygon, curve, fpolygon, fcurve renamed to fix name clash with Gem
- improved "new object" placement on screen
- fixed help dialog to remember previous directory (thanks to Harry
Castle)
- heterogeneous lists

Arrays can be written to and read from text files or from 16-bit binary
files. See ../2.starter/2G for an overview.

Guenter Geiger has contributed a Max-style "table" object which
creates an "array" object in a subwindow.

Guenter has also put in a "search path" feature for externs,
abstractions, etc.

The Help menu got reworked.

Select and Route were extended to work Zack-style with symbols.

"random" takes seeds now (see the "help" window)

Some more work on graphical lists; you can see the current state in
../7.stuff/data-structures. It's still nascent.


------------------- 0.23 -------------------

A first cut at the "pure data" feature is now included. See section 6
of the documentation for a quick introduction to it; see also patches 12
and 14 in the FFT examples.

The documentation has been reorganized. The most interesting new
features are:
- some new "tutorial" patches
- 15 "fft" examples
- improved help navigation

more bug fixes:
- titles on abstractions no longer saved inside file
- left-to-right sorting of inlets/outlets now seems to work
- nt audio setup got confused when driver couldn't do full duplex
- opening window with audio on is now fixed
- deleting inlets/outlets deletes connections first (used to crash)
- 1e20 parsed correctly now
- osc1~ fixed and optimized
- resizing arrays with DSP on used to crash; now fixed
- pasting now adds to the end of the list (used to add to beginning)
- clicking now selects the most recent object when two or more overlap
- Pd's "open" and "help" dialogs now maintain separate paths

The phasor~ object's "float" method has been REMOVED -- use the
right-hand inlet to set the internal phase. This is so that I can later
fix all tilde objects to convert messages to signals automatically at
all signal inputs.

------------------- 0.22 -------------------


bug fixes
- parsing 1e+006 gave symbol (now float)
- "." parsed as number, should be symbol
- change GUI polling loop to TK event dispatch (unix only)
- improved "tidy up" feature
- size check added to text boxes (used to crash; still not correct.)
- occasional bug sending text with CRs to tk
- binop startup bug
- key accelerators for creators wrong
- ftom range to 1500
- bug in pack, unpack
- windows restore bigger than saved

Nt-specific bug fixes:
- getsockopt for netreceive fails. Just omitted it for NT.
- put tcl dlls in tcl bin, not pd bin
--- archive tcl subsystem for easier version updates
--- fix README accordingly
- deal with bell sound
- turn on optimization
- looked for audio timeout bug but couldn't find it.

------------------- 0.21 -------------------

bug fixes:

table size change with DSP on: It used to crash Pd to resize an array
when DSP was turned on. This is now fixed.

deselect all when locking. When you lock a patch the selection is
cleared.

unlock when pasting. .. and if you paste into a pastch, it's unlocked.

lost keyboard events. Version 0.20 lost keyboard events and forgot
window size changes. This should now be fixed.
subpatches came up in wrong font size
dirty flag on window title bar fixed
improvement to netreceive suggested by Mark Danks
style notes fleshed out as suggested by Larry Troxler
fixed Bill Kleinsasser's bug (short and long array in same graph)

new features:
phase setting for phasor~
fft objects. Also, block~, for specifying block sizes and overlaps for
FFTs.
canvas_makefilename() (used, e.g., by array_read and write)
"stuff" directory with examples of real Pd applications.

------------------- 0.20 -------------------

In NT, the 0.19 release turned out not to contain all the files needed
to make TCL run. This problem should now be fixed.

Also, the array_write routine was fixed.

------------------- 0.19 -------------------
notable new objects:
- vcf~, a bandpass filter with a signal input for center frequency.
- delread, delwrite, vd, as in ISPW Max.
- various math and midi stuff
- catch~, throw~, send~, receive~ for nonlocal signal connections

- an experimental facility for array of floats is included. You can
make a new array (from the "put" menu) which will be given a name such
as "array1". You can then send it "read <file>", "write
<file>", "resize <N>", and "print" messages. File reading and
writing is in ascii. "resize" changes the size of the array, and
"print" prints its vital signs. You can then use "tabread4~" to do
a 4-point interpolating table lookup, and tabwrite~ to write audio
samples into the table.

Numbers now default to floating point, although certain objects like
"spigot" and "metro" still convert their boolean inputs to integers
so that 0.5 is "false." This behavior will probably change later. The
"div" and "mod" objects are introduced for explicit integer division
and remainder.

Number boxes drag in integer increments, or in hundredths if you hold
the "shift" key down when you click.

Pd documents now save their font sizes. The font size is global to an
entire document. New documents come up in the font size Pd was started
in (using the "-font" flag.) If you want to change the font size of an
existing document, use a text editor; the font size is the last argument
on the first line. 8, 10, 12, 14, 16, 18, and 24 are supported.

The abbreviations "t," "f," and "i" stand for "trigger,",
"float", and "int."

Inlets and outlets of subpatches are now sorted correctly; although
there is still a problem deleting inlets/outlets which have connections.

The size and screen location of Pd documents is saved correctly.

Tilde objects now work in "subpages" although there is no way to send
signals through their inlets and outlets; use throw~/catch~ or
send~/receive~.

On NT, the default is to open both audio output and input (this used not
to work.) The situation is still shaky; audio seems to hang up
sporadically on my machine; but I seem to have installed my audio driver
wrong anyway. I had to set a huge output FIFO (1/3 sec or so!) to get it
to work at all. You can type "pd -dac", "pd -adc", or "pd
-nosound" to get output only, input only, or no audio at all. NT's
MIDI input and output are supported, but on my machine MIDI output is
flaky. I'm curious how all this will work on other machines...

The list of classes is now:

GENERAL: field inlet outlet print int float send receive select route
pack unpack trigger spigot moses delay metro line timer makenote
stripnote random loadbang serial get netsend netreceive

MATH: + - * / == != > < >= <= & && | || % mod div sin cos tan
atan atan2 sqrt log exp abs mtof ftom powtodb rmstodb dbtopow dbtorms

MIDI: notein ctlin pgmin bendin touchin polytouchin noteout ctlout
pgmout bendout touchout polytouchout

SIGNAL: dac~ adc~ sig~ line~ snapshot~ +~ -~ *~ /~ phasor~
cos~ vcf~ noise~ env~ hip~ lop~ bp~ biquad~ samphold~ clip~
rsqrt~ sqrt~ wrap~ print~ scope~ tabwrite~ tabread4~ send~
receive~ catch~ throw~ delwrite~ delread~ vd~

------------------- 0.18 -------------------
Release notes now describe the three platforms Pd runs on: IRIX and NT
(maintained at UCSD) and LINUX, maintained by Guenter Geiger.

menu "close" on a dirty document now checks if you really want to
close without saving (although "quit" will still exit Pd without
verification.)

Got rid of "dll" error printout when loading abstractions

------------------- 0.12 - 0.17 -------------------

got Pd running under NT, although driver problems remain. Gem is also
distributed for both platforms.

------------------- 0.11 -------------------

Here's a list of all the objects in this release:
general: print int float send receive select pack unpack trigger spigot
time handling: delay metro line timer
arithmetic: + + - - * * / / == == != != > > < < >= >= <= <= &
&& | || %
midi: notein noteout makenote stripnote
other: random get
signals: dac~ adc~ sig~ line~ snapshot~ +~ *~
signal oscillators: phasor~ cos~
signal filters: env~ hip~
signal debugging : print~ scope~

"spigot" replaces "gate" but has the inputs reversed.

------------------- 0.10 -------------------

Many bug fixes. This was the first pre-release to be put on the FTP
site.

------------------- 0.09 -------------------

set up the "Help" menu
Bug in DSP sorting fixed
"Notein" and "noteout" objects
Comments from the Put menu say "comment" (they were invisible before)
The scheduler deals better when sound I/O malfunctions

------------------- 0.08 -------------------

metro bug
scrollbars
scheduler bug
text box wraparound at 80 chars.
fixed boxes to reconnect on retype

------------------- 0.07 -------------------

- made an adc~ object

------------------- 0.06 -------------------

- fixed two bugs in DSP sorting
- added DSP on/off gui
- added lock/unlock and changed the cursor behavior
- fixed -font flag to set font pointsize

------------------- 0.05 -------------------

- added scope~, which is just a stopgap until real sound editing comes
up.
- improved the open panel slightly.
- added atoms (int only).
- reworked text editing to reside in Pd, not Pd-gui.
- included a dbx-debuggable Pd in the distribution. I haven't yet
figured out how to get dbx to work with externs though.

------------------- 0.04 -------------------

fixed "cut" which crashed 0.03 if DSP was running. added clip~,
print~, line~, snapshot~.

------------------- 0.03 -------------------

"pd dsp 1", "pd dsp 0" messages added. If you edit a patch with DSP
on, PD resorts the DSP network as needed. Unconnected and multiple
signal inlets are allowed.

------------------- 0.02 -------------------

A DSP network mechanism has been added. DSP objects are: sig~, +~,
*~, phasor~, cos~.

Loading of externs is provided (although there is no search path
mechanism so the extern has to be in the patch's current directory.)
Look in pd/externs for an example.

------------------- 0.01. -------------------

This first release serves mostly to test the "release" mechanism. A Pd
"canvas" object is provided which does both graphing and patch
editing. The editing features apply only to the Max-like part; the
graphs have to be edited into a Pd file via text editor.

Four menu items (in the "put" menu) create the four kinds of
"patchable" objects; they can be dragged and connected as in Max; to
break a connection, just click on it (the cursor becomes a turkey to
indicate this.) Cut, paste, and duplicate seem to work, and a "Pd"
class offers subwindows.

The following max-like objects are included: print; +, *, -, /, ==, !=,
>, <, >=, <=, &, |, &&, ||, %; int, float, pack, unpack, trigger;
delay, metro, timer; send, receive.

-----------------------------------------

### known bugs

These are now tracked on the [Pd GitHub project
page](https://github.com/pure-data/pure-data/issues).

### differences from Max/MSP

It wasn't anyone's intention to make Pd a Max/MSP clone, but on the
other hand, if there's no reason for a feature to appear differently in
Pd than in Max/MSP, the choices in Pd tend to hew to those in Max/MSP.
Moreover, some effort has been undertaken (but more is needed) to make
the two interoperable.

You can use Pd to import and export patches to Max/MSP; just save as
text to a file with extension ".pat", and then open in the other
program. You'll at least get something. If you stick to common or
commonizable features you can actually develop patches for both
platforms. Pd importing of Max patches works best if you use the Pd
Extended package; there are many more compatible objects.

When specific objects exist on one platform and not on the other, it's
often possible to make abstractions to imitate the missing objects, in a
kind of personalized compatibility library.

There are, however, differences in semantics you'll want to know about;
a partial list follows.

**abstraction arguments.** In Pd you can edit instantiations of
abstractions and save the result back to the file of the abstraction.
This isn't possible in Max, because the instantiations are different
from the abstraction itself in that "#1", etc., are replaced by the
instantiation arguments. In Pd, these arguments appear as "$1", etc,
and are translated at a slightly later stage of the instantiation
process so that you still see them as "$" variables in the
instantiation. See section [(abstractions)]({{< mdlink "theory of operation" "abstractions">}}).

In Pd, to make current all instantiations of the abstraction, either
delete and recreate them or close and open the patch; this is done
automatically in Max/MSP.

In Pd, if you select "save" while in a subpatch, the parent is saved.
In Max/MSP, if you do this a dialogue box comes up asking if you want to
save the subpatch as a separate file. (if you want to save a subpatch to
a file in Pd, you have to copy and paste the contents to a new document.

In Pd, inlets and outlets are ordinary text objects; in Max/MSP they're
"gui" objects from the palette.

In Max/MSP, if an object's outlet is connected to several destinations,
corresponding messages are always sent in right-to-left screen order. In
Pd, the messages are sent in the order you made the connections in. In
either case, in situations where you care about the order it's
appropriate to use a "trigger" object to specify.

In Pd, there's no "gate"; instead it's "spigot" with the inlets in
the opposite, more natural order.

Switching subsets of the DSP patch on and off is done in completely
different ways in Pd and Max/MSP, and block sizes are handled
differently as well.

Max offers many "GUI" objects such as sliders, dials, VU meters, piano
keyboards, even "bpatchers." Until version 0.34, the only two in Pd
were the number box and graphical arrays. Starting in version 0.34, Pd
incorporates Thomas Musil's GUI objects: sliders, switches, and so on.
(Thanks Thomas!) Beyond this essential collection of GUI objects, it's
unlikely you'll ever find any commonality between the two. Also, as of
0.34, importing and exporting to Max doesn't know about the Musil
objects; I'll try to get that fixed for 0.35.

In Pd there's no "preset" object (I now think it's basically a bad
idea) and you have to use explicit sends and receives to restore values
to number boxes. Then just make a "message" box to re-send the values
you want.

In Macintosh land, instead of getting tabosc4~ and arrays, you get
cycle~ and buffer~. The only gotcha is that you probably can't draw
in buffer~ with the mouse as you can with arrays, but at least it's
possible to make a patch that copies a "table" into a "buffer~".

The "bpatcher" feature in Max has a correlate, "graph on parent"
subpatches, in Pd; however, Pd's version is quite different from
Max's.


[back to table of contents](index.html)


