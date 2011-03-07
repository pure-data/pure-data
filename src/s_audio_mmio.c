/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* modified 2/98 by Winfried Ritsch to deal with up to 4 synchronized
"wave" devices, which is how ADAT boards appear to the WAVE API. */

#include "m_pd.h"
#include "s_stuff.h"
#include <stdio.h>

#include <windows.h>

#include <MMSYSTEM.H>

/* ------------------------- audio -------------------------- */

static void nt_close_midiin(void);
static void nt_noresync( void);

static void postflags(void);

#define NAPORTS 16   /* wini hack for multiple ADDA devices  */
#define CHANNELS_PER_DEVICE 2
#define DEFAULTCHANS 2
#define DEFAULTSRATE 44100
#define SAMPSIZE 2

int nt_realdacblksize;
#define DEFREALDACBLKSIZE (4 * DEFDACBLKSIZE) /* larger underlying bufsize */

#define MAXBUFFER 100   /* number of buffers in use at maximum advance */
#define DEFBUFFER 30    /* default is about 30x6 = 180 msec! */
static int nt_naudiobuffer = DEFBUFFER;
float sys_dacsr = DEFAULTSRATE;

static int nt_whichapi = API_MMIO;
static int nt_meters;        /* true if we're metering */
static float nt_inmax;       /* max input amplitude */
static float nt_outmax;      /* max output amplitude */
static int nt_nwavein, nt_nwaveout;     /* number of WAVE devices in and out */

typedef struct _sbuf
{
    HANDLE hData;
    HPSTR  lpData;      // pointer to waveform data memory 
    HANDLE hWaveHdr; 
    WAVEHDR   *lpWaveHdr;   // pointer to header structure
} t_sbuf;

t_sbuf ntsnd_outvec[NAPORTS][MAXBUFFER];    /* circular buffer array */
HWAVEOUT ntsnd_outdev[NAPORTS];             /* output device */
static int ntsnd_outphase[NAPORTS];         /* index of next buffer to send */

t_sbuf ntsnd_invec[NAPORTS][MAXBUFFER];     /* circular buffer array */
HWAVEIN ntsnd_indev[NAPORTS];               /* input device */
static int ntsnd_inphase[NAPORTS];          /* index of next buffer to read */

static void nt_waveinerror(char *s, int err)
{
    char t[256];
    waveInGetErrorText(err, t, 256);
    fprintf(stderr, s, t);
}

static void nt_waveouterror(char *s, int err)
{
    char t[256];
    waveOutGetErrorText(err, t, 256);
    fprintf(stderr, s, t);
}

static void wave_prep(t_sbuf *bp, int setdone)
{
    WAVEHDR *wh;
    short *sp;
    int i;
    /* 
     * Allocate and lock memory for the waveform data. The memory 
     * for waveform data must be globally allocated with 
     * GMEM_MOVEABLE and GMEM_SHARE flags. 
     */ 

    if (!(bp->hData =
        GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE,
            (DWORD) (CHANNELS_PER_DEVICE * SAMPSIZE * nt_realdacblksize)))) 
                printf("alloc 1 failed\n");

    if (!(bp->lpData =
        (HPSTR) GlobalLock(bp->hData)))
            printf("lock 1 failed\n");

    /*  Allocate and lock memory for the header.  */ 

    if (!(bp->hWaveHdr =
        GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, (DWORD) sizeof(WAVEHDR))))
            printf("alloc 2 failed\n");

    if (!(wh = bp->lpWaveHdr =
        (WAVEHDR *) GlobalLock(bp->hWaveHdr))) 
            printf("lock 2 failed\n");

    for (i = CHANNELS_PER_DEVICE * nt_realdacblksize,
        sp = (short *)bp->lpData; i--; )
            *sp++ = 0;

    wh->lpData = bp->lpData;
    wh->dwBufferLength = (CHANNELS_PER_DEVICE * SAMPSIZE * nt_realdacblksize);
    wh->dwFlags = 0;
    wh->dwLoops = 0L;
    wh->lpNext = 0;
    wh->reserved = 0;
        /* optionally (for writing) set DONE flag as if we had queued them */
    if (setdone)
        wh->dwFlags = WHDR_DONE;
}

