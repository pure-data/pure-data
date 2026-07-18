/*
    canvas.c - use GTK to draw and manage Pd canvases (aka patch windows)
*/

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#define USINGGTK
#include "pdgtk.h"
#include <math.h>

typedef struct _dpoint
{
    double p_x;
    double p_y;
} t_dpoint;

typedef struct _color
{
    float c_red;
    float c_green;
    float c_blue;
} t_color;

typedef struct _path
{
    int p_n;
    t_dpoint *p_vec;
    double p_width;
    int p_dashed;
    int p_patchline;    /* LATER make sure these are drawn in front. */
} t_path;

typedef struct _rect
{
    double r_x1;
    double r_y1;
    double r_x2;
    double r_y2;
    double r_width;
    int r_oval;
} t_rect;

typedef struct _text
{
    double t_x;
    double t_y;
    double t_fontsize;
    int i_blue;
    char *t_text;
} t_text;

#define I_PATH 1
#define I_RECT 2
#define I_TEXT 3

typedef struct _item
{
    int i_type;
    char i_tag[80];
    char i_grouptag[80];
    t_color i_fill;        /* LATER allow paths to have color gradients */
    t_color i_outline;
    union
    {
        t_path *i_path;
        t_rect *i_rect;
        t_text *i_text;
    } i_w;
} t_item;

struct _canvas
{
    GtkWidget *c_window;
    GtkWidget *c_frame;
    GtkWidget *c_drawing_area;
    cairo_surface_t *c_surface;
    char c_tag[80];
    char c_basename[80];
    char c_tmpstring[80];
    int c_n;
    t_item *c_vec;
    int c_seltextindex;
    int c_selstart;
    int c_selend;
    int c_editmode;
    struct _canvas *c_next;
};

static void gfx_parse_color(const char *str, t_color *c)
{
    int color;
    if (str[0] != '#' || strlen(str) != 7 || sscanf(str+1, "%x", &color) < 1)
        fprintf(stderr, "can't parse color '%s'\n", str);
    c->c_red = (((color >> 16)) & 0xff) / 255.;
    c->c_green = (((color >> 8)) & 0xff) / 255.;
    c->c_blue = (((color >> 0)) & 0xff) / 255.;
}

static void gfx_set_color(cairo_t *cr, t_color *c)
{
    cairo_set_source_rgb(cr, c->c_red, c->c_green, c->c_blue);
}

static void gfx_path_draw(t_path *x, t_item *it, t_canvas *c, cairo_t *cr)
{
    int i;
    if (x->p_n < 1)
        return;
    gfx_set_color(cr, &it->i_outline);
    cairo_set_line_width(cr, 1.5*x->p_width);
    cairo_move_to(cr, x->p_vec[0].p_x, x->p_vec[0].p_y);
    for (i = 1; i < x->p_n; i++)
        cairo_line_to(cr, x->p_vec[i].p_x, x->p_vec[i].p_y);
    cairo_stroke(cr);
}

void gfx_canvas_addpath(t_canvas *x, char *tag, char *grouptag, int dashed,
    double width, int npoints, double *coords, int patchline)
{
    t_path *p;
    t_item *it;
    int i;

    x->c_vec = (t_item *)realloc(x->c_vec, (x->c_n+1)*sizeof(*x->c_vec));
    it = x->c_vec + x->c_n;
    it->i_type = I_PATH;
    p = it->i_w.i_path = (t_path *)malloc(sizeof(t_path));
    x->c_n++;
    p->p_n = npoints;
    p->p_vec = (t_dpoint *)malloc(npoints * sizeof(t_dpoint));
    for (i = 0; i < npoints; i++)
        p->p_vec[i].p_x = coords[2*i], p->p_vec[i].p_y = coords[2*i+1];
    p->p_width = width;
    p->p_dashed = dashed;
    p->p_patchline = patchline;
    gfx_parse_color("#000000", &it->i_outline);
    gfx_parse_color("#000000", &it->i_fill);
    strncpy(it->i_tag, tag, 80);
    it->i_tag[79] = 0;
    strncpy(it->i_grouptag, grouptag, 80);
    it->i_grouptag[79] = 0;
    gtk_widget_queue_draw(x->c_drawing_area);
}

void path_free(t_path *x)
{
    free(x->p_vec);
    free(x);
}

