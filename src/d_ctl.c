/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  sig~ and line~ control-to-signal converters;
    snapshot~ signal-to-control converter.
*/

#include "m_pd.h"
#include "math.h"

/* -------------------------- sig~ ------------------------------ */
static t_class *sig_tilde_class;

typedef struct _sig
{
    t_object x_obj;
    t_float x_f;
} t_sig;

static t_int *sig_tilde_perform(t_int *w)
{
    t_float f = *(t_float *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    while (n--)
        *out++ = f; 
    return (w+4);
}

static t_int *sig_tilde_perf8(t_int *w)
{
    t_float f = *(t_float *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    
    for (; n; n -= 8, out += 8)
    {
        out[0] = f;
        out[1] = f;
        out[2] = f;
        out[3] = f;
        out[4] = f;
        out[5] = f;
        out[6] = f;
        out[7] = f;
    }
    return (w+4);
}

void dsp_add_scalarcopy(t_float *in, t_sample *out, int n)
{
    if (n&7)
        dsp_add(sig_tilde_perform, 3, in, out, n);
    else        
        dsp_add(sig_tilde_perf8, 3, in, out, n);
}

static void sig_tilde_float(t_sig *x, t_float f)
{
    x->x_f = f;
}

static void sig_tilde_dsp(t_sig *x, t_signal **sp)
{
    dsp_add(sig_tilde_perform, 3, &x->x_f, sp[0]->s_vec, sp[0]->s_n);
}

static void *sig_tilde_new(t_floatarg f)
{
    t_sig *x = (t_sig *)pd_new(sig_tilde_class);
    x->x_f = f;
    outlet_new(&x->x_obj, gensym("signal"));
    return (x);
}

static void sig_tilde_setup(void)
{
    sig_tilde_class = class_new(gensym("sig~"), (t_newmethod)sig_tilde_new, 0,
        sizeof(t_sig), 0, A_DEFFLOAT, 0);
    class_addfloat(sig_tilde_class, (t_method)sig_tilde_float);
    class_addmethod(sig_tilde_class, (t_method)sig_tilde_dsp, gensym("dsp"), 0);
}

/* -------------------------- line~ ------------------------------ */
static t_class *line_tilde_class;

typedef struct _line
{
    t_object x_obj;
    t_sample x_target; /* target value of ramp */
    t_sample x_value; /* current value of ramp at block-borders */
    t_sample x_biginc;
    t_sample x_inc;
    t_float x_1overn;
    t_float x_dspticktomsec;
    t_float x_inletvalue;
    t_float x_inletwas;
    int x_ticksleft;
    int x_retarget;
} t_line;

static t_int *line_tilde_perform(t_int *w)
{
    t_line *x = (t_line *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    t_sample f = x->x_value;

    if (PD_BIGORSMALL(f))
            x->x_value = f = 0;
    if (x->x_retarget)
    {
        int nticks = x->x_inletwas * x->x_dspticktomsec;
        if (!nticks) nticks = 1;
        x->x_ticksleft = nticks;
        x->x_biginc = (x->x_target - x->x_value)/(t_float)nticks;
        x->x_inc = x->x_1overn * x->x_biginc;
        x->x_retarget = 0;
    }
    if (x->x_ticksleft)
    {
        t_sample f = x->x_value;
        while (n--) *out++ = f, f += x->x_inc;
        x->x_value += x->x_biginc;
        x->x_ticksleft--;
    }
    else
    {
        t_sample g = x->x_value = x->x_target;
        while (n--)
            *out++ = g;
    }
    return (w+4);
}

/* TB: vectorized version */
static t_int *line_tilde_perf8(t_int *w)
{
    t_line *x = (t_line *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    t_sample f = x->x_value;

    if (PD_BIGORSMALL(f))
        x->x_value = f = 0;
    if (x->x_retarget)
    {
        int nticks = x->x_inletwas * x->x_dspticktomsec;
        if (!nticks) nticks = 1;
        x->x_ticksleft = nticks;
        x->x_biginc = (x->x_target - x->x_value)/(t_sample)nticks;
        x->x_inc = x->x_1overn * x->x_biginc;
        x->x_retarget = 0;
    }
    if (x->x_ticksleft)
    {
        t_sample f = x->x_value;
        while (n--) *out++ = f, f += x->x_inc;
        x->x_value += x->x_biginc;
        x->x_ticksleft--;
    }
    else
    {
        t_sample f = x->x_value = x->x_target;
        for (; n; n -= 8, out += 8)
        {
            out[0] = f; out[1] = f; out[2] = f; out[3] = f; 
            out[4] = f; out[5] = f; out[6] = f; out[7] = f;
        }
    }
    return (w+4);
}

static void line_tilde_float(t_line *x, t_float f)
{
    if (x->x_inletvalue <= 0)
    {
        x->x_target = x->x_value = f;
        x->x_ticksleft = x->x_retarget = 0;
    }
    else
    {
        x->x_target = f;
        x->x_retarget = 1;
        x->x_inletwas = x->x_inletvalue;
        x->x_inletvalue = 0;
    }
}

static void line_tilde_stop(t_line *x)
{
    x->x_target = x->x_value;
    x->x_ticksleft = x->x_retarget = 0;
}

static void line_tilde_dsp(t_line *x, t_signal **sp)
{
    if(sp[0]->s_n&7)
        dsp_add(line_tilde_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
    else
        dsp_add(line_tilde_perf8, 3, x, sp[0]->s_vec, sp[0]->s_n);
    x->x_1overn = 1./sp[0]->s_n;
    x->x_dspticktomsec = sp[0]->s_sr / (1000 * sp[0]->s_n);
}

static void *line_tilde_new(void)
{
    t_line *x = (t_line *)pd_new(line_tilde_class);
    outlet_new(&x->x_obj, gensym("signal"));
    floatinlet_new(&x->x_obj, &x->x_inletvalue);
    x->x_ticksleft = x->x_retarget = 0;
    x->x_value = x->x_target = x->x_inletvalue = x->x_inletwas = 0;
    return (x);
}

static void line_tilde_setup(void)
{
    line_tilde_class = class_new(gensym("line~"), line_tilde_new, 0,
        sizeof(t_line), 0, 0);
    class_addfloat(line_tilde_class, (t_method)line_tilde_float);
    class_addmethod(line_tilde_class, (t_method)line_tilde_dsp,
        gensym("dsp"), 0);
    class_addmethod(line_tilde_class, (t_method)line_tilde_stop,
        gensym("stop"), 0);
}

/* -------------------------- vline~ ------------------------------ */
static t_class *vline_tilde_class;
#include "s_stuff.h"    /* for DEFDACBLKSIZE; this should be in m_pd.h */
typedef struct _vseg
{
    double s_targettime;
    double s_starttime;
    t_sample s_target;
    struct _vseg *s_next;
} t_vseg;

typedef struct _vline
{
    t_object x_obj;
    double x_value;
    double x_inc;
    double x_referencetime;
    double x_lastlogicaltime;
    double x_nextblocktime;
    double x_samppermsec;
    double x_msecpersamp;
    double x_targettime;
    t_sample x_target;
    t_float x_inlet1;
    t_float x_inlet2;
    t_vseg *x_list;
} t_vline;

static t_int *vline_tilde_perform(t_int *w)
{
    t_vline *x = (t_vline *)(w[1]);
    t_float *out = (t_float *)(w[2]);
    int n = (int)(w[3]), i;
    double f = x->x_value;
    double inc = x->x_inc;
    double msecpersamp = x->x_msecpersamp;
    double samppermsec = x->x_samppermsec;
    double timenow, logicaltimenow = clock_gettimesince(x->x_referencetime);
    t_vseg *s = x->x_list;
    if (logicaltimenow != x->x_lastlogicaltime)
    {
        int sampstotime = (n > DEFDACBLKSIZE ? n : DEFDACBLKSIZE);
        x->x_lastlogicaltime = logicaltimenow;
        x->x_nextblocktime = logicaltimenow - sampstotime * msecpersamp;
    }
    timenow = x->x_nextblocktime;
    x->x_nextblocktime = timenow + n * msecpersamp;
    for (i = 0; i < n; i++)
    {
        double timenext = timenow + msecpersamp;
    checknext:
        if (s)
        {
            /* has starttime elapsed?  If so update value and increment */
            if (s->s_starttime < timenext)
            {
                if (x->x_targettime <= timenext)
                    f = x->x_target, inc = 0;
                    /* if zero-length segment bash output value */
                if (s->s_targettime <= s->s_starttime)
                {
                    f = s->s_target;
                    inc = 0;
                }
                else
                {
                    double incpermsec = (s->s_target - f)/
                        (s->s_targettime - s->s_starttime);
                    f = f + incpermsec * (timenext - s->s_starttime);
                    inc = incpermsec * msecpersamp;
                }
                x->x_inc = inc;
                x->x_target = s->s_target;
                x->x_targettime = s->s_targettime;
                x->x_list = s->s_next;
                t_freebytes(s, sizeof(*s));
                s = x->x_list;
                goto checknext;
            }
        }
        if (x->x_targettime <= timenext)
            f = x->x_target, inc = x->x_inc = 0, x->x_targettime = 1e20;
        *out++ = f;
        f = f + inc;
        timenow = timenext;
    }
    x->x_value = f;
    return (w+4);
}

static void vline_tilde_stop(t_vline *x)
{
    t_vseg *s1, *s2;
    for (s1 = x->x_list; s1; s1 = s2)
        s2 = s1->s_next, t_freebytes(s1, sizeof(*s1));
    x->x_list = 0;
    x->x_inc = 0;
    x->x_inlet1 = x->x_inlet2 = 0;
    x->x_target = x->x_value;
    x->x_targettime = 1e20;
}

static void vline_tilde_float(t_vline *x, t_float f)
{
    double timenow = clock_gettimesince(x->x_referencetime);
    t_float inlet1 = (x->x_inlet1 < 0 ? 0 : x->x_inlet1);
    t_float inlet2 = x->x_inlet2;
    double starttime = timenow + inlet2;
    t_vseg *s1, *s2, *deletefrom = 0, *snew;
    if (PD_BIGORSMALL(f))
        f = 0;

        /* negative delay input means stop and jump immediately to new value */
    if (inlet2 < 0)
    {
        x->x_value = f;
        vline_tilde_stop(x);
        return;
    }
    snew = (t_vseg *)t_getbytes(sizeof(*snew));
        /* check if we supplant the first item in the list.  We supplant
        an item by having an earlier starttime, or an equal starttime unless
        the equal one was instantaneous and the new one isn't (in which case
        we'll do a jump-and-slide starting at that time.) */
    if (!x->x_list || x->x_list->s_starttime > starttime ||
        (x->x_list->s_starttime == starttime &&
            (x->x_list->s_targettime > x->x_list->s_starttime || inlet1 <= 0)))
    {
        deletefrom = x->x_list;
        x->x_list = snew;
    }
    else
    {
        for (s1 = x->x_list; s2 = s1->s_next; s1 = s2)
        {
            if (s2->s_starttime > starttime ||
                (s2->s_starttime == starttime &&
                    (s2->s_targettime > s2->s_starttime || inlet1 <= 0)))
            {
                deletefrom = s2;
                s1->s_next = snew;
                goto didit;
            }
        }
        s1->s_next = snew;
        deletefrom = 0;
    didit: ;
    }
    while (deletefrom)
    {
        s1 = deletefrom->s_next;
        t_freebytes(deletefrom, sizeof(*deletefrom));
        deletefrom = s1;
    }
    snew->s_next = 0;
    snew->s_target = f;
    snew->s_starttime = starttime;
    snew->s_targettime = starttime + inlet1;
    x->x_inlet1 = x->x_inlet2 = 0;
}

static void vline_tilde_dsp(t_vline *x, t_signal **sp)
{
    dsp_add(vline_tilde_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
    x->x_samppermsec = ((double)(sp[0]->s_sr)) / 1000;
    x->x_msecpersamp = ((double)1000) / sp[0]->s_sr;
}

static void *vline_tilde_new(void)
{
    t_vline *x = (t_vline *)pd_new(vline_tilde_class);
    outlet_new(&x->x_obj, gensym("signal"));
    floatinlet_new(&x->x_obj, &x->x_inlet1);
    floatinlet_new(&x->x_obj, &x->x_inlet2);
    x->x_inlet1 = x->x_inlet2 = 0;
    x->x_value = x->x_inc = 0;
    x->x_referencetime = x->x_lastlogicaltime = x->x_nextblocktime =
        clock_getlogicaltime();
    x->x_list = 0;
    x->x_samppermsec = 0;
    x->x_targettime = 1e20;
    return (x);
}

static void vline_tilde_setup(void)
{
    vline_tilde_class = class_new(gensym("vline~"), vline_tilde_new, 
        (t_method)vline_tilde_stop, sizeof(t_vline), 0, 0);
    class_addfloat(vline_tilde_class, (t_method)vline_tilde_float);
    class_addmethod(vline_tilde_class, (t_method)vline_tilde_dsp,
        gensym("dsp"), 0);
    class_addmethod(vline_tilde_class, (t_method)vline_tilde_stop,
        gensym("stop"), 0);
}

/* -------------------------- snapshot~ ------------------------------ */
static t_class *snapshot_tilde_class;

typedef struct _snapshot
{
    t_object x_obj;
    t_sample x_value;
    t_float x_f;
} t_snapshot;

static void *snapshot_tilde_new(void)
{
    t_snapshot *x = (t_snapshot *)pd_new(snapshot_tilde_class);
    x->x_value = 0;
    outlet_new(&x->x_obj, &s_float);
    x->x_f = 0;
    return (x);
}

static t_int *snapshot_tilde_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    *out = *in;
    return (w+3);
}

static void snapshot_tilde_dsp(t_snapshot *x, t_signal **sp)
{
    dsp_add(snapshot_tilde_perform, 2, sp[0]->s_vec + (sp[0]->s_n-1),
        &x->x_value);
}

static void snapshot_tilde_bang(t_snapshot *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_value);
}

static void snapshot_tilde_set(t_snapshot *x, t_floatarg f)
{
    x->x_value = f;
}

static void snapshot_tilde_setup(void)
{
    snapshot_tilde_class = class_new(gensym("snapshot~"), snapshot_tilde_new, 0,
        sizeof(t_snapshot), 0, 0);
    CLASS_MAINSIGNALIN(snapshot_tilde_class, t_snapshot, x_f);
    class_addmethod(snapshot_tilde_class, (t_method)snapshot_tilde_dsp,
        gensym("dsp"), 0);
    class_addmethod(snapshot_tilde_class, (t_method)snapshot_tilde_set,
        gensym("set"), A_DEFFLOAT, 0);
    class_addbang(snapshot_tilde_class, snapshot_tilde_bang);
}

/* -------------------------- vsnapshot~ ------------------------------ */
static t_class *vsnapshot_tilde_class;

typedef struct _vsnapshot
{
    t_object x_obj;
    int x_n;
    int x_gotone;
    t_sample *x_vec;
    t_float x_f;
    t_float x_sampspermsec;
    double x_time;
} t_vsnapshot;

static void *vsnapshot_tilde_new(void)
{
    t_vsnapshot *x = (t_vsnapshot *)pd_new(vsnapshot_tilde_class);
    outlet_new(&x->x_obj, &s_float);
    x->x_f = 0;
    x->x_n = 0;
    x->x_vec = 0;
    x->x_gotone = 0;
    return (x);
}

static t_int *vsnapshot_tilde_perform(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_vsnapshot *x = (t_vsnapshot *)(w[2]);
    t_sample *out = x->x_vec;
    int n = x->x_n, i;
    for (i = 0; i < n; i++)
        out[i] = in[i];
    x->x_time = clock_getlogicaltime();
    x->x_gotone = 1;
    return (w+3);
}

static void vsnapshot_tilde_dsp(t_vsnapshot *x, t_signal **sp)
{
    int n = sp[0]->s_n;
    if (n != x->x_n)
    {
        if (x->x_vec)
            t_freebytes(x->x_vec, x->x_n * sizeof(t_sample));
        x->x_vec = (t_sample *)getbytes(n * sizeof(t_sample));
        x->x_gotone = 0;
        x->x_n = n;
    }
    x->x_sampspermsec = sp[0]->s_sr / 1000;
    dsp_add(vsnapshot_tilde_perform, 2, sp[0]->s_vec, x);
}

static void vsnapshot_tilde_bang(t_vsnapshot *x)
{
    t_sample val;
    if (x->x_gotone)
    {
        int indx = clock_gettimesince(x->x_time) * x->x_sampspermsec;
        if (indx < 0)
            indx = 0;
        else if (indx >= x->x_n)
            indx = x->x_n - 1;
        val = x->x_vec[indx];
    }
    else val = 0;
    outlet_float(x->x_obj.ob_outlet, val);
}

static void vsnapshot_tilde_ff(t_vsnapshot *x)
{
    if (x->x_vec)
        t_freebytes(x->x_vec, x->x_n * sizeof(t_sample));
}

static void vsnapshot_tilde_setup(void)
{
    vsnapshot_tilde_class = class_new(gensym("vsnapshot~"),
        vsnapshot_tilde_new, (t_method)vsnapshot_tilde_ff,
        sizeof(t_vsnapshot), 0, 0);
    CLASS_MAINSIGNALIN(vsnapshot_tilde_class, t_vsnapshot, x_f);
    class_addmethod(vsnapshot_tilde_class, (t_method)vsnapshot_tilde_dsp, gensym("dsp"), 0);
    class_addbang(vsnapshot_tilde_class, vsnapshot_tilde_bang);
}


/* ---------------- env~ - simple envelope follower. ----------------- */

#define MAXOVERLAP 32
#define INITVSTAKEN 64

typedef struct sigenv
{
    t_object x_obj;                 /* header */
    void *x_outlet;                 /* a "float" outlet */
    void *x_clock;                  /* a "clock" object */
    t_sample *x_buf;                   /* a Hanning window */
    int x_phase;                    /* number of points since last output */
    int x_period;                   /* requested period of output */
    int x_realperiod;               /* period rounded up to vecsize multiple */
    int x_npoints;                  /* analysis window size in samples */
    t_float x_result;                 /* result to output */
    t_sample x_sumbuf[MAXOVERLAP];     /* summing buffer */
    t_float x_f;
    int x_allocforvs;               /* extra buffer for DSP vector size */
} t_sigenv;

t_class *env_tilde_class;
static void env_tilde_tick(t_sigenv *x);

static void *env_tilde_new(t_floatarg fnpoints, t_floatarg fperiod)
{
    int npoints = fnpoints;
    int period = fperiod;
    t_sigenv *x;
    t_sample *buf;
    int i;

    if (npoints < 1) npoints = 1024;
    if (period < 1) period = npoints/2;
    if (period < npoints / MAXOVERLAP + 1)
        period = npoints / MAXOVERLAP + 1;
    if (!(buf = getbytes(sizeof(t_sample) * (npoints + INITVSTAKEN))))
    {
        error("env: couldn't allocate buffer");
        return (0);
    }
    x = (t_sigenv *)pd_new(env_tilde_class);
    x->x_buf = buf;
    x->x_npoints = npoints;
    x->x_phase = 0;
    x->x_period = period;
    for (i = 0; i < MAXOVERLAP; i++) x->x_sumbuf[i] = 0;
    for (i = 0; i < npoints; i++)
        buf[i] = (1. - cos((2 * 3.14159 * i) / npoints))/npoints;
    for (; i < npoints+INITVSTAKEN; i++) buf[i] = 0;
    x->x_clock = clock_new(x, (t_method)env_tilde_tick);
    x->x_outlet = outlet_new(&x->x_obj, gensym("float"));
    x->x_f = 0;
    x->x_allocforvs = INITVSTAKEN;
    return (x);
}

static t_int *env_tilde_perform(t_int *w)
{
    t_sigenv *x = (t_sigenv *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    int count;
    t_sample *sump; 
    in += n;
    for (count = x->x_phase, sump = x->x_sumbuf;
        count < x->x_npoints; count += x->x_realperiod, sump++)
    {
        t_sample *hp = x->x_buf + count;
        t_sample *fp = in;
        t_sample sum = *sump;
        int i;
        
        for (i = 0; i < n; i++)
        {
            fp--;
            sum += *hp++ * (*fp * *fp);
        }
        *sump = sum;
    }
    sump[0] = 0;
    x->x_phase -= n;
    if (x->x_phase < 0)
    {
        x->x_result = x->x_sumbuf[0];
        for (count = x->x_realperiod, sump = x->x_sumbuf;
            count < x->x_npoints; count += x->x_realperiod, sump++)
                sump[0] = sump[1];
        sump[0] = 0;
        x->x_phase = x->x_realperiod - n;
        clock_delay(x->x_clock, 0L);
    }
    return (w+4);
}

static void env_tilde_dsp(t_sigenv *x, t_signal **sp)
{
    if (x->x_period % sp[0]->s_n) x->x_realperiod =
        x->x_period + sp[0]->s_n - (x->x_period % sp[0]->s_n);
    else x->x_realperiod = x->x_period;
    if (sp[0]->s_n > x->x_allocforvs)
    {
        void *xx = resizebytes(x->x_buf,
            (x->x_npoints + x->x_allocforvs) * sizeof(t_sample),
            (x->x_npoints + sp[0]->s_n) * sizeof(t_sample));
        if (!xx)
        {
            error("env~: out of memory");
            return;
        }
        x->x_buf = (t_sample *)xx;
        x->x_allocforvs = sp[0]->s_n;
    }
    dsp_add(env_tilde_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
}

static void env_tilde_tick(t_sigenv *x) /* callback function for the clock */
{
    outlet_float(x->x_outlet, powtodb(x->x_result));
}

static void env_tilde_ff(t_sigenv *x)           /* cleanup on free */
{
    clock_free(x->x_clock);
    freebytes(x->x_buf, (x->x_npoints + x->x_allocforvs) * sizeof(*x->x_buf));
}


void env_tilde_setup(void )
{
    env_tilde_class = class_new(gensym("env~"), (t_newmethod)env_tilde_new,
        (t_method)env_tilde_ff, sizeof(t_sigenv), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(env_tilde_class, t_sigenv, x_f);
    class_addmethod(env_tilde_class, (t_method)env_tilde_dsp, gensym("dsp"), 0);
}

/* --------------------- threshold~ ----------------------------- */

static t_class *threshold_tilde_class;

typedef struct _threshold_tilde
{
    t_object x_obj;
    t_outlet *x_outlet1;        /* bang out for high thresh */
    t_outlet *x_outlet2;        /* bang out for low thresh */
    t_clock *x_clock;           /* wakeup for message output */
    t_float x_f;                  /* scalar inlet */
    int x_state;                /* 1 = high, 0 = low */
    t_float x_hithresh;           /* value of high threshold */
    t_float x_lothresh;           /* value of low threshold */
    t_float x_deadwait;           /* msec remaining in dead period */
    t_float x_msecpertick;        /* msec per DSP tick */
    t_float x_hideadtime;         /* hi dead time in msec */
    t_float x_lodeadtime;         /* lo dead time in msec */
} t_threshold_tilde;

static void threshold_tilde_tick(t_threshold_tilde *x);
static void threshold_tilde_set(t_threshold_tilde *x,
    t_floatarg hithresh, t_floatarg hideadtime,
    t_floatarg lothresh, t_floatarg lodeadtime);

static t_threshold_tilde *threshold_tilde_new(t_floatarg hithresh,
    t_floatarg hideadtime, t_floatarg lothresh, t_floatarg lodeadtime)
{
    t_threshold_tilde *x = (t_threshold_tilde *)
        pd_new(threshold_tilde_class);
    x->x_state = 0;             /* low state */
    x->x_deadwait = 0;          /* no dead time */
    x->x_clock = clock_new(x, (t_method)threshold_tilde_tick);
    x->x_outlet1 = outlet_new(&x->x_obj, &s_bang);
    x->x_outlet2 = outlet_new(&x->x_obj, &s_bang);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("ft1"));
    x->x_msecpertick = 0.;
    x->x_f = 0;
    threshold_tilde_set(x, hithresh, hideadtime, lothresh, lodeadtime);
    return (x);
}

    /* "set" message to specify thresholds and dead times */
static void threshold_tilde_set(t_threshold_tilde *x,
    t_floatarg hithresh, t_floatarg hideadtime,
    t_floatarg lothresh, t_floatarg lodeadtime)
{
    if (lothresh > hithresh)
        lothresh = hithresh;
    x->x_hithresh = hithresh;
    x->x_hideadtime = hideadtime;
    x->x_lothresh = lothresh;
    x->x_lodeadtime = lodeadtime;
}

    /* number in inlet sets state -- note incompatible with JMAX which used
    "int" message for this, impossible here because of auto signal conversion */
static void threshold_tilde_ft1(t_threshold_tilde *x, t_floatarg f)
{
    x->x_state = (f != 0);
    x->x_deadwait = 0;
}

static void threshold_tilde_tick(t_threshold_tilde *x)  
{
    if (x->x_state)
        outlet_bang(x->x_outlet1);
    else outlet_bang(x->x_outlet2);
}

static t_int *threshold_tilde_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_threshold_tilde *x = (t_threshold_tilde *)(w[2]);
    int n = (t_int)(w[3]);
    if (x->x_deadwait > 0)
        x->x_deadwait -= x->x_msecpertick;
    else if (x->x_state)
    {
            /* we're high; look for low sample */
        for (; n--; in1++)
        {
            if (*in1 < x->x_lothresh)
            {
                clock_delay(x->x_clock, 0L);
                x->x_state = 0;
                x->x_deadwait = x->x_lodeadtime;
                goto done;
            }
        }
    }
    else
    {
            /* we're low; look for high sample */
        for (; n--; in1++)
        {
            if (*in1 >= x->x_hithresh)
            {
                clock_delay(x->x_clock, 0L);
                x->x_state = 1;
                x->x_deadwait = x->x_hideadtime;
                goto done;
            }
        }
    }
done:
    return (w+4);
}

void threshold_tilde_dsp(t_threshold_tilde *x, t_signal **sp)
{
    x->x_msecpertick = 1000. * sp[0]->s_n / sp[0]->s_sr;
    dsp_add(threshold_tilde_perform, 3, sp[0]->s_vec, x, sp[0]->s_n);
}

static void threshold_tilde_ff(t_threshold_tilde *x)
{
    clock_free(x->x_clock);
}

static void threshold_tilde_setup( void)
{
    threshold_tilde_class = class_new(gensym("threshold~"),
        (t_newmethod)threshold_tilde_new, (t_method)threshold_tilde_ff,
        sizeof(t_threshold_tilde), 0,
            A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(threshold_tilde_class, t_threshold_tilde, x_f);
    class_addmethod(threshold_tilde_class, (t_method)threshold_tilde_set,
        gensym("set"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(threshold_tilde_class, (t_method)threshold_tilde_ft1,
        gensym("ft1"), A_FLOAT, 0);
    class_addmethod(threshold_tilde_class, (t_method)threshold_tilde_dsp,
        gensym("dsp"), 0);
}

/* ------------------------ global setup routine ------------------------- */

void d_ctl_setup(void)
{
    sig_tilde_setup();
    line_tilde_setup();
    vline_tilde_setup();
    snapshot_tilde_setup();
    vsnapshot_tilde_setup();
    env_tilde_setup();
    threshold_tilde_setup();
}

