/*
Fast Fourier/Cosine/Sine Transform
    dimension   :one
    data length :power of 2
    decimation  :frequency
    radix       :split-radix
    data        :inplace
    table       :not use
functions
    cdft: Complex Discrete Fourier Transform
    rdft: Real Discrete Fourier Transform
    ddct: Discrete Cosine Transform
    ddst: Discrete Sine Transform
    dfct: Cosine Transform of RDFT (Real Symmetric DFT)
    dfst: Sine Transform of RDFT (Real Anti-symmetric DFT)
function prototypes
    void cdft(int, int, double *);
    void rdft(int, int, double *);
    void ddct(int, int, double *);
    void ddst(int, int, double *);
    void dfct(int, double *);
    void dfst(int, double *);
macro definitions
    USE_CDFT_PTHREADS : default=not defined
        CDFT_THREADS_BEGIN_N  : must be >= 512, default=8192
        CDFT_4THREADS_BEGIN_N : must be >= 512, default=65536
    USE_CDFT_WINTHREADS : default=not defined
        CDFT_THREADS_BEGIN_N  : must be >= 512, default=32768
        CDFT_4THREADS_BEGIN_N : must be >= 512, default=524288


-------- Complex DFT (Discrete Fourier Transform) --------
    [definition]
        <case1>
            X[k] = sum_j=0^n-1 x[j]*exp(2*pi*i*j*k/n), 0<=k<n
        <case2>
            X[k] = sum_j=0^n-1 x[j]*exp(-2*pi*i*j*k/n), 0<=k<n
        (notes: sum_j=0^n-1 is a summation from j=0 to n-1)
    [usage]
        <case1>
            cdft(2*n, 1, a);
        <case2>
            cdft(2*n, -1, a);
    [parameters]
        2*n            :data length (int)
                        n >= 1, n = power of 2
        a[0...2*n-1]   :input/output data (double *)
                        input data
                            a[2*j] = Re(x[j]), 
                            a[2*j+1] = Im(x[j]), 0<=j<n
                        output data
                            a[2*k] = Re(X[k]), 
                            a[2*k+1] = Im(X[k]), 0<=k<n
    [remark]
        Inverse of 
            cdft(2*n, -1, a);
        is 
            cdft(2*n, 1, a);
            for (j = 0; j <= 2 * n - 1; j++) {
                a[j] *= 1.0 / n;
            }
        .


-------- Real DFT / Inverse of Real DFT --------
    [definition]
        <case1> RDFT
            R[k] = sum_j=0^n-1 a[j]*cos(2*pi*j*k/n), 0<=k<=n/2
            I[k] = sum_j=0^n-1 a[j]*sin(2*pi*j*k/n), 0<k<n/2
        <case2> IRDFT (excluding scale)
            a[k] = (R[0] + R[n/2]*cos(pi*k))/2 + 
                   sum_j=1^n/2-1 R[j]*cos(2*pi*j*k/n) + 
                   sum_j=1^n/2-1 I[j]*sin(2*pi*j*k/n), 0<=k<n
    [usage]
        <case1>
            rdft(n, 1, a);
        <case2>
            rdft(n, -1, a);
    [parameters]
        n              :data length (int)
                        n >= 2, n = power of 2
        a[0...n-1]     :input/output data (double *)
                        <case1>
                            output data
                                a[2*k] = R[k], 0<=k<n/2
                                a[2*k+1] = I[k], 0<k<n/2
                                a[1] = R[n/2]
                        <case2>
                            input data
                                a[2*j] = R[j], 0<=j<n/2
                                a[2*j+1] = I[j], 0<j<n/2
                                a[1] = R[n/2]
    [remark]
        Inverse of 
            rdft(n, 1, a);
        is 
            rdft(n, -1, a);
            for (j = 0; j <= n - 1; j++) {
                a[j] *= 2.0 / n;
            }
        .


-------- DCT (Discrete Cosine Transform) / Inverse of DCT --------
    [definition]
        <case1> IDCT (excluding scale)
            C[k] = sum_j=0^n-1 a[j]*cos(pi*j*(k+1/2)/n), 0<=k<n
        <case2> DCT
            C[k] = sum_j=0^n-1 a[j]*cos(pi*(j+1/2)*k/n), 0<=k<n
    [usage]
        <case1>
            ddct(n, 1, a);
        <case2>
            ddct(n, -1, a);
    [parameters]
        n              :data length (int)
                        n >= 2, n = power of 2
        a[0...n-1]     :input/output data (double *)
                        output data
                            a[k] = C[k], 0<=k<n
    [remark]
        Inverse of 
            ddct(n, -1, a);
        is 
            a[0] *= 0.5;
            ddct(n, 1, a);
            for (j = 0; j <= n - 1; j++) {
                a[j] *= 2.0 / n;
            }
        .


-------- DST (Discrete Sine Transform) / Inverse of DST --------
    [definition]
        <case1> IDST (excluding scale)
            S[k] = sum_j=1^n A[j]*sin(pi*j*(k+1/2)/n), 0<=k<n
        <case2> DST
            S[k] = sum_j=0^n-1 a[j]*sin(pi*(j+1/2)*k/n), 0<k<=n
    [usage]
        <case1>
            ddst(n, 1, a);
        <case2>
            ddst(n, -1, a);
    [parameters]
        n              :data length (int)
                        n >= 2, n = power of 2
        a[0...n-1]     :input/output data (double *)
                        <case1>
                            input data
                                a[j] = A[j], 0<j<n
                                a[0] = A[n]
                            output data
                                a[k] = S[k], 0<=k<n
                        <case2>
                            output data
                                a[k] = S[k], 0<k<n
                                a[0] = S[n]
    [remark]
        Inverse of 
            ddst(n, -1, a);
        is 
            a[0] *= 0.5;
            ddst(n, 1, a);
            for (j = 0; j <= n - 1; j++) {
                a[j] *= 2.0 / n;
            }
        .


-------- Cosine Transform of RDFT (Real Symmetric DFT) --------
    [definition]
        C[k] = sum_j=0^n a[j]*cos(pi*j*k/n), 0<=k<=n
    [usage]
        dfct(n, a);
    [parameters]
        n              :data length - 1 (int)
                        n >= 2, n = power of 2
        a[0...n]       :input/output data (double *)
                        output data
                            a[k] = C[k], 0<=k<=n
    [remark]
        Inverse of 
            a[0] *= 0.5;
            a[n] *= 0.5;
            dfct(n, a);
        is 
            a[0] *= 0.5;
            a[n] *= 0.5;
            dfct(n, a);
            for (j = 0; j <= n; j++) {
                a[j] *= 2.0 / n;
            }
        .


-------- Sine Transform of RDFT (Real Anti-symmetric DFT) --------
    [definition]
        S[k] = sum_j=1^n-1 a[j]*sin(pi*j*k/n), 0<k<n
    [usage]
        dfst(n, a);
    [parameters]
        n              :data length + 1 (int)
                        n >= 2, n = power of 2
        a[0...n-1]     :input/output data (double *)
                        output data
                            a[k] = S[k], 0<k<n
                        (a[0] is used for work area)
    [remark]
        Inverse of 
            dfst(n, a);
        is 
            dfst(n, a);
            for (j = 1; j <= n - 1; j++) {
                a[j] *= 2.0 / n;
            }
        .
*/


void cdft(int n, int isgn, double *a)
{
    void cftfsub(int n, double *a);
    void cftbsub(int n, double *a);
    
    if (isgn >= 0) {
        cftfsub(n, a);
    } else {
        cftbsub(n, a);
    }
}


void rdft(int n, int isgn, double *a)
{
    void cftfsub(int n, double *a);
    void cftbsub(int n, double *a);
    void rftfsub(int n, double *a);
    void rftbsub(int n, double *a);
    double xi;
    
    if (isgn >= 0) {
        if (n > 4) {
            cftfsub(n, a);
            rftfsub(n, a);
        } else if (n == 4) {
            cftfsub(n, a);
        }
        xi = a[0] - a[1];
        a[0] += a[1];
        a[1] = xi;
    } else {
        a[1] = 0.5 * (a[0] - a[1]);
        a[0] -= a[1];
        if (n > 4) {
            rftbsub(n, a);
            cftbsub(n, a);
        } else if (n == 4) {
            cftbsub(n, a);
        }
    }
}


void ddct(int n, int isgn, double *a)
{
    void cftfsub(int n, double *a);
    void cftbsub(int n, double *a);
    void rftfsub(int n, double *a);
    void rftbsub(int n, double *a);
    void dctsub(int n, double *a);
    void dctsub4(int n, double *a);
    int j;
    double xr;
    
    if (isgn < 0) {
        xr = a[n - 1];
        for (j = n - 2; j >= 2; j -= 2) {
            a[j + 1] = a[j] - a[j - 1];
            a[j] += a[j - 1];
        }
        a[1] = a[0] - xr;
        a[0] += xr;
        if (n > 4) {
            rftbsub(n, a);
            cftbsub(n, a);
        } else if (n == 4) {
            cftbsub(n, a);
        }
    }
    if (n > 4) {
        dctsub(n, a);
    } else {
        dctsub4(n, a);
    }
    if (isgn >= 0) {
        if (n > 4) {
            cftfsub(n, a);
            rftfsub(n, a);
        } else if (n == 4) {
            cftfsub(n, a);
        }
        xr = a[0] - a[1];
        a[0] += a[1];
        for (j = 2; j < n; j += 2) {
            a[j - 1] = a[j] - a[j + 1];
            a[j] += a[j + 1];
        }
        a[n - 1] = xr;
    }
}


void ddst(int n, int isgn, double *a)
{
    void cftfsub(int n, double *a);
    void cftbsub(int n, double *a);
    void rftfsub(int n, double *a);
    void rftbsub(int n, double *a);
    void dstsub(int n, double *a);
    void dstsub4(int n, double *a);
    int j;
    double xr;
    
    if (isgn < 0) {
        xr = a[n - 1];
        for (j = n - 2; j >= 2; j -= 2) {
            a[j + 1] = -a[j] - a[j - 1];
            a[j] -= a[j - 1];
        }
        a[1] = a[0] + xr;
        a[0] -= xr;
        if (n > 4) {
            rftbsub(n, a);
            cftbsub(n, a);
        } else if (n == 4) {
            cftbsub(n, a);
        }
    }
    if (n > 4) {
        dstsub(n, a);
    } else {
        dstsub4(n, a);
    }
    if (isgn >= 0) {
        if (n > 4) {
            cftfsub(n, a);
            rftfsub(n, a);
        } else if (n == 4) {
            cftfsub(n, a);
        }
        xr = a[0] - a[1];
        a[0] += a[1];
        for (j = 2; j < n; j += 2) {
            a[j - 1] = -a[j] - a[j + 1];
            a[j] -= a[j + 1];
        }
        a[n - 1] = -xr;
    }
}


void dfct(int n, double *a)
{
    void ddct(int n, int isgn, double *a);
    void bitrv1(int n, double *a);
    int j, k, m, mh;
    double xr, xi, yr, yi, an;
    
    m = n >> 1;
    for (j = 0; j < m; j++) {
        k = n - j;
        xr = a[j] + a[k];
        a[j] -= a[k];
        a[k] = xr;
    }
    an = a[n];
    while (m >= 2) {
        ddct(m, 1, a);
        if (m > 2) {
            bitrv1(m, a);
        }
        mh = m >> 1;
        xi = a[m];
        a[m] = a[0];
        a[0] = an - xi;
        an += xi;
        for (j = 1; j < mh; j++) {
            k = m - j;
            xr = a[m + k];
            xi = a[m + j];
            yr = a[j];
            yi = a[k];
            a[m + j] = yr;
            a[m + k] = yi;
            a[j] = xr - xi;
            a[k] = xr + xi;
        }
        xr = a[mh];
        a[mh] = a[m + mh];
        a[m + mh] = xr;
        m = mh;
    }
    xi = a[1];
    a[1] = a[0];
    a[0] = an + xi;
    a[n] = an - xi;
    if (n > 2) {
        bitrv1(n, a);
    }
}


void dfst(int n, double *a)
{
    void ddst(int n, int isgn, double *a);
    void bitrv1(int n, double *a);
    int j, k, m, mh;
    double xr, xi, yr, yi;
    
    m = n >> 1;
    for (j = 1; j < m; j++) {
        k = n - j;
        xr = a[j] - a[k];
        a[j] += a[k];
        a[k] = xr;
    }
    a[0] = a[m];
    while (m >= 2) {
        ddst(m, 1, a);
        if (m > 2) {
            bitrv1(m, a);
        }
        mh = m >> 1;
        for (j = 1; j < mh; j++) {
            k = m - j;
            xr = a[m + k];
            xi = a[m + j];
            yr = a[j];
            yi = a[k];
            a[m + j] = yr;
            a[m + k] = yi;
            a[j] = xr + xi;
            a[k] = xr - xi;
        }
        a[m] = a[0];
        a[0] = a[m + mh];
        a[m + mh] = a[mh];
        m = mh;
    }
    a[1] = a[0];
    a[0] = 0;
    if (n > 2) {
        bitrv1(n, a);
    }
}


