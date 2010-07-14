/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* vdial.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */

/* name change to vradio by MSP (it's a radio button really) and changed to
put out a "float" as in sliders, toggles, etc. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "m_pd.h"
#include "g_canvas.h"

#include "g_all_guis.h"
#include <math.h>

/*------------------ global variables -------------------------*/


/*------------------ global functions -------------------------*/




/* ------------- vdl     gui-vertical radio button ---------------------- */

t_widgetbehavior vradio_widgetbehavior;
static t_class *vradio_class, *vradio_old_class;

/* widget helper functions */

void vradio_draw_update(t_gobj *client, t_glist *glist)
{
    t_hradio *x = (t_hradio *)client;
    if(glist_isvisible(glist))
    {
        t_canvas *canvas=glist_getcanvas(glist);

        sys_vgui(".x%lx.c itemconfigure %lxBUT%d -fill #%6.6x -outline #%6.6x\n",
                 canvas, x, x->x_drawn,
                 x->x_gui.x_bcol, x->x_gui.x_bcol);
        sys_vgui(".x%lx.c itemconfigure %lxBUT%d -fill #%6.6x -outline #%6.6x\n",
                 canvas, x, x->x_on,
                 x->x_gui.x_fcol, x->x_gui.x_fcol);
        x->x_drawn = x->x_on;
    }
}

void vradio_draw_new(t_vradio *x, t_glist *glist)
{
    t_canvas *canvas=glist_getcanvas(glist);
    int n=x->x_number, i, dy=x->x_gui.x_h, s4=dy/4;
    int yy11b=text_ypix(&x->x_gui.x_obj, glist); 
    int yy11=yy11b, yy12=yy11+dy;
    int yy21=yy11+s4, yy22=yy12-s4;
    int xx11=text_xpix(&x->x_gui.x_obj, glist), xx12=xx11+dy;
    int xx21=xx11+s4, xx22=xx12-s4;

    for(i=0; i<n; i++)
    {
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill #%6.6x -tags %lxBASE%d\n",
                 canvas, xx11, yy11, xx12, yy12,
                 x->x_gui.x_bcol, x, i);
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill #%6.6x -outline #%6.6x -tags %lxBUT%d\n",
                 canvas, xx21, yy21, xx22, yy22,
                 (x->x_on==i)?x->x_gui.x_fcol:x->x_gui.x_bcol,
                 (x->x_on==i)?x->x_gui.x_fcol:x->x_gui.x_bcol, x, i);
        yy11 += dy;
        yy12 += dy;
        yy21 += dy;
        yy22 += dy;
        x->x_drawn = x->x_on;
    }
    sys_vgui(".x%lx.c create text %d %d -text {%s} -anchor w \
             -font {{%s} -%d %s} -fill #%6.6x -tags [list %lxLABEL label text]\n",
             canvas, xx11+x->x_gui.x_ldx, yy11b+x->x_gui.x_ldy,
             strcmp(x->x_gui.x_lab->s_name, "empty")?x->x_gui.x_lab->s_name:"",
             x->x_gui.x_font, x->x_gui.x_fontsize, sys_fontweight,
             x->x_gui.x_lcol, x);
    if(!x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags [list %lxOUT%d outlet]\n",
             canvas, xx11, yy11-1, xx11 + IOWIDTH, yy11, x, 0);
    if(!x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags [list %lxIN%d inlet]\n",
             canvas, xx11, yy11b, xx11 + IOWIDTH, yy11b+1, x, 0);
}

