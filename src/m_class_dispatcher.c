/*
This program generates the files
- m_class_dispatcher_1.h, containing typedefs
- m_class_dispatcher_2.h, containing the dispatcher

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
*/

#include <stdio.h>
#include <stdlib.h>

#define MAXMAXARGS 16

const char *warning =
  "/* IMPORTANT: EDIT m_class_dispatcher.c INSTEAD OF THIS FILE */\n";

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
  o = fopen("m_class_dispatcher_1.h", "w");
  if (! o)
  {
    return 1;
  }
  fprintf(o, warning);
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
            {
              fprintf(o, ", ");
            }
            if (arg < iargs)
            {
              fprintf(o, "t_int i%d", arg);
            }
            else
            {
              fprintf(o, "t_floatarg d%d", arg - iargs);
            }
          }
        }
        else
        {
          fprintf(o, "void");
        }
        fprintf(o, ");\n");
      }
    }
  }
  fclose(o);
  o = fopen("m_class_dispatcher_2.h", "w");
  if (! o)
  {
    return 1;
  }
  fprintf(o, warning);
  fprintf(o,
    "if (x == &pd_objectmaker)\n"
    "{\n"
    "    switch (niarg * %d + nfarg)\n"
    "    {\n",
    MAXMAXARGS);
  for (args = 0; args <= MAXARGS; ++args)
  {
    for (iargs = 0; iargs <= args; ++iargs)
    {
      fargs = args - iargs;
      fprintf(o, "    case 0x%02x : bonzo = (*(t_fun%d%d)(m->me_fun))(",
        iargs * MAXMAXARGS + fargs, iargs, fargs);
      for (arg = 0; arg < args; ++arg)
      {
        if (arg > 0)
        {
          fprintf(o, ", ");
        }
        if (arg < iargs)
        {
          fprintf(o, "ai[%d]", arg);
        }
        else
        {
          fprintf(o, "ad[%d]", arg - iargs);
        }
      }
      fprintf(o, "); break;\n");
    }
  }
  fprintf(o,
    "    default : bonzo = 0;\n"
    "    }\n"
    "    pd_this->pd_newest = bonzo;\n"
    "}\n"
    "else\n"
    "{\n"
    "    switch (niarg * %d + nfarg)\n"
    "    {\n",
    MAXMAXARGS);
  for (args = 0; args <= MAXARGS; ++args)
  {
    for (iargs = 0; iargs <= args; ++iargs)
    {
      fargs = args - iargs;
      fprintf(o, "    case 0x%02x : (*(t_vfun%d%d)(m->me_fun))(",
        iargs * MAXMAXARGS + fargs, iargs, fargs);
      for (arg = 0; arg < args; ++arg)
      {
        if (arg > 0)
        {
          fprintf(o, ", ");
        }
        if (arg < iargs)
        {
          fprintf(o, "ai[%d]", arg);
        }
        else
        {
          fprintf(o, "ad[%d]", arg - iargs);
        }
      }
      fprintf(o, "); break;\n");
    }
  }
  fprintf(o,
    "    default : ;\n"
    "    }\n"
    "}\n");
  fclose(o);
  return 0;
}
