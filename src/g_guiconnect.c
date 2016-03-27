/* Copyright (c) 1997-2000 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  a thing to forward messages from the GUI, dealing with race conditions
in which the "target" gets deleted while the GUI is sending it something.

See also the gfxstub object that doesn't oblige the owner to keep a pointer
around (so is better suited to one-off dialogs)
*/

#include "m_pd.h"
#include "g_canvas.h"

struct _guiconnect
{
    t_object x_obj;
    t_pd *x_who;
    t_symbol *x_sym;
    t_clock *x_clock;
};

static t_class *guiconnect_class;

t_guiconnect *guiconnect_new(t_pd *who, t_symbol *sym)
{
    t_guiconnect *x = (t_guiconnect *)pd_new(guiconnect_class);
    x->x_who = who;
    x->x_sym = sym;
    pd_bind(&x->x_obj.ob_pd, sym);
    return (x);
}

    /* cleanup routine; delete any resources we have */
static void guiconnect_free(t_guiconnect *x)
{
    if (x->x_sym)
        pd_unbind(&x->x_obj.ob_pd, x->x_sym);
    if (x->x_clock)
        clock_free(x->x_clock);
}

    /* this is called when the clock times out to indicate the GUI should
    be gone by now. */
static void guiconnect_tick(t_guiconnect *x)
{
    pd_free(&x->x_obj.ob_pd);
}

    /* the target calls this to disconnect.  If the gui has "signed off"
    we're ready to delete the object; otherwise we wait either for signoff
    or for a timeout. */
void guiconnect_notarget(t_guiconnect *x, double timedelay)
{
    if (!x->x_sym)
        pd_free(&x->x_obj.ob_pd);
    else
    {
        x->x_who = 0;
        if (timedelay > 0)
        {
            x->x_clock = clock_new(x, (t_method)guiconnect_tick);
            clock_delay(x->x_clock, timedelay);
        }
    }
}

    /* the GUI calls this to send messages to the target. */
static void guiconnect_anything(t_guiconnect *x,
    t_symbol *s, int ac, t_atom *av)
{
    if (x->x_who)
        typedmess(x->x_who, s, ac, av);
}

    /* the GUI calls this when it disappears.  (If there's any chance the
    GUI will fail to do this, the "target", when it signs off, should specify
    a timeout after which the guiconnect will disappear.) */
static void guiconnect_signoff(t_guiconnect *x)
{
    if (!x->x_who)
        pd_free(&x->x_obj.ob_pd);
    else
    {
        pd_unbind(&x->x_obj.ob_pd, x->x_sym);
        x->x_sym = 0;
    }
}

void g_guiconnect_setup(void)
{
    guiconnect_class = class_new(gensym("guiconnect"), 0,
        (t_method)guiconnect_free, sizeof(t_guiconnect), CLASS_PD, 0);
    class_addanything(guiconnect_class, guiconnect_anything);
    class_addmethod(guiconnect_class, (t_method)guiconnect_signoff,
        gensym("signoff"), 0);
}
