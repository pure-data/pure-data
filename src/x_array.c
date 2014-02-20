/* Copyright (c) 1997-2013 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* The "array" object. */

#include "m_pd.h"
#include "g_canvas.h"
#include <string.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif
extern t_pd *newest;    /* OK - this should go into a .h file now :) */

#ifdef HAVE_ALLOCA_H        /* ifdef nonsense to find include for alloca() */
# include <alloca.h>        /* linux, mac, mingw, cygwin */
#elif defined _MSC_VER
# include <malloc.h>        /* MSVC */
#else
# include <stddef.h>        /* BSDs for example */
#endif                      /* end alloca() ifdef nonsense */

#ifndef HAVE_ALLOCA     /* can work without alloca() but we never need it */
#define HAVE_ALLOCA 1
#endif
#define TEXT_NGETBYTE 100 /* bigger that this we use alloc, not alloca */
#if HAVE_ALLOCA
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)((n) < TEXT_NGETBYTE ?  \
        alloca((n) * sizeof(t_atom)) : getbytes((n) * sizeof(t_atom))))
#define ATOMS_FREEA(x, n) ( \
    ((n) < TEXT_NGETBYTE || (freebytes((x), (n) * sizeof(t_atom)), 0)))
#else
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)getbytes((n) * sizeof(t_atom)))
#define ATOMS_FREEA(x, n) (freebytes((x), (n) * sizeof(t_atom)))
#endif

/* -- "table" - classic "array define" object by Guenter Geiger --*/

static int tabcount = 0;

static void *table_donew(t_symbol *s, int size, int flags,
    int xpix, int ypix)
{
    t_atom a[9];
    t_glist *gl;
    t_canvas *x, *z = canvas_getcurrent();
    if (s == &s_)
    {
         char  tabname[255];
         t_symbol *t = gensym("table"); 
         sprintf(tabname, "%s%d", t->s_name, tabcount++);
         s = gensym(tabname); 
    }
    if (size <= 1)
        size = 100;
    SETFLOAT(a, 0);
    SETFLOAT(a+1, 50);
    SETFLOAT(a+2, xpix + 100);
    SETFLOAT(a+3, ypix + 100);
    SETSYMBOL(a+4, s);
    SETFLOAT(a+5, 0);
    x = canvas_new(0, 0, 6, a);

    x->gl_owner = z;

        /* create a graph for the table */
    gl = glist_addglist((t_glist*)x, &s_, 0, -1, (size > 1 ? size-1 : 1), 1,
        50, ypix+50, xpix+50, 50);

    graph_array(gl, s, &s_float, size, flags);

    newest = &x->gl_pd;     /* mimic action of canvas_pop() */
    pd_popsym(&x->gl_pd);
    x->gl_loading = 0;

    return (x);
}

static void *table_new(t_symbol *s, t_floatarg f)
{
    return (table_donew(s, f, 0, 500, 300));
}

    /* return true if the "canvas" object is a "table". */
int canvas_istable(t_canvas *x)
{
    t_atom *argv = (x->gl_obj.te_binbuf? binbuf_getvec(x->gl_obj.te_binbuf):0);
    int argc = (x->gl_obj.te_binbuf? binbuf_getnatom(x->gl_obj.te_binbuf) : 0);
    int istable = (argc && argv[0].a_type == A_SYMBOL &&
        argv[0].a_w.w_symbol == gensym("table"));
    return (istable);
}

t_class *array_define_class;

static void array_define_yrange(t_glist *x, t_floatarg ylo, t_floatarg yhi)
{
    t_glist *gl = (x->gl_list ? pd_checkglist(&x->gl_list->g_pd) : 0);
    if (gl && gl->gl_list && pd_class(&gl->gl_list->g_pd) == garray_class)
    {
        int n = garray_getarray((t_garray *)gl->gl_list)->a_n;
        vmess(&x->gl_list->g_pd, gensym("bounds"),
            "ffff", 0., yhi, (float)(n-1), ylo);
        vmess(&x->gl_list->g_pd, gensym("xlabel"),
            "fff", ylo + glist_pixelstoy(gl, 2) - glist_pixelstoy(gl, 0),
                0., (float)(n-1));
        vmess(&x->gl_list->g_pd, gensym("ylabel"),
            "fff", glist_pixelstox(gl, 0) - glist_pixelstox(gl, 5), ylo, yhi);
    }
    else bug("array_define_yrange");
}

