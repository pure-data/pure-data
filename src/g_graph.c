/* Copyright (c) 1997-2001 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* This file deals with the behavior of glists as either "text objects" or
"graphs" inside another glist.  LATER move the inlet/outlet code of g_canvas.c
to this file... */

#include <stdlib.h>
#include "m_pd.h"

#include "g_canvas.h"
#include "s_stuff.h"    /* for sys_hostfontsize */
#include <stdio.h>
#include <string.h>

/* ---------------------- forward definitions ----------------- */

static void graph_vis(t_gobj *gr, t_glist *unused_glist, int vis);
static void graph_graphrect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2);
static void graph_getrect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2);

/* -------------------- maintaining the list -------------------- */

void canvas_drawredrect(t_canvas *x, int doit);

void glist_add(t_glist *x, t_gobj *y)
{
    t_object *ob;
    y->g_next = 0;
    if (!x->gl_list) x->gl_list = y;
    else
    {
        t_gobj *y2;
        for (y2 = x->gl_list; y2->g_next; y2 = y2->g_next);
        y2->g_next = y;
    }
    if (x->gl_editor && (ob = pd_checkobject(&y->g_pd)))
        rtext_new(x, ob);
    if (x->gl_editor && x->gl_isgraph && !x->gl_goprect
        && pd_checkobject(&y->g_pd))
    {
        x->gl_goprect = 1;
        canvas_drawredrect(x, 1);
    }
    if (glist_isvisible(x))
        gobj_vis(y, x, 1);
    if (class_isdrawcommand(y->g_pd))
        canvas_redrawallfortemplate(template_findbyname(canvas_makebindsym(
            glist_getcanvas(x)->gl_name)), 0);
}

    /* this is to protect against a hairy problem in which deleting
    a sub-canvas might delete an inlet on a box, after the box had
    been invisible-ized, so that we have to protect against redrawing it! */
int canvas_setdeleting(t_canvas *x, int flag)
{
    int ret = x->gl_isdeleting;
    x->gl_isdeleting = flag;
    return (ret);
}

    /* JMZ: emit a closebang message */
void rtext_freefortext(t_glist *gl, t_text *who);

    /* delete an object from a glist and free it */
void glist_delete(t_glist *x, t_gobj *y)
{
    t_gobj *g;
    t_object *ob;
    t_gotfn chkdsp = zgetfn(&y->g_pd, gensym("dsp"));
    t_canvas *canvas = glist_getcanvas(x);
    t_rtext *rtext = 0;
    int drawcommand = class_isdrawcommand(y->g_pd);
    int wasdeleting;

    if (pd_class(&y->g_pd) == canvas_class) {
            /* JMZ: send a closebang to the canvas */
        canvas_closebang((t_canvas *)y);
    }

    wasdeleting = canvas_setdeleting(canvas, 1);
    if (x->gl_editor)
    {
        if (x->gl_editor->e_grab == y) x->gl_editor->e_grab = 0;
        if (glist_isselected(x, y)) glist_deselect(x, y);

            /* HACK -- we had phantom outlets not getting erased on the
            screen because the canvas_setdeleting() mechanism is too
            crude.  LATER carefully set up rules for when the rtexts
            should exist, so that they stay around until all the
            steps of becoming invisible are done.  In the meantime, just
            zap the inlets and outlets here... */
        if (pd_class(&y->g_pd) == canvas_class)
        {
            t_glist *gl = (t_glist *)y;
            if (gl->gl_isgraph && glist_isvisible(x))
            {
                char tag[80];
                sprintf(tag, "graph%lx", (t_int)gl);
                glist_eraseiofor(x, &gl->gl_obj, tag);
            }
            else
            {
                if (glist_isvisible(x))
                    text_eraseborder(&gl->gl_obj, x,
                        rtext_gettag(glist_findrtext(x, &gl->gl_obj)));
            }
        }
    }
        /* if we're a drawing command, erase all scalars now, before deleting
        it; we'll redraw them once it's deleted below. */
    if (drawcommand)
        canvas_redrawallfortemplate(template_findbyname(canvas_makebindsym(
            glist_getcanvas(x)->gl_name)), 2);
    gobj_delete(y, x);
    if (glist_isvisible(canvas))
    {
        gobj_vis(y, x, 0);
    }
    if (x->gl_editor && (ob = pd_checkobject(&y->g_pd)) &&
        !(rtext = glist_findrtext(x, ob)))
            rtext = rtext_new(x, ob);
    if (x->gl_list == y) x->gl_list = y->g_next;
    else for (g = x->gl_list; g; g = g->g_next)
        if (g->g_next == y)
    {
        g->g_next = y->g_next;
        break;
    }
    pd_free(&y->g_pd);
    if (rtext)
        rtext_free(rtext);
    if (chkdsp) canvas_update_dsp();
    if (drawcommand)
        canvas_redrawallfortemplate(template_findbyname(canvas_makebindsym(
            glist_getcanvas(x)->gl_name)), 1);
    canvas_setdeleting(canvas, wasdeleting);
    x->gl_valid = ++glist_valid;
}

    /* remove every object from a glist.  Experimental. */
