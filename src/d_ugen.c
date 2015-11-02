/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  These routines build a copy of the DSP portion of a graph, which is
    then sorted into a linear list of DSP operations which are added to
    the DSP duty cycle called by the scheduler.  Once that's been done,
    we delete the copy.  The DSP objects are represented by "ugenbox"
    structures which are parallel to the DSP objects in the graph and
    have vectors of siginlets and sigoutlets which record their
    interconnections.
*/

#include "m_pd.h"
#include "m_imp.h"
#include <stdlib.h>
#include <stdarg.h>

extern t_class *vinlet_class, *voutlet_class, *canvas_class, *text_class;
t_float *obj_findsignalscalar(t_object *x, int m);
static int ugen_loud;

EXTERN_STRUCT _vinlet;
EXTERN_STRUCT _voutlet;

void vinlet_dspprolog(struct _vinlet *x, t_signal **parentsigs,
    int myvecsize, int calcsize, int phase, int period, int frequency,
    int downsample, int upsample,  int reblock, int switched);
void voutlet_dspprolog(struct _voutlet *x, t_signal **parentsigs,
    int myvecsize, int calcsize, int phase, int period, int frequency,
    int downsample, int upsample, int reblock, int switched);
void voutlet_dspepilog(struct _voutlet *x, t_signal **parentsigs,
    int myvecsize, int calcsize, int phase, int period, int frequency,
    int downsample, int upsample, int reblock, int switched);

t_int *zero_perform(t_int *w)   /* zero out a vector */
{
    t_sample *out = (t_sample *)(w[1]);
    int n = (int)(w[2]);
    while (n--) *out++ = 0;
    return (w+3);
}

t_int *zero_perf8(t_int *w)
{
    t_sample *out = (t_sample *)(w[1]);
    int n = (int)(w[2]);

    for (; n; n -= 8, out += 8)
    {
        out[0] = 0;
        out[1] = 0;
        out[2] = 0;
        out[3] = 0;
        out[4] = 0;
        out[5] = 0;
        out[6] = 0;
        out[7] = 0;
    }
    return (w+3);
}

void dsp_add_zero(t_sample *out, int n)
{
    if (n&7)
        dsp_add(zero_perform, 2, out, n);
    else
        dsp_add(zero_perf8, 2, out, n);
}

/* ---------------------------- block~ ----------------------------- */

/* The "block~ object maintains the containing canvas's DSP computation,
calling it at a super- or sub-multiple of the containing canvas's
calling frequency.  The block~'s creation arguments specify block size
and overlap.  Block~ does no "dsp" computation in its own right, but it
adds prolog and epilog code before and after the canvas's unit generators.

A subcanvas need not have a block~ at all; if there's none, its
ugens are simply put on the list without any prolog or epilog code.

Block~ may be invoked as switch~, in which case it also acts to switch the
subcanvas on and off.  The overall order of scheduling for a subcanvas
is thus,

    inlet and outlet prologue code (1)
    block prologue (2)
    the objects in the subcanvas, including inlets and outlets
    block epilogue (2)
    outlet epilogue code (2)

where (1) means, "if reblocked" and  (2) means, "if reblocked or switched".

If we're reblocked, the inlet prolog and outlet epilog code takes care of
overlapping and buffering to deal with vector size changes.  If we're switched
but not reblocked, the inlet prolog is not needed, and the output epilog is
ONLY run when the block is switched off; in this case the epilog code simply
copies zeros to all signal outlets.
*/

static int dsp_phase;
static t_class *block_class;

typedef struct _block
{
    t_object x_obj;
    int x_vecsize;      /* size of audio signals in this block */
    int x_calcsize;     /* number of samples actually to compute */
    int x_overlap;
    int x_phase;        /* from 0 to period-1; when zero we run the block */
    int x_period;       /* submultiple of containing canvas */
    int x_frequency;    /* supermultiple of comtaining canvas */
    int x_count;        /* number of times parent block has called us */
    int x_chainonset;   /* beginning of code in DSP chain */
    int x_blocklength;  /* length of dspchain for this block */
    int x_epiloglength; /* length of epilog */
    char x_switched;    /* true if we're acting as a a switch */
    char x_switchon;    /* true if we're switched on */
    char x_reblock;     /* true if inlets and outlets are reblocking */
    int x_upsample;     /* upsampling-factor */
    int x_downsample;   /* downsampling-factor */
    int x_return;       /* stop right after this block (for one-shots) */
} t_block;

static void block_set(t_block *x, t_floatarg fvecsize, t_floatarg foverlap,
    t_floatarg fupsample);

static void *block_new(t_floatarg fvecsize, t_floatarg foverlap,
                       t_floatarg fupsample)
{
    t_block *x = (t_block *)pd_new(block_class);
    x->x_phase = 0;
    x->x_period = 1;
    x->x_frequency = 1;
    x->x_switched = 0;
    x->x_switchon = 1;
    block_set(x, fvecsize, foverlap, fupsample);
    return (x);
}

