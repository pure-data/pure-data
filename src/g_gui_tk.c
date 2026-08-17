/* Copyright (c) 2026 The Pure Data Team.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

#include "m_pd.h"
#include "g_gui.h"
#include "s_inter_gui.h"
#include "s_stuff.h"
#include <stdio.h>
#include <string.h>

/* The default renderer backend.  Tcl/Tk details stay on this side of the
 * typed drawing API. */
static int pdgui_tk_backend_init(const char *libdir)
{
    return (pdgui_tk_transport_init(libdir));
}

static int pdgui_tk_backend_poll(void)
{
    return (0);
}

static void pdgui_tk_rect_create(t_canvas *canvas, const char *item,
    const char *group, int x1, int y1, int x2, int y2, int width,
    unsigned int fill, unsigned int outline)
{
    const char *tags[2];
    tags[0] = item;
    tags[1] = group;
    if (fill == PDGUI_COLOR_NONE)
        pdgui_vmess(0, "crr iiii ri rs rk rS", canvas, "create",
            "rectangle", x1, y1, x2, y2, "-width", width, "-fill", "",
            "-outline", (int)outline, "-tags", 2, tags);
    else
        pdgui_vmess("pdtk_canvas_create_rect", "crri kk iiii", canvas,
            item, group, width, (int)fill, (int)outline, x1, y1, x2, y2);
}

static void pdgui_tk_rect_create_grouped(t_canvas *canvas, const char *item,
    const char *group, const char *collection, int x1, int y1, int x2,
    int y2, int width, unsigned int fill, unsigned int outline)
{
    const char *tags[3];
    tags[0] = group;
    tags[1] = collection;
    tags[2] = item;
    if (fill == PDGUI_COLOR_NONE)
        pdgui_vmess(0, "crr iiii ri rs rk rS", canvas, "create",
            "rectangle", x1, y1, x2, y2, "-width", width, "-fill", "",
            "-outline", (int)outline, "-tags", 3, tags);
    else
        pdgui_vmess(0, "crr iiii ri rk rk rS", canvas, "create",
            "rectangle", x1, y1, x2, y2, "-width", width, "-fill",
            (int)fill, "-outline", (int)outline, "-tags", 3, tags);
}

static void pdgui_tk_rect_configure(t_canvas *canvas, const char *item,
    int x1, int y1, int x2, int y2, int width, unsigned int fill,
    unsigned int outline)
{
    pdgui_vmess(0, "crs iiii", canvas, "coords", item, x1, y1, x2, y2);
    if (fill == PDGUI_COLOR_NONE)
        pdgui_vmess(0, "crs ri rs rk", canvas, "itemconfigure", item,
            "-width", width, "-fill", "", "-outline", (int)outline);
    else
        pdgui_vmess(0, "crs ri rk rk", canvas, "itemconfigure", item,
            "-width", width, "-fill", (int)fill, "-outline", (int)outline);
}

static void pdgui_tk_rect_set_outline(t_canvas *canvas, const char *item,
    unsigned int outline)
{
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", item, "-outline",
        (int)outline);
}

static void pdgui_tk_rect_set_style(t_canvas *canvas, const char *item,
    int width, unsigned int fill, unsigned int outline)
{
    if (fill == PDGUI_COLOR_NONE)
        pdgui_vmess(0, "crs ri rs rk", canvas, "itemconfigure", item,
            "-width", width, "-fill", "", "-outline", (int)outline);
    else
        pdgui_vmess(0, "crs ri rk rk", canvas, "itemconfigure", item,
            "-width", width, "-fill", (int)fill, "-outline", (int)outline);
}

static void pdgui_tk_rect_set_bounds(t_canvas *canvas, const char *item,
    int x1, int y1, int x2, int y2)
{
    pdgui_vmess(0, "crs iiii", canvas, "coords", item, x1, y1, x2, y2);
}

static void pdgui_tk_oval_create(t_canvas *canvas, const char *item,
    const char *group, int x1, int y1, int x2, int y2, int width,
    unsigned int fill, unsigned int outline)
{
    pdgui_vmess("pdtk_canvas_create_oval", "crri kk iiii", canvas, item,
        group, width, (int)fill, (int)outline, x1, y1, x2, y2);
}

static void pdgui_tk_oval_configure(t_canvas *canvas, const char *item,
    int x1, int y1, int x2, int y2, int width, unsigned int fill,
    unsigned int outline)
{
    pdgui_vmess(0, "crs iiii", canvas, "coords", item, x1, y1, x2, y2);
    pdgui_vmess(0, "rcs ikk", "pdtk_canvas_configure_rect", canvas, item,
        width, (int)fill, (int)outline);
}

static void pdgui_tk_oval_set_style(t_canvas *canvas, const char *item,
    int width, unsigned int fill, unsigned int outline)
{
    pdgui_vmess(0, "rcs ikk", "pdtk_canvas_configure_rect", canvas, item,
        width, (int)fill, (int)outline);
}

static void pdgui_tk_line_set_style(t_canvas *canvas, const char *item,
    int width, unsigned int color)
{
    pdgui_vmess("pdtk_canvas_configure_line", "cs ik", canvas, item,
        width, (int)color);
}

static t_atom *pdgui_tk_coords_to_atoms(const int *coords, int ncoords)
{
    int i;
    t_atom *atoms = (t_atom *)getbytes(ncoords * sizeof(*atoms));
    for (i = 0; i < ncoords; i++)
        SETFLOAT(atoms + i, coords[i]);
    return (atoms);
}

