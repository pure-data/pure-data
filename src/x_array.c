/* Copyright (c) 1997-1999 Miller Puckette and others.
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
static t_class *text_define_class;

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

/* --"table" - classic"array define" object by Guenter Geiger --*/

static int tabcount = 0;

static void *table_new(t_symbol *s, t_floatarg f)
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
    if (f <= 1)
        f = 100;
    SETFLOAT(a, 0);
    SETFLOAT(a+1, 50);
    SETFLOAT(a+2, 600);
    SETFLOAT(a+3, 400);
    SETSYMBOL(a+4, s);
    SETFLOAT(a+5, 0);
    x = canvas_new(0, 0, 6, a);

    x->gl_owner = z;

        /* create a graph for the table */
    gl = glist_addglist((t_glist*)x, &s_, 0, -1, (f > 1 ? f-1 : 1), 1,
        50, 350, 550, 50);

    graph_array(gl, s, &s_float, f, 0);

    newest = &x->gl_pd;     /* mimic action of canvas_pop() */
    pd_popsym(&x->gl_pd);
    x->gl_loading = 0;

    return (x);
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

static void *array_define_new(t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *arrayname = &s_;
    float arraysize = 100;
    while (ac && av->a_type == A_SYMBOL && *av->a_w.w_symbol->s_name == '-')
    {
        {
            pd_error(x, "text define: unknown flag ...");
            postatom(ac, av);
        }
        ac--; av++;
    }
    if (ac && av->a_type == A_SYMBOL)
    {
        arrayname = av->a_w.w_symbol;
        ac--; av++;
    }
    if (ac && av->a_type == A_FLOAT)
    {
        arraysize = av->a_w.w_float;
        ac--; av++;
    }
    if (ac)
    {
        post("warning: text define ignoring extra argument: ");
        postatom(ac, av);
    }
    return (table_new(arrayname, arraysize));
}


/* ---  array_client - common code for objects that refer to arrays -- */

typedef struct _array_client
{
    t_object tc_obj;
    t_symbol *tc_sym;
    t_gpointer tc_gp;
    t_symbol *tc_struct;
    t_symbol *tc_field;
} t_array_client;

    /* find the array for this object.  Prints an error  message and returns
        0 on failure. */
static t_binbuf *array_client_getbuf(t_array_client *x)
{
    if (x->tc_sym)       /* named text object */
    {
        t_textbuf *y = (t_textbuf *)pd_findbyclass(x->tc_sym,
            text_define_class);
        if (y)
            return (y->b_binbuf);
        else
        {
            pd_error(x, "text: couldn't find text buffer '%s'",
                x->tc_sym->s_name);
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
            pd_error(x, "text: couldn't find struct %s", x->tc_struct->s_name);
            return (0);
        }
        if (!gpointer_check(&x->tc_gp, 0))
        {
            pd_error(x, "text: stale or empty pointer");
            return (0);
        }
        if (gs->gs_which == GP_ARRAY)
            vec = x->tc_gp.gp_un.gp_w;
        else vec = x->tc_gp.gp_un.gp_scalar->sc_vec;

        if (!template_find_field(template,
            x->tc_field, &onset, &type, &arraytype))
        {
            pd_error(x, "text: no field named %s", x->tc_field->s_name);
            return (0);
        }
        if (type != DT_LIST)
        {
            pd_error(x, "text: field %s not of type list", x->tc_field->s_name);
            return (0);
        }
        return (*(t_binbuf **)(((char *)vec) + onset));
    }
    else return (0);    /* shouldn't happen */
}

static  void array_client_senditup(t_array_client *x)
{
    if (x->tc_sym)       /* named text object */
    {
        t_textbuf *y = (t_textbuf *)pd_findbyclass(x->tc_sym,
            text_define_class);
        if (y)
            textbuf_senditup(y);
        else bug("array_client_senditup");
    }
    else if (x->tc_struct)   /* by pointer */
    {
        /* figure this out LATER */
    }
}

static void array_client_free(t_array_client *x)
{
    gpointer_unset(&x->tc_gp);
}

/* ----------  array size : get or set size of an array ---------------- */

static void *array_size_new(t_symbol *s, int argc, t_atom *argv)
{
    t_array_client *x = (t_text_setline *)pd_new(text_setline_class);
    x->x_sym = x->x_struct = x->x_field = 0;
    gpointer_init(&x->x_gp);
    while (ac && av->a_type == A_SYMBOL && *av->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(av->a_w.w_symbol->s_name, "-s") &&
            ac >= 3 && av[1].a_type == A_SYMBOL && av[2].a_type == A_SYMBOL)
        {
            x->x_struct = canvas_makebindsym(av[1].a_w.w_symbol);
            x->x_field = av[2].a_w.w_symbol;
            ac -= 2; av += 2;
        }
        else
        {
            pd_error(x, "text setline: unknown flag ...");
            postatom(ac, av);
        }
        ac--; av++;
    }
    if (ac && av->a_type == A_SYMBOL)
    {
        if (x->x_struct)
        {
            pd_error(x, "text setline: extra names after -s..");
            postatom(ac, av);
        }
        else x->x_sym = av->a_w.w_symbol;
        ac--; av++;
    }
    if (ac)
    {
        post("warning: text setline ignoring extra argument: ");
        postatom(ac, av);
    }
    if (x->x_struct)
        pointerinlet_new(&x->x_obj, &x->x_gp);
    return (x);
}

/* overall creator for "text" objects - dispatch to "text define" etc */
static void *text_new(t_symbol *s, int argc, t_atom *argv)
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
        else 
        {
            error("array %s: unknown function", str);
            newest = 0;
        }
    }
    return (newest);
}


/* ---------------- global setup function -------------------- */

void x_array_setup(void )
{

    class_addcreator((t_newmethod)array_new, gensym("array"), A_GIMME, 0);

    class_addcreator((t_newmethod)table_new, gensym("table"),
        A_DEFSYM, A_DEFFLOAT, 0);

}