void gfx_canvas_coords(t_canvas *x, char *tag, int npoints, double *coords)
{
    int indx, i;
    for (indx = 0; indx < x->c_n; indx++)
        if (!strcmp(tag, x->c_vec[indx].i_tag) ||
            !strcmp(tag, x->c_vec[indx].i_grouptag))
    {
        if (x->c_vec[indx].i_type == I_PATH)
        {
            t_path *y = x->c_vec[indx].i_w.i_path;
            y->p_vec = (t_dpoint *)realloc(y->p_vec,
                npoints * sizeof(*y->p_vec));
            for (i = 0; i < npoints; i++)
                y->p_vec[i].p_x = coords[2*i], y->p_vec[i].p_y = coords[2*i+1];
            gtk_widget_queue_draw(x->c_drawing_area);
            return;
        }
        else if (x->c_vec[indx].i_type == I_TEXT)
        {
            x->c_vec[indx].i_w.i_text->t_x = coords[0];
            x->c_vec[indx].i_w.i_text->t_y = coords[1];
            gtk_widget_queue_draw(x->c_drawing_area);
            return;
        }
        else if (x->c_vec[indx].i_type == I_RECT)
        {
            x->c_vec[indx].i_w.i_rect->r_x1 = coords[0];
            x->c_vec[indx].i_w.i_rect->r_y1 = coords[1];
            x->c_vec[indx].i_w.i_rect->r_x2 = coords[2];
            x->c_vec[indx].i_w.i_rect->r_y2 = coords[3];
            gtk_widget_queue_draw(x->c_drawing_area);
            return;
        }
    }
    fprintf(stderr, "canvas_coords: unknown tag %s\n", tag);
}

static void gfx_text_draw(t_text *x, t_item *it, t_canvas *c, cairo_t *cr)
{
    cairo_font_extents_t extents;
    char line[BIGSTRING], *wherenewline;
    int start, nline, linelength;
    gfx_set_color(cr, &it->i_fill);
    cairo_select_font_face(cr, "monospace",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, x->t_fontsize);
    cairo_font_extents (cr, &extents);
    for (start = nline = 0; x->t_text[start]; )
    {
        wherenewline = strchr(x->t_text + start, '\n');
        if (!wherenewline)
        {
            cairo_move_to(cr, x->t_x, x->t_y + extents.ascent
                + nline * (extents.ascent + extents.descent));
            cairo_show_text(cr, x->t_text + start);
            break;
        }
        linelength = wherenewline - (x->t_text + start);
        if (linelength > BIGSTRING-1)
            linelength = BIGSTRING-1;
        strncpy(line, x->t_text + start, linelength);
        line[linelength] = 0;
        cairo_move_to(cr, x->t_x, x->t_y + extents.ascent
            + nline * (extents.ascent + extents.descent));
        cairo_show_text(cr, line);
        nline++;
        start = (wherenewline - x->t_text) + 1;
    }
}

void gfx_canvas_addtext(t_canvas *x, char *tag, char *grouptag, char *text,
    double px, double py, double fontsize, char *color)
{
    t_text *t;
    t_item *it;
    int i;

    x->c_vec = (t_item *)realloc(x->c_vec, (x->c_n+1)*sizeof(*x->c_vec));
    it = x->c_vec + x->c_n;
    it->i_type = I_TEXT;
    t = it->i_w.i_text = (t_text *)malloc(sizeof(t_text));
    x->c_n++;
    t->t_text = malloc(strlen(text) + 1);
    strcpy(t->t_text, text);
    t->t_x = px;
    t->t_y = py;
    t->t_fontsize = fontsize;
    gfx_parse_color(color, &it->i_outline);
    gfx_parse_color(color, &it->i_fill);

    strncpy(it->i_tag, tag, 80);
    it->i_tag[79] = 0;
    strncpy(it->i_grouptag, grouptag, 80);
    it->i_grouptag[79] = 0;
    gtk_widget_queue_draw(x->c_drawing_area);
}

void gfx_canvas_text_set(t_canvas *x, char *tag, char *text)
{
    t_text *t;
    t_item *it;
    int i;

    for (i = 0; i < x->c_n; i++)
    {
        if (x->c_vec[i].i_type == I_TEXT && !strcmp(x->c_vec[i].i_tag, tag))
        {
            x->c_vec[i].i_w.i_text->t_text =
                realloc(x->c_vec[i].i_w.i_text->t_text, strlen(text) + 1);
            strncpy(x->c_vec[i].i_w.i_text->t_text, text, strlen(text) + 1);
            gtk_widget_queue_draw(x->c_drawing_area);
            return;
        }
    }
    fprintf(stderr, "text_set: couldn't find text with tag '%s'\n", tag);
}