void vradio_draw_move(t_vradio *x, t_glist *glist)
{
    t_canvas *canvas=glist_getcanvas(glist);
    int n=x->x_number, i, dy=x->x_gui.x_h, s4=dy/4;
    int yy11b=text_ypix(&x->x_gui.x_obj, glist);
    int yy11=yy11b, yy12=yy11+dy;
    int yy21=yy11+s4, yy22=yy12-s4;
    int xx11=text_xpix(&x->x_gui.x_obj, glist), xx12=xx11+dy;
    int xx21=xx11+s4, xx22=xx12-s4;

    for(i=0; i<n; i++)
    {
        sys_vgui(".x%lx.c coords %lxBASE%d %d %d %d %d\n",
                 canvas, x, i, xx11, yy11, xx12, yy12);
        sys_vgui(".x%lx.c coords %lxBUT%d %d %d %d %d\n",
                 canvas, x, i, xx21, yy21, xx22, yy22);
        yy11 += dy;
        yy12 += dy;
        yy21 += dy;
        yy22 += dy;
    }
    sys_vgui(".x%lx.c coords %lxLABEL %d %d\n",
             canvas, x, xx11+x->x_gui.x_ldx, yy11b+x->x_gui.x_ldy);
    if(!x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c coords %lxOUT%d %d %d %d %d\n",
             canvas, x, 0, xx11, yy11-1, xx11 + IOWIDTH, yy11);
    if(!x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c coords %lxIN%d %d %d %d %d\n",
             canvas, x, 0, xx11, yy11b, xx11 + IOWIDTH, yy11b+1);
}

