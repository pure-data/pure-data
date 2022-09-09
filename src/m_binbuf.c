/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */


#include <stdlib.h>
#include "m_pd.h"
#include "s_stuff.h"
#include "g_canvas.h"
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif
#include <string.h>
#include <stdarg.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

struct _binbuf
{
    int b_n;
    t_atom *b_vec;
};

t_binbuf *binbuf_new(void)
{
    t_binbuf *x = (t_binbuf *)t_getbytes(sizeof(*x));
    x->b_n = 0;
    x->b_vec = t_getbytes(0);
    return (x);
}

void binbuf_free(t_binbuf *x)
{
    t_freebytes(x->b_vec, x->b_n * sizeof(*x->b_vec));
    t_freebytes(x,  sizeof(*x));
}

t_binbuf *binbuf_duplicate(const t_binbuf *y)
{
    t_binbuf *x = (t_binbuf *)t_getbytes(sizeof(*x));
    x->b_n = y->b_n;
    x->b_vec = t_getbytes(x->b_n * sizeof(*x->b_vec));
    memcpy(x->b_vec, y->b_vec, x->b_n * sizeof(*x->b_vec));
    return (x);
}

void binbuf_clear(t_binbuf *x)
{
    x->b_vec = t_resizebytes(x->b_vec, x->b_n * sizeof(*x->b_vec), 0);
    x->b_n = 0;
}

    /* convert text to a binbuf */
void binbuf_text(t_binbuf *x, const char *text, size_t size)
{
    char buf[MAXPDSTRING+1], *bufp, *ebuf = buf+MAXPDSTRING;
    const char *textp = text, *etext = text+size;
    t_atom *ap;
    int nalloc = 16, natom = 0;
    binbuf_clear(x);
    if (!binbuf_resize(x, nalloc)) return;
    ap = x->b_vec;
    while (1)
    {
        int type;
            /* skip leading space */
        while ((textp != etext) && (*textp == ' ' || *textp == '\n'
            || *textp == '\r' || *textp == '\t')) textp++;
        if (textp == etext) break;
        if (*textp == ';') SETSEMI(ap), textp++;
        else if (*textp == ',') SETCOMMA(ap), textp++;
        else
        {
                /* it's an atom other than a comma or semi */
            char c;
            int floatstate = 0, slash = 0, lastslash = 0, dollar = 0;
            bufp = buf;
            do
            {
                c = *bufp = *textp++;
                lastslash = slash;
                slash = (c == '\\');

                if (floatstate >= 0)
                {
                    int digit = (c >= '0' && c <= '9'),
                        dot = (c == '.'), minus = (c == '-'),
                        plusminus = (minus || (c == '+')),
                        expon = (c == 'e' || c == 'E');
                    if (floatstate == 0)    /* beginning */
                    {
                        if (minus) floatstate = 1;
                        else if (digit) floatstate = 2;
                        else if (dot) floatstate = 3;
                        else floatstate = -1;
                    }
                    else if (floatstate == 1)   /* got minus */
                    {
                        if (digit) floatstate = 2;
                        else if (dot) floatstate = 3;
                        else floatstate = -1;
                    }
                    else if (floatstate == 2)   /* got digits */
                    {
                        if (dot) floatstate = 4;
                        else if (expon) floatstate = 6;
                        else if (!digit) floatstate = -1;
                    }
                    else if (floatstate == 3)   /* got '.' without digits */
                    {
                        if (digit) floatstate = 5;
                        else floatstate = -1;
                    }
                    else if (floatstate == 4)   /* got '.' after digits */
                    {
                        if (digit) floatstate = 5;
                        else if (expon) floatstate = 6;
                        else floatstate = -1;
                    }
                    else if (floatstate == 5)   /* got digits after . */
                    {
                        if (expon) floatstate = 6;
                        else if (!digit) floatstate = -1;
                    }
                    else if (floatstate == 6)   /* got 'e' */
                    {
                        if (plusminus) floatstate = 7;
                        else if (digit) floatstate = 8;
                        else floatstate = -1;
                    }
                    else if (floatstate == 7)   /* got plus or minus */
                    {
                        if (digit) floatstate = 8;
                        else floatstate = -1;
                    }
                    else if (floatstate == 8)   /* got digits */
                    {
                        if (!digit) floatstate = -1;
                    }
                }
                if (!lastslash && c == '$' && (textp != etext &&
                    textp[0] >= '0' && textp[0] <= '9'))
                        dollar = 1;
                if (!slash) bufp++;
                else if (lastslash)
                {
                    bufp++;
                    slash = 0;
                }
            }
            while (textp != etext && bufp != ebuf &&
                (slash || (*textp != ' ' && *textp != '\n' && *textp != '\r'
                    && *textp != '\t' &&*textp != ',' && *textp != ';')));
            *bufp = 0;
#if 0
            post("binbuf_text: buf %s", buf);
#endif
            if (floatstate == 2 || floatstate == 4 || floatstate == 5 ||
                floatstate == 8)
                    SETFLOAT(ap, atof(buf));
                /* LATER try to figure out how to mix "$" and "\$" correctly;
                here, the backslashes were already stripped so we assume all
                "$" chars are real dollars.  In fact, we only know at least one
                was. */
            else if (dollar)
            {
                if (buf[0] != '$')
                    dollar = 0;
                for (bufp = buf+1; *bufp; bufp++)
                    if (*bufp < '0' || *bufp > '9')
                        dollar = 0;
                if (dollar)
                    SETDOLLAR(ap, atoi(buf+1));
                else SETDOLLSYM(ap, gensym(buf));
            }
            else SETSYMBOL(ap, gensym(buf));
        }
        ap++;
        natom++;
        if (natom == nalloc)
        {
            if (!binbuf_resize(x, nalloc*2)) break;
            nalloc = nalloc * 2;
            ap = x->b_vec + natom;
        }
        if (textp == etext) break;
    }
    /* reallocate the vector to exactly the right size */
    binbuf_resize(x, natom);
}

    /* convert a binbuf to text; no null termination. */
