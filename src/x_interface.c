/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* interface objects */

#include "m_pd.h"

/* -------------------------- print ------------------------------ */
static t_class *print_class;

typedef struct _print
{
    t_object x_obj;
    t_symbol *x_sym;
} t_print;

static void *print_new(t_symbol *s)
{
    t_print *x = (t_print *)pd_new(print_class);
    if (*s->s_name) x->x_sym = s;
    else x->x_sym = gensym("print");
    return (x);
}

static void print_bang(t_print *x)
{
    post("%s: bang", x->x_sym->s_name);
}

static void print_pointer(t_print *x, t_gpointer *gp)
{
    post("%s: (gpointer)", x->x_sym->s_name);
}

static void print_float(t_print *x, t_float f)
{
    post("%s: %g", x->x_sym->s_name, f);
}

static void print_list(t_print *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    if (argc && argv->a_type != A_SYMBOL) startpost("%s:", x->x_sym->s_name);
    else startpost("%s: %s", x->x_sym->s_name,
        (argc > 1 ? s_list.s_name : (argc == 1 ? s_symbol.s_name :
            s_bang.s_name)));
    postatom(argc, argv);
    endpost();
}

static void print_anything(t_print *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    startpost("%s: %s", x->x_sym->s_name, s->s_name);
    postatom(argc, argv);
    endpost();
}

static void print_setup(void)
{
    print_class = class_new(gensym("print"), (t_newmethod)print_new, 0,
        sizeof(t_print), 0, A_DEFSYM, 0);
    class_addbang(print_class, print_bang);
    class_addfloat(print_class, print_float);
    class_addpointer(print_class, print_pointer);
    class_addlist(print_class, print_list);
    class_addanything(print_class, print_anything);
}



void x_interface_setup(void)
{
    print_setup();
}