static UINT nt_whichdac = WAVE_MAPPER, nt_whichadc = WAVE_MAPPER;

int mmio_do_open_audio(void)
{ 
    PCMWAVEFORMAT  form; 
    int i, j;
    UINT mmresult;
    int nad, nda;
    static int naudioprepped = 0, nindevsprepped = 0, noutdevsprepped = 0;
    if (sys_verbose)
        post("%d devices in, %d devices out",
            nt_nwavein, nt_nwaveout);

    form.wf.wFormatTag = WAVE_FORMAT_PCM;
    form.wf.nChannels = CHANNELS_PER_DEVICE;
    form.wf.nSamplesPerSec = sys_dacsr;
    form.wf.nAvgBytesPerSec = sys_dacsr * (CHANNELS_PER_DEVICE * SAMPSIZE);
    form.wf.nBlockAlign = CHANNELS_PER_DEVICE * SAMPSIZE;
    form.wBitsPerSample = 8 * SAMPSIZE;

    if (nt_nwavein <= 1 && nt_nwaveout <= 1)
        nt_noresync();

    if (nindevsprepped < nt_nwavein)
    {
        for (i = nindevsprepped; i < nt_nwavein; i++)
            for (j = 0; j < naudioprepped; j++)
                wave_prep(&ntsnd_invec[i][j], 0);
        nindevsprepped = nt_nwavein;
    }
    if (noutdevsprepped < nt_nwaveout)
    {
        for (i = noutdevsprepped; i < nt_nwaveout; i++)
            for (j = 0; j < naudioprepped; j++)
                wave_prep(&ntsnd_outvec[i][j], 1);
        noutdevsprepped = nt_nwaveout;
    }
    if (naudioprepped < nt_naudiobuffer)
    {
        for (j = naudioprepped; j < nt_naudiobuffer; j++)
        {
            for (i = 0; i < nt_nwavein; i++)
                wave_prep(&ntsnd_invec[i][j], 0);
            for (i = 0; i < nt_nwaveout; i++)
                wave_prep(&ntsnd_outvec[i][j], 1);
        }
        naudioprepped = nt_naudiobuffer;
    }
    for (nad=0; nad < nt_nwavein; nad++)
    {
        /* Open waveform device(s), sucessively numbered, for input */

        mmresult = waveInOpen(&ntsnd_indev[nad], nt_whichadc+nad,
                (WAVEFORMATEX *)(&form), 0L, 0L, CALLBACK_NULL);

        if (sys_verbose)
            printf("opened adc device %d with return %d\n",
                nt_whichadc+nad,mmresult);

        if (mmresult != MMSYSERR_NOERROR) 
        {
            nt_waveinerror("waveInOpen: %s\n", mmresult);
            nt_nwavein = nad; /* nt_nwavein = 0 wini */
        } 
        else 
        {
            for (i = 0; i < nt_naudiobuffer; i++)
            {
                mmresult = waveInPrepareHeader(ntsnd_indev[nad],
                    ntsnd_invec[nad][i].lpWaveHdr, sizeof(WAVEHDR));
                if (mmresult != MMSYSERR_NOERROR)
                    nt_waveinerror("waveinprepareheader: %s\n", mmresult);
                mmresult = waveInAddBuffer(ntsnd_indev[nad],
                    ntsnd_invec[nad][i].lpWaveHdr, sizeof(WAVEHDR));
                if (mmresult != MMSYSERR_NOERROR)
                    nt_waveinerror("waveInAddBuffer: %s\n", mmresult);
            }
        }
    }
        /* quickly start them all together */
    for (nad = 0; nad < nt_nwavein; nad++)
        waveInStart(ntsnd_indev[nad]);

    for (nda = 0; nda < nt_nwaveout; nda++)
    {   
            /* Open a waveform device for output in sucessiv device numbering*/
        mmresult = waveOutOpen(&ntsnd_outdev[nda], nt_whichdac + nda,
            (WAVEFORMATEX *)(&form), 0L, 0L, CALLBACK_NULL);

        if (sys_verbose)
            fprintf(stderr,"opened dac device %d, with return %d\n",
                nt_whichdac +nda, mmresult);

        if (mmresult != MMSYSERR_NOERROR)
        {
            fprintf(stderr,"Wave out open device %d + %d\n",nt_whichdac,nda);
            nt_waveouterror("waveOutOpen device: %s\n",  mmresult);
            nt_nwaveout = nda;
        }
    }

    return (0);
}

