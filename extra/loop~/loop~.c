/* loop~ -- loop generator for sampling */

/*  Copyright 1997-1999 Miller Puckette.
Permission is granted to use this software for any purpose provided you
keep this copyright notice intact.

THE AUTHOR AND HIS EMPLOYERS MAKE NO WARRANTY, EXPRESS OR IMPLIED,
IN CONNECTION WITH THIS SOFTWARE.

This file is downloadable from http://www.crca.ucsd.edu/~msp .

*/

#ifdef PD
#include "m_pd.h"
#else
#define t_sample float
#define t_float float
#endif



typedef struct _loopctl
{
    double l_phase;
    t_sample l_invwindow;
    t_sample l_window;
    int l_resync;
} t_loopctl;

static void loopctl_run(t_loopctl *x, t_sample *transposein,
        t_sample *windowin, t_sample *rawout, t_sample *windowout, int n)
{
    t_sample window, invwindow;
    double phase = x->l_phase;
    if (x->l_resync)
    {
        window = *windowin;
        if (window < 0)
        {
            if (window > -1)
                window = -1;
            invwindow = -1/window;
        }
        else
        {
            if (window < 1)
                window = 1;
            invwindow = 1/window;
        }
        x->l_resync = 0;
    }
    else
    {
        window = x->l_window;
        phase = x->l_phase;
        invwindow = x->l_invwindow;
    }
    while (n--)
    {
        double phaseinc = invwindow * *transposein++;
        double newphase;
        t_sample nwind = *windowin++;
        if (phaseinc >= 1 || phaseinc < 0)
            phaseinc = 0;
        newphase = phase + phaseinc;
        if (newphase >= 1)
        {
            window = nwind;
            if (window < 0)
            {
                if (window > -1)
                    window = -1;
                invwindow = -1/window;
            }
            else
            {
                if (window < 1)
                    window = 1;
                invwindow = 1/window;
            }
            newphase -= 1.;
        }
        phase = newphase;
        *rawout++ = (t_sample)phase;
        *windowout++ = window;
    }
    x->l_invwindow = invwindow;
    x->l_window = window;
    x->l_phase = phase;
}

static void loopctl_init(t_loopctl *x)
{
    x->l_window = 1;
    x->l_invwindow = 1;
    x->l_phase = 0;
}

static void loopctl_set(t_loopctl *x, t_float val)
{
    if (val < 0 || val > 1)
        val = 0;
    x->l_phase = val;
    x->l_resync = 1;
}

#ifdef PD

typedef struct _loop
{
    t_object x_obj;
    t_float x_f;
    t_loopctl x_loopctl;
} t_loop;

static t_class *loop_class;

static void *loop_new(void)
{
    t_loop *x = (t_loop *)pd_new(loop_class);
    loopctl_init(&x->x_loopctl);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, gensym("signal"));
    outlet_new(&x->x_obj, gensym("signal"));
    return (x);
}

static t_int *loop_perform(t_int *w)
{
    t_loopctl *ctl = (t_loopctl *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_sample *in2 = (t_sample *)(w[3]);
    t_sample *out1 = (t_sample *)(w[4]);
    t_sample *out2 = (t_sample *)(w[5]);
    int n = (int)(w[6]);
    loopctl_run(ctl, in1, in2, out1, out2, n);
    return (w+7);
}

static void loop_dsp(t_loop *x, t_signal **sp)
{
    dsp_add(loop_perform, 6,
        &x->x_loopctl, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec,
            sp[0]->s_n);
}

static void loop_set(t_loop *x, t_floatarg val)
{
    loopctl_set(&x->x_loopctl, val);
}

static void loop_bang(t_loop *x)
{
    loopctl_set(&x->x_loopctl, 0);
}

void loop_tilde_setup(void)
{
    loop_class = class_new(gensym("loop~"), (t_newmethod)loop_new, 0,
        sizeof(t_loop), 0, 0);
    class_addmethod(loop_class, (t_method)loop_dsp, gensym("dsp"), A_CANT, 0);
    CLASS_MAINSIGNALIN(loop_class, t_loop, x_f);
    class_addmethod(loop_class, (t_method)loop_set, gensym("set"),
        A_DEFFLOAT, 0);
    class_addbang(loop_class, loop_bang);
}

#endif /* PD */
