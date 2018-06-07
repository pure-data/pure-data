#include "m_pd.h"
#include "g_canvas.h"
#include <stdio.h>
#include "g_undo.h"

#define XUQUEUE(x) (canvas_undo_get(x)->u_queue)
#define XULAST(x) (canvas_undo_get(x)->u_last)

/* used for canvas_objtext to differentiate between objects being created
   by user vs. those (re)created by the undo/redo actions */

int we_are_undoing = 0;
extern const char *canvas_undo_name;

extern void glob_preset_node_list_seek_hub(void);
extern void glob_preset_node_list_check_loc_and_update(void);
extern void text_checkvalidwidth(t_glist *glist);

t_undo_action *canvas_undo_init(t_canvas *x)
{
    t_undo_action *a = (t_undo_action *)getbytes(sizeof(*a));
    a->type = 0;
    a->x = x;
    a->next = NULL;

    if (!XUQUEUE(x))
    {
        //this is the first init
        XUQUEUE(x) = a;
        XULAST(x) = a;
        a->prev = NULL;
        a->name = "no";
        gui_vmess("gui_undo_menu", "xss", (t_int)a->x, "no", "no");
    }
    else
    {
        if (XULAST(x)->next)
        {
            //we need to rebranch first then add the new action
            canvas_undo_rebranch(x);
        }
        XULAST(x)->next = a;
        a->prev = XULAST(x);
        XULAST(x) = a;
    }
    //fprintf(stderr,"canvas_undo_init\n");
    return(a);
}

t_undo_action *canvas_undo_add(t_canvas *x, int type, const char *name,
    void *data)
{
    //fprintf(stderr,"canvas_undo_add %d\n", type);
    t_undo_action *a = canvas_undo_init(x);
    a->type = type;
    a->data = (void *)data;
    a->name = (char *)name;
    canvas_undo_name = name;
    gui_vmess("gui_undo_menu", "xss", x, a->name, "no");
    return(a);
}

void canvas_undo_undo(t_canvas *x)
{
    int dspwas = canvas_suspend_dsp();
    if (XUQUEUE(x) && XULAST(x) != XUQUEUE(x))
    {
        we_are_undoing = 1;
        //fprintf(stderr,"canvas_undo_undo %d\n", XULAST(x)->type);
        canvas_editmode(x, 1);
        glist_noselect(x);
        canvas_undo_name = XULAST(x)->name;
        switch(XULAST(x)->type)
        {
            case 1:    canvas_undo_connect(x, XULAST(x)->data, UNDO_UNDO); break;         //connect
            case 2:    canvas_undo_disconnect(x, XULAST(x)->data, UNDO_UNDO); break;     //disconnect
            case 3:    canvas_undo_cut(x, XULAST(x)->data, UNDO_UNDO); break;             //cut
            case 4:    canvas_undo_move(x, XULAST(x)->data, UNDO_UNDO); break;            //move
            case 5:    canvas_undo_paste(x, XULAST(x)->data, UNDO_UNDO); break;        //paste
            case 6:    canvas_undo_apply(x, XULAST(x)->data, UNDO_UNDO); break;        //apply
            case 7:    canvas_undo_arrange(x, XULAST(x)->data, UNDO_UNDO); break;        //arrange
            case 8:    canvas_undo_canvas_apply(x, XULAST(x)->data, UNDO_UNDO); break;    //canvas apply
            case 9:    canvas_undo_create(x, XULAST(x)->data, UNDO_UNDO); break;        //create
            case 10:canvas_undo_recreate(x, XULAST(x)->data, UNDO_UNDO); break;        //recreate
            case 11:canvas_undo_font(x, XULAST(x)->data, UNDO_UNDO); break;            //font
            default:
                error("canvas_undo_undo: unsupported undo command %d", XULAST(x)->type);
        }
        XULAST(x) = XULAST(x)->prev;
        char *undo_action = XULAST(x)->name;
        char *redo_action = XULAST(x)->next->name;
        we_are_undoing = 0;
        /* here we call updating of all unpaired hubs and nodes since
           their regular call will fail in case their position needed
           to be updated by undo/redo first to reflect the old one */
        glob_preset_node_list_seek_hub();
        glob_preset_node_list_check_loc_and_update();
        if (glist_isvisible(x) && glist_istoplevel(x))
        {
            gui_vmess("gui_undo_menu", "xss",
                x, undo_action, redo_action);
            text_checkvalidwidth(x);
            canvas_getscroll(x);
        }
        canvas_dirty(x, 1);
    }
    canvas_resume_dsp(dspwas);
}

