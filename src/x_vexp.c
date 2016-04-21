/* Copyright (c) IRCAM.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* "expr" was written by Shahrokh Yadegari c. 1989. -msp */
/* "expr~" and "fexpr~" conversion by Shahrokh Yadegari c. 1999,2000 */


/*
 * Feb 2002 -   added access to variables
 *              multiple expression support
 *              new short hand forms for fexpr~
 *                      now $y or $y1 = $y1[-1] and $y2 = $y2[-1]
 *              --sdy
 *
 * July 2002
 *              fixed bugs introduced in last changes in store and ET_EQ
 *              --sdy
 *
 * Oct 2015
 *                              $x[-1] was not equal $x1[-1], not accessing the previous block
 *                              (bug fix by Dan Ellis)
 */

/*
 * vexp.c -- a variable expression evaluator
 *
 * This modules implements an expression evaluator using the
 * operator-precedence parsing.  It transforms an infix expression
 * to a prefix stack ready to be evaluated.  The expression sysntax
 * is close to that of C.  There are a few operators that are not
 * supported and functions are also recognized.  Strings can be
 * passed to functions when they are quoted in '"'s. "[]" are implememted
 * as an easy way of accessing the content of tables, and the syntax
 * table_name[index].
 * Variables (inlets) are specified with the following syntax: $x#,
 * where x is either i(integers), f(floats), and s(strings); and #
 * is a digit that coresponds to the inlet number.  The string variables
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
#include <ctype.h>
#include "x_vexp.h"
#include <errno.h>
#ifdef MSP
#undef isdigit
#define isdigit(x)      (x >= '0' && x <= '9')
#endif

#ifdef _MSC_VER
#define strtof _atoldbl
#endif


char *atoif(char *s, long int *value, long int *type);

static struct ex_ex *ex_lex(struct expr *expr, long int *n);
struct ex_ex *ex_match(struct ex_ex *eptr, long int op);
struct ex_ex *ex_parse(struct expr *expr, struct ex_ex *iptr,
                                        struct ex_ex *optr, long int *argc);
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

struct ex_ex nullex;

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

        memset(expr->exp_var, 0, MAX_VARS * sizeof (*expr->exp_var));
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
                expr->exp_nexpr++;
                ret = ex_match(list, (long)0);
                if (expr->exp_nexpr > MAX_VARS)
                    /* we cannot exceed MAX_VARS '$' variables */
                {
                        post_error((fts_object_t *) expr,
                            "expr: too many variables (maximum %d allowed)",
                                MAX_VARS);
                        goto error;
                }
                if (!ret)               /* syntax error */
                        goto error;
                ret = ex_parse(expr,
                        list, expr->exp_stack[expr->exp_nexpr - 1], (long *)0);
                if (!ret)
                        goto error;
        }
        *ret = nullex;
        t_freebytes(exp_string, exp_strlen+1);
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
        struct ex_ex *exptr;
        long non = 0;           /* number of nodes */
        long maxnode = 0;

        list_arr = (struct ex_ex *)fts_malloc(sizeof (struct ex_ex) * MINODES);
        if (! list_arr) {
                post("ex_lex: no mem\n");
                return ((struct ex_ex *)0);
        }
        exptr = list_arr;
        maxnode = MINODES;

        while (8)
        {
                if (non >= maxnode) {
                        maxnode += MINODES;

                        list_arr = fts_realloc((void *)list_arr,
                                        sizeof (struct ex_ex) * maxnode);
                        if (!list_arr) {
                                post("ex_lex: no mem\n");
                                return ((struct ex_ex *)0);
                        }
                        exptr = &(list_arr)[non];
                }

                if (getoken(expr, exptr)) {
                        fts_free(list_arr);
                        return ((struct ex_ex *)0);
                }
                non++;

                if (!exptr->ex_type)
                        break;

                exptr++;
        }
        *n = non;

        return list_arr;
}

