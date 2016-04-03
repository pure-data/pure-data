/*
 ###########################################################################
 # bonk~ - a Max/MSP external
 # by miller puckette and ted apel
 # http://crca.ucsd.edu/~msp/
 # Max/MSP port by barry threw
 # http://www.barrythrew.com
 # me@barrythrew.com
 # San Francisco, CA
 # (c) 2008
 # for Kesumo - http://www.kesumo.com
 ###########################################################################
 // bonk~ detects attacks in an audio signal
 ###########################################################################
 This software is copyrighted by Miller Puckette and others.  The following
 terms (the "Standard Improved BSD License") apply to all files associated with
 the software unless explicitly disclaimed in individual files:
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are
 met:
 
 1. Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above  
 copyright notice, this list of conditions and the following 
 disclaimer in the documentation and/or other materials provided
 with the distribution.
 3. The name of the author may not be used to endorse or promote
 products derived from this software without specific prior 
 written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,   
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
dolist:
decay and other times in msec 
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

/* These pragmas are only used for MSVC, not MinGW or Cygwin <hans@at.or.at> */
#ifdef _MSC_VER
#pragma warning (disable: 4305 4244)
#endif
 
#ifdef MSP
#include "ext.h"
#include "z_dsp.h"
#include "math.h"
#include "ext_support.h"
#include "ext_proto.h"
#include "ext_obex.h"

typedef double t_floatarg;      /* from m_pd.h */
#define flog log
#define fexp exp
#define fsqrt sqrt
#define t_resizebytes(a, b, c) t_resizebytes((char *)(a), (b), (c))

void *bonk_class;
#define getbytes t_getbytes
#define freebytes t_freebytes
#endif /* MSP */

#ifdef PD
#include "m_pd.h"
static t_class *bonk_class;
#endif

#ifdef _WIN32
#include <malloc.h>
#elif ! defined(_MSC_VER)
#include <alloca.h>
#endif

/* ------------------------ bonk~ ----------------------------- */

#define DEFNPOINTS 256
#define MAXCHANNELS 8
#define MINPOINTS 64
#define DEFPERIOD 128
#define DEFNFILTERS 11
#define DEFHALFTONES 6
#define DEFOVERLAP 1
#define DEFFIRSTBIN 1
#define DEFMINBANDWIDTH 1.5
#define DEFHITHRESH 5
#define DEFLOTHRESH 2.5
#define DEFMASKTIME 4
#define DEFMASKDECAY 0.7
#define DEFDEBOUNCEDECAY 0
#define DEFMINVEL 7
#define DEFATTACKBINS 1
#define MAXATTACKWAIT 4

typedef struct _filterkernel
{
    int k_filterpoints;
    int k_hoppoints;
    int k_skippoints;
    int k_nhops;
    t_float k_centerfreq;        /* center frequency, bins */
    t_float k_bandwidth;         /* bandwidth, bins */
    t_float *k_stuff;
} t_filterkernel;

typedef struct _filterbank
{
    int b_nfilters;             /* number of filters in bank */
    int b_npoints;              /* input vector size */
    t_float b_halftones;        /* filter bandwidth in halftones */
    t_float b_overlap;          /* overlap; default 1 for 1/2-power pts */
    t_float b_firstbin;         /* freq of first filter in bins, default 1 */
    t_float b_minbandwidth;     /* minimum bandwidth, default 1.5 */
    t_filterkernel *b_vec;      /* filter kernels */
    int b_refcount;             /* number of bonk~ objects using this */
    struct _filterbank *b_next; /* next in linked list */
} t_filterbank;

#if 0   /* this is the design for 1.0: */
static t_filterkernel bonk_filterkernels[] =
    {{256, 2, .01562}, {256, 4, .01562}, {256, 6, .01562}, {180, 6, .02222},
    {128, 6, .01803}, {90, 6, .02222}, {64, 6, .02362}, {46, 6, .02773},
    {32, 6, .03227}, {22, 6, .03932}, {16, 6, .04489}};
#endif

#if 0
    /* here's the 1.1 rev: */
static t_filterkernel bonk_filterkernels[] =
    {{256, 1, .01562, 0}, {256, 3, .01562, 0}, {256, 5, .01562, 0},
    {212, 6, .01886, 0}, {150, 6, .01885, 0}, {106, 6, .02179, 0},
    {76, 6, .0236, 0}, {54, 6, .02634, 0}, {38, 6, .03047, 0},
    {26, 6, .03667, 0}, {18, 6, .04458, 0}};

#define NFILTERS \
    ((int)(sizeof(bonk_filterkernels) / sizeof(bonk_filterkernels[0])))

#endif

#if 0
    /* and 1.2 */
#define NFILTERS 11
static t_filterkernel bonk_filterkernels[NFILTERS];
#endif

   /* and 1.3 */
#define MAXNFILTERS 50
#define MASKHIST 8

static t_filterbank *bonk_filterbanklist;

typedef struct _hist
{
    t_float h_power;
    t_float h_before;
    t_float h_outpower;
    int h_countup;
    t_float h_mask[MASKHIST];
} t_hist;

typedef struct template
{
    t_float t_amp[MAXNFILTERS];
} t_template;

typedef struct _insig
{
    t_hist g_hist[MAXNFILTERS];    /* history for each filter */
#ifdef PD
    t_outlet *g_outlet;         /* outlet for raw data */
#endif
#ifdef MSP
    void *g_outlet;             /* outlet for raw data */
#endif
    t_float *g_inbuf;           /* buffered input samples */
    t_sample *g_invec;           /* new input samples */
} t_insig;

typedef struct _bonk
{
#ifdef PD
    t_object x_obj;
    t_outlet *x_cookedout;
    t_clock *x_clock;
    t_canvas *x_canvas;     /* ptr to current canvas --fbar */
#endif /* PD */
#ifdef MSP
    t_pxobject x_obj;
    void *obex;
    void *x_cookedout;
    void *x_clock;
#endif /* MSP */
    /* parameters */
    int x_npoints;          /* number of points in input buffer */
    int x_period;           /* number of input samples between analyses */
    int x_nfilters;         /* number of filters requested */
    t_float x_halftones;    /* nominal halftones between filters */
    t_float x_overlap;
    t_float x_firstbin;
    t_float x_minbandwidth;
    t_float x_hithresh;     /* threshold for total growth to trigger */
    t_float x_lothresh;     /* threshold for total growth to re-arm */
    t_float x_minvel;       /* minimum velocity we output */
    t_float x_maskdecay;
    int x_masktime;
    int x_useloudness;      /* use loudness spectra instead of power */
    t_float x_debouncedecay;
    t_float x_debouncevel;
    double x_learndebounce; /* debounce time (in "learn" mode only) */
    int x_attackbins;       /* number of bins to wait for attack */

    t_filterbank *x_filterbank;
    t_hist x_hist[MAXNFILTERS];
    t_template *x_template;
    t_insig *x_insig;                   
    int x_ninsig;
    int x_ntemplate;
    int x_infill;
    int x_countdown;
    int x_willattack;
    int x_attacked;
    int x_debug;
    int x_learn;
    int x_learncount;           /* countup for "learn" mode */
    int x_spew;                 /* if true, always generate output! */
    int x_maskphase;            /* phase, 0 to MASKHIST-1, for mask history */
    t_float x_sr;               /* current sample rate in Hz. */
    int x_hit;                  /* next "tick" called because of a hit, not a poll */
} t_bonk;

