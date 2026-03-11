/* Copyright (c) 2003, Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  machine-independent (well, mostly!) audio layer.  Stores and recalls
    audio settings from argparse routine and from dialog window.

    LATER: save audio settings for various APIs for easier switching

*/

#include "m_pd.h"
#include "s_stuff.h"
#include <stdio.h>
#ifdef _WIN32
#include <time.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif /* _WIN32 */
#include <string.h>
#include <math.h>

#include "m_private_utils.h"

#define SYS_DEFAULTCH 2
#define MAXNDEV 128
#define DEVDESCSIZE 128
#define MAXBLOCKSIZE 2048

    /* exported variables */
int sys_schedadvance;   /* scheduler advance in microseconds */

static int sys_audioapiopened; /* what API is open, API_NONE if none */

    /* current parameters (if an API is open) or requested ones otherwise: */
static t_audiosettings audio_nextsettings;

void sched_audio_callbackfn(void);

int audio_isopen(void)
{
    return (sys_audioapiopened > 0);
}

static int audio_isfixedsr(int api)
{
#ifdef USEAPI_JACK
    /* JACK server sets it's own samplerate */
    return (api == API_JACK);
#endif
    return 0;
}

static int audio_isfixedblocksize(int api)
{
#ifdef USEAPI_JACK
    /* JACK server sets it's own blocksize */
    return (api == API_JACK);
#endif
    return 0;
}

#ifdef USEAPI_JACK
int jack_get_blocksize(void);
#endif

static int audio_getfixedblocksize(int api)
{
#ifdef USEAPI_JACK
    /* JACK server sets it's own blocksize */
    return (api == API_JACK ? jack_get_blocksize() : 0);
#endif
    return 0;
}

    /* inform rest of Pd of current channels and sample rate.  Do this when
    opening audio device.  This is also called from alsamm but I think that
    is no longer in use, so in principle this could be static. */

void sys_setchsr(int chin, int chout, int sr)
{
    int oldchin = STUFF->st_inchannels;
    int oldchout = STUFF->st_outchannels;
    int oldinbytes = (oldchin ? oldchin : 2) *
        (DEFDACBLKSIZE*sizeof(t_sample));
    int oldoutbytes = (oldchout ? oldchout : 2) *
        (DEFDACBLKSIZE*sizeof(t_sample));
    int inbytes = (chin ? chin : 2) *
        (DEFDACBLKSIZE*sizeof(t_sample));
    int outbytes = (chout ? chout : 2) *
        (DEFDACBLKSIZE*sizeof(t_sample));
    int changed = 0;

        /* NB: reallocating the input/output channel arrays requires a DSP
        graph update, so we only do it if the channel count has changed! */
    if (!STUFF->st_soundin || chin != oldchin)
    {
        if (STUFF->st_soundin)
            freebytes(STUFF->st_soundin, oldinbytes);
        STUFF->st_soundin = (t_sample *)getbytes(inbytes);
        STUFF->st_inchannels = chin;
        changed = 1;
    }
    memset(STUFF->st_soundin, 0, inbytes);

    if (!STUFF->st_soundout || chout != oldchout)
    {
        if (STUFF->st_soundout)
            freebytes(STUFF->st_soundout, oldoutbytes);
        STUFF->st_soundout = (t_sample *)getbytes(outbytes);
        STUFF->st_outchannels = chout;
        changed = 1;
    }
    memset(STUFF->st_soundout, 0, outbytes);

    if (!audio_isfixedsr(sys_audioapiopened))
    {
        if (STUFF->st_dacsr != sr)
            changed = 1;
        STUFF->st_dacsr = sr;
    }

    logpost(NULL, PD_VERBOSE, "input channels = %d, output channels = %d",
            STUFF->st_inchannels, STUFF->st_outchannels);

        /* prevent redundant DSP updates, particularly when toggling DSP */
    if (changed)
        canvas_update_dsp();
}

