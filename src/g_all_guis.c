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
#include "s_stuff.h"

#include "g_all_guis.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif


typedef struct _iemgui_private {
    int p_prevX, p_prevY;
    t_iemgui_drawfunctions p_widget;
} t_iemgui_private;

/*  #define GGEE_HSLIDER_COMPATIBLE  */

/* helpers */
static int srl_is_valid(const t_symbol* s)
{
    return (!!s && s != &s_);
}


/*------------------ global variables -------------------------*/

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
    const char *s1;
    char buf[MAXPDSTRING+1], *s2;
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
    const char *s1;
    char buf[MAXPDSTRING+1], *s2;
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
    if (IS_A_SYMBOL(argv, indx)) {
        t_symbol*name=atom_getsymbolarg(indx, 100000, argv);
        if(gensym("empty") == name)
            return 0;
        return name;
    }
    else if (IS_A_FLOAT(argv, indx))
    {
        char str[80];
        sprintf(str, "%d", (int)atom_getfloatarg(indx, 100000, argv));
        return (gensym(str));
    }
    else return 0;
}

void iemgui_new_getnames(t_iemgui *iemgui, int indx, t_atom *argv)
{
    if (argv)
    {
        iemgui->x_snd = iemgui_new_dogetname(iemgui, indx, argv);
        iemgui->x_rcv = iemgui_new_dogetname(iemgui, indx+1, argv);
        if(IS_A_FLOAT(argv, indx+2))
        {
            char str[80];
            atom_string(argv+indx+2, str, sizeof(str));
            iemgui->x_lab = gensym(str);
        } else {
            iemgui->x_lab = iemgui_new_dogetname(iemgui, indx+2, argv);
        }
    }
    else iemgui->x_snd = iemgui->x_rcv = iemgui->x_lab = 0;
    /* in the object's constructor, we can't access the raw values yet: */
    iemgui->x_snd_unexpanded = iemgui->x_rcv_unexpanded = iemgui->x_lab_unexpanded = 0;
    iemgui->x_binbufindex = indx;
    iemgui->x_labelbindex = indx + 3;
}

    /* initialize a single symbol in unexpanded form.  We reach into the
    binbuf to grab them; if there's nothing there, set it to the
    fallback; if still nothing, set to NULL. */
static void iemgui_init_sym2dollararg(t_iemgui *iemgui, t_symbol **symp,
    int indx, t_symbol *fallback)
{
    t_binbuf *b = iemgui->x_obj.ob_binbuf;
    if ((!*symp) && (binbuf_getnatom(b) > indx))
    {
        t_atom *a = binbuf_getvec(b) + indx;
        char astring[80];
        const char *buf = astring;
        if(A_SYMBOL == a->a_type)
        {
            buf = atom_getsymbol(a)->s_name;
        } else {
            atom_string(a, astring, sizeof(astring));
        }
        if(strcmp(buf, "empty"))
            *symp = gensym(buf);
    }
    if (!*symp)
        *symp = fallback;
}

    /* get the unexpanded versions of the symbols;
       initialize them if necessary. */
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

    /* helper for iemgui_all_dollararg2sym */
static t_symbol*do_all_dollarg2sym(t_iemgui*iemgui, t_symbol**s, size_t index)
{
    t_symbol*org = s[index];
    if(org) {
        s[index] = canvas_realizedollar(iemgui->x_glist, org);
    }
    return org;
}
    /* convert symbols in "$" form to the expanded symbols */
void iemgui_all_dollararg2sym(t_iemgui *iemgui, t_symbol **srlsym)
{
        /* save unexpanded ones for later */
    iemgui->x_snd_unexpanded = do_all_dollarg2sym(iemgui, srlsym, 0);
    iemgui->x_rcv_unexpanded = do_all_dollarg2sym(iemgui, srlsym, 1);
    iemgui->x_lab_unexpanded = do_all_dollarg2sym(iemgui, srlsym, 2);
}



static t_symbol* color2symbol(int col) {
    const int  compat = (pd_compatibilitylevel < 48) ? 1 : 0;

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

static void iemgui_all_col2save(t_iemgui *iemgui, t_symbol**bflcol)
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
        return atom_getfloatarg(index, argc, argv);
    if(IS_A_SYMBOL(argv,index))
    {
        t_symbol*s=atom_getsymbolarg(index, argc, argv);
        if ('#' == s->s_name[0])
        {
            int col = (int)strtol(s->s_name+1, 0, 16);
            return col & 0xFFFFFF;
        }
    }
    return 0;
}

