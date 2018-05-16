/* paf -- version of the paf generator derived from "jupiterpaf", from
Manoury's Jupiter, 1997 version.  Can be compiled as a standalone test program,
or as an object in Max 0.26, JMAX, or Pd. */

/*  Copyright 1997-1999 Miller Puckette.
Permission is granted to use this software for any purpose provided you
keep this copyright notice intact.

THE AUTHOR AND HIS EMPLOYERS MAKE NO WARRANTY, EXPRESS OR IMPLIED,
IN CONNECTION WITH THIS SOFTWARE.

This file is downloadable from http://www.crca.ucsd.edu/~msp .

Warning: the algorithms implemented here are covered by patents held by
IRCAM.  While this probably does not restrict anyone from distributing
software implementing the paf, any hardware implementor should obtain a
license from IRCAM.
*/

static char *paf_version = "paf version 0.06";

#ifdef NT
#pragma warning (disable: 4305 4244)
#endif

#ifdef TESTME
#include <stdio.h>
#endif
#ifdef FTS15
#include "mess.h"
#include "dsp.h"
#endif
#ifdef V26SGI
#include "m_extern.h"
#include "d_graph.h"
#include "d_ugen.h"
#endif
#ifdef PD
#include "m_pd.h"
#endif
#include <math.h>

#define LOGTABSIZE 9
#define TABSIZE (1 << LOGTABSIZE)
#define TABRANGE 3

typedef struct _tabpoint
{
    float p_y;
    float p_diff;
} t_tabpoint;

static t_tabpoint paf_gauss[TABSIZE];
static t_tabpoint paf_cauchy[TABSIZE];

typedef struct _linenv
{
    double l_current;
    double l_biginc;
    float l_1overn;
    float l_target;
    float l_msectodsptick;
    int l_ticks;
} t_linenv;

typedef struct _pafctl
{
    t_linenv x_freqenv;
    t_linenv x_cfenv;
    t_linenv x_bwenv;
    t_linenv x_ampenv;
    t_linenv x_vibenv;
    t_linenv x_vfrenv;
    t_linenv x_shiftenv;
    float x_isr;
    float x_held_freq;
    float x_held_intcar;
    float x_held_fraccar;
    float x_held_bwquotient;
    double x_phase;
    double x_shiftphase;
    double x_vibphase;
    int x_triggerme;
    int x_cauchy;
} t_pafctl;

static void linenv_debug(t_linenv *l, char *s)
{
#ifdef DEBUG
    post("%s: current %f, target %f", s, l->l_current, l->l_target);
#endif
}

static void linenv_init(t_linenv *l)
{
    l->l_current = l->l_biginc = 0;
    l->l_1overn = l->l_target = l->l_msectodsptick = 0;
    l->l_ticks = 0;
}

static void linenv_setsr(t_linenv *l, float sr, int vecsize)
{
    l->l_msectodsptick = sr / (1000.f * ((float)vecsize));
    l->l_1overn = 1.f/(float)vecsize;
}

static void linenv_set(t_linenv *l, float target, long timdel)
{
    if (timdel > 0)
    {
	l->l_ticks = ((float)timdel) * l->l_msectodsptick;
	if (!l->l_ticks) l->l_ticks = 1;
	l->l_target = target;
	l->l_biginc = (l->l_target - l->l_current)/l->l_ticks;
    }
    else
    {
	l->l_ticks = 0;
	l->l_current = l->l_target = target;
	l->l_biginc = 0;
    }
}

#define LINENV_RUN(linenv, current, incr) 		\
    if (linenv.l_ticks > 0)				\
    {							\
    	current = linenv.l_current;			\
    	incr = linenv.l_biginc * linenv.l_1overn;	\
	linenv.l_ticks--;				\
	linenv.l_current += linenv.l_biginc;		\
    }							\
    else						\
    {							\
    	linenv.l_current = current = linenv.l_target;	\
	incr = 0;					\
    }

#define UNITBIT32 1572864.		/* 3*2^19 -- bit 32 has value 1 */
#define TABFRACSHIFT (UNITBIT32/TABSIZE) 

#ifdef __sgi
    /* machine-dependent definitions: */
