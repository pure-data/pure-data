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

#ifdef _MSC_VER
#define snprintf sprintf_s
#endif

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/*  #define GGEE_HSLIDER_COMPATIBLE  */

/*------------------ global varaibles -------------------------*/

int iemgui_color_hex[]=
{
    16579836, 10526880, 4210752, 16572640, 16572608,
    16579784, 14220504, 14220540, 14476540, 16308476,
    14737632, 8158332, 2105376, 16525352, 16559172,
    15263784, 1370132, 2684148, 3952892, 16003312,
    12369084, 6316128, 0, 9177096, 5779456,
    7874580, 2641940, 17488, 5256, 5767248
};

int iemgui_vu_db2i[]=
{
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9,10,10,10,10,10,
    11,11,11,11,11,12,12,12,12,12,
    13,13,13,13,14,14,14,14,15,15,
    15,15,16,16,16,16,17,17,17,18,
    18,18,19,19,19,20,20,20,21,21,
    22,22,23,23,24,24,25,26,27,28,
    29,30,31,32,33,33,34,34,35,35,
    36,36,37,37,37,38,38,38,39,39,
    39,39,39,39,40,40
};

int iemgui_vu_col[]=
{
    0,17,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    15,15,15,15,15,15,15,15,15,15,14,14,13,13,13,13,13,13,13,13,13,13,13,19,19,19
};

char *iemgui_vu_scale_str[]=
{
    "",
    "<-99",
    "",
    "",
    "",
    "-50",
    "",
    "",
    "",
    "-30",
    "",
    "",
    "",
    "-20",
    "",
    "",
    "",
    "-12",
    "",
    "",
    "",
    "-6",
    "",
    "",
    "",
    "-2",
    "",
    "",
    "",
    "-0dB",
    "",
    "",
    "",
    "+2",
    "",
    "",
    "",
    "+6",
    "",
    "",
    "",
    ">+12",
    "",
    "",
    "",
    "",
    "",
};


/*------------------ global functions -------------------------*/


int iemgui_clip_size(int size)
{
    if(size < IEM_GUI_MINSIZE)
        size = IEM_GUI_MINSIZE;
    return(size);
}

int iemgui_clip_font(int size)
{
    if(size < IEM_FONT_MINSIZE)
        size = IEM_FONT_MINSIZE;
    return(size);
}

int iemgui_modulo_color(int col)
{
    while(col >= IEM_GUI_MAX_COLOR)
        col -= IEM_GUI_MAX_COLOR;
    while(col < 0)
        col += IEM_GUI_MAX_COLOR;
    return(col);
}

t_symbol *iemgui_dollar2raute(t_symbol *s)
{
    char buf[MAXPDSTRING+1], *s1, *s2;
    if (strlen(s->s_name) >= MAXPDSTRING)
        return (s);
    for (s1 = s->s_name, s2 = buf; ; s1++, s2++)
    {
        if (*s1 == '$')
            *s2 = '#';
        else if (!(*s2 = *s1))
            break;
    }
    return(gensym(buf));
}

t_symbol *iemgui_raute2dollar(t_symbol *s)
{
    char buf[MAXPDSTRING+1], *s1, *s2;
    if (strlen(s->s_name) >= MAXPDSTRING)
        return (s);
    for (s1 = s->s_name, s2 = buf; ; s1++, s2++)
    {
        if (*s1 == '#')
            *s2 = '$';
        else if (!(*s2 = *s1))
            break;
    }
    return(gensym(buf));
}

void iemgui_verify_snd_ne_rcv(t_iemgui *iemgui)
{
    iemgui->x_fsf.x_put_in2out = 1;
    if(iemgui->x_fsf.x_snd_able && iemgui->x_fsf.x_rcv_able)
    {
        if(!strcmp(iemgui->x_snd->s_name, iemgui->x_rcv->s_name))
            iemgui->x_fsf.x_put_in2out = 0;
    }
}

