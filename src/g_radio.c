/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* [hv]dial.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */

/* name change to [vv]radio by MSP (it's a radio button really) and changed to
   put out a "float" as in sliders, toggles, etc. */

#include <string.h>
#include <stdio.h>
#include "m_pd.h"

#include "g_all_guis.h"
#include "m_private_utils.h" // for ALLOCA and FREEA in mradio_spit

/* ------------- hdl     gui-horizontal dial ---------------------- */

t_widgetbehavior radio_widgetbehavior;
static t_class *radio_class;

static int clip_int(int val, int min_val, int max_val)
{
    if (val >= max_val)
        val = max_val - 1;
    if (val < min_val)
        val = min_val;
    return val;
}

static int radio_matrix_initialize(t_radio *x)
{
    size_t ncols = (size_t)x->x_number[0];
    size_t nrows = (size_t)x->x_number[1];
    size_t nbytes = ncols * nrows * sizeof(int);

    if (ncols <= 0 || nrows <= 0)
        return(0);

    if (ncols > (IEM_RADIO_MAX * IEM_RADIO_MAX) / nrows)
        return(0);

    if (x->x_matrix && x->x_matrix_idx){
        freebytes(x->x_matrix, nbytes);
        freebytes(x->x_matrix_idx, nbytes);
        x->x_matrix = NULL;
        x->x_matrix_idx = NULL;
    }

    x->x_matrix = (int *)getbytes(nbytes);
    x->x_matrix_idx = (int *)getbytes(nbytes);

    if (!x->x_matrix || !x->x_matrix_idx)
        return(0);

    memset(x->x_matrix, 0, nbytes);
    memset(x->x_matrix_idx, 0, nbytes);
    return(1);
}

static int radio_coord2idx(t_radio *x, int col, int row, int *idx)
{
    const int ncols = x->x_number[(int)x->x_orientation];
    const int nrows = x->x_number[!(int)x->x_orientation];

    if ((unsigned)col >= (unsigned)ncols || (unsigned)row >= (unsigned)nrows)
        return(0);

    int i;

    if (x->x_direction == horizontal)
        i = row * ncols + col;
    else
        i = col * nrows + row;

    if ((unsigned)i >= (unsigned)(ncols * nrows)) {
        return(0);
    }

    *idx = i;
    return(1);
}

static int radio_idx2coord(t_radio *x, int idx, int *col, int *row)
{
    const int ncols = x->x_number[(int)x->x_orientation];
    const int nrows = x->x_number[!(int)x->x_orientation];

    if (!col || !row || ncols <= 0 || nrows <= 0)
        return(0);

    if (idx < 0 || idx >= ncols * nrows)
        return(0);


    if (x->x_direction == horizontal) {
        *row = idx / ncols;
        *col = idx % ncols;
    } else {
        *col = idx / nrows;
        *row = idx % nrows;
    }
    return(1);
}

static int radio_is_vertical(t_radio *x)
{
    if (x->x_number[0] < x->x_number[1])
        return(1);
    return(0);
}

// Returns 1 on success, 0 on invalid input.
static int radio_encode_extended(int nrows, int ncols, t_iem_orientation direction, int *size) {
    if (!size) return(0);
    if (nrows <= 1 || nrows > IEM_RADIO_MAX) return(0);
    if (ncols <= 1 || ncols > IEM_RADIO_MAX) return(0);

    uint16_t r7 = (uint16_t)(nrows - 1);  // 0..127
    uint16_t c7 = (uint16_t)(ncols - 1);  // 0..127
    uint16_t d  = (uint16_t)direction; // 0/1

    uint16_t code = (uint16_t)((d << 14) | (r7 << 7) | c7); // 0..32767
    *size = -(int)(code + 1); // -1..-32768
    return(1);
}

// If number is extended (<0), decodes into nrows/ncols/direction and returns 1.
// If number is not extended (>=0), returns 0 (caller should handle legacy).
static int radio_decode_extended(int number, int *ncols, int *nrows, t_iem_orientation *direction) {
    if (!nrows || !ncols || !direction) return(0);
    if (number >= 0) return(0);

    uint16_t code = (uint16_t)(-number - 1); // 0..32767

    int d = (code >> 14) & 1;
    int r = ((code >> 7) & 0x7F) + 1;
    int c = (code & 0x7F) + 1;

    // Sanity (should always hold given the encoding)
    if (r < 1 || r > IEM_RADIO_MAX) return(0);
    if (c < 1 || c > IEM_RADIO_MAX) return(0);

    *direction = d;
    *nrows = r;
    *ncols = c;
    return(1);
}

static int radio_matrix_reindex(t_radio *x)
{
    int idx;
    const int ncols = x->x_number[(int)x->x_orientation];
    const int nrows = x->x_number[!(int)x->x_orientation];
    for(int row=0, i=0; row<nrows; ++row)
        for(int col=0; col<ncols; ++col) {
            if(!radio_coord2idx(x, col, row, &idx)) return(0);
            x->x_matrix_idx[i++] = idx;
        }
    return(1);
}

static void radio_doresize(t_radio *x, int ncols, int nrows) {
    int old_ncols = x->x_number[(int)x->x_orientation];
    int old_nrows = x->x_number[!(int)x->x_orientation];
    int new_ncols = (ncols < 1 ? 1 : ncols);
    int new_nrows = (nrows < 1 ? 1 : nrows);

    if ((new_ncols == old_ncols && new_nrows == old_nrows))
        return;

    x->x_number[(int)x->x_orientation] = new_ncols;
    x->x_number[!(int)x->x_orientation] = new_nrows;

    if(!radio_matrix_initialize(x))
        pd_error(x, "radio_doresize: Could not initialize after resizing.");

    if(!radio_matrix_reindex(x))
        return pd_error(x, "radio_doresize: Could not reindex matrix.");

}

/* widget helper functions */

/* cannot use iemgui's default draw_iolets, because
 * - vradio would use show the outlet at the 0th button rather than the last...
 */
