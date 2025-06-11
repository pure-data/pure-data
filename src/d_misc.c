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
    int nchans = (int)(w[3]);
    int n = (int)(w[4]);
    if (x->x_count)
    {
        int i, j;
        startpost("%s:", x->x_sym->s_name);
        for (j = 0; j < nchans; j++)
        {
            if (nchans > 1)
            {
                endpost();
                startpost("channel %d:", j + 1);
            }
            for (i = 0; i < n; i++) {
                if (i % 8 == 0)
                    endpost();
                startpost("%.4g  ", in[j * n + i]);
            }
        }
        endpost();
        x->x_count--;
    }
    return (w+5);
}

static void print_dsp(t_print *x, t_signal **sp)
{
    dsp_add(print_perform, 4, x, sp[0]->s_vec,
        (t_int)sp[0]->s_nchans, (t_int)sp[0]->s_n);
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
        sizeof(t_print), CLASS_MULTICHANNEL, A_DEFSYM, 0);
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

static void *bang_tilde_new(void)
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
    int x_nin;
} t_snake_in;

static void snake_in_tilde_dsp(t_snake_in *x, t_signal **sp)
{
    int i, j, outchan = 0;
        /* count total channels across all (multichannel) inputs */
    for (i = 0; i < x->x_nin; i++)
        outchan += sp[i]->s_nchans;
        /* create output signal with total channel count. sp has n+1 elements. */
    signal_setmultiout(&sp[x->x_nin], outchan);
        /* add copy operations to DSP chain, one per channel from each input */
    for (outchan = 0, i = 0; i < x->x_nin; i++)
        for (j = 0; j < sp[i]->s_nchans; j++, outchan++)
            dsp_add_copy(sp[i]->s_vec + j * sp[i]->s_length,
                sp[x->x_nin]->s_vec + outchan * sp[i]->s_length,
                sp[i]->s_length);
}

static void *snake_in_tilde_new(t_floatarg fnchans)
{
    t_snake_in *x = (t_snake_in *)pd_new(snake_in_tilde_class);
    int i;
    if ((x->x_nin = fnchans) <= 0)
        x->x_nin = 2;
    for (i = 1; i < x->x_nin; i++)
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

/* ------------------------ snake_count~ -------------------------- */

static t_class *snake_count_tilde_class;

typedef struct _snake_count
{
    t_object x_obj;
    t_sample x_f;
    int x_nchans;
} t_snake_count;

static void snake_count_tilde_dsp(t_snake_count *x, t_signal **sp)
{
        /* store actual input channel count */
    x->x_nchans = sp[0]->s_nchans;
}

static void snake_count_tilde_bang(t_snake_count *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_nchans);
}

static void *snake_count_tilde_new(void)
{
    t_snake_count *x = (t_snake_count *)pd_new(snake_count_tilde_class);
        /* default to single channel */
    x->x_nchans = 1;
    outlet_new(&x->x_obj, &s_float);
    return (x);
}

/* ------------------------ snake_sum~ -------------------------- */

static t_class *snake_sum_tilde_class;

typedef struct _snake_sum
{
    t_object x_obj;
    t_sample x_f;
    int x_bypass;
} t_snake_sum;

static void snake_sum_tilde_dsp(t_snake_sum *x, t_signal **sp)
{
    int i;
    if (x->x_bypass)
    {
            /* bypass mode: forward all input channels */
        signal_setmultiout(&sp[1], sp[0]->s_nchans);
        for (i = 0; i < sp[0]->s_nchans; i++)
            dsp_add_copy(sp[0]->s_vec + i * sp[0]->s_length,
                sp[1]->s_vec + i * sp[0]->s_length, sp[0]->s_length);
    }
    else
    {
            /* create single channel output signal */
        signal_setmultiout(&sp[1], 1);
            /* copy first channel to output */
        dsp_add_copy(sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_length);
            /* add remaining channels */
        for (i = 1; i < sp[0]->s_nchans; i++)
            dsp_add_plus(sp[1]->s_vec,
                sp[0]->s_vec + i * sp[0]->s_length,
                sp[1]->s_vec, sp[0]->s_length);
    }
}

