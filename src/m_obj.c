/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* this file handles Max-style patchable objects, i.e., objects which
can interconnect via inlets and outlets; also, the (terse) generic
behavior for "gobjs" appears at the end of this file.  */

#include "m_pd.h"
#include "m_imp.h"
#include <string.h>

#ifdef _WIN32
# include <malloc.h> /* MSVC or mingw on windows */
#elif defined(__linux__) || defined(__APPLE__) || defined(HAVE_ALLOCA_H)
# include <alloca.h> /* linux, mac, mingw, cygwin */
#endif
#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

union inletunion
{
    t_symbol *iu_symto;
    t_gpointer *iu_pointerslot;
    t_float *iu_floatslot;
    t_symbol **iu_symslot;
    t_float iu_floatsignalvalue;
};

struct _inlet
{
    t_pd i_pd;
    struct _inlet *i_next;
    t_object *i_owner;
    t_pd *i_dest;
    t_symbol *i_symfrom;
    union inletunion i_un;
};

#define i_symto i_un.iu_symto
#define i_pointerslot i_un.iu_pointerslot
#define i_floatslot i_un.iu_floatslot
#define i_symslot i_un.iu_symslot

static t_class *inlet_class, *pointerinlet_class, *floatinlet_class,
    *symbolinlet_class;

#define ISINLET(pd) ((*(pd) == inlet_class) || \
    (*(pd) == pointerinlet_class) || \
    (*(pd) == floatinlet_class) || \
    (*(pd) == symbolinlet_class))

/* --------------------- generic inlets ala max ------------------ */

t_inlet *inlet_new(t_object *owner, t_pd *dest, t_symbol *s1, t_symbol *s2)
{
    t_inlet *x = (t_inlet *)pd_new(inlet_class), *y, *y2;
    x->i_owner = owner;
    x->i_dest = dest;
    if (s1 == &s_signal)
        x->i_un.iu_floatsignalvalue = 0;
    else x->i_symto = s2;
    x->i_symfrom = s1;
    x->i_next = 0;
    if ((y = owner->ob_inlet))
    {
        while ((y2 = y->i_next)) y = y2;
        y->i_next = x;
    }
    else owner->ob_inlet = x;
    return (x);
}

t_inlet *signalinlet_new(t_object *owner, t_float f)
{
    t_inlet *x = inlet_new(owner, &owner->ob_pd, &s_signal, &s_signal);
    x->i_un.iu_floatsignalvalue = f;
    return (x);
}

static void inlet_wrong(t_inlet *x, t_symbol *s)
{
    pd_error(x->i_owner, "inlet: expected '%s' but got '%s'",
        x->i_symfrom->s_name, s->s_name);
}

static void inlet_list(t_inlet *x, t_symbol *s, int argc, t_atom *argv);
extern t_class *vinlet_class;

    /* LATER figure out how to make these efficient: */
static void inlet_bang(t_inlet *x)
{
    if (x->i_symfrom == &s_bang)
        pd_vmess(x->i_dest, x->i_symto, "");
    else if (!x->i_symfrom) pd_bang(x->i_dest);
    else if (x->i_symfrom == &s_list)
        inlet_list(x, &s_bang, 0, 0);
    else if (x->i_symfrom == &s_signal && zgetfn(x->i_dest, gensym("fwd")))
        vmess(x->i_dest, gensym("fwd"), "s", &s_bang);
    else inlet_wrong(x, &s_bang);
}

static void inlet_pointer(t_inlet *x, t_gpointer *gp)
{
    if (x->i_symfrom == &s_pointer)
        pd_vmess(x->i_dest, x->i_symto, "p", gp);
    else if (!x->i_symfrom) pd_pointer(x->i_dest, gp);
    else if (x->i_symfrom == &s_list)
    {
        t_atom a;
        SETPOINTER(&a, gp);
        inlet_list(x, &s_pointer, 1, &a);
    }
    else inlet_wrong(x, &s_pointer);
}