#define HIOFFSET 0    /* word offset to find MSB; endianness-dependent */
#define LOWOFFSET 0    /* word offset to find MSB; endianness-dependent */
#define int32 unsigned long  /* a data type that has 32 bits */
#define DONE_MACHINE_TYPE
#endif /* __sgi */

#ifdef NT
    /* little-endian; most significant byte is at highest address */
#define HIOFFSET 1
#define LOWOFFSET 0
#define int32 unsigned long
#define DONE_MACHINE_TYPE
#endif /* NT */

#ifdef MACOSX
#define HIOFFSET 0    /* word offset to find MSB */
#define LOWOFFSET 1    /* word offset to find LSB */
#define int32 int  /* a data type that has 32 bits */
#define DONE_MACHINE_TYPE
#endif /* MACOSX */

#ifdef __FreeBSD__
#include <machine/endian.h>
#if BYTE_ORDER == LITTLE_ENDIAN
#define HIOFFSET 1
#define LOWOFFSET 0
#else
#define HIOFFSET 0    /* word offset to find MSB */
#define LOWOFFSET 1    /* word offset to find LSB */
#endif /* BYTE_ORDER */
#include <sys/types.h>
#define int32 int32_t
#define DONE_MACHINE_TYPE
#endif /* __FreeBSD__ */

#ifdef  __linux__

#include <endian.h>

#if !defined(__BYTE_ORDER) || !defined(__LITTLE_ENDIAN)                         
#error No byte order defined                                                    
#endif                                                                          

#if __BYTE_ORDER == __LITTLE_ENDIAN                                             
#define HIOFFSET 1                                                              
#define LOWOFFSET 0                                                             
#else                                                                           
#define HIOFFSET 0    /* word offset to find MSB */                             
#define LOWOFFSET 1    /* word offset to find LSB */                            
#endif /* __BYTE_ORDER */                                                       

#include <sys/types.h>
#define int32 u_int32_t

#define DONE_MACHINE_TYPE
#endif /* __linux__ */

#ifndef DONE_MACHINE_TYPE
#error -- no machine architecture definition?
#endif

#define A1 ((float)(4 * (3.14159265/2)))
#define A3 ((float)(64 * (2.5 - 3.14159265)))
#define A5 ((float)(1024 * ((3.14159265/2) - 1.5)))

