#ifndef PORT_MIDI_H
#define PORT_MIDI_H
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * PortMidi Portable Real-Time MIDI Library
 * PortMidi API Header File
 * Latest version available at: http://www.cs.cmu.edu/~music/portmidi/
 *
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 * Copyright (c) 2001 Roger B. Dannenberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/* CHANGELOG FOR PORTMIDI
 *
 * 15Nov04 Ben Allison
 *  - sysex output now uses one buffer/message and reallocates buffer
 *  -    if needed
 *  - filters expanded for many message types and channels
 *  - detailed changes are as follows:
 *  ------------- in pmwinmm.c --------------
 *  - new #define symbol: OUTPUT_BYTES_PER_BUFFER
 *  - change SYSEX_BYTES_PER_BUFFER to 1024
 *  - added MIDIHDR_BUFFER_LENGTH(x) to correctly count midihdr buffer length
 *  - change MIDIHDR_SIZE(x) to (MIDIHDR_BUFFER_LENGTH(x) + sizeof(MIDIHDR))
 *  - change allocate_buffer to use new MIDIHDR_BUFFER_LENGTH macro
 *  - new macros for MIDIHDR_SYSEX_SIZE and MIDIHDR_SYSEX_BUFFER_LENGTH 
 *  -    similar to above, but counts appropriately for sysex messages
 *  - added the following members to midiwinmm_struct for sysex data:
 *  -    LPMIDIHDR *sysex_buffers;   ** pool of buffers for sysex data **
 *  -    int num_sysex_buffers;      ** how many sysex buffers **
 *  -    int next_sysex_buffer;      ** index of next sysexbuffer to send **
 *  -    HANDLE sysex_buffer_signal; ** to wait for free sysex buffer **
 *  - duplicated allocate_buffer, alocate_buffers and get_free_output_buffer 
 *  -    into equivalent sysex_buffer form
 *  - changed winmm_in_open to initialize new midiwinmm_struct members and 
 *  -    to use the new allocate_sysex_buffer() function instead of 
 *  -    allocate_buffer()
 *  - changed winmm_out_open to initialize new members, create sysex buffer 
 *  -    signal, and allocate 2 sysex buffers
 *  - changed winmm_out_delete to free sysex buffers and shut down the sysex
 *  -    buffer signal
 *  - create new function resize_sysex_buffer which resizes m->hdr to the 
 *  -    passed size, and corrects the midiwinmm_struct accordingly.
 *  - changed winmm_write_byte to use new resize_sysex_buffer function,
 *  -    if resize fails, write current buffer to output and continue
 *  - changed winmm_out_callback to use buffer_signal or sysex_buffer_signal
 *  -    depending on which buffer was finished
 *  ------------- in portmidi.h --------------
 *  - added pmBufferMaxSize to PmError to indicate that the buffer would be
 *  -    too large for the underlying API
 *  - added additional filters
 *  - added prototype, documentation, and helper macro for Pm_SetChannelMask
 *  ------------- in portmidi.c --------------
 *  - added pm_status_filtered() and pm_realtime_filtered() functions to
 *       separate filtering logic from buffer logic in pm_read_short
 *  - added Pm_SetChannelMask function
 *  - added pm_channel_filtered() function
 *  ------------- in pminternal.h --------------
 *  - added member to PortMidiStream for channel mask
 *
 * 25May04 RBD
 *  - removed support for MIDI THRU
 *  - moved filtering from Pm_Read to pm_enqueue to avoid buffer ovfl
 *  - extensive work on Mac OS X port, especially sysex and error handling
 *
 * 18May04 RBD
 *  - removed side-effects from assert() calls. Now you can disable assert().
 *  - no longer check pm_hosterror everywhere, fixing a bug where an open
 *    failure could cause a write not to work on a previously opened port
 *    until you call Pm_GetHostErrorText().
 * 16May04 RBD and Chris Roberts
 *  - Some documentation wordsmithing in portmidi.h
 *  - Dynamically allocate port descriptor structures
 *  - Fixed parameter error in midiInPrepareBuffer and midiInAddBuffer.
 *
 * 09Oct03 RBD
 *  - Changed Thru handling. Now the client does all the work and the client
 *    must poll or read to keep thru messages flowing.
 *
 * 31May03 RBD
 *  - Fixed various bugs.
 *  - Added linux ALSA support with help from Clemens Ladisch
 *  - Added Mac OS X support, implemented by Jon Parise, updated and 
 *       integrated by Andrew Zeldis and Zico Kolter
 *  - Added latency program to build histogram of system latency using PortTime.
 *
 * 30Jun02 RBD Extensive rewrite of sysex handling. It works now.
 *             Extensive reworking of error reporting and error text -- no
 *             longer use dictionary call to delete data; instead, Pm_Open
 *             and Pm_Close clean up before returning an error code, and
 *             error text is saved in a system-independent location.
 *             Wrote sysex.c to test sysex message handling.
 *
 * 15Jun02 BCT changes:
 *  - Added pmHostError text handling.
 *  - For robustness, check PortMidi stream args not NULL.
 *  - Re-C-ANSI-fied code (changed many C++ comments to C style)
 *  - Reorganized code in pmwinmm according to input/output functionality (made
 *    cleanup handling easier to reason about)
 *  - Fixed Pm_Write calls (portmidi.h says these should not return length but Pm_Error)
 *  - Cleaned up memory handling (now system specific data deleted via dictionary
 *    call in PortMidi, allows client to query host errors).
 *  - Added explicit asserts to verify various aspects of pmwinmm implementation behaves as
 *    logic implies it should. Specifically: verified callback routines not reentrant and
 *    all verified status for all unchecked Win32 MMedia API calls perform successfully
 *  - Moved portmidi initialization and clean-up routines into DLL to fix Win32 MMedia API
 *    bug (i.e. if devices not explicitly closed, must reboot to debug application further).
 *    With this change, clients no longer need explicitly call Pm_Initialize, Pm_Terminate, or
 *    explicitly Pm_Close open devices when using WinMM version of PortMidi.
 *
 * 23Jan02 RBD Fixed bug in pmwinmm.c thru handling
 *
 * 21Jan02 RBD Added tests in Pm_OpenInput() and Pm_OpenOutput() to prevent
 *               opening an input as output and vice versa.
 *             Added comments and documentation.
 *             Implemented Pm_Terminate().
 *
 *
 * IMPORTANT INFORMATION ABOUT A WIN32 BUG:
 *
 *    Windows apparently has a serious midi bug -- if you do not close ports, Windows
 *    may crash. PortMidi tries to protect against this by using a DLL to clean up.
 *
 *    If client exits for example with:
 *      i)  assert
 *      ii) Ctrl^c,
 *    then DLL clean-up routine called. However, when client does something
 *    really bad (e.g. assigns value to NULL pointer) then DLL CLEANUP ROUTINE
 *    NEVER RUNS! In this state, if you wait around long enough, you will
 *    probably get the blue screen of death. Can also go into Pview and there will
 *    exist zombie process that you can't kill.
 *
 * NOTES ON HOST ERROR REPORTING:
 *
 *    PortMidi errors (of type PmError) are generic, system-independent errors.
 *    When an error does not map to one of the more specific PmErrors, the
 *    catch-all code pmHostError is returned. This means that PortMidi has
 *    retained a more specific system-dependent error code. The caller can
 *    get more information by calling Pm_HasHostError() to test if there is
 *    a pending host error, and Pm_GetHostErrorText() to get a text string
 *    describing the error. Host errors are reported on a per-device basis
 *    because only after you open a device does PortMidi have a place to
 *    record the host error code. I.e. only
 *    those routines that receive a (PortMidiStream *) argument check and
 *    report errors. One exception to this is that Pm_OpenInput() and
 *    Pm_OpenOutput() can report errors even though when an error occurs,
 *    there is no PortMidiStream* to hold the error. Fortunately, both
 *    of these functions return any error immediately, so we do not really
 *    need per-device error memory. Instead, any host error code is stored
 *    in a global, pmHostError is returned, and the user can call
 *    Pm_GetHostErrorText() to get the error message (and the invalid stream
 *    parameter will be ignored.) The functions
 *    pm_init and pm_term do not fail or raise
 *    errors. The job of pm_init is to locate all available devices so that
 *    the caller can get information via PmDeviceInfo(). If an error occurs,
 *    the device is simply not listed as available.
 *
 *    Host errors come in two flavors:
 *      a) host error
 *      b) host error during callback
 *    These can occur w/midi input or output devices. (b) can only happen
 *    asynchronously (during callback routines), whereas (a) only occurs while
 *    synchronously running PortMidi and any resulting system dependent calls
 *
 *    Host-error reporting relies on following assumptions:
 *      1) PortMidi routines won't allow system dependent routines to be
 *         called when args are bogus.
 *         Thus, in pmwinmm.c it is safe to assume:
 *          - stream ptr valid
 *          - currently not operating in "has host error" state
 *      2) Host-error reporting relies on a staged delivery of error messages.
 *         When a host error occurs, the error code is saved with the stream.
 *         The error is reported as a return code from the next operation on
 *         the stream. This could be immediately if the error is synchronous,
 *         or delayed if the error is an asynchronous callback problem. In
 *         any case, when pmHostError is returned, the error is copied to
 *         a global, pm_hosterror and the error code stored with the stream
 *         is cleared. If the user chooses to inquire about the error using
 *         Pm_GetHostErrorText(), the error will be reported as text. If the
 *         user ignores the error and makes another call on the stream, the
 *         call will proceed because the error code associated with the stream
 *         has been cleared.
 *
 */

