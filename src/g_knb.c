/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* knb.c written by Antoine Rousseau from g_vslider.c.*/
/* g_7_guis.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "m_pd.h"
#include "g_canvas.h"

#include "g_all_guis.h"
#include <math.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define MKNOB_TANGLE 100
#define MKNOB_DEFAULTH 100
#define MKNOB_DEFAULTSIZE 25
#define MKNOB_MINSIZE 12

/* ------------ knb  ----------------------- */
typedef struct _knb
{
    t_iemgui x_gui;
    int      x_pos;
    int      x_val;
    t_symbol *x_wiper_style;
    t_symbol *x_current_wiper_style;
    int      x_arc_width;
    int      x_start_angle;
    int      x_end_angle;
    int      x_style;
    int      x_currentstyle;
    int      x_ticks;
    double   x_min;
    double   x_max;
    int   	 x_H;
    double   x_k;
    t_float  x_fval;
    unsigned int      x_lin0_log1:1;
    unsigned int      x_angular:1;
    unsigned int      x_full:1;
    unsigned int      x_force_outline_visible:1;
    unsigned int      x_outline_visible:1;
    unsigned int      x_inlet_visible:1;
    unsigned int      x_outlet_visible:1;
    unsigned int      x_wiper_visible:1;
    unsigned int      x_arc_visible:1;
} t_knb;

t_widgetbehavior knb_widgetbehavior;
static t_class *knb_class;

/* widget helper functions */

static void knb_draw_io(t_knb *x,t_glist *glist);

static void knb_update_knob(t_knb *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    float val = (x->x_val / 100.0) / (x->x_H - 1.0) - 0.001;
    float angle, angle0;
    float radius = x->x_gui.x_w / 2.0;
    float miniradius = radius / 6.0;
    int x0, y0, x1, y1, xc, yc, xp, yp, xpc, ypc;
    int xA0, yA0, xA1, yA1;
    
    if(miniradius < 3.0) miniradius = 3.0;

    x0 = text_xpix(&x->x_gui.x_obj, glist);
    y0 = text_ypix(&x->x_gui.x_obj, glist);
    x1 = x0 + x->x_gui.x_w;
    y1 = y0 + x->x_gui.x_w;
    xc = (x0 + x1) / 2;
    yc = (y0 + y1) / 2;

    if(x->x_full) {
        angle = val * M_PI * 2.0 + M_PI / 2.0;
        angle0 = M_PI / 2.0;
    }
    else {
        angle = val * M_PI * 1.5 + 3.0 * M_PI / 4.0;
        angle0 = 3.0 * M_PI / 4.0;
    }
    
    xp = xc + radius * cos(angle);
    yp = yc + radius * sin(angle);
    xpc = miniradius * cos(angle-M_PI/2);
    ypc = miniradius * sin(angle-M_PI/2);

    if(x->x_force_outline_visible && !x->x_gui.x_fsf.x_selected) {
        x->x_force_outline_visible = 0;
        knb_draw_io(x, glist);
    }
    /*switch(x->x_style) {
    case 1:
        sys_vgui(".x%lx.c coords %lxWIPER %d %d %d %d %d %d\n",
            canvas, x, xp, yp, xc + xpc, yc + ypc, xc - xpc, yc - ypc);
        break;
    case 2:
        sys_vgui(".x%lx.c coords %lxARC %d %d %d %d\n",
            canvas, x, x0 + 1, y0 + 1, x1 - 1, y1 - 1);
        sys_vgui(".x%lx.c itemconfigure %lxARC -start %f -extent %f\n",
            canvas, x, angle0 * -180.0 / M_PI, (angle - angle0) * -180.0 / M_PI);
        break;
     case 3:*/
    if(x->x_wiper_visible)
        sys_vgui(".x%lx.c coords %lxWIPER %d %d %d %d %d %d\n",
            canvas, x, xp, yp, xc + xpc, yc + ypc, xc - xpc, yc - ypc);

    if(x->x_arc_visible) {
        if(x->x_arc_width > 0) {
            xA0 = x0 + (x->x_arc_width / 2) + 1;
            yA0 = y0 + (x->x_arc_width / 2) + 1;
            xA1 = x1 - (x->x_arc_width / 2) - 1;
            yA1 = y1 - (x->x_arc_width / 2) - 1;
        } else {
            xA0 = xc + (x->x_arc_width / 2) + 1;
            yA0 = yc + (x->x_arc_width / 2) + 1;
            xA1 = xc - (x->x_arc_width / 2) - 1;
            yA1 = yc - (x->x_arc_width / 2) - 1;
        }
        sys_vgui(".x%lx.c coords %lxARC %d %d %d %d\n",
            canvas, x, xA0, yA0, xA1, yA1);
        sys_vgui(".x%lx.c itemconfigure %lxARC -start %f -extent %f -width %d\n",
            canvas, x, angle0 * -180.0 / M_PI, (angle - angle0) * -180.0 / M_PI,
            abs(x->x_arc_width));
    }
        /*break;
    }*/
}

static void knb_draw_update(t_knb *x, t_glist *glist)
{
    if (glist_isvisible(glist))
    {
        knb_update_knob(x, glist);
    }
}