static void pafctl_run(t_pafctl *x, float *out1, int n)
{
    float freqval, freqinc;
    float cfval, cfinc;
    float bwval, bwinc;
    float ampval, ampinc;
    float vibval, vibinc;
    float vfrval, vfrinc;
    float shiftval, shiftinc;
    float bwquotient, bwqincr;
    double  ub32 = UNITBIT32;
    double phase = x->x_phase + ub32;
    double phasehack;
    volatile double *phasehackp = &phasehack;
    double shiftphase = x->x_shiftphase + ub32;
    volatile int32 *hackptr = ((int32 *)(phasehackp)) + HIOFFSET, hackval;
    volatile int32 *lowptr = ((int32 *)(phasehackp)) + LOWOFFSET, lowbits;
    float held_freq = x->x_held_freq;
    float held_intcar = x->x_held_intcar;
    float held_fraccar = x->x_held_fraccar;
    float held_bwquotient = x->x_held_bwquotient;
    float sinvib, vibphase;
    t_tabpoint *paf_table = (x->x_cauchy ? paf_cauchy : paf_gauss);
    *phasehackp = ub32;
    hackval = *hackptr;

    	/* fractional part of shift phase */
    *phasehackp = shiftphase;
    *hackptr = hackval;
    shiftphase = *phasehackp;

	/* propagate line envelopes */
    LINENV_RUN(x->x_freqenv, freqval, freqinc);
    LINENV_RUN(x->x_cfenv, cfval, cfinc);
    LINENV_RUN(x->x_bwenv, bwval, bwinc);
    LINENV_RUN(x->x_ampenv, ampval, ampinc);
    LINENV_RUN(x->x_vibenv, vibval, vibinc);
    LINENV_RUN(x->x_vfrenv, vfrval, vfrinc);
    LINENV_RUN(x->x_shiftenv, shiftval, shiftinc);

	/* fake line envelope for quotient of bw and frequency */
    bwquotient = bwval/freqval;
    bwqincr = (((float)(x->x_bwenv.l_current))/
    	((float)(x->x_freqenv.l_current)) - bwquotient) *
		x->x_freqenv.l_1overn;

	/* run the vibrato oscillator */
    
    *phasehackp = ub32 + (x->x_vibphase + n * x->x_isr * vfrval);
    *hackptr = hackval;
    vibphase = (x->x_vibphase = *phasehackp - ub32);
    if (vibphase > 0.5)
    	sinvib = 1.0f - 16.0f * (0.75f-vibphase) * (0.75f - vibphase);
    else sinvib = -1.0f + 16.0f * (0.25f-vibphase) * (0.25f - vibphase);
    freqval = freqval * (1.0f + vibval * sinvib);

    shiftval *= x->x_isr;
    shiftinc *= x->x_isr;
    
	/* if phase or amplitude is zero, load in new params */
    if (ampval == 0 || phase == ub32 || x->x_triggerme)
    {
    	float cf_over_freq = cfval/freqval;
    	held_freq = freqval * x->x_isr;
	held_intcar = (float)((int)cf_over_freq);
	held_fraccar = cf_over_freq - held_intcar;
	held_bwquotient = bwquotient;
	x->x_triggerme = 0;
    }
    while (n--)
    {
    	double newphase = phase + held_freq;
	double carphase1, carphase2, fracnewphase;
	float fphase, fcarphase1, fcarphase2, carrier;
	float g, g2, g3, cosine1, cosine2, halfsine, mod, tabfrac;
	t_tabpoint *p;
   	    /* put new phase into 64-bit memory location.  Bash upper
	    32 bits to get fractional part (plus "ub32").  */

	*phasehackp = newphase;
	*hackptr = hackval;
	newphase = *phasehackp;
	fracnewphase = newphase-ub32;
	fphase = 2.0f * ((float)(fracnewphase)) - 1.0f;
	if (newphase < phase)
	{
	    float cf_over_freq = cfval/freqval;
	    held_freq = freqval * x->x_isr;
	    held_intcar = (float)((int)cf_over_freq);
	    held_fraccar = cf_over_freq - held_intcar;
	    held_bwquotient = bwquotient;
	}
	phase = newphase;
	*phasehackp = fracnewphase * held_intcar + shiftphase;
	*hackptr = hackval;
	carphase1 = *phasehackp;
	fcarphase1 = carphase1 - ub32;
	*phasehackp = carphase1 + fracnewphase;
	*hackptr = hackval;
	carphase2 = *phasehackp;
	fcarphase2 = carphase2 - ub32;
    	
	shiftphase += shiftval;
	
    	if (fcarphase1 > 0.5f)  g = fcarphase1 - 0.75f;
    	else g = 0.25f - fcarphase1;
    	g2 = g * g;
    	g3 = g * g2;
    	cosine1 = g * A1 + g3 * A3 + g2 * g3 * A5;

    	if (fcarphase2 > 0.5f)  g = fcarphase2 - 0.75f;
    	else g = 0.25f - fcarphase2;
    	g2 = g * g;
    	g3 = g * g2;
    	cosine2 = g * A1 + g3 * A3 + g2 * g3 * A5;
	
	carrier = cosine1 + held_fraccar * (cosine2-cosine1);

    	ampval += ampinc;
	bwquotient += bwqincr;

	/* printf("bwquotient %f\n", bwquotient); */

	halfsine = held_bwquotient * (1.0f - fphase * fphase);
	if (halfsine >= (float)(0.997 * TABRANGE))
	    halfsine = (float)(0.997 * TABRANGE);

#if 0
    	shape = halfsine * halfsine;
	mod = ampval * carrier *
	    (1 - bluntval * shape) / (1 + (1 - bluntval) * shape);
#endif
#if 0
    	shape = halfsine * halfsine;
	mod = ampval * carrier *
	    exp(-shape);
#endif
	halfsine *= (float)(1./TABRANGE);

   	    /* Get table index for "halfsine".  Bash upper
	    32 bits to get fractional part (plus "ub32").  Also grab
	    fractional part as a fixed-point number to use as table
	    address later. */

	*phasehackp = halfsine + ub32;
	lowbits = *lowptr;

    	    /* now shift again so that the fractional table address
	    appears in the low 32 bits, bash again, and extract this as
	    a floating point number from 0 to 1. */
	*phasehackp = halfsine + TABFRACSHIFT;
	*hackptr = hackval;
    	tabfrac = *phasehackp - ub32;

    	p = paf_table + (lowbits >> (32 - LOGTABSIZE));
	mod = ampval * carrier * (p->p_y + tabfrac * p->p_diff);
	
	*out1++ = mod;
    }
    x->x_phase = phase - ub32;
    x->x_shiftphase = shiftphase - ub32;
    x->x_held_freq = held_freq;
    x->x_held_intcar = held_intcar;
    x->x_held_fraccar = held_fraccar;
    x->x_held_bwquotient = held_bwquotient;
}

