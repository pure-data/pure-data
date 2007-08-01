/* pminternal.h -- header for interface implementations */

/* this file is included by files that implement library internals */
/* Here is a guide to implementers:
     provide an initialization function similar to pm_winmm_init()
     add your initialization function to pm_init()
     Note that your init function should never require not-standard
         libraries or fail in any way. If the interface is not available,
         simply do not call pm_add_device. This means that non-standard
         libraries should try to do dynamic linking at runtime using a DLL
         and return without error if the DLL cannot be found or if there
         is any other failure.
     implement functions as indicated in pm_fns_type to open, read, write,
         close, etc.
     call pm_add_device() for each input and output device, passing it a
         pm_fns_type structure.
     assumptions about pm_fns_type functions are given below.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* these are defined in system-specific file */
void *pm_alloc(size_t s);
void pm_free(void *ptr);

/* if an error occurs while opening or closing a midi stream, set these: */
extern int pm_hosterror;
extern char pm_hosterror_text[PM_HOST_ERROR_MSG_LEN];

struct pm_internal_struct;

/* these do not use PmInternal because it is not defined yet... */
typedef PmError (*pm_write_short_fn)(struct pm_internal_struct *midi,
                                     PmEvent *buffer);
typedef PmError (*pm_begin_sysex_fn)(struct pm_internal_struct *midi,
                                     PmTimestamp timestamp);
typedef PmError (*pm_end_sysex_fn)(struct pm_internal_struct *midi,
                                   PmTimestamp timestamp);
typedef PmError (*pm_write_byte_fn)(struct pm_internal_struct *midi,
                                    unsigned char byte, PmTimestamp timestamp);
typedef PmError (*pm_write_realtime_fn)(struct pm_internal_struct *midi,
                                        PmEvent *buffer);
typedef PmError (*pm_write_flush_fn)(struct pm_internal_struct *midi);
typedef PmTimestamp (*pm_synchronize_fn)(struct pm_internal_struct *midi);
/* pm_open_fn should clean up all memory and close the device if any part
   of the open fails */
typedef PmError (*pm_open_fn)(struct pm_internal_struct *midi,
                              void *driverInfo);
typedef PmError (*pm_abort_fn)(struct pm_internal_struct *midi);
/* pm_close_fn should clean up all memory and close the device if any
   part of the close fails. */
typedef PmError (*pm_close_fn)(struct pm_internal_struct *midi);
typedef PmError (*pm_poll_fn)(struct pm_internal_struct *midi);
typedef void (*pm_host_error_fn)(struct pm_internal_struct *midi, char * msg,
                                 unsigned int len);
typedef unsigned int (*pm_has_host_error_fn)(struct pm_internal_struct *midi);

typedef struct {
    pm_write_short_fn write_short; /* output short MIDI msg */
    pm_begin_sysex_fn begin_sysex; /* prepare to send a sysex message */
    pm_end_sysex_fn end_sysex; /* marks end of sysex message */
    pm_write_byte_fn write_byte; /* accumulate one more sysex byte */
    pm_write_realtime_fn write_realtime; /* send real-time message within sysex */
    pm_write_flush_fn write_flush; /* send any accumulated but unsent data */
    pm_synchronize_fn synchronize; /* synchronize portmidi time to stream time */
    pm_open_fn open;   /* open MIDI device */
    pm_abort_fn abort; /* abort */
    pm_close_fn close; /* close device */
    pm_poll_fn poll;   /* read pending midi events into portmidi buffer */
    pm_has_host_error_fn has_host_error; /* true when device has had host
                                            error message */
    pm_host_error_fn host_error; /* provide text readable host error message
                                    for device (clears and resets) */
} pm_fns_node, *pm_fns_type;


/* when open fails, the dictionary gets this set of functions: */
extern pm_fns_node pm_none_dictionary;

typedef struct {
    PmDeviceInfo pub; /* some portmidi state also saved in here (for autmatic
                         device closing (see PmDeviceInfo struct) */
    void *descriptor; /* ID number passed to win32 multimedia API open */
    void *internalDescriptor; /* points to PmInternal device, allows automatic
                                 device closing */
    pm_fns_type dictionary;
} descriptor_node, *descriptor_type;

extern int pm_descriptor_max;
extern descriptor_type descriptors;
extern int pm_descriptor_index;

typedef unsigned long (*time_get_proc_type)(void *time_info);

typedef struct pm_internal_struct {
    int device_id; /* which device is open (index to descriptors) */
    short write_flag; /* MIDI_IN, or MIDI_OUT */

    PmTimeProcPtr time_proc; /* where to get the time */
    void *time_info; /* pass this to get_time() */

    long buffer_len; /* how big is the buffer */
    PmEvent *buffer; /* storage for:
                        - midi input
                        - midi output w/latency != 0 */
    long head;
    long tail;

    long latency; /* time delay in ms between timestamps and actual output */
                  /* set to zero to get immediate, simple blocking output */
                  /* if latency is zero, timestamps will be ignored; */
                  /* if midi input device, this field ignored */

    int overflow; /* set to non-zero if input is dropped */
    int flush; /* flag to drop incoming sysex data because of overflow */
    int sysex_in_progress; /* use for overflow management */
    PmMessage sysex_message; /* buffer for 4 bytes of sysex data */
    int sysex_message_count; /* how many bytes in sysex_message so far */

    long filters; /* flags that filter incoming message classes */
    int channel_mask; /* filter incoming messages based on channel */
    PmTimestamp last_msg_time; /* timestamp of last message */
    PmTimestamp sync_time; /* time of last synchronization */
    PmTimestamp now; /* set by PmWrite to current time */
    int first_message; /* initially true, used to run first synchronization */
    pm_fns_type dictionary; /* implementation functions */
    void *descriptor; /* system-dependent state */

} PmInternal;

typedef struct {
    long head;
    long tail;
    long len;
    long msg_size;
    long overflow;
    char *buffer;
} PmQueueRep;

/* defined by system specific implementation, e.g. pmwinmm, used by PortMidi */
void pm_init(void);
void pm_term(void);

/* defined by portMidi, used by pmwinmm */
PmError none_write_short(PmInternal *midi, PmEvent *buffer);
PmError none_sysex(PmInternal *midi, PmTimestamp timestamp);
PmError none_write_byte(PmInternal *midi, unsigned char byte,
                        PmTimestamp timestamp);
PmTimestamp none_synchronize(PmInternal *midi);

PmError pm_fail_fn(PmInternal *midi);
PmError pm_success_fn(PmInternal *midi);
PmError pm_add_device(char *interf, char *name, int input, void *descriptor,
                      pm_fns_type dictionary);
void pm_read_byte(PmInternal *midi, unsigned char byte, PmTimestamp timestamp);
void pm_begin_sysex(PmInternal *midi);
void pm_end_sysex(PmInternal *midi);
void pm_read_short(PmInternal *midi, PmEvent *event);

#define none_write_flush pm_fail_fn
#define none_poll pm_fail_fn
#define success_poll pm_success_fn

#define MIDI_REALTIME_MASK 0xf8
#define is_real_time(msg) \
    ((Pm_MessageStatus(msg) & MIDI_REALTIME_MASK) == MIDI_REALTIME_MASK)

#ifdef __cplusplus
}
#endif

