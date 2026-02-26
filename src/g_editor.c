/* Copyright (c) 1997-2001 Miller Puckette and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include <stdio.h>
#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include "g_canvas.h"
#include "g_undo.h"
#include "s_utf8.h" /*-- moo --*/
#include <string.h>
#include "m_private_utils.h"


struct _instanceeditor
{
    t_binbuf *copy_binbuf;
    char *canvas_textcopybuf;
    int canvas_textcopybufsize;
    t_undofn canvas_undo_fn;         /* current undo function if any */
    int canvas_undo_whatnext;        /* whether we can now UNDO or REDO */
    void *canvas_undo_buf;           /* data private to the undo function */
    t_canvas *canvas_undo_canvas;    /* which canvas we can undo on */
    const char *canvas_undo_name;
    int canvas_undo_already_set_move;
    double canvas_upclicktime;
    int canvas_upx, canvas_upy;
    int canvas_find_index, canvas_find_wholeword;
    t_binbuf *canvas_findbuf;
    int paste_onset;
    t_canvas *paste_canvas;
    t_glist *canvas_last_glist;
    int canvas_last_glist_x, canvas_last_glist_y;
    t_canvas *canvas_cursorcanvaswas;
    unsigned int canvas_cursorwas;
};

    /* pointer to "globals" used in this Pd instance */
#define THISED (pd_this->pd_gui->i_editor)

/* positional offset for duplicated items */
#define PASTE_OFFSET 10

void glist_readfrombinbuf(t_glist *x, const t_binbuf *b, const char *filename,
    int selectem);

/* ------------------ forward declarations --------------- */
static void canvas_doclear(t_canvas *x);
static void glist_setlastxy(t_glist *gl, int xval, int yval);
static void glist_donewloadbangs(t_glist *x);
static t_binbuf *canvas_docopy(t_canvas *x);
static void canvas_dopaste(t_canvas *x, t_binbuf *b);
static void canvas_paste(t_canvas *x);
static void canvas_clearline(t_canvas *x);
static t_glist *glist_finddirty(t_glist *x);
static void canvas_zoom(t_canvas *x, t_floatarg zoom);
static void canvas_displaceselection(t_canvas *x, int dx, int dy);
void canvas_setgraph(t_glist *x, int flag, int nogoprect);

/* ------------------------ managing the selection ----------------- */
void glist_deselectline(t_glist *x);

static void _editor_selectlinecolor(t_glist *x, unsigned int color)
{
    char tag[128];
    sprintf(tag, "l%p", x->gl_editor->e_selectline_tag);
    pdgui_vmess(0, "crs rk",
        x, "itemconfigure", tag,
        "-fill", color);

}
void glist_selectline(t_glist *x, t_outconnect *oc, int index1,
    int outno, int index2, int inno)
{
    if (x->gl_editor)
    {
        glist_deselectline(x);

        x->gl_editor->e_selectedline = 1;
        x->gl_editor->e_selectline_index1 = index1;
        x->gl_editor->e_selectline_outno = outno;
        x->gl_editor->e_selectline_index2 = index2;
        x->gl_editor->e_selectline_inno = inno;
        x->gl_editor->e_selectline_tag = oc;
        _editor_selectlinecolor(x, THISGUI->i_selectcolor);
    }
}

void glist_deselectline(t_glist *x)
{
    if (x->gl_editor)
    {
        x->gl_editor->e_selectedline = 0;
        _editor_selectlinecolor(x, THISGUI->i_foregroundcolor);
    }
}

int glist_isselected(t_glist *x, t_gobj *y)
{
    if (x->gl_editor)
    {
        t_selection *sel;
        for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
            if (sel->sel_what == y) return (1);
    }
    return (0);
}

    /* call this for unselected objects only */
void glist_select(t_glist *x, t_gobj *y)
{
    if (x->gl_editor)
    {
        t_selection *sel = (t_selection *)getbytes(sizeof(*sel));
            /* LATER #ifdef out the following check */
        if (glist_isselected(x, y)) bug("glist_select");
        sel->sel_next = x->gl_editor->e_selection;
        sel->sel_what = y;
        x->gl_editor->e_selection = sel;
        gobj_select(y, x, 1);
    }
}

    /* recursively deselect everything in a gobj "g", if it happens to be
       a glist, in preparation for deselecting g itself in glist_dselect() */
static void glist_checkanddeselectall(t_glist *gl, t_gobj *g)
{
    t_glist *gl2;
    t_gobj *g2;
    if (pd_class(&g->g_pd) != canvas_class)
        return;
    gl2 = (t_glist *)g;
    for (g2 = gl2->gl_list; g2; g2 = g2->g_next)
        glist_checkanddeselectall(gl2, g2);
    glist_noselect(gl2);
}

    /* call this for selected objects only */
void glist_deselect(t_glist *x, t_gobj *y)
{
    int fixdsp = 0;
    t_selection *sel, *sel2;
    t_rtext *z = 0;

    if (!x->gl_editor)
        return;

    if (!glist_isselected(x, y)) bug("glist_deselect");
    if (glist_textedfor(x))
    {
        t_rtext *fuddy = glist_getrtext(x, (t_text *)y, 0);
        if (fuddy && glist_textedfor(x) == fuddy)
        {
            if (x->gl_editor->e_textdirty)
            {
                z = fuddy;
                canvas_undo_add(x, UNDO_SEQUENCE_START, "typing", 0);
                canvas_undo_add(x, UNDO_ARRANGE, "arrange",
                    canvas_undo_set_arrange(x, y, 1));
                canvas_stowconnections(glist_getcanvas(x));
                glist_checkanddeselectall(x, y);
            }
            gobj_activate(y, x, 0);
        }
        if (zgetfn(&y->g_pd, gensym("dsp")))
            fixdsp = canvas_suspend_dsp();
    }
    if ((sel = x->gl_editor->e_selection)->sel_what == y)
    {
        x->gl_editor->e_selection = x->gl_editor->e_selection->sel_next;
        gobj_select(sel->sel_what, x, 0);
        freebytes(sel, sizeof(*sel));
    }
    else
    {
        for (sel = x->gl_editor->e_selection; (sel2 = sel->sel_next);
             sel = sel2)
        {
            if (sel2->sel_what == y)
            {
                sel->sel_next = sel2->sel_next;
                gobj_select(sel2->sel_what, x, 0);
                freebytes(sel2, sizeof(*sel2));
                break;
            }
        }
    }
    if (z)
    {
        char *buf;
        int bufsize;

        rtext_gettext(z, &buf, &bufsize);
        text_setto((t_text *)y, x, buf, bufsize);
        canvas_fixlinesfor(x, (t_text *)y);
        glist_settexted(x, 0);
        canvas_undo_add(x, UNDO_SEQUENCE_END, "typing", 0);
    }
    if (fixdsp)
        canvas_resume_dsp(1);
}

void glist_noselect(t_glist *x)
{
    if (x->gl_editor)
    {
        while (x->gl_editor->e_selection)
            glist_deselect(x, x->gl_editor->e_selection->sel_what);
        if (x->gl_editor->e_selectedline)
            glist_deselectline(x);
    }
}

void glist_selectall(t_glist *x)
{
    if (x->gl_editor)
    {
        glist_noselect(x);
        if (x->gl_list)
        {
            t_selection *sel = (t_selection *)getbytes(sizeof(*sel));
            t_gobj *y = x->gl_list;
            x->gl_editor->e_selection = sel;
            sel->sel_what = y;
            gobj_select(y, x, 1);
            while ((y = y->g_next))
            {
                t_selection *sel2 = (t_selection *)getbytes(sizeof(*sel2));
                sel->sel_next = sel2;
                sel = sel2;
                sel->sel_what = y;
                gobj_select(y, x, 1);
            }
            sel->sel_next = 0;
        }
    }
}

    /* get the index of a gobj in a glist.  If y is zero, return the
       total number of objects. */
int glist_getindex(t_glist *x, t_gobj *y)
{
    t_gobj *y2;
    int indx;

    for (y2 = x->gl_list, indx = 0; y2 && y2 != y; y2 = y2->g_next)
        indx++;
    return (indx);
}

    /* get the index of the object, among selected items, if "selected"
       is set; otherwise, among unselected ones.  If y is zero, just
       counts the selected or unselected objects. */
int glist_selectionindex(t_glist *x, t_gobj *y, int selected)
{
    t_gobj *y2;
    int indx;

    for (y2 = x->gl_list, indx = 0; y2 && y2 != y; y2 = y2->g_next)
        if (selected == glist_isselected(x, y2))
            indx++;
    return (indx);
}

static t_gobj *glist_nth(t_glist *x, int n)
{
    t_gobj *y;
    int indx;
    for (y = x->gl_list, indx = 0; y; y = y->g_next, indx++)
        if (indx == n)
            return (y);
    return (0);
}

t_rtext *glist_textedfor(t_glist *gl)
{
    t_canvas *canvas = glist_getcanvas(gl);
    if (canvas->gl_editor)
        return (canvas->gl_editor->e_textedfor);
    else return (0);
}

void glist_settexted(t_glist *gl, t_rtext *x)
{
    while (gl->gl_owner && !gl->gl_isclone && !gl->gl_havewindow &&
        gl->gl_isgraph)
    {
        gl->gl_editor->e_textedfor = 0;
        gl = gl->gl_owner;
    }
    if (gl->gl_editor)
        gl->gl_editor->e_textedfor = x;
    else bug("glist_settexted");
}

/* ------------------- support for undo/redo  -------------------------- */

static void canvas_applybinbuf(t_canvas *x, t_binbuf *b)
{
    t_symbol*asym = gensym("#A");
    t_pd *boundx = s__X.s_thing,
        *bounda = asym->s_thing,
        *boundn = s__N.s_thing;

    asym->s_thing = 0;
    s__X.s_thing = &x->gl_pd;
    s__N.s_thing = &pd_canvasmaker;

    binbuf_eval(b, 0, 0, 0);

    asym->s_thing = bounda;
    s__X.s_thing = boundx;
    s__N.s_thing = boundn;
}


static int canvas_undo_confirmdiscard(t_gobj *g)
{
    t_glist *gl2;

    if (pd_class(&g->g_pd) == canvas_class &&
        canvas_isabstraction((t_glist *)g) &&
        (gl2 = glist_finddirty((t_glist *)g)))
    {
        t_canvas*c = canvas_getrootfor(gl2);
        const char *msg[]= {"Discard changes to '%s'?", c->gl_name->s_name};
        char buf[80];
        t_atom backmsg[2];
        sprintf(buf, ".x%lx", gl2);
        SETSYMBOL(backmsg+0, gensym("dirty"));
        SETFLOAT (backmsg+1, 0);
        vmess(&gl2->gl_pd, gensym("menu-open"), "");
        pdgui_vmess("pdtk_check", "^ Sms",
            c,
            2, msg,
            gensym(buf), 2, backmsg,
            "no");

        return 1;
    }
    return 0;
}

void canvas_undo_set_name(const char*name)
{
    THISED->canvas_undo_name = name;
}

void canvas_setundo(t_canvas *x, t_undofn undofn, void *buf,
    const char *name)
{
    int hadone = 0;
        /* blow away the old undo information.  In one special case the
           old undo info is re-used; if so we shouldn't free it here. */
    if (THISED->canvas_undo_fn && THISED->canvas_undo_buf && (buf != THISED->canvas_undo_buf))
    {
        (*THISED->canvas_undo_fn)(THISED->canvas_undo_canvas, THISED->canvas_undo_buf, UNDO_FREE);
        hadone = 1;
    }
    THISED->canvas_undo_canvas = x;
    THISED->canvas_undo_fn = undofn;
    THISED->canvas_undo_buf = buf;
    THISED->canvas_undo_whatnext = UNDO_UNDO;
    THISED->canvas_undo_name = name;
    if (x && glist_isvisible(x) && glist_istoplevel(x))
            /* enable undo in menu */
        pdgui_vmess("pdtk_undomenu", "^ss", x, name, "no");
    else if (hadone)
        pdgui_vmess("pdtk_undomenu", "rss", "nobody", "no", "no");
}

    /* clear undo if it happens to be for the canvas x.
       (but if x is 0, clear it regardless of who owns it.) */
void canvas_noundo(t_canvas *x)
{
    if (!x || (x == THISED->canvas_undo_canvas))
        canvas_setundo(0, 0, 0, "foo");
}

    /* user hits "undo" in GUI */
static void canvas_undomenu(t_canvas *x)
{
        /* first check if there is run-time typing, which we can back
        out of by merely calling rtext_untype().  Prior to 0.56 this
        happened when the user clicked outside an atom box - that
        action now adopts the new text.  If the canvas is in run mode
        the user might have control-clicked an atom box or scalar text
        field to change it; it's not clear what "undo" should mean in
        this case, since typing into a number box is not undo-able in
        run mode - it's considered a performance input.  But then, what
        about run-mode editing of numeric fields in scalars?  Shouldn't
        that be undo-able, even though you go into run mode (or use ctrl
        key) to do it?  This needs to be reconsidered as the scalar
        editing features evolve. */
    if (!x->gl_edit && x->gl_editor->e_textedfor &&
        x->gl_editor->e_textdirty)
            rtext_untype(x->gl_editor->e_textedfor);
    else canvas_undo_undo(x);
}

/* ------- specific undo methods: 1. connect -------- */
typedef struct _undo_connect
{
    int u_index1;
    int u_outletno;
    int u_index2;
    int u_inletno;
} t_undo_connect;

void *canvas_undo_set_disconnect(t_canvas *x,
    int index1, int outno, int index2, int inno);

/* connect just calls disconnect actions backward... (see below) */
void *canvas_undo_set_connect(t_canvas *x,
    int index1, int outno, int index2, int inno)
{
    return (canvas_undo_set_disconnect(x, index1, outno, index2, inno));
}

int canvas_undo_connect(t_canvas *x, void *z, int action)
{
    int myaction;
    if (action == UNDO_UNDO)
        myaction = UNDO_REDO;
    else if (action == UNDO_REDO)
        myaction = UNDO_UNDO;
    else myaction = action;
    canvas_undo_disconnect(x, z, myaction);
    return 1;
}

static void canvas_connect_with_undo(t_canvas *x,
    t_float index1, t_float outno, t_float index2, t_float inno)
{
    canvas_connect(x, index1, outno, index2, inno);
    canvas_undo_add(x, UNDO_CONNECT, "connect", canvas_undo_set_connect(x,
        index1, outno, index2, inno));
}

/* ------- specific undo methods: 2. disconnect -------- */

void *canvas_undo_set_disconnect(t_canvas *x,
    int index1, int outno, int index2, int inno)
{
    t_undo_connect *buf = (t_undo_connect *)getbytes(sizeof(*buf));
    buf->u_index1 = index1;
    buf->u_outletno = outno;
    buf->u_index2 = index2;
    buf->u_inletno = inno;
    return (buf);
}

void canvas_disconnect(t_canvas *x,
    t_float index1, t_float outno, t_float index2, t_float inno)
{
    t_linetraverser t;
    t_outconnect *oc;
    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
    {
        int srcno = canvas_getindex(x, &t.tr_ob->ob_g);
        int sinkno = canvas_getindex(x, &t.tr_ob2->ob_g);
        if (srcno == index1 && t.tr_outno == outno &&
            sinkno == index2 && t.tr_inno == inno)
        {
            if (glist_isvisible(x) && x->gl_havewindow)
            {
                char tag[128];
                sprintf(tag, "l%p", oc);
                pdgui_vmess(0, "rcr", "pdtk_canvas_delete", x, tag);
            }
            obj_disconnect(t.tr_ob, t.tr_outno, t.tr_ob2, t.tr_inno);
            break;
        }
    }
}

int canvas_undo_disconnect(t_canvas *x, void *z, int action)
{
    t_undo_connect *buf = z;
    if (action == UNDO_UNDO)
    {
        canvas_connect(x, buf->u_index1, buf->u_outletno,
            buf->u_index2, buf->u_inletno);
    }
    else if (action == UNDO_REDO)
    {
        canvas_disconnect(x, buf->u_index1, buf->u_outletno,
            buf->u_index2, buf->u_inletno);
    }
    else if (action == UNDO_FREE)
        t_freebytes(buf, sizeof(*buf));
    return 1;
}

static void canvas_disconnect_with_undo(t_canvas *x,
    t_float index1, t_float outno, t_float index2, t_float inno)
{
    canvas_disconnect(x, index1, outno, index2, inno);
    canvas_undo_add(x, UNDO_DISCONNECT, "disconnect", canvas_undo_set_disconnect(x,
        index1, outno, index2, inno));
}

/* ---------- ... 3. cut, clear, and typing into objects: -------- */

#define UCUT_CUT 1          /* operation was a cut */
#define UCUT_CLEAR 2        /* .. a clear */

/* following action is not needed any more LATER remove any signs of UCUT_TEXT
 * since recreate takes care of this in a more elegant way
 */
#define UCUT_TEXT 3         /* text typed into a box */

typedef struct _undo_cut
{
    t_binbuf *u_objectbuf;      /* the object cleared or typed into */
    t_binbuf *u_reconnectbuf;   /* connections into and out of object */
    t_binbuf *u_redotextbuf;    /* buffer to paste back for redo if TEXT */
    int u_mode;                 /* from flags above */
    int n_obj;                  /* number of selected objects to be cut */
    int p_a[1];    /* array of original glist positions of selected objects.
                      At least one object is selected, we dynamically resize
                      it later */
} t_undo_cut;

void *canvas_undo_set_cut(t_canvas *x, int mode)
{
    t_undo_cut *buf;
    t_linetraverser t;
    t_outconnect *oc;
    int nnotsel= glist_selectionindex(x, 0, 0);
    int nsel = glist_selectionindex(x, 0, 1);
    buf = (t_undo_cut *)getbytes(sizeof(*buf) +
        sizeof(buf->p_a[0]) * (nsel - 1));
    buf->n_obj = nsel;
    buf->u_mode = mode;
    buf->u_redotextbuf = 0;

        /* store connections into/out of the selection */
    buf->u_reconnectbuf = binbuf_new();
    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
    {
        int issel1 = glist_isselected(x, &t.tr_ob->ob_g);
        int issel2 = glist_isselected(x, &t.tr_ob2->ob_g);
        if (issel1 != issel2)
        {
            binbuf_addv(buf->u_reconnectbuf, "ssiiii;",
                gensym("#X"), gensym("connect"),
                (issel1 ? nnotsel : 0)
                    + glist_selectionindex(x, &t.tr_ob->ob_g, issel1),
                t.tr_outno,
                (issel2 ? nnotsel : 0)
                    + glist_selectionindex(x, &t.tr_ob2->ob_g, issel2),
                t.tr_inno);
        }
    }
    if (mode == UCUT_TEXT)
    {
        buf->u_objectbuf = canvas_docopy(x);
    }
    else if (mode == UCUT_CUT)
    {
        buf->u_objectbuf = canvas_docopy(x);
    }
    else if (mode == UCUT_CLEAR)
    {
        buf->u_objectbuf = canvas_docopy(x);
    }

        /* instantiate num_obj and fill array of positions of selected
        objects */
    if (mode == UCUT_CUT || mode == UCUT_CLEAR)
    {
        if (x->gl_list)
        {
            int i = 0, j = 0;
            t_gobj *y;
            for (y = x->gl_list; y; y = y->g_next)
            {
                if (glist_isselected(x, y))
                {
                    buf->p_a[i] = j;
                    i++;
                }
                j++;
            }
        }
    }

    return (buf);
}

int canvas_undo_cut(t_canvas *x, void *z, int action)
{
    t_undo_cut *buf = z;
    int mode = buf?(buf->u_mode):0;
    if (action == UNDO_UNDO)
    {
        if (mode == UCUT_CUT)
        {
            canvas_dopaste(x, buf->u_objectbuf);
        }
        else if (mode == UCUT_CLEAR)
        {
            canvas_dopaste(x, buf->u_objectbuf);
        }
        else if (mode == UCUT_TEXT)
        {
            t_gobj *y1, *y2;
            glist_noselect(x);
            for (y1 = x->gl_list; (y2 = y1->g_next); y1 = y2)
                ;
            if (y1)
            {
                if (!buf->u_redotextbuf)
                {
                    glist_noselect(x);
                    glist_select(x, y1);
                    buf->u_redotextbuf = canvas_docopy(x);
                    glist_noselect(x);
                }
                glist_delete(x, y1);
            }
            canvas_dopaste(x, buf->u_objectbuf);
        }
        if (buf)
            canvas_applybinbuf(x, buf->u_reconnectbuf);

            /* now reposition objects to their original locations */
        if (mode == UCUT_CUT || mode == UCUT_CLEAR)
        {
            int i = 0;

                /* location of the first newly pasted object */
            int paste_pos = glist_getindex(x,0) - buf->n_obj;
            t_gobj *y_prev, *y, *y_next;
            for (i = 0; i < buf->n_obj; i++)
            {
                    /* first check if we are in the same position already */
                if (paste_pos+i != buf->p_a[i])
                {
                    y_prev = glist_nth(x, paste_pos-1+i);
                    y = glist_nth(x, paste_pos+i);
                    y_next = glist_nth(x, paste_pos+1+i);
                        /* if the object is supposed to be first in the
                        gl_list */
                    if (buf->p_a[i] == 0)
                    {
                        if (y_prev && y_next)
                        {
                            y_prev->g_next = y_next;
                        }
                        else if (y_prev && !y_next)
                            y_prev->g_next = NULL;
                            /* now put the moved object at the beginning of
                            the queue */
                        y->g_next = glist_nth(x, 0);
                        x->gl_list = y;
                            /* LATER when objects are properly tagged lower
                            y here */
                    }
                        /* if the object is supposed to be in the middle of\
                           gl_list */
                    else {
                        if (y_prev && y_next)
                        {
                            y_prev->g_next = y_next;
                        }
                        else if (y_prev && !y_next)
                        {
                            y_prev->g_next = NULL;
                        }
                            /* now put the moved object in its right place */
                        y_prev = glist_nth(x, buf->p_a[i]-1);
                        y_next = glist_nth(x, buf->p_a[i]);

                        y_prev->g_next = y;
                        y->g_next = y_next;
                            /* LATER when objects are properly tagged lower
                                y here */
                    }
                }
            }
                /* LATER disable redrawing here */
            if (x->gl_havewindow)
                canvas_redraw(x);
            if (x->gl_owner && !x->gl_isclone && glist_isvisible(x->gl_owner))
            {
                gobj_vis((t_gobj *)x, x->gl_owner, 0);
                gobj_vis((t_gobj *)x, x->gl_owner, 1);
            }
        }
    }
    else if (action == UNDO_REDO)
    {
        if (mode == UCUT_CUT || mode == UCUT_CLEAR)
        {
            int i;
                /* we can't just blindly do clear here when the user may have
                 * unselected things between undo and redo, so first let's
                 *  select the right stuff
                 */
            glist_noselect(x);
            i = 0;
            for (i = 0; i < buf->n_obj; i++)
                glist_select(x, glist_nth(x, buf->p_a[i]));
            canvas_doclear(x);
        }
        else if (mode == UCUT_TEXT)
        {
            t_gobj *y1, *y2;
            for (y1 = x->gl_list; (y2 = y1->g_next); y1 = y2)
                ;
            if (y1)
                glist_delete(x, y1);
            canvas_dopaste(x, buf->u_redotextbuf);
            canvas_applybinbuf(x, buf->u_reconnectbuf);
        }
    }
    else if (action == UNDO_FREE)
        if (buf)
        {
            if (buf->u_objectbuf)
                binbuf_free(buf->u_objectbuf);
            if (buf->u_reconnectbuf)
                binbuf_free(buf->u_reconnectbuf);
            if (buf->u_redotextbuf)
                binbuf_free(buf->u_redotextbuf);
            t_freebytes(buf, sizeof(*buf) +
                sizeof(buf->p_a[0]) * (buf->n_obj-1));
        }
    return 1;
}