void gfx_canvas_text_select(t_canvas *x, char *tag, int start, int end)
{
    t_text *t;
    t_item *it;
    int i;

    if (!tag)
    {
        x->c_seltextindex = -1, x->c_selstart = x->c_selend = 0;
        return;
    }
    for (i = 0; i < x->c_n; i++)
    {
        if (x->c_vec[i].i_type == I_TEXT && !strcmp(x->c_vec[i].i_tag, tag))
        {
            if (start < 0 || start > end ||
                end > strlen(x->c_vec[i].i_w.i_text->t_text))
                    fprintf(stderr, "text_select: bad range\n");
            x->c_seltextindex = i;
            x->c_selstart = start;
            x->c_selend = end;
            fprintf(stderr, "index %d\n", x->c_seltextindex);
            return;
        }
    }
    fprintf(stderr, "text_select: couldn't find text with tag '%s'\n", tag);
}

void canvas_free_text(t_canvas *x, t_text *t)
{
    free(t->t_text);
    free(t);
}

static void gfx_rect_draw(t_rect *x, t_item *it, t_canvas *c, cairo_t *cr)
{
    if (x->r_oval)
        cairo_arc(cr, 0.5*(x->r_x1+x->r_x2), 0.5*(x->r_y1+x->r_y2),
            0.5 * fabs(x->r_x1-x->r_x2), 0, 2 * M_PI);
    else cairo_rectangle(cr, x->r_x1, x->r_y1, x->r_x2 - x->r_x1,
        x->r_y2 - x->r_y1);
    gfx_set_color(cr, &it->i_fill);
    cairo_fill(cr);
    cairo_set_line_width(cr, 1.5*x->r_width);
    if (x->r_oval)
        cairo_arc(cr, 0.5*(x->r_x1+x->r_x2), 0.5*(x->r_y1+x->r_y2),
            0.5 * fabs(x->r_x1-x->r_x2), 0, 2 * M_PI);
    else cairo_rectangle(cr, x->r_x1, x->r_y1, x->r_x2 - x->r_x1,
        x->r_y2 - x->r_y1);
    gfx_set_color(cr, &it->i_outline);
    cairo_stroke(cr);
}

void gfx_canvas_addrectangle(t_canvas *x, char *tag, char *grouptag,
    double width, char *fill, char *outline,
        double x1, double y1, double x2, double y2, int oval)
{
    t_rect *r;
    t_item *it;
    int i;

    x->c_vec = (t_item *)realloc(x->c_vec, (x->c_n+1)*sizeof(*x->c_vec));
    it = x->c_vec + x->c_n;
    it->i_type = I_RECT;
    r = it->i_w.i_rect = (t_rect *)malloc(sizeof(t_rect));
    x->c_n++;
    r->r_x1 = x1;
    r->r_y1 = y1;
    r->r_x2 = x2+width;
    r->r_y2 = y2+width;
    r->r_width = width;
    r->r_oval = oval;
    gfx_parse_color(fill, &it->i_fill);
    gfx_parse_color(outline, &it->i_outline);
    strncpy(it->i_tag, tag, 80);
    it->i_tag[79] = 0;
    strncpy(it->i_grouptag, grouptag, 80);
    it->i_grouptag[79] = 0;
    gtk_widget_queue_draw(x->c_drawing_area);
}

void rect_free(t_rect *x)
{
    free(x);
}

    /* set params for either a path or a rectangle.  Need not set all the
     parameters - can be called with null strings for colors or width < 0
    to avoid changing those params. */
void gfx_canvas_configure_whatev(t_canvas *x, char *tag, int width,
    const char *fillcolor, const char *outlinecolor)
{
    int indx, i;
    for (indx = 0; indx < x->c_n; indx++)
        if (!strcmp(tag, x->c_vec[indx].i_tag))
    {
        if (width >= 0 && x->c_vec[indx].i_type == I_RECT)
            x->c_vec[indx].i_w.i_rect->r_width = width;
        else if (width >= 0 && x->c_vec[indx].i_type == I_PATH)
            x->c_vec[indx].i_w.i_path->p_width = width;
        if (fillcolor)
            gfx_parse_color(fillcolor, &x->c_vec[indx].i_fill);
        if (outlinecolor)
            gfx_parse_color(outlinecolor, &x->c_vec[indx].i_outline);
        gtk_widget_queue_draw(x->c_drawing_area);
    }
}

