/* Copyright (c) IRCAM.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

/* "expr" was written by Shahrokh Yadegari c. 1989. -msp */
/* "expr~" and "fexpr~" conversion by Shahrokh Yadegari c. 1999,2000 */


/*
 *
 * July 2024, Version 0.58 (major changes)
 *      - Added string functions in expr; now expr can output symbols
 *      - fixed the expr~ array[0] bug
 *      - Provide better error messages;
 *        now expr prints the expr string when reporting errors
 *      - fixed a memory issue were extra unused vector output was
 *        allocated for each inlet even though the were not used
 *      - cleaned up some indentation issues
 *
 * Oct 2020, Version 0.57
 *  - fixed a bug in fact()
 *  - fixed the bad lvalue bug - "4 + 5 = 3" was not caught before
 *  - fact() (factorial) now calculates and returns its value in double
 *  - Added mtof(), mtof(), dbtorms(), rmstodb(), powtodb(), dbtopow()
 *
 *  July 2017,  Version 0.55
 *      - The arrays now redraw after a store into one of their members
 *      - ex_if() (the "if()" function is reworked to only evaluate
 *        either the left or the right args depending on the truth
 *        value of the condition. However, if the condition is a
 *        vector, both the left and the right are evaluated regardless.
 *      - priority of ',' and '=' was switched to fix the bug of using
 *        store "=" in functions with multiple arguments, which caused
 *        an error during execution.
 *      - The number of inlet and outlets (EX_MAX_INLETS) is now set at 100
 *
 * Oct 2015
 *       $x[-1] was not equal $x1[-1], not accessing the previous block
 *      (bug fix by Dan Ellis)
 *
 * July 2002
 *      fixed bugs introduced in last changes in store and ET_EQ
 *      --sdy
 *
 *  Jan 2018, Version 0.56
 *      -fexpr~ now accepts a float in its first input
 *      -Added avg() and Avg() back to the list of functions
 *
 * Feb 2002
 *      added access to variables
 *      multiple expression support
 *      new short hand forms for fexpr~
 *      now $y or $y1 = $y1[-1] and $y2 = $y2[-1]
 *      --sdy
 *
 */

/*
 * vexp.c -- a variable expression evaluator
 *
 * This modules implements an expression evaluator using the
 * operator-precedence parsing.  It transforms an infix expression
 * to a prefix stack ready to be evaluated.  The expression sysntax
 * is close to that of C.  There are a few operators that are not
 * supported and functions are also recognized.  Strings can be
 * passed to functions when they are quoted in '"'s. "[]" are implemented
 * as an easy way of accessing the content of tables, and the syntax
 * table_name[index].
 * Variables (inlets) are specified with the following syntax: $x#,
 * where x is either i(integers), f(floats), and s(strings); and #
 * is a digit that corresponds to the inlet number.  The string variables
 * can be used as strings when they are quoted and can also be used as
 * table names when they are followed by "[]".
 *
 * signal vectors have been added to this implementation:
 * $v# denotes a signal vector
 * $x#[index]  is the value of a sample at the index of a the signal vector
 * $x# is the shorthand for $x#[0]
 * $y[index]   is the value of the sample output at the index of a the
 *             signal output
 * "index" for $x#[index] has to have this range (0 <= index < vectorsize)
 * "index" for $y[index]  has to have this range (0 <  index < vectorsize)
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "x_vexp.h"
#include <errno.h>
#ifdef MSP
#undef isdigit
#define isdigit(x)      (x >= '0' && x <= '9')
#endif

#if defined _MSC_VER && (_MSC_VER < 1800)
#define strtof(a, b) _atoldbl(a, *b)
#endif


char *atoif(char *s, long int *value, long int *type);

static struct ex_ex *ex_lex(struct expr *expr, long int *n);
struct ex_ex *ex_match(struct ex_ex *eptr, long int op);
struct ex_ex *ex_parse(struct expr *expr, struct ex_ex *iptr,
                                        struct ex_ex *optr, long int *argc);
static int ex_checklval (struct expr *e, struct ex_ex *eptr);
struct ex_ex *ex_eval(struct expr *expr, struct ex_ex *eptr,
                                                struct ex_ex *optr, int i);

int expr_donew(struct expr *exprr, int ac, t_atom *av);
struct ex_ex *eval_func(struct expr *expr,struct ex_ex *eptr,
                                                struct ex_ex *optr, int i);
struct ex_ex *eval_tab(struct expr *expr, struct ex_ex *eptr,
                                                struct ex_ex *optr, int i);
struct ex_ex *eval_var(struct expr *expr, struct ex_ex *eptr,
                                                struct ex_ex *optr, int i);
struct ex_ex *eval_store(struct expr *expr, struct ex_ex *eptr,
                                                struct ex_ex *optr, int i);
struct ex_ex *eval_sigidx(struct expr *expr, struct ex_ex *eptr,
                                                struct ex_ex *optr, int i);
static int cal_sigidx(struct ex_ex *optr,       /* The output value */
           int i, t_float rem_i,      /* integer and fractinal part of index */
           int idx,                  /* index of current fexpr~ processing */
           int vsize,                                       /* vector size */
           t_float *curvec, t_float *prevec);   /* current and previous table */
t_ex_func *find_func(char *s);
void ex_dzdetect(struct expr *expr);

#define MAX_ARGS        10
extern t_ex_func ex_funcs[];

struct ex_ex nullex = { 0 };

void set_tokens (char *s);
int getoken (struct expr *expr, struct ex_ex *eptr);
void ex_print (struct ex_ex *eptr);
#ifdef MSP
void atom_string(t_atom *a, char *buf, unsigned int bufsize);

void atom_string(t_atom *a, char *buf, unsigned int bufsize)
{
    char tbuf[30];
    switch(a->a_type)
    {
    case A_SEMI: strcpy(buf, ";"); break;
    case A_COMMA: strcpy(buf, ","); break;
#ifdef PD
    case A_POINTER:
        strcpy(buf, "(pointer)");
        break;
#endif
    case A_FLOAT:
        sprintf(tbuf, "%g", a->a_w.w_float);
        if (strlen(tbuf) < bufsize-1) strcpy(buf, tbuf);
        else if (a->a_w.w_float < 0) strcpy(buf, "-");
        else  strcat(buf, "+");
        break;
    case A_LONG:
        sprintf(tbuf, "%d", a->a_w.w_long);
        if (strlen(tbuf) < bufsize-1) strcpy(buf, tbuf);
        else if (a->a_w.w_float < 0) strcpy(buf, "-");
        else  strcat(buf, "+");
        break;
    case A_SYMBOL:
    {
        char *sp;
        unsigned int len;
        int quote;
        for (sp = a->a_w.w_symbol->s_name, len = 0, quote = 0; *sp; sp++, len++)
            if (*sp == ';' || *sp == ',' || *sp == '\\' ||
                (*sp == '$' && sp == a->a_w.w_symbol->s_name && sp[1] >= '0'
                    && sp[1] <= '9'))
                quote = 1;
        if (quote)
        {
            char *bp = buf, *ep = buf + (bufsize-2);
            sp = a->a_w.w_symbol->s_name;
            while (bp < ep && *sp)
            {
                if (*sp == ';' || *sp == ',' || *sp == '\\' ||
                    (*sp == '$' && bp == buf && sp[1] >= '0' && sp[1] <= '9'))
                        *bp++ = '\\';
                *bp++ = *sp++;
            }
            if (*sp) *bp++ = '*';
            *bp = 0;
            /* post("quote %s -> %s", a->a_w.w_symbol->s_name, buf); */
        }
        else
        {
            if (len < bufsize-1) strcpy(buf, a->a_w.w_symbol->s_name);
            else
            {
                strncpy(buf, a->a_w.w_symbol->s_name, bufsize - 2);
                strcpy(buf + (bufsize - 2), "*");
            }
        }
    }
        break;
#ifdef PD
    case A_DOLLAR:
        sprintf(buf, "$%d", a->a_w.w_index);
        break;
    case A_DOLLSYM:
        sprintf(buf, "$%s", a->a_w.w_symbol->s_name);
                break;
#else /* MAX */
 case A_DOLLAR:
        sprintf(buf, "$%s", a->a_w.w_symbol->s_name);
        break;
#endif
    default:
        post("atom_string bug");
    }
}
#endif /* MSP */
/*
 * expr_donew -- create a new "expr" object.
 *           returns 1 on failure, 0 on success.
 */