static void block_set(t_block *x, t_floatarg fcalcsize, t_floatarg foverlap,
    t_floatarg fupsample)
{
    int upsample, downsample;
    int calcsize = fcalcsize;
    int overlap = foverlap;
    int dspstate = canvas_suspend_dsp();
    int vecsize;
    if (overlap < 1)
        overlap = 1;
    if (calcsize < 0)
        calcsize = 0;    /* this means we'll get it from parent later. */

    if (fupsample <= 0)
        upsample = downsample = 1;
    else if (fupsample >= 1) {
        upsample = fupsample;
        downsample   = 1;
    }
    else
    {
        downsample = 1.0 / fupsample;
        upsample   = 1;
    }

        /* vecsize is smallest power of 2 large enough to hold calcsize */
    if (calcsize)
    {
        if ((vecsize = (1 << ilog2(calcsize))) != calcsize)
            vecsize *= 2;
    }
    else vecsize = 0;
    if (vecsize && (vecsize != (1 << ilog2(vecsize))))
    {
        pd_error(x, "block~: vector size not a power of 2");
        vecsize = 64;
    }
    if (overlap != (1 << ilog2(overlap)))
    {
        pd_error(x, "block~: overlap not a power of 2");
        overlap = 1;
    }
    if (downsample != (1 << ilog2(downsample)))
    {
        pd_error(x, "block~: downsampling not a power of 2");
        downsample = 1;
    }
    if (upsample != (1 << ilog2(upsample)))
    {
        pd_error(x, "block~: upsampling not a power of 2");
        upsample = 1;
    }

    x->x_calcsize = calcsize;
    x->x_vecsize = vecsize;
    x->x_overlap = overlap;
    x->x_upsample = upsample;
    x->x_downsample = downsample;
    canvas_resume_dsp(dspstate);
}

static void *switch_new(t_floatarg fvecsize, t_floatarg foverlap,
                        t_floatarg fupsample)
{
    t_block *x = (t_block *)(block_new(fvecsize, foverlap, fupsample));
    x->x_switched = 1;
    x->x_switchon = 0;
    return (x);
}

static void block_float(t_block *x, t_floatarg f)
{
    if (x->x_switched)
        x->x_switchon = (f != 0);
}

static void block_bang(t_block *x)
{
    if (x->x_switched && !x->x_switchon && pd_this->pd_dspchain)
    {
        t_int *ip;
        x->x_return = 1;
        for (ip = pd_this->pd_dspchain + x->x_chainonset; ip; )
            ip = (*(t_perfroutine)(*ip))(ip);
        x->x_return = 0;
    }
    else pd_error(x, "bang to block~ or on-state switch~ has no effect");
}


#define PROLOGCALL 2
#define EPILOGCALL 2

static t_int *block_prolog(t_int *w)
{
    t_block *x = (t_block *)w[1];
    int phase = x->x_phase;
        /* if we're switched off, jump past the epilog code */
    if (!x->x_switchon)
        return (w + x->x_blocklength);
    if (phase)
    {
        phase++;
        if (phase == x->x_period) phase = 0;
        x->x_phase = phase;
        return (w + x->x_blocklength);  /* skip block; jump past epilog */
    }
    else
    {
        x->x_count = x->x_frequency;
        x->x_phase = (x->x_period > 1 ? 1 : 0);
        return (w + PROLOGCALL);        /* beginning of block is next ugen */
    }
}

static t_int *block_epilog(t_int *w)
{
    t_block *x = (t_block *)w[1];
    int count = x->x_count - 1;
    if (x->x_return)
        return (0);
    if (!x->x_reblock)
        return (w + x->x_epiloglength + EPILOGCALL);
    if (count)
    {
        x->x_count = count;
        return (w - (x->x_blocklength -
            (PROLOGCALL + EPILOGCALL)));   /* go to ugen after prolog */
    }
    else return (w + EPILOGCALL);
}

static void block_dsp(t_block *x, t_signal **sp)
{
    /* do nothing here */
}