void binbuf_gettext(const t_binbuf *x, char **bufp, int *lengthp)
{
    char *buf = getbytes(0), *newbuf;
    int length = 0;
    char string[MAXPDSTRING];
    const t_atom *ap;
    int indx;

    for (ap = x->b_vec, indx = x->b_n; indx--; ap++)
    {
        int newlength;
        if ((ap->a_type == A_SEMI || ap->a_type == A_COMMA) &&
                length && buf[length-1] == ' ') length--;
        atom_string(ap, string, MAXPDSTRING);
        newlength = length + (int)strlen(string) + 1;
        if (!(newbuf = resizebytes(buf, length, newlength))) break;
        buf = newbuf;
        strcpy(buf + length, string);
        length = newlength;
        if (ap->a_type == A_SEMI) buf[length-1] = '\n';
        else buf[length-1] = ' ';
    }
    if (length && buf[length-1] == ' ')
    {
        if ((newbuf = t_resizebytes(buf, length, length-1)))
        {
            buf = newbuf;
            length--;
        }
    }
    *bufp = buf;
    *lengthp = length;
}

/* LATER improve the out-of-space behavior below.  Also fix this so that
writing to file doesn't buffer everything together. */

void binbuf_add(t_binbuf *x, int argc, const t_atom *argv)
{
    int previoussize = x->b_n;
    int newsize = previoussize + argc, i;
    t_atom *ap;

    if (!binbuf_resize(x, newsize))
    {
        pd_error(0, "binbuf_addmessage: out of space");
        return;
    }
#if 0
    startpost("binbuf_add: ");
    postatom(argc, argv);
    endpost();
#endif
    for (ap = x->b_vec + previoussize, i = argc; i--; ap++)
        *ap = *(argv++);
}

#define MAXADDMESSV 100
void binbuf_addv(t_binbuf *x, const char *fmt, ...)
{
    va_list ap;
    t_atom arg[MAXADDMESSV], *at =arg;
    int nargs = 0;
    const char *fp = fmt;

    va_start(ap, fmt);
    while (1)
    {
        if (nargs >= MAXADDMESSV)
        {
            pd_error(0, "binbuf_addmessv: only %d allowed", MAXADDMESSV);
            break;
        }
        switch(*fp++)
        {
        case 'i': SETFLOAT(at, va_arg(ap, int)); break;
        case 'f': SETFLOAT(at, va_arg(ap, double)); break;
        case 's': SETSYMBOL(at, va_arg(ap, t_symbol *)); break;
        case ';': SETSEMI(at); break;
        case ',': SETCOMMA(at); break;
        default: goto done;
        }
        at++;
        nargs++;
    }
done:
    va_end(ap);
    binbuf_add(x, nargs, arg);
}

/* add a binbuf to another one for saving.  Semicolons and commas go to
symbols ";", "'",; and inside symbols, characters ';', ',' and '$' get
escaped.  LATER also figure out about escaping white space */

void binbuf_addbinbuf(t_binbuf *x, const t_binbuf *y)
{
    t_binbuf *z = binbuf_new();
    int i, fixit;
    t_atom *ap;
    binbuf_add(z, y->b_n, y->b_vec);
    for (i = 0, ap = z->b_vec; i < z->b_n; i++, ap++)
    {
        char tbuf[MAXPDSTRING];
        const char *s;
        switch (ap->a_type)
        {
        case A_FLOAT:
            break;
        case A_SEMI:
            SETSYMBOL(ap, gensym(";"));
            break;
        case A_COMMA:
            SETSYMBOL(ap, gensym(","));
            break;
        case A_DOLLAR:
            sprintf(tbuf, "$%d", ap->a_w.w_index);
            SETSYMBOL(ap, gensym(tbuf));
            break;
        case A_DOLLSYM:
            atom_string(ap, tbuf, MAXPDSTRING);
            SETSYMBOL(ap, gensym(tbuf));
            break;
        case A_SYMBOL:
            for (s = ap->a_w.w_symbol->s_name, fixit = 0; *s; s++)
                if (*s == ';' || *s == ',' || *s == '$' || *s == '\\')
                    fixit = 1;
            if (fixit)
            {
                atom_string(ap, tbuf, MAXPDSTRING);
                SETSYMBOL(ap, gensym(tbuf));
            }
            break;
        default:
            bug("binbuf_addbinbuf");
        }
    }

    binbuf_add(x, z->b_n, z->b_vec);
    binbuf_free(z);
}

void binbuf_addsemi(t_binbuf *x)
{
    t_atom a;
    SETSEMI(&a);
    binbuf_add(x, 1, &a);
}

/* Supply atoms to a binbuf from a message, making the opposite changes
from binbuf_addbinbuf.  The symbol ";" goes to a semicolon, etc. */

void binbuf_restore(t_binbuf *x, int argc, const t_atom *argv)
{
    int previoussize = x->b_n;
    int newsize = previoussize + argc, i;
    t_atom *ap;

    if (!binbuf_resize(x, newsize))
    {
        pd_error(0, "binbuf_restore: out of space");
        return;
    }

    for (ap = x->b_vec + previoussize, i = argc; i--; ap++)
    {
        if (argv->a_type == A_SYMBOL)
        {
            const char *str = argv->a_w.w_symbol->s_name, *str2;
            if (!strcmp(str, ";")) SETSEMI(ap);
            else if (!strcmp(str, ",")) SETCOMMA(ap);
            else
            {
                char buf[MAXPDSTRING], *sp1;
                const char *sp2, *usestr;
                int dollar = 0;
                if (strchr(str, '\\'))
                {
                    int slashed = 0;
                    for (sp1 = buf, sp2 = argv->a_w.w_symbol->s_name;
                        *sp2 && sp1 < buf + (MAXPDSTRING-1);
                            sp2++)
                    {
                        if (slashed)
                            *sp1++ = *sp2, slashed = 0;
                        else if (*sp2 == '\\')
                            slashed = 1;
                        else
                        {
                            if (*sp2 == '$' && sp2[1] >= '0' && sp2[1] <= '9')
                                dollar = 1;
                            *sp1++ = *sp2;
                            slashed = 0;
                        }
                    }
                    *sp1 = 0;
                    usestr = buf;
                }
                else usestr = str;
                if (dollar || (usestr== str && (str2 = strchr(usestr, '$')) &&
                    str2[1] >= '0' && str2[1] <= '9'))
                {
                    int dollsym = 0;
                    if (*usestr != '$')
                        dollsym = 1;
                    else for (str2 = usestr + 1; *str2; str2++)
                        if (*str2 < '0' || *str2 > '9')
                    {
                        dollsym = 1;
                        break;
                    }
                    if (dollsym)
                        SETDOLLSYM(ap, usestr == str ?
                            argv->a_w.w_symbol : gensym(usestr));
                    else
                    {
                        int dollar = 0;
                        sscanf(usestr + 1, "%d", &dollar);
                        SETDOLLAR(ap, dollar);
                    }
                }
                else SETSYMBOL(ap, usestr == str ?
                    argv->a_w.w_symbol : gensym(usestr));
                /* fprintf(stderr, "arg %s -> binbuf %s type %d\n",
                    argv->a_w.w_symbol->s_name, usestr, ap->a_type); */
            }
            argv++;
        }
        else *ap = *(argv++);
    }
}