static void radio_draw_io(t_radio* x, t_glist* glist, int old_snd_rcv_flags)
{
    const int zoom = IEMGUI_ZOOM(x);
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int nrows = x->x_number[!(int)x->x_orientation];
    int iow = IOWIDTH * zoom, ioh = IEM_GUI_IOHEIGHT * zoom;
    t_canvas *canvas = glist_getcanvas(glist);
    char tag_object[128], tag_but[128], tag[128];
    char *tags[] = {tag_object, tag};

    (void)old_snd_rcv_flags;

    sprintf(tag_object, "%pOBJ", x);
    sprintf(tag_but, "%pBUT", x);

    sprintf(tag, "%pOUT%d", x, 0);
    pdgui_vmess(0, "crs", canvas, "delete", tag);
    if(!x->x_gui.x_fsf.x_snd_able)
    {
        int height = x->x_gui.x_h * nrows;
        pdgui_vmess(0, "crr iiii rk rk rS", canvas, "create", "rectangle",
            xpos, ypos + height + zoom - ioh,
            xpos + iow, ypos + height,
            "-fill", THISGUI->i_foregroundcolor,
            "-outline", THISGUI->i_foregroundcolor,
            "-tags", 2, tags);

            /* keep buttons above outlet */
        pdgui_vmess(0, "crss", canvas, "lower", tag, tag_but);
    }

    sprintf(tag, "%pIN%d", x, 0);
    pdgui_vmess(0, "crs", canvas, "delete", tag);
    if(!x->x_gui.x_fsf.x_rcv_able)
    {
        pdgui_vmess(0, "crr iiii rk rk rS", canvas, "create", "rectangle",
            xpos, ypos,
            xpos + iow, ypos - zoom + ioh,
            "-fill", THISGUI->i_foregroundcolor,
            "-outline", THISGUI->i_foregroundcolor,
            "-tags", 2, tags);

            /* keep buttons above inlet */
        pdgui_vmess(0, "crss", canvas, "lower", tag, tag_but);
    }
}

static void radio_draw_config(t_radio* x, t_glist* glist)
{
    t_iemgui *iemgui = &x->x_gui;
    t_canvas *canvas = glist_getcanvas(glist);
    int col, row;
    char tag[128];
    t_atom fontatoms[3];
    const int zoom = IEMGUI_ZOOM(x);
    const int x0 = text_xpix(&x->x_gui.x_obj, glist);
    const int y0 = text_ypix(&x->x_gui.x_obj, glist);
    const int cellw = x->x_gui.x_w;
    const int cellh = x->x_gui.x_h;
    const int cellw4 = cellw / 4;
    const int cellh4 = cellh / 4;
    const int w = x->x_gui.x_w / zoom;
    int crossw = zoom;

    if(w >= 30)
        crossw = 2 * zoom;
    if(w >= 60)
        crossw = 3 * zoom;

    SETSYMBOL(fontatoms+0, gensym(iemgui->x_font));
    SETFLOAT (fontatoms+1, -iemgui->x_fontsize * zoom);
    SETSYMBOL(fontatoms+2, gensym(sys_fontweight));

    for(int i=0; i<x->x_number[0]*x->x_number[1]; i++) {
        const int idx = x->x_matrix_idx[i];
 
        if(!radio_idx2coord(x, idx, &col, &row))
            pd_error(x, "radio_draw_config: Bad index (%d)", idx);

        int xx11 = x0 + col * cellw;
        int yy11 = y0 + row * cellh;
        int xx12 = xx11 + cellw;
        int yy12 = yy11 + cellh;

        int curr_color = (x->x_on == idx) ? x->x_gui.x_fcol : x->x_gui.x_bcol;
        int step_color = (x->x_matrix[idx] != 0) ?  x->x_gui.x_fcol : x->x_gui.x_bcol;

        sprintf(tag, "%pBASE%d", x, idx);
        pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
            xx11, yy11, xx12, yy12);
        pdgui_vmess(0, "crs ri rk rk", canvas, "itemconfigure", tag,
            "-width", zoom, "-fill", x->x_gui.x_bcol,
            "-outline", x->x_gui.x_fcol);

        sprintf(tag, "%pBUT%d", x, idx);
        pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
            xx11 + cellw4, yy11 + cellh4, xx12 - cellw4, yy12 - cellh4);
        pdgui_vmess(0, "crs rk rk", canvas, "itemconfigure", tag,
            "-fill", curr_color, "-outline", curr_color);

        sprintf(tag, "%pX1%d", x, idx);
        pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
            xx11 + crossw + zoom, yy11 + crossw + zoom,
            xx11 + x->x_gui.x_w - crossw - zoom, yy11 + x->x_gui.x_h - crossw - zoom);
        pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
            "-width", crossw, "-fill", step_color);

        sprintf(tag, "%pX2%d", x, idx);
        pdgui_vmess(0, "crs iiii", canvas, "coords", tag,
            xx11 + crossw + zoom, yy11 + x->x_gui.x_h - crossw - zoom,
            xx11 + x->x_gui.x_w - crossw - zoom, yy11 + crossw + zoom);
        pdgui_vmess(0, "crs ri rk", canvas, "itemconfigure", tag,
            "-width", crossw, "-fill", step_color);
    }

    sprintf(tag, "%pLABEL", x);
    pdgui_vmess(0, "crs ii", canvas, "coords", tag,
        x0 + x->x_gui.x_ldx * zoom, y0 + x->x_gui.x_ldy * zoom);
    pdgui_vmess(0, "crs rA rk", canvas, "itemconfigure", tag,
        "-font", 3, fontatoms,
        "-fill", x->x_gui.x_lcol);
    iemgui_dolabel(x, &x->x_gui, x->x_gui.x_lab, 1);
    post("done config.");
}

