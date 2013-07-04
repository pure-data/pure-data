/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* This file defines the "scalar" object, which is not a text object, just a
"gobj".  Scalars have templates which describe their structures, which
can contain numbers, sublists, and arrays.

*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>      /* for read/write to files */
#include "m_pd.h"
#include "g_canvas.h"

t_class *scalar_class;

void word_init(t_word *wp, t_template *template, t_gpointer *gp)
{
    int i, nitems = template->t_n;
    t_dataslot *datatypes = template->t_vec;
    for (i = 0; i < nitems; i++, datatypes++, wp++)
    {
        int type = datatypes->ds_type;
        if (type == DT_FLOAT)
            wp->w_float = 0; 
        else if (type == DT_SYMBOL)
            wp->w_symbol = &s_symbol;
        else if (type == DT_ARRAY)
            wp->w_array = array_new(datatypes->ds_arraytemplate, gp);
        else if (type == DT_TEXT)
            wp->w_binbuf = binbuf_new();
    }
}

void word_restore(t_word *wp, t_template *template,
    int argc, t_atom *argv)
{
    int i, nitems = template->t_n;
    t_dataslot *datatypes = template->t_vec;
    for (i = 0; i < nitems; i++, datatypes++, wp++)
    {
        int type = datatypes->ds_type;
        if (type == DT_FLOAT)
        {
            t_float f;
            if (argc)
            {
                f =  atom_getfloat(argv);
                argv++, argc--;
            }
            else f = 0;
            wp->w_float = f; 
        }
        else if (type == DT_SYMBOL)
        {
            t_symbol *s;
            if (argc)
            {
                s =  atom_getsymbol(argv);
                argv++, argc--;
            }
            else s = &s_;
            wp->w_symbol = s;
        }
    }
    if (argc)
        post("warning: word_restore: extra arguments");
}

void word_free(t_word *wp, t_template *template)
{
    int i;
    t_dataslot *dt;
    for (dt = template->t_vec, i = 0; i < template->t_n; i++, dt++)
    {
        if (dt->ds_type == DT_ARRAY)
            array_free(wp[i].w_array);
        else if (dt->ds_type == DT_TEXT)
            binbuf_free(wp[i].w_binbuf);
    }
}

static int template_cancreate(t_template *template)
{
    int i, type, nitems = template->t_n;
    t_dataslot *datatypes = template->t_vec;
    t_template *elemtemplate;
    for (i = 0; i < nitems; i++, datatypes++)
        if (datatypes->ds_type == DT_ARRAY &&
            (!(elemtemplate = template_findbyname(datatypes->ds_arraytemplate))
                || !template_cancreate(elemtemplate)))
    {
        error("%s: no such template", datatypes->ds_arraytemplate->s_name);
        return (0);
    }
    return (1);
}

    /* make a new scalar and add to the glist.  We create a "gp" here which
    will be used for array items to point back here.  This gp doesn't do
    reference counting or "validation" updates though; the parent won't go away
    without the contained arrays going away too.  The "gp" is copied out
    by value in the word_init() routine so we can throw our copy away. */

t_scalar *scalar_new(t_glist *owner, t_symbol *templatesym)
{
    t_scalar *x;
    t_template *template;
    t_gpointer gp;
    gpointer_init(&gp);
    template = template_findbyname(templatesym);
    if (!template)
    {
        error("scalar: couldn't find template %s", templatesym->s_name);
        return (0);
    }
    if (!template_cancreate(template))
        return (0);
    x = (t_scalar *)getbytes(sizeof(t_scalar) +
        (template->t_n - 1) * sizeof(*x->sc_vec));
    x->sc_gobj.g_pd = scalar_class;
    x->sc_template = templatesym;
    gpointer_setglist(&gp, owner, x);
    word_init(x->sc_vec, template, &gp);
    return (x);
}

    /* Pd method to create a new scalar, add it to a glist, and initialize
    it from the message arguments. */

