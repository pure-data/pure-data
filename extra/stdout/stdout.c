/* stdout -- write messages to standard output.

  Copyright 2008 Miller Puckette
  BSD license; see README.txt in this distribution for details.
*/

#include "m_pd.h"
#include <stdio.h>
#include <string.h>
static t_class *stdout_class;

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
            x->x_mode = 1;
        }
        else if ((gensym("-b") == s) || (gensym("-binary") == s))
        {
                /* Binary mode:
                   no extra characters (semicolons, CR,...) is appended
                 */
            x->x_mode = -1;
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
    return (x);
}

static void stdout_binary(t_stdout *x, int argc, t_atom *argv)
{
#define BUFSIZE 65535
    char buf[BUFSIZE+1];
    int i;
    if(argc>BUFSIZE)
        argc = BUFSIZE;
    for (i=0; i<argc; i++)
        ((unsigned char *)buf)[i] = atom_getfloatarg(i, argc, argv);
    buf[i>BUFSIZE?BUFSIZE:i] = 0;
    printf("%s", buf);

    if (x->x_flush || !argc)
        fflush(stdout);
}

static void stdout_anything(t_stdout *x, t_symbol *s, int argc, t_atom *argv)
{
    char msgbuf[MAXPDSTRING], *sp, *ep = msgbuf+MAXPDSTRING;
    if (x->x_mode < 0)
    {
        if ((gensym("list") == s) || (gensym("float") == s) || (gensym("bang") == s))
            stdout_binary(x, argc, argv);
        else
            pd_error(x, "stdout: only 'list' messages allowed in binary mode (got '%s')", s->s_name);
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
    case 1:
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