void vradio_draw_erase(t_vradio* x, t_glist* glist)
{
    t_canvas *canvas=glist_getcanvas(glist);
    int n=x->x_number, i;

    for(i=0; i<n; i++)
    {
        sys_vgui(".x%lx.c delete %lxBASE%d\n", canvas, x, i);
        sys_vgui(".x%lx.c delete %lxBUT%d\n", canvas, x, i);
    }
    sys_vgui(".x%lx.c delete %lxLABEL\n", canvas, x);
    if(!x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c delete %lxOUT%d\n", canvas, x, 0);
    if(!x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c delete %lxIN%d\n", canvas, x, 0);
}

void vradio_draw_config(t_vradio* x, t_glist* glist)
{
    t_canvas *canvas=glist_getcanvas(glist);
    int n=x->x_number, i;

    sys_vgui(".x%lx.c itemconfigure %lxLABEL -font {{%s} -%d %s} -fill #%6.6x -text {%s} \n",
             canvas, x, x->x_gui.x_font, x->x_gui.x_fontsize, sys_fontweight, 
             x->x_gui.x_fsf.x_selected?IEM_GUI_COLOR_SELECTED:x->x_gui.x_lcol,
             strcmp(x->x_gui.x_lab->s_name, "empty")?x->x_gui.x_lab->s_name:"");
    for(i=0; i<n; i++)
    {
        sys_vgui(".x%lx.c itemconfigure %lxBASE%d -fill #%6.6x\n", canvas, x, i,
                 x->x_gui.x_bcol);
        sys_vgui(".x%lx.c itemconfigure %lxBUT%d -fill #%6.6x -outline #%6.6x\n", canvas, x, i,
                 (x->x_on==i)?x->x_gui.x_fcol:x->x_gui.x_bcol,
                 (x->x_on==i)?x->x_gui.x_fcol:x->x_gui.x_bcol);
    }
}

void vradio_draw_io(t_vradio* x, t_glist* glist, int old_snd_rcv_flags)
{
    t_canvas *canvas=glist_getcanvas(glist);
    int xpos=text_xpix(&x->x_gui.x_obj, glist);
    int ypos=text_ypix(&x->x_gui.x_obj, glist);

    if((old_snd_rcv_flags & IEM_GUI_OLD_SND_FLAG) && !x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxOUT%d\n",
                 canvas, xpos,
                 ypos+(x->x_number*x->x_gui.x_h)-1,
                 xpos+ IOWIDTH,
                 ypos+(x->x_number*x->x_gui.x_h), x, 0);
    if(!(old_snd_rcv_flags & IEM_GUI_OLD_SND_FLAG) && x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c delete %lxOUT%d\n", canvas, x, 0);
    if((old_snd_rcv_flags & IEM_GUI_OLD_RCV_FLAG) && !x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxIN%d\n",
                 canvas, xpos, ypos,
                 xpos+ IOWIDTH, ypos+1,
                 x, 0);
    if(!(old_snd_rcv_flags & IEM_GUI_OLD_RCV_FLAG) && x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c delete %lxIN%d\n", canvas, x, 0);
}

void vradio_draw_select(t_vradio* x, t_glist* glist)
{
    t_canvas *canvas=glist_getcanvas(glist);
    int n=x->x_number, i;

    if(x->x_gui.x_fsf.x_selected)
    {
        for(i=0; i<n; i++)
        {
            sys_vgui(".x%lx.c itemconfigure %lxBASE%d -outline #%6.6x\n", canvas, x, i,
                     IEM_GUI_COLOR_SELECTED);
        }
        sys_vgui(".x%lx.c itemconfigure %lxLABEL -fill #%6.6x\n", canvas, x, IEM_GUI_COLOR_SELECTED);
    }
    else
    {
        for(i=0; i<n; i++)
        {
            sys_vgui(".x%lx.c itemconfigure %lxBASE%d -outline #%6.6x\n", canvas, x, i,
                     IEM_GUI_COLOR_NORMAL);
        }
        sys_vgui(".x%lx.c itemconfigure %lxLABEL -fill #%6.6x\n", canvas, x,
                 x->x_gui.x_lcol);
    }
}

void vradio_draw(t_vradio *x, t_glist *glist, int mode)
{
    if(mode == IEM_GUI_DRAW_MODE_UPDATE)
        sys_queuegui(x, glist, vradio_draw_update);
    else if(mode == IEM_GUI_DRAW_MODE_MOVE)
        vradio_draw_move(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_NEW)
        vradio_draw_new(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_SELECT)
        vradio_draw_select(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_ERASE)
        vradio_draw_erase(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_CONFIG)
        vradio_draw_config(x, glist);
    else if(mode >= IEM_GUI_DRAW_MODE_IO)
        vradio_draw_io(x, glist, mode - IEM_GUI_DRAW_MODE_IO);
}

/* ------------------------ vdl widgetbehaviour----------------------------- */

static void vradio_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_vradio *x = (t_vradio *)z;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist);
    *xp2 = *xp1 + x->x_gui.x_w;
    *yp2 = *yp1 + x->x_gui.x_h*x->x_number;
}

static void vradio_save(t_gobj *z, t_binbuf *b)
{
    t_vradio *x = (t_vradio *)z;
    int bflcol[3];
    t_symbol *srl[3];

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiiisssiiiiiiii", gensym("#X"),gensym("obj"),
                (int)x->x_gui.x_obj.te_xpix,
                (int)x->x_gui.x_obj.te_ypix,
                (pd_class(&x->x_gui.x_obj.ob_pd) == vradio_old_class ?
                    gensym("vdl") : gensym("vradio")),
                x->x_gui.x_w,
                x->x_change, iem_symargstoint(&x->x_gui.x_isa), x->x_number,
                srl[0], srl[1], srl[2],
                x->x_gui.x_ldx, x->x_gui.x_ldy,
                iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize,
                bflcol[0], bflcol[1], bflcol[2], x->x_on);
    binbuf_addv(b, ";");
}

static void vradio_properties(t_gobj *z, t_glist *owner)
{
    t_vradio *x = (t_vradio *)z;
    char buf[800];
    t_symbol *srl[3];
    int hchange=-1;

    iemgui_properties(&x->x_gui, srl);
    if(pd_class(&x->x_gui.x_obj.ob_pd) == vradio_old_class)
        hchange = x->x_change;
    sprintf(buf, "pdtk_iemgui_dialog %%s |vradio| \
            ----------dimensions(pix):----------- %d %d size: 0 0 empty \
            empty 0.0 empty 0.0 empty %d \
            %d new-only new&old %d %d number: %d \
            %s %s \
            %s %d %d \
            %d %d \
            %d %d %d\n",
            x->x_gui.x_w, IEM_GUI_MINSIZE,
            0,/*no_schedule*/
            hchange, x->x_gui.x_isa.x_loadinit, -1, x->x_number,
            srl[0]->s_name, srl[1]->s_name,
            srl[2]->s_name, x->x_gui.x_ldx, x->x_gui.x_ldy,
            x->x_gui.x_fsf.x_font_style, x->x_gui.x_fontsize,
            0xffffff & x->x_gui.x_bcol, 0xffffff & x->x_gui.x_fcol, 0xffffff & x->x_gui.x_lcol);
    gfxstub_new(&x->x_gui.x_obj.ob_pd, x, buf);
}

static void vradio_dialog(t_vradio *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int a = (int)atom_getintarg(0, argc, argv);
    int chg = (int)atom_getintarg(4, argc, argv);
    int num = (int)atom_getintarg(6, argc, argv);
    int sr_flags;

    if(chg != 0) chg = 1;
    x->x_change = chg;
    sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);
    x->x_gui.x_w = iemgui_clip_size(a);
    x->x_gui.x_h = x->x_gui.x_w;
    if(x->x_number != num)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_ERASE);
        x->x_number = num;
        if(x->x_on >= x->x_number)
        {
            x->x_on = x->x_number - 1;
            x->x_on_old = x->x_on;
        }
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_NEW);
    }
    else
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_CONFIG);
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO + sr_flags);
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_MOVE);
        canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
    }
}

