/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* my_numbox.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "m_pd.h"

#include "g_all_guis.h"
#include <math.h>

#define MINDIGITS 1
#define MINFONT   4

/*------------------ global functions -------------------------*/

static void my_numbox_key(void *z, t_symbol *keysym, t_floatarg fkey);
static void my_numbox_draw_update(t_gobj *client, t_glist *glist);

void my_numbox_clip(t_my_numbox *x)
{
    if(x->x_val < x->x_min)
        x->x_val = x->x_min;
    if(x->x_val > x->x_max)
        x->x_val = x->x_max;
}

void my_numbox_calc_fontwidth(t_my_numbox *x)
{
    int w, f = 31;

    if(x->x_gui.x_fsf.x_font_style == 1)
        f = 27;
    else if(x->x_gui.x_fsf.x_font_style == 2)
        f = 25;

    w = x->x_gui.x_fontsize * f * x->x_numwidth;
    w /= 36;
    x->x_gui.x_w = (w + (x->x_gui.x_h/2)/IEMGUI_ZOOM(x) + 4) * IEMGUI_ZOOM(x);
}

void my_numbox_ftoa(t_my_numbox *x)
{
    double f = x->x_val;
    int bufsize, is_exp = 0, i, idecimal;

    sprintf(x->x_buf, "%g", f);
    bufsize = (int)strlen(x->x_buf);
    if(bufsize >= 5)/* if it is in exponential mode */
    {
        i = bufsize - 4;
        if((x->x_buf[i] == 'e') || (x->x_buf[i] == 'E'))
            is_exp = 1;
    }
    if(bufsize > x->x_numwidth)/* if to reduce */
    {
        if(is_exp)
        {
            if(x->x_numwidth <= 5)
            {
                x->x_buf[0] = (f < 0.0 ? '-' : '+');
                x->x_buf[1] = 0;
            }
            i = bufsize - 4;
            for(idecimal = 0; idecimal < i; idecimal++)
                if(x->x_buf[idecimal] == '.')
                    break;
            if(idecimal > (x->x_numwidth - 4))
            {
                x->x_buf[0] = (f < 0.0 ? '-' : '+');
                x->x_buf[1] = 0;
            }
            else
            {
                int new_exp_index = x->x_numwidth - 4;
                int old_exp_index = bufsize - 4;

                for(i = 0; i < 4; i++, new_exp_index++, old_exp_index++)
                    x->x_buf[new_exp_index] = x->x_buf[old_exp_index];
                x->x_buf[x->x_numwidth] = 0;
            }
        }
        else
        {
            for(idecimal = 0; idecimal < bufsize; idecimal++)
                if(x->x_buf[idecimal] == '.')
                    break;
            if(idecimal > x->x_numwidth)
            {
                x->x_buf[0] = (f < 0.0 ? '-' : '+');
                x->x_buf[1] = 0;
            }
            else
                x->x_buf[x->x_numwidth] = 0;
        }
    }
}

/* ------------ nbx gui-my number box ----------------------- */

t_widgetbehavior my_numbox_widgetbehavior;
static t_class *my_numbox_class;

#define my_numbox_draw_io 0