void gfx_canvas_move(t_canvas *x, char *tag, double dx, double dy)
{
    int indx, didone = 0, i;
    for (indx = 0; indx < x->c_n; indx++)
        if (!strcmp(tag, x->c_vec[indx].i_tag) ||
            !strcmp(tag, x->c_vec[indx].i_grouptag))
    {
        if (x->c_vec[indx].i_type == I_TEXT)
        {
            x->c_vec[indx].i_w.i_text->t_x += dx;
            x->c_vec[indx].i_w.i_text->t_y += dy;
            gtk_widget_queue_draw(x->c_drawing_area);
            didone = 1;
        }
        else if (x->c_vec[indx].i_type == I_PATH)
        {
            for (i = 0; i < x->c_vec[indx].i_w.i_path->p_n; i++)
                x->c_vec[indx].i_w.i_path->p_vec[i].p_x += dx,
                    x->c_vec[indx].i_w.i_path->p_vec[i].p_y += dy;
            gtk_widget_queue_draw(x->c_drawing_area);
            didone = 1;
        }
        else if (x->c_vec[indx].i_type == I_RECT)
        {
            x->c_vec[indx].i_w.i_rect->r_x1 += dx;
            x->c_vec[indx].i_w.i_rect->r_x2 += dx;
            x->c_vec[indx].i_w.i_rect->r_y1 += dy;
            x->c_vec[indx].i_w.i_rect->r_y2 += dy;
            gtk_widget_queue_draw(x->c_drawing_area);
            didone = 1;
        }
    }
    if (!didone)
        fprintf(stderr, "canvas_move: unknown tag %s\n", tag);
}

void gfx_canvas_delete(t_canvas *x, char *tag)
{
    int indx, didone = 0;
    for (indx = 0; indx < x->c_n; )
    {
        if (!strcmp(tag, x->c_vec[indx].i_tag))
        {
            if (x->c_seltextindex == indx)
                x->c_seltextindex = -1;
            if (x->c_vec[indx].i_type == I_PATH)
                path_free(x->c_vec[indx].i_w.i_path);
            else if (x->c_vec[indx].i_type == I_TEXT)
                canvas_free_text(x, x->c_vec[indx].i_w.i_text);
            else if (x->c_vec[indx].i_type == I_RECT)
                rect_free(x->c_vec[indx].i_w.i_rect);
            memmove((char *)(x->c_vec + indx), (char *)(x->c_vec + (indx+1)),
                (x->c_n - indx - 1) * sizeof(*x->c_vec));
            x->c_vec = (t_item *)realloc(x->c_vec,
                (x->c_n-1) * sizeof(*x->c_vec));
            x->c_n--;
            didone = 1;
        }
        else indx++;
    }
    if (!didone)
        fprintf(stderr, "canvas_delete: unknown tag %s\n", tag);
    gtk_widget_queue_draw(x->c_drawing_area);
    return;
}

    /* for debugging: */
void gfx_canvas_spew(t_canvas *x)
{
    int i, j;
    for (i = 0; i < x->c_n; i++)
    {
        switch (x->c_vec[i].i_type)
        {
            case I_PATH:
                fprintf(stderr, "path %s %s:", x->c_vec[i].i_tag,
                    x->c_vec[i].i_grouptag);
                for (j = 0; j < x->c_vec[i].i_w.i_path->p_n; j++)
                    fprintf(stderr, "%f %f:",
                        x->c_vec[i].i_w.i_path->p_vec[j].p_x,
                        x->c_vec[i].i_w.i_path->p_vec[j].p_y);
                fprintf(stderr, "\n");
                break;
            case I_RECT:
                fprintf(stderr, "rect %s %s:", x->c_vec[i].i_tag,
                    x->c_vec[i].i_grouptag);
                break;
            case I_TEXT:
                fprintf(stderr, "text\n");
                break;
            default:
                fprintf(stderr, "unknown item\n");
        }
    }
}

void gfx_canvas_settitle(t_canvas *x, const char *s, const char *basename)
{
    gtk_window_set_title(GTK_WINDOW(x->c_window), s);
    strncpy(x->c_basename, basename, 80);
    x->c_basename[79] = 0;
}

void gfx_canvas_menuclose_done(GObject *gob, GAsyncResult *res,
    gpointer z)
{
    int button = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(gob), res, 0);
    t_canvas *x = (t_canvas *)z;
    char msg2[80];
    if (button == 0)
    {
        snprintf(msg2, 80, "%s menusave 1;\n", x->c_tag);
        socket_send(msg2);
    }
    else if (button == 1)
        socket_send(x->c_tmpstring);
}

