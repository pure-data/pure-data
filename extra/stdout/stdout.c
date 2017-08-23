/* stdout -- write messages to standard output.

  Copyright 2008 Miller Puckette
  BSD license; see README.txt in this distribution for details.
*/

#include "m_pd.h"
#include <stdio.h>
#include <string.h>
static t_class *stdout_class;

#define MODE_DEFAULT 0  /* default, FUDI style */
#define MODE_CR 1       /* newline-terminate messages and omit semicolons */
#define MODE_BIN 2      /* binary messages supplied bytewise from patch */
#define MODE_PDTILDE 3  /* binary atoms for subprocess of pd~ object */

typedef struct _stdout
{
    t_object x_obj;
    int x_mode; /* 0=FUDI; 1=printf (no terminating semicolon); -1=binary */
    int x_flush; /* fflush() stdout after each message */
} t_stdout;

static void *stdout_new(t_symbol*s, int argc, t_atom*argv)
{
    t_stdout *x = (t_stdout *)pd_new(stdout_class);
        /* for some reason in MS windows we have to flush standard out here -
        otherwise these outputs get out of order from the ones from pdsched.c
        over in ../pd~.  I'm guessing mingw (with its different C runtime
        support) will handle this correctly and so am only ifdeffing this on
        Microsoft C compiler.  It's an efficiency hit, possibly a serious
        one. */
#ifdef _MSC_VER
    x->x_flush = 1;
#endif

    while(argc--)
    {
        s = atom_getsymbol(argv++);
        if (gensym("-cr") == s)
        {
                /* No-semicolon mode */
            x->x_mode = MODE_CR;
        }
        else if ((gensym("-b") == s) || (gensym("-binary") == s))
        {
                /* Binary mode:
                   no extra characters (semicolons, CR,...) is appended
                 */
            x->x_mode = MODE_BIN;
        }
        else if ((gensym("-f") == s) || (gensym("-flush") == s))
        {
            x->x_flush = 1;
        }
        else if ((gensym("-nf") == s) || (gensym("-noflush") == s))
        {
            x->x_flush = 0;
        }
        else if (gensym("") != s)
        {
                /* unknown mode; ignore it */
        }
    }
    if (gensym("#pd_binary_stdio")->s_thing)
        x->x_mode = MODE_PDTILDE;
    return (x);
}

static void stdout_binary(t_stdout *x, int argc, t_atom *argv)
{
#define BUFSIZE 65535
    char buf[BUFSIZE];
    int i;
    if (argc>BUFSIZE)
        argc = BUFSIZE;
    for (i=0; i<argc; i++)
        ((unsigned char *)buf)[i] = atom_getfloatarg(i, argc, argv);
    buf[i>BUFSIZE?BUFSIZE:i] = 0;
    fwrite(buf, 1, argc, stdout);

    if (x->x_flush || !argc)
        fflush(stdout);
}

static void pd_tilde_putfloat(float f, FILE *fd)
{
    putc(A_FLOAT, fd);
    fwrite(&f, sizeof(f), 1, fd);
}

static void pd_tilde_putsymbol(t_symbol *s, FILE *fd)
{
    char *sp = s->s_name;
    putc(A_SYMBOL, fd);
    do
        putc(*sp, fd);
    while (*sp++);
}

static void stdout_anything(t_stdout *x, t_symbol *s, int argc, t_atom *argv)
{
    char msgbuf[MAXPDSTRING], *sp, *ep = msgbuf+MAXPDSTRING;
    if (x->x_mode == MODE_BIN)
    {
        if ((gensym("list") == s) || (gensym("float") == s) ||
            (gensym("bang") == s))
                stdout_binary(x, argc, argv);
        else
            pd_error(x,
 "stdout: only 'list' messages allowed in binary mode (got '%s')",
                s->s_name);
        return;
    }
    else if (x->x_mode == MODE_PDTILDE)
    {
        pd_tilde_putsymbol(s, stdout);
        for (; argc--; argv++)
        {
            if (argv->a_type == A_FLOAT)
                pd_tilde_putfloat(argv->a_w.w_float, stdout);
            else if (argv->a_type == A_SYMBOL)
                pd_tilde_putsymbol(argv->a_w.w_symbol, stdout);
        }
        putc(A_SEMI, stdout);
        if (x->x_flush)
            fflush(stdout);
        return;
    }
    msgbuf[0] = 0;
    strncpy(msgbuf, s->s_name, MAXPDSTRING);
    msgbuf[MAXPDSTRING-1] = 0;
    sp = msgbuf + strlen(msgbuf);
    while (argc--)
    {
        if (sp < ep-1)
            sp[0] = ' ', sp[1] = 0, sp++;
        atom_string(argv++, sp, ep-sp);
        sp += strlen(sp);
    }
    switch(x->x_mode) {
    case MODE_CR:
        printf("%s\n", msgbuf);
        break;
    default:
        printf("%s;\n", msgbuf);
    }
    if (x->x_flush)
        fflush(stdout);
}

static void stdout_free(t_stdout *x)
{
    fflush(stdout);
}

void stdout_setup(void)
{
    stdout_class = class_new(gensym("stdout"), (t_newmethod)stdout_new,
        (t_method)stdout_free, sizeof(t_stdout), 0, A_GIMME, 0);
    class_addanything(stdout_class, stdout_anything);
}
