/* Copyright (c) 1997-1999 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* MIDI I/O for Linux using OSS */

#include <stdio.h>
#ifdef UNISTD
#include <unistd.h>
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "m_pd.h"
#include "s_stuff.h"

static int oss_nmidiin;
static int oss_midiinfd[MAXMIDIINDEV];
static int oss_nmidiout;
static int oss_midioutfd[MAXMIDIOUTDEV];

static void oss_midiout(int fd, int n)
{
    char b = n;
    if ((write(fd, (char *) &b, 1)) != 1)
        perror("midi write");
}

#define O_MIDIFLAG O_NDELAY

void sys_do_open_midi(int nmidiin, int *midiinvec,
    int nmidiout, int *midioutvec)
{
    int i;
    for (i = 0; i < nmidiout; i++)
        oss_midioutfd[i] = -1;
    for (i = 0, oss_nmidiin = 0; i < nmidiin; i++)
    {
        int fd = -1, j, outdevindex = -1;
        char namebuf[80];
        int devno = midiinvec[i];

        for (j = 0; j < nmidiout; j++)
            if (midioutvec[j] == midiinvec[i])
                outdevindex = j;
        
            /* try to open the device for read/write. */
        if (devno == 0 && fd < 0 && outdevindex >= 0)
        {
            sys_setalarm(1000000);
            fd = open("/dev/midi", O_RDWR | O_MIDIFLAG);
            if (sys_verbose)
                fprintf(stderr,
                    "device 1: tried /dev/midi READ/WRITE; returned %d\n", fd);
            if (outdevindex >= 0 && fd >= 0)
                oss_midioutfd[outdevindex] = fd;
        }
        if (fd < 0 && outdevindex >= 0)
        {
            sys_setalarm(1000000);
            sprintf(namebuf, "/dev/midi%2.2d", devno);
            fd = open(namebuf, O_RDWR | O_MIDIFLAG);
            if (sys_verbose)
                fprintf(stderr,
                    "device %d: tried %s READ/WRITE; returned %d\n",
                        devno, namebuf, fd);
            if (outdevindex >= 0 && fd >= 0)
                oss_midioutfd[outdevindex] = fd;
        }
        if (fd < 0 && outdevindex >= 0)
        {
            sys_setalarm(1000000);
            sprintf(namebuf, "/dev/midi%d", devno);
            fd = open(namebuf, O_RDWR | O_MIDIFLAG);
            if (sys_verbose)
                fprintf(stderr, "device %d: tried %s READ/WRITE; returned %d\n",
                    devno, namebuf, fd);
            if (outdevindex >= 0 && fd >= 0)
                oss_midioutfd[outdevindex] = fd;
        }
        if (devno == 1 && fd < 0)
        {
            sys_setalarm(1000000);
            fd = open("/dev/midi", O_RDONLY | O_MIDIFLAG);
            if (sys_verbose)
                fprintf(stderr,
                    "device 1: tried /dev/midi READONLY; returned %d\n", fd);
        }
        if (fd < 0)
        {
            sys_setalarm(1000000);
            sprintf(namebuf, "/dev/midi%2.2d", devno);
            fd = open(namebuf, O_RDONLY | O_MIDIFLAG);
            if (sys_verbose)
                fprintf(stderr, "device %d: tried %s READONLY; returned %d\n",
                    devno, namebuf, fd);
        }
        if (fd < 0)
        {
            sys_setalarm(1000000);
            sprintf(namebuf, "/dev/midi%d", devno);
            fd = open(namebuf, O_RDONLY | O_MIDIFLAG);
            if (sys_verbose)
                fprintf(stderr, "device %d: tried %s READONLY; returned %d\n",
                    devno, namebuf, fd);
        }
        if (fd >= 0)
            oss_midiinfd[oss_nmidiin++] = fd;       
        else post("couldn't open MIDI input device %d", devno);
    }
    for (i = 0, oss_nmidiout = 0; i < nmidiout; i++)
    {
        int fd = oss_midioutfd[i];
        char namebuf[80];
        int devno = midioutvec[i];
        if (devno == 1 && fd < 0)
        {
            sys_setalarm(1000000);
            fd = open("/dev/midi", O_WRONLY | O_MIDIFLAG);
            if (sys_verbose)
                fprintf(stderr,
                    "device 1: tried /dev/midi WRITEONLY; returned %d\n", fd);
        }
        if (fd < 0)
        {
            sys_setalarm(1000000);
            sprintf(namebuf, "/dev/midi%2.2d", devno);
            fd = open(namebuf, O_WRONLY | O_MIDIFLAG);
            if (sys_verbose)
                fprintf(stderr, "device %d: tried %s WRITEONLY; returned %d\n",
                    devno, namebuf, fd);
        }
        if (fd < 0)
        {
            sys_setalarm(1000000);
            sprintf(namebuf, "/dev/midi%d", devno);
            fd = open(namebuf, O_WRONLY | O_MIDIFLAG);
            if (sys_verbose)
                fprintf(stderr, "device %d: tried %s WRITEONLY; returned %d\n",
                    devno, namebuf, fd);
        }
        if (fd >= 0)
            oss_midioutfd[oss_nmidiout++] = fd;     
        else post("couldn't open MIDI output device %d", devno);
    }

    if (oss_nmidiin < nmidiin || oss_nmidiout < nmidiout || sys_verbose)
        post("opened %d MIDI input device(s) and %d MIDI output device(s).",
            oss_nmidiin, oss_nmidiout);

    sys_setalarm(0);
}