void gfx_canvas_menuclose(t_canvas *x, const char *cmd)
{
    int button;
    const char* labels[4];
    GtkAlertDialog *dog = gtk_alert_dialog_new(
        _("Do you want to save the changes you made in '%s'?"),
            x->c_basename);
    strncpy(x->c_tmpstring, cmd, 80);
    x->c_tmpstring[79] = 0;
    labels[0] = _("Yes");
    labels[1] = _("No");
    labels[2] = _("Cancel");
    labels[3] = 0;
    gtk_alert_dialog_set_buttons(dog, labels);
    gtk_alert_dialog_set_cancel_button (dog, 2);
    gtk_alert_dialog_set_default_button (dog, 0);
    gtk_alert_dialog_choose(dog, GTK_WINDOW(x->c_window), 0,
        gfx_canvas_menuclose_done, x);
}

static void gfx_canvas_dofree(t_canvas *x)
{
    int i;
    for (i = 0; i < x->c_n; i++)
        switch (x->c_vec[i].i_type)
    {
    case I_PATH:
        path_free(x->c_vec[i].i_w.i_path);
        break;
    case I_RECT:
        rect_free(x->c_vec[i].i_w.i_rect);
        break;
    case I_TEXT:
        canvas_free_text(x, x->c_vec[i].i_w.i_text);
        break;
    }
    free(x->c_vec);
    free(x);
}

void gfx_canvas_free(t_canvas *x)
{
    gtk_window_destroy((GtkWindow *)(x->c_window));
}

static t_canvas *gfx_canvas_list; /* probably can delete this */

    /* events from GTK */
static void gfx_window_destroy(GtkWidget *widget)
{
    t_canvas *y, *z;
    if (gfx_canvas_list->c_window != widget)
    {
        for (y = gfx_canvas_list; z = y->c_next; y = z)
            if (z->c_window == widget)
        {
            y->c_next = z->c_next;
            break;
        }
        fprintf(stderr, "couldn't find window to free\n");
        return;
    }
    else z = gfx_canvas_list, gfx_canvas_list = gfx_canvas_list->c_next;
    gfx_canvas_dofree(z);
}

#define CHARW 7.     /* hack - get this from cairo later */
#define CHARH 16.

void gfx_canvas_drawsel(t_canvas *x, cairo_t *cr)
{
    t_text *t = x->c_vec[x->c_seltextindex].i_w.i_text;
    int start = x->c_selstart, end = x->c_selend;
    double basex = t->t_x, basey = t->t_y;
    int i, n = strlen(t->t_text), row, col, row1, col1, row2, col2, width;
    for (i = row = col = row1 = col1 = row2 = col2 = width = 0; ; i++)
    {
        if (col > width)
            width = col;
        if (i == end)
            row2 = row, col2 = col;
        if (i == start)
            row1 = row, col1 = col;
        if (t->t_text[i] == '\n')
        {
            row++;
            col = 0;
        }
        else col++;
        if (i >= n)
            break;
    }

    cairo_set_source_rgb(cr, 0.0, 0.0, 1.);
    cairo_set_line_width(cr, 1.);
    cairo_move_to(cr, basex + CHARW * col1, basey + CHARH * row1);
    if (start == end)
    {
        cairo_line_to(cr, basex + CHARW * col1, basey + CHARH * (row1+1));
        cairo_stroke(cr);
    }
    else if (row2 > row1)
    {
        cairo_line_to(cr, basex + CHARW * col1, basey + CHARH * row1);
        cairo_line_to(cr, basex + CHARW * width, basey + CHARH * row1);
        cairo_line_to(cr, basex + CHARW * width, basey + CHARH * row2);
        cairo_line_to(cr, basex + CHARW * col2, basey + CHARH * row2);
        cairo_line_to(cr, basex + CHARW * col2, basey + CHARH * (row2+1));
        cairo_line_to(cr, basex + CHARW * 0, basey + CHARH * (row2+1));
        cairo_line_to(cr, basex + CHARW * 0, basey + CHARH * (row1+1));
        cairo_line_to(cr, basex + CHARW * col1, basey + CHARH * (row1+1));
        cairo_line_to(cr, basex + CHARW * col1, basey + CHARH * row1);
        cairo_stroke(cr);
    }
    else
    {
        cairo_line_to(cr, basex + CHARW * col2, basey + CHARH * row1);
        cairo_line_to(cr, basex + CHARW * col2, basey + CHARH * (row1+1));
        cairo_line_to(cr, basex + CHARW * col1, basey + CHARH * (row1+1));
        cairo_line_to(cr, basex + CHARW * col1, basey + CHARH * row1);
        cairo_stroke(cr);
    }
}

