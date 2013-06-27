/* Copyright (c) 1997-2013 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* The "scalar" object. */

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
extern t_pd *newest;

t_class *scalar_define_class;

static void *scalar_define_new(t_symbol *s, int argc, t_atom *argv)
{
    t_atom a[9];
    t_glist *gl;
    t_canvas *x, *z = canvas_getcurrent();
    t_symbol *templatesym = &s_float;
    t_template *template;
    t_scalar *sc;
    while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        {
            error("scalar define: unknown flag ...");
            postatom(argc, argv);
        }
        argc--; argv++;
    }
    if (argc && argv->a_type == A_SYMBOL)
    {
        templatesym = argv->a_w.w_symbol;
        argc--; argv++;
    }
    if (argc)
    {
        post("warning: scalar define ignoring extra argument: ");
        postatom(argc, argv);
    }
    
        /* make a canvas... */
    SETFLOAT(a, 0);
    SETFLOAT(a+1, 50);
    SETFLOAT(a+2, 600);
    SETFLOAT(a+3, 400);
    SETSYMBOL(a+4, s);
    SETFLOAT(a+5, 0);
    x = canvas_new(0, 0, 6, a);

    x->gl_owner = z;

        /* put a scalar in it */
    template = template_findbyname(canvas_makebindsym(templatesym));
    if (!template)
    {
        pd_error(x, "scalar define: couldn't find template %s",
            templatesym->s_name);
        goto noscalar;
    }
    sc = scalar_new(x, canvas_makebindsym(templatesym));
    if (!sc)
    {
        pd_error(x, "%s: couldn't create scalar", templatesym->s_name);
        goto noscalar;
    }
    sc->sc_gobj.g_next = 0;
    x->gl_list = &sc->sc_gobj;

noscalar:
    newest = &x->gl_pd;     /* mimic action of canvas_pop() */
    pd_popsym(&x->gl_pd);
    x->gl_loading = 0;
    
        /* bash the class to "scalar define" -- see comment in x_array,c */
    x->gl_obj.ob_pd = scalar_define_class;
    return (x);
}

/* overall creator for "scalar" objects - dispatch to "scalar define" etc */
static void *scalarobj_new(t_symbol *s, int argc, t_atom *argv)
{
    if (!argc || argv[0].a_type != A_SYMBOL)
        newest = scalar_define_new(s, argc, argv);
    else
    {
        char *str = argv[0].a_w.w_symbol->s_name;
        if (!strcmp(str, "d") || !strcmp(str, "define"))
            newest = scalar_define_new(s, argc-1, argv+1);
        else 
        {
            error("scalar %s: unknown function", str);
            newest = 0;
        }
    }
    return (newest);
}

    /* send a pointer to the scalar to whomever is bound to the symbol */
static void scalar_define_s(t_glist *x, t_symbol *s)
{
    t_glist *gl = (x->gl_list ? pd_checkglist(&x->gl_list->g_pd) : 0);
    if (!s->s_thing)
        pd_error(x, "scalar_define_s: %s: no such object", s->s_name);
    else if (gl && gl->gl_list && pd_class(&gl->gl_list->g_pd) == scalar_class)
    {
        t_gpointer gp;
        gpointer_init(&gp);
        gpointer_setglist(&gp, gl, (t_scalar *)&gl->gl_list->g_pd);
        pd_pointer(s->s_thing, &gp);
        gpointer_unset(&gp);
    }
    else bug("scalar_define_s");
}

void canvas_add_for_class(t_class *c);

/* ---------------- global setup function -------------------- */

void x_scalar_setup(void )
{
    scalar_define_class = class_new(gensym("scalar define"), 0,
        (t_method)canvas_free, sizeof(t_canvas), 0, 0);
    canvas_add_for_class(scalar_define_class);
    class_addmethod(scalar_define_class, (t_method)scalar_define_s,
        gensym("s"), A_SYMBOL, 0);
    class_sethelpsymbol(scalar_define_class, gensym("scalar-object"));

    class_addcreator((t_newmethod)scalarobj_new, gensym("scalar"), A_GIMME, 0);

}
