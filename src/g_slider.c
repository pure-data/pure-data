/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* g_7_guis.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja
 * refactored 2022 by IOhannes m zmölnig
 */

#include <string.h>
#include <stdio.h>
#include "m_pd.h"

#include "g_all_guis.h"
#include <math.h>

#define LMARGIN 3
#define RMARGIN 2

#define TMARGIN 2
#define BMARGIN 3

/* ------------ horizontal/vertical  slider ----------------------- */

t_widgetbehavior slider_widgetbehavior;
static t_class *slider_class;

/* forward declarations */
static void slider_set(t_slider *x, t_floatarg f);

/* widget helper functions */

/* cannot use iemgui's default draw_iolets, because
 * - we have to deal with those stupid offsets,...
 * - we want to make sure that the iolets are below the KNOB (rather than the LABEL)
 */
static void slider_draw_io(t_slider* x, t_glist* glist, int old_snd_rcv_flags)
{
    const int zoom = IEMGUI_ZOOM(x);
    t_canvas *canvas = glist_getcanvas(glist);
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int iow = IOWIDTH * zoom, ioh = IEM_GUI_IOHEIGHT * zoom;
    int lmargin = 0, tmargin=0, bmargin = 0;
    char tag_object[128], tag_knob[128], tag[128];
    char *tags[] = {tag_object, tag};

    (void)old_snd_rcv_flags;

    sprintf(tag_object, "%lxOBJ", x);
    sprintf(tag_knob, "%lxKNOB", x);

    if(x->x_orientation == horizontal)
    {
        lmargin = LMARGIN * zoom;
    } else {
        tmargin = TMARGIN * zoom;
        bmargin = BMARGIN * zoom;
    }

    sprintf(tag, "%lxOUT%d", x, 0);
    pdgui_vmess(0, "crs", canvas, "delete", tag);
    if(!x->x_gui.x_fsf.x_snd_able)
    {
        pdgui_vmess(0, "crr iiii rs rS", canvas, "create", "rectangle",
            xpos - lmargin, ypos + x->x_gui.x_h + bmargin + zoom - ioh,
            xpos - lmargin + iow, ypos + x->x_gui.x_h + bmargin,
            "-fill", "black",
            "-tags", 2, tags);

            /* keep knob above outlet */
        pdgui_vmess(0, "crss", canvas, "lower", tag, tag_knob);
    }

    sprintf(tag, "%lxIN%d", x, 0);
    pdgui_vmess(0, "crs", canvas, "delete", tag);
    if(!x->x_gui.x_fsf.x_rcv_able)
    {
        pdgui_vmess(0, "crr iiii rs rS", canvas, "create", "rectangle",
            xpos - lmargin, ypos - tmargin,
            xpos - lmargin + iow, ypos - tmargin - zoom + ioh,
            "-fill", "black",
            "-tags", 2, tags);

            /* keep knob above inlet */
        pdgui_vmess(0, "crss", canvas, "lower", tag, tag_knob);
    }
}

static void slider_knob_position(t_slider*x, t_glist *glist, int val, int *x0, int *y0, int *x1, int *y1)
{
    const int zoom = IEMGUI_ZOOM(x);
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    if(x->x_orientation == horizontal)
    {
        int r = xpos + val;
        *x0 = r;
        *y0 = ypos + (zoom + 1);
        *x1 = r;
        *y1 = ypos + x->x_gui.x_h - (zoom * 2);
    } else {
        int r = ypos + x->x_gui.x_h - val;
        *x0 = xpos + (zoom + 1);
        *y0 = r;
        *x1 = xpos + x->x_gui.x_w - (zoom * 2);
        *y1 = r;
    }

}

