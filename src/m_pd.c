/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include "m_imp.h"
#include "m_private_utils.h"
#include "g_canvas.h"   /* just for LB_LOAD */

#include <string.h>

    /* FIXME no out-of-memory testing yet! */

t_pd *pd_new(t_class *c)
{
    t_pd *x;
    if (!c) {
        bug ("pd_new: apparently called before setup routine");
        return NULL;
    }
    x = (t_pd *)t_getbytes(c->c_size);
    *x = c;
    if (c->c_patchable)
    {
        ((t_object *)x)->ob_inlet = 0;
        ((t_object *)x)->ob_outlet = 0;
    }
    return (x);
}

typedef void (*t_freemethod)(t_pd *);

void pd_free(t_pd *x)
{
    t_class *c = *x;
    if (c->c_freemethod) (*(t_freemethod)(c->c_freemethod))(x);
    if (c->c_patchable)
    {
        while (((t_object *)x)->ob_outlet)
            outlet_free(((t_object *)x)->ob_outlet);
        while (((t_object *)x)->ob_inlet)
            inlet_free(((t_object *)x)->ob_inlet);
        if (((t_object *)x)->ob_binbuf)
            binbuf_free(((t_object *)x)->ob_binbuf);
    }
    if (c->c_size) t_freebytes(x, c->c_size);
}

void gobj_save(t_gobj *x, t_binbuf *b)
{
    t_class *c = x->g_pd;
    if (c->c_savefn)
        (c->c_savefn)(x, b);
}

/* deal with several objects bound to the same symbol.  If more than one, we
actually bind a collection object to the symbol, which forwards messages sent
to the symbol.
CHR: We make a copy of the bindlist before messaging, so we can safely unbind
symbols while iterating over the bindlist. We only allocate on the stack up
to a certain limit, above whiche we switch to heap allocation. */

static PERTHREAD int stackcount = 0;

#define MAXSTACKSIZE 1024

#if DONT_USE_ALLOCA
#define BINDLIST_ALLOCA(x, n) ((x) = (t_pd **)getbytes((n) * sizeof(t_pd *)))
#define BINDLIST_FREEA(x, n) (freebytes((x), (n) * sizeof(t_pd *)))
#else
#define BINDLIST_ALLOCA(x, n) ((x) = (t_pd **)(stackcount <= MAXSTACKSIZE ?  \
    alloca((n) * sizeof(t_pd *)) : getbytes((n) * sizeof(t_pd *))))
#define BINDLIST_FREEA(x, n) ( \
    (stackcount <= MAXSTACKSIZE || (freebytes((x), (n) * sizeof(t_pd *)), 0)))
#endif

#define BINDLIST_PUSH(x, vec, n) stackcount += n; \
    BINDLIST_ALLOCA(vec, n); memcpy(vec, x->b_vec, n * sizeof(t_pd *));

#define BINDLIST_POP(vec, n) BINDLIST_FREEA(vec, n); stackcount -= n

static t_class *bindlist_class;

typedef struct _bindlist
{
    t_pd b_pd;
    t_pd **b_vec;
    int b_n;
} t_bindlist;

static void bindlist_bang(t_bindlist *x)
{
    t_pd **vec;
    int i, n = x->b_n;
    BINDLIST_PUSH(x, vec, n);
    for (i = 0; i < n; i++)
        pd_bang(vec[i]);
    BINDLIST_POP(vec, n);
}

static void bindlist_float(t_bindlist *x, t_float f)
{
    t_pd **vec;
    int i, n = x->b_n;
    BINDLIST_PUSH(x, vec, n);
    for (i = 0; i < n; i++)
        pd_float(vec[i], f);
    BINDLIST_POP(vec, n);
}

static void bindlist_symbol(t_bindlist *x, t_symbol *s)
{
    t_pd **vec;
    int i, n = x->b_n;
    BINDLIST_PUSH(x, vec, n);
    for (i = 0; i < n; i++)
        pd_symbol(vec[i], s);
    BINDLIST_POP(vec, n);
}

static void bindlist_pointer(t_bindlist *x, t_gpointer *gp)
{
    t_pd **vec;
    int i, n = x->b_n;
    BINDLIST_PUSH(x, vec, n);
    for (i = 0; i < n; i++)
        pd_pointer(vec[i], gp);
    BINDLIST_POP(vec, n);
}

static void bindlist_list(t_bindlist *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_pd **vec;
    int i, n = x->b_n;
    BINDLIST_PUSH(x, vec, n);
    for (i = 0; i < n; i++)
        pd_list(vec[i], s, argc, argv);
    BINDLIST_POP(vec, n);
}

