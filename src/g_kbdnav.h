#ifndef HAVE_KEYBOARDNAV
        #define HAVE_KEYBOARDNAV 1
#endif

#ifndef G_KBDNAV_H
#define G_KBDNAV_H

typedef struct _kbdnav
{
    int kn_state;        /* see KN_ consts below */
    int kn_ioindex;      /* selected inlet/outlet, 0-based*/
    int kn_connindex;    /* selected in/out connection, 0-based */
    t_outconnect *kn_outconnect;
    t_object *kn_selobj; /* currently selected object */
    char* kn_highlight;  /* highlight_color */
    struct _linetraverser *kn_linetraverser;
    struct _magtrav *kn_magtrav; /* linked list of connections for the magnetic connector*/
    int kn_chosennumber; /* used by the digit connector */
    int kn_iotype;       /* IO_INLET/IO_OUTLET */
    int kn_indexvis;     /* boolean: should we visualize object indexes? */
} t_kbdnav;

/* #define constants */

#define KN_INACTIVE 0 /* not navigating with the keyboard */
#define KN_IO_SELECTED 1 /* selecting an inlet/outlet */
#define KN_CONN_SELECTED 2 /* navigating through an in/outlet's connections */
#define KN_WAITING_NUMBER 3 /* waiting for a number from 0-9 when using the "digit connector" */

#define KBDNAV_SEL_PAD_X 4
#define KBDNAV_SEL_PAD_Y 3

#define IO_INLET 0
#define IO_OUTLET 1

/* stuff from g_editor.c */

EXTERN void canvas_reselect(t_canvas *x);
EXTERN void canvas_clearline(t_canvas *x);


/* ------- functions on glists related to keyboard navigation  ------- */

EXTERN t_kbdnav* kbdnav_new();
EXTERN t_kbdnav* canvas_get_kbdnav(t_canvas *x);
EXTERN void kbdnav_free(t_kbdnav *x);
EXTERN t_object *kbdnav_get_selected_obj(t_canvas *x);
EXTERN void kbdnav_up(t_canvas *x, int shift);
EXTERN void kbdnav_down(t_canvas *x, int shift);
EXTERN void kbdnav_left(t_canvas *x, int shift);
EXTERN void kbdnav_right(t_canvas *x, int shift);
EXTERN int kbdnav_count_selected_objects(t_canvas *x);
EXTERN void kbdnav_activate_single_selected_object(t_canvas *x, int io_type);
EXTERN void kbdnav_activate_object(t_canvas *x, t_object *obj, int io_type);
EXTERN void kbdnav_activate_kbdnav(t_canvas *x, int inlet_or_outlet);
EXTERN void kbdnav_deactivate(t_canvas *x);
EXTERN int kbdnav_noutconnections(const t_object *x, int outlet_index);
EXTERN int kbdnav_number_of_inlet_sources(t_canvas *x, t_object *obj, int inlet_index);
EXTERN void kbdnav_traverse_outconnections_start(t_canvas *x);
EXTERN void kbdnav_traverse_outconnections_next(t_canvas *x);
EXTERN void kbdnav_traverse_outconnections_prev(t_canvas *x);
EXTERN void kbdnav_traverse_inlet_sources_start(t_canvas *x);
EXTERN void kbdnav_traverse_inlet_sources_next(t_canvas *x);
EXTERN void kbdnav_traverse_inlet_sources_prev(t_canvas *x);
EXTERN void kbdnav_displayindices(t_canvas *x);
EXTERN void kbdnav_set_indices_visibility(t_canvas *x, t_floatarg indexarg);
EXTERN void kbdnav_debug(t_canvas *x);
EXTERN void kbdnav_magnetic_connect(t_canvas *x, int shift);
EXTERN void kbdnav_magnetic_disconnect(t_canvas *x);
EXTERN void kbdnav_magnetic_connect_start(t_canvas *x);
EXTERN void kbdnav_magnetic_connect_next(t_canvas *x);
EXTERN void kbdnav_magnetic_connect_free(t_canvas *x);
EXTERN void kbdnav_magnetic_connect_draw_numbers(t_canvas *x);
EXTERN void kbdnav_digitconnect_choose(t_canvas *x, int exit_after_connecting);
EXTERN void kbdnav_delete_connection(t_canvas *x);
EXTERN void kbdnav_digit_connect_display_numbers(t_canvas *x);
EXTERN void canvas_goto(t_canvas *x, t_floatarg indexarg);
EXTERN void kbdnav_displaceselection(t_canvas *x, int dx, int dy, t_selection *sel);
EXTERN int kbdnav_howputnew(t_canvas *x, t_gobj *g, int nobj, int *xpixp, int *ypixp,
    int x1, int y1, int x2, int y2);