static void vradio_set(t_vradio *x, t_floatarg f)
{
    int i=(int)f;
    int old;

    if(i < 0)
        i = 0;
    if(i >= x->x_number)
        i = x->x_number-1;
    if(x->x_on != x->x_on_old)
    {
        old = x->x_on_old;
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

static void vradio_bang(t_vradio *x)
{
        /* compatibility with earlier  "vdial" behavior */
    if (pd_class(&x->x_gui.x_obj.ob_pd) == vradio_old_class)
    {
        if((x->x_change)&&(x->x_on != x->x_on_old))
        {
            SETFLOAT(x->x_at, (t_float)x->x_on_old);
            SETFLOAT(x->x_at+1, 0.0);
            outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, x->x_at);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, x->x_at);
        }
        x->x_on_old = x->x_on;
        SETFLOAT(x->x_at, (t_float)x->x_on);
        SETFLOAT(x->x_at+1, 1.0);
        outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, x->x_at);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, x->x_at);
    }
    else
    {
        outlet_float(x->x_gui.x_obj.ob_outlet, x->x_on);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_float(x->x_gui.x_snd->s_thing, x->x_on);
    }
}

static void vradio_fout(t_vradio *x, t_floatarg f)
{
    int i=(int)f;

    if(i < 0)
        i = 0;
    if(i >= x->x_number)
        i = x->x_number-1;

    if (pd_class(&x->x_gui.x_obj.ob_pd) == vradio_old_class)
    {
            /* compatibility with earlier  "vdial" behavior */
        if((x->x_change)&&(i != x->x_on_old))
        {
            SETFLOAT(x->x_at, (t_float)x->x_on_old);
            SETFLOAT(x->x_at+1, 0.0);
            outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, x->x_at);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, x->x_at);
        }
        if(x->x_on != x->x_on_old)
            x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        x->x_on_old = x->x_on;
        SETFLOAT(x->x_at, (t_float)x->x_on);
        SETFLOAT(x->x_at+1, 1.0);
        outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, x->x_at);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, x->x_at);
    }
    else
    {
        x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        outlet_float(x->x_gui.x_obj.ob_outlet, x->x_on);
        if (x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_float(x->x_gui.x_snd->s_thing, x->x_on);
    }
}

