/* Copyright (c) 2001 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* this file calls Ross Bencina's and Phil Burk's Portaudio package.  It's
    the main way in for Mac OS and, with Michael Casey's help, also into
    ASIO in Windows.

    Both blocking and non-blocking call styles are supported.  If non-blocking
    is requested, either we call portaudio in non-blocking mode, or else we
    call portaudio in callback mode and manage our own FIFO se we can offer
    Pd "blocking" I/O calls.  To do the latter we define FAKEBLOCKING; this
    works better in MACOSX (gets 40 msec lower latency!) and might also in
    Windows.  If FAKEBLOCKING is defined we can choose between two methods
    for waiting on the (presumably other-thread) I/O to complete, either
    correct thread synchronization (by defining THREADSIGNAL) or just sleeping
    and polling; the latter seems to work better so far.
*/

/* dolist...
    switch to usleep in s_inter.c
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

#ifndef _WIN32          /* for the "dup2" workaround -- do we still need it? */
#include <unistd.h>
#endif

#if 1
#define FAKEBLOCKING
#endif

/* define this to enable thread signaling instead of polling */
// #define THREADSIGNAL

    /* LATER try to figure out how to handle default devices in portaudio;
    the way s_audio.c handles them isn't going to work here. */

    /* public interface declared in m_imp.h */

    /* implementation */
static PaStream *pa_stream;
static int pa_inchans, pa_outchans;
static t_sample *pa_soundin, *pa_soundout;
static t_audiocallback pa_callback;

static int pa_started;
static int pa_nbuffers;
static int pa_dio_error;

#ifdef FAKEBLOCKING
#include "s_audio_paring.h"
static PA_VOLATILE char *pa_outbuf;
static PA_VOLATILE sys_ringbuf pa_outring;
static PA_VOLATILE char *pa_inbuf;
static PA_VOLATILE sys_ringbuf pa_inring;
#ifdef THREADSIGNAL
#include <pthread.h>
#include <errno.h>
#ifdef _WIN32
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif
/* maximum time (in ms) to wait for the condition variable. */
#ifndef THREADSIGNAL_TIMEOUT
#define THREADSIGNAL_TIMEOUT 1000
#endif
static pthread_mutex_t pa_mutex;
static pthread_cond_t pa_sem;
#else /* THREADSIGNAL */
#if defined (FAKEBLOCKING) && defined(_WIN32)
#include <windows.h>    /* for Sleep() */
#endif
/* max time (in ms) before trying to reopen the device */
#ifndef POLL_TIMEOUT
#define POLL_TIMEOUT 2000
#endif
#endif /* THREADSIGNAL */
#endif  /* FAKEBLOCKING */

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


        if ( err != paNoError )
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

#ifdef FAKEBLOCKING
    /* callback for "non-callback" case in which we actually open portaudio
    in callback mode but fake "blocking mode". We communicate with the
    main thread via FIFO.  First read the audio output FIFO (which
    we sync on, not waiting for it but supplying zeros to the audio output if
    there aren't enough samples in the FIFO when we are called), then write
    to the audio input FIFO.  The main thread will wait for the input fifo.
    We can either throw it a pthreads condition or just allow the main thread
    to poll for us; so far polling seems to work better. */
static int pa_fifo_callback(const void *inputBuffer,
    void *outputBuffer, unsigned long nframes,
    const PaStreamCallbackTimeInfo *outTime, PaStreamCallbackFlags myflags,
    void *userData)
{
    /* callback routine for non-callback client... throw samples into
            and read them out of a FIFO */
    int ch;
    long fiforoom;
    float *fbuf;

#if CHECKFIFOS
    if (pa_inchans * sys_ringbuf_getreadavailable(&pa_outring) !=
        pa_outchans * sys_ringbuf_getwriteavailable(&pa_inring))
            post("warning: in and out rings unequal (%d, %d)",
                sys_ringbuf_getreadavailable(&pa_outring),
                 sys_ringbuf_getwriteavailable(&pa_inring));
#endif
    fiforoom = sys_ringbuf_getreadavailable(&pa_outring);
    if ((unsigned)fiforoom >= nframes*pa_outchans*sizeof(float))
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
    else
    {        /* PD could not keep up; generate zeros */
        if (pa_started)
            pa_dio_error = 1;
        if (outputBuffer)
        {
            for (ch = 0; ch < pa_outchans; ch++)
            {
                unsigned long frame;
                fbuf = ((float *)outputBuffer) + ch;
                for (frame = 0; frame < nframes; frame++, fbuf += pa_outchans)
                    *fbuf = 0;
            }
        }
    }
#ifdef THREADSIGNAL
    pthread_mutex_lock(&pa_mutex);
    pthread_cond_signal(&pa_sem);
    pthread_mutex_unlock(&pa_mutex);
#endif
    return 0;
}
#endif /* FAKEBLOCKING */