static int colfromatomload(t_atom*colatom)
{
    int color;
        /* old-fashioned color argument, either a number or symbol
        evaluating to an integer */
    if (colatom->a_type == A_FLOAT)
        color = atom_getfloat(colatom);
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
            int col=atom_getfloatarg(index, argc, argv);
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

void iemgui_send(void *x, t_iemgui *iemgui, t_symbol *s)
{
    int sndable=1, oldsndrcvable=0;

    if(iemgui->x_fsf.x_rcv_able)
        oldsndrcvable |= IEM_GUI_OLD_RCV_FLAG;
    if(iemgui->x_fsf.x_snd_able)
        oldsndrcvable |= IEM_GUI_OLD_SND_FLAG;

    if(s) {
        iemgui->x_snd_unexpanded = s;
        iemgui->x_snd = canvas_realizedollar(iemgui->x_glist, s);
    } else {
        iemgui->x_snd_unexpanded = &s_;
        iemgui->x_snd = 0;
        sndable = 0;
    }
    iemgui->x_fsf.x_snd_able = sndable;
    iemgui_verify_snd_ne_rcv(iemgui);
    if(glist_isvisible(iemgui->x_glist) && gobj_shouldvis((t_gobj *)x, iemgui->x_glist))
        (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_IO + oldsndrcvable);
}

void iemgui_receive(void *x, t_iemgui *iemgui, t_symbol *s)
{
    int oldsndrcvable=0;

    if(iemgui->x_fsf.x_rcv_able)
        oldsndrcvable |= IEM_GUI_OLD_RCV_FLAG;
    if(iemgui->x_fsf.x_snd_able)
        oldsndrcvable |= IEM_GUI_OLD_SND_FLAG;

    if(s) {
        iemgui->x_rcv_unexpanded = s;
        s = canvas_realizedollar(iemgui->x_glist, s);
    } else {
        iemgui->x_rcv_unexpanded = &s_;
    }
    if(s)
    {
        if(strcmp(s->s_name, iemgui->x_rcv->s_name))
        {
            if(iemgui->x_fsf.x_rcv_able)
                pd_unbind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);
            iemgui->x_rcv = s;
            pd_bind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);
        }
    }
    else if(iemgui->x_fsf.x_rcv_able)
    {
        pd_unbind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);
        iemgui->x_rcv = s;
    }
    iemgui->x_fsf.x_rcv_able = (s!=0);
    iemgui_verify_snd_ne_rcv(iemgui);
    if(glist_isvisible(iemgui->x_glist) && gobj_shouldvis((t_gobj *)x, iemgui->x_glist))
        (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_IO + oldsndrcvable);
}

static void iemgui_dolabelpos(t_object*obj, t_iemgui*iemgui) {
    int zoom = glist_getzoom(iemgui->x_glist);
    int x0 = text_xpix((t_object *)obj, iemgui->x_glist);
    int y0 = text_ypix((t_object *)obj, iemgui->x_glist);
    int dx = iemgui->x_ldx, dy = iemgui->x_ldy;
    char tag[128];
    sprintf(tag, "%lxLABEL", obj);
    if(gensym("") == iemgui->x_lab) {
        /* put empty labels where they don't create scrollbars */
        dx = 0;
        dy = 7;
    }
    pdgui_vmess(0, "crs ii",
        glist_getcanvas(iemgui->x_glist), "coords", tag,
        x0  + dx*zoom, y0 + dy*zoom);
}
void iemgui_dolabel(void *x, t_iemgui *iemgui, t_symbol *s, int senditup)
{
    t_symbol *empty = gensym("");
    t_symbol *old = iemgui->x_lab;
    s = s?canvas_realizedollar(iemgui->x_glist, s):0;
    if (!(s && s->s_name && s->s_name[0] && strcmp(s->s_name, "empty")))
        s = empty;
    iemgui->x_lab = s;

    if(senditup < 0) {
        senditup = (glist_isvisible(iemgui->x_glist) && iemgui->x_lab != old);
    }

    if(senditup)
    {
        const char*label = s->s_name;
        int have_label = (s != empty);
        char tag[128];
        sprintf(tag, "%lxLABEL", x);
        pdgui_vmess("pdtk_text_set", "cs s",
            glist_getcanvas(iemgui->x_glist), tag,
            have_label?s->s_name:"");
        iemgui_dolabelpos(x, iemgui);
    }
}
void iemgui_label(void *x, t_iemgui *iemgui, t_symbol *s)
{
    iemgui->x_lab_unexpanded = s;
    iemgui_dolabel(x, iemgui, s, -1);
}


