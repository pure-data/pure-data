/* Copyright (c) IRCAM.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* "expr" was written by Shahrokh Yadegari c. 1989. -msp */
/* "expr~" and "fexpr~" conversion by Shahrokh Yadegari c. 1999,2000 */

/*
 * Feb 2002 - added access to variables
 *            multiple expression support
 *            new short hand forms for fexpr~
 *              now $y or $y1 = $y1[-1] and $y2 = $y2[-1]
 * --sdy
 *
 *
 *  version 0.50 - March 2016
 *  version 0.55 - July 2017
 *  version 0.56 - January 2018
 *  version 0.57 - October 2020
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#include "x_vexp.h"

static char *exp_version = "0.57";

extern struct ex_ex *ex_eval(struct expr *expr, struct ex_ex *eptr,
                                                struct ex_ex *optr, int n);
extern void ex_size(t_expr *expr, long int argc, struct ex_ex *argv,
                                                        struct ex_ex *optr);
extern void ex_sum(t_expr *expr, long int argc, struct ex_ex *argv,
                                                        struct ex_ex *optr);
extern void ex_Sum(t_expr *expr, long int argc, struct ex_ex *argv,
                                                        struct ex_ex *optr);
extern void ex_avg(t_expr *expr, long int argc, struct ex_ex *argv,
                                                        struct ex_ex *optr);
extern void ex_Avg(t_expr *expr, long int argc, struct ex_ex *argv,
                                                        struct ex_ex *optr);
extern void ex_tabread4(t_expr *expr, long int argc, struct ex_ex *argv,
                                                        struct ex_ex *optr);
extern void ex_store(t_expr *expr, long int argc, struct ex_ex *argv,
                                                        struct ex_ex *optr);

#ifdef PD
static t_class *expr_class;
static t_class *expr_tilde_class;
static t_class *fexpr_tilde_class;
#else /* MSP */
void *expr_tilde_class;
#endif


/*------------------------- expr class -------------------------------------*/

extern int expr_donew(struct expr *expr, int ac, t_atom *av);

/*#define EXPR_DEBUG*/

static void expr_bang(t_expr *x);
t_int *expr_perform(t_int *w);


static void
expr_list(t_expr *x, t_symbol *s, int argc, const t_atom *argv)
{
        int i;

        if (argc > EX_MAX_INLETS) argc = EX_MAX_INLETS;

        for (i = 0; i < argc; i++)
        {
                if (argv[i].a_type == A_FLOAT)
                {
                        if (x->exp_var[i].ex_type == ET_FI)
                                x->exp_var[i].ex_flt = argv[i].a_w.w_float;
                        else if (x->exp_var[i].ex_type == ET_II)
                                x->exp_var[i].ex_int = argv[i].a_w.w_float;
                        else if (x->exp_var[i].ex_type)
                            pd_error(x, "expr: type mismatch for inlet %d", i + 1);
                }
                else if (argv[i].a_type == A_SYMBOL)
                {
                        if (x->exp_var[i].ex_type == ET_SI)
                                x->exp_var[i].ex_ptr = (char *)argv[i].a_w.w_symbol;
                        else if (x->exp_var[i].ex_type)
                            pd_error(x, "expr: type mismatch for inlet %d", i + 1);
                }
        }
        expr_bang(x);
}

static void
expr_flt(t_expr *x, t_float f, int in)
{
        if (in >= EX_MAX_INLETS)
                return;

        if (x->exp_var[in].ex_type == ET_FI)
                x->exp_var[in].ex_flt = f;
        else if (x->exp_var[in].ex_type == ET_II)
                x->exp_var[in].ex_int = f;
}

static t_class *exprproxy_class;

typedef struct _exprproxy {
        t_pd p_pd;
        int p_index;
        t_expr *p_owner;
        struct _exprproxy *p_next;
} t_exprproxy;

t_exprproxy *exprproxy_new(t_expr *owner, int indx);
void exprproxy_float(t_exprproxy *p, t_floatarg f);

t_exprproxy *
exprproxy_new(t_expr *owner, int indx)
{
        t_exprproxy *x = (t_exprproxy *)pd_new(exprproxy_class);
        x->p_owner = owner;
        x->p_index = indx;
        x->p_next = owner->exp_proxy;
        owner->exp_proxy = x;
        return (x);
}

void
exprproxy_float(t_exprproxy *p, t_floatarg f)
{
        t_expr *x = p->p_owner;
        int in = p->p_index;

        if (in >= EX_MAX_INLETS)
                return;

        if (x->exp_var[in].ex_type == ET_FI)
                x->exp_var[in].ex_flt = f;
        else if (x->exp_var[in].ex_type == ET_II)
                x->exp_var[in].ex_int = f;
}

/* method definitions */
static void
expr_ff(t_expr *x)
{
        t_exprproxy *y;
        int i;

        y = x->exp_proxy;
        while (y)
        {
                x->exp_proxy = y->p_next;
#ifdef PD
                pd_free(&y->p_pd);
#else /*MSP */
        /* SDY find out what needs to be called for MSP */

#endif
                y = x->exp_proxy;
        }
        t_freebytes(x->exp_string, strlen(x->exp_string));
        for (i = 0 ; i < x->exp_nexpr; i++)
                if (x->exp_stack[i])
                        fts_free(x->exp_stack[i]);
        /*
         * free all the allocated buffers here for expr~ and fexpr~
         */
        for (i = 0; i < EX_MAX_INLETS; i++) {
                if (x->exp_p_var[i])
                        fts_free(x->exp_p_var[i]);
                if (x->exp_p_res[i])
                        fts_free(x->exp_p_res[i]);
                if (x->exp_tmpres[i])
                        fts_free(x->exp_tmpres[i]);
        }


}

