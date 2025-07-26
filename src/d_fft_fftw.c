/* Copyright (c) 1997- Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* --------- Pd interface to FFTW library; imitate Mayer API ---------- */

/* changes and additions for FFTW3 by Thomas Grill                      */

#include "m_pd.h"
#include "m_imp.h"
#include <fftw3.h>
#if PDTHREADS && defined(PDINSTANCE)
#include <pthread.h>
#endif

int ilog2(int n);

#define MINFFT 0
#define MAXFFT 30

/* from the FFTW website:
 #include <fftw3.h>
     ...
     {
         fftw_complex *in, *out;
     fftw_plan p;
     ...
         in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
         out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
         p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
     ...
         fftw_execute(p);
     ...
     fftw_destroy_plan(p);
         fftw_free(in); fftw_free(out);
     }

FFTW_FORWARD or FFTW_BACKWARD, and indicates the direction of the transform you
are interested in. Alternatively, you can use the sign of the exponent in the
transform, -1 or +1, which corresponds to FFTW_FORWARD or FFTW_BACKWARD
respectively. The flags argument is either FFTW_MEASURE

*/

/* each Pd instance has its own plans and buffers, so that instances in
different threads can compute FFTs at the same time.  They are stored as
per-instance data of a class that exists just for this. */

#define NFFT (MAXFFT+1 - MINFFT)

typedef struct {
    fftwf_plan plan;
    fftwf_complex *in,*out;
} cfftw_info;

typedef struct {
    fftwf_plan plan;
    float *in,*out;
} rfftw_info;

typedef struct {
    cfftw_info c_fwd[NFFT], c_bwd[NFFT];
    rfftw_info r_fwd[NFFT], r_bwd[NFFT];
} t_fftwplans;

static t_class *fftwplans_class;

    /* of FFTW's routines only fftwf_execute() is thread-safe */
#if PDTHREADS && defined(PDINSTANCE)
static pthread_mutex_t planner_mutex = PTHREAD_MUTEX_INITIALIZER;
#define planner_lock() pthread_mutex_lock(&planner_mutex)
#define planner_unlock() pthread_mutex_unlock(&planner_mutex)
#else
#define planner_lock()
#define planner_unlock()
#endif

static void fftwplans_free(void *z);

static t_fftwplans *fftwplans_get(void)
{
    t_fftwplans *x = (t_fftwplans *)class_getinstancedata(fftwplans_class);
    if (!x)
    {
        x = (t_fftwplans *)getbytes(sizeof(*x));
        class_setinstancedata(fftwplans_class, x, fftwplans_free);
    }
    return (x);
}

/* complex stuff */

static cfftw_info *cfftw_getplan(int n,int fwd)
{
    cfftw_info *info;
    t_fftwplans *x;
    int logn = ilog2(n);
    if (logn < MINFFT || logn > MAXFFT)
        return (0);
    x = fftwplans_get();
    info = (fwd?x->c_fwd:x->c_bwd)+(logn-MINFFT);
    if (!info->plan)
    {
        planner_lock();
        info->in =
            (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * n);
        info->out =
            (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * n);
        info->plan = fftwf_plan_dft_1d(n, info->in, info->out,
            fwd?FFTW_FORWARD:FFTW_BACKWARD, FFTW_MEASURE);
        planner_unlock();
    }
    return info;
}

static void cfftw_term(t_fftwplans *x)
{
    int i, j;
    cfftw_info *cinfo[2];

    for (i = 0; i < NFFT; i++)
    {
      cinfo[0] = &x->c_fwd[i];
      cinfo[1] = &x->c_bwd[i];

      for (j = 0; j < 2; j++)
      {
        if (cinfo[j]->plan)
        {
          fftwf_destroy_plan(cinfo[j]->plan);
          fftwf_free(cinfo[j]->in);
          fftwf_free(cinfo[j]->out);
          cinfo[j]->plan = 0;
          cinfo[j]->in = 0;
          cinfo[j]->out = 0;
        }
      }
    }
}


/* real stuff */

