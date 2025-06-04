/* Copyright (c) 2001 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* this file calls Ross Bencina's and Phil Burk's Portaudio package.  It's
    the main way in for Mac OS and, with Michael Casey's help, also into
    ASIO in Windows.

    For the polling scheduler, we call portaudio in callback mode and manage
    our own FIFO, so we can offer Pd "blocking" I/O calls. We can choose
    between two methods for waiting on the I/O to complete, either correct
    thread synchronization (by defining THREADSIGNAL) or just sleeping and polling.

    For the callback scheduler, we call portaudio in callback mode and simply
    provide a callback function that ticks the scheduler, polls the GUI, etc.
*/

#include "m_pd.h"
#include "s_stuff.h"
#include "s_utf8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <portaudio.h>
#ifdef _WIN32
#include <pa_win_wasapi.h>
#endif

#include "m_private_utils.h"
#include "s_audio_paring.h"

#ifndef _WIN32          /* for the "dup2" workaround -- do we still need it? */
#include <unistd.h>
#endif

/* enable proper thread synchronization instead of polling */
#if 1
#define THREADSIGNAL
#endif

    /* LATER try to figure out how to handle default devices in portaudio;
    the way s_audio.c handles them isn't going to work here. */

    /* public interface declared in m_imp.h */

    /* implementation */
static PaStream *pa_stream;
static int pa_inchans, pa_outchans;
static t_sample *pa_soundin, *pa_soundout;
static t_audiocallback pa_callback;

static int pa_started;
static volatile int pa_dio_error;

static char *pa_outbuf;
static sys_ringbuf pa_outring;
static char *pa_inbuf;
static sys_ringbuf pa_inring;
#ifdef THREADSIGNAL
t_semaphore *pa_sem;
#endif
/* number of seconds to wait before checking the audio device
(and possibly trying to reopen it). */
#ifndef POLL_TIMEOUT
#define POLL_TIMEOUT 2.0
#endif
static double pa_lastdactime;

static void pa_init(void)        /* Initialize PortAudio  */
{
    static int initialized;
    if (!initialized)
    {
#ifdef __APPLE__
        /* for some reason, on the Mac Pa_Initialize() closes file descriptor
        1 (standard output) As a workaround, dup it to another number and dup2
        it back afterward. */
        int newfd = dup(1);
        int another = open("/dev/null", 0);
        dup2(another, 1);
        int err = Pa_Initialize();
        close(1);
        close(another);
        if (newfd >= 0)
        {
            fflush(stdout);
            dup2(newfd, 1);
            close(newfd);
        }
#else
        int err = Pa_Initialize();
#endif


        if (err != paNoError)
        {
            post("error#%d opening audio: %s", err, Pa_GetErrorText(err));
            return;
        }
        initialized = 1;
    }
}

static int pa_lowlevel_callback(const void *inputBuffer,
    void *outputBuffer, unsigned long nframes,
    const PaStreamCallbackTimeInfo *outTime, PaStreamCallbackFlags myflags,
    void *userData)
{
    int i;
    unsigned int n, j;
    float *fbuf, *fp2, *fp3;
    t_sample *soundiop;
    if (nframes % DEFDACBLKSIZE)
    {
        post("warning: audio nframes %ld not a multiple of blocksize %d",
            nframes, (int)DEFDACBLKSIZE);
        nframes -= (nframes % DEFDACBLKSIZE);
    }
    for (n = 0; n < nframes; n += DEFDACBLKSIZE)
    {
        if (inputBuffer != NULL)
        {
            fbuf = ((float *)inputBuffer) + n*pa_inchans;
            soundiop = pa_soundin;
            for (i = 0, fp2 = fbuf; i < pa_inchans; i++, fp2++)
                    for (j = 0, fp3 = fp2; j < DEFDACBLKSIZE;
                        j++, fp3 += pa_inchans)
                            *soundiop++ = (t_sample)*fp3;
        }
        else memset((void *)pa_soundin, 0,
            DEFDACBLKSIZE * pa_inchans * sizeof(t_sample));
        memset((void *)pa_soundout, 0,
            DEFDACBLKSIZE * pa_outchans * sizeof(t_sample));
        (*pa_callback)();
        if (outputBuffer != NULL)
        {
            fbuf = ((float *)outputBuffer) + n*pa_outchans;
            soundiop = pa_soundout;
            for (i = 0, fp2 = fbuf; i < pa_outchans; i++, fp2++)
                for (j = 0, fp3 = fp2; j < DEFDACBLKSIZE;
                    j++, fp3 += pa_outchans)
                        *fp3 = (float)*soundiop++;
        }
    }
    return 0;
}

    /* callback for "non-callback" case in which we actually open portaudio
    in callback mode but fake "blocking mode". We communicate with the main
    thread via two FIFOs: the audio input buffer is copied to our input FIFO
    and the audio output buffer is filled with samples from our output FIFO.
    If the input FIFO is full or the output FIFO is empty, the callback does
    not wait for the scheduler; instead, the input buffer is discarded and
    the output buffer is filled with zeros. After each scheduler tick, the
    main thread tries to drain the input FIFO and fill the output FIFO.
    It can either wait on a semaphore, or poll in regular intervals;
    the first option should be more responsive. */
