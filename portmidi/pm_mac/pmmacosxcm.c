/*
 * Platform interface to the MacOS X CoreMIDI framework
 * 
 * Jon Parise <jparise@cmu.edu>
 * and subsequent work by Andrew Zeldis and Zico Kolter
 * and Roger B. Dannenberg
 *
 * $Id: pmmacosx.c,v 1.17 2002/01/27 02:40:40 jon Exp $
 */
 
/* Notes:
    since the input and output streams are represented by MIDIEndpointRef
    values and almost no other state, we store the MIDIEndpointRef on
    descriptors[midi->device_id].descriptor. The only other state we need
    is for errors: we need to know if there is an error and if so, what is
    the error text. As in pmwinmm.c, we use a structure with two kinds of
    host error: "error" and "callback_error". That way, asynchronous callbacks
    do not interfere with other error information.
    
    OS X does not seem to have an error-code-to-text function, so we will
    just use text messages instead of error codes.
 */

#include <stdlib.h>

#include "portmidi.h"
#include "pminternal.h"
#include "porttime.h"
#include "pmmac.h"
#include "pmmacosxcm.h"

#include <stdio.h>
#include <string.h>

#include <CoreServices/CoreServices.h>
#include <CoreMIDI/MIDIServices.h>
#include <CoreAudio/HostTime.h>

#define PACKET_BUFFER_SIZE 1024

/* this is very strange: if I put in a reasonable 
   number here, e.g. 128, which would allow sysex data
   to be sent 128 bytes at a time, then I lose sysex
   data in my loopback test. With a buffer size of 4,
   we put at most 4 bytes in a packet (but maybe many
   packets in a packetList), and everything works fine.
 */
#define SYSEX_BUFFER_SIZE 4

#define VERBOSE_ON 1
#define VERBOSE if (VERBOSE_ON)

#define MIDI_SYSEX      0xf0
#define MIDI_EOX        0xf7
#define MIDI_STATUS_MASK 0x80

static MIDIClientRef	client = NULL;	/* Client handle to the MIDI server */
static MIDIPortRef	portIn = NULL;	/* Input port handle */
static MIDIPortRef	portOut = NULL;	/* Output port handle */

extern pm_fns_node pm_macosx_in_dictionary;
extern pm_fns_node pm_macosx_out_dictionary;

typedef struct midi_macosxcm_struct {
    unsigned long sync_time; /* when did we last determine delta? */
    UInt64 delta;	/* difference between stream time and real time in ns */
    UInt64 last_time;	/* last output time */
    int first_message;  /* tells midi_write to sychronize timestamps */
    int sysex_mode;     /* middle of sending sysex */
    unsigned long sysex_word; /* accumulate data when receiving sysex */
    unsigned int sysex_byte_count; /* count how many received */
    char error[PM_HOST_ERROR_MSG_LEN];
    char callback_error[PM_HOST_ERROR_MSG_LEN];
    Byte packetBuffer[PACKET_BUFFER_SIZE];
    MIDIPacketList *packetList; /* a pointer to packetBuffer */
    MIDIPacket *packet;
    Byte sysex_buffer[SYSEX_BUFFER_SIZE]; /* temp storage for sysex data */
    MIDITimeStamp sysex_timestamp; /* timestamp to use with sysex data */
} midi_macosxcm_node, *midi_macosxcm_type;

/* private function declarations */
MIDITimeStamp timestamp_pm_to_cm(PmTimestamp timestamp);
PmTimestamp timestamp_cm_to_pm(MIDITimeStamp timestamp);

char* cm_get_full_endpoint_name(MIDIEndpointRef endpoint);