void block_tilde_setup(void)
{
    block_class = class_new(gensym("block~"), (t_newmethod)block_new, 0,
            sizeof(t_block), 0, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addcreator((t_newmethod)switch_new, gensym("switch~"),
        A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(block_class, (t_method)block_set, gensym("set"),
        A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(block_class, (t_method)block_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(block_class, block_float);
    class_addbang(block_class, block_bang);
}

/* ------------------ DSP call list ----------------------- */

static t_int dsp_done(t_int *w)
{
    return (0);
}

void dsp_add(t_perfroutine f, int n, ...)
{
    int newsize = pd_this->pd_dspchainsize + n+1, i;
    va_list ap;

    pd_this->pd_dspchain = t_resizebytes(pd_this->pd_dspchain,
        pd_this->pd_dspchainsize * sizeof (t_int), newsize * sizeof (t_int));
    pd_this->pd_dspchain[pd_this->pd_dspchainsize-1] = (t_int)f;
    if (ugen_loud)
        post("add to chain: %lx",
            pd_this->pd_dspchain[pd_this->pd_dspchainsize-1]);
    va_start(ap, n);
    for (i = 0; i < n; i++)
    {
        pd_this->pd_dspchain[pd_this->pd_dspchainsize + i] = va_arg(ap, t_int);
        if (ugen_loud)
            post("add to chain: %lx",
                pd_this->pd_dspchain[pd_this->pd_dspchainsize + i]);
    }
    va_end(ap);
    pd_this->pd_dspchain[newsize-1] = (t_int)dsp_done;
    pd_this->pd_dspchainsize = newsize;
}

    /* at Guenter's suggestion, here's a vectorized version */
void dsp_addv(t_perfroutine f, int n, t_int *vec)
{
    int newsize = pd_this->pd_dspchainsize + n+1, i;

    pd_this->pd_dspchain = t_resizebytes(pd_this->pd_dspchain,
        pd_this->pd_dspchainsize * sizeof (t_int), newsize * sizeof (t_int));
    pd_this->pd_dspchain[pd_this->pd_dspchainsize-1] = (t_int)f;
    for (i = 0; i < n; i++)
        pd_this->pd_dspchain[pd_this->pd_dspchainsize + i] = vec[i];
    pd_this->pd_dspchain[newsize-1] = (t_int)dsp_done;
    pd_this->pd_dspchainsize = newsize;
}

void dsp_tick(void)
{
    if (pd_this->pd_dspchain)
    {
        t_int *ip;
        for (ip = pd_this->pd_dspchain; ip; ) ip = (*(t_perfroutine)(*ip))(ip);
        dsp_phase++;
    }
}

/* ---------------- signals ---------------------------- */

int ilog2(int n)
{
    int r = -1;
    if (n <= 0) return(0);
    while (n)
    {
        r++;
        n >>= 1;
    }
    return (r);
}

    /* list of signals which can be reused, sorted by buffer size */
static t_signal *signal_freelist[MAXLOGSIG+1];
    /* list of reusable "borrowed" signals (which don't own sample buffers) */
static t_signal *signal_freeborrowed;

    /* call this when DSP is stopped to free all the signals */
void signal_cleanup(void)
{
    t_signal **svec, *sig, *sig2;
    int i;
    while ((sig = pd_this->pd_signals))
    {
        pd_this->pd_signals = sig->s_nextused;
        if (!sig->s_isborrowed)
            t_freebytes(sig->s_vec, sig->s_vecsize * sizeof (*sig->s_vec));
        t_freebytes(sig, sizeof *sig);
    }
    for (i = 0; i <= MAXLOGSIG; i++)
        signal_freelist[i] = 0;
    signal_freeborrowed = 0;
}

    /* mark the signal "reusable." */
void signal_makereusable(t_signal *sig)
{
    int logn = ilog2(sig->s_vecsize);
#if 1
    t_signal *s5;
    for (s5 = signal_freeborrowed; s5; s5 = s5->s_nextfree)
    {
        if (s5 == sig)
        {
            bug("signal_free 3");
            return;
        }
    }
    for (s5 = signal_freelist[logn]; s5; s5 = s5->s_nextfree)
    {
        if (s5 == sig)
        {
            bug("signal_free 4");
            return;
        }
    }
#endif
    if (ugen_loud) post("free %lx: %d", sig, sig->s_isborrowed);
    if (sig->s_isborrowed)
    {
            /* if the signal is borrowed, decrement the borrowed-from signal's
                reference count, possibly marking it reusable too */
        t_signal *s2 = sig->s_borrowedfrom;
        if ((s2 == sig) || !s2)
            bug("signal_free");
        s2->s_refcount--;
        if (!s2->s_refcount)
            signal_makereusable(s2);
        sig->s_nextfree = signal_freeborrowed;
        signal_freeborrowed = sig;
    }
    else
    {
            /* if it's a real signal (not borrowed), put it on the free list
                so we can reuse it. */
        if (signal_freelist[logn] == sig) bug("signal_free 2");
        sig->s_nextfree = signal_freelist[logn];
        signal_freelist[logn] = sig;
    }
}

    /* reclaim or make an audio signal.  If n is zero, return a "borrowed"
    signal whose buffer and size will be obtained later via
    signal_setborrowed(). */

t_signal *signal_new(int n, t_float sr)
{
    int logn, n2, vecsize = 0;
    t_signal *ret, **whichlist;
    t_sample *fp;
    logn = ilog2(n);
    if (n)
    {
        if ((vecsize = (1<<logn)) != n)
            vecsize *= 2;
        if (logn > MAXLOGSIG)
            bug("signal buffer too large");
        whichlist = signal_freelist + logn;
    }
    else
        whichlist = &signal_freeborrowed;

        /* first try to reclaim one from the free list */
    if ((ret = *whichlist))
        *whichlist = ret->s_nextfree;
    else
    {
            /* LATER figure out what to do for out-of-space here! */
        ret = (t_signal *)t_getbytes(sizeof *ret);
        if (n)
        {
            ret->s_vec = (t_sample *)getbytes(vecsize * sizeof (*ret->s_vec));
            ret->s_isborrowed = 0;
        }
        else
        {
            ret->s_vec = 0;
            ret->s_isborrowed = 1;
        }
        ret->s_nextused = pd_this->pd_signals;
        pd_this->pd_signals = ret;
    }
    ret->s_n = n;
    ret->s_vecsize = vecsize;
    ret->s_sr = sr;
    ret->s_refcount = 0;
    ret->s_borrowedfrom = 0;
    if (ugen_loud) post("new %lx: %lx", ret, ret->s_vec);
    return (ret);
}

static t_signal *signal_newlike(const t_signal *sig)
{
    return (signal_new(sig->s_n, sig->s_sr));
}

void signal_setborrowed(t_signal *sig, t_signal *sig2)
{
    if (!sig->s_isborrowed || sig->s_borrowedfrom)
        bug("signal_setborrowed");
    if (sig == sig2)
        bug("signal_setborrowed 2");
    sig->s_borrowedfrom = sig2;
    sig->s_vec = sig2->s_vec;
    sig->s_n = sig2->s_n;
    sig->s_vecsize = sig2->s_vecsize;
    if (ugen_loud) post("set borrowed %lx: %lx", sig, sig->s_vec);
}

int signal_compatible(t_signal *s1, t_signal *s2)
{
    return (s1->s_n == s2->s_n && s1->s_sr == s2->s_sr);
}

/* ------------------ ugen ("unit generator") sorting ----------------- */

typedef struct _ugenbox
{
    struct _siginlet *u_in;
    int u_nin;
    struct _sigoutlet *u_out;
    int u_nout;
    int u_phase;
    struct _ugenbox *u_next;
    t_object *u_obj;
    int u_done;
} t_ugenbox;

typedef struct _siginlet
{
    int i_nconnect;
    int i_ngot;
    t_signal *i_signal;
} t_siginlet;

typedef struct _sigoutconnect
{
    t_ugenbox *oc_who;
    int oc_inno;
    struct _sigoutconnect *oc_next;
} t_sigoutconnect;

typedef struct _sigoutlet
{
    int o_nconnect;
    int o_nsent;
    t_signal *o_signal;
    t_sigoutconnect *o_connections;
} t_sigoutlet;


struct _dspcontext
{
    struct _ugenbox *dc_ugenlist;
    struct _dspcontext *dc_parentcontext;
    int dc_ninlets;
    int dc_noutlets;
    t_signal **dc_iosigs;
    t_float dc_srate;
    int dc_vecsize;         /* vector size, power of two */
    int dc_calcsize;        /* number of elements to calculate */
    char dc_toplevel;       /* true if "iosigs" is invalid. */
    char dc_reblock;        /* true if we have to reblock inlets/outlets */
    char dc_switched;       /* true if we're switched */

};

#define t_dspcontext struct _dspcontext

static int ugen_sortno = 0;
static t_dspcontext *ugen_currentcontext;

    /* get a new signal for the current context - used by clone~ object */
t_signal *signal_newfromcontext(int borrowed)
{
    return (signal_new((borrowed? 0 : ugen_currentcontext->dc_calcsize),
        ugen_currentcontext->dc_srate));
}

void ugen_stop(void)
{
    t_signal *s;
    int i;
    if (pd_this->pd_dspchain)
    {
        freebytes(pd_this->pd_dspchain,
            pd_this->pd_dspchainsize * sizeof (t_int));
        pd_this->pd_dspchain = 0;
    }
    signal_cleanup();

}

void ugen_start(void)
{
    ugen_stop();
    ugen_sortno++;
    pd_this->pd_dspchain = (t_int *)getbytes(sizeof(*pd_this->pd_dspchain));
    pd_this->pd_dspchain[0] = (t_int)dsp_done;
    pd_this->pd_dspchainsize = 1;
    if (ugen_currentcontext) bug("ugen_start");
}

int ugen_getsortno(void)
{
    return (ugen_sortno);
}

#if 1
void glob_foo(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int i, count;
    t_signal *sig;
    for (count = 0, sig = pd_this->pd_signals; sig;
        count++, sig = sig->s_nextused)
            ;
    post("used signals %d", count);
    for (i = 0; i < MAXLOGSIG; i++)
    {
        for (count = 0, sig = signal_freelist[i]; sig;
            count++, sig = sig->s_nextfree)
                ;
        if (count)
            post("size %d: free %d", (1 << i), count);
    }
    for (count = 0, sig = signal_freeborrowed; sig;
        count++, sig = sig->s_nextfree)
            ;
    post("free borrowed %d", count);

    ugen_loud = argc;
}
#endif

    /* start building the graph for a canvas */
t_dspcontext *ugen_start_graph(int toplevel, t_signal **sp,
    int ninlets, int noutlets)
{
    t_dspcontext *dc = (t_dspcontext *)getbytes(sizeof(*dc));
    int parent_vecsize, vecsize;

    if (ugen_loud) post("ugen_start_graph...");

    /* protect against invalid numsignals.  This might happen if we have
    an abstraction with inlet~/outlet~  opened as a toplevel patch */
    if (toplevel)
        ninlets = noutlets = 0;

    dc->dc_ugenlist = 0;
    dc->dc_toplevel = toplevel;
    dc->dc_iosigs = sp;
    dc->dc_ninlets = ninlets;
    dc->dc_noutlets = noutlets;
    dc->dc_parentcontext = ugen_currentcontext;
    ugen_currentcontext = dc;
    return (dc);
}

    /* first the canvas calls this to create all the boxes... */
void ugen_add(t_dspcontext *dc, t_object *obj)
{
    t_ugenbox *x = (t_ugenbox *)getbytes(sizeof *x);
    int i;
    t_sigoutlet *uout;
    t_siginlet *uin;

    x->u_next = dc->dc_ugenlist;
    dc->dc_ugenlist = x;
    x->u_obj = obj;
    x->u_nin = obj_nsiginlets(obj);
    x->u_in = getbytes(x->u_nin * sizeof (*x->u_in));
    for (uin = x->u_in, i = x->u_nin; i--; uin++)
        uin->i_nconnect = 0;
    x->u_nout = obj_nsigoutlets(obj);
    x->u_out = getbytes(x->u_nout * sizeof (*x->u_out));
    for (uout = x->u_out, i = x->u_nout; i--; uout++)
        uout->o_connections = 0, uout->o_nconnect = 0;
}

    /* and then this to make all the connections. */
void ugen_connect(t_dspcontext *dc, t_object *x1, int outno, t_object *x2,
    int inno)
{
    t_ugenbox *u1, *u2;
    t_sigoutlet *uout;
    t_siginlet *uin;
    t_sigoutconnect *oc;
    int sigoutno = obj_sigoutletindex(x1, outno);
    int siginno = obj_siginletindex(x2, inno);
    if (ugen_loud)
        post("%s -> %s: %d->%d",
            class_getname(x1->ob_pd),
                class_getname(x2->ob_pd), outno, inno);
    for (u1 = dc->dc_ugenlist; u1 && u1->u_obj != x1; u1 = u1->u_next);
    for (u2 = dc->dc_ugenlist; u2 && u2->u_obj != x2; u2 = u2->u_next);
    if (!u1 || !u2 || siginno < 0)
    {
        if (!u1)
            error("object with signal outlets but no DSP method?");
                /* check if it's a "text" (i.e., object wasn't created) -
                if so fail silently */
        else if (!(x2 && (pd_class(&x2->ob_pd) == text_class)))
            pd_error(u1->u_obj,
                "signal outlet connect to nonsignal inlet (ignored)");
        return;
    }
    if (sigoutno < 0 || sigoutno >= u1->u_nout || siginno >= u2->u_nin)
    {
        bug("ugen_connect %s %s %d %d (%d %d)",
            class_getname(x1->ob_pd),
            class_getname(x2->ob_pd), sigoutno, siginno, u1->u_nout,
                u2->u_nin);
    }
    uout = u1->u_out + sigoutno;
    uin = u2->u_in + siginno;

        /* add a new connection to the outlet's list */
    oc = (t_sigoutconnect *)getbytes(sizeof *oc);
    oc->oc_next = uout->o_connections;
    uout->o_connections = oc;
    oc->oc_who = u2;
    oc->oc_inno = siginno;
        /* update inlet and outlet counts  */
    uout->o_nconnect++;
    uin->i_nconnect++;
}

    /* get the index of a ugenbox or -1 if it's not on the list */
static int ugen_index(t_dspcontext *dc, t_ugenbox *x)
{
    int ret;
    t_ugenbox *u;
    for (u = dc->dc_ugenlist, ret = 0; u; u = u->u_next, ret++)
        if (u == x) return (ret);
    return (-1);
}
extern t_class *clone_class;

    /* put a ugenbox on the chain, recursively putting any others on that
    this one might uncover. */
static void ugen_doit(t_dspcontext *dc, t_ugenbox *u)
{
    t_sigoutlet *uout;
    t_siginlet *uin;
    t_sigoutconnect *oc, *oc2;
    t_class *class = pd_class(&u->u_obj->ob_pd);
    int i, n;
        /* suppress creating new signals for the outputs of signal
        inlets and subpatchs; except in the case we're an inlet and "blocking"
        is set.  We don't yet know if a subcanvas will be "blocking" so there
        we delay new signal creation, which will be handled by calling
        signal_setborrowed in the ugen_done_graph routine below. */
    int nonewsigs = (class == canvas_class || 
        ((class == vinlet_class) && !(dc->dc_reblock)));
        /* when we encounter a subcanvas or a signal outlet, suppress freeing
        the input signals as they may be "borrowed" for the super or sub
        patch; same exception as above, but also if we're "switched" we
        have to do a copy rather than a borrow.  */
    int nofreesigs = (class == canvas_class || class == clone_class ||
        ((class == voutlet_class) &&  !(dc->dc_reblock || dc->dc_switched)));
    t_signal **insig, **outsig, **sig, *s1, *s2, *s3;
    t_ugenbox *u2;

    if (ugen_loud) post("doit %s %d %d", class_getname(class), nofreesigs,
        nonewsigs);
    for (i = 0, uin = u->u_in; i < u->u_nin; i++, uin++)
    {
        if (!uin->i_nconnect)
        {
            t_float *scalar;
            s3 = signal_new(dc->dc_calcsize, dc->dc_srate);
            /* post("%s: unconnected signal inlet set to zero",
                class_getname(u->u_obj->ob_pd)); */
            if ((scalar = obj_findsignalscalar(u->u_obj, i)))
                dsp_add_scalarcopy(scalar, s3->s_vec, s3->s_n);
            else
                dsp_add_zero(s3->s_vec, s3->s_n);
            uin->i_signal = s3;
            s3->s_refcount = 1;
        }
    }
    insig = (t_signal **)getbytes((u->u_nin + u->u_nout) * sizeof(t_signal *));
    outsig = insig + u->u_nin;
    for (sig = insig, uin = u->u_in, i = u->u_nin; i--; sig++, uin++)
    {
        int newrefcount;
        *sig = uin->i_signal;
        newrefcount = --(*sig)->s_refcount;
            /* if the reference count went to zero, we free the signal now,
            unless it's a subcanvas or outlet; these might keep the
            signal around to send to objects connected to them.  In this
            case we increment the reference count; the corresponding decrement
            is in sig_makereusable(). */
        if (nofreesigs)
            (*sig)->s_refcount++;
        else if (!newrefcount)
            signal_makereusable(*sig);
    }
    for (sig = outsig, uout = u->u_out, i = u->u_nout; i--; sig++, uout++)
    {
            /* similarly, for outlets of subcanvases we delay creating
            them; instead we create "borrowed" ones so that the refcount
            is known.  The subcanvas replaces the fake signal with one showing
            where the output data actually is, to avoid having to copy it.
            For any other object, we just allocate a new output vector;
            since we've already freed the inputs the objects might get called
            "in place." */
        if (nonewsigs)
        {
            *sig = uout->o_signal =
                signal_new(0, dc->dc_srate);
        }
        else
            *sig = uout->o_signal = signal_new(dc->dc_calcsize, dc->dc_srate);
        (*sig)->s_refcount = uout->o_nconnect;
    }
        /* now call the DSP scheduling routine for the ugen.  This
        routine must fill in "borrowed" signal outputs in case it's either
        a subcanvas or a signal inlet. */
    mess1(&u->u_obj->ob_pd, gensym("dsp"), insig);

        /* if any output signals aren't connected to anyone, free them
        now; otherwise they'll either get freed when the reference count
        goes back to zero, or even later as explained above. */

    for (sig = outsig, uout = u->u_out, i = u->u_nout; i--; sig++, uout++)
    {
        if (!(*sig)->s_refcount)
            signal_makereusable(*sig);
    }
    if (ugen_loud)
    {
        if (u->u_nin + u->u_nout == 0) post("put %s %d",
            class_getname(u->u_obj->ob_pd), ugen_index(dc, u));
        else if (u->u_nin + u->u_nout == 1) post("put %s %d (%lx)",
            class_getname(u->u_obj->ob_pd), ugen_index(dc, u), sig[0]);
        else if (u->u_nin + u->u_nout == 2) post("put %s %d (%lx %lx)",
            class_getname(u->u_obj->ob_pd), ugen_index(dc, u),
                sig[0], sig[1]);
        else post("put %s %d (%lx %lx %lx ...)",
            class_getname(u->u_obj->ob_pd), ugen_index(dc, u),
                sig[0], sig[1], sig[2]);
    }

        /* pass it on and trip anyone whose last inlet was filled */
    for (uout = u->u_out, i = u->u_nout; i--; uout++)
    {
        s1 = uout->o_signal;
        for (oc = uout->o_connections; oc; oc = oc->oc_next)
        {
            u2 = oc->oc_who;
            uin = &u2->u_in[oc->oc_inno];
                /* if there's already someone here, sum the two */
            if ((s2 = uin->i_signal))
            {
                s1->s_refcount--;
                s2->s_refcount--;
                if (!signal_compatible(s1, s2))
                {
                    pd_error(u->u_obj, "%s: incompatible signal inputs",
                        class_getname(u->u_obj->ob_pd));
                    return;
                }
                s3 = signal_newlike(s1);
                dsp_add_plus(s1->s_vec, s2->s_vec, s3->s_vec, s1->s_n);
                uin->i_signal = s3;
                s3->s_refcount = 1;
                if (!s1->s_refcount) signal_makereusable(s1);
                if (!s2->s_refcount) signal_makereusable(s2);
            }
            else uin->i_signal = s1;
            uin->i_ngot++;
                /* if we didn't fill this inlet don't bother yet */
            if (uin->i_ngot < uin->i_nconnect)
                goto notyet;
                /* if there's more than one, check them all */
            if (u2->u_nin > 1)
            {
                for (uin = u2->u_in, n = u2->u_nin; n--; uin++)
                    if (uin->i_ngot < uin->i_nconnect) goto notyet;
            }
                /* so now we can schedule the ugen.  */
            ugen_doit(dc, u2);
        notyet: ;
        }
    }
    t_freebytes(insig,(u->u_nin + u->u_nout) * sizeof(t_signal *));
    u->u_done = 1;
}

    /* once the DSP graph is built, we call this routine to sort it.
    This routine also deletes the graph; later we might want to leave the
    graph around, in case the user is editing the DSP network, to save having
    to recreate it all the time.  But not today.  */

void ugen_done_graph(t_dspcontext *dc)
{
    t_ugenbox *u, *u2;
    t_sigoutlet *uout;
    t_siginlet *uin;
    t_sigoutconnect *oc, *oc2;
    int i, n;
    t_block *blk;
    t_dspcontext *parent_context = dc->dc_parentcontext;
    t_float parent_srate;
    int parent_vecsize;
    int period, frequency, phase, vecsize, calcsize;
    t_float srate;
    int chainblockbegin;    /* DSP chain onset before block prolog code */
    int chainblockend;      /* and after block epilog code */
    int chainafterall;      /* and after signal outlet epilog */
    int reblock = 0, switched;
    int downsample = 1, upsample = 1;
    /* debugging printout */

    if (ugen_loud)
    {
        post("ugen_done_graph...");
        for (u = dc->dc_ugenlist; u; u = u->u_next)
        {
            post("ugen: %s", class_getname(u->u_obj->ob_pd));
            for (uout = u->u_out, i = 0; i < u->u_nout; uout++, i++)
                for (oc = uout->o_connections; oc; oc = oc->oc_next)
            {
                post("... out %d to %s, index %d, inlet %d", i,
                    class_getname(oc->oc_who->u_obj->ob_pd),
                        ugen_index(dc, oc->oc_who), oc->oc_inno);
            }
        }
    }

        /* search for an object of class "block~" */
    for (u = dc->dc_ugenlist, blk = 0; u; u = u->u_next)
    {
        t_pd *zz = &u->u_obj->ob_pd;
        if (pd_class(zz) == block_class)
        {
            if (blk)
                pd_error(blk, "conflicting block~ objects in same page");
            else blk = (t_block *)zz;
        }
    }

        /* figure out block size, calling frequency, sample rate */
    if (parent_context)
    {
        parent_srate = parent_context->dc_srate;
        parent_vecsize = parent_context->dc_vecsize;
    }
    else
    {
        parent_srate = sys_getsr();
        parent_vecsize = sys_getblksize();
    }
    if (blk)
    {
        int realoverlap;
        vecsize = blk->x_vecsize;
        if (vecsize == 0)
            vecsize = parent_vecsize;
        calcsize = blk->x_calcsize;
        if (calcsize == 0)
            calcsize = vecsize;
        realoverlap = blk->x_overlap;
        if (realoverlap > vecsize) realoverlap = vecsize;
        downsample = blk->x_downsample;
        upsample   = blk->x_upsample;
        if (downsample > parent_vecsize)
            downsample = parent_vecsize;
        period = (vecsize * downsample)/
            (parent_vecsize * realoverlap * upsample);
        frequency = (parent_vecsize * realoverlap * upsample)/
            (vecsize * downsample);
        phase = blk->x_phase;
        srate = parent_srate * realoverlap * upsample / downsample;
        if (period < 1) period = 1;
        if (frequency < 1) frequency = 1;
        blk->x_frequency = frequency;
        blk->x_period = period;
        blk->x_phase = dsp_phase & (period - 1);
        if (! parent_context || (realoverlap != 1) ||
            (vecsize != parent_vecsize) ||
                (downsample != 1) || (upsample != 1))
                    reblock = 1;
        switched = blk->x_switched;
    }
    else
    {
        srate = parent_srate;
        vecsize = parent_vecsize;
        calcsize = (parent_context ? parent_context->dc_calcsize : vecsize);
        downsample = upsample = 1;
        period = frequency = 1;
        phase = 0;
        if (!parent_context) reblock = 1;
        switched = 0;
    }
    dc->dc_reblock = reblock;
    dc->dc_switched = switched;
    dc->dc_srate = srate;
    dc->dc_vecsize = vecsize;
    dc->dc_calcsize = calcsize;

        /* if we're reblocking or switched, we now have to create output
        signals to fill in for the "borrowed" ones we have now.  This
        is also possibly true even if we're not blocked/switched, in
        the case that there was a signal loop.  But we don't know this
        yet.  */

    if (dc->dc_iosigs && (switched || reblock))
    {
        t_signal **sigp;
        for (i = 0, sigp = dc->dc_iosigs + dc->dc_ninlets; i < dc->dc_noutlets;
            i++, sigp++)
        {
            if ((*sigp)->s_isborrowed && !(*sigp)->s_borrowedfrom)
            {
                signal_setborrowed(*sigp,
                    signal_new(parent_vecsize, parent_srate));
                (*sigp)->s_refcount++;

                if (ugen_loud) post("set %lx->%lx", *sigp,
                    (*sigp)->s_borrowedfrom);
            }
        }
    }

    if (ugen_loud)
        post("reblock %d, switched %d", reblock, switched);

        /* schedule prologs for inlets and outlets.  If the "reblock" flag
        is set, an inlet will put code on the DSP chain to copy its input
        into an internal buffer here, before any unit generators' DSP code
        gets scheduled.  If we don't "reblock", inlets will need to get
        pointers to their corresponding inlets/outlets on the box we're inside,
        if any.  Outlets will also need pointers, unless we're switched, in
        which case outlet epilog code will kick in. */

    for (u = dc->dc_ugenlist; u; u = u->u_next)
    {
        t_pd *zz = &u->u_obj->ob_pd;
        t_signal **insigs = dc->dc_iosigs, **outsigs = dc->dc_iosigs;
        if (outsigs) outsigs += dc->dc_ninlets;

        if (pd_class(zz) == vinlet_class)
            vinlet_dspprolog((struct _vinlet *)zz,
                dc->dc_iosigs, vecsize, calcsize, dsp_phase, period, frequency,
                    downsample, upsample, reblock, switched);
        else if (pd_class(zz) == voutlet_class)
            voutlet_dspprolog((struct _voutlet *)zz,
                outsigs, vecsize, calcsize, dsp_phase, period, frequency,
                    downsample, upsample, reblock, switched);
    }
    chainblockbegin = pd_this->pd_dspchainsize;

    if (blk && (reblock || switched))   /* add the block DSP prolog */
    {
        dsp_add(block_prolog, 1, blk);
        blk->x_chainonset = pd_this->pd_dspchainsize - 1;
    }
        /* Initialize for sorting */
    for (u = dc->dc_ugenlist; u; u = u->u_next)
    {
        u->u_done = 0;
        for (uout = u->u_out, i = u->u_nout; i--; uout++)
            uout->o_nsent = 0;
        for (uin = u->u_in, i = u->u_nin; i--; uin++)
            uin->i_ngot = 0, uin->i_signal = 0;
   }

        /* Do the sort */

    for (u = dc->dc_ugenlist; u; u = u->u_next)
    {
            /* check that we have no connected signal inlets */
        if (u->u_done) continue;
        for (uin = u->u_in, i = u->u_nin; i--; uin++)
            if (uin->i_nconnect) goto next;

        ugen_doit(dc, u);
    next: ;
    }

        /* check for a DSP loop, which is evidenced here by the presence
        of ugens not yet scheduled. */

    for (u = dc->dc_ugenlist; u; u = u->u_next)
        if (!u->u_done)
    {
        t_signal **sigp;
        pd_error(u->u_obj,
            "DSP loop detected (some tilde objects not scheduled)");
                /* this might imply that we have unfilled "borrowed" outputs
                which we'd better fill in now. */
        for (i = 0, sigp = dc->dc_iosigs + dc->dc_ninlets; i < dc->dc_noutlets;
            i++, sigp++)
        {
            if ((*sigp)->s_isborrowed && !(*sigp)->s_borrowedfrom)
            {
                t_signal *s3 = signal_new(parent_vecsize, parent_srate);
                signal_setborrowed(*sigp, s3);
                (*sigp)->s_refcount++;
                dsp_add_zero(s3->s_vec, s3->s_n);
                if (ugen_loud)
                    post("oops, belatedly set %lx->%lx", *sigp,
                        (*sigp)->s_borrowedfrom);
            }
        }
        break;   /* don't need to keep looking. */
    }

    if (blk && (reblock || switched))    /* add block DSP epilog */
        dsp_add(block_epilog, 1, blk);
    chainblockend = pd_this->pd_dspchainsize;

        /* add epilogs for outlets.  */

    for (u = dc->dc_ugenlist; u; u = u->u_next)
    {
        t_pd *zz = &u->u_obj->ob_pd;
        if (pd_class(zz) == voutlet_class)
        {
            t_signal **iosigs = dc->dc_iosigs;
            if (iosigs) iosigs += dc->dc_ninlets;
            voutlet_dspepilog((struct _voutlet *)zz,
                iosigs, vecsize, calcsize, dsp_phase, period, frequency,
                    downsample, upsample, reblock, switched);
        }
    }

    chainafterall = pd_this->pd_dspchainsize;
    if (blk)
    {
        blk->x_blocklength = chainblockend - chainblockbegin;
        blk->x_epiloglength = chainafterall - chainblockend;
        blk->x_reblock = reblock;
    }

    if (ugen_loud)
    {
        t_int *ip;
        if (!dc->dc_parentcontext)
            for (i = pd_this->pd_dspchainsize, ip = pd_this->pd_dspchain;
                i--; ip++)
                    post("chain %lx", *ip);
        post("... ugen_done_graph done.");
    }
        /* now delete everything. */
    while (dc->dc_ugenlist)
    {
        for (uout = dc->dc_ugenlist->u_out, n = dc->dc_ugenlist->u_nout;
            n--; uout++)
        {
            oc = uout->o_connections;
            while (oc)
            {
                oc2 = oc->oc_next;
                freebytes(oc, sizeof *oc);
                oc = oc2;
            }
        }
        freebytes(dc->dc_ugenlist->u_out, dc->dc_ugenlist->u_nout *
            sizeof (*dc->dc_ugenlist->u_out));
        freebytes(dc->dc_ugenlist->u_in, dc->dc_ugenlist->u_nin *
            sizeof(*dc->dc_ugenlist->u_in));
        u = dc->dc_ugenlist;
        dc->dc_ugenlist = u->u_next;
        freebytes(u, sizeof *u);
    }
    if (ugen_currentcontext == dc)
        ugen_currentcontext = dc->dc_parentcontext;
    else bug("ugen_currentcontext");
    freebytes(dc, sizeof(*dc));

}

t_signal *ugen_getiosig(int index, int inout)
{
    if (!ugen_currentcontext) bug("ugen_getiosig");
    if (ugen_currentcontext->dc_toplevel) return (0);
    if (inout) index += ugen_currentcontext->dc_ninlets;
    return (ugen_currentcontext->dc_iosigs[index]);
}

/* ------------------------ samplerate~~ -------------------------- */

static t_class *samplerate_tilde_class;

typedef struct _samplerate
{
    t_object x_obj;
    t_float x_sr;
    t_canvas *x_canvas;
} t_samplerate;

void *canvas_getblock(t_class *blockclass, t_canvas **canvasp);

static void samplerate_tilde_bang(t_samplerate *x)
{
    t_float srate = sys_getsr();
    t_canvas *canvas = x->x_canvas;
    while (canvas)
    {
        t_block *b = (t_block *)canvas_getblock(block_class, &canvas);
        if (b)
            srate *= (t_float)(b->x_upsample) / (t_float)(b->x_downsample);
    }
    outlet_float(x->x_obj.ob_outlet, srate);
}

static void *samplerate_tilde_new(t_symbol *s)
{
    t_samplerate *x = (t_samplerate *)pd_new(samplerate_tilde_class);
    outlet_new(&x->x_obj, &s_float);
    x->x_canvas = canvas_getcurrent();
    return (x);
}

static void samplerate_tilde_setup(void)
{
    samplerate_tilde_class = class_new(gensym("samplerate~"),
        (t_newmethod)samplerate_tilde_new, 0, sizeof(t_samplerate), 0, 0);
    class_addbang(samplerate_tilde_class, samplerate_tilde_bang);
}

/* -------------------- setup routine -------------------------- */

void d_ugen_setup(void)
{
    block_tilde_setup();
    samplerate_tilde_setup();
}

