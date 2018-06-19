#include "m_pd.h"
#include "g_canvas.h"
#include <stdio.h>
#include "g_undo.h"

#if 0
# define DEBUG_UNDO(x) x
#else
# define DEBUG_UNDO(x) do { } while(0)
#endif

/* used for canvas_objtext to differentiate between objects being created
   by user vs. those (re)created by the undo/redo actions */

void canvas_undo_set_name(const char*name);

static void canvas_undo_docleardirty(t_canvas *x)
{
    t_undo *udo = canvas_undo_get(x);
    if (udo) udo->u_cleanstate = udo->u_last;
}
void canvas_undo_cleardirty(t_canvas *x)
{
        /* clear dirty flags of this canvas
         * and all sub-canvases (but not abstractions/
         */
    t_gobj*y;
    canvas_undo_docleardirty(x);
    for(y=x->gl_list; y; y=y->g_next)
        if (pd_class(&y->g_pd) == canvas_class && !canvas_isabstraction((t_canvas*)y))
            canvas_undo_cleardirty((t_canvas*)y);
}

static int canvas_undo_doisdirty(t_canvas*x)
{
    t_undo *udo = x?canvas_undo_get(x):0;
    t_gobj*y;
    if (!udo) return 0;
    if (udo->u_last != udo->u_cleanstate) return 1;

    for(y=x->gl_list; y; y=y->g_next)
        if (pd_class(&y->g_pd) == canvas_class && !canvas_isabstraction((t_canvas*)y))
            if (canvas_undo_doisdirty((t_canvas*)y))
                return 1;
    return 0;
}
static int canvas_undo_isdirty(t_canvas *x)
{
    t_undo *udo = x?canvas_undo_get(x):0;
    return (0 != udo)
        && ((udo->u_last != udo->u_cleanstate)
            || canvas_undo_doisdirty(canvas_getrootfor(x)));
}

t_undo_action *canvas_undo_init(t_canvas *x)
{
    t_undo_action *a = 0;
    t_undo *udo = canvas_undo_get(x);
    if (!udo) return 0;
    a = (t_undo_action *)getbytes(sizeof(*a));
    a->type = 0;
    a->x = x;
    a->next = NULL;

    if (!udo->u_queue)
    {
        DEBUG_UNDO(post("%s: first init", __FUNCTION__));
        //this is the first init
        udo->u_queue = a;
        udo->u_last = a;

        canvas_undo_cleardirty(x);
            /* invalidate clean-state for re-created subpatches
             * since we do not know whether the re-created subpatch
             * is clean or unclean (it's undo-queue got lost when
             * it was deleted), we assume the worst */
        if (!canvas_isabstraction(x))
            udo->u_cleanstate = (void*)1;

        a->prev = NULL;
        a->name = "no";
        if (glist_isvisible(x) && glist_istoplevel(x))
            sys_vgui("pdtk_undomenu .x%lx no no\n", x);
    }
    else
    {
        DEBUG_UNDO(post("%s: re-init %p", __FUNCTION__, udo->u_last->next));
        if (udo->u_last->next)
        {
            //we need to rebranch first then add the new action
            canvas_undo_rebranch(x);
        }
        udo->u_last->next = a;
        a->prev = udo->u_last;
        udo->u_last = a;
    }
    return(a);
}

t_undo_action *canvas_undo_add(t_canvas *x, t_undo_type type, const char *name,
    void *data)
{
    t_undo_action *a = canvas_undo_init(x);
    a->type = type;
    a->data = (void *)data;
    a->name = (char *)name;
    canvas_undo_set_name(name);
    if (glist_isvisible(x) && glist_istoplevel(x))
        sys_vgui("pdtk_undomenu .x%lx %s no\n", x, a->name);

    DEBUG_UNDO(post("%s: done!", __FUNCTION__));
    return(a);
}

