/* bob~ - use a differential equation solver to imitate an analogue circuit */

/* copyright 2015 Miller Puckette - BSD license */

#include "m_pd.h"
#include <math.h>
#define DIM 4
#define FLOAT double

/* if CALCERROR is defined we compute an error estaimate to verify
the filter, outputting it from a second outlet on demand.  This
doubles the computation time, so it's only compiled in for testing. */

/* #define CALCERROR */

typedef struct _params
{
    FLOAT p_input;
    FLOAT p_cutoff;
    FLOAT p_resonance;
    FLOAT p_saturation;
    FLOAT p_derivativeswere[DIM];
} t_params;

    /* imitate the (tanh) clipping function of a transistor pair.  We
    hope/assume the C compiler is smart enough to inline this so use
    a function instead of a #define. */
#if 0
static FLOAT clip(FLOAT value, FLOAT saturation, FLOAT saturationinverse)
{
    return (saturation * tanh(value * saturationinverse));
}
#else
    /* cheaper way - to 4th order, tanh is x - x*x*x/3; this cubic's
    plateaus are at +/- 1 so clip to 1 and evaluate the cubic.
    This is pretty coarse - for instance if you clip a sinusoid this way you
    can sometimes hear the discontinuity in 4th derivative at the clip point */
static FLOAT clip(FLOAT value, FLOAT saturation, FLOAT saturationinverse)
{
    float v2 = (value*saturationinverse > 1 ? 1 :
        (value*saturationinverse < -1 ? -1:
            value*saturationinverse));
    return (saturation * (v2 - (1./3.) * v2 * v2 * v2));
}
#endif

static void calc_derivatives(FLOAT *dstate, FLOAT *state, t_params *params)
{
    FLOAT k = ((float)(2*3.14159)) * params->p_cutoff;
    FLOAT sat = params->p_saturation, satinv = 1./sat;
    FLOAT satstate0 = clip(state[0], sat, satinv);
    FLOAT satstate1 = clip(state[1], sat, satinv);
    FLOAT satstate2 = clip(state[2], sat, satinv);
    dstate[0] = k *
        (clip(params->p_input - params->p_resonance * state[3], sat, satinv)
            - satstate0);
    dstate[1] = k * (satstate0 - satstate1);
    dstate[2] = k * (satstate1 - satstate2);
    dstate[3] = k * (satstate2 - clip(state[3], sat, satinv));
}

static void solver_euler(FLOAT *state, FLOAT *errorestimate, 
    FLOAT stepsize, t_params *params)
{
    FLOAT cumerror = 0;
    int i;
    FLOAT derivatives[DIM];
    calc_derivatives(derivatives, state, params);
    *errorestimate = 0;
    for (i = 0; i < DIM; i++)
    {
        state[i] += stepsize * derivatives[i];
        *errorestimate += (derivatives[i] > params->p_derivativeswere[i] ?
            derivatives[i] - params->p_derivativeswere[i] :
            params->p_derivativeswere[i] - derivatives[i]);
    }
    for (i = 0; i < DIM; i++)
        params->p_derivativeswere[i] = derivatives[i];
}

static void solver_rungekutte(FLOAT *state, FLOAT *errorestimate, 
    FLOAT stepsize, t_params *params)
{
    FLOAT cumerror = 0;
    int i;
    FLOAT deriv1[DIM], deriv2[DIM], deriv3[DIM], deriv4[DIM], tempstate[DIM];
    FLOAT oldstate[DIM], backstate[DIM];
#if CALCERROR
    for (i = 0; i < DIM; i++)
        oldstate[i] = state[i];
#endif
    *errorestimate = 0;
    calc_derivatives(deriv1, state, params);
    for (i = 0; i < DIM; i++)
        tempstate[i] = state[i] + 0.5 * stepsize * deriv1[i];
    calc_derivatives(deriv2, tempstate, params);
    for (i = 0; i < DIM; i++)
        tempstate[i] = state[i] + 0.5 * stepsize * deriv2[i];
    calc_derivatives(deriv3, tempstate, params);
    for (i = 0; i < DIM; i++)
        tempstate[i] = state[i] + stepsize * deriv3[i];
    calc_derivatives(deriv4, tempstate, params);
    for (i = 0; i < DIM; i++)
        state[i] += (1./6.) * stepsize * 
            (deriv1[i] + 2 * deriv2[i] + 2 * deriv3[i] + deriv4[i]);
#if CALCERROR
    calc_derivatives(deriv1, state, params);
    for (i = 0; i < DIM; i++)
        tempstate[i] = state[i] - 0.5 * stepsize * deriv1[i];
    calc_derivatives(deriv2, tempstate, params);
    for (i = 0; i < DIM; i++)
        tempstate[i] = state[i] - 0.5 * stepsize * deriv2[i];
    calc_derivatives(deriv3, tempstate, params);
    for (i = 0; i < DIM; i++)
        tempstate[i] = state[i] - stepsize * deriv3[i];
    calc_derivatives(deriv4, tempstate, params);
    for (i = 0; i < DIM; i++)
    {
        backstate[i] = state[i ]- (1./6.) * stepsize * 
            (deriv1[i] + 2 * deriv2[i] + 2 * deriv3[i] + deriv4[i]);
        *errorestimate += (backstate[i] > oldstate[i] ?
            backstate[i] - oldstate[i] : oldstate[i] - backstate[i]);
    }
#endif
}

