/* Experimental GTK4 implementation of the typed Pd GUI renderer. */
#include "m_pd.h"
#include "g_canvas.h"
#include "g_gui.h"
#include "m_imp.h"
#include "s_stuff.h"
#include <gtk/gtk.h>
#include <cairo-ps.h>
#include <pango/pangocairo.h>
#include <stdint.h>
#include <string.h>

#define PDGUI_STRINGIFY_(value) #value
#define PDGUI_STRINGIFY(value) PDGUI_STRINGIFY_(value)
#define PDGUI_VERSION_STRING                                                                    \
    PDGUI_STRINGIFY(PD_MAJOR_VERSION) "." PDGUI_STRINGIFY(PD_MINOR_VERSION) "."                \
        PDGUI_STRINGIFY(PD_BUGFIX_VERSION) PD_TEST_VERSION

typedef enum { GI_RECT, GI_OVAL, GI_PATH, GI_TEXT } t_gi_type;
typedef struct _gi {
    t_gi_type type;
    char *name, *group, *collection, *text, *font, *weight;
    double *points;
    int npoints, width, dashed, closed, fontsize, editing, selstart, selend;
    unsigned int fill, outline;
    t_pdgui_anchor anchor;
    struct _gi *next;
} t_gi;

typedef struct _gc {
    t_canvas *canvas;
    GtkWidget *window, *area, *popup;
    t_clock *map_clock;
    t_gi *items;
    char *title;
    int editmode;
    unsigned int background, foreground;
    struct _gc *next;
} t_gc;

extern void sys_doneglobinit(void);
static t_gc *gc_list;
static int gtk_ready;
static GtkWidget *gtk_main_window;
static GtkWidget *gtk_console_view;
static GtkTextBuffer *gtk_console_buffer;
static GtkWidget *gtk_dsp_switch;
static GtkWidget *gtk_audio_label;
static int gtk_dsp_sync;
static int gtk_exiting;
static int gtk_dsp_state;
static int gtk_audio_api;
static int gtk_untitled_number = 1;
static GMenu *gtk_recent_menu;
static GMenu *gtk_window_menu;
static GPtrArray *gtk_recent_files;
static char *gtk_canvas_dialog_text;
static guint gtk_watchdog_source;

static gboolean gtk_watchdog_tick(gpointer data) {
    (void)data;
    glob_watchdog(0);
    return (G_SOURCE_CONTINUE);
}

#define GTK_PREF_AUDIO_DEVICES 4
#define GTK_PREF_MIDI_DEVICES 9

typedef struct _gtk_preferences {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *use_standard_path;
    GtkWidget *verbose;
    GtkWidget *defeat_realtime;
    GtkWidget *zoom_open;
    GtkWidget *flags;
    GtkWidget *paths;
    GtkWidget *libraries;
    GtkWidget *audio_input[GTK_PREF_AUDIO_DEVICES];
    GtkWidget *audio_input_channels[GTK_PREF_AUDIO_DEVICES];
    GtkWidget *audio_output[GTK_PREF_AUDIO_DEVICES];
    GtkWidget *audio_output_channels[GTK_PREF_AUDIO_DEVICES];
    GtkWidget *sample_rate;
    GtkWidget *block_size;
    GtkWidget *advance;
    GtkWidget *callback;
    GtkWidget *midi_input[GTK_PREF_MIDI_DEVICES];
    GtkWidget *midi_output[GTK_PREF_MIDI_DEVICES];
} t_gtk_preferences;

typedef struct _gtk_preferences_data {
    GPtrArray *paths;
    GPtrArray *temporary_paths;
    GPtrArray *static_paths;
    GPtrArray *libraries;
    GPtrArray *audio_inputs;
    GPtrArray *audio_outputs;
    GPtrArray *midi_inputs;
    GPtrArray *midi_outputs;
    t_float audio_input_used[GTK_PREF_AUDIO_DEVICES];
    t_float audio_input_channels[GTK_PREF_AUDIO_DEVICES];
    t_float audio_output_used[GTK_PREF_AUDIO_DEVICES];
    t_float audio_output_channels[GTK_PREF_AUDIO_DEVICES];
    t_float midi_input_used[GTK_PREF_MIDI_DEVICES];
    t_float midi_output_used[GTK_PREF_MIDI_DEVICES];
    char *sample_rate;
    char *block_size;
    char *callback;
    char *flags;
    int advance;
    int multi;
    int verbose;
    int use_standard_path;
    int defeat_realtime;
    int zoom_open;
} t_gtk_preferences_data;

static t_gtk_preferences gtk_preferences;
static t_gtk_preferences_data gtk_preferences_data;

typedef struct _gtk_canvas_properties {
    GtkWidget *window;
    GtkWidget *entries[10];
    GtkWidget *graph;
    GtkWidget *hide_name;
    GtkWidget *object_text;
    t_pdgui_stub *stub;
    struct _gtk_canvas_properties *next;
} t_gtk_canvas_properties;

enum {
    IEM_WIDTH,
    IEM_HEIGHT,
    IEM_RANGE_MIN,
    IEM_RANGE_MAX,
    IEM_NUMBER,
    IEM_SEND,
    IEM_RECEIVE,
    IEM_LABEL,
    IEM_LABEL_X,
    IEM_LABEL_Y,
    IEM_FONT_SIZE,
    IEM_ENTRY_COUNT
};

typedef struct _gtk_iemgui_properties {
    GtkWidget *window;
    GtkWidget *entries[IEM_ENTRY_COUNT];
    GtkWidget *mode;
    GtkWidget *loadinit;
    GtkWidget *steady;
    GtkWidget *font;
    GtkWidget *colors[3];
    t_pdgui_stub *stub;
    int has_loadinit, has_mode, has_steady;
    struct _gtk_iemgui_properties *next;
} t_gtk_iemgui_properties;

static t_gtk_canvas_properties *gtk_canvas_properties_list;
static t_gtk_iemgui_properties *gtk_iemgui_properties_list;

static GtkWidget *gtk_menu_bar_new(t_gc *c, int patch_window);
static void gtk_update_window_menu(void);
static void gtk_update_recent_menu(void);
static void gtk_new_patch(void);

static gboolean gtk_main_keypress(GtkEventControllerKey *controller, guint keyval,
                                  guint keycode, GdkModifierType state, gpointer data) {
    guint key = gdk_keyval_to_lower(keyval);
#ifdef MACOSX
    int primary = ((state & GDK_META_MASK) != 0);
#else
    int primary = ((state & GDK_CONTROL_MASK) != 0);
#endif
    (void)controller;
    (void)keycode;
    (void)data;
    if (primary && !(state & GDK_SHIFT_MASK) && key == GDK_KEY_n) {
        gtk_new_patch();
        return (TRUE);
    }
    return (FALSE);
}

static void gtk_console_append(const char *text, int level) {
    GtkTextIter end;
    const char *tag = 0;
    if (!gtk_console_buffer || !text) {
        return;
    }
    if (level == PD_CRITICAL || level == PD_ERROR) {
        tag = "error";
    } else if (level >= PD_VERBOSE) {
        tag = "verbose";
    } else if (level >= PD_DEBUG) {
        tag = "debug";
    }
    gtk_text_buffer_get_end_iter(gtk_console_buffer, &end);
    if (tag) {
        gtk_text_buffer_insert_with_tags_by_name(gtk_console_buffer, &end, text, -1, tag, NULL);
    } else {
        gtk_text_buffer_insert(gtk_console_buffer, &end, text, -1);
    }
    gtk_text_buffer_get_end_iter(gtk_console_buffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(gtk_console_view), &end, 0, FALSE, 0, 0);
}
static void gtk_console_clear(GtkButton *button, gpointer data) {
    (void)button;
    (void)data;
    if (gtk_console_buffer) {
        gtk_text_buffer_set_text(gtk_console_buffer, "", 0);
    }
}
static void gtk_dsp_changed(GObject *object, GParamSpec *parameter, gpointer data) {
    t_atom state;
    t_pd *receiver;
    (void)parameter;
    (void)data;
    if (gtk_dsp_sync) {
        return;
    }
    receiver = gensym("pd")->s_thing;
    if (receiver) {
        SETFLOAT(&state, gtk_switch_get_active(GTK_SWITCH(object)) ? 1 : 0);
        pd_typedmess(receiver, gensym("dsp"), 1, &state);
    }
}
static gboolean gtk_main_close(GtkWindow *window, gpointer data) {
    t_pd *receiver;
    (void)window;
    (void)data;
    if (gtk_exiting) {
        return (FALSE);
    }
    receiver = gensym("pd")->s_thing;
    if (receiver) {
        pd_typedmess(receiver, gensym("quit"), 0, 0);
    }
    return (TRUE);
}
static void gtk_main_create(void) {
    GtkWidget *box, *header, *title, *clear, *scroll;
    GtkEventController *key;
    char window_title[80];
    pd_snprintf(window_title, sizeof(window_title), "Pure Data %d.%d.%d", PD_MAJOR_VERSION,
                PD_MINOR_VERSION, PD_BUGFIX_VERSION);
    gtk_main_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(gtk_main_window), window_title);
    gtk_window_set_default_size(GTK_WINDOW(gtk_main_window), 600, 420);
    key = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(key, GTK_PHASE_CAPTURE);
    g_signal_connect(key, "key-pressed", G_CALLBACK(gtk_main_keypress), NULL);
    gtk_widget_add_controller(gtk_main_window, key);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(gtk_main_window), box);
    gtk_box_append(GTK_BOX(box), gtk_menu_bar_new(0, 0));
    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(header, 12);
    gtk_widget_set_margin_end(header, 12);
    gtk_widget_set_margin_top(header, 8);
    gtk_widget_set_margin_bottom(header, 8);
    gtk_box_append(GTK_BOX(box), header);

    title = gtk_label_new("Pure Data Console");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(header), title);
    gtk_audio_label = gtk_label_new("DSP");
    gtk_box_append(GTK_BOX(header), gtk_audio_label);
    gtk_dsp_switch = gtk_switch_new();
    gtk_widget_set_valign(gtk_dsp_switch, GTK_ALIGN_CENTER);
    g_signal_connect(gtk_dsp_switch, "notify::active", G_CALLBACK(gtk_dsp_changed), 0);
    gtk_box_append(GTK_BOX(header), gtk_dsp_switch);
    clear = gtk_button_new_with_label("Clear");
    g_signal_connect(clear, "clicked", G_CALLBACK(gtk_console_clear), 0);
    gtk_box_append(GTK_BOX(header), clear);

    scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(box), scroll);
    gtk_console_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(gtk_console_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(gtk_console_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(gtk_console_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gtk_console_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(gtk_console_view), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(gtk_console_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(gtk_console_view), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(gtk_console_view), 8);
    gtk_console_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_console_view));
    gtk_text_buffer_create_tag(gtk_console_buffer, "error", "foreground", "#c01c28", NULL);
    gtk_text_buffer_create_tag(gtk_console_buffer, "debug", "foreground", "#1c71d8", NULL);
    gtk_text_buffer_create_tag(gtk_console_buffer, "verbose", "foreground", "#77767b", NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), gtk_console_view);
    g_signal_connect(gtk_main_window, "close-request", G_CALLBACK(gtk_main_close), 0);
    gtk_window_present(GTK_WINDOW(gtk_main_window));
}

static char *sdup(const char *s) {
    return (g_strdup(s ? s : ""));
}
static int pdgui_gtk_backend_poll(void) {
    int didsomething = 0;
    if (!gtk_ready) {
        return (0);
    }
    while (g_main_context_iteration(0, FALSE)) {
        didsomething = 1;
    }
    return (didsomething);
}
static int gtk_start(void) {
    if (gtk_ready) {
        return (1);
    }
    if (!gtk_init_check()) {
        pd_error(0, "GTK renderer: cannot initialize GTK4");
        return (0);
    }
    gtk_ready = 1;
    gtk_main_create();
    return (1);
}
static int pdgui_gtk_backend_init(const char *libdir) {
    t_audiosettings settings;
    (void)libdir;
    if (!gtk_start()) {
        return (-1);
    }
#if defined(__linux__)
        /* Pd's non-Apple default is bold for the legacy Tcl/Tk renderer.
           GTK uses the platform font renderer directly and should default to
           the same normal weight used by the macOS GTK backend. */
    strcpy(sys_fontweight, "normal");
#endif
        /* GTK/AppKit must be serviced by the process main thread.  Pd's
           callback audio scheduler polls the GUI from the audio callback, so
           use polling audio while this in-process backend is active. */
    sys_get_audio_settings(&settings);
    settings.a_callback = 0;
    sys_set_audio_settings(&settings);
    sys_doneglobinit();
    return (0);
}
static t_gc *gc_find(t_canvas *canvas) {
    t_gc *c;
    for (c = gc_list; c; c = c->next) {
        if (c->canvas == canvas) {
            return (c);
        }
    }
    return (0);
}
static int gi_match(t_gi *i, const char *s) {
    return (s && (!strcmp(i->name, s) || !strcmp(i->group, s) || !strcmp(i->collection, s)));
}
static void color(cairo_t *cr, unsigned int c) {
    cairo_set_source_rgb(cr, ((c >> 16) & 255) / 255., ((c >> 8) & 255) / 255., (c & 255) / 255.);
}
static void redraw(t_gc *c) {
    if (c) {
        gtk_widget_queue_draw(c->area);
    }
}
static void points_int(t_gi *i, const int *v, int n) {
    int k;
    i->points = (double *)g_realloc(i->points, n * sizeof(*i->points));
    i->npoints = n;
    for (k = 0; k < n; k++) {
        i->points[k] = v[k];
    }
}
static void points_word(t_gi *i, const t_word *v, int n) {
    int k;
    i->points = (double *)g_realloc(i->points, n * sizeof(*i->points));
    i->npoints = n;
    for (k = 0; k < n; k++) {
        i->points[k] = v[k].w_float;
    }
}
static t_gi *gi_new(t_gc *c, t_gi_type type, const char *name, const char *group,
                    const char *collection) {
    t_gi *i = (t_gi *)g_malloc0(sizeof(*i)), **tail = &c->items;
    i->type = type;
    i->name = sdup(name);
    i->group = sdup(group);
    i->collection = sdup(collection);
    i->text = sdup("");
    i->font = sdup("Monospace");
    i->weight = sdup("normal");
    i->fill = PDGUI_COLOR_NONE;
    i->outline = 0;
    i->fontsize = 12;
    i->anchor = PDGUI_ANCHOR_WEST;
    while (*tail) {
        tail = &(*tail)->next;
    }
    *tail = i;
    return (i);
}
static void gi_free(t_gi *i) {
    g_free(i->name);
    g_free(i->group);
    g_free(i->collection);
    g_free(i->text);
    g_free(i->font);
    g_free(i->weight);
    g_free(i->points);
    g_free(i);
}
static void draw_path(cairo_t *cr, t_gi *i) {
    int k;
    double dash[2] = {4, 4};
    if (i->npoints < 2) {
        return;
    }
    cairo_move_to(cr, i->points[0], i->points[1]);
    for (k = 2; k + 1 < i->npoints; k += 2) {
        cairo_line_to(cr, i->points[k], i->points[k + 1]);
    }
    if (i->closed) {
        cairo_close_path(cr);
    }
    if (i->fill != PDGUI_COLOR_NONE) {
        color(cr, i->fill), cairo_fill_preserve(cr);
    }
    if (i->width && i->outline != PDGUI_COLOR_NONE) {
        cairo_set_line_width(cr, i->width);
        if (i->dashed) {
            cairo_set_dash(cr, dash, 2, 0);
        }
        color(cr, i->outline);
        cairo_stroke(cr);
        cairo_set_dash(cr, 0, 0, 0);
    } else {
        cairo_new_path(cr);
    }
}
static void draw_text(cairo_t *cr, t_gi *i) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc;
    char spec[256];
    int w, h;
    double x = i->points[0], y = i->points[1];
    pd_snprintf(spec, sizeof(spec), "%s %s %dpx", i->font, i->weight,
                i->fontsize > 0 ? i->fontsize : 12);
    desc = pango_font_description_from_string(spec);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, i->text, -1);
    if (i->editing && i->selstart != i->selend) {
        PangoAttrList *attributes = pango_attr_list_new();
        PangoAttribute *background = pango_attr_background_new(45232, 45232, 45232);
        PangoAttribute *foreground = pango_attr_foreground_new(0, 0, 0);
        int nchars = (int)g_utf8_strlen(i->text, -1);
        int startchar = CLAMP(MIN(i->selstart, i->selend), 0, nchars);
        int endchar = CLAMP(MAX(i->selstart, i->selend), 0, nchars);
        guint startbyte = (guint)(g_utf8_offset_to_pointer(i->text, startchar) - i->text);
        guint endbyte = (guint)(g_utf8_offset_to_pointer(i->text, endchar) - i->text);
        background->start_index = foreground->start_index = startbyte;
        background->end_index = foreground->end_index = endbyte;
        pango_attr_list_insert(attributes, background);
        pango_attr_list_insert(attributes, foreground);
        pango_layout_set_attributes(layout, attributes);
        pango_attr_list_unref(attributes);
    }
    pango_layout_get_pixel_size(layout, &w, &h);
    if (i->anchor == PDGUI_ANCHOR_CENTER || i->anchor == PDGUI_ANCHOR_NORTH ||
        i->anchor == PDGUI_ANCHOR_SOUTH) {
        x -= .5 * w;
    } else if (i->anchor == PDGUI_ANCHOR_EAST || i->anchor == PDGUI_ANCHOR_NORTH_EAST ||
               i->anchor == PDGUI_ANCHOR_SOUTH_EAST) {
        x -= w;
    }
    if (i->anchor == PDGUI_ANCHOR_CENTER || i->anchor == PDGUI_ANCHOR_EAST ||
        i->anchor == PDGUI_ANCHOR_WEST) {
        y -= .5 * h;
    } else if (i->anchor == PDGUI_ANCHOR_SOUTH || i->anchor == PDGUI_ANCHOR_SOUTH_EAST ||
               i->anchor == PDGUI_ANCHOR_SOUTH_WEST) {
        y -= h;
    }
    color(cr, i->fill == PDGUI_COLOR_NONE ? 0 : i->fill);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
    if (i->editing && i->selstart == i->selend) {
        PangoRectangle caret;
        int nchars = (int)g_utf8_strlen(i->text, -1);
        int charindex = CLAMP(i->selstart, 0, nchars);
        int byteindex = (int)(g_utf8_offset_to_pointer(i->text, charindex) - i->text);
        double caretx, carety, careth;
        pango_layout_get_cursor_pos(layout, byteindex, &caret, NULL);
        caretx = x + (double)caret.x / PANGO_SCALE;
        carety = y + (double)caret.y / PANGO_SCALE;
        careth = (double)caret.height / PANGO_SCALE;
        cairo_save(cr);
        color(cr, 0x0000ff);
        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, floor(caretx) + .5, carety);
        cairo_line_to(cr, floor(caretx) + .5, carety + careth);
        cairo_stroke(cr);
        cairo_restore(cr);
    }
    pango_font_description_free(desc);
    g_object_unref(layout);
}
static void draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    t_gc *c = (t_gc *)data;
    t_gi *i;
    color(cr, c->background);
    cairo_paint(cr);
    for (i = c->items; i; i = i->next) {
        if (i->type == GI_TEXT) {
            draw_text(cr, i);
        } else if (i->type == GI_PATH) {
            draw_path(cr, i);
        } else {
            double x = i->points[0], y = i->points[1];
            double w = i->points[2] - x, h = i->points[3] - y;
            if (i->type == GI_OVAL) {
                cairo_save(cr);
                cairo_translate(cr, x + .5 * w, y + .5 * h);
                cairo_scale(cr, .5 * w, .5 * h);
                cairo_arc(cr, 0, 0, 1, 0, 2 * G_PI);
                cairo_restore(cr);
            } else {
                cairo_rectangle(cr, x, y, w, h);
            }
            if (i->fill != PDGUI_COLOR_NONE) {
                color(cr, i->fill), cairo_fill_preserve(cr);
            }
            if (i->width && i->outline != PDGUI_COLOR_NONE) {
                cairo_set_line_width(cr, i->width), color(cr, i->outline), cairo_stroke(cr);
            } else {
                cairo_new_path(cr);
            }
        }
    }
}
static void mouse(t_gc *c, const char *sel, double x, double y, int button, GdkModifierType state) {
    t_atom a[4];
    int modifiers;
    modifiers = ((state & GDK_SHIFT_MASK) ? 1 : 0) | ((state & GDK_CONTROL_MASK) ? 2 : 0) |
                ((state & GDK_ALT_MASK) ? 4 : 0);
    if (button == GDK_BUTTON_SECONDARY) {
        modifiers |= 8;
    }
    SETFLOAT(a, x);
    SETFLOAT(a + 1, y);
    SETFLOAT(a + 2, button);
    SETFLOAT(a + 3, modifiers);
    pd_typedmess(&c->canvas->gl_pd, gensym(sel), 4, a);
}
static void canvas_message(t_gc *c, const char *selector) {
    pd_typedmess(&c->canvas->gl_pd, gensym(selector), 0, 0);
}
static void canvas_message_float(t_gc *c, const char *selector, t_float value) {
    t_atom atom;
    SETFLOAT(&atom, value);
    pd_typedmess(&c->canvas->gl_pd, gensym(selector), 1, &atom);
}