#ifdef MSP
static void *bonk_new(t_symbol *s, long ac, t_atom *av);
static void bonk_tick(t_bonk *x);
static void bonk_doit(t_bonk *x);
static void bonk_perform_generic(t_bonk *x, int n);
static t_int *bonk_perform(t_int *w);
static void bonk_perform64(t_bonk *x, t_object *dsp64, double **ins,
                           long numins, double **outs, long numouts,
                           long sampleframes, long flags, void *userparam);
static void bonk_dsp(t_bonk *x, t_signal **sp);
static void bonk_dsp64(t_bonk *x, t_object *dsp64, short *count,
                       double samplerate, long maxvectorsize, long flags);
void bonk_assist(t_bonk *x, void *b, long m, long a, char *s);
static void bonk_free(t_bonk *x);
void bonk_setup(void);
int main();

static void bonk_thresh(t_bonk *x, t_floatarg f1, t_floatarg f2);
static void bonk_print(t_bonk *x, t_floatarg f);
static void bonk_bang(t_bonk *x);

static void bonk_write(t_bonk *x, t_symbol *s);
static void bonk_dowrite(t_bonk *x, t_symbol *s);
static void bonk_writefile(t_bonk *x, char *filename, short path);

static void bonk_read(t_bonk *x, t_symbol *s);
static void bonk_doread(t_bonk *x, t_symbol *s);
static void bonk_openfile(t_bonk *x, char *filename, short path);

void bonk_minvel_set(t_bonk *x, void *attr, long ac, t_atom *av);
void bonk_lothresh_set(t_bonk *x, void *attr, long ac, t_atom *av);
void bonk_hithresh_set(t_bonk *x, void *attr, long ac, t_atom *av);
void bonk_masktime_set(t_bonk *x, void *attr, long ac, t_atom *av);
void bonk_maskdecay_set(t_bonk *x, void *attr, long ac, t_atom *av);
void bonk_debouncedecay_set(t_bonk *x, void *attr, long ac, t_atom *av);
void bonk_debug_set(t_bonk *x, void *attr, long ac, t_atom *av);
void bonk_spew_set(t_bonk *x, void *attr, long ac, t_atom *av);
void bonk_useloudness_set(t_bonk *x, void *attr, long ac, t_atom *av);
void bonk_attackbins_set(t_bonk *x, void *attr, long ac, t_atom *av);
void bonk_learn_set(t_bonk *x, void *attr, long ac, t_atom *av);

t_float qrsqrt(t_float f);
double clock_getsystime();
double clock_gettimesince(double prevsystime);
char *strcpy(char *s1, const char *s2);
#define SETFLOAT A_SETFLOAT
#endif

static void bonk_tick(t_bonk *x);

#define HALFWIDTH 0.75  /* half peak bandwidth at half power point in bins */
#define SLIDE 0.25    /* relative slide between filter subwindows */

static t_filterbank *bonk_newfilterbank(int npoints, int nfilters,
    t_float halftones, t_float overlap, t_float firstbin, t_float minbandwidth)
{
    int i, j;
    t_float cf, bw, h, relspace;
    t_filterbank *b = (t_filterbank *)getbytes(sizeof(*b));
    b->b_npoints = npoints;
    b->b_nfilters = nfilters;
    b->b_halftones = halftones;
    b->b_overlap = overlap;
    b->b_firstbin = firstbin;
    b->b_minbandwidth = minbandwidth;
    b->b_refcount = 0;
    b->b_next = bonk_filterbanklist;
    bonk_filterbanklist = b;
    b->b_vec = (t_filterkernel *)getbytes(nfilters * sizeof(*b->b_vec));
    
    h = exp((log(2.)/12.)*halftones);  /* specced interval between filters */
    relspace = (h - 1)/(h + 1);        /* nominal spacing-per-f for fbank */
    
    if (minbandwidth < 2*HALFWIDTH)
        minbandwidth = 2*HALFWIDTH;
    if (firstbin < minbandwidth/(2*HALFWIDTH))
        firstbin = minbandwidth/(2*HALFWIDTH);
    cf = firstbin;
    bw = cf * relspace * overlap;
    if (bw < (0.5*minbandwidth))
        bw = (0.5*minbandwidth);
    for (i = 0; i < nfilters; i++)
    {
        t_float *fp, newcf, newbw;
        t_float normalizer = 0;
        int filterpoints, skippoints, hoppoints, nhops;
        
        filterpoints = npoints * HALFWIDTH/bw;
        if (cf > npoints/2)
        {
            post("bonk~: only using %d filters (ran past Nyquist)", i+1);
            break;
        }
        if (filterpoints < 4)
        {
            post("bonk~: only using %d filters (kernels got too short)", i+1);
            break;
        }
        else if (filterpoints > npoints)
            filterpoints = npoints;
        
        hoppoints = SLIDE * npoints * HALFWIDTH/bw;
        
        nhops = 1. + (npoints-filterpoints)/(t_float)hoppoints;
        skippoints = 0.5 * (npoints-filterpoints - (nhops-1) * hoppoints);
        
        b->b_vec[i].k_stuff =
            (t_float *)getbytes(2 * sizeof(t_float) * filterpoints);
        b->b_vec[i].k_filterpoints = filterpoints;
        b->b_vec[i].k_nhops = nhops;
        b->b_vec[i].k_hoppoints = hoppoints;
        b->b_vec[i].k_skippoints = skippoints;
        b->b_vec[i].k_centerfreq = cf;
        b->b_vec[i].k_bandwidth = bw;
        
        for (fp = b->b_vec[i].k_stuff, j = 0; j < filterpoints; j++, fp+= 2)
        {
            t_float phase = j * cf * (2*3.141592653589793 / npoints);
            t_float wphase = j * (2*3.141592653589793 / filterpoints);
            t_float window = sin(0.5*wphase);
            fp[0] = window * cos(phase);
            fp[1] = window * sin(phase);
            normalizer += window;
        }
        normalizer = 1/(normalizer * sqrt(nhops));
        for (fp = b->b_vec[i].k_stuff, j = 0;
             j < filterpoints; j++, fp+= 2)
            fp[0] *= normalizer, fp[1] *= normalizer;
#if 0
        post("i %d  cf %.2f  bw %.2f  nhops %d, hop %d, skip %d, npoints %d",
             i, cf, bw, nhops, hoppoints, skippoints, filterpoints);
#endif
        newcf = (cf + bw/overlap)/(1 - relspace);
        newbw = newcf * overlap * relspace;
        if (newbw < 0.5*minbandwidth)
        {
            newbw = 0.5*minbandwidth;
            newcf = cf + minbandwidth / overlap;
        }
        cf = newcf;
        bw = newbw;
    }
    for (; i < nfilters; i++)
        b->b_vec[i].k_stuff = 0, b->b_vec[i].k_filterpoints = 0;
    return (b);
}

static void bonk_freefilterbank(t_filterbank *b)
{
    t_filterbank *b2, *b3;
    int i;
    if (bonk_filterbanklist == b)
        bonk_filterbanklist = b->b_next;
    else for (b2 = bonk_filterbanklist; (b3 = b2->b_next); b2 = b3)
        if (b3 == b)
    {
        b2->b_next = b3->b_next;
        break;
    }
    for (i = 0; i < b->b_nfilters; i++)
        if (b->b_vec[i].k_stuff)
            freebytes(b->b_vec[i].k_stuff,
                b->b_vec[i].k_filterpoints * sizeof(t_float));
    freebytes(b, sizeof(*b));
}