#ifndef FALSE
    #define FALSE 0
#endif
#ifndef TRUE
    #define TRUE 1
#endif

/* default size of buffers for sysex transmission: */
#define PM_DEFAULT_SYSEX_BUFFER_SIZE 1024


typedef enum {
    pmNoError = 0,
    pmHostError = -10000,
    pmInvalidDeviceId, /* out of range or output device when input is requested or vice versa */
    pmInsufficientMemory,
    pmBufferTooSmall,
    pmBufferOverflow,
    pmBadPtr,
    pmBadData, /* illegal midi data, e.g. missing EOX */
    pmInternalError,
    pmBufferMaxSize, /* buffer is already as large as it can be */
} PmError;

/*
    Pm_Initialize() is the library initialisation function - call this before
    using the library.
*/

PmError Pm_Initialize( void );

/*
    Pm_Terminate() is the library termination function - call this after
    using the library.
*/

PmError Pm_Terminate( void );

/*  A single PortMidiStream is a descriptor for an open MIDI device.
*/
typedef void PortMidiStream;
#define PmStream PortMidiStream

/*
    Test whether stream has a pending host error. Normally, the client finds
	out about errors through returned error codes, but some errors can occur
	asynchronously where the client does not
	explicitly call a function, and therefore cannot receive an error code.
	The client can test for a pending error using Pm_HasHostError(). If true,
	the error can be accessed and cleared by calling Pm_GetErrorText(). The
	client does not need to call Pm_HasHostError(). Any pending error will be
	reported the next time the client performs an explicit function call on
	the stream, e.g. an input or output operation.
*/
int Pm_HasHostError( PortMidiStream * stream );


