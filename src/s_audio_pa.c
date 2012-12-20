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
    works better in MAXOSX (gets 40 msec lower latency!) and might also in
    Windows.  If FAKEBLOCKING is defined we can choose between two methods
    for waiting on the (presumebly other-thread) I/O to complete, either
    correct thread synchronization (by defining THREADSIGNAL) or just sleeping
    and polling; the latter seems to work better so far.
*/

/* dolist...  
    switch to usleep in s_inter.c
*/

#include "m_pd.h"
#include "s_stuff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>

#ifndef _WIN32          /* for the "dup2" workaround -- do we still need it? */
#include <unistd.h>
#endif

#ifdef HAVE_ALLOCA_H        /* ifdef nonsense to find include for alloca() */
# include <alloca.h>        /* linux, mac, mingw, cygwin */
#elif defined _MSC_VER
# include <malloc.h>        /* MSVC */
#else
# include <stddef.h>        /* BSDs for example */
#endif                      /* end alloca() ifdef nonsense */


#if defined(__APPLE__)
#define FAKEBLOCKING
#endif

/* define this to enable thread signaling instead of polling */
/* #define THREADSIGNAL */

    /* LATER try to figure out how to handle default devices in portaudio;
    the way s_audio.c handles them isn't going to work here. */

    /* public interface declared in m_imp.h */

    /* implementation */
static PaStream *pa_stream;
static int pa_inchans, pa_outchans;
static float *pa_soundin, *pa_soundout;
static t_audiocallback pa_callback;

static int pa_started;
static int pa_nbuffers;
static int pa_dio_error;

#ifdef FAKEBLOCKING
#include "s_audio_paring.h"
static float *pa_outbuf;
static sys_ringbuf pa_outring;
static float *pa_inbuf;
static sys_ringbuf pa_inring;
#ifdef THREADSIGNAL
#include <pthread.h>
pthread_mutex_t pa_mutex;
pthread_cond_t pa_sem;
#endif /* THREADSIGNAL */
#endif  /* FAKEBLOCKING */


static void pa_init(void)
{
    static int initialized;
    if (!initialized)
    {
#ifndef _WIN32
        /* Initialize PortAudio  */
        /* for some reason Pa_Initialize(0 closes file descriptor 1.
        As a workaround, dup it to another number and dup2 it back
        afterward. */
        int newfd = dup(1);
        int err = Pa_Initialize();
        if (newfd >= 0)
        {
            dup2(newfd, 1);
            close(newfd);
        }
#else
        int err = Pa_Initialize();
#endif
        if ( err != paNoError ) 
        {
            fprintf( stderr,
                "Error number %d occured initializing portaudio\n",
                err); 
            fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
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
    float *fbuf, *fp2, *fp3, *soundiop;
    if (nframes % DEFDACBLKSIZE)
    {
        fprintf(stderr, "portaudio: nframes %ld not a multiple of blocksize %d\n",
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
                            *soundiop++ = *fp3;
        }
        else memset((void *)pa_soundin, 0,
            DEFDACBLKSIZE * pa_inchans * sizeof(float));
        memset((void *)pa_soundout, 0,
            DEFDACBLKSIZE * pa_outchans * sizeof(float));
        (*pa_callback)();
        if (outputBuffer != NULL)
        {
            fbuf = ((float *)outputBuffer) + n*pa_outchans;
            soundiop = pa_soundout;
            for (i = 0, fp2 = fbuf; i < pa_outchans; i++, fp2++)
                for (j = 0, fp3 = fp2; j < DEFDACBLKSIZE;
                    j++, fp3 += pa_outchans)
                        *fp3 = *soundiop++;
        }
    }
    return 0;
}

#ifdef FAKEBLOCKING
    /* callback for "non-callback" case in which we actualy open portaudio
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
    if (pa_inchans * sys_ringbuf_GetReadAvailable(&pa_outring) !=   
        pa_outchans * sys_ringbuf_GetWriteAvailable(&pa_inring))
            fprintf(stderr, "warning: in and out rings unequal (%d, %d)\n",
                sys_ringbuf_GetReadAvailable(&pa_outring),
                 sys_ringbuf_GetWriteAvailable(&pa_inring));
#endif
    fiforoom = sys_ringbuf_GetReadAvailable(&pa_outring);
    if ((unsigned)fiforoom >= nframes*pa_outchans*sizeof(float))
    {
        if (outputBuffer)
            sys_ringbuf_Read(&pa_outring, outputBuffer,
                nframes*pa_outchans*sizeof(float));
        else if (pa_outchans)
            fprintf(stderr, "no outputBuffer but output channels\n");
        if (inputBuffer)
            sys_ringbuf_Write(&pa_inring, inputBuffer,
                nframes*pa_inchans*sizeof(float));
        else if (pa_inchans)
            fprintf(stderr, "no inputBuffer but input channels\n");
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

    /* fprintf(stderr, "nchan %d, flags %d, bufs %d, framesperbuf %d\n",
            nchannels, flags, nbuffers, framesperbuf); */

    instreamparams.device = indeviceno;
    instreamparams.channelCount = inchannels;
    instreamparams.sampleFormat = paFloat32;
    instreamparams.suggestedLatency = nbuffers*framesperbuf/sampleRate;
    instreamparams.hostApiSpecificStreamInfo = 0;
    
    outstreamparams.device = outdeviceno;
    outstreamparams.channelCount = outchannels;
    outstreamparams.sampleFormat = paFloat32;
    outstreamparams.suggestedLatency = nbuffers*framesperbuf/sampleRate;;
    outstreamparams.hostApiSpecificStreamInfo = 0;

    if( inchannels>0 && indeviceno >= 0)
        p_instreamparams=&instreamparams;
    if( outchannels>0 && outdeviceno >= 0)
        p_outstreamparams=&outstreamparams;

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
            sampleRate=outRate;
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
        fprintf(stderr, "Pa_StartStream failed; closing audio stream...\n");
        pa_close_audio();
        goto error;
    }
    sys_dacsr=sampleRate;
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
    
    if (sys_verbose)
    {
        post("input device %d, channels %d", pa_indev, inchans);
        post("output device %d, channels %d", pa_outdev, outchans);
        post("framesperbuf %d, nbufs %d", framesperbuf, nbuffers);
        post("rate %d", rate);
    }
    pa_inchans = sys_inchannels = inchans;
    pa_outchans = sys_outchannels = outchans;
    pa_soundin = soundin;
    pa_soundout = soundout;

#ifdef FAKEBLOCKING
    if (pa_inbuf)
        free(pa_inbuf), pa_inbuf = 0;
    if (pa_outbuf)
        free(pa_outbuf), pa_outbuf = 0;
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
            sys_ringbuf_Init(&pa_inring,
                nbuffers*framesperbuf*pa_inchans*sizeof(float), pa_inbuf,
                    nbuffers*framesperbuf*pa_inchans*sizeof(float));
        }
        if (pa_outchans)
        {
            pa_outbuf = malloc(nbuffers*framesperbuf*pa_outchans*sizeof(float));
            sys_ringbuf_Init(&pa_outring,
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
        fprintf(stderr, "Error number %d opening portaudio stream\n",
            err); 
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        /* Pa_Terminate(); */
        return (1);
    }
    else if (sys_verbose)
        post("... opened OK.");
    return (0);
}

