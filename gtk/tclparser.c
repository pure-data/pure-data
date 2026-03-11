/*
    tclparser - takes tcl messages and adapts them to call into canvas.c
    and/or other gtk client source files
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <tcl.h>
#include "pdgtk.h"

/******************** tcl stuff ***********************/

static Tcl_Interp *tcl_interp;
static Tcl_HashTable tcl_windowlist;
static Tcl_HashTable tcl_canvaslist;
int tcl_debug;

/* widget commands to the window, e.g. :
    .x8101610 configure -cursor $cursor_runmode_nothing */

static int cmd_window(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    t_canvas *x = (t_canvas *)cdata;
    return (TCL_OK);
}

    /* get a {}-contained tag out of a string of them. */
static int cmd_get_tag(char *s, char *result, char **next)
{
    char *bracket1 = strchr(s, '{'), *bracket2 = strchr(s, '}');
    if (!bracket1 || !bracket2 || bracket2 <= bracket1 ||
        bracket2 > bracket1 + 78)
            return (0);
    strncpy(result, bracket1+1, bracket2 - bracket1);
    result[(bracket2 - bracket1) - 1] = 0;
    if (next)
        *next = bracket2 + 1;
    return (1);
}

/* widget commands to the canvas, e.g. :
    .x39ca5610.c create rectangle 101 14 108 16
        -tags {{.x39ca5610.t39d1b7f0i0} {inlet} } -fill black ; */

    /* note: these "widget commands" should be replaced with global functions
    such as pdtk_canvas_create_line below.  This will require replacing them
    in the "real" tcl code as well. */
static int cmd_canvas(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    t_canvas *x = (t_canvas *)cdata;
        /* "coords" - change coordinates of an exiting object, e,g,
          .x2ba85450.c coords {.x2ba85450.t2baef6f0R} 124 30 175 30 */
    if (objc >= 7 && !strcmp(Tcl_GetString(objv[1]), "coords"))
    {
        int npoints = (objc - 3)/2, dashed, i;
        double *coords = (double *)alloca(2 * npoints * sizeof(*coords));
        /* fprintf(stderr, "coords: tag %s\n", Tcl_GetString(objv[2])); */
        for (i = 0; i < 2 * npoints; i++)
            Tcl_GetDouble(interp, Tcl_GetString(objv[3+i]), &coords[i]);
        gfx_canvas_coords(x, Tcl_GetString(objv[2]), npoints, coords);
        return (TCL_OK);
    }
        /* "move" - displace a text object, e.g.,
          .xa737450.c move {.xa737450.ta7a16f0} -5 0 */
    if (objc == 5 && !strcmp(Tcl_GetString(objv[1]), "move"))
    {
        double dx, dy;
        Tcl_GetDouble(interp, Tcl_GetString(objv[3]), &dx);
        Tcl_GetDouble(interp, Tcl_GetString(objv[4]), &dy);
        gfx_canvas_move(x, Tcl_GetString(objv[2]), dx, dy);
        return (TCL_OK);
    }
        /* debugging - print out the list of canvas items */
    else if (objc >= 3 && !strcmp(Tcl_GetString(objv[1]), "raise") &&
         !strcmp(Tcl_GetString(objv[2]), "cord"))
    {
        /* this was to keep patch cords above everything else - LATER figure out
        a better way to do that. */
    }
        /* debugging - print out the list of canvas items */
    else if (objc >= 2 && !strcmp(Tcl_GetString(objv[1]), "spew"))
        gfx_canvas_spew(x);
    else if (objc >= 2)
    {
        fprintf(stderr, "%s: unknown canvas cmd\n", Tcl_GetString(objv[1]));
        return (TCL_OK);
    }
    return (TCL_OK);
}

    /* as in: pdtk_text_new .x39ca5610.c
        {{.x39ca5610.t39d1b7f0} {atom} {text} } 103 17 {0} 11 black ; */
static int cmd_pdtk_text_new(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    Tcl_HashEntry *hash;
    if (objc != 8)
        fprintf(stderr, "obj count %d, not 8\n", objc);
    else if (!(hash = Tcl_FindHashEntry(&tcl_canvaslist,
        Tcl_GetString(objv[1]))))
            fprintf(stderr, "cmd_pdtk_text_new: canvas %s not found\n",
                Tcl_GetString(objv[1]));
    else
    {
        t_canvas *c = (t_canvas *)Tcl_GetHashValue(hash);
        double px, py, fontsize;
        char tag[80], purpose[80], *endtag;
        int blue;
        if (!cmd_get_tag(Tcl_GetString(objv[2]), tag, &endtag) ||
            !cmd_get_tag(endtag, purpose, 0))
        {
            fprintf(stderr, "cmd_pdtk_text_new tags parsing failed: %s\n",
                Tcl_GetString(objv[2]));
            return (TCL_ERROR);
        }
        Tcl_GetDouble(interp, Tcl_GetString(objv[3]), &px);
        Tcl_GetDouble(interp, Tcl_GetString(objv[4]), &py);
        Tcl_GetDouble(interp, Tcl_GetString(objv[6]), &fontsize);
        blue = strcmp(Tcl_GetString(objv[7]), "black");
        gfx_canvas_addtext(c, tag, purpose, Tcl_GetString(objv[5]), px, py,
            fontsize, blue);
    }
    return (TCL_OK);
}

    /* as in: pdtk_text_set <canvas> <tag> {text} ; */