static void radio_draw_new(t_radio *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    char tag_n[128], tag[128], tag_object[128];
    char *tags[] = {tag_object, tag, tag_n, "text"};
    sprintf(tag_object, "%pOBJ", x);

    if(!radio_matrix_reindex(x))
        return pd_error(x, "radio_draw_new: could not reindex matrix.");

    for(int i=0; i<x->x_number[0]*x->x_number[1]; i++) {
        const int idx = x->x_matrix_idx[i];
 
        sprintf(tag, "%pBASE", x);
        sprintf(tag_n, "%pBASE%d", x, idx);
        pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle",
            0, 0, 0, 0, "-tags", 3, tags);

        sprintf(tag, "%pBUT", x);
        sprintf(tag_n, "%pBUT%d", x, idx);
        pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle",
            0, 0, 0, 0, "-tags", 3, tags);

        sprintf(tag, "%pSTEP", x);
        sprintf(tag_n, "%pX1%d", x, idx);
        pdgui_vmess(0, "crr iiii rS", canvas, "create", "line",
            0, 0, 0, 0, "-tags", 3, tags);

        sprintf(tag_n, "%pX2%d", x, idx);
        pdgui_vmess(0, "crr iiii rS", canvas, "create", "line",
            0, 0, 0, 0, "-tags", 3, tags);
    }
    /* make sure the buttons are above their base */
    sprintf(tag, "%pBUT", x);
    sprintf(tag_n, "%pBASE", x);
    pdgui_vmess(0, "crss", canvas, "raise", tag, tag_n);

    sprintf(tag, "%pSTEP", x);
    sprintf(tag_n, "%pBUT", x);
    pdgui_vmess(0, "crss", canvas, "raise", tag, tag_n);

    sprintf(tag, "%pLABEL", x);
    sprintf(tag_n, "label");
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text",
        0, 0, "-anchor", "w", "-tags", 4, tags);

    radio_draw_config(x, glist);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO);
}

static void radio_draw_select(t_radio* x, t_glist* glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    char tag[128];
    unsigned int col = THISGUI->i_foregroundcolor, lcol = x->x_gui.x_lcol;

    if(x->x_gui.x_fsf.x_selected)
        lcol = col = THISGUI->i_selectcolor;

    sprintf(tag, "%pBASE", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-outline", col);
    sprintf(tag, "%pLABEL", x);
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag, "-fill", lcol);
}

static void radio_draw_update(t_gobj *client, t_glist *glist)
{
    t_radio *x = (t_radio *)client;
    if(!glist_isvisible(glist)) return;

    t_canvas *canvas = glist_getcanvas(glist);
    char tag_but[128], tag_x1[128], tag_x2[128];

    for(int i=0; i<x->x_number[0]*x->x_number[1]; i++) {
        const int idx = x->x_matrix_idx[i];
        int but_on_cell = x->x_on == idx;
        int step_on_cell = x->x_matrix[idx] == 1;
        /* paint the BUTton if it is currently on that index */
        /* draw the X step if the value at that index is other than zero */
        int curr_color = but_on_cell ? x->x_gui.x_fcol : x->x_gui.x_bcol;
        int step_color = step_on_cell ? x->x_gui.x_fcol : x->x_gui.x_bcol;

        sprintf(tag_but, "%pBUT%d", x, idx);
        pdgui_vmess(0, "crs rk rk", canvas, "itemconfigure", tag_but,
                    "-fill", curr_color, "-outline", curr_color);

        sprintf(tag_x1, "%pX1%d", x, idx);
        pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag_x1,
                    "-fill", step_color);
        sprintf(tag_x2, "%pX2%d", x, idx);
        pdgui_vmess(0, "crs rk", canvas, "itemconfigure", tag_x2,
                    "-fill", step_color);

        if (step_on_cell == 1 && but_on_cell == 0) {
            pdgui_vmess(0, "crss", canvas, "raise", tag_x1, tag_but);
            pdgui_vmess(0, "crss", canvas, "raise", tag_x2, tag_but);
        }
        if (step_on_cell == 0 && but_on_cell == 1) {
            pdgui_vmess(0, "crss", canvas, "raise", tag_but, tag_x1);
            pdgui_vmess(0, "crss", canvas, "raise", tag_but, tag_x2);
        }
    }
}

/* ------------------------ hdl widgetbehaviour----------------------------- */

static void radio_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_radio *x = (t_radio *)z;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist);
    *xp2 = *xp1 + x->x_gui.x_w * x->x_number[(int)x->x_orientation];
    *yp2 = *yp1 + x->x_gui.x_h * x->x_number[!(int)x->x_orientation];
}


static void radio_resize(t_radio *x, t_floatarg cols, t_floatarg rows) {
  int ncols = clip_int((int)cols, 1, IEM_RADIO_MAX + 1);
  int nrows = clip_int((int)rows, 1, IEM_RADIO_MAX + 1);

  if (ncols != x->x_number[(int)x->x_orientation] || nrows != x->x_number[!(int)x->x_orientation]) {
    int vis = glist_isvisible(x->x_gui.x_glist);
    if(vis)
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_ERASE);
    radio_doresize(x, ncols, nrows);
    if(vis && gobj_shouldvis((t_gobj *)x, x->x_gui.x_glist))
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_NEW);
        canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
    }
  }
}

static void radio_save(t_gobj *z, t_binbuf *b)
{
    t_radio *x = (t_radio *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];
    const char*objname;
    const int direction = x->x_direction;
    const int ncols = x->x_number[(int)x->x_orientation];
    const int nrows = x->x_number[!(int)x->x_orientation];
    int size;

    if(radio_encode_extended(nrows, ncols, direction, &size)) {
        if (x->x_orientation == horizontal)
            objname="hradio";
        else
            objname="vradio";
    } else if(x->x_orientation == horizontal)
    {
        if(x->x_compat)
            objname="hdl";
        else
            objname="hradio";
        size = ncols;
    } else {
        if(x->x_compat)
            objname="vdl";
        else
            objname="vradio";
        size = nrows;
    }

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiiisssiiiisssf", gensym("#X"), gensym("obj"),
                (int)x->x_gui.x_obj.te_xpix,
                (int)x->x_gui.x_obj.te_ypix,
        gensym(objname),
                x->x_gui.x_w/IEMGUI_ZOOM(x),
                x->x_change, iem_symargstoint(&x->x_gui.x_isa), size,
                srl[0], srl[1], srl[2],
                x->x_gui.x_ldx, x->x_gui.x_ldy,
                iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize,
                bflcol[0], bflcol[1], bflcol[2],
                x->x_gui.x_isa.x_loadinit?x->x_fval:0.);
    binbuf_addv(b, ";");
    post("saved %s", objname);
}