/* -------- child routines -------- */


#include <math.h>
#ifndef M_PI_2
#define M_PI_2      1.570796326794896619231321691639751442098584699687
#endif
#ifndef WR5000  /* cos(M_PI_2*0.5000) */
#define WR5000      0.707106781186547524400844362104849039284835937688
#endif
#ifndef WR2500  /* cos(M_PI_2*0.2500) */
#define WR2500      0.923879532511286756128183189396788286822416625863
#endif
#ifndef WI2500  /* sin(M_PI_2*0.2500) */
#define WI2500      0.382683432365089771728459984030398866761344562485
#endif
#ifndef WR1250  /* cos(M_PI_2*0.1250) */
#define WR1250      0.980785280403230449126182236134239036973933730893
#endif
#ifndef WI1250  /* sin(M_PI_2*0.1250) */
#define WI1250      0.195090322016128267848284868477022240927691617751
#endif
#ifndef WR3750  /* cos(M_PI_2*0.3750) */
#define WR3750      0.831469612302545237078788377617905756738560811987
#endif
#ifndef WI3750  /* sin(M_PI_2*0.3750) */
#define WI3750      0.555570233019602224742830813948532874374937190754
#endif


#ifdef USE_CDFT_PTHREADS
#define USE_CDFT_THREADS
#ifndef CDFT_THREADS_BEGIN_N
#define CDFT_THREADS_BEGIN_N 8192
#endif
#ifndef CDFT_4THREADS_BEGIN_N
#define CDFT_4THREADS_BEGIN_N 65536
#endif
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define cdft_thread_t pthread_t
#define cdft_thread_create(thp,func,argp) { \
    if (pthread_create(thp, NULL, func, (void *) argp) != 0) { \
        fprintf(stderr, "cdft thread error\n"); \
        exit(1); \
    } \
}
#define cdft_thread_wait(th) { \
    if (pthread_join(th, NULL) != 0) { \
        fprintf(stderr, "cdft thread error\n"); \
        exit(1); \
    } \
}
#endif /* USE_CDFT_PTHREADS */


#ifdef USE_CDFT_WINTHREADS
#define USE_CDFT_THREADS
#ifndef CDFT_THREADS_BEGIN_N
#define CDFT_THREADS_BEGIN_N 32768
#endif
#ifndef CDFT_4THREADS_BEGIN_N
#define CDFT_4THREADS_BEGIN_N 524288
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#define cdft_thread_t HANDLE
#define cdft_thread_create(thp,func,argp) { \
    DWORD thid; \
    *(thp) = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) func, (LPVOID) argp, 0, &thid); \
    if (*(thp) == 0) { \
        fprintf(stderr, "cdft thread error\n"); \
        exit(1); \
    } \
}
#define cdft_thread_wait(th) { \
    WaitForSingleObject(th, INFINITE); \
    CloseHandle(th); \
}
#endif /* USE_CDFT_WINTHREADS */


#ifndef CDFT_LOOP_DIV  /* control of the CDFT's speed & tolerance */
#define CDFT_LOOP_DIV 32
#endif

#ifndef RDFT_LOOP_DIV  /* control of the RDFT's speed & tolerance */
#define RDFT_LOOP_DIV 64
#endif

#ifndef DCST_LOOP_DIV  /* control of the DCT,DST's speed & tolerance */
#define DCST_LOOP_DIV 64
#endif


void cftfsub(int n, double *a)
{
    void bitrv2(int n, double *a);
    void bitrv216(double *a);
    void bitrv208(double *a);
    void cftmdl1(int n, double *a);
    void cftrec4(int n, double *a);
    void cftleaf(int n, int isplt, double *a);
    void cftfx41(int n, double *a);
    void cftf161(double *a);
    void cftf081(double *a);
    void cftf040(double *a);
    void cftx020(double *a);
#ifdef USE_CDFT_THREADS
    void cftrec4_th(int n, double *a);
#endif /* USE_CDFT_THREADS */
    
    if (n > 8) {
        if (n > 32) {
            cftmdl1(n, a);
#ifdef USE_CDFT_THREADS
            if (n > CDFT_THREADS_BEGIN_N) {
                cftrec4_th(n, a);
            } else 
#endif /* USE_CDFT_THREADS */
            if (n > 512) {
                cftrec4(n, a);
            } else if (n > 128) {
                cftleaf(n, 1, a);
            } else {
                cftfx41(n, a);
            }
            bitrv2(n, a);
        } else if (n == 32) {
            cftf161(a);
            bitrv216(a);
        } else {
            cftf081(a);
            bitrv208(a);
        }
    } else if (n == 8) {
        cftf040(a);
    } else if (n == 4) {
        cftx020(a);
    }
}


void cftbsub(int n, double *a)
{
    void bitrv2conj(int n, double *a);
    void bitrv216neg(double *a);
    void bitrv208neg(double *a);
    void cftb1st(int n, double *a);
    void cftrec4(int n, double *a);
    void cftleaf(int n, int isplt, double *a);
    void cftfx41(int n, double *a);
    void cftf161(double *a);
    void cftf081(double *a);
    void cftb040(double *a);
    void cftx020(double *a);
#ifdef USE_CDFT_THREADS
    void cftrec4_th(int n, double *a);
#endif /* USE_CDFT_THREADS */
    
    if (n > 8) {
        if (n > 32) {
            cftb1st(n, a);
#ifdef USE_CDFT_THREADS
            if (n > CDFT_THREADS_BEGIN_N) {
                cftrec4_th(n, a);
            } else 
#endif /* USE_CDFT_THREADS */
            if (n > 512) {
                cftrec4(n, a);
            } else if (n > 128) {
                cftleaf(n, 1, a);
            } else {
                cftfx41(n, a);
            }
            bitrv2conj(n, a);
        } else if (n == 32) {
            cftf161(a);
            bitrv216neg(a);
        } else {
            cftf081(a);
            bitrv208neg(a);
        }
    } else if (n == 8) {
        cftb040(a);
    } else if (n == 4) {
        cftx020(a);
    }
}


void bitrv2(int n, double *a)
{
    int j0, k0, j1, k1, l, m, i, j, k, nh;
    double xr, xi, yr, yi;
    
    m = 4;
    for (l = n >> 2; l > 8; l >>= 2) {
        m <<= 1;
    }
    nh = n >> 1;
    if (l == 8) {
        j0 = 0;
        for (k0 = 0; k0 < m; k0 += 4) {
            k = k0;
            for (j = j0; j < j0 + k0; j += 4) {
                xr = a[j];
                xi = a[j + 1];
                yr = a[k];
                yi = a[k + 1];
                a[j] = yr;
                a[j + 1] = yi;
                a[k] = xr;
                a[k + 1] = xi;
                j1 = j + m;
                k1 = k + 2 * m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 -= m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 += 2 * m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += nh;
                k1 += 2;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= 2 * m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 += m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= 2 * m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += 2;
                k1 += nh;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 += 2 * m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 -= m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 += 2 * m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= nh;
                k1 -= 2;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= 2 * m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 += m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= 2 * m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                for (i = nh >> 1; i > (k ^= i); i >>= 1);
            }
            k1 = j0 + k0;
            j1 = k1 + 2;
            k1 += nh;
            xr = a[j1];
            xi = a[j1 + 1];
            yr = a[k1];
            yi = a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            j1 += m;
            k1 += 2 * m;
            xr = a[j1];
            xi = a[j1 + 1];
            yr = a[k1];
            yi = a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            j1 += m;
            k1 -= m;
            xr = a[j1];
            xi = a[j1 + 1];
            yr = a[k1];
            yi = a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            j1 -= 2;
            k1 -= nh;
            xr = a[j1];
            xi = a[j1 + 1];
            yr = a[k1];
            yi = a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            j1 += nh + 2;
            k1 += nh + 2;
            xr = a[j1];
            xi = a[j1 + 1];
            yr = a[k1];
            yi = a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            j1 -= nh - m;
            k1 += 2 * m - 2;
            xr = a[j1];
            xi = a[j1 + 1];
            yr = a[k1];
            yi = a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            for (i = nh >> 1; i > (j0 ^= i); i >>= 1);
        }
    } else {
        j0 = 0;
        for (k0 = 0; k0 < m; k0 += 4) {
            k = k0;
            for (j = j0; j < j0 + k0; j += 4) {
                xr = a[j];
                xi = a[j + 1];
                yr = a[k];
                yi = a[k + 1];
                a[j] = yr;
                a[j + 1] = yi;
                a[k] = xr;
                a[k + 1] = xi;
                j1 = j + m;
                k1 = k + m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += nh;
                k1 += 2;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += 2;
                k1 += nh;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 += m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= nh;
                k1 -= 2;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= m;
                xr = a[j1];
                xi = a[j1 + 1];
                yr = a[k1];
                yi = a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                for (i = nh >> 1; i > (k ^= i); i >>= 1);
            }
            k1 = j0 + k0;
            j1 = k1 + 2;
            k1 += nh;
            xr = a[j1];
            xi = a[j1 + 1];
            yr = a[k1];
            yi = a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            j1 += m;
            k1 += m;
            xr = a[j1];
            xi = a[j1 + 1];
            yr = a[k1];
            yi = a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            for (i = nh >> 1; i > (j0 ^= i); i >>= 1);
        }
    }
}


void bitrv2conj(int n, double *a)
{
    int j0, k0, j1, k1, l, m, i, j, k, nh;
    double xr, xi, yr, yi;
    
    m = 4;
    for (l = n >> 2; l > 8; l >>= 2) {
        m <<= 1;
    }
    nh = n >> 1;
    if (l == 8) {
        j0 = 0;
        for (k0 = 0; k0 < m; k0 += 4) {
            k = k0;
            for (j = j0; j < j0 + k0; j += 4) {
                xr = a[j];
                xi = -a[j + 1];
                yr = a[k];
                yi = -a[k + 1];
                a[j] = yr;
                a[j + 1] = yi;
                a[k] = xr;
                a[k + 1] = xi;
                j1 = j + m;
                k1 = k + 2 * m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 -= m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 += 2 * m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += nh;
                k1 += 2;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= 2 * m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 += m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= 2 * m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += 2;
                k1 += nh;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 += 2 * m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 -= m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 += 2 * m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= nh;
                k1 -= 2;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= 2 * m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 += m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= 2 * m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                for (i = nh >> 1; i > (k ^= i); i >>= 1);
            }
            k1 = j0 + k0;
            j1 = k1 + 2;
            k1 += nh;
            a[j1 - 1] = -a[j1 - 1];
            xr = a[j1];
            xi = -a[j1 + 1];
            yr = a[k1];
            yi = -a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            a[k1 + 3] = -a[k1 + 3];
            j1 += m;
            k1 += 2 * m;
            xr = a[j1];
            xi = -a[j1 + 1];
            yr = a[k1];
            yi = -a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            j1 += m;
            k1 -= m;
            xr = a[j1];
            xi = -a[j1 + 1];
            yr = a[k1];
            yi = -a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            j1 -= 2;
            k1 -= nh;
            xr = a[j1];
            xi = -a[j1 + 1];
            yr = a[k1];
            yi = -a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            j1 += nh + 2;
            k1 += nh + 2;
            xr = a[j1];
            xi = -a[j1 + 1];
            yr = a[k1];
            yi = -a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            j1 -= nh - m;
            k1 += 2 * m - 2;
            a[j1 - 1] = -a[j1 - 1];
            xr = a[j1];
            xi = -a[j1 + 1];
            yr = a[k1];
            yi = -a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            a[k1 + 3] = -a[k1 + 3];
            for (i = nh >> 1; i > (j0 ^= i); i >>= 1);
        }
    } else {
        j0 = 0;
        for (k0 = 0; k0 < m; k0 += 4) {
            k = k0;
            for (j = j0; j < j0 + k0; j += 4) {
                xr = a[j];
                xi = -a[j + 1];
                yr = a[k];
                yi = -a[k + 1];
                a[j] = yr;
                a[j + 1] = yi;
                a[k] = xr;
                a[k + 1] = xi;
                j1 = j + m;
                k1 = k + m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += nh;
                k1 += 2;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += 2;
                k1 += nh;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m;
                k1 += m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= nh;
                k1 -= 2;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 -= m;
                k1 -= m;
                xr = a[j1];
                xi = -a[j1 + 1];
                yr = a[k1];
                yi = -a[k1 + 1];
                a[j1] = yr;
                a[j1 + 1] = yi;
                a[k1] = xr;
                a[k1 + 1] = xi;
                for (i = nh >> 1; i > (k ^= i); i >>= 1);
            }
            k1 = j0 + k0;
            j1 = k1 + 2;
            k1 += nh;
            a[j1 - 1] = -a[j1 - 1];
            xr = a[j1];
            xi = -a[j1 + 1];
            yr = a[k1];
            yi = -a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            a[k1 + 3] = -a[k1 + 3];
            j1 += m;
            k1 += m;
            a[j1 - 1] = -a[j1 - 1];
            xr = a[j1];
            xi = -a[j1 + 1];
            yr = a[k1];
            yi = -a[k1 + 1];
            a[j1] = yr;
            a[j1 + 1] = yi;
            a[k1] = xr;
            a[k1 + 1] = xi;
            a[k1 + 3] = -a[k1 + 3];
            for (i = nh >> 1; i > (j0 ^= i); i >>= 1);
        }
    }
}