/* --------- 4. motion, including "tidy up" and stretching ----------- */

typedef struct _undo_move_elem
{
    int e_index;
    t_float e_xpix;
    t_float e_ypix;
} t_undo_move_elem;

typedef struct _undo_move
{
    t_undo_move_elem *u_vec;
    int u_n;
} t_undo_move;

void *canvas_undo_set_move(t_canvas *x, int selected)
{
    int x1, y1, x2, y2, indx;
    t_gobj *y;
    t_undo_move *buf =  (t_undo_move *)getbytes(sizeof(*buf));
    buf->u_n = selected ? glist_selectionindex(x, 0, 1) : glist_getindex(x, 0);
    buf->u_vec = (t_undo_move_elem *)getbytes(sizeof(*buf->u_vec) *
        (selected ? glist_selectionindex(x, 0, 1) : glist_getindex(x, 0)));
    if (selected)
    {
        int i;
        for (y = x->gl_list, i = indx = 0; y; y = y->g_next, indx++)
            if (glist_isselected(x, y))
            {
                gobj_getrect(y, x, &x1, &y1, &x2, &y2);
                buf->u_vec[i].e_index = indx;
                buf->u_vec[i].e_xpix = x1 / x->gl_zoom;
                buf->u_vec[i].e_ypix = y1 / x->gl_zoom;
                i++;
            }
    }
    else
    {
        for (y = x->gl_list, indx = 0; y; y = y->g_next, indx++)
        {
            gobj_getrect(y, x, &x1, &y1, &x2, &y2);
            buf->u_vec[indx].e_index = indx;
            buf->u_vec[indx].e_xpix = x1 / x->gl_zoom;
            buf->u_vec[indx].e_ypix = y1 / x->gl_zoom;
        }
    }
    THISED->canvas_undo_already_set_move = 1;
    return (buf);
}

int canvas_undo_move(t_canvas *x, void *z, int action)
{
    t_undo_move *buf = z;
    if (action == UNDO_UNDO || action == UNDO_REDO)
    {
        int resortin = 0, resortout = 0;
        int i;
        for (i = 0; i < buf->u_n; i++)
        {
            float newx = (buf->u_vec[i].e_xpix)*x->gl_zoom;
            float newy = (buf->u_vec[i].e_ypix)*x->gl_zoom;
            t_gobj*y = glist_nth(x, buf->u_vec[i].e_index);
            if (y)
            {
                int x1=0, y1=0, x2=0, y2=0;
                int doing = THISED->canvas_undo_already_set_move;
                t_class *cl = pd_class(&y->g_pd);
                glist_noselect(x);
                glist_select(x, y);
                gobj_getrect(y, x, &x1, &y1, &x2, &y2);
                THISED->canvas_undo_already_set_move = 1;
                canvas_displaceselection(x, (newx - x1)/(x->gl_zoom), (newy - y1)/(x->gl_zoom));
                THISED->canvas_undo_already_set_move = doing;
                buf->u_vec[i].e_xpix = x1/x->gl_zoom;
                buf->u_vec[i].e_ypix = y1/x->gl_zoom;
                if (cl == vinlet_class) resortin = 1;
                else if (cl == voutlet_class) resortout = 1;
            }
        }
        glist_noselect(x);
        for (i = 0; i < buf->u_n; i++)
        {
            t_gobj* y = glist_nth(x, buf->u_vec[i].e_index);
            if (y) glist_select(x, y);
        }
        if (resortin) canvas_resortinlets(x);
        if (resortout) canvas_resortoutlets(x);
    }
    else if (action == UNDO_FREE)
    {
        t_freebytes(buf->u_vec, buf->u_n * sizeof(*buf->u_vec));
        t_freebytes(buf, sizeof(*buf));
    }
    return 1;
}

/* --------- 5. paste (also duplicate) ----------- */

typedef struct _undo_paste
{
    int u_index;            /* index of first object pasted */
    int u_sel_index;        /* index of object selected at the time the other
                               object was pasted (for autopatching) */
    int u_offset;           /* xy-offset for duplicated items (since it differs
                               when duplicated into same or different canvas */
    t_binbuf *u_objectbuf;  /* here we store actual copied data */
} t_undo_paste;

void *canvas_undo_set_paste(t_canvas *x, int numpasted, int duplicate,
    int d_offset)
{
    t_undo_paste *buf =  (t_undo_paste *)getbytes(sizeof(*buf));
    buf->u_index = glist_getindex(x, 0) - numpasted; /* do we need numpasted at all? */
    if (!duplicate && x->gl_editor->e_selection &&
        !x->gl_editor->e_selection->sel_next)
    {
            /* if only one object is selected which will warrant autopatching */
        buf->u_sel_index = glist_getindex(x,
            x->gl_editor->e_selection->sel_what);
    }
    else
    {
        buf->u_sel_index = -1;
    }
    buf->u_offset = d_offset;
    buf->u_objectbuf = binbuf_duplicate(THISED->copy_binbuf);
    return (buf);
}
void *canvas_undo_set_pastebinbuf(t_canvas *x, t_binbuf *b,
    int numpasted, int duplicate, int d_offset)
{
    t_binbuf*tmpbuf = THISED->copy_binbuf;
    void*ret=0;
    THISED->copy_binbuf = b;
    ret = canvas_undo_set_paste(x, numpasted, duplicate, d_offset);
    THISED->copy_binbuf = tmpbuf;
    return ret;
}


int canvas_undo_paste(t_canvas *x, void *z, int action)
{
    t_undo_paste *buf = z;
    if (action == UNDO_UNDO)
    {
        t_gobj *y;
            /* check if the paste/duplicate we are undoing contains any
             * dirty abstractions; and if so, bail out */
        for (y = glist_nth(x, buf->u_index); y; y = y->g_next)
            if (canvas_undo_confirmdiscard(y))
                return 0;

        glist_noselect(x);
        for (y = glist_nth(x, buf->u_index); y; y = y->g_next)
            glist_select(x, y);
        canvas_doclear(x);
    }
    else if (action == UNDO_REDO)
    {
        glist_noselect(x);
            /* if the pasted object is supposed to be autopatched
             * then select the object it should be autopatched to
             */
        if (buf->u_sel_index > -1)
        {
            glist_select(x, glist_nth(x, buf->u_sel_index));
        }
        canvas_dopaste(x, buf->u_objectbuf);

            /* if it was "duplicate" have to re-enact the displacement. */
        if (buf->u_offset)
        {
            t_selection *y;
            for (y = x->gl_editor->e_selection; y; y = y->sel_next)
                gobj_displace(y->sel_what, x,
                    buf->u_offset, buf->u_offset);
        }
    }
    else if (action == UNDO_FREE)
    {
        if (buf->u_objectbuf)
            binbuf_free(buf->u_objectbuf);
        t_freebytes(buf, sizeof(*buf));
    }
    return 1;
}

/* --------- 6. apply  ----------- */

typedef struct _undo_apply
{
    t_binbuf *u_objectbuf;      /* the object cleared or typed into */
    t_binbuf *u_reconnectbuf;   /* connections into and out of object */
    int u_index;                /* index of the previous object */
} t_undo_apply;

void *canvas_undo_set_apply(t_canvas *x, int n)
{
    t_undo_apply *buf;
    t_gobj *obj;
    t_linetraverser t;
    t_outconnect *oc;
    int nnotsel;
        /* enable editor (in case it is disabled) and select the object
           we are working on */
    if (!x->gl_edit)
        canvas_editmode(x, 1);

        /* deselect all objects (if we are editing one while multiple are
         * selected, upon undoing this will recreate other selected objects,
         * effectively resulting in unwanted duplicates)
         * LATER: consider allowing concurrent editing of multiple objects
         */
    glist_noselect(x);

    obj = glist_nth(x, n);
    if (obj && !glist_isselected(x, obj))
        glist_select(x, obj);
        /* get number of all items for the offset below */
    nnotsel= glist_selectionindex(x, 0, 0);
    buf = (t_undo_apply *)getbytes(sizeof(*buf));

        /* store connections into/out of the selection */
    buf->u_reconnectbuf = binbuf_new();
    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
    {
        int issel1 = glist_isselected(x, &t.tr_ob->ob_g);
        int issel2 = glist_isselected(x, &t.tr_ob2->ob_g);
        if (issel1 != issel2)
        {
            binbuf_addv(buf->u_reconnectbuf, "ssiiii;",
                gensym("#X"), gensym("connect"),
                (issel1 ? nnotsel : 0)
                    + glist_selectionindex(x, &t.tr_ob->ob_g, issel1),
                t.tr_outno,
                (issel2 ? nnotsel : 0)
                    + glist_selectionindex(x, &t.tr_ob2->ob_g, issel2),
                t.tr_inno);
        }
    }
        /* copy object in its current state */
    buf->u_objectbuf = canvas_docopy(x);

        /* store index of the currently selected object */
    buf->u_index = n;

    return (buf);
}

static int canvas_apply_restore_original_position(t_canvas *x, int orig_pos);
int canvas_undo_apply(t_canvas *x, void *z, int action)
{
    t_undo_apply *buf = z;
    if (action == UNDO_UNDO || action == UNDO_REDO)
    {
            /* find current instance */
        t_binbuf *tmp;
        glist_noselect(x);
        glist_select(x, glist_nth(x, buf->u_index));

            /* copy it for the new undo/redo */
        tmp = canvas_docopy(x);

            /* delete current instance */
        canvas_doclear(x);

            /* replace it with previous instance */
        canvas_dopaste(x, buf->u_objectbuf);

            /* change previous instance with current one */
        buf->u_objectbuf = tmp;

            /* connections should stay the same */
        canvas_applybinbuf(x, buf->u_reconnectbuf);
            /* now we need to reposition the object to its original place */
        if (canvas_apply_restore_original_position(x, buf->u_index) && x->gl_havewindow)
            canvas_redraw(x);
    }
    else if (action == UNDO_FREE)
    {
        if (buf->u_objectbuf)
            binbuf_free(buf->u_objectbuf);
        if (buf->u_reconnectbuf)
            binbuf_free(buf->u_reconnectbuf);
        t_freebytes(buf, sizeof(*buf));
    }
    return 1;
}

int canvas_apply_restore_original_position(t_canvas *x, int orig_pos)
{
    t_gobj *y, *y_prev, *y_next;
        /* get the last object */
    y = glist_nth(x, glist_getindex(x, 0) - 1);
    if (glist_getindex(x, y) != orig_pos)
    {
            /* first make the object prior to the pasted one the end of the list */
        y_prev = glist_nth(x, glist_getindex(x, 0) - 2);
        if (y_prev)
            y_prev->g_next = NULL;
            /* if the object is supposed to be first in the gl_list */
        if (orig_pos == 0)
        {
            y->g_next = glist_nth(x, 0);
            x->gl_list = y;
        }
            /* if the object is supposed to be in the middle of the gl_list */
        else {
            y_prev = glist_nth(x, orig_pos-1);
            y_next = y_prev->g_next;
            y_prev->g_next = y;
            y->g_next = y_next;
        }
        return(1);
    }
    return(0);
}

/* --------- 7. arrange (to front/back)  ----------- */
typedef struct _undo_arrange
{
    int u_previndex;            /* old index */
    int u_newindex;                /* new index */
} t_undo_arrange;

void *canvas_undo_set_arrange(t_canvas *x, t_gobj *obj, int newindex)
{
        /* newindex tells us is the new index at the beginning (0) or the end (1) */

    t_undo_arrange *buf;
        /* enable editor (in case it is disabled) and select the object
           we are working on */
    if (!x->gl_edit)
        canvas_editmode(x, 1);

        /* select the object*/
    if (!glist_isselected(x, obj))
        glist_select(x, obj);

    buf = (t_undo_arrange *)getbytes(sizeof(*buf));

        /* set the u_newindex appropriately */
    if (newindex == 0) buf->u_newindex = 0;
    else buf->u_newindex = glist_getindex(x, 0) - 1;

        /* store index of the currently selected object */
    buf->u_previndex = glist_getindex(x, obj);

    return (buf);
}

/* called by undo/redo arrange and done_canvas_popup. only done_canvas_popup
   checks if it is a valid action and activates undo option */
static void canvas_doarrange(t_canvas *x, t_float which, t_gobj *oldy,
    t_gobj *oldy_prev, t_gobj *oldy_next)
{
    t_gobj *y_begin = x->gl_list;
    t_gobj *y_end = glist_nth(x, glist_getindex(x,0) - 1);

    switch((int)which)
    {
    case 3: /* to front */
            /* put the object at the end of the cue */
        y_end->g_next = oldy;
        oldy->g_next = NULL;

            /* now fix links in the hole made in the list due to moving of
                the oldy (we know there is oldy_next as y_end != oldy in
                canvas_done_popup)
             */
        if (oldy_prev) /* there is indeed more before the oldy position */
            oldy_prev->g_next = oldy_next;
        else x->gl_list = oldy_next;

#if 0
            /* and finally redraw */
        pdgui_vmess("gui_raise", "or", x, "selected");
#endif
        break;

    case 4: /* to back */
        x->gl_list = oldy; /* put it to the beginning of the cue */
        oldy->g_next = y_begin; /* make it point to the old beginning */

            /* now fix links in the hole made in the list due to moving of
              the oldy * (we know there is oldy_prev as y_begin != oldy in
              canvas_done_popup)
             */
        if (oldy_prev)
        {
            if (oldy_next) /* there is indeed more after oldy position */
                oldy_prev->g_next = oldy_next;
            else oldy_prev->g_next = NULL; /* oldy was the last in the cue */
        }

#if 0
            /* and finally redraw */
        pdgui_vmess("gui_lower", "or", x, "selected");
#endif
        break;
    default:
        bug("canvas_arrange");
        return;
    }
    canvas_dirty(x, 1);
}

int canvas_undo_arrange(t_canvas *x, void *z, int action)
{
    t_undo_arrange *buf = z;
    t_gobj *y=NULL, *prev=NULL, *next=NULL;

    if (!x->gl_edit)
        canvas_editmode(x, 1);

    switch(action)
    {

    case UNDO_UNDO:
        if(buf->u_newindex == buf->u_previndex) return 1;
            /* this is our object */
        y = glist_nth(x, buf->u_newindex);

            /* select object */
        glist_noselect(x);
        glist_select(x, y);

        if (buf->u_newindex)
        {
                /* if it is the last object */

                /* first previous object should point to nothing */
            prev = glist_nth(x, buf->u_newindex - 1);
            prev->g_next = NULL;

                /* now we reuse vars for the following:
                   old index should be right before the object previndex
                   is pointing to as the object was moved to the end */

                /* old position is not first */
            if (buf->u_previndex)
            {
                prev = glist_nth(x, buf->u_previndex - 1);
                next = prev->g_next;

                    /* now readjust pointers */
                prev->g_next = y;
                y->g_next = next;
            }
                /* old position is first */
            else {
                prev = NULL;
                next = x->gl_list;

                    /* now readjust pointers */
                y->g_next = next;
                x->gl_list = y;
            }
        }
        else {
                /* if it is the first object */

                /* old index should be right after the object previndex
                   is pointing to as the object was moved to the end */
            prev = glist_nth(x, buf->u_previndex);

                /* next may be NULL and that is ok */
            next = prev->g_next;

                /* first glist pointer needs to point to the second object */
            x->gl_list = y->g_next;

                /* now readjust pointers */
            prev->g_next = y;
            y->g_next = next;
        }
            /* and finally redraw canvas */
        if (x->gl_havewindow)
            canvas_redraw(x);
        break;
    case UNDO_REDO:
    {
        t_gobj *oldy_prev=NULL, *oldy_next=NULL;
        int arrangeaction;

        if(buf->u_newindex == buf->u_previndex) return 1;
            /* find our object */
        y = glist_nth(x, buf->u_previndex);

            /* select object */
        glist_noselect(x);
        glist_select(x, y);

        if (!buf->u_newindex) arrangeaction = 4;
        else arrangeaction = 3;


            /* if there is an object before ours (i.e., our index is > 0) */
        if (glist_getindex(x,y))
            oldy_prev = glist_nth(x, buf->u_previndex - 1);

            /* if there is an object after ours */
        if (y->g_next)
            oldy_next = y->g_next;

        canvas_doarrange(x, arrangeaction, y, oldy_prev, oldy_next);
    }
        break;
    case UNDO_FREE:
        t_freebytes(buf, sizeof(*buf));
    }
    return 1;
}

/* --------- 8. apply on canvas ----------- */
typedef struct _undo_canvas_properties
{
    int gl_pixwidth;            /* width in pixels (on parent, if a graph) */
    int gl_pixheight;
    t_float gl_x1;              /* bounding rectangle in our own coordinates */
    t_float gl_y1;
    t_float gl_x2;
    t_float gl_y2;
    int gl_screenx1;            /* screen coordinates when toplevel */
    int gl_screeny1;
    int gl_screenx2;
    int gl_screeny2;
    int gl_xmargin;             /* origin for GOP rectangle */
    int gl_ymargin;

    unsigned int gl_goprect:1;  /* draw rectangle for graph-on-parent */
    unsigned int gl_isgraph:1;  /* show as graph on parent */
    unsigned int gl_hidetext:1; /* hide object-name + args when doing
                                   graph on parent */
} t_undo_canvas_properties;

void *canvas_undo_set_canvas(t_canvas *x)
{
        /* enable editor (in case it is disabled) */
    t_undo_canvas_properties *buf =
        (t_undo_canvas_properties *)getbytes(sizeof(*buf));

    buf->gl_pixwidth = x->gl_pixwidth;
    buf->gl_pixheight = x->gl_pixheight;
    buf->gl_x1 = x->gl_x1;
    buf->gl_y1 = x->gl_y1;
    buf->gl_x2 = x->gl_x2;
    buf->gl_y2 = x->gl_y2;
    buf->gl_screenx1 = x->gl_screenx1;
    buf->gl_screeny1 = x->gl_screeny1;
    buf->gl_screenx2 = x->gl_screenx2;
    buf->gl_screeny2 = x->gl_screeny2;
    buf->gl_xmargin = x->gl_xmargin;
    buf->gl_ymargin = x->gl_ymargin;
    buf->gl_goprect = x->gl_goprect;
    buf->gl_isgraph = x->gl_isgraph;
    buf->gl_hidetext = x->gl_hidetext;

    return (buf);
}

int canvas_undo_canvas_apply(t_canvas *x, void *z, int action)
{
    t_undo_canvas_properties *buf = (t_undo_canvas_properties *)z;
    t_undo_canvas_properties tmp;

    if (action == UNDO_UNDO || action == UNDO_REDO)
    {
        if (!x->gl_edit)
            canvas_editmode(x, 1);
#if 0
            /* close properties window first */
        t_int properties = gfxstub_haveproperties((void *)x);
        if (properties)
        {
            pdgui_stub_deleteforkey(x);
        }
#endif
            /* store current canvas values into temporary data holder */
        tmp.gl_pixwidth = x->gl_pixwidth;
        tmp.gl_pixheight = x->gl_pixheight;
        tmp.gl_x1 = x->gl_x1;
        tmp.gl_y1 = x->gl_y1;
        tmp.gl_x2 = x->gl_x2;
        tmp.gl_y2 = x->gl_y2;
        tmp.gl_screenx1 = x->gl_screenx1;
        tmp.gl_screeny1 = x->gl_screeny1;
        tmp.gl_screenx2 = x->gl_screenx2;
        tmp.gl_screeny2 = x->gl_screeny2;
        tmp.gl_xmargin = x->gl_xmargin;
        tmp.gl_ymargin = x->gl_ymargin;
        tmp.gl_goprect = x->gl_goprect;
        tmp.gl_isgraph = x->gl_isgraph;
        tmp.gl_hidetext = x->gl_hidetext;

            /* change canvas values with the ones from the undo buffer */
        x->gl_pixwidth = buf->gl_pixwidth;
        x->gl_pixheight = buf->gl_pixheight;
        x->gl_x1 = buf->gl_x1;
        x->gl_y1 = buf->gl_y1;
        x->gl_x2 = buf->gl_x2;
        x->gl_y2 = buf->gl_y2;
        x->gl_screenx1 = buf->gl_screenx1;
        x->gl_screeny1 = buf->gl_screeny1;
        x->gl_screenx2 = buf->gl_screenx2;
        x->gl_screeny2 = buf->gl_screeny2;
        x->gl_xmargin = buf->gl_xmargin;
        x->gl_ymargin = buf->gl_ymargin;
        x->gl_goprect = buf->gl_goprect;
        x->gl_isgraph = buf->gl_isgraph;
        x->gl_hidetext = buf->gl_hidetext;

            /* copy data values from the temporary data to the undo buffer */
        buf->gl_pixwidth = tmp.gl_pixwidth;
        buf->gl_pixheight = tmp.gl_pixheight;
        buf->gl_x1 = tmp.gl_x1;
        buf->gl_y1 = tmp.gl_y1;
        buf->gl_x2 = tmp.gl_x2;
        buf->gl_y2 = tmp.gl_y2;
        buf->gl_screenx1 = tmp.gl_screenx1;
        buf->gl_screeny1 = tmp.gl_screeny1;
        buf->gl_screenx2 = tmp.gl_screenx2;
        buf->gl_screeny2 = tmp.gl_screeny2;
        buf->gl_xmargin = tmp.gl_xmargin;
        buf->gl_ymargin = tmp.gl_ymargin;
        buf->gl_goprect = tmp.gl_goprect;
        buf->gl_isgraph = tmp.gl_isgraph;
        buf->gl_hidetext = tmp.gl_hidetext;

            /* redraw */
        canvas_setgraph(x, x->gl_isgraph + 2*x->gl_hidetext, 0);
        canvas_dirty(x, 1);

        if (x->gl_havewindow)
        {
            canvas_redraw(x);
        }
        if (x->gl_owner && !x->gl_isclone && glist_isvisible(x->gl_owner))
        {
            glist_noselect(x);
            gobj_vis(&x->gl_gobj, x->gl_owner, 0);
            gobj_vis(&x->gl_gobj, x->gl_owner, 1);
            if (x->gl_owner->gl_havewindow)
                canvas_redraw(x->gl_owner);
        }
    }

    else if (action == UNDO_FREE)
    {
        if (buf)
            t_freebytes(buf, sizeof(*buf));
    }
    return 1;
}
/* --------- 9. create ----------- */

typedef struct _undo_create
{
    int u_index;                /* index of the created object object */
    t_binbuf *u_objectbuf;      /* the object cleared or typed into */
    t_binbuf *u_reconnectbuf;   /* connections into and out of object */
} t_undo_create;