static void
expr_bang(t_expr *x)
{
        int i;

#ifdef EXPR_DEBUG
        {
                struct ex_ex *eptr;

                for (i = 0, eptr = x->exp_var;  ; eptr++, i++)
                {
                        if (!eptr->ex_type)
                                break;
                        switch (eptr->ex_type)
                        {
                        case ET_II:
                                fprintf(stderr,"ET_II: %d \n", eptr->ex_int);
                                break;

                        case ET_FI:
                                fprintf(stderr,"ET_FT: %f \n", eptr->ex_flt);
                                break;

                        default:
                                fprintf(stderr,"oups\n");
                        }
                }
        }
#endif
        /* banging a signal or filter object means nothing */
        if (!IS_EXPR(x))
                return;
        for (i = x->exp_nexpr - 1; i > -1 ; i--) {
                if (!ex_eval(x, x->exp_stack[i], &x->exp_res[i], 0)) {
                        /*fprintf(stderr,"expr_bang(error evaluation)\n"); */
                /*  SDY now that we have multiple ones, on error we should
                 * continue
                        return;
                 */
                }
                switch(x->exp_res[i].ex_type) {
                case ET_INT:
                        outlet_float(x->exp_outlet[i],
                                        (t_float) x->exp_res[i].ex_int);
                        break;

                case ET_FLT:
                        outlet_float(x->exp_outlet[i],  x->exp_res[i].ex_flt);
                        break;

                case ET_SYM:
                        if (x->exp_res[i].ex_flags & EX_F_TSYM) {
                                outlet_symbol(x->exp_outlet[i],
                                                                                        gensym(x->exp_res[i].ex_ptr));
                                free(x->exp_res[i].ex_ptr);
                        } else {
                                outlet_symbol(x->exp_outlet[i],
                                                                                        (t_symbol *) x->exp_res[i].ex_ptr);
                        }
                        break;

                case ET_SI:
                        if (x->exp_var[x->exp_res[i].ex_int].ex_ptr)
                                outlet_symbol(x->exp_outlet[i], (t_symbol *)
                                      x->exp_var[x->exp_res[i].ex_int].ex_ptr);
                        else
                                outlet_symbol(x->exp_outlet[i],  gensym(""));
                        break;

                default:
                        post_error(x,
                            "expr:'%s': bang: unrecognized result %ld\n",
                                        x->exp_string, x->exp_res[i].ex_type);
                }
        }
}

static int expcount;

static t_expr *
#ifdef PD
expr_new(t_symbol *s, int ac, t_atom *av)
#else /* MSP */
Nexpr_new(t_symbol *s, int ac, t_atom *av)
#endif
{
        struct expr *x;
        int i, ninlet;
        struct ex_ex *eptr;
        t_atom fakearg;
        int dsp_index;  /* keeping track of the dsp inlets */


//printf ("expcount = %d\n", expcount++);
/*
 * SDY - we may need to call dsp_setup() in this function
 */

        if (!ac)
        {
                ac = 1;
                av = &fakearg;
                SETFLOAT(&fakearg, 0);
        }

#ifdef PD
        /*
         * figure out if we are expr, expr~, or fexpr~
         */
        if (!strcmp("expr", s->s_name)) {
                x = (t_expr *)pd_new(expr_class);
                SET_EXPR(x);
        } else if (!strcmp("expr~", s->s_name)) {
                x = (t_expr *)pd_new(expr_tilde_class);
                SET_EXPR_TILDE(x);
        } else if (!strcmp("fexpr~", s->s_name)) {
                x = (t_expr *)pd_new(fexpr_tilde_class);
                SET_FEXPR_TILDE(x);
        } else {
                post("expr_new: bad object name '%s'", s->s_name);
                /* assume expr */
                x = (t_expr *)pd_new(expr_class);
                SET_EXPR(x);
        }
#else /* MSP */
        /* for now assume an expr~ */
        x = (t_expr *)pd_new(expr_tilde_class);
        SET_EXPR_TILDE(x);
#endif

        /*
         * initialize the newly allocated object
         */
        x->exp_proxy = 0;
        x->exp_nivec = 0;
        x->exp_nexpr = 0;
        x->exp_error = 0;
        for (i = 0; i < EX_MAX_INLETS; i++) {
                x->exp_stack[i] = (struct ex_ex *)0;
                x->exp_outlet[i] = (t_outlet *)0;
                x->exp_res[i].ex_type = 0;
                x->exp_res[i].ex_int = 0;
                x->exp_p_res[i] = (t_float *)0;
                x->exp_var[i].ex_type = 0;
                x->exp_var[i].ex_int = 0;
                x->exp_p_var[i] = (t_float *)0;
                x->exp_tmpres[i] = (t_float *)0;
                x->exp_vsize = 0;
        }
        x->exp_f = 0; /* save the control value to be transformed to signal */


        if (expr_donew(x, ac, av))
        {
                pd_error(x, "expr: syntax error");
/*
SDY the following coredumps why?
                pd_free(&x->exp_ob.ob_pd);
*/
                return (0);
        }

        ninlet = 1;
        for (i = 0, eptr = x->exp_var; i < EX_MAX_INLETS ; i++, eptr++)
                if (eptr->ex_type) {
                        ninlet = i + 1;
                }

        /*
         * create the new inlets
         */
        for (i = 1, eptr = x->exp_var + 1, dsp_index=1; i<ninlet ; i++, eptr++)
        {
                t_exprproxy *p;
                switch (eptr->ex_type)
                {
                case 0:
                        /* nothing is using this inlet */
                        if (i < ninlet)
#ifdef PD
                                floatinlet_new(&x->exp_ob, &eptr->ex_flt);
#else /* MSP */
                                inlet_new(&x->exp_ob, "float");
#endif
                        break;

                case ET_II:
                case ET_FI:
                        p = exprproxy_new(x, i);
#ifdef PD
                        inlet_new(&x->exp_ob, &p->p_pd, &s_float, &s_float);
#else /* MSP */
                        inlet_new(&x->exp_ob, "float");
#endif
                        break;

                case ET_SI:
#ifdef PD
                        symbolinlet_new(&x->exp_ob, (t_symbol **)&eptr->ex_ptr);
#else /* MSP */
                        inlet_new(&x->exp_ob, "symbol");
#endif
                        break;

                case ET_XI:
                case ET_VI:
                        if (!IS_EXPR(x)) {
                                dsp_index++;
#ifdef PD
                                inlet_new(&x->exp_ob, &x->exp_ob.ob_pd,
                                                        &s_signal, &s_signal);
#else /* MSP */
                                inlet_new(&x->exp_ob, "signal");
#endif
                                break;
                        } else
                                post("expr: internal error expr_new");
                                /* falls through */
                default:
                        pd_error(x, "expr: bad type (%lx) inlet = %d\n",
                                            eptr->ex_type, i + 1);
                        break;
                }
        }
        if (IS_EXPR(x)) {
                for (i = 0; i < x->exp_nexpr; i++)
                        x->exp_outlet[i] = outlet_new(&x->exp_ob, 0);
        } else {
                for (i = 0; i < x->exp_nexpr; i++)
                        x->exp_outlet[i] = outlet_new(&x->exp_ob,
                                                        gensym("signal"));
                x->exp_nivec = dsp_index;
        }
        /*
         * for now assume a 64 sample size block but this may change once
         * expr_dsp is called
         */
        x->exp_vsize = 64;
        for (i = 0; i < x->exp_nexpr; i++) {
                x->exp_p_res[i] = fts_calloc(x->exp_vsize, sizeof (t_float));
                x->exp_tmpres[i] = fts_calloc(x->exp_vsize, sizeof (t_float));
        }
        for (i = 0; i < EX_MAX_INLETS; i++) {
                if (x->exp_var[i].ex_type == ET_VI ||
                    x->exp_var[i].ex_type == ET_XI ||
                    x->exp_var[i].ex_type == ET_YO ||
                    0)
                        x->exp_p_var[i] = fts_calloc(x->exp_vsize,
                                                        sizeof (t_float));
                else
                        x->exp_p_var[i] = (t_float *) 0;
        }

        return (x);
}