/*  Translate portmidi error number into human readable message.
    These strings are constants (set at compile time) so client has
	no need to allocate storage
*/
const char *Pm_GetErrorText( PmError errnum );

/*  Translate portmidi host error into human readable message.
    These strings are computed at run time, so client has to allocate storage.
	After this routine executes, the host error is cleared.
*/
void Pm_GetHostErrorText(char * msg, unsigned int len);

#define HDRLENGTH 50
#define PM_HOST_ERROR_MSG_LEN 256u /* any host error msg will occupy less
                                      than this number of characters */

/*
    Device enumeration mechanism.

    Device ids range from 0 to Pm_CountDevices()-1.

*/
typedef int PmDeviceID;
#define pmNoDevice -1
typedef struct {
    int structVersion;
    const char *interf; /* underlying MIDI API, e.g. MMSystem or DirectX */
    const char *name;   /* device name, e.g. USB MidiSport 1x1 */
    int input; /* true iff input is available */
    int output; /* true iff output is available */
    int opened; /* used by generic PortMidi code to do error checking on arguments */

} PmDeviceInfo;


int Pm_CountDevices( void );
/*
    Pm_GetDefaultInputDeviceID(), Pm_GetDefaultOutputDeviceID()

    Return the default device ID or pmNoDevice if there are no devices.
    The result can be passed to Pm_OpenMidi().

    On the PC, the user can specify a default device by
    setting an environment variable. For example, to use device #1.

        set PM_RECOMMENDED_OUTPUT_DEVICE=1

    The user should first determine the available device ID by using
    the supplied application "testin" or "testout".

    In general, the registry is a better place for this kind of info,
    and with USB devices that can come and go, using integers is not
    very reliable for device identification. Under Windows, if
    PM_RECOMMENDED_OUTPUT_DEVICE (or PM_RECOMMENDED_INPUT_DEVICE) is
    *NOT* found in the environment, then the default device is obtained
    by looking for a string in the registry under:
        HKEY_LOCAL_MACHINE/SOFTWARE/PortMidi/Recommended_Input_Device
    and HKEY_LOCAL_MACHINE/SOFTWARE/PortMidi/Recommended_Output_Device
    for a string. The number of the first device with a substring that
    matches the string exactly is returned. For example, if the string
    in the registry is "USB", and device 1 is named
    "In USB MidiSport 1x1", then that will be the default
    input because it contains the string "USB".

    In addition to the name, PmDeviceInfo has the member "interf", which
    is the interface name. (The "interface" is the underlying software
	system or API used by PortMidi to access devices. Examples are
	MMSystem, DirectX (not implemented), ALSA, OSS (not implemented), etc.)
	At present, the only Win32 interface is "MMSystem", the only Linux
	interface is "ALSA", and the only Max OS X interface is "CoreMIDI".
    To specify both the interface and the device name in the registry,
    separate the two with a comma and a space, e.g.:
        MMSystem, In USB MidiSport 1x1
    In this case, the string before the comma must be a substring of
    the "interf" string, and the string after the space must be a
    substring of the "name" name string in order to match the device.

    Note: in the current release, the default is simply the first device
	(the input or output device with the lowest PmDeviceID).
*/
PmDeviceID Pm_GetDefaultInputDeviceID( void );
PmDeviceID Pm_GetDefaultOutputDeviceID( void );