static void slider_draw_config(t_slider* x, t_glist* glist)
{
    const int zoom = IEMGUI_ZOOM(x);
    t_canvas *canvas = glist_getcanvas(glist);
    t_iemgui *iemgui = &x->x_gui;
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int val = ((x->x_val + 50)/100);
    int iow = IOWIDTH * zoom, ioh = IEM_GUI_IOHEIGHT * zoom;
    int lmargin = 0, rmargin = 0, tmargin = 0, bmargin = 0;
    int a, b, c, d;
    char tag[128];
    t_atom fontatoms[3];
    SETSYMBOL(fontatoms+0, gensym(iemgui->x_font));
    SETFLOAT (fontatoms+1, -iemgui->x_fontsize*zoom);
    SETSYMBOL(fontatoms+2, gensym(sys_fontweight));

    if(x->x_orientation == horizontal)
    {
        lmargin = LMARGIN * zoom;
        rmargin = RMARGIN * zoom;
    } else {
        tmargin = TMARGIN * zoom;
        bmargin = BMARGIN * zoom;
    }
    slider_knob_position(x, glist, val, &a, &b, &c, &d);

    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
        xpos - lmargin, ypos - tmargin,
        xpos + x->x_gui.x_w + rmargin, ypos + x->x_gui.x_h + bmargin);
    pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
        "-width", zoom,
        "-fill", x->x_gui.x_bcol);

    sprintf(tag, "%lxKNOB", x);
    pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
        a, b, c, d);
    pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
        "-width", 1 + 2 * zoom,
        "-outline", x->x_gui.x_fcol);

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crs ii", canvas, "coords", tag,
        xpos + x->x_gui.x_ldx * zoom, ypos + x->x_gui.x_ldy * zoom);

    pdgui_vmess(0, "crs rA rk", canvas, "itemconfigure", tag,
        "-font", 3, fontatoms,
        "-fill", (x->x_gui.x_fsf.x_selected ? IEM_GUI_COLOR_SELECTED : x->x_gui.x_lcol));
    iemgui_dolabel(x, &x->x_gui, x->x_gui.x_lab, 1);
}

static void slider_draw_new(t_slider *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    char tag[128], tag_object[128];
    char*tags[] = {tag_object, tag, "label", "text"};
    sprintf(tag_object, "%lxOBJ", x);


    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle",
         0, 0, 0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxKNOB", x);
    pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle",
         0, 0, 0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text",
         0, 0, "-anchor", "w", "-tags", 4, tags);

    slider_draw_config(x, glist);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO);
}

static void slider_draw_select(t_slider* x, t_glist* glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    int col = IEM_GUI_COLOR_NORMAL, lcol = x->x_gui.x_lcol;
    char tag[128];

    if(x->x_gui.x_fsf.x_selected)
        col = lcol = IEM_GUI_COLOR_SELECTED;

    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-outline", col);
    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-fill", lcol);
}

static void slider_draw_update(t_gobj *client, t_glist *glist)
{
    t_slider *x = (t_slider *)client;
    int a, b, c, d;
    if (glist_isvisible(glist))
    {
        const int zoom = IEMGUI_ZOOM(x);
        t_canvas *canvas = glist_getcanvas(glist);
        int xpos = text_xpix(&x->x_gui.x_obj, glist);
        int ypos = text_ypix(&x->x_gui.x_obj, glist);
        int val = ((x->x_val + 50) / 100) * zoom;
        char tag[128];
        sprintf(tag, "%lxKNOB", x);

        slider_knob_position(x, glist, val, &a, &b, &c, &d);
        pdgui_vmess(0, "crs iiii", canvas, "coords", tag, a, b, c, d);
    }
}


/* ------------------------ hsl widgetbehaviour----------------------------- */


static void slider_getrect(t_gobj *z, t_glist *glist,
                            int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_slider* x = (t_slider*)z;
    int zoom = glist_getzoom(glist);
    int dx1=0, dx2=0, dy1=0, dy2=0;
    if(x->x_orientation == horizontal)
    {
        dx1 = LMARGIN*zoom;
        dx2 = (LMARGIN + RMARGIN)*zoom;
    } else {
        dy1 = TMARGIN*zoom;
        dy2 = (TMARGIN + BMARGIN)*zoom;
    }


    *xp1 = text_xpix(&x->x_gui.x_obj, glist) - dx1;
    *yp1 = text_ypix(&x->x_gui.x_obj, glist) - dy1;

    *xp2 = *xp1 + x->x_gui.x_w + dx2;
    *yp2 = *yp1 + x->x_gui.x_h + dy2;
}