typedef enum {
    GTK_CHOOSE_OPEN,
    GTK_CHOOSE_SAVE_CANVAS,
    GTK_CHOOSE_SAVE_LOG,
    GTK_CHOOSE_PRINT,
    GTK_CHOOSE_PREF_SAVE,
    GTK_CHOOSE_PREF_LOAD
} t_gtk_choose_action;

typedef struct _gtk_choose {
    t_gtk_choose_action action;
    t_canvas *canvas;
    int destroy_after_save;
} t_gtk_choose;

static GtkWindow *gtk_menu_parent(t_gc *c) {
    return (GTK_WINDOW(c ? c->window : gtk_main_window));
}

static t_pd *gtk_pd_receiver(void) {
    return (gensym("pd")->s_thing);
}

static void gtk_pd_message(const char *selector, int argc, t_atom *argv) {
    t_pd *receiver = gtk_pd_receiver();
    if (receiver) {
        pd_typedmess(receiver, gensym(selector), argc, argv);
    }
}

static void gtk_open_patch(const char *filename, const char *directory, int help) {
    t_atom atoms[3];
    SETSYMBOL(atoms, gensym(filename));
    SETSYMBOL(atoms + 1, gensym(directory));
    SETFLOAT(atoms + 2, help);
    gtk_pd_message("open", 3, atoms);
}

static void gtk_open_path(const char *path, int help) {
    char *directory = g_path_get_dirname(path);
    char *filename = g_path_get_basename(path);
    gtk_open_patch(filename, directory, help);
    g_free(filename);
    g_free(directory);
}

static void gtk_open_document(const char *directory, const char *filename) {
    char *path = g_build_filename(sys_libdir->s_name, directory, filename, NULL);
    gtk_open_path(path, 1);
    g_free(path);
}

static void gtk_open_uri(t_gc *c, const char *uri) {
    gtk_show_uri(gtk_menu_parent(c), uri, GDK_CURRENT_TIME);
}

static void gtk_open_local_document(t_gc *c, const char *directory, const char *filename) {
    char *path = g_build_filename(sys_libdir->s_name, directory, filename, NULL);
    char *uri = g_filename_to_uri(path, NULL, NULL);
    if (uri) {
        gtk_open_uri(c, uri);
        g_free(uri);
    }
    g_free(path);
}

static void gtk_recent_add(const char *path) {
    unsigned int i;
    if (!gtk_recent_files) {
        gtk_recent_files = g_ptr_array_new_with_free_func(g_free);
    }
    for (i = 0; i < gtk_recent_files->len; i++) {
        if (!strcmp((const char *)g_ptr_array_index(gtk_recent_files, i), path)) {
            g_ptr_array_remove_index(gtk_recent_files, i);
            break;
        }
    }
    g_ptr_array_insert(gtk_recent_files, 0, g_strdup(path));
    while (gtk_recent_files->len > 10) {
        g_ptr_array_remove_index(gtk_recent_files, gtk_recent_files->len - 1);
    }
    gtk_update_recent_menu();
}

static void gtk_console_copy(void) {
    GtkTextIter first, last;
    if (gtk_console_buffer &&
        gtk_text_buffer_get_selection_bounds(gtk_console_buffer, &first, &last)) {
        char *text = gtk_text_buffer_get_text(gtk_console_buffer, &first, &last, FALSE);
        GdkClipboard *clipboard = gtk_widget_get_clipboard(gtk_console_view);
        gdk_clipboard_set_text(clipboard, text);
        g_free(text);
    }
}

typedef struct _gtk_canvas_paste {
    t_canvas *canvas;
    int into_text;
} t_gtk_canvas_paste;

static void gtk_clipboard_set(const char *text, int length) {
    GdkDisplay *display = gdk_display_get_default();
    if (display && text && length >= 0) {
        char *copy = g_strndup(text, length);
        gdk_clipboard_set_text(gdk_display_get_clipboard(display), copy);
        g_free(copy);
    }
}

static void gtk_canvas_paste_send(t_canvas *canvas, const char *selector, int argc, t_atom *argv) {
    pd_typedmess(&canvas->gl_pd, gensym(selector), argc, argv);
}

static void gtk_canvas_paste_text(t_canvas *canvas, int into_text, const char *text) {
    const char *p;
    t_atom atoms[32];
    int argc = 0;
    if (into_text) {
        for (p = text; *p; p = g_utf8_next_char(p)) {
            gunichar character = g_utf8_get_char_validated(p, -1);
            if (character == (gunichar)-1 || character == (gunichar)-2) {
                character = (unsigned char)*p;
            }
            SETFLOAT(atoms, 1);
            SETFLOAT(atoms + 1, character);
            SETFLOAT(atoms + 2, 0);
            gtk_canvas_paste_send(canvas, "key", 3, atoms);
        }
        return;
    }
    SETFLOAT(atoms, -1);
    gtk_canvas_paste_send(canvas, "pastechars", 1, atoms);
    for (p = text; *p; p = g_utf8_next_char(p)) {
        gunichar character = g_utf8_get_char_validated(p, -1);
        if (character == (gunichar)-1 || character == (gunichar)-2) {
            character = (unsigned char)*p;
        }
        SETFLOAT(atoms + argc, character);
        if (++argc == 32) {
            gtk_canvas_paste_send(canvas, "pastechars", argc, atoms);
            argc = 0;
        }
    }
    if (argc) {
        gtk_canvas_paste_send(canvas, "pastechars", argc, atoms);
    }
    SETFLOAT(atoms, -2);
    gtk_canvas_paste_send(canvas, "pastechars", 1, atoms);
}

static void gtk_canvas_paste_ready(GObject *source, GAsyncResult *result, gpointer data) {
    t_gtk_canvas_paste *request = (t_gtk_canvas_paste *)data;
    GError *error = NULL;
    char *text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source), result, &error);
    if (text && gc_find(request->canvas)) {
        gtk_canvas_paste_text(request->canvas, request->into_text, text);
    }
    g_clear_error(&error);
    g_free(text);
    g_free(request);
}

static void gtk_canvas_paste(t_canvas *canvas, int into_text) {
    GdkDisplay *display = gdk_display_get_default();
    t_gtk_canvas_paste *request;
    if (!display || !canvas) {
        return;
    }
    request = g_new(t_gtk_canvas_paste, 1);
    request->canvas = canvas;
    request->into_text = into_text;
    gdk_clipboard_read_text_async(gdk_display_get_clipboard(display), NULL, gtk_canvas_paste_ready,
                                  request);
}

static void gtk_console_select_all(void) {
    GtkTextIter first, last;
    if (gtk_console_buffer) {
        gtk_text_buffer_get_start_iter(gtk_console_buffer, &first);
        gtk_text_buffer_get_end_iter(gtk_console_buffer, &last);
        gtk_text_buffer_select_range(gtk_console_buffer, &first, &last);
    }
}

static void gtk_preferences_strings_set(GPtrArray **destination, int count,
                                        const char *const *strings) {
    int i;
    if (*destination) {
        g_ptr_array_unref(*destination);
    }
    *destination = g_ptr_array_new_with_free_func(g_free);
    for (i = 0; i < count; ++i) {
        g_ptr_array_add(*destination, g_strdup(strings[i] ? strings[i] : ""));
    }
}

static void gtk_preferences_floats_set(t_float *destination, int capacity,
                                       int count, const t_float *values) {
    int i;
    for (i = 0; i < capacity; ++i) {
        destination[i] = (i < count && values) ? values[i] : 0;
    }
}

static GtkWidget *gtk_preferences_page(void) {
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(page, 18);
    gtk_widget_set_margin_end(page, 18);
    gtk_widget_set_margin_top(page, 18);
    gtk_widget_set_margin_bottom(page, 18);
    return (page);
}

static GtkWidget *gtk_preferences_row(GtkWidget *box, const char *label,
                                      GtkWidget *control) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_set_hexpand(control, TRUE);
    gtk_box_append(GTK_BOX(row), title);
    gtk_box_append(GTK_BOX(row), control);
    gtk_box_append(GTK_BOX(box), row);
    return (row);
}

static GtkWidget *gtk_preferences_text_view(GPtrArray *strings) {
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *view = gtk_text_view_new();
    GString *text = g_string_new(NULL);
    guint i;
    if (strings) {
        for (i = 0; i < strings->len; ++i) {
            if (i) {
                g_string_append_c(text, '\n');
            }
            g_string_append(text, g_ptr_array_index(strings, i));
        }
    }
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)),
                             text->str, text->len);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_size_request(scroll, 420, 180);
    g_string_free(text, TRUE);
    return (scroll);
}

static GtkWidget *gtk_preferences_dropdown(GPtrArray *strings, int add_none,
                                           int selected) {
    const char **items;
    guint count = strings ? strings->len : 0;
    guint i;
    items = g_new0(const char *, count + add_none + 1);
    if (add_none) {
        items[0] = "None";
    }
    for (i = 0; i < count; ++i) {
        items[i + add_none] = g_ptr_array_index(strings, i);
    }
    {
        GtkWidget *dropdown = gtk_drop_down_new_from_strings(items);
        guint position = selected < 0 ? 0 : (guint)(selected + add_none);
        if (position >= count + add_none) {
            position = 0;
        }
        gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), position);
        g_free(items);
        return (dropdown);
    }
}

static GtkWidget *gtk_preferences_spin(double value, double minimum,
                                       double maximum) {
    GtkWidget *spin = gtk_spin_button_new_with_range(minimum, maximum, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), value);
    return (spin);
}

static const char *gtk_preferences_value(const char *value) {
    return (value && value[0] == '!' ? value + 1 : (value ? value : ""));
}

static int gtk_preferences_dropdown_value(GtkWidget *widget, int subtract) {
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(widget));
    if (selected == GTK_INVALID_LIST_POSITION) {
        return (subtract ? -1 : 0);
    }
    return ((int)selected - subtract);
}

static void gtk_preferences_send_lines(const char *selector, GtkWidget *view,
                                       int first_count, t_atom *first) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    GtkTextIter start, end;
    char *text;
    char **lines;
    t_atom *atoms;
    int count = first_count, i, nlines;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    lines = g_strsplit(text, "\n", -1);
    for (nlines = 0; lines[nlines]; ++nlines) {
        if (lines[nlines][0]) {
            ++count;
        }
    }
    atoms = g_new(t_atom, count);
    for (i = 0; i < first_count; ++i) {
        atoms[i] = first[i];
    }
    count = first_count;
    for (i = 0; lines[i]; ++i) {
        if (lines[i][0]) {
            SETSYMBOL(atoms + count++, gensym(lines[i]));
        }
    }
    gtk_pd_message(selector, count, atoms);
    g_free(atoms);
    g_strfreev(lines);
    g_free(text);
}

static void gtk_preferences_apply(void) {
    t_atom first[2], audio[20], midi[20], atom;
    int i;
    GtkWidget *paths_view = gtk_scrolled_window_get_child(
        GTK_SCROLLED_WINDOW(gtk_preferences.paths));
    GtkWidget *libraries_view = gtk_scrolled_window_get_child(
        GTK_SCROLLED_WINDOW(gtk_preferences.libraries));
    SETFLOAT(first, gtk_check_button_get_active(
        GTK_CHECK_BUTTON(gtk_preferences.use_standard_path)));
    SETFLOAT(first + 1, gtk_check_button_get_active(
        GTK_CHECK_BUTTON(gtk_preferences.verbose)));
    gtk_preferences_send_lines("path-dialog", paths_view, 2, first);

    SETFLOAT(first, gtk_check_button_get_active(
        GTK_CHECK_BUTTON(gtk_preferences.defeat_realtime)));
    SETSYMBOL(first + 1, gensym(gtk_editable_get_text(
        GTK_EDITABLE(gtk_preferences.flags))));
    gtk_preferences_send_lines("startup-dialog", libraries_view, 2, first);

    SETFLOAT(&atom, gtk_check_button_get_active(
        GTK_CHECK_BUTTON(gtk_preferences.zoom_open)));
    gtk_pd_message("zoom-open", 1, &atom);

    for (i = 0; i < GTK_PREF_AUDIO_DEVICES; ++i) {
        int device = gtk_preferences_dropdown_value(
            gtk_preferences.audio_input[i], 1);
        SETFLOAT(audio + i, device);
        SETFLOAT(audio + 4 + i, device >= 0 ? gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(gtk_preferences.audio_input_channels[i])) : 0);
        device = gtk_preferences_dropdown_value(
            gtk_preferences.audio_output[i], 1);
        SETFLOAT(audio + 8 + i, device);
        SETFLOAT(audio + 12 + i, device >= 0 ? gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(gtk_preferences.audio_output_channels[i])) : 0);
    }
    SETFLOAT(audio + 16, g_ascii_strtod(gtk_editable_get_text(
        GTK_EDITABLE(gtk_preferences.sample_rate)), NULL));
    SETFLOAT(audio + 17, gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(gtk_preferences.advance)));
    SETFLOAT(audio + 18, 0);
    SETFLOAT(audio + 19, g_ascii_strtod(gtk_editable_get_text(
        GTK_EDITABLE(gtk_preferences.block_size)), NULL));
    gtk_pd_message("audio-dialog", 20, audio);

    for (i = 0; i < GTK_PREF_MIDI_DEVICES; ++i) {
        SETFLOAT(midi + i, gtk_preferences_dropdown_value(
            gtk_preferences.midi_input[i], 0));
        SETFLOAT(midi + GTK_PREF_MIDI_DEVICES + i,
            gtk_preferences_dropdown_value(gtk_preferences.midi_output[i], 0));
    }
    SETFLOAT(midi + 18, 0);
    SETFLOAT(midi + 19, 0);
    gtk_pd_message("midi-dialog", 20, midi);
    gtk_pd_message("save-preferences", 0, NULL);
}

static void gtk_preferences_destroy(GtkWidget *widget, gpointer data) {
    memset(&gtk_preferences, 0, sizeof(gtk_preferences));
    (void)widget;
    (void)data;
}

