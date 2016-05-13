/* Copyright (c) 1997-1999 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* MIDI I/O for Linux using ALSA */

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include "m_pd.h"
#include "s_stuff.h"

//the maximum length of input messages
#ifndef ALSA_MAX_EVENT_SIZE
#define ALSA_MAX_EVENT_SIZE 512
#endif

static int alsa_nmidiin;
static int alsa_midiinfd[MAXMIDIINDEV];
static int alsa_nmidiout;
static int alsa_midioutfd[MAXMIDIOUTDEV];

static snd_seq_t *midi_handle;

static snd_midi_event_t *midiev;

void sys_alsa_do_open_midi(int nmidiin, int *midiinvec,
    int nmidiout, int *midioutvec)
{

    char portname[50];
    int err = 0;
    int client;
    int i;
    snd_seq_client_info_t *alsainfo;

    alsa_nmidiin = 0;
    alsa_nmidiout = 0;

    if (nmidiout == 0 && nmidiin == 0) return;

    if(nmidiin>MAXMIDIINDEV )
      {
        post("midi input ports reduced to maximum %d", MAXMIDIINDEV);
        nmidiin=MAXMIDIINDEV;
      }
    if(nmidiout>MAXMIDIOUTDEV)
      {
        post("midi output ports reduced to maximum %d", MAXMIDIOUTDEV);
        nmidiout=MAXMIDIOUTDEV;
      }

    if (nmidiin>0 && nmidiout>0)
        err = snd_seq_open(&midi_handle,"default",SND_SEQ_OPEN_DUPLEX,0);
    else if (nmidiin > 0)
        err = snd_seq_open(&midi_handle,"default",SND_SEQ_OPEN_INPUT,0);
    else if (nmidiout > 0)
        err = snd_seq_open(&midi_handle,"default",SND_SEQ_OPEN_OUTPUT,0);

    if (err!=0)
    {
            sys_setalarm(1000000);
            post("couldn't open alsa sequencer");
            return;
    }
    for (i=0;i<nmidiin;i++)
    {
        int port;
        sprintf(portname,"Pure Data Midi-In %d",i+1);
        port = snd_seq_create_simple_port(midi_handle,portname,
                                          SND_SEQ_PORT_CAP_WRITE |SND_SEQ_PORT_CAP_SUBS_WRITE,
                                          SND_SEQ_PORT_TYPE_APPLICATION);
        alsa_midiinfd[i] = port;
        if (port < 0) goto error;
    }

    for (i=0;i<nmidiout;i++)
    {
        int port;
        sprintf(portname,"Pure Data Midi-Out %d",i+1);
        port = snd_seq_create_simple_port(midi_handle,portname,
                                          SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                                          SND_SEQ_PORT_TYPE_APPLICATION);
        alsa_midioutfd[i] = port;
        if (port < 0) goto error;
    }

    snd_seq_client_info_malloc(&alsainfo);
    snd_seq_get_client_info(midi_handle,alsainfo);
    snd_seq_client_info_set_name(alsainfo,"Pure Data");
    client = snd_seq_client_info_get_client(alsainfo);
    snd_seq_set_client_info(midi_handle,alsainfo);
    snd_seq_client_info_free(alsainfo);
    post("Opened Alsa Client %d in:%d out:%d",client,nmidiin,nmidiout);
    sys_setalarm(0);
    snd_midi_event_new(ALSA_MAX_EVENT_SIZE,&midiev);
    alsa_nmidiout = nmidiout;
    alsa_nmidiin = nmidiin;

    return;
 error:
    sys_setalarm(1000000);
    post("couldn't open alsa MIDI output device");
    return;
}

#define md_msglen(x) (((x)<0xC0)?2:((x)<0xE0)?1:((x)<0xF0)?2:\
    ((x)==0xF2)?2:((x)<0xF4)?1:0)