static int pa_fifo_callback(const void *inputBuffer,
    void *outputBuffer, unsigned long nframes,
    const PaStreamCallbackTimeInfo *outTime, PaStreamCallbackFlags myflags,
    void *userData)
{
    /* callback routine for non-callback client... throw samples into
            and read them out of a FIFO */
    unsigned long infiforoom = sys_ringbuf_getwriteavailable(&pa_inring),
        outfiforoom = sys_ringbuf_getreadavailable(&pa_outring);
    int ch;

#if CHECKFIFOS
    if (pa_inchans * sys_ringbuf_getreadavailable(&pa_outring) !=
        pa_outchans * sys_ringbuf_getwriteavailable(&pa_inring))
            post("warning: in and out rings unequal (%d, %d)",
                sys_ringbuf_getreadavailable(&pa_outring),
                sys_ringbuf_getwriteavailable(&pa_inring));
#endif
    if (infiforoom < nframes * STUFF->st_inchannels * sizeof(t_sample) ||
        outfiforoom < nframes * STUFF->st_outchannels * sizeof(t_sample))
    {
        /* data late: output zeros, drop inputs, and leave FIFos untouched */
        if (pa_started)
            pa_dio_error = 1;
        if (outputBuffer)
        {
            for (ch = 0; ch < pa_outchans; ch++)
            {
                unsigned long frame;
                float *fbuf = ((float *)outputBuffer) + ch;
                for (frame = 0; frame < nframes; frame++, fbuf += pa_outchans)
                    *fbuf = 0;
            }
        }
    }
    else
    {
        if (outputBuffer)
            sys_ringbuf_read(&pa_outring, outputBuffer,
                nframes*pa_outchans*sizeof(float), pa_outbuf);
        else if (pa_outchans)
            post("audio error: no outputBuffer but output channels");
        if (inputBuffer)
            sys_ringbuf_write(&pa_inring, inputBuffer,
                nframes*pa_inchans*sizeof(float), pa_inbuf);
        else if (pa_inchans)
            post("audio error: no inputBuffer but input channels");
    }
#ifdef THREADSIGNAL
    sys_semaphore_post(pa_sem);
#endif
    return 0;
}