static int
midi_length(long msg)
{
    int status, high, low;
    static int high_lengths[] = {
        1, 1, 1, 1, 1, 1, 1, 1,         /* 0x00 through 0x70 */
        3, 3, 3, 3, 2, 2, 3, 1          /* 0x80 through 0xf0 */
    };
    static int low_lengths[] = {
        1, 1, 3, 2, 1, 1, 1, 1,         /* 0xf0 through 0xf8 */
        1, 1, 1, 1, 1, 1, 1, 1          /* 0xf9 through 0xff */
    };

    status = msg & 0xFF;
    high = status >> 4;
    low = status & 15;

    return (high != 0xF0) ? high_lengths[high] : low_lengths[low];
}

static PmTimestamp midi_synchronize(PmInternal *midi)
{
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    UInt64 pm_stream_time_2 = 
            AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());
    PmTimestamp real_time;
    UInt64 pm_stream_time;
    /* if latency is zero and this is an output, there is no 
       time reference and midi_synchronize should never be called */
    assert(midi->time_proc);
    assert(!(midi->write_flag && midi->latency == 0));
    do {
         /* read real_time between two reads of stream time */
         pm_stream_time = pm_stream_time_2;
         real_time = (*midi->time_proc)(midi->time_info);
         pm_stream_time_2 = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());
         /* repeat if more than 0.5 ms has elapsed */
    } while (pm_stream_time_2 > pm_stream_time + 500000);
    m->delta = pm_stream_time - ((UInt64) real_time * (UInt64) 1000000);
    m->sync_time = real_time;
    return real_time;
}


/* called when MIDI packets are received */
static void
readProc(const MIDIPacketList *newPackets, void *refCon, void *connRefCon)
{
    PmInternal *midi;
    midi_macosxcm_type m;
    PmEvent event;
    MIDIPacket *packet;
    unsigned int packetIndex;
    unsigned long now;
    unsigned int status;
    
    /* Retrieve the context for this connection */
    midi = (PmInternal *) connRefCon;
    m = (midi_macosxcm_type) midi->descriptor;
    assert(m);
    
    /* synchronize time references every 100ms */
    now = (*midi->time_proc)(midi->time_info);
    if (m->first_message || m->sync_time + 100 /*ms*/ < now) { 
        /* time to resync */
        now = midi_synchronize(midi);
        m->first_message = FALSE;
    }
    
    packet = (MIDIPacket *) &newPackets->packet[0];
    /* printf("readproc packet status %x length %d\n", packet->data[0], packet->length); */
    for (packetIndex = 0; packetIndex < newPackets->numPackets; packetIndex++) {
        /* Set the timestamp and dispatch this message */
        event.timestamp = 
                (AudioConvertHostTimeToNanos(packet->timeStamp) - m->delta) / 
                (UInt64) 1000000;
        status = packet->data[0];
        /* process packet as sysex data if it begins with MIDI_SYSEX, or
           MIDI_EOX or non-status byte */
        if (status == MIDI_SYSEX || status == MIDI_EOX ||
            !(status & MIDI_STATUS_MASK)) {
            int i = 0;
            while (i < packet->length) {
                pm_read_byte(midi, packet->data[i], event.timestamp);
                i++;
            }
        } else {
            /* Build the PmMessage for the PmEvent structure */
            switch (packet->length) {
                case 1:
                    event.message = Pm_Message(packet->data[0], 0, 0);
                    break; 
                case 2:
                    event.message = Pm_Message(packet->data[0], 
                                               packet->data[1], 0);
                    break;
                case 3:
                    event.message = Pm_Message(packet->data[0],
                                               packet->data[1], 
                                               packet->data[2]);
                    break;
                default:
                    /* Skip packets that are too large to fit in a PmMessage */
#ifdef DEBUG
                    printf("PortMidi debug msg: large packet skipped\n");
#endif
                    continue;
            }
            pm_read_short(midi, &event);
        }
        packet = MIDIPacketNext(packet);
    }
}

