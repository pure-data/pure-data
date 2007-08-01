/* Copyright (c) 2005 Miller Puckette.  BSD licensed.  No warranties. */

/*
    fix parameter settings
    not to report pitch if evidence too scanty?
    note-on detection triggered by falling envelope (a posteriori)
    reentrancy bug setting loud flag (other parameters too?)
    tweaked freqs still not stable enough
    implement block ("-b") mode
*/

#include "m_pd.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef MSW
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include <stdlib.h>
#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

typedef struct peak
{
    float p_freq;
    float p_amp;
    float p_ampreal;
    float p_ampimag;
    float p_pit;
    float p_db;
    float p_salience;
    float p_tmp;
} t_peak;

/********************** service routines **************************/

static int sigmund_ilog2(int n)
{
    int ret = -1;
    while (n)
    {
        n >>= 1;
        ret++;
    }
    return (ret);
}

/* parameters for von Hann window (change these to get Hamming if desired) */
#define W_ALPHA 0.5
#define W_BETA 0.5
#define NEGBINS 4   /* number of bins of negative frequency we'll need */

#define PI 3.14159265
#define LOG2  0.69314718
#define LOG10 2.30258509

static float sinx(float theta, float sintheta)
{
    if (theta > -0.003 && theta < 0.003)
        return (1);
    else return (sintheta/theta);
}

static float window_hann_mag(float pidetune, float sinpidetune)
{
    return (W_ALPHA * sinx(pidetune, sinpidetune)
        - 0.5 * W_BETA *
            (sinx(pidetune+PI, sinpidetune) + sinx(pidetune-PI, sinpidetune)));
}

static float window_mag(float pidetune, float cospidetune)
{
    return (sinx(pidetune + (PI/2), cospidetune)
        + sinx(pidetune - (PI/2), -cospidetune));
}

/*********** Routines to analyze a window into sinusoidal peaks *************/

static int sigmund_cmp_freq(const void *p1, const void *p2)
{
    if ((*(t_peak **)p1)->p_freq > (*(t_peak **)p2)->p_freq)
        return (1);
    else if ((*(t_peak **)p1)->p_freq < (*(t_peak **)p2)->p_freq)
        return (-1);
    else return (0);
}

static void sigmund_tweak(int npts, float *ftreal, float *ftimag,
    int npeak, t_peak *peaks, float fperbin, int loud)
{
    t_peak **peakptrs = (t_peak **)alloca(sizeof (*peakptrs) * (npeak+1));
    t_peak negpeak;
    int peaki, j, k;
    float ampreal[3], ampimag[3];
    float binperf = 1./fperbin;
    float phaseperbin = (npts-0.5)/npts, oneovern = 1./npts;
    if (npeak < 1)
        return;
    for (peaki = 0; peaki < npeak; peaki++)
        peakptrs[peaki+1] = &peaks[peaki];
    qsort(peakptrs+1, npeak, sizeof (*peakptrs), sigmund_cmp_freq);
    peakptrs[0] = &negpeak;
    negpeak.p_ampreal = peakptrs[1]->p_ampreal;
    negpeak.p_ampimag = -peakptrs[1]->p_ampimag;
    negpeak.p_freq = -peakptrs[1]->p_freq;
    for (peaki = 1; peaki <= npeak; peaki++)
    {
        int cbin = peakptrs[peaki]->p_freq*binperf + 0.5;
        int nsub = (peaki == npeak ? 1:2);
        float windreal, windimag, windpower, detune, pidetune, sinpidetune,
            cospidetune, ampcorrect, ampout, ampoutreal, ampoutimag, freqout;
        /* post("3 nsub %d amp %f freq %f", nsub,
            peakptrs[peaki]->p_amp, peakptrs[peaki]->p_freq); */
        if (cbin < 0 || cbin > 2*npts - 3)
            continue;
        for (j = 0; j < 3; j++)
            ampreal[j] = ftreal[cbin+2*j-2], ampimag[j] = ftimag[cbin+2*j-2];
        /* post("a %f %f", ampreal[1], ampimag[1]); */
        for (j = 0; j < nsub; j++)
        {
            t_peak *neighbor = peakptrs[(peaki-1) + 2*j];
            float neighborreal = npts * neighbor->p_ampreal;
            float neighborimag = npts * neighbor->p_ampimag;
            for (k = 0; k < 3; k++)
            {
                float freqdiff = (0.5*PI) * ((cbin + 2*k-2)
                    -binperf * neighbor->p_freq);
                float sx = sinx(freqdiff, sin(freqdiff));
                float phasere = cos(freqdiff * phaseperbin);
                float phaseim = sin(freqdiff * phaseperbin);
                ampreal[k] -=
                    sx * (phasere * neighborreal - phaseim * neighborimag);
                ampimag[k] -=
                    sx * (phaseim * neighborreal + phasere * neighborimag);
            }       
            /* post("b %f %f", ampreal[1], ampimag[1]); */
        }

        windreal = W_ALPHA * ampreal[1] -
            (0.5 * W_BETA) * (ampreal[0] + ampreal[2]);
        windimag = W_ALPHA * ampimag[1] -
            (0.5 * W_BETA) * (ampimag[0] + ampimag[2]);
        windpower = windreal * windreal + windimag * windimag;
        detune = (
            W_BETA*(ampreal[0] - ampreal[2]) * 
                (2.0*W_ALPHA * ampreal[1] - W_BETA * (ampreal[0] + ampreal[2]))
                    +
            W_BETA*(ampimag[0] - ampimag[2]) *
                (2.0*W_ALPHA * ampimag[1] - W_BETA * (ampimag[0] + ampimag[2]))
                        ) / (4.0 * windpower);
        if (detune > 0.5)
            detune = 0.5;
        else if (detune < -0.5)
            detune = -0.5;
        /* if (loud > 0)
            post("tweak: windpower %f, bin %d, detune %f",
                windpower, cbin, detune); */
        pidetune = PI * detune;
        sinpidetune = sin(pidetune);
        cospidetune = cos(pidetune);

        ampcorrect = 1.0 / window_hann_mag(pidetune, sinpidetune);

        ampout = oneovern * ampcorrect *sqrt(windpower);
        ampoutreal = oneovern * ampcorrect *
            (windreal * cospidetune - windimag * sinpidetune);
        ampoutimag = oneovern * ampcorrect *
            (windreal * sinpidetune + windimag * cospidetune);
        freqout = (cbin + 2*detune) * fperbin;
        if (loud > 1)
            post("amp %f, freq %f", ampout, freqout);
        
        peakptrs[peaki]->p_freq = freqout;
        peakptrs[peaki]->p_amp = ampout;
        peakptrs[peaki]->p_ampreal = ampoutreal;
        peakptrs[peaki]->p_ampimag = ampoutimag;
    }
}

