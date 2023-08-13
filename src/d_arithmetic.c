/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  arithmetic binops (+, -, *, /).
If no creation argument is given, there are two signal inlets for vector/vector
operation; otherwise it's vector/scalar and the second inlet takes a float
to reset the value.
*/

#include "m_pd.h"
#include <math.h> /* needed for log~ */

/* -------------- convenience routines for multichannel binops ----- */

    /* add a binary operation (such as "+") to the DSP chain, in
    which inputs may be of different sizes (but the output should have
    the same size as the larger input).  Whichever is shorter gets re-used
    as many times as necessary to match the longer one; so one can add signals
    with different numbers of channels. Two functions are passed, a general
    one and another that is called if the block size is a multiple of 8. */

static void dsp_add_multi(t_sample *vec1, int n1, t_sample *vec2,
    int n2, t_sample *outvec, t_perfroutine func, t_perfroutine func8)
{
    int i;
    if (n1 > n2)
        for (i = (n1+n2-1)/n2; i--; )
    {
        t_int blocksize = (n2 < n1 - i*n2 ?
            n2 : n1 - i*n2);
        dsp_add(((blocksize & 7) || !func8 ? func : func8), 4,
            vec1 + i * n2, vec2, outvec + i * n2, blocksize);
    }
    else for (i = (n1+n2-1)/n1; i--; )
    {
        t_int blocksize = (n1 < n2 - i*n1 ?
            n1 : n2 - i*n1);
        dsp_add(((blocksize & 7) || !func8 ? func : func8), 4,
            vec1, vec2 + i*n1, outvec + i*n1, blocksize);
    }
}

    /* more generally, add a binary operation to the DSP chain, that
    may be vector or scalar in either of its two inputs.  The caller
    supplies three versions: vector-vector, vector-scalar, and scalar-vector.
    In the scalar-vector case the "perf" routine gets the vector argument
    first (on the DSP chain) - this way, if the operation is commutative
    the same function can be supplied for perf_vs and perf_vs_reverse.
    If perf_vv8, etc, are nonzero, they are called if the vector size is a
    multiple of 8.  */
static void any_binop_dsp(t_signal **sp,
    t_perfroutine perf_vv, t_perfroutine perf_vv8,
    t_perfroutine perf_vs, t_perfroutine perf_vs8,
    t_perfroutine perf_vs_reverse, t_perfroutine perf_vs8_reverse)
{
    int bign0 = sp[0]->s_length * sp[0]->s_nchans,
        bign1 = sp[1]->s_length * sp[1]->s_nchans, outchans;
    if (bign1 > bign0)
        outchans = sp[1]->s_nchans;
    else if (bign0 > 1)
        outchans = sp[0]->s_nchans;
    else outchans = 1;
    signal_setmultiout(&sp[2], outchans);
    if (bign0 > 1) /* first input is a vector */
    {
        if (bign1 > 1)
                    /* general case: both inputs are vectors */
            dsp_add_multi(sp[0]->s_vec, bign0, sp[1]->s_vec, bign1,
                sp[2]->s_vec, perf_vv, perf_vv8);
                    /* add a scalar to a vector */
        else dsp_add(((bign0 & 7) || !perf_vs8 ? perf_vs : perf_vs8),
            4, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, (t_int)bign0);
    }
    else /* first input is scalar */
    {
        if (bign1 > 1)
                /* second input is a vector: use reverse scalar version */
            dsp_add(((bign0 & 7) || !perf_vs8_reverse ?
                perf_vs_reverse : perf_vs8_reverse),
                    4, sp[1]->s_vec, sp[0]->s_vec, sp[2]->s_vec, (t_int)bign1);
        else
        {
                /* operate on two scalars to make a vector, needing two ops */
            dsp_add(perf_vs, 4,
                sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, (t_int)1);
            dsp_add_scalarcopy(sp[2]->s_vec, sp[2]->s_vec,
                (t_int)sp[2]->s_length);
        }
    }
}

    /* vector-scalar version (as in "+~ 1") - here we don't deal with
    scalar left inputs - the class may not be declared CLASS_NOPROMOTELEFT. */
