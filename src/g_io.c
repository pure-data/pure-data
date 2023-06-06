/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* graphical inlets and outlets, both for control and signals.  */

/* This code is highly inefficient; messages actually have to be forwarded
by inlets and outlets.  The outlet is in even worse shape than the inlet;
in order to avoid having a "signal" method in the class, the oulet actually
sprouts an inlet, which forwards the message to the "outlet" object, which
sends it on to the outlet proper.  Another way to do it would be to have
separate classes for "signal" and "control" outlets, but this would complicate
life elsewhere. */


#include "m_pd.h"
#include "g_canvas.h"
#include <string.h>

static int symbol2resamplemethod(t_symbol*s)
{
    if      (s == gensym("hold"  )) return 1; /* up: sample and hold */
    else if (s == gensym("lin"   )) return 2; /* up: linear interpolation */
    else if (s == gensym("linear")) return 2; /* up: linear interpolation */
    else if (s == gensym("pad"   )) return 0; /* up: zero pad */
    return -1;  /* default: sample/hold except zero-pad if version<0.44 */
}

/* ------------------------- vinlet -------------------------- */
t_class *vinlet_class;

typedef struct _reblocker
{
    t_sample *r_buf;         /* signal buffer; zero if not a signal */
    t_resample r_updown;
} t_reblocker;

static void reblocker_init(t_reblocker *rb, int buflength)
{
    rb->r_buf = (t_sample *)getbytes(buflength * sizeof(t_sample));
    resample_init(&rb->r_updown);
}

static void reblocker_resize(t_reblocker **rb, int oldn, int newn,
    int buflength)
{
    int i;
    if (oldn == newn)
        return;
    for (i = newn; i < oldn; i++)
    {
        freebytes((*rb)[i].r_buf, buflength * sizeof(t_sample));
        resample_free(&(*rb)[i].r_updown);
    }
    *rb = (t_reblocker *)resizebytes(*rb, oldn * sizeof(t_reblocker),
        newn * sizeof(t_reblocker));
    for (i = oldn; i < newn; i++)
        reblocker_init(&(*rb)[i], buflength);
}

typedef struct _vinlet
{
    t_object x_obj;
    t_canvas *x_canvas;
    t_inlet *x_inlet;
    int x_buflength;        /* number of samples per channel in buffer */
    int x_fill;
    int x_read;
    int x_hop;
    int x_updownmethod;
            /* if not reblocking, the next slot communicates the parent's
                inlet signal from the prolog to the DSP routine: */
    t_signal *x_directsignal;
    int x_nchans;       /* this is also set in prolog & used in dsp */
    t_outlet *x_fwdout; /* optional outlet for forwarding messages to inlet~ */
    t_reblocker *x_rb;  /* reblocking and resampling, one per channel */
} t_vinlet;

static void *vinlet_new(t_symbol *s)
{
    t_vinlet *x = (t_vinlet *)pd_new(vinlet_class);
    x->x_canvas = canvas_getcurrent();
    x->x_inlet = canvas_addinlet(x->x_canvas, &x->x_obj.ob_pd, 0);
    x->x_buflength = 0;
    x->x_nchans = 0;
    x->x_rb = 0;
    outlet_new(&x->x_obj, 0);
    return (x);
}

static void vinlet_bang(t_vinlet *x)
{
    outlet_bang(x->x_obj.ob_outlet);
}

static void vinlet_pointer(t_vinlet *x, t_gpointer *gp)
{
    outlet_pointer(x->x_obj.ob_outlet, gp);
}

static void vinlet_float(t_vinlet *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, f);
}

static void vinlet_symbol(t_vinlet *x, t_symbol *s)
{
    outlet_symbol(x->x_obj.ob_outlet, s);
}

static void vinlet_list(t_vinlet *x, t_symbol *s, int argc, t_atom *argv)
{
    outlet_list(x->x_obj.ob_outlet, s, argc, argv);
}

static void vinlet_anything(t_vinlet *x, t_symbol *s, int argc, t_atom *argv)
{
    outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
}

static void vinlet_free(t_vinlet *x)
{
    int i;
    canvas_rminlet(x->x_canvas, x->x_inlet);
    if (x->x_rb)
        for (i = 0; i < x->x_nchans; i++)
    {
        freebytes((x->x_rb)[i].r_buf, x->x_buflength * sizeof(t_sample));
        resample_free(&(x->x_rb)[i].r_updown);
    }
}

