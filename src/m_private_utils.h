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

#ifdef HAVE_CONFIG_H
/* autotools might put all the HAVE_... defines into "config.h" */
# include "config.h"
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

#if !defined(BYTE_ORDER) || !defined(LITTLE_ENDIAN)
# if defined(__GNUC__) && defined(_XOPEN_SOURCE)
#  warning unable to detect endianness (continuing anyhow)
# else
#  error unable to detect endianness
# endif
#endif


/* -------------------------MSVC compat defines --------------------- */
#ifdef _MSC_VER
# define snprintf _snprintf
#endif


#endif /* M_PRIVATE_UTILS_H */