static void gtk_preferences_button(GtkButton *button, gpointer data) {
    int response = GPOINTER_TO_INT(data);
    if (response != GTK_RESPONSE_CANCEL) {
        gtk_preferences_apply();
    }
    if (response != GTK_RESPONSE_APPLY && gtk_preferences.window) {
        gtk_window_destroy(GTK_WINDOW(gtk_preferences.window));
    }
    (void)button;
}

static GtkWidget *gtk_preferences_audio_page(void) {
    GtkWidget *page = gtk_preferences_page();
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *frame = gtk_frame_new("Devices");
    int i;
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Input"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Channels"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Output"), 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Channels"), 4, 0, 1, 1);
    for (i = 0; i < GTK_PREF_AUDIO_DEVICES; ++i) {
        GtkWidget *number = gtk_label_new(NULL);
        char label[16];
        int input = (int)gtk_preferences_data.audio_input_used[i];
        int output = (int)gtk_preferences_data.audio_output_used[i];
        pd_snprintf(label, sizeof(label), "%d", i + 1);
        gtk_label_set_text(GTK_LABEL(number), label);
        gtk_preferences.audio_input[i] = gtk_preferences_dropdown(
            gtk_preferences_data.audio_inputs, 1, input);
        gtk_preferences.audio_output[i] = gtk_preferences_dropdown(
            gtk_preferences_data.audio_outputs, 1, output);
        gtk_preferences.audio_input_channels[i] = gtk_preferences_spin(
            gtk_preferences_data.audio_input_channels[i], 1, 256);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(
            gtk_preferences.audio_input_channels[i]),
            gtk_preferences_data.audio_input_channels[i] > 0 ?
            gtk_preferences_data.audio_input_channels[i] : 2);
        gtk_preferences.audio_output_channels[i] = gtk_preferences_spin(
            gtk_preferences_data.audio_output_channels[i], 1, 256);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(
            gtk_preferences.audio_output_channels[i]),
            gtk_preferences_data.audio_output_channels[i] > 0 ?
            gtk_preferences_data.audio_output_channels[i] : 2);
        gtk_grid_attach(GTK_GRID(grid), number, 0, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_preferences.audio_input[i], 1, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_preferences.audio_input_channels[i], 2, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_preferences.audio_output[i], 3, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_preferences.audio_output_channels[i], 4, i + 1, 1, 1);
    }
    gtk_frame_set_child(GTK_FRAME(frame), grid);
    gtk_box_append(GTK_BOX(page), frame);
    gtk_preferences.sample_rate = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(gtk_preferences.sample_rate),
        gtk_preferences_value(gtk_preferences_data.sample_rate));
    gtk_widget_set_sensitive(gtk_preferences.sample_rate,
        !(gtk_preferences_data.sample_rate && gtk_preferences_data.sample_rate[0] == '!'));
    gtk_preferences_row(page, "Sample rate", gtk_preferences.sample_rate);
    gtk_preferences.block_size = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(gtk_preferences.block_size),
        gtk_preferences_value(gtk_preferences_data.block_size));
    gtk_widget_set_sensitive(gtk_preferences.block_size,
        !(gtk_preferences_data.block_size && gtk_preferences_data.block_size[0] == '!'));
    gtk_preferences_row(page, "Block size", gtk_preferences.block_size);
    gtk_preferences.advance = gtk_preferences_spin(gtk_preferences_data.advance, 0, 1000);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_preferences.advance),
                              gtk_preferences_data.advance);
    gtk_preferences_row(page, "Delay (ms)", gtk_preferences.advance);
    gtk_preferences.callback = gtk_check_button_new_with_label("Use callbacks");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(gtk_preferences.callback), FALSE);
    gtk_widget_set_sensitive(gtk_preferences.callback, FALSE);
    gtk_widget_set_tooltip_text(gtk_preferences.callback,
        "Unavailable with the in-process GTK backend");
    gtk_box_append(GTK_BOX(page), gtk_preferences.callback);
    return (page);
}

static GtkWidget *gtk_preferences_midi_page(void) {
    GtkWidget *page = gtk_preferences_page();
    GtkWidget *grid = gtk_grid_new();
    int i;
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Input"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Output"), 2, 0, 1, 1);
    for (i = 0; i < GTK_PREF_MIDI_DEVICES; ++i) {
        char label[16];
        GtkWidget *number;
        pd_snprintf(label, sizeof(label), "%d", i + 1);
        number = gtk_label_new(label);
        gtk_preferences.midi_input[i] = gtk_preferences_dropdown(
            gtk_preferences_data.midi_inputs, 0,
            (int)gtk_preferences_data.midi_input_used[i]);
        gtk_preferences.midi_output[i] = gtk_preferences_dropdown(
            gtk_preferences_data.midi_outputs, 0,
            (int)gtk_preferences_data.midi_output_used[i]);
        gtk_grid_attach(GTK_GRID(grid), number, 0, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_preferences.midi_input[i], 1, i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), gtk_preferences.midi_output[i], 2, i + 1, 1, 1);
    }
    gtk_box_append(GTK_BOX(page), grid);
    return (page);
}

static void gtk_preferences_open(void) {
    GtkWidget *box, *content, *sidebar, *stack, *page, *buttons, *button;
    if (gtk_preferences.window) {
        gtk_window_present(GTK_WINDOW(gtk_preferences.window));
        return;
    }
    gtk_preferences.window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(gtk_preferences.window), "Preferences");
    gtk_window_set_default_size(GTK_WINDOW(gtk_preferences.window), 720, 560);
    gtk_window_set_transient_for(GTK_WINDOW(gtk_preferences.window),
                                 gtk_menu_parent(NULL));
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(gtk_preferences.window), box);
    content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(content, TRUE);
    gtk_box_append(GTK_BOX(box), content);
    stack = gtk_stack_new();
    gtk_preferences.stack = stack;
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_hexpand(stack, TRUE);
    gtk_widget_set_vexpand(stack, TRUE);
    sidebar = gtk_stack_sidebar_new();
    gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sidebar), GTK_STACK(stack));
    gtk_widget_set_size_request(sidebar, 150, -1);
    gtk_box_append(GTK_BOX(content), sidebar);
    gtk_box_append(GTK_BOX(content), stack);

    page = gtk_preferences_page();
    gtk_preferences.use_standard_path = gtk_check_button_new_with_label("Use standard paths");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(gtk_preferences.use_standard_path),
                                gtk_preferences_data.use_standard_path);
    gtk_box_append(GTK_BOX(page), gtk_preferences.use_standard_path);
    gtk_preferences.verbose = gtk_check_button_new_with_label("Verbose startup");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(gtk_preferences.verbose),
                                gtk_preferences_data.verbose);
    gtk_box_append(GTK_BOX(page), gtk_preferences.verbose);
    gtk_preferences.defeat_realtime = gtk_check_button_new_with_label("Defeat real-time scheduling");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(gtk_preferences.defeat_realtime),
                                gtk_preferences_data.defeat_realtime);
    gtk_box_append(GTK_BOX(page), gtk_preferences.defeat_realtime);
    gtk_preferences.zoom_open = gtk_check_button_new_with_label("Zoom new patch windows");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(gtk_preferences.zoom_open),
                                gtk_preferences_data.zoom_open);
    gtk_box_append(GTK_BOX(page), gtk_preferences.zoom_open);
    gtk_stack_add_titled(GTK_STACK(stack), page, "general", "General");

    page = gtk_preferences_page();
    gtk_box_append(GTK_BOX(page), gtk_label_new("One search path per line"));
    gtk_preferences.paths = gtk_preferences_text_view(gtk_preferences_data.paths);
    gtk_box_append(GTK_BOX(page), gtk_preferences.paths);
    gtk_stack_add_titled(GTK_STACK(stack), page, "paths", "Paths");

    page = gtk_preferences_page();
    gtk_preferences.flags = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(gtk_preferences.flags),
        gtk_preferences_data.flags ? gtk_preferences_data.flags : "");
    gtk_preferences_row(page, "Startup flags", gtk_preferences.flags);
    gtk_box_append(GTK_BOX(page), gtk_label_new("One startup library per line"));
    gtk_preferences.libraries = gtk_preferences_text_view(gtk_preferences_data.libraries);
    gtk_box_append(GTK_BOX(page), gtk_preferences.libraries);
    gtk_stack_add_titled(GTK_STACK(stack), page, "startup", "Startup");
    gtk_stack_add_titled(GTK_STACK(stack), gtk_preferences_audio_page(), "audio", "Audio");
    gtk_stack_add_titled(GTK_STACK(stack), gtk_preferences_midi_page(), "midi", "MIDI");

    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    gtk_widget_set_margin_start(buttons, 12);
    gtk_widget_set_margin_end(buttons, 12);
    gtk_widget_set_margin_top(buttons, 10);
    gtk_widget_set_margin_bottom(buttons, 10);
    button = gtk_button_new_with_label("Cancel");
    g_signal_connect(button, "clicked", G_CALLBACK(gtk_preferences_button),
                     GINT_TO_POINTER(GTK_RESPONSE_CANCEL));
    gtk_box_append(GTK_BOX(buttons), button);
    button = gtk_button_new_with_label("Apply");
    g_signal_connect(button, "clicked", G_CALLBACK(gtk_preferences_button),
                     GINT_TO_POINTER(GTK_RESPONSE_APPLY));
    gtk_box_append(GTK_BOX(buttons), button);
    button = gtk_button_new_with_label("OK");
    gtk_widget_add_css_class(button, "suggested-action");
    g_signal_connect(button, "clicked", G_CALLBACK(gtk_preferences_button),
                     GINT_TO_POINTER(GTK_RESPONSE_OK));
    gtk_box_append(GTK_BOX(buttons), button);
    gtk_box_append(GTK_BOX(box), buttons);
    g_signal_connect(gtk_preferences.window, "destroy",
                     G_CALLBACK(gtk_preferences_destroy), NULL);
    gtk_window_present(GTK_WINDOW(gtk_preferences.window));
}

static void gtk_preferences_show_page(const char *name) {
    gtk_preferences_open();
    if (gtk_preferences.stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(gtk_preferences.stack), name);
    }
}

static void gtk_canvas_save_to(t_canvas *canvas, const char *path, int destroy_after_save) {
    char *filename, *directory;
    t_atom atoms[3];
    char *with_extension = NULL;
    const char *dot = strrchr(path, '.');
    if (!dot || (g_ascii_strcasecmp(dot, ".pd") && g_ascii_strcasecmp(dot, ".pat") &&
                 g_ascii_strcasecmp(dot, ".mxt"))) {
        with_extension = g_strconcat(path, ".pd", NULL);
        path = with_extension;
    }
    filename = g_path_get_basename(path);
    directory = g_path_get_dirname(path);
    SETSYMBOL(atoms, gensym(filename));
    SETSYMBOL(atoms + 1, gensym(directory));
    SETFLOAT(atoms + 2, destroy_after_save);
    pd_typedmess(&canvas->gl_pd, gensym("savetofile"), 3, atoms);
    gtk_recent_add(path);
    g_free(directory);
    g_free(filename);
    g_free(with_extension);
}

static void gtk_choose_response(GtkNativeDialog *dialog, int response, gpointer data) {
    t_gtk_choose *choice = (t_gtk_choose *)data;
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    if (response == GTK_RESPONSE_ACCEPT) {
        if (choice->action == GTK_CHOOSE_OPEN) {
            GListModel *files = gtk_file_chooser_get_files(chooser);
            unsigned int i, count = g_list_model_get_n_items(files);
            for (i = 0; i < count; i++) {
                GFile *file = G_FILE(g_list_model_get_item(files, i));
                char *path = g_file_get_path(file);
                if (path) {
                    gtk_open_path(path, 0);
                    gtk_recent_add(path);
                    g_free(path);
                }
                g_object_unref(file);
            }
            g_object_unref(files);
        } else {
            GFile *file = gtk_file_chooser_get_file(chooser);
            char *path = file ? g_file_get_path(file) : NULL;
            if (path) {
                if (choice->action == GTK_CHOOSE_SAVE_CANVAS && choice->canvas &&
                    gc_find(choice->canvas)) {
                    gtk_canvas_save_to(choice->canvas, path, choice->destroy_after_save);
                } else if (choice->action == GTK_CHOOSE_SAVE_LOG && gtk_console_buffer) {
                    GtkTextIter first, last;
                    char *text;
                    gtk_text_buffer_get_start_iter(gtk_console_buffer, &first);
                    gtk_text_buffer_get_end_iter(gtk_console_buffer, &last);
                    text = gtk_text_buffer_get_text(gtk_console_buffer, &first, &last, FALSE);
                    g_file_set_contents(path, text, -1, NULL);
                    g_free(text);
                } else if (choice->action == GTK_CHOOSE_PRINT && choice->canvas &&
                           gc_find(choice->canvas)) {
                    t_atom atom;
                    SETSYMBOL(&atom, gensym(path));
                    pd_typedmess(&choice->canvas->gl_pd, gensym("print"), 1, &atom);
                } else if (choice->action == GTK_CHOOSE_PREF_SAVE) {
                    t_atom atom;
                    SETSYMBOL(&atom, gensym(path));
                    gtk_pd_message("save-preferences", 1, &atom);
                } else if (choice->action == GTK_CHOOSE_PREF_LOAD) {
                    t_atom atom;
                    SETSYMBOL(&atom, gensym(path));
                    gtk_pd_message("load-preferences", 1, &atom);
                }
                g_free(path);
            }
            if (file) {
                g_object_unref(file);
            }
        }
    }
    g_object_unref(dialog);
    g_free(choice);
}

static void gtk_choose_file(t_gc *c, t_gtk_choose_action action, t_canvas *canvas, const char *name,
                            const char *directory, int destroy_after_save) {
    GtkFileChooserAction chooser_action =
        action == GTK_CHOOSE_OPEN || action == GTK_CHOOSE_PREF_LOAD ? GTK_FILE_CHOOSER_ACTION_OPEN
                                                                    : GTK_FILE_CHOOSER_ACTION_SAVE;
    const char *title = chooser_action == GTK_FILE_CHOOSER_ACTION_OPEN
                            ? "Open"
                            : (action == GTK_CHOOSE_PRINT ? "Print..." : "Save As...");
    GtkFileChooserNative *dialog = gtk_file_chooser_native_new(
        title, gtk_menu_parent(c), chooser_action,
        chooser_action == GTK_FILE_CHOOSER_ACTION_OPEN ? "_Open" : "_Save", "_Cancel");
    GtkFileFilter *filter = gtk_file_filter_new();
    t_gtk_choose *choice = g_new0(t_gtk_choose, 1);
    choice->action = action;
    choice->canvas = canvas;
    choice->destroy_after_save = destroy_after_save;
    if (action == GTK_CHOOSE_OPEN || action == GTK_CHOOSE_SAVE_CANVAS) {
        gtk_file_filter_set_name(filter, "Pure Data patches");
        gtk_file_filter_add_pattern(filter, "*.pd");
        gtk_file_filter_add_pattern(filter, "*.pat");
        gtk_file_filter_add_pattern(filter, "*.mxt");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    } else if (action == GTK_CHOOSE_PRINT) {
        gtk_file_filter_set_name(filter, "PostScript");
        gtk_file_filter_add_pattern(filter, "*.ps");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    } else if (action == GTK_CHOOSE_PREF_SAVE || action == GTK_CHOOSE_PREF_LOAD) {
        gtk_file_filter_set_name(filter, "Pure Data settings");
        gtk_file_filter_add_pattern(filter, "*.pdsettings");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    } else {
        g_object_unref(filter);
    }
    if (action == GTK_CHOOSE_OPEN) {
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    }
    if (directory && *directory) {
        GFile *folder = g_file_new_for_path(directory);
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), folder, NULL);
        g_object_unref(folder);
    }
    if (name && *name && chooser_action == GTK_FILE_CHOOSER_ACTION_SAVE) {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), name);
    }
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_choose_response), choice);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}

static void gtk_find_response(GtkDialog *dialog, int response, gpointer data) {
    t_canvas *canvas = (t_canvas *)data;
    if (response == GTK_RESPONSE_ACCEPT && gc_find(canvas)) {
        GtkWidget *entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "entry"));
        GtkWidget *whole = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "whole"));
        const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
        if (*text) {
            t_atom atoms[2];
            SETSYMBOL(atoms, gensym(text));
            SETFLOAT(atoms + 1, gtk_check_button_get_active(GTK_CHECK_BUTTON(whole)));
            pd_typedmess(&canvas->gl_pd, gensym("find"), 2, atoms);
        }
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void gtk_find_dialog(t_gc *c) {
    GtkWidget *dialog, *box, *entry, *whole;
    if (!c) {
        return;
    }
    dialog = gtk_dialog_new_with_buttons("Find", GTK_WINDOW(c->window), GTK_DIALOG_MODAL, "_Cancel",
                                         GTK_RESPONSE_CANCEL, "_Find", GTK_RESPONSE_ACCEPT, NULL);
    box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Find text");
    whole = gtk_check_button_new_with_label("Whole word");
    gtk_box_append(GTK_BOX(box), entry);
    gtk_box_append(GTK_BOX(box), whole);
    g_object_set_data(G_OBJECT(dialog), "entry", entry);
    g_object_set_data(G_OBJECT(dialog), "whole", whole);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_find_response), c->canvas);
    gtk_window_present(GTK_WINDOW(dialog));
    gtk_widget_grab_focus(entry);
}

static void gtk_send_message_text(const char *text) {
    t_binbuf *binbuf = binbuf_new();
    int argc;
    t_atom *argv;
    binbuf_text(binbuf, text, strlen(text));
    argc = binbuf_getnatom(binbuf);
    argv = binbuf_getvec(binbuf);
    if (argc >= 2 && argv[0].a_type == A_SYMBOL && argv[1].a_type == A_SYMBOL) {
        t_pd *receiver = atom_getsymbol(argv)->s_thing;
        if (receiver) {
            pd_typedmess(receiver, atom_getsymbol(argv + 1), argc - 2, argv + 2);
        } else {
            pd_error(0, "%s: no such receiver", atom_getsymbol(argv)->s_name);
        }
    }
    binbuf_free(binbuf);
}