static void pafctl_init(t_pafctl *x)
{
    linenv_init(&x->x_freqenv);
    linenv_init(&x->x_cfenv);
    linenv_init(&x->x_bwenv);
    linenv_init(&x->x_ampenv);
    linenv_init(&x->x_vibenv);
    linenv_init(&x->x_vfrenv);
    linenv_init(&x->x_shiftenv);
    x->x_freqenv.l_target = x->x_freqenv.l_current = 1.0;
    x->x_isr = (float)(1./44100.);
    x->x_held_freq = 1.f;
    x->x_held_intcar = 0.f;
    x->x_held_fraccar = 0.f;
    x->x_held_bwquotient = 0.f;
    x->x_phase = 0.;
    x->x_shiftphase = 0.;
    x->x_vibphase = 0.;
    x->x_triggerme = 0;
    x->x_cauchy = 0;
}

static void pafctl_setsr(t_pafctl *x, float sr, int vecsize)
{
    x->x_isr = 1.f/sr;
    linenv_setsr(&x->x_freqenv, sr, vecsize);
    linenv_setsr(&x->x_cfenv, sr, vecsize);
    linenv_setsr(&x->x_bwenv, sr, vecsize);
    linenv_setsr(&x->x_ampenv, sr, vecsize);
    linenv_setsr(&x->x_vibenv, sr, vecsize);
    linenv_setsr(&x->x_vfrenv, sr, vecsize);
    linenv_setsr(&x->x_shiftenv, sr, vecsize);
}

static void pafctl_freq(t_pafctl *x, float val, int time)
{
    if (val < 1.f) val = 1.f;
    if (val > 10000000.f) val = 1000000.f;
    linenv_set(&x->x_freqenv, val, time);
}

static void pafctl_cf(t_pafctl *x, float val, int time)
{
    linenv_set(&x->x_cfenv, val, time);
}

static void pafctl_bw(t_pafctl *x, float val, int time)
{
    linenv_set(&x->x_bwenv, val, time);
}

static void pafctl_amp(t_pafctl *x, float val, int time)
{
    linenv_set(&x->x_ampenv, val, time);
}

static void pafctl_vib(t_pafctl *x, float val, int time)
{
    linenv_set(&x->x_vibenv, val, time);
}

static void pafctl_vfr(t_pafctl *x, float val, int time)
{
    linenv_set(&x->x_vfrenv, val, time);
}

static void pafctl_shift(t_pafctl *x, float val, int time)
{
    linenv_set(&x->x_shiftenv, val, time);
}

static void pafctl_phase(t_pafctl *x, float mainphase, float shiftphase,
    float vibphase)
{
    x->x_phase = mainphase;
    x->x_shiftphase = shiftphase;
    x->x_vibphase = vibphase;
    x->x_triggerme = 1;
}

    /* value of Cauchy distribution at TABRANGE */
#define CAUCHYVAL (1./ (1. + TABRANGE * TABRANGE))
    /* first derivative of Cauchy distribution at TABRANGE */
#define CAUCHYSLOPE ((-2. * TABRANGE) * CAUCHYVAL * CAUCHYVAL)
#define ADDSQ (- CAUCHYSLOPE / (2 * TABRANGE))