int
expr_donew(struct expr *expr, int ac, t_atom *av)
{
        struct ex_ex *list;
        struct ex_ex *ret;
        long max_node = 0;              /* maximum number of nodes needed */
        char *exp_string;
        int exp_strlen;
        t_binbuf *b;
        int i;

        memset(expr->exp_var, 0, EX_MAX_INLETS * sizeof (*expr->exp_var));
#ifdef PD
        b = binbuf_new();
        binbuf_add(b, ac, av);
        binbuf_gettext(b, &exp_string, &exp_strlen);
        binbuf_free(b);
#else /* MSP */
 {
    char *buf = getbytes(0), *newbuf;
    int length = 0;
    char string[250];
    t_atom *ap;
    int indx;

    for (ap = av, indx = 0; indx < ac; indx++, ap = ++av) {
        int newlength;

        if ((ap->a_type == A_SEMI || ap->a_type == A_COMMA) &&
                length && buf[length-1] == ' ')
                        length--;
        atom_string(ap, string, 250);
        newlength = length + strlen(string) + 1;
        if (!(newbuf = t_resizebytes(buf, length, newlength)))
                                break;
        buf = newbuf;
        strcpy(buf + length, string);
        length = newlength;
        if (ap->a_type == A_SEMI)
                buf[length-1] = '\n';
        else
                buf[length-1] = ' ';
    }

    if (length && buf[length-1] == ' ') {
        if (newbuf = t_resizebytes(buf, length, length-1))
        {
            buf = newbuf;
            length--;
        }
    }
    exp_string = buf;
    exp_strlen  = length;
 }
#endif
        exp_string = (char *)t_resizebytes(exp_string, exp_strlen,exp_strlen+1);
        exp_string[exp_strlen] = 0;
        expr->exp_string = exp_string;
        expr->exp_str = exp_string;
        expr->exp_nexpr = 0;
        ret = (struct ex_ex *) 0;
        /*
         * if ret == 0 it means that we have no expression
         * so we let the pass go through to build a single null stack
         */
        while (*expr->exp_str || !ret) {
                list = ex_lex(expr, &max_node);
                if (!list) {            /* syntax error */
                        goto error;
                }
                expr->exp_stack[expr->exp_nexpr] =
                  (struct ex_ex *)fts_malloc(max_node * sizeof (struct ex_ex));
                if (!expr->exp_stack[expr->exp_nexpr]) {
                        post_error( (fts_object_t *) expr,
                                "expr: malloc for expr nodes failed\n");
                        goto error;
                }
                expr->exp_stack[expr->exp_nexpr][max_node-1].ex_type=0;
                expr->exp_nexpr++;
                ret = ex_match(list, (long)0);
                if (expr->exp_nexpr  > EX_MAX_INLETS)
                    /* we cannot exceed EX_MAX_INLETS '$' variables */
                {
                        post_error((fts_object_t *) expr,
                            "expr: too many variables (maximum %d allowed)",
                                EX_MAX_INLETS);
                        goto error;
                }
                if (!ret)               /* syntax error */
                        goto error;
                ret = ex_parse(expr,
                        list, expr->exp_stack[expr->exp_nexpr - 1], (long *)0);
                if (!ret || ex_checklval(expr, expr->exp_stack[expr->exp_nexpr - 1]))
                        goto error;
                fts_free(list);
        }
        *ret = nullex;
        return (0);
error:
        for (i = 0; i < expr->exp_nexpr; i++) {
                fts_free(expr->exp_stack[i]);
                expr->exp_stack[i] = 0;
        }
        expr->exp_nexpr = 0;
        if (list)
                fts_free(list);
        t_freebytes(exp_string, exp_strlen+1);
        return (1);
}


void *
ex_calloc(size_t count, size_t size)
{
    //printf ("calloc called with %zu bytes\n", size);
    return calloc(count, size);
}

void
ex_free(void *ptr)
{
    free (ptr);
}


void *
ex_malloc(size_t size)
{
    //printf ("malloc called with %zu bytes\n", size);
    return malloc(size);
}

void *
ex_realloc(void *ptr, size_t size)
{
    printf ("realloc called with %zu bytes\n", size);
    return realloc(ptr, size);
}

/*
 * ex_lex -- This routine is a bit more than a lexical parser since it will
 *           also do some syntax checking.  It reads the string s and will
 *           return a linked list of struct ex_ex.
 *           It will also put the number of the nodes in *n.
 */
struct ex_ex *
ex_lex(struct expr *expr, long int *n)
{
        struct ex_ex *list_arr;
        struct ex_ex *exptr, *p;
        static struct ex_ex tmpnodes[EX_MINODES];
        long non = 0;           /* number of nodes */
        long maxnode = 0;
        int reallocated = 0;      /* did we reallocate the tmp buffer */

        memset ((void *) tmpnodes, 0, sizeof(struct ex_ex) * EX_MINODES);

        list_arr = tmpnodes;
        maxnode = EX_MINODES;

        while (1)
        {
                if (non >= maxnode) {
                        maxnode += EX_MINODES;

                        if (!reallocated) {
                                list_arr = fts_calloc(maxnode, sizeof (struct ex_ex));
                                memcpy (list_arr, tmpnodes, sizeof (struct ex_ex) * EX_MINODES);
                                if (!list_arr) {
                                        ex_error (expr, "ex_lex: no memory\n");
                                        return ((struct ex_ex *)0);
                                }
                                reallocated = 1;
                        } else {
                                p = fts_realloc((void *) list_arr,
                                        sizeof (struct ex_ex) * maxnode);
                                if (!p) {
                                        fts_free (list_arr);
                                        ex_error (expr, "ex_lex: no memory\n");
                                        return ((struct ex_ex *)0);
                                }
                                list_arr = p;
                        }
                }

                exptr = &list_arr[non];
                if (getoken(expr, exptr)) {
                        if (reallocated)
                                fts_free(exptr);
                        return ((struct ex_ex *)0);
                }
                non++;

                if (!exptr->ex_type)
                        break;
        }
        p = fts_calloc(non, sizeof (struct ex_ex));
        if (!p) {
                if (reallocated)
                        fts_free(list_arr);
                ex_error (expr, "ex_lex: no memory\n");
                return ((struct ex_ex *)0);
        }
        memcpy(p, list_arr, sizeof (struct ex_ex) * non);
        *n = non;
        if (reallocated)
                fts_free(list_arr);

        return (p);
}

/*
 * ex_match -- this routine walks through the eptr and matches the
 *             parentheses and brackets, it also converts the function
 *             names to a pointer to the describing structure of the
 *             specified function
 */
/* operator to match */
struct ex_ex *
ex_match(struct ex_ex *eptr, long int op)
{
        int firstone = 1;
        struct ex_ex *ret;
        t_ex_func *fun;

        for (; 8; eptr++, firstone = 0) {
                switch (eptr->ex_type) {
                case 0:
                        if (!op)
                                return (eptr);
                        post("expr syntax error: an open %s not matched\n",
                            op == OP_RP ? "parenthesis" : "bracket");
                        return (exNULL);
                case ET_INT:
                case ET_FLT:
                case ET_II:
                case ET_FI:
                case ET_SI:
                case ET_VI:
                case ET_SYM:
                case ET_VSYM:
                        continue;
                case ET_YO:
                        if (eptr[1].ex_type != ET_OP || eptr[1].ex_op != OP_LB)
                                eptr->ex_type = ET_YOM1;
                        continue;
                case ET_XI:
                        if (eptr[1].ex_type != ET_OP || eptr[1].ex_op != OP_LB)
                                eptr->ex_type = ET_XI0;
                        continue;
                /*
                 * tables, functions, parenthesis, and brackets, are marked as
                 * operations, and they are assigned their proper operation
                 * in this function. Thus, if we arrive to any of these in this
                 * type tokens at this location, we must have had some error
                 */
                case ET_TBL:
                case ET_FUNC:
                case ET_LP:
                case ET_LB:
                        post("ex_match: unexpected type, %ld\n", eptr->ex_type);
                        return (exNULL);
                case ET_OP:
                        if (op == eptr->ex_op)
                                return (eptr);
                        /*
                         * if we are looking for a right peranthesis
                         * or a right bracket and find the other kind,
                         * it has to be a syntax error
                         */
                        if ((eptr->ex_op == OP_RP && op == OP_RB) ||
                            (eptr->ex_op == OP_RB && op == OP_RP)) {
                                post("expr syntax error: prenthesis or brackets not matched\n");
                                return (exNULL);
                        }
                        /*
                         * Up to now we have marked the unary minuses as
                         * subrtacts.  Any minus that is the first one in
                         * chain or is preceded by anything except ')' and
                         * ']' is a unary minus.
                         */
                        if (eptr->ex_op == OP_SUB) {
                                ret = eptr - 1;
                                if (firstone ||  (ret->ex_type == ET_OP &&
                                    ret->ex_op != OP_RB && ret->ex_op != OP_RP))
                                        eptr->ex_op = OP_UMINUS;
                        } else if (eptr->ex_op == OP_LP) {
                                ret = ex_match(eptr + 1, OP_RP);
                                if (!ret)
                                        return (ret);
                                eptr->ex_type = ET_LP;
                                eptr->ex_ptr = (char *) ret;
                                eptr = ret;
                        } else if (eptr->ex_op == OP_LB) {
                                ret = ex_match(eptr + 1, OP_RB);
                                if (!ret)
                                        return (ret);
                                /*
                                 * this is a special case handling
                                 * for $1, $2 processing in Pd
                                 *
                                 * Pure data translates $#[x] (e.g. $1[x]) to 0[x]
                                 * for abstracting patches so that later
                                 * in the instantiation of the abstraction
                                 * the $# is replaced with the proper argument
                                 * of the abstraction
                                 * so we change 0[x] to a special table pointing to null
                                 * and catch errors in execution time
                                 */
                                if (!firstone && (eptr - 1)->ex_type == ET_INT &&
                                    ((eptr - 1)->ex_int == 0)) {
                                        (eptr - 1)->ex_type = ET_TBL;
                                        (eptr - 1)->ex_ptr = (char *)0;
                                }
                                eptr->ex_type = ET_LB;
                                eptr->ex_ptr = (char *) ret;
                                eptr = ret;
                        }
                        continue;
                case ET_STR:
                        if (eptr[1].ex_op == OP_LB) {
                                char *tmp;

                                eptr->ex_type = ET_TBL;
                                tmp = eptr->ex_ptr;
                                if (ex_getsym(tmp, (t_symbol **)&(eptr->ex_ptr))) {
                                        post("expr: syntax error: problem with ex_getsym\n");
                                        return (exNULL);
                                }
                                fts_free((void *)tmp);
                        } else if (eptr[1].ex_op == OP_LP) {
                                fun = find_func(eptr->ex_ptr);
                                if (!fun) {
                                        post(
                                         "expr: error: function %s not found\n",
                                                                eptr->ex_ptr);
                                        return (exNULL);
                                }
                                eptr->ex_type = ET_FUNC;
                                eptr->ex_ptr = (char *) fun;
                        } else {
                                char *tmp;

                                if (eptr[1].ex_type && eptr[1].ex_type!=ET_OP){
                                        post("expr: syntax error: bad string '%s'\n", eptr->ex_ptr);
                                        return (exNULL);
                                }
                                /* it is a variable */
                                eptr->ex_type = ET_VAR;
                                tmp = eptr->ex_ptr;
                                if (ex_getsym(tmp,
                                                (t_symbol **)&(eptr->ex_ptr))) {
                                        post("expr: variable '%s' not found",tmp);
                                        return (exNULL);
                                }
                        }
                        continue;
                default:
                        post("ex_match: bad type\n");
                        return (exNULL);
                }
        }
        /* NOTREACHED */
}