void *canvas_undo_set_create(t_canvas *x)
{
    t_gobj *y;
    t_linetraverser t;
    int nnotsel;
    t_undo_create *buf = (t_undo_create *)getbytes(sizeof(*buf));
    buf->u_index = glist_getindex(x, 0) - 1;
    nnotsel= glist_selectionindex(x, 0, 0);

    buf->u_objectbuf = binbuf_new();
    if (x->gl_list)
    {
        t_outconnect *oc;
        for (y = x->gl_list; y; y = y->g_next)
        {
            if (!y->g_next)
            {
                gobj_save(y, buf->u_objectbuf);
                break;
            }
        }
        buf->u_reconnectbuf = binbuf_new();
        linetraverser_start(&t, x);
        while ((oc = linetraverser_next(&t)))
        {
            int issel1, issel2;
            issel1 = ( &t.tr_ob->ob_g == y ? 1 : 0);
            issel2 = ( &t.tr_ob2->ob_g == y ? 1 : 0);
            if (issel1 != issel2)
            {
                binbuf_addv(buf->u_reconnectbuf, "ssiiii;",
                    gensym("#X"), gensym("connect"),
                    (issel1 ? nnotsel : 0)
                        + glist_selectionindex(x, &t.tr_ob->ob_g, issel1),
                    t.tr_outno,
                    (issel2 ? nnotsel : 0)
                        + glist_selectionindex(x, &t.tr_ob2->ob_g, issel2),
                    t.tr_inno);
            }
        }
    }
    return (buf);
}

int canvas_undo_create(t_canvas *x, void *z, int action)
{
    t_undo_create *buf = z;
    t_gobj *y;

    if (action == UNDO_UNDO)
    {
        glist_noselect(x);
        y = glist_nth(x, buf->u_index);
        glist_select(x, y);
        canvas_doclear(x);
    }
    else if (action == UNDO_REDO)
    {
        canvas_applybinbuf(x, buf->u_objectbuf);
        canvas_applybinbuf(x, buf->u_reconnectbuf);
        if (pd_this->pd_newest && pd_class(pd_this->pd_newest) == canvas_class)
            canvas_loadbang((t_canvas *)pd_this->pd_newest);
        y = glist_nth(x, buf->u_index);
        glist_select(x, y);
    }
    else if (action == UNDO_FREE)
    {
        binbuf_free(buf->u_objectbuf);
        binbuf_free(buf->u_reconnectbuf);
        t_freebytes(buf, sizeof(*buf));
    }
    return 1;
}

/* ------ 10. recreate (called from text_setto after text has changed) ------ */

/* recreate uses t_undo_create struct */

void *canvas_undo_set_recreate(t_canvas *x, t_gobj *y, int pos)
{
    t_linetraverser t;
    t_outconnect *oc;
    int nnotsel;

    t_undo_create *buf = (t_undo_create *)getbytes(sizeof(*buf));
    buf->u_index = pos;
    nnotsel= glist_selectionindex(x, 0, 0) - 1; /* - 1 is a critical difference from the create */
    buf->u_objectbuf = binbuf_new();
    gobj_save(y, buf->u_objectbuf);

    buf->u_reconnectbuf = binbuf_new();
    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
    {
        int issel1, issel2;
        issel1 = ( &t.tr_ob->ob_g == y ? 1 : 0);
        issel2 = ( &t.tr_ob2->ob_g == y ? 1 : 0);
        if (issel1 != issel2)
        {
            binbuf_addv(buf->u_reconnectbuf, "ssiiii;",
                gensym("#X"), gensym("connect"),
                (issel1 ? nnotsel : 0)
                    + glist_selectionindex(x, &t.tr_ob->ob_g, issel1),
                t.tr_outno,
                (issel2 ? nnotsel : 0)
                    + glist_selectionindex(x, &t.tr_ob2->ob_g, issel2),
                t.tr_inno);
        }
    }
    return (buf);
}

int canvas_undo_recreate(t_canvas *x, void *z, int action)
{
    t_undo_create *buf = z;
    t_gobj *y = NULL;
    if (action == UNDO_UNDO)
        y = glist_nth(x, glist_getindex(x, 0) - 1);
    else if (action == UNDO_REDO)
        y = glist_nth(x, buf->u_index);

        /* check if we are undoing the creation of a dirty abstraction;
         * if so, bail out */
    if ((action == UNDO_UNDO)
        && canvas_undo_confirmdiscard(y))
            return 0;

    if (action == UNDO_UNDO || action == UNDO_REDO)
    {
            /* first copy new state of the current object */
        t_undo_create *buf2 = (t_undo_create *)getbytes(sizeof(*buf));
        buf2->u_index = buf->u_index;

        buf2->u_objectbuf = binbuf_new();
        gobj_save(y, buf2->u_objectbuf);

        buf2->u_reconnectbuf = binbuf_duplicate(buf->u_reconnectbuf);

            /* now cut the existing object */
        glist_noselect(x);
        glist_select(x, y);
        canvas_doclear(x);

            /* then paste the old object */

            /* save and clear bindings to symbols #A, #N, #X; restore when done */
        canvas_applybinbuf(x, buf->u_objectbuf);
        canvas_applybinbuf(x, buf->u_reconnectbuf);

            /* free the old data */
        binbuf_free(buf->u_objectbuf);
        binbuf_free(buf->u_reconnectbuf);
        t_freebytes(buf, sizeof(*buf));

            /* re-adjust pointer
             * (this should probably belong into g_undo.c, but since it is
             * a unique case, we'll let it be for the time being)
             */
        canvas_undo_get(x)->u_last->data = (void *)buf2;
        buf = buf2;

            /* reposition object to its original place */
        if (action == UNDO_UNDO)
            if (canvas_apply_restore_original_position(x, buf->u_index) && x->gl_havewindow)
                canvas_redraw(x);

            /* send a loadbang */
        if (pd_this->pd_newest && pd_class(pd_this->pd_newest) == canvas_class)
            canvas_loadbang((t_canvas *)pd_this->pd_newest);

            /* select */
        if (action == UNDO_REDO)
            y = glist_nth(x, glist_getindex(x, 0) - 1);
        else
            y = glist_nth(x, buf->u_index);
        glist_select(x, y);
    }
    else if (action == UNDO_FREE)
    {
        binbuf_free(buf->u_objectbuf);
        binbuf_free(buf->u_reconnectbuf);
        t_freebytes(buf, sizeof(*buf));
    }
    return 1;
}

/* ----------- 11. font -------------- */
static void canvas_dofont(t_canvas *x, t_floatarg font, t_floatarg xresize, t_floatarg yresize);

typedef struct _undo_font
{
    int font;
    t_float resize;
    int which;
} t_undo_font;

void *canvas_undo_set_font(t_canvas *x, int font, t_float resize, int which)
{
    t_undo_font *u_f = (t_undo_font *)getbytes(sizeof(*u_f));
    u_f->font = font;
    u_f->resize = resize;
    u_f->which = which;
    return (u_f);
}

int canvas_undo_font(t_canvas *x, void *z, int action)
{
    t_undo_font *u_f = z;

    if (action == UNDO_UNDO || action == UNDO_REDO)
    {
        t_canvas *x2 = canvas_getrootfor(x);
        int tmp_font = x2->gl_font;

        int whichresize = u_f->which;
        t_float realresize = 1./u_f->resize;
        t_float realresx = 1, realresy = 1;
        if (whichresize != 3) realresx = realresize;
        if (whichresize != 2) realresy = realresize;
        canvas_dofont(x2, u_f->font, realresx, realresy);

        u_f->resize = realresize;
        u_f->font = tmp_font;
    }
    else if (action == UNDO_FREE)
    {
        if (u_f)
            freebytes(u_f, sizeof(*u_f));
    }
    return 1;
}

int clone_match(t_pd *z, t_symbol *name, t_symbol *dir);
static void canvas_cut(t_canvas *x);
int clone_reload(t_pd *z, t_canvas *except);
void clone_doreload(t_pd *z, t_symbol *name, t_symbol *dir, t_canvas *except);

    /* recursively check for abstractions to reload as result of a save.
       Don't reload the one we just saved ("except") though. */
    /* LATER try to do the same trick for externs. */
void glist_doreload(t_glist *gl, t_symbol *name, t_symbol *dir, t_canvas *except)
{
    t_gobj *g;
    int hadwindow = gl->gl_havewindow;
    int found = 0;
        /* to optimize redrawing we select all objects that need to be updated
           and redraw (cut+undo) them together. Then we look for sub-patches
           that may have more of the same... */
    for (g = gl->gl_list; g; g = g->g_next)
    {
            /* remake the object if it's an abstraction that appears to have
               been loaded from the file we just saved */
        int remakeit = (g != &except->gl_gobj && pd_class(&g->g_pd) == canvas_class &&
            canvas_isabstraction((t_canvas *)g) &&
                ((t_canvas *)g)->gl_name == name &&
                    canvas_getdir((t_canvas *)g) == dir);
            /* check if it's a "clone" with that name */
        if (pd_class(&g->g_pd) == clone_class &&
            clone_match(&g->g_pd, name, dir))
        {
                /* try to reload "clone" object, otherwise remake it */
            if (!clone_reload(&g->g_pd, (t_canvas *)except))
                remakeit = 1;
        }
        if (remakeit)
        {
                /* Bugfix for cases where canvas_vis doesn't actually create a
                   new editor. We need to fix canvas_vis so that the bug
                   doesn't get triggered. But since we know this fixes a
                   regression we'll keep this as a point in the history as we
                   fix canvas_vis. Once that's done we can remove this call. */
            canvas_create_editor(gl);

            if (!gl->gl_havewindow)
            {
                canvas_vis(glist_getcanvas(gl), 1);
            }
            if (!found)
            {
                glist_noselect(gl);
                found = 1;
            }
            glist_select(gl, g);
        }
    }
        /* cut all selected (matched) objects and undo, to reinstantiate them */
    if (found)
    {
        canvas_cut(gl);
        canvas_undo_undo(gl);
        canvas_undo_rebranch(gl);
        glist_noselect(gl);
    }

        /* now iterate over all the sub-patches and also abstractions and clones,
        but only the ones that don't match! */
    for (g = gl->gl_list; g; g = g->g_next)
    {
        if (g == &except->gl_gobj)
            continue;
        if (pd_class(&g->g_pd) == canvas_class &&
            (!canvas_isabstraction((t_canvas *)g) ||
                 ((t_canvas *)g)->gl_name != name ||
                    canvas_getdir((t_canvas *)g) != dir))
                glist_doreload((t_canvas *)g, name, dir, except);
        else if (pd_class(&g->g_pd) == clone_class &&
                !clone_match(&g->g_pd, name, dir))
            clone_doreload(&g->g_pd, name, dir, except);
    }
    if (!hadwindow && gl->gl_havewindow)
        canvas_vis(glist_getcanvas(gl), 0);
}

    /* call canvas_doreload on everyone */
void canvas_reload(t_symbol *name, t_symbol *dir, t_glist *except)
{
    t_canvas *x;
    int dspwas = canvas_suspend_dsp();
    t_binbuf*b = 0;
    if(THISED->copy_binbuf)
        b = binbuf_duplicate(THISED->copy_binbuf);

    THISGUI->i_reloadingabstraction = except;
        /* find all root canvases */
    for (x = pd_getcanvaslist(); x; x = x->gl_next)
        glist_doreload(x, name, dir, except);
    THISGUI->i_reloadingabstraction = 0;
    if(b)
    {
        if(THISED->copy_binbuf)
            binbuf_free(THISED->copy_binbuf);
        THISED->copy_binbuf = b;
    }
    canvas_resume_dsp(dspwas);
}

/* ------------------------ event handling ------------------------ */

static const char *cursorlist[] = {
    "$cursor_runmode_nothing",
    "$cursor_runmode_clickme",
    "$cursor_runmode_thicken",
    "$cursor_runmode_addpoint",
    "$cursor_editmode_nothing",
    "$cursor_editmode_connect",
    "$cursor_editmode_disconnect",
    "$cursor_editmode_resize"
};

void canvas_setcursor(t_canvas *x, unsigned int cursornum)
{
    if (cursornum >= sizeof(cursorlist)/sizeof *cursorlist)
    {
        bug("canvas_setcursor");
        return;
    }
    if (THISED->canvas_cursorcanvaswas != x ||
        THISED->canvas_cursorwas != cursornum)
    {
        pdgui_vmess(0, "^r rr", x, "configure", "-cursor", cursorlist[cursornum]);
        THISED->canvas_cursorcanvaswas = x;
        THISED->canvas_cursorwas = cursornum;
    }
}

    /* check if a point lies in a gobj.  */
int canvas_hitbox(t_canvas *x, t_gobj *y, int xpos, int ypos,
    int *x1p, int *y1p, int *x2p, int *y2p, int extrapix)
{
    int x1, y1, x2, y2;
    if (!gobj_shouldvis(y, x))
        return (0);
    gobj_getrect(y, x, &x1, &y1, &x2, &y2);
    if (xpos >= x1-extrapix && xpos <= x2+extrapix
        && ypos >= y1-extrapix && ypos <= y2+extrapix)
    {
        *x1p = x1;
        *y1p = y1;
        *x2p = x2;
        *y2p = y2;
        return (1);
    }
    else return (0);
}

    /* find the last gobj, if any, containing the point. */
static t_gobj *canvas_findhitbox(t_canvas *x, int xpos, int ypos,
    int *x1p, int *y1p, int *x2p, int *y2p)
{
    t_gobj *y, *rval = 0;
    int x1, y1, x2, y2;
    *x1p = -0x7fffffff;
    for (y = x->gl_list; y; y = y->g_next)
    {
        if (canvas_hitbox(x, y, xpos, ypos, &x1, &y1, &x2, &y2, 0)
            && (x1 > *x1p))
                *x1p = x1, *y1p = y1, *x2p = x2, *y2p = y2, rval = y;
    }
        /* if none, try again with fatter boxes so that we can click on
        inlets or outlets from slightly outside the box */
    if (!rval)
        for (y = x->gl_list; y; y = y->g_next)
    {
        if (canvas_hitbox(x, y, xpos, ypos, &x1, &y1, &x2, &y2, 4)
            && (x1 > *x1p))
                *x1p = x1, *y1p = y1, *x2p = x2, *y2p = y2, rval = y;
    }
        /* if there are at least two selected objects, we'd prefer
           to find a selected one (never mind which) to the one we got. */
    if (x->gl_editor && x->gl_editor->e_selection &&
        x->gl_editor->e_selection->sel_next && !glist_isselected(x, y))
    {
        t_selection *sel;
        for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
            if (canvas_hitbox(x, sel->sel_what, xpos, ypos,
                &x1, &y1, &x2, &y2,  0))
                    *x1p = x1, *y1p = y1, *x2p = x2, *y2p = y2,
                        rval = sel->sel_what;
    }
    return (rval);
}

/* -------------------  editors -------------------- */

static t_editor *editor_new(t_glist *owner)
{
    char buf[40];
    t_editor *x = (t_editor *)getbytes(sizeof(*x));
    x->e_connectbuf = binbuf_new();
    x->e_deleted = binbuf_new();
    x->e_glist = owner;
    sprintf(buf, ".x%lx", (t_int)owner);
    x->e_guiconnect = guiconnect_new(&owner->gl_pd, gensym(buf));
    return (x);
}

static void editor_free(t_editor *x, t_glist *y)
{
    glist_noselect(y);
    guiconnect_notarget(x->e_guiconnect, 1000);
    binbuf_free(x->e_connectbuf);
    binbuf_free(x->e_deleted);
    sys_unqueuegui(x);
    freebytes((void *)x, sizeof(*x));
}

    /* create an editor for the glist in preparation for making it visible */
void canvas_create_editor(t_glist *x)
{
    if (!x->gl_editor)
        x->gl_editor = editor_new(x);
}

void canvas_destroy_editor(t_glist *x)
{
    glist_noselect(x);
    if (x->gl_editor)
    {
        t_rtext *rtext;
            /* this happens if we had activated an atom box in run mode: */
        if (glist_textedfor(x))
            rtext_activate(glist_textedfor(x), 0);
        while ((rtext = x->gl_editor->e_rtext))
            rtext_free(rtext);
        editor_free(x->gl_editor, x);
        x->gl_editor = 0;
    }
}

void canvas_reflecttitle(t_canvas *x);
void canvas_map(t_canvas *x, t_floatarg f);

    /* we call this when we want the window to become visible, mapped, and
       in front of all windows; or with "f" zero, when we want to get rid of
       the window. */
void canvas_vis(t_canvas *x, t_floatarg f)
{
    int flag = (f != 0);
    if (flag)
    {
            /* If a subpatch has GOP/gl_isgraph set, then it will have
             * a gl_editor already, if its not, it will not have a gl_editor.
             * canvas_create_editor(x) checks if a gl_editor already exists,
             * so its ok to run it on a canvas that already has a gl_editor. */
        if (x->gl_editor && x->gl_havewindow)
        {           /* just put us in front */
            pdgui_vmess("pdtk_canvas_raise", "^", x);
        }
            /* ouch - if there's no gui yet, because we're still waiting for
            font metrics, just set the flag and we'll clean up the mess later
            when the font metrics arrive. */
        else if (!sys_havegui())
            x->gl_havewindow = 1;
        else
        {
            char cbuf[MAXPDSTRING];
            t_canvas *c = x;
            t_undo *undo = canvas_undo_get(x);
            t_undo_action *udo = undo ? undo->u_last : 0;
            char winpos[128];
            t_canvas**parents = getbytes(0);
            size_t numparents;

            canvas_create_editor(x);
                    /* initial values for new windows: */
            if ((GLIST_DEFCANVASXLOC == x->gl_screenx1) &&
                (GLIST_DEFCANVASYLOC == x->gl_screeny1))
                    winpos[0]=0;
            else
            {
                sprintf(winpos, "+%d+%d", (int)(x->gl_screenx1),
                    (int)(x->gl_screeny1));
            }

                /* if color isn't white, pass color as extra argument to
                pdtk_canvas_new; but if it's just white don't pass it in
                case we're talking to an older GUI version (so that
                pureVST can work with Pd 0.55 as its GUI) */
            if (THISGUI->i_backgroundcolor != 0xFFFFFF)
                pdgui_vmess("pdtk_canvas_new", "^ ii si kk", x,
                    (int)(x->gl_screenx2 - x->gl_screenx1),
                    (int)(x->gl_screeny2 - x->gl_screeny1),
                    winpos, x->gl_edit,
                    THISGUI->i_backgroundcolor, THISGUI->i_foregroundcolor);
            else pdgui_vmess("pdtk_canvas_new", "^ ii si", x,
                    (int)(x->gl_screenx2 - x->gl_screenx1),
                (int)(x->gl_screeny2 - x->gl_screeny1), winpos, x->gl_edit);

            numparents = 0;
            while (c->gl_owner && !c->gl_isclone) {
                t_canvas**newparents = (t_canvas**)resizebytes(parents,
                     numparents * sizeof(*newparents),
                        (numparents+1) * sizeof(*newparents));
                if (!newparents)
                    break;
                c = c->gl_owner;
                parents = newparents;
                parents[numparents] = c;
                numparents++;
            }
            pdgui_vmess("pdtk_canvas_setparents", "^C", x, numparents,
                parents);
            freebytes(parents, numparents * sizeof(t_canvas));

            x->gl_havewindow = 1;
            canvas_reflecttitle(x);
            canvas_updatewindowlist();
            pdgui_vmess("pdtk_undomenu", "^ ss", x, udo?(udo->name):"no",
                (udo && udo->next)?(udo->next->name):"no");
        }
    }
    else    /* make invisible */
    {
        int i;
        t_canvas *x2;
        if (!x->gl_havewindow)
        {
                /* bug workaround -- a graph in a visible patch gets "invised"
                when the patch is closed, and must lose the editor here.  It's
                probably not the natural place to do this.  Other cases like
                subpatches fall here too but don'd need the editor freed, so
                we check if it exists. */
            if (x->gl_editor)
                canvas_destroy_editor(x);
            return;
        }
        glist_noselect(x);
        if (glist_isvisible(x))
            canvas_map(x, 0);
        canvas_destroy_editor(x);
        pdgui_vmess("destroy", "^", x);
        for (i = 1, x2 = x; x2; x2 = x2->gl_next, i++)
            ;
            /* if we're a graph on our parent, and if the parent exists
               and is visible, show ourselves on parent. */
        if (glist_isgraph(x) && x->gl_owner && !x->gl_isclone)
        {
            t_glist *gl2 = x->gl_owner;
            if (glist_isvisible(gl2))
                gobj_vis(&x->gl_gobj, gl2, 0);
            x->gl_havewindow = 0;
            if (glist_isvisible(gl2) && !gl2->gl_isdeleting)
            {
                    /* make sure zoom level matches parent, ie. after an open
                       subpatch's zoom level was changed before being closed */
                if(x->gl_zoom != gl2->gl_zoom)
                    canvas_zoom(x, gl2->gl_zoom);
                gobj_vis(&x->gl_gobj, gl2, 1);
            }
        }
        else x->gl_havewindow = 0;
        canvas_updatewindowlist();
    }
}

    /* set a canvas up as a graph-on-parent.  Set reasonable defaults for
       any missing parameters and redraw things if necessary. */
void canvas_setgraph(t_glist *x, int flag, int nogoprect)
{
    int can_graph_on_parent = x->gl_owner && !x->gl_isclone && !x->gl_loading
        && glist_isvisible(x->gl_owner);
    if (!flag && glist_isgraph(x))
    {
        if (can_graph_on_parent)
            gobj_vis(&x->gl_gobj, x->gl_owner, 0);
                /* changing 'isgraph' might change which window some texts
                should appear on, so the rtexts will have to move.  So we
                nuke them; they'll be recreated in the correct glist next
                time each one is asked for. */
        glist_clearrtexts(x);
        x->gl_isgraph = x->gl_hidetext = 0;
        if (can_graph_on_parent)
        {
            gobj_vis(&x->gl_gobj, x->gl_owner, 1);
            canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
        }
    }
    else if (flag)
    {
        if (x->gl_pixwidth <= 0)
            x->gl_pixwidth = GLIST_DEFGRAPHWIDTH;

        if (x->gl_pixheight <= 0)
            x->gl_pixheight = GLIST_DEFGRAPHHEIGHT;

        if (can_graph_on_parent)
            gobj_vis(&x->gl_gobj, x->gl_owner, 0);
            /* only clear rtexts when transitioning into GOP */
        if (!glist_isgraph(x))
            glist_clearrtexts(x);
        x->gl_isgraph = 1;
        x->gl_hidetext = !(!(flag&2));
        x->gl_goprect = !nogoprect;
        if (glist_isvisible(x) && x->gl_goprect)
            glist_redraw(x);
        if (can_graph_on_parent)
        {
            gobj_vis(&x->gl_gobj, x->gl_owner, 1);
            canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
        }
    }
}

void garray_properties(t_garray *x);

    /* tell GUI to create a properties dialog on the canvas.  We tell
       the user the negative of the "pixel" y scale to make it appear to grow
       naturally upward, whereas pixels grow downward. */
