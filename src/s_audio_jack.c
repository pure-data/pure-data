

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
#define NUM_JACK_PORTS 32
#define BUF_JACK 4096
static jack_nframes_t jack_out_max;
#define JACK_OUT_MAX  64
static jack_nframes_t jack_filled = 0;
static float jack_outbuf[NUM_JACK_PORTS*BUF_JACK];
static float jack_inbuf[NUM_JACK_PORTS*BUF_JACK];
static int jack_started = 0;


static jack_port_t *input_port[NUM_JACK_PORTS];
static jack_port_t *output_port[NUM_JACK_PORTS];
static int outport_count = 0;
static jack_client_t *jack_client = NULL;
char *jack_client_names[MAX_CLIENTS];
static int jack_dio_error;


pthread_mutex_t jack_mutex;
pthread_cond_t jack_sem;


static int
process (jack_nframes_t nframes, void *arg)
{
        int j;
        float *out; 
        float *in;
        
        
        if (nframes > JACK_OUT_MAX) jack_out_max = nframes;
        else jack_out_max = JACK_OUT_MAX;
        if (jack_filled >= nframes) {
                if (jack_filled != nframes) fprintf(stderr,"Partial read");

                for (j = 0; j < sys_outchannels;  j++) {
                        out = jack_port_get_buffer (output_port[j], nframes);
                        memcpy(out, jack_outbuf + (j * BUF_JACK), sizeof (float) * nframes);
                }
                for (j = 0; j < sys_inchannels; j++) {
                        in = jack_port_get_buffer( input_port[j], nframes);
                        memcpy(jack_inbuf + (j * BUF_JACK), in, sizeof (float) * nframes);
                }
                jack_filled -= nframes;
        } else { /* PD could not keep up ! */
             if (jack_started) jack_dio_error = 1;
             for (j = 0; j < outport_count;  j++) {
                  out = jack_port_get_buffer (output_port[j], nframes);
                  memset(out, 0, sizeof (float) * nframes); 
             }
             memset(jack_outbuf,0,sizeof(jack_outbuf));
             jack_filled = 0;
        }
        pthread_cond_broadcast(&jack_sem);
        return 0;
}

static int
jack_srate (jack_nframes_t srate, void *arg)
{
        sys_dacsr = srate;
        return 0;
}

static void
jack_shutdown (void *arg)
{
        /* Ignore for now */
  //    exit (1);
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
        fprintf (stderr, "cannot connect input ports %s -> %s\n", jack_ports[i],jack_port_name (input_port[i]));
      
  
  
  jack_ports = jack_get_ports( jack_client, regex_pattern,
                               NULL, JackPortIsInput);
  if (jack_ports) 
    for (i=0;jack_ports[i] != NULL && i < sys_outchannels;i++)      
      if (jack_connect (jack_client, jack_port_name (output_port[i]), jack_ports[i])) 
        fprintf (stderr, "cannot connect output ports %s -> %s\n", jack_port_name (output_port[i]),jack_ports[i]);
  
  
  
  free(jack_ports);
  return 0;
}


void jack_error_callback(const char *desc) {
  return;
}


int
jack_open_audio(int inchans, int outchans, int rate)

{
        int j;
        char port_name[80] = "";
        int client_iterator = 0;
        int new_jack = 0;
        int srate;

        jack_dio_error = 0;
        
        if ((inchans == 0) && (outchans == 0)) return 0;

        if (outchans > NUM_JACK_PORTS) {
                fprintf(stderr,"%d output ports not supported, setting to %d\n",outchans, NUM_JACK_PORTS);
                outchans = NUM_JACK_PORTS;
        }

        if (inchans > NUM_JACK_PORTS) {
                fprintf(stderr,"%d input ports not supported, setting to %d\n",inchans, NUM_JACK_PORTS);
                inchans = NUM_JACK_PORTS;
        }

        /* try to become a client of the JACK server (we allow two pd's)*/
        if (!jack_client) {
          do {
            sprintf(port_name,"pure_data_%d",client_iterator);
            client_iterator++;
          } while (((jack_client = jack_client_new (port_name)) == 0) && client_iterator < 2);
        
          
          if (!jack_client) { // jack spits out enough messages already, do not warn
            sys_inchannels = sys_outchannels = 0;
            return 1;
          }
          
          jack_get_clients();

          /* tell the JACK server to call `process()' whenever
             there is work to be done.
          */
          
          jack_set_process_callback (jack_client, process, 0);
          
          jack_set_error_function (jack_error_callback);
          
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
          
          for (j=0;j<NUM_JACK_PORTS;j++) {
               input_port[j]=NULL;
               output_port[j] = NULL;
          }
          
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
        }

        for (j = 0; j < outchans; j++) {
                sprintf(port_name, "output%d", j);
                if (!output_port[j]) output_port[j] = jack_port_register (jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        } 
        outport_count = outchans;

        /* tell the JACK server that we are ready to roll */
 
        if (new_jack) {
          if (jack_activate (jack_client)) {
            fprintf (stderr, "cannot activate client\n");
            sys_inchannels = sys_outchannels = 0;
            return 1;
          }
          
          memset(jack_outbuf,0,sizeof(jack_outbuf));
          
          if (jack_client_names[0])
            jack_connect_ports(jack_client_names[0]);

          pthread_mutex_init(&jack_mutex,NULL);
          pthread_cond_init(&jack_sem,NULL);
        }
        return 0;
}

void jack_close_audio(void) 

{
        jack_started = 0;
}

int jack_send_dacs(void)

{
        float * fp;
        int j;
        int rtnval =  SENDDACS_YES;
        int timenow;
        int timeref = sys_getrealtime();
                
        if (!jack_client) return SENDDACS_NO;

        if (!sys_inchannels && !sys_outchannels) return (SENDDACS_NO); 

        if (jack_dio_error) {
                sys_log_error(ERR_RESYNC);
                jack_dio_error = 0;
        }
        if (jack_filled >= jack_out_max)
          pthread_cond_wait(&jack_sem,&jack_mutex);
          
        jack_started = 1;

        fp = sys_soundout;
        for (j = 0; j < sys_outchannels; j++) {
                memcpy(jack_outbuf + (j * BUF_JACK) + jack_filled,fp, DEFDACBLKSIZE*sizeof(float));
                fp += DEFDACBLKSIZE;  
        }
        fp = sys_soundin;
        for (j = 0; j < sys_inchannels; j++) {
                memcpy(fp, jack_inbuf + (j * BUF_JACK) + jack_filled, DEFDACBLKSIZE*sizeof(float));
                fp += DEFDACBLKSIZE;
        }

        if ((timenow = sys_getrealtime()) - timeref > 0.002)
          {
            rtnval = SENDDACS_SLEPT;
          }

        memset(sys_soundout,0,DEFDACBLKSIZE*sizeof(float)*sys_outchannels);
        jack_filled += DEFDACBLKSIZE;
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
