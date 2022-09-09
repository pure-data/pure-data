/* Copyright (c) 1997-2001 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* this file defines the "glist" class, also known as "canvas" (the two used
to be different but are now unified except for some fossilized names.) */

#include <stdlib.h>
#include <stdio.h>
#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include "g_canvas.h"
#include "s_utf8.h"
#include <string.h>
#include "g_undo.h"

#ifdef _WIN32
#include <io.h>
#endif
#ifdef _MSC_VER
#define snprintf _snprintf
#endif

    /* LATER consider adding font size to this struct (see glist_getfont()) */
struct _canvasenvironment
{
    t_symbol *ce_dir;      /* directory patch lives in */
    int ce_argc;           /* number of "$" arguments */
    t_atom *ce_argv;       /* array of "$" arguments */
    int ce_dollarzero;     /* value of "$0" */
    t_namelist *ce_path;   /* search path */
};
typedef struct _canvas_private
{
    t_undo undo;
} t_canvas_private;

#define GLIST_DEFCANVASWIDTH 450
#define GLIST_DEFCANVASHEIGHT 300

/* ---------------------- variables --------------------------- */

t_class *canvas_class;
t_canvas *canvas_whichfind;         /* last canvas we did a find in */

/* ------------------ forward function declarations --------------- */
static void canvas_start_dsp(void);
static void canvas_stop_dsp(void);
static void canvas_drawlines(t_canvas *x);
static void canvas_dosetbounds(t_canvas *x, int x1, int y1, int x2, int y2);
void canvas_reflecttitle(t_canvas *x);
static void canvas_addtolist(t_canvas *x);
static void canvas_takeofflist(t_canvas *x);
static void canvas_pop(t_canvas *x, t_floatarg fvis);
static void canvas_bind(t_canvas *x);
static void canvas_unbind(t_canvas *x);
void canvas_declare(t_canvas *x, t_symbol *s, int argc, t_atom *argv);

/* ---------------- generic widget behavior ------------------------- */

void gobj_getrect(t_gobj *x, t_glist *glist, int *x1, int *y1,
    int *x2, int *y2)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_getrectfn)
        (*x->g_pd->c_wb->w_getrectfn)(x, glist, x1, y1, x2, y2);
    else *x1 = *y1 = 0, *x2 = *y2 = 10;
}

void gobj_displace(t_gobj *x, t_glist *glist, int dx, int dy)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_displacefn)
        (*x->g_pd->c_wb->w_displacefn)(x, glist, dx, dy);
}

    /* here we add an extra check whether we're mapped, because some
       editing moves are carried out on invisible windows (notably, re-creating
       abstractions when one is saved).  Should any other widget functions also
       be doing this?  */
void gobj_select(t_gobj *x, t_glist *glist, int state)
{
    if (glist->gl_mapped && x->g_pd->c_wb && x->g_pd->c_wb->w_selectfn)
        (*x->g_pd->c_wb->w_selectfn)(x, glist, state);
}

void gobj_activate(t_gobj *x, t_glist *glist, int state)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_activatefn)
        (*x->g_pd->c_wb->w_activatefn)(x, glist, state);
}

void gobj_delete(t_gobj *x, t_glist *glist)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_deletefn)
        (*x->g_pd->c_wb->w_deletefn)(x, glist);
}

int gobj_shouldvis(t_gobj *x, struct _glist *glist)
{
    t_object *ob;
    int has_parent = !glist->gl_havewindow && glist->gl_isgraph
        && glist->gl_owner && !glist->gl_isclone;
        /* if our parent is a graph, and if that graph itself isn't
           visible, then we aren't either. */
    if (has_parent && !gobj_shouldvis(&glist->gl_gobj, glist->gl_owner))
            return (0);
        /* if we're graphing-on-parent and the object falls outside the
           graph rectangle, don't draw it. */
    if (has_parent && glist->gl_goprect)
    {
        int x1, y1, x2, y2, gx1, gy1, gx2, gy2, m;
            /* for some reason the bounds check on arrays and scalars
               don't seem to apply here.  Perhaps this was in order to allow
               arrays to reach outside their containers?  I no longer understand
               this. */
        if (pd_class(&x->g_pd) == scalar_class
            || pd_class(&x->g_pd) == garray_class)
                return (1);
        gobj_getrect(&glist->gl_gobj, glist->gl_owner, &x1, &y1, &x2, &y2);
        if (x1 > x2)
            m = x1, x1 = x2, x2 = m;
        if (y1 > y2)
            m = y1, y1 = y2, y2 = m;
        gobj_getrect(x, glist, &gx1, &gy1, &gx2, &gy2);
#if 0
        post("graph %d %d %d %d, %s %d %d %d %d",
             x1, x2, y1, y2, class_gethelpname(x->g_pd), gx1, gx2, gy1, gy2);
#endif
        if (gx1 < x1 || gx1 > x2 || gx2 < x1 || gx2 > x2 ||
            gy1 < y1 || gy1 > y2 || gy2 < y1 || gy2 > y2)
                return (0);
    }
    if ((ob = pd_checkobject(&x->g_pd)))
    {
            /* return true if the text box should be drawn.  We don't show text
               boxes inside graphs---except comments, if we're doing the new
               (goprect) style. */
        return (glist->gl_havewindow ||
            (ob->te_pd != canvas_class &&
                ob->te_pd->c_wb != &text_widgetbehavior) ||
            (ob->te_pd == canvas_class && (((t_glist *)ob)->gl_isgraph)) ||
            (glist->gl_goprect && (ob->te_type == T_TEXT)));
    }
    else return (1);
}

void gobj_vis(t_gobj *x, struct _glist *glist, int flag)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_visfn && gobj_shouldvis(x, glist))
        (*x->g_pd->c_wb->w_visfn)(x, glist, flag);
}

int gobj_click(t_gobj *x, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if (x->g_pd->c_wb && x->g_pd->c_wb->w_clickfn)
        return ((*x->g_pd->c_wb->w_clickfn)(x,
            glist, xpix, ypix, shift, alt, dbl, doit));
    else return (0);
}

    /* maintain the list of visible toplevels for the GUI's "windows" menu */
void canvas_updatewindowlist(void)
{
            /* not if we're in a reload */
    if (!THISGUI->i_reloadingabstraction)
        pdgui_vmess("::pd_menus::update_window_menu", 0);
}

    /* add a glist the list of "root" canvases (toplevels without parents.) */
static void canvas_addtolist(t_canvas *x)
{
    x->gl_next = pd_this->pd_canvaslist;
    pd_this->pd_canvaslist = x;
}

static void canvas_takeofflist(t_canvas *x)
{
        /* take it off the window list */
    if (x == pd_this->pd_canvaslist) pd_this->pd_canvaslist = x->gl_next;
    else
    {
        t_canvas *z;
        for (z = pd_this->pd_canvaslist; z->gl_next != x; z = z->gl_next)
            if (!z->gl_next) return;
        z->gl_next = x->gl_next;
    }
}

    /* turn message tracing on and off globally */

void obj_dosettracing(t_object *ob, int onoff);
static void canvas_dosettracing(t_canvas *x, int onoff)
{
    t_gobj *y;
    for (y = x->gl_list; y; y = y->g_next)
    {
        t_object *ob;
        if (pd_class(&y->g_pd) == canvas_class)
            canvas_dosettracing((t_canvas *)y, onoff);
        if ((ob = pd_checkobject(&y->g_pd)))
            obj_dosettracing(ob, onoff);
    }
}

void canvas_settracing(int onoff)
{
    t_glist *gl;
    for (gl = pd_this->pd_canvaslist; gl; gl = gl->gl_next)
        canvas_dosettracing(gl, onoff);
}

/* --------- functions to handle the canvas environment ----------- */

void canvas_setargs(int argc, const t_atom *argv)
{
        /* if there's an old one lying around free it here.  This
        happens if an abstraction is loaded but never gets as far
        as calling canvas_new(). */
    if (THISGUI->i_newargv)
        freebytes(THISGUI->i_newargv, THISGUI->i_newargc * sizeof(t_atom));
    THISGUI->i_newargc = argc;
    THISGUI->i_newargv = copybytes(argv, argc * sizeof(t_atom));
}

void glob_setfilename(void *dummy, t_symbol *filesym, t_symbol *dirsym)
{
    THISGUI->i_newfilename = filesym;
    THISGUI->i_newdirectory = dirsym;
}

void glob_menunew(void *dummy, t_symbol *filesym, t_symbol *dirsym)
{
    glob_setfilename(dummy, filesym, dirsym);
    canvas_new(0, 0, 0, 0);
    canvas_pop((t_canvas *)s__X.s_thing, 1);
}

t_canvas *canvas_getcurrent(void)
{
    return ((t_canvas *)pd_findbyclass(&s__X, canvas_class));
}

void canvas_setcurrent(t_canvas *x)
{
    pd_pushsym(&x->gl_pd);
}

void canvas_unsetcurrent(t_canvas *x)
{
    pd_popsym(&x->gl_pd);
}

t_canvasenvironment *canvas_getenv(const t_canvas *x)
{
    if (!x) bug("canvas_getenv");
    while (!x->gl_env)
        if (!(x = x->gl_owner))
            bug("t_canvasenvironment");
    return (x->gl_env);
}

int canvas_getdollarzero(void)
{
    t_canvas *x = canvas_getcurrent();
    t_canvasenvironment *env = (x ? canvas_getenv(x) : 0);
    if (env)
        return (env->ce_dollarzero);
    else return (0);
}

void canvas_getargs(int *argcp, t_atom **argvp)
{
    t_canvas *x = canvas_getcurrent();
    t_canvasenvironment *e = canvas_getenv(x);
    *argcp = e->ce_argc;
    *argvp = e->ce_argv;
}

