/* Copyright (c) 1997-1999 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* historically this file defined the qlist and textfile objects - at the
moment it also defines "text" but it may later be better to split this off. */

#include "m_pd.h"
#include "g_canvas.h"    /* just for glist_getfont, bother */
#include "s_stuff.h"    /* just for sys_hostfontsize, phooey */
#include <string.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif
extern t_pd *newest;    /* OK - this should go into a .h file now :) */
static t_class *text_define_class;

#ifdef _WIN32
# include <malloc.h> /* MSVC or mingw on windows */
#elif defined(__linux__) || defined(__APPLE__)
# include <alloca.h> /* linux, mac, mingw, cygwin */
#else
# include <stdlib.h> /* BSDs for example */
#endif

#ifndef HAVE_ALLOCA     /* can work without alloca() but we never need it */
#define HAVE_ALLOCA 1
#endif
#define TEXT_NGETBYTE 100 /* bigger that this we use alloc, not alloca */
#if HAVE_ALLOCA
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)((n) < TEXT_NGETBYTE ?  \
        alloca((n) * sizeof(t_atom)) : getbytes((n) * sizeof(t_atom))))
#define ATOMS_FREEA(x, n) ( \
    ((n) < TEXT_NGETBYTE || (freebytes((x), (n) * sizeof(t_atom)), 0)))
#else
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)getbytes((n) * sizeof(t_atom)))
#define ATOMS_FREEA(x, n) (freebytes((x), (n) * sizeof(t_atom)))
#endif

/* --- common code for text define, textfile, and qlist for storing text -- */

typedef struct _textbuf
{
    t_object b_ob;
    t_binbuf *b_binbuf;
    t_canvas *b_canvas;
    t_guiconnect *b_guiconnect;
} t_textbuf;

static void textbuf_init(t_textbuf *x)
{
    x->b_binbuf = binbuf_new();
    x->b_canvas = canvas_getcurrent();
}

static void textbuf_senditup(t_textbuf *x)
{
    int i, ntxt;
    char *txt;
    if (!x->b_guiconnect)
        return;
    binbuf_gettext(x->b_binbuf, &txt, &ntxt);
    sys_vgui("pdtk_textwindow_clear .x%lx\n", x);
    for (i = 0; i < ntxt; )
    {
        char *j = strchr(txt+i, '\n');
        if (!j) j = txt + ntxt;
        sys_vgui("pdtk_textwindow_append .x%lx {%.*s\n}\n",
            x, j-txt-i, txt+i);
        i = (j-txt)+1;
    }
    sys_vgui("pdtk_textwindow_setdirty .x%lx 0\n", x);
    t_freebytes(txt, ntxt);
}

static void textbuf_open(t_textbuf *x)
{
    if (x->b_guiconnect)
    {
        sys_vgui("wm deiconify .x%lx\n", x);
        sys_vgui("raise .x%lx\n", x);
        sys_vgui("focus .x%lx.text\n", x);
    }
    else
    {
        char buf[40];
        sys_vgui("pdtk_textwindow_open .x%lx %dx%d {%s: %s} %d\n",
            x, 600, 340, "myname", "text",
                 sys_hostfontsize(glist_getfont(x->b_canvas),
                    glist_getzoom(x->b_canvas)));
        sprintf(buf, ".x%lx", (unsigned long)x);
        x->b_guiconnect = guiconnect_new(&x->b_ob.ob_pd, gensym(buf));
        textbuf_senditup(x);
    }
}

static void textbuf_close(t_textbuf *x)
{
    sys_vgui("pdtk_textwindow_doclose .x%lx\n", x);
    if (x->b_guiconnect)
    {
        guiconnect_notarget(x->b_guiconnect, 1000);
        x->b_guiconnect = 0;
    }
}

static void textbuf_addline(t_textbuf *b, t_symbol *s, int argc, t_atom *argv)
{
    t_binbuf *z = binbuf_new();
    binbuf_restore(z, argc, argv);
    binbuf_add(b->b_binbuf, binbuf_getnatom(z), binbuf_getvec(z));
    binbuf_free(z);
    textbuf_senditup(b);
}