static void sigmund_remask(int maxbin, int bestindex, float powmask, 
    float maxpower, float *maskbuf)
{
    int bin;
    int bin1 = (bestindex > 52 ? bestindex-50:2);
    int bin2 = (maxbin < bestindex + 50 ? bestindex + 50 : maxbin);
    for (bin = bin1; bin < bin2; bin++)
    {
        float bindiff = bin - bestindex;
        float mymask;
        mymask = powmask/ (1. + bindiff * bindiff * bindiff * bindiff);
        if (bindiff < 2 && bindiff > -2)
            mymask = 2*maxpower;
        if (mymask > maskbuf[bin])
            maskbuf[bin] = mymask;
    } 
}

static void sigmund_getrawpeaks(int npts, float *insamps,
    int npeak, t_peak *peakv, int *nfound, float *power, float srate, int loud,
    float param1, float param2, float param3, float hifreq)
{
    float oneovern = 1.0/ (float)npts;
    float fperbin = 0.5 * srate * oneovern;
    int npts2 = 2*npts, i, bin;
    int count, peakcount = 0;
    float *fp1, *fp2;
    float *rawpow, *rawreal, *rawimag, *maskbuf, *powbuf;
    float *bigbuf = alloca(sizeof (float ) * (2*NEGBINS + 6*npts));
    int maxbin = hifreq/fperbin;
    int tweak = (param3 == 0);
    if (maxbin > npts - NEGBINS)
        maxbin = npts - NEGBINS;
    if (loud) post("tweak %d", tweak);
    maskbuf = bigbuf + npts2;
    powbuf = maskbuf + npts;
    rawreal = powbuf + npts+NEGBINS;
    rawimag = rawreal+npts+NEGBINS;
    for (i = 0; i < npts; i++)
        maskbuf[i] = 0;

    for (i = 0; i < npts; i++)
        bigbuf[i] = insamps[i];
    for (i = npts; i < 2*npts; i++)
        bigbuf[i] = 0;
    mayer_realfft(npts2, bigbuf);
    for (i = 0; i < npts; i++)
        rawreal[i] = bigbuf[i];
    for (i = 1; i < npts-1; i++)
        rawimag[i] = bigbuf[npts2-i];
    if (loud && npts == 1024)
    {
        float bigbuf2[2048];
        for (i = 0; i < 1024; i++)
            bigbuf2[i] = insamps[i];
        for (i = 1024; i < 2048; i++)
            bigbuf2[i] = 0;
        mayer_realfft(2048, bigbuf2);
        for (i = 1; i < 10; i++)
            post("(%10.2f, %10.2f) -> (%10.2f, %10.2f)",
                bigbuf2[i], bigbuf2[2048-i], rawreal[i], rawimag[i]);
    }

    rawreal[-1] = rawreal[1];
    rawreal[-2] = rawreal[2];
    rawreal[-3] = rawreal[3];
    rawreal[-4] = rawreal[4];
    rawimag[0] = rawimag[npts-1] = 0;
    rawimag[-1] = -rawimag[1];
    rawimag[-2] = -rawimag[2];
    rawimag[-3] = -rawimag[3];
    rawimag[-4] = -rawimag[4];
#if 1
    for (i = 0, fp1 = rawreal, fp2 = rawimag; i < npts-1; i++, fp1++, fp2++)
    {
        float x1 = fp1[1] - fp1[-1], x2 = fp2[1] - fp2[-1]; 
        powbuf[i] = x1*x1+x2*x2;
    }
    powbuf[npts-1] = 0;
#endif
    for (peakcount = 0; peakcount < npeak; peakcount++)
    {
        float pow1, maxpower = 0, totalpower = 0, windreal, windimag, windpower,
            detune, pidetune, sinpidetune, cospidetune, ampcorrect, ampout,
            ampoutreal, ampoutimag, freqout, freqcount1, freqcount2, powmask;
        int bestindex = -1;

        for (bin = 2, fp1 = rawreal+2, fp2 = rawimag+2;
            bin < maxbin; bin++, fp1++, fp2++)
        {
            pow1 = powbuf[bin];
            if (pow1 > maxpower && pow1 > maskbuf[bin])
            {
                float thresh = param2 * (powbuf[bin-2]+powbuf[bin+2]);
                if (pow1 > thresh)
                    maxpower = pow1, bestindex = bin;
            }
            totalpower += pow1;
        }

        if (totalpower <= 0 || maxpower < 1e-10*totalpower || bestindex < 0)
            break;
        fp1 = rawreal+bestindex;
        fp2 = rawimag+bestindex;
        *power = 0.5 * totalpower *oneovern * oneovern;
        powmask = maxpower * exp(-param1 * log(10.) / 10.);
        if (loud > 2)
            post("maxpower %f, powmask %f, param1 %f",
                maxpower, powmask, param1);
        sigmund_remask(maxbin, bestindex, powmask, maxpower, maskbuf);
        
        if (loud > 1)
            post("best index %d, total power %f", bestindex, totalpower);

        windreal = fp1[1] - fp1[-1];
        windimag = fp2[1] - fp2[-1];
        windpower = windreal * windreal + windimag * windimag;
        detune = ((fp1[1] * fp1[1] - fp1[-1]*fp1[-1]) 
            + (fp2[1] * fp2[1] - fp2[-1]*fp2[-1])) / (2 * windpower);
        if (loud > 2) post("(-1) %f %f; (1) %f %f",
            fp1[-1], fp2[-1], fp1[1], fp2[1]);
        if (loud > 2) post("peak %f %f",
            fp1[0], fp2[0]);

        if (detune > 0.5)
            detune = 0.5;
        else if (detune < -0.5)
            detune = -0.5;
        if (loud > 1)
            post("windpower %f, index %d, detune %f",
                windpower, bestindex, detune);
        pidetune = PI * detune;
        sinpidetune = sin(pidetune);
        cospidetune = cos(pidetune);
        ampcorrect = 1.0 / window_mag(pidetune, cospidetune);

        ampout = ampcorrect *sqrt(windpower);
        ampoutreal = ampcorrect *
            (windreal * cospidetune - windimag * sinpidetune);
        ampoutimag = ampcorrect *
            (windreal * sinpidetune + windimag * cospidetune);

            /* the frequency is the sum of the bin frequency and detuning */

        peakv[peakcount].p_freq = (freqout = (bestindex + 2*detune)) * fperbin;
        peakv[peakcount].p_amp = oneovern * ampout;
        peakv[peakcount].p_ampreal = oneovern * ampoutreal;
        peakv[peakcount].p_ampimag = oneovern * ampoutimag;
    }
    if (tweak)
    {
        sigmund_tweak(npts, rawreal, rawimag, peakcount, peakv, fperbin, loud);
        sigmund_tweak(npts, rawreal, rawimag, peakcount, peakv, fperbin, loud);
    }
    for (i = 0; i < peakcount; i++)
    {
        peakv[i].p_pit = ftom(peakv[i].p_freq);
        peakv[i].p_db = powtodb(peakv[i].p_amp);
    }
    *nfound = peakcount;
}