static void any_binop_scalar_dsp(t_signal **sp, t_sample *g,
    t_perfroutine perf, t_perfroutine perf8)
{
    t_int bign = sp[0]->s_length * sp[0]->s_nchans;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(((bign & 7) || !perf8 ? perf : perf8),
        4, sp[0]->s_vec, g, sp[1]->s_vec, bign);
}


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
    if (argc > 1)
        post("+~: extra arguments ignored");
    if (argc)   /* argument implies we'll do a scalar add as in "+~ 1" */
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

static void plus_dsp(t_plus *x, t_signal **sp)
{
    any_binop_dsp(sp, plus_perform, plus_perf8,
        scalarplus_perform, scalarplus_perf8,
        scalarplus_perform, scalarplus_perf8);
}

static void scalarplus_dsp(t_scalarplus *x, t_signal **sp)
{
    any_binop_scalar_dsp(sp, &x->x_g, scalarplus_perform, scalarplus_perf8);
}

static void plus_setup(void)
{
    plus_class = class_new(gensym("+~"), (t_newmethod)plus_new, 0,
        sizeof(t_plus),
            CLASS_MULTICHANNEL | CLASS_NOPROMOTESIG | CLASS_NOPROMOTELEFT,
                A_GIMME, 0);
    class_addmethod(plus_class, (t_method)plus_dsp, gensym("dsp"), A_CANT, 0);
    CLASS_MAINSIGNALIN(plus_class, t_plus, x_f);
    class_sethelpsymbol(plus_class, gensym("binops-tilde"));
    scalarplus_class = class_new(gensym("+~"), 0, 0,
        sizeof(t_scalarplus), CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(scalarplus_class, t_scalarplus, x_f);
    class_addmethod(scalarplus_class, (t_method)scalarplus_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarplus_class, gensym("binops-tilde"));
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

t_int *reversescalarminus_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float f = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--) *out++ = f - *in++;
    return (w+5);
}

t_int *reversescalarminus_perf8(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float g = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in += 8, out += 8)
    {
        t_sample f0 = in[0], f1 = in[1], f2 = in[2], f3 = in[3];
        t_sample f4 = in[4], f5 = in[5], f6 = in[6], f7 = in[7];

        out[0] = g - f0; out[1] = g - f1; out[2] = g - f2; out[3] = g - f3;
        out[4] = g - f4; out[5] = g - f5; out[6] = g - f6; out[7] = g - f7;
    }
    return (w+5);
}

static void minus_dsp(t_minus *x, t_signal **sp)
{
    any_binop_dsp(sp, minus_perform, minus_perf8,
        scalarminus_perform, scalarminus_perf8,
        reversescalarminus_perform, reversescalarminus_perf8);
}

static void scalarminus_dsp(t_scalarminus *x, t_signal **sp)
{
    any_binop_scalar_dsp(sp, &x->x_g, scalarminus_perform, scalarminus_perf8);
}