/*
    PmTimestamp is used to represent a millisecond clock with arbitrary
    start time. The type is used for all MIDI timestampes and clocks.
*/
typedef long PmTimestamp;
typedef PmTimestamp (*PmTimeProcPtr)(void *time_info);

/* TRUE if t1 before t2 */
#define PmBefore(t1,t2) ((t1-t2) < 0)

/*
    Pm_GetDeviceInfo() returns a pointer to a PmDeviceInfo structure
    referring to the device specified by id.
    If id is out of range the function returns NULL.

    The returned structure is owned by the PortMidi implementation and must
    not be manipulated or freed. The pointer is guaranteed to be valid
    between calls to Pm_Initialize() and Pm_Terminate().
*/
const PmDeviceInfo* Pm_GetDeviceInfo( PmDeviceID id );

/*
    Pm_OpenInput() and Pm_OpenOutput() open devices.

    stream is the address of a PortMidiStream pointer which will receive
    a pointer to the newly opened stream.

    inputDevice is the id of the device used for input (see PmDeviceID above).

    inputDriverInfo is a pointer to an optional driver specific data structure
    containing additional information for device setup or handle processing.
    inputDriverInfo is never required for correct operation. If not used
    inputDriverInfo should be NULL.

    outputDevice is the id of the device used for output (see PmDeviceID above.)

    outputDriverInfo is a pointer to an optional driver specific data structure
    containing additional information for device setup or handle processing.
    outputDriverInfo is never required for correct operation. If not used
    outputDriverInfo should be NULL.

    For input, the buffersize specifies the number of input events to be
    buffered waiting to be read using Pm_Read(). For output, buffersize
    specifies the number of output events to be buffered waiting for output.
    (In some cases -- see below -- PortMidi does not buffer output at all
	and merely passes data to a lower-level API, in which case buffersize
	is ignored.)

    latency is the delay in milliseconds applied to timestamps to determine
    when the output should actually occur. (If latency is < 0, 0 is assumed.)
    If latency is zero, timestamps are ignored and all output is delivered
    immediately. If latency is greater than zero, output is delayed until
    the message timestamp plus the latency. (NOTE: time is measured relative
    to the time source indicated by time_proc. Timestamps are absolute, not
    relative delays or offsets.) In some cases, PortMidi can obtain
	better timing than your application by passing timestamps along to the
	device driver or hardware. Latency may also help you to synchronize midi
	data to audio data by matching midi latency to the audio buffer latency.

    time_proc is a pointer to a procedure that returns time in milliseconds. It
    may be NULL, in which case a default millisecond timebase (PortTime) is
    used. If the application wants to use PortTime, it should start the timer
    (call Pt_Start) before calling Pm_OpenInput or Pm_OpenOutput. If the
    application tries to start the timer *after* Pm_OpenInput or Pm_OpenOutput,
    it may get a ptAlreadyStarted error from Pt_Start, and the application's
    preferred time resolution and callback function will be ignored.
    time_proc result values are appended to incoming MIDI data, and time_proc
    times are used to schedule outgoing MIDI data (when latency is non-zero).

    time_info is a pointer passed to time_proc.

    return value:
    Upon success Pm_Open() returns PmNoError and places a pointer to a
    valid PortMidiStream in the stream argument.
    If a call to Pm_Open() fails a nonzero error code is returned (see
    PMError above) and the value of port is invalid.

    Any stream that is successfully opened should eventually be closed
	by calling Pm_Close().

*/
PmError Pm_OpenInput( PortMidiStream** stream,
                PmDeviceID inputDevice,
                void *inputDriverInfo,
                long bufferSize,
                PmTimeProcPtr time_proc,
                void *time_info );