void glist_clear(t_glist *x)
{
    t_gobj *y, *y2;
    int dspstate = 0, suspended = 0;
    t_symbol *dspsym = gensym("dsp");
    while ((y = x->gl_list))
    {
            /* to avoid unnecessary DSP resorting, we suspend DSP
            only if we hit a patchable object. */
        if (!suspended && pd_checkobject(&y->g_pd) && zgetfn(&y->g_pd, dspsym))
        {
            dspstate = canvas_suspend_dsp();
            suspended = 1;
        }
            /* here's the real deletion. */
        glist_delete(x, y);
    }
    if (suspended)
        canvas_resume_dsp(dspstate);
}

void glist_retext(t_glist *glist, t_text *y)
{
    t_canvas *c = glist_getcanvas(glist);
        /* check that we have built rtexts yet.  LATER need a better test. */
    if (glist->gl_editor && glist->gl_editor->e_rtext)
    {
        t_rtext *rt = glist_findrtext(glist, y);
        if (rt)
            rtext_retext(rt);
    }
}

void glist_grab(t_glist *x, t_gobj *y, t_glistmotionfn motionfn,
    t_glistkeyfn keyfn, int xpos, int ypos)
{
    t_glist *x2 = glist_getcanvas(x);
    if (motionfn)
        x2->gl_editor->e_onmotion = MA_PASSOUT;
    else x2->gl_editor->e_onmotion = 0;
    x2->gl_editor->e_grab = y;
    x2->gl_editor->e_motionfn = motionfn;
    x2->gl_editor->e_keyfn = keyfn;
    x2->gl_editor->e_xwas = xpos;
    x2->gl_editor->e_ywas = ypos;
}

t_canvas *glist_getcanvas(t_glist *x)
{
    while (x->gl_owner && !x->gl_havewindow && x->gl_isgraph)
            x = x->gl_owner;
    return((t_canvas *)x);
}

static t_float gobj_getxforsort(t_gobj *g)
{
    if (pd_class(&g->g_pd) == scalar_class)
    {
        t_float x1, y1;
        scalar_getbasexy((t_scalar *)g, &x1, &y1);
        return(x1);
    }
    else return (0);
}

static t_gobj *glist_merge(t_glist *x, t_gobj *g1, t_gobj *g2)
{
    t_gobj *g = 0, *g9 = 0;
    t_float f1 = 0, f2 = 0;
    if (g1)
        f1 = gobj_getxforsort(g1);
    if (g2)
        f2 = gobj_getxforsort(g2);
    while (1)
    {
        if (g1)
        {
            if (g2)
            {
                if (f1 <= f2)
                    goto put1;
                else goto put2;
            }
            else goto put1;
        }
        else if (g2)
            goto put2;
        else break;
    put1:
        if (g9)
            g9->g_next = g1, g9 = g1;
        else g9 = g = g1;
        if ((g1 = g1->g_next))
            f1 = gobj_getxforsort(g1);
        g9->g_next = 0;
        continue;
    put2:
        if (g9)
            g9->g_next = g2, g9 = g2;
        else g9 = g = g2;
        if ((g2 = g2->g_next))
            f2 = gobj_getxforsort(g2);
        g9->g_next = 0;
        continue;
    }
    return (g);
}

static t_gobj *glist_dosort(t_glist *x,
    t_gobj *g, int nitems)
{
    if (nitems < 2)
        return (g);
    else
    {
        int n1 = nitems/2, n2 = nitems - n1, i;
        t_gobj *g2, *g3;
        for (g2 = g, i = n1-1; i--; g2 = g2->g_next)
            ;
        g3 = g2->g_next;
        g2->g_next = 0;
        g = glist_dosort(x, g, n1);
        g3 = glist_dosort(x, g3, n2);
        return (glist_merge(x, g, g3));
    }
}

void glist_sort(t_glist *x)
{
    int nitems = 0, foo = 0;
    t_float lastx = -1e37;
    t_gobj *g;
    for (g = x->gl_list; g; g = g->g_next)
    {
        t_float x1 = gobj_getxforsort(g);
        if (x1 < lastx)
            foo = 1;
        lastx = x1;
        nitems++;
    }
    if (foo)
        x->gl_list = glist_dosort(x, x->gl_list, nitems);
}

/* --------------- inlets and outlets  ----------- */


t_inlet *canvas_addinlet(t_canvas *x, t_pd *who, t_symbol *s)
{
    t_inlet *ip = inlet_new(&x->gl_obj, who, s, 0);
    if (!x->gl_loading && x->gl_owner && glist_isvisible(x->gl_owner))
    {
        gobj_vis(&x->gl_gobj, x->gl_owner, 0);
        gobj_vis(&x->gl_gobj, x->gl_owner, 1);
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
    }
    if (!x->gl_loading) canvas_resortinlets(x);
    return (ip);
}

void canvas_rminlet(t_canvas *x, t_inlet *ip)
{
    t_canvas *owner = x->gl_owner;
    int redraw = (owner && glist_isvisible(owner) && (!owner->gl_isdeleting)
        && glist_istoplevel(owner));

    if (owner) canvas_deletelinesforio(owner, &x->gl_obj, ip, 0);
    if (redraw)
        gobj_vis(&x->gl_gobj, x->gl_owner, 0);
    inlet_free(ip);
    if (redraw)
    {
        gobj_vis(&x->gl_gobj, x->gl_owner, 1);
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
    }
}

