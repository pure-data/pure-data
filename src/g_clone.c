#include "m_pd.h"
#include "g_canvas.h"
#include "m_imp.h"
#include <string.h>

/* ---------- clone - maintain copies of a patch ----------------- */

#include "m_private_utils.h"
#define LIST_NGETBYTE 100 /* bigger that this we use alloc, not alloca */

t_class *clone_class;
static t_class *clone_in_class, *clone_out_class;

typedef struct _outproxy
{
    t_class *o_pd;
    t_outlet *o_outlet;
    int o_n;
} t_outproxy;

typedef struct _copy
{
    t_glist *c_gl;
    t_outproxy *c_vec;
} t_copy;

typedef struct _in
{
    t_class *i_pd;
    struct _clone *i_owner;
    int i_signal;
    int i_n;
} t_in;

typedef struct _out
{
    t_outlet *o_outlet;
    int o_signal;
} t_out;


typedef struct _clone
{
    t_object x_obj;
    t_canvas *x_canvas; /* owning canvas */
    int x_n;            /* number of copies */
    t_copy *x_vec;      /* the copies */
    int x_nin;
    t_in *x_invec;      /* inlet proxies */
    int x_nout;
    t_out *x_outvec;    /* outlets */
    t_symbol *x_s;      /* name of abstraction */
    int x_argc;         /* creation arguments for abstractions */
    t_atom *x_argv;
    int x_phase;        /* phase for round-robin input message forwarding */
    int x_startvoice;   /* number of first voice, 0 by default */
    unsigned int x_suppressvoice:1; /* suppress voice number as $1 arg */
    unsigned int x_distributein:1;  /* distribute input signals across clones */
    unsigned int x_packout:1;       /* pack output signals */
} t_clone;

int clone_match(t_pd *z, t_symbol *name, t_symbol *dir)
{
    t_clone *x = (t_clone *)z;
    if (!x->x_n)
        return (0);
    return (x->x_vec[0].c_gl->gl_name == name &&
        canvas_getdir(x->x_vec[0].c_gl) == dir);
}

void obj_sendinlet(t_object *x, int n, t_symbol *s, int argc, t_atom *argv);

static void clone_in_list(t_in *x, t_symbol *s, int argc, t_atom *argv)
{
    int n;
    if (!x->i_owner->x_nin)
        return;
    if (argc < 1 || argv[0].a_type != A_FLOAT)
        pd_error(x->i_owner, "clone: no instance number in message");
    else if ((n = argv[0].a_w.w_float - x->i_owner->x_startvoice) < 0 ||
        n >= x->i_owner->x_n)
            pd_error(x->i_owner, "clone: instance number %d out of range",
                n + x->i_owner->x_startvoice);
    else if (argc > 1 && argv[1].a_type == A_SYMBOL)
        obj_sendinlet(&x->i_owner->x_vec[n].c_gl->gl_obj, x->i_n,
            argv[1].a_w.w_symbol, argc-2, argv+2);
    else obj_sendinlet(&x->i_owner->x_vec[n].c_gl->gl_obj, x->i_n,
            &s_list, argc-1, argv+1);
}

static void clone_in_this(t_in *x, t_symbol *s, int argc, t_atom *argv)
{
    int phase = x->i_owner->x_phase;
    if (phase < 0 || phase >= x->i_owner->x_n)
        phase = 0;
    if (argc <= 0 || !x->i_owner->x_nin)
        return;
    if (argv->a_type == A_SYMBOL)
        obj_sendinlet(&x->i_owner->x_vec[phase].c_gl->gl_obj, x->i_n,
            argv[0].a_w.w_symbol, argc-1, argv+1);
    else obj_sendinlet(&x->i_owner->x_vec[phase].c_gl->gl_obj, x->i_n,
            &s_list, argc, argv);
}

static void clone_in_next(t_in *x, t_symbol *s, int argc, t_atom *argv)
{
    int phase = x->i_owner->x_phase + 1;
    if (phase < 0 || phase >= x->i_owner->x_n)
        phase = 0;
    x->i_owner->x_phase = phase;
    clone_in_this(x, s, argc, argv);
}

static void clone_in_set(t_in *x, t_floatarg f)
{
    int phase = f;
    if (phase < 0 || phase >= x->i_owner->x_n)
        phase = 0;
    x->i_owner->x_phase = phase;
}