static rfftw_info *rfftw_getplan(int n,int fwd)
{
    rfftw_info *info;
    t_fftwplans *x;
    int logn = ilog2(n);
    if (logn < MINFFT || logn > MAXFFT)
        return (0);
    x = fftwplans_get();
    info = (fwd?x->r_fwd:x->r_bwd)+(logn-MINFFT);
    if (!info->plan)
    {
        planner_lock();
        info->in = (float*) fftwf_malloc(sizeof(float) * n);
        info->out = (float*) fftwf_malloc(sizeof(float) * n);
        info->plan = fftwf_plan_r2r_1d(n, info->in, info->out, fwd?FFTW_R2HC:FFTW_HC2R, FFTW_MEASURE);
        planner_unlock();
    }
    return info;
}

static void rfftw_term(t_fftwplans *x)
{
    int i, j;
    rfftw_info *rinfo[2];

    for (i = 0; i < NFFT; i++)
    {
      rinfo[0] = &x->r_fwd[i];
      rinfo[1] = &x->r_bwd[i];

      for (j = 0; j < 2; j++)
      {
        if (rinfo[j]->plan)
        {
          fftwf_destroy_plan(rinfo[j]->plan);
          fftwf_free(rinfo[j]->in);
          fftwf_free(rinfo[j]->out);
          rinfo[j]->plan = 0;
          rinfo[j]->in = 0;
          rinfo[j]->out = 0;
        }
      }
    }
}

    /* called when a Pd instance is freed, or by class_free() below */
static void fftwplans_free(void *z)
{
    t_fftwplans *x = (t_fftwplans *)z;
    planner_lock();
    cfftw_term(x);
    rfftw_term(x);
    planner_unlock();
    freebytes(x, sizeof(*x));
}

static int mayer_refcount = 0;

void mayer_init(void)
{
    if (mayer_refcount++ == 0)
        fftwplans_class = class_new(gensym("fftwplans"), 0, 0,
            sizeof(t_pd), CLASS_PD, 0);
}

void mayer_term(void)
{
    if (--mayer_refcount == 0)
    {
        class_free(fftwplans_class);
        fftwplans_class = 0;
    }
}


void mayer_fht(t_sample *fz, int n)
{
    post("FHT: not yet implemented");
}

static void mayer_do_cfft(int n, t_sample *fz1, t_sample *fz2, int fwd)
{
    int i;
    float *fz;
    cfftw_info *p = cfftw_getplan(n, fwd);
    if (!p)
        return;

    for (i = 0, fz = (float *)p->in; i < n; i++)
        fz[i*2] = fz1[i], fz[i*2+1] = fz2[i];

    fftwf_execute(p->plan);

    for (i = 0, fz = (float *)p->out; i < n; i++)
        fz1[i] = fz[i*2], fz2[i] = fz[i*2+1];
}

void mayer_fft(int n, t_sample *fz1, t_sample *fz2)
{
    mayer_do_cfft(n, fz1, fz2, 1);
}

void mayer_ifft(int n, t_sample *fz1, t_sample *fz2)
{
    mayer_do_cfft(n, fz1, fz2, 0);
}

/*
    in the following the sign flips are done to
    be compatible with the mayer_fft implementation,
    but it's probably the mayer_fft that should be corrected...
*/

void mayer_realfft(int n, t_sample *fz)
{
    int i;
    rfftw_info *p = rfftw_getplan(n, 1);
    if (!p)
        return;

    for (i = 0; i < n; i++)
        p->in[i] = fz[i];
    fftwf_execute(p->plan);
    for (i = 0; i < n/2+1; i++)
        fz[i] = p->out[i];
    for (; i < n; i++)
        fz[i] = -p->out[i];
}

void mayer_realifft(int n, t_sample *fz)
{
    int i;
    rfftw_info *p = rfftw_getplan(n, 0);
    if (!p)
        return;

    for (i = 0; i < n/2+1; i++)
        p->in[i] = fz[i];
    for (; i < n; i++)
        p->in[i] = -fz[i];
    fftwf_execute(p->plan);
    for (i = 0; i < n; i++)
        fz[i] = p->out[i];
}

    /* ancient ISPW-like version, used in fiddle~ and perhaps other externs
    here and there. */
void pd_fft(t_float *buf, int npoints, int inverse)
{
    cfftw_info *p = cfftw_getplan(npoints, !inverse);
    int i;
    float *fz;
    for (i = 0, fz = (float *)(p->in); i < 2 * npoints; i++)
        *fz++ = buf[i];
    fftwf_execute(p->plan);
    for (i = 0, fz = (float *)(p->out); i < 2 * npoints; i++)
        buf[i] = *fz++;
}

