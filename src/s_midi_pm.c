/* Copyright (c) 1997-2003 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.

   this file calls portmidi to do MIDI I/O for MSW and Mac OSX.

*/

#include "m_pd.h"
#include "s_stuff.h"
#include "s_utf8.h"
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <sys/time.h>
#ifndef _WIN32
#include <sys/resource.h>
#endif /* _WIN32 */
#endif /* HAVE_UNISTD_H */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "portmidi.h"
#include "porttime.h"

/* full status byte definitions in s_midi.c */
/* channel voice messages */
#define MIDI_NOTEOFF        0x80
#define MIDI_NOTEON         0x90
#define MIDI_POLYAFTERTOUCH 0xa0
#define MIDI_CONTROLCHANGE  0xb0
#define MIDI_PROGRAMCHANGE  0xc0
#define MIDI_AFTERTOUCH     0xd0
#define MIDI_PITCHBEND      0xe0
/* system common messages */
#define MIDI_SYSEX          0xf0
#define MIDI_TIMECODE       0xf1
#define MIDI_SONGPOS        0xf2
#define MIDI_SONGSELECT     0xf3
#define MIDI_SYSEXEND       0xf7

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
                        j, NULL, 1024, NULL, NULL);
                    if (err)
                        post("could not open MIDI input %d (%s): %s",
                            j, info->name, Pm_GetErrorText(err));
                    else
                    {
                        /* disable default active sense filtering */
                        Pm_SetFilter(mac_midiindevlist[mac_nmidiindev], 0);
                        logpost(NULL, PD_VERBOSE, "MIDI input (%s) opened.",
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
                        post("could not open MIDI output %d (%s): %s",
                            j, info->name, Pm_GetErrorText(err));
                    else
                    {
                        logpost(NULL, PD_VERBOSE, "MIDI output (%s) opened.",
                            info->name);
                        mac_nmidioutdev++;
                    }
                }
                devno++;
            }
        }
    }
}

void sys_close_midi(void)
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
    if (portno < 0 || portno >= mac_nmidioutdev)
        return;
    if (byte > MIDI_SYSEXEND)
    {
        /* realtime */
        writemidi4(mac_midioutdevlist[portno], byte, 0, 0, 0);
    }
    else if (byte == MIDI_SYSEX)
    {
        /* sysex start */
        mess[0] = MIDI_SYSEX;
        nbytes = 1;
        sysex = 1;
    }
    else if (byte == MIDI_SYSEXEND)
    {
        /* sysex end */
        mess[nbytes] = byte;
        for (i = nbytes+1; i < 4; i++)
            mess[i] = 0;
        writemidi4(mac_midioutdevlist[portno],
            mess[0], mess[1], mess[2], mess[3]);
        sysex = 0;
        nbytes = 0;
    }
    else if (byte >= MIDI_NOTEOFF)
    {
        /* status byte */
        sysex = 0;
        if (byte > MIDI_SONGSELECT)
        {
            /* 0 data bytes */
            writemidi4(mac_midioutdevlist[portno], byte, 0, 0, 0);
            nbytes = 0;
        }
        else
        {
            /* 1 or 2 data bytes */
            mess[0] = byte;
            nbytes = 1;
        }
    }
    else if (sysex)
    {
        /* sysex data byte */
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
        /* channel or system message */
        int status = mess[0];
        if (status < MIDI_SYSEX)
            status &= 0xf0;
        if (status == MIDI_PROGRAMCHANGE || status == MIDI_AFTERTOUCH ||
            status == MIDI_TIMECODE || status == MIDI_SONGSELECT)
        {
            writemidi4(mac_midioutdevlist[portno],
                mess[0], byte, 0, 0);
            nbytes = (status < MIDI_SYSEX ? 1 : 0);
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
int nd_sysex_mode = 0;

/* send in 4 bytes of sysex data. if one of the bytes is sysex end
    stop and unset nd_sysex_mode */
void nd_sysex_inword(int midiindev, int status, int data1, int data2, int data3)
{
    if (nd_sysex_mode) {
        sys_midibytein(midiindev, status);
        if (status == MIDI_SYSEXEND)
            nd_sysex_mode = 0;
    }

    if (nd_sysex_mode) {
        sys_midibytein(midiindev, data1);
        if (data1 == MIDI_SYSEXEND)
            nd_sysex_mode = 0;
    }

    if (nd_sysex_mode) {
        sys_midibytein(midiindev, data2);
        if (data2 == MIDI_SYSEXEND)
            nd_sysex_mode = 0;
    }

    if (nd_sysex_mode) {
        sys_midibytein(midiindev, data3);
        if (data3 == MIDI_SYSEXEND)
            nd_sysex_mode = 0;
    }
}

void sys_poll_midi(void)
{
    int i, nmess, throttle = 100;
    PmEvent buffer;
    for (i = 0; i < mac_nmidiindev; i++)
    {
        while((nmess = Pm_Read(mac_midiindevlist[i], &buffer, 1)))
        {
            if (!throttle--)
                goto overload;
            if (nmess > 0)
            {
                int status = Pm_MessageStatus(buffer.message);
                int data1  = Pm_MessageData1(buffer.message);
                int data2  = Pm_MessageData2(buffer.message);
                int data3  = ((buffer.message >> 24) & 0xff);
                int msgtype = ((status & 0xf0) == 0xf0 ?
                                status : (status & 0xf0));

                if (status > MIDI_SYSEXEND)
                {
                    /* realtime message */
                    sys_midibytein(i, status);
                }
                else if (nd_sysex_mode)
                    nd_sysex_inword(i, status, data1, data2, data3);
                else switch (msgtype)
                {
                    /* 2 data bytes */
                    case MIDI_NOTEOFF:
                    case MIDI_NOTEON:
                    case MIDI_POLYAFTERTOUCH:
                    case MIDI_CONTROLCHANGE:
                    case MIDI_PITCHBEND:
                    case MIDI_SONGPOS:
                        sys_midibytein(i, status);
                        sys_midibytein(i, data1);
                        sys_midibytein(i, data2);
                        break;
                    /* 1 data byte */
                    case MIDI_PROGRAMCHANGE:
                    case MIDI_AFTERTOUCH:
                    case MIDI_TIMECODE:
                    case MIDI_SONGSELECT:
                        sys_midibytein(i, status);
                        sys_midibytein(i, data1);
                        break;
                    /* no data bytes */
                    case MIDI_SYSEX:
                        nd_sysex_mode = 1;
                        nd_sysex_inword(i, status, data1, data2, data3);
                        break;
                    /* all others */
                    default:
                        sys_midibytein(i, status);
                        break;
                }
            }
            else
            {
                pd_error(0, "%s", Pm_GetErrorText(nmess));
                if (nmess != pmBufferOverflow)
                    break;
            }
        }
    }
    overload: ;
}

void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize)
{
    int i, nindev = 0, noutdev = 0;
    char utf8device[MAXPDSTRING];
    utf8device[0] = 0;
    for (i = 0; i < Pm_CountDevices(); i++)
    {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        /* post("%d: %s, %s (%d,%d)", i, info->interf, info->name,
            info->input, info->output); */
        if(!u8_nativetoutf8(utf8device, MAXPDSTRING, info->name, -1))
            continue;
        if (info->input && nindev < maxndev)
        {
            strcpy(indevlist + nindev * devdescsize, utf8device);
            nindev++;
        }
        if (info->output && noutdev < maxndev)
        {
            strcpy(outdevlist + noutdev * devdescsize, utf8device);
            noutdev++;
        }
    }
    *nindevs = nindev;
    *noutdevs = noutdev;
}