static void slider_save(t_gobj *z, t_binbuf *b)
{
    t_slider *x = (t_slider *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiffiisssiiiisssii", gensym("#X"), gensym("obj"),
                (int)x->x_gui.x_obj.te_xpix, (int)x->x_gui.x_obj.te_ypix,
                gensym((x->x_orientation==horizontal)?"hsl":"vsl"),
                x->x_gui.x_w/IEMGUI_ZOOM(x), x->x_gui.x_h/IEMGUI_ZOOM(x),
                (t_float)x->x_min, (t_float)x->x_max,
                x->x_lin0_log1, iem_symargstoint(&x->x_gui.x_isa),
                srl[0], srl[1], srl[2],
                x->x_gui.x_ldx, x->x_gui.x_ldy,
                iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize,
                bflcol[0], bflcol[1], bflcol[2],
                x->x_gui.x_isa.x_loadinit?x->x_val:0, x->x_steady);
    binbuf_addv(b, ";");
}

static int slider_check_range(t_slider *x, int v)
{
    if(v < IEM_SL_MINSIZE * IEMGUI_ZOOM(x))
        v = IEM_SL_MINSIZE * IEMGUI_ZOOM(x);
    if(x->x_val > (v * 100 - 100))
    {
        x->x_val = v * 100 - 100;
    }
    if(x->x_lin0_log1)
        x->x_k = log(x->x_max / x->x_min) / (double)(v/IEMGUI_ZOOM(x) - 1);
    else
        x->x_k = (x->x_max - x->x_min) / (double)(v/IEMGUI_ZOOM(x) - 1);

    return v;
}

static void slider_check_minmax(t_slider *x, double min, double max, t_float value)
{
    if(x->x_lin0_log1)
    {
        if((min == 0.0) && (max == 0.0))
            max = 1.0;
        if(max > 0.0)
        {
            if(min <= 0.0)
                min = 0.01 * max;
        }
        else
        {
            if(min > 0.0)
                max = 0.01 * min;
        }
    }
    x->x_min = min;
    x->x_max = max;
    if(x->x_lin0_log1)
        x->x_k = log(x->x_max/x->x_min) / (double)(value/IEMGUI_ZOOM(x) - 1);
    else
        x->x_k = (x->x_max - x->x_min) / (double)(value/IEMGUI_ZOOM(x) - 1);
}

static void slider_properties(t_gobj *z, t_glist *owner)
{
    t_slider *x = (t_slider *)z;
    const char*objname, *rangeA, *rangeB;
    int minWidth, minHeight;

    if(x->x_orientation == horizontal)
    {
        objname = "hsl";
        minWidth = IEM_SL_MINSIZE;
        minHeight = IEM_GUI_MINSIZE;
    } else {
        objname = "vsl";
        minWidth = IEM_GUI_MINSIZE;
        minHeight = IEM_SL_MINSIZE;
    }


    iemgui_new_dialog(x, &x->x_gui, objname,
                      x->x_gui.x_w/IEMGUI_ZOOM(x), minWidth,
                      x->x_gui.x_h/IEMGUI_ZOOM(x), minHeight,
                      x->x_min, x->x_max,
                      0,
                      x->x_lin0_log1, "linear", "logarithmic",
                      1, x->x_steady, -1);
}

    /* compute numeric value (fval) from pixel location (val) and range */
static t_float slider_getfval(t_slider *x)
{
    t_float fval;
    int rounded_val = (x->x_gui.x_fsf.x_finemoved) ? x->x_val : (x->x_val / 100) * 100;

    /* if rcv==snd, don't round the value to prevent bad dragging when zoomed-in */
    if(x->x_gui.x_fsf.x_snd_able && (x->x_gui.x_snd == x->x_gui.x_rcv))
        rounded_val = x->x_val;

    if (x->x_lin0_log1)
        fval = x->x_min * exp(x->x_k * (double)(rounded_val) * 0.01);
    else fval = (double)(rounded_val) * 0.01 * x->x_k + x->x_min;
    if ((fval < 1.0e-10) && (fval > -1.0e-10))
        fval = 0.0;
    return (fval);
}

static void slider_bang(t_slider *x)
{
    double out;

    if (pd_compatibilitylevel < 46)
        out = slider_getfval(x);
    else out = x->x_fval;
    outlet_float(x->x_gui.x_obj.ob_outlet, out);
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
        pd_float(x->x_gui.x_snd->s_thing, out);
}

