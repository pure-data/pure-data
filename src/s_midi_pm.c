/* Copyright (c) 1997-2003 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.

   this file calls portmidi to do MIDI I/O for MSW and Mac OSX.

*/

#include "m_pd.h"
#include "s_stuff.h"
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "portmidi.h"
#include "porttime.h"

#define MIDINOTEOFF       0x80  /* 2 following 'data bytes' */
#define MIDINOTEON        0x90  /* 2 */
#define MIDIPOLYTOUCH     0xa0  /* 2 */
#define MIDICONTROLCHANGE 0xb0  /* 2 */
#define MIDIPROGRAMCHANGE 0xc0  /* 1 */
#define MIDICHANNELTOUCH  0xd0  /* 1 */
#define MIDIPITCHBEND     0xe0  /* 2 */
#define MIDISTARTSYSEX    0xf0  /* (until F7) */
#define MIDITIMECODE      0xf1  /* 1 */
#define MIDISONGPOS       0xf2  /* 2 */
#define MIDISONGSELECT    0xf3  /* 1 */
#define MIDIRESERVED1     0xf4  /* ? */
#define MIDIRESERVED2     0xf5  /* ? */
#define MIDITUNEREQUEST   0xf6  /* 0 */
#define MIDIENDSYSEX      0xf7  /* 0 */
#define MIDICLOCK         0xf8  /* 0 */
#define MIDITICK          0xf9  /* 0 */
#define MIDISTART         0xfa  /* 0 */
#define MIDICONT          0xfb  /* 0 */
#define MIDISTOP          0xfc  /* 0 */
#define MIDIACTIVESENSE   0xfe  /* 0 */
#define MIDIRESET         0xff  /* 0 */

static PmStream *mac_midiindevlist[MAXMIDIINDEV];
static PmStream *mac_midioutdevlist[MAXMIDIOUTDEV];
static int mac_nmidiindev;
static int mac_nmidioutdev;

void sys_do_open_midi(int nmidiin, int *midiinvec,
    int nmidiout, int *midioutvec)
{
    int i = 0, j, devno;
    int n = 0;
    PmError err;

    Pt_Start(1, 0, 0); /* start a timer with millisecond accuracy */

    mac_nmidiindev = 0;
    for (i = 0; i < nmidiin; i++)
    {
        for (j = 0, devno = 0; j < Pm_CountDevices(); j++)
        {
            const PmDeviceInfo *info = Pm_GetDeviceInfo(j);
            if (info->input)
            {
                if (devno == midiinvec[i])
                {
                    err = Pm_OpenInput(&mac_midiindevlist[mac_nmidiindev],
                        j, NULL, 100, NULL, NULL);
                    if (err)
                        post("could not open midi input %d (%s): %s",
                            j, info->name, Pm_GetErrorText(err));

                    else
                    {
                        if (sys_verbose)
                            post("Midi Input (%s) opened.",
                                info->name);
                        mac_nmidiindev++;
                    }
                }
                devno++;
            }
        }
    }

    mac_nmidioutdev = 0;
    for (i = 0; i < nmidiout; i++)
    {
        for (j = 0, devno = 0; j < Pm_CountDevices(); j++)
        {
            const PmDeviceInfo *info = Pm_GetDeviceInfo(j);
            if (info->output)
            {
                if (devno == midioutvec[i])
                {
                    err = Pm_OpenOutput(
                        &mac_midioutdevlist[mac_nmidioutdev],
                            j, NULL, 0, NULL, NULL, 0);
                    if (err)
                        post("could not open midi output %d (%s): %s",
                            j, info->name, Pm_GetErrorText(err));
                    else
                    {
                        if (sys_verbose)
                            post("Midi Output (%s) opened.",
                                info->name);
                        mac_nmidioutdev++;
                    }
                }
                devno++;
            }
        }
    }
}

void sys_close_midi( void)
{
    int i;
    for (i = 0; i < mac_nmidiindev; i++)
        Pm_Close(mac_midiindevlist[i]);
    mac_nmidiindev = 0;
    for (i = 0; i < mac_nmidioutdev; i++)
        Pm_Close(mac_midioutdevlist[i]);
    mac_nmidioutdev = 0;
}

void sys_putmidimess(int portno, int a, int b, int c)
{
    PmEvent buffer;
    /* fprintf(stderr, "put 1 msg %d %d\n", portno, mac_nmidioutdev); */
    if (portno >= 0 && portno < mac_nmidioutdev)
    {
        buffer.message = Pm_Message(a, b, c);
        buffer.timestamp = 0;
        /* fprintf(stderr, "put msg\n"); */
        Pm_Write(mac_midioutdevlist[portno], &buffer, 1);
    }
}

static void writemidi4(PortMidiStream* stream, int a, int b, int c, int d)
{
    PmEvent buffer;
    buffer.timestamp = 0;
    buffer.message = ((a & 0xff) | ((b & 0xff) << 8)
        | ((c & 0xff) << 16) | ((d & 0xff) << 24));
    Pm_Write(stream, &buffer, 1);
}