extern t_inlet *vinlet_getit(t_pd *x);
extern void obj_moveinletfirst(t_object *x, t_inlet *i);

void canvas_resortinlets(t_canvas *x)
{
    int ninlets = 0, i, j, xmax;
    t_gobj *y, **vec, **vp, **maxp;

    for (ninlets = 0, y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == vinlet_class) ninlets++;

    if (ninlets < 2) return;

    vec = (t_gobj **)getbytes(ninlets * sizeof(*vec));

    for (y = x->gl_list, vp = vec; y; y = y->g_next)
        if (pd_class(&y->g_pd) == vinlet_class) *vp++ = y;

    for (i = ninlets; i--;)
    {
        t_inlet *ip;
        for (vp = vec, xmax = -0x7fffffff, maxp = 0, j = ninlets;
            j--; vp++)
        {
            int x1, y1, x2, y2;
            t_gobj *g = *vp;
            if (!g) continue;
            gobj_getrect(g, x, &x1, &y1, &x2, &y2);
            if (x1 > xmax) xmax = x1, maxp = vp;
        }
        if (!maxp) break;
        y = *maxp;
        *maxp = 0;
        ip = vinlet_getit(&y->g_pd);

        obj_moveinletfirst(&x->gl_obj, ip);
    }
    freebytes(vec, ninlets * sizeof(*vec));
    if (x->gl_owner && glist_isvisible(x->gl_owner))
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
}

t_outlet *canvas_addoutlet(t_canvas *x, t_pd *who, t_symbol *s)
{
    t_outlet *op = outlet_new(&x->gl_obj, s);
    if (!x->gl_loading && x->gl_owner && glist_isvisible(x->gl_owner))
    {
        gobj_vis(&x->gl_gobj, x->gl_owner, 0);
        gobj_vis(&x->gl_gobj, x->gl_owner, 1);
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
    }
    if (!x->gl_loading) canvas_resortoutlets(x);
    return (op);
}

void canvas_rmoutlet(t_canvas *x, t_outlet *op)
{
    t_canvas *owner = x->gl_owner;
    int redraw = (owner && glist_isvisible(owner) && (!owner->gl_isdeleting)
        && glist_istoplevel(owner));

    if (owner) canvas_deletelinesforio(owner, &x->gl_obj, 0, op);
    if (redraw)
        gobj_vis(&x->gl_gobj, x->gl_owner, 0);

    outlet_free(op);
    if (redraw)
    {
        gobj_vis(&x->gl_gobj, x->gl_owner, 1);
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
    }
}

extern t_outlet *voutlet_getit(t_pd *x);
extern void obj_moveoutletfirst(t_object *x, t_outlet *i);

void canvas_resortoutlets(t_canvas *x)
{
    int noutlets = 0, i, j, xmax;
    t_gobj *y, **vec, **vp, **maxp;

    for (noutlets = 0, y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == voutlet_class) noutlets++;

    if (noutlets < 2) return;

    vec = (t_gobj **)getbytes(noutlets * sizeof(*vec));

    for (y = x->gl_list, vp = vec; y; y = y->g_next)
        if (pd_class(&y->g_pd) == voutlet_class) *vp++ = y;

    for (i = noutlets; i--;)
    {
        t_outlet *ip;
        for (vp = vec, xmax = -0x7fffffff, maxp = 0, j = noutlets;
            j--; vp++)
        {
            int x1, y1, x2, y2;
            t_gobj *g = *vp;
            if (!g) continue;
            gobj_getrect(g, x, &x1, &y1, &x2, &y2);
            if (x1 > xmax) xmax = x1, maxp = vp;
        }
        if (!maxp) break;
        y = *maxp;
        *maxp = 0;
        ip = voutlet_getit(&y->g_pd);

        obj_moveoutletfirst(&x->gl_obj, ip);
    }
    freebytes(vec, noutlets * sizeof(*vec));
    if (x->gl_owner && glist_isvisible(x->gl_owner))
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
}

/* ----------calculating coordinates and controlling appearance --------- */


static void graph_bounds(t_glist *x, t_floatarg x1, t_floatarg y1,
    t_floatarg x2, t_floatarg y2)
{
    x->gl_x1 = x1;
    x->gl_x2 = x2;
    x->gl_y1 = y1;
    x->gl_y2 = y2;
    if (x->gl_x2 == x->gl_x1 ||
        x->gl_y2 == x->gl_y1)
    {
        error("graph: empty bounds rectangle");
        x1 = y1 = 0;
        x2 = y2 = 1;
    }
    glist_redraw(x);
}

static void graph_xticks(t_glist *x,
    t_floatarg point, t_floatarg inc, t_floatarg f)
{
    x->gl_xtick.k_point = point;
    x->gl_xtick.k_inc = inc;
    x->gl_xtick.k_lperb = f;
    glist_redraw(x);
}

static void graph_yticks(t_glist *x,
    t_floatarg point, t_floatarg inc, t_floatarg f)
{
    x->gl_ytick.k_point = point;
    x->gl_ytick.k_inc = inc;
    x->gl_ytick.k_lperb = f;
    glist_redraw(x);
}