t_symbol *iemgui_new_dogetname(t_iemgui *iemgui, int indx, t_atom *argv)
{
    if (IS_A_SYMBOL(argv, indx))
        return (atom_getsymbolarg(indx, 100000, argv));
    else if (IS_A_FLOAT(argv, indx))
    {
        char str[80];
        sprintf(str, "%d", (int)atom_getintarg(indx, 100000, argv));
        return (gensym(str));
    }
    else return (gensym("empty"));
}

void iemgui_new_getnames(t_iemgui *iemgui, int indx, t_atom *argv)
{
    if (argv)
    {
        iemgui->x_snd = iemgui_new_dogetname(iemgui, indx, argv);
        iemgui->x_rcv = iemgui_new_dogetname(iemgui, indx+1, argv);
        iemgui->x_lab = iemgui_new_dogetname(iemgui, indx+2, argv);
    }
    else iemgui->x_snd = iemgui->x_rcv = iemgui->x_lab = gensym("empty");
    iemgui->x_snd_unexpanded = iemgui->x_rcv_unexpanded =
        iemgui->x_lab_unexpanded = 0;
    iemgui->x_binbufindex = indx;
    iemgui->x_labelbindex = indx + 3;
}

    /* convert symbols in "$" form to the expanded symbols */
void iemgui_all_dollararg2sym(t_iemgui *iemgui, t_symbol **srlsym)
{
        /* save unexpanded ones for later */
    iemgui->x_snd_unexpanded = srlsym[0];
    iemgui->x_rcv_unexpanded = srlsym[1];
    iemgui->x_lab_unexpanded = srlsym[2];
    srlsym[0] = canvas_realizedollar(iemgui->x_glist, srlsym[0]);
    srlsym[1] = canvas_realizedollar(iemgui->x_glist, srlsym[1]);
    srlsym[2] = canvas_realizedollar(iemgui->x_glist, srlsym[2]);
}

    /* initialize a single symbol in unexpanded form.  We reach into the
    binbuf to grab them; if there's nothing there, set it to the
    fallback; if still nothing, set to "empty". */
static void iemgui_init_sym2dollararg(t_iemgui *iemgui, t_symbol **symp,
    int indx, t_symbol *fallback)
{
    if (!*symp)
    {
        t_binbuf *b = iemgui->x_obj.ob_binbuf;
        if (binbuf_getnatom(b) > indx)
        {
            char buf[80];
            atom_string(binbuf_getvec(b) + indx, buf, 80);
            *symp = gensym(buf);
        }
        else if (fallback)
            *symp = fallback;
        else *symp = gensym("empty");
    }
}

    /* get the unexpanded versions of the symbols; initialize them if
    necessary. */
void iemgui_all_sym2dollararg(t_iemgui *iemgui, t_symbol **srlsym)
{
    iemgui_init_sym2dollararg(iemgui, &iemgui->x_snd_unexpanded,
        iemgui->x_binbufindex+1, iemgui->x_snd);
    iemgui_init_sym2dollararg(iemgui, &iemgui->x_rcv_unexpanded,
        iemgui->x_binbufindex+2, iemgui->x_rcv);
    iemgui_init_sym2dollararg(iemgui, &iemgui->x_lab_unexpanded,
        iemgui->x_labelbindex, iemgui->x_lab);
    srlsym[0] = iemgui->x_snd_unexpanded;
    srlsym[1] = iemgui->x_rcv_unexpanded;
    srlsym[2] = iemgui->x_lab_unexpanded;
}

