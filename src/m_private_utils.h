/* Copyright (c) 1997-1999 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* This file contains some small header-only utilities to be used by Pd's code. */

/* NOTE: this file is an implementation detail of Pd, not to be in externals */

#ifndef M_PRIVATE_UTILS_H
#define M_PRIVATE_UTILS_H

#ifndef PD_INTERNAL
# error m_private_utils.h is a PRIVATE header. do *not* use it in your externals
#endif

/* ------------------------------ atomics ----------------------------------- */

#ifdef _MSC_VER
/* NB: Visual Studio only added C11 atomics support in version 17.5 */
#if _MSC_VER >= 1935
#define HAVE_C11_ATOMICS
#endif
#elif __STDC_VERSION__ >= 201112L
#define HAVE_C11_ATOMICS
#endif

#ifdef HAVE_C11_ATOMICS
/* use C11 stdatomic if available. */
#include <stdatomic.h>
/* atomic_int is defined in <stdatomic.h> */
#define atomic_int_load atomic_load
#define atomic_int_store atomic_store
#define atomic_int_fetch_add atomic_fetch_add
#define atomic_int_fetch_sub atomic_fetch_sub
#define atomic_int_exchange atomic_exchange
#define atomic_int_compare_exchange atomic_compare_exchange_strong
#else
/* emulate C11 atomics with platform specific intrinsics */
#ifdef _MSC_VER
/* Win32 instrinsics */
#include <windows.h>
#define atomic_int volatile int
#define atomic_int_load(ptr) _InterlockedOr((volatile long *)(ptr), 0)
#define atomic_int_store(ptr, value) _InterlockedExchange((volatile long *)(ptr), value)
#define atomic_int_fetch_add(ptr, value) _InterlockedExchangeAdd((volatile long *)(ptr), value)
#define atomic_int_fetch_sub(ptr, value) _InterlockedExchangeAdd((volatile long *)(ptr), -(value))
#define atomic_int_exchange(ptr, value) _InterlockedExchange((volatile long *)(ptr), value)
static int atomic_int_compare_exchange(volatile int *ptr, int *expected, int desired)
{
    long old = _InterlockedCompareExchange((volatile long *)ptr, *expected, desired);
    if (old == *expected)
        return 1;
    else
    {
        *expected = old;
        return 0;
    }
}
#elif __GNUC__
/* GCC/Clang atomics */
#define atomic_int volatile int
#define atomic_int_load(ptr) __atomic_load_4(ptr, __ATOMIC_SEQ_CST)
#define atomic_int_store(ptr, value) __atomic_store_4(ptr, value, __ATOMIC_SEQ_CST)
#define atomic_int_fetch_add(ptr, value) __atomic_fetch_add(ptr, value, __ATOMIC_SEQ_CST)
#define atomic_int_fetch_sub(ptr, value) __atomic_fetch_sub(ptr, value, __ATOMIC_SEQ_CST)
#define atomic_int_exchange(ptr, value) __atomic_exchange_4(ptr, value, __ATOMIC_SEQ_CST)
#define atomic_int_compare_exchange(ptr, expected, desired) \
    __atomic_compare_exchange_4(ptr, expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#else
#error "compiler not supported"
#endif
#endif

/* --------------------------- stack allocation helpers --------------------- */
/* alloca helpers
 * - ALLOCA(type, array, nmemb, maxnmemb)
 *     allocates <nmemb> elements of <type> and points <array> to the newly
 *     allocated memory. if <nmemb> exceeds <maxnmemb>, allocation is done on
 *     the heap, otherwise on the stack
 * - FREEA(type, array, nmemb, maxnmemb)
 *     frees the <array> allocated by ALLOCA() (unless allocation was done on
 *     the heap)
 * if DONT_USE_ALLOCA is defined (and true), always allocates on the heap
 *
 * usage example:
 * {
 *   t_atom*outv;
 *   ALLOCA(t_atom, outv, argc, 100);
 *   // do something with 'outv'
 *   FREEA(t_atom, outv, argc, 100);
 * }
 */
# ifdef ALLOCA
#  undef ALLOCA
# endif
# ifdef FREEA
#  undef FREEA
# endif

#if DONT_USE_ALLOCA
/* heap versions */
# define ALLOCA(type, array, nmemb, maxnmemb) ((array) = (type *)getbytes((nmemb) * sizeof(type)))
# define FREEA(type, array, nmemb, maxnmemb) (freebytes((array), (nmemb) * sizeof(type)))

#else /* !DONT_USE_ALLOCA */
/* stack version (unless <nmemb> exceeds <maxnmemb>) */

# ifdef HAVE_ALLOCA_H
#  include <alloca.h> /* linux, mac, mingw, cygwin,... */
# elif defined _WIN32
#  include <malloc.h> /* MSVC or mingw on windows */
# else
#  include <stdlib.h> /* BSDs for example */
# endif

# define ALLOCA(type, array, nmemb, maxnmemb) ((array) = (type *)((nmemb) < (maxnmemb) ? \
            alloca((nmemb) * sizeof(type)) : getbytes((nmemb) * sizeof(type))))
# define FREEA(type, array, nmemb, maxnmemb) (                          \
        ((nmemb) < (maxnmemb) || (freebytes((array), (nmemb) * sizeof(type)), 0)))
#endif /* !DONT_USE_ALLOCA */


/* --------------------------- endianness helpers --------------------- */
#ifdef HAVE_MACHINE_ENDIAN_H
# include <machine/endian.h>
#elif defined HAVE_ENDIAN_H
# include <endian.h>
#endif

#ifdef __MINGW32__
# include <sys/param.h>
#endif

/* BSD has deprecated BYTE_ORDER in favour of _BYTE_ORDER
 * others might follow...
 */
#if !defined(BYTE_ORDER) && defined(_BYTE_ORDER)
# define BYTE_ORDER _BYTE_ORDER
#endif
#if !defined(LITTLE_ENDIAN) && defined(_LITTLE_ENDIAN)
# define LITTLE_ENDIAN _LITTLE_ENDIAN
#endif

#ifdef _MSC_VER
/* _MSVC lacks BYTE_ORDER and LITTLE_ENDIAN */
# if !defined(LITTLE_ENDIAN)
#  define LITTLE_ENDIAN 0x0001
# endif
# if !defined(BYTE_ORDER)
#  define BYTE_ORDER LITTLE_ENDIAN
# endif
#endif

#ifdef __mips
# if !defined(LITTLE_ENDIAN)
#  define LITTLE_ENDIAN 1234
# endif
# if !defined(BIG_ENDIAN)
#  define BIG_ENDIAN 4321
# endif
# if !defined(BYTE_ORDER)
#  if defined(MIPSEL)
#   define BYTE_ORDER LITTLE_ENDIAN
#  elif defined(MIPSEB)
#   define BYTE_ORDER BIG_ENDIAN
#  endif
# endif
#endif

#if !defined(BYTE_ORDER) || !defined(LITTLE_ENDIAN)
# if defined(__GNUC__) && defined(_XOPEN_SOURCE)
#  warning unable to detect endianness (continuing anyhow)
# else
#  error unable to detect endianness
# endif
#endif

#endif /* M_PRIVATE_UTILS_H */