static void clone_in_all(t_in *x, t_symbol *s, int argc, t_atom *argv)
{
    int phasewas = x->i_owner->x_phase, i;
    for (i = 0; i < x->i_owner->x_n; i++)
    {
        x->i_owner->x_phase = i;
        clone_in_this(x, s, argc, argv);
    }
    x->i_owner->x_phase = phasewas;
}

static void clone_in_vis(t_in *x, t_floatarg fn, t_floatarg vis)
{
    int n = fn - x->i_owner->x_startvoice;
    if (n < 0)
        n = 0;
    else if (n >= x->i_owner->x_n)
        n = x->i_owner->x_n - 1;
    canvas_vis(x->i_owner->x_vec[n].c_gl, (vis != 0));
}

static void clone_in_fwd(t_in *x, t_symbol *s, int argc, t_atom *argv)
{
    if (argc > 0 && argv->a_type == A_SYMBOL)
        typedmess(&x->i_pd, argv->a_w.w_symbol, argc-1, argv+1);
}

static void clone_setn(t_clone *, t_floatarg);

static void clone_in_resize(t_in *x, t_floatarg f)
{
    int i, oldn = x->i_owner->x_n;
        /* We need to send closebangs to old instances. Currently,
        this is done in clone_freeinstance(), but later we would
        rather do it here, see comment in clone_loadbang() */
    canvas_setcurrent(x->i_owner->x_canvas);
    clone_setn(x->i_owner, f);
    canvas_unsetcurrent(x->i_owner->x_canvas);
        /* send loadbangs to new instances */
    for (i = oldn; i < x->i_owner->x_n; i++)
        canvas_loadbang(x->i_owner->x_vec[i].c_gl);
}

static void clone_out_anything(t_outproxy *x, t_symbol *s, int argc, t_atom *argv)
{
    t_atom *outv;
    int first =
        1 + (s != &s_list && s != &s_float && s != &s_symbol && s != &s_bang),
            outc = argc + first;
    ALLOCA(t_atom, outv, outc, LIST_NGETBYTE);
    SETFLOAT(outv, x->o_n);
    if (first == 2)
        SETSYMBOL(outv + 1, s);
    memcpy(outv+first, argv, sizeof(t_atom) * argc);
    outlet_list(x->o_outlet, 0, outc, outv);
    FREEA(t_atom, outv, outc, LIST_NGETBYTE);
}

static PERTHREAD int clone_voicetovis = -1;

static t_canvas *clone_makeone(t_symbol *s, int argc, t_atom *argv)
{
    t_canvas *retval;
    pd_this->pd_newest = 0;
    typedmess(&pd_objectmaker, s, argc, argv);
    if (pd_this->pd_newest == 0)
    {
        pd_error(0, "clone: can't create subpatch '%s'",
            s->s_name);
        return (0);
    }
    if (*pd_this->pd_newest != canvas_class)
    {
        pd_error(0, "clone: can't clone '%s' because it's not an abstraction",
            s->s_name);
        pd_free(pd_this->pd_newest);
        pd_this->pd_newest = 0;
        return (0);
    }
    retval = (t_canvas *)pd_this->pd_newest;
    pd_this->pd_newest = 0;
    retval->gl_isclone = 1;
    return (retval);
}

static void clone_initinstance(t_clone *x, int which, t_canvas *c)
{
    t_outproxy *outvec;
    int i;
    x->x_vec[which].c_gl = c;
    x->x_vec[which].c_vec = outvec =
        (t_outproxy *)getbytes(x->x_nout * sizeof(*outvec));
    for (i = 0; i < x->x_nout; i++)
    {
        outvec[i].o_pd = clone_out_class;
        outvec[i].o_n = x->x_startvoice + which;
        outvec[i].o_outlet = x->x_outvec[i].o_outlet;
        obj_connect(&x->x_vec[which].c_gl->gl_obj, i,
            (t_object *)(&outvec[i]), 0);
    }
}

static void clone_freeinstance(t_clone *x, int which)
{
    t_copy *c = &x->x_vec[which];
        /* see comment in clone_loadbang() */
    canvas_closebang(c->c_gl);
    pd_free(&c->c_gl->gl_pd);
    t_freebytes(c->c_vec, x->x_nout * sizeof(*c->c_vec));
}