static t_symbol* color2symbol(int col) {
    const int  compat = (pd_compatibilitylevel < 48)?1:
        /* FIXXME: for Pd>=0.48, the default compatibility mode should be OFF */
        1;

    char colname[MAXPDSTRING];
    colname[0] = colname[MAXPDSTRING-1] = 0;

    if (compat)
    {
            /* compatibility with Pd<=0.47: saves colors as numbers with limited resolution */
        int col2 = -1 - (((0xfc0000 & col) >> 6)|((0xfc00 & col) >> 4)|((0xfc & col) >> 2));
        snprintf(colname, MAXPDSTRING-1, "%d", col2);
    } else {
        snprintf(colname, MAXPDSTRING-1, "#%06x", col);
    }
    return gensym(colname);
}

void iemgui_all_col2save(t_iemgui *iemgui, t_symbol**bflcol)
{
    bflcol[0] = color2symbol(iemgui->x_bcol);
    bflcol[1] = color2symbol(iemgui->x_fcol);
    bflcol[2] = color2symbol(iemgui->x_lcol);
}

static int iemgui_getcolorarg(int index, int argc, t_atom*argv)
{
    if(index < 0 || index >= argc)
        return 0;
    if(IS_A_FLOAT(argv,index))
        return atom_getintarg(index, argc, argv);
    if(IS_A_SYMBOL(argv,index))
    {
        t_symbol*s=atom_getsymbolarg(index, argc, argv);
        if ('#' == s->s_name[0])
            return strtol(s->s_name+1, 0, 16);
    }
    return 0;
}

static int colfromatomload(t_atom*colatom)
{
    int color;
        /* old-fashioned color arguement, either a number or symbol
        evaluating to an integer */
    if (colatom->a_type == A_FLOAT)
        color = atom_getint(colatom);
    else if (colatom->a_type == A_SYMBOL &&
        (isdigit(colatom->a_w.w_symbol->s_name[0]) ||
            colatom->a_w.w_symbol->s_name[0] == '-'))
                color = atoi(colatom->a_w.w_symbol->s_name);

        /* symbolic color */
    else return (iemgui_getcolorarg(0, 1, colatom));
    if (color < 0)
    {
        color = -1 - color;
        color = ((color & 0x3f000) << 6)|((color & 0xfc0) << 4)|
            ((color & 0x3f) << 2);
    }
    else
    {
        color = iemgui_modulo_color(color);
        color = iemgui_color_hex[color];
    }
    return (color);
}

void iemgui_all_loadcolors(t_iemgui *iemgui, t_atom*bcol, t_atom*fcol, t_atom*lcol)
{
    if(bcol)iemgui->x_bcol = colfromatomload(bcol);
    if(fcol)iemgui->x_fcol = colfromatomload(fcol);
    if(lcol)iemgui->x_lcol = colfromatomload(lcol);
}

int iemgui_compatible_colorarg(int index, int argc, t_atom* argv)
{
    if (index < 0 || index >= argc)
        return 0;
    if(IS_A_FLOAT(argv,index))
        {
            int col=atom_getintarg(index, argc, argv);
            if(col >= 0)
            {
                int idx = iemgui_modulo_color(col);
                return(iemgui_color_hex[(idx)]);
            }
            else
               return((-1 -col)&0xffffff);
        }
    return iemgui_getcolorarg(index, argc, argv);
}

void iemgui_all_dollar2raute(t_symbol **srlsym)
{
    srlsym[0] = iemgui_dollar2raute(srlsym[0]);
    srlsym[1] = iemgui_dollar2raute(srlsym[1]);
    srlsym[2] = iemgui_dollar2raute(srlsym[2]);
}

void iemgui_all_raute2dollar(t_symbol **srlsym)
{
    srlsym[0] = iemgui_raute2dollar(srlsym[0]);
    srlsym[1] = iemgui_raute2dollar(srlsym[1]);
    srlsym[2] = iemgui_raute2dollar(srlsym[2]);
}

