/* Copyright (c) 2026 The Pure Data Team.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

#ifndef PD_G_GUI_H
#define PD_G_GUI_H

#include "m_pd.h"

#define PDGUI_COLOR_NONE 0xFFFFFFFFU

typedef enum _pdgui_anchor
{
    PDGUI_ANCHOR_CENTER,
    PDGUI_ANCHOR_NORTH,
    PDGUI_ANCHOR_SOUTH,
    PDGUI_ANCHOR_EAST,
    PDGUI_ANCHOR_WEST,
    PDGUI_ANCHOR_NORTH_EAST,
    PDGUI_ANCHOR_NORTH_WEST,
    PDGUI_ANCHOR_SOUTH_EAST,
    PDGUI_ANCHOR_SOUTH_WEST
} t_pdgui_anchor;

#define PDGUI_CHANGE_POINTS   0x01
#define PDGUI_CHANGE_WIDTH    0x02
#define PDGUI_CHANGE_FILL     0x04
#define PDGUI_CHANGE_OUTLINE  0x08
#define PDGUI_CHANGE_COLOR    0x10
#define PDGUI_CHANGE_CONTENT  0x20
#define PDGUI_CHANGE_FONT     0x40

typedef enum _pdgui_order
{
    PDGUI_ORDER_BELOW,
    PDGUI_ORDER_ABOVE,
    PDGUI_ORDER_TOP
} t_pdgui_order;

typedef enum _pdgui_service
{
    PDGUI_SERVICE_WINDOW_MENU_UPDATE,
    PDGUI_SERVICE_CANVAS_TITLE,
    PDGUI_SERVICE_CANVAS_SCROLL,
    PDGUI_SERVICE_DSP_STATE,
    PDGUI_SERVICE_BUSY_RELEASE,
    PDGUI_SERVICE_STRUCT_MENU_CLEAR,
    PDGUI_SERVICE_STRUCT_MENU_ADD,
    PDGUI_SERVICE_UNDO_MENU,
    PDGUI_SERVICE_CLIPBOARD_SET,
    PDGUI_SERVICE_CANVAS_CURSOR,
    PDGUI_SERVICE_CANVAS_RAISE,
    PDGUI_SERVICE_CANVAS_CREATE,
    PDGUI_SERVICE_CANVAS_PARENTS,
    PDGUI_SERVICE_WINDOW_DESTROY,
    PDGUI_SERVICE_CANVAS_DIALOG_TEXT,
    PDGUI_SERVICE_CANVAS_POPUP,
    PDGUI_SERVICE_CANVAS_EXPORT,
    PDGUI_SERVICE_CONFIRM,
    PDGUI_SERVICE_CANVAS_CLOSE_CONFIRM,
    PDGUI_SERVICE_FIND_RESULT,
    PDGUI_SERVICE_CANVAS_PASTE,
    PDGUI_SERVICE_POINTER_POSITION,
    PDGUI_SERVICE_PD_TEXTEDITOR,
    PDGUI_SERVICE_CANVAS_EDITMODE,
    PDGUI_SERVICE_ARRAY_PAGE,
    PDGUI_SERVICE_ARRAY_DATA,
    PDGUI_SERVICE_ARRAY_FOCUS,
    PDGUI_SERVICE_ARRAY_CLOSE,
    PDGUI_SERVICE_ARRAY_REFRESH,
    PDGUI_SERVICE_MISSING_OBJECT,
    PDGUI_SERVICE_CANVAS_SAVE_AS,
    PDGUI_SERVICE_CONSOLE_POST,
    PDGUI_SERVICE_CONSOLE_LOG,
    PDGUI_SERVICE_PLUGIN_DISPATCH,
    PDGUI_SERVICE_PREFERENCES_OPEN,
    PDGUI_SERVICE_PING,
    PDGUI_SERVICE_PREFERENCES_PATHS,
    PDGUI_SERVICE_PREFERENCES_FLAGS,
    PDGUI_SERVICE_DEKEN_PLATFORM,
    PDGUI_SERVICE_WATCHDOG,
    PDGUI_SERVICE_STARTUP,
    PDGUI_SERVICE_AUDIO_API,
    PDGUI_SERVICE_AUDIO_RUNNING,
    PDGUI_SERVICE_DIO_STATE,
    PDGUI_SERVICE_AUDIO_CONFIG,
    PDGUI_SERVICE_AUDIO_REFRESH,
    PDGUI_SERVICE_MIDI_API,
    PDGUI_SERVICE_MIDI_CONFIG,
    PDGUI_SERVICE_MIDI_REFRESH,
    PDGUI_SERVICE_OPEN_PANEL,
    PDGUI_SERVICE_SAVE_PANEL,
    PDGUI_SERVICE_OPEN_FILE,
    PDGUI_SERVICE_TEXTWINDOW_CLEAR,
    PDGUI_SERVICE_TEXTWINDOW_APPEND,
    PDGUI_SERVICE_TEXTWINDOW_APPEND_ATOMS,
    PDGUI_SERVICE_TEXTWINDOW_DIRTY,
    PDGUI_SERVICE_TEXTWINDOW_RAISE,
    PDGUI_SERVICE_TEXTWINDOW_OPEN,
    PDGUI_SERVICE_TEXTWINDOW_CLOSE,
    PDGUI_SERVICE_ARRAY_DIALOG,
    PDGUI_SERVICE_ARRAY_LISTVIEW_OPEN,
    PDGUI_SERVICE_GATOM_DIALOG,
    PDGUI_SERVICE_IEMGUI_DIALOG,
    PDGUI_SERVICE_DATA_DIALOG,
    PDGUI_SERVICE_CANVAS_DIALOG,
    PDGUI_SERVICE_FONT_DIALOG,
    PDGUI_SERVICE_AUDIO_DIALOG_OPEN,
    PDGUI_SERVICE_MIDI_DIALOG_OPEN,
    PDGUI_SERVICE_PATH_DIALOG_OPEN,
    PDGUI_SERVICE_STARTUP_DIALOG_OPEN,
    PDGUI_SERVICE_EXIT
} t_pdgui_service;

/* A dialog stub safely forwards responses to its owner while the owner
 * exists.  Backends must close the stub when their dialog is destroyed. */
