/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  arithmetic binops (+, -, *, /).
If no creation argument is given, there are two signal inlets for vector/vector
operation; otherwise it's vector/scalar and the second inlet takes a float
to reset the value.
*/

#include "m_pd.h"

/* ----------------------------- plus ----------------------------- */
static t_class *plus_class, *scalarplus_class;

typedef struct _plus
{
    t_object x_obj;
    t_float x_f;
} t_plus;

typedef struct _scalarplus
{
    t_object x_obj;
    t_float x_f;
    t_float x_g;            /* inlet value */
} t_scalarplus;

static void *plus_new(t_symbol *s, int argc, t_atom *argv)
{
    if (argc > 1) post("+~: extra arguments ignored");
    if (argc)
    {
        t_scalarplus *x = (t_scalarplus *)pd_new(scalarplus_class);
        floatinlet_new(&x->x_obj, &x->x_g);
        x->x_g = atom_getfloatarg(0, argc, argv);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
    else
    {
        t_plus *x = (t_plus *)pd_new(plus_class);
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
}

t_int *plus_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--) *out++ = *in1++ + *in2++;
    return (w+5);
}

t_int *plus_perf8(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in1 += 8, in2 += 8, out += 8)
    {
        t_sample f0 = in1[0], f1 = in1[1], f2 = in1[2], f3 = in1[3];
        t_sample f4 = in1[4], f5 = in1[5], f6 = in1[6], f7 = in1[7];

        t_sample g0 = in2[0], g1 = in2[1], g2 = in2[2], g3 = in2[3];
        t_sample g4 = in2[4], g5 = in2[5], g6 = in2[6], g7 = in2[7];

        out[0] = f0 + g0; out[1] = f1 + g1; out[2] = f2 + g2; out[3] = f3 + g3;
        out[4] = f4 + g4; out[5] = f5 + g5; out[6] = f6 + g6; out[7] = f7 + g7;
    }
    return (w+5);
}

t_int *scalarplus_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float f = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--) *out++ = *in++ + f;
    return (w+5);
}

t_int *scalarplus_perf8(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float g = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in += 8, out += 8)
    {
        t_sample f0 = in[0], f1 = in[1], f2 = in[2], f3 = in[3];
        t_sample f4 = in[4], f5 = in[5], f6 = in[6], f7 = in[7];

        out[0] = f0 + g; out[1] = f1 + g; out[2] = f2 + g; out[3] = f3 + g;
        out[4] = f4 + g; out[5] = f5 + g; out[6] = f6 + g; out[7] = f7 + g;
    }
    return (w+5);
}

void dsp_add_plus(t_sample *in1, t_sample *in2, t_sample *out, int n)
{
    if (n&7)
        dsp_add(plus_perform, 4, in1, in2, out, n);
    else
        dsp_add(plus_perf8, 4, in1, in2, out, n);
}