void bitrv216(double *a)
{
    double x1r, x1i, x2r, x2i, x3r, x3i, x4r, x4i, 
        x5r, x5i, x7r, x7i, x8r, x8i, x10r, x10i, 
        x11r, x11i, x12r, x12i, x13r, x13i, x14r, x14i;
    
    x1r = a[2];
    x1i = a[3];
    x2r = a[4];
    x2i = a[5];
    x3r = a[6];
    x3i = a[7];
    x4r = a[8];
    x4i = a[9];
    x5r = a[10];
    x5i = a[11];
    x7r = a[14];
    x7i = a[15];
    x8r = a[16];
    x8i = a[17];
    x10r = a[20];
    x10i = a[21];
    x11r = a[22];
    x11i = a[23];
    x12r = a[24];
    x12i = a[25];
    x13r = a[26];
    x13i = a[27];
    x14r = a[28];
    x14i = a[29];
    a[2] = x8r;
    a[3] = x8i;
    a[4] = x4r;
    a[5] = x4i;
    a[6] = x12r;
    a[7] = x12i;
    a[8] = x2r;
    a[9] = x2i;
    a[10] = x10r;
    a[11] = x10i;
    a[14] = x14r;
    a[15] = x14i;
    a[16] = x1r;
    a[17] = x1i;
    a[20] = x5r;
    a[21] = x5i;
    a[22] = x13r;
    a[23] = x13i;
    a[24] = x3r;
    a[25] = x3i;
    a[26] = x11r;
    a[27] = x11i;
    a[28] = x7r;
    a[29] = x7i;
}


void bitrv216neg(double *a)
{
    double x1r, x1i, x2r, x2i, x3r, x3i, x4r, x4i, 
        x5r, x5i, x6r, x6i, x7r, x7i, x8r, x8i, 
        x9r, x9i, x10r, x10i, x11r, x11i, x12r, x12i, 
        x13r, x13i, x14r, x14i, x15r, x15i;
    
    x1r = a[2];
    x1i = a[3];
    x2r = a[4];
    x2i = a[5];
    x3r = a[6];
    x3i = a[7];
    x4r = a[8];
    x4i = a[9];
    x5r = a[10];
    x5i = a[11];
    x6r = a[12];
    x6i = a[13];
    x7r = a[14];
    x7i = a[15];
    x8r = a[16];
    x8i = a[17];
    x9r = a[18];
    x9i = a[19];
    x10r = a[20];
    x10i = a[21];
    x11r = a[22];
    x11i = a[23];
    x12r = a[24];
    x12i = a[25];
    x13r = a[26];
    x13i = a[27];
    x14r = a[28];
    x14i = a[29];
    x15r = a[30];
    x15i = a[31];
    a[2] = x15r;
    a[3] = x15i;
    a[4] = x7r;
    a[5] = x7i;
    a[6] = x11r;
    a[7] = x11i;
    a[8] = x3r;
    a[9] = x3i;
    a[10] = x13r;
    a[11] = x13i;
    a[12] = x5r;
    a[13] = x5i;
    a[14] = x9r;
    a[15] = x9i;
    a[16] = x1r;
    a[17] = x1i;
    a[18] = x14r;
    a[19] = x14i;
    a[20] = x6r;
    a[21] = x6i;
    a[22] = x10r;
    a[23] = x10i;
    a[24] = x2r;
    a[25] = x2i;
    a[26] = x12r;
    a[27] = x12i;
    a[28] = x4r;
    a[29] = x4i;
    a[30] = x8r;
    a[31] = x8i;
}


void bitrv208(double *a)
{
    double x1r, x1i, x3r, x3i, x4r, x4i, x6r, x6i;
    
    x1r = a[2];
    x1i = a[3];
    x3r = a[6];
    x3i = a[7];
    x4r = a[8];
    x4i = a[9];
    x6r = a[12];
    x6i = a[13];
    a[2] = x4r;
    a[3] = x4i;
    a[6] = x6r;
    a[7] = x6i;
    a[8] = x1r;
    a[9] = x1i;
    a[12] = x3r;
    a[13] = x3i;
}


void bitrv208neg(double *a)
{
    double x1r, x1i, x2r, x2i, x3r, x3i, x4r, x4i, 
        x5r, x5i, x6r, x6i, x7r, x7i;
    
    x1r = a[2];
    x1i = a[3];
    x2r = a[4];
    x2i = a[5];
    x3r = a[6];
    x3i = a[7];
    x4r = a[8];
    x4i = a[9];
    x5r = a[10];
    x5i = a[11];
    x6r = a[12];
    x6i = a[13];
    x7r = a[14];
    x7i = a[15];
    a[2] = x7r;
    a[3] = x7i;
    a[4] = x3r;
    a[5] = x3i;
    a[6] = x5r;
    a[7] = x5i;
    a[8] = x1r;
    a[9] = x1i;
    a[10] = x6r;
    a[11] = x6i;
    a[12] = x2r;
    a[13] = x2i;
    a[14] = x4r;
    a[15] = x4i;
}


void bitrv1(int n, double *a)
{
    int j0, k0, j1, k1, l, m, i, j, k, nh;
    double x;
    
    nh = n >> 1;
    x = a[1];
    a[1] = a[nh];
    a[nh] = x;
    m = 2;
    for (l = n >> 2; l > 2; l >>= 2) {
        m <<= 1;
    }
    if (l == 2) {
        j1 = m + 1;
        k1 = m + nh;
        x = a[j1];
        a[j1] = a[k1];
        a[k1] = x;
        j0 = 0;
        for (k0 = 2; k0 < m; k0 += 2) {
            for (i = nh >> 1; i > (j0 ^= i); i >>= 1);
            k = k0;
            for (j = j0; j < j0 + k0; j += 2) {
                x = a[j];
                a[j] = a[k];
                a[k] = x;
                j1 = j + m;
                k1 = k + m;
                x = a[j1];
                a[j1] = a[k1];
                a[k1] = x;
                j1 += nh;
                k1++;
                x = a[j1];
                a[j1] = a[k1];
                a[k1] = x;
                j1 -= m;
                k1 -= m;
                x = a[j1];
                a[j1] = a[k1];
                a[k1] = x;
                j1++;
                k1 += nh;
                x = a[j1];
                a[j1] = a[k1];
                a[k1] = x;
                j1 += m;
                k1 += m;
                x = a[j1];
                a[j1] = a[k1];
                a[k1] = x;
                j1 -= nh;
                k1--;
                x = a[j1];
                a[j1] = a[k1];
                a[k1] = x;
                j1 -= m;
                k1 -= m;
                x = a[j1];
                a[j1] = a[k1];
                a[k1] = x;
                for (i = nh >> 1; i > (k ^= i); i >>= 1);
            }
            k1 = j0 + k0;
            j1 = k1 + 1;
            k1 += nh;
            x = a[j1];
            a[j1] = a[k1];
            a[k1] = x;
            j1 += m;
            k1 += m;
            x = a[j1];
            a[j1] = a[k1];
            a[k1] = x;
        }
    } else {
        j0 = 0;
        for (k0 = 2; k0 < m; k0 += 2) {
            for (i = nh >> 1; i > (j0 ^= i); i >>= 1);
            k = k0;
            for (j = j0; j < j0 + k0; j += 2) {
                x = a[j];
                a[j] = a[k];
                a[k] = x;
                j1 = j + nh;
                k1 = k + 1;
                x = a[j1];
                a[j1] = a[k1];
                a[k1] = x;
                j1++;
                k1 += nh;
                x = a[j1];
                a[j1] = a[k1];
                a[k1] = x;
                j1 -= nh;
                k1--;
                x = a[j1];
                a[j1] = a[k1];
                a[k1] = x;
                for (i = nh >> 1; i > (k ^= i); i >>= 1);
            }
            k1 = j0 + k0;
            j1 = k1 + 1;
            k1 += nh;
            x = a[j1];
            a[j1] = a[k1];
            a[k1] = x;
        }
    }
}


