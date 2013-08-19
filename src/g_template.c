/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "m_pd.h"
#include "s_stuff.h"    /* for sys_hostfontsize */
#include "g_canvas.h"

/*
This file contains text objects you would put in a canvas to define a
template.  Templates describe objects of type "array" (g_array.c) and
"scalar" (g_scalar.c).
*/

    /* the structure of a "struct" object (also the obsolete "gtemplate"
    you get when using the name "template" in a box.) */

struct _gtemplate
{
    t_object x_obj;
    t_template *x_template;
    t_canvas *x_owner;
    t_symbol *x_sym;
    struct _gtemplate *x_next;
    int x_argc;
    t_atom *x_argv;
};

/* ---------------- forward definitions ---------------- */

static void template_conformarray(t_template *tfrom, t_template *tto,
    int *conformaction, t_array *a);
static void template_conformglist(t_template *tfrom, t_template *tto,
    t_glist *glist,  int *conformaction);

/* ---------------------- storage ------------------------- */

static t_class *gtemplate_class;
static t_class *template_class;

/* there's a pre-defined "float" template.  LATER should we bind this
to a symbol such as "pd-float"??? */

    /* return true if two dataslot definitions match */
static int dataslot_matches(t_dataslot *ds1, t_dataslot *ds2,
    int nametoo)
{
    return ((!nametoo || ds1->ds_name == ds2->ds_name) &&
        ds1->ds_type == ds2->ds_type &&
            (ds1->ds_type != DT_ARRAY ||
                ds1->ds_arraytemplate == ds2->ds_arraytemplate));
}

/* -- templates, the active ingredient in gtemplates defined below. ------- */

t_template *template_new(t_symbol *templatesym, int argc, t_atom *argv)
{
    t_template *x = (t_template *)pd_new(template_class);
    x->t_n = 0;
    x->t_vec = (t_dataslot *)t_getbytes(0);
    while (argc > 0)
    {
        int newtype, oldn, newn;
        t_symbol *newname, *newarraytemplate = &s_, *newtypesym;
        if (argc < 2 || argv[0].a_type != A_SYMBOL ||
            argv[1].a_type != A_SYMBOL)
                goto bad;
        newtypesym = argv[0].a_w.w_symbol;
        newname = argv[1].a_w.w_symbol;
        if (newtypesym == &s_float)
            newtype = DT_FLOAT;
        else if (newtypesym == &s_symbol)
            newtype = DT_SYMBOL;
                /* "list" is old name.. accepted here but never saved as such */
        else if (newtypesym == gensym("text") || newtypesym == &s_list)
            newtype = DT_TEXT;
        else if (newtypesym == gensym("array"))
        {
            if (argc < 3 || argv[2].a_type != A_SYMBOL)
            {
                pd_error(x, "array lacks element template or name");
                goto bad;
            }
            newarraytemplate = canvas_makebindsym(argv[2].a_w.w_symbol);
            newtype = DT_ARRAY;
            argc--;
            argv++;
        }
        else
        {
            pd_error(x, "%s: no such type", newtypesym->s_name);
            goto bad;
        }
        newn = (oldn = x->t_n) + 1;
        x->t_vec = (t_dataslot *)t_resizebytes(x->t_vec,
            oldn * sizeof(*x->t_vec), newn * sizeof(*x->t_vec));
        x->t_n = newn;
        x->t_vec[oldn].ds_type = newtype;
        x->t_vec[oldn].ds_name = newname;
        x->t_vec[oldn].ds_arraytemplate = newarraytemplate;
    bad: 
        argc -= 2; argv += 2;
    }
    if (*templatesym->s_name)
    {
        x->t_sym = templatesym;
        pd_bind(&x->t_pdobj, x->t_sym);
    }
    else x->t_sym = templatesym;
    return (x);
}

int template_size(t_template *x)
{
    return (x->t_n * sizeof(t_word));
}

int template_find_field(t_template *x, t_symbol *name, int *p_onset,
    int *p_type, t_symbol **p_arraytype)
{
    t_template *t;
    int i, n;
    if (!x)
    {
        bug("template_find_field");
        return (0);
    }
    n = x->t_n;
    for (i = 0; i < n; i++)
        if (x->t_vec[i].ds_name == name)
    {
        *p_onset = i * sizeof(t_word);
        *p_type = x->t_vec[i].ds_type;
        *p_arraytype = x->t_vec[i].ds_arraytemplate;
        return (1);
    }
    return (0);
}

t_float template_getfloat(t_template *x, t_symbol *fieldname, t_word *wp,
    int loud)
{
    int onset, type;
    t_symbol *arraytype;
    t_float val = 0;
    if (template_find_field(x, fieldname, &onset, &type, &arraytype))
    {
        if (type == DT_FLOAT)
            val = *(t_float *)(((char *)wp) + onset);
        else if (loud) error("%s.%s: not a number",
            x->t_sym->s_name, fieldname->s_name);
    }
    else if (loud) error("%s.%s: no such field",
        x->t_sym->s_name, fieldname->s_name);
    return (val);
}

void template_setfloat(t_template *x, t_symbol *fieldname, t_word *wp, 
    t_float f, int loud)
{
    int onset, type;
    t_symbol *arraytype;
    if (template_find_field(x, fieldname, &onset, &type, &arraytype))
     {
        if (type == DT_FLOAT)
            *(t_float *)(((char *)wp) + onset) = f;
        else if (loud) error("%s.%s: not a number",
            x->t_sym->s_name, fieldname->s_name);
    }
    else if (loud) error("%s.%s: no such field",
        x->t_sym->s_name, fieldname->s_name);
}

t_symbol *template_getsymbol(t_template *x, t_symbol *fieldname, t_word *wp,
    int loud)
{
    int onset, type;
    t_symbol *arraytype;
    t_symbol *val = &s_;
    if (template_find_field(x, fieldname, &onset, &type, &arraytype))
    {
        if (type == DT_SYMBOL)
            val = *(t_symbol **)(((char *)wp) + onset);
        else if (loud) error("%s.%s: not a symbol",
            x->t_sym->s_name, fieldname->s_name);
    }
    else if (loud) error("%s.%s: no such field",
        x->t_sym->s_name, fieldname->s_name);
    return (val);
}

void template_setsymbol(t_template *x, t_symbol *fieldname, t_word *wp, 
    t_symbol *s, int loud)
{
    int onset, type;
    t_symbol *arraytype;
    if (template_find_field(x, fieldname, &onset, &type, &arraytype))
     {
        if (type == DT_SYMBOL)
            *(t_symbol **)(((char *)wp) + onset) = s;
        else if (loud) error("%s.%s: not a symbol",
            x->t_sym->s_name, fieldname->s_name);
    }
    else if (loud) error("%s.%s: no such field",
        x->t_sym->s_name, fieldname->s_name);
}

    /* stringent check to see if a "saved" template, x2, matches the current
        one (x1).  It's OK if x1 has additional scalar elements but not (yet)
        arrays.  This is used for reading in "data files". */
int template_match(t_template *x1, t_template *x2)
{
    int i;
    if (x1->t_n < x2->t_n)
        return (0);
    for (i = x2->t_n; i < x1->t_n; i++)
    {
        if (x1->t_vec[i].ds_type == DT_ARRAY)
                return (0);
    }
    if (x2->t_n > x1->t_n)
        post("add elements...");
    for (i = 0; i < x2->t_n; i++)
        if (!dataslot_matches(&x1->t_vec[i], &x2->t_vec[i], 1))
            return (0);
    return (1);
}

/* --------------- CONFORMING TO CHANGES IN A TEMPLATE ------------ */

/* the following routines handle updating scalars to agree with changes
in their template.  The old template is assumed to be the "installed" one
so we can delete old items; but making new ones we have to avoid scalar_new
which would make an old one whereas we will want a new one (but whose array
elements might still be old ones.)
    LATER deal with graphics updates too... */

    /* conform the word vector of a scalar to the new template */    
static void template_conformwords(t_template *tfrom, t_template *tto,
    int *conformaction, t_word *wfrom, t_word *wto)
{
    int nfrom = tfrom->t_n, nto = tto->t_n, i;
    for (i = 0; i < nto; i++)
    {
        if (conformaction[i] >= 0)
        {
                /* we swap the two, in case it's an array or list, so that
                when "wfrom" is deleted the old one gets cleaned up. */
            t_word wwas = wto[i];
            wto[i] = wfrom[conformaction[i]];
            wfrom[conformaction[i]] = wwas;
        }
    }
}

    /* conform a scalar, recursively conforming arrays  */
static t_scalar *template_conformscalar(t_template *tfrom, t_template *tto,
    int *conformaction, t_glist *glist, t_scalar *scfrom)
{
    t_scalar *x;
    t_gpointer gp;
    int nto = tto->t_n, nfrom = tfrom->t_n, i;
    t_template *scalartemplate;
    /* post("conform scalar"); */
        /* possibly replace the scalar */
    if (scfrom->sc_template == tfrom->t_sym)
    {
            /* see scalar_new() for comment about the gpointer. */
        gpointer_init(&gp);
        x = (t_scalar *)getbytes(sizeof(t_scalar) +
            (tto->t_n - 1) * sizeof(*x->sc_vec));
        x->sc_gobj.g_pd = scalar_class;
        x->sc_template = tfrom->t_sym;
        gpointer_setglist(&gp, glist, x);
            /* Here we initialize to the new template, but array and list
            elements will still belong to old template. */
        word_init(x->sc_vec, tto, &gp);

        template_conformwords(tfrom, tto, conformaction,
            scfrom->sc_vec, x->sc_vec);
            
            /* replace the old one with the new one in the list */
        if (glist->gl_list == &scfrom->sc_gobj)
        {
            glist->gl_list = &x->sc_gobj;
            x->sc_gobj.g_next = scfrom->sc_gobj.g_next;
        }
        else
        {
            t_gobj *y, *y2;
            for (y = glist->gl_list; y2 = y->g_next; y = y2)
                if (y2 == &scfrom->sc_gobj)
            {
                x->sc_gobj.g_next = y2->g_next;
                y->g_next = &x->sc_gobj;
                goto nobug;
            }
            bug("template_conformscalar");
        nobug: ;
        }
            /* burn the old one */
        pd_free(&scfrom->sc_gobj.g_pd);
        scalartemplate = tto;
    }
    else
    {
        x = scfrom;
        scalartemplate = template_findbyname(x->sc_template);
    }
        /* convert all array elements */
    for (i = 0; i < scalartemplate->t_n; i++)
    {
        t_dataslot *ds = scalartemplate->t_vec + i;
        if (ds->ds_type == DT_ARRAY)
        {
            template_conformarray(tfrom, tto, conformaction, 
                x->sc_vec[i].w_array);
        }
    }
    return (x);
}

    /* conform an array, recursively conforming sublists and arrays  */
static void template_conformarray(t_template *tfrom, t_template *tto,
    int *conformaction, t_array *a)
{
    int i, j;
    t_template *scalartemplate = 0;
    if (a->a_templatesym == tfrom->t_sym)
    {
        /* the array elements must all be conformed */
        int oldelemsize = sizeof(t_word) * tfrom->t_n,
            newelemsize = sizeof(t_word) * tto->t_n;
        char *newarray = getbytes(newelemsize * a->a_n);
        char *oldarray = a->a_vec;
        if (a->a_elemsize != oldelemsize)
            bug("template_conformarray");
        for (i = 0; i < a->a_n; i++)
        {
            t_word *wp = (t_word *)(newarray + newelemsize * i);
            word_init(wp, tto, &a->a_gp);
            template_conformwords(tfrom, tto, conformaction,
                (t_word *)(oldarray + oldelemsize * i), wp);
            word_free((t_word *)(oldarray + oldelemsize * i), tfrom);
        }
        scalartemplate = tto;
        a->a_vec = newarray;
        freebytes(oldarray, oldelemsize * a->a_n);
    }
    else scalartemplate = template_findbyname(a->a_templatesym);
        /* convert all arrays and sublist fields in each element of the array */
    for (i = 0; i < a->a_n; i++)
    {
        t_word *wp = (t_word *)(a->a_vec + sizeof(t_word) * a->a_n * i);
        for (j = 0; j < scalartemplate->t_n; j++)
        {
            t_dataslot *ds = scalartemplate->t_vec + j;
            if (ds->ds_type == DT_ARRAY)
            {
                template_conformarray(tfrom, tto, conformaction, 
                    wp[j].w_array);
            }
        }
    }
}

    /* this routine searches for every scalar in the glist that belongs
    to the "from" template and makes it belong to the "to" template.  Descend
    glists recursively.
    We don't handle redrawing here; this is to be filled in LATER... */

