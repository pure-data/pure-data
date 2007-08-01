#include "stdlib.h"
#include "string.h"
#include "portmidi.h"
#include "porttime.h"
#include "pminternal.h"
#include <assert.h>

#define MIDI_CLOCK      0xf8
#define MIDI_ACTIVE     0xfe
#define MIDI_STATUS_MASK 0x80
#define MIDI_SYSEX      0xf0
#define MIDI_EOX        0xf7
#define MIDI_START      0xFA
#define MIDI_STOP       0xFC
#define MIDI_CONTINUE   0xFB
#define MIDI_F9         0xF9
#define MIDI_FD         0xFD
#define MIDI_RESET      0xFF
#define MIDI_NOTE_ON    0x90
#define MIDI_NOTE_OFF   0x80
#define MIDI_CHANNEL_AT 0xD0
#define MIDI_POLY_AT    0xA0
#define MIDI_PROGRAM    0xC0
#define MIDI_CONTROL    0xB0
#define MIDI_PITCHBEND  0xE0
#define MIDI_MTC        0xF1
#define MIDI_SONGPOS    0xF2
#define MIDI_SONGSEL    0xF3
#define MIDI_TUNE       0xF6

#define is_empty(midi) ((midi)->tail == (midi)->head)

static int pm_initialized = FALSE;
int pm_hosterror = FALSE;
char pm_hosterror_text[PM_HOST_ERROR_MSG_LEN];

#ifdef PM_CHECK_ERRORS

#include <stdio.h>

#define STRING_MAX 80

static void prompt_and_exit(void)
{
    char line[STRING_MAX];
    printf("type ENTER...");
    fgets(line, STRING_MAX, stdin);
    /* this will clean up open ports: */
    exit(-1);
}


static PmError pm_errmsg(PmError err)
{
    if (err == pmHostError) {
        /* it seems pointless to allocate memory and copy the string,
         * so I will do the work of Pm_GetHostErrorText directly
         */
        printf("PortMidi found host error...\n  %s\n", pm_hosterror_text);
        pm_hosterror = FALSE;
        pm_hosterror_text[0] = 0; /* clear the message */
        prompt_and_exit();
    } else if (err < 0) {
        printf("PortMidi call failed...\n  %s\n", Pm_GetErrorText(err));
        prompt_and_exit();
    }
    return err;
}
#else
#define pm_errmsg(err) err
#endif

/*
====================================================================
system implementation of portmidi interface
====================================================================
*/

int pm_descriptor_max = 0;
int pm_descriptor_index = 0;
descriptor_type descriptors = NULL;

/* pm_add_device -- describe interface/device pair to library
 *
 * This is called at intialization time, once for each
 * interface (e.g. DirectSound) and device (e.g. SoundBlaster 1)
 * The strings are retained but NOT COPIED, so do not destroy them!
 *
 * returns pmInvalidDeviceId if device memory is exceeded
 * otherwise returns pmNoError
 */
PmError pm_add_device(char *interf, char *name, int input,
                      void *descriptor, pm_fns_type dictionary) {
    if (pm_descriptor_index >= pm_descriptor_max) {
        // expand descriptors
        descriptor_type new_descriptors =
                pm_alloc(sizeof(descriptor_node) * (pm_descriptor_max + 32));
        if (!new_descriptors) return pmInsufficientMemory;
        if (descriptors) {
            memcpy(new_descriptors, descriptors,
                   sizeof(descriptor_node) * pm_descriptor_max);
            free(descriptors);
        }
        pm_descriptor_max += 32;
        descriptors = new_descriptors;
    }
    descriptors[pm_descriptor_index].pub.interf = interf;
    descriptors[pm_descriptor_index].pub.name = name;
    descriptors[pm_descriptor_index].pub.input = input;
    descriptors[pm_descriptor_index].pub.output = !input;

    /* default state: nothing to close (for automatic device closing) */
    descriptors[pm_descriptor_index].pub.opened = FALSE;

    /* ID number passed to win32 multimedia API open */
    descriptors[pm_descriptor_index].descriptor = descriptor;

    /* points to PmInternal, allows automatic device closing */
    descriptors[pm_descriptor_index].internalDescriptor = NULL;

    descriptors[pm_descriptor_index].dictionary = dictionary;

    pm_descriptor_index++;

    return pmNoError;
}