static void vradio_float(t_vradio *x, t_floatarg f)
{
    int i=(int)f;

    if(i < 0)
        i = 0;
    if(i >= x->x_number)
        i = x->x_number-1;

    if (pd_class(&x->x_gui.x_obj.ob_pd) == vradio_old_class)
    {
            /* compatibility with earlier  "vdial" behavior */
        if((x->x_change)&&(i != x->x_on_old))
        {
            if(x->x_gui.x_fsf.x_put_in2out)
            {
                SETFLOAT(x->x_at, (t_float)x->x_on_old);
                SETFLOAT(x->x_at+1, 0.0);
                outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, x->x_at);
                if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                    pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, x->x_at);
            }
        }
        if(x->x_on != x->x_on_old)
            x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        x->x_on_old = x->x_on;
        if(x->x_gui.x_fsf.x_put_in2out)
        {
            SETFLOAT(x->x_at, (t_float)x->x_on);
            SETFLOAT(x->x_at+1, 1.0);
            outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, x->x_at);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, x->x_at);
        }
    }
    else
    {
        x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        if (x->x_gui.x_fsf.x_put_in2out)
        {
            outlet_float(x->x_gui.x_obj.ob_outlet, x->x_on);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_float(x->x_gui.x_snd->s_thing, x->x_on);
        }
    }
}

static void vradio_click(t_vradio *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    int yy =  (int)ypos - text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);

    vradio_fout(x, (t_float)(yy / x->x_gui.x_h));
}

static int vradio_newclick(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if(doit)
        vradio_click((t_vradio *)z, (t_floatarg)xpix, (t_floatarg)ypix,
            (t_floatarg)shift, 0, (t_floatarg)alt);
    return (1);
}

static void vradio_loadbang(t_vradio *x)
{
    if(!sys_noloadbang && x->x_gui.x_isa.x_loadinit)
        vradio_bang(x);
}

static void vradio_number(t_vradio *x, t_floatarg num)
{
    int n=(int)num;

    if(n < 1)
        n = 1;
    if(n > IEM_RADIO_MAX)
        n = IEM_RADIO_MAX;
    if(n != x->x_number)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_ERASE);
        x->x_number = n;
        if(x->x_on >= x->x_number)
            x->x_on = x->x_number - 1;
        x->x_on_old = x->x_on;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_NEW);
    }
}

static void vradio_size(t_vradio *x, t_symbol *s, int ac, t_atom *av)
{
    x->x_gui.x_w = iemgui_clip_size((int)atom_getintarg(0, ac, av));
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_size((void *)x, &x->x_gui);
}

static void vradio_delta(t_vradio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_delta((void *)x, &x->x_gui, s, ac, av);}

static void vradio_pos(t_vradio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_pos((void *)x, &x->x_gui, s, ac, av);}

static void vradio_color(t_vradio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_color((void *)x, &x->x_gui, s, ac, av);}

static void vradio_send(t_vradio *x, t_symbol *s)
{iemgui_send(x, &x->x_gui, s);}

static void vradio_receive(t_vradio *x, t_symbol *s)
{iemgui_receive(x, &x->x_gui, s);}

static void vradio_label(t_vradio *x, t_symbol *s)
{iemgui_label((void *)x, &x->x_gui, s);}

static void vradio_label_pos(t_vradio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_pos((void *)x, &x->x_gui, s, ac, av);}

static void vradio_label_font(t_vradio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_font((void *)x, &x->x_gui, s, ac, av);}

static void vradio_init(t_vradio *x, t_floatarg f)
{
    x->x_gui.x_isa.x_loadinit = (f==0.0)?0:1;
}

static void vradio_double_change(t_vradio *x)
{x->x_change = 1;}

static void vradio_single_change(t_vradio *x)
{x->x_change = 0;}

