/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */


#include "m_pd.h"

/* --------------------- up/down-sampling --------------------- */
t_int *downsampling_perform_0(t_int *w)
{
  t_sample *in  = (t_sample *)(w[1]); /* original signal     */
  t_sample *out = (t_sample *)(w[2]); /* downsampled signal  */
  int down     = (int)(w[3]);       /* downsampling factor */
  int parent   = (int)(w[4]);       /* original vectorsize */

  int n=parent/down;

  while(n--){
    *out++=*in;
    in+=down;
  }

  return (w+5);
}

t_int *upsampling_perform_0(t_int *w)
{
  t_sample *in  = (t_sample *)(w[1]); /* original signal     */
  t_sample *out = (t_sample *)(w[2]); /* upsampled signal    */
  int up       = (int)(w[3]);       /* upsampling factor   */
  int parent   = (int)(w[4]);       /* original vectorsize */

  int n=parent*up;
  t_sample *dummy = out;
  
  while(n--)*out++=0;

  n = parent;
  out = dummy;
  while(n--){
    *out=*in++;
    out+=up;
  }

  return (w+5);
}

t_int *upsampling_perform_hold(t_int *w)
{
  t_sample *in  = (t_sample *)(w[1]); /* original signal     */
  t_sample *out = (t_sample *)(w[2]); /* upsampled signal    */
  int up       = (int)(w[3]);       /* upsampling factor   */
  int parent   = (int)(w[4]);       /* original vectorsize */
  int i=up;

  int n=parent;
  t_sample *dum_out = out;
  t_sample *dum_in  = in;
  
  while (i--) {
    n = parent;
    out = dum_out+i;
    in  = dum_in;
    while(n--){
      *out=*in++;
      out+=up;
    }
  }
  return (w+5);
}

t_int *upsampling_perform_linear(t_int *w)
{
  t_resample *x= (t_resample *)(w[1]);
  t_sample *in  = (t_sample *)(w[2]); /* original signal     */
  t_sample *out = (t_sample *)(w[3]); /* upsampled signal    */
  int up       = (int)(w[4]);       /* upsampling factor   */
  int parent   = (int)(w[5]);       /* original vectorsize */
  int length   = parent*up;
  int n;
  t_sample *fp;
  t_sample a=*x->buffer, b=*in;

  
  for (n=0; n<length; n++) {
    t_sample findex = (t_sample)(n+1)/up;
    int     index  = findex;
    t_sample frac=findex - index;
    if (frac==0.)frac=1.;
    *out++ = frac * b + (1.-frac) * a;
    fp = in+index;
    b=*fp;
    a=(index)?*(fp-1):a;
  }

  *x->buffer = a;
  return (w+6);
}

/* ----------------------- public -------------------------------- */

/* utils */

void resample_init(t_resample *x)
{
  x->method=0;

  x->downsample=x->upsample=1;

  x->s_n = x->coefsize = x->bufsize = 0;
  x->s_vec = x->coeffs = x->buffer  = 0;
}

void resample_free(t_resample *x)
{
  if (x->s_n) t_freebytes(x->s_vec, x->s_n*sizeof(*x->s_vec));
  if (x->coefsize) t_freebytes(x->coeffs, x->coefsize*sizeof(*x->coeffs));
  if (x->bufsize) t_freebytes(x->buffer, x->bufsize*sizeof(*x->buffer));

  x->s_n = x->coefsize = x->bufsize = 0;
  x->s_vec = x->coeffs = x->buffer  = 0;
}


/* dsp-adding */

void resample_dsp(t_resample *x,
                  t_sample* in,  int insize,
                  t_sample* out, int outsize,
                  int method)
{
  if (insize == outsize){
    bug("nothing to be done");
    return;
  }

  if (insize > outsize) { /* downsampling */
    if (insize % outsize) {
      error("bad downsampling factor");
      return;
    }
    switch (method) {
    default:
      dsp_add(downsampling_perform_0, 4, in, out, insize/outsize, insize);
    }


  } else { /* upsampling */
    if (outsize % insize) {
      error("bad upsampling factor");
      return;
    }
    switch (method) {
    case 1:
      dsp_add(upsampling_perform_hold, 4, in, out, outsize/insize, insize);
      break;
    case 2:
      if (x->bufsize != 1) {
        t_freebytes(x->buffer, x->bufsize*sizeof(*x->buffer));
        x->bufsize = 1;
        x->buffer = t_getbytes(x->bufsize*sizeof(*x->buffer));
      }
      dsp_add(upsampling_perform_linear, 5, x, in, out, outsize/insize, insize);
      break;
    default:
      dsp_add(upsampling_perform_0, 4, in, out, outsize/insize, insize);
    }
  }
}

void resamplefrom_dsp(t_resample *x,
                           t_sample *in,
                           int insize, int outsize, int method)
{
  if (insize==outsize) {
   t_freebytes(x->s_vec, x->s_n * sizeof(*x->s_vec));
    x->s_n = 0;
    x->s_vec = in;
    return;
  }

  if (x->s_n != outsize) {
    t_sample *buf=x->s_vec;
    t_freebytes(buf, x->s_n * sizeof(*buf));
    buf = (t_sample *)t_getbytes(outsize * sizeof(*buf));
    x->s_vec = buf;
    x->s_n   = outsize;
  }

  resample_dsp(x, in, insize, x->s_vec, x->s_n, method);
  return;
}

void resampleto_dsp(t_resample *x,
                         t_sample *out, 
                         int insize, int outsize, int method)
{
  if (insize==outsize) {
    if (x->s_n)t_freebytes(x->s_vec, x->s_n * sizeof(*x->s_vec));
    x->s_n = 0;
    x->s_vec = out;
    return;
  }

  if (x->s_n != insize) {
    t_sample *buf=x->s_vec;
    t_freebytes(buf, x->s_n * sizeof(*buf));
    buf = (t_sample *)t_getbytes(insize * sizeof(*buf));
    x->s_vec = buf;
    x->s_n   = insize;
  }

  resample_dsp(x, x->s_vec, x->s_n, out, outsize, method);

  return;
}