/*
====================================================================
portmidi implementation
====================================================================
*/

int Pm_CountDevices( void )
{
    PmError err = Pm_Initialize();
    if (err)
        return pm_errmsg(err);
    return pm_descriptor_index;
}


const PmDeviceInfo* Pm_GetDeviceInfo( PmDeviceID id )
{
    PmError err = Pm_Initialize();
    if (err)
        return NULL;
    if (id >= 0 && id < pm_descriptor_index) {
        return &descriptors[id].pub;
    }
    return NULL;
}

/* pm_success_fn -- "noop" function pointer */
PmError pm_success_fn(PmInternal *midi) {
    return pmNoError;
}

/* none_write -- returns an error if called */
PmError none_write_short(PmInternal *midi, PmEvent *buffer)
{
    return pmBadPtr;
}

/* none_sysex -- placeholder for begin_sysex and end_sysex */
PmError none_sysex(PmInternal *midi, PmTimestamp timestamp)
{
    return pmBadPtr;
}

PmError none_write_byte(PmInternal *midi, unsigned char byte,
                        PmTimestamp timestamp)
{
    return pmBadPtr;
}

/* pm_fail_fn -- generic function, returns error if called */
PmError pm_fail_fn(PmInternal *midi)
{
    return pmBadPtr;
}

static PmError none_open(PmInternal *midi, void *driverInfo)
{
    return pmBadPtr;
}
static void none_get_host_error(PmInternal * midi, char * msg, unsigned int len) {
    strcpy(msg,"");
}
static unsigned int none_has_host_error(PmInternal * midi) {
    return FALSE;
}
PmTimestamp none_synchronize(PmInternal *midi) {
    return 0;
}

#define none_abort pm_fail_fn
#define none_close pm_fail_fn

pm_fns_node pm_none_dictionary = {
    none_write_short,
    none_sysex,
    none_sysex,
    none_write_byte,
    none_write_short,
    none_write_flush,
    none_synchronize,
    none_open,
    none_abort,
    none_close,
    none_poll,
    none_has_host_error,
    none_get_host_error
};


const char *Pm_GetErrorText( PmError errnum ) {
    const char *msg;

    switch(errnum)
    {
    case pmNoError:
        msg = "";
        break;
    case pmHostError:
        msg = "PortMidi: `Host error'";
        break;
    case pmInvalidDeviceId:
        msg = "PortMidi: `Invalid device ID'";
        break;
    case pmInsufficientMemory:
        msg = "PortMidi: `Insufficient memory'";
        break;
    case pmBufferTooSmall:
        msg = "PortMidi: `Buffer too small'";
        break;
    case pmBadPtr:
        msg = "PortMidi: `Bad pointer'";
        break;
    case pmInternalError:
        msg = "PortMidi: `Internal PortMidi Error'";
        break;
    case pmBufferOverflow:
        msg = "PortMidi: `Buffer overflow'";
        break;
    case pmBadData:
        msg = "PortMidi: `Invalid MIDI message Data'";
    default:
        msg = "PortMidi: `Illegal error number'";
        break;
    }
    return msg;
}


/* This can be called whenever you get a pmHostError return value.
 * The error will always be in the global pm_hosterror_text.
 */
void Pm_GetHostErrorText(char * msg, unsigned int len) {
    assert(msg);
    assert(len > 0);
    if (pm_hosterror) { /* we have the string already from open or close */
        strncpy(msg, (char *) pm_hosterror_text, len);
        pm_hosterror = FALSE;
        pm_hosterror_text[0] = 0; /* clear the message; not necessary, but it
                                 might help with debugging */
        msg[len - 1] = 0; /* make sure string is terminated */
    } else {
        msg[0] = 0; /* no string to return */
    }
}