typedef struct _gfxstub t_pdgui_stub;

/* Backend-only payload for semantic GUI services.  Public callers use the
 * typed pdgui_* functions below. */
typedef struct _pdgui_service_request
{
    t_canvas *sr_canvas;
    t_pd *sr_owner;
    const void *sr_object;
    t_symbol *sr_symbol;
    const char *sr_strings[20];
    int sr_ints[12];
    t_float sr_floats[12];
    const char *const *sr_string_arrays[3];
    int sr_string_counts[3];
    const int *sr_int_arrays[2];
    int sr_int_counts[2];
    const t_float *sr_float_arrays[4];
    int sr_float_counts[4];
    const t_atom *sr_atoms;
    int sr_natoms;
    t_canvas **sr_canvases;
    int sr_ncanvases;
} t_pdgui_service_request;

/* Coordinates and widths are in canvas pixels; colors are 0xRRGGBB.  Item
 * and group strings are opaque identifiers and must be copied by a backend
 * that retains them after a call returns. */
typedef struct _pdgui_backend
{
    void (*gb_rect_create)(t_canvas *canvas, const char *item,
        const char *group, const char *collection, int x1, int y1, int x2,
        int y2, int width, unsigned int fill, unsigned int outline);
    void (*gb_rect_update)(t_canvas *canvas, const char *item, int changes,
        int x1, int y1, int x2, int y2, int width, unsigned int fill,
        unsigned int outline);
    void (*gb_oval_create)(t_canvas *canvas, const char *item,
        const char *group, int x1, int y1, int x2, int y2, int width,
        unsigned int fill, unsigned int outline);
    void (*gb_oval_update)(t_canvas *canvas, const char *item, int changes,
        int x1, int y1, int x2, int y2, int width, unsigned int fill,
        unsigned int outline);
    void (*gb_line_create)(t_canvas *canvas, const char *item,
        const char *group, const int *coords, int ncoords, int width,
        unsigned int color, int dashed);
    void (*gb_line_update)(t_canvas *canvas, const char *item, int changes,
        const int *coords, int ncoords, int width, unsigned int color);
    void (*gb_polygon_create)(t_canvas *canvas, const char *item,
        const char *group, const int *coords, int ncoords, int width,
        unsigned int fill, unsigned int outline, int miter);
    void (*gb_polygon_update)(t_canvas *canvas, const char *item, int changes,
        const int *coords, int ncoords, int width, unsigned int fill,
        unsigned int outline);
    void (*gb_path_create)(t_canvas *canvas, const char *item,
        const t_word *coords, int ncoords, int closed, int smooth, int width,
        unsigned int fill, unsigned int outline);
    void (*gb_path_set_points)(t_canvas *canvas, const char *item,
        const t_word *coords, int ncoords);
    void (*gb_text_create)(t_canvas *canvas, const char *item,
        const char *group, int x, int y);
    void (*gb_text_create_plain)(t_canvas *canvas, const char *item,
        const char *group, int x, int y);
    void (*gb_text_create_grouped)(t_canvas *canvas, const char *item,
        const char *group, const char *collection, int x, int y);
    void (*gb_text_create_anchored)(t_canvas *canvas, const char *item,
        const char *group, const char *collection, int x, int y,
        const char *text, t_pdgui_anchor anchor, const char *font,
        int fontsize, const char *weight, unsigned int color);
    void (*gb_text_update)(t_canvas *canvas, const char *item, int changes,
        int x, int y, const char *text, const char *font, int fontsize,
        const char *weight, unsigned int color);
    void (*gb_canvas_text_create)(t_canvas *canvas, const char *item,
        int x, int y, const char *text, int fontsize, unsigned int color);
    void (*gb_canvas_text_create_grouped)(t_canvas *canvas,
        const char *item, const char *collection, int x, int y,
        const char *text, int fontsize, unsigned int color);
    void (*gb_canvas_text_create_label)(t_canvas *canvas, const char *item,
        int x, int y, const char *text, int fontsize, unsigned int color);
    void (*gb_text_set_selection)(t_canvas *canvas, const char *item,
        int start, int end);
    void (*gb_text_set_editing)(t_canvas *canvas, const char *item,
        int state);
    void (*gb_item_destroy)(t_canvas *canvas, const char *item);
    void (*gb_item_move)(t_canvas *canvas, const char *item, int dx, int dy);
    void (*gb_item_order)(t_canvas *canvas, const char *item,
        t_pdgui_order order, const char *relative);
    void (*gb_item_style)(t_canvas *canvas, const char *item, int changes,
        int width, unsigned int fill, unsigned int outline);
    void (*gb_canvas_clear)(t_canvas *canvas);
    void (*gb_canvas_set_colors)(t_canvas *canvas, unsigned int background,
        unsigned int foreground);
    void (*gb_canvas_set_patchcords_foreground)(t_canvas *canvas, int state);
    void (*gb_patchcord_create)(t_canvas *canvas, const char *item,
        int x1, int y1, int x2, int y2, int width, unsigned int color);
    void (*gb_service)(t_pdgui_service service,
        const t_pdgui_service_request *request);
    int (*gb_poll)(void);
    int (*gb_init)(const char *libdir);
} t_pdgui_backend;

