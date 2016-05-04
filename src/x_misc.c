/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* misc. */

#include "m_pd.h"
#include "s_stuff.h"
#include "g_canvas.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <wtypes.h>
#include <time.h>
#else
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/param.h>
#include <unistd.h>
#endif /* _WIN32 */

#if defined (__APPLE__) || defined (__FreeBSD__)
#define CLOCKHZ CLK_TCK
#endif
#if defined (__linux__) || defined (__CYGWIN__) || defined (ANDROID)
#define CLOCKHZ sysconf(_SC_CLK_TCK)
#endif
#if defined (__FreeBSD_kernel__) || defined(__GNU__) || defined(__OpenBSD__)
#include <time.h>
#define CLOCKHZ CLOCKS_PER_SEC
#endif

#ifdef _WIN32
# include <malloc.h> /* MSVC or mingw on windows */
#elif defined(__linux__) || defined(__APPLE__)
# include <alloca.h> /* linux, mac, mingw, cygwin */
#else
# include <stdlib.h> /* BSDs for example */
#endif

/* -------------------------- random ------------------------------ */
/* this is strictly homebrew and untested. */

static t_class *random_class;

typedef struct _random
{
    t_object x_obj;
    t_float x_f;
    unsigned int x_state;
} t_random;


static int makeseed(void)
{
    static unsigned int random_nextseed = 1489853723;
    random_nextseed = random_nextseed * 435898247 + 938284287;
    return (random_nextseed & 0x7fffffff);
}

static void *random_new(t_floatarg f)
{
    t_random *x = (t_random *)pd_new(random_class);
    x->x_f = f;
    x->x_state = makeseed();
    floatinlet_new(&x->x_obj, &x->x_f);
    outlet_new(&x->x_obj, &s_float);
    return (x);
}

static void random_bang(t_random *x)
{
    int n = x->x_f, nval;
    int range = (n < 1 ? 1 : n);
    unsigned int randval = x->x_state;
    x->x_state = randval = randval * 472940017 + 832416023;
    nval = ((double)range) * ((double)randval)
        * (1./4294967296.);
    if (nval >= range) nval = range-1;
    outlet_float(x->x_obj.ob_outlet, nval);
}

static void random_seed(t_random *x, t_float f, t_float glob)
{
    x->x_state = f;
}

static void random_setup(void)
{
    random_class = class_new(gensym("random"), (t_newmethod)random_new, 0,
        sizeof(t_random), 0, A_DEFFLOAT, 0);
    class_addbang(random_class, random_bang);
    class_addmethod(random_class, (t_method)random_seed,
        gensym("seed"), A_FLOAT, 0);
}


/* -------------------------- loadbang ------------------------------ */
static t_class *loadbang_class;

typedef struct _loadbang
{
    t_object x_obj;
} t_loadbang;

static void *loadbang_new(void)
{
    t_loadbang *x = (t_loadbang *)pd_new(loadbang_class);
    outlet_new(&x->x_obj, &s_bang);
    return (x);
}

static void loadbang_loadbang(t_loadbang *x, t_floatarg action)
{
    if (action == LB_LOAD)
        outlet_bang(x->x_obj.ob_outlet);
}

static void loadbang_setup(void)
{
    loadbang_class = class_new(gensym("loadbang"), (t_newmethod)loadbang_new, 0,
        sizeof(t_loadbang), CLASS_NOINLET, 0);
    class_addmethod(loadbang_class, (t_method)loadbang_loadbang,
        gensym("loadbang"), A_DEFFLOAT, 0);
}

/* ------------- namecanvas (delete this later) --------------------- */
static t_class *namecanvas_class;

typedef struct _namecanvas
{
    t_object x_obj;
    t_symbol *x_sym;
    t_pd *x_owner;
} t_namecanvas;

static void *namecanvas_new(t_symbol *s)
{
    t_namecanvas *x = (t_namecanvas *)pd_new(namecanvas_class);
    x->x_owner = (t_pd *)canvas_getcurrent();
    x->x_sym = s;
    if (*s->s_name) pd_bind(x->x_owner, s);
    return (x);
}

static void namecanvas_free(t_namecanvas *x)
{
    if (*x->x_sym->s_name) pd_unbind(x->x_owner, x->x_sym);
}

static void namecanvas_setup(void)
{
    namecanvas_class = class_new(gensym("namecanvas"),
        (t_newmethod)namecanvas_new, (t_method)namecanvas_free,
            sizeof(t_namecanvas), CLASS_NOINLET, A_DEFSYM, 0);
}

/* -------------------------- cputime ------------------------------ */

static t_class *cputime_class;