static void inlet_float(t_inlet *x, t_float f)
{
    if (x->i_symfrom == &s_float)
        pd_vmess(x->i_dest, x->i_symto, "f", (t_floatarg)f);
    else if (x->i_symfrom == &s_signal)
        x->i_un.iu_floatsignalvalue = f;
    else if (!x->i_symfrom)
        pd_float(x->i_dest, f);
    else if (x->i_symfrom == &s_list)
    {
        t_atom a;
        SETFLOAT(&a, f);
        inlet_list(x, &s_float, 1, &a);
    }
    else inlet_wrong(x, &s_float);
}

static void inlet_symbol(t_inlet *x, t_symbol *s)
{
    if (x->i_symfrom == &s_symbol)
        pd_vmess(x->i_dest, x->i_symto, "s", s);
    else if (!x->i_symfrom) pd_symbol(x->i_dest, s);
    else if (x->i_symfrom == &s_list)
    {
        t_atom a;
        SETSYMBOL(&a, s);
        inlet_list(x, &s_symbol, 1, &a);
    }
    else if (x->i_symfrom == &s_signal && zgetfn(x->i_dest, gensym("fwd")))
        vmess(x->i_dest, gensym("fwd"), "ss", &s_symbol, s);
    else inlet_wrong(x, &s_symbol);
}

    /* forward a message to an inlet~ object */
static void inlet_fwd(t_inlet *x, t_symbol *s, int argc, t_atom *argv)
{
    t_atom *argvec = (t_atom *)alloca((argc+1) * sizeof(t_atom));
    int i;
    SETSYMBOL(argvec, s);
    for (i = 0; i < argc; i++)
        argvec[i+1] = argv[i];
    typedmess(x->i_dest, gensym("fwd"), argc+1, argvec);
}

static void inlet_list(t_inlet *x, t_symbol *s, int argc, t_atom *argv)
{
    t_atom at;
    if (x->i_symfrom == &s_list || x->i_symfrom == &s_float
        || x->i_symfrom == &s_symbol || x->i_symfrom == &s_pointer)
            typedmess(x->i_dest, x->i_symto, argc, argv);
    else if (!x->i_symfrom) pd_list(x->i_dest, s, argc, argv);
    else if (!argc)
      inlet_bang(x);
    else if (argc==1 && argv->a_type == A_FLOAT)
      inlet_float(x, atom_getfloat(argv));
    else if (argc==1 && argv->a_type == A_SYMBOL)
      inlet_symbol(x, atom_getsymbol(argv));
    else if (x->i_symfrom == &s_signal && zgetfn(x->i_dest, gensym("fwd")))
        inlet_fwd(x, &s_list, argc, argv);
    else post("class %s", class_getname(*x->i_dest)), inlet_wrong(x, &s_list);
}

static void inlet_anything(t_inlet *x, t_symbol *s, int argc, t_atom *argv)
{
    if (x->i_symfrom == s)
    {
        /* the "symto" field is undefined for signal inlets, so we don't
         attempt to translate the selector, just forward the original msg. */
        
        if (x->i_symfrom == &s_signal)
            typedmess(x->i_dest, s, argc, argv);
        else
            typedmess(x->i_dest, x->i_symto, argc, argv);
    }
    else if (!x->i_symfrom)
        typedmess(x->i_dest, s, argc, argv);
    else if (x->i_symfrom == &s_signal && zgetfn(x->i_dest, gensym("fwd")))
        inlet_fwd(x, s, argc, argv);
    else inlet_wrong(x, s);
}

void inlet_free(t_inlet *x)
{
    t_object *y = x->i_owner;
    t_inlet *x2;
    if (y->ob_inlet == x) y->ob_inlet = x->i_next;
    else for (x2 = y->ob_inlet; x2; x2 = x2->i_next)
        if (x2->i_next == x)
    {
        x2->i_next = x->i_next;
        break;
    }
    t_freebytes(x, sizeof(*x));
}

/* ----- pointerinlets, floatinlets, syminlets: optimized inlets ------- */

