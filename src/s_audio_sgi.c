
/* ----------------------- Experimental routines for SGI -------------- */

/* written by Olaf Matthes <olaf.matthes@gmx.de> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <m_pd.h>
#include <s_stuff.h>

#include <dmedia/audio.h>
#include <sys/fpu.h>
#include <errno.h>

#define SGI_MAXDEV 4    /* it's just 3, but default counts as well... */
#define SGI_MAXCH 12    /* max. number of channels - is this enough? */

static ALport sgi_iport[SGI_MAXDEV];
static ALport sgi_oport[SGI_MAXDEV];
static ALconfig sgi_inconfig;
static ALconfig sgi_outconfig;
static int sgi_nindevs, sgi_noutdevs;
static int sgi_inchannels, sgi_outchannels;
static int sgi_ninchans[SGI_MAXDEV], sgi_noutchans[SGI_MAXDEV];


    /*
      set the special "flush zero" but (FS, bit 24) in the
      Control Status Register of the FPU of R4k and beyond
      so that the result of any underflowing operation will
      be clamped to zero, and no exception of any kind will
      be generated on the CPU.

      thanks to cpirazzi@cp.esd.sgi.com (Chris Pirazzi).
    */

static void sgi_flush_all_underflows_to_zero(void)
{
    union fpc_csr f;
    f.fc_word = get_fpc_csr();
    f.fc_struct.flush = 1;
    set_fpc_csr(f.fc_word);
}


    /* convert the most common errors into readable strings */
static char *sgi_get_error_message(int err)
{
    switch (err)
    {
    case AL_BAD_CONFIG:
        return "Invalid config";

    case AL_BAD_DIRECTION:
        return "Invalid direction (neither \"r\" nor \"w\")";

    case AL_BAD_OUT_OF_MEM:
        return "Not enough memory";

    case AL_BAD_DEVICE_ACCESS:
        return "Audio hardware is not available or is improperly configured";

    case AL_BAD_DEVICE:
        return "Invalid device";

    case AL_BAD_NO_PORTS:
        return "No audio ports available";

    case AL_BAD_QSIZE:
        return "Invalid fifo size";

    case AL_BAD_SAMPFMT:
        return "Invalid sample format";

    case AL_BAD_FLOATMAX:
        return "Invalid float maximum";

    case AL_BAD_WIDTH:
        return "Invalid sample width";

    default:
        return "Unknown error";
    }
}


