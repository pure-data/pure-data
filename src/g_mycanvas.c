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

/* ---------- cnv  my gui-canvas for a window ---------------- */

t_widgetbehavior my_canvas_widgetbehavior;
static t_class *my_canvas_class;

/* widget helper functions */

void my_canvas_draw_new(t_my_canvas *x, t_glist *glist)
{
    int xpos=text_xpix(&x->x_gui.x_obj, glist);
    int ypos=text_ypix(&x->x_gui.x_obj, glist);
    int zoomlabel =
        1 + (IEMGUI_ZOOM(x)-1) * (x->x_gui.x_ldx >= 0 && x->x_gui.x_ldy >= 0);
    t_canvas *canvas=glist_getcanvas(glist);

    sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill #%06x -outline #%06x -tags %lxRECT\n",
             canvas, xpos, ypos,
             xpos + x->x_vis_w*IEMGUI_ZOOM(x), ypos + x->x_vis_h*IEMGUI_ZOOM(x),
             x->x_gui.x_bcol, x->x_gui.x_bcol, x);
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline #%06x -tags %lxBASE\n",
             canvas, xpos, ypos,
             xpos + x->x_gui.x_w, ypos + x->x_gui.x_h,
             x->x_gui.x_bcol, x);
    sys_vgui(".x%lx.c create text %d %d -text {%s} -anchor w \
             -font {{%s} -%d %s} -fill #%06x -tags [list %lxLABEL label text]\n",
             canvas, xpos+x->x_gui.x_ldx * zoomlabel,
             ypos+x->x_gui.x_ldy * zoomlabel,
             strcmp(x->x_gui.x_lab->s_name, "empty")?x->x_gui.x_lab->s_name:"",
             x->x_gui.x_font, x->x_gui.x_fontsize, sys_fontweight,
             x->x_gui.x_lcol, x);
}

void my_canvas_draw_move(t_my_canvas *x, t_glist *glist)
{
    int xpos=text_xpix(&x->x_gui.x_obj, glist);
    int ypos=text_ypix(&x->x_gui.x_obj, glist);
    int zoomlabel =
        1 + (IEMGUI_ZOOM(x)-1) * (x->x_gui.x_ldx >= 0 && x->x_gui.x_ldy >= 0);
    t_canvas *canvas=glist_getcanvas(glist);

    sys_vgui(".x%lx.c coords %lxRECT %d %d %d %d\n",
             canvas, x, xpos, ypos, xpos + x->x_vis_w*IEMGUI_ZOOM(x),
             ypos + x->x_vis_h*IEMGUI_ZOOM(x));
    sys_vgui(".x%lx.c coords %lxBASE %d %d %d %d\n",
             canvas, x, xpos, ypos,
             xpos + x->x_gui.x_w, ypos + x->x_gui.x_h);
    sys_vgui(".x%lx.c coords %lxLABEL %d %d\n",
             canvas, x, xpos+x->x_gui.x_ldx * zoomlabel,
             ypos+x->x_gui.x_ldy * zoomlabel);
}

void my_canvas_draw_erase(t_my_canvas* x, t_glist* glist)
{
    t_canvas *canvas=glist_getcanvas(glist);

    sys_vgui(".x%lx.c delete %lxBASE\n", canvas, x);
    sys_vgui(".x%lx.c delete %lxRECT\n", canvas, x);
    sys_vgui(".x%lx.c delete %lxLABEL\n", canvas, x);
}