void cftb1st(int n, double *a)
{
    int i, i0, j, j0, j1, j2, j3, m, mh;
    double ew, w1r, w1i, wk1r, wk1i, wk3r, wk3i, 
        wd1r, wd1i, wd3r, wd3i, ss1, ss3;
    double x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;
    
    mh = n >> 3;
    m = 2 * mh;
    j1 = m;
    j2 = j1 + m;
    j3 = j2 + m;
    x0r = a[0] + a[j2];
    x0i = -a[1] - a[j2 + 1];
    x1r = a[0] - a[j2];
    x1i = -a[1] + a[j2 + 1];
    x2r = a[j1] + a[j3];
    x2i = a[j1 + 1] + a[j3 + 1];
    x3r = a[j1] - a[j3];
    x3i = a[j1 + 1] - a[j3 + 1];
    a[0] = x0r + x2r;
    a[1] = x0i - x2i;
    a[j1] = x0r - x2r;
    a[j1 + 1] = x0i + x2i;
    a[j2] = x1r + x3i;
    a[j2 + 1] = x1i + x3r;
    a[j3] = x1r - x3i;
    a[j3 + 1] = x1i - x3r;
    wd1r = 1;
    wd1i = 0;
    wd3r = 1;
    wd3i = 0;
    ew = M_PI_2 / m;
    w1r = cos(2 * ew);
    w1i = sin(2 * ew);
    wk1r = w1r;
    wk1i = w1i;
    ss1 = 2 * w1i;
    wk3i = 2 * ss1 * wk1r;
    wk3r = wk1r - wk3i * wk1i;
    wk3i = wk1i - wk3i * wk1r;
    ss3 = 2 * wk3i;
    i = 0;
    for (;;) {
        i0 = i + 4 * CDFT_LOOP_DIV;
        if (i0 > mh - 4) {
            i0 = mh - 4;
        }
        for (j = i + 2; j < i0; j += 4) {
            wd1r -= ss1 * wk1i;
            wd1i += ss1 * wk1r;
            wd3r -= ss3 * wk3i;
            wd3i += ss3 * wk3r;
            j1 = j + m;
            j2 = j1 + m;
            j3 = j2 + m;
            x0r = a[j] + a[j2];
            x0i = -a[j + 1] - a[j2 + 1];
            x1r = a[j] - a[j2];
            x1i = -a[j + 1] + a[j2 + 1];
            x2r = a[j1] + a[j3];
            x2i = a[j1 + 1] + a[j3 + 1];
            x3r = a[j1] - a[j3];
            x3i = a[j1 + 1] - a[j3 + 1];
            a[j] = x0r + x2r;
            a[j + 1] = x0i - x2i;
            a[j1] = x0r - x2r;
            a[j1 + 1] = x0i + x2i;
            x0r = x1r + x3i;
            x0i = x1i + x3r;
            a[j2] = wk1r * x0r - wk1i * x0i;
            a[j2 + 1] = wk1r * x0i + wk1i * x0r;
            x0r = x1r - x3i;
            x0i = x1i - x3r;
            a[j3] = wk3r * x0r + wk3i * x0i;
            a[j3 + 1] = wk3r * x0i - wk3i * x0r;
            x0r = a[j + 2] + a[j2 + 2];
            x0i = -a[j + 3] - a[j2 + 3];
            x1r = a[j + 2] - a[j2 + 2];
            x1i = -a[j + 3] + a[j2 + 3];
            x2r = a[j1 + 2] + a[j3 + 2];
            x2i = a[j1 + 3] + a[j3 + 3];
            x3r = a[j1 + 2] - a[j3 + 2];
            x3i = a[j1 + 3] - a[j3 + 3];
            a[j + 2] = x0r + x2r;
            a[j + 3] = x0i - x2i;
            a[j1 + 2] = x0r - x2r;
            a[j1 + 3] = x0i + x2i;
            x0r = x1r + x3i;
            x0i = x1i + x3r;
            a[j2 + 2] = wd1r * x0r - wd1i * x0i;
            a[j2 + 3] = wd1r * x0i + wd1i * x0r;
            x0r = x1r - x3i;
            x0i = x1i - x3r;
            a[j3 + 2] = wd3r * x0r + wd3i * x0i;
            a[j3 + 3] = wd3r * x0i - wd3i * x0r;
            j0 = m - j;
            j1 = j0 + m;
            j2 = j1 + m;
            j3 = j2 + m;
            x0r = a[j0] + a[j2];
            x0i = -a[j0 + 1] - a[j2 + 1];
            x1r = a[j0] - a[j2];
            x1i = -a[j0 + 1] + a[j2 + 1];
            x2r = a[j1] + a[j3];
            x2i = a[j1 + 1] + a[j3 + 1];
            x3r = a[j1] - a[j3];
            x3i = a[j1 + 1] - a[j3 + 1];
            a[j0] = x0r + x2r;
            a[j0 + 1] = x0i - x2i;
            a[j1] = x0r - x2r;
            a[j1 + 1] = x0i + x2i;
            x0r = x1r + x3i;
            x0i = x1i + x3r;
            a[j2] = wk1i * x0r - wk1r * x0i;
            a[j2 + 1] = wk1i * x0i + wk1r * x0r;
            x0r = x1r - x3i;
            x0i = x1i - x3r;
            a[j3] = wk3i * x0r + wk3r * x0i;
            a[j3 + 1] = wk3i * x0i - wk3r * x0r;
            x0r = a[j0 - 2] + a[j2 - 2];
            x0i = -a[j0 - 1] - a[j2 - 1];
            x1r = a[j0 - 2] - a[j2 - 2];
            x1i = -a[j0 - 1] + a[j2 - 1];
            x2r = a[j1 - 2] + a[j3 - 2];
            x2i = a[j1 - 1] + a[j3 - 1];
            x3r = a[j1 - 2] - a[j3 - 2];
            x3i = a[j1 - 1] - a[j3 - 1];
            a[j0 - 2] = x0r + x2r;
            a[j0 - 1] = x0i - x2i;
            a[j1 - 2] = x0r - x2r;
            a[j1 - 1] = x0i + x2i;
            x0r = x1r + x3i;
            x0i = x1i + x3r;
            a[j2 - 2] = wd1i * x0r - wd1r * x0i;
            a[j2 - 1] = wd1i * x0i + wd1r * x0r;
            x0r = x1r - x3i;
            x0i = x1i - x3r;
            a[j3 - 2] = wd3i * x0r + wd3r * x0i;
            a[j3 - 1] = wd3i * x0i - wd3r * x0r;
            wk1r -= ss1 * wd1i;
            wk1i += ss1 * wd1r;
            wk3r -= ss3 * wd3i;
            wk3i += ss3 * wd3r;
        }
        if (i0 == mh - 4) {
            break;
        }
        wd1r = cos(ew * i0);
        wd1i = sin(ew * i0);
        wd3i = 4 * wd1i * wd1r;
        wd3r = wd1r - wd3i * wd1i;
        wd3i = wd1i - wd3i * wd1r;
        wk1r = w1r * wd1r - w1i * wd1i;
        wk1i = w1r * wd1i + w1i * wd1r;
        wk3i = 4 * wk1i * wk1r;
        wk3r = wk1r - wk3i * wk1i;
        wk3i = wk1i - wk3i * wk1r;
        i = i0;
    }
    wd1r = WR5000;
    j0 = mh;
    j1 = j0 + m;
    j2 = j1 + m;
    j3 = j2 + m;
    x0r = a[j0 - 2] + a[j2 - 2];
    x0i = -a[j0 - 1] - a[j2 - 1];
    x1r = a[j0 - 2] - a[j2 - 2];
    x1i = -a[j0 - 1] + a[j2 - 1];
    x2r = a[j1 - 2] + a[j3 - 2];
    x2i = a[j1 - 1] + a[j3 - 1];
    x3r = a[j1 - 2] - a[j3 - 2];
    x3i = a[j1 - 1] - a[j3 - 1];
    a[j0 - 2] = x0r + x2r;
    a[j0 - 1] = x0i - x2i;
    a[j1 - 2] = x0r - x2r;
    a[j1 - 1] = x0i + x2i;
    x0r = x1r + x3i;
    x0i = x1i + x3r;
    a[j2 - 2] = wk1r * x0r - wk1i * x0i;
    a[j2 - 1] = wk1r * x0i + wk1i * x0r;
    x0r = x1r - x3i;
    x0i = x1i - x3r;
    a[j3 - 2] = wk3r * x0r + wk3i * x0i;
    a[j3 - 1] = wk3r * x0i - wk3i * x0r;
    x0r = a[j0] + a[j2];
    x0i = -a[j0 + 1] - a[j2 + 1];
    x1r = a[j0] - a[j2];
    x1i = -a[j0 + 1] + a[j2 + 1];
    x2r = a[j1] + a[j3];
    x2i = a[j1 + 1] + a[j3 + 1];
    x3r = a[j1] - a[j3];
    x3i = a[j1 + 1] - a[j3 + 1];
    a[j0] = x0r + x2r;
    a[j0 + 1] = x0i - x2i;
    a[j1] = x0r - x2r;
    a[j1 + 1] = x0i + x2i;
    x0r = x1r + x3i;
    x0i = x1i + x3r;
    a[j2] = wd1r * (x0r - x0i);
    a[j2 + 1] = wd1r * (x0i + x0r);
    x0r = x1r - x3i;
    x0i = x1i - x3r;
    a[j3] = -wd1r * (x0r + x0i);
    a[j3 + 1] = -wd1r * (x0i - x0r);
    x0r = a[j0 + 2] + a[j2 + 2];
    x0i = -a[j0 + 3] - a[j2 + 3];
    x1r = a[j0 + 2] - a[j2 + 2];
    x1i = -a[j0 + 3] + a[j2 + 3];
    x2r = a[j1 + 2] + a[j3 + 2];
    x2i = a[j1 + 3] + a[j3 + 3];
    x3r = a[j1 + 2] - a[j3 + 2];
    x3i = a[j1 + 3] - a[j3 + 3];
    a[j0 + 2] = x0r + x2r;
    a[j0 + 3] = x0i - x2i;
    a[j1 + 2] = x0r - x2r;
    a[j1 + 3] = x0i + x2i;
    x0r = x1r + x3i;
    x0i = x1i + x3r;
    a[j2 + 2] = wk1i * x0r - wk1r * x0i;
    a[j2 + 3] = wk1i * x0i + wk1r * x0r;
    x0r = x1r - x3i;
    x0i = x1i - x3r;
    a[j3 + 2] = wk3i * x0r + wk3r * x0i;
    a[j3 + 3] = wk3i * x0i - wk3r * x0r;
}


#ifdef USE_CDFT_THREADS
struct cdft_arg_st {
    int n0;
    int n;
    double *a;
};
typedef struct cdft_arg_st cdft_arg_t;


void cftrec4_th(int n, double *a)
{
    void *cftrec1_th(void *p);
    void *cftrec2_th(void *p);
    int i, idiv4, m, nthread;
    cdft_thread_t th[4];
    cdft_arg_t ag[4];
    
    nthread = 2;
    idiv4 = 0;
    m = n >> 1;
    if (n > CDFT_4THREADS_BEGIN_N) {
        nthread = 4;
        idiv4 = 1;
        m >>= 1;
    }
    for (i = 0; i < nthread; i++) {
        ag[i].n0 = n;
        ag[i].n = m;
        ag[i].a = &a[i * m];
        if (i != idiv4) {
            cdft_thread_create(&th[i], cftrec1_th, &ag[i]);
        } else {
            cdft_thread_create(&th[i], cftrec2_th, &ag[i]);
        }
    }
    for (i = 0; i < nthread; i++) {
        cdft_thread_wait(th[i]);
    }
}


void *cftrec1_th(void *p)
{
    int cfttree(int n, int j, int k, double *a);
    void cftleaf(int n, int isplt, double *a);
    void cftmdl1(int n, double *a);
    int isplt, j, k, m, n, n0;
    double *a;
    
    n0 = ((cdft_arg_t *) p)->n0;
    n = ((cdft_arg_t *) p)->n;
    a = ((cdft_arg_t *) p)->a;
    m = n0;
    while (m > 512) {
        m >>= 2;
        cftmdl1(m, &a[n - m]);
    }
    cftleaf(m, 1, &a[n - m]);
    k = 0;
    for (j = n - m; j > 0; j -= m) {
        k++;
        isplt = cfttree(m, j, k, a);
        cftleaf(m, isplt, &a[j - m]);
    }
    return (void *) 0;
}


void *cftrec2_th(void *p)
{
    int cfttree(int n, int j, int k, double *a);
    void cftleaf(int n, int isplt, double *a);
    void cftmdl2(int n, double *a);
    int isplt, j, k, m, n, n0;
    double *a;
    
    n0 = ((cdft_arg_t *) p)->n0;
    n = ((cdft_arg_t *) p)->n;
    a = ((cdft_arg_t *) p)->a;
    k = 1;
    m = n0;
    while (m > 512) {
        m >>= 2;
        k <<= 2;
        cftmdl2(m, &a[n - m]);
    }
    cftleaf(m, 0, &a[n - m]);
    k >>= 1;
    for (j = n - m; j > 0; j -= m) {
        k++;
        isplt = cfttree(m, j, k, a);
        cftleaf(m, isplt, &a[j - m]);
    }
    return (void *) 0;
}
#endif /* USE_CDFT_THREADS */


void cftrec4(int n, double *a)
{
    int cfttree(int n, int j, int k, double *a);
    void cftleaf(int n, int isplt, double *a);
    void cftmdl1(int n, double *a);
    int isplt, j, k, m;
    
    m = n;
    while (m > 512) {
        m >>= 2;
        cftmdl1(m, &a[n - m]);
    }
    cftleaf(m, 1, &a[n - m]);
    k = 0;
    for (j = n - m; j > 0; j -= m) {
        k++;
        isplt = cfttree(m, j, k, a);
        cftleaf(m, isplt, &a[j - m]);
    }
}


int cfttree(int n, int j, int k, double *a)
{
    void cftmdl1(int n, double *a);
    void cftmdl2(int n, double *a);
    int i, isplt, m;
    
    if ((k & 3) != 0) {
        isplt = k & 1;
        if (isplt != 0) {
            cftmdl1(n, &a[j - n]);
        } else {
            cftmdl2(n, &a[j - n]);
        }
    } else {
        m = n;
        for (i = k; (i & 3) == 0; i >>= 2) {
            m <<= 2;
        }
        isplt = i & 1;
        if (isplt != 0) {
            while (m > 128) {
                cftmdl1(m, &a[j - m]);
                m >>= 2;
            }
        } else {
            while (m > 128) {
                cftmdl2(m, &a[j - m]);
                m >>= 2;
            }
        }
    }
    return isplt;
}


void cftleaf(int n, int isplt, double *a)
{
    void cftmdl1(int n, double *a);
    void cftmdl2(int n, double *a);
    void cftf161(double *a);
    void cftf162(double *a);
    void cftf081(double *a);
    void cftf082(double *a);
    
    if (n == 512) {
        cftmdl1(128, a);
        cftf161(a);
        cftf162(&a[32]);
        cftf161(&a[64]);
        cftf161(&a[96]);
        cftmdl2(128, &a[128]);
        cftf161(&a[128]);
        cftf162(&a[160]);
        cftf161(&a[192]);
        cftf162(&a[224]);
        cftmdl1(128, &a[256]);
        cftf161(&a[256]);
        cftf162(&a[288]);
        cftf161(&a[320]);
        cftf161(&a[352]);
        if (isplt != 0) {
            cftmdl1(128, &a[384]);
            cftf161(&a[480]);
        } else {
            cftmdl2(128, &a[384]);
            cftf162(&a[480]);
        }
        cftf161(&a[384]);
        cftf162(&a[416]);
        cftf161(&a[448]);
    } else {
        cftmdl1(64, a);
        cftf081(a);
        cftf082(&a[16]);
        cftf081(&a[32]);
        cftf081(&a[48]);
        cftmdl2(64, &a[64]);
        cftf081(&a[64]);
        cftf082(&a[80]);
        cftf081(&a[96]);
        cftf082(&a[112]);
        cftmdl1(64, &a[128]);
        cftf081(&a[128]);
        cftf082(&a[144]);
        cftf081(&a[160]);
        cftf081(&a[176]);
        if (isplt != 0) {
            cftmdl1(64, &a[192]);
            cftf081(&a[240]);
        } else {
            cftmdl2(64, &a[192]);
            cftf082(&a[240]);
        }
        cftf081(&a[192]);
        cftf082(&a[208]);
        cftf081(&a[224]);
    }
}