static void bindlist_anything(t_bindlist *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_pd **vec;
    int i, n = x->b_n;
    BINDLIST_PUSH(x, vec, n);
    for (i = 0; i < n; i++)
        pd_typedmess(vec[i], s, argc, argv);
    BINDLIST_POP(vec, n);
}

void m_pd_setup(void)
{
    bindlist_class = class_new(gensym("bindlist"), 0, 0,
        sizeof(t_bindlist), CLASS_PD, 0);
    class_addbang(bindlist_class, bindlist_bang);
    class_addfloat(bindlist_class, (t_method)bindlist_float);
    class_addsymbol(bindlist_class, bindlist_symbol);
    class_addpointer(bindlist_class, bindlist_pointer);
    class_addlist(bindlist_class, bindlist_list);
    class_addanything(bindlist_class, bindlist_anything);
}

void pd_bind(t_pd *x, t_symbol *s)
{
#ifdef VST_CLEANSER     /* temporary workaround; see m_pd.h */
    vst_cleanser(&s);
#endif
    if (s->s_thing)
    {
        if (*s->s_thing == bindlist_class)
        {
            t_bindlist *b = (t_bindlist *)s->s_thing;
            int oldsize = b->b_n++;
            b->b_vec = (t_pd **)resizebytes(b->b_vec,
                oldsize * sizeof(t_pd *), b->b_n * sizeof(t_pd *));
            memmove(b->b_vec + 1, b->b_vec, oldsize * sizeof(t_pd *));
            b->b_vec[0] = x;
        }
        else
        {
            t_bindlist *b = (t_bindlist *)pd_new(bindlist_class);
            b->b_vec = (t_pd **)getbytes(2 * sizeof(t_pd *));
            b->b_n = 2;
            b->b_vec[0] = x;
            b->b_vec[1] = s->s_thing;
            s->s_thing = &b->b_pd;
        }
    }
    else s->s_thing = x;
}

void pd_unbind(t_pd *x, t_symbol *s)
{
#ifdef VST_CLEANSER
    vst_cleanser(&s);
#endif
    if (s->s_thing == x)
    {
        s->s_thing = 0;
        return;
    }
    if (s->s_thing && *s->s_thing == bindlist_class)
    {
            /* bindlists always have at least two elements... if the number
            goes down to one, get rid of the bindlist and bind the symbol
            straight to the remaining element. */
        t_bindlist *b = (t_bindlist *)s->s_thing;
        int i, n = b->b_n;
        for (i = 0; i < n; i++)
        {
            if (b->b_vec[i] == x)
            {
                if (n > 2)
                {
                    memmove(&b->b_vec[i], &b->b_vec[i + 1],
                        (n - i - 1) * sizeof(t_pd *));
                    b->b_vec = (t_pd **)resizebytes(b->b_vec,
                        n * sizeof(t_pd *), (n - 1) * sizeof(t_pd *));
                    b->b_n--;
                }
                else if (n == 2)
                {
                    s->s_thing = b->b_vec[i == 0 ? 1 : 0];
                    freebytes(b->b_vec, n * sizeof(t_pd *));
                    pd_free(&b->b_pd);
                }
                else
                    bug("pd_unbind");
                return;
            }
        }
    }
    pd_error(x, "%s: couldn't unbind", s->s_name);
}

t_pd *pd_findbyclass(t_symbol *s, const t_class *c)
{
    t_pd *x = 0;

    if (!s->s_thing) return (0);
    if (*s->s_thing == c) return (s->s_thing);
    if (*s->s_thing == bindlist_class)
    {
        t_bindlist *b = (t_bindlist *)s->s_thing;
        int warned = 0, n = b->b_n;
        t_pd **vec = b->b_vec;
        while (n--)
        {
            t_pd *obj = *vec++;
            if (*obj == c)
            {
                if (x && !warned)
                {
                    post("warning: %s: multiply defined", s->s_name);
                    warned = 1;
                }
                x = obj;
            }
        }
    }
    return x;
}

t_pd *pd_findbyclassname(t_symbol *s, const t_symbol *classname)
{
    t_pd *x = 0;

    if (!s->s_thing) return (0);
#ifdef PDINSTANCE
        /* NB: the class name symbol is shared between instances
        so we can't rely on pointer identity! */
    if (!strcmp((*s->s_thing)->c_name->s_name, classname->s_name))
        return (s->s_thing);
#else
    if ((*s->s_thing)->c_name == classname) return (s->s_thing);
#endif
    if (*s->s_thing == bindlist_class)
    {
        t_bindlist *b = (t_bindlist *)s->s_thing;
        int warned = 0, n = b->b_n;
        t_pd **vec = b->b_vec;
        while (n--)
        {
            t_pd *obj = *vec++;
        #ifdef PDINSTANCE /* see above */
            if (!strcmp((*obj)->c_name->s_name, classname->s_name))
        #else
            if ((*obj)->c_name == classname)
        #endif
            {
                if (x && !warned)
                {
                    post("warning: %s: multiply defined", s->s_name);
                    warned = 1;
                }
                x = obj;
            }
        }
    }
    return x;
}

