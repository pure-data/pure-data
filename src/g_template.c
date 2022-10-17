/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include <string.h>
#include <stdio.h>

#include "m_pd.h"
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

struct _instancetemplate
{
    int curve_motion_field;
    t_float curve_motion_xcumulative;
    t_float curve_motion_xbase;
    t_float curve_motion_xper;
    t_float curve_motion_ycumulative;
    t_float curve_motion_ybase;
    t_float curve_motion_yper;
    t_glist *curve_motion_glist;
    t_scalar *curve_motion_scalar;
    t_array *curve_motion_array;
    t_word *curve_motion_wp;
    t_template *curve_motion_template;
    t_gpointer curve_motion_gpointer;
    t_float array_motion_xcumulative;
    t_float array_motion_ycumulative;
    t_fielddesc *array_motion_xfield;
    t_fielddesc *array_motion_yfield;
    t_glist *array_motion_glist;
    t_scalar *array_motion_scalar;
    t_array *array_motion_array;
    t_word *array_motion_wp;
    t_template *array_motion_template;
    int array_motion_npoints;
    int array_motion_elemsize;
    int array_motion_altkey;
    t_float array_motion_initx;
    t_float array_motion_xperpix;
    t_float array_motion_yperpix;
    int array_motion_lastx;
    int array_motion_fatten;
    t_float drawnumber_motion_ycumulative;
    t_glist *drawnumber_motion_glist;
    t_scalar *drawnumber_motion_scalar;
    t_array *drawnumber_motion_array;
    t_word *drawnumber_motion_wp;
    t_template *drawnumber_motion_template;
    t_gpointer drawnumber_motion_gpointer;
    int drawnumber_motion_type;
    int drawnumber_motion_firstkey;
};


/* ---------------- forward definitions ---------------- */

static void template_addtolist(t_template *x);
static void template_takeofflist(t_template *x);
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

    /* add a template to the list */
static void template_addtolist(t_template *x)
{
    x->t_next = pd_this->pd_templatelist;
    pd_this->pd_templatelist = x;
}

static void template_takeofflist(t_template *x)
{
        /* take it off the template list */
    if (x == pd_this->pd_templatelist) pd_this->pd_templatelist = x->t_next;
    else
    {
        t_template *z;
        for (z = pd_this->pd_templatelist; z->t_next != x; z = z->t_next)
            if (!z->t_next) return;
        z->t_next = x->t_next;
    }
}