static void knb_draw_new(t_knb *x, t_glist *glist)
{
    int xpos=text_xpix(&x->x_gui.x_obj, glist);
    int ypos=text_ypix(&x->x_gui.x_obj, glist);
    int xc = xpos + x->x_gui.x_w / 2;
    int yc = ypos + x->x_gui.x_w / 2;
    t_canvas *canvas = glist_getcanvas(glist);

    x->x_currentstyle = x->x_style;
    sys_vgui(".x%lx.c create oval %d %d %d %d -fill #%06x -width %d -tags %lxBASE\n",
        canvas, xpos, ypos, xpos + x->x_gui.x_w, ypos + x->x_gui.x_w,
        x->x_gui.x_bcol, IEMGUI_ZOOM(x),
        x);
    knb_draw_io(x, glist);

    x->x_wiper_visible = (x->x_wiper_style != gensym("none"));
    sys_vgui(".x%lx.c create polygon %d %d %d %d %d %d -fill #%06x -state %s -tags %lxWIPER\n",
        canvas, xc, ypos, xc - 4, yc, xc + 4, yc, x->x_gui.x_fcol, 
        x->x_wiper_visible ? "normal" : "hidden", x);
    x->x_arc_visible = (x->x_arc_width != 0);
    sys_vgui(".x%lx.c create arc %d %d %d %d -style arc -outline #%06x \
        -width %d -state %s -tags %lxARC\n",
        canvas, xpos + (x->x_arc_width / 2) + 1, ypos + (x->x_arc_width / 2) + 1, 
        xpos + x->x_gui.x_w - (x->x_arc_width / 2) - 1, ypos + x->x_gui.x_w - (x->x_arc_width / 2) - 1,
        x->x_gui.x_fcol, abs(x->x_arc_width), x->x_arc_visible ? "normal" : "hidden", x);

    knb_update_knob(x,glist);
    sys_vgui(".x%lx.c create text %d %d -text {%s} -anchor w \
        -font {{%s} -%d %s} -fill #%06x -tags [list %lxLABEL label text]\n",
        canvas, xpos+x->x_gui.x_ldx * IEMGUI_ZOOM(x),
        ypos+x->x_gui.x_ldy * IEMGUI_ZOOM(x),
        strcmp(x->x_gui.x_lab->s_name, "empty")?x->x_gui.x_lab->s_name:"",
        x->x_gui.x_font, x->x_gui.x_fontsize * IEMGUI_ZOOM(x), sys_fontweight, 
        x->x_gui.x_lcol, x);
}

static void knb_draw_move(t_knb *x, t_glist *glist)
{
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int r = xpos + (x->x_val + 50) / 100;
    int iow = IOWIDTH * IEMGUI_ZOOM(x), ioh = IEM_GUI_IOHEIGHT * IEMGUI_ZOOM(x);
    t_canvas *canvas = glist_getcanvas(glist);

    sys_vgui(".x%lx.c coords %lxBASE %d %d %d %d\n",
        canvas, x,
        xpos, ypos,
        xpos + x->x_gui.x_w, ypos + x->x_gui.x_w);
    if(x->x_outline_visible) sys_vgui(".x%lx.c coords %lxOUTLINE %d %d %d %d\n",
        canvas, x,
        xpos, ypos,
        xpos + x->x_gui.x_w, ypos + x->x_gui.x_w);
    if(x->x_outlet_visible) sys_vgui(".x%lx.c coords %lxOUT%d %d %d %d %d\n",
        canvas, x, 0,
        xpos, ypos + x->x_gui.x_w + IEMGUI_ZOOM(x) - ioh,
        xpos + iow, ypos + x->x_gui.x_w);
    if(x->x_inlet_visible) sys_vgui(".x%lx.c coords %lxIN%d %d %d %d %d\n",
        canvas, x, 0,
        xpos, ypos,
        xpos + iow, ypos - IEMGUI_ZOOM(x) + ioh);
    knb_update_knob(x,glist);
    sys_vgui(".x%lx.c coords %lxLABEL %d %d\n",
        canvas, x, xpos+x->x_gui.x_ldx * IEMGUI_ZOOM(x),
        ypos+x->x_gui.x_ldy * IEMGUI_ZOOM(x));
}

static void knb_draw_erase(t_knb *x,t_glist *glist)
{
    t_canvas *canvas=glist_getcanvas(glist);

    sys_vgui(".x%lx.c delete %lxBASE\n", canvas, x);
    //if(x->x_wiper_visible) {
        sys_vgui(".x%lx.c delete %lxWIPER\n", canvas, x);
        //x->x_wiper_visible = 0;
    //}
    //if(x->x_arc_visible) {
        sys_vgui(".x%lx.c delete %lxARC\n", canvas, x);
        //x->x_arc_visible = 0;
    //}
    sys_vgui(".x%lx.c delete %lxLABEL\n", canvas, x);
    if(x->x_outline_visible) {
        sys_vgui(".x%lx.c delete %lxOUTLINE\n", canvas, x);
        x->x_outline_visible = 0;
    }
    if(x->x_outlet_visible) {
        sys_vgui(".x%lx.c delete %lxOUT%d\n", canvas, x, 0);
        x->x_outlet_visible = 0;
    }
    if(x->x_inlet_visible) {
        sys_vgui(".x%lx.c delete %lxIN%d\n", canvas, x, 0);
        x->x_inlet_visible = 0;
    }
}

