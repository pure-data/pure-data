/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

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


/* --------------- bng     gui-bang ------------------------- */

t_widgetbehavior bng_widgetbehavior;
static t_class *bng_class;

/*  widget helper functions  */


void bng_draw_update(t_bng *x, t_glist *glist)
{
    if(glist_isvisible(glist))
    {
        sys_vgui(".x%lx.c itemconfigure %lxBUT -fill #%6.6x\n", glist_getcanvas(glist), x,
                 x->x_flashed?x->x_gui.x_fcol:x->x_gui.x_bcol);
    }
}

void bng_draw_new(t_bng *x, t_glist *glist)
{
    int xpos=text_xpix(&x->x_gui.x_obj, glist);
    int ypos=text_ypix(&x->x_gui.x_obj, glist);
    int zoomlabel =
        1 + (IEMGUI_ZOOM(x)-1) * (x->x_gui.x_ldx >= 0 && x->x_gui.x_ldy >= 0);
    t_canvas *canvas=glist_getcanvas(glist);

    sys_vgui(".x%lx.c create rectangle %d %d %d %d -width %d -fill #%6.6x -tags %lxBASE\n",
             canvas, xpos, ypos,
             xpos + x->x_gui.x_w, ypos + x->x_gui.x_h,
             IEMGUI_ZOOM(x),
             x->x_gui.x_bcol, x);
    sys_vgui(".x%lx.c create oval %d %d %d %d -width %d -fill #%6.6x -tags %lxBUT\n",
             canvas, xpos+1, ypos+1,
             xpos + x->x_gui.x_w-1, ypos + x->x_gui.x_h-1,
             IEMGUI_ZOOM(x),
             x->x_flashed?x->x_gui.x_fcol:x->x_gui.x_bcol, x);
    sys_vgui(".x%lx.c create text %d %d -text {%s} -anchor w \
             -font {{%s} -%d %s} -fill #%6.6x -tags [list %lxLABEL label text]\n",
             canvas, xpos+x->x_gui.x_ldx * zoomlabel,
             ypos+x->x_gui.x_ldy * zoomlabel,
             strcmp(x->x_gui.x_lab->s_name, "empty")?x->x_gui.x_lab->s_name:"",
             x->x_gui.x_font, x->x_gui.x_fontsize, sys_fontweight,
             x->x_gui.x_lcol, x);
    if(!x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags [list %lxOUT%d outlet]\n",
             canvas, xpos,
             ypos + x->x_gui.x_h+1-2*IEMGUI_ZOOM(x), xpos + IOWIDTH,
             ypos + x->x_gui.x_h, x, 0);
    if(!x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags [list %lxIN%d inlet]\n",
             canvas, xpos, ypos,
             xpos + IOWIDTH, ypos-1+2*IEMGUI_ZOOM(x), x, 0);
}

void bng_draw_move(t_bng *x, t_glist *glist)
{
    int xpos=text_xpix(&x->x_gui.x_obj, glist);
    int ypos=text_ypix(&x->x_gui.x_obj, glist);
    int zoomlabel =
        1 + (IEMGUI_ZOOM(x)-1) * (x->x_gui.x_ldx >= 0 && x->x_gui.x_ldy >= 0);
    t_canvas *canvas=glist_getcanvas(glist);

    sys_vgui(".x%lx.c coords %lxBASE %d %d %d %d\n",
             canvas, x, xpos, ypos,
             xpos + x->x_gui.x_w, ypos + x->x_gui.x_h);
    sys_vgui(".x%lx.c coords %lxBUT %d %d %d %d\n",
             canvas, x, xpos+1,ypos+1,
             xpos + x->x_gui.x_w-1, ypos + x->x_gui.x_h-1);
    sys_vgui(".x%lx.c itemconfigure %lxBUT -fill #%6.6x\n", canvas, x,
             x->x_flashed?x->x_gui.x_fcol:x->x_gui.x_bcol);
    sys_vgui(".x%lx.c coords %lxLABEL %d %d\n",
             canvas, x, xpos+x->x_gui.x_ldx * zoomlabel,
             ypos+x->x_gui.x_ldy * zoomlabel);
    if(!x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c coords %lxOUT%d %d %d %d %d\n",
             canvas, x, 0, xpos,
             ypos + x->x_gui.x_h+1-2*IEMGUI_ZOOM(x), xpos + IOWIDTH,
             ypos + x->x_gui.x_h);
    if(!x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c coords %lxIN%d %d %d %d %d\n",
             canvas, x, 0, xpos, ypos,
             xpos + IOWIDTH, ypos-1+2*IEMGUI_ZOOM(x));
}