t_symbol *canvas_realizedollar(t_canvas *x, t_symbol *s)
{
    t_symbol *ret;
    const char *name = s->s_name;
    if (strchr(name, '$'))
    {
        t_canvasenvironment *env = canvas_getenv(x);
        canvas_setcurrent(x);
        ret = binbuf_realizedollsym(s, env->ce_argc, env->ce_argv, 1);
        canvas_unsetcurrent(x);
    }
    else ret = s;
    return (ret);
}

t_symbol *canvas_getcurrentdir(void)
{
    t_canvasenvironment *e = canvas_getenv(canvas_getcurrent());
    return (e->ce_dir);
}

t_symbol *canvas_getdir(const t_canvas *x)
{
    t_canvasenvironment *e = canvas_getenv(x);
    return (e->ce_dir);
}

void canvas_makefilename(const t_canvas *x, const char *file, char *result, int resultsize)
{
    const char *dir = canvas_getenv(x)->ce_dir->s_name;
    if (file[0] == '/' || (file[0] && file[1] == ':') || !*dir)
    {
        strncpy(result, file, resultsize);
        result[resultsize-1] = 0;
    }
    else
    {
        int nleft;
        strncpy(result, dir, resultsize);
        result[resultsize-1] = 0;
        nleft = resultsize - (int)strlen(result) - 1;
        if (nleft <= 0) return;
        strcat(result, "/");
        strncat(result, file, nleft);
        result[resultsize-1] = 0;
    }
}

void canvas_rename(t_canvas *x, t_symbol *s, t_symbol *dir)
{
    canvas_unbind(x);
    x->gl_name = s;
    canvas_bind(x);
    if (dir && dir != &s_)
    {
        t_canvasenvironment *e = canvas_getenv(x);
        e->ce_dir = dir;
    }
    if (x->gl_havewindow)
        canvas_reflecttitle(x);
}

/* --------------- traversing the set of lines in a canvas ----------- */

int canvas_getindex(t_canvas *x, t_gobj *y)
{
    t_gobj *y2;
    int indexno;
    for (indexno = 0, y2 = x->gl_list; y2 && y2 != y; y2 = y2->g_next)
        indexno++;
    return (indexno);
}

void linetraverser_start(t_linetraverser *t, t_canvas *x)
{
    t->tr_ob = 0;
    t->tr_x = x;
    t->tr_nextoc = 0;
    t->tr_nextoutno = t->tr_nout = 0;
}

t_outconnect *linetraverser_next(t_linetraverser *t)
{
    t_outconnect *rval = t->tr_nextoc;
    int outno;
    while (!rval)
    {
        outno = t->tr_nextoutno;
        while (outno == t->tr_nout)
        {
            t_gobj *y;
            t_object *ob = 0;
            if (!t->tr_ob) y = t->tr_x->gl_list;
            else y = t->tr_ob->ob_g.g_next;
            for (; y; y = y->g_next)
                if ((ob = pd_checkobject(&y->g_pd))) break;
            if (!ob) return (0);
            t->tr_ob = ob;
            t->tr_nout = obj_noutlets(ob);
            outno = 0;
            if (glist_isvisible(t->tr_x))
                gobj_getrect(y, t->tr_x,
                    &t->tr_x11, &t->tr_y11, &t->tr_x12, &t->tr_y12);
            else t->tr_x11 = t->tr_y11 = t->tr_x12 = t->tr_y12 = 0;
        }
        t->tr_nextoutno = outno + 1;
        rval = obj_starttraverseoutlet(t->tr_ob, &t->tr_outlet, outno);
        t->tr_outno = outno;
    }
    t->tr_nextoc = obj_nexttraverseoutlet(rval, &t->tr_ob2,
        &t->tr_inlet, &t->tr_inno);
    t->tr_nin = obj_ninlets(t->tr_ob2);
    if (!t->tr_nin) bug("drawline");
    if (glist_isvisible(t->tr_x))
    {
        int inplus = (t->tr_nin == 1 ? 1 : t->tr_nin - 1);
        int outplus = (t->tr_nout == 1 ? 1 : t->tr_nout - 1);
        int iow = IOWIDTH * t->tr_x->gl_zoom;
        int iom = IOMIDDLE * t->tr_x->gl_zoom;
        gobj_getrect(&t->tr_ob2->ob_g, t->tr_x,
            &t->tr_x21, &t->tr_y21, &t->tr_x22, &t->tr_y22);
        t->tr_lx1 = t->tr_x11 +
            ((t->tr_x12 - t->tr_x11 - iow) * t->tr_outno) /
                outplus + iom;
        t->tr_ly1 = t->tr_y12;
        t->tr_lx2 = t->tr_x21 +
            ((t->tr_x22 - t->tr_x21 - iow) * t->tr_inno)/inplus +
                iom;
        t->tr_ly2 = t->tr_y21;
    }
    else
    {
        t->tr_x21 = t->tr_y21 = t->tr_x22 = t->tr_y22 = 0;
        t->tr_lx1 = t->tr_ly1 = t->tr_lx2 = t->tr_ly2 = 0;
    }

    return (rval);
}

void linetraverser_skipobject(t_linetraverser *t)
{
    t->tr_nextoc = 0;
    t->tr_nextoutno = t->tr_nout;
}

/* -------------------- the canvas object -------------------------- */
int glist_valid = 10000;

void glist_init(t_glist *x)
{
        /* zero out everyone except "pd" field */
    memset(((char *)x) + sizeof(x->gl_pd), 0, sizeof(*x) - sizeof(x->gl_pd));
    x->gl_stub = gstub_new(x, 0);
    x->gl_valid = ++glist_valid;
    x->gl_xlabel = (t_symbol **)t_getbytes(0);
    x->gl_ylabel = (t_symbol **)t_getbytes(0);
    x->gl_privatedata = getbytes(sizeof(t_canvas_private));
}

    /* make a new glist.  It will either be a "root" canvas or else
    it appears as a "text" object in another window (canvas_getcurrent()
    tells us which.) */
t_canvas *canvas_new(void *dummy, t_symbol *sel, int argc, t_atom *argv)
{
    t_canvas *x = (t_canvas *)pd_new(canvas_class);
    t_canvas *owner = canvas_getcurrent();
    t_symbol *s = &s_;
    int vis = 0, width = GLIST_DEFCANVASWIDTH, height = GLIST_DEFCANVASHEIGHT;
    int xloc = GLIST_DEFCANVASXLOC, yloc = GLIST_DEFCANVASYLOC;
    int font = (owner ? owner->gl_font : sys_defaultfont);
    glist_init(x);
    x->gl_obj.te_type = T_OBJECT;
    if (!owner)
        canvas_addtolist(x);
    /* post("canvas %lx, owner %lx", x, owner); */

    if (argc == 5)  /* toplevel: x, y, w, h, font */
    {
        xloc = atom_getfloatarg(0, argc, argv);
        yloc = atom_getfloatarg(1, argc, argv);
        width = atom_getfloatarg(2, argc, argv);
        height = atom_getfloatarg(3, argc, argv);
        font = atom_getfloatarg(4, argc, argv);
    }
    else if (argc == 6)  /* subwindow: x, y, w, h, name, vis */
    {
        xloc = atom_getfloatarg(0, argc, argv);
        yloc = atom_getfloatarg(1, argc, argv);
        width = atom_getfloatarg(2, argc, argv);
        height = atom_getfloatarg(3, argc, argv);
        s = atom_getsymbolarg(4, argc, argv);
        vis = atom_getfloatarg(5, argc, argv);
    }

        /* (otherwise assume we're being created from the menu.) */
    if (THISGUI->i_newdirectory &&
        THISGUI->i_newdirectory->s_name[0])
    {
        t_canvasenvironment *env = x->gl_env =
            (t_canvasenvironment *)getbytes(sizeof(*x->gl_env));
        if (!THISGUI->i_newargv)
            THISGUI->i_newargv = getbytes(0);
        env->ce_dir = THISGUI->i_newdirectory;
        env->ce_argc = THISGUI->i_newargc;
        env->ce_argv = THISGUI->i_newargv;
        env->ce_dollarzero = THISGUI->i_dollarzero++;
        env->ce_path = 0;
        THISGUI->i_newdirectory = &s_;
        THISGUI->i_newargc = 0;
        THISGUI->i_newargv = 0;
    }
    else x->gl_env = 0;

        /* initialize private data, like the undo-queue */
    canvas_undo_init(x);

    x->gl_x1 = 0;
    x->gl_y1 = 0;
    x->gl_x2 = 1;
    x->gl_y2 = 1;
    canvas_dosetbounds(x, xloc, yloc, xloc + width, yloc + height);
    x->gl_owner = owner;
    x->gl_isclone = 0;
    x->gl_name = (*s->s_name ? s :
        (THISGUI->i_newfilename ? THISGUI->i_newfilename : gensym("Pd")));
    canvas_bind(x);
    x->gl_loading = 1;
    x->gl_goprect = 0;      /* no GOP rectangle unless it's turned on later */
        /* cancel "vis" flag if we're a subpatch of an
         abstraction inside another patch.  A separate mechanism prevents
         the toplevel abstraction from showing up. */
    if (vis && gensym("#X")->s_thing &&
        ((*gensym("#X")->s_thing) == canvas_class))
    {
        t_canvas *zzz = (t_canvas *)(gensym("#X")->s_thing);
        while (zzz && !zzz->gl_env)
            zzz = zzz->gl_owner;
        if (zzz && canvas_isabstraction(zzz) && zzz->gl_owner)
            vis = 0;
    }
    x->gl_willvis = vis;
    x->gl_edit = !UNTITLED_STRNCMP(x->gl_name->s_name);
    x->gl_font = sys_nearestfontsize(font);
    x->gl_zoom = (owner ? owner->gl_zoom : 1);
    pd_pushsym(&x->gl_pd);
    return(x);
}

