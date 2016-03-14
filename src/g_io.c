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
void signal_setborrowed(t_signal *sig, t_signal *sig2);
void signal_makereusable(t_signal *sig);

/* ------------------------- vinlet -------------------------- */
t_class *vinlet_class;

typedef struct _vinlet
{
    t_object x_obj;
    t_canvas *x_canvas;
    t_inlet *x_inlet;
    int x_bufsize;
    t_float *x_buf;         /* signal buffer; zero if not a signal */
    t_float *x_endbuf;
    t_float *x_fill;
    t_float *x_read;
    int x_hop;
  /* if not reblocking, the next slot communicates the parent's inlet
     signal from the prolog to the DSP routine: */
    t_signal *x_directsignal;

  t_resample x_updown;
} t_vinlet;

static void *vinlet_new(t_symbol *s)
{
    t_vinlet *x = (t_vinlet *)pd_new(vinlet_class);
    x->x_canvas = canvas_getcurrent();
    x->x_inlet = canvas_addinlet(x->x_canvas, &x->x_obj.ob_pd, 0);
    x->x_bufsize = 0;
    x->x_buf = 0;
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
    canvas_rminlet(x->x_canvas, x->x_inlet);
    if (x->x_buf)
        t_freebytes(x->x_buf, x->x_bufsize * sizeof(*x->x_buf));
    resample_free(&x->x_updown);
}

t_inlet *vinlet_getit(t_pd *x)
{
    if (pd_class(x) != vinlet_class) bug("vinlet_getit");
    return (((t_vinlet *)x)->x_inlet);
}

/* ------------------------- signal inlet -------------------------- */
int vinlet_issignal(t_vinlet *x)
{
    return (x->x_buf != 0);
}

static int tot;

t_int *vinlet_perform(t_int *w)
{
    t_vinlet *x = (t_vinlet *)(w[1]);
    t_float *out = (t_float *)(w[2]);
    int n = (int)(w[3]);
    t_float *in = x->x_read;
#if 0
    if (tot < 5) post("-in %lx out %lx n %d", in, out, n);
    if (tot < 5) post("-buf %lx endbuf %lx", x->x_buf, x->x_endbuf);
    if (tot < 5) post("in[0] %f in[1] %f in[2] %f", in[0], in[1], in[2]);
#endif
    while (n--) *out++ = *in++;
    if (in == x->x_endbuf) in = x->x_buf;
    x->x_read = in;
    return (w+4);
}

static void vinlet_dsp(t_vinlet *x, t_signal **sp)
{
    t_signal *outsig;
        /* no buffer means we're not a signal inlet */
    if (!x->x_buf)
        return;
    outsig = sp[0];
    if (x->x_directsignal)
    {
        signal_setborrowed(sp[0], x->x_directsignal);
    }
    else
    {
        dsp_add(vinlet_perform, 3, x, outsig->s_vec, outsig->s_vecsize);
        x->x_read = x->x_buf;
    }
}

    /* prolog code: loads buffer from parent patch */