int sgi_open_audio(int nindev,  int *indev,  int nchin,  int *chin,
                   int noutdev, int *outdev, int nchout, int *chout, int rate)
{
    ALpv pvbuf[2];
    int num_devs = 0;
    int in_dev = 0;
    int out_dev = 0;
    int inchans = 0;
    int outchans = 0;
    int err, n, gotchannels;
    char *indevnames[4] = {"DefaultIn", "AnalogIn", "AESIn", "ADATIn"};
    char *outdevnames[4] = {"DefaultOut", "AnalogOut", "AESOut", "ADATOut"};
        /* sys_advance_samples = (sys_schedadvance * sys_dacsr) / (1000000.); */
    int advance_samples = (sys_schedadvance * STUFF->st_dacsr) / (1000000.);

    sgi_flush_all_underflows_to_zero();

    if (sys_verbose)
        post("opening sound input...");

    for (n = 0; n < nindev; n++)
    {
        int gotchannels = 0;

        if (indev[n] >= 0 && indev[n] < SGI_MAXDEV)     /* open specified defice by name */
        {
            if (sys_verbose)
                post("opening %s", indevnames[indev[n]]);
            in_dev = alGetResourceByName(AL_SYSTEM,
                                         indevnames[indev[n]], AL_DEVICE_TYPE);
        }
        else    /* open default device */
        {
            if(sys_verbose)post("opening %s", indevnames[0]);
            in_dev = AL_DEFAULT_INPUT;
        }
        if (!in_dev)
        {
            pd_error(0, "%s\n", sgi_get_error_message(in_dev));
            continue;       /* try next device, if any */
        }

        sgi_inconfig = alNewConfig();
        alSetSampFmt(sgi_inconfig, AL_SAMPFMT_FLOAT);
        alSetFloatMax(sgi_inconfig, 1.1f);
        alSetChannels(sgi_inconfig, chin[n]);
        alSetQueueSize(sgi_inconfig, advance_samples * chin[n]);
        alSetDevice(sgi_inconfig, in_dev);
        sgi_iport[n] = alOpenPort("Pd input port", "r", sgi_inconfig);
        if (!sgi_iport[n])
            fprintf(stderr,"Pd: failed to open audio read port\n");

            /* try to set samplerate */
        pvbuf[0].param = AL_RATE;
        pvbuf[0].value.ll = alDoubleToFixed(rate);
        if ((err = alSetParams(in_dev, pvbuf, 1)) < 0)
        {
            post("could not set specified sample rate for input (%s)\n",
                 sgi_get_error_message(err));
            if(pvbuf[0].sizeOut < 0)
                post("rate was invalid\n");
        }
            /* check how many channels we actually got */
        pvbuf[0].param = AL_CHANNELS;
        if (alGetParams(in_dev, pvbuf, 1) < 0)
        {
            post("could not figure out how many input channels we got");
            gotchannels = chin[n];  /* assume we got them all */
        }
        else
        {
            gotchannels = pvbuf[0].value.i;
        }
        inchans += gotchannels; /* count total number of channels */
        sgi_ninchans[n] = gotchannels;  /* remember channels for this device */
    }

    if (sys_verbose)
        post("opening sound output...");

    for (n = 0; n < noutdev; n++)
    {
        if (outdev[n] >= 0 && outdev[n] < SGI_MAXDEV)   /* open specified defice by name */
        {
            if(sys_verbose)post("opening %s", outdevnames[outdev[n]]);
            out_dev = alGetResourceByName(AL_SYSTEM,
                                          outdevnames[outdev[n]], AL_DEVICE_TYPE);
        }
        else    /* open default device */
        {
            if (sys_verbose)
                post("opening %s", outdevnames[0]);
            out_dev = AL_DEFAULT_OUTPUT;
        }
        if (!out_dev)
        {
            pd_error(0, "%s\n", sgi_get_error_message(out_dev));
            continue;       /* try next device, if any */
        }

            /* configure the port before opening it */
        sgi_outconfig = alNewConfig();
        alSetSampFmt(sgi_outconfig, AL_SAMPFMT_FLOAT);
        alSetFloatMax(sgi_outconfig, 1.1f);
        alSetChannels(sgi_outconfig, chout[n]);
        alSetQueueSize(sgi_outconfig, advance_samples * chout[n]);
        alSetDevice(sgi_outconfig, out_dev);

            /* open the port */
        sgi_oport[n] = alOpenPort("Pd ouput port", "w", sgi_outconfig);
        if (!sgi_oport[n])
            fprintf(stderr,"Pd: failed to open audio write port\n");

            /* now try to set sample rate */
        pvbuf[0].param = AL_RATE;
        pvbuf[0].value.ll = alDoubleToFixed(rate);
        if ((err = alSetParams(out_dev, pvbuf, 1)) < 0)
        {
            post("could not set specified sample rate for output (%s)\n",
                 sgi_get_error_message(err));
            if(pvbuf[0].sizeOut < 0)
                post("rate was invalid\n");
        }
            /* check how many channels we actually got */
        pvbuf[0].param = AL_CHANNELS;
        if (alGetParams(out_dev, pvbuf, 1) < 0)
        {
            post("could not figure out how many output channels we got");
            gotchannels = chout[n];
        }
        else
        {
            gotchannels = pvbuf[0].value.i;
        }
        outchans += gotchannels;
        sgi_noutchans[n] = gotchannels;
    }

    sgi_noutdevs = noutdev;
    sgi_nindevs = nindev;

    sgi_inchannels = inchans;
    sgi_outchannels = outchans;

    return (!(inchans || outchans));
}