static void knb_draw_styleupdate(t_knb *x, t_glist *glist)
{
    if (!glist_isvisible(glist)) return;
    if(x->x_currentstyle == x->x_style) return;
    x->x_currentstyle = x->x_style;
    
    knb_draw_erase(x, glist);
    knb_draw_new(x, glist);
}

static void knb_draw_config(t_knb *x,t_glist *glist)
{
    t_canvas *canvas=glist_getcanvas(glist);

    sys_vgui(".x%lx.c itemconfigure %lxLABEL -font {{%s} -%d %s} -fill #%06x -text {%s} \n",
        canvas, x, x->x_gui.x_font, x->x_gui.x_fontsize * IEMGUI_ZOOM(x), sys_fontweight,
        x->x_gui.x_fsf.x_selected ? IEM_GUI_COLOR_SELECTED : x->x_gui.x_lcol,
        strcmp(x->x_gui.x_lab->s_name, "empty") ? x->x_gui.x_lab->s_name : "");

    x->x_wiper_visible = (x->x_wiper_style != gensym("none"));
    sys_vgui(".x%lx.c itemconfigure %lxWIPER -fill #%06x -width %d -state %s\n", canvas, 
        x, x->x_gui.x_fcol, IEMGUI_ZOOM(x), x->x_wiper_visible ? "normal" : "hidden");
    x->x_arc_visible = (x->x_arc_width != 0);
    sys_vgui(".x%lx.c itemconfigure %lxARC -outline #%06x -width %d -state %s\n", canvas, 
        x, x->x_gui.x_fcol, abs(x->x_arc_width), x->x_arc_visible ? "normal" : "hidden");

    sys_vgui(".x%lx.c itemconfigure %lxBASE -fill #%06x\n", canvas, x, x->x_gui.x_bcol);
}

static void knb_draw_io(t_knb *x,t_glist *glist)
{
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int iow = IOWIDTH * IEMGUI_ZOOM(x), ioh = IEM_GUI_IOHEIGHT * IEMGUI_ZOOM(x);
    t_canvas *canvas = glist_getcanvas(glist);

    if(x->x_force_outline_visible
    || (!x->x_gui.x_fsf.x_snd_able)
    || (!x->x_gui.x_fsf.x_rcv_able))
    {
        if(!x->x_outline_visible)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -width %d -tags %lxOUTLINE\n",
                canvas, xpos, ypos,
                xpos + x->x_gui.x_w, ypos + x->x_gui.x_w,
                IEMGUI_ZOOM(x),
                x);
        x->x_outline_visible = 1;
    } else {
        if(x->x_outline_visible) sys_vgui(".x%lx.c delete %lxOUTLINE\n", canvas, x);
        x->x_outline_visible = 0;
    }

    if(!x->x_gui.x_fsf.x_snd_able)
    {
        if(!x->x_outlet_visible)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lxOUT%d\n",
                canvas,
                xpos, ypos + x->x_gui.x_w + IEMGUI_ZOOM(x) - ioh,
                xpos + iow, ypos + x->x_gui.x_w,
                x, 0);
        x->x_outlet_visible = 1;
    } else {
        if(x->x_outlet_visible) sys_vgui(".x%lx.c delete %lxOUT%d\n", canvas, x, 0);
        x->x_outlet_visible = 0;
    }

    if(!x->x_gui.x_fsf.x_rcv_able)
    {
        if(!x->x_inlet_visible)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lxIN%d\n",
                canvas,
                xpos, ypos,
                xpos + iow, ypos - IEMGUI_ZOOM(x) + ioh,
                x, 0);
        x->x_inlet_visible = 1;
    }
    else {
        if(x->x_inlet_visible) sys_vgui(".x%lx.c delete %lxIN%d\n", canvas, x, 0);
        x->x_inlet_visible = 0;
    }
}

static void knb_draw_select(t_knb *x,t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);

    if(x->x_gui.x_fsf.x_selected)
    {
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline #%06x\n", canvas, x,
            IEM_GUI_COLOR_SELECTED);
        sys_vgui(".x%lx.c itemconfigure %lxLABEL -fill #%06x\n", canvas, x,
            IEM_GUI_COLOR_SELECTED);
    }
    else
    {
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline #%06x\n", canvas, x,
            IEM_GUI_COLOR_NORMAL);
        sys_vgui(".x%lx.c itemconfigure %lxLABEL -fill #%06x\n", canvas, x,
            x->x_gui.x_lcol);
    }
    if(x->x_gui.x_fsf.x_selected) {
        x->x_force_outline_visible = 1;
        knb_draw_io(x, glist);
    }
    if(x->x_outline_visible){
        if(x->x_gui.x_fsf.x_selected)
            sys_vgui(".x%lx.c itemconfigure %lxOUTLINE -outline #%06x\n", canvas, x,
                IEM_GUI_COLOR_SELECTED);
        else
            sys_vgui(".x%lx.c itemconfigure %lxOUTLINE -outline #%06x\n", canvas, x,
                IEM_GUI_COLOR_NORMAL);
    }
}

