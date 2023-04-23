#include "m_pd.h"

/* ------------------------ lrshift~ ----------------------------- */

static t_class *lrshift_tilde_class;

typedef struct _lrshift_tilde
{
    t_object x_obj;
    t_float  x_n;
    t_float  x_f;
} t_lrshift_tilde;

static t_int *lrshift_perform(t_int *w)
{
    t_lrshift_tilde *x = (t_lrshift_tilde *) w[1];
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out= (t_sample *)(w[3]);
    int n = (int)(w[4]);
    int shift = (int)x->x_n;
    t_sample *ptr,
        *out_end = out + n - 1,
        *in_end = in + n - 1;

    /* The loops are written this way to avoid "read after write" errors
    when *in and *out point to the same array */
    if (shift >= 0)  /* shift left, shift is a postive int or zero */
    {
        if (shift > n)
            shift = n;
        for (ptr = in + shift; ptr <= in_end;)
            *out++ = *ptr++;
        while (shift--)
            *out++ = 0.0f;
    }
    else /* shift right, shift is a negative int */
    {
        if (shift < -n)
            shift = -n;
        t_sample *out_end = out + n-1;
        for (ptr = in_end + shift; ptr >= in;)
            *out_end-- = *ptr--;
        while (shift++)
            *out_end-- = 0.0f;
    }

    return(w+5);
}

static void lrshift_tilde_dsp(t_lrshift_tilde *x, t_signal **sp)
{
    t_int n = sp[0]->s_n;
    dsp_add(lrshift_perform, 4, x,
        sp[0]->s_vec, sp[1]->s_vec, n);
}

static void *lrshift_tilde_new(t_floatarg f)
{
    t_lrshift_tilde *x = (t_lrshift_tilde *)pd_new(lrshift_tilde_class);
    x->x_n = f;
    x->x_f = 0;
    floatinlet_new(&x->x_obj, &x->x_n);
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