void iemgui_label_pos(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av)
{
    iemgui->x_ldx = (int)atom_getfloatarg(0, ac, av);
    iemgui->x_ldy = (int)atom_getfloatarg(1, ac, av);
    if(glist_isvisible(iemgui->x_glist))
        iemgui_dolabelpos(x, iemgui);
}

void iemgui_label_font(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av)
{
    int zoom = glist_getzoom(iemgui->x_glist);
    int f = (int)atom_getfloatarg(0, ac, av);

    if(f == 1) strcpy(iemgui->x_font, "helvetica");
    else if(f == 2) strcpy(iemgui->x_font, "times");
    else
    {
        f = 0;
        strcpy(iemgui->x_font, sys_font);
    }
    iemgui->x_fsf.x_font_style = f;
    f = (int)atom_getfloatarg(1, ac, av);
    if(f < 4)
        f = 4;
    iemgui->x_fontsize = f;
    if(glist_isvisible(iemgui->x_glist))
    {
        char tag[128];
        t_atom fontatoms[3];
        sprintf(tag, "%lxLABEL", x);
        SETSYMBOL(fontatoms+0, gensym(iemgui->x_font));
        SETFLOAT (fontatoms+1, -iemgui->x_fontsize*zoom);
        SETSYMBOL(fontatoms+2, gensym(sys_fontweight));
        pdgui_vmess(0, "crs rA",
            glist_getcanvas(iemgui->x_glist), "itemconfigure", tag,
            "-font", 3, fontatoms);
    }
}

static void iemgui_do_drawmove(void *x, t_iemgui*iemgui)
{
    if(glist_isvisible(iemgui->x_glist))
    {
        int xpos = text_xpix(&iemgui->x_obj, iemgui->x_glist);
        int ypos = text_ypix(&iemgui->x_obj, iemgui->x_glist);
        (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_MOVE);
        iemgui->x_private->p_prevX = xpos;
        iemgui->x_private->p_prevY = ypos;
        canvas_fixlinesfor(iemgui->x_glist, (t_text*)x);
    }
}

void iemgui_size(void *x, t_iemgui *iemgui)
{
    if(glist_isvisible(iemgui->x_glist))
    {
        (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_CONFIG);
        (*iemgui->x_draw)(x, iemgui->x_glist, IEM_GUI_DRAW_MODE_IO);
        canvas_fixlinesfor(iemgui->x_glist, (t_text*)x);
    }
}

void iemgui_delta(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av)
{
    int zoom = glist_getzoom(iemgui->x_glist);
    iemgui->x_obj.te_xpix += (int)atom_getfloatarg(0, ac, av);
    iemgui->x_obj.te_ypix += (int)atom_getfloatarg(1, ac, av);
    iemgui_do_drawmove(x, iemgui);
}

void iemgui_pos(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av)
{
    int zoom = glist_getzoom(iemgui->x_glist);
    iemgui->x_obj.te_xpix = (int)atom_getfloatarg(0, ac, av);
    iemgui->x_obj.te_ypix = (int)atom_getfloatarg(1, ac, av);
    iemgui_do_drawmove(x, iemgui);
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
    iemgui_do_drawmove(x, x);
}

void iemgui_select(t_gobj *z, t_glist *glist, int selected)
{
    t_iemgui *x = (t_iemgui *)z;

    x->x_fsf.x_selected = selected;
    if(glist_isvisible(x->x_glist))
        (*x->x_draw)((void *)z, glist, IEM_GUI_DRAW_MODE_SELECT);
}

void iemgui_delete(t_gobj *z, t_glist *glist)
{
    canvas_deletelinesfor(glist, (t_text *)z);
}