void sys_alsa_putmidimess(int portno, int a, int b, int c)
{
    int channel;
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    if (portno >= 0 && portno < alsa_nmidiout)
    {
        if (a >= 224)   // pitchbend
        {
            channel = a-224;
            snd_seq_ev_set_pitchbend(&ev, channel, (((c<<7)|b)-8192)); /* b and c are already correct but alsa needs to recalculate them */
        }
        else if (a >= 208)      // touch
        {
            channel = a-208;
            snd_seq_ev_set_chanpress(&ev,channel,b);
        }
        else if (a >= 192)      // program
        {
            channel = a-192;
            snd_seq_ev_set_pgmchange(&ev,channel,b);
        }
        else if (a >= 176)      // controller
        {
            channel = a-176;
            snd_seq_ev_set_controller(&ev,channel,b,c);
        }
        else if (a >= 160)      // polytouch
        {
            channel = a-160;
            snd_seq_ev_set_keypress(&ev,channel,b,c);
        }
        else if (a >= 144)      // note on
        {
            channel = a-144;
            snd_seq_ev_set_noteon(&ev,channel,b,c);
        }
        else if (a >= 128)      // note off
        {
            channel = a-128;
            snd_seq_ev_set_noteoff(&ev,channel,b,c);
        }
        snd_seq_ev_set_direct(&ev);
        snd_seq_ev_set_subs(&ev);
        snd_seq_ev_set_source(&ev,alsa_midioutfd[portno]);
        snd_seq_event_output_direct(midi_handle,&ev);
    }
    //post("%d %d %d\n",a,b,c);
}

void sys_alsa_putmidibyte(int portno, int byte)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    if (portno >= 0 && portno < alsa_nmidiout)
    {
        // repack into 1 byte char and put somewhere to point at
        unsigned char data = (unsigned char)byte;

        snd_seq_ev_set_sysex(&ev,1,&data); //...set_variable *should* have worked but didn't
        snd_seq_ev_set_direct(&ev);
        snd_seq_ev_set_subs(&ev);
        snd_seq_ev_set_source(&ev,alsa_midioutfd[portno]);
        snd_seq_event_output_direct(midi_handle,&ev);
    }
}


    /* this version uses the asynchronous "read()" ... */
void sys_alsa_poll_midi(void)
{
   unsigned char buf[ALSA_MAX_EVENT_SIZE];
   int count, alsa_source;
   int i;
   snd_seq_event_t *midievent = NULL;

   if (alsa_nmidiout == 0 && alsa_nmidiin == 0) return;

   snd_midi_event_init(midiev);

   if (!alsa_nmidiout && !alsa_nmidiin) return;
   count = snd_seq_event_input_pending(midi_handle,1);
   if (count != 0)
        count = snd_seq_event_input(midi_handle,&midievent);
   if (midievent != NULL)
   {
       count = snd_midi_event_decode(midiev,buf,sizeof(buf),midievent);
       alsa_source = midievent->dest.port;
       for(i=0;i<count;i++)
           sys_midibytein(alsa_source, (buf[i] & 0xff));
       //post("received %d midi bytes\n",count);
   }
}

void sys_alsa_close_midi()
{
    alsa_nmidiin = alsa_nmidiout = 0;
    if(midi_handle)
      {
        snd_seq_close(midi_handle);
        if(midiev)
          {
            snd_midi_event_free(midiev);
          }
      }
}

static int alsa_nmidiindevs = 1, alsa_nmidioutdevs = 1;

void midi_alsa_init(void)
{
}

void midi_alsa_setndevs(int in, int out)
{
    alsa_nmidiindevs = in;
    alsa_nmidioutdevs = out;
}

void midi_alsa_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize)
{
    int i, ndev;
    if ((ndev = alsa_nmidiindevs) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++)
        sprintf(indevlist + i * devdescsize, "ALSA MIDI device #%d", i+1);
    *nindevs = ndev;

    if ((ndev = alsa_nmidioutdevs) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++)
        sprintf(outdevlist + i * devdescsize, "ALSA MIDI device #%d", i+1);
    *noutdevs = ndev;
}