void cftmdl1(int n, double *a)
{
    int i, i0, j, j0, j1, j2, j3, m, mh;
    double ew, w1r, w1i, wk1r, wk1i, wk3r, wk3i, 
        wd1r, wd1i, wd3r, wd3i, ss1, ss3;
    double x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;
    
    mh = n >> 3;
    m = 2 * mh;
    j1 = m;
    j2 = j1 + m;
    j3 = j2 + m;
    x0r = a[0] + a[j2];
    x0i = a[1] + a[j2 + 1];
    x1r = a[0] - a[j2];
    x1i = a[1] - a[j2 + 1];
    x2r = a[j1] + a[j3];
    x2i = a[j1 + 1] + a[j3 + 1];
    x3r = a[j1] - a[j3];
    x3i = a[j1 + 1] - a[j3 + 1];
    a[0] = x0r + x2r;
    a[1] = x0i + x2i;
    a[j1] = x0r - x2r;
    a[j1 + 1] = x0i - x2i;
    a[j2] = x1r - x3i;
    a[j2 + 1] = x1i + x3r;
    a[j3] = x1r + x3i;
    a[j3 + 1] = x1i - x3r;
    wd1r = 1;
    wd1i = 0;
    wd3r = 1;
    wd3i = 0;
    ew = M_PI_2 / m;
    w1r = cos(2 * ew);
    w1i = sin(2 * ew);
    wk1r = w1r;
    wk1i = w1i;
    ss1 = 2 * w1i;
    wk3i = 2 * ss1 * wk1r;
    wk3r = wk1r - wk3i * wk1i;
    wk3i = wk1i - wk3i * wk1r;
    ss3 = 2 * wk3i;
    i = 0;
    for (;;) {
        i0 = i + 4 * CDFT_LOOP_DIV;
        if (i0 > mh - 4) {
            i0 = mh - 4;
        }
        for (j = i + 2; j < i0; j += 4) {
            wd1r -= ss1 * wk1i;
            wd1i += ss1 * wk1r;
            wd3r -= ss3 * wk3i;
            wd3i += ss3 * wk3r;
            j1 = j + m;
            j2 = j1 + m;
            j3 = j2 + m;
            x0r = a[j] + a[j2];
            x0i = a[j + 1] + a[j2 + 1];
            x1r = a[j] - a[j2];
            x1i = a[j + 1] - a[j2 + 1];
            x2r = a[j1] + a[j3];
            x2i = a[j1 + 1] + a[j3 + 1];
            x3r = a[j1] - a[j3];
            x3i = a[j1 + 1] - a[j3 + 1];
            a[j] = x0r + x2r;
            a[j + 1] = x0i + x2i;
            a[j1] = x0r - x2r;
            a[j1 + 1] = x0i - x2i;
            x0r = x1r - x3i;
            x0i = x1i + x3r;
            a[j2] = wk1r * x0r - wk1i * x0i;
            a[j2 + 1] = wk1r * x0i + wk1i * x0r;
            x0r = x1r + x3i;
            x0i = x1i - x3r;
            a[j3] = wk3r * x0r + wk3i * x0i;
            a[j3 + 1] = wk3r * x0i - wk3i * x0r;
            x0r = a[j + 2] + a[j2 + 2];
            x0i = a[j + 3] + a[j2 + 3];
            x1r = a[j + 2] - a[j2 + 2];
            x1i = a[j + 3] - a[j2 + 3];
            x2r = a[j1 + 2] + a[j3 + 2];
            x2i = a[j1 + 3] + a[j3 + 3];
            x3r = a[j1 + 2] - a[j3 + 2];
            x3i = a[j1 + 3] - a[j3 + 3];
            a[j + 2] = x0r + x2r;
            a[j + 3] = x0i + x2i;
            a[j1 + 2] = x0r - x2r;
            a[j1 + 3] = x0i - x2i;
            x0r = x1r - x3i;
            x0i = x1i + x3r;
            a[j2 + 2] = wd1r * x0r - wd1i * x0i;
            a[j2 + 3] = wd1r * x0i + wd1i * x0r;
            x0r = x1r + x3i;
            x0i = x1i - x3r;
            a[j3 + 2] = wd3r * x0r + wd3i * x0i;
            a[j3 + 3] = wd3r * x0i - wd3i * x0r;
            j0 = m - j;
            j1 = j0 + m;
            j2 = j1 + m;
            j3 = j2 + m;
            x0r = a[j0] + a[j2];
            x0i = a[j0 + 1] + a[j2 + 1];
            x1r = a[j0] - a[j2];
            x1i = a[j0 + 1] - a[j2 + 1];
            x2r = a[j1] + a[j3];
            x2i = a[j1 + 1] + a[j3 + 1];
            x3r = a[j1] - a[j3];
            x3i = a[j1 + 1] - a[j3 + 1];
            a[j0] = x0r + x2r;
            a[j0 + 1] = x0i + x2i;
            a[j1] = x0r - x2r;
            a[j1 + 1] = x0i - x2i;
            x0r = x1r - x3i;
            x0i = x1i + x3r;
            a[j2] = wk1i * x0r - wk1r * x0i;
            a[j2 + 1] = wk1i * x0i + wk1r * x0r;
            x0r = x1r + x3i;
            x0i = x1i - x3r;
            a[j3] = wk3i * x0r + wk3r * x0i;
            a[j3 + 1] = wk3i * x0i - wk3r * x0r;
            x0r = a[j0 - 2] + a[j2 - 2];
            x0i = a[j0 - 1] + a[j2 - 1];
            x1r = a[j0 - 2] - a[j2 - 2];
            x1i = a[j0 - 1] - a[j2 - 1];
            x2r = a[j1 - 2] + a[j3 - 2];
            x2i = a[j1 - 1] + a[j3 - 1];
            x3r = a[j1 - 2] - a[j3 - 2];
            x3i = a[j1 - 1] - a[j3 - 1];
            a[j0 - 2] = x0r + x2r;
            a[j0 - 1] = x0i + x2i;
            a[j1 - 2] = x0r - x2r;
            a[j1 - 1] = x0i - x2i;
            x0r = x1r - x3i;
            x0i = x1i + x3r;
            a[j2 - 2] = wd1i * x0r - wd1r * x0i;
            a[j2 - 1] = wd1i * x0i + wd1r * x0r;
            x0r = x1r + x3i;
            x0i = x1i - x3r;
            a[j3 - 2] = wd3i * x0r + wd3r * x0i;
            a[j3 - 1] = wd3i * x0i - wd3r * x0r;
            wk1r -= ss1 * wd1i;
            wk1i += ss1 * wd1r;
            wk3r -= ss3 * wd3i;
            wk3i += ss3 * wd3r;
        }
        if (i0 == mh - 4) {
            break;
        }
        wd1r = cos(ew * i0);
        wd1i = sin(ew * i0);
        wd3i = 4 * wd1i * wd1r;
        wd3r = wd1r - wd3i * wd1i;
        wd3i = wd1i - wd3i * wd1r;
        wk1r = w1r * wd1r - w1i * wd1i;
        wk1i = w1r * wd1i + w1i * wd1r;
        wk3i = 4 * wk1i * wk1r;
        wk3r = wk1r - wk3i * wk1i;
        wk3i = wk1i - wk3i * wk1r;
        i = i0;
    }
    wd1r = WR5000;
    j0 = mh;
    j1 = j0 + m;
    j2 = j1 + m;
    j3 = j2 + m;
    x0r = a[j0 - 2] + a[j2 - 2];
    x0i = a[j0 - 1] + a[j2 - 1];
    x1r = a[j0 - 2] - a[j2 - 2];
    x1i = a[j0 - 1] - a[j2 - 1];
    x2r = a[j1 - 2] + a[j3 - 2];
    x2i = a[j1 - 1] + a[j3 - 1];
    x3r = a[j1 - 2] - a[j3 - 2];
    x3i = a[j1 - 1] - a[j3 - 1];
    a[j0 - 2] = x0r + x2r;
    a[j0 - 1] = x0i + x2i;
    a[j1 - 2] = x0r - x2r;
    a[j1 - 1] = x0i - x2i;
    x0r = x1r - x3i;
    x0i = x1i + x3r;
    a[j2 - 2] = wk1r * x0r - wk1i * x0i;
    a[j2 - 1] = wk1r * x0i + wk1i * x0r;
    x0r = x1r + x3i;
    x0i = x1i - x3r;
    a[j3 - 2] = wk3r * x0r + wk3i * x0i;
    a[j3 - 1] = wk3r * x0i - wk3i * x0r;
    x0r = a[j0] + a[j2];
    x0i = a[j0 + 1] + a[j2 + 1];
    x1r = a[j0] - a[j2];
    x1i = a[j0 + 1] - a[j2 + 1];
    x2r = a[j1] + a[j3];
    x2i = a[j1 + 1] + a[j3 + 1];
    x3r = a[j1] - a[j3];
    x3i = a[j1 + 1] - a[j3 + 1];
    a[j0] = x0r + x2r;
    a[j0 + 1] = x0i + x2i;
    a[j1] = x0r - x2r;
    a[j1 + 1] = x0i - x2i;
    x0r = x1r - x3i;
    x0i = x1i + x3r;
    a[j2] = wd1r * (x0r - x0i);
    a[j2 + 1] = wd1r * (x0i + x0r);
    x0r = x1r + x3i;
    x0i = x1i - x3r;
    a[j3] = -wd1r * (x0r + x0i);
    a[j3 + 1] = -wd1r * (x0i - x0r);
    x0r = a[j0 + 2] + a[j2 + 2];
    x0i = a[j0 + 3] + a[j2 + 3];
    x1r = a[j0 + 2] - a[j2 + 2];
    x1i = a[j0 + 3] - a[j2 + 3];
    x2r = a[j1 + 2] + a[j3 + 2];
    x2i = a[j1 + 3] + a[j3 + 3];
    x3r = a[j1 + 2] - a[j3 + 2];
    x3i = a[j1 + 3] - a[j3 + 3];
    a[j0 + 2] = x0r + x2r;
    a[j0 + 3] = x0i + x2i;
    a[j1 + 2] = x0r - x2r;
    a[j1 + 3] = x0i - x2i;
    x0r = x1r - x3i;
    x0i = x1i + x3r;
    a[j2 + 2] = wk1i * x0r - wk1r * x0i;
    a[j2 + 3] = wk1i * x0i + wk1r * x0r;
    x0r = x1r + x3i;
    x0i = x1i - x3r;
    a[j3 + 2] = wk3i * x0r + wk3r * x0i;
    a[j3 + 3] = wk3i * x0i - wk3r * x0r;
}


