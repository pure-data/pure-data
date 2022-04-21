/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* [hv]dial.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */

/* name change to [vv]radio by MSP (it's a radio button really) and changed to
   put out a "float" as in sliders, toggles, etc. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "m_pd.h"
#include "g_canvas.h"

#include "g_all_guis.h"
#include <math.h>

/* ------------- hdl     gui-horizontal dial ---------------------- */

t_widgetbehavior radio_widgetbehavior;
static t_class *radio_class;

/* widget helper functions */
static void radio_draw_io(t_radio* x, t_glist* glist, int old_snd_rcv_flags)
{
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int iow = IOWIDTH * IEMGUI_ZOOM(x), ioh = IEM_GUI_IOHEIGHT * IEMGUI_ZOOM(x);
    t_canvas *canvas = glist_getcanvas(glist);

    (void)old_snd_rcv_flags;
    sys_vgui(".x%lx.c delete %lxIN%d\n", canvas, x, 0);
    sys_vgui(".x%lx.c delete %lxOUT%d\n", canvas, x, 0);

    if(!x->x_gui.x_fsf.x_snd_able)
    {
        int height = x->x_gui.x_h * ((x->x_orientation == horizontal)? 1: x->x_number);
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags [list %lxOBJ %lxOUT%d]\n",
            canvas,
            xpos, ypos + height + IEMGUI_ZOOM(x) - ioh,
            xpos + iow, ypos + height,
            x, x, 0);

            /* keep buttons above outlet */
        sys_vgui(".x%lx.c lower %lxOUT%d %lxBUT\n", canvas, x, 0, x);
    }
    if(!x->x_gui.x_fsf.x_rcv_able)
    {
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags [list %lxOBJ %lxIN%d]\n",
            canvas,
            xpos, ypos,
            xpos + iow, ypos - IEMGUI_ZOOM(x) + ioh,
            x, x, 0);

            /* keep buttons above inlet */
        sys_vgui(".x%lx.c lower %lxIN%d %lxBUT\n", canvas, x, 0, x);
    }
}

static void radio_draw_config(t_radio* x, t_glist* glist)
{
    int i;
    t_canvas *canvas = glist_getcanvas(glist);
    int iow = IOWIDTH * IEMGUI_ZOOM(x), ioh = IEM_GUI_IOHEIGHT * IEMGUI_ZOOM(x);
    int xx11b = text_xpix(&x->x_gui.x_obj, glist);
    int yy11b = text_ypix(&x->x_gui.x_obj, glist);
    int d, dx = 0, dy = 0, d4;

    int xx11=xx11b, xx12=0, xx21=0, xx22=0;
    int yy11=yy11b, yy12=0, yy21=0, yy22=0;

    if(x->x_orientation == horizontal)
    {
        d = dx = x->x_gui.x_w;
    } else {
        d = dy = x->x_gui.x_h;
    }
    d4 = d / 4;
    xx12 = xx11 + d;
    xx21 = xx11 + d4;
    xx22 = xx12 - d4;
    yy12 = yy11 + d;
    yy21 = yy11 + d4;
    yy22 = yy12 - d4;

    for(i = 0; i < x->x_number; i++)
    {
        int col = (x->x_on == i) ? x->x_gui.x_fcol : x->x_gui.x_bcol;
        sys_vgui(".x%lx.c coords %lxBASE%d %d %d %d %d\n", canvas, x, i,
            xx11, yy11, xx12, yy12);
        sys_vgui(".x%lx.c itemconfigure %lxBASE%d -width %d -fill #%06x\n", canvas, x, i,
            IEMGUI_ZOOM(x), x->x_gui.x_bcol);

        sys_vgui(".x%lx.c coords %lxBUT%d %d %d %d %d\n", canvas, x, i,
            xx21, yy21, xx22, yy22);
        sys_vgui(".x%lx.c itemconfigure %lxBUT%d -fill #%06x -outline #%06x\n", canvas, x, i,
            col, col);
        xx11 += dx; xx12 += dx; xx21 += dx; xx22 += dx;
        yy11 += dy; yy12 += dy; yy21 += dy; yy22 += dy;

        x->x_drawn = x->x_on;
    }

    sys_vgui(".x%lx.c coords %lxLABEL %d %d\n", canvas, x,
        xx11b + x->x_gui.x_ldx * IEMGUI_ZOOM(x), yy11b + x->x_gui.x_ldy * IEMGUI_ZOOM(x));
    sys_vgui(".x%lx.c itemconfigure %lxLABEL -text {%s} -anchor w -font {{%s} -%d %s} -fill #%06x\n", canvas, x,
        (strcmp(x->x_gui.x_lab->s_name, "empty") ? x->x_gui.x_lab->s_name : ""),
        x->x_gui.x_font, x->x_gui.x_fontsize * IEMGUI_ZOOM(x), sys_fontweight,
        x->x_gui.x_lcol);
}