PmError Pm_OpenOutput( PortMidiStream** stream,
                PmDeviceID outputDevice,
                void *outputDriverInfo,
                long bufferSize,
                PmTimeProcPtr time_proc,
                void *time_info,
                long latency );

/*
    Pm_SetFilter() sets filters on an open input stream to drop selected
    input types. By default, only active sensing messages are filtered.
    To prohibit, say, active sensing and sysex messages, call
    Pm_SetFilter(stream, PM_FILT_ACTIVE | PM_FILT_SYSEX);

    Filtering is useful when midi routing or midi thru functionality is being
    provided by the user application.
    For example, you may want to exclude timing messages (clock, MTC, start/stop/continue),
    while allowing note-related messages to pass.
    Or you may be using a sequencer or drum-machine for MIDI clock information but want to
    exclude any notes it may play.
 */

/* filter active sensing messages (0xFE): */
#define PM_FILT_ACTIVE 0x1
/* filter system exclusive messages (0xF0): */
#define PM_FILT_SYSEX 0x2
/* filter clock messages (0xF8 only, does not filter clock start, etc.): */
#define PM_FILT_CLOCK 0x4
/* filter play messages (start 0xFA, stop 0xFC, continue 0xFB) */
#define PM_FILT_PLAY 0x8
/* filter undefined F9 messages (some equipment uses this as a 10ms 'tick') */
#define PM_FILT_F9 0x10
#define PM_FILT_TICK PM_FILT_F9
/* filter undefined FD messages */
#define PM_FILT_FD 0x20
/* filter undefined real-time messages */
#define PM_FILT_UNDEFINED (PM_FILT_F9 | PM_FILT_FD)
/* filter reset messages (0xFF) */
#define PM_FILT_RESET 0x40
/* filter all real-time messages */
#define PM_FILT_REALTIME (PM_FILT_ACTIVE | PM_FILT_SYSEX | PM_FILT_CLOCK | PM_FILT_PLAY | PM_FILT_UNDEFINED | PM_FILT_RESET)
/* filter note-on and note-off (0x90-0x9F and 0x80-0x8F */
#define PM_FILT_NOTE 0x80
/* filter channel aftertouch (most midi controllers use this) (0xD0-0xDF)*/
#define PM_FILT_CHANNEL_AFTERTOUCH 0x100
/* per-note aftertouch (Ensoniq holds a patent on generating this on keyboards until June 2006) (0xA0-0xAF) */
#define PM_FILT_POLY_AFTERTOUCH 0x200
/* filter both channel and poly aftertouch */
#define PM_FILT_AFTERTOUCH (PM_FILT_CHANNEL_AFTERTOUCH | PM_FILT_POLY_AFTERTOUCH)
/* Program changes (0xC0-0xCF) */
#define PM_FILT_PROGRAM 0x400
/* Control Changes (CC's) (0xB0-0xBF)*/
#define PM_FILT_CONTROL 0x800
/* Pitch Bender (0xE0-0xEF*/
#define PM_FILT_PITCHBEND 0x1000
/* MIDI Time Code (0xF1)*/
#define PM_FILT_MTC 0x2000
/* Song Position (0xF2) */
#define PM_FILT_SONG_POSITION 0x4000
/* Song Select (0xF3)*/
#define PM_FILT_SONG_SELECT 0x8000
/* Tuning request (0xF6)*/
#define PM_FILT_TUNE 0x10000
/* All System Common messages (mtc, song position, song select, tune request) */
#define PM_FILT_SYSTEMCOMMON (PM_FILT_MTC | PM_FILT_SONG_POSITION | PM_FILT_SONG_SELECT | PM_FILT_TUNE)