static void *vradio_donew(t_symbol *s, int argc, t_atom *argv, int old)
{
    t_vradio *x = (t_vradio *)pd_new(old? vradio_old_class : vradio_class);
    int bflcol[]={-262144, -1, -1};
    int a=IEM_GUI_DEFAULTSIZE, on=0, f=0;
    int ldx=0, ldy=-8, chg=1, num=8;
    int fs=10;
    int ftbreak=IEM_BNG_DEFAULTBREAKFLASHTIME, fthold=IEM_BNG_DEFAULTHOLDFLASHTIME;
    char str[144];

    if((argc == 15)&&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)&&IS_A_FLOAT(argv,2)
       &&IS_A_FLOAT(argv,3)
       &&(IS_A_SYMBOL(argv,4)||IS_A_FLOAT(argv,4))
       &&(IS_A_SYMBOL(argv,5)||IS_A_FLOAT(argv,5))
       &&(IS_A_SYMBOL(argv,6)||IS_A_FLOAT(argv,6))
       &&IS_A_FLOAT(argv,7)&&IS_A_FLOAT(argv,8)
       &&IS_A_FLOAT(argv,9)&&IS_A_FLOAT(argv,10)&&IS_A_FLOAT(argv,11)
       &&IS_A_FLOAT(argv,12)&&IS_A_FLOAT(argv,13)&&IS_A_FLOAT(argv,14))
    {
        a = (int)atom_getintarg(0, argc, argv);
        chg = (int)atom_getintarg(1, argc, argv);
        iem_inttosymargs(&x->x_gui.x_isa, atom_getintarg(2, argc, argv));
        num = (int)atom_getintarg(3, argc, argv);
        iemgui_new_getnames(&x->x_gui, 4, argv);
        ldx = (int)atom_getintarg(7, argc, argv);
        ldy = (int)atom_getintarg(8, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getintarg(9, argc, argv));
        fs = (int)atom_getintarg(10, argc, argv);
        bflcol[0] = (int)atom_getintarg(11, argc, argv);
        bflcol[1] = (int)atom_getintarg(12, argc, argv);
        bflcol[2] = (int)atom_getintarg(13, argc, argv);
        on = (int)atom_getintarg(14, argc, argv);
    }
    else iemgui_new_getnames(&x->x_gui, 4, 0);
    x->x_gui.x_draw = (t_iemfunptr)vradio_draw;
    x->x_gui.x_fsf.x_snd_able = 1;
    x->x_gui.x_fsf.x_rcv_able = 1;
    x->x_gui.x_glist = (t_glist *)canvas_getcurrent();
    if (!strcmp(x->x_gui.x_snd->s_name, "empty"))
        x->x_gui.x_fsf.x_snd_able = 0;
    if (!strcmp(x->x_gui.x_rcv->s_name, "empty"))
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
    if(on < 0)
        on = 0;
    if(on >= x->x_number)
        on = x->x_number - 1;
    if(x->x_gui.x_isa.x_loadinit)
        x->x_on = on;
    else
        x->x_on = 0;
    x->x_on_old = x->x_on;
    x->x_change = (chg==0)?0:1;
    if (x->x_gui.x_fsf.x_rcv_able)
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;
    if(fs < 4)
        fs = 4;
    x->x_gui.x_fontsize = fs;
    x->x_gui.x_w = iemgui_clip_size(a);
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    iemgui_all_colfromload(&x->x_gui, bflcol);
    outlet_new(&x->x_gui.x_obj, &s_list);
    return (x);
}

static void *vradio_new(t_symbol *s, int argc, t_atom *argv)
{
    return (vradio_donew(s, argc, argv, 0));
}

static void *vdial_new(t_symbol *s, int argc, t_atom *argv)
{
    return (vradio_donew(s, argc, argv, 1));
}

static void vradio_ff(t_vradio *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    gfxstub_deleteforkey(x);
}