static void radio_draw_new(t_radio *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    int i;
    for(i=0; i<x->x_number; i++) {
        sys_vgui(".x%lx.c create rectangle 0 0 0 0 -tags [list %lxBASE%d %lxBASE %lxOBJ]\n", canvas, x, i, x, x);
        sys_vgui(".x%lx.c create rectangle 0 0 0 0 -tags [list %lxBUT%d %lxBUT %lxOBJ]\n", canvas, x, i, x, x);
    }
    sys_vgui(".x%lx.c create text 0 0 -tags [list %lxLABEL %lxOBJ label text]\n", canvas, x, x);
    radio_draw_config(x, glist);
    radio_draw_io(x, glist, 0);
}

static void radio_draw_erase(t_radio* x, t_glist* glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    sys_vgui(".x%lx.c delete %lxOBJ\n", canvas, x);
}

static void radio_draw_move(t_radio *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    int dx = text_xpix(&x->x_gui.x_obj, glist) - x->x_gui.x_prevX;
    int dy = text_ypix(&x->x_gui.x_obj, glist) - x->x_gui.x_prevY;
    sys_vgui(".x%lx.c move %lxOBJ %d %d\n", canvas, x, dx, dy);
}

static void radio_draw_select(t_radio* x, t_glist* glist)
{
    int n = x->x_number, i;
    t_canvas *canvas = glist_getcanvas(glist);
    int lcol = x->x_gui.x_lcol;
    int col = IEM_GUI_COLOR_NORMAL;

    if(x->x_gui.x_fsf.x_selected)
        lcol = col =  IEM_GUI_COLOR_SELECTED;
    sys_vgui(".x%lx.c itemconfigure %lxBASE -outline #%06x\n", canvas, x, col);
    sys_vgui(".x%lx.c itemconfigure %lxLABEL -fill #%06x\n", canvas, x, lcol);
}

static void radio_draw_update(t_gobj *client, t_glist *glist)
{
    t_radio *x = (t_radio *)client;
    if(glist_isvisible(glist))
    {
        t_canvas *canvas = glist_getcanvas(glist);

        sys_vgui(".x%lx.c itemconfigure %lxBUT%d -fill #%06x -outline #%06x\n",
            canvas, x, x->x_drawn,
            x->x_gui.x_bcol, x->x_gui.x_bcol);
        sys_vgui(".x%lx.c itemconfigure %lxBUT%d -fill #%06x -outline #%06x\n",
            canvas, x, x->x_on,
            x->x_gui.x_fcol, x->x_gui.x_fcol);
        x->x_drawn = x->x_on;
    }
}

static void radio_draw(t_radio *x, t_glist *glist, int mode)
{
    if(mode == IEM_GUI_DRAW_MODE_UPDATE)
        sys_queuegui(x, glist, radio_draw_update);
    else if(mode == IEM_GUI_DRAW_MODE_MOVE)
        radio_draw_move(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_NEW)
        radio_draw_new(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_SELECT)
        radio_draw_select(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_ERASE)
        radio_draw_erase(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_CONFIG)
        radio_draw_config(x, glist);
    else if(mode >= IEM_GUI_DRAW_MODE_IO)
        radio_draw_io(x, glist, mode - IEM_GUI_DRAW_MODE_IO);
}

/* ------------------------ hdl widgetbehaviour----------------------------- */

static void radio_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_radio *x = (t_radio *)z;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist);
    if(x->x_orientation == horizontal)
    {
        *xp2 = *xp1 + x->x_gui.x_w * x->x_number;
        *yp2 = *yp1 + x->x_gui.x_h;
    } else {
        *xp2 = *xp1 + x->x_gui.x_w;
        *yp2 = *yp1 + x->x_gui.x_h * x->x_number;
    }
}

