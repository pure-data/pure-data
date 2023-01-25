/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  miscellaneous: print~; more to come.
*/

#include "m_pd.h"

/* ------------------------- print~ -------------------------- */
static t_class *print_class;

typedef struct _print
{
    t_object x_obj;
    t_float x_f;
    t_symbol *x_sym;
    int x_count;
} t_print;

static t_int *print_perform(t_int *w)
{
    t_print *x = (t_print *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    if (x->x_count)
    {
        int i=0;
        startpost("%s:", x->x_sym->s_name);
        for(i=0; i<n; i++) {
          if(i%8==0)endpost();
          startpost("%.4g  ", in[i]);
        }
        endpost();
        x->x_count--;
    }
    return (w+4);
}

static void print_dsp(t_print *x, t_signal **sp)
{
    dsp_add(print_perform, 3, x, sp[0]->s_vec, (t_int)sp[0]->s_n);
}

static void print_float(t_print *x, t_float f)
{
    if (f < 0) f = 0;
    x->x_count = f;
}

static void print_bang(t_print *x)
{
    x->x_count = 1;
}

static void *print_new(t_symbol *s)
{
    t_print *x = (t_print *)pd_new(print_class);
    x->x_sym = (s->s_name[0]? s : gensym("print~"));
    x->x_count = 0;
    x->x_f = 0;
    return (x);
}

static void print_setup(void)
{
    print_class = class_new(gensym("print~"), (t_newmethod)print_new, 0,
        sizeof(t_print), 0, A_DEFSYM, 0);
    CLASS_MAINSIGNALIN(print_class, t_print, x_f);
    class_addmethod(print_class, (t_method)print_dsp, gensym("dsp"), A_CANT, 0);
    class_addbang(print_class, print_bang);
    class_addfloat(print_class, print_float);
}

/* ------------------------ bang~ -------------------------- */

static t_class *bang_tilde_class;

typedef struct _bang
{
    t_object x_obj;
    t_clock *x_clock;
} t_bang;

static t_int *bang_tilde_perform(t_int *w)
{
    t_bang *x = (t_bang *)(w[1]);
    clock_delay(x->x_clock, 0);
    return (w+2);
}

static void bang_tilde_dsp(t_bang *x, t_signal **sp)
{
    dsp_add(bang_tilde_perform, 1, x);
}

static void bang_tilde_tick(t_bang *x)
{
    outlet_bang(x->x_obj.ob_outlet);
}

static void bang_tilde_free(t_bang *x)
{
    clock_free(x->x_clock);
}

static void *bang_tilde_new(t_symbol *s)
{
    t_bang *x = (t_bang *)pd_new(bang_tilde_class);
    x->x_clock = clock_new(x, (t_method)bang_tilde_tick);
    outlet_new(&x->x_obj, &s_bang);
    return (x);
}

static void bang_tilde_setup(void)
{
    bang_tilde_class = class_new(gensym("bang~"), (t_newmethod)bang_tilde_new,
        (t_method)bang_tilde_free, sizeof(t_bang), 0, 0);
    class_addmethod(bang_tilde_class, (t_method)bang_tilde_dsp,
        gensym("dsp"), 0);
}


/* ------------------------ pack~ -------------------------- */

static t_class *pack_tilde_class;

typedef struct _pack
{
    t_object x_obj;
    t_sample x_f;
    int x_nchans;
} t_pack;

static void pack_tilde_dsp(t_pack *x, t_signal **sp)
{
    int i;
        /* create an n-channel output signal. sp has n+1 elements. */
    signal_setchansout(&sp[x->x_nchans], x->x_nchans);
        /* add n copy operations to the DSP chain, one from each input */
    for (i = 0; i < x->x_nchans; i++)
         dsp_add_copy(sp[i]->s_vec,
            sp[x->x_nchans]->s_vec + i * sp[0]->s_length, sp[0]->s_length);
}

static void *pack_tilde_new(t_floatarg fnchans)
{
    t_pack *x = (t_pack *)pd_new(pack_tilde_class);
    int i;
    if ((x->x_nchans = fnchans) <= 0)
        x->x_nchans = 2;
    for (i = 1; i < x->x_nchans; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return (x);
}

static void pack_tilde_setup(void)
{
    pack_tilde_class = class_new(gensym("pack~"),
        (t_newmethod)pack_tilde_new, 0, sizeof(t_pack), 0, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(pack_tilde_class, t_pack, x_f);
    class_setdspflags(pack_tilde_class, CLASS_MULTICHANNEL);
    class_addmethod(pack_tilde_class, (t_method)pack_tilde_dsp,
        gensym("dsp"), 0);
    class_sethelpsymbol(pack_tilde_class, gensym("pack-unpack-tilde"));
}

/* ------------------------ unpack~ -------------------------- */

static t_class *unpack_tilde_class;

typedef struct _unpack
{
    t_object x_obj;
    t_sample x_f;
    int x_nchans;
} t_unpack;

static void unpack_tilde_dsp(t_unpack *x, t_signal **sp)
{
    int i, usenchans = (x->x_nchans < sp[0]->s_nchans ?
        x->x_nchans : sp[0]->s_nchans);
        /* create n one-channel output signals and add a copy operation
        for each one tothe DSP chain */
    for (i = 0; i < x->x_nchans; i++)
    {
        signal_setchansout(&sp[i+1], 1);
        if (i < usenchans)
            dsp_add_copy(sp[0]->s_vec + i * sp[0]->s_length,
                sp[i+1]->s_vec, sp[0]->s_length);
        else dsp_add_zero(sp[i+1]->s_vec, sp[0]->s_length);
    }
}

static void *unpack_tilde_new(t_floatarg fnchans)
{
    t_unpack *x = (t_unpack *)pd_new(unpack_tilde_class);
    int i;
    if ((x->x_nchans = fnchans) <= 0)
        x->x_nchans = 2;
    for (i = 0; i < x->x_nchans; i++)
        outlet_new(&x->x_obj, &s_signal);
    return (x);
}

static void unpack_tilde_setup(void)
{
    unpack_tilde_class = class_new(gensym("unpack~"),
        (t_newmethod)unpack_tilde_new, 0, sizeof(t_unpack), 0, A_DEFFLOAT, 0);
    class_setdspflags(unpack_tilde_class, CLASS_MULTICHANNEL);
    CLASS_MAINSIGNALIN(unpack_tilde_class, t_unpack, x_f);
    class_addmethod(unpack_tilde_class, (t_method)unpack_tilde_dsp,
        gensym("dsp"), 0);
    class_sethelpsymbol(unpack_tilde_class, gensym("pack-unpack-tilde"));
}

/* ------------------------ global setup routine ------------------------- */

void d_misc_setup(void)
{
    print_setup();
    bang_tilde_setup();
    pack_tilde_setup();
    unpack_tilde_setup();
}