EXTERN int kbdnav_connect_new(t_glist *gl, int nobj, int indx);

/* g_kbdnav.c */
EXTERN void kbdnav_glist_drawiofor(t_glist *glist, t_object *ob, int firsttime, char *tag,
                                        int x1, int y1, int x2, int y2);
EXTERN void kbdnav_canvas_drawlines(t_canvas *x);

/* g_text.c */
EXTERN void kbdnav_io_pos(t_glist *glist, t_object *ob, int index, int iotype,
                                int *iox, int *ioy, int *iowidth, int *ioheight);

/* ---functions related to iemgui--- */

/* [hslider] */

struct _hslider;
typedef struct _hslider t_hslider;

EXTERN void kbdnav_hslider_draw_io_selection(t_hslider* x, t_glist* glist, int xpos, int ypos, int lmargin, int zoom, int iow, int ioh);
EXTERN void kbdnav_hslider_move(t_hslider* x, t_glist* glist, int xpos, int ypos, int lmargin, int zoom, int iow, int ioh);

/* [vslider] */

struct _vslider;
typedef struct _vslider t_vslider;

EXTERN void kbdnav_vslider_draw_io_selection(t_vslider *x, t_glist *glist, int xpos, int ypos, int tmargin, int bmargin, int zoom, int iow, int ioh);
EXTERN void kbdnav_vslider_move(t_vslider* x, t_glist* glist, int xpos, int ypos, int tmargin, int bmargin, int zoom, int iow, int ioh);

/* [bng] */

struct _bng;
typedef struct _bng t_bng;

EXTERN void kbdnav_bng_draw_io_selection(t_bng *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh);
EXTERN void kbdnav_bng_move(t_bng *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh);

/* [tgl] */

struct _toggle;
typedef struct _toggle t_toggle;

EXTERN void kbdnav_toggle_draw_io_selection(t_toggle *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh);
EXTERN void kbdnav_toggle_move(t_toggle *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh);

/* [vradio] */

struct _vdial;
typedef struct _vdial t_vradio;

EXTERN void kbdnav_vradio_draw_io_selection(t_vradio *x, t_glist *glist, int xx11, int yy11, int yy11b, int zoom, int iow, int ioh);
EXTERN void kbdnav_vradio_move(t_vradio *x, t_glist *glist, int xx11, int yy11, int yy11b, int zoom, int iow, int ioh);

/* [hradio] */

struct _hdial;
typedef struct _hdial t_hradio;

EXTERN void kbdnav_hradio_draw_io_selection(t_hradio *x, t_glist *glist, int xx11b, int yy12, int yy11, int zoom, int iow, int ioh);
EXTERN void kbdnav_hradio_move(t_hradio *x, t_glist *glist, int xx11b, int yy12, int yy11, int zoom, int iow, int ioh);

/* [vu meter] */

struct _vu;
typedef struct _vu t_vu;

EXTERN void kbdnav_vu_draw_io_selection(t_vu *x, t_glist *glist, int xpos, int ypos, int hmargin, int vmargin, int zoom, int iow, int ioh);
EXTERN void kbdnav_vu_move(t_vu *x, t_glist *glist, int xpos, int ypos, int hmargin, int vmargin, int zoom, int iow, int ioh);

/* [my numbox] */

struct _my_numbox;
typedef struct _my_numbox t_my_numbox;

EXTERN void kbdnav_my_numbox_draw_io_selection(t_my_numbox *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh);
EXTERN void kbdnav_my_numbox_move(t_my_numbox *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh);
EXTERN void kbdnav_register(t_class *canvas_class);

#endif