t_template *template_new(t_symbol *templatesym, int argc, t_atom *argv)
{
    t_template *x = (t_template *)pd_new(template_class);
    x->t_n = 0;
    x->t_vec = (t_dataslot *)t_getbytes(0);
    x->t_next = 0;
    template_addtolist(x);
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
        else if (loud) pd_error(0, "%s.%s: not a number",
            x->t_sym->s_name, fieldname->s_name);
    }
    else if (loud) pd_error(0, "%s.%s: no such field",
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
        else if (loud) pd_error(0, "%s.%s: not a number",
            x->t_sym->s_name, fieldname->s_name);
    }
    else if (loud) pd_error(0, "%s.%s: no such field",
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
        else if (loud) pd_error(0, "%s.%s: not a symbol",
            x->t_sym->s_name, fieldname->s_name);
    }
    else if (loud) pd_error(0, "%s.%s: no such field",
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
        else if (loud) pd_error(0, "%s.%s: not a symbol",
            x->t_sym->s_name, fieldname->s_name);
    }
    else if (loud) pd_error(0, "%s.%s: no such field",
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
            for (y = glist->gl_list; (y2 = y->g_next); y = y2)
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
        for (gl = pd_getcanvaslist(); gl; gl = gl->gl_next)
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
    {
        bug("template_findcanvas");
        return (0);
    }
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
                pd_error(0, "%s: template mismatch",
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
    template_takeofflist(x);
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
            for (x2 = x->x_template->t_list; (x3 = x2->x_next); x2 = x3)
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
                x->x_template = t = template_new(sym, argc, argv);
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
        argc--, argv++;
    if (sym->s_name[0] == '-')
        post("warning: struct '%s' initial '-' may confuse get/set, etc.",
            sym->s_name);
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
        for (x2 = t->t_list; (x3 = x2->x_next); x2 = x3)
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
        int cpy = (int)(s1 - s->s_name), got;
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

#define CLOSED 1      /* polygon */
#define BEZ 2         /* bezier shape */
#define NOMOUSERUN 4  /* disable mouse interaction when in run mode  */
#define NOMOUSEEDIT 8 /* same in edit mode */
#define NOVERTICES 16 /* disable only vertex grabbing in run mode */
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
            pd_error(0, "symbolic data field used as number");
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
            pd_error(0, "symbolic data field used as number");
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
            pd_error(0, "numeric data field used as symbol");
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
            pd_error(0, "attempt to set constant or symbolic data field to a number");
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
    int x_flags;    /* CLOSED, BEZ, NOMOUSERUN, NOMOUSEEDIT */
    t_fielddesc x_fillcolor;
    t_fielddesc x_outlinecolor;
    t_fielddesc x_width;
    t_fielddesc x_vis;
    int x_npoints;
    t_fielddesc *x_vec;
    t_canvas *x_canvas;
} t_curve;

static void *curve_new(t_symbol *classsym, int argc, t_atom *argv)
{
    t_curve *x = (t_curve *)pd_new(curve_class);
    const char *classname = classsym->s_name;
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
    while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        const char *flag = argv->a_w.w_symbol->s_name;
        if (!strcmp(flag, "-n"))
        {
            fielddesc_setfloat_const(&x->x_vis, 0);
        }
        else if (!strcmp(flag, "-v") && argc > 1)
        {
            fielddesc_setfloatarg(&x->x_vis, 1, argv+1);
            argc -= 1; argv += 1;
        }
        else if (!strcmp(flag, "-x"))
        {
            /* disable all mouse interaction */
            flags |= (NOMOUSERUN | NOMOUSEEDIT);
        }
        else if (!strcmp(flag, "-xr"))
        {
            /* disable mouse actions in run mode */
            flags |= NOMOUSERUN;
        }
        else if (!strcmp(flag, "-xe"))
        {
            /* disable mouse actions in edit mode */
            flags |= NOMOUSEEDIT;
        }
        else if (!strcmp(flag, "-xv"))
        {
            /* disable changing vertices in run mode */
            flags |= NOVERTICES;
        }
        else
        {
            pd_error(x, "%s: unknown flag '%s'...", classsym->s_name,
                flag);
        }
        argc--; argv++;
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
        (glist->gl_edit && x->x_flags & NOMOUSEEDIT) ||
        (!glist->gl_edit && x->x_flags & NOMOUSERUN))
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

static int numbertocolor(int n)
{
    int red, blue, green, color = 0;
    if (n < 0) n = 0;
    red = n / 100;
    blue = ((n / 10) % 10);
    green = n % 10;
    color |= rangecolor(red)   << 16;
    color |= rangecolor(blue)  <<  8;
    color |= rangecolor(green) <<  0;
    return color;
}

static void curve_vis(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int vis)
{
    t_curve *x = (t_curve *)z;
    int i, n = x->x_npoints;
    t_fielddesc *f = x->x_vec;
    char tag0[80], tag[80];
    const char*tags[] = {tag, tag0, "curve"};
        /* see comment in plot_vis() */
    if (vis && !fielddesc_getfloat(&x->x_vis, template, data, 0))
        return;
    sprintf(tag0, "curve%lx", x);
    sprintf(tag , "curve%lx_data%lx", x, data);
    if (vis)
    {
        if (n > 1)
        {
            int flags = x->x_flags, closed = (flags & CLOSED);
            t_float width = fielddesc_getfloat(&x->x_width, template, data, 1);
            int outline;
            t_word pix[200];

            if (n > 100)
                n = 100;
                /* calculate the pixel values before we start printing
                out the TK message so that "error" printout won't be
                interspersed with it.  Only show up to 100 points so we don't
                have to allocate memory here. */
            for (i = 0, f = x->x_vec; i < n; i++, f += 2)
            {
                pix[2*i].w_float = glist_xtopixels(glist,
                    basex + fielddesc_getcoord(f, template, data, 1));
                pix[2*i+1].w_float = glist_ytopixels(glist,
                    basey + fielddesc_getcoord(f+1, template, data, 1));
            }
            if (width < 1) width = 1;
            if (glist->gl_isgraph)
                width *= glist_getzoom(glist);
            outline = numbertocolor(
                fielddesc_getfloat(&x->x_outlinecolor, template, data, 1));

            pdgui_vmess(0, "crr iiii rf ri rS",
                glist_getcanvas(glist), "create",
                (flags & CLOSED)?"polygon":"line",
                0, 0, 0, 0,
                "-width", width,
                "-smooth", !!(flags & BEZ),
                "-tags", 3, tags);

            pdgui_vmess(0, "crs w",
                glist_getcanvas(glist), "coords", tag,
                2*n, pix);

            if (flags & CLOSED)
            {
                int fill = numbertocolor(
                    fielddesc_getfloat(&x->x_fillcolor, template, data, 1));
                pdgui_vmess(0, "crs rk rk",
                    glist_getcanvas(glist), "itemconfigure", tag,
                    "-fill", fill,
                    "-outline", outline);
            } else
                pdgui_vmess(0, "crs rk",
                    glist_getcanvas(glist), "itemconfigure", tag,
                    "-fill", outline);
        }
        else post("warning: drawing shapes need at least two points to be graphed");
    }
    else
    {
        if (n > 1)
            pdgui_vmess(0, "crs", glist_getcanvas(glist), "delete", tag);
    }
}

    /* LATER protect against the template changing or the scalar disappearing
    probably by attaching a gpointer here ... */

static void curve_motionfn(void *z, t_floatarg dx, t_floatarg dy, t_floatarg up)
{
    t_curve *x = (t_curve *)z;
    t_fielddesc *f = x->x_vec + TEMPLATE->curve_motion_field;
    t_atom at;
    if (up != 0)
        return;
    if (!gpointer_check(&TEMPLATE->curve_motion_gpointer, 0))
    {
        post("curve_motion: scalar disappeared");
        return;
    }
    TEMPLATE->curve_motion_xcumulative += dx;
    TEMPLATE->curve_motion_ycumulative += dy;
    if (f->fd_var && (dx != 0))
    {
        fielddesc_setcoord(f, TEMPLATE->curve_motion_template,
            TEMPLATE->curve_motion_wp,
            TEMPLATE->curve_motion_xbase +
            TEMPLATE->curve_motion_xcumulative * TEMPLATE->curve_motion_xper,
                1);
    }
    if ((f+1)->fd_var && (dy != 0))
    {
        fielddesc_setcoord(f+1, TEMPLATE->curve_motion_template,
            TEMPLATE->curve_motion_wp,
            TEMPLATE->curve_motion_ybase +
            TEMPLATE->curve_motion_ycumulative * TEMPLATE->curve_motion_yper,
                1);
    }
        /* LATER figure out what to do to notify for an array? */
    if (TEMPLATE->curve_motion_scalar)
        template_notifyforscalar(TEMPLATE->curve_motion_template,
            TEMPLATE->curve_motion_glist,
            TEMPLATE->curve_motion_scalar, gensym("change"), 1, &at);
    if (TEMPLATE->curve_motion_scalar)
        scalar_redraw(TEMPLATE->curve_motion_scalar,
            TEMPLATE->curve_motion_glist);
    if (TEMPLATE->curve_motion_array)
        array_redraw(TEMPLATE->curve_motion_array,
            TEMPLATE->curve_motion_glist);
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
    if ((x->x_flags & NOMOUSERUN) || (x->x_flags & NOVERTICES) ||
        !fielddesc_getfloat(&x->x_vis, template, data, 0))
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
            TEMPLATE->curve_motion_xbase = xval;
            TEMPLATE->curve_motion_ybase = yval;
            besterror = xerr;
            bestn = i;
        }
    }
    if (besterror > 6)
        return (0);
    if (doit)
    {
        TEMPLATE->curve_motion_xper = glist_pixelstox(glist, 1)
            - glist_pixelstox(glist, 0);
        TEMPLATE->curve_motion_yper = glist_pixelstoy(glist, 1)
            - glist_pixelstoy(glist, 0);
        TEMPLATE->curve_motion_xcumulative = 0;
        TEMPLATE->curve_motion_ycumulative = 0;
        TEMPLATE->curve_motion_glist = glist;
        TEMPLATE->curve_motion_scalar = sc;
        TEMPLATE->curve_motion_array = ap;
        TEMPLATE->curve_motion_wp = data;
        TEMPLATE->curve_motion_field = 2*bestn;
        TEMPLATE->curve_motion_template = template;
        if (TEMPLATE->curve_motion_scalar)
            gpointer_setglist(&TEMPLATE->curve_motion_gpointer,
                TEMPLATE->curve_motion_glist, TEMPLATE->curve_motion_scalar);
        else gpointer_setarray(&TEMPLATE->curve_motion_gpointer,
                TEMPLATE->curve_motion_array, TEMPLATE->curve_motion_wp);
        glist_grab(glist, z, curve_motionfn, 0, xpix, ypix);
    }
    return (1);
}