static void snake_sum_tilde_bypass(t_snake_sum *x, t_float f)
{
    x->x_bypass = (f != 0);
    canvas_update_dsp();
}

static void *snake_sum_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_snake_sum *x = (t_snake_sum *)pd_new(snake_sum_tilde_class);
    x->x_bypass = 0;

        /* check for bypass flag */
    while (argc--) {
        if (argv->a_type == A_SYMBOL) {
            if (argv->a_w.w_symbol == gensym("-b"))
                x->x_bypass = 1;
        }
        argv++;
    }

    outlet_new(&x->x_obj, &s_signal);
    return (x);
}

/* ------------------------ snake_split~ ------------------------- */

static t_class *snake_split_tilde_class;

typedef struct _snake_split
{
    t_object x_obj;
    t_sample x_f;
    int x_index;        /* split index (0-based) */
} t_snake_split;

static void snake_split_tilde_dsp(t_snake_split *x, t_signal **sp)
{
    int i, nchans = sp[0]->s_nchans;
    int left_chans, right_chans;

        /* calculate output channel counts */
    if (x->x_index <= 0) {
        left_chans = 1;     /* single channel of zeros */
        right_chans = nchans;
    } else if (x->x_index >= nchans) {
        left_chans = nchans;
        right_chans = 1;    /* single channel of zeros */
    } else {
        left_chans = x->x_index;
        right_chans = nchans - x->x_index;
    }

        /* set up output signals */
    signal_setmultiout(&sp[1], left_chans);   /* left output */
    signal_setmultiout(&sp[2], right_chans);  /* right output */

        /* route channels to outputs */
    if (x->x_index <= 0) {
            /* left output: zeros, right output: all channels */
        dsp_add_zero(sp[1]->s_vec, sp[1]->s_length);
        dsp_add_copy(sp[0]->s_vec, sp[2]->s_vec, nchans * sp[0]->s_length);
    } else if (x->x_index >= nchans) {
            /* left output: all channels, right output: zeros */
        dsp_add_copy(sp[0]->s_vec, sp[1]->s_vec, nchans * sp[0]->s_length);
        dsp_add_zero(sp[2]->s_vec, sp[2]->s_length);
    } else {
            /* normal split */
        for (i = 0; i < left_chans; i++)
            dsp_add_copy(sp[0]->s_vec + i * sp[0]->s_length,
                sp[1]->s_vec + i * sp[1]->s_length, sp[0]->s_length);
        for (i = 0; i < right_chans; i++)
            dsp_add_copy(sp[0]->s_vec + (x->x_index + i) * sp[0]->s_length,
                sp[2]->s_vec + i * sp[2]->s_length, sp[0]->s_length);
    }
}

static void snake_split_tilde_index(t_snake_split *x, t_floatarg f)
{
    x->x_index = (int)f;
    canvas_update_dsp();
}

static void *snake_split_tilde_new(t_floatarg f)
{
    t_snake_split *x = (t_snake_split *)pd_new(snake_split_tilde_class);
    x->x_index = (int)f;
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("index"));
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return (x);
}

/* ------------------------ snake_pick~ -------------------------- */

static t_class *snake_pick_tilde_class;

typedef struct _snake_pick
{
    t_object x_obj;
    t_sample x_f;
    int x_npick;        /* number of channels to pick */
    int *x_indices;     /* array of channel indices */
} t_snake_pick;