static void graph_xlabel(t_glist *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    if (argc < 1) error("graph_xlabel: no y value given");
    else
    {
        x->gl_xlabely = atom_getfloat(argv);
        argv++; argc--;
        x->gl_xlabel = (t_symbol **)t_resizebytes(x->gl_xlabel,
            x->gl_nxlabels * sizeof (t_symbol *), argc * sizeof (t_symbol *));
        x->gl_nxlabels = argc;
        for (i = 0; i < argc; i++) x->gl_xlabel[i] = atom_gensym(&argv[i]);
    }
    glist_redraw(x);
}

static void graph_ylabel(t_glist *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    if (argc < 1) error("graph_ylabel: no x value given");
    else
    {
        x->gl_ylabelx = atom_getfloat(argv);
        argv++; argc--;
        x->gl_ylabel = (t_symbol **)t_resizebytes(x->gl_ylabel,
            x->gl_nylabels * sizeof (t_symbol *), argc * sizeof (t_symbol *));
        x->gl_nylabels = argc;
        for (i = 0; i < argc; i++) x->gl_ylabel[i] = atom_gensym(&argv[i]);
    }
    glist_redraw(x);
}

/****** routines to convert pixels to X or Y value and vice versa ******/

    /* convert an x pixel value to an x coordinate value */
t_float glist_pixelstox(t_glist *x, t_float xpix)
{
        /* if we appear as a text box on parent, our range in our
        coordinates (x1, etc.) specifies the coordinate range
        of a one-pixel square at top left of the window. */
    if (!x->gl_isgraph)
        return (x->gl_x1 + (x->gl_x2 - x->gl_x1) * xpix);

        /* if we're a graph when shown on parent, but own our own
        window right now, our range in our coordinates (x1, etc.) is spread
        over the visible window size, given by screenx1, etc. */
    else if (x->gl_isgraph && x->gl_havewindow)
        return (x->gl_x1 + (x->gl_x2 - x->gl_x1) *
            (xpix) / (x->gl_screenx2 - x->gl_screenx1));

        /* otherwise, we appear in a graph within a parent glist,
         so get our screen rectangle on parent and transform. */
    else
    {
        int x1, y1, x2, y2;
        if (!x->gl_owner)
            bug("glist_pixelstox");
        graph_graphrect(&x->gl_gobj, x->gl_owner, &x1, &y1, &x2, &y2);
        return (x->gl_x1 + (x->gl_x2 - x->gl_x1) *
            (xpix - x1) / (x2 - x1));
    }
}

t_float glist_pixelstoy(t_glist *x, t_float ypix)
{
    if (!x->gl_isgraph)
        return (x->gl_y1 + (x->gl_y2 - x->gl_y1) * ypix);
    else if (x->gl_isgraph && x->gl_havewindow)
        return (x->gl_y1 + (x->gl_y2 - x->gl_y1) *
                (ypix) / (x->gl_screeny2 - x->gl_screeny1));
    else
    {
        int x1, y1, x2, y2;
        if (!x->gl_owner)
            bug("glist_pixelstox");
        graph_graphrect(&x->gl_gobj, x->gl_owner, &x1, &y1, &x2, &y2);
        return (x->gl_y1 + (x->gl_y2 - x->gl_y1) *
            (ypix - y1) / (y2 - y1));
    }
}

    /* convert an x coordinate value to an x pixel location in window */
t_float glist_xtopixels(t_glist *x, t_float xval)
{
    if (!x->gl_isgraph)
        return ((xval - x->gl_x1) / (x->gl_x2 - x->gl_x1));
    else if (x->gl_isgraph && x->gl_havewindow)
        return (x->gl_screenx2 - x->gl_screenx1) *
            (xval - x->gl_x1) / (x->gl_x2 - x->gl_x1);
    else
    {
        int x1, y1, x2, y2;
        if (!x->gl_owner)
            bug("glist_pixelstox");
        graph_graphrect(&x->gl_gobj, x->gl_owner, &x1, &y1, &x2, &y2);
        return (x1 + (x2 - x1) * (xval - x->gl_x1) / (x->gl_x2 - x->gl_x1));
    }
}

t_float glist_ytopixels(t_glist *x, t_float yval)
{
    if (!x->gl_isgraph)
        return ((yval - x->gl_y1) / (x->gl_y2 - x->gl_y1));
    else if (x->gl_isgraph && x->gl_havewindow)
        return (x->gl_screeny2 - x->gl_screeny1) *
                (yval - x->gl_y1) / (x->gl_y2 - x->gl_y1);
    else
    {
        int x1, y1, x2, y2;
        if (!x->gl_owner)
            bug("glist_pixelstox");
        graph_graphrect(&x->gl_gobj, x->gl_owner, &x1, &y1, &x2, &y2);
        return (y1 + (y2 - y1) * (yval - x->gl_y1) / (x->gl_y2 - x->gl_y1));
    }
}

    /* convert an X screen distance to an X coordinate increment.
      This is terribly inefficient;
      but probably not a big enough CPU hog to warrant optimizing. */
