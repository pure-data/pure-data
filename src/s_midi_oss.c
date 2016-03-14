/* Copyright (c) 1997-1999 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* MIDI I/O for Linux using OSS */

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "m_pd.h"
#include "s_stuff.h"

#define NSEARCH 10
static int oss_nmidiindevs, oss_nmidioutdevs;
static char oss_indevnames[NSEARCH][4], oss_outdevnames[NSEARCH][4];
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
        if (devno < 0 || devno >= oss_nmidiindevs)
            continue;
        for (j = 0; j < nmidiout; j++)
            if (midioutvec[j] >= 0 && midioutvec[j] <= oss_nmidioutdevs
                && !strcmp(oss_outdevnames[midioutvec[j]],
                oss_indevnames[devno]))
                    outdevindex = j;

        sprintf(namebuf, "/dev/midi%s", oss_indevnames[devno]);

            /* try to open the device for read/write. */
        if (outdevindex >= 0)
        {
            sys_setalarm(1000000);
            fd = open(namebuf, O_RDWR | O_MIDIFLAG);
            if (sys_verbose)
                post("tried to open %s read/write; got %d\n",
                    namebuf, fd);
            if (outdevindex >= 0 && fd >= 0)
                oss_midioutfd[outdevindex] = fd;
        }
            /* OK, try read-only */
        if (fd < 0)
        {
            sys_setalarm(1000000);
            fd = open(namebuf, O_RDONLY | O_MIDIFLAG);
            if (sys_verbose)
                post("tried to open %s read-only; got %d\n",
                    namebuf, fd);
        }
        if (fd >= 0)
            oss_midiinfd[oss_nmidiin++] = fd;
        else post("couldn't open MIDI input device %s", namebuf);
    }
    for (i = 0, oss_nmidiout = 0; i < nmidiout; i++)
    {
        int fd = oss_midioutfd[i];
        char namebuf[80];
        int devno = midioutvec[i];
        if (devno < 0 || devno >= oss_nmidioutdevs)
            continue;
        sprintf(namebuf, "/dev/midi%s", oss_outdevnames[devno]);
        if (fd < 0)
        {
            sys_setalarm(1000000);
            fd = open(namebuf, O_WRONLY | O_MIDIFLAG);
            if (sys_verbose)
                post("tried to open %s write-only; got %d\n",
                    namebuf, fd);
        }
        if (fd >= 0)
            oss_midioutfd[oss_nmidiout++] = fd;
        else post("couldn't open MIDI output device %s", namebuf);
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

void midi_oss_init(void)
{
    int fd, devno;
    struct stat statbuf;
    char namebuf[80];
         /* we only try to detect devices before trying to open them, because
         when they're open, they migth not be possible to reopen here */
    static int initted = 0;
    if (initted)
        return;
    initted = 1;
    oss_nmidiindevs = oss_nmidioutdevs = 0;

    for (devno = 0; devno < NSEARCH; devno++)
    {
        if (devno == 0)
        {
                /* try to open the device for reading */
            fd = open("/dev/midi", O_RDONLY | O_NDELAY);
            if (fd >= 0)
            {
                close(fd);
                strcpy(oss_indevnames[oss_nmidiindevs++], "");
            }
            fd = open("/dev/midi", O_WRONLY | O_NDELAY);
            if (fd >= 0)
            {
                close(fd);
                strcpy(oss_outdevnames[oss_nmidioutdevs++], "");
            }
        }
        if (oss_nmidiindevs >= NSEARCH || oss_nmidioutdevs >= NSEARCH)
            break;

        sprintf(namebuf, "/dev/midi%d", devno);
        fd = open(namebuf, O_RDONLY | O_NDELAY);
        if (fd >= 0)
        {
            close(fd);
            sprintf(oss_indevnames[oss_nmidiindevs++], "%d", devno);
        }
        fd = open(namebuf, O_WRONLY | O_NDELAY);
        if (fd >= 0)
        {
            close(fd);
            sprintf(oss_outdevnames[oss_nmidioutdevs++], "%d", devno);
        }
        if (oss_nmidiindevs >= NSEARCH || oss_nmidioutdevs >= NSEARCH)
            break;

        sprintf(namebuf, "/dev/midi%2.2d", devno);
        fd = open(namebuf, O_RDONLY | O_NDELAY);
        if (fd >= 0)
        {
            close(fd);
            sprintf(oss_indevnames[oss_nmidiindevs++], "%d", devno);
        }
        fd = open(namebuf, O_WRONLY | O_NDELAY);
        if (fd >= 0)
        {
            close(fd);
            sprintf(oss_outdevnames[oss_nmidioutdevs++], "%d", devno);
        }
        if (oss_nmidiindevs >= NSEARCH || oss_nmidioutdevs >= NSEARCH)
            break;

    }
}

void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize)
{
    int i, ndev;
    midi_oss_init();

    if ((ndev = oss_nmidiindevs) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++)
        sprintf(indevlist + i * devdescsize,
            "/dev/midi%s", oss_indevnames[i]);
    *nindevs = ndev;

    if ((ndev = oss_nmidioutdevs) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++)
        sprintf(outdevlist + i * devdescsize,
            "/dev/midi%s", oss_outdevnames[i]);
    *noutdevs = ndev;
}