static void snake_pick_tilde_dsp(t_snake_pick *x, t_signal **sp)
{
    int i, srcindex;
    t_signal *inputcopy;

        /* if no indices set, default to pass-through */
    if (x->x_npick == 0)
    {
        signal_setmultiout(&sp[1], sp[0]->s_nchans);
        dsp_add_copy(sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_nchans * sp[0]->s_length);
        return;
    }

    signal_setmultiout(&sp[1], x->x_npick);
        /* use temporary buffer to avoid overwriting input during reordering */
    inputcopy = signal_new(sp[0]->s_length, sp[0]->s_nchans, sp[0]->s_sr, 0);
    for (i = 0; i < sp[0]->s_nchans; i++)
        dsp_add_copy(sp[0]->s_vec + i * sp[0]->s_length,
            inputcopy->s_vec + i * sp[0]->s_length, sp[0]->s_length);
    for (i = 0; i < x->x_npick; i++)
    {
        if ((srcindex = x->x_indices[i]) >= 0 && srcindex < sp[0]->s_nchans)
            dsp_add_copy(inputcopy->s_vec + srcindex * sp[0]->s_length,
                sp[1]->s_vec + i * sp[0]->s_length, sp[0]->s_length);
        else
                /* output zeros for invalid or missing input channels */
            dsp_add_zero(sp[1]->s_vec + i * sp[0]->s_length, sp[0]->s_length);
    }
}

static void snake_pick_tilde_channels(t_snake_pick *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;

        /* (re)allocate indices array if channel count changed */
    if (argc != x->x_npick)
    {
        if (x->x_indices)
            freebytes(x->x_indices, x->x_npick * sizeof(int));
        x->x_indices = argc ? (int *)getbytes(argc * sizeof(int)) : NULL;
        x->x_npick = argc;
    }

        /* update indices (convert from 1-based to 0-based) */
    for (i = 0; i < argc; i++)
        x->x_indices[i] = (int)atom_getfloatarg(i, argc, argv) - 1;
    canvas_update_dsp();
}

static void snake_pick_tilde_free(t_snake_pick *x)
{
    if (x->x_indices)
        freebytes(x->x_indices, x->x_npick * sizeof(int));
}

static void *snake_pick_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_snake_pick *x = (t_snake_pick *)pd_new(snake_pick_tilde_class);

    x->x_npick = 0;
    x->x_indices = NULL;
    snake_pick_tilde_channels(x, s, argc, argv);

    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_list, gensym("channels"));
    outlet_new(&x->x_obj, &s_signal);
    return (x);
}

/* ------------------------ snake_fromlist~ ------------------------- */

static t_class *snake_fromlist_tilde_class;

typedef struct _snake_fromlist
{
    t_object x_obj;
    t_sample x_f;
    int x_nchans;       /* number of channels (list length) */
    t_sample *x_values; /* array of channel values */
} t_snake_fromlist;

static void snake_fromlist_tilde_dsp(t_snake_fromlist *x, t_signal **sp)
{
    int i;
    signal_setmultiout(&sp[0], x->x_nchans);

        /* generate constant signals for each channel */
    for (i = 0; i < x->x_nchans; i++)
        dsp_add_scalarcopy(&x->x_values[i], sp[0]->s_vec + i * sp[0]->s_length,
            sp[0]->s_length);
}

static void snake_fromlist_tilde_list(t_snake_fromlist *x, t_symbol *s, int argc, t_atom *argv)
{
    int i, new_nchans = argc ? argc : 1;

    if (new_nchans != x->x_nchans)
    {
        x->x_values = (t_sample *)resizebytes(x->x_values,
            x->x_nchans * sizeof(t_sample), new_nchans * sizeof(t_sample));
        x->x_nchans = new_nchans;
    }

    for (i = 0; i < argc; i++)
        x->x_values[i] = atom_getfloatarg(i, argc, argv);
    if (!argc)
        x->x_values[0] = 0;

    canvas_update_dsp();
}

static void snake_fromlist_tilde_free(t_snake_fromlist *x)
{
    if (x->x_values)
        freebytes(x->x_values, x->x_nchans * sizeof(t_sample));
}