static void paf_dosetup(void)
{
    int i;
    float CAUCHYFAKEAT3  =
    	(CAUCHYVAL + ADDSQ * TABRANGE * TABRANGE);
    float CAUCHYRESIZE = (1./ (1. - CAUCHYFAKEAT3));
    for (i = 0; i <= TABSIZE; i++)
    {
    	float f = i * ((float)TABRANGE/(float)TABSIZE);
	float gauss = exp(-f * f);
	float cauchygenuine = 1. / (1. + f * f);
	float cauchyfake = cauchygenuine + ADDSQ * f * f;
	float cauchyrenorm = (cauchyfake - 1.) * CAUCHYRESIZE + 1.;
	if (i != TABSIZE)
	{
	    paf_gauss[i].p_y = gauss;
	    paf_cauchy[i].p_y = cauchyrenorm;
	    /* post("%f", cauchyrenorm); */
	}
	if (i != 0)
	{
	    paf_gauss[i-1].p_diff = gauss - paf_gauss[i-1].p_y;
	    paf_cauchy[i-1].p_diff = cauchyrenorm - paf_cauchy[i-1].p_y;
	}
    }
}

#ifdef TESTME

#define BS 64
main()
{
    t_pafctl x;
    float x1[BS];
    int i;
    paf_dosetup();
    pafctl_init(&x);
    pafctl_setsr(&x, 16000., BS);
    pafctl_freq(&x, 1000, 0);
    pafctl_bw(&x, 2000, 0);
    pafctl_amp(&x, 1000, 0);
    pafctl_run(&x, x1, BS);
    for (i = 0; i < BS/4; i++)
    {
	printf("%15.5f %15.5f %15.5f %15.5f\n",
		x1[4*i], x1[4*i+1], x1[4*i+2], x1[4*i+3]);
    }
#if 0
    printf("\n");
    pafctl_bw(&x, 2000, 0);
    pafctl_run(&x, x1, BS);
    for (i = 0; i < BS/4; i++)
    {
	printf("%15.5f %15.5f %15.5f %15.5f\n",
		x1[4*i], x1[4*i+1], x1[4*i+2], x1[4*i+3]);
    }
#endif
}

#endif
#ifdef FTS1X

static fts_symbol_t *paf_dspname;

typedef struct _paf
{
  fts_object_t ob;		/* object header */
  t_pafctl pafctl;
} paf_t;

/* ---------------------------------------- */
/* Methods                                  */
/* ---------------------------------------- */

    /* formalities... */

static void paf_dspfun(fts_word_t *a)
{
    paf_t *x = (paf_t *)fts_word_get_obj(a);
    float *out1 = (float *)fts_word_get_obj(a + 1);
    long n = fts_word_get_long(a + 3);
    
    pafctl_run(&x->pafctl, out1, n);
}

void paf_put(fts_object_t *o, int winlet, fts_symbol_t *s,
    int ac, const fts_atom_t *at)
{
    paf_t *x = (paf_t *)o;
    fts_dsp_descr_t *dsp = (fts_dsp_descr_t *)fts_get_obj_arg(at, ac, 0, 0);
    fts_atom_t a[4];
    float sr = fts_dsp_get_output_srate(dsp, 0);
    int vecsize = fts_dsp_get_output_size(dsp, 0);

    pafctl_setsr(&x->pafctl, sr, vecsize);
    fts_set_obj(a, x);
    fts_set_symbol(a+1, fts_dsp_get_output_name(dsp, 0));
    fts_set_long(a+2, fts_dsp_get_output_size(dsp, 0));
    dsp_add_funcall(paf_dspname, 3, a);
}

static void paf_freq(fts_object_t *o, int winlet, fts_symbol_t *s,
    int ac, const fts_atom_t *at)
{
    paf_t *this = (paf_t *)o;
    float val = fts_get_float_long_arg( at, ac, 0, 0.0f);
    int time = fts_get_long_arg(at, ac, 1, 0);
    pafctl_freq(&this->pafctl, val, time);
}

static void paf_cf(fts_object_t *o, int winlet, fts_symbol_t *s,
    int ac, const fts_atom_t *at)
{
    paf_t *this = (paf_t *)o;
    float val = fts_get_float_long_arg( at, ac, 0, 0.0f);
    int time = fts_get_long_arg(at, ac, 1, 0);
    pafctl_cf(&this->pafctl, val, time);
}