static void slider_dialog(t_slider *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int w = (int)atom_getfloatarg(0, argc, argv);
    int h = (int)atom_getfloatarg(1, argc, argv);
    double min = (double)atom_getfloatarg(2, argc, argv);
    double max = (double)atom_getfloatarg(3, argc, argv);
    int lilo = (int)atom_getfloatarg(4, argc, argv);
    int steady = (int)atom_getfloatarg(17, argc, argv);
    int sr_flags;
    t_atom undo[18];

    if(x->x_orientation == horizontal)
        w *= IEMGUI_ZOOM(x);
    else
        h *= IEMGUI_ZOOM(x);

    iemgui_setdialogatoms(&x->x_gui, 18, undo);
    SETFLOAT(undo+2, x->x_min);
    SETFLOAT(undo+3, x->x_max);
    SETFLOAT(undo+4, x->x_lin0_log1);
    SETFLOAT(undo+17, x->x_steady);

    pd_undo_set_objectstate(x->x_gui.x_glist, (t_pd*)x, gensym("dialog"),
                            18, undo,
                            argc, argv);

    x->x_lin0_log1 = !!lilo;
    x->x_steady = !!steady;

    sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);

    if(x->x_orientation == horizontal)
    {
        x->x_gui.x_h = iemgui_clip_size(h) * IEMGUI_ZOOM(x);
        x->x_gui.x_w = slider_check_range(x, w);
        slider_check_minmax(x, min, max, x->x_gui.x_w);
    } else {
        x->x_gui.x_h = slider_check_range(x, h);
        x->x_gui.x_w = iemgui_clip_size(w) * IEMGUI_ZOOM(x);
        slider_check_minmax(x, min, max, x->x_gui.x_h);
    }

    iemgui_size(x, &x->x_gui);
    slider_set(x, x->x_fval);
}

static void slider_motion(t_slider *x, t_floatarg dx, t_floatarg dy,
    t_floatarg up)
{
    int old = x->x_val;
    int pos;
    float delta;

    if (up != 0)
        return;

    if(x->x_orientation == horizontal)
    {
        pos = x->x_gui.x_w / IEMGUI_ZOOM(x);
        delta = dx;
    } else {
        pos = x->x_gui.x_h / IEMGUI_ZOOM(x);
        delta = -dy;
    }

    if(!x->x_gui.x_fsf.x_finemoved)
        delta = (100 * delta) / IEMGUI_ZOOM(x);

    x->x_pos += (int)delta;
    x->x_val = x->x_pos;

    if(x->x_val > (100 * pos - 100))
    {
        x->x_val = 100 * pos - 100;
        x->x_pos += 50 / IEMGUI_ZOOM(x);
        x->x_pos -= x->x_pos % (100 / IEMGUI_ZOOM(x));
    }
    if(x->x_val < 0)
    {
        x->x_val = 0;
        x->x_pos -= 50 / IEMGUI_ZOOM(x);
        x->x_pos -= x->x_pos % (100 / IEMGUI_ZOOM(x));
    }
    x->x_fval = slider_getfval(x);
    if (old != x->x_val)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        slider_bang(x);
    }
}

static void slider_click(t_slider *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    t_float val;
    int maxval;
    if(x->x_orientation == horizontal)
    {
        val = (xpos - text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist));
        maxval = x->x_gui.x_w / IEMGUI_ZOOM(x);
    } else {
        val = (x->x_gui.x_h + text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist) - ypos);
        maxval = x->x_gui.x_h / IEMGUI_ZOOM(x);
    }
    maxval = 100 * maxval - 100;

    if(!x->x_steady)
        x->x_val = (int)((100.0 * val) / IEMGUI_ZOOM(x));
    if(x->x_val > maxval)
        x->x_val = maxval;

    if(x->x_val < 0)
        x->x_val = 0;
    x->x_fval = slider_getfval(x);
    x->x_pos = x->x_val;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    slider_bang(x);
    glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
        (t_glistmotionfn)slider_motion, 0, xpos, ypos);
}

static int slider_newclick(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_slider* x = (t_slider *)z;

    if(doit)
    {
        slider_click(x, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift,
            0, (t_floatarg)alt);
        if(shift)
            x->x_gui.x_fsf.x_finemoved = 1;
        else
            x->x_gui.x_fsf.x_finemoved = 0;
    }
    return (1);
}

