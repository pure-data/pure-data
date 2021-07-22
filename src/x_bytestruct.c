/* Copyright (c) 2021 IOhannes m zmölnig and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* The "bytestruct" object. */

#include "m_pd.h"
#include "s_utf8.h"
#include <string.h>

#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__FreeBSD_kernel__) \
    || defined(__OpenBSD__)
# include <machine/endian.h>
#endif

#if defined(__linux__) || defined(__CYGWIN__) || defined(__GNU__) ||    \
    defined(ANDROID)
# include <endian.h>
#endif


#ifdef __MINGW32__
# include <sys/param.h>
#endif

#ifdef _MSC_VER
/* _MSVC lacks BYTE_ORDER and LITTLE_ENDIAN */
# define LITTLE_ENDIAN 0x0001
# define BYTE_ORDER LITTLE_ENDIAN
/* and ssize_t */
# include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#if !defined(BYTE_ORDER) || !defined(LITTLE_ENDIAN)
# error No byte order defined
#endif

#define MARK() post("%s@%d:%s", __FILE__, __LINE__, __FUNCTION__)

/* float16 helpers
   taken from https://github.com/OGRECave/ogre/blob/master/OgreMain/include/OgreBitwise.h
   (be8c2a225ecae636c8e669a12129b603db6b0e3c)

   Copyright (c) 2000-2014 Torus Knot Software Ltd
   License: MIT

   these are not perfect, as they use trunc() rather than round(),
   unlike the python version...

   a simplified version of the float32->float16 can be found on
   https://stackoverflow.com/questions/3026441/float32-to-float16

   ```
   t_bytes_number32 f32;
   t_bytes_number16 f16;
   uint16_t tmp;
   f32.f=f;
   f16.u = (f32.u >> 31) << 5;
   tmp = (f32.u >> 23) & 0xff;
   tmp = (tmp - 0x70) & ((unsigned int)((int)(0x70 - tmp) >> 4) >> 27);
   f16.u = (f16.u | tmp) << 10;
   f16.u |= (f32.u >> 13) & 0x3ff;
   ```
*/

    /** Converts float in uint32 format to a half in uint16 format */
static uint16_t floatToHalfI(uint32_t i)
{
    int s =  (i >> 16) & 0x00008000;
    int e = ((i >> 23) & 0x000000ff) - (127 - 15);
    int m =   i        & 0x007fffff;

    if (e <= 0)
    {
        if (e < -10)
        {
            return 0;
        }
        m = (m | 0x00800000) >> (1 - e);

        return (uint16_t)(s | (m >> 13));
    }
    else if (e == 0xff - (127 - 15))
    {
        if (m == 0) /* Inf */
        {
            return (uint16_t)(s | 0x7c00);
        }
        else    /* NAN */
        {
            m >>= 13;
            return (uint16_t)(s | 0x7c00 | m | (m == 0));
        }
    }
    else
    {
        if (e > 30) /* Overflow */
        {
            return (uint16_t)(s | 0x7c00);
        }

        return (uint16_t)(s | (e << 10) | (m >> 13));
    }
}

    /* Converts a half in uint16 format to a float in uint32 format */
static uint32_t halfToFloatI(uint16_t y)
{
    int s = (y >> 15) & 0x00000001;
    int e = (y >> 10) & 0x0000001f;
    int m =  y        & 0x000003ff;

    if (e == 0)
    {
        if (m == 0) /* Plus or minus zero */
        {
            return s << 31;
        }
        else /* Denormalized number -- renormalize it */
        {
            while (!(m & 0x00000400))
            {
                m <<= 1;
                e -=  1;
            }

            e += 1;
            m &= ~0x00000400;
        }
    }
    else if (e == 31)
    {
        if (m == 0) /* Inf */
        {
            return (s << 31) | 0x7f800000;
        }
        else /* NaN */
        {
            return (s << 31) | 0x7f800000 | (m << 13);
        }
    }

    e = e + (127 - 15);
    m = m << 13;

    return (s << 31) | (e << 23) | m;
}


/* bitstruct helpers */

typedef enum {
    PAD=0,
    CHAR,
    SIGNED_CHAR,
    UNSIGNED_CHAR,
    BOOL,
    SIGNED_SHORT,
    UNSIGNED_SHORT,
    SIGNED_INT,
    UNSIGNED_INT,
    SIGNED_LONG,
    UNSIGNED_LONG,
    SIGNED_LONGLONG,
    UNSIGNED_LONGLONG,
    SIGNED_SIZE,
    UNSIGNED_SIZE,
    HALFFLOAT,
    FLOAT,
    DOUBLE,
    STRING,
    WSTRING,
    PASCALSTRING,
    POINTER,
    ILLEGAL
} t_structtype;