static void pointerinlet_pointer(t_inlet *x, t_gpointer *gp)
{
    gpointer_unset(x->i_pointerslot);
    *(x->i_pointerslot) = *gp;
    if (gp->gp_stub) gp->gp_stub->gs_refcount++;
}

t_inlet *pointerinlet_new(t_object *owner, t_gpointer *gp)
{
    t_inlet *x = (t_inlet *)pd_new(pointerinlet_class), *y, *y2;
    x->i_owner = owner;
    x->i_dest = 0;
    x->i_symfrom = &s_pointer;
    x->i_pointerslot = gp;
    x->i_next = 0;
    if ((y = owner->ob_inlet))
    {
        while ((y2 = y->i_next)) y = y2;
        y->i_next = x;
    }
    else owner->ob_inlet = x;
    return (x);
}

static void floatinlet_float(t_inlet *x, t_float f)
{
    *(x->i_floatslot) = f;
}

t_inlet *floatinlet_new(t_object *owner, t_float *fp)
{
    t_inlet *x = (t_inlet *)pd_new(floatinlet_class), *y, *y2;
    x->i_owner = owner;
    x->i_dest = 0;
    x->i_symfrom = &s_float;
    x->i_floatslot = fp;
    x->i_next = 0;
    if ((y = owner->ob_inlet))
    {
        while ((y2 = y->i_next)) y = y2;
        y->i_next = x;
    }
    else owner->ob_inlet = x;
    return (x);
}

static void symbolinlet_symbol(t_inlet *x, t_symbol *s)
{
    *(x->i_symslot) = s;
}

t_inlet *symbolinlet_new(t_object *owner, t_symbol **sp)
{
    t_inlet *x = (t_inlet *)pd_new(symbolinlet_class), *y, *y2;
    x->i_owner = owner;
    x->i_dest = 0;
    x->i_symfrom = &s_symbol;
    x->i_symslot = sp;
    x->i_next = 0;
    if ((y = owner->ob_inlet))
    {
        while ((y2 = y->i_next)) y = y2;
        y->i_next = x;
    }
    else owner->ob_inlet = x;
    return (x);
}

/* ---------------------- routine to handle lists ---------------------- */

    /* objects interpret lists by feeding them to the individual inlets.
    Before you call this check that the object doesn't have a more
    specific way to handle lists. */
void obj_list(t_object *x, t_symbol *s, int argc, t_atom *argv)
{
    t_atom *ap;
    int count;
    t_inlet *ip = ((t_object *)x)->ob_inlet;
    if (!argc)
    {
        pd_emptylist(&x->ob_pd);
        return;
    }
    for (count = argc-1, ap = argv+1; ip && count--; ap++, ip = ip->i_next)
    {
        if (ap->a_type == A_POINTER) pd_pointer(&ip->i_pd, ap->a_w.w_gpointer);
        else if (ap->a_type == A_FLOAT) pd_float(&ip->i_pd, ap->a_w.w_float);
        else pd_symbol(&ip->i_pd, ap->a_w.w_symbol);
    }
    if (argv->a_type == A_POINTER) pd_pointer(&x->ob_pd, argv->a_w.w_gpointer);
    else if (argv->a_type == A_FLOAT) pd_float(&x->ob_pd, argv->a_w.w_float);
    else pd_symbol(&x->ob_pd, argv->a_w.w_symbol);
}

/* --------------------------- outlets ------------------------------ */


struct _outconnect
{
    struct _outconnect *oc_next;
    t_pd *oc_to;
};

struct _outlet
{
    t_object *o_owner;
    struct _outlet *o_next;
    t_outconnect *o_connections;
    t_symbol *o_sym;
};

/* ------- backtracer - keep track of stack for backtracing  --------- */
#define NARGS 5
typedef struct _msgstack
{
    struct _backtracer *m_owner;
    t_symbol *m_sel;
    int m_argc;
    t_atom m_argv[NARGS];
    struct _msgstack *m_next;
} t_msgstack;

typedef struct _backtracer
{
    t_pd b_pd;
    t_outconnect *b_connections;
    t_pd *b_owner;
} t_backtracer;

