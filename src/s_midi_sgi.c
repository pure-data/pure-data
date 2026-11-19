/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* Updates in Pd 0.56.2 for SGI midiin and midiout object support */

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
#include <sys/fpu.h>
#include <dmedia/midi.h>
int mdInit(void);               /* prototype was messed up in midi.h */
/* #include "sys/select.h" */
    /*
      set the special "flush zero" bit (FS, bit 24) in the
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
#define SGI_MIDI_SYSEX_SIZE 65536

typedef struct _sgi_midi_byte_state
{
    unsigned char running_status;
    unsigned char message_status;
    unsigned char data[2];
    int data_count;
    int data_needed;
    int in_sysex;
    int sysex_overflow;
    int sysex_length;
    unsigned char sysex[SGI_MIDI_SYSEX_SIZE];
} t_sgi_midi_byte_state;

static MDport sgi_inport[NPORT];
static MDport sgi_outport[NPORT];
static t_sgi_midi_byte_state sgi_midi_byte_state[NPORT];

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
                pd_error(0, "MIDI input port %d: open failed", i+1);
        }
    }
    if (midiout)
    {
        for (i = 0; i < sgi_nports; i++)
        {
            if (!(sgi_outport[i] = mdOpenOutPort(mdGetName(i))))
                pd_error(0, "MIDI output port %d: open failed", i+1);
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
        pd_error(0, "MIDI output error\n");
}

static int sgi_midi_channel_data_length(int status)
{
    switch (status & 0xf0)
    {
    case 0x80:      /* note off */
    case 0x90:      /* note on */
    case 0xa0:      /* polyphonic key pressure */
    case 0xb0:      /* control change */
    case 0xe0:      /* pitch bend */
        return 2;
    case 0xc0:      /* program change */
    case 0xd0:      /* channel pressure */
        return 1;
    default:
        return 0;
    }
}

static int sgi_midi_system_data_length(int status)
{
    switch (status)
    {
    case 0xf1:      /* MIDI time-code quarter frame */
    case 0xf3:      /* song select */
        return 1;
    case 0xf2:      /* song position pointer */
        return 2;
    case 0xf6:      /* tune request */
        return 0;
    default:
        return -1;
    }
}

static void sgi_midi_send_short(int portno, int status,
    int data1, int data2)
{
    MDevent mdv;
    int result;

    if (portno < 0 || portno >= NPORT || !sgi_outport[portno])
        return;

    memset(&mdv, 0, sizeof(mdv));
    mdv.msg[0] = status;
    mdv.msg[1] = data1;
    mdv.msg[2] = data2;
    mdv.msg[3] = 0;
    mdv.sysexmsg = 0;
    mdv.stamp = 0;
    mdv.msglen = 0;

    result = mdSend(sgi_outport[portno], &mdv, 1);
    if (result != 1)
        pd_error(0, "MIDI output error on SGI port %d", portno + 1);
}

static void sgi_midi_send_sysex(int portno,
    t_sgi_midi_byte_state *state)
{
    MDevent mdv;
    int result;

    if (portno < 0 || portno >= NPORT || !sgi_outport[portno])
        return;
    if (state->sysex_length <= 0)
        return;

    memset(&mdv, 0, sizeof(mdv));
    mdv.msg[0] = MD_SYSEX;
    mdv.sysexmsg = (char *)state->sysex;
    mdv.msglen = state->sysex_length;
    mdv.stamp = 0;

    result = mdSend(sgi_outport[portno], &mdv, 1);
    if (result != 1)
        pd_error(0, "MIDI SysEx output error on SGI port %d",
            portno + 1);
}

static void sgi_midi_begin_message(t_sgi_midi_byte_state *state,
    int status, int data_needed)
{
    state->message_status = (unsigned char)status;
    state->data_count = 0;
    state->data_needed = data_needed;
}

static void sgi_midi_finish_message(int portno,
    t_sgi_midi_byte_state *state)
{
    int data1;
    int data2;

    data1 = state->data_count > 0 ? state->data[0] : 0;
    data2 = state->data_count > 1 ? state->data[1] : 0;
    sgi_midi_send_short(portno, state->message_status, data1, data2);

    if (state->message_status < 0xf0 && state->running_status)
    {
        state->message_status = state->running_status;
        state->data_count = 0;
        state->data_needed =
            sgi_midi_channel_data_length(state->running_status);
    }
    else
    {
        state->message_status = 0;
        state->data_count = 0;
        state->data_needed = 0;
    }
}

