/* Copyright (c) 2020, IOhannes m zmölnig
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  Audio back-end for connecting with the Pulse Audio sound server
*/

#include <string.h>
#include <stdlib.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "m_pd.h"
#include "s_stuff.h"

typedef struct _pd_pulseclient {
    pa_simple*client;
    pa_sample_spec spec;
    float*buffer;
    size_t bufsize;
} t_pd_pulseclient;

static t_pd_pulseclient *pulse_sink = NULL;
static t_pd_pulseclient *pulse_source = NULL;

static t_pd_pulseclient*free_client(t_pd_pulseclient*client) {
    if(!client)
        return NULL;
    if (client->buffer)
        freebytes(client->buffer, client->bufsize);
    if (client->client)
        pa_simple_free(client->client);
    freebytes(client, sizeof(*client));
    return NULL;
}

static t_pd_pulseclient*make_client(pa_stream_direction_t dir, int channels, int rate)
{
    const char *client_name = sys_get_audio_clientname("Pure Data");
    char *stream_name = "Music";
    t_pd_pulseclient*client = getbytes(sizeof(t_pd_pulseclient));
    if(!client)
        return 0;

    if(channels < 0)
        channels = 0;
    if(channels > 255)
        channels = 255;

    client->spec.format = PA_SAMPLE_FLOAT32;
    client->spec.rate = rate;
    client->spec.channels = channels;
    client->bufsize = sizeof(*client->buffer) * channels * DEFDACBLKSIZE;
    client->buffer = getbytes(client->bufsize);

    client->client = pa_simple_new(
        NULL,               // Use the default server.
        client_name,        // Our application's name.
        dir,
        NULL,               // Use the default device.
        stream_name,        // Description of our stream.
        &client->spec,          // Our sample format.
        NULL, NULL, NULL);

    if (!client->buffer || !client->client) {
        client = free_client(client);
    }
    return client;
}

int
pulse_open_audio(int inchans, int outchans, int rate)
{
    pulse_sink = make_client(PA_STREAM_PLAYBACK, outchans, rate);
    pulse_source = make_client(PA_STREAM_RECORD, inchans, rate);

    return !(pulse_sink || pulse_source);
}

void pulse_close_audio(void)
{
    pulse_sink = free_client(pulse_sink);
    pulse_source = free_client(pulse_source);
}

int pulse_send_dacs(void)
{

    if (!STUFF->st_inchannels && !STUFF->st_outchannels) return (SENDDACS_NO);

    if (STUFF->st_inchannels && pulse_source) {
        int err = 0;
        t_sample * fp = STUFF->st_soundin;
        if(pa_simple_read(pulse_source->client, pulse_source->buffer, pulse_source->bufsize, &err) < 0)
            error("Pulse Audio: %s", pa_strerror(err));
        else {
            const int channels = STUFF->st_inchannels;
            int chan, frame;
            float*buf = pulse_source->buffer;
            for(chan = 0; chan<channels; chan++)
                for(frame = 0; frame<DEFDACBLKSIZE; frame++)
                    *fp++ = (t_sample)buf[chan + frame * channels];
        }
    }

    if (STUFF->st_outchannels && pulse_sink) {
        int err = 0;
        t_sample * fp = STUFF->st_soundout;
        if (fp) {
            int chan, frame;
            float*buf = pulse_sink->buffer;
            const int channels = STUFF->st_outchannels;
            for(chan = 0; chan<channels; chan++)
                for(frame = 0; frame<DEFDACBLKSIZE; frame++)
                    buf[chan + frame * channels] = (float)*fp++;
        }
        if(pa_simple_write(pulse_sink->client, pulse_sink->buffer, pulse_sink->bufsize, &err) < 0)
            error("Pulse Audio: %s", pa_strerror(err));
    }

    memset(STUFF->st_soundout, 0, DEFDACBLKSIZE*sizeof(t_sample)*STUFF->st_outchannels);
    return SENDDACS_YES;
}

void pulse_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti,
        int maxndev, int devdescsize)
{
    int i, ndev;
    *canmulti = 0;  /* supports multiple devices */
    ndev = 1;
    for (i = 0; i < ndev; i++)
    {
        sprintf(indevlist + i * devdescsize, "PulseAudio");
        sprintf(outdevlist + i * devdescsize, "PulseAudio");
    }
    *nindevs = *noutdevs = ndev;
}

void pulse_listdevs(void)
{
    post("device listing not implemented for Pulse Audio yet\n");
}
