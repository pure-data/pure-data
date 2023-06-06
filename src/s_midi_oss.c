/* Copyright (c) 1997-1999 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* MIDI I/O for Linux using OSS */

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "m_pd.h"
#include "s_stuff.h"

#define MAXNDEV 10
static int oss_nmididevs;
static char oss_midinames[MAXNDEV][20];
static int oss_nmidiin;
static int oss_midiinfd[MAXMIDIINDEV+1]; /* +1 to suppress buggy GCC warning */
static int oss_nmidiout;
static int oss_midioutfd[MAXMIDIOUTDEV+1];

static void close_one_midi_fd(int fd)
{
    int i, j;
    close(fd);
    for (i = 0; i < oss_nmidiin; i++)
    {
        if (oss_midiinfd[i] == fd)
        {
            for (j = i; j < oss_nmidiin-1; j++)
                oss_midiinfd[j] = oss_midiinfd[j+1];
            oss_nmidiin--;
        }
    }
    for (i = 0; i < oss_nmidiout; i++)
    {
        if (oss_midioutfd[i] == fd)
        {
            for (j = i; j < oss_nmidiout-1; j++)
                oss_midioutfd[j] = oss_midioutfd[j+1];
            oss_nmidiout--;
        }
    }
}

static void oss_midiout(int fd, int n)
{
    char b = n;
    if ((write(fd, (char *) &b, 1)) != 1)
    {
        perror("MIDI write");
        if (errno == ENODEV)  /* probably a USB dev got unplugged */
            close_one_midi_fd(fd);
    }
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
        int devno = midiinvec[i];
        if (devno < 0 || devno >= oss_nmididevs)
            continue;
        for (j = 0; j < nmidiout; j++)
            if (midioutvec[j] >= 0 && midioutvec[j] <= oss_nmididevs
                && !strcmp(oss_midinames[midioutvec[j]],
                oss_midinames[devno]))
                    outdevindex = j;

            /* try to open the device for read/write. */
        if (outdevindex >= 0)
        {
            sys_setalarm(1000000);
            fd = open(oss_midinames[devno], O_RDWR | O_MIDIFLAG);
            logpost(NULL, PD_VERBOSE, "tried to open %s read/write; got %d\n",
                oss_midinames[devno], fd);
            if (outdevindex >= 0 && fd >= 0)
                oss_midioutfd[outdevindex] = fd;
        }
            /* OK, try read-only */
        if (fd < 0)
        {
            sys_setalarm(1000000);
            fd = open(oss_midinames[devno], O_RDONLY | O_MIDIFLAG);
            logpost(NULL, PD_VERBOSE, "tried to open %s read-only; got %d\n",
                oss_midinames[devno], fd);
        }
        if (fd >= 0)
            oss_midiinfd[oss_nmidiin++] = fd;
        else post("couldn't open MIDI input device %s",
            oss_midinames[devno]);
    }
    for (i = 0, oss_nmidiout = 0; i < nmidiout; i++)
    {
        int fd = oss_midioutfd[i];
        int devno = midioutvec[i];
        if (devno < 0 || devno >= oss_nmididevs)
            continue;
        if (fd < 0)
        {
            sys_setalarm(1000000);
            fd = open(oss_midinames[devno], O_WRONLY | O_MIDIFLAG);
            logpost(NULL, PD_VERBOSE, "tried to open %s write-only; got %d\n",
                oss_midinames[devno], fd);
        }
        if (fd >= 0)
            oss_midioutfd[oss_nmidiout++] = fd;
        else post("couldn't open MIDI output device %s",
            oss_midinames[devno]);
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

void sys_poll_midi(void)
{
    int i, throttle = 100;
    int did = 1, maxfd = 0;
    while (did)
    {
        did = 0;
        if (throttle-- < 0)
            break;
        for (i = 0; i < oss_nmidiin; i++)
        {
            char c;
            int ret = read(oss_midiinfd[i], &c, 1);
            if (ret < 0)
            {
                if (errno == ENODEV)
                {
                    close_one_midi_fd(oss_midiinfd[i]);
                    return; /* oss_nmidiinfd changed so blow off the rest */
                }
                else if (errno != EAGAIN)
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

    oss_nmididevs = 0;
    if (oss_nmididevs < MAXNDEV && !stat("/dev/midi", &statbuf))
        strcpy(oss_midinames[oss_nmididevs++], "/dev/midi");
    for (devno = 0; devno < MAXNDEV; devno++)
    {
        sprintf(namebuf, "/dev/midi%d", devno);
        if (oss_nmididevs < MAXNDEV && !stat(namebuf, &statbuf))
            strcpy(oss_midinames[oss_nmididevs++], namebuf);
        sprintf(namebuf, "/dev/midi%2.2d", devno);
        if (oss_nmididevs < MAXNDEV && !stat(namebuf, &statbuf))
            strcpy(oss_midinames[oss_nmididevs++], namebuf);
    }
}

void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize)
{
    int i, ndev;
    midi_oss_init();

    if ((ndev = oss_nmididevs) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++)
        strcpy(indevlist + i * devdescsize, oss_midinames[i]);
    *nindevs = ndev;

    if ((ndev = oss_nmididevs) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++)
        strcpy(outdevlist + i * devdescsize,
            oss_midinames[i]);
    *noutdevs = ndev;
}