static void gtk_message_response(GtkDialog *dialog, int response, gpointer data) {
    if (response == GTK_RESPONSE_ACCEPT) {
        GtkWidget *entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "entry"));
        const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
        if (*text) {
            gtk_send_message_text(text);
        }
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
    (void)data;
}

static void gtk_message_dialog(t_gc *c) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Send a Pd message", gtk_menu_parent(c),
                                                    GTK_DIALOG_MODAL, "_Close", GTK_RESPONSE_CANCEL,
                                                    "_Send", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "pd dsp 1");
    gtk_box_append(GTK_BOX(box), entry);
    g_object_set_data(G_OBJECT(dialog), "entry", entry);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_message_response), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
    gtk_widget_grab_focus(entry);
}

static void gtk_present_window(uint64_t id) {
    if (!id) {
        if (gtk_main_window) {
            gtk_window_present(GTK_WINDOW(gtk_main_window));
        }
    } else {
        t_gc *c = gc_find((t_canvas *)(uintptr_t)id);
        if (c) {
            gtk_window_present(GTK_WINDOW(c->window));
        }
    }
}

static void gtk_cycle_window(t_gc *current, int previous) {
    t_gc *c;
    if (!previous) {
        if (!current) {
            if (gc_list) {
                gtk_window_present(GTK_WINDOW(gc_list->window));
            }
        } else if (current->next) {
            gtk_window_present(GTK_WINDOW(current->next->window));
        } else if (gtk_main_window) {
            gtk_window_present(GTK_WINDOW(gtk_main_window));
        }
    } else if (!current) {
        for (c = gc_list; c && c->next; c = c->next)
            ;
        if (c) {
            gtk_window_present(GTK_WINDOW(c->window));
        }
    } else if (current == gc_list) {
        if (gtk_main_window) {
            gtk_window_present(GTK_WINDOW(gtk_main_window));
        }
    } else {
        for (c = gc_list; c && c->next != current; c = c->next)
            ;
        if (c) {
            gtk_window_present(GTK_WINDOW(c->window));
        }
    }
}

static void gtk_new_patch(void) {
    char name[80];
    char *directory = g_get_current_dir();
    t_atom atoms[2];
    pd_snprintf(name, sizeof(name), "Untitled-%d", gtk_untitled_number++);
    SETSYMBOL(atoms, gensym(name));
    SETSYMBOL(atoms + 1, gensym(directory));
    gtk_pd_message("menunew", 2, atoms);
    g_free(directory);
}

static void gtk_menu_command(GSimpleAction *action, GVariant *parameter, gpointer data) {
    t_gc *c = (t_gc *)data;
    const char *command = g_variant_get_string(parameter, NULL);
    if (!strcmp(command, "new")) {
        gtk_new_patch();
    } else if (!strcmp(command, "open")) {
        char *directory = g_get_current_dir();
        gtk_choose_file(c, GTK_CHOOSE_OPEN, NULL, NULL, directory, 0);
        g_free(directory);
    } else if (!strcmp(command, "close")) {
        if (c) {
            canvas_message_float(c, "menuclose", 0);
        } else if (gtk_main_window) {
            gtk_widget_set_visible(gtk_main_window, FALSE);
        }
    } else if (!strcmp(command, "save") && c) {
        canvas_message_float(c, "menusave", 0);
    } else if (!strcmp(command, "save-as")) {
        if (c) {
            canvas_message_float(c, "menusaveas", 0);
        } else {
            gtk_choose_file(NULL, GTK_CHOOSE_SAVE_LOG, NULL, "pd-console.txt", NULL, 0);
        }
    } else if (!strcmp(command, "print") && c) {
        gtk_choose_file(c, GTK_CHOOSE_PRINT, c->canvas, "patch.ps", NULL, 0);
    } else if (!strcmp(command, "quit")) {
        gtk_pd_message("verifyquit", 0, NULL);
    } else if (!strcmp(command, "preferences")) {
        gtk_pd_message("start-preference-dialog", 0, NULL);
    } else if (!strcmp(command, "preferences-save")) {
        gtk_pd_message("save-preferences", 0, NULL);
    } else if (!strcmp(command, "preferences-save-as")) {
        gtk_choose_file(c, GTK_CHOOSE_PREF_SAVE, NULL, "Untitled.pdsettings", NULL, 0);
    } else if (!strcmp(command, "preferences-load")) {
        gtk_choose_file(c, GTK_CHOOSE_PREF_LOAD, NULL, NULL, NULL, 0);
    } else if (!strcmp(command, "preferences-forget")) {
        gtk_pd_message("forget-preferences", 0, NULL);
    } else if (!strcmp(command, "copy") && !c) {
        gtk_console_copy();
    } else if (!strcmp(command, "selectall") && !c) {
        gtk_console_select_all();
    } else if (!strcmp(command, "clear-console")) {
        if (gtk_console_buffer) {
            gtk_text_buffer_set_text(gtk_console_buffer, "", 0);
        }
    } else if (!strcmp(command, "find")) {
        gtk_find_dialog(c);
    } else if (!strcmp(command, "find-error")) {
        gtk_pd_message("finderror", 0, NULL);
    } else if (!strcmp(command, "test-audio")) {
        gtk_open_document("doc/7.stuff/tools", "testtone.pd");
    } else if (!strcmp(command, "audio-settings")) {
        gtk_pd_message("audio-properties", 0, NULL);
    } else if (!strcmp(command, "midi-settings")) {
        gtk_pd_message("midi-properties", 0, NULL);
    } else if (!strcmp(command, "minimize")) {
        gtk_window_minimize(gtk_menu_parent(c));
    } else if (!strcmp(command, "next-window")) {
        gtk_cycle_window(c, 0);
    } else if (!strcmp(command, "previous-window")) {
        gtk_cycle_window(c, 1);
    } else if (!strcmp(command, "close-subwindows")) {
        gtk_pd_message("close-subwindows", 0, NULL);
    } else if (!strcmp(command, "pd-window")) {
        gtk_present_window(0);
    } else if (!strcmp(command, "load-meter")) {
        gtk_open_document("doc/7.stuff/tools", "load-meter.pd");
    } else if (!strcmp(command, "message")) {
        gtk_message_dialog(c);
    } else if (!strcmp(command, "about")) {
        gtk_show_about_dialog(gtk_menu_parent(c), "program-name", "Pure Data", "version",
                              PDGUI_VERSION_STRING, "comments",
                              "A real-time graphical programming environment", "website",
                              "https://puredata.info", NULL);
    } else if (!strcmp(command, "manual")) {
        gtk_open_local_document(c, "doc/1.manual", "index.htm");
    } else if (!strcmp(command, "browser") || !strcmp(command, "object-list")) {
        gtk_pd_message("help-intro", 0, NULL);
    } else if (!strcmp(command, "puredata-info")) {
        gtk_open_uri(c, "https://puredata.info");
    } else if (!strcmp(command, "updates")) {
        gtk_open_uri(c, "https://pdlatest.puredata.info");
    } else if (!strcmp(command, "bug")) {
        gtk_open_uri(c, "https://bugs.puredata.info");
    } else if (c) {
        if (!strcmp(command, "zoom-in")) {
            canvas_message_float(c, "zoom", 2);
        } else if (!strcmp(command, "zoom-out")) {
            canvas_message_float(c, "zoom", 1);
        } else {
            canvas_message(c, command);
        }
    }
    (void)action;
}

static void gtk_dsp_action(GSimpleAction *action, GVariant *parameter, gpointer data) {
    t_atom atom;
    gtk_dsp_state = g_variant_get_boolean(parameter);
    g_simple_action_set_state(action, g_variant_new_boolean(gtk_dsp_state));
    SETFLOAT(&atom, gtk_dsp_state);
    gtk_pd_message("dsp", 1, &atom);
    (void)data;
}

static void gtk_api_action(GSimpleAction *action, GVariant *parameter, gpointer data) {
    const char *name = g_action_get_name(G_ACTION(action));
    int api = g_variant_get_int32(parameter);
    t_atom atom;
    g_simple_action_set_state(action, g_variant_new_int32(api));
    SETFLOAT(&atom, api);
    if (!strcmp(name, "audio-api")) {
        gtk_audio_api = api;
        gtk_pd_message("audio-setapi", 1, &atom);
    } else {
        gtk_pd_message("midi-setapi", 1, &atom);
    }
    (void)data;
}

static void gtk_edit_action(GSimpleAction *action, GVariant *parameter, gpointer data) {
    t_gc *c = (t_gc *)data;
    int state;
    GVariant *oldstate;
    if (!c) {
        return;
    }
    oldstate = g_action_get_state(G_ACTION(action));
    state = !g_variant_get_boolean(oldstate);
    g_variant_unref(oldstate);
    g_simple_action_set_state(action, g_variant_new_boolean(state));
    canvas_message_float(c, "editmode", state);
    (void)parameter;
}

static void gtk_toggle_action(GSimpleAction *action, GVariant *parameter, gpointer data) {
    GVariant *oldstate = g_action_get_state(G_ACTION(action));
    int state = !g_variant_get_boolean(oldstate);
    g_variant_unref(oldstate);
    g_simple_action_set_state(action, g_variant_new_boolean(state));
    (void)parameter;
    (void)data;
}

static void gtk_recent_action(GSimpleAction *action, GVariant *parameter, gpointer data) {
    const char *path = g_variant_get_string(parameter, NULL);
    gtk_open_path(path, 0);
    gtk_recent_add(path);
    (void)action;
    (void)data;
}

static void gtk_recent_clear_action(GSimpleAction *action, GVariant *parameter, gpointer data) {
    if (gtk_recent_files) {
        g_ptr_array_set_size(gtk_recent_files, 0);
    }
    gtk_update_recent_menu();
    (void)action;
    (void)parameter;
    (void)data;
}

static void gtk_raise_action(GSimpleAction *action, GVariant *parameter, gpointer data) {
    gtk_present_window(g_variant_get_uint64(parameter));
    (void)action;
    (void)data;
}

static void gtk_action_group_add(GtkWidget *window, t_gc *c) {
    GSimpleActionGroup *group = g_simple_action_group_new();
    GSimpleAction *action;
    action = g_simple_action_new("menu", G_VARIANT_TYPE_STRING);
    g_signal_connect(action, "activate", G_CALLBACK(gtk_menu_command), c);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(action);
    action = g_simple_action_new_stateful("dsp", G_VARIANT_TYPE_BOOLEAN,
                                          g_variant_new_boolean(gtk_dsp_state));
    g_signal_connect(action, "activate", G_CALLBACK(gtk_dsp_action), c);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(action);
    action = g_simple_action_new_stateful("audio-api", G_VARIANT_TYPE_INT32,
                                          g_variant_new_int32(gtk_audio_api));
    g_signal_connect(action, "activate", G_CALLBACK(gtk_api_action), c);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(action);
    action = g_simple_action_new_stateful("midi-api", G_VARIANT_TYPE_INT32,
                                          g_variant_new_int32(sys_midiapi));
    g_signal_connect(action, "activate", G_CALLBACK(gtk_api_action), c);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(action);
    action = g_simple_action_new_stateful("edit-mode", NULL,
                                          g_variant_new_boolean(c ? c->editmode : FALSE));
    g_signal_connect(action, "activate", G_CALLBACK(gtk_edit_action), c);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(action);
    action = g_simple_action_new("recent", G_VARIANT_TYPE_STRING);
    g_signal_connect(action, "activate", G_CALLBACK(gtk_recent_action), c);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(action);
    action = g_simple_action_new("recent-clear", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(gtk_recent_clear_action), c);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(action);
    action = g_simple_action_new("raise", G_VARIANT_TYPE_UINT64);
    g_signal_connect(action, "activate", G_CALLBACK(gtk_raise_action), c);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(action);
    action = g_simple_action_new_stateful("tabbed-preferences", NULL, g_variant_new_boolean(FALSE));
    g_signal_connect(action, "activate", G_CALLBACK(gtk_toggle_action), c);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(action);
    action = g_simple_action_new("disabled", NULL);
    g_simple_action_set_enabled(action, FALSE);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
    g_object_unref(action);
    gtk_widget_insert_action_group(window, "pd", G_ACTION_GROUP(group));
    g_object_set_data_full(G_OBJECT(window), "pd-actions", g_object_ref(group), g_object_unref);
    g_object_unref(group);
}

static void gtk_set_boolean_action(GtkWidget *window, const char *name, int state) {
    GActionMap *map = G_ACTION_MAP(g_object_get_data(G_OBJECT(window), "pd-actions"));
    GAction *action = map ? g_action_map_lookup_action(map, name) : NULL;
    if (action) {
        g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(state != 0));
    }
}

static void gtk_set_integer_action(GtkWidget *window, const char *name, int state) {
    GActionMap *map = G_ACTION_MAP(g_object_get_data(G_OBJECT(window), "pd-actions"));
    GAction *action = map ? g_action_map_lookup_action(map, name) : NULL;
    if (action) {
        g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_int32(state));
    }
}

static void gtk_set_boolean_action_all(const char *name, int state) {
    t_gc *c;
    if (gtk_main_window) {
        gtk_set_boolean_action(gtk_main_window, name, state);
    }
    for (c = gc_list; c; c = c->next) {
        gtk_set_boolean_action(c->window, name, state);
    }
}

static void gtk_set_integer_action_all(const char *name, int state) {
    t_gc *c;
    if (gtk_main_window) {
        gtk_set_integer_action(gtk_main_window, name, state);
    }
    for (c = gc_list; c; c = c->next) {
        gtk_set_integer_action(c->window, name, state);
    }
}

static void gtk_menu_add(GMenu *menu, const char *label, const char *command) {
    GMenuItem *item = g_menu_item_new(label, NULL);
    g_menu_item_set_action_and_target(item, "pd.menu", "s", command);
    g_menu_append_item(menu, item);
    g_object_unref(item);
}

static void gtk_menu_add_target(GMenu *menu, const char *label, const char *action,
                                const char *format, ...) {
    va_list args;
    GMenuItem *item = g_menu_item_new(label, NULL);
    va_start(args, format);
    g_menu_item_set_action_and_target_value(item, action, g_variant_new_va(format, NULL, &args));
    va_end(args);
    g_menu_append_item(menu, item);
    g_object_unref(item);
}

static void gtk_menu_append_section(GMenu *menu, GMenu *section) {
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
}

static void gtk_update_recent_menu(void) {
    unsigned int i;
    if (!gtk_recent_menu) {
        return;
    }
    g_menu_remove_all(gtk_recent_menu);
    if (gtk_recent_files) {
        for (i = 0; i < gtk_recent_files->len; i++) {
            const char *path = (const char *)g_ptr_array_index(gtk_recent_files, i);
            char *basename = g_path_get_basename(path);
            char *label = g_strdup_printf("%u. %s", i + 1, basename);
            gtk_menu_add_target(gtk_recent_menu, label, "pd.recent", "s", path);
            g_free(label);
            g_free(basename);
        }
    }
    {
        GMenu *section = g_menu_new();
        GMenuItem *clear = g_menu_item_new("Clear Menu", "pd.recent-clear");
        g_menu_append_item(section, clear);
        g_object_unref(clear);
        gtk_menu_append_section(gtk_recent_menu, section);
    }
}

static void gtk_update_window_menu(void) {
    t_gc *c;
    if (!gtk_window_menu) {
        return;
    }
    g_menu_remove_all(gtk_window_menu);
    for (c = gc_list; c; c = c->next) {
        gtk_menu_add_target(gtk_window_menu, c->title ? c->title : c->canvas->gl_name->s_name,
                            "pd.raise", "t", (uint64_t)(uintptr_t)c->canvas);
    }
}

static GMenu *gtk_build_file_menu(int patch_window) {
    GMenu *menu = g_menu_new(), *section = g_menu_new(), *preferences;
    gtk_menu_add(section, "New", "new");
    gtk_menu_add(section, "Open", "open");
    if (!gtk_recent_menu) {
        gtk_recent_menu = g_menu_new();
    }
    g_menu_append_submenu(section, "Open Recent", G_MENU_MODEL(gtk_recent_menu));
    gtk_menu_append_section(menu, section);
    section = g_menu_new();
    gtk_menu_add(section, "Close", "close");
    if (patch_window) {
        gtk_menu_add(section, "Save", "save");
    }
    gtk_menu_add(section, "Save As...", "save-as");
    gtk_menu_append_section(menu, section);
    preferences = g_menu_new();
    gtk_menu_add(preferences, "Edit Preferences...", "preferences");
    gtk_menu_add(preferences, "Save All Preferences", "preferences-save");
    gtk_menu_add(preferences, "Save to...", "preferences-save-as");
    gtk_menu_add(preferences, "Load from...", "preferences-load");
    gtk_menu_add(preferences, "Forget All...", "preferences-forget");
    {
        GMenuItem *tabbed = g_menu_item_new("Tabbed preferences", "pd.tabbed-preferences");
        g_menu_append_item(preferences, tabbed);
        g_object_unref(tabbed);
    }
    section = g_menu_new();
    g_menu_append_submenu(section, "Preferences", G_MENU_MODEL(preferences));
    g_object_unref(preferences);
    if (patch_window) {
        gtk_menu_add(section, "Print...", "print");
    }
    gtk_menu_append_section(menu, section);
    section = g_menu_new();
    gtk_menu_add(section, "Quit", "quit");
    gtk_menu_append_section(menu, section);
    gtk_update_recent_menu();
    return (menu);
}

