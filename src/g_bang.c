/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* g_7_guis.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */

#include <string.h>
#include <stdio.h>
#include "m_pd.h"

#include "g_all_guis.h"

/* --------------- bng     gui-bang ------------------------- */

t_widgetbehavior bng_widgetbehavior;
static t_class *bng_class;

/*  widget helper functions  */
#define bng_draw_io 0
static void bng_draw_config(t_bng* x, t_glist* glist)
{
    const int zoom = IEMGUI_ZOOM(x);
    t_iemgui *iemgui = &x->x_gui;
    t_canvas *canvas = glist_getcanvas(glist);
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int iow = IOWIDTH * zoom, ioh = IEM_GUI_IOHEIGHT * zoom;
    int inset = zoom;
    char tag[128];
    t_atom fontatoms[3];
    SETSYMBOL(fontatoms+0, gensym(iemgui->x_font));
    SETFLOAT (fontatoms+1, -iemgui->x_fontsize*zoom);
    SETSYMBOL(fontatoms+2, gensym(sys_fontweight));

    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
        xpos, ypos, xpos + x->x_gui.x_w, ypos + x->x_gui.x_h);
    pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
        "-width", zoom, "-fill", x->x_gui.x_bcol);

    sprintf(tag, "%lxBUT", x);
    pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
        xpos + inset, ypos + inset,
        xpos + x->x_gui.x_w - inset, ypos + x->x_gui.x_h - inset);
    pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
        "-width", zoom, "-fill", (x->x_flashed ? x->x_gui.x_fcol : x->x_gui.x_bcol));

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crs ii", canvas, "coords", tag,
        xpos + x->x_gui.x_ldx * zoom, ypos + x->x_gui.x_ldy * zoom);
    pdgui_vmess(0, "crs rA rk", canvas, "itemconfigure", tag,
        "-font", 3, fontatoms,
        "-fill", (x->x_gui.x_fsf.x_selected ? IEM_GUI_COLOR_SELECTED : x->x_gui.x_lcol));
    iemgui_dolabel(x, &x->x_gui, x->x_gui.x_lab, 1);
}

static void bng_draw_new(t_bng *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    char tag[128], tag_object[128];
    char*tags[] = {tag_object, tag, "label", "text"};
    sprintf(tag_object, "%lxOBJ", x);

    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle",
        0, 0, 0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxBUT", x);
    pdgui_vmess(0, "crr iiii rS", canvas, "create", "oval",
        0, 0, 0, 0, "-tags", 2, tags);

    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text",
        0, 0, "-anchor", "w", "-tags", 4, tags);

    bng_draw_config(x, glist);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO);
}