PaError pa_open_callback(double samplerate, int inchannels, int outchannels,
    int framesperbuf, int indeviceno, int outdeviceno, PaStreamCallback *callbackfn)
{
    long bytesPerSample;
    PaError err;
    PaStreamParameters instreamparams, outstreamparams;
    PaStreamParameters*p_instreamparams=0, *p_outstreamparams=0;
#ifdef _WIN32
    PaWasapiStreamInfo wasapiinfo;
#endif

    /* fprintf(stderr, "nchan %d, flags %d, bufs %d, framesperbuf %d\n",
            nchannels, flags, nbuffers, framesperbuf); */

    instreamparams.device = indeviceno;
    instreamparams.channelCount = inchannels;
    instreamparams.sampleFormat = paFloat32;
    instreamparams.hostApiSpecificStreamInfo = 0;
    instreamparams.suggestedLatency = 0;
    if (inchannels > 0 && indeviceno >= 0)
        p_instreamparams = &instreamparams;

    outstreamparams.device = outdeviceno;
    outstreamparams.channelCount = outchannels;
    outstreamparams.sampleFormat = paFloat32;
    outstreamparams.hostApiSpecificStreamInfo = 0;
    outstreamparams.suggestedLatency = 0;
    if (outchannels > 0 && outdeviceno >= 0)
        p_outstreamparams = &outstreamparams;

#ifdef _WIN32
        /* set WASAPI options */
    memset(&wasapiinfo, 0, sizeof(wasapiinfo));
    wasapiinfo.size = sizeof(PaWasapiStreamInfo);
    wasapiinfo.hostApiType = paWASAPI;
    wasapiinfo.version = 1;
    wasapiinfo.flags = paWinWasapiAutoConvert;
    if (p_instreamparams)
    {
        const PaDeviceInfo *dev = Pa_GetDeviceInfo(p_instreamparams->device);
        const PaHostApiInfo *api = Pa_GetHostApiInfo(dev->hostApi);
        if (api->type == paWASAPI)
            p_instreamparams->hostApiSpecificStreamInfo = &wasapiinfo;
    }
    if (p_outstreamparams)
    {
        const PaDeviceInfo *dev = Pa_GetDeviceInfo(p_outstreamparams->device);
        const PaHostApiInfo *api = Pa_GetHostApiInfo(dev->hostApi);
        if (api->type == paWASAPI)
            p_outstreamparams->hostApiSpecificStreamInfo = &wasapiinfo;
    }
#endif

    err = Pa_IsFormatSupported(p_instreamparams, p_outstreamparams, samplerate);
    if (err != paFormatIsSupported)
    {
        /* check whether we have to change the numbers of channel and/or samplerate */
        const PaDeviceInfo* info = 0;
        double inrate = 0, outrate = 0;

        if (inchannels > 0)
        {
            if ((info = Pa_GetDeviceInfo(instreamparams.device)) != NULL)
            {
                inrate = info->defaultSampleRate;
                if (info->maxInputChannels < inchannels)
                    instreamparams.channelCount = info->maxInputChannels;
            }
        }

        if (outchannels > 0)
        {
            if ((info = Pa_GetDeviceInfo(outstreamparams.device)) != NULL)
            {
                outrate = info->defaultSampleRate;
                if (info->maxOutputChannels < outchannels)
                    outstreamparams.channelCount = info->maxOutputChannels;
            }
        }

        if (err == paInvalidSampleRate)
        {
            double oldrate = samplerate;
            samplerate = outrate > 0 ? outrate : inrate;
            logpost(0, PD_NORMAL,
                "warning: requested samplerate %d not supported, using %d.",
                    (int)oldrate, (int)samplerate);
        }

        err = Pa_IsFormatSupported(p_instreamparams, p_outstreamparams, samplerate);
        if (err != paFormatIsSupported)
            goto error;
    }
    err = Pa_OpenStream(&pa_stream, p_instreamparams, p_outstreamparams,
        samplerate, framesperbuf, paNoFlag, callbackfn, 0);
    if (err != paNoError)
        goto error;

    err = Pa_StartStream(pa_stream);
    if (err != paNoError)
    {
        post("could not start stream; closing audio: %s",
            Pa_GetErrorText(err));
        pa_close_audio();
        goto error;
    }
    STUFF->st_dacsr = samplerate;
    return paNoError;
error:
    pa_stream = NULL;
    return err;
}