void glist_scalar(t_glist *glist,
    t_symbol *classname, t_int argc, t_atom *argv)
{
    t_symbol *templatesym =
        canvas_makebindsym(atom_getsymbolarg(0, argc, argv));
    t_binbuf *b;
    int natoms, nextmsg = 0;
    t_atom *vec;
    if (!template_findbyname(templatesym))
    {
        pd_error(glist, "%s: no such template",
            atom_getsymbolarg(0, argc, argv)->s_name);
        return;
    }

    b = binbuf_new();
    binbuf_restore(b, argc, argv);
    natoms = binbuf_getnatom(b);
    vec = binbuf_getvec(b);
    canvas_readscalar(glist, natoms, vec, &nextmsg, 0);
    binbuf_free(b);
}

/* -------------------- widget behavior for scalar ------------ */
void scalar_getbasexy(t_scalar *x, t_float *basex, t_float *basey)
{
    t_template *template = template_findbyname(x->sc_template);
    *basex = template_getfloat(template, gensym("x"), x->sc_vec, 0);
    *basey = template_getfloat(template, gensym("y"), x->sc_vec, 0);
}

static void scalar_getrect(t_gobj *z, t_glist *owner,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_scalar *x = (t_scalar *)z;
    t_template *template = template_findbyname(x->sc_template);
    t_canvas *templatecanvas = template_findcanvas(template);
    int x1 = 0x7fffffff, x2 = -0x7fffffff, y1 = 0x7fffffff, y2 = -0x7fffffff;
    t_gobj *y;
    t_float basex, basey;
    scalar_getbasexy(x, &basex, &basey);
        /* if someone deleted the template canvas, we're just a point */
    if (!templatecanvas)
    {
        x1 = x2 = glist_xtopixels(owner, basex);
        y1 = y2 = glist_ytopixels(owner, basey);
    }
    else
    {
        x1 = y1 = 0x7fffffff;
        x2 = y2 = -0x7fffffff;
        for (y = templatecanvas->gl_list; y; y = y->g_next)
        {
            t_parentwidgetbehavior *wb = pd_getparentwidget(&y->g_pd);
            int nx1, ny1, nx2, ny2;
            if (!wb) continue;
            (*wb->w_parentgetrectfn)(y, owner,
                x->sc_vec, template, basex, basey,
                &nx1, &ny1, &nx2, &ny2);
            if (nx1 < x1) x1 = nx1;
            if (ny1 < y1) y1 = ny1;
            if (nx2 > x2) x2 = nx2;
            if (ny2 > y2) y2 = ny2;
        }
        if (x2 < x1 || y2 < y1)
            x1 = y1 = x2 = y2 = 0;
    }
    /* post("scalar x1 %d y1 %d x2 %d y2 %d", x1, y1, x2, y2); */
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2; 
}

static void scalar_drawselectrect(t_scalar *x, t_glist *glist, int state)
{
    if (state)
    {
        int x1, y1, x2, y2;
       
        scalar_getrect(&x->sc_gobj, glist, &x1, &y1, &x2, &y2);
        x1--; x2++; y1--; y2++;
        sys_vgui(".x%lx.c create line %d %d %d %d %d %d %d %d %d %d \
            -width 0 -fill blue -tags select%lx\n",
                glist_getcanvas(glist), x1, y1, x1, y2, x2, y2, x2, y1, x1, y1,
                x);
    }
    else
    {
        sys_vgui(".x%lx.c delete select%lx\n", glist_getcanvas(glist), x);
    }
}

static void scalar_select(t_gobj *z, t_glist *owner, int state)
{
    t_scalar *x = (t_scalar *)z;
    t_template *tmpl;
    t_symbol *templatesym = x->sc_template;
    t_atom at;
    t_gpointer gp;
    gpointer_init(&gp);
    gpointer_setglist(&gp, owner, x);
    SETPOINTER(&at, &gp);
    if (tmpl = template_findbyname(templatesym))
        template_notify(tmpl, (state ? gensym("select") : gensym("deselect")),
            1, &at);
    gpointer_unset(&gp);
    scalar_drawselectrect(x, owner, state);
}

