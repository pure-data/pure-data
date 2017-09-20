/* Copyright (c) 1999 Miller Puckette.  The
contents of this file are free for any use, but BOTH THE AUTHOR AND UCSD
DISCLAIM ALL WARRANTIES related to it.  Although not written in Java, this
still should not be used to control any machinery containing a sharp blade or
combustible materiel, or as part of any life support system or weapon. */

#include "m_pd.h"
#include <math.h>
#include <stdio.h>
/* These pragmas are only used for MSVC, not MinGW or Cygwin <hans@at.or.at> */
#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

static t_class *pique_class;

typedef struct _pique
{
    t_object x_obj;
    int x_n;
    t_float x_errthresh;
    t_float *x_freq;
    t_float *x_amp;
    t_float *x_ampre;
    t_float *x_ampim;
} t_pique;

static void *pique_new(t_floatarg f)
{
    int n = f;
    t_pique *x = (t_pique *)pd_new(pique_class);
    if (n < 1) n = 100;
    x->x_n = n;
    x->x_errthresh = 0;
    x->x_freq = t_getbytes(n * sizeof(*x->x_freq));
    x->x_amp = t_getbytes(n * sizeof(*x->x_amp));
    x->x_ampre = t_getbytes(n * sizeof(*x->x_ampre));
    x->x_ampim = t_getbytes(n * sizeof(*x->x_ampim));
    outlet_new(&x->x_obj, &s_list);
    return (x);
}

static t_float hanning(t_float pidetune, t_float sinpidetune)
{
    t_float pi = 3.141592653589793;
    if (pidetune < 0.01 && pidetune > -0.01) return (1);
    else if (pidetune > 3.14 && pidetune < 3.143) return (0.5);
    else if (pidetune < -3.14 && pidetune > -3.143) return (0.5);
    else return (sinpidetune/pidetune - 0.5 *
        (sinpidetune/(pidetune+pi) + sinpidetune/(pidetune-pi)));
}

static t_float peakerror(t_word *fpreal, t_word *fpimag, t_float pidetune,
    t_float norm, t_float peakreal, t_float peakimag)
{
    t_float sinpidetune = sin(pidetune);
    t_float cospidetune = cos(pidetune);
    t_float windowshould = hanning(pidetune, sinpidetune);
    t_float realshould = windowshould * (
        peakreal * cospidetune + peakimag * sinpidetune);
    t_float imagshould =  windowshould * (
        peakimag * cospidetune - peakreal * sinpidetune);
    t_float realgot = norm * (fpreal[0].w_float -
        0.5 * (fpreal[1].w_float + fpreal[-1].w_float));
    t_float imaggot = norm * (fpimag[0].w_float -
        0.5 * (fpimag[1].w_float + fpimag[-1].w_float));
    t_float realdev = realshould - realgot, imagdev = imagshould - imaggot;
    
    /* post("real %f->%f; imag %f->%f", realshould, realgot,
        imagshould, imaggot); */
    return (realdev * realdev + imagdev * imagdev);
}