void iemgui_vis(t_gobj *z, t_glist *glist, int vis)
{
    t_iemgui *x = (t_iemgui *)z;

    (*x->x_draw)((void *)z, glist, IEM_GUI_DRAW_MODE_ERASE);
    if (vis)
        (*x->x_draw)((void *)z, glist, IEM_GUI_DRAW_MODE_NEW);
    else
    {
        sys_unqueuegui(z);
    }
    x->x_private->p_prevX = text_xpix(&x->x_obj, glist);
    x->x_private->p_prevY = text_ypix(&x->x_obj, glist);
}

/* store saveable symbols (with spaces and dollars escaped) into srl[3] */
void iemgui_save(t_iemgui *iemgui, t_symbol **srl, t_symbol**bflcol)
{
    int i;
    srl[0] = iemgui->x_snd;
    srl[1] = iemgui->x_rcv;
    srl[2] = iemgui->x_lab;
    iemgui_all_sym2dollararg(iemgui, srl);
    for(i=0; i<3; i++)
    {
        if(!srl[i] || !srl[i]->s_name || !srl[i]->s_name[0])
            srl[i]=gensym("empty");
    }
    iemgui_all_col2save(iemgui, bflcol);
}

    /* inform GUIs that glist's zoom is about to change.  The glist will
    take care of x,y locations but we have to adjust width and height */
void iemgui_zoom(t_iemgui *iemgui, t_floatarg zoom)
{
    int oldzoom = iemgui->x_glist->gl_zoom;
    if (oldzoom < 1)
        oldzoom = 1;
    iemgui->x_w = (int)(iemgui->x_w)/oldzoom*(int)zoom;
    iemgui->x_h = (int)(iemgui->x_h)/oldzoom*(int)zoom;
}

    /* when creating a new GUI from menu onto a zoomed canvas, pretend to
    change the canvas's zoom so we'll get properly sized */
void iemgui_newzoom(t_iemgui *iemgui)
{
    if (iemgui->x_glist->gl_zoom != 1)
    {
        int newzoom = iemgui->x_glist->gl_zoom;
        iemgui->x_glist->gl_zoom = 1;
        iemgui_zoom(iemgui, (t_floatarg)newzoom);
        iemgui->x_glist->gl_zoom = newzoom;
    }
}

void iemgui_properties(t_iemgui *iemgui, t_symbol **srl)
{
    char label[MAXPDSTRING];
    int i;
    srl[0] = iemgui->x_snd;
    srl[1] = iemgui->x_rcv;
    srl[2] = iemgui->x_lab;

    iemgui_all_sym2dollararg(iemgui, srl);

    for(i=0; i<3; i++) {
        if(srl[i])
            srl[i] = gensym(pdgui_strnescape(label, sizeof(label), srl[i]->s_name, strlen(srl[i]->s_name)));
    }
}

void iemgui_new_dialog(void*x, t_iemgui*iemgui,
                       const char*objname,
                       t_float width,  t_float width_min,
                       t_float height, t_float height_min,
                       t_float range_min, t_float range_max,
                       int schedule,
                       int mode, /* lin0_log1 */
                       const char* label_mode0,
                       const char* label_mode1,
                       int canloadbang, int steady, int number)
{
    char objname_[MAXPDSTRING];
    t_symbol *srl[3];
    iemgui_properties(iemgui, srl);
    sprintf(objname_, "|%s|", objname);

    pdgui_stub_vnew(&iemgui->x_obj.ob_pd, "pdtk_iemgui_dialog", x,
        "r s ffs ffs sfsfs i iss ii si sss ii ii kkk",
        objname_,
        "",
        width, width_min, "",
        height, height_min, "",
        "", range_min, "", range_max, "",
        schedule,
        mode, label_mode0, label_mode1,
        canloadbang?iemgui->x_isa.x_loadinit:-1, steady,
        "", number,
        srl[0]?srl[0]->s_name:"", srl[1]?srl[1]->s_name:"", srl[2]?srl[2]->s_name:"",
        iemgui->x_ldx, iemgui->x_ldy,
        iemgui->x_fsf.x_font_style, iemgui->x_fontsize,
        iemgui->x_bcol, iemgui->x_fcol, iemgui->x_lcol);
}