static void pdgui_tk_polyline_configure(t_canvas *canvas, const char *item,
    const int *coords, int ncoords, int width, unsigned int color)
{
    t_atom *atoms = pdgui_tk_coords_to_atoms(coords, ncoords);
    pdgui_vmess(0, "crs a", canvas, "coords", item, ncoords, atoms);
    pdgui_vmess("pdtk_canvas_configure_line", "cs ik", canvas, item,
        width, (int)color);
    freebytes(atoms, ncoords * sizeof(*atoms));
}

static void pdgui_tk_polyline_create_dashed(t_canvas *canvas,
    const char *item, const char *group, const int *coords, int ncoords,
    int width, unsigned int color, int dashed)
{
    t_atom *atoms = pdgui_tk_coords_to_atoms(coords, ncoords);
    pdgui_vmess("pdtk_canvas_create_line", "crr iik a", canvas, item,
        group, dashed, width, (int)color, ncoords, atoms);
    freebytes(atoms, ncoords * sizeof(*atoms));
}

static void pdgui_tk_polyline_set_points(t_canvas *canvas, const char *item,
    const int *coords, int ncoords)
{
    t_atom *atoms = pdgui_tk_coords_to_atoms(coords, ncoords);
    pdgui_vmess(0, "crs a", canvas, "coords", item, ncoords, atoms);
    freebytes(atoms, ncoords * sizeof(*atoms));
}

static void pdgui_tk_polygon_create(t_canvas *canvas, const char *item,
    const char *group, const int *coords, int ncoords, int width,
    unsigned int fill, unsigned int outline)
{
    const char *tags[2];
    t_atom *atoms = pdgui_tk_coords_to_atoms(coords, ncoords);
    tags[0] = item;
    tags[1] = group;
    pdgui_vmess(0, "crr a ri rk rk rS", canvas, "create", "polygon",
        ncoords, atoms, "-width", width, "-fill", (int)fill, "-outline",
        (int)outline, "-tags", 2, tags);
    freebytes(atoms, ncoords * sizeof(*atoms));
}

static void pdgui_tk_polygon_configure(t_canvas *canvas, const char *item,
    const int *coords, int ncoords, int width, unsigned int fill,
    unsigned int outline)
{
    t_atom *atoms = pdgui_tk_coords_to_atoms(coords, ncoords);
    pdgui_vmess(0, "crs a", canvas, "coords", item, ncoords, atoms);
    pdgui_vmess(0, "crs ri rk rk", canvas, "itemconfigure", item,
        "-width", width, "-fill", (int)fill, "-outline", (int)outline);
    freebytes(atoms, ncoords * sizeof(*atoms));
}

static void pdgui_tk_polygon_create_miter(t_canvas *canvas,
    const char *item, const char *group, const int *coords, int ncoords,
    int width, unsigned int fill, unsigned int outline)
{
    const char *tags[2];
    t_atom *atoms = pdgui_tk_coords_to_atoms(coords, ncoords);
    tags[0] = item;
    tags[1] = group;
    pdgui_vmess(0, "crr a ri rk rs rS", canvas, "create", "polygon",
        ncoords, atoms, "-width", width, "-fill", (int)fill,
        "-joinstyle", "miter", "-tags", 2, tags);
    freebytes(atoms, ncoords * sizeof(*atoms));
    (void)outline;
}

static void pdgui_tk_path_create(t_canvas *canvas, const char *item,
    const t_word *coords, int ncoords, int closed, int smooth, int width,
    unsigned int fill, unsigned int outline)
{
    pdgui_vmess("pdtk_canvas_create_poly", "cr iif kk iiii", canvas, item,
        closed, smooth, width, (int)fill, (int)outline, 0, 0, 0, 0);
    pdgui_vmess(0, "crs w", canvas, "coords", item, ncoords, coords);
}

static void pdgui_tk_path_set_points(t_canvas *canvas, const char *item,
    const t_word *coords, int ncoords)
{
    pdgui_vmess(0, "crs w", canvas, "coords", item, ncoords, coords);
}

static void pdgui_tk_text_create(t_canvas *canvas, const char *item,
    const char *group, int x, int y)
{
    const char *tags[4];
    tags[0] = group;
    tags[1] = item;
    tags[2] = "label";
    tags[3] = "text";
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text", x, y,
        "-anchor", "w", "-tags", 4, tags);
}

static void pdgui_tk_text_create_plain(t_canvas *canvas, const char *item,
    const char *group, int x, int y)
{
    const char *tags[2];
    tags[0] = group;
    tags[1] = item;
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text", x, y,
        "-anchor", "w", "-tags", 2, tags);
}

static void pdgui_tk_text_create_grouped(t_canvas *canvas, const char *item,
    const char *group, const char *collection, int x, int y)
{
    const char *tags[4];
    tags[0] = group;
    tags[1] = collection;
    tags[2] = item;
    tags[3] = "text";
    pdgui_vmess(0, "crr ii rs rS", canvas, "create", "text", x, y,
        "-anchor", "w", "-tags", 4, tags);
}

static const char *pdgui_tk_anchor(t_pdgui_anchor anchor)
{
    static const char *anchors[] =
        {"center", "n", "s", "e", "w", "ne", "nw", "se", "sw"};
    return (anchors[(int)anchor]);
}

static void pdgui_tk_text_create_anchored(t_canvas *canvas,
    const char *item, const char *group, const char *collection, int x, int y,
    const char *text, t_pdgui_anchor anchor, const char *font, int fontsize,
    const char *weight, unsigned int color)
{
    const char *tags[3];
    t_atom fontatoms[3];
    tags[0] = item;
    tags[1] = group;
    tags[2] = collection;
    SETSYMBOL(fontatoms, gensym(font));
    SETFLOAT(fontatoms + 1, -fontsize);
    SETSYMBOL(fontatoms + 2, gensym(weight));
    pdgui_vmess(0, "crr ii rs rk rr rA rS", canvas, "create", "text",
        x, y, "-text", text, "-fill", (int)color, "-anchor",
        pdgui_tk_anchor(anchor), "-font", 3, fontatoms, "-tags", 3, tags);
}