static void radio_properties(t_gobj *z, t_glist *owner)
{
    t_radio *x = (t_radio *)z;
    int hchange = -1;
    const char*objname;
    const int is_vertical = radio_is_vertical(x);
    const int number = x->x_number[is_vertical];

    objname = is_vertical ? "vradio" : "hradio";

    if(x->x_compat)
        hchange = x->x_change;

    iemgui_new_dialog(x, &x->x_gui, objname,
        (float)(x->x_gui.x_w)/IEMGUI_ZOOM(x), IEM_GUI_MINSIZE,
        0, 0,
        0, 0,
        0,
        hchange, "new-only", "new&old",
        1, -1, number);
}

static void radio_dialog(t_radio *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int a = (int)atom_getfloatarg(0, argc, argv);
    int chg = (int)atom_getfloatarg(4, argc, argv);
    int num = (int)atom_getfloatarg(6, argc, argv);
    int sr_flags;
    int redraw = 0;
    t_atom undo[18];
    iemgui_setdialogatoms(&x->x_gui, 18, undo);
    SETFLOAT(undo+1, 0);
    SETFLOAT(undo+2, 0);
    SETFLOAT(undo+3, 0);
    SETFLOAT(undo+4, (x->x_compat)?x->x_change:-1);
    SETFLOAT(undo+6, x->x_number[(int)x->x_orientation]);

    pd_undo_set_objectstate(x->x_gui.x_glist, (t_pd*)x, gensym("dialog"),
                            18, undo,
                            argc, argv);

    if(chg != 0) chg = 1;
    x->x_change = chg;
    sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);
    x->x_gui.x_w = iemgui_clip_size(a) * IEMGUI_ZOOM(x);
    x->x_gui.x_h = x->x_gui.x_w;

    int *ncols = &x->x_number[0];
    int *nrows = &x->x_number[1];
    t_iem_orientation *direction = &x->x_direction;
    t_iem_orientation arg_orient = ('v' == *s->s_name) ? vertical : horizontal;

    if (num != *ncols && glist_isvisible(x->x_gui.x_glist))
    {
        /* we need to recreate the buttons */
        /* if it's a matrix, decode size, direction, and infer and set orientation */
        if(radio_decode_extended(num, ncols, nrows, direction))
            x->x_orientation = (*ncols < *nrows) ? vertical : horizontal;
        else {
            /* we need to update the size and the orientation */
            *ncols = (arg_orient == horizontal) ? num : 1;
            *nrows = (arg_orient == horizontal) ? 1 : num;
            x->x_orientation = horizontal; // needs to be horizontal by default
        }
        radio_resize(x, *ncols, *nrows);
        redraw = 1;
    }
    if(x->x_on >= *ncols)
    {
        x->x_on_old = x->x_on = *ncols - 1;
        x->x_on_old = x->x_on;
    }

    if (redraw && gobj_shouldvis((t_gobj *)x, x->x_gui.x_glist))
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_NEW);
        canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
    } else {
        /* just reconfigure */
        iemgui_size((void *)x, &x->x_gui);
    }
}

