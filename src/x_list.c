/* Copyright (c) 1997- Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
/* #include <string.h> */

#ifdef HAVE_ALLOCA_H        /* ifdef nonsense to find include for alloca() */
# include <alloca.h>        /* linux, mac, mingw, cygwin */
#elif defined _MSC_VER
# include <malloc.h>        /* MSVC */
#else
# include <stddef.h>        /* BSDs for example */
#endif                      /* end alloca() ifdef nonsense */

extern t_pd *newest;

#ifndef HAVE_ALLOCA     /* can work without alloca() but we never need it */
#define HAVE_ALLOCA 1
#endif

#define LIST_NGETBYTE 100 /* bigger that this we use alloc, not alloca */

/* the "list" object family.

    list append - append a list to another
    list prepend - prepend a list to another
    list split - first n elements to first outlet, rest to second outlet 
    list trim - trim off "list" selector
    list length - output number of items in list

Need to think more about:
    list foreach - spit out elements of a list one by one (also in reverse?)
    list array - get items from a named array as a list
    list reverse - permute elements of a list back to front
    list pack - synonym for 'pack'
    list unpack - synonym for 'unpack'
    list cat - build a list by accumulating elements

Probably don't need:
    list first - output first n elements.
    list last - output last n elements
    list nth - nth item in list, counting from zero
*/

/* -------------- utility functions: storage, copying  -------------- */
    /* List element for storage.  Keep an atom and, in case it's a pointer,
        an associated 'gpointer' to protect against stale pointers. */
typedef struct _listelem
{
    t_atom l_a;
    t_gpointer l_p;
} t_listelem;

typedef struct _alist
{
    t_pd l_pd;          /* object to point inlets to */
    int l_n;            /* number of items */
    int l_npointer;     /* number of pointers */
    t_listelem *l_vec;  /* pointer to items */
} t_alist;

#if HAVE_ALLOCA
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)((n) < LIST_NGETBYTE ?  \
        alloca((n) * sizeof(t_atom)) : getbytes((n) * sizeof(t_atom))))
#define ATOMS_FREEA(x, n) ( \
    ((n) < LIST_NGETBYTE || (freebytes((x), (n) * sizeof(t_atom)), 0)))
#else
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)getbytes((n) * sizeof(t_atom)))
#define ATOMS_FREEA(x, n) (freebytes((x), (n) * sizeof(t_atom)))
#endif

static void atoms_copy(int argc, t_atom *from, t_atom *to)
{
    int i;
    for (i = 0; i < argc; i++)
        to[i] = from[i];
}

/* ------------- fake class to divert inlets to ----------------- */

t_class *alist_class;

static void alist_init(t_alist *x)
{
    x->l_pd = alist_class;
    x->l_n = x->l_npointer = 0;
    x->l_vec = 0;
}

static void alist_clear(t_alist *x)
{
    int i;
    for (i = 0; i < x->l_n; i++)
    {
        if (x->l_vec[i].l_a.a_type == A_POINTER)
            gpointer_unset(x->l_vec[i].l_a.a_w.w_gpointer);
    }
    if (x->l_vec)
        freebytes(x->l_vec, x->l_n * sizeof(*x->l_vec));
}

static void alist_list(t_alist *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    alist_clear(x);
    if (!(x->l_vec = (t_listelem *)getbytes(argc * sizeof(*x->l_vec))))
    {
        x->l_n = 0;
        error("list_alloc: out of memory");
        return;
    }
    x->l_n = argc;
    x->l_npointer = 0;
    for (i = 0; i < argc; i++)
    {
        x->l_vec[i].l_a = argv[i];
        if (x->l_vec[i].l_a.a_type == A_POINTER)
        {
            x->l_npointer++;
            gpointer_copy(x->l_vec[i].l_a.a_w.w_gpointer, &x->l_vec[i].l_p);
            x->l_vec[i].l_a.a_w.w_gpointer = &x->l_vec[i].l_p;
        }
    }
}