void canvas_undo_undo(t_canvas *x)
{
    t_undo *udo = canvas_undo_get(x);
    if (!udo) return;
    int dspwas = canvas_suspend_dsp();
    DEBUG_UNDO(post("%s: %p != %p", __FUNCTION__, udo->u_queue, udo->u_last));
    if (udo->u_queue && udo->u_last != udo->u_queue)
    {
        udo->u_doing = 1;
        canvas_editmode(x, 1);
        glist_noselect(x);
        canvas_undo_set_name(udo->u_last->name);

        switch(udo->u_last->type)
        {
            case UNDO_CONNECT:      canvas_undo_connect(x, udo->u_last->data, UNDO_UNDO); break;      //connect
            case UNDO_DISCONNECT:   canvas_undo_disconnect(x, udo->u_last->data, UNDO_UNDO); break;   //disconnect
            case UNDO_CUT:          canvas_undo_cut(x, udo->u_last->data, UNDO_UNDO); break;          //cut
            case UNDO_MOTION:       canvas_undo_move(x, udo->u_last->data, UNDO_UNDO); break;         //move
            case UNDO_PASTE:        canvas_undo_paste(x, udo->u_last->data, UNDO_UNDO); break;        //paste
            case UNDO_APPLY:        canvas_undo_apply(x, udo->u_last->data, UNDO_UNDO); break;        //apply
            case UNDO_ARRANGE:      canvas_undo_arrange(x, udo->u_last->data, UNDO_UNDO); break;      //arrange
            case UNDO_CANVAS_APPLY: canvas_undo_canvas_apply(x, udo->u_last->data, UNDO_UNDO); break; //canvas apply
            case UNDO_CREATE:       canvas_undo_create(x, udo->u_last->data, UNDO_UNDO); break;       //create
            case UNDO_RECREATE:     canvas_undo_recreate(x, udo->u_last->data, UNDO_UNDO); break;     //recreate
            case UNDO_FONT:         canvas_undo_font(x, udo->u_last->data, UNDO_UNDO); break;         //font
            default:
                error("canvas_undo_undo: unsupported undo command %d", udo->u_last->type);
        }
        udo->u_last = udo->u_last->prev;
        char *undo_action = udo->u_last->name;
        char *redo_action = udo->u_last->next->name;

        udo->u_doing = 0;
        /* here we call updating of all unpaired hubs and nodes since
           their regular call will fail in case their position needed
           to be updated by undo/redo first to reflect the old one */
        if (glist_isvisible(x) && glist_istoplevel(x))
        {
            if (glist_isvisible(x) && glist_istoplevel(x))
                sys_vgui("pdtk_undomenu .x%lx %s %s\n", x, undo_action, redo_action);
        }
        canvas_dirty(x, canvas_undo_isdirty(x));
    }
    canvas_resume_dsp(dspwas);
}

void canvas_undo_redo(t_canvas *x)
{
    int dspwas;
    t_undo *udo = canvas_undo_get(x);
    if (!udo) return;
    dspwas = canvas_suspend_dsp();
    if (udo->u_queue && udo->u_last->next)
    {
        char *undo_action, *redo_action;
        udo->u_doing = 1;
        udo->u_last = udo->u_last->next;
        canvas_editmode(x, 1);
        glist_noselect(x);
        canvas_undo_set_name(udo->u_last->name);
        switch(udo->u_last->type)
        {
            case UNDO_CONNECT:      canvas_undo_connect(x, udo->u_last->data, UNDO_REDO); break;      //connect
            case UNDO_DISCONNECT:   canvas_undo_disconnect(x, udo->u_last->data, UNDO_REDO); break;   //disconnect
            case UNDO_CUT:          canvas_undo_cut(x, udo->u_last->data, UNDO_REDO); break;          //cut
            case UNDO_MOTION:       canvas_undo_move(x, udo->u_last->data, UNDO_REDO); break;         //move
            case UNDO_PASTE:        canvas_undo_paste(x, udo->u_last->data, UNDO_REDO); break;        //paste
            case UNDO_APPLY:        canvas_undo_apply(x, udo->u_last->data, UNDO_REDO); break;        //apply
            case UNDO_ARRANGE:      canvas_undo_arrange(x, udo->u_last->data, UNDO_REDO); break;      //arrange
            case UNDO_CANVAS_APPLY: canvas_undo_canvas_apply(x, udo->u_last->data, UNDO_REDO); break; //canvas apply
            case UNDO_CREATE:       canvas_undo_create(x, udo->u_last->data, UNDO_REDO); break;       //create
            case UNDO_RECREATE:     canvas_undo_recreate(x, udo->u_last->data, UNDO_REDO); break;     //recreate
            case UNDO_FONT:         canvas_undo_font(x, udo->u_last->data, UNDO_REDO); break;         //font
            default:
                error("canvas_undo_redo: unsupported redo command %d", udo->u_last->type);
        }
        undo_action = udo->u_last->name;
        redo_action = (udo->u_last->next ? udo->u_last->next->name : "no");
        udo->u_doing = 0;
        /* here we call updating of all unpaired hubs and nodes since their
           regular call will fail in case their position needed to be updated
           by undo/redo first to reflect the old one */
        if (glist_isvisible(x) && glist_istoplevel(x))
        {
            if (glist_isvisible(x) && glist_istoplevel(x))
                sys_vgui("pdtk_undomenu .x%lx %s %s\n", x, undo_action, redo_action);
        }
        canvas_dirty(x, canvas_undo_isdirty(x));
    }
    canvas_resume_dsp(dspwas);
}

