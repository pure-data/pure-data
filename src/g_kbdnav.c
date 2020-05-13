#include "m_pd.h"
#include "m_imp.h"
#include "g_canvas.h"
#include <math.h>
#include <string.h>

#include "g_kbdnav.h"
#include "g_all_guis.h"

/* This file contains the core of navigating
   and connecting stuff using the keyboard. */

/* ------------------ forward declarations --------------- */
int canvas_canconnect(t_canvas*x, t_object*src, int nout, t_object*sink, int nin);


/* in/outlet traverser for the magnetic connector.
   It is also used by the digit connector. */
typedef struct _magtrav
{
    double mt_dist;
    int mt_iotype;
    int mt_objindex;
    int mt_ioindex;
    struct _magtrav *mt_next;
} t_magtrav;




void kbdnav_debug(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);

    post("-----------------dbg-------------------");
    if(kbdnav->kn_selobj)
    {
        /* get the object text */
        char *buf;
        int bufsize;
        binbuf_gettext(kbdnav->kn_selobj->te_binbuf, &buf, &bufsize);
        buf = t_resizebytes(buf, bufsize, bufsize+1);
        buf[bufsize] = 0;
        post("   selected object text = [%s]", buf);
    }
    else
    {
        post("   selected object text = NULL");
    }
    post("   kn_ioindex = %d", kbdnav->kn_ioindex);
    post("   kn_moddown = %d", kbdnav->kn_moddown);
    post("   kn_connindex = %d", kbdnav->kn_connindex);
    post("   kn_chosennumber = %d", kbdnav->kn_chosennumber);
    post("   kn_indexvis = %d", kbdnav->kn_indexvis);
    if( kbdnav->kn_outconnect){
        post("   kn_outconnect = %lx", kbdnav->kn_outconnect);
    }
    else
    {
        post("   kn_outconnect = NULL");
    }
    if( kbdnav->kn_selobj){
        post("   kn_selobj = %lx", kbdnav->kn_selobj);
        int i = canvas_getindex( x, &kbdnav->kn_selobj->ob_g );
        post("   kn_selobj index = %d", i);
    }
    else
    {
        post("   kn_selobj = NULL");
    }
    if( kbdnav->kn_linetraverser){
        post("   kn_linetraverser = %lx", kbdnav->kn_linetraverser);
        post("      tr_ob = %lx", kbdnav->kn_linetraverser->tr_ob);
        post("      tr_ob index= %d", canvas_getindex( x, &kbdnav->kn_linetraverser->tr_ob->ob_g ));
        post("      tr_outno= %d", kbdnav->kn_linetraverser->tr_outno);
        post("      tr_ob2 = %lx", kbdnav->kn_linetraverser->tr_ob2);
        post("      tr_ob2 index= %d", canvas_getindex( x, &kbdnav->kn_linetraverser->tr_ob2->ob_g ));
        post("      tr_inno= %d", kbdnav->kn_linetraverser->tr_inno);

    }
    else
    {
        post("   kn_linetraverser = NULL");
    }
    if( kbdnav->kn_magtrav)
    {
        post("   kn_magtrav = %lx", kbdnav->kn_magtrav);
    }
    else
    {
        post("   kn_magtrav = NULL");
    }
    char *str;
    switch(kbdnav->kn_state)
    {
        case KN_INACTIVE:
            str = "KN_INACTIVE";
            break;
        case KN_IO_SELECTED:
            str = "KN_IO_SELECTED";
            break;
        case KN_CONN_SELECTED:
            str = "KN_CONN_SELECTED";
            break;
        case KN_WAITING_NUMBER:
            str = "KN_WAITING_NUMBER";
            break;
        default:
            str = "=====BUG=====";
    }

    post("   kn_state = %s", str);
    switch(kbdnav->kn_iotype)
    {
        case IO_INLET:
            str = "IO_INLET";
            break;
        case IO_OUTLET:
            str = "IO_OUTLET";
            break;
        default:
            str = "=====BUG=====";
    }
    post("   kn_iotype = %s", str);
    int count = 0;
    for( t_magtrav *mag = kbdnav->kn_magtrav ;
         mag;
         mag = mag->mt_next )
    {
        t_gobj *gobj = glist_nth(x, mag->mt_objindex);
        t_object *obj = pd_checkobject(&gobj->g_pd);
            /* get the object text */
            char *buf;
            int bufsize;
            binbuf_gettext(obj->te_binbuf, &buf, &bufsize);
            buf = t_resizebytes(buf, bufsize, bufsize+1);
            buf[bufsize] = 0;
        int type = mag->mt_iotype;
        post("   magtrav[%d] OBJ NAME = [%s]", count, buf);
        post("   magtrav[%d].mt_dist = %f", count, mag->mt_dist);
        post("   magtrav[%d].mt_iotype = %d", count, mag->mt_iotype);
        post("   magtrav[%d].type = %s", count, type == IO_OUTLET ? "outlet" : "inlet");
        post("   magtrav[%d].mt_objindex = %d", count, mag->mt_objindex);
        post("   magtrav[%d].mt_ioindex = %d", count, mag->mt_ioindex);
        post("   magtrav[%d].mt_next = %lx", count, mag->mt_next);
        ++count;
    }
    post("-----");
}

t_kbdnav* kbdnav_new()
{
    t_kbdnav *x = getbytes(sizeof(*x));
    x->kn_ioindex = 0;
    x->kn_state = KN_INACTIVE;
    x->kn_moddown = 0;
    x->kn_connindex = 0;
    x->kn_outconnect = NULL;
    x->kn_highlight = "blue";
    x->kn_linetraverser = (t_linetraverser *)getbytes(sizeof(*x->kn_linetraverser));
    x->kn_magtrav = NULL;
    x->kn_chosennumber = -1;
    return x;
}

void kbdnav_free(t_kbdnav *x)
{
    freebytes((void *)x->kn_linetraverser, sizeof(*x->kn_linetraverser));
    freebytes((void *)x, sizeof(*x));
}


/* callled when the user enters keyboard navigation
   by selecting an object and pressing mod+Up/down.
   We activate the selected object's first inlet or outlet,
   depending on the pressed arrow.
   "io_type" describes if the user will start
   navigation on an inlet or outlet ("IO_INLET" or "IO_OUTLET") */
void kbdnav_activate_kbdnav(t_canvas *x, int io_type)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    int n_of_selected_objs = kbdnav_count_selected_objects(x);
    if( n_of_selected_objs == 1 )
    {
        kbdnav_activate_object( x, kbdnav_get_selected_obj(x), io_type );
    }
    else
    {
        kbdnav_deactivate(x);
        return;
    }
}

/* called when the user exits keyboard navigation */
void kbdnav_deactivate(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    kbdnav->kn_ioindex = 0;
    kbdnav->kn_state = KN_INACTIVE;
    kbdnav->kn_connindex = 0;
    kbdnav->kn_indexvis = 0;
    kbdnav->kn_selobj = 0;
    kbdnav->kn_outconnect = NULL;
    kbdnav->kn_chosennumber = -1;
    kbdnav_magnetic_connect_free(x);
    canvas_redraw(x);
}


/* parse key press/release events related to keyboard navigation*/
int kbdnav_key(t_canvas *x, t_symbol *s, int ac, t_atom *av, int keynum, int down, int shift, t_symbol *gotkeysym)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    if( !kbdnav ) return 1;

    /* set our modifier state */
#if defined(__APPLE__)
    if( !strcmp(gotkeysym->s_name, "Meta_L") )
#else
    /* TCL reports Right Control keyup events as Control_L */
    /* https://wiki.tcl-lang.org/page/Modifier+Keys*/
    if( ( !strcmp(gotkeysym->s_name, "Control_L") || !strcmp(gotkeysym->s_name, "Control_R")) )