void my_canvas_draw_config(t_my_canvas* x, t_glist* glist)
{
    t_canvas *canvas=glist_getcanvas(glist);

    sys_vgui(".x%lx.c itemconfigure %lxRECT -fill #%06x -outline #%06x\n", canvas, x,
             x->x_gui.x_bcol, x->x_gui.x_bcol);
    sys_vgui(".x%lx.c itemconfigure %lxBASE -outline #%06x\n", canvas, x,
             x->x_gui.x_fsf.x_selected?IEM_GUI_COLOR_SELECTED:x->x_gui.x_bcol);
    sys_vgui(".x%lx.c itemconfigure %lxLABEL -font {{%s} -%d %s} -fill #%06x -text {%s} \n",
             canvas, x, x->x_gui.x_font, x->x_gui.x_fontsize, sys_fontweight,
             x->x_gui.x_lcol,
             strcmp(x->x_gui.x_lab->s_name, "empty")?x->x_gui.x_lab->s_name:"");
}

void my_canvas_draw_select(t_my_canvas* x, t_glist* glist)
{
    t_canvas *canvas=glist_getcanvas(glist);

    if(x->x_gui.x_fsf.x_selected)
    {
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline #%06x\n", canvas, x, IEM_GUI_COLOR_SELECTED);
    }
    else
    {
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline #%06x\n", canvas, x, x->x_gui.x_bcol);
    }
}

void my_canvas_draw(t_my_canvas *x, t_glist *glist, int mode)
{
    if(mode == IEM_GUI_DRAW_MODE_MOVE)
        my_canvas_draw_move(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_NEW)
        my_canvas_draw_new(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_SELECT)
        my_canvas_draw_select(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_ERASE)
        my_canvas_draw_erase(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_CONFIG)
        my_canvas_draw_config(x, glist);
}

/* ------------------------ cnv widgetbehaviour----------------------------- */

static void my_canvas_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_my_canvas *x = (t_my_canvas *)z;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist);
    *xp2 = *xp1 + x->x_gui.x_w;
    *yp2 = *yp1 + x->x_gui.x_h;
}

static void my_canvas_save(t_gobj *z, t_binbuf *b)
{
    t_my_canvas *x = (t_my_canvas *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiisssiiiissi", gensym("#X"),gensym("obj"),
                (int)x->x_gui.x_obj.te_xpix, (int)x->x_gui.x_obj.te_ypix,
                gensym("cnv"), x->x_gui.x_w, x->x_vis_w, x->x_vis_h,
                srl[0], srl[1], srl[2], x->x_gui.x_ldx, x->x_gui.x_ldy,
                iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize,
                bflcol[0], bflcol[2], iem_symargstoint(&x->x_gui.x_isa));
    binbuf_addv(b, ";");
}

static void my_canvas_properties(t_gobj *z, t_glist *owner)
{
    t_my_canvas *x = (t_my_canvas *)z;
    char buf[800];
    t_symbol *srl[3];

    iemgui_properties(&x->x_gui, srl);
    sprintf(buf, "pdtk_iemgui_dialog %%s |cnv| \
            ------selectable_dimensions(pix):------ %d %d size: 0.0 0.0 empty \
            ------visible_rectangle(pix)(pix):------ %d width: %d height: %d \
            %d empty empty %d %d empty %d \
            %s %s \
            %s %d %d \
            %d %d \
            #%06x none #%06x\n",
            x->x_gui.x_w, 1,
            x->x_vis_w, x->x_vis_h, 0,/*no_schedule*/
            -1, -1, -1, -1,/*no linlog, no init, no multi*/
            srl[0]->s_name, srl[1]->s_name,
            srl[2]->s_name, x->x_gui.x_ldx, x->x_gui.x_ldy,
            x->x_gui.x_fsf.x_font_style, x->x_gui.x_fontsize,
            0xffffff & x->x_gui.x_bcol, 0xffffff & x->x_gui.x_lcol);
    gfxstub_new(&x->x_gui.x_obj.ob_pd, x, buf);
}

static void my_canvas_get_pos(t_my_canvas *x)
{
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
    {
        x->x_at[0].a_w.w_float = text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist);
        x->x_at[1].a_w.w_float = text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);
        pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, x->x_at);
    }
}