t_int *
expr_perform(t_int *w)
{
        int i, j;
        t_expr *x = (t_expr *)w[1];
        struct ex_ex res;
        int n;

        /* sanity check */
        if (IS_EXPR(x)) {
                post("expr_perform: bad x->exp_flags = %d", x->exp_flags);
                abort();
        }

        if (x->exp_flags & EF_STOP) {
                for (i = 0; i < x->exp_nexpr; i++)
                        memset(x->exp_res[i].ex_vec, 0,
                                        x->exp_vsize * sizeof (t_float));
                return (w + 2);
        }

        if (IS_EXPR_TILDE(x)) {
                /*
                 * if we have only one expression, we can write on
                 * on the output directly, otherwise we have to copy
                 * the data because, outputs could be the same buffer as
                 * inputs
                 */
                if ( x->exp_nexpr == 1) {
                        x->exp_res[0].ex_type = ET_VEC;
                        ex_eval(x, x->exp_stack[0], &x->exp_res[0], 0);
                } else {
                        res.ex_type = ET_VEC;
                        for (i = 0; i < x->exp_nexpr; i++) {
                                res.ex_vec = x->exp_tmpres[i];
                                ex_eval(x, x->exp_stack[i], &res, 0);
                        }
                        n = x->exp_vsize * sizeof(t_float);
                        for (i = 0; i < x->exp_nexpr; i++)
                                memcpy(x->exp_res[i].ex_vec, x->exp_tmpres[i],
                                                                        n);
                }
                return (w + 2);
        }

        if (!IS_FEXPR_TILDE(x)) {
                post_error(x, "expr_perform: bad x->exp_flags = %d - expecting fexpr",
                                                                x->exp_flags);
                return (w + 2);
        }
        /*
         * since the output buffer could be the same as one of the inputs
         * we need to keep the output in  a different buffer
         */
        for (i = 0; i < x->exp_vsize; i++) for (j = 0; j < x->exp_nexpr; j++) {
                res.ex_type = 0;
                res.ex_int = 0;
                ex_eval(x, x->exp_stack[j], &res, i);
                switch (res.ex_type) {
                case ET_INT:
                        x->exp_tmpres[j][i] = (t_float) res.ex_int;
                        break;
                case ET_FLT:
                        x->exp_tmpres[j][i] = res.ex_flt;
                        break;
                case ET_SYM:
                        if (!(x->exp_error & EE_BADSYM)) {
                                x->exp_error |= EE_BADSYM;
                                        post_error((t_object *) x,
                                                "fexpr~: '%s': cannot convert string to sample value\n",
                                                                        x->exp_string);
                                post_error(x,
                                      "fexpr~: No more such errors will be reported");
                                post_error(x,
                                      "fexpr~: till the next reset");
                        }
                        x->exp_tmpres[j][i] = 0;
                        break;
                default:
                        post("expr_perform: bad result type %d", res.ex_type);
                }
        }
        /*
         * copy inputs and results to the save buffers
         * inputs need to be copied first as the output buffer can be
         * same as an input buffer
         */
        n = x->exp_vsize * sizeof(t_float);
        for (i = 0; i < EX_MAX_INLETS; i++)
                if (x->exp_var[i].ex_type == ET_XI)
                        memcpy(x->exp_p_var[i], x->exp_var[i].ex_vec, n);
        for (i = 0; i < x->exp_nexpr; i++) {
                memcpy(x->exp_p_res[i], x->exp_tmpres[i], n);
                memcpy(x->exp_res[i].ex_vec, x->exp_tmpres[i], n);
        }
        return (w + 2);
}

static void
expr_dsp(t_expr *x, t_signal **sp)
{
        int i, nv;
        int newsize;

        x->exp_error = 0;               /* reset all errors */
        newsize = (x->exp_vsize !=  sp[0]->s_n);
        x->exp_vsize = sp[0]->s_n;      /* record the vector size */
        for (i = 0; i < x->exp_nexpr; i++) {
                x->exp_res[i].ex_type = ET_VEC;
                x->exp_res[i].ex_vec =  sp[x->exp_nivec + i]->s_vec;
        }
        for (i = 0, nv = 0; i < EX_MAX_INLETS; i++)
                /*
                 * the first inlet is always a signal
                 *
                 * SDY  We are warning the user till this limitation
                 * is taken away from pd
                 */
                if (!i || x->exp_var[i].ex_type == ET_VI ||
                                        x->exp_var[i].ex_type == ET_XI) {
                        if (nv >= x->exp_nivec) {
                          post("expr_dsp int. err nv = %d, x->exp_nive = %d",
                                                          nv,  x->exp_nivec);
                          abort();
                        }
                        x->exp_var[i].ex_vec  = sp[nv]->s_vec;
                        nv++;
                }
        /* we always have one inlet but we may not use it */
        if (nv != x->exp_nivec && (nv != 0 ||  x->exp_nivec != 1)) {
                post("expr_dsp internal error 2 nv = %d, x->exp_nive = %d",
                                                          nv,  x->exp_nivec);
                abort();
        }

        dsp_add(expr_perform, 1, (t_int *) x);

        /*
         * The buffer are now being allocated for expr~ and fexpr~
         * because if we have more than one expression we need the
         * temporary buffers, The save buffers are not really needed
        if (!IS_FEXPR_TILDE(x))
                return;
         */
        /*
         * if we have already allocated the buffers and we have a
         * new size free all the buffers
         */
        if (x->exp_p_res[0]) {
                if (!newsize)
                        return;
                /*
                 * if new size, reallocate all the previous buffers for fexpr~
                 */
                for (i = 0; i < x->exp_nexpr; i++) {
                        fts_free(x->exp_p_res[i]);
                        fts_free(x->exp_tmpres[i]);
                }
                for (i = 0; i < EX_MAX_INLETS; i++)
                        if (x->exp_p_var[i])
                                fts_free(x->exp_p_var[i]);

        }
        for (i = 0; i < x->exp_nexpr; i++) {
                x->exp_p_res[i] = fts_calloc(x->exp_vsize, sizeof (t_float));
                x->exp_tmpres[i] = fts_calloc(x->exp_vsize, sizeof (t_float));
        }
        for (i = 0; i < EX_MAX_INLETS; i++) {
                if (x->exp_var[i].ex_type == ET_VI ||
                    x->exp_var[i].ex_type == ET_XI ||
                    x->exp_var[i].ex_type == ET_YO ||
                    0)
                        x->exp_p_var[i] = fts_calloc(x->exp_vsize,
                                                        sizeof (t_float));
                else
                        x->exp_p_var[i] = (t_float *) 0;
        }
}