static void radio_save(t_gobj *z, t_binbuf *b)
{
    t_radio *x = (t_radio *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];

    const char*objname;
    if(x->x_orientation == horizontal)
    {
        if(x->x_compat)
            objname="hdl";
        else
            objname="hradio";
    } else {
        if(x->x_compat)
            objname="vdl";
        else
            objname="vradio";
    }

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiiisssiiiisssf", gensym("#X"), gensym("obj"),
                (int)x->x_gui.x_obj.te_xpix,
                (int)x->x_gui.x_obj.te_ypix,
        gensym(objname),
                x->x_gui.x_w/IEMGUI_ZOOM(x),
                x->x_change, iem_symargstoint(&x->x_gui.x_isa), x->x_number,
                srl[0], srl[1], srl[2],
                x->x_gui.x_ldx, x->x_gui.x_ldy,
                iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize,
                bflcol[0], bflcol[1], bflcol[2],
                x->x_gui.x_isa.x_loadinit?x->x_fval:0.);
    binbuf_addv(b, ";");
}

static void radio_properties(t_gobj *z, t_glist *owner)
{
    t_radio *x = (t_radio *)z;
    char buf[800];
    t_symbol *srl[3];
    int hchange = -1;
    const char*objname;

    if(x->x_orientation == horizontal)
    {
        objname = "hradio";
    } else {
        objname = "vradio";
    }

    iemgui_properties(&x->x_gui, srl);
    if(x->x_compat)
        hchange = x->x_change;

    sprintf(buf, "pdtk_iemgui_dialog %%s |%s| \
            ----------dimensions(pix):----------- %d %d size: 0 0 empty \
            empty 0.0 empty 0.0 empty %d \
            %d new-only new&old %d %d number: %d \
            %s %s \
            %s %d %d \
            %d %d \
            #%06x #%06x #%06x\n",
        objname,
        x->x_gui.x_w/IEMGUI_ZOOM(x), IEM_GUI_MINSIZE,
        0,/*no_schedule*/
        hchange, x->x_gui.x_isa.x_loadinit, -1, x->x_number,
        srl[0]->s_name, srl[1]->s_name, srl[2]->s_name,
        x->x_gui.x_ldx, x->x_gui.x_ldy,
        x->x_gui.x_fsf.x_font_style, x->x_gui.x_fontsize,
        0xffffff & x->x_gui.x_bcol, 0xffffff & x->x_gui.x_fcol, 0xffffff & x->x_gui.x_lcol);
    gfxstub_new(&x->x_gui.x_obj.ob_pd, x, buf);
}

static void radio_dialog(t_radio *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int a = (int)atom_getfloatarg(0, argc, argv);
    int chg = (int)atom_getfloatarg(4, argc, argv);
    int num = (int)atom_getfloatarg(6, argc, argv);
    int sr_flags;
    t_atom undo[18];
    iemgui_setdialogatoms(&x->x_gui, 18, undo);
    SETFLOAT(undo+1, 0);
    SETFLOAT(undo+2, 0);
    SETFLOAT(undo+3, 0);
    SETFLOAT(undo+4, (x->x_compat)?x->x_change:-1);
    SETFLOAT(undo+6, x->x_number);

    pd_undo_set_objectstate(x->x_gui.x_glist, (t_pd*)x, gensym("dialog"),
                            18, undo,
                            argc, argv);

    if(chg != 0) chg = 1;
    x->x_change = chg;
    sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);
    x->x_gui.x_w = iemgui_clip_size(a) * IEMGUI_ZOOM(x);
    x->x_gui.x_h = x->x_gui.x_w;
    x->x_number = num;
    if(x->x_on >= x->x_number)
    {
        x->x_on_old = x->x_on = x->x_number - 1;
        x->x_on_old = x->x_on;
    }

    iemgui_size((void *)x, &x->x_gui);
}

static void radio_set(t_radio *x, t_floatarg f)
{
    int i = (int)f;

    x->x_fval = f;
    if(i < 0)
        i = 0;
    if(i >= x->x_number)
        i = x->x_number - 1;
    if(x->x_on != x->x_on_old)
    {
        int old = x->x_on_old;
        x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        x->x_on_old = old;
    }
    else
    {
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    }
}