void canvas_setgraph(t_glist *x, int flag, int nogoprect);

static void canvas_coords(t_glist *x, t_symbol *s, int argc, t_atom *argv)
{
          /* FIXME: this is a stopgap - we should always be using
            glist_getzoom() and never gl_zoom in rest of code. */
    x->gl_zoom = glist_getzoom(x);
    x->gl_x1 = atom_getfloatarg(0, argc, argv);
    x->gl_y1 = atom_getfloatarg(1, argc, argv);
    x->gl_x2 = atom_getfloatarg(2, argc, argv);
    x->gl_y2 = atom_getfloatarg(3, argc, argv);
    x->gl_pixwidth = atom_getfloatarg(4, argc, argv);
    x->gl_pixheight = atom_getfloatarg(5, argc, argv);
    if (argc <= 7)
        canvas_setgraph(x, atom_getfloatarg(6, argc, argv), 1);
    else
    {
        x->gl_xmargin = atom_getfloatarg(7, argc, argv);
        x->gl_ymargin = atom_getfloatarg(8, argc, argv);
        canvas_setgraph(x, atom_getfloatarg(6, argc, argv), 0);
    }
}


    /* make a new glist and add it to this glist.  It will appear as
    a "graph", not a text object.  */
t_glist *glist_addglist(t_glist *g, t_symbol *sym,
    t_float x1, t_float y1, t_float x2, t_float y2,
    t_float px1, t_float py1, t_float px2, t_float py2)
{
    static int gcount = 0;  /* it's OK if two threads get the same value */
    int zz;
    int menu = 0;
    const char *str;
    t_glist *x = (t_glist *)pd_new(canvas_class);
    glist_init(x);
    x->gl_obj.te_type = T_OBJECT;
    if (!*sym->s_name)
    {
        char buf[40];
        sprintf(buf, "graph%d", ++gcount);
        sym = gensym(buf);
        menu = 1;
    }
    else if (!strncmp((str = sym->s_name), "graph", 5)
        && (zz = atoi(str + 5)) > gcount)
            gcount = zz;
        /* in 0.34 and earlier, the pixel rectangle and the y bounds were
        reversed; this would behave the same, except that the dialog window
        would be confusing.  The "correct" way is to have "py1" be the value
        that is higher on the screen. */
    if (py2 < py1)
    {
        t_float zz;
        zz = y2;
        y2 = y1;
        y1 = zz;
        zz = py2;
        py2 = py1;
        py1 = zz;
    }
    if (x1 == x2 || y1 == y2)
        x1 = 0, x2 = 100, y1 = 1, y2 = -1;
    if (px1 >= px2 || py1 >= py2)
        px1 = 100, py1 = 20, px2 = 100 + GLIST_DEFGRAPHWIDTH,
            py2 = 20 + GLIST_DEFGRAPHHEIGHT;
    x->gl_name = sym;
    x->gl_x1 = x1;
    x->gl_x2 = x2;
    x->gl_y1 = y1;
    x->gl_y2 = y2;
    x->gl_obj.te_xpix = px1;
    x->gl_obj.te_ypix = py1;
    x->gl_pixwidth = px2 - px1;
    x->gl_pixheight = py2 - py1;
    x->gl_font =  (canvas_getcurrent() ?
        canvas_getcurrent()->gl_font : sys_defaultfont);
    x->gl_zoom = g->gl_zoom;
    x->gl_screenx1 = GLIST_DEFCANVASXLOC;
    x->gl_screeny1 = GLIST_DEFCANVASYLOC;
    x->gl_screenx2 = GLIST_DEFCANVASWIDTH;
    x->gl_screeny2 = GLIST_DEFCANVASHEIGHT;
    x->gl_owner = g;
    canvas_bind(x);
    x->gl_isgraph = 1;
    x->gl_goprect = 0;
    x->gl_obj.te_binbuf = binbuf_new();
        /* initialize private data, like the undo-queue */
    canvas_undo_init(x);

    binbuf_addv(x->gl_obj.te_binbuf, "s", gensym("graph"));
    if (!menu)
        pd_pushsym(&x->gl_pd);
    glist_add(g, &x->gl_gobj);
    return (x);
}

    /* call glist_addglist from a Pd message */
void glist_glist(t_glist *g, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *sym = atom_getsymbolarg(0, argc, argv);
    t_float x1 = atom_getfloatarg(1, argc, argv);
    t_float y1 = atom_getfloatarg(2, argc, argv);
    t_float x2 = atom_getfloatarg(3, argc, argv);
    t_float y2 = atom_getfloatarg(4, argc, argv);
    t_float px1 = atom_getfloatarg(5, argc, argv);
    t_float py1 = atom_getfloatarg(6, argc, argv);
    t_float px2 = atom_getfloatarg(7, argc, argv);
    t_float py2 = atom_getfloatarg(8, argc, argv);
    glist_addglist(g, sym, x1, y1, x2, y2, px1, py1, px2, py2);
    if (!canvas_undo_get(glist_getcanvas(g))->u_doing)
        canvas_undo_add(glist_getcanvas(g), UNDO_CREATE, "create",
            (void *)canvas_undo_set_create(glist_getcanvas(g)));
}

    /* return true if the glist should appear as a graph on parent;
    otherwise it appears as a text box. */
int glist_isgraph(t_glist *x)
{
  return (x->gl_isgraph|(x->gl_hidetext<<1));
}

    /* This is sent from the GUI to inform a toplevel that its window has been
    moved or resized. */
static void canvas_setbounds(t_canvas *x, t_float left, t_float top,
                             t_float right, t_float bottom)
{
    canvas_dosetbounds(x, (int)left, (int)top, (int)right, (int)bottom);
}

/* this is the internal version using ints */
static void canvas_dosetbounds(t_canvas *x, int x1, int y1, int x2, int y2)
{
    int heightwas = y2 - y1;
    int heightchange = y2 - y1 - (x->gl_screeny2 - x->gl_screeny1);
    if (x->gl_screenx1 == x1 && x->gl_screeny1 == y1 &&
        x->gl_screenx2 == x2 && x->gl_screeny2 == y2)
            return;
    x->gl_screenx1 = x1;
    x->gl_screeny1 = y1;
    x->gl_screenx2 = x2;
    x->gl_screeny2 = y2;
    if (!glist_isgraph(x) && (x->gl_y2 < x->gl_y1))
    {
            /* if it's flipped so that y grows upward,
            fix so that zero is bottom edge and redraw.  This is
            only appropriate if we're a regular "text" object on the
            parent. */
        t_float diff = x->gl_y1 - x->gl_y2;
        t_gobj *y;
        x->gl_y1 = heightwas * diff/x->gl_zoom;
        x->gl_y2 = x->gl_y1 - diff;
            /* and move text objects accordingly; they should stick
            to the bottom, not the top. */
        for (y = x->gl_list; y; y = y->g_next)
            if (pd_checkobject(&y->g_pd))
                gobj_displace(y, x, 0, heightchange/x->gl_zoom);
        canvas_redraw(x);
    }
}

t_symbol *canvas_makebindsym(t_symbol *s)
{
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, "pd-%s", s->s_name);
    buf[MAXPDSTRING-1] = 0;
    return (gensym(buf));
}

    /* functions to bind and unbind canvases to symbol "pd-blah".  As
    discussed on Pd dev list there should be a way to defeat this for
    abstractions.  (Claude Heiland et al. Aug 9 2013) */
static void canvas_bind(t_canvas *x)
{
    if (strcmp(x->gl_name->s_name, "Pd"))
        pd_bind(&x->gl_pd, canvas_makebindsym(x->gl_name));
}

static void canvas_unbind(t_canvas *x)
{
    if (strcmp(x->gl_name->s_name, "Pd"))
        pd_unbind(&x->gl_pd, canvas_makebindsym(x->gl_name));
}

void canvas_reflecttitle(t_canvas *x)
{
    char namebuf[MAXPDSTRING];
    t_canvasenvironment *env = canvas_getenv(x);
    if (!x->gl_havewindow)
    {
        bug("canvas_reflecttitle");
        return;
    }
    if (env->ce_argc)
    {
        int i;
        strcpy(namebuf, " (");
        for (i = 0; i < env->ce_argc; i++)
        {
            if (strlen(namebuf) > MAXPDSTRING/2 - 5)
                break;
            if (i != 0)
                strcat(namebuf, " ");
            atom_string(&env->ce_argv[i], namebuf + strlen(namebuf),
                MAXPDSTRING/2);
        }
        strcat(namebuf, ")");
    }
    else namebuf[0] = 0;
    if (x->gl_edit)
    {
        strncat(namebuf, " [edit]", MAXPDSTRING-strlen(namebuf)-1);
        namebuf[MAXPDSTRING-1] = 0;
    }
    pdgui_vmess("pdtk_canvas_reflecttitle", "^ sss i",
        x,
        canvas_getdir(x)->s_name, x->gl_name->s_name, namebuf,
        x->gl_dirty);
}

    /* mark a glist dirty or clean */
void canvas_dirty(t_canvas *x, t_floatarg f)
{
    t_canvas *x2 = canvas_getrootfor(x);
    unsigned int n = f;
    if (THISGUI->i_reloadingabstraction)
        return;
    if (n != x2->gl_dirty)
    {
        x2->gl_dirty = n;
        if (x2->gl_havewindow)
            canvas_reflecttitle(x2);
    }
    if(!n)
        canvas_undo_cleardirty(x);
}

