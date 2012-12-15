#include "m_pd.h"

/* ------------------------ lrshift~ ----------------------------- */

static t_class *lrshift_tilde_class;

typedef struct _lrshift_tilde
{
    t_object x_obj;
    int x_n;
    t_float x_f;
} t_lrshift_tilde;

static t_int *leftshift_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out= (t_sample *)(w[2]);
    int n = (int)(w[3]);
    int shift = (int)(w[4]);
    in += shift;
    n -= shift;
    while (n--)
        *out++ = *in++;
    while (shift--)
        *out++ = 0;
    return (w+5);
}

static t_int *rightshift_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out= (t_sample *)(w[2]);
    int n = (int)(w[3]);
    int shift = (int)(w[4]);
    n -= shift;
    in -= shift;
    while (n--)
        *--out = *--in;
    while (shift--)
        *--out = 0;
    return (w+5);
}

static void lrshift_tilde_dsp(t_lrshift_tilde *x, t_signal **sp)
{
    int n = sp[0]->s_n;
    int shift = x->x_n;
    if (shift > n)
        shift = n;
    if (shift < -n)
        shift = -n;
    if (shift < 0)
        dsp_add(rightshift_perform, 4,
            sp[0]->s_vec + n, sp[1]->s_vec + n, n, -shift);
    else dsp_add(leftshift_perform, 4,
            sp[0]->s_vec, sp[1]->s_vec, n, shift);
}

static void *lrshift_tilde_new(t_floatarg f)
{
    t_lrshift_tilde *x = (t_lrshift_tilde *)pd_new(lrshift_tilde_class);
    x->x_n = f;
    x->x_f = 0;
    outlet_new(&x->x_obj, gensym("signal"));
    return (x);
}

void lrshift_tilde_setup(void)
{
    lrshift_tilde_class = class_new(gensym("lrshift~"),
        (t_newmethod)lrshift_tilde_new, 0, sizeof(t_lrshift_tilde), 0, 
            A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(lrshift_tilde_class, t_lrshift_tilde, x_f);
    class_addmethod(lrshift_tilde_class, (t_method)lrshift_tilde_dsp,
        gensym("dsp"), 0);
}
