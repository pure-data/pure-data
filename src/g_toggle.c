/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* g_7_guis.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */


#include <string.h>
#include <stdio.h>
#include "m_pd.h"

#include "g_all_guis.h"

/* --------------- tgl     gui-toggle ------------------------- */

t_widgetbehavior toggle_widgetbehavior;
static t_class *toggle_class;

/* widget helper functions */
#define toggle_draw_io 0
void toggle_draw_config(t_toggle* x, t_glist* glist)
{
    const int zoom = IEMGUI_ZOOM(x);
    t_iemgui *iemgui = &x->x_gui;
    t_canvas *canvas = glist_getcanvas(glist);
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int iow = IOWIDTH * zoom, ioh = IEM_GUI_IOHEIGHT * zoom;
    int crossw = 1, w = x->x_gui.x_w / zoom;
    int col = x->x_on ? x->x_gui.x_fcol : x->x_gui.x_bcol;
    char tag[128];
    t_atom fontatoms[3];
    SETSYMBOL(fontatoms+0, gensym(iemgui->x_font));
    SETFLOAT (fontatoms+1, -iemgui->x_fontsize*zoom);
    SETSYMBOL(fontatoms+2, gensym(sys_fontweight));

    if(w >= 30)
        crossw = 2;
    if(w >= 60)
        crossw = 3;
    crossw *= zoom;

    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
        xpos, ypos, xpos + x->x_gui.x_w, ypos + x->x_gui.x_h);
    pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
        "-width", zoom, "-fill", x->x_gui.x_bcol);

    sprintf(tag, "%lxX1", x);
    pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
        xpos + crossw + zoom, ypos + crossw + zoom,
        xpos + x->x_gui.x_w - crossw - zoom, ypos + x->x_gui.x_h - crossw - zoom);
    pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
        "-width", crossw, "-fill", col);

    sprintf(tag, "%lxX2", x);
    pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
        xpos + crossw + zoom, ypos + x->x_gui.x_h - crossw - zoom,
        xpos + x->x_gui.x_w - crossw - zoom, ypos + crossw + zoom);
    pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
        "-width", crossw, "-fill", col);

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crs ii", canvas, "coords", tag,
        xpos + x->x_gui.x_ldx * zoom, ypos + x->x_gui.x_ldy * zoom);
    pdgui_vmess(0, "crs rA rk", canvas, "itemconfigure", tag,
        "-font", 3, fontatoms,
        "-fill", (x->x_gui.x_fsf.x_selected ? IEM_GUI_COLOR_SELECTED : x->x_gui.x_lcol));
    iemgui_dolabel(x, &x->x_gui, x->x_gui.x_lab, 1);
}
void toggle_draw_new(t_toggle *x, t_glist *glist)
{
        /* create the widgets (but don't configure them yet) */
    t_canvas *canvas = glist_getcanvas(glist);
    char tag[128], tag_object[128];
    char*tags[] = {tag_object, tag, "label", "text"};
    sprintf(tag_object, "%lxOBJ", x);

    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle",
        0, 0, 0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxX1", x);
    pdgui_vmess(0, "crr iiii rS", canvas, "create", "line",
        0, 0, 0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxX2", x);
    pdgui_vmess(0, "crr iiii rS", canvas, "create", "line",
        0, 0, 0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text",
        0, 0, "-anchor", "w", "-tags", 4, tags);

    toggle_draw_config(x, glist);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO);
}

void toggle_draw_select(t_toggle* x, t_glist* glist)
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

void toggle_draw_update(t_toggle *x, t_glist *glist)
{
    if(glist_isvisible(glist))
    {
        t_canvas *canvas = glist_getcanvas(glist);
        int col = (x->x_on != 0.0) ? x->x_gui.x_fcol : x->x_gui.x_bcol;
        char tag[128];

        sprintf(tag, "%lxX1", x);
        pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-fill", col);
        sprintf(tag, "%lxX2", x);
        pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-fill", col);
    }
}