static void my_numbox_draw_config(t_my_numbox* x, t_glist* glist)
{
    const int zoom = IEMGUI_ZOOM(x);
    t_canvas *canvas = glist_getcanvas(glist);
    t_iemgui *iemgui = &x->x_gui;
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int w = x->x_gui.x_w, half = x->x_gui.x_h/2;
    int d = zoom + x->x_gui.x_h/(34*zoom);
    int corner = x->x_gui.x_h/4;
    int iow = IOWIDTH * zoom, ioh = IEM_GUI_IOHEIGHT * zoom;
    char tag[128];

    int lcol = x->x_gui.x_lcol;
    int fcol = x->x_gui.x_fcol;
    t_atom fontatoms[3];
    SETSYMBOL(fontatoms+0, gensym(iemgui->x_font));
    SETFLOAT (fontatoms+1, -iemgui->x_fontsize*zoom);
    SETSYMBOL(fontatoms+2, gensym(sys_fontweight));

    if(x->x_gui.x_fsf.x_selected)
        fcol = lcol = IEM_GUI_COLOR_SELECTED;
    if(x->x_gui.x_fsf.x_change)
        fcol =  IEM_GUI_COLOR_EDITED;

    my_numbox_ftoa(x);

    sprintf(tag, "%lxBASE1", x);
    pdgui_vmess(0, "crs  ii ii ii ii ii ii", canvas, "coords", tag,
        xpos,              ypos,
        xpos + w - corner, ypos,
        xpos + w,          ypos + corner,
        xpos + w,          ypos + x->x_gui.x_h,
        xpos,              ypos + x->x_gui.x_h,
        xpos,              ypos);
    pdgui_vmess(0, "crs  ri rk rk", canvas, "itemconfigure", tag,
        "-width", zoom,
        "-outline", IEM_GUI_COLOR_NORMAL,
        "-fill", x->x_gui.x_bcol);


    sprintf(tag, "%lxBASE2", x);
    pdgui_vmess(0, "crs  ii ii ii", canvas, "coords", tag,
        xpos + zoom, ypos + zoom,
        xpos + half, ypos + half,
        xpos + zoom, ypos + x->x_gui.x_h - zoom);
    pdgui_vmess(0, "crs  ri rk", canvas, "itemconfigure", tag,
        "-width", zoom,
        "-fill", x->x_gui.x_fcol);

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crs  ii", canvas, "coords", tag,
        xpos + x->x_gui.x_ldx * zoom,
        ypos + x->x_gui.x_ldy * zoom);
    pdgui_vmess(0, "crs  rA rk", canvas, "itemconfigure", tag,
        "-font", 3, fontatoms,
        "-fill", lcol);
    iemgui_dolabel(x, &x->x_gui, x->x_gui.x_lab, 1);

    sprintf(tag, "%lxNUMBER", x);
    pdgui_vmess(0, "crs  ii", canvas, "coords", tag,
        xpos + half + 2*zoom, ypos + half + d);
    pdgui_vmess(0, "crs  rs rA rk", canvas, "itemconfigure", tag,
        "-text", x->x_buf,
        "-font", 3, fontatoms,
        "-fill", fcol);
}

static void my_numbox_draw_new(t_my_numbox *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    char tag[128], tag_object[128];
    char*tags[] = {tag_object, tag, "label", "text"};
    sprintf(tag_object, "%lxOBJ", x);

    sprintf(tag, "%lxBASE1", x);
    pdgui_vmess(0, "crr ii rS", canvas, "create", "polygon",
        0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxBASE2", x);
    pdgui_vmess(0, "crr iiii rS", canvas, "create", "line",
        0, 0, 0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text",
        0, 0, "-anchor", "w", "-tags", 4, tags);

    sprintf(tag, "%lxNUMBER", x);
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text",
        0, 0, "-anchor", "w", "-tags", 2, tags);

    my_numbox_draw_config(x, glist);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO);
}

