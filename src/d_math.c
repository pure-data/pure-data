/* Copyright (c) 1997-2001 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  mathematical functions and other transfer functions, including tilde
    versions of stuff from x_acoustics.c.
*/

#include "m_pd.h"
#include <math.h>
#define LOGTEN 2.302585092994

/* ------------------------- clip~ -------------------------- */
static t_class *clip_class;

typedef struct _clip
{
    t_object x_obj;
    float x_f;
    t_sample x_lo;
    t_sample x_hi;
} t_clip;

static void *clip_new(t_floatarg lo, t_floatarg hi)
{
    t_clip *x = (t_clip *)pd_new(clip_class);
    x->x_lo = lo;
    x->x_hi = hi;
    outlet_new(&x->x_obj, gensym("signal"));
    floatinlet_new(&x->x_obj, &x->x_lo);
    floatinlet_new(&x->x_obj, &x->x_hi);
    x->x_f = 0;
    return (x);
}

static t_int *clip_perform(t_int *w)
{
    t_clip *x = (t_clip *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        float f = *in++;
        if (f < x->x_lo) f = x->x_lo;
        if (f > x->x_hi) f = x->x_hi;
        *out++ = f;
    }
    return (w+5);
}

static void clip_dsp(t_clip *x, t_signal **sp)
{
    dsp_add(clip_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

static void clip_setup(void)
{
    clip_class = class_new(gensym("clip~"), (t_newmethod)clip_new, 0,
        sizeof(t_clip), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(clip_class, t_clip, x_f);
    class_addmethod(clip_class, (t_method)clip_dsp, gensym("dsp"), 0);
}

/* sigrsqrt - reciprocal square root good to 8 mantissa bits  */

#define DUMTAB1SIZE 256
#define DUMTAB2SIZE 1024

#ifdef MSW
#define int32 long
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <sys/types.h>
#define int32 int32_t
#endif

static float rsqrt_exptab[DUMTAB1SIZE], rsqrt_mantissatab[DUMTAB2SIZE];

static void init_rsqrt(void)
{
    int i;
    for (i = 0; i < DUMTAB1SIZE; i++)
    {
        float f;
        int32 l = (i ? (i == DUMTAB1SIZE-1 ? DUMTAB1SIZE-2 : i) : 1)<< 23;
        *(int32 *)(&f) = l;
        rsqrt_exptab[i] = 1./sqrt(f);   
    }
    for (i = 0; i < DUMTAB2SIZE; i++)
    {
        float f = 1 + (1./DUMTAB2SIZE) * i;
        rsqrt_mantissatab[i] = 1./sqrt(f);      
    }
}

    /* these are used in externs like "bonk" */

float q8_rsqrt(float f)
{
    long l = *(long *)(&f);
    if (f < 0) return (0);
    else return (rsqrt_exptab[(l >> 23) & 0xff] *
            rsqrt_mantissatab[(l >> 13) & 0x3ff]);
}

float q8_sqrt(float f)
{
    long l = *(long *)(&f);
    if (f < 0) return (0);
    else return (f * rsqrt_exptab[(l >> 23) & 0xff] *
            rsqrt_mantissatab[(l >> 13) & 0x3ff]);
}

    /* the old names are OK unless we're in IRIX N32 */

#ifndef N32
float qsqrt(float f) {return (q8_sqrt(f)); }
float qrsqrt(float f) {return (q8_rsqrt(f)); }
#endif



typedef struct sigrsqrt
{
    t_object x_obj;
    float x_f;
} t_sigrsqrt;

static t_class *sigrsqrt_class;

static void *sigrsqrt_new(void)
{
    t_sigrsqrt *x = (t_sigrsqrt *)pd_new(sigrsqrt_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *sigrsqrt_perform(t_int *w)
{
    float *in = *(t_float **)(w+1), *out = *(t_float **)(w+2);
    t_int n = *(t_int *)(w+3);
    while (n--)
    {   
        float f = *in;
        long l = *(long *)(in++);
        if (f < 0) *out++ = 0;
        else
        {
            float g = rsqrt_exptab[(l >> 23) & 0xff] *
                rsqrt_mantissatab[(l >> 13) & 0x3ff];
            *out++ = 1.5 * g - 0.5 * g * g * g * f;
        }
    }
    return (w + 4);
}

static void sigrsqrt_dsp(t_sigrsqrt *x, t_signal **sp)
{
    dsp_add(sigrsqrt_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void sigrsqrt_setup(void)
{
    init_rsqrt();
    sigrsqrt_class = class_new(gensym("rsqrt~"), (t_newmethod)sigrsqrt_new, 0,
        sizeof(t_sigrsqrt), 0, 0);
            /* an old name for it: */
    class_addcreator(sigrsqrt_new, gensym("q8_rsqrt~"), 0);
    CLASS_MAINSIGNALIN(sigrsqrt_class, t_sigrsqrt, x_f);
    class_addmethod(sigrsqrt_class, (t_method)sigrsqrt_dsp, gensym("dsp"), 0);
}


/* sigsqrt -  square root good to 8 mantissa bits  */

typedef struct sigsqrt
{
    t_object x_obj;
    float x_f;
} t_sigsqrt;

static t_class *sigsqrt_class;

static void *sigsqrt_new(void)
{
    t_sigsqrt *x = (t_sigsqrt *)pd_new(sigsqrt_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

t_int *sigsqrt_perform(t_int *w)    /* not static; also used in d_fft.c */
{
    float *in = *(t_float **)(w+1), *out = *(t_float **)(w+2);
    t_int n = *(t_int *)(w+3);
    while (n--)
    {   
        float f = *in;
        long l = *(long *)(in++);
        if (f < 0) *out++ = 0;
        else
        {
            float g = rsqrt_exptab[(l >> 23) & 0xff] *
                rsqrt_mantissatab[(l >> 13) & 0x3ff];
            *out++ = f * (1.5 * g - 0.5 * g * g * g * f);
        }
    }
    return (w + 4);
}

static void sigsqrt_dsp(t_sigsqrt *x, t_signal **sp)
{
    dsp_add(sigsqrt_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void sigsqrt_setup(void)
{
    sigsqrt_class = class_new(gensym("sqrt~"), (t_newmethod)sigsqrt_new, 0,
        sizeof(t_sigsqrt), 0, 0);
    class_addcreator(sigsqrt_new, gensym("q8_sqrt~"), 0);   /* old name */
    CLASS_MAINSIGNALIN(sigsqrt_class, t_sigsqrt, x_f);
    class_addmethod(sigsqrt_class, (t_method)sigsqrt_dsp, gensym("dsp"), 0);
}

/* ------------------------------ wrap~ -------------------------- */

typedef struct wrap
{
    t_object x_obj;
    float x_f;
} t_sigwrap;

t_class *sigwrap_class;

static void *sigwrap_new(void)
{
    t_sigwrap *x = (t_sigwrap *)pd_new(sigwrap_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *sigwrap_perform(t_int *w)
{
    float *in = *(t_float **)(w+1), *out = *(t_float **)(w+2);
    t_int n = *(t_int *)(w+3);
    while (n--)
    {   
        float f = *in++;
        int k = f;
        if (f > 0) *out++ = f-k;
        else *out++ = f - (k-1);
    }
    return (w + 4);
}

static void sigwrap_dsp(t_sigwrap *x, t_signal **sp)
{
    dsp_add(sigwrap_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void sigwrap_setup(void)
{
    sigwrap_class = class_new(gensym("wrap~"), (t_newmethod)sigwrap_new, 0,
        sizeof(t_sigwrap), 0, 0);
    CLASS_MAINSIGNALIN(sigwrap_class, t_sigwrap, x_f);
    class_addmethod(sigwrap_class, (t_method)sigwrap_dsp, gensym("dsp"), 0);
}

/* ------------------------------ mtof_tilde~ -------------------------- */

typedef struct mtof_tilde
{
    t_object x_obj;
    float x_f;
} t_mtof_tilde;

t_class *mtof_tilde_class;

static void *mtof_tilde_new(void)
{
    t_mtof_tilde *x = (t_mtof_tilde *)pd_new(mtof_tilde_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *mtof_tilde_perform(t_int *w)
{
    float *in = *(t_float **)(w+1), *out = *(t_float **)(w+2);
    t_int n = *(t_int *)(w+3);
    for (; n--; in++, out++)
    {
        float f = *in;
        if (f <= -1500) *out = 0;
        else
        {
            if (f > 1499) f = 1499;
            *out = 8.17579891564 * exp(.0577622650 * f);
        }
    }
    return (w + 4);
}

static void mtof_tilde_dsp(t_mtof_tilde *x, t_signal **sp)
{
    dsp_add(mtof_tilde_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void mtof_tilde_setup(void)
{
    mtof_tilde_class = class_new(gensym("mtof~"), (t_newmethod)mtof_tilde_new, 0,
        sizeof(t_mtof_tilde), 0, 0);
    CLASS_MAINSIGNALIN(mtof_tilde_class, t_mtof_tilde, x_f);
    class_addmethod(mtof_tilde_class, (t_method)mtof_tilde_dsp, gensym("dsp"), 0);
}

/* ------------------------------ ftom_tilde~ -------------------------- */

typedef struct ftom_tilde
{
    t_object x_obj;
    float x_f;
} t_ftom_tilde;

t_class *ftom_tilde_class;

static void *ftom_tilde_new(void)
{
    t_ftom_tilde *x = (t_ftom_tilde *)pd_new(ftom_tilde_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *ftom_tilde_perform(t_int *w)
{
    float *in = *(t_float **)(w+1), *out = *(t_float **)(w+2);
    t_int n = *(t_int *)(w+3);
    for (; n--; *in++, out++)
    {
        float f = *in;
        *out = (f > 0 ? 17.3123405046 * log(.12231220585 * f) : -1500);
    }
    return (w + 4);
}

static void ftom_tilde_dsp(t_ftom_tilde *x, t_signal **sp)
{
    dsp_add(ftom_tilde_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void ftom_tilde_setup(void)
{
    ftom_tilde_class = class_new(gensym("ftom~"), (t_newmethod)ftom_tilde_new, 0,
        sizeof(t_ftom_tilde), 0, 0);
    CLASS_MAINSIGNALIN(ftom_tilde_class, t_ftom_tilde, x_f);
    class_addmethod(ftom_tilde_class, (t_method)ftom_tilde_dsp, gensym("dsp"), 0);
}

/* ------------------------------ dbtorms~ -------------------------- */

typedef struct dbtorms_tilde
{
    t_object x_obj;
    float x_f;
} t_dbtorms_tilde;

t_class *dbtorms_tilde_class;

static void *dbtorms_tilde_new(void)
{
    t_dbtorms_tilde *x = (t_dbtorms_tilde *)pd_new(dbtorms_tilde_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *dbtorms_tilde_perform(t_int *w)
{
    float *in = *(t_float **)(w+1), *out = *(t_float **)(w+2);
    t_int n = *(t_int *)(w+3);
    for (; n--; in++, out++)
    {
        float f = *in;
        if (f <= 0) *out = 0;
        else
        {
            if (f > 485)
                f = 485;
            *out = exp((LOGTEN * 0.05) * (f-100.));
        }
    }
    return (w + 4);
}

static void dbtorms_tilde_dsp(t_dbtorms_tilde *x, t_signal **sp)
{
    dsp_add(dbtorms_tilde_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void dbtorms_tilde_setup(void)
{
    dbtorms_tilde_class = class_new(gensym("dbtorms~"), (t_newmethod)dbtorms_tilde_new, 0,
        sizeof(t_dbtorms_tilde), 0, 0);
    CLASS_MAINSIGNALIN(dbtorms_tilde_class, t_dbtorms_tilde, x_f);
    class_addmethod(dbtorms_tilde_class, (t_method)dbtorms_tilde_dsp, gensym("dsp"), 0);
}

/* ------------------------------ rmstodb~ -------------------------- */

typedef struct rmstodb_tilde
{
    t_object x_obj;
    float x_f;
} t_rmstodb_tilde;

t_class *rmstodb_tilde_class;

static void *rmstodb_tilde_new(void)
{
    t_rmstodb_tilde *x = (t_rmstodb_tilde *)pd_new(rmstodb_tilde_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *rmstodb_tilde_perform(t_int *w)
{
    float *in = *(t_float **)(w+1), *out = *(t_float **)(w+2);
    t_int n = *(t_int *)(w+3);
    for (; n--; in++, out++)
    {
        float f = *in;
        if (f <= 0) *out = 0;
        else
        {
            float g = 100 + 20./LOGTEN * log(f);
            *out = (g < 0 ? 0 : g);
        }
    }
    return (w + 4);
}

static void rmstodb_tilde_dsp(t_rmstodb_tilde *x, t_signal **sp)
{
    dsp_add(rmstodb_tilde_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void rmstodb_tilde_setup(void)
{
    rmstodb_tilde_class = class_new(gensym("rmstodb~"), (t_newmethod)rmstodb_tilde_new, 0,
        sizeof(t_rmstodb_tilde), 0, 0);
    CLASS_MAINSIGNALIN(rmstodb_tilde_class, t_rmstodb_tilde, x_f);
    class_addmethod(rmstodb_tilde_class, (t_method)rmstodb_tilde_dsp, gensym("dsp"), 0);
}

/* ------------------------------ dbtopow~ -------------------------- */

typedef struct dbtopow_tilde
{
    t_object x_obj;
    float x_f;
} t_dbtopow_tilde;

t_class *dbtopow_tilde_class;

static void *dbtopow_tilde_new(void)
{
    t_dbtopow_tilde *x = (t_dbtopow_tilde *)pd_new(dbtopow_tilde_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *dbtopow_tilde_perform(t_int *w)
{
    float *in = *(t_float **)(w+1), *out = *(t_float **)(w+2);
    t_int n = *(t_int *)(w+3);
    for (; n--; in++, out++)
    {
        float f = *in;
        if (f <= 0) *out = 0;
        else
        {
            if (f > 870)
                f = 870;
            *out = exp((LOGTEN * 0.1) * (f-100.));
        }
    }
    return (w + 4);
}

static void dbtopow_tilde_dsp(t_dbtopow_tilde *x, t_signal **sp)
{
    dsp_add(dbtopow_tilde_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void dbtopow_tilde_setup(void)
{
    dbtopow_tilde_class = class_new(gensym("dbtopow~"), (t_newmethod)dbtopow_tilde_new, 0,
        sizeof(t_dbtopow_tilde), 0, 0);
    CLASS_MAINSIGNALIN(dbtopow_tilde_class, t_dbtopow_tilde, x_f);
    class_addmethod(dbtopow_tilde_class, (t_method)dbtopow_tilde_dsp, gensym("dsp"), 0);
}

/* ------------------------------ powtodb~ -------------------------- */

typedef struct powtodb_tilde
{
    t_object x_obj;
    float x_f;
} t_powtodb_tilde;

t_class *powtodb_tilde_class;

static void *powtodb_tilde_new(void)
{
    t_powtodb_tilde *x = (t_powtodb_tilde *)pd_new(powtodb_tilde_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *powtodb_tilde_perform(t_int *w)
{
    float *in = *(t_float **)(w+1), *out = *(t_float **)(w+2);
    t_int n = *(t_int *)(w+3);
    for (; n--; in++, out++)
    {
        float f = *in;
        if (f <= 0) *out = 0;
        else
        {
            float g = 100 + 10./LOGTEN * log(f);
            *out = (g < 0 ? 0 : g);
        }
    }
    return (w + 4);
}

static void powtodb_tilde_dsp(t_powtodb_tilde *x, t_signal **sp)
{
    dsp_add(powtodb_tilde_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void powtodb_tilde_setup(void)
{
    powtodb_tilde_class = class_new(gensym("powtodb~"), (t_newmethod)powtodb_tilde_new, 0,
        sizeof(t_powtodb_tilde), 0, 0);
    CLASS_MAINSIGNALIN(powtodb_tilde_class, t_powtodb_tilde, x_f);
    class_addmethod(powtodb_tilde_class, (t_method)powtodb_tilde_dsp, gensym("dsp"), 0);
}


/* ------------------------ global setup routine ------------------------- */

void d_math_setup(void)
{
    t_symbol *s = gensym("acoustics~.pd");
    clip_setup();
    sigrsqrt_setup();
    sigsqrt_setup();
    sigwrap_setup();
    mtof_tilde_setup();
    ftom_tilde_setup();
    dbtorms_tilde_setup();
    rmstodb_tilde_setup();
    dbtopow_tilde_setup();
    powtodb_tilde_setup();

    class_sethelpsymbol(mtof_tilde_class, s);
    class_sethelpsymbol(ftom_tilde_class, s);
    class_sethelpsymbol(dbtorms_tilde_class, s);
    class_sethelpsymbol(rmstodb_tilde_class, s);
    class_sethelpsymbol(dbtopow_tilde_class, s);
    class_sethelpsymbol(powtodb_tilde_class, s);
}