void knb_draw(t_knb *x, t_glist *glist, int mode)
{
    if(mode == IEM_GUI_DRAW_MODE_UPDATE)
        knb_draw_update(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_MOVE)
        knb_draw_move(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_NEW)
        knb_draw_new(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_SELECT)
        knb_draw_select(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_ERASE)
        knb_draw_erase(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_CONFIG)
        knb_draw_config(x, glist);
    else if(mode >= IEM_GUI_DRAW_MODE_IO)
        knb_draw_io(x, glist);//, mode - IEM_GUI_DRAW_MODE_IO);
}

/* ------------------------ knb widgetbehaviour----------------------------- */

#define GRECTRATIO 0
static void knb_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_knb *x = (t_knb *)z;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist) + GRECTRATIO * x->x_gui.x_w;
    *yp1 = text_ypix(&x->x_gui.x_obj, glist) + GRECTRATIO * x->x_gui.x_w;
    *xp2 = text_xpix(&x->x_gui.x_obj, glist) + x->x_gui.x_w * (1 - GRECTRATIO);
    *yp2 = text_ypix(&x->x_gui.x_obj, glist) + x->x_gui.x_w * (1 - GRECTRATIO);
}

static void knb_save(t_gobj *z, t_binbuf *b)
{
    t_knb *x = (t_knb *)z;
    int bflcol_compat[3];
    t_symbol *bflcol[3];
    t_symbol *srl[3];

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiffiisssiiiisssiiiiisiii", gensym("#X"),gensym("obj"),
        (t_int)x->x_gui.x_obj.te_xpix, (t_int)x->x_gui.x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_gui.x_obj.te_binbuf)),
        x->x_gui.x_w / IEMGUI_ZOOM(x), x->x_gui.x_h / IEMGUI_ZOOM(x),
        (float)x->x_min, (float)x->x_max,
        x->x_lin0_log1, iem_symargstoint(&x->x_gui.x_isa),
        srl[0], srl[1], srl[2],
        x->x_gui.x_ldx, x->x_gui.x_ldy,
        iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize,
        bflcol[0], bflcol[1], bflcol[2],
        x->x_val, x->x_full, x->x_style, x->x_angular, x->x_ticks,
        x->x_wiper_style, x->x_arc_width, x->x_start_angle, x->x_end_angle);
    binbuf_addv(b, ";");
}

void knb_update_H(t_knb *x)
{
    int H;

    H = x->x_gui.x_h;
    if(x->x_angular) H = x->x_full ? 360 : 270;
    x->x_H = H;

    if(x->x_lin0_log1)
        x->x_k = log(x->x_max/x->x_min)/(double)(x->x_H / IEMGUI_ZOOM(x) - 1);
    else
        x->x_k = (x->x_max - x->x_min)/(double)(x->x_H / IEMGUI_ZOOM(x) - 1);
}

void knb_check_wh(t_knb *x, int w, int h)
{
    int H;
    if(w < MKNOB_MINSIZE * IEMGUI_ZOOM(x))
        w = MKNOB_MINSIZE * IEMGUI_ZOOM(x);
    x->x_gui.x_w = w;

    /*if(h < 0) h = 0;
    if((h > 0) && (h < 5)) h = 5;*/
    if(h < 5) h = 5;
    x->x_gui.x_h = h;

    knb_update_H(x);
}

void knb_check_minmax(t_knb *x, double min, double max)
{
    int H;

    if(x->x_lin0_log1)
    {
        if((min == 0.0)&&(max == 0.0))
            max = 1.0;
        if(max > 0.0)
        {
            if(min <= 0.0) min = 0.01 * max;
        }
        else
        {
            if(min > 0.0) max = 0.01 * min;
        }
    }
    x->x_min = min;
    x->x_max = max;

    if(x->x_lin0_log1)
        x->x_k = log(x->x_max/x->x_min)/(double)(x->x_H / IEMGUI_ZOOM(x) - 1);
    else
        x->x_k = (x->x_max - x->x_min)/(double)(x->x_H / IEMGUI_ZOOM(x) - 1);
}

static void knb_properties(t_gobj *z, t_glist *owner)
{
    t_knb *x = (t_knb *)z;
    char buf[800];
    t_symbol *srl[3];

    iemgui_properties(&x->x_gui, srl);
    sprintf(buf, "pdtk_iemgui_dialog %%s |knb| \
            --------dimensions(pix)(pix):-------- %d %d size: %d %d sensitivity: \
            -----------output-range:----------- %g left: %g right: %g \
            %d lin log %d %d Ticks %d \
            %s %s \
            %s %d %d \
            %d %d \
            #%06x #%06x #%06x\
            %d %s %d %d %d\n",
            x->x_gui.x_w / IEMGUI_ZOOM(x), MKNOB_MINSIZE, x->x_gui.x_h / IEMGUI_ZOOM(x), 0,
            x->x_min, x->x_max, 0.0,/*no_schedule*/
            x->x_lin0_log1, x->x_gui.x_isa.x_loadinit, x->x_angular, -1,/*no multi, but iem-characteristic*/
            srl[0]->s_name, srl[1]->s_name,
            srl[2]->s_name, x->x_gui.x_ldx, x->x_gui.x_ldy,
            x->x_gui.x_fsf.x_font_style, x->x_gui.x_fontsize,
            0xffffff & x->x_gui.x_bcol, 0xffffff & x->x_gui.x_fcol, 0xffffff & x->x_gui.x_lcol,
            x->x_ticks, x->x_wiper_style->s_name, x->x_arc_width, x->x_start_angle, x->x_end_angle);
    gfxstub_new(&x->x_gui.x_obj.ob_pd, x, buf);
}

    /* compute numeric value (fval) from pixel location (val) and range */