static void audio_make_sane(int *ndev, int *devvec,
    int *nchan, int *chanvec, int maxdev)
{
    int i;
    if (*ndev == -1)
    {           /* no input audio devices specified */
        if (*nchan == -1)
        {
            if (*ndev >= 1)
            {
                *nchan=1;
                chanvec[0] = SYS_DEFAULTCH;
                *ndev = 1;
                devvec[0] = DEFAULTAUDIODEV;
            }
            else *ndev = *nchan = 0;
        }
        else
        {
            for (i = 0; i < maxdev; i++)
                devvec[i] = i;
            *ndev = *nchan;
        }
    }
    else
    {
        if (*nchan == -1)
        {
            *nchan = *ndev;
            for (i = 0; i < *ndev; i++)
                chanvec[i] = SYS_DEFAULTCH;
        }
        else if (*nchan > *ndev)
        {
            for (i = *ndev; i < *nchan; i++)
            {
                if (i == 0)
                    devvec[0] = DEFAULTAUDIODEV;
                else devvec[i] = devvec[i-1] + 1;
            }
            *ndev = *nchan;
        }
        else if (*nchan < *ndev)
        {
            for (i = *nchan; i < *ndev; i++)
            {
                if (i == 0)
                    chanvec[0] = SYS_DEFAULTCH;
                else chanvec[i] = chanvec[i-1];
            }
            *ndev = *nchan;
        }
    }
    for (i = *ndev; i < maxdev; i++)
        devvec[i] = -1;
    for (i = *nchan; i < maxdev; i++)
        chanvec[i] = 0;

}

    /* compact the list of audio devices by skipping those whose channel
    counts are zero, and add up all channel counts.  Assumes you've already
    called make_sane above */

static void audio_compact_and_count_channels(int *ndev, int *devvec,
    int *chanvec, int *totalchans, int maxdev)
{
    int i, newndev;
            /* count total number of input and output channels */
    for (i = newndev = *totalchans = 0; i < *ndev; i++)
        if (chanvec[i] > 0)
    {
        chanvec[newndev] = chanvec[i];
        devvec[newndev] = devvec[i];
        *totalchans += chanvec[i];
        newndev++;
    }
    *ndev = newndev;
}

/* ----------------------- public routines ----------------------- */

static int initted = 0;

void sys_get_audio_settings(t_audiosettings *a)
{
    if (!initted)
    {
        audio_nextsettings.a_api = API_DEFAULT;
        audio_nextsettings.a_srate = DEFAULTSRATE;
        audio_nextsettings.a_nindev = audio_nextsettings.a_nchindev =
            audio_nextsettings.a_noutdev = audio_nextsettings.a_nchoutdev
                = 1;
        audio_nextsettings.a_indevvec[0] =
            audio_nextsettings.a_outdevvec[0] = DEFAULTAUDIODEV;
        audio_nextsettings.a_chindevvec[0] =
            audio_nextsettings.a_choutdevvec[0] = SYS_DEFAULTCH;
        audio_nextsettings.a_advance = DEFAULTADVANCE;
        audio_nextsettings.a_blocksize = DEFDACBLKSIZE;
        initted = 1;
    }
    *a = audio_nextsettings;
    if (audio_isfixedsr(a->a_api))
        a->a_srate = STUFF->st_dacsr;
    if (audio_isfixedblocksize(a->a_api))
        a->a_blocksize = audio_getfixedblocksize(a->a_api);
}

    /* Since the channel vector might be longer than the
    audio device vector, or vice versa, we fill the shorter one
    in to match the longer one.  Also, if both are empty, we fill in
    one device (the default) and two channels. This function can leave number
    of channels at zero which is appropriate for the dialog window but before
    starting audio we also call audio_compact_and_count_channels below.*/
    /* set audio device settings (after cleaning up the specified device and
    channel vectors).  The audio devices are "zero based" (i.e. "0" means the
    first one.)  We can later re-open audio and/or show the settings on a
    dialog window.   */

void sys_set_audio_settings(t_audiosettings *a)
{
    int i;
    char indevlist[MAXNDEV*DEVDESCSIZE], outdevlist[MAXNDEV*DEVDESCSIZE];
    int indevs = 0, outdevs = 0, canmulti = 0, cancallback = 0;
    sys_get_audio_devs(indevlist, &indevs, outdevlist, &outdevs, &canmulti,
        &cancallback, MAXNDEV, DEVDESCSIZE, a->a_api);

    if (a->a_srate < 1)
        a->a_srate = DEFAULTSRATE;
    if (a->a_advance < 0)
        a->a_advance = DEFAULTADVANCE;
    a->a_blocksize = 1 << ilog2(a->a_blocksize);
    if (a->a_blocksize < DEFDACBLKSIZE || a->a_blocksize > MAXBLOCKSIZE)
        a->a_blocksize = DEFDACBLKSIZE;
    if (a->a_callback && !cancallback)
        a->a_callback = 0;

    audio_make_sane(&a->a_noutdev, a->a_outdevvec,
        &a->a_nchoutdev, a->a_choutdevvec, MAXAUDIOOUTDEV);
    audio_make_sane(&a->a_nindev, a->a_indevvec,
        &a->a_nchindev, a->a_chindevvec, MAXAUDIOINDEV);

    sys_schedadvance = a->a_advance * 1000;
    audio_nextsettings = *a;
    initted = 1;

    sys_log_error(ERR_NOTHING);
    pdgui_vmess("set", "ri", "pd_whichapi", audio_nextsettings.a_api);
}

    /* close the audio device. Must not be called from a Pd message!
    Always called with Pd locked. */
