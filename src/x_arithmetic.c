/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* arithmetic: binops ala C language.  The 4 functions and relationals are
done on floats; the logical and bitwise binops convert their
inputs to int and their outputs back to float. */

#include "m_pd.h"
#include <math.h>


typedef struct _binop
{
    t_object x_obj;
    t_float x_f1;
    t_float x_f2;
} t_binop;

/* ------------------ binop1:  +, -, *, / ----------------------------- */

static void *binop1_new(t_class *floatclass, t_floatarg f)
{
    t_binop *x = (t_binop *)pd_new(floatclass);
    outlet_new(&x->x_obj, &s_float);
    floatinlet_new(&x->x_obj, &x->x_f2);
    x->x_f1 = 0;
    x->x_f2 = f;
    return (x);
}

/* --------------------- addition ------------------------------- */

static t_class *binop1_plus_class;

static void *binop1_plus_new(t_floatarg f)
{
    return (binop1_new(binop1_plus_class, f));
}

static void binop1_plus_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f1 + x->x_f2);
}

static void binop1_plus_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, (x->x_f1 = f) + x->x_f2);
}

/* --------------------- subtraction ------------------------------- */

static t_class *binop1_minus_class;

static void *binop1_minus_new(t_floatarg f)
{
    return (binop1_new(binop1_minus_class, f));
}

static void binop1_minus_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f1 - x->x_f2);
}

static void binop1_minus_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, (x->x_f1 = f) - x->x_f2);
}

/* --------------------- multiplication ------------------------------- */

static t_class *binop1_times_class;

static void *binop1_times_new(t_floatarg f)
{
    return (binop1_new(binop1_times_class, f));
}

static void binop1_times_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f1 * x->x_f2);
}

static void binop1_times_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, (x->x_f1 = f) * x->x_f2);
}

/* --------------------- division ------------------------------- */

static t_class *binop1_div_class;

static void *binop1_div_new(t_floatarg f)
{
    return (binop1_new(binop1_div_class, f));
}

static void binop1_div_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet,
        (x->x_f2 != 0 ? x->x_f1 / x->x_f2 : 0));
}

static void binop1_div_float(t_binop *x, t_float f)
{
    x->x_f1 = f;
    outlet_float(x->x_obj.ob_outlet,
        (x->x_f2 != 0 ? x->x_f1 / x->x_f2 : 0));
}

/* ------------------------ pow -------------------------------- */

static t_class *binop1_pow_class;

static void *binop1_pow_new(t_floatarg f)
{
    return (binop1_new(binop1_pow_class, f));
}

static void binop1_pow_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet,
        (x->x_f1 > 0 ? powf(x->x_f1, x->x_f2) : 0));
}

static void binop1_pow_float(t_binop *x, t_float f)
{
    x->x_f1 = f;
    outlet_float(x->x_obj.ob_outlet,
        (x->x_f1 > 0 ? powf(x->x_f1, x->x_f2) : 0));
}

/* ------------------------ max -------------------------------- */

static t_class *binop1_max_class;

static void *binop1_max_new(t_floatarg f)
{
    return (binop1_new(binop1_max_class, f));
}

static void binop1_max_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet,
        (x->x_f1 > x->x_f2 ? x->x_f1 : x->x_f2));
}

static void binop1_max_float(t_binop *x, t_float f)
{
    x->x_f1 = f;
    outlet_float(x->x_obj.ob_outlet,
        (x->x_f1 > x->x_f2 ? x->x_f1 : x->x_f2));
}

/* ------------------------ min -------------------------------- */

static t_class *binop1_min_class;

static void *binop1_min_new(t_floatarg f)
{
    return (binop1_new(binop1_min_class, f));
}

static void binop1_min_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet,
        (x->x_f1 < x->x_f2 ? x->x_f1 : x->x_f2));
}

static void binop1_min_float(t_binop *x, t_float f)
{
    x->x_f1 = f;
    outlet_float(x->x_obj.ob_outlet,
        (x->x_f1 < x->x_f2 ? x->x_f1 : x->x_f2));
}

/* ------------------ binop2: ==, !=, >, <, >=, <=. -------------------- */

static void *binop2_new(t_class *floatclass, t_floatarg f)
{
    t_binop *x = (t_binop *)pd_new(floatclass);
    outlet_new(&x->x_obj, &s_float);
    floatinlet_new(&x->x_obj, &x->x_f2);
    x->x_f1 = 0;
    x->x_f2 = f;
    return (x);
}