/*************** Routines for finding fundamental pitch *************/

#define PITCHNPEAK 12
#define PITCHUNCERTAINTY 0.3
#define HALFTONEINC 0.059
#define SUBHARMONICS 16
#define DBPERHALFTONE 0.5

static void sigmund_getpitch(int npeak, t_peak *peakv, float *freqp,
    float npts, float srate, int loud)
{
    float fperbin = 0.5 * srate / npts;
    int npit = 48 * sigmund_ilog2(npts), i, j, k, nsalient;
    float bestbin, bestweight, sumamp, sumweight, sumfreq, sumallamp, 
        freq;
    float *weights =  (float *)alloca(sizeof(float) * npit);
    t_peak *bigpeaks[PITCHNPEAK];
    int nbigpeaks;
    if (npeak < 1)
    {
        freq = 0;
        goto done;
    }
    for (i = 0; i < npit; i++)
        weights[i] = 0;
    for (i = 0; i < npeak; i++)
    {
        peakv[i].p_tmp = 0;
        peakv[i].p_salience = peakv[i].p_db - DBPERHALFTONE * peakv[i].p_pit;
    }
    for (nsalient = 0; nsalient < PITCHNPEAK; nsalient++)
    {
        t_peak *bestpeak = 0;
        float bestsalience = -1e20;
        for (j = 0; j < npeak; j++)
            if (peakv[j].p_tmp == 0 && peakv[j].p_salience > bestsalience)
        {
            bestsalience = peakv[j].p_salience;
            bestpeak = &peakv[j];
        }
        if (!bestpeak)
            break;
        bigpeaks[nsalient] = bestpeak;
        bestpeak->p_tmp = 1;
        /* post("peak f=%f a=%f", bestpeak->p_freq, bestpeak->p_amp); */
    }
    sumweight = 0;
    for (i = 0; i < nsalient; i++)
    {
        t_peak *thispeak = bigpeaks[i];
        float pitchuncertainty =
            4 * PITCHUNCERTAINTY * fperbin / (HALFTONEINC * thispeak->p_freq);
        float weightindex = (48./LOG2) *
            log(thispeak->p_freq/(2.*fperbin));
        float loudness = sqrt(thispeak->p_amp);
        /* post("index %f, uncertainty %f", weightindex, pitchuncertainty); */
        for (j = 0; j < SUBHARMONICS; j++)
        {
            float subindex = weightindex -
                (48./LOG2) * log(j + 1.);
            int loindex = subindex - pitchuncertainty;
            int hiindex = subindex + pitchuncertainty + 1;
            if (hiindex < 0)
                break;
            if (hiindex >= npit)
                continue;
            if (loindex < 0)
                loindex = 0;
            for (k = loindex; k <= hiindex; k++)
                weights[k] += loudness * 4. / (4. + j);
        }
        sumweight += loudness;
    }
#if 0
    for (i = 0; i < npit; i++)
    {
        postfloat(weights[i]);
        if (!((i+1)%12)) post("");
    }
#endif
    bestbin = -1;
    bestweight = -1e20;
    for (i = 0; i < npit; i++)
        if (weights[i] > bestweight)
            bestweight = weights[i], bestbin = i;
    if (bestweight < sumweight * 0.4)
        bestbin = -1;
    
    if (bestbin < 0)
    {
        freq = 0;
        goto done;
    }
    for (i = bestbin+1; i < npit; i++)
    {
        if (weights[i] < bestweight)
            break;
        bestbin += 0.5;
    }
    freq = 2*fperbin * exp((LOG2/48.)*bestbin);

    for (sumamp = sumweight = sumfreq = 0, i = 0; i < nsalient; i++)
    {
        t_peak *thispeak = bigpeaks[i];
        float thisloudness = sqrt(thispeak->p_amp);
        float thisfreq = thispeak->p_freq;
        float harmonic = thisfreq/freq;
        float intpart = (int)(0.5 + harmonic);
        float inharm = freq * (harmonic - intpart);
        if (harmonic < 1)
            continue;
        if (inharm < 0.25*fperbin && inharm > -0.25*fperbin)
        {
            float weight = thisloudness * intpart;
            sumweight += weight;
            sumfreq += weight*thisfreq/intpart;
        }
    }
    if (sumweight > 0)
        freq = sumfreq / sumweight;
done:
    if (!(freq >= 0 || freq <= 0))
    {
        post("freq nan cancelled");
        freq = 0;
    }
    *freqp = freq;
}