/*
 * ex_parse -- This function is called when we have already done some
 *             parsing on the expression, and we have already matched
 *             our brackets and parenthesis.  The main job of this
 *             function is to convert the infix expression to the
 *             prefix form.
 *             First we find the operator with the lowest precedence and
 *             put it on the stack ('optr', it is really just an array), then
 *             we call ourself (ex_parse()), on its arguments (unary operators
 *             only have one operator.)
 *             When "argc" is set it means that we are parsing the arguments
 *             of a function and we will increment *argc anytime we find
 *             a segment that can qualify as an argument (counting commas).
 *
 *             returns 0 on syntax error
 */
/* number of argument separated by comma */
struct ex_ex *
ex_parse(struct expr *x, struct ex_ex *iptr, struct ex_ex *optr, long int *argc)
{
        struct ex_ex *eptr;
        struct ex_ex *lowpre = 0;       /* pointer to the lowest precedence */
        struct ex_ex savex = { 0 };
        struct ex_ex *tmpex;
        long pre = HI_PRE;
        long count;
        t_ex_func *f;                   /* function pointer */

        if (!iptr) {
                post("ex_parse: input is null, iptr = 0x%lx\n", iptr);
                return (exNULL);
        }
        if (!iptr->ex_type)
                return (exNULL);

        /*
         * the following loop finds the lowest precedence operator in the
         * the input token list, comma is explicitly checked here since
         * that is a special operator and is only legal in functions
         */
        for (eptr = iptr, count = 0; eptr->ex_type; eptr++, count++) {
                switch (eptr->ex_type) {
                case ET_SYM:
                case ET_VSYM:
                case ET_INT:
                case ET_FLT:
                case ET_II:
                case ET_FI:
                case ET_XI0:
                case ET_YOM1:
                case ET_VI:
                case ET_VAR:
                        if (!count && !eptr[1].ex_type) {
                                *optr = *eptr;
                                tmpex = optr;
                                tmpex->ex_end = ++optr;
                                return (optr);
                        }
                        break;
                case ET_SI:
                        if (eptr[1].ex_type == ET_LB) {
                                eptr->ex_flags |= EX_F_SI_TAB;
                                goto processtable;
                        }
                        if (!count && !eptr[1].ex_type) {
                                eptr->ex_flags &= ~EX_F_SI_TAB;
                                *optr = *eptr;
                                tmpex = optr;
                                tmpex->ex_end = ++optr;
                                return (optr);
                        }
                        break;
                case ET_XI:
                case ET_YO:
                case ET_TBL:
processtable:
                        if (eptr[1].ex_type != ET_LB) {
                                post("expr: '%s' - syntax error: brackets missing\n",
                                                                        x->exp_string);
                                return (exNULL);
                        }
                        /* if this table is the only token, parse the table */
                        if (!count &&
                            !((struct ex_ex *) eptr[1].ex_ptr)[1].ex_type) {
                                savex = *((struct ex_ex *) eptr[1].ex_ptr);
                                *((struct ex_ex *) eptr[1].ex_ptr) = nullex;
                                *optr = *eptr;
                                lowpre = ex_parse(x, &eptr[2], optr + 1, (long *)0);
                                *((struct ex_ex *) eptr[1].ex_ptr) = savex;
                                                                optr->ex_end = lowpre;
                                return(lowpre);
                        }
                        eptr = (struct ex_ex *) eptr[1].ex_ptr;
                        break;
                case ET_OP:
                        if (eptr->ex_op == OP_COMMA) {
                                if (!argc || !count || !eptr[1].ex_type) {
                                        post("expr: '%s' - syntax error: illegal comma\n",
                                                                x->exp_string);
                                        return (exNULL);
                                }
                        }
                        if (!eptr[1].ex_type) {
                                post("expr: '%s' - syntax error: missing operand\n",
                                                                x->exp_string);
                                return (exNULL);
                        }
                        if ((eptr->ex_op & PRE_MASK) <= pre) {
                                pre = eptr->ex_op & PRE_MASK;
                                lowpre = eptr;
                        }
                        break;
                case ET_FUNC:
                        if (eptr[1].ex_type != ET_LP) {
                                post("expr: '%s' - ex_parse: no parenthesis\n",
                                                                        x->exp_string);
                                return (exNULL);
                        }
                        /* if this function is the only token, parse it */
                        if (!count &&
                            !((struct ex_ex *) eptr[1].ex_ptr)[1].ex_type) {
                                long ac = 0;

                                *optr = *eptr;
                                savex = *((struct ex_ex *) eptr[1].ex_ptr);
                                /* do we have arguments? */
                                if (eptr[1].ex_ptr != (char *) &eptr[2]) {
                                    /* parse the arguments */
                                    savex = *((struct ex_ex *) eptr[1].ex_ptr);
                                    *((struct ex_ex *) eptr[1].ex_ptr) = nullex;
                                    lowpre = ex_parse(x, &eptr[2], optr + 1, &ac);
                                    if (!lowpre)
                                            return (exNULL);
                                    ac++;
                                }

                                f  = (t_ex_func *) eptr->ex_ptr;
                                if (f->f_argc != -1 && ac != f->f_argc){
                                        ex_error(x,
                                        "syntax error: function '%s' needs %ld arguments\n",
                                            f->f_name, f->f_argc);
                                        return (exNULL);
                                } else
                                        optr->ex_argc = ac;
                                *((struct ex_ex *) eptr[1].ex_ptr) = savex;
                                if (lowpre) {
                                    optr->ex_end = lowpre;
                                    return (lowpre);
                                }
                                /* we have no arguments */
                                tmpex = optr;
                                tmpex->ex_end = ++optr;
                                return (optr);

                        }
                        eptr = (struct ex_ex *) eptr[1].ex_ptr;
                        break;
                case ET_LP:
                case ET_LB:
                        if (!count &&
                            !((struct ex_ex *) eptr->ex_ptr)[1].ex_type) {
                                if (eptr->ex_ptr == (char *)(&eptr[1])) {
                                        ex_error(x, "expr: '%s' - syntax error: empty '%s'\n",
                                            x->exp_string,
                                            eptr->ex_type==ET_LP?"()":"[]");
                                        return (exNULL);
                                }
                                savex = *((struct ex_ex *) eptr->ex_ptr);
                                *((struct ex_ex *) eptr->ex_ptr) = nullex;
                                lowpre = ex_parse(x, &eptr[1], optr, (long *)0);
                                *((struct ex_ex *) eptr->ex_ptr) = savex;
                                                                optr->ex_end = lowpre;
                                return (lowpre);
                        }
                        eptr = (struct ex_ex *)eptr->ex_ptr;
                        break;
                case ET_STR:
                default:
                        post("expr: '%s' - ex_parse: type = 0x%lx\n",
                                                        x->exp_string, eptr->ex_type);
                        return (exNULL);
                }
          }

        if (pre == HI_PRE) {
                post("expr: '%s' - syntax error: missing operation\n", x->exp_string);
                return (exNULL);
        }
        if (count < 2) {
                post("expr: '%s' - syntax error: mission operand\n", x->exp_string);
                return (exNULL);
        }
        if (count == 2) {
                if (lowpre != iptr) {
                        post("expr: ex_parse: unary operator should be first\n");
                        return (exNULL);
                }
                if (!unary_op(lowpre->ex_op)) {
                        post("expr: '%s' - syntax error: not a uniary operator\n",
                                                                        x->exp_string);
                        return (exNULL);
                }
                *optr = *lowpre;
                eptr = ex_parse(x, &lowpre[1], optr + 1, argc);
                                optr->ex_end = eptr;
                return (eptr);
        }
        /* this is the case of using unary operator as a binary opetator */
        if (count == 3 && unary_op(lowpre->ex_op)) {
                post("expr: '%s' - syntax error, missing operand before unary operator\n",
                                                                        x->exp_string);
                return (exNULL);
        }
        if (lowpre == iptr) {
                post("expr: '%s' - syntax error: mission operand\n", x->exp_string);
                return (exNULL);
        }
        savex = *lowpre;
        *lowpre = nullex;
        if (savex.ex_op != OP_COMMA) {
                *optr = savex;
                eptr = ex_parse(x, iptr, optr + 1, argc);
        } else {
                (*argc)++;
                eptr = ex_parse(x, iptr, optr, argc);
        }
        if (eptr) {
                eptr = ex_parse(x, &lowpre[1], eptr, argc);
                *lowpre = savex;
        }
        optr->ex_end = eptr;
        return (eptr);
}

/*
 * ex_checklval -- check the left value for all stores ('=')
 *                 all left values should either be a variable or a table
 *                 return 1 if syntax error
 *                 return 0 on success
 */

static int
ex_checklval(struct expr *e, struct ex_ex *eptr)
{
        struct ex_ex *extmp;

        extmp = eptr->ex_end;
        while (eptr->ex_type && eptr != extmp) {
                if (eptr->ex_type == ET_OP && eptr->ex_op == OP_STORE) {
                        if (eptr[1].ex_type != ET_VAR &&
                            eptr[1].ex_type != ET_SI &&
                            eptr[1].ex_type != ET_TBL) {
                                post("expr: '%s' - Bad left value: ", e->exp_string);
                               return (1);
                         }
                }
                eptr++;
        }
        return (0);
}


/*
 * this is the divide zero check for a non divide operator
 */
#define DZC(ARG1,OPR,ARG2)      (ARG1 OPR ARG2)

