/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include "s_stuff.h"
#include <stdio.h>

#include <windows.h>
#include <MMSYSTEM.H>

    /* ------------- MIDI time stamping from audio clock ------------ */

#ifdef MIDI_TIMESTAMP

static double msw_hibuftime;
static double initsystime = -1;

    /* call this whenever we reset audio */
static void msw_resetmidisync(void)
{
    initsystime = clock_getsystime();
    msw_hibuftime = sys_getrealtime();
}

    /* call this whenever we're idled waiting for audio to be ready. 
    The routine maintains a high and low water point for the difference
    between real and DAC time. */

static void msw_midisync(void)
{
    double jittersec, diff;
    
    if (initsystime == -1) msw_resetmidisync();
    jittersec = (msw_dacjitterbufsallowed > msw_adcjitterbufsallowed ?
        msw_dacjitterbufsallowed : msw_adcjitterbufsallowed)
            * REALDACBLKSIZE / sys_getsr();
    diff = sys_getrealtime() - 0.001 * clock_gettimesince(initsystime);
    if (diff > msw_hibuftime) msw_hibuftime = diff;
    if (diff < msw_hibuftime - jittersec)
    {
        post("jitter excess %d %f", dac, diff);
        msw_resetmidisync();
    }
}

static double msw_midigettimefor(LARGE_INTEGER timestamp)
{
    /* this is broken now... used to work when "timestamp" was derived from
        QueryPerformanceCounter() instead of the gates approved 
            timeGetSystemTime() call in the MIDI callback routine below. */
    return (msw_tixtotime(timestamp) - msw_hibuftime);
}
#endif      /* MIDI_TIMESTAMP */


/* ------------------------- MIDI output -------------------------- */
static void msw_midiouterror(char *s, int err)
{
    char t[256];
    midiOutGetErrorText(err, t, 256);
    fprintf(stderr, s, t);
}

static HMIDIOUT hMidiOut[MAXMIDIOUTDEV];    /* output device */
static int msw_nmidiout;                            /* number of devices */

static void msw_open_midiout(int nmidiout, int *midioutvec)
{
    UINT result, wRtn;
    int i;
    int dev;
    MIDIOUTCAPS midioutcaps;
    if (nmidiout > MAXMIDIOUTDEV)
        nmidiout = MAXMIDIOUTDEV;

    dev = 0;

    for (i = 0; i < nmidiout; i++)
    {
        MIDIOUTCAPS mocap;
        int devno =  midioutvec[i];
        result = midiOutOpen(&hMidiOut[dev], devno, 0, 0, 
            CALLBACK_NULL);
        wRtn = midiOutGetDevCaps(i, (LPMIDIOUTCAPS) &mocap,
            sizeof(mocap));
        if (result != MMSYSERR_NOERROR)
        {
            fprintf(stderr,"midiOutOpen: %s\n",midioutcaps.szPname);
            msw_midiouterror("midiOutOpen: %s\n", result);
        }
        else
        {
            if (sys_verbose)
                fprintf(stderr,"midiOutOpen: Open %s as Port %d\n",
                    midioutcaps.szPname, dev);
            dev++;
        }
    }
    msw_nmidiout = dev;
}

static void msw_close_midiout(void)
{
    int i;
    for (i = 0; i < msw_nmidiout; i++)
    {
        midiOutReset(hMidiOut[i]);
        midiOutClose(hMidiOut[i]);
    }
    msw_nmidiout = 0;
}

/* -------------------------- MIDI input ---------------------------- */

#define INPUT_BUFFER_SIZE 1000     // size of input buffer in events

static void msw_midiinerror(char *s, int err)
{
    char t[256];
    midiInGetErrorText(err, t, 256);
    fprintf(stderr, s, t);
}


/* Structure to represent a single MIDI event.
 */

#define EVNT_F_ERROR    0x00000001L

typedef struct evemsw_tag
{
    DWORD fdwEvent;
    DWORD dwDevice;
    LARGE_INTEGER timestamp;
    DWORD data;
} EVENT;
typedef EVENT FAR *LPEVENT;

/* Structure to manage the circular input buffer.
 */
