/* code for the "obj5" pd class.  This shows "gimme" arguments, which have
variable arguments parsed by the routines (both "new" and "rats".) */

#include "m_pd.h"
 
typedef struct obj5
{
  t_object x_ob;
} t_obj5;

    /* the "rats" method is called with the selector (just "rats" again)
    and an array of the typed areguments, which are each either a number
    or a symbol.  We just print them out. */
void obj5_rats(t_obj5 *x, t_symbol *selector, int argcount, t_atom *argvec)
{
    int i;
    post("rats: selector %s", selector->s_name);
    for (i = 0; i < argcount; i++)
    {
    	if (argvec[i].a_type == A_FLOAT)
	    post("float: %f", argvec[i].a_w.w_float);
    	else if (argvec[i].a_type == A_SYMBOL)
	    post("symbol: %s", argvec[i].a_w.w_symbol->s_name);
    }
}

t_class *obj5_class;

    /* same for the "new" (creation) routine, except that we don't have
    "x" as an argument since we have to create "x" in this routine. */
void *obj5_new(t_symbol *selector, int argcount, t_atom *argvec)
{
    t_obj5 *x = (t_obj5 *)pd_new(obj5_class);
    int i;
    post("new: selector %s", selector->s_name);
    for (i = 0; i < argcount; i++)
    {
    	if (argvec[i].a_type == A_FLOAT)
	    post("float: %f", argvec[i].a_w.w_float);
    	else if (argvec[i].a_type == A_SYMBOL)
	    post("symbol: %s", argvec[i].a_w.w_symbol->s_name);
    }
    return (void *)x;
}

void obj5_setup(void)
{
    	/* We specify "A_GIMME" as creation argument for both the creation
	routine and the method (callback) for the "rats" message.  */
    obj5_class = class_new(gensym("obj5"), (t_newmethod)obj5_new,
    	0, sizeof(t_obj5), 0, A_GIMME, 0);
    class_addmethod(obj5_class, (t_method)obj5_rats, gensym("rats"), A_GIMME, 0);
}