static void bng_draw_select(t_bng* x, t_glist* glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    int col = IEM_GUI_COLOR_NORMAL, lcol = x->x_gui.x_lcol;
    char tag[128];

    if(x->x_gui.x_fsf.x_selected)
        col = lcol = IEM_GUI_COLOR_SELECTED;

    sprintf(tag, "%lxBASE", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-outline", col);
    sprintf(tag, "%lxBUT", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-outline", col);
    sprintf(tag, "%lxLABEL", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-fill", lcol);
}

static void bng_draw_update(t_bng *x, t_glist *glist)
{
    if(glist_isvisible(glist))
    {
        char tag[128];
        sprintf(tag, "%lxBUT", x);
        pdgui_vmess(0, "crs rk", glist_getcanvas(glist), "itemconfigure", tag,
            "-fill", (x->x_flashed ? x->x_gui.x_fcol : x->x_gui.x_bcol));
    }
}

/* ------------------------ bng widgetbehaviour----------------------------- */

static void bng_getrect(t_gobj *z, t_glist *glist,
                        int *xp1, int *yp1, int *xp2, int *yp2)
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
                gensym("bng"), x->x_gui.x_w/IEMGUI_ZOOM(x),
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
    iemgui_new_dialog(x, &x->x_gui, "bang",
                      x->x_gui.x_w/IEMGUI_ZOOM(x), IEM_GUI_MINSIZE,
                      0, 0,
                      x->x_flashtime_break, x->x_flashtime_hold,
                      2,
                      -1, "", "",
                      1, -1, -1);
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

static void bng_bout1(t_bng *x) /* wird nur mehr gesendet, wenn snd != rcv*/
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

static void bng_bout2(t_bng *x) /* wird immer gesendet, wenn moeglich*/
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

static void bng_bang(t_bng *x) /* wird nur mehr gesendet, wenn snd != rcv*/
{
    if(!x->x_gui.x_isa.x_locked)
    {
        bng_set(x);
        bng_bout1(x);
    }
}

static void bng_bang2(t_bng *x) /* wird immer gesendet, wenn moeglich*/
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
    int a = (int)atom_getfloatarg(0, argc, argv);
    int fthold = (int)atom_getfloatarg(2, argc, argv);
    int ftbreak = (int)atom_getfloatarg(3, argc, argv);
    int sr_flags;

    t_atom undo[18];
    iemgui_setdialogatoms(&x->x_gui, 18, undo);
    SETFLOAT (undo+1, 0);
    SETFLOAT (undo+2, x->x_flashtime_break);
    SETFLOAT (undo+3, x->x_flashtime_hold);
    pd_undo_set_objectstate(x->x_gui.x_glist, (t_pd*)x, gensym("dialog"),
                            18, undo,
                            argc, argv);

    sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);
    x->x_gui.x_w = iemgui_clip_size(a) * IEMGUI_ZOOM(x);
    x->x_gui.x_h = x->x_gui.x_w;
    bng_check_minmax(x, ftbreak, fthold);

    iemgui_size((void *)x, &x->x_gui);
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
{bng_bang2(x);}

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
    x->x_gui.x_w = iemgui_clip_size((int)atom_getfloatarg(0, ac, av)) * IEMGUI_ZOOM(x);
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_size((void *)x, &x->x_gui);
}

static void bng_delta(t_bng *x, t_symbol *s, int ac, t_atom *av)
{iemgui_delta((void *)x, &x->x_gui, s, ac, av);}

static void bng_pos(t_bng *x, t_symbol *s, int ac, t_atom *av)
{iemgui_pos((void *)x, &x->x_gui, s, ac, av);}

static void bng_flashtime(t_bng *x, t_symbol *s, int ac, t_atom *av)
{
    bng_check_minmax(x, (int)atom_getfloatarg(0, ac, av),
                        (int)atom_getfloatarg(1, ac, av));
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
    x->x_gui.x_isa.x_loadinit = (f == 0.0) ? 0 : 1;
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
    t_bng *x = (t_bng *)iemgui_new(bng_class);
    int a = IEM_GUI_DEFAULTSIZE;
    int ldx = 0, ldy = -8 * IEM_GUI_DEFAULTSIZE_SCALE;
    int fs = x->x_gui.x_fontsize;
    int ftbreak = IEM_BNG_DEFAULTBREAKFLASHTIME,
        fthold = IEM_BNG_DEFAULTHOLDFLASHTIME;

    IEMGUI_SETDRAWFUNCTIONS(x, bng);

    if((argc == 14)&&IS_A_FLOAT(argv,0)
       &&IS_A_FLOAT(argv,1)&&IS_A_FLOAT(argv,2)
       &&IS_A_FLOAT(argv,3)
       &&(IS_A_SYMBOL(argv,4)||IS_A_FLOAT(argv,4))
       &&(IS_A_SYMBOL(argv,5)||IS_A_FLOAT(argv,5))
       &&(IS_A_SYMBOL(argv,6)||IS_A_FLOAT(argv,6))
       &&IS_A_FLOAT(argv,7)&&IS_A_FLOAT(argv,8)
       &&IS_A_FLOAT(argv,9)&&IS_A_FLOAT(argv,10))
    {
        a = (int)atom_getfloatarg(0, argc, argv);
        fthold = (int)atom_getfloatarg(1, argc, argv);
        ftbreak = (int)atom_getfloatarg(2, argc, argv);
        iem_inttosymargs(&x->x_gui.x_isa, atom_getfloatarg(3, argc, argv));
        iemgui_new_getnames(&x->x_gui, 4, argv);
        ldx = (int)atom_getfloatarg(7, argc, argv);
        ldy = (int)atom_getfloatarg(8, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getfloatarg(9, argc, argv));
        fs = (int)atom_getfloatarg(10, argc, argv);
        iemgui_all_loadcolors(&x->x_gui, argv+11, argv+12, argv+13);
    }
    else iemgui_new_getnames(&x->x_gui, 4, 0);

    x->x_gui.x_fsf.x_snd_able = (0 != x->x_gui.x_snd);
    x->x_gui.x_fsf.x_rcv_able = (0 != x->x_gui.x_rcv);
    x->x_flashed = 0;
    if(x->x_gui.x_fsf.x_font_style == 1) strcpy(x->x_gui.x_font, "helvetica");
    else if(x->x_gui.x_fsf.x_font_style == 2) strcpy(x->x_gui.x_font, "times");
    else { x->x_gui.x_fsf.x_font_style = 0;
        strcpy(x->x_gui.x_font, sys_font); }

    if (x->x_gui.x_fsf.x_rcv_able)
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;

    x->x_gui.x_fontsize = (fs < 4)?4:fs;
    x->x_gui.x_w = iemgui_clip_size(a);
    x->x_gui.x_h = x->x_gui.x_w;
    bng_check_minmax(x, ftbreak, fthold);
    x->x_gui.x_isa.x_locked = 0;
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    x->x_lastflashtime = clock_getsystime();
    x->x_clock_hld = clock_new(x, (t_method)bng_tick_hld);
    x->x_clock_lck = clock_new(x, (t_method)bng_tick_lck);
    iemgui_newzoom(&x->x_gui);
    outlet_new(&x->x_gui.x_obj, &s_bang);
    return (x);
}

static void bng_free(t_bng *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    clock_free(x->x_clock_lck);
    clock_free(x->x_clock_hld);
    pdgui_stub_deleteforkey(x);
}

void g_bang_setup(void)
{
    bng_class = class_new(gensym("bng"), (t_newmethod)bng_new,
        (t_method)bng_free, sizeof(t_bng), 0, A_GIMME, 0);
    class_addbang(bng_class, bng_bang);
    class_addfloat(bng_class, bng_float);
    class_addsymbol(bng_class, bng_symbol);
    class_addpointer(bng_class, bng_pointer);
    class_addlist(bng_class, bng_list);
    class_addanything(bng_class, bng_anything);
    class_addmethod(bng_class, (t_method)bng_click,
        gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(bng_class, (t_method)bng_dialog,
        gensym("dialog"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_loadbang,
        gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(bng_class, (t_method)bng_size,
        gensym("size"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_delta,
        gensym("delta"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_pos,
        gensym("pos"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_flashtime,
        gensym("flashtime"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_color,
        gensym("color"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_send,
        gensym("send"), A_DEFSYM, 0);
    class_addmethod(bng_class, (t_method)bng_receive,
        gensym("receive"), A_DEFSYM, 0);
    class_addmethod(bng_class, (t_method)bng_label,
        gensym("label"), A_DEFSYM, 0);
    class_addmethod(bng_class, (t_method)bng_label_pos,
        gensym("label_pos"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_label_font,
        gensym("label_font"), A_GIMME, 0);
    class_addmethod(bng_class, (t_method)bng_init,
        gensym("init"), A_FLOAT, 0);
    class_addmethod(bng_class, (t_method)iemgui_zoom,
        gensym("zoom"), A_CANT, 0);
    bng_widgetbehavior.w_getrectfn = bng_getrect;
    bng_widgetbehavior.w_displacefn = iemgui_displace;
    bng_widgetbehavior.w_selectfn = iemgui_select;
    bng_widgetbehavior.w_activatefn = NULL;
    bng_widgetbehavior.w_deletefn = iemgui_delete;
    bng_widgetbehavior.w_visfn = iemgui_vis;
    bng_widgetbehavior.w_clickfn = bng_newclick;
    class_setwidget(bng_class, &bng_widgetbehavior);
    class_setsavefn(bng_class, bng_save);
    class_setpropertiesfn(bng_class, bng_properties);
}