int Pm_HasHostError(PortMidiStream * stream) {
    if (stream) {
        PmInternal * midi = (PmInternal *) stream;
        pm_hosterror = (*midi->dictionary->has_host_error)(midi);
        if (pm_hosterror) {
            midi->dictionary->host_error(midi, pm_hosterror_text,
                                         PM_HOST_ERROR_MSG_LEN);
            /* now error message is global */
            return TRUE;
        }
    }
    return FALSE;
}


PmError Pm_Initialize( void ) {
    pm_hosterror_text[0] = 0; /* the null string */
    if (!pm_initialized) {
        pm_init();
        pm_initialized = TRUE;
    }
    return pmNoError;
}


PmError Pm_Terminate( void ) {
    if (pm_initialized) {
        pm_term();
        pm_initialized = FALSE;
    }
    return pmNoError;
}


/* Pm_Read -- read up to length longs from source into buffer */
/*
   returns number of longs actually read, or error code

   When the reader wants data:
     if overflow_flag:
         do not get anything
         empty the buffer (read_ptr = write_ptr)
         clear overflow_flag
         return pmBufferOverflow
     get data
     return number of messages
*/
PmError Pm_Read(PortMidiStream *stream, PmEvent *buffer, long length) {
    PmInternal *midi = (PmInternal *) stream;
    int n = 0;
    long head;
    PmError err = pmNoError;

    /* arg checking */
    if(midi == NULL)
        err = pmBadPtr;
    else if(Pm_HasHostError(midi))
        err = pmHostError;
    else if(!descriptors[midi->device_id].pub.opened)
        err = pmBadPtr;
    else if(!descriptors[midi->device_id].pub.input)
        err = pmBadPtr;

    /* First poll for data in the buffer...
     * This either simply checks for data, or attempts first to fill the buffer
     * with data from the MIDI hardware; this depends on the implementation.
     * We could call Pm_Poll here, but that would redo a lot of redundant
     * parameter checking, so I copied some code from Pm_Poll to here: */
    else err = (*(midi->dictionary->poll))(midi);

    if (err != pmNoError) {
        return pm_errmsg(err);
    }

    head = midi->head;
    while (head != midi->tail && n < length) {
        PmEvent event = midi->buffer[head++];
        *buffer++ = event;
        if (head == midi->buffer_len) head = 0;
        n++;
    }
    midi->head = head;
    if (midi->overflow) {
        midi->head = midi->tail;
        midi->overflow = FALSE;
        return pm_errmsg(pmBufferOverflow);
    }
    return n;
}

PmError Pm_Poll( PortMidiStream *stream )
{
    PmInternal *midi = (PmInternal *) stream;
    PmError err;

    /* arg checking */
    if(midi == NULL)
        err = pmBadPtr;
    else if(Pm_HasHostError(midi))
        err = pmHostError;
    else if(!descriptors[midi->device_id].pub.opened)
        err = pmBadPtr;
    else if(!descriptors[midi->device_id].pub.input)
        err = pmBadPtr;
    else
        err = (*(midi->dictionary->poll))(midi);

    if (err != pmNoError)
        return pm_errmsg(err);
    else
        return midi->head != midi->tail;
}

/* to facilitate correct error-handling, Pm_Write, Pm_WriteShort, and
   Pm_WriteSysEx all operate a state machine that "outputs" calls to
   write_short, begin_sysex, write_byte, end_sysex, and write_realtime */

