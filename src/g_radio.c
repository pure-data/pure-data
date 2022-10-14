/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* [hv]dial.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */

/* name change to [vv]radio by MSP (it's a radio button really) and changed to
   put out a "float" as in sliders, toggles, etc. */

#include <string.h>
#include <stdio.h>
#include "m_pd.h"

#include "g_all_guis.h"

/* ------------- hdl     gui-horizontal dial ---------------------- */

t_widgetbehavior radio_widgetbehavior;
static t_class *radio_class;

/* widget helper functions */

/* cannot use iemgui's default draw_iolets, because
 * - vradio would use show the outlet at the 0th button rather than the last...
 */
static void radio_draw_io(t_radio* x, t_glist* glist, int old_snd_rcv_flags)
{
    const int zoom = IEMGUI_ZOOM(x);
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int iow = IOWIDTH * zoom, ioh = IEM_GUI_IOHEIGHT * zoom;
    t_canvas *canvas = glist_getcanvas(glist);
    char tag_object[128], tag_but[128], tag[128];
    char *tags[] = {tag_object, tag};

    (void)old_snd_rcv_flags;

    sprintf(tag_object, "%lxOBJ", x);
    sprintf(tag_but, "%lxBUT", x);

    sprintf(tag, "%lxOUT%d", x, 0);
    pdgui_vmess(0, "crs", canvas, "delete", tag);
    if(!x->x_gui.x_fsf.x_snd_able)
    {
        int height = x->x_gui.x_h * ((x->x_orientation == horizontal)? 1: x->x_number);
        pdgui_vmess(0, "crr iiii rs rS", canvas, "create", "rectangle",
            xpos, ypos + height + zoom - ioh,
            xpos + iow, ypos + height,
            "-fill", "black",
            "-tags", 2, tags);

            /* keep buttons above outlet */
        pdgui_vmess(0, "crss", canvas, "lower", tag, tag_but);
    }

    sprintf(tag, "%lxIN%d", x, 0);
    pdgui_vmess(0, "crs", canvas, "delete", tag);
    if(!x->x_gui.x_fsf.x_rcv_able)
    {
        pdgui_vmess(0, "crr iiii rs rS", canvas, "create", "rectangle",
            xpos, ypos,
            xpos + iow, ypos - zoom + ioh,
            "-fill", "black",
            "-tags", 2, tags);

            /* keep buttons above inlet */
        pdgui_vmess(0, "crss", canvas, "lower", tag, tag_but);
    }
}

static void radio_draw_config(t_radio* x, t_glist* glist)
{
    int i;
    const int zoom = IEMGUI_ZOOM(x);
    t_iemgui *iemgui = &x->x_gui;
    t_canvas *canvas = glist_getcanvas(glist);
    int iow = IOWIDTH * zoom, ioh = IEM_GUI_IOHEIGHT * zoom;
    int xx11b = text_xpix(&x->x_gui.x_obj, glist);
    int yy11b = text_ypix(&x->x_gui.x_obj, glist);
    int d, dx = 0, dy = 0, d4;

    int xx11=xx11b, xx12=0, xx21=0, xx22=0;
    int yy11=yy11b, yy12=0, yy21=0, yy22=0;

    char tag[128];
    t_atom fontatoms[3];
    SETSYMBOL(fontatoms+0, gensym(iemgui->x_font));
    SETFLOAT (fontatoms+1, -iemgui->x_fontsize*zoom);
    SETSYMBOL(fontatoms+2, gensym(sys_fontweight));

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
        sprintf(tag, "%lxBASE%d", x, i);
        pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
            xx11, yy11, xx12, yy12);
        pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
            "-width", zoom, "-fill", x->x_gui.x_bcol);

        sprintf(tag, "%lxBUT%d", x, i);
        pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
            xx21, yy21, xx22, yy22);
        pdgui_vmess(0, "crs rk rk", canvas, "itemconfigure", tag,
            "-fill", col, "-outline", col);
        xx11 += dx; xx12 += dx; xx21 += dx; xx22 += dx;
        yy11 += dy; yy12 += dy; yy21 += dy; yy22 += dy;

        x->x_drawn = x->x_on;
    }

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crs ii", canvas, "coords", tag,
        xx11b + x->x_gui.x_ldx * zoom, yy11b + x->x_gui.x_ldy * zoom);
    pdgui_vmess(0, "crs rA rk", canvas, "itemconfigure", tag,
        "-font", 3, fontatoms,
        "-fill", x->x_gui.x_lcol);
    iemgui_dolabel(x, &x->x_gui, x->x_gui.x_lab, 1);
}

