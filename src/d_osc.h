/* Copyright (c) 1997-2024 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* performance routines for cos~, osc~, and vcf~.  These are broken out in
this file so that they can be included twice with different hard-coded vector
sizes.  (This gives a modest improvement in execution time.  In the 1990s it
was considered extremely important to have the very fastest oscillator you
could make.  I'm not sure how deeply important this is today, but am doing
it rather than have to reconsider my programming habits :)

N.B. this could also be some ugly macro in d_osc.c itself with every line
terminated in a backslash; I think this is slightly less odious than that
would have been.

The macros COSPERF, etc. should expand to the name of the routine, and
COSTABLESIZE and COSTABLENAME should be set variously to the standard table
size and to OLDTABSIZE, and corresponding table pointers, as appropriate.
*/


static t_int *COSPERF(t_int *w)
{
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    float *tab = COSTABLENAME, *addr;
    t_float f1, f2, frac;
    double dphase;
    int normhipart;
    union tabfudge tf;

    tf.tf_d = UNITBIT32;
    normhipart = tf.tf_i[HIOFFSET];

#if 0           /* this is the readable version of the code. */
    while (n--)
    {
        dphase = (double)(*in++ * (float)(COSTABLESIZE)) + UNITBIT32;
        tf.tf_d = dphase;
        addr = tab + (tf.tf_i[HIOFFSET] & (COSTABLESIZE-1));
        tf.tf_i[HIOFFSET] = normhipart;
        frac = tf.tf_d - UNITBIT32;
        f1 = addr[0];
        f2 = addr[1];
        *out++ = f1 + frac * (f2 - f1);
    }
#endif
#if 1           /* this is the same, unwrapped by hand. */
        dphase = (double)(*in++ * (float)(COSTABLESIZE)) + UNITBIT32;
        tf.tf_d = dphase;
        addr = tab + (tf.tf_i[HIOFFSET] & (COSTABLESIZE-1));
        tf.tf_i[HIOFFSET] = normhipart;
    while (--n)
    {
        dphase = (double)(*in++ * (float)(COSTABLESIZE)) + UNITBIT32;
            frac = tf.tf_d - UNITBIT32;
        tf.tf_d = dphase;
            f1 = addr[0];
            f2 = addr[1];
        addr = tab + (tf.tf_i[HIOFFSET] & (COSTABLESIZE-1));
            *out++ = f1 + frac * (f2 - f1);
        tf.tf_i[HIOFFSET] = normhipart;
    }
            frac = tf.tf_d - UNITBIT32;
            f1 = addr[0];
            f2 = addr[1];
            *out++ = f1 + frac * (f2 - f1);
#endif
    return (w+4);
}

static t_int *OSCPERF(t_int *w)
{
    t_osc *x = (t_osc *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    float *tab = COSTABLENAME, *addr;
    t_float f1, f2, frac;
    double dphase = x->x_phase + UNITBIT32;
    int normhipart;
    union tabfudge tf;
    float conv = x->x_conv;

    tf.tf_d = UNITBIT32;
    normhipart = tf.tf_i[HIOFFSET];
#if 0
    while (n--)
    {
        tf.tf_d = dphase;
        dphase += *in++ * conv;
        addr = tab + (tf.tf_i[HIOFFSET] & (COSTABLESIZE-1));
        tf.tf_i[HIOFFSET] = normhipart;
        frac = tf.tf_d - UNITBIT32;
        f1 = addr[0];
        f2 = addr[1];
        *out++ = f1 + frac * (f2 - f1);
    }
#endif
#if 1
        tf.tf_d = dphase;
        dphase += *in++ * conv;
        addr = tab + (tf.tf_i[HIOFFSET] & (COSTABLESIZE-1));
        tf.tf_i[HIOFFSET] = normhipart;
        frac = tf.tf_d - UNITBIT32;
    while (--n)
    {
        tf.tf_d = dphase;
            f1 = addr[0];
        dphase += *in++ * conv;
            f2 = addr[1];
        addr = tab + (tf.tf_i[HIOFFSET] & (COSTABLESIZE-1));
        tf.tf_i[HIOFFSET] = normhipart;
            *out++ = f1 + frac * (f2 - f1);
        frac = tf.tf_d - UNITBIT32;
    }
            f1 = addr[0];
            f2 = addr[1];
            *out++ = f1 + frac * (f2 - f1);
#endif

    tf.tf_d = UNITBIT32 * COSTABLESIZE;
    normhipart = tf.tf_i[HIOFFSET];
    tf.tf_d = dphase + (UNITBIT32 * COSTABLESIZE - UNITBIT32);
    tf.tf_i[HIOFFSET] = normhipart;
    x->x_phase = tf.tf_d - UNITBIT32 * COSTABLESIZE;
    return (w+5);
}

static t_int *SIGVCFPERF(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out1 = (t_sample *)(w[3]);
    t_sample *out2 = (t_sample *)(w[4]);
    t_vcfctl *c = (t_vcfctl *)(w[5]);
    int n = (int)w[6];
    int i;
    t_float re = c->c_re, re2;
    t_float im = c->c_im;
    t_float q = c->c_q;
    t_float isr = c->c_isr;
    t_float qinv = (q > 0? 1.0f/q : 0);
    t_float ampcorrect = 2. - 2. / (q + 2.);
    t_float coefr, coefi;
    float *tab = COSTABLENAME, *addr, f1, f2, frac;
    double dphase;
    int normhipart, tabindex;
    union tabfudge tf;

    tf.tf_d = UNITBIT32;
    normhipart = tf.tf_i[HIOFFSET];

    for (i = 0; i < n; i++)
    {
        float cf, cfindx, r, oneminusr;
        cf = *in2++ * isr;
        if (cf < 0) cf = 0;
        cfindx = cf * (float)(COSTABLESIZE/6.28318f);
        r = (qinv > 0 ? 1 - cf * qinv : 0);
        if (r < 0) r = 0;
        oneminusr = 1.0f - r;
        dphase = ((double)(cfindx)) + UNITBIT32;
        tf.tf_d = dphase;
        tabindex = tf.tf_i[HIOFFSET] & (COSTABLESIZE-1);
        addr = tab + tabindex;
        tf.tf_i[HIOFFSET] = normhipart;
        frac = tf.tf_d - UNITBIT32;
        f1 = addr[0];
        f2 = addr[1];
        coefr = r * (f1 + frac * (f2 - f1));

        addr = tab + ((tabindex - (COSTABLESIZE/4)) & (COSTABLESIZE-1));
        f1 = addr[0];
        f2 = addr[1];
        coefi = r * (f1 + frac * (f2 - f1));

        f1 = *in1++;
        re2 = re;
        *out1++ = re = ampcorrect * oneminusr * f1
            + coefr * re2 - coefi * im;
        *out2++ = im = coefi * re2 + coefr * im;
    }
    if (PD_BIGORSMALL(re))
        re = 0;
    if (PD_BIGORSMALL(im))
        im = 0;
    c->c_re = re;
    c->c_im = im;
    return (w+7);
}