void canvas_properties(t_gobj*z, t_glist*unused)
{
    t_glist *x = (t_glist*)z;
    t_gobj *y;
    int isgraph = glist_isgraph(x);
    t_float x1=0., y1=-1., x2=1., y2=1.;
    t_float xscale=0., yscale=0.;
    char *textbuf = 0;
    int textsize;
    if (x->gl_owner && canvas_isabstraction(x))
    {
        binbuf_gettext(x->gl_obj.te_binbuf, &textbuf, &textsize);
        textbuf = (char *)resizebytes(textbuf, textsize, textsize+1);
        textbuf[textsize] = 0;
    }
    pdgui_vmess("::dialog_canvas::set_text", "s", (textbuf ? textbuf : ""));
    if(isgraph) {
        x1=x->gl_x1;
        y1=x->gl_y1;
        x2=x->gl_x2;
        y2=x->gl_y2;
    } else {
        xscale= glist_dpixtodx(x, 1);
        yscale=-glist_dpixtody(x, 1);
    }
    pdgui_stub_vnew(&x->gl_pd, "pdtk_canvas_dialog", x, "ff i ffff ii ii",
        xscale, yscale,  /* used to be %g ... */
        isgraph,
        x1,y1, x2,y2,  /* used to be %g ... */
        (int)x->gl_pixwidth, (int)x->gl_pixheight,
        (int)x->gl_xmargin, (int)x->gl_ymargin);


        /* if any arrays are in the graph, put out their dialogs too */
    for (y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == garray_class)
            garray_properties((t_garray *)y);
    if (textbuf)
        freebytes(textbuf, textsize+1);
}

    /* called from the gui when "OK" is selected on the canvas properties
       dialog.  Again we negate "y" scale. */
static void canvas_donecanvasdialog(t_glist *x,
    t_symbol *s, int argc, t_atom *argv)
{
    t_float xperpix, yperpix, x1, y1, x2, y2, xpix, ypix, xmargin, ymargin;
    int graphme, redraw = 0, fromgui;
    glist_clearrtexts(x);
        /* if there are extra arguments, the user has typed new text in
        the dialog window.  Unfortunately, some old patches use this to
        resize rectangles programmatically, and moreover send extra
        arguments.  This will break in 0.56 and later.  At least we can
        catch cases in which the patch isn't yet visible and in that case
        ignore the extra args - the dialog should only send this for a
        visible one.  (Actually, is that really true?)  Anyhow, since
        at least one bandit call to this method contains extra arguments
        (apparently from the object itself which is then getting deleted
        and crashing Pd) we test that there's an extra string "text" before
        the new object creation text.   */

    if (x->gl_owner && argc > 13 && glist_isvisible(x->gl_owner) &&
        !strcmp(atom_getsymbolarg(12, argc, argv)->s_name, "text"))
    {
        t_gobj *y =  &x->gl_obj.ob_g;
        t_binbuf *b = binbuf_new();
        char *textbuf;
        int textsize;
        binbuf_restore(b, argc-13, argv+13);
        binbuf_gettext(b, &textbuf, &textsize);
        binbuf_free(b);
        canvas_undo_add(x->gl_owner, UNDO_SEQUENCE_START, "typing", 0);

        glist_noselect(x->gl_owner);
        canvas_undo_add(x->gl_owner, UNDO_ARRANGE, "arrange",
            canvas_undo_set_arrange(x->gl_owner, y, 1));

            /* store the current connections of the owner */
        glist_noselect(x->gl_owner);
        glist_select(x->gl_owner, y);
        canvas_stowconnections(glist_getcanvas(x->gl_owner));

            /* change the text (this restores the connections we just saved) */
        glist_noselect(x->gl_owner);
        t_canvas *owner = x->gl_owner;
        text_setto(&x->gl_obj, x->gl_owner, textbuf, textsize);

        canvas_fixlinesfor(owner, &x->gl_obj);
        glist_settexted(owner, 0);

        canvas_undo_add(owner, UNDO_SEQUENCE_END, "typing", 0);
        freebytes(textbuf, textsize);
        canvas_dirty(owner, 1);
        return;
    }

    xperpix = atom_getfloatarg(0, argc, argv);
    yperpix = atom_getfloatarg(1, argc, argv);
    graphme = (int)(atom_getfloatarg(2, argc, argv));
    x1 = atom_getfloatarg(3, argc, argv);
    y1 = atom_getfloatarg(4, argc, argv);
    x2 = atom_getfloatarg(5, argc, argv);
    y2 = atom_getfloatarg(6, argc, argv);
    xpix = atom_getfloatarg(7, argc, argv);
    ypix = atom_getfloatarg(8, argc, argv);
    xmargin = atom_getfloatarg(9, argc, argv);
    ymargin = atom_getfloatarg(10, argc, argv);
    fromgui = atom_getfloatarg(11, argc, argv);
        /* hack - if graphme is 2 (meaning, not GOP but hide the text anyhow),
           perhaps we're happier with 0.  This is only checked if this is really
           being called, as intended, from the GUI.  For compatibility with old
           patches that reverse-engineered donecanvasdialog to modify patch
           parameters, we leave the buggy behavior in when there's no "fromgui"
           argument supplied. */
    if (fromgui && (!(graphme & 1)))
        graphme = 0;
        /* parent windows are treated the same as
           applies to individual objects */
    canvas_undo_add(x, UNDO_CANVAS_APPLY, "apply", canvas_undo_set_canvas(x));

    x->gl_pixwidth = xpix;
    x->gl_pixheight = ypix;
    x->gl_xmargin = xmargin;
    x->gl_ymargin = ymargin;

    yperpix = -yperpix;
    if (xperpix == 0)
        xperpix = 1;
    if (yperpix == 0)
        yperpix = 1;

    if (graphme)
    {
        if (x1 != x2)
            x->gl_x1 = x1, x->gl_x2 = x2;
        else x->gl_x1 = 0, x->gl_x2 = 1;
        if (y1 != y2)
            x->gl_y1 = y1, x->gl_y2 = y2;
        else x->gl_y1 = 0, x->gl_y2 = 1;
    }
    else
    {
        if (xperpix != glist_dpixtodx(x, 1) || yperpix != glist_dpixtody(x, 1))
            redraw = 1;
        if (xperpix > 0)
        {
            x->gl_x1 = 0;
            x->gl_x2 = xperpix;
        }
        else
        {
            x->gl_x1 = -xperpix * (x->gl_screenx2 - x->gl_screenx1);
            x->gl_x2 = x->gl_x1 + xperpix;
        }
        if (yperpix > 0)
        {
            x->gl_y1 = 0;
            x->gl_y2 = yperpix;
        }
        else
        {
            x->gl_y1 = -yperpix * (x->gl_screeny2 - x->gl_screeny1);
            x->gl_y2 = x->gl_y1 + yperpix;
        }
    }
        /* LATER avoid doing 2 redraws here (possibly one inside setgraph) */
    if (graphme != glist_isgraph(x))
        canvas_setgraph(x, graphme, 0);
    canvas_dirty(x, 1);
    if (x->gl_owner)
        canvas_redraw(glist_getcanvas(x->gl_owner));
    if (x->gl_havewindow)
        canvas_redraw(x);
}

    /* called from the gui when a popup menu comes back with "properties,"
       "open," or "help." */
static void canvas_done_popup(t_canvas *x, t_float which,
    t_float xpos, t_float ypos)
{
    char namebuf[MAXPDSTRING], *basenamep;
    t_gobj *y;
    for (y = x->gl_list; y; y = y->g_next)
    {
        int x1, y1, x2, y2;
        if (canvas_hitbox(x, y, xpos, ypos, &x1, &y1, &x2, &y2, 0))
        {
            if (which == 0)     /* properties */
            {
                if (!class_getpropertiesfn(pd_class(&y->g_pd)))
                    continue;
                (*class_getpropertiesfn(pd_class(&y->g_pd)))(y, x);
                return;
            }
            else if (which == 1)    /* open */
            {
                if (!zgetfn(&y->g_pd, gensym("menu-open")))
                    continue;
                vmess(&y->g_pd, gensym("menu-open"), "");
                return;
            }
            else    /* help */
            {
                const char *dir;
                if (pd_class(&y->g_pd) == canvas_class &&
                    canvas_isabstraction((t_canvas *)y))
                {
                    t_object *ob = (t_object *)y;
                    int ac = binbuf_getnatom(ob->te_binbuf);
                    t_atom *av = binbuf_getvec(ob->te_binbuf);
                    if (ac < 1)
                        return;
                    atom_string(av, namebuf, MAXPDSTRING);

                        /* strip dir from name : */
                    basenamep = strrchr(namebuf, '/');
#ifdef _WIN32
                    if (!basenamep)
                        basenamep = strrchr(namebuf, '\\');
#endif
                    if (!basenamep)
                        basenamep = namebuf;
                    else basenamep++;   /* strip last '/' */

                    dir = canvas_getdir((t_canvas *)y)->s_name;
                }
                else
                {
                    strncpy(namebuf, class_gethelpname(pd_class(&y->g_pd)),
                        MAXPDSTRING-1);
                    namebuf[MAXPDSTRING-1] = 0;
                    dir = class_gethelpdir(pd_class(&y->g_pd));
                    basenamep = namebuf;
                }
                if (strlen(namebuf) < 4 ||
                    strcmp(namebuf + strlen(namebuf) - 3, ".pd"))
                        strcat(namebuf, ".pd");
                open_via_helppath(basenamep, dir);
                return;
            }
        }
    }
    if (which == 0)
        canvas_properties(&x->gl_gobj, 0);
    else if (which == 2)
        open_via_helppath("intro.pd", canvas_getdir((t_canvas *)x)->s_name);
}

#define NOMOD 0
#define SHIFTMOD 1
#define CTRLMOD 2
#define ALTMOD 4
#define RIGHTCLICK 8

#define DCLICKINTERVAL 0.25

    /* undarken deselected gatoms:
     * it's slightly ugly to have this in here,  but we cannot undarken
     * in gatom_key (which is the gatom's e_keyfn) as this is also called
     * when the user just hits <kbd>Enter</kbd>
     */
void gatom_undarken(t_text *x);
static void undarken_if_gatom(t_gobj *gobj)
{
    t_object *obj = (gobj ? pd_checkobject(&gobj->g_pd) : 0);
    if (obj && T_ATOM == obj->te_type)
        gatom_undarken(obj);
}

    /* right-clicking on a canvas object pops up a menu. */
static void canvas_rightclick(t_canvas *x, int xpos, int ypos, int mod)
{
    int x1, y1, x2, y2;
    t_gobj *y = canvas_findhitbox(x, xpos, ypos, &x1, &y1, &x2, &y2);
    int canprop = (!y || class_getpropertiesfn(pd_class(&y->g_pd)));
    int canopen = (y && zgetfn(&y->g_pd, gensym("menu-open")));
    pdgui_vmess("pdtk_canvas_popup", "^ ii ii",
        x,  xpos, ypos,  canprop, canopen);
}

    /* mouse click or, if !doit, mouse motion (in which case we still search
    through the canvas to show the appropriate cursor) */
static void canvas_doclick(t_canvas *x, int xpix, int ypix, int mod, int doit)
{
    t_gobj *hitbox;
    int shiftmod, runmode, altmod, doubleclick = 0, rightclick;
    int x1=0, y1=0, x2=0, y2=0, clickreturned = 0;
    t_text *hitobj = 0;
    t_scalar *hitscalar = 0;
    t_word *hitwords = 0;
    t_gobj *hitdrawtext = 0;
    t_rtext *rtext;

    if(!x->gl_editor) {
        bug("editor");
        return;
    }

    shiftmod = (mod & SHIFTMOD);
    runmode = ((mod & CTRLMOD) || (!x->gl_edit));
    altmod = (mod & ALTMOD);
    rightclick = (mod & RIGHTCLICK);

    THISED->canvas_undo_already_set_move = 0;

    if (doit && xpix == THISED->canvas_upx && ypix == THISED->canvas_upy &&
        sys_getrealtime() - THISED->canvas_upclicktime < DCLICKINTERVAL)
            doubleclick = 1;
    x->gl_editor->e_lastmoved = 0;

    if (doit)
    {
        /* if keyboard was grabbed, notify grabber and cancel the grab */
        if (x->gl_editor->e_grab && x->gl_editor->e_keyfn)
        {
            (*x->gl_editor->e_keyfn)(x->gl_editor->e_grab, &s_, 0);
            undarken_if_gatom(x->gl_editor->e_grab);
            glist_grab(x, 0, 0, 0, 0, 0);
        }
        x->gl_editor->e_grab = 0;
        x->gl_editor->e_onmotion = MA_NONE;
    }

        /* do not update cursor if we're dragging  */
    if (!doit && x->gl_editor->e_onmotion != MA_NONE)
        return;
#if 0
    post("click %d %d, mod=%d, doit=%d",
        xpix, ypix, mod, doit);
#endif

    x->gl_editor->e_xwas = xpix;
    x->gl_editor->e_ywas = ypix;

    if (rightclick)
    {
            /* right click.  "doit" is then true as well. */
        canvas_rightclick(x, xpix, ypix, mod);
        return;
    }

        /* is the mouse over a text, in either an object box or a scalar? */
    rtext = rtext_findhit(x, xpix, ypix, &hitobj, &hitscalar,
        &hitwords, &hitdrawtext);

       /* did we click when there's an active text? */
    if (doit && x->gl_editor->e_textedfor)
    {
            /* did we click inside the active text? */
        if (rtext == x->gl_editor->e_textedfor)
        {
            int x1, y1, x2, y2;
            rtext_getrect(rtext, &x1, &y1, &x2, &y2);
            rtext_mouse(glist_textedfor(x), xpix - x1, ypix - y1,
                (shiftmod? RTEXT_SHIFT :
                    (doubleclick ? RTEXT_DBL : RTEXT_DOWN)));
            x->gl_editor->e_onmotion = MA_DRAGTEXT;
            x->gl_editor->e_xwas = x1;
            x->gl_editor->e_ywas = y1;
            return;
        }
            /* otherwise we clicked away from an active text - handle it inside
            g_rtext.c and fall through to rest of routine */
        else rtext_unmouse(x->gl_editor->e_textedfor);
            /* just in case that made rtext disappear, which seems unlikely: */
        rtext = rtext_findhit(x, xpix, ypix, &hitobj, &hitscalar,
            &hitwords, &hitdrawtext);
    }
        /* if not a click but in the editing box, set cursor and return */
    else if (!doit && x->gl_editor->e_textedfor &&
        rtext == x->gl_editor->e_textedfor)
    {
        canvas_setcursor(x, CURSOR_RUNMODE_NOTHING);
        return;
    }
        /* did we click in run mode inside a text which is not active? */
    if (doit && runmode && rtext && rtext != x->gl_editor->e_textedfor)
    {
            /* double clicking on an atom activates the text */
        if (hitobj && hitobj->te_type == T_ATOM && doubleclick)
        {
            undarken_if_gatom(&hitobj->te_g);
            /* gobj_activate(&hitobj->te_g, x, 1); */
            rtext_activate(rtext, 1);
            return;
        }
        if (hitscalar)   /* hit a scalar */
        {
            t_template *template =
                template_findbyname(hitscalar->sc_template);
            if (doubleclick)
            {
                rtext_activate(rtext, 1);
                return;
            }
            scalar_click(&hitscalar->sc_gobj, rtext_getglist(rtext),
                xpix, ypix, shiftmod, altmod, doubleclick, 1);
            return;
        }
    }

    if (runmode) /* left mouse click */
    {
        for (hitbox = x->gl_list; hitbox; hitbox = hitbox->g_next)
        {
                /* check if the object wants to be clicked.  If it's not a
                scalar, check box bounds first.  Scalars will check
                click location against their particular assortments of
                hot points. */
            if ((hitbox->g_pd == scalar_class ||
                canvas_hitbox(x, hitbox, xpix, ypix, &x1, &y1, &x2, &y2, 0))
                    && (clickreturned = gobj_click(hitbox, x, xpix, ypix,
                        shiftmod, ((mod & CTRLMOD) && (!x->gl_edit)) || altmod,
                            doubleclick, doit)))
                                break;
        }
        if (!doit)
        {
            if (hitbox)
                canvas_setcursor(x, clickreturned);
            else canvas_setcursor(x, CURSOR_RUNMODE_NOTHING);
        }
        return;
    }
        /* if not a runmode left click, fall here. */
    hitbox = canvas_findhitbox(x, xpix, ypix, &x1, &y1, &x2, &y2);
    hitobj = (hitbox ? pd_checkobject(&hitbox->g_pd) : 0);
    if (hitbox)
    {
        if (shiftmod)
        {
            if (doit)
            {
                t_rtext *rt;
                if (hitobj && (rt = glist_textedfor(x)) &&
                    rt == glist_getrtext(x, hitobj, 0))
                {
                    rtext_mouse(rt, xpix - x1, ypix - y1, RTEXT_SHIFT);
                    x->gl_editor->e_onmotion = MA_DRAGTEXT;
                    x->gl_editor->e_xwas = x1;
                    x->gl_editor->e_ywas = y1;
                }
                else
                {
                    if (glist_isselected(x, hitbox))
                        glist_deselect(x, hitbox);
                    else glist_select(x, hitbox);
                }
            }
        }
        else
        {
            int noutlet;
            int out_activeminh = OHEIGHT  * x->gl_zoom;
            int out_activemaxh = (y2 - y1) / 8;
            int out_activeheight = OHEIGHT * 2 * x->gl_zoom;
            if (out_activeheight > out_activemaxh)
                out_activeheight = out_activemaxh;
            if (out_activeheight < out_activeminh)
                out_activeheight = out_activeminh;
                /* resize? only for "true" text boxes or canvases */
            if (xpix >= x2-4 && ypix < y2-4 && hitobj &&
                    (hitobj->te_pd->c_wb == &text_widgetbehavior ||
                    hitobj->te_type == T_ATOM ||
                    pd_checkglist(&hitobj->te_pd)))
            {
                if (doit)
                {
                    if (!glist_isselected(x, hitbox))
                    {
                        glist_noselect(x);
                        glist_select(x, hitbox);
                    }
                    x->gl_editor->e_onmotion = MA_RESIZE;
                    x->gl_editor->e_xwas = x1;
                    x->gl_editor->e_ywas = y1;
                    x->gl_editor->e_xnew = xpix;
                    x->gl_editor->e_ynew = ypix;
                    canvas_undo_add(x, UNDO_APPLY, "resize",
                        canvas_undo_set_apply(x, glist_getindex(x, hitbox)));
                }
                else canvas_setcursor(x, CURSOR_EDITMODE_RESIZE);
            }
                /* look for an outlet */
            else if (hitobj && (noutlet = obj_noutlets(hitobj)) &&
                ypix >= y2 - out_activeheight)
            {
                int width = x2 - x1;
                int iow = IOWIDTH * x->gl_zoom;
                int nout1 = (noutlet > 1 ? noutlet - 1 : 1);
                int closest = ((xpix-x1) * (nout1) + width/2)/width;
                if (noutlet == 1 || closest < noutlet)
                {
                    if (doit)
                    {
                        int issignal = obj_issignaloutlet(hitobj, closest);
                        int xout = x1 + IOMIDDLE * x->gl_zoom +
                            (noutlet > 1 ? ((width - iow) * closest)/nout1 : 0);
                        x->gl_editor->e_onmotion = MA_CONNECT;
                        x->gl_editor->e_xwas = xout;
                        x->gl_editor->e_ywas = y2;
                        pdgui_vmess("::pdtk_canvas::cords_to_foreground",
                            "ci", x, 0);
                        pdgui_vmess(0, "rcr iik iiii",
                            "pdtk_canvas_create_line", x, "x",
                            0, x->gl_zoom, THISGUI->i_foregroundcolor,
                            x->gl_editor->e_xwas, x->gl_editor->e_ywas,
                            xpix, ypix);
                    }
                    else canvas_setcursor(x, CURSOR_EDITMODE_CONNECT);
                }
                else if (doit)
                    goto nooutletafterall;
                else canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
            }
                /* not in an outlet; select and move */
            else if (doit)
            {
                t_rtext *rt;
                    /* check if the box is being text edited */
            nooutletafterall:
                if (hitobj && (rt = glist_textedfor(x)) &&
                    rt == glist_getrtext(x, hitobj, 0))
                {
                    rtext_mouse(rt, xpix - x1, ypix - y1,
                        (doubleclick ? RTEXT_DBL : RTEXT_DOWN));
                    x->gl_editor->e_onmotion = MA_DRAGTEXT;
                    x->gl_editor->e_xwas = x1;
                    x->gl_editor->e_ywas = y1;
                }
                else
                {
                        /* otherwise select and drag to displace */
                    if (!glist_isselected(x, hitbox))
                    {
                        glist_noselect(x);
                        glist_select(x, hitbox);
                    }
                    x->gl_editor->e_onmotion = MA_MOVE;
                    x->gl_editor->e_waittodrag = 1;
                }
            }
            else canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
        }
        return;
    }

        /* if in runmode, and didn't hit anything, set cursor and return */
    if (runmode)
    {
        canvas_setcursor(x, CURSOR_RUNMODE_NOTHING);
        return;
    }
        /* having failed to find a box, we try lines now. */
    if (!runmode && !altmod)
    {
        t_linetraverser t;
        t_outconnect *oc;
        t_float fx = xpix, fy = ypix;
        t_glist *glist2 = glist_getcanvas(x);
        linetraverser_start(&t, glist2);
        while ((oc = linetraverser_next(&t)))
        {
            int outindex, inindex;
            t_float lx1 = t.tr_lx1, ly1 = t.tr_ly1,
                lx2 = t.tr_lx2, ly2 = t.tr_ly2;
            t_float area = (lx2 - lx1) * (fy - ly1) -
                (ly2 - ly1) * (fx - lx1);
            t_float dsquare = (lx2-lx1) * (lx2-lx1) + (ly2-ly1) * (ly2-ly1);
            if (area * area >= 50 * dsquare) continue;
            if ((lx2-lx1) * (fx-lx1) + (ly2-ly1) * (fy-ly1) < 0) continue;
            if ((lx2-lx1) * (lx2-fx) + (ly2-ly1) * (ly2-fy) < 0) continue;
            outindex = canvas_getindex(glist2, &t.tr_ob->ob_g);
            inindex = canvas_getindex(glist2, &t.tr_ob2->ob_g);
            if (shiftmod)
            {
                int soutindex, sinindex, soutno, sinno;
                    /* if no line is selected, just add this line to
                        the selection */
                if(!x->gl_editor->e_selectedline)
                {
                    if (doit)
                    {
                        glist_selectline(glist2, oc,
                            outindex, t.tr_outno,
                            inindex, t.tr_inno);
                    }
                    canvas_setcursor(x, CURSOR_EDITMODE_DISCONNECT);
                    return;
                }
                soutindex = x->gl_editor->e_selectline_index1;
                sinindex = x->gl_editor->e_selectline_index2;
                soutno = x->gl_editor->e_selectline_outno;
                sinno = x->gl_editor->e_selectline_inno;
                    /* if the hovered line is already selected, deselect it */
                if ((outindex == soutindex) && (inindex == sinindex)
                    && (soutno == t.tr_outno) && (sinno == t.tr_inno))
                {
                    if(doit)
                        glist_deselectline(x);
                    canvas_setcursor(x, CURSOR_EDITMODE_DISCONNECT);
                    return;
                }

                    /* swap selected and hovered connection */
                if ((!x->gl_editor->e_selection)
                    && ((outindex == soutindex) || (inindex == sinindex)))
                {
                    if (doit)
                    {
                        canvas_undo_add(x, UNDO_SEQUENCE_START, "reconnect", 0);
                        canvas_disconnect_with_undo(x, soutindex, soutno,
                            sinindex, sinno);
                        canvas_disconnect_with_undo(x, outindex, t.tr_outno,
                            inindex, t.tr_inno);
                        canvas_connect_with_undo(x, outindex, t.tr_outno,
                            sinindex, sinno);
                        canvas_connect_with_undo(x, soutindex, soutno, inindex,
                            t.tr_inno);
                        canvas_undo_add(x, UNDO_SEQUENCE_END, "reconnect", 0);

                        x->gl_editor->e_selectline_index1 = soutindex;
                        x->gl_editor->e_selectline_outno = soutno;
                        x->gl_editor->e_selectline_index2 = inindex;
                        x->gl_editor->e_selectline_inno = t.tr_inno;

                        canvas_dirty(x, 1);
                    }
                    canvas_setcursor(x, CURSOR_EDITMODE_DISCONNECT);
                    return;
                }
            }
            if (!shiftmod)
            {
                    /* !shiftmode: clear selection before selecting line */
                if (doit)
                {
                    glist_noselect(x);
                    glist_selectline(glist2, oc,
                        outindex, t.tr_outno,
                        inindex, t.tr_inno);
                }
                canvas_setcursor(x, CURSOR_EDITMODE_DISCONNECT);
                return;
            }
        }
    }
    canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
    if (doit)
    {
        if (!shiftmod)
            glist_noselect(x);
        pdgui_vmess(0, "crr iiii rk rs",
            x, "create", "rectangle",
            xpix,ypix, xpix,ypix,
            "-outline", THISGUI->i_selectcolor,
            "-tags", "x");
        x->gl_editor->e_xwas = xpix;
        x->gl_editor->e_ywas = ypix;
        x->gl_editor->e_onmotion = MA_REGION;
    }
}

    /* mouse click or motion as a Pd message.  The 'which' arg
    is ignored but kept for back compatibility with anyone who might
    have reverse-engineered the GUI */