static void bonk_donew(t_bonk *x, int npoints, int period, int nsig, 
    int nfilters, t_float halftones, t_float overlap, t_float firstbin,
    t_float minbandwidth, t_float samplerate)
{
    int i, j;
    t_hist *h;
    t_float *fp;
    t_insig *g;
    t_filterbank *fb;
    for (j = 0, g = x->x_insig; j < nsig; j++, g++)
    {
        for (i = 0, h = g->g_hist; i--; h++)
        {
            h->h_power = h->h_before = 0, h->h_countup = 0;
            for (j = 0; j < MASKHIST; j++)
                h->h_mask[j] = 0;
        }
            /* we ought to check for failure to allocate memory here */
        g->g_inbuf = (t_float *)getbytes(npoints * sizeof(t_float));
        for (i = npoints, fp = g->g_inbuf; i--; fp++) *fp = 0;
    }
    if (!period) period = npoints/2;
    x->x_npoints = npoints;
    x->x_period = period;
    x->x_ninsig = nsig;
    x->x_nfilters = nfilters;
    x->x_halftones = halftones;
    x->x_template = (t_template *)getbytes(0);
    x->x_ntemplate = 0;
    x->x_infill = 0;
    x->x_countdown = 0;
    x->x_willattack = 0;
    x->x_attacked = 0;
    x->x_maskphase = 0;
    x->x_debug = 0;
    x->x_hithresh = DEFHITHRESH;
    x->x_lothresh = DEFLOTHRESH;
    x->x_masktime = DEFMASKTIME;
    x->x_maskdecay = DEFMASKDECAY;
    x->x_learn = 0;
    x->x_learndebounce = clock_getsystime();
    x->x_learncount = 0;
    x->x_debouncedecay = DEFDEBOUNCEDECAY;
    x->x_minvel = DEFMINVEL;
    x->x_useloudness = 0;
    x->x_debouncevel = 0;
    x->x_attackbins = DEFATTACKBINS;
    x->x_sr = samplerate;
    x->x_filterbank = 0;
    x->x_hit = 0;
    for (fb = bonk_filterbanklist; fb; fb = fb->b_next)
        if (fb->b_nfilters == x->x_nfilters &&
            fb->b_halftones == x->x_halftones &&
            fb->b_firstbin == firstbin &&
            fb->b_overlap == overlap &&
            fb->b_npoints == x->x_npoints &&
            fb->b_minbandwidth == minbandwidth)
    {
        fb->b_refcount++;
        x->x_filterbank = fb;
        break;
    }
    if (!x->x_filterbank)
        x->x_filterbank = bonk_newfilterbank(npoints, nfilters, 
            halftones, overlap, firstbin, minbandwidth),
                x->x_filterbank->b_refcount++;
}

static void bonk_tick(t_bonk *x)
{
    t_atom at[MAXNFILTERS], *ap, at2[3];
    int i, j, k, n;
    t_hist *h;
    t_float *pp, vel = 0., temperature = 0.;
    t_float *fp;
    t_template *tp;
    int nfit, ninsig = x->x_ninsig, ntemplate = x->x_ntemplate, nfilters = x->x_nfilters;
    t_insig *gp;
#ifdef _MSC_VER
    t_float powerout[MAXNFILTERS*MAXCHANNELS];
#else
    t_float *powerout = alloca(x->x_nfilters * x->x_ninsig * sizeof(*powerout));
#endif
    
    for (i = ninsig, pp = powerout, gp = x->x_insig; i--; gp++)
    {
        for (j = 0, h = gp->g_hist; j < nfilters; j++, h++, pp++)
        {
            t_float power = h->h_outpower;
            t_float intensity = *pp = (power > 0. ? 100. * qrsqrt(qrsqrt(power)) : 0.);
            vel += intensity;
            temperature += intensity * (t_float)j;
        }
    }
    if (vel > 0) temperature /= vel;
    else temperature = 0;
    vel *= 0.5 / ninsig;        /* fudge factor */
    if (x->x_hit)
    {
        /* if hit nonzero it's a clock callback.  if in "learn" mode update the
         template list; in any event match the hit to known templates. */
        
        if (vel < x->x_debouncevel)
        {
            if (x->x_debug)
                post("bounce cancelled: vel %f debounce %f",
                     vel, x->x_debouncevel);
            return;
        }
        if (vel < x->x_minvel)
        {
            if (x->x_debug)
                post("low velocity cancelled: vel %f, minvel %f",
                     vel, x->x_minvel);
            return;
        }
        x->x_debouncevel = vel;
        if (x->x_learn)
        {
            double lasttime = x->x_learndebounce;
            double msec = clock_gettimesince(lasttime);
            if ((!ntemplate) || (msec > 200))
            {
                int countup = x->x_learncount;
                /* normalize to 100  */
                t_float norm;
                for (i = nfilters * ninsig, norm = 0, pp = powerout; i--; pp++)
                    norm += *pp * *pp;
                if (norm < 1.0e-15) norm = 1.0e-15;
                norm = 100. * qrsqrt(norm);
                /* check if this is the first strike for a new template */
                if (!countup)
                {
                    int oldn = ntemplate;
                    x->x_ntemplate = ntemplate = oldn + ninsig;
                    x->x_template = (t_template *)t_resizebytes(x->x_template,
                        oldn * sizeof(x->x_template[0]),
                            ntemplate * sizeof(x->x_template[0]));
                    for (i = ninsig, pp = powerout; i--; oldn++)
                        for (j = nfilters, fp = x->x_template[oldn].t_amp; j--;
                             pp++, fp++)
                                *fp = *pp * norm;
                }
                else
                {
                    int oldn = ntemplate - ninsig;
                    if (oldn < 0) post("bonk_tick bug");
                    for (i = ninsig, pp = powerout; i--; oldn++)
                    {
                        for (j = nfilters, fp = x->x_template[oldn].t_amp; j--;
                             pp++, fp++)
                            *fp = (countup * *fp + *pp * norm)
                            /(countup + 1.0);
                    }
                }
                countup++;
                if (countup == x->x_learn) countup = 0;
                x->x_learncount = countup;
            }
            else return;
        }
        x->x_learndebounce = clock_getsystime();
        if (ntemplate)
        {
            t_float bestfit = -1e30;
            int templatecount;
            nfit = -1;
            for (i = 0, templatecount = 0, tp = x->x_template; 
                 templatecount < ntemplate; i++)
            {
                t_float dotprod = 0;
                for (k = 0, pp = powerout;
                     k < ninsig && templatecount < ntemplate;
                     k++, tp++, templatecount++)
                {
                    for (j = nfilters, fp = tp->t_amp;
                         j--; fp++, pp++)
                    {
                        if (*fp < 0 || *pp < 0) post("bonk_tick bug 2");
                        dotprod += *fp * *pp;
                    }
                }
                if (dotprod > bestfit)
                {
                    bestfit = dotprod;
                    nfit = i;
                }
            }
            if (nfit < 0) post("bonk_tick bug");
        }
        else nfit = 0;
    }
    else nfit = -1;     /* hit is zero; this is the "bang" method. */
    
    x->x_attacked = 1;
    if (x->x_debug)
        post("bonk out: number %d, vel %f, temperature %f",
            nfit, vel, temperature);
    
    SETFLOAT(at2, nfit);
    SETFLOAT(at2+1, vel);
    SETFLOAT(at2+2, temperature);
    outlet_list(x->x_cookedout, 0, 3, at2);
    
    for (n = 0, gp = x->x_insig + (ninsig-1),
        pp = powerout + nfilters * (ninsig-1); n < ninsig;
            n++, gp--, pp -= nfilters)
    {
        t_float *pp2;
        for (i = 0, ap = at, pp2 = pp; i < nfilters;
            i++, ap++, pp2++)
        {
            ap->a_type = A_FLOAT;
            ap->a_w.w_float = *pp2;
        }
        outlet_list(gp->g_outlet, 0, nfilters, at);
    }
}