static PmError
midi_in_open(PmInternal *midi, void *driverInfo)
{
    MIDIEndpointRef endpoint;
    midi_macosxcm_type m;
    OSStatus macHostError;
    
    /* insure that we have a time_proc for timing */
    if (midi->time_proc == NULL) {
        if (!Pt_Started()) 
            Pt_Start(1, 0, 0);
        /* time_get does not take a parameter, so coerce */
        midi->time_proc = (PmTimeProcPtr) Pt_Time;
    }
    
    endpoint = (MIDIEndpointRef) descriptors[midi->device_id].descriptor;
    if (endpoint == NULL) {
        return pmInvalidDeviceId;
    }

    m = (midi_macosxcm_type) pm_alloc(sizeof(midi_macosxcm_node)); /* create */
    midi->descriptor = m;
    if (!m) {
        return pmInsufficientMemory;
    }
    m->error[0] = 0;
    m->callback_error[0] = 0;
    m->sync_time = 0;
    m->delta = 0;
    m->last_time = 0;
    m->first_message = TRUE;
    m->sysex_mode = FALSE;
    m->sysex_word = 0;
    m->sysex_byte_count = 0;
    m->packetList = NULL;
    m->packet = NULL;
    
    macHostError = MIDIPortConnectSource(portIn, endpoint, midi);
    if (macHostError != noErr) {
        pm_hosterror = macHostError;
        sprintf(pm_hosterror_text, 
                "Host error %ld: MIDIPortConnectSource() in midi_in_open()",
                macHostError);
        midi->descriptor = NULL;
        pm_free(m);
        return pmHostError;
    }
    
    return pmNoError;
}

static PmError
midi_in_close(PmInternal *midi)
{
    MIDIEndpointRef endpoint;
    OSStatus macHostError;
    PmError err = pmNoError;
    
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    
    if (!m) return pmBadPtr;

    endpoint = (MIDIEndpointRef) descriptors[midi->device_id].descriptor;
    if (endpoint == NULL) {
        pm_hosterror = pmBadPtr;
    }
    
    /* shut off the incoming messages before freeing data structures */
    macHostError = MIDIPortDisconnectSource(portIn, endpoint);
    if (macHostError != noErr) {
        pm_hosterror = macHostError;
        sprintf(pm_hosterror_text, 
                "Host error %ld: MIDIPortDisconnectSource() in midi_in_close()",
                macHostError);
        err = pmHostError;
    }
    
    midi->descriptor = NULL;
    pm_free(midi->descriptor);
    
    return err;
}


static PmError
midi_out_open(PmInternal *midi, void *driverInfo)
{
    midi_macosxcm_type m;

    m = (midi_macosxcm_type) pm_alloc(sizeof(midi_macosxcm_node)); /* create */
    midi->descriptor = m;
    if (!m) {
        return pmInsufficientMemory;
    }
    m->error[0] = 0;
    m->callback_error[0] = 0;
    m->sync_time = 0;
    m->delta = 0;
    m->last_time = 0;
    m->first_message = TRUE;
    m->sysex_mode = FALSE;
    m->sysex_word = 0;
    m->sysex_byte_count = 0;
    m->packetList = (MIDIPacketList *) m->packetBuffer;
    m->packet = NULL;

    return pmNoError;
}

static PmError
midi_out_close(PmInternal *midi)
{
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    if (!m) return pmBadPtr;
    
    midi->descriptor = NULL;
    pm_free(midi->descriptor);
    
    return pmNoError;
}

static PmError
midi_abort(PmInternal *midi)
{
    return pmNoError;
}


static PmError
midi_write_flush(PmInternal *midi)
{
    OSStatus macHostError;
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    MIDIEndpointRef endpoint = 
            (MIDIEndpointRef) descriptors[midi->device_id].descriptor;
    assert(m);
    assert(endpoint);
    if (m->packet != NULL) {
        /* out of space, send the buffer and start refilling it */
        macHostError = MIDISend(portOut, endpoint, m->packetList);
        m->packet = NULL; /* indicate no data in packetList now */
        if (macHostError != noErr) goto send_packet_error;
    }
    return pmNoError;
    
send_packet_error:
    pm_hosterror = macHostError;
    sprintf(pm_hosterror_text, 
            "Host error %ld: MIDISend() in midi_write()",
            macHostError);
    return pmHostError;

}