void canvas_mouse(t_canvas *x, t_floatarg xpix, t_floatarg ypix,
    t_floatarg which, t_floatarg mod)
{
    canvas_doclick(x, xpix, ypix, mod, 1);
}

int canvas_isconnected (t_canvas *x, t_text *ob1, int n1,
    t_text *ob2, int n2)
{
    t_linetraverser t;
    t_outconnect *oc;
    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
        if (t.tr_ob == ob1 && t.tr_outno == n1 &&
            t.tr_ob2 == ob2 && t.tr_inno == n2)
                return (1);
    return (0);
}

static int canconnect(t_canvas*x, t_object *src, int nout,
    t_object *sink, int nin)
{
            /* do source and sink exist (and are not the same)?*/
    if (!src || !sink || sink == src)
        return 0;
            /* do the requested iolets exist? */
    if (nin >= obj_ninlets(sink) || (nout >= obj_noutlets(src)))
        return 0;
            /* are the objects already connected? */
    if (canvas_isconnected(x, src, nout, sink, nin))
        return 0;
    return (!obj_issignaloutlet(src, nout) || /* are the iolets compatible? */
            obj_issignalinlet(sink, nin));
}

static int tryconnect(t_canvas*x, t_object *src, int nout,
    t_object *sink, int nin)
{
    if(canconnect(x, src, nout, sink, nin))
    {
        t_outconnect *oc = obj_connect(src, nout, sink, nin);
        if(oc)
        {
            int iow = IOWIDTH * x->gl_zoom;
            int iom = IOMIDDLE * x->gl_zoom;
            int x11=0, x12=0, x21=0, x22=0;
            int y11=0, y12=0, y21=0, y22=0;
            int noutlets1, ninlets, lx1, ly1, lx2, ly2;
            char tag[128];
            sprintf(tag, "l%p", oc);
            gobj_getrect(&src->ob_g, x, &x11, &y11, &x12, &y12);
            gobj_getrect(&sink->ob_g, x, &x21, &y21, &x22, &y22);

            noutlets1 = obj_noutlets(src);
            ninlets = obj_ninlets(sink);

            lx1 = x11 + (noutlets1 > 1 ?
                             ((x12-x11-iow) * nout)/(noutlets1-1) : 0)
                + iom;
            ly1 = y12;
            lx2 = x21 + (ninlets > 1 ?
                             ((x22-x21-iow) * nin)/(ninlets-1) : 0)
                + iom;
            ly2 = y21;
            pdgui_vmess(0, "rcr ii k iiii",
                "pdtk_canvas_create_patchcord", glist_getcanvas(x), tag,
                    0, (obj_issignaloutlet(src, nout) ? 2 : 1) * x->gl_zoom,
                        THISGUI->i_foregroundcolor,
                            lx1,ly1, lx2,ly2);
            canvas_undo_add(x, UNDO_CONNECT, "connect",
                canvas_undo_set_connect(x,
                    canvas_getindex(x, &src->ob_g), nout,
                    canvas_getindex(x, &sink->ob_g), nin));
            canvas_dirty(x, 1);
            return 1;
        }
    }
    return 0;
}

static void canvas_doconnect(t_canvas *x, int xpos, int ypos, int mod, int doit)
{
    int x11=0, y11=0, x12=0, y12=0;
    t_gobj *y1;
    int x21=0, y21=0, x22=0, y22=0;
    t_gobj *y2;
    int xwas = x->gl_editor->e_xwas,
        ywas = x->gl_editor->e_ywas;
#if 0
    post("canvas_doconnect(%p, %d, %d, %d, %d)", x, xpos, ypos, mod, doit);
#endif
    if (doit) {
        pdgui_vmess("::pdtk_canvas::cords_to_foreground", "ci", x, 1);
        pdgui_vmess(0, "rcr", "pdtk_canvas_delete", x, "x");
    }
    else
        pdgui_vmess(0, "crs iiii",
            x, "coords", "x",
            x->gl_editor->e_xwas,x->gl_editor->e_ywas, xpos,ypos);

    if ((y1 = canvas_findhitbox(x, xwas, ywas, &x11, &y11, &x12, &y12))
        && (y2 = canvas_findhitbox(x, xpos, ypos, &x21, &y21, &x22, &y22)))
    {
        t_object *ob1 = pd_checkobject(&y1->g_pd);
        t_object *ob2 = pd_checkobject(&y2->g_pd);
        int noutlet1, ninlet2;
        if (ob1 && ob2 && ob1 != ob2 &&
            (noutlet1 = obj_noutlets(ob1))
            && (ninlet2 = obj_ninlets(ob2)))
        {
            int width1 = x12 - x11, closest1, hotspot1;
            int width2 = x22 - x21, closest2, hotspot2;

            if (noutlet1 > 1)
            {
                closest1 = ((xwas-x11) * (noutlet1-1) + width1/2)/width1;
                hotspot1 = x11 +
                    (width1 - IOWIDTH) * closest1 / (noutlet1-1);
            }
            else closest1 = 0, hotspot1 = x11;

            if (ninlet2 > 1)
            {
                closest2 = ((xpos-x21) * (ninlet2-1) + width2/2)/width2;
                hotspot2 = x21 +
                    (width2 - IOWIDTH) * closest2 / (ninlet2-1);
            }
            else closest2 = 0, hotspot2 = x21;

            if (closest1 >= noutlet1)
                closest1 = noutlet1 - 1;
            if (closest2 >= ninlet2)
                closest2 = ninlet2 - 1;

            if (canvas_isconnected (x, ob1, closest1, ob2, closest2))
            {
                canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
                return;
            }
            if (obj_issignaloutlet(ob1, closest1) &&
                !obj_issignalinlet(ob2, closest2))
            {
                if (doit)
                    pd_error(0,
                        "can't connect audio signal outlet to nonsignal inlet");
                canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
                return;
            }
            if (doit)
            {
                t_selection *sel;
                int selmode = 0;
                canvas_undo_add(x, UNDO_SEQUENCE_START, "connect", 0);
                tryconnect(x, ob1, closest1, ob2, closest2);
                canvas_dirty(x, 1);
                    /* now find out if either ob1 xor ob2 are part of the
                     * selection,
                     * and if so, connect the rest of the selection as well */
                if (mod & SHIFTMOD)
                    /* intelligent patching needs to be activated by
                                         modifier key */
                    selmode = glist_isselected(x, &ob1->ob_g) +
                        2 * glist_isselected(x, &ob2->ob_g);
                switch(selmode) {
                case 3: /* both source and sink are selected */
                        /* if only the source & sink are selected, keep
                             connecting them */
                    if(0 == x->gl_editor->e_selection->sel_next->sel_next)
                    {
                        int i, j;
                        for(i=closest1, j=closest2; (i < noutlet1) &&
                            (j < ninlet2); i++, j++ )
                                tryconnect(x, ob1, i, ob2, j);
                    }
                    else
                            /* if other objects are selected as well, connect
                             * those either as sources or sinks, whichever
                             * allows for more connections
                             */
                    {
                            /* get a left-right sorted list of all selected
                             * objects (but the already connected ones)
                             * count the possibles sinks and sources
                             */
                        int mode = 0;
                        int i;
                        int sinks = 0, sources = 0;
                        t_float ysinks = 0., ysources = 0.;
                        int msgout = !obj_issignaloutlet(ob1, closest1);
                        int sigin = obj_issignalinlet(ob2, closest2);
                        t_selection*sortedsel = 0;
                            /* sort the selected objects from left-right */
                        for(sel = x->gl_editor->e_selection, i=1; sel;
                            sel = sel->sel_next, i++)
                        {
                            t_object*ob = pd_checkobject(&sel->sel_what->g_pd);
                            t_selection*sob = 0;
                                /* skip illegal objects and the reference
                                     source&sink */
                            if (!ob || (ob1 == ob) || (ob2 == ob))
                                continue;

                            if (canconnect(x, ob1, closest1 + 1 + sinks, ob,
                                 closest2))
                            {
                                sinks += 1;
                                ysinks += ob->te_ypix;
                            }
                            if (canconnect(x, ob, closest1, ob2,
                                closest2 + 1 + sources))
                            {
                                sources += 1;
                                ysources += ob->te_ypix;
                            }

                                /* insert the object into the sortedsel list */
                            if((sob = getbytes(sizeof(*sob)))) {
                                t_selection*s, *slast=0;
                                sob->sel_what = &ob->te_g;
                                for(s=sortedsel; s; s=s->sel_next)
                                {
                                    t_object*o =
                                        pd_checkobject(&s->sel_what->g_pd);
                                    if(!o) continue;
                                    if((ob->te_xpix < o->te_xpix) ||
                                       ((ob->te_xpix == o->te_xpix) &&
                                           (ob->te_ypix < o->te_ypix)))
                                    {
                                        sob->sel_next = s;
                                        if(slast)
                                            slast->sel_next = sob;
                                        else
                                            sortedsel = sob;
                                        break;
                                    }
                                    slast=s;
                                }
                                if(slast)
                                    slast->sel_next = sob;
                                else
                                    sortedsel = sob;
                            }
                        }
                            /* try to maximize connections */
                        mode = (sinks > sources);

                            /* maximizing failed, so prefer to connect from
                                top to bottom */
                        if (sinks && (sinks == sources)) {
                            mode = ((ysinks - ob1->te_ypix) / sinks) >
                                ((ysources - ob2->te_ypix) / sources) * -1.;
                        }

                        sinks = 0;
                        sources = 0;
                        if (mode)
                            for(sel=sortedsel; ((closest1 + 1 + sinks) <
                                 noutlet1) && sel; sel=sel->sel_next)
                            {
                                sinks += tryconnect(x, ob1,
                                    closest1 + 1 + sinks,
                                        pd_checkobject(&sel->sel_what->g_pd),
                                            closest2);
                            }
                        else
                            for(sel=sortedsel; ((closest2 + 1 + sources) <
                                 ninlet2) && sel; sel=sel->sel_next)
                            {
                                sources += tryconnect(x, pd_checkobject(
                                    &sel->sel_what->g_pd), closest1,
                                        ob2, closest2 + 1 + sources);
                            }

                            /* free the sorted list of selections */
                        for(sel=sortedsel; sel; )
                        {
                            t_selection*s = sel->sel_next;
                            freebytes(sel, sizeof(*sel));
                            sel = s;
                        }
                    }
                    break;
                case 1: /* source(s) selected */
                    for(sel = x->gl_editor->e_selection; sel;
                        sel = sel->sel_next)
                    {
                        t_object*selo = pd_checkobject(&sel->sel_what->g_pd);
                        if (!selo || selo == ob1)
                            continue;
                        tryconnect(x, selo, closest1, ob2, closest2);
                    }
                    break;
                case 2: /* sink(s) selected */
                    for(sel = x->gl_editor->e_selection; sel;
                        sel = sel->sel_next)
                    {
                        t_object*selo = pd_checkobject(&sel->sel_what->g_pd);
                        if (!selo || selo == ob2)
                            continue;
                        tryconnect(x, ob1, closest1, selo, closest2);
                    }
                    break;
                default: break;
                }
                canvas_undo_add(x, UNDO_SEQUENCE_END, "connect", 0);
            }
            else canvas_setcursor(x, CURSOR_EDITMODE_CONNECT);
            return;
        }
    }
    canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
}

void canvas_selectinrect(t_canvas *x, int lox, int loy, int hix, int hiy)
{
    t_gobj *y;
    for (y = x->gl_list; y; y = y->g_next)
    {
        int x1, y1, x2, y2;
        gobj_getrect(y, x, &x1, &y1, &x2, &y2);
        if (hix >= x1 && lox <= x2 && hiy >= y1 && loy <= y2
            && !glist_isselected(x, y))
                glist_select(x, y);
    }
}

static void canvas_doregion(t_canvas *x, int xpos, int ypos, int doit)
{
    if (doit)
    {
        int lox, loy, hix, hiy;
        if (x->gl_editor->e_xwas < xpos)
            lox = x->gl_editor->e_xwas, hix = xpos;
        else hix = x->gl_editor->e_xwas, lox = xpos;
        if (x->gl_editor->e_ywas < ypos)
            loy = x->gl_editor->e_ywas, hiy = ypos;
        else hiy = x->gl_editor->e_ywas, loy = ypos;
        canvas_selectinrect(x, lox, loy, hix, hiy);
        pdgui_vmess(0, "rcr", "pdtk_canvas_delete", x, "x");
        x->gl_editor->e_onmotion = MA_NONE;
    }
    else
        pdgui_vmess(0, "crs iiii",
            x, "coords", "x",
            x->gl_editor->e_xwas,x->gl_editor->e_ywas, xpos,ypos);
}

void canvas_mouseup(t_canvas *x,
    t_floatarg fxpos, t_floatarg fypos, t_floatarg fwhich,
    t_floatarg fmod)
{
    int xpos = fxpos, ypos = fypos, which = fwhich;
    int mod = fmod;
#if 0
    post("mouseup %d %d %d %d", xpos, ypos, which, mod);
#endif
    if (!x->gl_editor)
    {
        bug("editor");
        return;
    }

    THISED->canvas_upclicktime = sys_getrealtime();
    THISED->canvas_upx = xpos;
    THISED->canvas_upy = ypos;

    if (x->gl_editor->e_onmotion == MA_CONNECT)
        canvas_doconnect(x, xpos, ypos, mod, 1);
    else if (x->gl_editor->e_onmotion == MA_REGION)
        canvas_doregion(x, xpos, ypos, 1);
    else if ((x->gl_editor->e_onmotion == MA_MOVE ||
              x->gl_editor->e_onmotion == MA_RESIZE))
    {
            /* if there's only one text item selected activate the text. */
        if (x->gl_editor->e_selection &&
            !(x->gl_editor->e_selection->sel_next))
        {
            gobj_activate(x->gl_editor->e_selection->sel_what, x, 1);
        }
    }
    else if (x->gl_editor->e_onmotion == MA_PASSOUT)
    {
        if (!x->gl_editor->e_motionfn)
            bug("e_motionfn");
        (*x->gl_editor->e_motionfn)(&x->gl_editor->e_grab->g_pd,
            xpos - x->gl_editor->e_xwas, ypos - x->gl_editor->e_ywas, 1);
    }
    x->gl_editor->e_onmotion = MA_NONE;
}

    /* displace the selection by (dx, dy) pixels */
static void canvas_displaceselection(t_canvas *x, int dx, int dy)
{
    t_selection *y;
    int resortin = 0, resortout = 0;
    if (x->gl_editor->e_selection && !THISED->canvas_undo_already_set_move)
    {
        canvas_undo_add(x, UNDO_MOTION, "motion", canvas_undo_set_move(x, 1));
        THISED->canvas_undo_already_set_move = 1;
    }
    for (y = x->gl_editor->e_selection; y; y = y->sel_next)
    {
        t_class *cl = pd_class(&y->sel_what->g_pd);
        gobj_displace(y->sel_what, x, dx, dy);
        if (cl == vinlet_class) resortin = 1;
        else if (cl == voutlet_class) resortout = 1;
    }
    if (resortin) canvas_resortinlets(x);
    if (resortout) canvas_resortoutlets(x);
    pdgui_vmess("pdtk_canvas_getscroll", "c", x);
    if (x->gl_editor->e_selection)
        canvas_dirty(x, 1);
}

    /* this routine is called whenever a key is pressed or released.  "x"
       may be zero if there's no current canvas.  The first argument is true or
       false for down/up; the second one is either a symbolic key name (e.g.,
       "Right" or an Ascii key number.  The third is the shift key. */
void canvas_key(t_canvas *x, t_symbol *s, int ac, t_atom *av)
{
    int keynum, fflag;
    t_symbol *gotkeysym;

    int down, shift;

    if (ac < 3)
        return;

    THISED->canvas_undo_already_set_move = 0;
    down = (atom_getfloat(av) != 0);  /* nonzero if it's a key down */
    shift = (atom_getfloat(av+2) != 0);  /* nonzero if shift-ed */
    if (av[1].a_type == A_SYMBOL)
        gotkeysym = av[1].a_w.w_symbol;
    else if (av[1].a_type == A_FLOAT)
    {
        char buf[UTF8_MAXBYTES1];
        switch((int)(av[1].a_w.w_float))
        {
        case 8:  gotkeysym = gensym("BackSpace"); break;
        case 9:  gotkeysym = gensym("Tab"); break;
        case 10: gotkeysym = gensym("Return"); break;
        case 27: gotkeysym = gensym("Escape"); break;
        case 32: gotkeysym = gensym("Space"); break;
        case 127:gotkeysym = gensym("Delete"); break;
        default:
          /*-- moo: assume keynum is a Unicode codepoint; encode as UTF-8 --*/
            u8_wc_toutf8_nul(buf, (UCS4)(av[1].a_w.w_float));
            gotkeysym = gensym(buf);
        }
    }
    else gotkeysym = gensym("?");
    fflag = (av[0].a_type == A_FLOAT ? av[0].a_w.w_float : 0);
    keynum = (av[1].a_type == A_FLOAT ? av[1].a_w.w_float : 0);
    if (keynum == '{' || keynum == '}')
    {
        post("keycode %d: dropped", (int)keynum);
        return;
    }
#if 0
    post("keynum %d, down %d", (int)keynum, down);
#endif
    if (keynum == '\r') keynum = '\n';
    if (av[1].a_type == A_SYMBOL &&
        !strcmp(av[1].a_w.w_symbol->s_name, "Return"))
            keynum = '\n';
        /* alias Apple key numbers to symbols.  This is done unconditionally,
           not just if we're on an Apple, just in case the GUI is remote. */
    if (keynum == 30 || keynum == 63232)
        keynum = 0, gotkeysym = gensym("Up");
    else if (keynum == 31 || keynum == 63233)
        keynum = 0, gotkeysym = gensym("Down");
    else if (keynum == 28 || keynum == 63234)
        keynum = 0, gotkeysym = gensym("Left");
    else if (keynum == 29 || keynum == 63235)
        keynum = 0, gotkeysym = gensym("Right");
    else if (keynum == 63273)
        keynum = 0, gotkeysym = gensym("Home");
    else if (keynum == 63275)
        keynum = 0, gotkeysym = gensym("End");
    else if (keynum == 63276)
        keynum = 0, gotkeysym = gensym("Prior");
    else if (keynum == 63277)
        keynum = 0, gotkeysym = gensym("Next");
    else if (keynum == 63236)
        keynum = 0, gotkeysym = gensym("F1");
    else if (keynum == 63237)
        keynum = 0, gotkeysym = gensym("F2");
    else if (keynum == 63238)
        keynum = 0, gotkeysym = gensym("F3");
    else if (keynum == 63239)
        keynum = 0, gotkeysym = gensym("F4");
    else if (keynum == 63240)
        keynum = 0, gotkeysym = gensym("F5");
    else if (keynum == 63241)
        keynum = 0, gotkeysym = gensym("F6");
    else if (keynum == 63242)
        keynum = 0, gotkeysym = gensym("F7");
    else if (keynum == 63243)
        keynum = 0, gotkeysym = gensym("F8");
    else if (keynum == 63244)
        keynum = 0, gotkeysym = gensym("F9");
    else if (keynum == 63245)
        keynum = 0, gotkeysym = gensym("F10");
    else if (keynum == 63246)
        keynum = 0, gotkeysym = gensym("F11");
    else if (keynum == 63247)
        keynum = 0, gotkeysym = gensym("F12");
    if (gensym("#key")->s_thing && down)
        pd_float(gensym("#key")->s_thing, (t_float)keynum);
    if (gensym("#keyup")->s_thing && !down)
        pd_float(gensym("#keyup")->s_thing, (t_float)keynum);
    if (gensym("#keyname")->s_thing)
    {
        t_atom at[2];
        at[0] = av[0];
        SETFLOAT(at, down);
        SETSYMBOL(at+1, gotkeysym);
        pd_list(gensym("#keyname")->s_thing, 0, 2, at);
    }
    if (!x || !x->gl_editor)  /* if that 'invis'ed the window,  stop. */
        return;
    if (x && down)
    {
            /* cancel any dragging action */
        if (x->gl_editor->e_onmotion == MA_MOVE)
            x->gl_editor->e_onmotion = MA_NONE;
            /* if an object has "grabbed" keys just send them on */
        if (x->gl_editor->e_grab
            && x->gl_editor->e_keyfn && keynum)
                (* x->gl_editor->e_keyfn)
                    (x->gl_editor->e_grab, gotkeysym, (t_float)keynum);
            /* if a text editor is open send the key on, as long as
               it is either "real" (has a key number) or else is an arrow key. */
        else if (glist_textedfor(x) && (keynum
            || !strcmp(gotkeysym->s_name, "Home")
            || !strcmp(gotkeysym->s_name, "End")
            || !strcmp(gotkeysym->s_name, "Up")
            || !strcmp(gotkeysym->s_name, "Down")
            || !strcmp(gotkeysym->s_name, "Left")
            || !strcmp(gotkeysym->s_name, "Right")))
        {
                /* if the typed object is also the selected object in the
                 canvas and is an abstraction, and if its text has not been
                 modified yet, then ask whether to discard any changes inside
                  it. */
            if (x->gl_editor->e_selection &&
                pd_class(&x->gl_editor->e_selection->sel_what->g_pd)
                    == canvas_class && !x->gl_editor->e_textdirty
                    /* only ask if the keystroke would really modify the text */
                        && keynum
            )
            {
                t_gobj *selected_canvas = x->gl_editor->e_selection->sel_what;
                if(canvas_undo_confirmdiscard(selected_canvas))
                    return;
            }
                /* send the key to the box's editor */
            if (!x->gl_editor->e_textdirty)
            {
                canvas_setundo(x, canvas_undo_cut,
                    canvas_undo_set_cut(x, UCUT_TEXT), "typing");
            }
            rtext_key(glist_textedfor(x), (int)keynum, gotkeysym);
            if (x->gl_editor->e_textdirty)
                canvas_dirty(x, 1);
        }
            /* check for backspace or clear */
        else if (keynum == 8 || keynum == 127)
        {
            if (x->gl_editor->e_selection)
                canvas_undo_add(x, UNDO_SEQUENCE_START, "clear", 0);
            if (x->gl_editor->e_selectedline)
                canvas_clearline(x);
            if (x->gl_editor->e_selection)
            {
                canvas_undo_add(x, UNDO_CUT, "clear",
                    canvas_undo_set_cut(x, UCUT_CLEAR));
                canvas_doclear(x);
                canvas_undo_add(x, UNDO_SEQUENCE_END, "clear", 0);
            }
        }
            /* check for arrow keys */
        else if (!strcmp(gotkeysym->s_name, "Up"))
            canvas_displaceselection(x, 0, shift ? -10 : -1);
        else if (!strcmp(gotkeysym->s_name, "Down"))
            canvas_displaceselection(x, 0, shift ? 10 : 1);
        else if (!strcmp(gotkeysym->s_name, "Left"))
            canvas_displaceselection(x, shift ? -10 : -1, 0);
        else if (!strcmp(gotkeysym->s_name, "Right"))
            canvas_displaceselection(x, shift ? 10 : 1, 0);
        else if ((MA_CONNECT == x->gl_editor->e_onmotion)
            && (CURSOR_EDITMODE_CONNECT == THISED->canvas_cursorwas)
                 && !strncmp(gotkeysym->s_name, "Shift", 5))
        {
                /* <Shift> while in connect-mode: create connection... */
            canvas_doconnect(x, x->gl_editor->e_xnew, x->gl_editor->e_ynew,
                1, 1);
                /* ... and continue in connect-mode */
            canvas_doclick(x, x->gl_editor->e_xwas, x->gl_editor->e_ywas,
                0, 1);

        }
    }
        /* if control key goes up or down, and if we're in edit mode, change
           cursor to indicate how the click action changes */
    if (x && keynum == 0 && x->gl_edit &&
        !strncmp(gotkeysym->s_name, "Control", 7))
            canvas_setcursor(x, down ?
                CURSOR_RUNMODE_NOTHING :CURSOR_EDITMODE_NOTHING);
}