/* The backend table must remain valid until another backend is installed. */
EXTERN int pdgui_set_backend(const t_pdgui_backend *backend);
EXTERN int pdgui_init(const char *libdir);
EXTERN int pdgui_poll(void);

EXTERN void pdgui_rect_create(t_canvas *canvas, const char *item,
    const char *group, int x1, int y1, int x2, int y2, int width,
    unsigned int fill, unsigned int outline);
EXTERN void pdgui_rect_create_grouped(t_canvas *canvas, const char *item,
    const char *group, const char *collection, int x1, int y1, int x2,
    int y2, int width, unsigned int fill, unsigned int outline);
EXTERN void pdgui_rect_configure(t_canvas *canvas, const char *item,
    int x1, int y1, int x2, int y2, int width, unsigned int fill,
    unsigned int outline);
EXTERN void pdgui_rect_set_outline(t_canvas *canvas, const char *item,
    unsigned int outline);
EXTERN void pdgui_rect_set_style(t_canvas *canvas, const char *item, int width,
    unsigned int fill, unsigned int outline);
EXTERN void pdgui_rect_set_bounds(t_canvas *canvas, const char *item,
    int x1, int y1, int x2, int y2);
EXTERN void pdgui_oval_create(t_canvas *canvas, const char *item,
    const char *group, int x1, int y1, int x2, int y2, int width,
    unsigned int fill, unsigned int outline);
EXTERN void pdgui_oval_configure(t_canvas *canvas, const char *item,
    int x1, int y1, int x2, int y2, int width, unsigned int fill,
    unsigned int outline);
EXTERN void pdgui_oval_set_style(t_canvas *canvas, const char *item,
    int width, unsigned int fill, unsigned int outline);
EXTERN void pdgui_line_create(t_canvas *canvas, const char *item,
    const char *group, int x1, int y1, int x2, int y2, int width,
    unsigned int color);
