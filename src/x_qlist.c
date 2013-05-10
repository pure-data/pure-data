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

#ifdef HAVE_ALLOCA_H        /* ifdef nonsense to find include for alloca() */
# include <alloca.h>        /* linux, mac, mingw, cygwin */
#elif defined _MSC_VER
# include <malloc.h>        /* MSVC */
#else
# include <stddef.h>        /* BSDs for example */
#endif                      /* end alloca() ifdef nonsense */

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
                 sys_hostfontsize(glist_getfont(x->b_canvas)));
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

static void textbuf_addline(t_textbuf *b, t_symbol *s, int ac, t_atom *av)
{
    t_binbuf *z = binbuf_new();
    binbuf_restore(z, ac, av);
    binbuf_add(b->b_binbuf, binbuf_getnatom(z), binbuf_getvec(z));
    binbuf_free(z);
    textbuf_senditup(b);
}

static void textbuf_read(t_textbuf *x, t_symbol *s, int ac, t_atom *av)
{
    int cr = 0;
    t_symbol *filename;
    while (ac && av->a_type == A_SYMBOL && *av->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(av->a_w.w_symbol->s_name, "-c"))
            cr = 1;
        else
        {
            pd_error(x, "text read: unknown flag ...");
            postatom(ac, av);
        }
        ac--; av++;
    }
    if (ac && av->a_type == A_SYMBOL)
    {
        filename = av->a_w.w_symbol;
        ac--; av++;
    }
    else
    {
        pd_error(x, "text read: no file name given");
        return;
    }
    if (ac)
    {
        post("warning: text define ignoring extra argument: ");
        postatom(ac, av);
    }
    if (binbuf_read_via_canvas(x->b_binbuf, filename->s_name, x->b_canvas, cr))
            pd_error(x, "%s: read failed", filename->s_name);
    textbuf_senditup(x);
}

static void textbuf_write(t_textbuf *x, t_symbol *s, int ac, t_atom *av)
{
    int cr = 0;
    t_symbol *filename;
    char buf[MAXPDSTRING];
    while (ac && av->a_type == A_SYMBOL && *av->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(av->a_w.w_symbol->s_name, "-c"))
            cr = 1;
        else
        {
            pd_error(x, "text write: unknown flag ...");
            postatom(ac, av);
        }
        ac--; av++;
    }
    if (ac && av->a_type == A_SYMBOL)
    {
        filename = av->a_w.w_symbol;
        ac--; av++;
    }
    else
    {
        pd_error(x, "text write: no file name given");
        return;
    }
    if (ac)
    {
        post("warning: text define ignoring extra argument: ");
        postatom(ac, av);
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
    while (x2 = pd_findbyclass(gensym("#A"), text_define_class))
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
    unsigned char x_keep;   /* whether to embed contents in patch on save */
    t_symbol *x_bindsym;
} t_text_define;

#define x_ob x_textbuf.b_ob
#define x_binbuf x_textbuf.b_binbuf
#define x_canvas x_textbuf.b_canvas

static void *text_define_new(t_symbol *s, int ac, t_atom *av)
{
    t_text_define *x = (t_text_define *)pd_new(text_define_class);
    t_symbol *asym = gensym("#A");
    x->x_keep = 0;
    x->x_bindsym = &s_;
    while (ac && av->a_type == A_SYMBOL && *av->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(av->a_w.w_symbol->s_name, "-k"))
            x->x_keep = 1;
        else
        {
            pd_error(x, "text define: unknown flag ...");
            postatom(ac, av);
        }
        ac--; av++;
    }
    if (ac && av->a_type == A_SYMBOL)
    {
        pd_bind(&x->x_ob.ob_pd, av->a_w.w_symbol);
        x->x_bindsym = av->a_w.w_symbol;
        ac--; av++;
    }
    if (ac)
    {
        post("warning: text define ignoring extra argument: ");
        postatom(ac, av);
    }
    textbuf_init(&x->x_textbuf);
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
        gp, s, "text_frompointer");
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

void text_define_save(t_gobj *z, t_binbuf *bb)
{
    t_text_define *b = (t_text_define *)z;
    binbuf_addv(bb, "ssff", &s__X, gensym("obj"),
        (float)b->x_ob.te_xpix, (float)b->x_ob.te_ypix);
    binbuf_addbinbuf(bb, b->x_ob.ob_binbuf);
    binbuf_addsemi(bb);

    if (b->x_keep)
    {
        binbuf_addv(bb, "ss", gensym("#A"), gensym("addline"));
        binbuf_addbinbuf(bb, b->x_binbuf);
        binbuf_addsemi(bb);
    }
}