/* stack for maintaining bindings for the #X symbol during nestable loads.
*/

typedef struct _gstack
{
    t_pd *g_what;
    t_symbol *g_loadingabstraction;
    struct _gstack *g_next;
} t_gstack;

static t_gstack *gstack_head = 0;
static t_pd *lastpopped;
static t_symbol *pd_loadingabstraction;

int pd_setloadingabstraction(t_symbol *sym)
{
    t_gstack *foo = gstack_head;
    for (foo = gstack_head; foo; foo = foo->g_next)
        if (foo->g_loadingabstraction == sym)
            return (1);
    pd_loadingabstraction = sym;
    return (0);
}

void pd_pushsym(t_pd *x)
{
    t_gstack *y = (t_gstack *)t_getbytes(sizeof(*y));
    y->g_what = s__X.s_thing;
    y->g_next = gstack_head;
    y->g_loadingabstraction = pd_loadingabstraction;
    pd_loadingabstraction = 0;
    gstack_head = y;
    s__X.s_thing = x;
}

void pd_popsym(t_pd *x)
{
    if (!gstack_head || s__X.s_thing != x) bug("gstack_pop");
    else
    {
        t_gstack *headwas = gstack_head;
        s__X.s_thing = headwas->g_what;
        gstack_head = headwas->g_next;
        t_freebytes(headwas, sizeof(*headwas));
        lastpopped = x;
    }
}

void pd_doloadbang(void)
{
    if (lastpopped)
        pd_vmess(lastpopped, gensym("loadbang"), "f", LB_LOAD);
    lastpopped = 0;
}

void pd_bang(t_pd *x)
{
    (*(*x)->c_bangmethod)(x);
}

void pd_float(t_pd *x, t_float f)
{
    if (x == &pd_objectmaker)
        ((t_floatmethodr)(*(*x)->c_floatmethod))(x, f);
    else
        (*(*x)->c_floatmethod)(x, f);
}

void pd_pointer(t_pd *x, t_gpointer *gp)
{
    (*(*x)->c_pointermethod)(x, gp);
}

void pd_symbol(t_pd *x, t_symbol *s)
{
#ifdef VST_CLEANSER
    vst_cleanser(&s);
#endif
    (*(*x)->c_symbolmethod)(x, s);
}

void pd_list(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
#ifdef VST_CLEANSER
    int i;
    if (s)
        vst_cleanser(&s);
    for (i = 0; i < argc; i++)
        if (argv[i].a_type == A_SYMBOL)
            vst_cleanser(&argv[i].a_w.w_symbol);
#endif
    (*(*x)->c_listmethod)(x, &s_list, argc, argv);
}

void pd_anything(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
#ifdef VST_CLEANSER
    int i;
    vst_cleanser(&s);
    for (i = 0; i < argc; i++)
        if (argv[i].a_type == A_SYMBOL)
            vst_cleanser(&argv[i].a_w.w_symbol);
#endif
    (*(*x)->c_anymethod)(x, s, argc, argv);
}

void mess_init(void);
void sched_init(void);
void obj_init(void);
void conf_init(void);
void glob_init(void);
void garray_init(void);
void ooura_term(void);

void pd_init(void)
{
#ifndef PDINSTANCE
    static int initted = 0;
    if (initted)
        return;
    initted = 1;
#else
    if (pd_instances)
        return;
    pd_instances = (t_pdinstance **)getbytes(sizeof(*pd_instances));
    pd_instances[0] = &pd_maininstance;
    pd_ninstances = 1;
#endif
    pd_init_systems();
}

void pd_term(void)
{
    t_glist *c;
    for (c = pd_getcanvaslist(); c; c = c->gl_next)
        canvas_closebang(c);
#if 0
        /* Canvases may be slow to close and as a workaround people
        may want Pd to shutdown quickly. Conversely, others might
        prefer it if canvases (and all their containing objects) are
        always freed properly. For now let's exit quickly and LATER
        figure out a way to handle this. */
    while ((c = pd_getcanvaslist()))
        pd_free((t_pd *)c);
#endif
    pd_term_systems();
}

void pd_init_systems(void)
{
    mess_init();
    sched_init();
    sys_lock();
    obj_init();
    conf_init();
    glob_init();
    garray_init();
    sys_unlock();
}

void pd_term_systems(void)
{
        /* TODO free resources */
}

t_canvas *pd_getcanvaslist(void)
{
    return (pd_this->pd_canvaslist);
}
