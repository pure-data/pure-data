/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include "s_stuff.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_BSTRING_H
#include <bstring.h>
#endif
#include <sys/types.h>
#include <sys/time.h>

#include <dmedia/audio.h>
#include <sys/fpu.h>
#include <dmedia/midi.h>
int mdInit(void);               /* prototype was messed up in midi.h */
/* #include "sys/select.h" */


  /*
    set the special "flush zero" but (FS, bit 24) in the
    Control Status Register of the FPU of R4k and beyond
    so that the result of any underflowing operation will
    be clamped to zero, and no exception of any kind will
    be generated on the CPU.
    
    thanks to cpirazzi@cp.esd.sgi.com (Chris Pirazzi).
  */

static void sgi_flush_all_underflows_to_zero(void)
{
    union fpc_csr f;
    f.fc_word = get_fpc_csr();
    f.fc_struct.flush = 1;
    set_fpc_csr(f.fc_word);
}

#define NPORT 2

static MDport sgi_inport[NPORT];
static MDport sgi_outport[NPORT];

void sgi_open_midi(int midiin, int midiout)
{
    int i;
    int sgi_nports = mdInit();
    if (sgi_nports < 0) sgi_nports = 0;
    else if (sgi_nports > NPORT) sgi_nports = NPORT;
    if (sys_verbose)
    {
        if (!sgi_nports) 
        {
            post("no serial ports are configured for MIDI;");
            post("if you want to use MIDI, try exiting Pd, typing");
            post("'startmidi -d /dev/ttyd2' to a shell, and restarting Pd.");
        }
        else if (sgi_nports == 1)
            post("Found one MIDI port on %s", mdGetName(0));
        else if (sgi_nports == 2)
            post("Found MIDI ports on %s and %s",
                mdGetName(0), mdGetName(1));
    }
    if (midiin)
    {
        for (i = 0; i < sgi_nports; i++)
        {
            if (!(sgi_inport[i] = mdOpenInPort(mdGetName(i)))) 
                error("MIDI input port %d: open failed", i+1);;
        }
    }
    if (midiout)
    {
        for (i = 0; i < sgi_nports; i++)
        {
            if (!(sgi_outport[i] = mdOpenOutPort(mdGetName(i))))
                error("MIDI output port %d: open failed", i+1);;
        }
    }
    return;
}

void sys_putmidimess(int portno, int a, int b, int c)
{
    MDevent mdv;
    if (portno >= NPORT || portno < 0 || !sgi_outport[portno]) return;
    mdv.msg[0] = a;
    mdv.msg[1] = b;
    mdv.msg[2] = c;
    mdv.msg[3] = 0;
    mdv.sysexmsg = 0;
    mdv.stamp = 0;
    mdv.msglen = 0;
    if (mdSend(sgi_outport[portno], &mdv, 1) < 0)
        error("MIDI output error\n");
    post("msg out %d %d %d", a, b, c);
}

void sys_putmidibyte(int portno, int foo)
{
    error("MIDI raw byte output not available on SGI");
}

void inmidi_noteon(int portno, int channel, int pitch, int velo);
void inmidi_controlchange(int portno, int channel, int ctlnumber, int value);
void inmidi_programchange(int portno, int channel, int value);
void inmidi_pitchbend(int portno, int channel, int value);
void inmidi_aftertouch(int portno, int channel, int value);
void inmidi_polyaftertouch(int portno, int channel, int pitch, int value);

void sys_poll_midi(void)
{
    int i;
    MDport *mp;
    for (i = 0, mp = sgi_inport; i < NPORT; i++, mp++)
    {
        int ret,  status,  b1,  b2, nfds;
        MDevent mdv;
        fd_set inports;
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        if (!*mp) continue;
        FD_ZERO(&inports);
        FD_SET(mdGetFd(*mp), &inports);

        if (select(mdGetFd(*mp)+1 , &inports, 0, 0, &timeout) < 0)
            perror("midi select");
        if (FD_ISSET(mdGetFd(*mp),&inports))
        {
            if (mdReceive(*mp, &mdv, 1) < 0)
                error("failure receiving message\n");
            else if (mdv.msg[0] == MD_SYSEX) mdFree(mdv.sysexmsg);

            else
            {
                int status = mdv.msg[0];
                int channel = (status & 0xf) + 1;
                int b1 = mdv.msg[1];
                int b2 = mdv.msg[2];
                switch(status & 0xf0)
                {
                case MD_NOTEOFF:
                    inmidi_noteon(i, channel, b1, 0);
                    break;
                case MD_NOTEON:
                    inmidi_noteon(i, channel, b1, b2);
                    break;
                case MD_POLYKEYPRESSURE:
                    inmidi_polyaftertouch(i, channel, b1, b2);
                    break;
                case MD_CONTROLCHANGE:
                    inmidi_controlchange(i, channel, b1, b2);
                    break;
                case MD_PITCHBENDCHANGE:
                    inmidi_pitchbend(i, channel, ((b2 << 7) + b1));
                    break;
                case MD_PROGRAMCHANGE:
                    inmidi_programchange(i, channel, b1);
                    break;
                case MD_CHANNELPRESSURE:
                    inmidi_aftertouch(i, channel, b1);
                    break;
                }
            }
        }
    }
}

void sys_do_open_midi(int nmidiin, int *midiinvec,
    int nmidiout, int *midioutvec)
{
    sgi_open_midi(nmidiin!=0, nmidiout!=0);
}


void sys_close_midi( void)
{
    /* ??? */
}


void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize)
{
    int i, nindev = 0, noutdev = 0;
    for (i = 0; i < mdInit(); i++)
    {
                if (nindev < maxndev)
                {
                    strcpy(indevlist + nindev * devdescsize, mdGetName(i));
                    nindev++;

                    strcpy(outdevlist + noutdev * devdescsize, mdGetName(i));
                    noutdev++;
                }
    }
    *nindevs = nindev;
    *noutdevs = noutdev;
}