static void text_define_free(t_text_define *x)
{
    textbuf_free(&x->x_textbuf);
    if (x->x_bindsym != &s_)
        pd_unbind(&x->x_ob.ob_pd, x->x_bindsym);
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
        if (type != DT_LIST)
        {
            pd_error(x, "text: field %s not of type list", x->tc_field->s_name);
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
        /* figure this out LATER */
    }
}

static void text_client_free(t_text_client *x)
{
    gpointer_unset(&x->tc_gp);
}

/* ---------------- text_getline object - output nth line --------------*/
t_class *text_getline_class;

typedef struct _text_getline
{
    t_text_client x_tc;
    t_outlet *x_out1;       /* list */
    t_outlet *x_out2;       /* 1 if comma terminated, 0 if semi, 2 if none */
} t_text_getline;

#define x_obj x_tc.tc_obj
#define x_sym x_tc.tc_sym
#define x_gp x_tc.tc_gp
#define x_struct x_tc.tc_struct
#define x_field x_tc.tc_field

static void *text_getline_new(t_symbol *s, int ac, t_atom *av)
{
    t_text_getline *x = (t_text_getline *)pd_new(text_getline_class);
    x->x_out1 = outlet_new(&x->x_obj, &s_list);
    x->x_out2 = outlet_new(&x->x_obj, &s_float);
    x->x_sym = x->x_struct = x->x_field = 0;
    gpointer_init(&x->x_gp);
    while (ac && av->a_type == A_SYMBOL && *av->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(av->a_w.w_symbol->s_name, "-s") &&
            ac >= 3 && av[1].a_type == A_SYMBOL && av[2].a_type == A_SYMBOL)
        {
            x->x_struct = canvas_makebindsym(av[1].a_w.w_symbol);
            x->x_field = av[2].a_w.w_symbol;
            ac -= 2; av += 2;
        }
        else
        {
            pd_error(x, "text getline: unknown flag ...");
            postatom(ac, av);
        }
        ac--; av++;
    }
    if (ac && av->a_type == A_SYMBOL)
    {
        if (x->x_struct)
        {
            pd_error(x, "text getline: extra names after -s..");
            postatom(ac, av);
        }
        else x->x_sym = av->a_w.w_symbol;
        ac--; av++;
    }
    if (ac)
    {
        post("warning: text getline ignoring extra argument: ");
        postatom(ac, av);
    }
    if (x->x_struct)
        pointerinlet_new(&x->x_obj, &x->x_gp);
    return (x);
}

static void text_getline_float(t_text_getline *x, t_floatarg f)
{
    t_binbuf *b = text_client_getbuf(&x->x_tc);
    int start, end, n;
    t_atom *vec;
    if (!b)
       return;
    vec = binbuf_getvec(b);
    n = binbuf_getnatom(b);
    if (text_nthline(n, vec, f, &start, &end))
    {
        int outc = end - start, k;
        t_atom *outv;
        outlet_float(x->x_out2, (end < n && vec[end].a_type == A_COMMA));
        ATOMS_ALLOCA(outv, outc);
        for (k = 0; k < outc; k++)
            outv[k] = vec[start+k];
        outlet_list(x->x_out1, 0, outc, outv);
        ATOMS_FREEA(outv, outc);
    }
    else
    {
        outlet_float(x->x_out2, 2);         /* 2 for out of range */
        outlet_list(x->x_out1, 0, 0, 0);    /* ... and empty list */
    }
}

/* ---------------- text_setline object - output nth line --------------*/
typedef struct _text_setline
{
    t_text_client x_tc;
    t_float x_f;            /* line number */
} t_text_setline;

t_class *text_setline_class;

static void *text_setline_new(t_symbol *s, int ac, t_atom *av)
{
    t_text_setline *x = (t_text_setline *)pd_new(text_setline_class);
    floatinlet_new(&x->x_obj, &x->x_f);
    x->x_sym = x->x_struct = x->x_field = 0;
    gpointer_init(&x->x_gp);
    while (ac && av->a_type == A_SYMBOL && *av->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(av->a_w.w_symbol->s_name, "-s") &&
            ac >= 3 && av[1].a_type == A_SYMBOL && av[2].a_type == A_SYMBOL)
        {
            x->x_struct = canvas_makebindsym(av[1].a_w.w_symbol);
            x->x_field = av[2].a_w.w_symbol;
            ac -= 2; av += 2;
        }
        else
        {
            pd_error(x, "text setline: unknown flag ...");
            postatom(ac, av);
        }
        ac--; av++;
    }
    if (ac && av->a_type == A_SYMBOL)
    {
        if (x->x_struct)
        {
            pd_error(x, "text setline: extra names after -s..");
            postatom(ac, av);
        }
        else x->x_sym = av->a_w.w_symbol;
        ac--; av++;
    }
    if (ac)
    {
        post("warning: text setline ignoring extra argument: ");
        postatom(ac, av);
    }
    if (x->x_struct)
        pointerinlet_new(&x->x_obj, &x->x_gp);
    return (x);
}