static void alist_anything(t_alist *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    alist_clear(x);
    if (!(x->l_vec = (t_listelem *)getbytes((argc+1) * sizeof(*x->l_vec))))
    {
        x->l_n = 0;
        error("list_alloc: out of memory");
        return;
    }
    x->l_n = argc+1;
    x->l_npointer = 0;
    SETSYMBOL(&x->l_vec[0].l_a, s);
    for (i = 0; i < argc; i++)
    {
        x->l_vec[i+1].l_a = argv[i];
        if (x->l_vec[i+1].l_a.a_type == A_POINTER)
        {
            x->l_npointer++;            
            gpointer_copy(x->l_vec[i+1].l_a.a_w.w_gpointer, &x->l_vec[i+1].l_p);
            x->l_vec[i+1].l_a.a_w.w_gpointer = &x->l_vec[i+1].l_p;
        }
    }
}

static void alist_toatoms(t_alist *x, t_atom *to)
{
    int i;
    for (i = 0; i < x->l_n; i++)
        to[i] = x->l_vec[i].l_a;
}


static void alist_clone(t_alist *x, t_alist *y)
{
    int i;
    y->l_pd = alist_class;
    y->l_n = x->l_n;
    y->l_npointer = x->l_npointer;
    if (!(y->l_vec = (t_listelem *)getbytes(y->l_n * sizeof(*y->l_vec))))
    {
        y->l_n = 0;
        error("list_alloc: out of memory");
    }
    else for (i = 0; i < x->l_n; i++)
    {
        y->l_vec[i].l_a = x->l_vec[i].l_a;
        if (y->l_vec[i].l_a.a_type == A_POINTER)
        {
            gpointer_copy(y->l_vec[i].l_a.a_w.w_gpointer, &y->l_vec[i].l_p);
            y->l_vec[i].l_a.a_w.w_gpointer = &y->l_vec[i].l_p;
        }
    }
}

static void alist_setup(void)
{
    alist_class = class_new(gensym("list inlet"),
        0, 0, sizeof(t_alist), 0, 0);
    class_addlist(alist_class, alist_list);
    class_addanything(alist_class, alist_anything);
}

/* ------------- list append --------------------- */

t_class *list_append_class;

typedef struct _list_append
{
    t_object x_obj;
    t_alist x_alist;
} t_list_append;

static void *list_append_new(t_symbol *s, int argc, t_atom *argv)
{
    t_list_append *x = (t_list_append *)pd_new(list_append_class);
    alist_init(&x->x_alist);
    alist_list(&x->x_alist, 0, argc, argv);
    outlet_new(&x->x_obj, &s_list);
    inlet_new(&x->x_obj, &x->x_alist.l_pd, 0, 0);
    return (x);
}

static void list_append_list(t_list_append *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_atom *outv;
    int n, outc = x->x_alist.l_n + argc;
    ATOMS_ALLOCA(outv, outc);
    atoms_copy(argc, argv, outv);
    if (x->x_alist.l_npointer)
    {
        t_alist y;
        alist_clone(&x->x_alist, &y);
        alist_toatoms(&y, outv+argc);
        outlet_list(x->x_obj.ob_outlet, &s_list, outc, outv);
        alist_clear(&y);
    }
    else
    {
        alist_toatoms(&x->x_alist, outv+argc);
        outlet_list(x->x_obj.ob_outlet, &s_list, outc, outv);
    }
    ATOMS_FREEA(outv, outc);
}

static void list_append_anything(t_list_append *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_atom *outv;
    int n, outc = x->x_alist.l_n + argc + 1;
    ATOMS_ALLOCA(outv, outc);
    SETSYMBOL(outv, s);
    atoms_copy(argc, argv, outv + 1);
    if (x->x_alist.l_npointer)
    {
        t_alist y;
        alist_clone(&x->x_alist, &y);
        alist_toatoms(&y, outv + 1 + argc);
        outlet_list(x->x_obj.ob_outlet, &s_list, outc, outv);
        alist_clear(&y);
    }
    else
    {
        alist_toatoms(&x->x_alist, outv + 1 + argc);
        outlet_list(x->x_obj.ob_outlet, &s_list, outc, outv);
    }
    ATOMS_FREEA(outv, outc);
}

static void list_append_free(t_list_append *x)
{
    alist_clear(&x->x_alist);
}