t_array *garray_getarray(t_garray *x);

static void template_conformglist(t_template *tfrom, t_template *tto,
    t_glist *glist,  int *conformaction)
{
    t_gobj *g;
    /* post("conform glist %s", glist->gl_name->s_name); */
    for (g = glist->gl_list; g; g = g->g_next)
    {
        if (pd_class(&g->g_pd) == scalar_class)
            g = &template_conformscalar(tfrom, tto, conformaction,
                glist, (t_scalar *)g)->sc_gobj;
        else if (pd_class(&g->g_pd) == canvas_class)
            template_conformglist(tfrom, tto, (t_glist *)g, conformaction);
        else if (pd_class(&g->g_pd) == garray_class)
            template_conformarray(tfrom, tto, conformaction,
                garray_getarray((t_garray *)g));
    }
}

    /* globally conform all scalars from one template to another */ 
void template_conform(t_template *tfrom, t_template *tto)
{
    int nto = tto->t_n, nfrom = tfrom->t_n, i, j,
        *conformaction = (int *)getbytes(sizeof(int) * nto),
        *conformedfrom = (int *)getbytes(sizeof(int) * nfrom), doit = 0;
    for (i = 0; i < nto; i++)
        conformaction[i] = -1;
    for (i = 0; i < nfrom; i++)
        conformedfrom[i] = 0;
    for (i = 0; i < nto; i++)
    {
        t_dataslot *dataslot = &tto->t_vec[i];
        for (j = 0; j < nfrom; j++)
        {
            t_dataslot *dataslot2 = &tfrom->t_vec[j];
            if (dataslot_matches(dataslot, dataslot2, 1))
            {
                conformaction[i] = j;
                conformedfrom[j] = 1;
            }
        }
    }
    for (i = 0; i < nto; i++)
        if (conformaction[i] < 0)
    {
        t_dataslot *dataslot = &tto->t_vec[i];
        for (j = 0; j < nfrom; j++)
            if (!conformedfrom[j] &&
                dataslot_matches(dataslot, &tfrom->t_vec[j], 0))
        {
            conformaction[i] = j;
            conformedfrom[j] = 1;
        }
    }
    if (nto != nfrom)
        doit = 1;
    else for (i = 0; i < nto; i++)
        if (conformaction[i] != i)
            doit = 1;

    if (doit)
    {
        t_glist *gl;
        for (gl = canvas_list; gl; gl = gl->gl_next)
            template_conformglist(tfrom, tto, gl, conformaction);
    }
    freebytes(conformaction, sizeof(int) * nto);
    freebytes(conformedfrom, sizeof(int) * nfrom);
}

t_template *template_findbyname(t_symbol *s)
{
    return ((t_template *)pd_findbyclass(s, template_class));
}

t_canvas *template_findcanvas(t_template *template)
{
    t_gtemplate *gt;
    if (!template) 
        bug("template_findcanvas");
    if (!(gt = template->t_list))
        return (0);
    return (gt->x_owner);
    /* return ((t_canvas *)pd_findbyclass(template->t_sym, canvas_class)); */
}

void template_notify(t_template *template, t_symbol *s, int argc, t_atom *argv)
{
    if (template->t_list)
        outlet_anything(template->t_list->x_obj.ob_outlet, s, argc, argv);
}

    /* bash the first of (argv) with a pointer to a scalar, and send on
    to template as a notification message */
void template_notifyforscalar(t_template *template, t_glist *owner,
    t_scalar *sc, t_symbol *s, int argc, t_atom *argv)
{
    t_gpointer gp;
    gpointer_init(&gp);
    gpointer_setglist(&gp, owner, sc);
    SETPOINTER(argv, &gp);
    template_notify(template, s, argc, argv);
    gpointer_unset(&gp);
}

    /* call this when reading a patch from a file to declare what templates
    we'll need.  If there's already a template, check if it matches.
    If it doesn't it's still OK as long as there are no "struct" (gtemplate)
    objects hanging from it; we just conform everyone to the new template.
    If there are still struct objects belonging to the other template, we're
    in trouble.  LATER we'll figure out how to conform the new patch's objects
    to the pre-existing struct. */
static void *template_usetemplate(void *dummy, t_symbol *s,
    int argc, t_atom *argv)
{
    t_template *x;
    t_symbol *templatesym =
        canvas_makebindsym(atom_getsymbolarg(0, argc, argv));
    if (!argc)
        return (0);
    argc--; argv++;
            /* check if there's already a template by this name. */
    if ((x = (t_template *)pd_findbyclass(templatesym, template_class)))
    {
        t_template *y = template_new(&s_, argc, argv), *y2;
            /* If the new template is the same as the old one,
            there's nothing to do.  */
        if (!template_match(x, y))
        {
                /* Are there "struct" objects upholding this template? */
            if (x->t_list)
            {
                    /* don't know what to do here! */
                error("%s: template mismatch",
                    templatesym->s_name);
            }
            else
            {
                    /* conform everyone to the new template */
                template_conform(x, y);
                pd_free(&x->t_pdobj);
                y2 = template_new(templatesym, argc, argv);
                y2->t_list = 0;
            }
        }
        pd_free(&y->t_pdobj);
    }
        /* otherwise, just make one. */
    else template_new(templatesym, argc, argv);
    return (0);
}

    /* here we assume someone has already cleaned up all instances of this. */
void template_free(t_template *x)
{
    if (*x->t_sym->s_name)
        pd_unbind(&x->t_pdobj, x->t_sym);
    t_freebytes(x->t_vec, x->t_n * sizeof(*x->t_vec));
}

static void template_setup(void)
{
    template_class = class_new(gensym("template"), 0, (t_method)template_free,
        sizeof(t_template), CLASS_PD, 0);
    class_addmethod(pd_canvasmaker, (t_method)template_usetemplate,
        gensym("struct"), A_GIMME, 0);
        
}

/* ---------------- gtemplates.  One per canvas. ----------- */

/* "Struct": an object that searches for, and if necessary creates, 
a template (above).  Other objects in the canvas then can give drawing
instructions for the template.  The template doesn't go away when the
"struct" is deleted, so that you can replace it with
another one to add new fields, for example. */

static void *gtemplate_donew(t_symbol *sym, int argc, t_atom *argv)
{
    t_gtemplate *x = (t_gtemplate *)pd_new(gtemplate_class);
    t_template *t = template_findbyname(sym);
    int i;
    t_symbol *sx = gensym("x");
    x->x_owner = canvas_getcurrent();
    x->x_next = 0;
    x->x_sym = sym;
    x->x_argc = argc;
    x->x_argv = (t_atom *)getbytes(argc * sizeof(t_atom));
    for (i = 0; i < argc; i++)
        x->x_argv[i] = argv[i];

        /* already have a template by this name? */
    if (t)
    {
        x->x_template = t;
            /* if it's already got a "struct" object we
            just tack this one to the end of the list and leave it
            there. */
        if (t->t_list)
        {
            t_gtemplate *x2, *x3;
            for (x2 = x->x_template->t_list; x3 = x2->x_next; x2 = x3)
                ;
            x2->x_next = x;
            post("template %s: warning: already exists.", sym->s_name);
        }
        else
        {
                /* if there's none, we just replace the template with
                our own and conform it. */
            t_template *y = template_new(&s_, argc, argv);
            canvas_redrawallfortemplate(t, 2);
                /* Unless the new template is different from the old one,
                there's nothing to do.  */
            if (!template_match(t, y))
            {
                    /* conform everyone to the new template */
                template_conform(t, y);
                pd_free(&t->t_pdobj);
                t = template_new(sym, argc, argv);
            }
            pd_free(&y->t_pdobj);
            t->t_list = x;
            canvas_redrawallfortemplate(t, 1);
        }
    }
    else
    {
            /* otherwise make a new one and we're the only struct on it. */
        x->x_template = t = template_new(sym, argc, argv);
        t->t_list = x;
    }
    outlet_new(&x->x_obj, 0);
    return (x);
}

static void *gtemplate_new(t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *sym = atom_getsymbolarg(0, argc, argv);
    if (argc >= 1)
        argc--; argv++;
    return (gtemplate_donew(canvas_makebindsym(sym), argc, argv));
}

    /* old version (0.34) -- delete 2003 or so */
static void *gtemplate_new_old(t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *sym = canvas_makebindsym(canvas_getcurrent()->gl_name);
    static int warned;
    if (!warned)
    {
        post("warning -- 'template' (%s) is obsolete; replace with 'struct'",
            sym->s_name);
        warned = 1;
    }
    return (gtemplate_donew(sym, argc, argv));
}

t_template *gtemplate_get(t_gtemplate *x)
{
    return (x->x_template);
}

static void gtemplate_free(t_gtemplate *x)
{
        /* get off the template's list */
    t_template *t = x->x_template;
    t_gtemplate *y;
    if (x == t->t_list)
    {
        canvas_redrawallfortemplate(t, 2);
        if (x->x_next)
        {
                /* if we were first on the list, and there are others on
                the list, make a new template corresponding to the new
                first-on-list and replace the existing template with it. */
            t_template *z = template_new(&s_,
                x->x_next->x_argc, x->x_next->x_argv);
            template_conform(t, z);
            pd_free(&t->t_pdobj);
            pd_free(&z->t_pdobj);
            z = template_new(x->x_sym, x->x_next->x_argc, x->x_next->x_argv);
            z->t_list = x->x_next;
            for (y = z->t_list; y ; y = y->x_next)
                y->x_template = z;
        }
        else t->t_list = 0;
        canvas_redrawallfortemplate(t, 1);
    }
    else
    {
        t_gtemplate *x2, *x3;
        for (x2 = t->t_list; x3 = x2->x_next; x2 = x3)
        {
            if (x == x3)
            {
                x2->x_next = x3->x_next;
                break;
            }
        }
    }
    freebytes(x->x_argv, sizeof(t_atom) * x->x_argc);
}

static void gtemplate_setup(void)
{
    gtemplate_class = class_new(gensym("struct"),
        (t_newmethod)gtemplate_new, (t_method)gtemplate_free,
        sizeof(t_gtemplate), CLASS_NOINLET, A_GIMME, 0);
    class_addcreator((t_newmethod)gtemplate_new_old, gensym("template"),
        A_GIMME, 0);
}

/* ---------------  FIELD DESCRIPTORS ---------------------- */

/* a field descriptor can hold a constant or a variable; if a variable,
it's the name of a field in the template we belong to.  LATER, we might
want to cache the offset of the field so we don't have to search for it
every single time we draw the object.
*/

struct _fielddesc
{
    char fd_type;       /* LATER consider removing this? */
    char fd_var;
    union
    {
        t_float fd_float;       /* the field is a constant float */
        t_symbol *fd_symbol;    /* the field is a constant symbol */
        t_symbol *fd_varsym;    /* the field is variable and this is the name */
    } fd_un;
    float fd_v1;        /* min and max values */
    float fd_v2;
    float fd_screen1;   /* min and max screen values */
    float fd_screen2;
    float fd_quantum;   /* quantization in value */ 
};

static void fielddesc_setfloat_const(t_fielddesc *fd, t_float f)
{
    fd->fd_type = A_FLOAT;
    fd->fd_var = 0;
    fd->fd_un.fd_float = f;
    fd->fd_v1 = fd->fd_v2 = fd->fd_screen1 = fd->fd_screen2 =
        fd->fd_quantum = 0;
}

