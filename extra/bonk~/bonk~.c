/*  Copyright 1997-1999 Miller Puckette (msp@ucsd.edu) and Ted Apel
(tapel@ucsd.edu). Permission is granted to use this software for any
noncommercial purpose. For commercial licensing please contact the UCSD
Technology Transfer Office.

THE AUTHORS AND THEIR EMPLOYERS MAKE NO WARRANTY, EXPRESS OR IMPLIED,
IN CONNECTION WITH THIS SOFTWARE!
*/

#include <math.h>
#include <stdio.h>

#ifdef NT
#pragma warning (disable: 4305 4244)
#endif

#ifdef MSP

#include "ext.h"
#include "z_dsp.h"
#include "math.h"
#include "ext_support.h"
#include "ext_proto.h"

typedef double t_floatarg;      /* from m_pd.h */
#define flog log
#define fexp exp
#define fsqrt sqrt
#define t_resizebytes(a, b, c) t_resizebytes((char *)(a), (b), (c))

#define flog log
#define fexp exp
#define fsqrt sqrt

#define FILE_DIALOG 1   /* use dialogs to get file name */
#define FILE_NAMED 2    /* symbol specifies file name */

#define DUMTAB1SIZE 256
#define DUMTAB2SIZE 1024

static float rsqrt_exptab[DUMTAB1SIZE], rsqrt_mantissatab[DUMTAB2SIZE];

void *bonk_class;
#define getbytes t_getbytes
#define freebytes t_freebytes
#endif /* MSP */

#ifdef PD
#include "m_pd.h"
static t_class *bonk_class;
#endif

/* ------------------------ bonk~ ----------------------------- */

#define NPOINTS 256
#define MAXCHANNELS 8
#define DEFPERIOD 128
#define DEFHITHRESH 60
#define DEFLOTHRESH 50
#define DEFMASKTIME 4
#define DEFMASKDECAY 0.7
#define DEFDEBOUNCEDECAY 0
#define DEFMINVEL 7



typedef struct _filterkernel
{
    int k_npoints;
    float k_freq;
    float k_normalize;
    float *k_stuff;
} t_filterkernel;

#if 0   /* this is the design for 1.0: */
static t_filterkernel bonk_filterkernels[] =
    {{256, 2, .01562}, {256, 4, .01562}, {256, 6, .01562}, {180, 6, .02222},
    {128, 6, .01803}, {90, 6, .02222}, {64, 6, .02362}, {46, 6, .02773},
    {32, 6, .03227}, {22, 6, .03932}, {16, 6, .04489}};
#endif

    /* here's the 1.1 rev: */
static t_filterkernel bonk_filterkernels[] =
    {{256, 1, .01562, 0}, {256, 3, .01562, 0}, {256, 5, .01562, 0},
    {212, 6, .01886, 0}, {150, 6, .01885, 0}, {106, 6, .02179, 0},
    {76, 6, .0236, 0}, {54, 6, .02634, 0}, {38, 6, .03047, 0},
    {26, 6, .03667, 0}, {18, 6, .04458, 0}};

#define NFILTERS ((int)(sizeof(bonk_filterkernels)/ \
    sizeof(bonk_filterkernels[0])))

static float bonk_hanningwindow[NPOINTS];

typedef struct _hist
{
    float h_power;
    float h_mask;
    float h_before;
    int h_countup;
} t_hist;

typedef struct template
{
    float t_amp[NFILTERS];
} t_template;

typedef struct _insig
{
    t_hist g_hist[NFILTERS];    /* history for each filter */
#ifdef PD
    t_outlet *g_outlet;         /* outlet for raw data */
#endif
#ifdef MSP
    void *g_outlet;             /* outlet for raw data */
#endif
    float *g_inbuf;             /* buffered input samples */
    t_float *g_invec;           /* new input samples */
} t_insig;