t_float glist_dpixtodx(t_glist *x, t_float dxpix)
{
    return (dxpix * (glist_pixelstox(x, 1) - glist_pixelstox(x, 0)));
}

t_float glist_dpixtody(t_glist *x, t_float dypix)
{
    return (dypix * (glist_pixelstoy(x, 1) - glist_pixelstoy(x, 0)));
}

    /* get the window location in pixels of a "text" object.  The
    object's x and y positions are in pixels when the glist they're
    in is toplevel.  Otherwise, if it's a new-style graph-on-parent
    (so gl_goprect is set) we use the offset into the framing subrectangle
    as an offset into the parent rectangle.  Finally, it might be an old,
    proportional-style GOP.  In this case we do a coordinate transformation. */
int text_xpix(t_text *x, t_glist *glist)
{
    if (glist->gl_havewindow || !glist->gl_isgraph)
        return (x->te_xpix);
    else if (glist->gl_goprect)
        return (glist_xtopixels(glist, glist->gl_x1) +
            x->te_xpix - glist->gl_xmargin);
    else return (glist_xtopixels(glist,
            glist->gl_x1 + (glist->gl_x2 - glist->gl_x1) *
                x->te_xpix / (glist->gl_screenx2 - glist->gl_screenx1)));
}

int text_ypix(t_text *x, t_glist *glist)
{
    if (glist->gl_havewindow || !glist->gl_isgraph)
        return (x->te_ypix);
    else if (glist->gl_goprect)
        return (glist_ytopixels(glist, glist->gl_y1) +
            x->te_ypix - glist->gl_ymargin);
    else return (glist_ytopixels(glist,
            glist->gl_y1 + (glist->gl_y2 - glist->gl_y1) *
                x->te_ypix / (glist->gl_screeny2 - glist->gl_screeny1)));
}

    /* redraw all the items in a glist.  We construe this to mean
    redrawing in its own window and on parent, as needed in each case.
    This is too conservative -- for instance, when you draw an "open"
    rectangle on the parent, you shouldn't have to redraw the window!  */
void glist_redraw(t_glist *x)
{
    if (glist_isvisible(x))
    {
            /* LATER fix the graph_vis() code to handle both cases */
        if (glist_istoplevel(x))
        {
            t_gobj *g;
            t_linetraverser t;
            t_outconnect *oc;
            for (g = x->gl_list; g; g = g->g_next)
            {
                gobj_vis(g, x, 0);
                gobj_vis(g, x, 1);
            }
                /* redraw all the lines */
            linetraverser_start(&t, x);
            while ((oc = linetraverser_next(&t)))
                sys_vgui(".x%lx.c coords l%lx %d %d %d %d\n",
                    glist_getcanvas(x), oc,
                        t.tr_lx1, t.tr_ly1, t.tr_lx2, t.tr_ly2);
            canvas_drawredrect(x, 0);
            if (x->gl_goprect)
            {
                canvas_drawredrect(x, 1);
            }
        }
        if (x->gl_owner && glist_isvisible(x->gl_owner))
        {
            graph_vis(&x->gl_gobj, x->gl_owner, 0);
            graph_vis(&x->gl_gobj, x->gl_owner, 1);
        }
    }
}

/* --------------------------- widget behavior  ------------------- */

int garray_getname(t_garray *x, t_symbol **namep);


    /* Note that some code in here would also be useful for drawing
    graph decorations in toplevels... */