static char structtype2char(t_structtype t) {
    switch(t) {
    case PAD: return 'x';
    case CHAR: return 'c';
    case BOOL: return '?';
    case SIGNED_CHAR: return 'b';
    case UNSIGNED_CHAR: return 'B';
    case SIGNED_SHORT: return 'h';
    case UNSIGNED_SHORT: return 'H';
    case SIGNED_INT: return 'i';
    case UNSIGNED_INT: return 'I';
    case SIGNED_LONG: return 'l';
    case UNSIGNED_LONG: return 'L';
    case SIGNED_LONGLONG: return 'q';
    case UNSIGNED_LONGLONG: return 'Q';
    case SIGNED_SIZE: return 'n';
    case UNSIGNED_SIZE: return 'N';
    case HALFFLOAT: return 'e';
    case FLOAT: return 'f';
    case DOUBLE: return 'd';
    case STRING: return 's';
    case WSTRING: return 'S';
    case PASCALSTRING: return 'p';
    case POINTER: return 'P';
    default: break;
    }
    return 0;
}
static t_structtype char2structtype(char c) {
    switch(c) {
    case 'x': return PAD;
    case 'c': return CHAR;
    case '?': return BOOL;
    case 'b': return SIGNED_CHAR;
    case 'B': return UNSIGNED_CHAR;
    case 'h': return SIGNED_SHORT;
    case 'H': return UNSIGNED_SHORT;
    case 'i': return SIGNED_INT;
    case 'I': return UNSIGNED_INT;
    case 'l': return SIGNED_LONG;
    case 'L': return UNSIGNED_LONG;
    case 'q': return SIGNED_LONGLONG;
    case 'Q': return UNSIGNED_LONGLONG;
    case 'n': return SIGNED_SIZE;
    case 'N': return UNSIGNED_SIZE;
    case 'e': return HALFFLOAT;
    case 'f': return FLOAT;
    case 'd': return DOUBLE;
    case 's': return STRING;
    case 'S': return WSTRING;
    case 'p': return PASCALSTRING;
    case 'P': return POINTER;
    default: break;
    }
    return ILLEGAL;
}

/* size=0: evaluate at runtime */
/* size=-1: illegal (no UNSIGNED_SIZE, SIGNED_SIZE and POINTER in 'standard' mode) */
static const int structtype_size[][2] = {
        /* {standard, native} */
    {0, 0}, /* PAD */
    {1, sizeof(char)}, /* CHAR */
    {1, sizeof(signed char)}, /* SIGNED_CHAR */
    {1, sizeof(unsigned char)}, /* UNSIGNED_CHAR */
    {1, sizeof(char)}, /* BOOL */
    {2, sizeof(short)}, /* SHORT */
    {2, sizeof(unsigned short)}, /* UNSIGNED_SHORT */
    {4, sizeof(int)}, /* INT */
    {4, sizeof(unsigned int)}, /* UNSIGNED_INT */
    {4, sizeof(long)}, /* LONG */
    {4, sizeof(unsigned long)}, /* UNSIGNED_LONG */
    {8, sizeof(long long)}, /* LONGLONG */
    {8, sizeof(unsigned long long)}, /* UNSIGNED_LONGLONG */
    {-1, sizeof(ssize_t)}, /* SIGNED_SIZE */
    {-1, sizeof(size_t)}, /* UNSIGNED_SIZE */
    {2, 2}, /* HALFFLOAT */
    {4, sizeof(float)}, /* FLOAT */
    {8, sizeof(double)}, /* DOUBLE */
    {0, 0}, /* STRING */
    {0, 0}, /* WSTRING */
    {0, 0}, /* PASCALSTRING */
    {-1, sizeof(void*)}, /* POINTER */
    {-1, -1} /* ILLEGAL */
};

/* converter functions
   valid byte-memory is buf[0:size-1]
   returns 1 on success, 0 on error
*/
typedef int (*t_packfun)(const t_atom*inatom, unsigned char*outbuf, size_t outsize, int byteswap);
typedef int (*t_unpackfun)(const unsigned char*inbuf, size_t insize, t_atom*outatom, int byteswap);

typedef struct _bs_list {
    t_structtype type;
    t_packfun packfun; /* converter function: atom to bytes */
    t_unpackfun unpackfun; /* converter function: bytes to atom */
    size_t atomcount; /* number of atoms to consume */
    size_t bytecount; /* number of bytes to consume */

    struct _bs_list*next;
} t_bs_list;

typedef union {
    signed char s;
    unsigned char u;
    unsigned char b[1];
} t_bytes_number8;
typedef union {
    int16_t s;
    uint16_t u;
    unsigned char b[2];
} t_bytes_number16;
typedef union {
    float f;
    int32_t s;
    uint32_t u;
    unsigned char b[4];
} t_bytes_number32;
typedef union {
    double f;
    int64_t s;
    uint64_t u;
    unsigned char b[8];
} t_bytes_number64;
typedef union {
    const void*ptr;
    unsigned char bytes[8];
} t_bytes_pointer;