static void bonk_doit(t_bonk *x)
{
    int i, j, ch, n;
    t_filterkernel *k;
    t_hist *h;
    t_float growth = 0, *fp1, *fp3, *fp4, hithresh, lothresh;
    int ninsig = x->x_ninsig, nfilters = x->x_nfilters,
        maskphase = x->x_maskphase, nextphase, oldmaskphase;
    t_insig *gp;
    nextphase = maskphase + 1;
    if (nextphase >= MASKHIST)
        nextphase = 0;
    x->x_maskphase = nextphase;
    oldmaskphase = nextphase - x->x_attackbins;
    if (oldmaskphase < 0)
        oldmaskphase += MASKHIST;
    if (x->x_useloudness)
        hithresh = qrsqrt(qrsqrt(x->x_hithresh)),
        lothresh = qrsqrt(qrsqrt(x->x_lothresh));
    else hithresh = x->x_hithresh, lothresh = x->x_lothresh;
    for (ch = 0, gp = x->x_insig; ch < ninsig; ch++, gp++)
    {
        for (i = 0, k = x->x_filterbank->b_vec, h = gp->g_hist;
             i < nfilters; i++, k++, h++)
        {
            t_float power = 0, maskpow = h->h_mask[maskphase];
            t_float *inbuf= gp->g_inbuf + k->k_skippoints;
            int countup = h->h_countup;
            int filterpoints = k->k_filterpoints;
            /* if the user asked for more filters that fit under the
             Nyquist frequency, some filters won't actually be filled in
             so we skip running them. */
            if  (!filterpoints)
            {
                h->h_countup = 0;
                h->h_mask[nextphase] = 0;
                h->h_power = 0;
                continue;
            }
            /* run the filter repeatedly, sliding it forward by hoppoints,
             for nhop times */
            for (fp1 = inbuf, n = 0;
                 n < k->k_nhops; fp1 += k->k_hoppoints, n++)
            {
                t_float rsum = 0, isum = 0;
                for (fp3 = fp1, fp4 = k->k_stuff, j = filterpoints; j--;)
                {
                    t_float g = *fp3++;
                    rsum += g * *fp4++;
                    isum += g * *fp4++;
                }
                power += rsum * rsum + isum * isum;
            }
            if (!x->x_willattack) 
                h->h_before = maskpow;
            
            if (power > h->h_mask[oldmaskphase])
            {
                if (x->x_useloudness)
                    growth += qrsqrt(qrsqrt(
                        power/(h->h_mask[oldmaskphase] + 1.0e-15))) - 1.;
                else growth += power/(h->h_mask[oldmaskphase] + 1.0e-15) - 1.;
            }
            if (!x->x_willattack && countup >= x->x_masktime)
                maskpow *= x->x_maskdecay;
            
            if (power > maskpow)
            {
                maskpow = power;
                countup = 0;
            }
            countup++;
            h->h_countup = countup;
            h->h_mask[nextphase] = maskpow;
            h->h_power = power;
        }
    }
    if (x->x_willattack)
    {
        if (x->x_willattack > MAXATTACKWAIT || growth < x->x_lothresh)
        {
            /* if haven't yet, and if not in spew mode, report a hit */
            if (!x->x_spew && !x->x_attacked)
            {
                for (ch = 0, gp = x->x_insig; ch < ninsig; ch++, gp++)
                    for (i = nfilters, h = gp->g_hist; i--; h++)
                        h->h_outpower = h->h_mask[nextphase];
                x->x_hit = 1;
                clock_delay(x->x_clock, 0);
            }
        }
        if (growth < x->x_lothresh)
            x->x_willattack = 0;
        else x->x_willattack++;
    }
    else if (growth > x->x_hithresh)
    {
        if (x->x_debug) post("attack: growth = %f", growth);
        x->x_willattack = 1;
        x->x_attacked = 0;
        for (ch = 0, gp = x->x_insig; ch < ninsig; ch++, gp++)
            for (i = nfilters, h = gp->g_hist; i--; h++)
                h->h_mask[nextphase] = h->h_power, h->h_countup = 0;
    }
    
    /* if in "spew" mode just always output */
    if (x->x_spew)
    {
        for (ch = 0, gp = x->x_insig; ch < ninsig; ch++, gp++)
            for (i = nfilters, h = gp->g_hist; i--; h++)
                h->h_outpower = h->h_power;
        x->x_hit = 0;
        clock_delay(x->x_clock, 0);
    }
    x->x_debouncevel *= x->x_debouncedecay;
}

static void bonk_perform_generic(t_bonk *x, int n) {
    int onset = 0;
    if (x->x_countdown >= n)
        x->x_countdown -= n;
    else
    {
        int i, j, ninsig = x->x_ninsig;
        t_insig *gp;
        if (x->x_countdown > 0)
        {
            n -= x->x_countdown;
            onset += x->x_countdown;
            x->x_countdown = 0;
        }
        while (n > 0)
        {
            int infill = x->x_infill;
            int m = (n < (x->x_npoints - infill) ?
                     n : (x->x_npoints - infill));
            for (i = 0, gp = x->x_insig; i < ninsig; i++, gp++)
            {
                t_float *fp = gp->g_inbuf + infill;
                t_sample *in1 = gp->g_invec + onset;
                for (j = 0; j < m; j++)
                    *fp++ = *in1++;
            }
            infill += m;
            x->x_infill = infill;
            if (infill == x->x_npoints)
            {
                bonk_doit(x);
                
                /* shift or clear the input buffer and update counters */
                if (x->x_period > x->x_npoints)
                    x->x_countdown = x->x_period - x->x_npoints;
                else x->x_countdown = 0;
                if (x->x_period < x->x_npoints)
                {
                    int overlap = x->x_npoints - x->x_period;
                    t_float *fp1, *fp2;
                    for (n = 0, gp = x->x_insig; n < ninsig; n++, gp++)
                        for (i = overlap, fp1 = gp->g_inbuf,
                             fp2 = fp1 + x->x_period; i--;)
                            *fp1++ = *fp2++;
                    x->x_infill = overlap;
                }
                else x->x_infill = 0;
            }
            n -= m;
            onset += m;
        }
    }
}

static t_int *bonk_perform(t_int *w)
{
    t_bonk *x = (t_bonk *)(w[1]);
    int n = (int)(w[2]);
    bonk_perform_generic(x, n);
    return (w+3);
}