static void delay_move(t_gobj *client, t_glist *glist)
{
    t_canvas *x = (t_canvas *)client;
    int incx = x->gl_editor->e_xnew - x->gl_editor->e_xwas,
        incy = x->gl_editor->e_ynew - x->gl_editor->e_ywas;
        /* insist on at elast 2 pixel displacement to avoid accidental
        displacements of objects you only meant to select */
    if (x->gl_editor->e_waittodrag &&
        incx > -2 && incx < 2 && incy > -2 && incy < 2)
            return;
    incx /= x->gl_zoom;
    incy /= x->gl_zoom;

    x->gl_editor->e_waittodrag = 0;
    if (incx || incy)
        canvas_displaceselection(x, incx, incy);
    x->gl_editor->e_xwas += incx * x->gl_zoom;
    x->gl_editor->e_ywas += incy * x->gl_zoom;
}

    /* defined in g_text.c: */
extern void text_getfont(t_text *x, t_glist *thisglist,
    int *fwidthp, int *fheightp, int *guifsize);

void canvas_motion(t_canvas *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg fmod)
{
#if 0
    post("motion %d %d %d %d", (int)xpos, (int)ypos, (int)fmod,
        x->gl_editor->e_onmotion);
#endif
    int mod = fmod;
    if (!x->gl_editor)
    {
        bug("editor");
        return;
    }
    glist_setlastxy(x, xpos, ypos);
    if (x->gl_editor->e_onmotion == MA_MOVE)
    {
        sys_queuegui(x, glist_getcanvas(x), delay_move);
        x->gl_editor->e_xnew = xpos;
        x->gl_editor->e_ynew = ypos;
    }
    else if (x->gl_editor->e_onmotion == MA_REGION)
        canvas_doregion(x, xpos, ypos, 0);
    else if (x->gl_editor->e_onmotion == MA_CONNECT)
    {
        canvas_doconnect(x, xpos, ypos, mod, 0);
        x->gl_editor->e_xnew = xpos;
        x->gl_editor->e_ynew = ypos;
    }
    else if (x->gl_editor->e_onmotion == MA_PASSOUT)
    {
        if (!x->gl_editor->e_motionfn)
            bug("e_motionfn");
        (*x->gl_editor->e_motionfn)(&x->gl_editor->e_grab->g_pd,
            xpos - x->gl_editor->e_xwas, ypos - x->gl_editor->e_ywas, 0);
        x->gl_editor->e_xwas = xpos;
        x->gl_editor->e_ywas = ypos;
    }
    else if (x->gl_editor->e_onmotion == MA_DRAGTEXT)
    {
        if (glist_textedfor(x))
            rtext_mouse(glist_textedfor(x), xpos - x->gl_editor->e_xwas,
                ypos - x->gl_editor->e_ywas, RTEXT_DRAG);
    }
    else if (x->gl_editor->e_onmotion == MA_RESIZE)
    {
        int x11=0, y11=0, x12=0, y12=0;
        t_gobj *y1;
        if ((y1 = canvas_findhitbox(x,
            x->gl_editor->e_xwas, x->gl_editor->e_ywas,
                &x11, &y11, &x12, &y12)))
        {
            int wantwidth = xpos - x11;
            t_object *ob = pd_checkobject(&y1->g_pd);
            if (ob && ((ob->te_pd->c_wb == &text_widgetbehavior) ||
                ob->te_type == T_ATOM ||
                    (pd_checkglist(&ob->te_pd) &&
                     !((t_canvas *)ob)->gl_isgraph)))
            {
                int fwidth, fheight, guifsize;
                text_getfont(ob, x, &fwidth, &fheight, &guifsize);
                wantwidth = wantwidth / fwidth;
                if (wantwidth < 1)
                    wantwidth = 1;
                ob->te_width = wantwidth;
                gobj_vis(y1, x, 0);
                canvas_fixlinesfor(x, ob);
                gobj_vis(y1, x, 1);
            }
            else if (ob && ob->ob_pd == canvas_class)
            {
                gobj_vis(y1, x, 0);
                ((t_canvas *)ob)->gl_pixwidth += xpos - x->gl_editor->e_xnew;
                ((t_canvas *)ob)->gl_pixheight += ypos - x->gl_editor->e_ynew;
                x->gl_editor->e_xnew = xpos;
                x->gl_editor->e_ynew = ypos;
                canvas_fixlinesfor(x, ob);
                gobj_vis(y1, x, 1);
            }
            else post("not resizable");
        }
    }
    else canvas_doclick(x, xpos, ypos, mod, 0);

    x->gl_editor->e_lastmoved = 1;
}

void canvas_startmotion(t_canvas *x)
{
    int xval, yval;
    if (!x->gl_editor) return;
    glist_getnextxy(x, &xval, &yval);
    if (xval == 0 && yval == 0) return;
    x->gl_editor->e_onmotion = MA_MOVE;
    x->gl_editor->e_waittodrag = 0;
    x->gl_editor->e_xwas = xval;
    x->gl_editor->e_ywas = yval;
}

/* ----------------------------- window stuff ----------------------- */
extern int sys_perf;

void canvas_print(t_canvas *x, t_symbol *s)
{
    const char*filename = (*s->s_name)?(s->s_name):"x.ps";
    pdgui_vmess(0, "cr rs",
        x, "postscript",
        "-file", filename);
}

    /* find the innermost dirty sub-glist, if any, of this one
        (including itself) */
static t_glist *glist_finddirty(t_glist *x)
{
    t_gobj *g;
    t_glist *g2;
    for (g = x->gl_list; g; g = g->g_next)
        if (pd_class(&g->g_pd) == canvas_class &&
            (g2 = glist_finddirty((t_glist *)g)))
                return (g2);

    if (x->gl_env && x->gl_dirty)
        return (x);
    else
        return (0);
}

    /* quit, after calling glist_finddirty() on all toplevels and verifying
       the user really wants to discard changes  */
void glob_verifyquit(void *dummy, t_floatarg f)
{
    t_glist *g, *g2;
    const char*msg = "really quit?";
        /* find all root canvases */
    for (g = pd_getcanvaslist(); g; g = g->gl_next)
        if ((g2 = glist_finddirty(g)))
        {
            t_atom backmsg[2];
            char buf[40];
            sprintf(buf, ".x%lx", g2);
            SETSYMBOL(backmsg+0, gensym("menuclose"));
            SETFLOAT (backmsg+1, 3);

            canvas_vis(g2, 1);
            pdgui_vmess("pdtk_canvas_menuclose", "^m",
                        canvas_getrootfor(g2),
                        gensym(buf), 2, backmsg);
            return;
        }
    if (f == 0 && sys_perf)
        pdgui_vmess("pdtk_check", "r Sss",
            ".pdwindow",
            1, &msg,
            "pd quit", "yes");
    else glob_exit(0, 0);
}

    /* close a window (or possibly quit Pd), checking for dirty flags.
       The "force" parameter is interpreted as follows:
       0 - request from GUI to close, verifying whether clean or dirty
       1 - request from GUI to close, no verification
       2 - verified - mark this one clean, then continue as in 1
       3 - verified - mark this one clean, then verify-and-quit
    */
void canvas_menuclose(t_canvas *x, t_floatarg fforce)
{
    int force = fforce;
    t_glist *g;
    t_atom backmsg[2];
    char buf[40];
    SETSYMBOL(backmsg+0, gensym("menuclose"));
    SETSYMBOL(backmsg+1, 0);
    if (x->gl_owner && (force == 0 || force == 1))
        canvas_vis(x, 0);   /* if subpatch, just invis it */
    else if (force == 0)
    {
        g = glist_finddirty(x);
        if (g)
        {
            sprintf(buf, ".x%lx", g);
            SETFLOAT(backmsg+1, 2);

            vmess(&g->gl_pd, gensym("menu-open"), "");
            pdgui_vmess("pdtk_canvas_menuclose", "^m",
                canvas_getrootfor(g), gensym(buf), 2, backmsg);
            return;
        }
        else if (sys_perf)
        {
            const char*msg = "Close this window?";
            sprintf(buf, ".x%lx", x);
            SETFLOAT(backmsg+1, 1);

            pdgui_vmess("pdtk_check", "^ Sms",
                canvas_getrootfor(x),
                1, &msg,
                gensym(buf), 2, backmsg,
                "yes");
        }
        else pd_free(&x->gl_pd);
    }
    else if (force == 1)
        pd_free(&x->gl_pd);
    else if (force == 2)
    {
        canvas_dirty(x, 0);
        while (x->gl_owner && !x->gl_isclone)
            x = x->gl_owner;
        g = glist_finddirty(x);
        if (g)
        {
            sprintf(buf, ".x%lx", g);
            SETFLOAT(backmsg+1, 2);

            vmess(&g->gl_pd, gensym("menu-open"), "");
            pdgui_vmess("pdtk_canvas_menuclose", "^m",
                        canvas_getrootfor(g), gensym(buf), 2, backmsg);
            return;
        }
        else pd_free(&x->gl_pd);
    }
    else if (force == 3)
    {
        canvas_dirty(x, 0);
        glob_verifyquit(0, 1);
    }
}

    /* put up a dialog which may call canvas_font back to do the work */
static void canvas_menufont(t_canvas *x)
{
    t_canvas *x2 = canvas_getrootfor(x);
    pdgui_stub_deleteforkey(x2);
    pdgui_stub_vnew(&x2->gl_pd, "pdtk_canvas_dofont", &x2->gl_pd,
        "i", x2->gl_font);
}

typedef void (*t_zoomfn)(void *x, t_floatarg arg1);

/* LATER, if canvas is flipped, re-scroll to preserve bottom left corner */
static void canvas_zoom(t_canvas *x, t_floatarg zoom)
{
    if (zoom != x->gl_zoom && (zoom == 1 || zoom == 2))
    {
        t_gobj *g;
        t_object *obj;
        for (g = x->gl_list; g; g = g->g_next)
            if ((obj = pd_checkobject(&g->g_pd)))
        {
                /* pass zoom message on to all objects, except canvases
                   that aren't GOP */
            t_gotfn zoommethod;
            if ((zoommethod = zgetfn(&obj->te_pd, gensym("zoom"))) &&
                (!(pd_class(&obj->te_pd) == canvas_class) ||
                 (((t_glist *)obj)->gl_isgraph)))
                    (*(t_zoomfn)zoommethod)(&obj->te_pd, zoom);
        }
        x->gl_zoom = zoom;
        if (x->gl_havewindow)
        {
            if (!glist_isgraph(x) && (x->gl_y2 < x->gl_y1))
            {
                /* if it's flipped so that y grows upward,
                fix so that zero is bottom edge as in canvas_dosetbounds() */
                t_float diff = x->gl_y1 - x->gl_y2;
                x->gl_y1 = (x->gl_screeny2 - x->gl_screeny1) * diff/x->gl_zoom;
                x->gl_y2 = x->gl_y1 - diff;
            }
            canvas_redraw(x);
        }
    }
}

    /* function to support searching */
static int atoms_match(int inargc, t_atom *inargv, int searchargc,
    t_atom *searchargv, int wholeword)
{
    int indexin, nmatched;
    for (indexin = 0; indexin <= inargc - searchargc; indexin++)
    {
        for (nmatched = 0; nmatched < searchargc; nmatched++)
        {
            t_atom *a1 = &inargv[indexin + nmatched],
                *a2 = &searchargv[nmatched];
            if (a1->a_type == A_SEMI || a1->a_type == A_COMMA)
            {
                if (a2->a_type != a1->a_type)
                    goto nomatch;
            }
            else if (a1->a_type == A_FLOAT || a1->a_type == A_DOLLAR)
            {
                if (a2->a_type != a1->a_type ||
                    a1->a_w.w_float != a2->a_w.w_float)
                        goto nomatch;
            }
            else if (a1->a_type == A_SYMBOL || a1->a_type == A_DOLLSYM)
            {
                if ((a2->a_type != A_SYMBOL && a2->a_type != A_DOLLSYM)
                    || (wholeword && a1->a_w.w_symbol != a2->a_w.w_symbol)
                    || (!wholeword &&  !strstr(a1->a_w.w_symbol->s_name,
                                        a2->a_w.w_symbol->s_name)))
                        goto nomatch;
            }
        }
        return (1);
    nomatch: ;
    }
    return (0);
}

extern int clone_get_n(t_gobj *x);
extern t_glist *clone_get_instance(t_gobj *x, int n);

    /* find an atom or string of atoms */
static int canvas_dofind(t_canvas *x, int *myindexp)
{
    t_gobj *y;
    int findargc = binbuf_getnatom(THISED->canvas_findbuf), didit = 0;
    t_atom *findargv = binbuf_getvec(THISED->canvas_findbuf);
    for (y = x->gl_list; y; y = y->g_next)
    {
        t_object *ob = 0;
        if ((ob = pd_checkobject(&y->g_pd)))
        {
            int n;
            if (atoms_match(binbuf_getnatom(ob->ob_binbuf),
                binbuf_getvec(ob->ob_binbuf), findargc, findargv,
                    THISED->canvas_find_wholeword))
            {
                if (*myindexp == THISED->canvas_find_index)
                {
                    glist_noselect(x);
                    vmess(&x->gl_pd, gensym("menu-open"), "");
                    canvas_editmode(x, 1.);
                    glist_select(x, y);
                    didit = 1;
                }
                (*myindexp)++;
            }
            if ((n = clone_get_n((t_gobj *)ob)) != 0)
            {
                int i = 0;
                    /* should we search in every clone instance,
                        or only the first one? */
                /*for(i = 0; i < n; i++)
                {*/
                    didit |= canvas_dofind((t_canvas *)
                        clone_get_instance((t_gobj *)ob, i), myindexp);
                /*}*/
            }
        }
    }
    for (y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == canvas_class)
            didit |= canvas_dofind((t_canvas *)y, myindexp);
    return (didit);
}

static void canvas_find(t_canvas *x, t_symbol *s, t_floatarg wholeword)
{
    int myindex = 0, found;
    t_symbol *decodedsym = sys_decodedialog(s);
    if (!THISED->canvas_findbuf)
        THISED->canvas_findbuf = binbuf_new();
    binbuf_text(THISED->canvas_findbuf, decodedsym->s_name,
        strlen(decodedsym->s_name));
    THISED->canvas_find_index = 0;
    THISED->canvas_find_wholeword = wholeword;
    canvas_whichfind = x;
    found = canvas_dofind(x, &myindex);
    if (found)
        THISED->canvas_find_index = 1;
    pdgui_vmess("pdtk_showfindresult", "^ iii",
        x,
        found, THISED->canvas_find_index, myindex);
}

static void canvas_find_again(t_canvas *x)
{
    int myindex = 0, found;
    if (!THISED->canvas_findbuf || !canvas_whichfind)
        return;
    found = canvas_dofind(canvas_whichfind, &myindex);
    pdgui_vmess("pdtk_showfindresult", "^ iii",
        x,
       found, ++THISED->canvas_find_index, myindex);
    if (!found)
        THISED->canvas_find_index = 0;
}

static void canvas_find_parent(t_canvas *x)
{
    if (x->gl_owner)
        canvas_vis(x->gl_owner, 1);
}

extern t_pd *message_get_responder(t_gobj *x);
extern t_class *text_class;

static int glist_dofinderror(t_glist *gl, const void *error_object)
{
    t_gobj *g;
    int n;

    for (g = gl->gl_list; g; g = g->g_next)
    {
        if (((const void *)g == error_object) ||
            (message_get_responder(g) == error_object))
        {
                /* got it... now show it. */
            glist_noselect(gl);
            canvas_vis((t_canvas *)gl, 1);
            canvas_editmode((t_canvas *)gl, 1.);
            glist_select(gl, g);
            return (1);
        }
        else if (g->g_pd == canvas_class)
        {
            if (glist_dofinderror((t_canvas *)g, error_object))
                return (1);
        }
        else if ((n = clone_get_n(g)) != 0)
        {
            int i;
            for(i = 0; i < n; i++)
            {
                if (glist_dofinderror(clone_get_instance(g, i), error_object))
                    return 1;
            }
        }
    }
    return (0);
}

void canvas_finderror(const void *error_object)
{
    t_canvas *x;
        /* find all root canvases */
    for (x = pd_getcanvaslist(); x; x = x->gl_next)
    {
        if (glist_dofinderror(x, error_object))
            return;
    }
    pd_error(0, "... sorry, I couldn't find the source of that error.");
}

void canvas_stowconnections(t_canvas *x)
{
    t_gobj *selhead = 0, *seltail = 0, *nonhead = 0, *nontail = 0, *y, *y2;
    t_linetraverser t;
    t_outconnect *oc;
    if (!x->gl_editor) return;
        /* split list to "selected" and "unselected" parts */
    for (y = x->gl_list; y; y = y2)
    {
        y2 = y->g_next;
        if (glist_isselected(x, y))
        {
            if (seltail)
            {
                seltail->g_next = y;
                seltail = y;
                y->g_next = 0;
            }
            else
            {
                selhead = seltail = y;
                seltail->g_next = 0;
            }
        }
        else
        {
            if (nontail)
            {
                nontail->g_next = y;
                nontail = y;
                y->g_next = 0;
            }
            else
            {
                nonhead = nontail = y;
                nontail->g_next = 0;
            }
        }
    }
        /* move the selected part to the end */
    if (!nonhead) x->gl_list = selhead;
    else x->gl_list = nonhead, nontail->g_next = selhead;

        /* add connections to binbuf */
    binbuf_clear(x->gl_editor->e_connectbuf);
    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
    {
        int s1 = glist_isselected(x, &t.tr_ob->ob_g);
        int s2 = glist_isselected(x, &t.tr_ob2->ob_g);
        if (s1 != s2)
            binbuf_addv(x->gl_editor->e_connectbuf, "ssiiii;",
                gensym("#X"), gensym("connect"),
                glist_getindex(x, &t.tr_ob->ob_g), t.tr_outno,
                glist_getindex(x, &t.tr_ob2->ob_g), t.tr_inno);
    }
}

void canvas_restoreconnections(t_canvas *x)
{
    t_pd *boundx = s__X.s_thing;
    s__X.s_thing = &x->gl_pd;
    binbuf_eval(x->gl_editor->e_connectbuf, 0, 0, 0);
    s__X.s_thing = boundx;
}

static t_binbuf *canvas_docopy(t_canvas *x)
{
    t_gobj *y;
    t_linetraverser t;
    t_outconnect *oc;
    t_binbuf *b = binbuf_new();
    for (y = x->gl_list; y; y = y->g_next)
    {
        if (glist_isselected(x, y))
            gobj_save(y, b);
    }
    linetraverser_start(&t, x);
    while ((oc = linetraverser_next(&t)))
    {
        if (glist_isselected(x, &t.tr_ob->ob_g)
            && glist_isselected(x, &t.tr_ob2->ob_g))
        {
            binbuf_addv(b, "ssiiii;", gensym("#X"), gensym("connect"),
                glist_selectionindex(x, &t.tr_ob->ob_g, 1), t.tr_outno,
                glist_selectionindex(x, &t.tr_ob2->ob_g, 1), t.tr_inno);
        }
    }
    return (b);
}

static void canvas_copy(t_canvas *x)
{
    if (!x->gl_editor)
        return;
    if (x->gl_editor->e_selection)
    {
        binbuf_free(THISED->copy_binbuf);
        THISED->copy_binbuf = canvas_docopy(x);
    }
    if (glist_textedfor(x))
    {
        char *buf;
        int bufsize;
        rtext_getseltext(glist_textedfor(x), &buf, &bufsize);
        pdgui_vmess("clipboard", "r", "clear");
        pdgui_vmess("clipboard", "rp",  "append", bufsize, buf);
    }
}