static int copy_bs(const unsigned char*src, unsigned char*dest, size_t len, int byteswap) {
    if(byteswap) {
        src += len-1;
        while(len--) {
            *dest++ = *src--;
        }
    } else {
        memcpy(dest, src, len);
    }
    return (int)len;
}
#define copy_value2outbuf()                             \
    copy_bs(value.b, outbuf, sizeof(value.b), byteswap)

/* pack converters: atom -> bytes */
static int bs_atom_pad(const t_atom*a, unsigned char*outbuf, size_t outsize, int byteswap) {
    (void)a;
    (void)byteswap;
    memset(outbuf, 0, outsize);
    return 1;
}
static int bs_atom_char(const t_atom*a, unsigned char*outbuf, size_t outsize, int byteswap) {
    (void)outsize;
    (void)byteswap;
    switch(a->a_type) {
    case A_FLOAT:
        *outbuf=(char)atom_getfloat(a);
        break;
    case A_SYMBOL:
        *outbuf=(char)atom_getsymbol(a)->s_name[0];
        break;
    default:
        return 01;
    }
    return 1;
}
static int bs_atom_bool(const t_atom*a, unsigned char*outbuf, size_t outsize, int byteswap) {
    (void)outsize;
    (void)byteswap;
    switch(a->a_type) {
    case A_FLOAT:
        *outbuf=(char)((atom_getfloat(a))!=0);
        break;
    case A_SYMBOL:
        *outbuf=(atom_getsymbol(a)!=gensym(""));
        break;
    default:
        return 0;
    }
    return 1;
}
static int bs_atom_float16(const t_atom*a, unsigned char*outbuf, size_t outsize, int byteswap) {
    t_bytes_number32 f32;
    t_bytes_number16 value;
    (void)outsize;

    switch(a->a_type) {
    case A_FLOAT:
        f32.f=atom_getfloat(a);
        break;
    default:
        return 0;
    }
    value.u = floatToHalfI(f32.u);
    copy_value2outbuf();
    return 1;
}

#define bs_atom_number(typename, unionname, member, type)               \
    static int bs_atom_##typename(const t_atom*a, unsigned char*outbuf, size_t outsize, int byteswap) { \
        t_bytes_##unionname value;                                      \
        (void)outsize;                                                  \
        switch(a->a_type) {                                             \
        case A_FLOAT:                                                   \
            value.member = (type)atom_getfloat(a);                      \
            break;                                                      \
        default:                                                        \
            return 0;                                                   \
        }                                                               \
        copy_value2outbuf();                                            \
        return 1;                                                       \
    }


bs_atom_number(int8, number8, s, signed char);
bs_atom_number(uint8, number8, u, unsigned char);
bs_atom_number(int16, number16, s, int16_t);
bs_atom_number(uint16, number16, u, uint16_t);
bs_atom_number(int32, number32, s, int32_t);
bs_atom_number(uint32, number32, u, uint32_t);
bs_atom_number(int64, number64, s, int64_t);
bs_atom_number(uint64, number64, u, uint64_t);
bs_atom_number(float32, number32, f, float);
bs_atom_number(float64, number64, f, double);

static int bs_atom_string(const t_atom*a, unsigned char*outbuf, size_t outsize, int byteswap) {
    const char*s=0;
    (void)byteswap;
    switch(a->a_type) {
    case A_SYMBOL:
        s=atom_getsymbol(a)->s_name;
        break;
    default:
        return 0;
    }
    memset(outbuf, 0, outsize);
    strncpy((char*)outbuf, s, outsize);
    return 1;
}
static int bs_atom_wstring(const t_atom*a, unsigned char*outbuf, size_t outsize, int byteswap) {
    uint16_t ucs2[MAXPDSTRING];
    size_t i;
    switch(a->a_type) {
    case A_SYMBOL:
        u8_utf8toucs2(ucs2, MAXPDSTRING, atom_getsymbol(a)->s_name, -1);
        break;
    default:
        return 0;
    }
    memset(outbuf, 0, outsize);
    if(outsize>=MAXPDSTRING)
        outsize=MAXPDSTRING-1;
    for(i=0; i<outsize; i++) {
        copy_bs((const unsigned char*)(ucs2+i), outbuf+2*i, 2, byteswap);
    }
    return 1;
}
static int bs_atom_pascalstring(const t_atom*a, unsigned char*outbuf, size_t outsize, int byteswap) {
    size_t len=0;
    const char*s=0;
    char*obuf=(char*)outbuf;
    (void)byteswap;
    switch(a->a_type) {
    case A_SYMBOL:
        s=atom_getsymbol(a)->s_name;
        break;
    default:
        return 0;
    }
    if(!outsize)
        return 1;
    memset(obuf, 0, outsize);
    len=strlen(s);
    if(len>=outsize)
        len=outsize-1;
    if(len>0xFF)
        len=0xFF;

    obuf[0] = (unsigned char)(len);
    strncpy(obuf+1, s, outsize-1);
    return 1;
}
static int bs_atom_pointer(const t_atom*a, unsigned char*outbuf, size_t outsize, int byteswap) {
    t_bytes_pointer value;
    (void)byteswap;
    switch(a->a_type) {
    case A_POINTER:
        value.ptr=a->a_w.w_gpointer;
        break;
    case A_SYMBOL:
        value.ptr=atom_getsymbol(a)->s_name;
        break;
    default:
        return 0;
    }
    memcpy(outbuf, value.bytes, outsize);
    return 1;
}