PaError pa_open_callback(double sampleRate, int inchannels, int outchannels,
    int framesperbuf, int nbuffers, int indeviceno, int outdeviceno, PaStreamCallback *callbackfn)
{
    long   bytesPerSample;
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

    outstreamparams.device = outdeviceno;
    outstreamparams.channelCount = outchannels;
    outstreamparams.sampleFormat = paFloat32;
    outstreamparams.hostApiSpecificStreamInfo = 0;

#ifdef FAKEBLOCKING
    instreamparams.suggestedLatency = outstreamparams.suggestedLatency = 0;
#else
    instreamparams.suggestedLatency = outstreamparams.suggestedLatency =
        nbuffers*framesperbuf/sampleRate;
#endif /* FAKEBLOCKING */

    if( inchannels>0 && indeviceno >= 0)
        p_instreamparams=&instreamparams;
    if( outchannels>0 && outdeviceno >= 0)
        p_outstreamparams=&outstreamparams;

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

    err=Pa_IsFormatSupported(p_instreamparams, p_outstreamparams, sampleRate);

    if (paFormatIsSupported != err)
    {
        /* check whether we have to change the numbers of channel and/or samplerate */
        const PaDeviceInfo* info = 0;
        double inRate=0, outRate=0;

        if (inchannels>0)
        {
            if (NULL != (info = Pa_GetDeviceInfo( instreamparams.device )))
            {
              inRate=info->defaultSampleRate;

              if(info->maxInputChannels<inchannels)
                instreamparams.channelCount=info->maxInputChannels;
            }
        }

        if (outchannels>0)
        {
            if (NULL != (info = Pa_GetDeviceInfo( outstreamparams.device )))
            {
              outRate=info->defaultSampleRate;

              if(info->maxOutputChannels<outchannels)
                outstreamparams.channelCount=info->maxOutputChannels;
            }
        }

        if (err == paInvalidSampleRate)
        {
            double oldrate = sampleRate;
            sampleRate = outRate > 0 ? outRate : inRate;
            logpost(0, PD_NORMAL,
                "warning: requested samplerate %d not supported, using %d.",
                    (int)oldrate, (int)sampleRate);
        }

        err=Pa_IsFormatSupported(p_instreamparams, p_outstreamparams,
            sampleRate);
        if (paFormatIsSupported != err)
        goto error;
    }
    err = Pa_OpenStream(
              &pa_stream,
              p_instreamparams,
              p_outstreamparams,
              sampleRate,
              framesperbuf,
              paNoFlag,      /* portaudio will clip for us */
              callbackfn,
              0);
    if (err != paNoError)
        goto error;

    err = Pa_StartStream(pa_stream);
    if (err != paNoError)
    {
        post("error opening failed; closing audio stream: %s",
            Pa_GetErrorText(err));
        pa_close_audio();
        goto error;
    }
    STUFF->st_dacsr=sampleRate;
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
    }

    if (inchans > 0 && pa_indev == -1)
        inchans = 0;
    if (outchans > 0 && pa_outdev == -1)
        outchans = 0;

    logpost(NULL, PD_VERBOSE, "input device %d, channels %d", pa_indev, inchans);
    logpost(NULL, PD_VERBOSE, "output device %d, channels %d", pa_outdev, outchans);
    logpost(NULL, PD_VERBOSE, "framesperbuf %d, nbufs %d", framesperbuf, nbuffers);
    logpost(NULL, PD_VERBOSE, "rate %d", rate);

    pa_inchans = STUFF->st_inchannels = inchans;
    pa_outchans = STUFF->st_outchannels = outchans;
    pa_soundin = soundin;
    pa_soundout = soundout;

#ifdef FAKEBLOCKING
    if (pa_inbuf)
        free((char *)pa_inbuf), pa_inbuf = 0;
    if (pa_outbuf)
        free((char *)pa_outbuf), pa_outbuf = 0;
#ifdef THREADSIGNAL
    pthread_mutex_init(&pa_mutex, 0);
    pthread_cond_init(&pa_sem, 0);
#endif
#endif

    if (! inchans && !outchans)
        return (0);

    if (callbackfn)
    {
        pa_callback = callbackfn;
        err = pa_open_callback(rate, inchans, outchans,
            framesperbuf, nbuffers, pa_indev, pa_outdev, pa_lowlevel_callback);
    }
    else
    {
#ifdef FAKEBLOCKING
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
            framesperbuf, nbuffers, pa_indev, pa_outdev, pa_fifo_callback);