static void paf_bw(fts_object_t *o, int winlet, fts_symbol_t *s,
    int ac, const fts_atom_t *at)
{
    paf_t *this = (paf_t *)o;
    float val = fts_get_float_long_arg( at, ac, 0, 0.0f);
    int time = fts_get_long_arg(at, ac, 1, 0);
    pafctl_bw(&this->pafctl, val, time);
}

static void paf_amp(fts_object_t *o, int winlet, fts_symbol_t *s,
    int ac, const fts_atom_t *at)
{
    paf_t *this = (paf_t *)o;
    
    float val = fts_get_float_long_arg( at, ac, 0, 0.0f);
    int time = fts_get_long_arg(at, ac, 1, 0);
    pafctl_amp(&this->pafctl, val, time);
}

static void paf_vib(fts_object_t *o, int winlet, fts_symbol_t *s,
    int ac, const fts_atom_t *at)
{
    paf_t *this = (paf_t *)o;
    float val = fts_get_float_long_arg( at, ac, 0, 0.0f);
    int time = fts_get_long_arg(at, ac, 1, 0);
    pafctl_vib(&this->pafctl, val, time);
}

static void paf_vfr(fts_object_t *o, int winlet, fts_symbol_t *s,
    int ac, const fts_atom_t *at)
{
    paf_t *this = (paf_t *)o;
    float val = fts_get_float_long_arg( at, ac, 0, 0.0f);
    int time = fts_get_long_arg(at, ac, 1, 0);
    pafctl_vfr(&this->pafctl, val, time);
}

static void paf_shift(fts_object_t *o, int winlet, fts_symbol_t *s,
    int ac, const fts_atom_t *at)
{
    paf_t *this = (paf_t *)o;
    float val = fts_get_float_long_arg( at, ac, 0, 0.0f);
    int time = fts_get_long_arg(at, ac, 1, 0);
    pafctl_shift(&this->pafctl, val, time);
}

static void paf_phase(fts_object_t *o, int winlet, fts_symbol_t *s,
    int ac, const fts_atom_t *at)
{
    paf_t *this = (paf_t *)o;
    float phase = fts_get_float_long_arg( at, ac, 0, 0.0f);
    float shiftphase = fts_get_long_arg(at, ac, 1, 0);
    float vibphase = fts_get_long_arg(at, ac, 2, 0);
    pafctl_shift(&this->pafctl, phase, shiftphase, vibphase);
}

static void
paf_print(fts_object_t *o, int winlet, fts_symbol_t *s, int ac, const fts_atom_t *at)
{
}

static void
paf_init(fts_object_t *o, int winlet, fts_symbol_t *s, int ac, const fts_atom_t *at)
{
    paf_t *this = (paf_t *)o;
    int i;

    dsp_list_insert(o); 	/* put object in list */
    pafctl_init(&this->pafctl);
}

static void paf_delete(fts_object_t *o, int winlet, fts_symbol_t *s,
    int ac, const fts_atom_t *at)
{
    paf_t *this = (paf_t *)o;

    dsp_list_remove(o);
}

static fts_status_t paf_instantiate( fts_class_t *cl,
    int ac, const fts_atom_t *at)
{
    fts_atom_type_t a[3];
    
    fts_class_init( cl, sizeof(paf_t), 1, 2, 0);
    
    a[0] = fts_Symbol;
    fts_method_define( cl, fts_SystemInlet, fts_s_init, paf_init, 1, a);
    fts_method_define( cl, fts_SystemInlet, fts_s_delete, paf_delete, 1, a);
    
    a[0] = fts_Float|fts_Long;
    a[1] = fts_Long;
    fts_method_define( cl, 0, fts_new_symbol("freq"), paf_freq, 2, a);
    fts_method_define( cl, 0, fts_new_symbol("cf"), paf_cf, 2, a);
    fts_method_define( cl, 0, fts_new_symbol("bw"), paf_bw, 2, a);
    fts_method_define( cl, 0, fts_new_symbol("amp"), paf_amp, 2, a);
    fts_method_define( cl, 0, fts_new_symbol("vib"), paf_vib, 2, a);
    fts_method_define( cl, 0, fts_new_symbol("vfr"), paf_vfr, 2, a);
    fts_method_define( cl, 0, fts_new_symbol("shift"), paf_shift, 2, a);
    fts_method_define( cl, 0, fts_new_symbol("phase"), paf_phase, 3, a);
    
    fts_method_define( cl, 0, fts_new_symbol("print"), paf_print, 0, 0);
    
    a[0] = fts_Object;  
    fts_method_define(cl, fts_SystemInlet, fts_new_symbol("put"),
	paf_put, 1, a);
    
    dsp_sig_inlet(cl, 0);	/* order forcing only */
    
    dsp_sig_outlet(cl, 0);
    
    return fts_Success;
}