/* pack converters: bytes -> atom */
static int bs_pad_atom(const unsigned char*inbuf, size_t insize, t_atom*outatom, int byteswap) {
    (void)inbuf;
    (void)insize;
    (void)outatom;
    (void)byteswap;
    return 1;
}
static int bs_char_atom(const unsigned char*inbuf, size_t insize, t_atom*outatom, int byteswap) {
    char value = *inbuf;
    (void)insize;
    (void)byteswap;
    SETFLOAT(outatom, value);
    return 1;
}
static int bs_bool_atom(const unsigned char*inbuf, size_t insize, t_atom*outatom, int byteswap) {
    char value = *inbuf;
    (void)insize;
    (void)byteswap;
    SETFLOAT(outatom, value!=0);
    return 1;
}
#define bs_number_atom(typename, unionname, member, type)               \
    static int bs_##typename##_atom(const unsigned char*inbuf, size_t insize, t_atom *a, int byteswap) { \
        t_bytes_##unionname value;                                      \
        (void)insize;                                                   \
        copy_bs(inbuf, value.b, sizeof(value.b), byteswap);             \
        SETFLOAT(a, value.member);                                      \
        return 1;                                                       \
    }
bs_number_atom(int8, number8, s, signed char);
bs_number_atom(uint8, number8, u, unsigned char);
bs_number_atom(int16, number16, s, int16_t);
bs_number_atom(uint16, number16, u, uint16_t);
bs_number_atom(int32, number32, s, int32_t);
bs_number_atom(uint32, number32, u, uint32_t);
bs_number_atom(int64, number64, s, int64_t);
bs_number_atom(uint64, number64, u, uint64_t);
bs_number_atom(float32, number32, f, float);
bs_number_atom(float64, number64, f, double);


static int bs_float16_atom(const unsigned char*inbuf, size_t insize, t_atom*outatom, int byteswap) {
    t_bytes_number32 f32;
    t_bytes_number16 value;
    (void)insize;
    copy_bs(inbuf, value.b, sizeof(value.b), byteswap);
    f32.u = halfToFloatI(value.u);
    SETFLOAT(outatom, f32.f);
    return 1;
}
static int bs_string_atom(const unsigned char*inbuf, size_t insize, t_atom*outatom, int byteswap) {
    char s[MAXPDSTRING];
    (void)byteswap;
    if(insize>=MAXPDSTRING)
        insize=MAXPDSTRING-1;
    strncpy(s, (char*)inbuf, insize);
    s[insize]=0;

    SETSYMBOL(outatom, gensym(s));
    return 1;
}
static int bs_wstring_atom(const unsigned char*inbuf, size_t insize, t_atom*outatom, int byteswap) {
    char s[MAXPDSTRING];
    uint16_t ucs2[MAXPDSTRING];
    size_t i;
    if(insize>=MAXPDSTRING)
        insize=MAXPDSTRING-1;
    for(i=0; i<insize; i++) {
        copy_bs(inbuf+2*i, (unsigned char*)(ucs2+i), 2, byteswap);
    }
    u8_ucs2toutf8(s, MAXPDSTRING, ucs2, insize);
    s[insize]=0;

    SETSYMBOL(outatom, gensym(s));
    return 1;
}
static int bs_pascalstring_atom(const unsigned char*inbuf, size_t insize, t_atom*outatom, int byteswap) {
    size_t len = *inbuf++;
    char s[MAXPDSTRING];
    (void)byteswap;
    if(0xFF == len || len>insize)
        len = insize;
    if(len>=MAXPDSTRING)
        len=MAXPDSTRING-1;
    strncpy(s, (char*)inbuf, len);
    SETSYMBOL(outatom, gensym(s));
    return 1;
}
static int bs_pointer_atom(const unsigned char*inbuf, size_t insize, t_atom*outatom, int byteswap) {
    const t_int*data=(void*)inbuf;
    void*ptr = (void*)data[0];
    char s[MAXPDSTRING];
    (void)insize;
    (void)byteswap;
    sprintf(s, "%p", ptr);
    SETSYMBOL(outatom, gensym(s));
    return 1;
}