EXTERN void pdgui_line_configure(t_canvas *canvas, const char *item,
    int x1, int y1, int x2, int y2, int width, unsigned int color);
EXTERN void pdgui_line_set_style(t_canvas *canvas, const char *item,
    int width, unsigned int color);
EXTERN void pdgui_polyline_create(t_canvas *canvas, const char *item,
    const char *group, const int *coords, int ncoords, int width,
    unsigned int color);
EXTERN void pdgui_polyline_configure(t_canvas *canvas, const char *item,
    const int *coords, int ncoords, int width, unsigned int color);
EXTERN void pdgui_polygon_create(t_canvas *canvas, const char *item,
    const char *group, const int *coords, int ncoords, int width,
    unsigned int fill, unsigned int outline);
EXTERN void pdgui_polygon_configure(t_canvas *canvas, const char *item,
    const int *coords, int ncoords, int width, unsigned int fill,
    unsigned int outline);
EXTERN void pdgui_polygon_create_miter(t_canvas *canvas, const char *item,
    const char *group, const int *coords, int ncoords, int width,
    unsigned int fill, unsigned int outline);
EXTERN void pdgui_path_create(t_canvas *canvas, const char *item,
    const t_word *coords, int ncoords, int closed, int smooth, int width,
    unsigned int fill, unsigned int outline);
EXTERN void pdgui_path_set_points(t_canvas *canvas, const char *item,
    const t_word *coords, int ncoords);
/* Text coordinates refer to the vertical center of the left edge.  Font size
 * is in pixels. */
EXTERN void pdgui_text_create(t_canvas *canvas, const char *item,
    const char *group, int x, int y);
EXTERN void pdgui_text_create_plain(t_canvas *canvas, const char *item,
    const char *group, int x, int y);
EXTERN void pdgui_text_create_grouped(t_canvas *canvas, const char *item,
    const char *group, const char *collection, int x, int y);
EXTERN void pdgui_text_create_anchored(t_canvas *canvas, const char *item,
    const char *group, const char *collection, int x, int y,
    const char *text, t_pdgui_anchor anchor, const char *font,
    int fontsize, const char *weight, unsigned int color);
EXTERN void pdgui_text_configure(t_canvas *canvas, const char *item,
    int x, int y, const char *font, int fontsize, const char *weight,
    unsigned int color);
EXTERN void pdgui_polyline_create_dashed(t_canvas *canvas, const char *item,
    const char *group, const int *coords, int ncoords, int width,
    unsigned int color, int dashed);
EXTERN void pdgui_polyline_set_points(t_canvas *canvas, const char *item,
    const int *coords, int ncoords);
EXTERN void pdgui_text_set_color(t_canvas *canvas, const char *item,
    unsigned int color);
EXTERN void pdgui_text_set_position(t_canvas *canvas, const char *item,
    int x, int y);
EXTERN void pdgui_text_set_content(t_canvas *canvas, const char *item,
    const char *text);
EXTERN void pdgui_text_set_font(t_canvas *canvas, const char *item,
    const char *font, int fontsize, const char *weight);
EXTERN void pdgui_text_set_content_color(t_canvas *canvas, const char *item,
    const char *text, unsigned int color);
EXTERN void pdgui_canvas_text_create(t_canvas *canvas, const char *item,
    int x, int y, const char *text, int fontsize, unsigned int color);
EXTERN void pdgui_canvas_text_create_grouped(t_canvas *canvas,
    const char *item, const char *collection, int x, int y,
    const char *text, int fontsize, unsigned int color);
EXTERN void pdgui_canvas_text_create_label(t_canvas *canvas, const char *item,
    int x, int y, const char *text, int fontsize, unsigned int color);
EXTERN void pdgui_text_set_selection(t_canvas *canvas, const char *item,
    int start, int end);
EXTERN void pdgui_text_set_editing(t_canvas *canvas, const char *item,
    int state);
EXTERN void pdgui_item_destroy(t_canvas *canvas, const char *item);
EXTERN void pdgui_item_move(t_canvas *canvas, const char *item,
    int dx, int dy);
EXTERN void pdgui_item_lower(t_canvas *canvas, const char *item,
    const char *below);
EXTERN void pdgui_item_raise(t_canvas *canvas, const char *item,
    const char *above);
EXTERN void pdgui_item_set_fill(t_canvas *canvas, const char *item,
    unsigned int fill);
EXTERN void pdgui_item_set_outline(t_canvas *canvas, const char *item,
    unsigned int outline);