static t_msgstack *backtracer_stack;
int backtracer_cantrace = 0;
int backtracer_tracing;
t_class *backtracer_class;

static PERTHREAD int stackcount = 0; /* iteration counter */
#define STACKITER 1000 /* maximum iterations allowed */

static PERTHREAD int outlet_eventno;

    /* initialize stack depth count on each incoming event that can set off
    messages so that  the outlet functions can check to prevent stack overflow]
    from message recursion.  Also count message initiations. */

void outlet_setstacklim(void)
{
    t_msgstack *m;
    while ((m = backtracer_stack))
        backtracer_stack = m->m_next; t_freebytes(m, sizeof (*m));
    stackcount = 0;
    outlet_eventno++;
}

    /* get a number unique to the (clock, MIDI, GUI, etc.) event we're on */
int sched_geteventno(void)
{
    return (outlet_eventno);
}

    /* get pointer to connection list for an outlet (for editing/traversing) */
static t_outconnect **outlet_getconnectionpointer(t_outlet *x)
{
    if (x->o_connections && *(x->o_connections->oc_to) == backtracer_class)
        return (&((t_backtracer *)(x->o_connections->oc_to))->b_connections);
    else return (&x->o_connections);
}

static void backtracer_printmsg(t_pd *who, t_symbol *s,
    int argc, t_atom *argv)
{
    char msgbuf[104];
    int nprint = (argc > NARGS ? NARGS : argc), nchar, i;
    snprintf(msgbuf, 100, "%s: %s ", class_getname(*who), s->s_name);
    nchar = strlen(msgbuf);
    for (i = 0; i < nprint && nchar < 100; i++)
        if (nchar < 100)
    {
        char buf[100];
        atom_string(&argv[i], buf, 100);
        snprintf(msgbuf + nchar, 100-nchar, " %s", buf);
        nchar = strlen(msgbuf);
    }
    if (argc > nprint && nchar < 100)
        sprintf(msgbuf + nchar, "...");
    else memcpy(msgbuf+100, "...", 4); /* in case we didn't finish */
    logpost(who, 2, "%s", msgbuf);
}

static void backtracer_anything(t_backtracer *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_msgstack *m = (t_msgstack *)t_getbytes(sizeof(t_msgstack));
    t_outconnect *oc;
    int ncopy = (argc > NARGS ? NARGS : argc), i;
    m->m_next = backtracer_stack;
    backtracer_stack = m;
    m->m_sel = s;
    m->m_argc = argc;
    for (i = 0; i < ncopy; i++)
        m->m_argv[i] = argv[i];
    m->m_owner = x;
    if (backtracer_tracing)
        backtracer_printmsg(x->b_owner, s, argc, argv);
    for (oc = x->b_connections; oc; oc = oc->oc_next)
        typedmess(oc->oc_to, s, argc, argv);
    backtracer_stack = m->m_next;
    t_freebytes(m, sizeof(*m));
}

t_backtracer *backtracer_new(t_pd *owner)
{
    t_backtracer *x = (t_backtracer *)pd_new(backtracer_class);
    x->b_connections = 0;
    x->b_owner = owner;
    return (x);
}

int backtracer_settracing(void *x, int tracing)
{
    if (tracing)
    {
        if (backtracer_tracing)
        {
            pd_error(x, "trace: already tracing");
            return (0);
        }
        else
        {
            backtracer_tracing = 1;
            return (1);
        }
    }
    else    /* when stopping, print backtrace to here */
    {
        t_msgstack *m = backtracer_stack;
        post("backtrace:");
        while (m)
        {
            backtracer_printmsg(m->m_owner->b_owner, m->m_sel,
                m->m_argc, m->m_argv);
            m = m->m_next;
        }
        backtracer_tracing = 0;
        return (0);
    }
}

void canvas_settracing(int onoff);
static t_clock *backtrace_unsetclock;

static void backtrace_dounsettracing(void *dummy)
{
    canvas_settracing(0);
    backtracer_cantrace = 0;
    clock_free(backtrace_unsetclock);
    backtrace_unsetclock = 0;
}

    /* globally turn tracing on and off. */