static void clone_setn(t_clone *x, t_floatarg f)
{
    int dspstate = canvas_suspend_dsp();
    int nwas = x->x_n, wantn = f, i, j;
    if (!nwas)
    {
        pd_error(x, "clone: no abstraction");
        return;
    }
    if (wantn < 1)
    {
        pd_error(x, "clone: can't resize to zero or negative number; setting to 1");
        wantn = 1;
    }
    if (wantn > nwas)
        for (i = nwas; i < wantn; i++)
    {
        t_canvas *c;
        SETFLOAT(x->x_argv, x->x_startvoice + i);
        if (!(c = clone_makeone(x->x_s, x->x_argc - x->x_suppressvoice,
            x->x_argv + x->x_suppressvoice)))
        {
            pd_error(x, "clone: couldn't create '%s'", x->x_s->s_name);
            goto done;
        }
        x->x_vec = (t_copy *)t_resizebytes(x->x_vec, i * sizeof(t_copy),
            (i+1) * sizeof(t_copy));
        x->x_n++;
        clone_initinstance(x, i, c);
    }
    else if (wantn < nwas)
    {
        for (i = wantn; i < nwas; i++)
            clone_freeinstance(x, i);
        x->x_vec = (t_copy *)t_resizebytes(x->x_vec, nwas * sizeof(t_copy),
            wantn * sizeof(t_copy));
        x->x_n = wantn;
    }
done:
    canvas_resume_dsp(dspstate);
}

static void clone_click(t_clone *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    if (!x->x_n)
        return;
    canvas_vis(x->x_vec[0].c_gl, 1);
}

static void clone_loadbang(t_clone *x, t_floatarg f)
{
    int i;
    if (f == LB_LOAD)
        for (i = 0; i < x->x_n; i++)
            canvas_loadbang(x->x_vec[i].c_gl);
#if 0
        /* Pd currently does not send closebangs to objects on a root canvas
        or when an object is retexted. There is a pending PR to fix this, but
        in the meantime we just send the closebang in clone_freeinstance(). */
    else if (f == LB_CLOSE)
        for (i = 0; i < x->x_n; i++)
            canvas_closebang(x->x_vec[i].c_gl);
#endif
}

void canvas_dodsp(t_canvas *x, int toplevel, t_signal **sp);
t_signal *signal_newfromcontext(int borrowed, int nchans);
void signal_makereusable(t_signal *sig);

