/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  "filters", both linear and nonlinear.
*/
#include "m_pd.h"
#include <math.h>

/* ---------------- hip~ - 1-pole 1-zero hipass filter. ----------------- */

typedef struct hipctl
{
    t_sample c_x;
    t_sample c_coef;
} t_hipctl;

typedef struct sighip
{
    t_object x_obj;
    t_float x_sr;
    t_float x_hz;
    t_hipctl x_cspace;
    t_hipctl *x_ctl;
    t_float x_f;
} t_sighip;

t_class *sighip_class;
static void sighip_ft1(t_sighip *x, t_floatarg f);

static void *sighip_new(t_floatarg f)
{
    t_sighip *x = (t_sighip *)pd_new(sighip_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("ft1"));
    outlet_new(&x->x_obj, &s_signal);
    x->x_sr = 44100;
    x->x_ctl = &x->x_cspace;
    x->x_cspace.c_x = 0;
    sighip_ft1(x, f);
    x->x_f = 0;
    return (x);
}

static void sighip_ft1(t_sighip *x, t_floatarg f)
{
    if (f < 0) f = 0;
    x->x_hz = f;
    x->x_ctl->c_coef = 1 - f * (2 * 3.14159) / x->x_sr;
    if (x->x_ctl->c_coef < 0)
        x->x_ctl->c_coef = 0;
    else if (x->x_ctl->c_coef > 1)
        x->x_ctl->c_coef = 1;
}

static t_int *sighip_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    t_hipctl *c = (t_hipctl *)(w[3]);
    int n = (t_int)(w[4]);
    int i;
    t_sample last = c->c_x;
    t_sample coef = c->c_coef;
    if (coef < 1)
    {
        t_sample normal = 0.5*(1+coef);
        for (i = 0; i < n; i++)
        {
            t_sample new = *in++ + coef * last;
            *out++ = normal * (new - last);
            last = new;
        }
        if (PD_BIGORSMALL(last))
            last = 0;
        c->c_x = last;
    }
    else
    {
        for (i = 0; i < n; i++)
            *out++ = *in++;
        c->c_x = 0;
    }
    return (w+5);
}

static t_int *sighip_perform_old(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    t_hipctl *c = (t_hipctl *)(w[3]);
    int n = (t_int)(w[4]);
    int i;
    t_sample last = c->c_x;
    t_sample coef = c->c_coef;
    if (coef < 1)
    {
        for (i = 0; i < n; i++)
        {
            t_sample new = *in++ + coef * last;
            *out++ = new - last;
            last = new;
        }
        if (PD_BIGORSMALL(last))
            last = 0;
        c->c_x = last;
    }
    else
    {
        for (i = 0; i < n; i++)
            *out++ = *in++;
        c->c_x = 0;
    }
    return (w+5);
}

static void sighip_dsp(t_sighip *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    sighip_ft1(x,  x->x_hz);
    dsp_add((pd_compatibilitylevel > 43 ?
        sighip_perform : sighip_perform_old),
            4, sp[0]->s_vec, sp[1]->s_vec, x->x_ctl, sp[0]->s_n);
}

static void sighip_clear(t_sighip *x, t_floatarg q)
{
    x->x_cspace.c_x = 0;
}