void binbuf_print(const t_binbuf *x)
{
    int i, startedpost = 0, newline = 1;
    for (i = 0; i < x->b_n; i++)
    {
        if (newline)
        {
            if (startedpost) endpost();
            startpost("");
            startedpost = 1;
        }
        postatom(1, x->b_vec + i);
        if (x->b_vec[i].a_type == A_SEMI)
            newline = 1;
        else newline = 0;
    }
    if (startedpost) endpost();
}

int binbuf_getnatom(const t_binbuf *x)
{
    return (x->b_n);
}

t_atom *binbuf_getvec(const t_binbuf *x)
{
    return (x->b_vec);
}

int binbuf_resize(t_binbuf *x, int newsize)
{
    t_atom *new = t_resizebytes(x->b_vec,
        x->b_n * sizeof(*x->b_vec), newsize * sizeof(*x->b_vec));
    if (new)
        x->b_vec = new, x->b_n = newsize;
    return (new != 0);
}

int canvas_getdollarzero(void);

/* JMZ:
 * s points to the first character after the $
 * (e.g. if the org.symbol is "$1-bla", then s will point to "1-bla")
 * (e.g. org.symbol="hu-$1mu", s="1mu")
 * LATER: think about more complex $args, like ${$1+3}
 *
 * the return value holds the length of the $arg (in most cases: 1)
 * buf holds the expanded $arg
 *
 * if some error occurred, "-1" is returned
 *
 * e.g. "$1-bla" with list "10 20 30"
 * s="1-bla"
 * buf="10"
 * return value = 1; (s+1=="-bla")
 */
static int binbuf_expanddollsym(const char *s, char *buf, t_atom *dollar0,
    int ac, const t_atom *av, int tonew)
{
    int argno = (int)atol(s);
    int arglen = 0;
    const char *cs = s;
    char c = *cs;

    *buf=0;
    while (c && (c>='0') && (c<='9'))
    {
        c = *cs++;
        arglen++;
    }

    if (cs==s)      /* invalid $-expansion (like "$bla") */
    {
        sprintf(buf, "$");
        return 0;
    }
    else if (argno < 0 || argno > ac) /* undefined argument */
    {
        if (!tonew)
            return 0;
        sprintf(buf, "$%d", argno);
    }
    else        /* well formed; expand it */
    {
        const t_atom *dollarvalue = (argno ? &av[argno-1] : dollar0);
        if (dollarvalue->a_type == A_SYMBOL)
        {
            strncpy(buf, dollarvalue->a_w.w_symbol->s_name, MAXPDSTRING/2-1);
            buf[MAXPDSTRING/2-2] = 0;
        }
        else atom_string(dollarvalue, buf, MAXPDSTRING/2-1);
    }
    return (arglen-1);
}

/* expand any '$' variables in the symbol s.  "tonow" is set if this is in the
context of a message to create a new object; in this case out-of-range '$'
args become 0 - otherwise zero is returned and the caller has to check the
result. */
/* LATER remove the dependence on the current canvas for $0; should be another
argument. */
t_symbol *binbuf_realizedollsym(t_symbol *s, int ac, const t_atom *av,
    int tonew)
{
    char buf[MAXPDSTRING];
    char buf2[MAXPDSTRING];
    const char*str=s->s_name;
    char*substr;
    int next=0;
    t_atom dollarnull;
    SETFLOAT(&dollarnull, canvas_getdollarzero());
    buf2[0] = buf2[MAXPDSTRING-1] = 0;

    substr=strchr(str, '$');
    if (!substr || substr-str >= MAXPDSTRING)
        return (s);

    strncpy(buf2, str, (substr-str));
    buf2[substr-str] = 0;
    str=substr+1;

    while((next=binbuf_expanddollsym(str, buf, &dollarnull, ac, av, tonew))>=0)
    {
        /*
        * JMZ: i am not sure what this means, so i might have broken it
        * it seems like that if "tonew" is set and the $arg cannot be expanded
        * (or the dollarsym is in reality a A_DOLLAR)
        * 0 is returned from binbuf_realizedollsym
        * this happens, when expanding in a message-box, but does not happen
        * when the A_DOLLSYM is the name of a subpatch
        */
        if (!tonew && (0==next) && (0==*buf))
        {
            return 0; /* JMZ: this should mimic the original behaviour */
        }

        strncat(buf2, buf, MAXPDSTRING-strlen(buf2)-1);
        str+=next;
        substr=strchr(str, '$');
        if (substr)
        {
            unsigned long n = substr-str;
            if(n>MAXPDSTRING-strlen(buf2)-1) n=MAXPDSTRING-strlen(buf2)-1;
            strncat(buf2, str, n);
            str=substr+1;
        }
        else
        {
            strncat(buf2, str, MAXPDSTRING-strlen(buf2)-1);
            goto done;
        }
    }
done:
    return (gensym(buf2));
}

#define SMALLMSG 5
#define HUGEMSG 1000

#ifndef HAVE_ALLOCA     /* can work without alloca() but we never need it */
#define HAVE_ALLOCA 1
#endif

#ifdef HAVE_ALLOCA

#ifdef _WIN32
# include <malloc.h> /* MSVC or mingw on windows */
#elif defined(__linux__) || defined(__APPLE__) || defined(HAVE_ALLOCA_H)
# include <alloca.h> /* linux, mac, mingw, cygwin */
#else
# include <stdlib.h> /* BSDs for example */
#endif

#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)((n) < HUGEMSG ?  \
        alloca((n) * sizeof(t_atom)) : getbytes((n) * sizeof(t_atom))))
#define ATOMS_FREEA(x, n) ( \
    ((n) < HUGEMSG || (freebytes((x), (n) * sizeof(t_atom)), 0)))
#else
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)getbytes((n) * sizeof(t_atom)))
#define ATOMS_FREEA(x, n) (freebytes((x), (n) * sizeof(t_atom)))
#endif