static PmError
send_packet(PmInternal *midi, Byte *message, unsigned int messageLength, 
            MIDITimeStamp timestamp)
{
    PmError err;
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    assert(m);
    
    /* printf("add %d to packet %lx len %d\n", message[0], m->packet, messageLength); */
    m->packet = MIDIPacketListAdd(m->packetList, sizeof(m->packetBuffer), 
                                  m->packet, timestamp, messageLength, 
                                  message);
    if (m->packet == NULL) {
        /* out of space, send the buffer and start refilling it */
        /* make midi->packet non-null to fool midi_write_flush into sending */
        m->packet = (MIDIPacket *) 4; 
        if ((err = midi_write_flush(midi)) != pmNoError) return err;
        m->packet = MIDIPacketListInit(m->packetList);
        assert(m->packet); /* if this fails, it's a programming error */
        m->packet = MIDIPacketListAdd(m->packetList, sizeof(m->packetBuffer),
                                      m->packet, timestamp, messageLength, 
                                      message);
        assert(m->packet); /* can't run out of space on first message */           
    }
    return pmNoError;
}    


static PmError
midi_write_short(PmInternal *midi, PmEvent *event)
{
    long when = event->timestamp;
    long what = event->message;
    MIDITimeStamp timestamp;
    UInt64 when_ns;
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    Byte message[4];
    unsigned int messageLength;

    if (m->packet == NULL) {
        m->packet = MIDIPacketListInit(m->packetList);
        /* this can never fail, right? failure would indicate something 
           unrecoverable */
        assert(m->packet);
    }
    
    /* compute timestamp */
    if (when == 0) when = midi->now;
    /* if latency == 0, midi->now is not valid. We will just set it to zero */
    if (midi->latency == 0) when = 0;
    when_ns = ((UInt64) (when + midi->latency) * (UInt64) 1000000) + m->delta;
    /* make sure we don't go backward in time */
    if (when_ns < m->last_time) when_ns = m->last_time;
    m->last_time = when_ns;
    timestamp = (MIDITimeStamp) AudioConvertNanosToHostTime(when_ns);

    message[0] = Pm_MessageStatus(what);
    message[1] = Pm_MessageData1(what);
    message[2] = Pm_MessageData2(what);
    messageLength = midi_length(what);
        
    /* Add this message to the packet list */
    return send_packet(midi, message, messageLength, timestamp);
}


static PmError 
midi_begin_sysex(PmInternal *midi, PmTimestamp when)
{
    UInt64 when_ns;
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    assert(m);
    m->sysex_byte_count = 0;
    
    /* compute timestamp */
    if (when == 0) when = midi->now;
    /* if latency == 0, midi->now is not valid. We will just set it to zero */
    if (midi->latency == 0) when = 0;
    when_ns = ((UInt64) (when + midi->latency) * (UInt64) 1000000) + m->delta;
    m->sysex_timestamp = (MIDITimeStamp) AudioConvertNanosToHostTime(when_ns);

    if (m->packet == NULL) {
        m->packet = MIDIPacketListInit(m->packetList);
        /* this can never fail, right? failure would indicate something 
           unrecoverable */
        assert(m->packet);
    }
    return pmNoError;
}