PmError Pm_Write( PortMidiStream *stream, PmEvent *buffer, long length)
{
    PmInternal *midi = (PmInternal *) stream;
    PmError err;
    int i;
    int bits;

    /* arg checking */
    if(midi == NULL)
        err = pmBadPtr;
    else if(Pm_HasHostError(midi))
        err = pmHostError;
    else if(!descriptors[midi->device_id].pub.opened)
        err = pmBadPtr;
    else if(!descriptors[midi->device_id].pub.output)
        err = pmBadPtr;
    else
        err = pmNoError;

    if (err != pmNoError) goto pm_write_error;

    if (midi->latency == 0) {
        midi->now = 0;
    } else {
        midi->now = (*(midi->time_proc))(midi->time_info);
        if (midi->first_message || midi->sync_time + 100 /*ms*/ < midi->now) {
            /* time to resync */
            midi->now = (*midi->dictionary->synchronize)(midi);
            midi->first_message = FALSE;
        }
    }

    for (i = 0; i < length; i++) {
        unsigned long msg = buffer[i].message;
        bits = 0;
        /* is this a sysex message? */
        if (Pm_MessageStatus(msg) == MIDI_SYSEX) {
            if (midi->sysex_in_progress) {
                /* error: previous sysex was not terminated by EOX */
                midi->sysex_in_progress = FALSE;
                err = pmBadData;
                goto pm_write_error;
            }
            midi->sysex_in_progress = TRUE;
            if ((err = (*midi->dictionary->begin_sysex)(midi,
                               buffer[i].timestamp)) != pmNoError)
                goto pm_write_error;
            if ((err = (*midi->dictionary->write_byte)(midi, MIDI_SYSEX,
                               buffer[i].timestamp)) != pmNoError)
                goto pm_write_error;
            bits = 8;
            /* fall through to continue sysex processing */
        } else if ((msg & MIDI_STATUS_MASK) &&
                   (Pm_MessageStatus(msg) != MIDI_EOX)) {
            /* a non-sysex message */
            if (midi->sysex_in_progress) {
                /* this should be a non-realtime message */
                if (is_real_time(msg)) {
                    if ((err = (*midi->dictionary->write_realtime)(midi,
                                       &(buffer[i]))) != pmNoError)
                        goto pm_write_error;
                } else {
                    midi->sysex_in_progress = FALSE;
                    err = pmBadData;
                    /* ignore any error from this, because we already have one */
                    /* pass 0 as timestamp -- it's ignored */
                    (*midi->dictionary->end_sysex)(midi, 0);
                    goto pm_write_error;
                }
            } else { /* regular short midi message */
                if ((err = (*midi->dictionary->write_short)(midi,
                                   &(buffer[i]))) != pmNoError)
                    goto pm_write_error;
                continue;
            }
        }
        if (midi->sysex_in_progress) { /* send sysex bytes until EOX */
            while (bits < 32) {
                unsigned char midi_byte = (unsigned char) (msg >> bits);
                if ((err = (*midi->dictionary->write_byte)(midi, midi_byte,
                                   buffer[i].timestamp)) != pmNoError)
                    goto pm_write_error;
                if (midi_byte == MIDI_EOX) {
                    midi->sysex_in_progress = FALSE;
                    if ((err = (*midi->dictionary->end_sysex)(midi,
                                       buffer[i].timestamp)) != pmNoError)
                        goto pm_write_error;
                    break; /* from while loop */
                }
                bits += 8;
            }
        } else {
            /* not in sysex mode, but message did not start with status */
            err = pmBadData;
            goto pm_write_error;
        }
    }
    /* after all messages are processed, send the data */
    err = (*midi->dictionary->write_flush)(midi);
pm_write_error:
    return pm_errmsg(err);
}


PmError Pm_WriteShort( PortMidiStream *stream, long when, long msg)
{
    PmEvent event;

    event.timestamp = when;
    event.message = msg;
    return Pm_Write(stream, &event, 1);
}


PmError Pm_WriteSysEx(PortMidiStream *stream, PmTimestamp when,
                      unsigned char *msg)
{
    /* allocate buffer space for PM_DEFAULT_SYSEX_BUFFER_SIZE bytes */
    /* each PmEvent holds sizeof(PmMessage) bytes of sysex data */
#define BUFLEN (PM_DEFAULT_SYSEX_BUFFER_SIZE / sizeof(PmMessage))
    PmEvent buffer[BUFLEN];
    /* the next byte in the buffer is represented by an index, bufx, and
       a shift in bits */
    int shift = 0;
    int bufx = 0;
    buffer[0].message = 0;
    buffer[0].timestamp = when;

    while (1) {
        /* insert next byte into buffer */
        buffer[bufx].message |= ((*msg) << shift);
        shift += 8;
        if (shift == 32) {
            shift = 0;
            bufx++;
            if (bufx == BUFLEN) {
                PmError err = Pm_Write(stream, buffer, BUFLEN);
                if (err) return err;
                /* prepare to fill another buffer */
                bufx = 0;
            }
            buffer[bufx].message = 0;
            buffer[bufx].timestamp = when;
        }
        /* keep inserting bytes until you find MIDI_EOX */
        if (*msg++ == MIDI_EOX) break;
    }

    /* we're finished sending full buffers, but there may
     * be a partial one left.
     */
    if (shift != 0) bufx++; /* add partial message to buffer len */
    if (bufx) { /* bufx is number of PmEvents to send from buffer */
        return Pm_Write(stream, buffer, bufx);
    }
    return pmNoError;
}



