/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  miscellaneous: print~; more to come.
*/

#include "m_pd.h"
#include <string.h>

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


/* ------------------------ snake_in~ -------------------------- */

static t_class *snake_in_tilde_class;

typedef struct _snake_in
{
    t_object x_obj;
    t_sample x_f;
    int x_nchans;
} t_snake_in;

static void snake_in_tilde_dsp(t_snake_in *x, t_signal **sp)
{
    int i;
        /* create an n-channel output signal. sp has n+1 elements. */
    signal_setmultiout(&sp[x->x_nchans], x->x_nchans);
        /* add n copy operations to the DSP chain, one from each input */
    for (i = 0; i < x->x_nchans; i++)
         dsp_add_copy(sp[i]->s_vec,
            sp[x->x_nchans]->s_vec + i * sp[0]->s_length, sp[0]->s_length);
}

static void *snake_in_tilde_new(t_floatarg fnchans)
{
    t_snake_in *x = (t_snake_in *)pd_new(snake_in_tilde_class);
    int i;
    if ((x->x_nchans = fnchans) <= 0)
        x->x_nchans = 2;
    for (i = 1; i < x->x_nchans; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return (x);
}

/* ------------------------ snake_out~ -------------------------- */

static t_class *snake_out_tilde_class;

typedef struct _snake_out
{
    t_object x_obj;
    t_sample x_f;
    int x_nchans;
} t_snake_out;

static void snake_out_tilde_dsp(t_snake_out *x, t_signal **sp)
{
    int i, usenchans = (x->x_nchans < sp[0]->s_nchans ?
        x->x_nchans : sp[0]->s_nchans);
        /* create n one-channel output signals and add a copy operation
        for each one tothe DSP chain */
    for (i = 0; i < x->x_nchans; i++)
    {
        signal_setmultiout(&sp[i+1], 1);
        if (i < usenchans)
            dsp_add_copy(sp[0]->s_vec + i * sp[0]->s_length,
                sp[i+1]->s_vec, sp[0]->s_length);
        else dsp_add_zero(sp[i+1]->s_vec, sp[0]->s_length);
    }
}

static void *snake_out_tilde_new(t_floatarg fnchans)
{
    t_snake_out *x = (t_snake_out *)pd_new(snake_out_tilde_class);
    int i;
    if ((x->x_nchans = fnchans) <= 0)
        x->x_nchans = 2;
    for (i = 0; i < x->x_nchans; i++)
        outlet_new(&x->x_obj, &s_signal);
    return (x);
}

static void *snake_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    if (!argc || argv[0].a_type != A_SYMBOL)
        pd_this->pd_newest =
            snake_in_tilde_new(atom_getfloatarg(0, argc, argv));
    else
    {
        const char *str = argv[0].a_w.w_symbol->s_name;
        if (!strcmp(str, "in"))
            pd_this->pd_newest =
                snake_in_tilde_new(atom_getfloatarg(1, argc, argv));
        else if (!strcmp(str, "out"))
            pd_this->pd_newest =
                snake_out_tilde_new(atom_getfloatarg(1, argc, argv));
        else
        {
            pd_error(0, "list %s: unknown function", str);
            pd_this->pd_newest = 0;
        }
    }
    return (pd_this->pd_newest);
}

static void snake_tilde_setup(void)
{
    snake_in_tilde_class = class_new(gensym("snake_in~"),
        (t_newmethod)snake_in_tilde_new, 0, sizeof(t_snake_in),
            CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(snake_in_tilde_class, t_snake_in, x_f);
    class_addmethod(snake_in_tilde_class, (t_method)snake_in_tilde_dsp,
        gensym("dsp"), 0);
    class_sethelpsymbol(snake_in_tilde_class, gensym("snake-tilde"));

    snake_out_tilde_class = class_new(gensym("snake_out~"),
        (t_newmethod)snake_out_tilde_new, 0, sizeof(t_snake_out),
            CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(snake_out_tilde_class, t_snake_out, x_f);
    class_addmethod(snake_out_tilde_class, (t_method)snake_out_tilde_dsp,
        gensym("dsp"), 0);
    class_sethelpsymbol(snake_out_tilde_class, gensym("snake-tilde"));

    class_addcreator((t_newmethod)snake_tilde_new, gensym("snake~"),
        A_GIMME, 0);
}

/* ------------------------ global setup routine ------------------------- */

void d_misc_setup(void)
{
    print_setup();
    bang_tilde_setup();
    snake_tilde_setup();
}
