#include "m_pd.h"
#include "g_canvas.h"
#include <stdio.h>
#include "g_undo.h"

#if 0
# define DEBUG_UNDO(x) startpost("[%s:%d] ", __FILE__, __LINE__), x
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
    t_undo_action *a = 0;
    t_undo * udo = canvas_undo_get(x);
    DEBUG_UNDO(post("%s: %d %s %p!", __FUNCTION__, type, name, data));
    if(UNDO_SEQUENCE_END == type
       && udo && udo->u_last
       && UNDO_SEQUENCE_START == udo->u_last->type)
    {
            /* empty undo sequence...get rid of it */
        udo->u_last = udo->u_last->prev;
        canvas_undo_rebranch(x);
        udo->u_last->next = 0;
        canvas_undo_set_name(udo->u_last->name);
        if (glist_isvisible(x) && glist_istoplevel(x))
            sys_vgui("pdtk_undomenu .x%lx %s no\n", x, udo->u_last->name);
        return 0;
    }

    a = canvas_undo_init(x);
    if(!a)return a;
    a->type = type;
    a->data = (void *)data;
    a->name = (char *)name;
    canvas_undo_set_name(name);
    if (glist_isvisible(x) && glist_istoplevel(x))
        sys_vgui("pdtk_undomenu .x%lx %s no\n", x, a->name);

    DEBUG_UNDO(post("%s: done!", __FUNCTION__));
    return(a);
}

static int canvas_undo_doit(t_canvas *x, t_undo_action *udo, int action, const char*funname)
{
    DEBUG_UNDO(post("%s: %s(%d) %d %p", __FUNCTION__, funname, action, udo->type, udo->data));

    switch(udo->type)
    {
    case UNDO_CONNECT:      return canvas_undo_connect(x, udo->data, action);      //connect
    case UNDO_DISCONNECT:   return canvas_undo_disconnect(x, udo->data, action);   //disconnect
    case UNDO_CUT:          return canvas_undo_cut(x, udo->data, action);          //cut
    case UNDO_MOTION:       return canvas_undo_move(x, udo->data, action);         //move
    case UNDO_PASTE:        return canvas_undo_paste(x, udo->data, action);        //paste
    case UNDO_APPLY:        return canvas_undo_apply(x, udo->data, action);        //apply
    case UNDO_ARRANGE:      return canvas_undo_arrange(x, udo->data, action);      //arrange
    case UNDO_CANVAS_APPLY: return canvas_undo_canvas_apply(x, udo->data, action); //canvas apply
    case UNDO_CREATE:       return canvas_undo_create(x, udo->data, action);       //create
    case UNDO_RECREATE:     return canvas_undo_recreate(x, udo->data, action);     //recreate
    case UNDO_FONT:         return canvas_undo_font(x, udo->data, action);         //font
            /* undo sequences are handled in canvas_undo_undo resp canvas_undo_redo */
    case UNDO_SEQUENCE_START: return 1;                                            //start undo sequence
    case UNDO_SEQUENCE_END: return 1;                                              //end undo sequence
    case UNDO_INIT:         if (UNDO_FREE == action) return 1;/* FALLS THROUGH */  //init
    default:
        error("%s: unsupported undo command %d", funname, udo->type);
    }
    return 0;
}

void canvas_undo_undo(t_canvas *x)
{
    t_undo *udo = canvas_undo_get(x);
    int dspwas;

    if (!udo) return;
    dspwas = canvas_suspend_dsp();
    DEBUG_UNDO(post("%s: %p != %p", __FUNCTION__, udo->u_queue, udo->u_last));
    if (udo->u_queue && udo->u_last != udo->u_queue)
    {
        udo->u_doing = 1;
        canvas_editmode(x, 1);
        glist_noselect(x);
        canvas_undo_set_name(udo->u_last->name);

        if(UNDO_SEQUENCE_END == udo->u_last->type)
        {
            int sequence_depth = 1;
            while((udo->u_last = udo->u_last->prev)
                  && (UNDO_INIT != udo->u_last->type))
            {
                DEBUG_UNDO(post("%s:sequence[%d] %d", __FUNCTION__, sequence_depth, udo->u_last->type));
                switch(udo->u_last->type)
                {
                case UNDO_SEQUENCE_START:
                    sequence_depth--;
                    break;
                case UNDO_SEQUENCE_END:
                    sequence_depth++;
                    break;
                default:
                    canvas_undo_doit(x, udo->u_last, UNDO_UNDO, __FUNCTION__);
                }
                if (sequence_depth < 1)
                    break;
            }
            if (sequence_depth < 0)
                bug("undo sequence missing end");
            else if (sequence_depth > 0)
                bug("undo sequence missing start");
        }

        if(canvas_undo_doit(x, udo->u_last, UNDO_UNDO, __FUNCTION__))
        {
            char *undo_action, *redo_action;
            udo->u_last = udo->u_last->prev;
            undo_action = udo->u_last->name;
            redo_action = udo->u_last->next->name;

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

        if(UNDO_SEQUENCE_START == udo->u_last->type)
        {
            int sequence_depth = 1;
            while(udo->u_last->next && (udo->u_last = udo->u_last->next))
            {
                DEBUG_UNDO(post("%s:sequence[%d] %d", __FUNCTION__, sequence_depth, udo->u_last->type));
                switch(udo->u_last->type)
                {
                case UNDO_SEQUENCE_END:
                    sequence_depth--;
                    break;
                case UNDO_SEQUENCE_START:
                    sequence_depth++;
                    break;
                default:
                    canvas_undo_doit(x, udo->u_last, UNDO_REDO, __FUNCTION__);
                }
                if (sequence_depth < 1)
                    break;
            }
            if (sequence_depth < 0)
                bug("undo sequence end without start");
            else if (sequence_depth > 0)
                bug("undo sequence start without end");
        }

        canvas_undo_doit(x, udo->u_last, UNDO_REDO, __FUNCTION__);
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
            canvas_undo_doit(x, a1, UNDO_FREE, __FUNCTION__);
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
            canvas_undo_doit(x, a1, UNDO_FREE, __FUNCTION__);
            a2 = a1->next;
            freebytes(a1, sizeof(*a1));
            a1 = a2;
        }
    }
    canvas_resume_dsp(dspwas);
}