const t_parentwidgetbehavior curve_widgetbehavior =
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
    class_sethelpsymbol(curve_class, gensym("draw-shapes"));
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
    t_fielddesc x_edit;         /* enable/disable mouse editing */
} t_plot;

static void *plot_new(t_symbol *classsym, int argc, t_atom *argv)
{
    t_plot *x = (t_plot *)pd_new(plot_class);
    int defstyle = PLOTSTYLE_POLY;
    x->x_canvas = canvas_getcurrent();

    fielddesc_setfloat_var(&x->x_xpoints, gensym("x"));
    fielddesc_setfloat_var(&x->x_ypoints, gensym("y"));
    fielddesc_setfloat_var(&x->x_wpoints, gensym("w"));

    fielddesc_setfloat_const(&x->x_vis, 1);
    fielddesc_setfloat_const(&x->x_scalarvis, 1);
    fielddesc_setfloat_const(&x->x_edit, 1);
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
        else if (!strcmp(firstarg->s_name, "-e") && argc > 1)
        {
            fielddesc_setfloatarg(&x->x_edit, 1, argv+1);
            argc -= 2; argv += 2;
        }
        else if (*firstarg->s_name == '-')
        {
            pd_error(x, "%s: unknown flag '%s'...", classsym->s_name,
                firstarg->s_name);
            argc--; argv++;
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
    t_float *linewidthp, t_float *xlocp, t_float *xincp, t_float *ylocp,
    t_float *stylep, t_float *visp, t_float *scalarvisp, t_float *editp,
    t_fielddesc **xfield, t_fielddesc **yfield, t_fielddesc **wfield)
{
    int arrayonset, type;
    t_symbol *elemtemplatesym;
    t_array *array;

        /* find the data and verify it's an array */
    if (x->x_data.fd_type != A_ARRAY || !x->x_data.fd_var)
    {
        pd_error(0, "plot: needs an array field");
        return (-1);
    }
    if (!template_find_field(ownertemplate, x->x_data.fd_un.fd_varsym,
        &arrayonset, &type, &elemtemplatesym))
    {
        pd_error(0, "plot: %s: no such field", x->x_data.fd_un.fd_varsym->s_name);
        return (-1);
    }
    if (type != DT_ARRAY)
    {
        pd_error(0, "plot: %s: not an array", x->x_data.fd_un.fd_varsym->s_name);
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
    *editp = fielddesc_getfloat(&x->x_edit, ownertemplate, data, 1);
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
        pd_error(0, "plot: %s: no such template", elemtemplatesym->s_name);
        return (-1);
    }
    if (!((elemtemplatesym == &s_float) ||
        (elemtemplatecanvas = template_findcanvas(elemtemplate))))
    {
        pd_error(0, "plot: %s: no canvas for this template", elemtemplatesym->s_name);
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
    t_float linewidth, xloc, xinc, yloc, style, yval, vis, scalarvis, edit;
    double xsum;
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
            &vis, &scalarvis, &edit, &xfielddesc, &yfielddesc, &wfielddesc) &&
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
                    const t_parentwidgetbehavior *wb =
                        pd_getparentwidget(&y->g_pd);
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
    t_float linewidth, xloc, xinc, yloc, style, yval,
        vis, scalarvis, edit;
    double xsum;
    t_array *array;
    int nelem;
    char *elem;
    t_fielddesc *xfielddesc, *yfielddesc, *wfielddesc;
    char tag[80], tag0[80];
    const char*tags[] = {tag, tag0, "array"};
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
        &vis, &scalarvis, &edit, &xfielddesc, &yfielddesc, &wfielddesc) ||
            ((vis == 0) && tovis) /* see above for 'tovis' */
            || array_getfields(elemtemplatesym, &elemtemplatecanvas,
                &elemtemplate, &elemsize, xfielddesc, yfielddesc, wfielddesc,
                &xonset, &yonset, &wonset))
                    return;
    nelem = array->a_n;
    elem = (char *)array->a_vec;

    sprintf(tag , "plot%lx", data);
        /* a tag that uniquely identifies the sub-plot */
    sprintf(tag0, "plot%lx_array%lx_onset%+d%+d%+d", data, elem, wonset, xonset, yonset);

    if (glist->gl_isgraph)
        linewidth *= glist_getzoom(glist);

    if (tovis)
    {
         /* we use t_word because pdgui_vmess() has a convenient FLOATWORDS type
          * FLOATARRAY is impractical (as it sends a list, and the GUI expects arguments)
          */
        t_word coordinates[1024*2];

        if (style == PLOTSTYLE_POINTS)
        {
            t_float minyval = 1e20, maxyval = -1e20;
            int ndrawn = 0;
            int color = numbertocolor(
                fielddesc_getfloat(&x->x_outlinecolor, template, data, 1));

            for (xsum = basex + xloc, i = 0; i < nelem; i++)
            {
                t_float yval, xpix, ypix, nextxloc, usexloc;
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

                    pdgui_vmess(0, "crr iiii rk rf rS",
                        glist_getcanvas(glist), "create", "rectangle",
                        ixpix , (int) glist_ytopixels(glist, basey + fielddesc_cvttocoord(yfielddesc, minyval)),
                        inextx, (int)(glist_ytopixels(glist, basey + fielddesc_cvttocoord(yfielddesc, maxyval)) + linewidth),
                        "-fill", color,
                        "-width", 0.,
                        "-tags", 3, tags);
                    ndrawn++;
                    minyval = 1e20;
                    maxyval = -1e20;
                }
                if (ndrawn > 2000) break;
            }
        }
        else
        {
            int outline = numbertocolor(
                fielddesc_getfloat(&x->x_outlinecolor, template, data, 1));
            int lastpixel = -1, ndrawn = 0;
            t_float yval = 0, wval = 0, xpix;
            int ixpix = 0;
                /* draw the trace */


            if (wonset >= 0)
            {
                    /* found "w" field which controls linewidth.  The trace is
                    a filled polygon with 2n points. */
                for (i = 0, xsum = xloc; i < nelem; i++)
                {
                    t_float usexloc;
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
                        coordinates[ndrawn*2+0].w_float = ixpix;
                        coordinates[ndrawn*2+1].w_float = glist_ytopixels(
                            glist,
                            basey
                            + yloc
                            + fielddesc_cvttocoord(yfielddesc, yval)
                            - fielddesc_cvttocoord(wfielddesc, wval));
                        ndrawn++;
                    }
                    lastpixel = ixpix;
                    if (ndrawn*2 >= sizeof(coordinates)/sizeof(*coordinates))
                        goto ouch;
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
                        coordinates[ndrawn*2+0].w_float = ixpix;
                        coordinates[ndrawn*2+1].w_float = glist_ytopixels(
                            glist,
                            basey
                            + yloc
                            + fielddesc_cvttocoord(yfielddesc, yval)
                            + fielddesc_cvttocoord(wfielddesc, wval));
                        ndrawn++;
                    }
                    lastpixel = ixpix;
                    if (ndrawn*2 >= sizeof(coordinates)/sizeof(*coordinates))
                        goto ouch;
                }

                    /* TK will complain if there aren't at least 3 points.
                    There should be at least two already. */
                if (ndrawn < 4)
                {
                    coordinates[ndrawn*2+0].w_float = ixpix + 10;
                    coordinates[ndrawn*2+1].w_float = glist_ytopixels(
                        glist,
                        basey
                        + yloc
                        + fielddesc_cvttocoord(yfielddesc, yval)
                        - fielddesc_cvttocoord(wfielddesc, wval));
                    ndrawn++;

                    coordinates[ndrawn*2+0].w_float = ixpix + 10;
                    coordinates[ndrawn*2+1].w_float = glist_ytopixels(
                        glist,
                        basey
                        + yloc
                        + fielddesc_cvttocoord(yfielddesc, yval)
                        + fielddesc_cvttocoord(wfielddesc, wval));
                    ndrawn++;
                }
            ouch:

                pdgui_vmess(0, "crr ri rk rk ri rS",
                    glist_getcanvas(glist), "create", "polygon",
                    "-width", (glist->gl_isgraph ? glist_getzoom(glist) : 1),
                    "-fill", outline,
                    "-outline", outline,
                    "-smooth", (style == PLOTSTYLE_BEZ),
                    "-tags", 3, tags);

                pdgui_vmess(0, "crs w",
                    glist_getcanvas(glist), "coords", tag0,
                    ndrawn*2, coordinates);
            }
            else if (linewidth > 0)
            {
                    /* no "w" field.  If the linewidth is positive, draw a
                    segmented line with the requested width; otherwise don't
                    draw the trace at all. */
                for (i = 0, xsum = xloc; i < nelem; i++)
                {
                    t_float usexloc;
                    if (xonset >= 0)
                        usexloc = xloc + *(t_float *)((elem + elemsize * i)
                            + xonset);
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
                        coordinates[ndrawn*2+0].w_float = ixpix;
                        coordinates[ndrawn*2+1].w_float = glist_ytopixels(
                            glist,
                            basey
                            + yloc
                            + fielddesc_cvttocoord(yfielddesc, yval));
                        ndrawn++;
                    }
                    lastpixel = ixpix;
                    if (ndrawn*2 >= sizeof(coordinates)/sizeof(*coordinates)) break;
                }

                    /* TK will complain if there aren't at least 2 points... */
                if (ndrawn == 1)
                {
                    coordinates[2].w_float = ixpix + 10;
                    coordinates[3].w_float = glist_ytopixels(glist, basey + yloc + fielddesc_cvttocoord(yfielddesc, yval));
                    ndrawn = 2;
                }

                if(ndrawn)
                {
                    pdgui_vmess(0, "crr iiii rf rk ri rS",
                        glist_getcanvas(glist), "create", "line",
                        0, 0, 0, 0,
                        "-width", linewidth,
                        "-fill", outline,
                        "-smooth", (style == PLOTSTYLE_BEZ),
                        "-tags", 3, tags);
                    pdgui_vmess(0, "crs w",
                        glist_getcanvas(glist), "coords", tag0,
                        ndrawn*2, coordinates);
                }
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
                    const t_parentwidgetbehavior *wb =
                        pd_getparentwidget(&y->g_pd);
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
                    const t_parentwidgetbehavior *wb =
                        pd_getparentwidget(&y->g_pd);
                    if (!wb) continue;
                    (*wb->w_parentvisfn)(y, glist,
                        (t_word *)(elem + elemsize * i), elemtemplate,
                            0, 0, 0);
                }
            }
        }
            /* and then the trace */
        pdgui_vmess(0, "crs", glist_getcanvas(glist), "delete", tag);
    }
}

    /* LATER protect against the template changing or the scalar disappearing
    probably by attaching a gpointer here ... */

