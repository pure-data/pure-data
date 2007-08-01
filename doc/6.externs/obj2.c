/* code for the "obj2" pd class.  This one, in addition to the "obj1"
code, has an inlet taking numbers.  */

#include "m_pd.h"
 
typedef struct obj2
{
  t_object x_ob;
} t_obj2;

void obj2_float(t_obj2 *x, t_floatarg f)
{
    post("obj2: %f", f);
}

void obj2_rats(t_obj2 *x)
{
    post("obj2: rats");
}

void obj2_ft1(t_obj2 *x, t_floatarg g)
{
    post("ft1: %f", g);
}

t_class *obj2_class;

void *obj2_new(void)
{
    t_obj2 *x = (t_obj2 *)pd_new(obj2_class);
    inlet_new(&x->x_ob, &x->x_ob.ob_pd, gensym("float"), gensym("ft1"));
    post("obj2_new");
    return (void *)x;
}

void obj2_setup(void)
{
    post("obj2_setup");
    obj2_class = class_new(gensym("obj2"), (t_newmethod)obj2_new,
    	0, sizeof(t_obj2), 0, 0);
    class_addmethod(obj2_class, (t_method)obj2_rats, gensym("rats"), 0);
    class_addmethod(obj2_class, (t_method)obj2_ft1, gensym("ft1"), A_FLOAT, 0);
    class_addfloat(obj2_class, obj2_float);
}