void canvas_undo_rebranch(t_canvas *x)
{
    int dspwas = canvas_suspend_dsp();
    t_undo_action *a1, *a2;
    t_undo *udo = canvas_undo_get(x);
    if (!udo) return;
    if (udo->u_last->next)
    {
        a1 = udo->u_last->next;
        while(a1)
        {
            switch(a1->type)
            {
                case UNDO_CONNECT:      canvas_undo_connect(x, a1->data, UNDO_FREE); break;         //connect
                case UNDO_DISCONNECT:   canvas_undo_disconnect(x, a1->data, UNDO_FREE); break;         //disconnect
                case UNDO_CUT:          canvas_undo_cut(x, a1->data, UNDO_FREE); break;             //cut
                case UNDO_MOTION:       canvas_undo_move(x, a1->data, UNDO_FREE); break;            //move
                case UNDO_PASTE:        canvas_undo_paste(x, a1->data, UNDO_FREE); break;            //paste
                case UNDO_APPLY:        canvas_undo_apply(x, a1->data, UNDO_FREE); break;            //apply
                case UNDO_ARRANGE:      canvas_undo_arrange(x, a1->data, UNDO_FREE); break;            //arrange
                case UNDO_CANVAS_APPLY: canvas_undo_canvas_apply(x, a1->data, UNDO_FREE); break;    //canvas apply
                case UNDO_CREATE:       canvas_undo_create(x, a1->data, UNDO_FREE); break;            //create
                case UNDO_RECREATE:     canvas_undo_recreate(x, a1->data, UNDO_FREE); break;        //recreate
                case UNDO_FONT:         canvas_undo_font(x, a1->data, UNDO_FREE); break;            //font
                default:
                    error("canvas_undo_rebranch: unsupported undo command %d", a1->type);
            }
            a2 = a1->next;
            freebytes(a1, sizeof(*a1));
            a1 = a2;
        }
    }
    canvas_resume_dsp(dspwas);
}

void canvas_undo_check_canvas_pointers(t_canvas *x)
{
    /* currently unnecessary unless we decide to implement one central undo
       for all patchers */
}

void canvas_undo_purge_abstraction_actions(t_canvas *x)
{
    /* currently unnecessary unless we decide to implement one central undo
       for all patchers */
}

void canvas_undo_free(t_canvas *x)
{
    int dspwas;
    t_undo_action *a1, *a2;
    t_undo *udo = canvas_undo_get(x);
    if (!udo) return;
    dspwas = canvas_suspend_dsp();
    if (udo->u_queue)
    {
        a1 = udo->u_queue;
        while(a1)
        {
            switch(a1->type)
            {
                case UNDO_INIT: break;                                                           //init
                case UNDO_CONNECT:      canvas_undo_connect(x, a1->data, UNDO_FREE); break;      //connect
                case UNDO_DISCONNECT:   canvas_undo_disconnect(x, a1->data, UNDO_FREE); break;   //disconnect
                case UNDO_CUT:          canvas_undo_cut(x, a1->data, UNDO_FREE); break;          //cut
                case UNDO_MOTION:       canvas_undo_move(x, a1->data, UNDO_FREE); break;         //move
                case UNDO_PASTE:        canvas_undo_paste(x, a1->data, UNDO_FREE); break;        //paste
                case UNDO_APPLY:        canvas_undo_apply(x, a1->data, UNDO_FREE); break;        //apply
                case UNDO_ARRANGE:      canvas_undo_arrange(x, a1->data, UNDO_FREE); break;      //arrange
                case UNDO_CANVAS_APPLY: canvas_undo_canvas_apply(x, a1->data, UNDO_FREE); break; //canvas apply
                case UNDO_CREATE:       canvas_undo_create(x, a1->data, UNDO_FREE); break;       //create
                case UNDO_RECREATE:     canvas_undo_recreate(x, a1->data, UNDO_FREE); break;     //recreate
                case UNDO_FONT:         canvas_undo_font(x, a1->data, UNDO_FREE); break;         //font
                default:
                    error("canvas_undo_free: unsupported undo command %d", a1->type);
            }
            a2 = a1->next;
            freebytes(a1, sizeof(*a1));
            a1 = a2;
        }
    }
    canvas_resume_dsp(dspwas);
}
