/* Copyright (c) 1997-1999 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

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

static void textbuf_open(t_textbuf *x)
{
    int i, ntxt;
    char *txt, buf[40];
    if (x->b_guiconnect)
    {
        sys_vgui("wm deiconify .x%lx\n", x);
        sys_vgui("raise .x%lx\n", x);
        sys_vgui("focus .x%lx.text\n", x);
    }
    else
    {
        sys_vgui("pdtk_textwindow_open .x%lx %dx%d {%s: %s} %d\n",
            x, 600, 340, "myname", "text", 
                 sys_hostfontsize(glist_getfont(x->b_canvas)));
        binbuf_gettext(x->b_binbuf, &txt, &ntxt);
        for (i = 0; i < ntxt; )
        {
            char *j = strchr(txt+i, '\n');
            if (!j) j = txt + ntxt;
            sys_vgui("pdtk_textwindow_append .x%lx {%.*s\n}\n", x, j-txt-i, txt+i);
            i = (j-txt)+1;
        }
        sys_vgui("pdtk_textwindow_setdirty .x%lx 0\n", x);
        t_freebytes(txt, ntxt);
        sprintf(buf, ".x%lx", (unsigned long)x);
        x->b_guiconnect = guiconnect_new(&x->b_ob.ob_pd, gensym(buf));
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
}

/* textobj object - buffer for text, accessible by other accessor objects */

typedef struct _textobj
{
    t_textbuf x_textbuf;
    unsigned char x_embed;   /* whether to embed contents in patch on save */
} t_textobj;

#define x_ob x_textbuf.b_ob
#define x_binbuf x_textbuf.b_binbuf
#define x_canvas x_textbuf.b_canvas

static t_class *textobj_class;

static void *textobj_new(t_symbol *s, int ac, t_atom *av)
{
    t_textobj *x = (t_textobj *)pd_new(textobj_class);
    t_symbol *asym = gensym("#A");
    textbuf_init(&x->x_textbuf);
           /* bashily unbind #A -- this would create garbage if #A were
           multiply bound but we believe in this context it's at most
           bound to whichever textobj or array was created most recently */
    asym->s_thing = 0;
        /* and now bind #A to us to receive following messages in the
        saved file or copy buffer */
    pd_bind(&x->x_ob.ob_pd, asym); 
    return (x);
}

static void textobj_clear(t_textobj *x)
{
    binbuf_clear(x->x_binbuf);
}

void textobj_save(t_gobj *z, t_binbuf *bb)
{
    t_textobj *b = (t_textobj *)z;
    binbuf_addv(bb, "ssff", &s__X, gensym("obj"),
        (float)b->x_ob.te_xpix, (float)b->x_ob.te_ypix);
    binbuf_addbinbuf(bb, b->x_ob.ob_binbuf);
    binbuf_addsemi(bb);

    binbuf_addv(bb, "ss", gensym("#A"), gensym("addline"));
    binbuf_addbinbuf(bb, b->x_binbuf);
    binbuf_addsemi(bb);
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
    while (x2 = pd_findbyclass(gensym("#A"), textobj_class))
        pd_unbind(x2, gensym("#A"));
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
    textobj_class = class_new(gensym("text"), (t_newmethod)textobj_new,
        (t_method)textbuf_free, sizeof(t_textobj), 0, A_GIMME, 0);
    class_addmethod(textobj_class, (t_method)textbuf_open, gensym("click"), 0);
    class_addmethod(textobj_class, (t_method)textbuf_close, gensym("close"), 0);
    class_addmethod(textobj_class, (t_method)textbuf_addline, 
        gensym("addline"), A_GIMME, 0);
    class_addmethod(textobj_class, (t_method)textobj_clear,
        gensym("clear"), 0);
    class_addmethod(textobj_class, (t_method)qlist_print, gensym("print"),
        A_DEFSYM, 0);
    class_setsavefn(textobj_class, textobj_save);
    class_sethelpsymbol(textobj_class, gensym("text-object"));

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
    class_addmethod(textfile_class, (t_method)textbuf_close, gensym("close"), 0);
    class_addmethod(textfile_class, (t_method)textbuf_addline, 
        gensym("addline"), A_GIMME, 0);
    class_addmethod(textfile_class, (t_method)qlist_print, gensym("print"),
        A_DEFSYM, 0);
    class_addbang(textfile_class, textfile_bang);
}