static void minus_setup(void)
{
    minus_class = class_new(gensym("-~"), (t_newmethod)minus_new, 0,
        sizeof(t_minus),
            CLASS_MULTICHANNEL | CLASS_NOPROMOTESIG | CLASS_NOPROMOTELEFT,
                A_GIMME, 0);
    CLASS_MAINSIGNALIN(minus_class, t_minus, x_f);
    class_addmethod(minus_class, (t_method)minus_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(minus_class, gensym("binops-tilde"));
    scalarminus_class = class_new(gensym("-~"), 0, 0,
        sizeof(t_scalarminus), CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(scalarminus_class, t_scalarminus, x_f);
    class_addmethod(scalarminus_class, (t_method)scalarminus_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarminus_class, gensym("binops-tilde"));
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
    any_binop_dsp(sp, times_perform, times_perf8,
        scalartimes_perform, scalartimes_perf8,
        scalartimes_perform, scalartimes_perf8);
}

static void scalartimes_dsp(t_scalartimes *x, t_signal **sp)
{
    any_binop_scalar_dsp(sp, &x->x_g, scalartimes_perform, scalartimes_perf8);
}

static void times_setup(void)
{
    times_class = class_new(gensym("*~"), (t_newmethod)times_new, 0,
        sizeof(t_times),
            CLASS_MULTICHANNEL | CLASS_NOPROMOTESIG | CLASS_NOPROMOTELEFT,
                A_GIMME, 0);
    CLASS_MAINSIGNALIN(times_class, t_times, x_f);
    class_addmethod(times_class, (t_method)times_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(times_class, gensym("binops-tilde"));
    scalartimes_class = class_new(gensym("*~"), 0, 0,
        sizeof(t_scalartimes), CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(scalartimes_class, t_scalartimes, x_f);
    class_addmethod(scalartimes_class, (t_method)scalartimes_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalartimes_class, gensym("binops-tilde"));
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
        t_sample f = *in1++, g = *in2++;
        *out++ = (g ? f / g : 0);
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

t_int *reversescalarover_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float f = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample g = *in++;
        *out++ = (g != 0 ? f/g : 0);
    }
    return (w+5);
}

t_int *reversescalarover_perf8(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_float g = *(t_float *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    for (; n; n -= 8, in += 8, out += 8)
    {
        t_sample f0 = in[0], f1 = in[1], f2 = in[2], f3 = in[3];
        t_sample f4 = in[4], f5 = in[5], f6 = in[6], f7 = in[7];

        out[0] = (f0 != 0 ? g/f0 : 0); out[1] = (f1 != 0 ? g/f1 : 0);
        out[2] = (f2 != 0 ? g/f2 : 0); out[3] = (f3 != 0 ? g/f3 : 0);
        out[4] = (f4 != 0 ? g/f4 : 0); out[5] = (f5 != 0 ? g/f5 : 0);
        out[6] = (f6 != 0 ? g/f6 : 0); out[7] = (f7 != 0 ? g/f7 : 0);
    }
    return (w+5);
}


static void over_dsp(t_over *x, t_signal **sp)
{
    any_binop_dsp(sp, over_perform, over_perf8,
        scalarover_perform, scalarover_perf8,
        reversescalarover_perform, reversescalarover_perf8);
}

static void scalarover_dsp(t_scalarover *x, t_signal **sp)
{
    any_binop_scalar_dsp(sp, &x->x_g, scalarover_perform, scalarover_perf8);
}

static void over_setup(void)
{
    over_class = class_new(gensym("/~"), (t_newmethod)over_new, 0,
        sizeof(t_over),
            CLASS_MULTICHANNEL | CLASS_NOPROMOTESIG | CLASS_NOPROMOTELEFT,
                A_GIMME, 0);
    CLASS_MAINSIGNALIN(over_class, t_over, x_f);
    class_addmethod(over_class, (t_method)over_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(over_class, gensym("binops-tilde"));
    scalarover_class = class_new(gensym("/~"), 0, 0,
        sizeof(t_scalarover), CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(scalarover_class, t_scalarover, x_f);
    class_addmethod(scalarover_class, (t_method)scalarover_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarover_class, gensym("binops-tilde"));
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
    any_binop_dsp(sp, max_perform, max_perf8,
        scalarmax_perform, scalarmax_perf8,
        scalarmax_perform, scalarmax_perf8);
}

static void scalarmax_dsp(t_scalarmax *x, t_signal **sp)
{
    any_binop_scalar_dsp(sp, &x->x_g, scalarmax_perform, scalarmax_perf8);
}

static void max_setup(void)
{
    max_class = class_new(gensym("max~"), (t_newmethod)max_new, 0,
        sizeof(t_max),
            CLASS_MULTICHANNEL | CLASS_NOPROMOTESIG | CLASS_NOPROMOTELEFT,
                A_GIMME, 0);
    CLASS_MAINSIGNALIN(max_class, t_max, x_f);
    class_addmethod(max_class, (t_method)max_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(max_class, gensym("binops-tilde"));
    scalarmax_class = class_new(gensym("max~"), 0, 0,
        sizeof(t_scalarmax), CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(scalarmax_class, t_scalarmax, x_f);
    class_addmethod(scalarmax_class, (t_method)scalarmax_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarmax_class, gensym("binops-tilde"));
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
    t_sample *out = (t_sample *)(w[3]);
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
    any_binop_dsp(sp, min_perform, min_perf8,
        scalarmin_perform, scalarmin_perf8,
        scalarmin_perform, scalarmin_perf8);
}

static void scalarmin_dsp(t_scalarmin *x, t_signal **sp)
{
    any_binop_scalar_dsp(sp, &x->x_g, scalarmin_perform, scalarmin_perf8);
}

static void min_setup(void)
{
    min_class = class_new(gensym("min~"), (t_newmethod)min_new, 0,
        sizeof(t_min),
            CLASS_MULTICHANNEL | CLASS_NOPROMOTESIG | CLASS_NOPROMOTELEFT,
                A_GIMME, 0);
    CLASS_MAINSIGNALIN(min_class, t_min, x_f);
    class_addmethod(min_class, (t_method)min_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(min_class, gensym("binops-tilde"));
    scalarmin_class = class_new(gensym("min~"), 0, 0,
        sizeof(t_scalarmin), CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(scalarmin_class, t_scalarmin, x_f);
    class_addmethod(scalarmin_class, (t_method)scalarmin_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarmin_class, gensym("binops-tilde"));
}

/* ----------------------------- log ----------------------------- */
static t_class *log_tilde_class, *scalarlog_tilde_class;

typedef struct _log_tilde
{
    t_object x_obj;
    t_float x_f;
} t_log_tilde;

typedef struct _scalarlog_tilde
{
    t_object x_obj;
    t_float x_f;
    t_float x_g;
} t_scalarlog_tilde;

static void *log_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    if (argc > 1) post("-~: extra arguments ignored");
    if (argc)
    {
        t_scalarlog_tilde *x =
            (t_scalarlog_tilde *)pd_new(scalarlog_tilde_class);
        floatinlet_new(&x->x_obj, &x->x_g);
        x->x_g = atom_getfloatarg(0, argc, argv);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
    else
    {
        t_log_tilde *x = (t_log_tilde *)pd_new(log_tilde_class);
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
}

t_int *log_tilde_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample f = *in1++, g = *in2++;
        if (f <= 0)
            *out = -1000;   /* rather than blow up, output a number << 0 */
        else if (g == 1 || g <= 0)
            *out = log(f);
        else *out = log(f)/log(g);
        out++;
    }
    return (w+5);
}

t_int *log_tilde_perform_scalar(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample base = *(t_sample *)(w[2]),
        multiplier = ((base > 0 && base != 1) ? 1./log(base) : 1);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample f = *in1++;
        if (f <= 0)
            *out = -1000;   /* rather than blow up, output a number << 0 */
        else *out = log(f) * multiplier;
        out++;
    }
    return (w+5);
}

    /* nobody sane will ever ask for log(scalar) to a signal base but ok... */
t_int *log_tilde_perform_reversescalar(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample scalarin = *(t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample base = *in1++;
        if (base <= 1)
            *out = -1000;   /* rather than blow up, output a number << 0 */
        else if (scalarin <- 0)
            *out = -1000;
        else *out = log(scalarin) / log(base);
        out++;
    }
    return (w+5);
}

static void log_tilde_dsp(t_log_tilde *x, t_signal **sp)
{
    any_binop_dsp(sp, log_tilde_perform, log_tilde_perform,
        log_tilde_perform_scalar, log_tilde_perform_scalar,
        log_tilde_perform_reversescalar, log_tilde_perform_reversescalar);
}

static void scalarlog_tilde_dsp(t_scalarlog_tilde *x, t_signal **sp)
{
    any_binop_scalar_dsp(sp, &x->x_g, log_tilde_perform_scalar,
        log_tilde_perform_scalar);
}

static void log_tilde_setup(void)
{
    log_tilde_class = class_new(gensym("log~"), (t_newmethod)log_tilde_new, 0,
        sizeof(t_log_tilde),
            CLASS_MULTICHANNEL | CLASS_NOPROMOTESIG | CLASS_NOPROMOTELEFT,
                A_GIMME, 0);
    CLASS_MAINSIGNALIN(log_tilde_class, t_log_tilde, x_f);
    class_addmethod(log_tilde_class, (t_method)log_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(log_tilde_class, gensym("binops-tilde"));
    scalarlog_tilde_class = class_new(gensym("-~"), 0, 0,
        sizeof(t_scalarlog_tilde), CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(scalarlog_tilde_class, t_scalarlog_tilde, x_f);
    class_addmethod(scalarlog_tilde_class, (t_method)scalarlog_tilde_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarlog_tilde_class, gensym("binops-tilde"));
}

/* ----------------------------- pow ----------------------------- */
static t_class *pow_tilde_class, *scalarpow_tilde_class;

typedef struct _pow_tilde
{
    t_object x_obj;
    t_float x_f;
} t_pow_tilde;

typedef struct _scalarpow_tilde
{
    t_object x_obj;
    t_float x_f;
    t_float x_g;
} t_scalarpow_tilde;

static void *pow_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    if (argc > 1) post("-~: extra arguments ignored");
    if (argc)
    {
        t_scalarpow_tilde *x =
            (t_scalarpow_tilde *)pd_new(scalarpow_tilde_class);
        floatinlet_new(&x->x_obj, &x->x_g);
        x->x_g = atom_getfloatarg(0, argc, argv);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
    else
    {
        t_pow_tilde *x = (t_pow_tilde *)pd_new(pow_tilde_class);
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        outlet_new(&x->x_obj, &s_signal);
        x->x_f = 0;
        return (x);
    }
}

t_int *pow_tilde_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample f = *in1++, g = *in2++;
        *out++ = (f == 0 && g < 0) ||
            (f < 0 && (g - (int)g) != 0) ?
                0 : pow(f, g);
    }
    return (w+5);
}

t_int *pow_tilde_perform_scalar(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample g = *(t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample f = *in1++;
        *out++ = (f == 0 && g < 0) ||
            (f < 0 && (g - (int)g) != 0) ?
                0 : pow(f, g);
    }
    return (w+5);
}

t_int *pow_tilde_perform_reversescalar(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample f = *(t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while (n--)
    {
        t_sample g = *in1++;
        *out++ = (f == 0 && g < 0) ||
            (f < 0 && (g - (int)g) != 0) ?
                0 : pow(f, g);
    }
    return (w+5);
}

static void pow_tilde_dsp(t_pow_tilde *x, t_signal **sp)
{
    any_binop_dsp(sp, pow_tilde_perform, pow_tilde_perform,
        pow_tilde_perform_scalar, pow_tilde_perform_scalar,
        pow_tilde_perform_reversescalar, pow_tilde_perform_reversescalar);
}

static void scalarpow_tilde_dsp(t_scalarpow_tilde *x, t_signal **sp)
{
    any_binop_scalar_dsp(sp, &x->x_g, pow_tilde_perform_scalar,
        pow_tilde_perform_scalar);
}

static void pow_tilde_setup(void)
{
    pow_tilde_class = class_new(gensym("pow~"), (t_newmethod)pow_tilde_new, 0,
        sizeof(t_pow_tilde),
            CLASS_MULTICHANNEL | CLASS_NOPROMOTESIG | CLASS_NOPROMOTELEFT,
                A_GIMME, 0);
    CLASS_MAINSIGNALIN(pow_tilde_class, t_pow_tilde, x_f);
    class_addmethod(pow_tilde_class, (t_method)pow_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(pow_tilde_class, gensym("binops-tilde"));
    scalarpow_tilde_class = class_new(gensym("-~"), 0, 0,
        sizeof(t_scalarpow_tilde), CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(scalarpow_tilde_class, t_scalarpow_tilde, x_f);
    class_addmethod(scalarpow_tilde_class, (t_method)scalarpow_tilde_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(scalarpow_tilde_class, gensym("binops-tilde"));
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
    log_tilde_setup();
    pow_tilde_setup();
}