static void gfx_window_draw(GtkDrawingArea *drawing_area, cairo_t *cr,
    int width, int height, t_canvas *x)
{
    int i;
    /* fprintf(stderr, "draw callback\n"); */
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    if (x->c_seltextindex >= 0)
        gfx_canvas_drawsel(x, cr);
    for (i = 0; i < x->c_n; i++)
    {
        switch (x->c_vec[i].i_type)
        {
            case I_PATH:
                gfx_path_draw(x->c_vec[i].i_w.i_path, &x->c_vec[i], x, cr);
                break;
            case I_RECT:
                gfx_rect_draw(x->c_vec[i].i_w.i_rect, &x->c_vec[i], x, cr);
                break;
            case I_TEXT:
                gfx_text_draw(x->c_vec[i].i_w.i_text, &x->c_vec[i], x, cr);
                break;
            default:
                fprintf(stderr, "unknown item\n");
        }
    }
}

static void gfx_window_resize(GtkWidget *widget, int width, int height,
    t_canvas *x)
{
}

static void gfx_window_motion(GtkEventControllerMotion *controller,
  gdouble px, gdouble py, t_canvas *x)
{
    int shift = 0;
    char event[81];
    event[80] = 0;
    snprintf(event, 80, "%s motion %f %f %d;\n", x->c_tag, px, py, shift);
    socket_send(event);
#ifdef DBUGGTK
    fprintf(stderr, "motion %lf %lf\n", px, py);
#endif
}

static gboolean dokey(GtkEventControllerKey* self, guint keyval, guint keycode,
  GdkModifierType state, t_canvas *x, int down)
{
    char event[81], *keysym = 0;
    int ascii = 0;
    event[80] = 0;
#ifdef DBUGGTK
    fprintf(stderr, "key %d %d %d %d\n", keyval, keycode, state, down);
#endif
    /*if (((state & GDK_CONTROL_MASK) || (state & GDK_META_MASK))
        && (keyval < 255))
            keyval &= (~96); */
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
        keyval = 10;
    else if (keyval == GDK_KEY_BackSpace)
        keyval = 8;
    else if  (keyval == GDK_KEY_Delete)
        keyval = 127;
    else if  (keyval == GDK_KEY_Control_L)
        keyval = 0, keysym = "CONTROL_L";
    else if  (keyval == GDK_KEY_Control_R)
        keyval = 0, keysym = "CONTROL_R";
    else if  (keyval == GDK_KEY_Shift_L)
        keyval = 0, keysym = "SHIFT_L";
    else if  (keyval == GDK_KEY_Shift_R)
        keyval = 0, keysym = "SHIFT_R";
    if (keysym)
        snprintf(event, 80, "%s key %d %s %d;\n", x->c_tag,
            down, keysym, (state & GDK_SHIFT_MASK) != 0);
    else snprintf(event, 80, "%s key %d %d %d;\n", x->c_tag,
            down, keyval, (state & GDK_SHIFT_MASK) != 0);

    event[80] = 0;
    socket_send(event);
    return (TRUE);
}

gboolean gfx_keypress(GtkEventControllerKey* self, guint keyval, guint keycode,
    GdkModifierType state, t_canvas *x)
{
    return (dokey(self, keyval, keycode, state, x, 1));
}

gboolean gfx_keyrelease(GtkEventControllerKey* self, guint keyval,
    guint keycode, GdkModifierType state, t_canvas *x)
{
    return (dokey(self, keyval, keycode, state, x, 0));
}

static void gfx_pressed(GtkGestureClick *gesture, int n_press,
    double px, double py, t_canvas *x)
{
        /* need to get shift key status here and deal with right clicks */
    int shift = 0;
    char event[81];
    event[80] = 0;
    snprintf(event, 80, "%s mouse %lf %lf 1 %d;\n", x->c_tag, px, py, shift);
    socket_send(event);
#ifdef DEBUGGTK
    fprintf(stderr, "mouse down %lf %lf\n", px, py);
#endif
}

static void gfx_released(GtkGestureClick *gesture, int n_press,
    double px, double py, t_canvas *x)
{
    char event[81];
    event[80] = 0;
    snprintf(event, 80, "%s mouseup %lf %lf 1 %d;\n", x->c_tag, px, py, 0);
    socket_send(event);
#ifdef DEBUGGTK
    fprintf(stderr, "mouse up %lf %lf\n", px, py);
#endif
}

static void winmenu_simpleitem(GtkApplicationWindow *win, gpointer *data,
    const char *what)
{
    char cmd[80];
    snprintf(cmd, 80, "%s %s;\n", ((t_canvas *)data)->c_tag, what);
    cmd[79] = 0;
    socket_send(cmd);
}

    /* function bindings for simple menu actions - I can't find a way to
    just pass the action name as an argument so we have to have 100 stupid
    functions here... */