#endif
    {
        kbdnav->kn_moddown = down ? 1 : 0 ;
    }

    /* Leave kbdnavigation if # of selected objects != 1 */
    if ( down
         && kbdnav->kn_state != KN_INACTIVE
         && (!strcmp(gotkeysym->s_name, "Up") || !strcmp(gotkeysym->s_name, "Down")
            || !strcmp(gotkeysym->s_name, "Left") || !strcmp(gotkeysym->s_name, "Right")) )
    {
        if ( kbdnav_count_selected_objects(x) != 1)
        {
            kbdnav_deactivate(x);
            return 0;
        }
    }

    /* Navigation with arrow keys */
    int isArrowKey = !strcmp(gotkeysym->s_name, "Up") || !strcmp(gotkeysym->s_name, "Down")
                 || !strcmp(gotkeysym->s_name, "Left") || !strcmp(gotkeysym->s_name, "Right");
    if (down && isArrowKey)
    {
        if( kbdnav->kn_moddown )
        {
            kbdnav_magnetic_connect_free(x);
            if (!strcmp(gotkeysym->s_name, "Up"))
                kbdnav_up(x, shift);
            else if (!strcmp(gotkeysym->s_name, "Down"))
                kbdnav_down(x, shift);
            else if (!strcmp(gotkeysym->s_name, "Left"))
                kbdnav_left(x, shift);
            else if (!strcmp(gotkeysym->s_name, "Right"))
                kbdnav_right(x, shift);
            /* we call canvas_redraw because the code that draws the io/outlet highlighting
               is inside glist_drawiofor(). Maybe later we could keep track of the things we draw
               so we can just move/delete them. For now we need to redraw the patch. */
            canvas_redraw(x);
            /* after it's redrawn we can display the numbers*/
            if (kbdnav->kn_state == KN_WAITING_NUMBER)
                kbdnav_digit_connect_display_numbers(x);
            return 0;
        }
        else
        {
            return 1;
        }
    }

    /* Escape leaves kbd navigation */
    if( down && kbdnav->kn_state != KN_INACTIVE && !strcmp(gotkeysym->s_name, "Escape") )
    {
        /* The GUI has a binding (on pd_bindings.tcl) which calls
           deselect all on an escape press so we store the object again
           so we can select it again */
        t_gobj *gobj = &kbdnav->kn_selobj->te_g;
        kbdnav_deactivate(x);
        glist_select(x, gobj);
        return 0;
    }

    /* Shift+Return simulates a click on the selected object */
    if( down
        && !strcmp(gotkeysym->s_name, "Return")
        && kbdnav_count_selected_objects(x) == 1)
    {
        if( shift && !kbdnav->kn_moddown)
        {
            t_object *ob = kbdnav_get_selected_obj(x);
            t_gobj *g = &(ob->ob_g);
            int xpos, ypos, dummy;
            gobj_getrect(g, x, &xpos, &ypos, &dummy, &dummy);

            gobj_click(g, x, xpos, ypos, 0, 0, 0, 1);
            gobj_click(g, x, xpos, ypos, 0, 0, 0, 0);
            /* this second gobj_click() call (mouse up) doesn't solve the problem with sliders and radios */
            /* TODO: we shouldn't allow this "click" on sliders and radios as there is no control the user will have on where they're clicking! */
            return 0;
        }
        else if ( !shift && kbdnav->kn_moddown)
        {
            canvas_reselect(x);
            return 0;
        }
    }

    /* Mod+Tab display the object indexes. This is useful for the goto feature
       Note: for some reason "Tab" is only detected on key release events! (tested on Win7 and Win10) */
    if( !down && kbdnav->kn_moddown && !strcmp(gotkeysym->s_name, "Tab"))
    {
        kbdnav->kn_indexvis = !kbdnav->kn_indexvis;
        canvas_redraw(x);
        return 0;
    }


    /* ------- magnetic connector ------- */

    if (down
        /* && kbdnav->kn_moddown */
        && (keynum == 103 || keynum == 71) /* g or G*/
        && !x->gl_editor->e_textedfor
        && kbdnav->kn_state == KN_IO_SELECTED)
    {
            if( !kbdnav->kn_magtrav )
            {
                kbdnav_magnetic_connect_start(x);
                kbdnav_magnetic_connect(x);
                return 0;
            }
            else
            {
                if ( keynum == 71 /* upper case (G) means shift is pressed */
                     && kbdnav->kn_magtrav )
                {
                    kbdnav_magnetic_disconnect(x);
                }
                kbdnav_magnetic_connect_next(x);
                return 0;
            }
    }

    /* ------- digit connector ------- */

    /* if we received a digit and we are waiting to make a connection */
    if (down
        && keynum >= 48 && keynum <= 57
        && kbdnav->kn_state == KN_WAITING_NUMBER)
    {
        kbdnav->kn_chosennumber = keynum-48;
        kbdnav_digitconnect_choose(x, 1);
        return 0;
    }

    /* if the user press shift+digit it will arrive as symbol, like the following
       !@#$%¨&*()
       so we have to check for them */

    if (down && kbdnav->kn_state == KN_WAITING_NUMBER)
    {
        int shift_digit_pressed = 1;
        switch(keynum)
        {
            case 33:
                kbdnav->kn_chosennumber = 1;
                break;
            case 64:
                kbdnav->kn_chosennumber = 2;
                break;
            case 35:
                kbdnav->kn_chosennumber = 3;
                break;
            case 36:
                kbdnav->kn_chosennumber = 4;
                break;
            case 37:
                kbdnav->kn_chosennumber = 5;
                break;
            case 168:
                kbdnav->kn_chosennumber = 6;
                break;
            case 38:
                kbdnav->kn_chosennumber = 7;
                break;
            case 42:
                kbdnav->kn_chosennumber = 8;
                break;
            case 40:
                kbdnav->kn_chosennumber = 9;
                break;
            case 41:
                kbdnav->kn_chosennumber = 0;
                break;
            default:
                shift_digit_pressed = 0;
                break;
        }

        if(shift_digit_pressed)
        {
            kbdnav_digitconnect_choose(x, 0);
            return 0;
        }
    }

    if (down & !strcmp(gotkeysym->s_name, "F3"))
    {
        kbdnav_debug(x);
        return 0;
    }

    /* check for backspace or clear */
    if (keynum == 8 || keynum == 127)
    {
        if(kbdnav->kn_state != KN_INACTIVE)
        {
            kbdnav_delete_connection(x);
            return 0;
        }
        else
            return 1;
    }

    return 1;
}

int kbdnav_count_selected_objects(t_canvas *x)
{
    t_selection *curr_sel_obj;
    int selected_count = 0;
    for (curr_sel_obj = x->gl_editor->e_selection; curr_sel_obj; curr_sel_obj = curr_sel_obj->sel_next)
    {
        ++selected_count;
    }
    return selected_count;
}


t_object *kbdnav_get_selected_obj(t_canvas *x)
{
    t_object *output = pd_checkobject( &(x->gl_editor->e_selection->sel_what->g_pd) );
    return output;
}

void kbdnav_up(t_canvas *x, int shift)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);

    if( kbdnav->kn_state == KN_INACTIVE )
    {
        kbdnav_activate_kbdnav(x, IO_INLET);
        return;
    }

    kbdnav->kn_selobj = kbdnav_get_selected_obj(x);

    if( kbdnav->kn_state == KN_IO_SELECTED )
    {
        switch(kbdnav->kn_iotype)
        {
            case IO_OUTLET:
                /*we are on an outlet -> go to inlet */
                if ( obj_ninlets(kbdnav->kn_selobj) > 0)
                {
                    /* there are inlets */
                    kbdnav->kn_iotype = IO_INLET;
                    kbdnav->kn_ioindex = kbdnav->kn_ioindex > obj_ninlets(kbdnav->kn_selobj)-1 ? obj_ninlets(kbdnav->kn_selobj)-1 : kbdnav->kn_ioindex;
                }
                break;
            case IO_INLET:
                /* we are on an inlet. */
                if( shift )
                {
                    /* initialize numeric magnetic connection */
                    kbdnav->kn_state = KN_WAITING_NUMBER;
                    kbdnav_magnetic_connect_start(x);
                }
                else
                {
                    /* search for connections */
                    int n_in_sources = kbdnav_number_of_inlet_sources(x, kbdnav->kn_selobj, kbdnav->kn_ioindex);
                    if( n_in_sources > 0)
                    {
                        /* go to the first connection */
                        kbdnav_traverse_inlet_sources_start(x);
                    }
                }
                break;
            default:
                bug("kbdnav_up io selected");
        }
    }
    else if( kbdnav->kn_state == KN_CONN_SELECTED )
    {
        t_object *obj = kbdnav->kn_linetraverser->tr_ob;
        t_gobj *gobj = &obj->ob_g;
        switch(kbdnav->kn_iotype)
        {
            case IO_INLET:
                /* if we ARE selecting a connection and we are going UP
                  go down to that connection's source */

                /* select the object */
                glist_noselect(x);
                glist_select(x, gobj);

                /* activate it (set related kbd_nav variables) */
                kbdnav_activate_object(x,
                    kbdnav->kn_linetraverser->tr_ob, IO_OUTLET);
                kbdnav->kn_ioindex = kbdnav->kn_linetraverser->tr_outno;
                kbdnav->kn_outconnect = NULL;
                break;
            case IO_OUTLET:
                /* if we ARE selecting a connection and we are going DOWN
                  go back to inlet selection */
                kbdnav->kn_state = KN_IO_SELECTED;
                kbdnav->kn_outconnect = NULL;
                kbdnav_magnetic_connect_free(x);
                break;
            default:
                bug("kbdnav_up conn selected");
        }
    }
    else if(kbdnav->kn_state == KN_WAITING_NUMBER )
    {
        switch( kbdnav->kn_iotype )
        {
            case IO_INLET:
                if( shift && kbdnav->kn_moddown)
                {
                    kbdnav->kn_state = KN_IO_SELECTED;
                    /* note we don't use the .c in the .x%lx.c */
                    char *str = "source_obj_n source_io_n";
                    sys_vgui("::dialog_kbdconnect::pdtk_kbdconnect_prefilled .x%lx \"\" \"\" %d %d\n",
                        x,
                        glist_getindex(x, &kbdnav->kn_selobj->ob_g),
                        kbdnav->kn_ioindex);
                    sys_vgui("::dialog_kbdconnect::set_focus .x%lx 0\n", x);
                    kbdnav->kn_indexvis = 1;
                }
                break;
            case IO_OUTLET:
                kbdnav->kn_state = KN_IO_SELECTED;
                break;
            default:
                bug("wrong iotype on kbdnav_up()");
        }
    }
}

