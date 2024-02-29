#include "m_pd.h"
#include "g_canvas.h"
#include "g_checkchange.h"

extern t_checkchange *editor_get_checkchange(void);

    /* this callback will be called by the gui once the user accepted to discard the changes,
        or if there are no changes */
void checkchange_accept_discard(t_canvas *x)
{
        /* close the canvas window */
    canvas_vis(x, 0);

        /* restart the check with next canvas */
    checkchange_start();
}

void checkchange_setup(void)
{
    t_checkchange *c = editor_get_checkchange();
    c->c_cb = NULL;
    c->c_selection = NULL;
    class_addmethod(canvas_class, (t_method)checkchange_accept_discard,
        gensym("accept_discard"), A_FLOAT, A_NULL);
}

static void checkchange_reset(void)
{
    t_checkchange *c = editor_get_checkchange();
    if (c->c_cb != NULL)
    {
            /* clear the selection */
        while(c->c_selection)
        {
            t_selection *s = c->c_selection->sel_next;
            freebytes(c->c_selection, sizeof(t_selection));
            c->c_selection = s;
        }
        c->c_cb = NULL;
    }
}

void checkchange_init(checkchange_cb cb, ...)
{
    t_checkchange *c = editor_get_checkchange();
    int arg_i;
    checkchange_reset();
    c->c_cb = cb;
    va_list ap;
    va_start(ap, cb);
    for (arg_i = 0; arg_i < CHECKCHANGE_CB_MAXARGS; arg_i++)
        c->c_args[arg_i] = va_arg(ap, void *);
    va_end(ap);
}

    /* actually select a canvas */
static void checkchange_select(t_checkchange *c, t_canvas *x)
{
    t_selection *sel = (t_selection *)getbytes(sizeof(*sel));
    sel->sel_next = c->c_selection;
    sel->sel_what = (t_gobj*) x;
    c->c_selection = sel;
}

void checkchange_find_dirty(t_canvas *x)
{
    t_checkchange *c = editor_get_checkchange();
    t_gobj *g;
    if(c->c_cb == NULL) bug("checkchange_find_dirty");
    for (g = x->gl_list; g; g = g->g_next)
        if (pd_class(&g->g_pd) == canvas_class)
            checkchange_find_dirty((t_glist *)g);

    if (x->gl_env && x->gl_dirty)
        checkchange_select(c, x);
}

void checkchange_find_dirty_in_selection(t_canvas *x)
{
    t_selection *y;
    for (y = x->gl_editor->e_selection; y; y = y->sel_next)
        if (pd_class(&y->sel_what->g_pd) == canvas_class)
            checkchange_find_dirty((t_canvas*)(y->sel_what));
}

static void checkchange_open_dialog(t_canvas *x)
{
    t_canvas *c = canvas_getrootfor(x);
    const char *msg[]= {"Discard changes to '%s'?", c->gl_name->s_name};
    char buf[80];
    t_atom backmsg[2];
    sprintf(buf, ".x%lx", x);
    SETSYMBOL(backmsg+0, gensym("accept_discard"));
    SETFLOAT (backmsg+1, 0);
    vmess(&x->gl_pd, gensym("menu-open"), "");
    pdgui_vmess("pdtk_check", "^ Sms",
        c,
        2, msg,
        gensym(buf), 2, backmsg,
        "no");
}

void checkchange_start(void)
{
    t_checkchange *c = editor_get_checkchange();
    t_selection *s;
    t_canvas *x;
    if(c->c_cb == NULL) bug("checkchange_start");

        /* if there is no more dirty canvas to check,
           call the callback, cleanup and return */
    if(c->c_selection == NULL)
    {
        c->c_cb(c->c_args);
        checkchange_reset();
        return;
    }

        /* pop the first canvas from the selection */
    s = c->c_selection->sel_next;
    x = (t_canvas *)(c->c_selection->sel_what);
    freebytes(c->c_selection, sizeof(t_selection));
    c->c_selection = s;

        /* ask user to discard the changes in this canvas */
    checkchange_open_dialog(x);
}