static t_packfun inttype2packfun(size_t bytecount, int issigned) {
    switch (bytecount) {
    case 1: return issigned?bs_atom_int8:bs_atom_uint8;
    case 2: return issigned?bs_atom_int16:bs_atom_uint16;
    case 4: return issigned?bs_atom_int32:bs_atom_uint32;
    case 8: return issigned?bs_atom_int64:bs_atom_uint64;
    default: break;
    }
    return 0;
}
static t_packfun floattype2packfun(size_t bytecount) {
    switch (bytecount) {
    case 2: return bs_atom_float16;
    case 4: return bs_atom_float32;
    case 8: return bs_atom_float64;
    default: break;
    }
    return 0;
}

static t_packfun type2packfun(t_structtype t, int native) {
    switch(t) {
    case PAD: return bs_atom_pad;
    case CHAR: return bs_atom_char;
    case BOOL: return bs_atom_bool;
    case SIGNED_CHAR: return inttype2packfun(structtype_size[t][native], 1);
    case UNSIGNED_CHAR: return inttype2packfun(structtype_size[t][native], 0);
    case SIGNED_SHORT: return inttype2packfun(structtype_size[t][native], 1);
    case UNSIGNED_SHORT: return inttype2packfun(structtype_size[t][native], 0);
    case SIGNED_INT: return inttype2packfun(structtype_size[t][native], 1);
    case UNSIGNED_INT: return inttype2packfun(structtype_size[t][native], 0);
    case SIGNED_LONG: return inttype2packfun(structtype_size[t][native], 1);
    case UNSIGNED_LONG: return inttype2packfun(structtype_size[t][native], 0);
    case SIGNED_LONGLONG: return inttype2packfun(structtype_size[t][native], 1);
    case UNSIGNED_LONGLONG: return inttype2packfun(structtype_size[t][native], 0);
    case SIGNED_SIZE: return inttype2packfun(structtype_size[t][native], 1);
    case UNSIGNED_SIZE: return inttype2packfun(structtype_size[t][native], 0);

    case HALFFLOAT: return floattype2packfun(structtype_size[t][native]);
    case FLOAT: return floattype2packfun(structtype_size[t][native]);
    case DOUBLE: return floattype2packfun(structtype_size[t][native]);

    case STRING: return bs_atom_string;
    case WSTRING: return bs_atom_wstring;
    case PASCALSTRING: return bs_atom_pascalstring;
    case POINTER: return bs_atom_pointer;
    default: break;
    }
    return 0;
}

static t_unpackfun inttype2unpackfun(size_t bitsize, int issigned) {
    switch (bitsize) {
    case 1: return issigned?bs_int8_atom:bs_uint8_atom;
    case 2: return issigned?bs_int16_atom:bs_uint16_atom;
    case 4: return issigned?bs_int32_atom:bs_uint32_atom;
    case 8: return issigned?bs_int64_atom:bs_uint64_atom;
    default: break;
    }
    return 0;
}
static t_unpackfun floattype2unpackfun(size_t bitsize) {
    switch (bitsize) {
    case 2: return bs_float16_atom;
    case 4: return bs_float32_atom;
    case 8: return bs_float64_atom;
    default: break;
    }
    return 0;
}