void kbdnav_down(t_canvas *x, int shift)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    if( kbdnav->kn_state == KN_INACTIVE )
    {
        kbdnav_activate_kbdnav(x, IO_OUTLET);
        return;
    }

    kbdnav->kn_selobj = kbdnav_get_selected_obj(x);

    if(kbdnav->kn_state == KN_IO_SELECTED)
    {
        switch(kbdnav->kn_iotype)
        {
            case IO_INLET:
                if ( obj_noutlets(kbdnav->kn_selobj) > 0)
                {
                    /* there are outlets */
                    kbdnav->kn_iotype = IO_OUTLET;
                    kbdnav->kn_ioindex = kbdnav->kn_ioindex > obj_noutlets(kbdnav->kn_selobj)-1 ? obj_noutlets(kbdnav->kn_selobj)-1 : kbdnav->kn_ioindex;
                }
                break;
            case IO_OUTLET:
                if( shift )
                {
                    /* initialize numeric magnetic connection */
                    kbdnav->kn_state = KN_WAITING_NUMBER;
                    kbdnav_magnetic_connect_start(x);
                }
                else
                {
                    /* search for connections! */
                    /* get first outlet */
                    int n_of_connections = kbdnav_noutconnections(kbdnav->kn_selobj, kbdnav->kn_ioindex);
                    if( n_of_connections > 0)
                    {
                        kbdnav_traverse_outconnections_start(x);
                    }
                }
                break;
            default:
                bug("kbd down io selected");
        }
    }
    else if(kbdnav->kn_state == KN_CONN_SELECTED)
    {
        t_object *obj = kbdnav->kn_linetraverser->tr_ob2;
        t_gobj *gobj = &obj->ob_g;

        switch(kbdnav->kn_iotype)
        {
            case IO_OUTLET:
                /* if we ARE selecting a connection and we are going DOWN
                   go down to that connection's destination */

                /* select the object */
                glist_noselect(x);
                glist_select(x, gobj);

                /* activate it (set related kbd_nav variables) */
                kbdnav_activate_object(x,
                    kbdnav->kn_linetraverser->tr_ob2, IO_INLET); //IO_INLET doesn't matter here
                kbdnav->kn_ioindex = kbdnav->kn_linetraverser->tr_inno;
                kbdnav->kn_outconnect = NULL;
                break;
            case IO_INLET:
                /* if we ARE selecting a connection and we are going UP
                   go back to outlet selection */
                kbdnav->kn_outconnect = NULL;
                kbdnav->kn_state = KN_IO_SELECTED;
                kbdnav_magnetic_connect_free(x);
                break;
            default:
                bug("kbd down conn selected");
        }
    } else if(kbdnav->kn_state == KN_WAITING_NUMBER){
        switch( kbdnav->kn_iotype )
        {
            case IO_INLET:
                kbdnav->kn_state = KN_IO_SELECTED;
                break;
            case IO_OUTLET:
                if( shift && kbdnav->kn_moddown )
                {
                    kbdnav->kn_state = KN_IO_SELECTED;
                    /* note we don't use the .c in the .x%lx.c */
                    char *str = "dest_obj_n dest_io_n";
                    sys_vgui("::dialog_kbdconnect::pdtk_kbdconnect_prefilled .x%lx %d %d \"\" \"\"\n",
                        x,
                        glist_getindex(x, &kbdnav->kn_selobj->ob_g),
                        kbdnav->kn_ioindex);
                    kbdnav->kn_indexvis = 1;
                    sys_vgui("::dialog_kbdconnect::set_focus .x%lx 2\n", x);
                }
                break;
            default:
                bug("wrong iotype on kbdnav_down");
        }


    }
}

void kbdnav_left(t_canvas *x, int shift)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);

    if(kbdnav->kn_state == KN_INACTIVE){
        return;
    }

    if(kbdnav->kn_state == KN_IO_SELECTED)
    {
        if (kbdnav->kn_ioindex > 0)
            --kbdnav->kn_ioindex;
    }
    else if(kbdnav->kn_state == KN_CONN_SELECTED)
    {
        switch(kbdnav->kn_iotype)
        {
            case IO_OUTLET:
                kbdnav_traverse_outconnections_prev(x);
                break;
            case IO_INLET:
                kbdnav_traverse_inlet_sources_prev(x);
                break;
            default:
                bug("kbdnav left");
        }
    }
}

void kbdnav_right(t_canvas *x, int shift)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);

    if(kbdnav->kn_state == KN_INACTIVE){
        return;
    }

    if(kbdnav->kn_state == KN_IO_SELECTED)
    {
        int n = kbdnav->kn_iotype == IO_INLET ? obj_ninlets(kbdnav->kn_selobj) : obj_noutlets(kbdnav->kn_selobj);
        if ( kbdnav->kn_ioindex < n-1 )
            ++kbdnav->kn_ioindex;
    }
    else if(kbdnav->kn_state == KN_CONN_SELECTED)
    {
        switch(kbdnav->kn_iotype)
        {
            case IO_OUTLET:
                kbdnav_traverse_outconnections_next(x);
                break;
            case IO_INLET:
                kbdnav_traverse_inlet_sources_next(x);
                break;
            default:
                bug("kbdnav right");
        }
    }
}


/* this function sets the variables to start keyboard navigation */
void kbdnav_activate_object(t_canvas *x, t_object *obj, int io_type)
{
    if ( !x || !obj || !x->gl_editor)
    {
        bug("null pointer error on kbdnav_activate_object");
        return;
    }

    t_kbdnav *kbdnav = canvas_get_kbdnav(x);

    kbdnav->kn_iotype = io_type;
    switch( io_type )
    {
        case IO_INLET:
            if( obj_ninlets(obj) > 0)
            {
                kbdnav->kn_state = KN_IO_SELECTED;
                kbdnav->kn_ioindex = 0;
                kbdnav->kn_selobj = obj;
            }
            else
            {
                kbdnav_deactivate(x);
            }
            break;
        case IO_OUTLET:
            if( obj_noutlets(obj) > 0 )
            {
                kbdnav->kn_state = KN_IO_SELECTED;
                kbdnav->kn_ioindex = 0;
                kbdnav->kn_selobj = obj;
            }
            else
            {
                kbdnav_deactivate(x);
            }
            break;
        default:
            bug("kbdnav_activate_object");
    }
    canvas_redraw(x);
}

int kbdnav_noutconnections(const t_object *x, int outlet_index)
{

    int count = 0;
    t_outconnect *connection;
    t_outlet *outlet;
    connection = obj_starttraverseoutlet(x, &outlet, outlet_index);
    t_object *destination_object;
    t_inlet *inlet;
    int which_what;
    while ( connection )
    {
        connection = obj_nexttraverseoutlet(connection, &destination_object, &inlet, &which_what);
        ++count;
    }
    return count;
}

/* this prepares the traversal for all the t_outconnections in the
   current outlet the user have seleced */
void kbdnav_traverse_outconnections_start(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    kbdnav->kn_connindex = 0 ;
    kbdnav->kn_state = KN_CONN_SELECTED;
    /* linetraverser traverses the whole patch (glist/t_canvas)
       so we must check both the object and the connection */

    linetraverser_start(kbdnav->kn_linetraverser, x);
    t_outconnect *oc = NULL;

    while (  oc = linetraverser_next(kbdnav->kn_linetraverser)  )
    {
        if(kbdnav->kn_linetraverser->tr_ob == kbdnav->kn_selobj)
        {
            if(kbdnav->kn_linetraverser->tr_outno == kbdnav->kn_ioindex)
                break;
        }
    }
    if( oc==NULL )
    {
        kbdnav->kn_state = KN_IO_SELECTED; /* Is this case a possibility? */
    }
    kbdnav->kn_outconnect = oc;
}

/* this traverses all the t_outconnections in the current outlet the user have selected */
void kbdnav_traverse_outconnections_next(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    int n_of_connections = kbdnav_noutconnections(kbdnav->kn_selobj, kbdnav->kn_ioindex);
    if (kbdnav->kn_connindex+1 == n_of_connections)
    {
        kbdnav_traverse_outconnections_start(x);
    }
    else if( n_of_connections > 0)
    {
        ++kbdnav->kn_connindex;
        kbdnav->kn_outconnect = linetraverser_next(kbdnav->kn_linetraverser);
    }
    else
    {
        /* n_of_connections == 0 happens when you delete the last connection with the Delete key*/
        kbdnav->kn_state = KN_IO_SELECTED;
    }
}

void kbdnav_traverse_outconnections_prev(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    int n_of_connections = kbdnav_noutconnections(kbdnav->kn_selobj, kbdnav->kn_ioindex);
    int target_connection = (kbdnav->kn_connindex == 0) ? n_of_connections-1 : kbdnav->kn_connindex-1;

    kbdnav_traverse_outconnections_start(x);
    for (int i = 0; i < target_connection; ++i)
        kbdnav_traverse_outconnections_next(x);
}

/* gets the number of connections whose destination is the given inlet of a given object */
int kbdnav_number_of_inlet_sources(t_canvas *x, t_object *obj, int inlet_index)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    int count = 0;
    linetraverser_start(kbdnav->kn_linetraverser, x);
    while ( linetraverser_next(kbdnav->kn_linetraverser) )
    {
        if( kbdnav->kn_linetraverser->tr_ob2 == kbdnav->kn_selobj
            && kbdnav->kn_linetraverser->tr_inno == inlet_index )
        {
            ++count;
        }
    }
    return count;
}

void kbdnav_traverse_inlet_sources_start(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);

    kbdnav->kn_state = KN_CONN_SELECTED;
    kbdnav->kn_connindex = 0 ;
    /* linetraverser traverses the whole patch (glist/t_canvas)
       so we must check both the object and the connection */

    linetraverser_start(kbdnav->kn_linetraverser, x);
    t_outconnect *oc = NULL;

    while (  oc = linetraverser_next(kbdnav->kn_linetraverser)  )
    {
        if(kbdnav->kn_linetraverser->tr_ob2 == kbdnav->kn_selobj)
        {
            if(kbdnav->kn_linetraverser->tr_inno == kbdnav->kn_ioindex)
                break;
        }
    }
    if( oc==NULL )
    {
        /* happens when you delete the last connection with the Delete key */
        kbdnav->kn_state = KN_IO_SELECTED;
    }
    kbdnav->kn_outconnect = oc;
}