/*************** gather peak lists into sinusoidal tracks *************/

static void sigmund_peaktrack(int ninpeak, t_peak *inpeakv, 
    int noutpeak, t_peak *outpeakv, int loud)
{
    int incnt, outcnt;
    for (outcnt = 0; outcnt < noutpeak; outcnt++)
        outpeakv[outcnt].p_tmp = -1;
        
        /* first pass. Match each "in" peak with the closest previous
        "out" peak, but no two to the same one. */
    for (incnt = 0; incnt < ninpeak; incnt++)
    {
        float besterror = 1e20;
        int bestcnt = -1;
        inpeakv[incnt].p_tmp = -1;
        for (outcnt = 0; outcnt < noutpeak; outcnt++)
        {
            float thiserror =
                inpeakv[incnt].p_freq - outpeakv[outcnt].p_freq;
            if (thiserror < 0)
                thiserror = -thiserror;
            if (thiserror < besterror)
            {
                besterror = thiserror;
                bestcnt = outcnt;
            }
        }
        if (outpeakv[bestcnt].p_tmp < 0)
        {
            outpeakv[bestcnt] = inpeakv[incnt];
            inpeakv[incnt].p_tmp = 0;
            outpeakv[bestcnt].p_tmp = 0;
        }
    }
        /* second pass.  Unmatched "in" peaks assigned to free "out"
        peaks */
    for (incnt = 0; incnt < ninpeak; incnt++)
        if (inpeakv[incnt].p_tmp < 0)
    {
        for (outcnt = 0; outcnt < noutpeak; outcnt++)
            if (outpeakv[outcnt].p_tmp < 0)
        {
            outpeakv[outcnt] = inpeakv[incnt];
            inpeakv[incnt].p_tmp = 0;
            outpeakv[outcnt].p_tmp = 1;
            break;
        }
    }
    for (outcnt = 0; outcnt < noutpeak; outcnt++)
        if (outpeakv[outcnt].p_tmp == -1)
            outpeakv[outcnt].p_amp = 0;
}

/**************** parse continuous pitch into note starts ***************/

#define NHISTPOINT 100

typedef struct _histpoint
{
    float h_freq;
    float h_power;
} t_histpoint;

typedef struct _notefinder
{
    float n_age;
    float n_hifreq;
    float n_lofreq;
    int n_peaked;
    t_histpoint n_hist[NHISTPOINT];
    int n_histphase;
} t_notefinder;


static void notefinder_init(t_notefinder *x)
{
    int i;
    x->n_peaked = x->n_age = 0;
    x->n_hifreq = x->n_lofreq = 0;
    x->n_histphase = 0;
    for (i = 0; i < NHISTPOINT; i++)
        x->n_hist[i].h_freq =x->n_hist[i].h_power = 0;
}