#define EVAL(OPR);                                                      \
eptr = ex_eval(expr, ex_eval(expr, eptr, &left, idx), &right, idx);     \
switch (left.ex_type) {                                                 \
case ET_INT:                                                            \
        switch(right.ex_type) {                                         \
        case ET_INT:                                                    \
                if (optr->ex_type == ET_VEC) {                          \
                        op = optr->ex_vec;                              \
                        scalar = (t_float)DZC(left.ex_int, OPR, right.ex_int); \
                        for (j = 0; j < expr->exp_vsize; j++)           \
                                *op++ = scalar;                         \
                } else {                                                \
                        optr->ex_type = ET_INT;                         \
                        optr->ex_int = DZC(left.ex_int, OPR, right.ex_int); \
                }                                                       \
                break;                                                  \
        case ET_FLT:                                                    \
                if (optr->ex_type == ET_VEC) {                          \
                        op = optr->ex_vec;                              \
                        scalar = DZC(((t_float)left.ex_int), OPR, right.ex_flt);\
                        for (j = 0; j < expr->exp_vsize; j++)           \
                                *op++ = scalar;                         \
                } else {                                                \
                        optr->ex_type = ET_FLT;                         \
                        optr->ex_flt = DZC(((t_float)left.ex_int), OPR,   \
                                                        right.ex_flt);  \
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
                          fts_malloc(sizeof (t_float)*expr->exp_vsize); \
                }                                                       \
                scalar = left.ex_int;                                   \
                rp = right.ex_vec;                                      \
                op = optr->ex_vec;                                      \
                for (i = 0; i < expr->exp_vsize; i++) {                 \
                        *op++ = DZC (scalar, OPR, *rp);                 \
                        rp++;                                           \
                }                                                       \
                break;                                                  \
        case ET_SYM:                                                    \
        default:                                                        \
                post_error((fts_object_t *) expr,                       \
                      "expr: ex_eval(%d): bad right type %ld\n",        \
                                              __LINE__, right.ex_type); \
                nullret = 1;                                            \
        }                                                               \
        break;                                                          \
case ET_FLT:                                                            \
        switch(right.ex_type) {                                         \
        case ET_INT:                                                    \
                if (optr->ex_type == ET_VEC) {                          \
                        op = optr->ex_vec;                              \
                        scalar = DZC((t_float) left.ex_flt, OPR, right.ex_int); \
                        for (j = 0; j < expr->exp_vsize; j++)           \
                                *op++ = scalar;                         \
                } else {                                                \
                        optr->ex_type = ET_FLT;                         \
                        optr->ex_flt = DZC(left.ex_flt, OPR, right.ex_int); \
                }                                                       \
                break;                                                  \
        case ET_FLT:                                                    \
                if (optr->ex_type == ET_VEC) {                          \
                        op = optr->ex_vec;                              \
                        scalar = DZC(left.ex_flt, OPR, right.ex_flt);   \
                        for (j = 0; j < expr->exp_vsize; j++)           \
                                *op++ = scalar;                         \
                } else {                                                \
                        optr->ex_type = ET_FLT;                         \
                        optr->ex_flt= DZC(left.ex_flt, OPR, right.ex_flt); \
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
                          fts_malloc(sizeof (t_float)*expr->exp_vsize); \
                }                                                       \
                scalar = left.ex_flt;                                   \
                rp = right.ex_vec;                                      \
                op = optr->ex_vec;                                      \
                for (i = 0; i < expr->exp_vsize; i++) {                 \
                        *op++ = DZC(scalar, OPR, *rp);                  \
                        rp++;                                           \
                }                                                       \
                break;                                                  \
        case ET_SYM:                                                    \
        default:                                                        \
                post_error((fts_object_t *) expr,                       \
                      "expr: ex_eval(%d): bad right type %ld\n",        \
                                              __LINE__, right.ex_type); \
                nullret = 1;                                            \
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
                  fts_malloc(sizeof (t_float)*expr->exp_vsize);         \
        }                                                               \
        op = optr->ex_vec;                                              \
        lp = left.ex_vec;                                               \
        switch(right.ex_type) {                                         \
        case ET_INT:                                                    \
                scalar = right.ex_int;                                  \
                for (i = 0; i < expr->exp_vsize; i++) {                 \
                        *op++ = DZC(*lp, OPR, scalar);                  \
                        lp++;                                           \
                }                                                       \
                break;                                                  \
        case ET_FLT:                                                    \
                scalar = right.ex_flt;                                  \
                for (i = 0; i < expr->exp_vsize; i++) {                 \
                        *op++ = DZC(*lp, OPR, scalar);                  \
                        lp++;                                           \
                }                                                       \
                break;                                                  \
        case ET_VEC:                                                    \
        case ET_VI:                                                     \
                rp = right.ex_vec;                                      \
                for (i = 0; i < expr->exp_vsize; i++) {                 \
                        /*                                              \
                         * on a RISC processor one could copy           \
                         * 8 times in each round to get a considerable  \
                         * improvement                                  \
                         */                                             \
                        *op++ = DZC(*lp, OPR, *rp);                     \
                        rp++; lp++;                                     \
                }                                                       \
                break;                                                  \
        case ET_SYM:                                                    \
        default:                                                        \
                post_error((fts_object_t *) expr,                       \
                      "expr: ex_eval(%d): bad right type %ld\n",        \
                                              __LINE__, right.ex_type); \
                nullret = 1;                                            \
        }                                                               \
        break;                                                          \
case ET_SYM:                                                            \
default:                                                                \
                post_error((fts_object_t *) expr,                       \
                      "expr: ex_eval(%d): bad left type %ld\n",         \
                                              __LINE__, left.ex_type);  \
}                                                                       \
break;

/*
 * evaluate a unary operator, TYPE is applied to float operands
 */
#define EVAL_UNARY(OPR, TYPE)                                           \
        eptr = ex_eval(expr, eptr, &left, idx);                         \
        switch(left.ex_type) {                                          \
        case ET_INT:                                                    \
                if (optr->ex_type == ET_VEC) {                          \
                        ex_mkvector(optr->ex_vec,(t_float)(OPR left.ex_int),\
                                                        expr->exp_vsize);\
                        break;                                          \
                }                                                       \
                optr->ex_type = ET_INT;                                 \
                optr->ex_int = OPR left.ex_int;                         \
                break;                                                  \
        case ET_FLT:                                                    \
                if (optr->ex_type == ET_VEC) {                          \
                        ex_mkvector(optr->ex_vec, OPR (TYPE left.ex_flt),\
                                                        expr->exp_vsize);\
                        break;                                          \
                }                                                       \
                optr->ex_type = ET_FLT;                                 \
                optr->ex_flt = OPR (TYPE left.ex_flt);                  \
                break;                                                  \
        case ET_VI:                                                     \
        case ET_VEC:                                                    \
                j = expr->exp_vsize;                                    \
                if (optr->ex_type != ET_VEC) {                          \
                        optr->ex_type = ET_VEC;                         \
                        optr->ex_vec = (t_float *)                      \
                          fts_malloc(sizeof (t_float)*expr->exp_vsize); \
                }                                                       \
                op = optr->ex_vec;                                      \
                lp = left.ex_vec;                                       \
                j = expr->exp_vsize;                                    \
                for (i = 0; i < j; i++)                                 \
                        *op++ = OPR (TYPE *lp++);                       \
                break;                                                  \
        default:                                                        \
                post_error((fts_object_t *) expr,                       \
                        "expr: ex_eval(%d): bad left type %ld\n",       \
                                              __LINE__, left.ex_type);  \
                nullret++;                                              \
        }                                                               \
        break;

void
ex_mkvector(t_float *fp, t_float x, int size)
{
        while (size--)
                *fp++ = x;
}

/*
 * ex_dzdetect -- divide by zero detected
 */
void
ex_dzdetect(struct expr *expr)
{
        char *etype;

        if ((!expr->exp_error) & EE_DZ) {
                if (IS_EXPR(expr))
                        etype = "expr";
                else if (IS_EXPR_TILDE(expr))
                        etype = "expr~";
                else if (IS_FEXPR_TILDE(expr))
                        etype = "fexpr~";
                else {
                        post ("expr -- ex_dzdetect internal error");
                        etype = "";
                }
                post ("%s divide by zero detected - '%s'", etype, expr->exp_string);
                expr->exp_error |= EE_DZ;
        }
}


/*
 * ex_eval -- evaluate the array of prefix expression
 *            ex_eval returns the pointer to the first unevaluated node
 *            in the array.  This is a recursive routine.
 */