static t_float knb_getfval(t_knb *x)
{
    t_float fval;
    int zoom = x->x_angular ? 1 : IEMGUI_ZOOM(x);
    int zoomval = (x->x_gui.x_fsf.x_finemoved) ?
        x->x_val / zoom : (x->x_val / (100 * zoom)) * 100;

    if (x->x_lin0_log1)
        fval = x->x_min * exp(x->x_k * (double)(zoomval) * 0.01);
    else fval = (double)(zoomval) * 0.01 * x->x_k + x->x_min;
    if ((fval < 1.0e-10) && (fval > -1.0e-10))
        fval = 0.0;
    return (fval);
}

static void knb_set(t_knb *x, t_floatarg f)
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
        g = log(f/x->x_min)/x->x_k;
    else
        g = (f - x->x_min) / x->x_k;
    x->x_val = (x->x_angular ? 1 : IEMGUI_ZOOM(x)) * (int)(100.0*g + 0.49999);
    x->x_pos = x->x_val;
    if(x->x_val != old)
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void knb_bang(t_knb *x)
{
    double out;

    if (pd_compatibilitylevel < 46)
        out = knb_getfval(x);
    else out = x->x_fval;
    outlet_float(x->x_gui.x_obj.ob_outlet, out);
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
        pd_float(x->x_gui.x_snd->s_thing, out);
}

static void knb_dialog(t_knb *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int w = (int)atom_getintarg(0, argc, argv) * IEMGUI_ZOOM(x);
    int h = (int)atom_getintarg(1, argc, argv);
    double min = (double)atom_getfloatarg(2, argc, argv);
    double max = (double)atom_getfloatarg(3, argc, argv);
    int lilo = (int)atom_getintarg(4, argc, argv);
    int angular = (int)atom_getintarg(17, argc, argv);
    int ticks = (int)atom_getintarg(18, argc, argv);
    t_symbol *wiperstyle = atom_getsymbolarg(19, argc, argv);
    int arcwidth = (int)atom_getintarg(20, argc, argv);
    int startangle = (int)atom_getintarg(21, argc, argv);
    int endangle = (int)atom_getintarg(22, argc, argv);

    int sr_flags;

    if(lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;
    if(angular) x->x_angular = 1;
    else x->x_angular = 0;
    /*if(style < 1) style = 1;
    x->x_style = style;*/
    if(ticks < 0) ticks = 0;
    x->x_ticks = ticks;
    x->x_wiper_style = wiperstyle;
    x->x_arc_width = arcwidth;
    x->x_start_angle = startangle;
    x->x_end_angle = endangle;
    
    sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);

    if(h > 0) h *= IEMGUI_ZOOM(x);
    knb_check_wh(x, w, h);
    knb_check_minmax(x, min, max);
    knb_draw_styleupdate(x, x->x_gui.x_glist);
    knb_set(x, x->x_fval);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_CONFIG);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO + sr_flags);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_MOVE);
    canvas_fixlinesfor(x->x_gui.x_glist, (t_text *)x);
}

static int xm0, ym0, xm, ym;

static void knb_motion(t_knb *x, t_floatarg dx, t_floatarg dy)
{
    int old = x->x_val;
    float d=-dy;

    if (abs(dx)>abs(dy)) d=dx;

    if(x->x_gui.x_fsf.x_finemoved)
        x->x_pos += (int)d;
    else
        x->x_pos += 100*(int)d;
    x->x_val = x->x_pos;
    if(x->x_val > (100*x->x_H - 100))
    {
        x->x_val = 100*x->x_H - 100;
        x->x_pos += 50;
        x->x_pos -= x->x_pos%100;
    }
    if(x->x_val < 0)
    {
        x->x_val = 0;
        x->x_pos -= 50;
        x->x_pos -= x->x_pos%100;
    }
    x->x_fval = knb_getfval(x);
    if(old != x->x_val)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        knb_bang(x);
    }
}

static void knb_motion_circular(t_knb *x, t_floatarg dx, t_floatarg dy)
{
    int xc = text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist) + x->x_gui.x_w / 2;
    int yc = text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist) + x->x_gui.x_w / 2;
    int old = x->x_val;
    float alpha;

    xm += dx;
    ym += dy;

    alpha = atan2(xm - xc, ym - yc) * 180.0 / M_PI;

    x->x_pos = (int)(31500 - alpha * 100.0) % 36000;
    if(x->x_pos > 31500) x->x_pos = 0;
    else if(x->x_pos > (27000 - 100)) x->x_pos = 27000 - 100;

    x->x_val = x->x_pos;
    x->x_fval = knb_getfval(x);

    if(old != x->x_val)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        knb_bang(x);
    }
}

