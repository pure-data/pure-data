/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* misc. */

#include "m_pd.h"
#include "s_stuff.h"
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

static void loadbang_loadbang(t_loadbang *x)
{
    if (!sys_noloadbang)
        outlet_bang(x->x_obj.ob_outlet);
}

static void loadbang_setup(void)
{
    loadbang_class = class_new(gensym("loadbang"), (t_newmethod)loadbang_new, 0,
        sizeof(t_loadbang), CLASS_NOINLET, 0);
    class_addmethod(loadbang_class, (t_method)loadbang_loadbang,
        gensym("loadbang"), 0);
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

/* ---------------serial ports (_WIN32 only -- hack) ------------------------- */
#define MAXSERIAL 100

static t_class *serial_class;

typedef struct _serial
{
    t_object x_obj;
    int x_portno;
    int x_open;
} t_serial;

static void serial_float(t_serial *x, t_float f)
{
    int n = f;
    char message[MAXSERIAL * 4 + 100];
    if (!x->x_open)
    {
        sys_vgui("com%d_open\n", x->x_portno);
        x->x_open = 1;
    }
    sprintf(message, "com%d_send \"\\%3.3o\"\n", x->x_portno, n);
    sys_gui(message);
}

static void *serial_new(t_floatarg fportno)
{
    int portno = fportno;
    t_serial *x = (t_serial *)pd_new(serial_class);
    if (!portno) portno = 1;
    x->x_portno = portno;
    x->x_open = 0;
    return (x);
}

static void serial_setup(void)
{
    serial_class = class_new(gensym("serial"), (t_newmethod)serial_new, 0,
        sizeof(t_serial), 0, A_DEFFLOAT, 0);
    class_addfloat(serial_class, serial_float);
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

void x_misc_setup(void)
{
    random_setup();
    loadbang_setup();
    namecanvas_setup();
    serial_setup();
    cputime_setup();
    realtime_setup();
}