static void text_setline_list(t_text_setline *x,
    t_symbol *s, int ac, t_atom *av)
{
    t_binbuf *b = text_client_getbuf(&x->x_tc);
    int start, end, n, lineno = x->x_f, i;
    t_atom *vec;
    if (!b)
       return;
    vec = binbuf_getvec(b);
    n = binbuf_getnatom(b);
    if (lineno < 0)
    {
        pd_error(x, "text setline: line number (%d) < 0", lineno);
        return;
    }
    if (text_nthline(n, vec, lineno, &start, &end))
    {
        if (end - start != ac)  /* grow or shrink */
        {
            (void)binbuf_resize(b, (n = n + (ac - (end-start))));
            vec = binbuf_getvec(b);
            memmove(&vec[start + ac], &vec[end],
                sizeof(*vec) * (n - (start+ac)));
        }
    }
    else    /* if line number too high just append to end */
    {
        int addsemi = (n && vec[n-1].a_type != A_SEMI &&
            vec[n-1].a_type != A_COMMA), newsize = n + addsemi + ac + 1;
        (void)binbuf_resize(b, newsize);
        vec = binbuf_getvec(b);
        if (addsemi)
            SETSEMI(&vec[n]);
        SETSEMI(&vec[newsize-1]);
        start = n+addsemi;
    }
    for (i = 0; i < ac; i++)
    {
        if (av[i].a_type == A_POINTER)
            SETSYMBOL(&vec[start+i], gensym("(pointer)"));
        else vec[start+i] = av[i];
    }
    text_client_senditup(&x->x_tc);
}

/* overall creator for "text" objects - dispatch to "text define" etc */
static void *text_new(t_symbol *s, int argc, t_atom *argv)
{
    if (!argc || argv[0].a_type != A_SYMBOL)
        newest = text_define_new(s, argc, argv);
    else
    {
        char *str = argv[0].a_w.w_symbol->s_name;
        if (!strcmp(str, "d") || !strcmp(str, "define"))
            newest = text_define_new(s, argc-1, argv+1);
        else if (!strcmp(str, "getline"))
            newest = text_getline_new(s, argc-1, argv+1);
        else if (!strcmp(str, "setline"))
            newest = text_setline_new(s, argc-1, argv+1);
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

static void qlist_add(t_qlist *x, t_symbol *s, int ac, t_atom *av)
{
    t_atom a;
    SETSEMI(&a);
    binbuf_add(x->x_binbuf, ac, av);
    binbuf_add(x->x_binbuf, 1, &a);
}

static void qlist_add2(t_qlist *x, t_symbol *s, int ac, t_atom *av)
{
    binbuf_add(x->x_binbuf, ac, av);
}

static void qlist_clear(t_qlist *x)
{
    qlist_rewind(x);
    binbuf_clear(x->x_binbuf);
}

static void qlist_set(t_qlist *x, t_symbol *s, int ac, t_atom *av)
{
    qlist_clear(x);
    qlist_add(x, s, ac, av);
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

void x_qlist_setup(void )
{
    text_define_class = class_new(gensym("text define"),
        (t_newmethod)text_define_new,
        (t_method)text_define_free, sizeof(t_text_define), 0, A_GIMME, 0);
    class_addmethod(text_define_class, (t_method)textbuf_open,
        gensym("click"), 0);
    class_addmethod(text_define_class, (t_method)textbuf_close,
        gensym("close"), 0);
    class_addmethod(text_define_class, (t_method)textbuf_addline, 
        gensym("addline"), A_GIMME, 0);
    class_addmethod(text_define_class, (t_method)text_define_clear,
        gensym("clear"), 0);
    class_addmethod(text_define_class, (t_method)textbuf_write,
        gensym("write"), A_GIMME, 0);
    class_addmethod(text_define_class, (t_method)textbuf_read,
        gensym("read"), A_GIMME, 0);
    class_setsavefn(text_define_class, text_define_save);
    class_sethelpsymbol(text_define_class, gensym("text-object"));

    class_addcreator((t_newmethod)text_new, gensym("text"), A_GIMME, 0);

    text_getline_class = class_new(gensym("text getline"),
        (t_newmethod)text_getline_new, (t_method)text_client_free,
            sizeof(t_text_getline), 0, A_GIMME, 0);
    class_addfloat(text_getline_class, text_getline_float);
    
    text_setline_class = class_new(gensym("text setline"),
        (t_newmethod)text_setline_new, (t_method)text_client_free,
            sizeof(t_text_getline), 0, A_GIMME, 0);
    class_addlist(text_setline_class, text_setline_list);

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