void cftmdl2(int n, double *a)
{
    int i, i0, j, j0, j1, j2, j3, m, mh;
    double ew, w1r, w1i, wn4r, wk1r, wk1i, wk3r, wk3i, 
        wl1r, wl1i, wl3r, wl3i, wd1r, wd1i, wd3r, wd3i, 
        we1r, we1i, we3r, we3i, ss1, ss3;
    double x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i, y0r, y0i, y2r, y2i;
    
    mh = n >> 3;
    m = 2 * mh;
    wn4r = WR5000;
    j1 = m;
    j2 = j1 + m;
    j3 = j2 + m;
    x0r = a[0] - a[j2 + 1];
    x0i = a[1] + a[j2];
    x1r = a[0] + a[j2 + 1];
    x1i = a[1] - a[j2];
    x2r = a[j1] - a[j3 + 1];
    x2i = a[j1 + 1] + a[j3];
    x3r = a[j1] + a[j3 + 1];
    x3i = a[j1 + 1] - a[j3];
    y0r = wn4r * (x2r - x2i);
    y0i = wn4r * (x2i + x2r);
    a[0] = x0r + y0r;
    a[1] = x0i + y0i;
    a[j1] = x0r - y0r;
    a[j1 + 1] = x0i - y0i;
    y0r = wn4r * (x3r - x3i);
    y0i = wn4r * (x3i + x3r);
    a[j2] = x1r - y0i;
    a[j2 + 1] = x1i + y0r;
    a[j3] = x1r + y0i;
    a[j3 + 1] = x1i - y0r;
    wl1r = 1;
    wl1i = 0;
    wl3r = 1;
    wl3i = 0;
    we1r = wn4r;
    we1i = wn4r;
    we3r = -wn4r;
    we3i = -wn4r;
    ew = M_PI_2 / (2 * m);
    w1r = cos(2 * ew);
    w1i = sin(2 * ew);
    wk1r = w1r;
    wk1i = w1i;
    wd1r = wn4r * (w1r - w1i);
    wd1i = wn4r * (w1i + w1r);
    ss1 = 2 * w1i;
    wk3i = 2 * ss1 * wk1r;
    wk3r = wk1r - wk3i * wk1i;
    wk3i = wk1i - wk3i * wk1r;
    ss3 = 2 * wk3i;
    wd3r = -wn4r * (wk3r - wk3i);
    wd3i = -wn4r * (wk3i + wk3r);
    i = 0;
    for (;;) {
        i0 = i + 4 * CDFT_LOOP_DIV;
        if (i0 > mh - 4) {
            i0 = mh - 4;
        }
        for (j = i + 2; j < i0; j += 4) {
            wl1r -= ss1 * wk1i;
            wl1i += ss1 * wk1r;
            wl3r -= ss3 * wk3i;
            wl3i += ss3 * wk3r;
            we1r -= ss1 * wd1i;
            we1i += ss1 * wd1r;
            we3r -= ss3 * wd3i;
            we3i += ss3 * wd3r;
            j1 = j + m;
            j2 = j1 + m;
            j3 = j2 + m;
            x0r = a[j] - a[j2 + 1];
            x0i = a[j + 1] + a[j2];
            x1r = a[j] + a[j2 + 1];
            x1i = a[j + 1] - a[j2];
            x2r = a[j1] - a[j3 + 1];
            x2i = a[j1 + 1] + a[j3];
            x3r = a[j1] + a[j3 + 1];
            x3i = a[j1 + 1] - a[j3];
            y0r = wk1r * x0r - wk1i * x0i;
            y0i = wk1r * x0i + wk1i * x0r;
            y2r = wd1r * x2r - wd1i * x2i;
            y2i = wd1r * x2i + wd1i * x2r;
            a[j] = y0r + y2r;
            a[j + 1] = y0i + y2i;
            a[j1] = y0r - y2r;
            a[j1 + 1] = y0i - y2i;
            y0r = wk3r * x1r + wk3i * x1i;
            y0i = wk3r * x1i - wk3i * x1r;
            y2r = wd3r * x3r + wd3i * x3i;
            y2i = wd3r * x3i - wd3i * x3r;
            a[j2] = y0r + y2r;
            a[j2 + 1] = y0i + y2i;
            a[j3] = y0r - y2r;
            a[j3 + 1] = y0i - y2i;
            x0r = a[j + 2] - a[j2 + 3];
            x0i = a[j + 3] + a[j2 + 2];
            x1r = a[j + 2] + a[j2 + 3];
            x1i = a[j + 3] - a[j2 + 2];
            x2r = a[j1 + 2] - a[j3 + 3];
            x2i = a[j1 + 3] + a[j3 + 2];
            x3r = a[j1 + 2] + a[j3 + 3];
            x3i = a[j1 + 3] - a[j3 + 2];
            y0r = wl1r * x0r - wl1i * x0i;
            y0i = wl1r * x0i + wl1i * x0r;
            y2r = we1r * x2r - we1i * x2i;
            y2i = we1r * x2i + we1i * x2r;
            a[j + 2] = y0r + y2r;
            a[j + 3] = y0i + y2i;
            a[j1 + 2] = y0r - y2r;
            a[j1 + 3] = y0i - y2i;
            y0r = wl3r * x1r + wl3i * x1i;
            y0i = wl3r * x1i - wl3i * x1r;
            y2r = we3r * x3r + we3i * x3i;
            y2i = we3r * x3i - we3i * x3r;
            a[j2 + 2] = y0r + y2r;
            a[j2 + 3] = y0i + y2i;
            a[j3 + 2] = y0r - y2r;
            a[j3 + 3] = y0i - y2i;
            j0 = m - j;
            j1 = j0 + m;
            j2 = j1 + m;
            j3 = j2 + m;
            x0r = a[j0] - a[j2 + 1];
            x0i = a[j0 + 1] + a[j2];
            x1r = a[j0] + a[j2 + 1];
            x1i = a[j0 + 1] - a[j2];
            x2r = a[j1] - a[j3 + 1];
            x2i = a[j1 + 1] + a[j3];
            x3r = a[j1] + a[j3 + 1];
            x3i = a[j1 + 1] - a[j3];
            y0r = wd1i * x0r - wd1r * x0i;
            y0i = wd1i * x0i + wd1r * x0r;
            y2r = wk1i * x2r - wk1r * x2i;
            y2i = wk1i * x2i + wk1r * x2r;
            a[j0] = y0r + y2r;
            a[j0 + 1] = y0i + y2i;
            a[j1] = y0r - y2r;
            a[j1 + 1] = y0i - y2i;
            y0r = wd3i * x1r + wd3r * x1i;
            y0i = wd3i * x1i - wd3r * x1r;
            y2r = wk3i * x3r + wk3r * x3i;
            y2i = wk3i * x3i - wk3r * x3r;
            a[j2] = y0r + y2r;
            a[j2 + 1] = y0i + y2i;
            a[j3] = y0r - y2r;
            a[j3 + 1] = y0i - y2i;
            x0r = a[j0 - 2] - a[j2 - 1];
            x0i = a[j0 - 1] + a[j2 - 2];
            x1r = a[j0 - 2] + a[j2 - 1];
            x1i = a[j0 - 1] - a[j2 - 2];
            x2r = a[j1 - 2] - a[j3 - 1];
            x2i = a[j1 - 1] + a[j3 - 2];
            x3r = a[j1 - 2] + a[j3 - 1];
            x3i = a[j1 - 1] - a[j3 - 2];
            y0r = we1i * x0r - we1r * x0i;
            y0i = we1i * x0i + we1r * x0r;
            y2r = wl1i * x2r - wl1r * x2i;
            y2i = wl1i * x2i + wl1r * x2r;
            a[j0 - 2] = y0r + y2r;
            a[j0 - 1] = y0i + y2i;
            a[j1 - 2] = y0r - y2r;
            a[j1 - 1] = y0i - y2i;
            y0r = we3i * x1r + we3r * x1i;
            y0i = we3i * x1i - we3r * x1r;
            y2r = wl3i * x3r + wl3r * x3i;
            y2i = wl3i * x3i - wl3r * x3r;
            a[j2 - 2] = y0r + y2r;
            a[j2 - 1] = y0i + y2i;
            a[j3 - 2] = y0r - y2r;
            a[j3 - 1] = y0i - y2i;
            wk1r -= ss1 * wl1i;
            wk1i += ss1 * wl1r;
            wk3r -= ss3 * wl3i;
            wk3i += ss3 * wl3r;
            wd1r -= ss1 * we1i;
            wd1i += ss1 * we1r;
            wd3r -= ss3 * we3i;
            wd3i += ss3 * we3r;
        }
        if (i0 == mh - 4) {
            break;
        }
        wl1r = cos(ew * i0);
        wl1i = sin(ew * i0);
        wl3i = 4 * wl1i * wl1r;
        wl3r = wl1r - wl3i * wl1i;
        wl3i = wl1i - wl3i * wl1r;
        we1r = wn4r * (wl1r - wl1i);
        we1i = wn4r * (wl1i + wl1r);
        we3r = -wn4r * (wl3r - wl3i);
        we3i = -wn4r * (wl3i + wl3r);
        wk1r = w1r * wl1r - w1i * wl1i;
        wk1i = w1r * wl1i + w1i * wl1r;
        wk3i = 4 * wk1i * wk1r;
        wk3r = wk1r - wk3i * wk1i;
        wk3i = wk1i - wk3i * wk1r;
        wd1r = wn4r * (wk1r - wk1i);
        wd1i = wn4r * (wk1i + wk1r);
        wd3r = -wn4r * (wk3r - wk3i);
        wd3i = -wn4r * (wk3i + wk3r);
        i = i0;
    }
    wl1r = WR2500;
    wl1i = WI2500;
    j0 = mh;
    j1 = j0 + m;
    j2 = j1 + m;
    j3 = j2 + m;
    x0r = a[j0 - 2] - a[j2 - 1];
    x0i = a[j0 - 1] + a[j2 - 2];
    x1r = a[j0 - 2] + a[j2 - 1];
    x1i = a[j0 - 1] - a[j2 - 2];
    x2r = a[j1 - 2] - a[j3 - 1];
    x2i = a[j1 - 1] + a[j3 - 2];
    x3r = a[j1 - 2] + a[j3 - 1];
    x3i = a[j1 - 1] - a[j3 - 2];
    y0r = wk1r * x0r - wk1i * x0i;
    y0i = wk1r * x0i + wk1i * x0r;
    y2r = wd1r * x2r - wd1i * x2i;
    y2i = wd1r * x2i + wd1i * x2r;
    a[j0 - 2] = y0r + y2r;
    a[j0 - 1] = y0i + y2i;
    a[j1 - 2] = y0r - y2r;
    a[j1 - 1] = y0i - y2i;
    y0r = wk3r * x1r + wk3i * x1i;
    y0i = wk3r * x1i - wk3i * x1r;
    y2r = wd3r * x3r + wd3i * x3i;
    y2i = wd3r * x3i - wd3i * x3r;
    a[j2 - 2] = y0r + y2r;
    a[j2 - 1] = y0i + y2i;
    a[j3 - 2] = y0r - y2r;
    a[j3 - 1] = y0i - y2i;
    x0r = a[j0] - a[j2 + 1];
    x0i = a[j0 + 1] + a[j2];
    x1r = a[j0] + a[j2 + 1];
    x1i = a[j0 + 1] - a[j2];
    x2r = a[j1] - a[j3 + 1];
    x2i = a[j1 + 1] + a[j3];
    x3r = a[j1] + a[j3 + 1];
    x3i = a[j1 + 1] - a[j3];
    y0r = wl1r * x0r - wl1i * x0i;
    y0i = wl1r * x0i + wl1i * x0r;
    y2r = wl1i * x2r - wl1r * x2i;
    y2i = wl1i * x2i + wl1r * x2r;
    a[j0] = y0r + y2r;
    a[j0 + 1] = y0i + y2i;
    a[j1] = y0r - y2r;
    a[j1 + 1] = y0i - y2i;
    y0r = wl1i * x1r - wl1r * x1i;
    y0i = wl1i * x1i + wl1r * x1r;
    y2r = wl1r * x3r - wl1i * x3i;
    y2i = wl1r * x3i + wl1i * x3r;
    a[j2] = y0r - y2r;
    a[j2 + 1] = y0i - y2i;
    a[j3] = y0r + y2r;
    a[j3 + 1] = y0i + y2i;
    x0r = a[j0 + 2] - a[j2 + 3];
    x0i = a[j0 + 3] + a[j2 + 2];
    x1r = a[j0 + 2] + a[j2 + 3];
    x1i = a[j0 + 3] - a[j2 + 2];
    x2r = a[j1 + 2] - a[j3 + 3];
    x2i = a[j1 + 3] + a[j3 + 2];
    x3r = a[j1 + 2] + a[j3 + 3];
    x3i = a[j1 + 3] - a[j3 + 2];
    y0r = wd1i * x0r - wd1r * x0i;
    y0i = wd1i * x0i + wd1r * x0r;
    y2r = wk1i * x2r - wk1r * x2i;
    y2i = wk1i * x2i + wk1r * x2r;
    a[j0 + 2] = y0r + y2r;
    a[j0 + 3] = y0i + y2i;
    a[j1 + 2] = y0r - y2r;
    a[j1 + 3] = y0i - y2i;
    y0r = wd3i * x1r + wd3r * x1i;
    y0i = wd3i * x1i - wd3r * x1r;
    y2r = wk3i * x3r + wk3r * x3i;
    y2i = wk3i * x3i - wk3r * x3r;
    a[j2 + 2] = y0r + y2r;
    a[j2 + 3] = y0i + y2i;
    a[j3 + 2] = y0r - y2r;
    a[j3 + 3] = y0i - y2i;
}


void cftfx41(int n, double *a)
{
    void cftf161(double *a);
    void cftf162(double *a);
    void cftf081(double *a);
    void cftf082(double *a);
    
    if (n == 128) {
        cftf161(a);
        cftf162(&a[32]);
        cftf161(&a[64]);
        cftf161(&a[96]);
    } else {
        cftf081(a);
        cftf082(&a[16]);
        cftf081(&a[32]);
        cftf081(&a[48]);
    }
}


