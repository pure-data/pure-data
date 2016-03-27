/* Copyright (c) IRCAM.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* "expr" was written by Shahrokh Yadegari c. 1989. -msp
 *
 * Nov. 2001 --sdy
 *      conversion for expr~
 *
 * Jan, 2002 --sdy
 *      added fmod()
 *
 * May  2002
 *      added floor and ceil for expr -- Orm Finnendahl
 *
 * July 2002 --sdy
 *      added the following math funtions:
 *              cbrt - cube root
 *              erf - error function
 *              erfc - complementary error function
 *              expm1 - exponential minus 1,
 *              log1p - logarithm of 1 plus
 *              isinf - is the value infinite,
 *              finite - is the value finite
 *              isnan -- is the resut a nan (Not a number)
 *              copysign - copy sign of a number
 *              ldexp  -  multiply floating-point number by integral power of 2
 *              imodf - get signed integral value from floating-point number
 *              modf - get signed fractional value from floating-point number
 *              drem - floating-point remainder function
 *
 *      The following are done but not popular enough in math libss
 *      to be included yet
 *              hypoth - Euclidean distance function
 *              trunc
 *              round
 *              nearbyint -
 *  November 2015
 *                              - drem() is now obsolete but it is kept here so that other patches do not break
 *                              - added remainder() - floating-point remainder function
 *                              - fixed the bug that unary operators could be used as
 *                                binary ones (10 ~ 1)
 *                              - fixed ceil() and floor() which should have only one argument
 *                              - added copysign  (the previous one "copysig" which was
 *                                defined with one argument was kept for compatibilty)
 *                              - fixed sum("table"), and Sum("table", x, y)
 *                              - deleted avg(), Avg() as they can be simple expressions
 *                              - deleted store as this can be achieved by the '=' operator
 */



/*
 * vexp_func.c -- this file include all the functions for vexp.
 *                the first two arguments to the function are the number
 *                of argument and an array of arguments (argc, argv)
 *                the last argument is a pointer to a struct ex_ex for
 *                the result.  Up do this point, the content of the
 *                struct ex_ex that these functions receive are either
 *                ET_INT (long), ET_FLT (t_float), or ET_SYM (char **, it is
 *                char ** and not char * since NewHandle of Mac returns
 *                a char ** for relocatability.)  The common practice in
 *                these functions is that they figure out the type of their
 *                result according to the type of the arguments. In general
 *                the ET_SYM is used an ET_INT when we expect a value.
 *                It is the users responsibility not to pass strings to the
 *                function.
 */

#include <stdlib.h>
#include <string.h>

#define __STRICT_BSD__
#include <math.h>
#undef __STRICT_BSD__


#include "x_vexp.h"

/* forward declarations */