/*
 * expr_verbose -- toggle the verbose switch
 */
static void
expr_verbose(t_expr *x)
{
        if (x->exp_flags & EF_VERBOSE) {
                x->exp_flags &= ~EF_VERBOSE;
                post ("verbose off");
        } else {
                x->exp_flags |= EF_VERBOSE;
                post ("verbose on");
        }
}


static void
expr_version(t_expr *x)
{
        post( "expr, expr~, fexpr~ version %s", exp_version);
}

/*
 * expr_start -- turn on expr processing for now only used for fexpr~
 */
static void
expr_start(t_expr *x)
{
        x->exp_flags &= ~EF_STOP;
}

/*
 * expr_stop -- turn on expr processing for now only used for fexpr~
 */
static void
expr_stop(t_expr *x)
{
        x->exp_flags |= EF_STOP;
}
static void
fexpr_set_usage(void)
{
        post("fexpr~: set val ...");
        post("fexpr~: set {xy}[#] val ...");
}

/*
 * fexpr_tilde_set -- set previous values of the buffers
 *              set val val ... - sets the first elements of output buffers
 *              set x val ...   - sets the elements of the first input buffer
 *              set x# val ...  - sets the elements of the #th input buffers
 *              set y val ...   - sets the elements of the first output buffer
 *              set y# val ...  - sets the elements of the #th output buffers
 */
static void
fexpr_tilde_set(t_expr *x, t_symbol *s, int argc, t_atom *argv)
{
        t_symbol *sx;
        int vecno;
        int i, nargs;

        if (!argc)
                return;
        sx = atom_getsymbolarg(0, argc, argv);
        switch(sx->s_name[0]) {
        case 'x':
                if (!sx->s_name[1])
                        vecno = 0;
                else {
                        vecno = atoi(sx->s_name + 1);
                        if (!vecno) {
                                post("fexpr~.set: bad set x vector number");
                                fexpr_set_usage();
                                return;
                        }
                        if (vecno >= EX_MAX_INLETS) {
                                post("fexpr~.set: no more than %d inlets",
                                                              EX_MAX_INLETS);
                                return;
                        }
                        vecno--;
                }
                if (x->exp_var[vecno].ex_type != ET_XI) {
                        post("fexpr~-set: no signal at inlet %d", vecno + 1);
                        return;
                }
                nargs = argc - 1;
                if (!nargs) {
                        post("fexpr~-set: no argument to set");
                        return;
                }
                if (nargs > x->exp_vsize) {
                   post("fexpr~.set: %d set values larger than vector size(%d)",
                                                        nargs,  x->exp_vsize);
                   post("fexpr~.set: only the first %d values will be set",
                                                                x->exp_vsize);
                   nargs = x->exp_vsize;
                }
                for (i = 0; i < nargs; i++) {
                        x->exp_p_var[vecno][x->exp_vsize - i - 1] =
                                        atom_getfloatarg(i + 1, argc, argv);
                }
                return;
        case 'y':
                if (!sx->s_name[1])
                        vecno = 0;
                else {
                        vecno = atoi(sx->s_name + 1);
                        if (!vecno) {
                                post("fexpr~.set: bad set y vector number");
                                fexpr_set_usage();
                                return;
                        }
                        vecno--;
                }
                if (vecno >= x->exp_nexpr) {
                        post("fexpr~.set: only %d outlets", x->exp_nexpr);
                        return;
                }
                nargs = argc - 1;
                if (!nargs) {
                        post("fexpr~-set: no argument to set");
                        return;
                }
                if (nargs > x->exp_vsize) {
                   post("fexpr~-set: %d set values larger than vector size(%d)",
                                                        nargs,  x->exp_vsize);
                   post("fexpr~.set: only the first %d values will be set",
                                                                x->exp_vsize);
                   nargs = x->exp_vsize;
                }
                for (i = 0; i < nargs; i++) {
                        x->exp_p_res[vecno][x->exp_vsize - i - 1] =
                                        atom_getfloatarg(i + 1, argc, argv);
                }
                return;
        case 0:
                if (argc >  x->exp_nexpr) {
                        post("fexpr~.set: only %d outlets available",
                                                                x->exp_nexpr);
                        post("fexpr~.set: the extra set values are ignored");
                }
                for (i = 0; i < x->exp_nexpr && i < argc; i++)
                        x->exp_p_res[i][x->exp_vsize - 1] =
                                        atom_getfloatarg(i, argc, argv);
                return;
        default:
                fexpr_set_usage();
                return;
        }
        return;
}

/*
 * fexpr_tilde_clear - clear the past buffers
 */
static void
fexpr_tilde_clear(t_expr *x, t_symbol *s, int argc, t_atom *argv)
{
        t_symbol *sx;
        int vecno;
        int i, nargs;

        /*
         *  if no argument clear all input and output buffers
         */
        if (!argc) {
                for (i = 0; i < x->exp_nexpr; i++)
                        memset(x->exp_p_res[i], 0, x->exp_vsize*sizeof(t_float));
                for (i = 0; i < EX_MAX_INLETS; i++)
                        if (x->exp_var[i].ex_type == ET_XI)
                                memset(x->exp_p_var[i], 0,
                                                x->exp_vsize*sizeof(t_float));
                return;
        }
        if (argc > 1) {
                post("fexpr~ usage: 'clear' or 'clear {xy}[#]'");
                return;
        }

        sx = atom_getsymbolarg(0, argc, argv);
        switch(sx->s_name[0]) {
        case 'x':
                if (!sx->s_name[1])
                        vecno = 0;
                else {
                        vecno = atoi(sx->s_name + 1);
                        if (!vecno) {
                                post("fexpr~.clear: bad clear x vector number");
                                return;
                        }
                        if (vecno >= EX_MAX_INLETS) {
                                post("fexpr~.clear: no more than %d inlets",
                                                              EX_MAX_INLETS);
                                return;
                        }
                        vecno--;
                }
                if (x->exp_var[vecno].ex_type != ET_XI) {
                        post("fexpr~-clear: no signal at inlet %d", vecno + 1);
                        return;
                }
                memset(x->exp_p_var[vecno], 0, x->exp_vsize*sizeof(t_float));
                return;
        case 'y':
                if (!sx->s_name[1])
                        vecno = 0;
                else {
                        vecno = atoi(sx->s_name + 1);
                        if (!vecno) {
                                post("fexpr~.clear: bad clear y vector number");
                                return;
                        }
                        vecno--;
                }
                if (vecno >= x->exp_nexpr) {
                        post("fexpr~.clear: only %d outlets", x->exp_nexpr);
                        return;
                }
                memset(x->exp_p_res[vecno], 0, x->exp_vsize*sizeof(t_float));
                return;
                return;
        default:
                post("fexpr~ usage: 'clear' or 'clear {xy}[#]'");
                return;
        }
        return;
}

