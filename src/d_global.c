/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  send~, receive~, throw~, catch~ */

#include "m_pd.h"
#include <string.h>

/* ----------------------------- send~ ----------------------------- */
static t_class *sigsend_class;

typedef struct _sigsend
{
    t_object x_obj;
    t_symbol *x_sym;
    t_canvas *x_canvas;
    int x_length;
    int x_nchans;
    t_sample *x_vec;
    t_float x_f;
} t_sigsend;

static void *sigsend_new(t_symbol *s, t_floatarg fnchans)
{
    t_sigsend *x = (t_sigsend *)pd_new(sigsend_class);
    if (*s->s_name)
        pd_bind(&x->x_obj.ob_pd, s);
    x->x_sym = s;
    if ((x->x_nchans = fnchans) < 1)
        x->x_nchans = 1;
    x->x_length = 1;
    x->x_vec = (t_sample *)getbytes(x->x_nchans * sizeof(t_sample));
    x->x_f = 0;
    x->x_canvas = canvas_getcurrent();
    return (x);
}

static t_int *sigsend_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    while (n--)
    {
        *out = (PD_BIGORSMALL(*in) ? 0 : *in);
        out++;
        in++;
    }
    return (w+4);
}

static void sigsend_channels(t_sigsend *x, t_float fnchans)
{
    x->x_nchans = fnchans >= 1 ? fnchans : 1;
    x->x_length = 1; /* trigger update via sigsend_fixbuf */
    canvas_update_dsp();
}

static void sigsend_fixbuf(t_sigsend *x, int length)
{
    if (x->x_length != length)
    {
        x->x_vec = (t_sample *)resizebytes(x->x_vec,
            x->x_length * x->x_nchans * sizeof(t_sample),
            length * x->x_nchans * sizeof(t_sample));
        x->x_length = length;
    }
}

static void sigsend_dsp(t_sigsend *x, t_signal **sp)
{
    int usenchans = (x->x_nchans < sp[0]->s_nchans ?
        x->x_nchans : sp[0]->s_nchans);
    sigsend_fixbuf(x, sp[0]->s_length);
    dsp_add(sigsend_perform, 3, sp[0]->s_vec, x->x_vec,
        x->x_length * usenchans);
    if (x->x_nchans > usenchans)
        memset(x->x_vec + usenchans * x->x_length, 0,
            (x->x_nchans - usenchans) * x->x_length * sizeof(t_sample));
}

static void sigsend_free(t_sigsend *x)
{
    if (*x->x_sym->s_name)
        pd_unbind(&x->x_obj.ob_pd, x->x_sym);
    freebytes(x->x_vec, x->x_length * sizeof(t_sample));
}