static void pdgui_tk_text_configure(t_canvas *canvas, const char *item,
    int x, int y, const char *font, int fontsize, const char *weight,
    unsigned int color)
{
    t_atom fontatoms[3];
    SETSYMBOL(fontatoms, gensym(font));
    SETFLOAT(fontatoms + 1, -fontsize);
    SETSYMBOL(fontatoms + 2, gensym(weight));
    pdgui_vmess(0, "crs ii", canvas, "coords", item, x, y);
    pdgui_vmess(0, "crs rA rk", canvas, "itemconfigure", item, "-font",
        3, fontatoms, "-fill", (int)color);
}

static void pdgui_tk_text_set_color(t_canvas *canvas, const char *item,
    unsigned int color)
{
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", item, "-fill",
        (int)color);
}

static void pdgui_tk_text_set_position(t_canvas *canvas, const char *item,
    int x, int y)
{
    pdgui_vmess(0, "crs ii", canvas, "coords", item, x, y);
}

static void pdgui_tk_text_set_content(t_canvas *canvas, const char *item,
    const char *text)
{
    pdgui_vmess("pdtk_text_set", "cs s", canvas, item, text);
}

static void pdgui_tk_text_set_font(t_canvas *canvas, const char *item,
    const char *font, int fontsize, const char *weight)
{
    t_atom fontatoms[3];
    SETSYMBOL(fontatoms, gensym(font));
    SETFLOAT(fontatoms + 1, -fontsize);
    SETSYMBOL(fontatoms + 2, gensym(weight));
    pdgui_vmess(0, "crs rA", canvas, "itemconfigure", item, "-font",
        3, fontatoms);
}

static void pdgui_tk_text_set_content_color(t_canvas *canvas,
    const char *item, const char *text, unsigned int color)
{
    pdgui_vmess(0, "crs rk rs", canvas, "itemconfigure", item, "-fill",
        (int)color, "-text", text);
}

static void pdgui_tk_canvas_text_create(t_canvas *canvas, const char *item,
    int x, int y, const char *text, int fontsize, unsigned int color)
{
    const char *tags[2];
    tags[0] = item;
    tags[1] = "text";
    pdgui_vmess("pdtk_text_new", "c S ii s i k", canvas, 2, tags,
        x, y, text, fontsize, (int)color);
}

static void pdgui_tk_canvas_text_create_grouped(t_canvas *canvas,
    const char *item, const char *collection, int x, int y,
    const char *text, int fontsize, unsigned int color)
{
    const char *tags[2];
    tags[0] = item;
    tags[1] = collection;
    pdgui_vmess("pdtk_text_new", "cS iis i k", canvas, 2, tags,
        x, y, text, fontsize, (int)color);
}

static void pdgui_tk_canvas_text_create_label(t_canvas *canvas,
    const char *item, int x, int y, const char *text, int fontsize,
    unsigned int color)
{
    const char *tags[3];
    tags[0] = item;
    tags[1] = "label";
    tags[2] = "text";
    pdgui_vmess("pdtk_text_new", "cS ii s ik", canvas, 3, tags,
        x, y, text, fontsize, (int)color);
}

static void pdgui_tk_text_set_selection(t_canvas *canvas, const char *item,
    int start, int end)
{
    pdgui_vmess("pdtk_text_select", "cs i i", canvas, item, start, end);
}

static void pdgui_tk_text_set_editing(t_canvas *canvas, const char *item,
    int state)
{
    pdgui_vmess("pdtk_text_editing", "^si", canvas, item, state);
}

static void pdgui_tk_item_destroy(t_canvas *canvas, const char *item)
{
    pdgui_vmess("pdtk_canvas_delete", "cs", canvas, item);
}

static void pdgui_tk_item_move(t_canvas *canvas, const char *item,
    int dx, int dy)
{
    pdgui_vmess(0, "crs ii", canvas, "move", item, dx, dy);
}

static void pdgui_tk_item_lower(t_canvas *canvas, const char *item,
    const char *below)
{
    pdgui_vmess(0, "crss", canvas, "lower", item, below);
}

static void pdgui_tk_item_raise(t_canvas *canvas, const char *item,
    const char *above)
{
    pdgui_vmess(0, "crss", canvas, "raise", item, above);
}

static void pdgui_tk_item_set_fill(t_canvas *canvas, const char *item,
    unsigned int fill)
{
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", item, "-fill",
        (int)fill);
}

static void pdgui_tk_item_set_outline(t_canvas *canvas, const char *item,
    unsigned int outline)
{
    pdgui_vmess(0, "crs rk", canvas, "itemconfigure", item, "-outline",
        (int)outline);
}

static void pdgui_tk_item_set_width(t_canvas *canvas, const char *item,
    int width)
{
    pdgui_vmess(0, "crs ri", canvas, "itemconfigure", item, "-width",
        width);
}

static void pdgui_tk_item_raise_top(t_canvas *canvas, const char *item)
{
    pdgui_vmess(0, "crr", canvas, "raise", item);
}

static void pdgui_tk_canvas_clear(t_canvas *canvas)
{
    pdgui_vmess("pdtk_canvas_delete", "cs", canvas, "all");
}

static void pdgui_tk_canvas_set_colors(t_canvas *canvas,
    unsigned int background, unsigned int foreground)
{
    pdgui_vmess("pdtk_canvas_setcolors", "^ kk", canvas,
        (int)background, (int)foreground);
}

