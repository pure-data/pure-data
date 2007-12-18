/* Copyright (c) 1997- Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"

/* This file interfaces to one of the Mayer, Ooura, or fftw FFT packages
to implement the "fft~", etc, Pd objects.  If using Mayer, also compile
d_fft_mayer.c; if ooura, use d_fft_fftsg.c instead; if fftw, use d_fft_fftw.c
and also link in the fftw library.  You can only have one of these three
linked in.  The configure script can be used to select which one.
*/

/* ---------------- utility functions for DSP chains ---------------------- */

    /* swap two arrays */
static t_int *sigfft_swap(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    int n = w[3];
    for (;n--; in1++, in2++)
    {   
        t_sample f = *in1;
        *in1 = *in2;
        *in2 = f;
    }
    return (w+4);    
}

    /* take array1 (supply a pointer to beginning) and copy it,
    into decreasing addresses, into array 2 (supply a pointer one past the
    end), and negate the sign. */

static t_int *sigrfft_flip(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = w[3];
    while (n--)
        *(--out) = - *in++;
    return (w+4);
}

/* ------------------------ fft~ and ifft~ -------------------------------- */
static t_class *sigfft_class, *sigifft_class;

typedef struct fft
{
    t_object x_obj;
    t_float x_f;
} t_sigfft;