static void clone_dsp(t_clone *x, t_signal **sp)
{
    int i, j, nin, nout, *noutchans;
    t_signal **tempio;
    if (!x->x_n)
        return;
    for (i = nin = 0; i < x->x_nin; i++)
        if (x->x_invec[i].i_signal)
            nin++;
    for (i = nout = 0; i < x->x_nout; i++)
        if (x->x_outvec[i].o_signal)
            nout++;
    for (j = 0; j < x->x_n; j++)
    {
        if (obj_ninlets(&x->x_vec[j].c_gl->gl_obj) != x->x_nin ||
            obj_noutlets(&x->x_vec[j].c_gl->gl_obj) != x->x_nout ||
                obj_nsiginlets(&x->x_vec[j].c_gl->gl_obj) != nin ||
                    obj_nsigoutlets(&x->x_vec[j].c_gl->gl_obj) != nout)
        {
            pd_error(x, "clone: can't do DSP until edited copy is saved");
            for (i = 0; i < nout; i++)
            {
                    /* create dummy output signals */
                signal_setmultiout(&sp[nin+i], x->x_packout ? x->x_n : 1);
                dsp_add_zero(sp[nin+i]->s_vec,
                    sp[nin+i]->s_length * sp[nin+i]->s_nchans);
            }
            return;
        }
    }
    tempio = (nin + nout) > 0 ?
        (t_signal **)alloca((nin + nout) * sizeof(*tempio)) : 0;
    noutchans = nout > 0 ? (int *)alloca(nout * sizeof(*noutchans)) : 0;
        /* load input signals into signal vector to send subpatches */
    if (x->x_packout)   /* pack individual mono outputs to a multichannel one */
    {
        for (j = 0; j < x->x_n; j++)
        {
            for (i = 0; i < nin; i++)
            {
                if (x->x_distributein)
                {
                        /* distribute multi-channel signal over instances;
                        wrap around if channel count is lower than instance
                        count */
                    int offset = j % sp[i]->s_nchans;
                    tempio[i] = signal_new(0, 1, sp[i]->s_sr, 0);
                    signal_setborrowed(tempio[i], sp[i]);
                    tempio[i]->s_nchans = 1;
                    tempio[i]->s_vec = sp[i]->s_vec + offset * sp[i]->s_length;
                    tempio[i]->s_refcount = 1;
                }
                else
                    tempio[i] = sp[i];
            }
            for (i = 0; i < nout; i++)
                tempio[nin + i] = signal_newfromcontext(1, 1);
            canvas_dodsp(x->x_vec[j].c_gl, 0, tempio);
            if (x->x_distributein)
            {
                for (i = 0; i < nin; i++)
                {
                    if (!--tempio[i]->s_refcount)
                        signal_makereusable(tempio[i]);
                    else
                        bug("clone 1: %d", tempio[i]->s_refcount);
                }
            }
            for (i = 0; i < nout; i++)
            {
                int nchans = tempio[nin + i]->s_nchans;
                int length = tempio[nin + i]->s_length;
                t_sample *to, *from = tempio[nin + i]->s_vec;
                if (j == 0) /* now we can the create the output signal */
                {
                    signal_setmultiout(&sp[nin + i], nchans * x->x_n);
                    noutchans[i] = nchans;
                }
                    /* NB: it is possible for instances to have different
                    output channel counts. In this case we always take the
                    channel count of the first instance. */
                to = sp[nin + i]->s_vec + j * length * noutchans[i];
                if (nchans == noutchans[i])
                    dsp_add_copy(from, to, length * nchans);
                else
                {
                    if (nchans > noutchans[i]) /* ignore extra channels */
                        dsp_add_copy(from, to, noutchans[i] * length);
                    else /* fill missing channels with zeros */
                    {
                        dsp_add_copy(from, to, nchans * length);
                        dsp_add_zero(to + length * nchans,
                            length * (noutchans[i] - nchans));
                    }
                #if 1
                    pd_error(x, "warning: clone instance %d: channel count "
                        "of outlet %d (%d) does not match first instance (%d)",
                            j, i, nchans, noutchans[i]);
                #endif
                }
                signal_makereusable(tempio[nin + i]);
            }
        }
    }
    else    /* otherwise add the individual outputs */
    {
        for (j = 0; j < x->x_n; j++)
        {
            for (i = 0; i < nin; i++)
            {
                if (x->x_distributein)
                {
                        /* distribute multi-channel signal over instances;
                        wrap around if channel count is lower than instance count */
                    int offset = j % sp[i]->s_nchans;
                    tempio[i] = signal_new(0, 1, sp[i]->s_sr, 0);
                    signal_setborrowed(tempio[i], sp[i]);
                    tempio[i]->s_nchans = 1;
                    tempio[i]->s_vec = sp[i]->s_vec + offset * sp[i]->s_length;
                    tempio[i]->s_refcount = 1;
                }
                else
                    tempio[i] = sp[i];
            }
            for (i = 0; i < nout; i++)
                tempio[nin + i] = signal_newfromcontext(1, 1);
            canvas_dodsp(x->x_vec[j].c_gl, 0, tempio);
            if (x->x_distributein)
            {
                for (i = 0; i < nin; i++)
                {
                    if (!--tempio[i]->s_refcount)
                        signal_makereusable(tempio[i]);
                    else
                        bug("clone 2: %d", tempio[i]->s_refcount);
                }
            }
            for (i = 0; i < nout; i++)
            {
                int nchans = tempio[nin + i]->s_nchans;
                int length = tempio[nin + i]->s_length;
                if (j == 0)
                {
                        /* first instance: create output signal and copy content */
                    signal_setmultiout(&sp[nin + i], nchans);
                    dsp_add_copy(tempio[nin + i]->s_vec,
                        sp[nin + i]->s_vec, length * nchans);
                    noutchans[i] = nchans;
                }
                else /* add to existing signal */
                {
                    int nsamples = nchans > noutchans[i] ?
                        noutchans[i] * length : nchans * length;
                #if 1
                    if (nchans != noutchans[i])
                        pd_error(x, "warning: clone instance %d: channel count "
                            "of outlet %d (%d) does not match first instance (%d)",
                                j, i, nchans, noutchans[i]);
                #endif
                    dsp_add_plus(tempio[nin + i]->s_vec, sp[nin + i]->s_vec,
                        sp[nin + i]->s_vec, nsamples);
                }
                signal_makereusable(tempio[nin + i]);
            }
        }
    }
    for (i = 0; i < nin; i++)
    {
        if (sp[i]->s_refcount <= 0)
            bug("clone 3 %d", sp[i]->s_refcount);
    }
}