static int cmd_pdtk_text_set(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    Tcl_HashEntry *hash;
    if (objc != 4)
        fprintf(stderr, "obj count %d, not 5\n", objc);
    else if (!(hash = Tcl_FindHashEntry(&tcl_canvaslist,
        Tcl_GetString(objv[1]))))
            fprintf(stderr, "cmd_pdtk_text_set: canvas %s not found\n",
                Tcl_GetString(objv[1]));
    else
    {
        t_canvas *c = (t_canvas *)Tcl_GetHashValue(hash);
        gfx_canvas_text_set(c, Tcl_GetString(objv[2]), Tcl_GetString(objv[3]));
    }
    return (TCL_OK);
}

 /* cmd_pdtk_canvas_create_line
    <canvas> <tag> <dashed> <width> <color> <coords...> */
static int cmd_pdtk_canvas_do_create_line(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[], int patchline)
{
    Tcl_HashEntry *hash;
    if (objc < 10 || (objc & 1))
    {
        fprintf(stderr, "pdtk_canvas_do_create_line: bad #args = %d\n", objc);
        return (TCL_ERROR);
    }
    else if (!(hash = Tcl_FindHashEntry(&tcl_canvaslist,
        Tcl_GetString(objv[1]))))
            fprintf(stderr, "pdtk_canvas_do_create_line: canvas %s not found\n",
                Tcl_GetString(objv[1]));
    else
    {
        t_canvas *c = (t_canvas *)Tcl_GetHashValue(hash);
        int npoints = (objc - 5)/2, dashed, i;
        double *coords = (double *)alloca(2 * npoints * sizeof(*coords)), width;
        char *tag, purpose[80], *endtag, *color;
        dashed = *Tcl_GetString(objv[3]);   /* nonempty -> dashed */
        Tcl_GetDouble(interp, Tcl_GetString(objv[4]), &width);
        color = Tcl_GetString(objv[5]);
        tag = Tcl_GetString(objv[2]);
        for (i = 0; i < 2 * npoints; i++)
            Tcl_GetDouble(interp, Tcl_GetString(objv[6+i]), &coords[i]);
        dashed = strcmp(Tcl_GetString(objv[1]), "");
        gfx_canvas_addpath(c, tag, "x", dashed, width, npoints, coords,
            patchline);
    }
    return (TCL_OK);
}

 /* cmd_pdtk_canvas_create_line
    <canvas> <tag> <dashed> <width> <color> <coords...> */
static int cmd_pdtk_canvas_create_line(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    cmd_pdtk_canvas_do_create_line(cdata, interp, objc, objv, 0);
}

 /* cmd_pdtk_canvas_create_patchcord
    <canvas> <tag> <dashed> <width> <color> <coords...> */
static int cmd_pdtk_canvas_create_patchcord(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    cmd_pdtk_canvas_do_create_line(cdata, interp, objc, objv, 1);
}


 /* cmd_pdtk_canvas_create_rect
    <canvas> <tag> <width> <color> x1 y1 x2 y2 */
static int cmd_pdtk_canvas_create_rect(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    Tcl_HashEntry *hash;
    if (objc != 9)
    {
        fprintf(stderr, "pdtk_canvas_create_rect: bad #args = %d\n", objc);
        return (TCL_ERROR);
    }
    else if (!(hash = Tcl_FindHashEntry(&tcl_canvaslist,
        Tcl_GetString(objv[1]))))
            fprintf(stderr, "pdtk_canvas_create_line: canvas %s not found\n",
                Tcl_GetString(objv[1]));
    else
    {
        t_canvas *c = (t_canvas *)Tcl_GetHashValue(hash);
        double x1, y1, x2, y2, width;
        Tcl_GetDouble(interp, Tcl_GetString(objv[5]), &x1);
        Tcl_GetDouble(interp, Tcl_GetString(objv[6]), &y1);
        Tcl_GetDouble(interp, Tcl_GetString(objv[7]), &x2);
        Tcl_GetDouble(interp, Tcl_GetString(objv[8]), &y2);
        Tcl_GetDouble(interp, Tcl_GetString(objv[3]), &width);
        gfx_canvas_addrectangle(c, Tcl_GetString(objv[2]),
            "", width, x1, y1, x2, y2);
    }
    return (TCL_OK);
}


 /* cmd_pdtk_canvas_delete <canvas> <tag> */