static void fielddesc_setsymbol_const(t_fielddesc *fd, t_symbol *s)
{
    fd->fd_type = A_SYMBOL;
    fd->fd_var = 0;
    fd->fd_un.fd_symbol = s;
    fd->fd_v1 = fd->fd_v2 = fd->fd_screen1 = fd->fd_screen2 =
        fd->fd_quantum = 0;
}

static void fielddesc_setfloat_var(t_fielddesc *fd, t_symbol *s)
{
    char *s1, *s2, *s3, strbuf[MAXPDSTRING];
    int i;
    fd->fd_type = A_FLOAT;
    fd->fd_var = 1;
    if (!(s1 = strchr(s->s_name, '(')) || !(s2 = strchr(s->s_name, ')'))
        || (s1 > s2))
    {
        fd->fd_un.fd_varsym = s;
        fd->fd_v1 = fd->fd_v2 = fd->fd_screen1 = fd->fd_screen2 =
            fd->fd_quantum = 0;
    }
    else
    {
        int cpy = s1 - s->s_name, got;
        double v1, v2, screen1, screen2, quantum;
        if (cpy > MAXPDSTRING-5)
            cpy = MAXPDSTRING-5;
        strncpy(strbuf, s->s_name, cpy);
        strbuf[cpy] = 0;
        fd->fd_un.fd_varsym = gensym(strbuf);
        got = sscanf(s1, "(%lf:%lf)(%lf:%lf)(%lf)",
            &v1, &v2, &screen1, &screen2,
                &quantum);
        fd->fd_v1=v1;
        fd->fd_v2=v2;
        fd->fd_screen1=screen1;
        fd->fd_screen2=screen2;
        fd->fd_quantum=quantum;
        if (got < 2)
            goto fail;
        if (got == 3 || (got < 4 && strchr(s2, '(')))
            goto fail;
        if (got < 5 && (s3 = strchr(s2, '(')) && strchr(s3+1, '('))
            goto fail;
        if (got == 4)
            fd->fd_quantum = 0;
        else if (got == 2)
        {
            fd->fd_quantum = 0;
            fd->fd_screen1 = fd->fd_v1;
            fd->fd_screen2 = fd->fd_v2;
        }
        return;
    fail:
        post("parse error: %s", s->s_name);
        fd->fd_v1 = fd->fd_screen1 = fd->fd_v2 = fd->fd_screen2 =
            fd->fd_quantum = 0;
    }
}

#define CLOSED 1
#define BEZ 2
#define NOMOUSE 4
#define A_ARRAY 55      /* LATER decide whether to enshrine this in m_pd.h */

static void fielddesc_setfloatarg(t_fielddesc *fd, int argc, t_atom *argv)
{
        if (argc <= 0) fielddesc_setfloat_const(fd, 0);
        else if (argv->a_type == A_SYMBOL)
            fielddesc_setfloat_var(fd, argv->a_w.w_symbol);
        else fielddesc_setfloat_const(fd, argv->a_w.w_float);
}

static void fielddesc_setsymbolarg(t_fielddesc *fd, int argc, t_atom *argv)
{
        if (argc <= 0) fielddesc_setsymbol_const(fd, &s_);
        else if (argv->a_type == A_SYMBOL)
        {
            fd->fd_type = A_SYMBOL;
            fd->fd_var = 1;
            fd->fd_un.fd_varsym = argv->a_w.w_symbol;
            fd->fd_v1 = fd->fd_v2 = fd->fd_screen1 = fd->fd_screen2 =
                fd->fd_quantum = 0;
        }
        else fielddesc_setsymbol_const(fd, &s_);
}

static void fielddesc_setarrayarg(t_fielddesc *fd, int argc, t_atom *argv)
{
        if (argc <= 0) fielddesc_setfloat_const(fd, 0);
        else if (argv->a_type == A_SYMBOL)
        {
            fd->fd_type = A_ARRAY;
            fd->fd_var = 1;
            fd->fd_un.fd_varsym = argv->a_w.w_symbol;
        }
        else fielddesc_setfloat_const(fd, argv->a_w.w_float);
}

    /* getting and setting values via fielddescs -- note confusing names;
    the above are setting up the fielddesc itself. */
static t_float fielddesc_getfloat(t_fielddesc *f, t_template *template,
    t_word *wp, int loud)
{
    if (f->fd_type == A_FLOAT)
    {
        if (f->fd_var)
            return (template_getfloat(template, f->fd_un.fd_varsym, wp, loud));
        else return (f->fd_un.fd_float);
    }
    else
    {
        if (loud)
            error("symbolic data field used as number");
        return (0);
    }
}

    /* convert a variable's value to a screen coordinate via its fielddesc */
t_float fielddesc_cvttocoord(t_fielddesc *f, t_float val)
{
    t_float coord, pix, extreme, div;
    if (f->fd_v2 == f->fd_v1)
        return (val);
    div = (f->fd_screen2 - f->fd_screen1)/(f->fd_v2 - f->fd_v1);
    coord = f->fd_screen1 + (val - f->fd_v1) * div;
    extreme = (f->fd_screen1 < f->fd_screen2 ?
        f->fd_screen1 : f->fd_screen2);
    if (coord < extreme)
        coord = extreme;
    extreme = (f->fd_screen1 > f->fd_screen2 ? 
        f->fd_screen1 : f->fd_screen2);
    if (coord > extreme)
        coord = extreme;
    return (coord);
}

    /* read a variable via fielddesc and convert to screen coordinate */
t_float fielddesc_getcoord(t_fielddesc *f, t_template *template,
    t_word *wp, int loud)
{
    if (f->fd_type == A_FLOAT)
    {
        if (f->fd_var)
        {
            t_float val = template_getfloat(template,
                f->fd_un.fd_varsym, wp, loud);
            return (fielddesc_cvttocoord(f, val));
        }
        else return (f->fd_un.fd_float);
    }
    else
    {
        if (loud)
            error("symbolic data field used as number");
        return (0);
    }
}

static t_symbol *fielddesc_getsymbol(t_fielddesc *f, t_template *template,
    t_word *wp, int loud)
{
    if (f->fd_type == A_SYMBOL)
    {
        if (f->fd_var)
            return(template_getsymbol(template, f->fd_un.fd_varsym, wp, loud));
        else return (f->fd_un.fd_symbol);
    }
    else
    {
        if (loud)
            error("numeric data field used as symbol");
        return (&s_);
    }
}

    /* convert from a screen coordinate to a variable value */
t_float fielddesc_cvtfromcoord(t_fielddesc *f, t_float coord)
{
    t_float val;
    if (f->fd_screen2 == f->fd_screen1)
        val = coord;
    else
    {
        t_float div = (f->fd_v2 - f->fd_v1)/(f->fd_screen2 - f->fd_screen1);
        t_float extreme;
        val = f->fd_v1 + (coord - f->fd_screen1) * div;
        if (f->fd_quantum != 0)
            val = ((int)((val/f->fd_quantum) + 0.5)) *  f->fd_quantum;
        extreme = (f->fd_v1 < f->fd_v2 ?
            f->fd_v1 : f->fd_v2);
        if (val < extreme) val = extreme;
        extreme = (f->fd_v1 > f->fd_v2 ?
            f->fd_v1 : f->fd_v2);
        if (val > extreme) val = extreme;
    }
    return (val);
 }

void fielddesc_setcoord(t_fielddesc *f, t_template *template,
    t_word *wp, t_float coord, int loud)
{
    if (f->fd_type == A_FLOAT && f->fd_var)
    {
        t_float val = fielddesc_cvtfromcoord(f, coord);
        template_setfloat(template,
                f->fd_un.fd_varsym, wp, val, loud);
    }
    else
    {
        if (loud)
            error("attempt to set constant or symbolic data field to a number");
    }
}

/* ---------------- curves and polygons (joined segments) ---------------- */

/*
curves belong to templates and describe how the data in the template are to
be drawn.  The coordinates of the curve (and other display features) can
be attached to fields in the template.
*/

t_class *curve_class;

typedef struct _curve
{
    t_object x_obj;
    int x_flags;            /* CLOSED and/or BEZ and/or NOMOUSE */
    t_fielddesc x_fillcolor;
    t_fielddesc x_outlinecolor;
    t_fielddesc x_width;
    t_fielddesc x_vis;
    int x_npoints;
    t_fielddesc *x_vec;
    t_canvas *x_canvas;
} t_curve;