PmError Pm_SetFilter( PortMidiStream* stream, long filters );


/*
    Pm_SetChannelMask() filters incoming messages based on channel.
    The mask is a 16-bit bitfield corresponding to appropriate channels
    The Pm_Channel macro can assist in calling this function.
    i.e. to set receive only input on channel 1, call with
    Pm_SetChannelMask(Pm_Channel(1));
    Multiple channels should be OR'd together, like
    Pm_SetChannelMask(Pm_Channel(10) | Pm_Channel(11))

    All channels are allowed by default
*/
#define Pm_Channel(channel) (1<<(channel))

PmError Pm_SetChannelMask(PortMidiStream *stream, int mask);

/*
    Pm_Abort() terminates outgoing messages immediately
    The caller should immediately close the output port;
    this call may result in transmission of a partial midi message.
    There is no abort for Midi input because the user can simply
    ignore messages in the buffer and close an input device at
    any time.
 */
PmError Pm_Abort( PortMidiStream* stream );

/*
    Pm_Close() closes a midi stream, flushing any pending buffers.
	(PortMidi attempts to close open streams when the application
	exits -- this is particularly difficult under Windows.)
*/
PmError Pm_Close( PortMidiStream* stream );

/*
    Pm_Message() encodes a short Midi message into a long word. If data1
    and/or data2 are not present, use zero.

    Pm_MessageStatus(), Pm_MessageData1(), and
    Pm_MessageData2() extract fields from a long-encoded midi message.
*/
#define Pm_Message(status, data1, data2) \
         ((((data2) << 16) & 0xFF0000) | \
          (((data1) << 8) & 0xFF00) | \
          ((status) & 0xFF))
#define Pm_MessageStatus(msg) ((msg) & 0xFF)
#define Pm_MessageData1(msg) (((msg) >> 8) & 0xFF)
#define Pm_MessageData2(msg) (((msg) >> 16) & 0xFF)