static t_unpackfun type2unpackfun(t_structtype t, int native) {
    switch(t) {
    case PAD: return bs_pad_atom;
    case CHAR: return bs_char_atom;
    case BOOL: return bs_bool_atom;
    case SIGNED_CHAR: return inttype2unpackfun(structtype_size[t][native], 1);
    case UNSIGNED_CHAR: return inttype2unpackfun(structtype_size[t][native], 0);
    case SIGNED_SHORT: return inttype2unpackfun(structtype_size[t][native], 1);
    case UNSIGNED_SHORT: return inttype2unpackfun(structtype_size[t][native], 0);
    case SIGNED_INT: return inttype2unpackfun(structtype_size[t][native], 1);
    case UNSIGNED_INT: return inttype2unpackfun(structtype_size[t][native], 0);
    case SIGNED_LONG: return inttype2unpackfun(structtype_size[t][native], 1);
    case UNSIGNED_LONG: return inttype2unpackfun(structtype_size[t][native], 0);
    case SIGNED_LONGLONG: return inttype2unpackfun(structtype_size[t][native], 1);
    case UNSIGNED_LONGLONG: return inttype2unpackfun(structtype_size[t][native], 0);
    case SIGNED_SIZE: return inttype2unpackfun(structtype_size[t][native], 1);
    case UNSIGNED_SIZE: return inttype2unpackfun(structtype_size[t][native], 0);

    case HALFFLOAT: return floattype2unpackfun(structtype_size[t][native]);
    case FLOAT: return floattype2unpackfun(structtype_size[t][native]);
    case DOUBLE: return floattype2unpackfun(structtype_size[t][native]);

    case STRING: return bs_string_atom;
    case WSTRING: return bs_wstring_atom;
    case PASCALSTRING: return bs_pascalstring_atom;
    case POINTER: return bs_pointer_atom;
    default: break;
    }
    return 0;
}
static t_bs_list*bs_free(t_bs_list*bs) {
    while(bs) {
        t_bs_list*next=bs->next;
        freebytes(bs, sizeof(*bs));
        bs=next;
    }
    return bs;
}
static t_bs_list*bs_do_new(t_structtype t, int native) {
    t_bs_list*bs;
    t_unpackfun ufun = type2unpackfun(t, native);
    t_packfun pfun = type2packfun(t, native);
    int bytecount;
    if(!ufun || !pfun)
        return 0;
    bytecount = structtype_size[t][native];
    if(bytecount<0)
        return 0;
    bs = getbytes(sizeof(*bs));
    bs->type = t;
    bs->packfun = pfun;
    bs->unpackfun = ufun;
    bs->atomcount = (t!=PAD);
    bs->bytecount = bytecount;
    bs->next = 0;
    return bs;
}
static t_bs_list*bs_new(t_structtype t, unsigned int count, int native) {
    t_bs_list*bs, *bs2;
    if(!count) {
        switch(t) {
        default:
            return 0;
        case PASCALSTRING: case STRING: case WSTRING:
            break;
        }
    }
    bs = bs_do_new(t, native);
    if(!bs)
        return bs;
    if (!bs->bytecount) {
        switch(t) {
        default: break;
        case WSTRING: count *= 2;
        }
        bs->bytecount = count;
        return bs;
    }
    bs2=bs;
    while(--count) {
        t_bs_list*b = bs_do_new(t, native);
        if(!b) {
            return bs_free(bs);
        }
        bs2->next = b;
        bs2 = b;
    }
    return bs;
}

static void bs_print(t_bs_list*bs) {
    while(bs) {
        post("%c[%d] bytes=%d atoms=%d"
             , structtype2char(bs->type)
             , bs->type
             , bs->bytecount
             , bs->atomcount
#if 0
             , bs->packfun
             , bs->unpackfun
#endif
            );
        bs = bs->next;
    }
}

/* ================================================================ */
typedef struct _bytestruct {
    t_object x_obj;
    t_outlet*x_dataout;
    t_outlet*x_infoout;

    t_bs_list*x_bs;
    unsigned int x_wantatoms;
    unsigned int x_natoms;
    t_atom*x_atoms;
    unsigned int x_nbytes;
    unsigned char*x_bytes;
    int x_byteswap;
} t_bytestruct;

static t_class*bytestruct_size_class;
static t_class*bytestruct_pack_class;
static t_class*bytestruct_unpack_class;
static void bytestruct_free(t_bytestruct*x);