int pa_open_audio(int inchans, int outchans, int rate, t_sample *soundin,
    t_sample *soundout, int framesperbuf, int nbuffers,
    int indeviceno, int outdeviceno, t_audiocallback callbackfn)
{
    PaError err;
    int j, devno, pa_indev = -1, pa_outdev = -1;

    pa_callback = callbackfn;
    /* fprintf(stderr, "open callback %d\n", (callbackfn != 0)); */
    pa_init();
    /* post("in %d out %d rate %d device %d", inchans, outchans, rate, deviceno); */

    if (pa_stream)
        pa_close_audio();

    if (inchans > 0)
    {
        for (j = 0, devno = 0; j < Pa_GetDeviceCount(); j++)
        {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(j);
            if (info->maxInputChannels > 0)
            {
                if (devno == indeviceno)
                {
                    if (inchans > info->maxInputChannels)
                      inchans = info->maxInputChannels;

                    pa_indev = j;
                    break;
                }
                devno++;
            }
        }
        if (pa_indev == -1)
        {
            inchans = 0;
            pd_error(0, "audio input device number (%d) out of range",
                indeviceno);
        }
    }

    if (outchans > 0)
    {
        for (j = 0, devno = 0; j < Pa_GetDeviceCount(); j++)
        {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(j);
            if (info->maxOutputChannels > 0)
            {
                if (devno == outdeviceno)
                {
                    if (outchans > info->maxOutputChannels)
                      outchans = info->maxOutputChannels;

                    pa_outdev = j;
                    break;
                }
                devno++;
            }
        }
        if (pa_outdev == -1)
        {
            outchans = 0;
            pd_error(0, "audio output device number (%d) out of range",
                outdeviceno);
        }
    }

    logpost(NULL, PD_VERBOSE, "input device %d, channels %d", pa_indev, inchans);
    logpost(NULL, PD_VERBOSE, "output device %d, channels %d", pa_outdev, outchans);
    logpost(NULL, PD_VERBOSE, "framesperbuf %d, nbufs %d", framesperbuf, nbuffers);
    logpost(NULL, PD_VERBOSE, "rate %d", rate);

    if (!inchans && !outchans)
        return (1);

    pa_inchans = STUFF->st_inchannels = inchans;
    pa_outchans = STUFF->st_outchannels = outchans;
    pa_soundin = soundin;
    pa_soundout = soundout;

    if (pa_inbuf)
        free((char *)pa_inbuf), pa_inbuf = 0;
    if (pa_outbuf)
        free((char *)pa_outbuf), pa_outbuf = 0;
#ifdef THREADSIGNAL
    pa_sem = sys_semaphore_create();
#endif
    pa_lastdactime = 0;

    if (callbackfn)
    {
        pa_callback = callbackfn;
        err = pa_open_callback(rate, inchans, outchans,
            framesperbuf, pa_indev, pa_outdev, pa_lowlevel_callback);
    }
    else
    {
        if (pa_inchans)
        {
            pa_inbuf = malloc(nbuffers*framesperbuf*pa_inchans*sizeof(float));
            sys_ringbuf_init(&pa_inring,
                nbuffers*framesperbuf*pa_inchans*sizeof(float), pa_inbuf,
                    nbuffers*framesperbuf*pa_inchans*sizeof(float));
        }
        if (pa_outchans)
        {
            pa_outbuf = malloc(nbuffers*framesperbuf*pa_outchans*sizeof(float));
            sys_ringbuf_init(&pa_outring,
                nbuffers*framesperbuf*pa_outchans*sizeof(float), pa_outbuf, 0);
        }
        err = pa_open_callback(rate, inchans, outchans,
            framesperbuf, pa_indev, pa_outdev, pa_fifo_callback);
    }
    pa_started = 0;
    pa_dio_error = 0;
    if (err != paNoError)
    {
        pd_error(0, "error opening audio: %s", Pa_GetErrorText(err));
        /* Pa_Terminate(); */
        return (1);
    }
    else logpost(NULL, PD_VERBOSE, "... opened OK.");
    return (0);
}

void pa_close_audio(void)
{
    if (pa_stream)
    {
        Pa_AbortStream(pa_stream);
        Pa_CloseStream(pa_stream);
    }
    pa_stream = 0;
    if (pa_inbuf)
        free((char *)pa_inbuf), pa_inbuf = 0;
    if (pa_outbuf)
        free((char *)pa_outbuf), pa_outbuf = 0;
#ifdef THREADSIGNAL
    if (pa_sem)
    {
        sys_semaphore_destroy(pa_sem);
        pa_sem = 0;
    }
#endif
}

void sys_do_close_audio(void);
void sys_do_reopen_audio(void);

    /* Always call with Pd locked! */
int pa_reopen_audio(void)
{
    int success;
        /* NB: unfortunately, Pa_IsStreamActive() always returns true,
           even if the audio device has been disconnected... */
    sys_do_close_audio();
#ifdef __APPLE__
        /* TODO: the portaudio coreaudio implementation doesn't handle
           re-connection, so suggest restarting */
    pd_error(0, "audio device not responding - closing audio");
    pd_error(0, "you may need to save and restart pd");
    return 0;
#else
        /* portaudio seems to handle this better on windows and linux */
    pd_error(0, "trying to reopen audio device");

    sys_do_reopen_audio(); /* try to reopen it */
    success = audio_isopen();

    if (success)
        pd_error(0, "successfully reopened audio device");
    else
        pd_error(0, "audio device not responding - closing audio.\n"
                    "please try to reconnect and reselect it in the settings (or toggle DSP)");
#endif
    return success;
}

int sched_idletask(void);