fts_symbol_t *paf_qui;

void paf_config(void)
{
    sys_log(paf_version);
    paf_dspname = fts_new_symbol("paf");
    dsp_declare_function(paf_dspname, paf_dspfun);
    fts_metaclass_create(fts_new_symbol("paf"),
	paf_instantiate, fts_always_equiv);
    paf_dosetup();
}

fts_module_t paf_module =
  {"paf", "paf", paf_config, 0};

#endif /* FTS1X */

#ifdef V26

typedef struct _paf
{
    t_head x_h;
    t_sig *x_io[IN1+OUT1];
    t_pafctl x_pafctl;
} t_paf;

static void paf_put(t_paf *x, long int whether)
{
    if (whether)
    {
	float sr = x->x_io[0]->s_sr;
	int vecsize = x->x_io[0]->s_n;
    	u_stdout((t_ugen *)x);
	pafctl_setsr(&x->x_pafctl, sr, vecsize);
	dspchain_addc(pafctl_run, 3,
	    &x->x_pafctl, x->x_io[1]->s_shit, x->x_io[1]->s_n);
    }
}

static void paf_freq(t_paf *x, double val, int time)
{
    pafctl_freq(&x->x_pafctl, val, time);
}

static void paf_cf(t_paf *x, double val, int time)
{
    pafctl_cf(&x->x_pafctl, val, time);
}

static void paf_bw(t_paf *x, double val, int time)
{
    pafctl_bw(&x->x_pafctl, val, time);
}

static void paf_amp(t_paf *x, double val, int time)
{
    pafctl_amp(&x->x_pafctl, val, time);
}

static void paf_vib(t_paf *x, double val, int time)
{
    pafctl_vib(&x->x_pafctl, val, time);
}

static void paf_vfr(t_paf *x, double val, int time)
{
    pafctl_vfr(&x->x_pafctl, val, time);
}

static void paf_shift(t_paf *x, double val, int time)
{
    pafctl_shift(&x->x_pafctl, val, time);
}

static void paf_phase(t_paf *x, double phase, double shiftphase,
    double vibphase)
{
    pafctl_phase(&x->x_pafctl, phase, shiftphase, vibphase);
}

static void paf_debug(t_paf *x)
{
    linenv_debug( &x->x_pafctl.x_ampenv, "amp");
    linenv_debug(&x->x_pafctl.x_freqenv, "fre");
}

t_externclass *paf_class;

static void *paf_new()
{
    t_paf *x = (t_paf *)obj_new(&paf_class, 0);
    u_setup((t_ugen *)x, IN1, OUT1);
    u_setforcer(x);
    pafctl_init(&x->x_pafctl);
    return (x);
}