PmError Pm_OpenInput(PortMidiStream** stream,
                     PmDeviceID inputDevice,
                     void *inputDriverInfo,
                     long bufferSize,
                     PmTimeProcPtr time_proc,
                     void *time_info)
{
    PmInternal *midi;
    PmError err = pmNoError;
    pm_hosterror = FALSE;
    *stream = NULL;

    /* arg checking */
    if (inputDevice < 0 || inputDevice >= pm_descriptor_index)
        err = pmInvalidDeviceId;
    else if (!descriptors[inputDevice].pub.input)
        err =  pmBadPtr;
    else if(descriptors[inputDevice].pub.opened)
        err =  pmBadPtr;

    if (err != pmNoError)
        goto error_return;

    /* create portMidi internal data */
    midi = (PmInternal *) pm_alloc(sizeof(PmInternal));
    *stream = midi;
    if (!midi) {
        err = pmInsufficientMemory;
        goto error_return;
    }
    midi->device_id = inputDevice;
    midi->write_flag = FALSE;
    midi->time_proc = time_proc;
    midi->time_info = time_info;
    /* windows adds timestamps in the driver and these are more accurate than
       using a time_proc, so do not automatically provide a time proc. Non-win
       implementations may want to provide a default time_proc in their
       system-specific midi_out_open() method.
     */
    if (bufferSize <= 0) bufferSize = 256; /* default buffer size */
    else bufferSize++; /* buffer holds N-1 msgs, so increase request by 1 */
    midi->buffer_len = bufferSize; /* portMidi input storage */
    midi->buffer = (PmEvent *) pm_alloc(sizeof(PmEvent) * midi->buffer_len);
    if (!midi->buffer) {
        /* free portMidi data */
        *stream = NULL;
        pm_free(midi);
        err = pmInsufficientMemory;
        goto error_return;
    }
    midi->head = 0;
    midi->tail = 0;
    midi->latency = 0; /* not used */
    midi->overflow = FALSE;
    midi->flush = FALSE;
    midi->sysex_in_progress = FALSE;
    midi->sysex_message = 0;
    midi->sysex_message_count = 0;
    midi->filters = PM_FILT_ACTIVE;
    midi->channel_mask = 0xFFFF;
    midi->sync_time = 0;
    midi->first_message = TRUE;
    midi->dictionary = descriptors[inputDevice].dictionary;
    descriptors[inputDevice].internalDescriptor = midi;
    /* open system dependent input device */
    err = (*midi->dictionary->open)(midi, inputDriverInfo);
    if (err) {
        *stream = NULL;
        descriptors[inputDevice].internalDescriptor = NULL;
        /* free portMidi data */
        pm_free(midi->buffer);
        pm_free(midi);
    } else {
        /* portMidi input open successful */
        descriptors[inputDevice].pub.opened = TRUE;
    }
error_return:
    return pm_errmsg(err);
}