static void bonk_dsp(t_bonk *x, t_signal **sp)
{
    int i, n = sp[0]->s_n, ninsig = x->x_ninsig;
    t_insig *gp;
    
    x->x_sr = sp[0]->s_sr;
    
    for (i = 0, gp = x->x_insig; i < ninsig; i++, gp++)
        gp->g_invec = (*(sp++))->s_vec;
    
    dsp_add(bonk_perform, 2, x, n);
}

static void bonk_thresh(t_bonk *x, t_floatarg f1, t_floatarg f2)
{
    if (f1 > f2)
        post("bonk: warning: low threshold greater than hi threshold");
    x->x_lothresh = (f1 <= 0 ? 0.0001 : f1);
    x->x_hithresh = (f2 <= 0 ? 0.0001 : f2);
}

#ifdef PD
static void bonk_mask(t_bonk *x, t_floatarg f1, t_floatarg f2)
{
    int ticks = f1;
    if (ticks < 0) ticks = 0;
    if (f2 < 0) f2 = 0;
    else if (f2 > 1) f2 = 1;
    x->x_masktime = ticks;
    x->x_maskdecay = f2;
}

static void bonk_debounce(t_bonk *x, t_floatarg f1)
{
    if (f1 < 0) f1 = 0;
    else if (f1 > 1) f1 = 1;
    x->x_debouncedecay = f1;
}

static void bonk_minvel(t_bonk *x, t_floatarg f)
{
    if (f < 0) f = 0; 
    x->x_minvel = f;
}

static void bonk_debug(t_bonk *x, t_floatarg f)
{
    x->x_debug = (f != 0);
}

static void bonk_spew(t_bonk *x, t_floatarg f)
{
    x->x_spew = (f != 0);
}

static void bonk_useloudness(t_bonk *x, t_floatarg f)
{
    x->x_useloudness = (f != 0);
}

static void bonk_attackbins(t_bonk *x, t_floatarg f)
{
    if (f < 1)
        f = 1;
    else if (f > MASKHIST)
        f = MASKHIST;
    x->x_attackbins = f;
}

static void bonk_learn(t_bonk *x, t_floatarg f)
{
    int n = f;
    if (n < 0) n = 0;
    if (n)
    {
        x->x_template = (t_template *)t_resizebytes(x->x_template,
            x->x_ntemplate * sizeof(x->x_template[0]), 0);
        x->x_ntemplate = 0;
    }
    x->x_learn = n;
    x->x_learncount = 0;
}
#endif

static void bonk_print(t_bonk *x, t_floatarg f)
{
    int i;
    post("thresh %f %f", x->x_lothresh, x->x_hithresh);
    post("mask %d %f", x->x_masktime, x->x_maskdecay);
    post("attack-frames %d", x->x_attackbins);
    post("debounce %f", x->x_debouncedecay);
    post("minvel %f", x->x_minvel);
    post("spew %d", x->x_spew);
    post("useloudness %d", x->x_useloudness);
    
#if 0       /* LATER rewrite without hard-coded 11 filters */
    if (x->x_ntemplate)
    {
        post("templates:");
        for (i = 0; i < x->x_ntemplate; i++)
            post(
"%2d %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f",
                i,
                x->x_template[i].t_amp[0],
                x->x_template[i].t_amp[1],
                x->x_template[i].t_amp[2],
                x->x_template[i].t_amp[3],
                x->x_template[i].t_amp[4],
                x->x_template[i].t_amp[5],
                x->x_template[i].t_amp[6],
                x->x_template[i].t_amp[7],
                x->x_template[i].t_amp[8],
                x->x_template[i].t_amp[9],
                x->x_template[i].t_amp[10]);
    }
    else post("no templates");
#endif
    post("number of templates %d", x->x_ntemplate);
    if (x->x_learn) post("learn mode");
    if (f != 0)
    {
        int j, ninsig = x->x_ninsig;
        t_insig *gp;
        for (j = 0, gp = x->x_insig; j < ninsig; j++, gp++)
        {
            t_hist *h;
            if (ninsig > 1) post("input %d:", j+1);
            for (i = x->x_nfilters, h = gp->g_hist; i--; h++)
                post("pow %f mask %f before %f count %d",
                     h->h_power, h->h_mask[x->x_maskphase],
                     h->h_before, h->h_countup);
        }
        post("filter details (frequencies are in units of %.2f-Hz. bins):",
             x->x_sr/x->x_npoints);
        for (j = 0; j < x->x_nfilters; j++)
            post("%2d  cf %.2f  bw %.2f  nhops %d hop %d skip %d npoints %d",
                 j, 
                 x->x_filterbank->b_vec[j].k_centerfreq,
                 x->x_filterbank->b_vec[j].k_bandwidth,
                 x->x_filterbank->b_vec[j].k_nhops,
                 x->x_filterbank->b_vec[j].k_hoppoints,
                 x->x_filterbank->b_vec[j].k_skippoints,
                 x->x_filterbank->b_vec[j].k_filterpoints);
    }
    if (x->x_debug) post("debug mode");
}

static void bonk_forget(t_bonk *x)
{
    int ntemplate = x->x_ntemplate, newn = ntemplate - x->x_ninsig;
    if (newn < 0) newn = 0;
    x->x_template = (t_template *)t_resizebytes(x->x_template,
        x->x_ntemplate * sizeof(x->x_template[0]),
            newn * sizeof(x->x_template[0]));
    x->x_ntemplate = newn;
    x->x_learncount = 0;
}

static void bonk_bang(t_bonk *x)
{
    int i, ch;
    t_insig *gp;
    x->x_hit = 0;
    for (ch = 0, gp = x->x_insig; ch < x->x_ninsig; ch++, gp++)
    {
        t_hist *h;
        for (i = 0, h = gp->g_hist; i < x->x_nfilters; i++, h++)
            h->h_outpower = h->h_power;
    }
    bonk_tick(x);
}

#ifdef PD
static void bonk_read(t_bonk *x, t_symbol *s)
{
    float vec[MAXNFILTERS];
    int i, ntemplate = 0, remaining;
    float *fp;
    t_float *fp2;

    /* fbar: canvas_open code taken from g_array.c */
    FILE *fd;
    char buf[MAXPDSTRING], *bufptr;
    int filedesc;

    if ((filedesc = canvas_open(x->x_canvas,
            s->s_name, "", buf, &bufptr, MAXPDSTRING, 0)) < 0 
                || !(fd = fdopen(filedesc, "r")))
    {
        post("%s: open failed", s->s_name);
        return;
    }
    x->x_template = (t_template *)t_resizebytes(x->x_template, 
        x->x_ntemplate * sizeof(t_template), 0);
    while (1)
    {
        for (i = x->x_nfilters, fp = vec; i--; fp++)
            if (fscanf(fd, "%f", fp) < 1) goto nomore;
        x->x_template = (t_template *)t_resizebytes(x->x_template,
            ntemplate * sizeof(t_template),
                (ntemplate + 1) * sizeof(t_template));
        for (i = x->x_nfilters, fp = vec,
             fp2 = x->x_template[ntemplate].t_amp; i--;)
            *fp2++ = *fp++;
        ntemplate++;
    }
nomore:
    if ((remaining = (ntemplate % x->x_ninsig)))
    {
        post("bonk_read: %d templates not a multiple of %d; dropping extras");
        x->x_template = (t_template *)t_resizebytes(x->x_template,
            ntemplate * sizeof(t_template),
                (ntemplate - remaining) * sizeof(t_template));
        ntemplate = ntemplate - remaining;
    }
    post("bonk: read %d templates\n", ntemplate);
    x->x_ntemplate = ntemplate;
    fclose(fd);
}
#endif