static void radio_set(t_radio *x, t_floatarg f)
{
    int max_val = x->x_number[(int)x->x_orientation];
    int i = clip_int((int)f, 0, max_val);

    x->x_fval = f;

    if(x->x_on != x->x_on_old)
    {
        int old = x->x_on_old;
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

static void radio_bang(t_radio *x)
{
    if(x->x_compat)
    {
            /* compatibility with earlier "[hv]dial" behavior */
        t_atom at[2];
        if((x->x_change) && (x->x_on != x->x_on_old))
        {
            SETFLOAT(at+0, (t_float)x->x_on_old);
            SETFLOAT(at+1, 0.0);
            outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
        }
        x->x_on_old = x->x_on;
        SETFLOAT(at+0, (t_float)x->x_on);
        SETFLOAT(at+1, 1.0);
        outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
    }
    else
    {
        t_float outval = (pd_compatibilitylevel < 46 ? x->x_on : x->x_fval);
        outlet_float(x->x_gui.x_obj.ob_outlet, outval);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_float(x->x_gui.x_snd->s_thing, outval);
    }
}

static void radio_fout(t_radio *x, t_floatarg f)
{
    int max_val = x->x_number[(int)x->x_orientation]; // keep first index here as max val
    int i = clip_int((int)f, 0, max_val);

    x->x_fval = f;

    if(x->x_compat)
    {
            /* compatibility with earlier "[hv]dial" behavior */
        t_atom at[2];
        if((x->x_change) && (i != x->x_on_old))
        {
            SETFLOAT(at+0, (t_float)x->x_on_old);
            SETFLOAT(at+1, 0.0);
            outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
        }
        if(x->x_on != x->x_on_old)
            x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        x->x_on_old = x->x_on;
        SETFLOAT(at+0, (t_float)x->x_on);
        SETFLOAT(at+1, 1.0);
        outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
    }
    else
    {
        t_float outval = (pd_compatibilitylevel < 46 ? i : x->x_fval);
        x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        outlet_float(x->x_gui.x_obj.ob_outlet, outval);
        if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_float(x->x_gui.x_snd->s_thing, outval);
    }
}

static void radio_get_cell(t_radio *x, t_floatarg fidx)
{
    const int maxval = x->x_number[0] * x->x_number[1];
    const int idx = clip_int((int)fidx, 0, maxval);
    const t_float outval = (t_float)x->x_matrix[idx];
    outlet_float(x->x_gui.x_obj.ob_outlet, outval);
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
        pd_float(x->x_gui.x_snd->s_thing, outval);
}

static void radio_set_cell(t_radio *x, t_floatarg fidx, t_floatarg fval)
{
    const int maxval = x->x_number[0] * x->x_number[1];
    const int idx = clip_int((int)fidx, 0, maxval);
    int val = ((int)fval != 0 ? 1 : 0 );
    x->x_matrix[idx] = val;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void radio_cell(t_radio *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!argc)
        return;

    t_float fidx = atom_getfloatarg(0, argc, argv);
    if (argc==1) {
        return(radio_get_cell(x, fidx));
    }
    t_float fval = atom_getfloatarg(1, argc, argv);
    if (argc>2) {
        post("Ignoring extra arguments");
        postatom(argc-2, argv+2);
        endpost();
    }
    return(radio_set_cell(x, fidx, fval));
}

static void radio_line(t_radio *x, t_floatarg faxis, t_floatarg flinenum)
{
    t_atom *av;
    int ac = 0;
    t_symbol *s = &s_list;
    const int axis = ((int)faxis != 0 ? 1 : 0);
    const int max_val = x->x_number[axis];
    const int i = clip_int((int)flinenum, 0, max_val);

    ALLOCA(t_atom, av, max_val, IEM_RADIO_MAX + 1);

    for (int j = 0; j < max_val; ++j) {
        int idx;
        const int col = (!axis) ? j : i;
        const int row = (!axis) ? i : j;
        if(radio_coord2idx(x, col, row, &idx)) {
            SETFLOAT(av + ac, (t_float)x->x_matrix[idx]);
            ac++;
        }
    }

    if (!ac)
        pd_error(x, "Could not output line.");
    else {
        outlet_list(x->x_gui.x_obj.ob_outlet, s, ac, av);
        if (x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
            pd_list(x->x_gui.x_snd->s_thing, s, ac, av);
    }

    FREEA(t_atom, av, max_val, IEM_RADIO_MAX + 1);
}

static void radio_float(t_radio *x, t_floatarg f)
{
    int ncols = x->x_number[(int)x->x_orientation];
    int nrows = x->x_number[!(int)x->x_orientation];
    int i = clip_int((int)f, 0, ncols);

    x->x_fval = f;

    if(x->x_compat)
    {
        t_atom at[2];
            /* compatibility with earlier "[hv]dial" behavior */
        if((x->x_change) && (i != x->x_on_old))
        {
            if(x->x_gui.x_fsf.x_put_in2out)
            {
                SETFLOAT(at+0, (t_float)x->x_on_old);
                SETFLOAT(at+1, 0.0);
                outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
                if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                    pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
            }
        }
        if(x->x_on != x->x_on_old)
            x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        x->x_on_old = x->x_on;
        if(x->x_gui.x_fsf.x_put_in2out)
        {
            SETFLOAT(at+0, (t_float)x->x_on);
            SETFLOAT(at+1, 1.0);
            outlet_list(x->x_gui.x_obj.ob_outlet, &s_list, 2, at);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, at);
        }
    } else if (ncols > 1 && nrows > 1) {
        if (x->x_output_mode==1)
            radio_get_cell(x, x->x_fval);
        else if (x->x_output_mode==2)
            radio_line(x, x->x_orientation, x->x_fval);
        else
            radio_fout(x, x->x_fval);
    } else
    {
        t_float outval = (pd_compatibilitylevel < 46 ? i : x->x_fval);
        x->x_on_old = x->x_on;
        x->x_on = i;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        if(x->x_gui.x_fsf.x_put_in2out)
        {
            outlet_float(x->x_gui.x_obj.ob_outlet, outval);
            if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
                pd_float(x->x_gui.x_snd->s_thing, outval);
        }
    }
}

static void radio_spit(t_radio *x)
{
    t_atom *av;
    t_symbol *s = &s_list;
    const int size = x->x_number[0] * x->x_number[1];

    ALLOCA(t_atom, av, size, IEM_RADIO_MAX + 1);
    for(int i=0; i<size; i++) {
        const int idx = x->x_matrix_idx[i];
        SETFLOAT(&av[i], (t_float)x->x_matrix[idx]);
    }

    outlet_list(x->x_gui.x_obj.ob_outlet, s, size, av);
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
        pd_list(x->x_gui.x_snd->s_thing, s, size, av);

    FREEA(t_atom, av, size, IEM_RADIO_MAX + 1);
}

static void radio_fill_line(t_radio *x, t_floatarg faxis, t_floatarg flinenum, t_floatarg fval)
{
    int val = ((int)fval != 0 ? 1 : 0 );
    const int axis = ((int)faxis != 0 ? 1 : 0);
    const int max_val = x->x_number[axis];
    const int i = clip_int((int)flinenum, 0, max_val);

    for (int j = 0; j < max_val; ++j) {
        int idx;
        const int col = (!axis) ? j : i;
        const int row = (!axis) ? i : j;
        if(radio_coord2idx(x, col, row, &idx))
            x->x_matrix[idx] = val;
    }
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void radio_get_row(t_radio *x, t_floatarg frow)
{
    t_float faxis = (t_float)horizontal;
    t_float flinenum = frow;
    return(radio_line(x, faxis, flinenum));
}

static void radio_set_row(t_radio *x, t_floatarg frow, t_floatarg fval)
{
    t_float faxis = (t_float)horizontal;
    t_float flinenum = frow;
    return(radio_fill_line(x, faxis, flinenum, fval));
}

static void radio_row(t_radio *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!argc)
        return;

    t_float frow = atom_getfloatarg(0, argc, argv);
    if (argc==1)
        return(radio_get_row(x, frow));

    t_float fval = atom_getfloatarg(1, argc, argv);
    if (argc>2) {
        post("Ignoring extra arguments");
        postatom(argc-2, argv+2);
        endpost();
    }
    return(radio_set_row(x, frow, fval));
}

static void radio_get_column(t_radio *x, t_floatarg fcol)
{
    t_float faxis = (t_float)vertical;
    t_float flinenum = fcol;
    return(radio_line(x, faxis, flinenum));
}

static void radio_set_column(t_radio *x, t_floatarg fcol, t_floatarg fval)
{
    t_float faxis = (t_float)vertical;
    t_float flinenum = fcol;
    return(radio_fill_line(x, faxis, flinenum, fval));
}
static void radio_column(t_radio *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!argc)
        return;

    t_float fcol = atom_getfloatarg(0, argc, argv);
    if (argc==1)
        return(radio_get_column(x, fcol));

    t_float fval = atom_getfloatarg(1, argc, argv);
    if (argc>2) {
        post("Ignoring extra arguments");
        postatom(argc-2, argv+2);
        endpost();
    }
    return(radio_set_column(x, fcol, fval));
}

static void radio_get(t_radio *x, t_floatarg fcol, t_floatarg frow)
{
    const int ncols = x->x_number[(int)x->x_orientation];
    const int nrows = x->x_number[!(int)x->x_orientation];
    // case 1: one column, one row
    if (ncols == 1 && nrows == 1)
        return radio_get_cell(x, 0.0);
    const int col = clip_int((int)fcol, -1, ncols);
    const int row = clip_int((int)frow, -1, nrows);
    int idx = 0;
    // case 2: one column, multiple rows
    if (ncols == 1) {
        if (col > 1)
            return pd_error(x, "radio_get: Illegal arguments.");
        if (row < 0) return radio_get_column(x, 1.0);
        else {
            if (!radio_coord2idx(x, 1, row, &idx))
                return pd_error(x, "radio_get: Bad index (%d,%d)", col, row);
            return radio_get_cell(x, (t_float)idx);
        }
    }
    // case 3: multiple columns, one row
    if (nrows == 1) {
        if (row > 1)
            return pd_error(x, "radio_get: Illegal arguments.");
        if (col < 0) return radio_get_row(x, 1.0);
        else {
            if (!radio_coord2idx(x, col, 1, &idx))
                return pd_error(x, "radio_get: Bad index (%d,%d)", col, row);
            return radio_get_cell(x, (t_float)idx);
        }
    }
    // case 4: multiple columns, multiple rows
    if(col < 0 && row < 0) return radio_spit(x);
    else if (col < 0) return radio_get_row(x, (t_float)row);
    else if (row < 0) return radio_get_column(x, (t_float)col);
    else {
        if (!radio_coord2idx(x, col, row, &idx))
            return pd_error(x, "radio_get: Bad index (%d,%d)", col, row);
        return radio_get_cell(x, (t_float)idx);
    }
}

static void radio_fill(t_radio *x, t_floatarg fval)
{
    int val = ((int)fval != 0 ? 1 : 0 );
    for(int i=0; i<x->x_number[0]*x->x_number[1]; i++)
        x->x_matrix[i] = val;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void radio_matrix_set(t_radio *x, t_floatarg fcol, t_floatarg frow, t_floatarg fval)
{
    const int ncols = x->x_number[(int)x->x_orientation];
    const int nrows = x->x_number[!(int)x->x_orientation];
    // case 1: one column, one row
    if (ncols == 1 && nrows == 1)
        return radio_set_cell(x, 0.0, fval);
    const int col = clip_int((int)fcol, -1, ncols);
    const int row = clip_int((int)frow, -1, nrows);
    int idx = 0;
    // case 2: one column, multiple rows
    if (ncols == 1) {
        if (col > 1)
            return pd_error(x, "radio_get: Illegal arguments.");
        if (row < 0) return radio_set_column(x, 1.0, fval);
        else {
            if (!radio_coord2idx(x, 1, row, &idx))
                return pd_error(x, "radio_get: Bad index (%d,%d)", col, row);
            return radio_set_cell(x, (t_float)idx, fval);
        }
    }
    // case 3: multiple columns, one row
    if (nrows == 1) {
        if (row > 1)
            return pd_error(x, "radio_get: Illegal arguments.");
        if (col < 0) return radio_set_row(x, 1.0, fval);
        else {
            if (!radio_coord2idx(x, col, 1, &idx))
                return pd_error(x, "radio_get: Bad index (%d,%d)", col, row);
            return radio_set_cell(x, (t_float)idx, fval);
        }
    }
    // case 4: multiple columns, multiple rows
    if(col < 0 && row < 0) return radio_fill(x, fval);
    else if (col < 0) return radio_set_row(x, (t_float)row, fval);
    else if (row < 0) return radio_set_column(x, (t_float)col, fval);
    else {
        if (!radio_coord2idx(x, col, row, &idx))
            return pd_error(x, "radio_get: Bad index (%d,%d)", col, row);
        return radio_set_cell(x, (t_float)idx, fval);
    }
}

static void radio_clear(t_radio *x)
{
    return(radio_fill(x, 0));
}

static void radio_click(t_radio *x, t_floatarg xpos, t_floatarg ypos, t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    const int ncols = x->x_number[(int)x->x_orientation];
    const int nrows = x->x_number[!(int)x->x_orientation];
    const int xx = (int)xpos - (int)text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist);
    const int yy = (int)ypos - (int)text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);
    const int col = clip_int(xx / (float)x->x_gui.x_w, 0, ncols);
    const int row = clip_int(yy / (float)x->x_gui.x_h, 0, nrows);
    int idx;
    if(!radio_coord2idx(x, col, row, &idx))
        return(pd_error(x, "Bad index."));

    if (!shift)
        return(radio_fout(x, (t_float)(idx % (x->x_direction == vertical ? nrows : ncols))));
    else {
        x->x_matrix[idx] = (!x->x_matrix[idx]) ? 1 : 0;
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    }
}