/* --------------------- == ------------------------------- */

static t_class *binop2_ee_class;

static void *binop2_ee_new(t_floatarg f)
{
    return (binop2_new(binop2_ee_class, f));
}

static void binop2_ee_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f1 == x->x_f2);
}

static void binop2_ee_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, (x->x_f1 = f) == x->x_f2);
}

/* --------------------- != ------------------------------- */

static t_class *binop2_ne_class;

static void *binop2_ne_new(t_floatarg f)
{
    return (binop2_new(binop2_ne_class, f));
}

static void binop2_ne_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f1 != x->x_f2);
}

static void binop2_ne_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, (x->x_f1 = f) != x->x_f2);
}

/* --------------------- > ------------------------------- */

static t_class *binop2_gt_class;

static void *binop2_gt_new(t_floatarg f)
{
    return (binop2_new(binop2_gt_class, f));
}

static void binop2_gt_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f1 > x->x_f2);
}

static void binop2_gt_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, (x->x_f1 = f) > x->x_f2);
}

/* --------------------- < ------------------------------- */

static t_class *binop2_lt_class;

static void *binop2_lt_new(t_floatarg f)
{
    return (binop2_new(binop2_lt_class, f));
}

static void binop2_lt_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f1 < x->x_f2);
}

static void binop2_lt_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, (x->x_f1 = f) < x->x_f2);
}

/* --------------------- >= ------------------------------- */

static t_class *binop2_ge_class;

static void *binop2_ge_new(t_floatarg f)
{
    return (binop2_new(binop2_ge_class, f));
}

static void binop2_ge_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f1 >= x->x_f2);
}

static void binop2_ge_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, (x->x_f1 = f) >= x->x_f2);
}

/* --------------------- <= ------------------------------- */

static t_class *binop2_le_class;

static void *binop2_le_new(t_floatarg f)
{
    return (binop2_new(binop2_le_class, f));
}

static void binop2_le_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f1 <= x->x_f2);
}

static void binop2_le_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, (x->x_f1 = f) <= x->x_f2);
}

/* ------------- binop3: &, |, &&, ||, <<, >>, %, mod, div ------------------ */

static void *binop3_new(t_class *fixclass, t_floatarg f)
{
    t_binop *x = (t_binop *)pd_new(fixclass);
    outlet_new(&x->x_obj, &s_float);
    floatinlet_new(&x->x_obj, &x->x_f2);
    x->x_f1 = 0;
    x->x_f2 = f;
    return (x);
}

/* --------------------------- & ---------------------------- */

static t_class *binop3_ba_class;

static void *binop3_ba_new(t_floatarg f)
{
    return (binop3_new(binop3_ba_class, f));
}

static void binop2_ba_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1)) & (int)(x->x_f2));
}

static void binop2_ba_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1 = f)) & (int)(x->x_f2));
}

/* --------------------------- && ---------------------------- */

static t_class *binop3_la_class;

static void *binop3_la_new(t_floatarg f)
{
    return (binop3_new(binop3_la_class, f));
}

static void binop2_la_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1)) && (int)(x->x_f2));
}

static void binop2_la_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1 = f)) && (int)(x->x_f2));
}

/* --------------------------- | ---------------------------- */

static t_class *binop3_bo_class;

static void *binop3_bo_new(t_floatarg f)
{
    return (binop3_new(binop3_bo_class, f));
}

static void binop2_bo_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1)) | (int)(x->x_f2));
}

static void binop2_bo_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1 = f)) | (int)(x->x_f2));
}

/* --------------------------- || ---------------------------- */

static t_class *binop3_lo_class;

static void *binop3_lo_new(t_floatarg f)
{
    return (binop3_new(binop3_lo_class, f));
}

static void binop2_lo_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1)) || (int)(x->x_f2));
}

static void binop2_lo_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1 = f)) || (int)(x->x_f2));
}

/* --------------------------- << ---------------------------- */

static t_class *binop3_ls_class;

static void *binop3_ls_new(t_floatarg f)
{
    return (binop3_new(binop3_ls_class, f));
}

static void binop2_ls_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1)) << (int)(x->x_f2));
}