static void *sigfft_new(void)
{
    t_sigfft *x = (t_sigfft *)pd_new(sigfft_class);
    outlet_new(&x->x_obj, gensym("signal"));
    outlet_new(&x->x_obj, gensym("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_f = 0;
    return (x);
}

static void *sigifft_new(void)
{
    t_sigfft *x = (t_sigfft *)pd_new(sigifft_class);
    outlet_new(&x->x_obj, gensym("signal"));
    outlet_new(&x->x_obj, gensym("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_f = 0;
    return (x);
}

static t_int *sigfft_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    int n = w[3];
    mayer_fft(n, in1, in2);
    return (w+4);
}

static t_int *sigifft_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    int n = w[3];
    mayer_ifft(n, in1, in2);
    return (w+4);
}

static void sigfft_dspx(t_sigfft *x, t_signal **sp, t_int *(*f)(t_int *w))
{
    int n = sp[0]->s_n;
    t_sample *in1 = sp[0]->s_vec;
    t_sample *in2 = sp[1]->s_vec;
    t_sample *out1 = sp[2]->s_vec;
    t_sample *out2 = sp[3]->s_vec;
    if (out1 == in2 && out2 == in1)
        dsp_add(sigfft_swap, 3, out1, out2, n);
    else if (out1 == in2)
    {
        dsp_add(copy_perform, 3, in2, out2, n);
        dsp_add(copy_perform, 3, in1, out1, n);
    }
    else
    {
        if (out1 != in1) dsp_add(copy_perform, 3, in1, out1, n);
        if (out2 != in2) dsp_add(copy_perform, 3, in2, out2, n);
    }
    dsp_add(f, 3, sp[2]->s_vec, sp[3]->s_vec, n);
}

static void sigfft_dsp(t_sigfft *x, t_signal **sp)
{
    sigfft_dspx(x, sp, sigfft_perform);
}

static void sigifft_dsp(t_sigfft *x, t_signal **sp)
{
    sigfft_dspx(x, sp, sigifft_perform);
}

static void sigfft_setup(void)
{
    sigfft_class = class_new(gensym("fft~"), sigfft_new, 0,
        sizeof(t_sigfft), 0, 0);
    CLASS_MAINSIGNALIN(sigfft_class, t_sigfft, x_f);
    class_addmethod(sigfft_class, (t_method)sigfft_dsp, gensym("dsp"), 0);

    sigifft_class = class_new(gensym("ifft~"), sigifft_new, 0,
        sizeof(t_sigfft), 0, 0);
    CLASS_MAINSIGNALIN(sigifft_class, t_sigfft, x_f);
    class_addmethod(sigifft_class, (t_method)sigifft_dsp, gensym("dsp"), 0);
    class_sethelpsymbol(sigifft_class, gensym("fft~"));
}

/* ----------------------- rfft~ -------------------------------- */

static t_class *sigrfft_class;

typedef struct rfft
{
    t_object x_obj;
    t_float x_f;
} t_sigrfft;

static void *sigrfft_new(void)
{
    t_sigrfft *x = (t_sigrfft *)pd_new(sigrfft_class);
    outlet_new(&x->x_obj, gensym("signal"));
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *sigrfft_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    int n = w[2];
    mayer_realfft(n, in);
    return (w+3);
}

static void sigrfft_dsp(t_sigrfft *x, t_signal **sp)
{
    int n = sp[0]->s_n, n2 = (n>>1);
    t_sample *in1 = sp[0]->s_vec;
    t_sample *out1 = sp[1]->s_vec;
    t_sample *out2 = sp[2]->s_vec;
    if (n < 4)
    {
        error("fft: minimum 4 points");
        return;
    }
    if (in1 != out1)
        dsp_add(copy_perform, 3, in1, out1, n);
    dsp_add(sigrfft_perform, 2, out1, n);
    dsp_add(sigrfft_flip, 3, out1 + (n2+1), out2 + n2, n2-1);
    dsp_add_zero(out1 + (n2+1), ((n2-1)&(~7)));
    dsp_add_zero(out1 + (n2+1) + ((n2-1)&(~7)), ((n2-1)&7));
    dsp_add_zero(out2 + n2, n2);
    dsp_add_zero(out2, 1);
}

static void sigrfft_setup(void)
{
    sigrfft_class = class_new(gensym("rfft~"), sigrfft_new, 0,
        sizeof(t_sigrfft), 0, 0);
    CLASS_MAINSIGNALIN(sigrfft_class, t_sigrfft, x_f);
    class_addmethod(sigrfft_class, (t_method)sigrfft_dsp, gensym("dsp"), 0);
    class_sethelpsymbol(sigrfft_class, gensym("fft~"));
}

/* ----------------------- rifft~ -------------------------------- */

static t_class *sigrifft_class;

typedef struct rifft
{
    t_object x_obj;
    t_float x_f;
} t_sigrifft;

static void *sigrifft_new(void)
{
    t_sigrifft *x = (t_sigrifft *)pd_new(sigrifft_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *sigrifft_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    int n = w[2];
    mayer_realifft(n, in);
    return (w+3);
}

static void sigrifft_dsp(t_sigrifft *x, t_signal **sp)
{
    int n = sp[0]->s_n, n2 = (n>>1);
    t_sample *in1 = sp[0]->s_vec;
    t_sample *in2 = sp[1]->s_vec;
    t_sample *out1 = sp[2]->s_vec;
    if (n < 4)
    {
        error("fft: minimum 4 points");
        return;
    }
    if (in2 == out1)
    {
        dsp_add(sigrfft_flip, 3, out1+1, out1 + n, n2-1);
        dsp_add(copy_perform, 3, in1, out1, n2+1);
    }
    else
    {
        if (in1 != out1) dsp_add(copy_perform, 3, in1, out1, n2+1);
        dsp_add(sigrfft_flip, 3, in2+1, out1 + n, n2-1);
    }
    dsp_add(sigrifft_perform, 2, out1, n);
}

static void sigrifft_setup(void)
{
    sigrifft_class = class_new(gensym("rifft~"), sigrifft_new, 0,
        sizeof(t_sigrifft), 0, 0);
    CLASS_MAINSIGNALIN(sigrifft_class, t_sigrifft, x_f);
    class_addmethod(sigrifft_class, (t_method)sigrifft_dsp, gensym("dsp"), 0);
    class_sethelpsymbol(sigrifft_class, gensym("fft~"));
}

/* ----------------------- framp~ -------------------------------- */

static t_class *sigframp_class;

typedef struct framp
{
    t_object x_obj;
    t_float x_f;
} t_sigframp;

static void *sigframp_new(void)
{
    t_sigframp *x = (t_sigframp *)pd_new(sigframp_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, gensym("signal"));
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *sigframp_perform(t_int *w)
{
    t_sample *inreal = (t_sample *)(w[1]);
    t_sample *inimag = (t_sample *)(w[2]);
    t_sample *outfreq = (t_sample *)(w[3]);
    t_sample *outamp = (t_sample *)(w[4]);
    t_sample lastreal = 0, currentreal = inreal[0], nextreal = inreal[1];
    t_sample lastimag = 0, currentimag = inimag[0], nextimag = inimag[1];
    int n = w[5];
    int m = n + 1;
    t_sample fbin = 1, oneovern2 = 1.f/((t_sample)n * (t_sample)n);
    
    inreal += 2;
    inimag += 2;
    *outamp++ = *outfreq++ = 0;
    n -= 2;
    while (n--)
    {
        t_sample re, im, pow, freq;
        lastreal = currentreal;
        currentreal = nextreal;
        nextreal = *inreal++;
        lastimag = currentimag;
        currentimag = nextimag;
        nextimag = *inimag++;
        re = currentreal - 0.5f * (lastreal + nextreal);
        im = currentimag - 0.5f * (lastimag + nextimag);
        pow = re * re + im * im;
        if (pow > 1e-19)
        {
            t_sample detune = ((lastreal - nextreal) * re +
                    (lastimag - nextimag) * im) / (2.0f * pow);
            if (detune > 2 || detune < -2) freq = pow = 0;
            else freq = fbin + detune;
        }
        else freq = pow = 0;
        *outfreq++ = freq;
        *outamp++ = oneovern2 * pow;
        fbin += 1.0f;
    }
    while (m--) *outamp++ = *outfreq++ = 0;
    return (w+6);
}

t_int *sigsqrt_perform(t_int *w);

static void sigframp_dsp(t_sigframp *x, t_signal **sp)
{
    int n = sp[0]->s_n, n2 = (n>>1);
    if (n < 4)
    {
        error("framp: minimum 4 points");
        return;
    }
    dsp_add(sigframp_perform, 5, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, n2);
    dsp_add(sigsqrt_perform, 3, sp[3]->s_vec, sp[3]->s_vec, n2);
}

static void sigframp_setup(void)
{
    sigframp_class = class_new(gensym("framp~"), sigframp_new, 0,
        sizeof(t_sigframp), 0, 0);
    CLASS_MAINSIGNALIN(sigframp_class, t_sigframp, x_f);
    class_addmethod(sigframp_class, (t_method)sigframp_dsp, gensym("dsp"), 0);
}

/* ------------------------ global setup routine ------------------------- */

void d_fft_setup(void)
{
    sigfft_setup();
    sigrfft_setup();
    sigrifft_setup();
    sigframp_setup();
}