PmError Pm_OpenOutput(PortMidiStream** stream,
                      PmDeviceID outputDevice,
                      void *outputDriverInfo,
                      long bufferSize,
                      PmTimeProcPtr time_proc,
                      void *time_info,
                      long latency)
{
    PmInternal *midi;
    PmError err = pmNoError;
    pm_hosterror = FALSE;
    *stream =  NULL;

    /* arg checking */
    if (outputDevice < 0 || outputDevice >= pm_descriptor_index)
        err = pmInvalidDeviceId;
    else if (!descriptors[outputDevice].pub.output)
        err = pmBadPtr;
    else if (descriptors[outputDevice].pub.opened)
        err = pmBadPtr;
    if (err != pmNoError)
        goto error_return;

    /* create portMidi internal data */
    midi = (PmInternal *) pm_alloc(sizeof(PmInternal));
    *stream = midi;
    if (!midi) {
        err = pmInsufficientMemory;
        goto error_return;
    }
    midi->device_id = outputDevice;
    midi->write_flag = TRUE;
    midi->time_proc = time_proc;
    /* if latency > 0, we need a time reference. If none is provided,
       use PortTime library */
    if (time_proc == NULL && latency != 0) {
        if (!Pt_Started())
            Pt_Start(1, 0, 0);
        /* time_get does not take a parameter, so coerce */
        midi->time_proc = (PmTimeProcPtr) Pt_Time;
    }
    midi->time_info = time_info;
    /* when stream used, this buffer allocated and used by
        winmm_out_open; deleted by winmm_out_close */
    midi->buffer_len = bufferSize;
    midi->buffer = NULL;
    midi->head = 0; /* unused by output */
    midi->tail = 0; /* unused by output */
    /* if latency zero, output immediate (timestamps ignored) */
    /* if latency < 0, use 0 but don't return an error */
    if (latency < 0) latency = 0;
    midi->latency = latency;
    midi->overflow = FALSE; /* not used */
    midi->flush = FALSE; /* not used */
    midi->sysex_in_progress = FALSE;
    midi->sysex_message = 0; /* unused by output */
    midi->sysex_message_count = 0; /* unused by output */
    midi->filters = 0; /* not used for output */
    midi->channel_mask = 0xFFFF; /* not used for output */
    midi->sync_time = 0;
    midi->first_message = TRUE;
    midi->dictionary = descriptors[outputDevice].dictionary;
    descriptors[outputDevice].internalDescriptor = midi;
    /* open system dependent output device */
    err = (*midi->dictionary->open)(midi, outputDriverInfo);
    if (err) {
        *stream = NULL;
        descriptors[outputDevice].internalDescriptor = NULL;
        /* free portMidi data */
        pm_free(midi);
    } else {
        /* portMidi input open successful */
        descriptors[outputDevice].pub.opened = TRUE;
    }
error_return:
    return pm_errmsg(err);
}

PmError Pm_SetChannelMask(PortMidiStream *stream, int mask)
{
    PmInternal *midi = (PmInternal *) stream;
    PmError err = pmNoError;

    if (midi == NULL)
        err = pmBadPtr;
    else
        midi->channel_mask = mask;

    return pm_errmsg(err);
}

PmError Pm_SetFilter(PortMidiStream *stream, long filters)
{
    PmInternal *midi = (PmInternal *) stream;
    PmError err = pmNoError;

    /* arg checking */
    if (midi == NULL)
        err = pmBadPtr;
    else if (!descriptors[midi->device_id].pub.opened)
        err = pmBadPtr;
    else
        midi->filters = filters;
    return pm_errmsg(err);
}


PmError Pm_Close( PortMidiStream *stream )
{
    PmInternal *midi = (PmInternal *) stream;
    PmError err = pmNoError;

    /* arg checking */
    if (midi == NULL) /* midi must point to something */
        err = pmBadPtr;
    /* if it is an open device, the device_id will be valid */
    else if (midi->device_id < 0 || midi->device_id >= pm_descriptor_index)
        err = pmBadPtr;
    /* and the device should be in the opened state */
    else if (!descriptors[midi->device_id].pub.opened)
        err = pmBadPtr;

    if (err != pmNoError)
        goto error_return;

    /* close the device */
    err = (*midi->dictionary->close)(midi);
    /* even if an error occurred, continue with cleanup */
    descriptors[midi->device_id].internalDescriptor = NULL;
    descriptors[midi->device_id].pub.opened = FALSE;
    pm_free(midi->buffer);
    pm_free(midi);
error_return:
    return pm_errmsg(err);
}


