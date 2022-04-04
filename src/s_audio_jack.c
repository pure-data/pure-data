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
#include "s_stuff.h"
#include "s_audio_paring.h"
#ifdef __APPLE__
#include <jack/weakjack.h>
#endif
#include <jack/jack.h>
#include <regex.h>

#define MAX_CLIENTS 100
#define MAX_JACK_PORTS 128  /* higher values seem to give bad xrun problems */
#define BUF_JACK 4096
/* taken from the PipeWire libjack implementation: the larger of the
 * `JACK_CLIENT_NAME_SIZE` definitions I could find in the wild. */
#define CLIENT_NAME_SIZE_FALLBACK 128

static jack_nframes_t jack_out_max;
static jack_nframes_t jack_filled = 0;
static int jack_started = 0;
static jack_port_t *input_port[MAX_JACK_PORTS];
static jack_port_t *output_port[MAX_JACK_PORTS];
static jack_client_t *jack_client = NULL;
static char * desired_client_name = NULL;
char *jack_client_names[MAX_CLIENTS];
static int jack_dio_error;
static t_audiocallback jack_callback;
static int jack_should_autoconnect = 1;
static int jack_blocksize = 0; /* should this be PERTHREAD? */
pthread_mutex_t jack_mutex;
pthread_cond_t jack_sem;
static PA_VOLATILE char *jack_outbuf;
static PA_VOLATILE sys_ringbuf jack_outring;
static PA_VOLATILE char *jack_inbuf;
static PA_VOLATILE sys_ringbuf jack_inring;

/* #define TESTCANSLEEP */

    /* callback routine for non-callback client... throw samples into
        and read them out of a FIFO.  Since we don't know at compile time
        how many samples jack will treat us to, we interleave them in the
        two FIFos.  So we have to mux/demux them both here and up at the
        user level in jack_send_dacs(). */