void canvas_undo_redo(t_canvas *x)
{
    int dspwas = canvas_suspend_dsp();
    if (XUQUEUE(x) && XULAST(x)->next)
    {
        we_are_undoing = 1;
        XULAST(x) = XULAST(x)->next;
        //fprintf(stderr,"canvas_undo_redo %d\n", XULAST(x)->type);
        canvas_editmode(x, 1);
        glist_noselect(x);
        canvas_undo_name = XULAST(x)->name;
        switch(XULAST(x)->type)
        {
            case 1:    canvas_undo_connect(x, XULAST(x)->data, UNDO_REDO); break;         //connect
            case 2:    canvas_undo_disconnect(x, XULAST(x)->data, UNDO_REDO); break;     //disconnect
            case 3:    canvas_undo_cut(x, XULAST(x)->data, UNDO_REDO); break;             //cut
            case 4:    canvas_undo_move(x, XULAST(x)->data, UNDO_REDO); break;            //move
            case 5:    canvas_undo_paste(x, XULAST(x)->data, UNDO_REDO); break;        //paste
            case 6:    canvas_undo_apply(x, XULAST(x)->data, UNDO_REDO); break;        //apply
            case 7:    canvas_undo_arrange(x, XULAST(x)->data, UNDO_REDO); break;        //arrange
            case 8:    canvas_undo_canvas_apply(x, XULAST(x)->data, UNDO_REDO); break;    //canvas apply
            case 9:    canvas_undo_create(x, XULAST(x)->data, UNDO_REDO); break;        //create
            case 10:canvas_undo_recreate(x, XULAST(x)->data, UNDO_REDO); break;        //recreate
            case 11:canvas_undo_font(x, XULAST(x)->data, UNDO_REDO); break;            //font
            default:
                error("canvas_undo_redo: unsupported redo command %d", XULAST(x)->type);
        }
        char *undo_action = XULAST(x)->name;
        char *redo_action = (XULAST(x)->next ? XULAST(x)->next->name : "no");
        we_are_undoing = 0;
        /* here we call updating of all unpaired hubs and nodes since their
           regular call will fail in case their position needed to be updated
           by undo/redo first to reflect the old one */
        glob_preset_node_list_seek_hub();
        glob_preset_node_list_check_loc_and_update();
        if (glist_isvisible(x) && glist_istoplevel(x))
        {
            gui_vmess("gui_undo_menu", "xss",
                x, undo_action, redo_action);
            text_checkvalidwidth(x);
            canvas_getscroll(x);
        }
        canvas_dirty(x, 1);
    }
    canvas_resume_dsp(dspwas);
}

void canvas_undo_rebranch(t_canvas *x)
{
    int dspwas = canvas_suspend_dsp();
    t_undo_action *a1, *a2;
    //fprintf(stderr,"canvas_undo_rebranch");
    if (XULAST(x)->next)
    {
        a1 = XULAST(x)->next;
        while(a1)
        {
            //fprintf(stderr,".");
            switch(a1->type)
            {
                case 1:    canvas_undo_connect(x, a1->data, UNDO_FREE); break;         //connect
                case 2:    canvas_undo_disconnect(x, a1->data, UNDO_FREE); break;         //disconnect
                case 3:    canvas_undo_cut(x, a1->data, UNDO_FREE); break;             //cut
                case 4:    canvas_undo_move(x, a1->data, UNDO_FREE); break;            //move
                case 5:    canvas_undo_paste(x, a1->data, UNDO_FREE); break;            //paste
                case 6:    canvas_undo_apply(x, a1->data, UNDO_FREE); break;            //apply
                case 7:    canvas_undo_arrange(x, a1->data, UNDO_FREE); break;            //arrange
                case 8:    canvas_undo_canvas_apply(x, a1->data, UNDO_FREE); break;    //canvas apply
                case 9:    canvas_undo_create(x, a1->data, UNDO_FREE); break;            //create
                case 10:canvas_undo_recreate(x, a1->data, UNDO_FREE); break;        //recreate
                case 11:canvas_undo_font(x, a1->data, UNDO_FREE); break;            //font
                default:
                    error("canvas_undo_rebranch: unsupported undo command %d", a1->type);
            }
            a2 = a1->next;
            freebytes(a1, sizeof(*a1));
            a1 = a2;
        }
    }
    canvas_resume_dsp(dspwas);
    //fprintf(stderr,"done!\n");
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
    int dspwas = canvas_suspend_dsp();
    t_undo_action *a1, *a2;
    //fprintf(stderr,"canvas_undo_free");
    if (XUQUEUE(x))
    {
        a1 = XUQUEUE(x);
        while(a1)
        {
            //fprintf(stderr,".");
            switch(a1->type)
            {
                case 0: break;                                                        //init
                case 1:    canvas_undo_connect(x, a1->data, UNDO_FREE); break;         //connect
                case 2:    canvas_undo_disconnect(x, a1->data, UNDO_FREE); break;         //disconnect
                case 3:    canvas_undo_cut(x, a1->data, UNDO_FREE); break;             //cut
                case 4:    canvas_undo_move(x, a1->data, UNDO_FREE); break;            //move
                case 5:    canvas_undo_paste(x, a1->data, UNDO_FREE); break;            //paste
                case 6:    canvas_undo_apply(x, a1->data, UNDO_FREE); break;            //apply
                case 7:    canvas_undo_arrange(x, a1->data, UNDO_FREE); break;            //arrange
                case 8:    canvas_undo_canvas_apply(x, a1->data, UNDO_FREE); break;    //canvas apply
                case 9:    canvas_undo_create(x, a1->data, UNDO_FREE); break;            //create
                case 10:canvas_undo_recreate(x, a1->data, UNDO_FREE); break;        //recreate
                case 11:canvas_undo_font(x, a1->data, UNDO_FREE); break;            //font
                default:
                    error("canvas_undo_free: unsupported undo command %d", a1->type);
            }
            a2 = a1->next;
            freebytes(a1, sizeof(*a1));
            a1 = a2;
        }
    }
    canvas_resume_dsp(dspwas);
    //fprintf(stderr,"done!\n");
}