/* this traverses all the t_outconnections in the current outlet the user have seleced, one by one */
void kbdnav_traverse_inlet_sources_next(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);

    int n_of_connections = kbdnav_number_of_inlet_sources(x, kbdnav->kn_selobj, kbdnav->kn_ioindex);

    if (kbdnav->kn_connindex+1 == n_of_connections)
    {
        kbdnav_traverse_inlet_sources_start(x);
    }
    else if(n_of_connections > 0)
    {
        /* search for the next connection */
        linetraverser_start(kbdnav->kn_linetraverser, x);
        ++kbdnav->kn_connindex;
        int count = 0;
        t_outconnect *oc;
        while (  oc = linetraverser_next(kbdnav->kn_linetraverser)  )
        {
            if( kbdnav->kn_linetraverser->tr_ob2 == kbdnav->kn_selobj
                && kbdnav->kn_linetraverser->tr_inno == kbdnav->kn_ioindex)
            {
                count++;
                if( count == kbdnav->kn_connindex+1)
                  break;
            }
        }
        kbdnav->kn_outconnect = oc;
    }
    else
    {
        /* happens when you delete the last connection with the Delete key */
        kbdnav->kn_state = KN_IO_SELECTED;
    }
}

void kbdnav_traverse_inlet_sources_prev(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    int n_of_connections = kbdnav_number_of_inlet_sources(x, kbdnav->kn_selobj, kbdnav->kn_ioindex);
    int target_connection = kbdnav->kn_connindex == 0 ? n_of_connections-1 : kbdnav->kn_connindex-1;

    kbdnav_traverse_inlet_sources_start(x);
    for (int i = 0; i<target_connection; ++i){
        kbdnav_traverse_inlet_sources_next(x);
    }
}

void kbdnav_displayindices(t_canvas *x)
{
    t_gobj *gobj = x->gl_list;
    int i = 0;
    while ( gobj )
    {
        t_object *obj = pd_checkobject(&gobj->g_pd);
        int objindex = canvas_getindex(x,gobj);
        sys_vgui(".x%lx.c create text %d %d -text {%d} -tags [list objindex%d obj_index_number] -fill blue\n",
                x,
                obj->te_xpix-10, obj->te_ypix+5,
                i,
                objindex);
        //https://wiki.tcl-lang.org/page/canvas+text
        sys_vgui(".x%lx.c create rect [.x%lx.c bbox objindex%d]  -fill white -outline blue -tag [list objindex_bkg%d obj_index_number_bkg]\n",
                x,
                x,
                objindex,
                objindex);
        sys_vgui(".x%lx.c lower objindex_bkg%d objindex%d\n",
                x,
                objindex,
                objindex);
        ++i;
        gobj = gobj->g_next;
    }
}


int kbdnav_closest(const void* a, const void* b)
{
    t_magtrav *x = (t_magtrav *)a;
    t_magtrav *y = (t_magtrav *)b;
    return x->mt_dist > y->mt_dist;
}

/* this function initializes the linked list of possible connections
   to be used by both the magnetic connector and the digit connector */
void kbdnav_magnetic_connect_start(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    int maxdist = 300;
    int outlet_sel = kbdnav->kn_iotype == IO_OUTLET;

    /* info about the currently selected in/outlet */

    t_object *selobj = kbdnav->kn_selobj;
    int selobj_io = kbdnav->kn_ioindex;
    /* positional information about current selected in/outlet */
    int sel_io_xpos, sel_io_ypos, width_not_used, height_not_used;
    kbdnav_io_pos(x,
        kbdnav->kn_selobj,
        selobj_io,
        kbdnav->kn_iotype,
        &sel_io_xpos, &sel_io_ypos,
        &width_not_used, &height_not_used);

    int objcount = 0;
    t_gobj *curr;

    kbdnav->kn_magtrav = (t_magtrav *)getbytes(sizeof(*kbdnav->kn_magtrav));
        kbdnav->kn_magtrav->mt_next = NULL;
    t_magtrav *magtravhead = kbdnav->kn_magtrav;
    int init = 0;
    int foundqty = 0;

    /* checking every object in the patch window */
    for(curr = x->gl_list;
        curr;
        curr = curr->g_next, ++objcount)
    {

        t_object *obj = pd_checkobject(&curr->g_pd);
        if ( obj == selobj ) {
            continue;
        }

        /* checking every inlet and outlet on current obj */
        int ioqty = outlet_sel ? obj_ninlets(obj) : obj_noutlets(obj);
        for (int curr_io_index = 0; curr_io_index < ioqty; ++curr_io_index)
        {
            /* checking every in/outlet on that object */
            int iox, ioy, iow, ioh;
            kbdnav_io_pos(x, obj,
                curr_io_index,
                kbdnav->kn_iotype == IO_OUTLET ? IO_INLET : IO_OUTLET,
                &iox, &ioy, &iow, &ioh);

            float dist = sqrt(  pow(sel_io_xpos-iox, 2)+pow(sel_io_ypos-ioy, 2)  );

            if ( dist < maxdist )
            {

                /* before finally adding it to the linked list
                   we check to see if there is already a connection */

                int already_connected;
                int is_conn_legal;

                if( kbdnav->kn_iotype == IO_OUTLET )
                {
                    already_connected = canvas_isconnected(x, selobj, selobj_io, obj, curr_io_index);
                    is_conn_legal = canvas_canconnect(x, selobj, selobj_io, obj, curr_io_index);
                }
                else
                {
                    already_connected = canvas_isconnected(x, obj, curr_io_index, selobj, selobj_io);
                    is_conn_legal = canvas_canconnect(x, obj, curr_io_index, selobj, selobj_io);
                }

                if ( already_connected || !is_conn_legal ){
                    continue;
                }

                /* finally add it to the linked list */
                if (init)
                {
                    /* initialize the linked list*/
                    magtravhead->mt_next = (t_magtrav *)getbytes(sizeof(*magtravhead->mt_next));
                    magtravhead = magtravhead->mt_next;
                    magtravhead->mt_next = NULL;
                }
                magtravhead->mt_dist = dist;
                magtravhead->mt_iotype = outlet_sel ? IO_INLET : IO_OUTLET;
                magtravhead->mt_objindex = objcount;
                magtravhead->mt_ioindex = curr_io_index;
                init = 1;
                ++foundqty;
            }
        }
    }
    if (foundqty==0)
    {
        kbdnav->kn_magtrav = NULL;
        kbdnav->kn_state = KN_IO_SELECTED;
        return;
    }

    /* sort it */
    for( t_magtrav *a = kbdnav->kn_magtrav ; a ; a = a->mt_next )
    {
        if ( a->mt_next ) /* TODO: is this needed or the condition "b" on the "for" is enough? */
        {
            for( t_magtrav *b = a->mt_next ; b ; b = b->mt_next )
            {
                /* swap if a is more distant than b */
                if( a->mt_dist > b->mt_dist)
                {
                    /* save b data */
                    double temp_dist = b->mt_dist;
                    int temp_iotype = b->mt_iotype;
                    int temp_objindex = b->mt_objindex;
                    int temp_ioindex = b->mt_ioindex;
                    /* put a on b */
                    b->mt_dist = a->mt_dist;
                    b->mt_iotype = a->mt_iotype;
                    b->mt_objindex = a->mt_objindex;
                    b->mt_ioindex = a->mt_ioindex;
                    /* put a saved data on b */
                    a->mt_dist = temp_dist;
                    a->mt_iotype = temp_iotype;
                    a->mt_objindex = temp_objindex;
                    a->mt_ioindex = temp_ioindex;
                }
            }
        }
    }
}

/* Goes to the next possible connection when using
   magnetic connect. (We free the elements on the
   linked list when they're not used so we have to
   start again upon reaching the last) */
void kbdnav_magnetic_connect_next(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);

    int selobj_index = glist_getindex(x, &kbdnav->kn_selobj->ob_g);
    if ( !kbdnav->kn_magtrav->mt_next )
    {
        /* start again */
        kbdnav_magnetic_connect_start(x);
        kbdnav_magnetic_connect(x);
    }
    else
    {
        /* go to the next in/outlet and free the last one*/
        t_magtrav *old = kbdnav->kn_magtrav;
        kbdnav->kn_magtrav = kbdnav->kn_magtrav->mt_next;
        freebytes(old, sizeof(*old));

        kbdnav_magnetic_connect(x);
    }
}

/* reads data from t_kbdnav and connects the selected in/outlet to
   the current out/inlet */
void kbdnav_magnetic_connect(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    if (!kbdnav->kn_magtrav) return;
    int selobj_index = glist_getindex(x, &kbdnav->kn_selobj->ob_g);

    /* info on the chosen object*/
    int mag_objindex;
    int mag_connindex;

    /* MAGNETIC CONNECTOR */
    /* if we are using the G key */
    if( kbdnav->kn_state != KN_WAITING_NUMBER )
    {
        mag_objindex = kbdnav->kn_magtrav->mt_objindex;
        mag_connindex = kbdnav->kn_magtrav->mt_ioindex;
    }
    else
    {
        /* DIGIT CONNECTOR */
        /* if we are using a number key */
        int count = 0;
        t_magtrav *mag = kbdnav->kn_magtrav;
        while( count <= kbdnav->kn_chosennumber)
        {
            if(!mag)
            {
                post("Digit connector error. Invalid number %d", kbdnav->kn_chosennumber);
                return;
            }
            mag_objindex = mag->mt_objindex;
            mag_connindex = mag->mt_ioindex;
            mag = mag->mt_next;
            ++count;
        }
    }

    t_object *sel_obj = kbdnav->kn_selobj;
    t_gobj *mag_gobj = glist_nth(x, mag_objindex);
    t_object *mag_obj = pd_checkobject(&mag_gobj->g_pd);

    switch( kbdnav->kn_iotype )
    {
        case IO_OUTLET:
            if (canvas_isconnected(x, sel_obj, kbdnav->kn_ioindex,
                    mag_obj, mag_connindex )  )
            {
                // post("already connected");
                return;
            }
            canvas_connect_with_undo(x,
             selobj_index, kbdnav->kn_ioindex,
             mag_objindex, mag_connindex );
            break;
        case IO_INLET:
            if (canvas_isconnected(x, mag_obj, mag_connindex,
                   sel_obj, kbdnav->kn_ioindex ))
            {
                // post("already connected");
                return;
            }
            canvas_connect_with_undo(x,
             mag_objindex, mag_connindex,
             selobj_index, kbdnav->kn_ioindex);
            break;
    }
}