void sys_putmidibyte(int portno, int value)
{
    t_sgi_midi_byte_state *state;
    int status;
    int data_needed;

    if (portno < 0 || portno >= NPORT || !sgi_outport[portno])
        return;
    if (value < 0 || value > 255)
    {
        pd_error(0, "MIDI byte out of range on SGI port %d: %d",
            portno + 1, value);
        return;
    }

    state = &sgi_midi_byte_state[portno];
    status = value & 0xff;

    /* Real-time bytes can occur anywhere and do not disturb parser state. */
    if (status >= 0xf8)
    {
        sgi_midi_send_short(portno, status, 0, 0);
        return;
    }

    /* Accumulate SysEx from F0 through F7. */
    if (state->in_sysex)
    {
        if (!state->sysex_overflow)
        {
            if (state->sysex_length < SGI_MIDI_SYSEX_SIZE)
                state->sysex[state->sysex_length++] =
                    (unsigned char)status;
            else
            {
                state->sysex_overflow = 1;
                pd_error(0, "MIDI SysEx message too large on SGI port %d",
                    portno + 1);
            }
        }
        if (status == 0xf7)
        {
            if (!state->sysex_overflow)
                sgi_midi_send_sysex(portno, state);
            state->in_sysex = 0;
            state->sysex_overflow = 0;
            state->sysex_length = 0;
        }
        return;
    }

    /* Status byte. */
    if (status & 0x80)
    {
        if (status == 0xf0)
        {
            state->running_status = 0;
            state->message_status = 0;
            state->data_count = 0;
            state->data_needed = 0;
            state->in_sysex = 1;
            state->sysex_overflow = 0;
            state->sysex_length = 1;
            state->sysex[0] = 0xf0;
            return;
        }

        /* F7 outside SysEx is ignored. F4 and F5 are undefined. */
        if (status == 0xf7 || status == 0xf4 || status == 0xf5)
        {
            state->running_status = 0;
            state->message_status = 0;
            state->data_count = 0;
            state->data_needed = 0;
            return;
        }

        if (status < 0xf0)
        {
            data_needed = sgi_midi_channel_data_length(status);
            if (data_needed <= 0)
                return;
            state->running_status = (unsigned char)status;
            sgi_midi_begin_message(state, status, data_needed);
            return;
        }

        /* System-common status cancels running status. */
        state->running_status = 0;
        data_needed = sgi_midi_system_data_length(status);
        if (data_needed < 0)
            return;
        sgi_midi_begin_message(state, status, data_needed);
        if (data_needed == 0)
            sgi_midi_finish_message(portno, state);
        return;
    }

    /* Data byte. If no message is active, try running status. */
    if (state->data_needed <= 0)
    {
        if (!state->running_status)
            return;
        data_needed =
            sgi_midi_channel_data_length(state->running_status);
        if (data_needed <= 0)
            return;
        sgi_midi_begin_message(state, state->running_status, data_needed);
    }

    if (state->data_count < 2)
        state->data[state->data_count++] = (unsigned char)status;
    if (state->data_count >= state->data_needed)
        sgi_midi_finish_message(portno, state);
}

void sys_poll_midi(void)
{
    int i;
    MDport *mp;
    for (i = 0, mp = sgi_inport; i < NPORT; i++, mp++)
    {
        MDevent mdv;
        fd_set inports;
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        if (!*mp) continue;
        FD_ZERO(&inports);
        FD_SET(mdGetFd(*mp), &inports);
        if (select(mdGetFd(*mp)+1, &inports, 0, 0, &timeout) < 0)
            perror("midi select");
        if (FD_ISSET(mdGetFd(*mp), &inports))
        {
            if (mdReceive(*mp, &mdv, 1) < 0)
                pd_error(0, "failure receiving message\n");
            else if (mdv.msg[0] == MD_SYSEX) mdFree(mdv.sysexmsg);
            else
            {
                int status = mdv.msg[0];
                int b1 = mdv.msg[1];
                int b2 = mdv.msg[2];
                sys_midibytein(i, status);
                switch (status & 0xf0)
                {
                case MD_NOTEOFF:
                case MD_NOTEON:
                case MD_POLYKEYPRESSURE:
                case MD_CONTROLCHANGE:
                case MD_PITCHBENDCHANGE:
                    sys_midibytein(i, b1);
                    sys_midibytein(i, b2);
                    break;
                case MD_PROGRAMCHANGE:
                case MD_CHANNELPRESSURE:
                    sys_midibytein(i, b1);
                    break;
                }
            }
        }
    }
}

void sys_reinit_midi(void) {}

void sys_do_open_midi(int nmidiin, int *midiinvec,
    int nmidiout, int *midioutvec)
{
    sgi_open_midi(nmidiin != 0, nmidiout != 0);
}

void sys_close_midi(void)
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