void bng_draw_erase(t_bng* x, t_glist* glist)
{
    t_canvas *canvas=glist_getcanvas(glist);

    sys_vgui(".x%lx.c delete %lxBASE\n", canvas, x);
    sys_vgui(".x%lx.c delete %lxBUT\n", canvas, x);
    sys_vgui(".x%lx.c delete %lxLABEL\n", canvas, x);
    if(!x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c delete %lxOUT%d\n", canvas, x, 0);
    if(!x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c delete %lxIN%d\n", canvas, x, 0);
}

void bng_draw_config(t_bng* x, t_glist* glist)
{
    t_canvas *canvas=glist_getcanvas(glist);

    sys_vgui(".x%lx.c itemconfigure %lxLABEL -font {{%s} -%d %s} -fill #%6.6x -text {%s} \n",
             canvas, x, x->x_gui.x_font, x->x_gui.x_fontsize, sys_fontweight,
             x->x_gui.x_fsf.x_selected?IEM_GUI_COLOR_SELECTED:x->x_gui.x_lcol,
             strcmp(x->x_gui.x_lab->s_name, "empty")?x->x_gui.x_lab->s_name:"");
    sys_vgui(".x%lx.c itemconfigure %lxBASE -fill #%6.6x\n", canvas, x, x->x_gui.x_bcol);
    sys_vgui(".x%lx.c itemconfigure %lxBUT -fill #%6.6x\n", canvas, x,
             x->x_flashed?x->x_gui.x_fcol:x->x_gui.x_bcol);
}

void bng_draw_io(t_bng* x, t_glist* glist, int old_snd_rcv_flags)
{
    int xpos=text_xpix(&x->x_gui.x_obj, glist);
    int ypos=text_ypix(&x->x_gui.x_obj, glist);
    t_canvas *canvas=glist_getcanvas(glist);

    if((old_snd_rcv_flags & IEM_GUI_OLD_SND_FLAG) && !x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxOUT%d\n",
             canvas, xpos,
             ypos + x->x_gui.x_h-1, xpos + IOWIDTH,
             ypos + x->x_gui.x_h, x, 0);
    if(!(old_snd_rcv_flags & IEM_GUI_OLD_SND_FLAG) && x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c delete %lxOUT%d\n", canvas, x, 0);
    if((old_snd_rcv_flags & IEM_GUI_OLD_RCV_FLAG) && !x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxIN%d\n",
             canvas, xpos, ypos,
             xpos + IOWIDTH, ypos+1, x, 0);
    if(!(old_snd_rcv_flags & IEM_GUI_OLD_RCV_FLAG) && x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c delete %lxIN%d\n", canvas, x, 0);
}

void bng_draw_select(t_bng* x, t_glist* glist)
{
    t_canvas *canvas=glist_getcanvas(glist);

    if(x->x_gui.x_fsf.x_selected)
    {
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline #%6.6x\n", canvas, x, IEM_GUI_COLOR_SELECTED);
        sys_vgui(".x%lx.c itemconfigure %lxBUT -outline #%6.6x\n", canvas, x, IEM_GUI_COLOR_SELECTED);
        sys_vgui(".x%lx.c itemconfigure %lxLABEL -fill #%6.6x\n", canvas, x, IEM_GUI_COLOR_SELECTED);
    }
    else
    {
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline #%6.6x\n", canvas, x, IEM_GUI_COLOR_NORMAL);
        sys_vgui(".x%lx.c itemconfigure %lxBUT -outline #%6.6x\n", canvas, x, IEM_GUI_COLOR_NORMAL);
        sys_vgui(".x%lx.c itemconfigure %lxLABEL -fill #%6.6x\n", canvas, x, x->x_gui.x_lcol);
    }
}

void bng_draw(t_bng *x, t_glist *glist, int mode)
{
    if(mode == IEM_GUI_DRAW_MODE_UPDATE)
        bng_draw_update(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_MOVE)
        bng_draw_move(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_NEW)
        bng_draw_new(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_SELECT)
        bng_draw_select(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_ERASE)
        bng_draw_erase(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_CONFIG)
        bng_draw_config(x, glist);
    else if(mode >= IEM_GUI_DRAW_MODE_IO)
        bng_draw_io(x, glist, mode - IEM_GUI_DRAW_MODE_IO);
}

/* ------------------------ bng widgetbehaviour----------------------------- */

static void bng_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_bng *x = (t_bng *)z;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist);
    *xp2 = *xp1 + x->x_gui.x_w;
    *yp2 = *yp1 + x->x_gui.x_h;
}

static void bng_save(t_gobj *z, t_binbuf *b)
{
    t_bng *x = (t_bng *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiiisssiiiisss", gensym("#X"),gensym("obj"),
                (int)x->x_gui.x_obj.te_xpix, (int)x->x_gui.x_obj.te_ypix,
                gensym("bng"), x->x_gui.x_w,
                x->x_flashtime_hold, x->x_flashtime_break,
                iem_symargstoint(&x->x_gui.x_isa),
                srl[0], srl[1], srl[2],
                x->x_gui.x_ldx, x->x_gui.x_ldy,
                iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize,
                bflcol[0], bflcol[1], bflcol[2]);
    binbuf_addv(b, ";");
}

void bng_check_minmax(t_bng *x, int ftbreak, int fthold)
{
    if(ftbreak > fthold)
    {
        int h;

        h = ftbreak;
        ftbreak = fthold;
        fthold = h;
    }
    if(ftbreak < IEM_BNG_MINBREAKFLASHTIME)
        ftbreak = IEM_BNG_MINBREAKFLASHTIME;
    if(fthold < IEM_BNG_MINHOLDFLASHTIME)
        fthold = IEM_BNG_MINHOLDFLASHTIME;
    x->x_flashtime_break = ftbreak;
    x->x_flashtime_hold = fthold;
}

static void bng_properties(t_gobj *z, t_glist *owner)
{
    t_bng *x = (t_bng *)z;
    char buf[800];
    t_symbol *srl[3];

    iemgui_properties(&x->x_gui, srl);
    sprintf(buf, "pdtk_iemgui_dialog %%s |bang| \
            ----------dimensions(pix):----------- %d %d size: 0 0 empty \
            --------flash-time(ms)(ms):--------- %d intrrpt: %d hold: %d \
            %d empty empty %d %d empty %d \
            %s %s \
            %s %d %d \
            %d %d \
            #%06x #%06x #%06x\n",
            x->x_gui.x_w, IEM_GUI_MINSIZE,
            x->x_flashtime_break, x->x_flashtime_hold, 2,/*min_max_schedule+clip*/
            -1, x->x_gui.x_isa.x_loadinit, -1, -1,/*no linlog, no multi*/
            srl[0]->s_name, srl[1]->s_name,
            srl[2]->s_name, x->x_gui.x_ldx, x->x_gui.x_ldy,
            x->x_gui.x_fsf.x_font_style, x->x_gui.x_fontsize,
            0xffffff & x->x_gui.x_bcol, 0xffffff & x->x_gui.x_fcol, 0xffffff & x->x_gui.x_lcol);
    gfxstub_new(&x->x_gui.x_obj.ob_pd, x, buf);
}

static void bng_set(t_bng *x)
{
    int holdtime = x->x_flashtime_hold;
    int sincelast = clock_gettimesince(x->x_lastflashtime);
    x->x_lastflashtime = clock_getsystime();
    if (sincelast < x->x_flashtime_hold*2)
        holdtime = sincelast/2;
    if (holdtime < x->x_flashtime_break)
        holdtime = x->x_flashtime_break;
    x->x_flashed = 1;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    clock_delay(x->x_clock_hld, holdtime);
}

static void bng_bout1(t_bng *x)/*wird nur mehr gesendet, wenn snd != rcv*/
{
    if(!x->x_gui.x_fsf.x_put_in2out)
    {
        x->x_gui.x_isa.x_locked = 1;
        clock_delay(x->x_clock_lck, 2);
    }
    outlet_bang(x->x_gui.x_obj.ob_outlet);
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing && x->x_gui.x_fsf.x_put_in2out)
        pd_bang(x->x_gui.x_snd->s_thing);
}

static void bng_bout2(t_bng *x)/*wird immer gesendet, wenn moeglich*/
{
    if(!x->x_gui.x_fsf.x_put_in2out)
    {
        x->x_gui.x_isa.x_locked = 1;
        clock_delay(x->x_clock_lck, 2);
    }
    outlet_bang(x->x_gui.x_obj.ob_outlet);
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
        pd_bang(x->x_gui.x_snd->s_thing);
}

static void bng_bang(t_bng *x)/*wird nur mehr gesendet, wenn snd != rcv*/
{
    if(!x->x_gui.x_isa.x_locked)
    {
        bng_set(x);
        bng_bout1(x);
    }
}

static void bng_bang2(t_bng *x)/*wird immer gesendet, wenn moeglich*/
{
    if(!x->x_gui.x_isa.x_locked)
    {
        bng_set(x);
        bng_bout2(x);
    }
}

static void bng_dialog(t_bng *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int a = (int)atom_getintarg(0, argc, argv);
    int fthold = (int)atom_getintarg(2, argc, argv);
    int ftbreak = (int)atom_getintarg(3, argc, argv);
    int sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);

    x->x_gui.x_w = iemgui_clip_size(a);
    x->x_gui.x_h = x->x_gui.x_w;
    bng_check_minmax(x, ftbreak, fthold);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_CONFIG);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO + sr_flags);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_MOVE);
    canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
}