#ifdef PD

void
expr_setup(void)
{
        /*
         * expr initialization
         */
        expr_class = class_new(gensym("expr"), (t_newmethod)expr_new,
            (t_method)expr_ff, sizeof(t_expr), 0, A_GIMME, 0);
        class_addlist(expr_class, expr_list);
        exprproxy_class = class_new(gensym("exprproxy"), 0,
                                        0, sizeof(t_exprproxy), CLASS_PD, 0);
        class_addfloat(exprproxy_class, exprproxy_float);
        class_addmethod(expr_class,(t_method)expr_version,
                                                        gensym("version"), 0);

        /*
         * expr~ initialization
         */
        expr_tilde_class = class_new(gensym("expr~"), (t_newmethod)expr_new,
            (t_method)expr_ff, sizeof(t_expr), 0, A_GIMME, 0);
        class_addmethod(expr_tilde_class, nullfn, gensym("signal"), 0);
        CLASS_MAINSIGNALIN(expr_tilde_class, t_expr, exp_f);
        class_addmethod(expr_tilde_class,(t_method)expr_dsp, gensym("dsp"),
                                                        A_CANT, 0);
        class_sethelpsymbol(expr_tilde_class, gensym("expr"));
        class_addmethod(expr_tilde_class,(t_method)expr_version,
                                                        gensym("version"), 0);
        /*
         * fexpr~ initialization
         */
        fexpr_tilde_class = class_new(gensym("fexpr~"), (t_newmethod)expr_new,
            (t_method)expr_ff, sizeof(t_expr), 0, A_GIMME, 0);
        class_addmethod(fexpr_tilde_class, nullfn, gensym("signal"), 0);
        CLASS_MAINSIGNALIN(fexpr_tilde_class, t_expr, exp_f);
        class_addmethod(fexpr_tilde_class,(t_method)expr_start,
                                                        gensym("start"), 0);
        class_addmethod(fexpr_tilde_class,(t_method)expr_stop,
                                                        gensym("stop"), 0);

        class_addmethod(fexpr_tilde_class,(t_method)expr_dsp,gensym("dsp"),
                                                        A_CANT, 0);
        class_addmethod(fexpr_tilde_class, (t_method)fexpr_tilde_set,
                        gensym("set"), A_GIMME, 0);
        class_addmethod(fexpr_tilde_class, (t_method)fexpr_tilde_clear,
                        gensym("clear"), A_GIMME, 0);
        class_addmethod(fexpr_tilde_class,(t_method)expr_verbose,
                                                        gensym("verbose"), 0);
        class_addmethod(fexpr_tilde_class,(t_method)expr_version,
                                                        gensym("version"), 0);
        class_sethelpsymbol(fexpr_tilde_class, gensym("expr"));

}

void
expr_tilde_setup(void)
{
        expr_setup();
}

void
fexpr_tilde_setup(void)
{
        expr_setup();
}
#else /* MSP */
void
main(void)
{
        setup((t_messlist **)&expr_tilde_class, (method)Nexpr_new,
                (method)expr_ff, (short)sizeof(t_expr), 0L, A_GIMME, 0);
        addmess((method)expr_dsp, "dsp", A_CANT, 0); // dsp method
        dsp_initclass();
}
#endif


/* -- the following functions use Pd internals and so are in the "if" file. */


int
ex_getsym(char *p, t_symbol **s)
{
        *s = gensym(p);
        return (0);
}

char *
ex_symname(t_symbol *s)
{
        if (!s)
            return (0);
        return ((char *) s->s_name);
}

#if 0
SDY tabled code for when 4 point interpolation of vectors are implemented
/*
 * ex_vectablelookup - a vector table look up with 4point interpolation
 * return 0 on success
 *     wvec - pointer to the table
 *     tsize - table size
 *     iptr - ptr to the index vector
 *     optr - output pointer
 */
int
ex_vectablelookup(t_expr *e, t_word *wvec, int tsize, t_float *iptr, struct ex_ex *optr)
{
    int i;
    t_float pos, a, b, c, d, cminusb, frac;
    t_word *wp;
    int int_pos;
    int n ; /* position in the array */

    if (optr->ex_type != ET_VEC) {
        pd_error(e, "'%s': bad ooutput type", e->exp_string);
        memset((void *) optr->ex_vec, 0,
                            e->exp_vsize * sizeof (t_float));
        return (0);
    }

    if (tsize < 4) {
        memset((void *) optr->ex_vec, 0, e->exp_vsize * sizeof (t_float));
        return (0);
    }
    for (i = 0; i < e->exp_vsize; i++) {
        pos = iptr[i];
        if (pos <= 1) {
            optr->ex_vec[i] = wvec[1].w_float;
            continue;
        }
        if (pos >= tsize - 2) {
            optr->ex_vec[i] = wvec[tsize - 2].w_float;
            continue;
        }
        n = (int) pos;
        if (n >= tsize - 2)
            n = tsize - 3;
        wp = wvec + n;
        frac = pos - n;
        a = wp[-1].w_float;
        b = wp[0].w_float;
        c = wp[1].w_float;
        d = wp[2].w_float;
        cminusb = c-b;
        optr->ex_vec[i] = b + frac * ( cminusb - 0.1666667f * (1.-frac) * (
                (d - a - 3.0f * cminusb) * frac + (d + 2.0f*a - 3.0f*b)));
    }
#if 0
        /* alternative interpolation code */
        int_pos = (int)floorl(pos);
        frac_pos = pos - int_pos;

        x2  = wvec[(tsize + int_pos - 2) % tsize].w_float;
        x1  = wvec[(tsize + int_pos - 1) % tsize].w_float;
        x0  = wvec[(tsize + int_pos)     % tsize].w_float;
        xm1 = wvec[(tsize + int_pos + 1) % tsize].w_float;
        if(int_pos >= tsize)
            x1 = x0;
        if(int_pos >= tsize - 1)
            x2 = x1;
        if(int_pos <= 0)
            x0 = x1;
        if(int_pos <= 1)
            xm1 = x0;
        a = (3.0f * (x0 - x1) - xm1 + x2) * 0.5f;
        b = 2.0f * x1 + xm1 - (5.0f * x0 + x2) * 0.5f;
        c = (x1 - xm1) * 0.5f;
        optr->ex_vec[i] =
            (((((a * frac_pos) + b) * frac_pos) + c) * frac_pos) + x0;
#endif
    return(0);
}
#endif