void canvas_drawredrect(t_canvas *x, int doit)
{
    if (doit)
    {
        int x1 = x->gl_zoom * x->gl_xmargin,
            x2 = x1 + x->gl_zoom * x->gl_pixwidth,
            y1 = x->gl_zoom * x->gl_ymargin,
            y2 = y1 + x->gl_zoom * x->gl_pixheight;
        pdgui_vmess(0, "crr iiiiiiiiii rr ri rr rr",
            glist_getcanvas(x), "create", "line",
            x1,y1, x1,y2, x2,y2, x2,y1, x1,y1,
            "-fill", "#ff8080",
            "-width", x->gl_zoom,
            "-capstyle", "projecting",
            "-tags", "GOP"); /* better: "-tags", 1, &"GOP" */
    }
    else
        pdgui_vmess(0, "crs", glist_getcanvas(x), "delete", "GOP");
}

    /* the window becomes "mapped" (visible and not miniaturized) or
    "unmapped" (either miniaturized or just plain gone.)  This should be
    called from the GUI after the fact to "notify" us that we're mapped. */
void canvas_map(t_canvas *x, t_floatarg f)
{
    int flag = (f != 0);
    t_gobj *y;
    if (flag)
    {
        if (!glist_isvisible(x))
        {
            t_selection *sel;
            if (!x->gl_havewindow)
            {
                bug("canvas_map");
                canvas_vis(x, 1);
            }
            for (y = x->gl_list; y; y = y->g_next)
                gobj_vis(y, x, 1);
            x->gl_mapped = 1;
            for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
                gobj_select(sel->sel_what, x, 1);
            canvas_drawlines(x);
            if (x->gl_isgraph && x->gl_goprect)
                canvas_drawredrect(x, 1);
            pdgui_vmess("pdtk_canvas_getscroll", "c", x);
        }
    }
    else
    {
        if (glist_isvisible(x))
        {
            if (!x->gl_havewindow)
            {
                bug("canvas_map");
                return;
            }
                /* just clear out the whole canvas */
            pdgui_vmess(0, "crs", x, "delete", "all");
            x->gl_mapped = 0;
        }
    }
}

void canvas_redraw(t_canvas *x)
{
    if (glist_isvisible(x))
    {
        canvas_map(x, 0);
        canvas_map(x, 1);
    }
}


    /* we call this on a non-toplevel glist to "open" it into its
    own window. */
void glist_menu_open(t_glist *x)
{
    if (glist_isvisible(x) && !glist_istoplevel(x))
    {
        t_glist *gl2 = x->gl_owner;
        if (!gl2)
            bug("glist_menu_open");  /* shouldn't happen but not dangerous */
        else
        {
                /* erase ourself in parent window */
            gobj_vis(&x->gl_gobj, gl2, 0);
                    /* get rid of our editor (and subeditors) */
            if (x->gl_editor)
                canvas_destroy_editor(x);
            x->gl_havewindow = 1;
                    /* redraw ourself in parent window (blanked out this time) */
            gobj_vis(&x->gl_gobj, gl2, 1);
        }
    }
    canvas_vis(x, 1);
}

int glist_isvisible(t_glist *x)
{
    return ((!x->gl_loading) && glist_getcanvas(x)->gl_mapped);
}

int glist_istoplevel(t_glist *x)
{
        /* we consider a graph "toplevel" if it has its own window
        or if it appears as a box in its parent window so that we
        don't draw the actual contents there. */
    return (x->gl_havewindow || !x->gl_isgraph);
}

int glist_getfont(t_glist *x)
{
    while (!x->gl_env)
        if (!(x = x->gl_owner))
            bug("t_canvasenvironment");
    return (x->gl_font);
}

int glist_getzoom(t_glist *x)
{
    t_glist *gl2 = x;
    while (!glist_istoplevel(gl2) && gl2->gl_owner)
        gl2 = gl2->gl_owner;
    return (gl2->gl_zoom);
}

int glist_fontwidth(t_glist *x)
{
    return (sys_zoomfontwidth(glist_getfont(x), glist_getzoom(x), 0));
}

int glist_fontheight(t_glist *x)
{
    return (sys_zoomfontheight(glist_getfont(x), glist_getzoom(x), 0));
}

void canvas_free(t_canvas *x)
{
    t_gobj *y;
    t_canvas_private*private = x->gl_privatedata;
    int dspstate = canvas_suspend_dsp();
    canvas_noundo(x);
    if (canvas_whichfind == x)
        canvas_whichfind = 0;
    glist_noselect(x);
    while ((y = x->gl_list))
        glist_delete(x, y);
    if (x == glist_getcanvas(x))
        canvas_vis(x, 0);
    if (x->gl_editor)
        canvas_destroy_editor(x);   /* bug workaround; should already be gone*/
    canvas_unbind(x);

    if (x->gl_env)
    {
        freebytes(x->gl_env->ce_argv, x->gl_env->ce_argc * sizeof(t_atom));
        freebytes(x->gl_env, sizeof(*x->gl_env));
    }
    canvas_undo_free(x);
    freebytes(private, sizeof(*private));
    canvas_resume_dsp(dspstate);
    freebytes(x->gl_xlabel, x->gl_nxlabels * sizeof(*(x->gl_xlabel)));
    freebytes(x->gl_ylabel, x->gl_nylabels * sizeof(*(x->gl_ylabel)));
    gstub_cutoff(x->gl_stub);
    pdgui_stub_deleteforkey(x);        /* probably unnecessary */
    if (!x->gl_owner && !x->gl_isclone)
        canvas_takeofflist(x);
}

/* ----------------- lines ---------- */

static void canvas_drawlines(t_canvas *x)
{
    t_linetraverser t;
    t_outconnect *oc;
    {
        char tag[128];
        const char*tags[2] = {tag, "cord"};
        linetraverser_start(&t, x);
        while ((oc = linetraverser_next(&t)))
        {
            sprintf(tag, "l%lx", oc);
            pdgui_vmess(0, "crr iiii ri rS",
                glist_getcanvas(x), "create", "line",
                t.tr_lx1,t.tr_ly1, t.tr_lx2,t.tr_ly2,
                "-width", (outlet_getsymbol(t.tr_outlet) == &s_signal ? 2:1) * x->gl_zoom,
                "-tags", 2, tags);
        }
    }
}
void canvas_fixlinesfor(t_canvas *x, t_text *text)
{
    t_linetraverser t;
    t_outconnect *oc;

    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
    {
        if (t.tr_ob == text || t.tr_ob2 == text)
        {
            char tag[128];
            sprintf(tag, "l%lx", oc);
            pdgui_vmess(0, "crs iiii",
                glist_getcanvas(x), "coords", tag,
                t.tr_lx1,t.tr_ly1, t.tr_lx2,t.tr_ly2);
        }
    }
}

static void _canvas_delete_line(t_canvas*x, t_outconnect *oc)
{
    char tag[128];
    if (!glist_isvisible(x))
        return;
    sprintf(tag, "l%lx", oc);
    pdgui_vmess(0, "crs", glist_getcanvas(x), "delete", tag);
}

    /* kill all lines for the object */
void canvas_deletelinesfor(t_canvas *x, t_text *text)
{
    t_linetraverser t;
    t_outconnect *oc;
    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
    {
        if (t.tr_ob == text || t.tr_ob2 == text)
        {
            _canvas_delete_line(x, oc);
            obj_disconnect(t.tr_ob, t.tr_outno, t.tr_ob2, t.tr_inno);
        }
    }
}

    /* kill all lines for one inlet or outlet */
void canvas_deletelinesforio(t_canvas *x, t_text *text,
    t_inlet *inp, t_outlet *outp)
{
    t_linetraverser t;
    t_outconnect *oc;
    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
    {
        if ((t.tr_ob == text && t.tr_outlet == outp) ||
            (t.tr_ob2 == text && t.tr_inlet == inp))
        {
            _canvas_delete_line(x, oc);
            obj_disconnect(t.tr_ob, t.tr_outno, t.tr_ob2, t.tr_inno);
        }
    }
}

typedef void (*t_zoomfn)(void *x, t_floatarg arg1);

static void canvas_pop(t_canvas *x, t_floatarg fvis)
{
    if (glist_istoplevel(x) && (sys_zoom_open == 2))
    {
        t_zoomfn zoommethod = (t_zoomfn)zgetfn(&x->gl_pd, gensym("zoom"));
        if (zoommethod)
            (*zoommethod)(&x->gl_pd, (t_floatarg)2);
    }
    if (fvis != 0)
        canvas_vis(x, 1);
    pd_popsym(&x->gl_pd);
    canvas_resortinlets(x);
    canvas_resortoutlets(x);
    x->gl_loading = 0;
}

void canvas_objfor(t_glist *gl, t_text *x, int argc, t_atom *argv);


void canvas_restore(t_canvas *x, t_symbol *s, int argc, t_atom *argv)
{
    t_pd *z;
    if (argc > 3)
    {
        t_atom *ap=argv+3;
        if (ap->a_type == A_SYMBOL)
        {
            t_canvasenvironment *e = canvas_getenv(canvas_getcurrent());
            canvas_rename(x, binbuf_realizedollsym(ap->a_w.w_symbol,
                e->ce_argc, e->ce_argv, 1), 0);
        }
    }
    canvas_pop(x, x->gl_willvis);

    if (!(z = gensym("#X")->s_thing)) pd_error(0, "canvas_restore: out of context");
    else if (*z != canvas_class) pd_error(0, "canvas_restore: wasn't a canvas");
    else
    {
        t_canvas *x2 = (t_canvas *)z;
        x->gl_owner = x2;
        canvas_objfor(x2, &x->gl_obj, argc, argv);
    }
}