static int cmd_pdtk_canvas_delete(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    Tcl_HashEntry *hash;
    if (objc != 3)
    {
        fprintf(stderr, "pdtk_canvas_delete: bad #args = %d\n", objc);
        return (TCL_ERROR);
    }
    else if (!(hash = Tcl_FindHashEntry(&tcl_canvaslist,
        Tcl_GetString(objv[1]))))
            fprintf(stderr, "pdtk_canvas_delete: canvas %s not found\n",
                Tcl_GetString(objv[1]));
    else
    {
        t_canvas *c = (t_canvas *)Tcl_GetHashValue(hash);
        gfx_canvas_delete(c, Tcl_GetString(objv[2]));
    }
    return (TCL_OK);
}

static int cmd_pdtk_canvas_new(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    char *tag, *geom;
    int xloc, yloc, width, height, editmode, isnew;
    char canvasname[80];
    char returnmsg[80];
    Tcl_HashEntry *windowhash, *canvashash;
    t_canvas *c;

    if (objc != 6)
    {
        fprintf(stderr, "pdtk_canvas_new: needs 6 args, got %d\n", objc);
        return (TCL_ERROR);
    }
    tag = Tcl_GetString(objv[1]);
    snprintf(canvasname, 80, "%s.c", tag);
    canvasname[79] = 0;

    width = atoi(Tcl_GetString(objv[2]));
    height = atoi(Tcl_GetString(objv[3]));
    geom = Tcl_GetString(objv[4]);
    editmode = atoi(Tcl_GetString(objv[5]));
    if (sscanf(geom, "+%d+%d", &xloc, &yloc) < 2)
    {
        fprintf(stderr, "geom (%s)?\n", geom);
        return (TCL_ERROR);
    }
    fprintf(stderr, "cmd_pdtk_canvas_new %s %d %d %d %d %d\n",
        tag, xloc, yloc, width, height, editmode);

    if (Tcl_FindHashEntry(&tcl_windowlist, tag) ||
        Tcl_FindHashEntry(&tcl_canvaslist, canvasname))
    {
        fprintf(stderr, "%s or %s already exists\n", tag, canvasname);
        return (TCL_ERROR);
    }
    windowhash = Tcl_CreateHashEntry(&tcl_windowlist, tag, &isnew);
    canvashash = Tcl_CreateHashEntry(&tcl_canvaslist, canvasname, &isnew);

    c = gfx_canvas_new(tag, xloc, yloc, width, height, editmode);
    Tcl_SetHashValue(windowhash, (ClientData)c);
    Tcl_SetHashValue(canvashash, (ClientData)c);

    fprintf(stderr, "create commmands %s ... %s\n", tag, canvasname);
     Tcl_CreateObjCommand(tcl_interp, tag, cmd_window,
        (ClientData)c, NULL);
    Tcl_CreateObjCommand(tcl_interp, canvasname, cmd_canvas,
        (ClientData)c, NULL);

    sprintf(returnmsg, "%s map 1;\n", tag);
    socket_send(returnmsg);

    return (TCL_OK);
}

static int cmd_destroy(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    char *tag;
    Tcl_HashEntry *hash;
    if (objc != 2)
    {
        fprintf(stderr, "destroy: needs 2 args, got %d\n", objc);
        return (TCL_ERROR);
    }
    tag = Tcl_GetString(objv[1]);
    if (!(hash = Tcl_FindHashEntry(&tcl_windowlist, tag)))
        fprintf(stderr, "destroy: window %s not found\n", tag);
    else
    {
        t_canvas *c = (t_canvas *)Tcl_GetHashValue(hash);
        fprintf(stderr, "destroying %s\n", tag);
        gfx_canvas_free(c);
        Tcl_DeleteHashEntry(hash);
    }
    return (TCL_OK);
}

 /* pdtk_canvas_reflecttitle <tag> <dir> <filename> <args> <dirty>
 e.g., pdtk_canvas_reflecttitle .x2f403240 {/tmp} {z2.pd} {} 0 */
