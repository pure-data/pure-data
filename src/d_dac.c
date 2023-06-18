/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  The dac~ and adc~ routines.
*/

#include "m_pd.h"
#include "s_stuff.h"
#include <string.h>

/* ----------------------------- dac~ --------------------------- */
static t_class *dac_class;

typedef struct _dac
{
    t_object x_obj;
    t_int x_n;
    t_int *x_vec;
    t_float x_f;
} t_dac;

static void *dac_new(t_symbol *s, int argc, t_atom *argv)
{
    t_dac *x = (t_dac *)pd_new(dac_class);
    t_atom defarg[2];
    int i;
    if (!argc)
    {
        argv = defarg;
        argc = 2;
        SETFLOAT(&defarg[0], 1);
        SETFLOAT(&defarg[1], 2);
    }
    x->x_n = argc;
    x->x_vec = (t_int *)getbytes(argc * sizeof(*x->x_vec));
    for (i = 0; i < argc; i++)
        x->x_vec[i] = atom_getfloatarg(i, argc, argv);
    for (i = 1; i < argc; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_f = 0;
    return (x);
}

static void dac_dsp(t_dac *x, t_signal **sp)
{
    t_int i, j;
    for (i = 0; i < x->x_n; i++)
    {
        int ch = (int)(x->x_vec[i] - 1);
        if (sp[i]->s_length != DEFDACBLKSIZE)
            pd_error(x,
              "dac~: input vector size (%d) doesn't match Pd vector size (%d)",
                    sp[i]->s_length, DEFDACBLKSIZE);
        else for (j = 0; j < sp[i]->s_nchans; j++)
        {
            if (ch + j >= 0 && ch + j < sys_get_outchannels())
                dsp_add(plus_perform, 4,
                    STUFF->st_soundout + DEFDACBLKSIZE * (ch + j),
                sp[i]->s_vec + j * sp[i]->s_length,
                    STUFF->st_soundout + DEFDACBLKSIZE * (ch + j),
                        (t_int)DEFDACBLKSIZE);
        }
    }
}

static void dac_set(t_dac *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    for (i = 0; i < argc && i < x->x_n; i++)
        x->x_vec[i] = atom_getfloatarg(i, argc, argv);
    canvas_update_dsp();
}

static void dac_free(t_dac *x)
{
    freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
}

static void dac_setup(void)
{
    dac_class = class_new(gensym("dac~"), (t_newmethod)dac_new,
        (t_method)dac_free, sizeof(t_dac), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(dac_class, t_dac, x_f);
    class_addmethod(dac_class, (t_method)dac_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(dac_class, (t_method)dac_set, gensym("set"), A_GIMME, 0);
    class_sethelpsymbol(dac_class, gensym("adc~_dac~"));
}

/* ----------------------------- adc~ --------------------------- */
static t_class *adc_class;

typedef struct _adc
{
    t_object x_obj;
    int x_n;
    int *x_vec;
    int x_multi;
} t_adc;

static void *adc_new(t_symbol *s, int argc, t_atom *argv)
{
    t_adc *x = (t_adc *)pd_new(adc_class);
    t_atom defarg[2];
    int i, firstchan;
    if (!argc)
    {
        argv = defarg;
        argc = 2;
        SETFLOAT(&defarg[0], 1);
        SETFLOAT(&defarg[1], 2);
    }
    if (argc >= 2 && argv[0].a_type == A_SYMBOL &&
        !strcmp(argv[0].a_w.w_symbol->s_name, "-m"))
    {       /* multichannel version: -m [nchans] [start channel] */
        x->x_multi = 1;
        if ((x->x_n = atom_getfloatarg(1, argc, argv)) < 1)
            x->x_n = 2;
        if ((firstchan = atom_getfloatarg(2, argc, argv)) < 1)
            firstchan = 1;
        x->x_vec = (int *)getbytes(argc * sizeof(*x->x_vec));
        for (i = 0; i < x->x_n; i++)
            x->x_vec[i] = firstchan+i;
        outlet_new(&x->x_obj, &s_signal);
    }
    else
    {
        x->x_multi = 0;
        x->x_n = argc;
        x->x_vec = (int *)getbytes(argc * sizeof(*x->x_vec));
        for (i = 0; i < argc; i++)
            x->x_vec[i] = atom_getfloatarg(i, argc, argv);
        for (i = 0; i < x->x_n; i++)
            outlet_new(&x->x_obj, &s_signal);
    }
    return (x);
}

static void adc_dsp(t_adc *x, t_signal **sp)
{
    int i;
    if (x->x_multi)
        signal_setmultiout(sp, x->x_n);
    else for (i = 0; i < x->x_n; i++)
        signal_setmultiout(&sp[i], 1);
    if (sp[0]->s_length != DEFDACBLKSIZE)
    {
        pd_error(0, "adc~: local vector size %d doesn't match system (%d)",
            sp[0]->s_length, DEFDACBLKSIZE);
        return;
    }
    for (i = 0; i < x->x_n; i++)
    {
        int ch = x->x_vec[i] - 1;
        t_sample *out = (x->x_multi? sp[0]->s_vec + sp[0]->s_length * i:
            sp[i]->s_vec);
        if (ch >= 0 && ch < sys_get_inchannels())
            dsp_add_copy(STUFF->st_soundin + DEFDACBLKSIZE*ch,
                out, DEFDACBLKSIZE);
        else dsp_add_zero(out, DEFDACBLKSIZE);
    }
}

static void adc_set(t_adc *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    if (x->x_multi)
    {
        int nchannels = atom_getfloatarg(0, argc, argv),
            startchannel = atom_getfloatarg(1, argc, argv);
        if (nchannels < 1)
            nchannels = 2;
        if (startchannel < 1)
            startchannel = 1;
        x->x_vec = (int *)t_resizebytes(x->x_vec,
            x->x_n * sizeof(*x->x_vec), nchannels * sizeof(*x->x_vec));
        for (i = 0; i < nchannels; i++)
            x->x_vec[i] = startchannel + i;
        x->x_n = nchannels;
    }
    else for (i = 0; i < argc && i < x->x_n; i++)
        x->x_vec[i] = atom_getfloatarg(i, argc, argv);
    canvas_update_dsp();
}

static void adc_free(t_adc *x)
{
    freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
}

static void adc_setup(void)
{
    adc_class = class_new(gensym("adc~"), (t_newmethod)adc_new,
        (t_method)adc_free, sizeof(t_adc), 0, A_GIMME, 0);
    class_addmethod(adc_class, (t_method)adc_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(adc_class, (t_method)adc_set, gensym("set"), A_GIMME, 0);
    class_sethelpsymbol(adc_class, gensym("adc~_dac~"));
}

void d_dac_setup(void)
{
    dac_setup();
    adc_setup();
}