/*
 * max_ex_tab -- evaluate this table access
 *               eptr is the name of the table and arg is the index we
 *               have to put the result in optr
 *               return 1 on error and 0 otherwise
 *
 * Arguments:
 *          expr - the expr object
 *          s - table pointer
 *          arg - pointer to the  the index argument
 *          interpol - flag to interpolate on single float index
 *          optr - the result pointer
 */
int
max_ex_tab(struct expr *expr, t_symbol *s, struct ex_ex *arg, int interpol,
    struct ex_ex *optr)
{
#ifdef PD
        t_garray *garray;
        int size;
        long indx;
        t_float flt_value = 0.0;
        t_word *wvec;

        if (!s || !(garray = (t_garray *)pd_findbyclass(s, garray_class)) ||
            !garray_getfloatwords(garray, &size, &wvec)) {
                optr->ex_type = ET_FLT;
                optr->ex_flt = 0;
                pd_error(expr, "no such table '%s'", ex_symname(s));
                return (1);
        }
#if 1

        switch (arg->ex_type) {
        case ET_INT:
                indx = arg->ex_int;
                break;
        case ET_FLT:
                /* strange interpolation code deleted here -msp */
                indx = arg->ex_flt;
                break;

        default:        /* do something with strings */
                ex_error(expr, "expr: bad argument for table '%s'\n", fts_symbol_name(s));
                if (optr->ex_type == ET_VEC)
                    ex_mkvector(optr->ex_vec, 0.0, expr->exp_vsize);
                else {
                    optr->ex_type = ET_INT;
                    optr->ex_int = 0;
                }
                return (1);
        }
        if (indx < 0)
            indx = 0;
        else if (indx >= size)
            indx = size - 1;
        flt_value = wvec[indx].w_float;
        switch (optr->ex_type) {
        case ET_VEC:
            ex_mkvector(optr->ex_vec, flt_value, expr->exp_vsize);
            return(0);
        case ET_SYM:
            ex_error(expr, "expr:'%s' bad output type  '%ld'\n",
                              expr->exp_string, optr->ex_type);
            break;
        case ET_INT:
        case ET_FLT:
        default:
            optr->ex_type = ET_FLT;
            optr->ex_flt = flt_value;
            return(0);
        }
        optr->ex_type = ET_INT;
        optr->ex_int = 0;
        return(1);
#else /*  if 1 tabled code */
 The code below implement 4 point interpolation of table access
 which for now I (sdy) am tabling for the next release
        long n;
        float flt_value;
        t_float pos, a, b, c, d, cminusb, frac;
        t_word *wp;

        switch (arg->ex_type) {
        case ET_INT:
        case ET_FLT:
                if (arg->ex_type == ET_INT) {
                    n = arg->ex_int;
                    n = MAX(0,  MIN(n, size - 1));
                    flt_value = wvec[n].w_float;
                } else if (!interpol) {
                    /*
                     * for compatibility purposes with older versions we
                     * do not interpolate the table lookup with float
                     * values, but it will be done for signal indexes,
                     * and in the tablelook up function
                     */
                    n = arg->ex_flt;
                    n = MAX(0,  MIN(n, size - 1));
                    flt_value = wvec[n].w_float;
                } else { /* interpolate */
                    pos = arg->ex_flt;
                    if (size < 4) {
                        flt_value = 0;
                    } else if (pos <= 1) {
                        flt_value = wvec[1].w_float;
                    } else if (pos >= size - 2) {
                        flt_value = wvec[size - 2].w_float;
                    } else {
                        n = (int) pos;
                        if (n >= size - 2)
                            n = size - 3;
                        wp = wvec + n;
                        frac = pos - n;
                        a = wp[-1].w_float;
                        b = wp[0].w_float;
                        c = wp[1].w_float;
                        d = wp[2].w_float;
                        cminusb = c-b;
                        flt_value = b + frac *
                            ( cminusb - 0.1666667f * (1.-frac) * (
                            (d - a - 3.0f * cminusb) * frac +
                            (d + 2.0f*a - 3.0f*b)));
                    }
                }

                switch (optr->ex_type) {
                case ET_VEC:
                        ex_mkvector(optr->ex_vec, flt_value,
                                        expr->exp_vsize);
                        return(0);
                case ET_SYM:
                        pd_error(expr, "expr:'%s' bad output type  '%ld'\n",
                                      expr->exp_string, optr->ex_type);
                        break;
                case ET_INT:
                case ET_FLT:
                default:
                        optr->ex_type = ET_FLT;
                        optr->ex_flt = flt_value;
                        return(0);
                }
                break;
        case ET_VI:
                if (optr->ex_type != ET_VEC) {
                        pd_error(expr,
                "expr:'%s': bad table arg - cannot convert vector to scalar\n",
                                                expr->exp_string);
                        break;
                }
                return(ex_vectablelookup(expr, wvec, size, arg->ex_vec, optr));

        case ET_VEC:
                if (optr->ex_type != ET_VEC) {
                    pd_error(expr,
                "expr:'%s': bad table arg - cannot convert vector to scalar\n",
                                                 expr->exp_string);
                    break;
                }
                return (ex_vectablelookup(expr, wvec, size,
                                            arg->ex_vec, optr));
        default:        /* do something with strings */
                pd_error(expr,
                      "expr:'%s' bad argument for table '%s'\n",
                           expr->exp_string, s->s_name);
        }
        optr->ex_type = ET_INT;
        optr->ex_int = 0;
        return(1);
#endif

#else /* MSP */
        /*
         * table lookup not done for MSP yet
         */
        post("max_ex_tab: not complete for MSP yet!");
        optr->ex_type = ET_FLT;
        optr->ex_flt = 0;
#endif
        return (0);
}

/*
 * max_ex_tab_store -- store a value in a table
 *      tbl[arg->value] = rval.value
 *      eptr is the name of the table and arg is the index we
 *      have to put the result in optr
 *      return 1 on error and 0 otherwise
 *
 * Arguments:
 *  the expr object
 *  table
 *  the argument
 *  value to be stored
 *  the result pointer
 */