int pa_send_dacs(void)
{
    t_sample *fp;
    float *fp2, *fp3;
    float *conversionbuf;
    int j, k;
    int retval = SENDDACS_YES;
    double timeref = sys_getrealtime();
        /* this shouldn't really happen... */
    if (!pa_stream || (!STUFF->st_inchannels && !STUFF->st_outchannels))
        return (SENDDACS_NO);

    conversionbuf = (float *)alloca((STUFF->st_inchannels > STUFF->st_outchannels?
        STUFF->st_inchannels:STUFF->st_outchannels) * DEFDACBLKSIZE * sizeof(float));

    while (
        (sys_ringbuf_getreadavailable(&pa_inring) <
            (long)(STUFF->st_inchannels * DEFDACBLKSIZE*sizeof(t_sample))) ||
        (sys_ringbuf_getwriteavailable(&pa_outring) <
            (long)(STUFF->st_outchannels * DEFDACBLKSIZE*sizeof(t_sample))))
    {
#ifdef THREADSIGNAL
        if (sched_idletask())
            continue;
            /* only go to sleep if there is nothing else to do. */
        if (!sys_semaphore_waitfor(pa_sem, POLL_TIMEOUT))
        {
            sys_lock();
            pa_reopen_audio();
            sys_unlock();
            return SENDDACS_NO;
        }
        retval = SENDDACS_SLEPT;
#else
        if ((timeref - pa_lastdactime) >= POLL_TIMEOUT)
        {
            sys_lock();
            pa_reopen_audio();
            sys_unlock();
        }
        return SENDDACS_NO;
#endif
    }
    pa_lastdactime = timeref;
        /* write output */
    if (STUFF->st_outchannels)
    {
        for (j = 0, fp = STUFF->st_soundout, fp2 = conversionbuf;
            j < STUFF->st_outchannels; j++, fp2++)
                for (k = 0, fp3 = fp2; k < DEFDACBLKSIZE;
                    k++, fp++, fp3 += STUFF->st_outchannels)
                        *fp3 = *fp;
        sys_ringbuf_write(&pa_outring, conversionbuf,
            STUFF->st_outchannels*(DEFDACBLKSIZE*sizeof(float)), pa_outbuf);
    }
        /* read input */
    if (STUFF->st_inchannels)
    {
        sys_ringbuf_read(&pa_inring, conversionbuf,
            STUFF->st_inchannels*(DEFDACBLKSIZE*sizeof(float)), pa_inbuf);
        for (j = 0, fp = STUFF->st_soundin, fp2 = conversionbuf;
            j < STUFF->st_inchannels; j++, fp2++)
                for (k = 0, fp3 = fp2; k < DEFDACBLKSIZE;
                    k++, fp++, fp3 += STUFF->st_inchannels)
                        *fp = *fp3;
    }

    if (pa_dio_error)
    {
        sys_log_error(ERR_RESYNC);
        pa_dio_error = 0;
    }
    pa_started = 1;

    memset(STUFF->st_soundout, 0,
        DEFDACBLKSIZE*sizeof(t_sample)*STUFF->st_outchannels);

    return retval;
}

static char*pdi2devname(const PaDeviceInfo*pdi, char*buf, size_t bufsize) {
    const PaHostApiInfo *api = 0;
    char utf8device[MAXPDSTRING];
    utf8device[0] = 0;

    if(!pdi)
        return 0;

    api = Pa_GetHostApiInfo(pdi->hostApi);
    if(api)
        pd_snprintf(utf8device, MAXPDSTRING, "%s: %s",
            api->name, pdi->name);
    else
        pd_snprintf(utf8device, MAXPDSTRING, "%s",
            pdi->name);

    u8_nativetoutf8(buf, bufsize, utf8device, MAXPDSTRING);
    return buf;
}

    /* scanning for devices */
void pa_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti,
        int maxndev, int devdescsize)
{
    int i, nin = 0, nout = 0, ndev;
    *canmulti = 1;  /* one dev each for input and output */

    pa_init();
    ndev = Pa_GetDeviceCount();
    for (i = 0; i < ndev; i++)
    {
        char utf8device[MAXPDSTRING];
        const PaDeviceInfo *pdi = Pa_GetDeviceInfo(i);
        char*devname = pdi2devname(pdi, utf8device, MAXPDSTRING);
        if(!devname)continue;
        if (pdi->maxInputChannels > 0 && nin < maxndev)
        {
                /* LATER figure out how to get API name correctly */
            pd_snprintf(indevlist + nin * devdescsize, devdescsize,
                "%s", devname);
            nin++;
        }
        if (pdi->maxOutputChannels > 0 && nout < maxndev)
        {
            pd_snprintf(outdevlist + nout * devdescsize, devdescsize,
                "%s", devname);
            nout++;
        }
    }
    *nindevs = nin;
    *noutdevs = nout;
}