static void radio_bang(t_radio *x)
{
    if(x->x_compat)
    {
            /* compatibility with earlier "[hv]dial" behavior */
        t_atom at[2];
        if((x->x_change) && (x->x_on != x->x_on_old))
        {
            SETFLOAT(at+0, (t_float)x->x_on_old);
            SETFLOAT(at+1, 0.0);
            outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
        }
        x->x_on_old = x->x_on;
        SETFLOAT(at+0, (t_float)x->x_on);
        SETFLOAT(at+1, 1.0);
        outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
    }
    else
    {
        t_float outval = (pd_compatibilitylevel < 46 ? x->x_on : x->x_fval);
        outlet_float(x->x_gui.x_obj.ob_outlet, outval);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_float(x->x_gui.x_snd->s_thing, outval);
    }
}

static void radio_fout(t_radio *x, t_floatarg f)
{
    int i = (int)f;

    x->x_fval = f;
    if(i < 0)
        i = 0;
    if(i >= x->x_number)
        i = x->x_number - 1;

    if(x->x_compat)
    {
            /* compatibility with earlier "[hv]dial" behavior */
        t_atom at[2];
        if((x->x_change) && (i != x->x_on_old))
        {
            SETFLOAT(at+0, (t_float)x->x_on_old);
            SETFLOAT(at+1, 0.0);
            outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
        }
        if(x->x_on != x->x_on_old)
            x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        x->x_on_old = x->x_on;
        SETFLOAT(at+0, (t_float)x->x_on);
        SETFLOAT(at+1, 1.0);
        outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
    }
    else
    {
        t_float outval = (pd_compatibilitylevel < 46 ? i : x->x_fval);
        x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        outlet_float(x->x_gui.x_obj.ob_outlet, outval);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_float(x->x_gui.x_snd->s_thing, outval);
    }
}

static void radio_float(t_radio *x, t_floatarg f)
{
    int i = (int)f;
    x->x_fval = f;
    if(i < 0)
        i = 0;
    if(i >= x->x_number)
        i = x->x_number - 1;

    if(x->x_compat)
    {
        t_atom at[2];
            /* compatibility with earlier "[hv]dial" behavior */
        if((x->x_change) && (i != x->x_on_old))
        {
            if(x->x_gui.x_fsf.x_put_in2out)
            {
                SETFLOAT(at+0, (t_float)x->x_on_old);
                SETFLOAT(at+1, 0.0);
                outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
                if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                    pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
            }
        }
        if(x->x_on != x->x_on_old)
            x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        x->x_on_old = x->x_on;
        if(x->x_gui.x_fsf.x_put_in2out)
        {
            SETFLOAT(at+0, (t_float)x->x_on);
            SETFLOAT(at+1, 1.0);
            outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
        }
    }
    else
    {
        t_float outval = (pd_compatibilitylevel < 46 ? i : x->x_fval);
        x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        if(x->x_gui.x_fsf.x_put_in2out)
        {
            outlet_float(x->x_gui.x_obj.ob_outlet, outval);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_float(x->x_gui.x_snd->s_thing, outval);
        }
    }
}

static void radio_click(t_radio *x, t_floatarg xpos, t_floatarg ypos, t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    if (x->x_orientation == horizontal)
    {
        int xx = (int)xpos - (int)text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist);
        radio_fout(x, (t_float)(xx / x->x_gui.x_w));
    } else {
        int yy =  (int)ypos - text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);
        radio_fout(x, (t_float)(yy / x->x_gui.x_h));
    }
}

static int radio_newclick(t_gobj *z, struct _glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if(doit)
        radio_click((t_radio *)z, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift, 0, (t_floatarg)alt);
    return (1);
}

static void radio_loadbang(t_radio *x, t_floatarg action)
{
    if(action == LB_LOAD && x->x_gui.x_isa.x_loadinit)
        radio_bang(x);
}

static void radio_number(t_radio *x, t_floatarg num)
{
    int n = (int)num;

    if(n < 1)
        n = 1;
    if(n > IEM_RADIO_MAX)
        n = IEM_RADIO_MAX;
    if(n != x->x_number)
    {
        int vis = glist_isvisible(x->x_gui.x_glist);
        if(vis)
            (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_ERASE);
        x->x_number = n;
        if(x->x_on >= x->x_number)
            x->x_on = x->x_number - 1;
        x->x_on_old = x->x_on;
        if(vis && gobj_shouldvis((t_gobj *)x, x->x_gui.x_glist))
        {
            (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_NEW);
            canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
        }
    }
}

static void radio_orientation(t_radio *x, t_floatarg forient)
{
    x->x_orientation = !!(int)forient;

    if(!glist_isvisible(x->x_gui.x_glist))
        return;

    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_CONFIG);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO);
    canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
}