static void scalar_displace(t_gobj *z, t_glist *glist, int dx, int dy)
{
    t_scalar *x = (t_scalar *)z;
    t_symbol *templatesym = x->sc_template;
    t_template *template = template_findbyname(templatesym);
    t_symbol *zz;
    t_atom at[3];
    t_gpointer gp;
    int xonset, yonset, xtype, ytype, gotx, goty;
    if (!template)
    {
        error("scalar: couldn't find template %s", templatesym->s_name);
        return;
    }
    gotx = template_find_field(template, gensym("x"), &xonset, &xtype, &zz);
    if (gotx && (xtype != DT_FLOAT))
        gotx = 0;
    goty = template_find_field(template, gensym("y"), &yonset, &ytype, &zz);
    if (goty && (ytype != DT_FLOAT))
        goty = 0;
    if (gotx)
        *(t_float *)(((char *)(x->sc_vec)) + xonset) +=
            dx * (glist_pixelstox(glist, 1) - glist_pixelstox(glist, 0));
    if (goty)
        *(t_float *)(((char *)(x->sc_vec)) + yonset) +=
            dy * (glist_pixelstoy(glist, 1) - glist_pixelstoy(glist, 0));
    gpointer_init(&gp);
    gpointer_setglist(&gp, glist, x);
    SETPOINTER(&at[0], &gp);
    SETFLOAT(&at[1], (t_float)dx);
    SETFLOAT(&at[2], (t_float)dy);
    template_notify(template, gensym("displace"), 2, at);
    scalar_redraw(x, glist);
}

static void scalar_activate(t_gobj *z, t_glist *owner, int state)
{
    /* post("scalar_activate %d", state); */
    /* later */
}

static void scalar_delete(t_gobj *z, t_glist *glist)
{
    /* nothing to do */
}

static void scalar_vis(t_gobj *z, t_glist *owner, int vis)
{
    t_scalar *x = (t_scalar *)z;
    t_template *template = template_findbyname(x->sc_template);
    t_canvas *templatecanvas = template_findcanvas(template);
    t_gobj *y;
    t_float basex, basey;
    scalar_getbasexy(x, &basex, &basey);
        /* if we don't know how to draw it, make a small rectangle */
    if (!templatecanvas)
    {
        if (vis)
        {
            int x1 = glist_xtopixels(owner, basex);
            int y1 = glist_ytopixels(owner, basey);
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags scalar%lx\n",
                glist_getcanvas(owner), x1-1, y1-1, x1+1, y1+1, x);
        }
        else sys_vgui(".x%lx.c delete scalar%lx\n", glist_getcanvas(owner), x);
        return;
    }

    for (y = templatecanvas->gl_list; y; y = y->g_next)
    {
        t_parentwidgetbehavior *wb = pd_getparentwidget(&y->g_pd);
        if (!wb) continue;
        (*wb->w_parentvisfn)(y, owner, x->sc_vec, template, basex, basey, vis);
    }
    if (glist_isselected(owner, &x->sc_gobj))
    {
        scalar_drawselectrect(x, owner, 0);
        scalar_drawselectrect(x, owner, 1);
    }
    sys_unqueuegui(x);
}

static void scalar_doredraw(t_gobj *client, t_glist *glist)
{
    scalar_vis(client, glist, 0);
    scalar_vis(client, glist, 1);
}

void scalar_redraw(t_scalar *x, t_glist *glist)
{
    if (glist_isvisible(glist))
        sys_queuegui(x, glist, scalar_doredraw);
}

extern void template_notifyforscalar(t_template *template, t_glist *owner,
    t_scalar *sc, t_symbol *s, int argc, t_atom *argv);

