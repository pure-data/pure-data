/* pmwinmm.c -- system specific definitions */

#include "windows.h"
#include "mmsystem.h"
#include "portmidi.h"
#include "pminternal.h"
#include "pmwinmm.h"
#include "string.h"
#include "porttime.h"

/* asserts used to verify portMidi code logic is sound; later may want
    something more graceful */
#include <assert.h>

#ifdef DEBUG
/* this printf stuff really important for debugging client app w/host errors.
    probably want to do something else besides read/write from/to console
    for portability, however */
#define STRING_MAX 80
#include "stdio.h"
#endif

#define streql(x, y) (strcmp(x, y) == 0)

#define MIDI_SYSEX      0xf0
#define MIDI_EOX        0xf7

/* callback routines */
static void CALLBACK winmm_in_callback(HMIDIIN hMidiIn,
                                       WORD wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
static void CALLBACK winmm_streamout_callback(HMIDIOUT hmo, UINT wMsg,
        DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
static void CALLBACK winmm_out_callback(HMIDIOUT hmo, UINT wMsg,
                                        DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

extern pm_fns_node pm_winmm_in_dictionary;
extern pm_fns_node pm_winmm_out_dictionary;

static void winmm_out_delete(PmInternal *midi); /* forward reference */

#define SYSEX_BYTES_PER_BUFFER 1024
/* 3 midi messages per buffer */
#define OUTPUT_BYTES_PER_BUFFER 36

#define MIDIHDR_SYSEX_BUFFER_LENGTH(x) ((x) + sizeof(long)*3)
#define MIDIHDR_SYSEX_SIZE(x) (MIDIHDR_SYSEX_BUFFER_LENGTH(x) + sizeof(MIDIHDR))
#define MIDIHDR_BUFFER_LENGTH(x) (x)
#define MIDIHDR_SIZE(x) (MIDIHDR_BUFFER_LENGTH(x) + sizeof(MIDIHDR))

/*
==============================================================================
win32 mmedia system specific structure passed to midi callbacks
==============================================================================
*/

/* global winmm device info */
MIDIINCAPS *midi_in_caps = NULL;
MIDIINCAPS midi_in_mapper_caps;
UINT midi_num_inputs = 0;
MIDIOUTCAPS *midi_out_caps = NULL;
MIDIOUTCAPS midi_out_mapper_caps;
UINT midi_num_outputs = 0;

/* per device info */
typedef struct midiwinmm_struct
{

    union {
        HMIDISTRM stream;   /* windows handle for stream */
        HMIDIOUT out;       /* windows handle for out calls */
        HMIDIIN in;         /* windows handle for in calls */
    } handle;

    /* midi output messages are sent in these buffers, which are allocated
     * in a round-robin fashion, using next_buffer as an index
     */
    LPMIDIHDR *buffers;     /* pool of buffers for midi in or out data */
    int num_buffers;        /* how many buffers */
    int next_buffer;        /* index of next buffer to send */
    HANDLE buffer_signal;   /* used to wait for buffer to become free */

    LPMIDIHDR *sysex_buffers;   /* pool of buffers for sysex data */
    int num_sysex_buffers;      /* how many sysex buffers */
    int next_sysex_buffer;      /* index of next sysexbuffer to send */
    HANDLE sysex_buffer_signal; /* used to wait for sysex buffer to become free */

    unsigned long last_time;    /* last output time */
    int first_message;          /* flag: treat first message differently */
    int sysex_mode;             /* middle of sending sysex */
    unsigned long sysex_word;   /* accumulate data when receiving sysex */
    unsigned int sysex_byte_count; /* count how many received or to send */
    LPMIDIHDR hdr;              /* the message accumulating sysex to send */
    unsigned long sync_time;    /* when did we last determine delta? */
    long delta;                 /* difference between stream time and
                                       real time */
    int error;                  /* host error from doing port midi call */
    int callback_error;         /* host error from midi in or out callback */
}
midiwinmm_node, *midiwinmm_type;


/*
=============================================================================
general MIDI device queries
=============================================================================
*/
static void pm_winmm_general_inputs()
{
    UINT i;
    WORD wRtn;
    midi_num_inputs = midiInGetNumDevs();
    midi_in_caps = pm_alloc(sizeof(MIDIINCAPS) * midi_num_inputs);

    if (midi_in_caps == NULL) {
        // if you can't open a particular system-level midi interface
        // (such as winmm), we just consider that system or API to be
        // unavailable and move on without reporting an error. This
        // may be the wrong thing to do, especially in this case.
        return ;
    }

    for (i = 0; i < midi_num_inputs; i++) {
        wRtn = midiInGetDevCaps(i, (LPMIDIINCAPS) & midi_in_caps[i],
                                sizeof(MIDIINCAPS));
        if (wRtn == MMSYSERR_NOERROR) {
            /* ignore errors here -- if pm_descriptor_max is exceeded, some
               devices will not be accessible. */
            pm_add_device("MMSystem", midi_in_caps[i].szPname, TRUE,
                          (void *) i, &pm_winmm_in_dictionary);
        }
    }
}


static void pm_winmm_mapper_input()
{
    WORD wRtn;
    /* Note: if MIDIMAPPER opened as input (documentation implies you
        can, but current system fails to retrieve input mapper
        capabilities) then you still should retrieve some formof
        setup info. */
    wRtn = midiInGetDevCaps((UINT) MIDIMAPPER,
                            (LPMIDIINCAPS) & midi_in_mapper_caps, sizeof(MIDIINCAPS));
    if (wRtn == MMSYSERR_NOERROR) {
        pm_add_device("MMSystem", midi_in_mapper_caps.szPname, TRUE,
                      (void *) MIDIMAPPER, &pm_winmm_in_dictionary);
    }
}


static void pm_winmm_general_outputs()
{
    UINT i;
    DWORD wRtn;
    midi_num_outputs = midiOutGetNumDevs();
    midi_out_caps = pm_alloc( sizeof(MIDIOUTCAPS) * midi_num_outputs );

    if (midi_out_caps == NULL) {
        // no error is reported -- see pm_winmm_general_inputs
        return ;
    }

    for (i = 0; i < midi_num_outputs; i++) {
        wRtn = midiOutGetDevCaps(i, (LPMIDIOUTCAPS) & midi_out_caps[i],
                                 sizeof(MIDIOUTCAPS));
        if (wRtn == MMSYSERR_NOERROR) {
            pm_add_device("MMSystem", midi_out_caps[i].szPname, FALSE,
                          (void *) i, &pm_winmm_out_dictionary);
        }
    }
}


static void pm_winmm_mapper_output()
{
    WORD wRtn;
    /* Note: if MIDIMAPPER opened as output (pseudo MIDI device
        maps device independent messages into device dependant ones,
        via NT midimapper program) you still should get some setup info */
    wRtn = midiOutGetDevCaps((UINT) MIDIMAPPER, (LPMIDIOUTCAPS)
                             & midi_out_mapper_caps, sizeof(MIDIOUTCAPS));
    if (wRtn == MMSYSERR_NOERROR) {
        pm_add_device("MMSystem", midi_out_mapper_caps.szPname, FALSE,
                      (void *) MIDIMAPPER, &pm_winmm_out_dictionary);
    }
}


/*
=========================================================================================
host error handling
=========================================================================================
*/
static unsigned int winmm_has_host_error(PmInternal * midi)
{
    midiwinmm_type m = (midiwinmm_type)midi->descriptor;
    return m->callback_error || m->error;
}


/* str_copy_len -- like strcat, but won't overrun the destination string */
/*
 * returns length of resulting string
 */
static int str_copy_len(char *dst, char *src, int len)
{
    strncpy(dst, src, len);
    /* just in case suffex is greater then len, terminate with zero */
    dst[len - 1] = 0;
    return strlen(dst);
}


static void winmm_get_host_error(PmInternal * midi, char * msg, UINT len)
{
    /* precondition: midi != NULL */
    midiwinmm_node * m = (midiwinmm_node *) midi->descriptor;
    char *hdr1 = "Host error: ";
    char *hdr2 = "Host callback error: ";

    msg[0] = 0; /* initialize result string to empty */

    if (descriptors[midi->device_id].pub.input) {
        /* input and output use different winmm API calls */
        if (m) { /* make sure there is an open device to examine */
            if (m->error != MMSYSERR_NOERROR) {
                int n = str_copy_len(msg, hdr1, len);
                /* read and record host error */
                int err = midiInGetErrorText(m->error, msg + n, len - n);
                assert(err == MMSYSERR_NOERROR);
                m->error = MMSYSERR_NOERROR;
            } else if (m->callback_error != MMSYSERR_NOERROR) {
                int n = str_copy_len(msg, hdr2, len);
                int err = midiInGetErrorText(m->callback_error, msg + n,
                                             len - n);
                assert(err == MMSYSERR_NOERROR);
                m->callback_error = MMSYSERR_NOERROR;
            }
        }
    } else { /* output port */
        if (m) {
            if (m->error != MMSYSERR_NOERROR) {
                int n = str_copy_len(msg, hdr1, len);
                int err = midiOutGetErrorText(m->error, msg + n, len - n);
                assert(err == MMSYSERR_NOERROR);
                m->error = MMSYSERR_NOERROR;
            } else if (m->callback_error != MMSYSERR_NOERROR) {
                int n = str_copy_len(msg, hdr2, len);
                int err = midiOutGetErrorText(m->callback_error, msg + n,
                                              len = n);
                assert(err == MMSYSERR_NOERROR);
                m->callback_error = MMSYSERR_NOERROR;
            }
        }
    }
}


/*
=============================================================================
buffer handling
=============================================================================
*/
static MIDIHDR *allocate_buffer(long data_size)
{
    /*
     * with short messages, the MIDIEVENT structure contains the midi message,
     * so there is no need for additional data
     */

    LPMIDIHDR hdr = (LPMIDIHDR) pm_alloc(MIDIHDR_SIZE(data_size));
    MIDIEVENT *evt;
    if (!hdr) return NULL;
    evt = (MIDIEVENT *) (hdr + 1); /* place MIDIEVENT after header */
    hdr->lpData = (LPSTR) evt;
    hdr->dwBufferLength = MIDIHDR_BUFFER_LENGTH(data_size); /* was: sizeof(MIDIEVENT) + data_size; */
    hdr->dwFlags = 0;
    hdr->dwUser = 0;
    return hdr;
}

static MIDIHDR *allocate_sysex_buffer(long data_size)
{
    /* we're actually allocating slightly more than data_size because one more word of
     * data is contained in MIDIEVENT. We include the size of MIDIEVENT because we need
     * the MIDIEVENT header in addition to the data
     */
    LPMIDIHDR hdr = (LPMIDIHDR) pm_alloc(MIDIHDR_SYSEX_SIZE(data_size));
    MIDIEVENT *evt;
    if (!hdr) return NULL;
    evt = (MIDIEVENT *) (hdr + 1); /* place MIDIEVENT after header */
    hdr->lpData = (LPSTR) evt;
    hdr->dwBufferLength = MIDIHDR_SYSEX_BUFFER_LENGTH(data_size); /* was: sizeof(MIDIEVENT) + data_size; */
    hdr->dwFlags = 0;
    hdr->dwUser = 0;
    return hdr;
}

static PmError allocate_buffers(midiwinmm_type m, long data_size, long count)
{
    PmError rslt = pmNoError;
    /* buffers is an array of count pointers to MIDIHDR/MIDIEVENT struct */
    m->buffers = (LPMIDIHDR *) pm_alloc(sizeof(LPMIDIHDR) * count);
    if (!m->buffers) return pmInsufficientMemory;
    m->num_buffers = count;
    while (count > 0) {
        LPMIDIHDR hdr = allocate_buffer(data_size);
        if (!hdr) rslt = pmInsufficientMemory;
        count--;
        m->buffers[count] = hdr; /* this may be NULL if allocation fails */
    }
    return rslt;
}

static PmError allocate_sysex_buffers(midiwinmm_type m, long data_size, long count)
{
    PmError rslt = pmNoError;
    /* buffers is an array of count pointers to MIDIHDR/MIDIEVENT struct */
    m->sysex_buffers = (LPMIDIHDR *) pm_alloc(sizeof(LPMIDIHDR) * count);
    if (!m->sysex_buffers) return pmInsufficientMemory;
    m->num_sysex_buffers = count;
    while (count > 0) {
        LPMIDIHDR hdr = allocate_sysex_buffer(data_size);
        if (!hdr) rslt = pmInsufficientMemory;
        count--;
        m->sysex_buffers[count] = hdr; /* this may be NULL if allocation fails */
    }
    return rslt;
}

static LPMIDIHDR get_free_sysex_buffer(PmInternal *midi)
{
    LPMIDIHDR r = NULL;
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    if (!m->sysex_buffers) {
        if (allocate_sysex_buffers(m, SYSEX_BYTES_PER_BUFFER, 2)) {
            return NULL;
        }
    }
    /* busy wait until we find a free buffer */
    while (TRUE) {
        int i;
        for (i = 0; i < m->num_sysex_buffers; i++) {
            m->next_sysex_buffer++;
            if (m->next_sysex_buffer >= m->num_sysex_buffers) m->next_sysex_buffer = 0;
            r = m->sysex_buffers[m->next_sysex_buffer];
            if ((r->dwFlags & MHDR_PREPARED) == 0) goto found_sysex_buffer;
        }
        /* after scanning every buffer and not finding anything, block */
        WaitForSingleObject(m->sysex_buffer_signal, INFINITE);
    }
found_sysex_buffer:
    r->dwBytesRecorded = 0;
    m->error = midiOutPrepareHeader(m->handle.out, r, sizeof(MIDIHDR));
    return r;
}


static LPMIDIHDR get_free_output_buffer(PmInternal *midi)
{
    LPMIDIHDR r = NULL;
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    if (!m->buffers) {
        if (allocate_buffers(m, OUTPUT_BYTES_PER_BUFFER, 2)) {
            return NULL;
        }
    }
    /* busy wait until we find a free buffer */
    while (TRUE) {
        int i;
        for (i = 0; i < m->num_buffers; i++) {
            m->next_buffer++;
            if (m->next_buffer >= m->num_buffers) m->next_buffer = 0;
            r = m->buffers[m->next_buffer];
            if ((r->dwFlags & MHDR_PREPARED) == 0) goto found_buffer;
        }
        /* after scanning every buffer and not finding anything, block */
        WaitForSingleObject(m->buffer_signal, INFINITE);
    }
found_buffer:
    r->dwBytesRecorded = 0;
    m->error = midiOutPrepareHeader(m->handle.out, r, sizeof(MIDIHDR));
    return r;
}

static PmError resize_sysex_buffer(PmInternal *midi, long old_size, long new_size)
{
    LPMIDIHDR big;
    int i;
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    /* buffer must be smaller than 64k, but be also be a multiple of 4 */
    if (new_size > 65520) {
        if (old_size >= 65520)
            return pmBufferMaxSize;
        else
            new_size = 65520;
    }
    /* allocate a bigger message  */
    big = allocate_sysex_buffer(new_size);
    /* printf("expand to %d bytes\n", new_size);*/
    if (!big) return pmInsufficientMemory;
    m->error = midiOutPrepareHeader(m->handle.out, big, sizeof(MIDIHDR));
    if (m->error) {
        pm_free(big);
        return pmHostError;
    }
    /* make sure we're not going to overwrite any memory */
    assert(old_size <= new_size);
    memcpy(big->lpData, m->hdr->lpData, old_size);

    /* find which buffer this was, and replace it */

    for (i = 0;i < m->num_sysex_buffers;i++) {
        if (m->sysex_buffers[i] == m->hdr) {
            m->sysex_buffers[i] = big;
            pm_free(m->hdr);
            m->hdr = big;
            break;
        }
    }
    assert(i != m->num_sysex_buffers);

    return pmNoError;

}
/*
=========================================================================================
begin midi input implementation
=========================================================================================
*/

static PmError winmm_in_open(PmInternal *midi, void *driverInfo)
{
    DWORD dwDevice;
    int i = midi->device_id;
    midiwinmm_type m;
    LPMIDIHDR hdr;
    long buffer_len;
    dwDevice = (DWORD) descriptors[i].descriptor;

    /* create system dependent device data */
    m = (midiwinmm_type) pm_alloc(sizeof(midiwinmm_node)); /* create */
    midi->descriptor = m;
    if (!m) goto no_memory;
    m->handle.in = NULL;
    m->buffers = NULL;
    m->num_buffers = 0;
    m->next_buffer = 0;
    m->sysex_buffers = NULL;
    m->num_sysex_buffers = 0;
    m->next_sysex_buffer = 0;
    m->last_time = 0;
    m->first_message = TRUE; /* not used for input */
    m->sysex_mode = FALSE;
    m->sysex_word = 0;
    m->sysex_byte_count = 0;
    m->sync_time = 0;
    m->delta = 0;
    m->error = MMSYSERR_NOERROR;
    m->callback_error = MMSYSERR_NOERROR;

    /* open device */
    pm_hosterror = midiInOpen(&(m->handle.in),  /* input device handle */
                              dwDevice,  /* device ID */
                              (DWORD) winmm_in_callback,  /* callback address */
                              (DWORD) midi,  /* callback instance data */
                              CALLBACK_FUNCTION); /* callback is a procedure */
    if (pm_hosterror) goto free_descriptor;

    /* allocate first buffer for sysex data */
    buffer_len = midi->buffer_len - 1;
    if (midi->buffer_len < 32)
        buffer_len = PM_DEFAULT_SYSEX_BUFFER_SIZE;

    hdr = allocate_sysex_buffer(buffer_len);
    if (!hdr) goto close_device;
    pm_hosterror = midiInPrepareHeader(m->handle.in, hdr, sizeof(MIDIHDR));
    if (pm_hosterror) {
        pm_free(hdr);
        goto close_device;
    }
    pm_hosterror = midiInAddBuffer(m->handle.in, hdr, sizeof(MIDIHDR));
    if (pm_hosterror) goto close_device;

    /* allocate second buffer */
    hdr = allocate_sysex_buffer(buffer_len);
    if (!hdr) goto close_device;
    pm_hosterror = midiInPrepareHeader(m->handle.in, hdr, sizeof(MIDIHDR));
    if (pm_hosterror) {
        pm_free(hdr);
        goto reset_device; /* because first buffer was added */
    }
    pm_hosterror = midiInAddBuffer(m->handle.in, hdr, sizeof(MIDIHDR));
    if (pm_hosterror) goto reset_device;

    /* start device */
    pm_hosterror = midiInStart(m->handle.in);
    if (pm_hosterror) goto reset_device;
    return pmNoError;

    /* undo steps leading up to the detected error */
reset_device:
    /* ignore return code (we already have an error to report) */
    midiInReset(m->handle.in);
close_device:
    midiInClose(m->handle.in); /* ignore return code */
free_descriptor:
    midi->descriptor = NULL;
    pm_free(m);
no_memory:
    if (pm_hosterror) {
        int err = midiInGetErrorText(pm_hosterror, (char *) pm_hosterror_text,
                                     PM_HOST_ERROR_MSG_LEN);
        assert(err == MMSYSERR_NOERROR);
        return pmHostError;
    }
    /* if !pm_hosterror, then the error must be pmInsufficientMemory */
    return pmInsufficientMemory;
    /* note: if we return an error code, the device will be
       closed and memory will be freed. It's up to the caller
       to free the parameter midi */
}


/* winmm_in_close -- close an open midi input device */
/*
 * assume midi is non-null (checked by caller)
 */
static PmError winmm_in_close(PmInternal *midi)
{
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    if (!m) return pmBadPtr;
    /* device to close */
    if (pm_hosterror = midiInStop(m->handle.in)) {
        midiInReset(m->handle.in); /* try to reset and close port */
        midiInClose(m->handle.in);
    } else if (pm_hosterror = midiInReset(m->handle.in)) {
        midiInClose(m->handle.in); /* best effort to close midi port */
    } else {
        pm_hosterror = midiInClose(m->handle.in);
    }
    midi->descriptor = NULL;
    pm_free(m); /* delete */
    if (pm_hosterror) {
        int err = midiInGetErrorText(pm_hosterror, (char *) pm_hosterror_text,
                                     PM_HOST_ERROR_MSG_LEN);
        assert(err == MMSYSERR_NOERROR);
        return pmHostError;
    }
    return pmNoError;
}


/* Callback function executed via midiInput SW interrupt (via midiInOpen). */
static void FAR PASCAL winmm_in_callback(
    HMIDIIN hMidiIn,    /* midiInput device Handle */
    WORD wMsg,          /* midi msg */
    DWORD dwInstance,   /* application data */
    DWORD dwParam1,     /* MIDI data */
    DWORD dwParam2)    /* device timestamp (wrt most recent midiInStart) */
{
    static int entry = 0;
    PmInternal *midi = (PmInternal *) dwInstance;
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;

    if (++entry > 1) {
        assert(FALSE);
    }

    /* for simplicity, this logic perhaps overly conservative */
    /* note also that this might leak memory if buffers are being
       returned as a result of midiInReset */
    if (m->callback_error) {
        entry--;
        return ;
    }

    switch (wMsg) {
    case MIM_DATA: {
            /* dwParam1 is MIDI data received, packed into DWORD w/ 1st byte of
                    message LOB;
               dwParam2 is time message received by input device driver, specified
                in [ms] from when midiInStart called.
               each message is expanded to include the status byte */

            long new_driver_time = dwParam2;

            if ((dwParam1 & 0x80) == 0) {
                /* not a status byte -- ignore it. This happens running the
                   sysex.c test under Win2K with MidiMan USB 1x1 interface.
                   Is it a driver bug or a Win32 bug? If not, there's a bug
                   here somewhere. -RBD
                 */
            } else { /* data to process */
                PmEvent event;
                if (midi->time_proc)
                    dwParam2 = (*midi->time_proc)(midi->time_info);
                event.timestamp = dwParam2;
                event.message = dwParam1;
                pm_read_short(midi, &event);
            }
            break;
        }
    case MIM_LONGDATA: {
            MIDIHDR *lpMidiHdr = (MIDIHDR *) dwParam1;
            unsigned char *data = lpMidiHdr->lpData;
            unsigned int i = 0;
            long size = sizeof(MIDIHDR) + lpMidiHdr->dwBufferLength;
            /* ignore sysex data, but free returned buffers */
            if (lpMidiHdr->dwBytesRecorded > 0 &&
                    midi->filters & PM_FILT_SYSEX) {
                m->callback_error = midiInAddBuffer(hMidiIn, lpMidiHdr,
                                                    sizeof(MIDIHDR));
                break;
            }
            if (midi->time_proc)
                dwParam2 = (*midi->time_proc)(midi->time_info);

            while (i < lpMidiHdr->dwBytesRecorded) {
                /* collect bytes from *data into a word */
                pm_read_byte(midi, *data, dwParam2);
                data++;
                i++;
            }
            /* when a device is closed, the pending MIM_LONGDATA buffers are
               returned to this callback with dwBytesRecorded == 0. In this
               case, we do not want to send them back to the interface (if
               we do, the interface will not close, and Windows OS may hang). */
            if (lpMidiHdr->dwBytesRecorded > 0) {
                m->callback_error = midiInAddBuffer(hMidiIn, lpMidiHdr,
                                                    sizeof(MIDIHDR));
            } else {
                pm_free(lpMidiHdr);
            }
            break;
        }
    case MIM_OPEN:  /* fall thru */
    case MIM_CLOSE:
    case MIM_ERROR:
    case MIM_LONGERROR:
    default:
        break;
    }
    entry--;
}

/*
=========================================================================================
begin midi output implementation
=========================================================================================
*/

/* begin helper routines used by midiOutStream interface */

/* add_to_buffer -- adds timestamped short msg to buffer, returns fullp */
static int add_to_buffer(midiwinmm_type m, LPMIDIHDR hdr,
                         unsigned long delta, unsigned long msg)
{
    unsigned long *ptr = (unsigned long *)
                         (hdr->lpData + hdr->dwBytesRecorded);
    *ptr++ = delta; /* dwDeltaTime */
    *ptr++ = 0;     /* dwStream */
    *ptr++ = msg;   /* dwEvent */
    hdr->dwBytesRecorded += 3 * sizeof(long);
    /* if the addition of three more words (a message) would extend beyond
       the buffer length, then return TRUE (full)
     */
    return hdr->dwBytesRecorded + 3 * sizeof(long) > hdr->dwBufferLength;
}

#ifdef GARBAGE
static void start_sysex_buffer(LPMIDIHDR hdr, unsigned long delta)
{
    unsigned long *ptr = (unsigned long *) hdr->lpData;
    *ptr++ = delta;
    *ptr++ = 0;
    *ptr = MEVT_F_LONG;
    hdr->dwBytesRecorded = 3 * sizeof(long);
}

static int add_byte_to_buffer(midiwinmm_type m, LPMIDIHDR hdr,
                              unsigned char midi_byte)
{
    allocate message if hdr is null
    send message if it is full
        add byte to non - full message
            unsigned char *ptr = (unsigned char *) (hdr->lpData + hdr->dwBytesRecorded);
    *ptr = midi_byte;
    return ++hdr->dwBytesRecorded >= hdr->dwBufferLength;
}
#endif


static PmTimestamp pm_time_get(midiwinmm_type m)
{
    MMTIME mmtime;
    MMRESULT wRtn;
    mmtime.wType = TIME_TICKS;
    mmtime.u.ticks = 0;
    wRtn = midiStreamPosition(m->handle.stream, &mmtime, sizeof(mmtime));
    assert(wRtn == MMSYSERR_NOERROR);
    return mmtime.u.ticks;
}

#ifdef GARBAGE
static unsigned long synchronize(PmInternal *midi, midiwinmm_type m)
{
    unsigned long pm_stream_time_2 = pm_time_get(m);
    unsigned long real_time;
    unsigned long pm_stream_time;
    /* figure out the time */
    do {
        /* read real_time between two reads of stream time */
        pm_stream_time = pm_stream_time_2;
        real_time = (*midi->time_proc)(midi->time_info);
        pm_stream_time_2 = pm_time_get(m);
        /* repeat if more than 1ms elapsed */
    } while (pm_stream_time_2 > pm_stream_time + 1);
    m->delta = pm_stream_time - real_time;
    m->sync_time = real_time;
    return real_time;
}
#endif


/* end helper routines used by midiOutStream interface */


static PmError winmm_out_open(PmInternal *midi, void *driverInfo)
{
    DWORD dwDevice;
    int i = midi->device_id;
    midiwinmm_type m;
    MIDIPROPTEMPO propdata;
    MIDIPROPTIMEDIV divdata;
    dwDevice = (DWORD) descriptors[i].descriptor;

    /* create system dependent device data */
    m = (midiwinmm_type) pm_alloc(sizeof(midiwinmm_node)); /* create */
    midi->descriptor = m;
    if (!m) goto no_memory;
    m->handle.out = NULL;
    m->buffers = NULL;
    m->num_buffers = 0;
    m->next_buffer = 0;
    m->sysex_buffers = NULL;
    m->num_sysex_buffers = 0;
    m->next_sysex_buffer = 0;
    m->last_time = 0;
    m->first_message = TRUE; /* we treat first message as special case */
    m->sysex_mode = FALSE;
    m->sysex_word = 0;
    m->sysex_byte_count = 0;
    m->hdr = NULL;
    m->sync_time = 0;
    m->delta = 0;
    m->error = MMSYSERR_NOERROR;
    m->callback_error = MMSYSERR_NOERROR;

    /* create a signal */
    m->buffer_signal = CreateEvent(NULL, FALSE, FALSE, NULL);
    m->sysex_buffer_signal = CreateEvent(NULL, FALSE, FALSE, NULL);

    /* this should only fail when there are very serious problems */
    assert(m->buffer_signal);
    assert(m->sysex_buffer_signal);

    /* open device */
    if (midi->latency == 0) {
        /* use simple midi out calls */
        pm_hosterror = midiOutOpen((LPHMIDIOUT) & m->handle.out,  /* device Handle */
                                   dwDevice,  /* device ID  */
                                   (DWORD) winmm_out_callback,
                                   (DWORD) midi,  /* callback instance data */
                                   CALLBACK_FUNCTION); /* callback type */
    } else {
        /* use stream-based midi output (schedulable in future) */
        pm_hosterror = midiStreamOpen(&m->handle.stream,  /* device Handle */
                                      (LPUINT) & dwDevice,  /* device ID pointer */
                                      1,  /* reserved, must be 1 */
                                      (DWORD) winmm_streamout_callback,
                                      (DWORD) midi,  /* callback instance data */
                                      CALLBACK_FUNCTION);
    }
    if (pm_hosterror != MMSYSERR_NOERROR) {
        goto free_descriptor;
    }

    if (midi->latency != 0) {
        long dur = 0;
        /* with stream output, specified number of buffers allocated here */
        int count = midi->buffer_len;
        if (count == 0)
            count = midi->latency / 2; /* how many buffers to get */

        propdata.cbStruct = sizeof(MIDIPROPTEMPO);
        propdata.dwTempo = 480000; /* microseconds per quarter */
        pm_hosterror = midiStreamProperty(m->handle.stream,
                                          (LPBYTE) & propdata,
                                          MIDIPROP_SET | MIDIPROP_TEMPO);
        if (pm_hosterror) goto close_device;

        divdata.cbStruct = sizeof(MIDIPROPTEMPO);
        divdata.dwTimeDiv = 480;   /* divisions per quarter */
        pm_hosterror = midiStreamProperty(m->handle.stream,
                                          (LPBYTE) & divdata,
                                          MIDIPROP_SET | MIDIPROP_TIMEDIV);
        if (pm_hosterror) goto close_device;

        /* allocate at least 3 buffers */
        if (count < 3) count = 3;
        if (allocate_buffers(m, OUTPUT_BYTES_PER_BUFFER, count)) goto free_buffers;
        if (allocate_sysex_buffers(m, SYSEX_BYTES_PER_BUFFER, 2)) goto free_buffers;
        /* start device */
        pm_hosterror = midiStreamRestart(m->handle.stream);
        if (pm_hosterror != MMSYSERR_NOERROR) goto free_buffers;
    }
    return pmNoError;

free_buffers:
    /* buffers are freed below by winmm_out_delete */
close_device:
    midiOutClose(m->handle.out);
free_descriptor:
    midi->descriptor = NULL;
    winmm_out_delete(midi); /* frees buffers and m */
no_memory:
    if (pm_hosterror) {
        int err = midiOutGetErrorText(pm_hosterror, (char *) pm_hosterror_text,
                                      PM_HOST_ERROR_MSG_LEN);
        assert(err == MMSYSERR_NOERROR);
        return pmHostError;
    }
    return pmInsufficientMemory;
}


/* winmm_out_delete -- carefully free data associated with midi */
/**/
static void winmm_out_delete(PmInternal *midi)
{
    /* delete system dependent device data */
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    if (m) {
        if (m->buffer_signal) {
            /* don't report errors -- better not to stop cleanup */
            CloseHandle(m->buffer_signal);
        }
        if (m->sysex_buffer_signal) {
            /* don't report errors -- better not to stop cleanup */
            CloseHandle(m->sysex_buffer_signal);
        }
        if (m->buffers) {
            /* if using stream output, free buffers */
            int i;
            for (i = 0; i < m->num_buffers; i++) {
                if (m->buffers[i]) pm_free(m->buffers[i]);
            }
            pm_free(m->buffers);
        }

        if (m->sysex_buffers) {
            /* free sysex buffers */
            int i;
            for (i = 0; i < m->num_sysex_buffers; i++) {
                if (m->sysex_buffers[i]) pm_free(m->sysex_buffers[i]);
            }
            pm_free(m->sysex_buffers);
        }
    }
    midi->descriptor = NULL;
    pm_free(m); /* delete */
}


/* see comments for winmm_in_close */
static PmError winmm_out_close(PmInternal *midi)
{
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    if (m->handle.out) {
        /* device to close */
        if (midi->latency == 0) {
            pm_hosterror = midiOutClose(m->handle.out);
        } else {
            pm_hosterror = midiStreamClose(m->handle.stream);
        }
        /* regardless of outcome, free memory */
        winmm_out_delete(midi);
    }
    if (pm_hosterror) {
        int err = midiOutGetErrorText(pm_hosterror,
                                      (char *) pm_hosterror_text,
                                      PM_HOST_ERROR_MSG_LEN);
        assert(err == MMSYSERR_NOERROR);
        return pmHostError;
    }
    return pmNoError;
}


static PmError winmm_out_abort(PmInternal *midi)
{
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    m->error = MMSYSERR_NOERROR;

    /* only stop output streams */
    if (midi->latency > 0) {
        m->error = midiStreamStop(m->handle.stream);
    }
    return m->error ? pmHostError : pmNoError;
}

#ifdef GARBAGE
static PmError winmm_write_sysex_byte(PmInternal *midi, unsigned char byte)
{
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    unsigned char *msg_buffer;

    /* at the beginning of sysex, m->hdr is NULL */
    if (!m->hdr) { /* allocate a buffer if none allocated yet */
        m->hdr = get_free_output_buffer(midi);
        if (!m->hdr) return pmInsufficientMemory;
        m->sysex_byte_count = 0;
    }
    /* figure out where to write byte */
    msg_buffer = (unsigned char *) (m->hdr->lpData);
    assert(m->hdr->lpData == (char *) (m->hdr + 1));

    /* check for overflow */
    if (m->sysex_byte_count >= m->hdr->dwBufferLength) {
        /* allocate a bigger message -- double it every time */
        LPMIDIHDR big = allocate_buffer(m->sysex_byte_count * 2);
        /* printf("expand to %d bytes\n", m->sysex_byte_count * 2); */
        if (!big) return pmInsufficientMemory;
        m->error = midiOutPrepareHeader(m->handle.out, big,
                                        sizeof(MIDIHDR));
        if (m->error) {
            m->hdr = NULL;
            return pmHostError;
        }
        memcpy(big->lpData, msg_buffer, m->sysex_byte_count);
        msg_buffer = (unsigned char *) (big->lpData);
        if (m->buffers[0] == m->hdr) {
            m->buffers[0] = big;
            pm_free(m->hdr);
            /* printf("freed m->hdr\n"); */
        } else if (m->buffers[1] == m->hdr) {
            m->buffers[1] = big;
            pm_free(m->hdr);
            /* printf("freed m->hdr\n"); */
        }
        m->hdr = big;
    }

    /* append byte to message */
    msg_buffer[m->sysex_byte_count++] = byte;

    /* see if we have a complete message */
    if (byte == MIDI_EOX) {
        m->hdr->dwBytesRecorded = m->sysex_byte_count;
        /*
        { int i; int len = m->hdr->dwBytesRecorded;
          printf("OutLongMsg %d ", len);
          for (i = 0; i < len; i++) {
              printf("%2x ", msg_buffer[i]);
          }
        }
        */
        m->error = midiOutLongMsg(m->handle.out, m->hdr, sizeof(MIDIHDR));
        m->hdr = NULL; /* stop using this message buffer */
        if (m->error) return pmHostError;
    }
    return pmNoError;
}
#endif


static PmError winmm_write_short(PmInternal *midi, PmEvent *event)
{
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    PmError rslt = pmNoError;
    assert(m);
    if (midi->latency == 0) { /* use midiOut interface, ignore timestamps */
        m->error = midiOutShortMsg(m->handle.out, event->message);
        if (m->error) rslt = pmHostError;
    } else {  /* use midiStream interface -- pass data through buffers */
        unsigned long when = event->timestamp;
        unsigned long delta;
        int full;
        if (when == 0) when = midi->now;
        /* when is in real_time; translate to intended stream time */
        when = when + m->delta + midi->latency;
        /* make sure we don't go backward in time */
        if (when < m->last_time) when = m->last_time;
        delta = when - m->last_time;
        m->last_time = when;
        /* before we insert any data, we must have a buffer */
        if (m->hdr == NULL) {
            /* stream interface: buffers allocated when stream is opened */
            m->hdr = get_free_output_buffer(midi);
        }
        full = add_to_buffer(m, m->hdr, delta, event->message);
        if (full) {
            m->error = midiStreamOut(m->handle.stream, m->hdr,
                                     sizeof(MIDIHDR));
            if (m->error) rslt = pmHostError;
            m->hdr = NULL;
        }
    }
    return rslt;
}


static PmError winmm_begin_sysex(PmInternal *midi, PmTimestamp timestamp)
{
    PmError rslt = pmNoError;
    if (midi->latency == 0) {
        /* do nothing -- it's handled in winmm_write_byte */
    } else {
        midiwinmm_type m = (midiwinmm_type) midi->descriptor;
        /* sysex expects an empty buffer */
        if (m->hdr) {
            m->error = midiStreamOut(m->handle.stream, m->hdr, sizeof(MIDIHDR));
            if (m->error) rslt = pmHostError;
        }
        m->hdr = NULL;
    }
    return rslt;
}


static PmError winmm_end_sysex(PmInternal *midi, PmTimestamp timestamp)
{
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    PmError rslt = pmNoError;
    assert(m);

    if (midi->latency == 0) {
        /* Not using the stream interface. The entire sysex message is
           in m->hdr, and we send it using midiOutLongMsg.
         */
        m->hdr->dwBytesRecorded = m->sysex_byte_count;
        /*
        { int i; int len = m->hdr->dwBytesRecorded;
          printf("OutLongMsg %d ", len);
          for (i = 0; i < len; i++) {
              printf("%2x ", msg_buffer[i]);
          }
        }
        */

        m->error = midiOutLongMsg(m->handle.out, m->hdr, sizeof(MIDIHDR));
        if (m->error) rslt = pmHostError;
    } else if (m->hdr) {
        /* Using stream interface. There are accumulated bytes in m->hdr
           to send using midiStreamOut
         */
        /* add bytes recorded to MIDIEVENT length, but don't
           count the MIDIEVENT data (3 longs) */
        MIDIEVENT *evt = (MIDIEVENT *) m->hdr->lpData;
        evt->dwEvent += m->hdr->dwBytesRecorded - 3 * sizeof(long);
        /* round up BytesRecorded to multiple of 4 */
        m->hdr->dwBytesRecorded = (m->hdr->dwBytesRecorded + 3) & ~3;

        m->error = midiStreamOut(m->handle.stream, m->hdr,
                                 sizeof(MIDIHDR));
        if (m->error) rslt = pmHostError;
    }
    m->hdr = NULL; /* make sure we don't send it again */
    return rslt;
}


static PmError winmm_write_byte(PmInternal *midi, unsigned char byte,
                                PmTimestamp timestamp)
{
    /* write a sysex byte */
    PmError rslt = pmNoError;
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    assert(m);
    if (midi->latency == 0) {
        /* Not using stream interface. Accumulate the entire message into
           m->hdr */
        unsigned char *msg_buffer;
        /* at the beginning of sysex, m->hdr is NULL */
        if (!m->hdr) { /* allocate a buffer if none allocated yet */
            m->hdr = get_free_sysex_buffer(midi);
            if (!m->hdr) return pmInsufficientMemory;
            m->sysex_byte_count = 0;
        }
        /* figure out where to write byte */
        msg_buffer = (unsigned char *) (m->hdr->lpData);
        assert(m->hdr->lpData == (char *) (m->hdr + 1));

        /* append byte to message */
        msg_buffer[m->sysex_byte_count++] = byte;

        /* check for overflow */
        if (m->sysex_byte_count >= m->hdr->dwBufferLength) {
            rslt = resize_sysex_buffer(midi, m->sysex_byte_count, m->sysex_byte_count * 2);

            if (rslt == pmBufferMaxSize) /* if the buffer can't be resized */
                rslt = winmm_end_sysex(midi, timestamp); /* write what we've got and continue */

        }

    } else { /* latency is not zero, use stream interface: accumulate
                        sysex data in m->hdr and send whenever the buffer fills */
        int full;
        unsigned char *ptr;

        /* if m->hdr does not exist, allocate it */
        if (m->hdr == NULL) {
            unsigned long when = (unsigned long) timestamp;
            unsigned long delta;
            unsigned long *ptr;
            if (when == 0) when = midi->now;
            /* when is in real_time; translate to intended stream time */
            when = when + m->delta + midi->latency;
            /* make sure we don't go backward in time */
            if (when < m->last_time) when = m->last_time;
            delta = when - m->last_time;
            m->last_time = when;

            m->hdr = get_free_sysex_buffer(midi);
            assert(m->hdr);
            ptr = (unsigned long *) m->hdr->lpData;
            *ptr++ = delta;
            *ptr++ = 0;
            *ptr = MEVT_F_LONG;
            m->hdr->dwBytesRecorded = 3 * sizeof(long);
        }

        /* add the data byte */
        ptr = (unsigned char *) (m->hdr->lpData + m->hdr->dwBytesRecorded);
        *ptr = byte;
        full = ++m->hdr->dwBytesRecorded >= m->hdr->dwBufferLength;

        /* see if we need to resize */
        if (full) {
            int bytesRecorded = m->hdr->dwBytesRecorded; /* this field gets wiped out, so we'll save it */
            rslt = resize_sysex_buffer(midi, bytesRecorded, 2 * bytesRecorded);
            m->hdr->dwBytesRecorded = bytesRecorded;

            if (rslt == pmBufferMaxSize) /* if buffer can't be resized */
                rslt = winmm_end_sysex(midi, timestamp); /* write what we've got and continue */
        }
    }
    return rslt;
}



static PmError winmm_write_flush(PmInternal *midi)
{
    PmError rslt = pmNoError;
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    assert(m);
    if (midi->latency == 0) {
        /* all messages are sent immediately */
    } else if ((m->hdr) && (!midi->sysex_in_progress)) {
        /* sysex messages are sent upon completion, but ordinary messages
           may be sitting in a buffer
         */
        m->error = midiStreamOut(m->handle.stream, m->hdr, sizeof(MIDIHDR));
        m->hdr = NULL;
        if (m->error) rslt = pmHostError;
    }
    return rslt;
}

static PmTimestamp winmm_synchronize(PmInternal *midi)
{
    midiwinmm_type m;
    unsigned long pm_stream_time_2;
    unsigned long real_time;
    unsigned long pm_stream_time;

    /* only synchronize if we are using stream interface */
    if (midi->latency == 0) return 0;

    /* figure out the time */
    m = (midiwinmm_type) midi->descriptor;
    pm_stream_time_2 = pm_time_get(m);

    do {
        /* read real_time between two reads of stream time */
        pm_stream_time = pm_stream_time_2;
        real_time = (*midi->time_proc)(midi->time_info);
        pm_stream_time_2 = pm_time_get(m);
        /* repeat if more than 1ms elapsed */
    } while (pm_stream_time_2 > pm_stream_time + 1);
    m->delta = pm_stream_time - real_time;
    m->sync_time = real_time;
    return real_time;
}


#ifdef GARBAGE
static PmError winmm_write(PmInternal *midi,
                           PmEvent *buffer,
                           long length)
{
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    unsigned long now;
    int i;
    long msg;
    PmError rslt = pmNoError;

    m->error = MMSYSERR_NOERROR;
    if (midi->latency == 0) { /* use midiOut interface, ignore timestamps */
        for (i = 0; (i < length) && (rslt == pmNoError); i++) {
            int b = 0; /* count sysex bytes as they are handled */
            msg = buffer[i].message;
            if ((msg & 0xFF) == MIDI_SYSEX) {
                /* start a sysex message */
                m->sysex_mode = TRUE;
                unsigned char midi_byte = (unsigned char) msg;
                rslt = winmm_write_sysex_byte(midi, midi_byte);
                b = 8;
            } else if ((msg & 0x80) && ((msg & 0xFF) != MIDI_EOX)) {
                /* a non-sysex message */
                m->error = midiOutShortMsg(m->handle.out, msg);
                if (m->error) rslt = pmHostError;
                /* any non-real-time message will terminate sysex message */
                if (!is_real_time(msg)) m->sysex_mode = FALSE;
            }
            /* transmit sysex bytes until we find EOX */
            if (m->sysex_mode) {
                while (b < 32 /*bits*/ && (rslt == pmNoError)) {
                    unsigned char midi_byte = (unsigned char) (msg >> b);
                    rslt = winmm_write_sysex_byte(midi, midi_byte);
                    if (midi_byte == MIDI_EOX) {
                        b = 24; /* end of message */
                        m->sysex_mode = FALSE;
                    }
                    b += 8;
                }
            }
        }
    } else { /* use midiStream interface -- pass data through buffers */
        LPMIDIHDR hdr = NULL;
        now = (*midi->time_proc)(midi->time_info);
        if (m->first_message || m->sync_time + 100 /*ms*/ < now) {
            /* time to resync */
            now = synchronize(midi, m);
            m->first_message = FALSE;
        }
        for (i = 0; i < length && rslt == pmNoError; i++) {
            unsigned long when = buffer[i].timestamp;
            unsigned long delta;
            if (when == 0) when = now;
            /* when is in real_time; translate to intended stream time */
            when = when + m->delta + midi->latency;
            /* make sure we don't go backward in time */
            if (when < m->last_time) when = m->last_time;
            delta = when - m->last_time;
            m->last_time = when;
            /* before we insert any data, we must have a buffer */
            if (hdr == NULL) {
                /* stream interface: buffers allocated when stream is opened */
                hdr = get_free_output_buffer(midi);
                assert(hdr);
                if (m->sysex_mode) {
                    /* we are in the middle of a sysex message */
                    start_sysex_buffer(hdr, delta);
                }
            }
            msg = buffer[i].message;
            if ((msg & 0xFF) == MIDI_SYSEX) {
                /* sysex expects an empty buffer */
                if (hdr->dwBytesRecorded != 0) {
                    m->error = midiStreamOut(m->handle.stream, hdr, sizeof(MIDIHDR));
                    if (m->error) rslt = pmHostError;
                    hdr = get_free_output_buffer(midi);
                    assert(hdr);
                }
                /* when we see a MIDI_SYSEX, we always enter sysex mode and call
                   start_sysex_buffer() */
                start_sysex_buffer(hdr, delta);
                m->sysex_mode = TRUE;
            }
            /* allow a non-real-time status byte to terminate sysex message */
            if (m->sysex_mode && (msg & 0x80) && (msg & 0xFF) != MIDI_SYSEX &&
                    !is_real_time(msg)) {
                /* I'm not sure what WinMM does if you send an incomplete sysex
                   message, but the best way out of this mess seems to be to
                   recreate the code used when you encounter an EOX, so ...
                 */
                MIDIEVENT *evt = (MIDIEVENT) hdr->lpData;
                evt->dwEvent += hdr->dwBytesRecorded - 3 * sizeof(long);
                /* round up BytesRecorded to multiple of 4 */
                hdr->dwBytesRecorded = (hdr->dwBytesRecorded + 3) & ~3;
                m->error = midiStreamOut(m->handle.stream, hdr,
                                         sizeof(MIDIHDR));
                if (m->error) {
                    rslt = pmHostError;
                }
                hdr = NULL; /* make sure we don't send it again */
                m->sysex_mode = FALSE; /* skip to normal message send code */
            }
            if (m->sysex_mode) {
                int b = 0; /* count bytes as they are handled */
                while (b < 32 /* bits per word */ && (rslt == pmNoError)) {
                    int full;
                    unsigned char midi_byte = (unsigned char) (msg >> b);
                    if (!hdr) {
                        hdr = get_free_output_buffer(midi);
                        assert(hdr);
                        /* get ready to put sysex bytes in buffer */
                        start_sysex_buffer(hdr, delta);
                    }
                    full = add_byte_to_buffer(m, hdr, midi_byte);
                    if (midi_byte == MIDI_EOX) {
                        b = 24; /* pretend this is last byte to exit loop */
                        m->sysex_mode = FALSE;
                    }
                    /* see if it's time to send buffer, note that by always
                       sending complete sysex message right away, we can use
                       this code to set up the MIDIEVENT properly
                     */
                    if (full || midi_byte == MIDI_EOX) {
                        /* add bytes recorded to MIDIEVENT length, but don't
                           count the MIDIEVENT data (3 longs) */
                        MIDIEVENT *evt = (MIDIEVENT *) hdr->lpData;
                        evt->dwEvent += hdr->dwBytesRecorded - 3 * sizeof(long);
                        /* round up BytesRecorded to multiple of 4 */
                        hdr->dwBytesRecorded = (hdr->dwBytesRecorded + 3) & ~3;
                        m->error = midiStreamOut(m->handle.stream, hdr,
                                                 sizeof(MIDIHDR));
                        if (m->error) {
                            rslt = pmHostError;
                        }
                        hdr = NULL; /* make sure we don't send it again */
                    }
                    b += 8; /* shift to next byte */
                }
                /* test rslt here in case it was set when we terminated a sysex early
                   (see above) */
            } else if (rslt == pmNoError) {
                int full = add_to_buffer(m, hdr, delta, msg);
                if (full) {
                    m->error = midiStreamOut(m->handle.stream, hdr,
                                             sizeof(MIDIHDR));
                    if (m->error) rslt = pmHostError;
                    hdr = NULL;
                }
            }
        }
        if (hdr && rslt == pmNoError) {
            if (m->sysex_mode) {
                MIDIEVENT *evt = (MIDIEVENT *) hdr->lpData;
                evt->dwEvent += hdr->dwBytesRecorded - 3 * sizeof(long);
                /* round up BytesRecorded to multiple of 4 */
                hdr->dwBytesRecorded = (hdr->dwBytesRecorded + 3) & ~3;
            }
            m->error = midiStreamOut(m->handle.stream, hdr, sizeof(MIDIHDR));
            if (m->error) rslt = pmHostError;
        }
    }
    return rslt;
}
#endif


/* winmm_out_callback -- recycle sysex buffers */
static void CALLBACK winmm_out_callback(HMIDIOUT hmo, UINT wMsg,
                                        DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    int i;
    PmInternal *midi = (PmInternal *) dwInstance;
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    LPMIDIHDR hdr = (LPMIDIHDR) dwParam1;
    int err = 0;  /* set to 0 so that no buffer match will also be an error */
    static int entry = 0;
    if (++entry > 1) {
        assert(FALSE);
    }
    if (m->callback_error || wMsg != MOM_DONE) {
        entry--;
        return ;
    }
    /* Future optimization: eliminate UnprepareHeader calls -- they aren't
       necessary; however, this code uses the prepared-flag to indicate which
       buffers are free, so we need to do something to flag empty buffers if
       we leave them prepared
     */
    m->callback_error = midiOutUnprepareHeader(m->handle.out, hdr,
                        sizeof(MIDIHDR));
    /* notify waiting sender that a buffer is available */
    /* any errors could be reported via callback_error, but this is always
       treated as a Midi error, so we'd have to write a lot more code to
       detect that a non-Midi error occurred and do the right thing to find
       the corresponding error message text. Therefore, just use assert()
     */

    /* determine if this is an output buffer or a sysex buffer */

    for (i = 0 ;i < m->num_buffers;i++) {
        if (hdr == m->buffers[i]) {
            err = SetEvent(m->buffer_signal);
            break;
        }
    }
    for (i = 0 ;i < m->num_sysex_buffers;i++) {
        if (hdr == m->sysex_buffers[i]) {
            err = SetEvent(m->sysex_buffer_signal);
            break;
        }
    }
    assert(err); /* false -> error */
    entry--;
}


/* winmm_streamout_callback -- unprepare (free) buffer header */
static void CALLBACK winmm_streamout_callback(HMIDIOUT hmo, UINT wMsg,
        DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    PmInternal *midi = (PmInternal *) dwInstance;
    midiwinmm_type m = (midiwinmm_type) midi->descriptor;
    LPMIDIHDR hdr = (LPMIDIHDR) dwParam1;
    int err;
    static int entry = 0;
    if (++entry > 1) {
        /* We've reentered this routine. I assume this never happens, but
           check to make sure. Apparently, it is possible that this callback
           can be called reentrantly because it happened once while debugging.
           It looks like this routine is actually reentrant so we can remove
           the assertion if necessary. */
        assert(FALSE);
    }
    if (m->callback_error || wMsg != MOM_DONE) {
        entry--;
        return ;
    }
    m->callback_error = midiOutUnprepareHeader(m->handle.out, hdr,
                        sizeof(MIDIHDR));
    err = SetEvent(m->buffer_signal);
    assert(err); /* false -> error */
    entry--;
}


/*
=========================================================================================
begin exported functions
=========================================================================================
*/

#define winmm_in_abort pm_fail_fn
pm_fns_node pm_winmm_in_dictionary = {
                                         none_write_short,
                                         none_sysex,
                                         none_sysex,
                                         none_write_byte,
                                         none_write_short,
                                         none_write_flush,
                                         winmm_synchronize,
                                         winmm_in_open,
                                         winmm_in_abort,
                                         winmm_in_close,
                                         success_poll,
                                         winmm_has_host_error,
                                         winmm_get_host_error
                                     };

pm_fns_node pm_winmm_out_dictionary = {
                                          winmm_write_short,
                                          winmm_begin_sysex,
                                          winmm_end_sysex,
                                          winmm_write_byte,
                                          winmm_write_short,  /* short realtime message */
                                          winmm_write_flush,
                                          winmm_synchronize,
                                          winmm_out_open,
                                          winmm_out_abort,
                                          winmm_out_close,
                                          none_poll,
                                          winmm_has_host_error,
                                          winmm_get_host_error
                                      };


/* initialize winmm interface. Note that if there is something wrong
   with winmm (e.g. it is not supported or installed), it is not an
   error. We should simply return without having added any devices to
   the table. Hence, no error code is returned. Furthermore, this init
   code is called along with every other supported interface, so the
   user would have a very hard time figuring out what hardware and API
   generated the error. Finally, it would add complexity to pmwin.c to
   remember where the error code came from in order to convert to text.
 */
void pm_winmm_init( void )
{
    pm_winmm_mapper_input();
    pm_winmm_mapper_output();
    pm_winmm_general_inputs();
    pm_winmm_general_outputs();
}


/* no error codes are returned, even if errors are encountered, because
   there is probably nothing the user could do (e.g. it would be an error
   to retry.
 */
void pm_winmm_term( void )
{
    int i;
#ifdef DEBUG
    char msg[PM_HOST_ERROR_MSG_LEN];
#endif
    int doneAny = 0;
#ifdef DEBUG
    printf("pm_winmm_term called\n");
#endif
    for (i = 0; i < pm_descriptor_index; i++) {
        PmInternal * midi = descriptors[i].internalDescriptor;
        if (midi) {
            midiwinmm_type m = (midiwinmm_type) midi->descriptor;
            if (m->handle.out) {
                /* close next open device*/
#ifdef DEBUG
                if (doneAny == 0) {
                    printf("begin closing open devices...\n");
                    doneAny = 1;
                }
                /* report any host errors; this EXTEREMELY useful when
                   trying to debug client app */
                if (winmm_has_host_error(midi)) {
                    winmm_get_host_error(midi, msg, PM_HOST_ERROR_MSG_LEN);
                    printf(msg);
                }
#endif
                /* close all open ports */
                (*midi->dictionary->close)(midi);
            }
        }
    }
#ifdef DEBUG
    if (doneAny) {
        printf("warning: devices were left open. They have been closed.\n");
    }
    printf("pm_winmm_term exiting\n");
#endif
    pm_descriptor_index = 0;
}
