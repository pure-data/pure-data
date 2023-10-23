/* Copyright (c) 1997-1999 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* reading and writing arrays */
/* LATER consider adding methods to set gpointer */

#include "m_pd.h"
#include "g_canvas.h"

    /* common struct for reading or writing to an array at DSP time. */
typedef struct _dsparray
{
    t_symbol *d_symbol;
    t_gpointer d_gp;
    int d_phase;    /* used for tabwrite~ and tabplay~ */
    void *d_owner;  /* for pd_error() */
} t_dsparray;

typedef struct _arrayvec
{
    int v_n;
    t_dsparray *v_vec;
} t_arrayvec;

    /* LATER consider exporting this and using it for tabosc4~ too */
static int dsparray_get_array(t_dsparray *d, int *npoints, t_word **vec,
    int recover)
{
    t_garray *a;

    if (gpointer_check(&d->d_gp, 0))
    {
        *vec = (t_word *)d->d_gp.gp_stub->gs_un.gs_array->a_vec;
        *npoints = d->d_gp.gp_stub->gs_un.gs_array->a_n;
        return 1;
    }
    else if (recover || d->d_gp.gp_stub)
        /* when the pointer is invalid: if "recover" is true or if the array
        has already been successfully acquired, then try re-acquiring it */
    {
        if (!(a = (t_garray *)pd_findbyclass(d->d_symbol, garray_class)))
        {
            if (d->d_owner && *d->d_symbol->s_name)
                pd_error(d->d_owner, "%s: no such array", d->d_symbol->s_name);
            gpointer_unset(&d->d_gp);
            return 0;
        }
        else if (!garray_getfloatwords(a, npoints, vec))
        {
            if (d->d_owner)
                pd_error(d->d_owner, "%s: bad template", d->d_symbol->s_name);
            gpointer_unset(&d->d_gp);
            return 0;
        }
        else
        {
            gpointer_setarray(&d->d_gp, garray_getarray(a), *vec);
            return 1;
        }
    }
    return 0;
}

static void arrayvec_testvec(t_arrayvec *v)
{
    int i, vecsize;
    t_word *vec;
    for (i = 0; i < v->v_n; i++)
    {
        if (*v->v_vec[i].d_symbol->s_name)
            dsparray_get_array(&v->v_vec[i], &vecsize, &vec, 1);
    }
}

static void arrayvec_set(t_arrayvec *v, int argc, t_atom *argv)
{
    int i;
    for (i = 0; i < v->v_n && i < argc; i++)
    {
        gpointer_unset(&v->v_vec[i].d_gp); /* reset the pointer */
        if (argv[i].a_type != A_SYMBOL)
            pd_error(v->v_vec[i].d_owner,
                "expected symbolic array name, got number instead"),
                v->v_vec[i].d_symbol = &s_;
        else
        {
            v->v_vec[i].d_phase = 0x7fffffff;
            v->v_vec[i].d_symbol = argv[i].a_w.w_symbol;
        }
    }
    if (pd_getdspstate())
        arrayvec_testvec(v);
}

static void arrayvec_init(t_arrayvec *v, void *x, int rawargc, t_atom *rawargv)
{
    int i, argc;
    t_atom a, *argv;
    if (rawargc == 0)
    {
        argc = 1;
        SETSYMBOL(&a, &s_);
        argv = &a;
    }
    else argc = rawargc, argv=rawargv;

    v->v_vec = (t_dsparray *)getbytes(argc * sizeof(*v->v_vec));
    v->v_n = argc;
    for (i = 0; i < v->v_n; i++)
    {
        v->v_vec[i].d_owner = x;
        v->v_vec[i].d_phase = 0x7fffffff;
        gpointer_init(&v->v_vec[i].d_gp);
    }
    arrayvec_set(v, argc, argv);
}

static void arrayvec_free(t_arrayvec *v)
{
    int i;
    for (i = 0; i < v->v_n; i++)
        gpointer_unset(&v->v_vec[i].d_gp);
    freebytes(v->v_vec, v->v_n * sizeof(*v->v_vec));
}

/* ------------------------- tabwrite~ -------------------------- */

static t_class *tabwrite_tilde_class;

typedef struct _tabwrite_tilde
{
    t_object x_obj;
    t_arrayvec x_v;
    t_float x_f;
} t_tabwrite_tilde;