void mmio_close_audio( void)
{
    int errcode;
    int nda, nad;
    if (sys_verbose)
        post("closing audio...");

    for (nda=0; nda < nt_nwaveout; nda++) /*if (nt_nwaveout) wini */
    {
       errcode = waveOutReset(ntsnd_outdev[nda]);
       if (errcode != MMSYSERR_NOERROR)
           printf("error resetting output %d: %d\n", nda, errcode);
       errcode = waveOutClose(ntsnd_outdev[nda]);
       if (errcode != MMSYSERR_NOERROR)
           printf("error closing output %d: %d\n",nda , errcode);          
    }
    nt_nwaveout = 0;

    for(nad=0; nad < nt_nwavein;nad++) /* if (nt_nwavein) wini */
    {
        errcode = waveInReset(ntsnd_indev[nad]);
        if (errcode != MMSYSERR_NOERROR)
            printf("error resetting input: %d\n", errcode);
        errcode = waveInClose(ntsnd_indev[nad]);
        if (errcode != MMSYSERR_NOERROR)
            printf("error closing input: %d\n", errcode);
    }
    nt_nwavein = 0;
}


#define ADCJITTER 10    /* We tolerate X buffers of jitter by default */
#define DACJITTER 10

static int nt_adcjitterbufsallowed = ADCJITTER;
static int nt_dacjitterbufsallowed = DACJITTER;

    /* ------------- MIDI time stamping from audio clock ------------ */

#ifdef MIDI_TIMESTAMP

static double nt_hibuftime;
static double initsystime = -1;

    /* call this whenever we reset audio */
static void nt_resetmidisync(void)
{
    initsystime = clock_getsystime();
    nt_hibuftime = sys_getrealtime();
}

    /* call this whenever we're idled waiting for audio to be ready. 
    The routine maintains a high and low water point for the difference
    between real and DAC time. */

static void nt_midisync(void)
{
    double jittersec, diff;
    
    if (initsystime == -1) nt_resetmidisync();
    jittersec = (nt_dacjitterbufsallowed > nt_adcjitterbufsallowed ?
        nt_dacjitterbufsallowed : nt_adcjitterbufsallowed)
            * nt_realdacblksize / sys_getsr();
    diff = sys_getrealtime() - 0.001 * clock_gettimesince(initsystime);
    if (diff > nt_hibuftime) nt_hibuftime = diff;
    if (diff < nt_hibuftime - jittersec)
    {
        post("jitter excess %d %f", dac, diff);
        nt_resetmidisync();
    }
}

static double nt_midigettimefor(LARGE_INTEGER timestamp)
{
    /* this is broken now... used to work when "timestamp" was derived from
        QueryPerformanceCounter() instead of the gates approved 
            timeGetSystemTime() call in the MIDI callback routine below. */
    return (nt_tixtotime(timestamp) - nt_hibuftime);
}
#endif      /* MIDI_TIMESTAMP */