static void canvas_loadbangabstractions(t_canvas *x)
{
    t_gobj *y;
    t_symbol *s = gensym("loadbang");
    for (y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == canvas_class)
        {
            if (canvas_isabstraction((t_canvas *)y))
                canvas_loadbang((t_canvas *)y);
            else
                canvas_loadbangabstractions((t_canvas *)y);
        }
        else if ((pd_class(&y->g_pd) == clone_class) &&
            zgetfn(&y->g_pd, s))
        {
            pd_vmess(&y->g_pd, s, "f", (t_floatarg)LB_LOAD);
        }
}

void canvas_loadbangsubpatches(t_canvas *x)
{
    t_gobj *y;
    t_symbol *s = gensym("loadbang");
    for (y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == canvas_class)
        {
            if (!canvas_isabstraction((t_canvas *)y))
                canvas_loadbangsubpatches((t_canvas *)y);
        }
    for (y = x->gl_list; y; y = y->g_next)
        if ((pd_class(&y->g_pd) != canvas_class) &&
            (pd_class(&y->g_pd) != clone_class) &&
            zgetfn(&y->g_pd, s))
        {
            pd_vmess(&y->g_pd, s, "f", (t_floatarg)LB_LOAD);
        }
}

void canvas_loadbang(t_canvas *x)
{
    canvas_loadbangabstractions(x);
    canvas_loadbangsubpatches(x);
}

/* JMZ/MSP:
 * initbang is emitted after a canvas is read from a file, but before the
   parent canvas is finished loading.  This is apparently used so that
   abstractions can create inlets/outlets as a function of creation arguments.
   This practice is quite ugly but there's no other way to do it so far.
 */
void canvas_initbang(t_canvas *x)
{
    t_gobj *y;
    t_symbol *s = gensym("loadbang");
    /* run "initbang" for all subpatches, but NOT for the child abstractions */
    for (y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == canvas_class &&
            !canvas_isabstraction((t_canvas *)y))
                canvas_initbang((t_canvas *)y);
    /* call the initbang()-method for objects that have one */
    for (y = x->gl_list; y; y = y->g_next)
        if ((pd_class(&y->g_pd) != canvas_class) && zgetfn(&y->g_pd, s))
            pd_vmess(&y->g_pd, s, "f", (t_floatarg)LB_INIT);
}

/* JMZ:
 * closebang is emitted before the canvas is destroyed
 * and BEFORE subpatches/abstractions in this canvas are destroyed
 */
void canvas_closebang(t_canvas *x)
{
    t_gobj *y;
    t_symbol *s = gensym("loadbang");

    /* call the closebang()-method for objects that have one
     * but NOT for subpatches/abstractions: these are called separately
     * from g_graph:glist_delete()
     */
    for (y = x->gl_list; y; y = y->g_next)
        if ((pd_class(&y->g_pd) != canvas_class) && zgetfn(&y->g_pd, s))
            pd_vmess(&y->g_pd, s, "f", (t_floatarg)LB_CLOSE);
}

/* no longer used by 'pd-gui', but kept here for backwards compatibility.  The
 * new method calls canvas_setbounds() directly. */
static void canvas_relocate(t_canvas *x, t_symbol *canvasgeom,
    t_symbol *topgeom)
{
    int cxpix, cypix, cw, ch, txpix, typix, tw, th;
    if (sscanf(canvasgeom->s_name, "%dx%d+%d+%d", &cw, &ch, &cxpix, &cypix)
        < 4 ||
        sscanf(topgeom->s_name, "%dx%d+%d+%d", &tw, &th, &txpix, &typix) < 4)
        bug("canvas_relocate");
            /* for some reason this is initially called with cw=ch=1 so
            we just suppress that here. */
    if (cw > 5 && ch > 5)
        canvas_dosetbounds(x, txpix, typix,
            txpix + cw, typix + ch);
}

void canvas_popabstraction(t_canvas *x)
{
    pd_this->pd_newest = &x->gl_pd;
    gensym("#A")->s_thing = 0;
    pd_bind(pd_this->pd_newest, gensym("#A"));
    pd_popsym(&x->gl_pd);
    x->gl_loading = 0;
    canvas_resortinlets(x);
    canvas_resortoutlets(x);
}

void canvas_logerror(t_object *y)
{
#ifdef LATER
    canvas_vis(x, 1);
    if (!glist_isselected(x, &y->ob_g))
        glist_select(x, &y->ob_g);
#endif
}

/* -------------------------- subcanvases ---------------------- */