static void pdgui_tk_canvas_set_patchcords_foreground(t_canvas *canvas,
    int state)
{
    pdgui_vmess("::pdtk_canvas::cords_to_foreground", "ci", canvas, state);
}

static void pdgui_tk_patchcord_create(t_canvas *canvas, const char *item,
    int x1, int y1, int x2, int y2, int width, unsigned int color)
{
    pdgui_vmess("pdtk_canvas_create_patchcord", "crrr ik iiii", canvas,
        item, "-", "-", width, (int)color, x1, y1, x2, y2);
}

/* Keep the backend contract small.  These adapters combine the public
 * convenience operations into the primitive operations another renderer
 * must implement. */
static void pdgui_tk_backend_rect_create(t_canvas *canvas, const char *item,
    const char *group, const char *collection, int x1, int y1, int x2,
    int y2, int width, unsigned int fill, unsigned int outline)
{
    if (collection)
        pdgui_tk_rect_create_grouped(canvas, item, group, collection,
            x1, y1, x2, y2, width, fill, outline);
    else pdgui_tk_rect_create(canvas, item, group, x1, y1, x2, y2,
        width, fill, outline);
}

static void pdgui_tk_backend_rect_update(t_canvas *canvas, const char *item,
    int changes, int x1, int y1, int x2, int y2, int width,
    unsigned int fill, unsigned int outline)
{
    if (changes == PDGUI_CHANGE_POINTS)
        pdgui_tk_rect_set_bounds(canvas, item, x1, y1, x2, y2);
    else if (changes == PDGUI_CHANGE_OUTLINE)
        pdgui_tk_rect_set_outline(canvas, item, outline);
    else if (changes & PDGUI_CHANGE_POINTS)
        pdgui_tk_rect_configure(canvas, item, x1, y1, x2, y2, width,
            fill, outline);
    else pdgui_tk_rect_set_style(canvas, item, width, fill, outline);
}

static void pdgui_tk_backend_oval_update(t_canvas *canvas, const char *item,
    int changes, int x1, int y1, int x2, int y2, int width,
    unsigned int fill, unsigned int outline)
{
    if (changes & PDGUI_CHANGE_POINTS)
        pdgui_tk_oval_configure(canvas, item, x1, y1, x2, y2, width,
            fill, outline);
    else pdgui_tk_oval_set_style(canvas, item, width, fill, outline);
}

static void pdgui_tk_backend_line_create(t_canvas *canvas, const char *item,
    const char *group, const int *coords, int ncoords, int width,
    unsigned int color, int dashed)
{
    pdgui_tk_polyline_create_dashed(canvas, item, group, coords, ncoords,
        width, color, dashed);
}

static void pdgui_tk_backend_line_update(t_canvas *canvas, const char *item,
    int changes, const int *coords, int ncoords, int width,
    unsigned int color)
{
    if ((changes & PDGUI_CHANGE_POINTS) && ncoords == 4)
    {
        pdgui_vmess(0, "crs iiii", canvas, "coords", item, coords[0],
            coords[1], coords[2], coords[3]);
        if (changes & (PDGUI_CHANGE_WIDTH | PDGUI_CHANGE_COLOR))
            pdgui_tk_line_set_style(canvas, item, width, color);
    }
    else if ((changes & PDGUI_CHANGE_POINTS) &&
        (changes & (PDGUI_CHANGE_WIDTH | PDGUI_CHANGE_COLOR)))
        pdgui_tk_polyline_configure(canvas, item, coords, ncoords, width,
            color);
    else if (changes & PDGUI_CHANGE_POINTS)
        pdgui_tk_polyline_set_points(canvas, item, coords, ncoords);
    else pdgui_tk_line_set_style(canvas, item, width, color);
}

static void pdgui_tk_backend_polygon_create(t_canvas *canvas,
    const char *item, const char *group, const int *coords, int ncoords,
    int width, unsigned int fill, unsigned int outline, int miter)
{
    if (miter)
        pdgui_tk_polygon_create_miter(canvas, item, group, coords, ncoords,
            width, fill, outline);
    else pdgui_tk_polygon_create(canvas, item, group, coords, ncoords,
        width, fill, outline);
}

static void pdgui_tk_backend_polygon_update(t_canvas *canvas,
    const char *item, int changes, const int *coords, int ncoords, int width,
    unsigned int fill, unsigned int outline)
{
    pdgui_tk_polygon_configure(canvas, item, coords, ncoords, width, fill,
        outline);
    (void)changes;
}

static void pdgui_tk_backend_text_update(t_canvas *canvas, const char *item,
    int changes, int x, int y, const char *text, const char *font,
    int fontsize, const char *weight, unsigned int color)
{
    if (changes == (PDGUI_CHANGE_POINTS | PDGUI_CHANGE_FONT |
        PDGUI_CHANGE_COLOR))
        pdgui_tk_text_configure(canvas, item, x, y, font, fontsize, weight,
            color);
    else if (changes == (PDGUI_CHANGE_CONTENT | PDGUI_CHANGE_COLOR))
        pdgui_tk_text_set_content_color(canvas, item, text, color);
    else
    {
        if (changes & PDGUI_CHANGE_POINTS)
            pdgui_tk_text_set_position(canvas, item, x, y);
        if (changes & PDGUI_CHANGE_FONT)
            pdgui_tk_text_set_font(canvas, item, font, fontsize, weight);
        if (changes & PDGUI_CHANGE_CONTENT)
            pdgui_tk_text_set_content(canvas, item, text);
        if (changes & PDGUI_CHANGE_COLOR)
            pdgui_tk_text_set_color(canvas, item, color);
    }
}