static void notefinder_doit(t_notefinder *x, float freq, float power,
    float *note, float vibrato, int stableperiod, float powerthresh,
        float growththresh, int loud)
{
        /* calculate frequency ratio between allowable vibrato extremes
        (equal to twice the vibrato deviation from center) */
    float vibmultiple = exp((2*LOG2/12) * vibrato);
    int oldhistphase, i, k;
    if (stableperiod > NHISTPOINT - 1)
        stableperiod = NHISTPOINT - 1;
    if (++x->n_histphase == NHISTPOINT)
        x->n_histphase = 0;
    x->n_hist[x->n_histphase].h_freq = freq;
    x->n_hist[x->n_histphase].h_power = power;
    x->n_age++;
    *note = 0;
    if (loud)
    {
        post("stable %d, age %d, vibmultiple %f, powerthresh %f, hifreq %f",
            stableperiod, (int)x->n_age ,vibmultiple, powerthresh, x->n_hifreq);
        post("histfreq %f %f %f %f",
            x->n_hist[x->n_histphase].h_freq,
            x->n_hist[(x->n_histphase+NHISTPOINT-1)%NHISTPOINT].h_freq,
            x->n_hist[(x->n_histphase+NHISTPOINT-2)%NHISTPOINT].h_freq,
            x->n_hist[(x->n_histphase+NHISTPOINT-3)%NHISTPOINT].h_freq);
        post("power %f %f %f %f",
            x->n_hist[x->n_histphase].h_power,
            x->n_hist[(x->n_histphase+NHISTPOINT-1)%NHISTPOINT].h_power,
            x->n_hist[(x->n_histphase+NHISTPOINT-2)%NHISTPOINT].h_power,
            x->n_hist[(x->n_histphase+NHISTPOINT-3)%NHISTPOINT].h_power);
        for (i = 0, k = x->n_histphase; i < stableperiod; i++)
        {
            post("pit %5.1f  pow %f", ftom(x->n_hist[k].h_freq),
                x->n_hist[k].h_power);
            if (--k < 0)
                k = NHISTPOINT - 1;
        }
    }
       /* look for shorter notes than "stableperiod" in length.
       The amplitude must rise and then fall while the pitch holds
       steady. */
    if (x->n_hifreq <= 0 && x->n_age > stableperiod)
    {
        float maxpow = 0, freqatmaxpow = 0,
            localhifreq = -1e20, locallofreq = 1e20;
        int startphase = x->n_histphase - stableperiod + 1;
        if (startphase < 0)
            startphase += NHISTPOINT;
        for (i = 0, k = startphase; i < stableperiod; i++)
        {
            if (x->n_hist[k].h_freq <= 0)
                break;
            if (x->n_hist[k].h_power > maxpow)
                maxpow = x->n_hist[k].h_power,
                    freqatmaxpow = x->n_hist[k].h_freq;
            if (x->n_hist[k].h_freq > localhifreq)
                localhifreq = x->n_hist[k].h_freq;
            if (x->n_hist[k].h_freq < locallofreq)
                locallofreq = x->n_hist[k].h_freq;
            if (localhifreq > locallofreq * vibmultiple)
                break;
            if (maxpow > power * growththresh &&
                maxpow > x->n_hist[startphase].h_power * growththresh &&
                    localhifreq < vibmultiple * locallofreq
                        && freqatmaxpow > 0 && maxpow > powerthresh)
            {
                x->n_hifreq = x->n_lofreq = *note = freqatmaxpow;
                x->n_age = 0;
                x->n_peaked = 0;
                /* post("got short note"); */
                return;
            }
            if (++k >= NHISTPOINT)
                k = 0;
        }
        
    }
    if (x->n_hifreq > 0)
    {
            /* test if we're within "vibrato" range, and if so update range */
        if (freq * vibmultiple >= x->n_hifreq &&
            x->n_lofreq * vibmultiple >= freq)
        {
            if (freq > x->n_hifreq)
                x->n_hifreq = freq;
            if (freq < x->n_lofreq)
                x->n_lofreq = freq;
        }
        else if (x->n_hifreq > 0 && x->n_age > stableperiod)
        {
                /* if we've been out of range at least 1/2 the
                last "stableperiod" analyses, clear the note */
            int nbad = 0;
            for (i = 0, k = x->n_histphase; i < stableperiod - 1; i++)
            {
                if (--k < 0)
                    k = NHISTPOINT - 1;
                if (x->n_hist[k].h_freq * vibmultiple <= x->n_hifreq ||
                    x->n_lofreq * vibmultiple <= x->n_hist[k].h_freq)
                        nbad++;
            }
            if (2 * nbad >= stableperiod)
            {
                x->n_hifreq = x->n_lofreq = 0;
                x->n_age = 0;
            }
        }
    }

    oldhistphase = x->n_histphase - stableperiod;
    if (oldhistphase < 0)
        oldhistphase += NHISTPOINT;
        
        /* look for envelope attacks */

    if (x->n_hifreq > 0 && x->n_peaked)
    {
        if (freq > 0 && power > powerthresh &&
            power > x->n_hist[oldhistphase].h_power *
                exp((LOG10*0.1)*growththresh))
        {
                /* clear it and fall through for new stable-note test */
            x->n_peaked = 0;
            x->n_hifreq = x->n_lofreq = 0;
            x->n_age = 0;
        }
    }
    else if (!x->n_peaked)
    {
        if (x->n_hist[oldhistphase].h_power > powerthresh &&
            x->n_hist[oldhistphase].h_power > power)
                x->n_peaked = 1;
    }

        /* test for a new note using a stability criterion. */

    if (freq >= 0 &&
        (x->n_hifreq <= 0 || freq > x->n_hifreq || freq < x->n_lofreq))
    {
        float testfhi, testflo, maxpow = 0;
        for (i = 0, k = x->n_histphase, testfhi = testflo = freq;
            i < stableperiod-1; i++)
        {
            if (--k < 0)
                k = NHISTPOINT - 1;
            if (x->n_hist[k].h_freq > testfhi)
                testfhi = x->n_hist[k].h_freq;
            if (x->n_hist[k].h_freq < testflo)
                testflo = x->n_hist[k].h_freq;
            if (x->n_hist[k].h_power > maxpow)
                maxpow = x->n_hist[k].h_power;
        }
        if (testflo > 0 && testfhi <= vibmultiple * testflo
            && maxpow > powerthresh)
        {
                /* report new note */
            float sumf = 0, sumw = 0, thisf, thisw;
            for (i = 0, k = x->n_histphase; i < stableperiod; i++)
            {
                thisw = x->n_hist[k].h_power;
                sumw += thisw;
                sumf += thisw*x->n_hist[k].h_freq;
                if (--k < 0)
                    k = NHISTPOINT - 1;
            }
            x->n_hifreq = x->n_lofreq = *note = (sumw > 0 ? sumf/sumw : 0);
#if 0
                /* debugging printout */
            for (i = 0; i < stableperiod; i++)
            {
                int k3 = x->n_histphase - i;
                if (k3 < 0)
                    k3 += NHISTPOINT;
                startpost("%5.1f ", ftom(x->n_hist[k3].h_freq));
            }
            post("");
#endif
            x->n_age = 0;
            x->n_peaked = 0;
            return;
        }
    }
    *note = 0;
    return;
}

/*************************** Glue for Pd ************************/

static t_class *sigmund_class;
#define NHIST 100

#define MODE_STREAM 1
#define MODE_BLOCK 2        /* unimplemented */
#define MODE_TABLE 3

#define NPOINTS_DEF 1024
#define NPOINTS_MIN 128

#define HOP_DEF 512
#define NPEAK_DEF 20

#define VIBRATO_DEF 1
#define STABLETIME_DEF 50
#define MINPOWER_DEF 50
#define GROWTH_DEF 7

#define OUT_PITCH 0
#define OUT_ENV 1
#define OUT_NOTE 2
#define OUT_PEAKS 3
#define OUT_TRACKS 4
#define OUT_SMSPITCH 5
#define OUT_SMSNONPITCH 6

typedef struct _varout
{
    t_outlet *v_outlet;
    int v_what;
} t_varout;

typedef struct _sigmund
{
    t_object x_obj;
    t_varout *x_varoutv;
    int x_nvarout;
    t_clock *x_clock;
    float x_f;          /* for main signal inlet */
    float x_sr;         /* sample rate */
    int x_mode;         /* MODE_STREAM, etc. */
    int x_npts;         /* number of points in analysis window */
    int x_npeak;        /* number of peaks to find */
    int x_loud;         /* debug level */
    t_sample *x_inbuf;  /* input buffer */
    int x_infill;       /* number of points filled */
    int x_countdown;    /* countdown to start filling buffer */
    int x_hop;          /* samples between analyses */ 
    float x_maxfreq;    /* highest-frequency peak to report */ 
    float x_vibrato;    /* vibrato depth in half tones */ 
    float x_stabletime; /* period of stability needed for note */ 
    float x_growth;     /* growth to set off a new note */ 
    float x_minpower;   /* minimum power, in DB, for a note */ 
    float x_param1;
    float x_param2;
    float x_param3;
    t_notefinder x_notefinder;
    t_peak *x_trackv;
    int x_ntrack;
    unsigned int x_dopitch:1;
    unsigned int x_donote:1;
    unsigned int x_dotracks:1;
} t_sigmund;