static void textbuf_read(t_textbuf *x, t_symbol *s, int argc, t_atom *argv)
{
    int cr = 0;
    t_symbol *filename;
    while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(argv->a_w.w_symbol->s_name, "-c"))
            cr = 1;
        else
        {
            pd_error(x, "text read: unknown flag ...");
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc && argv->a_type == A_SYMBOL)
    {
        filename = argv->a_w.w_symbol;
        argc--; argv++;
    }
    else
    {
        pd_error(x, "text read: no file name given");
        return;
    }
    if (argc)
    {
        post("warning: text define ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    if (binbuf_read_via_canvas(x->b_binbuf, filename->s_name, x->b_canvas, cr))
            pd_error(x, "%s: read failed", filename->s_name);
    textbuf_senditup(x);
}

static void textbuf_write(t_textbuf *x, t_symbol *s, int argc, t_atom *argv)
{
    int cr = 0;
    t_symbol *filename;
    char buf[MAXPDSTRING];
    while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(argv->a_w.w_symbol->s_name, "-c"))
            cr = 1;
        else
        {
            pd_error(x, "text write: unknown flag ...");
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc && argv->a_type == A_SYMBOL)
    {
        filename = argv->a_w.w_symbol;
        argc--; argv++;
    }
    else
    {
        pd_error(x, "text write: no file name given");
        return;
    }
    if (argc)
    {
        post("warning: text define ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    canvas_makefilename(x->b_canvas, filename->s_name,
        buf, MAXPDSTRING);
    if (binbuf_write(x->b_binbuf, buf, "", cr))
            pd_error(x, "%s: write failed", filename->s_name);
}

static void textbuf_free(t_textbuf *x)
{
    t_pd *x2;
    binbuf_free(x->b_binbuf);
    if (x->b_guiconnect)
    {
        sys_vgui("destroy .x%lx\n", x);
        guiconnect_notarget(x->b_guiconnect, 1000);
    }
        /* just in case we're still bound to #A from loading... */
    while ((x2 = pd_findbyclass(gensym("#A"), text_define_class)))
        pd_unbind(x2, gensym("#A"));
}

    /* random helper function */
static int text_nthline(int n, t_atom *vec, int line, int *startp, int *endp)
{
    int i, cnt = 0;
    for (i = 0; i < n; i++)
    {
        if (cnt == line)
        {
            int j = i, outc, k;
            while (j < n && vec[j].a_type != A_SEMI &&
                vec[j].a_type != A_COMMA)
                    j++;
            *startp = i;
            *endp = j;
            return (1);
        }
        else if (vec[i].a_type == A_SEMI || vec[i].a_type == A_COMMA)
            cnt++;
    }
    return (0);
}

/* text_define object - text buffer, accessible by other accessor objects */

typedef struct _text_define
{
    t_textbuf x_textbuf;
    t_outlet *x_out;
    t_symbol *x_bindsym;
    t_scalar *x_scalar;     /* faux scalar (struct text-scalar) to point to */
    t_gpointer x_gp;        /* pointer to it */
    t_canvas *x_canvas;     /* owning canvas whose stub we use for x_gp */
    unsigned char x_keep;   /* whether to embed contents in patch on save */
} t_text_define;

#define x_ob x_textbuf.b_ob
#define x_binbuf x_textbuf.b_binbuf
#define x_canvas x_textbuf.b_canvas

static void *text_define_new(t_symbol *s, int argc, t_atom *argv)
{
    t_text_define *x = (t_text_define *)pd_new(text_define_class);
    t_symbol *asym = gensym("#A");
    x->x_keep = 0;
    x->x_bindsym = &s_;
    while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(argv->a_w.w_symbol->s_name, "-k"))
            x->x_keep = 1;
        else
        {
            pd_error(x, "text define: unknown flag ...");
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc && argv->a_type == A_SYMBOL)
    {
        pd_bind(&x->x_ob.ob_pd, argv->a_w.w_symbol);
        x->x_bindsym = argv->a_w.w_symbol;
        argc--; argv++;
    }
    if (argc)
    {
        post("warning: text define ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    textbuf_init(&x->x_textbuf);
        /* set up a scalar and a pointer to it that we can output */
    x->x_scalar = scalar_new(canvas_getcurrent(), gensym("pd-text"));
    binbuf_free(x->x_scalar->sc_vec[2].w_binbuf);
    x->x_scalar->sc_vec[2].w_binbuf = x->x_binbuf;
    x->x_out = outlet_new(&x->x_ob, &s_pointer);
    gpointer_init(&x->x_gp);
    x->x_canvas = canvas_getcurrent();
           /* bashily unbind #A -- this would create garbage if #A were
           multiply bound but we believe in this context it's at most
           bound to whichever text_define or array was created most recently */
    asym->s_thing = 0;
        /* and now bind #A to us to receive following messages in the
        saved file or copy buffer */
    pd_bind(&x->x_ob.ob_pd, asym);
    return (x);
}

static void text_define_clear(t_text_define *x)
{
    binbuf_clear(x->x_binbuf);
    textbuf_senditup(&x->x_textbuf);
}

    /* from g_traversal.c - maybe put in a header? */
t_binbuf *pointertobinbuf(t_pd *x, t_gpointer *gp, t_symbol *s,
    const char *fname);

    /* these are unused; they copy text from this object to and from a text
        field in a scalar. */
static void text_define_frompointer(t_text_define *x, t_gpointer *gp,
    t_symbol *s)
{
    t_binbuf *b = pointertobinbuf(&x->x_textbuf.b_ob.ob_pd,
        gp, s, "text_frompointer");
    if (b)
    {
        t_gstub *gs = gp->gp_stub;
        binbuf_clear(x->x_textbuf.b_binbuf);
        binbuf_add(x->x_textbuf.b_binbuf, binbuf_getnatom(b), binbuf_getvec(b));
    }
}

static void text_define_topointer(t_text_define *x, t_gpointer *gp, t_symbol *s)
{
    t_binbuf *b = pointertobinbuf(&x->x_textbuf.b_ob.ob_pd,
        gp, s, "text_topointer");
    if (b)
    {
        t_gstub *gs = gp->gp_stub;
        binbuf_clear(b);
        binbuf_add(b, binbuf_getnatom(x->x_textbuf.b_binbuf),
            binbuf_getvec(x->x_textbuf.b_binbuf));
        if (gs->gs_which == GP_GLIST)
            scalar_redraw(gp->gp_un.gp_scalar, gs->gs_un.gs_glist);
        else
        {
            t_array *owner_array = gs->gs_un.gs_array;
            while (owner_array->a_gp.gp_stub->gs_which == GP_ARRAY)
                owner_array = owner_array->a_gp.gp_stub->gs_un.gs_array;
            scalar_redraw(owner_array->a_gp.gp_un.gp_scalar,
                owner_array->a_gp.gp_stub->gs_un.gs_glist);
        }
    }
}

    /* bang: output a pointer to a struct containing this text */
void text_define_bang(t_text_define *x)
{
    gpointer_setglist(&x->x_gp, x->x_canvas, x->x_scalar);
    outlet_pointer(x->x_out, &x->x_gp);
}

    /* set from a list */
void text_define_set(t_text_define *x, t_symbol *s, int argc, t_atom *argv)
{
    binbuf_restore(x->x_binbuf, argc, argv);
    textbuf_senditup(&x->x_textbuf);
}


static void text_define_save(t_gobj *z, t_binbuf *bb)
{
    t_text_define *x = (t_text_define *)z;
    binbuf_addv(bb, "ssff", &s__X, gensym("obj"),
        (float)x->x_ob.te_xpix, (float)x->x_ob.te_ypix);
    binbuf_addbinbuf(bb, x->x_ob.ob_binbuf);
    binbuf_addsemi(bb);
    if (x->x_keep)
    {
        binbuf_addv(bb, "ss", gensym("#A"), gensym("set"));
        binbuf_addbinbuf(bb, x->x_binbuf);
        binbuf_addsemi(bb);
    }
    obj_saveformat(&x->x_ob, bb);
}

static void text_define_free(t_text_define *x)
{
    textbuf_free(&x->x_textbuf);
    if (x->x_bindsym != &s_)
        pd_unbind(&x->x_ob.ob_pd, x->x_bindsym);
    gpointer_unset(&x->x_gp);
}

/* ---  text_client - common code for objects that refer to text buffers -- */

typedef struct _text_client
{
    t_object tc_obj;
    t_symbol *tc_sym;
    t_gpointer tc_gp;
    t_symbol *tc_struct;
    t_symbol *tc_field;
} t_text_client;

    /* parse buffer-finding arguments */
static void text_client_argparse(t_text_client *x, int *argcp, t_atom **argvp,
    char *name)
{
    int argc = *argcp;
    t_atom *argv = *argvp;
    x->tc_sym = x->tc_struct = x->tc_field = 0;
    gpointer_init(&x->tc_gp);
    while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(argv->a_w.w_symbol->s_name, "-s") &&
            argc >= 3 && argv[1].a_type == A_SYMBOL && argv[2].a_type == A_SYMBOL)
        {
            x->tc_struct = canvas_makebindsym(argv[1].a_w.w_symbol);
            x->tc_field = argv[2].a_w.w_symbol;
            argc -= 2; argv += 2;
        }
        else
        {
            pd_error(x, "%s: unknown flag '%s'...", name,
                argv->a_w.w_symbol->s_name);
        }
        argc--; argv++;
    }
    if (argc && argv->a_type == A_SYMBOL)
    {
        if (x->tc_struct)
            pd_error(x, "%s: extra names after -s..", name);
        else x->tc_sym = argv->a_w.w_symbol;
        argc--; argv++;
    }
    *argcp = argc;
    *argvp = argv;
}

    /* find the binbuf for this object.  This should be reusable for other
    objects.  Prints an error  message and returns 0 on failure. */
static t_binbuf *text_client_getbuf(t_text_client *x)
{
    if (x->tc_sym)       /* named text object */
    {
        t_textbuf *y = (t_textbuf *)pd_findbyclass(x->tc_sym,
            text_define_class);
        if (y)
            return (y->b_binbuf);
        else
        {
            pd_error(x, "text: couldn't find text buffer '%s'",
                x->tc_sym->s_name);
            return (0);
        }
    }
    else if (x->tc_struct)   /* by pointer */
    {
        t_template *template = template_findbyname(x->tc_struct);
        t_gstub *gs = x->tc_gp.gp_stub;
        t_word *vec;
        int onset, type;
        t_symbol *arraytype;
        if (!template)
        {
            pd_error(x, "text: couldn't find struct %s", x->tc_struct->s_name);
            return (0);
        }
        if (!gpointer_check(&x->tc_gp, 0))
        {
            pd_error(x, "text: stale or empty pointer");
            return (0);
        }
        if (gs->gs_which == GP_ARRAY)
            vec = x->tc_gp.gp_un.gp_w;
        else vec = x->tc_gp.gp_un.gp_scalar->sc_vec;

        if (!template_find_field(template,
            x->tc_field, &onset, &type, &arraytype))
        {
            pd_error(x, "text: no field named %s", x->tc_field->s_name);
            return (0);
        }
        if (type != DT_TEXT)
        {
            pd_error(x, "text: field %s not of type text", x->tc_field->s_name);
            return (0);
        }
        return (*(t_binbuf **)(((char *)vec) + onset));
    }
    else return (0);    /* shouldn't happen */
}

static  void text_client_senditup(t_text_client *x)
{
    if (x->tc_sym)       /* named text object */
    {
        t_textbuf *y = (t_textbuf *)pd_findbyclass(x->tc_sym,
            text_define_class);
        if (y)
            textbuf_senditup(y);
        else bug("text_client_senditup");
    }
    else if (x->tc_struct)   /* by pointer */
    {
        t_template *template = template_findbyname(x->tc_struct);
        t_gstub *gs = x->tc_gp.gp_stub;
        if (!template)
        {
            pd_error(x, "text: couldn't find struct %s", x->tc_struct->s_name);
            return;
        }
        if (!gpointer_check(&x->tc_gp, 0))
        {
            pd_error(x, "text: stale or empty pointer");
            return;
        }
        if (gs->gs_which == GP_GLIST)
            scalar_redraw(x->tc_gp.gp_un.gp_scalar, gs->gs_un.gs_glist);
        else
        {
            t_array *owner_array = gs->gs_un.gs_array;
            while (owner_array->a_gp.gp_stub->gs_which == GP_ARRAY)
                owner_array = owner_array->a_gp.gp_stub->gs_un.gs_array;
            scalar_redraw(owner_array->a_gp.gp_un.gp_scalar,
                owner_array->a_gp.gp_stub->gs_un.gs_glist);
        }
    }
}

static void text_client_free(t_text_client *x)
{
    gpointer_unset(&x->tc_gp);
}

/* ------- text_get object - output all or part of nth lines ----------- */
t_class *text_get_class;

typedef struct _text_get
{
    t_text_client x_tc;
    t_outlet *x_out1;       /* list */
    t_outlet *x_out2;       /* 1 if comma terminated, 0 if semi, 2 if none */
    t_float x_f1;           /* field number, -1 for whole line */
    t_float x_f2;           /* number of fields */
} t_text_get;