void g_vradio_setup(void)
{
    vradio_class = class_new(gensym("vradio"), (t_newmethod)vradio_new,
        (t_method)vradio_ff, sizeof(t_vradio), 0, A_GIMME, 0);
    class_addbang(vradio_class, vradio_bang);
    class_addfloat(vradio_class, vradio_float);
    class_addmethod(vradio_class, (t_method)vradio_click, gensym("click"),
                    A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(vradio_class, (t_method)vradio_dialog, gensym("dialog"),
                    A_GIMME, 0);
    class_addmethod(vradio_class, (t_method)vradio_loadbang,
        gensym("loadbang"), 0);
    class_addmethod(vradio_class, (t_method)vradio_set,
        gensym("set"), A_FLOAT, 0);
    class_addmethod(vradio_class, (t_method)vradio_size,
        gensym("size"), A_GIMME, 0);
    class_addmethod(vradio_class, (t_method)vradio_delta,
        gensym("delta"), A_GIMME, 0);
    class_addmethod(vradio_class, (t_method)vradio_pos,
        gensym("pos"), A_GIMME, 0);
    class_addmethod(vradio_class, (t_method)vradio_color,
        gensym("color"), A_GIMME, 0);
    class_addmethod(vradio_class, (t_method)vradio_send,
        gensym("send"), A_DEFSYM, 0);
    class_addmethod(vradio_class, (t_method)vradio_receive,
        gensym("receive"), A_DEFSYM, 0);
    class_addmethod(vradio_class, (t_method)vradio_label,
        gensym("label"), A_DEFSYM, 0);
    class_addmethod(vradio_class, (t_method)vradio_label_pos,
        gensym("label_pos"), A_GIMME, 0);
    class_addmethod(vradio_class, (t_method)vradio_label_font,
        gensym("label_font"), A_GIMME, 0);
    class_addmethod(vradio_class, (t_method)vradio_init,
        gensym("init"), A_FLOAT, 0);
    class_addmethod(vradio_class, (t_method)vradio_number,
        gensym("number"), A_FLOAT, 0);
    class_addmethod(vradio_class, (t_method)vradio_single_change,
        gensym("single_change"), 0);
    class_addmethod(vradio_class, (t_method)vradio_double_change,
        gensym("double_change"), 0);
    vradio_widgetbehavior.w_getrectfn = vradio_getrect;
    vradio_widgetbehavior.w_displacefn = iemgui_displace;
    vradio_widgetbehavior.w_selectfn = iemgui_select;
    vradio_widgetbehavior.w_activatefn = NULL;
    vradio_widgetbehavior.w_deletefn = iemgui_delete;
    vradio_widgetbehavior.w_visfn = iemgui_vis;
    vradio_widgetbehavior.w_clickfn = vradio_newclick;
    class_setwidget(vradio_class, &vradio_widgetbehavior);
    class_sethelpsymbol(vradio_class, gensym("vradio"));
    class_setsavefn(vradio_class, vradio_save);
    class_setpropertiesfn(vradio_class, vradio_properties);

        /* obsolete version (0.34-0.35) */
    vradio_old_class = class_new(gensym("vdl"), (t_newmethod)vdial_new,
        (t_method)vradio_ff, sizeof(t_vradio), 0, A_GIMME, 0);
    class_addbang(vradio_old_class, vradio_bang);
    class_addfloat(vradio_old_class, vradio_float);
    class_addmethod(vradio_old_class, (t_method)vradio_click, gensym("click"),
                    A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_dialog, gensym("dialog"),
                    A_GIMME, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_loadbang, gensym("loadbang"), 0);
    class_addmethod(vradio_old_class, (t_method)vradio_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_size, gensym("size"), A_GIMME, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_delta, gensym("delta"), A_GIMME, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_pos, gensym("pos"), A_GIMME, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_color, gensym("color"), A_GIMME, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_label, gensym("label"), A_DEFSYM, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_label_pos, gensym("label_pos"), A_GIMME, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_label_font, gensym("label_font"), A_GIMME, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_init, gensym("init"), A_FLOAT, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_number, gensym("number"), A_FLOAT, 0);
    class_addmethod(vradio_old_class, (t_method)vradio_single_change, gensym("single_change"), 0);
    class_addmethod(vradio_old_class, (t_method)vradio_double_change, gensym("double_change"), 0);
    class_setwidget(vradio_old_class, &vradio_widgetbehavior);
    class_sethelpsymbol(vradio_old_class, gensym("vradio"));
}