void kbdnav_magnetic_disconnect(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    int selobj_index = glist_getindex(x, &kbdnav->kn_selobj->ob_g);

    int mag_objindex = kbdnav->kn_magtrav->mt_objindex;
    int mag_connindex = kbdnav->kn_magtrav->mt_ioindex;
    if( kbdnav->kn_iotype == IO_OUTLET )
    {
        canvas_disconnect_with_undo(x,
         selobj_index, kbdnav->kn_ioindex,
         mag_objindex, mag_connindex );
    }
    else
    {
        canvas_disconnect_with_undo(x,
         mag_objindex, mag_connindex,
         selobj_index, kbdnav->kn_ioindex);
    }

}

/* frees the linked list used for magnetic connection*/
void kbdnav_magnetic_connect_free(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    t_magtrav *m = kbdnav->kn_magtrav;
    while( m )
    {
        t_magtrav *old = m;
        m = m->mt_next;
        freebytes(old, sizeof(*old));
    }
    kbdnav->kn_magtrav = NULL;
}

/* display the numbers used by the digit connector*/
void kbdnav_digit_connect_display_numbers(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    int ioindex = 0;
    for( t_magtrav *mag = kbdnav->kn_magtrav ;
         mag && ioindex < 10 ;
         mag = mag->mt_next )
    {
        t_gobj *gobj = glist_nth(x, mag->mt_objindex);
        t_object *obj = pd_checkobject(&gobj->g_pd);

        int iox, ioy, iowidth, ioheight;
        int io_index = mag->mt_ioindex;
        int type = mag->mt_iotype;
        int objindex = canvas_getindex(x,gobj);
        kbdnav_io_pos(x, obj, io_index , type, &iox, &ioy, &iowidth, &ioheight);
        int delta_xpos = iowidth/2;
        sys_vgui(".x%lx.c create text %d %d -text {%d} -tags [list ioindex_%lx_%d_%d] -fill black\n",
                x,
                iox+delta_xpos,
                mag->mt_iotype == IO_OUTLET ? ioy+12 : ioy-10,
                ioindex,
                x, objindex, ioindex);
        sys_vgui(".x%lx.c create rect [.x%lx.c bbox ioindex_%lx_%d_%d]  -fill white -outline blue -tag [list ioindex_bkg_%lx_%d_%d obj_index_number_bkg]\n",
                x, x,
                x, objindex, ioindex,
                x, objindex, ioindex);
        sys_vgui(".x%lx.c lower ioindex_bkg_%lx_%d_%d ioindex_%lx_%d_%d\n",
                x,
                x, objindex, ioindex,
                x, objindex, ioindex);
        ++ioindex;
    }
}

/* called when the user types a number
   choosing a connection to be made */
void kbdnav_digitconnect_choose(t_canvas *x, int exit_after_connecting)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    kbdnav_magnetic_connect(x);
    if( exit_after_connecting ){
        kbdnav->kn_state = KN_IO_SELECTED;
        canvas_redraw(x); /* get rid of the numbers */
    }
}

void kbdnav_delete_connection(t_canvas *x)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);

    if( kbdnav->kn_state == KN_CONN_SELECTED )
    {
        int out_obj_index , out_conn_index;
        int in_obj_index, in_conn_index;

        if(kbdnav->kn_iotype == IO_OUTLET)
        {
            /* we are on an outlet */
            out_obj_index = canvas_getindex(x, &kbdnav->kn_selobj->ob_g);
            out_conn_index = kbdnav->kn_linetraverser->tr_outno;

            t_object *obj = kbdnav->kn_linetraverser->tr_ob2;
            in_obj_index = canvas_getindex(x, &obj->ob_g);
            in_conn_index = kbdnav->kn_linetraverser->tr_inno;
        }
        else
        {
            /* we are on an inlet */
            t_object *obj = kbdnav->kn_linetraverser->tr_ob;
            out_obj_index = canvas_getindex(x, &obj->ob_g);
            out_conn_index = kbdnav->kn_linetraverser->tr_outno;

            in_obj_index = canvas_getindex(x, &kbdnav->kn_selobj->ob_g);
            in_conn_index = kbdnav->kn_linetraverser->tr_inno;
        }
        glist_selectline(x, kbdnav->kn_outconnect,
                            out_obj_index, out_conn_index,
                            in_obj_index, in_conn_index);
        canvas_clearline(x);
        if(kbdnav->kn_iotype == IO_OUTLET)
        {
            kbdnav_traverse_outconnections_next(x);
        }
        else
        {
            kbdnav_traverse_inlet_sources_next(x);
        }
        canvas_redraw(x);
    } else
    {
        /* not selecting a connection */
        /* Possibility: just run normal delete when the user press delete and is not selecting a connection? */
    }
}

    /* draw inlets and outlets for a text object or for a graph. */
void kbdnav_glist_drawiofor(t_glist *glist, t_object *ob, int firsttime,
    char *tag, int x1, int y1, int x2, int y2)
{
    int n = obj_noutlets(ob), nplus = (n == 1 ? 1 : n-1), i;
    int width = x2 - x1;
    int iow = IOWIDTH * glist->gl_zoom;
    int ih = IHEIGHT * glist->gl_zoom, oh = OHEIGHT * glist->gl_zoom;
    /* draw over border, so assume border width = 1 pixel * glist->gl_zoom */

    char* io_color;

    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if(!kbdnav){
        /* nested Graph-On-Parents */
        return;
    }

    /* outlets */
    for (i = 0; i < n; i++)
    {
        int ishighlighted = i == kbdnav->kn_ioindex
                            && ob == kbdnav->kn_selobj
                            && kbdnav->kn_iotype == IO_OUTLET;
        io_color = ishighlighted ? kbdnav->kn_highlight : "black" ;

        int onset = x1 + (width - iow) * i / nplus;

        if (firsttime)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d "
                "-tags [list %so%d outlet] -fill %s -outline %s\n",
                glist_getcanvas(glist),
                onset, y2 - oh + glist->gl_zoom,
                onset + iow, y2,
                tag, i,
                io_color, io_color);
        else
            sys_vgui(".x%lx.c coords %so%d %d %d %d %d\n",
                glist_getcanvas(glist), tag, i,
                onset, y2 - oh + glist->gl_zoom,
                onset + iow, y2);
        if (ishighlighted)
        {
            /* delta_pad makes the in/outlet selection retangle bigger to give a visual hint
               the user is navigating through connections */
            int delta_pad = kbdnav->kn_state==KN_CONN_SELECTED || kbdnav->kn_state==KN_WAITING_NUMBER ? 1 : 0;
            if (firsttime)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d"
                         " -tags [list %soh%d outlet_highlight] -outline blue\n",
                    glist_getcanvas(glist),
                    onset - KBDNAV_SEL_PAD_X - delta_pad       , y2 - oh + glist->gl_zoom - KBDNAV_SEL_PAD_Y - delta_pad,
                    onset + iow + KBDNAV_SEL_PAD_X + delta_pad , y2 + KBDNAV_SEL_PAD_Y + delta_pad,
                    tag,i);
            else
                sys_vgui(".x%lx.c coords %soh%d %d %d %d %d\n",
                    glist_getcanvas(glist), tag, i,
                    onset-KBDNAV_SEL_PAD_X - delta_pad         , y2 - oh + glist->gl_zoom - KBDNAV_SEL_PAD_Y - delta_pad,
                    onset + iow + KBDNAV_SEL_PAD_X + delta_pad , y2 + KBDNAV_SEL_PAD_Y + delta_pad);
        }
    }

    /* inlets */
    n = obj_ninlets(ob);
    nplus = (n == 1 ? 1 : n-1);
    for (i = 0; i < n; i++)
    {
        int ishighlighted = i == kbdnav->kn_ioindex
                            && ob == kbdnav->kn_selobj
                            && kbdnav->kn_iotype == IO_INLET;
        io_color = ishighlighted ? kbdnav->kn_highlight : "black";

        int onset = x1 + (width - iow) * i / nplus;
        if (firsttime)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d "
                "-tags [list %si%d inlet] -fill %s -outline %s\n",
                glist_getcanvas(glist),
                onset, y1,
                onset + iow, y1 + ih - glist->gl_zoom,
                tag, i,
                io_color, io_color);
        else
            sys_vgui(".x%lx.c coords %si%d %d %d %d %d\n",
                glist_getcanvas(glist), tag, i,
                onset, y1,
                onset + iow, y1 + ih - glist->gl_zoom);
        if (ishighlighted)
        {
            /* delta_pad makes the in/outlet selection retangle bigger to give a visual hint
               the user is navigating through connections */
            int delta_pad = kbdnav->kn_state==KN_CONN_SELECTED || kbdnav->kn_state==KN_WAITING_NUMBER ? 1 : 0;
            if (firsttime)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d"
                         " -tags [list %sih%d inlet_highlight] -outline blue\n",
                    glist_getcanvas(glist),
                    onset - KBDNAV_SEL_PAD_X -delta_pad      , y1 - KBDNAV_SEL_PAD_Y - delta_pad,
                    onset + iow+KBDNAV_SEL_PAD_X + delta_pad , y1 + ih - glist->gl_zoom + KBDNAV_SEL_PAD_Y + delta_pad,
                    tag, i);
            else
                sys_vgui(".x%lx.c coords %sih%d %d %d %d %d\n",
                    glist_getcanvas(glist), tag, i,
                    onset - KBDNAV_SEL_PAD_X - delta_pad     , y1 - KBDNAV_SEL_PAD_Y - delta_pad,
                    onset + iow+KBDNAV_SEL_PAD_X + delta_pad , y1 + ih - glist->gl_zoom + KBDNAV_SEL_PAD_Y + delta_pad);
        }
    }
}