static void binop2_ls_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1 = f)) << (int)(x->x_f2));
}

/* --------------------------- >> ---------------------------- */

static t_class *binop3_rs_class;

static void *binop3_rs_new(t_floatarg f)
{
    return (binop3_new(binop3_rs_class, f));
}

static void binop2_rs_bang(t_binop *x)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1)) >> (int)(x->x_f2));
}

static void binop2_rs_float(t_binop *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1 = f)) >> (int)(x->x_f2));
}

/* --------------------------- % ---------------------------- */

static t_class *binop3_pc_class;

static void *binop3_pc_new(t_floatarg f)
{
    return (binop3_new(binop3_pc_class, f));
}

static void binop2_pc_bang(t_binop *x)
{
    int n2 = x->x_f2;
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1)) % (n2 ? n2 : 1));
}

static void binop2_pc_float(t_binop *x, t_float f)
{
    int n2 = x->x_f2;
    outlet_float(x->x_obj.ob_outlet, ((int)(x->x_f1 = f)) % (n2 ? n2 : 1));
}

/* --------------------------- mod ---------------------------- */

static t_class *binop3_mod_class;

static void *binop3_mod_new(t_floatarg f)
{
    return (binop3_new(binop3_mod_class, f));
}

static void binop3_mod_bang(t_binop *x)
{
    int n2 = x->x_f2, result;
    if (n2 < 0) n2 = -n2;
    else if (!n2) n2 = 1;
    result = ((int)(x->x_f1)) % n2;
    if (result < 0) result += n2;
    outlet_float(x->x_obj.ob_outlet, (t_float)result);
}

static void binop3_mod_float(t_binop *x, t_float f)
{
    x->x_f1 = f;
    binop3_mod_bang(x);
}

/* --------------------------- div ---------------------------- */

static t_class *binop3_div_class;

static void *binop3_div_new(t_floatarg f)
{
    return (binop3_new(binop3_div_class, f));
}

static void binop3_div_bang(t_binop *x)
{
    int n1 = x->x_f1, n2 = x->x_f2, result;
    if (n2 < 0) n2 = -n2;
    else if (!n2) n2 = 1;
    if (n1 < 0) n1 -= (n2-1);
    result = n1 / n2;
    outlet_float(x->x_obj.ob_outlet, (t_float)result);
}

static void binop3_div_float(t_binop *x, t_float f)
{
    x->x_f1 = f;
    binop3_div_bang(x);
}

/* -------------------- mathematical functions ------------------ */

static t_class *sin_class;      /* ----------- sin --------------- */

static void *sin_new(void)
{
    t_object *x = (t_object *)pd_new(sin_class);
    outlet_new(x, &s_float);
    return (x);
}

static void sin_float(t_object *x, t_float f)
{
    outlet_float(x->ob_outlet, sinf(f));
}

static t_class *cos_class;      /* ----------- cos --------------- */

static void *cos_new(void)
{
    t_object *x = (t_object *)pd_new(cos_class);
    outlet_new(x, &s_float);
    return (x);
}

static void cos_float(t_object *x, t_float f)
{
    outlet_float(x->ob_outlet, cosf(f));
}

static t_class *tan_class;      /* ----------- tan --------------- */

static void *tan_new(void)
{
    t_object *x = (t_object *)pd_new(tan_class);
    outlet_new(x, &s_float);
    return (x);
}

static void tan_float(t_object *x, t_float f)
{
    t_float c = cosf(f);
    t_float t = (c == 0 ? 0 : sinf(f)/c);
    outlet_float(x->ob_outlet, t);
}

static t_class *atan_class;     /* ----------- atan --------------- */

static void *atan_new(void)
{
    t_object *x = (t_object *)pd_new(atan_class);
    outlet_new(x, &s_float);
    return (x);
}

static void atan_float(t_object *x, t_float f)
{
    outlet_float(x->ob_outlet, atanf(f));
}

static t_class *atan2_class;    /* ----------- atan2 --------------- */

typedef struct _atan2
{
    t_object x_ob;
    t_float x_f;
} t_atan2;

static void *atan2_new(void)
{
    t_atan2 *x = (t_atan2 *)pd_new(atan2_class);
    floatinlet_new(&x->x_ob, &x->x_f);
    x->x_f = 0;
    outlet_new(&x->x_ob, &s_float);
    return (x);
}