static void slider_set(t_slider *x, t_floatarg f)
{
    int old = x->x_val;
    double g;

    x->x_fval = f;
    if (x->x_min > x->x_max)
    {
        if(f > x->x_min)
            f = x->x_min;
        if(f < x->x_max)
            f = x->x_max;
    }
    else
    {
        if(f > x->x_max)
            f = x->x_max;
        if(f < x->x_min)
            f = x->x_min;
    }
    if(x->x_lin0_log1)
        g = log(f/x->x_min) / x->x_k;
    else
        g = (f - x->x_min) / x->x_k;
    x->x_val = (int)(100.0*g + 0.49999);
    if(x->x_val != old)
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void slider_float(t_slider *x, t_floatarg f)
{
    slider_set(x, f);
    if(x->x_gui.x_fsf.x_put_in2out)
        slider_bang(x);
}

static void slider_size(t_slider *x, t_symbol *s, int ac, t_atom *av)
{
    int w = (int)atom_getfloatarg(0, ac, av);
    int h = (int)atom_getfloatarg(1, ac, av);
    if(x->x_orientation == horizontal)
    {
        x->x_gui.x_w = slider_check_range(x, w*IEMGUI_ZOOM(x));
        if(ac > 1)
            x->x_gui.x_h = iemgui_clip_size(h) * IEMGUI_ZOOM(x);
    } else {
        x->x_gui.x_w = iemgui_clip_size(w) * IEMGUI_ZOOM(x);
        if(ac > 1)
            x->x_gui.x_h = slider_check_range(x, h*IEMGUI_ZOOM(x));
    }
    iemgui_size((void *)x, &x->x_gui);
    slider_set(x, x->x_fval);
}

static void slider_delta(t_slider *x, t_symbol *s, int ac, t_atom *av)
{iemgui_delta((void *)x, &x->x_gui, s, ac, av);}

static void slider_pos(t_slider *x, t_symbol *s, int ac, t_atom *av)
{iemgui_pos((void *)x, &x->x_gui, s, ac, av);}

static void slider_range(t_slider *x, t_symbol *s, int ac, t_atom *av)
{
    slider_check_minmax(x,
        (double)atom_getfloatarg(0, ac, av),
        (double)atom_getfloatarg(1, ac, av),
        (x->x_orientation==horizontal)?x->x_gui.x_w:x->x_gui.x_h);
    slider_set(x, x->x_fval);
}

static void slider_color(t_slider *x, t_symbol *s, int ac, t_atom *av)
{iemgui_color((void *)x, &x->x_gui, s, ac, av);}

static void slider_send(t_slider *x, t_symbol *s)
{iemgui_send(x, &x->x_gui, s);}

static void slider_receive(t_slider *x, t_symbol *s)
{iemgui_receive(x, &x->x_gui, s);}

static void slider_label(t_slider *x, t_symbol *s)
{iemgui_label((void *)x, &x->x_gui, s);}

static void slider_label_pos(t_slider *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_pos((void *)x, &x->x_gui, s, ac, av);}

static void slider_label_font(t_slider *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_font((void *)x, &x->x_gui, s, ac, av);}

static void slider_log(t_slider *x)
{
    x->x_lin0_log1 = 1;
    slider_check_minmax(x, x->x_min, x->x_max,
        (x->x_orientation==horizontal)?x->x_gui.x_w:x->x_gui.x_h);
    slider_set(x, x->x_fval);
}

static void slider_lin(t_slider *x)
{
    double v = (x->x_orientation==horizontal)?x->x_gui.x_w:x->x_gui.x_h;
    x->x_lin0_log1 = 0;
    x->x_k = (x->x_max - x->x_min) / (v/IEMGUI_ZOOM(x) - 1);
    slider_set(x, x->x_fval);
}

static void slider_init(t_slider *x, t_floatarg f)
{
    x->x_gui.x_isa.x_loadinit = (f == 0.0) ? 0 : 1;
}

static void slider_steady(t_slider *x, t_floatarg f)
{
    x->x_steady = (f == 0.0) ? 0 : 1;
}

static void slider_orientation(t_slider *x, t_floatarg forient)
{
    int orient = !!(int)forient;
    if(orient != x->x_orientation)
    {
        int w = x->x_gui.x_w;
        x->x_gui.x_w = x->x_gui.x_h;
        x->x_gui.x_h = w;

            /* urgh. for historical reasons,
             * sliders have a different offset depending on their orientation.
             * for backwards-compatibility, an ideal solution would handle
             * these offsets during load/save,
             * but we cannot access the object position in the constructor,
             * so we need to do it here...
             */
        x->x_gui.x_obj.te_xpix += LMARGIN * ((horizontal==orient)?1:-1);
        x->x_gui.x_obj.te_ypix -= TMARGIN * ((horizontal==orient)?1:-1);
    }
    x->x_orientation = orient;

    iemgui_size(x, &x->x_gui);
}

static void slider_zoom(t_slider *x, t_floatarg f)
{
    iemgui_zoom(&x->x_gui, f);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void slider_loadbang(t_slider *x, t_floatarg action)
{
    if (action == LB_LOAD && x->x_gui.x_isa.x_loadinit)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        slider_bang(x);
    }
}

static void *slider_new(t_symbol *s, int argc, t_atom *argv)
{
    t_slider *x = (t_slider *)iemgui_new(slider_class);
    int lilo = 0, steady = 1;
    int fs = x->x_gui.x_fontsize;
    double min = 0.0, max = (double)(IEM_SL_DEFAULTSIZE-1);
    t_float v = 0;
    int w, h, ldx, ldy;

    IEMGUI_SETDRAWFUNCTIONS(x, slider);

    if('v' == *s->s_name)
        x->x_orientation = vertical;

    if(x->x_orientation == horizontal)
    {
        w = IEM_SL_DEFAULTSIZE;
        h = IEM_GUI_DEFAULTSIZE;
        w *= IEM_GUI_DEFAULTSIZE_SCALE; /* keep aspect ratio */

        ldx = -2;
        ldy = -8 * IEM_GUI_DEFAULTSIZE_SCALE;
    } else {
        w = IEM_GUI_DEFAULTSIZE;
        h = IEM_SL_DEFAULTSIZE;
        h *= IEM_GUI_DEFAULTSIZE_SCALE; /* keep aspect ratio */

        ldx = 0;
        ldy = -9;
    }

    if(((argc == 17)||(argc == 18))&&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)
       &&IS_A_FLOAT(argv,2)&&IS_A_FLOAT(argv,3)
       &&IS_A_FLOAT(argv,4)&&IS_A_FLOAT(argv,5)
       &&(IS_A_SYMBOL(argv,6)||IS_A_FLOAT(argv,6))
       &&(IS_A_SYMBOL(argv,7)||IS_A_FLOAT(argv,7))
       &&(IS_A_SYMBOL(argv,8)||IS_A_FLOAT(argv,8))
       &&IS_A_FLOAT(argv,9)&&IS_A_FLOAT(argv,10)
       &&IS_A_FLOAT(argv,11)&&IS_A_FLOAT(argv,12)&&IS_A_FLOAT(argv,16))
    {
        w = (int)atom_getfloatarg(0, argc, argv);
        h = (int)atom_getfloatarg(1, argc, argv);
        min = (double)atom_getfloatarg(2, argc, argv);
        max = (double)atom_getfloatarg(3, argc, argv);
        lilo = (int)atom_getfloatarg(4, argc, argv);
        iem_inttosymargs(&x->x_gui.x_isa, atom_getfloatarg(5, argc, argv));
        iemgui_new_getnames(&x->x_gui, 6, argv);
        ldx = (int)atom_getfloatarg(9, argc, argv);
        ldy = (int)atom_getfloatarg(10, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getfloatarg(11, argc, argv));
        fs = (int)atom_getfloatarg(12, argc, argv);
        iemgui_all_loadcolors(&x->x_gui, argv+13, argv+14, argv+15);
        v = atom_getfloatarg(16, argc, argv);
    }
    else iemgui_new_getnames(&x->x_gui, 6, 0);
    if((argc == 18)&&IS_A_FLOAT(argv,17))
        steady = (int)atom_getfloatarg(17, argc, argv);
    x->x_gui.x_fsf.x_snd_able = (0 != x->x_gui.x_snd);
    x->x_gui.x_fsf.x_rcv_able = (0 != x->x_gui.x_rcv);
    if (x->x_gui.x_isa.x_loadinit)
        x->x_val = v;
    else x->x_val = 0;
    if(lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;
    if(steady != 0) steady = 1;
    x->x_steady = steady;
    if(x->x_gui.x_fsf.x_font_style == 1) strcpy(x->x_gui.x_font, "helvetica");
    else if(x->x_gui.x_fsf.x_font_style == 2) strcpy(x->x_gui.x_font, "times");
    else { x->x_gui.x_fsf.x_font_style = 0;
        strcpy(x->x_gui.x_font, sys_font); }
    if(x->x_gui.x_fsf.x_rcv_able) pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;
    x->x_gui.x_fontsize = (fs < 4)?4:fs;

    if(x->x_orientation == horizontal)
    {
        x->x_gui.x_w = slider_check_range(x, w);
        x->x_gui.x_h = iemgui_clip_size(h);
    } else {
        x->x_gui.x_w = iemgui_clip_size(w);
        x->x_gui.x_h = slider_check_range(x, h);
    }

    iemgui_verify_snd_ne_rcv(&x->x_gui);
    iemgui_newzoom(&x->x_gui);
    slider_check_minmax(x, min, max,
        (x->x_orientation==horizontal)?x->x_gui.x_w:x->x_gui.x_h);

    outlet_new(&x->x_gui.x_obj, &s_float);
    x->x_fval = slider_getfval(x);
    return (x);
}

static void slider_free(t_slider *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    pdgui_stub_deleteforkey(x);
}

void g_slider_setup(void)
{
    slider_class = class_new(gensym("hsl"), (t_newmethod)slider_new,
        (t_method)slider_free, sizeof(t_slider), 0, A_GIMME, 0);
    class_addcreator((t_newmethod)slider_new, gensym("vsl"), A_GIMME, 0);
    class_addcreator((t_newmethod)slider_new, gensym("hslider"), A_GIMME, 0);
    class_addcreator((t_newmethod)slider_new, gensym("vslider"), A_GIMME, 0);

    class_addbang(slider_class, slider_bang);
    class_addfloat(slider_class, slider_float);
    class_addmethod(slider_class, (t_method)slider_click,
        gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(slider_class, (t_method)slider_motion,
        gensym("motion"), A_FLOAT, A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(slider_class, (t_method)slider_dialog,
        gensym("dialog"), A_GIMME, 0);
    class_addmethod(slider_class, (t_method)slider_loadbang,
        gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(slider_class, (t_method)slider_set,
        gensym("set"), A_FLOAT, 0);
    class_addmethod(slider_class, (t_method)slider_size,
        gensym("size"), A_GIMME, 0);
    class_addmethod(slider_class, (t_method)slider_delta,
        gensym("delta"), A_GIMME, 0);
    class_addmethod(slider_class, (t_method)slider_pos,
        gensym("pos"), A_GIMME, 0);
    class_addmethod(slider_class, (t_method)slider_range,
        gensym("range"), A_GIMME, 0);
    class_addmethod(slider_class, (t_method)slider_color,
        gensym("color"), A_GIMME, 0);
    class_addmethod(slider_class, (t_method)slider_send,
        gensym("send"), A_DEFSYM, 0);
    class_addmethod(slider_class, (t_method)slider_receive,
        gensym("receive"), A_DEFSYM, 0);
    class_addmethod(slider_class, (t_method)slider_label,
        gensym("label"), A_DEFSYM, 0);
    class_addmethod(slider_class, (t_method)slider_label_pos,
        gensym("label_pos"), A_GIMME, 0);
    class_addmethod(slider_class, (t_method)slider_label_font,
        gensym("label_font"), A_GIMME, 0);
    class_addmethod(slider_class, (t_method)slider_log,
        gensym("log"), 0);
    class_addmethod(slider_class, (t_method)slider_lin,
        gensym("lin"), 0);
    class_addmethod(slider_class, (t_method)slider_init,
        gensym("init"), A_FLOAT, 0);
    class_addmethod(slider_class, (t_method)slider_steady,
        gensym("steady"), A_FLOAT, 0);
    class_addmethod(slider_class, (t_method)slider_orientation,
        gensym("orientation"), A_FLOAT, 0);
    class_addmethod(slider_class, (t_method)slider_zoom,
        gensym("zoom"), A_CANT, 0);
    slider_widgetbehavior.w_getrectfn =    slider_getrect;
    slider_widgetbehavior.w_displacefn =   iemgui_displace;
    slider_widgetbehavior.w_selectfn =     iemgui_select;
    slider_widgetbehavior.w_activatefn =   NULL;
    slider_widgetbehavior.w_deletefn =     iemgui_delete;
    slider_widgetbehavior.w_visfn =        iemgui_vis;
    slider_widgetbehavior.w_clickfn =      slider_newclick;
    class_setwidget(slider_class, &slider_widgetbehavior);
    class_sethelpsymbol(slider_class, gensym("sliders"));
    class_setsavefn(slider_class, slider_save);
    class_setpropertiesfn(slider_class, slider_properties);
}