static int jack_polling_callback(jack_nframes_t nframes, void *unused)
{
    unsigned long infiforoom = sys_ringbuf_getwriteavailable(&jack_inring),
        outfiforoom = sys_ringbuf_getreadavailable(&jack_outring);
    t_sample *muxbuffer =  (t_sample *)alloca(sizeof (t_sample) * nframes *
        (STUFF->st_inchannels > STUFF->st_outchannels ?
            STUFF->st_inchannels : STUFF->st_outchannels));
    int j;
    jack_default_audio_sample_t *jp;

        /* even though the FIFO is lock-free we have to lock here
        to prevent a race condition in the waiting thread between
        the FIFO and the pthread_cond_wait() call. */
    pthread_mutex_lock(&jack_mutex);
    if (infiforoom < nframes * STUFF->st_inchannels * sizeof(t_sample) ||
        outfiforoom < nframes * STUFF->st_outchannels * sizeof(t_sample))
    {
        /* data late: output zeros, drop inputs, and leave FIFos untouched */
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
    pthread_cond_broadcast(&jack_sem);
    pthread_mutex_unlock(&jack_mutex);
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

static int
jack_srate (jack_nframes_t srate, void *arg)
{
    const t_float oldrate = STUFF->st_dacsr;
    STUFF->st_dacsr = srate;
    if (oldrate != STUFF->st_dacsr)
        canvas_update_dsp();
    return 0;
}

static int
jack_bsize (jack_nframes_t bufsize, void *arg)
{
    jack_blocksize = bufsize;
    return 0;
}

void glob_audio_setapi(void *dummy, t_floatarg f);

static void
jack_shutdown (void *arg)
{
  pd_error(0, "JACK: server shut down");

  jack_deactivate (jack_client);
  jack_client = NULL;
  jack_blocksize = 0;

  glob_audio_setapi(NULL, API_NONE); // set pd_whichapi 0
}

static int jack_xrun(void* arg) {
  jack_dio_error = 1;
  return 0;
}


static char** jack_get_clients(void)
{
    const char **jack_ports;
    int tmp_client_name_size = jack_client_name_size ? jack_client_name_size() : CLIENT_NAME_SIZE_FALLBACK;
    char* tmp_client_name = (char*)getbytes(tmp_client_name_size);
    int i,j;
    int num_clients = 0;
    regex_t port_regex;
    jack_ports = jack_get_ports( jack_client, "", "", 0 );
    regcomp( &port_regex, "^[^:]*", REG_EXTENDED );

    jack_client_names[0] = NULL;

    /* Build a list of clients from the list of ports */
    for( i = 0; jack_ports[i] != NULL; i++ )
    {
        int client_seen;
        regmatch_t match_info;

        if(num_clients>=MAX_CLIENTS)break;


        /* extract the client name from the port name, using a regex
         * that parses the clientname:portname syntax */
        regexec( &port_regex, jack_ports[i], 1, &match_info, 0 );
        memcpy( tmp_client_name, &jack_ports[i][match_info.rm_so],
                match_info.rm_eo - match_info.rm_so );
        tmp_client_name[ match_info.rm_eo - match_info.rm_so ] = '\0';

        /* do we know about this port's client yet? */
        client_seen = 0;

        for( j = 0; j < num_clients; j++ )
            if( strcmp( tmp_client_name, jack_client_names[j] ) == 0 )
                client_seen = 1;

        if( client_seen == 0 )
        {
            jack_client_names[num_clients] = (char*)getbytes(strlen(tmp_client_name) + 1);

            /* The alsa_pcm client should go in spot 0.  If this
             * is the alsa_pcm client AND we are NOT about to put
             * it in spot 0 put it in spot 0 and move whatever
             * was already in spot 0 to the end. */

            if( strcmp( "alsa_pcm", tmp_client_name ) == 0 && num_clients > 0 )
            {
              char* tmp;
                /* alsa_pcm goes in spot 0 */
              tmp = jack_client_names[ num_clients ];
              jack_client_names[ num_clients ] = jack_client_names[0];
              jack_client_names[0] = tmp;
              strcpy( jack_client_names[0], tmp_client_name);
            }
            else
            {
                /* put the new client at the end of the client list */
                strcpy( jack_client_names[ num_clients ], tmp_client_name );
            }
            num_clients++;
        }
    }

    /*    for (i=0;i<num_clients;i++) post("client: %s",jack_client_names[i]); */

    freebytes( tmp_client_name, tmp_client_name_size );
    free( jack_ports );
    return jack_client_names;
}

/*
 *   Wire up all the ports of one client.
 *
 */

static int jack_connect_ports(char* client)
{
    char  regex_pattern[100]; /* its always the same, ... */
    int i;
    const char **jack_ports;

    if (strlen(client) > 96)  return -1;

    sprintf( regex_pattern, "%s:.*", client );

    jack_ports = jack_get_ports( jack_client, regex_pattern,
                                 NULL, JackPortIsOutput);
    if (jack_ports)
    {
        for (i=0;jack_ports[i] != NULL && i < STUFF->st_inchannels;i++)
            if (jack_connect (jack_client, jack_ports[i],
               jack_port_name (input_port[i])))
                  pd_error(0, "JACK: cannot connect input ports %s -> %s",
                      jack_ports[i],jack_port_name (input_port[i]));
        free(jack_ports);
    }
    jack_ports = jack_get_ports( jack_client, regex_pattern,
                                 NULL, JackPortIsInput);
    if (jack_ports)
    {
        for (i=0;jack_ports[i] != NULL && i < STUFF->st_outchannels;i++)
          if (jack_connect (jack_client, jack_port_name (output_port[i]),
            jack_ports[i]))
              pd_error(0,  "JACK: cannot connect output ports %s -> %s",
                jack_port_name (output_port[i]),jack_ports[i]);

        free(jack_ports);
    }
    return 0;
}


static void pd_jack_error_callback(const char *desc) {
  pd_error(0, "JACKerror: %s", desc);
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

    jack_dio_error = 0;

    if ((inchans == 0) && (outchans == 0)) return 0;

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
        jack_client_open() will start uone up by default.  It's not clear
        whether or not this is desirable; see long Pd list thread started by
        yvan volochine, June 2013) */
    if (!desired_client_name || !strlen(desired_client_name))
        jack_client_name("pure_data");
    jack_client = jack_client_open (desired_client_name, JackNoStartServer,
      &status, NULL);
    if (status & JackFailure) {
        pd_error(0, "JACK: couldn't connect to server, is JACK running?");
        logpost(NULL, PD_VERBOSE, "JACK: returned status is: %d", status);
        jack_client=NULL;
        /* jack spits out enough messages already, do not warn */
        STUFF->st_inchannels = STUFF->st_outchannels = 0;
        return 1;
    }
    if (status & JackNameNotUnique)
        jack_client_name(jack_get_client_name(jack_client));
    logpost(NULL, PD_VERBOSE, "JACK: registered as '%s'", desired_client_name);

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

    /* tell the JACK server to call `jack_srate()' whenever
       the sample rate of the system changes.
    */

    jack_set_sample_rate_callback (jack_client, jack_srate, 0);

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
    if (advance_samples < jack_blocksize)
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

    if (jack_activate (jack_client))
    {
        pd_error(0, "cannot activate client");
        STUFF->st_inchannels = STUFF->st_outchannels = 0;
        return 1;
    }

    if (jack_client_names[0] && jack_should_autoconnect)
        jack_connect_ports(jack_client_names[0]);

    pthread_mutex_init(&jack_mutex, NULL);
    pthread_cond_init(&jack_sem, NULL);

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

        /* this should never be necessary since jack_close_audio() should
        only be called form the main thread.  Still, it doesn't hurt
        anything. */
    pthread_cond_broadcast(&jack_sem);

    pthread_cond_destroy(&jack_sem);
    pthread_mutex_destroy(&jack_mutex);
}

int jack_send_dacs(void)
{
    unsigned long infiforoom, outfiforoom;
    t_sample *muxbuffer;
    t_sample *fp, *fp2, *jp;
    int j, ch;
    double timenow, timeref = sys_getrealtime();
    if (!STUFF->st_inchannels && !STUFF->st_outchannels) return (SENDDACS_NO);

#ifdef TESTCANSLEEP
    pthread_mutex_lock(&jack_mutex);
    while (jack_client &&
        (sys_ringbuf_getreadavailable(&jack_inring) <
            (long)(STUFF->st_inchannels * DEFDACBLKSIZE*sizeof(t_sample))) &&
        (sys_ringbuf_getwriteavailable(&jack_outring) <
            (long)(STUFF->st_outchannels * DEFDACBLKSIZE*sizeof(t_sample))))
                pthread_cond_wait(&jack_sem,&jack_mutex);
    pthread_mutex_unlock(&jack_mutex);
    if (!jack_client)
        return SENDDACS_NO;
#else
    if (!jack_client ||
        (sys_ringbuf_getreadavailable(&jack_inring) <
            (long)(STUFF->st_inchannels * DEFDACBLKSIZE*sizeof(t_sample))) ||
        (sys_ringbuf_getwriteavailable(&jack_outring) <
            (long)(STUFF->st_outchannels * DEFDACBLKSIZE*sizeof(t_sample))))
                return (SENDDACS_NO);
#endif
    if (jack_dio_error)
    {
        sys_log_error(ERR_RESYNC);
        jack_dio_error = 0;
    }
    jack_started = 1;

    muxbuffer =  (t_sample *)alloca(sizeof (t_sample) * DEFDACBLKSIZE *
        (STUFF->st_inchannels > STUFF->st_outchannels ?
            STUFF->st_inchannels : STUFF->st_outchannels));
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
            /* fprintf(stderr, "%g ", sys_getrealtime() - timeref); */
    if ((timenow = sys_getrealtime()) - timeref > 0.0002)
        return (SENDDACS_SLEPT);
    else return (SENDDACS_YES);
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

void jack_client_name(const char *name)
{
    if (desired_client_name) {
        free(desired_client_name);
        desired_client_name = NULL;
    }
    if (name) {
        desired_client_name = (char*)getbytes(strlen(name) + 1);
        strcpy(desired_client_name, name);
    }
}

int jack_get_blocksize(void)
{
    return jack_blocksize;
}

#endif /* JACK */