void glob_settracing(void *dummy, t_float f)
{
#ifndef PDINSTANCE  /* this won't work with pd instances so just don't */
    if (f != 0)
    {
        if (backtracer_cantrace)
            post("pd: tracing already enabled");
        else canvas_settracing(1);
        backtracer_cantrace = 1;
    }
    else
    {
        if (!backtracer_cantrace)
            post("pd: tracing already disabled");
        else if (!backtrace_unsetclock)
        {
            backtrace_unsetclock = clock_new(dummy,
                (t_method)backtrace_dounsettracing);
            clock_delay(backtrace_unsetclock, 0);
        }
    }
#endif
}

    /* this is called on every object, via canvas_settracing() call above */
void obj_dosettracing(t_object *ob, int onoff)
{
    t_outlet *o;
    for (o = ob->ob_outlet; o; o = o->o_next)
    {
        if (onoff)
        {
            t_backtracer *b = backtracer_new(&ob->ob_pd);
            b->b_connections = o->o_connections;
            o->o_connections =  (t_outconnect *)t_getbytes(sizeof(t_outconnect));
            o->o_connections->oc_next = 0;
            o->o_connections->oc_to = &b->b_pd;
        }
        else if (o->o_connections &&
            (*o->o_connections->oc_to == backtracer_class))
        {
            t_backtracer *b = (t_backtracer *)o->o_connections->oc_to;
            t_freebytes(o->o_connections, sizeof(*o->o_connections));
            o->o_connections = b->b_connections;
            t_freebytes(b, sizeof(*b));
        }
        else bug("obj_dosettracing");
    }
}

t_outlet *outlet_new(t_object *owner, t_symbol *s)
{
    t_outlet *x = (t_outlet *)getbytes(sizeof(*x)), *y, *y2;
    x->o_owner = owner;
    x->o_next = 0;
    if ((y = owner->ob_outlet))
    {
        while ((y2 = y->o_next)) y = y2;
        y->o_next = x;
    }
    else owner->ob_outlet = x;
    if (backtracer_cantrace)
    {
        t_backtracer *b = backtracer_new(&owner->ob_pd);
        x->o_connections =  (t_outconnect *)t_getbytes(sizeof(t_outconnect));
        x->o_connections->oc_next = 0;
        x->o_connections->oc_to = &b->b_pd;
    }
    else x->o_connections = 0;
    x->o_sym = s;
    return (x);
}

static void outlet_stackerror(t_outlet *x)
{
    pd_error(x->o_owner, "stack overflow");
}

void outlet_bang(t_outlet *x)
{
    t_outconnect *oc;
    if(++stackcount >= STACKITER)
        outlet_stackerror(x);
    else
    for (oc = x->o_connections; oc; oc = oc->oc_next)
        pd_bang(oc->oc_to);
    --stackcount;
}

void outlet_pointer(t_outlet *x, t_gpointer *gp)
{
    t_outconnect *oc;
    t_gpointer gpointer;
    if(++stackcount >= STACKITER)
        outlet_stackerror(x);
    else
    {
        gpointer = *gp;
        for (oc = x->o_connections; oc; oc = oc->oc_next)
            pd_pointer(oc->oc_to, &gpointer);
    }
    --stackcount;
}

void outlet_float(t_outlet *x, t_float f)
{
    t_outconnect *oc;
    if(++stackcount >= STACKITER)
        outlet_stackerror(x);
    else
    for (oc = x->o_connections; oc; oc = oc->oc_next)
        pd_float(oc->oc_to, f);
    --stackcount;
}

void outlet_symbol(t_outlet *x, t_symbol *s)
{
    t_outconnect *oc;
    if(++stackcount >= STACKITER)
        outlet_stackerror(x);
    else
    for (oc = x->o_connections; oc; oc = oc->oc_next)
        pd_symbol(oc->oc_to, s);
    --stackcount;
}

