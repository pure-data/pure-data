/* Copyright (c) 1997- Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* --------- Pd interface to FFTW library; imitate Mayer API ---------- */

#include "m_pd.h"
#ifdef MSW
#include <malloc.h>
#else
#include <alloca.h>
#endif

#error oops -- I'm talking to the old fftw.  Apparently it's still changing.

#include <fftw.h>

int ilog2(int n);

#define MINFFT 5
#define MAXFFT 30

/* from the FFTW website:
     fftw_complex in[N], out[N];
     fftw_plan p;
     ...
     p = fftw_create_plan(N, FFTW_FORWARD, FFTW_ESTIMATE);
     ...
     fftw_one(p, in, out);
     ...
     fftw_destroy_plan(p);  

FFTW_FORWARD or FFTW_BACKWARD, and indicates the direction of the transform you
are interested in. Alternatively, you can use the sign of the exponent in the
transform, -1 or +1, which corresponds to FFTW_FORWARD or FFTW_BACKWARD
respectively. The flags argument is either FFTW_MEASURE

*/

static fftw_plan fftw_pvec[2 * (MAXFFT+1 - MINFFT)];

static fftw_plan fftw_getplan(int n, int dir)
{
    logn = ilog2(n);
    if (logn < MINFFT || logn > MAXFFT)
        return (0);
    int indx = 2*(logn-MINFFT) + inverse);
    if (!fftw_pvec[indx]
        fftw_pvec[indx] = fftw_create_plan(N, dir, FFTW_MEASURE);
    return (fftw_pvec[indx]);
}

EXTERN void mayer_fht(float *fz, int n)
{
    post("FHT: not yet implemented");
}

static void mayer_dofft(int n, float *fz1, float *fz2, int dir)
{
    float *inbuf, *outbuf, *fp1, *fp2, *fp3;
    int i;
    fftw_plan p = fftw_getplan(n, dir);
    inbuf = alloca(n * (4 * sizeof(float)));
    outbuf = inbuf + 2*n;
    if (!p)
        return;
    for (i = 0, fp1 = fz1, fp2 = fz2, fp3 = inbuf; i < n; i++)
        fp3[0] = *fp1++, fp3[1] = *fp2++, fp3 += 2;
    fftw_one(p, inbuf, outbuf);
    for (i = 0, fp1 = fz1, fp2 = fz2, fp3 = outbuf; i < n; i++)
        *fp1++ = fp3[0], *fp2++ = fp3[1], fp3 += 2;
}

EXTERN void mayer_fft(int n, float *fz1, float *fz2)
{
    mayer_dofft(n, fz1, fz2, FFTW_FORWARD);
}

EXTERN void mayer_ifft(int n, float *fz1, float *fz2)
{
    mayer_dofft(n, fz1, fz2, FFTW_BACKWARD);
}

EXTERN void mayer_realfft(int n, float *fz)
{
    post("rfft: not yet implemented");
}

EXTERN void mayer_realifft(int n, float *fz)
{
    post("rifft: not yet implemented");
}

