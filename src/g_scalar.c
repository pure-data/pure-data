/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* This file defines the "scalar" object, which is not a text object, just a
"gobj".  Scalars have templates which describe their structures, which
can contain numbers, sublists, and arrays.

*/

#include <stdio.h>      /* for read/write to files */
#include "m_pd.h"
#include "g_canvas.h"

/* ------------- gstubs and gpointers - safe pointing --------------- */

/* create a gstub which is "owned" by a glist (gl) or an array ("a"). */

t_gstub *gstub_new(t_glist *gl, t_array *a)
{
    t_gstub *gs = t_getbytes(sizeof(*gs));
    if (gl)
    {
        gs->gs_which = GP_GLIST;
        gs->gs_un.gs_glist = gl;
    }
    else
    {
        gs->gs_which = GP_ARRAY;
        gs->gs_un.gs_array = a;
    }
    gs->gs_refcount = 0;
    return (gs);
}

/* when a "gpointer" is set to point to this stub (so we can later chase
down the owner) we increase a reference count.  The following routine is called
whenever a gpointer is unset from pointing here.  If the owner is
gone and the refcount goes to zero, we can free the gstub safely. */

static void gstub_dis(t_gstub *gs)
{
    int refcount = --gs->gs_refcount;
    if ((!refcount) && gs->gs_which == GP_NONE)
        t_freebytes(gs, sizeof (*gs));
    else if (refcount < 0) bug("gstub_dis");
}

/* this routing is called by the owner to inform the gstub that it is
being deleted.  If no gpointers are pointing here, we can free the gstub;
otherwise we wait for the last gstub_dis() to free it. */

void gstub_cutoff(t_gstub *gs)
{
    gs->gs_which = GP_NONE;
    if (gs->gs_refcount < 0) bug("gstub_cutoff");
    if (!gs->gs_refcount) t_freebytes(gs, sizeof (*gs));
}

/* call this to verify that a pointer is fresh, i.e., that it either
points to real data or to the head of a list, and that in either case
the object hasn't disappeared since this pointer was generated.
Unless "headok" is set,  the routine also fails for the head of a list. */

int gpointer_check(const t_gpointer *gp, int headok)
{
    t_gstub *gs = gp->gp_stub;
    if (!gs) return (0);
    if (gs->gs_which == GP_ARRAY)
    {
        if (gs->gs_un.gs_array->a_valid != gp->gp_valid) return (0);
        else return (1);
    }
    else if (gs->gs_which == GP_GLIST)
    {
        if (!headok && !gp->gp_un.gp_scalar) return (0);
        else if (gs->gs_un.gs_glist->gl_valid != gp->gp_valid) return (0);
        else return (1);
    }
    else return (0);
}

    /* copy a pointer to another, assuming the second one hasn't yet been
    initialized.  New gpointers should be initialized either by this
    routine or by gpointer_init below. */
void gpointer_copy(const t_gpointer *gpfrom, t_gpointer *gpto)
{
    *gpto = *gpfrom;
    if (gpto->gp_stub)
        gpto->gp_stub->gs_refcount++;
    else bug("gpointer_copy");
}

    /* clear a gpointer that was previously set, releasing the associated
    gstub if this was the last reference to it. */
void gpointer_unset(t_gpointer *gp)
{
    t_gstub *gs;
    if ((gs = gp->gp_stub))
    {
        gstub_dis(gs);
        gp->gp_stub = 0;
    }
}

void gpointer_setglist(t_gpointer *gp, t_glist *glist, t_scalar *x)
{
    t_gstub *gs;
    if ((gs = gp->gp_stub)) gstub_dis(gs);
    gp->gp_stub = gs = glist->gl_stub;
    gp->gp_valid = glist->gl_valid;
    gp->gp_un.gp_scalar = x;
    gs->gs_refcount++;
}

void gpointer_setarray(t_gpointer *gp, t_array *array, t_word *w)
{
    t_gstub *gs;
    if ((gs = gp->gp_stub)) gstub_dis(gs);
    gp->gp_stub = gs = array->a_stub;
    gp->gp_valid = array->a_valid;
    gp->gp_un.gp_w = w;
    gs->gs_refcount++;
}

void gpointer_init(t_gpointer *gp)
{
    gp->gp_stub = 0;
    gp->gp_valid = 0;
    gp->gp_un.gp_scalar = 0;
}

/*********  random utility function to find a binbuf in a datum */

/* get the template for the object pointer to.  Assumes we've already checked
freshness. */

t_symbol *gpointer_gettemplatesym(const t_gpointer *gp)
{
    t_gstub *gs = gp->gp_stub;
    if (gs->gs_which == GP_GLIST)
    {
        t_scalar *sc = gp->gp_un.gp_scalar;
        if (sc)
            return (sc->sc_template);
        else return (0);
    }
    else
    {
        t_array *a = gs->gs_un.gs_array;
        return (a->a_templatesym);
    }
}

