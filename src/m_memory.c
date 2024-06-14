/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include <stdlib.h>
#include <string.h>
#include "m_pd.h"
#include "s_stuff.h"
#if (defined LOUD) || (defined DEBUGMEM)
# include <stdio.h>
#endif

/*-------------------------- heap allocation --------------------------------*/

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

void *copybytes(const void *src, size_t nbytes)
{
    void *ret;
    ret = getbytes(nbytes);
    if (nbytes && ret)
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
void glob_foo(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    fprintf(stderr, "total mem %d\n", totalmem);
}
#endif

/*-------------------------- pseudo-stack allocation --------------------------------*/

#define PAGESIZE 4096
#define STACK_ALIGNMENT 16
#define STACK_ALIGN(x) (((x) + (STACK_ALIGNMENT - 1)) & ~(STACK_ALIGNMENT - 1))
#define STACK_MAGIC 0xDEADBEEF

/* define this for debugging purposes */
/* #define DEBUG_STACK */

/* define this for additional safety; slightly slower and with some memory overhead */
/* #define SAFE_STACK */

#ifdef SAFE_STACK
typedef struct _memheader
{
    size_t size;
    char heap;
} t_memheader;

#define MEMHEADERSIZE STACK_ALIGNMENT
#endif

void pd_stack_init(void)
{
        /* align stack memory to page size.
         * LATER allow to set stack size with a command line option? */
    char *mem;
    size_t extrabytes = PAGESIZE - 1;
    size_t allocsize = PD_DEFSTACKSIZE + extrabytes;
    STUFF->st_stackmem = mem = getbytes(allocsize);
    STUFF->st_stackstart = (char *)(((size_t)mem + extrabytes) & ~extrabytes);
    STUFF->st_stackend = mem + allocsize;
    STUFF->st_stackptr = STUFF->st_stackstart;
#ifndef NDEBUG
        /* initialize memory with magic value */
    while (mem < STUFF->st_stackend)
    {
        uint32_t val = STACK_MAGIC;
        memcpy(mem, &val, sizeof(val));
        mem += sizeof(val);
    }
#endif
}

void pd_stack_cleanup(void)
{
    freebytes(STUFF->st_stackmem, (STUFF->st_stackend - STUFF->st_stackmem));
}

void *pd_stack_alloc(size_t nbytes)
{
    size_t allocsize = STACK_ALIGN(nbytes);
    char *ptr = STUFF->st_stackptr;
#ifdef SAFE_STACK
    t_memheader m = { nbytes, 0 };
    allocsize += MEMHEADERSIZE;
#endif
    if (allocsize > (STUFF->st_stackend - ptr))
    {
        /* out of memory: fallback to heap allocation */
#ifdef DEBUG_STACK
        fprintf(stderr, "pd_stack_alloc: heap allocate %d (%d) bytes\n",
                (int)nbytes, (int)allocsize);
#endif
#ifdef SAFE_STACK
        m.heap = 1;
#endif
        ptr = getbytes(allocsize);
    }
    else
    {
        STUFF->st_stackptr += allocsize;
#ifdef DEBUG_STACK
        fprintf(stderr, "pd_stack_alloc: allocate %d (%d) bytes\n"
            "  total usage: %d / %d bytes\n",
                (int)nbytes, (int)allocsize,
                (int)(STUFF->st_stackptr - STUFF->st_stackstart),
                (int)(STUFF->st_stackend - STUFF->st_stackstart));
#endif
    }
#ifdef SAFE_STACK
    memcpy(ptr, &m, sizeof(m));
    ptr += MEMHEADERSIZE;
#endif
    return ptr;
}

void pd_stack_free(void *x, size_t nbytes)
{
    size_t allocsize = STACK_ALIGN(nbytes);
#ifdef SAFE_STACK
    t_memheader *m = (t_memheader *)((char *)x - MEMHEADERSIZE);
    allocsize += MEMHEADERSIZE;
    if (m->size != nbytes)
    {
        bug("pd_stack_free: wrong size (%d, expected %d)", (int)nbytes, (int)m->size);
            /* bash to "real" alloc size */
        allocsize = STACK_ALIGN(m->size) + MEMHEADERSIZE;
    }
    if (!m->heap)
#else
        /* check if the pointer belongs to our pseudo-stack memory region.
         * NB: according to the C standard, relational pointer comparison
         * is undefined if the pointers do not point into the same object/array.
         * The integer cast gets rid of the undefined behavior, but it is still
         * implementation defined. In practice, the code below should work fine
         * on all major platforms; otherwise we'd just need to define SAFE_STACK. */
    if ((uintptr_t)x >= (uintptr_t)STUFF->st_stackstart &&
        (uintptr_t)x < (uintptr_t)STUFF->st_stackend)
#endif
    {
        if ((STUFF->st_stackptr -= allocsize) < STUFF->st_stackstart)
        {
            bug("pd_stack_free");
            STUFF->st_stackptr = STUFF->st_stackstart;
        }
#ifndef NDEBUG
        else /* fill reclaimed memory with magic number */
        {
            char *ptr = STUFF->st_stackptr;
            char *end = ptr + allocsize;
            while (ptr < end)
            {
                uint32_t val = STACK_MAGIC;
                memcpy(ptr, &val, sizeof(val));
                ptr += sizeof(val);
            }
        }
#endif
#ifdef SAFE_STACK
        if ((char *)m != STUFF->st_stackptr)
            bug("pd_stack_free: not the most recent allocation");
#endif
#ifdef DEBUG_STACK
        fprintf(stderr, "pd_stack_free: free %d (%d) bytes\n"
            "  total usage: %d / %d bytes\n",
                (int)nbytes, (int)allocsize,
                (int)(STUFF->st_stackptr - STUFF->st_stackstart),
                (int)(STUFF->st_stackend - STUFF->st_stackstart));
#endif
    }
    else /* has been allocated from the heap */
    {
#ifdef DEBUG_STACK
        fprintf(stderr, "pd_stack_free: heap free %d (%d) bytes\n",
            (int)nbytes, (int)allocsize);
#endif
#ifdef SAFE_STACK
        freebytes(m, allocsize);
#else
        freebytes(x, allocsize);
#endif
    }
}

int pd_stack_check(void)
{
    if (STUFF->st_stackptr == STUFF->st_stackstart)
        return 1;
    else
    {
        pd_error(0, "leaked %d bytes of stack memory!",
            (int)(STUFF->st_stackptr - STUFF->st_stackstart));
        return 0;
    }
}