/* All midi data comes in the form of PmEvent structures. A sysex
   message is encoded as a sequence of PmEvent structures, with each
   structure carrying 4 bytes of the message, i.e. only the first
   PmEvent carries the status byte.

   Note that MIDI allows nested messages: the so-called "real-time" MIDI
   messages can be inserted into the MIDI byte stream at any location,
   including within a sysex message. MIDI real-time messages are one-byte
   messages used mainly for timing (see the MIDI spec). PortMidi retains
   the order of non-real-time MIDI messages on both input and output, but
   it does not specify exactly how real-time messages are processed. This
   is particulary problematic for MIDI input, because the input parser
   must either prepare to buffer an unlimited number of sysex message
   bytes or to buffer an unlimited number of real-time messages that
   arrive embedded in a long sysex message. To simplify things, the input
   parser is allowed to pass real-time MIDI messages embedded within a
   sysex message, and it is up to the client to detect, process, and
   remove these messages as they arrive.

   When receiving sysex messages, the sysex message is terminated
   by either an EOX status byte (anywhere in the 4 byte messages) or
   by a non-real-time status byte in the low order byte of the message.
   If you get a non-real-time status byte but there was no EOX byte, it
   means the sysex message was somehow truncated. This is not
   considered an error; e.g., a missing EOX can result from the user
   disconnecting a MIDI cable during sysex transmission.

   A real-time message can occur within a sysex message. A real-time
   message will always occupy a full PmEvent with the status byte in
   the low-order byte of the PmEvent message field. (This implies that
   the byte-order of sysex bytes and real-time message bytes may not
   be preserved -- for example, if a real-time message arrives after
   3 bytes of a sysex message, the real-time message will be delivered
   first. The first word of the sysex message will be delivered only
   after the 4th byte arrives, filling the 4-byte PmEvent message field.

   The timestamp field is observed when the output port is opened with
   a non-zero latency. A timestamp of zero means "use the current time",
   which in turn means to deliver the message with a delay of
   latency (the latency parameter used when opening the output port.)
   Do not expect PortMidi to sort data according to timestamps --
   messages should be sent in the correct order, and timestamps MUST
   be non-decreasing.

   A sysex message will generally fill many PmEvent structures. On
   output to a PortMidiStream with non-zero latency, the first timestamp
   on sysex message data will determine the time to begin sending the
   message. PortMidi implementations may ignore timestamps for the
   remainder of the sysex message.

   On input, the timestamp ideally denotes the arrival time of the
   status byte of the message. The first timestamp on sysex message
   data will be valid. Subsequent timestamps may denote
   when message bytes were actually received, or they may be simply
   copies of the first timestamp.

   Timestamps for nested messages: If a real-time message arrives in
   the middle of some other message, it is enqueued immediately with
   the timestamp corresponding to its arrival time. The interrupted
   non-real-time message or 4-byte packet of sysex data will be enqueued
   later. The timestamp of interrupted data will be equal to that of
   the interrupting real-time message to insure that timestamps are
   non-decreasing.
 */
typedef long PmMessage;
typedef struct {
    PmMessage      message;
    PmTimestamp    timestamp;
} PmEvent;

/*
    Pm_Read() retrieves midi data into a buffer, and returns the number
    of events read. Result is a non-negative number unless an error occurs,
    in which case a PmError value will be returned.

    Buffer Overflow

    The problem: if an input overflow occurs, data will be lost, ultimately
    because there is no flow control all the way back to the data source.
    When data is lost, the receiver should be notified and some sort of
    graceful recovery should take place, e.g. you shouldn't resume receiving
    in the middle of a long sysex message.

    With a lock-free fifo, which is pretty much what we're stuck with to
    enable portability to the Mac, it's tricky for the producer and consumer
    to synchronously reset the buffer and resume normal operation.

    Solution: the buffer managed by PortMidi will be flushed when an overflow
    occurs. The consumer (Pm_Read()) gets an error message (pmBufferOverflow)
    and ordinary processing resumes as soon as a new message arrives. The
    remainder of a partial sysex message is not considered to be a "new
    message" and will be flushed as well.

*/
PmError Pm_Read( PortMidiStream *stream, PmEvent *buffer, long length );

/*
    Pm_Poll() tests whether input is available,
    returning TRUE, FALSE, or an error value.
*/
PmError Pm_Poll( PortMidiStream *stream);

/*
    Pm_Write() writes midi data from a buffer. This may contain:
        - short messages
    or
        - sysex messages that are converted into a sequence of PmEvent
          structures, e.g. sending data from a file or forwarding them
          from midi input.

    Use Pm_WriteSysEx() to write a sysex message stored as a contiguous
    array of bytes.

    Sysex data may contain embedded real-time messages.
*/
PmError Pm_Write( PortMidiStream *stream, PmEvent *buffer, long length );

/*
    Pm_WriteShort() writes a timestamped non-system-exclusive midi message.
	Messages are delivered in order as received, and timestamps must be
	non-decreasing. (But timestamps are ignored if the stream was opened
	with latency = 0.)
*/
PmError Pm_WriteShort( PortMidiStream *stream, PmTimestamp when, long msg);

/*
    Pm_WriteSysEx() writes a timestamped system-exclusive midi message.
*/
PmError Pm_WriteSysEx( PortMidiStream *stream, PmTimestamp when, unsigned char *msg);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* PORT_MIDI_H */