static GMenu *gtk_build_edit_menu(int patch_window) {
    GMenu *menu = g_menu_new(), *section = g_menu_new();
    if (patch_window) {
        gtk_menu_add(section, "Undo", "undo");
        gtk_menu_add(section, "Redo", "redo");
        gtk_menu_append_section(menu, section);
        section = g_menu_new();
        gtk_menu_add(section, "Cut", "cut");
        gtk_menu_add(section, "Copy", "copy");
        gtk_menu_add(section, "Paste", "paste");
        gtk_menu_add(section, "Paste Replace", "paste-replace");
        gtk_menu_append_section(menu, section);
        section = g_menu_new();
        gtk_menu_add(section, "Select All", "selectall");
        gtk_menu_append_section(menu, section);
        section = g_menu_new();
        gtk_menu_add(section, "Duplicate", "duplicate");
        gtk_menu_add(section, "Tidy Up", "tidy");
        gtk_menu_add(section, "(Dis)Connect Selection", "connect_selection");
        gtk_menu_add(section, "Triggerize", "triggerize");
        gtk_menu_append_section(menu, section);
        section = g_menu_new();
        gtk_menu_add(section, "Font", "menufont");
        gtk_menu_add(section, "Zoom In", "zoom-in");
        gtk_menu_add(section, "Zoom Out", "zoom-out");
        gtk_menu_append_section(menu, section);
    } else {
        {
            GMenuItem *cut = g_menu_item_new("Cut", "pd.disabled");
            g_menu_append_item(section, cut);
            g_object_unref(cut);
        }
        gtk_menu_add(section, "Copy", "copy");
        {
            GMenuItem *paste = g_menu_item_new("Paste", "pd.disabled");
            g_menu_append_item(section, paste);
            g_object_unref(paste);
        }
        gtk_menu_append_section(menu, section);
        section = g_menu_new();
        gtk_menu_add(section, "Select All", "selectall");
        gtk_menu_append_section(menu, section);
        section = g_menu_new();
        gtk_menu_add(section, "Font", "font-console");
        gtk_menu_append_section(menu, section);
    }
    section = g_menu_new();
    gtk_menu_add(section, "Clear Console", "clear-console");
    if (patch_window) {
        GMenuItem *edit = g_menu_item_new("Edit Mode", "pd.edit-mode");
        g_menu_append_item(section, edit);
        g_object_unref(edit);
    }
    gtk_menu_append_section(menu, section);
    return (menu);
}

static GMenu *gtk_build_find_menu(int patch_window) {
    GMenu *menu = g_menu_new();
    gtk_menu_add(menu, "Find...", "find");
    if (patch_window) {
        gtk_menu_add(menu, "Find Again", "findagain");
    }
    gtk_menu_add(menu, "Find Last Error", "find-error");
    return (menu);
}

static GMenu *gtk_build_media_menu(void) {
    GMenu *menu = g_menu_new(), *section = g_menu_new();
    const char *names[16];
    int ids[16], i, count;
    t_audiosettings settings;
    sys_get_audio_settings(&settings);
    gtk_audio_api = settings.a_api;
    gtk_menu_add_target(section, "DSP On", "pd.dsp", "b", TRUE);
    gtk_menu_add_target(section, "DSP Off", "pd.dsp", "b", FALSE);
    gtk_menu_append_section(menu, section);
    section = g_menu_new();
    gtk_menu_add(section, "Test Audio and MIDI...", "test-audio");
    gtk_menu_append_section(menu, section);
    count = sys_get_audio_apis(16, names, ids);
    if (count > 0) {
        section = g_menu_new();
        for (i = 0; i < count; i++) {
            gtk_menu_add_target(section, names[i], "pd.audio-api", "i", ids[i]);
        }
        gtk_menu_append_section(menu, section);
    }
    count = sys_get_midi_apis(16, names, ids);
    if (count > 0) {
        section = g_menu_new();
        for (i = 0; i < count; i++) {
            gtk_menu_add_target(section, names[i], "pd.midi-api", "i", ids[i]);
        }
        gtk_menu_append_section(menu, section);
    }
    section = g_menu_new();
    gtk_menu_add(section, "Audio Settings...", "audio-settings");
    gtk_menu_add(section, "MIDI Settings...", "midi-settings");
    gtk_menu_append_section(menu, section);
    return (menu);
}

static GMenu *gtk_build_window_menu(int patch_window) {
    GMenu *menu = g_menu_new(), *section = g_menu_new();
    gtk_menu_add(section, "Minimize", "minimize");
    gtk_menu_add(section, "Next Window", "next-window");
    gtk_menu_add(section, "Previous Window", "previous-window");
    gtk_menu_add(section, "Close subwindows", "close-subwindows");
    gtk_menu_append_section(menu, section);
    section = g_menu_new();
    gtk_menu_add(section, "Pd window", "pd-window");
    if (patch_window) {
        gtk_menu_add(section, "Parent Window", "findparent");
    }
    gtk_menu_append_section(menu, section);
    if (!gtk_window_menu) {
        gtk_window_menu = g_menu_new();
    }
    g_menu_append_section(menu, NULL, G_MENU_MODEL(gtk_window_menu));
    gtk_update_window_menu();
    return (menu);
}

static GMenu *gtk_build_tool_menu(void) {
    GMenu *menu = g_menu_new(), *section = g_menu_new();
    gtk_menu_add(section, "Load Meter", "load-meter");
    gtk_menu_append_section(menu, section);
    section = g_menu_new();
    gtk_menu_add(section, "Message...", "message");
    gtk_menu_append_section(menu, section);
    return (menu);
}

static GMenu *gtk_build_help_menu(void) {
    GMenu *menu = g_menu_new(), *section = g_menu_new();
    gtk_menu_add(section, "About Pd", "about");
    gtk_menu_add(section, "HTML Manual...", "manual");
    gtk_menu_add(section, "Browser...", "browser");
    gtk_menu_add(section, "List of objects...", "object-list");
    gtk_menu_append_section(menu, section);
    section = g_menu_new();
    gtk_menu_add(section, "puredata.info", "puredata-info");
    gtk_menu_add(section, "Check for updates", "updates");
    gtk_menu_add(section, "Report a bug", "bug");
    gtk_menu_append_section(menu, section);
    return (menu);
}

static void gtk_menubar_add(GMenu *bar, const char *label, GMenu *menu) {
    g_menu_append_submenu(bar, label, G_MENU_MODEL(menu));
    g_object_unref(menu);
}

static GtkWidget *gtk_menu_bar_new(t_gc *c, int patch_window) {
    GMenu *bar = g_menu_new();
    GtkWidget *widget;
    gtk_action_group_add(c ? c->window : gtk_main_window, c);
    gtk_menubar_add(bar, "File", gtk_build_file_menu(patch_window));
    gtk_menubar_add(bar, "Edit", gtk_build_edit_menu(patch_window));
    gtk_menubar_add(bar, "Find", gtk_build_find_menu(patch_window));
    gtk_menubar_add(bar, "Media", gtk_build_media_menu());
    gtk_menubar_add(bar, "Window", gtk_build_window_menu(patch_window));
    gtk_menubar_add(bar, "Tool", gtk_build_tool_menu());
    gtk_menubar_add(bar, "Help", gtk_build_help_menu());
    widget = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(bar));
    g_object_unref(bar);
    return (widget);
}
static void canvas_set_editmode(t_gc *c, int state) {
    canvas_message_float(c, "editmode", state != 0);
}
static int key_shortcut(t_gc *c, guint keyval, GdkModifierType state, int down) {
    guint key = gdk_keyval_to_lower(keyval);
#ifdef MACOSX
    int primary = ((state & GDK_META_MASK) != 0);
#else
    int primary = ((state & GDK_CONTROL_MASK) != 0);
#endif
    int shift = ((state & GDK_SHIFT_MASK) != 0);
    const char *command = 0;
    if (!primary) {
        return (0);
    }
    if (key == GDK_KEY_e) {
        if (down) {
            canvas_set_editmode(c, !c->editmode);
        }
        return (1);
    }
    if (key == GDK_KEY_n && !shift) {
        if (down) {
            gtk_new_patch();
        }
        return (1);
    }
    if (key == GDK_KEY_1) {
        command = "obj";
    } else if (key == GDK_KEY_2) {
        command = "msg";
    } else if (key == GDK_KEY_3) {
        command = "floatatom";
    } else if (key == GDK_KEY_4) {
        command = "listbox";
    } else if (key == GDK_KEY_5) {
        command = "text";
    }
    if (command) {
        if (down) {
            canvas_message_float(c, command, 0);
        }
        return (1);
    }
    if (key == GDK_KEY_a) {
        command = "selectall";
    } else if (key == GDK_KEY_c) {
        command = "copy";
    } else if (key == GDK_KEY_d) {
        command = "duplicate";
    } else if (key == GDK_KEY_k) {
        command = "connect_selection";
    } else if (key == GDK_KEY_s) {
        command = shift ? "menusaveas" : "menusave";
    } else if (key == GDK_KEY_v) {
        command = "paste";
    } else if (key == GDK_KEY_x) {
        command = "cut";
    } else if (key == GDK_KEY_z) {
        command = shift ? "redo" : "undo";
    }
    if (command) {
        if (down) {
            canvas_message(c, command);
        }
        return (1);
    }
    return (0);
}
static void pressed(GtkGestureClick *g, int n, double x, double y, gpointer d) {
    GdkModifierType s = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(g));
    mouse((t_gc *)d, "mouse", x, y, gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(g)),
          s);
}
static void released(GtkGestureClick *g, int n, double x, double y, gpointer d) {
    GdkModifierType s = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(g));
    int button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(g));
    if (button == GDK_BUTTON_PRIMARY) {
        mouse((t_gc *)d, "mouseup", x, y, button, s);
    }
}
static void motion(GtkEventControllerMotion *ctl, double x, double y, gpointer d) {
    t_atom a[3];
    GdkModifierType s = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(ctl));
    SETFLOAT(a, x);
    SETFLOAT(a + 1, y);
    SETFLOAT(a + 2, ((s & GDK_SHIFT_MASK) ? 1 : 0) | ((s & GDK_CONTROL_MASK) ? 2 : 0) |
                        ((s & GDK_ALT_MASK) ? 4 : 0));
    pd_typedmess(&((t_gc *)d)->canvas->gl_pd, gensym("motion"), 3, a);
}
static gboolean keyevent(guint keyval, GdkModifierType state, gpointer d, int down) {
    t_atom a[3];
    t_gc *c = (t_gc *)d;
    gunichar code = gdk_keyval_to_unicode(keyval);
    if (key_shortcut(c, keyval, state, down)) {
        return (TRUE);
    }
    if (down && keyval == GDK_KEY_Escape) {
        canvas_message(c, "deselectall");
    }
    if (down && (keyval == GDK_KEY_Tab || keyval == GDK_KEY_ISO_Left_Tab)) {
        canvas_message_float(c, "cycleselect", (state & GDK_SHIFT_MASK) ? -1 : 1);
    }
    SETFLOAT(a, down);
    if (keyval == GDK_KEY_BackSpace) {
        SETFLOAT(a + 1, 8);
    } else if (keyval == GDK_KEY_Tab) {
        SETFLOAT(a + 1, 9);
    } else if (keyval == GDK_KEY_Return) {
        SETFLOAT(a + 1, 10);
    } else if (keyval == GDK_KEY_Escape) {
        SETFLOAT(a + 1, 27);
    } else if (keyval == GDK_KEY_Delete) {
        SETFLOAT(a + 1, 127);
    } else if (keyval == GDK_KEY_dead_tilde) {
        SETFLOAT(a + 1, '~');
    } else if (code) {
        SETFLOAT(a + 1, code);
    } else {
        SETSYMBOL(a + 1, gensym(gdk_keyval_name(keyval)));
    }
    SETFLOAT(a + 2, (state & GDK_SHIFT_MASK) ? 1 : 0);
    pd_typedmess(&c->canvas->gl_pd, gensym("key"), 3, a);
    return (TRUE);
}
static gboolean keypress(GtkEventControllerKey *ctl, guint keyval, guint keycode,
                         GdkModifierType state, gpointer d) {
    return (keyevent(keyval, state, d, 1));
}
static void keyrelease(GtkEventControllerKey *ctl, guint keyval, guint keycode,
                       GdkModifierType state, gpointer d) {
    keyevent(keyval, state, d, 0);
}
static gboolean close_window(GtkWindow *w, gpointer d) {
    t_atom a;
    SETFLOAT(&a, 0);
    pd_typedmess(&((t_gc *)d)->canvas->gl_pd, gensym("menuclose"), 1, &a);
    return (TRUE);
}
static void mapped_tick(void *d) {
    t_atom a;
    SETFLOAT(&a, 1);
    pd_typedmess(&((t_gc *)d)->canvas->gl_pd, gensym("map"), 1, &a);
}
static void mapped(GtkWidget *widget, gpointer d) {
    t_gc *c = (t_gc *)d;
    if (!c->map_clock) {
        c->map_clock = clock_new(c, (t_method)mapped_tick);
    }
    clock_delay(c->map_clock, 0);
}
static t_gc *gtk_canvas_new(t_canvas *canvas, int width, int height, unsigned int bg,
                            unsigned int fg, int editmode) {
    t_gc *c;
    GtkWidget *box;
    GtkEventController *m, *k;
    GtkGesture *click;
    if (!gtk_start()) {
        return (0);
    }
    c = (t_gc *)g_malloc0(sizeof(*c));
    c->canvas = canvas;
    c->editmode = editmode;
    c->background = bg;
    c->foreground = fg;
    c->window = gtk_window_new();
    c->area = gtk_drawing_area_new();
    gtk_window_set_default_size(GTK_WINDOW(c->window), width, height);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(c->window), box);
    gtk_box_append(GTK_BOX(box), gtk_menu_bar_new(c, 1));
    gtk_widget_set_vexpand(c->area, TRUE);
    gtk_box_append(GTK_BOX(box), c->area);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(c->area), draw, c, 0);
    gtk_widget_set_focusable(c->area, TRUE);
    m = gtk_event_controller_motion_new();
    g_signal_connect(m, "motion", G_CALLBACK(motion), c);
    gtk_widget_add_controller(c->area, m);
    k = gtk_event_controller_key_new();
    g_signal_connect(k, "key-pressed", G_CALLBACK(keypress), c);
    g_signal_connect(k, "key-released", G_CALLBACK(keyrelease), c);
    gtk_widget_add_controller(c->area, k);
    click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(click, "pressed", G_CALLBACK(pressed), c);
    g_signal_connect(click, "released", G_CALLBACK(released), c);
    gtk_widget_add_controller(c->area, GTK_EVENT_CONTROLLER(click));
    g_signal_connect(c->window, "map", G_CALLBACK(mapped), c);
    g_signal_connect(c->window, "close-request", G_CALLBACK(close_window), c);
    c->next = gc_list;
    gc_list = c;
    gtk_window_present(GTK_WINDOW(c->window));
    gtk_widget_grab_focus(c->area);
    return (c);
}
static void gtk_canvas_free(t_canvas *canvas) {
    t_gc **p = &gc_list, *c;
    while ((c = *p)) {
        if (c->canvas == canvas) {
            t_gi *i = c->items;
            *p = c->next;
            if (c->map_clock) {
                clock_free(c->map_clock);
            }
            gtk_window_destroy(GTK_WINDOW(c->window));
            while (i) {
                t_gi *n = i->next;
                gi_free(i);
                i = n;
            }
            g_free(c->title);
            g_free(c);
            gtk_update_window_menu();
            return;
        }
        p = &c->next;
    }
}

