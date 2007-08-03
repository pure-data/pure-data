File: PortMidi Win32 Readme
Author: Belinda Thom, June 16 2002
Revised by: Roger Dannenberg, June 2002, May 2004

=============================================================================
USING PORTMIDI:
=============================================================================

PortMidi has been created using a DLL because the Win32 MMedia API doesn't 
handle midiInput properly in the debugger. Specifically, it doesn't clean up
after itself if the user (i.e. you, a PortMidi application) hasn't explicitly
closed all open midi input devices. This lack of cleanup can lead to much
pain and agony, including the blue-screen-of-death. This situation becomes
increasingly unacceptable when you are debugging your code, so a portMidi DLL
seemed to be the most elegant solution.

Using Microsoft Visual C++ project files (provided with PortMidi), there
are two configurations of the PortMidi library. The Debug version is 
intended for debugging, especially in a console application. The Debug
version enables some extra error checking and outputs some text as well
as a prompt to type ENTER so that you don't lose any debugging text when
the program exits. You can turn off this extra debugging info by taking
out the compile-time definition for DEBUG. (But leave _DEBUG, which I
think is important for compiling in Debug mode.) This debugging version also
defines PM_CHECK_ERRORS, which forces a check for error return codes from
every call to PortMidi. You can disable this checking (especially if you
want to handle error codes in your own way) by removing PM_CHECK_ERRORS
from the predefined symbols list in the Settings dialog box.

PortMidi is designed to run without a console and should work perfectly 
well within a graphical user interface application. The Release version
is both optimized and lacking the debugging printout code of the Debug
version.

Read the portmidi.h file for PortMidi API details on using the PortMidi API.
See <...>\pm_dll_test\test.c or <...>\multithread\test.c for usage examples.

=============================================================================
TO INSTALL PORTMIDI:
=============================================================================
1)  download portmidi.zip

2)  unzip portmidi.zip into directory: <...>\portmidi

=============================================================================
TO COMPILE PORTMIDI:
=============================================================================

3)  cd to or open the portmidi directory

4)  start or click on the portmidi.dsw workspace

5)  the following projects exist within this workspace:
    - portmidi (the PortMidi library)
	- pm_dll (the dll library used to close midi ports on program exit)
	- porttime (a small portable library implementing timer facilities)
	- test (simple midi I/O testing)
	- multithread (an example illustrating low-latency MIDI processing
            using a dedicated low-latency thread)
	- sysex (simple sysex message I/O testing)
	- latency (uses porttime to measure system latency)

6)  verify that all project settings are for Win32 Debug release:
	- type Alt-F7
	- highlight all three projects in left part of Project Settings window; 
	- "Settings For" should say "Win32 Debug"

7)  use Build->Batch Build ... to build everything in the project

8)  The settings for these projects were distributed in the zip file, so
    compile should just work.

9)  IMPORTANT! PortMidi uses a DLL, pm_dll.dll, but there is no simple way
    to set up projects to use pm_dll. THEREFORE, you need to copy DLLs
    as follows (you can do this with <...>\portmidi\pm_win\copy-dll.bat):
        copy <...>\portmidi\pm_win\Debug\pm_dll.dll to:
            <...>\portmidi\pm_test\latencyDebug\pm_dll.dll
            <...>\portmidi\pm_test\midithreadDebug\pm_dll.dll
            <...>\portmidi\pm_test\sysexDebug\pm_dll.dll
            <...>\portmidi\pm_test\testDebug\pm_dll.dll
            <...>\portmidi\pm_test\midithruDebug\pm_dll.dll
        and copy <...>\portmidi\pm_win\Release\pm_dll.dll to:
            <...>\portmidi\pm_test\latencyRelease\pm_dll.dll
            <...>\portmidi\pm_test\midithreadRelease\pm_dll.dll
            <...>\portmidi\pm_test\sysexRelease\pm_dll.dll
            <...>\portmidi\pm_test\testRelease\pm_dll.dll
            <...>\portmidi\pm_test\midithruRelease\pm_dll.dll
    each time you rebuild the pm_dll project, these copies must be redone!

    Since Windows will look in the executable directory for DLLs, we 
    recommend that you always install a copy of pm_dll.dll (either the
    debug version or the release version) in the same directory as the
    application using PortMidi. The release DLL is about 40KB. This will 
    ensure that the application uses the correct DLL.

