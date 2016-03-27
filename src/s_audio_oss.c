/* Copyright (c) 1997-2003 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* this file inputs and outputs audio using the OSS API available on linux. */

#include <sys/soundcard.h>

#ifndef SNDCTL_DSP_GETISPACE
#define SNDCTL_DSP_GETISPACE SOUND_PCM_GETISPACE
#endif
#ifndef SNDCTL_DSP_GETOSPACE
#define SNDCTL_DSP_GETOSPACE SOUND_PCM_GETOSPACE
#endif

#include "m_pd.h"
#include "s_stuff.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>


/* Defines */
#define DEBUG(x) x
#define DEBUG2(x) {x;}

#define OSS_MAXCHPERDEV 32      /* max channels per OSS device */
#define OSS_MAXDEV 4            /* maximum number of input or output devices */
#define OSS_DEFFRAGSIZE 256     /* default log fragment size (frames) */
#define OSS_DEFAUDIOBUF 40000   /* default audiobuffer, microseconds */
#define OSS_DEFAULTCH 2
#define RME_DEFAULTCH 8     /* need this even if RME undefined */
typedef int16_t t_oss_int16;
typedef int32_t t_oss_int32;
#define OSS_MAXSAMPLEWIDTH sizeof(t_oss_int32)
#define OSS_BYTESPERCHAN(width) (DEFDACBLKSIZE * (width))
#define OSS_XFERSAMPS(chans) (DEFDACBLKSIZE* (chans))
#define OSS_XFERSIZE(chans, width) (DEFDACBLKSIZE * (chans) * (width))

/* GLOBALS */
static int linux_meters;        /* true if we're metering */
static t_sample linux_inmax;       /* max input amplitude */
static t_sample linux_outmax;      /* max output amplitude */
static int linux_fragsize = 0;  /* for block mode; block size (sample frames) */
extern int audio_blocksize;     /* stolen from s_audio.c */
/* our device handles */

typedef struct _oss_dev
{
    int d_fd;
    unsigned int d_space;   /* bytes available for writing/reading  */
    int d_bufsize;          /* total buffer size in blocks for this device */
    int d_dropcount;        /* # of buffers to drop for resync (output only) */
    unsigned int d_nchannels;   /* number of channels for this device */
    unsigned int d_bytespersamp; /* bytes per sample (2 for 16 bit, 4 for 32) */
} t_oss_dev;

static t_oss_dev linux_dacs[OSS_MAXDEV];
static t_oss_dev linux_adcs[OSS_MAXDEV];
static int linux_noutdevs = 0;
static int linux_nindevs = 0;

    /* exported variables */
t_float sys_dacsr;
t_sample *sys_soundout;
t_sample *sys_soundin;

    /* OSS-specific private variables */
static int oss_blockmode = 1;   /* flag to use "blockmode"  */
static char ossdsp[] = "/dev/dsp%d";

    /* don't assume we can turn all 31 bits when doing float-to-fix;
    otherwise some audio drivers (e.g. Midiman/ALSA) wrap around. */
#define FMAX 0x7ffff000
#define CLIP32(x) (((x)>FMAX)?FMAX:((x) < -FMAX)?-FMAX:(x))

/* ---------------- public routines ----------------------- */

static int oss_ndev = 0;

    /* find out how many OSS devices we have.  Since this has to
    open the devices to find out if they're there, we have
    to be called before audio is actually started up.  So we
    cache the results, which in effect are the number of available
    devices.  */
void oss_init(void)
{
    int fd, i;
    static int countedthem = 0;
    if (countedthem)
        return;
    for (i = 0; i < 10; i++)
    {
        char devname[100];
        if (i == 0)
            strcpy(devname, "/dev/dsp");
        else sprintf(devname, "/dev/dsp%d", i);
        if ( (fd = open(devname, O_WRONLY|O_NONBLOCK)) != -1)
        {
            oss_ndev++;
            close(fd);
        }
        else break;
    }
    countedthem = 1;
}

typedef struct _multidev {
     int fd;
     int channels;
     int format;
} t_multidev;

int oss_reset(int fd) {
     int err;
     if ((err = ioctl(fd,SNDCTL_DSP_RESET)) < 0)
          error("OSS: Could not reset");
     return err;
}