void kbdnav_canvas_drawlines(t_canvas *x)
{
    t_linetraverser t;
    t_outconnect *oc;

    // color of the selected connection
    char* selected_color = "blue";
    /* color for the unselected connections on that same in/outlet
       (connection swith other in/outlets will be black) */
    char* unselected_color = "#888888";
    char* line_color;

    int yoffset = x->gl_zoom; /* slight offset to hide thick line corners */
    {
        linetraverser_start(&t, x);
        while ((oc = linetraverser_next(&t)))
        {
            line_color = "black";
            t_kbdnav *kbdnav = canvas_get_kbdnav(x);

            if ( kbdnav && kbdnav->kn_state == KN_CONN_SELECTED )
            {
                char* startEnd;
                switch( kbdnav->kn_iotype )
                {
                    case IO_OUTLET:
                        line_color = t.tr_ob == kbdnav->kn_selobj
                                     && kbdnav->kn_ioindex == t.tr_outno
                                     ? unselected_color : "black";
                         startEnd = "last";
                        break;
                    case IO_INLET:
                        line_color = t.tr_ob2 == kbdnav->kn_selobj
                                     && kbdnav->kn_ioindex == t.tr_inno
                                     ? unselected_color : "black";
                         startEnd = "first";
                        break;
                    default:
                        bug("invalid iotype on canvas_drawlines()");
                }
                if ( oc == kbdnav->kn_outconnect ){
                    line_color = selected_color;
                    sys_vgui(".x%lx.c create line %d %d %d %d -width %d -tags [list l%lx cord] -fill %s -arrow %s\n",
                            glist_getcanvas(x),
                            t.tr_lx1, t.tr_ly1 - yoffset, t.tr_lx2, t.tr_ly2 + yoffset,
                            (outlet_getsymbol(t.tr_outlet) == &s_signal ? 2:1) * x->gl_zoom,
                            oc,
                            line_color,
                            startEnd);
                }
                else
                {
                    line_color = line_color;
                    sys_vgui(".x%lx.c create line %d %d %d %d -width %d -tags [list l%lx cord] -fill %s\n",
                            glist_getcanvas(x),
                            t.tr_lx1, t.tr_ly1 - yoffset, t.tr_lx2, t.tr_ly2 + yoffset,
                            (outlet_getsymbol(t.tr_outlet) == &s_signal ? 2:1) * x->gl_zoom,
                            oc,
                            line_color);
                }
            }
            else
            {
                sys_vgui(".x%lx.c create line %d %d %d %d -width %d -tags [list l%lx cord] -fill %s\n",
                        glist_getcanvas(x),
                        t.tr_lx1, t.tr_ly1 - yoffset, t.tr_lx2, t.tr_ly2 + yoffset,
                        (outlet_getsymbol(t.tr_outlet) == &s_signal ? 2:1) * x->gl_zoom,
                        oc,
                        line_color);
            }
        }
    }
}


/* ---------- Functions related to iemgui ----------  */

/* note that there might be no editor if we are being drawn inside a GOP abstraction
 * like if the object's pos is being controlled programatically through an
 * |pos $1 $2( msg, for example * for that reason we always check if(!editor) first */

/* This method draws the in/outlet selection for a [hsl]
 * Since some stuff used in g_hslider.c are #defined locally
 * it takes everything it needs as arguments */
void kbdnav_hslider_draw_io_selection(t_hslider *x, t_glist *glist, int xpos, int ypos, int lmargin, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

     if(kbdnav->kn_selobj == &x->x_gui.x_obj)
     {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET )
        {
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxOUT_KBDNAV_SEL%d outlet]\n",
                 canvas,
                 xpos - lmargin-KBDNAV_SEL_PAD_X, ypos + x->x_gui.x_h + zoom - ioh-KBDNAV_SEL_PAD_Y,
                 xpos - lmargin + iow +KBDNAV_SEL_PAD_X, ypos + x->x_gui.x_h + KBDNAV_SEL_PAD_Y,
                 x, 0);
        }
        if( kbdnav->kn_iotype == IO_INLET )
        {
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxIN_KBDNAV_SEL%d inlet]\n",
                 canvas,
                 xpos - lmargin-KBDNAV_SEL_PAD_X, ypos-KBDNAV_SEL_PAD_Y,
                 xpos - lmargin + iow +KBDNAV_SEL_PAD_X, ypos - zoom + ioh + KBDNAV_SEL_PAD_Y,
                 x, 0);
        }
     }
}

void kbdnav_hslider_move(t_hslider* x, t_glist* glist, int xpos, int ypos, int lmargin, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

    /* check for private because we might be inside an abstraction with active GOP
     * In that case there is no t_editor */
    if( kbdnav->kn_selobj == &x->x_gui.x_obj )
    {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET )
            sys_vgui(".x%lx.c coords %lxOUT_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xpos - lmargin-KBDNAV_SEL_PAD_X,
                 ypos + x->x_gui.x_h + zoom - ioh - KBDNAV_SEL_PAD_Y,
                 xpos - lmargin + iow + KBDNAV_SEL_PAD_X,
                 ypos + x->x_gui.x_h + KBDNAV_SEL_PAD_Y);
        if( kbdnav->kn_iotype == IO_INLET )
            sys_vgui(".x%lx.c coords %lxIN_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xpos - lmargin - KBDNAV_SEL_PAD_X,
                 ypos - KBDNAV_SEL_PAD_Y,
                 xpos - lmargin + iow + KBDNAV_SEL_PAD_X,
                 ypos - zoom + ioh + KBDNAV_SEL_PAD_Y);
    }
}

/* This method draws the in/outlet selection for a [vsl]
 * Since some stuff used in g_hslider.c are #defined locally
 * it takes everything it needs as arguments */
void kbdnav_vslider_draw_io_selection(t_vslider *x, t_glist *glist, int xpos, int ypos, int tmargin, int bmargin, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

     if(kbdnav->kn_selobj == &x->x_gui.x_obj)
     {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET ){
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxOUT_KBDNAV_SEL%d outlet]\n",
                 canvas,
                 xpos -KBDNAV_SEL_PAD_X,
                 ypos + x->x_gui.x_h + bmargin + zoom - ioh - KBDNAV_SEL_PAD_Y,
                 xpos + iow + KBDNAV_SEL_PAD_X,
                 ypos + x->x_gui.x_h + bmargin + KBDNAV_SEL_PAD_Y,
                 x, 0);
        }
        if( kbdnav->kn_iotype == IO_INLET ){
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxIN_KBDNAV_SEL%d inlet]\n",
                 canvas,
                 xpos - KBDNAV_SEL_PAD_X,
                 ypos - tmargin - KBDNAV_SEL_PAD_Y,
                 xpos + iow + KBDNAV_SEL_PAD_X,
                 ypos - tmargin - zoom + ioh + KBDNAV_SEL_PAD_Y,
                 x, 0);
        }
     }
}

void kbdnav_vslider_move(t_vslider* x, t_glist* glist, int xpos, int ypos, int tmargin, int bmargin, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

    /* check for private because we might be inside an abstraction with active GOP
     * In that case there is no t_editor */
    if( kbdnav->kn_selobj == &x->x_gui.x_obj )
    {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET )
            if(!x->x_gui.x_fsf.x_snd_able)
                sys_vgui(".x%lx.c coords %lxOUT_KBDNAV_SEL%d %d %d %d %d\n",
                     canvas, x, 0,
                     xpos - KBDNAV_SEL_PAD_X ,
                     ypos + x->x_gui.x_h + bmargin + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                     xpos + iow + KBDNAV_SEL_PAD_X,
                     ypos + x->x_gui.x_h + bmargin + KBDNAV_SEL_PAD_Y);
        if( kbdnav->kn_iotype == IO_INLET )
            if(!x->x_gui.x_fsf.x_rcv_able)
                sys_vgui(".x%lx.c coords %lxIN_KBDNAV_SEL%d %d %d %d %d\n",
                     canvas, x, 0,
                     xpos - KBDNAV_SEL_PAD_X,
                     ypos - tmargin - KBDNAV_SEL_PAD_Y,
                     xpos + iow + KBDNAV_SEL_PAD_X,
                     ypos - tmargin - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y);
    }
}

void kbdnav_bng_draw_io_selection(t_bng *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

     if(kbdnav->kn_selobj == &x->x_gui.x_obj)
     {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET ){
            if(!x->x_gui.x_fsf.x_snd_able)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxOUT_KBDNAV_SEL%d outlet]\n",
                     canvas,
                     xpos - KBDNAV_SEL_PAD_X,
                     ypos + x->x_gui.x_h + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                     xpos + iow + KBDNAV_SEL_PAD_X,
                     ypos + x->x_gui.x_h + KBDNAV_SEL_PAD_Y,
                     x, 0);
        }
        if( kbdnav->kn_iotype == IO_INLET ){
            if(!x->x_gui.x_fsf.x_rcv_able)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxIN_KBDNAV_SEL%d inlet]\n",
                     canvas,
                     xpos - KBDNAV_SEL_PAD_X,
                     ypos - KBDNAV_SEL_PAD_Y,
                     xpos + iow + KBDNAV_SEL_PAD_X,
                     ypos - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y,
                     x, 0);
        }
     }
}