void sys_do_close_audio(void)
{
    if (sys_externalschedlib)
    {
        return;
    }
    if (!audio_isopen())
        return;
#ifdef USEAPI_PORTAUDIO
    if (sys_audioapiopened == API_PORTAUDIO)
        pa_close_audio();
    else
#endif
#ifdef USEAPI_JACK
    if (sys_audioapiopened == API_JACK)
        jack_close_audio();
    else
#endif
#ifdef USEAPI_OSS
    if (sys_audioapiopened == API_OSS)
        oss_close_audio();
    else
#endif
#ifdef USEAPI_ALSA
    if (sys_audioapiopened == API_ALSA)
        alsa_close_audio();
    else
#endif
#ifdef USEAPI_MMIO
    if (sys_audioapiopened == API_MMIO)
        mmio_close_audio();
    else
#endif
#ifdef USEAPI_AUDIOUNIT
    if (sys_audioapiopened == API_AUDIOUNIT)
        audiounit_close_audio();
    else
#endif
#ifdef USEAPI_ESD
    if (sys_audioapiopened == API_ESD)
        esd_close_audio();
    else
#endif
#ifdef USEAPI_SGI
    if (sys_audioapiopened == API_SGI)
        sgi_close_audio();
    else
#endif
#ifdef USEAPI_DUMMY
    if (sys_audioapiopened == API_DUMMY)
        dummy_close_audio();
    else
#endif
        post("sys_close_audio: unknown API %d", sys_audioapiopened);
    sys_audioapiopened = API_NONE;
    sched_set_using_audio(SCHED_AUDIO_NONE);

    pdgui_vmess("set", "ri", "pd_whichapi", 0);
}

void sys_init_audio(void)
{
    t_audiosettings as;
    int totalinchans, totaloutchans;
    sys_get_audio_settings(&as);
    audio_compact_and_count_channels(&as.a_nindev, as.a_indevvec,
        as.a_chindevvec, &totalinchans, MAXAUDIOINDEV);
    audio_compact_and_count_channels(&as.a_noutdev, as.a_outdevvec,
        as.a_choutdevvec, &totaloutchans, MAXAUDIOOUTDEV);
    sys_setchsr(totalinchans, totaloutchans, as.a_srate);
}

    /* open audio using currently requested parameters.
    Must not be called from a Pd message!
    Always called with Pd locked. */