static void *snake_fromlist_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_snake_fromlist *x = (t_snake_fromlist *)pd_new(snake_fromlist_tilde_class);

    x->x_nchans = 0;
    x->x_values = NULL;
    snake_fromlist_tilde_list(x, s, argc, argv);

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
        else if (!strcmp(str, "count"))
            pd_this->pd_newest =
                snake_count_tilde_new();
        else if (!strcmp(str, "sum"))
            pd_this->pd_newest =
                snake_sum_tilde_new(s, argc-1, argv+1);
        else if (!strcmp(str, "split"))
            pd_this->pd_newest =
                snake_split_tilde_new(atom_getfloatarg(1, argc, argv));
        else if (!strcmp(str, "pick"))
            pd_this->pd_newest =
                snake_pick_tilde_new(s, argc-1, argv+1);
        else if (!strcmp(str, "fromlist"))
            pd_this->pd_newest =
                snake_fromlist_tilde_new(s, argc-1, argv+1);
        else
        {
            pd_error(0, "snake~ %s: unknown function", str);
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

    snake_count_tilde_class = class_new(gensym("snake_count~"),
        (t_newmethod)snake_count_tilde_new, 0, sizeof(t_snake_count),
            CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(snake_count_tilde_class, t_snake_count, x_f);
    class_addmethod(snake_count_tilde_class, (t_method)snake_count_tilde_dsp,
        gensym("dsp"), 0);
    class_addbang(snake_count_tilde_class, snake_count_tilde_bang);
    class_sethelpsymbol(snake_count_tilde_class, gensym("snake-tilde"));

    snake_sum_tilde_class = class_new(gensym("snake_sum~"),
        (t_newmethod)snake_sum_tilde_new, 0, sizeof(t_snake_sum),
            CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(snake_sum_tilde_class, t_snake_sum, x_f);
    class_addmethod(snake_sum_tilde_class, (t_method)snake_sum_tilde_dsp,
        gensym("dsp"), 0);
    class_addmethod(snake_sum_tilde_class, (t_method)snake_sum_tilde_bypass,
        gensym("bypass"), A_FLOAT, 0);
    class_sethelpsymbol(snake_sum_tilde_class, gensym("snake-tilde"));

    snake_split_tilde_class = class_new(gensym("snake_split~"),
        (t_newmethod)snake_split_tilde_new, 0, sizeof(t_snake_split),
            CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(snake_split_tilde_class, t_snake_split, x_f);
    class_addmethod(snake_split_tilde_class, (t_method)snake_split_tilde_dsp,
        gensym("dsp"), 0);
    class_addmethod(snake_split_tilde_class, (t_method)snake_split_tilde_index,
        gensym("index"), A_FLOAT, 0);
    class_sethelpsymbol(snake_split_tilde_class, gensym("snake-tilde"));

    snake_pick_tilde_class = class_new(gensym("snake_pick~"),
        (t_newmethod)snake_pick_tilde_new, (t_method)snake_pick_tilde_free,
            sizeof(t_snake_pick), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(snake_pick_tilde_class, t_snake_pick, x_f);
    class_addmethod(snake_pick_tilde_class, (t_method)snake_pick_tilde_dsp,
        gensym("dsp"), 0);
    class_addmethod(snake_pick_tilde_class, (t_method)snake_pick_tilde_channels,
        gensym("channels"), A_GIMME, 0);
    class_sethelpsymbol(snake_pick_tilde_class, gensym("snake-tilde"));

    snake_fromlist_tilde_class = class_new(gensym("snake_fromlist~"),
        (t_newmethod)snake_fromlist_tilde_new, (t_method)snake_fromlist_tilde_free,
            sizeof(t_snake_fromlist), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(snake_fromlist_tilde_class, (t_method)snake_fromlist_tilde_dsp,
        gensym("dsp"), 0);
    class_addlist(snake_fromlist_tilde_class, snake_fromlist_tilde_list);
    class_sethelpsymbol(snake_fromlist_tilde_class, gensym("snake-tilde"));

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