#define md_msglen(x) (((x)<0xC0)?2:((x)<0xE0)?1:((x)<0xF0)?2:\
    ((x)==0xF2)?2:((x)<0xF4)?1:0)

void sys_putmidimess(int portno, int a, int b, int c)
{
    if (portno >= 0 && portno < oss_nmidiout)
    {
       switch (md_msglen(a))
       {
       case 2:
            oss_midiout(oss_midioutfd[portno],a);        
            oss_midiout(oss_midioutfd[portno],b);        
            oss_midiout(oss_midioutfd[portno],c);
            return;
       case 1:
            oss_midiout(oss_midioutfd[portno],a);        
            oss_midiout(oss_midioutfd[portno],b);        
            return;
       case 0:
            oss_midiout(oss_midioutfd[portno],a);        
            return;
       };
    }
}

void sys_putmidibyte(int portno, int byte)
{
    if (portno >= 0 && portno < oss_nmidiout)
        oss_midiout(oss_midioutfd[portno], byte);       
}

#if 0   /* this is the "select" version which doesn't work with OSS
        driver for emu10k1 (it doesn't implement select.) */
void sys_poll_midi(void)
{
    int i, throttle = 100;
    struct timeval timout;
    int did = 1, maxfd = 0;
    while (did)
    {
        fd_set readset, writeset, exceptset;
        did = 0;
        if (throttle-- < 0)
            break;
        timout.tv_sec = 0;
        timout.tv_usec = 0;

        FD_ZERO(&writeset);
        FD_ZERO(&readset);
        FD_ZERO(&exceptset);
        for (i = 0; i < oss_nmidiin; i++)
        {
            if (oss_midiinfd[i] > maxfd)
                maxfd = oss_midiinfd[i];
            FD_SET(oss_midiinfd[i], &readset);
        }
        select(maxfd+1, &readset, &writeset, &exceptset, &timout);
        for (i = 0; i < oss_nmidiin; i++)
            if (FD_ISSET(oss_midiinfd[i], &readset))
        {
            char c;
            int ret = read(oss_midiinfd[i], &c, 1);
            if (ret <= 0)
                fprintf(stderr, "Midi read error\n");
            else sys_midibytein(i, (c & 0xff));
            did = 1;
        }
    }
}
#else 

    /* this version uses the asynchronous "read()" ... */
void sys_poll_midi(void)
{
    int i, throttle = 100;
    struct timeval timout;
    int did = 1, maxfd = 0;
    while (did)
    {
        fd_set readset, writeset, exceptset;
        did = 0;
        if (throttle-- < 0)
            break;
        for (i = 0; i < oss_nmidiin; i++)
        {
            char c;
            int ret = read(oss_midiinfd[i], &c, 1);
            if (ret < 0)
            {
                if (errno != EAGAIN)
                    perror("MIDI");
            }
            else if (ret != 0)
            {
                sys_midibytein(i, (c & 0xff));
                did = 1;
            }
        }
    }
}
#endif

void sys_close_midi()
{
    int i;
    for (i = 0; i < oss_nmidiin; i++)
        close(oss_midiinfd[i]);
    for (i = 0; i < oss_nmidiout; i++)
        close(oss_midioutfd[i]);
    oss_nmidiin = oss_nmidiout = 0;
}

#define NSEARCH 10
static int oss_nmidiindevs, oss_nmidioutdevs, oss_initted;

void midi_oss_init(void)     
{
    int i;
    if (oss_initted)
        return;
    oss_initted = 1;
    for (i = 0; i < NSEARCH; i++)
    {
        int fd;
        char namebuf[80];
        
        oss_nmidiindevs = i;
            /* try to open the device for reading */
        if (i == 0)
        {
            fd = open("/dev/midi", O_RDONLY | O_NDELAY);
            if (fd >= 0)
            {
                close(fd);
                continue;
            }
        }
        sprintf(namebuf, "/dev/midi%2.2d", i);
        fd = open(namebuf, O_RDONLY | O_NDELAY);
        if (fd >= 0)
        {
            close(fd);
            continue;
        }
        sprintf(namebuf, "/dev/midi%d", i);
        fd = open(namebuf, O_RDONLY | O_NDELAY);
        if (fd >= 0)
        {
            close(fd);
            continue;
        }
        break;
    }
    for (i = 0; i < NSEARCH; i++)
    {
        int fd;
        char namebuf[80];
        
        oss_nmidioutdevs = i;
            /* try to open the device for writing */
        if (i == 0)
        {
            fd = open("/dev/midi", O_WRONLY | O_NDELAY);
            if (fd >= 0)
            {
                close(fd);
                continue;
            }
        }
        sprintf(namebuf, "/dev/midi%2.2d", i);
        fd = open(namebuf, O_WRONLY | O_NDELAY);
        if (fd >= 0)
        {
            close(fd);
            continue;
        }
        sprintf(namebuf, "/dev/midi%d", i);
        fd = open(namebuf, O_WRONLY | O_NDELAY);
        if (fd >= 0)
        {
            close(fd);
            continue;
        }
        break;
    }
}

void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize)
{
    int i, ndev;
    if ((ndev = oss_nmidiindevs) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++)
        sprintf(indevlist + i * devdescsize, "OSS MIDI device #%d", i+1);
    *nindevs = ndev;

    if ((ndev = oss_nmidioutdevs) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++)
        sprintf(outdevlist + i * devdescsize, "OSS MIDI device #%d", i+1);
    *noutdevs = ndev;
}
