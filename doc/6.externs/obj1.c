/* code for "obj1" pd class.  This takes two messages: floating-point
numbers, and "rats", and just prints something out for each message. */

#include "m_pd.h"

    /* the data structure for each copy of "obj1".  In this case we
    on;y need pd's obligatory header (of type t_object). */
typedef struct obj1
{
  t_object x_ob;
} t_obj1;

    /* this is called back when obj1 gets a "float" message (i.e., a
    number.) */
void obj1_float(t_obj1 *x, t_floatarg f)
{
    post("obj1: %f", f);
}

    /* this is called when obj1 gets the message, "rats". */
void obj1_rats(t_obj1 *x)
{
    post("obj1: rats");
}

    /* this is a pointer to the class for "obj1", which is created in the
    "setup" routine below and used to create new ones in the "new" routine. */
t_class *obj1_class;

    /* this is called when a new "obj1" object is created. */
void *obj1_new(void)
{
    t_obj1 *x = (t_obj1 *)pd_new(obj1_class);
    post("obj1_new");
    return (void *)x;
}

    /* this is called once at setup time, when this code is loaded into Pd. */
void obj1_setup(void)
{
    post("obj1_setup");
    obj1_class = class_new(gensym("obj1"), (t_newmethod)obj1_new, 0,
    	sizeof(t_obj1), 0, 0);
    class_addmethod(obj1_class, (t_method)obj1_rats, gensym("rats"), 0);
    class_addfloat(obj1_class, obj1_float);
}