static void plus_dsp(t_plus *x, t_signal **sp)
{
    dsp_add_plus(sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

static void scalarplus_dsp(t_scalarplus *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(scalarplus_perform, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
    else
        dsp_add(scalarplus_perf8, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
}

static void plus_setup(void)
{
    plus_class = class_new(gensym("+~"), (t_newmethod)plus_new, 0,
        sizeof(t_plus), 0, A_GIMME, 0);
    class_addmethod(plus_class, (t_method)plus_dsp, gensym("dsp"), A_CANT, 0);
    CLASS_MAINSIGNALIN(plus_class, t_plus, x_f);
    class_sethelpsymbol(plus_class, gensym("sigbinops"));
    scalarplus_class = class_new(gensym("+~"), 0, 0,
        sizeof(t_scalarplus), 0, 0);
    CLASS_MAINSIGNALIN(scalarplus_class, t_scalarplus, x_f);
    class_addmethod(scalarplus_class, (t_method)scalarplus_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarplus_class, gensym("sigbinops"));
}

/* ----------------------------- minus ----------------------------- */
static t_class *minus_class, *scalarminus_class;

typedef struct _minus
{
    t_object x_obj;
    t_float x_f;
} t_minus;

typedef struct _scalarminus
{
    t_object x_obj;
    t_float x_f;
    t_float x_g;
} t_scalarminus;

static void *minus_new(t_symbol *s, int argc, t_atom *argv)
{
    if (argc > 1) post("-~: extra arguments ignored");
    if (argc)
    {
        t_scalarminus *x = (t_scalarminus *)pd_new(scalarminus_class);
        floatinlet_new(&x->x_obj, &x->x_g);
        x->x_g = atom_getfloatarg(0, argc, argv);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
    else
    {
        t_minus *x = (t_minus *)pd_new(minus_class);
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
}

t_int *minus_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--) *out++ = *in1++ - *in2++;
    return (w+5);
}

t_int *minus_perf8(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in1 += 8, in2 += 8, out += 8)
    {
        t_sample f0 = in1[0], f1 = in1[1], f2 = in1[2], f3 = in1[3];
        t_sample f4 = in1[4], f5 = in1[5], f6 = in1[6], f7 = in1[7];

        t_sample g0 = in2[0], g1 = in2[1], g2 = in2[2], g3 = in2[3];
        t_sample g4 = in2[4], g5 = in2[5], g6 = in2[6], g7 = in2[7];

        out[0] = f0 - g0; out[1] = f1 - g1; out[2] = f2 - g2; out[3] = f3 - g3;
        out[4] = f4 - g4; out[5] = f5 - g5; out[6] = f6 - g6; out[7] = f7 - g7;
    }
    return (w+5);
}

t_int *scalarminus_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float f = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--) *out++ = *in++ - f;
    return (w+5);
}

t_int *scalarminus_perf8(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float g = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in += 8, out += 8)
    {
        t_sample f0 = in[0], f1 = in[1], f2 = in[2], f3 = in[3];
        t_sample f4 = in[4], f5 = in[5], f6 = in[6], f7 = in[7];

        out[0] = f0 - g; out[1] = f1 - g; out[2] = f2 - g; out[3] = f3 - g;
        out[4] = f4 - g; out[5] = f5 - g; out[6] = f6 - g; out[7] = f7 - g;
    }
    return (w+5);
}