static void ex_min(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_max(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_toint(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_rint(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_tofloat(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_pow(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_exp(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_log(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_ln(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_sin(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_cos(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_asin(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_acos(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_tan(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_atan(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_sinh(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_cosh(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_asinh(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_acosh(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_tanh(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_atanh(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_atan2(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_sqrt(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_fact(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_random(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_abs(t_expr *expr, long int argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_fmod(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_ceil(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_floor(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_if(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_ldexp(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_imodf(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_modf(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
#ifndef _WIN32
static void ex_cbrt(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_erf(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_erfc(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_expm1(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_log1p(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_isinf(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_finite(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_isnan(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_copysign(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_drem(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_remainder(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_round(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_trunc(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
static void ex_nearbyint(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
#endif
#ifdef notdef
/* the following will be added once they are more popular in math libraries */
static void ex_hypoth(t_expr *expr, long argc, struct ex_ex *argv, struct ex_ex *optr);
#endif


t_ex_func ex_funcs[] = {
        {"min",         ex_min,         2},
        {"max",         ex_max,         2},
        {"int",         ex_toint,       1},
        {"rint",        ex_rint,        1},
        {"float",       ex_tofloat,     1},
        {"fmod",        ex_fmod,        2},
        {"floor",       ex_floor,       1},
        {"ceil",        ex_ceil,        1},
        {"pow",         ex_pow,         2},
        {"sqrt",        ex_sqrt,        1},
        {"exp",         ex_exp,         1},
        {"log10",       ex_log,         1},
        {"ln",          ex_ln,          1},
        {"log",         ex_ln,          1},
        {"sin",         ex_sin,         1},
        {"cos",         ex_cos,         1},
        {"tan",         ex_tan,         1},
        {"asin",        ex_asin,        1},
        {"acos",        ex_acos,        1},
        {"atan",        ex_atan,        1},
        {"atan2",       ex_atan2,       2},
        {"sinh",        ex_sinh,        1},
        {"cosh",        ex_cosh,        1},
        {"tanh",        ex_tanh,        1},
        {"fact",        ex_fact,        1},
        {"random",      ex_random,      2},     /* random number */
        {"abs",         ex_abs,         1},
        {"if",          ex_if,          3},
        {"ldexp",       ex_ldexp,       2},
        {"imodf",       ex_imodf,       1},
        {"modf",        ex_modf,        1},
#ifndef _WIN32
        {"cbrt",        ex_cbrt,        1},
        {"erf",         ex_erf,         1},
        {"erfc",        ex_erfc,        1},
        {"expm1",       ex_expm1,       1},
        {"log1p",       ex_log1p,       1},
        {"isinf",       ex_isinf,       1},
        {"finite",      ex_finite,      1},
        {"isnan",       ex_isnan,       1},
        {"copysign",    ex_copysign,    2},
        {"remainder",   ex_remainder,           2},
        {"asinh",       ex_asinh,       1},
        {"acosh",       ex_acosh,       1},
        {"atanh",       ex_atanh,       1},     /* hyperbolic atan */
        {"round",       ex_round,       1},
        {"trunc",       ex_trunc,       1},
        {"nearbyint",   ex_nearbyint,   1},
#endif
#ifdef PD
        {"size",        ex_size,        1},
        {"sum",         ex_sum,         1},
        {"Sum",         ex_Sum,         3},
#endif
#ifdef notdef
/* the following will be added once they are more popular in math libraries */
        {"hypoth",      ex_hypoth,      1},
#endif
        {0,             0,              0}
};

/*
 * FUN_EVAL --  do type checking, evaluate a function,
 *              if fltret is set return float
 *              otherwise return value based on regular typechecking,
 */
#define FUNC_EVAL(left, right, func, leftfuncast, rightfuncast, optr, fltret) \
switch (left->ex_type) {                                                \
case ET_INT:                                                            \
        switch(right->ex_type) {                                        \
        case ET_INT:                                                    \
                if (optr->ex_type == ET_VEC) {                          \
                        op = optr->ex_vec;                              \
                        scalar = (t_float)func(leftfuncast left->ex_int,  \
                                        rightfuncast right->ex_int);    \
                        j = e->exp_vsize;                               \
                        while (j--)                                     \
                                *op++ = scalar;                         \
                } else {                                                \
                        if (fltret) {                                   \
                                optr->ex_type = ET_FLT;                 \
                                optr->ex_flt = (t_float)func(leftfuncast  \
                                left->ex_int, rightfuncast right->ex_int); \
                        } else {                                        \
                                optr->ex_type = ET_INT;                 \
                                optr->ex_int = (int)func(leftfuncast    \
                                left->ex_int, rightfuncast right->ex_int); \
                        }                                               \
                }                                                       \
                break;                                                  \
        case ET_FLT:                                                    \
                if (optr->ex_type == ET_VEC) {                          \
                        op = optr->ex_vec;                              \
                        scalar = (t_float)func(leftfuncast left->ex_int,  \
                                        rightfuncast right->ex_flt);    \
                        j = e->exp_vsize;                               \
                        while (j--)                                     \
                                *op++ = scalar;                         \
                } else {                                                \
                        optr->ex_type = ET_FLT;                         \
                        optr->ex_flt = (t_float)func(leftfuncast left->ex_int,  \
                                        rightfuncast right->ex_flt);    \
                }                                                       \
                break;                                                  \
        case ET_VEC:                                                    \
        case ET_VI:                                                     \
                if (optr->ex_type != ET_VEC) {                          \
                        if (optr->ex_type == ET_VI) {                   \
                                post("expr~: Int. error %d", __LINE__); \
                                abort();                                \
                        }                                               \
                        optr->ex_type = ET_VEC;                         \
                        optr->ex_vec = (t_float *)                      \
                          fts_malloc(sizeof (t_float)*e->exp_vsize);    \
                }                                                       \
                scalar = left->ex_int;                                  \
                rp = right->ex_vec;                                     \
                op = optr->ex_vec;                                      \
                j = e->exp_vsize;                                       \
                while (j--) {                                           \
                        *op++ = (t_float)func(leftfuncast scalar,         \
                                                rightfuncast *rp);      \
                        rp++;                                           \
                }                                                       \
                break;                                                  \
        case ET_SYM:                                                    \
        default:                                                        \
                post_error((fts_object_t *) e,                          \
                      "expr: FUNC_EVAL(%d): bad right type %ld\n",      \
                                              __LINE__, right->ex_type);\
        }                                                               \
        break;                                                          \
case ET_FLT:                                                            \
        switch(right->ex_type) {                                        \
        case ET_INT:                                                    \
                if (optr->ex_type == ET_VEC) {                          \
                        op = optr->ex_vec;                              \
                        scalar = (t_float)func(leftfuncast left->ex_flt,  \
                                        rightfuncast right->ex_int);    \
                        j = e->exp_vsize;                               \
                        while (j--)                                     \
                                *op++ = scalar;                         \
                } else {                                                \
                        optr->ex_type = ET_FLT;                         \
                        optr->ex_flt = (t_float)func(leftfuncast left->ex_flt,  \
                                        rightfuncast right->ex_int);    \
                }                                                       \
                break;                                                  \
        case ET_FLT:                                                    \
                if (optr->ex_type == ET_VEC) {                          \
                        op = optr->ex_vec;                              \
                        scalar = (t_float)func(leftfuncast left->ex_flt,  \
                                        rightfuncast right->ex_flt);    \
                        j = e->exp_vsize;                               \
                        while (j--)                                     \
                                *op++ = scalar;                         \
                } else {                                                \
                        optr->ex_type = ET_FLT;                         \
                        optr->ex_flt = (t_float)func(leftfuncast left->ex_flt,  \
                                        rightfuncast right->ex_flt);    \
                }                                                       \
                break;                                                  \
        case ET_VEC:                                                    \
        case ET_VI:                                                     \
                if (optr->ex_type != ET_VEC) {                          \
                        if (optr->ex_type == ET_VI) {                   \
                                post("expr~: Int. error %d", __LINE__); \
                                abort();                                \
                        }                                               \
                        optr->ex_type = ET_VEC;                         \
                        optr->ex_vec = (t_float *)                      \
                          fts_malloc(sizeof (t_float) * e->exp_vsize);\
                }                                                       \
                scalar = left->ex_flt;                                  \
                rp = right->ex_vec;                                     \
                op = optr->ex_vec;                                      \
                j = e->exp_vsize;                                       \
                while (j--) {                                           \
                        *op++ = (t_float)func(leftfuncast scalar,         \
                                                rightfuncast *rp);      \
                        rp++;                                           \
                }                                                       \
                break;                                                  \
        case ET_SYM:                                                    \
        default:                                                        \
                post_error((fts_object_t *) e,                  \
                      "expr: FUNC_EVAL(%d): bad right type %ld\n",      \
                                              __LINE__, right->ex_type);\
        }                                                               \
        break;                                                          \
case ET_VEC:                                                            \
case ET_VI:                                                             \
        if (optr->ex_type != ET_VEC) {                                  \
                if (optr->ex_type == ET_VI) {                           \
                        post("expr~: Int. error %d", __LINE__);         \
                        abort();                                        \
                }                                                       \
                optr->ex_type = ET_VEC;                                 \
                optr->ex_vec = (t_float *)                              \
                  fts_malloc(sizeof (t_float) * e->exp_vsize);  \
        }                                                               \
        op = optr->ex_vec;                                              \
        lp = left->ex_vec;                                              \
        switch(right->ex_type) {                                        \
        case ET_INT:                                                    \
                scalar = right->ex_int;                                 \
                j = e->exp_vsize;                                       \
                while (j--) {                                           \
                        *op++ = (t_float)func(leftfuncast *lp,            \
                                                rightfuncast scalar);   \
                        lp++;                                           \
                }                                                       \
                break;                                                  \
        case ET_FLT:                                                    \
                scalar = right->ex_flt;                                 \
                j = e->exp_vsize;                                       \
                while (j--) {                                           \
                        *op++ = (t_float)func(leftfuncast *lp,            \
                                                rightfuncast scalar);   \
                        lp++;                                           \
                }                                                       \
                break;                                                  \
        case ET_VEC:                                                    \
        case ET_VI:                                                     \
                rp = right->ex_vec;                                     \
                j = e->exp_vsize;                                       \
                while (j--) {                                           \
                        /*                                              \
                         * on a RISC processor one could copy           \
                         * 8 times in each round to get a considerable  \
                         * improvement                                  \
                         */                                             \
                        *op++ = (t_float)func(leftfuncast *lp,            \
                                                rightfuncast *rp);      \
                        rp++; lp++;                                     \
                }                                                       \
                break;                                                  \
        case ET_SYM:                                                    \
        default:                                                        \
                post_error((fts_object_t *) e,                  \
                      "expr: FUNC_EVAL(%d): bad right type %ld\n",      \
                                              __LINE__, right->ex_type);\
        }                                                               \
        break;                                                          \
case ET_SYM:                                                            \
default:                                                                \
                post_error((fts_object_t *) e,                  \
                      "expr: FUNC_EVAL(%d): bad left type %ld\n",               \
                                              __LINE__, left->ex_type); \
}

/*
 * FUNC_EVAL_UNARY - evaluate a unary function,
 *              if fltret is set return t_float
 *              otherwise return value based on regular typechecking,
 */
#define FUNC_EVAL_UNARY(left, func, leftcast, optr, fltret)             \
switch(left->ex_type) {                                         \
case ET_INT:                                                    \
        if (optr->ex_type == ET_VEC) {                          \
                ex_mkvector(optr->ex_vec,                       \
                (t_float)(func (leftcast left->ex_int)), e->exp_vsize);\
                break;                                          \
        }                                                       \
        if (fltret) {                                           \
                optr->ex_type = ET_FLT;                         \
                optr->ex_flt = (t_float) func(leftcast left->ex_int); \
                break;                                          \
        }                                                       \
        optr->ex_type = ET_INT;                                 \
        optr->ex_int = (int) func(leftcast left->ex_int);       \
        break;                                                  \
case ET_FLT:                                                    \
        if (optr->ex_type == ET_VEC) {                          \
                ex_mkvector(optr->ex_vec,                       \
                (t_float)(func (leftcast left->ex_flt)), e->exp_vsize);\
                break;                                          \
        }                                                       \
        optr->ex_type = ET_FLT;                                 \
        optr->ex_flt = (t_float) func(leftcast left->ex_flt);     \
        break;                                                  \
case ET_VI:                                                     \
case ET_VEC:                                                    \
        if (optr->ex_type != ET_VEC) {                          \
                optr->ex_type = ET_VEC;                         \
                optr->ex_vec = (t_float *)                      \
                  fts_malloc(sizeof (t_float)*e->exp_vsize);    \
        }                                                       \
        op = optr->ex_vec;                                      \
        lp = left->ex_vec;                                      \
        j = e->exp_vsize;                                       \
        while (j--)                                             \
                *op++ = (t_float)(func (leftcast *lp++));         \
        break;                                                  \
default:                                                        \
        post_error((fts_object_t *) e,                          \
                "expr: FUNV_EVAL_UNARY(%d): bad left type %ld\n",\
                                      __LINE__, left->ex_type); \
}

#undef min
#undef max
#define min(x,y)        (x > y ? y : x)
#define max(x,y)        (x > y ? x : y)

#define FUNC_DEF(ex_func, func, castleft, castright, fltret);                   \
static void                                                             \
ex_func(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)\
{                                                                       \
        struct ex_ex *left, *right;                                     \
        t_float *op; /* output pointer */                                 \
        t_float *lp, *rp;         /* left and right vector pointers */    \
        t_float scalar;                                                   \
        int j;                                                          \
                                                                        \
        left = argv++;                                                  \
        right = argv;                                                   \
        FUNC_EVAL(left, right, func, castleft, castright, optr, fltret); \
}


#define FUNC_DEF_UNARY(ex_func, func, cast, fltret);                    \
static void                                                             \
ex_func(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)\
{                                                                       \
        struct ex_ex *left;                                             \
        t_float *op; /* output pointer */                                 \
        t_float *lp, *rp;         /* left and right vector pointers */    \
        t_float scalar;                                                   \
        int j;                                                          \
                                                                        \
        left = argv++;                                                  \
                                                                        \
        FUNC_EVAL_UNARY(left, func, cast, optr, fltret);                \
}

/*
 * ex_min -- if any of the arguments are or the output are vectors, a vector
 *           of floats is generated otherwise the type of the result is the
 *           type of the smaller value
 */
static void
ex_min(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left, *right;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;
        right = argv;

        FUNC_EVAL(left, right, min, (double), (double), optr, 0);
}

/*
 * ex_max -- if any of the arguments are or the output are vectors, a vector
 *           of floats is generated otherwise the type of the result is the
 *           type of the larger value
 */
static void
ex_max(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left, *right;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;
        right = argv;

        FUNC_EVAL(left, right, max, (double), (double), optr, 0);
}

/*
 * ex_toint -- convert to integer
 */
static void
ex_toint(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

#define toint(x)        ((int)(x))
                FUNC_EVAL_UNARY(left, toint, (int), optr, 0);
        }

#ifdef _WIN32
/* No rint in NT land ??? */
double rint(double x);

double
rint(double x)
{
        return (floor(x + 0.5));
}
#endif

/*
 * ex_rint -- rint() round to the nearest int according to the common
 *            rounding mechanism
 */
static void
ex_rint(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;


        FUNC_EVAL_UNARY(left, rint, (double), optr, 1);
}

/*
 * ex_tofloat -- convert to t_float
 */
static void
ex_tofloat(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

#define tofloat(x)      ((t_float)(x))
        FUNC_EVAL_UNARY(left, tofloat, (t_float), optr, 1);
}


/*
 * ex_pow -- the power of
 */
static void
ex_pow(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left, *right;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;
        right = argv;
        FUNC_EVAL(left, right, pow, (double), (double), optr, 1);
}

/*
 * ex_sqrt -- square root
 */
static void
ex_sqrt(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, sqrt, (double), optr, 1);
}

/*
 * ex_exp -- e to the power of
 */
static void
ex_exp(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, exp, (double), optr, 1);
}

/*
 * ex_log -- 10 based logarithm
 */
static void
ex_log(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, log10, (double), optr, 1);
}

/*
 * ex_ln -- natural log
 */
static void
ex_ln(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, log, (double), optr, 1);
}

static void
ex_sin(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, sin, (double), optr, 1);
}

static void
ex_cos(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, cos, (double), optr, 1);
}


static void
ex_tan(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, tan, (double), optr, 1);
}

static void
ex_asin(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, asin, (double), optr, 1);
}

static void
ex_acos(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, acos, (double), optr, 1);
}


static void
ex_atan(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, atan, (double), optr, 1);
}

/*
 *ex_atan2 --
 */
static void
ex_atan2(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left, *right;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;
        right = argv;
        FUNC_EVAL(left, right, atan2, (double), (double), optr, 1);
}

/*
 * ex_fmod -- floating point modulo
 */
static void
ex_fmod(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left, *right;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;
        right = argv;
        FUNC_EVAL(left, right, fmod, (double), (double), optr, 1);
}


/*
 * ex_floor -- floor
 */
static void
ex_floor(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;
        FUNC_EVAL_UNARY(left, floor, (double), optr, 1);
}


/*
 * ex_ceil -- ceil
 */
static void
ex_ceil(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;
        FUNC_EVAL_UNARY(left, ceil, (double), optr, 1);
}

static void
ex_sinh(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, sinh, (double), optr, 1);
}

static void
ex_cosh(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, cosh, (double), optr, 1);
}


static void
ex_tanh(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, tanh, (double), optr, 1);
}


#ifndef _WIN32
static void
ex_asinh(t_expr *e, long argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, asinh, (double), optr, 1);
}

static void
ex_acosh(t_expr *e, long argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, acosh, (double), optr, 1);
}

static void
ex_atanh(t_expr *e, long argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, atanh, (double), optr, 1);
}
#endif

static int
ex_dofact(int i)
{
        int ret = 0;

        if (i)
                ret = 1;
        else
                return (1);

        do {
                ret *= i;
        } while (--i);

        return(ret);
}

static void
ex_fact(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, ex_dofact,  (int), optr, 0);
}

static int
ex_dorandom(int i1, int i2)
{
                int i;
                int j;

                j = rand() & 0x7fffL;
                i = i1 + (int)((((float)(i2 - i1)) * (float)j) / pow (2, 15));
                return (i);
}
/*
 * ex_random -- return a random number
 */
static void
ex_random(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left, *right;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;
        right = argv;
        FUNC_EVAL(left, right, ex_dorandom, (int), (int), optr, 0);
}


static void
ex_abs(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float scalar;
        int j;

        left = argv++;

        FUNC_EVAL_UNARY(left, fabs, (double), optr, 0);
}

/*
 *ex_if -- floating point modulo
 */
static void
ex_if(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        struct ex_ex *left, *right, *cond, *res;
        t_float *op; /* output pointer */
        t_float *lp, *rp;         /* left and right vector pointers */
        t_float *cp;              /* condition pointer */
        t_float leftvalue, rightvalue;
        int j;

        cond = argv++;
        left = argv++;
        right = argv;

        switch (cond->ex_type) {
        case ET_VEC:
        case ET_VI:
                if (optr->ex_type != ET_VEC) {
                        if (optr->ex_type == ET_VI) {
                                /* SDY remove this test */
                                post("expr~: Int. error %d", __LINE__);
                                return;
                        }
                                optr->ex_type = ET_VEC;
                                optr->ex_vec = (t_float *)
                                  fts_malloc(sizeof (t_float) * e->exp_vsize);
                }
                op = optr->ex_vec;
                j = e->exp_vsize;
                cp = cond->ex_vec;
                switch (left->ex_type) {
                case ET_INT:
                        leftvalue = left->ex_int;
                        switch (right->ex_type) {
                        case ET_INT:
                                rightvalue = right->ex_int;
                                while (j--) {
                                        if (*cp++)
                                                *op++ = leftvalue;
                                        else
                                                *op++ = rightvalue;
                                }
                        return;
                        case ET_FLT:
                                rightvalue = right->ex_flt;
                                while (j--) {
                                        if (*cp++)
                                                *op++ = leftvalue;
                                        else
                                                *op++ = rightvalue;
                                }
                                return;
                        case ET_VEC:
                        case ET_VI:
                                rp = right->ex_vec;
                                while (j--) {
                                        if (*cp++)
                                                *op++ = leftvalue;
                                        else
                                                *op++ = *rp;
                                        rp++;
                                }
                                return;
                        case ET_SYM:
                        default:
                                post_error((fts_object_t *) e,
                              "expr: FUNC_EVAL(%d): bad right type %ld\n",
                                                      __LINE__, right->ex_type);
                                return;
                        }
                case ET_FLT:
                        leftvalue = left->ex_flt;
                        switch (right->ex_type) {
                        case ET_INT:
                                rightvalue = right->ex_int;
                                while (j--) {
                                        if (*cp++)
                                                *op++ = leftvalue;
                                        else
                                                *op++ = rightvalue;
                                }
                                return;
                        case ET_FLT:
                                rightvalue = right->ex_flt;
                                while (j--) {
                                        if (*cp++)
                                                *op++ = leftvalue;
                                        else
                                                *op++ = rightvalue;
                                }
                                return;
                        case ET_VEC:
                        case ET_VI:
                                rp = right->ex_vec;
                                while (j--) {
                                        if (*cp++)
                                                *op++ = leftvalue;
                                        else
                                                *op++ = *rp;
                                        rp++;
                                }
                                return;
                        case ET_SYM:
                        default:
                                post_error((fts_object_t *) e,
                              "expr: FUNC_EVAL(%d): bad right type %ld\n",
                                                      __LINE__, right->ex_type);
                                return;
                        }
                case ET_VEC:
                case ET_VI:
                        lp = left->ex_vec;
                        switch (right->ex_type) {
                        case ET_INT:
                                rightvalue = right->ex_int;
                                while (j--) {
                                        if (*cp++)
                                                *op++ = *lp;
                                        else
                                                *op++ = rightvalue;
                                        lp++;
                                }
                                return;
                        case ET_FLT:
                                rightvalue = right->ex_flt;
                                while (j--) {
                                        if (*cp++)
                                                *op++ = *lp;
                                        else
                                                *op++ = rightvalue;
                                        lp++;
                                }
                                return;
                        case ET_VEC:
                        case ET_VI:
                                rp = right->ex_vec;
                                while (j--) {
                                        if (*cp++)
                                                *op++ = *lp;
                                        else
                                                *op++ = *rp;
                                        lp++; rp++;
                                }
                                return;
                        case ET_SYM:
                        default:
                                post_error((fts_object_t *) e,
                              "expr: FUNC_EVAL(%d): bad right type %ld\n",
                                                      __LINE__, right->ex_type);
                                return;
                        }
                case ET_SYM:
                default:
                        post_error((fts_object_t *) e,
                      "expr: FUNC_EVAL(%d): bad left type %ld\n",
                                              __LINE__, left->ex_type);
                        return;
                }
        case ET_INT:
                if (cond->ex_int)
                        res = left;
                else
                        res = right;
                break;
        case ET_FLT:
                if (cond->ex_flt)
                        res = left;
                else
                        res = right;
                break;
        case ET_SYM:
        default:
                post_error((fts_object_t *) e,
              "expr: FUNC_EVAL(%d): bad condition type %ld\n",
                                      __LINE__, cond->ex_type);
                return;
        }
        switch(res->ex_type) {
        case ET_INT:
                if (optr->ex_type == ET_VEC) {
                        ex_mkvector(optr->ex_vec, (t_float)res->ex_int,
                                                                e->exp_vsize);
                        return;
                }
                *optr = *res;
                return;
        case ET_FLT:
                if (optr->ex_type == ET_VEC) {
                        ex_mkvector(optr->ex_vec, (t_float)res->ex_flt,
                                                                e->exp_vsize);
                        return;
                }
                *optr = *res;
                return;
        case ET_VEC:
        case ET_VI:
                if (optr->ex_type != ET_VEC) {
                        if (optr->ex_type == ET_VI) {
                                /* SDY remove this test */
                                post("expr~: Int. error %d", __LINE__);
                                return;
                        }
                                optr->ex_type = ET_VEC;
                                optr->ex_vec = (t_float *)
                                  fts_malloc(sizeof (t_float) * e->exp_vsize);
                }
                memcpy(optr->ex_vec, res->ex_vec, e->exp_vsize*sizeof(t_float));
                return;
        case ET_SYM:
        default:
                post_error((fts_object_t *) e,
              "expr: FUNC_EVAL(%d): bad res type %ld\n",
                                      __LINE__, res->ex_type);
                return;
        }

}

/*
 * ex_imodf -   extract signed integral value from floating-point number
 */
static double
imodf(double x)
{
        double  xx;

        modf(x, &xx);
        return (xx);
}
FUNC_DEF_UNARY(ex_imodf, imodf, (double), 1);

/*
 * ex_modf - extract signed  fractional value from floating-point number
 *
 *  using fracmodf because fmodf() is alrady defined in a .h file
 */
static double
fracmodf(double x)
{
        double  xx;

        return(modf(x, &xx));
}
FUNC_DEF_UNARY(ex_modf, fracmodf, (double), 1);

/*
 * ex_ldexp  -  multiply floating-point number by integral power of 2
 */
FUNC_DEF(ex_ldexp, ldexp, (double), (int), 1);

#ifndef _WIN32
/*
 * ex_cbrt - cube root
 */
FUNC_DEF_UNARY(ex_cbrt, cbrt, (double), 1);

/*
 * ex_erf - error function
 */
FUNC_DEF_UNARY(ex_erf, erf, (double), 1);

/*
 * ex_erfc - complementary error function
 */
FUNC_DEF_UNARY(ex_erfc, erfc, (double), 1);

/*
 * ex_expm1 - exponential minus 1,
 */
FUNC_DEF_UNARY(ex_expm1, expm1, (double), 1);

/*
 * ex_log1p - logarithm of 1 plus
 */
FUNC_DEF_UNARY(ex_log1p, log1p, (double), 1);

/*
 * ex_isinf - is the value infinite,
 */
FUNC_DEF_UNARY(ex_isinf, isinf, (double), 0);

/*
 * ex_finite - is the value finite
 */
FUNC_DEF_UNARY(ex_finite, isfinite, (double), 0);

/*
 * ex_isnan -- is the resut a nan (Not a number)
 */
FUNC_DEF_UNARY(ex_isnan, isnan, (double), 0);

/*
 * ex_copysign - copy sign of a number
 */
FUNC_DEF(ex_copysign, copysign, (double), (double), 1);

/*
 * drem() is now obsolute
 * ex_drem - floating-point remainder function
 */
FUNC_DEF(ex_drem, remainder, (double), (double), 1);

/*
 * ex_remainder - floating-point remainder function
 */
FUNC_DEF(ex_remainder, remainder, (double), (double), 1);

/*
 * ex_round -  round to nearest integer, away from zero
 */
FUNC_DEF_UNARY(ex_round, round, (double), 1);

/*
 * ex_trunc -  round to interger, towards zero
 */
FUNC_DEF_UNARY(ex_trunc, trunc, (double), 1);

/*
 * ex_nearbyint -  round to nearest integer
 */
FUNC_DEF_UNARY(ex_nearbyint, nearbyint, (double), 1);

#endif

#ifdef notdef
/* the following will be added once they are more popular in math libraries */
/*
 * ex_hypoth - Euclidean distance function
 */
FUNC_DEF(ex_hypoth, hypoth, (double), (double), 1);

#endif
