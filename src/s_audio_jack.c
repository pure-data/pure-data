/* Copyright (c) 2003, Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  Audio back-end for connecting with the JACK audio interconnect system.
*/

#ifdef USEAPI_JACK

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "m_pd.h"
#include "m_private_utils.h"
#include "s_stuff.h"
#include "s_audio_paring.h"
#ifdef __APPLE__
# include <jack/weakjack.h>
#elif defined _MSC_VER
# define strdup _strdup
#endif
#include <jack/jack.h>
#include <regex.h>

#define MAX_CLIENTS 100
#define MAX_JACK_PORTS 128  /* higher values seem to give bad xrun problems */
#define BUF_JACK 4096
/* taken from the PipeWire libjack implementation: the larger of the
 * `JACK_CLIENT_NAME_SIZE` definitions I could find in the wild. */
#define CLIENT_NAME_SIZE_FALLBACK 128

#define MAX_ALLOCA_SAMPLES 16*1024

/* enable thread signaling instead of polling */
#if 1
#define THREADSIGNAL
#endif

static jack_nframes_t jack_out_max;
static jack_nframes_t jack_filled = 0;
static int jack_started = 0;
static int jack_isopening = 0;
static jack_port_t *input_port[MAX_JACK_PORTS];
static jack_port_t *output_port[MAX_JACK_PORTS];
static jack_client_t *jack_client = NULL;
const char *jack_client_names[MAX_CLIENTS];
static volatile int jack_dio_error;
static volatile int jack_didshutdown;
static t_audiocallback jack_callback;
static int jack_should_autoconnect = 1;
static int jack_defaultsource = 0;
static int jack_defaultsink = 0;
static int jack_blocksize = 0; /* should this be PERTHREAD? */
#ifdef THREADSIGNAL
t_semaphore *jack_sem;
#endif
static char *jack_outbuf;
static sys_ringbuf jack_outring;
static char *jack_inbuf;
static sys_ringbuf jack_inring;

    /* callback routine for non-callback client... throw samples into
        and read them out of a FIFO.  Since we don't know at compile time
        how many samples jack will treat us to, we interleave them in the
        two FIFOs.  So we have to mux/demux them both here and up at the
        user level in jack_send_dacs(). */
static int jack_polling_callback(jack_nframes_t nframes, void *unused)
{
    unsigned long infiforoom = sys_ringbuf_getwriteavailable(&jack_inring),
        outfiforoom = sys_ringbuf_getreadavailable(&jack_outring);
    const size_t muxbufsize = nframes *
        (STUFF->st_inchannels > STUFF->st_outchannels ?
            STUFF->st_inchannels : STUFF->st_outchannels);
    t_sample *muxbuffer;
    int j;
    jack_default_audio_sample_t *jp;
    ALLOCA(t_sample, muxbuffer, muxbufsize, MAX_ALLOCA_SAMPLES);

    if (infiforoom < nframes * STUFF->st_inchannels * sizeof(t_sample) ||
        outfiforoom < nframes * STUFF->st_outchannels * sizeof(t_sample))
    {
        /* data late: output zeros, drop inputs, and leave FIFOs untouched */
        if (jack_started)
            jack_dio_error = 1;
        for (j = 0; j < STUFF->st_outchannels; j++)
        {
            if ((jp = jack_port_get_buffer(output_port[j], nframes)))
                memset(jp, 0, sizeof (jack_default_audio_sample_t) * nframes);
        }
    }
    else
    {
        t_sample *fp, *fp2;
        int ch;
        if (STUFF->st_inchannels)
        {
            for (fp = muxbuffer, ch = 0; ch < STUFF->st_inchannels; ch++, fp++)
            {
                jp = jack_port_get_buffer(input_port[ch], nframes);
                for (j = 0, fp2 = fp; j < (int)nframes;
                    j++, fp2 += STUFF->st_inchannels)
                        *fp2 = jp[j];
            }
            sys_ringbuf_write(&jack_inring, muxbuffer,
                nframes * STUFF->st_inchannels * sizeof(t_sample),
                    jack_inbuf);
        }
        if (STUFF->st_outchannels)
        {
            sys_ringbuf_read(&jack_outring, muxbuffer,
                nframes * STUFF->st_outchannels * sizeof(t_sample),
                    jack_outbuf);
            for (fp = muxbuffer, ch = 0; ch < STUFF->st_outchannels; ch++, fp++)
            {
                jp = jack_port_get_buffer(output_port[ch], nframes);
                for (j = 0, fp2 = fp; j < (int)nframes;
                    j++, fp2 += STUFF->st_outchannels)
                        jp[j] = *fp2;
            }
        }
    }
#ifdef THREADSIGNAL
    sys_semaphore_post(jack_sem);
#endif
    FREEA(t_sample, muxbuffer, muxbufsize, MAX_ALLOCA_SAMPLES);
    return 0;
}