typedef struct circularBuffer_tag
{
    HANDLE  hSelf;          /* handle to this structure */
    HANDLE  hBuffer;        /* buffer handle */
    WORD    wError;         /* error flags */
    DWORD   dwSize;         /* buffer size (in EVENTS) */
    DWORD   dwCount;        /* byte count (in EVENTS) */
    LPEVENT lpStart;        /* ptr to start of buffer */
    LPEVENT lpEnd;          /* ptr to end of buffer (last byte + 1) */
    LPEVENT lpHead;         /* ptr to head (next location to fill) */
    LPEVENT lpTail;         /* ptr to tail (next location to empty) */
} CIRCULARBUFFER;
typedef CIRCULARBUFFER FAR *LPCIRCULARBUFFER;


/* Structure to pass instance data from the application
   to the low-level callback function.
 */
typedef struct callbackInstance_tag
{
    HANDLE              hSelf;  
    DWORD               dwDevice;
    LPCIRCULARBUFFER    lpBuf;
} CALLBACKINSTANCEDATA;
typedef CALLBACKINSTANCEDATA FAR *LPCALLBACKINSTANCEDATA;

/* Function prototypes
 */
LPCALLBACKINSTANCEDATA FAR PASCAL AllocCallbackInstanceData(void);
void FAR PASCAL FreeCallbackInstanceData(LPCALLBACKINSTANCEDATA lpBuf);

LPCIRCULARBUFFER AllocCircularBuffer(DWORD dwSize);
void FreeCircularBuffer(LPCIRCULARBUFFER lpBuf);
WORD FAR PASCAL GetEvent(LPCIRCULARBUFFER lpBuf, LPEVENT lpEvent);

// Callback instance data pointers
LPCALLBACKINSTANCEDATA lpCallbackInstanceData[MAXMIDIINDEV];

UINT wNumDevices = 0;                    // Number of MIDI input devices opened
BOOL bRecordingEnabled = 1;             // Enable/disable recording flag
int  nNumBufferLines = 0;                // Number of lines in display buffer
RECT rectScrollClip;                    // Clipping rectangle for scrolling

LPCIRCULARBUFFER lpInputBuffer;         // Input buffer structure
EVENT incomingEvent;                    // Incoming MIDI event structure

MIDIINCAPS midiInCaps[MAXMIDIINDEV]; // Device capabilities structures
HMIDIIN hMidiIn[MAXMIDIINDEV];       // MIDI input device handles


/* AllocCallbackInstanceData - Allocates a CALLBACKINSTANCEDATA
 *      structure.  This structure is used to pass information to the
 *      low-level callback function, each time it receives a message.
 *
 *      Because this structure is accessed by the low-level callback
 *      function, it must be allocated using GlobalAlloc() with the 
 *      GMEM_SHARE and GMEM_MOVEABLE flags and page-locked with
 *      GlobalPageLock().
 *
 * Params:  void
 *
 * Return:  A pointer to the allocated CALLBACKINSTANCE data structure.
 */
LPCALLBACKINSTANCEDATA FAR PASCAL AllocCallbackInstanceData(void)
{
    HANDLE hMem;
    LPCALLBACKINSTANCEDATA lpBuf;
    
    /* Allocate and lock global memory.
     */
    hMem = GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE,
                       (DWORD)sizeof(CALLBACKINSTANCEDATA));
    if(hMem == NULL)
        return NULL;
    
    lpBuf = (LPCALLBACKINSTANCEDATA)GlobalLock(hMem);
    if(lpBuf == NULL){
        GlobalFree(hMem);
        return NULL;
    }
    
    /* Page lock the memory.
     */
    //GlobalPageLock((HGLOBAL)HIWORD(lpBuf));

    /* Save the handle.
     */
    lpBuf->hSelf = hMem;

    return lpBuf;
}

/* FreeCallbackInstanceData - Frees the given CALLBACKINSTANCEDATA structure.
 *
 * Params:  lpBuf - Points to the CALLBACKINSTANCEDATA structure to be freed.
 *
 * Return:  void
 */
