/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include <stdio.h>
#include <string.h>

    /* convenience routines for checking and getting values of
        atoms.  There's no "pointer" version since there's nothing
        safe to return if there's an error. */

t_float atom_getfloat(t_atom *a)
{
    if (a->a_type == A_FLOAT) return (a->a_w.w_float);
    else return (0);
}

t_int atom_getint(t_atom *a)
{
    return (atom_getfloat(a));
}

t_symbol *atom_getsymbol(t_atom *a)  /* LATER think about this more carefully */
{
    char buf[30];
    if (a->a_type == A_SYMBOL) return (a->a_w.w_symbol);
    else return (&s_float);
}

t_symbol *atom_gensym(t_atom *a)  /* this works  better for graph labels */
{
    char buf[30];
    if (a->a_type == A_SYMBOL) return (a->a_w.w_symbol);
    else if (a->a_type == A_FLOAT)
        sprintf(buf, "%g", a->a_w.w_float);
    else strcpy(buf, "???");
    return (gensym(buf));
}

t_float atom_getfloatarg(int which, int argc, t_atom *argv)
{
    if (argc <= which) return (0);
    argv += which;
    if (argv->a_type == A_FLOAT) return (argv->a_w.w_float);
    else return (0);
}

t_int atom_getintarg(int which, int argc, t_atom *argv)
{
    return (atom_getfloatarg(which, argc, argv));
}

t_symbol *atom_getsymbolarg(int which, int argc, t_atom *argv)
{
    if (argc <= which) return (&s_);
    argv += which;
    if (argv->a_type == A_SYMBOL) return (argv->a_w.w_symbol);
    else return (&s_);
}

/* convert an atom into a string, in the reverse sense of binbuf_text (q.v.)
* special attention is paid to symbols containing the special characters
* ';', ',', '$', and '\'; these are quoted with a preceding '\', except that
* the '$' only gets quoted at the beginning of the string.
*/

void atom_string(t_atom *a, char *buf, unsigned int bufsize)
{
    char tbuf[30];
    switch(a->a_type)
    {
    case A_SEMI: strcpy(buf, ";"); break;
    case A_COMMA: strcpy(buf, ","); break;
    case A_POINTER:
        strcpy(buf, "(pointer)");
        break;
    case A_FLOAT:
        sprintf(tbuf, "%g", a->a_w.w_float);
        if (strlen(tbuf) < bufsize-1) strcpy(buf, tbuf);
        else if (a->a_w.w_float < 0) strcpy(buf, "-");
        else  strcpy(buf, "+");
        break;
    case A_SYMBOL:
    {
        char *sp;
        unsigned int len;
        int quote;
        for (sp = a->a_w.w_symbol->s_name, len = 0, quote = 0; *sp; sp++, len++)
            if (*sp == ';' || *sp == ',' || *sp == '\\' ||
                (*sp == '$' && sp[1] >= '0' && sp[1] <= '9'))
                quote = 1;
        if (quote)
        {
            char *bp = buf, *ep = buf + (bufsize-2);
            sp = a->a_w.w_symbol->s_name;
            while (bp < ep && *sp)
            {
                if (*sp == ';' || *sp == ',' || *sp == '\\' ||
                    (*sp == '$' && sp[1] >= '0' && sp[1] <= '9'))
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
    case A_DOLLAR:
        sprintf(buf, "$%d", a->a_w.w_index);
        break;
    case A_DOLLSYM:
        strncpy(buf, a->a_w.w_symbol->s_name, bufsize);
        buf[bufsize-1] = 0;
        break;
    default:
        bug("atom_string");
    }
}