static void list_append_setup(void)
{
    list_append_class = class_new(gensym("list append"),
        (t_newmethod)list_append_new, (t_method)list_append_free,
        sizeof(t_list_append), 0, A_GIMME, 0);
    class_addlist(list_append_class, list_append_list);
    class_addanything(list_append_class, list_append_anything);
    class_sethelpsymbol(list_append_class, &s_list);
}

/* ------------- list prepend --------------------- */

t_class *list_prepend_class;

typedef struct _list_prepend
{
    t_object x_obj;
    t_alist x_alist;
} t_list_prepend;

static void *list_prepend_new(t_symbol *s, int argc, t_atom *argv)
{
    t_list_prepend *x = (t_list_prepend *)pd_new(list_prepend_class);
    alist_init(&x->x_alist);
    alist_list(&x->x_alist, 0, argc, argv);
    outlet_new(&x->x_obj, &s_list);
    inlet_new(&x->x_obj, &x->x_alist.l_pd, 0, 0);
    return (x);
}

static void list_prepend_list(t_list_prepend *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_atom *outv;
    int n, outc = x->x_alist.l_n + argc;
    ATOMS_ALLOCA(outv, outc);
    atoms_copy(argc, argv, outv + x->x_alist.l_n);
    if (x->x_alist.l_npointer)
    {
        t_alist y;
        alist_clone(&x->x_alist, &y);
        alist_toatoms(&y, outv);
        outlet_list(x->x_obj.ob_outlet, &s_list, outc, outv);
        alist_clear(&y);
    }
    else
    {
        alist_toatoms(&x->x_alist, outv);
        outlet_list(x->x_obj.ob_outlet, &s_list, outc, outv);
    }
    ATOMS_FREEA(outv, outc);
}



static void list_prepend_anything(t_list_prepend *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_atom *outv;
    int n, outc = x->x_alist.l_n + argc + 1;
    ATOMS_ALLOCA(outv, outc);
    SETSYMBOL(outv + x->x_alist.l_n, s);
    atoms_copy(argc, argv, outv + x->x_alist.l_n + 1);
    if (x->x_alist.l_npointer)
    {
        t_alist y;
        alist_clone(&x->x_alist, &y);
        alist_toatoms(&y, outv);
        outlet_list(x->x_obj.ob_outlet, &s_list, outc, outv);
        alist_clear(&y);
    }
    else
    {
        alist_toatoms(&x->x_alist, outv);
        outlet_list(x->x_obj.ob_outlet, &s_list, outc, outv);
    }
    ATOMS_FREEA(outv, outc);
}

static void list_prepend_free(t_list_prepend *x)
{
    alist_clear(&x->x_alist);
}

static void list_prepend_setup(void)
{
    list_prepend_class = class_new(gensym("list prepend"),
        (t_newmethod)list_prepend_new, (t_method)list_prepend_free,
        sizeof(t_list_prepend), 0, A_GIMME, 0);
    class_addlist(list_prepend_class, list_prepend_list);
    class_addanything(list_prepend_class, list_prepend_anything);
    class_sethelpsymbol(list_prepend_class, &s_list);
}

/* ------------- list split --------------------- */

t_class *list_split_class;

typedef struct _list_split
{
    t_object x_obj;
    t_float x_f;
    t_outlet *x_out1;
    t_outlet *x_out2;
    t_outlet *x_out3;
} t_list_split;

static void *list_split_new(t_floatarg f)
{
    t_list_split *x = (t_list_split *)pd_new(list_split_class);
    x->x_out1 = outlet_new(&x->x_obj, &s_list);
    x->x_out2 = outlet_new(&x->x_obj, &s_list);
    x->x_out3 = outlet_new(&x->x_obj, &s_list);
    floatinlet_new(&x->x_obj, &x->x_f);
    x->x_f = f;
    return (x);
}

static void list_split_list(t_list_split *x, t_symbol *s,
    int argc, t_atom *argv)
{
    int n = x->x_f;
    if (n < 0)
        n = 0;
    if (argc >= n)
    {
        outlet_list(x->x_out2, &s_list, argc-n, argv+n);
        outlet_list(x->x_out1, &s_list, n, argv);
    }
    else outlet_list(x->x_out3, &s_list, argc, argv);
}