PmError Pm_Abort( PortMidiStream* stream )
{
    PmInternal *midi = (PmInternal *) stream;
    PmError err;
    /* arg checking */
    if (midi == NULL)
        err = pmBadPtr;
    if (!descriptors[midi->device_id].pub.output)
        err = pmBadPtr;
    if (!descriptors[midi->device_id].pub.opened)
        err = pmBadPtr;
    else
        err = (*midi->dictionary->abort)(midi);
    return pm_errmsg(err);
}

/* in win32 multimedia API (via callbacks) some of these functions used; assume never fail */
long pm_next_time(PmInternal *midi) {

    /* arg checking */
    assert(midi != NULL);
    assert(!Pm_HasHostError(midi));

    return midi->buffer[midi->head].timestamp;
}
/* pm_channel_filtered returns non-zero if the channel mask is blocking the current channel */
static int pm_channel_filtered(int status, int mask)
{
    if ((status & 0xF0) == 0xF0) /* 0xF? messages don't have a channel */
        return 0;
    return !(Pm_Channel(status & 0x0F) & mask);
    /* it'd be easier to return 0 for filtered, 1 for allowed,
       but it would different from the other filtering functions
    */

}
/* The following two functions will checks to see if a MIDI message matches
   the filtering criteria.  Since the sysex routines only want to filter realtime messages,
   we need to have separate routines.
 */

/* pm_realtime_filtered returns non-zero if the filter will kill the current message.
   Note that only realtime messages are checked here.
 */
static int pm_realtime_filtered(int status, long filters)
{
    return ((status == MIDI_ACTIVE) && (filters & PM_FILT_ACTIVE))
            ||  ((status == MIDI_CLOCK) && (filters & PM_FILT_CLOCK))
            ||  ((status == MIDI_START) && (filters & PM_FILT_PLAY))
            ||  ((status == MIDI_STOP) && (filters & PM_FILT_PLAY))
            ||  ((status == MIDI_CONTINUE) && (filters & PM_FILT_PLAY))
            ||  ((status == MIDI_F9) && (filters & PM_FILT_F9))
            ||  ((status == MIDI_FD) && (filters & PM_FILT_FD))
            ||  ((status == MIDI_RESET) && (filters & PM_FILT_RESET))
            ||  ((status == MIDI_MTC) && (filters & PM_FILT_MTC))
            ||  ((status == MIDI_SONGPOS) && (filters & PM_FILT_SONG_POSITION))
            ||  ((status == MIDI_SONGSEL) && (filters & PM_FILT_SONG_SELECT))
            ||  ((status == MIDI_TUNE) && (filters & PM_FILT_TUNE));
}
/* pm_status_filtered returns non-zero if a filter will kill the current message, based on status.
   Note that sysex and real time are not checked.  It is up to the subsystem (winmm, core midi, alsa)
   to filter sysex, as it is handled more easily and efficiently at that level.
   Realtime message are filtered in pm_realtime_filtered.
 */

static int pm_status_filtered(int status, long filters)
{
    status &= 0xF0; /* remove channel information */
    return  ((status == MIDI_NOTE_ON) && (filters & PM_FILT_NOTE))
            ||  ((status == MIDI_NOTE_OFF) && (filters & PM_FILT_NOTE))
            ||  ((status == MIDI_CHANNEL_AT) && (filters & PM_FILT_CHANNEL_AFTERTOUCH))
            ||  ((status == MIDI_POLY_AT) && (filters & PM_FILT_POLY_AFTERTOUCH))
            ||  ((status == MIDI_PROGRAM) && (filters & PM_FILT_PROGRAM))
            ||  ((status == MIDI_CONTROL) && (filters & PM_FILT_CONTROL))
            ||  ((status == MIDI_PITCHBEND) && (filters & PM_FILT_PITCHBEND));

}

/* pm_read_short and pm_read_byte
   are the interface between system-dependent MIDI input handlers
   and the system-independent PortMIDI code.
   The input handler MUST obey these rules:
   1) all short input messages must be sent to pm_read_short, which
      enqueues them to a FIFO for the application.
   2) eash sysex byte should be reported by calling pm_read_byte
      (which sets midi->sysex_in_progress). After the eox byte,
      pm_read_byte will clear sysex_in_progress and midi->flush
   (Note that the overflow flag is managed by pm_read_short
    and Pm_Read, so the supplier should not read or write it.)
 */

