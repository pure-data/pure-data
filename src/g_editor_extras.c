/* Copyright (c) 2018 Miller Puckette, IOhannes m zmölnig and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */


#include "m_pd.h"

#include "g_canvas.h"
#include "m_imp.h"
#include "g_undo.h"


void *canvas_undo_set_pastebinbuf(t_canvas *x, t_binbuf *b,
    int numpasted, int duplicate, int d_offset);

/* ------------ utilities ---------- */
static t_gobj*o2g(t_object*obj)
{
    return &(obj->te_g);
}
static t_object*g2o(t_gobj*gobj)
{
    return pd_checkobject(&gobj->g_pd);
}
static t_gobj*glist_getlast(t_glist*cnv)
{
        /* get last object on <ncv> */
    t_gobj*result=NULL;
    for(result=cnv->gl_list; result->g_next;) result=result->g_next;
    return result;
}
static void dereconnect(t_glist*cnv, t_object*org, t_object*replace)
{
    t_gobj*gobj;
    int replace_i = canvas_getindex(cnv, o2g(replace));
    for(gobj=cnv->gl_list; gobj; gobj=gobj->g_next)
    {
        t_object*obj=g2o(gobj);
        int obj_i = canvas_getindex(cnv, gobj);
        int obj_nout=0;
        int nout;
        if(!obj)continue;
        obj_nout=obj_noutlets(obj);
        for(nout=0; nout<obj_nout; nout++)
        {
            t_outlet*out=0;
            t_outconnect*conn=obj_starttraverseoutlet(obj, &out, nout);
            while(conn)
            {
                int which;
                t_object*dest=0;
                t_inlet *in =0;
                int dest_i;
                conn=obj_nexttraverseoutlet(conn, &dest, &in, &which);
                if(dest!=org)
                    continue;
                dest_i = canvas_getindex(cnv, o2g(dest));
                obj_disconnect(obj, nout, dest, which);
                canvas_undo_add(cnv, UNDO_DISCONNECT, "disconnect",
                    canvas_undo_set_disconnect(cnv, obj_i, nout, dest_i, which));
                obj_connect(obj, nout, replace, which);
                canvas_undo_add(cnv, UNDO_CONNECT, "connect",
                    canvas_undo_set_connect(cnv, obj_i, nout, replace_i, which));
            }
        }
    }
}
static void obj_delete_undo(t_glist*x, t_object *obj)
{
        /* delete an object with undo */
    t_gobj*gobj=o2g(obj);
    canvas_undo_add(x, UNDO_RECREATE, "recreate",
        (void *)canvas_undo_set_recreate(x, gobj,canvas_getindex(x, gobj)));
    glist_delete(x, gobj);
}
static t_object*triggerize_createobj(t_glist*x, t_binbuf*b)
{
        /* send a binbuf to a canvas */
    t_pd *boundx = s__X.s_thing, *boundn = s__N.s_thing;
    s__X.s_thing = &x->gl_pd;
    s__N.s_thing = &pd_canvasmaker;

    binbuf_eval(b, 0, 0, 0);

    s__X.s_thing = boundx;
    s__N.s_thing = boundn;
    return g2o(glist_getlast(x));
}
static void stack_conn(t_glist*x, t_object*new, int*newoutlet, t_object*org,
    int orgoutlet, t_outconnect*conn)
{
    t_object*dest=0;
    t_inlet *in =0;
    int which;
    int new_i, org_i, dest_i;
    if(!conn)
        return;
    conn=obj_nexttraverseoutlet(conn, &dest, &in, &which);
    stack_conn(x, new, newoutlet, org, orgoutlet, conn);
    new_i = canvas_getindex(x, o2g(new));
    org_i = canvas_getindex(x, o2g(org));
    dest_i = canvas_getindex(x, o2g(dest));
    obj_disconnect(org, orgoutlet, dest, which);
    canvas_undo_add(x, UNDO_DISCONNECT, "disconnect",
        canvas_undo_set_disconnect(x, org_i, orgoutlet, dest_i, which));
    obj_connect(new, *newoutlet, dest, which);
    canvas_undo_add(x, UNDO_CONNECT, "connect",
        canvas_undo_set_connect(x, new_i, *newoutlet, dest_i, which));
    (*newoutlet)++;
}
static int has_fanout(t_object*obj)
{
        /* check if we actually do have a fan out */
    int obj_nout=obj_noutlets(obj);
    int nout;
    for(nout=0; nout<obj_nout; nout++)
    {
        t_outlet*out=0;
        t_outconnect*conn=obj_starttraverseoutlet(obj, &out, nout);
        int count=0;
        if(obj_issignaloutlet(obj, nout))
            continue;
        while(conn)
        {
            int which;
            t_object*dest=0;
            t_inlet *in =0;
            if(count)return 1;
            conn=obj_nexttraverseoutlet(conn, &dest, &in, &which);
            count++;
        }
    }
    return 0;
}
static int only_triggers_selected(t_glist*cnv)
{
        /* check if all selected objects in <cnv> are triggers */
    const t_symbol*s_trigger=gensym("trigger");
    t_gobj*gobj = NULL;

    for(gobj=cnv->gl_list; gobj; gobj=gobj->g_next)
    {
        t_object*obj=g2o(gobj);
        if(obj && glist_isselected(cnv, gobj)
           && (s_trigger != obj->te_g.g_pd->c_name))
        {
            return 0;
        }
    }
    return 1;
}