void cftf161(double *a)
{
    double wn4r, wk1r, wk1i, 
        x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i, 
        y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i, 
        y4r, y4i, y5r, y5i, y6r, y6i, y7r, y7i, 
        y8r, y8i, y9r, y9i, y10r, y10i, y11r, y11i, 
        y12r, y12i, y13r, y13i, y14r, y14i, y15r, y15i;
    
    wn4r = WR5000;
    wk1r = WR2500;
    wk1i = WI2500;
    x0r = a[0] + a[16];
    x0i = a[1] + a[17];
    x1r = a[0] - a[16];
    x1i = a[1] - a[17];
    x2r = a[8] + a[24];
    x2i = a[9] + a[25];
    x3r = a[8] - a[24];
    x3i = a[9] - a[25];
    y0r = x0r + x2r;
    y0i = x0i + x2i;
    y4r = x0r - x2r;
    y4i = x0i - x2i;
    y8r = x1r - x3i;
    y8i = x1i + x3r;
    y12r = x1r + x3i;
    y12i = x1i - x3r;
    x0r = a[2] + a[18];
    x0i = a[3] + a[19];
    x1r = a[2] - a[18];
    x1i = a[3] - a[19];
    x2r = a[10] + a[26];
    x2i = a[11] + a[27];
    x3r = a[10] - a[26];
    x3i = a[11] - a[27];
    y1r = x0r + x2r;
    y1i = x0i + x2i;
    y5r = x0r - x2r;
    y5i = x0i - x2i;
    x0r = x1r - x3i;
    x0i = x1i + x3r;
    y9r = wk1r * x0r - wk1i * x0i;
    y9i = wk1r * x0i + wk1i * x0r;
    x0r = x1r + x3i;
    x0i = x1i - x3r;
    y13r = wk1i * x0r - wk1r * x0i;
    y13i = wk1i * x0i + wk1r * x0r;
    x0r = a[4] + a[20];
    x0i = a[5] + a[21];
    x1r = a[4] - a[20];
    x1i = a[5] - a[21];
    x2r = a[12] + a[28];
    x2i = a[13] + a[29];
    x3r = a[12] - a[28];
    x3i = a[13] - a[29];
    y2r = x0r + x2r;
    y2i = x0i + x2i;
    y6r = x0r - x2r;
    y6i = x0i - x2i;
    x0r = x1r - x3i;
    x0i = x1i + x3r;
    y10r = wn4r * (x0r - x0i);
    y10i = wn4r * (x0i + x0r);
    x0r = x1r + x3i;
    x0i = x1i - x3r;
    y14r = wn4r * (x0r + x0i);
    y14i = wn4r * (x0i - x0r);
    x0r = a[6] + a[22];
    x0i = a[7] + a[23];
    x1r = a[6] - a[22];
    x1i = a[7] - a[23];
    x2r = a[14] + a[30];
    x2i = a[15] + a[31];
    x3r = a[14] - a[30];
    x3i = a[15] - a[31];
    y3r = x0r + x2r;
    y3i = x0i + x2i;
    y7r = x0r - x2r;
    y7i = x0i - x2i;
    x0r = x1r - x3i;
    x0i = x1i + x3r;
    y11r = wk1i * x0r - wk1r * x0i;
    y11i = wk1i * x0i + wk1r * x0r;
    x0r = x1r + x3i;
    x0i = x1i - x3r;
    y15r = wk1r * x0r - wk1i * x0i;
    y15i = wk1r * x0i + wk1i * x0r;
    x0r = y12r - y14r;
    x0i = y12i - y14i;
    x1r = y12r + y14r;
    x1i = y12i + y14i;
    x2r = y13r - y15r;
    x2i = y13i - y15i;
    x3r = y13r + y15r;
    x3i = y13i + y15i;
    a[24] = x0r + x2r;
    a[25] = x0i + x2i;
    a[26] = x0r - x2r;
    a[27] = x0i - x2i;
    a[28] = x1r - x3i;
    a[29] = x1i + x3r;
    a[30] = x1r + x3i;
    a[31] = x1i - x3r;
    x0r = y8r + y10r;
    x0i = y8i + y10i;
    x1r = y8r - y10r;
    x1i = y8i - y10i;
    x2r = y9r + y11r;
    x2i = y9i + y11i;
    x3r = y9r - y11r;
    x3i = y9i - y11i;
    a[16] = x0r + x2r;
    a[17] = x0i + x2i;
    a[18] = x0r - x2r;
    a[19] = x0i - x2i;
    a[20] = x1r - x3i;
    a[21] = x1i + x3r;
    a[22] = x1r + x3i;
    a[23] = x1i - x3r;
    x0r = y5r - y7i;
    x0i = y5i + y7r;
    x2r = wn4r * (x0r - x0i);
    x2i = wn4r * (x0i + x0r);
    x0r = y5r + y7i;
    x0i = y5i - y7r;
    x3r = wn4r * (x0r - x0i);
    x3i = wn4r * (x0i + x0r);
    x0r = y4r - y6i;
    x0i = y4i + y6r;
    x1r = y4r + y6i;
    x1i = y4i - y6r;
    a[8] = x0r + x2r;
    a[9] = x0i + x2i;
    a[10] = x0r - x2r;
    a[11] = x0i - x2i;
    a[12] = x1r - x3i;
    a[13] = x1i + x3r;
    a[14] = x1r + x3i;
    a[15] = x1i - x3r;
    x0r = y0r + y2r;
    x0i = y0i + y2i;
    x1r = y0r - y2r;
    x1i = y0i - y2i;
    x2r = y1r + y3r;
    x2i = y1i + y3i;
    x3r = y1r - y3r;
    x3i = y1i - y3i;
    a[0] = x0r + x2r;
    a[1] = x0i + x2i;
    a[2] = x0r - x2r;
    a[3] = x0i - x2i;
    a[4] = x1r - x3i;
    a[5] = x1i + x3r;
    a[6] = x1r + x3i;
    a[7] = x1i - x3r;
}


void cftf162(double *a)
{
    double wn4r, wk1r, wk1i, wk2r, wk2i, wk3r, wk3i, 
        x0r, x0i, x1r, x1i, x2r, x2i, 
        y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i, 
        y4r, y4i, y5r, y5i, y6r, y6i, y7r, y7i, 
        y8r, y8i, y9r, y9i, y10r, y10i, y11r, y11i, 
        y12r, y12i, y13r, y13i, y14r, y14i, y15r, y15i;
    
    wn4r = WR5000;
    wk1r = WR1250;
    wk1i = WI1250;
    wk2r = WR2500;
    wk2i = WI2500;
    wk3r = WR3750;
    wk3i = WI3750;
    x1r = a[0] - a[17];
    x1i = a[1] + a[16];
    x0r = a[8] - a[25];
    x0i = a[9] + a[24];
    x2r = wn4r * (x0r - x0i);
    x2i = wn4r * (x0i + x0r);
    y0r = x1r + x2r;
    y0i = x1i + x2i;
    y4r = x1r - x2r;
    y4i = x1i - x2i;
    x1r = a[0] + a[17];
    x1i = a[1] - a[16];
    x0r = a[8] + a[25];
    x0i = a[9] - a[24];
    x2r = wn4r * (x0r - x0i);
    x2i = wn4r * (x0i + x0r);
    y8r = x1r - x2i;
    y8i = x1i + x2r;
    y12r = x1r + x2i;
    y12i = x1i - x2r;
    x0r = a[2] - a[19];
    x0i = a[3] + a[18];
    x1r = wk1r * x0r - wk1i * x0i;
    x1i = wk1r * x0i + wk1i * x0r;
    x0r = a[10] - a[27];
    x0i = a[11] + a[26];
    x2r = wk3i * x0r - wk3r * x0i;
    x2i = wk3i * x0i + wk3r * x0r;
    y1r = x1r + x2r;
    y1i = x1i + x2i;
    y5r = x1r - x2r;
    y5i = x1i - x2i;
    x0r = a[2] + a[19];
    x0i = a[3] - a[18];
    x1r = wk3r * x0r - wk3i * x0i;
    x1i = wk3r * x0i + wk3i * x0r;
    x0r = a[10] + a[27];
    x0i = a[11] - a[26];
    x2r = wk1r * x0r + wk1i * x0i;
    x2i = wk1r * x0i - wk1i * x0r;
    y9r = x1r - x2r;
    y9i = x1i - x2i;
    y13r = x1r + x2r;
    y13i = x1i + x2i;
    x0r = a[4] - a[21];
    x0i = a[5] + a[20];
    x1r = wk2r * x0r - wk2i * x0i;
    x1i = wk2r * x0i + wk2i * x0r;
    x0r = a[12] - a[29];
    x0i = a[13] + a[28];
    x2r = wk2i * x0r - wk2r * x0i;
    x2i = wk2i * x0i + wk2r * x0r;
    y2r = x1r + x2r;
    y2i = x1i + x2i;
    y6r = x1r - x2r;
    y6i = x1i - x2i;
    x0r = a[4] + a[21];
    x0i = a[5] - a[20];
    x1r = wk2i * x0r - wk2r * x0i;
    x1i = wk2i * x0i + wk2r * x0r;
    x0r = a[12] + a[29];
    x0i = a[13] - a[28];
    x2r = wk2r * x0r - wk2i * x0i;
    x2i = wk2r * x0i + wk2i * x0r;
    y10r = x1r - x2r;
    y10i = x1i - x2i;
    y14r = x1r + x2r;
    y14i = x1i + x2i;
    x0r = a[6] - a[23];
    x0i = a[7] + a[22];
    x1r = wk3r * x0r - wk3i * x0i;
    x1i = wk3r * x0i + wk3i * x0r;
    x0r = a[14] - a[31];
    x0i = a[15] + a[30];
    x2r = wk1i * x0r - wk1r * x0i;
    x2i = wk1i * x0i + wk1r * x0r;
    y3r = x1r + x2r;
    y3i = x1i + x2i;
    y7r = x1r - x2r;
    y7i = x1i - x2i;
    x0r = a[6] + a[23];
    x0i = a[7] - a[22];
    x1r = wk1i * x0r + wk1r * x0i;
    x1i = wk1i * x0i - wk1r * x0r;
    x0r = a[14] + a[31];
    x0i = a[15] - a[30];
    x2r = wk3i * x0r - wk3r * x0i;
    x2i = wk3i * x0i + wk3r * x0r;
    y11r = x1r + x2r;
    y11i = x1i + x2i;
    y15r = x1r - x2r;
    y15i = x1i - x2i;
    x1r = y0r + y2r;
    x1i = y0i + y2i;
    x2r = y1r + y3r;
    x2i = y1i + y3i;
    a[0] = x1r + x2r;
    a[1] = x1i + x2i;
    a[2] = x1r - x2r;
    a[3] = x1i - x2i;
    x1r = y0r - y2r;
    x1i = y0i - y2i;
    x2r = y1r - y3r;
    x2i = y1i - y3i;
    a[4] = x1r - x2i;
    a[5] = x1i + x2r;
    a[6] = x1r + x2i;
    a[7] = x1i - x2r;
    x1r = y4r - y6i;
    x1i = y4i + y6r;
    x0r = y5r - y7i;
    x0i = y5i + y7r;
    x2r = wn4r * (x0r - x0i);
    x2i = wn4r * (x0i + x0r);
    a[8] = x1r + x2r;
    a[9] = x1i + x2i;
    a[10] = x1r - x2r;
    a[11] = x1i - x2i;
    x1r = y4r + y6i;
    x1i = y4i - y6r;
    x0r = y5r + y7i;
    x0i = y5i - y7r;
    x2r = wn4r * (x0r - x0i);
    x2i = wn4r * (x0i + x0r);
    a[12] = x1r - x2i;
    a[13] = x1i + x2r;
    a[14] = x1r + x2i;
    a[15] = x1i - x2r;
    x1r = y8r + y10r;
    x1i = y8i + y10i;
    x2r = y9r - y11r;
    x2i = y9i - y11i;
    a[16] = x1r + x2r;
    a[17] = x1i + x2i;
    a[18] = x1r - x2r;
    a[19] = x1i - x2i;
    x1r = y8r - y10r;
    x1i = y8i - y10i;
    x2r = y9r + y11r;
    x2i = y9i + y11i;
    a[20] = x1r - x2i;
    a[21] = x1i + x2r;
    a[22] = x1r + x2i;
    a[23] = x1i - x2r;
    x1r = y12r - y14i;
    x1i = y12i + y14r;
    x0r = y13r + y15i;
    x0i = y13i - y15r;
    x2r = wn4r * (x0r - x0i);
    x2i = wn4r * (x0i + x0r);
    a[24] = x1r + x2r;
    a[25] = x1i + x2i;
    a[26] = x1r - x2r;
    a[27] = x1i - x2i;
    x1r = y12r + y14i;
    x1i = y12i - y14r;
    x0r = y13r - y15i;
    x0i = y13i + y15r;
    x2r = wn4r * (x0r - x0i);
    x2i = wn4r * (x0i + x0r);
    a[28] = x1r - x2i;
    a[29] = x1i + x2r;
    a[30] = x1r + x2i;
    a[31] = x1i - x2r;
}


void cftf081(double *a)
{
    double wn4r, x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i, 
        y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i, 
        y4r, y4i, y5r, y5i, y6r, y6i, y7r, y7i;
    
    wn4r = WR5000;
    x0r = a[0] + a[8];
    x0i = a[1] + a[9];
    x1r = a[0] - a[8];
    x1i = a[1] - a[9];
    x2r = a[4] + a[12];
    x2i = a[5] + a[13];
    x3r = a[4] - a[12];
    x3i = a[5] - a[13];
    y0r = x0r + x2r;
    y0i = x0i + x2i;
    y2r = x0r - x2r;
    y2i = x0i - x2i;
    y1r = x1r - x3i;
    y1i = x1i + x3r;
    y3r = x1r + x3i;
    y3i = x1i - x3r;
    x0r = a[2] + a[10];
    x0i = a[3] + a[11];
    x1r = a[2] - a[10];
    x1i = a[3] - a[11];
    x2r = a[6] + a[14];
    x2i = a[7] + a[15];
    x3r = a[6] - a[14];
    x3i = a[7] - a[15];
    y4r = x0r + x2r;
    y4i = x0i + x2i;
    y6r = x0r - x2r;
    y6i = x0i - x2i;
    x0r = x1r - x3i;
    x0i = x1i + x3r;
    x2r = x1r + x3i;
    x2i = x1i - x3r;
    y5r = wn4r * (x0r - x0i);
    y5i = wn4r * (x0r + x0i);
    y7r = wn4r * (x2r - x2i);
    y7i = wn4r * (x2r + x2i);
    a[8] = y1r + y5r;
    a[9] = y1i + y5i;
    a[10] = y1r - y5r;
    a[11] = y1i - y5i;
    a[12] = y3r - y7i;
    a[13] = y3i + y7r;
    a[14] = y3r + y7i;
    a[15] = y3i - y7r;
    a[0] = y0r + y4r;
    a[1] = y0i + y4i;
    a[2] = y0r - y4r;
    a[3] = y0i - y4i;
    a[4] = y2r - y6i;
    a[5] = y2i + y6r;
    a[6] = y2r + y6i;
    a[7] = y2i - y6r;
}