void FAR PASCAL FreeCallbackInstanceData(LPCALLBACKINSTANCEDATA lpBuf)
{
    HANDLE hMem;

    /* Save the handle until we're through here.
     */
    hMem = lpBuf->hSelf;

    /* Free the structure.
     */
    //GlobalPageUnlock((HGLOBAL)HIWORD(lpBuf));
    GlobalUnlock(hMem);
    GlobalFree(hMem);
}


/*
 * AllocCircularBuffer -    Allocates memory for a CIRCULARBUFFER structure 
 * and a buffer of the specified size.  Each memory block is allocated 
 * with GlobalAlloc() using GMEM_SHARE and GMEM_MOVEABLE flags, locked 
 * with GlobalLock(), and page-locked with GlobalPageLock().
 *
 * Params:  dwSize - The size of the buffer, in events.
 *
 * Return:  A pointer to a CIRCULARBUFFER structure identifying the 
 *      allocated display buffer.  NULL if the buffer could not be allocated.
 */
 
 
LPCIRCULARBUFFER AllocCircularBuffer(DWORD dwSize)
{
    HANDLE hMem;
    LPCIRCULARBUFFER lpBuf;
    LPEVENT lpMem;
    
    /* Allocate and lock a CIRCULARBUFFER structure.
     */
    hMem = GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE,
                       (DWORD)sizeof(CIRCULARBUFFER));
    if(hMem == NULL)
        return NULL;

    lpBuf = (LPCIRCULARBUFFER)GlobalLock(hMem);
    if(lpBuf == NULL)
    {
        GlobalFree(hMem);
        return NULL;
    }
    
    /* Page lock the memory.  Global memory blocks accessed by
     * low-level callback functions must be page locked.
     */
#ifndef _WIN32
    GlobalSmartPageLock((HGLOBAL)HIWORD(lpBuf));
#endif

    /* Save the memory handle.
     */
    lpBuf->hSelf = hMem;
    
    /* Allocate and lock memory for the actual buffer.
     */
    hMem = GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE, dwSize * sizeof(EVENT));
    if(hMem == NULL)
    {
#ifndef _WIN32
        GlobalSmartPageUnlock((HGLOBAL)HIWORD(lpBuf));
#endif
        GlobalUnlock(lpBuf->hSelf);
        GlobalFree(lpBuf->hSelf);
        return NULL;
    }
    
    lpMem = (LPEVENT)GlobalLock(hMem);
    if(lpMem == NULL)
    {
        GlobalFree(hMem);
#ifndef _WIN32
        GlobalSmartPageUnlock((HGLOBAL)HIWORD(lpBuf));
#endif
        GlobalUnlock(lpBuf->hSelf);
        GlobalFree(lpBuf->hSelf);
        return NULL;
    }
    
    /* Page lock the memory.  Global memory blocks accessed by
     * low-level callback functions must be page locked.
     */
#ifndef _WIN32
    GlobalSmartPageLock((HGLOBAL)HIWORD(lpMem));
#endif
    
    /* Set up the CIRCULARBUFFER structure.
     */
    lpBuf->hBuffer = hMem;
    lpBuf->wError = 0;
    lpBuf->dwSize = dwSize;
    lpBuf->dwCount = 0L;
    lpBuf->lpStart = lpMem;
    lpBuf->lpEnd = lpMem + dwSize;
    lpBuf->lpTail = lpMem;
    lpBuf->lpHead = lpMem;
        
    return lpBuf;
}

/* FreeCircularBuffer - Frees the memory for the given CIRCULARBUFFER 
 * structure and the memory for the buffer it references.
 *
 * Params:  lpBuf - Points to the CIRCULARBUFFER to be freed.
 *
 * Return:  void
 */
void FreeCircularBuffer(LPCIRCULARBUFFER lpBuf)
{
    HANDLE hMem;
    
    /* Free the buffer itself.
     */
#ifndef _WIN32
    GlobalSmartPageUnlock((HGLOBAL)HIWORD(lpBuf->lpStart));
#endif
    GlobalUnlock(lpBuf->hBuffer);
    GlobalFree(lpBuf->hBuffer);
    
    /* Free the CIRCULARBUFFER structure.
     */
    hMem = lpBuf->hSelf;
#ifndef _WIN32
    GlobalSmartPageUnlock((HGLOBAL)HIWORD(lpBuf));
#endif
    GlobalUnlock(hMem);
    GlobalFree(hMem);
}