static int callbackprocess(jack_nframes_t nframes, void *arg)
{
    int chan, j, k;
    unsigned int n;
    jack_default_audio_sample_t *out[MAX_JACK_PORTS], *in[MAX_JACK_PORTS], *jp;
    if (nframes % DEFDACBLKSIZE)
    {
        fprintf(stderr, "jack: nframes %d not a multiple of blocksize %d\n",
            nframes, DEFDACBLKSIZE);
        nframes -= (nframes % DEFDACBLKSIZE);
    }
    for (chan = 0; chan < STUFF->st_inchannels; chan++)
        in[chan] = jack_port_get_buffer(input_port[chan], nframes);
    for (chan = 0; chan < STUFF->st_outchannels; chan++)
        out[chan] = jack_port_get_buffer(output_port[chan], nframes);
    for (n = 0; n < nframes; n += DEFDACBLKSIZE)
    {
        t_sample *fp;
        for (chan = 0; chan < STUFF->st_inchannels; chan++)
            if (in[chan])
        {
            for (fp = STUFF->st_soundin + chan*DEFDACBLKSIZE,
                jp = in[chan] + n, j=0; j < DEFDACBLKSIZE; j++)
                    *fp++ = *jp++;
        }
        for (chan = 0; chan < STUFF->st_outchannels; chan++)
        {
            for (fp = STUFF->st_soundout + chan*DEFDACBLKSIZE,
                j = 0; j < DEFDACBLKSIZE; j++)
                    *fp++ = 0;
        }
        (*jack_callback)();
        for (chan = 0; chan < STUFF->st_outchannels; chan++)
            if (out[chan])
        {
            for (fp = STUFF->st_soundout + chan*DEFDACBLKSIZE, jp = out[chan] + n,
                j=0; j < DEFDACBLKSIZE; j++)
                    *jp++ = *fp++;
        }
    }
    return 0;
}

static int jack_srate(jack_nframes_t srate, void *arg)
{
        /* prevent recursion/deadlock in jack_open_audio()! */
    if (!jack_isopening)
    {
        sys_lock();
        if (srate != STUFF->st_dacsr)
        {
            STUFF->st_dacsr = srate;
            canvas_update_dsp();
        }
        sys_unlock();
    }
    return 0;
}

static int jack_bsize(jack_nframes_t bufsize, void *arg)
{
    jack_blocksize = bufsize;
    return 0;
}

    /* This callback function must be async-signal-safe, so the only thing
    we can really do is set a flag and wake up the scheduler. The shutdown
    is actually handled in jack_send_dacs() or sys_try_reopen_audio();
    the latter is called by the callback scheduler once it has noticed that
    audio processing has halted. */
static void jack_shutdown(void *arg)
{
    jack_didshutdown = 1;
#ifdef THREADSIGNAL
        /* sem_post() is async-signal-safe */
    sys_semaphore_post(jack_sem);
#endif
}

static int jack_xrun(void* arg)
{
    jack_dio_error = 1;
    return 0;
}

typedef struct _jclient {
    const char*name;
    int input;
    int output;
    struct _jclient *next;
} t_jclient;