static void *array_define_new(t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *arrayname = &s_;
    float arraysize = 100;
    t_glist *x;
    int keep = 0;
    float ylo = -1, yhi = 1;
    float xpix = 500, ypix = 300;
    while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(argv->a_w.w_symbol->s_name, "-k"))
            keep = 1;
        else if (!strcmp(argv->a_w.w_symbol->s_name, "-yrange") &&
            argc >= 3 && argv[1].a_type == A_FLOAT &&
                argv[2].a_type == A_FLOAT)
        {
            ylo = atom_getfloatarg(1, argc, argv);
            yhi = atom_getfloatarg(2, argc, argv);
            if (ylo == yhi)
                ylo = -1, yhi = 1;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(argv->a_w.w_symbol->s_name, "-pix") &&
            argc >= 3 && argv[1].a_type == A_FLOAT &&
                argv[2].a_type == A_FLOAT)
        {
            if ((xpix = atom_getfloatarg(1, argc, argv)) < 10)
                xpix = 10;
            if ((ypix = atom_getfloatarg(2, argc, argv)) < 10)
                ypix = 10;
            argc -= 2; argv += 2;
        }
        else
        {
            error("array define: unknown flag ...");
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc && argv->a_type == A_SYMBOL)
    {
        arrayname = argv->a_w.w_symbol;
        argc--; argv++;
    }
    if (argc && argv->a_type == A_FLOAT)
    {
        arraysize = argv->a_w.w_float;
        argc--; argv++;
    }
    if (argc)
    {
        post("warning: array define ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    x = (t_glist *)table_donew(arrayname, arraysize, keep, xpix, ypix);
    
        /* bash the class to "array define".  We don't do this earlier in
        part so that canvas_getcurrent() will work while the glist and
        garray are being created.  There may be other, unknown side effects. */
    x->gl_obj.ob_pd = array_define_class;
    array_define_yrange(x, ylo, yhi);
    return (x);
}

void garray_savecontentsto(t_garray *x, t_binbuf *b);

void array_define_save(t_gobj *z, t_binbuf *bb)
{
    t_glist *x = (t_glist *)z;
    t_glist *gl = (x->gl_list ? pd_checkglist(&x->gl_list->g_pd) : 0);
    binbuf_addv(bb, "ssff", &s__X, gensym("obj"),
        (float)x->gl_obj.te_xpix, (float)x->gl_obj.te_ypix);
    binbuf_addbinbuf(bb, x->gl_obj.ob_binbuf);
    binbuf_addsemi(bb);

    garray_savecontentsto((t_garray *)gl->gl_list, bb);
    obj_saveformat(&x->gl_obj, bb);
}

t_scalar *garray_getscalar(t_garray *x);

    /* send a pointer to the scalar that owns this array to
    whomever is bound to the given symbol */
static void array_define_send(t_glist *x, t_symbol *s)
{
    t_glist *gl = (x->gl_list ? pd_checkglist(&x->gl_list->g_pd) : 0);
    if (!s->s_thing)
        pd_error(x, "array_define_send: %s: no such object", s->s_name);
    else if (gl && gl->gl_list && pd_class(&gl->gl_list->g_pd) == garray_class)
    {
        t_gpointer gp;
        gpointer_init(&gp);
        gpointer_setglist(&gp, gl,
            garray_getscalar((t_garray *)gl->gl_list));
        pd_pointer(s->s_thing, &gp);
        gpointer_unset(&gp);
    }
    else bug("array_define_anything");
}

    /* just forward any messages to the garray */
static void array_define_anything(t_glist *x,
    t_symbol *s, int argc, t_atom *argv)
{
    t_glist *gl = (x->gl_list ? pd_checkglist(&x->gl_list->g_pd) : 0);
    if (gl && gl->gl_list && pd_class(&gl->gl_list->g_pd) == garray_class)
        typedmess(&gl->gl_list->g_pd, s, argc, argv);
    else bug("array_define_anything");
}

    /* ignore messages like "editmode" */
static void array_define_ignore(t_glist *x,
    t_symbol *s, int argc, t_atom *argv)
{
}

/* ---  array_client - common code for objects that refer to arrays -- */

typedef struct _array_client
{
    t_object tc_obj;
    t_symbol *tc_sym;
    t_gpointer tc_gp;
    t_symbol *tc_struct;
    t_symbol *tc_field;
    t_canvas *tc_canvas;
} t_array_client;

#define x_sym x_tc.tc_sym
#define x_struct x_tc.tc_struct
#define x_field x_tc.tc_field
#define x_gp x_tc.tc_gp

    /* find the array for this object.  Prints an error  message and returns
        0 on failure. */
static t_array *array_client_getbuf(t_array_client *x, t_glist **glist)
{
    if (x->tc_sym)       /* named array object */
    {
        t_garray *y = (t_garray *)pd_findbyclass(x->tc_sym, garray_class);
        if (y)
        {
            *glist = garray_getglist(y);
            return (garray_getarray(y));
        }
        else
        {
            pd_error(x, "array: couldn't find named array '%s'",
                x->tc_sym->s_name);
            *glist = 0;
            return (0);
        }
    }
    else if (x->tc_struct)   /* by pointer */
    {
        t_template *template = template_findbyname(x->tc_struct);
        t_gstub *gs = x->tc_gp.gp_stub;
        t_word *vec; 
        int onset, type;
        t_symbol *arraytype;
        if (!template)
        {
            pd_error(x, "array: couldn't find struct %s", x->tc_struct->s_name);
            return (0);
        }
        if (!gpointer_check(&x->tc_gp, 0))
        {
            pd_error(x, "array: stale or empty pointer");
            return (0);
        }
        if (gs->gs_which == GP_ARRAY)
            vec = x->tc_gp.gp_un.gp_w;
        else vec = x->tc_gp.gp_un.gp_scalar->sc_vec;

        if (!template_find_field(template,
            x->tc_field, &onset, &type, &arraytype))
        {
            pd_error(x, "array: no field named %s", x->tc_field->s_name);
            return (0);
        }
        if (type != DT_ARRAY)
        {
            pd_error(x, "array: field %s not of type array",
                x->tc_field->s_name);
            return (0);
        }
        if (gs->gs_which == GP_GLIST)
            *glist = gs->gs_un.gs_glist;
        else
        {
            t_array *owner_array = gs->gs_un.gs_array;
            while (owner_array->a_gp.gp_stub->gs_which == GP_ARRAY)
                owner_array = owner_array->a_gp.gp_stub->gs_un.gs_array;
            *glist = owner_array->a_gp.gp_stub->gs_un.gs_glist;
        }
        return (*(t_array **)(((char *)vec) + onset));
    }
    else return (0);    /* shouldn't happen */
}

static void array_client_senditup(t_array_client *x)
{
    t_glist *glist = 0;
    t_array *a = array_client_getbuf(x, &glist);
    array_redraw(a, glist);
}

static void array_client_free(t_array_client *x)
{
    gpointer_unset(&x->tc_gp);
}

/* ----------  array size : get or set size of an array ---------------- */
static t_class *array_size_class;

typedef struct _array_size
{
    t_array_client x_tc;
} t_array_size;
#define x_outlet x_tc.tc_obj.ob_outlet

static void *array_size_new(t_symbol *s, int argc, t_atom *argv)
{
    t_array_size *x = (t_array_size *)pd_new(array_size_class);
    x->x_sym = x->x_struct = x->x_field = 0;
    gpointer_init(&x->x_gp);
    while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(argv->a_w.w_symbol->s_name, "-s") &&
            argc >= 3 && argv[1].a_type == A_SYMBOL &&
                argv[2].a_type == A_SYMBOL)
        {
            x->x_struct = canvas_makebindsym(argv[1].a_w.w_symbol);
            x->x_field = argv[2].a_w.w_symbol;
            argc -= 2; argv += 2;
        }
        else
        {
            pd_error(x, "array setline: unknown flag ...");
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc && argv->a_type == A_SYMBOL)
    {
        if (x->x_struct)
        {
            pd_error(x, "array setline: extra names after -s..");
            postatom(argc, argv); endpost();
        }
        else x->x_sym = argv->a_w.w_symbol;
        argc--; argv++;
    }
    if (argc)
    {
        post("warning: array setline ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    if (x->x_struct)
        pointerinlet_new(&x->x_tc.tc_obj, &x->x_gp);
    else symbolinlet_new(&x->x_tc.tc_obj, &x->x_tc.tc_sym);
    outlet_new(&x->x_tc.tc_obj, &s_float);
    return (x);
}

static void array_size_bang(t_array_size *x)
{
    t_glist *glist;
    t_array *a = array_client_getbuf(&x->x_tc, &glist);
    if (a)
        outlet_float(x->x_outlet, a->a_n);
}

static void array_size_float(t_array_size *x, t_floatarg f)
{
    t_glist *glist;
    t_array *a = array_client_getbuf(&x->x_tc, &glist);
    if (a)
    {
              /* if it's a named array object we have to go back and find the
              garray (repeating work done in array_client_getbuf()) because
              the garray might want to adjust.  Maybe array_client_getbuf
              should have a return slot for the garray if any?  */
        if (x->x_tc.tc_sym)
        {
            t_garray *y = (t_garray *)pd_findbyclass(x->x_tc.tc_sym,
                garray_class);
            garray_resize(y, f);
        }
        else
        {
            int n = f;
            if (n < 1)
                n = 1;
             array_resize_and_redraw(a, glist, n);
        }
    }
}

/* ------  range operations - act on a specifiable range in an array ----- */
static t_class *array_sum_class;

typedef struct _array_rangeop   /* any operation meaningful on a subrange */
{
    t_array_client x_tc;
    float x_onset;
    float x_n;
    t_symbol *x_elemfield;
    t_symbol *x_elemtemplate;   /* unused - perhaps should at least check it */
} t_array_rangeop;

    /* generic creator for operations on ranges (array {get,set,sum,random,
        quantile,search,...}  "onsetin" and "nin" are true if we should make
        inlets for onset and n - if no inlet for 'n' we also won't allow
        it to be specified as an argument.  Everything can take an onset but
        sometimes we don't need an inlet because it's the inlet itself.  In
        any case we allow onset to be specified as an argument (even if it's
        the 'hot inlet') -- for the same reason as in the 'delay' object.
        Finally we can optionally warn if there are extra arguemnts; some
        specific arguemtns (e.g., search) allow them but most don't. */
static void *array_rangeop_new(t_class *class,
    t_symbol *s, int *argcp, t_atom **argvp,
    int onsetin, int nin, int warnextra)
{
    int argc = *argcp;
    t_atom *argv = *argvp;
    t_array_rangeop *x = (t_array_rangeop *)pd_new(class);
    x->x_sym = x->x_struct = x->x_field = 0;
    gpointer_init(&x->x_gp);
    x->x_elemtemplate = &s_;
    x->x_elemfield = gensym("y"); 
    x->x_onset = 0;
    x->x_n = -1;
    if (onsetin)
        floatinlet_new(&x->x_tc.tc_obj, &x->x_onset);
    if (nin)
        floatinlet_new(&x->x_tc.tc_obj, &x->x_n);
    while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(argv->a_w.w_symbol->s_name, "-s") &&
            argc >= 3 && argv[1].a_type == A_SYMBOL &&
                argv[2].a_type == A_SYMBOL)
        {
            x->x_struct = canvas_makebindsym(argv[1].a_w.w_symbol);
            x->x_field = argv[2].a_w.w_symbol;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(argv->a_w.w_symbol->s_name, "-f") &&
            argc >= 3 && argv[1].a_type == A_SYMBOL &&
                argv[2].a_type == A_SYMBOL)
        {
            x->x_elemtemplate = argv[1].a_w.w_symbol;
            x->x_elemfield = argv[2].a_w.w_symbol;
            argc -= 2; argv += 2;
        }
        else
        {
            pd_error(x, "%s: unknown flag ...", class_getname(class));
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc && argv->a_type == A_SYMBOL)
    {
        if (x->x_struct)
        {
            pd_error(x, "%s: extra names after -s..", class_getname(class));
            postatom(argc, argv); endpost();
        }
        else x->x_sym = argv->a_w.w_symbol;
        argc--; argv++;
    }
    if (argc && argv->a_type == A_FLOAT)
    {
        x->x_onset = argv->a_w.w_float;
        argc--; argv++;
    }
    if (argc && argv->a_type == A_FLOAT)
    {
        x->x_n = argv->a_w.w_float;
        argc--; argv++;
    }
    if (argc && warnextra)
    {
        post("warning: %s ignoring extra argument: ", class_getname(class));
        postatom(argc, argv); endpost();
    }
    if (x->x_struct)
        pointerinlet_new(&x->x_tc.tc_obj, &x->x_gp);
    else symbolinlet_new(&x->x_tc.tc_obj, &x->x_tc.tc_sym);
    *argcp = argc;
    *argvp = argv;
    return (x);
}

static int array_rangeop_getrange(t_array_rangeop *x,
    char **firstitemp, int *nitemp, int *stridep)
{
    t_glist *glist;
    t_array *a = array_client_getbuf(&x->x_tc, &glist);
    char *elemp;
    int stride, onset, firstitem, nitem, i, type;
    t_symbol *arraytype;
    double sum;
    t_template *template;
    if (!a)
        return (0);
    template = template_findbyname(a->a_templatesym);
    if (!template_find_field(template, x->x_elemfield, &onset,
        &type, &arraytype) || type != DT_FLOAT)
    {
        pd_error(x, "can't find field %s in struct %s",
            x->x_elemfield->s_name, a->a_templatesym->s_name);
        return (0);
    }
    stride = a->a_elemsize;
    firstitem = x->x_onset;
    if (firstitem < 0)
        firstitem = 0;
    else if (firstitem > a->a_n)
        firstitem = a->a_n;
    if (x->x_n < 0)
        nitem = a->a_n - firstitem;
    else
    {
        nitem = x->x_n;
        if (nitem + firstitem > a->a_n)
            nitem = a->a_n - firstitem;
    }
    *firstitemp = a->a_vec+onset+firstitem*stride;
    *nitemp = nitem;
    *stridep = stride;
    return (1);
}

/* --------  specific operations on ranges of arrays -------- */

/* ----------------  array sum -- add them up ------------------- */
static t_class *array_sum_class;

#define t_array_sum t_array_rangeop

static void *array_sum_new(t_symbol *s, int argc, t_atom *argv)
{
    t_array_sum *x = array_rangeop_new(array_sum_class, s, &argc, &argv,
        0, 1, 1);
    outlet_new(&x->x_tc.tc_obj, &s_float);
    return (x);
}

static void array_sum_bang(t_array_rangeop *x)
{
    char *itemp, *firstitem;
    int stride, nitem, i;
    double sum;
    if (!array_rangeop_getrange(x, &firstitem, &nitem, &stride))
        return;
    for (i = 0, sum = 0, itemp = firstitem; i < nitem; i++, itemp += stride)
        sum += *(t_float *)itemp;
    outlet_float(x->x_outlet, sum);
}

static void array_sum_float(t_array_rangeop *x, t_floatarg f)
{
    x->x_onset = f;
    array_sum_bang(x);
}

/* ----------------  array get -- output as list ------------------- */
static t_class *array_get_class;

#define t_array_get t_array_rangeop

static void *array_get_new(t_symbol *s, int argc, t_atom *argv)
{
    t_array_get *x = array_rangeop_new(array_get_class, s, &argc, &argv,
        0, 1, 1);
    outlet_new(&x->x_tc.tc_obj, &s_float);
    return (x);
}

static void array_get_bang(t_array_rangeop *x)
{
    char *itemp, *firstitem;
    int stride, nitem, i;
    t_atom *outv;
    if (!array_rangeop_getrange(x, &firstitem, &nitem, &stride))
        return;
    ATOMS_ALLOCA(outv, nitem);
    for (i = 0, itemp = firstitem; i < nitem; i++, itemp += stride)
        SETFLOAT(&outv[i],  *(t_float *)itemp);
    outlet_list(x->x_outlet, 0, nitem, outv);
}

static void array_get_float(t_array_rangeop *x, t_floatarg f)
{
    x->x_onset = f;
    array_get_bang(x);
}

/* --------------  array set -- copy list to array -------------- */
static t_class *array_set_class;

#define t_array_set t_array_rangeop

static void *array_set_new(t_symbol *s, int argc, t_atom *argv)
{
    t_array_set *x = array_rangeop_new(array_set_class, s, &argc, &argv,
        1, 0, 1);
    return (x);
}

static void array_set_list(t_array_rangeop *x, t_symbol *s,
    int argc, t_atom *argv)
{
    char *itemp, *firstitem;
    int stride, nitem, i;
    if (!array_rangeop_getrange(x, &firstitem, &nitem, &stride))
        return;
    if (nitem > argc)
        nitem = argc;
    for (i = 0, itemp = firstitem; i < nitem; i++, itemp += stride)
        *(t_float *)itemp = atom_getfloatarg(i, argc, argv);
    array_client_senditup(&x->x_tc);
}

/* -----  array quantile -- output quantile for input from 0 to 1 ------- */
static t_class *array_quantile_class;

#define t_array_quantile t_array_rangeop

static void *array_quantile_new(t_symbol *s, int argc, t_atom *argv)
{
    t_array_quantile *x = array_rangeop_new(array_quantile_class, s,
        &argc, &argv, 1, 1, 1);
    outlet_new(&x->x_tc.tc_obj, &s_float);
    return (x);
}

static void array_quantile_float(t_array_rangeop *x, t_floatarg f)
{
    char *itemp, *firstitem;
    int stride, nitem, i;
    double sum;
    if (!array_rangeop_getrange(x, &firstitem, &nitem, &stride))
        return;
    for (i = 0, sum = 0, itemp = firstitem; i < nitem; i++, itemp += stride)
        sum += (*(t_float *)itemp > 0? *(t_float *)itemp : 0);
    sum *= f;
    for (i = 0, itemp = firstitem; i < (nitem-1); i++, itemp += stride)
    {
        sum -= (*(t_float *)itemp > 0? *(t_float *)itemp : 0);
        if (sum < 0)
            break;
    }
    outlet_float(x->x_outlet, i);
}

/* ----  array random -- output random value with array as distribution ---- */
static t_class *array_random_class;

typedef struct _array_random   /* any operation meaningful on a subrange */
{
    t_array_rangeop x_r;
    unsigned int x_state;
} t_array_random;

static void *array_random_new(t_symbol *s, int argc, t_atom *argv)
{
    t_array_random *x = array_rangeop_new(array_random_class, s,
        &argc, &argv, 0, 1, 1);
    static unsigned int random_nextseed = 584926371;
    random_nextseed = random_nextseed * 435898247 + 938284287;
    x->x_state = random_nextseed;
    outlet_new(&x->x_r.x_tc.tc_obj, &s_float);
    return (x);
}

static void array_random_seed(t_array_random *x, t_floatarg f)
{
    x->x_state = f;
}

static void array_random_bang(t_array_random *x)
{
    char *itemp, *firstitem;
    int stride, nitem, i;
    
    if (!array_rangeop_getrange(&x->x_r, &firstitem, &nitem, &stride))
        return;
    x->x_state = x->x_state * 472940017 + 832416023;
    array_quantile_float(&x->x_r, (1./4294967296.0) * (double)(x->x_state));
}

static void array_random_float(t_array_random *x, t_floatarg f)
{
    x->x_r.x_onset = f;
    array_random_bang(x);
}

/* ----  array max -- output largest value and its index ------------ */
static t_class *array_max_class;

typedef struct _array_max
{
    t_array_rangeop x_rangeop;
    t_outlet *x_out1;       /* value */
    t_outlet *x_out2;       /* index */
    int x_onset;            /* search onset */
} t_array_max;

static void *array_max_new(t_symbol *s, int argc, t_atom *argv)
{
    t_array_max *x = array_rangeop_new(array_max_class, s, &argc, &argv,
        0, 1, 1);
    x->x_out1 = outlet_new(&x->x_rangeop.x_tc.tc_obj, &s_float);
    x->x_out2 = outlet_new(&x->x_rangeop.x_tc.tc_obj, &s_float);
    return (x);
}

static void array_max_bang(t_array_max *x)
{
    char *itemp, *firstitem;
    int stride, nitem, i, besti;
    t_float bestf;
    if (!array_rangeop_getrange(&x->x_rangeop, &firstitem, &nitem, &stride))
        return;
    for (i = 0, besti = 0, bestf= -1e30, itemp = firstitem;
        i < nitem; i++, itemp += stride)
            if (*(t_float *)itemp > bestf)
                bestf = *(t_float *)itemp, besti = i;
    outlet_float(x->x_out2, besti+x->x_onset);
    outlet_float(x->x_out1, bestf);
}

static void array_max_float(t_array_max *x, t_floatarg f)
{
    x->x_onset = f;
    array_max_bang(x);
}

/* ----  array min -- output largest value and its index ------------ */
static t_class *array_min_class;

typedef struct _array_min
{
    t_array_rangeop x_rangeop;
    t_outlet *x_out1;       /* value */
    t_outlet *x_out2;       /* index */
    int x_onset;            /* search onset */
} t_array_min;

static void *array_min_new(t_symbol *s, int argc, t_atom *argv)
{
    t_array_min *x = array_rangeop_new(array_min_class, s, &argc, &argv,
        0, 1, 1);
    x->x_out1 = outlet_new(&x->x_rangeop.x_tc.tc_obj, &s_float);
    x->x_out2 = outlet_new(&x->x_rangeop.x_tc.tc_obj, &s_float);
    return (x);
}

static void array_min_bang(t_array_min *x)
{
    char *itemp, *firstitem;
    int stride, nitem, i, besti;
    t_float bestf;
    if (!array_rangeop_getrange(&x->x_rangeop, &firstitem, &nitem, &stride))
        return;
    for (i = 0, besti = 0, bestf= 1e30, itemp = firstitem;
        i < nitem; i++, itemp += stride)
            if (*(t_float *)itemp < bestf)
                bestf = *(t_float *)itemp, besti = i;
    outlet_float(x->x_out2, besti+x->x_onset);
    outlet_float(x->x_out1, bestf);
}

static void array_min_float(t_array_min *x, t_floatarg f)
{
    x->x_onset = f;
    array_min_bang(x);
}

/* overall creator for "array" objects - dispatch to "array define" etc */
static void *arrayobj_new(t_symbol *s, int argc, t_atom *argv)
{
    if (!argc || argv[0].a_type != A_SYMBOL)
        newest = array_define_new(s, argc, argv);
    else
    {
        char *str = argv[0].a_w.w_symbol->s_name;
        if (!strcmp(str, "d") || !strcmp(str, "define"))
            newest = array_define_new(s, argc-1, argv+1);
        else if (!strcmp(str, "size"))
            newest = array_size_new(s, argc-1, argv+1);
        else if (!strcmp(str, "sum"))
            newest = array_sum_new(s, argc-1, argv+1);
        else if (!strcmp(str, "get"))
            newest = array_get_new(s, argc-1, argv+1);
        else if (!strcmp(str, "set"))
            newest = array_set_new(s, argc-1, argv+1);
        else if (!strcmp(str, "quantile"))
            newest = array_quantile_new(s, argc-1, argv+1);
        else if (!strcmp(str, "random"))
            newest = array_random_new(s, argc-1, argv+1);
        else if (!strcmp(str, "max"))
            newest = array_max_new(s, argc-1, argv+1);
        else if (!strcmp(str, "min"))
            newest = array_min_new(s, argc-1, argv+1);
        else 
        {
            error("array %s: unknown function", str);
            newest = 0;
        }
    }
    return (newest);
}

void canvas_add_for_class(t_class *c);

/* ---------------- global setup function -------------------- */

void x_array_setup(void )
{
    array_define_class = class_new(gensym("array define"), 0,
        (t_method)canvas_free, sizeof(t_canvas), 0, 0);
    canvas_add_for_class(array_define_class);
    class_addmethod(array_define_class, (t_method)array_define_send,
        gensym("send"), A_SYMBOL, 0);
    class_addanything(array_define_class, array_define_anything);
    class_sethelpsymbol(array_define_class, gensym("array-object"));
    class_setsavefn(array_define_class, array_define_save);

    class_addmethod(array_define_class, (t_method)array_define_ignore,
        gensym("editmode"), A_GIMME, 0);

    class_addcreator((t_newmethod)arrayobj_new, gensym("array"), A_GIMME, 0);

    class_addcreator((t_newmethod)table_new, gensym("table"),
        A_DEFSYM, A_DEFFLOAT, 0);

    array_size_class = class_new(gensym("array size"),
        (t_newmethod)array_size_new, (t_method)array_client_free,
            sizeof(t_array_size), 0, A_GIMME, 0);
    class_addbang(array_size_class, array_size_bang);
    class_addfloat(array_size_class, array_size_float);
    class_sethelpsymbol(array_size_class, gensym("array-object"));

    array_sum_class = class_new(gensym("array sum"),
        (t_newmethod)array_sum_new, (t_method)array_client_free,
            sizeof(t_array_sum), 0, A_GIMME, 0);
    class_addbang(array_sum_class, array_sum_bang);
    class_addfloat(array_sum_class, array_sum_float);
    class_sethelpsymbol(array_sum_class, gensym("array-object"));

    array_get_class = class_new(gensym("array get"),
        (t_newmethod)array_get_new, (t_method)array_client_free,
            sizeof(t_array_get), 0, A_GIMME, 0);
    class_addbang(array_get_class, array_get_bang);
    class_addfloat(array_get_class, array_get_float);
    class_sethelpsymbol(array_get_class, gensym("array-object"));

    array_set_class = class_new(gensym("array set"),
        (t_newmethod)array_set_new, (t_method)array_client_free,
            sizeof(t_array_set), 0, A_GIMME, 0);
    class_addlist(array_set_class, array_set_list);
    class_sethelpsymbol(array_set_class, gensym("array-object"));

    array_quantile_class = class_new(gensym("array quantile"),
        (t_newmethod)array_quantile_new, (t_method)array_client_free,
            sizeof(t_array_quantile), 0, A_GIMME, 0);
    class_addfloat(array_quantile_class, array_quantile_float);
    class_sethelpsymbol(array_quantile_class, gensym("array-object"));

    array_random_class = class_new(gensym("array random"),
        (t_newmethod)array_random_new, (t_method)array_client_free,
            sizeof(t_array_random), 0, A_GIMME, 0);
    class_addmethod(array_random_class, (t_method)array_random_seed,
        gensym("seed"), A_FLOAT, 0);
    class_addfloat(array_random_class, array_random_float);
    class_addbang(array_random_class, array_random_bang);
    class_sethelpsymbol(array_random_class, gensym("array-object"));

    array_max_class = class_new(gensym("array max"),
        (t_newmethod)array_max_new, (t_method)array_client_free,
            sizeof(t_array_max), 0, A_GIMME, 0);
    class_addfloat(array_max_class, array_max_float);
    class_addbang(array_max_class, array_max_bang);
    class_sethelpsymbol(array_max_class, gensym("array-object"));

    array_min_class = class_new(gensym("array min"),
        (t_newmethod)array_min_new, (t_method)array_client_free,
            sizeof(t_array_min), 0, A_GIMME, 0);
    class_addfloat(array_min_class, array_min_float);
    class_addbang(array_min_class, array_min_bang);
    class_sethelpsymbol(array_min_class, gensym("array-object"));
}