static void pique_doit(int npts, t_word *fpreal, t_word *fpimag,
    int npeak, int *nfound, t_float *fpfreq, t_float *fpamp,
        t_float *fpampre, t_float *fpampim, t_float errthresh)
{
    t_float srate = sys_getsr();      /* not sure how to get this correctly */
    t_float oneovern = 1.0/ (t_float)npts;
    t_float fperbin = srate * oneovern;
    t_float pow1, pow2 = 0, pow3 = 0, pow4 = 0, pow5 = 0;
    t_float re1, re2 = 0, re3 = fpreal->w_float;
    t_float im1, im2 = 0, im3 = 0, powthresh, relativeerror;
    int count, peakcount = 0, n2 = (npts >> 1);
    t_float *fp1, *fp2;
    t_word *wp1, *wp2;
    for (count = n2, wp1 = fpreal, wp2 = fpimag, powthresh = 0;
        count--; wp1++, wp2++)
            powthresh += (wp1->w_float) * (wp1->w_float) +
                (wp2->w_float) * (wp2->w_float) ; 
    powthresh *= 0.00001;
    for (count = 1; count < n2; count++)
    {
        t_float windreal, windimag, pi = 3.141592653589793;
        t_float detune, pidetune, sinpidetune, cospidetune,
            ampcorrect, freqout, ampout, ampoutreal, ampoutimag;
        t_float rpeak, rpeaknext, rpeakprev;
        t_float ipeak, ipeaknext, ipeakprev;
        t_float errleft, errright;
        fpreal++;
        fpimag++;
        re1 = re2;
        re2 = re3;
        re3 = fpreal->w_float;
        im1 = im2;
        im2 = im3;
        im3 = fpimag->w_float;
        if (count < 2) continue;
        pow1 = pow2;
        pow2 = pow3;
        pow3 = pow4;
        pow4 = pow5;
            /* get Hanning-windowed spectrum by convolution */
        windreal = re2 - 0.5 * (re1 + re3);
        windimag = im2 - 0.5 * (im1 + im3);
        pow5 = windreal * windreal + windimag * windimag;
        /* if (count < 30) post("power %f", pow5); */
        if (count < 5) continue;
            /* check for a peak.  The actual bin is count-3. */
        if (pow3 <= pow2 || pow3 <= pow4 || pow3 <= pow1 || pow3 <= pow5
            || pow3 < powthresh)
                continue;
            /* go back for the raw FFT values around the peak. */
        rpeak = fpreal[-3].w_float;
        rpeaknext = fpreal[-2].w_float;
        rpeakprev = fpreal[-4].w_float;
        ipeak = fpimag[-3].w_float;
        ipeaknext = fpimag[-2].w_float;
        ipeakprev = fpimag[-4].w_float;
            /* recalculate Hanning-windowed spectrum by convolution */
        windreal = rpeak - 0.5 * (rpeaknext + rpeakprev);
        windimag = ipeak - 0.5 * (ipeaknext + ipeakprev);
        
        detune = ((rpeakprev - rpeaknext) *
            (2.0 * rpeak - rpeakprev - rpeaknext) +
                (ipeakprev - ipeaknext) *
                    (2.0 * ipeak - ipeakprev - ipeaknext)) /
                        (4.0 * pow3);
        /* if (count < 30) post("detune %f", detune); */
        if (detune > 0.7 || detune < -0.7) continue;
            /* the frequency is the sum of the bin frequency and detuning */ 
        freqout = fperbin * ((t_float)(count-3) + detune);
        pidetune = pi * detune;
        sinpidetune = sin(pidetune);
        cospidetune = cos(pidetune);
        ampcorrect = 1.0 / hanning(pidetune, sinpidetune);
                /* Multiply by 2 to get real-sinusoid peak amplitude 
                and divide by N to normalize FFT */
        ampcorrect *= 2. * oneovern;
            /* amplitude is peak height, corrected for Hanning window shape */

        ampout = ampcorrect * sqrt(pow3);
        ampoutreal = ampcorrect *
            (windreal * cospidetune - windimag * sinpidetune);
        ampoutimag = ampcorrect *
            (windreal * sinpidetune + windimag * cospidetune);
        if (errthresh > 0)
        {
            /* post("peak %f %f", freqout, ampout); */
            errleft = peakerror(fpreal-4, fpimag-4, pidetune+pi,
                2. * oneovern, ampoutreal, ampoutimag);
            errright = peakerror(fpreal-2, fpimag-2, pidetune-pi,
                2. * oneovern,  ampoutreal, ampoutimag);
            relativeerror = (errleft + errright)/(ampout * ampout);
            if (relativeerror > errthresh) continue;
        }
        /* post("power %f, error %f, relative %f",
            pow3, errleft + errright, relativeerror); */
        *fpfreq++ = freqout;
        *fpamp++ = ampout;
        *fpampre++ = ampoutreal;
        *fpampim++ = ampoutimag;
        if (++peakcount == npeak) break;
    }
    *nfound = peakcount;
}

static void pique_list(t_pique *x, t_symbol *s, int argc, t_atom *argv)
{
    int npts = atom_getintarg(0, argc, argv);
    t_symbol *symreal = atom_getsymbolarg(1, argc, argv);
    t_symbol *symimag = atom_getsymbolarg(2, argc, argv);
    int npeak = atom_getintarg(3, argc, argv);
    int n;
    t_garray *a;
    t_word *fpreal, *fpimag;
    if (npts < 8 || npeak < 1) error("pique: bad npoints or npeak");
    if (npeak > x->x_n) npeak = x->x_n;
    if (!(a = (t_garray *)pd_findbyclass(symreal, garray_class)) ||
        !garray_getfloatwords(a, &n, &fpreal) ||
            n < npts)
                error("%s: missing or bad array", symreal->s_name);
    else if (!(a = (t_garray *)pd_findbyclass(symimag, garray_class)) ||
        !garray_getfloatwords(a, &n, &fpimag) ||
            n < npts)
                error("%s: missing or bad array", symimag->s_name);
    else
    {
        int nfound, i;
        t_float *fpfreq = x->x_freq;
        t_float *fpamp = x->x_amp;
        t_float *fpampre = x->x_ampre;
        t_float *fpampim = x->x_ampim;
        pique_doit(npts, fpreal, fpimag, npeak,
            &nfound, fpfreq, fpamp, fpampre, fpampim, x->x_errthresh);
        for (i = 0; i < nfound; i++, fpamp++, fpfreq++, fpampre++, fpampim++)
        {
            t_atom at[5];
            SETFLOAT(at, (t_float)i);
            SETFLOAT(at+1, *fpfreq);
            SETFLOAT(at+2, *fpamp);
            SETFLOAT(at+3, *fpampre);
            SETFLOAT(at+4, *fpampim);
            outlet_list(x->x_obj.ob_outlet, &s_list, 5, at);   
        }
    }
}

static void pique_errthresh(t_pique *x, t_floatarg f)
{
    x->x_errthresh = f;
}

static void pique_free(t_pique *x)
{
    int n = x->x_n;
    t_freebytes(x->x_freq, n * sizeof(*x->x_freq));
    t_freebytes(x->x_amp, n * sizeof(*x->x_amp));
    t_freebytes(x->x_ampre, n * sizeof(*x->x_ampre));
    t_freebytes(x->x_ampim, n * sizeof(*x->x_ampim));
}

void pique_setup(void)
{
    pique_class = class_new(gensym("pique"), (t_newmethod)pique_new,
        (t_method)pique_free, sizeof(t_pique),0, A_DEFFLOAT, 0);
    class_addlist(pique_class, pique_list);
    class_addmethod(pique_class, (t_method)pique_errthresh,
        gensym("errthresh"), A_FLOAT, 0);
    post("pique 0.1 for PD version 23");
}