void sighip_setup(void)
{
    sighip_class = class_new(gensym("hip~"), (t_newmethod)sighip_new, 0,
        sizeof(t_sighip), 0, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(sighip_class, t_sighip, x_f);
    class_addmethod(sighip_class, (t_method)sighip_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(sighip_class, (t_method)sighip_ft1,
        gensym("ft1"), A_FLOAT, 0);
    class_addmethod(sighip_class, (t_method)sighip_clear, gensym("clear"), 0);
}

/* ---------------- lop~ - 1-pole lopass filter. ----------------- */

typedef struct lopctl
{
    t_sample c_x;
    t_sample c_coef;
} t_lopctl;

typedef struct siglop
{
    t_object x_obj;
    t_float x_sr;
    t_float x_hz;
    t_lopctl x_cspace;
    t_lopctl *x_ctl;
    t_float x_f;
} t_siglop;

t_class *siglop_class;

static void siglop_ft1(t_siglop *x, t_floatarg f);

static void *siglop_new(t_floatarg f)
{
    t_siglop *x = (t_siglop *)pd_new(siglop_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("ft1"));
    outlet_new(&x->x_obj, &s_signal);
    x->x_sr = 44100;
    x->x_ctl = &x->x_cspace;
    x->x_cspace.c_x = 0;
    siglop_ft1(x, f);
    x->x_f = 0;
    return (x);
}

static void siglop_ft1(t_siglop *x, t_floatarg f)
{
    if (f < 0) f = 0;
    x->x_hz = f;
    x->x_ctl->c_coef = f * (2 * 3.14159) / x->x_sr;
    if (x->x_ctl->c_coef > 1)
        x->x_ctl->c_coef = 1;
    else if (x->x_ctl->c_coef < 0)
        x->x_ctl->c_coef = 0;
}

static void siglop_clear(t_siglop *x, t_floatarg q)
{
    x->x_cspace.c_x = 0;
}

static t_int *siglop_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    t_lopctl *c = (t_lopctl *)(w[3]);
    int n = (t_int)(w[4]);
    int i;
    t_sample last = c->c_x;
    t_sample coef = c->c_coef;
    t_sample feedback = 1 - coef;
    for (i = 0; i < n; i++)
        last = *out++ = coef * *in++ + feedback * last;
    if (PD_BIGORSMALL(last))
        last = 0;
    c->c_x = last;
    return (w+5);
}

static void siglop_dsp(t_siglop *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    siglop_ft1(x,  x->x_hz);
    dsp_add(siglop_perform, 4,
        sp[0]->s_vec, sp[1]->s_vec,
            x->x_ctl, sp[0]->s_n);

}

void siglop_setup(void)
{
    siglop_class = class_new(gensym("lop~"), (t_newmethod)siglop_new, 0,
        sizeof(t_siglop), 0, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(siglop_class, t_siglop, x_f);
    class_addmethod(siglop_class, (t_method)siglop_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(siglop_class, (t_method)siglop_ft1,
        gensym("ft1"), A_FLOAT, 0);
    class_addmethod(siglop_class, (t_method)siglop_clear, gensym("clear"), 0);
}

/* ---------------- bp~ - 2-pole bandpass filter. ----------------- */

typedef struct bpctl
{
    t_sample c_x1;
    t_sample c_x2;
    t_sample c_coef1;
    t_sample c_coef2;
    t_sample c_gain;
} t_bpctl;

typedef struct sigbp
{
    t_object x_obj;
    t_float x_sr;
    t_float x_freq;
    t_float x_q;
    t_bpctl x_cspace;
    t_bpctl *x_ctl;
    t_float x_f;
} t_sigbp;

t_class *sigbp_class;

static void sigbp_docoef(t_sigbp *x, t_floatarg f, t_floatarg q);

static void *sigbp_new(t_floatarg f, t_floatarg q)
{
    t_sigbp *x = (t_sigbp *)pd_new(sigbp_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("ft1"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("ft2"));
    outlet_new(&x->x_obj, &s_signal);
    x->x_sr = 44100;
    x->x_ctl = &x->x_cspace;
    x->x_cspace.c_x1 = 0;
    x->x_cspace.c_x2 = 0;
    sigbp_docoef(x, f, q);
    x->x_f = 0;
    return (x);
}

static t_float sigbp_qcos(t_float f)
{
    if (f >= -(0.5f*3.14159f) && f <= 0.5f*3.14159f)
    {
        t_float g = f*f;
        return (((g*g*g * (-1.0f/720.0f) + g*g*(1.0f/24.0f)) - g*0.5) + 1);
    }
    else return (0);
}

static void sigbp_docoef(t_sigbp *x, t_floatarg f, t_floatarg q)
{
    t_float r, oneminusr, omega;
    if (f < 0.001) f = 10;
    if (q < 0) q = 0;
    x->x_freq = f;
    x->x_q = q;
    omega = f * (2.0f * 3.14159f) / x->x_sr;
    if (q < 0.001) oneminusr = 1.0f;
    else oneminusr = omega/q;
    if (oneminusr > 1.0f) oneminusr = 1.0f;
    r = 1.0f - oneminusr;
    x->x_ctl->c_coef1 = 2.0f * sigbp_qcos(omega) * r;
    x->x_ctl->c_coef2 = - r * r;
    x->x_ctl->c_gain = 2 * oneminusr * (oneminusr + r * omega);
    /* post("r %f, omega %f, coef1 %f, coef2 %f",
        r, omega, x->x_ctl->c_coef1, x->x_ctl->c_coef2); */
}

static void sigbp_ft1(t_sigbp *x, t_floatarg f)
{
    sigbp_docoef(x, f, x->x_q);
}

static void sigbp_ft2(t_sigbp *x, t_floatarg q)
{
    sigbp_docoef(x, x->x_freq, q);
}

static void sigbp_clear(t_sigbp *x, t_floatarg q)
{
    x->x_ctl->c_x1 = x->x_ctl->c_x2 = 0;
}

static t_int *sigbp_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    t_bpctl *c = (t_bpctl *)(w[3]);
    int n = (t_int)(w[4]);
    int i;
    t_sample last = c->c_x1;
    t_sample prev = c->c_x2;
    t_sample coef1 = c->c_coef1;
    t_sample coef2 = c->c_coef2;
    t_sample gain = c->c_gain;
    for (i = 0; i < n; i++)
    {
        t_sample output =  *in++ + coef1 * last + coef2 * prev;
        *out++ = gain * output;
        prev = last;
        last = output;
    }
    if (PD_BIGORSMALL(last))
        last = 0;
    if (PD_BIGORSMALL(prev))
        prev = 0;
    c->c_x1 = last;
    c->c_x2 = prev;
    return (w+5);
}

static void sigbp_dsp(t_sigbp *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    sigbp_docoef(x, x->x_freq, x->x_q);
    dsp_add(sigbp_perform, 4,
        sp[0]->s_vec, sp[1]->s_vec,
            x->x_ctl, sp[0]->s_n);

}

void sigbp_setup(void)
{
    sigbp_class = class_new(gensym("bp~"), (t_newmethod)sigbp_new, 0,
        sizeof(t_sigbp), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(sigbp_class, t_sigbp, x_f);
    class_addmethod(sigbp_class, (t_method)sigbp_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(sigbp_class, (t_method)sigbp_ft1,
        gensym("ft1"), A_FLOAT, 0);
    class_addmethod(sigbp_class, (t_method)sigbp_ft2,
        gensym("ft2"), A_FLOAT, 0);
    class_addmethod(sigbp_class, (t_method)sigbp_clear, gensym("clear"), 0);
}

/* ---------------- biquad~ - raw biquad filter ----------------- */

typedef struct biquadctl
{
    t_sample c_x1;
    t_sample c_x2;
    t_sample c_fb1;
    t_sample c_fb2;
    t_sample c_ff1;
    t_sample c_ff2;
    t_sample c_ff3;
} t_biquadctl;

typedef struct sigbiquad
{
    t_object x_obj;
    t_float x_f;
    t_biquadctl x_cspace;
    t_biquadctl *x_ctl;
} t_sigbiquad;

t_class *sigbiquad_class;

static void sigbiquad_list(t_sigbiquad *x, t_symbol *s, int argc, t_atom *argv);

static void *sigbiquad_new(t_symbol *s, int argc, t_atom *argv)
{
    t_sigbiquad *x = (t_sigbiquad *)pd_new(sigbiquad_class);
    outlet_new(&x->x_obj, &s_signal);
    x->x_ctl = &x->x_cspace;
    x->x_cspace.c_x1 = x->x_cspace.c_x2 = 0;
    sigbiquad_list(x, s, argc, argv);
    x->x_f = 0;
    return (x);
}

static t_int *sigbiquad_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    t_biquadctl *c = (t_biquadctl *)(w[3]);
    int n = (t_int)(w[4]);
    int i;
    t_sample last = c->c_x1;
    t_sample prev = c->c_x2;
    t_sample fb1 = c->c_fb1;
    t_sample fb2 = c->c_fb2;
    t_sample ff1 = c->c_ff1;
    t_sample ff2 = c->c_ff2;
    t_sample ff3 = c->c_ff3;
    for (i = 0; i < n; i++)
    {
        t_sample output =  *in++ + fb1 * last + fb2 * prev;
        if (PD_BIGORSMALL(output))
            output = 0;
        *out++ = ff1 * output + ff2 * last + ff3 * prev;
        prev = last;
        last = output;
    }
    c->c_x1 = last;
    c->c_x2 = prev;
    return (w+5);
}

static void sigbiquad_list(t_sigbiquad *x, t_symbol *s, int argc, t_atom *argv)
{
    t_float fb1 = atom_getfloatarg(0, argc, argv);
    t_float fb2 = atom_getfloatarg(1, argc, argv);
    t_float ff1 = atom_getfloatarg(2, argc, argv);
    t_float ff2 = atom_getfloatarg(3, argc, argv);
    t_float ff3 = atom_getfloatarg(4, argc, argv);
    t_float discriminant = fb1 * fb1 + 4 * fb2;
    t_biquadctl *c = x->x_ctl;
    if (discriminant < 0) /* imaginary roots -- resonant filter */
    {
            /* they're conjugates so we just check that the product
            is less than one */
        if (fb2 >= -1.0f) goto stable;
    }
    else    /* real roots */
    {
            /* check that the parabola 1 - fb1 x - fb2 x^2 has a
                vertex between -1 and 1, and that it's nonnegative
                at both ends, which implies both roots are in [1-,1]. */
        if (fb1 <= 2.0f && fb1 >= -2.0f &&
            1.0f - fb1 -fb2 >= 0 && 1.0f + fb1 - fb2 >= 0)
                goto stable;
    }
        /* if unstable, just bash to zero */
    fb1 = fb2 = ff1 = ff2 = ff3 = 0;
stable:
    c->c_fb1 = fb1;
    c->c_fb2 = fb2;
    c->c_ff1 = ff1;
    c->c_ff2 = ff2;
    c->c_ff3 = ff3;
}

static void sigbiquad_set(t_sigbiquad *x, t_symbol *s, int argc, t_atom *argv)
{
    t_biquadctl *c = x->x_ctl;
    c->c_x1 = atom_getfloatarg(0, argc, argv);
    c->c_x2 = atom_getfloatarg(1, argc, argv);
}

static void sigbiquad_dsp(t_sigbiquad *x, t_signal **sp)
{
    dsp_add(sigbiquad_perform, 4,
        sp[0]->s_vec, sp[1]->s_vec,
            x->x_ctl, sp[0]->s_n);

}

void sigbiquad_setup(void)
{
    sigbiquad_class = class_new(gensym("biquad~"), (t_newmethod)sigbiquad_new,
        0, sizeof(t_sigbiquad), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(sigbiquad_class, t_sigbiquad, x_f);
    class_addmethod(sigbiquad_class, (t_method)sigbiquad_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addlist(sigbiquad_class, sigbiquad_list);
    class_addmethod(sigbiquad_class, (t_method)sigbiquad_set, gensym("set"),
        A_GIMME, 0);
    class_addmethod(sigbiquad_class, (t_method)sigbiquad_set, gensym("clear"),
        A_GIMME, 0);
}

/* ---------------- samphold~ - sample and hold  ----------------- */

typedef struct sigsamphold
{
    t_object x_obj;
    t_float x_f;
    t_sample x_lastin;
    t_sample x_lastout;
} t_sigsamphold;

t_class *sigsamphold_class;

static void *sigsamphold_new(void)
{
    t_sigsamphold *x = (t_sigsamphold *)pd_new(sigsamphold_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    x->x_lastin = 0;
    x->x_lastout = 0;
    x->x_f = 0;
    return (x);
}

static t_int *sigsamphold_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    t_sigsamphold *x = (t_sigsamphold *)(w[4]);
    int n = (t_int)(w[5]);
    int i;
    t_sample lastin = x->x_lastin;
    t_sample lastout = x->x_lastout;
    for (i = 0; i < n; i++, in1++)
    {
        t_sample next = *in2++;
        if (next < lastin) lastout = *in1;
        *out++ = lastout;
        lastin = next;
    }
    x->x_lastin = lastin;
    x->x_lastout = lastout;
    return (w+6);
}

static void sigsamphold_dsp(t_sigsamphold *x, t_signal **sp)
{
    dsp_add(sigsamphold_perform, 5,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
            x, sp[0]->s_n);
}

static void sigsamphold_reset(t_sigsamphold *x, t_symbol *s, int argc,
    t_atom *argv)
{
    x->x_lastin = ((argc > 0 && (argv[0].a_type == A_FLOAT)) ?
        argv[0].a_w.w_float : 1e20);
}

static void sigsamphold_set(t_sigsamphold *x, t_float f)
{
    x->x_lastout = f;
}

void sigsamphold_setup(void)
{
    sigsamphold_class = class_new(gensym("samphold~"),
        (t_newmethod)sigsamphold_new, 0, sizeof(t_sigsamphold), 0, 0);
    CLASS_MAINSIGNALIN(sigsamphold_class, t_sigsamphold, x_f);
    class_addmethod(sigsamphold_class, (t_method)sigsamphold_set,
        gensym("set"), A_DEFFLOAT, 0);
    class_addmethod(sigsamphold_class, (t_method)sigsamphold_reset,
        gensym("reset"), A_GIMME, 0);
    class_addmethod(sigsamphold_class, (t_method)sigsamphold_dsp,
        gensym("dsp"), A_CANT, 0);
}

/* ---------------- rpole~ - real one-pole filter (raw) ----------------- */

typedef struct sigrpole
{
    t_object x_obj;
    t_float x_f;
    t_sample x_last;
} t_sigrpole;

t_class *sigrpole_class;

static void *sigrpole_new(t_float f)
{
    t_sigrpole *x = (t_sigrpole *)pd_new(sigrpole_class);
    pd_float(
        (t_pd *)inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal),
            f);
    outlet_new(&x->x_obj, &s_signal);
    x->x_last = 0;
    return (x);
}

static t_int *sigrpole_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    t_sigrpole *x = (t_sigrpole *)(w[4]);
    int n = (t_int)(w[5]);
    int i;
    t_sample last = x->x_last;
    for (i = 0; i < n; i++)
    {
        t_sample next = *in1++;
        t_sample coef = *in2++;
        *out++ = last = coef * last + next;
    }
    if (PD_BIGORSMALL(last))
        last = 0;
    x->x_last = last;
    return (w+6);
}

static void sigrpole_dsp(t_sigrpole *x, t_signal **sp)
{
    dsp_add(sigrpole_perform, 5,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
            x, sp[0]->s_n);
}

static void sigrpole_clear(t_sigrpole *x)
{
    x->x_last = 0;
}

static void sigrpole_set(t_sigrpole *x, t_float f)
{
    x->x_last = f;
}

void sigrpole_setup(void)
{
    sigrpole_class = class_new(gensym("rpole~"),
        (t_newmethod)sigrpole_new, 0, sizeof(t_sigrpole), 0, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(sigrpole_class, t_sigrpole, x_f);
    class_addmethod(sigrpole_class, (t_method)sigrpole_set,
        gensym("set"), A_DEFFLOAT, 0);
    class_addmethod(sigrpole_class, (t_method)sigrpole_clear,
        gensym("clear"), 0);
    class_addmethod(sigrpole_class, (t_method)sigrpole_dsp,
        gensym("dsp"), A_CANT, 0);
}

/* ---------------- rzero~ - real one-zero filter (raw) ----------------- */

typedef struct sigrzero
{
    t_object x_obj;
    t_float x_f;
    t_sample x_last;
} t_sigrzero;

t_class *sigrzero_class;

static void *sigrzero_new(t_float f)
{
    t_sigrzero *x = (t_sigrzero *)pd_new(sigrzero_class);
    pd_float(
        (t_pd *)inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal),
            f);
    outlet_new(&x->x_obj, &s_signal);
    x->x_last = 0;
    return (x);
}

static t_int *sigrzero_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    t_sigrzero *x = (t_sigrzero *)(w[4]);
    int n = (t_int)(w[5]);
    int i;
    t_sample last = x->x_last;
    for (i = 0; i < n; i++)
    {
        t_sample next = *in1++;
        t_sample coef = *in2++;
        *out++ = next - coef * last;
        last = next;
    }
    x->x_last = last;
    return (w+6);
}

static void sigrzero_dsp(t_sigrzero *x, t_signal **sp)
{
    dsp_add(sigrzero_perform, 5,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
            x, sp[0]->s_n);
}

static void sigrzero_clear(t_sigrzero *x)
{
    x->x_last = 0;
}

static void sigrzero_set(t_sigrzero *x, t_float f)
{
    x->x_last = f;
}

void sigrzero_setup(void)
{
    sigrzero_class = class_new(gensym("rzero~"),
        (t_newmethod)sigrzero_new, 0, sizeof(t_sigrzero), 0, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(sigrzero_class, t_sigrzero, x_f);
    class_addmethod(sigrzero_class, (t_method)sigrzero_set,
        gensym("set"), A_DEFFLOAT, 0);
    class_addmethod(sigrzero_class, (t_method)sigrzero_clear,
        gensym("clear"), 0);
    class_addmethod(sigrzero_class, (t_method)sigrzero_dsp,
        gensym("dsp"), A_CANT, 0);
}

/* ---------- rzero_rev~ - real, reverse one-zero filter (raw) ------------ */

typedef struct sigrzero_rev
{
    t_object x_obj;
    t_float x_f;
    t_sample x_last;
} t_sigrzero_rev;

t_class *sigrzero_rev_class;

static void *sigrzero_rev_new(t_float f)
{
    t_sigrzero_rev *x = (t_sigrzero_rev *)pd_new(sigrzero_rev_class);
    pd_float(
        (t_pd *)inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal),
            f);
    outlet_new(&x->x_obj, &s_signal);
    x->x_last = 0;
    return (x);
}

static t_int *sigrzero_rev_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    t_sigrzero_rev *x = (t_sigrzero_rev *)(w[4]);
    int n = (t_int)(w[5]);
    int i;
    t_sample last = x->x_last;
    for (i = 0; i < n; i++)
    {
        t_sample next = *in1++;
        t_sample coef = *in2++;
        *out++ = last - coef * next;
        last = next;
    }
    x->x_last = last;
    return (w+6);
}

static void sigrzero_rev_dsp(t_sigrzero_rev *x, t_signal **sp)
{
    dsp_add(sigrzero_rev_perform, 5,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
            x, sp[0]->s_n);
}

static void sigrzero_rev_clear(t_sigrzero_rev *x)
{
    x->x_last = 0;
}

static void sigrzero_rev_set(t_sigrzero_rev *x, t_float f)
{
    x->x_last = f;
}

void sigrzero_rev_setup(void)
{
    sigrzero_rev_class = class_new(gensym("rzero_rev~"),
        (t_newmethod)sigrzero_rev_new, 0, sizeof(t_sigrzero_rev),
        0, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(sigrzero_rev_class, t_sigrzero_rev, x_f);
    class_addmethod(sigrzero_rev_class, (t_method)sigrzero_rev_set,
        gensym("set"), A_DEFFLOAT, 0);
    class_addmethod(sigrzero_rev_class, (t_method)sigrzero_rev_clear,
        gensym("clear"), 0);
    class_addmethod(sigrzero_rev_class, (t_method)sigrzero_rev_dsp,
        gensym("dsp"), A_CANT, 0);
}

/* -------------- cpole~ - complex one-pole filter (raw) --------------- */

typedef struct sigcpole
{
    t_object x_obj;
    t_float x_f;
    t_sample x_lastre;
    t_sample x_lastim;
} t_sigcpole;

t_class *sigcpole_class;

static void *sigcpole_new(t_float re, t_float im)
{
    t_sigcpole *x = (t_sigcpole *)pd_new(sigcpole_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    pd_float(
        (t_pd *)inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal),
            re);
    pd_float(
        (t_pd *)inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal),
            im);
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    x->x_lastre = x->x_lastim = 0;
    x->x_f = 0;
    return (x);
}

static t_int *sigcpole_perform(t_int *w)
{
    t_sample *inre1 = (t_sample *)(w[1]);
    t_sample *inim1 = (t_sample *)(w[2]);
    t_sample *inre2 = (t_sample *)(w[3]);
    t_sample *inim2 = (t_sample *)(w[4]);
    t_sample *outre = (t_sample *)(w[5]);
    t_sample *outim = (t_sample *)(w[6]);
    t_sigcpole *x = (t_sigcpole *)(w[7]);
    int n = (t_int)(w[8]);
    int i;
    t_sample lastre = x->x_lastre;
    t_sample lastim = x->x_lastim;
    for (i = 0; i < n; i++)
    {
        t_sample nextre = *inre1++;
        t_sample nextim = *inim1++;
        t_sample coefre = *inre2++;
        t_sample coefim = *inim2++;
        t_sample tempre = *outre++ = nextre + lastre * coefre - lastim * coefim;
        lastim = *outim++ = nextim + lastre * coefim + lastim * coefre;
        lastre = tempre;
    }
    if (PD_BIGORSMALL(lastre))
        lastre = 0;
    if (PD_BIGORSMALL(lastim))
        lastim = 0;
    x->x_lastre = lastre;
    x->x_lastim = lastim;
    return (w+9);
}

static void sigcpole_dsp(t_sigcpole *x, t_signal **sp)
{
    dsp_add(sigcpole_perform, 8,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec,
        sp[4]->s_vec, sp[5]->s_vec, x, sp[0]->s_n);
}

static void sigcpole_clear(t_sigcpole *x)
{
    x->x_lastre = x->x_lastim = 0;
}

static void sigcpole_set(t_sigcpole *x, t_float re, t_float im)
{
    x->x_lastre = re;
    x->x_lastim = im;
}

void sigcpole_setup(void)
{
    sigcpole_class = class_new(gensym("cpole~"),
        (t_newmethod)sigcpole_new, 0, sizeof(t_sigcpole), 0,
            A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(sigcpole_class, t_sigcpole, x_f);
    class_addmethod(sigcpole_class, (t_method)sigcpole_set,
        gensym("set"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(sigcpole_class, (t_method)sigcpole_clear,
        gensym("clear"), 0);
    class_addmethod(sigcpole_class, (t_method)sigcpole_dsp,
        gensym("dsp"), A_CANT, 0);
}

/* -------------- czero~ - complex one-zero filter (raw) --------------- */

typedef struct sigczero
{
    t_object x_obj;
    t_float x_f;
    t_sample x_lastre;
    t_sample x_lastim;
} t_sigczero;

t_class *sigczero_class;

static void *sigczero_new(t_float re, t_float im)
{
    t_sigczero *x = (t_sigczero *)pd_new(sigczero_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    pd_float(
        (t_pd *)inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal),
            re);
    pd_float(
        (t_pd *)inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal),
            im);
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    x->x_lastre = x->x_lastim = 0;
    x->x_f = 0;
    return (x);
}

static t_int *sigczero_perform(t_int *w)
{
    t_sample *inre1 = (t_sample *)(w[1]);
    t_sample *inim1 = (t_sample *)(w[2]);
    t_sample *inre2 = (t_sample *)(w[3]);
    t_sample *inim2 = (t_sample *)(w[4]);
    t_sample *outre = (t_sample *)(w[5]);
    t_sample *outim = (t_sample *)(w[6]);
    t_sigczero *x = (t_sigczero *)(w[7]);
    int n = (t_int)(w[8]);
    int i;
    t_sample lastre = x->x_lastre;
    t_sample lastim = x->x_lastim;
    for (i = 0; i < n; i++)
    {
        t_sample nextre = *inre1++;
        t_sample nextim = *inim1++;
        t_sample coefre = *inre2++;
        t_sample coefim = *inim2++;
        *outre++ = nextre - lastre * coefre + lastim * coefim;
        *outim++ = nextim - lastre * coefim - lastim * coefre;
        lastre = nextre;
        lastim = nextim;
    }
    x->x_lastre = lastre;
    x->x_lastim = lastim;
    return (w+9);
}

static void sigczero_dsp(t_sigczero *x, t_signal **sp)
{
    dsp_add(sigczero_perform, 8,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec,
        sp[4]->s_vec, sp[5]->s_vec, x, sp[0]->s_n);
}

static void sigczero_clear(t_sigczero *x)
{
    x->x_lastre = x->x_lastim = 0;
}

static void sigczero_set(t_sigczero *x, t_float re, t_float im)
{
    x->x_lastre = re;
    x->x_lastim = im;
}

void sigczero_setup(void)
{
    sigczero_class = class_new(gensym("czero~"),
        (t_newmethod)sigczero_new, 0, sizeof(t_sigczero), 0,
            A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(sigczero_class, t_sigczero, x_f);
    class_addmethod(sigczero_class, (t_method)sigczero_set,
        gensym("set"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(sigczero_class, (t_method)sigczero_clear,
        gensym("clear"), 0);
    class_addmethod(sigczero_class, (t_method)sigczero_dsp,
        gensym("dsp"), A_CANT, 0);
}

/* ------ czero_rev~ - complex one-zero filter (raw, reverse form) ----- */

typedef struct sigczero_rev
{
    t_object x_obj;
    t_float x_f;
    t_sample x_lastre;
    t_sample x_lastim;
} t_sigczero_rev;

t_class *sigczero_rev_class;

static void *sigczero_rev_new(t_float re, t_float im)
{
    t_sigczero_rev *x = (t_sigczero_rev *)pd_new(sigczero_rev_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    pd_float(
        (t_pd *)inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal),
            re);
    pd_float(
        (t_pd *)inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal),
            im);
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    x->x_lastre = x->x_lastim = 0;
    x->x_f = 0;
    return (x);
}

static t_int *sigczero_rev_perform(t_int *w)
{
    t_sample *inre1 = (t_sample *)(w[1]);
    t_sample *inim1 = (t_sample *)(w[2]);
    t_sample *inre2 = (t_sample *)(w[3]);
    t_sample *inim2 = (t_sample *)(w[4]);
    t_sample *outre = (t_sample *)(w[5]);
    t_sample *outim = (t_sample *)(w[6]);
    t_sigczero_rev *x = (t_sigczero_rev *)(w[7]);
    int n = (t_int)(w[8]);
    int i;
    t_sample lastre = x->x_lastre;
    t_sample lastim = x->x_lastim;
    for (i = 0; i < n; i++)
    {
        t_sample nextre = *inre1++;
        t_sample nextim = *inim1++;
        t_sample coefre = *inre2++;
        t_sample coefim = *inim2++;
            /* transfer function is (A bar) - Z^-1, for the same
            frequency response as 1 - AZ^-1 from czero_tilde. */
        *outre++ = lastre - nextre * coefre - nextim * coefim;
        *outim++ = lastim - nextre * coefim + nextim * coefre;
        lastre = nextre;
        lastim = nextim;
    }
    x->x_lastre = lastre;
    x->x_lastim = lastim;
    return (w+9);
}

static void sigczero_rev_dsp(t_sigczero_rev *x, t_signal **sp)
{
    dsp_add(sigczero_rev_perform, 8,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec,
        sp[4]->s_vec, sp[5]->s_vec, x, sp[0]->s_n);
}

static void sigczero_rev_clear(t_sigczero_rev *x)
{
    x->x_lastre = x->x_lastim = 0;
}

static void sigczero_rev_set(t_sigczero_rev *x, t_float re, t_float im)
{
    x->x_lastre = re;
    x->x_lastim = im;
}

void sigczero_rev_setup(void)
{
    sigczero_rev_class = class_new(gensym("czero_rev~"),
        (t_newmethod)sigczero_rev_new, 0, sizeof(t_sigczero_rev), 0,
            A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(sigczero_rev_class, t_sigczero_rev, x_f);
    class_addmethod(sigczero_rev_class, (t_method)sigczero_rev_set,
        gensym("set"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(sigczero_rev_class, (t_method)sigczero_rev_clear,
        gensym("clear"), 0);
    class_addmethod(sigczero_rev_class, (t_method)sigczero_rev_dsp,
        gensym("dsp"), A_CANT, 0);
}

/* ------------------------ setup routine ------------------------- */

void d_filter_setup(void)
{
    sighip_setup();
    siglop_setup();
    sigbp_setup();
    sigbiquad_setup();
    sigsamphold_setup();
    sigrpole_setup();
    sigrzero_setup();
    sigrzero_rev_setup();
    sigcpole_setup();
    sigczero_setup();
    sigczero_rev_setup();
}