static void my_canvas_dialog(t_my_canvas *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int a = (int)atom_getintarg(0, argc, argv);
    int w = (int)atom_getintarg(2, argc, argv);
    int h = (int)atom_getintarg(3, argc, argv);
    int sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);

    x->x_gui.x_isa.x_loadinit = 0;
    if(a < 1)
        a = 1;
    x->x_gui.x_w = a;
    x->x_gui.x_h = x->x_gui.x_w;
    if(w < 1)
        w = 1;
    x->x_vis_w = w;
    if(h < 1)
        h = 1;
    x->x_vis_h = h;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_CONFIG);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_MOVE);
}

static void my_canvas_size(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{
    int i = (int)atom_getintarg(0, ac, av);

    if(i < 1)
        i = 1;
    x->x_gui.x_w = i;
    x->x_gui.x_h = i;
    iemgui_size((void *)x, &x->x_gui);
}

static void my_canvas_delta(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{iemgui_delta((void *)x, &x->x_gui, s, ac, av);}

static void my_canvas_pos(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{iemgui_pos((void *)x, &x->x_gui, s, ac, av);}

static void my_canvas_vis_size(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{
    int i;

    i = (int)atom_getintarg(0, ac, av);
    if(i < 1)
        i = 1;
    x->x_vis_w = i;
    if(ac > 1)
    {
        i = (int)atom_getintarg(1, ac, av);
        if(i < 1)
            i = 1;
    }
    x->x_vis_h = i;
    if(glist_isvisible(x->x_gui.x_glist))
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_MOVE);
}

static void my_canvas_color(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{iemgui_color((void *)x, &x->x_gui, s, ac, av);}

static void my_canvas_send(t_my_canvas *x, t_symbol *s)
{iemgui_send(x, &x->x_gui, s);}

static void my_canvas_receive(t_my_canvas *x, t_symbol *s)
{iemgui_receive(x, &x->x_gui, s);}

static void my_canvas_label(t_my_canvas *x, t_symbol *s)
{iemgui_label((void *)x, &x->x_gui, s);}

static void my_canvas_label_pos(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_pos((void *)x, &x->x_gui, s, ac, av);}

static void my_canvas_label_font(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_font((void *)x, &x->x_gui, s, ac, av);}

static void *my_canvas_new(t_symbol *s, int argc, t_atom *argv)
{
    t_my_canvas *x = (t_my_canvas *)pd_new(my_canvas_class);
    int a=IEM_GUI_DEFAULTSIZE, w=100, h=60;
    int ldx=20, ldy=12, f=2, i=0;
    int fs=14;
    char str[144];

    iem_inttosymargs(&x->x_gui.x_isa, 0);
    iem_inttofstyle(&x->x_gui.x_fsf, 0);

    x->x_gui.x_bcol = 0xE0E0E0;
    x->x_gui.x_fcol = 0x00;
    x->x_gui.x_lcol = 0x404040;

    if(((argc >= 10)&&(argc <= 13))
       &&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)&&IS_A_FLOAT(argv,2))
    {
        a = (int)atom_getintarg(0, argc, argv);
        w = (int)atom_getintarg(1, argc, argv);
        h = (int)atom_getintarg(2, argc, argv);
    }
    if((argc >= 12)&&(IS_A_SYMBOL(argv,3)||IS_A_FLOAT(argv,3))&&(IS_A_SYMBOL(argv,4)||IS_A_FLOAT(argv,4)))
    {
        i = 2;
        iemgui_new_getnames(&x->x_gui, 3, argv);
    }
    else if((argc == 11)&&(IS_A_SYMBOL(argv,3)||IS_A_FLOAT(argv,3)))
    {
        i = 1;
        iemgui_new_getnames(&x->x_gui, 3, argv);
    }
    else iemgui_new_getnames(&x->x_gui, 3, 0);

    if(((argc >= 10)&&(argc <= 13))
       &&(IS_A_SYMBOL(argv,i+3)||IS_A_FLOAT(argv,i+3))&&IS_A_FLOAT(argv,i+4)
       &&IS_A_FLOAT(argv,i+5)&&IS_A_FLOAT(argv,i+6)
       &&IS_A_FLOAT(argv,i+7))
    {
            /* disastrously, the "label" sits in a different part of the
            message.  So we have to track its location separately (in
            the slot x_labelbindex) and initialize it specially here. */
        iemgui_new_dogetname(&x->x_gui, i+3, argv);
        x->x_gui.x_labelbindex = i+4;
        ldx = (int)atom_getintarg(i+4, argc, argv);
        ldy = (int)atom_getintarg(i+5, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getintarg(i+6, argc, argv));
        fs = (int)atom_getintarg(i+7, argc, argv);
        iemgui_all_loadcolors(&x->x_gui, argv+i+8, 0, argv+i+9);
    }
    if((argc == 13)&&IS_A_FLOAT(argv,i+10))
    {
        iem_inttosymargs(&x->x_gui.x_isa, atom_getintarg(i+10, argc, argv));
    }
    x->x_gui.x_draw = (t_iemfunptr)my_canvas_draw;
    x->x_gui.x_fsf.x_snd_able = 1;
    x->x_gui.x_fsf.x_rcv_able = 1;
    x->x_gui.x_glist = (t_glist *)canvas_getcurrent();
    if (!strcmp(x->x_gui.x_snd->s_name, "empty"))
        x->x_gui.x_fsf.x_snd_able = 0;
    if (!strcmp(x->x_gui.x_rcv->s_name, "empty"))
        x->x_gui.x_fsf.x_rcv_able = 0;
    if(a < 1)
        a = 1;
    x->x_gui.x_w = a;
    x->x_gui.x_h = x->x_gui.x_w;
    if(w < 1)
        w = 1;
    x->x_vis_w = w;
    if(h < 1)
        h = 1;
    x->x_vis_h = h;
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
    x->x_at[0].a_type = A_FLOAT;
    x->x_at[1].a_type = A_FLOAT;
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    return (x);
}

static void my_canvas_ff(t_my_canvas *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    gfxstub_deleteforkey(x);
}

void g_mycanvas_setup(void)
{
    my_canvas_class = class_new(gensym("cnv"), (t_newmethod)my_canvas_new,
                                (t_method)my_canvas_ff, sizeof(t_my_canvas), CLASS_NOINLET, A_GIMME, 0);
    class_addcreator((t_newmethod)my_canvas_new, gensym("my_canvas"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_dialog, gensym("dialog"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_size, gensym("size"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_delta, gensym("delta"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_pos, gensym("pos"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_vis_size, gensym("vis_size"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_color, gensym("color"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_label, gensym("label"), A_DEFSYM, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_label_pos, gensym("label_pos"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_label_font, gensym("label_font"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_get_pos, gensym("get_pos"), 0);
    class_addmethod(my_canvas_class, (t_method)iemgui_zoom, gensym("zoom"),
        A_CANT, 0);

    my_canvas_widgetbehavior.w_getrectfn = my_canvas_getrect;
    my_canvas_widgetbehavior.w_displacefn = iemgui_displace;
    my_canvas_widgetbehavior.w_selectfn = iemgui_select;
    my_canvas_widgetbehavior.w_activatefn = NULL;
    my_canvas_widgetbehavior.w_deletefn = iemgui_delete;
    my_canvas_widgetbehavior.w_visfn = iemgui_vis;
    my_canvas_widgetbehavior.w_clickfn = NULL;
    class_setwidget(my_canvas_class, &my_canvas_widgetbehavior);
    class_sethelpsymbol(my_canvas_class, gensym("my_canvas"));
    class_setsavefn(my_canvas_class, my_canvas_save);
    class_setpropertiesfn(my_canvas_class, my_canvas_properties);
}
