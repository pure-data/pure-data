#define BIGSTRING 10000

int tcl_init(void);

void tcl_runcommand(char *s);

struct _canvas;
#define t_canvas struct _canvas


/* defined in canvas.c : */
t_canvas *gfx_canvas_new(const char *tag,
    int xloc, int yloc, int width, int height, int editmode);
void gfx_canvas_free(t_canvas *x);
void gfx_canvas_settitle(t_canvas *x, const char *s, const char *basename);
void gfx_canvas_menuclose(t_canvas *x, const char *s);

/* paths (polygons, etc) */
void gfx_canvas_addpath(t_canvas *x, char *tag, char *grouptag, int dashed,
    double width, int npoints, double *coords, int patchline);
void gfx_canvas_configurepath(t_canvas *x, char *tag, int width,
    const char *color);

/* add text */
void gfx_canvas_addtext(t_canvas *x, char *tag, char *grouptag, char *text,
    double px, double py, double fontsize, char *color);

/* add a rectangle */
void gfx_canvas_addrectangle(t_canvas *x, char *tag, char *grouptag,
    double width, char *fill, char *outline,
        double x1, double y1, double x2, double y2, int oval);

/* set text */
void gfx_canvas_text_set(t_canvas *x, char *tag, char *text);

/* move an item (text only?) */
void gfx_canvas_move(t_canvas *x, char *tag, double dx, double dy);

/* delete an item */
void gfx_canvas_delete(t_canvas *x, char *tag);

/* change coordinates of a path or text */
void gfx_canvas_coords(t_canvas *x, char *tag, int npoints, double *coords);

/* show filename etc in title bar */
void gfx_canvas_reflecttitle(t_canvas *x, const char *path, const char *name,
    const char *arguments, int dirty);

/* debugging printout to stderr */
void gfx_canvas_spew(t_canvas *x);

int socket_open(int portno);
int socket_send(char *msg);
int socket_startpd(int argc, char **argv);

void pdgtk_start_watchdog( void);

extern int tcl_debug;

/* pass "app" from pdgtk.c to canvas.c without bothering tclparser about it */
void *pdgtk_thisapp( void);


/* #define DEBUGTCL */
/* #define DEBUGGTK */