void cftf082(double *a)
{
    double wn4r, wk1r, wk1i, x0r, x0i, x1r, x1i, 
        y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i, 
        y4r, y4i, y5r, y5i, y6r, y6i, y7r, y7i;
    
    wn4r = WR5000;
    wk1r = WR2500;
    wk1i = WI2500;
    y0r = a[0] - a[9];
    y0i = a[1] + a[8];
    y1r = a[0] + a[9];
    y1i = a[1] - a[8];
    x0r = a[4] - a[13];
    x0i = a[5] + a[12];
    y2r = wn4r * (x0r - x0i);
    y2i = wn4r * (x0i + x0r);
    x0r = a[4] + a[13];
    x0i = a[5] - a[12];
    y3r = wn4r * (x0r - x0i);
    y3i = wn4r * (x0i + x0r);
    x0r = a[2] - a[11];
    x0i = a[3] + a[10];
    y4r = wk1r * x0r - wk1i * x0i;
    y4i = wk1r * x0i + wk1i * x0r;
    x0r = a[2] + a[11];
    x0i = a[3] - a[10];
    y5r = wk1i * x0r - wk1r * x0i;
    y5i = wk1i * x0i + wk1r * x0r;
    x0r = a[6] - a[15];
    x0i = a[7] + a[14];
    y6r = wk1i * x0r - wk1r * x0i;
    y6i = wk1i * x0i + wk1r * x0r;
    x0r = a[6] + a[15];
    x0i = a[7] - a[14];
    y7r = wk1r * x0r - wk1i * x0i;
    y7i = wk1r * x0i + wk1i * x0r;
    x0r = y0r + y2r;
    x0i = y0i + y2i;
    x1r = y4r + y6r;
    x1i = y4i + y6i;
    a[0] = x0r + x1r;
    a[1] = x0i + x1i;
    a[2] = x0r - x1r;
    a[3] = x0i - x1i;
    x0r = y0r - y2r;
    x0i = y0i - y2i;
    x1r = y4r - y6r;
    x1i = y4i - y6i;
    a[4] = x0r - x1i;
    a[5] = x0i + x1r;
    a[6] = x0r + x1i;
    a[7] = x0i - x1r;
    x0r = y1r - y3i;
    x0i = y1i + y3r;
    x1r = y5r - y7r;
    x1i = y5i - y7i;
    a[8] = x0r + x1r;
    a[9] = x0i + x1i;
    a[10] = x0r - x1r;
    a[11] = x0i - x1i;
    x0r = y1r + y3i;
    x0i = y1i - y3r;
    x1r = y5r + y7r;
    x1i = y5i + y7i;
    a[12] = x0r - x1i;
    a[13] = x0i + x1r;
    a[14] = x0r + x1i;
    a[15] = x0i - x1r;
}


void cftf040(double *a)
{
    double x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;
    
    x0r = a[0] + a[4];
    x0i = a[1] + a[5];
    x1r = a[0] - a[4];
    x1i = a[1] - a[5];
    x2r = a[2] + a[6];
    x2i = a[3] + a[7];
    x3r = a[2] - a[6];
    x3i = a[3] - a[7];
    a[0] = x0r + x2r;
    a[1] = x0i + x2i;
    a[2] = x1r - x3i;
    a[3] = x1i + x3r;
    a[4] = x0r - x2r;
    a[5] = x0i - x2i;
    a[6] = x1r + x3i;
    a[7] = x1i - x3r;
}


void cftb040(double *a)
{
    double x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;
    
    x0r = a[0] + a[4];
    x0i = a[1] + a[5];
    x1r = a[0] - a[4];
    x1i = a[1] - a[5];
    x2r = a[2] + a[6];
    x2i = a[3] + a[7];
    x3r = a[2] - a[6];
    x3i = a[3] - a[7];
    a[0] = x0r + x2r;
    a[1] = x0i + x2i;
    a[2] = x1r + x3i;
    a[3] = x1i - x3r;
    a[4] = x0r - x2r;
    a[5] = x0i - x2i;
    a[6] = x1r - x3i;
    a[7] = x1i + x3r;
}


void cftx020(double *a)
{
    double x0r, x0i;
    
    x0r = a[0] - a[2];
    x0i = a[1] - a[3];
    a[0] += a[2];
    a[1] += a[3];
    a[2] = x0r;
    a[3] = x0i;
}


void rftfsub(int n, double *a)
{
    int i, i0, j, k;
    double ec, w1r, w1i, wkr, wki, wdr, wdi, ss, xr, xi, yr, yi;
    
    ec = 2 * M_PI_2 / n;
    wkr = 0;
    wki = 0;
    wdi = cos(ec);
    wdr = sin(ec);
    wdi *= wdr;
    wdr *= wdr;
    w1r = 1 - 2 * wdr;
    w1i = 2 * wdi;
    ss = 2 * w1i;
    i = n >> 1;
    for (;;) {
        i0 = i - 4 * RDFT_LOOP_DIV;
        if (i0 < 4) {
            i0 = 4;
        }
        for (j = i - 4; j >= i0; j -= 4) {
            k = n - j;
            xr = a[j + 2] - a[k - 2];
            xi = a[j + 3] + a[k - 1];
            yr = wdr * xr - wdi * xi;
            yi = wdr * xi + wdi * xr;
            a[j + 2] -= yr;
            a[j + 3] -= yi;
            a[k - 2] += yr;
            a[k - 1] -= yi;
            wkr += ss * wdi;
            wki += ss * (0.5 - wdr);
            xr = a[j] - a[k];
            xi = a[j + 1] + a[k + 1];
            yr = wkr * xr - wki * xi;
            yi = wkr * xi + wki * xr;
            a[j] -= yr;
            a[j + 1] -= yi;
            a[k] += yr;
            a[k + 1] -= yi;
            wdr += ss * wki;
            wdi += ss * (0.5 - wkr);
        }
        if (i0 == 4) {
            break;
        }
        wkr = 0.5 * sin(ec * i0);
        wki = 0.5 * cos(ec * i0);
        wdr = 0.5 - (wkr * w1r - wki * w1i);
        wdi = wkr * w1i + wki * w1r;
        wkr = 0.5 - wkr;
        i = i0;
    }
    xr = a[2] - a[n - 2];
    xi = a[3] + a[n - 1];
    yr = wdr * xr - wdi * xi;
    yi = wdr * xi + wdi * xr;
    a[2] -= yr;
    a[3] -= yi;
    a[n - 2] += yr;
    a[n - 1] -= yi;
}


void rftbsub(int n, double *a)
{
    int i, i0, j, k;
    double ec, w1r, w1i, wkr, wki, wdr, wdi, ss, xr, xi, yr, yi;
    
    ec = 2 * M_PI_2 / n;
    wkr = 0;
    wki = 0;
    wdi = cos(ec);
    wdr = sin(ec);
    wdi *= wdr;
    wdr *= wdr;
    w1r = 1 - 2 * wdr;
    w1i = 2 * wdi;
    ss = 2 * w1i;
    i = n >> 1;
    for (;;) {
        i0 = i - 4 * RDFT_LOOP_DIV;
        if (i0 < 4) {
            i0 = 4;
        }
        for (j = i - 4; j >= i0; j -= 4) {
            k = n - j;
            xr = a[j + 2] - a[k - 2];
            xi = a[j + 3] + a[k - 1];
            yr = wdr * xr + wdi * xi;
            yi = wdr * xi - wdi * xr;
            a[j + 2] -= yr;
            a[j + 3] -= yi;
            a[k - 2] += yr;
            a[k - 1] -= yi;
            wkr += ss * wdi;
            wki += ss * (0.5 - wdr);
            xr = a[j] - a[k];
            xi = a[j + 1] + a[k + 1];
            yr = wkr * xr + wki * xi;
            yi = wkr * xi - wki * xr;
            a[j] -= yr;
            a[j + 1] -= yi;
            a[k] += yr;
            a[k + 1] -= yi;
            wdr += ss * wki;
            wdi += ss * (0.5 - wkr);
        }
        if (i0 == 4) {
            break;
        }
        wkr = 0.5 * sin(ec * i0);
        wki = 0.5 * cos(ec * i0);
        wdr = 0.5 - (wkr * w1r - wki * w1i);
        wdi = wkr * w1i + wki * w1r;
        wkr = 0.5 - wkr;
        i = i0;
    }
    xr = a[2] - a[n - 2];
    xi = a[3] + a[n - 1];
    yr = wdr * xr + wdi * xi;
    yi = wdr * xi - wdi * xr;
    a[2] -= yr;
    a[3] -= yi;
    a[n - 2] += yr;
    a[n - 1] -= yi;
}


void dctsub(int n, double *a)
{
    int i, i0, j, k, m;
    double ec, w1r, w1i, wkr, wki, wdr, wdi, ss, xr, xi, yr, yi;
    
    ec = M_PI_2 / n;
    wkr = 0.5;
    wki = 0.5;
    w1r = cos(ec);
    w1i = sin(ec);
    wdr = 0.5 * (w1r - w1i);
    wdi = 0.5 * (w1r + w1i);
    ss = 2 * w1i;
    m = n >> 1;
    i = 0;
    for (;;) {
        i0 = i + 2 * DCST_LOOP_DIV;
        if (i0 > m - 2) {
            i0 = m - 2;
        }
        for (j = i + 2; j <= i0; j += 2) {
            k = n - j;
            xr = wdi * a[j - 1] - wdr * a[k + 1];
            xi = wdr * a[j - 1] + wdi * a[k + 1];
            wkr -= ss * wdi;
            wki += ss * wdr;
            yr = wki * a[j] - wkr * a[k];
            yi = wkr * a[j] + wki * a[k];
            wdr -= ss * wki;
            wdi += ss * wkr;
            a[k + 1] = xr;
            a[k] = yr;
            a[j - 1] = xi;
            a[j] = yi;
        }
        if (i0 == m - 2) {
            break;
        }
        wdr = cos(ec * i0);
        wdi = sin(ec * i0);
        wkr = 0.5 * (wdr - wdi);
        wki = 0.5 * (wdr + wdi);
        wdr = wkr * w1r - wki * w1i;
        wdi = wkr * w1i + wki * w1r;
        i = i0;
    }
    xr = wdi * a[m - 1] - wdr * a[m + 1];
    a[m - 1] = wdr * a[m - 1] + wdi * a[m + 1];
    a[m + 1] = xr;
    a[m] *= WR5000;
}


void dstsub(int n, double *a)
{
    int i, i0, j, k, m;
    double ec, w1r, w1i, wkr, wki, wdr, wdi, ss, xr, xi, yr, yi;
    
    ec = M_PI_2 / n;
    wkr = 0.5;
    wki = 0.5;
    w1r = cos(ec);
    w1i = sin(ec);
    wdr = 0.5 * (w1r - w1i);
    wdi = 0.5 * (w1r + w1i);
    ss = 2 * w1i;
    m = n >> 1;
    i = 0;
    for (;;) {
        i0 = i + 2 * DCST_LOOP_DIV;
        if (i0 > m - 2) {
            i0 = m - 2;
        }
        for (j = i + 2; j <= i0; j += 2) {
            k = n - j;
            xr = wdi * a[k + 1] - wdr * a[j - 1];
            xi = wdr * a[k + 1] + wdi * a[j - 1];
            wkr -= ss * wdi;
            wki += ss * wdr;
            yr = wki * a[k] - wkr * a[j];
            yi = wkr * a[k] + wki * a[j];
            wdr -= ss * wki;
            wdi += ss * wkr;
            a[j - 1] = xr;
            a[j] = yr;
            a[k + 1] = xi;
            a[k] = yi;
        }
        if (i0 == m - 2) {
            break;
        }
        wdr = cos(ec * i0);
        wdi = sin(ec * i0);
        wkr = 0.5 * (wdr - wdi);
        wki = 0.5 * (wdr + wdi);
        wdr = wkr * w1r - wki * w1i;
        wdi = wkr * w1i + wki * w1r;
        i = i0;
    }
    xr = wdi * a[m + 1] - wdr * a[m - 1];
    a[m + 1] = wdr * a[m + 1] + wdi * a[m - 1];
    a[m - 1] = xr;
    a[m] *= WR5000;
}


void dctsub4(int n, double *a)
{
    int m;
    double wki, wdr, wdi, xr;
    
    wki = WR5000;
    m = n >> 1;
    if (m == 2) {
        wdr = wki * WI2500;
        wdi = wki * WR2500;
        xr = wdi * a[1] - wdr * a[3];
        a[1] = wdr * a[1] + wdi * a[3];
        a[3] = xr;
    }
    a[m] *= wki;
}


void dstsub4(int n, double *a)
{
    int m;
    double wki, wdr, wdi, xr;
    
    wki = WR5000;
    m = n >> 1;
    if (m == 2) {
        wdr = wki * WI2500;
        wdi = wki * WR2500;
        xr = wdi * a[3] - wdr * a[1];
        a[3] = wdr * a[3] + wdi * a[1];
        a[1] = xr;
    }
    a[m] *= wki;
}

