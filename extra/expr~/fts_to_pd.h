/* fts_to_pd.h -- alias some fts names to compile in Pd.

copyright 1999 Miller Puckette;
permission is granted to use this file for any purpose.
*/


#define fts_malloc malloc
#define fts_calloc calloc
#define fts_free free
#define fts_realloc realloc
#define fts_atom_t t_atom
#define fts_object_t t_object
typedef t_symbol *fts_symbol_t;

#ifdef MSP
#define t_atom          Atom
#define t_symbol        Symbol
#define pd_new(x)       newobject(x);
#define pd_free(x)      freeobject(x);
#define t_outlet        void
#define t_binbuf        void
typedef t_class         *t_pd;
typedef float           t_floatarg;

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

void pd_error(void *object, char *fmt, ...);

#endif /* MSP */

#define post_error pd_error
#define fts_is_floatg(x) ((x)->a_type == A_FLOAT)

#define fts_new_symbol_copy gensym

#define fts_symbol_name(x) ((x)->s_name)