/* pm_read_short is the place where all input messages arrive from
   system-dependent code such as pmwinmm.c. Here, the messages
   are entered into the PortMidi input buffer.
 */

 /* Algorithnm:
    if overflow or flush, return
    ATOMIC:
        enqueue data
        if buffer overflow, set overflow
        if buffer overflow and sysex_in_progress, set flush
  */
void pm_read_short(PmInternal *midi, PmEvent *event)
{
    long tail;
    int status;
    /* arg checking */
    assert(midi != NULL);
    assert(!Pm_HasHostError(midi));
    /* midi filtering is applied here */
    status = Pm_MessageStatus(event->message);
    if (!pm_status_filtered(status, midi->filters)
        && !pm_realtime_filtered(status, midi->filters)
        && !pm_channel_filtered(status, midi->channel_mask)) {
        /* if sysex is in progress and we get a status byte, it had
           better be a realtime message or the starting SYSEX byte;
           otherwise, we exit the sysex_in_progress state
         */
        if (midi->sysex_in_progress && (status & MIDI_STATUS_MASK) &&
            !is_real_time(status) && status != MIDI_SYSEX ) {
            midi->sysex_in_progress = FALSE;
            midi->flush = FALSE;
        }

        /* don't try to do anything more in an overflow state */
        if (midi->overflow || midi->flush) return;

        /* insert the message */
        tail = midi->tail;
        midi->buffer[tail++] = *event;
        if (tail == midi->buffer_len) tail = 0;
        if (tail == midi->head || midi->overflow) {
             midi->overflow = TRUE;
            if (midi->sysex_in_progress) midi->flush = TRUE;
            /* drop the rest of the message, this must be cleared
               by caller when EOX is received */
               return;
        }
        midi->tail = tail; /* complete the write */
    }
}


void pm_flush_sysex(PmInternal *midi, PmTimestamp timestamp)
{
    PmEvent event;

    /* there may be nothing in the buffer */
    if (midi->sysex_message_count == 0) return; /* nothing to flush */

    event.message = midi->sysex_message;
    event.timestamp = timestamp;
    pm_read_short(midi, &event);
    midi->sysex_message_count = 0;
    midi->sysex_message = 0;
}


void pm_read_byte(PmInternal *midi, unsigned char byte, PmTimestamp timestamp)
{
    assert(midi);
    assert(!Pm_HasHostError(midi));
    /* here is the logic for controlling sysex_in_progress */
    if (midi->sysex_in_progress) {
        if (byte == MIDI_EOX) midi->sysex_in_progress = FALSE;
        else if (byte == MIDI_SYSEX) {
            /* problem: need to terminate the current sysex and start
               a new one
             */
            pm_flush_sysex(midi, timestamp);
        }
    } else if (byte == MIDI_SYSEX) {
        midi->sysex_in_progress = TRUE;
    } else {
        /* error: we're getting data bytes or EOX but we're no sysex is
           in progress. Drop the data. (Would it be better to report an
           error? Is this a host error or a pmBadData error? Is this
           ever possible?
         */
#ifdef DEBUG
        printf("PortMidi debug msg: unexpected sysex data or EOX\n");
#endif
        return;
    }

    if (pm_realtime_filtered(byte, midi->filters))
        return;
    /* this awkward expression places the bytes in increasingly higher-
       order bytes of the long message */
    midi->sysex_message |= (byte << (8 * midi->sysex_message_count++));
    if (midi->sysex_message_count == 4 || !midi->sysex_in_progress) {
        pm_flush_sysex(midi, timestamp);
        if (!midi->sysex_in_progress) midi->flush = FALSE;
    }
}


int pm_queue_full(PmInternal *midi)
{
    long tail;

    /* arg checking */
    assert(midi != NULL);
    assert(!Pm_HasHostError(midi));

    tail = midi->tail + 1;
    if (tail == midi->buffer_len) tail = 0;
    return tail == midi->head;
}