static int nt_fill = 0;
#define WRAPFWD(x) ((x) >= nt_naudiobuffer ? (x) - nt_naudiobuffer: (x))
#define WRAPBACK(x) ((x) < 0 ? (x) + nt_naudiobuffer: (x))
#define MAXRESYNC 500

#if 0       /* this is used for debugging */
static void nt_printaudiostatus(void)
{
    int nad, nda;
    for (nad = 0; nad < nt_nwavein; nad++)
    {
        int phase = ntsnd_inphase[nad];
        int phase2 = phase, phase3 = WRAPFWD(phase2), count, ntrans = 0;
        int firstphasedone = -1, firstphasebusy = -1;
        for (count = 0; count < nt_naudiobuffer; count++)
        {
            int donethis =
                (ntsnd_invec[nad][phase2].lpWaveHdr->dwFlags & WHDR_DONE);
            int donenext =
                (ntsnd_invec[nad][phase3].lpWaveHdr->dwFlags & WHDR_DONE);
            if (donethis && !donenext)
            {
                if (firstphasebusy >= 0) goto multipleadc;
                firstphasebusy = count;
            }
            if (!donethis && donenext)
            {
                if (firstphasedone >= 0) goto multipleadc;
                firstphasedone = count;
            }
            phase2 = phase3;
            phase3 = WRAPFWD(phase2 + 1);
        }
        post("nad %d phase %d busy %d done %d", nad, phase, firstphasebusy,
            firstphasedone);
        continue;
    multipleadc:
        startpost("nad %d phase %d: oops:", nad, phase);
        for (count = 0; count < nt_naudiobuffer; count++)
        {
            char buf[80];
            sprintf(buf, " %d", 
                (ntsnd_invec[nad][count].lpWaveHdr->dwFlags & WHDR_DONE));
            poststring(buf);
        }
        endpost();
    }
    for (nda = 0; nda < nt_nwaveout; nda++)
    {
        int phase = ntsnd_outphase[nad];
        int phase2 = phase, phase3 = WRAPFWD(phase2), count, ntrans = 0;
        int firstphasedone = -1, firstphasebusy = -1;
        for (count = 0; count < nt_naudiobuffer; count++)
        {
            int donethis =
                (ntsnd_outvec[nda][phase2].lpWaveHdr->dwFlags & WHDR_DONE);
            int donenext =
                (ntsnd_outvec[nda][phase3].lpWaveHdr->dwFlags & WHDR_DONE);
            if (donethis && !donenext)
            {
                if (firstphasebusy >= 0) goto multipledac;
                firstphasebusy = count;
            }
            if (!donethis && donenext)
            {
                if (firstphasedone >= 0) goto multipledac;
                firstphasedone = count;
            }
            phase2 = phase3;
            phase3 = WRAPFWD(phase2 + 1);
        }
        if (firstphasebusy < 0) post("nda %d phase %d all %d",
            nda, phase, (ntsnd_outvec[nad][0].lpWaveHdr->dwFlags & WHDR_DONE));
        else post("nda %d phase %d busy %d done %d", nda, phase, firstphasebusy,
            firstphasedone);
        continue;
    multipledac:
        startpost("nda %d phase %d: oops:", nda, phase);
        for (count = 0; count < nt_naudiobuffer; count++)
        {
            char buf[80];
            sprintf(buf, " %d", 
                (ntsnd_outvec[nad][count].lpWaveHdr->dwFlags & WHDR_DONE));
            poststring(buf);
        }
        endpost();
    }
}
#endif /* 0 */

/* this is a hack to avoid ever resyncing audio pointers in case for whatever
reason the sync testing below gives false positives. */

static int nt_resync_cancelled;

static void nt_noresync( void)
{
    nt_resync_cancelled = 1;
}