typedef struct _cputime
{
    t_object x_obj;
#ifdef _WIN32
    LARGE_INTEGER x_kerneltime;
    LARGE_INTEGER x_usertime;
    int x_warned;
#else
    struct tms x_setcputime;
#endif /* _WIN32 */
} t_cputime;

static void cputime_bang(t_cputime *x)
{
#ifdef _WIN32
    FILETIME ignorethis, ignorethat;
    BOOL retval;
    retval = GetProcessTimes(GetCurrentProcess(), &ignorethis, &ignorethat,
        (FILETIME *)&x->x_kerneltime, (FILETIME *)&x->x_usertime);
    if (!retval)
    {
        if (!x->x_warned)
            post("cputime is apparently not supported on your platform");
        x->x_warned = 1;
        x->x_kerneltime.QuadPart = 0;
        x->x_usertime.QuadPart = 0;
    }
#else
    times(&x->x_setcputime);
#endif /* _WIN32 */
}

static void cputime_bang2(t_cputime *x)
{
#ifndef _WIN32
    t_float elapsedcpu;
    struct tms newcputime;
    times(&newcputime);
    elapsedcpu = 1000 * (
        newcputime.tms_utime + newcputime.tms_stime -
            x->x_setcputime.tms_utime - x->x_setcputime.tms_stime) / CLOCKHZ;
    outlet_float(x->x_obj.ob_outlet, elapsedcpu);
#else
    t_float elapsedcpu;
    FILETIME ignorethis, ignorethat;
    LARGE_INTEGER usertime, kerneltime;
    BOOL retval;

    retval = GetProcessTimes(GetCurrentProcess(), &ignorethis, &ignorethat,
        (FILETIME *)&kerneltime, (FILETIME *)&usertime);
    if (retval)
        elapsedcpu = 0.0001 *
            ((kerneltime.QuadPart - x->x_kerneltime.QuadPart) +
                (usertime.QuadPart - x->x_usertime.QuadPart));
    else elapsedcpu = 0;
    outlet_float(x->x_obj.ob_outlet, elapsedcpu);
#endif /* NOT _WIN32 */
}

static void *cputime_new(void)
{
    t_cputime *x = (t_cputime *)pd_new(cputime_class);
    outlet_new(&x->x_obj, gensym("float"));

    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("bang"), gensym("bang2"));
#ifdef _WIN32
    x->x_warned = 0;
#endif
    cputime_bang(x);
    return (x);
}

static void cputime_setup(void)
{
    cputime_class = class_new(gensym("cputime"), (t_newmethod)cputime_new, 0,
        sizeof(t_cputime), 0, 0);
    class_addbang(cputime_class, cputime_bang);
    class_addmethod(cputime_class, (t_method)cputime_bang2, gensym("bang2"), 0);
}

/* -------------------------- realtime ------------------------------ */

static t_class *realtime_class;

typedef struct _realtime
{
    t_object x_obj;
    double x_setrealtime;
} t_realtime;

static void realtime_bang(t_realtime *x)
{
    x->x_setrealtime = sys_getrealtime();
}

static void realtime_bang2(t_realtime *x)
{
    outlet_float(x->x_obj.ob_outlet,
        (sys_getrealtime() - x->x_setrealtime) * 1000.);
}