void sys_do_reopen_audio(void)
{
    t_audiosettings as;
    int outcome = 0, totalinchans, totaloutchans;
    sys_get_audio_settings(&as);
    /* fprintf(stderr, "audio in ndev %d, dev %d; out ndev %d, dev %d\n",
        as.a_nindev, as.a_indevvec[0], as.a_noutdev, as.a_outdevvec[0]); */
    audio_compact_and_count_channels(&as.a_nindev, as.a_indevvec,
        as.a_chindevvec, &totalinchans, MAXAUDIOINDEV);
    audio_compact_and_count_channels(&as.a_noutdev, as.a_outdevvec,
        as.a_choutdevvec, &totaloutchans, MAXAUDIOOUTDEV);
    sys_setchsr(totalinchans, totaloutchans, as.a_srate);
    if (!as.a_nindev && !as.a_noutdev)
    {
        sched_set_using_audio(SCHED_AUDIO_NONE);
        return;
    }
#ifdef USEAPI_PORTAUDIO
    if (as.a_api == API_PORTAUDIO)
    {
        int blksize = (as.a_blocksize ? as.a_blocksize : 64);
        int nbufs = (double)sys_schedadvance / 1000000. * as.a_srate / blksize;
            /* make sure that the delay is not smaller than the hardware blocksize.
            NB: nbufs has no effect if callbacks are enabled. */
        if (nbufs < 1 && !as.a_callback)
        {
            int delay = ((double)sys_schedadvance / 1000.) + 0.5;
            int limit = ceil(blksize * 1000. / (double)as.a_srate);
            nbufs = 1;
            post("warning: 'delay' setting (%d ms) too small for current blocksize "
                 "(%d samples); falling back to minimum value (%d ms)",
                 delay, blksize, limit);
        }
        outcome = pa_open_audio((as.a_nindev > 0 ? as.a_chindevvec[0] : 0),
            (as.a_noutdev > 0 ? as.a_choutdevvec[0] : 0), as.a_srate,
            STUFF->st_soundin, STUFF->st_soundout, blksize, nbufs,
            (as.a_nindev > 0 ? as.a_indevvec[0] : 0),
            (as.a_noutdev > 0 ? as.a_outdevvec[0] : 0),
            (as.a_callback ? sched_audio_callbackfn : 0));
    }
    else
#endif
#ifdef USEAPI_JACK
    if (as.a_api == API_JACK)
        outcome = jack_open_audio((as.a_nindev > 0 ? as.a_chindevvec[0] : 0),
            (as.a_noutdev > 0 ? as.a_choutdevvec[0] : 0),
                (as.a_callback ? sched_audio_callbackfn : 0));

    else
#endif
#ifdef USEAPI_OSS
    if (as.a_api == API_OSS)
        outcome = oss_open_audio(as.a_nindev, as.a_indevvec,
        as.a_nindev, as.a_chindevvec, as.a_noutdev, as.a_outdevvec,
        as.a_noutdev, as.a_choutdevvec, as.a_srate,
                as.a_blocksize);
    else
#endif
#ifdef USEAPI_ALSA
    if (as.a_api == API_ALSA)
        outcome = alsa_open_audio(as.a_nindev, as.a_indevvec,
            as.a_nindev, as.a_chindevvec, as.a_noutdev,
            as.a_outdevvec, as.a_noutdev, as.a_choutdevvec, as.a_srate,
                as.a_blocksize);
    else
#endif
#ifdef USEAPI_MMIO
    if (as.a_api == API_MMIO)
        outcome = mmio_open_audio(as.a_nindev, as.a_indevvec,
            as.a_nindev, as.a_chindevvec, as.a_noutdev,
                as.a_outdevvec, as.a_noutdev, as.a_choutdevvec, as.a_srate,
                as.a_blocksize);
    else
#endif
#ifdef USEAPI_AUDIOUNIT
    if (as.a_api == API_AUDIOUNIT)
        outcome = audiounit_open_audio(
            (as.a_nindev > 0 ? as.a_chindev[0] : 0),
            (as.a_nindev > 0 ? as.a_choutdev[0] : 0), as.a_srate);
    else
#endif
#ifdef USEAPI_ESD
    if (as.a_api == API_ESD)
        outcome = esd_open_audio(as.a_nindev, as.a_indevvec,
            as.a_nindev, as.a_chindevvec, as.a_noutdev,
                as.a_outdevvec, as.a_noutdev, as.a_choutdevvec, as.a_srate);
    else
#endif
#ifdef USEAPI_SGI
    if (as.a_api == API_SGI)
        outcome = sgi_open_audio(
            as.a_nindev, as.a_indevvec, as.a_nindev, as.a_chindevvec,
            as.a_noutdev, as.a_outdevvec, as.a_noutdev, as.a_choutdevvec,
            as.a_srate);
    else
#endif
#ifdef USEAPI_DUMMY
    if (as.a_api == API_DUMMY)
        outcome = dummy_open_audio(as.a_nindev, as.a_noutdev,
            as.a_srate);
    else
#endif
    if (as.a_api == API_NONE)
        ;
    else {
        post("unknown audio API specified %d", as.a_api);
        outcome = 1;
    }
    if (outcome)    /* failed */
    {
        sys_audioapiopened = API_NONE;
        sched_set_using_audio(SCHED_AUDIO_NONE);
    }
    else
    {
        sys_audioapiopened = as.a_api;
        sched_set_using_audio(
            (as.a_callback ? SCHED_AUDIO_CALLBACK : SCHED_AUDIO_POLL));
    }
    pdgui_vmess("set", "ri", "pd_whichapi", sys_audioapiopened);
}

    /* called by the scheduler if the audio system appears to be stuck. */
int sys_try_reopen_audio(void)
{
    int success;

    sys_lock();

#ifdef USEAPI_PORTAUDIO
    if (sys_audioapiopened == API_PORTAUDIO)
    {
        success = pa_reopen_audio();
        sys_unlock();
        return success;
    }
#endif
#ifdef USEAPI_JACK
    if (sys_audioapiopened == API_JACK)
    {
        success = jack_reopen_audio();
        sys_unlock();
        return success;
    }
#endif
        /* generic implementation: close audio and try to reopen it */
    sys_do_close_audio();

    pd_error(0, "trying to reopen audio device");

    sys_do_reopen_audio();
    success = audio_isopen();

    if (success)
        pd_error(0, "successfully reopened audio device");
    else
        pd_error(0, "audio device not responding - closing audio.\n"
                    "please try to reconnect and reselect it in the settings (or toggle DSP)");

    sys_unlock();

    return success;
}