static void graph_vis(t_gobj *gr, t_glist *parent_glist, int vis)
{
    t_glist *x = (t_glist *)gr;
    char tag[50];
    t_gobj *g;
    int x1, y1, x2, y2;
        /* ordinary subpatches: just act like a text object */
    if (!x->gl_isgraph)
    {
        text_widgetbehavior.w_visfn(gr, parent_glist, vis);
        return;
    }

    if (vis && canvas_showtext(x))
        rtext_draw(glist_findrtext(parent_glist, &x->gl_obj));
    graph_getrect(gr, parent_glist, &x1, &y1, &x2, &y2);
    if (!vis)
        rtext_erase(glist_findrtext(parent_glist, &x->gl_obj));

    sprintf(tag, "graph%lx", (t_int)x);
    if (vis)
        glist_drawiofor(parent_glist, &x->gl_obj, 1,
            tag, x1, y1, x2, y2);
    else glist_eraseiofor(parent_glist, &x->gl_obj, tag);
        /* if we look like a graph but have been moved to a toplevel,
        just show the bounding rectangle */
    if (x->gl_havewindow)
    {
        if (vis)
        {
            sys_vgui(".x%lx.c create polygon\
 %d %d %d %d %d %d %d %d %d %d -tags [list %s graph] -fill #c0c0c0\n",
                glist_getcanvas(x->gl_owner),
                x1, y1, x1, y2, x2, y2, x2, y1, x1, y1, tag);
        }
        else
        {
            sys_vgui(".x%lx.c delete %s\n",
                glist_getcanvas(x->gl_owner), tag);
        }
        return;
    }
        /* otherwise draw (or erase) us as a graph inside another glist. */
    if (vis)
    {
        int i;
        t_float f;
        t_gobj *g;
        t_symbol *arrayname;
        t_garray *ga;
        char *ylabelanchor =
            (x->gl_ylabelx > 0.5*(x->gl_x1 + x->gl_x2) ? "w" : "e");
        char *xlabelanchor =
            (x->gl_xlabely > 0.5*(x->gl_y1 + x->gl_y2) ? "s" : "n");

            /* draw a rectangle around the graph */
        sys_vgui(".x%lx.c create line\
            %d %d %d %d %d %d %d %d %d %d -tags [list %s graph]\n",
            glist_getcanvas(x->gl_owner),
            x1, y1, x1, y2, x2, y2, x2, y1, x1, y1, tag);

            /* if there's just one "garray" in the graph, write its name
                along the top */
        for (i = (y1 < y2 ? y1 : y2)-1, g = x->gl_list; g; g = g->g_next)
            if (g->g_pd == garray_class &&
                !garray_getname((t_garray *)g, &arrayname))
        {
            i -= glist_fontheight(x);
            sys_vgui(".x%lx.c create text %d %d -text {%s} -anchor nw\
             -font {{%s} -%d %s} -tags [list %s label graph]\n",
             (long)glist_getcanvas(x),  x1, i, arrayname->s_name, sys_font,
                sys_hostfontsize(glist_getfont(x), x->gl_zoom),
                    sys_fontweight, tag);
        }

            /* draw ticks on horizontal borders.  If lperb field is
            zero, this is disabled. */
        if (x->gl_xtick.k_lperb)
        {
            t_float upix, lpix;
            if (y2 < y1)
                upix = y1, lpix = y2;
            else upix = y2, lpix = y1;
            for (i = 0, f = x->gl_xtick.k_point;
                f < 0.99 * x->gl_x2 + 0.01*x->gl_x1; i++,
                    f += x->gl_xtick.k_inc)
            {
                int tickpix = (i % x->gl_xtick.k_lperb ? 2 : 4);
                sys_vgui(".x%lx.c create line %d %d %d %d -tags [list %s graph]\n",
                    glist_getcanvas(x->gl_owner),
                    (int)glist_xtopixels(x, f), (int)upix,
                    (int)glist_xtopixels(x, f), (int)upix - tickpix, tag);
                sys_vgui(".x%lx.c create line %d %d %d %d -tags [list %s graph]\n",
                    glist_getcanvas(x->gl_owner),
                    (int)glist_xtopixels(x, f), (int)lpix,
                    (int)glist_xtopixels(x, f), (int)lpix + tickpix, tag);
            }
            for (i = 1, f = x->gl_xtick.k_point - x->gl_xtick.k_inc;
                f > 0.99 * x->gl_x1 + 0.01*x->gl_x2;
                    i++, f -= x->gl_xtick.k_inc)
            {
                int tickpix = (i % x->gl_xtick.k_lperb ? 2 : 4);
                sys_vgui(".x%lx.c create line %d %d %d %d -tags [list %s graph]\n",
                    glist_getcanvas(x->gl_owner),
                    (int)glist_xtopixels(x, f), (int)upix,
                    (int)glist_xtopixels(x, f), (int)upix - tickpix, tag);
                sys_vgui(".x%lx.c create line %d %d %d %d -tags [list %s graph]\n",
                    glist_getcanvas(x->gl_owner),
                    (int)glist_xtopixels(x, f), (int)lpix,
                    (int)glist_xtopixels(x, f), (int)lpix + tickpix, tag);
            }
        }

            /* draw ticks in vertical borders*/
        if (x->gl_ytick.k_lperb)
        {
            t_float ubound, lbound;
            if (x->gl_y2 < x->gl_y1)
                ubound = x->gl_y1, lbound = x->gl_y2;
            else ubound = x->gl_y2, lbound = x->gl_y1;
            for (i = 0, f = x->gl_ytick.k_point;
                f < 0.99 * ubound + 0.01 * lbound;
                    i++, f += x->gl_ytick.k_inc)
            {
                int tickpix = (i % x->gl_ytick.k_lperb ? 2 : 4);
                sys_vgui(".x%lx.c create line %d %d %d %d -tags [list %s graph]\n",
                    glist_getcanvas(x->gl_owner),
                    x1, (int)glist_ytopixels(x, f),
                    x1 + tickpix, (int)glist_ytopixels(x, f), tag);
                sys_vgui(".x%lx.c create line %d %d %d %d -tags [list %s graph]\n",
                    glist_getcanvas(x->gl_owner),
                    x2, (int)glist_ytopixels(x, f),
                    x2 - tickpix, (int)glist_ytopixels(x, f), tag);
            }
            for (i = 1, f = x->gl_ytick.k_point - x->gl_ytick.k_inc;
                f > 0.99 * lbound + 0.01 * ubound;
                    i++, f -= x->gl_ytick.k_inc)
            {
                int tickpix = (i % x->gl_ytick.k_lperb ? 2 : 4);
                sys_vgui(".x%lx.c create line %d %d %d %d -tags [list %s graph]\n",
                    glist_getcanvas(x->gl_owner),
                    x1, (int)glist_ytopixels(x, f),
                    x1 + tickpix, (int)glist_ytopixels(x, f), tag);
                sys_vgui(".x%lx.c create line %d %d %d %d -tags [list %s graph]\n",
                    glist_getcanvas(x->gl_owner),
                    x2, (int)glist_ytopixels(x, f),
                    x2 - tickpix, (int)glist_ytopixels(x, f), tag);
            }
        }
            /* draw x labels */
        for (i = 0; i < x->gl_nxlabels; i++)
            sys_vgui(".x%lx.c create text\
 %d %d -text {%s} -font {{%s} -%d %s} -anchor %s -tags [list %s label graph]\n",
                glist_getcanvas(x),
                (int)glist_xtopixels(x, atof(x->gl_xlabel[i]->s_name)),
                (int)glist_ytopixels(x, x->gl_xlabely),
                x->gl_xlabel[i]->s_name, sys_font,
                     glist_getfont(x), sys_fontweight, xlabelanchor, tag);

            /* draw y labels */
        for (i = 0; i < x->gl_nylabels; i++)
            sys_vgui(".x%lx.c create text\
 %d %d -text {%s} -font {{%s} -%d %s} -anchor %s -tags [list %s label graph]\n",
                glist_getcanvas(x),
                (int)glist_xtopixels(x, x->gl_ylabelx),
                (int)glist_ytopixels(x, atof(x->gl_ylabel[i]->s_name)),
                x->gl_ylabel[i]->s_name, sys_font,
                glist_getfont(x), sys_fontweight, ylabelanchor, tag);

            /* draw contents of graph as glist */
        for (g = x->gl_list; g; g = g->g_next)
            gobj_vis(g, x, 1);
    }
    else
    {
        sys_vgui(".x%lx.c delete %s\n",
            glist_getcanvas(x->gl_owner), tag);
        for (g = x->gl_list; g; g = g->g_next)
            gobj_vis(g, x, 0);
    }
}

    /* get the graph's rectangle, not counting extra swelling for controls
    to keep them inside the graph.  This is the "logical" pixel size. */

