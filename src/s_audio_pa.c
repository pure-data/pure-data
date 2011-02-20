/* Copyright (c) 2001 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* this file calls Ross Bencina's and Phil Burk's Portaudio package.  It's
    the main way in for Mac OS and, with Michael Casey's help, also into
    ASIO in Windows. */


#include "m_pd.h"
#include "s_stuff.h"
#include <stdio.h>
#include <stdlib.h>
#include <portaudio.h>
#include "s_audio_pablio.h"
#ifdef MSW
#include <malloc.h>
#else
#include <alloca.h>
#endif

    /* LATER try to figure out how to handle default devices in portaudio;
    the way s_audio.c handles them isn't going to work here. */

    /* public interface declared in m_imp.h */

    /* implementation */
static PABLIO_Stream  *pa_stream;
static PaStream *pa_callbackstream;
static int pa_inchans, pa_outchans;
static float *pa_soundin, *pa_soundout;
static t_audiocallback pa_callback;

int pa_foo;

#ifndef MSW
#include <unistd.h>
#endif
static void pa_init(void)
{
    static int initialized;
    if (!initialized)
    {
#ifndef MSW
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
    if (pa_foo)
       fprintf(stderr, "pa_lowlevel_callback\n");
    if (nframes % DEFDACBLKSIZE)
    {
        fprintf(stderr, "jack: nframes %ld not a multiple of blocksize %d\n",
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
    if (pa_foo)
        fprintf(stderr, "done pa_lowlevel_callback\n"); 
    return 0;
}

PaError pa_open_callback(double sampleRate, int inchannels, int outchannels,
    int framesperbuf, int nbuffers, int indeviceno, int outdeviceno)
{
    long   bytesPerSample;
    PaError err;
    PaStreamParameters instreamparams, outstreamparams;
    PaStreamParameters*p_instreamparams=0, *p_outstreamparams=0;

    if (indeviceno < 0) 
    {
        indeviceno = Pa_GetDefaultInputDevice();
        fprintf(stderr, "using default input device number: %d\n", indeviceno);
    }
    if (outdeviceno < 0)
    {
        outdeviceno = Pa_GetDefaultOutputDevice();
        fprintf(stderr, "using default output device number: %d\n", outdeviceno);
    }
    /* fprintf(stderr, "nchan %d, flags %d, bufs %d, framesperbuf %d\n",
            nchannels, flags, nbuffers, framesperbuf); */

    instreamparams.device = indeviceno;   /* MSP... */
    instreamparams.channelCount = inchannels;
    instreamparams.sampleFormat = paFloat32;
    instreamparams.suggestedLatency = nbuffers*framesperbuf/sampleRate;
    instreamparams.hostApiSpecificStreamInfo = 0;
    
    outstreamparams.device = outdeviceno;
    outstreamparams.channelCount = outchannels;
    outstreamparams.sampleFormat = paFloat32;
    outstreamparams.suggestedLatency = nbuffers*framesperbuf/sampleRate;
    outstreamparams.hostApiSpecificStreamInfo = 0;  /* ... MSP */

    if(inchannels>0)
        p_instreamparams=&instreamparams;
    if(outchannels>0)
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
              &pa_callbackstream,
              p_instreamparams,
              p_outstreamparams,
              sampleRate,
              framesperbuf,
              paNoFlag,      /* portaudio will clip for us */
              pa_lowlevel_callback,
              0);
    if (err != paNoError)
        goto error;

    err = Pa_StartStream(pa_callbackstream);
    if (err != paNoError)
    {
        fprintf(stderr, "Pa_StartStream failed; closing audio stream...\n");
        CloseAudioStream(pa_callbackstream);
        goto error;
    }
    sys_dacsr=sampleRate;
    return paNoError;
error:
    pa_callbackstream = NULL;
    return err;
}

int pa_open_audio(int inchans, int outchans, int rate, t_sample *soundin,
    t_sample *soundout, int framesperbuf, int nbuffers,
    int indeviceno, int outdeviceno, t_audiocallback callbackfn)
{
    PaError err;
    int j, devno, pa_indev = 0, pa_outdev = 0;
    
    pa_callback = callbackfn;
    /* fprintf(stderr, "open callback %d\n", (callbackfn != 0)); */
    pa_init();
    /* post("in %d out %d rate %d device %d", inchans, outchans, rate, deviceno); */
    
    if (pa_stream || pa_callbackstream)
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

    if (sys_verbose)
    {
        post("input device %d, channels %d", pa_indev, inchans);
        post("output device %d, channels %d", pa_outdev, outchans);
        post("framesperbuf %d, nbufs %d", framesperbuf, nbuffers);
    }
    pa_inchans = inchans;
    pa_outchans = outchans;
    pa_soundin = soundin;
    pa_soundout = soundout;
    if (! inchans && !outchans)
        return(0);
    if (callbackfn)
    {
        pa_callback = callbackfn;
        err = pa_open_callback(rate, inchans, outchans,
            framesperbuf, nbuffers, pa_indev, pa_outdev);
    }
    else
    {
        err = OpenAudioStream( &pa_stream, rate, paFloat32,
            inchans, outchans, framesperbuf, nbuffers,
                pa_indev, pa_outdev);
        if ( err == paInvalidSampleRate ) 
        {
          /* try again with the default samplerate... */
          const PaDeviceInfo* info = 0;
          int devno=pa_outdev;
          if(devno<0)
            devno=pa_indev;
          if(devno<0)
            devno=Pa_GetDefaultOutputDevice();

          info=Pa_GetDeviceInfo( devno );
          if(info)
            rate=info->defaultSampleRate;

          err = OpenAudioStream( &pa_stream, rate, paFloat32,
                                 inchans, outchans, framesperbuf, nbuffers,
                                 pa_indev, pa_outdev);

          if ( paNoError == err ) 
            sys_dacsr=rate;

        }
    }
    if ( err != paNoError ) 
    {
        fprintf(stderr, "Error number %d opening portaudio stream\n",
            err); 
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        Pa_Terminate();
        return (1);
    }
    else if (sys_verbose)
        post("... opened OK.");
    return (0);
}

void pa_close_audio( void)
{
    /* fprintf(stderr, "close\n"); */
    if (pa_stream)
        CloseAudioStream( pa_stream );
    pa_stream = 0;
    if (pa_callbackstream) {
      //   CloseAudioStream(pa_callbackstream);
      if(paNoError == Pa_StopStream( pa_callbackstream ))
        Pa_CloseStream( pa_callbackstream );
    }
    pa_callbackstream = 0;
    
}

int pa_send_dacs(void)
{
    unsigned int framesize = (sizeof(float) * DEFDACBLKSIZE) *
        (pa_inchans > pa_outchans ? pa_inchans:pa_outchans);
    float *samples, *fp1, *fp2;
    int i, j;
    double timebefore;
    
    
    samples = alloca(framesize);
    
    timebefore = sys_getrealtime();
    if ((pa_inchans && GetAudioStreamReadable(pa_stream) < DEFDACBLKSIZE) ||
        (pa_outchans && GetAudioStreamWriteable(pa_stream) < DEFDACBLKSIZE))
    {
        if (pa_inchans && pa_outchans)
        {
            int synced = 0;
            while (GetAudioStreamWriteable(pa_stream) > 2*DEFDACBLKSIZE)
            {
                for (j = 0; j < pa_outchans; j++)
                    for (i = 0, fp2 = samples + j; i < DEFDACBLKSIZE; i++,
                        fp2 += pa_outchans)
                {
                    *fp2 = 0;
                }
                synced = 1;
                WriteAudioStream(pa_stream, samples, DEFDACBLKSIZE);
            }
            while (GetAudioStreamReadable(pa_stream) > 2*DEFDACBLKSIZE)
            {
                synced = 1;
                ReadAudioStream(pa_stream, samples, DEFDACBLKSIZE);
            }
            /* if (synced)
                post("sync"); */
        }
        return (SENDDACS_NO);
    }
    if (pa_inchans)
    {
        ReadAudioStream(pa_stream, samples, DEFDACBLKSIZE);
        for (j = 0, fp1 = pa_soundin; j < pa_inchans; j++, fp1 += DEFDACBLKSIZE)
            for (i = 0, fp2 = samples + j; i < DEFDACBLKSIZE; i++,
                fp2 += pa_inchans)
        {
            fp1[i] = *fp2;
        }
    }
#if 0 
        {
                static int nread;
                if (nread == 0)
                {
                        post("it's %f %f %f %f",
                        pa_soundin[0], pa_soundin[1], pa_soundin[2],                                    pa_soundin[3]);
                        nread = 1000;
                }
                nread--;
        }
#endif
    if (pa_outchans)
    {
        for (j = 0, fp1 = pa_soundout; j < pa_outchans; j++,
            fp1 += DEFDACBLKSIZE)
                for (i = 0, fp2 = samples + j; i < DEFDACBLKSIZE; i++,
                    fp2 += pa_outchans)
        {
            *fp2 = fp1[i];
            fp1[i] = 0;
        }
        WriteAudioStream(pa_stream, samples, DEFDACBLKSIZE);
    }

    if (sys_getrealtime() > timebefore + 0.002)
    {
        /* post("slept"); */
        return (SENDDACS_SLEPT);
    }
    else return (SENDDACS_YES);
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