void sys_putmidibyte(int portno, int byte)
{
        /* try to parse the bytes into MIDI messages so they can
        fit into PortMidi buffers. */
    static int mess[4];
    static int nbytes = 0, sysex = 0, i;
    if (byte >= 0xf8)   /* MIDI real time */
        writemidi4(mac_midioutdevlist[portno], byte, 0, 0, 0);
    else if (byte == 0xf0)
    {
        mess[0] = 0xf0;
        nbytes = 1;
        sysex = 1;
    }
    else if (byte == 0xf7)
    {
        mess[nbytes] = byte;
        for (i = nbytes+1; i < 4; i++)
            mess[i] = 0;
        writemidi4(mac_midioutdevlist[portno],
            mess[0], mess[1], mess[2], mess[3]);
        sysex = 0;
        nbytes = 0;
    }
    else if (byte >= 0x80)
    {
        sysex = 0;
        if (byte == 0xf4 || byte == 0xf5 || byte == 0xf6)
        {
            writemidi4(mac_midioutdevlist[portno], byte, 0, 0, 0);
            nbytes = 0;
        }
        else
        {
            mess[0] = byte;
            nbytes = 1;
        }
    }
    else if (sysex)
    {
        mess[nbytes] = byte;
        nbytes++;
        if (nbytes == 4)
        {
            writemidi4(mac_midioutdevlist[portno],
                mess[0], mess[1], mess[2], mess[3]);
            nbytes = 0;
        }
    }
    else if (nbytes)
    {
        int status = mess[0];
        if (status < 0xf0)
            status &= 0xf0;
                /* 2 byte messages: */
        if (status == 0xc0 || status == 0xd0 ||
            status == 0xf1 || status == 0xf3)
        {
            writemidi4(mac_midioutdevlist[portno],
                mess[0], byte, 0, 0);
            nbytes = (status < 0xf0 ? 1 : 0);
        }
        else
        {
            if (nbytes == 1)
            {
                mess[1] = byte;
                nbytes = 2;
            }
            else
            {
                writemidi4(mac_midioutdevlist[portno],
                    mess[0], mess[1], byte, 0);
                nbytes = (status < 0xf0 ? 1 : 0);
            }
        }
    }
}

/* this is non-zero if we are in the middle of transmitting sysex */

int nd_sysex_mode=0;

/* send in 4 bytes of sysex data. if one of the bytes is 0xF7 (sysex end)
    stop and unset nd_sysex_mode */
void nd_sysex_inword(int midiindev, int status, int data1, int data2, int data3)
{
    if (nd_sysex_mode) {
        sys_midibytein(midiindev, status);
        if (status == 0xF7)
            nd_sysex_mode = 0;
    }

    if (nd_sysex_mode) {
        sys_midibytein(midiindev, data1);
        if (data1 == 0xF7)
            nd_sysex_mode = 0;
    }

    if (nd_sysex_mode) {
        sys_midibytein(midiindev, data2);
        if (data2 == 0xF7)
            nd_sysex_mode = 0;
    }

    if (nd_sysex_mode) {
        sys_midibytein(midiindev, data3);
        if (data3 == 0xF7)
            nd_sysex_mode = 0;
    }
}

void sys_poll_midi(void)
{
    int i, nmess, throttle = 100;
    PmEvent buffer;
    for (i = 0; i < mac_nmidiindev; i++)
    {
        while (1)
        {
            if (!throttle--)
                goto overload;
            nmess = Pm_Read(mac_midiindevlist[i], &buffer, 1);
            if (nmess > 0)
            {
                int status = Pm_MessageStatus(buffer.message);
                int data1  = Pm_MessageData1(buffer.message);
                int data2  = Pm_MessageData2(buffer.message);
                int data3 = ((buffer.message >> 24) & 0xff);
                int msgtype = ((status & 0xf0) == 0xf0 ?
                    status : (status & 0xf0));
                if (nd_sysex_mode)
                    nd_sysex_inword(i, status, data1, data2, data3);
                else switch (msgtype)
                {
                case MIDINOTEOFF:
                case MIDINOTEON:
                case MIDIPOLYTOUCH:
                case MIDICONTROLCHANGE:
                case MIDIPITCHBEND:
                case MIDISONGPOS:
                    sys_midibytein(i, status);
                    sys_midibytein(i, data1);
                    sys_midibytein(i, data2);
                    break;
                case MIDIPROGRAMCHANGE:
                case MIDICHANNELTOUCH:
                case MIDITIMECODE:
                case MIDISONGSELECT:
                    sys_midibytein(i, status);
                    sys_midibytein(i, data1);
                    break;
                case MIDISTARTSYSEX:
                    nd_sysex_mode=1;
                    nd_sysex_inword(i, status, data1, data2, data3);
                    break;
                default:
                    sys_midibytein(i, status);
                    break;
                }
            }
            else break;
        }
    }
    overload: ;
}

void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize)
{
    int i, nindev = 0, noutdev = 0;
    for (i = 0; i < Pm_CountDevices(); i++)
    {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        /* post("%d: %s, %s (%d,%d)", i, info->interf, info->name,
            info->input, info->output); */
        if (info->input && nindev < maxndev)
        {
            strcpy(indevlist + nindev * devdescsize, info->name);
            nindev++;
        }
        if (info->output && noutdev < maxndev)
        {
            strcpy(outdevlist + noutdev * devdescsize, info->name);
            noutdev++;
        }
    }
    *nindevs = nindev;
    *noutdevs = noutdev;
}