EXTERN void pdgui_item_set_width(t_canvas *canvas, const char *item, int width);
EXTERN void pdgui_item_raise_top(t_canvas *canvas, const char *item);
EXTERN void pdgui_canvas_clear(t_canvas *canvas);
EXTERN void pdgui_canvas_set_colors(t_canvas *canvas,
    unsigned int background, unsigned int foreground);
EXTERN void pdgui_canvas_set_patchcords_foreground(t_canvas *canvas,
    int state);
EXTERN void pdgui_patchcord_create(t_canvas *canvas, const char *item,
    int x1, int y1, int x2, int y2, int width, unsigned int color);
EXTERN void pdgui_patchcord_set_position(t_canvas *canvas, const char *item,
    int x1, int y1, int x2, int y2);

EXTERN void pdgui_window_menu_update(void);
EXTERN void pdgui_canvas_set_title(t_canvas *canvas, const char *directory,
    const char *name, const char *display_name, int dirty, int editmode);
EXTERN void pdgui_canvas_update_scrollbars(t_canvas *canvas);
EXTERN void pdgui_set_dsp_state(int state);
EXTERN void pdgui_busy_release(void);
EXTERN void pdgui_struct_menu_clear(void);
EXTERN void pdgui_struct_menu_add(const char *name);
EXTERN void pdgui_undo_menu(t_canvas *canvas, const char *undo,
    const char *redo);
EXTERN void pdgui_clipboard_set(const char *text, int length);
EXTERN void pdgui_canvas_set_cursor(t_canvas *canvas, int cursor);
EXTERN void pdgui_canvas_raise_window(t_canvas *canvas);
EXTERN void pdgui_canvas_create_window(t_canvas *canvas, int width,
    int height, const char *position, int editmode, unsigned int background,
    unsigned int foreground, int custom_colors);
EXTERN void pdgui_canvas_set_parents(t_canvas *canvas, int count,
    t_canvas **parents);
EXTERN void pdgui_window_destroy(void *window);
EXTERN void pdgui_canvas_dialog_set_text(const char *text);
EXTERN void pdgui_canvas_popup(t_canvas *canvas, int x, int y,
    int can_properties, int can_open);
EXTERN void pdgui_canvas_export_postscript(t_canvas *canvas,
    const char *filename);
EXTERN void pdgui_confirm(t_canvas *canvas, const char *window,
    int message_count, const char **messages, t_symbol *callback,
    int callback_argc, const t_atom *callback_argv,
    const char *default_answer);
EXTERN void pdgui_canvas_close_confirm(t_canvas *canvas, t_symbol *callback,
    int callback_argc, const t_atom *callback_argv);
EXTERN void pdgui_find_result(t_canvas *canvas, int found, int find_index,
    int object_index);
EXTERN void pdgui_canvas_paste(t_canvas *canvas, int into_text);
EXTERN void pdgui_canvas_set_pointer(t_canvas *canvas, int x, int y);
EXTERN void pdgui_pd_texteditor(const char *text, int length);
EXTERN void pdgui_canvas_set_editmode(t_canvas *canvas, int state);
EXTERN void pdgui_array_set_page(const char *name, int page, int pages,
    int page_size);
EXTERN void pdgui_array_set_data(const char *name, int offset, int length,
    const t_word *data);
EXTERN void pdgui_array_set_focus(const char *name, int item);
EXTERN void pdgui_array_close(const char *name);
EXTERN void pdgui_array_refresh(const char *name);
EXTERN void pdgui_missing_object(void *object, const char *name);
EXTERN void pdgui_canvas_save_as(t_canvas *canvas, const char *name,
    const char *directory, int destroy_after_save);
EXTERN void pdgui_confirm_quit(const char *message);
EXTERN void pdgui_console_post(const char *text);
EXTERN void pdgui_console_log(const void *object, int level, const char *text);
EXTERN void pdgui_plugin_dispatch(int argc, const t_atom *argv);
EXTERN void pdgui_preferences_open(void);
EXTERN void pdgui_ping(void);
EXTERN void pdgui_preferences_set_paths(int nsearch, const char **search,
    int ntemp, const char **temp, int nstatic, const char **static_paths);
EXTERN void pdgui_preferences_set_startup(int nlibs, const char **libs,
    int verbose, int use_standard_path, int defeat_realtime,
    int zoom_open, const char *flags);
EXTERN void pdgui_deken_set_platform(const char *os, const char *cpu,
    t_float pointer_bits, t_float float_bits);