int scalar_doclick(t_word *data, t_template *template, t_scalar *sc,
    t_array *ap, struct _glist *owner,
    t_float xloc, t_float yloc, int xpix, int ypix,
    int shift, int alt, int dbl, int doit)
{
    int hit = 0;
    t_canvas *templatecanvas = template_findcanvas(template);
    t_gobj *y;
    t_atom at[2];
    t_float basex = template_getfloat(template, gensym("x"), data, 0);
    t_float basey = template_getfloat(template, gensym("y"), data, 0);
    SETFLOAT(at, basex + xloc);
    SETFLOAT(at+1, basey + yloc);
    if (doit)
        template_notifyforscalar(template, owner, 
            sc, gensym("click"), 2, at);
    for (y = templatecanvas->gl_list; y; y = y->g_next)
    {
        t_parentwidgetbehavior *wb = pd_getparentwidget(&y->g_pd);
        if (!wb) continue;
        if (hit = (*wb->w_parentclickfn)(y, owner,
            data, template, sc, ap, basex + xloc, basey + yloc,
            xpix, ypix, shift, alt, dbl, doit))
                return (hit);
    }
    return (0);
}

static int scalar_click(t_gobj *z, struct _glist *owner,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_scalar *x = (t_scalar *)z;
    t_template *template = template_findbyname(x->sc_template);
    return (scalar_doclick(x->sc_vec, template, x, 0,
        owner, 0, 0, xpix, ypix, shift, alt, dbl, doit));
}

static void scalar_save(t_gobj *z, t_binbuf *b)
{
    t_scalar *x = (t_scalar *)z;
    t_binbuf *b2 = binbuf_new();
    t_atom a, *argv;
    int i, argc;
    canvas_writescalar(x->sc_template, x->sc_vec, b2, 0);
    binbuf_addv(b, "ss", &s__X, gensym("scalar"));
    binbuf_addbinbuf(b, b2);
    binbuf_addsemi(b);
    binbuf_free(b2);
}

static void scalar_properties(t_gobj *z, struct _glist *owner)
{
    t_scalar *x = (t_scalar *)z;
    char *buf, buf2[80];
    int bufsize;
    t_binbuf *b;
    glist_noselect(owner);
    glist_select(owner, z);
    b = glist_writetobinbuf(owner, 0);
    binbuf_gettext(b, &buf, &bufsize);
    binbuf_free(b);
    buf = t_resizebytes(buf, bufsize, bufsize+1);
    buf[bufsize] = 0;
    sprintf(buf2, "pdtk_data_dialog %%s {");
    gfxstub_new((t_pd *)owner, x, buf2);
    sys_gui(buf);
    sys_gui("}\n");
    t_freebytes(buf, bufsize+1);
}

static t_widgetbehavior scalar_widgetbehavior =
{
    scalar_getrect,
    scalar_displace,
    scalar_select,
    scalar_activate,
    scalar_delete,
    scalar_vis,
    scalar_click,
};

static void scalar_free(t_scalar *x)
{
    int i;
    t_dataslot *datatypes, *dt;
    t_symbol *templatesym = x->sc_template;
    t_template *template = template_findbyname(templatesym);
    if (!template)
    {
        error("scalar: couldn't find template %s", templatesym->s_name);
        return;
    }
    word_free(x->sc_vec, template);
    gfxstub_deleteforkey(x);
        /* the "size" field in the class is zero, so Pd doesn't try to free
        us automatically (see pd_free()) */
    freebytes(x, sizeof(t_scalar) + (template->t_n - 1) * sizeof(*x->sc_vec));
}

/* ----------------- setup function ------------------- */

void g_scalar_setup(void)
{
    scalar_class = class_new(gensym("scalar"), 0, (t_method)scalar_free, 0,
        CLASS_GOBJ, 0);
    class_setwidget(scalar_class, &scalar_widgetbehavior);
    class_setsavefn(scalar_class, scalar_save);
    class_setpropertiesfn(scalar_class, scalar_properties);
}