static int radio_newclick(t_gobj *z, struct _glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if(doit)
        radio_click((t_radio *)z, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift, 0, (t_floatarg)alt);
    return (1);
}

static void radio_loadbang(t_radio *x, t_floatarg action)
{
    if(action == LB_LOAD && x->x_gui.x_isa.x_loadinit)
        radio_bang(x);
}

static void radio_number(t_radio *x, t_floatarg num)
{
    radio_resize(x, num, 1.0);
    x->x_on = clip_int(x->x_on, 1, x->x_number[(int)x->x_orientation]);
    x->x_on_old = x->x_on;
}

static void radio_orientation(t_radio *x, t_floatarg forient)
{
    const int ncols = x->x_number[(int)x->x_orientation];
    const int nrows = x->x_number[!(int)x->x_orientation];
    if (ncols == nrows || (ncols == 1 && nrows == 1))
        return;

    t_iem_orientation orientation = horizontal;
    if ((int)forient != 0)
        orientation = vertical;

    if (x->x_orientation == orientation)
        return;

    x->x_orientation = orientation;
    radio_matrix_reindex(x);
    iemgui_size(x, &x->x_gui);
}

static void radio_direction(t_radio *x, t_floatarg fdirect)
{
    t_iem_orientation direction = horizontal;
    if ((int)fdirect != 0)
        direction = vertical;
    if (x->x_direction == direction)
        return;
    x->x_direction = direction;
    radio_matrix_reindex(x);
}