void oss_configure(t_oss_dev *dev, int srate, int dac, int skipblocksize,
    int suggestedblocksize)
{
    int orig, param, nblk, fd = dev->d_fd, wantformat;
    int nchannels = dev->d_nchannels;
    int advwas = sys_schedadvance;

    audio_buf_info ainfo;

        /* we only know how to do 2 byte samples */
    wantformat = AFMT_S16_NE;
    dev->d_bytespersamp = 2;

    param = wantformat;

    if (ioctl(fd, SNDCTL_DSP_SETFMT, &param) == -1)
        fprintf(stderr,"OSS: Could not set DSP format\n");
    else if (wantformat != param)
        fprintf(stderr,"OSS: DSP format: wanted %d, got %d\n",
            wantformat, param);

    /* sample rate */
    orig = param = srate;
    if (ioctl(fd, SNDCTL_DSP_SPEED, &param) == -1)
        fprintf(stderr,"OSS: Could not set sampling rate for device\n");
    else if( orig != param )
        fprintf(stderr,"OSS: sampling rate: wanted %d, got %d\n",
            orig, param );

    if (oss_blockmode && !skipblocksize)
    {
        int fragbytes, logfragsize, nfragment;
            /* setting fragment count and size.  */
        linux_fragsize = suggestedblocksize;
        if (!linux_fragsize)
        {
            linux_fragsize = OSS_DEFFRAGSIZE;
            while (linux_fragsize > DEFDACBLKSIZE
                && linux_fragsize * 6 > sys_advance_samples)
                    linux_fragsize = linux_fragsize/2;
        }
            /* post("adv_samples %d", sys_advance_samples); */
        nfragment = (sys_schedadvance * (44100. * 1.e-6)) / linux_fragsize;

        fragbytes = linux_fragsize * (dev->d_bytespersamp * nchannels);
        logfragsize = ilog2(fragbytes);

        if (fragbytes != (1 << logfragsize))
            post("warning: OSS takes only power of 2 blocksize; using %d",
                (1 << logfragsize)/(dev->d_bytespersamp * nchannels));
        if (sys_verbose)
            post("setting nfrags = %d, fragsize %d\n", nfragment, fragbytes);

        param = orig = (nfragment<<16) + logfragsize;
        if (ioctl(fd,SNDCTL_DSP_SETFRAGMENT, &param) == -1)
            error("OSS: Could not set or read fragment size\n");
        if (param != orig)
        {
            nfragment = ((param >> 16) & 0xffff);
            logfragsize = (param & 0xffff);
            post("warning: actual fragments %d, blocksize %d",
                nfragment, (1 << logfragsize));
        }
        if (sys_verbose)
            post("audiobuffer set to %d msec", (int)(0.001 * sys_schedadvance));
    }

    if (dac)
    {
        /* use "free space" to learn the buffer size.  Normally you
        should set this to your own desired value; but this seems not
        to be implemented uniformly across different sound cards.  LATER
        we should figure out what to do if the requested scheduler advance
        is greater than this buffer size; for now, we just print something
        out.  */

        int defect;
        if (ioctl(fd, SNDCTL_DSP_GETOSPACE,&ainfo) < 0)
           fprintf(stderr,"OSS: ioctl on output device failed");
        dev->d_bufsize = ainfo.bytes;

        defect = sys_advance_samples * (dev->d_bytespersamp * nchannels)
            - dev->d_bufsize - OSS_XFERSIZE(nchannels, dev->d_bytespersamp);
        if (defect > 0)
        {
            if (sys_verbose || defect > (dev->d_bufsize >> 2))
                fprintf(stderr,
                    "OSS: requested audio buffer size %d limited to %d\n",
                        sys_advance_samples * (dev->d_bytespersamp * nchannels),
                        dev->d_bufsize);
            sys_advance_samples =
                (dev->d_bufsize - OSS_XFERSAMPS(nchannels)) /
                    (dev->d_bytespersamp *nchannels);
        }
    }
}