void pa_close_audio( void)
{
    if (pa_stream)
    {
        Pa_AbortStream(pa_stream);
        Pa_CloseStream(pa_stream);
    }
    pa_stream = 0;
#ifdef FAKEBLOCKING
    if (pa_inbuf)
        free(pa_inbuf), pa_inbuf = 0;
    if (pa_outbuf)
        free(pa_outbuf), pa_outbuf = 0;
#endif
}

int pa_send_dacs(void)
{
    t_sample *fp;
    float *fp2, *fp3;
    float *conversionbuf;
    int j, k;
    int rtnval =  SENDDACS_YES;
#ifndef FAKEBLOCKING
    double timebefore;
#endif /* FAKEBLOCKING */
    if (!sys_inchannels && !sys_outchannels || !pa_stream)
        return (SENDDACS_NO); 
    conversionbuf = (float *)alloca((sys_inchannels > sys_outchannels?
        sys_inchannels:sys_outchannels) * DEFDACBLKSIZE * sizeof(float));

#ifdef FAKEBLOCKING
    if (!sys_inchannels)    /* if no input channels sync on output */
    {
#ifdef THREADSIGNAL
        pthread_mutex_lock(&pa_mutex);
#endif
        while (sys_ringbuf_GetWriteAvailable(&pa_outring) <
            (long)(sys_outchannels * DEFDACBLKSIZE * sizeof(float)))
        {
            rtnval = SENDDACS_SLEPT;
#ifdef THREADSIGNAL
            pthread_cond_wait(&pa_sem, &pa_mutex);
#else
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif /* _WIN32 */
#endif /* THREADSIGNAL */
        }
#ifdef THREADSIGNAL
        pthread_mutex_unlock(&pa_mutex);
#endif
    }
        /* write output */
    if (sys_outchannels)
    {
        for (j = 0, fp = sys_soundout, fp2 = conversionbuf;
            j < sys_outchannels; j++, fp2++)
                for (k = 0, fp3 = fp2; k < DEFDACBLKSIZE;
                    k++, fp++, fp3 += sys_outchannels)
                        *fp3 = *fp;
        sys_ringbuf_Write(&pa_outring, conversionbuf,
            sys_outchannels*(DEFDACBLKSIZE*sizeof(float)));
    }
    if (sys_inchannels)    /* if there is input sync on it */
    {
#ifdef THREADSIGNAL
        pthread_mutex_lock(&pa_mutex);
#endif
        while (sys_ringbuf_GetReadAvailable(&pa_inring) <
            (long)(sys_inchannels * DEFDACBLKSIZE * sizeof(float)))
        {
            rtnval = SENDDACS_SLEPT;
#ifdef THREADSIGNAL
            pthread_cond_wait(&pa_sem, &pa_mutex);
#else
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif /* _WIN32 */
#endif /* THREADSIGNAL */
        }
#ifdef THREADSIGNAL
        pthread_mutex_unlock(&pa_mutex);
#endif
    }
    if (sys_inchannels)
    {
        sys_ringbuf_Read(&pa_inring, conversionbuf,
            sys_inchannels*(DEFDACBLKSIZE*sizeof(float)));
        for (j = 0, fp = sys_soundin, fp2 = conversionbuf;
            j < sys_inchannels; j++, fp2++)
                for (k = 0, fp3 = fp2; k < DEFDACBLKSIZE;
                    k++, fp++, fp3 += sys_inchannels)
                        *fp = *fp3;
    }

#else /* FAKEBLOCKING */
    timebefore = sys_getrealtime();
        /* write output */
    if (sys_outchannels)
    {
        if (!pa_started)
        {
            memset(conversionbuf, 0,
                sys_outchannels * DEFDACBLKSIZE * sizeof(float));
            for (j = 0; j < pa_nbuffers-1; j++)
                Pa_WriteStream(pa_stream, conversionbuf, DEFDACBLKSIZE);
        }
        for (j = 0, fp = sys_soundout, fp2 = conversionbuf;
            j < sys_outchannels; j++, fp2++)
                for (k = 0, fp3 = fp2; k < DEFDACBLKSIZE;
                    k++, fp++, fp3 += sys_outchannels)
                        *fp3 = *fp;
        Pa_WriteStream(pa_stream, conversionbuf, DEFDACBLKSIZE);
    }

    if (sys_inchannels)
    {
        Pa_ReadStream(pa_stream, conversionbuf, DEFDACBLKSIZE);
        for (j = 0, fp = sys_soundin, fp2 = conversionbuf;
            j < sys_inchannels; j++, fp2++)
                for (k = 0, fp3 = fp2; k < DEFDACBLKSIZE;
                    k++, fp++, fp3 += sys_inchannels)
                        *fp = *fp3;
    }
    if (sys_getrealtime() - timebefore > 0.002)
    {
        rtnval = SENDDACS_SLEPT;
    }
#endif /* FAKEBLOCKING */
    pa_started = 1;

    memset(sys_soundout, 0, DEFDACBLKSIZE*sizeof(t_sample)*sys_outchannels);
    return (rtnval);
}

