/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* this file defines the structure for "glists" and related structures and
functions.  "Glists" and "canvases" and "graphs" used to be different
structures until being unified in version 0.35.

A glist occupies its own window if the "gl_havewindow" flag is set.  Its
appearance on its "parent", also called "owner", (if it has one) is as a graph
if "gl_isgraph" is set, and otherwise as a text box.

A glist is "root" if it has no owner, i.e., a document window.  In this
case "gl_havewindow" is always set.

We maintain a list of root windows, so that we can traverse the whole
collection of everything in a Pd process.

If a glist has a window it may still not be "mapped."  Miniaturized
windows aren't mapped, for example, but a window is also not mapped
immediately upon creation.  In either case gl_havewindow is true but
gl_mapped is false.

Closing a non-root window makes it invisible; closing a root destroys it.

A glist that's just a text object on its parent is always "toplevel."  An
embedded glist can switch back and forth to appear as a toplevel by double-
clicking on it.  Single-clicking a text box makes the toplevel become visible
and raises the window it's in.

If a glist shows up as a graph on its parent, the graph is blanked while the
glist has its own window, even if miniaturized.

*/

/* NOTE: this file describes Pd implementation details which may change
in future releases.  The public (stable) API is in m_pd.h. */  

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus)
extern "C" {
#endif

/* --------------------- geometry ---------------------------- */
#define IOWIDTH 7       /* width of an inlet/outlet in pixels */
#define IOMIDDLE ((IOWIDTH-1)/2)
#define GLIST_DEFGRAPHWIDTH 200
#define GLIST_DEFGRAPHHEIGHT 140
/* ----------------------- data ------------------------------- */

typedef struct _updateheader
{
    struct _updateheader *upd_next;
    unsigned int upd_array:1;       /* true if array, false if glist */
    unsigned int upd_queued:1;      /* true if we're queued */
} t_updateheader;

    /* types to support glists grabbing mouse motion or keys from parent */
typedef void (*t_glistmotionfn)(void *z, t_floatarg dx, t_floatarg dy);
typedef void (*t_glistkeyfn)(void *z, t_floatarg key);

EXTERN_STRUCT _rtext;
#define t_rtext struct _rtext

EXTERN_STRUCT _gtemplate;
#define t_gtemplate struct _gtemplate

EXTERN_STRUCT _guiconnect;
#define t_guiconnect struct _guiconnect

EXTERN_STRUCT _tscalar;
#define t_tscalar struct _tscalar

EXTERN_STRUCT _canvasenvironment;
#define t_canvasenvironment struct _canvasenvironment 

EXTERN_STRUCT _fielddesc;
#define t_fielddesc struct _fielddesc

typedef struct _selection
{
    t_gobj *sel_what;
    struct _selection *sel_next;
} t_selection;

    /* this structure is instantiated whenever a glist becomes visible. */
typedef struct _editor
{
    t_updateheader e_upd;           /* update header structure */
    t_selection *e_updlist;         /* list of objects to update */
    t_rtext *e_rtext;               /* text responder linked list */
    t_selection *e_selection;       /* head of the selection list */
    t_rtext *e_textedfor;           /* the rtext if any that we are editing */
    t_gobj *e_grab;                 /* object being "dragged" */
    t_glistmotionfn e_motionfn;     /* ... motion callback */
    t_glistkeyfn e_keyfn;           /* ... keypress callback */
    t_binbuf *e_connectbuf;         /* connections to deleted objects */
    t_binbuf *e_deleted;            /* last stuff we deleted */
    t_guiconnect *e_guiconnect;     /* GUI connection for filtering messages */
    struct _glist *e_glist;         /* glist which owns this */
    int e_xwas;                     /* xpos on last mousedown or motion event */
    int e_ywas;                     /* ypos, similarly */
    int e_selectline_index1;        /* indices for the selected line if any */
    int e_selectline_outno;         /* (only valid if e_selectedline is set) */
    int e_selectline_index2;
    int e_selectline_inno;
    t_outconnect *e_selectline_tag;
    unsigned int e_onmotion: 3;     /* action to take on motion */
    unsigned int e_lastmoved: 1;    /* one if mouse has moved since click */
    unsigned int e_textdirty: 1;    /* one if e_textedfor has changed */
    unsigned int e_selectedline: 1; /* one if a line is selected */
    t_clock *e_clock;               /* clock to filter GUI move messages */
    int e_xnew;                     /* xpos for next move event */
    int e_ynew;                     /* ypos, similarly */
} t_editor;

#define MA_NONE    0    /* e_onmotion: do nothing on mouse motion */
#define MA_MOVE    1    /* drag the selection around */
#define MA_CONNECT 2    /* make a connection */
#define MA_REGION  3    /* selection region */
#define MA_PASSOUT 4    /* send on to e_grab */
#define MA_DRAGTEXT 5   /* drag in text editor to alter selection */

/* editor structure for "garrays".  We don't bother to delete and regenerate
this structure when the "garray" becomes invisible or visible, although we
could do so if the structure gets big (like the "editor" above.) */
    
typedef struct _arrayvis
{
    t_updateheader av_upd;          /* update header structure */
    t_garray *av_garray;            /* owning structure */    
} t_arrayvis;

/* the t_tick structure describes where to draw x and y "ticks" for a glist */

typedef struct _tick    /* where to put ticks on x or y axes */
{
    t_float k_point;      /* one point to draw a big tick at */
    t_float k_inc;        /* x or y increment per little tick */
    int k_lperb;        /* little ticks per big; 0 if no ticks to draw */
} t_tick;

/* the t_glist structure, which describes a list of elements that live on an
area of a window.

*/

struct _glist
{  
    t_object gl_obj;            /* header in case we're a glist */
    t_gobj *gl_list;            /* the actual data */
    struct _gstub *gl_stub;     /* safe pointer handler */
    int gl_valid;               /* incremented when pointers might be stale */
    struct _glist *gl_owner;    /* parent glist, supercanvas, or 0 if none */
    int gl_pixwidth;            /* width in pixels (on parent, if a graph) */
    int gl_pixheight;
    t_float gl_x1;                /* bounding rectangle in our own coordinates */
    t_float gl_y1;
    t_float gl_x2;
    t_float gl_y2;
    int gl_screenx1;            /* screen coordinates when toplevel */
    int gl_screeny1;
    int gl_screenx2;
    int gl_screeny2;
    int gl_xmargin;                /* origin for GOP rectangle */
    int gl_ymargin;
    t_tick gl_xtick;            /* ticks marking X values */    
    int gl_nxlabels;            /* number of X coordinate labels */
    t_symbol **gl_xlabel;           /* ... an array to hold them */
    t_float gl_xlabely;               /* ... and their Y coordinates */
    t_tick gl_ytick;            /* same as above for Y ticks and labels */
    int gl_nylabels;
    t_symbol **gl_ylabel;
    t_float gl_ylabelx;
    t_editor *gl_editor;        /* editor structure when visible */
    t_symbol *gl_name;          /* symbol bound here */
    int gl_font;                /* nominal font size in points, e.g., 10 */
    struct _glist *gl_next;         /* link in list of toplevels */
    t_canvasenvironment *gl_env;    /* root canvases and abstractions only */
    unsigned int gl_havewindow:1;   /* true if we own a window */
    unsigned int gl_mapped:1;       /* true if, moreover, it's "mapped" */
    unsigned int gl_dirty:1;        /* (root canvas only:) patch has changed */
    unsigned int gl_loading:1;      /* am now loading from file */
    unsigned int gl_willvis:1;      /* make me visible after loading */ 
    unsigned int gl_edit:1;         /* edit mode */
    unsigned int gl_isdeleting:1;   /* we're inside glist_delete -- hack! */
    unsigned int gl_goprect:1;      /* draw rectangle for graph-on-parent */
    unsigned int gl_isgraph:1;      /* show as graph on parent */
    unsigned int gl_hidetext:1;     /* hide object-name + args when doing graph on parent */
};

#define gl_gobj gl_obj.te_g
#define gl_pd gl_gobj.g_pd

/* a data structure to describe a field in a pure datum */

#define DT_FLOAT 0
#define DT_SYMBOL 1
#define DT_LIST 2
#define DT_ARRAY 3

typedef struct _dataslot
{
    int ds_type;
    t_symbol *ds_name;
    t_symbol *ds_arraytemplate;     /* filled in for arrays only */
} t_dataslot;

typedef struct _template
{
    t_pd t_pdobj;               /* header */
    struct _gtemplate *t_list;  /* list of "struct"/gtemplate objects */
    t_symbol *t_sym;            /* name */
    int t_n;                    /* number of dataslots (fields) */
    t_dataslot *t_vec;          /* array of dataslots */
} t_template;

struct _array
{
    int a_n;            /* number of elements */
    int a_elemsize;     /* size in bytes; LATER get this from template */
    char *a_vec;        /* array of elements */
    t_symbol *a_templatesym;    /* template for elements */
    int a_valid;        /* protection against stale pointers into array */
    t_gpointer a_gp;    /* pointer to scalar or array element we're in */
    t_gstub *a_stub;    /* stub for pointing into this array */
};

    /* structure for traversing all the connections in a glist */
typedef struct _linetraverser
{
    t_canvas *tr_x;
    t_object *tr_ob;
    int tr_nout;
    int tr_outno;
    t_object *tr_ob2;
    t_outlet *tr_outlet;
    t_inlet *tr_inlet;
    int tr_nin;
    int tr_inno;
    int tr_x11, tr_y11, tr_x12, tr_y12;
    int tr_x21, tr_y21, tr_x22, tr_y22;
    int tr_lx1, tr_ly1, tr_lx2, tr_ly2;
    t_outconnect *tr_nextoc;
    int tr_nextoutno;
} t_linetraverser;

/* function types used to define graphical behavior for gobjs, a bit like X
widgets.  We don't use Pd methods because Pd's typechecking can't specify the
types of pointer arguments.  Also it's more convenient this way, since
every "patchable" object can just get the "text" behaviors. */

        /* Call this to get a gobj's bounding rectangle in pixels */
typedef void (*t_getrectfn)(t_gobj *x, struct _glist *glist,
    int *x1, int *y1, int *x2, int *y2);
        /* and this to displace a gobj: */
typedef void (*t_displacefn)(t_gobj *x, struct _glist *glist, int dx, int dy);
        /* change color to show selection: */
typedef void (*t_selectfn)(t_gobj *x, struct _glist *glist, int state);
        /* change appearance to show activation/deactivation: */
typedef void (*t_activatefn)(t_gobj *x, struct _glist *glist, int state);
        /* warn a gobj it's about to be deleted */
typedef void (*t_deletefn)(t_gobj *x, struct _glist *glist);
        /*  making visible or invisible */
typedef void (*t_visfn)(t_gobj *x, struct _glist *glist, int flag);
        /* field a mouse click (when not in "edit" mode) */
typedef int (*t_clickfn)(t_gobj *x, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit);
        /* ... and later, resizing; getting/setting font or color... */

struct _widgetbehavior
{
    t_getrectfn w_getrectfn;
    t_displacefn w_displacefn;
    t_selectfn w_selectfn;
    t_activatefn w_activatefn;
    t_deletefn w_deletefn;
    t_visfn w_visfn;
    t_clickfn w_clickfn;
};

/* -------- behaviors for scalars defined by objects in template --------- */
/* these are set by "drawing commands" in g_template.c which add appearance to
scalars, which live in some other window.  If the scalar is just included
in a canvas the "parent" is a misnomer.  There is also a text scalar object
which really does draw the scalar on the parent window; see g_scalar.c. */

/* note how the click function wants the whole scalar, not the "data", so
doesn't work on array elements... LATER reconsider this */

        /* bounding rectangle: */
typedef void (*t_parentgetrectfn)(t_gobj *x, struct _glist *glist,
    t_word *data, t_template *tmpl, t_float basex, t_float basey,
    int *x1, int *y1, int *x2, int *y2);
        /* displace it */
typedef void (*t_parentdisplacefn)(t_gobj *x, struct _glist *glist, 
    t_word *data, t_template *tmpl, t_float basex, t_float basey,
    int dx, int dy);
        /* change color to show selection */
typedef void (*t_parentselectfn)(t_gobj *x, struct _glist *glist,
    t_word *data, t_template *tmpl, t_float basex, t_float basey,
    int state);
        /* change appearance to show activation/deactivation: */
typedef void (*t_parentactivatefn)(t_gobj *x, struct _glist *glist,
    t_word *data, t_template *tmpl, t_float basex, t_float basey,
    int state);
        /*  making visible or invisible */
typedef void (*t_parentvisfn)(t_gobj *x, struct _glist *glist,
    t_word *data, t_template *tmpl, t_float basex, t_float basey,
    int flag);
        /*  field a mouse click */
typedef int (*t_parentclickfn)(t_gobj *x, struct _glist *glist,
    t_word *data, t_template *tmpl, t_scalar *sc, t_array *ap,
    t_float basex, t_float basey,
    int xpix, int ypix, int shift, int alt, int dbl, int doit);

struct _parentwidgetbehavior
{
    t_parentgetrectfn w_parentgetrectfn;
    t_parentdisplacefn w_parentdisplacefn;
    t_parentselectfn w_parentselectfn;
    t_parentactivatefn w_parentactivatefn;
    t_parentvisfn w_parentvisfn;
    t_parentclickfn w_parentclickfn;
};

    /* cursor definitions; used as return value for t_parentclickfn */
#define CURSOR_RUNMODE_NOTHING 0
#define CURSOR_RUNMODE_CLICKME 1
#define CURSOR_RUNMODE_THICKEN 2
#define CURSOR_RUNMODE_ADDPOINT 3
#define CURSOR_EDITMODE_NOTHING 4
#define CURSOR_EDITMODE_CONNECT 5
#define CURSOR_EDITMODE_DISCONNECT 6
EXTERN void canvas_setcursor(t_glist *x, unsigned int cursornum);

extern t_canvas *canvas_editing;    /* last canvas to start text edting */ 
extern t_canvas *canvas_whichfind;  /* last canvas we did a find in */ 
extern t_canvas *canvas_list;       /* list of all root canvases */
extern t_class *vinlet_class, *voutlet_class;
extern int glist_valid;         /* incremented when pointers might be stale */

#define PLOTSTYLE_POINTS 0     /* plotting styles for arrays */
#define PLOTSTYLE_POLY 1
#define PLOTSTYLE_BEZ 2

/* ------------------- functions on any gobj ----------------------------- */
EXTERN void gobj_getrect(t_gobj *x, t_glist *owner, int *x1, int *y1,
    int *x2, int *y2);
EXTERN void gobj_displace(t_gobj *x, t_glist *owner, int dx, int dy);
EXTERN void gobj_select(t_gobj *x, t_glist *owner, int state);
EXTERN void gobj_activate(t_gobj *x, t_glist *owner, int state);
EXTERN void gobj_delete(t_gobj *x, t_glist *owner);
EXTERN void gobj_vis(t_gobj *x, t_glist *glist, int flag);
EXTERN int gobj_click(t_gobj *x, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit);
EXTERN void gobj_save(t_gobj *x, t_binbuf *b);
EXTERN void gobj_properties(t_gobj *x, struct _glist *glist);
EXTERN void gobj_save(t_gobj *x, t_binbuf *b);
EXTERN int gobj_shouldvis(t_gobj *x, struct _glist *glist);

/* -------------------- functions on glists --------------------- */
EXTERN t_glist *glist_new( void);
EXTERN void glist_init(t_glist *x);
EXTERN void glist_add(t_glist *x, t_gobj *g);
EXTERN void glist_cleanup(t_glist *x);
EXTERN void glist_free(t_glist *x);

EXTERN void glist_clear(t_glist *x);
EXTERN t_canvas *glist_getcanvas(t_glist *x);
EXTERN int glist_isselected(t_glist *x, t_gobj *y);
EXTERN void glist_select(t_glist *x, t_gobj *y);
EXTERN void glist_deselect(t_glist *x, t_gobj *y);
EXTERN void glist_noselect(t_glist *x);
EXTERN void glist_selectall(t_glist *x);
EXTERN void glist_delete(t_glist *x, t_gobj *y);
EXTERN void glist_retext(t_glist *x, t_text *y);
EXTERN void glist_grab(t_glist *x, t_gobj *y, t_glistmotionfn motionfn,
    t_glistkeyfn keyfn, int xpos, int ypos);
EXTERN int glist_isvisible(t_glist *x);
EXTERN int glist_istoplevel(t_glist *x);
EXTERN t_glist *glist_findgraph(t_glist *x);
EXTERN int glist_getfont(t_glist *x);
EXTERN void glist_sort(t_glist *canvas);
EXTERN void glist_read(t_glist *x, t_symbol *filename, t_symbol *format);
EXTERN void glist_mergefile(t_glist *x, t_symbol *filename, t_symbol *format);

EXTERN t_float glist_pixelstox(t_glist *x, t_float xpix);
EXTERN t_float glist_pixelstoy(t_glist *x, t_float ypix);
EXTERN t_float glist_xtopixels(t_glist *x, t_float xval);
EXTERN t_float glist_ytopixels(t_glist *x, t_float yval);
EXTERN t_float glist_dpixtodx(t_glist *x, t_float dxpix);
EXTERN t_float glist_dpixtody(t_glist *x, t_float dypix);

EXTERN void glist_getnextxy(t_glist *gl, int *xval, int *yval);
EXTERN void glist_glist(t_glist *g, t_symbol *s, int argc, t_atom *argv);
EXTERN t_glist *glist_addglist(t_glist *g, t_symbol *sym,
    t_float x1, t_float y1, t_float x2, t_float y2,
    t_float px1, t_float py1, t_float px2, t_float py2);
EXTERN void glist_arraydialog(t_glist *parent, t_symbol *name,
    t_floatarg size, t_floatarg saveit, t_floatarg newgraph);
EXTERN t_binbuf *glist_writetobinbuf(t_glist *x, int wholething);
EXTERN int glist_isgraph(t_glist *x);
EXTERN void glist_redraw(t_glist *x);
EXTERN void glist_drawiofor(t_glist *glist, t_object *ob, int firsttime,
    char *tag, int x1, int y1, int x2, int y2);
EXTERN void glist_eraseiofor(t_glist *glist, t_object *ob, char *tag);
EXTERN void canvas_create_editor(t_glist *x);
EXTERN void canvas_destroy_editor(t_glist *x);
void canvas_deletelinesforio(t_canvas *x, t_text *text,
    t_inlet *inp, t_outlet *outp);
extern int glist_amreloadingabstractions; /* stop GUI changes while reloading */ 

/* -------------------- functions on texts ------------------------- */
EXTERN void text_setto(t_text *x, t_glist *glist, char *buf, int bufsize);
EXTERN void text_drawborder(t_text *x, t_glist *glist, char *tag,
    int width, int height, int firsttime);
EXTERN void text_eraseborder(t_text *x, t_glist *glist, char *tag);
EXTERN int text_xcoord(t_text *x, t_glist *glist);
EXTERN int text_ycoord(t_text *x, t_glist *glist);
EXTERN int text_xpix(t_text *x, t_glist *glist);
EXTERN int text_ypix(t_text *x, t_glist *glist);

/* -------------------- functions on rtexts ------------------------- */
#define RTEXT_DOWN 1
#define RTEXT_DRAG 2
#define RTEXT_DBL 3
#define RTEXT_SHIFT 4

EXTERN t_rtext *rtext_new(t_glist *glist, t_text *who);
EXTERN t_rtext *glist_findrtext(t_glist *gl, t_text *who);
EXTERN void rtext_draw(t_rtext *x);
EXTERN void rtext_erase(t_rtext *x);
EXTERN t_rtext *rtext_remove(t_rtext *first, t_rtext *x);
EXTERN int rtext_height(t_rtext *x);
EXTERN void rtext_displace(t_rtext *x, int dx, int dy);
EXTERN void rtext_select(t_rtext *x, int state);
EXTERN void rtext_activate(t_rtext *x, int state);
EXTERN void rtext_free(t_rtext *x);
EXTERN void rtext_key(t_rtext *x, int n, t_symbol *s);
EXTERN void rtext_mouse(t_rtext *x, int xval, int yval, int flag);
EXTERN void rtext_retext(t_rtext *x);
EXTERN int rtext_width(t_rtext *x);
EXTERN int rtext_height(t_rtext *x);
EXTERN char *rtext_gettag(t_rtext *x);
EXTERN void rtext_gettext(t_rtext *x, char **buf, int *bufsize);
EXTERN void rtext_getseltext(t_rtext *x, char **buf, int *bufsize);

/* -------------------- functions on canvases ------------------------ */
EXTERN t_class *canvas_class;

EXTERN t_canvas *canvas_new(void *dummy, t_symbol *sel, int argc, t_atom *argv);
EXTERN t_symbol *canvas_makebindsym(t_symbol *s);
EXTERN void canvas_vistext(t_canvas *x, t_text *y);
EXTERN void canvas_fixlinesfor(t_canvas *x, t_text *text);
EXTERN void canvas_deletelinesfor(t_canvas *x, t_text *text);
EXTERN void canvas_stowconnections(t_canvas *x);
EXTERN void canvas_restoreconnections(t_canvas *x);
EXTERN void canvas_redraw(t_canvas *x);

EXTERN t_inlet *canvas_addinlet(t_canvas *x, t_pd *who, t_symbol *sym);
EXTERN void canvas_rminlet(t_canvas *x, t_inlet *ip);
EXTERN t_outlet *canvas_addoutlet(t_canvas *x, t_pd *who, t_symbol *sym);
EXTERN void canvas_rmoutlet(t_canvas *x, t_outlet *op);
EXTERN void canvas_redrawallfortemplate(t_template *tmpl, int action);
EXTERN void canvas_redrawallfortemplatecanvas(t_canvas *x, int action);
EXTERN void canvas_zapallfortemplate(t_canvas *tmpl);
EXTERN void canvas_setusedastemplate(t_canvas *x);
EXTERN t_canvas *canvas_getcurrent(void);
EXTERN void canvas_setcurrent(t_canvas *x);
EXTERN void canvas_unsetcurrent(t_canvas *x);
EXTERN t_symbol *canvas_realizedollar(t_canvas *x, t_symbol *s);
EXTERN t_canvas *canvas_getrootfor(t_canvas *x);
EXTERN void canvas_dirty(t_canvas *x, t_floatarg n);
EXTERN int canvas_getfont(t_canvas *x);
typedef int (*t_canvasapply)(t_canvas *x, t_int x1, t_int x2, t_int x3);

EXTERN t_int *canvas_recurapply(t_canvas *x, t_canvasapply *fn,
    t_int x1, t_int x2, t_int x3);

EXTERN void canvas_resortinlets(t_canvas *x);
EXTERN void canvas_resortoutlets(t_canvas *x);
EXTERN void canvas_free(t_canvas *x);
EXTERN void canvas_updatewindowlist( void);
EXTERN void canvas_editmode(t_canvas *x, t_floatarg state);
EXTERN int canvas_isabstraction(t_canvas *x);
EXTERN int canvas_istable(t_canvas *x);
EXTERN int canvas_showtext(t_canvas *x);
EXTERN void canvas_vis(t_canvas *x, t_floatarg f);
EXTERN t_canvasenvironment *canvas_getenv(t_canvas *x);
EXTERN void canvas_rename(t_canvas *x, t_symbol *s, t_symbol *dir);
EXTERN void canvas_loadbang(t_canvas *x);
EXTERN int canvas_hitbox(t_canvas *x, t_gobj *y, int xpos, int ypos,
    int *x1p, int *y1p, int *x2p, int *y2p);
EXTERN int canvas_setdeleting(t_canvas *x, int flag);

typedef void (*t_undofn)(t_canvas *canvas, void *buf,
    int action);        /* a function that does UNDO/REDO */
#define UNDO_FREE 0                     /* free current undo/redo buffer */
#define UNDO_UNDO 1                     /* undo */
#define UNDO_REDO 2                     /* redo */
EXTERN void canvas_setundo(t_canvas *x, t_undofn undofn, void *buf,
    const char *name);
EXTERN void canvas_noundo(t_canvas *x);
EXTERN int canvas_getindex(t_canvas *x, t_gobj *y);

EXTERN void canvas_connect(t_canvas *x,
    t_floatarg fwhoout, t_floatarg foutno,t_floatarg fwhoin, t_floatarg finno);
EXTERN void canvas_disconnect(t_canvas *x,
    t_float index1, t_float outno, t_float index2, t_float inno);
EXTERN int canvas_isconnected (t_canvas *x,
    t_text *ob1, int n1, t_text *ob2, int n2);
EXTERN void canvas_selectinrect(t_canvas *x, int lox, int loy, int hix, int hiy);


/* ---- functions on canvasses as objects  --------------------- */

EXTERN void canvas_fattenforscalars(t_canvas *x,
    int *x1, int *y1, int *x2, int *y2);
EXTERN void canvas_visforscalars(t_canvas *x, t_glist *glist, int vis);
EXTERN int canvas_clicksub(t_canvas *x, int xpix, int ypix, int shift,
    int alt, int dbl, int doit);
EXTERN t_glist *canvas_getglistonsuper(void);

EXTERN void linetraverser_start(t_linetraverser *t, t_canvas *x);
EXTERN t_outconnect *linetraverser_next(t_linetraverser *t);
EXTERN void linetraverser_skipobject(t_linetraverser *t);

/* --------- functions on garrays (graphical arrays) -------------------- */

EXTERN t_template *garray_template(t_garray *x);

/* -------------------- arrays --------------------- */
EXTERN t_garray *graph_array(t_glist *gl, t_symbol *s, t_symbol *tmpl,
    t_floatarg f, t_floatarg saveit);
EXTERN t_array *array_new(t_symbol *templatesym, t_gpointer *parent);
EXTERN void array_resize(t_array *x, int n);
EXTERN void array_free(t_array *x);

/* --------------------- gpointers and stubs ---------------- */
EXTERN t_gstub *gstub_new(t_glist *gl, t_array *a);
EXTERN void gstub_cutoff(t_gstub *gs);
EXTERN void gpointer_setglist(t_gpointer *gp, t_glist *glist, t_scalar *x);
EXTERN void gpointer_setarray(t_gpointer *gp, t_array *array, t_word *w);

/* --------------------- scalars ------------------------- */
EXTERN void word_init(t_word *wp, t_template *tmpl, t_gpointer *gp);
EXTERN void word_restore(t_word *wp, t_template *tmpl,
    int argc, t_atom *argv);
EXTERN t_scalar *scalar_new(t_glist *owner,
    t_symbol *templatesym);
EXTERN void word_free(t_word *wp, t_template *tmpl);
EXTERN void scalar_getbasexy(t_scalar *x, t_float *basex, t_float *basey);
EXTERN void scalar_redraw(t_scalar *x, t_glist *glist);

/* ------helper routines for "garrays" and "plots" -------------- */
EXTERN int array_doclick(t_array *array, t_glist *glist, t_scalar *sc, t_array *ap,
    t_symbol *elemtemplatesym,
    t_float linewidth, t_float xloc, t_float xinc, t_float yloc, t_float scalarvis,
    t_fielddesc *xfield, t_fielddesc *yfield, t_fielddesc *wfield,
    int xpix, int ypix, int shift, int alt, int dbl, int doit);

EXTERN void array_getcoordinate(t_glist *glist,
    char *elem, int xonset, int yonset, int wonset, int indx,
    t_float basex, t_float basey, t_float xinc,
    t_fielddesc *xfielddesc, t_fielddesc *yfielddesc, t_fielddesc *wfielddesc,
    t_float *xp, t_float *yp, t_float *wp);

EXTERN int array_getfields(t_symbol *elemtemplatesym,
    t_canvas **elemtemplatecanvasp,
    t_template **elemtemplatep, int *elemsizep,
    t_fielddesc *xfielddesc, t_fielddesc *yfielddesc, t_fielddesc *wfielddesc, 
    int *xonsetp, int *yonsetp, int *wonsetp);

/* --------------------- templates ------------------------- */
EXTERN t_template *template_new(t_symbol *sym, int argc, t_atom *argv);
EXTERN void template_free(t_template *x);
EXTERN int template_match(t_template *x1, t_template *x2);
EXTERN int template_find_field(t_template *x, t_symbol *name, int *p_onset,
    int *p_type, t_symbol **p_arraytype);
EXTERN t_float template_getfloat(t_template *x, t_symbol *fieldname, t_word *wp,
    int loud);
EXTERN void template_setfloat(t_template *x, t_symbol *fieldname, t_word *wp,
    t_float f, int loud);
EXTERN t_symbol *template_getsymbol(t_template *x, t_symbol *fieldname,
    t_word *wp, int loud);
EXTERN void template_setsymbol(t_template *x, t_symbol *fieldname,
    t_word *wp, t_symbol *s, int loud);

EXTERN t_template *gtemplate_get(t_gtemplate *x);
EXTERN t_template *template_findbyname(t_symbol *s);
EXTERN t_canvas *template_findcanvas(t_template *tmpl);
EXTERN void template_notify(t_template *tmpl,
    t_symbol *s, int argc, t_atom *argv);

EXTERN t_float template_getfloat(t_template *x, t_symbol *fieldname,
    t_word *wp, int loud);
EXTERN void template_setfloat(t_template *x, t_symbol *fieldname,
    t_word *wp, t_float f, int loud);
EXTERN t_symbol *template_getsymbol(t_template *x, t_symbol *fieldname,
    t_word *wp, int loud);
EXTERN void template_setsymbol(t_template *x, t_symbol *fieldname,
    t_word *wp, t_symbol *s, int loud);
EXTERN t_float fielddesc_getcoord(t_fielddesc *f, t_template *tmpl,
    t_word *wp, int loud);
EXTERN void fielddesc_setcoord(t_fielddesc *f, t_template *tmpl,
    t_word *wp, t_float pix, int loud);
EXTERN t_float fielddesc_cvttocoord(t_fielddesc *f, t_float val);
EXTERN t_float fielddesc_cvtfromcoord(t_fielddesc *f, t_float coord);


/* ----------------------- guiconnects, g_guiconnect.c --------- */
EXTERN t_guiconnect *guiconnect_new(t_pd *who, t_symbol *sym);
EXTERN void guiconnect_notarget(t_guiconnect *x, double timedelay);

/* ------------- IEMGUI routines used in other g_ files ---------------- */
EXTERN t_symbol *iemgui_raute2dollar(t_symbol *s);
EXTERN t_symbol *iemgui_dollar2raute(t_symbol *s);

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus)
}
#endif