static int oss_setchannels(int fd, int wantchannels, char *devname)
{
    int param;
    if (sys_verbose)
        post("setchan %d", wantchannels);
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &param) == -1)
    {
        if (sys_verbose)
            error("OSS: SOUND_DSP_READ_CHANNELS failed %s", devname);
    }
    else
    {
        if (sys_verbose)
            post("channels originally %d for %s", param, devname);
        if (param == wantchannels)
        {
            if (sys_verbose)
                post("number of channels doesn't need setting\n");
            return (wantchannels);
        }
    }
    param = wantchannels;
whynot:
    while (param > 1)
    {
        int save = param;
        if (ioctl(fd, SNDCTL_DSP_CHANNELS, &param) == -1)
            error("OSS: SNDCTL_DSP_CHANNELS failed %s",devname);
        else if (param == save)
            return (param);
        param = save - 1;
    }

    return (0);
}

int oss_open_audio(int nindev,  int *indev,  int nchin,  int *chin,
    int noutdev, int *outdev, int nchout, int *chout, int rate,
        int blocksize)
{
    int capabilities = 0;
    int inchannels = 0, outchannels = 0;
    char devname[20];
    int n, i, fd, flags;
    char buf[OSS_MAXSAMPLEWIDTH * DEFDACBLKSIZE * OSS_MAXCHPERDEV];
    int num_devs = 0;
    int wantmore=0;
    int spread = 0;
    audio_buf_info ainfo;

    linux_nindevs = linux_noutdevs = 0;
        /* mark devices unopened */
    for (i = 0; i < OSS_MAXDEV; i++)
        linux_adcs[i].d_fd = linux_dacs[i].d_fd = -1;

    /* open output devices */
    wantmore=0;
    if (noutdev < 0 || nindev < 0)
        bug("linux_open_audio");

    for (n = 0; n < noutdev; n++)
    {
        int gotchans, j, inindex = -1;
        int thisdevice = (outdev[n] >= 0 ? outdev[n] : 0);
        int wantchannels = (nchout>n) ? chout[n] : wantmore;
        fd = -1;
        if (!wantchannels)
            goto end_out_loop;

        if (thisdevice > 0)
            sprintf(devname, "/dev/dsp%d", thisdevice);
        else sprintf(devname, "/dev/dsp");
            /* search for input request for same device.  Succeed only
            if the number of channels matches. */
        for (j = 0; j < nindev; j++)
            if (indev[j] == thisdevice && chin[j] == wantchannels)
                inindex = j;

            /* if the same device is requested for input and output,
            try to open it read/write */
        if (inindex >= 0)
        {
            sys_setalarm(1000000);
            if ((fd = open(devname, O_RDWR | O_NDELAY)) == -1)
            {
                post("%s (read/write): %s", devname, strerror(errno));
                post("(now will try write-only...)");
            }
            else
            {
                if (fcntl(fd, F_SETFD, 1) < 0)
                    post("couldn't set close-on-exec flag on audio");
                if ((flags = fcntl(fd, F_GETFL)) < 0)
                    post("couldn't get audio device flags");
                else if (fcntl(fd, F_SETFL, flags & (~O_NDELAY)) < 0)
                    post("couldn't set audio device flags");
                if (sys_verbose)
                    post("opened %s for reading and writing\n", devname);
                linux_adcs[inindex].d_fd = fd;
            }
        }
            /* if that didn't happen or if it failed, try write-only */
        if (fd == -1)
        {
            sys_setalarm(1000000);
            if ((fd = open(devname, O_WRONLY | O_NDELAY)) == -1)
            {
                post("%s (writeonly): %s",
                     devname, strerror(errno));
                break;
            }
            if (fcntl(fd, F_SETFD, 1) < 0)
                post("couldn't set close-on-exec flag on audio");
            if ((flags = fcntl(fd, F_GETFL)) < 0)
                post("couldn't get audio device flags");
            else if (fcntl(fd, F_SETFL, flags & (~O_NDELAY)) < 0)
                post("couldn't set audio device flags");
            if (sys_verbose)
                post("opened %s for writing only\n", devname);
        }
        if (ioctl(fd, SNDCTL_DSP_GETCAPS, &capabilities) == -1)
            error("OSS: SNDCTL_DSP_GETCAPS failed %s", devname);

        gotchans = oss_setchannels(fd,
            (wantchannels>OSS_MAXCHPERDEV)?OSS_MAXCHPERDEV:wantchannels,
                    devname);

        if (sys_verbose)
            post("opened audio output on %s; got %d channels",
                 devname, gotchans);

        if (gotchans < 2)
        {
                /* can't even do stereo? just give up. */
            close(fd);
        }
        else
        {
            linux_dacs[linux_noutdevs].d_nchannels = gotchans;
            linux_dacs[linux_noutdevs].d_fd = fd;
            oss_configure(linux_dacs+linux_noutdevs, rate, 1, 0, blocksize);

            linux_noutdevs++;
            outchannels += gotchans;
            if (inindex >= 0)
            {
                linux_adcs[inindex].d_nchannels = gotchans;
                chin[inindex] = gotchans;
            }
        }
        /* LATER think about spreading large numbers of channels over
            various dsp's and vice-versa */
        wantmore = wantchannels - gotchans;
    end_out_loop: ;
    }

    /* open input devices */
    wantmore = 0;
    for (n = 0; n < nindev; n++)
    {
        int gotchans=0;
        int thisdevice = (indev[n] >= 0 ? indev[n] : 0);
        int wantchannels = (nchin>n)?chin[n]:wantmore;
        int alreadyopened = 0;
        if (!wantchannels)
            goto end_in_loop;

        if (thisdevice > 0)
            sprintf(devname, "/dev/dsp%d", thisdevice);
        else sprintf(devname, "/dev/dsp");

        sys_setalarm(1000000);

            /* perhaps it's already open from the above? */
        if (linux_adcs[n].d_fd >= 0)
        {
            fd = linux_adcs[n].d_fd;
            alreadyopened = 1;
            if (sys_verbose)
                post("already opened it");
        }
        else
        {
                /* otherwise try to open it here. */
            if ((fd = open(devname, O_RDONLY | O_NDELAY)) == -1)
            {
                post("%s (readonly): %s", devname, strerror(errno));
                goto end_in_loop;
            }
            if (fcntl(fd, F_SETFD, 1) < 0)
                post("couldn't set close-on-exec flag on audio");
            if ((flags = fcntl(fd, F_GETFL)) < 0)
                post("couldn't get audio device flags");
            else if (fcntl(fd, F_SETFL, flags & (~O_NDELAY)) < 0)
                post("couldn't set audio device flags");
            if (sys_verbose)
                post("opened %s for reading only\n", devname);
        }
        linux_adcs[linux_nindevs].d_fd = fd;

        gotchans = oss_setchannels(fd,
            (wantchannels>OSS_MAXCHPERDEV)?OSS_MAXCHPERDEV:wantchannels,
                devname);
        if (sys_verbose)
            post("opened audio input device %s; got %d channels",
                devname, gotchans);

        if (gotchans < 1)
        {
            close(fd);
            goto end_in_loop;
        }

        linux_adcs[linux_nindevs].d_nchannels = gotchans;

        oss_configure(linux_adcs+linux_nindevs, rate, 0, alreadyopened,
            blocksize);

        inchannels += gotchans;
        linux_nindevs++;

        wantmore = wantchannels-gotchans;
        /* LATER think about spreading large numbers of channels over
            various dsp's and vice-versa */
    end_in_loop: ;
    }

    /* We have to do a read to start the engine. This is
       necessary because sys_send_dacs waits until the input
       buffer is filled and only reads on a filled buffer.
       This is good, because it's a way to make sure that we
       will not block.  But I wonder why we only have to read
       from one of the devices and not all of them??? */

    if (linux_nindevs)
    {
        if (sys_verbose)
            fprintf(stderr,("OSS: issuing first ADC 'read' ... "));
        read(linux_adcs[0].d_fd, buf,
            linux_adcs[0].d_bytespersamp *
                linux_adcs[0].d_nchannels * DEFDACBLKSIZE);
        if (sys_verbose)
            fprintf(stderr, "...done.\n");
    }
        /* now go and fill all the output buffers. */
    for (i = 0; i < linux_noutdevs; i++)
    {
        int j;
        memset(buf, 0, linux_dacs[i].d_bytespersamp *
                linux_dacs[i].d_nchannels * DEFDACBLKSIZE);
        for (j = 0; j < sys_advance_samples/DEFDACBLKSIZE; j++)
            write(linux_dacs[i].d_fd, buf,
                linux_dacs[i].d_bytespersamp *
                    linux_dacs[i].d_nchannels * DEFDACBLKSIZE);
    }
    sys_setalarm(0);
    sys_inchannels = inchannels;
    sys_outchannels = outchannels;
    return (0);
}