static void *clone_new(t_symbol *s, int argc, t_atom *argv)
{
    t_clone *x = (t_clone *)pd_new(clone_class);
    t_canvas *c;
    int wantn, dspstate, i, voicetovis = clone_voicetovis;
    x->x_canvas = canvas_getcurrent();
    x->x_invec = 0;
    x->x_outvec = 0;
    x->x_argv = 0;
    x->x_startvoice = 0;
    x->x_suppressvoice = 0;
    x->x_distributein = 0;
    x->x_packout = 0;
    clone_voicetovis = -1;
    if (argc == 0)
    {
        x->x_vec = 0;
        x->x_n = 0;
        return (x);
    }
    dspstate = canvas_suspend_dsp();
    while (argc > 0 && argv[0].a_type == A_SYMBOL &&
        argv[0].a_w.w_symbol->s_name[0] == '-')
    {
        if (!strcmp(argv[0].a_w.w_symbol->s_name, "-s") && argc > 1 &&
            argv[1].a_type == A_FLOAT)
        {
            x->x_startvoice = argv[1].a_w.w_float;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(argv[0].a_w.w_symbol->s_name, "-x"))
            x->x_suppressvoice = 1, argc--, argv++;
        else if (!strcmp(argv[0].a_w.w_symbol->s_name, "-d"))
            x->x_distributein = x->x_packout = 1, argc--, argv++;
        else if (!strcmp(argv[0].a_w.w_symbol->s_name, "-di"))
            x->x_distributein = 1, argc--, argv++;
        else if (!strcmp(argv[0].a_w.w_symbol->s_name, "-do"))
            x->x_packout = 1, argc--, argv++;
        else goto usage;
    }
    if (argc >= 2 && (wantn = atom_getfloatarg(0, argc, argv)) >= 0
        && argv[1].a_type == A_SYMBOL)
            x->x_s = argv[1].a_w.w_symbol;
    else if (argc >= 2 && (wantn = atom_getfloatarg(1, argc, argv)) >= 0
        && argv[0].a_type == A_SYMBOL)
            x->x_s = argv[0].a_w.w_symbol;
    else goto usage;
        /* store a copy of the argmuents with an extra space (argc+1) for
        supplying an instance number, which we'll bash as we go. */
    x->x_argc = argc - 1;
    x->x_argv = getbytes(x->x_argc * sizeof(*x->x_argv));
    memcpy(x->x_argv, argv+1, x->x_argc * sizeof(*x->x_argv));
    SETFLOAT(x->x_argv, x->x_startvoice);
    if (!(c = clone_makeone(x->x_s, x->x_argc - x->x_suppressvoice,
        x->x_argv + x->x_suppressvoice)))
            goto fail;
        /* inlets */
    x->x_nin = obj_ninlets(&c->gl_obj);
    if (x->x_nin > 0)
    {
        x->x_invec = (t_in *)getbytes(x->x_nin * sizeof(*x->x_invec));
        for (i = 0; i < x->x_nin; i++)
        {
            x->x_invec[i].i_pd = clone_in_class;
            x->x_invec[i].i_owner = x;
            x->x_invec[i].i_signal =
                obj_issignalinlet(&c->gl_obj, i);
            x->x_invec[i].i_n = i;
            if (x->x_invec[i].i_signal)
                inlet_new(&x->x_obj, &x->x_invec[i].i_pd,
                    &s_signal, &s_signal);
            else inlet_new(&x->x_obj, &x->x_invec[i].i_pd, 0, 0);
        }
    }
    else /* fake inlet to send messages like "vis" or "resize" */
    {
        x->x_invec = (t_in *)getbytes(sizeof(*x->x_invec));
        x->x_invec->i_pd = clone_in_class;
        x->x_invec->i_owner = x;
        x->x_invec->i_signal = 0;
        x->x_invec->i_n = 0;
        inlet_new(&x->x_obj, &x->x_invec->i_pd, 0, 0);
    }
        /* outlets */
    x->x_nout = obj_noutlets(&c->gl_obj);
    x->x_outvec = (t_out *)getbytes(x->x_nout * sizeof(*x->x_outvec));
    for (i = 0; i < x->x_nout; i++)
    {
        x->x_outvec[i].o_signal = obj_issignaloutlet(&c->gl_obj, i);
        x->x_outvec[i].o_outlet =
            outlet_new(&x->x_obj, (x->x_outvec[i].o_signal ? &s_signal : 0));
    }
        /* first copy */
    x->x_vec = getbytes(sizeof(*x->x_vec));
    x->x_n = 1;
    clone_initinstance(x, 0, c);
        /* remaining copies */
    clone_setn(x, (t_floatarg)(wantn));
    x->x_phase = wantn-1;
    canvas_resume_dsp(dspstate);
    if (voicetovis >= 0 && voicetovis < x->x_n)
        canvas_vis(x->x_vec[voicetovis].c_gl, 1);
    return (x);
usage:
    pd_error(0, "usage: clone [-s starting-number] <number> <name> [arguments]");
fail:
    if (x->x_argv)
        freebytes(x->x_argv, sizeof(x->x_argc * sizeof(*x->x_argv)));
    freebytes(x, sizeof(t_clone));
    canvas_resume_dsp(dspstate);
    return (0);
}