void sgi_close_audio(void)
{
    int err, n;
    for (n = 0; n < sgi_nindevs; n++)
    {
        if (sgi_iport[n])
        {
            err = alClosePort(sgi_iport[n]);
            if (err < 0)
                pd_error(0, "closing input %d: %s (%d)", n + 1,
                      alGetErrorString(oserror()), err);
        }
    }
    for (n = 0; n < sgi_noutdevs; n++)
    {
        if (sgi_oport[n])
        {
            err = alClosePort(sgi_oport[n]);
            if (err < 0)
                pd_error(0, "closing output %d: %s (%d)", n + 1,
                      alGetErrorString(oserror()), err);
        }
    }
}


    /* call this only if both input and output are open */
static void sgi_checkiosync(void)
{
    int i, result, checkit = 1, giveup = 1000, alreadylogged = 0;
    long indelay, outdelay, defect;

    if (!(sgi_outchannels && sgi_inchannels))
        return;
}


int sgi_send_dacs(void)
{
    float buf[SGI_MAXCH * DEFDACBLKSIZE], *fp1, *fp2, *fp3, *fp4;
    static int xferno = 0;
    static int callno = 0;
    static double timenow;
    double timelast;
    int inchannels = (STUFF->st_inchannels > sgi_inchannels ?
                      sgi_inchannels : STUFF->st_inchannels);
    int outchannels = (STUFF->st_outchannels > sgi_outchannels ?
                       sgi_outchannels : STUFF->st_outchannels);
    long outfill[SGI_MAXDEV], infill[SGI_MAXDEV];
    int outdevchannels, indevchannels;
    int i, n, channel;
    int result;
    unsigned int outtransfersize = DEFDACBLKSIZE;
    unsigned int intransfersize = DEFDACBLKSIZE;

        /* no audio channels open, return */
    if (!inchannels && !outchannels)
        return (SENDDACS_NO);

    timelast = timenow;
    timenow = sys_getrealtime();

#ifdef DEBUG_SGI_XFER
    if (timenow - timelast > 0.050)
        fprintf(stderr, "(%d)",
                (int)(1000 * (timenow - timelast))), fflush(stderr);
#endif

    callno++;

    sgi_checkiosync();     /* check I/O are in sync and data not late */

        /* check whether there is enough space in buffers */
    if (sgi_nindevs)
    {
        for (n = 0; n < sgi_nindevs; n++)
        {
            if (alGetFilled(sgi_iport[n]) < intransfersize)
                return SENDDACS_NO;
        }
    }
    if (sgi_noutdevs)
    {
        for(n = 0; n < sgi_noutdevs; n++)
        {
            if (alGetFillable(sgi_oport[n]) < outtransfersize)
                return SENDDACS_NO;
        }
    }

        /* output audio data, if we use audio out */
    if (sgi_noutdevs)
    {
        fp2 = STUFF->st_soundout;             /* point to current output position in buffer */
        for(n = 0; n < sgi_noutdevs; n++)
        {
            outdevchannels = sgi_noutchans[n];      /* channels supported by this device */

            for (channel = 0, fp1 = buf;
                 channel < outdevchannels; channel++, fp1++, fp2 += DEFDACBLKSIZE)
            {
                for (i = 0, fp3 = fp1, fp4 = fp2; i < DEFDACBLKSIZE;
                     i++, fp3 += outdevchannels, fp4++)
                    *fp3 = *fp4, *fp4 = 0;
            }
            alWriteFrames(sgi_oport[n], buf, DEFDACBLKSIZE);
        }
    }

        /* zero out the output buffer */
    memset(STUFF->st_soundout, 0, DEFDACBLKSIZE * sizeof(*STUFF->st_soundout) * STUFF->st_outchannels);
    if (sys_getrealtime() - timenow > 0.002)
    {
#ifdef DEBUG_SGI_XFER
        fprintf(stderr, "output %d took %d msec\n",
                callno, (int)(1000 * (timenow - timelast))), fflush(stderr);
#endif
        timenow = sys_getrealtime();
        sys_log_error(ERR_DACSLEPT);
    }

        /* get audio data from input, if we use audio in */
    if (sgi_nindevs)
    {
        fp2 = STUFF->st_soundin;              /* point to current input position in buffer */
        for (n = 0; n < sgi_nindevs; n++)
        {
            indevchannels = sgi_ninchans[n];        /* channels supported by this device */

            if (alGetFilled(sgi_iport[n]) > DEFDACBLKSIZE)
            {
                alReadFrames(sgi_iport[n], buf, DEFDACBLKSIZE);
            }
            else    /* have to read but nothing's there... */
            {
                    // if (sys_verbose) post("extra ADC buf");
                    /* set buffer to silence */
                memset(buf, 0, intransfersize * sizeof(*STUFF->st_soundout) * sgi_ninchans[n]);
            }
            for (channel = 0, fp1 = buf;
                 channel < indevchannels; channel++, fp1++, fp2 += DEFDACBLKSIZE)
            {
                for (i = 0, fp3 = fp1, fp4 = fp2; i < DEFDACBLKSIZE;
                     i++, fp3 += indevchannels, fp4++)
                    *fp4 = *fp3;
            }
        }
    }

    xferno++;

    if (sys_getrealtime() - timenow > 0.002)
    {
#ifdef DEBUG_SGI_XFER
        fprintf(stderr, "routine took %d msec\n",
                (int)(1000 * (sys_getrealtime() - timenow)));
#endif
        sys_log_error(ERR_ADCSLEPT);
    }
    return (SENDDACS_YES);
}

