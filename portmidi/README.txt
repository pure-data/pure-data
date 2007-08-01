README for PortMidi
Roger Dannenberg
6 April 2003
revised May 2004

For Windows, please see also README_WIN.txt and debugging_dlls.txt
in pm_win.

For Linux, please see also README_LINUX.txt in pm_linux.

POINTERS VS DEVICE NUMBERS

When you open a MIDI port, PortMidi allocates a structure to
maintain the state of the open device. Since every device is
also listed in a table, you might think it would be simpler to
use the table index rather than a pointer to identify a device.
This would also help with error checking (it's hard to make
sure a pointer is valid). PortMidi's design parallels that of
PortAudio.

ERROR HANDLING

Error handling turned out to be much more complicated than expected.
PortMidi functions return error codes that the caller can check.
In addition, errors may occur asynchronously due to MIDI input. In
this case, the error code is transferred to the next call to
Pm_Read or Pm_Write. Furthermore, an error can arise during a MIDI THRU
operation that is also invoked as a side effect of polling for input.

Ordinarily, the caller checks for an error code. If the error is
system-dependent, pmHostError is returned and the caller can
call Pm_GetHostErrorText to get a text description of the error.

Host errors are recorded in the system-specific data allocated for
each open MIDI port. However, if an error occurs on open or close,
we cannot store the error with the device because there will be
no device data (assuming PortMidi cleans up after devices that
are not open). For open and close, we will store the host error
in a global variable. The PortMidi is smart enough to look here
first when the user asks for ErrorText.

Because output to a MIDI Thru stream can be invoked as a side-effect
of a MIDI read operation, some errors normally associated with
writing MIDI can be returned from Pm_Read.

DEBUGGING

If you are building a console application for research, we suggest
compiling with the option PM_CHECK_ERRORS. This will insert a
check for error return values at the end of each PortMidi 
function. If an error is encountered, a text message is printed
using printf(), the user is asked to type ENTER, and then exit(-1)
is called to clean up and terminate the program.

You should not use PM_CHECK_ERRORS if printf() does not work
(e.g. this is not a console application under Windows, or there
is no visible console on some other OS), and you should not use
PM_CHECK_ERRORS if you intend to recover from errors rather than
abruptly terminate the program.

The Windows version (and perhaps others) also offers a DEBUG
compile-time option. See README_WIN.txt.