static void array_motionfn(void *z, t_floatarg dx, t_floatarg dy, t_floatarg up)
{
    if (up != 0)
        return;
    TEMPLATE->array_motion_xcumulative += dx * TEMPLATE->array_motion_xperpix;
    TEMPLATE->array_motion_ycumulative += dy * TEMPLATE->array_motion_yperpix;
    if (TEMPLATE->array_motion_xfield)
    {
            /* it's an x, y plot */
        int i;
        for (i = 0; i < TEMPLATE->array_motion_npoints; i++)
        {
            t_word *thisword = (t_word *)(((char *)TEMPLATE->array_motion_wp) +
                i * TEMPLATE->array_motion_elemsize);
            t_float xwas = fielddesc_getcoord(TEMPLATE->array_motion_xfield,
                TEMPLATE->array_motion_template, thisword, 1);
            t_float ywas = (TEMPLATE->array_motion_yfield ?
                fielddesc_getcoord(TEMPLATE->array_motion_yfield,
                    TEMPLATE->array_motion_template, thisword, 1) : 0);
            fielddesc_setcoord(TEMPLATE->array_motion_xfield,
                TEMPLATE->array_motion_template, thisword,
                    xwas + dx * TEMPLATE->array_motion_xperpix, 1);
            if (TEMPLATE->array_motion_yfield)
            {
                if (TEMPLATE->array_motion_fatten)
                {
                    if (i == 0)
                    {
                        t_float newy = ywas +
                            dy * TEMPLATE->array_motion_yperpix;
                        if (newy < 0)
                            newy = 0;
                        fielddesc_setcoord(TEMPLATE->array_motion_yfield,
                            TEMPLATE->array_motion_template, thisword, newy, 1);
                    }
                }
                else
                {
                    fielddesc_setcoord(TEMPLATE->array_motion_yfield,
                        TEMPLATE->array_motion_template, thisword,
                            ywas + dy * TEMPLATE->array_motion_yperpix, 1);
                }
            }
        }
    }
    else if (TEMPLATE->array_motion_yfield)
    {
            /* a y-only plot. */
        int thisx = TEMPLATE->array_motion_initx +
            TEMPLATE->array_motion_xcumulative + 0.5, x2;
        int increment, i, nchange;
        t_float newy = TEMPLATE->array_motion_ycumulative,
            oldy = fielddesc_getcoord(TEMPLATE->array_motion_yfield,
                TEMPLATE->array_motion_template,
                    (t_word *)(((char *)TEMPLATE->array_motion_wp) +
                        TEMPLATE->array_motion_elemsize *
                            TEMPLATE->array_motion_lastx),
                            1);
        t_float ydiff = newy - oldy;
        if (thisx < 0) thisx = 0;
        else if (thisx >= TEMPLATE->array_motion_npoints)
            thisx = TEMPLATE->array_motion_npoints - 1;
        increment = (thisx > TEMPLATE->array_motion_lastx ? -1 : 1);
        nchange = 1 + increment * (TEMPLATE->array_motion_lastx - thisx);

        for (i = 0, x2 = thisx; i < nchange; i++, x2 += increment)
        {
            fielddesc_setcoord(TEMPLATE->array_motion_yfield,
                TEMPLATE->array_motion_template,
                    (t_word *)(((char *)TEMPLATE->array_motion_wp) +
                        TEMPLATE->array_motion_elemsize * x2), newy, 1);
            if (nchange > 1)
                newy -= ydiff * (1./(nchange - 1));
         }
         TEMPLATE->array_motion_lastx = thisx;
    }
    if (TEMPLATE->array_motion_scalar)
        scalar_redraw(TEMPLATE->array_motion_scalar,
            TEMPLATE->array_motion_glist);
    if (TEMPLATE->array_motion_array)
        array_redraw(TEMPLATE->array_motion_array,
            TEMPLATE->array_motion_glist);
}