static void *curve_new(t_symbol *classsym, t_int argc, t_atom *argv)
{
    t_curve *x = (t_curve *)pd_new(curve_class);
    char *classname = classsym->s_name;
    int flags = 0;
    int nxy, i;
    t_fielddesc *fd;
    x->x_canvas = canvas_getcurrent();
    if (classname[0] == 'f')
    {
        classname += 6;
        flags |= CLOSED;
    }
    else classname += 4;
    if (classname[0] == 'c') flags |= BEZ;
    fielddesc_setfloat_const(&x->x_vis, 1);
    while (1)
    {
        t_symbol *firstarg = atom_getsymbolarg(0, argc, argv);
        if (!strcmp(firstarg->s_name, "-v") && argc > 1)
        {
            fielddesc_setfloatarg(&x->x_vis, 1, argv+1);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-x"))
        {
            flags |= NOMOUSE;
            argc -= 1; argv += 1;
        }
        else break;
    }
    x->x_flags = flags;
    if ((flags & CLOSED) && argc)
        fielddesc_setfloatarg(&x->x_fillcolor, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_fillcolor, 0); 
    if (argc) fielddesc_setfloatarg(&x->x_outlinecolor, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_outlinecolor, 0);
    if (argc) fielddesc_setfloatarg(&x->x_width, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_width, 1);
    if (argc < 0) argc = 0;
    nxy =  (argc + (argc & 1));
    x->x_npoints = (nxy>>1);
    x->x_vec = (t_fielddesc *)t_getbytes(nxy * sizeof(t_fielddesc));
    for (i = 0, fd = x->x_vec; i < argc; i++, fd++, argv++)
        fielddesc_setfloatarg(fd, 1, argv);
    if (argc & 1) fielddesc_setfloat_const(fd, 0);

    return (x);
}

void curve_float(t_curve *x, t_floatarg f)
{
    int viswas;
    if (x->x_vis.fd_type != A_FLOAT || x->x_vis.fd_var)
    {
        pd_error(x, "global vis/invis for a template with variable visibility");
        return;
    }
    viswas = (x->x_vis.fd_un.fd_float != 0);
    
    if ((f != 0 && viswas) || (f == 0 && !viswas))
        return;
    canvas_redrawallfortemplatecanvas(x->x_canvas, 2);
    fielddesc_setfloat_const(&x->x_vis, (f != 0));
    canvas_redrawallfortemplatecanvas(x->x_canvas, 1);
}

/* -------------------- widget behavior for curve ------------ */

static void curve_getrect(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_curve *x = (t_curve *)z;
    int i, n = x->x_npoints;
    t_fielddesc *f = x->x_vec;
    int x1 = 0x7fffffff, x2 = -0x7fffffff, y1 = 0x7fffffff, y2 = -0x7fffffff;
    if (!fielddesc_getfloat(&x->x_vis, template, data, 0) ||
        (x->x_flags & NOMOUSE))
    {
        *xp1 = *yp1 = 0x7fffffff;
        *xp2 = *yp2 = -0x7fffffff;
        return;
    }
    for (i = 0, f = x->x_vec; i < n; i++, f += 2)
    {
        int xloc = glist_xtopixels(glist,
            basex + fielddesc_getcoord(f, template, data, 0));
        int yloc = glist_ytopixels(glist,
            basey + fielddesc_getcoord(f+1, template, data, 0));
        if (xloc < x1) x1 = xloc;
        if (xloc > x2) x2 = xloc;
        if (yloc < y1) y1 = yloc;
        if (yloc > y2) y2 = yloc;
    }
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2; 
}

static void curve_displace(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int dx, int dy)
{
    /* refuse */
}

static void curve_select(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int state)
{
    /* fill in later */
}

static void curve_activate(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int state)
{
    /* fill in later */
}

#if 0
static int rangecolor(int n)    /* 0 to 9 in 5 steps */
{
    int n2 = n/2;               /* 0 to 4 */
    int ret = (n2 << 6);        /* 0 to 256 in 5 steps */
    if (ret > 255) ret = 255;
    return (ret);
}
#endif

static int rangecolor(int n)    /* 0 to 9 in 5 steps */
{
    int n2 = (n == 9 ? 8 : n);               /* 0 to 8 */
    int ret = (n2 << 5);        /* 0 to 256 in 9 steps */
    if (ret > 255) ret = 255;
    return (ret);
}

static void numbertocolor(int n, char *s)
{
    int red, blue, green;
    if (n < 0) n = 0;
    red = n / 100;
    blue = ((n / 10) % 10);
    green = n % 10;
    sprintf(s, "#%2.2x%2.2x%2.2x", rangecolor(red), rangecolor(blue),
        rangecolor(green));
}

static void curve_vis(t_gobj *z, t_glist *glist, 
    t_word *data, t_template *template, t_float basex, t_float basey,
    int vis)
{
    t_curve *x = (t_curve *)z;
    int i, n = x->x_npoints;
    t_fielddesc *f = x->x_vec;
    
        /* see comment in plot_vis() */
    if (vis && !fielddesc_getfloat(&x->x_vis, template, data, 0))
        return;
    if (vis)
    {
        if (n > 1)
        {
            int flags = x->x_flags, closed = (flags & CLOSED);
            t_float width = fielddesc_getfloat(&x->x_width, template, data, 1);
            char outline[20], fill[20];
            int pix[200];
            if (n > 100)
                n = 100;
                /* calculate the pixel values before we start printing
                out the TK message so that "error" printout won't be
                interspersed with it.  Only show up to 100 points so we don't
                have to allocate memory here. */
            for (i = 0, f = x->x_vec; i < n; i++, f += 2)
            {
                pix[2*i] = glist_xtopixels(glist,
                    basex + fielddesc_getcoord(f, template, data, 1));
                pix[2*i+1] = glist_ytopixels(glist,
                    basey + fielddesc_getcoord(f+1, template, data, 1));
            }
            if (width < 1) width = 1;
            numbertocolor(
                fielddesc_getfloat(&x->x_outlinecolor, template, data, 1),
                outline);
            if (flags & CLOSED)
            {
                numbertocolor(
                    fielddesc_getfloat(&x->x_fillcolor, template, data, 1),
                    fill);
                sys_vgui(".x%lx.c create polygon\\\n",
                    glist_getcanvas(glist));
            }
            else sys_vgui(".x%lx.c create line\\\n", glist_getcanvas(glist));
            for (i = 0; i < n; i++)
                sys_vgui("%d %d\\\n", pix[2*i], pix[2*i+1]);
            sys_vgui("-width %f\\\n", width);
            if (flags & CLOSED) sys_vgui("-fill %s -outline %s\\\n",
                fill, outline);
            else sys_vgui("-fill %s\\\n", outline);
            if (flags & BEZ) sys_vgui("-smooth 1\\\n");
            sys_vgui("-tags curve%lx\n", data);
        }
        else post("warning: curves need at least two points to be graphed");
    }
    else
    {
        if (n > 1) sys_vgui(".x%lx.c delete curve%lx\n",
            glist_getcanvas(glist), data);      
    }
}

static int curve_motion_field;
static t_float curve_motion_xcumulative;
static t_float curve_motion_xbase;
static t_float curve_motion_xper;
static t_float curve_motion_ycumulative;
static t_float curve_motion_ybase;
static t_float curve_motion_yper;
static t_glist *curve_motion_glist;
static t_scalar *curve_motion_scalar;
static t_array *curve_motion_array;
static t_word *curve_motion_wp;
static t_template *curve_motion_template;
static t_gpointer curve_motion_gpointer;

    /* LATER protect against the template changing or the scalar disappearing
    probably by attaching a gpointer here ... */

static void curve_motion(void *z, t_floatarg dx, t_floatarg dy)
{
    t_curve *x = (t_curve *)z;
    t_fielddesc *f = x->x_vec + curve_motion_field;
    t_atom at;
    if (!gpointer_check(&curve_motion_gpointer, 0))
    {
        post("curve_motion: scalar disappeared");
        return;
    }
    curve_motion_xcumulative += dx;
    curve_motion_ycumulative += dy;
    if (f->fd_var && (dx != 0))
    {
        fielddesc_setcoord(f, curve_motion_template, curve_motion_wp,
            curve_motion_xbase + curve_motion_xcumulative * curve_motion_xper,
                1); 
    }
    if ((f+1)->fd_var && (dy != 0))
    {
        fielddesc_setcoord(f+1, curve_motion_template, curve_motion_wp,
            curve_motion_ybase + curve_motion_ycumulative * curve_motion_yper,
                1); 
    }
        /* LATER figure out what to do to notify for an array? */
    if (curve_motion_scalar)
        template_notifyforscalar(curve_motion_template, curve_motion_glist, 
            curve_motion_scalar, gensym("change"), 1, &at);
    if (curve_motion_scalar)
        scalar_redraw(curve_motion_scalar, curve_motion_glist);
    if (curve_motion_array)
        array_redraw(curve_motion_array, curve_motion_glist);
}

static int curve_click(t_gobj *z, t_glist *glist, 
    t_word *data, t_template *template, t_scalar *sc, t_array *ap,
    t_float basex, t_float basey,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_curve *x = (t_curve *)z;
    int i, n = x->x_npoints;
    int bestn = -1;
    int besterror = 0x7fffffff;
    t_fielddesc *f;
    if (!fielddesc_getfloat(&x->x_vis, template, data, 0))
        return (0);
    for (i = 0, f = x->x_vec; i < n; i++, f += 2)
    {
        int xval = fielddesc_getcoord(f, template, data, 0),
            xloc = glist_xtopixels(glist, basex + xval);
        int yval = fielddesc_getcoord(f+1, template, data, 0),
            yloc = glist_ytopixels(glist, basey + yval);
        int xerr = xloc - xpix, yerr = yloc - ypix;
        if (!f->fd_var && !(f+1)->fd_var)
            continue;
        if (xerr < 0)
            xerr = -xerr;
        if (yerr < 0)
            yerr = -yerr;
        if (yerr > xerr)
            xerr = yerr;
        if (xerr < besterror)
        {
            curve_motion_xbase = xval;
            curve_motion_ybase = yval;
            besterror = xerr;
            bestn = i;
        }
    }
    if (besterror > 6)
        return (0);
    if (doit)
    {
        curve_motion_xper = glist_pixelstox(glist, 1)
            - glist_pixelstox(glist, 0);
        curve_motion_yper = glist_pixelstoy(glist, 1)
            - glist_pixelstoy(glist, 0);
        curve_motion_xcumulative = 0;
        curve_motion_ycumulative = 0;
        curve_motion_glist = glist;
        curve_motion_scalar = sc;
        curve_motion_array = ap;
        curve_motion_wp = data;
        curve_motion_field = 2*bestn;
        curve_motion_template = template;
        if (curve_motion_scalar)
            gpointer_setglist(&curve_motion_gpointer, curve_motion_glist,
                curve_motion_scalar);
        else gpointer_setarray(&curve_motion_gpointer,
                curve_motion_array, curve_motion_wp);
        glist_grab(glist, z, curve_motion, 0, xpix, ypix);
    }
    return (1);
}

t_parentwidgetbehavior curve_widgetbehavior =
{
    curve_getrect,
    curve_displace,
    curve_select,
    curve_activate,
    curve_vis,
    curve_click,
};

static void curve_free(t_curve *x)
{
    t_freebytes(x->x_vec, 2 * x->x_npoints * sizeof(*x->x_vec));
}

static void curve_setup(void)
{
    curve_class = class_new(gensym("drawpolygon"), (t_newmethod)curve_new,
        (t_method)curve_free, sizeof(t_curve), 0, A_GIMME, 0);
    class_setdrawcommand(curve_class);
    class_addcreator((t_newmethod)curve_new, gensym("drawcurve"),
        A_GIMME, 0);
    class_addcreator((t_newmethod)curve_new, gensym("filledpolygon"),
        A_GIMME, 0);
    class_addcreator((t_newmethod)curve_new, gensym("filledcurve"),
        A_GIMME, 0);
    class_setparentwidget(curve_class, &curve_widgetbehavior);
    class_addfloat(curve_class, curve_float);
}

/* --------- plots for showing arrays --------------- */

t_class *plot_class;

typedef struct _plot
{
    t_object x_obj;
    t_canvas *x_canvas;
    t_fielddesc x_outlinecolor;
    t_fielddesc x_width;
    t_fielddesc x_xloc;
    t_fielddesc x_yloc;
    t_fielddesc x_xinc;
    t_fielddesc x_style;
    t_fielddesc x_data;
    t_fielddesc x_xpoints;
    t_fielddesc x_ypoints;
    t_fielddesc x_wpoints;
    t_fielddesc x_vis;          /* visible */
    t_fielddesc x_scalarvis;    /* true if drawing the scalar at each point */
} t_plot;

static void *plot_new(t_symbol *classsym, t_int argc, t_atom *argv)
{
    t_plot *x = (t_plot *)pd_new(plot_class);
    int defstyle = PLOTSTYLE_POLY;
    x->x_canvas = canvas_getcurrent();

    fielddesc_setfloat_var(&x->x_xpoints, gensym("x"));
    fielddesc_setfloat_var(&x->x_ypoints, gensym("y"));
    fielddesc_setfloat_var(&x->x_wpoints, gensym("w"));
    
    fielddesc_setfloat_const(&x->x_vis, 1);
    fielddesc_setfloat_const(&x->x_scalarvis, 1);
    while (1)
    {
        t_symbol *firstarg = atom_getsymbolarg(0, argc, argv);
        if (!strcmp(firstarg->s_name, "curve") ||
            !strcmp(firstarg->s_name, "-c"))
        {
            defstyle = PLOTSTYLE_BEZ;
            argc--, argv++;
        }
        else if (!strcmp(firstarg->s_name, "-v") && argc > 1)
        {
            fielddesc_setfloatarg(&x->x_vis, 1, argv+1);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-vs") && argc > 1)
        {
            fielddesc_setfloatarg(&x->x_scalarvis, 1, argv+1);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-x") && argc > 1)
        {
            fielddesc_setfloatarg(&x->x_xpoints, 1, argv+1);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-y") && argc > 1)
        {
            fielddesc_setfloatarg(&x->x_ypoints, 1, argv+1);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-w") && argc > 1)
        {
            fielddesc_setfloatarg(&x->x_wpoints, 1, argv+1);
            argc -= 2; argv += 2;
        }
        else break;
    }
    if (argc) fielddesc_setarrayarg(&x->x_data, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_data, 1);
    if (argc) fielddesc_setfloatarg(&x->x_outlinecolor, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_outlinecolor, 0);
    if (argc) fielddesc_setfloatarg(&x->x_width, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_width, 1);
    if (argc) fielddesc_setfloatarg(&x->x_xloc, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_xloc, 1);
    if (argc) fielddesc_setfloatarg(&x->x_yloc, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_yloc, 1);
    if (argc) fielddesc_setfloatarg(&x->x_xinc, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_xinc, 1);
    if (argc) fielddesc_setfloatarg(&x->x_style, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_style, defstyle);
    return (x);
}

void plot_float(t_plot *x, t_floatarg f)
{
    int viswas;
    if (x->x_vis.fd_type != A_FLOAT || x->x_vis.fd_var)
    {
        pd_error(x, "global vis/invis for a template with variable visibility");
        return;
    }
    viswas = (x->x_vis.fd_un.fd_float != 0);
    
    if ((f != 0 && viswas) || (f == 0 && !viswas))
        return;
    canvas_redrawallfortemplatecanvas(x->x_canvas, 2);
    fielddesc_setfloat_const(&x->x_vis, (f != 0));
    canvas_redrawallfortemplatecanvas(x->x_canvas, 1);
}

/* -------------------- widget behavior for plot ------------ */


    /* get everything we'll need from the owner template of the array being
    plotted. Not used for garrays, but see below */
static int plot_readownertemplate(t_plot *x,
    t_word *data, t_template *ownertemplate, 
    t_symbol **elemtemplatesymp, t_array **arrayp,
    t_float *linewidthp, t_float *xlocp, t_float *xincp, t_float *ylocp, t_float *stylep,
    t_float *visp, t_float *scalarvisp,
    t_fielddesc **xfield, t_fielddesc **yfield, t_fielddesc **wfield)
{
    int arrayonset, type;
    t_symbol *elemtemplatesym;
    t_array *array;

        /* find the data and verify it's an array */
    if (x->x_data.fd_type != A_ARRAY || !x->x_data.fd_var)
    {
        error("plot: needs an array field");
        return (-1);
    }
    if (!template_find_field(ownertemplate, x->x_data.fd_un.fd_varsym,
        &arrayonset, &type, &elemtemplatesym))
    {
        error("plot: %s: no such field", x->x_data.fd_un.fd_varsym->s_name);
        return (-1);
    }
    if (type != DT_ARRAY)
    {
        error("plot: %s: not an array", x->x_data.fd_un.fd_varsym->s_name);
        return (-1);
    }
    array = *(t_array **)(((char *)data) + arrayonset);
    *linewidthp = fielddesc_getfloat(&x->x_width, ownertemplate, data, 1);
    *xlocp = fielddesc_getfloat(&x->x_xloc, ownertemplate, data, 1);
    *xincp = fielddesc_getfloat(&x->x_xinc, ownertemplate, data, 1);
    *ylocp = fielddesc_getfloat(&x->x_yloc, ownertemplate, data, 1);
    *stylep = fielddesc_getfloat(&x->x_style, ownertemplate, data, 1);
    *visp = fielddesc_getfloat(&x->x_vis, ownertemplate, data, 1);
    *scalarvisp = fielddesc_getfloat(&x->x_scalarvis, ownertemplate, data, 1);
    *elemtemplatesymp = elemtemplatesym;
    *arrayp = array;
    *xfield = &x->x_xpoints;
    *yfield = &x->x_ypoints;
    *wfield = &x->x_wpoints;
    return (0);
}

    /* get everything else you could possibly need about a plot,
    either for plot's own purposes or for plotting a "garray" */
int array_getfields(t_symbol *elemtemplatesym,
    t_canvas **elemtemplatecanvasp,
    t_template **elemtemplatep, int *elemsizep,
    t_fielddesc *xfielddesc, t_fielddesc *yfielddesc, t_fielddesc *wfielddesc, 
    int *xonsetp, int *yonsetp, int *wonsetp)
{
    int arrayonset, elemsize, yonset, wonset, xonset, type;
    t_template *elemtemplate;
    t_symbol *dummy, *varname;
    t_canvas *elemtemplatecanvas = 0;

        /* the "float" template is special in not having to have a canvas;
        template_findbyname is hardwired to return a predefined 
        template. */

    if (!(elemtemplate =  template_findbyname(elemtemplatesym)))
    {
        error("plot: %s: no such template", elemtemplatesym->s_name);
        return (-1);
    }
    if (!((elemtemplatesym == &s_float) ||
        (elemtemplatecanvas = template_findcanvas(elemtemplate))))
    {
        error("plot: %s: no canvas for this template", elemtemplatesym->s_name);
        return (-1);
    }
    elemsize = elemtemplate->t_n * sizeof(t_word);
    if (yfielddesc && yfielddesc->fd_var)
        varname = yfielddesc->fd_un.fd_varsym;
    else varname = gensym("y");
    if (!template_find_field(elemtemplate, varname, &yonset, &type, &dummy)
        || type != DT_FLOAT)    
            yonset = -1;
    if (xfielddesc && xfielddesc->fd_var)
        varname = xfielddesc->fd_un.fd_varsym;
    else varname = gensym("x");
    if (!template_find_field(elemtemplate, varname, &xonset, &type, &dummy)
        || type != DT_FLOAT) 
            xonset = -1;
    if (wfielddesc && wfielddesc->fd_var)
        varname = wfielddesc->fd_un.fd_varsym;
    else varname = gensym("w");
    if (!template_find_field(elemtemplate, varname, &wonset, &type, &dummy)
        || type != DT_FLOAT) 
            wonset = -1;

        /* fill in slots for return values */
    *elemtemplatecanvasp = elemtemplatecanvas;
    *elemtemplatep = elemtemplate;
    *elemsizep = elemsize;
    *xonsetp = xonset;
    *yonsetp = yonset;
    *wonsetp = wonset;
    return (0);
}

static void plot_getrect(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_plot *x = (t_plot *)z;
    int elemsize, yonset, wonset, xonset;
    t_canvas *elemtemplatecanvas;
    t_template *elemtemplate;
    t_symbol *elemtemplatesym;
    t_float linewidth, xloc, xinc, yloc, style, xsum, yval, vis, scalarvis;
    t_array *array;
    int x1 = 0x7fffffff, y1 = 0x7fffffff, x2 = -0x7fffffff, y2 = -0x7fffffff;
    int i;
    t_float xpix, ypix, wpix;
    t_fielddesc *xfielddesc, *yfielddesc, *wfielddesc;
        /* if we're the only plot in the glist claim the whole thing */
    if (glist->gl_list && !glist->gl_list->g_next)
    {
        *xp1 = *yp1 = -0x7fffffff;
        *xp2 = *yp2 = 0x7fffffff;
        return;
    }
    if (!plot_readownertemplate(x, data, template, 
        &elemtemplatesym, &array, &linewidth, &xloc, &xinc, &yloc, &style,
            &vis, &scalarvis, &xfielddesc, &yfielddesc, &wfielddesc) &&
                (vis != 0) &&
            !array_getfields(elemtemplatesym, &elemtemplatecanvas,
                &elemtemplate, &elemsize, 
                xfielddesc, yfielddesc, wfielddesc,
                &xonset, &yonset, &wonset))
    {
            /* if it has more than 2000 points, just check 1000 of them. */
        int incr = (array->a_n <= 2000 ? 1 : array->a_n / 1000);
        for (i = 0, xsum = 0; i < array->a_n; i += incr)
        {
            t_float usexloc, useyloc;
            t_gobj *y;
                /* get the coords of the point proper */
            array_getcoordinate(glist, (char *)(array->a_vec) + i * elemsize,
                xonset, yonset, wonset, i, basex + xloc, basey + yloc, xinc,
                xfielddesc, yfielddesc, wfielddesc, &xpix, &ypix, &wpix);
            if (xpix < x1)
                x1 = xpix;
            if (xpix > x2)
                x2 = xpix;
            if (ypix - wpix < y1)
                y1 = ypix - wpix;
            if (ypix + wpix > y2)
                y2 = ypix + wpix;
            
            if (scalarvis != 0)
            {
                    /* check also the drawing instructions for the scalar */ 
                if (xonset >= 0)
                    usexloc = basex + xloc + fielddesc_cvttocoord(xfielddesc, 
                        *(t_float *)(((char *)(array->a_vec) + elemsize * i)
                            + xonset));
                else usexloc = basex + xsum, xsum += xinc;
                if (yonset >= 0)
                    yval = *(t_float *)(((char *)(array->a_vec) + elemsize * i)
                        + yonset);
                else yval = 0;
                useyloc = basey + yloc + fielddesc_cvttocoord(yfielddesc, yval);
                for (y = elemtemplatecanvas->gl_list; y; y = y->g_next)
                {
                    int xx1, xx2, yy1, yy2;
                    t_parentwidgetbehavior *wb = pd_getparentwidget(&y->g_pd);
                    if (!wb) continue;
                    (*wb->w_parentgetrectfn)(y, glist,
                        (t_word *)((char *)(array->a_vec) + elemsize * i),
                            elemtemplate, usexloc, useyloc, 
                                &xx1, &yy1, &xx2, &yy2);
                    if (xx1 < x1)
                        x1 = xx1;
                    if (yy1 < y1)
                        y1 = yy1;
                     if (xx2 > x2)
                        x2 = xx2;
                    if (yy2 > y2)
                        y2 = yy2;   
                }
            }
        }
    }

    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

static void plot_displace(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int dx, int dy)
{
        /* not yet */
}

static void plot_select(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int state)
{
    /* not yet */
}

static void plot_activate(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int state)
{
        /* not yet */
}

#define CLIP(x) ((x) < 1e20 && (x) > -1e20 ? x : 0)

static void plot_vis(t_gobj *z, t_glist *glist, 
    t_word *data, t_template *template, t_float basex, t_float basey,
    int tovis)
{
    t_plot *x = (t_plot *)z;
    int elemsize, yonset, wonset, xonset, i;
    t_canvas *elemtemplatecanvas;
    t_template *elemtemplate;
    t_symbol *elemtemplatesym;
    t_float linewidth, xloc, xinc, yloc, style, usexloc, xsum, yval, vis,
        scalarvis;
    t_array *array;
    int nelem;
    char *elem;
    t_fielddesc *xfielddesc, *yfielddesc, *wfielddesc;
        /* even if the array is "invisible", if its visibility is
        set by an instance variable you have to explicitly erase it,
        because the flag could earlier have been on when we were getting
        drawn.  Rather than look to try to find out whether we're
        visible we just do the erasure.  At the TK level this should
        cause no action because the tag matches nobody.  LATER we
        might want to optimize this somehow.  Ditto the "vis()" routines
        for other drawing instructions. */
        
    if (plot_readownertemplate(x, data, template, 
        &elemtemplatesym, &array, &linewidth, &xloc, &xinc, &yloc, &style,
        &vis, &scalarvis, &xfielddesc, &yfielddesc, &wfielddesc) ||
            ((vis == 0) && tovis) /* see above for 'tovis' */
            || array_getfields(elemtemplatesym, &elemtemplatecanvas,
                &elemtemplate, &elemsize, xfielddesc, yfielddesc, wfielddesc,
                &xonset, &yonset, &wonset))
                    return;
    nelem = array->a_n;
    elem = (char *)array->a_vec;

    if (tovis)
    {
        if (style == PLOTSTYLE_POINTS)
        {
            t_float minyval = 1e20, maxyval = -1e20;
            int ndrawn = 0;
            for (xsum = basex + xloc, i = 0; i < nelem; i++)
            {
                t_float yval, xpix, ypix, nextxloc;
                int ixpix, inextx;

                if (xonset >= 0)
                {
                    usexloc = basex + xloc +
                        *(t_float *)((elem + elemsize * i) + xonset);
                    ixpix = glist_xtopixels(glist, 
                        fielddesc_cvttocoord(xfielddesc, usexloc));
                    inextx = ixpix + 2;
                }
                else
                {
                    usexloc = xsum;
                    xsum += xinc;
                    ixpix = glist_xtopixels(glist,
                        fielddesc_cvttocoord(xfielddesc, usexloc));
                    inextx = glist_xtopixels(glist,
                        fielddesc_cvttocoord(xfielddesc, xsum));
                }

                if (yonset >= 0)
                    yval = yloc + *(t_float *)((elem + elemsize * i) + yonset);
                else yval = 0;
                yval = CLIP(yval);
                if (yval < minyval)
                    minyval = yval;
                if (yval > maxyval)
                    maxyval = yval;
                if (i == nelem-1 || inextx != ixpix)
                {
                    sys_vgui(".x%lx.c create rectangle %d %d %d %d \
-fill black -width 0  -tags [list plot%lx array]\n",
                        glist_getcanvas(glist),
                        ixpix, (int)glist_ytopixels(glist, 
                            basey + fielddesc_cvttocoord(yfielddesc, minyval)),
                        inextx, (int)(glist_ytopixels(glist, 
                            basey + fielddesc_cvttocoord(yfielddesc, maxyval))
                                + linewidth), data);
                    ndrawn++;
                    minyval = 1e20;
                    maxyval = -1e20;
                }
                if (ndrawn > 2000 || ixpix >= 3000) break;
            }
        }
        else
        {
            char outline[20];
            int lastpixel = -1, ndrawn = 0;
            t_float yval = 0, wval = 0, xpix;
            int ixpix = 0;
                /* draw the trace */
            numbertocolor(fielddesc_getfloat(&x->x_outlinecolor, template,
                data, 1), outline);
            if (wonset >= 0)
            {
                    /* found "w" field which controls linewidth.  The trace is
                    a filled polygon with 2n points. */
                sys_vgui(".x%lx.c create polygon \\\n",
                    glist_getcanvas(glist));

                for (i = 0, xsum = xloc; i < nelem; i++)
                {
                    if (xonset >= 0)
                        usexloc = xloc + *(t_float *)((elem + elemsize * i)
                            + xonset);
                    else usexloc = xsum, xsum += xinc;
                    if (yonset >= 0)
                        yval = *(t_float *)((elem + elemsize * i) + yonset);
                    else yval = 0;
                    yval = CLIP(yval);
                    wval = *(t_float *)((elem + elemsize * i) + wonset);
                    wval = CLIP(wval);
                    xpix = glist_xtopixels(glist,
                        basex + fielddesc_cvttocoord(xfielddesc, usexloc));
                    ixpix = xpix + 0.5;
                    if (xonset >= 0 || ixpix != lastpixel)
                    {
                        sys_vgui("%d %f \\\n", ixpix,
                            glist_ytopixels(glist,
                                basey + fielddesc_cvttocoord(yfielddesc, 
                                    yloc + yval) -
                                        fielddesc_cvttocoord(wfielddesc,wval)));
                        ndrawn++;
                    }
                    lastpixel = ixpix;
                    if (ndrawn >= 1000) goto ouch;
                }
                lastpixel = -1;
                for (i = nelem-1; i >= 0; i--)
                {
                    t_float usexloc;
                    if (xonset >= 0)
                        usexloc = xloc + *(t_float *)((elem + elemsize * i)
                            + xonset);
                    else xsum -= xinc, usexloc = xsum;
                    if (yonset >= 0)
                        yval = *(t_float *)((elem + elemsize * i) + yonset);
                    else yval = 0;
                    yval = CLIP(yval);
                    wval = *(t_float *)((elem + elemsize * i) + wonset);
                    wval = CLIP(wval);
                    xpix = glist_xtopixels(glist,
                        basex + fielddesc_cvttocoord(xfielddesc, usexloc));
                    ixpix = xpix + 0.5;
                    if (xonset >= 0 || ixpix != lastpixel)
                    {
                        sys_vgui("%d %f \\\n", ixpix, glist_ytopixels(glist,
                            basey + yloc + fielddesc_cvttocoord(yfielddesc,
                                yval) +
                                    fielddesc_cvttocoord(wfielddesc, wval)));
                        ndrawn++;
                    }
                    lastpixel = ixpix;
                    if (ndrawn >= 1000) goto ouch;
                }
                    /* TK will complain if there aren't at least 3 points.
                    There should be at least two already. */
                if (ndrawn < 4)
                {
                    sys_vgui("%d %f \\\n", ixpix + 10, glist_ytopixels(glist,
                        basey + yloc + fielddesc_cvttocoord(yfielddesc,
                            yval) +
                                fielddesc_cvttocoord(wfielddesc, wval)));
                    sys_vgui("%d %f \\\n", ixpix + 10, glist_ytopixels(glist,
                        basey + yloc + fielddesc_cvttocoord(yfielddesc,
                            yval) -
                                fielddesc_cvttocoord(wfielddesc, wval)));
                }
            ouch:
                sys_vgui(" -width 1 -fill %s -outline %s\\\n",
                    outline, outline);
                if (style == PLOTSTYLE_BEZ) sys_vgui("-smooth 1\\\n");

                sys_vgui("-tags [list plot%lx array]\n", data);
            }
            else if (linewidth > 0)
            {
                    /* no "w" field.  If the linewidth is positive, draw a
                    segmented line with the requested width; otherwise don't
                    draw the trace at all. */
                sys_vgui(".x%lx.c create line \\\n", glist_getcanvas(glist));

                for (xsum = xloc, i = 0; i < nelem; i++)
                {
                    t_float usexloc;
                    if (xonset >= 0)
                        usexloc = xloc + *(t_float *)((elem + elemsize * i) +
                            xonset);
                    else usexloc = xsum, xsum += xinc;
                    if (yonset >= 0)
                        yval = *(t_float *)((elem + elemsize * i) + yonset);
                    else yval = 0;
                    yval = CLIP(yval);
                    xpix = glist_xtopixels(glist,
                        basex + fielddesc_cvttocoord(xfielddesc, usexloc));
                    ixpix = xpix + 0.5;
                    if (xonset >= 0 || ixpix != lastpixel)
                    {
                        sys_vgui("%d %f \\\n", ixpix,
                            glist_ytopixels(glist,
                                basey + yloc + fielddesc_cvttocoord(yfielddesc,
                                    yval)));
                        ndrawn++;
                    }
                    lastpixel = ixpix;
                    if (ndrawn >= 1000) break;
                }
                    /* TK will complain if there aren't at least 2 points... */
                if (ndrawn == 0) sys_vgui("0 0 0 0 \\\n");
                else if (ndrawn == 1) sys_vgui("%d %f \\\n", ixpix + 10,
                    glist_ytopixels(glist, basey + yloc + 
                        fielddesc_cvttocoord(yfielddesc, yval)));

                sys_vgui("-width %f\\\n", linewidth);
                sys_vgui("-fill %s\\\n", outline);
                if (style == PLOTSTYLE_BEZ) sys_vgui("-smooth 1\\\n");

                sys_vgui("-tags [list plot%lx array]\n", data);
            }
        }
            /* We're done with the outline; now draw all the points.
            This code is inefficient since the template has to be
            searched for drawing instructions for every last point. */
        if (scalarvis != 0)
        {
            for (xsum = xloc, i = 0; i < nelem; i++)
            {
                t_float usexloc, useyloc;
                t_gobj *y;
                if (xonset >= 0)
                    usexloc = basex + xloc +
                        *(t_float *)((elem + elemsize * i) + xonset);
                else usexloc = basex + xsum, xsum += xinc;
                if (yonset >= 0)
                    yval = *(t_float *)((elem + elemsize * i) + yonset);
                else yval = 0;
                useyloc = basey + yloc +
                    fielddesc_cvttocoord(yfielddesc, yval);
                for (y = elemtemplatecanvas->gl_list; y; y = y->g_next)
                {
                    t_parentwidgetbehavior *wb = pd_getparentwidget(&y->g_pd);
                    if (!wb) continue;
                    (*wb->w_parentvisfn)(y, glist,
                        (t_word *)(elem + elemsize * i),
                            elemtemplate, usexloc, useyloc, tovis);
                }
            }
        }
    }
    else
    {
            /* un-draw the individual points */
        if (scalarvis != 0)
        {
            int i;
            for (i = 0; i < nelem; i++)
            {
                t_gobj *y;
                for (y = elemtemplatecanvas->gl_list; y; y = y->g_next)
                {
                    t_parentwidgetbehavior *wb = pd_getparentwidget(&y->g_pd);
                    if (!wb) continue;
                    (*wb->w_parentvisfn)(y, glist,
                        (t_word *)(elem + elemsize * i), elemtemplate,
                            0, 0, 0);
                }
            }
        }
            /* and then the trace */
        sys_vgui(".x%lx.c delete plot%lx\n",
            glist_getcanvas(glist), data);      
    }
}

static t_float array_motion_xcumulative;
static t_float array_motion_ycumulative;
static t_fielddesc *array_motion_xfield;
static t_fielddesc *array_motion_yfield;
static t_glist *array_motion_glist;
static t_scalar *array_motion_scalar;
static t_array *array_motion_array;
static t_word *array_motion_wp;
static t_template *array_motion_template;
static int array_motion_npoints;
static int array_motion_elemsize;
static int array_motion_altkey;
static t_float array_motion_initx;
static t_float array_motion_xperpix;
static t_float array_motion_yperpix;
static int array_motion_lastx;
static int array_motion_fatten;

    /* LATER protect against the template changing or the scalar disappearing
    probably by attaching a gpointer here ... */

static void array_motion(void *z, t_floatarg dx, t_floatarg dy)
{
    array_motion_xcumulative += dx * array_motion_xperpix;
    array_motion_ycumulative += dy * array_motion_yperpix;
    if (array_motion_xfield)
    {
            /* it's an x, y plot */
        int i;
        for (i = 0; i < array_motion_npoints; i++)
        {
            t_word *thisword = (t_word *)(((char *)array_motion_wp) +
                i * array_motion_elemsize);
            t_float xwas = fielddesc_getcoord(array_motion_xfield, 
                array_motion_template, thisword, 1);
            t_float ywas = (array_motion_yfield ?
                fielddesc_getcoord(array_motion_yfield, 
                    array_motion_template, thisword, 1) : 0);
            fielddesc_setcoord(array_motion_xfield,
                array_motion_template, thisword, xwas + dx, 1);
            if (array_motion_yfield)
            {
                if (array_motion_fatten)
                {
                    if (i == 0)
                    {
                        t_float newy = ywas + dy * array_motion_yperpix;
                        if (newy < 0)
                            newy = 0;
                        fielddesc_setcoord(array_motion_yfield,
                            array_motion_template, thisword, newy, 1);
                    }
                }
                else
                {
                    fielddesc_setcoord(array_motion_yfield,
                        array_motion_template, thisword,
                            ywas + dy * array_motion_yperpix, 1);
                }
            }
        }
    }
    else if (array_motion_yfield)
    {
            /* a y-only plot. */
        int thisx = array_motion_initx + array_motion_xcumulative + 0.5, x2;
        int increment, i, nchange;
        t_float newy = array_motion_ycumulative,
            oldy = fielddesc_getcoord(array_motion_yfield,
                array_motion_template,
                    (t_word *)(((char *)array_motion_wp) +
                        array_motion_elemsize * array_motion_lastx),
                            1);
        t_float ydiff = newy - oldy;
        if (thisx < 0) thisx = 0;
        else if (thisx >= array_motion_npoints)
            thisx = array_motion_npoints - 1;
        increment = (thisx > array_motion_lastx ? -1 : 1);
        nchange = 1 + increment * (array_motion_lastx - thisx);

        for (i = 0, x2 = thisx; i < nchange; i++, x2 += increment)
        {
            fielddesc_setcoord(array_motion_yfield,
                array_motion_template,
                    (t_word *)(((char *)array_motion_wp) +
                        array_motion_elemsize * x2), newy, 1);
            if (nchange > 1)
                newy -= ydiff * (1./(nchange - 1));
         }
         array_motion_lastx = thisx;
    }
    if (array_motion_scalar)
        scalar_redraw(array_motion_scalar, array_motion_glist);
    if (array_motion_array)
        array_redraw(array_motion_array, array_motion_glist);
}

int scalar_doclick(t_word *data, t_template *template, t_scalar *sc,
    t_array *ap, struct _glist *owner,
    t_float xloc, t_float yloc, int xpix, int ypix,
    int shift, int alt, int dbl, int doit);

    /* try clicking on an element of the array as a scalar (if clicking
    on the trace of the array failed) */
static int array_doclick_element(t_array *array, t_glist *glist,
    t_scalar *sc, t_array *ap,
    t_symbol *elemtemplatesym,
    t_float linewidth, t_float xloc, t_float xinc, t_float yloc,
    t_fielddesc *xfield, t_fielddesc *yfield, t_fielddesc *wfield,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_canvas *elemtemplatecanvas;
    t_template *elemtemplate;
    int elemsize, yonset, wonset, xonset, i, incr, hit;
    t_float xsum;

    if (elemtemplatesym == &s_float)
        return (0);
    if (array_getfields(elemtemplatesym, &elemtemplatecanvas,
        &elemtemplate, &elemsize, xfield, yfield, wfield,
            &xonset, &yonset, &wonset))
                return (0);
        /* if it has more than 2000 points, just check 300 of them. */
    if (array->a_n < 2000)
        incr = 1;
    else incr = array->a_n / 300;
    for (i = 0, xsum = 0; i < array->a_n; i += incr)
    {
        t_float usexloc, useyloc;
        if (xonset >= 0)
            usexloc = xloc + fielddesc_cvttocoord(xfield, 
                *(t_float *)(((char *)(array->a_vec) + elemsize * i) + xonset));
        else usexloc = xloc + xsum, xsum += xinc;
        useyloc = yloc + (yonset >= 0 ? fielddesc_cvttocoord(yfield,
            *(t_float *)(((char *)(array->a_vec) + elemsize * i) + yonset)) : 0);
        
        if (hit = scalar_doclick(
            (t_word *)((char *)(array->a_vec) + i * elemsize),
            elemtemplate, 0, array,
            glist, usexloc, useyloc,
            xpix, ypix, shift, alt, dbl, doit))
                return (hit);
    }
    return (0);
}

static int array_doclick(t_array *array, t_glist *glist, t_scalar *sc,
    t_array *ap, t_symbol *elemtemplatesym,
    t_float linewidth, t_float xloc, t_float xinc, t_float yloc, t_float scalarvis,
    t_fielddesc *xfield, t_fielddesc *yfield, t_fielddesc *wfield,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_canvas *elemtemplatecanvas;
    t_template *elemtemplate;
    int elemsize, yonset, wonset, xonset, i, callmotion = 0;

    if (!array_getfields(elemtemplatesym, &elemtemplatecanvas,
        &elemtemplate, &elemsize, xfield, yfield, wfield,
        &xonset, &yonset, &wonset))
    {
        t_float best = 100;
            /* if it has more than 2000 points, just check 1000 of them. */
        int incr = (array->a_n <= 2000 ? 1 : array->a_n / 1000);
        array_motion_elemsize = elemsize;
        array_motion_glist = glist;
        array_motion_scalar = sc;
        array_motion_array = ap;
        array_motion_template = elemtemplate;
        array_motion_xperpix = glist_dpixtodx(glist, 1);
        array_motion_yperpix = glist_dpixtody(glist, 1);
            /* if we're the only array here and if we appear to have
            only a 'y' field, click always succeeds and furthermore we'll
            call "motion" later. */
        if (glist->gl_list && !glist->gl_list->g_next &&
            elemsize == sizeof(t_word))
        {
            int xval = glist_pixelstox(glist, xpix);
            if (xval < 0)
                xval = 0;
            else if (xval >= array->a_n)
                xval = array->a_n - 1;
            if (doit)
            {
                fielddesc_setcoord(yfield, elemtemplate,
                    (t_word *)(((char *)array->a_vec) + elemsize * xval),
                        glist_pixelstoy(glist, ypix), 1);
                glist_grab(glist, 0, array_motion, 0, xpix, ypix);
            }
            array_motion_yfield = yfield;
            array_motion_ycumulative = glist_pixelstoy(glist, ypix);
            array_motion_fatten = 0;
            array_motion_xfield = 0;
            array_motion_xcumulative = 0;
            array_motion_lastx = array_motion_initx = xval;
            array_motion_npoints = array->a_n;
            array_motion_wp = (t_word *)((char *)array->a_vec);
        }
        else
        {
            for (i = 0; i < array->a_n; i += incr)
            {
                t_float pxpix, pypix, pwpix, dx, dy;
                array_getcoordinate(glist,
                    (char *)(array->a_vec) + i * elemsize,
                    xonset, yonset, wonset, i, xloc, yloc, xinc,
                    xfield, yfield, wfield, &pxpix, &pypix, &pwpix);
                if (pwpix < 4)
                    pwpix = 4;
                dx = pxpix - xpix;
                if (dx < 0) dx = -dx;
                if (dx > 8)
                    continue;   
                dy = pypix - ypix;
                if (dy < 0) dy = -dy;
                if (dx + dy < best)
                    best = dx + dy;
                if (wonset >= 0)
                {
                    dy = (pypix + pwpix) - ypix;
                    if (dy < 0) dy = -dy;
                    if (dx + dy < best)
                        best = dx + dy;
                    dy = (pypix - pwpix) - ypix;
                    if (dy < 0) dy = -dy;
                    if (dx + dy < best)
                        best = dx + dy;
                }
            }
            if (best > 8)
            {
                if (scalarvis != 0)
                    return (array_doclick_element(array, glist, sc, ap,
                        elemtemplatesym, linewidth, xloc, xinc, yloc,
                            xfield, yfield, wfield,
                            xpix, ypix, shift, alt, dbl, doit));
                else return (0);
            }
            best += 0.001;  /* add truncation error margin */
            for (i = 0; i < array->a_n; i += incr)
            {
                t_float pxpix, pypix, pwpix, dx, dy, dy2, dy3;
                array_getcoordinate(glist, (char *)(array->a_vec) + i * elemsize,
                    xonset, yonset, wonset, i, xloc, yloc, xinc,
                    xfield, yfield, wfield, &pxpix, &pypix, &pwpix);
                if (pwpix < 4)
                    pwpix = 4;
                dx = pxpix - xpix;
                if (dx < 0) dx = -dx;
                dy = pypix - ypix;
                if (dy < 0) dy = -dy;
                if (wonset >= 0)
                {
                    dy2 = (pypix + pwpix) - ypix;
                    if (dy2 < 0) dy2 = -dy2;
                    dy3 = (pypix - pwpix) - ypix;
                    if (dy3 < 0) dy3 = -dy3;
                    if (yonset < 0)
                        dy = 100;
                }
                else dy2 = dy3 = 100;
                if (dx + dy <= best || dx + dy2 <= best || dx + dy3 <= best)
                {
                    if (dy < dy2 && dy < dy3)
                        array_motion_fatten = 0;
                    else if (dy2 < dy3)
                        array_motion_fatten = -1;
                    else array_motion_fatten = 1;
                    if (doit)
                    {
                        char *elem = (char *)array->a_vec;
                        if (alt && xpix < pxpix) /* delete a point */
                        {
                            if (array->a_n <= 1)
                                return (0);
                            memmove((char *)(array->a_vec) + elemsize * i, 
                                (char *)(array->a_vec) + elemsize * (i+1),
                                    (array->a_n - 1 - i) * elemsize);
                            array_resize_and_redraw(array, glist, array->a_n - 1);
                            return (0);
                        }
                        else if (alt)
                        {
                            /* add a point (after the clicked-on one) */
                            array_resize_and_redraw(array, glist, array->a_n + 1);
                            elem = (char *)array->a_vec;
                            memmove(elem + elemsize * (i+1), 
                                elem + elemsize * i,
                                    (array->a_n - i - 1) * elemsize);
                            i++;
                        }
                        if (xonset >= 0)
                        {
                            array_motion_xfield = xfield;
                            array_motion_xcumulative = 
                                fielddesc_getcoord(xfield, array_motion_template,
                                    (t_word *)(elem + i * elemsize), 1);
                                array_motion_wp = (t_word *)(elem + i * elemsize);
                            if (shift)
                                array_motion_npoints = array->a_n - i;
                            else array_motion_npoints = 1;
                        }
                        else
                        {
                            array_motion_xfield = 0;
                            array_motion_xcumulative = 0;
                            array_motion_wp = (t_word *)elem;
                            array_motion_npoints = array->a_n;

                            array_motion_initx = i;
                            array_motion_lastx = i;
                            array_motion_xperpix *= (xinc == 0 ? 1 : 1./xinc);
                        }
                        if (array_motion_fatten)
                        {
                            array_motion_yfield = wfield;
                            array_motion_ycumulative = 
                                fielddesc_getcoord(wfield, array_motion_template,
                                    (t_word *)(elem + i * elemsize), 1);
                            array_motion_yperpix *= -array_motion_fatten;
                        }
                        else if (yonset >= 0)
                        {
                            array_motion_yfield = yfield;
                            array_motion_ycumulative = 
                                fielddesc_getcoord(yfield, array_motion_template,
                                    (t_word *)(elem + i * elemsize), 1);
                                /* *(t_float *)((elem + elemsize * i) + yonset); */
                        }
                        else
                        {
                            array_motion_yfield = 0;
                            array_motion_ycumulative = 0;
                        }
                        glist_grab(glist, 0, array_motion, 0, xpix, ypix);
                    }
                    if (alt)
                    {
                        if (xpix < pxpix)
                            return (CURSOR_EDITMODE_DISCONNECT);
                        else return (CURSOR_RUNMODE_ADDPOINT);
                    }
                    else return (array_motion_fatten ?
                        CURSOR_RUNMODE_THICKEN : CURSOR_RUNMODE_CLICKME);
                }
            }   
        }
    }
    return (0);
}

static int plot_click(t_gobj *z, t_glist *glist, 
    t_word *data, t_template *template, t_scalar *sc, t_array *ap,
    t_float basex, t_float basey,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_plot *x = (t_plot *)z;
    t_symbol *elemtemplatesym;
    t_float linewidth, xloc, xinc, yloc, style, vis, scalarvis;
    t_array *array;
    t_fielddesc *xfielddesc, *yfielddesc, *wfielddesc;

    if (!plot_readownertemplate(x, data, template, 
        &elemtemplatesym, &array, &linewidth, &xloc, &xinc, &yloc, &style,
        &vis, &scalarvis,
        &xfielddesc, &yfielddesc, &wfielddesc) && (vis != 0))
    {
        return (array_doclick(array, glist, sc, ap,
            elemtemplatesym,
            linewidth, basex + xloc, xinc, basey + yloc, scalarvis,
            xfielddesc, yfielddesc, wfielddesc,
            xpix, ypix, shift, alt, dbl, doit));
    }
    else return (0);
}

t_parentwidgetbehavior plot_widgetbehavior =
{
    plot_getrect,
    plot_displace,
    plot_select,
    plot_activate,
    plot_vis,
    plot_click,
};

static void plot_setup(void)
{
    plot_class = class_new(gensym("plot"), (t_newmethod)plot_new, 0,
        sizeof(t_plot), 0, A_GIMME, 0);
    class_setdrawcommand(plot_class);
    class_addfloat(plot_class, plot_float);
    class_setparentwidget(plot_class, &plot_widgetbehavior);
}

/* ---------------- drawnumber: draw a number (or symbol) ---------------- */

/*
    drawnumbers draw numeric fields at controllable locations, with
    controllable color and label.  invocation:
    (drawnumber|drawsymbol) [-v <visible>] variable x y color label
*/

t_class *drawnumber_class;

typedef struct _drawnumber
{
    t_object x_obj;
    t_symbol *x_fieldname;
    t_fielddesc x_xloc;
    t_fielddesc x_yloc;
    t_fielddesc x_color;
    t_fielddesc x_vis;
    t_symbol *x_label;
    t_canvas *x_canvas;
} t_drawnumber;

static void *drawnumber_new(t_symbol *classsym, t_int argc, t_atom *argv)
{
    t_drawnumber *x = (t_drawnumber *)pd_new(drawnumber_class);
    char *classname = classsym->s_name;

    fielddesc_setfloat_const(&x->x_vis, 1);
    x->x_canvas = canvas_getcurrent();
    while (1)
    {
        t_symbol *firstarg = atom_getsymbolarg(0, argc, argv);
        if (!strcmp(firstarg->s_name, "-v") && argc > 1)
        {
            fielddesc_setfloatarg(&x->x_vis, 1, argv+1);
            argc -= 2; argv += 2;
        }
        else break;
    }
        /* next argument is name of field to draw - we don't know its type yet
        but fielddesc_setfloatarg() will do fine here. */
    x->x_fieldname = atom_getsymbolarg(0, argc, argv);
    if (argc)
        argc--, argv++;
    if (argc) fielddesc_setfloatarg(&x->x_xloc, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_xloc, 0);
    if (argc) fielddesc_setfloatarg(&x->x_yloc, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_yloc, 0);
    if (argc) fielddesc_setfloatarg(&x->x_color, argc--, argv++);
    else fielddesc_setfloat_const(&x->x_color, 1);
    if (argc)
        x->x_label = atom_getsymbolarg(0, argc, argv);
    else x->x_label = &s_;

    return (x);
}

void drawnumber_float(t_drawnumber *x, t_floatarg f)
{
    int viswas;
    if (x->x_vis.fd_type != A_FLOAT || x->x_vis.fd_var)
    {
        pd_error(x, "global vis/invis for a template with variable visibility");
        return;
    }
    viswas = (x->x_vis.fd_un.fd_float != 0);
    
    if ((f != 0 && viswas) || (f == 0 && !viswas))
        return;
    canvas_redrawallfortemplatecanvas(x->x_canvas, 2);
    fielddesc_setfloat_const(&x->x_vis, (f != 0));
    canvas_redrawallfortemplatecanvas(x->x_canvas, 1);
}

/* -------------------- widget behavior for drawnumber ------------ */

static int drawnumber_gettype(t_drawnumber *x, t_word *data,
    t_template *template, int *onsetp)
{
    int type;
    t_symbol *arraytype;
    if (template_find_field(template, x->x_fieldname, onsetp, &type,
        &arraytype) && type != DT_ARRAY)
            return (type);
    else return (-1);
}

#define DRAWNUMBER_BUFSIZE 1024
static void drawnumber_getbuf(t_drawnumber *x, t_word *data,
    t_template *template, char *buf)
{
    int nchars, onset, type = drawnumber_gettype(x, data, template, &onset);
    if (type < 0)
        buf[0] = 0;
    else
    {
        strncpy(buf, x->x_label->s_name, DRAWNUMBER_BUFSIZE);
        buf[DRAWNUMBER_BUFSIZE - 1] = 0;
        nchars = strlen(buf);
        if (type == DT_TEXT)
        {
            char *buf2;
            int size2, ncopy;
            binbuf_gettext(((t_word *)((char *)data + onset))->w_binbuf,
                &buf2, &size2);
            ncopy = (size2 > DRAWNUMBER_BUFSIZE-1-nchars ? 
                DRAWNUMBER_BUFSIZE-1-nchars: size2);
            memcpy(buf+nchars, buf2, ncopy);
            buf[nchars+ncopy] = 0;
            if (nchars+ncopy == DRAWNUMBER_BUFSIZE-1)
                strcpy(buf+(DRAWNUMBER_BUFSIZE-4), "...");
            t_freebytes(buf2, size2);
        }
        else
        {
            t_atom at;
            if (type == DT_FLOAT)
                SETFLOAT(&at, ((t_word *)((char *)data + onset))->w_float);
            else SETSYMBOL(&at, ((t_word *)((char *)data + onset))->w_symbol);
            atom_string(&at, buf + nchars, DRAWNUMBER_BUFSIZE - nchars);
        }
    }
}

static void drawnumber_getrect(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_drawnumber *x = (t_drawnumber *)z;
    t_atom at;
    int xloc, yloc, font, fontwidth, fontheight, bufsize, width, height;
    char buf[DRAWNUMBER_BUFSIZE], *startline, *newline;

    if (!fielddesc_getfloat(&x->x_vis, template, data, 0))
    {
        *xp1 = *yp1 = 0x7fffffff;
        *xp2 = *yp2 = -0x7fffffff;
        return;
    }
    xloc = glist_xtopixels(glist,
        basex + fielddesc_getcoord(&x->x_xloc, template, data, 0));
    yloc = glist_ytopixels(glist,
        basey + fielddesc_getcoord(&x->x_yloc, template, data, 0));
    font = glist_getfont(glist);
    fontwidth = sys_fontwidth(font);
        fontheight = sys_fontheight(font);
    drawnumber_getbuf(x, data, template, buf);
    width = 0;
    height = 1;
    for (startline = buf; newline = strchr(startline, '\n');
        startline = newline+1)
    {
        if (newline - startline > width)
            width = newline - startline;
        height++;
    }
    if (strlen(startline) > (unsigned)width)
        width = strlen(startline);
    *xp1 = xloc;
    *yp1 = yloc;
    *xp2 = xloc + fontwidth * width;
    *yp2 = yloc + fontheight * height;
}

static void drawnumber_displace(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int dx, int dy)
{
    /* refuse */
}

static void drawnumber_select(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int state)
{
    post("drawnumber_select %d", state);
    /* fill in later */
}

static void drawnumber_activate(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int state)
{
    post("drawnumber_activate %d", state);
}

static void drawnumber_vis(t_gobj *z, t_glist *glist, 
    t_word *data, t_template *template, t_float basex, t_float basey,
    int vis)
{
    t_drawnumber *x = (t_drawnumber *)z;
    
        /* see comment in plot_vis() */
    if (vis && !fielddesc_getfloat(&x->x_vis, template, data, 0))
        return;
    if (vis)
    {
        t_atom at;
        int xloc = glist_xtopixels(glist,
            basex + fielddesc_getcoord(&x->x_xloc, template, data, 0));
        int yloc = glist_ytopixels(glist,
            basey + fielddesc_getcoord(&x->x_yloc, template, data, 0));
        char colorstring[20], buf[DRAWNUMBER_BUFSIZE];
        numbertocolor(fielddesc_getfloat(&x->x_color, template, data, 1),
            colorstring);
        drawnumber_getbuf(x, data, template, buf);
        sys_vgui(".x%lx.c create text %d %d -anchor nw -fill %s -text {%s}",
                glist_getcanvas(glist), xloc, yloc, colorstring, buf);
        sys_vgui(" -font {{%s} -%d %s}", sys_font,
                 sys_hostfontsize(glist_getfont(glist)), sys_fontweight);
        sys_vgui(" -tags [list drawnumber%lx label]\n", data);
    }
    else sys_vgui(".x%lx.c delete drawnumber%lx\n", glist_getcanvas(glist), data);
}

static t_float drawnumber_motion_ycumulative;
static t_glist *drawnumber_motion_glist;
static t_scalar *drawnumber_motion_scalar;
static t_array *drawnumber_motion_array;
static t_word *drawnumber_motion_wp;
static t_template *drawnumber_motion_template;
static t_gpointer drawnumber_motion_gpointer;
static int drawnumber_motion_type;
static int drawnumber_motion_firstkey;

static void drawnumber_motion(void *z, t_floatarg dx, t_floatarg dy)
{
    t_drawnumber *x = (t_drawnumber *)z;
    t_atom at;
    if (!gpointer_check(&drawnumber_motion_gpointer, 0))
    {
        post("drawnumber_motion: scalar disappeared");
        return;
    }
    if (drawnumber_motion_type != DT_FLOAT)
        return;
    drawnumber_motion_ycumulative -= dy;
    template_setfloat(drawnumber_motion_template,
        x->x_fieldname,
            drawnumber_motion_wp, 
            drawnumber_motion_ycumulative,
                1);
    if (drawnumber_motion_scalar)
        template_notifyforscalar(drawnumber_motion_template,
            drawnumber_motion_glist, drawnumber_motion_scalar,
                gensym("change"), 1, &at);

    if (drawnumber_motion_scalar)
        scalar_redraw(drawnumber_motion_scalar, drawnumber_motion_glist);
    if (drawnumber_motion_array)
        array_redraw(drawnumber_motion_array, drawnumber_motion_glist);
}

static void drawnumber_key(void *z, t_floatarg fkey)
{
    t_drawnumber *x = (t_drawnumber *)z;
    int key = fkey;
    char sbuf[MAXPDSTRING];
    t_atom at;
    if (!gpointer_check(&drawnumber_motion_gpointer, 0))
    {
        post("drawnumber_motion: scalar disappeared");
        return;
    }
    if (key == 0)
        return;
    if (drawnumber_motion_type == DT_SYMBOL)
    {
            /* key entry for a symbol field */
        if (drawnumber_motion_firstkey)
            sbuf[0] = 0;
        else strncpy(sbuf, template_getsymbol(drawnumber_motion_template,
            x->x_fieldname, drawnumber_motion_wp, 1)->s_name,
                MAXPDSTRING);
        sbuf[MAXPDSTRING-1] = 0;
        if (key == '\b')
        {
            if (*sbuf)
                sbuf[strlen(sbuf)-1] = 0;
        }
        else
        {
            sbuf[strlen(sbuf)+1] = 0;
            sbuf[strlen(sbuf)] = key;
        }
    }
    else if (drawnumber_motion_type == DT_FLOAT)
    {
            /* key entry for a numeric field.  This is just a stopgap. */
        double newf;
        if (drawnumber_motion_firstkey)
            sbuf[0] = 0;
        else sprintf(sbuf, "%g", template_getfloat(drawnumber_motion_template,
            x->x_fieldname, drawnumber_motion_wp, 1));
        drawnumber_motion_firstkey = (key == '\n');
        if (key == '\b')
        {
            if (*sbuf)
                sbuf[strlen(sbuf)-1] = 0;
        }
        else
        {
            sbuf[strlen(sbuf)+1] = 0;
            sbuf[strlen(sbuf)] = key;
        }
        if (sscanf(sbuf, "%lg", &newf) < 1)
            newf = 0;
        template_setfloat(drawnumber_motion_template,
            x->x_fieldname, drawnumber_motion_wp, (t_float)newf, 1);
        if (drawnumber_motion_scalar)
            template_notifyforscalar(drawnumber_motion_template,
                drawnumber_motion_glist, drawnumber_motion_scalar,
                    gensym("change"), 1, &at);
        if (drawnumber_motion_scalar)
            scalar_redraw(drawnumber_motion_scalar, drawnumber_motion_glist);
        if (drawnumber_motion_array)
            array_redraw(drawnumber_motion_array, drawnumber_motion_glist);
    }
    else post("typing at text fields not yet implemented");
}

static int drawnumber_click(t_gobj *z, t_glist *glist, 
    t_word *data, t_template *template, t_scalar *sc, t_array *ap,
    t_float basex, t_float basey,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_drawnumber *x = (t_drawnumber *)z;
    int x1, y1, x2, y2, type, onset;
    drawnumber_getrect(z, glist,
        data, template, basex, basey,
        &x1, &y1, &x2, &y2);
    if (xpix >= x1 && xpix <= x2 && ypix >= y1 && ypix <= y2 &&
        ((type = drawnumber_gettype(x, data, template, &onset)) == DT_FLOAT ||
            type == DT_SYMBOL))
    {
        if (doit)
        {
            drawnumber_motion_glist = glist;
            drawnumber_motion_wp = data;
            drawnumber_motion_template = template;
            drawnumber_motion_scalar = sc;
            drawnumber_motion_array = ap;
            drawnumber_motion_firstkey = 1;
            drawnumber_motion_ycumulative =
                template_getfloat(template, x->x_fieldname, data, 0);
            drawnumber_motion_type = type;
            if (drawnumber_motion_scalar)
                gpointer_setglist(&drawnumber_motion_gpointer, 
                    drawnumber_motion_glist, drawnumber_motion_scalar);
            else gpointer_setarray(&drawnumber_motion_gpointer,
                    drawnumber_motion_array, drawnumber_motion_wp);
            glist_grab(glist, z, drawnumber_motion, drawnumber_key,
                xpix, ypix);
        }
        return (1);
    }
    else return (0);
}

t_parentwidgetbehavior drawnumber_widgetbehavior =
{
    drawnumber_getrect,
    drawnumber_displace,
    drawnumber_select,
    drawnumber_activate,
    drawnumber_vis,
    drawnumber_click,
};

static void drawnumber_free(t_drawnumber *x)
{
}

static void drawnumber_setup(void)
{
    drawnumber_class = class_new(gensym("drawtext"),
        (t_newmethod)drawnumber_new, (t_method)drawnumber_free,
        sizeof(t_drawnumber), 0, A_GIMME, 0);
    class_setdrawcommand(drawnumber_class);
    class_addfloat(drawnumber_class, drawnumber_float);
    class_addcreator((t_newmethod)drawnumber_new, gensym("drawsymbol"),
        A_GIMME, 0);
    class_addcreator((t_newmethod)drawnumber_new, gensym("drawnumber"),
        A_GIMME, 0);
    class_setparentwidget(drawnumber_class, &drawnumber_widgetbehavior);
}

/* ---------------------- setup function ---------------------------- */

void g_template_setup(void)
{
    template_setup();
    gtemplate_setup();
    curve_setup();
    plot_setup();
    drawnumber_setup();
}