static void graph_graphrect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_glist *x = (t_glist *)z;
    int x1 = text_xpix(&x->gl_obj, glist);
    int y1 = text_ypix(&x->gl_obj, glist);
    int x2, y2;
    x2 = x1 + x->gl_pixwidth;
    y2 = y1 + x->gl_pixheight;

    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

    /* get the rectangle, enlarged to contain all the "contents" --
    meaning their formal bounds rectangles. */
static void graph_getrect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    int x1 = 0x7fffffff, y1 = 0x7fffffff, x2 = -0x7fffffff, y2 = -0x7fffffff;
    t_glist *x = (t_glist *)z;
    if (x->gl_isgraph)
    {
        int hadwindow;
        t_gobj *g;
        t_text *ob;
        int x21, y21, x22, y22;

        graph_graphrect(z, glist, &x1, &y1, &x2, &y2);
        if (canvas_showtext(x))
        {
            text_widgetbehavior.w_getrectfn(z, glist, &x21, &y21, &x22, &y22);
            if (x22 > x2)
                x2 = x22;
            if (y22 > y2)
                y2 = y22;
        }
        if (!x->gl_goprect)
        {
            /* expand the rectangle to fit in text objects; this applies only
            to the old (0.37) graph-on-parent behavior. */
            /* lie about whether we have our own window to affect gobj_getrect
            calls below.  */
            hadwindow = x->gl_havewindow;
            x->gl_havewindow = 0;
            for (g = x->gl_list; g; g = g->g_next)
            {
                    /* don't do this for arrays, just let them hang outside the
                    box.  And ignore "text" objects which aren't shown on
                    parent */
                if (pd_class(&g->g_pd) == garray_class ||
                    pd_checkobject(&g->g_pd))
                        continue;
                gobj_getrect(g, x, &x21, &y21, &x22, &y22);
                if (x22 > x2)
                    x2 = x22;
                if (y22 > y2)
                    y2 = y22;
            }
            x->gl_havewindow = hadwindow;
        }
    }
    else text_widgetbehavior.w_getrectfn(z, glist, &x1, &y1, &x2, &y2);
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

static void graph_displace(t_gobj *z, t_glist *glist, int dx, int dy)
{
    t_glist *x = (t_glist *)z;
    if (!x->gl_isgraph)
        text_widgetbehavior.w_displacefn(z, glist, dx, dy);
    else
    {
        x->gl_obj.te_xpix += dx;
        x->gl_obj.te_ypix += dy;
        glist_redraw(x);
        canvas_fixlinesfor(glist, &x->gl_obj);
    }
}

static void graph_select(t_gobj *z, t_glist *glist, int state)
{
    t_glist *x = (t_glist *)z;
    if (!x->gl_isgraph)
        text_widgetbehavior.w_selectfn(z, glist, state);
    else
    {
        t_rtext *y = glist_findrtext(glist, &x->gl_obj);
        if (canvas_showtext(x))
            rtext_select(y, state);
        sys_vgui(".x%lx.c itemconfigure %sR -fill %s\n", glist,
        rtext_gettag(y), (state? "blue" : "black"));
        sys_vgui(".x%lx.c itemconfigure graph%lx -fill %s\n",
            glist_getcanvas(glist), z, (state? "blue" : "black"));
    }
}

