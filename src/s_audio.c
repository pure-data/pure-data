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

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#define SYS_DEFAULTCH 2
#define MAXNDEV 128
#define DEVDESCSIZE 128
#define MAXBLOCKSIZE 2048

    /* exported variables */
int sys_schedadvance;   /* scheduler advance in microseconds */

static int sys_audioapiopened; /* what API is open, API_NONE if none */
static int audio_callback_is_open;  /* true if we're open in callback mode */

    /* current parameters (if an API is open) or requested ones otherwise: */
static t_audiosettings audio_nextsettings;

void sched_audio_callbackfn(void);
void sched_reopenmeplease(void);

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
    int inbytes = (chin ? chin : 2) *
                (DEFDACBLKSIZE*sizeof(t_sample));
    int outbytes = (chout ? chout : 2) *
                (DEFDACBLKSIZE*sizeof(t_sample));

    if (STUFF->st_soundin)
        freebytes(STUFF->st_soundin,
            (STUFF->st_inchannels? STUFF->st_inchannels : 2) *
                (DEFDACBLKSIZE*sizeof(t_sample)));
    if (STUFF->st_soundout)
        freebytes(STUFF->st_soundout,
            (STUFF->st_outchannels? STUFF->st_outchannels : 2) *
                (DEFDACBLKSIZE*sizeof(t_sample)));
    STUFF->st_inchannels = chin;
    STUFF->st_outchannels = chout;
    if (!audio_isfixedsr(sys_audioapiopened))
        STUFF->st_dacsr = sr;

    STUFF->st_soundin = (t_sample *)getbytes(inbytes);
    memset(STUFF->st_soundin, 0, inbytes);

    STUFF->st_soundout = (t_sample *)getbytes(outbytes);
    memset(STUFF->st_soundout, 0, outbytes);

    logpost(NULL, PD_VERBOSE, "input channels = %d, output channels = %d",
            STUFF->st_inchannels, STUFF->st_outchannels);
    canvas_resume_dsp(canvas_suspend_dsp());
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

void sys_close_audio(void)
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
#ifdef USEAPI_DUMMY
    if (sys_audioapiopened == API_DUMMY)
        dummy_close_audio();
    else
#endif
        post("sys_close_audio: unknown API %d", sys_audioapiopened);
    sys_audioapiopened = API_NONE;
    sched_set_using_audio(SCHED_AUDIO_NONE);
    audio_callback_is_open = 0;

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

    /* open audio using currently requested parameters */
void sys_reopen_audio(void)
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
            /* make sure that the delay is not smaller than the hardware blocksize */
        if (nbufs < 1)
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
    if (as.a_api == API_ALSA)
        outcome = esd_open_audio(as.a_nindev, as.a_indevvec,
            as.a_nindev, as.a_chindevvec, as.a_noutdev,
                as.a_outdevvec, as.a_noutdev, as.a_choutdevvec, as.a_srate);
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
    else post("unknown audio API specified");
    if (outcome)    /* failed */
    {
        sys_audioapiopened = API_NONE;
        sched_set_using_audio(SCHED_AUDIO_NONE);
        audio_callback_is_open = 0;
    }
    else
    {
        sys_audioapiopened = as.a_api;
        sched_set_using_audio(
            (as.a_callback ? SCHED_AUDIO_CALLBACK : SCHED_AUDIO_POLL));
        audio_callback_is_open = as.a_callback;
    }
    pdgui_vmess("set", "ri", "pd_whichapi", sys_audioapiopened);
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

    /* start an audio settings dialog window */