void binbuf_eval(const t_binbuf *x, t_pd *target, int argc, const t_atom *argv)
{
    t_atom smallstack[SMALLMSG], *mstack, *msp;
    const t_atom *at = x->b_vec;
    int ac = x->b_n;
    int nargs, maxnargs = 0;
    t_pd *initial_target = target;

    if (ac <= SMALLMSG)
        mstack = smallstack;
    else
    {
#if 1
            /* count number of args in biggest message.  The weird
            treatment of "pd_objectmaker" is because when the message
            goes out to objectmaker, commas and semis are passed
            on as regular args (see below).  We're tacitly assuming here
            that the pd_objectmaker target can't come up via a named
            destination in the message, only because the original "target"
            points there. */
        if (target == &pd_objectmaker)
            maxnargs = ac;
        else
        {
            int i, j = (target ? 0 : -1);
            for (i = 0; i < ac; i++)
            {
                if (at[i].a_type == A_SEMI)
                    j = -1;
                else if (at[i].a_type == A_COMMA)
                    j = 0;
                else if (++j > maxnargs)
                    maxnargs = j;
            }
        }
        if (maxnargs <= SMALLMSG)
            mstack = smallstack;
        else ATOMS_ALLOCA(mstack, maxnargs);
#else
            /* just pessimistically allocate enough to hold everything
            at once.  This turned out to run slower in a simple benchmark
            I tried, perhaps because the extra memory allocation
            hurt the cache hit rate. */
        maxnargs = ac;
        ATOMS_ALLOCA(mstack, maxnargs);
#endif

    }
    msp = mstack;
    while (1)
    {
        t_pd *nexttarget;
            /* get a target. */
        while (!target)
        {
            t_symbol *s;
            while (ac && (at->a_type == A_SEMI || at->a_type == A_COMMA))
                ac--,  at++;
            if (!ac) break;
            if (at->a_type == A_DOLLAR)
            {
                if (at->a_w.w_index <= 0 || at->a_w.w_index > argc)
                {
                    pd_error(initial_target, "$%d: not enough arguments supplied",
                            at->a_w.w_index);
                    goto cleanup;
                }
                else if (argv[at->a_w.w_index-1].a_type != A_SYMBOL)
                {
                    pd_error(initial_target, "$%d: symbol needed as message destination",
                        at->a_w.w_index);
                    goto cleanup;
                }
                else s = argv[at->a_w.w_index-1].a_w.w_symbol;
            }
            else if (at->a_type == A_DOLLSYM)
            {
                if (!(s = binbuf_realizedollsym(at->a_w.w_symbol,
                    argc, argv, 0)))
                {
                    pd_error(initial_target, "$%s: not enough arguments supplied",
                        at->a_w.w_symbol->s_name);
                    goto cleanup;
                }
            }
            else s = atom_getsymbol(at);
            if (!(target = s->s_thing))
            {
                pd_error(initial_target, "%s: no such object ", s->s_name);
            cleanup:
                do at++, ac--;
                while (ac && at->a_type != A_SEMI);
                    /* LATER eat args until semicolon and continue */
                continue;
            }
            else
            {
                at++, ac--;
                break;
            }
        }
        if (!ac) break;
        nargs = 0;
        nexttarget = target;
        while (1)
        {
            t_symbol *s9;
            if (!ac) goto gotmess;
            switch (at->a_type)
            {
            case A_SEMI:
                    /* semis and commas in new message just get bashed to
                    a symbol.  This is needed so you can pass them to "expr." */
                if (target == &pd_objectmaker)
                {
                    SETSYMBOL(msp, gensym(";"));
                    break;
                }
                else
                {
                    nexttarget = 0;
                    goto gotmess;
                }
            case A_COMMA:
                if (target == &pd_objectmaker)
                {
                    SETSYMBOL(msp, gensym(","));
                    break;
                }
                else goto gotmess;
            case A_FLOAT:
            case A_SYMBOL:
                *msp = *at;
                break;
            case A_DOLLAR:
                if (at->a_w.w_index > 0 && at->a_w.w_index <= argc)
                    *msp = argv[at->a_w.w_index-1];
                else if (at->a_w.w_index == 0)
                    SETFLOAT(msp, canvas_getdollarzero());
                else
                {
                    if (target == &pd_objectmaker)
                        SETFLOAT(msp, 0);
                    else
                    {
                        pd_error(target, "$%d: argument number out of range",
                            at->a_w.w_index);
                        SETFLOAT(msp, 0);
                    }
                }
                break;
            case A_DOLLSYM:
                s9 = binbuf_realizedollsym(at->a_w.w_symbol, argc, argv,
                    target == &pd_objectmaker);
                if (!s9)
                {
                    pd_error(target, "%s: argument number out of range", at->a_w.w_symbol->s_name);
                    SETSYMBOL(msp, at->a_w.w_symbol);
                }
                else SETSYMBOL(msp, s9);
                break;
            default:
                bug("bad item in binbuf");
                goto broken;
            }
            msp++;
            ac--;
            at++;
            nargs++;
        }
    gotmess:
        if (nargs)
        {
            switch (mstack->a_type)
            {
            case A_SYMBOL:
                typedmess(target, mstack->a_w.w_symbol, nargs-1, mstack+1);
                break;
            case A_FLOAT:
                if (nargs == 1) pd_float(target, mstack->a_w.w_float);
                else pd_list(target, 0, nargs, mstack);
                break;
            case A_POINTER:
                if (nargs == 1) pd_pointer(target, mstack->a_w.w_gpointer);
                else pd_list(target, 0, nargs, mstack);
                break;
            default:
                bug("bad selector");
                break;
            }
        }
        msp = mstack;
        if (!ac) break;
        target = nexttarget;
        at++;
        ac--;
    }
broken:
    if (maxnargs > SMALLMSG)
         ATOMS_FREEA(mstack, maxnargs);
}