/* SDY - look into the variable nullret */
struct ex_ex *
ex_eval(struct expr *expr, struct ex_ex *eptr, struct ex_ex *optr, int idx)
/* the expr object data pointer */
/* the operation stack */
/* the result pointer */
/* the sample number processed for fexpr~ */
{
        int i, j;
        t_float *lp, *rp, *op; /* left, right, and out pointer to vectors */
        t_float scalar;
        int nullret = 0;                /* did we have an error */
        struct ex_ex left = { 0 }, right = { 0 };       /* left and right operands */

        left.ex_type = 0;
        left.ex_int = 0;
        right.ex_type = 0;
        right.ex_int = 0;

        if (!eptr)
                return (exNULL);
        switch (eptr->ex_type) {
        case ET_INT:
                if (optr->ex_type == ET_VEC)
                        ex_mkvector(optr->ex_vec, (t_float) eptr->ex_int,
                                                                expr->exp_vsize);
                else
                        *optr = *eptr;
                return (++eptr);

        case ET_FLT:

                if (optr->ex_type == ET_VEC)
                        ex_mkvector(optr->ex_vec, eptr->ex_flt, expr->exp_vsize);
                else
                        *optr = *eptr;
                return (++eptr);

        case ET_SYM:
                if (optr->ex_type == ET_VEC) {
                        if (!(expr->exp_error & EE_BADSYM)) {
                                expr->exp_error |= EE_BADSYM;
                                post_error((fts_object_t *) expr,
                                    "expr~: '%s': cannot convert string to vector\n",
                                    expr->exp_string);
                                post_error(expr,
                                    "expr~: No more symbol-vector errors will be reported");
                                post_error(expr,
                                    "expr~: till the next reset");
                        }
                        ex_mkvector(optr->ex_vec, 0, expr->exp_vsize);
                } else
                        *optr = *eptr;
                return (++eptr);
        case ET_II:
                if (eptr->ex_int == -1) {
                        post_error((fts_object_t *) expr,
                            "expr: '%s': inlet number not set\n", expr->exp_string);
                        return (exNULL);
                }
                if (optr->ex_type == ET_VEC) {
                        ex_mkvector(optr->ex_vec,
                               (t_float)expr->exp_var[eptr->ex_int].ex_int,
                                                                expr->exp_vsize);
                } else {
                        optr->ex_type = ET_INT;
                        optr->ex_int = expr->exp_var[eptr->ex_int].ex_int;
                }
                return (++eptr);
        case ET_FI:
                if (eptr->ex_int == -1) {
                        post_error((fts_object_t *) expr,
                            "expr: ex_eval: inlet number not set\n");
                        return (exNULL);
                }
                if (optr->ex_type == ET_VEC) {
                        ex_mkvector(optr->ex_vec,
                             expr->exp_var[eptr->ex_int].ex_flt, expr->exp_vsize);
                } else {
                        optr->ex_type = ET_FLT;
                        optr->ex_flt = expr->exp_var[eptr->ex_int].ex_flt;
                }
                return (++eptr);

/* SDY ET_VSYM is gone? */
        case ET_VSYM:
                if (optr->ex_type == ET_VEC) {
                        post_error((fts_object_t *) expr,
                            "expr: IntErr. vsym in for vec out\n");
                        return (exNULL);
                }
                if (eptr->ex_int == -1) {
                        post_error((fts_object_t *) expr,
                                "expr: ex_eval: inlet number not set\n");
                        return (exNULL);
                }
                optr->ex_type = ET_SYM;
                /* not a temporary symbol; do not free this ET_SYM expression */
                optr->ex_flags &= ~EX_F_TSYM;
                optr->ex_ptr = expr->exp_var[eptr->ex_int].ex_ptr;
                return(++eptr);

        case ET_VI:
                if (optr->ex_type != ET_VEC)
                        *optr = expr->exp_var[eptr->ex_int];
                else if (optr->ex_vec != expr->exp_var[eptr->ex_int].ex_vec)
                        memcpy(optr->ex_vec, expr->exp_var[eptr->ex_int].ex_vec,
                                        expr->exp_vsize * sizeof (t_float));
                return(++eptr);
        case ET_VEC:
                if (optr->ex_type != ET_VEC) {
                        optr->ex_type = ET_VEC;
                        optr->ex_vec = eptr->ex_vec;
                        eptr->ex_type = ET_INT;
                        eptr->ex_int = 0;
                } else if (optr->ex_vec != eptr->ex_vec) {
                        memcpy(optr->ex_vec, eptr->ex_vec,
                                        expr->exp_vsize * sizeof (t_float));
                        fts_free(eptr->ex_vec);
                } else { /* this should not happen */
                        post("expr int. error, optr->ex_vec = %d",optr->ex_vec);
                        abort();
                }
                return(++eptr);
        case ET_XI0:
                /* short hand for $x?[0] */

                /* SDY delete the following check */
                if (!IS_FEXPR_TILDE(expr) || optr->ex_type==ET_VEC) {
                        post("%d:exp->exp_flags = %d", __LINE__,expr->exp_flags);
                        abort();
                }
                optr->ex_type = ET_FLT;
                optr->ex_flt = expr->exp_var[eptr->ex_int].ex_vec[idx];
                return(++eptr);
        case ET_YOM1:
                /*
                 * short hand for $y?[-1]
                 * if we are calculating the first sample of the vector
                 * we need to look at the previous results buffer
                 */
                optr->ex_type = ET_FLT;
                if (idx == 0)
                        optr->ex_flt =
                        expr->exp_p_res[eptr->ex_int][expr->exp_vsize - 1];
                else
                        optr->ex_flt=expr->exp_tmpres[eptr->ex_int][idx-1];
                return(++eptr);

        case ET_YO:
        case ET_XI:
                /* SDY delete the following */
                if (!IS_FEXPR_TILDE(expr) || optr->ex_type==ET_VEC) {
                        post("%d:expr->exp_flags = %d", __LINE__,expr->exp_flags);
                        abort();
                }
                return (eval_sigidx(expr, eptr, optr, idx));

        case ET_TBL:
                return (eval_tab(expr, eptr, optr, idx));
        case ET_SI:
                /* check if the symbol input is a table */
                if (eptr->ex_flags & EX_F_SI_TAB) {
                        eptr = eval_tab(expr, eptr, optr, idx);
                        return (eptr);
                        //return (eval_tab(expr, eptr, optr, idx));
                }
                /* it is just a symbol input */
                if (optr->ex_type == ET_VEC) {
                        if (!(expr->exp_error & EE_BADSYM)) {
                                expr->exp_error |= EE_BADSYM;
                                post_error((fts_object_t *) expr,
                                    "expr~: '%s': cannot convert string inlet to vector\n",
                                    expr->exp_string);
                                post_error(expr,
                                    "expr~: No more symbol-vector errors will be reported");
                                post_error(expr,
                                    "expr~: till the next reset");
                        }
                        ex_mkvector(optr->ex_vec, 0, expr->exp_vsize);
                        return (exNULL);
                }
                *optr = *eptr++;
                return(eptr);

        case ET_FUNC:
                return (eval_func(expr, eptr, optr, idx));
        case ET_VAR:
                return (eval_var(expr, eptr, optr, idx));
        case ET_OP:
                break;
        case ET_STR:
        case ET_LP:
        case ET_LB:
        default:
                post_error((fts_object_t *) expr,
                        "expr: ex_eval: unexpected type %d\n",
                            (int)eptr->ex_type);
                return (exNULL);
        }
        if (!eptr[1].ex_type) {
                post_error((fts_object_t *) expr,
                                        "expr: ex_eval: not enough nodes 1\n");
                return (exNULL);
        }
        if (!unary_op(eptr->ex_op) && !eptr[2].ex_type) {
                post_error((fts_object_t *) expr,
                                        "expr: ex_eval: not enough nodes 2\n");
                return (exNULL);
        }

        switch((eptr++)->ex_op) {
        case OP_STORE:
                return (eval_store(expr, eptr, optr, idx));
        case OP_NOT:
                EVAL_UNARY(!, +);
        case OP_NEG:
                EVAL_UNARY(~, (long));
        case OP_UMINUS:
                EVAL_UNARY(-, +);
        case OP_MUL:
                EVAL(*);
        case OP_ADD:
                EVAL(+);
        case OP_SUB:
                EVAL(-);
        case OP_LT:
                EVAL(<);
        case OP_LE:
                EVAL(<=);
        case OP_GT:
                EVAL(>);
        case OP_GE:
                EVAL(>=);
        case OP_EQ:
                EVAL(==);
        case OP_NE:
                EVAL(!=);
/*
 * following operators convert their argument to integer
 */
#undef DZC
#define DZC(ARG1,OPR,ARG2)      (((int)ARG1) OPR ((int)ARG2))
        case OP_SL:
                EVAL(<<);
        case OP_SR:
                EVAL(>>);
        case OP_AND:
                EVAL(&);
        case OP_XOR:
                EVAL(^);
        case OP_OR:
                EVAL(|);
        case OP_LAND:
                EVAL(&&);
        case OP_LOR:
                EVAL(||);
/*
 * for modulo we need to convert to integer and check for divide by zero
 */
#undef DZC
#define DZC(ARG1,OPR,ARG2)      ((((int)ARG2)?(((int)ARG1) OPR ((int)ARG2)) \
                                                        : (ex_dzdetect(expr),0)))
        case OP_MOD:
                EVAL(%);
/*
 * define the divide by zero check for divide
 */
#undef DZC
#define DZC(ARG1,OPR,ARG2)      (((ARG2)?(ARG1 OPR ARG2):(ex_dzdetect(expr),0)))
        case OP_DIV:
                EVAL(/);
        case OP_LP:
        case OP_RP:
        case OP_LB:
        case OP_RB:
        case OP_COMMA:
        case OP_SEMI:
        default:
                post_error((fts_object_t *) expr,
                    "expr: '%s' - bad op 0x%x\n",
                                        expr->exp_string, (unsigned)eptr->ex_op);
                return (exNULL);
        }


        /*
         * the left and right nodes could have been transformed to vectors
         * down the chain
         */
        if (left.ex_type == ET_VEC)
                fts_free(left.ex_vec);
        if (right.ex_type == ET_VEC)
                fts_free(right.ex_vec);
        if (nullret)
                return (exNULL);
        else
                return (eptr);
}

extern struct ex_ex * ex_if(t_expr *expr,  struct ex_ex *eptr,
                                struct ex_ex *optr,struct ex_ex *argv, int idx);

/*
 * eval_func --  evaluate a function, call ex_eval() on all the arguments
 *               so that all of them are terminal nodes. The call the
 *               appropriate function
 */