void iemgui_send(void *x, t_iemgui *iemgui, t_symbol *s)
{
    t_symbol *snd;
    int pargc, tail_len, nth_arg, sndable=1, oldsndrcvable=0;
    t_atom *pargv;

    if(iemgui->x_fsf.x_rcv_able)
        oldsndrcvable += IEM_GUI_OLD_RCV_FLAG;
    if(iemgui->x_fsf.x_snd_able)
        oldsndrcvable += IEM_GUI_OLD_SND_FLAG;

    if(!strcmp(s->s_name, "empty")) sndable = 0;
    snd = iemgui_raute2dollar(s);
    iemgui->x_snd_unexpanded = snd;
    iemgui->x_snd = snd = canvas_realizedollar(iemgui->x_glist, snd);
    iemgui->x_fsf.x_snd_able = sndable;
    iemgui_verify_snd_ne_rcv(iemgui);
    (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_IO + oldsndrcvable);
}

void iemgui_receive(void *x, t_iemgui *iemgui, t_symbol *s)
{
    t_symbol *rcv;
    int pargc, tail_len, nth_arg, rcvable=1, oldsndrcvable=0;
    t_atom *pargv;

    if(iemgui->x_fsf.x_rcv_able)
        oldsndrcvable += IEM_GUI_OLD_RCV_FLAG;
    if(iemgui->x_fsf.x_snd_able)
        oldsndrcvable += IEM_GUI_OLD_SND_FLAG;

    if(!strcmp(s->s_name, "empty")) rcvable = 0;
    rcv = iemgui_raute2dollar(s);
    iemgui->x_rcv_unexpanded = rcv;
    rcv = canvas_realizedollar(iemgui->x_glist, rcv);
    if(rcvable)
    {
        if(strcmp(rcv->s_name, iemgui->x_rcv->s_name))
        {
            if(iemgui->x_fsf.x_rcv_able)
                pd_unbind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);
            iemgui->x_rcv = rcv;
            pd_bind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);
        }
    }
    else if(!rcvable && iemgui->x_fsf.x_rcv_able)
    {
        pd_unbind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);
        iemgui->x_rcv = rcv;
    }
    iemgui->x_fsf.x_rcv_able = rcvable;
    iemgui_verify_snd_ne_rcv(iemgui);
    (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_IO + oldsndrcvable);
}

void iemgui_label(void *x, t_iemgui *iemgui, t_symbol *s)
{
    t_symbol *old;
    int pargc, tail_len, nth_arg;
    t_atom *pargv;

        /* tb: fix for empty label { */
        if (s == gensym(""))
                s = gensym("empty");
        /* tb } */

    old = iemgui->x_lab;
    iemgui->x_lab_unexpanded = iemgui_raute2dollar(s);
    iemgui->x_lab = canvas_realizedollar(iemgui->x_glist, iemgui->x_lab_unexpanded);

    if(glist_isvisible(iemgui->x_glist) && iemgui->x_lab != old)
        sys_vgui(".x%lx.c itemconfigure %lxLABEL -text {%s} \n",
                 glist_getcanvas(iemgui->x_glist), x,
                 strcmp(s->s_name, "empty")?iemgui->x_lab->s_name:"");
}

void iemgui_label_pos(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av)
{
    iemgui->x_ldx = (int)atom_getintarg(0, ac, av);
    iemgui->x_ldy = (int)atom_getintarg(1, ac, av);
    if(glist_isvisible(iemgui->x_glist))
        sys_vgui(".x%lx.c coords %lxLABEL %d %d\n",
                 glist_getcanvas(iemgui->x_glist), x,
                 text_xpix((t_object *)x,iemgui->x_glist)+iemgui->x_ldx,
                 text_ypix((t_object *)x,iemgui->x_glist)+iemgui->x_ldy);
}