#define x_obj x_tc.tc_obj
#define x_sym x_tc.tc_sym
#define x_gp x_tc.tc_gp
#define x_struct x_tc.tc_struct
#define x_field x_tc.tc_field

static void *text_get_new(t_symbol *s, int argc, t_atom *argv)
{
    t_text_get *x = (t_text_get *)pd_new(text_get_class);
    x->x_out1 = outlet_new(&x->x_obj, &s_list);
    x->x_out2 = outlet_new(&x->x_obj, &s_float);
    floatinlet_new(&x->x_obj, &x->x_f1);
    floatinlet_new(&x->x_obj, &x->x_f2);
    x->x_f1 = -1;
    x->x_f2 = 1;
    text_client_argparse(&x->x_tc, &argc, &argv, "text get");
    if (argc)
    {
        if (argv->a_type == A_FLOAT)
            x->x_f1 = argv->a_w.w_float;
        else
        {
            post("text get: can't understand field number");
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc)
    {
        if (argv->a_type == A_FLOAT)
            x->x_f2 = argv->a_w.w_float;
        else
        {
            post("text get: can't understand field count");
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc)
    {
        post("warning: text get ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    if (x->x_struct)
        pointerinlet_new(&x->x_obj, &x->x_gp);
    else symbolinlet_new(&x->x_obj, &x->x_tc.tc_sym);
    return (x);
}

static void text_get_float(t_text_get *x, t_floatarg f)
{
    t_binbuf *b = text_client_getbuf(&x->x_tc);
    int start, end, n, startfield, nfield;
    t_atom *vec;
    if (!b)
       return;
    vec = binbuf_getvec(b);
    n = binbuf_getnatom(b);
    startfield = x->x_f1;
    nfield = x->x_f2;
    if (text_nthline(n, vec, f, &start, &end))
    {
        int outc = end - start, k;
        t_atom *outv;
        if (x->x_f1 < 0)    /* negative start field for whole line */
        {
                /* tell us what terminated the line (semi or comma) */
            outlet_float(x->x_out2, (end < n && vec[end].a_type == A_COMMA));
            ATOMS_ALLOCA(outv, outc);
            for (k = 0; k < outc; k++)
                outv[k] = vec[start+k];
            outlet_list(x->x_out1, 0, outc, outv);
            ATOMS_FREEA(outv, outc);
        }
        else if (startfield + nfield > outc)
            pd_error(x, "text get: field request (%d %d) out of range",
                startfield, nfield);
        else
        {
            ATOMS_ALLOCA(outv, nfield);
            for (k = 0; k < nfield; k++)
                outv[k] = vec[(start+startfield)+k];
            outlet_list(x->x_out1, 0, nfield, outv);
            ATOMS_FREEA(outv, nfield);
        }
    }
    else if (x->x_f1 < 0)   /* whole line but out of range: empty list and 2 */
    {
        outlet_float(x->x_out2, 2);         /* 2 for out of range */
        outlet_list(x->x_out1, 0, 0, 0);    /* ... and empty list */
    }
}

/* --------- text_set object - replace all or part of nth line ----------- */
typedef struct _text_set
{
    t_text_client x_tc;
    t_float x_f1;           /* line number */
    t_float x_f2;           /* field number, -1 for whole line */
} t_text_set;

t_class *text_set_class;

static void *text_set_new(t_symbol *s, int argc, t_atom *argv)
{
    t_text_set *x = (t_text_set *)pd_new(text_set_class);
    floatinlet_new(&x->x_obj, &x->x_f1);
    floatinlet_new(&x->x_obj, &x->x_f2);
    x->x_f1 = 0;
    x->x_f2 = -1;
    text_client_argparse(&x->x_tc, &argc, &argv, "text set");
    if (argc)
    {
        if (argv->a_type == A_FLOAT)
            x->x_f1 = argv->a_w.w_float;
        else
        {
            post("text set: can't understand field number");
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc)
    {
        if (argv->a_type == A_FLOAT)
            x->x_f2 = argv->a_w.w_float;
        else
        {
            post("text set: can't understand field count");
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc)
    {
        post("warning: text set ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    if (x->x_struct)
        pointerinlet_new(&x->x_obj, &x->x_gp);
    else symbolinlet_new(&x->x_obj, &x->x_tc.tc_sym);
    return (x);
}

static void text_set_list(t_text_set *x,
    t_symbol *s, int argc, t_atom *argv)
{
    t_binbuf *b = text_client_getbuf(&x->x_tc);
    int start, end, n, lineno = x->x_f1, fieldno = x->x_f2, i;
    t_atom *vec;
    if (!b)
       return;
    vec = binbuf_getvec(b);
    n = binbuf_getnatom(b);
    if (lineno < 0)
    {
        pd_error(x, "text set: line number (%d) < 0", lineno);
        return;
    }
    if (text_nthline(n, vec, lineno, &start, &end))
    {
        if (fieldno < 0)
        {
            if (end - start != argc)  /* grow or shrink */
            {
                int oldn = n;
                n = n + (argc - (end-start));
                if (n > oldn)
                    (void)binbuf_resize(b, n);
                vec = binbuf_getvec(b);
                memmove(&vec[start + argc], &vec[end],
                    sizeof(*vec) * (oldn - end));
                if (n < oldn)
                {
                    (void)binbuf_resize(b, n);
                    vec = binbuf_getvec(b);
                }
            }
        }
        else
        {
            if (fieldno >= end - start)
            {
                pd_error(x, "text set: field number (%d) past end of line",
                    fieldno);
                return;
            }
            if (fieldno + argc > end - start)
                argc = (end - start) - fieldno;
            start += fieldno;
        }
    }
    else if (fieldno < 0)  /* if line number too high just append to end */
    {
        int addsemi = (n && vec[n-1].a_type != A_SEMI &&
            vec[n-1].a_type != A_COMMA), newsize = n + addsemi + argc + 1;
        (void)binbuf_resize(b, newsize);
        vec = binbuf_getvec(b);
        if (addsemi)
            SETSEMI(&vec[n]);
        SETSEMI(&vec[newsize-1]);
        start = n+addsemi;
    }
    else
    {
        post("text set: %d: line number out of range", lineno);
        return;
    }
    for (i = 0; i < argc; i++)
    {
        if (argv[i].a_type == A_POINTER)
            SETSYMBOL(&vec[start+i], gensym("(pointer)"));
        else vec[start+i] = argv[i];
    }
    text_client_senditup(&x->x_tc);
}

/* --------- text_delete object - delete nth line ----------- */
typedef struct _text_delete
{
    t_text_client x_tc;
} t_text_delete;

t_class *text_delete_class;

static void *text_delete_new(t_symbol *s, int argc, t_atom *argv)
{
    t_text_delete *x = (t_text_delete *)pd_new(text_delete_class);
    text_client_argparse(&x->x_tc, &argc, &argv, "text delete");
    if (argc)
    {
        post("warning: text delete ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    if (x->x_struct)
        pointerinlet_new(&x->x_obj, &x->x_gp);
    else symbolinlet_new(&x->x_obj, &x->x_tc.tc_sym);
    return (x);
}

static void text_delete_float(t_text_delete *x, t_floatarg f)
{
    t_binbuf *b = text_client_getbuf(&x->x_tc);
    int start, end, n, lineno = f;
    t_atom *vec;
    if (!b)
       return;
    vec = binbuf_getvec(b);
    n = binbuf_getnatom(b);
    if (lineno < 0)
        binbuf_clear(b);
    else if (text_nthline(n, vec, lineno, &start, &end))
    {
        if (end < n)
            end++;
        memmove(&vec[start], &vec[end], sizeof(*vec) * (n - end));
        (void)binbuf_resize(b, n - (end - start));
    }
    else
    {
        post("text delete: %d: line number out of range", lineno);
        return;
    }
    text_client_senditup(&x->x_tc);
}

/* ---------------- text_size object - output number of lines -------------- */
t_class *text_size_class;

typedef struct _text_size
{
    t_text_client x_tc;
    t_outlet *x_out1;       /* float */
} t_text_size;

static void *text_size_new(t_symbol *s, int argc, t_atom *argv)
{
    t_text_size *x = (t_text_size *)pd_new(text_size_class);
    x->x_out1 = outlet_new(&x->x_obj, &s_float);
    text_client_argparse(&x->x_tc, &argc, &argv, "text size");
    if (argc)
    {
        post("warning: text size ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    if (x->x_struct)
        pointerinlet_new(&x->x_obj, &x->x_gp);
    else symbolinlet_new(&x->x_obj, &x->x_tc.tc_sym);
    return (x);
}

static void text_size_bang(t_text_size *x)
{
    t_binbuf *b = text_client_getbuf(&x->x_tc);
    int n, i, cnt = 0;
    t_atom *vec;
    if (!b)
       return;
    vec = binbuf_getvec(b);
    n = binbuf_getnatom(b);
    for (i = 0; i < n; i++)
    {
        if (vec[i].a_type == A_SEMI || vec[i].a_type == A_COMMA)
            cnt++;
    }
    if (n && vec[n-1].a_type != A_SEMI && vec[n-1].a_type != A_COMMA)
        cnt++;
    outlet_float(x->x_out1, cnt);
}

static void text_size_float(t_text_size *x, t_floatarg f)
{
    t_binbuf *b = text_client_getbuf(&x->x_tc);
    int start, end, n;
    t_atom *vec;
    if (!b)
       return;
    vec = binbuf_getvec(b);
    n = binbuf_getnatom(b);
    if (text_nthline(n, vec, f, &start, &end))
        outlet_float(x->x_out1, end-start);
    else outlet_float(x->x_out1, -1);
}

/* ---------------- text_tolist object - output text as a list ----------- */
t_class *text_tolist_class;

#define t_text_tolist t_text_client

static void *text_tolist_new(t_symbol *s, int argc, t_atom *argv)
{
    t_text_tolist *x = (t_text_tolist *)pd_new(text_tolist_class);
    outlet_new(&x->tc_obj, &s_list);
    text_client_argparse(x, &argc, &argv, "text tolist");
    if (argc)
    {
        post("warning: text tolist ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    if (x->tc_struct)
        pointerinlet_new(&x->tc_obj, &x->tc_gp);
    else symbolinlet_new(&x->tc_obj, &x->tc_sym);
    return (x);
}

static void text_tolist_bang(t_text_tolist *x)
{
    t_binbuf *b = text_client_getbuf(x), *b2;
    int n, i, cnt = 0;
    t_atom *vec;
    if (!b)
       return;
    b2 = binbuf_new();
    binbuf_addbinbuf(b2, b);
    outlet_list(x->tc_obj.ob_outlet, 0, binbuf_getnatom(b2), binbuf_getvec(b2));
    binbuf_free(b2);
}

/* ------------- text_fromlist object - set text from a list -------- */
t_class *text_fromlist_class;

#define t_text_fromlist t_text_client

static void *text_fromlist_new(t_symbol *s, int argc, t_atom *argv)
{
    t_text_fromlist *x = (t_text_fromlist *)pd_new(text_fromlist_class);
    text_client_argparse(x, &argc, &argv, "text fromlist");
    if (argc)
    {
        post("warning: text fromlist ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    if (x->tc_struct)
        pointerinlet_new(&x->tc_obj, &x->tc_gp);
    else symbolinlet_new(&x->tc_obj, &x->tc_sym);
    return (x);
}

static void text_fromlist_list(t_text_fromlist *x,
    t_symbol *s, int argc, t_atom *argv)
{
    t_binbuf *b = text_client_getbuf(x);
    if (!b)
       return;
    binbuf_clear(b);
    binbuf_restore(b, argc, argv);
    text_client_senditup(x);
}

/* ---- text_search object - output index of line(s) matching criteria ---- */

t_class *text_search_class;

    /* relations we can test when searching: */
#define KB_EQ 0     /*  equal */
#define KB_GT 1     /* > (etc..) */
#define KB_GE 2
#define KB_LT 3
#define KB_LE 4
#define KB_NEAR 5   /* anything matches but closer is better */

typedef struct _key
{
    int k_field;
    int k_binop;
} t_key;

typedef struct _text_search
{
    t_text_client x_tc;
    t_outlet *x_out1;       /* line indices */
    int x_nkeys;
    t_key *x_keyvec;
} t_text_search;

static void *text_search_new(t_symbol *s, int argc, t_atom *argv)
{
    t_text_search *x = (t_text_search *)pd_new(text_search_class);
    int i, key, nkey, nextop;
    x->x_out1 = outlet_new(&x->x_obj, &s_list);
    text_client_argparse(&x->x_tc, &argc, &argv, "text search");
    for (i = nkey = 0; i < argc; i++)
        if (argv[i].a_type == A_FLOAT)
            nkey++;
    if (nkey == 0)
        nkey = 1;
    x->x_nkeys = nkey;
    x->x_keyvec = (t_key *)getbytes(nkey * sizeof(*x->x_keyvec));
    if (!argc)
        x->x_keyvec[0].k_field = 0, x->x_keyvec[0].k_binop = KB_EQ;
    else for (i = key = 0, nextop = -1; i < argc; i++)
    {
        if (argv[i].a_type == A_FLOAT)
        {
            x->x_keyvec[key].k_field =
                (argv[i].a_w.w_float > 0 ? argv[i].a_w.w_float : 0);
            x->x_keyvec[key].k_binop = (nextop >= 0 ? nextop : KB_EQ);
            nextop = -1;
            key++;
        }
        else
        {
            char *s = argv[i].a_w.w_symbol->s_name;
            if (nextop >= 0)
                pd_error(x,
                    "text search: extra operation argument ignored: %s", s);
            else if (!strcmp(argv[i].a_w.w_symbol->s_name, ">"))
                nextop = KB_GT;
            else if (!strcmp(argv[i].a_w.w_symbol->s_name, ">="))
                nextop = KB_GE;
            else if (!strcmp(argv[i].a_w.w_symbol->s_name, "<"))
                nextop = KB_LT;
            else if (!strcmp(argv[i].a_w.w_symbol->s_name, "<="))
                nextop = KB_LE;
            else if (!strcmp(argv[i].a_w.w_symbol->s_name, "near"))
                nextop = KB_NEAR;
            else pd_error(x,
                    "text search: unknown operation argument: %s", s);
        }
    }
    if (x->x_struct)
        pointerinlet_new(&x->x_obj, &x->x_gp);
    else symbolinlet_new(&x->x_obj, &x->x_tc.tc_sym);
    return (x);
}

static void text_search_list(t_text_search *x,
    t_symbol *s, int argc, t_atom *argv)
{
    t_binbuf *b = text_client_getbuf(&x->x_tc);
    int i, j, n, lineno, bestline = -1, beststart=-1, bestn, thisstart, thisn,
        nkeys = x->x_nkeys, failed = 0;
    t_atom *vec;
    t_key *kp = x->x_keyvec;
    if (!b)
       return;
    if (argc < nkeys)
    {
        pd_error(x, "need %d keys, only got %d in list",
            nkeys, argc);
    }
    vec = binbuf_getvec(b);
    n = binbuf_getnatom(b);
    if (nkeys < 1)
        bug("text_search");
    for (i = lineno = thisstart = 0; i < n; i++)
    {
        if (vec[i].a_type == A_SEMI || vec[i].a_type == A_COMMA || i == n-1)
        {
            int thisn = i - thisstart, j, field = x->x_keyvec[0].k_field,
                binop = x->x_keyvec[0].k_binop;
                /* do we match? */
            for (j = 0; j < argc; )
            {
                if (field >= thisn ||
                    vec[thisstart+field].a_type != argv[j].a_type)
                        goto nomatch;
                if (argv[j].a_type == A_FLOAT)      /* arg is a float */
                {
                    switch (binop)
                    {
                        case KB_EQ:
                            if (vec[thisstart+field].a_w.w_float !=
                                argv[j].a_w.w_float)
                                    goto nomatch;
                        break;
                        case KB_GT:
                            if (vec[thisstart+field].a_w.w_float <=
                                argv[j].a_w.w_float)
                                    goto nomatch;
                        break;
                        case KB_GE:
                            if (vec[thisstart+field].a_w.w_float <
                                argv[j].a_w.w_float)
                                    goto nomatch;
                        break;
                        case KB_LT:
                            if (vec[thisstart+field].a_w.w_float >=
                                argv[j].a_w.w_float)
                                    goto nomatch;
                        break;
                        case KB_LE:
                            if (vec[thisstart+field].a_w.w_float >
                                argv[j].a_w.w_float)
                                    goto nomatch;
                        break;
                            /* the other possibility ('near') never fails */
                    }
                }
                else                                /* arg is a symbol */
                {
                    if (binop != KB_EQ)
                    {
                        if (!failed)
                        {
                            pd_error(x,
                    "text search (%s): only exact matches allowed for symbols",
                                argv[j].a_w.w_symbol->s_name);
                            failed = 1;
                        }
                        goto nomatch;
                    }
                    if (vec[thisstart+field].a_w.w_symbol !=
                        argv[j].a_w.w_symbol)
                            goto nomatch;
                }
                if (++j >= nkeys)    /* if at last key just increment field */
                    field++;
                else field = x->x_keyvec[j].k_field,    /* else next key */
                        binop = x->x_keyvec[j].k_binop;
            }
                /* the line matches.  Now, if there is a previous match, are
                we better than it? */
            if (bestline >= 0)
            {
                field = x->x_keyvec[0].k_field;
                binop = x->x_keyvec[0].k_binop;
                for (j = 0; j < argc; )
                {
                    if (field >= thisn
                        || vec[thisstart+field].a_type != argv[j].a_type)
                            bug("text search 2");
                    if (argv[j].a_type == A_FLOAT)      /* arg is a float */
                    {
                        float thisv = vec[thisstart+field].a_w.w_float,
                            bestv = (beststart >= 0 ?
                                vec[beststart+field].a_w.w_float : -1e20);
                        switch (binop)
                        {
                            case KB_GT:
                            case KB_GE:
                                if (thisv < bestv)
                                    goto replace;
                                else if (thisv > bestv)
                                    goto nomatch;
                            break;
                            case KB_LT:
                            case KB_LE:
                                if (thisv > bestv)
                                    goto replace;
                                else if (thisv < bestv)
                                    goto nomatch;
                            break;
                            case KB_NEAR:
                                if (thisv >= argv[j].a_w.w_float &&
                                    bestv >= argv[j].a_w.w_float)
                                {
                                    if (thisv < bestv)
                                        goto replace;
                                    else if (thisv > bestv)
                                        goto nomatch;
                                }
                                else if (thisv <= argv[j].a_w.w_float &&
                                    bestv <= argv[j].a_w.w_float)
                                {
                                    if (thisv > bestv)
                                        goto replace;
                                    else if (thisv < bestv)
                                        goto nomatch;
                                }
                                else
                                {
                                    float d1 = thisv - argv[j].a_w.w_float,
                                        d2 = bestv - argv[j].a_w.w_float;
                                    if (d1 < 0)
                                        d1 = -d1;
                                    if (d2 < 0)
                                        d2 = -d2;

                                    if (d1 < d2)
                                        goto replace;
                                    else if (d1 > d2)
                                        goto nomatch;
                                }
                            break;
                                /* the other possibility ('=') never decides */
                        }
                    }
                    if (++j >= nkeys)    /* last key - increment field */
                        field++;
                    else field = x->x_keyvec[j].k_field,    /* else next key */
                            binop = x->x_keyvec[j].k_binop;
                }
                goto nomatch;   /* a tie - keep the old one */
            replace:
                bestline = lineno, beststart = thisstart, bestn = thisn;
            }
                /* no previous match so we're best */
            else bestline = lineno, beststart = thisstart, bestn = thisn;
        nomatch:
            lineno++;
            thisstart = i+1;
        }
    }
    outlet_float(x->x_out1, bestline);
}

/* ---------------- text_sequence object - sequencer ----------- */
t_class *text_sequence_class;

typedef struct _text_sequence
{
    t_text_client x_tc;
    t_outlet *x_mainout;    /* outlet for lists, zero if "global" */
    t_outlet *x_waitout;    /* outlet for wait times, zero if we never wait */
    t_outlet *x_endout;    /* bang when hit end */
    int x_onset;
    int x_argc;
    t_atom *x_argv;
    t_symbol *x_waitsym;    /* symbol to initiate wait, zero if none */
    int x_waitargc;         /* how many leading numbers to use for waiting */
    t_clock *x_clock;       /* calback for auto mode */
    t_float x_nextdelay;
    t_symbol *x_lastto;     /* destination symbol if we're after a comma */
    unsigned char x_eaten;  /* true if we've eaten leading numbers already */
    unsigned char x_loop;   /* true if we can send multiple lines */
    unsigned char x_auto;   /* set timer when we hit single-number time list */
} t_text_sequence;

static void text_sequence_tick(t_text_sequence *x);
static void text_sequence_tempo(t_text_sequence *x,
    t_symbol *unitname, t_floatarg tempo);
void parsetimeunits(void *x, t_float amount, t_symbol *unitname,
    t_float *unit, int *samps); /* time unit parsing from x_time.c */

static void *text_sequence_new(t_symbol *s, int argc, t_atom *argv)
{
    t_text_sequence *x = (t_text_sequence *)pd_new(text_sequence_class);
    int global = 0;
    text_client_argparse(&x->x_tc, &argc, &argv, "text sequence");
    x->x_waitsym = 0;
    x->x_waitargc = 0;
    x->x_eaten = 0;
    x->x_loop = 0;
    x->x_lastto = 0;
    while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(argv->a_w.w_symbol->s_name, "-w") && argc >= 2)
        {
            if (argv[1].a_type == A_SYMBOL)
            {
                x->x_waitsym = argv[1].a_w.w_symbol;
                x->x_waitargc = 0;
            }
            else
            {
                x->x_waitsym = 0;
                if ((x->x_waitargc = argv[1].a_w.w_float) < 0)
                    x->x_waitargc = 0;
            }
            argc -= 1; argv += 1;
        }
        else if (!strcmp(argv->a_w.w_symbol->s_name, "-g"))
            global = 1;
        else if (!strcmp(argv->a_w.w_symbol->s_name, "-t") && argc >= 3)
        {
            text_sequence_tempo(x, atom_getsymbolarg(2, argc, argv),
                atom_getfloatarg(1, argc, argv));
             argc -= 2; argv += 2;
        }
        else
        {
            pd_error(x, "text sequence: unknown flag '%s'...",
                argv->a_w.w_symbol->s_name);
        }
        argc--; argv++;
    }
    if (argc)
    {
        post("warning: text sequence ignoring extra argument: ");
        postatom(argc, argv); endpost();
    }
    if (x->x_tc.tc_struct)
        pointerinlet_new(&x->x_tc.tc_obj, &x->x_tc.tc_gp);
    else symbolinlet_new(&x->x_tc.tc_obj, &x->x_tc.tc_sym);
    x->x_argc = 0;
    x->x_argv = (t_atom *)getbytes(0);
    x->x_onset = 0x7fffffff;
    x->x_mainout = (!global ? outlet_new(&x->x_obj, &s_list) : 0);
    x->x_waitout = (global || x->x_waitsym || x->x_waitargc ?
        outlet_new(&x->x_obj, &s_list) : 0);
    x->x_endout = outlet_new(&x->x_obj, &s_bang);
    x->x_clock = clock_new(x, (t_method)text_sequence_tick);
    if (global)
    {
        if (x->x_waitargc)
            pd_error(x,
       "warning: text sequence: numeric 'w' argument ignored if '-g' given");
        x->x_waitargc = 0x40000000;
    }
    return (x);
}

static void text_sequence_doit(t_text_sequence *x, int argc, t_atom *argv)
{
    t_binbuf *b = text_client_getbuf(&x->x_tc), *b2;
    int n, i, onset, nfield, wait, eatsemi = 1, gotcomma = 0;
    t_atom *vec, *outvec, *ap;
    if (!b)
        goto nosequence;
    vec = binbuf_getvec(b);
    n = binbuf_getnatom(b);
    if (x->x_onset >= n)
    {
    nosequence:
        x->x_onset = 0x7fffffff;
        x->x_loop = x->x_auto = 0;
        outlet_bang(x->x_endout);
        return;
    }
    onset = x->x_onset;

        /* test if leading numbers, or a leading symbol equal to our
        "wait symbol", are directing us to wait */
    if (!x->x_lastto && (
        (vec[onset].a_type == A_FLOAT && x->x_waitargc && !x->x_eaten) ||
            (vec[onset].a_type == A_SYMBOL &&
                vec[onset].a_w.w_symbol == x->x_waitsym)))
    {
        if (vec[onset].a_type == A_FLOAT)
        {
            for (i = onset; i < n && i < onset + x->x_waitargc &&
                vec[i].a_type == A_FLOAT; i++)
                    ;
            x->x_eaten = 1;
            eatsemi = 0;
        }
        else
        {
            for (i = onset; i < n && vec[i].a_type != A_SEMI &&
                vec[i].a_type != A_COMMA; i++)
                    ;
            x->x_eaten = 1;
            onset++;    /* symbol isn't part of wait list */
        }
        wait = 1;
    }
    else    /* message to send */
    {
        for (i = onset; i < n && vec[i].a_type != A_SEMI &&
            vec[i].a_type != A_COMMA; i++)
                ;
        wait = 0;
        x->x_eaten = 0;
        if (i < n && vec[i].a_type == A_COMMA)
            gotcomma = 1;
    }
    nfield = i - onset;
    i += eatsemi;
    if (i >= n)
        i = 0x7fffffff;
    x->x_onset = i;
        /* generate output list, realizing dolar sign atoms.  Allocate one
        extra atom in case we want to prepend a symbol later */
    ATOMS_ALLOCA(outvec, nfield+1);
    for (i = 0, ap = vec+onset; i < nfield; i++, ap++)
    {
        int type = ap->a_type;
        if (type == A_FLOAT || type == A_SYMBOL)
            outvec[i] = *ap;
        else if (type == A_DOLLAR)
        {
            int atno = ap->a_w.w_index-1;
            if (atno < 0 || atno >= argc)
            {
                pd_error(x, "argument $%d out of range", atno+1);
                SETFLOAT(outvec+i, 0);
            }
            else outvec[i] = argv[atno];
        }
        else if (type == A_DOLLSYM)
        {
            t_symbol *s =
                binbuf_realizedollsym(ap->a_w.w_symbol, argc, argv, 0);
            if (s)
                SETSYMBOL(outvec+i, s);
            else
            {
                error("$%s: not enough arguments supplied",
                    ap->a_w.w_symbol->s_name);
                SETSYMBOL(outvec+i, &s_symbol);
            }
        }
        else bug("text sequence");
    }
    if (wait)
    {
        x->x_loop = 0;
        x->x_lastto = 0;
        if (x->x_auto && nfield == 1 && outvec[0].a_type == A_FLOAT)
            x->x_nextdelay = outvec[0].a_w.w_float;
        else if (!x->x_waitout)
            bug("text sequence 3");
        else
        {
            x->x_auto = 0;
            outlet_list(x->x_waitout, 0, nfield, outvec);
        }
    }
    else if (x->x_mainout)
    {
        int n2 = nfield;
        if (x->x_lastto)
        {
            memmove(outvec+1, outvec, nfield * sizeof(*outvec));
            SETSYMBOL(outvec, x->x_lastto);
            n2++;
        }
        if (!gotcomma)
            x->x_lastto = 0;
        else if (!x->x_lastto && nfield && outvec->a_type == A_SYMBOL)
            x->x_lastto = outvec->a_w.w_symbol;
        outlet_list(x->x_mainout, 0, n2, outvec);
    }
    else if (nfield)
    {
        t_symbol *tosym = x->x_lastto;
        t_pd *to = 0;
        t_atom *vecleft = outvec;
        int nleft = nfield;
        if (!tosym)
        {
            if (outvec[0].a_type != A_SYMBOL)
                bug("text sequence 2");
            else tosym = outvec[0].a_w.w_symbol;
            vecleft++;
            nleft--;
        }
        if (tosym)
        {
            if (!(to = tosym->s_thing))
                pd_error(x, "%s: no such object", tosym->s_name);
        }
        x->x_lastto = (gotcomma ? tosym : 0);
        if (to)
        {
            if (nleft > 0 && vecleft[0].a_type == A_SYMBOL)
                typedmess(to, vecleft->a_w.w_symbol, nleft-1, vecleft+1);
            else pd_list(to, 0, nleft, vecleft);
        }
    }
    ATOMS_FREEA(outvec, nfield+1);
}

static void text_sequence_list(t_text_sequence *x, t_symbol *s, int argc,
    t_atom *argv)
{
    x->x_loop = 1;
    while (x->x_loop)
    {
        if (argc)
            text_sequence_doit(x, argc, argv);
        else text_sequence_doit(x, x->x_argc, x->x_argv);
    }
}

static void text_sequence_stop(t_text_sequence *x)
{
    x->x_loop = 0;
    if (x->x_auto)
    {
        clock_unset(x->x_clock);
        x->x_auto = 0;
    }
}

static void text_sequence_tick(t_text_sequence *x)  /* clock callback */
{
    x->x_lastto = 0;
    while (x->x_auto)
    {
        x->x_loop = 1;
        while (x->x_loop)
            text_sequence_doit(x, x->x_argc, x->x_argv);
        if (x->x_nextdelay > 0)
            break;
    }
    if (x->x_auto)
        clock_delay(x->x_clock, x->x_nextdelay);
}

static void text_sequence_auto(t_text_sequence *x)
{
    x->x_lastto = 0;
    if (x->x_auto)
        clock_unset(x->x_clock);
    x->x_auto = 1;
    text_sequence_tick(x);
}

static void text_sequence_step(t_text_sequence *x)
{
    text_sequence_stop(x);
    text_sequence_doit(x, x->x_argc, x->x_argv);
}

static void text_sequence_line(t_text_sequence *x, t_floatarg f)
{
    t_binbuf *b = text_client_getbuf(&x->x_tc), *b2;
    int n, start, end;
    t_atom *vec;
    if (!b)
       return;
    x->x_lastto = 0;
    vec = binbuf_getvec(b);
    n = binbuf_getnatom(b);
    if (!text_nthline(n, vec, f, &start, &end))
    {
        pd_error(x, "text sequence: line number %d out of range", (int)f);
        x->x_onset = 0x7fffffff;
    }
    else x->x_onset = start;
    x->x_eaten = 0;
}

static void text_sequence_args(t_text_sequence *x, t_symbol *s,
    int argc, t_atom *argv)
{
    int i;
    x->x_argv = t_resizebytes(x->x_argv,
        x->x_argc * sizeof(t_atom), argc * sizeof(t_atom));
    for (i = 0; i < argc; i++)
        x->x_argv[i] = argv[i];
    x->x_argc = argc;
}

static void text_sequence_tempo(t_text_sequence *x,
    t_symbol *unitname, t_floatarg tempo)
{
    t_float unit;
    int samps;
    parsetimeunits(x, tempo, unitname, &unit, &samps);
    clock_setunit(x->x_clock, unit, samps);
}

static void text_sequence_free(t_text_sequence *x)
{
    t_freebytes(x->x_argv, sizeof(t_atom) * x->x_argc);
    clock_free(x->x_clock);
    text_client_free(&x->x_tc);
}

/* --- overall creator for "text" objects: dispatch to "text define" etc --- */
static void *text_new(t_symbol *s, int argc, t_atom *argv)
{
    if (!argc || argv[0].a_type != A_SYMBOL)
        newest = text_define_new(s, argc, argv);
    else
    {
        char *str = argv[0].a_w.w_symbol->s_name;
        if (!strcmp(str, "d") || !strcmp(str, "define"))
            newest = text_define_new(s, argc-1, argv+1);
        else if (!strcmp(str, "get"))
            newest = text_get_new(s, argc-1, argv+1);
        else if (!strcmp(str, "set"))
            newest = text_set_new(s, argc-1, argv+1);
        else if (!strcmp(str, "delete"))
            newest = text_delete_new(s, argc-1, argv+1);
        else if (!strcmp(str, "size"))
            newest = text_size_new(s, argc-1, argv+1);
        else if (!strcmp(str, "tolist"))
            newest = text_tolist_new(s, argc-1, argv+1);
        else if (!strcmp(str, "fromlist"))
            newest = text_fromlist_new(s, argc-1, argv+1);
        else if (!strcmp(str, "search"))
            newest = text_search_new(s, argc-1, argv+1);
        else if (!strcmp(str, "sequence"))
            newest = text_sequence_new(s, argc-1, argv+1);
        else
        {
            error("list %s: unknown function", str);
            newest = 0;
        }
    }
    return (newest);
}

/*  the qlist and textfile objects, as of 0.44, are 'derived' from
* the text object above.  Maybe later it will be desirable to add new
* functionality to textfile; qlist is an ancient holdover (1987) and
* is probably best left alone.
*/

typedef struct _qlist
{
    t_textbuf x_textbuf;
    t_outlet *x_bangout;
    int x_onset;                /* playback position */
    t_clock *x_clock;
    t_float x_tempo;
    double x_whenclockset;
    t_float x_clockdelay;
    int x_rewound;          /* we've been rewound since last start */
    int x_innext;           /* we're currently inside the "next" routine */
} t_qlist;
#define x_ob x_textbuf.b_ob
#define x_binbuf x_textbuf.b_binbuf
#define x_canvas x_textbuf.b_canvas

static void qlist_tick(t_qlist *x);

static t_class *qlist_class;

static void *qlist_new( void)
{
    t_qlist *x = (t_qlist *)pd_new(qlist_class);
    textbuf_init(&x->x_textbuf);
    x->x_clock = clock_new(x, (t_method)qlist_tick);
    outlet_new(&x->x_ob, &s_list);
    x->x_bangout = outlet_new(&x->x_ob, &s_bang);
    x->x_onset = 0x7fffffff;
    x->x_tempo = 1;
    x->x_whenclockset = 0;
    x->x_clockdelay = 0;
    x->x_rewound = x->x_innext = 0;
    return (x);
}

static void qlist_rewind(t_qlist *x)
{
    x->x_onset = 0;
    if (x->x_clock) clock_unset(x->x_clock);
    x->x_whenclockset = 0;
    x->x_rewound = 1;
}

static void qlist_donext(t_qlist *x, int drop, int automatic)
{
    t_pd *target = 0;
    if (x->x_innext)
    {
        pd_error(x, "qlist sent 'next' from within itself");
        return;
    }
    x->x_innext = 1;
    while (1)
    {
        int argc = binbuf_getnatom(x->x_binbuf),
            count, onset = x->x_onset, onset2, wasrewound;
        t_atom *argv = binbuf_getvec(x->x_binbuf);
        t_atom *ap = argv + onset, *ap2;
        if (onset >= argc) goto end;
        while (ap->a_type == A_SEMI || ap->a_type == A_COMMA)
        {
            if (ap->a_type == A_SEMI) target = 0;
            onset++, ap++;
            if (onset >= argc) goto end;
        }

        if (!target && ap->a_type == A_FLOAT)
        {
            ap2 = ap + 1;
            onset2 = onset + 1;
            while (onset2 < argc && ap2->a_type == A_FLOAT)
                onset2++, ap2++;
            x->x_onset = onset2;
            if (automatic)
            {
                clock_delay(x->x_clock,
                    x->x_clockdelay = ap->a_w.w_float * x->x_tempo);
                x->x_whenclockset = clock_getsystime();
            }
            else outlet_list(x->x_ob.ob_outlet, 0, onset2-onset, ap);
            x->x_innext = 0;
            return;
        }
        ap2 = ap + 1;
        onset2 = onset + 1;
        while (onset2 < argc &&
            (ap2->a_type == A_FLOAT || ap2->a_type == A_SYMBOL))
                onset2++, ap2++;
        x->x_onset = onset2;
        count = onset2 - onset;
        if (!target)
        {
            if (ap->a_type != A_SYMBOL) continue;
            else if (!(target = ap->a_w.w_symbol->s_thing))
            {
                pd_error(x, "qlist: %s: no such object",
                    ap->a_w.w_symbol->s_name);
                continue;
            }
            ap++;
            onset++;
            count--;
            if (!count)
            {
                x->x_onset = onset2;
                continue;
            }
        }
        wasrewound = x->x_rewound;
        x->x_rewound = 0;
        if (!drop)
        {
            if (ap->a_type == A_FLOAT)
                typedmess(target, &s_list, count, ap);
            else if (ap->a_type == A_SYMBOL)
                typedmess(target, ap->a_w.w_symbol, count-1, ap+1);
        }
        if (x->x_rewound)
        {
            x->x_innext = 0;
            return;
        }
        x->x_rewound = wasrewound;
    }  /* while (1); never falls through */

end:
    x->x_onset = 0x7fffffff;
    x->x_whenclockset = 0;
    x->x_innext = 0;
    outlet_bang(x->x_bangout);
}

static void qlist_next(t_qlist *x, t_floatarg drop)
{
    qlist_donext(x, drop != 0, 0);
}

static void qlist_bang(t_qlist *x)
{
    qlist_rewind(x);
        /* if we're restarted reentrantly from a "next" message set ourselves
        up to do this non-reentrantly after a delay of 0 */
    if (x->x_innext)
    {
        x->x_whenclockset = clock_getsystime();
        x->x_clockdelay = 0;
        clock_delay(x->x_clock, 0);
    }
    else qlist_donext(x, 0, 1);
}

static void qlist_tick(t_qlist *x)
{
    x->x_whenclockset = 0;
    qlist_donext(x, 0, 1);
}

static void qlist_add(t_qlist *x, t_symbol *s, int argc, t_atom *argv)
{
    t_atom a;
    SETSEMI(&a);
    binbuf_add(x->x_binbuf, argc, argv);
    binbuf_add(x->x_binbuf, 1, &a);
}

static void qlist_add2(t_qlist *x, t_symbol *s, int argc, t_atom *argv)
{
    binbuf_add(x->x_binbuf, argc, argv);
}

static void qlist_clear(t_qlist *x)
{
    qlist_rewind(x);
    binbuf_clear(x->x_binbuf);
}

static void qlist_set(t_qlist *x, t_symbol *s, int argc, t_atom *argv)
{
    qlist_clear(x);
    qlist_add(x, s, argc, argv);
}

static void qlist_read(t_qlist *x, t_symbol *filename, t_symbol *format)
{
    int cr = 0;
    if (!strcmp(format->s_name, "cr"))
        cr = 1;
    else if (*format->s_name)
        pd_error(x, "qlist_read: unknown flag: %s", format->s_name);

    if (binbuf_read_via_canvas(x->x_binbuf, filename->s_name, x->x_canvas, cr))
            pd_error(x, "%s: read failed", filename->s_name);
    x->x_onset = 0x7fffffff;
    x->x_rewound = 1;
}

static void qlist_write(t_qlist *x, t_symbol *filename, t_symbol *format)
{
    int cr = 0;
    char buf[MAXPDSTRING];
    canvas_makefilename(x->x_canvas, filename->s_name,
        buf, MAXPDSTRING);
    if (!strcmp(format->s_name, "cr"))
        cr = 1;
    else if (*format->s_name)
        pd_error(x, "qlist_read: unknown flag: %s", format->s_name);
    if (binbuf_write(x->x_binbuf, buf, "", cr))
            pd_error(x, "%s: write failed", filename->s_name);
}

static void qlist_print(t_qlist *x)
{
    post("--------- textfile or qlist contents: -----------");
    binbuf_print(x->x_binbuf);
}

static void qlist_tempo(t_qlist *x, t_float f)
{
    t_float newtempo;
    if (f < 1e-20) f = 1e-20;
    else if (f > 1e20) f = 1e20;
    newtempo = 1./f;
    if (x->x_whenclockset != 0)
    {
        t_float elapsed = clock_gettimesince(x->x_whenclockset);
        t_float left = x->x_clockdelay - elapsed;
        if (left < 0) left = 0;
        left *= newtempo / x->x_tempo;
        clock_delay(x->x_clock, left);
    }
    x->x_tempo = newtempo;
}

static void qlist_free(t_qlist *x)
{
    textbuf_free(&x->x_textbuf);
    clock_free(x->x_clock);
}

/* -------------------- textfile ------------------------------- */

/* has the same struct as qlist (so we can reuse some of its
* methods) but "sequencing" here only relies on 'binbuf' and 'onset'
* fields.
*/

static t_class *textfile_class;

static void *textfile_new( void)
{
    t_qlist *x = (t_qlist *)pd_new(textfile_class);
    textbuf_init(&x->x_textbuf);
    outlet_new(&x->x_ob, &s_list);
    x->x_bangout = outlet_new(&x->x_ob, &s_bang);
    x->x_onset = 0x7fffffff;
    x->x_rewound = 0;
    x->x_tempo = 1;
    x->x_whenclockset = 0;
    x->x_clockdelay = 0;
    x->x_clock = NULL;
    return (x);
}

static void textfile_bang(t_qlist *x)
{
    int argc = binbuf_getnatom(x->x_binbuf),
        count, onset = x->x_onset, onset2;
    t_atom *argv = binbuf_getvec(x->x_binbuf);
    t_atom *ap = argv + onset, *ap2;
    while (onset < argc &&
        (ap->a_type == A_SEMI || ap->a_type == A_COMMA))
            onset++, ap++;
    onset2 = onset;
    ap2 = ap;
    while (onset2 < argc &&
        (ap2->a_type != A_SEMI && ap2->a_type != A_COMMA))
            onset2++, ap2++;
    if (onset2 > onset)
    {
        x->x_onset = onset2;
        if (ap->a_type == A_SYMBOL)
            outlet_anything(x->x_ob.ob_outlet, ap->a_w.w_symbol,
                onset2-onset-1, ap+1);
        else outlet_list(x->x_ob.ob_outlet, 0, onset2-onset, ap);
    }
    else
    {
        x->x_onset = 0x7fffffff;
        outlet_bang(x->x_bangout);
    }
}

static void textfile_rewind(t_qlist *x)
{
    x->x_onset = 0;
}

/* ---------------- global setup function -------------------- */

static t_pd *text_templatecanvas;
static char text_templatefile[] = "\
canvas 0 0 458 153 10;\n\
#X obj 43 31 struct text float x float y text t;\n\
";

/* create invisible, built-in canvas to supply template containing one text
field named 't'.  I don't know how to make this not break
pre-0.45 patches using templates named 'text'... perhaps this is a minor
enough incompatibility that I'll just get away with it. */

static void text_template_init( void)
{
    t_binbuf *b;
    if (text_templatecanvas)
        return;
    b = binbuf_new();

    glob_setfilename(0, gensym("_text_template"), gensym("."));
    binbuf_text(b, text_templatefile, strlen(text_templatefile));
    binbuf_eval(b, &pd_canvasmaker, 0, 0);
    vmess(s__X.s_thing, gensym("pop"), "i", 0);

    glob_setfilename(0, &s_, &s_);
    binbuf_free(b);
}

void x_qlist_setup(void )
{
    text_template_init();
    text_define_class = class_new(gensym("text define"),
        (t_newmethod)text_define_new,
        (t_method)text_define_free, sizeof(t_text_define), 0, A_GIMME, 0);
    class_addmethod(text_define_class, (t_method)textbuf_open,
        gensym("click"), 0);
    class_addmethod(text_define_class, (t_method)textbuf_close,
        gensym("close"), 0);
    class_addmethod(text_define_class, (t_method)textbuf_addline,
        gensym("addline"), A_GIMME, 0);
    class_addmethod(text_define_class, (t_method)text_define_set,
        gensym("set"), A_GIMME, 0);
    class_addmethod(text_define_class, (t_method)text_define_clear,
        gensym("clear"), 0);
    class_addmethod(text_define_class, (t_method)textbuf_write,
        gensym("write"), A_GIMME, 0);
    class_addmethod(text_define_class, (t_method)textbuf_read,
        gensym("read"), A_GIMME, 0);
    class_setsavefn(text_define_class, text_define_save);
    class_addbang(text_define_class, text_define_bang);
    class_sethelpsymbol(text_define_class, gensym("text-object"));

    class_addcreator((t_newmethod)text_new, gensym("text"), A_GIMME, 0);

    text_get_class = class_new(gensym("text get"),
        (t_newmethod)text_get_new, (t_method)text_client_free,
            sizeof(t_text_get), 0, A_GIMME, 0);
    class_addfloat(text_get_class, text_get_float);
    class_sethelpsymbol(text_get_class, gensym("text-object"));

    text_set_class = class_new(gensym("text set"),
        (t_newmethod)text_set_new, (t_method)text_client_free,
            sizeof(t_text_set), 0, A_GIMME, 0);
    class_addlist(text_set_class, text_set_list);
    class_sethelpsymbol(text_set_class, gensym("text-object"));

    text_delete_class = class_new(gensym("text delete"),
        (t_newmethod)text_delete_new, (t_method)text_client_free,
            sizeof(t_text_delete), 0, A_GIMME, 0);
    class_addfloat(text_delete_class, text_delete_float);
    class_sethelpsymbol(text_delete_class, gensym("text-object"));

    text_size_class = class_new(gensym("text size"),
        (t_newmethod)text_size_new, (t_method)text_client_free,
            sizeof(t_text_size), 0, A_GIMME, 0);
    class_addbang(text_size_class, text_size_bang);
    class_addfloat(text_size_class, text_size_float);
    class_sethelpsymbol(text_size_class, gensym("text-object"));

    text_tolist_class = class_new(gensym("text tolist"),
        (t_newmethod)text_tolist_new, (t_method)text_client_free,
            sizeof(t_text_tolist), 0, A_GIMME, 0);
    class_addbang(text_tolist_class, text_tolist_bang);
    class_sethelpsymbol(text_tolist_class, gensym("text-object"));

    text_fromlist_class = class_new(gensym("text fromlist"),
        (t_newmethod)text_fromlist_new, (t_method)text_client_free,
            sizeof(t_text_fromlist), 0, A_GIMME, 0);
    class_addlist(text_fromlist_class, text_fromlist_list);
    class_sethelpsymbol(text_fromlist_class, gensym("text-object"));

    text_search_class = class_new(gensym("text search"),
        (t_newmethod)text_search_new, (t_method)text_client_free,
            sizeof(t_text_search), 0, A_GIMME, 0);
    class_addlist(text_search_class, text_search_list);
    class_sethelpsymbol(text_search_class, gensym("text-object"));

    text_sequence_class = class_new(gensym("text sequence"),
        (t_newmethod)text_sequence_new, (t_method)text_sequence_free,
            sizeof(t_text_sequence), 0, A_GIMME, 0);
    class_addmethod(text_sequence_class, (t_method)text_sequence_step,
        gensym("step"), 0);
    class_addmethod(text_sequence_class, (t_method)text_sequence_line,
        gensym("line"), A_FLOAT, 0);
    class_addmethod(text_sequence_class, (t_method)text_sequence_auto,
        gensym("auto"), 0);
    class_addmethod(text_sequence_class, (t_method)text_sequence_stop,
        gensym("stop"), 0);
    class_addmethod(text_sequence_class, (t_method)text_sequence_args,
        gensym("args"), A_GIMME, 0);
    class_addmethod(text_sequence_class, (t_method)text_sequence_tempo,
        gensym("tempo"), A_FLOAT, A_SYMBOL, 0);
    class_addlist(text_sequence_class, text_sequence_list);
    class_sethelpsymbol(text_sequence_class, gensym("text-object"));

    qlist_class = class_new(gensym("qlist"), (t_newmethod)qlist_new,
        (t_method)qlist_free, sizeof(t_qlist), 0, 0);
    class_addmethod(qlist_class, (t_method)qlist_rewind, gensym("rewind"), 0);
    class_addmethod(qlist_class, (t_method)qlist_next,
        gensym("next"), A_DEFFLOAT, 0);
    class_addmethod(qlist_class, (t_method)qlist_set, gensym("set"),
        A_GIMME, 0);
    class_addmethod(qlist_class, (t_method)qlist_clear, gensym("clear"), 0);
    class_addmethod(qlist_class, (t_method)qlist_add, gensym("add"),
        A_GIMME, 0);
    class_addmethod(qlist_class, (t_method)qlist_add2, gensym("add2"),
        A_GIMME, 0);
    class_addmethod(qlist_class, (t_method)qlist_add, gensym("append"),
        A_GIMME, 0);
    class_addmethod(qlist_class, (t_method)qlist_read, gensym("read"),
        A_SYMBOL, A_DEFSYM, 0);
    class_addmethod(qlist_class, (t_method)qlist_write, gensym("write"),
        A_SYMBOL, A_DEFSYM, 0);
    class_addmethod(qlist_class, (t_method)textbuf_open, gensym("click"), 0);
    class_addmethod(qlist_class, (t_method)textbuf_close, gensym("close"), 0);
    class_addmethod(qlist_class, (t_method)textbuf_addline,
        gensym("addline"), A_GIMME, 0);
    class_addmethod(qlist_class, (t_method)qlist_print, gensym("print"),
        A_DEFSYM, 0);
    class_addmethod(qlist_class, (t_method)qlist_tempo,
        gensym("tempo"), A_FLOAT, 0);
    class_addbang(qlist_class, qlist_bang);

    textfile_class = class_new(gensym("textfile"), (t_newmethod)textfile_new,
        (t_method)textbuf_free, sizeof(t_qlist), 0, 0);
    class_addmethod(textfile_class, (t_method)textfile_rewind, gensym("rewind"),
        0);
    class_addmethod(textfile_class, (t_method)qlist_set, gensym("set"),
        A_GIMME, 0);
    class_addmethod(textfile_class, (t_method)qlist_clear, gensym("clear"), 0);
    class_addmethod(textfile_class, (t_method)qlist_add, gensym("add"),
        A_GIMME, 0);
    class_addmethod(textfile_class, (t_method)qlist_add2, gensym("add2"),
        A_GIMME, 0);
    class_addmethod(textfile_class, (t_method)qlist_add, gensym("append"),
        A_GIMME, 0);
    class_addmethod(textfile_class, (t_method)qlist_read, gensym("read"),
        A_SYMBOL, A_DEFSYM, 0);
    class_addmethod(textfile_class, (t_method)qlist_write, gensym("write"),
        A_SYMBOL, A_DEFSYM, 0);
    class_addmethod(textfile_class, (t_method)textbuf_open, gensym("click"), 0);
    class_addmethod(textfile_class, (t_method)textbuf_close, gensym("close"),
        0);
    class_addmethod(textfile_class, (t_method)textbuf_addline,
        gensym("addline"), A_GIMME, 0);
    class_addmethod(textfile_class, (t_method)qlist_print, gensym("print"),
        A_DEFSYM, 0);
    class_addbang(textfile_class, textfile_bang);
}