t_int *vinlet_doprolog(t_int *w)
{
    t_vinlet *x = (t_vinlet *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    int n = (int)(w[3]);
    t_float *out = x->x_fill;
    if (out == x->x_endbuf)
    {
      t_float *f1 = x->x_buf, *f2 = x->x_buf + x->x_hop;
        int nshift = x->x_bufsize - x->x_hop;
        out -= x->x_hop;
        while (nshift--) *f1++ = *f2++;
    }
#if 0
    if (tot < 5) post("in %lx out %lx n %lx", in, out, n), tot++;
    if (tot < 5) post("in[0] %f in[1] %f in[2] %f", in[0], in[1], in[2]);
#endif

    while (n--) *out++ = *in++;
    x->x_fill = out;
    return (w+4);
}

int inlet_getsignalindex(t_inlet *x);

        /* set up prolog DSP code  */
void vinlet_dspprolog(struct _vinlet *x, t_signal **parentsigs,
    int myvecsize, int calcsize, int phase, int period, int frequency,
    int downsample, int upsample,  int reblock, int switched)
{
    t_signal *insig, *outsig;
        /* no buffer means we're not a signal inlet */
    if (!x->x_buf)
        return;
    x->x_updown.downsample = downsample;
    x->x_updown.upsample   = upsample;

        /* if the "reblock" flag is set, arrange to copy data in from the
        parent. */
    if (reblock)
    {
        int parentvecsize, bufsize, oldbufsize, prologphase;
        int re_parentvecsize; /* resampled parentvectorsize */
            /* this should never happen: */
        if (!x->x_buf) return;

            /* the prolog code counts from 0 to period-1; the
            phase is backed up by one so that AFTER the prolog code
            runs, the "x_fill" phase is in sync with the "x_read" phase. */
        prologphase = (phase - 1) & (period - 1);
        if (parentsigs)
        {
            insig = parentsigs[inlet_getsignalindex(x->x_inlet)];
            parentvecsize = insig->s_vecsize;
            re_parentvecsize = parentvecsize * upsample / downsample;
        }
        else
        {
            insig = 0;
            parentvecsize = 1;
            re_parentvecsize = 1;
        }

        bufsize = re_parentvecsize;
        if (bufsize < myvecsize) bufsize = myvecsize;
        if (bufsize != (oldbufsize = x->x_bufsize))
        {
            t_float *buf = x->x_buf;
            t_freebytes(buf, oldbufsize * sizeof(*buf));
            buf = (t_float *)t_getbytes(bufsize * sizeof(*buf));
            memset((char *)buf, 0, bufsize * sizeof(*buf));
            x->x_bufsize = bufsize;
            x->x_endbuf = buf + bufsize;
            x->x_buf = buf;
        }
        if (parentsigs)
        {
            x->x_hop = period * re_parentvecsize;

            x->x_fill = x->x_endbuf -
              (x->x_hop - prologphase * re_parentvecsize);

            if (upsample * downsample == 1)
                    dsp_add(vinlet_doprolog, 3, x, insig->s_vec,
                        re_parentvecsize);
            else {
              int method = (x->x_updown.method == 3?
                (pd_compatibilitylevel < 44 ? 0 : 1) : x->x_updown.method);
              resamplefrom_dsp(&x->x_updown, insig->s_vec, parentvecsize,
                re_parentvecsize, method);
              dsp_add(vinlet_doprolog, 3, x, x->x_updown.s_vec,
                re_parentvecsize);
        }

            /* if the input signal's reference count is zero, we have
               to free it here because we didn't in ugen_doit(). */
            if (!insig->s_refcount)
                signal_makereusable(insig);
        }
        else memset((char *)(x->x_buf), 0, bufsize * sizeof(*x->x_buf));
        x->x_directsignal = 0;
    }
    else
    {
            /* no reblocking; in this case our output signal is "borrowed"
            and merely needs to be pointed to the real one. */
        x->x_directsignal = parentsigs[inlet_getsignalindex(x->x_inlet)];
    }
}

static void *vinlet_newsig(t_symbol *s)
{
    t_vinlet *x = (t_vinlet *)pd_new(vinlet_class);
    x->x_canvas = canvas_getcurrent();
    x->x_inlet = canvas_addinlet(x->x_canvas, &x->x_obj.ob_pd, &s_signal);
    x->x_endbuf = x->x_buf = (t_float *)getbytes(0);
    x->x_bufsize = 0;
    x->x_directsignal = 0;
    outlet_new(&x->x_obj, &s_signal);

    resample_init(&x->x_updown);

    /* this should be though over:
     * it might prove hard to provide consistency between labeled up- & downsampling methods
     * maybe indeces would be better...
     *
     * up till now we provide several upsampling methods and 1 single downsampling method (no filtering !)
     */
    if (s == gensym("hold"))
        x->x_updown.method=1;       /* up: sample and hold */
    else if (s == gensym("lin") || s == gensym("linear"))
        x->x_updown.method=2;       /* up: linear interpolation */
    else if (s == gensym("pad"))
        x->x_updown.method=0;       /* up: zero-padding */
    else x->x_updown.method=3;      /* sample/hold unless version<0.44 */

    return (x);
}

static void vinlet_setup(void)
{
    vinlet_class = class_new(gensym("inlet"), (t_newmethod)vinlet_new,
        (t_method)vinlet_free, sizeof(t_vinlet), CLASS_NOINLET, A_DEFSYM, 0);
    class_addcreator((t_newmethod)vinlet_newsig, gensym("inlet~"), A_DEFSYM, 0);
    class_addbang(vinlet_class, vinlet_bang);
    class_addpointer(vinlet_class, vinlet_pointer);
    class_addfloat(vinlet_class, vinlet_float);
    class_addsymbol(vinlet_class, vinlet_symbol);
    class_addlist(vinlet_class, vinlet_list);
    class_addanything(vinlet_class, vinlet_anything);
    class_addmethod(vinlet_class, (t_method)vinlet_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(vinlet_class, gensym("pd"));
}

/* ------------------------- voutlet -------------------------- */

t_class *voutlet_class;

typedef struct _voutlet
{
    t_object x_obj;
    t_canvas *x_canvas;
    t_outlet *x_parentoutlet;
    int x_bufsize;
    t_sample *x_buf;         /* signal buffer; zero if not a signal */
    t_sample *x_endbuf;
    t_sample *x_empty;       /* next to read out of buffer in epilog code */
    t_sample *x_write;       /* next to write in to buffer */
    int x_hop;              /* hopsize */
        /* vice versa from the inlet, if we don't block, this holds the
        parent's outlet signal, valid between the prolog and the dsp setup
        routines.  */
    t_signal *x_directsignal;
        /* and here's a flag indicating that we aren't blocked but have to
        do a copy (because we're switched). */
    char x_justcopyout;
  t_resample x_updown;
} t_voutlet;

static void *voutlet_new(t_symbol *s)
{
    t_voutlet *x = (t_voutlet *)pd_new(voutlet_class);
    x->x_canvas = canvas_getcurrent();
    x->x_parentoutlet = canvas_addoutlet(x->x_canvas, &x->x_obj.ob_pd, 0);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, 0, 0);
    x->x_bufsize = 0;
    x->x_buf = 0;
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
    canvas_rmoutlet(x->x_canvas, x->x_parentoutlet);
    if (x->x_buf)
        t_freebytes(x->x_buf, x->x_bufsize * sizeof(*x->x_buf));
    resample_free(&x->x_updown);
}

t_outlet *voutlet_getit(t_pd *x)
{
    if (pd_class(x) != voutlet_class) bug("voutlet_getit");
    return (((t_voutlet *)x)->x_parentoutlet);
}

/* ------------------------- signal outlet -------------------------- */

int voutlet_issignal(t_voutlet *x)
{
    return (x->x_buf != 0);
}

    /* LATER optimize for non-overlapped case where the "+=" isn't needed */
t_int *voutlet_perform(t_int *w)
{
    t_voutlet *x = (t_voutlet *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    int n = (int)(w[3]);
    t_sample *out = x->x_write, *outwas = out;
#if 0
    if (tot < 5) post("-in %lx out %lx n %d", in, out, n);
    if (tot < 5) post("-buf %lx endbuf %lx", x->x_buf, x->x_endbuf);
#endif
    while (n--)
    {
        *out++ += *in++;
        if (out == x->x_endbuf) out = x->x_buf;
    }
    outwas += x->x_hop;
    if (outwas >= x->x_endbuf) outwas = x->x_buf;
    x->x_write = outwas;
    return (w+4);
}

    /* epilog code for blocking: write buffer to parent patch */
static t_int *voutlet_doepilog(t_int *w)
{
    t_voutlet *x = (t_voutlet *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);

    int n = (int)(w[3]);
    t_sample *in = x->x_empty;
    if (x->x_updown.downsample != x->x_updown.upsample)
        out = x->x_updown.s_vec;

#if 0
    if (tot < 5) post("outlet in %lx out %lx n %lx", in, out, n), tot++;
#endif
    for (; n--; in++) *out++ = *in, *in = 0;
    if (in == x->x_endbuf) in = x->x_buf;
    x->x_empty = in;
    return (w+4);
}

static t_int *voutlet_doepilog_resampling(t_int *w)
{
    t_voutlet *x = (t_voutlet *)(w[1]);
    int n = (int)(w[2]);
    t_sample *in  = x->x_empty;
    t_sample *out = x->x_updown.s_vec;

#if 0
    if (tot < 5) post("outlet in %lx out %lx n %lx", in, out, n), tot++;
#endif
    for (; n--; in++) *out++ = *in, *in = 0;
    if (in == x->x_endbuf) in = x->x_buf;
    x->x_empty = in;
    return (w+3);
}

int outlet_getsignalindex(t_outlet *x);

        /* prolog for outlets -- store pointer to the outlet on the
        parent, which, if "reblock" is false, will want to refer
        back to whatever we see on our input during the "dsp" method
        called later.  */
void voutlet_dspprolog(struct _voutlet *x, t_signal **parentsigs,
    int myvecsize, int calcsize, int phase, int period, int frequency,
    int downsample, int upsample, int reblock, int switched)
{
        /* no buffer means we're not a signal outlet */
    if (!x->x_buf)
        return;
    x->x_updown.downsample=downsample;
    x->x_updown.upsample=upsample;
    x->x_justcopyout = (switched && !reblock);
    if (reblock)
    {
        x->x_directsignal = 0;
    }
    else
    {
        if (!parentsigs) bug("voutlet_dspprolog");
        x->x_directsignal =
            parentsigs[outlet_getsignalindex(x->x_parentoutlet)];
    }
}

static void voutlet_dsp(t_voutlet *x, t_signal **sp)
{
    t_signal *insig;
    if (!x->x_buf) return;
    insig = sp[0];
    if (x->x_justcopyout)
        dsp_add_copy(insig->s_vec, x->x_directsignal->s_vec, insig->s_n);
    else if (x->x_directsignal)
    {
            /* if we're just going to make the signal available on the
            parent patch, hand it off to the parent signal. */
        /* this is done elsewhere--> sp[0]->s_refcount++; */
        signal_setborrowed(x->x_directsignal, sp[0]);
    }
    else
        dsp_add(voutlet_perform, 3, x, insig->s_vec, insig->s_n);
}

        /* set up epilog DSP code.  If we're reblocking, this is the
        time to copy the samples out to the containing object's outlets.
        If we aren't reblocking, there's nothing to do here.  */
void voutlet_dspepilog(struct _voutlet *x, t_signal **parentsigs,
    int myvecsize, int calcsize, int phase, int period, int frequency,
    int downsample, int upsample, int reblock, int switched)
{
    if (!x->x_buf) return;  /* this shouldn't be necesssary... */
    x->x_updown.downsample=downsample;
    x->x_updown.upsample=upsample;
    if (reblock)
    {
        t_signal *insig, *outsig;
        int parentvecsize, bufsize, oldbufsize;
        int re_parentvecsize;
        int bigperiod, epilogphase, blockphase;
        if (parentsigs)
        {
            outsig = parentsigs[outlet_getsignalindex(x->x_parentoutlet)];
            parentvecsize = outsig->s_vecsize;
            re_parentvecsize = parentvecsize * upsample / downsample;
        }
        else
        {
            outsig = 0;
            parentvecsize = 1;
            re_parentvecsize = 1;
        }
        bigperiod = myvecsize/re_parentvecsize;
        if (!bigperiod) bigperiod = 1;
        epilogphase = phase & (bigperiod - 1);
        blockphase = (phase + period - 1) & (bigperiod - 1) & (- period);
        bufsize = re_parentvecsize;
        if (bufsize < myvecsize) bufsize = myvecsize;
        if (bufsize != (oldbufsize = x->x_bufsize))
        {
            t_sample *buf = x->x_buf;
            t_freebytes(buf, oldbufsize * sizeof(*buf));
            buf = (t_sample *)t_getbytes(bufsize * sizeof(*buf));
            memset((char *)buf, 0, bufsize * sizeof(*buf));
            x->x_bufsize = bufsize;
            x->x_endbuf = buf + bufsize;
            x->x_buf = buf;
        }
        if (re_parentvecsize * period > bufsize) bug("voutlet_dspepilog");
        x->x_write = x->x_buf + re_parentvecsize * blockphase;
        if (x->x_write == x->x_endbuf) x->x_write = x->x_buf;
        if (period == 1 && frequency > 1)
            x->x_hop = re_parentvecsize / frequency;
        else x->x_hop = period * re_parentvecsize;
        /* post("phase %d, block %d, parent %d", phase & 63,
            parentvecsize * blockphase, parentvecsize * epilogphase); */
        if (parentsigs)
        {
            /* set epilog pointer and schedule it */
            x->x_empty = x->x_buf + re_parentvecsize * epilogphase;
            if (upsample * downsample == 1)
                dsp_add(voutlet_doepilog, 3, x, outsig->s_vec,
                    re_parentvecsize);
            else
            {
                int method = (x->x_updown.method == 3?
                    (pd_compatibilitylevel < 44 ? 0 : 1) : x->x_updown.method);
                dsp_add(voutlet_doepilog_resampling, 2, x, re_parentvecsize);
                resampleto_dsp(&x->x_updown, outsig->s_vec, re_parentvecsize,
                    parentvecsize, method);
            }
        }
    }
        /* if we aren't blocked but we are switched, the epilog code just
        copies zeros to the output.  In this case the blocking code actually
        jumps over the epilog if the block is running. */
    else if (switched)
    {
        if (parentsigs)
        {
            t_signal *outsig =
                parentsigs[outlet_getsignalindex(x->x_parentoutlet)];
            dsp_add_zero(outsig->s_vec, outsig->s_n);
        }
    }
}

static void *voutlet_newsig(t_symbol *s)
{
    t_voutlet *x = (t_voutlet *)pd_new(voutlet_class);
    x->x_canvas = canvas_getcurrent();
    x->x_parentoutlet = canvas_addoutlet(x->x_canvas,
        &x->x_obj.ob_pd, &s_signal);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_endbuf = x->x_buf = (t_sample *)getbytes(0);
    x->x_bufsize = 0;

    resample_init(&x->x_updown);

    /* this should be though over:
     * it might prove hard to provide consistency between labeled up- & downsampling methods
     * maybe indeces would be better...
     *
     * up till now we provide several upsampling methods and 1 single downsampling method (no filtering !)
     */
    if (s == gensym("hold"))x->x_updown.method=1;        /* up: sample and hold */
    else if (s == gensym("lin"))x->x_updown.method=2;    /* up: linear interpolation */
    else if (s == gensym("linear"))x->x_updown.method=2; /* up: linear interpolation */
    else if (s == gensym("pad"))x->x_updown.method=0;    /* up: zero pad */
    else x->x_updown.method=3;                           /* up: zero-padding; down: ignore samples inbetween */

    return (x);
}


static void voutlet_setup(void)
{
    voutlet_class = class_new(gensym("outlet"), (t_newmethod)voutlet_new,
        (t_method)voutlet_free, sizeof(t_voutlet), CLASS_NOINLET, A_DEFSYM, 0);
    class_addcreator((t_newmethod)voutlet_newsig, gensym("outlet~"), A_DEFSYM, 0);
    class_addbang(voutlet_class, voutlet_bang);
    class_addpointer(voutlet_class, voutlet_pointer);
    class_addfloat(voutlet_class, (t_method)voutlet_float);
    class_addsymbol(voutlet_class, voutlet_symbol);
    class_addlist(voutlet_class, voutlet_list);
    class_addanything(voutlet_class, voutlet_anything);
    class_addmethod(voutlet_class, (t_method)voutlet_dsp,
        gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(voutlet_class, gensym("pd"));
}


/* ---------------------------- overall setup ----------------------------- */

void g_io_setup(void)
{
    vinlet_setup();
    voutlet_setup();
}