t_inlet *vinlet_getit(t_pd *x)
{
    if (pd_class(x) != vinlet_class) bug("vinlet_getit");
    return (((t_vinlet *)x)->x_inlet);
}

/* ------------------------- signal inlet -------------------------- */
int vinlet_issignal(t_vinlet *x)
{
    return (x->x_rb != 0);
}

t_int *vinlet_perform(t_int *w)
{
    t_vinlet *x = (t_vinlet *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    t_reblocker *rb = (t_reblocker *)(w[3]);
    int hop = (int)(w[4]), n = (int)(w[5]), read = x->x_read;
    t_sample *in = rb->r_buf + read;
    while (n--)
        *out++ = *in++;
    if (hop)
    {
        if ((read += hop) == x->x_buflength)
            read = 0;
        x->x_read = read;
    }
    return (w+6);
}

static void vinlet_fwd(t_vinlet *x, t_symbol *s, int argc, t_atom *argv)
{

    if (!x->x_rb)   /* if we're not signal, just forward */
        outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
    else if (x->x_fwdout && argc > 0 && argv->a_type == A_SYMBOL)
        outlet_anything(x->x_fwdout, argv->a_w.w_symbol, argc-1, argv+1);
}

static void vinlet_dsp(t_vinlet *x, t_signal **sp)
{
        /* no buffer means we're not a signal inlet */
    if (!x->x_rb)
        return;
    if (x->x_directsignal)
    {
        sp[0] = signal_new(0, 1, sp[0]->s_sr, 0);
        signal_setborrowed(sp[0], x->x_directsignal);
    }
    else
    {
        int i;
        signal_setmultiout(sp, x->x_nchans);
        for (i = 0; i < x->x_nchans; i++)
            dsp_add(vinlet_perform, 5, x, sp[0]->s_vec + i * sp[0]->s_length,
                &x->x_rb[i], (t_int)(i == x->x_nchans-1 ? sp[0]->s_length : 0),
                    (t_int)(sp[0]->s_length));
        x->x_read = 0;
    }
}

    /* prolog code: loads buffer from parent patch */
t_int *vinlet_doprolog(t_int *w)
{
    t_vinlet *x = (t_vinlet *)(w[1]);
    t_sample *in = (t_sample *)(w[2]), *out;
    t_sample *buf = (t_sample *)(w[3]);
    int lastone = (int)(w[4]), n = (int)(w[5]), fill = x->x_fill;

    if (fill == x->x_buflength)
    {
        t_sample *f1 = buf, *f2 = buf + x->x_hop;
        int nshift = x->x_buflength - x->x_hop;
        while (nshift--) *f1++ = *f2++;
        fill -= x->x_hop;
    }
    out = buf + fill;
    if (lastone)
        x->x_fill = fill + n;
    while (n--)
        *out++ = *in++;
    return (w+6);
}

int inlet_getsignalindex(t_inlet *x);

        /* set up prolog DSP code  */
void vinlet_dspprolog(struct _vinlet *x, t_signal **parentsigs,
    int myvecsize, int phase, int period, int frequency,
    int downsample, int upsample,  int reblock, int switched)
{
    t_signal *insig;
        /* no buffer means we're not a signal inlet */
    if (!x->x_rb)
        return;

        /* if the "reblock" flag is set, arrange to copy data in from the
        parent. */
    if (reblock)
    {
        int parentvecsize, bufsize, oldbufsize, prologphase, i;
        int re_parentvecsize; /* resampled parentvectorsize */

            /* the prolog code counts from 0 to period-1; the
            phase is backed up by one so that AFTER the prolog code
            runs, the "x_fill" phase is in sync with the "x_read" phase. */
        prologphase = (phase + period - 1) % period;
        if (parentsigs)
        {
            insig = parentsigs[inlet_getsignalindex(x->x_inlet)];
            parentvecsize = insig->s_length;
            re_parentvecsize = parentvecsize * upsample / downsample;
            reblocker_resize(&x->x_rb, x->x_nchans, insig->s_nchans,
                x->x_buflength);
            x->x_nchans = insig->s_nchans;
        }
        else
        {
            insig = 0;
            parentvecsize = re_parentvecsize = 1;
        }

        bufsize = re_parentvecsize;
        if (bufsize < myvecsize)
            bufsize = myvecsize;
        if (bufsize != (oldbufsize = x->x_buflength))
        {
            for (i = 0; i < x->x_nchans; i++)
            {
                x->x_rb[i].r_buf = (t_sample *)t_resizebytes(x->x_rb[i].r_buf,
                    oldbufsize * sizeof(t_sample), bufsize * sizeof(t_sample));
                memset((char *)x->x_rb[i].r_buf, 0, bufsize * sizeof(t_sample));
            }
            x->x_buflength = bufsize;
        }
        if (parentsigs)
        {
            x->x_hop = period * re_parentvecsize;

            x->x_fill = prologphase ?
                x->x_buflength - (x->x_hop - prologphase * re_parentvecsize) :
                    x->x_buflength;
            for (i = 0; i < x->x_nchans; i++)
            {
                x->x_rb[i].r_updown.downsample = downsample;
                x->x_rb[i].r_updown.upsample   = upsample;
                if (upsample == 1 && downsample == 1)
                    dsp_add(vinlet_doprolog, 5, x,
                        insig->s_vec + i * parentvecsize, x->x_rb[i].r_buf,
                            (t_int)(i == x->x_nchans-1),
                                (t_int)re_parentvecsize);
                else
                {
                    int method = (x->x_updownmethod == -1?
                        (pd_compatibilitylevel < 44 ? 0 : 1) :
                            x->x_updownmethod);
                    resamplefrom_dsp(&x->x_rb[i].r_updown,
                        insig->s_vec + i * parentvecsize, parentvecsize,
                            re_parentvecsize, method);
                    dsp_add(vinlet_doprolog, 5, x, x->x_rb[i].r_updown.s_vec,
                        x->x_rb[i].r_buf, (t_int)(i == x->x_nchans-1),
                                (t_int)re_parentvecsize);
                }
            }
        }
        else for (i = 0; i < x->x_nchans; i++)
            memset((char *)(x->x_rb[i].r_buf), 0, bufsize * sizeof(t_sample));
        x->x_directsignal = 0;
    }
    else
    {
            /* no reblocking; in this case our output signal is "borrowed"
            and merely needs to be pointed to the real one. */
        x->x_directsignal = parentsigs[inlet_getsignalindex(x->x_inlet)];
    }
}

static void *vinlet_newsig(t_symbol *s, int argc, t_atom *argv)
{
    t_vinlet *x = (t_vinlet *)pd_new(vinlet_class);
    x->x_canvas = canvas_getcurrent();
    x->x_inlet = canvas_addinlet(x->x_canvas, &x->x_obj.ob_pd, &s_signal);
    x->x_nchans = 1;
    x->x_buflength = 0;
    x->x_rb = (t_reblocker *)getbytes(sizeof(*x->x_rb));
    reblocker_init(x->x_rb, x->x_buflength);
    x->x_directsignal = 0;
    x->x_fwdout = 0;
    outlet_new(&x->x_obj, &s_signal);
    inlet_new(&x->x_obj, (t_pd *)x->x_inlet, 0, 0);

    /* this should be thought over:
     * it might prove hard to provide consistency between labeled up- &
     * downsampling methods - maybe indices would be better...
     *
     * up till now we provide several upsampling methods and 1 single
     *  downsampling method (no filtering !)
     */
    x->x_updownmethod = -1;
    while (argc-->0)
    {
        int method;
        s = atom_getsymbol(argv++);
        method = symbol2resamplemethod(s);
        if (method >= 0)
            x->x_updownmethod = method;
    }
    x->x_fwdout = outlet_new(&x->x_obj, 0);
    return (x);
}

static void vinlet_setup(void)
{
    vinlet_class = class_new(gensym("inlet"), (t_newmethod)vinlet_new,
        (t_method)vinlet_free, sizeof(t_vinlet),
            CLASS_NOINLET | CLASS_MULTICHANNEL, A_DEFSYM, 0);
    class_addcreator((t_newmethod)vinlet_newsig, gensym("inlet~"), A_GIMME, 0);
    class_addbang(vinlet_class, vinlet_bang);
    class_addpointer(vinlet_class, vinlet_pointer);
    class_addfloat(vinlet_class, vinlet_float);
    class_addsymbol(vinlet_class, vinlet_symbol);
    class_addlist(vinlet_class, vinlet_list);
    class_addanything(vinlet_class, vinlet_anything);
    class_addmethod(vinlet_class,(t_method)vinlet_fwd,  gensym("fwd"),
        A_GIMME, 0);
    class_addmethod(vinlet_class, (t_method)vinlet_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(vinlet_class, gensym("inlet-outlet"));
}

/* ------------------------- voutlet -------------------------- */

t_class *voutlet_class;

typedef struct _voutlet
{
    t_object x_obj;
    t_canvas *x_canvas;
    t_outlet *x_parentoutlet;
    int x_buflength;
    int x_empty;            /* next to read out of buffer in epilog code */
    int x_write;            /* next to write in to buffer */
    int x_hop;              /* hopsize */
    int x_updownmethod;
        /*  parent's outlet signal, valid between the prolog and the dsp setup
        routines.  */
    t_signal **x_parentsignal;
    int x_nchans;       /* this is also set in prolog & used in dsp */
    t_reblocker *x_rb;  /* reblocking and resampling, one per channel */
    unsigned int x_justcopyout:1;   /* switched but not blocked */
    unsigned int x_borrowed:1;      /* output is borrowed from our inlet */
} t_voutlet;

static void *voutlet_new(t_symbol *s)
{
    t_voutlet *x = (t_voutlet *)pd_new(voutlet_class);
    x->x_canvas = canvas_getcurrent();
    x->x_parentoutlet = canvas_addoutlet(x->x_canvas, &x->x_obj.ob_pd, 0);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, 0, 0);
    x->x_buflength = 0;
    x->x_rb = 0;
    x->x_nchans = 0;
    return (x);
}

static void voutlet_bang(t_voutlet *x)
{
    outlet_bang(x->x_parentoutlet);
}

static void voutlet_pointer(t_voutlet *x, t_gpointer *gp)
{
    outlet_pointer(x->x_parentoutlet, gp);
}

static void voutlet_float(t_voutlet *x, t_float f)
{
    outlet_float(x->x_parentoutlet, f);
}

static void voutlet_symbol(t_voutlet *x, t_symbol *s)
{
    outlet_symbol(x->x_parentoutlet, s);
}

static void voutlet_list(t_voutlet *x, t_symbol *s, int argc, t_atom *argv)
{
    outlet_list(x->x_parentoutlet, s, argc, argv);
}

static void voutlet_anything(t_voutlet *x, t_symbol *s, int argc, t_atom *argv)
{
    outlet_anything(x->x_parentoutlet, s, argc, argv);
}

static void voutlet_free(t_voutlet *x)
{
    int i;
    canvas_rmoutlet(x->x_canvas, x->x_parentoutlet);
    if (x->x_rb)
        for (i = 0; i < x->x_nchans; i++)
    {
        freebytes((x->x_rb)[i].r_buf, x->x_buflength * sizeof(t_sample));
        resample_free(&(x->x_rb)[i].r_updown);
    }
}

t_outlet *voutlet_getit(t_pd *x)
{
    if (pd_class(x) != voutlet_class) bug("voutlet_getit");
    return (((t_voutlet *)x)->x_parentoutlet);
}

/* ------------------------- signal outlet -------------------------- */

int voutlet_issignal(t_voutlet *x)
{
    return (x->x_rb != 0);
}

    /* LATER optimize for non-overlapped case where the "+=" isn't needed */
t_int *voutlet_perform(t_int *w)
{
    t_voutlet *x = (t_voutlet *)(w[1]);
    t_sample *in = (t_sample *)(w[2]), *buf= (t_sample *)(w[3]);
    int xwrite = x->x_write;
    int hop = (int)(w[4]);
    int n = (int)(w[5]);
    t_sample *out = buf + xwrite,
        *endbuf = buf + x->x_buflength;
    while (n--)
    {
        *out++ += *in++;
        if (out == endbuf)
            out = buf;
    }
    xwrite += x->x_hop;
    if (xwrite >= x->x_buflength)
        xwrite = 0;
    x->x_write = xwrite;
    return (w+6);
}

    /* epilog code for blocking: write buffer to parent patch */
static t_int *voutlet_doepilog(t_int *w)
{
    t_voutlet *x = (t_voutlet *)(w[1]);
    t_sample *in, *out = (t_sample *)(w[2]), *buf = (t_sample *)(w[3]);
    int hop = (int)(w[4]), n = (int)(w[5]), empty = x->x_empty;
    if (empty == x->x_buflength)
        empty = 0;
    x->x_empty = empty + hop;
    in = buf + empty;
    for (; n--; in++)
        *out++ = *in, *in = 0;
    return (w+6);
}

    /* It's dumb that we have to offer a duplicate but we don't have the
    goddam resampling buffer allocated by the time this goes on the DSP
    chain so we have to look it up dynamically. */
static t_int *voutlet_doepilog_resample(t_int *w)
{
    t_voutlet *x = (t_voutlet *)(w[1]);
    t_sample *in, *out = ((t_resample *)(w[2]))->s_vec,
        *buf = (t_sample *)(w[3]);
    int hop = (int)(w[4]), n = (int)(w[5]), empty = x->x_empty;
    if (empty == x->x_buflength)
        empty = 0;
    x->x_empty = empty + hop;
    in = buf + empty;
    for (; n--; in++)
        *out++ = *in, *in = 0;
    return (w+6);
}

int outlet_getsignalindex(t_outlet *x);

        /* prolog for outlets -- store pointer to the outlet on the
        parent, which, if "reblock" is false, will want to refer
        back to whatever we see on our input during the "dsp" method
        called later.  */
void voutlet_dspprolog(struct _voutlet *x, t_signal **parentsigs,
    int myvecsize, int phase, int period, int frequency,
    int downsample, int upsample, int reblock, int switched)
{
        /* no buffer means we're not a signal outlet */
    if (!x->x_rb)
        return;
    x->x_justcopyout = (switched && !reblock);
    x->x_parentsignal = (parentsigs?
        &parentsigs[outlet_getsignalindex(x->x_parentoutlet)] : 0);
    if (!parentsigs)
        return;
    if (switched || reblock)
        x->x_borrowed = 0;
    else    /* OK, borrow it */
    {
        x->x_borrowed = 1;
        if (!parentsigs)
            bug("voutlet_dspprolog");
                /* create new borrowed signal to be set in dsp routine below */
        *(x->x_parentsignal) = signal_new(0, 1,
            (*(x->x_parentsignal))->s_sr, 0);
    }
    if (reblock)
    {
        int parentvecsize, buflength, i;
        int re_parentvecsize;
        int bigperiod, epilogphase, blockphase;

        parentvecsize = (*x->x_parentsignal)->s_length;
        re_parentvecsize = parentvecsize * upsample / downsample;
        buflength = re_parentvecsize;
        if (buflength < myvecsize)
            buflength = myvecsize;
        if (buflength != x->x_buflength)
        {
            for (i = 0; i < x->x_nchans; i++)
            {
                x->x_rb[i].r_buf = (t_sample *)t_resizebytes(x->x_rb[i].r_buf,
                    x->x_buflength * sizeof(t_sample),
                        buflength * sizeof(t_sample));
                memset((char *)x->x_rb[i].r_buf, 0,
                    buflength * sizeof(t_sample));
            }
            x->x_buflength = buflength;
        }
    }
}

static void voutlet_dsp(t_voutlet *x, t_signal **sp)
{
    if (!x->x_rb)
        return;
    reblocker_resize(&x->x_rb, x->x_nchans, sp[0]->s_nchans, x->x_buflength);
    x->x_nchans = sp[0]->s_nchans;
    if (x->x_borrowed)
    {
            /* if we're just going to make the signal available on the
            parent patch, hand it off to the parent signal. */
        signal_setborrowed(*x->x_parentsignal, sp[0]);
    }
    else if (x->x_parentsignal)
    {
        int i;
        signal_setmultiout(x->x_parentsignal, sp[0]->s_nchans);
        if (x->x_justcopyout)
            dsp_add_copy(sp[0]->s_vec, (*x->x_parentsignal)->s_vec,
                sp[0]->s_length * sp[0]->s_nchans);
        else for (i = 0; i < x->x_nchans; i++)
            dsp_add(voutlet_perform, 5, x, sp[0]->s_vec + i * sp[0]->s_length,
                x->x_rb[i].r_buf, (t_int)(i == x->x_nchans - 1 ? x->x_hop : 0),
                    (t_int)sp[0]->s_length);
    }
}

        /* set up epilog DSP code.  If we're reblocking, this is the
        time to copy the samples out to the containing object's outlets.
        If we aren't reblocking, there's nothing to do here.  */
void voutlet_dspepilog(struct _voutlet *x, t_signal **parentsigs,
    int myvecsize, int phase, int period, int frequency,
    int downsample, int upsample, int reblock, int switched)
{
    if (!x->x_rb)
        return;  /* this shouldn't be necesssary... */
    if (!x->x_parentsignal) /* toplevels do nothing */
        return;
    if (reblock)
    {
        int parentvecsize, i;
        int re_parentvecsize;
        int bigperiod, epilogphase, blockphase;
        if (x->x_parentsignal)
        {
            parentvecsize = (*x->x_parentsignal)->s_length;
            re_parentvecsize = parentvecsize * upsample / downsample;
        }
        else
        {
            bug("voutlet_dspepilog");
            parentvecsize = re_parentvecsize = 1;
        }
        bigperiod = myvecsize/re_parentvecsize;
        if (!bigperiod)
            bigperiod = 1;
        epilogphase = phase & (bigperiod - 1);
        blockphase = (phase + period - 1) & (bigperiod - 1) & (- period);
        if (re_parentvecsize * period > x->x_buflength)
            bug("voutlet_dspepilog");
        x->x_write = re_parentvecsize * blockphase;
        if (x->x_write == x->x_buflength)
            x->x_write = 0;
        if (period == 1 && frequency > 1)
            x->x_hop = re_parentvecsize / frequency;
        else x->x_hop = period * re_parentvecsize;
        if (x->x_parentsignal)
        {
                    /* set epilog pointer and schedule it */
            x->x_empty = re_parentvecsize * epilogphase;
            for (i = 0; i < x->x_nchans; i++)
            {
                t_int hop = (i == x->x_nchans-1 ? re_parentvecsize : 0);
                if (upsample * downsample == 1)
                    dsp_add(voutlet_doepilog, 5, x,
                        (*x->x_parentsignal)->s_vec + i * parentvecsize,
                            x->x_rb[i].r_buf,  hop, (t_int)parentvecsize);
                else
                {
                    int method = (x->x_updownmethod < 0 ?
                        (pd_compatibilitylevel < 44 ? 0 : 1) :
                            x->x_updownmethod);
                    x->x_rb[i].r_updown.downsample=downsample;
                    x->x_rb[i].r_updown.upsample=upsample;
                    dsp_add(voutlet_doepilog_resample, 5, x,
                        &x->x_rb[i].r_updown,
                            x->x_rb[i].r_buf, hop, (t_int)re_parentvecsize);
                    resampleto_dsp(&x->x_rb[i].r_updown,
                        (*x->x_parentsignal)->s_vec + i * parentvecsize,
                            re_parentvecsize, parentvecsize, method);
                }
            }
        }
    }
        /* if we aren't blocked but we are switched, the epilog code just
        copies zeros to the output.  In this case the DSP chain contains a
        function that causes DSP passes to jump over the epilog when the block
        is switched on, so this only happens when switched off. */
    else if (switched)
    {
        if (*x->x_parentsignal)
            dsp_add_zero((*x->x_parentsignal)->s_vec,
                (*x->x_parentsignal)->s_length *
                    (*x->x_parentsignal)->s_nchans);
    }
}

static void *voutlet_newsig(t_symbol *s)
{
    t_voutlet *x = (t_voutlet *)pd_new(voutlet_class);
    x->x_canvas = canvas_getcurrent();
    x->x_parentoutlet = canvas_addoutlet(x->x_canvas,
        &x->x_obj.ob_pd, &s_signal);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_buflength = 0;
    x->x_rb = (t_reblocker *)getbytes(sizeof(*x->x_rb));
    reblocker_init(x->x_rb, x->x_buflength);
    x->x_buflength = 0;
    x->x_nchans = 1;

    /*
     * up till now we provide several upsampling methods and 1 single
     * downsampling method (no filtering !)
     */
    x->x_updownmethod = symbol2resamplemethod(s);

    return (x);
}


static void voutlet_setup(void)
{
    voutlet_class = class_new(gensym("outlet"), (t_newmethod)voutlet_new,
        (t_method)voutlet_free, sizeof(t_voutlet),
            CLASS_NOINLET | CLASS_MULTICHANNEL, A_DEFSYM, 0);
    class_addcreator((t_newmethod)voutlet_newsig, gensym("outlet~"), A_DEFSYM, 0);
    class_addbang(voutlet_class, voutlet_bang);
    class_addpointer(voutlet_class, voutlet_pointer);
    class_addfloat(voutlet_class, (t_method)voutlet_float);
    class_addsymbol(voutlet_class, voutlet_symbol);
    class_addlist(voutlet_class, voutlet_list);
    class_addanything(voutlet_class, voutlet_anything);
    class_addmethod(voutlet_class, (t_method)voutlet_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(voutlet_class, gensym("inlet-outlet"));
}

/* ---------------------------- overall setup ----------------------------- */

void g_io_setup(void)
{
    vinlet_setup();
    voutlet_setup();
}