/* ------------------------ tgl widgetbehaviour----------------------------- */

static void toggle_getrect(t_gobj *z, t_glist *glist,
                           int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_toggle *x = (t_toggle *)z;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist);
    *xp2 = *xp1 + x->x_gui.x_w;
    *yp2 = *yp1 + x->x_gui.x_h;
}

static void toggle_save(t_gobj *z, t_binbuf *b)
{
    t_toggle *x = (t_toggle *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiisssiiiisssff", gensym("#X"), gensym("obj"),
                (int)x->x_gui.x_obj.te_xpix,
                (int)x->x_gui.x_obj.te_ypix,
                gensym("tgl"), x->x_gui.x_w/IEMGUI_ZOOM(x),
                iem_symargstoint(&x->x_gui.x_isa),
                srl[0], srl[1], srl[2],
                x->x_gui.x_ldx, x->x_gui.x_ldy,
                iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize,
                bflcol[0], bflcol[1], bflcol[2],
                x->x_gui.x_isa.x_loadinit?x->x_on:0.f,
                x->x_nonzero);
    binbuf_addv(b, ";");
}

static void toggle_properties(t_gobj *z, t_glist *owner)
{
    t_toggle *x = (t_toggle *)z;
    iemgui_new_dialog(x, &x->x_gui, "tgl",
                      x->x_gui.x_w/IEMGUI_ZOOM(x), IEM_GUI_MINSIZE,
                      0, 0,
                      x->x_nonzero, 0,
                      1,
                      -1, "", "",
                      1, -1, -1);
}

static void toggle_bang(t_toggle *x)
{
    x->x_on = (x->x_on == 0.0) ? x->x_nonzero : 0.0;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    outlet_float(x->x_gui.x_obj.ob_outlet, x->x_on);
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
        pd_float(x->x_gui.x_snd->s_thing, x->x_on);
}

static void toggle_dialog(t_toggle *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int a = (int)atom_getfloatarg(0, argc, argv);
    t_float nonzero = (t_float)atom_getfloatarg(2, argc, argv);
    int sr_flags;
    t_atom undo[18];
    iemgui_setdialogatoms(&x->x_gui, 18, undo);
    SETFLOAT (undo+1, 0);
    SETFLOAT (undo+2, x->x_nonzero);
    SETFLOAT (undo+3, 0);

    pd_undo_set_objectstate(x->x_gui.x_glist, (t_pd*)x, gensym("dialog"),
                            18, undo,
                            argc, argv);

    if(nonzero == 0.0)
        nonzero = 1.0;
    x->x_nonzero = nonzero;
    if(x->x_on != 0.0)
        x->x_on = x->x_nonzero;
    sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);
    x->x_gui.x_w = iemgui_clip_size(a) * IEMGUI_ZOOM(x);
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_size(x, &x->x_gui);
}