static void *tabwrite_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_tabwrite_tilde *x = (t_tabwrite_tilde *)pd_new(tabwrite_tilde_class);
    arrayvec_init(&x->x_v, x, argc, argv);
    x->x_f = 0;
    return (x);
}

static void tabwrite_tilde_redraw(t_symbol *arraysym)
{
    t_garray *a = (t_garray *)pd_findbyclass(arraysym, garray_class);
    if (!a)
        bug("tabwrite_tilde_redraw");
    else garray_redraw(a);
}

static t_int *tabwrite_tilde_perform(t_int *w)
{
    t_dsparray *d = (t_dsparray *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    int n = (int)(w[3]), phase = d->d_phase, endphase;
    t_word *buf;

    if (!dsparray_get_array(d, &endphase, &buf, 0))
        goto noop;

    if (phase < endphase)
    {
        int nxfer = endphase - phase;
        t_word *wp = buf + phase;
        if (nxfer > n)
            nxfer = n;
        phase += nxfer;
        while (nxfer--)
        {
            t_sample f = *in++;
            if (PD_BIGORSMALL(f))
                f = 0;
            (wp++)->w_float = f;
        }
        if (phase >= endphase)
        {
            tabwrite_tilde_redraw(d->d_symbol);
            phase = 0x7fffffff;
        }
        d->d_phase = phase;
    }
    else d->d_phase = 0x7fffffff;
noop:
    return (w+4);
}

static void tabwrite_tilde_set(t_tabwrite_tilde *x, t_symbol *s,
    int argc, t_atom *argv)
{
    arrayvec_set(&x->x_v, argc, argv);
}

static void tabwrite_tilde_dsp(t_tabwrite_tilde *x, t_signal **sp)
{
    int i, nchans = (sp[0]->s_nchans < x->x_v.v_n ?
        sp[0]->s_nchans : x->x_v.v_n);
    arrayvec_testvec(&x->x_v);
    for (i = 0; i < nchans; i++)
        dsp_add(tabwrite_tilde_perform, 3, x->x_v.v_vec+i,
            sp[0]->s_vec + i * sp[0]->s_length, (t_int)sp[0]->s_length);
}

static void tabwrite_tilde_start(t_tabwrite_tilde *x, t_floatarg f)
{
    int i;
    for (i = 0; i < x->x_v.v_n; i++)
        x->x_v.v_vec[i].d_phase = (f > 0 ? f : 0);
}

static void tabwrite_tilde_bang(t_tabwrite_tilde *x)
{
    tabwrite_tilde_start(x, 0);
}

static void tabwrite_tilde_stop(t_tabwrite_tilde *x)
{
    int i;
    for (i = 0; i < x->x_v.v_n; i++)
        if (x->x_v.v_vec[i].d_phase != 0x7fffffff)
    {
        tabwrite_tilde_redraw(x->x_v.v_vec[i].d_symbol);
        x->x_v.v_vec[i].d_phase = 0x7fffffff;
    }
}

static void tabwrite_tilde_free(t_tabwrite_tilde *x)
{
    arrayvec_free(&x->x_v);
}

static void tabwrite_tilde_setup(void)
{
    tabwrite_tilde_class = class_new(gensym("tabwrite~"),
        (t_newmethod)tabwrite_tilde_new, (t_method)tabwrite_tilde_free,
        sizeof(t_tabwrite_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(tabwrite_tilde_class, t_tabwrite_tilde, x_f);
    class_addmethod(tabwrite_tilde_class, (t_method)tabwrite_tilde_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(tabwrite_tilde_class, (t_method)tabwrite_tilde_set,
        gensym("set"), A_GIMME, 0);
    class_addmethod(tabwrite_tilde_class, (t_method)tabwrite_tilde_stop,
        gensym("stop"), 0);
    class_addmethod(tabwrite_tilde_class, (t_method)tabwrite_tilde_start,
        gensym("start"), A_DEFFLOAT, 0);
    class_addbang(tabwrite_tilde_class, tabwrite_tilde_bang);
}

/* ------------ tabplay~ - non-transposing sample playback --------------- */

static t_class *tabplay_tilde_class;

typedef struct _tabplay_tilde
{
    t_object x_obj;
    t_outlet *x_bangout;
    int x_limit;
    t_clock *x_clock;
    t_arrayvec x_v;
} t_tabplay_tilde;

static void tabplay_tilde_tick(t_tabplay_tilde *x);

static void *tabplay_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_tabplay_tilde *x = (t_tabplay_tilde *)pd_new(tabplay_tilde_class);
    x->x_clock = clock_new(x, (t_method)tabplay_tilde_tick);
    outlet_new(&x->x_obj, &s_signal);
    x->x_bangout = outlet_new(&x->x_obj, &s_bang);
    arrayvec_init(&x->x_v, x, argc, argv);
    x->x_limit = 0;
    return (x);
}

static t_int *tabplay_tilde_perform(t_int *w)
{
    t_tabplay_tilde *x = (t_tabplay_tilde *)(w[1]);
    t_dsparray *d = (t_dsparray *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    t_word *wp;
    int n = (int)(w[4]), phase = d->d_phase, endphase, nxfer, n3;
    t_word *buf;

    if (!dsparray_get_array(d, &endphase, &buf, 0) || phase >= endphase)
        goto zero;
    if (endphase > x->x_limit)
        endphase = x->x_limit;
    nxfer = endphase - phase;
    wp = buf + phase;
    if (nxfer > n)
        nxfer = n;
    n3 = n - nxfer;
    phase += nxfer;
    while (nxfer--)
        *out++ = (wp++)->w_float;
    if (phase >= endphase)
    {
        int i, playing = 0;
        d->d_phase = 0x7fffffff;
            /* set the clock when all channels have run out */
        for (i = 0; i < x->x_v.v_n; i++)
            if (x->x_v.v_vec[i].d_phase < 0x7fffffff)
                playing = 1;
        if (!playing)
            clock_delay(x->x_clock, 0);
        while (n3--)
            *out++ = 0;
    }
    else d->d_phase = phase;

    return (w+5);
zero:
    while (n--) *out++ = 0;
    return (w+5);
}

static void tabplay_tilde_set(t_tabplay_tilde *x, t_symbol *s,
    int argc, t_atom *argv)
{
    arrayvec_set(&x->x_v, argc, argv);
}

static void tabplay_tilde_dsp(t_tabplay_tilde *x, t_signal **sp)
{
    int i;
    signal_setmultiout(&sp[0], x->x_v.v_n);
    arrayvec_testvec(&x->x_v);
    for (i = 0; i < x->x_v.v_n; i++)
        dsp_add(tabplay_tilde_perform, 4, x, &x->x_v.v_vec[i],
            sp[0]->s_vec + i * sp[0]->s_length, (t_int)sp[0]->s_length);
}

static void tabplay_tilde_list(t_tabplay_tilde *x, t_symbol *s,
    int argc, t_atom *argv)
{
    long start = atom_getfloatarg(0, argc, argv);
    long length = atom_getfloatarg(1, argc, argv);
    int i;
    if (start < 0) start = 0;
    if (length <= 0)
        x->x_limit = 0x7fffffff;
    else
        x->x_limit = (int)(start + length);
    for (i = 0; i < x->x_v.v_n; i++)
        x->x_v.v_vec[i].d_phase = (int)start;
}

static void tabplay_tilde_stop(t_tabplay_tilde *x)
{
    int i;
    for (i = 0; i < x->x_v.v_n; i++)
        x->x_v.v_vec[i].d_phase = 0x7fffffff;
}

static void tabplay_tilde_tick(t_tabplay_tilde *x)
{
    outlet_bang(x->x_bangout);
}

static void tabplay_tilde_free(t_tabplay_tilde *x)
{
    clock_free(x->x_clock);
    arrayvec_free(&x->x_v);
}

static void tabplay_tilde_setup(void)
{
    tabplay_tilde_class = class_new(gensym("tabplay~"),
        (t_newmethod)tabplay_tilde_new, (t_method)tabplay_tilde_free,
        sizeof(t_tabplay_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(tabplay_tilde_class, (t_method)tabplay_tilde_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(tabplay_tilde_class, (t_method)tabplay_tilde_stop,
        gensym("stop"), 0);
    class_addmethod(tabplay_tilde_class, (t_method)tabplay_tilde_set,
        gensym("set"), A_GIMME, 0);
    class_addlist(tabplay_tilde_class, tabplay_tilde_list);
}

/******************** tabread~ ***********************/

static t_class *tabread_tilde_class;

typedef struct _tabread_tilde
{
    t_object x_obj;
    t_arrayvec x_v;
    t_float x_f;
} t_tabread_tilde;

static void *tabread_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_tabread_tilde *x = (t_tabread_tilde *)pd_new(tabread_tilde_class);
    arrayvec_init(&x->x_v, x, argc, argv);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static t_int *tabread_tilde_perform(t_int *w)
{
    t_dsparray *d = (t_dsparray *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]), i, maxindex;
    t_word *buf;

    if (!dsparray_get_array(d, &maxindex, &buf, 0))
        goto zero;
    maxindex -= 1;

    for (i = 0; i < n; i++)
    {
        int index = *in++;
        if (index < 0)
            index = 0;
        else if (index > maxindex)
            index = maxindex;
        *out++ = buf[index].w_float;
    }
    return (w+5);
 zero:
    while (n--) *out++ = 0;

    return (w+5);
}

static void tabread_tilde_set(t_tabread_tilde *x, t_symbol *s,
    int argc, t_atom *argv)
{
    arrayvec_set(&x->x_v, argc, argv);
}

static void tabread_tilde_dsp(t_tabread_tilde *x, t_signal **sp)
{
    int i;
    signal_setmultiout(&sp[1], x->x_v.v_n);
    arrayvec_testvec(&x->x_v);
    for (i = 0; i < x->x_v.v_n; i++)
        dsp_add(tabread_tilde_perform, 4, &x->x_v.v_vec[i],
            sp[0]->s_vec + (i%(sp[0]->s_nchans)) * sp[0]->s_length,
                sp[1]->s_vec + i * sp[0]->s_length, (t_int)sp[0]->s_length);

}

static void tabread_tilde_free(t_tabread_tilde *x)
{
    arrayvec_free(&x->x_v);
}

static void tabread_tilde_setup(void)
{
    tabread_tilde_class = class_new(gensym("tabread~"),
        (t_newmethod)tabread_tilde_new, (t_method)tabread_tilde_free,
        sizeof(t_tabread_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(tabread_tilde_class, t_tabread_tilde, x_f);
    class_addmethod(tabread_tilde_class, (t_method)tabread_tilde_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(tabread_tilde_class, (t_method)tabread_tilde_set,
        gensym("set"), A_GIMME, 0);
}

/******************** tabread4~ ***********************/

static t_class *tabread4_tilde_class;

typedef struct _tabread4_tilde
{
    t_object x_obj;
    t_arrayvec x_v;
    t_float x_f;
    t_float x_onset;
} t_tabread4_tilde;

static void *tabread4_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_tabread4_tilde *x = (t_tabread4_tilde *)pd_new(tabread4_tilde_class);
    arrayvec_init(&x->x_v, x, argc, argv);
    outlet_new(&x->x_obj, gensym("signal"));
    floatinlet_new(&x->x_obj, &x->x_onset);
    x->x_f = x->x_onset = 0;
    return (x);
}

static t_int *tabread4_tilde_perform(t_int *w)
{
    t_dsparray *d = (t_dsparray *)(w[1]);
    double onset = *(t_float *)(w[2]);
    t_sample *in = (t_sample *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    int n = (int)(w[5]);
    int maxindex, i;
    t_word *buf, *wp;
    const t_sample one_over_six = 1./6.;

    if (!dsparray_get_array(d, &maxindex, &buf, 0))
        goto zero;

    maxindex -= 3;
    if (maxindex < 0) goto zero;
    if (!buf || maxindex < 1)
        goto zero;

    for (i = 0; i < n; i++)
    {
        double findex = *in++ + onset;
        int index = findex;
        t_sample frac,  a,  b,  c,  d, cminusb;
        if (index < 1)
            index = 1, frac = 0;
        else if (index > maxindex)
            index = maxindex, frac = 1;
        else frac = findex - index;
        wp = buf + index;
        a = wp[-1].w_float;
        b = wp[0].w_float;
        c = wp[1].w_float;
        d = wp[2].w_float;
        cminusb = c-b;
        *out++ = b + frac * (
            cminusb - one_over_six * ((t_sample)1.-frac) * (
                (d - a - (t_sample)3.0 * cminusb) * frac +
                (d + a*(t_sample)2.0 - b*(t_sample)3.0)
            )
        );
    }
    return (w+6);
 zero:
    while (n--)
        *out++ = 0;

    return (w+6);
}

static void tabread4_tilde_set(t_tabread4_tilde *x, t_symbol *s,
    int argc, t_atom *argv)
{
    arrayvec_set(&x->x_v, argc, argv);
}

static void tabread4_tilde_dsp(t_tabread4_tilde *x, t_signal **sp)
{
    int i;
    signal_setmultiout(&sp[1], x->x_v.v_n);
    arrayvec_testvec(&x->x_v);
    for (i = 0; i < x->x_v.v_n; i++)
        dsp_add(tabread4_tilde_perform, 5, &x->x_v.v_vec[i], &x->x_onset,
            sp[0]->s_vec + (i%(sp[0]->s_nchans)) * sp[0]->s_length,
                sp[1]->s_vec + i * sp[0]->s_length, (t_int)sp[0]->s_length);

}

static void tabread4_tilde_free(t_tabread4_tilde *x)
{
    arrayvec_free(&x->x_v);
}

static void tabread4_tilde_setup(void)
{
    tabread4_tilde_class = class_new(gensym("tabread4~"),
        (t_newmethod)tabread4_tilde_new, (t_method)tabread4_tilde_free,
        sizeof(t_tabread4_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(tabread4_tilde_class, t_tabread4_tilde, x_f);
    class_addmethod(tabread4_tilde_class, (t_method)tabread4_tilde_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(tabread4_tilde_class, (t_method)tabread4_tilde_set,
        gensym("set"), A_GIMME, 0);
}


/* ------------------------ tabsend~ ------------------------- */

static t_class *tabsend_class;

typedef struct _tabsend
{
    t_object x_obj;
    t_arrayvec x_v;
    int x_graphperiod;
    t_float x_f;
} t_tabsend;

static void tabsend_tick(t_tabsend *x);

static void *tabsend_new(t_symbol *s, int argc, t_atom *argv)
{
    t_tabsend *x = (t_tabsend *)pd_new(tabsend_class);
    arrayvec_init(&x->x_v, x, argc, argv);
    x->x_graphperiod = 1;
    x->x_f = 0;
    return (x);
}

static t_int *tabsend_perform(t_int *w)
{
    t_tabsend *x = (t_tabsend *)(w[1]);
    t_dsparray *d = (t_dsparray *)(w[2]);
    t_sample *in = (t_sample *)(w[3]);
    int n = (int)w[4], maxindex;
    t_word *dest;
    int phase = d->d_phase;

    if (!dsparray_get_array(d, &maxindex, &dest, 0))
        goto bad;

    if (n > maxindex)
        n = maxindex;
    while (n--)
    {
        t_sample f = *in++;
        if (PD_BIGORSMALL(f))
            f = 0;
         (dest++)->w_float = f;
    }
    if (phase++ >= x->x_graphperiod)
    {
        tabwrite_tilde_redraw(d->d_symbol);
        phase = 0;
    }
    d->d_phase = phase;
bad:
    return (w+5);
}

static void tabsend_set(t_tabsend *x, t_symbol *s, int argc, t_atom *argv)
{
    arrayvec_set(&x->x_v, argc, argv);
}

static void tabsend_dsp(t_tabsend *x, t_signal **sp)
{
    int i, nchans = (sp[0]->s_nchans < x->x_v.v_n ?
        sp[0]->s_nchans : x->x_v.v_n);
    int length = sp[0]->s_length;
    int tickspersec = sp[0]->s_sr/length;
    if (tickspersec < 1)
        tickspersec = 1;
    x->x_graphperiod = tickspersec;
    arrayvec_testvec(&x->x_v);
    for (i = 0; i < nchans; i++)
        dsp_add(tabsend_perform, 4, x, x->x_v.v_vec+i,
            sp[0]->s_vec + i * sp[0]->s_length, (t_int)sp[0]->s_length);
}

static void tabsend_free(t_tabsend *x)
{
    arrayvec_free(&x->x_v);
}

static void tabsend_setup(void)
{
    tabsend_class = class_new(gensym("tabsend~"),
        (t_newmethod)tabsend_new, (t_method)tabsend_free,
        sizeof(t_tabsend), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(tabsend_class, t_tabsend, x_f);
    class_addmethod(tabsend_class, (t_method)tabsend_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(tabsend_class, (t_method)tabsend_set,
        gensym("set"), A_GIMME, 0);
    class_sethelpsymbol(tabsend_class, gensym("tabsend-receive~"));
}

/* ------------------------ tabreceive~ ------------------------- */

static t_class *tabreceive_class;

typedef struct _tabreceive
{
    t_object x_obj;
    t_arrayvec x_v;
} t_tabreceive;

static void *tabreceive_new(t_symbol *s, int argc, t_atom *argv)
{
    t_tabreceive *x = (t_tabreceive *)pd_new(tabreceive_class);
    outlet_new(&x->x_obj, &s_signal);
    arrayvec_init(&x->x_v, x, argc, argv);
    return (x);
}

static t_int *tabreceive_perform(t_int *w)
{
    t_dsparray *d = (t_dsparray *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)w[3], maxindex;
    t_word *from;

    if (dsparray_get_array(d, &maxindex, &from, 0))
    {
        t_int vecsize = maxindex;
        if (vecsize > n)
            vecsize = n;
        while (vecsize--)
            *out++ = (from++)->w_float;
        vecsize = n - maxindex;
        if (vecsize > 0)
            while (vecsize--)
                *out++ = 0;
    }
    else while (n--)
            *out++ = 0;
    return (w+4);
}

static void tabreceive_set(t_tabreceive *x, t_symbol *s,
    int argc, t_atom *argv)
{
    arrayvec_set(&x->x_v, argc, argv);
}

static void tabreceive_dsp(t_tabreceive *x, t_signal **sp)
{
    int i;
    signal_setmultiout(&sp[0], x->x_v.v_n);
    arrayvec_testvec(&x->x_v);
    for (i = 0; i < x->x_v.v_n; i++)
        dsp_add(tabreceive_perform, 3, &x->x_v.v_vec[i],
            sp[0]->s_vec + i * sp[0]->s_length, (t_int)sp[0]->s_length);
}

static void tabreceive_free(t_tabreceive *x)
{
    arrayvec_free(&x->x_v);
}

static void tabreceive_setup(void)
{
    tabreceive_class = class_new(gensym("tabreceive~"),
        (t_newmethod)tabreceive_new, (t_method)tabreceive_free,
        sizeof(t_tabreceive), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(tabreceive_class, (t_method)tabreceive_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(tabreceive_class, (t_method)tabreceive_set,
        gensym("set"), A_GIMME, 0);
    class_sethelpsymbol(tabreceive_class, gensym("tabsend-receive~"));
}

/* ---------- tabread: control, non-interpolating ------------------------ */

static t_class *tabread_class;

typedef struct _tabread
{
    t_object x_obj;
    t_symbol *x_arrayname;
} t_tabread;

static void tabread_float(t_tabread *x, t_float f)
{
    t_garray *a;
    int npoints;
    t_word *vec;

    if (!(a = (t_garray *)pd_findbyclass(x->x_arrayname, garray_class)))
        pd_error(x, "%s: no such array", x->x_arrayname->s_name);
    else if (!garray_getfloatwords(a, &npoints, &vec))
        pd_error(x, "%s: bad template for tabread", x->x_arrayname->s_name);
    else
    {
        int n = f;
        if (n < 0) n = 0;
        else if (n >= npoints) n = npoints - 1;
        outlet_float(x->x_obj.ob_outlet, (npoints ? vec[n].w_float : 0));
    }
}

static void tabread_set(t_tabread *x, t_symbol *s)
{
    x->x_arrayname = s;
}

static void *tabread_new(t_symbol *s)
{
    t_tabread *x = (t_tabread *)pd_new(tabread_class);
    x->x_arrayname = s;
    outlet_new(&x->x_obj, &s_float);
    return (x);
}

static void tabread_setup(void)
{
    tabread_class = class_new(gensym("tabread"), (t_newmethod)tabread_new,
        0, sizeof(t_tabread), 0, A_DEFSYM, 0);
    class_addfloat(tabread_class, (t_method)tabread_float);
    class_addmethod(tabread_class, (t_method)tabread_set, gensym("set"),
        A_SYMBOL, 0);
}

/* ---------- tabread4: control, 4-point interpolation --------------- */

static t_class *tabread4_class;

typedef struct _tabread4
{
    t_object x_obj;
    t_symbol *x_arrayname;
} t_tabread4;

static void tabread4_float(t_tabread4 *x, t_float f)
{
    t_garray *a;
    int npoints;
    t_word *vec;

    if (!(a = (t_garray *)pd_findbyclass(x->x_arrayname, garray_class)))
        pd_error(x, "%s: no such array", x->x_arrayname->s_name);
    else if (!garray_getfloatwords(a, &npoints, &vec))
        pd_error(x, "%s: bad template for tabread4", x->x_arrayname->s_name);
    else if (npoints < 4)
        outlet_float(x->x_obj.ob_outlet, 0);
    else if (f <= 1)
        outlet_float(x->x_obj.ob_outlet, vec[1].w_float);
    else if (f >= npoints - 2)
        outlet_float(x->x_obj.ob_outlet, vec[npoints - 2].w_float);
    else
    {
        int n = f;
        float a, b, c, d, cminusb, frac;
        t_word *wp;
        if (n >= npoints - 2)
            n = npoints - 3;
        wp = vec + n;
        frac = f - n;
        a = wp[-1].w_float;
        b = wp[0].w_float;
        c = wp[1].w_float;
        d = wp[2].w_float;
        cminusb = c-b;
        outlet_float(x->x_obj.ob_outlet, b + frac * (
            cminusb - 0.1666667f * (1.-frac) * (
                (d - a - 3.0f * cminusb) * frac + (d + 2.0f*a - 3.0f*b))));
    }
}

static void tabread4_set(t_tabread4 *x, t_symbol *s)
{
    x->x_arrayname = s;
}

static void *tabread4_new(t_symbol *s)
{
    t_tabread4 *x = (t_tabread4 *)pd_new(tabread4_class);
    x->x_arrayname = s;
    outlet_new(&x->x_obj, &s_float);
    return (x);
}

static void tabread4_setup(void)
{
    tabread4_class = class_new(gensym("tabread4"), (t_newmethod)tabread4_new,
        0, sizeof(t_tabread4), 0, A_DEFSYM, 0);
    class_addfloat(tabread4_class, (t_method)tabread4_float);
    class_addmethod(tabread4_class, (t_method)tabread4_set, gensym("set"),
        A_SYMBOL, 0);
}

/* ------------------ tabwrite: control ------------------------ */

static t_class *tabwrite_class;

typedef struct _tabwrite
{
    t_object x_obj;
    t_symbol *x_arrayname;
    t_float x_ft1;
} t_tabwrite;

static void tabwrite_float(t_tabwrite *x, t_float f)
{
    int vecsize;
    t_garray *a;
    t_word *vec;

    if (!(a = (t_garray *)pd_findbyclass(x->x_arrayname, garray_class)))
        pd_error(x, "%s: no such array", x->x_arrayname->s_name);
    else if (!garray_getfloatwords(a, &vecsize, &vec))
        pd_error(x, "%s: bad template for tabwrite", x->x_arrayname->s_name);
    else
    {
        int n = x->x_ft1;
        if (n < 0)
            n = 0;
        else if (n >= vecsize)
            n = vecsize-1;
        vec[n].w_float = f;
        garray_redraw(a);
    }
}

static void tabwrite_set(t_tabwrite *x, t_symbol *s)
{
    x->x_arrayname = s;
}

static void *tabwrite_new(t_symbol *s)
{
    t_tabwrite *x = (t_tabwrite *)pd_new(tabwrite_class);
    x->x_ft1 = 0;
    x->x_arrayname = s;
    floatinlet_new(&x->x_obj, &x->x_ft1);
    return (x);
}

void tabwrite_setup(void)
{
    tabwrite_class = class_new(gensym("tabwrite"), (t_newmethod)tabwrite_new,
        0, sizeof(t_tabwrite), 0, A_DEFSYM, 0);
    class_addfloat(tabwrite_class, (t_method)tabwrite_float);
    class_addmethod(tabwrite_class, (t_method)tabwrite_set, gensym("set"),
        A_SYMBOL, 0);
}

/* ------------------------ global setup routine ------------------------- */

void d_array_setup(void)
{
    tabwrite_tilde_setup();
    tabplay_tilde_setup();
    tabread_tilde_setup();
    tabread4_tilde_setup();
    tabsend_setup();
    tabreceive_setup();
    tabread_setup();
    tabread4_setup();
    tabwrite_setup();
}