static void bng_click(t_bng *x, t_floatarg xpos, t_floatarg ypos, t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    bng_set(x);
    bng_bout2(x);
}

static int bng_newclick(t_gobj *z, struct _glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if(doit)
        bng_click((t_bng *)z, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift, 0, (t_floatarg)alt);
    return (1);
}

static void bng_float(t_bng *x, t_floatarg f)
{bng_bang2(x);}

static void bng_symbol(t_bng *x, t_symbol *s)
{bng_bang2(x);}

static void bng_pointer(t_bng *x, t_gpointer *gp)
{bng_bang2(x);}

static void bng_list(t_bng *x, t_symbol *s, int ac, t_atom *av)
{
    bng_bang2(x);
}

static void bng_anything(t_bng *x, t_symbol *s, int argc, t_atom *argv)
{bng_bang2(x);}

static void bng_loadbang(t_bng *x, t_floatarg action)
{
    if (action == LB_LOAD && x->x_gui.x_isa.x_loadinit)
    {
        bng_set(x);
        bng_bout2(x);
    }
}

static void bng_size(t_bng *x, t_symbol *s, int ac, t_atom *av)
{
    x->x_gui.x_w = iemgui_clip_size((int)atom_getintarg(0, ac, av));
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_size((void *)x, &x->x_gui);
}

