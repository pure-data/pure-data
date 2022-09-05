/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* g_7_guis.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */

#include <string.h>
#include <stdio.h>
#include "m_pd.h"

#include "g_all_guis.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/* ---------- cnv  my gui-canvas for a window ---------------- */

t_widgetbehavior my_canvas_widgetbehavior;
static t_class *my_canvas_class;

/* widget helper functions */

#define my_canvas_draw_update 0

static void my_canvas_draw_io(t_my_canvas* x, t_glist* glist, int mode) { ; }
static void my_canvas_draw_config(t_my_canvas* x, t_glist* glist)
{
    const int zoom = IEMGUI_ZOOM(x);
    t_canvas *canvas = glist_getcanvas(glist);
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int offset = (zoom > 1 ? zoom : 0); /* keep zoomed border inside visible area */
    char tag[128];
    t_atom fontatoms[3];
    SETSYMBOL(fontatoms+0, gensym(x->x_gui.x_font));
    SETFLOAT (fontatoms+1, -(x->x_gui.x_fontsize)*zoom);
    SETSYMBOL(fontatoms+2, gensym(sys_fontweight));

    sprintf(tag, "%lxRECT", x);
    pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
        xpos, ypos, xpos + x->x_vis_w * zoom, ypos + x->x_vis_h * zoom);
    pdgui_vmess(0, "crs rk rk", canvas, "itemconfigure", tag,
        "-fill", x->x_gui.x_bcol,
        "-outline", x->x_gui.x_bcol);

    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
        xpos + offset, ypos + offset,
        xpos + offset + x->x_gui.x_w, ypos + offset + x->x_gui.x_h);
    pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
        "-width", zoom,
        "-outline", (x->x_gui.x_fsf.x_selected ? IEM_GUI_COLOR_SELECTED : x->x_gui.x_bcol));

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crs ii", canvas, "coords", tag,
        xpos + x->x_gui.x_ldx * zoom,
        ypos + x->x_gui.x_ldy * zoom);
    pdgui_vmess(0, "crs rA rk", canvas, "itemconfigure", tag,
        "-font", 3, fontatoms,
        "-fill", x->x_gui.x_lcol);
    iemgui_dolabel(x, &x->x_gui, x->x_gui.x_lab, 1);
}

static void my_canvas_draw_new(t_my_canvas *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    char tag_object[128], tag[128];
    char *tags[] = {tag_object, tag, "label", "text"};
    sprintf(tag_object, "%lxOBJ", x);

    sprintf(tag, "%lxRECT", x);
    pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle",
        0, 0, 0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle",
        0, 0, 0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text",
        0, 0, "-anchor", "w", "-tags", 4, tags);

    my_canvas_draw_config(x, glist);
}

static void my_canvas_draw_select(t_my_canvas* x, t_glist* glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    char tag[128];
    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag,
        "-outline", (x->x_gui.x_fsf.x_selected ? IEM_GUI_COLOR_SELECTED : x->x_gui.x_bcol));
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
                gensym("cnv"), x->x_gui.x_w/IEMGUI_ZOOM(x), x->x_vis_w, x->x_vis_h,
                srl[0], srl[1], srl[2], x->x_gui.x_ldx, x->x_gui.x_ldy,
                iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize,
                bflcol[0], bflcol[2], iem_symargstoint(&x->x_gui.x_isa));
    binbuf_addv(b, ";");
}

static void my_canvas_properties(t_gobj *z, t_glist *owner)
{
    t_my_canvas *x = (t_my_canvas *)z;
    iemgui_new_dialog(x, &x->x_gui, "cnv",
                      x->x_gui.x_w/IEMGUI_ZOOM(x), 1,
                      0, 0,
                      x->x_vis_w, x->x_vis_h,
                      0,
                      -1, "", "",
                      0, -1, -1);

}

static void my_canvas_get_pos(t_my_canvas *x)
{
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
    {
        x->x_at[0].a_w.w_float = text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist)/IEMGUI_ZOOM(x);
        x->x_at[1].a_w.w_float = text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist)/IEMGUI_ZOOM(x);
        pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, x->x_at);
    }
}

static void my_canvas_dialog(t_my_canvas *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int a = atom_getfloatarg(0, argc, argv);
    int w = atom_getfloatarg(2, argc, argv);
    int h = atom_getfloatarg(3, argc, argv);
    int sr_flags;
    t_atom undo[18];
    iemgui_setdialogatoms(&x->x_gui, 18, undo);
    SETFLOAT (undo+1, 0);
    SETFLOAT (undo+2, x->x_vis_w);
    SETFLOAT (undo+3, x->x_vis_h);
    SETFLOAT (undo+5, -1);
    SETSYMBOL(undo+15, gensym("none"));

    pd_undo_set_objectstate(x->x_gui.x_glist, (t_pd*)x, gensym("dialog"),
                            18, undo,
                            argc, argv);

    sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);
    x->x_gui.x_isa.x_loadinit = 0;
    if(a < 1)
        a = 1;
    x->x_gui.x_w = a * IEMGUI_ZOOM(x);
    x->x_gui.x_h = x->x_gui.x_w;
    if(w < 1)
        w = 1;
    x->x_vis_w = w;
    if(h < 1)
        h = 1;
    x->x_vis_h = h;
    iemgui_size((void *)x, &x->x_gui);
}

