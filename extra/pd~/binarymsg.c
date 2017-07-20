#include <stdio.h>
#include "m_pd.h"

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

static int pd_tilde_getatom(t_atom *ap, FILE *fd)
{
    char buf[MAXPDSTRING];
    while (1)
    {
        int type = getc(fd), fill;
        float f;
        switch (type)
        {
        case EOF:
            return (0);
        case A_SEMI:
            SETSEMI(ap);
            return (1);
        case A_FLOAT:
            if (fread(&f, sizeof(f), 1, fd) >= 1)
            {
                SETFLOAT(ap, f);
                return (1);
            }
            else return (0);
        case A_SYMBOL:
            for (fill = 0; fill < MAXPDSTRING; fill++)
            {
                int c = getc(fd);
                if (c == EOF)
                    return (0);
                else buf[fill] = c;
                if (!c)
                {
                    SETSYMBOL(ap, gensym(buf));
                    return (1);
                }
            }
            return (0);
        }
    }
}