10) run test project; use the menu that shows up from the command prompt to
    test that portMidi works on your system. tests include: 
		- verify midi output works
		- verify midi input works
		- verify midi input w/midi thru works

11) run other projects if you wish: sysex, latency, midithread, mm, qtest

============================================================================
TO CREATE YOUR OWN PORTMIDI CLIENT APPLICATION:
============================================================================

NOTE: this section needs to be reviewed and tested. My suggestion would
be to copy the test project file (test.dsp) and modify it. -RBD

The easiest way is to start a new project w/in the portMidi workspace:

1) To open new project: 
	- File->New->Projects
	- Location: <...>\portmidi\<yourProjectName>
	- check Add to current workspace
	- select Win32 Console Application (recommended for now)
	- do *NOT* select the "make dependency" box (you will explicitly do this
      in the next step)
	- Click OK
	- Select "An Empty Project" and click Finish

2) Now this project will be the active project. Make it explicitly depend
   on PortMidi dll:
	- Project->Dependencies
	- Click pm_dll

3) Important! in order to be able to use portMidi DLL from your new project
   and set breakpoints,	copy following files from <...>\pm_dll\Debug into 
   <...>\<yourProjectName>\Debug directory:
		pm_dll.lib
		pm_dll.dll
    each time you rebuild pm_dll, these copies must be redone!

4) add whatever files you wish to add to your new project, using portMidi
   calls as desired (see USING PORTMIDI at top of this readme)

5) when you include portMidi files, do so like this:
	- #include "..\pm_dll\portmidi.h"
	- etc.

6) build and run your project

============================================================================
DESIGN NOTES
============================================================================

The DLL is used so that PortMidi can (usually) close open devices when the
program terminates. Failure to close input devices under WinNT, Win2K, and
probably later systems causes the OS to crash.

This is accomplished with a .LIB/.DLL pair, linking to the .LIB
in order to access functions in the .DLL. 

PortMidi for Win32 exists as a simple library,
with Win32-specific code in pmwin.c and MM-specific code in pmwinmm.c.
pmwin.c uses a DLL in pmdll.c to call Pm_Terminate() when the program
exits to make sure that all MIDI ports are closed.

Orderly cleanup after errors are encountered is based on a fixed order of
steps and state changes to reflect each step. Here's the order:

To open input:
    initialize return value to NULL
    - allocate the PmInternal strucure (representation of PortMidiStream)
    return value is (non-null) PmInternal structure
    - allocate midi buffer
    set buffer field of PmInternal structure
    - call system-dependent open code
        - allocate midiwinmm_type for winmm dependent data
        set descriptor field of PmInternal structure
        - open device
        set handle field of midiwinmm_type structure
        - allocate buffer 1 for sysex
        buffer is added to input port
        - allocate buffer 2 for sysex
        buffer is added to input port
        - return
    - return

SYSEX HANDLING -- the most complex, least exercised, and therefore most
      buggy part of PortMidi (but maybe bugs are finally gone)

There are three cases: simple output, stream output, input
Each must deal with:
 1. Buffer Initialization (creating buffers)
 2. Buffer Allocation (finding a free buffer)
 3. Buffer Fill (putting bytes in the buffer)
 4. Buffer Preparation (midiOutPrepare, etc.)
 5. Buffer Send (to Midi device)
 6. Buffer Receive (in callback)
 7. Buffer Empty (removing bytes from buffer)
 8. Buffer Free (returning to the buffer pool)
 9. Buffer Finalization (returning to heap)