static void sigsend_setup(void)
{
    sigsend_class = class_new(gensym("send~"), (t_newmethod)sigsend_new,
        (t_method)sigsend_free, sizeof(t_sigsend), CLASS_MULTICHANNEL,
            A_DEFSYM, A_DEFFLOAT, 0);
    class_addcreator((t_newmethod)sigsend_new, gensym("s~"),
        A_DEFSYM, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(sigsend_class, t_sigsend, x_f);
    class_addmethod(sigsend_class, (t_method)sigsend_channels,
        gensym("channels"), A_FLOAT, 0);
    class_addmethod(sigsend_class, (t_method)sigsend_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(sigsend_class, gensym("send-receive-tilde"));
}

/* ----------------------------- receive~ ----------------------------- */
static t_class *sigreceive_class;

typedef struct _sigreceive
{
    t_object x_obj;
    t_symbol *x_sym;
    t_sample *x_wherefrom;
    int x_length;
    int x_nchans;
} t_sigreceive;

static void *sigreceive_new(t_symbol *s)
{
    t_sigreceive *x = (t_sigreceive *)pd_new(sigreceive_class);
    x->x_length = 0;             /* this is changed in dsp routine */
    x->x_nchans = 1;
    x->x_sym = s;
    x->x_wherefrom = 0;
    outlet_new(&x->x_obj, &s_signal);
    return (x);
}

static t_int *sigreceive_perform(t_int *w)
{
    t_sigreceive *x = (t_sigreceive *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    t_sample *in = x->x_wherefrom;
    if (in)
    {
        while (n--)
            *out++ = *in++;
    }
    else
    {
        while (n--)
            *out++ = 0;
    }
    return (w+4);
}

/* tb: vectorized receive function */
static t_int *sigreceive_perf8(t_int *w)
{
    t_sigreceive *x = (t_sigreceive *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    t_sample *in = x->x_wherefrom;
    if (in)
    {
        for (; n; n -= 8, in += 8, out += 8)
        {
            out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; out[3] = in[3];
            out[4] = in[4]; out[5] = in[5]; out[6] = in[6]; out[7] = in[7];
        }
    }
    else
    {
        for (; n; n -= 8, in += 8, out += 8)
        {
            out[0] = 0; out[1] = 0; out[2] = 0; out[3] = 0;
            out[4] = 0; out[5] = 0; out[6] = 0; out[7] = 0;
        }
    }
    return (w+4);
}

    /* set receive symbol.  Also check our signal length (setting
    x->x_length) and chase down the sender to verify length and nchans match */
static void sigreceive_set(t_sigreceive *x, t_symbol *s)
{
    t_sigsend *sender = (t_sigsend *)pd_findbyclass((x->x_sym = s),
        sigsend_class);
    x->x_wherefrom = 0;
    if (sender)
    {
        int length = canvas_getsignallength(sender->x_canvas),
            dspstate = pd_getdspstate();
        sigsend_fixbuf(sender, length);
        if (!dspstate)
            x->x_nchans = sender->x_nchans;
        if (sender->x_nchans == x->x_nchans &&
            length == x->x_length)
                x->x_wherefrom = sender->x_vec;
        else if (x->x_length)
        {
            if (dspstate && length == x->x_length)
                pd_error(x,
     "receive~ (set %s) changed number of channels; restart DSP to fix",
                    s->s_name);
            else pd_error(x,
                "receive~ %s: dimensions %dx%d don't match the send~ (%dx%d)",
                x->x_sym->s_name, x->x_nchans, x->x_length,
                    sender->x_nchans, sender->x_length);
        }
    }
    else if (*x->x_sym->s_name)
        pd_error(x, "receive~ %s: no matching send", x->x_sym->s_name);
}

static void sigreceive_dsp(t_sigreceive *x, t_signal **sp)
{
    x->x_length = sp[0]->s_length;
    sigreceive_set(x, x->x_sym);
    signal_setmultiout(&sp[0], x->x_nchans);
    if ((x->x_length * x->x_nchans) & 7)
        dsp_add(sigreceive_perform, 3,
            x, sp[0]->s_vec, (t_int)(x->x_length * x->x_nchans));
    else dsp_add(sigreceive_perf8, 3,
        x, sp[0]->s_vec, (t_int)(x->x_length * x->x_nchans));
}

static void sigreceive_setup(void)
{
    sigreceive_class = class_new(gensym("receive~"),
        (t_newmethod)sigreceive_new, 0,
        sizeof(t_sigreceive), CLASS_MULTICHANNEL, A_DEFSYM, 0);
    class_addcreator((t_newmethod)sigreceive_new, gensym("r~"),
        A_DEFSYM, A_DEFFLOAT, 0);
    class_addmethod(sigreceive_class, (t_method)sigreceive_set, gensym("set"),
        A_SYMBOL, 0);
    class_addmethod(sigreceive_class, (t_method)sigreceive_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(sigreceive_class, gensym("send-receive-tilde"));
}

/* ----------------------------- catch~ ----------------------------- */
static t_class *sigcatch_class;

typedef struct _sigcatch
{
    t_object x_obj;
    t_symbol *x_sym;
    t_canvas *x_canvas;
    int x_length;
    int x_nchans;
    t_sample *x_vec;
} t_sigcatch;

static void *sigcatch_new(t_symbol *s, t_floatarg fnchans)
{
    t_sigcatch *x = (t_sigcatch *)pd_new(sigcatch_class);
    if (*s->s_name)
        pd_bind(&x->x_obj.ob_pd, s);
    x->x_sym = s;
    x->x_canvas = canvas_getcurrent();
    x->x_length = 1;     /* replaced later */
    if ((x->x_nchans = fnchans) < 1)
        x->x_nchans = 1;
    x->x_vec = (t_sample *)getbytes(x->x_length * sizeof(t_sample));
    outlet_new(&x->x_obj, &s_signal);
    return (x);
}

static void sigcatch_channels(t_sigcatch *x, t_float fnchans)
{
    x->x_nchans = fnchans >= 1 ? fnchans : 1;
    x->x_length = 1; /* trigger update via sigcatch_fixbuf */
    canvas_update_dsp();
}

static void sigcatch_fixbuf(t_sigcatch *x, int length)
{
    if (x->x_length != length)
    {
        x->x_vec = (t_sample *)resizebytes(x->x_vec,
            x->x_length * x->x_nchans * sizeof(t_sample),
            length * x->x_nchans * sizeof(t_sample));
        x->x_length = length;
    }
}

static t_int *sigcatch_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    while (n--)
    {
        float f = *in;
        *in++ = 0;
        *out++ = (PD_BIGORSMALL(f) ? 0 : f);
    }
    return (w+4);
}

static void sigcatch_dsp(t_sigcatch *x, t_signal **sp)
{
    sigcatch_fixbuf(x, sp[0]->s_length);
    signal_setmultiout(&sp[0], x->x_nchans);
    dsp_add(sigcatch_perform, 3, x->x_vec, sp[0]->s_vec,
        x->x_length * x->x_nchans);
}

static void sigcatch_free(t_sigcatch *x)
{
    if (*x->x_sym->s_name)
        pd_unbind(&x->x_obj.ob_pd, x->x_sym);
    freebytes(x->x_vec, x->x_length * sizeof(t_sample));
}

static void sigcatch_setup(void)
{
    sigcatch_class = class_new(gensym("catch~"), (t_newmethod)sigcatch_new,
        (t_method)sigcatch_free, sizeof(t_sigcatch),
            CLASS_MULTICHANNEL, A_DEFSYM, A_DEFFLOAT, 0);
    class_addmethod(sigcatch_class, (t_method)sigcatch_channels,
        gensym("channels"), A_FLOAT, 0);
    class_addmethod(sigcatch_class, (t_method)sigcatch_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(sigcatch_class, gensym("throw~-catch~"));
}

/* ----------------------------- throw~ ----------------------------- */
static t_class *sigthrow_class;

typedef struct _sigthrow
{
    t_object x_obj;
    t_symbol *x_sym;
    t_sample *x_whereto;
    int x_length;
    int x_nsamps;
    t_float x_f;
} t_sigthrow;

static void *sigthrow_new(t_symbol *s)
{
    t_sigthrow *x = (t_sigthrow *)pd_new(sigthrow_class);
    x->x_sym = s;
    x->x_whereto  = 0;
    x->x_length = 0;
    x->x_f = 0;
    return (x);
}

static t_int *sigthrow_perform(t_int *w)
{
    t_sigthrow *x = (t_sigthrow *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    t_sample *out = x->x_whereto;
    if (out)
    {
        n = (n <= x->x_nsamps ? n : x->x_nsamps);
        while (n--)
            *out++ += *in++;
    }
    return (w+4);
}

static void sigthrow_set(t_sigthrow *x, t_symbol *s)
{
    t_sigcatch *catcher = (t_sigcatch *)pd_findbyclass((x->x_sym = s),
        sigcatch_class);
    if (catcher)
    {
        int length = canvas_getsignallength(catcher->x_canvas);
        sigcatch_fixbuf(catcher, length);
        if (x->x_length && length != x->x_length)
        {
            pd_error(x, "throw~ %s: my vector size %d doesn't match catch~ (%d)",
                x->x_sym->s_name, x->x_length, length);
            x->x_whereto = 0;
        }
        else
        {
            x->x_whereto = catcher->x_vec;
            x->x_nsamps = catcher->x_length * catcher->x_nchans;
        }
    }
    else x->x_whereto = 0;
}

static void sigthrow_dsp(t_sigthrow *x, t_signal **sp)
{
    x->x_length = sp[0]->s_n;
    sigthrow_set(x, x->x_sym);
    dsp_add(sigthrow_perform, 3,
        x, sp[0]->s_vec, (t_int)(sp[0]->s_length * sp[0]->s_nchans));
}

static void sigthrow_setup(void)
{
    sigthrow_class = class_new(gensym("throw~"), (t_newmethod)sigthrow_new, 0,
        sizeof(t_sigthrow), CLASS_MULTICHANNEL, A_DEFSYM, 0);
    class_addmethod(sigthrow_class, (t_method)sigthrow_set, gensym("set"),
        A_SYMBOL, 0);
    CLASS_MAINSIGNALIN(sigthrow_class, t_sigthrow, x_f);
    class_addmethod(sigthrow_class, (t_method)sigthrow_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(sigthrow_class, gensym("throw~-catch~"));
}

/* ----------------------- global setup routine ---------------- */

void d_global_setup(void)
{
    sigsend_setup();
    sigreceive_setup();
    sigcatch_setup();
    sigthrow_setup();
}
