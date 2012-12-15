/* choice -- match incoming list against a collection of stored templates. */

/*  Copyright 1999 Miller Puckette.
Permission is granted to use this software for any purpose provided you
keep this copyright notice intact.

THE AUTHOR AND HIS EMPLOYERS MAKE NO WARRANTY, EXPRESS OR IMPLIED,
IN CONNECTION WITH THIS SOFTWARE.
 
This file is downloadable from http://www.crca.ucsd.edu/~msp .
*/

#include "m_pd.h"
#include <math.h>
static t_class *choice_class;
#define DIMENSION 10

typedef struct _elem
{
    t_float e_age;
    t_float e_weight[DIMENSION];
} t_elem;

typedef struct _choice
{
    t_object x_obj;
    t_elem *x_vec;
    int x_n;
    int x_nonrepeat;
} t_choice;

static void *choice_new(t_float fnonrepeat)
{
    t_choice *x = (t_choice *)pd_new(choice_class);
    outlet_new(&x->x_obj, gensym("float"));
    x->x_vec = (t_elem *)getbytes(0);
    x->x_n = 0;
    x->x_nonrepeat = (fnonrepeat != 0);
    return (x);
}

static void choice_clear(t_choice *x)
{
    x->x_vec = (t_elem *)resizebytes(x->x_vec, x->x_n * sizeof(t_elem), 0);
    x->x_n = 0;
}

static void choice_print(t_choice *x)
{
    int j;
    for (j = 0; j < x->x_n; j++)
    {
        t_elem *e = x->x_vec + j;
        t_float *w = e->e_weight;
        post("%2d age %2d \
w %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f",
            j, (int)(e->e_age), w[0], w[1], w[2], w[3], w[4], w[5],
                w[6], w[7], w[8], w[9]);
    }
}

static void choice_add(t_choice *x, t_symbol *s, int argc, t_atom *argv)
{
    int oldn = x->x_n, newn = oldn + 1, i;
    t_elem *e;
    t_float sum, normal;
    x->x_vec = (t_elem *)resizebytes(x->x_vec, oldn * sizeof(t_elem),
        newn * sizeof(t_elem));
    x->x_n = newn;
    e = x->x_vec + oldn;
    e->e_age = 2;
    
    for (i = 0, sum = 0; i < DIMENSION; i++)
    {
        t_float f = atom_getfloatarg(i, argc, argv);
        e->e_weight[i] = f;
        sum += f*f;
    }
    normal = (t_float)(sum > 0 ? 1./sqrt(sum) : 1);
    for (i = 0; i < DIMENSION; i++)
        e->e_weight[i] *= normal;
}

static void choice_list(t_choice *x, t_symbol *s, int argc, t_atom *argv)
{
    int i, j;
    t_float bestsum = 0;
    int bestindex = -1;
    t_float invec[DIMENSION];
    for (i = 0; i < DIMENSION; i++)
        invec[i] = atom_getfloatarg(i, argc, argv);
    for (j = 0; j < x->x_n; j++)
    {
        t_elem *e = x->x_vec + j;
        t_float sum;
        for (i = 0, sum = 0; i < DIMENSION; i++)
            sum += e->e_weight[i] * invec[i];
        if (x->x_nonrepeat) sum *= (t_float)(log(e->e_age));
        if (sum > bestsum)
        {
            bestsum = sum;
            sum = 1;
            bestindex = j;
        }
    }
    if (bestindex >= 0)
    {
        for (j = 0; j < x->x_n; j++)
            x->x_vec[j].e_age += 1.;
        x->x_vec[bestindex].e_age = 1;
    }
    outlet_float(x->x_obj.ob_outlet, (t_float)bestindex);
}

static void choice_free(t_choice *x)
{
    freebytes(x->x_vec, x->x_n * sizeof(t_elem));
}

void choice_setup(void)
{
    choice_class = class_new(gensym("choice"), (t_newmethod)choice_new,
        (t_method)choice_free, sizeof(t_choice), 0, A_DEFFLOAT, 0);
    class_addmethod(choice_class, (t_method)choice_add, gensym("add"), A_GIMME, 0);
    class_addmethod(choice_class, (t_method)choice_clear, gensym("clear"), 0);
    class_addmethod(choice_class, (t_method)choice_print, gensym("print"), 0);
    class_addlist(choice_class, choice_list);
}
