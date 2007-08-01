/* code for the "obj3" pd class.  This adds an outlet and a state variable.  */

#include "m_pd.h"
 
typedef struct obj3
{
  t_object x_ob;
  t_outlet *x_outlet;
  float x_value;
} t_obj3;

void obj3_float(t_obj3 *x, t_floatarg f)
{
    outlet_float(x->x_outlet, f + x->x_value);
}

void obj3_ft1(t_obj3 *x, t_floatarg g)
{
    x->x_value = g;
}

t_class *obj3_class;

void *obj3_new(void)
{
    t_obj3 *x = (t_obj3 *)pd_new(obj3_class);
    inlet_new(&x->x_ob, &x->x_ob.ob_pd, gensym("float"), gensym("ft1"));
    x->x_outlet = outlet_new(&x->x_ob, gensym("float"));
    return (void *)x;
}

void obj3_setup(void)
{
    obj3_class = class_new(gensym("obj3"), (t_newmethod)obj3_new,
    	0, sizeof(t_obj3), 0, 0);
    class_addmethod(obj3_class, (t_method)obj3_ft1, gensym("ft1"), A_FLOAT, 0);
    class_addfloat(obj3_class, obj3_float);
}