static int bytestruct_do_set(t_bytestruct*x, t_symbol*formatsym) {
    const char*format=formatsym?formatsym->s_name:0;
    int native = 1;
    int bigendian =
#if BYTE_ORDER == LITTLE_ENDIAN
        0
#else
        1
#endif
        ;
    const int endianness = bigendian;
    size_t atomcount = 0, bytecount = 0;
    t_bs_list*bs = 0;

    bytestruct_free(x);
    if(!format || !*format)
        goto fail;

        /* LATER: ignore whitespace... */

        /* post("parsing bytestruct '%s'", format); */
    switch(*format) {
    case '@':
        format++;
        break;
    case '=':
        native = 0;
        format++;
        break;
    case '<':
        native = 0;
        bigendian = 0;
        format++;
        break;
    case '>': case '!':
        native = 0;
        bigendian = 1;
        format++;
        break;
    default:
        break;
    }
    while(*format) {
        t_structtype structtype = ILLEGAL;
        unsigned int count = 0;
        t_bs_list*nextbs = 0, *tmpbs;
        int didit=0;
        while (*format && *format >= '0' && *format <= '9') {
            count=10*count + (*format++-'0');
            didit=1;
        }
            /* default count = 1 */
        if (!count && !didit) count=1;
        structtype = char2structtype(*format++);
        if(native && structtype && structtype<ILLEGAL && bytecount) {
                /* we might need padding */
            int alignment = structtype_size[structtype][native];
            if(alignment>0 && bytecount % alignment) {
                int padding = alignment-(bytecount%alignment);
                nextbs=bs_new(PAD, padding, native);
                if(!nextbs) goto fail;
                bytecount+=padding;
                bs->next = nextbs;
                for(; bs->next; bs=bs->next);
            }
        }
        nextbs = bs_new(structtype, count, native);
        if(!nextbs){
            if(count)
                goto fail;
            continue;
        }

        for(tmpbs=nextbs; tmpbs; tmpbs=tmpbs->next) {
            atomcount += tmpbs->atomcount;
            bytecount += tmpbs->bytecount;
        }
        if(!bs) {
            bs=nextbs;
        } else {
            bs->next = nextbs;
        }
        if(!x->x_bs)
            x->x_bs = bs;
        for(; bs->next; bs=bs->next);
    }
    x->x_wantatoms = atomcount;
        /* we allocate enough atoms so we can use it for both the
         * output of 'unpack' (atom list) and the
         * output of 'pack' (byte list)
         */
    if(bytecount>atomcount)
        atomcount=bytecount;
    x->x_atoms = getbytes(atomcount*sizeof(*x->x_atoms));
    if(x->x_atoms) x->x_natoms=atomcount;

    x->x_bytes = getbytes(bytecount*sizeof(*x->x_bytes));
    if(x->x_bytes) x->x_nbytes=bytecount;
    x->x_byteswap = (endianness != bigendian);
    return 1;
fail:
    bytestruct_free(x);
    x->x_wantatoms=(unsigned int)-1;
    return 0;

}
static void bytestruct_set(t_bytestruct*x, t_symbol*formatsym) {
    bytestruct_do_set(x, formatsym);
}
static void bytestruct_dump(t_bytestruct*x) {
    post("bytes=%d", x->x_nbytes);
    post("atoms=%d [%d]", x->x_wantatoms, x->x_natoms);
    post("byteswap=%d", x->x_byteswap);
    bs_print(x->x_bs);
}

static void bytestruct_free(t_bytestruct*x) {
    x->x_bs = bs_free(x->x_bs);

    if(x->x_natoms) {
        freebytes(x->x_atoms, x->x_natoms * sizeof(*x->x_atoms));
    }
    x->x_natoms = 0;
    x->x_atoms = 0;
    x->x_wantatoms = 0;

    if(x->x_nbytes) {
        freebytes(x->x_bytes, x->x_nbytes * sizeof(*x->x_bytes));
    }
    x->x_nbytes = 0;
    x->x_bytes = 0;
}

static void bytestruct_pack_list(t_bytestruct*x, t_symbol*s, int argc, t_atom*argv) {
    t_bs_list*bs;
    unsigned char*buffer=x->x_bytes;
    (void)s;
    if(!x->x_bs && x->x_wantatoms) {
        pd_error(x, "illegal struct specification");
        outlet_bang(x->x_infoout);
        return;
    }
    if(x->x_wantatoms != (unsigned int)argc) {
        pd_error(x, "expected %d atoms, but got %d", x->x_wantatoms, argc);
        outlet_bang(x->x_infoout);
        return;
    }
    for(bs = x->x_bs; bs; bs=bs->next) {
        if(!bs->packfun(argv, buffer, bs->bytecount, x->x_byteswap)) {
            pd_error(x, "failed to convert atom");
            outlet_bang(x->x_infoout);
            return;
        }
        argv+=bs->atomcount;
        buffer+=bs->bytecount;
    }
    for(size_t i=0;i<x->x_nbytes;i++) {
        SETFLOAT(x->x_atoms+i, x->x_bytes[i]);
    }
    outlet_list(x->x_dataout, gensym("list"), x->x_nbytes, x->x_atoms);
}

static void bytestruct_unpack_list(t_bytestruct*x, t_symbol*s, int argc, t_atom*argv) {
    t_bs_list*bs;
    unsigned char*buffer=x->x_bytes;
    int i;
    (void)s;
    if(!x->x_bs && x->x_wantatoms) {
        pd_error(x, "illegal struct specification");
        outlet_bang(x->x_infoout);
        return;
    }

    if(x->x_nbytes != (unsigned int)argc) {
        pd_error(x, "expected %d atoms, but got %d", x->x_nbytes, argc);
        outlet_bang(x->x_infoout);
        return;
    }
    for(i=0;i<argc;i++) {
        t_float f=atom_getfloat(argv+i);
        if(argv[i].a_type != A_FLOAT ||
           (f<0 || f>255)) {
            pd_error(x, "invalid atom#%d: must be a number 0..255!", i);
            return;
        }
        buffer[i] = (char)f;
    }

    buffer = x->x_bytes;
    argv = x->x_atoms;
    for(bs = x->x_bs; bs; bs=bs->next) {
        if(!bs->unpackfun(buffer, bs->bytecount, argv, x->x_byteswap)) {
            pd_error(x, "failed to convert atom");
            outlet_bang(x->x_infoout);
            return;
        }
        argv+=bs->atomcount;
        buffer+=bs->bytecount;
    }
    outlet_list(x->x_dataout, gensym("list"), x->x_wantatoms, x->x_atoms);
}