static void rect_create(t_canvas *cv, const char *n, const char *g, const char *cl, int x1, int y1,
                        int x2, int y2, int w, unsigned int f, unsigned int o) {
    int p[4];
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    p[0] = x1;
    p[1] = y1;
    p[2] = x2;
    p[3] = y2;
    i = gi_new(c, GI_RECT, n, g, cl);
    points_int(i, p, 4);
    i->width = w;
    i->fill = f;
    i->outline = o;
    redraw(c);
}
static void shape_update(t_canvas *cv, const char *n, int ch, int x1, int y1, int x2, int y2, int w,
                         unsigned int f, unsigned int o) {
    int p[4];
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    p[0] = x1;
    p[1] = y1;
    p[2] = x2;
    p[3] = y2;
    for (i = c->items; i; i = i->next) {
        if (gi_match(i, n)) {
            if (ch & PDGUI_CHANGE_POINTS) {
                points_int(i, p, 4);
            }
            if (ch & PDGUI_CHANGE_WIDTH) {
                i->width = w;
            }
            if (ch & PDGUI_CHANGE_FILL) {
                i->fill = f;
            }
            if (ch & PDGUI_CHANGE_OUTLINE) {
                i->outline = o;
            }
        }
    }
    redraw(c);
}
static void oval_create(t_canvas *cv, const char *n, const char *g, int x1, int y1, int x2, int y2,
                        int w, unsigned int f, unsigned int o) {
    t_gc *c;
    t_gi *i;
    rect_create(cv, n, g, 0, x1, y1, x2, y2, w, f, o);
    c = gc_find(cv);
    if (!c) {
        return;
    }
    i = c->items;
    while (i && i->next) {
        i = i->next;
    }
    if (i) {
        i->type = GI_OVAL;
    }
}
static void line_create(t_canvas *cv, const char *n, const char *g, const int *p, int np, int w,
                        unsigned int col, int dash) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    i = gi_new(c, GI_PATH, n, g, 0);
    points_int(i, p, np);
    i->width = w;
    i->outline = col;
    i->dashed = dash;
    redraw(c);
}
static void line_update(t_canvas *cv, const char *n, int ch, const int *p, int np, int w,
                        unsigned int col) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    for (i = c->items; i; i = i->next) {
        if (gi_match(i, n)) {
            if (ch & PDGUI_CHANGE_POINTS) {
                points_int(i, p, np);
            }
            if (ch & PDGUI_CHANGE_WIDTH) {
                i->width = w;
            }
            if (ch & PDGUI_CHANGE_COLOR) {
                i->outline = col;
            }
        }
    }
    redraw(c);
}
static void poly_create(t_canvas *cv, const char *n, const char *g, const int *p, int np, int w,
                        unsigned int f, unsigned int o, int m) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    i = gi_new(c, GI_PATH, n, g, 0);
    points_int(i, p, np);
    i->closed = 1;
    i->width = w;
    i->fill = f;
    i->outline = o;
    redraw(c);
}
static void poly_update(t_canvas *cv, const char *n, int ch, const int *p, int np, int w,
                        unsigned int f, unsigned int o) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    for (i = c->items; i; i = i->next) {
        if (gi_match(i, n)) {
            if (ch & PDGUI_CHANGE_POINTS) {
                points_int(i, p, np);
            }
            if (ch & PDGUI_CHANGE_WIDTH) {
                i->width = w;
            }
            if (ch & PDGUI_CHANGE_FILL) {
                i->fill = f;
            }
            if (ch & PDGUI_CHANGE_OUTLINE) {
                i->outline = o;
            }
        }
    }
    redraw(c);
}
static void path_create(t_canvas *cv, const char *n, const t_word *p, int np, int closed,
                        int smooth, int w, unsigned int f, unsigned int o) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    i = gi_new(c, GI_PATH, n, 0, 0);
    points_word(i, p, np);
    i->closed = closed;
    i->width = w;
    i->fill = f;
    i->outline = o;
    redraw(c);
}
static void path_points(t_canvas *cv, const char *n, const t_word *p, int np) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    for (i = c->items; i; i = i->next) {
        if (gi_match(i, n)) {
            points_word(i, p, np);
        }
    }
    redraw(c);
}
static t_gi *text_new(t_canvas *cv, const char *n, const char *g, const char *cl, int x, int y) {
    int p[2];
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return (0);
    }
    p[0] = x;
    p[1] = y;
    i = gi_new(c, GI_TEXT, n, g, cl);
    points_int(i, p, 2);
    i->fill = 0;
    return (i);
}
static void text_create(t_canvas *c, const char *n, const char *g, int x, int y) {
    text_new(c, n, g, 0, x, y);
}
static void text_plain(t_canvas *c, const char *n, const char *g, int x, int y) {
    text_new(c, n, g, 0, x, y);
}
static void text_grouped(t_canvas *c, const char *n, const char *g, const char *cl, int x, int y) {
    text_new(c, n, g, cl, x, y);
}
static void text_anchor(t_canvas *c, const char *n, const char *g, const char *cl, int x, int y,
                        const char *t, t_pdgui_anchor a, const char *f, int fs, const char *w,
                        unsigned int col) {
    t_gi *i = text_new(c, n, g, cl, x, y);
    if (!i) {
        return;
    }
    g_free(i->text);
    i->text = sdup(t);
    g_free(i->font);
    i->font = sdup(f);
    g_free(i->weight);
    i->weight = sdup(w);
    i->fontsize = fs;
    i->fill = col;
    i->anchor = a;
    redraw(gc_find(c));
}
static void text_update(t_canvas *cv, const char *n, int ch, int x, int y, const char *t,
                        const char *f, int fs, const char *w, unsigned int col) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    for (i = c->items; i; i = i->next) {
        if (gi_match(i, n)) {
            if (ch & PDGUI_CHANGE_POINTS) {
                i->points[0] = x, i->points[1] = y;
            }
            if (ch & PDGUI_CHANGE_CONTENT) {
                g_free(i->text);
                i->text = sdup(t);
            }
            if (ch & PDGUI_CHANGE_FONT) {
                g_free(i->font);
                i->font = sdup(f);
                g_free(i->weight);
                i->weight = sdup(w);
                i->fontsize = fs;
            }
            if (ch & PDGUI_CHANGE_COLOR) {
                i->fill = col;
            }
        }
    }
    redraw(c);
}
static void canvas_text(t_canvas *c, const char *n, int x, int y, const char *t, int fs,
                        unsigned int col) {
    text_anchor(c, n, 0, 0, x, y, t, PDGUI_ANCHOR_NORTH_WEST, sys_font, fs, sys_fontweight, col);
}
static void canvas_text_group(t_canvas *c, const char *n, const char *cl, int x, int y,
                              const char *t, int fs, unsigned int col) {
    text_anchor(c, n, 0, cl, x, y, t, PDGUI_ANCHOR_NORTH_WEST, sys_font, fs, sys_fontweight, col);
}
static void canvas_text_label(t_canvas *c, const char *n, int x, int y, const char *t, int fs,
                              unsigned int col) {
    canvas_text_group(c, n, "label", x, y, t, fs, col);
}
static void selection(t_canvas *cv, const char *n, int a, int b) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    for (i = c->items; i; i = i->next) {
        if (gi_match(i, n)) {
            i->selstart = a, i->selend = b;
        }
    }
    redraw(c);
}
static void editing(t_canvas *cv, const char *n, int s) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    for (i = c->items; i; i = i->next) {
        if (gi_match(i, n)) {
            i->editing = s;
        }
    }
    redraw(c);
}
static void item_destroy(t_canvas *cv, const char *n) {
    t_gc *c = gc_find(cv);
    t_gi **p, *i;
    if (!c) {
        return;
    }
    p = &c->items;
    while ((i = *p)) {
        if (gi_match(i, n)) {
            *p = i->next;
            gi_free(i);
        } else {
            p = &i->next;
        }
    }
    redraw(c);
}
static void item_move(t_canvas *cv, const char *n, int dx, int dy) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    int k;
    if (!c) {
        return;
    }
    for (i = c->items; i; i = i->next) {
        if (gi_match(i, n)) {
            for (k = 0; k + 1 < i->npoints; k += 2) {
                i->points[k] += dx, i->points[k + 1] += dy;
            }
        }
    }
    redraw(c);
}
static void item_order(t_canvas *cv, const char *n, t_pdgui_order o, const char *r) {
    t_gc *c = gc_find(cv);
    t_gi **p, *i, **d;
    if (!c) {
        return;
    }
    p = &c->items;
    while (*p && !gi_match(*p, n)) {
        p = &(*p)->next;
    }
    if (!(i = *p)) {
        return;
    }
    *p = i->next;
    i->next = 0;
    d = &c->items;
    if (o == PDGUI_ORDER_TOP) {
        while (*d) {
            d = &(*d)->next;
        }
    } else {
        while (*d && !gi_match(*d, r)) {
            d = &(*d)->next;
        }
        if (o == PDGUI_ORDER_ABOVE && *d) {
            d = &(*d)->next;
        }
    }
    i->next = *d;
    *d = i;
    redraw(c);
}
static void item_style(t_canvas *cv, const char *n, int ch, int w, unsigned int f, unsigned int o) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    for (i = c->items; i; i = i->next) {
        if (gi_match(i, n)) {
            if (ch & PDGUI_CHANGE_WIDTH) {
                i->width = w;
            }
            if (ch & PDGUI_CHANGE_FILL) {
                if (i->type == GI_PATH && !i->closed) {
                    i->outline = f;
                } else {
                    i->fill = f;
                }
            }
            if (ch & PDGUI_CHANGE_OUTLINE) {
                i->outline = o;
            }
        }
    }
    redraw(c);
}
static void canvas_clear(t_canvas *cv) {
    t_gc *c = gc_find(cv);
    t_gi *i;
    if (!c) {
        return;
    }
    i = c->items;
    c->items = 0;
    while (i) {
        t_gi *n = i->next;
        gi_free(i);
        i = n;
    }
    redraw(c);
}
static void canvas_colors(t_canvas *cv, unsigned int b, unsigned int f) {
    t_gc *c = gc_find(cv);
    if (c) {
        c->background = b, c->foreground = f, redraw(c);
    }
}
static void cords(t_canvas *cv, int s) {
    redraw(gc_find(cv));
}
static void patchcord(t_canvas *c, const char *n, int x1, int y1, int x2, int y2, int w,
                      unsigned int col) {
    int p[4];
    p[0] = x1;
    p[1] = y1;
    p[2] = x2;
    p[3] = y2;
    line_create(c, n, "cord", p, 4, w, col, 0);
}

static void popup_action(GtkButton *button, gpointer data) {
    t_gc *c = (t_gc *)data;
    t_atom a[3];
    int action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "pd-action"));
    int x = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "pd-x"));
    int y = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "pd-y"));
    if (c->popup) {
        gtk_popover_popdown(GTK_POPOVER(c->popup));
    }
    SETFLOAT(a, action);
    SETFLOAT(a + 1, x);
    SETFLOAT(a + 2, y);
    pd_typedmess(&c->canvas->gl_pd, gensym("done-popup"), 3, a);
}

static void popup_button(t_gc *c, GtkWidget *box, const char *label, int action, int x, int y,
                         int enabled) {
    GtkWidget *button = gtk_button_new_with_label(label);
    g_object_set_data(G_OBJECT(button), "pd-action", GINT_TO_POINTER(action));
    g_object_set_data(G_OBJECT(button), "pd-x", GINT_TO_POINTER(x));
    g_object_set_data(G_OBJECT(button), "pd-y", GINT_TO_POINTER(y));
    gtk_widget_set_sensitive(button, enabled);
    g_signal_connect(button, "clicked", G_CALLBACK(popup_action), c);
    gtk_box_append(GTK_BOX(box), button);
}

static void canvas_popup(t_gc *c, int x, int y, int can_properties, int can_open) {
    GtkWidget *box;
    GdkRectangle point;
    if (c->popup) {
        gtk_widget_unparent(c->popup);
    }
    c->popup = gtk_popover_new();
    gtk_widget_set_parent(c->popup, c->area);
    gtk_popover_set_autohide(GTK_POPOVER(c->popup), TRUE);
    gtk_popover_set_position(GTK_POPOVER(c->popup), GTK_POS_BOTTOM);
    point.x = x;
    point.y = y;
    point.width = 1;
    point.height = 1;
    gtk_popover_set_pointing_to(GTK_POPOVER(c->popup), &point);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(box, 4);
    gtk_widget_set_margin_end(box, 4);
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);
    gtk_popover_set_child(GTK_POPOVER(c->popup), box);
    popup_button(c, box, "Properties", 0, x, y, can_properties);
    popup_button(c, box, "Open", 1, x, y, can_open);
    popup_button(c, box, "Help", 2, x, y, 1);
    gtk_popover_popup(GTK_POPOVER(c->popup));
}

typedef struct _gtk_confirm {
    t_canvas *canvas;
    t_symbol *receiver;
    t_atom *atoms;
    int argc;
    int save_discard_cancel;
} t_gtk_confirm;

static GtkWidget *gtk_properties_entry(GtkWidget *grid, int row, const char *label, double value) {
    GtkWidget *caption = gtk_label_new(label);
    GtkWidget *entry = gtk_entry_new();
    char text[64];
    g_ascii_dtostr(text, sizeof(text), value);
    gtk_label_set_xalign(GTK_LABEL(caption), 0);
    gtk_editable_set_text(GTK_EDITABLE(entry), text);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), caption, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);
    return (entry);
}

static GtkWidget *gtk_properties_compact_entry(GtkWidget *grid, int row, int column,
                                               const char *label, double value) {
    GtkWidget *caption = gtk_label_new(label);
    GtkWidget *entry = gtk_entry_new();
    char text[64];
    g_ascii_dtostr(text, sizeof(text), value);
    gtk_editable_set_text(GTK_EDITABLE(entry), text);
    gtk_editable_set_width_chars(GTK_EDITABLE(entry), 5);
    gtk_grid_attach(GTK_GRID(grid), caption, column, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry, column + 1, row, 1, 1);
    g_object_set_data(G_OBJECT(entry), "pd-caption", caption);
    return (entry);
}

static double gtk_properties_value(GtkWidget *entry) {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    char *end = NULL;
    double value = g_ascii_strtod(text, &end);
    return (end != text ? value : 0);
}

static void gtk_canvas_properties_sensitivity(t_gtk_canvas_properties *p) {
    int graph = gtk_check_button_get_active(GTK_CHECK_BUTTON(p->graph));
    int i;
    gtk_widget_set_sensitive(p->entries[0], !graph);
    gtk_widget_set_sensitive(p->entries[1], !graph);
    gtk_widget_set_sensitive(p->hide_name, graph);
    for (i = 2; i < 10; i++) {
        gtk_widget_set_sensitive(p->entries[i], graph);
    }
}

static void gtk_canvas_properties_toggled(GtkCheckButton *button, gpointer data) {
    gtk_canvas_properties_sensitivity((t_gtk_canvas_properties *)data);
    (void)button;
}

static void gtk_canvas_properties_apply(t_gtk_canvas_properties *p) {
    t_atom base[13];
    t_atom *atoms = base;
    int graph = gtk_check_button_get_active(GTK_CHECK_BUTTON(p->graph));
    int hide = gtk_check_button_get_active(GTK_CHECK_BUTTON(p->hide_name));
    int argc = 12, i;
    t_binbuf *text = NULL;
    SETFLOAT(atoms, gtk_properties_value(p->entries[0]));
    SETFLOAT(atoms + 1, gtk_properties_value(p->entries[1]));
    SETFLOAT(atoms + 2, graph + 2 * hide);
    for (i = 2; i < 10; i++) {
        SETFLOAT(atoms + i + 1, gtk_properties_value(p->entries[i]));
    }
    SETFLOAT(atoms + 11, 1);
    if (p->object_text) {
        const char *object = gtk_editable_get_text(GTK_EDITABLE(p->object_text));
        text = binbuf_new();
        binbuf_text(text, object, strlen(object));
        if (binbuf_getnatom(text) > 0) {
            int text_argc = binbuf_getnatom(text);
            atoms = (t_atom *)getbytes((13 + text_argc) * sizeof(*atoms));
            memcpy(atoms, base, 12 * sizeof(*atoms));
            SETSYMBOL(atoms + 12, gensym("text"));
            memcpy(atoms + 13, binbuf_getvec(text), text_argc * sizeof(*atoms));
            argc = 13 + text_argc;
        }
    }
    pdgui_dialog_stub_send(p->stub, "donecanvasdialog", argc, atoms);
    if (atoms != base) {
        freebytes(atoms, argc * sizeof(*atoms));
    }
    if (text) {
        binbuf_free(text);
    }
}

static void gtk_canvas_properties_destroy(GtkWidget *window, gpointer data) {
    t_gtk_canvas_properties *p = (t_gtk_canvas_properties *)data;
    t_gtk_canvas_properties **link = &gtk_canvas_properties_list;
    while (*link && *link != p) {
        link = &(*link)->next;
    }
    if (*link) {
        *link = p->next;
    }
    pdgui_dialog_stub_close(p->stub);
    g_free(p);
    (void)window;
}

static void gtk_canvas_properties_response(GtkDialog *dialog, int response, gpointer data) {
    t_gtk_canvas_properties *p = (t_gtk_canvas_properties *)data;
    if (response == GTK_RESPONSE_APPLY || response == GTK_RESPONSE_ACCEPT) {
        gtk_canvas_properties_apply(p);
    }
    if (response != GTK_RESPONSE_APPLY) {
        gtk_window_destroy(GTK_WINDOW(dialog));
    }
}

static void gtk_canvas_properties_open(const t_pdgui_service_request *r) {
    t_gtk_canvas_properties *p = g_new0(t_gtk_canvas_properties, 1);
    t_gc *c = gc_find((t_canvas *)r->sr_object);
    GtkWidget *content, *grid, *appearance, *range, *heading;
    int graph_flags = r->sr_ints[0];
    p->stub = pdgui_dialog_stub_new(r->sr_owner, (void *)r->sr_object);
    p->window = gtk_dialog_new_with_buttons(
        "Canvas Properties", gtk_menu_parent(c), GTK_DIALOG_MODAL, "_Cancel", GTK_RESPONSE_CANCEL,
        "_Apply", GTK_RESPONSE_APPLY, "_OK", GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_resizable(GTK_WINDOW(p->window), FALSE);
    content = gtk_dialog_get_content_area(GTK_DIALOG(p->window));
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);
    gtk_widget_set_margin_top(content, 12);
    gtk_widget_set_margin_bottom(content, 12);
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    heading = gtk_label_new("Scale");
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    gtk_widget_add_css_class(heading, "heading");
    gtk_box_append(GTK_BOX(content), heading);
    gtk_box_append(GTK_BOX(content), grid);
    p->entries[0] = gtk_properties_entry(grid, 0, "X units per pixel", r->sr_floats[0]);
    p->entries[1] = gtk_properties_entry(grid, 1, "Y units per pixel", r->sr_floats[1]);
    appearance = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_top(appearance, 10);
    heading = gtk_label_new("Appearance on parent patch");
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    gtk_widget_add_css_class(heading, "heading");
    gtk_box_append(GTK_BOX(appearance), heading);
    gtk_box_append(GTK_BOX(content), appearance);
    p->graph = gtk_check_button_new_with_label("Graph-On-Parent");
    p->hide_name = gtk_check_button_new_with_label("Hide object name and arguments");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(p->graph), graph_flags & 1);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(p->hide_name), graph_flags & 2);
    gtk_box_append(GTK_BOX(appearance), p->graph);
    gtk_box_append(GTK_BOX(appearance), p->hide_name);
    heading = gtk_label_new("Range and size");
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    gtk_widget_add_css_class(heading, "heading");
    gtk_widget_set_margin_top(heading, 10);
    gtk_box_append(GTK_BOX(content), heading);
    range = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(range), 6);
    gtk_grid_set_row_spacing(GTK_GRID(range), 6);
    gtk_box_append(GTK_BOX(content), range);
    p->entries[2] = gtk_properties_compact_entry(range, 0, 0, "X from", r->sr_floats[2]);
    p->entries[4] = gtk_properties_compact_entry(range, 0, 2, "to", r->sr_floats[4]);
    p->entries[6] = gtk_properties_compact_entry(range, 0, 4, "Size", r->sr_ints[1]);
    p->entries[8] = gtk_properties_compact_entry(range, 0, 6, "Margin", r->sr_ints[3]);
    p->entries[3] = gtk_properties_compact_entry(range, 1, 0, "Y from", r->sr_floats[3]);
    p->entries[5] = gtk_properties_compact_entry(range, 1, 2, "to", r->sr_floats[5]);
    p->entries[7] = gtk_properties_compact_entry(range, 1, 4, "Size", r->sr_ints[2]);
    p->entries[9] = gtk_properties_compact_entry(range, 1, 6, "Margin", r->sr_ints[4]);
    if (gtk_canvas_dialog_text && *gtk_canvas_dialog_text) {
        GtkWidget *label = gtk_label_new("Object");
        p->object_text = gtk_entry_new();
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_editable_set_text(GTK_EDITABLE(p->object_text), gtk_canvas_dialog_text);
        gtk_box_append(GTK_BOX(appearance), label);
        gtk_box_append(GTK_BOX(appearance), p->object_text);
    }
    g_signal_connect(p->graph, "toggled", G_CALLBACK(gtk_canvas_properties_toggled), p);
    g_signal_connect(p->window, "response", G_CALLBACK(gtk_canvas_properties_response), p);
    g_signal_connect(p->window, "destroy", G_CALLBACK(gtk_canvas_properties_destroy), p);
    p->next = gtk_canvas_properties_list;
    gtk_canvas_properties_list = p;
    gtk_canvas_properties_sensitivity(p);
    gtk_window_present(GTK_WINDOW(p->window));
}