/* ------------------------- triggerize ---------------------------- */
static int triggerize_fanout_inplace(t_glist*x, t_object*obj)
{
        /* avoid fanouts in [t] objects by adding additional outlets */

    int posX=obj->te_xpix;
    int posY=obj->te_ypix;
    t_atom*argv=binbuf_getvec(obj->te_binbuf);
    int obj_nout=obj_noutlets(obj);
    int nout, newout;
    t_binbuf*b=0;
    t_object*stub=0;
        /* check if we actually do have a fan out */
    if(!has_fanout(obj))return 0;
        /* create a new trigger object, that has outlets for the fans */
    b=binbuf_new();
    binbuf_addv(b, "ssii", gensym("#X"), gensym("obj"), posX, posY);
    binbuf_add(b, 1, argv);
    argv++;
    for(nout=0; nout<obj_nout; nout++)
    {
        t_outlet*out=0;
        t_outconnect*conn=obj_starttraverseoutlet(obj, &out, nout);
        while(conn)
        {
            int which;
            t_object*dest=0;
            t_inlet *in =0;
            conn=obj_nexttraverseoutlet(conn, &dest, &in, &which);
            binbuf_add(b, 1, argv);
        }
        argv++;
    }
    binbuf_addsemi(b);
    canvas_undo_add(x, UNDO_PASTE, "paste",
        canvas_undo_set_pastebinbuf(x, b, 0, 0, 0));
    stub=triggerize_createobj(x, b);
    binbuf_free(b);

        /* connect */
    newout=0;
    dereconnect(x, obj, stub);
    for(nout=0; nout<obj_nout; nout++)
    {
        t_outlet*out=0;
        t_outconnect*conn=obj_starttraverseoutlet(obj, &out, nout);
        stack_conn(x, stub, &newout, obj, nout, conn);
    }

        /* free old object */
    obj_delete_undo(x, obj);
    return 1;
}
static void triggerize_defanout(t_glist*x, int count, t_outconnect*conn,
    t_object*obj, t_object*trigger, int nout)
{
    t_object *dest=0;
    t_inlet *in=0;
    int which=0;
    int obj_i = canvas_getindex(x, o2g(obj));
    int trigger_i = canvas_getindex(x, o2g(trigger));
    int dest_i;
    if(!conn) return;
    conn = obj_nexttraverseoutlet(conn, &dest, &in, &which);
    triggerize_defanout(x, count-1, conn, obj, trigger, nout);

    dest_i = canvas_getindex(x, o2g(dest));
    obj_disconnect(obj, nout, dest, which);
    canvas_undo_add(x, UNDO_DISCONNECT, "disconnect",
        canvas_undo_set_disconnect(x, obj_i, nout, dest_i, which));
    obj_connect(trigger, count, dest, which);
    canvas_undo_add(x, UNDO_CONNECT, "connect",
        canvas_undo_set_connect(x, trigger_i, count, dest_i, which));
}