void outlet_list(t_outlet *x, t_symbol *s, int argc, t_atom *argv)
{
    t_outconnect *oc;
    if(++stackcount >= STACKITER)
        outlet_stackerror(x);
    else
    for (oc = x->o_connections; oc; oc = oc->oc_next)
        pd_list(oc->oc_to, s, argc, argv);
    --stackcount;
}

void outlet_anything(t_outlet *x, t_symbol *s, int argc, t_atom *argv)
{
    t_outconnect *oc;
    if(++stackcount >= STACKITER)
        outlet_stackerror(x);
    else
    for (oc = x->o_connections; oc; oc = oc->oc_next)
        typedmess(oc->oc_to, s, argc, argv);
    --stackcount;
}

    /* get the outlet's declared symbol */
t_symbol *outlet_getsymbol(t_outlet *x)
{
    return (x->o_sym);
}

void outlet_free(t_outlet *x)
{
    t_object *y = x->o_owner;
    t_outlet *x2;
    if (y->ob_outlet == x) y->ob_outlet = x->o_next;
    else for (x2 = y->ob_outlet; x2; x2 = x2->o_next)
        if (x2->o_next == x)
    {
        x2->o_next = x->o_next;
        break;
    }
    t_freebytes(x, sizeof(*x));
}

    /* connect an outlet of one object to an inlet of another.  The receiving
    "pd" is usually a patchable object, but this may be used to add a
    non-patchable pd to an outlet by specifying the 0th inlet. */
t_outconnect *obj_connect(t_object *source, int outno,
    t_object *sink, int inno)
{
    t_inlet *i;
    t_outlet *o;
    t_pd *to;
    t_outconnect *oc, *oc2, **ochead;

    for (o = source->ob_outlet; o && outno; o = o->o_next, outno--) ;
    if (!o) return (0);

    if (sink->ob_pd->c_firstin)
    {
        if (!inno)
        {
            to = &sink->ob_pd;
            goto doit;
        }
        else inno--;
    }
    for (i = sink->ob_inlet; i && inno; i = i->i_next, inno--) ;
    if (!i) return (0);
    to = &i->i_pd;
doit:
    ochead = outlet_getconnectionpointer(o);
    oc = (t_outconnect *)t_getbytes(sizeof(*oc));
    oc->oc_next = 0;
    oc->oc_to = to;
        /* append it to the end of the list */
        /* LATER we might cache the last "oc" to make this faster. */
    if ((oc2 = *ochead))
    {
        while (oc2->oc_next)
            oc2 = oc2->oc_next;
        oc2->oc_next = oc;
    }
    else *ochead = oc;
    if (o->o_sym == &s_signal) canvas_update_dsp();

    return (oc);
}

void obj_disconnect(t_object *source, int outno, t_object *sink, int inno)
{
    t_inlet *i;
    t_outlet *o;
    t_pd *to;
    t_outconnect *oc, *oc2, **ochead;

    for (o = source->ob_outlet; o && outno; o = o->o_next, outno--) ;
    if (!o) return;
    if (sink->ob_pd->c_firstin)
    {
        if (!inno)
        {
            to = &sink->ob_pd;
            goto doit;
        }
        else inno--;
    }
    for (i = sink->ob_inlet; i && inno; i = i->i_next, inno--) ;
    if (!i) return;
    to = &i->i_pd;
doit:
    ochead = outlet_getconnectionpointer(o);
    if (!(oc = *ochead)) return;
    if (oc->oc_to == to)
    {
        *ochead = oc->oc_next;
        freebytes(oc, sizeof(*oc));
        goto done;
    }
    while ((oc2 = oc->oc_next))
    {
        if (oc2->oc_to == to)
        {
            oc->oc_next = oc2->oc_next;
            freebytes(oc2, sizeof(*oc2));
            goto done;
        }
        oc = oc2;
    }
done:
    if (o->o_sym == &s_signal) canvas_update_dsp();
}

/* ------ traversal routines for code that can't see our structures ------ */

int obj_noutlets(const t_object *x)
{
    int n;
    t_outlet *o;
    for (o = x->ob_outlet, n = 0; o; o = o->o_next) n++;
    return (n);
}