struct ex_ex *
eval_func(struct expr *expr, struct ex_ex *eptr, struct ex_ex *optr, int idx)
/* the expr object data pointer */
/* the operation stack */
/* the result pointer */
{
        int i;
        struct ex_ex args[MAX_ARGS];
        t_ex_func *f;
        int argc;

        argc = eptr->ex_argc;
        f = (t_ex_func *)(eptr++)->ex_ptr;
        if (!f || !f->f_name) {
                return (exNULL);
        }
        if (argc > MAX_ARGS) {
                post_error((fts_object_t *) expr, "expr: eval_func: asking too many arguments\n");
                return (exNULL);
        }

        /*
         * We treat the "if" function differently to be able to evaluate
         * the args selectively based on the truth value of the "condition"
         */
        if (f->f_func != (void (*)) ex_if) {
                for (i = 0; i < argc; i++) {
                        args[i].ex_type = 0;
                        args[i].ex_int = 0;
                        eptr = ex_eval(expr, eptr, &args[i], idx);
                }
                (*f->f_func)(expr, argc, args, optr);
        } else {
                for (i = 0; i < argc; i++) {
                        args[i].ex_type = 0;
                        args[i].ex_int = 0;
                }
                eptr = ex_if(expr, eptr, optr, args, idx);
        }
        for (i = 0; i < argc; i++) {
                if (args[i].ex_type == ET_VEC)
                        free(args[i].ex_vec);
                else if (args[i].ex_type == ET_SYM
                                && args[i].ex_flags & EX_F_TSYM) {
                        free(args[i].ex_ptr);
                        args[i].ex_flags &= ~EX_F_TSYM;
                }
        }
        return (eptr);
}


/*
 * eval_store --  evaluate the '=' operator,
 *                make sure the first operator is a legal left operator
 *                and call ex_eval on the right operator
 */
struct ex_ex *
eval_store(struct expr *expr, struct ex_ex *eptr, struct ex_ex *optr, int idx)
/* the expr object data pointer */
/* the operation stack */
/* the result pointer */
{
        struct ex_ex arg = { 0 };
        struct ex_ex rval = { 0 };
        struct ex_ex *retp;
        char *tbl = (char *) 0;
        char *var = (char *) 0;
        int badleft = 0;
        int notable = 0;

        arg.ex_type = ET_INT;
        arg.ex_int = 0;

        switch (eptr->ex_type) {
        case ET_VAR:
                var = (char *) eptr->ex_ptr;
                eptr = ex_eval(expr, ++eptr, &arg, idx);
                if (max_ex_var_store(expr, (t_symbol *)var, &arg, optr))
                        retp = exNULL;
                else
                        retp = eptr;

                if (arg.ex_type == ET_VEC)
                        fts_free(arg.ex_vec);
                return(retp);
        case ET_TBL:
                tbl = (char *) eptr->ex_ptr;
                break;
        case ET_SI:
                if (eptr->ex_flags & EX_F_SI_TAB) {
                        post("expr: symbol cannot be a left value '%s'",
                                expr->exp_string);
                        retp = exNULL;
                        return (retp);
                }
                if (!expr->exp_var[eptr->ex_int].ex_ptr) {
                        if (!(expr->exp_error & EE_NOTABLE)) {
                                post_error(expr, "expr: '%s': syntax error: no string for inlet %ld",
                                            expr->exp_string, eptr->ex_int + 1);
                                post_error(expr, "expr: No more table errors will be reported");
                                post_error(expr, "expr: till the next reset");
                                expr->exp_error |= EE_NOTABLE;
                        }
                        badleft++;
                        post("expr: '%s' - Bad left value", expr->exp_string);
                        retp = exNULL;
                        return (retp);
                } else {
                        tbl = (char *) expr->exp_var[eptr->ex_int].ex_ptr;
                }
                break;
        default:
                post("expr: '%s' - Bad left value", expr->exp_string);
                /* report Error */
                retp = exNULL;
                return (retp);
        }
        arg.ex_type = 0;
        arg.ex_int = 0;
        /* evaluate the index of the table */
        if (!(eptr = ex_eval(expr, ++eptr, &arg, idx)))
                return (eptr);

        /* evaluate the right index of the table */
        if (!(eptr = ex_eval(expr, eptr, &rval, idx)))
                return (eptr);
        optr->ex_type = ET_INT;
        optr->ex_int = 0;
        if (!notable || badleft)
                (void)max_ex_tab_store(expr, (t_symbol *)tbl, &arg, &rval,optr);
        if (arg.ex_type == ET_VEC)
                fts_free(arg.ex_vec);
        return (eptr);
}

/*
 * eval_tab -- evaluate a table operation
 */
struct ex_ex *
eval_tab(struct expr *expr, struct ex_ex *eptr, struct ex_ex *optr, int idx)
/* the expr object data pointer */
/* the operation stack */
/* the result pointer */
{
        struct ex_ex arg = { 0 };
        char *tbl = (char *) 0;
        int notable = 0;

        if (eptr->ex_type == ET_SI) {
                if (!expr->exp_var[eptr->ex_int].ex_ptr) {
                        if (!(expr->exp_error & EE_NOTABLE)) {
                                post_error(expr, "expr:'%s': no string for inlet %ld",
                                                                                                                        expr->exp_string, eptr->ex_int + 1);
                                post("expr: No more table errors will be reported");
                                post("expr: till the next reset");
                                expr->exp_error |= EE_NOTABLE;
                        }
                        notable++;
                } else
                        tbl = (char *) expr->exp_var[eptr->ex_int].ex_ptr;
        } else if (eptr->ex_type == ET_TBL) {
                tbl = (char *) eptr->ex_ptr;
                if (!tbl) {
                        post("expr: abstraction argument for table not set");
                        notable++;
                }

        } else {
                post_error((fts_object_t *) expr,"expr: eval_tbl: bad type %ld\n",eptr->ex_type);
                notable++;

        }
        arg.ex_type = 0;
        arg.ex_int = 0;
        if (!(eptr = ex_eval(expr, ++eptr, &arg, idx)))
                return (eptr);

        if (!notable)
                (void)max_ex_tab(expr, (t_symbol *)tbl, &arg, 0, optr);
        if (arg.ex_type == ET_VEC)
                fts_free(arg.ex_vec);
        return (eptr);
}

/*
 * eval_var -- evaluate a variable
 */
struct ex_ex *
eval_var(struct expr *expr, struct ex_ex *eptr, struct ex_ex *optr, int idx)
/* the expr object data pointer */
/* the operation stack */
/* the result pointer */
{
        char *var = (char *) 0;
        int novar = 0;

        if (eptr->ex_type == ET_SI) {
                if (!expr->exp_var[eptr->ex_int].ex_ptr) {
                        if (!(expr->exp_error & EE_NOVAR)) {
                                post("expr: syntax error: no string for inlet %d", eptr->ex_int + 1);
                                post("expr: no more table errors will be reported");
                                post("expr: till the next reset");
                                expr->exp_error |= EE_NOVAR;
                        }
                        novar++;
                } else
                        var = (char *) expr->exp_var[eptr->ex_int].ex_ptr;
        } else if (eptr->ex_type == ET_VAR)
                var = (char *) eptr->ex_ptr;
        else {
                post_error((fts_object_t *) expr, "expr: eval_tbl: bad type %ld\n", eptr->ex_type);
                novar++;

        }

        if (!novar)
                (void)max_ex_var(expr, (t_symbol *)var, optr, idx);
        else {
            if (optr->ex_type == ET_VEC)
                ex_mkvector(optr->ex_vec, 0, expr->exp_vsize);
            else {
                optr->ex_type = ET_INT;
                optr->ex_int = 0;
            }
        }
        return (++eptr);
}

/*
 * eval_sigidx -- evaluate the value of an indexed signal for fexpr~
 */
struct ex_ex *
eval_sigidx(struct expr *expr, struct ex_ex *eptr, struct ex_ex *optr, int idx)
/* the expr object data pointer */
/* the operation stack */
/* the result pointer */
/* the index */
{
        struct ex_ex arg = { 0 };
        struct ex_ex *reteptr;
        int i = 0;
        t_float fi = 0,         /* index in float */
              rem_i = 0;        /* remains of the float */

        arg.ex_type = 0;
        arg.ex_int = 0;
        reteptr = ex_eval(expr, eptr + 1, &arg, idx);
        if (arg.ex_type == ET_FLT) {
                fi = arg.ex_flt;                /* float index */
                i = (int) arg.ex_flt;           /* integer index */
                rem_i =  arg.ex_flt - i;        /* remains of integer */
        } else if (arg.ex_type == ET_INT) {
                fi = arg.ex_int;                /* float index */
                i = (int) arg.ex_int;           /* integer index */
                rem_i = 0;
        } else {
                post("eval_sigidx: bad res type (%d)", arg.ex_type);
        }
        optr->ex_type = ET_FLT;
        /*
         * indexing an input vector
         */
        if (eptr->ex_type == ET_XI) {
                if (fi > 0) {
                        if (!(expr->exp_error & EE_BI_INPUT)) {
                                expr->exp_error |= EE_BI_INPUT;
                          post("expr: '%s' - input vector index > 0, (vector x%d[%f])",
                                       expr->exp_string, eptr->ex_int + 1, i + rem_i);
                                post("fexpr~: index assumed to be = 0");
                                post("fexpr~: no error report till next reset");
                        }
                        /* just replace it with zero */
                        i = 0;
                        rem_i = 0;
                }
                if (cal_sigidx(optr, i, rem_i, idx, expr->exp_vsize,
                                        expr->exp_var[eptr->ex_int].ex_vec,
                                                expr->exp_p_var[eptr->ex_int])) {
                        if (!(expr->exp_error & EE_BI_INPUT)) {
                                expr->exp_error |= EE_BI_INPUT;
                                post("expr: '%s' - input vector index <  -VectorSize, (vector x%d[%f])",
                                        expr->exp_string, eptr->ex_int + 1, fi);
                                post("fexpr~: index assumed to be = -%d",
                                        expr->exp_vsize);
                                post("fexpr~: no error report till next reset");
                        }
                }

        /*
         * indexing an output vector
         */
        } else if (eptr->ex_type == ET_YO) {
                /* for output vectors index of zero is not legal */
                if (fi >= 0) {
                        if (!(expr->exp_error & EE_BI_OUTPUT)) {
                                expr->exp_error |= EE_BI_OUTPUT;
                                post("fexpr~: '%s' - bad output index, (%f)",
                                                                expr->exp_string, fi);
                                post("fexpr~: no error report till next reset");
                                post("fexpr~: index assumed to be = -1");
                        }
                        i = -1;
                }
                if (eptr->ex_int >= expr->exp_nexpr) {
                        post("fexpr~: $y%d illegal: not that many expr's",
                                                                eptr->ex_int);
                        optr->ex_flt = 0;
                        return (reteptr);
                }
                if (cal_sigidx(optr, i, rem_i, idx, expr->exp_vsize,
                             expr->exp_tmpres[eptr->ex_int],
                                                expr->exp_p_res[eptr->ex_int])) {
                        if (!(expr->exp_error & EE_BI_OUTPUT)) {
                                expr->exp_error |= EE_BI_OUTPUT;
                                post("fexpr~: '%s' - bad output index, (%f)",
                                                                expr->exp_string, fi);
                                post("fexpr~: index assumed to be = -%d",
                                        expr->exp_vsize);
                        }
                }
        } else {
                optr->ex_flt = 0;
                post("fexpr~:eval_sigidx: internal error - unknown vector (%d)",
                                                                eptr->ex_type);
        }
        return (reteptr);
}