void oss_close_audio( void)
{
     int i;
     for (i=0;i<linux_nindevs;i++)
          close(linux_adcs[i].d_fd);

     for (i=0;i<linux_noutdevs;i++)
          close(linux_dacs[i].d_fd);

    linux_nindevs = linux_noutdevs = 0;
}

static int linux_dacs_write(int fd,void* buf,long bytes)
{
    return write(fd, buf, bytes);
}

static int linux_adcs_read(int fd,void*  buf,long bytes)
{
     return read(fd, buf, bytes);
}

    /* query audio devices for "available" data size. */
static void oss_calcspace(void)
{
    int dev;
    audio_buf_info ainfo;
    for (dev=0; dev < linux_noutdevs; dev++)
    {
        if (ioctl(linux_dacs[dev].d_fd, SNDCTL_DSP_GETOSPACE, &ainfo) < 0)
           fprintf(stderr,"OSS: ioctl on output device %d failed",dev);
        linux_dacs[dev].d_space = ainfo.bytes;
    }

    for (dev = 0; dev < linux_nindevs; dev++)
    {
        if (ioctl(linux_adcs[dev].d_fd, SNDCTL_DSP_GETISPACE,&ainfo) < 0)
            fprintf(stderr, "OSS: ioctl on input device %d, fd %d failed",
                dev, linux_adcs[dev].d_fd);
        linux_adcs[dev].d_space = ainfo.bytes;
    }
}