static void canvas_clearline(t_canvas *x)
{
    if (x->gl_editor->e_selectedline)
    {
        canvas_disconnect_with_undo(x,
            x->gl_editor->e_selectline_index1,
            x->gl_editor->e_selectline_outno,
            x->gl_editor->e_selectline_index2,
            x->gl_editor->e_selectline_inno);
        x->gl_editor->e_selectedline = 0;
        canvas_dirty(x, 1);
    }
}

static void canvas_doclear(t_canvas *x)
{
    t_gobj *y, *y2;
    int dspstate;

    dspstate = canvas_suspend_dsp();
    if (x->gl_editor->e_selectedline)
    {
        canvas_disconnect_with_undo(x,
            x->gl_editor->e_selectline_index1,
            x->gl_editor->e_selectline_outno,
            x->gl_editor->e_selectline_index2,
            x->gl_editor->e_selectline_inno);
        x->gl_editor->e_selectedline=0;
    }
        /* if text is selected, deselecting it might remake the
           object. So we deselect it and hunt for a "new" object on
           the glist to reselect. */
    if (glist_textedfor(x))
    {
        t_gobj *selwas = x->gl_editor->e_selection->sel_what;
        pd_this->pd_newest = 0;
        glist_noselect(x);
        if (pd_this->pd_newest)
        {
            for (y = x->gl_list; y; y = y->g_next)
                if (&y->g_pd == pd_this->pd_newest) glist_select(x, y);
        }
    }
    while (1)   /* this is pretty weird...  should rewrite it */
    {
        for (y = x->gl_list; y; y = y2)
        {
            y2 = y->g_next;
            if (glist_isselected(x, y))
            {
                glist_delete(x, y);
                goto next;
            }
        }
        goto restore;
    next: ;
    }
restore:
    canvas_resume_dsp(dspstate);
    canvas_dirty(x, 1);
}

static void canvas_cut(t_canvas *x)
{
    if (!x->gl_editor)  /* ignore if invisible */
        return;
    if (x->gl_editor->e_selectedline)   /* delete line */
        canvas_clearline(x);
    else if (glist_textedfor(x)) /* delete selected text in a box */
    {
        char *buf;
        int bufsize;
        rtext_getseltext(glist_textedfor(x), &buf, &bufsize);
        if (!bufsize && x->gl_editor->e_selection &&
            !x->gl_editor->e_selection->sel_next)
        {
                /* if the text is already empty, delete the box.  We
                   first clear 'textedfor' so that canvas_doclear later will
                   think the whole box was selected, not the text */
            glist_settexted(x, 0);
            goto deleteobj;
        }
        canvas_copy(x);
        rtext_key(glist_textedfor(x), 127, &s_);
        canvas_dirty(x, 1);
    }
    else if (x->gl_editor->e_selection)
    {
    deleteobj:      /* delete one or more objects */
        canvas_undo_add(x, UNDO_CUT, "cut", canvas_undo_set_cut(x, UCUT_CUT));
        canvas_copy(x);
        canvas_doclear(x);
        pdgui_vmess("pdtk_canvas_getscroll", "c", x);
    }
}

static void glist_donewloadbangs(t_glist *x)
{
    if (x->gl_editor)
    {
        t_selection *sel;
        for (sel = x->gl_editor->e_selection; sel; sel = sel->sel_next)
            if (pd_class(&sel->sel_what->g_pd) == canvas_class)
                canvas_loadbang((t_canvas *)(&sel->sel_what->g_pd));
            else if (zgetfn(&sel->sel_what->g_pd, gensym("loadbang")))
                vmess(&sel->sel_what->g_pd, gensym("loadbang"), "f", LB_LOAD);
    }
}

static int binbuf_nextmess(int argc, const t_atom *argv)
{
    int i=0;
    while(argc--)
    {
        argv++;
        i++;
        if (A_SEMI == argv->a_type) {
            return i+1;
        }
    }
    return i;
}
static int binbuf_getpos(t_binbuf*b, int *x0, int *y0, t_symbol**type)
{
        /*
         * checks how many objects the binbuf contains and where they are
         * located.
         * for simplicity, we stop after the first object...
         * "objects" are any patchable things
         * returns: 0: no objects/...
         *          1: single object in binbuf
         *          2: more than one object in binbuf
         * (x0,y0) are the coordinates of the first object
         * (type) is the type of the first object ("obj", "msg",...)
         */
    t_atom*argv = binbuf_getvec(b);
    int argc = binbuf_getnatom(b);
    const int argc0 = argc;
    int count = 0;
    t_symbol*s;
        /* get the position of the first object in the argv binbuf */
    if(argc > 2
       && atom_getsymbol(argv+0) == &s__N
       && atom_getsymbol(argv+1) == gensym("canvas"))
    {
        int ac = argc;
        t_atom*ap = argv;
        int stack = 0;
        do {
            int off = binbuf_nextmess(argc, argv);
            if(!off)
                break;
            ac = argc;
            ap = argv;
            argc-=off;
            argv+=off;
            count+=off;
            if(off >= 2)
            {
                if (atom_getsymbol(ap+1) == gensym("restore")
                    && atom_getsymbol(ap) == &s__X)
                    {
                        stack--;
                    }
                if (atom_getsymbol(ap+1) == gensym("canvas")
                    && atom_getsymbol(ap) == &s__N)
                    {
                        stack++;
                    }
            }
            if(argc<0)
                return 0;
        } while (stack>0);
        argc = ac;
        argv = ap;
    }
    if(argc < 4 || atom_getsymbol(argv) != &s__X)
        return 0;
        /* #X obj|msg|text|floatatom|symbolatom <x> <y> ...
         * TODO: subpatches #N canvas + #X restore <x> <y>
         */
    s = atom_getsymbol(argv+1);
    if(gensym("restore") == s
       || gensym("obj") == s
       || gensym("msg") == s
       || gensym("text") == s
       || gensym("floatatom") == s
       || gensym("listbox") == s
       || gensym("symbolatom") == s)
    {
        if(x0)*x0=atom_getfloat(argv+2);
        if(y0)*y0=atom_getfloat(argv+3);
        if(type)*type=s;
    } else
        return 0;

        /* no wind the binbuf to the next message */
    while(argc--)
    {
        count++;
        if (A_SEMI == argv->a_type)
            break;
        argv++;
    }
    return 1+(argc0 > count);
}

static void canvas_dopaste(t_canvas *x, t_binbuf *b)
{
    t_gobj *g2;
    int dspstate = canvas_suspend_dsp(), nbox, count;
    t_symbol *asym = gensym("#A");
        /* save and clear bindings to symbols #A, #N, #X; restore when done */
    t_pd *boundx = s__X.s_thing, *bounda = asym->s_thing,
        *boundn = s__N.s_thing;
    asym->s_thing = 0;
    s__X.s_thing = &x->gl_pd;
    s__N.s_thing = &pd_canvasmaker;

    canvas_editmode(x, 1.);
    glist_noselect(x);
    for (g2 = x->gl_list, nbox = 0; g2; g2 = g2->g_next) nbox++;

    THISED->paste_onset = nbox;
    THISED->paste_canvas = x;

    binbuf_eval(b, 0, 0, 0);
    for (g2 = x->gl_list, count = 0; g2; g2 = g2->g_next, count++)
        if (count >= nbox)
            glist_select(x, g2);
    THISED->paste_canvas = 0;
    canvas_resume_dsp(dspstate);
    canvas_dirty(x, 1);
    if (x->gl_mapped)
        pdgui_vmess("pdtk_canvas_getscroll", "c", x);
    if (!sys_noloadbang)
        glist_donewloadbangs(x);
    asym->s_thing = bounda;
    s__X.s_thing = boundx;
    s__N.s_thing = boundn;
}

static t_symbol *get_object_type(t_object *obj)
{
    t_symbol *s=0;
    t_binbuf *bb=0;
    if(!obj)
        return 0;
    switch(obj->te_type) {
    case T_OBJECT:
        return gensym("obj");
    case T_MESSAGE:
        return gensym("msg");
    case T_TEXT:
        return gensym("text");
    default:
            /* detecting the type of a gatom by using it's save function */
        bb=binbuf_new();
        gobj_save(&obj->te_g, bb);
        binbuf_getpos(bb, 0, 0, &s);
        binbuf_free(bb);
        return s;
    }
    return 0;
}

static void canvas_paste_replace(t_canvas *x)
{
    int x0=0, y0=0;
    t_symbol *typ0 = 0;
    if (!x->gl_editor)
        return;
    if(x->gl_editor->e_selection &&
        1==binbuf_getpos(THISED->copy_binbuf, &x0, &y0, &typ0))
    {
        t_canvas *canvas = glist_getcanvas(x);
        t_selection *mysel = 0, *y;
        t_symbol *seltype = 0;

            /* check whether all the selected objects have the same type */
        for (y = x->gl_editor->e_selection; y; y = y->sel_next)
        {
            t_symbol *s=get_object_type(pd_checkobject(&y->sel_what->g_pd));
            if (!s)
                continue;
            if(seltype && seltype != s)
            {
                seltype = 0;
                break;
            }
            seltype = s;
        }

            /* we will do a lot of reslecting; so copy the selection */
        for (y = x->gl_editor->e_selection; y; y = y->sel_next)
        {
            t_selection *sel = 0;
            t_object *obj=pd_checkobject(&y->sel_what->g_pd);
            if(!obj)
               continue;
                    /* if the selection mixes obj, msg,... we only want to
                     * replace the same type;
                     * if the selection is homogeneous (seltype==NULL), we
                     * also allow typechanges
                     */
            if (!seltype && get_object_type(obj) != typ0)
                continue;
            sel = (t_selection *)getbytes(sizeof(*sel));
            sel->sel_what = y->sel_what;
            sel->sel_next = mysel;
            mysel = sel;
        }

        canvas_undo_add(x, UNDO_SEQUENCE_START, "paste/replace", 0);

        for (y = mysel; y; y = y->sel_next)
        {
            t_object *o = (t_object *)(&y->sel_what->g_pd);
            int dx = o->te_xpix - x0;
            int dy = o->te_ypix - y0;
            glist_noselect(x);
            THISED->canvas_undo_already_set_move = 0;
                /* save connections and move object to the end */
                /* note: undo sequence selects the object as a side effect */
            canvas_undo_add(x, UNDO_ARRANGE, "arrange",
                canvas_undo_set_arrange(x, y->sel_what, 1));
            canvas_stowconnections(canvas);
                /* recreate object */
                /* remove the old object */
            canvas_undo_add(x, UNDO_CUT, "clear",
                canvas_undo_set_cut(x, UCUT_CLEAR));
            canvas_doclear(x);

                /* create the new object (and loadbang if needed) */
            canvas_applybinbuf(x, THISED->copy_binbuf);

            glist_noselect(x);
            glist_select(x, glist_nth(x, glist_getindex(x, 0) - 1));
                /* displace object (includes UNDO) */
            canvas_displaceselection(x, dx, dy);
                /* restore connections */
            canvas_restoreconnections(canvas);

            canvas_undo_add(x, UNDO_CREATE, "create",
                (void *)canvas_undo_set_create(x));

            if (pd_this->pd_newest &&
                pd_class(pd_this->pd_newest) == canvas_class)
                    canvas_loadbang((t_canvas *)pd_this->pd_newest);
        }
        canvas_undo_add(x, UNDO_SEQUENCE_END, "paste/replace", 0);

            /* free the selection copy */
        for (y = mysel; y; )
        {
            t_selection*next = y->sel_next;
            freebytes(y, sizeof(*y));
            y = next;
        }

    }
}


static void canvas_paste(t_canvas *x)
{
    if (!x->gl_editor)
        return;
    if (glist_textedfor(x))
    {
            /* simulate keystrokes as if the copy buffer were typed in. */
        pdgui_vmess("pdtk_pastetext", "^", x);
    }
    else
    {
        int offset = 0;
        int x0 = 0, y0 = 0;
        int foundplace = 0;
        binbuf_getpos(THISED->copy_binbuf, &x0, &y0, 0);
        do {
                /* iterate over all existing objects
                 * to see whether one occupies the space we want.
                 * if so, move along
                 */
            t_gobj *y;
            foundplace = 1;
            for (y = x->gl_list; y; y = y->g_next) {
                t_text *txt = (t_text *)y;
                if((x0 + offset) == txt->te_xpix &&
                    (y0 + offset) == txt->te_ypix)
                {
                    foundplace = 0;
                    offset += PASTE_OFFSET;
                    break;
                }
            }
        } while(!foundplace);
        canvas_undo_add(x, UNDO_PASTE, "paste",
            (void *)canvas_undo_set_paste(x, 0, 0, offset));
        canvas_dopaste(x, THISED->copy_binbuf);
        if(offset)
        {
            t_selection *y;
            for (y = x->gl_editor->e_selection; y; y = y->sel_next)
                gobj_displace(y->sel_what, x,
                    offset, offset);
        }
    }
}

static void canvas_duplicate(t_canvas *x)
{
    if (!x->gl_editor)
        return;

    if (x->gl_editor->e_selection && x->gl_editor->e_selectedline)
        glist_deselectline(x);

        /* if a connection is selected, extend it to the right (if possible) */
    if (x->gl_editor->e_selectedline)
    {
        int outindex = x->gl_editor->e_selectline_index1;
        int inindex  = x->gl_editor->e_selectline_index2;
        int outno = x->gl_editor->e_selectline_outno + 1;
        int inno  = x->gl_editor->e_selectline_inno + 1;
        t_gobj *outgobj = 0, *ingobj = 0;
        t_object *outobj = 0, *inobj = 0;
        int whoout = outindex;
        int whoin = inindex;

        for (outgobj = x->gl_list; whoout; outgobj = outgobj->g_next, whoout--)
            if (!outgobj->g_next) return;
        for (ingobj = x->gl_list; whoin; ingobj = ingobj->g_next, whoin--)
            if (!ingobj->g_next) return;
        outobj = (t_object*)outgobj;
        inobj = (t_object*)ingobj;

        while(!canconnect(x, outobj, outno, inobj, inno))
        {
            if (!outobj || obj_noutlets(outobj) <= outno)
                return;
            if (!inobj  || obj_ninlets (inobj ) <= inno )
                return;
            outno++;
            inno++;
        }

        if(tryconnect(x, outobj, outno, inobj, inno))
        {
            x->gl_editor->e_selectline_outno = outno;
            x->gl_editor->e_selectline_inno = inno;
        }
        return;
    }
    if (x->gl_editor->e_onmotion == MA_NONE && x->gl_editor->e_selection)
    {
        t_selection *y;
        t_binbuf*b = 0;
        if(THISED->copy_binbuf)
            b = binbuf_duplicate(THISED->copy_binbuf);
        canvas_copy(x);
        canvas_undo_add(x, UNDO_PASTE, "duplicate",
            (void *)canvas_undo_set_paste(x, 0, 1, PASTE_OFFSET));
        canvas_dopaste(x, THISED->copy_binbuf);
        for (y = x->gl_editor->e_selection; y; y = y->sel_next)
            gobj_displace(y->sel_what, x,
                PASTE_OFFSET, PASTE_OFFSET);
        if(b)
        {
            if(THISED->copy_binbuf)
                binbuf_free(THISED->copy_binbuf);
            THISED->copy_binbuf = b;
        }
        canvas_dirty(x, 1);
    }
}

static void canvas_selectall(t_canvas *x)
{
    t_gobj *y;
    if (!x->gl_editor)
        return;
    if (!x->gl_edit)
        canvas_editmode(x, 1);
        /* if everyone is already selected deselect everyone */
    if (!glist_selectionindex(x, 0, 0))
        glist_noselect(x);
    else for (y = x->gl_list; y; y = y->g_next)
         {
             if (!glist_isselected(x, y))
                 glist_select(x, y);
         }
}

static void canvas_deselectall(t_canvas *x)
{
    if(x)glist_noselect(x);
}
static void canvas_cycleselect(t_canvas*x, t_float foffset)
{
        /* select (currentselection+offset)%objectcount */
    int offset = (int)foffset;
    if (!x->gl_editor)
        return;

    if (x->gl_editor->e_onmotion == MA_CONNECT)
    {
            /* during connection, cycle through inlets/outlets */
        int xwas = x->gl_editor->e_xwas,
            ywas = x->gl_editor->e_ywas;
        int xpos = THISED->canvas_last_glist_x,
            ypos = THISED->canvas_last_glist_y;
        t_object *src, *snk;
        t_gobj*gobj;
        int srcX1=0, srcY1=0, srcX2=0, srcY2=0;
        int snkX1=0, snkY1=0, snkX2=0, snkY2=0;
        if(THISED->canvas_last_glist != x)
                /* we don't know the current mouse coordinates in this canvas, so return... */
            return;
        gobj = canvas_findhitbox(x, xwas, ywas, &srcX1, &srcY1, &srcX2, &srcY2);
        src = gobj?pd_checkobject(&gobj->g_pd):0;
        gobj = canvas_findhitbox(x, xpos, ypos, &snkX1, &snkY1, &snkX2, &snkY2);
        snk = gobj?pd_checkobject(&gobj->g_pd):0;

        if(!src) /* this should never happen */
            return;

            /* are we hovering over an object?
             * - if so, cycle through inlets
             * - else, cycle through outlets
             */
        if(snk && snk != src) {
                /* cycle inlets */
            int width = snkX2 - snkX1, hotspot, closest;
            int nios = obj_ninlets(snk);
            if (nios <= 1) /* no use cycling */
                return;
            closest = ((xpos-snkX1) * (nios-1) + width/2)/width;
            closest = ((closest + offset) % nios + nios) % nios;
            hotspot = snkX1 + (width - IOWIDTH) * closest / (nios-1.0) + IOWIDTH*0.5;
            pdgui_vmess("::pdtk_canvas::setmouse", "cii",  glist_getcanvas(x), hotspot, ypos);
        } else {
                /* cycle outlets */
            int width = srcX2 - srcX1, hotspot, closest;
            int nios = obj_noutlets(src);
            if (nios <= 1) /* no use cycling */
                return;
            closest = ((xwas-srcX1) * (nios-1) + width/2)/width;
            closest = ((closest + offset) % nios + nios) % nios;
            hotspot = srcX1 + (width - IOWIDTH) * closest / (nios-1.0) + IOWIDTH*0.5;
            x->gl_editor->e_xwas = hotspot;
            canvas_doconnect(x, xpos, ypos, 0, 0);
        }
        return;
    }
    if (x->gl_editor->e_selection)
    {
            /* cycle the selection to the next object */
        int newindex;
        int objectcount = glist_getindex(x, 0);
            /* only cycle selection if the current selection contains exactly 1 item */
        t_gobj* y = x->gl_editor->e_selection->sel_next ? 0 : x->gl_editor->e_selection->sel_what;
        if (!y || !objectcount)
            return;
        newindex = (glist_getindex(x, y) + offset) % objectcount;
        if (newindex < 0) newindex += objectcount;
        glist_deselect(x, y);
        glist_select(x, glist_nth(x, newindex));
        return;
    }
    if (x->gl_editor->e_selectedline)
    {
            /* if (only) a line is selected, cycle to next line */
        int connectioncount = 0;
        int foundit = 0;
        t_linetraverser t;
        t_outconnect *oc = 0;

        linetraverser_start(&t, x);
        while (offset && (oc = linetraverser_next(&t)))
        {
            connectioncount++;
            if(!foundit) {
                int srcno = glist_getindex(x, &t.tr_ob->ob_g);
                int sinkno = glist_getindex(x, &t.tr_ob2->ob_g);
                if((srcno      == x->gl_editor->e_selectline_index1) &&
                    (t.tr_outno == x->gl_editor->e_selectline_outno) &&
                    (sinkno     == x->gl_editor->e_selectline_index2) &&
                    (t.tr_inno  == x->gl_editor->e_selectline_inno)) {
                    foundit = connectioncount;
                }
            } else
                offset--;
        }

        if (!connectioncount)
            offset = 0;

            /* if the offset is non-0, wrap it... */
        if (offset)
        {
            offset = (((offset - 1) % connectioncount) + connectioncount)
                % connectioncount;
            /* ... and start from the beginning */
            linetraverser_start(&t, x);
            while ((oc = linetraverser_next(&t)))
            {
                if(!offset)break;
                offset--;
            }
        }
        if (oc)
            glist_selectline(x, oc,
                glist_getindex(x, &t.tr_ob->ob_g), t.tr_outno,
                glist_getindex(x, &t.tr_ob2->ob_g), t.tr_inno);
        return;
    }
}


static void canvas_reselect(t_canvas *x)
{
    t_gobj *g, *gwas;
        /* if someone is text editing, and if only one object is
           selected,  deselect everyone and reselect.  */
    if (glist_textedfor(x))
    {
            /* only do this if exactly one item is selected. */
        if ((gwas = x->gl_editor->e_selection->sel_what) &&
            !x->gl_editor->e_selection->sel_next)
        {
            int nobjwas = glist_getindex(x, 0),
                indx = canvas_getindex(x, x->gl_editor->e_selection->sel_what);
            glist_noselect(x);
            for (g = x->gl_list; g; g = g->g_next)
                if (g == gwas)
                {
                    glist_select(x, g);
                    return;
                }
                /* "gwas" must have disappeared; just search to the last
                   object and select it */
            for (g = x->gl_list; g; g = g->g_next)
                if (!g->g_next)
                    glist_select(x, g);
        }
    }
    else if (x->gl_editor->e_selection &&
             !x->gl_editor->e_selection->sel_next)
            /* otherwise activate first item in selection */
        gobj_activate(x->gl_editor->e_selection->sel_what, x, 1);
}

void canvas_connect(t_canvas *x, t_floatarg fwhoout, t_floatarg foutno,
    t_floatarg fwhoin, t_floatarg finno)
{
    int whoout = fwhoout, outno = foutno, whoin = fwhoin, inno = finno;
    t_gobj *src = 0, *sink = 0;
    t_object *objsrc, *objsink;
    t_outconnect *oc;
    int nin = whoin, nout = whoout;
    if (THISED->paste_canvas == x) whoout += THISED->paste_onset,
        whoin += THISED->paste_onset;
    for (src = x->gl_list; whoout; src = src->g_next, whoout--)
        if (!src->g_next) {
            src = NULL;
            logpost(sink, PD_DEBUG, "cannot connect non-existing object");
            goto bad; /* bug fix thanks to Hannes */
        }
    for (sink = x->gl_list; whoin; sink = sink->g_next, whoin--)
        if (!sink->g_next) {
            sink = NULL;
            logpost(src, PD_DEBUG, "cannot connect to non-existing object");
            goto bad;
        }

        /* check they're both patchable objects */
    if (!(objsrc = pd_checkobject(&src->g_pd)) ||
        !(objsink = pd_checkobject(&sink->g_pd))) {
        logpost(src?src:sink, PD_DEBUG, "cannot connect unpatchable object");
        goto bad;
    }

        /* check if objects are already connected */
    if (canvas_isconnected(x, objsrc, outno, objsink, inno)) {
        logpost(src, PD_DEBUG, "io pair already connected");
        goto bad;
    }

        /* if object creation failed, make dummy inlets or outlets
           as needed */
    if (pd_class(&src->g_pd) == text_class && objsrc->te_type == T_OBJECT)
        while (outno >= obj_noutlets(objsrc))
            outlet_new(objsrc, 0);
    if (pd_class(&sink->g_pd) == text_class && objsink->te_type == T_OBJECT)
        while (inno >= obj_ninlets(objsink))
            inlet_new(objsink, &objsink->ob_pd, 0, 0);

    if (!(oc = obj_connect(objsrc, outno, objsink, inno))) goto bad;
    if (glist_isvisible(x) && x->gl_havewindow)
    {
        char tag[128];
        sprintf(tag, "l%p", oc);
        pdgui_vmess(0, "rcr iik iiii",
            "pdtk_canvas_create_patchcord", glist_getcanvas(x), tag,
                0, (obj_issignaloutlet(objsrc, outno) ? 2 : 1) * x->gl_zoom,
                    THISGUI->i_foregroundcolor,
                        0, 0, 0, 0);
        canvas_fixlinesfor(x, objsrc);
    }
    return;

bad:
    post("%s %d %d %d %d (%s->%s) connection failed",
        x->gl_name->s_name, nout, outno, nin, inno,
            (src? class_getname(pd_class(&src->g_pd)) : "???"),
            (sink? class_getname(pd_class(&sink->g_pd)) : "???"));
}