static void radio_size(t_radio *x, t_symbol *s, int ac, t_atom *av)
{
    x->x_gui.x_w = iemgui_clip_size((int)atom_getfloatarg(0, ac, av)) * IEMGUI_ZOOM(x);
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_size((void *)x, &x->x_gui);
}

static void radio_delta(t_radio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_delta((void *)x, &x->x_gui, s, ac, av);}

static void radio_pos(t_radio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_pos((void *)x, &x->x_gui, s, ac, av);}

static void radio_color(t_radio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_color((void *)x, &x->x_gui, s, ac, av);}

static void radio_send(t_radio *x, t_symbol *s)
{iemgui_send(x, &x->x_gui, s);}

static void radio_receive(t_radio *x, t_symbol *s)
{iemgui_receive(x, &x->x_gui, s);}

static void radio_label(t_radio *x, t_symbol *s)
{iemgui_label((void *)x, &x->x_gui, s);}

static void radio_label_pos(t_radio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_pos((void *)x, &x->x_gui, s, ac, av);}

static void radio_label_font(t_radio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_font((void *)x, &x->x_gui, s, ac, av);}

static void radio_init(t_radio *x, t_floatarg f)
{x->x_gui.x_isa.x_loadinit = (f == 0.0) ? 0 : 1;}

static void radio_double_change(t_radio *x)
{
    if(x->x_compat)
        x->x_change = 1;
    else
        pd_error(x, "radio: no method for 'double_change'");
}

static void radio_single_change(t_radio *x)
{
    if(x->x_compat)
        x->x_change = 0;
    else
        pd_error(x, "radio: no method for 'single_change'");
}

static void *radio_donew(t_symbol *s, int argc, t_atom *argv, int old)
{
    t_radio *x = (t_radio *)pd_new(radio_class);
    int a = IEM_GUI_DEFAULTSIZE, on = 0;
    int ldx = 0, ldy = -8, chg = 1, num = 8;
    int fs = 10;
    t_float fval = 0;
    if('v' == *s->s_name)
        x->x_orientation = vertical;
    x->x_compat = old;

    iem_inttosymargs(&x->x_gui.x_isa, 0);
    iem_inttofstyle(&x->x_gui.x_fsf, 0);

    x->x_gui.x_bcol = 0xFCFCFC;
    x->x_gui.x_fcol = 0x00;
    x->x_gui.x_lcol = 0x00;

    if((argc == 15)&&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)&&IS_A_FLOAT(argv,2)
       &&IS_A_FLOAT(argv,3)
       &&(IS_A_SYMBOL(argv,4)||IS_A_FLOAT(argv,4))
       &&(IS_A_SYMBOL(argv,5)||IS_A_FLOAT(argv,5))
       &&(IS_A_SYMBOL(argv,6)||IS_A_FLOAT(argv,6))
       &&IS_A_FLOAT(argv,7)&&IS_A_FLOAT(argv,8)
       &&IS_A_FLOAT(argv,9)&&IS_A_FLOAT(argv,10)&&IS_A_FLOAT(argv,14))
    {
        a = (int)atom_getfloatarg(0, argc, argv);
        chg = (int)atom_getfloatarg(1, argc, argv);
        iem_inttosymargs(&x->x_gui.x_isa, atom_getfloatarg(2, argc, argv));
        num = (int)atom_getfloatarg(3, argc, argv);
        iemgui_new_getnames(&x->x_gui, 4, argv);
        ldx = (int)atom_getfloatarg(7, argc, argv);
        ldy = (int)atom_getfloatarg(8, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getfloatarg(9, argc, argv));
        fs = (int)atom_getfloatarg(10, argc, argv);
        iemgui_all_loadcolors(&x->x_gui, argv+11, argv+12, argv+13);
        fval = atom_getfloatarg(14, argc, argv);
    }
    else iemgui_new_getnames(&x->x_gui, 4, 0);
    x->x_gui.x_draw = (t_iemfunptr)radio_draw;
    x->x_gui.x_fsf.x_snd_able = 1;
    x->x_gui.x_fsf.x_rcv_able = 1;
    x->x_gui.x_glist = (t_glist *)canvas_getcurrent();
    if(!strcmp(x->x_gui.x_snd->s_name, "empty"))
        x->x_gui.x_fsf.x_snd_able = 0;
    if(!strcmp(x->x_gui.x_rcv->s_name, "empty"))
        x->x_gui.x_fsf.x_rcv_able = 0;
    if(x->x_gui.x_fsf.x_font_style == 1) strcpy(x->x_gui.x_font, "helvetica");
    else if(x->x_gui.x_fsf.x_font_style == 2) strcpy(x->x_gui.x_font, "times");
    else { x->x_gui.x_fsf.x_font_style = 0;
        strcpy(x->x_gui.x_font, sys_font); }
    if(num < 1)
        num = 1;
    if(num > IEM_RADIO_MAX)
        num = IEM_RADIO_MAX;
    x->x_number = num;
    x->x_fval = fval;
    on = fval;
    if(on < 0)
        on = 0;
    if(on >= x->x_number)
        on = x->x_number - 1;
    if(x->x_gui.x_isa.x_loadinit)
        x->x_on = on;
    else
        x->x_on = 0;
    x->x_on_old = x->x_on;
    x->x_change = (chg == 0) ? 0 : 1;
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;
    if(fs < 4)
        fs = 4;
    x->x_gui.x_fontsize = fs;
    x->x_gui.x_w = iemgui_clip_size(a);
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    iemgui_newzoom(&x->x_gui);
    outlet_new(&x->x_gui.x_obj, &s_list);
    return (x);
}

