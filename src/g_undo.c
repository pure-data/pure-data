#include "m_pd.h"
#include "g_canvas.h"
#include <stdio.h>
#include "g_undo.h"

#if 0
# define DEBUG_UNDO(x) x
#else
# define DEBUG_UNDO(x) do { } while(0)
#endif

#define XUQUEUE(x) (canvas_undo_get(x)->u_queue)
#define XULAST(x) (canvas_undo_get(x)->u_last)
#define XUNDOING(x) (canvas_undo_get(x)->u_doing)

/* used for canvas_objtext to differentiate between objects being created
   by user vs. those (re)created by the undo/redo actions */

void canvas_undo_set_name(const char*name);

#define XUNODIRTY(x) (canvas_undo_get(x)->u_nodirty)
void canvas_undo_nodirty(t_canvas *x)
{
    XUNODIRTY(x) = XULAST(x);
}

static int canvas_undo_isdirty(t_canvas *x)
{
    return (XULAST(x) != XUNODIRTY(x))?1:0;
}

t_undo_action *canvas_undo_init(t_canvas *x)
{
    t_undo_action *a = 0;
    if(!canvas_undo_get(x))return 0;
    a = (t_undo_action *)getbytes(sizeof(*a));
    a->type = 0;
    a->x = x;
    a->next = NULL;

    if (!XUQUEUE(x))
    {
        DEBUG_UNDO(post("%s: first init", __FUNCTION__));
        //this is the first init
        XUQUEUE(x) = a;
        XULAST(x) = a;
        canvas_undo_nodirty(x);

        a->prev = NULL;
        a->name = "no";
        if (glist_isvisible(x) && glist_istoplevel(x))
            sys_vgui("pdtk_undomenu .x%lx no no\n", x);
    }
    else
    {
        DEBUG_UNDO(post("%s: re-init %p", __FUNCTION__, XULAST(x)->next));
        if (XULAST(x)->next)
        {
            //we need to rebranch first then add the new action
            canvas_undo_rebranch(x);
        }
        XULAST(x)->next = a;
        a->prev = XULAST(x);
        XULAST(x) = a;
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
    if(!canvas_undo_get(x))return;
    int dspwas = canvas_suspend_dsp();
    DEBUG_UNDO(post("%s: %p != %p", __FUNCTION__, XUQUEUE(x), XULAST(x)));
    if (XUQUEUE(x) && XULAST(x) != XUQUEUE(x))
    {
        XUNDOING(x) = 1;
        canvas_editmode(x, 1);
        glist_noselect(x);
        canvas_undo_set_name(XULAST(x)->name);

        switch(XULAST(x)->type)
        {
            case UNDO_CONNECT:      canvas_undo_connect(x, XULAST(x)->data, UNDO_UNDO); break;      //connect
            case UNDO_DISCONNECT:   canvas_undo_disconnect(x, XULAST(x)->data, UNDO_UNDO); break;   //disconnect
            case UNDO_CUT:          canvas_undo_cut(x, XULAST(x)->data, UNDO_UNDO); break;          //cut
            case UNDO_MOTION:       canvas_undo_move(x, XULAST(x)->data, UNDO_UNDO); break;         //move
            case UNDO_PASTE:        canvas_undo_paste(x, XULAST(x)->data, UNDO_UNDO); break;        //paste
            case UNDO_APPLY:        canvas_undo_apply(x, XULAST(x)->data, UNDO_UNDO); break;        //apply
            case UNDO_ARRANGE:      canvas_undo_arrange(x, XULAST(x)->data, UNDO_UNDO); break;      //arrange
            case UNDO_CANVAS_APPLY: canvas_undo_canvas_apply(x, XULAST(x)->data, UNDO_UNDO); break; //canvas apply
            case UNDO_CREATE:       canvas_undo_create(x, XULAST(x)->data, UNDO_UNDO); break;       //create
            case UNDO_RECREATE:     canvas_undo_recreate(x, XULAST(x)->data, UNDO_UNDO); break;     //recreate
            case UNDO_FONT:         canvas_undo_font(x, XULAST(x)->data, UNDO_UNDO); break;         //font
            default:
                error("canvas_undo_undo: unsupported undo command %d", XULAST(x)->type);
        }
        XULAST(x) = XULAST(x)->prev;
        char *undo_action = XULAST(x)->name;
        char *redo_action = XULAST(x)->next->name;

        XUNDOING(x) = 0;
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
    if(!canvas_undo_get(x))return;
    dspwas = canvas_suspend_dsp();
    if (XUQUEUE(x) && XULAST(x)->next)
    {
        XUNDOING(x) = 1;
        XULAST(x) = XULAST(x)->next;
        canvas_editmode(x, 1);
        glist_noselect(x);
        canvas_undo_set_name(XULAST(x)->name);
        switch(XULAST(x)->type)
        {
            case UNDO_CONNECT:      canvas_undo_connect(x, XULAST(x)->data, UNDO_REDO); break;      //connect
            case UNDO_DISCONNECT:   canvas_undo_disconnect(x, XULAST(x)->data, UNDO_REDO); break;   //disconnect
            case UNDO_CUT:          canvas_undo_cut(x, XULAST(x)->data, UNDO_REDO); break;          //cut
            case UNDO_MOTION:       canvas_undo_move(x, XULAST(x)->data, UNDO_REDO); break;         //move
            case UNDO_PASTE:        canvas_undo_paste(x, XULAST(x)->data, UNDO_REDO); break;        //paste
            case UNDO_APPLY:        canvas_undo_apply(x, XULAST(x)->data, UNDO_REDO); break;        //apply
            case UNDO_ARRANGE:      canvas_undo_arrange(x, XULAST(x)->data, UNDO_REDO); break;      //arrange
            case UNDO_CANVAS_APPLY: canvas_undo_canvas_apply(x, XULAST(x)->data, UNDO_REDO); break; //canvas apply
            case UNDO_CREATE:       canvas_undo_create(x, XULAST(x)->data, UNDO_REDO); break;       //create
            case UNDO_RECREATE:     canvas_undo_recreate(x, XULAST(x)->data, UNDO_REDO); break;     //recreate
            case UNDO_FONT:         canvas_undo_font(x, XULAST(x)->data, UNDO_REDO); break;         //font
            default:
                error("canvas_undo_redo: unsupported redo command %d", XULAST(x)->type);
        }
        char *undo_action = XULAST(x)->name;
        char *redo_action = (XULAST(x)->next ? XULAST(x)->next->name : "no");
        XUNDOING(x) = 0;
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
    if (XULAST(x)->next)
    {
        a1 = XULAST(x)->next;
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
    if(!canvas_undo_get(x))return;
    dspwas = canvas_suspend_dsp();
    if (XUQUEUE(x))
    {
        a1 = XUQUEUE(x);
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
