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
} t_stdout;

static void *stdout_new(t_float fnonrepeat)
{
    t_stdout *x = (t_stdout *)pd_new(stdout_class);
    return (x);
}

static void stdout_anything(t_stdout *x, t_symbol *s, int argc, t_atom *argv)
{
    char msgbuf[MAXPDSTRING], *sp, *ep = msgbuf+MAXPDSTRING;
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
    printf("%s;\n", msgbuf);
        /* for some reason in MS windows we have to flush standard out here -
        otherwise these outputs get out of order from the ones from pdsched.c
        over in ../pd~.  I'm guessing mingw (with its different C runtime
        support) will handle this correctly and so am only ifdeffing this on
        Microsoft C compiler.  It's an efficiency hit, possibly a serious
        one. */
#ifdef _MSC_VER   
    fflush(stdout);
#endif
}

static void stdout_free(t_stdout *x)
{
    fflush(stdout);
}

void stdout_setup(void)
{
    stdout_class = class_new(gensym("stdout"), (t_newmethod)stdout_new,
        (t_method)stdout_free, sizeof(t_stdout), 0, 0);
    class_addanything(stdout_class, stdout_anything);
}