/* GetEvent - Gets a MIDI event from the circular input buffer.  Events
 *  are removed from the buffer.  The corresponding PutEvent() function
 *  is called by the low-level callback function, so it must reside in
 *  the callback DLL.  PutEvent() is defined in the CALLBACK.C module.
 *
 * Params:  lpBuf - Points to the circular buffer.
 *          lpEvent - Points to an EVENT structure that is filled with the
 *              retrieved event.
 *
 * Return:  Returns non-zero if successful, zero if there are no 
 *   events to get.
 */
WORD FAR PASCAL GetEvent(LPCIRCULARBUFFER lpBuf, LPEVENT lpEvent)
{
        /* If no event available, return */
    if (!wNumDevices || lpBuf->dwCount <= 0) return (0);
    
    /* Get the event.
     */
    *lpEvent = *lpBuf->lpTail;
    
    /* Decrement the byte count, bump the tail pointer.
     */
    --lpBuf->dwCount;
    ++lpBuf->lpTail;
    
    /* Wrap the tail pointer, if necessary.
     */
    if(lpBuf->lpTail >= lpBuf->lpEnd)
        lpBuf->lpTail = lpBuf->lpStart;

    return 1;
}

/* PutEvent - Puts an EVENT in a CIRCULARBUFFER.  If the buffer is full, 
 *      it sets the wError element of the CIRCULARBUFFER structure 
 *      to be non-zero.
 *
 * Params:  lpBuf - Points to the CIRCULARBUFFER.
 *          lpEvent - Points to the EVENT.
 *
 * Return:  void
*/

void FAR PASCAL PutEvent(LPCIRCULARBUFFER lpBuf, LPEVENT lpEvent)
{
    /* If the buffer is full, set an error and return. 
     */
    if(lpBuf->dwCount >= lpBuf->dwSize){
        lpBuf->wError = 1;
        return;
    }
    
    /* Put the event in the buffer, bump the head pointer and the byte count.
     */
    *lpBuf->lpHead = *lpEvent;
    
    ++lpBuf->lpHead;
    ++lpBuf->dwCount;

    /* Wrap the head pointer, if necessary.
     */
    if(lpBuf->lpHead >= lpBuf->lpEnd)
        lpBuf->lpHead = lpBuf->lpStart;
}

/* midiInputHandler - Low-level callback function to handle MIDI input.
 *      Installed by midiInOpen().  The input handler takes incoming
 *      MIDI events and places them in the circular input buffer.  It then
 *      notifies the application by posting a MM_MIDIINPUT message.
 *
 *      This function is accessed at interrupt time, so it should be as 
 *      fast and efficient as possible.  You can't make any
 *      Windows calls here, except PostMessage().  The only Multimedia
 *      Windows call you can make are timeGetSystemTime(), midiOutShortMsg().
 *      
 *
 * Param:   hMidiIn - Handle for the associated input device.
 *          wMsg - One of the MIM_***** messages.
 *          dwInstance - Points to CALLBACKINSTANCEDATA structure.
 *          dwParam1 - MIDI data.
 *          dwParam2 - Timestamp (in milliseconds)
 *
 * Return:  void
 */     
void FAR PASCAL midiInputHandler(
HMIDIIN hMidiIn, 
WORD wMsg, 
DWORD dwInstance, 
DWORD dwParam1, 
DWORD dwParam2)
{
    EVENT event;
    
    switch(wMsg)
    {
        case MIM_OPEN:
            break;

        /* The only error possible is invalid MIDI data, so just pass
         * the invalid data on so we'll see it.
         */
        case MIM_ERROR:
        case MIM_DATA:
            event.fdwEvent = (wMsg == MIM_ERROR) ? EVNT_F_ERROR : 0;
            event.dwDevice = ((LPCALLBACKINSTANCEDATA)dwInstance)->dwDevice;
            event.data = dwParam1;
#ifdef MIDI_TIMESTAMP
            event.timestamp = timeGetSystemTime();
#endif
            /* Put the MIDI event in the circular input buffer.
             */

            PutEvent(((LPCALLBACKINSTANCEDATA)dwInstance)->lpBuf,
                       (LPEVENT) &event); 

            break;

        default:
            break;
    }
}