void linux_audiostatus(void)
{
    int dev;
    if (!oss_blockmode)
    {
        oss_calcspace();
        for (dev=0; dev < linux_noutdevs; dev++)
            fprintf(stderr, "dac %d space %d\n", dev, linux_dacs[dev].d_space);

        for (dev = 0; dev < linux_nindevs; dev++)
            fprintf(stderr, "adc %d space %d\n", dev, linux_adcs[dev].d_space);

    }
}

/* this call resyncs audio output and input which will cause discontinuities
in audio output and/or input. */

static void oss_doresync( void)
{
    int dev, zeroed = 0, wantsize;
    char buf[OSS_MAXSAMPLEWIDTH * DEFDACBLKSIZE * OSS_MAXCHPERDEV];
    audio_buf_info ainfo;

        /* 1. if any input devices are ahead (have more than 1 buffer stored),
            drop one or more buffers worth */
    for (dev = 0; dev < linux_nindevs; dev++)
    {
        if (linux_adcs[dev].d_space == 0)
        {
            linux_adcs_read(linux_adcs[dev].d_fd, buf,
                OSS_XFERSIZE(linux_adcs[dev].d_nchannels,
                    linux_adcs[dev].d_bytespersamp));
        }
        else while (linux_adcs[dev].d_space >
            OSS_XFERSIZE(linux_adcs[dev].d_nchannels,
                linux_adcs[dev].d_bytespersamp))
        {
            linux_adcs_read(linux_adcs[dev].d_fd, buf,
                OSS_XFERSIZE(linux_adcs[dev].d_nchannels,
                    linux_adcs[dev].d_bytespersamp));
            if (ioctl(linux_adcs[dev].d_fd, SNDCTL_DSP_GETISPACE, &ainfo) < 0)
            {
                fprintf(stderr, "OSS: ioctl on input device %d, fd %d failed",
                    dev, linux_adcs[dev].d_fd);
                break;
            }
            linux_adcs[dev].d_space = ainfo.bytes;
        }
    }

        /* 2. if any output devices are behind, feed them zeros to catch them
            up */
    for (dev = 0; dev < linux_noutdevs; dev++)
    {
        while (linux_dacs[dev].d_space > linux_dacs[dev].d_bufsize -
            sys_advance_samples * (linux_dacs[dev].d_nchannels *
                linux_dacs[dev].d_bytespersamp))
        {
            if (!zeroed)
            {
                unsigned int i;
                for (i = 0; i < OSS_XFERSAMPS(linux_dacs[dev].d_nchannels);
                    i++)
                        buf[i] = 0;
                zeroed = 1;
            }
            linux_dacs_write(linux_dacs[dev].d_fd, buf,
                OSS_XFERSIZE(linux_dacs[dev].d_nchannels,
                    linux_dacs[dev].d_bytespersamp));
            if (ioctl(linux_dacs[dev].d_fd, SNDCTL_DSP_GETOSPACE, &ainfo) < 0)
            {
                fprintf(stderr, "OSS: ioctl on output device %d, fd %d failed",
                    dev, linux_dacs[dev].d_fd);
                break;
            }
            linux_dacs[dev].d_space = ainfo.bytes;
        }
    }
        /* 3. if any DAC devices are too far ahead, plan to drop the
            number of frames which will let the others catch up. */
    for (dev = 0; dev < linux_noutdevs; dev++)
    {
        if (linux_dacs[dev].d_space > linux_dacs[dev].d_bufsize -
            (sys_advance_samples - 1) * linux_dacs[dev].d_nchannels *
                linux_dacs[dev].d_bytespersamp)
        {
            linux_dacs[dev].d_dropcount = sys_advance_samples - 1 -
                (linux_dacs[dev].d_space - linux_dacs[dev].d_bufsize) /
                     (linux_dacs[dev].d_nchannels *
                        linux_dacs[dev].d_bytespersamp) ;
        }
        else linux_dacs[dev].d_dropcount = 0;
    }
}