static void radio_draw_new(t_radio *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    int i;
    char tag_n[128], tag[128], tag_object[128];
    char *tags[] = {tag_object, tag, tag_n, "text"};
    sprintf(tag_object, "%lxOBJ", x);

    for(i=0; i<x->x_number; i++) {
        sprintf(tag, "%lxBASE", x);
        sprintf(tag_n, "%lxBASE%d", x, i);
        pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle",
            0, 0, 0, 0, "-tags", 3, tags);

        sprintf(tag, "%lxBUT", x);
        sprintf(tag_n, "%lxBUT%d", x, i);
        pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle",
            0, 0, 0, 0, "-tags", 3, tags);
    }
    /* make sure the buttons are above their base */
    sprintf(tag, "%lxBUT", x);
    sprintf(tag_n, "%lxBASE", x);
    pdgui_vmess(0, "crss", canvas, "raise", tag, tag_n);

    sprintf(tag, "%lxLABEL", x);
    sprintf(tag_n, "label");
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text",
        0, 0, "-anchor", "w", "-tags", 4, tags);

    radio_draw_config(x, glist);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO);
}

static void radio_draw_select(t_radio* x, t_glist* glist)
{
    int n = x->x_number, i;
    t_canvas *canvas = glist_getcanvas(glist);
    int lcol = x->x_gui.x_lcol;
    int col = IEM_GUI_COLOR_NORMAL;
    char tag[128];

    if(x->x_gui.x_fsf.x_selected)
        lcol = col =  IEM_GUI_COLOR_SELECTED;

    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-outline", col);
    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-fill", lcol);
}

static void radio_draw_update(t_gobj *client, t_glist *glist)
{
    t_radio *x = (t_radio *)client;
    if(glist_isvisible(glist))
    {
        t_canvas *canvas = glist_getcanvas(glist);
        char tag[128];

        sprintf(tag, "%lxBUT%d", x, x->x_drawn);
        pdgui_vmess(0, "crs rk rk", canvas, "itemconfigure", tag,
            "-fill", x->x_gui.x_bcol,
            "-outline", x->x_gui.x_bcol);

        sprintf(tag, "%lxBUT%d", x, x->x_on);
        pdgui_vmess(0, "crs rk rk", canvas, "itemconfigure", tag,
            "-fill", x->x_gui.x_fcol,
            "-outline", x->x_gui.x_fcol);

        x->x_drawn = x->x_on;
    }
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
    int hchange = -1;
    const char*objname;

    if(x->x_orientation == horizontal)
    {
        objname = "hradio";
    } else {
        objname = "vradio";
    }
    if(x->x_compat)
        hchange = x->x_change;

    iemgui_new_dialog(x, &x->x_gui, objname,
        x->x_gui.x_w/IEMGUI_ZOOM(x), IEM_GUI_MINSIZE,
        0, 0,
        0, 0,
        0,
        hchange, "new-only", "new&old",
        1, -1, x->x_number);
}

static void radio_dialog(t_radio *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int a = (int)atom_getfloatarg(0, argc, argv);
    int chg = (int)atom_getfloatarg(4, argc, argv);
    int num = (int)atom_getfloatarg(6, argc, argv);
    int sr_flags;
    int redraw = 0;
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
    if (num != x->x_number && glist_isvisible(x->x_gui.x_glist))
    {
        /* we need to recreate the buttons */
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_ERASE);
        redraw = 1;
    }
    x->x_number = num;
    if(x->x_on >= x->x_number)
    {
        x->x_on_old = x->x_on = x->x_number - 1;
        x->x_on_old = x->x_on;
    }

    if (redraw && gobj_shouldvis((t_gobj *)x, x->x_gui.x_glist))
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_NEW);
        canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
    } else {
        /* just reconfigure */
        iemgui_size((void *)x, &x->x_gui);
    }
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
    iemgui_size(x, &x->x_gui);
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
    t_radio *x = (t_radio *)iemgui_new(radio_class);
    int a = IEM_GUI_DEFAULTSIZE, on = 0;
    int ldx = 0, ldy = -8 * IEM_GUI_DEFAULTSIZE_SCALE, chg = 1, num = 8;
    int fs = x->x_gui.x_fontsize;
    t_float fval = 0;
    if('v' == *s->s_name)
        x->x_orientation = vertical;
    x->x_compat = old;

    IEMGUI_SETDRAWFUNCTIONS(x, radio);

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
    x->x_gui.x_fsf.x_snd_able = (0 != x->x_gui.x_snd);
    x->x_gui.x_fsf.x_rcv_able = (0 != x->x_gui.x_rcv);
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
    x->x_gui.x_fontsize = (fs < 4)?4:fs;
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
    pdgui_stub_deleteforkey(x);
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

    class_sethelpsymbol(radio_class, gensym("radio"));
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