void iemgui_label_font(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av)
{
    int f = (int)atom_getintarg(0, ac, av);

    if(f == 1) strcpy(iemgui->x_font, "helvetica");
    else if(f == 2) strcpy(iemgui->x_font, "times");
    else
    {
        f = 0;
        strcpy(iemgui->x_font, sys_font);
    }
    iemgui->x_fsf.x_font_style = f;
    f = (int)atom_getintarg(1, ac, av);
    if(f < 4)
        f = 4;
    iemgui->x_fontsize = f;
    if(glist_isvisible(iemgui->x_glist))
        sys_vgui(".x%lx.c itemconfigure %lxLABEL -font {{%s} -%d %s}\n",
                 glist_getcanvas(iemgui->x_glist), x, iemgui->x_font,
                 iemgui->x_fontsize, sys_fontweight);
}

void iemgui_size(void *x, t_iemgui *iemgui)
{
    if(glist_isvisible(iemgui->x_glist))
    {
        (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_MOVE);
        canvas_fixlinesfor(iemgui->x_glist, (t_text*)x);
    }
}

void iemgui_delta(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av)
{
    iemgui->x_obj.te_xpix += (int)atom_getintarg(0, ac, av);
    iemgui->x_obj.te_ypix += (int)atom_getintarg(1, ac, av);
    if(glist_isvisible(iemgui->x_glist))
    {
        (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_MOVE);
        canvas_fixlinesfor(iemgui->x_glist, (t_text*)x);
    }
}

void iemgui_pos(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av)
{
    iemgui->x_obj.te_xpix = (int)atom_getintarg(0, ac, av);
    iemgui->x_obj.te_ypix = (int)atom_getintarg(1, ac, av);
    if(glist_isvisible(iemgui->x_glist))
    {
        (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_MOVE);
        canvas_fixlinesfor(iemgui->x_glist, (t_text*)x);
    }
}

void iemgui_color(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av)
{
    if (ac >= 1)
        iemgui->x_bcol = iemgui_compatible_colorarg(0, ac, av);
    if (ac == 2 && pd_compatibilitylevel < 47)
            /* old versions of Pd updated foreground and label color
            if only two args; now we do it more coherently. */
        iemgui->x_lcol = iemgui_compatible_colorarg(1, ac, av);
    else if (ac >= 2)
        iemgui->x_fcol = iemgui_compatible_colorarg(1, ac, av);
    if (ac >= 3)
        iemgui->x_lcol = iemgui_compatible_colorarg(2, ac, av);
    if(glist_isvisible(iemgui->x_glist))
        (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_CONFIG);
}

void iemgui_displace(t_gobj *z, t_glist *glist, int dx, int dy)
{
    t_iemgui *x = (t_iemgui *)z;

    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    (*x->x_draw)((void *)z, glist, IEM_GUI_DRAW_MODE_MOVE);
    canvas_fixlinesfor(glist, (t_text *)z);
}

void iemgui_select(t_gobj *z, t_glist *glist, int selected)
{
    t_iemgui *x = (t_iemgui *)z;

    x->x_fsf.x_selected = selected;
    (*x->x_draw)((void *)z, glist, IEM_GUI_DRAW_MODE_SELECT);
}

void iemgui_delete(t_gobj *z, t_glist *glist)
{
    canvas_deletelinesfor(glist, (t_text *)z);
}

void iemgui_vis(t_gobj *z, t_glist *glist, int vis)
{
    t_iemgui *x = (t_iemgui *)z;

    if (vis)
        (*x->x_draw)((void *)z, glist, IEM_GUI_DRAW_MODE_NEW);
    else
    {
        (*x->x_draw)((void *)z, glist, IEM_GUI_DRAW_MODE_ERASE);
        sys_unqueuegui(z);
    }
}

void iemgui_save(t_iemgui *iemgui, t_symbol **srl, t_symbol**bflcol)
{
    srl[0] = iemgui->x_snd;
    srl[1] = iemgui->x_rcv;
    srl[2] = iemgui->x_lab;
    iemgui_all_sym2dollararg(iemgui, srl);
    iemgui_all_col2save(iemgui, bflcol);
}