/*
 * ex_match -- this routine walks through the eptr and matches the
 *             perentheses and brackets, it also converts the function
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
                         * chain or is preceeded by anything except ')' and
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
                                        post("expr: syntax error: problms with ex_getsym\n");
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
 * ex_parse -- This function if called when we have already done some
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
        struct ex_ex savex;
        long pre = HI_PRE;
        long count;

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
        for (eptr = iptr, count = 0; eptr->ex_type; eptr++, count++)
                switch (eptr->ex_type) {
                case ET_SYM:
                case ET_VSYM:
                        if (!argc) {
                                post("expr: syntax error: symbols allowed for functions only\n");
                                ex_print(eptr);
                                return (exNULL);
                        }
                case ET_INT:
                case ET_FLT:
                case ET_II:
                case ET_FI:
                case ET_XI0:
                case ET_YOM1:
                case ET_VI:
                case ET_VAR:
                        if (!count && !eptr[1].ex_type) {
                                *optr++ = *eptr;
                                return (optr);
                        }
                        break;
                case ET_XI:
                case ET_YO:
                case ET_SI:
                case ET_TBL:
                        if (eptr[1].ex_type != ET_LB) {
                                post("expr: syntax error: brackets missing\n");
                                ex_print(eptr);
                                return (exNULL);
                        }
                        /* if this table is the only token, parse the table */
                        if (!count &&
                            !((struct ex_ex *) eptr[1].ex_ptr)[1].ex_type) {
                                savex = *((struct ex_ex *) eptr[1].ex_ptr);
                                *((struct ex_ex *) eptr[1].ex_ptr) = nullex;
                                *optr++ = *eptr;
                                lowpre = ex_parse(x, &eptr[2], optr, (long *)0);
                                *((struct ex_ex *) eptr[1].ex_ptr) = savex;
                                return(lowpre);
                        }
                        eptr = (struct ex_ex *) eptr[1].ex_ptr;
                        break;
                case ET_OP:
                        if (eptr->ex_op == OP_COMMA) {
                                if (!argc || !count || !eptr[1].ex_type) {
                                        post("expr: syntax error: illegal comma\n");
                                        ex_print(eptr[1].ex_type ? eptr : iptr);
                                        return (exNULL);
                                }
                        }
                        if (!eptr[1].ex_type) {
                                post("expr: syntax error: missing operand\n");
                                ex_print(iptr);
                                return (exNULL);
                        }
                        if ((eptr->ex_op & PRE_MASK) <= pre) {
                                pre = eptr->ex_op & PRE_MASK;
                                lowpre = eptr;
                        }
                        break;
                case ET_FUNC:
                        if (eptr[1].ex_type != ET_LP) {
                                post("expr: ex_parse: no parenthesis\n");
                                return (exNULL);
                        }
                        /* if this function is the only token, parse it */
                        if (!count &&
                            !((struct ex_ex *) eptr[1].ex_ptr)[1].ex_type) {
                                long ac;

                                if (eptr[1].ex_ptr == (char *) &eptr[2]) {
                                        post("expr: syntax error: missing argument\n");
                                        ex_print(eptr);
                                        return (exNULL);
                                }
                                ac = 0;
                                savex = *((struct ex_ex *) eptr[1].ex_ptr);
                                *((struct ex_ex *) eptr[1].ex_ptr) = nullex;
                                *optr++ = *eptr;
                                lowpre = ex_parse(x, &eptr[2], optr, &ac);
                                if (!lowpre)
                                        return (exNULL);
                                ac++;
                                if (ac !=
                                    ((t_ex_func *)eptr->ex_ptr)->f_argc){
                                        post("expr: syntax error: function '%s' needs %ld arguments\n",
                                            ((t_ex_func *)eptr->ex_ptr)->f_name,
                                            ((t_ex_func *)eptr->ex_ptr)->f_argc);
                                        return (exNULL);
                                }
                                *((struct ex_ex *) eptr[1].ex_ptr) = savex;
                                return (lowpre);
                        }
                        eptr = (struct ex_ex *) eptr[1].ex_ptr;
                        break;
                case ET_LP:
                case ET_LB:
                        if (!count &&
                            !((struct ex_ex *) eptr->ex_ptr)[1].ex_type) {
                                if (eptr->ex_ptr == (char *)(&eptr[1])) {
                                        post("expr: syntax error: empty '%s'\n",
                                            eptr->ex_type==ET_LP?"()":"[]");
                                        ex_print(eptr);
                                        return (exNULL);
                                }
                                savex = *((struct ex_ex *) eptr->ex_ptr);
                                *((struct ex_ex *) eptr->ex_ptr) = nullex;
                                lowpre = ex_parse(x, &eptr[1], optr, (long *)0);
                                *((struct ex_ex *) eptr->ex_ptr) = savex;
                                return (lowpre);
                        }
                        eptr = (struct ex_ex *)eptr->ex_ptr;
                        break;
                case ET_STR:
                default:
                        ex_print(eptr);
                        post("expr: ex_parse: type = 0x%lx\n", eptr->ex_type);
                        return (exNULL);
                }

        if (pre == HI_PRE) {
                post("expr: syntax error: missing operation\n");
                ex_print(iptr);
                return (exNULL);
        }
        if (count < 2) {
                post("expr: syntax error: mission operand\n");
                ex_print(iptr);
                return (exNULL);
        }
        if (count == 2) {
                if (lowpre != iptr) {
                        post("expr: ex_parse: unary operator should be first\n");
                        return (exNULL);
                }
                if (!unary_op(lowpre->ex_op)) {
                        post("expr: syntax error: not a uniary operator\n");
                        ex_print(iptr);
                        return (exNULL);
                }
                *optr++ = *lowpre;
                eptr = ex_parse(x, &lowpre[1], optr, argc);
                return (eptr);
        }
                /* this is the case of using unary operator as a binary opetator */
                if (count == 3 && unary_op(lowpre->ex_op)) {
                                post("expr: syntax error, missing operand before unary operator\n");
                                ex_print(iptr);
                                return (exNULL);
                }
        if (lowpre == iptr) {
                post("expr: syntax error: mission operand\n");
                ex_print(iptr);
                return (exNULL);
        }
        savex = *lowpre;
        *lowpre = nullex;
        if (savex.ex_op != OP_COMMA)
                *optr++ = savex;
        else
                (*argc)++;
        eptr = ex_parse(x, iptr, optr, argc);
        if (eptr) {
                eptr = ex_parse(x, &lowpre[1], eptr, argc);
                *lowpre = savex;
        }
        return (eptr);
}