static const char **jack_get_clients(void)
{
    int jack_physicalsource = -1;
    int jack_physicalsink = -1;
    const char **jack_ports;
    int tmp_client_name_size = jack_client_name_size ?
        jack_client_name_size() : CLIENT_NAME_SIZE_FALLBACK;
    char* tmp_client_name = (char*)getbytes(tmp_client_name_size);
    int i;
    int num_clients = 0;
    t_jclient*available_clients = 0;
    regex_t port_regex;
    regcomp(&port_regex, "^[^:]*", REG_EXTENDED);

    for (i=0; i< MAX_CLIENTS; i++)
        jack_client_names[i] = NULL;


    jack_ports = jack_get_ports(jack_client, "", JACK_DEFAULT_AUDIO_TYPE, 0);
    if(jack_ports) {
        t_jclient*tmp_client;
        for (i = 0; jack_ports[i] != NULL; i++)
        {
            regmatch_t match_info;
            t_jclient*last_client=0;
            jack_port_t*port = 0;
            if(jack_port_by_name)
                port = jack_port_by_name(jack_client, jack_ports[i]);

                /* extract the client name from the port name, using a regex
                 * that parses the clientname:portname syntax */
            regexec(&port_regex, jack_ports[i], 1, &match_info, 0);
            memcpy(tmp_client_name, &jack_ports[i][match_info.rm_so],
                match_info.rm_eo - match_info.rm_so);
            tmp_client_name[ match_info.rm_eo - match_info.rm_so ] = '\0';

                /* check if we already have this port */
            for (tmp_client=available_clients; tmp_client;
                tmp_client=tmp_client->next)
            {
                last_client = tmp_client;
                if(strcmp(tmp_client_name, tmp_client->name) == 0) {
                    break;
                }
            }
                /* append the new port */
            if(!tmp_client) {
                tmp_client = getbytes(sizeof(*tmp_client));
                tmp_client->name = strdup(tmp_client_name);

                    /* The alsa_pcm client should go in spot 0. */
                if(strcmp("alsa_pcm", tmp_client_name) == 0) {
                    tmp_client->next = available_clients;
                    available_clients = tmp_client;
                } else if(!available_clients) {
                    available_clients = tmp_client;
                } else {
                    last_client->next = tmp_client;
                }
            }
                /* remember the capabilities of this client;
                 * we keep input and output separate,
                 * so we can distinguish between physical inputs and outputs
                 * (e.g. a client with physical outputs but no physical inputs)
                 */
            if(port && jack_port_flags) {
                int flags = jack_port_flags(port);
                if (flags & JackPortIsInput)
                    tmp_client->input |= flags;
                if (flags & JackPortIsOutput)
                    tmp_client->output |= flags;
            }
        }
    }

        /* now that we have a list of usable JACK clients,
           filter for inputs and outputs
        */
    jack_defaultsource = jack_defaultsink = -1;
    if(available_clients) {
        t_jclient*tmp_client;
        for(num_clients=0, tmp_client = available_clients;
            num_clients < MAX_CLIENTS && tmp_client;
            tmp_client = tmp_client->next)
        {
#if 0
            printf("JACK client#%d: '%s' source:%d sink:%d\n", num_clients,
                tmp_client->name, tmp_client->output, tmp_client->input);
#endif
            jack_client_names[num_clients] = tmp_client->name;
            tmp_client->name = 0; /* so we don't free it later */
            if(tmp_client->input) {
                if(jack_defaultsink < 0)
                    jack_defaultsink = num_clients;
                if ((jack_physicalsink < 0) && (tmp_client->input &
                    JackPortIsPhysical))
                        jack_physicalsink = num_clients;
            }
            if(tmp_client->output) {
                if(jack_defaultsource < 0)
                    jack_defaultsource = num_clients;
                if ((jack_physicalsource < 0) && (tmp_client->output &
                    JackPortIsPhysical))
                        jack_physicalsource = num_clients;
            }
            num_clients++;
        }

            /* clean up */
        tmp_client = available_clients;
        while(tmp_client) {
            t_jclient*next = tmp_client->next;
            free((void*)tmp_client->name);
            freebytes(tmp_client, sizeof(*tmp_client));
            tmp_client = next;
        }
    }
#if 0
    for (i=0;i<num_clients;i++) post("client: %s",jack_client_names[i]);
#endif

    free(jack_ports);
    regfree(&port_regex);
    freebytes(tmp_client_name, tmp_client_name_size);

        /* if we have a physical client, prefer that */
    if(jack_physicalsource >= 0)
        jack_defaultsource = jack_physicalsource;
    if(jack_physicalsink >= 0)
        jack_defaultsink = jack_physicalsink;

    if (jack_defaultsource < 0)
        jack_defaultsource = 0;
    if (jack_defaultsink < 0)
        jack_defaultsink = 0;

    return jack_client_names;
}