void msw_open_midiin(int nmidiin, int *midiinvec)
{
    UINT  wRtn;
    char szErrorText[256];
    unsigned int i;
    unsigned int ndev = 0;
    /* Allocate a circular buffer for low-level MIDI input.  This buffer
     * is filled by the low-level callback function and emptied by the
     * application.
     */
    lpInputBuffer = AllocCircularBuffer((DWORD)(INPUT_BUFFER_SIZE));
    if (lpInputBuffer == NULL)
    {
        printf("Not enough memory available for input buffer.\n");
        return;
    }

    /* Open all MIDI input devices after allocating and setting up
     * instance data for each device.  The instance data is used to
     * pass buffer management information between the application and
     * the low-level callback function.  It also includes a device ID,
     * a handle to the MIDI Mapper, and a handle to the application's
     * display window, so the callback can notify the window when input
     * data is available.  A single callback function is used to service
     * all opened input devices.
     */
    for (i=0; (i<(unsigned)nmidiin) && (i<MAXMIDIINDEV); i++)
    {
        if ((lpCallbackInstanceData[ndev] = AllocCallbackInstanceData()) == NULL)
        {
            printf("Not enough memory available.\n");
            FreeCircularBuffer(lpInputBuffer);
            return;
        }
        lpCallbackInstanceData[i]->dwDevice = i;
        lpCallbackInstanceData[i]->lpBuf = lpInputBuffer;
        
        wRtn = midiInOpen((LPHMIDIIN)&hMidiIn[ndev],
              midiinvec[i],
              (DWORD)midiInputHandler,
              (DWORD)lpCallbackInstanceData[ndev],
              CALLBACK_FUNCTION);
        if (wRtn)
        {
            FreeCallbackInstanceData(lpCallbackInstanceData[ndev]);
            msw_midiinerror("midiInOpen: %s\n", wRtn);
        }
        else ndev++;
    }

    /* Start MIDI input.
     */
    for (i=0; i<ndev; i++)
    {
        if (hMidiIn[i])
            midiInStart(hMidiIn[i]);
    }
    wNumDevices = ndev;
}

static void msw_close_midiin(void)
{
    unsigned int i;
    /* Stop, reset, close MIDI input.  Free callback instance data.
     */

    for (i=0; (i<wNumDevices) && (i<MAXMIDIINDEV); i++)
    {
        if (hMidiIn[i])
        {
            if (sys_verbose)
                post("closing MIDI input %d...", i);
            midiInStop(hMidiIn[i]);
            midiInReset(hMidiIn[i]);
            midiInClose(hMidiIn[i]);
            FreeCallbackInstanceData(lpCallbackInstanceData[i]);
        }
    }
    
    /* Free input buffer.
     */
    if (lpInputBuffer)
        FreeCircularBuffer(lpInputBuffer);

    if (sys_verbose)
        post("...done");
    wNumDevices = 0;
}

/* ------------------- public routines -------------------------- */

void sys_putmidimess(int portno, int a, int b, int c)
{
    DWORD foo;
    MMRESULT res;
    if (portno >= 0 && portno < msw_nmidiout)
    {
        foo = (a & 0xff) | ((b & 0xff) << 8) | ((c & 0xff) << 16);
        res = midiOutShortMsg(hMidiOut[portno], foo);
        if (res != MMSYSERR_NOERROR)
            post("MIDI out error %d", res);
    }
}

void sys_putmidibyte(int portno, int byte)
{
    MMRESULT res;
    if (portno >= 0 && portno < msw_nmidiout)
    {
        res = midiOutShortMsg(hMidiOut[portno], byte);
        if (res != MMSYSERR_NOERROR)
            post("MIDI out error %d", res);
    }
}