int iemgui_dialog(t_iemgui *iemgui, t_symbol **srl, int argc, t_atom *argv)
{
    char str[144];
    int init = (int)atom_getfloatarg(5, argc, argv);
    int ldx = (int)atom_getfloatarg(10, argc, argv);
    int ldy = (int)atom_getfloatarg(11, argc, argv);
    int f = (int)atom_getfloatarg(12, argc, argv);
    int fs = (int)atom_getfloatarg(13, argc, argv);
    int bcol = (int)iemgui_getcolorarg(14, argc, argv);
    int fcol = (int)iemgui_getcolorarg(15, argc, argv);
    int lcol = (int)iemgui_getcolorarg(16, argc, argv);
    int rcv_changed=0, oldsndrcvable=0;
    int i;

    if(iemgui->x_fsf.x_rcv_able)
        oldsndrcvable |= IEM_GUI_OLD_RCV_FLAG;
    if(iemgui->x_fsf.x_snd_able)
        oldsndrcvable |= IEM_GUI_OLD_SND_FLAG;
    if(IS_A_SYMBOL(argv,7))
        srl[0] = atom_getsymbolarg(7, argc, argv);
    else if(IS_A_FLOAT(argv,7))
    {
        srl[0] = gensym("empty");
    }
    if(IS_A_SYMBOL(argv,8))
        srl[1] = atom_getsymbolarg(8, argc, argv);
    else if(IS_A_FLOAT(argv,8))
    {
        srl[1] = gensym("empty");
    }
    if(IS_A_SYMBOL(argv,9))
        srl[2] = atom_getsymbolarg(9, argc, argv);
    else if(IS_A_FLOAT(argv,9))
    {
        sprintf(str, "%g", atom_getfloatarg(9, argc, argv));
        srl[2] = gensym(str);
    }
    if(init != 0) init = 1;
    iemgui->x_isa.x_loadinit = init;
    for(i=0; i<3; i++)
        if(!srl_is_valid(srl[i]) || (!strcmp(srl[i]->s_name, "empty"))) srl[i] = &s_;

        /* expand dollargs
         * after this, srl holds the $-expanded versions of the labels
         * and iemgui->x_(snd|rcv|lab)_unexpanded hold the unexpanded versions
         */
    iemgui_all_dollararg2sym(iemgui, srl);

        /* check if the receiver changed */
    if(0
       || (!srl_is_valid(iemgui->x_rcv) && srl_is_valid(srl[1])) /* there was none, but now there is */
       || ( srl_is_valid(iemgui->x_rcv) && !(srl_is_valid(srl[1]))) /* there was one, but now there is */
       || ( srl_is_valid(iemgui->x_rcv) && srl_is_valid(srl[1]) && iemgui->x_rcv != srl[1])) /* both are valid, but changed */
        rcv_changed = 1;

        /* if the receiver changed (and was previously set), unbind it */
    if(rcv_changed && srl_is_valid(iemgui->x_rcv))
        pd_unbind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);

    iemgui->x_snd = srl[0];
    iemgui->x_fsf.x_snd_able = srl_is_valid(srl[0]);
    iemgui->x_rcv = srl[1];
    iemgui->x_fsf.x_rcv_able = srl_is_valid(srl[1]);
    iemgui->x_lab = srl[2];
    iemgui->x_lcol = lcol & 0xffffff;
    iemgui->x_fcol = fcol & 0xffffff;
    iemgui->x_bcol = bcol & 0xffffff;
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

        /* if the receiver changed (and is now set), bind it */
    if(rcv_changed && srl_is_valid(iemgui->x_rcv))
        pd_bind(&iemgui->x_obj.ob_pd, iemgui->x_rcv);

    iemgui_verify_snd_ne_rcv(iemgui);
    canvas_dirty(iemgui->x_glist, 1);
    return(oldsndrcvable);
}