static void pdgui_tk_backend_item_order(t_canvas *canvas, const char *item,
    t_pdgui_order order, const char *relative)
{
    if (order == PDGUI_ORDER_BELOW)
        pdgui_tk_item_lower(canvas, item, relative);
    else if (order == PDGUI_ORDER_ABOVE)
        pdgui_tk_item_raise(canvas, item, relative);
    else pdgui_tk_item_raise_top(canvas, item);
}

static void pdgui_tk_backend_item_style(t_canvas *canvas, const char *item,
    int changes, int width, unsigned int fill, unsigned int outline)
{
    if (changes & PDGUI_CHANGE_WIDTH)
        pdgui_tk_item_set_width(canvas, item, width);
    if (changes & PDGUI_CHANGE_FILL)
        pdgui_tk_item_set_fill(canvas, item, fill);
    if (changes & PDGUI_CHANGE_OUTLINE)
        pdgui_tk_item_set_outline(canvas, item, outline);
}

static void pdgui_tk_api_list(char *buf, size_t size, int count,
    const char *const *names, const int *ids)
{
    int i;
    size_t used;
    char escaped[MAXPDSTRING];
    if (count <= 0)
    {
        pd_snprintf(buf, size, "{}");
        return;
    }
    pd_snprintf(buf, size, "{ ");
    for (i = 0; i < count; i++)
    {
        pdgui_strnescape(escaped, sizeof(escaped), names[i], 0);
        used = strlen(buf);
        if (used < size)
            pd_snprintf(buf + used, size - used, "{{%s} %d} ",
                escaped, ids[i]);
    }
    used = strlen(buf);
    if (used < size)
        pd_snprintf(buf + used, size - used, "}");
}