Here's how simple output handles sysex:
 1. Buffer Initialization (creating buffers)
  allocated when code tries to write first byte to a buffer
  the test is "if (!m->sysex_buffers[0]) { ... }"
  this field is initialized to NULL when device is opened
  the size is SYSEX_BYTES_PER_BUFFER
  allocate_sysex_buffers() does the initialization
  note that the actual size of the allocation includes
      additional space for a MIDIEVENT (3 longs) which are
      not used in this case
 2. Buffer Allocation (finding a free buffer)
  see get_free_sysex_buffer()
  cycle through m->sysex_buffers[] using m->next_sysex_buffer
      to determine where to look next
  if nothing is found, wait by blocking on m->sysex_buffer_signal
  this is signaled by the callback every time a message is
      received
 3. Buffer Fill (putting bytes in the buffer)
  essentially a state machine approach
  hdr->dwBytesRecorded is a position in message pointed to by m->hdr
  keep appending bytes until dwBytesRecorded >= SYSEX_BYTES_PER_BUFFER
  then send the message, reseting the state to initial values
 4. Buffer Preparation (midiOutPrepare, etc.)
  just before sending in winmm_end_sysex()
 5. Buffer Send (to Midi device)
  message is padded with zero at end (since extra space was allocated
      this is ok) -- the zero works around a bug in (an old version of)
      MIDI YOKE drivers
  dwBufferLength gets dwBytesRecorded, and dwBytesRecorded gets 0
  uses midiOutLongMsg()
 6. Buffer Receive (in callback)
 7. Buffer Empty (removing bytes from buffer)
  not applicable for output
 8. Buffer Free (returning to the buffer pool)
  unprepare message to indicate that it is free
  SetEvent on m->buffer_signal in case client is waiting
 9. Buffer Finalization (returning to heap)
  when device is closed, winmm_out_delete frees all sysex buffers

Here's how stream output handles sysex:
 1. Buffer Initialization (creating buffers)
  same code as simple output (see above)
 2. Buffer Allocation (finding a free buffer)
  same code as simple output (see above)
 3. Buffer Fill (putting bytes in the buffer)
  essentially a state machine approach
  m->dwBytesRecorded is a position in message
  keep appending bytes until buffer is full (one byte to spare)
 4. Buffer Preparation (midiOutPrepare, etc.)
  done before sending message
  dwBytesRecorded and dwBufferLength are set in winmm_end_sysex
 5. Buffer Send (to Midi device)
  uses midiStreamOutMsg()
 6. Buffer Receive (in callback)
 7. Buffer Empty (removing bytes from buffer)
  not applicable for output
 8. Buffer Free (returning to the buffer pool)
  unprepare message to indicate that it is free
  SetEvent on m->buffer_signal in case client is waiting
 9. Buffer Finalization (returning to heap)
  when device is closed, winmm_out_delete frees all sysex buffers


Here's how input handles sysex:
 1. Buffer Initialization (creating buffers)
  two buffers are allocated in winmm_in_open
 2. Buffer Allocation (finding a free buffer)
  same code as simple output (see above)
 3. Buffer Fill (putting bytes in the buffer)
  not applicable for input
 4. Buffer Preparation (midiOutPrepare, etc.)
  done before sending message -- in winmm_in_open and in callback
 5. Buffer Send (to Midi device)
  uses midiInAddbuffer in allocate_sysex_input_buffer (called from
      winmm_in_open) and callback
 6. Buffer Receive (in callback)
 7. Buffer Empty (removing bytes from buffer)
      done without pause in loop in callback
 8. Buffer Free (returning to the buffer pool)
  done by midiInAddBuffer in callback, no pointer to buffers
      is retained except by device
 9. Buffer Finalization (returning to heap)
  when device is closed, empty buffers are delivered to callback,
      which frees them

IMPORTANT: In addition to the above, PortMidi now has
"shortcuts" to optimize the transfer of sysex data. To enable
the optimization for sysex output, the system-dependent code
sets fields in the pmInternal structure: fill_base, fill_offset_ptr,
and fill_length. When fill_base is non-null, the system-independent
part of PortMidi is allowed to directly copy sysex bytes to
"fill_base[*fill_offset_ptr++]" until *fill_offset_ptr reaches
fill_length. See the code for details.


  