int sys_send_dacs(void)
{
#ifdef USEAPI_PORTAUDIO
    if (sys_audioapiopened == API_PORTAUDIO)
        return (pa_send_dacs());
    else
#endif
#ifdef USEAPI_JACK
      if (sys_audioapiopened == API_JACK)
        return (jack_send_dacs());
    else
#endif
#ifdef USEAPI_OSS
    if (sys_audioapiopened == API_OSS)
        return (oss_send_dacs());
    else
#endif
#ifdef USEAPI_ALSA
    if (sys_audioapiopened == API_ALSA)
        return (alsa_send_dacs());
    else
#endif
#ifdef USEAPI_MMIO
    if (sys_audioapiopened == API_MMIO)
        return (mmio_send_dacs());
    else
#endif
#ifdef USEAPI_AUDIOUNIT
    if (sys_audioapiopened == API_AUDIOUNIT)
        return (audiounit_send_dacs());
    else
#endif
#ifdef USEAPI_ESD
    if (sys_audioapiopened == API_ESD)
        return (esd_send_dacs());
    else
#endif
#ifdef USEAPI_SGI
    if (sys_audioapiopened == API_SGI)
        return (sgi_send_dacs());
    else
#endif
#ifdef USEAPI_DUMMY
    if (sys_audioapiopened == API_DUMMY)
        return (dummy_send_dacs());
    else
#endif
    post("unknown API");
    return (0);
}

t_float sys_getsr(void)
{
     return (STUFF->st_dacsr);
}

int sys_get_outchannels(void)
{
     return (STUFF->st_outchannels);
}

int sys_get_inchannels(void)
{
     return (STUFF->st_inchannels);
}

/* this could later be set by a preference but for now it seems OK to just
keep jack audio open but close unused audio devices for any other API */
int audio_shouldkeepopen(void)
{
    if (sys_audioapiopened == API_NONE)
        return (audio_nextsettings.a_api == API_JACK);
    else
        return (sys_audioapiopened == API_JACK);
}

    /* get names of available audio devices for the specified API */
void sys_get_audio_devs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, int *cancallback,
        int maxndev, int devdescsize, int api)
{
    *cancallback = 0;   /* may be overridden by specific API implementation */
#ifdef USEAPI_PORTAUDIO
    if (api == API_PORTAUDIO)
    {
        pa_getdevs(indevlist, nindevs, outdevlist, noutdevs, canmulti,
            maxndev, devdescsize);
        *cancallback = 1;
    }
    else
#endif
#ifdef USEAPI_JACK
    if (api == API_JACK)
    {
        jack_getdevs(indevlist, nindevs, outdevlist, noutdevs, canmulti,
            maxndev, devdescsize);
        *cancallback = 1;
    }
    else
#endif
#ifdef USEAPI_OSS
    if (api == API_OSS)
    {
        oss_getdevs(indevlist, nindevs, outdevlist, noutdevs, canmulti,
            maxndev, devdescsize);
    }
    else
#endif
#ifdef USEAPI_ALSA
    if (api == API_ALSA)
    {
        alsa_getdevs(indevlist, nindevs, outdevlist, noutdevs, canmulti,
            maxndev, devdescsize);
    }
    else
#endif
#ifdef USEAPI_MMIO
    if (api == API_MMIO)
    {
        mmio_getdevs(indevlist, nindevs, outdevlist, noutdevs, canmulti,
            maxndev, devdescsize);
    }
    else
#endif
#ifdef USEAPI_AUDIOUNIT
    if (api == API_AUDIOUNIT)
    {
    }
    else
#endif
#ifdef USEAPI_ESD
    if (api == API_ESD)
    {
        esd_getdevs(indevlist, nindevs, outdevlist, noutdevs, canmulti,
            maxndev, devdescsize);
    }
    else
#endif
#ifdef USEAPI_SGI
    if (api == API_SGI)
    {
        sgi_getdevs(indevlist, nindevs, outdevlist, noutdevs, canmulti,
            maxndev, devdescsize);
    }
    else
#endif
#ifdef USEAPI_DUMMY
    if (api == API_DUMMY)
    {
        dummy_getdevs(indevlist, nindevs, outdevlist, noutdevs, canmulti,
            maxndev, devdescsize);
    }
    else
#endif
    {
            /* this shouldn't happen once all the above get filled in. */
        int i;
        *nindevs = *noutdevs = 3;
        for (i = 0; i < 3; i++)
        {
            sprintf(indevlist + i * devdescsize, "input device #%d", i+1);
            sprintf(outdevlist + i * devdescsize, "output device #%d", i+1);
        }
        *canmulti = 0;
    }
}


