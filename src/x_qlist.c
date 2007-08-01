/* Copyright (c) 1997-1999 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include <string.h>
#ifdef UNISTD
#include <unistd.h>
#endif
#ifdef MSW
#include <io.h>
#endif

typedef struct _qlist
{
    t_object x_ob;
    t_outlet *x_bangout;
    void *x_binbuf;
    int x_onset;                /* playback position */
    t_clock *x_clock;
    float x_tempo;
    double x_whenclockset;
    float x_clockdelay;
    t_symbol *x_dir;
    t_canvas *x_canvas;
    int x_reentered;
} t_qlist;

static void qlist_tick(t_qlist *x);

static t_class *qlist_class;

static void *qlist_new( void)
{
    t_symbol *name, *filename = 0;
    t_qlist *x = (t_qlist *)pd_new(qlist_class);
    x->x_binbuf = binbuf_new();
    x->x_clock = clock_new(x, (t_method)qlist_tick);
    outlet_new(&x->x_ob, &s_list);
    x->x_bangout = outlet_new(&x->x_ob, &s_bang);
    x->x_onset = 0x7fffffff;
    x->x_tempo = 1;
    x->x_whenclockset = 0;
    x->x_clockdelay = 0;
    x->x_canvas = canvas_getcurrent();
    x->x_reentered = 0;
    return (x);
}

static void qlist_rewind(t_qlist *x)
{
    x->x_onset = 0;
    if (x->x_clock) clock_unset(x->x_clock);
    x->x_whenclockset = 0;
    x->x_reentered = 1;
}

static void qlist_donext(t_qlist *x, int drop, int automatic)
{
    t_pd *target = 0;
    while (1)
    {
        int argc = binbuf_getnatom(x->x_binbuf),
            count, onset = x->x_onset, onset2, wasreentered;
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
                error("qlist: %s: no such object", ap->a_w.w_symbol->s_name);
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
        wasreentered = x->x_reentered;
        x->x_reentered = 0;
        if (!drop)
        {   
            if (ap->a_type == A_FLOAT)
                typedmess(target, &s_list, count, ap);
            else if (ap->a_type == A_SYMBOL)
                typedmess(target, ap->a_w.w_symbol, count-1, ap+1);
        }
        if (x->x_reentered)
            return;
        x->x_reentered = wasreentered;
    }  /* while (1); never falls through */

end:
    x->x_onset = 0x7fffffff;
    outlet_bang(x->x_bangout);
    x->x_whenclockset = 0;
}

static void qlist_next(t_qlist *x, t_floatarg drop)
{
    qlist_donext(x, drop != 0, 0);
}

static void qlist_bang(t_qlist *x)
{
    qlist_rewind(x);
    qlist_donext(x, 0, 1);
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
        error("qlist_read: unknown flag: %s", format->s_name);

    if (binbuf_read_via_canvas(x->x_binbuf, filename->s_name, x->x_canvas, cr))
            error("%s: read failed", filename->s_name);
    x->x_onset = 0x7fffffff;
    x->x_reentered = 1;
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
        error("qlist_read: unknown flag: %s", format->s_name);
    if (binbuf_write(x->x_binbuf, buf, "", cr))
            error("%s: write failed", filename->s_name);
}

static void qlist_print(t_qlist *x)
{
    post("--------- textfile or qlist contents: -----------");
    binbuf_print(x->x_binbuf);
}

static void qlist_tempo(t_qlist *x, t_float f)
{
    float newtempo;
    if (f < 1e-20) f = 1e-20;
    else if (f > 1e20) f = 1e20;
    newtempo = 1./f;
    if (x->x_whenclockset != 0)
    {
        float elapsed = clock_gettimesince(x->x_whenclockset);
        float left = x->x_clockdelay - elapsed;
        if (left < 0) left = 0;
        left *= newtempo / x->x_tempo;
        clock_delay(x->x_clock, left);
    }
    x->x_tempo = newtempo;
}

static void qlist_free(t_qlist *x)
{
    binbuf_free(x->x_binbuf);
    if (x->x_clock) clock_free(x->x_clock);
}

/* -------------------- textfile ------------------------------- */

static t_class *textfile_class;
typedef t_qlist t_textfile;

static void *textfile_new( void)
{
    t_symbol *name, *filename = 0;
    t_textfile *x = (t_textfile *)pd_new(textfile_class);
    x->x_binbuf = binbuf_new();
    outlet_new(&x->x_ob, &s_list);
    x->x_bangout = outlet_new(&x->x_ob, &s_bang);
    x->x_onset = 0x7fffffff;
    x->x_reentered = 0;
    x->x_tempo = 1;
    x->x_whenclockset = 0;
    x->x_clockdelay = 0;
    x->x_clock = NULL;
    x->x_canvas = canvas_getcurrent();
    return (x);
}

static void textfile_bang(t_textfile *x)
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

static void textfile_free(t_textfile *x)
{
    binbuf_free(x->x_binbuf);
}

/* ---------------- global setup function -------------------- */

void x_qlist_setup(void )
{
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
    class_addmethod(qlist_class, (t_method)qlist_print, gensym("print"),
        A_DEFSYM, 0);
    class_addmethod(qlist_class, (t_method)qlist_tempo,
        gensym("tempo"), A_FLOAT, 0);
    class_addbang(qlist_class, qlist_bang);

    textfile_class = class_new(gensym("textfile"), (t_newmethod)textfile_new,
        (t_method)textfile_free, sizeof(t_textfile), 0, 0);
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
    class_addmethod(textfile_class, (t_method)qlist_print, gensym("print"),
        A_DEFSYM, 0);
    class_addbang(textfile_class, textfile_bang);
}