static void pdgui_tk_service(t_pdgui_service service,
    const t_pdgui_service_request *r)
{
    static const char *cursors[] =
    {
        "$cursor_runmode_nothing", "$cursor_runmode_clickme",
        "$cursor_runmode_thicken", "$cursor_runmode_addpoint",
        "$cursor_editmode_nothing", "$cursor_editmode_connect",
        "$cursor_editmode_disconnect", "$cursor_editmode_resize"
    };
    switch (service)
    {
    case PDGUI_SERVICE_WINDOW_MENU_UPDATE:
        pdgui_vmess("::pd_menus::update_window_menu", 0); break;
    case PDGUI_SERVICE_CANVAS_TITLE:
        pdgui_vmess("pdtk_canvas_reflecttitle", "^ sss ii", r->sr_canvas,
            r->sr_strings[0], r->sr_strings[1], r->sr_strings[2],
            r->sr_ints[0], r->sr_ints[1]); break;
    case PDGUI_SERVICE_CANVAS_SCROLL:
        pdgui_vmess("pdtk_canvas_getscroll", "c", r->sr_canvas); break;
    case PDGUI_SERVICE_DSP_STATE:
        pdgui_vmess("pdtk_pd_dsp", "s", r->sr_ints[0] ? "ON" : "OFF");
        break;
    case PDGUI_SERVICE_BUSY_RELEASE:
        pdgui_vmess("::pdwindow::busyrelease", 0); break;
    case PDGUI_SERVICE_STRUCT_MENU_CLEAR:
        pdgui_vmess("pdtk_newstructs", 0); break;
    case PDGUI_SERVICE_STRUCT_MENU_ADD:
        pdgui_vmess("pdtk_addstruct", "r", r->sr_strings[0]); break;
    case PDGUI_SERVICE_UNDO_MENU:
        if (r->sr_canvas)
            pdgui_vmess("pdtk_undomenu", "^ ss", r->sr_canvas,
                r->sr_strings[0], r->sr_strings[1]);
        else pdgui_vmess("pdtk_undomenu", "rss", "nobody",
            r->sr_strings[0], r->sr_strings[1]);
        break;
    case PDGUI_SERVICE_CLIPBOARD_SET:
        pdgui_vmess("clipboard", "r", "clear");
        pdgui_vmess("clipboard", "rp", "append", r->sr_ints[0],
            r->sr_strings[0]); break;
    case PDGUI_SERVICE_CANVAS_CURSOR:
        pdgui_vmess(0, "^r rr", r->sr_canvas, "configure", "-cursor",
            cursors[r->sr_ints[0]]); break;
    case PDGUI_SERVICE_CANVAS_RAISE:
        pdgui_vmess("pdtk_canvas_raise", "^", r->sr_canvas); break;
    case PDGUI_SERVICE_CANVAS_CREATE:
        if (r->sr_ints[5])
            pdgui_vmess("pdtk_canvas_new", "^ ii si kk", r->sr_canvas,
                r->sr_ints[0], r->sr_ints[1], r->sr_strings[0],
                r->sr_ints[2], r->sr_ints[3], r->sr_ints[4]);
        else pdgui_vmess("pdtk_canvas_new", "^ ii si", r->sr_canvas,
            r->sr_ints[0], r->sr_ints[1], r->sr_strings[0], r->sr_ints[2]);
        break;
    case PDGUI_SERVICE_CANVAS_PARENTS:
        pdgui_vmess("pdtk_canvas_setparents", "^C", r->sr_canvas,
            r->sr_ncanvases, r->sr_canvases); break;
    case PDGUI_SERVICE_WINDOW_DESTROY:
        if (r->sr_strings[0])
            pdgui_vmess("destroy", "s", r->sr_strings[0]);
        else pdgui_vmess("destroy", "^", r->sr_object);
        break;
    case PDGUI_SERVICE_CANVAS_DIALOG_TEXT:
        pdgui_vmess("::dialog_canvas::set_text", "s", r->sr_strings[0]);
        break;
    case PDGUI_SERVICE_CANVAS_POPUP:
        pdgui_vmess("pdtk_canvas_popup", "^ ii ii", r->sr_canvas,
            r->sr_ints[0], r->sr_ints[1], r->sr_ints[2], r->sr_ints[3]);
        break;
    case PDGUI_SERVICE_CANVAS_EXPORT:
        pdgui_vmess(0, "cr rs", r->sr_canvas, "postscript", "-file",
            r->sr_strings[0]); break;
    case PDGUI_SERVICE_CONFIRM:
        if (r->sr_canvas)
            pdgui_vmess("pdtk_check", "^ Sms", r->sr_canvas,
                r->sr_string_counts[0], r->sr_string_arrays[0], r->sr_symbol,
                r->sr_natoms, r->sr_atoms, r->sr_strings[1]);
        else
            pdgui_vmess("pdtk_check", "r Sss", ".pdwindow", 1,
                r->sr_strings, "pd quit", "yes");
        break;
    case PDGUI_SERVICE_CANVAS_CLOSE_CONFIRM:
        pdgui_vmess("pdtk_canvas_menuclose", "^m", r->sr_canvas,
            r->sr_symbol, r->sr_natoms, r->sr_atoms); break;
    case PDGUI_SERVICE_FIND_RESULT:
        pdgui_vmess("pdtk_showfindresult", "^ iii", r->sr_canvas,
            r->sr_ints[0], r->sr_ints[1], r->sr_ints[2]); break;
    case PDGUI_SERVICE_CANVAS_PASTE:
        pdgui_vmess(r->sr_ints[0] ? "pdtk_pastetext" : "pdtk_pasteany",
            "^", r->sr_canvas); break;
    case PDGUI_SERVICE_POINTER_POSITION:
        pdgui_vmess("::pdtk_canvas::setmouse", "cii", r->sr_canvas,
            r->sr_ints[0], r->sr_ints[1]); break;
    case PDGUI_SERVICE_PD_TEXTEDITOR:
        pdgui_vmess("pdtk_pd_texteditor", "p", r->sr_ints[0],
            r->sr_strings[0]); break;
    case PDGUI_SERVICE_CANVAS_EDITMODE:
        pdgui_vmess("pdtk_canvas_editmode", "^i", r->sr_canvas,
            r->sr_ints[0]); break;
    case PDGUI_SERVICE_ARRAY_PAGE:
        pdgui_vmess("::dialog_array::listview_setpage", "s iii",
            r->sr_strings[0], r->sr_ints[0], r->sr_ints[1], r->sr_ints[2]);
        break;
    case PDGUI_SERVICE_ARRAY_DATA:
        pdgui_vmess("::dialog_array::listview_setdata", "siw",
            r->sr_strings[0], r->sr_ints[0], r->sr_ints[1], r->sr_object);
        break;
    case PDGUI_SERVICE_ARRAY_FOCUS:
        pdgui_vmess("::dialog_array::listview_focus", "si",
            r->sr_strings[0], r->sr_ints[0]); break;
    case PDGUI_SERVICE_ARRAY_CLOSE:
        pdgui_vmess("pdtk_array_listview_closeWindow", "s",
            r->sr_strings[0]); break;
    case PDGUI_SERVICE_ARRAY_REFRESH:
        pdgui_vmess("pdtk_array_listview_fillpage", "s", r->sr_strings[0]);
        break;
    case PDGUI_SERVICE_MISSING_OBJECT:
        pdgui_vmess("::pdwindow::add_missingobject", "os", r->sr_object,
            r->sr_strings[0]); break;
    case PDGUI_SERVICE_CANVAS_SAVE_AS:
        pdgui_vmess("pdtk_canvas_saveas", "^ ss i", r->sr_canvas,
            r->sr_strings[0], r->sr_strings[1], r->sr_ints[0]); break;
    case PDGUI_SERVICE_CONSOLE_POST:
        pdgui_vmess("::pdwindow::post", "s", r->sr_strings[0]); break;
    case PDGUI_SERVICE_CONSOLE_LOG:
        pdgui_vmess("::pdwindow::logpost", "ois", r->sr_object,
            r->sr_ints[0], r->sr_strings[0]); break;
    case PDGUI_SERVICE_PLUGIN_DISPATCH:
        pdgui_vmess("pdtk_plugin_dispatch", "a", r->sr_natoms,
            r->sr_atoms); break;
    case PDGUI_SERVICE_PREFERENCES_OPEN:
        pdgui_vmess("::dialog_preferences::create", ""); break;
    case PDGUI_SERVICE_PING:
        pdgui_vmess("pdtk_ping", 0); break;
    case PDGUI_SERVICE_PREFERENCES_PATHS:
        pdgui_vmess("::dialog_path::set_paths", "SSS",
            r->sr_string_counts[0], r->sr_string_arrays[0],
            r->sr_string_counts[1], r->sr_string_arrays[1],
            r->sr_string_counts[2], r->sr_string_arrays[2]); break;
    case PDGUI_SERVICE_PREFERENCES_FLAGS:
        pdgui_vmess("::dialog_startup::set_libraries", "S",
            r->sr_string_counts[0], r->sr_string_arrays[0]);
        pdgui_vmess("set_escaped", "ri", "::sys_verbose", r->sr_ints[0]);
        pdgui_vmess("set_escaped", "ri", "::sys_use_stdpath",
            r->sr_ints[1]);
        pdgui_vmess("set_escaped", "ri", "::sys_defeatrt", r->sr_ints[2]);
        pdgui_vmess("set_escaped", "ri", "::sys_zoom_open", r->sr_ints[3]);
        pdgui_vmess("::dialog_startup::set_flags", "s", r->sr_strings[0]);
        break;
    case PDGUI_SERVICE_DEKEN_PLATFORM:
        pdgui_vmess("::deken::set_platform", "ssff", r->sr_strings[0],
            r->sr_strings[1], r->sr_floats[0], r->sr_floats[1]); break;
    case PDGUI_SERVICE_WATCHDOG:
        pdgui_vmess("pdtk_watchdog", 0); break;
    case PDGUI_SERVICE_STARTUP:
    {
        char audio_apis[1024], midi_apis[1024];
        pdgui_tk_api_list(audio_apis, sizeof(audio_apis),
            r->sr_string_counts[0], r->sr_string_arrays[0],
            r->sr_int_arrays[0]);
        pdgui_tk_api_list(midi_apis, sizeof(midi_apis),
            r->sr_string_counts[1], r->sr_string_arrays[1],
            r->sr_int_arrays[1]);
        pdgui_vmess("pdtk_pd_startup", "iiis rr ss", r->sr_ints[0],
            r->sr_ints[1], r->sr_ints[2], r->sr_strings[0],
            audio_apis, midi_apis, r->sr_strings[1], r->sr_strings[2]);
        break;
    }
    case PDGUI_SERVICE_AUDIO_API:
        pdgui_vmess("set", "ri", "pd_whichapi", r->sr_ints[0]); break;
    case PDGUI_SERVICE_AUDIO_RUNNING:
        pdgui_vmess("pdtk_pd_audio", "r", r->sr_ints[0] ? "on" : "off");
        break;
    case PDGUI_SERVICE_DIO_STATE:
        pdgui_vmess("pdtk_pd_dio", "i", r->sr_ints[0]); break;
    case PDGUI_SERVICE_AUDIO_CONFIG:
        pdgui_vmess("::dialog_audio::set_configuration", "SFF SFF ssi si",
            r->sr_string_counts[0], r->sr_string_arrays[0],
            r->sr_float_counts[0], r->sr_float_arrays[0],
            r->sr_float_counts[1], r->sr_float_arrays[1],
            r->sr_string_counts[1], r->sr_string_arrays[1],
            r->sr_float_counts[2], r->sr_float_arrays[2],
            r->sr_float_counts[3], r->sr_float_arrays[3],
            r->sr_strings[0], r->sr_strings[1], r->sr_ints[0],
            r->sr_strings[2], r->sr_ints[1]); break;
    case PDGUI_SERVICE_AUDIO_REFRESH:
        pdgui_vmess("::dialog_audio::refresh_ui", ""); break;
    case PDGUI_SERVICE_MIDI_API:
        pdgui_vmess("set", "ri", "pd_whichmidiapi", r->sr_ints[0]); break;
    case PDGUI_SERVICE_MIDI_CONFIG:
        pdgui_vmess("::dialog_midi::set_configuration", "i SF SF",
            r->sr_ints[0], r->sr_string_counts[0], r->sr_string_arrays[0],
            r->sr_float_counts[0], r->sr_float_arrays[0],
            r->sr_string_counts[1], r->sr_string_arrays[1],
            r->sr_float_counts[1], r->sr_float_arrays[1]); break;
    case PDGUI_SERVICE_MIDI_REFRESH:
        pdgui_vmess("::dialog_midi::refresh_ui", ""); break;
    case PDGUI_SERVICE_OPEN_PANEL:
        pdgui_vmess("pdtk_openpanel", "ssic", r->sr_strings[0],
            r->sr_strings[1], r->sr_ints[0], r->sr_canvas); break;
    case PDGUI_SERVICE_SAVE_PANEL:
        pdgui_vmess("pdtk_savepanel", "ssc", r->sr_strings[0],
            r->sr_strings[1], r->sr_canvas); break;
    case PDGUI_SERVICE_OPEN_FILE:
        pdgui_vmess("::pd_menucommands::menu_openfile", "s",
            r->sr_strings[0]); break;
    case PDGUI_SERVICE_TEXTWINDOW_CLEAR:
        pdgui_vmess("pdtk_textwindow_clear", "^", r->sr_object); break;
    case PDGUI_SERVICE_TEXTWINDOW_APPEND:
        pdgui_vmess("pdtk_textwindow_append", "^s", r->sr_object,
            r->sr_strings[0]); break;
    case PDGUI_SERVICE_TEXTWINDOW_APPEND_ATOMS:
        pdgui_vmess("pdtk_textwindow_appendatoms", "^A", r->sr_object,
            r->sr_natoms, r->sr_atoms); break;
    case PDGUI_SERVICE_TEXTWINDOW_DIRTY:
        pdgui_vmess("pdtk_textwindow_setdirty", "^i", r->sr_object,
            r->sr_ints[0]); break;
    case PDGUI_SERVICE_TEXTWINDOW_RAISE:
    {
        char textid[128];
        sprintf(textid, ".x%lx.text", (unsigned long)r->sr_object);
        pdgui_vmess("wm", "r^", "deiconify", r->sr_object);
        pdgui_vmess("raise", "^", r->sr_object);
        pdgui_vmess("focus", "s", textid);
        break;
    }
    case PDGUI_SERVICE_TEXTWINDOW_OPEN:
        pdgui_vmess("pdtk_textwindow_open", "^r si", r->sr_object,
            r->sr_strings[0], r->sr_strings[1], r->sr_ints[0]); break;
    case PDGUI_SERVICE_TEXTWINDOW_CLOSE:
        pdgui_vmess("pdtk_textwindow_doclose", "^", r->sr_object); break;
    case PDGUI_SERVICE_ARRAY_DIALOG:
        pdgui_stub_vnew(r->sr_owner, "pdtk_array_dialog",
            (void *)r->sr_object, "siii", r->sr_strings[0], r->sr_ints[0],
            r->sr_ints[1], r->sr_ints[2]); break;
    case PDGUI_SERVICE_ARRAY_LISTVIEW_OPEN:
        pdgui_stub_vnew(r->sr_owner, "pdtk_array_listview_new",
            (void *)r->sr_object, "si", r->sr_strings[0], r->sr_ints[0]);
        break;
    case PDGUI_SERVICE_GATOM_DIALOG:
        pdgui_stub_vnew(r->sr_owner, "pdtk_gatom_dialog",
            (void *)r->sr_object, "i ff i sss i", r->sr_ints[0],
            r->sr_floats[0], r->sr_floats[1], r->sr_ints[1],
            r->sr_strings[0], r->sr_strings[1], r->sr_strings[2],
            r->sr_ints[2]); break;
    case PDGUI_SERVICE_IEMGUI_DIALOG:
    {
        char send[MAXPDSTRING], receive[MAXPDSTRING], label[MAXPDSTRING];
        pdgui_strnescape(send, sizeof(send), r->sr_strings[3], 0);
        pdgui_strnescape(receive, sizeof(receive), r->sr_strings[4], 0);
        pdgui_strnescape(label, sizeof(label), r->sr_strings[5], 0);
        pdgui_stub_vnew(r->sr_owner, "pdtk_iemgui_dialog",
            (void *)r->sr_object,
            "r s ffs ffs sfsfs i iss ii si sss ii ii kkk",
            r->sr_strings[0], "", r->sr_floats[0], r->sr_floats[1], "",
            r->sr_floats[2], r->sr_floats[3], "", "", r->sr_floats[4],
            "", r->sr_floats[5], "", r->sr_ints[0], r->sr_ints[1],
            r->sr_strings[1], r->sr_strings[2], r->sr_ints[2],
            r->sr_ints[3], "", r->sr_ints[4], send,
            receive, label, r->sr_ints[5],
            r->sr_ints[6], r->sr_ints[7], r->sr_ints[8], r->sr_ints[9],
            r->sr_ints[10], r->sr_ints[11]);
        break;
    }
    case PDGUI_SERVICE_DATA_DIALOG:
        pdgui_stub_vnew(r->sr_owner, "pdtk_data_dialog",
            (void *)r->sr_object, "p", r->sr_ints[0], r->sr_strings[0]);
        break;
    case PDGUI_SERVICE_CANVAS_DIALOG:
        pdgui_stub_vnew(r->sr_owner, "pdtk_canvas_dialog",
            (void *)r->sr_object, "ff i ffff ii ii", r->sr_floats[0],
            r->sr_floats[1], r->sr_ints[0], r->sr_floats[2],
            r->sr_floats[3], r->sr_floats[4], r->sr_floats[5],
            r->sr_ints[1], r->sr_ints[2], r->sr_ints[3], r->sr_ints[4]);
        break;
    case PDGUI_SERVICE_FONT_DIALOG:
        pdgui_stub_vnew(r->sr_owner, "pdtk_canvas_dofont",
            (void *)r->sr_object, "i", r->sr_ints[0]); break;
    case PDGUI_SERVICE_AUDIO_DIALOG_OPEN:
        pdgui_stub_vnew(r->sr_owner, "::dialog_audio::create",
            (void *)r->sr_object, ""); break;
    case PDGUI_SERVICE_MIDI_DIALOG_OPEN:
        pdgui_stub_vnew(r->sr_owner, "::dialog_midi::create",
            (void *)r->sr_object, ""); break;
    case PDGUI_SERVICE_PATH_DIALOG_OPEN:
        pdgui_stub_vnew(r->sr_owner, "pdtk_path_dialog",
            (void *)r->sr_object, "ii", r->sr_ints[0], r->sr_ints[1]);
        break;
    case PDGUI_SERVICE_STARTUP_DIALOG_OPEN:
        pdgui_stub_vnew(r->sr_owner, "pdtk_startup_dialog",
            (void *)r->sr_object, "is", r->sr_ints[0], r->sr_strings[0]);
        break;
    case PDGUI_SERVICE_EXIT:
        sys_vgui("%s", "exit\n"); break;
    default:
        break;
    }
}