void glob_audio_properties(t_pd *dummy, t_floatarg flongform)
{
    t_audiosettings as;
        /* these are all the devices on your system: */
    char indevlist[MAXNDEV*DEVDESCSIZE], outdevlist[MAXNDEV*DEVDESCSIZE];
    char device[MAXPDSTRING];
    char *indevs[MAXNDEV], *outdevs[MAXNDEV];
    int nindevs = 0, noutdevs = 0, canmulti = 0, cancallback = 0, i;
    char srate[80], callback[80], blocksize[80];

    sys_get_audio_devs(indevlist, &nindevs, outdevlist, &noutdevs, &canmulti,
         &cancallback, MAXNDEV, DEVDESCSIZE, audio_nextsettings.a_api);

    for (i = 0; i < nindevs; i++)
        indevs[i] = indevlist + i * DEVDESCSIZE;
    for (i = 0; i < noutdevs; i++)
        outdevs[i] = outdevlist + i * DEVDESCSIZE;

    if(!nindevs) {
        nindevs = 1;
        indevs[0] = "";
    }
    if(!noutdevs) {
        noutdevs = 1;
        outdevs[0] = "";
    }

    pdgui_vmess("set", "rS",
            "::audio_indevlist",
            nindevs, indevs);
    pdgui_vmess("set", "rS",
            "::audio_outdevlist",
            noutdevs, outdevs);

    sys_get_audio_settings(&as);

    if (as.a_nindev > 1 || as.a_noutdev > 1)
        flongform = 1;

    sprintf(srate, "%s%d", audio_isfixedsr(as.a_api)?"!":"", as.a_srate);
    sprintf(callback, "%s%d", cancallback?"":"!", as.a_callback);
    sprintf(blocksize, "%s%d", audio_isfixedblocksize(as.a_api)?"!":"", as.a_blocksize);

        /* values that are fixed and must not be changed by the GUI are
        prefixed with '!';  * the GUI will then display these values but
        disable their widgets */
    pdgui_stub_deleteforkey(0);
    pdgui_stub_vnew(&glob_pdobject,
        "pdtk_audio_dialog", (void *)glob_audio_properties,
        "iiii iiii iiii iiii  s ii s i s",
        as.a_indevvec   [0], as.a_indevvec   [1], as.a_indevvec   [2], as.a_indevvec   [3],
        as.a_chindevvec [0], as.a_chindevvec [1], as.a_chindevvec [2], as.a_chindevvec [3],
        as.a_outdevvec  [0], as.a_outdevvec  [1], as.a_outdevvec  [2], as.a_outdevvec  [3],
        as.a_choutdevvec[0], as.a_choutdevvec[1], as.a_choutdevvec[2], as.a_choutdevvec[3],
        srate,
        as.a_advance, canmulti,
        callback,
        (flongform != 0),
        blocksize);
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
        as.a_chindevvec[i] = atom_getfloatarg(i+4, argc, argv);
        as.a_outdevvec[i] = atom_getfloatarg(i+8, argc, argv);
        as.a_choutdevvec[i] = atom_getfloatarg(i+12, argc, argv);
    }
        /* compact out any zeros and count nonzero entries */
    for (i = 0, as.a_nindev = 0; i < 4; i++)
    {
        if (as.a_chindevvec[i])
        {
            as.a_indevvec[as.a_nindev] = as.a_indevvec[i];
            as.a_chindevvec[as.a_nindev] = as.a_chindevvec[i];
            as.a_nindev++;
        }
    }
    for (i = 0, as.a_noutdev = 0; i < 4; i++)
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

    if (!audio_callback_is_open && !as.a_callback)
        sys_close_audio();
    sys_set_audio_settings(&as);
    if (!audio_callback_is_open && !as.a_callback)
        sys_reopen_audio();
    else sched_reopenmeplease();
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
        /* (see also sys_mmio variable in s_main.c)  */

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
        if (newapi == audio_nextsettings.a_api)
        {
            if (!audio_isopen() && audio_shouldkeepopen())
                sys_reopen_audio();
        }
        else
        {
            sys_close_audio();
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
            sys_reopen_audio();
        }
        glob_audio_properties(0, 0);
    }
    else if (audio_isopen())
    {
        sys_close_audio();
    }
}

    /* start or stop the audio hardware */
void sys_set_audio_state(int onoff)
{
    if (onoff)  /* start */
    {
        if (!audio_isopen())
            sys_reopen_audio();
    }
    else
    {
        if (audio_isopen())
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
#ifdef USEAPI_MMIO
    {"\"standard (MMIO)\"", API_MMIO},
#endif
#ifdef USEAPI_ALSA
    {"ALSA", API_ALSA},
#endif
#ifdef USEAPI_PORTAUDIO
#ifdef _WIN32
    {"\"ASIO (portaudio)\"", API_PORTAUDIO},
#else
#ifdef __APPLE__
    {"\"standard (portaudio)\"", API_PORTAUDIO},
#else
    {"portaudio", API_PORTAUDIO},
#endif
#endif
#endif  /* USEAPI_PORTAUDIO */
#ifdef USEAPI_JACK
    {"jack", API_JACK},
#endif
#ifdef USEAPI_AUDIOUNIT
    {"Audiounit", API_AUDIOUNIT},
#endif
#ifdef USEAPI_ESD
    {"ESD",  API_ESD},
#endif
#ifdef USEAPI_DUMMY
    {"dummy", API_DUMMY},
#endif
};

void sys_get_audio_apis(char *buf)
{
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