static void atan2_float(t_atan2 *x, t_float f)
{
    t_float r = (f == 0 && x->x_f == 0 ? 0 : atan2f(f, x->x_f));
    outlet_float(x->x_ob.ob_outlet, r);
}

static t_class *sqrt_class;     /* ----------- sqrt --------------- */

static void *sqrt_new(void)
{
    t_object *x = (t_object *)pd_new(sqrt_class);
    outlet_new(x, &s_float);
    return (x);
}

static void sqrt_float(t_object *x, t_float f)
{
    t_float r = (f > 0 ? sqrtf(f) : 0);
    outlet_float(x->ob_outlet, r);
}

static t_class *log_class;      /* ----------- log --------------- */

static void *log_new(void)
{
    t_object *x = (t_object *)pd_new(log_class);
    outlet_new(x, &s_float);
    return (x);
}

static void log_float(t_object *x, t_float f)
{
    t_float r = (f > 0 ? logf(f) : -1000);
    outlet_float(x->ob_outlet, r);
}

static t_class *exp_class;      /* ----------- exp --------------- */

static void *exp_new(void)
{
    t_object *x = (t_object *)pd_new(exp_class);
    outlet_new(x, &s_float);
    return (x);
}

#define MAXLOG 87.3365
static void exp_float(t_object *x, t_float f)
{
    t_float g;
#ifdef MSW
    char buf[10];
#endif
    if (f > MAXLOG) f = MAXLOG;
    g = expf(f);
    outlet_float(x->ob_outlet, g);
}

static t_class *abs_class;      /* ----------- abs --------------- */

static void *abs_new(void)
{
    t_object *x = (t_object *)pd_new(abs_class);
    outlet_new(x, &s_float);
    return (x);
}

static void abs_float(t_object *x, t_float f)
{
    outlet_float(x->ob_outlet, fabsf(f));
}

static t_class *wrap_class;      /* ----------- wrap --------------- */

static void *wrap_new(void)
{
    t_object *x = (t_object *)pd_new(wrap_class);
    outlet_new(x, &s_float);
    return (x);
}

static void wrap_float(t_object *x, t_float f)
{
    outlet_float(x->ob_outlet, f - floor(f));
}

/* ------------------------  misc ------------------------ */

static t_class *clip_class;

typedef struct _clip
{
    t_object x_ob;
    t_float x_f1;
    t_float x_f2;
    t_float x_f3;
} t_clip;

static void *clip_new(t_floatarg f1, t_floatarg f2)
{
    t_clip *x = (t_clip *)pd_new(clip_class);
    floatinlet_new(&x->x_ob, &x->x_f2);
    floatinlet_new(&x->x_ob, &x->x_f3);
    outlet_new(&x->x_ob, &s_float);
    x->x_f2 = f1;
    x->x_f3 = f2;
    return (x);
}

static void clip_bang(t_clip *x)
{
        outlet_float(x->x_ob.ob_outlet, (x->x_f1 < x->x_f2 ? x->x_f2 : (
        x->x_f1 > x->x_f3 ? x->x_f3 : x->x_f1)));
}

static void clip_float(t_clip *x, t_float f)
{
        x->x_f1 = f;
        outlet_float(x->x_ob.ob_outlet, (x->x_f1 < x->x_f2 ? x->x_f2 : (
        x->x_f1 > x->x_f3 ? x->x_f3 : x->x_f1)));
}