static void graph_activate(t_gobj *z, t_glist *glist, int state)
{
    t_glist *x = (t_glist *)z;
    if (canvas_showtext(x))
        text_widgetbehavior.w_activatefn(z, glist, state);
}

static void graph_delete(t_gobj *z, t_glist *glist)
{
    t_glist *x = (t_glist *)z;
    t_gobj *y;
    while ((y = x->gl_list))
        glist_delete(x, y);
    if (glist_isvisible(x))
        text_widgetbehavior.w_deletefn(z, glist);
            /* if we have connections to the actual 'canvas' object, zap
            them as well (e.g., array or scalar objects that are implemented
            as canvases with "real" inlets).  Connections to ordinary canvas
            in/outlets already got zapped when we cleared the contents above */
    canvas_deletelinesfor(glist, &x->gl_obj);
}

static t_float graph_lastxpix, graph_lastypix;

static void graph_motion(void *z, t_floatarg dx, t_floatarg dy)
{
    t_glist *x = (t_glist *)z;
    t_float newxpix = graph_lastxpix + dx, newypix = graph_lastypix + dy;
    t_garray *a = (t_garray *)(x->gl_list);
    int oldx = 0.5 + glist_pixelstox(x, graph_lastxpix);
    int newx = 0.5 + glist_pixelstox(x, newxpix);
    t_word *vec;
    int nelem, i;
    t_float oldy = glist_pixelstoy(x, graph_lastypix);
    t_float newy = glist_pixelstoy(x, newypix);
    graph_lastxpix = newxpix;
    graph_lastypix = newypix;
        /* verify that the array is OK */
    if (!a || pd_class((t_pd *)a) != garray_class)
        return;
    if (!garray_getfloatwords(a, &nelem, &vec))
        return;
    if (oldx < 0) oldx = 0;
    if (oldx >= nelem)
        oldx = nelem - 1;
    if (newx < 0) newx = 0;
    if (newx >= nelem)
        newx = nelem - 1;
    if (oldx < newx - 1)
    {
        for (i = oldx + 1; i <= newx; i++)
            vec[i].w_float = newy + (oldy - newy) *
                ((t_float)(newx - i))/(t_float)(newx - oldx);
    }
    else if (oldx > newx + 1)
    {
        for (i = oldx - 1; i >= newx; i--)
            vec[i].w_float = newy + (oldy - newy) *
                ((t_float)(newx - i))/(t_float)(newx - oldx);
    }
    else vec[newx].w_float = newy;
    garray_redraw(a);
}

static int graph_click(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_glist *x = (t_glist *)z;
    t_gobj *y;
    int clickreturned = 0;
    if (!x->gl_isgraph)
        return (text_widgetbehavior.w_clickfn(z, glist,
            xpix, ypix, shift, alt, dbl, doit));
    else if (x->gl_havewindow)
        return (0);
    else
    {
        for (y = x->gl_list; y; y = y->g_next)
        {
            int x1, y1, x2, y2;
                /* check if the object wants to be clicked */
            if (canvas_hitbox(x, y, xpix, ypix, &x1, &y1, &x2, &y2)
                &&  (clickreturned = gobj_click(y, x, xpix, ypix,
                    shift, alt, 0, doit)))
                        break;
        }
        if (!doit)
        {
            if (y)
                canvas_setcursor(glist_getcanvas(x), clickreturned);
            else canvas_setcursor(glist_getcanvas(x), CURSOR_RUNMODE_NOTHING);
        }
        return (clickreturned);
    }
}

t_widgetbehavior graph_widgetbehavior =
{
    graph_getrect,
    graph_displace,
    graph_select,
    graph_activate,
    graph_delete,
    graph_vis,
    graph_click,
};

    /* find the graph most recently added to this glist;
        if none exists, return 0. */

t_glist *glist_findgraph(t_glist *x)
{
    t_gobj *y = 0, *z;
    for (z = x->gl_list; z; z = z->g_next)
        if (pd_class(&z->g_pd) == canvas_class && ((t_glist *)z)->gl_isgraph)
            y = z;
    return ((t_glist *)y);
}

extern void canvas_menuarray(t_glist *canvas);

void g_graph_setup_class(t_class *c)
{
    class_setwidget(c, &graph_widgetbehavior);
    class_addmethod(c, (t_method)graph_bounds, gensym("bounds"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(c, (t_method)graph_xticks, gensym("xticks"),
        A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(c, (t_method)graph_xlabel, gensym("xlabel"),
        A_GIMME, 0);
    class_addmethod(c, (t_method)graph_yticks, gensym("yticks"),
        A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(c, (t_method)graph_ylabel, gensym("ylabel"),
        A_GIMME, 0);
    class_addmethod(c, (t_method)graph_array, gensym("array"),
        A_SYMBOL, A_FLOAT, A_SYMBOL, A_DEFFLOAT, A_NULL);
    class_addmethod(c, (t_method)canvas_menuarray,
        gensym("menuarray"), A_NULL);
    class_addmethod(c, (t_method)glist_sort,
        gensym("sort"), A_NULL);
}

void g_graph_setup( void)
{
    g_graph_setup_class(canvas_class);
}