static PmError
midi_end_sysex(PmInternal *midi, PmTimestamp when)
{
    PmError err;
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    assert(m);
    
    /* make sure we don't go backward in time */
    if (m->sysex_timestamp < m->last_time) m->sysex_timestamp = m->last_time;
    
    /* now send what's in the buffer */
    if (m->packet == NULL) {
        /* if flush has been called in the meantime, packet list is NULL */
        m->packet = MIDIPacketListInit(m->packetList);
        /* this can never fail, right? failure would indicate something 
           unrecoverable */
        assert(m->packet);
    }

    err = send_packet(midi, m->sysex_buffer, m->sysex_byte_count,
                      m->sysex_timestamp);
    m->sysex_byte_count = 0;
    if (err != pmNoError) {
        m->packet = NULL; /* flush everything in the packet list */
        return err;
    }
    return pmNoError;
}


static PmError
midi_write_byte(PmInternal *midi, unsigned char byte, PmTimestamp timestamp)
{
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    assert(m);
    if (m->sysex_byte_count >= SYSEX_BUFFER_SIZE) {
        PmError err = midi_end_sysex(midi, timestamp);
        if (err != pmNoError) return err;
    }
    m->sysex_buffer[m->sysex_byte_count++] = byte;
    return pmNoError;
}


static PmError
midi_write_realtime(PmInternal *midi, PmEvent *event)
{
    /* to send a realtime message during a sysex message, first
       flush all pending sysex bytes into packet list */
    PmError err = midi_end_sysex(midi, 0);
    if (err != pmNoError) return err;
    /* then we can just do a normal midi_write_short */
    return midi_write_short(midi, event);
}

static unsigned int midi_has_host_error(PmInternal *midi)
{
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    return (m->callback_error[0] != 0) || (m->error[0] != 0);
}


static void midi_get_host_error(PmInternal *midi, char *msg, unsigned int len)
{
    midi_macosxcm_type m = (midi_macosxcm_type) midi->descriptor;
    msg[0] = 0; /* initialize to empty string */
    if (m) { /* make sure there is an open device to examine */
        if (m->error[0]) {
            strncpy(msg, m->error, len);
            m->error[0] = 0; /* clear the error */
        } else if (m->callback_error[0]) {
            strncpy(msg, m->callback_error, len);
            m->callback_error[0] = 0; /* clear the error */
        }
        msg[len - 1] = 0; /* make sure string is terminated */
    }
}


MIDITimeStamp timestamp_pm_to_cm(PmTimestamp timestamp)
{
    UInt64 nanos;
    if (timestamp <= 0) {
        return (MIDITimeStamp)0;
    } else {
        nanos = (UInt64)timestamp * (UInt64)1000000;
        return (MIDITimeStamp)AudioConvertNanosToHostTime(nanos);
    }
}

PmTimestamp timestamp_cm_to_pm(MIDITimeStamp timestamp)
{
    UInt64 nanos;
    nanos = AudioConvertHostTimeToNanos(timestamp);
    return (PmTimestamp)(nanos / (UInt64)1000000);
}


char* cm_get_full_endpoint_name(MIDIEndpointRef endpoint)
{
    MIDIEntityRef entity;
    MIDIDeviceRef device;
    CFStringRef endpointName = NULL, deviceName = NULL, fullName = NULL;
    CFStringEncoding defaultEncoding;
    char* newName;

    /* get the default string encoding */
    defaultEncoding = CFStringGetSystemEncoding();

    /* get the entity and device info */
    MIDIEndpointGetEntity(endpoint, &entity);
    MIDIEntityGetDevice(entity, &device);

    /* create the nicely formated name */
    MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &endpointName);
    MIDIObjectGetStringProperty(device, kMIDIPropertyName, &deviceName);
    if (deviceName != NULL) {
        fullName = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@: %@"),
                                            deviceName, endpointName);
    } else {
        fullName = endpointName;
    }
    
    /* copy the string into our buffer */
    newName = (char*)malloc(CFStringGetLength(fullName) + 1);
    CFStringGetCString(fullName, newName, CFStringGetLength(fullName) + 1,
                        defaultEncoding);

    /* clean up */
    if (endpointName) CFRelease(endpointName);
    if (deviceName) CFRelease(deviceName);
    if (fullName) CFRelease(fullName);

    return newName;
}

 