typedef struct _bonk
{
#ifdef PD
    t_object x_obj;
    t_outlet *x_cookedout;
    t_clock *x_clock;
#endif /* PD */
#ifdef MSP
    t_pxobject x_obj;
    void *x_cookedout;
    void *x_clock;
    short x_vol;
#endif /* MSP */

    t_hist x_hist[NFILTERS];
    t_template *x_template;
    t_insig *x_insig;                   
    int x_ninsig;
    int x_ntemplate;
    int x_infill;
    int x_countdown;
    int x_period;
    int x_willattack;
    int x_debug;
    float x_hithresh;
    float x_lothresh;
    int x_masktime;
    float x_maskdecay;
    int x_learn;
    double x_learndebounce;     /* debounce time for "learn" mode */
    int x_learncount;           /* countup for "learn" mode */
    float x_debouncedecay;
    float x_minvel;             /* minimum velocity we output */
    float x_debouncevel;
} t_bonk;

#ifdef MSP
static void *bonk_new(int period, int bonk2);
void bonk_tick(t_bonk *x);
void bonk_doit(t_bonk *x);
static t_int *bonk_perform(t_int *w);
void bonk_dsp(t_bonk *x, t_signal **sp);
void bonk_assist(t_bonk *x, void *b, long m, long a, char *s);
void bonk_thresh(t_bonk *x, t_floatarg f1, t_floatarg f2);
void bonk_mask(t_bonk *x, t_floatarg f1, t_floatarg f2);
void bonk_debounce(t_bonk *x, t_floatarg f1);
static void bonk_print(t_bonk *x, t_floatarg f);
static void bonk_learn(t_bonk *x, t_floatarg f);
void bonk_bang(t_bonk *x);
void bonk_setupkernels(void);
void bonk_free(t_bonk *x);
void bonk_setup(void);
void main();
float qrsqrt(float f);
static void bonk_write(t_bonk *x, t_symbol *s);
static void bonk_read(t_bonk *x, t_symbol *s);

double clock_getsystime();
double clock_gettimesince(double prevsystime);
static char *strcpy(char *s1, const char *s2);
static int ilog2(int n);
#endif

static void bonk_tick(t_bonk *x);

static void bonk_donew(t_bonk *x, int period, int nsig)
{
    int i, j;
    t_hist *h;
    float *fp;
    t_insig *g;
    
    for (j = 0, g = x->x_insig; j < nsig; j++, g++)
    {
        for (i = 0, h = g->g_hist; i--; h++)
            h->h_power = h->h_mask = h->h_before = 0, h->h_countup = 0;
                /* we ought to check for failure to allocate memory here */
        g->g_inbuf = (float *)getbytes(NPOINTS * sizeof(float));
        for (i = NPOINTS, fp = g->g_inbuf; i--; fp++) *fp = 0;
    }
    x->x_ninsig = nsig;
    x->x_template = (t_template *)getbytes(0);
    x->x_ntemplate = 0;
    x->x_infill = 0;
    x->x_countdown = 0;
    if (!period) period = NPOINTS/2;
    x->x_period = 1 << ilog2(period);
    x->x_willattack = 0;
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
    x->x_debouncevel = 0;
}


static void bonk_print(t_bonk *x, t_floatarg f);