static void winmenu_save(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "menusave 0"); }
static void winmenu_saveas(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "menusaveas 0"); }
static void winmenu_close(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "menuclose 0"); }
static void editmenu_undo(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "undo"); }
static void editmenu_redo(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "redo"); }
static void editmenu_cut(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "cut"); }
static void editmenu_copy(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "copy"); }
static void editmenu_dup(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "duplicate"); }

void wombat( void);

static void paste_callback(GObject *source_object, GAsyncResult* res,
    gpointer data)
{
    GdkClipboard *clipboard = GDK_CLIPBOARD(source_object);
    char *str = gdk_clipboard_read_text_finish(clipboard, res, 0), msg[80];
    int i, n = strlen(str), outlen, havechars;
    t_canvas *x = (t_canvas *)data;
    snprintf(msg, 80, "%s pastechars -1;\n", x->c_tag);
    socket_send(msg);
    snprintf(msg, 80, "%s pastechars", x->c_tag);
    outlen = strlen(msg);
    havechars = 0;
    for (i = 0; i < n; i++)
    {
        snprintf(msg + outlen, 80-outlen, " %d", 0xff & str[i]);
        havechars = 1;
        outlen += strlen(msg+outlen);
        if (outlen > 70)
        {
            strncat(msg + outlen, ";\n", 80-outlen);
            socket_send(msg);
            snprintf(msg, 80, "%s pastechars", x->c_tag);
            outlen = strlen(msg);
            havechars = 0;
        }
    }
    if (havechars)
    {
        strncat(msg + outlen, ";\n", 80-outlen);
        socket_send(msg);
    }
    snprintf(msg, 80, "%s pastechars -2;\n", x->c_tag);
    socket_send(msg);
}

static void editmenu_paste(GtkWindow *win, void *zz, gpointer *data)
{
    t_canvas *x = (t_canvas *)data;
    GtkWidget *zwidget;
    GdkClipboard *zclipboard;
    zwidget = GTK_WIDGET(x->c_window);
    zclipboard = gtk_widget_get_clipboard(zwidget);
    gdk_clipboard_read_text_async(zclipboard, 0, paste_callback, x);
}

static void editmenu_edit(GtkApplicationWindow *win, void *unused,
    gpointer *data)
{
    t_canvas *x = (t_canvas *)data;
    x->c_editmode = !x->c_editmode;
    char cmd[80];
    snprintf(cmd, 80, "%s editmode %d\n", x->c_tag, x->c_editmode);
    socket_send(cmd);
}

static void putmenu_obj(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "obj"); }
static void putmenu_msg(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "msg"); }
static void putmenu_num(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "floatatom"); }
static void putmenu_list(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "listbox"); }
static void putmenu_text(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "text"); }
static void putmenu_bng(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "bng"); }
static void putmenu_tgl(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "tgl"); }
static void putmenu_n2(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "numbox"); }
static void putmenu_vsl(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "vslider"); }
static void putmenu_hsl(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "hslider"); }
static void putmenu_vrad(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "vradio"); }
static void putmenu_hrad(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "hradio"); }
static void putmenu_graph(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "graph"); }
static void putmenu_array(GtkApplicationWindow *win, void *zz, gpointer *data)
    { winmenu_simpleitem(win, data, "array"); }


static void gfx_canvas_addaction(t_canvas *x, GtkWidget *win, const char *name,
    GCallback fn)
{
    GSimpleAction *action = g_simple_action_new(name, NULL);
    g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(action));
    g_signal_connect(action, "activate", fn, x);
}

