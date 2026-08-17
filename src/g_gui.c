/* Copyright (c) 2026 The Pure Data Team.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

#include "g_gui.h"
#include "g_canvas.h"
#include "s_stuff.h"
#include <string.h>

extern const t_pdgui_backend pdgui_tk_backend;

#ifdef PDGUI_DEFAULT_GTK
extern const t_pdgui_backend pdgui_gtk_backend;
static const t_pdgui_backend *pdgui_backend = &pdgui_gtk_backend;
#else
static const t_pdgui_backend *pdgui_backend = &pdgui_tk_backend;
#endif

int pdgui_set_backend(const t_pdgui_backend *backend) {
    if (!backend || !backend->gb_rect_create || !backend->gb_rect_update ||
        !backend->gb_oval_create || !backend->gb_oval_update || !backend->gb_line_create ||
        !backend->gb_line_update || !backend->gb_polygon_create || !backend->gb_polygon_update ||
        !backend->gb_path_create || !backend->gb_path_set_points || !backend->gb_text_create ||
        !backend->gb_text_create_plain || !backend->gb_text_create_grouped ||
        !backend->gb_text_create_anchored || !backend->gb_text_update ||
        !backend->gb_canvas_text_create || !backend->gb_canvas_text_create_grouped ||
        !backend->gb_canvas_text_create_label || !backend->gb_text_set_selection ||
        !backend->gb_text_set_editing || !backend->gb_item_destroy || !backend->gb_item_move ||
        !backend->gb_item_order || !backend->gb_item_style || !backend->gb_canvas_clear ||
        !backend->gb_canvas_set_colors || !backend->gb_canvas_set_patchcords_foreground ||
        !backend->gb_patchcord_create || !backend->gb_service ||
        !backend->gb_poll || !backend->gb_init) {
        return (0);
    }
    pdgui_backend = backend;
    return (1);
}

int pdgui_init(const char *libdir) {
    t_canvas *canvas;
    if (!pdgui_backend->gb_init) {
        return (-1);
    }
    for (canvas = pd_getcanvaslist(); canvas; canvas = canvas->gl_next) {
        canvas_vis(canvas, 0);
    }
    sys_guiinit();
    return ((*pdgui_backend->gb_init)(libdir));
}

int pdgui_poll(void) {
    return ((*pdgui_backend->gb_poll)());
}

void pdgui_rect_create(t_canvas *canvas, const char *item, const char *group, int x1, int y1,
                       int x2, int y2, int width, unsigned int fill, unsigned int outline) {
    (*pdgui_backend->gb_rect_create)(canvas, item, group, 0, x1, y1, x2, y2, width, fill, outline);
}

void pdgui_rect_create_grouped(t_canvas *canvas, const char *item, const char *group,
                               const char *collection, int x1, int y1, int x2, int y2, int width,
                               unsigned int fill, unsigned int outline) {
    (*pdgui_backend->gb_rect_create)(canvas, item, group, collection, x1, y1, x2, y2, width, fill,
                                     outline);
}

void pdgui_rect_configure(t_canvas *canvas, const char *item, int x1, int y1, int x2, int y2,
                          int width, unsigned int fill, unsigned int outline) {
    (*pdgui_backend->gb_rect_update)(canvas, item,
                                     PDGUI_CHANGE_POINTS | PDGUI_CHANGE_WIDTH | PDGUI_CHANGE_FILL |
                                         PDGUI_CHANGE_OUTLINE,
                                     x1, y1, x2, y2, width, fill, outline);
}

void pdgui_rect_set_outline(t_canvas *canvas, const char *item, unsigned int outline) {
    (*pdgui_backend->gb_rect_update)(canvas, item, PDGUI_CHANGE_OUTLINE, 0, 0, 0, 0, 0, 0, outline);
}

void pdgui_rect_set_style(t_canvas *canvas, const char *item, int width, unsigned int fill,
                          unsigned int outline) {
    (*pdgui_backend->gb_rect_update)(canvas, item,
                                     PDGUI_CHANGE_WIDTH | PDGUI_CHANGE_FILL | PDGUI_CHANGE_OUTLINE,
                                     0, 0, 0, 0, width, fill, outline);
}

void pdgui_rect_set_bounds(t_canvas *canvas, const char *item, int x1, int y1, int x2, int y2) {
    (*pdgui_backend->gb_rect_update)(canvas, item, PDGUI_CHANGE_POINTS, x1, y1, x2, y2, 0, 0, 0);
}

void pdgui_oval_create(t_canvas *canvas, const char *item, const char *group, int x1, int y1,
                       int x2, int y2, int width, unsigned int fill, unsigned int outline) {
    (*pdgui_backend->gb_oval_create)(canvas, item, group, x1, y1, x2, y2, width, fill, outline);
}

void pdgui_oval_configure(t_canvas *canvas, const char *item, int x1, int y1, int x2, int y2,
                          int width, unsigned int fill, unsigned int outline) {
    (*pdgui_backend->gb_oval_update)(canvas, item,
                                     PDGUI_CHANGE_POINTS | PDGUI_CHANGE_WIDTH | PDGUI_CHANGE_FILL |
                                         PDGUI_CHANGE_OUTLINE,
                                     x1, y1, x2, y2, width, fill, outline);
}

void pdgui_oval_set_style(t_canvas *canvas, const char *item, int width, unsigned int fill,
                          unsigned int outline) {
    (*pdgui_backend->gb_oval_update)(canvas, item,
                                     PDGUI_CHANGE_WIDTH | PDGUI_CHANGE_FILL | PDGUI_CHANGE_OUTLINE,
                                     0, 0, 0, 0, width, fill, outline);
}

void pdgui_line_create(t_canvas *canvas, const char *item, const char *group, int x1, int y1,
                       int x2, int y2, int width, unsigned int color) {
    int coords[4];
    coords[0] = x1;
    coords[1] = y1;
    coords[2] = x2;
    coords[3] = y2;
    (*pdgui_backend->gb_line_create)(canvas, item, group, coords, 4, width, color, 0);
}

void pdgui_line_configure(t_canvas *canvas, const char *item, int x1, int y1, int x2, int y2,
                          int width, unsigned int color) {
    int coords[4];
    coords[0] = x1;
    coords[1] = y1;
    coords[2] = x2;
    coords[3] = y2;
    (*pdgui_backend->gb_line_update)(canvas, item,
                                     PDGUI_CHANGE_POINTS | PDGUI_CHANGE_WIDTH | PDGUI_CHANGE_COLOR,
                                     coords, 4, width, color);
}

void pdgui_line_set_style(t_canvas *canvas, const char *item, int width, unsigned int color) {
    (*pdgui_backend->gb_line_update)(canvas, item, PDGUI_CHANGE_WIDTH | PDGUI_CHANGE_COLOR, 0, 0,
                                     width, color);
}

void pdgui_polyline_create(t_canvas *canvas, const char *item, const char *group, const int *coords,
                           int ncoords, int width, unsigned int color) {
    (*pdgui_backend->gb_line_create)(canvas, item, group, coords, ncoords, width, color, 0);
}

void pdgui_polyline_configure(t_canvas *canvas, const char *item, const int *coords, int ncoords,
                              int width, unsigned int color) {
    (*pdgui_backend->gb_line_update)(canvas, item,
                                     PDGUI_CHANGE_POINTS | PDGUI_CHANGE_WIDTH | PDGUI_CHANGE_COLOR,
                                     coords, ncoords, width, color);
}

void pdgui_polyline_create_dashed(t_canvas *canvas, const char *item, const char *group,
                                  const int *coords, int ncoords, int width, unsigned int color,
                                  int dashed) {
    (*pdgui_backend->gb_line_create)(canvas, item, group, coords, ncoords, width, color, dashed);
}

void pdgui_polyline_set_points(t_canvas *canvas, const char *item, const int *coords, int ncoords) {
    (*pdgui_backend->gb_line_update)(canvas, item, PDGUI_CHANGE_POINTS, coords, ncoords, 0, 0);
}

void pdgui_polygon_create(t_canvas *canvas, const char *item, const char *group, const int *coords,
                          int ncoords, int width, unsigned int fill, unsigned int outline) {
    (*pdgui_backend->gb_polygon_create)(canvas, item, group, coords, ncoords, width, fill, outline,
                                        0);
}

void pdgui_polygon_configure(t_canvas *canvas, const char *item, const int *coords, int ncoords,
                             int width, unsigned int fill, unsigned int outline) {
    (*pdgui_backend->gb_polygon_update)(canvas, item,
                                        PDGUI_CHANGE_POINTS | PDGUI_CHANGE_WIDTH |
                                            PDGUI_CHANGE_FILL | PDGUI_CHANGE_OUTLINE,
                                        coords, ncoords, width, fill, outline);
}

void pdgui_polygon_create_miter(t_canvas *canvas, const char *item, const char *group,
                                const int *coords, int ncoords, int width, unsigned int fill,
                                unsigned int outline) {
    (*pdgui_backend->gb_polygon_create)(canvas, item, group, coords, ncoords, width, fill, outline,
                                        1);
}

void pdgui_path_create(t_canvas *canvas, const char *item, const t_word *coords, int ncoords,
                       int closed, int smooth, int width, unsigned int fill, unsigned int outline) {
    (*pdgui_backend->gb_path_create)(canvas, item, coords, ncoords, closed, smooth, width, fill,
                                     outline);
}

void pdgui_path_set_points(t_canvas *canvas, const char *item, const t_word *coords, int ncoords) {
    (*pdgui_backend->gb_path_set_points)(canvas, item, coords, ncoords);
}

void pdgui_text_create(t_canvas *canvas, const char *item, const char *group, int x, int y) {
    (*pdgui_backend->gb_text_create)(canvas, item, group, x, y);
}

void pdgui_text_create_plain(t_canvas *canvas, const char *item, const char *group, int x, int y) {
    (*pdgui_backend->gb_text_create_plain)(canvas, item, group, x, y);
}

void pdgui_text_create_grouped(t_canvas *canvas, const char *item, const char *group,
                               const char *collection, int x, int y) {
    (*pdgui_backend->gb_text_create_grouped)(canvas, item, group, collection, x, y);
}

void pdgui_text_create_anchored(t_canvas *canvas, const char *item, const char *group,
                                const char *collection, int x, int y, const char *text,
                                t_pdgui_anchor anchor, const char *font, int fontsize,
                                const char *weight, unsigned int color) {
    (*pdgui_backend->gb_text_create_anchored)(canvas, item, group, collection, x, y, text, anchor,
                                              font, fontsize, weight, color);
}

void pdgui_text_configure(t_canvas *canvas, const char *item, int x, int y, const char *font,
                          int fontsize, const char *weight, unsigned int color) {
    (*pdgui_backend->gb_text_update)(canvas, item,
                                     PDGUI_CHANGE_POINTS | PDGUI_CHANGE_FONT | PDGUI_CHANGE_COLOR,
                                     x, y, 0, font, fontsize, weight, color);
}

void pdgui_text_set_color(t_canvas *canvas, const char *item, unsigned int color) {
    (*pdgui_backend->gb_text_update)(canvas, item, PDGUI_CHANGE_COLOR, 0, 0, 0, 0, 0, 0, color);
}

void pdgui_text_set_position(t_canvas *canvas, const char *item, int x, int y) {
    (*pdgui_backend->gb_text_update)(canvas, item, PDGUI_CHANGE_POINTS, x, y, 0, 0, 0, 0, 0);
}

void pdgui_text_set_content(t_canvas *canvas, const char *item, const char *text) {
    (*pdgui_backend->gb_text_update)(canvas, item, PDGUI_CHANGE_CONTENT, 0, 0, text, 0, 0, 0, 0);
}

void pdgui_text_set_font(t_canvas *canvas, const char *item, const char *font, int fontsize,
                         const char *weight) {
    (*pdgui_backend->gb_text_update)(canvas, item, PDGUI_CHANGE_FONT, 0, 0, 0, font, fontsize,
                                     weight, 0);
}

void pdgui_text_set_content_color(t_canvas *canvas, const char *item, const char *text,
                                  unsigned int color) {
    (*pdgui_backend->gb_text_update)(canvas, item, PDGUI_CHANGE_CONTENT | PDGUI_CHANGE_COLOR, 0, 0,
                                     text, 0, 0, 0, color);
}

void pdgui_canvas_text_create(t_canvas *canvas, const char *item, int x, int y, const char *text,
                              int fontsize, unsigned int color) {
    (*pdgui_backend->gb_canvas_text_create)(canvas, item, x, y, text, fontsize, color);
}

void pdgui_canvas_text_create_grouped(t_canvas *canvas, const char *item, const char *collection,
                                      int x, int y, const char *text, int fontsize,
                                      unsigned int color) {
    (*pdgui_backend->gb_canvas_text_create_grouped)(canvas, item, collection, x, y, text, fontsize,
                                                    color);
}

void pdgui_canvas_text_create_label(t_canvas *canvas, const char *item, int x, int y,
                                    const char *text, int fontsize, unsigned int color) {
    (*pdgui_backend->gb_canvas_text_create_label)(canvas, item, x, y, text, fontsize, color);
}

void pdgui_text_set_selection(t_canvas *canvas, const char *item, int start, int end) {
    (*pdgui_backend->gb_text_set_selection)(canvas, item, start, end);
}

void pdgui_text_set_editing(t_canvas *canvas, const char *item, int state) {
    (*pdgui_backend->gb_text_set_editing)(canvas, item, state);
}

void pdgui_item_destroy(t_canvas *canvas, const char *item) {
    (*pdgui_backend->gb_item_destroy)(canvas, item);
}

void pdgui_item_move(t_canvas *canvas, const char *item, int dx, int dy) {
    (*pdgui_backend->gb_item_move)(canvas, item, dx, dy);
}

void pdgui_item_lower(t_canvas *canvas, const char *item, const char *below) {
    (*pdgui_backend->gb_item_order)(canvas, item, PDGUI_ORDER_BELOW, below);
}

void pdgui_item_raise(t_canvas *canvas, const char *item, const char *above) {
    (*pdgui_backend->gb_item_order)(canvas, item, PDGUI_ORDER_ABOVE, above);
}

void pdgui_item_set_fill(t_canvas *canvas, const char *item, unsigned int fill) {
    (*pdgui_backend->gb_item_style)(canvas, item, PDGUI_CHANGE_FILL, 0, fill, 0);
}

void pdgui_item_set_outline(t_canvas *canvas, const char *item, unsigned int outline) {
    (*pdgui_backend->gb_item_style)(canvas, item, PDGUI_CHANGE_OUTLINE, 0, 0, outline);
}

void pdgui_item_set_width(t_canvas *canvas, const char *item, int width) {
    (*pdgui_backend->gb_item_style)(canvas, item, PDGUI_CHANGE_WIDTH, width, 0, 0);
}

void pdgui_item_raise_top(t_canvas *canvas, const char *item) {
    (*pdgui_backend->gb_item_order)(canvas, item, PDGUI_ORDER_TOP, 0);
}

void pdgui_canvas_clear(t_canvas *canvas) {
    (*pdgui_backend->gb_canvas_clear)(canvas);
}

void pdgui_canvas_set_colors(t_canvas *canvas, unsigned int background, unsigned int foreground) {
    (*pdgui_backend->gb_canvas_set_colors)(canvas, background, foreground);
}

void pdgui_canvas_set_patchcords_foreground(t_canvas *canvas, int state) {
    (*pdgui_backend->gb_canvas_set_patchcords_foreground)(canvas, state);
}

void pdgui_patchcord_create(t_canvas *canvas, const char *item, int x1, int y1, int x2, int y2,
                            int width, unsigned int color) {
    (*pdgui_backend->gb_patchcord_create)(canvas, item, x1, y1, x2, y2, width, color);
}

void pdgui_patchcord_set_position(t_canvas *canvas, const char *item, int x1, int y1, int x2,
                                  int y2) {
    int coords[4];
    coords[0] = x1;
    coords[1] = y1;
    coords[2] = x2;
    coords[3] = y2;
    (*pdgui_backend->gb_line_update)(canvas, item, PDGUI_CHANGE_POINTS, coords, 4, 0, 0);
}

static void pdgui_service_send(t_pdgui_service service, t_pdgui_service_request *request) {
    (*pdgui_backend->gb_service)(service, request);
}

#define PDGUI_REQUEST(name)                                                                        \
    t_pdgui_service_request name;                                                                  \
    memset(&(name), 0, sizeof(name))

void pdgui_window_menu_update(void) {
    PDGUI_REQUEST(r);
    pdgui_service_send(PDGUI_SERVICE_WINDOW_MENU_UPDATE, &r);
}

void pdgui_canvas_set_title(t_canvas *canvas, const char *directory, const char *name,
                            const char *display_name, int dirty, int editmode) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_strings[0] = directory;
    r.sr_strings[1] = name;
    r.sr_strings[2] = display_name;
    r.sr_ints[0] = dirty;
    r.sr_ints[1] = editmode;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_TITLE, &r);
}

void pdgui_canvas_update_scrollbars(t_canvas *canvas) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_SCROLL, &r);
}

void pdgui_set_dsp_state(int state) {
    PDGUI_REQUEST(r);
    r.sr_ints[0] = state;
    pdgui_service_send(PDGUI_SERVICE_DSP_STATE, &r);
}

void pdgui_busy_release(void) {
    PDGUI_REQUEST(r);
    pdgui_service_send(PDGUI_SERVICE_BUSY_RELEASE, &r);
}

void pdgui_struct_menu_clear(void) {
    PDGUI_REQUEST(r);
    pdgui_service_send(PDGUI_SERVICE_STRUCT_MENU_CLEAR, &r);
}

void pdgui_struct_menu_add(const char *name) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = name;
    pdgui_service_send(PDGUI_SERVICE_STRUCT_MENU_ADD, &r);
}

void pdgui_undo_menu(t_canvas *canvas, const char *undo, const char *redo) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_strings[0] = undo;
    r.sr_strings[1] = redo;
    pdgui_service_send(PDGUI_SERVICE_UNDO_MENU, &r);
}

void pdgui_clipboard_set(const char *text, int length) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = text;
    r.sr_ints[0] = length;
    pdgui_service_send(PDGUI_SERVICE_CLIPBOARD_SET, &r);
}

void pdgui_canvas_set_cursor(t_canvas *canvas, int cursor) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_ints[0] = cursor;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_CURSOR, &r);
}

void pdgui_canvas_raise_window(t_canvas *canvas) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_RAISE, &r);
}

void pdgui_canvas_create_window(t_canvas *canvas, int width, int height, const char *position,
                                int editmode, unsigned int background, unsigned int foreground,
                                int custom_colors) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_strings[0] = position;
    r.sr_ints[0] = width;
    r.sr_ints[1] = height;
    r.sr_ints[2] = editmode;
    r.sr_ints[3] = (int)background;
    r.sr_ints[4] = (int)foreground;
    r.sr_ints[5] = custom_colors;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_CREATE, &r);
}

void pdgui_canvas_set_parents(t_canvas *canvas, int count, t_canvas **parents) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_ncanvases = count;
    r.sr_canvases = parents;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_PARENTS, &r);
}

void pdgui_window_destroy(void *window) {
    PDGUI_REQUEST(r);
    r.sr_object = window;
    pdgui_service_send(PDGUI_SERVICE_WINDOW_DESTROY, &r);
}

void pdgui_canvas_dialog_set_text(const char *text) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = text;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_DIALOG_TEXT, &r);
}

void pdgui_canvas_popup(t_canvas *canvas, int x, int y, int can_properties, int can_open) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_ints[0] = x;
    r.sr_ints[1] = y;
    r.sr_ints[2] = can_properties;
    r.sr_ints[3] = can_open;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_POPUP, &r);
}

void pdgui_canvas_export_postscript(t_canvas *canvas, const char *filename) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_strings[0] = filename;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_EXPORT, &r);
}

void pdgui_confirm(t_canvas *canvas, const char *window, int message_count, const char **messages,
                   t_symbol *callback, int callback_argc, const t_atom *callback_argv,
                   const char *default_answer) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_strings[0] = window;
    r.sr_string_arrays[0] = messages;
    r.sr_string_counts[0] = message_count;
    r.sr_symbol = callback;
    r.sr_atoms = callback_argv;
    r.sr_natoms = callback_argc;
    r.sr_strings[1] = default_answer;
    pdgui_service_send(PDGUI_SERVICE_CONFIRM, &r);
}

void pdgui_canvas_close_confirm(t_canvas *canvas, t_symbol *callback, int callback_argc,
                                const t_atom *callback_argv) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_symbol = callback;
    r.sr_atoms = callback_argv;
    r.sr_natoms = callback_argc;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_CLOSE_CONFIRM, &r);
}

void pdgui_find_result(t_canvas *canvas, int found, int find_index, int object_index) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_ints[0] = found;
    r.sr_ints[1] = find_index;
    r.sr_ints[2] = object_index;
    pdgui_service_send(PDGUI_SERVICE_FIND_RESULT, &r);
}

void pdgui_canvas_paste(t_canvas *canvas, int into_text) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_ints[0] = into_text;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_PASTE, &r);
}

void pdgui_canvas_set_pointer(t_canvas *canvas, int x, int y) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_ints[0] = x;
    r.sr_ints[1] = y;
    pdgui_service_send(PDGUI_SERVICE_POINTER_POSITION, &r);
}

void pdgui_pd_texteditor(const char *text, int length) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = text;
    r.sr_ints[0] = length;
    pdgui_service_send(PDGUI_SERVICE_PD_TEXTEDITOR, &r);
}

void pdgui_canvas_set_editmode(t_canvas *canvas, int state) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_ints[0] = state;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_EDITMODE, &r);
}

void pdgui_array_set_page(const char *name, int page, int pages, int page_size) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = name;
    r.sr_ints[0] = page;
    r.sr_ints[1] = pages;
    r.sr_ints[2] = page_size;
    pdgui_service_send(PDGUI_SERVICE_ARRAY_PAGE, &r);
}

void pdgui_array_set_data(const char *name, int offset, int length, const t_word *data) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = name;
    r.sr_ints[0] = offset;
    r.sr_ints[1] = length;
    r.sr_object = (void *)data;
    pdgui_service_send(PDGUI_SERVICE_ARRAY_DATA, &r);
}

void pdgui_array_set_focus(const char *name, int item) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = name;
    r.sr_ints[0] = item;
    pdgui_service_send(PDGUI_SERVICE_ARRAY_FOCUS, &r);
}

void pdgui_array_close(const char *name) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = name;
    pdgui_service_send(PDGUI_SERVICE_ARRAY_CLOSE, &r);
}

void pdgui_array_refresh(const char *name) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = name;
    pdgui_service_send(PDGUI_SERVICE_ARRAY_REFRESH, &r);
}

void pdgui_missing_object(void *object, const char *name) {
    PDGUI_REQUEST(r);
    r.sr_object = object;
    r.sr_strings[0] = name;
    pdgui_service_send(PDGUI_SERVICE_MISSING_OBJECT, &r);
}

void pdgui_canvas_save_as(t_canvas *canvas, const char *name, const char *directory,
                          int destroy_after_save) {
    PDGUI_REQUEST(r);
    r.sr_canvas = canvas;
    r.sr_strings[0] = name;
    r.sr_strings[1] = directory;
    r.sr_ints[0] = destroy_after_save;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_SAVE_AS, &r);
}

void pdgui_confirm_quit(const char *message) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = message;
    pdgui_service_send(PDGUI_SERVICE_CONFIRM, &r);
}

void pdgui_console_post(const char *text) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = text;
    pdgui_service_send(PDGUI_SERVICE_CONSOLE_POST, &r);
}

void pdgui_console_log(const void *object, int level, const char *text) {
    PDGUI_REQUEST(r);
    r.sr_object = object;
    r.sr_ints[0] = level;
    r.sr_strings[0] = text;
    pdgui_service_send(PDGUI_SERVICE_CONSOLE_LOG, &r);
}

void pdgui_plugin_dispatch(int argc, const t_atom *argv) {
    PDGUI_REQUEST(r);
    r.sr_natoms = argc;
    r.sr_atoms = argv;
    pdgui_service_send(PDGUI_SERVICE_PLUGIN_DISPATCH, &r);
}

void pdgui_preferences_open(void) {
    PDGUI_REQUEST(r);
    pdgui_service_send(PDGUI_SERVICE_PREFERENCES_OPEN, &r);
}

void pdgui_ping(void) {
    PDGUI_REQUEST(r);
    pdgui_service_send(PDGUI_SERVICE_PING, &r);
}

void pdgui_preferences_set_paths(int nsearch, const char **search, int ntemp, const char **temp,
                                 int nstatic, const char **static_paths) {
    PDGUI_REQUEST(r);
    r.sr_string_counts[0] = nsearch;
    r.sr_string_arrays[0] = search;
    r.sr_string_counts[1] = ntemp;
    r.sr_string_arrays[1] = temp;
    r.sr_string_counts[2] = nstatic;
    r.sr_string_arrays[2] = static_paths;
    pdgui_service_send(PDGUI_SERVICE_PREFERENCES_PATHS, &r);
}

void pdgui_preferences_set_startup(int nlibs, const char **libs, int verbose, int use_standard_path,
                                   int defeat_realtime, int zoom_open, const char *flags) {
    PDGUI_REQUEST(r);
    r.sr_string_counts[0] = nlibs;
    r.sr_string_arrays[0] = libs;
    r.sr_ints[0] = verbose;
    r.sr_ints[1] = use_standard_path;
    r.sr_ints[2] = defeat_realtime;
    r.sr_ints[3] = zoom_open;
    r.sr_strings[0] = flags;
    pdgui_service_send(PDGUI_SERVICE_PREFERENCES_FLAGS, &r);
}

void pdgui_deken_set_platform(const char *os, const char *cpu, t_float pointer_bits,
                              t_float float_bits) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = os;
    r.sr_strings[1] = cpu;
    r.sr_floats[0] = pointer_bits;
    r.sr_floats[1] = float_bits;
    pdgui_service_send(PDGUI_SERVICE_DEKEN_PLATFORM, &r);
}

void pdgui_watchdog_start(void) {
    PDGUI_REQUEST(r);
    pdgui_service_send(PDGUI_SERVICE_WATCHDOG, &r);
}

void pdgui_startup(int major, int minor, int bugfix, const char *test, int naudio_apis,
                   const char *const *audio_api_names, const int *audio_api_ids, int nmidi_apis,
                   const char *const *midi_api_names, const int *midi_api_ids, const char *font,
                   const char *font_weight) {
    PDGUI_REQUEST(r);
    r.sr_ints[0] = major;
    r.sr_ints[1] = minor;
    r.sr_ints[2] = bugfix;
    r.sr_strings[0] = test;
    r.sr_string_counts[0] = naudio_apis;
    r.sr_string_arrays[0] = audio_api_names;
    r.sr_int_counts[0] = naudio_apis;
    r.sr_int_arrays[0] = audio_api_ids;
    r.sr_string_counts[1] = nmidi_apis;
    r.sr_string_arrays[1] = midi_api_names;
    r.sr_int_counts[1] = nmidi_apis;
    r.sr_int_arrays[1] = midi_api_ids;
    r.sr_strings[1] = font;
    r.sr_strings[2] = font_weight;
    pdgui_service_send(PDGUI_SERVICE_STARTUP, &r);
}

void pdgui_set_audio_api(int api) {
    PDGUI_REQUEST(r);
    r.sr_ints[0] = api;
    pdgui_service_send(PDGUI_SERVICE_AUDIO_API, &r);
}

void pdgui_set_audio_running(int state) {
    PDGUI_REQUEST(r);
    r.sr_ints[0] = state;
    pdgui_service_send(PDGUI_SERVICE_AUDIO_RUNNING, &r);
}

void pdgui_set_dio_state(int state) {
    PDGUI_REQUEST(r);
    r.sr_ints[0] = state;
    pdgui_service_send(PDGUI_SERVICE_DIO_STATE, &r);
}

void pdgui_audio_set_configuration(
    int nin_devices, const char *const *in_devices, int nin_used, const t_float *in_used,
    int nin_channels, const t_float *in_channels, int nout_devices, const char *const *out_devices,
    int nout_used, const t_float *out_used, int nout_channels, const t_float *out_channels,
    const char *sample_rate, const char *block_size, int advance, const char *callback, int multi) {
    PDGUI_REQUEST(r);
    r.sr_string_counts[0] = nin_devices;
    r.sr_string_arrays[0] = in_devices;
    r.sr_string_counts[1] = nout_devices;
    r.sr_string_arrays[1] = out_devices;
    r.sr_float_counts[0] = nin_used;
    r.sr_float_arrays[0] = in_used;
    r.sr_float_counts[1] = nin_channels;
    r.sr_float_arrays[1] = in_channels;
    r.sr_float_counts[2] = nout_used;
    r.sr_float_arrays[2] = out_used;
    r.sr_float_counts[3] = nout_channels;
    r.sr_float_arrays[3] = out_channels;
    r.sr_strings[0] = sample_rate;
    r.sr_strings[1] = block_size;
    r.sr_strings[2] = callback;
    r.sr_ints[0] = advance;
    r.sr_ints[1] = multi;
    pdgui_service_send(PDGUI_SERVICE_AUDIO_CONFIG, &r);
}

void pdgui_audio_refresh(void) {
    PDGUI_REQUEST(r);
    pdgui_service_send(PDGUI_SERVICE_AUDIO_REFRESH, &r);
}

void pdgui_set_midi_api(int api) {
    PDGUI_REQUEST(r);
    r.sr_ints[0] = api;
    pdgui_service_send(PDGUI_SERVICE_MIDI_API, &r);
}

void pdgui_midi_set_configuration(int api, int nin_devices, const char *const *in_devices,
                                  int nin_used, const t_float *in_used, int nout_devices,
                                  const char *const *out_devices, int nout_used,
                                  const t_float *out_used) {
    PDGUI_REQUEST(r);
    r.sr_ints[0] = api;
    r.sr_string_counts[0] = nin_devices;
    r.sr_string_arrays[0] = in_devices;
    r.sr_string_counts[1] = nout_devices;
    r.sr_string_arrays[1] = out_devices;
    r.sr_float_counts[0] = nin_used;
    r.sr_float_arrays[0] = in_used;
    r.sr_float_counts[1] = nout_used;
    r.sr_float_arrays[1] = out_used;
    pdgui_service_send(PDGUI_SERVICE_MIDI_CONFIG, &r);
}

void pdgui_midi_refresh(void) {
    PDGUI_REQUEST(r);
    pdgui_service_send(PDGUI_SERVICE_MIDI_REFRESH, &r);
}

void pdgui_named_window_destroy(const char *window) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = window;
    pdgui_service_send(PDGUI_SERVICE_WINDOW_DESTROY, &r);
}

void pdgui_open_panel(const char *callback, const char *path, int mode, t_canvas *canvas) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = callback;
    r.sr_strings[1] = path;
    r.sr_ints[0] = mode;
    r.sr_canvas = canvas;
    pdgui_service_send(PDGUI_SERVICE_OPEN_PANEL, &r);
}

void pdgui_save_panel(const char *callback, const char *path, t_canvas *canvas) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = callback;
    r.sr_strings[1] = path;
    r.sr_canvas = canvas;
    pdgui_service_send(PDGUI_SERVICE_SAVE_PANEL, &r);
}

void pdgui_open_file(const char *path) {
    PDGUI_REQUEST(r);
    r.sr_strings[0] = path;
    pdgui_service_send(PDGUI_SERVICE_OPEN_FILE, &r);
}

void pdgui_textwindow_clear(void *window) {
    PDGUI_REQUEST(r);
    r.sr_object = window;
    pdgui_service_send(PDGUI_SERVICE_TEXTWINDOW_CLEAR, &r);
}

void pdgui_textwindow_append(void *window, const char *text) {
    PDGUI_REQUEST(r);
    r.sr_object = window;
    r.sr_strings[0] = text;
    pdgui_service_send(PDGUI_SERVICE_TEXTWINDOW_APPEND, &r);
}

void pdgui_textwindow_append_atoms(void *window, int argc, const t_atom *argv) {
    PDGUI_REQUEST(r);
    r.sr_object = window;
    r.sr_natoms = argc;
    r.sr_atoms = argv;
    pdgui_service_send(PDGUI_SERVICE_TEXTWINDOW_APPEND_ATOMS, &r);
}

void pdgui_textwindow_set_dirty(void *window, int dirty) {
    PDGUI_REQUEST(r);
    r.sr_object = window;
    r.sr_ints[0] = dirty;
    pdgui_service_send(PDGUI_SERVICE_TEXTWINDOW_DIRTY, &r);
}

void pdgui_textwindow_raise(void *window) {
    PDGUI_REQUEST(r);
    r.sr_object = window;
    pdgui_service_send(PDGUI_SERVICE_TEXTWINDOW_RAISE, &r);
}

void pdgui_textwindow_open(void *window, const char *geometry, const char *title, int font_size) {
    PDGUI_REQUEST(r);
    r.sr_object = window;
    r.sr_strings[0] = geometry;
    r.sr_strings[1] = title;
    r.sr_ints[0] = font_size;
    pdgui_service_send(PDGUI_SERVICE_TEXTWINDOW_OPEN, &r);
}

void pdgui_textwindow_close(void *window) {
    PDGUI_REQUEST(r);
    r.sr_object = window;
    pdgui_service_send(PDGUI_SERVICE_TEXTWINDOW_CLOSE, &r);
}

void pdgui_array_dialog(t_pd *owner, void *key, const char *name, int size, int flags,
                        int create_new) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    r.sr_strings[0] = name;
    r.sr_ints[0] = size;
    r.sr_ints[1] = flags;
    r.sr_ints[2] = create_new;
    pdgui_service_send(PDGUI_SERVICE_ARRAY_DIALOG, &r);
}

void pdgui_array_listview_open(t_pd *owner, void *key, const char *name, int page) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    r.sr_strings[0] = name;
    r.sr_ints[0] = page;
    pdgui_service_send(PDGUI_SERVICE_ARRAY_LISTVIEW_OPEN, &r);
}

void pdgui_gatom_dialog(t_pd *owner, void *key, int width, t_float lower, t_float upper,
                        int label_position, const char *label, const char *receive,
                        const char *send, int font_size) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    r.sr_ints[0] = width;
    r.sr_ints[1] = label_position;
    r.sr_ints[2] = font_size;
    r.sr_floats[0] = lower;
    r.sr_floats[1] = upper;
    r.sr_strings[0] = label;
    r.sr_strings[1] = receive;
    r.sr_strings[2] = send;
    pdgui_service_send(PDGUI_SERVICE_GATOM_DIALOG, &r);
}

void pdgui_iemgui_dialog(t_pd *owner, void *key, const char *object_name, t_float width,
                         t_float width_min, t_float height, t_float height_min, t_float range_min,
                         t_float range_max, int schedule, int mode, const char *mode0,
                         const char *mode1, int loadinit, int steady, int number, const char *send,
                         const char *receive, const char *label, int label_x, int label_y,
                         int font_style, int font_size, unsigned int background,
                         unsigned int foreground, unsigned int label_color) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    r.sr_strings[0] = object_name;
    r.sr_strings[1] = mode0;
    r.sr_strings[2] = mode1;
    r.sr_strings[3] = send;
    r.sr_strings[4] = receive;
    r.sr_strings[5] = label;
    r.sr_floats[0] = width;
    r.sr_floats[1] = width_min;
    r.sr_floats[2] = height;
    r.sr_floats[3] = height_min;
    r.sr_floats[4] = range_min;
    r.sr_floats[5] = range_max;
    r.sr_ints[0] = schedule;
    r.sr_ints[1] = mode;
    r.sr_ints[2] = loadinit;
    r.sr_ints[3] = steady;
    r.sr_ints[4] = number;
    r.sr_ints[5] = label_x;
    r.sr_ints[6] = label_y;
    r.sr_ints[7] = font_style;
    r.sr_ints[8] = font_size;
    r.sr_ints[9] = (int)background;
    r.sr_ints[10] = (int)foreground;
    r.sr_ints[11] = (int)label_color;
    pdgui_service_send(PDGUI_SERVICE_IEMGUI_DIALOG, &r);
}

void pdgui_data_dialog(t_pd *owner, void *key, const char *text, int length) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    r.sr_strings[0] = text;
    r.sr_ints[0] = length;
    pdgui_service_send(PDGUI_SERVICE_DATA_DIALOG, &r);
}

void pdgui_canvas_properties_dialog(t_pd *owner, void *key, t_float xscale, t_float yscale,
                                    int is_graph, t_float x1, t_float y1, t_float x2, t_float y2,
                                    int pixel_width, int pixel_height, int xmargin, int ymargin) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    r.sr_floats[0] = xscale;
    r.sr_floats[1] = yscale;
    r.sr_floats[2] = x1;
    r.sr_floats[3] = y1;
    r.sr_floats[4] = x2;
    r.sr_floats[5] = y2;
    r.sr_ints[0] = is_graph;
    r.sr_ints[1] = pixel_width;
    r.sr_ints[2] = pixel_height;
    r.sr_ints[3] = xmargin;
    r.sr_ints[4] = ymargin;
    pdgui_service_send(PDGUI_SERVICE_CANVAS_DIALOG, &r);
}

void pdgui_font_dialog(t_pd *owner, void *key, int font_size) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    r.sr_ints[0] = font_size;
    pdgui_service_send(PDGUI_SERVICE_FONT_DIALOG, &r);
}

void pdgui_audio_dialog_open(t_pd *owner, void *key) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    pdgui_service_send(PDGUI_SERVICE_AUDIO_DIALOG_OPEN, &r);
}

void pdgui_midi_dialog_open(t_pd *owner, void *key) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    pdgui_service_send(PDGUI_SERVICE_MIDI_DIALOG_OPEN, &r);
}

void pdgui_path_dialog_open(t_pd *owner, void *key, int use_standard_path, int verbose) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    r.sr_ints[0] = use_standard_path;
    r.sr_ints[1] = verbose;
    pdgui_service_send(PDGUI_SERVICE_PATH_DIALOG_OPEN, &r);
}

void pdgui_startup_dialog_open(t_pd *owner, void *key, int defeat_realtime, const char *flags) {
    PDGUI_REQUEST(r);
    r.sr_owner = owner;
    r.sr_object = key;
    r.sr_ints[0] = defeat_realtime;
    r.sr_strings[0] = flags;
    pdgui_service_send(PDGUI_SERVICE_STARTUP_DIALOG_OPEN, &r);
}

void pdgui_exit(void) {
    PDGUI_REQUEST(r);
    pdgui_service_send(PDGUI_SERVICE_EXIT, &r);
}