static void bonk_dotick(t_bonk *x, int hit)
{
    t_atom at[NFILTERS], *ap, at2[3];
    int i, j, k, n;
    t_hist *h;
    float powerout[NFILTERS*MAXCHANNELS], *pp, vel = 0, temperature = 0;
    float *fp;
    t_template *tp;
    int nfit, ninsig = x->x_ninsig, ntemplate = x->x_ntemplate;
    t_insig *gp;
    int totalbins = NFILTERS * ninsig;
    
    x->x_willattack = 0;

    for (i = ninsig, pp = powerout, gp = x->x_insig; i--; gp++)
    {
        for (j = 0, h = gp->g_hist; j < NFILTERS; j++, h++, pp++)
        {
            float power = (hit ? h->h_mask - h->h_before : h->h_power);
            float intensity = *pp =
                (power > 0 ? 100. * qrsqrt(qrsqrt(power)) : 0);
            vel += intensity;
            temperature += intensity * (float)j;
        }
    }
    if (vel > 0) temperature /= vel;
    else temperature = 0;
    vel *= 0.5 / ninsig;        /* fudge factor */
    if (hit)
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
                float norm;
                for (i = NFILTERS * ninsig, norm = 0, pp = powerout; i--; pp++)
                    norm += *pp * *pp;
                if (norm < 1.0e-15) norm = 1.0e-15;
                norm = 100.f * qrsqrt(norm);
                    /* check if this is the first strike for a new template */
                if (!countup)
                {
                    int oldn = ntemplate;
                    x->x_ntemplate = ntemplate = oldn + ninsig;
                    x->x_template = (t_template *)t_resizebytes(x->x_template,
                        oldn * sizeof(x->x_template[0]),
                        ntemplate * sizeof(x->x_template[0]));
                    for (i = ninsig, pp = powerout; i--; oldn++)
                        for (j = NFILTERS, fp = x->x_template[oldn].t_amp; j--;
                            pp++, fp++)
                                *fp = *pp * norm;
                }
                else
                {
                    int oldn = ntemplate - ninsig;
                    if (oldn < 0) post("bonk_tick bug");
                    for (i = ninsig, pp = powerout; i--; oldn++)
                    {
                        for (j = NFILTERS, fp = x->x_template[oldn].t_amp; j--;
                            pp++, fp++)
                                *fp = (countup * *fp + *pp * norm)
                                    /(countup + 1.0f);
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
            float bestfit = -1e30;
            int templatecount;
            nfit = -1;
            for (i = 0, templatecount = 0, tp = x->x_template; 
                templatecount < ntemplate; i++)
            {
                float dotprod = 0;
                for (k = 0, pp = powerout;
                    k < ninsig && templatecount < ntemplate;
                        k++, tp++, templatecount++)
                {
                    for (j = NFILTERS, fp = tp->t_amp;
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

    if (x->x_debug)
        post("bonk out: number %d, vel %f, temperature %f",
            nfit, vel, temperature);
    SETFLOAT(at2, nfit);
    SETFLOAT(at2+1, vel);
    SETFLOAT(at2+2, temperature);
    outlet_list(x->x_cookedout, 0, 3, at2);

    for (n = 0, gp = x->x_insig + (ninsig-1),
        pp = powerout + NFILTERS * (ninsig-1);
            n < ninsig; n++, gp--, pp -= NFILTERS)
    {
        float *pp2;
        for (i = 0, ap = at, pp2 = pp; i < NFILTERS;
            i++, ap++, pp2++)
        {
            ap->a_type = A_FLOAT;
            ap->a_w.w_float = *pp2;
        }
        outlet_list(gp->g_outlet, 0, NFILTERS, at);
    }
}

static void bonk_tick(t_bonk *x)
{
    bonk_dotick(x, 1);
}

static void bonk_doit(t_bonk *x)
{
    int i, j, n;
    t_filterkernel *k;
    t_hist *h;
    float growth = 0, *fp1, *fp2, *fp3, *fp4;
    float windowbuf[NPOINTS];
    static int poodle;
    int ninsig = x->x_ninsig;
    t_insig *gp;
    
    for (n = 0, gp = x->x_insig; n < ninsig; n++, gp++)
    {    
        for (i = NPOINTS, fp1 = gp->g_inbuf, fp2 = bonk_hanningwindow,
            fp3 = windowbuf; i--; fp1++, fp2++, fp3++)
                *fp3 = *fp1 * *fp2;

        for (i = 0, k = bonk_filterkernels, h = gp->g_hist;
            i < NFILTERS; i++, k++, h++)
        {
            float power = 0, maskpow = h->h_mask;
            int countup = h->h_countup;
            int npoints = k->k_npoints;
                /* special case: the fourth filter is centered */
            float *inbuf = gp->g_inbuf +
                (i == 3 ? ((NPOINTS - npoints) / 2) : 0);

                /* run the filter repeatedly, sliding it forward by half its
                length, stopping when it runs past the end of the buffer */
            for (fp1 = inbuf, fp2 = fp1 + NPOINTS - k->k_npoints;
                fp1 <= fp2; fp1 += npoints/2)
            {
                float rsum = 0, isum = 0;
                for (fp3 = fp1, fp4 = k->k_stuff, j = npoints; j--;)
                {
                    float g = *fp3++;
                    rsum += g * *fp4++;
                    isum += g * *fp4++;
                }
                power += rsum * rsum + isum * isum;
            }

            if (!x->x_willattack) h->h_before = maskpow;

            if (power > maskpow)
                growth += power/(maskpow + 1.0e-15) - 1.f;
            if (!x->x_willattack && countup >= x->x_masktime)
                maskpow *= x->x_maskdecay;

            if (power > maskpow)
            {
                maskpow = power;
                countup = 0;
            }
            countup++;
            h->h_countup = countup;
            h->h_mask = maskpow;
            h->h_power = power;
        }
    }
    if (x->x_willattack > 4)
    {
            /* if it takes more than 4 analyses for the energy to stop growing,
            forget it; we would rather miss the note than report it late. */
        if (x->x_debug) post("soft attack cancelled");
        x->x_willattack = 0;
    }
    else if (x->x_willattack)
    {
        if (growth < x->x_lothresh)
            clock_delay(x->x_clock, 0);
        else x->x_willattack++;
    }
    else if (growth > x->x_hithresh)
    {
        if (x->x_debug) post("attack; growth = %f", growth);
        x->x_willattack = 1;
        for (n = 0, gp = x->x_insig; n < ninsig; n++, gp++)
            for (i = NFILTERS, h = gp->g_hist; i--; h++)
                h->h_mask = h->h_power, h->h_countup = 0;
    }
    
    x->x_debouncevel *= x->x_debouncedecay;
    
        /* shift the input buffer and update counters */
    if (x->x_period > NPOINTS) x->x_countdown = x->x_period - NPOINTS;
    else x->x_countdown = 0;
    if (x->x_period < NPOINTS)
    {
        int overlap = NPOINTS - x->x_period;
        
        for (n = 0, gp = x->x_insig; n < ninsig; n++, gp++)
            for (i = overlap, fp1 = gp->g_inbuf, fp2 = fp1 + x->x_period; i--;)
                *fp1++ = *fp2++;
        x->x_infill = overlap;
    }
    else x->x_infill = 0;
    poodle = 1;
}

static t_int *bonk_perform(t_int *w)
{
    t_bonk *x = (t_bonk *)(w[1]);
    int n = (int)(w[2]);
    int onset = (int)(w[3]);
    if (x->x_countdown > 0) x->x_countdown -= n;
    else
    {
        int i, j, infill = x->x_infill, ninsig = x->x_ninsig;
        t_insig *gp;
        for (i = 0, gp = x->x_insig; i < ninsig; i++, gp++)
        {
            float *fp = gp->g_inbuf + infill;
            t_float *in1 = gp->g_invec + onset;
            for (j = 0; j < n; j++)
                *fp++ = *in1++;
        }
        infill += n;
        x->x_infill = infill;   
        if (infill == NPOINTS) bonk_doit(x);
    }
    return (w+4);
}

static void bonk_dsp(t_bonk *x, t_signal **sp)
{
    int i, n = sp[0]->s_n, vsize = x->x_period, ninsig = x->x_ninsig;
    t_insig *gp;
    if (vsize > n) vsize = n;
    
    for (i = 0, gp = x->x_insig; i < ninsig; i++, gp++)
        gp->g_invec = (*(sp++))->s_vec;

    for (i = 0; i < n; i += vsize)
        dsp_add(bonk_perform, 3, x, vsize, i);
}

static void bonk_thresh(t_bonk *x, t_floatarg f1, t_floatarg f2)
{
    if (f1 > f2)
        post("bonk: warning: low threshold greater than hi threshold");
    x->x_lothresh = f1;
    x->x_hithresh = f2;
}

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

static void bonk_print(t_bonk *x, t_floatarg f)
{
    int i;
    post("thresh %f %f", x->x_lothresh, x->x_hithresh);
    post("mask %d %f", x->x_masktime, x->x_maskdecay);
    post("debounce %f", x->x_debouncedecay);
    post("minvel %f", x->x_minvel);
    if (x->x_ntemplate)
    {
        post("templates:");
        for (i = 0; i < x->x_ntemplate; i++)
            post("%2d \
%5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f", i,
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
    if (x->x_learn) post("learn mode");
    if (f != 0)
    {
        int j, ninsig = x->x_ninsig;
        t_insig *gp;
        for (j = 0, gp = x->x_insig; j < ninsig; j++, gp++)
        {
            t_hist *h;
            if (ninsig > 1) post("input %d:", j+1);
            for (i = NFILTERS, h = gp->g_hist; i--; h++)
                post("pow %f mask %f before %f count %d",
                    h->h_power, h->h_mask, h->h_before, h->h_countup);
        }
    }
    if (x->x_debug) post("debug mode");
}

static void bonk_debug(t_bonk *x, t_floatarg f)
{
    x->x_debug = (f != 0);
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

#if 0
static void bonk_bang(t_bonk *x)
{
    t_atom at[NFILTERS];
    int i, j, ninsig = x->x_ninsig;
    t_insig *gp;

    SETFLOAT(at2, nfit);
    SETFLOAT(at2+1, vel);
    SETFLOAT(at2+2, temperature);
    outlet_list(x->x_cookedout, 0L, 3, at2);
    for (i = 0, gp = x->x_insig + (ninsig-1); i < ninsig; i++, gp--)
    {
        for (j = 0; j < NFILTERS; j++)
        {
            at[j].a_type = A_FLOAT;
            at[j].a_w.w_float = 100 * qrsqrt(qrsqrt(gp->g_hist[j].h_power));
        }
        outlet_list(gp->g_outlet, 0L, NFILTERS, at);
    }
}
#endif

static void bonk_bang(t_bonk *x)
{
    bonk_dotick(x, 0);
}

static void bonk_setupkernels(void)
{
    int i, j;
    float *fp;
    for (i = 0; i < NFILTERS; i++)
    {
        int npoints = bonk_filterkernels[i].k_npoints;
        float freq = bonk_filterkernels[i].k_freq;
        float normalize = bonk_filterkernels[i].k_normalize;
        float phaseinc = (2.f * 3.14159f) / npoints;
        bonk_filterkernels[i].k_stuff =
            (float *)getbytes(2 * sizeof(float) * npoints);
        for (fp = bonk_filterkernels[i].k_stuff, j = npoints; j--;)
        {
            float phase = j * phaseinc;
            float window = normalize * (0.5f - 0.5f * cos(phase));
            *fp++ = window * cos(freq * phase);
            *fp++ = window * sin(freq * phase);
        }
    }
    for (i = 0; i < NPOINTS; i++)
        bonk_hanningwindow[i] = (0.5f - 0.5f * cos(i * (2*3.14159)/NPOINTS));
}

#ifdef PD
static void bonk_read(t_bonk *x, t_symbol *s)
{
    FILE *fd = fopen(s->s_name, "r");
    float vec[NFILTERS];
    int i, ntemplate = 0, remaining;
    float *fp, *fp2;
    if (!fd)
    {
        post("%s: open failed", s->s_name);
        return;
    }
    x->x_template = (t_template *)t_resizebytes(x->x_template, 
        x->x_ntemplate * sizeof(t_template), 0);
    while (1)
    {
        for (i = NFILTERS, fp = vec; i--; fp++)
            if (fscanf(fd, "%f", fp) < 1) goto nomore;
        x->x_template = (t_template *)t_resizebytes(x->x_template,
            ntemplate * sizeof(t_template),
            (ntemplate + 1) * sizeof(t_template));
        for (i = NFILTERS, fp = vec,
            fp2 = x->x_template[ntemplate].t_amp; i--;)
                *fp2++ = *fp++;
        ntemplate++;
    }
nomore:
    if (remaining = (ntemplate % x->x_ninsig))
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
#endif /* PD */

#ifdef MSP
static void bonk_read(t_bonk *x, t_symbol *s)
{
    SFTypeList types;
    short vol = 0;
    OSType type;
    char name[256];
    char **buf;
    int eaten;
    long size = 100;
    int i, ntemplate = 0;
    float vec[NFILTERS];
    float *fp, *fp2;
    if (s->s_name[0])
    {
        vol = defvolume();
        strcpy (name, s->s_name);

        if (readtohandle (name, vol, &buf, &size) != 0)

        {
            post("bonk~: problem with reading file.");
            return;

        }
        else
        {
            post("bonk~: template read successfully.");
        }
        for (eaten = 0; ;)
        {
            for (i = NFILTERS, fp = vec; i--; fp++)
            {
                while (eaten < size && (
                    (*buf)[eaten] == ' ' ||
                    (*buf)[eaten] == '\t' ||
                    (*buf)[eaten] == '\n' ||
                    (*buf)[eaten] == ';'  ||
                    (*buf)[eaten] == '\r'))
                            eaten++;
                if (eaten >= size) goto nomore;
                if (sscanf(&(*buf)[eaten], "%f", fp) < 1) goto nomore;

                while (eaten < size && !(
                    (*buf)[eaten] == ' ' ||
                    (*buf)[eaten] == '\t' ||
                    (*buf)[eaten] == '\n' ||
                    (*buf)[eaten] == ';'  ||
                    (*buf)[eaten] == '\r'))
                            eaten++;
            }
            x->x_template = (t_template *)t_resizebytes(x->x_template,

            ntemplate * sizeof(t_template),
            (ntemplate + 1) * sizeof(t_template));

            for (i = NFILTERS, fp = vec, 
                fp2 = x->x_template[ntemplate].t_amp; i--;)
                    *fp2++ = *fp++;
            ntemplate++;
            post("bonk~: fp = %f", fp);
        }
    }
    else
    {
        name[0] = 0;
        types[0]='TEXT';
        types[1]='maxb';

        open_promptset("Select template for reading.");

        if (open_dialog(name, &vol, &type, types, 2))
        {
            post("bonk~: open canceled");
            return;
        }
        x->x_template = (t_template *)t_resizebytes(x->x_template,

        x->x_ntemplate * sizeof(t_template), 0);


        if (readtohandle (name, vol, &buf, &size) != 0)

        {
            post("bonk~: problem with reading file.");
            return;

        }
        else
        {
            post("bonk~: template read successfully.");
        }
        for (eaten = 0; ;)
        {
            for (i = NFILTERS, fp = vec; i--; fp++)
            {
                while (eaten < size && (
                    (*buf)[eaten] == ' ' ||
                    (*buf)[eaten] == '\t' ||
                    (*buf)[eaten] == '\n' ||
                    (*buf)[eaten] == ';'  ||
                    (*buf)[eaten] == '\r'))
                            eaten++;
                if (eaten >= size) goto nomore;
                if (sscanf(&(*buf)[eaten], "%f", fp) < 1) goto nomore;

                while (eaten < size && !(
                    (*buf)[eaten] == ' ' ||
                    (*buf)[eaten] == '\t' ||
                    (*buf)[eaten] == '\n' ||
                    (*buf)[eaten] == ';'  ||
                    (*buf)[eaten] == '\r'))
                            eaten++;
            }
            x->x_template = (t_template *)t_resizebytes(x->x_template,

            ntemplate * sizeof(t_template),
            (ntemplate + 1) * sizeof(t_template));

            for (i = NFILTERS, fp = vec,
                fp2 = x->x_template[ntemplate].t_amp; i--;)
                    *fp2++ = *fp++;
            ntemplate++;
        }
        nomore:
        post("bonk~: read %d templates", ntemplate);

        x->x_ntemplate = ntemplate;
    }
}
#endif /* MSP */

#ifdef PD
static void bonk_write(t_bonk *x, t_symbol *s)
{
    FILE *fd = fopen(s->s_name, "w");
    int i, ntemplate = x->x_ntemplate;
    t_template *tp = x->x_template;
    float *fp;
    if (!fd)
    {
        post("%s: couldn't create", s->s_name);
        return;
    }
    for (; ntemplate--; tp++)
    {
        for (i = NFILTERS, fp = tp->t_amp; i--; fp++)
                fprintf(fd, "%6.2f ", *fp);
        fprintf(fd, "\n");
    }
    post("bonk: wrote %d templates\n", x->x_ntemplate);
    fclose(fd);
}
#endif /* PD */

#ifdef MSP
static void bonk_write(t_bonk *x, t_symbol *s)
{

    char fn[236];
    short vol;
    short bin = 0;
    void* b;
    int i, ntemplate = x->x_ntemplate;
    t_template *tp = x->x_template;
    if (s->s_name[0]) 
    {
        strcpy (fn, s->s_name);
        vol = defvolume();
        b = binbuf_new();
        for (; ntemplate--; tp++)
        {
            int i;
            Atom at[11];
            for (i = 0; i < 11; i++)
                at[i].a_type = A_FLOAT, at[i].a_w.w_float = tp->t_amp[i];
            binbuf_insert(b, 0L, 11, at);
        }
        binbuf_write(b, fn, vol, bin);
        freeobject(b);
        post("bonk~: wrote file %s", fn);
    }

    else
    {
        saveas_promptset("Save Template file as");
        strcpy(fn, "");
        if (!saveas_dialog(fn, &vol, 0L))
        {
            b = binbuf_new();
            for (; ntemplate--; tp++)
            {
                int i;
                Atom at[11];
                for (i = 0; i < 11; i++)
                    at[i].a_type = A_FLOAT, at[i].a_w.w_float =
                        tp->t_amp[i];
                binbuf_insert(b, 0L, 11, at);
            }
            binbuf_write(b, fn, vol, bin);
            freeobject(b);
            post("bonk~: wrote file %s", fn);
        }
    } 
}
#endif /* MSP */

static void bonk_free(t_bonk *x)
{
    int i, ninsig = x->x_ninsig;
    t_insig *gp = x->x_insig;
    for (i = 0, gp = x->x_insig; i < ninsig; i++, gp++)
        freebytes(gp->g_inbuf, NPOINTS * sizeof(float));
    clock_free(x->x_clock);
}

/* -------------------------- Pd glue ------------------------- */
#ifdef PD

static void *bonk_new(t_floatarg fperiod, t_floatarg fnsig)
{
    t_bonk *x = (t_bonk *)pd_new(bonk_class);
    int nsig = fnsig, j;
    t_insig *g;
    if (nsig < 1) nsig = 1;
    if (nsig > MAXCHANNELS) nsig = MAXCHANNELS;
    
    x->x_clock = clock_new(x, (t_method)bonk_tick);
    x->x_insig = (t_insig *)getbytes(nsig * sizeof(*x->x_insig));
    for (j = 0, g = x->x_insig; j < nsig; j++, g++)
    {
        g->g_outlet = outlet_new(&x->x_obj, gensym("list"));
        if (j)
            inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    }
    x->x_cookedout = outlet_new(&x->x_obj, gensym("list"));
    bonk_donew(x, fperiod, nsig);
    return (x);
}

void bonk_tilde_setup(void)
{
    bonk_class = class_new(gensym("bonk~"), (t_newmethod)bonk_new,
        (t_method)bonk_free, sizeof(t_bonk), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(bonk_class, nullfn, gensym("signal"), 0);
    class_addmethod(bonk_class, (t_method)bonk_dsp, gensym("dsp"), 0);
    class_addbang(bonk_class, bonk_bang);
    class_addmethod(bonk_class, (t_method)bonk_learn, gensym("learn"),
        A_FLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_forget, gensym("forget"), 0);
    class_addmethod(bonk_class, (t_method)bonk_thresh, gensym("thresh"),
        A_FLOAT, A_FLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_mask, gensym("mask"),
        A_FLOAT, A_FLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_debounce, gensym("debounce"),
        A_FLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_minvel, gensym("minvel"),
        A_FLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_print, gensym("print"),
        A_DEFFLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_debug, gensym("debug"),
        A_DEFFLOAT, 0);
    class_addmethod(bonk_class, (t_method)bonk_read, gensym("read"),
        A_SYMBOL, 0);
    class_addmethod(bonk_class, (t_method)bonk_write, gensym("write"),
        A_SYMBOL, 0);
    bonk_setupkernels();
    post("bonk version 1.1 TEST 3");
}
#endif

/* -------------------------- MSP glue ------------------------- */
#ifdef MSP

static int ilog2(int n)
{
    int ret = -1;
    while (n)
    {
        n >>= 1;
        ret++;
    }
    return (ret);
}

static char *strcpy(char *s1, const char *s2)
{
    char *ret = s1;

    while ((*s1++ = *s2++) != 0)
        ;

    return ret;
}

static void *bonk_new(int period, int nsig)
{
    int i, j;
    t_hist *h;
    t_bonk *x = (t_bonk *)newobject(bonk_class);
    float *fp;
    t_insig *g;

    if (nsig < 1) nsig = 1;
    if (nsig > MAXCHANNELS) nsig = MAXCHANNELS;
    x->x_insig = (t_insig *)getbytes(nsig * sizeof(*x->x_insig));
    dsp_setup((t_pxobject *)x, nsig);
    x->x_cookedout = listout((t_object *)x);
    for (j = 0, g = x->x_insig + nsig-1; j < nsig; j++, g--)
    {
        g->g_outlet = listout((t_object *)x);
    }
    x->x_cookedout = listout((t_object *)x);
    x->x_clock = clock_new(x, (method)bonk_tick);

    bonk_donew(x, period, nsig);
    return (x);
}

void main()
{
        setup(&bonk_class, bonk_new, (method)bonk_free,
            (short)sizeof(t_bonk), 0L, A_DEFLONG, A_DEFLONG, 0);
        addmess((method)bonk_dsp,               "dsp",          0);
        addbang((method)bonk_bang);
        addmess((method)bonk_forget,    "forget",       0);
        addmess((method)bonk_learn,     "learn",        A_FLOAT, 0);
        addmess((method)bonk_thresh,    "thresh",       A_FLOAT, A_FLOAT, 0);
        addmess((method)bonk_mask,      "mask",         A_FLOAT, A_FLOAT, 0);
        addmess((method)bonk_minvel,    "minvel",       A_FLOAT, 0);
        addmess((method)bonk_debounce,  "debounce", A_FLOAT, 0);
        addmess((method)bonk_print,     "print",        A_DEFFLOAT, 0);
        addmess((method)bonk_read,              "read",         A_DEFSYM, 0);
        addmess((method)bonk_write,     "write",        A_DEFSYM, 0);
        addmess((method)bonk_assist,    "assist",       A_CANT, 0);
        addmess((method)bonk_debug,     "debug",        A_FLOAT, 0);
        bonk_setupkernels();
        post("bonk~ v1.00");
        dsp_initclass();
        rescopy('STR#',3747);
}

void bonk_assist(t_bonk *x, void *b, long m, long a, char *s)
{
        assist_string(3747,m,a,1,2,s);
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


float qrsqrt(float f)
{
        return 1/sqrt(f);

}
#endif /* MSP */
