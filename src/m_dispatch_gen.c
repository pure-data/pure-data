/*
This program generates the file m_dispatch.h.

Function pointers must be called at their exact type in Emscripten,
including return type, which makes the number of cases explode (42
instead of 6). This is not checked at compile time, but wrong usage
crashes at runtime. The naming pattern is t_funMN for M t_int args
and N t_floatarg args returning t_pd *, with t_vfunMN for the void-
returning variants. The versions with a t_pd * are for pd_objectmaker,
which returns the new object from the method, c.f. pd_newest support.
For the same reason, bang_new() and similar can't be private in
Emscripten, because their return type is different (as pd_objectmaker
methods) vs regular bang methods, so they need special treatment in the
dispatcher: the dispatcher checks explicitly for pd_objectmaker and
calls the method at the correct type.

This file only needs to be compiled and run if the object message
passing changes in the future. (Unlikely.)
To regenerate m_dispatch.h:
    cd src
    cc -o m_dispatch_gen m_dispatch_gen.c
    ./m_dispatch_gen[.exe] <maxargs>

NB: the <maxargs> argument should be at least MAXPDARGS+1 because the
receiver itself is passed as an additional t_int argument!
*/

#include <stdio.h>
#include <stdlib.h>

#define MAXMAXARGS 16

int main(int argc, char **argv)
{
    const char *rettype[2] = { "void ", "t_pd *" };
    int MAXARGS, ret, args, iargs, fargs, arg;
    FILE *o;
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s maxargs\n", argv[0]);
        return 1;
    }
    MAXARGS = atoi(argv[1]);
    if (MAXARGS >= MAXMAXARGS)
    {
        fprintf(stderr, "error: too many arguments for today\n");
        return 1;
    }
    o = fopen("m_dispatch.h", "wb");
    if (!o)
        return 1;

    fprintf(o, "/* IMPORTANT: EDIT m_dispatch_gen.c INSTEAD OF THIS FILE */\n\n");
        /* function declarations */
    for (ret = 0; ret < 2; ++ret)
    {
        for (args = 0; args <= MAXARGS; ++args)
        {
            for (iargs = 0; iargs <= args; ++iargs)
            {
                fargs = args - iargs;
                fprintf(o, "typedef %s(*t_%sfun%d%d)(",
                    rettype[ret], ret ? "" : "v", iargs, fargs);
                if (args)
                {
                    for (arg = 0; arg < args; ++arg)
                    {
                        if (arg > 0)
                            fprintf(o, ", ");
                        if (arg < iargs)
                            fprintf(o, "t_int i%d", arg);
                        else
                            fprintf(o, "t_floatarg d%d", arg - iargs);
                    }
                }
                else
                    fprintf(o, "void");
                fprintf(o, ");\n");
            }
        }
    }
    fprintf(o, "\n");
        /* mess_dispatch() function */
    fprintf(o,
        "static void mess_dispatch(t_pd *x, t_gotfn fn, int niarg, t_int *ai,\n"
        "    int nfarg, t_floatarg *ad)\n"
        "{\n"
        "    if (x == &pd_objectmaker)\n"
        "    {\n"
        "        t_pd *ret;\n"
        "        switch (niarg * %d + nfarg)\n"
        "        {\n",
        MAXMAXARGS);
    for (args = 0; args <= MAXARGS; ++args)
    {
        for (iargs = 0; iargs <= args; ++iargs)
        {
            fargs = args - iargs;
            fprintf(o, "        case 0x%02x : ret = (*(t_fun%d%d)fn)(",
                iargs * MAXMAXARGS + fargs, iargs, fargs);
            for (arg = 0; arg < args; ++arg)
            {
                if (arg > 0)
                    fprintf(o, ", ");
                if (arg < iargs)
                    fprintf(o, "ai[%d]", arg);
                else
                    fprintf(o, "ad[%d]", arg - iargs);
            }
            fprintf(o, "); break;\n");
        }
    }
    fprintf(o,
        "        default : ret = 0; break;\n"
        "        }\n"
        "        pd_this->pd_newest = ret;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        switch (niarg * %d + nfarg)\n"
        "        {\n",
        MAXMAXARGS);
    for (args = 0; args <= MAXARGS; ++args)
    {
        for (iargs = 0; iargs <= args; ++iargs)
        {
            fargs = args - iargs;
            fprintf(o, "        case 0x%02x : (*(t_vfun%d%d)fn)(",
                iargs * MAXMAXARGS + fargs, iargs, fargs);
            for (arg = 0; arg < args; ++arg)
            {
                if (arg > 0)
                    fprintf(o, ", ");
                if (arg < iargs)
                    fprintf(o, "ai[%d]", arg);
                else
                    fprintf(o, "ad[%d]", arg - iargs);
            }
            fprintf(o, "); break;\n");
        }
    }
    fprintf(o,
        "        default : break;\n"
        "        }\n"
        "    }\n"
        "}\n");

    fclose(o);

    return 0;
}