void sigpaf_setup()
{
    post(paf_version);
    c_extern(&paf_class, paf_new, u_clean,
	gensym("paf"), sizeof(t_paf), 0,  0);
    c_addmess(paf_put, gensym("put"), A_CANT, 0);
    c_addmess(paf_freq, gensym("freq"), A_FLOAT, A_LONG, 0);
    c_addmess(paf_cf, gensym("cf"), A_FLOAT, A_LONG, 0);
    c_addmess(paf_bw, gensym("bw"), A_FLOAT, A_LONG, 0);
    c_addmess(paf_amp, gensym("amp"), A_FLOAT, A_LONG, 0);
    c_addmess(paf_vib, gensym("vib"), A_FLOAT, A_LONG, 0);
    c_addmess(paf_vfr, gensym("vfr"), A_FLOAT, A_LONG, 0);
    c_addmess(paf_shift, gensym("shift"), A_FLOAT, A_LONG, 0);
    c_addmess(paf_phase, gensym("phase"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    c_addmess(paf_debug, gensym("debug"), 0);
    u_inletmethod(0);
    paf_dosetup();
}

#endif /* V26 */

#ifdef PD

typedef struct _paf
{
    t_object x_obj;
    t_pafctl x_pafctl;
} t_paf;

static t_class *paf_class;

static void *paf_new(void)
{
    t_paf *x = (t_paf *)pd_new(paf_class);
    pafctl_init(&x->x_pafctl);
    outlet_new(&x->x_obj, gensym("signal"));
    return (x);
}

static t_int *paf_perform(t_int *w)
{
    t_pafctl *ctl = (t_pafctl *)(w[1]);
    t_float *out1 = (t_float *)(w[2]);
    int n = (int)(w[3]);
    pafctl_run(ctl, out1, n);
    return (w+4);
}

static void paf_dsp(t_paf *x, t_signal **sp)
{
    float sr = sp[0]->s_sr;
    int vecsize = sp[0]->s_n;
    pafctl_setsr(&x->x_pafctl, sr, vecsize);
    dsp_add(paf_perform, 3,
	&x->x_pafctl, sp[0]->s_vec, sp[0]->s_n);
}

static void paf_freq(t_paf *x, t_floatarg val, t_floatarg time)
{
    pafctl_freq(&x->x_pafctl, val, time);
}

static void paf_cf(t_paf *x, t_floatarg val, t_floatarg time)
{
    pafctl_cf(&x->x_pafctl, val, time);
}

static void paf_bw(t_paf *x, t_floatarg val, t_floatarg time)
{
    pafctl_bw(&x->x_pafctl, val, time);
}

static void paf_amp(t_paf *x, t_floatarg val, t_floatarg time)
{
    pafctl_amp(&x->x_pafctl, val, time);
}

static void paf_vib(t_paf *x, t_floatarg val, t_floatarg time)
{
    pafctl_vib(&x->x_pafctl, val, time);
}

static void paf_vfr(t_paf *x, t_floatarg val, t_floatarg time)
{
    pafctl_vfr(&x->x_pafctl, val, time);
}

static void paf_shift(t_paf *x, t_floatarg val, t_floatarg time)
{
    pafctl_shift(&x->x_pafctl, val, time);
}

static void paf_phase(t_paf *x, t_floatarg mainphase, t_floatarg shiftphase,
    t_floatarg vibphase)
{
    pafctl_phase(&x->x_pafctl, mainphase, shiftphase, vibphase);
}

static void paf_setcauchy(t_paf *x, t_floatarg f)
{
    x->x_pafctl.x_cauchy = (f != 0);
}

static void paf_debug(t_paf *x)
{
    /* whatever you want... */
}

void paf_tilde_setup(void)
{
    post(paf_version);
    paf_class = class_new(gensym("paf~"), (t_newmethod)paf_new, 0,
    	sizeof(t_paf), 0, 0);
    class_addmethod(paf_class, (t_method)paf_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(paf_class, (t_method)paf_freq, gensym("freq"),
    	A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(paf_class, (t_method)paf_cf, gensym("cf"),
    	A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(paf_class, (t_method)paf_bw, gensym("bw"),
    	A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(paf_class, (t_method)paf_amp, gensym("amp"),
    	A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(paf_class, (t_method)paf_vib, gensym("vib"),
    	A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(paf_class, (t_method)paf_vfr, gensym("vfr"),
    	A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(paf_class, (t_method)paf_shift, gensym("shift"),
    	A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(paf_class, (t_method)paf_phase, gensym("phase"),
    	A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(paf_class, (t_method)paf_setcauchy, gensym("cauchy"),
    	A_FLOAT, 0);
    class_addmethod(paf_class, (t_method)paf_debug, gensym("debug"), 0);
    paf_dosetup();
}

#endif /* PD */