static void my_numbox_draw_select(t_my_numbox *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    int bcol = IEM_GUI_COLOR_NORMAL, lcol = x->x_gui.x_lcol, fcol = x->x_gui.x_fcol;
    char tag[128];

    if(x->x_gui.x_fsf.x_selected)
    {
        if(x->x_gui.x_fsf.x_change)
        {
            x->x_gui.x_fsf.x_change = 0;
            x->x_buf[0] = 0;
            sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
        }
        bcol = lcol = fcol = IEM_GUI_COLOR_SELECTED;
    }

    sprintf(tag, "%lxBASE1", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-outline", bcol);
    sprintf(tag, "%lxBASE2", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-fill", fcol);
    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-fill", lcol);
    sprintf(tag, "%lxNUMBER", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-fill", fcol);
}

static void my_numbox_draw_update(t_gobj *client, t_glist *glist)
{
    t_my_numbox *x = (t_my_numbox *)client;
    if(glist_isvisible(glist))
    {
        t_canvas *canvas = glist_getcanvas(glist);
        char tag[128];
        sprintf(tag, "%lxNUMBER", x);
        if(x->x_gui.x_fsf.x_change)
        {
            if(x->x_buf[0])
            {
                char *cp = x->x_buf;
                int sl = (int)strlen(x->x_buf);

                x->x_buf[sl] = '>';
                x->x_buf[sl+1] = 0;
                if(sl >= x->x_numwidth)
                    cp += sl - x->x_numwidth + 1;
                pdgui_vmess(0, "crs rk rs", canvas, "itemconfigure", tag,
                    "-fill", IEM_GUI_COLOR_EDITED, "-text", cp);
                x->x_buf[sl] = 0;
            }
            else
            {
                my_numbox_ftoa(x);
                pdgui_vmess(0, "crs rk rs", canvas, "itemconfigure", tag,
                    "-fill", IEM_GUI_COLOR_EDITED, "-text", x->x_buf);
                x->x_buf[0] = 0;
            }
        }
        else
        {
            my_numbox_ftoa(x);
                pdgui_vmess(0, "crs rk rs", canvas, "itemconfigure", tag,
                    "-fill", (x->x_gui.x_fsf.x_selected ? IEM_GUI_COLOR_SELECTED : x->x_gui.x_fcol),
                    "-text", x->x_buf);
            x->x_buf[0] = 0;
        }
    }
}

/* widget helper functions */

static void my_numbox_tick_wait(t_my_numbox *x)
{
    sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
}

/* ------------------------ nbx widgetbehaviour----------------------------- */

static void my_numbox_getrect(t_gobj *z, t_glist *glist,
                              int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_my_numbox* x = (t_my_numbox*)z;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist);
    *xp2 = *xp1 + x->x_gui.x_w;
    *yp2 = *yp1 + x->x_gui.x_h;
}

static void my_numbox_save(t_gobj *z, t_binbuf *b)
{
    t_my_numbox *x = (t_my_numbox *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];

    iemgui_save(&x->x_gui, srl, bflcol);
    if(x->x_gui.x_fsf.x_change)
    {
        x->x_gui.x_fsf.x_change = 0;
        sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
    }
    binbuf_addv(b, "ssiisiiffiisssiiiisssfi", gensym("#X"), gensym("obj"),
                (int)x->x_gui.x_obj.te_xpix, (int)x->x_gui.x_obj.te_ypix,
                gensym("nbx"), x->x_numwidth, x->x_gui.x_h/IEMGUI_ZOOM(x),
                (t_float)x->x_min, (t_float)x->x_max,
                x->x_lin0_log1, iem_symargstoint(&x->x_gui.x_isa),
                srl[0], srl[1], srl[2],
                x->x_gui.x_ldx, x->x_gui.x_ldy,
                iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize,
                bflcol[0], bflcol[1], bflcol[2],
                x->x_gui.x_isa.x_loadinit?x->x_val:0., x->x_log_height);
    binbuf_addv(b, ";");
}

int my_numbox_check_minmax(t_my_numbox *x, double min, double max)
{
    int ret = 0;

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
    if(x->x_val < x->x_min)
    {
        x->x_val = x->x_min;
        ret = 1;
    }
    if(x->x_val > x->x_max)
    {
        x->x_val = x->x_max;
        ret = 1;
    }
    if(x->x_lin0_log1)
        x->x_k = exp(log(x->x_max/x->x_min) / (double)(x->x_log_height));
    else
        x->x_k = 1.0;
    return(ret);
}

static void my_numbox_properties(t_gobj *z, t_glist *owner)
{
    t_my_numbox *x = (t_my_numbox *)z;

    if(x->x_gui.x_fsf.x_change)
    {
        x->x_gui.x_fsf.x_change = 0;
        sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
    }
    iemgui_new_dialog(x, &x->x_gui, "nbx",
                      x->x_numwidth, MINDIGITS,
                      x->x_gui.x_h/IEMGUI_ZOOM(x), IEM_GUI_MINSIZE,
                      x->x_min, x->x_max,
                      0,
                      x->x_lin0_log1, "linear", "logarithmic",
                      1, -1, x->x_log_height);
}

static void my_numbox_bang(t_my_numbox *x)
{
    outlet_float(x->x_gui.x_obj.ob_outlet, x->x_val);
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
        pd_float(x->x_gui.x_snd->s_thing, x->x_val);
}

static void my_numbox_dialog(t_my_numbox *x, t_symbol *s, int argc,
    t_atom *argv)
{
    t_symbol *srl[3];
    int w = (int)atom_getfloatarg(0, argc, argv);
    int h = (int)atom_getfloatarg(1, argc, argv);
    double min = (double)atom_getfloatarg(2, argc, argv);
    double max = (double)atom_getfloatarg(3, argc, argv);
    int lilo = (int)atom_getfloatarg(4, argc, argv);
    int log_height = (int)atom_getfloatarg(6, argc, argv);
    int sr_flags;
    t_atom undo[18];
    iemgui_setdialogatoms(&x->x_gui, 18, undo);
    SETFLOAT(undo+0, x->x_numwidth);
    SETFLOAT(undo+2, x->x_min);
    SETFLOAT(undo+3, x->x_max);
    SETFLOAT(undo+4, x->x_lin0_log1);
    SETFLOAT(undo+6, x->x_log_height);

    pd_undo_set_objectstate(x->x_gui.x_glist, (t_pd*)x, gensym("dialog"),
                            18, undo,
                            argc, argv);

    if(lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;
    sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);
    if(w < MINDIGITS)
        w = MINDIGITS;
    x->x_numwidth = w;
    if(h < IEM_GUI_MINSIZE)
        h = IEM_GUI_MINSIZE;
    x->x_gui.x_h = h * IEMGUI_ZOOM(x);
    if(log_height < 10)
        log_height = 10;
    x->x_log_height = log_height;
    my_numbox_calc_fontwidth(x);
    /*if(my_numbox_check_minmax(x, min, max))
     my_numbox_bang(x);*/
    my_numbox_check_minmax(x, min, max);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    iemgui_size(x, &x->x_gui);
}

static void my_numbox_motion(t_my_numbox *x, t_floatarg dx, t_floatarg dy,
    t_floatarg up)
{
    double k2 = 1.0;
    if (up != 0)
        return;

    if(x->x_gui.x_fsf.x_finemoved)
        k2 = 0.01;
    if(x->x_lin0_log1)
        x->x_val *= pow(x->x_k, -k2*dy);
    else
        x->x_val -= k2*dy;
    my_numbox_clip(x);
    sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
    my_numbox_bang(x);
}

static void my_numbox_click(t_my_numbox *x, t_floatarg xpos, t_floatarg ypos,
                            t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
        (t_glistmotionfn)my_numbox_motion, my_numbox_key, xpos, ypos);
}

static int my_numbox_newclick(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_my_numbox* x = (t_my_numbox *)z;

    if(doit)
    {
        my_numbox_click( x, (t_floatarg)xpix, (t_floatarg)ypix,
            (t_floatarg)shift, 0, (t_floatarg)alt);
        if(shift)
            x->x_gui.x_fsf.x_finemoved = 1;
        else
            x->x_gui.x_fsf.x_finemoved = 0;
        if(!x->x_gui.x_fsf.x_change)
        {
            clock_delay(x->x_clock_wait, 50);
            x->x_gui.x_fsf.x_change = 1;
            x->x_buf[0] = 0;
        }
        else
        {
            x->x_gui.x_fsf.x_change = 0;
            x->x_buf[0] = 0;
            sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
        }
    }
    return (1);
}

static void my_numbox_set(t_my_numbox *x, t_floatarg f)
{
    t_float ftocompare = f;
        /* bitwise comparison, suggested by Dan Borstein - to make this work
        ftocompare must be t_float type like x_val. */
    if (memcmp(&ftocompare, &x->x_val, sizeof(ftocompare)))
    {
        x->x_val = ftocompare;
        if (pd_compatibilitylevel < 53)
            my_numbox_clip(x);
        sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
    }
}

static void my_numbox_log_height(t_my_numbox *x, t_floatarg lh)
{
    if(lh < 10.0)
        lh = 10.0;
    x->x_log_height = (int)lh;
    if(x->x_lin0_log1)
        x->x_k = exp(log(x->x_max/x->x_min)/(double)(x->x_log_height));
    else
        x->x_k = 1.0;
}

static void my_numbox_float(t_my_numbox *x, t_floatarg f)
{
    my_numbox_set(x, f);
    if(x->x_gui.x_fsf.x_put_in2out)
        my_numbox_bang(x);
}

static void my_numbox_size(t_my_numbox *x, t_symbol *s, int ac, t_atom *av)
{
    int h, w;

    w = (int)atom_getfloatarg(0, ac, av);
    if(w < MINDIGITS)
        w = MINDIGITS;
    x->x_numwidth = w;
    if(ac > 1)
    {
        h = (int)atom_getfloatarg(1, ac, av);
        if(h < IEM_GUI_MINSIZE)
            h = IEM_GUI_MINSIZE;
        x->x_gui.x_h = h * IEMGUI_ZOOM(x);
    }
    my_numbox_calc_fontwidth(x);
    iemgui_size((void *)x, &x->x_gui);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void my_numbox_delta(t_my_numbox *x, t_symbol *s, int ac, t_atom *av)
{iemgui_delta((void *)x, &x->x_gui, s, ac, av);}

static void my_numbox_pos(t_my_numbox *x, t_symbol *s, int ac, t_atom *av)
{iemgui_pos((void *)x, &x->x_gui, s, ac, av);}

static void my_numbox_range(t_my_numbox *x, t_symbol *s, int ac, t_atom *av)
{
    if(my_numbox_check_minmax(x, (double)atom_getfloatarg(0, ac, av),
                                 (double)atom_getfloatarg(1, ac, av)))
    {
        sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
        /*my_numbox_bang(x);*/
    }
}

static void my_numbox_color(t_my_numbox *x, t_symbol *s, int ac, t_atom *av)
{iemgui_color((void *)x, &x->x_gui, s, ac, av);}

static void my_numbox_send(t_my_numbox *x, t_symbol *s)
{iemgui_send(x, &x->x_gui, s);}

static void my_numbox_receive(t_my_numbox *x, t_symbol *s)
{iemgui_receive(x, &x->x_gui, s);}

static void my_numbox_label(t_my_numbox *x, t_symbol *s)
{iemgui_label((void *)x, &x->x_gui, s);}

static void my_numbox_label_pos(t_my_numbox *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_pos((void *)x, &x->x_gui, s, ac, av);}

static void my_numbox_label_font(t_my_numbox *x,
    t_symbol *s, int ac, t_atom *av)
{
    int f = (int)atom_getfloatarg(1, ac, av);

    if(f < 4)
        f = 4;
    x->x_gui.x_fontsize = f;
    f = (int)atom_getfloatarg(0, ac, av);
    if((f < 0) || (f > 2))
        f = 0;
    x->x_gui.x_fsf.x_font_style = f;
    my_numbox_calc_fontwidth(x);
    iemgui_label_font((void *)x, &x->x_gui, s, ac, av);
}

static void my_numbox_log(t_my_numbox *x)
{
    x->x_lin0_log1 = 1;
    if(my_numbox_check_minmax(x, x->x_min, x->x_max))
    {
        sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
        /*my_numbox_bang(x);*/
    }
}

static void my_numbox_lin(t_my_numbox *x)
{
    x->x_lin0_log1 = 0;
}

static void my_numbox_init(t_my_numbox *x, t_floatarg f)
{
    x->x_gui.x_isa.x_loadinit = (f == 0.0) ? 0 : 1;
}

static void my_numbox_loadbang(t_my_numbox *x, t_floatarg action)
{
    if(action == LB_LOAD && x->x_gui.x_isa.x_loadinit)
    {
        sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
        my_numbox_bang(x);
    }
}

static void my_numbox_key(void *z, t_symbol *keysym, t_floatarg fkey)
{
    t_my_numbox *x = z;
    char c = fkey;
    char buf[3];
    buf[1] = 0;

    if(c == 0)
    {
        x->x_gui.x_fsf.x_change = 0;
        sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
        return;
    }
    if(((c >= '0') && (c <= '9')) || (c == '.') || (c == '-') ||
        (c == 'e') || (c == '+') || (c == 'E'))
    {
        if(strlen(x->x_buf) < (IEMGUI_MAX_NUM_LEN-2))
        {
            buf[0] = c;
            strcat(x->x_buf, buf);
            sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
        }
    }
    else if((c == '\b') || (c == 127))
    {
        int sl = (int)strlen(x->x_buf) - 1;

        if(sl < 0)
            sl = 0;
        x->x_buf[sl] = 0;
        sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
    }
    else if((c == '\n') || (c == 13))
    {
        if(x->x_buf[0])
        {
            x->x_val = atof(x->x_buf);
            x->x_buf[0] = 0;
            if (pd_compatibilitylevel < 53)
                my_numbox_clip(x);
            sys_queuegui(x, x->x_gui.x_glist, my_numbox_draw_update);
        }
        my_numbox_bang(x);
    }
}

static void my_numbox_list(t_my_numbox *x, t_symbol *s, int ac, t_atom *av)
{
    if(!ac) {
        my_numbox_bang(x);
    }
    else if(IS_A_FLOAT(av, 0))
    {
        my_numbox_set(x, atom_getfloatarg(0, ac, av));
        my_numbox_bang(x);
    }
}

static void *my_numbox_new(t_symbol *s, int argc, t_atom *argv)
{
    t_my_numbox *x = (t_my_numbox *)iemgui_new(my_numbox_class);
    int w = 5, h = 14 * IEM_GUI_DEFAULTSIZE_SCALE;
    int lilo = 0, ldx = 0, ldy = -8 * IEM_GUI_DEFAULTSIZE_SCALE;
    int fs = x->x_gui.x_fontsize;
    int log_height = 256;
    double min = -1.0e+37, max = 1.0e+37, v = 0.0;

    IEMGUI_SETDRAWFUNCTIONS(x, my_numbox);

    if((argc >= 17)&&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)
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
    {
        log_height = (int)atom_getfloatarg(17, argc, argv);
    }
    x->x_gui.x_fsf.x_snd_able = (0 != x->x_gui.x_snd);
    x->x_gui.x_fsf.x_rcv_able = (0 != x->x_gui.x_rcv);
    if(x->x_gui.x_isa.x_loadinit)
        x->x_val = v;
    else
        x->x_val = 0.0;
    if(lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;
    if(log_height < 10)
        log_height = 10;
    x->x_log_height = log_height;
    if(x->x_gui.x_fsf.x_font_style == 1) strcpy(x->x_gui.x_font, "helvetica");
    else if(x->x_gui.x_fsf.x_font_style == 2) strcpy(x->x_gui.x_font, "times");
    else { x->x_gui.x_fsf.x_font_style = 0;
        strcpy(x->x_gui.x_font, sys_font); }
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;
    x->x_gui.x_fontsize = (fs < MINFONT)?MINFONT:fs;
    if(w < MINDIGITS)
        w = MINDIGITS;
    x->x_numwidth = w;
    if(h < IEM_GUI_MINSIZE)
        h = IEM_GUI_MINSIZE;
    x->x_gui.x_h = h;
    x->x_buf[0] = 0;
    my_numbox_check_minmax(x, min, max);
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    x->x_clock_wait = clock_new(x, (t_method)my_numbox_tick_wait);
    x->x_gui.x_fsf.x_change = 0;
    iemgui_newzoom(&x->x_gui);
    my_numbox_calc_fontwidth(x);
    outlet_new(&x->x_gui.x_obj, &s_float);
    return (x);
}

static void my_numbox_free(t_my_numbox *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    clock_free(x->x_clock_wait);
    pdgui_stub_deleteforkey(x);
}

void g_numbox_setup(void)
{
    my_numbox_class = class_new(gensym("nbx"), (t_newmethod)my_numbox_new,
        (t_method)my_numbox_free, sizeof(t_my_numbox), 0, A_GIMME, 0);
    class_addcreator((t_newmethod)my_numbox_new, gensym("my_numbox"), A_GIMME, 0);
    class_addbang(my_numbox_class, my_numbox_bang);
    class_addfloat(my_numbox_class, my_numbox_float);
    class_addlist(my_numbox_class, my_numbox_list);
    class_addmethod(my_numbox_class, (t_method)my_numbox_click,
        gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_motion,
        gensym("motion"), A_FLOAT, A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_dialog,
        gensym("dialog"), A_GIMME, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_loadbang,
        gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_set,
        gensym("set"), A_FLOAT, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_size,
        gensym("size"), A_GIMME, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_delta,
        gensym("delta"), A_GIMME, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_pos,
        gensym("pos"), A_GIMME, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_range,
        gensym("range"), A_GIMME, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_color,
        gensym("color"), A_GIMME, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_send,
        gensym("send"), A_DEFSYM, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_receive,
        gensym("receive"), A_DEFSYM, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_label,
        gensym("label"), A_DEFSYM, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_label_pos,
        gensym("label_pos"), A_GIMME, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_label_font,
        gensym("label_font"), A_GIMME, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_log,
        gensym("log"), 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_lin,
        gensym("lin"), 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_init,
        gensym("init"), A_FLOAT, 0);
    class_addmethod(my_numbox_class, (t_method)my_numbox_log_height,
        gensym("log_height"), A_FLOAT, 0);
    class_addmethod(my_numbox_class, (t_method)iemgui_zoom,
        gensym("zoom"), A_CANT, 0);
    my_numbox_widgetbehavior.w_getrectfn =    my_numbox_getrect;
    my_numbox_widgetbehavior.w_displacefn =   iemgui_displace;
    my_numbox_widgetbehavior.w_selectfn =     iemgui_select;
    my_numbox_widgetbehavior.w_activatefn =   NULL;
    my_numbox_widgetbehavior.w_deletefn =     iemgui_delete;
    my_numbox_widgetbehavior.w_visfn =        iemgui_vis;
    my_numbox_widgetbehavior.w_clickfn =      my_numbox_newclick;
    class_setwidget(my_numbox_class, &my_numbox_widgetbehavior);
    class_setsavefn(my_numbox_class, my_numbox_save);
    class_setpropertiesfn(my_numbox_class, my_numbox_properties);
}