int obj_ninlets(const t_object *x)
{
    int n;
    t_inlet *i;
    for (i = x->ob_inlet, n = 0; i; i = i->i_next) n++;
    if (x->ob_pd->c_firstin) n++;
    return (n);
}

t_outconnect *obj_starttraverseoutlet(const t_object *x, t_outlet **op, int nout)
{
    t_outlet *o = x->ob_outlet;
    while (nout-- && o) o = o->o_next;
    *op = o;
    if (o)
        return (*outlet_getconnectionpointer(o));
    else return (0);
}

t_outconnect *obj_nexttraverseoutlet(t_outconnect *lastconnect,
    t_object **destp, t_inlet **inletp, int *whichp)
{
    t_pd *y;
    y = lastconnect->oc_to;
    if (ISINLET(y))
    {
        int n;
        t_inlet *i = (t_inlet *)y, *i2;
        t_object *dest = i->i_owner;
        for (n = dest->ob_pd->c_firstin, i2 = dest->ob_inlet;
            i2 && i2 != i; i2 = i2->i_next) n++;
        *whichp = n;
        *destp = dest;
        *inletp = i;
    }
    else
    {
        *whichp = 0;
        *inletp = 0;
        *destp = ((t_object *)y);
    }
    return (lastconnect->oc_next);
}

    /* this one checks that a pd is indeed a patchable object, and returns
    it, correctly typed, or zero if the check failed. */
t_object *pd_checkobject(t_pd *x)
{
    if ((*x)->c_patchable) return ((t_object *)x);
    else return (0);
}

    /* move an inlet or outlet to the head of the list */
void obj_moveinletfirst(t_object *x, t_inlet *i)
{
    t_inlet *i2;
    if (x->ob_inlet == i) return;
    else for (i2 = x->ob_inlet; i2; i2 = i2->i_next)
        if (i2->i_next == i)
    {
        i2->i_next = i->i_next;
        i->i_next = x->ob_inlet;
        x->ob_inlet = i;
        return;
    }
}

void obj_moveoutletfirst(t_object *x, t_outlet *o)
{
    t_outlet *o2;
    if (x->ob_outlet == o) return;
    else for (o2 = x->ob_outlet; o2; o2 = o2->o_next)
        if (o2->o_next == o)
    {
        o2->o_next = o->o_next;
        o->o_next = x->ob_outlet;
        x->ob_outlet = o;
        return;
    }
}

    /* routines for DSP sorting, which are used in d_ugen.c and g_canvas.c */
    /* LATER try to consolidate all the slightly different routines. */

int obj_nsiginlets(const t_object *x)
{
    int n;
    t_inlet *i;
    for (i = x->ob_inlet, n = 0; i; i = i->i_next)
        if (i->i_symfrom == &s_signal) n++;
    if (x->ob_pd->c_firstin && x->ob_pd->c_floatsignalin) n++;
    return (n);
}

    /* get the index, among signal inlets, of the mth inlet overall */
int obj_siginletindex(const t_object *x, int m)
{
    int n = 0;
    t_inlet *i;
    if (x->ob_pd->c_firstin)
    {
        if (!m--)
            return (0);
        if (x->ob_pd->c_floatsignalin)
            n++;
    }
    for (i = x->ob_inlet; i; i = i->i_next, m--)
        if (i->i_symfrom == &s_signal)
    {
        if (m == 0) return (n);
        n++;
    }
    return (-1);
}

int obj_issignalinlet(const t_object *x, int m)
{
    t_inlet *i;
    if (x->ob_pd->c_firstin)
    {
        if (!m)
            return (x->ob_pd->c_firstin && x->ob_pd->c_floatsignalin);
        else m--;
    }
    for (i = x->ob_inlet; i && m; i = i->i_next, m--)
        ;
    return (i && (i->i_symfrom == &s_signal));
}

int obj_nsigoutlets(const t_object *x)
{
    int n;
    t_outlet *o;
    for (o = x->ob_outlet, n = 0; o; o = o->o_next)
        if (o->o_sym == &s_signal) n++;
    return (n);
}