#else
        err = pa_open_callback(rate, inchans, outchans,
            framesperbuf, nbuffers, pa_indev, pa_outdev, 0);
#endif
    }
    pa_started = 0;
    pa_nbuffers = nbuffers;
    if ( err != paNoError )
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
#ifdef FAKEBLOCKING
    if (pa_inbuf)
        free((char *)pa_inbuf), pa_inbuf = 0;
    if (pa_outbuf)
        free((char *)pa_outbuf), pa_outbuf = 0;
#ifdef THREADSIGNAL
    pthread_mutex_destroy(&pa_mutex);
    pthread_cond_destroy(&pa_sem);
#endif
#endif
}

int pa_send_dacs(void)
{
    t_sample *fp;
    float *fp2, *fp3;
    float *conversionbuf;
    int j, k;
    int rtnval =  SENDDACS_YES;
    int locked = 0;
    double timebefore;
#ifdef FAKEBLOCKING
#ifdef THREADSIGNAL
    struct timespec ts;
    double timeout;
#ifdef _WIN32
    struct __timeb64 tb;
    _ftime64(&tb);
    timeout = (double)tb.time + tb.millitm * 0.001;
#else
    struct timeval now;
    gettimeofday(&now, 0);
    timeout = (double)now.tv_sec + (1./1000000.) * now.tv_usec;
#endif
    timeout += THREADSIGNAL_TIMEOUT * 0.001;
    ts.tv_sec = (long long)timeout;
    ts.tv_nsec = (timeout - (double)ts.tv_sec) * 1e9;
#else
#endif /* THREADSIGNAL */
#endif /* FAKEBLOCKING */
    timebefore = sys_getrealtime();

    if ((!STUFF->st_inchannels && !STUFF->st_outchannels) || !pa_stream)
        return (SENDDACS_NO);
    conversionbuf = (float *)alloca((STUFF->st_inchannels > STUFF->st_outchannels?
        STUFF->st_inchannels:STUFF->st_outchannels) * DEFDACBLKSIZE * sizeof(float));

#ifdef FAKEBLOCKING
    if (!STUFF->st_inchannels)    /* if no input channels sync on output */
    {
#ifdef THREADSIGNAL
        pthread_mutex_lock(&pa_mutex);
#endif
        while (sys_ringbuf_getwriteavailable(&pa_outring) <
            (long)(STUFF->st_outchannels * DEFDACBLKSIZE * sizeof(float)))
        {
            rtnval = SENDDACS_SLEPT;
#ifdef THREADSIGNAL
            if (pthread_cond_timedwait(&pa_sem, &pa_mutex, &ts) == ETIMEDOUT)
            {
                locked = 1;
                break;
            }
#else
            if (Pa_IsStreamActive(&pa_stream) < 0 &&
                (sys_getrealtime() - timebefore) > (POLL_TIMEOUT * 0.001))
            {
                locked = 1;
                break;
            }
            sys_microsleep();
            if (!pa_stream)     /* sys_microsleep() may have closed device */
                return SENDDACS_NO;
#endif /* THREADSIGNAL */
        }
#ifdef THREADSIGNAL
        pthread_mutex_unlock(&pa_mutex);
#endif
    }
        /* write output */
    if (STUFF->st_outchannels && !locked)
    {
        for (j = 0, fp = STUFF->st_soundout, fp2 = conversionbuf;
            j < STUFF->st_outchannels; j++, fp2++)
                for (k = 0, fp3 = fp2; k < DEFDACBLKSIZE;
                    k++, fp++, fp3 += STUFF->st_outchannels)
                        *fp3 = *fp;
        sys_ringbuf_write(&pa_outring, conversionbuf,
            STUFF->st_outchannels*(DEFDACBLKSIZE*sizeof(float)), pa_outbuf);
    }
    if (STUFF->st_inchannels)    /* if there is input sync on it */
    {
#ifdef THREADSIGNAL
        pthread_mutex_lock(&pa_mutex);
#endif
        while (sys_ringbuf_getreadavailable(&pa_inring) <
            (long)(STUFF->st_inchannels * DEFDACBLKSIZE * sizeof(float)))
        {
            rtnval = SENDDACS_SLEPT;
#ifdef THREADSIGNAL
            if (pthread_cond_timedwait(&pa_sem, &pa_mutex, &ts) == ETIMEDOUT)
            {
                locked = 1;
                break;
            }
#else
            if (Pa_IsStreamActive(&pa_stream) < 0 &&
                (sys_getrealtime() - timebefore) > (POLL_TIMEOUT * 0.001))
            {
                locked = 1;
                break;
            }
            sys_microsleep();
            if (!pa_stream)     /* sys_microsleep() may have closed device */
                return SENDDACS_NO;
#endif /* THREADSIGNAL */
        }
#ifdef THREADSIGNAL
        pthread_mutex_unlock(&pa_mutex);
#endif
    }
    if (STUFF->st_inchannels && !locked)
    {
        sys_ringbuf_read(&pa_inring, conversionbuf,
            STUFF->st_inchannels*(DEFDACBLKSIZE*sizeof(float)), pa_inbuf);
        for (j = 0, fp = STUFF->st_soundin, fp2 = conversionbuf;
            j < STUFF->st_inchannels; j++, fp2++)
                for (k = 0, fp3 = fp2; k < DEFDACBLKSIZE;
                    k++, fp++, fp3 += STUFF->st_inchannels)
                        *fp = *fp3;
    }

#else /* FAKEBLOCKING */
        /* write output */
    if (STUFF->st_outchannels)
    {
        if (!pa_started)
        {
            memset(conversionbuf, 0,
                STUFF->st_outchannels * DEFDACBLKSIZE * sizeof(float));
            for (j = 0; j < pa_nbuffers-1; j++)
                Pa_WriteStream(pa_stream, conversionbuf, DEFDACBLKSIZE);
        }
        for (j = 0, fp = STUFF->st_soundout, fp2 = conversionbuf;
            j < STUFF->st_outchannels; j++, fp2++)
                for (k = 0, fp3 = fp2; k < DEFDACBLKSIZE;
                    k++, fp++, fp3 += STUFF->st_outchannels)
                        *fp3 = *fp;
        if (Pa_WriteStream(pa_stream, conversionbuf, DEFDACBLKSIZE) != paNoError)
            if (Pa_IsStreamActive(&pa_stream) < 0)
                locked = 1;
    }

    if (STUFF->st_inchannels)
    {
        if (Pa_ReadStream(pa_stream, conversionbuf, DEFDACBLKSIZE) != paNoError)
            if (Pa_IsStreamActive(&pa_stream) < 0)
                locked = 1;
        for (j = 0, fp = STUFF->st_soundin, fp2 = conversionbuf;
            j < STUFF->st_inchannels; j++, fp2++)
                for (k = 0, fp3 = fp2; k < DEFDACBLKSIZE;
                    k++, fp++, fp3 += STUFF->st_inchannels)
                        *fp = *fp3;
    }
    if (sys_getrealtime() - timebefore > 0.002)
    {
        rtnval = SENDDACS_SLEPT;
    }
#endif /* FAKEBLOCKING */
    pa_started = 1;

    memset(STUFF->st_soundout, 0,
        DEFDACBLKSIZE*sizeof(t_sample)*STUFF->st_outchannels);
    if (locked)
    {
        PaError err = Pa_IsStreamActive(&pa_stream);
        pd_error(0, "error %d: %s", err, Pa_GetErrorText(err));
        sys_close_audio();
        #ifdef __APPLE__
            /* TODO: the portaudio coreaudio implementation doesn't handle
               re-connection, so suggest restarting */
            pd_error(0, "audio device not responding - closing audio");
            pd_error(0, "you may need to save and restart pd");
        #else
            /* portaudio seems to handle this better on windows and linux */
            pd_error(0, "trying to reopen audio device");
            sys_reopen_audio(); /* try to reopen it */
            if (audio_isopen())
                pd_error(0, "successfully reopened audio device");
            else
            {
                pd_error(0, "audio device not responding - closing audio");
                pd_error(0, "reconnect and reselect it in the settings (or toggle DSP)");
            }
        #endif
        return SENDDACS_NO;
    }
    else return (rtnval);
}

static char*pdi2devname(const PaDeviceInfo*pdi, char*buf, size_t bufsize) {
    const PaHostApiInfo *api = 0;
    char utf8device[MAXPDSTRING];
    utf8device[0] = 0;

    if(!pdi)
        return 0;

    api = Pa_GetHostApiInfo(pdi->hostApi);
    if(api)
        snprintf(utf8device, MAXPDSTRING, "%s: %s",
            api->name, pdi->name);
    else
        snprintf(utf8device, MAXPDSTRING, "%s",
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
            snprintf(indevlist + nin * devdescsize, devdescsize,
                "%s", devname);
            nin++;
        }
        if (pdi->maxOutputChannels > 0 && nout < maxndev)
        {
            snprintf(outdevlist + nout * devdescsize, devdescsize,
                "%s", devname);
            nout++;
        }
    }
    *nindevs = nin;
    *noutdevs = nout;
}
