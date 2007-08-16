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

    /* LATER try to figure out how to handle default devices in portaudio;
    the way s_audio.c handles them isn't going to work here. */

    /* public interface declared in m_imp.h */

    /* implementation */
static PABLIO_Stream  *pa_stream;
static int pa_inchans, pa_outchans;
static float *pa_soundin, *pa_soundout;
static t_audiocallback pa_callback;

#define MAX_PA_CHANS 32
#define MAX_SAMPLES_PER_FRAME MAX_PA_CHANS * DEFDACBLKSIZE

static int pa_lowlevel_callback(const void *inputBuffer,
    void *outputBuffer, unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo *outTime, PaStreamCallbackFlags myflags, 
    void *userData)
{
    int i; 
    unsigned int j;
    float *fbuf, *fp2, *fp3, *soundiop;
        if (framesPerBuffer != DEFDACBLKSIZE)
        {
                fprintf(stderr, "ignoring buffer size %d\n", framesPerBuffer);
                return;
    }
        if (inputBuffer != NULL)
    {
        fbuf = (float *)inputBuffer;
        soundiop = pa_soundin;
        for (i = 0, fp2 = fbuf; i < pa_inchans; i++, fp2++)
            for (j = 0, fp3 = fp2; j < framesPerBuffer; j++, fp3 += pa_inchans)
                *soundiop++ = *fp3;
    }
    else memset((void *)pa_soundin, 0,
        framesPerBuffer * pa_inchans * sizeof(float));
    (*pa_callback)();
    if (outputBuffer != NULL)
    {
        fbuf = (float *)outputBuffer;
        soundiop = pa_soundout;
        for (i = 0, fp2 = fbuf; i < pa_outchans; i++, fp2++)
            for (j = 0, fp3 = fp2; j < framesPerBuffer; j++, fp3 += pa_outchans)
                *fp3 = *soundiop++;
    }

    return 0;
}

PaError pa_open_callback(double sampleRate, int inchannels, int outchannels,
    int framesperbuf, int nbuffers, int indeviceno, int outdeviceno)
{
    long   bytesPerSample;
    PaError err;
    PABLIO_Stream *pastream;
    long   numFrames;
    PaStreamParameters instreamparams, outstreamparams;

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

    /* Allocate PABLIO_Stream structure for caller. */
    pastream = (PABLIO_Stream *)malloc( sizeof(PABLIO_Stream));
    if (pastream == NULL)
        return (1);
    memset(pastream, 0, sizeof(PABLIO_Stream));

    /* Determine size of a sample. */
    bytesPerSample = Pa_GetSampleSize(paFloat32);
    if (bytesPerSample < 0)
    {
        err = (PaError) bytesPerSample;
        goto error;
    }
    pastream->insamplesPerFrame = inchannels;
    pastream->inbytesPerFrame = bytesPerSample * pastream->insamplesPerFrame;
    pastream->outsamplesPerFrame = outchannels;
    pastream->outbytesPerFrame = bytesPerSample * pastream->outsamplesPerFrame;

    numFrames = nbuffers * framesperbuf;

    instreamparams.device = indeviceno;
    instreamparams.channelCount = inchannels;
    instreamparams.sampleFormat = paFloat32;
    instreamparams.suggestedLatency = nbuffers*framesperbuf/sampleRate;
    instreamparams.hostApiSpecificStreamInfo = 0;
    
    outstreamparams.device = outdeviceno;
    outstreamparams.channelCount = outchannels;
    outstreamparams.sampleFormat = paFloat32;
    outstreamparams.suggestedLatency = nbuffers*framesperbuf/sampleRate;
    outstreamparams.hostApiSpecificStreamInfo = 0;

    err = Pa_OpenStream(
              &pastream->stream,
              (inchannels ? &instreamparams : 0),
              (outchannels ? &outstreamparams : 0),
              sampleRate,
              DEFDACBLKSIZE,
              paNoFlag,      /* portaudio will clip for us */
              pa_lowlevel_callback,
              pastream);
    if (err != paNoError)
        goto error;

    err = Pa_StartStream(pastream->stream);
    if (err != paNoError)
    {
        fprintf(stderr, "Pa_StartStream failed; closing audio stream...\n");
        CloseAudioStream( pastream );
        goto error;
    }
    pa_stream = pastream;
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
    static int initialized;
    int j, devno, pa_indev = 0, pa_outdev = 0;
    
    pa_callback = callbackfn;
    if (callbackfn)
        fprintf(stderr, "callback enabled\n");
    if (!initialized)
    {
        /* Initialize PortAudio  */
        int err = Pa_Initialize();
        if ( err != paNoError ) 
        {
            fprintf( stderr,
                "Error number %d occured initializing portaudio\n",
                err); 
            fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
            return (1);
        }
        initialized = 1;
    }
    /* post("in %d out %d rate %d device %d", inchans, outchans, rate, deviceno); */
    if (inchans > MAX_PA_CHANS)
    {
        post("input channels reduced to maximum %d", MAX_PA_CHANS);
        inchans = MAX_PA_CHANS;
    }
    if (outchans > MAX_PA_CHANS)
    {
        post("output channels reduced to maximum %d", MAX_PA_CHANS);
        outchans = MAX_PA_CHANS;
    }
    
    if (inchans > 0)
    {
        for (j = 0, devno = 0; j < Pa_GetDeviceCount(); j++)
        {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(j);
            if (info->maxInputChannels > 0)
            {
                if (devno == indeviceno)
                {
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
    if (pa_inchans || pa_outchans)
        CloseAudioStream( pa_stream );
    pa_inchans = pa_outchans = 0;
}

int pa_send_dacs(void)
{
    float samples[MAX_SAMPLES_PER_FRAME], *fp1, *fp2;
    int i, j;
    double timebefore;
    
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
    Pa_Initialize();
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

    Pa_Initialize();
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