void iemgui_setdialogatoms(t_iemgui *iemgui, int argc, t_atom*argv)
{
#define SETCOLOR(a, col) do {char color[MAXPDSTRING]; snprintf(color, MAXPDSTRING-1, "#%06x", 0xffffff & col); color[MAXPDSTRING-1] = 0; SETSYMBOL(a, gensym(color));} while(0)
    t_float zoom = iemgui->x_glist->gl_zoom;
    t_symbol *srl[3];
    int for_undo = 1;
    int i;
    for(i=0; i<argc; i++)
        SETFLOAT(argv+i, -1); /* initialize */

    if(!for_undo)
        iemgui_properties(iemgui, srl);

    if(argc> 0) SETFLOAT (argv+ 0, iemgui->x_w/zoom);
    if(argc> 1) SETFLOAT (argv+ 1, iemgui->x_h/zoom);
    if(argc> 5) SETFLOAT (argv+ 5, iemgui->x_isa.x_loadinit);
    if(argc> 6) SETFLOAT (argv+ 6, 1); /* num */
    if(argc> 7) SETSYMBOL(argv+ 7, for_undo?iemgui->x_snd:srl[0]);
    if(argc> 8) SETSYMBOL(argv+ 8, for_undo?iemgui->x_rcv:srl[1]);
    if(argc> 9) SETSYMBOL(argv+ 9, for_undo?iemgui->x_lab:srl[2]);
    if(argc>10) SETFLOAT (argv+10, iemgui->x_ldx);
    if(argc>11) SETFLOAT (argv+11, iemgui->x_ldy);
    if(argc>12) SETFLOAT (argv+12, iemgui->x_fsf.x_font_style);
    if(argc>13) SETFLOAT (argv+13, iemgui->x_fontsize);
    if(argc>14) SETCOLOR (argv+14, iemgui->x_bcol);
    if(argc>15) SETCOLOR (argv+15, iemgui->x_fcol);
    if(argc>16) SETCOLOR (argv+16, iemgui->x_lcol);
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

static void iemgui_draw_new(t_iemgui*x, t_glist*glist) {;}
static void iemgui_draw_config(t_iemgui*x, t_glist*glist) {;}
static void iemgui_draw_update(t_iemgui*x, t_glist*glist) {;}
static void iemgui_draw_select(t_iemgui*x, t_glist*glist) {;}
static void iemgui_draw_iolets(t_iemgui*x, t_glist*glist, int old_snd_rcv_flags)
{
    const int zoom = x->x_glist->gl_zoom;
    int xpos = text_xpix(&x->x_obj, glist);
    int ypos = text_ypix(&x->x_obj, glist);
    int iow = IOWIDTH * zoom, ioh = IEM_GUI_IOHEIGHT * zoom;
    t_canvas *canvas = glist_getcanvas(glist);
    char tag_object[128], tag_label[128], tag[128];
    char *tags[] = {tag_object, tag};

    (void)old_snd_rcv_flags;

    sprintf(tag_object, "%lxOBJ", x);
    sprintf(tag_label, "%lxLABEL", x);

    /* re-create outlet */
    sprintf(tag, "%lxOUT%d", x, 0);
    pdgui_vmess(0, "crs", canvas, "delete", tag);
    if(!x->x_fsf.x_snd_able) {
        pdgui_vmess(0, "crr iiii rs rS",
            canvas, "create", "rectangle",
            xpos, ypos + x->x_h + zoom - ioh, xpos + iow, ypos + x->x_h,
            "-fill", "black",
            "-tags", 2, tags);
        /* keep label above outlet */
        pdgui_vmess(0, "crss", canvas, "lower", tag, tag_label);
    }

    /* re-create inlet */
    sprintf(tag, "%lxIN%d", x, 0);
    pdgui_vmess(0, "crs", canvas, "delete", tag);
    if(!x->x_fsf.x_rcv_able) {
        pdgui_vmess(0, "crr iiii rs rS",
            canvas, "create", "rectangle",
            xpos, ypos, xpos + iow, ypos - zoom + ioh,
            "-fill", "black",
            "-tags", 2, tags);
        /* keep label above inlet */
        pdgui_vmess(0, "crss", canvas, "lower", tag, tag_label);
    }
}

static void iemgui_draw_erase(t_iemgui* x, t_glist* glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    char tag_object[128];
    sprintf(tag_object, "%lxOBJ", x);

    pdgui_vmess(0, "crs", canvas, "delete", tag_object);
}

static void iemgui_draw_move(t_iemgui *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    int dx = text_xpix(&x->x_obj, glist) - x->x_private->p_prevX;
    int dy = text_ypix(&x->x_obj, glist) - x->x_private->p_prevY;

    char tag_object[128];
    sprintf(tag_object, "%lxOBJ", x);

    pdgui_vmess(0, "crs ii", canvas, "move", tag_object, dx, dy);
}

static void iemgui_draw(t_iemgui *x, t_glist *glist, int mode)
{
#define DRAW_FUN(fun, x, glist) {                                       \
        t_iemdrawfunptr do_draw = x->x_private->p_widget.draw_##fun;    \
        if (!do_draw) do_draw = (t_iemdrawfunptr)iemgui_draw_##fun;     \
        do_draw(x, glist);                                              \
    }

    switch(mode) {
    case (IEM_GUI_DRAW_MODE_UPDATE):
    {
        t_iemdrawfunptr draw_update = x->x_private->p_widget.draw_update;
        if(!draw_update)
            draw_update = (t_iemdrawfunptr)iemgui_draw_update;
        sys_queuegui(x, x->x_glist, (t_guicallbackfn)draw_update);
    }
        break;
    case (IEM_GUI_DRAW_MODE_MOVE):
        DRAW_FUN(move, x, glist);
        break;
    case (IEM_GUI_DRAW_MODE_NEW):
        DRAW_FUN(new, x, glist);
        break;
    case (IEM_GUI_DRAW_MODE_SELECT):
        DRAW_FUN(select, x, glist);
        break;
    case (IEM_GUI_DRAW_MODE_ERASE):
        DRAW_FUN(erase, x, glist);
        break;
    case (IEM_GUI_DRAW_MODE_CONFIG):
        DRAW_FUN(config, x, glist);
        break;
    default:
        if(x->x_private->p_widget.draw_iolets)
            x->x_private->p_widget.draw_iolets(x, glist, mode - IEM_GUI_DRAW_MODE_IO);
        else
            iemgui_draw_iolets(x, glist, mode - IEM_GUI_DRAW_MODE_IO);
    }
}

