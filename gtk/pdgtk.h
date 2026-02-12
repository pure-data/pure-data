#define BIGSTRING 10000

#ifdef USINGGTK
extern GtkApplication *tcl_gtkapp;
#endif

int tcl_init(void);

void tcl_runcommand(char *s);

struct _canvas;
#define t_canvas struct _canvas


/* defined in canvas.c : */
t_canvas *gfx_canvas_new(const char *tag, int xloc, int yloc,
    int width, int height, int editmode);
void gfx_canvas_free(t_canvas *x);
void gfx_canvas_settitle(t_canvas *x, char *s);

/* add a path (polygon, etc) */
void gfx_canvas_addpath(t_canvas *x, char *tag, char *purpose, int dashed,
    double width, int npoints, double *coords, int patchline);

/* add text */
void gfx_canvas_addtext(t_canvas *x, char *tag, char *purpose, char *text,
    double px, double py, double fontsize, int blue);

/* add a rectangle */
void gfx_canvas_addrectangle(t_canvas *x, char *tag, char *purpose,
    double width, double x1, double y1, double x2, double y2);

/* set text */
void gfx_canvas_text_set(t_canvas *x, char *tag, char *text);

/* move an item (text only?) */
void gfx_canvas_move(t_canvas *x, char *tag, double dx, double dy);

/* delete an item */
void gfx_canvas_delete(t_canvas *x, char *tag);

/* change coordinates of a path or text */
void gfx_canvas_coords(t_canvas *x, char *tag, int npoints, double *coords);

/* debugging printout to stderr */
void gfx_canvas_spew(t_canvas *x);


int socket_open(int portno);
int socket_send(char *msg);

extern int tcl_debug;
/* #define DEBUGTCL */
/* #define DEBUGGTK */