void sys_gui_audiopreferences(void) {
    t_audiosettings as;
        /* these are all the devices on your system: */
    char indevlist[MAXNDEV*DEVDESCSIZE], outdevlist[MAXNDEV*DEVDESCSIZE];
    char srate[80], callback[80], blocksize[80];
    const char *devicesI[MAXNDEV], *devicesO[MAXNDEV];
    t_float usedevsI[MAXAUDIOINDEV], devchansI[MAXAUDIOINDEV];
    t_float usedevsO[MAXAUDIOOUTDEV], devchansO[MAXAUDIOOUTDEV];
    int num_usedevsI, num_devchansI, num_usedevsO, num_devchansO;
    int num_devicesI = 0, num_devicesO = 0, canmulti = 0, cancallback = 0;
    int i;

        /* query the current AUDIO settings */
    sys_get_audio_settings(&as);
    sys_get_audio_devs(indevlist, &num_devicesI, outdevlist, &num_devicesO, &canmulti,
        &cancallback, MAXNDEV, DEVDESCSIZE, as.a_api);

        /* normalize the data a bit */
    if(!num_devicesI) {
        num_devicesI = 1;
        devicesI[0] = "";
    } else {
        for(i=0; i<num_devicesI; i++)
            devicesI[i] = indevlist + i*DEVDESCSIZE;
    }
    num_usedevsI = sizeof(as.a_indevvec)/sizeof(*as.a_indevvec);
    for(i=0; i<num_usedevsI; i++) {
        usedevsI[i] = (t_float)as.a_indevvec[i];
    }
    num_devchansI = sizeof(as.a_indevvec)/sizeof(*as.a_indevvec);
    for(i=0; i<num_devchansI; i++) {
        devchansI[i] = (t_float)as.a_chindevvec[i];
    }

    if(!num_devicesO) {
        num_devicesO = 1;
        devicesO[0] = "";
    } else {
        for(i=0; i<num_devicesO; i++)
            devicesO[i] = outdevlist + i*DEVDESCSIZE;
    }
    num_usedevsO = sizeof(as.a_outdevvec)/sizeof(*as.a_outdevvec);
    for(i=0; i<num_usedevsO; i++) {
        usedevsO[i] = (t_float)as.a_outdevvec[i];
    }
    num_devchansO = sizeof(as.a_outdevvec)/sizeof(*as.a_outdevvec);
    for(i=0; i<num_devchansO; i++) {
        devchansO[i] = (t_float)as.a_choutdevvec[i];
    }

    sprintf(srate, "%s%d", audio_isfixedsr(as.a_api)?"!":"", as.a_srate);
    sprintf(callback, "%s%d", cancallback?"":"!", as.a_callback);
    sprintf(blocksize, "%s%d", audio_isfixedblocksize(as.a_api)?"!":"", as.a_blocksize);

        /* and send it over to the GUI */
    pdgui_vmess("::dialog_audio::set_configuration", "SFF SFF ssi si",
        num_devicesI, devicesI, num_usedevsI, usedevsI, num_devchansI, devchansI,
        num_devicesO, devicesO, num_usedevsO, usedevsO, num_devchansO, devchansO,
        srate, blocksize, as.a_advance,
        callback, canmulti);
}

    /* start an audio settings dialog window */
void glob_audio_properties(t_pd *dummy)
{
    sys_gui_audiopreferences();
    pdgui_stub_deleteforkey(0);
    pdgui_stub_vnew(&glob_pdobject, "::dialog_audio::create",
        (void *)glob_audio_properties, "");
}

    /* new values from dialog window */
void glob_audio_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    t_audiosettings as;
    as.a_api = audio_nextsettings.a_api;
    as.a_srate = atom_getfloatarg(16, argc, argv);
    as.a_advance = atom_getfloatarg(17, argc, argv);
    as.a_callback = atom_getfloatarg(18, argc, argv);
    as.a_blocksize = atom_getfloatarg(19, argc, argv);

    for (i = 0; i < 4; i++)
    {
        as.a_indevvec[i] = atom_getfloatarg(i, argc, argv);
        as.a_chindevvec[i] = (as.a_indevvec[i] >= 0) ? atom_getfloatarg(i+4, argc, argv) : 0;
        as.a_outdevvec[i] = atom_getfloatarg(i+8, argc, argv);
        as.a_choutdevvec[i] = (as.a_outdevvec[i] >= 0) ? atom_getfloatarg(i+12, argc, argv) : 0;
    }
        /* compact out any zeros and count nonzero entries */
    for (i = 0, as.a_nindev = 0; i < MAXAUDIOINDEV; i++)
    {
        if (as.a_chindevvec[i])
        {
            as.a_indevvec[as.a_nindev] = as.a_indevvec[i];
            as.a_chindevvec[as.a_nindev] = as.a_chindevvec[i];
            as.a_nindev++;
        }
    }
    for (i = 0, as.a_noutdev = 0; i < MAXAUDIOOUTDEV; i++)
    {
        if (as.a_choutdevvec[i])
        {
            as.a_outdevvec[as.a_noutdev] = as.a_outdevvec[i];
            as.a_choutdevvec[as.a_noutdev] = as.a_choutdevvec[i];
            as.a_noutdev++;
        }
    }
    as.a_nchindev = as.a_nindev;
    as.a_nchoutdev = as.a_noutdev;
    if (as.a_callback < 0)
        as.a_callback = 0;
    as.a_blocksize = (1<<ilog2(as.a_blocksize));
    if (as.a_blocksize < DEFDACBLKSIZE || as.a_blocksize > MAXBLOCKSIZE)
            as.a_blocksize = DEFDACBLKSIZE;

    sys_set_audio_settings(&as);
    if (canvas_dspstate || audio_shouldkeepopen())
        sys_reopen_audio();
}

    /* rescan audio devices.  This is probably only useful for portaudio API
    for which we must deinit and reinit the library to see new devices. */