static void knb_motion_fullcircular(t_knb *x, t_floatarg dx, t_floatarg dy)
{
    int xc = text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist) + x->x_gui.x_w / 2;
    int yc = text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist) + x->x_gui.x_w / 2;
    int old = x->x_val;
    float alpha;

    xm += dx;
    ym += dy;

    alpha = atan2(xm - xc, ym - yc) * 180.0 / M_PI;

    x->x_pos = (int)(36000 - alpha * 100.0) % 36000;

    if(x->x_pos > (36000 - 100)) x->x_pos = 36000 - 100;
    x->x_val = x->x_pos;
    x->x_fval = knb_getfval(x);

    if(old != x->x_val)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        knb_bang(x);
    }
}

static void knb_click(t_knb *x, t_floatarg xpos, t_floatarg ypos,
                        t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    xm0 = xm = xpos;
    ym0 = ym = ypos;
    if(x->x_val > (100 * x->x_H - 100))
        x->x_val = 100 * x->x_H - 100;
    if(x->x_val < 0)
        x->x_val = 0;
    x->x_fval = knb_getfval(x);
    x->x_pos = x->x_val;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    knb_bang(x);

    if(x->x_angular) {
        if(x->x_full == 1)
            glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
                (t_glistmotionfn)knb_motion_fullcircular, 0, xpos, ypos);
        else
            glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
                (t_glistmotionfn)knb_motion_circular, 0, xpos, ypos);
    }
    else
        glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
            (t_glistmotionfn)knb_motion, 0, xpos, ypos);
}

static int knb_newclick(t_gobj *z, struct _glist *glist,
                          int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_knb *x = (t_knb *)z;

    if(x->x_force_outline_visible && !x->x_gui.x_fsf.x_selected) {
        x->x_force_outline_visible = 0;
        knb_draw_io(x, glist);
    }

    if(doit)
    {
        knb_click( x, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift,
                     0, (t_floatarg)alt);
        if(shift)
            x->x_gui.x_fsf.x_finemoved = 1;
        else
            x->x_gui.x_fsf.x_finemoved = 0;
    }
    return (1);
}

static void knb_size(t_knb *x, t_floatarg f)
{
    int w = (int)f * IEMGUI_ZOOM(x);
    int h = x->x_gui.x_h;

    knb_check_wh(x, w, h);
    iemgui_size((void *)x, &x->x_gui);
}

static void knb_sensitivity(t_knb *x, t_floatarg f)
{
    int w = x->x_gui.x_w;
    int h = (int)f * IEMGUI_ZOOM(x);
    float fval = knb_getfval(x);

    knb_check_wh(x, w, h);
    iemgui_size((void *)x, &x->x_gui);
    knb_set(x, fval);
}

static void knb_angular(t_knb *x, t_floatarg f)
{
    float fval = knb_getfval(x);
    /*int w = x->x_gui.x_w;
    int h = (int)f * IEMGUI_ZOOM(x);

    knb_check_wh(x, w, h);
    iemgui_size((void *)x, &x->x_gui);*/
    x->x_angular = (f != 0);
    knb_update_H(x);
    knb_set(x, fval);
}

static void knb_delta(t_knb *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_delta((void *)x, &x->x_gui, s, ac, av);
}

static void knb_pos(t_knb *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_pos((void *)x, &x->x_gui, s, ac, av);
}

static void knb_range(t_knb *x, t_symbol *s, int ac, t_atom *av)
{
    knb_check_minmax(x, (double)atom_getfloatarg(0, ac, av),
                       (double)atom_getfloatarg(1, ac, av));
}

static void knb_color(t_knb *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_color((void *)x, &x->x_gui, s, ac, av);
}

static void knb_send(t_knb *x, t_symbol *s)
{
    iemgui_send(x, &x->x_gui, s);
}

static void knb_receive(t_knb *x, t_symbol *s)
{
    iemgui_receive(x, &x->x_gui, s);
}

static void knb_label(t_knb *x, t_symbol *s)
{
    iemgui_label((void *)x, &x->x_gui, s);
}

static void knb_label_pos(t_knb *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_label_pos((void *)x, &x->x_gui, s, ac, av);
}

static void knb_label_font(t_knb *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_label_font((void *)x, &x->x_gui, s, ac, av);
}

static void knb_log(t_knb *x)
{
    x->x_lin0_log1 = 1;
    knb_check_minmax(x, x->x_min, x->x_max);
}

static void knb_lin(t_knb *x)
{
    x->x_lin0_log1 = 0;
    knb_check_minmax(x, x->x_min, x->x_max);
}

static void knb_init(t_knb *x, t_floatarg f)
{
    x->x_gui.x_isa.x_loadinit = (f == 0.0 ? 0 : 1);
}

static void knb_full(t_knb *x, t_floatarg f)
{
    x->x_full = (f == 0.0 ? 0 : 1);
    knb_update_H(x);
    knb_draw_update(x, x->x_gui.x_glist);
}

static void knb_style(t_knb *x, t_floatarg f)
{
    if(f < 1) f = 1;
    x->x_style = f;
    knb_draw_styleupdate(x, x->x_gui.x_glist);
}