int scalar_doclick(t_word *data, t_template *template, t_scalar *sc,
    t_array *ap, struct _glist *owner,
    t_float xloc, t_float yloc, int xpix, int ypix,
    int shift, int alt, int dbl, int doit);

    /* try clicking on an element of the array as a scalar (if clicking
    on the trace of the array failed) */
static int array_doclick_element(t_array *array, t_glist *glist,
    t_symbol *elemtemplatesym,
    t_float xloc, t_float xinc, t_float yloc,
    t_fielddesc *xfield, t_fielddesc *yfield, t_fielddesc *wfield,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_canvas *elemtemplatecanvas;
    t_template *elemtemplate;
    int elemsize, yonset, wonset, xonset, i, incr, hit;
    double xsum;

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
            *(t_float *)
                (((char *)(array->a_vec) + elemsize * i) + yonset)) : 0);

        if ((hit = scalar_doclick(
            (t_word *)((char *)(array->a_vec) + i * elemsize),
            elemtemplate, 0, array,
            glist, usexloc, useyloc,
            xpix, ypix, shift, alt, dbl, doit)))
                return (hit);
    }
    return (0);
}

static int array_doclick(t_array *array, t_glist *glist, t_scalar *sc,
    t_array *ap, t_symbol *elemtemplatesym,
    t_float xloc, t_float xinc, t_float yloc,
    t_float scalarvis, t_float edit,
    t_fielddesc *xfield, t_fielddesc *yfield, t_fielddesc *wfield,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_canvas *elemtemplatecanvas;
    t_template *elemtemplate;
    int elemsize, yonset, wonset, xonset, i;

    if (!array_getfields(elemtemplatesym, &elemtemplatecanvas,
        &elemtemplate, &elemsize, xfield, yfield, wfield,
        &xonset, &yonset, &wonset))
    {
        t_float best = 100;
            /* if it has more than 2000 points, just check 1000 of them. */
        int incr = (array->a_n <= 2000 ? 1 : array->a_n / 1000);
        TEMPLATE->array_motion_elemsize = elemsize;
        TEMPLATE->array_motion_glist = glist;
        TEMPLATE->array_motion_scalar = sc;
        TEMPLATE->array_motion_array = ap;
        TEMPLATE->array_motion_template = elemtemplate;
        TEMPLATE->array_motion_xperpix = glist_dpixtodx(glist, 1);
        TEMPLATE->array_motion_yperpix = glist_dpixtody(glist, 1);
            /* if we're a garray, the only one here, and if we appear to have
            only a 'y' field, click always succeeds and furthermore we'll
            call "motion" later. */
        if (glist->gl_list && pd_class(&glist->gl_list->g_pd) == garray_class
            && !glist->gl_list->g_next &&
                elemsize == sizeof(t_word))
        {
            int xval = glist_pixelstox(glist, xpix);
            if (xval < 0)
                xval = 0;
            else if (xval >= array->a_n)
                xval = array->a_n - 1;
            TEMPLATE->array_motion_yfield = yfield;
            TEMPLATE->array_motion_ycumulative = glist_pixelstoy(glist, ypix);
            TEMPLATE->array_motion_fatten = 0;
            TEMPLATE->array_motion_xfield = 0;
            TEMPLATE->array_motion_xcumulative = 0;
            TEMPLATE->array_motion_lastx = TEMPLATE->array_motion_initx = xval;
            TEMPLATE->array_motion_npoints = array->a_n;
            TEMPLATE->array_motion_wp = (t_word *)((char *)array->a_vec);
            if (doit)
            {
                fielddesc_setcoord(yfield, elemtemplate,
                    (t_word *)(((char *)array->a_vec) + elemsize * xval),
                        glist_pixelstoy(glist, ypix), 1);
                glist_grab(glist, 0, array_motionfn, 0, xpix, ypix);
                if (TEMPLATE->array_motion_scalar)
                    scalar_redraw(TEMPLATE->array_motion_scalar,
                        TEMPLATE->array_motion_glist);
                if (TEMPLATE->array_motion_array)
                    array_redraw(TEMPLATE->array_motion_array,
                        TEMPLATE->array_motion_glist);
            }
        }
        else
        {
                /* First we get the closest distance to any element */
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
                /* If we're not too close, we first try to click a scalar
                (if visible). This would not affect the array */
            if (best > 8)
            {
                if (scalarvis != 0)
                {
                    return (array_doclick_element(array, glist,
                        elemtemplatesym, xloc, xinc, yloc,
                            xfield, yfield, wfield,
                            xpix, ypix, shift, alt, dbl, doit));

                }
                else return (0);
            }
                /* Now we walk over the array again and decide whether we
                a) grab an element, b) change the line width,
                c) delete an element or d) add a new element */
            best += 0.001;  /* add truncation error margin */
             /* otherwise we try to grab a vertex */
            if (!edit)
                return (0);
            for (i = 0; i < array->a_n; i += incr)
            {
                t_float pxpix, pypix, pwpix, dx, dy, dy2, dy3;
                array_getcoordinate(glist,
                    (char *)(array->a_vec) + i * elemsize,
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
                        TEMPLATE->array_motion_fatten = 0;
                    else if (dy2 < dy3)
                        TEMPLATE->array_motion_fatten = -1;
                    else TEMPLATE->array_motion_fatten = 1;
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
                            array_resize_and_redraw(array,
                                glist, array->a_n - 1);
                            return (0);
                        }
                        else if (alt)
                        {
                            /* add a point (after the clicked-on one) */
                            array_resize_and_redraw(array, glist,
                                array->a_n + 1);
                            elem = (char *)array->a_vec;
                            memmove(elem + elemsize * (i+1),
                                elem + elemsize * i,
                                    (array->a_n - i - 1) * elemsize);
                            i++;
                        }
                        if (xonset >= 0)
                        {
                            TEMPLATE->array_motion_xfield = xfield;
                            TEMPLATE->array_motion_xcumulative =
                                fielddesc_getcoord(xfield,
                                    TEMPLATE->array_motion_template,
                                    (t_word *)(elem + i * elemsize), 1);
                                TEMPLATE->array_motion_wp =
                                    (t_word *)(elem + i * elemsize);
                            if (shift)
                                TEMPLATE->array_motion_npoints =
                                    array->a_n - i;
                            else TEMPLATE->array_motion_npoints = 1;
                        }
                        else
                        {
                            TEMPLATE->array_motion_xfield = 0;
                            TEMPLATE->array_motion_xcumulative = 0;
                            TEMPLATE->array_motion_wp = (t_word *)elem;
                            TEMPLATE->array_motion_npoints = array->a_n;

                            TEMPLATE->array_motion_initx = i;
                            TEMPLATE->array_motion_lastx = i;
                            TEMPLATE->array_motion_xperpix *=
                                (xinc == 0 ? 1 : 1./xinc);
                        }
                        if (TEMPLATE->array_motion_fatten)
                        {
                            TEMPLATE->array_motion_yfield = wfield;
                            TEMPLATE->array_motion_ycumulative =
                                fielddesc_getcoord(wfield,
                                    TEMPLATE->array_motion_template,
                                    (t_word *)(elem + i * elemsize), 1);
                            if (TEMPLATE->array_motion_yperpix < 0)
                                TEMPLATE->array_motion_yperpix *= -1;
                            TEMPLATE->array_motion_yperpix *=
                                -TEMPLATE->array_motion_fatten;
                        }
                        else if (yonset >= 0)
                        {
                            TEMPLATE->array_motion_yfield = yfield;
                            TEMPLATE->array_motion_ycumulative =
                                fielddesc_getcoord(yfield,
                                    TEMPLATE->array_motion_template,
                                    (t_word *)(elem + i * elemsize), 1);
                        }
                        else
                        {
                            TEMPLATE->array_motion_yfield = 0;
                            TEMPLATE->array_motion_ycumulative = 0;
                        }
                        glist_grab(glist, 0, array_motionfn, 0, xpix, ypix);
                    }
                    if (alt)
                    {
                        if (xpix < pxpix)
                            return (CURSOR_EDITMODE_DISCONNECT);
                        else return (CURSOR_RUNMODE_ADDPOINT);
                    }
                    else return (TEMPLATE->array_motion_fatten ?
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
    t_float linewidth, xloc, xinc, yloc, style, vis, scalarvis, edit;
    t_array *array;
    t_fielddesc *xfielddesc, *yfielddesc, *wfielddesc;

    if (!plot_readownertemplate(x, data, template,
        &elemtemplatesym, &array, &linewidth, &xloc, &xinc, &yloc, &style,
        &vis, &scalarvis, &edit,
        &xfielddesc, &yfielddesc, &wfielddesc) && (vis != 0))
    {
        return (array_doclick(array, glist, sc, ap, elemtemplatesym,
            basex + xloc, xinc, basey + yloc, scalarvis, edit,
            xfielddesc, yfielddesc, wfielddesc,
            xpix, ypix, shift, alt, dbl, doit));
    }
    else return (0);
}

const t_parentwidgetbehavior plot_widgetbehavior =
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

static void *drawnumber_new(t_symbol *classsym, int argc, t_atom *argv)
{
    t_drawnumber *x = (t_drawnumber *)pd_new(drawnumber_class);
    const char *classname = classsym->s_name;

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
        nchars = (int)strlen(buf);
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
            switch(type)
            {
            default:
                SETSYMBOL(&at, ((t_word *)((char *)data + onset))->w_symbol);
                atom_string(&at, buf + nchars, DRAWNUMBER_BUFSIZE - nchars);
                break;
            case DT_FLOAT:
                SETFLOAT(&at, ((t_word *)((char *)data + onset))->w_float);
                atom_string(&at, buf + nchars, DRAWNUMBER_BUFSIZE - nchars);
                break;
            case DT_SYMBOL:
                strncpy(buf + nchars,
                    ((t_word *)((char *)data + onset))->w_symbol->s_name,
                    DRAWNUMBER_BUFSIZE - nchars);
            }
        }
    }
}

static void drawnumber_getrect(t_gobj *z, t_glist *glist,
    t_word *data, t_template *template, t_float basex, t_float basey,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_drawnumber *x = (t_drawnumber *)z;
    t_atom at;
    int xloc, yloc, fontwidth, fontheight, bufsize, width, height;
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
    fontwidth = glist_fontwidth(glist);
    fontheight = glist_fontheight(glist);
    drawnumber_getbuf(x, data, template, buf);
    width = 0;
    height = 1;
    for (startline = buf; (newline = strchr(startline, '\n'));
        startline = newline+1)
    {
        if (newline - startline > width)
            width = (int)(newline - startline);
        height++;
    }
    if (strlen(startline) > (unsigned)width)
        width = (int)strlen(startline);
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
    char tag[80];
    const char*tags[] = {tag, "label"};

        /* see comment in plot_vis() */
    if (vis && !fielddesc_getfloat(&x->x_vis, template, data, 0))
        return;
    sprintf(tag, "drawnumber%lx", data);
    if (vis)
    {
        t_atom fontatoms[3];
        t_atom at;
        int xloc = glist_xtopixels(glist,
            basex + fielddesc_getcoord(&x->x_xloc, template, data, 0));
        int yloc = glist_ytopixels(glist,
            basey + fielddesc_getcoord(&x->x_yloc, template, data, 0));
        char buf[DRAWNUMBER_BUFSIZE];
        int color = numbertocolor(
            fielddesc_getfloat(&x->x_color, template, data, 1));
        drawnumber_getbuf(x, data, template, buf);

        SETSYMBOL(fontatoms+0, gensym(sys_font));
        SETFLOAT (fontatoms+1,-sys_hostfontsize(glist_getfont(glist), glist_getzoom(glist)));
        SETSYMBOL(fontatoms+2, gensym(sys_fontweight));
        pdgui_vmess(0, "crr ii rs rk rs rA rS",
            glist_getcanvas(glist), "create", "text",
            xloc, yloc,
            "-anchor", "nw",
            "-fill", color,
            "-text", buf,
            "-font", 3, fontatoms,
            "-tags", 2, tags);
    }
    else
        pdgui_vmess(0, "crs", glist_getcanvas(glist), "delete", tag);
}

static void drawnumber_motionfn(void *z, t_floatarg dx, t_floatarg dy,
    t_floatarg up)
{
    t_drawnumber *x = (t_drawnumber *)z;
    t_atom at;
    if (up != 0)
        return;
    if (!gpointer_check(&TEMPLATE->drawnumber_motion_gpointer, 0))
    {
        post("drawnumber_motion: scalar disappeared");
        return;
    }
    if (TEMPLATE->drawnumber_motion_type != DT_FLOAT)
        return;
    TEMPLATE->drawnumber_motion_ycumulative -= dy;
    template_setfloat(TEMPLATE->drawnumber_motion_template,
        x->x_fieldname,
            TEMPLATE->drawnumber_motion_wp,
            TEMPLATE->drawnumber_motion_ycumulative,
                1);
    if (TEMPLATE->drawnumber_motion_scalar)
        template_notifyforscalar(TEMPLATE->drawnumber_motion_template,
            TEMPLATE->drawnumber_motion_glist,
                TEMPLATE->drawnumber_motion_scalar,
                gensym("change"), 1, &at);

    if (TEMPLATE->drawnumber_motion_scalar)
        scalar_redraw(TEMPLATE->drawnumber_motion_scalar,
            TEMPLATE->drawnumber_motion_glist);
    if (TEMPLATE->drawnumber_motion_array)
        array_redraw(TEMPLATE->drawnumber_motion_array,
            TEMPLATE->drawnumber_motion_glist);
}

static void drawnumber_key(void *z, t_symbol *keysym, t_floatarg fkey)
{
    t_drawnumber *x = (t_drawnumber *)z;
    int key = fkey;
    char sbuf[MAXPDSTRING];
    t_atom at;
    if (!gpointer_check(&TEMPLATE->drawnumber_motion_gpointer, 0))
    {
        post("drawnumber_motion: scalar disappeared");
        return;
    }
    if (key == 0)
        return;
    if (TEMPLATE->drawnumber_motion_type == DT_SYMBOL)
    {
            /* key entry for a symbol field */
        if (TEMPLATE->drawnumber_motion_firstkey)
            sbuf[0] = 0;
        else strncpy(sbuf,
            template_getsymbol(TEMPLATE->drawnumber_motion_template,
            x->x_fieldname, TEMPLATE->drawnumber_motion_wp, 1)->s_name,
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
    else if (TEMPLATE->drawnumber_motion_type == DT_FLOAT)
    {
            /* key entry for a numeric field.  This is just a stopgap. */
        double newf;
        if (TEMPLATE->drawnumber_motion_firstkey)
            sbuf[0] = 0;
        else sprintf(sbuf, "%g",
            template_getfloat(TEMPLATE->drawnumber_motion_template,
            x->x_fieldname, TEMPLATE->drawnumber_motion_wp, 1));
        TEMPLATE->drawnumber_motion_firstkey = (key == '\n');
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
        template_setfloat(TEMPLATE->drawnumber_motion_template,
            x->x_fieldname, TEMPLATE->drawnumber_motion_wp, (t_float)newf, 1);
        if (TEMPLATE->drawnumber_motion_scalar)
            template_notifyforscalar(TEMPLATE->drawnumber_motion_template,
                TEMPLATE->drawnumber_motion_glist,
                    TEMPLATE->drawnumber_motion_scalar,
                    gensym("change"), 1, &at);
        if (TEMPLATE->drawnumber_motion_scalar)
            scalar_redraw(TEMPLATE->drawnumber_motion_scalar,
                TEMPLATE->drawnumber_motion_glist);
        if (TEMPLATE->drawnumber_motion_array)
            array_redraw(TEMPLATE->drawnumber_motion_array,
                TEMPLATE->drawnumber_motion_glist);
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
            TEMPLATE->drawnumber_motion_glist = glist;
            TEMPLATE->drawnumber_motion_wp = data;
            TEMPLATE->drawnumber_motion_template = template;
            TEMPLATE->drawnumber_motion_scalar = sc;
            TEMPLATE->drawnumber_motion_array = ap;
            TEMPLATE->drawnumber_motion_firstkey = 1;
            TEMPLATE->drawnumber_motion_ycumulative =
                template_getfloat(template, x->x_fieldname, data, 0);
            TEMPLATE->drawnumber_motion_type = type;
            if (TEMPLATE->drawnumber_motion_scalar)
                gpointer_setglist(&TEMPLATE->drawnumber_motion_gpointer,
                    TEMPLATE->drawnumber_motion_glist,
                        TEMPLATE->drawnumber_motion_scalar);
            else gpointer_setarray(&TEMPLATE->drawnumber_motion_gpointer,
                    TEMPLATE->drawnumber_motion_array,
                        TEMPLATE->drawnumber_motion_wp);
            glist_grab(glist, z, drawnumber_motionfn, drawnumber_key,
                xpix, ypix);
        }
        return (1);
    }
    else return (0);
}

const t_parentwidgetbehavior drawnumber_widgetbehavior =
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

void g_template_newpdinstance(void)
{
    TEMPLATE = getbytes(sizeof(*TEMPLATE));
}

void g_template_freepdinstance(void)
{
    freebytes(TEMPLATE, sizeof(*TEMPLATE));
}
