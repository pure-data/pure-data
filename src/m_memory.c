/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include <stdlib.h>
#include <string.h>
#include "m_pd.h"
#include "m_imp.h"

/* #define LOUD */
#ifdef LOUD
#include <stdio.h>
#endif

/* #define DEBUGMEM */
#ifdef DEBUGMEM
static int totalmem = 0;
#endif

void *getbytes(size_t nbytes)
{
    void *ret;
    if (nbytes < 1) nbytes = 1;
    ret = (void *)calloc(nbytes, 1);
#ifdef LOUD
    fprintf(stderr, "new  %lx %d\n", (int)ret, nbytes);
#endif /* LOUD */
#ifdef DEBUGMEM
    totalmem += nbytes;
#endif
    if (!ret)
        post("pd: getbytes() failed -- out of memory");
    return (ret);
}

void *getzbytes(size_t nbytes)  /* obsolete name */
{
    return (getbytes(nbytes));
}

void *copybytes(void *src, size_t nbytes)
{
    void *ret;
    ret = getbytes(nbytes);
    if (nbytes)
        memcpy(ret, src, nbytes);
    return (ret);
}

void *resizebytes(void *old, size_t oldsize, size_t newsize)
{
    void *ret;
    if (newsize < 1) newsize = 1;
    if (oldsize < 1) oldsize = 1;
    ret = (void *)realloc((char *)old, newsize);
    if (newsize > oldsize && ret)
        memset(((char *)ret) + oldsize, 0, newsize - oldsize);
#ifdef LOUD
    fprintf(stderr, "resize %lx %d --> %lx %d\n", (int)old, oldsize, (int)ret, newsize);
#endif /* LOUD */
#ifdef DEBUGMEM
    totalmem += (newsize - oldsize);
#endif
    if (!ret)
        post("pd: resizebytes() failed -- out of memory");
    return (ret);
}

void freebytes(void *fatso, size_t nbytes)
{
    if (nbytes == 0)
        nbytes = 1;
#ifdef LOUD
    fprintf(stderr, "free %lx %d\n", (int)fatso, nbytes);
#endif /* LOUD */
#ifdef DEBUGMEM
    totalmem -= nbytes;
#endif
    free(fatso);
}

#ifdef DEBUGMEM
#include <stdio.h>

void glob_foo(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    fprintf(stderr, "total mem %d\n", totalmem);
}
#endif