void sys_poll_midi(void)
{
    static EVENT msw_nextevent;
    static int msw_isnextevent;
    static double msw_nexteventtime;

    while (1)
    {
        if (!msw_isnextevent)
        {
            if (!GetEvent(lpInputBuffer, &msw_nextevent)) break;
            msw_isnextevent = 1;
#ifdef MIDI_TIMESTAMP
            msw_nexteventtime = msw_midigettimefor(&foo.timestamp);
#endif
        }
#ifdef MIDI_TIMESTAMP
        if (0.001 * clock_gettimesince(initsystime) >= msw_nexteventtime)
#endif
        {
            int msgtype = ((msw_nextevent.data & 0xf0) >> 4) - 8;
            int commandbyte = msw_nextevent.data & 0xff;
            int byte1 = (msw_nextevent.data >> 8) & 0xff;
            int byte2 = (msw_nextevent.data >> 16) & 0xff;
            int portno = msw_nextevent.dwDevice;
            switch (msgtype)
            {
            case 0: 
            case 1: 
            case 2:
            case 3:
            case 6:
                sys_midibytein(portno, commandbyte);
                sys_midibytein(portno, byte1);
                sys_midibytein(portno, byte2);
                break; 
            case 4:
            case 5:
                sys_midibytein(portno, commandbyte);
                sys_midibytein(portno, byte1);
                break;
            case 7:
                sys_midibytein(portno, commandbyte);
                break; 
            }
            msw_isnextevent = 0;
        }
    }
}

void sys_do_open_midi(int nmidiin, int *midiinvec,
    int nmidiout, int *midioutvec)
{
    if (nmidiout)
        msw_open_midiout(nmidiout, midioutvec);
    if (nmidiin)
    {
        post(
         "Warning: midi input is dangerous in Microsoft Windows; see Pd manual)");
        msw_open_midiin(nmidiin, midiinvec);
    }
}

void sys_close_midi( void)
{
    msw_close_midiin();
    msw_close_midiout();
}

#if 0
/* list the audio and MIDI device names */
void sys_listmididevs(void)
{
    UINT  wRtn, ndevices;
    unsigned int i;

    /* for MIDI and audio in and out, get the number of devices.
        Then get the capabilities of each device and print its description. */

    ndevices = midiInGetNumDevs();
    for (i = 0; i < ndevices; i++)
    {
        MIDIINCAPS micap;
        wRtn = midiInGetDevCaps(i, (LPMIDIINCAPS) &micap,
            sizeof(micap));
        if (wRtn) msw_midiinerror("midiInGetDevCaps: %s\n", wRtn);
        else fprintf(stderr,
            "MIDI input device #%d: %s\n", i+1, micap.szPname);
    }

    ndevices = midiOutGetNumDevs();
    for (i = 0; i < ndevices; i++)
    {
        MIDIOUTCAPS mocap;
        wRtn = midiOutGetDevCaps(i, (LPMIDIOUTCAPS) &mocap,
            sizeof(mocap));
        if (wRtn) msw_midiouterror("midiOutGetDevCaps: %s\n", wRtn);
        else fprintf(stderr,
            "MIDI output device #%d: %s\n", i+1, mocap.szPname);
    }

}
#endif

void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize)
{
    int i, nin = midiInGetNumDevs(), nout = midiOutGetNumDevs();
    UINT  wRtn;
    if (nin > maxndev)
        nin = maxndev;
    for (i = 0; i < nin; i++)
    {
        MIDIINCAPS micap;
        wRtn = midiInGetDevCaps(i, (LPMIDIINCAPS) &micap, sizeof(micap));
        strncpy(indevlist + i * devdescsize, 
            (wRtn ? "???" : micap.szPname), devdescsize);
        indevlist[(i+1) * devdescsize - 1] = 0;
    }
    if (nout > maxndev)
        nout = maxndev;
    for (i = 0; i < nout; i++)
    {
        MIDIOUTCAPS mocap;
        wRtn = midiOutGetDevCaps(i, (LPMIDIOUTCAPS) &mocap, sizeof(mocap));
        strncpy(outdevlist + i * devdescsize, 
            (wRtn ? "???" : mocap.szPname), devdescsize);
        outdevlist[(i+1) * devdescsize - 1] = 0;
    }
    *nindevs = nin;
    *noutdevs = nout;
}