int
max_ex_tab_store(struct expr *expr, t_symbol *s, struct ex_ex *arg,
                                        struct ex_ex *rval, struct ex_ex *optr)
{
#ifdef PD
        t_garray *garray;
        int size;
        long indx;
        t_word *wvec;

        if (!s || !(garray = (t_garray *)pd_findbyclass(s, garray_class)) ||
                !garray_getfloatwords(garray, &size, &wvec)) {
                optr->ex_type = ET_FLT;
                optr->ex_flt = 0;
                if (s)
                    pd_error(expr, "%s: no such table to store '%s'",
                                    expr->exp_string, s->s_name);
                else
                    pd_error(expr, "%s: cannot store in unnamed table",
                                                            expr->exp_string);
                return (1);
        }
        optr->ex_type = ET_FLT;

        switch (arg->ex_type) {
        case ET_INT:
                indx = arg->ex_int;
                break;
        case ET_FLT:
                /* strange interpolation code deleted here -msp */
                indx = arg->ex_flt;
                break;

        default:        /* do something with strings */
                pd_error(expr, "expr: bad argument for table store '%s'\n",
                        s->s_name);
                indx = 0;
        }
        if (indx < 0)
                indx = 0;
        else if (indx >= size)
                indx = size - 1;
        *optr = *rval;
        switch (rval->ex_type) {
        case ET_INT:
                wvec[indx].w_float = rval->ex_int;
                break;
        case ET_FLT:
                wvec[indx].w_float = rval->ex_flt;
                break;
        default:
                pd_error(expr, "expr:bad right value type '%ld'", rval->ex_type);
                optr->ex_type = ET_FLT;
                optr->ex_flt = 0;
                return (1);
        }
        garray_redraw(garray);
        return(0);

#else /* MSP */
        /*
         * table lookup not done for MSP yet
         */
        post("max_ex_tab: not complete for MSP yet!");
        optr->ex_type = ET_FLT;
        optr->ex_flt = 0;
#endif
        return (0);
}

int
max_ex_var(struct expr *expr, t_symbol *var, struct ex_ex *optr, int idx)
{
        t_float value;
        if (!strcmp(var->s_name, "sys_idx")) {
                if (optr->ex_type == ET_VEC)
                    ex_mkvector(optr->ex_vec, (t_float) idx, expr->exp_vsize);
                else {
                    optr->ex_int = idx;
                    optr->ex_type = ET_INT;
                }
                return (0);
        }
        if (value_getfloat(var, &value)) {
                if (!(expr->exp_error & EE_NOVAR)) {
                        ex_error(expr, "no such var '%s'", var->s_name);
                    if (!IS_EXPR(expr)) {
                        post("expr: no more var errors will be reported");
                        post("expr: till the next reset");
                        expr->exp_error |= EE_NOVAR;
                    }
                }

                if (optr->ex_type == ET_VEC)
                    ex_mkvector(optr->ex_vec, 0.0, expr->exp_vsize);
                else {
                    optr->ex_int = 0;
                    optr->ex_type = ET_INT;
                }
                return (1);
        }
        if (optr->ex_type == ET_VEC)
            ex_mkvector(optr->ex_vec, value, expr->exp_vsize);
        else {
            optr->ex_flt = value;
            optr->ex_type = ET_FLT;
        }
        return (0);
}

#ifdef PD /* this goes to the end of this file as the following functions
           * should be defined in the expr object in MSP
           */
#define ISTABLE(e, sym, garray, size, vec)                              \
        switch  (argv->ex_type) {                                       \
                case ET_SYM:                                            \
                    if (argv->ex_flags & EX_F_TSYM) {                   \
                        /* SDY the symbol has to be freed too */        \
                        s = gensym(argv->ex_ptr);                       \
                        argv->ex_flags &= ~EX_F_TSYM;                   \
                        free(argv->ex_ptr);                             \
                     } else                                             \
                        s = (t_symbol *) argv->ex_ptr;                  \
                    break;                                              \
                case ET_SI:                                             \
                    s = (t_symbol *) e->exp_var[argv->ex_int].ex_ptr;   \
                    break;                                              \
                default:                                                \
                    post(                                               \
                "expr:'%s' arg to function needs to be a table name\n", \
                                                e->exp_string);         \
                    optr->ex_type = ET_INT;                             \
                    optr->ex_int = 0;                                   \
                    return;                                             \
        }                                                               \
        if (!sym || !(garray =                                          \
                (t_garray *)pd_findbyclass(sym, garray_class)) ||       \
                !garray_getfloatwords(garray, &size, &vec))  {          \
                optr->ex_type = ET_FLT;                                 \
                optr->ex_int = 0;                                       \
                pd_error(0, "%s: no such table '%s'",                   \
                        e->exp_string, sym?(sym->s_name):"(null)");     \
                return;                                                 \
        }

/*
 * ex_size -- find the size of a table
 */
void
ex_size(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        t_symbol *s;
        t_garray *garray;
        int size;
        t_word *wvec;

        ISTABLE(e, s, garray, size, wvec);

        if (optr->ex_type == ET_VEC)
                ex_mkvector(optr->ex_vec, (t_float) size, e->exp_vsize);
        else  {
                optr->ex_type = ET_INT;
                optr->ex_int = size;
        }
}

/*
 * ex_sum -- calculate the sum of all elements of a table
 */

void
ex_sum(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        t_symbol *s;
        t_garray *garray;
        int size;
        t_word *wvec;
        t_float sum;
        int indx;

        ISTABLE(e, s, garray, size, wvec);

        for (indx = 0, sum = 0; indx < size; indx++)
                sum += wvec[indx].w_float;

        if (optr->ex_type == ET_VEC)
                ex_mkvector(optr->ex_vec, (t_float) size, e->exp_vsize);
        else  {
                optr->ex_type = ET_FLT;
                optr->ex_flt = sum;
        }
}


/*
 * ex_Sum -- calculate the sum of table with the given boundaries
 */