typedef struct _bob
{
    t_object x_obj;
    t_float x_f;
    t_outlet *x_out1;    /* signal output */
#ifdef CALCERROR
    t_outlet *x_out2;    /* error estimate */
    FLOAT x_cumerror;
#endif
    t_params x_params;
    FLOAT x_state[DIM];
    FLOAT x_sr;
    int x_oversample;
    int x_errorcount;
} t_bob;

static t_class *bob_class;

static void bob_saturation(t_bob *x, t_float saturation)
{
    if (saturation <= 1e-3)
        saturation = 1e-3;
    x->x_params.p_saturation = saturation;
}

static void bob_oversample(t_bob *x, t_float oversample)
{
    if (oversample <= 1)
        oversample = 1;
    x->x_oversample = oversample;
}

static void bob_clear(t_bob *x)
{
    int i;
    for (i = 0; i < DIM; i++)
        x->x_state[i] = x->x_params.p_derivativeswere[i] = 0;
}

static void bob_error(t_bob *x)
{
#ifdef CALCERROR
    outlet_float(x->x_out2,
        (x->x_errorcount ? x->x_cumerror/x->x_errorcount : 0));
    x->x_cumerror = 0;
    x->x_errorcount = 0;
#else
    post("error estimate unavailable (not compiled in)");
#endif
}

static void bob_print(t_bob *x)
{
    int i;
    for (i = 0; i < DIM; i++)
        post("state %d: %f", i, x->x_state[i]);
    post("saturation %f", x->x_params.p_saturation);
    post("oversample %d", x->x_oversample);
}

static void *bob_new( void)
{
    t_bob *x = (t_bob *)pd_new(bob_class);
    x->x_out1 = outlet_new(&x->x_obj, gensym("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_f = 0;
    bob_clear(x);
    bob_saturation(x, 3); 
    bob_oversample(x, 2);
#ifdef CALCERROR
    x->x_cumerror = 0;
    x->x_errorcount = 0;
    x->x_out2 = outlet_new(&x->x_obj, gensym("float"));
#endif
    return (x);
}

static t_int *bob_perform(t_int *w)
{
    t_bob *x = (t_bob *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *cutoffin = (t_float *)(w[3]);
    t_float *resonancein = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    int n = (int)(w[6]), i, j;
    FLOAT stepsize = 1./(x->x_oversample * x->x_sr);
    FLOAT errorestimate;
    for (i = 0; i < n; i++)
    {
        x->x_params.p_input = *in1++;
        x->x_params.p_cutoff = *cutoffin++;
        if ((x->x_params.p_resonance = *resonancein++) < 0)
            x->x_params.p_resonance = 0;
        for (j = 0; j < x->x_oversample; j++)
            solver_rungekutte(x->x_state, &errorestimate,
                stepsize, &x->x_params);
        *out++ = x->x_state[0];
#if CALCERROR
        x->x_cumerror += errorestimate;
        x->x_errorcount++;
#endif
    }
    return (w+7);
}

static void bob_dsp(t_bob *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    dsp_add(bob_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[0]->s_n);
}

void bob_tilde_setup(void)
{
    int i;
    bob_class = class_new(gensym("bob~"),
        (t_newmethod)bob_new, 0, sizeof(t_bob), 0, 0);
    class_addmethod(bob_class, (t_method)bob_saturation, gensym("saturation"),
        A_FLOAT, 0);
    class_addmethod(bob_class, (t_method)bob_oversample, gensym("oversample"),
        A_FLOAT, 0);
    class_addmethod(bob_class, (t_method)bob_clear, gensym("clear"), 0);
    class_addmethod(bob_class, (t_method)bob_print, gensym("print"), 0);
    class_addmethod(bob_class, (t_method)bob_error, gensym("error"), 0);

    class_addmethod(bob_class, (t_method)bob_dsp, gensym("dsp"), A_CANT, 0);
    CLASS_MAINSIGNALIN(bob_class, t_bob, x_f);
}