t_binbuf *pointertobinbuf(t_pd *x, t_gpointer *gp, t_symbol *s,
    const char *fname)
{
    t_symbol *templatesym = gpointer_gettemplatesym(gp), *arraytype;
    t_template *template;
    int onset, type;
    t_binbuf *b;
    t_gstub *gs = gp->gp_stub;
    t_word *vec;
    if (!templatesym)
    {
        pd_error(x, "%s: bad pointer", fname);
        return (0);
    }
    if (!(template = template_findbyname(templatesym)))
    {
        pd_error(x, "%s: couldn't find template %s", fname,
            templatesym->s_name);
        return (0);
    }
    if (!template_find_field(template, s, &onset, &type, &arraytype))
    {
        pd_error(x, "%s: %s.%s: no such field", fname,
            templatesym->s_name, s->s_name);
        return (0);
    }
    if (type != DT_TEXT)
    {
        pd_error(x, "%s: %s.%s: not a list", fname,
            templatesym->s_name, s->s_name);
        return (0);
    }
    if (gs->gs_which == GP_ARRAY)
        vec = gp->gp_un.gp_w;
    else vec = gp->gp_un.gp_scalar->sc_vec;
    return (vec[onset].w_binbuf);
}

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
        pd_error(0, "%s: no such template", datatypes->ds_arraytemplate->s_name);
        return (0);
    }
    return (1);
}

/* ------------------- scalars ---------------------- */

t_class *scalar_class;
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
        pd_error(0, "scalar: couldn't find template %s", templatesym->s_name);
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
    t_symbol *classname, int argc, t_atom *argv)
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
            const t_parentwidgetbehavior *wb = pd_getparentwidget(&y->g_pd);
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
    char tag[128];
    sprintf(tag, "select%lx", x);
    if (state)
    {
        int x1, y1, x2, y2;
        scalar_getrect(&x->sc_gobj, glist, &x1, &y1, &x2, &y2);
        x1--; x2++; y1--; y2++;
        pdgui_vmess(0, "crr iiiiiiiiii ri rr rs",
                  glist_getcanvas(glist), "create", "line",
                  x1,y1, x1,y2, x2,y2, x2,y1, x1,y1,
                  "-width", 0,
                  "-fill", "blue",
                  "-tags", tag);
    } else {
        pdgui_vmess(0, "crs", glist_getcanvas(glist), "delete", tag);
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
    if ((tmpl = template_findbyname(templatesym)))
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
        pd_error(0, "scalar: couldn't find template %s", templatesym->s_name);
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
            glist->gl_zoom * dx * (glist_pixelstox(glist, 1) - glist_pixelstox(glist, 0));
    if (goty)
        *(t_float *)(((char *)(x->sc_vec)) + yonset) +=
            glist->gl_zoom * dy * (glist_pixelstoy(glist, 1) - glist_pixelstoy(glist, 0));
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
        char tag[128];
        sprintf(tag, "scalar%lx", x);

        if (vis)
        {
            int x1 = glist_xtopixels(owner, basex);
            int y1 = glist_ytopixels(owner, basey);
            pdgui_vmess(0, "crr iiii rs",
                      glist_getcanvas(owner),
                      "create", "rectangle",
                      x1-1,y1-1, x1+1,y1+1,
                      "-tags", tag);
        }
        else
            pdgui_vmess(0, "crs", glist_getcanvas(owner), "delete", tag);
        return;
    }

    for (y = templatecanvas->gl_list; y; y = y->g_next)
    {
        const t_parentwidgetbehavior *wb = pd_getparentwidget(&y->g_pd);
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
    if (glist_isvisible(glist))
    {
        scalar_vis(client, glist, 0);
        scalar_vis(client, glist, 1);
    }
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
    t_atom at[3];
    t_float basex = template_getfloat(template, gensym("x"), data, 0);
    t_float basey = template_getfloat(template, gensym("y"), data, 0);
    SETFLOAT(at, 0); /* unused - this is later bashed to the gpointer */
    SETFLOAT(at+1, basex + xloc);
    SETFLOAT(at+2, basey + yloc);
    if (doit)
        template_notifyforscalar(template, owner,
            sc, gensym("click"), 3, at);
    for (y = templatecanvas->gl_list; y; y = y->g_next)
    {
        const t_parentwidgetbehavior *wb = pd_getparentwidget(&y->g_pd);
        if (!wb) continue;
        if ((hit = (*wb->w_parentclickfn)(y, owner,
            data, template, sc, ap, basex + xloc, basey + yloc,
            xpix, ypix, shift, alt, dbl, doit)))
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
    pdgui_stub_vnew((t_pd*)owner, "pdtk_data_dialog", x,
        "p", bufsize, buf);
    t_freebytes(buf, bufsize);
}

static const t_widgetbehavior scalar_widgetbehavior =
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
    sys_unqueuegui(x);
    if (!template)
    {
        pd_error(0, "scalar: couldn't find template %s", templatesym->s_name);
        return;
    }
    word_free(x->sc_vec, template);
    pdgui_stub_deleteforkey(x);
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