static int triggerize_fanout(t_glist*x, t_object*obj)
{
        /* replace all fanouts with [trigger]s
         * if the fanning out object is already a trigger, just expand it */
    const t_symbol*s_trigger=gensym("trigger");
        /* shift trigger object slightly, to make it easier selectable in case of overlaps: */
    const int xoffset = -10;
    const int yoffset = 5;
    int obj_nout=obj_noutlets(obj);
    int nout;
    int posX, posY;
    t_binbuf*b=binbuf_new();
    int didit=0;

    int _x; /* dummy variable */
    gobj_getrect(o2g(obj), x, &_x, &_x, &_x, &posY);
    posY += yoffset * x->gl_zoom;

        /* if the object is a [trigger], we just insert new outlets */
    if(s_trigger == obj->te_g.g_pd->c_name)
    {
        return triggerize_fanout_inplace(x, obj);
    }
        /* for other objects, we create a new [trigger a a] object
         *  and replace the fan-out with that */
    for(nout=0; nout<obj_nout; nout++)
    {
        t_outlet*out=0;
        t_outconnect*conn=obj_starttraverseoutlet(obj, &out, nout);
        int count=0;
        if(obj_issignaloutlet(obj, nout))
            continue;
        while(conn)
        {
            int which;
            t_object*dest=0;
            t_inlet *in =0;
            conn=obj_nexttraverseoutlet(conn, &dest, &in, &which);
            count++;
        }
        if(count>1)
        {
            int i, obj_i, stub_i;
            t_object *stub;
                /* fan out: create a [t] object to resolve it */

                /* need to get the coordinates of the fanning outlet */
            t_linetraverser t;
            linetraverser_start(&t, x);
            while((conn = linetraverser_next(&t)))
            {
                if((obj == t.tr_ob) && nout == t.tr_outno)
                {
                    posX = t.tr_lx1 - (IOMIDDLE - xoffset) * t.tr_x->gl_zoom;
                    break;
                }
            }

            obj_i = canvas_getindex(x, o2g(obj));
            stub=0;
            binbuf_clear(b);
            binbuf_addv(b, "ssiis", gensym("#X"), gensym("obj"), posX, posY, gensym("t"));
            for(i=0; i<count; i++)
            {
                binbuf_addv(b, "s", gensym("a"));
            }
            binbuf_addsemi(b);
            canvas_undo_add(x, UNDO_PASTE, "paste",
                canvas_undo_set_pastebinbuf(x, b, 0, 0, 0));
            stub=triggerize_createobj(x, b);
            stub_i = canvas_getindex(x, o2g(stub));
            conn=obj_starttraverseoutlet(obj, &out, nout);
            triggerize_defanout(x, count-1, conn, obj, stub, nout);
            obj_connect(obj, nout, stub, 0);
            canvas_undo_add(x, UNDO_CONNECT, "connect",
                canvas_undo_set_connect(x, obj_i, nout, stub_i, 0));
            glist_select(x, o2g(stub));
        }
        didit++;
    }
    binbuf_free(b);
    return didit;
}
static int triggerize_fanouts(t_glist*cnv)
{
        /* iterate over all selected objects and try to eliminate fanouts */
    t_gobj*gobj = NULL;
    int count=0;
    canvas_undo_add(cnv, UNDO_SEQUENCE_START, "triggerize", 0);
    for(gobj=cnv->gl_list; gobj; gobj=gobj->g_next)
    {
        t_object*obj=g2o(gobj);
        if(obj && glist_isselected(cnv, gobj) && triggerize_fanout(cnv, obj))
            count++;
    }
    canvas_undo_add(cnv, UNDO_SEQUENCE_END, "triggerize", 0);
    return count;
}