t_canvas *gfx_canvas_new(const char *tag,
    int xloc, int yloc, int width, int height, int editmode)
{
    GtkEventController *ctl_motion;
    GtkEventController *ctl_key;
    GtkGesture *press_left;
    GSimpleAction *act_save, *act_saveas, *act_close, *action;
    GtkWidget *win;

    t_canvas *x = (t_canvas *)calloc(1, sizeof(*x));
    x->c_n = 0;
    x->c_vec = (t_item *)calloc(0, sizeof(*x->c_vec));
    x->c_editmode = editmode;
    x->c_seltextindex = -1;
    strncpy(x->c_tag, tag, 80);
    x->c_tag[79] = 0;
    x->c_window = (win = gtk_application_window_new(
        (GtkApplication *)pdgtk_thisapp()));
    gtk_window_set_title(GTK_WINDOW(x->c_window), "Pure Data");
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(x->c_window),
        TRUE);

    g_signal_connect(x->c_window, "destroy", G_CALLBACK(gfx_window_destroy), x);

    x->c_frame = gtk_frame_new (NULL);
    gtk_window_set_child(GTK_WINDOW(x->c_window), x->c_frame);

    x->c_drawing_area = gtk_drawing_area_new();

    gtk_widget_set_size_request(x->c_drawing_area, width, height);

    gtk_frame_set_child(GTK_FRAME(x->c_frame), x->c_drawing_area);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(x->c_drawing_area),
        (GtkDrawingAreaDrawFunc)gfx_window_draw, x, NULL);

    g_signal_connect_after(x->c_drawing_area, "resize",
        G_CALLBACK(gfx_window_resize), x);

    ctl_motion = gtk_event_controller_motion_new();
    gtk_widget_add_controller(x->c_drawing_area,
        GTK_EVENT_CONTROLLER(ctl_motion));
    g_signal_connect(ctl_motion, "motion", G_CALLBACK(gfx_window_motion), x);

    ctl_key = gtk_event_controller_key_new();
    gtk_widget_add_controller(x->c_drawing_area, GTK_EVENT_CONTROLLER(ctl_key));
    g_signal_connect(ctl_key, "key-pressed", G_CALLBACK(gfx_keypress), x);
    g_signal_connect(ctl_key, "key-released", G_CALLBACK(gfx_keyrelease), x);

    press_left = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(press_left),
        GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(x->c_drawing_area,
        GTK_EVENT_CONTROLLER(press_left));
    g_signal_connect(press_left, "pressed", G_CALLBACK(gfx_pressed), x);
    g_signal_connect(press_left, "released", G_CALLBACK(gfx_released), x);

    gtk_window_present(GTK_WINDOW(x->c_window));

    gtk_widget_set_focusable(x->c_drawing_area, TRUE);
    if (gtk_widget_grab_focus(x->c_drawing_area) != TRUE)
        fprintf(stderr, "grab focus failed\n");

    act_save = g_simple_action_new("zsave", NULL);
    g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(act_save));
    g_signal_connect(act_save, "activate", G_CALLBACK(winmenu_save), x);

    act_saveas = g_simple_action_new("zsaveas", NULL);
    g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(act_saveas));
    g_signal_connect(act_saveas, "activate", G_CALLBACK(winmenu_saveas), x);

    act_close = g_simple_action_new("zclose", NULL);
    g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(act_close));
    g_signal_connect(act_close, "activate", G_CALLBACK(winmenu_close), x);

        /* -------------- edit menu ---------------- */
    gfx_canvas_addaction(x, win, "zundo", G_CALLBACK(editmenu_undo));
    gfx_canvas_addaction(x, win, "zredo", G_CALLBACK(editmenu_redo));
    gfx_canvas_addaction(x, win, "zcut", G_CALLBACK(editmenu_cut));
    gfx_canvas_addaction(x, win, "zcopy", G_CALLBACK(editmenu_copy));
    gfx_canvas_addaction(x, win, "zpaste", G_CALLBACK(editmenu_paste));
    gfx_canvas_addaction(x, win, "zduplicate", G_CALLBACK(editmenu_dup));
    gfx_canvas_addaction(x, win, "zedit", G_CALLBACK(editmenu_edit));

        /* -------------- put menu ---------------- */
    gfx_canvas_addaction(x, win, "zputobj", G_CALLBACK(putmenu_obj));
    gfx_canvas_addaction(x, win, "zputmsg", G_CALLBACK(putmenu_msg));
    gfx_canvas_addaction(x, win, "zputnum", G_CALLBACK(putmenu_num));
    gfx_canvas_addaction(x, win, "zputlist", G_CALLBACK(putmenu_list));
    gfx_canvas_addaction(x, win, "zputtext", G_CALLBACK(putmenu_text));

    gfx_canvas_addaction(x, win, "zputbang", G_CALLBACK(putmenu_bng));
    gfx_canvas_addaction(x, win, "zputtgl",  G_CALLBACK(putmenu_tgl));
    gfx_canvas_addaction(x, win, "zputn2",   G_CALLBACK(putmenu_n2));
    gfx_canvas_addaction(x, win, "zputvsl",  G_CALLBACK(putmenu_vsl));
    gfx_canvas_addaction(x, win, "zputhsl",  G_CALLBACK(putmenu_hsl));
    gfx_canvas_addaction(x, win, "zputvrad", G_CALLBACK(putmenu_vrad));
    gfx_canvas_addaction(x, win, "zputhrad", G_CALLBACK(putmenu_hrad));
    gfx_canvas_addaction(x, win, "zputgraph",  G_CALLBACK(putmenu_graph));
    gfx_canvas_addaction(x, win, "zputarray",  G_CALLBACK(putmenu_array));

    x->c_next = gfx_canvas_list;
    gfx_canvas_list = x;

    return (x);
}