int obj_sigoutletindex(const t_object *x, int m)
{
    int n;
    t_outlet *o2;
    for (o2 = x->ob_outlet, n = 0; o2; o2 = o2->o_next, m--)
        if (o2->o_sym == &s_signal)
    {
        if (m == 0) return (n);
        n++;
    }
    return (-1);
}

int obj_issignaloutlet(const t_object *x, int m)
{
    int n;
    t_outlet *o2;
    for (o2 = x->ob_outlet, n = 0; o2 && m--; o2 = o2->o_next);
    return (o2 && (o2->o_sym == &s_signal));
}

t_float *obj_findsignalscalar(const t_object *x, int m)
{
    t_inlet *i;
    if (x->ob_pd->c_firstin && x->ob_pd->c_floatsignalin)
    {
        if (!m--)
            return (x->ob_pd->c_floatsignalin > 0 ?
                (t_float *)(((char *)x) + x->ob_pd->c_floatsignalin) : 0);
    }
    for (i = x->ob_inlet; i; i = i->i_next)
        if (i->i_symfrom == &s_signal)
    {
        if (m-- == 0)
            return (&i->i_un.iu_floatsignalvalue);
    }
    return (0);
}

/* and these are only used in g_io.c... */

int inlet_getsignalindex(t_inlet *x)
{
    int n = 0;
    t_inlet *i;
    if (x->i_symfrom != &s_signal)
        bug("inlet_getsignalindex");
    for (i = x->i_owner->ob_inlet, n = 0; i && i != x; i = i->i_next)
        if (i->i_symfrom == &s_signal) n++;
    return (n);
}

int outlet_getsignalindex(t_outlet *x)
{
    int n = 0;
    t_outlet *o;
    for (o = x->o_owner->ob_outlet, n = 0; o && o != x; o = o->o_next)
        if (o->o_sym == &s_signal) n++;
    return (n);
}

void obj_saveformat(const t_object *x, t_binbuf *bb)
{
    if (x->te_width)
        binbuf_addv(bb, "ssf;", &s__X, gensym("f"), (float)x->te_width);
}

/* this one only in g_clone.c -- LATER consider sending the message
without having to chase the linked list every time? */
void obj_sendinlet(t_object *x, int n, t_symbol *s, int argc, t_atom *argv)
{
    t_inlet *i;
    for (i = x->ob_inlet; i && n; i = i->i_next, n--)
        ;
    if (i)
        typedmess(&i->i_pd, s, argc, argv);
    else bug("obj_sendinlet");
}

/* ------------------- setup routine, somewhat misnamed */
void obj_init(void)
{
    inlet_class = class_new(gensym("inlet"), 0, 0,
        sizeof(t_inlet), CLASS_PD, 0);
    class_addbang(inlet_class, inlet_bang);
    class_addpointer(inlet_class, inlet_pointer);
    class_addfloat(inlet_class, inlet_float);
    class_addsymbol(inlet_class, inlet_symbol);
    class_addlist(inlet_class, inlet_list);
    class_addanything(inlet_class, inlet_anything);

    pointerinlet_class = class_new(gensym("inlet"), 0, 0,
        sizeof(t_inlet), CLASS_PD, 0);
    class_addpointer(pointerinlet_class, pointerinlet_pointer);
    class_addanything(pointerinlet_class, inlet_wrong);

    floatinlet_class = class_new(gensym("inlet"), 0, 0,
        sizeof(t_inlet), CLASS_PD, 0);
    class_addfloat(floatinlet_class, (t_method)floatinlet_float);
    class_addanything(floatinlet_class, inlet_wrong);

    symbolinlet_class = class_new(gensym("inlet"), 0, 0,
        sizeof(t_inlet), CLASS_PD, 0);
    class_addsymbol(symbolinlet_class, symbolinlet_symbol);
    class_addanything(symbolinlet_class, inlet_wrong);

    backtracer_class = class_new(gensym("backtracer"), 0, 0,
        sizeof(t_backtracer), CLASS_PD, 0);
    class_addanything(backtracer_class, backtracer_anything);

}