/*
 * this is the devide zero check for a a non devide operator
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

        if (!expr->exp_error & EE_DZ) {
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
                post ("%s divide by zero detected", etype);
                expr->exp_error |= EE_DZ;
        }
}


/*
 * ex_eval -- evaluate the array of prefix expression
 *            ex_eval returns the pointer to the first unevaluated node
 *            in the array.  This is a recursive routine.
 */

/* SDY - potential memory leak
all the returns in this function need to be changed so that the code
ends up at the end to check for newly allocated right and left vectors which
need to be freed

look into the variable nullret
*/
struct ex_ex *
ex_eval(struct expr *expr, struct ex_ex *eptr, struct ex_ex *optr, int idx)
/* the expr object data pointer */
/* the operation stack */
/* the result pointer */
/* the sample numnber processed for fexpr~ */
{
        int i, j;
        t_float *lp, *rp, *op; /* left, right, and out pointer to vectors */
        t_float scalar;
        int nullret = 0;                /* did we have an error */
        struct ex_ex left, right;       /* left and right operands */

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
                        post_error((fts_object_t *) expr,
                              "expr: ex_eval: cannot turn string to vector\n");
                        return (exNULL);
                }
                *optr = *eptr;
                return (++eptr);
        case ET_II:
                if (eptr->ex_int == -1) {
                        post_error((fts_object_t *) expr,
                            "expr: ex_eval: inlet number not set\n");
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
        case ET_SI:
                return (eval_tab(expr, eptr, optr, idx));
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
                    "expr: ex_print: bad op 0x%x\n", (unsigned)eptr->ex_op);
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

        f = (t_ex_func *)(eptr++)->ex_ptr;
        if (!f || !f->f_name) {
                return (exNULL);
        }
        if (f->f_argc > MAX_ARGS) {
                post_error((fts_object_t *) expr, "expr: eval_func: asking too many arguments\n");
                return (exNULL);
        }

        for (i = 0; i < f->f_argc; i++) {
                args[i].ex_type = 0;
                args[i].ex_int = 0;
                eptr = ex_eval(expr, eptr, &args[i], idx);
        }
        (*f->f_func)(expr, f->f_argc, args, optr);
        for (i = 0; i < f->f_argc; i++) {
                if (args[i].ex_type == ET_VEC)
                        fts_free(args[i].ex_vec);
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
        struct ex_ex arg;
        struct ex_ex rval;
        struct ex_ex *retp;
        int isvalue;
        char *tbl = (char *) 0;
        char *var = (char *) 0;
        int badleft = 0;
        int notable = 0;

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
                if (!expr->exp_var[eptr->ex_int].ex_ptr) {
                        if (!(expr->exp_error & EE_NOTABLE)) {
                                post("expr: syntax error: no string for inlet %d",
                                                                                                                eptr->ex_int + 1);
                                post("expr: No more table errors will be reported");
                                post("expr: till the next reset");
                                expr->exp_error |= EE_NOTABLE;
                        }
                        badleft++;
                        post("Bad left value: ");
                        /* report Error */
                        ex_print(eptr);
                        retp = exNULL;
                        return (retp);
                } else {
                        tbl = (char *) expr->exp_var[eptr->ex_int].ex_ptr;
                }
                break;
        default:
                post("Bad left value: ");
                /* report Error */
                ex_print(eptr);
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
                (void)max_ex_tab_store(expr, (t_symbol *)tbl, &arg, &rval, optr);
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
        struct ex_ex arg;
        char *tbl = (char *) 0;
        int notable = 0;

        if (eptr->ex_type == ET_SI) {
                if (!expr->exp_var[eptr->ex_int].ex_ptr) {
                        if (!(expr->exp_error & EE_NOTABLE)) {
                                post("expr: syntax error: no string for inlet %d",
                                                                                                                        eptr->ex_int + 1);
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

        optr->ex_type = ET_INT;
        optr->ex_int = 0;
        if (!notable)
                (void)max_ex_tab(expr, (t_symbol *)tbl, &arg, optr);
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
        struct ex_ex arg;
        char *var = (char *) 0;
        int novar = 0;

        if (eptr->ex_type == ET_SI) {
                if (!expr->exp_var[eptr->ex_int].ex_ptr) {
                                if (!(expr->exp_error & EE_NOVAR)) {
                                        post("expr: syntax error: no string for inlet %d", eptr->ex_int + 1);
                                        post("expr: No more table errors will be reported");
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

        optr->ex_type = ET_INT;
        optr->ex_int = 0;
        if (!novar)
                (void)max_ex_var(expr, (t_symbol *)var, optr, idx);
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
        struct ex_ex arg;
        struct ex_ex *reteptr;
        int i = 0, j = 0;
        t_float fi = 0,         /* index in float */
              rem_i = 0;        /* remains of the float */
        char *tbl;

        arg.ex_type = 0;
        arg.ex_int = 0;
        reteptr = ex_eval(expr, eptr + 1, &arg, idx);
        if (arg.ex_type == ET_FLT) {
                fi = arg.ex_flt;                /* float index */
                i = (int) arg.ex_flt;           /* integer index */
                rem_i =  arg.ex_flt - i;        /* remains of integer */
        } else if (arg.ex_type == ET_INT) {
                fi = arg.ex_int;                /* float index */
                i = arg.ex_int;
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
                          post("expr: input vector index > 0, (vector x%d[%f])",
                                               eptr->ex_int + 1, i + rem_i);
                                post("fexpr~: index assumed to be = 0");
                                post("fexpr~: no error report till next reset");
                                ex_print(eptr);
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
                                post("expr: input vector index <  -VectorSize, (vector x%d[%f])", eptr->ex_int + 1, fi);
                                ex_print(eptr);
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
                                post("fexpr~: bad output index, (%f)", fi);
                                ex_print(eptr);
                                post("fexpr~: no error report till next reset");
                                post("fexpr~: index assumed to be = -1");
                        }
                        i = -1;
                }
                if (eptr->ex_int >= expr->exp_nexpr) {
                        post("fexpr~: $y%d illegal: not that many exprs",
                                                                eptr->ex_int);
                        optr->ex_flt = 0;
                        return (reteptr);
                }
                if (cal_sigidx(optr, i, rem_i, idx, expr->exp_vsize,
                             expr->exp_tmpres[eptr->ex_int],
                                                expr->exp_p_res[eptr->ex_int])) {
                        if (!(expr->exp_error & EE_BI_OUTPUT)) {
                                expr->exp_error |= EE_BI_OUTPUT;
                                post("fexpr~: bad output index, (%f)", fi);
                                ex_print(eptr);
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
        char *p;
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
                                if (isdigit(*expr->exp_str))
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
                                if (isdigit(*expr->exp_str))
                                        break;
                                /* for $y[] is a shorhand for $y1[] */
                                /* eptr->ex_int = 0; */
                                                                eptr->ex_op = 0;
                                                                expr->exp_var[eptr->ex_op].ex_type = eptr->ex_type;
                                goto noinletnum;
                        }
                        post("$y works only for fexpr~");
                                /*
                                 * allow $# for abstration argument substitution
                                 *  $1+1 is translated to 0+1 and in abstration substitution
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
                if (!eptr->ex_op || (eptr->ex_op)-- > MAX_VARS) {
                 post("expr: syntax error: inlet or outlet out of range: %s\n",
                                                             expr->exp_str);
                        return (1);
                }

                /*
                 * until we can change the input type of inlets on
                 * the fly (at pd_new()
                 * time) the first input to expr~ is always a vectore
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
                        struct ex_ex ex;

                        p = expr->exp_str;
                        if (!*expr->exp_str || *expr->exp_str == '"') {
                                post("expr: syntax error: empty symbol: %s\n", --expr->exp_str);
                                return (1);
                        }
                        if (getoken(expr, &ex))
                                return (1);
                        switch (ex.ex_type) {
                        case ET_STR:
                                if (ex_getsym(ex.ex_ptr, (t_symbol **)&(eptr->ex_ptr))) {
                                        post("expr: syntax error: getoken: problms with ex_getsym\n");
                                        return (1);
                                }
                                eptr->ex_type = ET_SYM;
                                break;
                        case ET_SI:
                                *eptr = ex;
                                eptr->ex_type = ET_VSYM;
                                break;
                        default:
                                post("expr: syntax error: bad symbol name: %s\n", p);
                                return (1);
                        }
                        if (*expr->exp_str++ != '"') {
                                post("expr: syntax error: missing '\"'\n");
                                return (1);
                        }
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
        *type = ET_INT;
        *((t_int *) value) = lval;
        return (p);
}



#ifdef notdef
/*
 * atoif -- ascii to float or integer (understands hex numbers also)
 */
char *
atoif(char *s, long int *value, long int *type)
{
        char *p;
        long int_val = 0;
        int flt = 0;
        t_float pos = 0;
        t_float flt_val = 0;
        int base = 10;

        p = s;
        if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
                base = 16;
                p += 2;
        }
        while (8) {
                switch (*p) {
                case '.':
                        if (flt || base != 10) {
                                post("expr: syntax error: %s\n", s);
                                return ((char *) 0);
                        }
                        flt++;
                        pos = 10;
                        flt_val = int_val;
                        break;
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
                        if (flt) {
                                flt_val += (*p - '0') / pos;
                                pos *= 10;
                        } else {
                                int_val *= base;
                                int_val += (*p - '0');
                        }
                        break;
                case 'e':
                                                if (base != 16) {
                                                        if (
                case 'a':
                case 'b':
                case 'c':
                case 'd':
                case 'f':
                        if (base != 16 || flt) {
                                post("expr: syntax error: %s\n", s);
                                return ((char *) 0);
                        }
                        int_val *= base;
                        int_val += (*p - 'a' + 10);
                        break;
                case 'E':
                case 'A':
                case 'B':
                case 'C':
                case 'D':
                case 'F':
                        if (base != 16 || flt) {
                                post("expr: syntax error: %s\n", s);
                                return ((char *) 0);
                        }
                        int_val *= base;
                        int_val += (*p - 'A' + 10);
                        break;
                default:
                        if (flt) {
                                *type = ET_FLT;
                                *((t_float *) value) = flt_val;
                        } else {
                                *type = ET_INT;
                                *value = int_val;
                        }
                        return (p);
                }
                p++;
        }
}
#endif

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
 * ex_print -- print an expression array
 */

void
ex_print(struct ex_ex *eptr)
{

        while (eptr->ex_type) {
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
                case ET_VAR:
                        post("%s ", ex_symname((fts_symbol_t )eptr->ex_ptr));
                        break;
                case ET_SYM:
                        post("\"%s\" ", ex_symname((fts_symbol_t )eptr->ex_ptr));
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
                        post("$s%lx ", eptr->ex_ptr);
                        break;
                case ET_VI:
                        post("$v%lx ", eptr->ex_vec);
                        break;
                case ET_VEC:
                        post("vec = %ld ", eptr->ex_vec);
                        break;
                case ET_YOM1:
                case ET_YO:
                        post("$y%d", eptr->ex_int + 1);
                        break;
                case ET_XI:
                case ET_XI0:
                        post("$x%d", eptr->ex_int + 1);
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
void ABORT( void) {bug("expr");}
#endif