static void bng_delta(t_bng *x, t_symbol *s, int ac, t_atom *av)
{iemgui_delta((void *)x, &x->x_gui, s, ac, av);}

static void bng_pos(t_bng *x, t_symbol *s, int ac, t_atom *av)
{iemgui_pos((void *)x, &x->x_gui, s, ac, av);}

static void bng_flashtime(t_bng *x, t_symbol *s, int ac, t_atom *av)
{
    bng_check_minmax(x, (int)atom_getintarg(0, ac, av),
                     (int)atom_getintarg(1, ac, av));
}

static void bng_color(t_bng *x, t_symbol *s, int ac, t_atom *av)
{iemgui_color((void *)x, &x->x_gui, s, ac, av);}

static void bng_send(t_bng *x, t_symbol *s)
{iemgui_send(x, &x->x_gui, s);}

static void bng_receive(t_bng *x, t_symbol *s)
{iemgui_receive(x, &x->x_gui, s);}

static void bng_label(t_bng *x, t_symbol *s)
{iemgui_label((void *)x, &x->x_gui, s);}

static void bng_label_pos(t_bng *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_pos((void *)x, &x->x_gui, s, ac, av);}

static void bng_label_font(t_bng *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_font((void *)x, &x->x_gui, s, ac, av);}

static void bng_init(t_bng *x, t_floatarg f)
{
    x->x_gui.x_isa.x_loadinit = (f==0.0)?0:1;
}