int oss_send_dacs(void)
{
    t_sample *fp1, *fp2;
    long fill;
    int i, j, dev, rtnval = SENDDACS_YES;
    char buf[OSS_MAXSAMPLEWIDTH * DEFDACBLKSIZE * OSS_MAXCHPERDEV];
    t_oss_int16 *sp;
    t_oss_int32 *lp;
        /* the maximum number of samples we should have in the ADC buffer */
    int idle = 0;
    int thischan;
    double timeref, timenow;

    if (!linux_nindevs && !linux_noutdevs)
        return (SENDDACS_NO);

    if (!oss_blockmode)
    {
        /* determine whether we're idle.  This is true if either (1)
        some input device has less than one buffer to read or (2) some
        output device has fewer than (sys_advance_samples) blocks buffered
        already. */
        oss_calcspace();

        for (dev=0; dev < linux_noutdevs; dev++)
            if (linux_dacs[dev].d_dropcount ||
                (linux_dacs[dev].d_bufsize - linux_dacs[dev].d_space >
                    sys_advance_samples * linux_dacs[dev].d_bytespersamp *
                        linux_dacs[dev].d_nchannels))
                            idle = 1;
        for (dev=0; dev < linux_nindevs; dev++)
            if (linux_adcs[dev].d_space <
                OSS_XFERSIZE(linux_adcs[dev].d_nchannels,
                    linux_adcs[dev].d_bytespersamp))
                        idle = 1;
    }

    if (idle && !oss_blockmode)
    {
            /* sometimes---rarely---when the ADC available-byte-count is
            zero, it's genuine, but usually it's because we're so
            late that the ADC has overrun its entire kernel buffer.  We
            distinguish between the two by waiting 2 msec and asking again.
            There should be an error flag we could check instead; look for this
            someday... */
        for (dev = 0;dev < linux_nindevs; dev++)
            if (linux_adcs[dev].d_space == 0)
        {
            audio_buf_info ainfo;
            sys_microsleep(2000);
            oss_calcspace();
            if (linux_adcs[dev].d_space != 0) continue;

                /* here's the bad case.  Give up and resync. */
            sys_log_error(ERR_DATALATE);
            oss_doresync();
            return (SENDDACS_NO);
        }
            /* check for slippage between devices, either because
            data got lost in the driver from a previous late condition, or
            because the devices aren't synced.  When we're idle, no
            input device should have more than one buffer readable and
            no output device should have less than sys_advance_samples-1
            */

        for (dev=0; dev < linux_noutdevs; dev++)
            if (!linux_dacs[dev].d_dropcount &&
                (linux_dacs[dev].d_bufsize - linux_dacs[dev].d_space <
                    (sys_advance_samples - 2) *
                        (linux_dacs[dev].d_bytespersamp *
                            linux_dacs[dev].d_nchannels)))
                        goto badsync;
        for (dev=0; dev < linux_nindevs; dev++)
            if (linux_adcs[dev].d_space > 3 *
                OSS_XFERSIZE(linux_adcs[dev].d_nchannels,
                    linux_adcs[dev].d_bytespersamp))
                        goto badsync;

            /* return zero to tell the scheduler we're idle. */
        return (SENDDACS_NO);
    badsync:
        sys_log_error(ERR_RESYNC);
        oss_doresync();
        return (SENDDACS_NO);

    }

        /* do output */

    timeref = sys_getrealtime();
    for (dev=0, thischan = 0; dev < linux_noutdevs; dev++)
    {
        int nchannels = linux_dacs[dev].d_nchannels;
        if (linux_dacs[dev].d_dropcount)
            linux_dacs[dev].d_dropcount--;
        else
        {
            if (linux_dacs[dev].d_bytespersamp == 2)
            {
                for (i = DEFDACBLKSIZE,  fp1 = sys_soundout +
                    DEFDACBLKSIZE*thischan,
                    sp = (t_oss_int16 *)buf; i--; fp1++, sp += nchannels)
                {
                    for (j=0, fp2 = fp1; j<nchannels; j++, fp2 += DEFDACBLKSIZE)
                    {
                        int s = *fp2 * 32767.;
                        if (s > 32767) s = 32767;
                        else if (s < -32767) s = -32767;
                        sp[j] = s;
                    }
                }
            }
            linux_dacs_write(linux_dacs[dev].d_fd, buf,
                OSS_XFERSIZE(nchannels, linux_dacs[dev].d_bytespersamp));
            if ((timenow = sys_getrealtime()) - timeref > 0.002)
            {
                if (!oss_blockmode)
                    sys_log_error(ERR_DACSLEPT);
                else rtnval = SENDDACS_SLEPT;
            }
            timeref = timenow;
        }
        thischan += nchannels;
    }
    memset(sys_soundout, 0,
        sys_outchannels * (sizeof(t_sample) * DEFDACBLKSIZE));

        /* do input */

    for (dev = 0, thischan = 0; dev < linux_nindevs; dev++)
    {
        int nchannels = linux_adcs[dev].d_nchannels;
        linux_adcs_read(linux_adcs[dev].d_fd, buf,
            OSS_XFERSIZE(nchannels, linux_adcs[dev].d_bytespersamp));

        if ((timenow = sys_getrealtime()) - timeref > 0.002)
        {
            if (!oss_blockmode)
                sys_log_error(ERR_ADCSLEPT);
            else
                rtnval = SENDDACS_SLEPT;
        }
        timeref = timenow;

        if (linux_adcs[dev].d_bytespersamp == 2)
        {
            for (i = DEFDACBLKSIZE,fp1 = sys_soundin + thischan*DEFDACBLKSIZE,
                sp = (t_oss_int16 *)buf; i--; fp1++, sp += nchannels)
            {
                for (j=0;j<nchannels;j++)
                    fp1[j*DEFDACBLKSIZE] = (float)sp[j]*(float)3.051850e-05;
            }
        }
        thischan += nchannels;
     }
     return (rtnval);
}

void oss_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti,
        int maxndev, int devdescsize)
{
    int i, ndev;
    *canmulti = 2;  /* supports multiple devices */
    if ((ndev = oss_ndev) > maxndev)
        ndev = maxndev;
    for (i = 0; i < ndev; i++)
    {
        sprintf(indevlist + i * devdescsize, "OSS device #%d", i+1);
        sprintf(outdevlist + i * devdescsize, "OSS device #%d", i+1);
    }
    *nindevs = *noutdevs = ndev;
}