static void list_split_anything(t_list_split *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_atom *outv;
    ATOMS_ALLOCA(outv, argc+1);
    SETSYMBOL(outv, s);
    atoms_copy(argc, argv, outv + 1);
    list_split_list(x, &s_list, argc+1, outv);
    ATOMS_FREEA(outv, argc+1);
}

static void list_split_setup(void)
{
    list_split_class = class_new(gensym("list split"),
        (t_newmethod)list_split_new, 0,
        sizeof(t_list_split), 0, A_DEFFLOAT, 0);
    class_addlist(list_split_class, list_split_list);
    class_addanything(list_split_class, list_split_anything);
    class_sethelpsymbol(list_split_class, &s_list);
}

/* ------------- list trim --------------------- */

t_class *list_trim_class;

typedef struct _list_trim
{
    t_object x_obj;
} t_list_trim;

static void *list_trim_new( void)
{
    t_list_trim *x = (t_list_trim *)pd_new(list_trim_class);
    outlet_new(&x->x_obj, &s_list);
    return (x);
}

static void list_trim_list(t_list_trim *x, t_symbol *s,
    int argc, t_atom *argv)
{
    if (argc < 1 || argv[0].a_type != A_SYMBOL)
        outlet_list(x->x_obj.ob_outlet, &s_list, argc, argv);
    else outlet_anything(x->x_obj.ob_outlet, argv[0].a_w.w_symbol,
        argc-1, argv+1);
}

static void list_trim_anything(t_list_trim *x, t_symbol *s,
    int argc, t_atom *argv)
{
    outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
}

static void list_trim_setup(void)
{
    list_trim_class = class_new(gensym("list trim"),
        (t_newmethod)list_trim_new, 0,
        sizeof(t_list_trim), 0, 0);
    class_addlist(list_trim_class, list_trim_list);
    class_addanything(list_trim_class, list_trim_anything);
    class_sethelpsymbol(list_trim_class, &s_list);
}

/* ------------- list length --------------------- */

t_class *list_length_class;

typedef struct _list_length
{
    t_object x_obj;
} t_list_length;

static void *list_length_new( void)
{
    t_list_length *x = (t_list_length *)pd_new(list_length_class);
    outlet_new(&x->x_obj, &s_float);
    return (x);
}

static void list_length_list(t_list_length *x, t_symbol *s,
    int argc, t_atom *argv)
{
    outlet_float(x->x_obj.ob_outlet, (t_float)argc);
}

static void list_length_anything(t_list_length *x, t_symbol *s,
    int argc, t_atom *argv)
{
    outlet_float(x->x_obj.ob_outlet, (t_float)argc+1);
}

static void list_length_setup(void)
{
    list_length_class = class_new(gensym("list length"),
        (t_newmethod)list_length_new, 0,
        sizeof(t_list_length), 0, 0);
    class_addlist(list_length_class, list_length_list);
    class_addanything(list_length_class, list_length_anything);
    class_sethelpsymbol(list_length_class, &s_list);
}

/* ------------- list ------------------- */

static void *list_new(t_pd *dummy, t_symbol *s, int argc, t_atom *argv)
{
    if (!argc || argv[0].a_type != A_SYMBOL)
        newest = list_append_new(s, argc, argv);
    else
    {
        t_symbol *s2 = argv[0].a_w.w_symbol;
        if (s2 == gensym("append"))
            newest = list_append_new(s, argc-1, argv+1);
        else if (s2 == gensym("prepend"))
            newest = list_prepend_new(s, argc-1, argv+1);
        else if (s2 == gensym("split"))
            newest = list_split_new(atom_getfloatarg(1, argc, argv));
        else if (s2 == gensym("trim"))
            newest = list_trim_new();
        else if (s2 == gensym("length"))
            newest = list_length_new();
        else 
        {
            error("list %s: unknown function", s2->s_name);
            newest = 0;
        }
    }
    return (newest);
}

void x_list_setup(void)
{
    alist_setup();
    list_append_setup();
    list_prepend_setup();
    list_split_setup();
    list_trim_setup();
    list_length_setup();
    class_addcreator((t_newmethod)list_new, &s_list, A_GIMME, 0);
}