#ifdef MSP
static void bonk_perform64(t_bonk *x, t_object *dsp64, double **ins,
                           long numins, double **outs, long numouts,
                           long sampleframes, long flags, void *userparam)
{
    int n = sampleframes;

    int i = sampleframes, ninsig = x->x_ninsig;
    t_insig *gp;

    for (i = 0, gp = x->x_insig; i < ninsig; i++, gp++)
        gp->g_invec = ins[0];

    bonk_perform_generic(x, n);
}

static void bonk_dsp64(t_bonk *x, t_object *dsp64, short *count,
                       double samplerate, long maxvectorsize, long flags)
{

    x->x_sr = samplerate;
    object_method(dsp64, gensym("dsp_add64"), x, bonk_perform64, 0, NULL);
}


static void bonk_read(t_bonk *x, t_symbol *s)
{
    defer(x, (method)bonk_doread, s, 0, NULL);
}

static void bonk_doread(t_bonk *x, t_symbol *s)
{
    t_fourcc filetype = 'TEXT', outtype;
    char filename[512];
    short path;
    
    if (s == gensym("")) {
        if (open_dialog(filename, &path, &outtype, &filetype, 1))
            return;
    } else {
        strcpy(filename, s->s_name);
        if (locatefile_extended(filename, &path, &outtype, &filetype, 1)) {
            object_error((t_object *) x, "%s: not found", s->s_name);
            return;
        }
    }
    // we have a file
    bonk_openfile(x, filename, path);
}

static void bonk_openfile(t_bonk *x, char *filename, short path) {
    t_float vec[MAXNFILTERS];
    int i, ntemplate = 0, remaining;
    t_float *fp, *fp2;
    
    t_filehandle fh;
    char **texthandle;
    char *tokptr;
    
    t_ptr_size size;

    if (path_opensysfile(filename, path, &fh, READ_PERM)) {
        object_error((t_object *) x, "error opening %s", filename);
        return;
    }
    
    sysfile_geteof(fh, &size); 
    texthandle = sysmem_newhandleclear(size + 1);
    sysfile_readtextfile(fh, texthandle, 0, TEXT_LB_NATIVE);
    sysfile_close(fh);
    
    x->x_template = (t_template *)t_resizebytes(x->x_template, 
                                                x->x_ntemplate * sizeof(t_template), 0);
    
    tokptr = strtok(*texthandle, " \n");
    
    while(tokptr != NULL)
    {
        for (i = x->x_nfilters, fp = vec; i--; fp++) {
            if (sscanf(tokptr, "%f", fp) < 1) 
                goto nomore;
            tokptr = strtok(NULL, " \n");
        }
        x->x_template = (t_template *)t_resizebytes(x->x_template,
                                                    ntemplate * sizeof(t_template),
                                                    (ntemplate + 1) * sizeof(t_template));
        for (i = x->x_nfilters, fp = vec,
             fp2 = x->x_template[ntemplate].t_amp; i--;)
            *fp2++ = *fp++;
        ntemplate++;
    }
nomore:
    if ((remaining = (ntemplate % x->x_ninsig)))
    {
        post("bonk_read: %d templates not a multiple of %d; dropping extras");
        x->x_template = (t_template *)t_resizebytes(x->x_template,
                                                    ntemplate * sizeof(t_template),
                                                    (ntemplate - remaining) * sizeof(t_template));
        ntemplate = ntemplate - remaining;
    }
    
    sysmem_freehandle(texthandle);
    post("bonk: read %d templates\n", ntemplate);
    x->x_ntemplate = ntemplate;
}
#endif

#ifdef PD
static void bonk_write(t_bonk *x, t_symbol *s)
{
    FILE *fd;
    char buf[MAXPDSTRING]; /* fbar */
    int i, ntemplate = x->x_ntemplate;
    t_template *tp = x->x_template;
    t_float *fp;
    
    /* fbar: canvas-code as in g_array.c */
    canvas_makefilename(x->x_canvas, s->s_name,
        buf, MAXPDSTRING);
    sys_bashfilename(buf, buf);

    if (!(fd = fopen(buf, "w")))
    {
        post("%s: couldn't create", s->s_name);
        return;
    }
    for (; ntemplate--; tp++)
    {
        for (i = x->x_nfilters, fp = tp->t_amp; i--; fp++)
            fprintf(fd, "%6.2f ", *fp);
        fprintf(fd, "\n");
    }
    post("bonk: wrote %d templates\n", x->x_ntemplate);
    fclose(fd);
}
#endif

#ifdef MSP
static void bonk_write(t_bonk *x, t_symbol *s)
{
    defer(x, (method)bonk_dowrite, s, 0, NULL);
}

static void bonk_dowrite(t_bonk *x, t_symbol *s)
{
    t_fourcc filetype = 'TEXT', outtype;
    char filename[MAX_FILENAME_CHARS];
    short path;
    
    if (s == gensym("")) {
        sprintf(filename, "bonk_template.txt");
        saveas_promptset("Save template as...");   
        if (saveasdialog_extended(filename, &path, &outtype, &filetype, 0))
            return;
    } else {
        strcpy(filename, s->s_name);
        path = path_getdefault();
    }
    bonk_writefile(x, filename, path);
}

void bonk_writefile(t_bonk *x, char *filename, short path)
{
    int i, ntemplate = x->x_ntemplate;
    t_template *tp = x->x_template;
    t_float *fp;
    long err;
    t_ptr_size buflen;
    
    t_filehandle fh;
    
    char buf[20];
    
    err = path_createsysfile(filename, path, 'TEXT', &fh); 
    
    if (err)
        return;
    
    for (; ntemplate--; tp++)
    {
        for (i = x->x_nfilters, fp = tp->t_amp; i--; fp++) {
            snprintf(buf, 20, "%6.2f ", *fp);
            buflen = strlen(buf);
            sysfile_write(fh, &buflen, buf);
        }
        buflen = 1;
        sysfile_write(fh, &buflen, "\n");
    }
        
    sysfile_close(fh);
}
#endif

static void bonk_free(t_bonk *x)
{
    
    int i, ninsig = x->x_ninsig;
    t_insig *gp = x->x_insig;
#ifdef MSP
    dsp_free((t_pxobject *)x);
#endif
    for (i = 0, gp = x->x_insig; i < ninsig; i++, gp++)
        freebytes(gp->g_inbuf, x->x_npoints * sizeof(t_float));
    clock_free(x->x_clock);
    if (!--(x->x_filterbank->b_refcount))
        bonk_freefilterbank(x->x_filterbank);
    
}

/* -------------------------- Pd glue ------------------------- */
#ifdef PD