#define XTOLERANCE 18
#define YTOLERANCE 17
#define NHIST 35

    /* LATER might have to speed this up */
static void canvas_tidy(t_canvas *x)
{
    t_gobj *y, *y2;
    int ax1, ay1, ax2, ay2, bx1, by1, bx2, by2;
    int histogram[NHIST], *ip, i, besthist, bestdist;
        /* if nobody is selected, this means do it to all boxes;
           otherwise just the selection */
    int all = (x->gl_editor ? (x->gl_editor->e_selection == 0) : 1);

    canvas_undo_add(x, UNDO_MOTION, "{tidy up}", canvas_undo_set_move(x, !all));

        /* tidy horizontally */
    for (y = x->gl_list; y; y = y->g_next)
        if (all || glist_isselected(x, y))
        {
            gobj_getrect(y, x, &ax1, &ay1, &ax2, &ay2);

            for (y2 = x->gl_list; y2; y2 = y2->g_next)
                if (all || glist_isselected(x, y2))
                {
                    gobj_getrect(y2, x, &bx1, &by1, &bx2, &by2);
                    if (by1 <= ay1 + YTOLERANCE && by1 >= ay1 - YTOLERANCE &&
                        bx1 < ax1)
                        goto nothorizhead;
                }

            for (y2 = x->gl_list; y2; y2 = y2->g_next)
                if (all || glist_isselected(x, y2))
                {
                    gobj_getrect(y2, x, &bx1, &by1, &bx2, &by2);
                    if (by1 <= ay1 + YTOLERANCE && by1 >= ay1 - YTOLERANCE
                        && by1 != ay1)
                        gobj_displace(y2, x, 0, ay1-by1);
                }
        nothorizhead: ;
        }
        /* tidy vertically.  First guess the user's favorite vertical spacing */
    for (i = NHIST, ip = histogram; i--; ip++) *ip = 0;
    for (y = x->gl_list; y; y = y->g_next)
        if (all || glist_isselected(x, y))
        {
            gobj_getrect(y, x, &ax1, &ay1, &ax2, &ay2);
            for (y2 = x->gl_list; y2; y2 = y2->g_next)
                if (all || glist_isselected(x, y2))
                {
                    gobj_getrect(y2, x, &bx1, &by1, &bx2, &by2);
                    if (bx1 <= ax1 + XTOLERANCE && bx1 >= ax1 - XTOLERANCE)
                    {
                        int distance = by1-ay2;
                        if (distance >= 0 && distance < NHIST)
                            histogram[distance]++;
                    }
                }
        }
    for (i = 2, besthist = 0, bestdist = 4, ip = histogram + 2;
         i < (NHIST-2); i++, ip++)
    {
        int hit = ip[-2] + 2 * ip[-1] + 3 * ip[0] + 2* ip[1] + ip[2];
        if (hit > besthist)
        {
            besthist = hit;
            bestdist = i;
        }
    }
    logpost(NULL, PD_DEBUG, "tidy: best vertical distance %d", bestdist);
    for (y = x->gl_list; y; y = y->g_next)
        if (all || glist_isselected(x, y))
        {
            int keep = 1;
            gobj_getrect(y, x, &ax1, &ay1, &ax2, &ay2);
            for (y2 = x->gl_list; y2; y2 = y2->g_next)
                if (all || glist_isselected(x, y2))
                {
                    gobj_getrect(y2, x, &bx1, &by1, &bx2, &by2);
                    if (bx1 <= ax1 + XTOLERANCE && bx1 >= ax1 - XTOLERANCE &&
                        ay1 >= by2 - 10 && ay1 < by2 + NHIST)
                        goto nothead;
                }
            while (keep)
            {
                keep = 0;
                for (y2 = x->gl_list; y2; y2 = y2->g_next)
                    if (all || glist_isselected(x, y2))
                    {
                        gobj_getrect(y2, x, &bx1, &by1, &bx2, &by2);
                        if (bx1 <= ax1 + XTOLERANCE &&
                            bx1 >= ax1 - XTOLERANCE &&
                                by1 > ay1 && by1 < ay2 + NHIST)
                        {
                            int vmove = ay2 + bestdist - by1;
                            gobj_displace(y2, x, ax1-bx1, vmove);
                            ay1 = by1 + vmove;
                            ay2 = by2 + vmove;
                            keep = 1;
                            break;
                        }
                    }
            }
        nothead: ;
        }
    canvas_dirty(x, 1);
}

/* returns the total number of connections between two objects
 * outno/inno are set to the last connection indices
 */
static int canvas_getconns(t_object*objsrc, int *outno, t_object*objsink, int *inno)
{
    int count = 0;
    int n;
    for(n=0; n<obj_noutlets(objsrc); n++)
    {
        t_outlet*out = 0;
        t_outconnect *oc = obj_starttraverseoutlet(objsrc, &out, n);
        while(oc)
        {
            t_object*o;
            t_inlet*in;
            int which;
            oc = obj_nexttraverseoutlet(oc, &o, &in, &which);
            if(o == objsink)
                *outno = n, *inno = which, count++;
        }
    }
    return count;
}
static int canvas_try_bypassobj1(t_canvas* x,
    t_object* obj0, int in0, int out0,
    t_object* obj1, int in1, int out1,
    t_object* obj2, int in2, int out2)
{
        /* tries to bypass 'obj1' so 'obj0->obj1->obj2' becomes
         * 'obj0->obj2' (+ 'obj1'); only bypass if there's exactly one
         * connection between both obj0->obj1 and obj1->obj2 and the two
         * connections are of the same type
         * skip connection, if it's already there
         */
        /* this is doing an awful lot of iterating over the same things again and again
         * LATER speed this up */
    int A, B, C;
        /* check connections (obj0->obj1->obj2, but not obj2->obj0) */
        /*
           valid:   out0, in1, out1, in2
           invalid: in0, out2
        */
    if(out0<0 || out1<0 || out2>=0 || in0>=0 || in1<0 || in2<0)
        return 0;
        /* check whether the connection types match */
    if(obj_issignaloutlet(obj0, out0) && !obj_issignalinlet(obj2, in2))
    {
        /* prevent new sig->msg connections! */
        if ((obj_issignalinlet(obj1, in1) && !obj_issignaloutlet(obj1, out1)))
            return 0;
            /* if we reach this, the new (bypass) connection is wrong,
             * but the bypassed object was wrongly connected anyhow,
             * so no new regression is introduced
             */
    }

    A = glist_getindex(x, &obj0->te_g);
    B = glist_getindex(x, &obj1->te_g);
    C = glist_getindex(x, &obj2->te_g);
    canvas_disconnect_with_undo(x, A, out0, B, in1);
    canvas_disconnect_with_undo(x, B, out1, C, in2);
    if (!canvas_isconnected(x, obj0, out0, obj2, in2))
        canvas_connect_with_undo(x, A, out0, C, in2);
    return 1;
}
static int canvas_try_insert(t_canvas *x
    , t_object* obj00, int in00, int out00 /* source */
    , t_object* obj11, int in11, int out11 /* sink   */
    , t_object* obj22, int in22, int out22 /* insert */
    )
{
    int out21 = 0, in02 = 0; /* iolets of the insert-objects */
    int A, B, C;

        /* check connections (obj00->obj11, but not obj22) */
    if(out00<0 || out22>=0 || out11>=0 || in00>=0 || in22>=0 || in11<0)
        return 0;

        /* check whether the connection types match */
    if((obj_issignaloutlet(obj00, out00) && !obj_issignalinlet(obj22, in02)))
        return 0;
    if((obj_issignaloutlet(obj22, out21) && !obj_issignalinlet(obj11, in11)))
        return 0;

        /* then connect them */
    A = glist_getindex(x, &obj00->te_g);
    B = glist_getindex(x, &obj11->te_g);
    C = glist_getindex(x, &obj22->te_g);

    canvas_disconnect_with_undo(x, A, out00, B, in11);
    if (!canvas_isconnected     (x, obj00, out00, obj22, in02))
        canvas_connect_with_undo(x, A,     out00, C,     in02);
    if (!canvas_isconnected     (x, obj22, out21, obj11, in11))
        canvas_connect_with_undo(x, C,     out21, B,     in11);
    return 1;
}
    /* If we have two selected objects on the canvas, try to connect
       the first outlet of the upper object to the first inlet with
       a compatible type in the lower one. */
static void canvas_connect_selection(t_canvas *x)
{
    t_gobj *a, *b, *c;
    t_selection *sel;
    t_object *objsrc, *objsink;

    a = b = c = NULL;
    sel = x->gl_editor ? x->gl_editor->e_selection : NULL;
    for (; sel; sel = sel->sel_next)
    {
        if (!a)
            a = sel->sel_what;
        else if (!b)
            b = sel->sel_what;
        else if (!c)
            c = sel->sel_what;
        else
            return;
    }

    if(!a)
        return;

    if(!b)
    {
            /* only a single object is selected.
             * if a connection is selected, insert the object
             * if no connection is selected, disconnect the object
             */
        t_object*obj = pd_checkobject(&a->g_pd);
        if(!obj)
            return;
        if(x->gl_editor->e_selectedline)
        {
            b = glist_nth(x, x->gl_editor->e_selectline_index1);
            objsrc = b?pd_checkobject(&b->g_pd):0;
            b = glist_nth(x, x->gl_editor->e_selectline_index2);
            objsink = b?pd_checkobject(&b->g_pd):0;

            if(canconnect(x, objsrc, x->gl_editor->e_selectline_outno, obj, 0)
               && canconnect(x, obj, 0, objsink,
                   x->gl_editor->e_selectline_inno))
            {
                canvas_undo_add(x, UNDO_SEQUENCE_START, "reconnect", 0);
                tryconnect(x, objsrc, x->gl_editor->e_selectline_outno, obj, 0);
                tryconnect(x, obj, 0, objsink, x->gl_editor->e_selectline_inno);
                canvas_clearline(x);
                canvas_undo_add(x, UNDO_SEQUENCE_END, "reconnect", 0);
            }
        }
        else
        {
                /* disconnect the entire object */
            t_linetraverser t;
            t_outconnect *oc;
            canvas_undo_add(x, UNDO_SEQUENCE_START, "disconnect", 0);
            linetraverser_start(&t, x);
            while ((oc = linetraverser_next(&t)))
            {
                if ((obj == t.tr_ob) || (obj == t.tr_ob2))
                {
                    int srcno = glist_getindex(x, &t.tr_ob->ob_g);
                    int sinkno = glist_getindex(x, &t.tr_ob2->ob_g);
                    canvas_disconnect_with_undo(x, srcno, t.tr_outno,
                        sinkno, t.tr_inno);
                }
            }
            canvas_undo_add(x, UNDO_SEQUENCE_END, "disconnect", 0);
        }
            /* need to return since we have touched 'b' */
        return;
    }

    if(!c)
    {
            /* exactly two objects are selected
             * connect them (top to bottom) if they are patchable
             */
        if (!(objsrc = pd_checkobject(&a->g_pd)) ||
            !(objsink = pd_checkobject(&b->g_pd)))
            return;

        if(objsink->te_ypix < objsrc->te_ypix)
        {
            t_object*obj = objsink;
            objsink = objsrc;
            objsrc = obj;
        }
        if (!objsrc || !objsink)
            return;
        if (obj_noutlets(objsrc))
        {
            int noutlets = obj_noutlets(objsrc);
            int ninlets = obj_ninlets(objsink);
            int fanout = (noutlets == 1) && obj_issignaloutlet(objsrc, 0);
            int out = 0, in = 0;
            while(!tryconnect(x, objsrc, out, objsink, in))
            {
                if (noutlets <= out)
                    return;
                if (ninlets <= in )
                    return;
                in++;
                if(!fanout)
                    out++;
            }
        }
        return;
    }

        /* exactly three objects are selected
         * if they are chained up, unconnect the middle object, and connect the source to the sink
         * if only two of them are connected, insert the third
         */
    if ((objsrc = pd_checkobject(&a->g_pd)) &&
        (objsink = pd_checkobject(&b->g_pd)))
    {
        t_object *obj0 = objsrc, *obj2 = objsink;
        t_object *obj1 = pd_checkobject(&c->g_pd);
        int out01, out02, out10, out12, out20, out21;
        int in01, in02, in10, in12, in20, in21;
        if(!obj1
           || (obj0 == obj1)
           || (obj2 == obj1)
           || (obj0 == obj2))
            return;
#define GET1CONN(a, b) \
        if (1 != canvas_getconns(obj##a, &out##a##b, obj##b, &in##b##a)) \
            out##a##b = in##b##a = -1
        GET1CONN(0, 1);
        GET1CONN(0, 2);
        GET1CONN(1, 0);
        GET1CONN(1, 2);
        GET1CONN(2, 0);
        GET1CONN(2, 1);
#define TRYCONNCHANGE(fun, a, b, c)                                      \
        canvas_try_##fun(x, obj##a, in##a##c, out##a##b, obj##b, in##b##a, out##b##c, obj##c, in##c##b, out##c##a)

        canvas_undo_add(x, UNDO_SEQUENCE_START, "reconnect", 0);
        0
            || TRYCONNCHANGE(bypassobj1, 0, 1, 2)
            || TRYCONNCHANGE(bypassobj1, 0, 2, 1)
            || TRYCONNCHANGE(bypassobj1, 1, 0, 2)
            || TRYCONNCHANGE(bypassobj1, 1, 2, 0)
            || TRYCONNCHANGE(bypassobj1, 2, 0, 1)
            || TRYCONNCHANGE(bypassobj1, 2, 1, 0)
            || TRYCONNCHANGE(insert,     0, 1, 2)
            || TRYCONNCHANGE(insert,     0, 2, 1)
            || TRYCONNCHANGE(insert,     1, 0, 2)
            || TRYCONNCHANGE(insert,     1, 2, 0)
            || TRYCONNCHANGE(insert,     2, 0, 1)
            || TRYCONNCHANGE(insert,     2, 1, 0)
            ;
        canvas_undo_add(x, UNDO_SEQUENCE_END, "reconnect", 0);
    }
}

static void canvas_texteditor(t_canvas *x)
{
    t_rtext *foo;
    char *buf;
    int bufsize;
    if ((foo = glist_textedfor(x)))
        rtext_gettext(foo, &buf, &bufsize);
    else buf = "", bufsize = 0;
    pdgui_vmess("pdtk_pd_texteditor", "p", bufsize, buf);
}

void glob_key(void *dummy, t_symbol *s, int ac, t_atom *av)
{
        /* canvas_key checks for zero */
    canvas_key(0, s, ac, av);
}

void canvas_editmode(t_canvas *x, t_floatarg state)
{
    if (x->gl_edit == (unsigned int) state)
        return;
    x->gl_edit = (unsigned int) state;
    if (x->gl_edit && glist_isvisible(x) && glist_istoplevel(x))
    {
        t_gobj *g;
        t_object *ob;
        canvas_setcursor(x, CURSOR_EDITMODE_NOTHING);
            /* comments only get their 'border' (really just a bar for
            resizing) if we're in edit mode so draw the borders here */
        for (g = x->gl_list; g; g = g->g_next)
            if ((ob = pd_checkobject(&g->g_pd)) && ob->te_type == T_TEXT)
        {
            t_rtext *y = glist_getrtext(x, ob, 0);
            if (y)
                text_drawborder(ob, x, rtext_gettag(y), 1);
        }
    }
    else
    {
        glist_noselect(x);  /* this can knock us back into edit mode so : */
        x->gl_edit = (unsigned int) state;
        if (glist_isvisible(x) && glist_istoplevel(x))
        {
            t_gobj *g;
            t_object *ob;
            t_rtext *y;
                /* erase 'borders' on comment boxes; see above */
            for (g = x->gl_list; g; g = g->g_next)
                if ((ob = pd_checkobject(&g->g_pd)) && ob->te_type == T_TEXT
                    && (y = glist_getrtext(x, ob, 0)))
            {
                char tagR[128];
                sprintf(tagR, "%sR", rtext_gettag(y));
                pdgui_vmess(0, "rcr",
                    "pdtk_canvas_delete", glist_getcanvas(x), tagR);
            }
            canvas_setcursor(x, CURSOR_RUNMODE_NOTHING);
        }
    }
    if (glist_isvisible(x) && x->gl_havewindow)
    {
        pdgui_vmess("pdtk_canvas_editmode", "^i",
            glist_getcanvas(x), x->gl_edit);
        canvas_reflecttitle(x);
    }
}

    /* called by canvas_font below */
static void canvas_dofont(t_canvas *x, t_floatarg font, t_floatarg xresize,
    t_floatarg yresize)
{
    t_gobj *y;
    x->gl_font = font;
    if (xresize != 1 || yresize != 1)
    {
        canvas_setundo(x, canvas_undo_move, canvas_undo_set_move(x, 0),
            "motion");
        for (y = x->gl_list; y; y = y->g_next)
        {
            int x1, x2, y1, y2, nx1, ny1;
            gobj_getrect(y, x, &x1, &y1, &x2, &y2);
            nx1 = x1 * xresize + 0.5;
            ny1 = y1 * yresize + 0.5;
            gobj_displace(y, x, nx1-x1, ny1-y1);
        }
    }
    for (y = x->gl_list; y; y = y->g_next)
        if (pd_checkglist(&y->g_pd)  && !canvas_isabstraction((t_canvas *)y))
            canvas_dofont((t_canvas *)y, font, xresize, yresize);
    if(x->gl_havewindow) canvas_redraw(x);
}

    /* canvas_menufont calls up a TK dialog which calls this back */
static void canvas_font(t_canvas *x, t_floatarg font, t_floatarg resize,
    t_floatarg whichresize)
{
    t_float realresize, realresx = 1, realresy = 1;
    t_canvas *x2 = canvas_getrootfor(x);
    int oldfont = x2->gl_font;
    if (!resize) realresize = 1;
    else
    {
        if (resize < 20) resize = 20;
        if (resize > 500) resize = 500;
        realresize = resize * 0.01;
    }
    if (whichresize != 3) realresx = realresize;
    if (whichresize != 2) realresy = realresize;
    canvas_dofont(x2, font, realresx, realresy);
    canvas_undo_add(x2, UNDO_FONT, "font",
        canvas_undo_set_font(x2, oldfont, realresize, whichresize));

    sys_defaultfont = font;
}

void glist_getnextxy(t_glist *gl, int *xpix, int *ypix)
{
    if (THISED->canvas_last_glist == gl)
        *xpix = THISED->canvas_last_glist_x,
        *ypix = THISED->canvas_last_glist_y;
    else *xpix = *ypix = 40;
}

static void glist_setlastxy(t_glist *gl, int xval, int yval)
{
    THISED->canvas_last_glist = gl;
    THISED->canvas_last_glist_x = xval;
    THISED->canvas_last_glist_y = yval;
}


void canvas_triggerize(t_glist*cnv);

void g_editor_setup(void)
{
/* ------------------------ events ---------------------------------- */
    class_addmethod(canvas_class, (t_method)canvas_mouse, gensym("mouse"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_mouseup, gensym("mouseup"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_DEFFLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_key, gensym("key"),
        A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_motion, gensym("motion"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);

/* ------------------------ menu actions ---------------------------- */
    class_addmethod(canvas_class, (t_method)canvas_menuclose,
        gensym("menuclose"), A_DEFFLOAT, 0);
    class_addmethod(canvas_class, (t_method)canvas_cut,
        gensym("cut"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_copy,
        gensym("copy"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_paste,
        gensym("paste"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_paste_replace,
        gensym("paste-replace"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_duplicate,
        gensym("duplicate"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_selectall,
        gensym("selectall"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_deselectall,
        gensym("deselectall"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_reselect,
        gensym("reselect"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_cycleselect,
        gensym("cycleselect"), A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_undomenu,
        gensym("undo"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_undo_redo,
        gensym("redo"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_tidy,
        gensym("tidy"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_connect_selection,
        gensym("connect_selection"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_texteditor,
        gensym("texteditor"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_editmode,
        gensym("editmode"), A_DEFFLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_print,
        gensym("print"), A_SYMBOL, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_menufont,
        gensym("menufont"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_font,
        gensym("font"), A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_zoom,
        gensym("zoom"), A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_find,
        gensym("find"), A_SYMBOL, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_find_again,
        gensym("findagain"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_find_parent,
        gensym("findparent"), A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_done_popup,
        gensym("done-popup"), A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_donecanvasdialog,
        gensym("donecanvasdialog"), A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)glist_arraydialog,
        gensym("arraydialog"), A_SYMBOL, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(canvas_class, (t_method)canvas_triggerize,
        gensym("triggerize"), 0);
    class_addmethod(canvas_class, (t_method)canvas_disconnect,
        gensym("disconnect"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
}

void canvas_editor_for_class(t_class *c)
{
    class_addmethod(c, (t_method)canvas_mouse, gensym("mouse"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(c, (t_method)canvas_mouseup, gensym("mouseup"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(c, (t_method)canvas_key, gensym("key"),
        A_GIMME, A_NULL);
    class_addmethod(c, (t_method)canvas_motion, gensym("motion"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_DEFFLOAT, A_NULL);

/* ------------------------ menu actions ---------------------------- */
    class_addmethod(c, (t_method)canvas_menuclose,
        gensym("menuclose"), A_DEFFLOAT, 0);
    class_addmethod(c, (t_method)canvas_find_parent,
        gensym("findparent"), A_NULL);
}

void g_editor_newpdinstance(void)
{
    THISED = getbytes(sizeof(*THISED));
        /* other stuff is null-checked but this needs to exist: */
    THISED->copy_binbuf = binbuf_new();
}

void g_editor_freepdinstance(void)
{
    if (THISED->copy_binbuf)
        binbuf_free(THISED->copy_binbuf);
    if (THISED->canvas_undo_buf)
    {
        if (!THISED->canvas_undo_fn)
            bug("g_editor_freepdinstance");
        else (*THISED->canvas_undo_fn)
            (THISED->canvas_undo_canvas, THISED->canvas_undo_buf, UNDO_FREE);
    }
    if (THISED->canvas_findbuf)
        binbuf_free(THISED->canvas_findbuf);
    freebytes(THISED, sizeof(*THISED));
}