/*
 * cal_sigidx -- given two tables (one current one previous) calculate an
 *               evaluation of a float index into the vectors by linear
 *               interpolation
 *              return 0 on success, 1 on failure (index out of bound)
 */
static int
cal_sigidx(struct ex_ex *optr,  /* The output value */
           int i, t_float rem_i,/* integer and fractinal part of index */
           int idx,             /* index of current fexpr~ processing */
           int vsize,           /* vector size */
           t_float *curvec, t_float *prevec)        /* current and previous table */
{
        int n;

        n = i + idx;
        if (n > 0) {
                /* from the curvec */
                if (rem_i)
                        optr->ex_flt = curvec[n] +
                                        rem_i * (curvec[n] - curvec[n - 1]);
                else
                        optr->ex_flt = curvec[n];
                return (0);
        }
        if (n == 0) {
                /*
                 * this is the case that the remaining float
                 * is between two tables
                 */
                if (rem_i)
                        optr->ex_flt = *curvec +
                                        rem_i * (*curvec - prevec[vsize - 1]);
                else
                        optr->ex_flt = *curvec;
                return (0);
        }
        /* find the index in the saved buffer */
        n = vsize + n;
        if (n > 0) {
                if (rem_i)
                        optr->ex_flt = prevec[n] +
                                        rem_i * (prevec[n] - prevec[n - 1]);
                else
                        optr->ex_flt = prevec[n];
                return (0);
        }
        /* out of bound */
        optr->ex_flt = *prevec;
        return (1);
}

/*
 * getoken -- return 1 on syntax error otherwise 0
 */
int
getoken(struct expr *expr, struct ex_ex *eptr)
{
        char *p, *tmpstr;
        long i;



        if (!expr->exp_str) {
                post("expr: getoken: expression string not set\n");
                return (0);
        }
retry:
        if (!*expr->exp_str) {
                eptr->ex_type = 0;
                eptr->ex_int = 0;
                return (0);
        }
        if (*expr->exp_str == ';') {
                expr->exp_str++;
                eptr->ex_type = 0;
                eptr->ex_int = 0;
                return (0);
        }
        eptr->ex_type = ET_OP;
        switch (*expr->exp_str++) {
        case '\\':
        case ' ':
        case '\t':
                goto retry;
        case ';':
                post("expr: syntax error: ';' not implemented\n");
                return (1);
        case ',':
                eptr->ex_op = OP_COMMA;
                break;
        case '(':
                eptr->ex_op = OP_LP;
                break;
        case ')':
                eptr->ex_op = OP_RP;
                break;
        case ']':
                eptr->ex_op = OP_RB;
                break;
        case '~':
                eptr->ex_op = OP_NEG;
                break;
                /* we will take care of unary minus later */
        case '*':
                eptr->ex_op = OP_MUL;
                break;
        case '/':
                eptr->ex_op = OP_DIV;
                break;
        case '%':
                eptr->ex_op = OP_MOD;
                break;
        case '+':
                eptr->ex_op = OP_ADD;
                break;
        case '-':
                eptr->ex_op = OP_SUB;
                break;
        case '^':
                eptr->ex_op = OP_XOR;
                break;
        case '[':
                eptr->ex_op = OP_LB;
                break;
        case '!':
                if (*expr->exp_str == '=') {
                        eptr->ex_op = OP_NE;
                        expr->exp_str++;
                } else
                        eptr->ex_op = OP_NOT;
                break;
        case '<':
                switch (*expr->exp_str) {
                case '<':
                        eptr->ex_op = OP_SL;
                        expr->exp_str++;
                        break;
                case '=':
                        eptr->ex_op = OP_LE;
                        expr->exp_str++;
                        break;
                default:
                        eptr->ex_op = OP_LT;
                        break;
                }
                break;
        case '>':
                switch (*expr->exp_str) {
                case '>':
                        eptr->ex_op = OP_SR;
                        expr->exp_str++;
                        break;
                case '=':
                        eptr->ex_op = OP_GE;
                        expr->exp_str++;
                        break;
                default:
                        eptr->ex_op = OP_GT;
                        break;
                }
                break;
        case '=':
                if (*expr->exp_str != '=')
                        eptr->ex_op = OP_STORE;
                else {
                        expr->exp_str++;
                        eptr->ex_op = OP_EQ;
                }
                break;

        case '&':
                if (*expr->exp_str == '&') {
                        expr->exp_str++;
                        eptr->ex_op = OP_LAND;
                } else
                        eptr->ex_op = OP_AND;
                break;

        case '|':
                if (*expr->exp_str == '|') {
                        expr->exp_str++;
                        eptr->ex_op = OP_LOR;
                } else
                        eptr->ex_op = OP_OR;
                break;
        case '$':
                switch (*expr->exp_str++) {
                case 'I':
                case 'i':
                        eptr->ex_type = ET_II;
                        break;
                case 'F':
                case 'f':
                        eptr->ex_type = ET_FI;
                        break;
                case 'S':
                case 's':
                        eptr->ex_type = ET_SI;
                        break;
                case 'V':
                case 'v':
                        if (IS_EXPR_TILDE(expr)) {
                                eptr->ex_type = ET_VI;
                                break;
                        }
                        post("$v? works only for expr~");
                        post("expr: syntax error: %s\n", &expr->exp_str[-2]);
                        return (1);
                case 'X':
                case 'x':
                        if (IS_FEXPR_TILDE(expr)) {
                                eptr->ex_type = ET_XI;
                                if (isdigit((int)(*expr->exp_str)))
                                        break;
                                /* for $x[] is a shorhand for $x1[] */
                                /* eptr->ex_int = 0; */
                                                                eptr->ex_op = 0;
                                                                expr->exp_var[eptr->ex_op].ex_type = eptr->ex_type;
                                goto noinletnum;
                        }
                        post("$x? works only for fexpr~");
                        post("expr: syntax error: %s\n", &expr->exp_str[-2]);
                        return (1);
                case 'y':
                case 'Y':
                        if (IS_FEXPR_TILDE(expr)) {
                                eptr->ex_type = ET_YO;
                                /*$y takes no number */
                                if (isdigit((int)(*expr->exp_str)))
                                        break;
                                /* for $y[] is a shorhand for $y1[] */
                                /* eptr->ex_int = 0; */
                                                                eptr->ex_op = 0;
                                                                expr->exp_var[eptr->ex_op].ex_type = eptr->ex_type;
                                goto noinletnum;
                        }
                        post("$y works only for fexpr~");
                                /* falls through */
                                /*
                                 * allow $# for abstraction argument substitution
                                 *  $1+1 is translated to 0+1 and in abstraction substitution
                                 *  the value is replaced with the new string
                                 */
                                case '0':
                                case '1':
                                case '2':
                                case '3':
                                case '4':
                                case '5':
                                case '6':
                                case '7':
                                case '8':
                                case '9':
                                        p = atoif(--expr->exp_str, &eptr->ex_op, &i);
                        if (!p)
                        return (1);
                                        if (i != ET_INT) {
                                                post("expr: syntax error: %s\n", expr->exp_str);
                                                return (1);
                                        }
                        expr->exp_str = p;
                                        eptr->ex_type = ET_INT;
                                        eptr->ex_int = 0;
                        return (0);
                default:
                        post("expr: syntax error: %s\n", &expr->exp_str[-2]);
                        return (1);
                }
                p = atoif(expr->exp_str, &eptr->ex_op, &i);
                if (!p) {
                        post("expr: syntax error: %s\n", &expr->exp_str[-2]);
                        return (1);
                }
                if (i != ET_INT) {
                        post("expr: syntax error: %s\n", expr->exp_str);
                        return (1);
                }
                /*
                 * make the user inlets one based rather than zero based
                 * therefore we decrement the number that user has supplied
                 */
                if (!eptr->ex_op || (eptr->ex_op)-- > EX_MAX_INLETS) {
                 post("expr: syntax error: inlet or outlet out of range: %s\n",
                                                             expr->exp_str);
                        return (1);
                }

                /*
                 * until we can change the input type of inlets on
                 * the fly (at pd_new()
                 * time) the first input to expr~ is always a vector
                 * and $f1 or $i1 is
                 * illegal for fexpr~
                 */
                if (eptr->ex_op == 0 &&
                   (IS_FEXPR_TILDE(expr) ||  IS_EXPR_TILDE(expr)) &&
                   (eptr->ex_type==ET_II || eptr->ex_type==ET_FI ||
                                                        eptr->ex_type==ET_SI)) {
                       post("first inlet of expr~/fexpr~ can only be a vector");
                       return (1);
                }
                /* record the inlet or outlet type and check for consistency */
                if (eptr->ex_type == ET_YO ) {
                        /* it is an outlet  for fexpr~*/
                        /* no need to do anything */
                        ;
                } else if (!expr->exp_var[eptr->ex_op].ex_type)
                        expr->exp_var[eptr->ex_op].ex_type = eptr->ex_type;
                else if (expr->exp_var[eptr->ex_op].ex_type != eptr->ex_type) {
                        post("expr: syntax error: inlets can only have one type: %s\n", expr->exp_str);
                        return (1);
                }
                expr->exp_str = p;
noinletnum:
                break;

        case '"':
        {
                struct ex_ex ex = { 0 };
                char *savestrp;

                p = expr->exp_str;
                /*
                 * Before strings were supported a string inlet for functions
                 * such as size(), avg(), etc had the hacky syntax as
                 *   size ("$s2")
                 * all other inlet in strings such as 'size ("$f1")'
                 * were considered syntax error
                 * Now that strings are processed fully, the quotes are
                 * no longer necessary, however, in order to keep a backward
                 * compatibility we process func("$s#") as func($s#) with
                 * a message to the user as this will become deprecated
                 */
                if (*p == '$' && p[1] == 's') {
                        /* check for possible backward compatibilty */
                        savestrp = expr->exp_str;
                        if (getoken(expr, &ex))
                                return (1);
                        if (ex.ex_type == ET_SI && *expr->exp_str++ == '"') {
                                /* this is the old case */
                                *eptr = ex;
                                ex_error(expr, "expr: deprecated old func(\"$s#\") format detected;\nDouble quotes should no longer be used\nUse func($s#) instead");
                                break;
                        }
                        /*
                         * not in compatibility mode
                         * parse as usual
                         */
                        expr->exp_str = savestrp;
                        p = expr->exp_str;
                }

                i = 0;
                while (*p != '"') {
                        if (!*p) { /* missing close quote */
                                post("expr: syntax error: missing '\"': \n", expr->exp_str - 1);
                                return (1);
                        }
                        i++;
                        p++;
                }
                tmpstr= calloc(i + 1, sizeof (char));
                if (!tmpstr) {
                        post("expr: no memory %s: %d\n", __FILE__, __LINE__);
                        return (1);
                }
                strncpy(tmpstr, expr->exp_str, i);
                expr->exp_str= p + 1;
                /*
                 * ET_SYM in the expr string are converted to symbols,
                 * to avoid the symbol generation in eval
                 */
                if (ex_getsym(tmpstr, (t_symbol **)&(eptr->ex_ptr))) {
                        fts_free(tmpstr);
                        post("expr: syntax error: getoken: problms with ex_getsym %s:%d\n" __FILE__, __LINE__);
                        return (1);
                 }
                fts_free(tmpstr);
                eptr->ex_type = ET_SYM;
                /* not a temporary symbol; do not free this ET_SYM expression during eval*/
                eptr->ex_flags &= ~EX_F_TSYM;
                break;

        }
        case '.':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
                p = atoif(--expr->exp_str, &eptr->ex_int, &eptr->ex_type);
                if (!p)
                        return (1);
                expr->exp_str = p;
                break;

        default:
                /*
                 * has to be a string, it should either be a
                 * function or a table
                 */
                p = --expr->exp_str;
                for (i = 0; name_ok(*p); i++)
                        p++;
                if (!i) {
                        post("expr: syntax error: %s\n", expr->exp_str);
                        return (1);
                }
                eptr->ex_ptr = (char *)fts_malloc(i + 1);
                strncpy(eptr->ex_ptr, expr->exp_str, (int) i);
                (eptr->ex_ptr)[i] = 0;
                expr->exp_str = p;
                /*
                 * we mark this as a string and later we will change this
                 * to either a function or a table
                 */
                eptr->ex_type = ET_STR;
                break;
        }
        return (0);
}