static void minus_dsp(t_minus *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(minus_perform, 4,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
    else
        dsp_add(minus_perf8, 4,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

static void scalarminus_dsp(t_scalarminus *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(scalarminus_perform, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
    else
        dsp_add(scalarminus_perf8, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
}

static void minus_setup(void)
{
    minus_class = class_new(gensym("-~"), (t_newmethod)minus_new, 0,
        sizeof(t_minus), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(minus_class, t_minus, x_f);
    class_addmethod(minus_class, (t_method)minus_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(minus_class, gensym("sigbinops"));
    scalarminus_class = class_new(gensym("-~"), 0, 0,
        sizeof(t_scalarminus), 0, 0);
    CLASS_MAINSIGNALIN(scalarminus_class, t_scalarminus, x_f);
    class_addmethod(scalarminus_class, (t_method)scalarminus_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarminus_class, gensym("sigbinops"));
}

/* ----------------------------- times ----------------------------- */

static t_class *times_class, *scalartimes_class;

typedef struct _times
{
    t_object x_obj;
    t_float x_f;
} t_times;

typedef struct _scalartimes
{
    t_object x_obj;
    t_float x_f;
    t_float x_g;
} t_scalartimes;

static void *times_new(t_symbol *s, int argc, t_atom *argv)
{
    if (argc > 1) post("*~: extra arguments ignored");
    if (argc)
    {
        t_scalartimes *x = (t_scalartimes *)pd_new(scalartimes_class);
        floatinlet_new(&x->x_obj, &x->x_g);
        x->x_g = atom_getfloatarg(0, argc, argv);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
    else
    {
        t_times *x = (t_times *)pd_new(times_class);
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
}

t_int *times_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--) *out++ = *in1++ * *in2++;
    return (w+5);
}

t_int *times_perf8(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in1 += 8, in2 += 8, out += 8)
    {
        t_sample f0 = in1[0], f1 = in1[1], f2 = in1[2], f3 = in1[3];
        t_sample f4 = in1[4], f5 = in1[5], f6 = in1[6], f7 = in1[7];

        t_sample g0 = in2[0], g1 = in2[1], g2 = in2[2], g3 = in2[3];
        t_sample g4 = in2[4], g5 = in2[5], g6 = in2[6], g7 = in2[7];

        out[0] = f0 * g0; out[1] = f1 * g1; out[2] = f2 * g2; out[3] = f3 * g3;
        out[4] = f4 * g4; out[5] = f5 * g5; out[6] = f6 * g6; out[7] = f7 * g7;
    }
    return (w+5);
}

t_int *scalartimes_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float f = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--) *out++ = *in++ * f;
    return (w+5);
}

t_int *scalartimes_perf8(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float g = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in += 8, out += 8)
    {
        t_sample f0 = in[0], f1 = in[1], f2 = in[2], f3 = in[3];
        t_sample f4 = in[4], f5 = in[5], f6 = in[6], f7 = in[7];

        out[0] = f0 * g; out[1] = f1 * g; out[2] = f2 * g; out[3] = f3 * g;
        out[4] = f4 * g; out[5] = f5 * g; out[6] = f6 * g; out[7] = f7 * g;
    }
    return (w+5);
}

static void times_dsp(t_times *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(times_perform, 4,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
    else
        dsp_add(times_perf8, 4,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

static void scalartimes_dsp(t_scalartimes *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(scalartimes_perform, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
    else
        dsp_add(scalartimes_perf8, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
}

static void times_setup(void)
{
    times_class = class_new(gensym("*~"), (t_newmethod)times_new, 0,
        sizeof(t_times), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(times_class, t_times, x_f);
    class_addmethod(times_class, (t_method)times_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(times_class, gensym("sigbinops"));
    scalartimes_class = class_new(gensym("*~"), 0, 0,
        sizeof(t_scalartimes), 0, 0);
    CLASS_MAINSIGNALIN(scalartimes_class, t_scalartimes, x_f);
    class_addmethod(scalartimes_class, (t_method)scalartimes_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalartimes_class, gensym("sigbinops"));
}

/* ----------------------------- over ----------------------------- */
static t_class *over_class, *scalarover_class;

typedef struct _over
{
    t_object x_obj;
    t_float x_f;
} t_over;

typedef struct _scalarover
{
    t_object x_obj;
    t_float x_f;
    t_float x_g;
} t_scalarover;

static void *over_new(t_symbol *s, int argc, t_atom *argv)
{
    if (argc > 1) post("/~: extra arguments ignored");
    if (argc)
    {
        t_scalarover *x = (t_scalarover *)pd_new(scalarover_class);
        floatinlet_new(&x->x_obj, &x->x_g);
        x->x_g = atom_getfloatarg(0, argc, argv);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
    else
    {
        t_over *x = (t_over *)pd_new(over_class);
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
}

t_int *over_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample g = *in2++;
        *out++ = (g ? *in1++ / g : 0);
    }
    return (w+5);
}

t_int *over_perf8(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in1 += 8, in2 += 8, out += 8)
    {
        t_sample f0 = in1[0], f1 = in1[1], f2 = in1[2], f3 = in1[3];
        t_sample f4 = in1[4], f5 = in1[5], f6 = in1[6], f7 = in1[7];

        t_sample g0 = in2[0], g1 = in2[1], g2 = in2[2], g3 = in2[3];
        t_sample g4 = in2[4], g5 = in2[5], g6 = in2[6], g7 = in2[7];

        out[0] = (g0? f0 / g0 : 0);
        out[1] = (g1? f1 / g1 : 0);
        out[2] = (g2? f2 / g2 : 0);
        out[3] = (g3? f3 / g3 : 0);
        out[4] = (g4? f4 / g4 : 0);
        out[5] = (g5? f5 / g5 : 0);
        out[6] = (g6? f6 / g6 : 0);
        out[7] = (g7? f7 / g7 : 0);
    }
    return (w+5);
}

t_int *scalarover_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float f = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    if(f) f = 1./f;
    while (n--) *out++ = *in++ * f;
    return (w+5);
}

t_int *scalarover_perf8(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float g = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    if (g) g = 1.f / g;
    for (; n; n -= 8, in += 8, out += 8)
    {
        t_sample f0 = in[0], f1 = in[1], f2 = in[2], f3 = in[3];
        t_sample f4 = in[4], f5 = in[5], f6 = in[6], f7 = in[7];

        out[0] = f0 * g; out[1] = f1 * g; out[2] = f2 * g; out[3] = f3 * g;
        out[4] = f4 * g; out[5] = f5 * g; out[6] = f6 * g; out[7] = f7 * g;
    }
    return (w+5);
}

static void over_dsp(t_over *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(over_perform, 4,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
    else
        dsp_add(over_perf8, 4,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

static void scalarover_dsp(t_scalarover *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(scalarover_perform, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
    else
        dsp_add(scalarover_perf8, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
}

static void over_setup(void)
{
    over_class = class_new(gensym("/~"), (t_newmethod)over_new, 0,
        sizeof(t_over), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(over_class, t_over, x_f);
    class_addmethod(over_class, (t_method)over_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(over_class, gensym("sigbinops"));
    scalarover_class = class_new(gensym("/~"), 0, 0,
        sizeof(t_scalarover), 0, 0);
    CLASS_MAINSIGNALIN(scalarover_class, t_scalarover, x_f);
    class_addmethod(scalarover_class, (t_method)scalarover_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarover_class, gensym("sigbinops"));
}

/* ----------------------------- max ----------------------------- */
static t_class *max_class, *scalarmax_class;

typedef struct _max
{
    t_object x_obj;
    t_float x_f;
} t_max;

typedef struct _scalarmax
{
    t_object x_obj;
    t_float x_f;
    t_float x_g;
} t_scalarmax;

static void *max_new(t_symbol *s, int argc, t_atom *argv)
{
    if (argc > 1) post("max~: extra arguments ignored");
    if (argc)
    {
        t_scalarmax *x = (t_scalarmax *)pd_new(scalarmax_class);
        floatinlet_new(&x->x_obj, &x->x_g);
        x->x_g = atom_getfloatarg(0, argc, argv);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
    else
    {
        t_max *x = (t_max *)pd_new(max_class);
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
}

t_int *max_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample f = *in1++, g = *in2++;
        *out++ = (f > g ? f : g);
    }
    return (w+5);
}

t_int *max_perf8(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in1 += 8, in2 += 8, out += 8)
    {
        t_sample f0 = in1[0], f1 = in1[1], f2 = in1[2], f3 = in1[3];
        t_sample f4 = in1[4], f5 = in1[5], f6 = in1[6], f7 = in1[7];

        t_sample g0 = in2[0], g1 = in2[1], g2 = in2[2], g3 = in2[3];
        t_sample g4 = in2[4], g5 = in2[5], g6 = in2[6], g7 = in2[7];

        out[0] = (f0 > g0 ? f0 : g0); out[1] = (f1 > g1 ? f1 : g1);
        out[2] = (f2 > g2 ? f2 : g2); out[3] = (f3 > g3 ? f3 : g3);
        out[4] = (f4 > g4 ? f4 : g4); out[5] = (f5 > g5 ? f5 : g5);
        out[6] = (f6 > g6 ? f6 : g6); out[7] = (f7 > g7 ? f7 : g7);
    }
    return (w+5);
}

t_int *scalarmax_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float f = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample g = *in++;
        *out++ = (f > g ? f : g);
    }
    return (w+5);
}

t_int *scalarmax_perf8(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float g = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in += 8, out += 8)
    {
        t_sample f0 = in[0], f1 = in[1], f2 = in[2], f3 = in[3];
        t_sample f4 = in[4], f5 = in[5], f6 = in[6], f7 = in[7];

        out[0] = (f0 > g ? f0 : g); out[1] = (f1 > g ? f1 : g);
        out[2] = (f2 > g ? f2 : g); out[3] = (f3 > g ? f3 : g);
        out[4] = (f4 > g ? f4 : g); out[5] = (f5 > g ? f5 : g);
        out[6] = (f6 > g ? f6 : g); out[7] = (f7 > g ? f7 : g);
    }
    return (w+5);
}

static void max_dsp(t_max *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(max_perform, 4,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
    else
        dsp_add(max_perf8, 4,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

static void scalarmax_dsp(t_scalarmax *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(scalarmax_perform, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
    else
        dsp_add(scalarmax_perf8, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
}

static void max_setup(void)
{
    max_class = class_new(gensym("max~"), (t_newmethod)max_new, 0,
        sizeof(t_max), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(max_class, t_max, x_f);
    class_addmethod(max_class, (t_method)max_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(max_class, gensym("sigbinops"));
    scalarmax_class = class_new(gensym("max~"), 0, 0,
        sizeof(t_scalarmax), 0, 0);
    CLASS_MAINSIGNALIN(scalarmax_class, t_scalarmax, x_f);
    class_addmethod(scalarmax_class, (t_method)scalarmax_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarmax_class, gensym("sigbinops"));
}

/* ----------------------------- min ----------------------------- */
static t_class *min_class, *scalarmin_class;

typedef struct _min
{
    t_object x_obj;
    t_float x_f;
} t_min;

typedef struct _scalarmin
{
    t_object x_obj;
    t_float x_g;
    t_float x_f;
} t_scalarmin;

static void *min_new(t_symbol *s, int argc, t_atom *argv)
{
    if (argc > 1) post("min~: extra arguments ignored");
    if (argc)
    {
        t_scalarmin *x = (t_scalarmin *)pd_new(scalarmin_class);
        floatinlet_new(&x->x_obj, &x->x_g);
        x->x_g = atom_getfloatarg(0, argc, argv);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
    else
    {
        t_min *x = (t_min *)pd_new(min_class);
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
}

t_int *min_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample f = *in1++, g = *in2++;
        *out++ = (f < g ? f : g);
    }
    return (w+5);
}

t_int *min_perf8(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in1 += 8, in2 += 8, out += 8)
    {
        t_sample f0 = in1[0], f1 = in1[1], f2 = in1[2], f3 = in1[3];
        t_sample f4 = in1[4], f5 = in1[5], f6 = in1[6], f7 = in1[7];

        t_sample g0 = in2[0], g1 = in2[1], g2 = in2[2], g3 = in2[3];
        t_sample g4 = in2[4], g5 = in2[5], g6 = in2[6], g7 = in2[7];

        out[0] = (f0 < g0 ? f0 : g0); out[1] = (f1 < g1 ? f1 : g1);
        out[2] = (f2 < g2 ? f2 : g2); out[3] = (f3 < g3 ? f3 : g3);
        out[4] = (f4 < g4 ? f4 : g4); out[5] = (f5 < g5 ? f5 : g5);
        out[6] = (f6 < g6 ? f6 : g6); out[7] = (f7 < g7 ? f7 : g7);
    }
    return (w+5);
}

t_int *scalarmin_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float f = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample g = *in++;
        *out++ = (f < g ? f : g);
    }
    return (w+5);
}

t_int *scalarmin_perf8(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float g = *(t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in += 8, out += 8)
    {
        t_sample f0 = in[0], f1 = in[1], f2 = in[2], f3 = in[3];
        t_sample f4 = in[4], f5 = in[5], f6 = in[6], f7 = in[7];

        out[0] = (f0 < g ? f0 : g); out[1] = (f1 < g ? f1 : g);
        out[2] = (f2 < g ? f2 : g); out[3] = (f3 < g ? f3 : g);
        out[4] = (f4 < g ? f4 : g); out[5] = (f5 < g ? f5 : g);
        out[6] = (f6 < g ? f6 : g); out[7] = (f7 < g ? f7 : g);
    }
    return (w+5);
}

static void min_dsp(t_min *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(min_perform, 4,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
    else
        dsp_add(min_perf8, 4,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

static void scalarmin_dsp(t_scalarmin *x, t_signal **sp)
{
    if (sp[0]->s_n&7)
        dsp_add(scalarmin_perform, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
    else
        dsp_add(scalarmin_perf8, 4, sp[0]->s_vec, &x->x_g,
            sp[1]->s_vec, sp[0]->s_n);
}

static void min_setup(void)
{
    min_class = class_new(gensym("min~"), (t_newmethod)min_new, 0,
        sizeof(t_min), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(min_class, t_min, x_f);
    class_addmethod(min_class, (t_method)min_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(min_class, gensym("sigbinops"));
    scalarmin_class = class_new(gensym("min~"), 0, 0,
        sizeof(t_scalarmin), 0, 0);
    CLASS_MAINSIGNALIN(scalarmin_class, t_scalarmin, x_f);
    class_addmethod(scalarmin_class, (t_method)scalarmin_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarmin_class, gensym("sigbinops"));
}

/* ----------------------- global setup routine ---------------- */
void d_arithmetic_setup(void)
{
    plus_setup();
    minus_setup();
    times_setup();
    over_setup();
    max_setup();
    min_setup();
}