void kbdnav_bng_move(t_bng *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

    if( kbdnav->kn_selobj == &x->x_gui.x_obj )
    {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET )
            sys_vgui(".x%lx.c coords %lxOUT_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xpos - KBDNAV_SEL_PAD_X,
                 ypos + x->x_gui.x_h + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                 xpos + iow + KBDNAV_SEL_PAD_X,
                 ypos + x->x_gui.x_h + KBDNAV_SEL_PAD_Y);
        if( kbdnav->kn_iotype == IO_INLET )
            sys_vgui(".x%lx.c coords %lxIN_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xpos - KBDNAV_SEL_PAD_X,
                 ypos - KBDNAV_SEL_PAD_Y,
                 xpos + iow + KBDNAV_SEL_PAD_X,
                 ypos - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y);
    }
}

void kbdnav_toggle_draw_io_selection(t_toggle *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

     if(kbdnav->kn_selobj == &x->x_gui.x_obj)
     {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET ){
            if(!x->x_gui.x_fsf.x_snd_able)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxOUT_KBDNAV_SEL%d outlet]\n",
                     canvas,
                     xpos - KBDNAV_SEL_PAD_X,
                     ypos + x->x_gui.x_h + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                     xpos + iow + KBDNAV_SEL_PAD_X,
                     ypos + x->x_gui.x_h + KBDNAV_SEL_PAD_Y,
                     x, 0);
        }
        if( kbdnav->kn_iotype == IO_INLET ){
            if(!x->x_gui.x_fsf.x_rcv_able)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxIN_KBDNAV_SEL%d inlet]\n",
                     canvas,
                     xpos - KBDNAV_SEL_PAD_X,
                     ypos - KBDNAV_SEL_PAD_Y,
                     xpos + iow + KBDNAV_SEL_PAD_X,
                     ypos - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y,
                     x, 0);
        }
     }
}

void kbdnav_toggle_move(t_toggle *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

    if( kbdnav->kn_selobj == &x->x_gui.x_obj )
    {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET )
            sys_vgui(".x%lx.c coords %lxOUT_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xpos - KBDNAV_SEL_PAD_X,
                 ypos + x->x_gui.x_h + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                 xpos + iow + KBDNAV_SEL_PAD_X,
                 ypos + x->x_gui.x_h + KBDNAV_SEL_PAD_Y);
        if( kbdnav->kn_iotype == IO_INLET )
            sys_vgui(".x%lx.c coords %lxIN_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xpos - KBDNAV_SEL_PAD_X,
                 ypos - KBDNAV_SEL_PAD_Y,
                 xpos + iow + KBDNAV_SEL_PAD_X,
                 ypos - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y);
    }
}

void kbdnav_vradio_draw_io_selection(t_vradio *x, t_glist *glist, int xx11, int yy11, int yy11b, int zoom, int iow, int ioh)
{
  t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
  if (!kbdnav)
     return; /* no editor because we're inside a GOP */

  if(kbdnav->kn_selobj == &x->x_gui.x_obj)
  {
     t_canvas *canvas = glist_getcanvas(glist);
     if( kbdnav->kn_iotype == IO_OUTLET ){
         if(!x->x_gui.x_fsf.x_snd_able)
             sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxOUT_KBDNAV_SEL%d outlet]\n",
                  canvas,
                  xx11 - KBDNAV_SEL_PAD_X,
                  yy11 + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                  xx11 + iow + KBDNAV_SEL_PAD_X,
                  yy11 + KBDNAV_SEL_PAD_Y,
                  x, 0);
     }
     if( kbdnav->kn_iotype == IO_INLET ){
         if(!x->x_gui.x_fsf.x_rcv_able)
             sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxIN_KBDNAV_SEL%d inlet]\n",
                  canvas,
                  xx11 - KBDNAV_SEL_PAD_X,
                  yy11b - KBDNAV_SEL_PAD_Y,
                  xx11 + iow + KBDNAV_SEL_PAD_X,
                  yy11b - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y,
                  x, 0);
     }
  }

}
void kbdnav_vradio_move(t_vradio *x, t_glist *glist, int xx11, int yy11, int yy11b, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

    if( kbdnav->kn_selobj == &x->x_gui.x_obj )
    {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET )
            sys_vgui(".x%lx.c coords %lxOUT_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xx11 - KBDNAV_SEL_PAD_X,
                 yy11 + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                 xx11 + iow + KBDNAV_SEL_PAD_X,
                 yy11 + KBDNAV_SEL_PAD_Y);
        if( kbdnav->kn_iotype == IO_INLET )
            sys_vgui(".x%lx.c coords %lxIN_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xx11 - KBDNAV_SEL_PAD_X,
                 yy11b - KBDNAV_SEL_PAD_Y,
                 xx11 + iow + KBDNAV_SEL_PAD_X,
                 yy11b - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y);
    }
}

void kbdnav_hradio_draw_io_selection(t_hradio *x, t_glist *glist, int xx11b, int yy12, int yy11, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
       return; /* no editor because we're inside a GOP */


    if(kbdnav->kn_selobj == &x->x_gui.x_obj)
    {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET ){
         if(!x->x_gui.x_fsf.x_snd_able)
             sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxOUT_KBDNAV_SEL%d outlet]\n",
                  canvas,
                  xx11b - KBDNAV_SEL_PAD_X,
                  yy12 + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                  xx11b + iow + KBDNAV_SEL_PAD_X,
                  yy12 + KBDNAV_SEL_PAD_Y,
                  x, 0);
        }
        if( kbdnav->kn_iotype == IO_INLET ){
         if(!x->x_gui.x_fsf.x_rcv_able)
             sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxIN_KBDNAV_SEL%d inlet]\n",
                  canvas,
                  xx11b - KBDNAV_SEL_PAD_X,
                  yy11 - KBDNAV_SEL_PAD_Y,
                  xx11b + iow + KBDNAV_SEL_PAD_X,
                  yy11 - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y,
                  x, 0);
        }
    }
}

void kbdnav_hradio_move(t_hradio *x, t_glist *glist, int xx11b, int yy12, int yy11, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

    if( kbdnav->kn_selobj == &x->x_gui.x_obj )
    {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET )
            sys_vgui(".x%lx.c coords %lxOUT_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xx11b - KBDNAV_SEL_PAD_X,
                 yy12 + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                 xx11b + iow + KBDNAV_SEL_PAD_X,
                 yy12 + KBDNAV_SEL_PAD_Y);
        if( kbdnav->kn_iotype == IO_INLET )
            sys_vgui(".x%lx.c coords %lxIN_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xx11b - KBDNAV_SEL_PAD_X,
                 yy11 - KBDNAV_SEL_PAD_Y,
                 xx11b + iow + KBDNAV_SEL_PAD_X,
                 yy11 - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y);
    }
}

void kbdnav_vu_draw_io_selection(t_vu *x, t_glist *glist, int xpos, int ypos, int hmargin, int vmargin, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
       return; /* no editor because we're inside a GOP */

  if(kbdnav->kn_selobj == &x->x_gui.x_obj)
  {
     t_canvas *canvas = glist_getcanvas(glist);
     if( kbdnav->kn_iotype == IO_OUTLET )
     {
         if(!x->x_gui.x_fsf.x_snd_able)
         {
            if(kbdnav->kn_ioindex == 0)
            {
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxOUT_KBDNAV_SEL%d outlet]\n",
                    canvas,
                    xpos - hmargin - KBDNAV_SEL_PAD_X,
                    ypos + x->x_gui.x_h + vmargin + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                    xpos - hmargin + iow + KBDNAV_SEL_PAD_X,
                    ypos + x->x_gui.x_h + vmargin + KBDNAV_SEL_PAD_Y,
                    x, 0);
            }
            else if(kbdnav->kn_ioindex == 1)
            {
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxOUT_KBDNAV_SEL%d outlet]\n",
                    canvas,
                    xpos + x->x_gui.x_w + hmargin - iow - KBDNAV_SEL_PAD_X,
                    ypos + x->x_gui.x_h + vmargin + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                    xpos + x->x_gui.x_w + hmargin + KBDNAV_SEL_PAD_X,
                    ypos + x->x_gui.x_h + vmargin + KBDNAV_SEL_PAD_Y,
                    x, 1);
            }
        }
     }
     if( kbdnav->kn_iotype == IO_INLET )
     {
         if(!x->x_gui.x_fsf.x_rcv_able)
         {
            if(kbdnav->kn_ioindex == 0)
            {
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxIN_KBDNAV_SEL%d inlet]\n",
                    canvas,
                    xpos - hmargin - KBDNAV_SEL_PAD_X,
                    ypos - vmargin - KBDNAV_SEL_PAD_Y,
                    xpos - hmargin + iow + KBDNAV_SEL_PAD_X,
                    ypos - vmargin - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y,
                    x, 0);
            }
            else if(kbdnav->kn_ioindex == 1)
            {
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxIN_KBDNAV_SEL%d inlet]\n",
                    canvas,
                    xpos + x->x_gui.x_w + hmargin - iow - KBDNAV_SEL_PAD_X,
                    ypos - vmargin - KBDNAV_SEL_PAD_Y,
                    xpos + x->x_gui.x_w + hmargin + KBDNAV_SEL_PAD_X,
                    ypos - vmargin - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y,
                    x, 1);
            }
         }
     }
  }
}