void iemgui_zoom(t_iemgui *iemgui, t_floatarg zoom)
{
    int oldzoom = iemgui->x_glist->gl_zoom;
    if (oldzoom < 1)
        oldzoom = 1;
    iemgui->x_w = (int)(iemgui->x_w)/oldzoom*(int)zoom;
    iemgui->x_h = (int)(iemgui->x_h)/oldzoom*(int)zoom;
}

void iemgui_properties(t_iemgui *iemgui, t_symbol **srl)
{
    srl[0] = iemgui->x_snd;
    srl[1] = iemgui->x_rcv;
    srl[2] = iemgui->x_lab;
    iemgui_all_sym2dollararg(iemgui, srl);
    iemgui_all_dollar2raute(srl);
}

int iemgui_dialog(t_iemgui *iemgui, t_symbol **srl, int argc, t_atom *argv)
{
    char str[144];
    int init = (int)atom_getintarg(5, argc, argv);
    int ldx = (int)atom_getintarg(10, argc, argv);
    int ldy = (int)atom_getintarg(11, argc, argv);
    int f = (int)atom_getintarg(12, argc, argv);
    int fs = (int)atom_getintarg(13, argc, argv);
    int bcol = (int)iemgui_getcolorarg(14, argc, argv);
    int fcol = (int)iemgui_getcolorarg(15, argc, argv);
    int lcol = (int)iemgui_getcolorarg(16, argc, argv);
    int sndable=1, rcvable=1, oldsndrcvable=0;

    if(iemgui->x_fsf.x_rcv_able)
        oldsndrcvable += IEM_GUI_OLD_RCV_FLAG;
    if(iemgui->x_fsf.x_snd_able)
        oldsndrcvable += IEM_GUI_OLD_SND_FLAG;
    if(IS_A_SYMBOL(argv,7))
        srl[0] = atom_getsymbolarg(7, argc, argv);
    else if(IS_A_FLOAT(argv,7))
    {
        sprintf(str, "%d", (int)atom_getintarg(7, argc, argv));
        srl[0] = gensym(str);
    }
    if(IS_A_SYMBOL(argv,8))
        srl[1] = atom_getsymbolarg(8, argc, argv);
    else if(IS_A_FLOAT(argv,8))
    {
        sprintf(str, "%d", (int)atom_getintarg(8, argc, argv));
        srl[1] = gensym(str);
    }
    if(IS_A_SYMBOL(argv,9))
        srl[2] = atom_getsymbolarg(9, argc, argv);
    else if(IS_A_FLOAT(argv,9))
    {
        sprintf(str, "%d", (int)atom_getintarg(9, argc, argv));
        srl[2] = gensym(str);
    }
    if(init != 0) init = 1;
    iemgui->x_isa.x_loadinit = init;
    if(!strcmp(srl[0]->s_name, "empty")) sndable = 0;
    if(!strcmp(srl[1]->s_name, "empty")) rcvable = 0;
    iemgui_all_raute2dollar(srl);
    iemgui_all_dollararg2sym(iemgui, srl);
    if(rcvable)
    {
        if(strcmp(srl[1]->s_name, iemgui->x_rcv->s_name))
        {
            if(iemgui->x_fsf.x_rcv_able)
                pd_unbind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);
            iemgui->x_rcv = srl[1];
            pd_bind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);
        }
    }
    else if(!rcvable && iemgui->x_fsf.x_rcv_able)
    {
        pd_unbind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);
        iemgui->x_rcv = srl[1];
    }
    iemgui->x_snd = srl[0];
    iemgui->x_fsf.x_snd_able = sndable;
    iemgui->x_fsf.x_rcv_able = rcvable;
    iemgui->x_lcol = lcol & 0xffffff;
    iemgui->x_fcol = fcol & 0xffffff;
    iemgui->x_bcol = bcol & 0xffffff;
    iemgui->x_lab = srl[2];
    iemgui->x_ldx = ldx;
    iemgui->x_ldy = ldy;
    if(f == 1) strcpy(iemgui->x_font, "helvetica");
    else if(f == 2) strcpy(iemgui->x_font, "times");
    else
    {
        f = 0;
        strcpy(iemgui->x_font, sys_font);
    }
    iemgui->x_fsf.x_font_style = f;
    if(fs < 4)
        fs = 4;
    iemgui->x_fontsize = fs;
    iemgui_verify_snd_ne_rcv(iemgui);
    canvas_dirty(iemgui->x_glist, 1);
    return(oldsndrcvable);
}