static void *radio_new(t_symbol *s, int argc, t_atom *argv)
{
    return (radio_donew(s, argc, argv, 0));
}

static void *dial_new(t_symbol *s, int argc, t_atom *argv)
{
    return (radio_donew(s, argc, argv, 1));
}

static void radio_free(t_radio *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    gfxstub_deleteforkey(x);
}

void g_radio_setup(void)
{
    radio_class = class_new(gensym("hradio"), (t_newmethod)radio_new,
        (t_method)radio_free, sizeof(t_radio), 0, A_GIMME, 0);
    class_addcreator((t_newmethod)radio_new, gensym("vradio"), A_GIMME, 0);

    class_addcreator((t_newmethod)radio_new, gensym("rdb"), A_GIMME, 0);
    class_addcreator((t_newmethod)radio_new, gensym("radiobut"), A_GIMME, 0);
    class_addcreator((t_newmethod)radio_new, gensym("radiobutton"), A_GIMME, 0);

    class_addbang(radio_class, radio_bang);
    class_addfloat(radio_class, radio_float);
    class_addmethod(radio_class, (t_method)radio_click,
        gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_dialog,
        gensym("dialog"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_loadbang,
        gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_set,
        gensym("set"), A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_size,
        gensym("size"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_delta,
        gensym("delta"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_pos,
        gensym("pos"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_color,
        gensym("color"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_send,
        gensym("send"), A_DEFSYM, 0);
    class_addmethod(radio_class, (t_method)radio_receive,
        gensym("receive"), A_DEFSYM, 0);
    class_addmethod(radio_class, (t_method)radio_label,
        gensym("label"), A_DEFSYM, 0);
    class_addmethod(radio_class, (t_method)radio_label_pos,
        gensym("label_pos"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_label_font,
        gensym("label_font"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_init,
        gensym("init"), A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_number,
        gensym("number"), A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_orientation,
        gensym("orientation"), A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)iemgui_zoom,
        gensym("zoom"), A_CANT, 0);
    radio_widgetbehavior.w_getrectfn = radio_getrect;
    radio_widgetbehavior.w_displacefn = iemgui_displace;
    radio_widgetbehavior.w_selectfn = iemgui_select;
    radio_widgetbehavior.w_activatefn = NULL;
    radio_widgetbehavior.w_deletefn = iemgui_delete;
    radio_widgetbehavior.w_visfn = iemgui_vis;
    radio_widgetbehavior.w_clickfn = radio_newclick;
    class_setwidget(radio_class, &radio_widgetbehavior);

    class_sethelpsymbol(radio_class, gensym("hradio"));
    class_setsavefn(radio_class, radio_save);
    class_setpropertiesfn(radio_class, radio_properties);

        /* obsolete version (0.34-0.35) */
    class_addcreator((t_newmethod)dial_new, gensym("hdl"), A_GIMME, 0);
    class_addcreator((t_newmethod)dial_new, gensym("vdl"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_single_change,
        gensym("single_change"), 0);
    class_addmethod(radio_class, (t_method)radio_double_change,
        gensym("double_change"), 0);
}