static void clip_setup(void)
{
    clip_class = class_new(gensym("clip"), (t_newmethod)clip_new, 0,
        sizeof(t_clip), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addfloat(clip_class, clip_float);
    class_addbang(clip_class, clip_bang);
}

void x_arithmetic_setup(void)
{
    t_symbol *binop1_sym = gensym("operators");
    t_symbol *binop23_sym = gensym("otherbinops");
    t_symbol *math_sym = gensym("math");

    binop1_plus_class = class_new(gensym("+"), (t_newmethod)binop1_plus_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop1_plus_class, binop1_plus_bang);
    class_addfloat(binop1_plus_class, (t_method)binop1_plus_float);
    class_sethelpsymbol(binop1_plus_class, binop1_sym);
    
    binop1_minus_class = class_new(gensym("-"),
        (t_newmethod)binop1_minus_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop1_minus_class, binop1_minus_bang);
    class_addfloat(binop1_minus_class, (t_method)binop1_minus_float);
    class_sethelpsymbol(binop1_minus_class, binop1_sym);

    binop1_times_class = class_new(gensym("*"),
        (t_newmethod)binop1_times_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop1_times_class, binop1_times_bang);
    class_addfloat(binop1_times_class, (t_method)binop1_times_float);
    class_sethelpsymbol(binop1_times_class, binop1_sym);

    binop1_div_class = class_new(gensym("/"),
        (t_newmethod)binop1_div_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop1_div_class, binop1_div_bang);
    class_addfloat(binop1_div_class, (t_method)binop1_div_float);
    class_sethelpsymbol(binop1_div_class, binop1_sym);

    binop1_pow_class = class_new(gensym("pow"),
        (t_newmethod)binop1_pow_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop1_pow_class, binop1_pow_bang);
    class_addfloat(binop1_pow_class, (t_method)binop1_pow_float);
    class_sethelpsymbol(binop1_pow_class, binop1_sym);

    binop1_max_class = class_new(gensym("max"),
        (t_newmethod)binop1_max_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop1_max_class, binop1_max_bang);
    class_addfloat(binop1_max_class, (t_method)binop1_max_float);
    class_sethelpsymbol(binop1_max_class, binop1_sym);

    binop1_min_class = class_new(gensym("min"),
        (t_newmethod)binop1_min_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop1_min_class, binop1_min_bang);
    class_addfloat(binop1_min_class, (t_method)binop1_min_float);
    class_sethelpsymbol(binop1_min_class, binop1_sym);

        /* ------------------ binop2 ----------------------- */

    binop2_ee_class = class_new(gensym("=="), (t_newmethod)binop2_ee_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop2_ee_class, binop2_ee_bang);
    class_addfloat(binop2_ee_class, (t_method)binop2_ee_float);
    class_sethelpsymbol(binop2_ee_class, binop23_sym);

    binop2_ne_class = class_new(gensym("!="), (t_newmethod)binop2_ne_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop2_ne_class, binop2_ne_bang);
    class_addfloat(binop2_ne_class, (t_method)binop2_ne_float);
    class_sethelpsymbol(binop2_ne_class, binop23_sym);

    binop2_gt_class = class_new(gensym(">"), (t_newmethod)binop2_gt_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop2_gt_class, binop2_gt_bang);
    class_addfloat(binop2_gt_class, (t_method)binop2_gt_float);
    class_sethelpsymbol(binop2_gt_class, binop23_sym);

    binop2_lt_class = class_new(gensym("<"), (t_newmethod)binop2_lt_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop2_lt_class, binop2_lt_bang);
    class_addfloat(binop2_lt_class, (t_method)binop2_lt_float);
    class_sethelpsymbol(binop2_lt_class, binop23_sym);

    binop2_ge_class = class_new(gensym(">="), (t_newmethod)binop2_ge_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop2_ge_class, binop2_ge_bang);
    class_addfloat(binop2_ge_class, (t_method)binop2_ge_float);
    class_sethelpsymbol(binop2_ge_class, binop23_sym);

    binop2_le_class = class_new(gensym("<="), (t_newmethod)binop2_le_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop2_le_class, binop2_le_bang);
    class_addfloat(binop2_le_class, (t_method)binop2_le_float);
    class_sethelpsymbol(binop2_le_class, binop23_sym);

        /* ------------------ binop3 ----------------------- */

    binop3_ba_class = class_new(gensym("&"), (t_newmethod)binop3_ba_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop3_ba_class, binop2_ba_bang);
    class_addfloat(binop3_ba_class, (t_method)binop2_ba_float);
    class_sethelpsymbol(binop3_ba_class, binop23_sym);

    binop3_la_class = class_new(gensym("&&"), (t_newmethod)binop3_la_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop3_la_class, binop2_la_bang);
    class_addfloat(binop3_la_class, (t_method)binop2_la_float);
    class_sethelpsymbol(binop3_la_class, binop23_sym);

    binop3_bo_class = class_new(gensym("|"), (t_newmethod)binop3_bo_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop3_bo_class, binop2_bo_bang);
    class_addfloat(binop3_bo_class, (t_method)binop2_bo_float);
    class_sethelpsymbol(binop3_bo_class, binop23_sym);

    binop3_lo_class = class_new(gensym("||"), (t_newmethod)binop3_lo_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop3_lo_class, binop2_lo_bang);
    class_addfloat(binop3_lo_class, (t_method)binop2_lo_float);
    class_sethelpsymbol(binop3_lo_class, binop23_sym);

    binop3_ls_class = class_new(gensym("<<"), (t_newmethod)binop3_ls_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop3_ls_class, binop2_ls_bang);
    class_addfloat(binop3_ls_class, (t_method)binop2_ls_float);
    class_sethelpsymbol(binop3_ls_class, binop23_sym);

    binop3_rs_class = class_new(gensym(">>"), (t_newmethod)binop3_rs_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop3_rs_class, binop2_rs_bang);
    class_addfloat(binop3_rs_class, (t_method)binop2_rs_float);
    class_sethelpsymbol(binop3_rs_class, binop23_sym);

    binop3_pc_class = class_new(gensym("%"), (t_newmethod)binop3_pc_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop3_pc_class, binop2_pc_bang);
    class_addfloat(binop3_pc_class, (t_method)binop2_pc_float);
    class_sethelpsymbol(binop3_pc_class, binop23_sym);

    binop3_mod_class = class_new(gensym("mod"), (t_newmethod)binop3_mod_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop3_mod_class, binop3_mod_bang);
    class_addfloat(binop3_mod_class, (t_method)binop3_mod_float);
    class_sethelpsymbol(binop3_mod_class, binop23_sym);

    binop3_div_class = class_new(gensym("div"), (t_newmethod)binop3_div_new, 0,
        sizeof(t_binop), 0, A_DEFFLOAT, 0);
    class_addbang(binop3_div_class, binop3_div_bang);
    class_addfloat(binop3_div_class, (t_method)binop3_div_float);
    class_sethelpsymbol(binop3_div_class, binop23_sym);

        /* ------------------- math functions --------------- */
    
    sin_class = class_new(gensym("sin"), sin_new, 0,
        sizeof(t_object), 0, 0);
    class_addfloat(sin_class, (t_method)sin_float);
    class_sethelpsymbol(sin_class, math_sym);
    
    cos_class = class_new(gensym("cos"), cos_new, 0,
        sizeof(t_object), 0, 0);
    class_addfloat(cos_class, (t_method)cos_float);
    class_sethelpsymbol(cos_class, math_sym);
    
    tan_class = class_new(gensym("tan"), tan_new, 0,
        sizeof(t_object), 0, 0);
    class_addfloat(tan_class, (t_method)tan_float);
    class_sethelpsymbol(tan_class, math_sym);

    atan_class = class_new(gensym("atan"), atan_new, 0,
        sizeof(t_object), 0, 0);
    class_addfloat(atan_class, (t_method)atan_float);
    class_sethelpsymbol(atan_class, math_sym);

    atan2_class = class_new(gensym("atan2"), atan2_new, 0,
        sizeof(t_atan2), 0, 0);
    class_addfloat(atan2_class, (t_method)atan2_float);    
    class_sethelpsymbol(atan2_class, math_sym);

    sqrt_class = class_new(gensym("sqrt"), sqrt_new, 0,
        sizeof(t_object), 0, 0);
    class_addfloat(sqrt_class, (t_method)sqrt_float);
    class_sethelpsymbol(sqrt_class, math_sym);

    log_class = class_new(gensym("log"), log_new, 0,
        sizeof(t_object), 0, 0);
    class_addfloat(log_class, (t_method)log_float);    
    class_sethelpsymbol(log_class, math_sym);

    exp_class = class_new(gensym("exp"), exp_new, 0,
        sizeof(t_object), 0, 0);
    class_addfloat(exp_class, (t_method)exp_float);
    class_sethelpsymbol(exp_class, math_sym);

    abs_class = class_new(gensym("abs"), abs_new, 0,
        sizeof(t_object), 0, 0);
    class_addfloat(abs_class, (t_method)abs_float);    
    class_sethelpsymbol(abs_class, math_sym);

    wrap_class = class_new(gensym("wrap"), wrap_new, 0,
        sizeof(t_object), 0, 0);
    class_addfloat(wrap_class, (t_method)wrap_float);    
    class_sethelpsymbol(wrap_class, math_sym);

/* ------------------------  misc ------------------------ */

    clip_setup();
}