/*
 * atoif -- ascii to float or integer (strtof() understand exponential notations  ad strtod() understand hex numbers)
 */
char *
atoif(char *s, long int *value, long int *type)
{
        const char*s0 = s;
        char *p;
        long lval;
        float fval;

        lval = strtod(s, &p);
        fval = strtof(s, &p);
        if (lval != (int) fval) {
                *type = ET_FLT;
                *((t_float *) value) = fval;
                return (p);
        }
        while (s != p) {
                if (*s == 'x' || *s == 'X')
                                break;
                if (*s == '.' || *s == 'e' || *s == 'E') {
                        *type = ET_FLT;
                        *((t_float *) value) = fval;
                        return (p);
                }
                s++;
        }
        if(s0 == p)
            return 0;

        *type = ET_INT;
        *((t_int *) value) = lval;
        return (p);
}

/*
 * find_func -- returns a pointer to the found function structure
 *              otherwise it returns 0
 */
t_ex_func *
find_func(char *s)
{
        t_ex_func *f;

        for (f = ex_funcs; f->f_name; f++)
                if (!strcmp(f->f_name, s))
                        return (f);
        return ((t_ex_func *) 0);
}

/*
 * ex_error - print an error message
 */

void
ex_error(t_expr *e, const char *fmt, ...)
{
    char buf[1024];
    va_list args;

    post_error( (fts_object_t *) e, "expr: '%s'", e->exp_string);
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    post_error((fts_object_t *) e, "%s", buf);
    va_end(args);

}

/*
 * ex_print -- print an expression array
 */

void
ex_print(struct ex_ex *eptr)
{
        struct ex_ex *extmp;

#define post printf

        extmp = eptr->ex_end;
        while (eptr->ex_type && eptr != extmp) {
                switch (eptr->ex_type) {
                case ET_INT:
                        post("%ld ", eptr->ex_int);
                        break;
                case ET_FLT:
                        post("%f ", eptr->ex_flt);
                        break;
                case ET_STR:
                        post("%s ", eptr->ex_ptr);
                        break;
                case ET_TBL:
                        if (!eptr->ex_ptr) { /* special case of $# processing */
                                post("%s ", "$$");
                            break;
                        }
                            /* falls through */
                case ET_VAR:
                        post("%s ", ex_symname((fts_symbol_t )eptr->ex_ptr));
                        break;
                case ET_SYM:
                        if (eptr->ex_flags & EX_F_TSYM) {
                                post("\"%s\"\n", eptr->ex_ptr);
                                /* temporary symbols (TSYM) are just one off */
                                return;
                        } else
                                post("\"%s\" ",
                                   ex_symname((fts_symbol_t )eptr->ex_ptr));
                        break;
                case ET_VSYM:
                        post("\"$s%ld\" ", eptr->ex_int + 1);
                        break;
                case ET_FUNC:
                        post("%s ",
                            ((t_ex_func *)eptr->ex_ptr)->f_name);
                        break;
                case ET_LP:
                        post("%c", '(');
                        break;
                        /* CHANGE
                 case ET_RP:
                         post("%c ", ')');
                         break;
                         */
                case ET_LB:
                        post("%c", '[');
                        break;
                        /* CHANGE
                case ET_RB:
                         post("%c ", ']');
                         break;
                         */
                case ET_II:
                        post("$i%ld ", eptr->ex_int + 1);
                        break;
                case ET_FI:
                        post("$f%ld ", eptr->ex_int + 1);
                        break;
                case ET_SI:
                        post("$s%lx ", (long) eptr->ex_ptr + 1);
                        break;
                case ET_VI:
                        post("$v%lx ", (long) eptr->ex_vec);
                        break;
                case ET_VEC:
                        post("vec = %ld ", (long) eptr->ex_vec);
                        break;
                case ET_YOM1:
                case ET_YO:
                        post("$y%ld", eptr->ex_int + 1);
                        break;
                case ET_XI:
                case ET_XI0:
                        post("$x%ld", eptr->ex_int + 1);
                        break;
                case ET_OP:
                        switch (eptr->ex_op) {
                        case OP_LP:
                                post("%c", '(');
                                break;
                        case OP_RP:
                                post("%c ", ')');
                                break;
                        case OP_LB:
                                post("%c", '[');
                                break;
                        case OP_RB:
                                post("%c ", ']');
                                break;
                        case OP_NOT:
                                post("%c", '!');
                                break;
                        case OP_NEG:
                                post("%c", '~');
                                break;
                        case OP_UMINUS:
                                post("%c", '-');
                                break;
                        case OP_MUL:
                                post("%c", '*');
                                break;
                        case OP_DIV:
                                post("%c", '/');
                                break;
                        case OP_MOD:
                                post("%c", '%');
                                break;
                        case OP_ADD:
                                post("%c", '+');
                                break;
                        case OP_SUB:
                                post("%c", '-');
                                break;
                        case OP_SL:
                                post("%s", "<<");
                                break;
                        case OP_SR:
                                post("%s", ">>");
                                break;
                        case OP_LT:
                                post("%c", '<');
                                break;
                        case OP_LE:
                                post("%s", "<=");
                                break;
                        case OP_GT:
                                post("%c", '>');
                                break;
                        case OP_GE:
                                post("%s", ">=");
                                break;
                        case OP_EQ:
                                post("%s", "==");
                                break;
                        case OP_STORE:
                                post("%s", "=");
                                break;
                        case OP_NE:
                                post("%s", "!=");
                                break;
                        case OP_AND:
                                post("%c", '&');
                                break;
                        case OP_XOR:
                                post("%c", '^');
                                break;
                        case OP_OR:
                                post("%c", '|');
                                break;
                        case OP_LAND:
                                post("%s", "&&");
                                break;
                        case OP_LOR:
                                post("%s", "||");
                                break;
                        case OP_COMMA:
                                post("%c", ',');
                                break;
                        case OP_SEMI:
                                post("%c", ';');
                                break;
                        default:
                                post("expr: ex_print: bad op 0x%lx\n", eptr->ex_op);
                        }
                        break;
                default:
                        post("expr: ex_print: bad type 0x%lx\n", eptr->ex_type);
                }
                eptr++;
        }
        post("\n");
}

#ifdef _WIN32
void ABORT(void) {bug("expr");}
#endif
