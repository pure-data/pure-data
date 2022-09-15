/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* interface objects */

#include "m_pd.h"
#include "s_stuff.h"
#include <string.h>

/* -------------------------- print ------------------------------ */
static t_class *print_class;

#define PRINT_LOGLEVEL 2

  /* imported from s_print.c: */
extern void startlogpost(const void *object, const int level, const char *fmt, ...)
    ATTRIBUTE_FORMAT_PRINTF(3, 4);

  /* avoid prefixing with "verbose(PRINT_LOGLEVEL): "
  when printing to stderr or via printhook. */
#define print_startlogpost(object, fmt, ...) do{ \
    if (STUFF->st_printhook || sys_printtostderr) \
        startpost(fmt, __VA_ARGS__); \
    else startlogpost(object, PRINT_LOGLEVEL, fmt, __VA_ARGS__); \
} while(0)

typedef struct _print
{
    t_object x_obj;
    t_symbol *x_sym;
} t_print;

static void *print_new(t_symbol *sel, int argc, t_atom *argv)
{
    t_print *x = (t_print *)pd_new(print_class);
    if (argc == 0)
        x->x_sym = gensym("print");
    else if (argc == 1 && argv->a_type == A_SYMBOL)
    {
        t_symbol *s = atom_getsymbolarg(0, argc, argv);
        if (!strcmp(s->s_name, "-n"))
            x->x_sym = &s_;
        else x->x_sym = s;
    }
    else
    {
        int bufsize;
        char *buf;
        t_binbuf *bb = binbuf_new();
        binbuf_add(bb, argc, argv);
        binbuf_gettext(bb, &buf, &bufsize);
        buf = resizebytes(buf, bufsize, bufsize+1);
        buf[bufsize] = 0;
        x->x_sym = gensym(buf);
        freebytes(buf, bufsize+1);
        binbuf_free(bb);
    }
    return (x);
}

static void print_bang(t_print *x)
{
    print_startlogpost(x, "%s%sbang", x->x_sym->s_name, (*x->x_sym->s_name ? ": " : ""));
    endpost();
}

static void print_pointer(t_print *x, t_gpointer *gp)
{
    print_startlogpost(x, "%s%s(pointer)", x->x_sym->s_name, (*x->x_sym->s_name ? ": " : ""));
    endpost();
}

static void print_float(t_print *x, t_float f)
{
    print_startlogpost(x, "%s%s%g", x->x_sym->s_name, (*x->x_sym->s_name ? ": " : ""), f);
    endpost();
}

static void print_anything(t_print *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    print_startlogpost(x, "%s%s%s", x->x_sym->s_name, (*x->x_sym->s_name ? ": " : ""),
        s->s_name);
    for (i = 0; i < argc; i++)
    {
        char buf[MAXPDSTRING];
        atom_string(argv+i, buf, MAXPDSTRING);
        print_startlogpost(x, " %s", buf);
    }
    endpost();
}

static void print_list(t_print *x, t_symbol *s, int argc, t_atom *argv)
{
    if (!argc)
        print_bang(x);
    else if (argc == 1)
    {
        switch (argv->a_type)
        {
        case A_FLOAT:
            print_float(x, argv->a_w.w_float);
            break;
        case A_SYMBOL:
            print_anything(x, &s_symbol, 1, argv);
            break;
        case A_POINTER:
            print_pointer(x, argv->a_w.w_gpointer);
            break;
        default:
            bug("print");
            break;
        }
    }
    else if (argv->a_type == A_FLOAT)
    {
        int i;
        /* print first (numeric) atom, to avoid a leading space */
        if (*x->x_sym->s_name)
            print_startlogpost(x, "%s: %g", x->x_sym->s_name, atom_getfloat(argv));
        else
            print_startlogpost(x, "%g", atom_getfloat(argv));
        argc--; argv++;
        for (i = 0; i < argc; i++)
        {
            char buf[MAXPDSTRING];
            atom_string(argv+i, buf, MAXPDSTRING);
            print_startlogpost(x, " %s", buf);
        }
        endpost();
    }
    else
        print_anything(x, &s_list, argc, argv);
}

static void print_setup(void)
{
    print_class = class_new(gensym("print"), (t_newmethod)print_new, 0,
        sizeof(t_print), 0, A_GIMME, 0);
    class_addbang(print_class, print_bang);
    class_addfloat(print_class, print_float);
    class_addpointer(print_class, print_pointer);
    class_addlist(print_class, print_list);
    class_addanything(print_class, print_anything);
}

/* ------------------- trace - see message passing traces ------------- */
int backtracer_settracing(void *x, int tracing);
extern int backtracer_cantrace;

static t_class *trace_class;

typedef struct _trace
{
    t_object x_obj;
    t_symbol *x_sym;
    t_float x_f;
} t_trace;

static void *trace_new(t_symbol *s)
{
    t_trace *x = (t_trace *)pd_new(trace_class);
    x->x_sym = s;
    x->x_f = 0;
    floatinlet_new(&x->x_obj, &x->x_f);
    outlet_new(&x->x_obj, &s_anything);
    return (x);
}

static void trace_anything(t_trace *x, t_symbol *s, int argc, t_atom *argv)
{
    int nturns = x->x_f;
    if (nturns > 0)
    {
        if (!backtracer_cantrace)
        {
            pd_error(x, "trace requested but tracing is not enabled");
            x->x_f = 0;
        }
        else if (backtracer_settracing(x, 1))
        {
            outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
            x->x_f = nturns-1;
            (void)backtracer_settracing(x, 0);
        }
    }
    else outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
}

static void trace_setup(void)
{
    trace_class = class_new(gensym("trace"), (t_newmethod)trace_new, 0,
        sizeof(t_trace), 0, A_DEFSYM, 0);
    class_addanything(trace_class, trace_anything);
}

void x_interface_setup(void)
{
    print_setup();
    trace_setup();
}
