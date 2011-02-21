

/* ----------------------- Experimental routines for jack -------------- */
#ifdef USEAPI_JACK

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m_pd.h"
#include "s_stuff.h"
#include <jack/jack.h>
#include <regex.h>

#define MAX_CLIENTS 100
#define MAX_JACK_PORTS 128  /* seems like higher values give bad xrun problems */
#define BUF_JACK 4096
static jack_nframes_t jack_out_max;
#define JACK_OUT_MAX  64
static jack_nframes_t jack_filled = 0;
static t_sample *jack_outbuf;
static t_sample *jack_inbuf;
static int jack_started = 0;


static jack_port_t *input_port[MAX_JACK_PORTS];
static jack_port_t *output_port[MAX_JACK_PORTS];
static int outport_count = 0;
static jack_client_t *jack_client = NULL;
char *jack_client_names[MAX_CLIENTS];
static int jack_dio_error;
static t_audiocallback jack_callback;

pthread_mutex_t jack_mutex;
pthread_cond_t jack_sem;


static int
process (jack_nframes_t nframes, void *arg)
{
    int j;
    jack_default_audio_sample_t *out, *in;

    pthread_mutex_lock(&jack_mutex);
    if (nframes > JACK_OUT_MAX) jack_out_max = nframes;
    else jack_out_max = JACK_OUT_MAX;
    if (jack_filled >= nframes)
    {
        if (jack_filled != nframes) fprintf(stderr,"Partial read");
        /* hmm, how to find out whether 't_sample' and
            'jack_default_audio_sample_t' are actually the same type??? */
        if (sizeof(t_sample)==sizeof(jack_default_audio_sample_t)) 
        {
            for (j = 0; j < sys_outchannels;  j++)
            {
                out = jack_port_get_buffer (output_port[j], nframes);
                memcpy(out, jack_outbuf + (j * BUF_JACK),
                    sizeof (jack_default_audio_sample_t) * nframes);
            }
            for (j = 0; j < sys_inchannels; j++)
            {
                in = jack_port_get_buffer( input_port[j], nframes);
                memcpy(jack_inbuf + (j * BUF_JACK), in,
                    sizeof (jack_default_audio_sample_t) * nframes);
            }
        } 
        else
        {
            unsigned int frame=0;
            t_sample*data;
            for (j = 0; j < sys_outchannels;  j++)
            {
                out = jack_port_get_buffer (output_port[j], nframes);
                data = jack_outbuf + (j * BUF_JACK);
                for(frame=0; frame<nframes; frame++)
                {
                    *out++=*data++;
                }
            }
            for (j = 0; j < sys_inchannels; j++)
            {
                in = jack_port_get_buffer( input_port[j], nframes);
                data = jack_inbuf + (j * BUF_JACK);
                for(frame=0; frame<nframes; frame++)
                {
                    *data++=*in++;
                }
            }
        }
        jack_filled -= nframes;
    }
    else
            { /* PD could not keep up ! */
        if (jack_started) jack_dio_error = 1;
        for (j = 0; j < outport_count;  j++)
        {
            out = jack_port_get_buffer (output_port[j], nframes);
            memset(out, 0, sizeof (float) * nframes); 
            memset(jack_outbuf + j * BUF_JACK, 0, BUF_JACK * sizeof(t_sample));
        }
        jack_filled = 0;
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
    for (chan = 0; chan < sys_inchannels; chan++)
        in[chan] = jack_port_get_buffer(input_port[chan], nframes);
    for (chan = 0; chan < sys_outchannels; chan++)
        out[chan] = jack_port_get_buffer(output_port[chan], nframes);
    for (n = 0; n < nframes; n += DEFDACBLKSIZE)
    {
        t_sample *fp;
        for (chan = 0; chan < sys_inchannels; chan++)
        {
            for (fp = sys_soundin + chan*DEFDACBLKSIZE,
                jp = in[chan] + n, j=0; j < DEFDACBLKSIZE; j++)
                    *fp++ = *jp++;
        }
        for (chan = 0; chan < sys_outchannels; chan++)
        {
            for (fp = sys_soundout + chan*DEFDACBLKSIZE,
                j = 0; j < DEFDACBLKSIZE; j++)
                    *fp++ = 0;
        }
        (*jack_callback)();
        for (chan = 0; chan < sys_outchannels; chan++)
        {
            for (fp = sys_soundout + chan*DEFDACBLKSIZE, jp = out[chan] + n,
                j=0; j < DEFDACBLKSIZE; j++)
                    *jp++ = *fp++;
        }
    }       
    return 0;
}

static int
jack_srate (jack_nframes_t srate, void *arg)
{
        sys_dacsr = srate;
        return 0;
}


void glob_audio_setapi(void *dummy, t_floatarg f);

static void
jack_shutdown (void *arg)
{
  error("JACK: server shut down");
  
  jack_deactivate (jack_client);
  //jack_client_close(jack_client); /* likely to hang if the server shut down */
  jack_client = NULL;

  glob_audio_setapi(NULL, API_NONE); // set pd_whichapi 0
}

static int jack_xrun(void* arg) {
  jack_dio_error = 1;
  return 0;
}


static char** jack_get_clients(void)
{
    const char **jack_ports;
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
        char tmp_client_name[100];

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
    for (i=0;jack_ports[i] != NULL && i < sys_inchannels;i++)      
      if (jack_connect (jack_client, jack_ports[i], jack_port_name (input_port[i]))) 
        error ("JACK: cannot connect input ports %s -> %s", jack_ports[i],jack_port_name (input_port[i]));
      
  
  
  jack_ports = jack_get_ports( jack_client, regex_pattern,
                               NULL, JackPortIsInput);
  if (jack_ports) 
    for (i=0;jack_ports[i] != NULL && i < sys_outchannels;i++)      
      if (jack_connect (jack_client, jack_port_name (output_port[i]), jack_ports[i])) 
        error( "JACK: cannot connect output ports %s -> %s", jack_port_name (output_port[i]),jack_ports[i]);
  
  
  
  free(jack_ports);
  return 0;
}


static void pd_jack_error_callback(const char *desc) {
  error("JACKerror: %s", desc);
  return;
}

int
jack_open_audio(int inchans, int outchans, int rate, t_audiocallback callback)
{
    int j;
    char port_name[80] = "";
    char client_name[80] = "";

    int client_iterator = 0;
    int new_jack = 0;
    int srate;
    jack_status_t status;

    if (NULL==jack_client_new)
    {
        fprintf(stderr,"JACK framework not available\n");
        return 1;
    }

    jack_dio_error = 0;

    if ((inchans == 0) && (outchans == 0)) return 0;

    if (outchans > MAX_JACK_PORTS) {
        error("JACK: %d output ports not supported, setting to %d",
            outchans, MAX_JACK_PORTS);
        outchans = MAX_JACK_PORTS;
    }

    if (inchans > MAX_JACK_PORTS) {
        error("JACK: %d input ports not supported, setting to %d",
            inchans, MAX_JACK_PORTS);
        inchans = MAX_JACK_PORTS;
    }
    /* try to become a client of the JACK server */
    /* if no JACK server exists, start a default one (jack_client_open() does that for us... */
    if (!jack_client) {
        do {
          sprintf(client_name,"pure_data_%d",client_iterator);
          client_iterator++;
          jack_client = jack_client_open (client_name, JackNullOption, &status, NULL);
          if (status & JackServerFailed) {
            error("JACK: unable to connect to JACK server");
            jack_client=NULL;
            break;
          }
        } while (status & JackNameNotUnique);

        if(status) {
          if (status & JackServerStarted) {
            verbose(1, "JACK: started server");
          } else {
            error("JACK: server returned status %d", status);
          }
        }
        verbose(1, "JACK: started server as '%s'", client_name);

        if (!jack_client) {
            /* jack spits out enough messages already, do not warn */
            sys_inchannels = sys_outchannels = 0;
            return 1;
        }
        
        sys_inchannels = inchans;
        sys_outchannels = outchans;
        if (jack_inbuf)
            free(jack_inbuf);
        if (sys_inchannels)
            jack_inbuf = calloc(sizeof(t_sample), sys_inchannels * BUF_JACK); 
        if (jack_outbuf)
            free(jack_outbuf);
        if (sys_outchannels)
            jack_outbuf = calloc(sizeof(t_sample), sys_outchannels * BUF_JACK); 

        jack_get_clients();

        /* tell the JACK server to call `process()' whenever
           there is work to be done.
        */
        jack_callback = callback;
        jack_set_process_callback (jack_client, 
            (callback? callbackprocess : process), 0);

        jack_set_error_function (pd_jack_error_callback);

#ifdef JACK_XRUN
      jack_set_xrun_callback (jack_client, jack_xrun, NULL);
#endif

        /* tell the JACK server to call `srate()' whenever
           the sample rate of the system changes.
        */

        jack_set_sample_rate_callback (jack_client, jack_srate, 0);


        /* tell the JACK server to call `jack_shutdown()' if
           it ever shuts down, either entirely, or if it
           just decides to stop calling us.
        */

        jack_on_shutdown (jack_client, jack_shutdown, 0);

        for (j=0; j<sys_inchannels; j++)
             input_port[j]=NULL;
        for (j=0; j<sys_outchannels; j++)
             output_port[j] = NULL;

        new_jack = 1;
    }

    /* display the current sample rate. once the client is activated
       (see below), you should rely on your own sample rate
       callback (see above) for this value.
    */

    srate = jack_get_sample_rate (jack_client);
    sys_dacsr = srate;

    /* create the ports */

    for (j = 0; j < inchans; j++) {
        sprintf(port_name, "input%d", j);
        if (!input_port[j]) input_port[j] = jack_port_register (jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        if (!input_port[j]) {
          error("JACK: can only register %d input ports (instead of requested %d)", j, inchans);
          sys_inchannels = inchans = j;
          break;
        }
    }

    for (j = 0; j < outchans; j++) {
        sprintf(port_name, "output%d", j);
        if (!output_port[j]) output_port[j] = jack_port_register (jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (!output_port[j]) {
          error("JACK: can only register %d output ports (instead of requested %d)", j, outchans);
          sys_outchannels = outchans = j;
          break;
        }
    } 
    outport_count = outchans;

    /* tell the JACK server that we are ready to roll */

    if (new_jack) {
        if (jack_activate (jack_client)) {
            error("cannot activate client");
            sys_inchannels = sys_outchannels = 0;
            return 1;
        }

        for (j = 0; j < outchans; j++) 
            memset(jack_outbuf + j * BUF_JACK, 0,
                BUF_JACK * sizeof(t_sample));

        if (jack_client_names[0])
            jack_connect_ports(jack_client_names[0]);

        pthread_mutex_init(&jack_mutex, NULL);
        pthread_cond_init(&jack_sem, NULL);
    }
    return 0;
}

void jack_close_audio(void) 
{
    if (jack_client){
        jack_deactivate (jack_client);
        jack_client_close(jack_client);
    }

    jack_client=NULL;
    jack_started = 0;

    pthread_cond_broadcast(&jack_sem);

    pthread_cond_destroy(&jack_sem);
    pthread_mutex_destroy(&jack_mutex);
    if (jack_inbuf)
        free(jack_inbuf), jack_inbuf = 0;
    if (jack_outbuf)
        free(jack_outbuf), jack_outbuf = 0;
 
}

int jack_send_dacs(void)

{
    t_sample * fp;
    int j;
    int rtnval =  SENDDACS_YES;
    int timenow;
    int timeref = sys_getrealtime();
    if (!jack_client) return SENDDACS_NO;
    if (!sys_inchannels && !sys_outchannels) return (SENDDACS_NO); 
    if (jack_dio_error)
    {
        sys_log_error(ERR_RESYNC);
        jack_dio_error = 0;
    }
    pthread_mutex_lock(&jack_mutex);
    if (jack_filled >= jack_out_max)
        pthread_cond_wait(&jack_sem,&jack_mutex);

    if (!jack_client)
    {
        pthread_mutex_unlock(&jack_mutex);
        return SENDDACS_NO;
    }
    jack_started = 1;

    fp = sys_soundout;
    for (j = 0; j < sys_outchannels; j++)
    {
        memcpy(jack_outbuf + (j * BUF_JACK) + jack_filled, fp,
            DEFDACBLKSIZE*sizeof(t_sample));
        fp += DEFDACBLKSIZE;  
    }
    fp = sys_soundin;
    for (j = 0; j < sys_inchannels; j++)
    {
        memcpy(fp, jack_inbuf + (j * BUF_JACK) + jack_filled,
            DEFDACBLKSIZE*sizeof(t_sample));
        fp += DEFDACBLKSIZE;
    }
    jack_filled += DEFDACBLKSIZE;
    pthread_mutex_unlock(&jack_mutex);

    if ((timenow = sys_getrealtime()) - timeref > 0.002)
    {
        rtnval = SENDDACS_SLEPT;
    }
    memset(sys_soundout, 0, DEFDACBLKSIZE*sizeof(t_sample)*sys_outchannels);
    return rtnval;
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

void jack_listdevs( void)
{
    post("device listing not implemented for jack yet\n");
}

#endif /* JACK */