static int cmd_pdtk_canvas_reflecttitle(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    char *tag;
    Tcl_HashEntry *hash;
    char title[BIGSTRING + 1];
    title[BIGSTRING] = 0;
    if (objc != 6)
    {
        fprintf(stderr, "destroy: needs 2 args, got %d\n", objc);
        return (TCL_ERROR);
    }
    tag = Tcl_GetString(objv[1]);
    if (!(hash = Tcl_FindHashEntry(&tcl_windowlist, tag)))
        fprintf(stderr, "destroy: window %s not found\n", tag);
    else
    {
        t_canvas *c = (t_canvas *)Tcl_GetHashValue(hash);
        snprintf(title, BIGSTRING, "%s%c%s - %s",
            Tcl_GetString(objv[3]),
            (atoi(Tcl_GetString(objv[5])) ? '*' : ' '),
            Tcl_GetString(objv[4]), Tcl_GetString(objv[2]));
#ifdef DEBUGTCL
        fprintf(stderr, "set title for %s: %s\n", tag, title);
#endif
        gfx_canvas_settitle(c, title);
    }
    return (TCL_OK);
}



 /* pdtk_ping - flow control between pd and gui */
static int cmd_pdtk_ping(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    socket_send("pd ping;\n");
}

typedef int (*t_tcl_creatorfn)(ClientData cdata, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[]);

typedef struct _tcl_entry
{
    char *e_name;
    t_tcl_creatorfn e_fn;
} t_tcl_entry;

static t_tcl_entry tcl_knowncommands[] = {
    {"pdtk_canvas_new", cmd_pdtk_canvas_new},
    {"destroy", cmd_destroy},
    {"pdtk_text_new", cmd_pdtk_text_new},
    {"pdtk_text_set", cmd_pdtk_text_set},
    {"pdtk_canvas_reflecttitle", cmd_pdtk_canvas_reflecttitle},
    {"pdtk_canvas_create_line", cmd_pdtk_canvas_create_line},
    {"pdtk_canvas_create_patchcord", cmd_pdtk_canvas_create_patchcord},
    {"pdtk_canvas_create_rect", cmd_pdtk_canvas_create_rect},
    {"pdtk_canvas_delete", cmd_pdtk_canvas_delete},
    {"pdtk_ping", cmd_pdtk_ping},
    {"set", 0},
};

static int tcl_isknowncommand(const char *s)
{
    int i;
    char head[81];

        /* command is delimited eithr by ' ' or ';' */
    if (sscanf(s, "%80s", head) < 1)
        return (0);
    head[80] = 0;
    if (strchr(head, ';'))
        *(strchr(head, ';')) = 0;
    for (i = 0; i < sizeof(tcl_knowncommands)/sizeof(*tcl_knowncommands); i++)
        if (!strcmp(head, tcl_knowncommands[i].e_name))
            return (1);
    if (sscanf(s, "%79s", head) >= 1)
    {
        if (Tcl_FindHashEntry(&tcl_windowlist, head) ||
            Tcl_FindHashEntry(&tcl_canvaslist, head))
                return (1);
    }
    return (0);
}

void tcl_runcommand(char *s)
{
    if (tcl_isknowncommand(s))
    {
        int rc;
        if (tcl_debug)
        {
            fprintf(stderr, "pdgtk: %s", (s[0] == '\n' ? s+1 : s));
            if (s[strlen(s)-1] != '\n')
                fprintf(stderr, "\n");
        }
        rc = Tcl_Eval(tcl_interp, s);
        /* if (rc != TCL_OK)
            fprintf(stderr, "tcl eval error\n"); */
    }
    else
    {
        if (strlen(s) > 40)
            strncpy(s+35, " ...", 5);
        if (s[0] == '\n')
            s++;
        if (s[strlen(s)-1] == '\n')
            s[strlen(s)-1] = 0;
        fprintf(stderr, "unknown command %s\n", s);
    }
}

int tcl_init(void)
{
    int rc, i;

    tcl_interp = Tcl_CreateInterp();
    Tcl_InitHashTable(&tcl_windowlist, TCL_STRING_KEYS);
    Tcl_InitHashTable(&tcl_canvaslist, TCL_STRING_KEYS);

    if (tcl_interp == NULL)
    {
        fprintf(stderr, "Could not create interpreter!\n");
        return (1);
    }
    if (Tcl_InitStubs(tcl_interp, TCL_VERSION, 0) == NULL)
    {
        fprintf(stderr, "Tcl_InitStubs failed\n");
        return (1);
    }
    for (i = 0; i < sizeof(tcl_knowncommands)/sizeof(*tcl_knowncommands); i++)
        if (tcl_knowncommands[i].e_fn)
            Tcl_CreateObjCommand(tcl_interp, tcl_knowncommands[i].e_name,
                tcl_knowncommands[i].e_fn,  (ClientData)NULL, NULL);

    rc = Tcl_Eval(tcl_interp, "puts stderr \"started tcl interpreter\"\n");
    if (rc != TCL_OK)
    {
        fprintf(stderr, "Error 1\n");
        return (1);
    }
    return (0);
}