static void nt_resyncaudio(void)
{
    UINT mmresult; 
    int nad, nda, count;
    if (nt_resync_cancelled)
        return;
        /* for each open input device, eat all buffers which are marked
            ready.  The next one will thus be "busy".  */
    post("resyncing audio");
    for (nad = 0; nad < nt_nwavein; nad++)
    {
        int phase = ntsnd_inphase[nad];
        for (count = 0; count < MAXRESYNC; count++)
        {
            WAVEHDR *inwavehdr = ntsnd_invec[nad][phase].lpWaveHdr;
            if (!(inwavehdr->dwFlags & WHDR_DONE)) break;
            if (inwavehdr->dwFlags & WHDR_PREPARED)
                waveInUnprepareHeader(ntsnd_indev[nad],
                    inwavehdr, sizeof(WAVEHDR)); 
            inwavehdr->dwFlags = 0L;
            waveInPrepareHeader(ntsnd_indev[nad], inwavehdr, sizeof(WAVEHDR)); 
            mmresult = waveInAddBuffer(ntsnd_indev[nad], inwavehdr,
                sizeof(WAVEHDR));
            if (mmresult != MMSYSERR_NOERROR)
                nt_waveinerror("waveInAddBuffer: %s\n", mmresult);
            ntsnd_inphase[nad] = phase = WRAPFWD(phase + 1);
        }
        if (count == MAXRESYNC) post("resync error 1");
    }
        /* Each output buffer which is "ready" is filled with zeros and
        queued. */
    for (nda = 0; nda < nt_nwaveout; nda++)
    {
        int phase = ntsnd_outphase[nda];
        for (count = 0; count < MAXRESYNC; count++)
        {
            WAVEHDR *outwavehdr = ntsnd_outvec[nda][phase].lpWaveHdr;
            if (!(outwavehdr->dwFlags & WHDR_DONE)) break;
            if (outwavehdr->dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(ntsnd_outdev[nda],
                    outwavehdr, sizeof(WAVEHDR)); 
            outwavehdr->dwFlags = 0L;
            memset((char *)(ntsnd_outvec[nda][phase].lpData),
                0, (CHANNELS_PER_DEVICE * SAMPSIZE * nt_realdacblksize));
            waveOutPrepareHeader(ntsnd_outdev[nda], outwavehdr,
                sizeof(WAVEHDR)); 
            mmresult = waveOutWrite(ntsnd_outdev[nda], outwavehdr,
                sizeof(WAVEHDR)); 
            if (mmresult != MMSYSERR_NOERROR)
                nt_waveouterror("waveOutAddBuffer: %s\n", mmresult);
            ntsnd_outphase[nda] = phase = WRAPFWD(phase + 1);
        }
        if (count == MAXRESYNC) post("resync error 2");
    }

#ifdef MIDI_TIMESTAMP
    nt_resetmidisync();
#endif

} 

#define LATE 0
#define RESYNC 1
#define NOTHING 2
static int nt_errorcount;
static int nt_resynccount;
static double nt_nextreporttime = -1;

void nt_logerror(int which)
{
#if 0
    post("error %d %d", count, which);
    if (which < NOTHING) nt_errorcount++;
    if (which == RESYNC) nt_resynccount++;
    if (sys_getrealtime() > nt_nextreporttime)
    {
        post("%d audio I/O error%s", nt_errorcount,
            (nt_errorcount > 1 ? "s" : ""));
        if (nt_resynccount) post("DAC/ADC sync error");
        nt_errorcount = nt_resynccount = 0;
        nt_nextreporttime = sys_getrealtime() - 5;
    }
#endif
}

/* system buffer with t_sample types for one tick */
t_sample *sys_soundout;
t_sample *sys_soundin;
float sys_dacsr;

int mmio_send_dacs(void)
{
    HMMIO hmmio; 
    UINT mmresult; 
    HANDLE hFormat; 
    int i, j;
    short *sp1, *sp2;
    float *fp1, *fp2;
    int nextfill, doxfer = 0;
    int nda, nad;
    if (!nt_nwavein && !nt_nwaveout) return (0);


    if (nt_meters)
    {
        int i, n;
        float maxsamp;
        for (i = 0, n = 2 * nt_nwavein * DEFDACBLKSIZE, maxsamp = nt_inmax;
            i < n; i++)
        {
            float f = sys_soundin[i];
            if (f > maxsamp) maxsamp = f;
            else if (-f > maxsamp) maxsamp = -f;
        }
        nt_inmax = maxsamp;
        for (i = 0, n = 2 * nt_nwaveout * DEFDACBLKSIZE, maxsamp = nt_outmax;
            i < n; i++)
        {
            float f = sys_soundout[i];
            if (f > maxsamp) maxsamp = f;
            else if (-f > maxsamp) maxsamp = -f;
        }
        nt_outmax = maxsamp;
    }

        /* the "fill pointer" nt_fill controls where in the next
        I/O buffers we will write and/or read.  If it's zero, we
        first check whether the buffers are marked "done". */

    if (!nt_fill)
    {
        for (nad = 0; nad < nt_nwavein; nad++)
        {
            int phase = ntsnd_inphase[nad];
            WAVEHDR *inwavehdr = ntsnd_invec[nad][phase].lpWaveHdr;
            if (!(inwavehdr->dwFlags & WHDR_DONE)) goto idle;
        }
        for (nda = 0; nda < nt_nwaveout; nda++)
        {
            int phase = ntsnd_outphase[nda];
            WAVEHDR *outwavehdr =
                ntsnd_outvec[nda][phase].lpWaveHdr;
            if (!(outwavehdr->dwFlags & WHDR_DONE)) goto idle;
        }
        for (nad = 0; nad < nt_nwavein; nad++)
        {
            int phase = ntsnd_inphase[nad];
            WAVEHDR *inwavehdr =
                ntsnd_invec[nad][phase].lpWaveHdr;
            if (inwavehdr->dwFlags & WHDR_PREPARED)
                waveInUnprepareHeader(ntsnd_indev[nad],
                    inwavehdr, sizeof(WAVEHDR)); 
        }
        for (nda = 0; nda < nt_nwaveout; nda++)
        {
            int phase = ntsnd_outphase[nda];
            WAVEHDR *outwavehdr = ntsnd_outvec[nda][phase].lpWaveHdr;
            if (outwavehdr->dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(ntsnd_outdev[nda],
                    outwavehdr, sizeof(WAVEHDR)); 
        }
    }

        /* Convert audio output to fixed-point and put it in the output
        buffer. */ 
    for (nda = 0, fp1 = sys_soundout; nda < nt_nwaveout; nda++)
    {
        int phase = ntsnd_outphase[nda];

        for (i = 0, sp1 = (short *)(ntsnd_outvec[nda][phase].lpData) +
            CHANNELS_PER_DEVICE * nt_fill;
                i < 2; i++, fp1 += DEFDACBLKSIZE, sp1++)
        {
            for (j = 0, fp2 = fp1, sp2 = sp1; j < DEFDACBLKSIZE;
                j++, fp2++, sp2 += CHANNELS_PER_DEVICE)
            {
                int x1 = 32767.f * *fp2;
                if (x1 > 32767) x1 = 32767;
                else if (x1 < -32767) x1 = -32767;
                *sp2 = x1;  
            }
        }
    }
    memset(sys_soundout, 0, 
        (DEFDACBLKSIZE *sizeof(t_sample)*CHANNELS_PER_DEVICE)*nt_nwaveout);

        /* vice versa for the input buffer */ 

    for (nad = 0, fp1 = sys_soundin; nad < nt_nwavein; nad++)
    {
        int phase = ntsnd_inphase[nad];

        for (i = 0, sp1 = (short *)(ntsnd_invec[nad][phase].lpData) +
            CHANNELS_PER_DEVICE * nt_fill;
                i < 2; i++, fp1 += DEFDACBLKSIZE, sp1++)
        {
            for (j = 0, fp2 = fp1, sp2 = sp1; j < DEFDACBLKSIZE;
                j++, fp2++, sp2 += CHANNELS_PER_DEVICE)
            {
                *fp2 = ((float)(1./32767.)) * (float)(*sp2);    
            }
        }
    }

    nt_fill = nt_fill + DEFDACBLKSIZE;
    if (nt_fill == nt_realdacblksize)
    {
        nt_fill = 0;

        for (nad = 0; nad < nt_nwavein; nad++)
        {
            int phase = ntsnd_inphase[nad];
            HWAVEIN device = ntsnd_indev[nad];
            WAVEHDR *inwavehdr = ntsnd_invec[nad][phase].lpWaveHdr;
            waveInPrepareHeader(device, inwavehdr, sizeof(WAVEHDR)); 
            mmresult = waveInAddBuffer(device, inwavehdr, sizeof(WAVEHDR)); 
            if (mmresult != MMSYSERR_NOERROR)
                nt_waveinerror("waveInAddBuffer: %s\n", mmresult);
            ntsnd_inphase[nad] = WRAPFWD(phase + 1);
        }
        for (nda = 0; nda < nt_nwaveout; nda++)
        {
            int phase = ntsnd_outphase[nda];
            HWAVEOUT device = ntsnd_outdev[nda];
            WAVEHDR *outwavehdr = ntsnd_outvec[nda][phase].lpWaveHdr;
            waveOutPrepareHeader(device, outwavehdr, sizeof(WAVEHDR)); 
            mmresult = waveOutWrite(device, outwavehdr, sizeof(WAVEHDR)); 
            if (mmresult != MMSYSERR_NOERROR)
                nt_waveouterror("waveOutWrite: %s\n", mmresult);
            ntsnd_outphase[nda] = WRAPFWD(phase + 1);
        }       

            /* check for DAC underflow or ADC overflow. */
        for (nad = 0; nad < nt_nwavein; nad++)
        {
            int phase = WRAPBACK(ntsnd_inphase[nad] - 2);
            WAVEHDR *inwavehdr = ntsnd_invec[nad][phase].lpWaveHdr;
            if (inwavehdr->dwFlags & WHDR_DONE) goto late;
        }
        for (nda = 0; nda < nt_nwaveout; nda++)
        {
            int phase = WRAPBACK(ntsnd_outphase[nda] - 2);
            WAVEHDR *outwavehdr = ntsnd_outvec[nda][phase].lpWaveHdr;
            if (outwavehdr->dwFlags & WHDR_DONE) goto late;
        }       
    }
    return (1);

late:

    nt_logerror(LATE);
    nt_resyncaudio();
    return (1);

idle:

    /* If more than nt_adcjitterbufsallowed ADC buffers are ready
    on any input device, resynchronize */

    for (nad = 0; nad < nt_nwavein; nad++)
    {
        int phase = ntsnd_inphase[nad];
        WAVEHDR *inwavehdr =
            ntsnd_invec[nad]
                [WRAPFWD(phase + nt_adcjitterbufsallowed)].lpWaveHdr;
        if (inwavehdr->dwFlags & WHDR_DONE)
        {
            nt_resyncaudio();
            return (0);
        }
    }

        /* test dac sync the same way */
    for (nda = 0; nda < nt_nwaveout; nda++)
    {
        int phase = ntsnd_outphase[nda];
        WAVEHDR *outwavehdr =
            ntsnd_outvec[nda]
                [WRAPFWD(phase + nt_dacjitterbufsallowed)].lpWaveHdr;
        if (outwavehdr->dwFlags & WHDR_DONE)
        {
            nt_resyncaudio();
            return (0);
        }
    }
#ifdef MIDI_TIMESTAMP
    nt_midisync();
#endif
    return (0);
}

/* ------------------- public routines -------------------------- */

int mmio_open_audio(int naudioindev, int *audioindev,
    int nchindev, int *chindev, int naudiooutdev, int *audiooutdev,
    int nchoutdev, int *choutdev, int rate, int blocksize)
{
    int nbuf;

    nt_realdacblksize = (blocksize ? blocksize : DEFREALDACBLKSIZE);
    nbuf = sys_advance_samples/nt_realdacblksize;
    if (nbuf >= MAXBUFFER)
    {
        fprintf(stderr, "pd: audio buffering maxed out to %d\n",
            (int)(MAXBUFFER * ((nt_realdacblksize * 1000.)/44100.)));
        nbuf = MAXBUFFER;
    }
    else if (nbuf < 4) nbuf = 4;
    /* fprintf(stderr, "%d audio buffers\n", nbuf); */
    nt_naudiobuffer = nbuf;
    if (nt_adcjitterbufsallowed > nbuf - 2)
        nt_adcjitterbufsallowed = nbuf - 2;
    if (nt_dacjitterbufsallowed > nbuf - 2)
        nt_dacjitterbufsallowed = nbuf - 2;

    nt_nwavein = sys_inchannels / 2;
    nt_nwaveout = sys_outchannels / 2;
    nt_whichadc = (naudioindev < 1 ?
        (nt_nwavein > 1 ? WAVE_MAPPER : -1) : audioindev[0]);
    nt_whichdac = (naudiooutdev < 1 ?
        (nt_nwaveout > 1 ? WAVE_MAPPER : -1) : audiooutdev[0]);
    if (naudiooutdev > 1 || naudioindev > 1)
 post("separate audio device choice not supported; using sequential devices.");
    return (mmio_do_open_audio());
}


void mmio_reportidle(void)
{
}

#if 0
/* list the audio and MIDI device names */
void mmio_listdevs(void)
{
    UINT  wRtn, ndevices;
    unsigned int i;

    ndevices = waveInGetNumDevs();
    for (i = 0; i < ndevices; i++)
    {
        WAVEINCAPS wicap;
        wRtn = waveInGetDevCaps(i, (LPWAVEINCAPS) &wicap,
            sizeof(wicap));
        if (wRtn) nt_waveinerror("waveInGetDevCaps: %s\n", wRtn);
        else fprintf(stderr,
            "audio input device #%d: %s\n", i+1, wicap.szPname);
    }

    ndevices = waveOutGetNumDevs();
    for (i = 0; i < ndevices; i++)
    {
        WAVEOUTCAPS wocap;
        wRtn = waveOutGetDevCaps(i, (LPWAVEOUTCAPS) &wocap,
            sizeof(wocap));
        if (wRtn) nt_waveouterror("waveOutGetDevCaps: %s\n", wRtn);
        else fprintf(stderr,
            "audio output device #%d: %s\n", i+1, wocap.szPname);
    }
}
#endif

void mmio_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize)
{
    int  wRtn, ndev, i;

    *canmulti = 2;  /* supports multiple devices */
    ndev = waveInGetNumDevs();
    if (ndev > maxndev)
        ndev = maxndev;
    *nindevs = ndev;
    for (i = 0; i < ndev; i++)
    {
        WAVEINCAPS wicap;
        wRtn = waveInGetDevCaps(i, (LPWAVEINCAPS) &wicap, sizeof(wicap));
        sprintf(indevlist + i * devdescsize, (wRtn ? "???" : wicap.szPname));
    }

    ndev = waveOutGetNumDevs();
    if (ndev > maxndev)
        ndev = maxndev;
    *noutdevs = ndev;
    for (i = 0; i < ndev; i++)
    {
        WAVEOUTCAPS wocap;
        wRtn = waveOutGetDevCaps(i, (LPWAVEOUTCAPS) &wocap, sizeof(wocap));
        sprintf(outdevlist + i * devdescsize, (wRtn ? "???" : wocap.szPname));
    }
}