static void bng_tick_hld(t_bng *x)
{
    x->x_flashed = 0;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void bng_tick_lck(t_bng *x)
{
    x->x_gui.x_isa.x_locked = 0;
}

static void *bng_new(t_symbol *s, int argc, t_atom *argv)
{
    t_bng *x = (t_bng *)pd_new(bng_class);
    int a=IEM_GUI_DEFAULTSIZE;
    int ldx=17, ldy=7;
    int fs=10;
    int ftbreak=IEM_BNG_DEFAULTBREAKFLASHTIME,
        fthold=IEM_BNG_DEFAULTHOLDFLASHTIME;
    char str[144];

    iem_inttosymargs(&x->x_gui.x_isa, 0);
    iem_inttofstyle(&x->x_gui.x_fsf, 0);

    x->x_gui.x_bcol = 0xFCFCFC;
    x->x_gui.x_fcol = 0x00;
    x->x_gui.x_lcol = 0x00;

    if((argc == 14)&&IS_A_FLOAT(argv,0)
       &&IS_A_FLOAT(argv,1)&&IS_A_FLOAT(argv,2)
       &&IS_A_FLOAT(argv,3)
       &&(IS_A_SYMBOL(argv,4)||IS_A_FLOAT(argv,4))
       &&(IS_A_SYMBOL(argv,5)||IS_A_FLOAT(argv,5))
       &&(IS_A_SYMBOL(argv,6)||IS_A_FLOAT(argv,6))
       &&IS_A_FLOAT(argv,7)&&IS_A_FLOAT(argv,8)
       &&IS_A_FLOAT(argv,9)&&IS_A_FLOAT(argv,10))
    {

        a = (int)atom_getintarg(0, argc, argv);
        fthold = (int)atom_getintarg(1, argc, argv);
        ftbreak = (int)atom_getintarg(2, argc, argv);
        iem_inttosymargs(&x->x_gui.x_isa, atom_getintarg(3, argc, argv));
        iemgui_new_getnames(&x->x_gui, 4, argv);
        ldx = (int)atom_getintarg(7, argc, argv);
        ldy = (int)atom_getintarg(8, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getintarg(9, argc, argv));
        fs = (int)atom_getintarg(10, argc, argv);
        iemgui_all_loadcolors(&x->x_gui, argv+11, argv+12, argv+13);
    }
    else iemgui_new_getnames(&x->x_gui, 4, 0);

    x->x_gui.x_draw = (t_iemfunptr)bng_draw;

    x->x_gui.x_fsf.x_snd_able = 1;
    x->x_gui.x_fsf.x_rcv_able = 1;
    x->x_flashed = 0;
    x->x_gui.x_glist = (t_glist *)canvas_getcurrent();
    if (!strcmp(x->x_gui.x_snd->s_name, "empty"))
        x->x_gui.x_fsf.x_snd_able = 0;
    if (!strcmp(x->x_gui.x_rcv->s_name, "empty"))
        x->x_gui.x_fsf.x_rcv_able = 0;
    if(x->x_gui.x_fsf.x_font_style == 1) strcpy(x->x_gui.x_font, "helvetica");
    else if(x->x_gui.x_fsf.x_font_style == 2) strcpy(x->x_gui.x_font, "times");
    else { x->x_gui.x_fsf.x_font_style = 0;
        strcpy(x->x_gui.x_font, sys_font); }

    if (x->x_gui.x_fsf.x_rcv_able)
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;

    if(fs < 4)
        fs = 4;
    x->x_gui.x_fontsize = fs;
    x->x_gui.x_w = iemgui_clip_size(a);
    x->x_gui.x_h = x->x_gui.x_w;
    bng_check_minmax(x, ftbreak, fthold);
    x->x_gui.x_isa.x_locked = 0;
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    x->x_lastflashtime = clock_getsystime();
    x->x_clock_hld = clock_new(x, (t_method)bng_tick_hld);
    x->x_clock_lck = clock_new(x, (t_method)bng_tick_lck);
    outlet_new(&x->x_gui.x_obj, &s_bang);
    return (x);
}

static void bng_ff(t_bng *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    clock_free(x->x_clock_lck);
    clock_free(x->x_clock_hld);
    gfxstub_deleteforkey(x);
}

void g_bang_setup(void)
{
    bng_class = class_new(gensym("bng"), (t_newmethod)bng_new,
                          (t_method)bng_ff, sizeof(t_bng), 0, A_GIMME, 0);
    class_addbang(bng_class, bng_bang);
    class_addfloat(bng_class, bng_float);
    class_addsymbol(bng_class, bng_symbol);
    class_addpointer(bng_class, bng_pointer);
    class_addlist(bng_class, bng_list);
    class_addanything(bng_class, bng_anything);
    class_addmethod(bng_class, (t_method)bng_click, gensym("click"),
                    A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(bng_class, (t_method)bng_dialog, gensym("dialog"),
                    A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_loadbang, gensym("loadbang"),
        A_DEFFLOAT, 0);
    class_addmethod(bng_class, (t_method)bng_size, gensym("size"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_delta, gensym("delta"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_pos, gensym("pos"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_flashtime, gensym("flashtime"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_color, gensym("color"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(bng_class, (t_method)bng_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(bng_class, (t_method)bng_label, gensym("label"), A_DEFSYM, 0);
    class_addmethod(bng_class, (t_method)bng_label_pos, gensym("label_pos"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_label_font, gensym("label_font"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_init, gensym("init"), A_FLOAT, 0);
    class_addmethod(bng_class, (t_method)iemgui_zoom, gensym("zoom"),
        A_CANT, 0);
    bng_widgetbehavior.w_getrectfn = bng_getrect;
    bng_widgetbehavior.w_displacefn = iemgui_displace;
    bng_widgetbehavior.w_selectfn = iemgui_select;
    bng_widgetbehavior.w_activatefn = NULL;
    bng_widgetbehavior.w_deletefn = iemgui_delete;
    bng_widgetbehavior.w_visfn = iemgui_vis;
    bng_widgetbehavior.w_clickfn = bng_newclick;
    class_setwidget(bng_class, &bng_widgetbehavior);
    class_sethelpsymbol(bng_class, gensym("bng"));
    class_setsavefn(bng_class, bng_save);
    class_setpropertiesfn(bng_class, bng_properties);
}
