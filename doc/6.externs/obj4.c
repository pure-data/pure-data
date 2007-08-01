/* code for the "obj4" pd class.  This adds a creation argument, of
type "float".  */

#include "m_pd.h"
 
typedef struct obj4
{
  t_object x_ob;
  t_outlet *x_outlet;
  float x_value;
} t_obj4;

void obj4_float(t_obj4 *x, t_floatarg f)
{
    outlet_float(x->x_outlet, x->x_value + f);
}

void obj4_ft1(t_obj4 *x, t_floatarg g)
{
    x->x_value = g;
}

t_class *obj4_class;

    /* as requested by the new invocation of "class_new" below, the new
    routine will be called with a "float" argument. */
void *obj4_new(t_floatarg f)
{
    t_obj4 *x = (t_obj4 *)pd_new(obj4_class);
    inlet_new(&x->x_ob, &x->x_ob.ob_pd, gensym("float"), gensym("ft1"));
    x->x_outlet = outlet_new(&x->x_ob, gensym("float"));
    	/* just stick the argument in the object structure for later. */
    x->x_value = f;
    return (void *)x;
}

void obj4_setup(void)
{
    	/* here we add "A_DEFFLOAT" to the (zero-terminated) list of arg
	types we declare for a new object.  The value will be filled
	in as 0 if not given in the object box.  */
    obj4_class = class_new(gensym("obj4"), (t_newmethod)obj4_new,
    	0, sizeof(t_obj4), 0, A_DEFFLOAT, 0);
    class_addmethod(obj4_class, (t_method)obj4_ft1, gensym("ft1"), A_FLOAT, 0);
    class_addfloat(obj4_class, obj4_float);
}