static void *bonk_new(t_symbol *s, int argc, t_atom *argv)
{
    t_bonk *x = (t_bonk *)pd_new(bonk_class);
    int nsig = 1, period = DEFPERIOD, npts = DEFNPOINTS,
        nfilters = DEFNFILTERS, j;
    t_float halftones = DEFHALFTONES, overlap = DEFOVERLAP,
        firstbin = DEFFIRSTBIN, minbandwidth = DEFMINBANDWIDTH;
    t_insig *g;

    x->x_canvas = canvas_getcurrent(); /* fbar: bind current canvas to x */
    if (argc > 0 && argv[0].a_type == A_FLOAT)
    {
            /* old style args for compatibility */
        period = atom_getfloatarg(0, argc, argv);
        nsig = atom_getfloatarg(1, argc, argv);
    }
    else while (argc > 0)
    {
        t_symbol *firstarg = atom_getsymbolarg(0, argc, argv);
        if (!strcmp(firstarg->s_name, "-npts") && argc > 1)
        {
            npts = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-hop") && argc > 1)
        {
            period = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-nsigs") && argc > 1)
        {
            nsig = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-nfilters") && argc > 1)
        {
            nfilters = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-halftones") && argc > 1)
        {
            halftones = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-overlap") && argc > 1)
        {
            overlap = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-firstbin") && argc > 1)
        {
            firstbin = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-minbandwidth") && argc > 1)
        {
            minbandwidth = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-spew") && argc > 1)
        {
            x->x_spew = (atom_getfloatarg(1, argc, argv) != 0);
            argc -= 2; argv += 2;
        }
        else
        {
            pd_error(x,
"usage is: bonk [-npts #] [-hop #] [-nsigs #] [-nfilters #] [-halftones #]"); 
            post(
"... [-overlap #] [-firstbin #] [-spew #]");
            argc = 0;
        }
    }

    x->x_npoints = (npts >= MINPOINTS ? npts : DEFNPOINTS);
    x->x_period = (period >= 1 ? period : npts/2);
    x->x_nfilters = (nfilters >= 1 ? nfilters : DEFNFILTERS);
    if (halftones < 0.01)
        halftones = DEFHALFTONES;
    else if (halftones > 12)
        halftones = 12;
    if (nsig < 1)
        nsig = 1;
    else if (nsig > MAXCHANNELS)
        nsig = MAXCHANNELS;
    if (firstbin < 0.5)
        firstbin = 0.5;
    if (overlap < 1)
        overlap = 1;

    x->x_clock = clock_new(x, (t_method)bonk_tick);
    x->x_insig = (t_insig *)getbytes(nsig * sizeof(*x->x_insig));
    for (j = 0, g = x->x_insig; j < nsig; j++, g++)
    {
        g->g_outlet = outlet_new(&x->x_obj, gensym("list"));
        if (j)
            inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    }
    x->x_cookedout = outlet_new(&x->x_obj, gensym("list"));
    bonk_donew(x, npts, period, nsig, nfilters, halftones, overlap,
        firstbin, minbandwidth, sys_getsr());
    return (x);
}

void bonk_tilde_setup(void)
{
    bonk_class = class_new(gensym("bonk~"), (t_newmethod)bonk_new,
        (t_method)bonk_free, sizeof(t_bonk), 0, A_GIMME, 0);
    class_addmethod(bonk_class, nullfn, gensym("signal"), 0);
    class_addmethod(bonk_class, (t_method)bonk_dsp, gensym("dsp"), 0);
    class_addbang(bonk_class, bonk_bang);
    class_addmethod(bonk_class, (t_method)bonk_learn,
        gensym("learn"), A_FLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_forget, gensym("forget"), 0);
    class_addmethod(bonk_class, (t_method)bonk_thresh,
        gensym("thresh"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_mask,
        gensym("mask"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_debounce,
        gensym("debounce"), A_FLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_minvel,
        gensym("minvel"), A_FLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_print,
        gensym("print"), A_DEFFLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_debug,
        gensym("debug"), A_DEFFLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_spew,
        gensym("spew"), A_DEFFLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_useloudness,
        gensym("useloudness"), A_DEFFLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_attackbins,
        gensym("attack-bins"), A_DEFFLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_attackbins,
        gensym("attack-frames"), A_DEFFLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_read,
        gensym("read"), A_SYMBOL, 0);
    class_addmethod(bonk_class, (t_method)bonk_write,
        gensym("write"), A_SYMBOL, 0);
    post("bonk version 1.5");
}
#endif

/* -------------------------- MSP glue ------------------------- */
#ifdef MSP

int main()
{       
        t_class *c;
        t_object *attr;
        long attrflags = 0;
        t_symbol *sym_long = gensym("long"), *sym_float32 = gensym("float32");
        
        c = class_new("bonk~", (method)bonk_new, (method)bonk_free, sizeof(t_bonk), (method)0L, A_GIMME, 0);
        
        class_obexoffset_set(c, calcoffset(t_bonk, obex));
        
        attr = attr_offset_new("npoints", sym_long, attrflags, (method)0L, (method)0L, calcoffset(t_bonk, x_npoints));
        class_addattr(c, attr);
        
        attr = attr_offset_new("hop", sym_long, attrflags, (method)0L, (method)0L, calcoffset(t_bonk, x_period));
        class_addattr(c, attr);
        
        attr = attr_offset_new("nfilters", sym_long, attrflags, (method)0L, (method)0L, calcoffset(t_bonk, x_nfilters));
        class_addattr(c, attr);
        
        attr = attr_offset_new("halftones", sym_float32, attrflags, (method)0L, (method)0L, calcoffset(t_bonk, x_halftones));
        class_addattr(c, attr);
        
        attr = attr_offset_new("overlap", sym_float32, attrflags, (method)0L, (method)0L, calcoffset(t_bonk, x_overlap));
        class_addattr(c, attr);
        
        attr = attr_offset_new("firstbin", sym_float32, attrflags, (method)0L, (method)0L, calcoffset(t_bonk, x_firstbin));
        class_addattr(c, attr);
        
        attr = attr_offset_new("minbandwidth", sym_float32, attrflags, (method)0L, (method)0L, calcoffset(t_bonk, x_minbandwidth));
        class_addattr(c, attr);
        
        attr = attr_offset_new("minvel", sym_float32, attrflags, (method)0L, (method)bonk_minvel_set, calcoffset(t_bonk, x_minvel));
        class_addattr(c, attr);
        
        attr = attr_offset_new("lothresh", sym_float32, attrflags, (method)0L, (method)bonk_lothresh_set, calcoffset(t_bonk, x_lothresh));
        class_addattr(c, attr);
        
        attr = attr_offset_new("hithresh", sym_float32, attrflags, (method)0L, (method)bonk_hithresh_set, calcoffset(t_bonk, x_hithresh));
        class_addattr(c, attr);
        
        attr = attr_offset_new("masktime", sym_long, attrflags, (method)0L, (method)bonk_masktime_set, calcoffset(t_bonk, x_masktime));
        class_addattr(c, attr);
        
        attr = attr_offset_new("maskdecay", sym_float32, attrflags, (method)0L, (method)bonk_maskdecay_set, calcoffset(t_bonk, x_maskdecay));
        class_addattr(c, attr);
    
        attr = attr_offset_new("debouncedecay", sym_float32, attrflags, (method)0L, (method)bonk_debouncedecay_set, calcoffset(t_bonk, x_debouncedecay));
        class_addattr(c, attr);
        
        attr = attr_offset_new("debug", sym_long, attrflags, (method)0L, (method)bonk_debug_set, calcoffset(t_bonk, x_debug));
        class_addattr(c, attr);
        
        attr = attr_offset_new("spew", sym_long, attrflags, (method)0L, (method)bonk_spew_set, calcoffset(t_bonk, x_spew));
        class_addattr(c, attr);
        
        attr = attr_offset_new("useloudness", sym_long, attrflags, (method)0L, (method)bonk_useloudness_set, calcoffset(t_bonk, x_useloudness));
        class_addattr(c, attr);

        attr = attr_offset_new("attackframes", sym_long, attrflags, (method)0L, (method)bonk_attackbins_set, calcoffset(t_bonk, x_attackbins));
        class_addattr(c, attr);
        
        attr = attr_offset_new("learn", sym_long, attrflags, (method)0L, (method)bonk_learn_set, calcoffset(t_bonk, x_learn));
        class_addattr(c, attr);
    
        class_addmethod(c, (method)bonk_dsp, "dsp", A_CANT, 0);
        class_addmethod(c, (method)bonk_dsp64, "dsp64", A_CANT, 0);
        class_addmethod(c, (method)bonk_bang, "bang", A_CANT, 0);
        class_addmethod(c, (method)bonk_forget, "forget", 0);
        class_addmethod(c, (method)bonk_thresh, "thresh", A_FLOAT, A_FLOAT, 0);
        class_addmethod(c, (method)bonk_print, "print", A_DEFFLOAT, 0);
        class_addmethod(c, (method)bonk_read, "read", A_DEFSYM, 0);
        class_addmethod(c, (method)bonk_write, "write", A_DEFSYM, 0);
        class_addmethod(c, (method)bonk_assist, "assist", A_CANT, 0);
        
        class_addmethod(c, (method)object_obex_dumpout, "dumpout", A_CANT, 0);
        class_addmethod(c, (method)object_obex_quickref, "quickref", A_CANT, 0);
        
        class_dspinit(c);
    
        class_register(CLASS_BOX, c);
        bonk_class = c;
        
        post("bonk~ v1.5");
        return (0);
}

static void *bonk_new(t_symbol *s, long ac, t_atom *av)
{
    short j;
    t_bonk *x;
        
    if ((x = (t_bonk *)object_alloc(bonk_class))) {
        
        t_insig *g;

        x->x_npoints = DEFNPOINTS;
        x->x_period = DEFPERIOD;
        x->x_nfilters = DEFNFILTERS;
        x->x_halftones = DEFHALFTONES;
        x->x_firstbin = DEFFIRSTBIN;
        x->x_minbandwidth = DEFMINBANDWIDTH;
        x->x_overlap = DEFOVERLAP;
        x->x_ninsig = 1;

        x->x_hithresh = DEFHITHRESH;
        x->x_lothresh = DEFLOTHRESH;
        x->x_masktime = DEFMASKTIME;
        x->x_maskdecay = DEFMASKDECAY;
        x->x_debouncedecay = DEFDEBOUNCEDECAY;
        x->x_minvel = DEFMINVEL;
        x->x_attackbins = DEFATTACKBINS;
        
        if (!x->x_period) x->x_period = x->x_npoints/2;
        x->x_template = (t_template *)getbytes(0);
        x->x_ntemplate = 0;
        x->x_infill = 0;
        x->x_countdown = 0;
        x->x_willattack = 0;
        x->x_attacked = 0;
        x->x_maskphase = 0;
        x->x_debug = 0;
        x->x_learn = 0;
        x->x_learndebounce = clock_getsystime();
        x->x_learncount = 0;
        x->x_useloudness = 0;
        x->x_debouncevel = 0;
        x->x_sr = sys_getsr();
        
        if (ac) {
            switch (av[0].a_type) {
                case A_LONG:
                    x->x_ninsig = av[0].a_w.w_long;
                    break;
            }
        }

        if (x->x_ninsig < 1) x->x_ninsig = 1;
        if (x->x_ninsig > MAXCHANNELS) x->x_ninsig = MAXCHANNELS;
        
        attr_args_process(x, ac, av);   

        x->x_insig = (t_insig *)getbytes(x->x_ninsig * sizeof(*x->x_insig));

        dsp_setup((t_pxobject *)x, x->x_ninsig);

        object_obex_store(x, gensym("dumpout"), outlet_new(x, NULL));

        x->x_cookedout = listout((t_object *)x);

        for (j = 0, g = x->x_insig + x->x_ninsig-1; j < x->x_ninsig; j++, g--) {
                g->g_outlet = listout((t_object *)x);
        }

        x->x_clock = clock_new(x, (method)bonk_tick);

        bonk_donew(x, x->x_npoints, x->x_period, x->x_ninsig, x->x_nfilters,
            x->x_halftones, x->x_overlap, x->x_firstbin, x->x_minbandwidth,
                sys_getsr());
    }
    return (x);
}

/* Attribute setters. */
void bonk_minvel_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        t_float f = atom_getfloat(av);
        if (f < 0) f = 0; 
        x->x_minvel = f;
    }
}

void bonk_lothresh_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        t_float f = atom_getfloat(av);
        if (f > x->x_hithresh)
            post("bonk: warning: low threshold greater than hi threshold");
        x->x_lothresh = (f <= 0 ? 0.0001 : f);
    }
}
    
void bonk_hithresh_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        t_float f = atom_getfloat(av);
        if (f < x->x_lothresh)
            post("bonk: warning: low threshold greater than hi threshold");
        x->x_hithresh = (f <= 0 ? 0.0001 : f);
    }
}

void bonk_masktime_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        int n = atom_getlong(av);
        x->x_masktime = (n < 0) ? 0 : n;
    }
}

void bonk_maskdecay_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        t_float f = atom_getfloat(av);
        f = (f < 0) ? 0 : f;
        f = (f > 1) ? 1 : f;
        x->x_maskdecay = f;
    }
}