extern void canvas_obj(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
static void *subcanvas_new(t_symbol *s)
{
    t_atom a[6];
    t_canvas *x, *z = canvas_getcurrent();

    if (!*s->s_name) s = gensym("/SUBPATCH/");
    SETFLOAT(a+0, GLIST_DEFCANVASXLOC);
    SETFLOAT(a+1, GLIST_DEFCANVASYLOC);
    SETFLOAT(a+2, GLIST_DEFCANVASWIDTH);
    SETFLOAT(a+3, GLIST_DEFCANVASHEIGHT);
    SETSYMBOL(a+4, s);
    SETFLOAT(a+5, 1);
    x = canvas_new(0, 0, 6, a);

        /* check if subpatch is supposed to be connected (on the 1st inlet) */
    if(z && z->gl_editor && z->gl_editor->e_connectbuf)
    {
        t_atom*argv = binbuf_getvec(z->gl_editor->e_connectbuf);
        int argc = binbuf_getnatom(z->gl_editor->e_connectbuf);
        t_symbol *sob = 0;
        if ((argc == 7)
            && atom_getsymbolarg(0, argc, argv) == gensym("#X")
            && atom_getsymbolarg(1, argc, argv) == gensym("connect"))
        {
            int index2 = canvas_getindex(z, &x->gl_gobj);
            if (((int)atom_getfloat(argv+5) == 0)
                && (int)atom_getfloat(argv+4) == index2)
                {
                    int index1 = (int)atom_getfloat(argv+2);
                    int outno = (int)atom_getfloat(argv+3);
                    t_gobj*outobj=z->gl_list;
                        /* get handle to object */
                    while(index1-->0 && outobj)
                        outobj=outobj->g_next;
                    if(outobj && pd_checkobject(&outobj->g_pd))
                    {
                        if (obj_issignaloutlet(pd_checkobject(&outobj->g_pd), outno))
                            sob = gensym("inlet~");
                        else
                            sob = gensym("inlet");
                    }
                }
        }
        if(sob)
        {
                /* JMZ: weirdo hardcoded numbers, taken from
                 * glist_getnextxy(): 40
                 * and canvas_howputnew(): -3
                 */
            SETFLOAT(a+0, 37);
            SETFLOAT(a+1, 37);
            SETSYMBOL(a+2, sob);
            canvas_obj(x, gensym("obj"), 3, a);

                /* select the newly created inlet to continue autopatching */
            canvas_create_editor(x);
            glist_noselect(x);
            glist_select(x, x->gl_list);
        }
    }
    x->gl_owner = z;
    canvas_pop(x, 1);
    return (x);
}

static void canvas_click(t_canvas *x,
    t_floatarg xpos, t_floatarg ypos,
        t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    canvas_vis(x, 1);
}


    /* find out from subcanvas contents how much to fatten the box */
void canvas_fattensub(t_canvas *x,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_gobj *y;
    *xp2 += 50;     /* fake for now */
    *yp2 += 50;
}

static void canvas_rename_method(t_canvas *x, t_symbol *s, int ac, t_atom *av)
{
    if (ac && av->a_type == A_SYMBOL)
        canvas_rename(x, av->a_w.w_symbol, 0);
    else if (ac && av->a_type == A_DOLLSYM)
    {
        t_canvasenvironment *e = canvas_getenv(x);
        canvas_setcurrent(x);
        canvas_rename(x, binbuf_realizedollsym(av->a_w.w_symbol,
            e->ce_argc, e->ce_argv, 1), 0);
        canvas_unsetcurrent(x);
    }
    else canvas_rename(x, gensym("Pd"), 0);
}


    /* return true if the "canvas" object is an abstraction (so we don't
    save its contents, for example.)  */
int canvas_isabstraction(const t_canvas *x)
{
    return (x->gl_env != 0);
}

    /* return true if the "canvas" object should be treated as a text
    object.  This is true for abstractions but also for "table"s... */
/* JMZ: add a flag to gop-abstractions to hide the title */
int canvas_showtext(const t_canvas *x)
{
    t_atom *argv = (x->gl_obj.te_binbuf? binbuf_getvec(x->gl_obj.te_binbuf):0);
    int argc = (x->gl_obj.te_binbuf? binbuf_getnatom(x->gl_obj.te_binbuf) : 0);
    int isarray = (argc && argv[0].a_type == A_SYMBOL &&
        argv[0].a_w.w_symbol == gensym("graph"));
    if(x->gl_hidetext)
      return 0;
    else
      return (!isarray);
}

    /* get the document containing this canvas */
t_canvas *canvas_getrootfor(t_canvas *x)
{
    if ((!x->gl_owner) || canvas_isabstraction(x))
        return (x);
    else return (canvas_getrootfor(x->gl_owner));
}

t_undo* canvas_undo_get(t_canvas *x)
{
    t_canvas_private*private = x?(x->gl_privatedata):0;
    if(private)
        return &(private->undo);
    return 0;
}

/* ------------------------- DSP chain handling ------------------------- */

EXTERN_STRUCT _dspcontext;
#define t_dspcontext struct _dspcontext

void ugen_start(void);
void ugen_stop(void);

t_dspcontext *ugen_start_graph(int toplevel, t_signal **sp,
    int ninlets, int noutlets);
void ugen_add(t_dspcontext *dc, t_object *x);
void ugen_connect(t_dspcontext *dc, t_object *x1, int outno,
    t_object *x2, int inno);
void ugen_done_graph(t_dspcontext *dc);

    /* schedule one canvas for DSP.  This is called below for all "root"
    canvases, but is also called from the "dsp" method for sub-
    canvases, which are treated almost like any other tilde object.  */

void canvas_dodsp(t_canvas *x, int toplevel, t_signal **sp)
{
    t_linetraverser t;
    t_outconnect *oc;
    t_gobj *y;
    t_object *ob;
    t_symbol *dspsym = gensym("dsp");
    t_dspcontext *dc;

        /* create a new "DSP graph" object to use in sorting this canvas.
        If we aren't toplevel, there are already other dspcontexts around. */

    dc = ugen_start_graph(toplevel, sp,
        obj_nsiginlets(&x->gl_obj),
        obj_nsigoutlets(&x->gl_obj));

        /* find all the "dsp" boxes and add them to the graph */

    for (y = x->gl_list; y; y = y->g_next)
        if ((ob = pd_checkobject(&y->g_pd)) && zgetfn(&y->g_pd, dspsym))
            ugen_add(dc, ob);

        /* ... and all dsp interconnections */
    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
        if (obj_issignaloutlet(t.tr_ob, t.tr_outno))
            ugen_connect(dc, t.tr_ob, t.tr_outno, t.tr_ob2, t.tr_inno);

        /* finally, sort them and add them to the DSP chain */
    ugen_done_graph(dc);
}

static void canvas_dsp(t_canvas *x, t_signal **sp)
{
    canvas_dodsp(x, 0, sp);
}

int canvas_dspstate;    /* for back compatibility with externs - don't use */

    /* this routine starts DSP for all root canvases. */
static void canvas_start_dsp(void)
{
    t_canvas *x;
    if (THISGUI->i_dspstate) ugen_stop();
    else pdgui_vmess("pdtk_pd_dsp", "s", "ON");
    ugen_start();

    for (x = pd_getcanvaslist(); x; x = x->gl_next)
        canvas_dodsp(x, 1, 0);

    canvas_dspstate = THISGUI->i_dspstate = 1;
    if (gensym("pd-dsp-started")->s_thing)
        pd_bang(gensym("pd-dsp-started")->s_thing);
}

static void canvas_stop_dsp(void)
{
    if (THISGUI->i_dspstate)
    {
        ugen_stop();
        pdgui_vmess("pdtk_pd_dsp", "s", "OFF");
        canvas_dspstate = THISGUI->i_dspstate = 0;
        if (gensym("pd-dsp-stopped")->s_thing)
            pd_bang(gensym("pd-dsp-stopped")->s_thing);
    }
}

    /* DSP can be suspended before, and resumed after, operations which
    might affect the DSP chain.  For example, we suspend before loading and
    resume afterward, so that DSP doesn't get resorted for every DSP object
    int the patch. */

int canvas_suspend_dsp(void)
{
    int rval = THISGUI->i_dspstate;
    if (rval) canvas_stop_dsp();
    return (rval);
}

void canvas_resume_dsp(int oldstate)
{
    if (oldstate) canvas_start_dsp();
}

    /* this is equivalent to suspending and resuming in one step. */
void canvas_update_dsp(void)
{
    if (THISGUI->i_dspstate) canvas_start_dsp();
}

/* the "dsp" message to pd starts and stops DSP computation, and, if
appropriate, also opens and closes the audio device.  On exclusive-access
APIs such as ALSA, MMIO, and ASIO (I think) it's appropriate to close the
audio devices when not using them; but jack behaves better if audio I/O
simply keeps running.  This is wasteful of CPU cycles but we do it anyway
and can perhaps regard this is a design flaw in jack that we're working around
here.  The function audio_shouldkeepopen() is provided by s_audio.c to tell
us that we should elide the step of closing audio when DSP is turned off.*/

void glob_dsp(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int newstate;
    if (argc)
    {
        newstate = atom_getfloatarg(0, argc, argv);
        if (newstate && !THISGUI->i_dspstate)
        {
            sys_set_audio_state(1);
            canvas_start_dsp();
        }
        else if (!newstate && THISGUI->i_dspstate)
        {
            canvas_stop_dsp();
            if (!audio_shouldkeepopen())
                sys_set_audio_state(0);
        }
    }
    else post("dsp state %d", THISGUI->i_dspstate);
}

void *canvas_getblock(t_class *blockclass, t_canvas **canvasp)
{
    t_canvas *canvas = *canvasp;
    t_gobj *g;
    void *ret = 0;
    for (g = canvas->gl_list; g; g = g->g_next)
    {
        if (g->g_pd == blockclass)
            ret = g;
    }
    *canvasp = canvas->gl_owner;
    return(ret);
}

/******************* redrawing  data *********************/

    /* redraw all "scalars" (do this if a drawing command is changed.)
    LATER we'll use the "template" information to select which ones we
    redraw.   Action = 0 for redraw, 1 for draw only, 2 for erase. */
static void glist_redrawall(t_glist *gl, int action)
{
    t_gobj *g;
    int vis = glist_isvisible(gl);
    for (g = gl->gl_list; g; g = g->g_next)
    {
        if (vis && g->g_pd == scalar_class)
        {
            if (action == 1)
            {
                if (glist_isvisible(gl))
                    gobj_vis(g, gl, 1);
            }
            else if (action == 2)
            {
                if (glist_isvisible(gl))
                    gobj_vis(g, gl, 0);
            }
            else scalar_redraw((t_scalar *)g, gl);
        }
        else if (g->g_pd == canvas_class)
            glist_redrawall((t_glist *)g, action);
    }
}

    /* public interface for above. */
void canvas_redrawallfortemplate(t_template *template, int action)
{
    t_canvas *x;
        /* find all root canvases */
    for (x = pd_getcanvaslist(); x; x = x->gl_next)
        glist_redrawall(x, action);
}

    /* find the template defined by a canvas, and redraw all elements
    for that */
void canvas_redrawallfortemplatecanvas(t_canvas *x, int action)
{
    t_gobj *g;
    t_template *tmpl;
    t_symbol *s1 = gensym("struct");
    for (g = x->gl_list; g; g = g->g_next)
    {
        t_object *ob = pd_checkobject(&g->g_pd);
        t_atom *argv;
        if (!ob || ob->te_type != T_OBJECT ||
            binbuf_getnatom(ob->te_binbuf) < 2)
            continue;
        argv = binbuf_getvec(ob->te_binbuf);
        if (argv[0].a_type != A_SYMBOL || argv[1].a_type != A_SYMBOL
            || argv[0].a_w.w_symbol != s1)
                continue;
        tmpl = template_findbyname(argv[1].a_w.w_symbol);
        canvas_redrawallfortemplate(tmpl, action);
    }
    canvas_redrawallfortemplate(0, action);
}

/* ------------------------------- declare ------------------------ */

/* put "declare" objects in a patch to tell it about the environment in
which objects should be created in this canvas.  This includes directories to
search ("-path", "-stdpath") and object libraries to load
("-lib" and "-stdlib").  These must be set before the patch containing
the "declare" object is filled in with its contents; so when the patch is
saved,  we throw early messages to the canvas to set the environment
before any objects are created in it. */

static t_class *declare_class;

typedef struct _declare
{
    t_object x_obj;
    t_canvas *x_canvas;
    int x_useme;
} t_declare;

static void *declare_new(t_symbol *s, int argc, t_atom *argv)
{
    t_declare *x = (t_declare *)pd_new(declare_class);
    x->x_useme = 1;
    x->x_canvas = canvas_getcurrent();
        /* LATER update environment and/or load libraries */
    if (!x->x_canvas->gl_loading)
    {
        /* the object is created by the user (not by loading a patch),
         * so update canvas's properties on the fly */
        canvas_declare(x->x_canvas, s, argc, argv);
    }
    return (x);
}

static void declare_free(t_declare *x)
{
    x->x_useme = 0;
        /* LATER update environment */
}

void canvas_savedeclarationsto(t_canvas *x, t_binbuf *b)
{
    t_gobj *y;

    for (y = x->gl_list; y; y = y->g_next)
    {
        if (pd_class(&y->g_pd) == declare_class)
        {
            binbuf_addv(b, "s", gensym("#X"));
            binbuf_addbinbuf(b, ((t_declare *)y)->x_obj.te_binbuf);
            binbuf_addv(b, ";");
        }
            /* before 0.47 we also allowed abstractions to write out to the
            parent's declarations; now we only allow non-abstraction subpatches
            to do so. */
        else if (pd_checkglist(&y->g_pd) &&
            (pd_compatibilitylevel < 47 || !canvas_isabstraction((t_canvas *)y)))
                canvas_savedeclarationsto((t_canvas *)y, b);
    }
}

static void canvas_completepath(const char *from, char *to, int bufsize,
    t_canvas *x)
{
    if (sys_isabsolutepath(from))
    {
        to[0] = '\0';
    }
    else if (x)
    {
        /* append canvas dir */
        const char *dir = canvas_getdir(x)->s_name;
        int dirlen = (int)strlen(dir);
        strncpy(to, dir, bufsize-dirlen);
        to[bufsize-dirlen-1] = '\0';
        strcat(to, "/");
    }
     else
    {   /* append Pd lib dir */
        strncpy(to, sys_libdir->s_name, bufsize-10);
        to[bufsize-9] = '\0';
        strcat(to, "/extra/");
    }
    strncat(to, from, bufsize-strlen(to));
    to[bufsize-1] = '\0';
}

/* maybe we should rename check_exists() to sys_access() and move it to s_path */
#ifdef _WIN32
static int check_exists(const char*path)
{
    char pathbuf[MAXPDSTRING];
    wchar_t ucs2path[MAXPDSTRING];
    sys_bashfilename(path, pathbuf);
    u8_utf8toucs2(ucs2path, MAXPDSTRING, pathbuf, MAXPDSTRING-1);
    return (0 ==  _waccess(ucs2path, 0));
}
#else
#include <unistd.h>
static int check_exists(const char*path)
{
    char pathbuf[MAXPDSTRING];
    sys_bashfilename(path, pathbuf);
    return (0 == access(pathbuf, 0));
}
#endif

static void canvas_path(t_canvas *x, t_canvasenvironment *e, const char *path)
{
    t_namelist *nl;
    char strbuf[MAXPDSTRING];
    if (sys_isabsolutepath(path))
    {
        e->ce_path = namelist_append(e->ce_path, path, 0);
        return;
    }

        /* explicit relative path, starts with ./ or ../ */
    if ((strncmp("./", path, 2) == 0) || (strncmp("../", path, 3) == 0))
    {
        e->ce_path = namelist_append(e->ce_path, path, 0);
        return;
    }

        /* check if path is a subdir of the canvas-path */
    canvas_completepath(path, strbuf, MAXPDSTRING, x);
    if (check_exists(strbuf))
    {
        e->ce_path = namelist_append(e->ce_path, path, 0);
        return;
    }

        /* check whether the given subdir is in one of the user search-paths */
    for (nl=STUFF->st_searchpath; nl; nl=nl->nl_next)
    {
        snprintf(strbuf, MAXPDSTRING-1, "%s/%s/", nl->nl_string, path);
        strbuf[MAXPDSTRING-1]=0;
        if (check_exists(strbuf))
        {
            e->ce_path = namelist_append(e->ce_path, strbuf, 0);
            return;
        }
    }

        /* check whether the given subdir is in one of the standard-paths */
    for (nl=STUFF->st_staticpath; nl; nl=nl->nl_next)
    {
        snprintf(strbuf, MAXPDSTRING-1, "%s/%s/", nl->nl_string, path);
        strbuf[MAXPDSTRING-1]=0;
        if (check_exists(strbuf))
        {
            e->ce_path = namelist_append(e->ce_path, strbuf, 0);
            return;
        }
    }
}
static void canvas_lib(t_canvas *x, t_canvasenvironment *e, const char *lib)
{
    t_namelist *nl;
    char strbuf[MAXPDSTRING];
    if (sys_isabsolutepath(lib))
    {
        sys_load_lib(x, lib);
        return;
    }

        /* explicit relative path, starts with ./ or ../ */
    if ((strncmp("./", lib, 2) == 0) || (strncmp("../", lib, 3) == 0))
    {
        sys_load_lib(x, lib);
        return;
    }

        /* prefix canvas-path */
    canvas_completepath(lib, strbuf, MAXPDSTRING, x);
    if (sys_load_lib(x, lib))
        return;

    /* check whether the given lib is located in one of the user search-paths */
    for (nl=STUFF->st_searchpath; nl; nl=nl->nl_next)
    {
        snprintf(strbuf, MAXPDSTRING-1, "%s/%s", nl->nl_string, lib);
        strbuf[MAXPDSTRING-1]=0;
        if (sys_load_lib(x, strbuf))
            return;
    }
}

static void canvas_stdpath(t_canvasenvironment *e, const char *stdpath)
{
    t_namelist *nl;
    char strbuf[MAXPDSTRING];
    if (sys_isabsolutepath(stdpath))
    {
        e->ce_path = namelist_append(e->ce_path, stdpath, 0);
        return;
    }

        /* strip "extra/"-prefix */
    if (!strncmp("extra/", stdpath, 6))
        stdpath+=6;

    /* prefix full pd-path (including extra) */
    canvas_completepath(stdpath, strbuf, MAXPDSTRING, 0);
    if (check_exists(strbuf))
    {
        e->ce_path = namelist_append(e->ce_path, strbuf, 0);
        return;
    }

    /* check whether the given subdir is in one of the standard-paths */
    for (nl=STUFF->st_staticpath; nl; nl=nl->nl_next)
    {
        snprintf(strbuf, MAXPDSTRING-1, "%s/%s/", nl->nl_string, stdpath);
        strbuf[MAXPDSTRING-1]=0;
        if (check_exists(strbuf))
        {
            e->ce_path = namelist_append(e->ce_path, strbuf, 0);
            return;
        }
    }
}
static void canvas_stdlib(t_canvasenvironment *e, const char *stdlib)
{
    t_namelist *nl;
    char strbuf[MAXPDSTRING];
    if (sys_isabsolutepath(stdlib))
    {
        sys_load_lib(0, stdlib);
        return;
    }

        /* strip    "extra/"-prefix */
    if (!strncmp("extra/", stdlib, 6))
        stdlib+=6;

        /* prefix full pd-path (including extra) */
    canvas_completepath(stdlib, strbuf, MAXPDSTRING, 0);
    if (sys_load_lib(0, strbuf))
        return;

    /* check whether the given lib is located in one of the standard-paths */
    for (nl=STUFF->st_staticpath; nl; nl=nl->nl_next)
    {
        snprintf(strbuf, MAXPDSTRING-1, "%s/%s", nl->nl_string, stdlib);
        strbuf[MAXPDSTRING-1]=0;
        if (sys_load_lib(0, strbuf))
            return;
    }
}


void canvas_declare(t_canvas *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    t_canvasenvironment *e = canvas_getenv(x);
#if 0
    startpost("declare:: %s", s->s_name);
    postatom(argc, argv);
    endpost();
#endif
    for (i = 0; i < argc; i++)
    {
        const char *flag = atom_getsymbolarg(i, argc, argv)->s_name;
        const char *item = (argc > i+1)?atom_getsymbolarg(i+1, argc, argv)->s_name:0;
        if ((item) && !strcmp(flag, "-path"))
        {
            canvas_path(x, e, item);
            i++;
        }
        else if ((item) && !strcmp(flag, "-stdpath"))
        {
            canvas_stdpath(e, item);
            i++;
        }
        else if ((item) && !strcmp(flag, "-lib"))
        {
            canvas_lib(x, e, item);
            i++;
        }
        else if ((item) && !strcmp(flag, "-stdlib"))
        {
            canvas_stdlib(e, item);
            i++;
        }
        else post("declare: %s: unknown declaration", flag);
    }
}

typedef struct _canvasopen
{
    const char *name;
    const char *ext;
    char *dirresult;
    char **nameresult;
    unsigned int size;
    int bin;
    int fd;
} t_canvasopen;

static int canvas_open_iter(const char *path, t_canvasopen *co)
{
    int fd;
    if ((fd = sys_trytoopenone(path, co->name, co->ext,
        co->dirresult, co->nameresult, co->size, co->bin)) >= 0)
    {
        co->fd = fd;
        return 0;
    }
    return 1;
}

    /* utility function to read a file, looking first down the canvas's search
    path (set with "declare" objects in the patch and recursively in calling
    patches), then down the system one.  The filename is the concatenation of
    "name" and "ext".  "Name" may be absolute, or may be relative with
    slashes.  If anything can be opened, the true directory
    ais put in the buffer dirresult (provided by caller), which should
    be "size" bytes.  The "nameresult" pointer will be set somewhere in
    the interior of "dirresult" and will give the file basename (with
    slashes trimmed).  If "bin" is set a 'binary' open is
    attempted, otherwise ASCII (this only matters on Microsoft.)
    If "x" is zero, the file is sought in the directory "." or in the
    global path.*/
int canvas_open(const t_canvas *x, const char *name, const char *ext,
    char *dirresult, char **nameresult, unsigned int size, int bin)
{
    int fd = -1;
    t_canvasopen co;

        /* first check if "name" is absolute (and if so, try to open) */
    if (sys_open_absolute(name, ext, dirresult, nameresult, size, bin, &fd))
        return (fd);

        /* otherwise "name" is relative; iterate over all the search-paths */
    co.name = name;
    co.ext = ext;
    co.dirresult = dirresult;
    co.nameresult = nameresult;
    co.size = size;
    co.bin = bin;
    co.fd = -1;

    canvas_path_iterate(x, (t_canvas_path_iterator)canvas_open_iter, &co);

    return (co.fd);
}

/*
 * Iterate over all search-paths for <x> calling <fun> with the user-supplied
 * <data>.  The function is called with two arguments: a pathname to try to
 * open, and <data>.
 */
int canvas_path_iterate(const t_canvas *x, t_canvas_path_iterator fun,
    void *user_data)
{
    const t_canvas *y = 0;
    t_namelist *nl = 0;
    int count = 0;
    if (!fun)
        return 0;
        /* iterate through canvas-local paths */
    for (y = x; y; y = y->gl_owner)
        if (y->gl_env)
    {
        const char *dir;
        dir = canvas_getdir(y)->s_name;
        for (nl = y->gl_env->ce_path; nl; nl = nl->nl_next)
        {
            char realname[MAXPDSTRING];
            if (sys_isabsolutepath(nl->nl_string))
                realname[0] = '\0';
            else
            {   /* if not absolute path, append Pd lib dir */
                strncpy(realname, dir, MAXPDSTRING);
                realname[MAXPDSTRING-3] = 0;
                strcat(realname, "/");
            }
            strncat(realname, nl->nl_string, MAXPDSTRING-strlen(realname));
            realname[MAXPDSTRING-1] = 0;
            if (!fun(realname, user_data))
                return count+1;
            count++;
        }
    }
    /* try canvas dir */
    if (!fun((x ? canvas_getdir(x)->s_name : "."), user_data))
        return count+1;
    count++;

    /* now iterate through the global paths */
    for (nl = STUFF->st_searchpath; nl; nl = nl->nl_next)
    {
        if (!fun(nl->nl_string, user_data))
            return count+1;
        count++;
    }
    /* and the temp paths from the commandline */
    for (nl = STUFF->st_temppath; nl; nl = nl->nl_next)
    {
        if (!fun(nl->nl_string, user_data))
            return count+1;
        count++;
    }
    /* and the default paths */
    if (sys_usestdpath)
        for (nl = STUFF->st_staticpath; nl; nl = nl->nl_next)
        {
            if (!fun(nl->nl_string, user_data))
                return count+1;
            count++;
        }

    return count;
}

static void canvas_f(t_canvas *x, t_symbol *s, int argc, t_atom *argv)
{
    static int warned;
    t_gobj *g, *g2;
    t_object *ob;
    if (argc > 1 && !warned)
    {
        post("** ignoring width or font settings from future Pd version **");
        warned = 1;
    }
    if (!x->gl_list)
        return;
    for (g = x->gl_list; (g2 = g->g_next); g = g2)
        ;
    if ((ob = pd_checkobject(&g->g_pd)))
    {
        ob->te_width = atom_getfloatarg(0, argc, argv);
        if (glist_isvisible(x))
        {
            gobj_vis(g, x, 0);
            gobj_vis(g, x, 1);
        }
    }
}

extern t_class *array_define_class;     /* LATER datum class too */

    /* check if a pd can be treated as a glist - true if we're of any of
    the glist classes, which all have 'glist' as the first item in struct */
t_glist *pd_checkglist(t_pd *x)
{
    if (*x == canvas_class || *x == array_define_class)
        return ((t_canvas *)x);
    else return (0);
}

/* ------------------------------- setup routine ------------------------ */

    /* why are some of these "glist" and others "canvas"? */
extern void glist_text(t_glist *x, t_symbol *s, int argc, t_atom *argv);
extern void canvas_obj(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_bng(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_toggle(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_vslider(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_hslider(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_vdial(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
    /* old version... */
extern void canvas_hdial(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_hdial(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
    /* new version: */
extern void canvas_hradio(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_vradio(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_vumeter(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_mycnv(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_numbox(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_msg(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_floatatom(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_listbox(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void canvas_symbolatom(t_glist *gl, t_symbol *s, int argc, t_atom *argv);
extern void glist_scalar(t_glist *canvas, t_symbol *s, int argc, t_atom *argv);

void g_graph_setup(void);
void g_editor_setup(void);
void g_readwrite_setup(void);
extern void canvas_properties(t_gobj *z, t_glist *canvas);

void g_canvas_setup(void)
{
        /* we prevent the user from typing "canvas" in an object box
        by sending 0 for a creator function. */
    canvas_class = class_new(gensym("canvas"), 0,
        (t_method)canvas_free, sizeof(t_canvas), CLASS_NOINLET, 0);
            /* here is the real creator function, invoked in patch files
            by sending the "canvas" message to #N, which is bound
            to pd_camvasmaker. */
    class_addmethod(pd_canvasmaker, (t_method)canvas_new, gensym("canvas"),
        A_GIMME, 0);
    class_addmethod(canvas_class, (t_method)canvas_restore,
        gensym("restore"), A_GIMME, 0);
    class_addmethod(canvas_class, (t_method)canvas_coords,
        gensym("coords"), A_GIMME, 0);

/* -------------------------- objects ----------------------------- */
    class_addmethod(canvas_class, (t_method)canvas_obj,
        gensym("obj"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_msg,
        gensym("msg"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_floatatom,
        gensym("floatatom"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_listbox,
        gensym("listbox"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_symbolatom,
        gensym("symbolatom"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)glist_text,
        gensym("text"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)glist_glist, gensym("graph"),
        A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)glist_scalar,
        gensym("scalar"), A_GIMME, A_NULL);

/* -------------- connect method used in reading files ------------------ */
    class_addmethod(canvas_class, (t_method)canvas_connect,
        gensym("connect"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);


/* -------------- IEMGUI: button, toggle, slider, etc.  ------------ */
    class_addmethod(canvas_class, (t_method)canvas_bng, gensym("bng"),
                    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_toggle, gensym("toggle"),
                    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_vslider, gensym("vslider"),
                    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_hslider, gensym("hslider"),
                    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_hdial, gensym("hdial"),
                    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_vdial, gensym("vdial"),
                    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_hradio, gensym("hradio"),
                    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_vradio, gensym("vradio"),
                    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_vumeter, gensym("vumeter"),
                    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_mycnv, gensym("mycnv"),
                    A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_numbox, gensym("numbox"),
                    A_GIMME, A_NULL);

/* ------------------------ gui stuff --------------------------- */
    class_addmethod(canvas_class, (t_method)canvas_pop, gensym("pop"),
        A_DEFFLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_loadbang,
        gensym("loadbang"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_setbounds,
        gensym("setbounds"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_relocate,
        gensym("relocate"), A_SYMBOL, A_SYMBOL, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_vis,
        gensym("vis"), A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)glist_menu_open,
        gensym("menu-open"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_map,
        gensym("map"), A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_dirty,
        gensym("dirty"), A_FLOAT, A_NULL);
    class_setpropertiesfn(canvas_class, canvas_properties);

/* ---------------------- list handling ------------------------ */
    class_addmethod(canvas_class, (t_method)glist_clear, gensym("clear"),
        A_NULL);

/* ----- subcanvases, which you get by typing "pd" in a box ---- */
    class_addcreator((t_newmethod)subcanvas_new, gensym("pd"), A_DEFSYMBOL, 0);
    class_addcreator((t_newmethod)subcanvas_new, gensym("page"),  A_DEFSYMBOL, 0);

    class_addmethod(canvas_class, (t_method)canvas_click,
        gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(canvas_class, (t_method)canvas_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(canvas_class, (t_method)canvas_rename_method,
        gensym("rename"), A_GIMME, 0);

/*---------------------------- declare ------------------- */
    declare_class = class_new(gensym("declare"), (t_newmethod)declare_new,
        (t_method)declare_free, sizeof(t_declare), CLASS_NOINLET, A_GIMME, 0);
    class_addmethod(canvas_class, (t_method)canvas_declare,
        gensym("declare"), A_GIMME, 0);

/*--------------- future message to set formatting  -------------- */
    class_addmethod(canvas_class, (t_method)canvas_f,
        gensym("f"), A_GIMME, 0);
/* -------------- setups from other files for canvas_class ---------------- */
    g_graph_setup();
    g_editor_setup();
    g_readwrite_setup();
}

    /* functions to add basic gui (e.g., clicking but not editing) to things
    based on canvases that aren't editable, like "array define" object */
void canvas_editor_for_class(t_class *c);
void g_graph_setup_class(t_class *c);
void canvas_readwrite_for_class(t_class *c);

void canvas_add_for_class(t_class *c)
{
    class_addmethod(c, (t_method)canvas_restore,
        gensym("restore"), A_GIMME, 0);
    class_addmethod(c, (t_method)canvas_click,
        gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(c, (t_method)canvas_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(c, (t_method)canvas_map,
        gensym("map"), A_FLOAT, A_NULL);
    class_addmethod(c, (t_method)canvas_setbounds,
        gensym("setbounds"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    canvas_editor_for_class(c);
    canvas_readwrite_for_class(c);
    /* g_graph_setup_class(c); */
}

void g_canvas_newpdinstance(void)
{
    THISGUI = getbytes(sizeof(*THISGUI));
    THISGUI->i_newfilename = THISGUI->i_newdirectory = &s_;
    THISGUI->i_newargc = 0;
    THISGUI->i_newargv = 0;
    THISGUI->i_reloadingabstraction = 0;
    THISGUI->i_dspstate = 0;
    THISGUI->i_dollarzero = 1000;
    g_editor_newpdinstance();
    g_template_newpdinstance();
}

void g_canvas_freepdinstance(void)
{
    g_editor_freepdinstance();
    g_template_freepdinstance();
    freebytes(THISGUI, sizeof(*THISGUI));
}

EXTERN int pd_getdspstate(void)
{
    return (THISGUI->i_dspstate);
}

void pd_doloadbang(void);

    /* evaluate a file, which is expected to create a patch, and perform
    post-evaluation cleanup and loadbang */
t_pd *glob_evalfile(t_pd *ignore, t_symbol *name, t_symbol *dir)
{
    t_pd *x = 0, *boundx;
    int dspstate;

        /* even though binbuf_evalfile appears to take care of dspstate,
        we have to do it again here, because canvas_startdsp() assumes
        that all toplevel canvases are visible.  LATER check if this
        is still necessary -- probably not. */
    dspstate = canvas_suspend_dsp();
    boundx = s__X.s_thing;
        s__X.s_thing = 0;       /* don't save #X; we'll need to leave it bound
                                for the caller to grab it. */
    binbuf_evalfile(name, dir);
    while ((x != s__X.s_thing) && s__X.s_thing)
    {
        x = s__X.s_thing;
        vmess(x, gensym("pop"), "i", 1);
    }
    if (!sys_noloadbang)
        pd_doloadbang();
    canvas_resume_dsp(dspstate);
    s__X.s_thing = boundx;
    return x;
}

    /* open a file as if from an open dialog from the GUI.  If the optional
    argument "f" is nonzero, first check if the file is already open and if
    so, just "vis" it.  This would be useful if you want merely to make sure a
    patch is open, but don't want more than one copy. */
void glob_open(t_pd *ignore, t_symbol *name, t_symbol *dir, t_floatarg f)
{
    t_glist *gl;
    if (f != 0)
        for (gl = pd_getcanvaslist(); gl; gl = gl->gl_next)
            if (name == gl->gl_name && gl->gl_env && gl->gl_env->ce_dir == dir)
    {
            /* don't reopen already-open document, just vis it */
        canvas_vis(gl, 1);
        return;
    }
    if (!glob_evalfile(ignore, name, dir))
        pdgui_vmess("::pdwindow::busyrelease", 0);
}