static int triggerize_line(t_glist*x)
{
        /* triggerize a single selected line, by inserting a [t a] object
         * (or it's signal equivalen) */
    t_editor*ed=x->gl_editor;
    int src_obj, src_out, dst_obj, dst_in, new_obj;
    t_gobj *src = 0, *dst = 0;
    t_binbuf*b=0;
    int posx=100, posy=100;
    t_object*stub=0;

    if(!ed->e_selectedline)
        return 0;
    src_obj=ed->e_selectline_index1;
    src_out=ed->e_selectline_outno;
    dst_obj=ed->e_selectline_index2;
    dst_in =ed->e_selectline_inno;
    for (src = x->gl_list; src_obj; src = src->g_next, src_obj--)
        if (!src->g_next) goto bad;
    for (dst = x->gl_list; dst_obj; dst = dst->g_next, dst_obj--)
        if (!dst->g_next) goto bad;
    src_obj=ed->e_selectline_index1;
    dst_obj=ed->e_selectline_index2;

    if(1)
    {
        t_object*obj1=g2o(src);
        t_object*obj2=g2o(dst);
        if(obj1 && obj2)
        {
            posx=(obj1->te_xpix+obj2->te_xpix)>>1;
            posy=(obj1->te_ypix+obj2->te_ypix)>>1;
        }
    }

    canvas_undo_add(x, UNDO_SEQUENCE_START, "{insert object}", 0);
    b=binbuf_new();
    if(obj_issignaloutlet(g2o(src), src_out))
    {
        binbuf_addv(b, "ssiiiisi;", gensym("#N"),
            gensym("canvas"), 200, 100, 190, 200, gensym("nop~"), 0);
        binbuf_addv(b, "ssiis;", gensym("#X"),
            gensym("obj"), 50, 70, gensym("inlet~"));
        binbuf_addv(b, "ssiis;", gensym("#X"),
            gensym("obj"), 50,140, gensym("outlet~"));
        binbuf_addv(b, "ssiiii;", gensym("#X"),
            gensym("connect"), 0,0,1,0);
        binbuf_addv(b, "ssiiss", gensym("#X"),
            gensym("restore"), posx, posy, gensym("pd"), gensym("nop~"));
    } else {
        binbuf_addv(b,"ssii", gensym("#X"), gensym("obj"), posx, posy);
        binbuf_addv(b,"ss", gensym("t"), gensym("a"));
    }
    binbuf_addsemi(b);
    canvas_undo_add(x, UNDO_PASTE, "paste",
        canvas_undo_set_pastebinbuf(x, b, 0, 0, 0));

    stub=triggerize_createobj(x, b);
    new_obj = canvas_getindex(x, &stub->ob_g);
    binbuf_free(b);b=0;

    obj_disconnect(g2o(src), src_out, g2o(dst), dst_in);
    canvas_undo_add(x, UNDO_DISCONNECT, "disconnect",
        canvas_undo_set_disconnect(x, src_obj, src_out, dst_obj, dst_in));

    obj_connect(g2o(src), src_out, stub, 0);
    canvas_undo_add(x, UNDO_CONNECT, "connect",
        canvas_undo_set_connect(x, src_obj, src_out, new_obj, 0));

    obj_connect(stub, 0, g2o(dst), dst_in);
    canvas_undo_add(x, UNDO_CONNECT, "connect",
        canvas_undo_set_connect(x, new_obj, 0, dst_obj, dst_in));

    glist_select(x, o2g(stub));

    canvas_undo_add(x, UNDO_SEQUENCE_END, "{insert object}", 0);
    return 1;
bad:
    return 0;
}
static int minimize_trigger(t_glist*cnv, t_object*obj)
{
        /* remove all unused outlets from [trigger] */
    t_binbuf*b=binbuf_new();
    t_atom*argv=binbuf_getvec(obj->te_binbuf);
    t_object*stub=0;
    int obj_nout=obj_noutlets(obj);
    int nout, stub_i, obj_i = canvas_getindex(cnv, o2g(obj));

    int count = 0;

    binbuf_addv(b, "ssii", gensym("#X"),
        gensym("obj"), obj->te_xpix, obj->te_ypix);
    binbuf_add(b, 1, argv);
        /* go through all the outlets, and add those that have connections */
    for(nout = 0; nout < obj_nout; nout++)
    {
        t_outlet*out=0;
        t_outconnect*conn = obj_starttraverseoutlet(obj, &out, nout);
        if(conn)
        {
            binbuf_add(b, 1, argv + 1 + nout);
        } else {
            count++;
        }
    }
    if(!count || count == obj_nout)
    {
            /* either no outlet to delete or all: skip this object */
        binbuf_free(b);
        return 0;
    }
        /* create the replacement object (which only has outlets that
         * are going to be connected) */
    canvas_undo_add(cnv, UNDO_PASTE, "paste",
        canvas_undo_set_pastebinbuf(cnv, b, 0, 0, 0));
    stub=triggerize_createobj(cnv, b);
    stub_i = canvas_getindex(cnv, o2g(stub));

        /* no go through the original object's outlets, and duplicate the
         * connection of each to the new object */
    for(nout=0, count=0; nout<obj_nout; nout++)
    {
        t_outlet*out=0;
        t_outconnect*conn=obj_starttraverseoutlet(obj, &out, nout);
        if(!conn) /* nothing connected here; skip it */
            continue;

            /* repeat all connections of this outlet (should only be one) */
        while(conn)
        {
            int which, dest_i;
            t_object*dest=0;
            t_inlet *in =0;
            conn=obj_nexttraverseoutlet(conn, &dest, &in, &which);
            dest_i = canvas_getindex(cnv, o2g(dest));
            obj_disconnect(obj, nout, dest, which);
            canvas_undo_add(cnv, UNDO_DISCONNECT, "disconnect",
                canvas_undo_set_disconnect(cnv, obj_i, nout, dest_i, which));
            obj_connect(stub, count, dest, which);
            canvas_undo_add(cnv, UNDO_CONNECT, "connect",
                canvas_undo_set_connect(cnv, stub_i, count, dest_i, which));
        }
        count++;
    }
    binbuf_free(b);
    dereconnect(cnv, obj, stub);
    obj_delete_undo(cnv, obj);
    return 1;
}
static int expand_trigger(t_glist*cnv, t_object*obj)
{
        /* expand the trigger to the left, by inserting a first "a" outlet */
    t_binbuf*b=binbuf_new();
    int argc=binbuf_getnatom(obj->te_binbuf);
    t_atom*argv=binbuf_getvec(obj->te_binbuf);
    t_object*stub=0;
    int obj_nout=obj_noutlets(obj);
    int stub_i, obj_i = canvas_getindex(cnv, o2g(obj));
    int nout;

    binbuf_addv(b, "ssii", gensym("#X"),
        gensym("obj"), obj->te_xpix, obj->te_ypix);
    binbuf_add(b, 1, argv);
    binbuf_addv(b, "s", gensym("a"));
    binbuf_add(b, argc-1, argv+1);
    stub=triggerize_createobj(cnv, b);
    stub_i = canvas_getindex(cnv, o2g(stub));
    for(nout=0; nout<obj_nout; nout++)
    {
        t_outlet*out=0;
        t_outconnect*conn=obj_starttraverseoutlet(obj, &out, nout);
        while(conn)
        {
            int which;
            t_object*dest=0;
            t_inlet *in =0;
            int dest_i;
            conn=obj_nexttraverseoutlet(conn, &dest, &in, &which);
            dest_i = canvas_getindex(cnv, o2g(dest));
            obj_disconnect(obj, nout, dest, which);
            canvas_undo_add(cnv, UNDO_DISCONNECT, "disconnect",
                canvas_undo_set_disconnect(cnv, obj_i, nout, dest_i, which));
            obj_connect(stub, nout+1, dest, which);
            canvas_undo_add(cnv, UNDO_CONNECT, "connect",
                canvas_undo_set_connect(cnv, stub_i, nout+1, dest_i, which));
        }
    }
    binbuf_free(b);
    dereconnect(cnv, obj, stub);
    obj_delete_undo(cnv, obj);
    return 1;
}
typedef int (*t_fun_withobject)(t_glist*, t_object*);
static int with_triggers(t_glist*cnv, t_fun_withobject fun)
{
        /* run <fun> on all selected [trigger] objects */
    const t_symbol*s_trigger=gensym("trigger");
    int count=0;
    t_gobj*gobj = NULL;
    for(gobj=cnv->gl_list; gobj; gobj=gobj->g_next)
    {
        t_object*obj=g2o(gobj);
        if(obj && glist_isselected(cnv, gobj))
        {
            const t_symbol*c_name=obj->te_g.g_pd->c_name;
            if((s_trigger == c_name) && fun(cnv, obj))
                count++;
        }
    }
    return count;
}
static int triggerize_triggers(t_glist*cnv)
{
        /* cleanup [trigger] objects */
    int count=0;

        /* nothing to do, if the selection is not exclusively [trigger] objects */
    if(!only_triggers_selected(cnv))
        return 0;

        /* TODO: here's the place to merge multiple connected triggers */

        /* remove all unused outlets from (selected) triggers */
    canvas_undo_add(cnv, UNDO_SEQUENCE_START, "{minimize triggers}", 0);
    count = with_triggers(cnv, minimize_trigger);
    canvas_undo_add(cnv, UNDO_SEQUENCE_END, "{minimize triggers}", 0);
    if(count)
        return count;

        /* expand each (selected) trigger to the left */
    canvas_undo_add(cnv, UNDO_SEQUENCE_START, "{expand triggers}", 0);
    count = with_triggers(cnv, expand_trigger);
    canvas_undo_add(cnv, UNDO_SEQUENCE_END, "{expand triggers}", 0);
    if(count)
        return count;

        /* nothing more to do */
    return 0;
}

static void canvas_do_triggerize(t_glist*cnv)
{
        /*
         * selected msg-connection: insert [t a] (->triggerize_line)
         * selected sig-connection: insert [pd nop~] (->triggerize_line)
         * selected [trigger]s with fan-outs: remove them (by inserting new outlets of the correct type) (->triggerize_fanouts)
         * selected objects with fan-outs: remove fan-outs (->triggerize_fanouts)
         * selected [trigger]: remove unused outlets (->triggerize_triggers)
         * selected [trigger]: else, add left-most "a" outlet (->triggerize_triggers)
         */

    if(triggerize_line(cnv)
       || triggerize_fanouts(cnv)
       || triggerize_triggers(cnv))
    canvas_dirty(cnv, 1);
}
void canvas_triggerize(t_glist*cnv)
{
    int dspstate;
    if(NULL == cnv)return;

        /* suspend system */
    dspstate = canvas_suspend_dsp();

    canvas_do_triggerize(cnv);

        /* restore state */
    canvas_redraw(cnv);
    glist_redraw(cnv);
    canvas_resume_dsp(dspstate);
}