void pa_listdevs(void)     /* lifted from pa_devs.c in portaudio */
{
    int      i,j;
    int      numDevices;
    const    PaDeviceInfo *pdi;
    PaError  err;
    pa_init();
    numDevices = Pa_GetDeviceCount();
    if( numDevices < 0 )
    {
        fprintf(stderr, "ERROR: Pa_GetDeviceCount returned %d\n", numDevices );
        err = numDevices;
        goto error;
    }
    fprintf(stderr, "Audio Devices:\n");
    for( i=0; i<numDevices; i++ )
    {
        pdi = Pa_GetDeviceInfo( i );
        fprintf(stderr, "device %d:", i+1 );
        fprintf(stderr, " %s;", pdi->name );
        fprintf(stderr, "%d inputs, ", pdi->maxInputChannels  );
        fprintf(stderr, "%d outputs", pdi->maxOutputChannels  );
        if ( i == Pa_GetDefaultInputDevice() )
            fprintf(stderr, " (Default Input)");
        if ( i == Pa_GetDefaultOutputDevice() )
            fprintf(stderr, " (Default Output)");
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "\n");
    return;

error:
    fprintf( stderr, "An error occured while using the portaudio stream\n" ); 
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );

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
        const PaDeviceInfo *pdi = Pa_GetDeviceInfo(i);
        if (pdi->maxInputChannels > 0 && nin < maxndev)
        {
            sprintf(indevlist + nin * devdescsize, "(%d)%s",
                pdi->hostApi,pdi->name);
            /* strcpy(indevlist + nin * devdescsize, pdi->name); */
            nin++;
        }
        if (pdi->maxOutputChannels > 0 && nout < maxndev)
        {
            sprintf(outdevlist + nout * devdescsize, "(%d)%s",
                pdi->hostApi,pdi->name);
            /* strcpy(outdevlist + nout * devdescsize, pdi->name); */
            nout++;
        }
    }
    *nindevs = nin;
    *noutdevs = nout;
}