/*
 *   Wire up all the ports of one client.
 *
 */

static int jack_connect_ports(const char* source, const char* sink)
{
    char  regex_pattern[100]; /* its always the same, ... */
    int i;
    const char **jack_ports;
    int ret = -1;

    if (source && strlen(source) <= 96) {
        sprintf(regex_pattern, "%s:.*", source);

        jack_ports = jack_get_ports(jack_client, regex_pattern,
                                    NULL, JackPortIsOutput);
        if (jack_ports)
        {
            for (i=0;jack_ports[i] != NULL && i < STUFF->st_inchannels;i++)
                if (jack_connect (jack_client, jack_ports[i],
                                  jack_port_name (input_port[i])))
                    pd_error(0, "JACK: cannot connect input ports %s -> %s",
                             jack_ports[i],jack_port_name (input_port[i]));
                else
                    ret = 0;
            free(jack_ports);
        }
    }
    if (sink && strlen(sink) <= 96) {
        sprintf(regex_pattern, "%s:.*", sink);

        jack_ports = jack_get_ports(jack_client, regex_pattern,
                                    NULL, JackPortIsInput);
        if (jack_ports)
        {
            for (i=0;jack_ports[i] != NULL && i < STUFF->st_outchannels;i++)
                if (jack_connect (jack_client, jack_port_name(output_port[i]),
                                  jack_ports[i]))
                    pd_error(0,  "JACK: cannot connect output ports %s -> %s",
                             jack_port_name (output_port[i]),jack_ports[i]);
                else
                    ret = 0;

            free(jack_ports);
        }
    }

    return ret;
}


static void pd_jack_error_callback(const char *desc)
{
    sys_lock();
    logpost(0, PD_DEBUG, "JACK error: %s", desc);
    sys_unlock();
    return;
}