void kbdnav_vu_move(t_vu *x, t_glist *glist, int xpos, int ypos, int hmargin, int vmargin, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

    if( kbdnav->kn_selobj == &x->x_gui.x_obj )
    {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET )
        {
            if(kbdnav->kn_ioindex == 0)
            {
                sys_vgui(".x%lx.c coords %lxOUT_KBDNAV_SEL%d %d %d %d %d\n",
                     canvas, x, 0,
                     xpos - hmargin - KBDNAV_SEL_PAD_X,
                     ypos + x->x_gui.x_h + vmargin + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                     xpos - hmargin + iow + KBDNAV_SEL_PAD_X,
                     ypos + x->x_gui.x_h + vmargin + KBDNAV_SEL_PAD_Y);
            }
            else if(kbdnav->kn_ioindex == 1)
            {
                sys_vgui(".x%lx.c coords %lxOUT_KBDNAV_SEL%d %d %d %d %d\n",
                    canvas, x, 1,
                    xpos + x->x_gui.x_w + hmargin - iow - KBDNAV_SEL_PAD_X,
                    ypos + x->x_gui.x_h + vmargin + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                    xpos + x->x_gui.x_w + hmargin + KBDNAV_SEL_PAD_X,
                    ypos + x->x_gui.x_h + vmargin + KBDNAV_SEL_PAD_Y);
            }
        }
        if( kbdnav->kn_iotype == IO_INLET )
        {
            if(kbdnav->kn_ioindex == 0)
            {
            sys_vgui(".x%lx.c coords %lxIN_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xpos - hmargin - KBDNAV_SEL_PAD_X,
                 ypos - vmargin - KBDNAV_SEL_PAD_Y,
                 xpos - hmargin + iow + KBDNAV_SEL_PAD_X,
                 ypos - vmargin - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y);
            } else if(kbdnav->kn_ioindex == 1)
            {
                sys_vgui(".x%lx.c coords %lxIN_KBDNAV_SEL%d %d %d %d %d\n",
                     canvas, x, 1,
                     xpos + x->x_gui.x_w + hmargin - iow - KBDNAV_SEL_PAD_X,
                     ypos - vmargin - KBDNAV_SEL_PAD_Y,
                     xpos + x->x_gui.x_w + hmargin + KBDNAV_SEL_PAD_X,
                     ypos - vmargin - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y);
            }
        }
    }
}

void kbdnav_my_numbox_draw_io_selection(t_my_numbox *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

     if(kbdnav->kn_selobj == &x->x_gui.x_obj)
     {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET )
        {
            if(!x->x_gui.x_fsf.x_snd_able)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxOUT_KBDNAV_SEL%d outlet]\n",
                     canvas,
                     xpos - KBDNAV_SEL_PAD_X,
                     ypos + x->x_gui.x_h + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                     xpos + iow + KBDNAV_SEL_PAD_X,
                     ypos + x->x_gui.x_h + KBDNAV_SEL_PAD_Y,
                     x, 0);
        }
        if( kbdnav->kn_iotype == IO_INLET )
        {
            if(!x->x_gui.x_fsf.x_rcv_able)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline blue -tags [list %lxIN_KBDNAV_SEL%d inlet]\n",
                     canvas,
                     xpos - KBDNAV_SEL_PAD_X,
                     ypos - KBDNAV_SEL_PAD_Y,
                     xpos + iow + KBDNAV_SEL_PAD_X,
                     ypos - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y,
                     x, 0);
        }
     }
}

void kbdnav_my_numbox_move(t_my_numbox *x, t_glist *glist, int xpos, int ypos, int zoom, int iow, int ioh)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(glist);
    if (!kbdnav)
        return; /* no editor because we're inside a GOP */

    if( kbdnav->kn_selobj == &x->x_gui.x_obj )
    {
        t_canvas *canvas = glist_getcanvas(glist);
        if( kbdnav->kn_iotype == IO_OUTLET )
            sys_vgui(".x%lx.c coords %lxOUT_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xpos - KBDNAV_SEL_PAD_X,
                 ypos + x->x_gui.x_h + IEMGUI_ZOOM(x) - ioh - KBDNAV_SEL_PAD_Y,
                 xpos + iow + KBDNAV_SEL_PAD_X,
                 ypos + x->x_gui.x_h + KBDNAV_SEL_PAD_Y);
        if( kbdnav->kn_iotype == IO_INLET )
            sys_vgui(".x%lx.c coords %lxIN_KBDNAV_SEL%d %d %d %d %d\n",
                 canvas, x, 0,
                 xpos - KBDNAV_SEL_PAD_X,
                 ypos - KBDNAV_SEL_PAD_Y,
                 xpos + iow + KBDNAV_SEL_PAD_X,
                 ypos - IEMGUI_ZOOM(x) + ioh + KBDNAV_SEL_PAD_Y);
    }
}

void canvas_goto(t_canvas *x, t_floatarg indexarg)
{
    int index = indexarg;
    if ( index < 0)
    {
        post("go to error (negative index): %d", index);
        return;
    }
    t_gobj *gobj = x->gl_list;
    if ( !gobj )
    {
        error("canvas_goto() error: patch is empty.");
        return;
    }
    glist_noselect(x);
    int count = 0;
    while ( gobj->g_next )
    {
        if ( count == index )
            break;
        gobj = gobj->g_next;
        ++count;
    }
    if ( count < index )
    {
        post("go to error (no such element): %d. Patch only has %d elements", index,count);
        return;
    }
    canvas_redraw(x);
    if( !glist_isselected(x, gobj) )
    {
        t_editor *editor = x->gl_editor;
        if(editor)
        {
            t_kbdnav *kbdnav = canvas_get_kbdnav(x);
            kbdnav->kn_indexvis = 0;
        }
        glist_select(x, gobj);
        kbdnav_deactivate(x);
        canvas_redraw(x);
    }
    else
    {
        post("canvas_goto: object was already selected. index: %d", index);
    }
}

void canvas_set_indices_visibility(t_canvas *x, t_floatarg indexarg)
{
    int i = indexarg;
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    if(!kbdnav) return;
    kbdnav->kn_indexvis = i != 0;
    if( kbdnav->kn_indexvis )
        kbdnav_displayindices(x);
    else
        canvas_redraw(x);
}

void kbdnav_displaceselection(t_canvas *x, int dx, int dy, t_selection *sel)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    if( kbdnav && kbdnav->kn_indexvis)
    {
        t_gobj *gobj = sel->sel_what;
        t_object *obj = pd_checkobject(&gobj->g_pd);
        int objindex = canvas_getindex(x,gobj);
        sys_vgui(".x%lx.c coords objindex%d %d %d\n",
            x,
            objindex,
            obj->te_xpix-10, obj->te_ypix+5);
        sys_vgui(".x%lx.c coords objindex_bkg%d [.x%lx.c bbox objindex%d]\n",
            x,
            objindex,
            x,
            objindex);
    }
}

/* just like canvas_howputnew() but takes in consideration
 *  the current in/outlet the user has selected (if any)
 *
 * x1, y1, x2, y2 should be obtained with:
 * gobj_getrect(g, x, &x1, &y1, &x2, &y2);
 *
 *  returns 1 if the user is  keyboard navigating
 *  returns 0 otherwise */
int kbdnav_howputnew(t_canvas *x, t_gobj *g, int nobj, int *xpixp, int *ypixp,
    int x1, int y1, int x2, int y2)
{
    int indx = nobj;
    t_object *obj = pd_checkobject(&g->g_pd);
    t_kbdnav *kbdnav = canvas_get_kbdnav(x);
    int objheight = y2-y1;
    int iox, ioy, iow, ioh;

    if ( !kbdnav || kbdnav->kn_state == KN_INACTIVE )
        return 0;
    if ( !obj ) bug("canvas_howputnew keyboard navigation bug: !obj");
    switch ( kbdnav->kn_iotype )
    {
        case IO_OUTLET:
            kbdnav_io_pos(x, obj, kbdnav->kn_ioindex, kbdnav->kn_iotype, &iox, &ioy, &iow, &ioh);
            *xpixp = iox;
            *ypixp = ioy+ioh+5;
            break;
        case IO_INLET:
            kbdnav_io_pos(x, obj, kbdnav->kn_ioindex, kbdnav->kn_iotype, &iox, &ioy, &iow, &ioh);
            *xpixp = iox;
            *ypixp = ioy-5-objheight;
            break;
        default:
            bug("invalid io type on canvas_howputnew(): %d", kbdnav->kn_iotype);
    }
    return 1;
}

/* called when a new obj is created to determine its position
 * depending on the user selection of an in/outlet.
 *
 * returns 1 if the user was kbd navigating or 0 otherwise */
int kbdnav_connect_new(t_glist *gl, int nobj, int indx)
{
    t_kbdnav *kbdnav = canvas_get_kbdnav(gl);
    if ( !kbdnav || kbdnav->kn_state == KN_INACTIVE )
        return 0;
    switch( kbdnav->kn_iotype )
    {
        case IO_INLET:
            canvas_connect(gl, nobj, 0, indx, kbdnav->kn_ioindex);
            break;
        case IO_OUTLET:
            canvas_connect(gl, indx, kbdnav->kn_ioindex, nobj, 0);
            break;
        default:
            bug("invalid iotype on function kbdnav_connect_new()");
    }
    kbdnav_deactivate(gl);
    canvas_redraw(gl);
    return 1;
}