pm_fns_node pm_macosx_in_dictionary = {
    none_write_short,
    none_sysex,
    none_sysex,
    none_write_byte,
    none_write_short,
    none_write_flush,
    none_synchronize,
    midi_in_open,
    midi_abort,
    midi_in_close,
    success_poll,
    midi_has_host_error,
    midi_get_host_error,
};

pm_fns_node pm_macosx_out_dictionary = {
    midi_write_short,
    midi_begin_sysex,
    midi_end_sysex,
    midi_write_byte,
    midi_write_realtime,
    midi_write_flush,
    midi_synchronize,
    midi_out_open,
    midi_abort,
    midi_out_close,
    success_poll,
    midi_has_host_error,
    midi_get_host_error,
};


PmError pm_macosxcm_init(void)
{
    ItemCount numInputs, numOutputs, numDevices;
    MIDIEndpointRef endpoint;
    int i;
    OSStatus macHostError;
    char *error_text;

    /* Determine the number of MIDI devices on the system */
    numDevices = MIDIGetNumberOfDevices();
    numInputs = MIDIGetNumberOfSources();
    numOutputs = MIDIGetNumberOfDestinations();

    /* Return prematurely if no devices exist on the system
       Note that this is not an error. There may be no devices.
       Pm_CountDevices() will return zero, which is correct and
       useful information
     */
    if (numDevices <= 0) {
        return pmNoError;
    }


    /* Initialize the client handle */
    macHostError = MIDIClientCreate(CFSTR("PortMidi"), NULL, NULL, &client);
    if (macHostError != noErr) {
        error_text = "MIDIClientCreate() in pm_macosxcm_init()";
        goto error_return;
    }

    /* Create the input port */
    macHostError = MIDIInputPortCreate(client, CFSTR("Input port"), readProc,
                                          NULL, &portIn);
    if (macHostError != noErr) {
        error_text = "MIDIInputPortCreate() in pm_macosxcm_init()";
        goto error_return;
    }
        
    /* Create the output port */
    macHostError = MIDIOutputPortCreate(client, CFSTR("Output port"), &portOut);
    if (macHostError != noErr) {
        error_text = "MIDIOutputPortCreate() in pm_macosxcm_init()";
        goto error_return;
    }

    /* Iterate over the MIDI input devices */
    for (i = 0; i < numInputs; i++) {
        endpoint = MIDIGetSource(i);
        if (endpoint == NULL) {
            continue;
        }

        /* set the first input we see to the default */
        if (pm_default_input_device_id == -1)
            pm_default_input_device_id = pm_descriptor_index;
        
        /* Register this device with PortMidi */
        pm_add_device("CoreMIDI", cm_get_full_endpoint_name(endpoint),
                      TRUE, (void*)endpoint, &pm_macosx_in_dictionary);
    }

    /* Iterate over the MIDI output devices */
    for (i = 0; i < numOutputs; i++) {
        endpoint = MIDIGetDestination(i);
        if (endpoint == NULL) {
            continue;
        }

        /* set the first output we see to the default */
        if (pm_default_output_device_id == -1)
            pm_default_output_device_id = pm_descriptor_index;

        /* Register this device with PortMidi */
        pm_add_device("CoreMIDI", cm_get_full_endpoint_name(endpoint),
                      FALSE, (void*)endpoint, &pm_macosx_out_dictionary);
    }
    return pmNoError;
    
error_return:
    pm_hosterror = macHostError;
    sprintf(pm_hosterror_text, "Host error %ld: %s\n", macHostError, error_text);
    pm_macosxcm_term(); /* clear out any opened ports */
    return pmHostError;
}

void pm_macosxcm_term(void)
{
    if (client != NULL)	 MIDIClientDispose(client);
    if (portIn != NULL)	 MIDIPortDispose(portIn);
    if (portOut != NULL) MIDIPortDispose(portOut);
}