static void knb_float(t_knb *x, t_floatarg f)
{
    double out;

    knb_set(x, f);
    if(x->x_gui.x_fsf.x_put_in2out)
        knb_bang(x);
}

static void knb_zoom(t_knb *x, t_floatarg f)
{
    if(!x->x_angular)
    {
    /* scale current pixel value */
        x->x_val = (IEMGUI_ZOOM(x) == 2 ? (x->x_val)/2 : (x->x_val)*2);
        x->x_H = (IEMGUI_ZOOM(x) == 2 ? (x->x_H)/2 : (x->x_H)*2);
        x->x_pos = x->x_val;
    }
    iemgui_zoom(&x->x_gui, f);
}

static void knb_loadbang(t_knb *x, t_floatarg action)
{
    if (action == LB_LOAD && x->x_gui.x_isa.x_loadinit)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        knb_bang(x);
    }
}

static void *knb_new(t_symbol *s, int argc, t_atom *argv)
{
    t_knb *x = (t_knb *)pd_new(knb_class);
    int w = MKNOB_DEFAULTSIZE, h = MKNOB_DEFAULTH;
    int fs = 8, lilo = 0, ldx = -2, ldy = -6, f = 0, v = 0, full = 0, style = 1;
    int angular = 0, ticks = 0, arcwidth = 0, start_angle = -135, end_angle = 135;
    t_symbol *wiperstyle = gensym("line");
    double min = 0.0, max = (double)(IEM_SL_DEFAULTSIZE - 1);
    char str[144];

    iem_inttosymargs(&x->x_gui.x_isa, 0);
    iem_inttofstyle(&x->x_gui.x_fsf, 0);

    x->x_gui.x_bcol = 0xFCFCFC;
    x->x_gui.x_fcol = 0x00;
    x->x_gui.x_lcol = 0x00;

    if((argc >= 17)&&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)
            &&IS_A_FLOAT(argv,2)&&IS_A_FLOAT(argv,3)
            &&IS_A_FLOAT(argv,4)&&IS_A_FLOAT(argv,5)
            &&(IS_A_SYMBOL(argv,6)||IS_A_FLOAT(argv,6))
            &&(IS_A_SYMBOL(argv,7)||IS_A_FLOAT(argv,7))
            &&(IS_A_SYMBOL(argv,8)||IS_A_FLOAT(argv,8))
            &&IS_A_FLOAT(argv,9)&&IS_A_FLOAT(argv,10)
            &&IS_A_FLOAT(argv,11)&&IS_A_FLOAT(argv,12)&&IS_A_FLOAT(argv,16))
    {
        w = (int)atom_getintarg(0, argc, argv);
        h = (int)atom_getintarg(1, argc, argv);
        min = (double)atom_getfloatarg(2, argc, argv);
        max = (double)atom_getfloatarg(3, argc, argv);
        lilo = (int)atom_getintarg(4, argc, argv);
        iem_inttosymargs(&x->x_gui.x_isa, atom_getintarg(5, argc, argv));
        iemgui_new_getnames(&x->x_gui, 6, argv);
        ldx = (int)atom_getintarg(9, argc, argv);
        ldy = (int)atom_getintarg(10, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getintarg(11, argc, argv));
        fs = (int)atom_getintarg(12, argc, argv);

        iemgui_all_loadcolors(&x->x_gui, argv+13, argv+14, argv+15);

        v = (int)atom_getintarg(16, argc, argv);
    }
    else iemgui_new_getnames(&x->x_gui, 6, 0);

    argc -= 17; argv += 17;
    if((argc > 7) && (IS_A_FLOAT(argv,0)) && (IS_A_FLOAT(argv,1)) 
        && (IS_A_FLOAT(argv,2)) && (IS_A_FLOAT(argv,3)) 
        && (IS_A_SYMBOL(argv,4)) && (IS_A_FLOAT(argv,5)) 
        && (IS_A_FLOAT(argv,6)) && (IS_A_FLOAT(argv,7)))
    {
        full = (int)atom_getint(argv++);
        style = (int)atom_getint(argv++);
        angular = (int)atom_getint(argv++);
        ticks = (int)atom_getint(argv++);
        wiperstyle = atom_getsymbol(argv++);
        arcwidth = (int)atom_getint(argv++);
        start_angle = (int)atom_getint(argv++);
        end_angle = (int)atom_getint(argv++);
    }

    x->x_gui.x_draw = (t_iemfunptr)knb_draw;
    x->x_gui.x_fsf.x_snd_able = 1;
    x->x_gui.x_fsf.x_rcv_able = 1;
    x->x_gui.x_glist = (t_glist *)canvas_getcurrent();

    if(x->x_gui.x_isa.x_loadinit)
        x->x_val = v;
    else
        x->x_val = 0;

    x->x_pos = x->x_val;

    if(lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;

    if(full != 0) full = 1;
    x->x_full = full;

    x->x_angular = (angular != 0);
    
    if(ticks < 0) ticks = 0;
    if(ticks > 100) ticks = 100;
    x->x_ticks = ticks;

    if(!strcmp(x->x_gui.x_snd->s_name, "empty")) x->x_gui.x_fsf.x_snd_able = 0;
    if(!strcmp(x->x_gui.x_rcv->s_name, "empty")) x->x_gui.x_fsf.x_rcv_able = 0;

    if(x->x_gui.x_fsf.x_font_style == 1) strcpy(x->x_gui.x_font, "helvetica");
    else if(x->x_gui.x_fsf.x_font_style == 2) strcpy(x->x_gui.x_font, "times");
    else
    {
        x->x_gui.x_fsf.x_font_style = 0;
        strcpy(x->x_gui.x_font, sys_font);
    }

    if(x->x_gui.x_fsf.x_rcv_able) pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);

    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;

    if(fs < 4) fs = 4;
    x->x_gui.x_fontsize = fs;

    if(style < 1) style = 1;
    x->x_currentstyle = x->x_style = style;
    
    x->x_current_wiper_style = x->x_wiper_style = wiperstyle;
    x->x_arc_width = arcwidth;
    x->x_start_angle = start_angle;
    x->x_end_angle = end_angle;

    iemgui_verify_snd_ne_rcv(&x->x_gui);
    knb_check_wh(x, w, h);
    knb_check_minmax(x, min, max);
    iemgui_newzoom(&x->x_gui);
    knb_update_H(x);

    x->x_fval = knb_getfval(x);
    x->x_force_outline_visible = 0;
    x->x_outline_visible = 0;
    x->x_inlet_visible = 0;
    x->x_outlet_visible = 0;
    outlet_new(&x->x_gui.x_obj, &s_float);
    return (x);
}