static void bytestruct_size_bang(t_bytestruct*x) {
    if(!x->x_bs && x->x_wantatoms) {
        pd_error(x, "illegal struct specification");
        outlet_bang(x->x_infoout);
    } else {
        outlet_float(x->x_dataout, x->x_nbytes);
    }
}
static void bytestruct_size_symbol(t_bytestruct*x, t_symbol*s) {
    bytestruct_do_set(x, s);
    bytestruct_size_bang(x);
}
static t_pd*bytestruct_do_new(t_class*c, t_symbol*s) {
    t_bytestruct*x=(t_bytestruct*)pd_new(c);
    x->x_dataout = outlet_new((t_object*)x, 0);
    x->x_infoout = outlet_new((t_object*)x, 0);
    bytestruct_set(x, s);
    return (t_pd*)x;
}
static t_pd*bytestruct_size_new(t_symbol*s) {
    return bytestruct_do_new(bytestruct_size_class, s);
}
static t_pd*bytestruct_pack_new(t_symbol*s) {
    t_pd*x=bytestruct_do_new(bytestruct_pack_class, s);
    inlet_new((t_object*)x, x, gensym("symbol"), gensym("set"));
    return x;
}
static t_pd*bytestruct_unpack_new(t_symbol*s) {
    t_pd*x=bytestruct_do_new(bytestruct_unpack_class, s);
    inlet_new((t_object*)x, x, gensym("symbol"), gensym("set"));
    return x;
}

static t_pd*bytestruct_new(t_symbol*s, int argc, t_atom*argv) {
    t_symbol*verb = atom_getsymbolarg(0, argc, argv);
    if(argc<1 || argc>2 || A_SYMBOL != argv[0].a_type) {
        goto fail;
    }
    s = atom_getsymbol(argv+0);
    if (0) ;
    else if (!strcmp("pack", verb->s_name))
        return bytestruct_pack_new(atom_getsymbolarg(1, argc, argv));
    else if (!strcmp("unpack", verb->s_name))
        return bytestruct_unpack_new(atom_getsymbolarg(1, argc, argv));
    else if (!strcmp("size", verb->s_name))
        return bytestruct_size_new(atom_getsymbolarg(1, argc, argv));
fail:
    error("%s: valid verbs are 'pack', 'unpack' 'size'", s->s_name);
    return 0;
}

void x_bytestruct_setup(void)
{
    class_addcreator((t_newmethod)bytestruct_new, gensym("bytestruct"), A_GIMME, 0);

        /* [bytestruct size] */
    bytestruct_size_class = class_new(
        gensym("bytestruct size"),
        (t_newmethod)bytestruct_size_new,
        (t_method)bytestruct_free,
        sizeof(t_bytestruct),
        0,
        A_DEFSYM, 0);
    class_addbang(bytestruct_size_class, (t_method)bytestruct_size_bang);
    class_addsymbol(bytestruct_size_class, (t_method)bytestruct_size_symbol);

        /* [bytestruct pack] */
    bytestruct_pack_class = class_new(
        gensym("bytestruct pack"),
        (t_newmethod)bytestruct_pack_new,
        (t_method)bytestruct_free,
        sizeof(t_bytestruct),
        0,
        A_DEFSYM, 0);
    class_addlist(bytestruct_pack_class, (t_method)bytestruct_pack_list);
    class_addmethod(bytestruct_pack_class, (t_method)bytestruct_set,
                    gensym("set"), A_SYMBOL, 0);

        /* [bytestruct unpack] */
    bytestruct_unpack_class = class_new(
        gensym("bytestruct unpack"),
        (t_newmethod)bytestruct_unpack_new,
        (t_method)bytestruct_free,
        sizeof(t_bytestruct),
        0,
        A_DEFSYM, 0);
    class_addlist(bytestruct_unpack_class, (t_method)bytestruct_unpack_list);
    class_addmethod(bytestruct_unpack_class, (t_method)bytestruct_set,
                    gensym("set"), A_SYMBOL, 0);


    class_addmethod(bytestruct_pack_class, (t_method)bytestruct_dump,
                    gensym("dump"), 0);
    class_addmethod(bytestruct_unpack_class, (t_method)bytestruct_dump,
                    gensym("dump"), 0);
    class_addmethod(bytestruct_size_class, (t_method)bytestruct_dump,
                    gensym("dump"), 0);

    class_sethelpsymbol(bytestruct_size_class, gensym("bytestruct"));
    class_sethelpsymbol(bytestruct_pack_class, gensym("bytestruct"));
    class_sethelpsymbol(bytestruct_unpack_class, gensym("bytestruct"));
}