EXTERN void pdgui_watchdog_start(void);
EXTERN void pdgui_startup(int major, int minor, int bugfix, const char *test,
    int naudio_apis, const char *const *audio_api_names,
    const int *audio_api_ids, int nmidi_apis,
    const char *const *midi_api_names, const int *midi_api_ids,
    const char *font, const char *font_weight);
EXTERN void pdgui_set_audio_api(int api);
EXTERN void pdgui_set_audio_running(int state);
EXTERN void pdgui_set_dio_state(int state);
EXTERN void pdgui_audio_set_configuration(int nin_devices,
    const char *const *in_devices, int nin_used, const t_float *in_used,
    int nin_channels, const t_float *in_channels, int nout_devices,
    const char *const *out_devices, int nout_used, const t_float *out_used,
    int nout_channels, const t_float *out_channels, const char *sample_rate,
    const char *block_size, int advance, const char *callback, int multi);
EXTERN void pdgui_audio_refresh(void);
EXTERN void pdgui_set_midi_api(int api);
EXTERN void pdgui_midi_set_configuration(int api, int nin_devices,
    const char *const *in_devices, int nin_used, const t_float *in_used,
    int nout_devices, const char *const *out_devices, int nout_used,
    const t_float *out_used);
EXTERN void pdgui_midi_refresh(void);
EXTERN void pdgui_named_window_destroy(const char *window);
EXTERN void pdgui_open_panel(const char *callback, const char *path, int mode,
    t_canvas *canvas);
EXTERN void pdgui_save_panel(const char *callback, const char *path,
    t_canvas *canvas);
EXTERN void pdgui_open_file(const char *path);
EXTERN void pdgui_textwindow_clear(void *window);
EXTERN void pdgui_textwindow_append(void *window, const char *text);
EXTERN void pdgui_textwindow_append_atoms(void *window, int argc,
    const t_atom *argv);
EXTERN void pdgui_textwindow_set_dirty(void *window, int dirty);
EXTERN void pdgui_textwindow_raise(void *window);
EXTERN void pdgui_textwindow_open(void *window, const char *geometry,
    const char *title, int font_size);
EXTERN void pdgui_textwindow_close(void *window);
EXTERN void pdgui_array_dialog(t_pd *owner, void *key, const char *name,
    int size, int flags, int create_new);
EXTERN void pdgui_array_listview_open(t_pd *owner, void *key,
    const char *name, int page);
EXTERN void pdgui_gatom_dialog(t_pd *owner, void *key, int width,
    t_float lower, t_float upper, int label_position, const char *label,
    const char *receive, const char *send, int font_size);
EXTERN void pdgui_iemgui_dialog(t_pd *owner, void *key,
    const char *object_name, t_float width, t_float width_min,
    t_float height, t_float height_min, t_float range_min, t_float range_max,
    int schedule, int mode, const char *mode0, const char *mode1,
    int loadinit, int steady, int number, const char *send,
    const char *receive, const char *label, int label_x, int label_y,
    int font_style, int font_size, unsigned int background,
    unsigned int foreground, unsigned int label_color);
EXTERN void pdgui_data_dialog(t_pd *owner, void *key, const char *text,
    int length);
EXTERN t_pdgui_stub *pdgui_dialog_stub_new(t_pd *owner, void *key);
EXTERN void pdgui_dialog_stub_send(t_pdgui_stub *stub, const char *selector,
    int argc, const t_atom *argv);
EXTERN const char *pdgui_dialog_stub_name(t_pdgui_stub *stub);
EXTERN void pdgui_dialog_stub_close(t_pdgui_stub *stub);
EXTERN void pdgui_canvas_properties_dialog(t_pd *owner, void *key,
    t_float xscale, t_float yscale, int is_graph, t_float x1, t_float y1,
    t_float x2, t_float y2, int pixel_width, int pixel_height,
    int xmargin, int ymargin);
EXTERN void pdgui_font_dialog(t_pd *owner, void *key, int font_size);
EXTERN void pdgui_audio_dialog_open(t_pd *owner, void *key);
EXTERN void pdgui_midi_dialog_open(t_pd *owner, void *key);
EXTERN void pdgui_path_dialog_open(t_pd *owner, void *key,
    int use_standard_path, int verbose);
EXTERN void pdgui_startup_dialog_open(t_pd *owner, void *key,
    int defeat_realtime, const char *flags);
EXTERN void pdgui_exit(void);

#endif /* PD_G_GUI_H */