static void knb_free(t_knb *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    gfxstub_deleteforkey(x);
}

extern void canvas_iemguis(t_glist *gl, t_symbol *guiobjname);

void canvas_knb(t_glist *gl, t_symbol *s, int argc, t_atom *argv)
{
    canvas_iemguis(gl, gensym("knb"));
}

void g_knb_setup(void)
{
    knb_class = class_new(gensym("knb"), (t_newmethod)knb_new,
                            (t_method)knb_free, sizeof(t_knb), 0, A_GIMME, 0);

    class_addbang(knb_class,knb_bang);
    class_addfloat(knb_class,knb_float);
    class_addmethod(knb_class, (t_method)knb_click, gensym("click"),
                    A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(knb_class, (t_method)knb_motion, gensym("motion"),
                    A_FLOAT, A_FLOAT, 0);
    class_addmethod(knb_class, (t_method)knb_dialog, gensym("dialog"), A_GIMME, 0);
    class_addmethod(knb_class, (t_method)knb_loadbang, gensym("loadbang"), 0);
    class_addmethod(knb_class, (t_method)knb_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(knb_class, (t_method)knb_size, gensym("size"), A_FLOAT, 0);
    class_addmethod(knb_class, (t_method)knb_sensitivity, gensym("sensitivity"), A_FLOAT, 0);
    class_addmethod(knb_class, (t_method)knb_angular, gensym("angular"), A_FLOAT, 0);
    class_addmethod(knb_class, (t_method)knb_delta, gensym("delta"), A_GIMME, 0);
    class_addmethod(knb_class, (t_method)knb_pos, gensym("pos"), A_GIMME, 0);
    class_addmethod(knb_class, (t_method)knb_range, gensym("range"), A_GIMME, 0);
    class_addmethod(knb_class, (t_method)knb_color, gensym("color"), A_GIMME, 0);
    class_addmethod(knb_class, (t_method)knb_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(knb_class, (t_method)knb_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(knb_class, (t_method)knb_label, gensym("label"), A_DEFSYM, 0);
    class_addmethod(knb_class, (t_method)knb_label_pos, gensym("label_pos"), A_GIMME, 0);
    class_addmethod(knb_class, (t_method)knb_label_font, gensym("label_font"), A_GIMME, 0);
    class_addmethod(knb_class, (t_method)knb_log, gensym("log"), 0);
    class_addmethod(knb_class, (t_method)knb_lin, gensym("lin"), 0);
    class_addmethod(knb_class, (t_method)knb_init, gensym("init"), A_FLOAT, 0);
    class_addmethod(knb_class, (t_method)knb_full, gensym("full"), A_FLOAT, 0);
    class_addmethod(knb_class, (t_method)knb_style, gensym("style"), A_FLOAT, 0);
    class_addmethod(knb_class, (t_method)knb_zoom, gensym("zoom"), A_CANT, 0);

    knb_widgetbehavior.w_getrectfn =    knb_getrect;
    knb_widgetbehavior.w_displacefn =   iemgui_displace;
    knb_widgetbehavior.w_selectfn =     iemgui_select;
    knb_widgetbehavior.w_activatefn =   NULL;
    knb_widgetbehavior.w_deletefn =     iemgui_delete;
    knb_widgetbehavior.w_visfn =        iemgui_vis;
    knb_widgetbehavior.w_clickfn =      knb_newclick;

    class_setwidget(knb_class, &knb_widgetbehavior);

    class_setsavefn(knb_class, knb_save);
    class_setpropertiesfn(knb_class, knb_properties);

    class_addmethod(canvas_class, (t_method)canvas_knb, gensym("knb"),
                    A_GIMME, A_NULL);
}