void glob_rescanaudio(t_pd *dummy)
{
    if (sched_get_using_audio() == SCHED_AUDIO_CALLBACK)
    {
        pd_error(0, "Cannot refresh device list in 'callback' mode when audio is running. "
            "Please turn off DSP and try again.");
        return;
    }
#ifdef USEAPI_PORTAUDIO
    if (audio_nextsettings.a_api == API_PORTAUDIO)
    {
        int was_open = audio_isopen();
        if (was_open) sys_close_audio();
        pa_reinitialize();
        if (was_open) sys_reopen_audio();
    }
#else
    post("Skipping rescan because it's not defined for this API");
#endif
    sys_gui_audiopreferences();
        /* refresh audio dialog (if it's open) */
    pdgui_vmess("::dialog_audio::refresh_ui", "");
}

void sys_listdevs(void)
{
    char indevlist[MAXNDEV*DEVDESCSIZE], outdevlist[MAXNDEV*DEVDESCSIZE];
    int nindevs = 0, noutdevs = 0, i, canmulti = 0, cancallback = 0;
    int offset = 0;

    sys_get_audio_devs(indevlist, &nindevs, outdevlist, &noutdevs,
        &canmulti, &cancallback, MAXNDEV, DEVDESCSIZE,
            audio_nextsettings.a_api);

#if 0
        /* To agree with command line flags, normally start at 1 */
        /* But microsoft "MMIO" device list starts at 0 (the "mapper"). */

       /* JMZ: otoh, it seems that the '-audiodev' flags 0-based
        * indices on ALSA and PORTAUDIO as well,
        * so we better show the correct ones here
        * (hence this line is disabled via #ifdef's)
        */
    offset = (audio_nextsettings.a_api != API_MMIO);
#endif

    if (!nindevs)
        post("no audio input devices found");
    else
    {
        post("audio input devices:");
        for (i = 0; i < nindevs; i++)
            post("%d. %s", i + offset,
                indevlist + i * DEVDESCSIZE);
    }
    if (!noutdevs)
        post("no audio output devices found");
    else
    {
        post("audio output devices:");
        for (i = 0; i < noutdevs; i++)
            post("%d. %s", i + offset,
                outdevlist + i * DEVDESCSIZE);
    }
    post("API number %d\n", audio_nextsettings.a_api);
    sys_listmididevs();
}

void glob_audio_setapi(void *dummy, t_floatarg f)
{
    int newapi = f;
    if (newapi)
    {
        if (newapi != audio_nextsettings.a_api)
        {
            audio_nextsettings.a_api = newapi;
                /* bash device params back to default */
            audio_nextsettings.a_nindev = audio_nextsettings.a_nchindev =
                audio_nextsettings.a_noutdev = audio_nextsettings.a_nchoutdev
                    = 1;
            audio_nextsettings.a_indevvec[0] =
                audio_nextsettings.a_outdevvec[0] = DEFAULTAUDIODEV;
            audio_nextsettings.a_chindevvec[0] =
                audio_nextsettings.a_choutdevvec[0] = SYS_DEFAULTCH;
            audio_nextsettings.a_blocksize = DEFDACBLKSIZE;
            audio_nextsettings.a_callback = 0;
            if (canvas_dspstate || audio_shouldkeepopen())
                sys_reopen_audio();
        }
        glob_audio_properties(0);
    }
    else if (audio_isopen())
    {
        sys_close_audio();
    }
}