int jack_open_audio(int inchans, int outchans, t_audiocallback callback)
{
    int j, advance_samples;
    char port_name[80] = "";
    char client_name[80] = "";

    int client_iterator = 0;
    jack_status_t status;

    if (!jack_client_open)
    {
        pd_error(0, "Can't open Jack (it seems not to be installed)");
        return 1;
    }
    if (jack_client)
        jack_close_audio();

#ifdef THREADSIGNAL
    jack_sem = sys_semaphore_create();
#endif
    jack_didshutdown = 0;
    jack_dio_error = 0;

    if ((inchans == 0) && (outchans == 0)) return 1;

    if (outchans > MAX_JACK_PORTS) {
        pd_error(0, "JACK: %d output ports not supported, setting to %d",
            outchans, MAX_JACK_PORTS);
        outchans = MAX_JACK_PORTS;
    }

    if (inchans > MAX_JACK_PORTS) {
        pd_error(0, "JACK: %d input ports not supported, setting to %d",
            inchans, MAX_JACK_PORTS);
        inchans = MAX_JACK_PORTS;
    }

    /* try to become a client of the JACK server.  (If no JACK server exists,
        jack_client_open() don't start one up by default.  It's not clear
        whether or not this is desirable; see long Pd list thread started by
        yvan volochine, June 2013) */    
    jack_client = jack_client_open(sys_devicename, JackNoStartServer,
      &status, NULL);
    if (status & JackFailure) {
        pd_error(0, "JACK: couldn't connect to server, is JACK running?");
        logpost(NULL, PD_VERBOSE, "JACK: returned status is: %d", status);
        jack_client=NULL;
        /* jack spits out enough messages already, do not warn */
        STUFF->st_inchannels = STUFF->st_outchannels = 0;
        return 1;
    } 
    logpost(NULL, PD_VERBOSE, "JACK: registered as '%s'", jack_get_client_name(jack_client));

    STUFF->st_inchannels = inchans;
    STUFF->st_outchannels = outchans;

    jack_get_clients();

    /* set JACK callback functions */

    jack_callback = callback;
    jack_set_process_callback(jack_client,
        (callback? callbackprocess : jack_polling_callback), 0);

    jack_set_error_function (pd_jack_error_callback);

#ifdef JACK_XRUN
    jack_set_xrun_callback (jack_client, jack_xrun, NULL);
#endif

        /* tell the JACK server to call `jack_srate()' whenever the
        sample rate of the system changes. Curiously, this immediately
        fires the callback, so we need to guard it. See jack_srate() */
    jack_isopening = 1;
    jack_set_sample_rate_callback (jack_client, jack_srate, 0);
    jack_isopening = 0;

    /* tell the JACK server to call `jack_bsize()' whenever
       the buffer size of the system changes.
    */

    jack_set_buffer_size_callback (jack_client, jack_bsize, 0);

    /* tell the JACK server to call `jack_shutdown()' if
       it ever shuts down, either entirely, or if it
       just decides to stop calling us.
    */

    jack_on_shutdown (jack_client, jack_shutdown, 0);

    for (j=0; j<STUFF->st_inchannels; j++)
         input_port[j]=NULL;
    for (j=0; j<STUFF->st_outchannels; j++)
         output_port[j] = NULL;

    /* display the current sample rate & block size. once the client is activated
       (see below), you should rely on your own sample rate
       callback (see above) for this value.
    */

    STUFF->st_dacsr = jack_get_sample_rate (jack_client);
    jack_blocksize = jack_get_buffer_size (jack_client);
    advance_samples = sys_schedadvance * (double)STUFF->st_dacsr / 1.e6;
    advance_samples -= (advance_samples % DEFDACBLKSIZE);
        /* make sure that the delay is not smaller than the Jack blocksize! */
    if (!callback && advance_samples < jack_blocksize)
    {
        int delay = ((double)sys_schedadvance / 1000.) + 0.5;
        int limit = ceil(jack_blocksize * 1000. / (double)STUFF->st_dacsr);
        advance_samples = jack_blocksize;
        post("warning: 'delay' setting (%d ms) too small for current blocksize "
             "(%d samples); falling back to minimum value (%d ms)",
             delay, jack_blocksize, limit);
    }

    /* create the ports */

    for (j = 0; j < inchans; j++)
    {
        sprintf(port_name, "input_%d", j+1);
        if (!input_port[j]) input_port[j] = jack_port_register (jack_client,
            port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        if (!input_port[j])
        {
          pd_error(0, "JACK: can only register %d input ports (of %d requested)",
            j, inchans);
          STUFF->st_inchannels = inchans = j;
          break;
        }
    }

    for (j = 0; j < outchans; j++)
    {
        sprintf(port_name, "output_%d", j+1);
        if (!output_port[j]) output_port[j] = jack_port_register (jack_client,
            port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (!output_port[j])
        {
            pd_error(0, "JACK: can only register %d output ports (of %d requested)",
                j, outchans);
            STUFF->st_outchannels = outchans = j;
            break;
        }
    }

        /* create ring buffers (if not callback) */

    if (!callback && STUFF->st_inchannels)
    {
        jack_inbuf = malloc(sizeof(t_sample) * STUFF->st_inchannels
            * advance_samples);
        sys_ringbuf_init(&jack_inring,
            sizeof(t_sample) * STUFF->st_inchannels * advance_samples,
                jack_inbuf,
                    sizeof(t_sample) * STUFF->st_inchannels * advance_samples);
    }
    if (!callback && STUFF->st_outchannels)
    {
        jack_outbuf = malloc(sizeof(t_sample) * STUFF->st_outchannels
             * advance_samples);
        sys_ringbuf_init(&jack_outring,
            sizeof(t_sample) * STUFF->st_outchannels * advance_samples,
                jack_outbuf, 0);
    }

    /* tell the JACK server that we are ready to roll */

        /* this calls the jack_block_size() callback */
    if (jack_activate (jack_client))
    {
        pd_error(0, "cannot activate client");
        STUFF->st_inchannels = STUFF->st_outchannels = 0;
        return 1;
    }

    if (jack_client_names[0] && jack_should_autoconnect)
        jack_connect_ports(jack_client_names[jack_defaultsource], jack_client_names[jack_defaultsink]);
    return 0;
}

void jack_close_audio(void)
{
    if (jack_client){
        jack_deactivate (jack_client);
        jack_client_close(jack_client);
        jack_client = 0;
    }
    if (jack_inbuf)
        free(jack_inbuf), jack_inbuf = 0;
    if (jack_outbuf)
        free(jack_outbuf), jack_outbuf = 0;

    jack_started = 0;
    jack_blocksize = 0;

#ifdef THREADSIGNAL
    if (jack_sem)
    {
        sys_semaphore_destroy(jack_sem);
        jack_sem = 0;
    }
#endif
}

void sys_do_close_audio(void);

    /* Always called with Pd locked! */
int jack_reopen_audio(void)
{
        /* we don't actually try to reopen (yet?) */
    if (jack_didshutdown)
    {
        pd_error(0, "JACK: server shutdown");
        jack_didshutdown = 0;
    }
    sys_do_close_audio();
    return 0;
}

int sched_idletask(void);

int jack_send_dacs(void)
{
    t_sample *muxbuffer;
    t_sample *fp, *fp2, *jp;
    int j, ch;
    const size_t muxbufsize = DEFDACBLKSIZE *
        (STUFF->st_inchannels > STUFF->st_outchannels ?
         STUFF->st_inchannels : STUFF->st_outchannels);
    int retval = SENDDACS_YES;
        /* this shouldn't really happen... */
    if (!jack_client || (!STUFF->st_inchannels && !STUFF->st_outchannels))
        return (SENDDACS_NO);

    while (
        (sys_ringbuf_getreadavailable(&jack_inring) <
            (long)(STUFF->st_inchannels * DEFDACBLKSIZE*sizeof(t_sample))) ||
        (sys_ringbuf_getwriteavailable(&jack_outring) <
            (long)(STUFF->st_outchannels * DEFDACBLKSIZE*sizeof(t_sample))))
    {
        if (jack_didshutdown)
        {
            sys_lock();
            jack_reopen_audio(); /* handle server shutdown */
            sys_unlock();
            return (SENDDACS_NO);
        }
#ifdef THREADSIGNAL
        if (sched_idletask())
            continue;
            /* only go to sleep if there is nothing else to do. */
        sys_semaphore_wait(jack_sem);
        retval = SENDDACS_SLEPT;
#else
        return (SENDDACS_NO);
#endif
    }
    if (jack_dio_error)
    {
        sys_log_error(ERR_RESYNC);
        jack_dio_error = 0;
    }
    jack_started = 1;

    ALLOCA(t_sample, muxbuffer, muxbufsize, MAX_ALLOCA_SAMPLES);
    if (STUFF->st_inchannels)
    {
        sys_ringbuf_read(&jack_inring, muxbuffer,
            DEFDACBLKSIZE * STUFF->st_inchannels * sizeof(t_sample),
                jack_inbuf);
        for (fp = muxbuffer, ch = 0; ch < STUFF->st_inchannels; ch++, fp++)
        {
            jp = STUFF->st_soundin + ch * DEFDACBLKSIZE;
            for (j = 0, fp2 = fp; j < DEFDACBLKSIZE;
                j++, fp2 += STUFF->st_inchannels)
                    jp[j] = *fp2;
        }
    }
    if (STUFF->st_outchannels)
    {
        for (fp = muxbuffer, ch = 0; ch < STUFF->st_outchannels; ch++, fp++)
        {
            jp = STUFF->st_soundout + ch * DEFDACBLKSIZE;
            for (j = 0, fp2 = fp; j < DEFDACBLKSIZE;
                j++, fp2 += STUFF->st_outchannels)
                    *fp2 = jp[j];
        }
        sys_ringbuf_write(&jack_outring, muxbuffer,
            DEFDACBLKSIZE * STUFF->st_outchannels * sizeof(t_sample),
                jack_outbuf);
    }
    memset(STUFF->st_soundout, 0,
        DEFDACBLKSIZE*sizeof(t_sample) * STUFF->st_outchannels);
    FREEA(t_sample, muxbuffer, muxbufsize, MAX_ALLOCA_SAMPLES);
    return retval;
}

void jack_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti,
        int maxndev, int devdescsize)
{
    int i, ndev;
    *canmulti = 0;  /* supports multiple devices */
    ndev = 1;
    for (i = 0; i < ndev; i++)
    {
        sprintf(indevlist + i * devdescsize, "JACK");
        sprintf(outdevlist + i * devdescsize, "JACK");
    }
    *nindevs = *noutdevs = ndev;
}

void jack_listdevs(void)
{
    post("device listing not implemented for jack yet\n");
}

void jack_autoconnect(int v)
{
    jack_should_autoconnect = v;
}

int jack_get_blocksize(void)
{
    return jack_blocksize;
}

#endif /* JACK */