static void radio_mode(t_radio *x, t_floatarg foutputmode)
{
    int output_mode = clip_int((int)foutputmode, 0, 3);
    if (x->x_output_mode == output_mode)
        return;
    x->x_output_mode = output_mode;
    post("output_mode: %d", x->x_output_mode);
}

static void radio_size(t_radio *x, t_symbol *s, int ac, t_atom *av)
{
    const int size = (int)atom_getfloatarg(0, ac, av) * IEMGUI_ZOOM(x);
    x->x_gui.x_w = iemgui_clip_size(size);
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_size((void *)x, &x->x_gui);
}

static void radio_delta(t_radio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_delta((void *)x, &x->x_gui, s, ac, av);}

static void radio_pos(t_radio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_pos((void *)x, &x->x_gui, s, ac, av);}

static void radio_color(t_radio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_color((void *)x, &x->x_gui, s, ac, av);}

static void radio_send(t_radio *x, t_symbol *s)
{iemgui_send(x, &x->x_gui, s);}

static void radio_receive(t_radio *x, t_symbol *s)
{iemgui_receive(x, &x->x_gui, s);}

static void radio_label(t_radio *x, t_symbol *s)
{iemgui_label((void *)x, &x->x_gui, s);}

static void radio_label_pos(t_radio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_pos((void *)x, &x->x_gui, s, ac, av);}

static void radio_label_font(t_radio *x, t_symbol *s, int ac, t_atom *av)
{iemgui_label_font((void *)x, &x->x_gui, s, ac, av);}

static void radio_init(t_radio *x, t_floatarg f)
{x->x_gui.x_isa.x_loadinit = (f == 0.0) ? 0 : 1;}

static void radio_double_change(t_radio *x)
{
    if(x->x_compat)
        x->x_change = 1;
    else
        pd_error(x, "radio: no method for 'double_change'");
}

static void radio_single_change(t_radio *x)
{
    if(x->x_compat)
        x->x_change = 0;
    else
        pd_error(x, "radio: no method for 'single_change'");
}

static void *radio_donew(t_symbol *s, int argc, t_atom *argv, int old)
{
    t_radio *x = (t_radio *)iemgui_new(radio_class);
    int a = IEM_GUI_DEFAULTSIZE, on = 0;
    int ldx = 0, ldy = -8 * IEM_GUI_DEFAULTSIZE_SCALE, chg = 1, num = 8;
    int fs = x->x_gui.x_fontsize;
    t_float fval = 0;

    t_iem_orientation arg_orient = ('v' == *s->s_name) ? vertical : horizontal;

    x->x_compat = old;

    IEMGUI_SETDRAWFUNCTIONS(x, radio);

    if((argc == 15)&&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)&&IS_A_FLOAT(argv,2)
       &&IS_A_FLOAT(argv,3)
       &&(IS_A_SYMBOL(argv,4)||IS_A_FLOAT(argv,4))
       &&(IS_A_SYMBOL(argv,5)||IS_A_FLOAT(argv,5))
       &&(IS_A_SYMBOL(argv,6)||IS_A_FLOAT(argv,6))
       &&IS_A_FLOAT(argv,7)&&IS_A_FLOAT(argv,8)
       &&IS_A_FLOAT(argv,9)&&IS_A_FLOAT(argv,10)&&IS_A_FLOAT(argv,14))
    {
        a = (int)atom_getfloatarg(0, argc, argv);
        chg = (int)atom_getfloatarg(1, argc, argv);
        iem_inttosymargs(&x->x_gui.x_isa, atom_getfloatarg(2, argc, argv));
        num = (int)atom_getfloatarg(3, argc, argv);
        iemgui_new_getnames(&x->x_gui, 4, argv);
        ldx = (int)atom_getfloatarg(7, argc, argv);
        ldy = (int)atom_getfloatarg(8, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getfloatarg(9, argc, argv));
        fs = (int)atom_getfloatarg(10, argc, argv);
        iemgui_all_loadcolors(&x->x_gui, argv+11, argv+12, argv+13);
        fval = atom_getfloatarg(14, argc, argv);
    }
    else iemgui_new_getnames(&x->x_gui, 4, 0);
    x->x_gui.x_fsf.x_snd_able = (0 != x->x_gui.x_snd);
    x->x_gui.x_fsf.x_rcv_able = (0 != x->x_gui.x_rcv);
    if(x->x_gui.x_fsf.x_font_style == 1) strcpy(x->x_gui.x_font, "helvetica");
    else if(x->x_gui.x_fsf.x_font_style == 2) strcpy(x->x_gui.x_font, "times");
    else { x->x_gui.x_fsf.x_font_style = 0;
        strcpy(x->x_gui.x_font, sys_font); }

    /* if it's a matrix, decode size, direction, and infer and set orientation */
    int *ncols = &x->x_number[0];
    int *nrows = &x->x_number[1];
    t_iem_orientation *direction = &x->x_direction;
    if(radio_decode_extended(num, ncols, nrows, direction))
        x->x_orientation = (*ncols < *nrows) ? vertical : horizontal;
    else {
        /* we need to update the size and the orientation */
        *ncols = (arg_orient == horizontal) ? num : 1;
        *nrows = (arg_orient == horizontal) ? 1 : num;
        x->x_orientation = horizontal; // needs to be horizontal by default
    }
    x->x_output_mode = 0; // default to normal index output

    x->x_fval = fval;
    on = fval;
    if(on < 0)
        on = 0;
    if(on >= *ncols)
        on = (*ncols) - 1;
    if(x->x_gui.x_isa.x_loadinit)
        x->x_on = on;
    else
        x->x_on = 0;
    x->x_on_old = x->x_on;
    x->x_change = (chg == 0) ? 0 : 1;

    if(!radio_matrix_initialize(x))
        pd_error(x, "Could not initialize matrix.");

    if(x->x_gui.x_fsf.x_rcv_able)
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;
    x->x_gui.x_fontsize = (fs < 4)?4:fs;
    x->x_gui.x_w = iemgui_clip_size(a);
    x->x_gui.x_h = x->x_gui.x_w;
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    iemgui_newzoom(&x->x_gui);
    outlet_new(&x->x_gui.x_obj, &s_list);
    return (x);
}