int binbuf_read(t_binbuf *b, const char *filename, const char *dirname, int crflag)
{
    long length;
    int fd;
    int readret;
    char *buf;
    char namebuf[MAXPDSTRING];

    if (*dirname)
        snprintf(namebuf, MAXPDSTRING-1, "%s/%s", dirname, filename);
    else
        snprintf(namebuf, MAXPDSTRING-1, "%s", filename);
    namebuf[MAXPDSTRING-1] = 0;

    if ((fd = sys_open(namebuf, 0)) < 0)
    {
        fprintf(stderr, "open: ");
        perror(namebuf);
        return (1);
    }
    if ((length = (long)lseek(fd, 0, SEEK_END)) < 0 || lseek(fd, 0, SEEK_SET) < 0
        || !(buf = t_getbytes(length)))
    {
        fprintf(stderr, "lseek: ");
        perror(namebuf);
        close(fd);
        return(1);
    }
    if ((readret = (int)read(fd, buf, length)) < length)
    {
        fprintf(stderr, "read (%d %ld) -> %d\n", fd, length, readret);
        perror(namebuf);
        close(fd);
        t_freebytes(buf, length);
        return(1);
    }
        /* optionally map carriage return to semicolon */
    if (crflag)
    {
        int i;
        for (i = 0; i < length; i++)
            if (buf[i] == '\n')
                buf[i] = ';';
    }
    binbuf_text(b, buf, length);

#if 0
    startpost("binbuf_read "); postatom(b->b_n, b->b_vec); endpost();
#endif

    t_freebytes(buf, length);
    close(fd);
    return (0);
}

    /* read a binbuf from a file, via the search patch of a canvas */
int binbuf_read_via_canvas(t_binbuf *b, const char *filename,
    const t_canvas *canvas, int crflag)
{
    int filedesc;
    char buf[MAXPDSTRING], *bufptr;
    if ((filedesc = canvas_open(canvas, filename, "",
        buf, &bufptr, MAXPDSTRING, 0)) < 0)
    {
        pd_error(0, "%s: can't open", filename);
        return (1);
    }
    else close (filedesc);
    if (binbuf_read(b, bufptr, buf, crflag))
        return (1);
    else return (0);
}

    /* old version */
int binbuf_read_via_path(t_binbuf *b, const char *filename, const char *dirname,
    int crflag)
{
    int filedesc;
    char buf[MAXPDSTRING], *bufptr;
    if ((filedesc = open_via_path(
        dirname, filename, "", buf, &bufptr, MAXPDSTRING, 0)) < 0)
    {
        pd_error(0, "%s: can't open", filename);
        return (1);
    }
    else close (filedesc);
    if (binbuf_read(b, bufptr, buf, crflag))
        return (1);
    else return (0);
}

#define WBUFSIZE 4096
static t_binbuf *binbuf_convert(const t_binbuf *oldb, int maxtopd);

    /* write a binbuf to a text file.  If "crflag" is set we suppress
    semicolons. */
int binbuf_write(const t_binbuf *x, const char *filename, const char *dir, int crflag)
{
    FILE *f = 0;
    char sbuf[WBUFSIZE], fbuf[MAXPDSTRING], *bp = sbuf, *ep = sbuf + WBUFSIZE;
    t_atom *ap;
    t_binbuf *y = 0;
    const t_binbuf *z = x;
    int indx;

    if (*dir)
        snprintf(fbuf, MAXPDSTRING-1, "%s/%s", dir, filename);
    else
        snprintf(fbuf, MAXPDSTRING-1, "%s", filename);
    fbuf[MAXPDSTRING-1] = 0;

    if (!strcmp(filename + strlen(filename) - 4, ".pat") ||
        !strcmp(filename + strlen(filename) - 4, ".mxt"))
    {
        y = binbuf_convert(x, 0);
        z = y;
    }

    if (!(f = sys_fopen(fbuf, "w")))
        goto fail;
    for (ap = z->b_vec, indx = z->b_n; indx--; ap++)
    {
        int length;
            /* estimate how many characters will be needed.  Printing out
            symbols may need extra characters for inserting backslashes. */
        if (ap->a_type == A_SYMBOL || ap->a_type == A_DOLLSYM)
            length = 80 + (int)strlen(ap->a_w.w_symbol->s_name);
        else length = 40;
        if (ep - bp < length)
        {
            if (fwrite(sbuf, bp-sbuf, 1, f) < 1)
                goto fail;
            bp = sbuf;
        }
        if ((ap->a_type == A_SEMI || ap->a_type == A_COMMA) &&
            bp > sbuf && bp[-1] == ' ') bp--;
        if (!crflag || ap->a_type != A_SEMI)
        {
            atom_string(ap, bp, (unsigned int)((ep-bp)-2));
            length = (int)strlen(bp);
            bp += length;
        }
        if (ap->a_type == A_SEMI)
        {
            *bp++ = '\n';
        }
        else
        {
            *bp++ = ' ';
        }
    }
    if (fwrite(sbuf, bp-sbuf, 1, f) < 1)
        goto fail;

    if (fflush(f) != 0)
        goto fail;

    if (y)
        binbuf_free(y);
    fclose(f);
    return (0);
fail:
    if (y)
        binbuf_free(y);
    if (f)
        fclose(f);
    return (1);
}

/* The following routine attempts to convert from max to pd or back.  The
max to pd direction is working OK but you will need to make lots of
abstractions for objects like "gate" which don't exist in Pd.  conversion
from Pd to Max hasn't been tested for patches with subpatches yet!  */

#define MAXSTACK 1000

#define ISSYMBOL(a, b) ((a)->a_type == A_SYMBOL && \
    !strcmp((a)->a_w.w_symbol->s_name, (b)))