static void my_canvas_size(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{
    int i = atom_getfloatarg(0, ac, av);

    if(i < 1)
        i = 1;
    x->x_gui.x_w = i*IEMGUI_ZOOM(x);
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_size((void *)x, &x->x_gui);
}

static void my_canvas_delta(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{iemgui_delta((void *)x, &x->x_gui, s, ac, av);}

static void my_canvas_pos(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{iemgui_pos((void *)x, &x->x_gui, s, ac, av);}

static void my_canvas_vis_size(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{
    int i;

    i = atom_getfloatarg(0, ac, av);
    if(i < 1)
        i = 1;
    x->x_vis_w = i;
    if(ac > 1)
    {
        i = atom_getfloatarg(1, ac, av);
        if(i < 1)
            i = 1;
    }
    x->x_vis_h = i;
    iemgui_size(x, &x->x_gui);
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
    t_my_canvas *x = (t_my_canvas *)iemgui_new(my_canvas_class);
    int a = IEM_GUI_DEFAULTSIZE;
    int w = 100 * IEM_GUI_DEFAULTSIZE_SCALE, h = 60 * IEM_GUI_DEFAULTSIZE_SCALE;
    int ldx = 20, ldy = 12, f = 2, i = 0;
    int fs = x->x_gui.x_fontsize;

    IEMGUI_SETDRAWFUNCTIONS(x, my_canvas);

    x->x_gui.x_bcol = 0xE0E0E0;
    x->x_gui.x_fcol = 0x00;
    x->x_gui.x_lcol = 0x404040;

    if(((argc >= 10)&&(argc <= 13))
       &&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)&&IS_A_FLOAT(argv,2))
    {
        a = atom_getfloatarg(0, argc, argv);
        w = atom_getfloatarg(1, argc, argv);
        h = atom_getfloatarg(2, argc, argv);
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
        if(IS_A_FLOAT(argv, i+3))
        {
            char str[80];
            atom_string(argv+i+3, str, sizeof(str));
            x->x_gui.x_lab = gensym(str);
        } else {
            x->x_gui.x_lab = iemgui_new_dogetname(&x->x_gui, i+3, argv);
        }
        x->x_gui.x_labelbindex = i+4;
        ldx = atom_getfloatarg(i+4, argc, argv);
        ldy = atom_getfloatarg(i+5, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getfloatarg(i+6, argc, argv));
        fs = atom_getfloatarg(i+7, argc, argv);
        x->x_gui.x_fontsize = fs;
        iemgui_all_loadcolors(&x->x_gui, argv+i+8, 0, argv+i+9);
    }
    if((argc == 13)&&IS_A_FLOAT(argv,i+10))
    {
        iem_inttosymargs(&x->x_gui.x_isa, atom_getfloatarg(i+10, argc, argv));
    }
    x->x_gui.x_fsf.x_snd_able = (0 != x->x_gui.x_snd);
    x->x_gui.x_fsf.x_rcv_able = (0 != x->x_gui.x_rcv);
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
    x->x_gui.x_fontsize = (fs < 4)?4:fs;
    x->x_at[0].a_type = A_FLOAT;
    x->x_at[1].a_type = A_FLOAT;
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    iemgui_newzoom(&x->x_gui);
    return (x);
}

static void my_canvas_free(t_my_canvas *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    pdgui_stub_deleteforkey(x);
}

void g_mycanvas_setup(void)
{
    my_canvas_class = class_new(gensym("cnv"), (t_newmethod)my_canvas_new,
        (t_method)my_canvas_free, sizeof(t_my_canvas), CLASS_NOINLET, A_GIMME, 0);
    class_addcreator((t_newmethod)my_canvas_new, gensym("my_canvas"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_dialog,
        gensym("dialog"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_size,
        gensym("size"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_delta,
        gensym("delta"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_pos,
        gensym("pos"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_vis_size,
        gensym("vis_size"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_color,
        gensym("color"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_send,
        gensym("send"), A_DEFSYM, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_receive,
        gensym("receive"), A_DEFSYM, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_label,
        gensym("label"), A_DEFSYM, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_label_pos,
        gensym("label_pos"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_label_font,
        gensym("label_font"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_get_pos,
        gensym("get_pos"), 0);
    class_addmethod(my_canvas_class, (t_method)iemgui_zoom,
        gensym("zoom"), A_CANT, 0);
    my_canvas_widgetbehavior.w_getrectfn  = my_canvas_getrect;
    my_canvas_widgetbehavior.w_displacefn = iemgui_displace;
    my_canvas_widgetbehavior.w_selectfn   = iemgui_select;
    my_canvas_widgetbehavior.w_activatefn = NULL;
    my_canvas_widgetbehavior.w_deletefn   = iemgui_delete;
    my_canvas_widgetbehavior.w_visfn      = iemgui_vis;
    my_canvas_widgetbehavior.w_clickfn    = NULL;
    class_setwidget(my_canvas_class, &my_canvas_widgetbehavior);
    class_setsavefn(my_canvas_class, my_canvas_save);
    class_setpropertiesfn(my_canvas_class, my_canvas_properties);
}