#define MAXAPIENTRY 10
typedef struct _apientry
{
    char a_name[30];
    int a_id;
} t_apientry;

static t_apientry audio_apilist[] = {
#ifdef USEAPI_OSS
    {"OSS", API_OSS},
#endif
#ifdef USEAPI_ALSA
    {"ALSA", API_ALSA},
#endif
#ifdef USEAPI_PORTAUDIO
#ifdef _WIN32
    {"\"standard (portaudio)\"", API_PORTAUDIO},
#else
#ifdef __APPLE__
    {"\"standard (portaudio)\"", API_PORTAUDIO},
#else
    {"portaudio", API_PORTAUDIO},
#endif
#endif
#endif  /* USEAPI_PORTAUDIO */
#ifdef USEAPI_MMIO
    {"\"old MMIO system\"", API_MMIO},
#endif
#ifdef USEAPI_JACK
    {"jack", API_JACK},
#endif
#ifdef USEAPI_AUDIOUNIT
    {"Audiounit", API_AUDIOUNIT},
#endif
#ifdef USEAPI_ESD
    {"ESD",  API_ESD},
#endif
#ifdef USEAPI_SGI
    {"sgi", API_SGI},
#endif
#ifdef USEAPI_DUMMY
    {"dummy", API_DUMMY},
#endif
};

void sys_get_audio_apis(char *buf)
{
        /* FIXXME: this returns a raw Tcl-list!
         *  instead it should return something we can use with pdgui_vmess()
         */
    unsigned int n;
    if (sizeof(audio_apilist)/sizeof(t_apientry) < 2)
        strcpy(buf, "{}");
    else
    {
        strcpy(buf, "{ ");
        for (n = 0; n < sizeof(audio_apilist)/sizeof(t_apientry); n++)
            sprintf(buf + strlen(buf), "{%s %d} ",
                audio_apilist[n].a_name, audio_apilist[n].a_id);
        strcat(buf, "}");
    }
}

/* convert a device name to a (1-based) device number.  (Output device if
'output' parameter is true, otherwise input device).  Negative on failure. */

int sys_audiodevnametonumber(int output, const char *name)
{
    char indevlist[MAXNDEV*DEVDESCSIZE], outdevlist[MAXNDEV*DEVDESCSIZE];
    int nindevs = 0, noutdevs = 0, i, canmulti, cancallback;

    sys_get_audio_devs(indevlist, &nindevs, outdevlist, &noutdevs,
        &canmulti, &cancallback, MAXNDEV, DEVDESCSIZE,
            audio_nextsettings.a_api);

    if (output)
    {
            /* try first for exact match */
        for (i = 0; i < noutdevs; i++)
            if (!strcmp(name, outdevlist + i * DEVDESCSIZE))
                return (i);
            /* failing that, a match up to end of shorter string */
        for (i = 0; i < noutdevs; i++)
        {
            unsigned long comp = strlen(name);
            if (comp > strlen(outdevlist + i * DEVDESCSIZE))
                comp = strlen(outdevlist + i * DEVDESCSIZE);
            if (!strncmp(name, outdevlist + i * DEVDESCSIZE, comp))
                return (i);
        }
    }
    else
    {
        for (i = 0; i < nindevs; i++)
            if (!strcmp(name, indevlist + i * DEVDESCSIZE))
                return (i);
        for (i = 0; i < nindevs; i++)
        {
            unsigned long comp = strlen(name);
            if (comp > strlen(indevlist + i * DEVDESCSIZE))
                comp = strlen(indevlist + i * DEVDESCSIZE);
            if (!strncmp(name, indevlist + i * DEVDESCSIZE, comp))
                return (i);
        }
    }
    return (-1);
}

/* convert a (1-based) device number to a device name.  (Output device if
'output' parameter is true, otherwise input device).  Empty string on failure.
*/

void sys_audiodevnumbertoname(int output, int devno, char *name, int namesize)
{
    char indevlist[MAXNDEV*DEVDESCSIZE], outdevlist[MAXNDEV*DEVDESCSIZE];
    int nindevs = 0, noutdevs = 0, canmulti, cancallback;
    if (devno < 0)
    {
        *name = 0;
        return;
    }
    sys_get_audio_devs(indevlist, &nindevs, outdevlist, &noutdevs,
        &canmulti, &cancallback, MAXNDEV, DEVDESCSIZE,
            audio_nextsettings.a_api);
    if (output && (devno < noutdevs))
        strncpy(name, outdevlist + devno * DEVDESCSIZE, namesize);
    else if (!output && (devno < nindevs))
        strncpy(name, indevlist + devno * DEVDESCSIZE, namesize);
    else *name = 0;
    name[namesize-1] = 0;
}