static GtkWidget *gtk_properties_text_entry(GtkWidget *grid, int row, int column, const char *label,
                                            const char *value, int width) {
    GtkWidget *caption = gtk_label_new(label);
    GtkWidget *entry = gtk_entry_new();
    gtk_label_set_xalign(GTK_LABEL(caption), 0);
    gtk_editable_set_text(GTK_EDITABLE(entry), value ? value : "");
    gtk_editable_set_width_chars(GTK_EDITABLE(entry), width);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), caption, column, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry, column + 1, row, 1, 1);
    return (entry);
}

static GtkWidget *gtk_properties_number_entry(GtkWidget *grid, int row, int column,
                                              const char *label, double value) {
    char text[64];
    g_ascii_dtostr(text, sizeof(text), value);
    return (gtk_properties_text_entry(grid, row, column, label, text, 7));
}

static GtkWidget *gtk_properties_dropdown(GtkWidget *grid, int row, int column, const char *label,
                                          const char *const *items, int selected) {
    GtkWidget *caption = gtk_label_new(label);
    GtkWidget *dropdown = gtk_drop_down_new_from_strings(items);
    gtk_label_set_xalign(GTK_LABEL(caption), 0);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), selected);
    gtk_grid_attach(GTK_GRID(grid), caption, column, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), dropdown, column + 1, row, 1, 1);
    g_object_set_data(G_OBJECT(dropdown), "pd-caption", caption);
    return (dropdown);
}

static void gtk_properties_field_visible(GtkWidget *widget, int visible) {
    GtkWidget *caption = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "pd-caption"));
    gtk_widget_set_visible(widget, visible);
    if (caption) {
        gtk_widget_set_visible(caption, visible);
    }
}

static GtkWidget *gtk_properties_color(GtkWidget *box, const char *label, unsigned int color) {
    GdkRGBA rgba;
    GtkWidget *column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *caption = gtk_label_new(label);
    GtkWidget *button;
    rgba.red = ((color >> 16) & 0xff) / 255.;
    rgba.green = ((color >> 8) & 0xff) / 255.;
    rgba.blue = (color & 0xff) / 255.;
    rgba.alpha = 1.;
    button = gtk_color_button_new_with_rgba(&rgba);
    gtk_box_append(GTK_BOX(column), caption);
    gtk_box_append(GTK_BOX(column), button);
    gtk_box_append(GTK_BOX(box), column);
    return (button);
}

static GtkWidget *gtk_properties_frame(GtkWidget *content, const char *title) {
    GtkWidget *frame = gtk_frame_new(title);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_margin_top(box, 6);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_frame_set_child(GTK_FRAME(frame), box);
    gtk_box_append(GTK_BOX(content), frame);
    return (box);
}

static void gtk_properties_style(GtkWidget *window) {
    static int installed;
    if (!installed) {
        static const char css[] =
            ".pd-properties { background-color: @theme_bg_color; }"
            ".pd-properties entry { min-height: 22px; padding: 2px 7px; "
            "background-color: alpha(currentColor, .065); border: none; "
            "border-radius: 9px; box-shadow: none; }"
            ".pd-properties entry:focus { outline: 2px solid @theme_selected_bg_color; "
            "outline-offset: -2px; }"
            ".pd-properties button { min-height: 22px; padding: 2px 8px; }"
            ".pd-properties button.pill { border-radius: 9999px; font-weight: 600; }"
            ".pd-properties dropdown button { min-height: 22px; padding: 2px 7px; "
            "background-color: alpha(currentColor, .065); border: none; "
            "border-radius: 9px; box-shadow: none; }"
            ".pd-properties frame { margin-top: 1px; margin-bottom: 1px; }"
            ".pd-properties frame > border { border-width: 1px; border-radius: 12px; "
            "border-color: alpha(currentColor, .18); }";
        GtkCssProvider *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(provider, css, -1);
        gtk_style_context_add_provider_for_display(gtk_widget_get_display(window),
                                                   GTK_STYLE_PROVIDER(provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(provider);
        installed = 1;
    }
    gtk_widget_add_css_class(window, "pd-properties");
}

static void gtk_properties_color_string(GtkWidget *button, char *text, size_t size) {
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &rgba);
    pd_snprintf(text, size, "#%02x%02x%02x", (int)(rgba.red * 255. + .5),
                (int)(rgba.green * 255. + .5), (int)(rgba.blue * 255. + .5));
}

static t_symbol *gtk_properties_symbol(GtkWidget *entry) {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    return (gensym(*text ? text : "empty"));
}

static void gtk_iemgui_properties_apply(t_gtk_iemgui_properties *p) {
    t_atom atoms[18];
    char background[8], foreground[8], label_color[8];
    SETFLOAT(atoms, gtk_properties_value(p->entries[IEM_WIDTH]));
    SETFLOAT(atoms + 1, gtk_properties_value(p->entries[IEM_HEIGHT]));
    SETFLOAT(atoms + 2, gtk_properties_value(p->entries[IEM_RANGE_MIN]));
    SETFLOAT(atoms + 3, gtk_properties_value(p->entries[IEM_RANGE_MAX]));
    SETFLOAT(atoms + 4, p->has_mode ? gtk_drop_down_get_selected(GTK_DROP_DOWN(p->mode)) : -1);
    SETFLOAT(atoms + 5,
             p->has_loadinit ? gtk_check_button_get_active(GTK_CHECK_BUTTON(p->loadinit)) : -1);
    SETFLOAT(atoms + 6, gtk_properties_value(p->entries[IEM_NUMBER]));
    SETSYMBOL(atoms + 7, gtk_properties_symbol(p->entries[IEM_SEND]));
    SETSYMBOL(atoms + 8, gtk_properties_symbol(p->entries[IEM_RECEIVE]));
    SETSYMBOL(atoms + 9, gtk_properties_symbol(p->entries[IEM_LABEL]));
    SETFLOAT(atoms + 10, gtk_properties_value(p->entries[IEM_LABEL_X]));
    SETFLOAT(atoms + 11, gtk_properties_value(p->entries[IEM_LABEL_Y]));
    SETFLOAT(atoms + 12, gtk_drop_down_get_selected(GTK_DROP_DOWN(p->font)));
    SETFLOAT(atoms + 13, gtk_properties_value(p->entries[IEM_FONT_SIZE]));
    gtk_properties_color_string(p->colors[0], background, sizeof(background));
    gtk_properties_color_string(p->colors[1], foreground, sizeof(foreground));
    gtk_properties_color_string(p->colors[2], label_color, sizeof(label_color));
    SETSYMBOL(atoms + 14, gensym(background));
    SETSYMBOL(atoms + 15, gensym(foreground));
    SETSYMBOL(atoms + 16, gensym(label_color));
    SETFLOAT(atoms + 17,
             p->has_steady ? gtk_check_button_get_active(GTK_CHECK_BUTTON(p->steady)) : -1);
    pdgui_dialog_stub_send(p->stub, "dialog", 18, atoms);
}

static void gtk_iemgui_properties_destroy(GtkWidget *window, gpointer data) {
    t_gtk_iemgui_properties *p = (t_gtk_iemgui_properties *)data;
    t_gtk_iemgui_properties **link = &gtk_iemgui_properties_list;
    while (*link && *link != p) {
        link = &(*link)->next;
    }
    if (*link) {
        *link = p->next;
    }
    pdgui_dialog_stub_close(p->stub);
    g_free(p);
    (void)window;
}

static void gtk_iemgui_properties_response(GtkDialog *dialog, int response, gpointer data) {
    t_gtk_iemgui_properties *p = (t_gtk_iemgui_properties *)data;
    if (response == GTK_RESPONSE_APPLY || response == GTK_RESPONSE_ACCEPT) {
        gtk_iemgui_properties_apply(p);
    }
    if (response != GTK_RESPONSE_APPLY) {
        gtk_window_destroy(GTK_WINDOW(dialog));
    }
}

static void gtk_iemgui_properties_open(const t_pdgui_service_request *r) {
    static const char *const fonts[] = {"Default", "Helvetica", "Times", NULL};
    const char *mode_items[3] = {r->sr_strings[1], r->sr_strings[2], NULL};
    const char *raw_name = r->sr_strings[0] ? r->sr_strings[0] : "GUI";
    const char *width_text = "Width", *range_min_text = "Range minimum";
    const char *range_max_text = "Range maximum", *number_text = "Number";
    t_gtk_iemgui_properties *p = g_new0(t_gtk_iemgui_properties, 1);
    GtkWidget *content, *grid, *checks, *colors;
    GtkWidget *parameters, *messages, *label_box, *colors_box;
    char *object_name, *title, *width_label, *height_label;
    size_t name_length = strlen(raw_name);
    if (name_length > 1 && raw_name[0] == '|' && raw_name[name_length - 1] == '|') {
        object_name = g_strndup(raw_name + 1, name_length - 2);
    } else {
        object_name = g_strdup(raw_name);
    }
    if (!strcmp(object_name, "bang")) {
        width_text = "Size";
        range_min_text = "Flash minimum (ms)";
        range_max_text = "Flash maximum (ms)";
    } else if (!strcmp(object_name, "tgl")) {
        width_text = "Size";
        range_min_text = "Non-zero value";
    } else if (!strcmp(object_name, "hradio") || !strcmp(object_name, "vradio")) {
        width_text = "Size";
        number_text = "Number of cells";
    } else if (!strcmp(object_name, "cnv")) {
        width_text = "Size";
        range_min_text = "Visible width";
        range_max_text = "Visible height";
    } else if (!strcmp(object_name, "nbx")) {
        width_text = "Width (digits)";
        number_text = "Log height";
    }
    title = g_strdup_printf("%s Properties", object_name);
    width_label = g_strdup_printf("%s (min %g)", width_text, r->sr_floats[1]);
    height_label = g_strdup_printf("Height (min %g)", r->sr_floats[3]);
    if (!mode_items[0] || !*mode_items[0]) {
        mode_items[0] = "Linear";
    }
    if (!mode_items[1] || !*mode_items[1]) {
        mode_items[1] = "Logarithmic";
    }
    p->stub = pdgui_dialog_stub_new(r->sr_owner, (void *)r->sr_object);
    p->has_loadinit = (r->sr_ints[2] >= 0);
    p->has_mode = (r->sr_ints[1] >= 0);
    p->has_steady = (r->sr_ints[3] >= 0);
    p->window = gtk_dialog_new_with_buttons(title, gtk_menu_parent(NULL), GTK_DIALOG_MODAL,
                                            "_Cancel", GTK_RESPONSE_CANCEL, "_Apply",
                                            GTK_RESPONSE_APPLY, "_OK", GTK_RESPONSE_ACCEPT, NULL);
    gtk_properties_style(p->window);
    gtk_widget_add_css_class(
        gtk_dialog_get_widget_for_response(GTK_DIALOG(p->window), GTK_RESPONSE_CANCEL), "pill");
    gtk_widget_add_css_class(
        gtk_dialog_get_widget_for_response(GTK_DIALOG(p->window), GTK_RESPONSE_APPLY), "pill");
    gtk_widget_add_css_class(
        gtk_dialog_get_widget_for_response(GTK_DIALOG(p->window), GTK_RESPONSE_ACCEPT), "pill");
    gtk_window_set_resizable(GTK_WINDOW(p->window), FALSE);
    content = gtk_dialog_get_content_area(GTK_DIALOG(p->window));
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);
    gtk_widget_set_margin_top(content, 12);
    gtk_widget_set_margin_bottom(content, 12);
    gtk_box_set_spacing(GTK_BOX(content), 6);
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_box_append(GTK_BOX(content), grid);
    p->entries[IEM_WIDTH] = gtk_properties_number_entry(grid, 0, 0, width_label, r->sr_floats[0]);
    p->entries[IEM_HEIGHT] = gtk_properties_number_entry(grid, 0, 2, height_label, r->sr_floats[2]);

    parameters = gtk_properties_frame(content, "Parameters");
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_box_append(GTK_BOX(parameters), grid);
    p->entries[IEM_RANGE_MIN] =
        gtk_properties_number_entry(grid, 0, 0, range_min_text, r->sr_floats[4]);
    p->entries[IEM_RANGE_MAX] =
        gtk_properties_number_entry(grid, 0, 2, range_max_text, r->sr_floats[5]);
    p->mode = gtk_properties_dropdown(grid, 1, 0, "Mode", mode_items, r->sr_ints[1] > 0);
    p->entries[IEM_NUMBER] = gtk_properties_number_entry(grid, 1, 2, number_text, r->sr_ints[4]);
    gtk_widget_set_sensitive(p->entries[IEM_HEIGHT], r->sr_floats[3] > 0);
    if (!strcmp(object_name, "hradio") || !strcmp(object_name, "vradio") ||
        !strcmp(object_name, "vu")) {
        gtk_widget_set_sensitive(p->entries[IEM_RANGE_MIN], FALSE);
        gtk_widget_set_sensitive(p->entries[IEM_RANGE_MAX], FALSE);
    } else if (!strcmp(object_name, "tgl")) {
        gtk_widget_set_sensitive(p->entries[IEM_RANGE_MAX], FALSE);
    }
    checks = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    p->loadinit = gtk_check_button_new_with_label("Initialize");
    p->steady = gtk_check_button_new_with_label("Steady on click");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(p->loadinit), r->sr_ints[2] > 0);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(p->steady), r->sr_ints[3] > 0);
    gtk_widget_set_sensitive(p->loadinit, p->has_loadinit);
    gtk_widget_set_sensitive(p->mode, p->has_mode);
    gtk_widget_set_sensitive(p->entries[IEM_NUMBER], r->sr_ints[4] >= 0);
    gtk_widget_set_sensitive(p->steady, p->has_steady);
    gtk_box_append(GTK_BOX(checks), p->loadinit);
    gtk_box_append(GTK_BOX(checks), p->steady);
    gtk_box_append(GTK_BOX(parameters), checks);

    messages = gtk_properties_frame(content, "Messages");
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_box_append(GTK_BOX(messages), grid);
    p->entries[IEM_SEND] =
        gtk_properties_text_entry(grid, 0, 0, "Send symbol", r->sr_strings[3], 12);
    p->entries[IEM_RECEIVE] =
        gtk_properties_text_entry(grid, 0, 2, "Receive symbol", r->sr_strings[4], 12);

    label_box = gtk_properties_frame(content, "Label");
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_box_append(GTK_BOX(label_box), grid);
    p->entries[IEM_LABEL] = gtk_properties_text_entry(grid, 0, 0, "Label", r->sr_strings[5], 12);
    p->entries[IEM_LABEL_X] =
        gtk_properties_number_entry(grid, 1, 0, "Label X offset", r->sr_ints[5]);
    p->entries[IEM_LABEL_Y] =
        gtk_properties_number_entry(grid, 1, 2, "Label Y offset", r->sr_ints[6]);
    p->font = gtk_properties_dropdown(grid, 2, 0, "Label font", fonts,
                                      r->sr_ints[7] >= 0 && r->sr_ints[7] <= 2 ? r->sr_ints[7] : 0);
    p->entries[IEM_FONT_SIZE] =
        gtk_properties_number_entry(grid, 2, 2, "Label font size", r->sr_ints[8]);

    colors_box = gtk_properties_frame(content, "Colors");
    colors = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_widget_set_halign(colors, GTK_ALIGN_CENTER);
    p->colors[0] = gtk_properties_color(colors, "Background", (unsigned int)r->sr_ints[9]);
    p->colors[1] = gtk_properties_color(colors, "Foreground", (unsigned int)r->sr_ints[10]);
    p->colors[2] = gtk_properties_color(colors, "Label", (unsigned int)r->sr_ints[11]);
    if (!strcmp(object_name, "vu") || !strcmp(object_name, "cnv")) {
        gtk_widget_set_sensitive(p->colors[1], FALSE);
    }
    gtk_box_append(GTK_BOX(colors_box), colors);
    g_signal_connect(p->window, "response", G_CALLBACK(gtk_iemgui_properties_response), p);
    g_signal_connect(p->window, "destroy", G_CALLBACK(gtk_iemgui_properties_destroy), p);
    p->next = gtk_iemgui_properties_list;
    gtk_iemgui_properties_list = p;
    gtk_window_present(GTK_WINDOW(p->window));
    g_free(height_label);
    g_free(width_label);
    g_free(title);
    g_free(object_name);
}