/* pre-0.46 the flags were 1 for 'loadinit' and 1<<20 for 'scale'.
Starting in 0.46, take either 1<<20 or 1<<1 for 'scale' and save to both
bits (so that old versions can read files we write).  In the future (2015?)
we can stop writing the annoying  1<<20 bit. */
#define LOADINIT 1
#define SCALE 2
#define SCALEBIS (1<<20)

void iem_inttosymargs(t_iem_init_symargs *symargp, int n)
{
    memset(symargp, 0, sizeof(*symargp));
    symargp->x_loadinit = ((n & LOADINIT) != 0);
    symargp->x_scale = ((n & SCALE) || (n & SCALEBIS)) ;
    symargp->x_flashed = 0;
    symargp->x_locked = 0;
}

int iem_symargstoint(t_iem_init_symargs *symargp)
{
    return ((symargp->x_loadinit ? LOADINIT : 0) |
        (symargp->x_scale ? (SCALE | SCALEBIS) : 0));
}

void iem_inttofstyle(t_iem_fstyle_flags *fstylep, int n)
{
    memset(fstylep, 0, sizeof(*fstylep));
    fstylep->x_font_style = (n >> 0);
    fstylep->x_shiftdown = 0;
    fstylep->x_selected = 0;
    fstylep->x_finemoved = 0;
    fstylep->x_put_in2out = 0;
    fstylep->x_change = 0;
    fstylep->x_thick = 0;
    fstylep->x_lin0_log1 = 0;
    fstylep->x_steady = 0;
}

int iem_fstyletoint(t_iem_fstyle_flags *fstylep)
{
    return ((fstylep->x_font_style << 0) & 63);
}

    /* for compatibility with pre-0.47 unofficial IEM GUIS like "knob". */
void iemgui_all_colfromload(t_iemgui *iemgui, int *bflcol)
{
    static int warned;
    if (!warned)
    {
        post("warning:\
external GUI object uses obsolete Pd function iemgui_all_colfromload()");
        warned = 1;
    }
    if(bflcol[0] < 0)
    {
        bflcol[0] = -1 - bflcol[0];
        iemgui->x_bcol = ((bflcol[0] & 0x3f000) << 6)|((bflcol[0] & 0xfc0) << 4)|
            ((bflcol[0] & 0x3f) << 2);
    }
    else
    {
        bflcol[0] = iemgui_modulo_color(bflcol[0]);
        iemgui->x_bcol = iemgui_color_hex[bflcol[0]];
    }
    if(bflcol[1] < 0)
    {
        bflcol[1] = -1 - bflcol[1];
        iemgui->x_fcol = ((bflcol[1] & 0x3f000) << 6)|((bflcol[1] & 0xfc0) << 4)|
            ((bflcol[1] & 0x3f) << 2);
    }
    else
    {
        bflcol[1] = iemgui_modulo_color(bflcol[1]);
        iemgui->x_fcol = iemgui_color_hex[bflcol[1]];
    }
    if(bflcol[2] < 0)
    {
        bflcol[2] = -1 - bflcol[2];
        iemgui->x_lcol = ((bflcol[2] & 0x3f000) << 6)|((bflcol[2] & 0xfc0) << 4)|
            ((bflcol[2] & 0x3f) << 2);
    }
    else
    {
        bflcol[2] = iemgui_modulo_color(bflcol[2]);
        iemgui->x_lcol = iemgui_color_hex[bflcol[2]];
    }
}