static t_binbuf *binbuf_convert(const t_binbuf *oldb, int maxtopd)
{
    t_binbuf *newb = binbuf_new();
    t_atom *vec = oldb->b_vec;
    t_int n = oldb->b_n, nextindex, stackdepth = 0, stack[MAXSTACK] = {0},
        nobj = 0, gotfontsize = 0;
	int i;
    t_atom outmess[MAXSTACK], *nextmess;
    t_float fontsize = 10;
    if (!maxtopd)
        binbuf_addv(newb, "ss;", gensym("max"), gensym("v2"));
    for (nextindex = 0; nextindex < n; )
    {
        int endmess, natom;
        const char *first, *second, *third;
        for (endmess = (int)nextindex; endmess < n && vec[endmess].a_type != A_SEMI;
            endmess++)
                ;
        if (endmess == n) break;
        if (endmess == nextindex || endmess == nextindex + 1
            || vec[nextindex].a_type != A_SYMBOL ||
                vec[nextindex+1].a_type != A_SYMBOL)
        {
            nextindex = endmess + 1;
            continue;
        }
        natom = endmess - (int)nextindex;
        if (natom > MAXSTACK-10) natom = MAXSTACK-10;
        nextmess = vec + nextindex;
        first = nextmess->a_w.w_symbol->s_name;
        second = (nextmess+1)->a_w.w_symbol->s_name;
        if (maxtopd)
        {
                /* case 1: importing a ".pat" file into Pd. */

                /* dollar signs in file translate to symbols */
            for (i = 0; i < natom; i++)
            {
                if (nextmess[i].a_type == A_DOLLAR)
                {
                    char buf[100];
                    sprintf(buf, "$%d", nextmess[i].a_w.w_index);
                    SETSYMBOL(nextmess+i, gensym(buf));
                }
                else if (nextmess[i].a_type == A_DOLLSYM)
                {
                    char buf[100];
                    sprintf(buf, "%s", nextmess[i].a_w.w_symbol->s_name);
                    SETSYMBOL(nextmess+i, gensym(buf));
                }
            }
            if (!strcmp(first, "#N"))
            {
                if (!strcmp(second, "vpatcher"))
                {
                    if (stackdepth >= MAXSTACK)
                    {
                        pd_error(0, "stack depth exceeded: too many embedded patches");
                        return (newb);
                    }
                    stack[stackdepth] = nobj;
                    stackdepth++;
                    nobj = 0;
                    binbuf_addv(newb, "ssfffff;",
                        gensym("#N"), gensym("canvas"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            atom_getfloatarg(4, natom, nextmess) -
                                atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(5, natom, nextmess) -
                                atom_getfloatarg(3, natom, nextmess),
                            (t_float)sys_defaultfont);
                }
            }
            if (!strcmp(first, "#P"))
            {
                    /* drop initial "hidden" flag */
                if (!strcmp(second, "hidden"))
                {
                    nextmess++;
                    natom--;
                    second = (nextmess+1)->a_w.w_symbol->s_name;
                }
                if (natom >= 7 && !strcmp(second, "newobj")
                    && (ISSYMBOL(&nextmess[6], "patcher") ||
                        ISSYMBOL(&nextmess[6], "p")))
                {
                    binbuf_addv(newb, "ssffss;",
                        gensym("#X"), gensym("restore"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym("pd"), atom_getsymbolarg(7, natom, nextmess));
                    if (stackdepth) stackdepth--;
                    nobj = stack[stackdepth];
                    nobj++;
                }
                else if (!strcmp(second, "newex") || !strcmp(second, "newobj"))
                {
                    t_symbol *classname =
                        atom_getsymbolarg(6, natom, nextmess);
                    if (classname == gensym("trigger") ||
                        classname == gensym("t"))
                    {
                        for (i = 7; i < natom; i++)
                            if (nextmess[i].a_type == A_SYMBOL &&
                                nextmess[i].a_w.w_symbol == gensym("i"))
                                    nextmess[i].a_w.w_symbol = gensym("f");
                    }
                    if (classname == gensym("table"))
                        classname = gensym("TABLE");
                    SETSYMBOL(outmess, gensym("#X"));
                    SETSYMBOL(outmess + 1, gensym("obj"));
                    outmess[2] = nextmess[2];
                    outmess[3] = nextmess[3];
                    SETSYMBOL(outmess+4, classname);
                    for (i = 7; i < natom; i++)
                        outmess[i-2] = nextmess[i];
                    SETSEMI(outmess + natom - 2);
                    binbuf_add(newb, natom - 1, outmess);
                    nobj++;
                }
                else if (!strcmp(second, "message") ||
                    !strcmp(second, "comment"))
                {
                    SETSYMBOL(outmess, gensym("#X"));
                    SETSYMBOL(outmess + 1, gensym(
                        (strcmp(second, "message") ? "text" : "msg")));
                    outmess[2] = nextmess[2];
                    outmess[3] = nextmess[3];
                    for (i = 6; i < natom; i++)
                        outmess[i-2] = nextmess[i];
                    SETSEMI(outmess + natom - 2);
                    binbuf_add(newb, natom - 1, outmess);
                    nobj++;
                }
                else if (!strcmp(second, "button"))
                {
                    binbuf_addv(newb, "ssffs;",
                        gensym("#X"), gensym("obj"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym("bng"));
                    nobj++;
                }
                else if (!strcmp(second, "number") || !strcmp(second, "flonum"))
                {
                    binbuf_addv(newb, "ssff;",
                        gensym("#X"), gensym("floatatom"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess));
                    nobj++;
                }
                else if (!strcmp(second, "slider"))
                {
                    t_float inc = atom_getfloatarg(7, natom, nextmess);
                    if (inc <= 0)
                        inc = 1;
                    binbuf_addv(newb, "ssffsffffffsssfffffffff;",
                        gensym("#X"), gensym("obj"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym("vsl"),
                        atom_getfloatarg(4, natom, nextmess),
                        atom_getfloatarg(5, natom, nextmess),
                        atom_getfloatarg(6, natom, nextmess),
                        atom_getfloatarg(6, natom, nextmess)
                            + (atom_getfloatarg(5, natom, nextmess) - 1) * inc,
                        0., 0.,
                        gensym("empty"), gensym("empty"), gensym("empty"),
                        0., -8., 0., 8., -262144., -1., -1., 0., 1.);
                    nobj++;
                }
                else if (!strcmp(second, "toggle"))
                {
                    binbuf_addv(newb, "ssffs;",
                        gensym("#X"), gensym("obj"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym("tgl"));
                    nobj++;
                }
                else if (!strcmp(second, "inlet"))
                {
                    binbuf_addv(newb, "ssffs;",
                        gensym("#X"), gensym("obj"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym((natom > 5 ? "inlet~" : "inlet")));
                    nobj++;
                }
                else if (!strcmp(second, "outlet"))
                {
                    binbuf_addv(newb, "ssffs;",
                        gensym("#X"), gensym("obj"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym((natom > 5 ? "outlet~" : "outlet")));
                    nobj++;
                }
                else if (!strcmp(second, "user"))
                {
                    third = (nextmess+2)->a_w.w_symbol->s_name;
                    if (!strcmp(third, "hslider"))
                    {
                        t_float range = atom_getfloatarg(7, natom, nextmess);
                        t_float multiplier = atom_getfloatarg(8, natom, nextmess);
                        t_float offset = atom_getfloatarg(9, natom, nextmess);
                        binbuf_addv(newb, "ssffsffffffsssfffffffff;",
                                    gensym("#X"), gensym("obj"),
                                    atom_getfloatarg(3, natom, nextmess),
                                    atom_getfloatarg(4, natom, nextmess),
                                    gensym("hsl"),
                                    atom_getfloatarg(6, natom, nextmess),
                                    atom_getfloatarg(5, natom, nextmess),
                                    offset,
                                    range + offset,
                                    0., 0.,
                                    gensym("empty"), gensym("empty"), gensym("empty"),
                                    0., -8., 0., 8., -262144., -1., -1., 0., 1.);
                   }
                    else if (!strcmp(third, "uslider"))
                    {
                        t_float range = atom_getfloatarg(7, natom, nextmess);
                        t_float multiplier = atom_getfloatarg(8, natom, nextmess);
                        t_float offset = atom_getfloatarg(9, natom, nextmess);
                        binbuf_addv(newb, "ssffsffffffsssfffffffff;",
                                    gensym("#X"), gensym("obj"),
                                    atom_getfloatarg(3, natom, nextmess),
                                    atom_getfloatarg(4, natom, nextmess),
                                    gensym("vsl"),
                                    atom_getfloatarg(5, natom, nextmess),
                                    atom_getfloatarg(6, natom, nextmess),
                                    offset,
                                    range + offset,
                                    0., 0.,
                                    gensym("empty"), gensym("empty"), gensym("empty"),
                                    0., -8., 0., 8., -262144., -1., -1., 0., 1.);
                    }
                    else
                        binbuf_addv(newb, "ssffs;",
                                    gensym("#X"), gensym("obj"),
                                    atom_getfloatarg(3, natom, nextmess),
                                    atom_getfloatarg(4, natom, nextmess),
                                    atom_getsymbolarg(2, natom, nextmess));
                    nobj++;
                }
                else if (!strcmp(second, "connect")||
                    !strcmp(second, "fasten"))
                {
                    binbuf_addv(newb, "ssffff;",
                        gensym("#X"), gensym("connect"),
                        nobj - atom_getfloatarg(2, natom, nextmess) - 1,
                        atom_getfloatarg(3, natom, nextmess),
                        nobj - atom_getfloatarg(4, natom, nextmess) - 1,
                        atom_getfloatarg(5, natom, nextmess));
                }
            }
        }
        else        /* Pd to Max */
        {
            if (!strcmp(first, "#N"))
            {
                if (!strcmp(second, "canvas"))
                {
                    t_float x, y;
                    if (stackdepth >= MAXSTACK)
                    {
                        pd_error(0, "stack depth exceeded: too many embedded patches");
                        return (newb);
                    }
                    stack[stackdepth] = nobj;
                    stackdepth++;
                    nobj = 0;
                    if(!gotfontsize) { /* only the first canvas sets the font size */
                        fontsize = atom_getfloatarg(6, natom, nextmess);
                        gotfontsize = 1;
                    }
                    x = atom_getfloatarg(2, natom, nextmess);
                    y = atom_getfloatarg(3, natom, nextmess);
                    binbuf_addv(newb, "ssffff;",
                        gensym("#N"), gensym("vpatcher"),
                            x, y,
                            atom_getfloatarg(4, natom, nextmess) + x,
                            atom_getfloatarg(5, natom, nextmess) + y);
                }
            }
            if (!strcmp(first, "#X"))
            {
                if (natom >= 5 && !strcmp(second, "restore")
                    && (ISSYMBOL (&nextmess[4], "pd")))
                {
                    binbuf_addv(newb, "ss;", gensym("#P"), gensym("pop"));
                    SETSYMBOL(outmess, gensym("#P"));
                    SETSYMBOL(outmess + 1, gensym("newobj"));
                    outmess[2] = nextmess[2];
                    outmess[3] = nextmess[3];
                    SETFLOAT(outmess + 4, 50.*(natom-5));
                    SETFLOAT(outmess + 5, fontsize);
                    SETSYMBOL(outmess + 6, gensym("p"));
                    for (i = 5; i < natom; i++)
                        outmess[i+2] = nextmess[i];
                    SETSEMI(outmess + natom + 2);
                    binbuf_add(newb, natom + 3, outmess);
                    if (stackdepth) stackdepth--;
                    nobj = stack[stackdepth];
                    nobj++;
                }
                else if (!strcmp(second, "obj"))
                {
                    t_symbol *classname =
                        atom_getsymbolarg(4, natom, nextmess);
                    if (classname == gensym("inlet"))
                        binbuf_addv(newb, "ssfff;", gensym("#P"),
                            gensym("inlet"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            10. + fontsize);
                    else if (classname == gensym("inlet~"))
                        binbuf_addv(newb, "ssffff;", gensym("#P"),
                            gensym("inlet"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            10. + fontsize, 1.);
                    else if (classname == gensym("outlet"))
                        binbuf_addv(newb, "ssfff;", gensym("#P"),
                            gensym("outlet"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            10. + fontsize);
                    else if (classname == gensym("outlet~"))
                        binbuf_addv(newb, "ssffff;", gensym("#P"),
                            gensym("outlet"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            10. + fontsize, 1.);
                    else if (classname == gensym("bng"))
                        binbuf_addv(newb, "ssffff;", gensym("#P"),
                            gensym("button"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            atom_getfloatarg(5, natom, nextmess), 0.);
                    else if (classname == gensym("tgl"))
                        binbuf_addv(newb, "ssffff;", gensym("#P"),
                            gensym("toggle"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            atom_getfloatarg(5, natom, nextmess), 0.);
                    else if (classname == gensym("vsl"))
                        binbuf_addv(newb, "ssffffff;", gensym("#P"),
                            gensym("slider"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            atom_getfloatarg(5, natom, nextmess),
                            atom_getfloatarg(6, natom, nextmess),
                            (atom_getfloatarg(8, natom, nextmess) -
                                atom_getfloatarg(7, natom, nextmess)) /
                                    (atom_getfloatarg(6, natom, nextmess) == 1? 1 :
                                         atom_getfloatarg(6, natom, nextmess) - 1),
                            atom_getfloatarg(7, natom, nextmess));
                    else if (classname == gensym("hsl"))
                    {
                        t_float slmin = atom_getfloatarg(7, natom, nextmess);
                        t_float slmax = atom_getfloatarg(8, natom, nextmess);
                        binbuf_addv(newb, "sssffffffff;", gensym("#P"),
                            gensym("user"),
                            gensym("hslider"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            atom_getfloatarg(6, natom, nextmess),
                            atom_getfloatarg(5, natom, nextmess),
                            slmax - slmin + 1, /* range */
                            1.,            /* multiplier */
                            slmin,         /* offset */
                            0.);
                    }
                    else if ( (classname == gensym("trigger")) ||
                              (classname == gensym("t")) )
                    {
                        t_symbol *arg;
                        SETSYMBOL(outmess, gensym("#P"));
                        SETSYMBOL(outmess + 1, gensym("newex"));
                        outmess[2] = nextmess[2];
                        outmess[3] = nextmess[3];
                        SETFLOAT(outmess + 4, 50.*(natom-4));
                        SETFLOAT(outmess + 5, fontsize);
                        outmess[6] = nextmess[4];
                        for (i = 5; i < natom; i++) {
                            arg = atom_getsymbolarg(i, natom, nextmess);
                            if (arg == gensym("a"))
                                SETSYMBOL(outmess + i + 2, gensym("l"));
                            else if (arg == gensym("anything"))
                                SETSYMBOL(outmess + i + 2, gensym("l"));
                            else if (arg == gensym("bang"))
                                SETSYMBOL(outmess + i + 2, gensym("b"));
                            else if (arg == gensym("float"))
                                SETSYMBOL(outmess + i + 2, gensym("f"));
                            else if (arg == gensym("list"))
                                SETSYMBOL(outmess + i + 2, gensym("l"));
                            else if (arg == gensym("symbol"))
                                SETSYMBOL(outmess + i + 2, gensym("s"));
                            else
                                outmess[i+2] = nextmess[i];
                        }
                        SETSEMI(outmess + natom + 2);
                        binbuf_add(newb, natom + 3, outmess);
                    }
                    else
                    {
                        SETSYMBOL(outmess, gensym("#P"));
                        SETSYMBOL(outmess + 1, gensym("newex"));
                        outmess[2] = nextmess[2];
                        outmess[3] = nextmess[3];
                        SETFLOAT(outmess + 4, 50.*(natom-4));
                        SETFLOAT(outmess + 5, fontsize);
                        for (i = 4; i < natom; i++)
                            outmess[i+2] = nextmess[i];
                        if (classname == gensym("osc~"))
                            SETSYMBOL(outmess + 6, gensym("cycle~"));
                        SETSEMI(outmess + natom + 2);
                        binbuf_add(newb, natom + 3, outmess);
                    }
                    nobj++;

                }
                else if (!strcmp(second, "msg") ||
                    !strcmp(second, "text"))
                {
                    SETSYMBOL(outmess, gensym("#P"));
                    SETSYMBOL(outmess + 1, gensym(
                        (strcmp(second, "msg") ? "comment" : "message")));
                    outmess[2] = nextmess[2];
                    outmess[3] = nextmess[3];
                    SETFLOAT(outmess + 4, 50.*(natom-4));
                    SETFLOAT(outmess + 5, fontsize);
                    for (i = 4; i < natom; i++)
                        outmess[i+2] = nextmess[i];
                    SETSEMI(outmess + natom + 2);
                    binbuf_add(newb, natom + 3, outmess);
                    nobj++;
                }
                else if (!strcmp(second, "floatatom"))
                {
                    t_float width = atom_getfloatarg(4, natom, nextmess)*fontsize;
                    if(width<8) width = 150; /* if pd width=0, set it big */
                    binbuf_addv(newb, "ssfff;",
                        gensym("#P"), gensym("flonum"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        width);
                    nobj++;
                }
                else if (!strcmp(second, "connect"))
                {
                    binbuf_addv(newb, "ssffff;",
                        gensym("#P"), gensym("connect"),
                        nobj - atom_getfloatarg(2, natom, nextmess) - 1,
                        atom_getfloatarg(3, natom, nextmess),
                        nobj - atom_getfloatarg(4, natom, nextmess) - 1,
                        atom_getfloatarg(5, natom, nextmess));
                }
            }
        }
        nextindex = endmess + 1;
    }
    if (!maxtopd)
        binbuf_addv(newb, "ss;", gensym("#P"), gensym("pop"));
#if 0
    binbuf_write(newb, "import-result.pd", "/tmp", 0);
#endif
    return (newb);
}

/* LATER make this evaluate the file on-the-fly. */
/* LATER figure out how to log errors */
void binbuf_evalfile(t_symbol *name, t_symbol *dir)
{
    t_binbuf *b = binbuf_new();
    int import = !strcmp(name->s_name + strlen(name->s_name) - 4, ".pat") ||
        !strcmp(name->s_name + strlen(name->s_name) - 4, ".mxt");
    int dspstate = canvas_suspend_dsp();
        /* set filename so that new canvases can pick them up */
    glob_setfilename(0, name, dir);
    if (binbuf_read(b, name->s_name, dir->s_name, 0))
        pd_error(0, "%s: read failed; %s", name->s_name, strerror(errno));
    else
    {
            /* save bindings of symbols #N, #A (and restore afterward) */
        t_pd *bounda = gensym("#A")->s_thing, *boundn = s__N.s_thing;
        gensym("#A")->s_thing = 0;
        s__N.s_thing = &pd_canvasmaker;
        if (import)
        {
            t_binbuf *newb = binbuf_convert(b, 1);
            binbuf_free(b);
            b = newb;
        }
        binbuf_eval(b, 0, 0, 0);
            /* avoid crashing if no canvas was created by binbuf eval */
        if (s__X.s_thing && *s__X.s_thing == canvas_class)
            canvas_initbang((t_canvas *)(s__X.s_thing)); /* JMZ*/
        gensym("#A")->s_thing = bounda;
        s__N.s_thing = boundn;
    }
    glob_setfilename(0, &s_, &s_);
    binbuf_free(b);
    canvas_resume_dsp(dspstate);
}

    /* save a text object to a binbuf for a file or copy buf */
void binbuf_savetext(const t_binbuf *bfrom, t_binbuf *bto)
{
    int k, n = binbuf_getnatom(bfrom);
    const t_atom *ap = binbuf_getvec(bfrom);
    t_atom at;
    for (k = 0; k < n; k++)
    {
        if (ap[k].a_type == A_FLOAT ||
            (ap[k].a_type == A_SYMBOL &&
                !strchr(ap[k].a_w.w_symbol->s_name, ';') &&
                !strchr(ap[k].a_w.w_symbol->s_name, ',') &&
                !strchr(ap[k].a_w.w_symbol->s_name, '$')))
                    binbuf_add(bto, 1, &ap[k]);
        else
        {
            char buf[MAXPDSTRING+1];
            atom_string(&ap[k], buf, MAXPDSTRING);
            SETSYMBOL(&at, gensym(buf));
            binbuf_add(bto, 1, &at);
        }
    }
    binbuf_addsemi(bto);
}