static void sigmund_clock(t_sigmund *x);
static void sigmund_clear(t_sigmund *x);
static void sigmund_npts(t_sigmund *x, t_floatarg f);
static void sigmund_hop(t_sigmund *x, t_floatarg f);
static void sigmund_npeak(t_sigmund *x, t_floatarg f);
static void sigmund_maxfreq(t_sigmund *x, t_floatarg f);
static void sigmund_vibrato(t_sigmund *x, t_floatarg f);
static void sigmund_stabletime(t_sigmund *x, t_floatarg f);
static void sigmund_growth(t_sigmund *x, t_floatarg f);
static void sigmund_minpower(t_sigmund *x, t_floatarg f);

static void *sigmund_new(t_symbol *s, int argc, t_atom *argv)
{
    t_sigmund *x = (t_sigmund *)pd_new(sigmund_class);
    x->x_npts = NPOINTS_DEF;
    x->x_param1 = 0;
    x->x_param2 = 0.6;
    x->x_param3 = 0;
    x->x_hop = HOP_DEF;
    x->x_mode = MODE_STREAM;
    x->x_npeak = NPEAK_DEF;
    x->x_vibrato = VIBRATO_DEF;
    x->x_stabletime = STABLETIME_DEF;
    x->x_growth = GROWTH_DEF;
    x->x_minpower = MINPOWER_DEF;
    x->x_maxfreq = 1000000;
    x->x_loud = 0;
    x->x_sr = 1;
    x->x_nvarout = 0;
    x->x_varoutv = (t_varout *)getbytes(0);
    x->x_trackv = 0;
    x->x_ntrack = 0;
    x->x_dopitch = x->x_donote = x->x_dotracks = 0;
    x->x_inbuf = 0;

    while (argc > 0)
    {
        t_symbol *firstarg = atom_getsymbolarg(0, argc, argv);
        if (!strcmp(firstarg->s_name, "-t"))
        {
            x->x_mode = MODE_TABLE;
            argc--, argv++;
        }
        else if (!strcmp(firstarg->s_name, "-s"))
        {
            x->x_mode = MODE_STREAM;
            argc--, argv++;
        }
#if 0
        else if (!strcmp(firstarg->s_name, "-b"))
        {
            x->x_mode = MODE_BLOCK;
            argc--, argv++;
        }
#endif
        else if (!strcmp(firstarg->s_name, "-npts") && argc > 1)
        {
            x->x_npts = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-hop") && argc > 1)
        {
            sigmund_hop(x, atom_getfloatarg(1, argc, argv));
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-npeak") && argc > 1)
        {
            sigmund_npeak(x, atom_getfloatarg(1, argc, argv));
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-maxfreq") && argc > 1)
        {
            sigmund_maxfreq(x, atom_getfloatarg(1, argc, argv));
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-vibrato") && argc > 1)
        {
            sigmund_vibrato(x, atom_getfloatarg(1, argc, argv));
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-stabletime") && argc > 1)
        {
            sigmund_stabletime(x, atom_getfloatarg(1, argc, argv));
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-growth") && argc > 1)
        {
            sigmund_growth(x, atom_getfloatarg(1, argc, argv));
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-minpower") && argc > 1)
        {
            sigmund_minpower(x, atom_getfloatarg(1, argc, argv));
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "pitch"))
        {
            int n2 = x->x_nvarout+1;
            x->x_varoutv = (t_varout *)t_resizebytes(x->x_varoutv,
                x->x_nvarout*sizeof(t_varout), n2*sizeof(t_varout));
            x->x_varoutv[x->x_nvarout].v_outlet =
                outlet_new(&x->x_obj, &s_float);
            x->x_varoutv[x->x_nvarout].v_what = OUT_PITCH;
            x->x_nvarout = n2;
            x->x_dopitch = 1;
            argc--, argv++;
        }
        else if (!strcmp(firstarg->s_name, "env"))
        {
            int n2 = x->x_nvarout+1;
            x->x_varoutv = (t_varout *)t_resizebytes(x->x_varoutv,
                x->x_nvarout*sizeof(t_varout), n2*sizeof(t_varout));
            x->x_varoutv[x->x_nvarout].v_outlet =
                outlet_new(&x->x_obj, &s_float);
            x->x_varoutv[x->x_nvarout].v_what = OUT_ENV;
            x->x_nvarout = n2;
            argc--, argv++;
        }
        else if (!strcmp(firstarg->s_name, "note")
            || !strcmp(firstarg->s_name, "notes"))
        {
            int n2 = x->x_nvarout+1;
            x->x_varoutv = (t_varout *)t_resizebytes(x->x_varoutv,
                x->x_nvarout*sizeof(t_varout), n2*sizeof(t_varout));
            x->x_varoutv[x->x_nvarout].v_outlet =
                outlet_new(&x->x_obj, &s_float);
            x->x_varoutv[x->x_nvarout].v_what = OUT_NOTE;
            x->x_nvarout = n2;
            x->x_dopitch = x->x_donote = 1;
            argc--, argv++;
        }
        else if (!strcmp(firstarg->s_name, "peaks"))
        {
            int n2 = x->x_nvarout+1;
            x->x_varoutv = (t_varout *)t_resizebytes(x->x_varoutv,
                x->x_nvarout*sizeof(t_varout), n2*sizeof(t_varout));
            x->x_varoutv[x->x_nvarout].v_outlet =
                outlet_new(&x->x_obj, &s_list);
            x->x_varoutv[x->x_nvarout].v_what = OUT_PEAKS;
            x->x_nvarout = n2;
            argc--, argv++;
        }
        else if (!strcmp(firstarg->s_name, "tracks"))
        {
            int n2 = x->x_nvarout+1;
            x->x_varoutv = (t_varout *)t_resizebytes(x->x_varoutv,
                x->x_nvarout*sizeof(t_varout), n2*sizeof(t_varout));
            x->x_varoutv[x->x_nvarout].v_outlet =
                outlet_new(&x->x_obj, &s_list);
            x->x_varoutv[x->x_nvarout].v_what = OUT_TRACKS;
            x->x_nvarout = n2;
            x->x_dotracks = 1;
            argc--, argv++;
        }
        else
        {
            pd_error(x, "sigmund: %s: unknown flag or argument missing",
                firstarg->s_name);
            argc--, argv++;
        }
    }
    if (!x->x_nvarout)
    {
        x->x_varoutv = (t_varout *)t_resizebytes(x->x_varoutv,
            0, 2*sizeof(t_varout));
        x->x_varoutv[0].v_outlet = outlet_new(&x->x_obj, &s_float);
        x->x_varoutv[0].v_what = OUT_PITCH;
        x->x_varoutv[1].v_outlet = outlet_new(&x->x_obj, &s_float);
        x->x_varoutv[1].v_what = OUT_ENV;
        x->x_nvarout = 2;
        x->x_dopitch = 1;
    }
    if (x->x_dotracks)
    {
        x->x_ntrack = x->x_npeak;
        x->x_trackv = (t_peak *)getbytes(x->x_ntrack * sizeof(*x->x_trackv));
    }
    x->x_clock = clock_new(&x->x_obj.ob_pd, (t_method)sigmund_clock);
    
    x->x_infill = 0;
    x->x_countdown = 0;
    sigmund_npts(x, x->x_npts);
    notefinder_init(&x->x_notefinder);
    sigmund_clear(x);
    return (x);
}

static void sigmund_doit(t_sigmund *x, int npts, float *arraypoints,
    int loud, float srate)
{
    t_peak *peakv = (t_peak *)alloca(sizeof(t_peak) * x->x_npeak);
    int nfound, i, cnt;
    float freq = 0, power, note = 0;
    sigmund_getrawpeaks(npts, arraypoints, x->x_npeak, peakv,
        &nfound, &power, srate, loud, x->x_param1, x->x_param2, x->x_param3,
        x->x_maxfreq);
    if (x->x_dopitch)
        sigmund_getpitch(nfound, peakv, &freq, npts, srate, loud);
    if (x->x_donote)
        notefinder_doit(&x->x_notefinder, freq, power, &note, x->x_vibrato, 
            x->x_stabletime * 0.001f * x->x_sr / (float)x->x_hop,
                exp(LOG10*0.1*(x->x_minpower - 100)), x->x_growth, loud);
    if (x->x_dotracks)
        sigmund_peaktrack(nfound, peakv, x->x_ntrack, x->x_trackv, loud);
    
    for (cnt = x->x_nvarout; cnt--;)
    {
        t_varout *v = &x->x_varoutv[cnt];
        switch (v->v_what)
        {
        case OUT_PITCH:
            outlet_float(v->v_outlet, ftom(freq));
            break;
        case OUT_ENV:
            outlet_float(v->v_outlet, powtodb(power));
            break;
        case OUT_NOTE:
            if (note > 0)
                outlet_float(v->v_outlet, ftom(note));
            break;
        case OUT_PEAKS:
            for (i = 0; i < nfound; i++)
            {
                t_atom at[5];
                SETFLOAT(at, (float)i);
                SETFLOAT(at+1, peakv[i].p_freq);
                SETFLOAT(at+2, 2*peakv[i].p_amp);
                SETFLOAT(at+3, 2*peakv[i].p_ampreal);
                SETFLOAT(at+4, 2*peakv[i].p_ampimag);
                outlet_list(v->v_outlet, &s_list, 5, at);   
            }
            break;
        case OUT_TRACKS:
            for (i = 0; i < x->x_ntrack; i++)
            {
                t_atom at[4];
                SETFLOAT(at, (float)i);
                SETFLOAT(at+1, x->x_trackv[i].p_freq);
                SETFLOAT(at+2, 2*x->x_trackv[i].p_amp);
                SETFLOAT(at+3, x->x_trackv[i].p_tmp);
                outlet_list(v->v_outlet, &s_list, 4, at);   
            }
            break;
        }
    }
}

static void sigmund_list(t_sigmund *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *syminput = atom_getsymbolarg(0, argc, argv);
    int npts = atom_getintarg(1, argc, argv);
    int onset = atom_getintarg(2, argc, argv);
    float srate = atom_getfloatarg(3, argc, argv);
    int loud = atom_getfloatarg(4, argc, argv);
    int arraysize, totstorage, nfound, i;
    t_garray *a;
    float *arraypoints, pit;
    t_word *wordarray = 0;
    if (argc < 5)
    {
        post(
         "sigmund: array-name, npts, array-onset, samplerate, loud");
        return;
    }
    if (npts < 64 || npts != (1 << ilog2(npts))) 
    {
        error("sigmund: bad npoints");
        return;
    }
    if (onset < 0)
    {
        error("sigmund: negative onset");
        return;
    }
    arraypoints = alloca(sizeof(float)*npts);
    if (!(a = (t_garray *)pd_findbyclass(syminput, garray_class)) ||
        !garray_getfloatwords(a, &arraysize, &wordarray) ||
            arraysize < onset + npts)
    {
        error("%s: array missing or too small", syminput->s_name);
        return;
    }
    if (arraysize < npts)
    {
        error("sigmund~: too few points in array");
        return;
    }
    for (i = 0; i < npts; i++)
        arraypoints[i] = wordarray[i+onset].w_float;
    sigmund_doit(x, npts, arraypoints, loud, srate);
}

static void sigmund_clear(t_sigmund *x)
{
    if (x->x_trackv)
        memset(x->x_trackv, 0, x->x_ntrack * sizeof(*x->x_trackv));
    x->x_infill = x->x_countdown = 0;
}

    /* these are for testing; their meanings vary... */
static void sigmund_param1(t_sigmund *x, t_floatarg f)
{
    x->x_param1 = f;
}

static void sigmund_param2(t_sigmund *x, t_floatarg f)
{
    x->x_param2 = f;
}

static void sigmund_param3(t_sigmund *x, t_floatarg f)
{
    x->x_param3 = f;
}

static void sigmund_npts(t_sigmund *x, t_floatarg f)
{
    int nwas = x->x_npts, npts = f;
        /* check parameter ranges */
    if (npts < NPOINTS_MIN)
        post("sigmund~: minimum points %d", NPOINTS_MIN),
            npts = NPOINTS_MIN;
    if (npts != (1 << sigmund_ilog2(npts)))
        post("sigmund~: adjusting analysis size to %d points",
            (npts = (1 << sigmund_ilog2(npts))));
    if (npts != nwas)
        x->x_countdown = x->x_infill  = 0;
    if (x->x_mode == MODE_STREAM)
    {
        if (x->x_inbuf)
            x->x_inbuf = resizebytes(x->x_inbuf,
                sizeof(*x->x_inbuf) * nwas, sizeof(*x->x_inbuf) * npts);
        else x->x_inbuf = getbytes(sizeof(*x->x_inbuf) * npts);
    }
    else x->x_inbuf = 0;
    x->x_npts = npts;
}

static void sigmund_hop(t_sigmund *x, t_floatarg f)
{
    x->x_hop = f;
        /* check parameter ranges */
    if (x->x_hop != (1 << sigmund_ilog2(x->x_hop)))
        post("sigmund~: adjusting analysis size to %d points",
            (x->x_hop = (1 << sigmund_ilog2(x->x_hop))));
}

static void sigmund_npeak(t_sigmund *x, t_floatarg f)
{
    if (f < 1)
        f = 1;
    x->x_npeak = f;
}

static void sigmund_maxfreq(t_sigmund *x, t_floatarg f)
{
    x->x_maxfreq = f;
}

static void sigmund_vibrato(t_sigmund *x, t_floatarg f)
{
    if (f < 0)
        f = 0;
    x->x_vibrato = f;
}

static void sigmund_stabletime(t_sigmund *x, t_floatarg f)
{
    if (f < 0)
        f = 0;
    x->x_stabletime = f;
}

static void sigmund_growth(t_sigmund *x, t_floatarg f)
{
    if (f < 0)
        f = 0;
    x->x_growth = f;
}

static void sigmund_minpower(t_sigmund *x, t_floatarg f)
{
    if (f < 0)
        f = 0;
    x->x_minpower = f;
}

static void sigmund_print(t_sigmund *x)
{
    post("sigmund~ settings:");
    post("npts %d", (int)x->x_npts);
    post("hop %d", (int)x->x_hop);
    post("npeak %d", (int)x->x_npeak);
    post("maxfreq %g", x->x_maxfreq);
    post("vibrato %g", x->x_vibrato);
    post("stabletime %g", x->x_stabletime);
    post("growth %g", x->x_growth);
    post("minpower %g", x->x_minpower);
}

static void sigmund_printnext(t_sigmund *x, t_float f)
{
    x->x_loud = f;
}

static void sigmund_clock(t_sigmund *x)
{
    if (x->x_infill == x->x_npts)
    {
        sigmund_doit(x, x->x_npts, x->x_inbuf, x->x_loud, x->x_sr);
        if (x->x_hop >= x->x_npts)
        {
            x->x_infill = 0;
            x->x_countdown = x->x_hop - x->x_npts;
        }
        else
        {
            memmove(x->x_inbuf, x->x_inbuf + x->x_hop,
                (x->x_infill = x->x_npts - x->x_hop) * sizeof(*x->x_inbuf));
            x->x_countdown = 0;
        }
        x->x_loud = 0;
    }
}

static t_int *sigmund_perform(t_int *w)
{
    t_sigmund *x = (t_sigmund *)(w[1]);
    float *in = (float *)(w[2]);
    int n = (int)(w[3]);

    if (x->x_hop % n)
        return (w+4);
    if (x->x_countdown > 0)
        x->x_countdown -= n;
    else if (x->x_infill != x->x_npts)
    {
        int i, j;
        float *fp = x->x_inbuf + x->x_infill;
        for (j = 0; j < n; j++)
            *fp++ = *in++;
        x->x_infill += n;
        if (x->x_infill == x->x_npts)
            clock_delay(x->x_clock, 0);
    }
    return (w+4);
}

static void sigmund_dsp(t_sigmund *x, t_signal **sp)
{
    if (x->x_mode == MODE_STREAM)
    {
        if (x->x_hop % sp[0]->s_n)
            post("sigmund: adjusting hop size to %d",
                (x->x_hop = sp[0]->s_n * (x->x_hop / sp[0]->s_n)));
        x->x_sr = sp[0]->s_sr;
        dsp_add(sigmund_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
    }
}

static void sigmund_free(t_sigmund *x)
{
    if (x->x_inbuf)
        freebytes(x->x_inbuf, x->x_npts * sizeof(*x->x_inbuf));
    if (x->x_trackv)
        freebytes(x->x_trackv, x->x_ntrack * sizeof(*x->x_trackv));
    clock_free(x->x_clock);
}

void sigmund_tilde_setup(void)
{
    sigmund_class = class_new(gensym("sigmund~"), (t_newmethod)sigmund_new,
        (t_method)sigmund_free, sizeof(t_sigmund), 0, A_GIMME, 0);
    class_addlist(sigmund_class, sigmund_list);
    class_addmethod(sigmund_class, (t_method)sigmund_dsp, gensym("dsp"), 0);
    CLASS_MAINSIGNALIN(sigmund_class, t_sigmund, x_f);
    class_addmethod(sigmund_class, (t_method)sigmund_param1,
        gensym("param1"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_param2,
        gensym("param2"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_param3,
        gensym("param3"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_npts,
        gensym("npts"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_hop,
        gensym("hop"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_maxfreq,
        gensym("maxfreq"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_npeak,
        gensym("npeak"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_vibrato,
        gensym("vibrato"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_stabletime,
        gensym("stabletime"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_growth,
        gensym("growth"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_minpower,
        gensym("minpower"), A_FLOAT, 0);
    class_addmethod(sigmund_class, (t_method)sigmund_print,
        gensym("print"), 0);
    class_addmethod(sigmund_class, (t_method)sigmund_printnext,
        gensym("printnext"), A_FLOAT, 0);
    post("sigmund version 0.03");
}