const t_pdgui_backend pdgui_tk_backend =
{
    pdgui_tk_backend_rect_create,
    pdgui_tk_backend_rect_update,
    pdgui_tk_oval_create,
    pdgui_tk_backend_oval_update,
    pdgui_tk_backend_line_create,
    pdgui_tk_backend_line_update,
    pdgui_tk_backend_polygon_create,
    pdgui_tk_backend_polygon_update,
    pdgui_tk_path_create,
    pdgui_tk_path_set_points,
    pdgui_tk_text_create,
    pdgui_tk_text_create_plain,
    pdgui_tk_text_create_grouped,
    pdgui_tk_text_create_anchored,
    pdgui_tk_backend_text_update,
    pdgui_tk_canvas_text_create,
    pdgui_tk_canvas_text_create_grouped,
    pdgui_tk_canvas_text_create_label,
    pdgui_tk_text_set_selection,
    pdgui_tk_text_set_editing,
    pdgui_tk_item_destroy,
    pdgui_tk_item_move,
    pdgui_tk_backend_item_order,
    pdgui_tk_backend_item_style,
    pdgui_tk_canvas_clear,
    pdgui_tk_canvas_set_colors,
    pdgui_tk_canvas_set_patchcords_foreground,
    pdgui_tk_patchcord_create,
    pdgui_tk_service,
    pdgui_tk_backend_poll,
    pdgui_tk_backend_init
};