void
ex_Sum(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        t_symbol *s;
        t_garray *garray;
        int size;
        t_word *wvec;
        t_float sum;
        long indx, n1, n2;

        ISTABLE(e, s, garray, size, wvec);

        switch((++argv)->ex_type) {
        case ET_INT:
            n1 = argv->ex_int;
            break;
        case ET_FLT:
            n1 = argv->ex_flt;
            break;
        default:
            ex_error(e, "expr: Sum: boundaries have to be fix values\n");
            if (optr->ex_type == ET_VEC)
                ex_mkvector(optr->ex_vec, 0.0, e->exp_vsize);
            else {
                optr->ex_type = ET_INT;
                optr->ex_int = 0;
            }
            return;
        }
        if (n1 < 0)
            n1 = 0;

        switch((++argv)->ex_type) {
        case ET_INT:
            n2 = argv->ex_int;
            break;
        case ET_FLT:
            n2 = argv->ex_flt;
            break;
        default:
            ex_error(e, "expr: Sum: boundaries have to be fix values\n");
            if (optr->ex_type == ET_VEC)
                ex_mkvector(optr->ex_vec, 0.0, e->exp_vsize);
            else {
                optr->ex_type = ET_INT;
                optr->ex_int = 0;
            }
            return;
        }
        if (n2 > size)
            n2 = size;

        for (indx = n1, sum = 0; indx <= n2; indx++)
            if (indx >= 0 && indx < size)
                sum += wvec[indx].w_float;

        if (optr->ex_type == ET_VEC)
            ex_mkvector(optr->ex_vec, sum, e->exp_vsize);
        else {
            optr->ex_type = ET_FLT;
            optr->ex_flt = sum;
        }
}

/*
 * ex_avg -- calculate the average of a table
 */

void
ex_avg(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        t_symbol *s;
        t_garray *garray;
        int size;
        t_word *wvec;
        t_float sum;
        int indx;

        ISTABLE(e, s, garray, size, wvec);

        for (indx = 0, sum = 0; indx < size; indx++)
                sum += wvec[indx].w_float;

        if (optr->ex_type == ET_VEC)
            ex_mkvector(optr->ex_vec, sum / size, e->exp_vsize);
        else {
            optr->ex_type = ET_FLT;
            optr->ex_flt = sum / size;
        }
}


/*
 * ex_Avg -- calculate the average of table with the given boundaries
 */

void
ex_Avg(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
        t_symbol *s;
        t_garray *garray;
        int size;
        t_word *wvec;
        t_float sum;
        long indx, n1, n2;

        ISTABLE(e, s, garray, size, wvec);

        switch((++argv)->ex_type) {
        case ET_INT:
                n1 = argv->ex_int;
                break;
        case ET_FLT:
                n1 = argv->ex_flt;
                        break;
        default:
                ex_error(e, "expr: Avg: boundaries have to be fix values\n");
                if (optr->ex_type == ET_VEC)
                    ex_mkvector(optr->ex_vec, 0.0, e->exp_vsize);
                else {
                    optr->ex_type = ET_INT;
                    optr->ex_int = 0;
                }
                return;
        }
        if (n1 < 0)
                n1 = 0;

        switch((++argv)->ex_type) {
        case ET_INT:
                n2 = argv->ex_int;
                        break;
        case ET_FLT:
                n2 = argv->ex_flt;
                        break;
        default:
                ex_error(e, "expr: Avg: boundaries have to be fix values\n");
                if (optr->ex_type == ET_VEC)
                    ex_mkvector(optr->ex_vec, 0.0, e->exp_vsize);
                else {
                    optr->ex_type = ET_INT;
                    optr->ex_int = 0;
                }
                return;
        }
        if (n2 >= size)
                n2 = size - 1;

        for (indx = n1, sum = 0; indx <= n2; indx++)
                if (indx >= 0 && indx < size)
                        sum += wvec[indx].w_float;

        if (optr->ex_type == ET_VEC)
            ex_mkvector(optr->ex_vec, sum / (n2 - n1 + 1), e->exp_vsize);
        else {
            optr->ex_type = ET_FLT;
            optr->ex_flt = sum / (n2 - n1 + 1);
        }
}

/*
 * max_ex_store --- store a value in a variable or table
 */
int
max_ex_var_store(struct expr *expr, t_symbol * var, struct ex_ex *eptr, struct ex_ex *optr)
{
        t_float value = 0.;

        switch (eptr->ex_type) {
        case ET_INT:
                value = eptr->ex_int;
                break;
        case ET_FLT:
                value = eptr->ex_flt;
                break;
        default:
                ex_error(expr, "bad right value - '%s'\n", expr->exp_string);
                if (optr->ex_type == ET_VEC)
                    ex_mkvector(optr->ex_vec, 0.0, expr->exp_vsize);
                else {
                    optr->ex_int = 0;
                    optr->ex_type = ET_INT;
                }
                return (1);
        }

        if (value_setfloat(var, value)) {
                ex_error(expr, "no such var '%s'", var->s_name);
                if (optr->ex_type == ET_VEC)
                    ex_mkvector(optr->ex_vec, 0.0, expr->exp_vsize);
                else {
                    optr->ex_type = ET_INT;
                    optr->ex_int = 0;
                }
                return (1);
        }
        if (optr->ex_type == ET_VEC)
            ex_mkvector(optr->ex_vec, value, expr->exp_vsize);
        else
            *optr = *eptr;
        return (0);
}

/*
 * UNUSED FUNCTION
 *
 * ex_store -- store a value in a table
 *             if the index is greater the size of the table,
 *             we will make a modulo the size of the table
 */

void
ex_store(t_expr *e, long int argc, struct ex_ex *argv, struct ex_ex *optr)
{
/* SDY - look into this function */
#if 0
        t_symbol *s;
        fts_integer_vector_t *tw = 0;

        if (argv->ex_type != ET_SYM)
        {
                post("expr: store: need a table name\n");
        }

        s = (t_symbol *) (argv++)->ex_ptr;

        tw = table_integer_vector_get_by_name(s);

        if (! tw)
        {
                optr->ex_type = ET_INT;
                optr->ex_int = 0;
                post("expr: store: no such table %s\n", s->s_name);
                return;
        }

        if (argv->ex_type != ET_INT || argv[1].ex_type != ET_INT)
        {
                post("expr: store: arguments have to be integer\n");
                optr->ex_type = ET_INT;
                optr->ex_int = 0;
        }

        fts_integer_vector_set_element(tw, argv->ex_int < 0 ? 0 : argv->ex_int % fts_integer_vector_get_size(tw), argv[1].ex_int);
        *optr = argv[1];
#endif
}

#else /* MSP */

void
pd_error(void *object, char *fmt, ...)
{
    va_list ap;
    t_int arg[8];
    int i;
    static int saidit = 0;
    va_start(ap, fmt);
/* SDY
    vsprintf(error_string, fmt, ap);
*/ post(fmt, ap);
        va_end(ap);
/* SDY
    fprintf(stderr, "error: %s\n", error_string);
    error_object = object;
*/
    if (!saidit)
    {
        post("... you might be able to track this down from the Find menu.");
        saidit = 1;
    }
}
#endif