static void clone_free(t_clone *x)
{
    if (x->x_vec)
    {
        int i, voicetovis = -1;
        if (THISGUI->i_reloadingabstraction)
        {
            for (i = 0; i < x->x_n; i++)
                if (x->x_vec[i].c_gl == THISGUI->i_reloadingabstraction)
                    voicetovis = i;
        }
        for (i = 0; i < x->x_n; i++)
            clone_freeinstance(x, i);
        t_freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
        t_freebytes(x->x_argv, x->x_argc * sizeof(*x->x_argv));
        if (x->x_nin)
            t_freebytes(x->x_invec, x->x_nin * sizeof(*x->x_invec));
        else /* fake inlet */
            t_freebytes(x->x_invec, sizeof(*x->x_invec));
        t_freebytes(x->x_outvec, x->x_nout * sizeof(*x->x_outvec));
        clone_voicetovis = voicetovis;
    }
}

void clone_setup(void)
{
    clone_class = class_new(gensym("clone"), (t_newmethod)clone_new,
        (t_method)clone_free, sizeof(t_clone),
            CLASS_NOINLET | CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(clone_class, (t_method)clone_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(clone_class, (t_method)clone_loadbang, gensym("loadbang"),
        A_FLOAT, 0);
    class_addmethod(clone_class, (t_method)clone_dsp,
        gensym("dsp"), A_CANT, 0);

    clone_in_class = class_new(gensym("clone-inlet"), 0, 0,
        sizeof(t_in), CLASS_PD, 0);
    class_addmethod(clone_in_class, (t_method)clone_in_next, gensym("next"),
        A_GIMME, 0);
    class_addmethod(clone_in_class, (t_method)clone_in_this, gensym("this"),
        A_GIMME, 0);
    class_addmethod(clone_in_class, (t_method)clone_in_set, gensym("set"),
        A_FLOAT, 0);
    class_addmethod(clone_in_class, (t_method)clone_in_all, gensym("all"),
        A_GIMME, 0);
    class_addmethod(clone_in_class, (t_method)clone_in_vis, gensym("vis"),
        A_FLOAT, A_FLOAT, 0);
    class_addmethod(clone_in_class, (t_method)clone_in_fwd, gensym("fwd"),
        A_GIMME, 0);
    class_addmethod(clone_in_class, (t_method)clone_in_resize, gensym("resize"),
        A_FLOAT, 0);
    class_addlist(clone_in_class, (t_method)clone_in_list);

    clone_out_class = class_new(gensym("clone-outlet"), 0, 0,
        sizeof(t_in), CLASS_PD, 0);
    class_addanything(clone_out_class, (t_method)clone_out_anything);
}

    /* for the needs of g_editor::glist_dofinderror(): */

int clone_get_n(t_gobj *x)
{
    if (pd_class(&x->g_pd) != clone_class) return 0;
    else return ((t_clone *)x)->x_n;
}

t_glist *clone_get_instance(t_gobj *x, int n)
{
    t_clone *c;

    if (pd_class(&x->g_pd) != clone_class) return NULL;

    c = (t_clone *)x;
    n -= c->x_startvoice;
    if (n < 0)
        n = 0;
    else if (n >= c->x_n)
        n = c->x_n - 1;
    return  c->x_vec[n].c_gl;
}