static void *radio_new(t_symbol *s, int argc, t_atom *argv)
{
    return (radio_donew(s, argc, argv, 0));
}

static void *dial_new(t_symbol *s, int argc, t_atom *argv)
{
    return (radio_donew(s, argc, argv, 1));
}

static void radio_free(t_radio *x) {
    size_t nbytes = x->x_number[0] * x->x_number[1] * sizeof(int);
    freebytes(x->x_matrix, nbytes);
    freebytes(x->x_matrix_idx, nbytes);
    iemgui_free((t_iemgui *)x);
}

void g_radio_setup(void)
{
    radio_class = class_new(gensym("hradio"), (t_newmethod)radio_new,
        (t_method)radio_free, sizeof(t_radio), 0, A_GIMME, 0);
    class_addcreator((t_newmethod)radio_new, gensym("vradio"), A_GIMME, 0);
    class_addcreator((t_newmethod)radio_new, gensym("rdb"), A_GIMME, 0);
    class_addcreator((t_newmethod)radio_new, gensym("radiobut"), A_GIMME, 0);
    class_addcreator((t_newmethod)radio_new, gensym("radiobutton"), A_GIMME, 0);

    class_addbang(radio_class, radio_bang);
    class_addfloat(radio_class, radio_float);
    class_addmethod(radio_class, (t_method)radio_click,
        gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_dialog,
        gensym("dialog"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_clear,
        gensym("clear"), A_NULL);
    class_addmethod(radio_class, (t_method)radio_loadbang,
        gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_set,
        gensym("set"), A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_matrix_set,
        gensym("set"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_size,
        gensym("size"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_delta,
        gensym("delta"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_pos,
        gensym("pos"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_color,
        gensym("color"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_send,
        gensym("send"), A_DEFSYM, 0);
    class_addmethod(radio_class, (t_method)radio_receive,
        gensym("receive"), A_DEFSYM, 0);
    class_addmethod(radio_class, (t_method)radio_label,
        gensym("label"), A_DEFSYM, 0);
    class_addmethod(radio_class, (t_method)radio_label_pos,
        gensym("label_pos"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_label_font,
        gensym("label_font"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_init,
        gensym("init"), A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_number,
        gensym("number"), A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_resize,
        gensym("resize"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_orientation,
        gensym("orientation"), A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_direction,
        gensym("direction"), A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)iemgui_zoom,
        gensym("zoom"), A_CANT, 0);
    // class_addmethod(radio_class, (t_method)radio_focus,
    //     gensym("focus"), A_FLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_mode,
        gensym("mode"), A_DEFFLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_spit,
        gensym("spit"), A_NULL);
    class_addmethod(radio_class, (t_method)radio_fill,
        gensym("fill"), A_DEFFLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_line,
        gensym("line"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_fill_line,
        gensym("line"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_get,
        gensym("get"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(radio_class, (t_method)radio_cell,
        gensym("cell"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_row,
        gensym("row"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_column,
        gensym("column"), A_GIMME, 0);
    radio_widgetbehavior.w_getrectfn = radio_getrect;
    radio_widgetbehavior.w_displacefn = iemgui_displace;
    radio_widgetbehavior.w_selectfn = iemgui_select;
    radio_widgetbehavior.w_activatefn = NULL;
    radio_widgetbehavior.w_deletefn = iemgui_delete;
    radio_widgetbehavior.w_visfn = iemgui_vis;
    radio_widgetbehavior.w_clickfn = radio_newclick;
    class_setwidget(radio_class, &radio_widgetbehavior);

    class_sethelpsymbol(radio_class, gensym("radio"));
    class_setsavefn(radio_class, radio_save);
    class_setpropertiesfn(radio_class, radio_properties);

        /* obsolete version (0.34-0.35) */
    class_addcreator((t_newmethod)dial_new, gensym("hdl"), A_GIMME, 0);
    class_addcreator((t_newmethod)dial_new, gensym("vdl"), A_GIMME, 0);
    class_addmethod(radio_class, (t_method)radio_single_change,
        gensym("single_change"), 0);
    class_addmethod(radio_class, (t_method)radio_double_change,
        gensym("double_change"), 0);
    post("under construction...");
}