void sgi_getdevs(char *indevlist, int *nindevs,
                 char *outdevlist, int *noutdevs, int *canmulti,
                 int maxndev, int devdescsize)
{
#if 0
    ALpv      pvs[1];
    char      name[32];
    int i, ndev, err;
    *canmulti = 3;  /* supports multiple devices */

        /* get max. number of audio ports from system */
    pvs[0].param = AL_DEVICES;
    err = alGetParams(AL_SYSTEM, pvs, 1);
    if (err < 0)
    {
        sprintf("alGetParams failed: %s\n", alGetErrorString(oserror()));
    }
    ndev = pvs[0].value.i;

    for (i = 0; i < ndev; i++)
    {
        pvs[0].param = AL_NAME;  /* a string parameter */
        pvs[0].value.ptr = name; /* the string we've allocated */
        pvs[0].sizeIn = 32;      /* the array size, in characters,
                                    including space for NULL */
        if (alGetParams(i, pvs, 1) < 0)
        {
            sprintf("alGetParams failed: %s\n", alGetErrorString(oserror()));
        }
        sprintf(indevlist + i * devdescsize, "%d: %s", i + 1, name);
        sprintf(outdevlist + i * devdescsize, "%d: %s", i + 1, name);
    }
    *nindevs = ndev;
    *noutdevs = ndev;
#else
    sprintf(indevlist + 0 * devdescsize, "Default In");
    sprintf(outdevlist + 0 * devdescsize, "Default Out");
    sprintf(indevlist + 1 * devdescsize, "Analog In");
    sprintf(outdevlist + 1 * devdescsize, "Analog Out");
    sprintf(indevlist + 2 * devdescsize, "AES In");
    sprintf(outdevlist + 2 * devdescsize, "AES Out");
    sprintf(indevlist + 3 * devdescsize, "ADAT In");
    sprintf(outdevlist + 3 * devdescsize, "ADAT Out");

    *nindevs = 4;
    *noutdevs = 4;
    *canmulti = 3;  /* supports multiple devices */
#endif
}


/* list devices: only reflect the most common setup (Octane) */
void sgi_listaudiodevs(void)
{
    post("common devices on SGI machines:");
    post("#-1 - Default In/Out selected in Audio Panel");
    post("#1  - Analog In/Out");
    post("#2  - AES In/Out");
    post("#3  - ADAT I/O");
}