void iemgui_setdrawfunctions(t_iemgui *iemgui, t_iemgui_drawfunctions *w)
{
#define SET_DRAW(x, fun) \
    x->x_private->p_widget.draw_##fun = w->draw_##fun

    SET_DRAW(iemgui, new);
    SET_DRAW(iemgui, config);
    SET_DRAW(iemgui, iolets);
    SET_DRAW(iemgui, update);
    SET_DRAW(iemgui, select);
    SET_DRAW(iemgui, erase);
    SET_DRAW(iemgui, move);
}



t_iemgui *iemgui_new(t_class*cls)
{
    t_iemgui *x = (t_iemgui *)pd_new(cls);
    t_glist *cnv = canvas_getcurrent();
    int fs = cnv->gl_font;
    x->x_glist = cnv;
    x->x_private = (t_iemgui_private*)getbytes(sizeof(*x->x_private));
    x->x_draw = (t_iemfunptr)iemgui_draw;

    x->x_fontsize = (fs<4)?4:fs;
/*
    int fs = x->x_gui.x_fontsize;
    x->x_gui.x_fontsize = (fs < 4)?4:fs;
*/
    iem_inttosymargs(&x->x_isa, 0);
    iem_inttofstyle(&x->x_fsf, 0);

    x->x_bcol = 0xFCFCFC;
    x->x_fcol = 0x00;
    x->x_lcol = 0x00;

    return x;
}


#if 1
/* LEGACY (for binary compatibility with existing externals)
 * DO NOT USE
 */

/* g_all_guis.h */
/* *********** */
int iemgui_clip_font(int size)
{
    if(size < IEM_FONT_MINSIZE)
        size = IEM_FONT_MINSIZE;
    return(size);
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



/* g_canvas.h */
/* ********** */

t_symbol *iemgui_put_in_braces(t_symbol *s)
{
    const char *s1;
    char buf[MAXPDSTRING+1], *s2;
    int i = 0;
    if (strlen(s->s_name) >= MAXPDSTRING)
        return (s);
    for (s1 = s->s_name, s2 = buf; ; s1++, s2++, i++)
    {
        if (i == 0)
        {
            *s2 = '{';
            s2++;
        }
        if (!(*s2 = *s1))
        {
            *s2 = '}';
            s2++;
            *s2 = '\0';
            break;
        }
    }
    return(gensym(buf));
}


/* no header */
/* ********* */
void iemgui_all_put_in_braces(t_symbol **srlsym)
{
    srlsym[0] = iemgui_put_in_braces(srlsym[0]);
    srlsym[1] = iemgui_put_in_braces(srlsym[1]);
    srlsym[2] = iemgui_put_in_braces(srlsym[2]);
}

    /* for compatibility with pre-0.47 unofficial IEM GUIS like "knob". */
void iemgui_all_colfromload(t_iemgui *iemgui, int *bflcol)
{
    static int warned = 0;
    if (!warned)
    {
        post("warning: external GUI object uses obsolete Pd function %s()", __FUNCTION__);
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
#endif