static void toggle_click(t_toggle *x, t_floatarg xpos, t_floatarg ypos, t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{toggle_bang(x);}

static int toggle_newclick(t_gobj *z, struct _glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if(doit)
        toggle_click((t_toggle *)z, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift, 0, (t_floatarg)alt);
    return (1);
}

static void toggle_set(t_toggle *x, t_floatarg f)
{
    int old = (x->x_on != 0);
    x->x_on = f;
    if (f != 0.0 && pd_compatibilitylevel < 46)
        x->x_nonzero = f;
    if ((x->x_on != 0) != old)
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void toggle_float(t_toggle *x, t_floatarg f)
{
    toggle_set(x, f);
    if(x->x_gui.x_fsf.x_put_in2out)
    {
        outlet_float(x->x_gui.x_obj.ob_outlet, x->x_on);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_float(x->x_gui.x_snd->s_thing, x->x_on);
    }
}

static void toggle_fout(t_toggle *x, t_floatarg f)
{
    toggle_set(x, f);
    outlet_float(x->x_gui.x_obj.ob_outlet, x->x_on);
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
        pd_float(x->x_gui.x_snd->s_thing, x->x_on);
}

static void toggle_loadbang(t_toggle *x, t_floatarg action)
{
    if (action == LB_LOAD && x->x_gui.x_isa.x_loadinit)
        toggle_fout(x, (t_float)x->x_on);
}

static void toggle_size(t_toggle *x, t_symbol *s, int ac, t_atom *av)
{
    x->x_gui.x_w = iemgui_clip_size((int)atom_getfloatarg(0, ac, av)) * IEMGUI_ZOOM(x);
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_size((void *)x, &x->x_gui);
}

static void toggle_delta(t_toggle *x, t_symbol *s, int ac, t_atom *av)
{iemgui_delta((void *)x, &x->x_gui, s, ac, av);}

static void toggle_pos(t_toggle *x, t_symbol *s, int ac, t_atom *av)
{iemgui_pos((void *)x, &x->x_gui, s, ac, av);}

static void toggle_color(t_toggle *x, t_symbol *s, int ac, t_atom *av)
{iemgui_color((void *)x, &x->x_gui, s, ac, av);}

static void toggle_send(t_toggle *x, t_symbol *s)
{iemgui_send(x, &x->x_gui, s);}

static void toggle_receive(t_toggle *x, t_symbol *s)
{iemgui_receive(x, &x->x_gui, s);}

static void toggle_label(t_toggle *x, t_symbol *s)
{iemgui_label((void *)x, &x->x_gui, s);}

static void toggle_label_font(t_toggle *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_font((void *)x, &x->x_gui, s, ac, av);}

static void toggle_label_pos(t_toggle *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_pos((void *)x, &x->x_gui, s, ac, av);}

static void toggle_init(t_toggle *x, t_floatarg f)
{
    x->x_gui.x_isa.x_loadinit = (f == 0.0) ? 0 : 1;
}

static void toggle_nonzero(t_toggle *x, t_floatarg f)
{
    if(f != 0.0)
        x->x_nonzero = f;
}

static void *toggle_new(t_symbol *s, int argc, t_atom *argv)
{
    t_toggle *x = (t_toggle *)iemgui_new(toggle_class);
    int a = IEM_GUI_DEFAULTSIZE, f = 0;
    int ldx = 0, ldy = -8 * IEM_GUI_DEFAULTSIZE_SCALE;
    int fs = x->x_gui.x_fontsize;
    t_float on = 0.0, nonzero = 1.0;
    char str[144];

    IEMGUI_SETDRAWFUNCTIONS(x, toggle);

    if(((argc == 13)||(argc == 14))&&IS_A_FLOAT(argv,0)
       &&IS_A_FLOAT(argv,1)
       &&(IS_A_SYMBOL(argv,2)||IS_A_FLOAT(argv,2))
       &&(IS_A_SYMBOL(argv,3)||IS_A_FLOAT(argv,3))
       &&(IS_A_SYMBOL(argv,4)||IS_A_FLOAT(argv,4))
       &&IS_A_FLOAT(argv,5)&&IS_A_FLOAT(argv,6)
       &&IS_A_FLOAT(argv,7)&&IS_A_FLOAT(argv,8)&&IS_A_FLOAT(argv,12))
    {
        a = (int)atom_getfloatarg(0, argc, argv);
        iem_inttosymargs(&x->x_gui.x_isa, atom_getfloatarg(1, argc, argv));
        iemgui_new_getnames(&x->x_gui, 2, argv);
        ldx = (int)atom_getfloatarg(5, argc, argv);
        ldy = (int)atom_getfloatarg(6, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getfloatarg(7, argc, argv));
        fs = (int)atom_getfloatarg(8, argc, argv);
        iemgui_all_loadcolors(&x->x_gui, argv+9, argv+10, argv+11);
        on = (t_float)atom_getfloatarg(12, argc, argv);
    }
    else iemgui_new_getnames(&x->x_gui, 2, 0);
    if((argc == 14)&&IS_A_FLOAT(argv,13))
        nonzero = (t_float)atom_getfloatarg(13, argc, argv);

    x->x_gui.x_fsf.x_snd_able = (0 != x->x_gui.x_snd);
    x->x_gui.x_fsf.x_rcv_able = (0 != x->x_gui.x_rcv);
    if(x->x_gui.x_fsf.x_font_style == 1) strcpy(x->x_gui.x_font, "helvetica");
    else if(x->x_gui.x_fsf.x_font_style == 2) strcpy(x->x_gui.x_font, "times");
    else { x->x_gui.x_fsf.x_font_style = 0;
        strcpy(x->x_gui.x_font, sys_font); }
    x->x_nonzero = (nonzero != 0.0) ? nonzero : 1.0;
    if(x->x_gui.x_isa.x_loadinit)
        x->x_on = (on != 0.0) ? nonzero : 0.0;
    else
        x->x_on = 0.0;
    if (x->x_gui.x_fsf.x_rcv_able)
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;
    x->x_gui.x_fontsize = (fs < 4)?4:fs;
    x->x_gui.x_w = iemgui_clip_size(a);
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    iemgui_newzoom(&x->x_gui);
    outlet_new(&x->x_gui.x_obj, &s_float);
    return (x);
}

static void toggle_free(t_toggle *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    pdgui_stub_deleteforkey(x);
}

void g_toggle_setup(void)
{
    toggle_class = class_new(gensym("tgl"), (t_newmethod)toggle_new,
        (t_method)toggle_free, sizeof(t_toggle), 0, A_GIMME, 0);
    class_addcreator((t_newmethod)toggle_new, gensym("toggle"), A_GIMME, 0);
    class_addbang(toggle_class, toggle_bang);
    class_addfloat(toggle_class, toggle_float);
    class_addmethod(toggle_class, (t_method)toggle_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(toggle_class, (t_method)toggle_dialog, gensym("dialog"),
        A_GIMME, 0);
    class_addmethod(toggle_class, (t_method)toggle_loadbang,
        gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(toggle_class, (t_method)toggle_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(toggle_class, (t_method)toggle_size, gensym("size"), A_GIMME, 0);
    class_addmethod(toggle_class, (t_method)toggle_delta, gensym("delta"), A_GIMME, 0);
    class_addmethod(toggle_class, (t_method)toggle_pos, gensym("pos"), A_GIMME, 0);
    class_addmethod(toggle_class, (t_method)toggle_color, gensym("color"), A_GIMME, 0);
    class_addmethod(toggle_class, (t_method)toggle_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(toggle_class, (t_method)toggle_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(toggle_class, (t_method)toggle_label, gensym("label"), A_DEFSYM, 0);
    class_addmethod(toggle_class, (t_method)toggle_label_pos, gensym("label_pos"), A_GIMME, 0);
    class_addmethod(toggle_class, (t_method)toggle_label_font, gensym("label_font"), A_GIMME, 0);
    class_addmethod(toggle_class, (t_method)toggle_init, gensym("init"), A_FLOAT, 0);
    class_addmethod(toggle_class, (t_method)toggle_nonzero, gensym("nonzero"), A_FLOAT, 0);
    class_addmethod(toggle_class, (t_method)iemgui_zoom, gensym("zoom"),
        A_CANT, 0);
    toggle_widgetbehavior.w_getrectfn = toggle_getrect;
    toggle_widgetbehavior.w_displacefn = iemgui_displace;
    toggle_widgetbehavior.w_selectfn = iemgui_select;
    toggle_widgetbehavior.w_activatefn = NULL;
    toggle_widgetbehavior.w_deletefn = iemgui_delete;
    toggle_widgetbehavior.w_visfn = iemgui_vis;
    toggle_widgetbehavior.w_clickfn = toggle_newclick;
    class_setwidget(toggle_class, &toggle_widgetbehavior);
    class_sethelpsymbol(toggle_class, gensym("toggle"));
    class_setsavefn(toggle_class, toggle_save);
    class_setpropertiesfn(toggle_class, toggle_properties);
}