static void gtk_properties_destroy_named(const char *name) {
    t_gtk_canvas_properties *p;
    t_gtk_iemgui_properties *iem;
    for (p = gtk_canvas_properties_list; p; p = p->next) {
        if (!strcmp(pdgui_dialog_stub_name(p->stub), name)) {
            gtk_window_destroy(GTK_WINDOW(p->window));
            return;
        }
    }
    for (iem = gtk_iemgui_properties_list; iem; iem = iem->next) {
        if (!strcmp(pdgui_dialog_stub_name(iem->stub), name)) {
            gtk_window_destroy(GTK_WINDOW(iem->window));
            return;
        }
    }
}

static void gtk_confirm_send(t_gtk_confirm *confirm) {
    t_pd *receiver;
    if (!confirm->receiver || confirm->argc < 1 || confirm->atoms[0].a_type != A_SYMBOL) {
        gtk_pd_message("quit", 0, NULL);
        return;
    }
    receiver = confirm->receiver->s_thing;
    if (receiver) {
        pd_typedmess(receiver, atom_getsymbol(confirm->atoms), confirm->argc - 1,
                     confirm->atoms + 1);
    }
}

static void gtk_confirm_response(GtkDialog *dialog, int response, gpointer data) {
    t_gtk_confirm *confirm = (t_gtk_confirm *)data;
    if (confirm->save_discard_cancel && response == GTK_RESPONSE_YES && confirm->canvas &&
        gc_find(confirm->canvas)) {
        canvas_message_float(gc_find(confirm->canvas), "menusave", 1);
    } else if (response == GTK_RESPONSE_YES || response == GTK_RESPONSE_NO) {
        if (!confirm->save_discard_cancel || response == GTK_RESPONSE_NO) {
            gtk_confirm_send(confirm);
        }
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
    g_free(confirm->atoms);
    g_free(confirm);
}

static char *gtk_confirm_message(const t_pdgui_service_request *r) {
    if (r->sr_string_counts[0] > 1) {
        return (g_strdup_printf(r->sr_string_arrays[0][0], r->sr_string_arrays[0][1]));
    }
    if (r->sr_string_counts[0] == 1) {
        return (g_strdup(r->sr_string_arrays[0][0]));
    }
    return (g_strdup(r->sr_strings[0] ? r->sr_strings[0] : "Continue?"));
}

static void gtk_confirm_open(const t_pdgui_service_request *r, int save_discard_cancel) {
    t_gc *c = r->sr_canvas ? gc_find(r->sr_canvas) : NULL;
    GtkWidget *dialog;
    char *message = save_discard_cancel && r->sr_canvas
                        ? g_strdup_printf("Do you want to save the changes you made in '%s'?",
                                          r->sr_canvas->gl_name->s_name)
                        : gtk_confirm_message(r);
    t_gtk_confirm *confirm = g_new0(t_gtk_confirm, 1);
    dialog = gtk_message_dialog_new(gtk_menu_parent(c), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_NONE, "%s", message);
    if (save_discard_cancel) {
        gtk_dialog_add_buttons(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_CANCEL, "_Discard",
                               GTK_RESPONSE_NO, "_Save", GTK_RESPONSE_YES, NULL);
    } else {
        gtk_dialog_add_buttons(GTK_DIALOG(dialog), "_No", GTK_RESPONSE_CANCEL, "_Yes",
                               GTK_RESPONSE_YES, NULL);
    }
    confirm->canvas = r->sr_canvas;
    confirm->receiver = r->sr_symbol;
    confirm->argc = r->sr_natoms;
    confirm->save_discard_cancel = save_discard_cancel;
    if (confirm->argc) {
        confirm->atoms = (t_atom *)g_memdup2(r->sr_atoms, confirm->argc * sizeof(*confirm->atoms));
    }
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_confirm_response), confirm);
    gtk_window_present(GTK_WINDOW(dialog));
    g_free(message);
}

static void service(t_pdgui_service s, const t_pdgui_service_request *r) {
    t_gc *c;
    switch (s) {
    case PDGUI_SERVICE_WINDOW_MENU_UPDATE:
        gtk_update_window_menu();
        break;
    case PDGUI_SERVICE_DSP_STATE:
        gtk_dsp_state = (r->sr_ints[0] != 0);
        if (gtk_dsp_switch) {
            gtk_dsp_sync = 1;
            gtk_switch_set_active(GTK_SWITCH(gtk_dsp_switch), gtk_dsp_state);
            gtk_dsp_sync = 0;
        }
        gtk_set_boolean_action_all("dsp", gtk_dsp_state);
        break;
    case PDGUI_SERVICE_CONSOLE_POST:
        gtk_console_append(r->sr_strings[0], PD_NORMAL);
        break;
    case PDGUI_SERVICE_CONSOLE_LOG:
        gtk_console_append(r->sr_strings[0], r->sr_ints[0]);
        break;
    case PDGUI_SERVICE_PREFERENCES_PATHS:
        gtk_preferences_strings_set(&gtk_preferences_data.paths,
            r->sr_string_counts[0], r->sr_string_arrays[0]);
        gtk_preferences_strings_set(&gtk_preferences_data.temporary_paths,
            r->sr_string_counts[1], r->sr_string_arrays[1]);
        gtk_preferences_strings_set(&gtk_preferences_data.static_paths,
            r->sr_string_counts[2], r->sr_string_arrays[2]);
        break;
    case PDGUI_SERVICE_PREFERENCES_FLAGS:
        gtk_preferences_strings_set(&gtk_preferences_data.libraries,
            r->sr_string_counts[0], r->sr_string_arrays[0]);
        gtk_preferences_data.verbose = r->sr_ints[0];
        gtk_preferences_data.use_standard_path = r->sr_ints[1];
        gtk_preferences_data.defeat_realtime = r->sr_ints[2];
        gtk_preferences_data.zoom_open = r->sr_ints[3];
        g_free(gtk_preferences_data.flags);
        gtk_preferences_data.flags = g_strdup(r->sr_strings[0] ?
                                               r->sr_strings[0] : "");
        break;
    case PDGUI_SERVICE_AUDIO_CONFIG:
        gtk_preferences_strings_set(&gtk_preferences_data.audio_inputs,
            r->sr_string_counts[0], r->sr_string_arrays[0]);
        gtk_preferences_strings_set(&gtk_preferences_data.audio_outputs,
            r->sr_string_counts[1], r->sr_string_arrays[1]);
        gtk_preferences_floats_set(gtk_preferences_data.audio_input_used,
            GTK_PREF_AUDIO_DEVICES, r->sr_float_counts[0], r->sr_float_arrays[0]);
        gtk_preferences_floats_set(gtk_preferences_data.audio_input_channels,
            GTK_PREF_AUDIO_DEVICES, r->sr_float_counts[1], r->sr_float_arrays[1]);
        gtk_preferences_floats_set(gtk_preferences_data.audio_output_used,
            GTK_PREF_AUDIO_DEVICES, r->sr_float_counts[2], r->sr_float_arrays[2]);
        gtk_preferences_floats_set(gtk_preferences_data.audio_output_channels,
            GTK_PREF_AUDIO_DEVICES, r->sr_float_counts[3], r->sr_float_arrays[3]);
        g_free(gtk_preferences_data.sample_rate);
        g_free(gtk_preferences_data.block_size);
        g_free(gtk_preferences_data.callback);
        gtk_preferences_data.sample_rate = g_strdup(r->sr_strings[0]);
        gtk_preferences_data.block_size = g_strdup(r->sr_strings[1]);
        gtk_preferences_data.callback = g_strdup(r->sr_strings[2]);
        gtk_preferences_data.advance = r->sr_ints[0];
        gtk_preferences_data.multi = r->sr_ints[1];
        break;
    case PDGUI_SERVICE_MIDI_CONFIG:
        gtk_preferences_strings_set(&gtk_preferences_data.midi_inputs,
            r->sr_string_counts[0], r->sr_string_arrays[0]);
        gtk_preferences_strings_set(&gtk_preferences_data.midi_outputs,
            r->sr_string_counts[1], r->sr_string_arrays[1]);
        gtk_preferences_floats_set(gtk_preferences_data.midi_input_used,
            GTK_PREF_MIDI_DEVICES, r->sr_float_counts[0], r->sr_float_arrays[0]);
        gtk_preferences_floats_set(gtk_preferences_data.midi_output_used,
            GTK_PREF_MIDI_DEVICES, r->sr_float_counts[1], r->sr_float_arrays[1]);
        break;
    case PDGUI_SERVICE_PREFERENCES_OPEN:
        gtk_preferences_open();
        break;
    case PDGUI_SERVICE_AUDIO_REFRESH:
    case PDGUI_SERVICE_MIDI_REFRESH:
        if (gtk_preferences.window) {
            const char *page = s == PDGUI_SERVICE_AUDIO_REFRESH ? "audio" : "midi";
            gtk_window_destroy(GTK_WINDOW(gtk_preferences.window));
            gtk_preferences_show_page(page);
        }
        break;
    case PDGUI_SERVICE_AUDIO_DIALOG_OPEN:
        gtk_preferences_show_page("audio");
        break;
    case PDGUI_SERVICE_MIDI_DIALOG_OPEN:
        gtk_preferences_show_page("midi");
        break;
    case PDGUI_SERVICE_PATH_DIALOG_OPEN:
        gtk_preferences_show_page("paths");
        break;
    case PDGUI_SERVICE_STARTUP_DIALOG_OPEN:
        gtk_preferences_show_page("startup");
        break;
    case PDGUI_SERVICE_CLIPBOARD_SET:
        gtk_clipboard_set(r->sr_strings[0], r->sr_ints[0]);
        break;
    case PDGUI_SERVICE_UNDO_MENU:
        /* Actions already dispatch to Pd; dynamic menu labels come later. */
        break;
    case PDGUI_SERVICE_AUDIO_RUNNING:
        if (gtk_audio_label) {
            gtk_label_set_text(GTK_LABEL(gtk_audio_label), "DSP");
            gtk_widget_set_tooltip_text(gtk_dsp_switch,
                                        r->sr_ints[0] ? "Audio is running" : "Audio is stopped");
        }
        break;
    case PDGUI_SERVICE_DIO_STATE:
        if (gtk_audio_label && r->sr_ints[0]) {
            gtk_label_set_text(GTK_LABEL(gtk_audio_label), "DSP error");
        }
        break;
    case PDGUI_SERVICE_CANVAS_CREATE:
        gtk_canvas_new(r->sr_canvas, r->sr_ints[0], r->sr_ints[1],
                       r->sr_ints[5] ? (unsigned int)r->sr_ints[3] : 0xFFFFFF,
                       r->sr_ints[5] ? (unsigned int)r->sr_ints[4] : 0x000000, r->sr_ints[2]);
        break;
    case PDGUI_SERVICE_CANVAS_TITLE: {
        char b[MAXPDSTRING];
        c = gc_find(r->sr_canvas);
        if (c) {
            pd_snprintf(b, sizeof(b), "%s%s - %s", r->sr_ints[0] ? "*" : "", r->sr_strings[2],
                        r->sr_strings[0]);
            gtk_window_set_title(GTK_WINDOW(c->window), b);
            g_free(c->title);
            c->title = g_strdup(b);
            gtk_update_window_menu();
        }
        break;
    }
    case PDGUI_SERVICE_CANVAS_RAISE:
        c = gc_find(r->sr_canvas);
        if (c) {
            gtk_window_present(GTK_WINDOW(c->window));
        }
        break;
    case PDGUI_SERVICE_WINDOW_DESTROY:
        if (r->sr_strings[0]) {
            gtk_properties_destroy_named(r->sr_strings[0]);
        } else if (r->sr_object && gc_find((t_canvas *)r->sr_object)) {
            gtk_canvas_free((t_canvas *)r->sr_object);
        }
        break;
    case PDGUI_SERVICE_CANVAS_DIALOG_TEXT:
        g_free(gtk_canvas_dialog_text);
        gtk_canvas_dialog_text = g_strdup(r->sr_strings[0] ? r->sr_strings[0] : "");
        break;
    case PDGUI_SERVICE_CANVAS_CURSOR: {
        static const char *names[8] = {"default", "pointer",   "crosshair",   "copy",
                                       "default", "crosshair", "not-allowed", "nwse-resize"};
        c = gc_find(r->sr_canvas);
        if (c && r->sr_ints[0] >= 0 && r->sr_ints[0] < 8) {
            gtk_widget_set_cursor_from_name(c->area, names[r->sr_ints[0]]);
        }
        break;
    }
    case PDGUI_SERVICE_CANVAS_EDITMODE:
        c = gc_find(r->sr_canvas);
        if (c) {
            c->editmode = (r->sr_ints[0] != 0);
            gtk_set_boolean_action(c->window, "edit-mode", c->editmode);
        }
        break;
    case PDGUI_SERVICE_CANVAS_POPUP:
        c = gc_find(r->sr_canvas);
        if (c) {
            canvas_popup(c, r->sr_ints[0], r->sr_ints[1], r->sr_ints[2], r->sr_ints[3]);
        }
        break;
    case PDGUI_SERVICE_CANVAS_PASTE:
        gtk_canvas_paste(r->sr_canvas, r->sr_ints[0]);
        break;
    case PDGUI_SERVICE_CONFIRM:
        gtk_confirm_open(r, 0);
        break;
    case PDGUI_SERVICE_CANVAS_CLOSE_CONFIRM:
        gtk_confirm_open(r, 1);
        break;
    case PDGUI_SERVICE_CANVAS_SCROLL:
    case PDGUI_SERVICE_CANVAS_PARENTS:
    case PDGUI_SERVICE_POINTER_POSITION:
        break;
    case PDGUI_SERVICE_CANVAS_EXPORT:
        c = gc_find(r->sr_canvas);
        if (c && r->sr_strings[0]) {
            int width = gtk_widget_get_width(c->area);
            int height = gtk_widget_get_height(c->area);
            cairo_surface_t *surface = cairo_ps_surface_create(
                r->sr_strings[0], width > 0 ? width : 600, height > 0 ? height : 400);
            cairo_t *cr = cairo_create(surface);
            draw(GTK_DRAWING_AREA(c->area), cr, width, height, c);
            cairo_show_page(cr);
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
        }
        break;
    case PDGUI_SERVICE_CANVAS_SAVE_AS:
        c = gc_find(r->sr_canvas);
        if (c) {
            gtk_choose_file(c, GTK_CHOOSE_SAVE_CANVAS, r->sr_canvas, r->sr_strings[0],
                            r->sr_strings[1], r->sr_ints[0]);
        }
        break;
    case PDGUI_SERVICE_CANVAS_DIALOG:
        gtk_canvas_properties_open(r);
        break;
    case PDGUI_SERVICE_IEMGUI_DIALOG:
        gtk_iemgui_properties_open(r);
        break;
    case PDGUI_SERVICE_AUDIO_API:
        gtk_audio_api = r->sr_ints[0];
        gtk_set_integer_action_all("audio-api", gtk_audio_api);
        break;
    case PDGUI_SERVICE_MIDI_API:
        gtk_set_integer_action_all("midi-api", r->sr_ints[0]);
        break;
    case PDGUI_SERVICE_WATCHDOG:
        if (!gtk_watchdog_source) {
            glob_watchdog(0);
            gtk_watchdog_source = g_timeout_add_seconds(2, gtk_watchdog_tick, NULL);
        }
        break;
    case PDGUI_SERVICE_EXIT:
        gtk_exiting = 1;
        if (gtk_watchdog_source) {
            g_source_remove(gtk_watchdog_source);
            gtk_watchdog_source = 0;
        }
        if (gtk_preferences.window) {
            gtk_window_destroy(GTK_WINDOW(gtk_preferences.window));
        }
        while (gtk_canvas_properties_list) {
            gtk_window_destroy(GTK_WINDOW(gtk_canvas_properties_list->window));
        }
        while (gtk_iemgui_properties_list) {
            gtk_window_destroy(GTK_WINDOW(gtk_iemgui_properties_list->window));
        }
        g_clear_pointer(&gtk_canvas_dialog_text, g_free);
        while (gc_list) {
            gtk_canvas_free(gc_list->canvas);
        }
        if (gtk_main_window) {
            gtk_window_destroy(GTK_WINDOW(gtk_main_window));
            gtk_main_window = 0;
            gtk_console_view = 0;
            gtk_console_buffer = 0;
            gtk_dsp_switch = 0;
            gtk_audio_label = 0;
        }
        break;
    default:
        pd_error(NULL, "Something %d: not implemented on GTK yet", s);
        break;
    }
}

const t_pdgui_backend pdgui_gtk_backend = {rect_create,
                                           shape_update,
                                           oval_create,
                                           shape_update,
                                           line_create,
                                           line_update,
                                           poly_create,
                                           poly_update,
                                           path_create,
                                           path_points,
                                           text_create,
                                           text_plain,
                                           text_grouped,
                                           text_anchor,
                                           text_update,
                                           canvas_text,
                                           canvas_text_group,
                                           canvas_text_label,
                                           selection,
                                           editing,
                                           item_destroy,
                                           item_move,
                                           item_order,
                                           item_style,
                                           canvas_clear,
                                           canvas_colors,
                                           cords,
                                           patchcord,
                                           service,
                                           pdgui_gtk_backend_poll,
                                           pdgui_gtk_backend_init};