void bonk_debouncedecay_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        t_float f = atom_getfloat(av);
        f = (f < 0) ? 0 : f;
        f = (f > 1) ? 1 : f;
        x->x_debouncedecay = f;
    }
}

void bonk_debug_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        int n = atom_getlong(av);
        x->x_debug = (n != 0);
    }
}

void bonk_spew_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        int n = atom_getlong(av);
        x->x_spew = (n != 0);
    }
}

void bonk_useloudness_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        int n = atom_getlong(av);
        x->x_useloudness = (n != 0);
    }
}

void bonk_attackbins_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        int n = atom_getlong(av);
        n = (n < 1) ? 1 : n;
        n = (n > MASKHIST) ? MASKHIST : n;
        x->x_attackbins = n;
    }
}

void bonk_learn_set(t_bonk *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        int n = atom_getlong(av);
        if (n != 0) {
            x->x_template = (t_template *)t_resizebytes(x->x_template,
                x->x_ntemplate * sizeof(x->x_template[0]), 0);
            x->x_ntemplate = 0;
        }
        x->x_learn = n;
        x->x_learncount = 0;
    }
}
/* end attr setters */

void bonk_assist(t_bonk *x, void *b, long m, long a, char *s)
{
}

    /* get current system time */
double clock_getsystime()
{
    return gettime();
}

    /* elapsed time in milliseconds since the given system time */
double clock_gettimesince(double prevsystime)
{
    return ((gettime() - prevsystime));
}

t_float qrsqrt(t_float f)
{
    return 1/sqrt(f);
}
#endif /* MSP */