static void *realtime_new(void)
{
    t_realtime *x = (t_realtime *)pd_new(realtime_class);
    outlet_new(&x->x_obj, gensym("float"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("bang"), gensym("bang2"));
    realtime_bang(x);
    return (x);
}

static void realtime_setup(void)
{
    realtime_class = class_new(gensym("realtime"), (t_newmethod)realtime_new, 0,
        sizeof(t_realtime), 0, 0);
    class_addbang(realtime_class, realtime_bang);
    class_addmethod(realtime_class, (t_method)realtime_bang2, gensym("bang2"),
        0);
}

/* ---------- oscparse - parse simple OSC messages ----------------- */

static t_class *oscparse_class;

typedef struct _oscparse
{
    t_object x_obj;
} t_oscparse;

#define ROUNDUPTO4(x) (((x) + 3) & (~3))

#define READINT(x)  ((((int)(((x)  )->a_w.w_float)) & 0xff) << 24) | \
                    ((((int)(((x)+1)->a_w.w_float)) & 0xff) << 16) | \
                    ((((int)(((x)+2)->a_w.w_float)) & 0xff) << 8) | \
                    ((((int)(((x)+3)->a_w.w_float)) & 0xff) << 0)

static t_symbol *grabstring(int argc, t_atom *argv, int *ip, int slash)
{
    char buf[MAXPDSTRING];
    int first, nchar;
    if (slash)
        while (*ip < argc && argv[*ip].a_w.w_float == '/')
            (*ip)++;
    for (nchar = 0; nchar < MAXPDSTRING-1 && *ip < argc; nchar++, (*ip)++)
    {
        char c = argv[*ip].a_w.w_float;
        if (c == 0 || (slash && c == '/'))
            break;
        buf[nchar] = c;
    }
    buf[nchar] = 0;
    if (!slash)
        *ip = ROUNDUPTO4(*ip+1);
    if (*ip > argc)
        *ip = argc;
    return (gensym(buf));
}

static void oscparse_list(t_oscparse *x, t_symbol *s, int argc, t_atom *argv)
{
    int i, j, j2, k, outc = 1, blob = 0, typeonset, dataonset, nfield;
    t_atom *outv;
    if (!argc)
        return;
    for (i = 0; i < argc; i++)
        if (argv[i].a_type != A_FLOAT)
    {
        pd_error(x, "oscparse: takes numbers only");
        return;
    }
    if (argv[0].a_w.w_float == '#') /* it's a bundle */
    {
        if (argv[1].a_w.w_float != 'b' || argc < 16)
        {
            pd_error(x, "oscparse: malformed bundle");
            return;
        }
            /* we ignore the timetag since there's no correct way to
            convert it to Pd logical time that I can think of.  LATER
            consider at least outputting timetag differentially converted
            into Pd time units. */
        for (i = 16; i < argc-4; )
        {
            int msize = READINT(argv+i);
            if (msize <= 0 || msize & 3)
            {
                pd_error(x, "oscparse: bad bundle element size");
                return;
            }
            oscparse_list(x, 0, msize, argv+i+4);
            i += msize+4;
        }
        return;
    }
    else if (argv[0].a_w.w_float != '/')
    {
        pd_error(x, "oscparse: not an OSC message (no leading slash)");
        return;
    }
    for (i = 1; i < argc && argv[i].a_w.w_float != 0; i++)
        if (argv[i].a_w.w_float == '/')
            outc++;
    i = ROUNDUPTO4(i+1);
    if (argv[i].a_w.w_float != ',' || (i+1) >= argc)
    {
        pd_error(x, "oscparse: malformed type string (char %d, index %d)",
            (int)(argv[i].a_w.w_float), i);
        return;
    }
    typeonset = ++i;
    for (; i < argc && argv[i].a_w.w_float != 0; i++)
        if (argv[i].a_w.w_float == 'b')
            blob = 1;
    nfield = i - typeonset;
    if (blob)
        outc += argc - typeonset;
    else outc += nfield;
    outv = (t_atom *)alloca(outc * sizeof(t_atom));
    dataonset = ROUNDUPTO4(i + 1);
    /* post("outc %d, typeonset %d, dataonset %d, nfield %d", outc, typeonset,
        dataonset, nfield); */
    for (i = j = 0; i < typeonset-1 && argv[i].a_w.w_float != 0 &&
        j < outc; j++)
            SETSYMBOL(outv+j, grabstring(argc, argv, &i, 1));
    for (i = typeonset, k = dataonset; i < typeonset + nfield; i++)
    {
        union
        {
            float z_f;
            uint32_t z_i;
        } z;
        float f;
        int blobsize;
        switch ((int)(argv[i].a_w.w_float))
        {
        case 'f':
            if (k > argc - 4)
                goto tooshort;
            z.z_i = READINT(argv+k);
            f = z.z_f;
            if (PD_BADFLOAT(f))
                f = 0;
            if (j >= outc)
            {
                bug("oscparse 1: %d >=%d", j, outc);
                return;
            }
            SETFLOAT(outv+j, f);
            j++; k += 4;
            break;
        case 'i':
            if (k > argc - 4)
                goto tooshort;
            if (j >= outc)
            {
                bug("oscparse 2");
                return;
            }
            SETFLOAT(outv+j, READINT(argv+k));
            j++; k += 4;
            break;
        case 's':
            if (j >= outc)
            {
                bug("oscparse 3");
                return;
            }
            SETSYMBOL(outv+j, grabstring(argc, argv, &k, 0));
            j++;
            break;
        case 'b':
            if (k > argc - 4)
                goto tooshort;
            blobsize = READINT(argv+k);
            k += 4;
            if (blobsize < 0 || blobsize > argc - k)
                goto tooshort;
            if (j + blobsize + 1 > outc)
            {
                bug("oscparse 4");
                return;
            }
            if (k + blobsize > argc)
                goto tooshort;
            SETFLOAT(outv+j, blobsize);
            j++;
            for (j2 = 0; j2 < blobsize; j++, j2++, k++)
                SETFLOAT(outv+j, argv[k].a_w.w_float);
            k = ROUNDUPTO4(k);
            break;
        default:
            pd_error(x, "oscparse: unknown tag '%c' (%d)",
                (int)(argv[i].a_w.w_float), (int)(argv[i].a_w.w_float));
        }
    }
    outlet_list(x->x_obj.ob_outlet, 0, j, outv);
    return;
tooshort:
    pd_error(x, "oscparse: OSC message ended prematurely");
}

static t_oscparse *oscparse_new(t_symbol *s, int argc, t_atom *argv)
{
    t_oscparse *x = (t_oscparse *)pd_new(oscparse_class);
    outlet_new(&x->x_obj, gensym("list"));
    return (x);
}

void oscparse_setup(void)
{
    oscparse_class = class_new(gensym("oscparse"), (t_newmethod)oscparse_new,
        0, sizeof(t_oscparse), 0, A_GIMME, 0);
    class_addlist(oscparse_class, oscparse_list);
}

/* --------- oscformat - format simple OSC messages -------------- */
static t_class *oscformat_class;

typedef struct _oscformat
{
    t_object x_obj;
    char *x_pathbuf;
    int x_pathsize;
    t_symbol *x_format;
} t_oscformat;

static void oscformat_set(t_oscformat *x, t_symbol *s, int argc, t_atom *argv)
{
    char buf[MAXPDSTRING];
    int i, newsize;
    *x->x_pathbuf = 0;
    buf[0] = '/';
    for (i = 0; i < argc; i++)
    {
        char *where = (argv[i].a_type == A_SYMBOL &&
            *argv[i].a_w.w_symbol->s_name == '/' ? buf : buf+1);
        atom_string(&argv[i], where, MAXPDSTRING-1);
        if ((newsize = strlen(buf) + strlen(x->x_pathbuf) + 1) > x->x_pathsize)
        {
            x->x_pathbuf = resizebytes(x->x_pathbuf, x->x_pathsize, newsize);
            x->x_pathsize = newsize;
        }
        strcat(x->x_pathbuf, buf);
    }
}

static void oscformat_format(t_oscformat *x, t_symbol *s)
{
    char *sp;
    for (sp = s->s_name; *sp; sp++)
    {
        if (*sp != 'f' && *sp != 'i' && *sp != 's' && *sp != 'b')
        {
            pd_error(x,
                "oscformat '%s' may only contain 'f', 'i'. 's', and/or 'b'",
                    sp);
            return;
        }
    }
    x->x_format = s;
}

#define WRITEINT(msg, i)    SETFLOAT((msg),   (((i) >> 24) & 0xff)); \
                            SETFLOAT((msg)+1, (((i) >> 16) & 0xff)); \
                            SETFLOAT((msg)+2, (((i) >>  8) & 0xff)); \
                            SETFLOAT((msg)+3, (((i)      ) & 0xff))

static void putstring(t_atom *msg, int *ip, const char *s)
{
    const char *sp = s;
    do
    {
        SETFLOAT(&msg[*ip], *sp & 0xff);
        (*ip)++;
    }
    while (*sp++);
    while (*ip & 3)
    {
        SETFLOAT(&msg[*ip], 0);
        (*ip)++;
    }
}

static void oscformat_list(t_oscformat *x, t_symbol *s, int argc, t_atom *argv)
{
    int typeindex = 0, j, msgindex, msgsize, datastart, ndata;
    t_atom *msg;
    char *sp, *formatp = x->x_format->s_name, typecode;
        /* pass 1: go through args to find overall message size */
    for (j = ndata = 0, sp = formatp, msgindex = 0; j < argc;)
    {
        if (*sp)
            typecode = *sp++;
        else if (argv[j].a_type == A_SYMBOL)
            typecode = 's';
        else typecode = 'f';
        if (typecode == 's')
            msgindex += ROUNDUPTO4(strlen(argv[j].a_w.w_symbol->s_name) + 1);
        else if (typecode == 'b')
        {
            int blobsize = 0x7fffffff, blobindex;
                /* check if we have a nonnegative size field */
            if (argv[j].a_type == A_FLOAT &&
                (int)(argv[j].a_w.w_float) >= 0)
                    blobsize = (int)(argv[j].a_w.w_float);
            if (blobsize > argc - j - 1)
                blobsize = argc - j - 1;    /* if no or bad size, eat it all */
            msgindex += 4 + ROUNDUPTO4(blobsize);
            j += blobsize;
        }
        else msgindex += 4;
        j++;
        ndata++;
    }
    datastart = ROUNDUPTO4(strlen(x->x_pathbuf)+1) + ROUNDUPTO4(ndata + 2);
    msgsize = datastart + msgindex;
    msg = (t_atom *)alloca(msgsize * sizeof(t_atom));
    putstring(msg, &typeindex, x->x_pathbuf);
    SETFLOAT(&msg[typeindex], ',');
    typeindex++;
        /* pass 2: fill in types and data portion of packet */
    for (j = 0, sp = formatp, msgindex = datastart; j < argc;)
    {
        if (*sp)
            typecode = *sp++;
        else if (argv[j].a_type == A_SYMBOL)
            typecode = 's';
        else typecode = 'f';
        SETFLOAT(&msg[typeindex], typecode & 0xff);
        typeindex++;
        if (typecode == 'f')
        {
            union
            {
                float z_f;
                uint32_t z_i;
            } z;
            z.z_f = atom_getfloat(&argv[j]);
            WRITEINT(msg+msgindex, z.z_i);
            msgindex += 4;
        }
        else if (typecode == 'i')
        {
            int dat = atom_getfloat(&argv[j]);
            WRITEINT(msg+msgindex, dat);
            msgindex += 4;
        }
        else if (typecode == 's')
            putstring(msg, &msgindex, argv[j].a_w.w_symbol->s_name);
        else if (typecode == 'b')
        {
            int blobsize = 0x7fffffff, blobindex;
            if (argv[j].a_type == A_FLOAT &&
                (int)(argv[j].a_w.w_float) >= 0)
                    blobsize = (int)(argv[j].a_w.w_float);
            if (blobsize > argc - j - 1)
                blobsize = argc - j - 1;
            WRITEINT(msg+msgindex, blobsize);
            msgindex += 4;
            for (blobindex = 0; blobindex < blobsize; blobindex++)
                SETFLOAT(msg+msgindex+blobindex,
                    (argv[j+1+blobindex].a_type == A_FLOAT ?
                        argv[j+1+blobindex].a_w.w_float :
                        (argv[j+1+blobindex].a_type == A_SYMBOL ?
                            argv[j+1+blobindex].a_w.w_symbol->s_name[0] & 0xff :
                            0)));
            j += blobsize;
            while (blobsize & 3)
                SETFLOAT(msg+msgindex+blobsize, 0), blobsize++;
            msgindex += blobsize;
        }
        j++;
    }
    SETFLOAT(&msg[typeindex], 0);
    typeindex++;
    while (typeindex & 3)
        SETFLOAT(&msg[typeindex], 0), typeindex++;
    if (typeindex != datastart || msgindex != msgsize)
        bug("oscformat: typeindex %d, datastart %d, msgindex %d, msgsize %d",
            typeindex, datastart, msgindex, msgsize);
    /* else post("datastart %d, msgsize %d", datastart, msgsize); */
    outlet_list(x->x_obj.ob_outlet, 0, msgsize, msg);
}

static void oscformat_free(t_oscformat *x)
{
    freebytes(x->x_pathbuf, x->x_pathsize);
}

static void *oscformat_new(t_symbol *s, int argc, t_atom *argv)
{
    t_oscformat *x = (t_oscformat *)pd_new(oscformat_class);
    outlet_new(&x->x_obj, gensym("list"));
    x->x_pathbuf = getbytes(1);
    x->x_pathsize = 1;
    *x->x_pathbuf = 0;
    x->x_format = &s_;
    if (argc > 1 && argv[0].a_type == A_SYMBOL &&
        argv[1].a_type == A_SYMBOL &&
            !strcmp(argv[0].a_w.w_symbol->s_name, "-f"))
    {
        oscformat_format(x, argv[1].a_w.w_symbol);
        argc -= 2;
        argv += 2;
    }
    oscformat_set(x, 0, argc, argv);
    return (x);
}

void oscformat_setup(void)
{
    oscformat_class = class_new(gensym("oscformat"), (t_newmethod)oscformat_new,
        (t_method)oscformat_free, sizeof(t_oscformat), 0, A_GIMME, 0);
    class_addmethod(oscformat_class, (t_method)oscformat_set,
        gensym("set"), A_GIMME, 0);
    class_addmethod(oscformat_class, (t_method)oscformat_format,
        gensym("format"), A_DEFSYM, 0);
    class_addlist(oscformat_class, oscformat_list);
}

void x_misc_setup(void)
{
    random_setup();
    loadbang_setup();
    namecanvas_setup();
    cputime_setup();
    realtime_setup();
    oscparse_setup();
    oscformat_setup();
}
