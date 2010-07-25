/*****************************************************************************/
/*                                                                           */
/* Fast Fourier Transform                                                    */
/* Network Abstraction, Definitions                                          */
/* Kevin Peterson, MIT Media Lab, EMS                                        */
/* UROP - Fall '86                                                           */
/* REV: 6/12/87(KHP) - To incorporate link list of different sized networks  */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* added debug option 5/91 brown@nadia                                       */
/* change sign at AAA                                                        */
/*                                                                           */
/* Fast Fourier Transform                                                    */
/* FFT Network Interaction and Support Modules                               */
/* Kevin Peterson, MIT Media Lab, EMS                                        */
/* UROP - Fall '86                                                           */
/* REV: 6/12/87(KHP) - Generalized to one procedure call with typed I/O      */
/*                                                                           */
/*****************************************************************************/

/* Overview:
        
   My realization of the FFT involves a representation of a network of
   "butterfly" elements that takes a set of 'N' sound samples as input and
   computes the discrete Fourier transform.  This network consists of a 
   series of stages (log2 N), each stage consisting of N/2 parallel butterfly
   elements. Consecutive stages are connected by specific, predetermined flow 
   paths, (see Oppenheim, Schafer for details) and each butterfly element has
   an associated multiplicative coefficient.

   FFT NETWORK:
   -----------  
      ____    _    ____    _    ____    _    ____    _    ____
  o--|    |o-| |-o|    |o-| |-o|    |o-| |-o|    |o-| |-o|    |--o
     |reg1|  | |  |W^r1|  | |  |reg1|  | |  |W^r1|  | |  |reg1|
     |    |  | |  |    |  | |  |    |  | |  |    |  | |  |    | .....
     |    |  | |  |    |  | |  |    |  | |  |    |  | |  |    |  
  o--|____|o-| |-o|____|o-| |-o|____|o-| |-o|____|o-| |-o|____|--o
             | |          | |          | |          | |
             | |          | |          | |          | |
      ____   | |   ____   | |   ____   | |   ____   | |   ____ 
  o--|    |o-| |-o|    |o-| |-o|    |o-| |-o|    |o-| |-o|    |--o
     |reg2|  | |  |W^r2|  | |  |reg2|  | |  |W^r2|  | |  |reg2|
     |    |  | |  |    |  | |  |    |  | |  |    |  | |  |    | .....
     |    |  | |  |    |  | |  |    |  | |  |    |  | |  |    |
  o--|____|o-| |-o|____|o-| |-o|____|o-| |-o|____|o-| |-o|____|--o
             | |          | |          | |          | |
             | |          | |          | |          | |
       :      :     :      :     :      :     :      :     :
       :      :     :      :     :      :     :      :     :
       :      :     :      :     :      :     :      :     :
       :      :     :      :     :      :     :      :     :
       :      :     :      :     :      :     :      :     :

      ____   | |   ____   | |   ____   | |   ____   | |   ____ 
  o--|    |o-| |-o|    |o-| |-o|    |o-| |-o|    |o-| |-o|    |--o
     |reg |  | |  |W^r |  | |  |reg |  | |  |W^r |  | |  |reg |
     | N/2|  | |  | N/2|  | |  | N/2|  | |  | N/2|  | |  | N/2| .....
     |    |  | |  |    |  | |  |    |  | |  |    |  | |  |    |
  o--|____|o-|_|-o|____|o-|_|-o|____|o-|_|-o|____|o-|_|-o|____|--o

              ^            ^            ^            ^
    Initial   |  Bttrfly   |   Rd/Wrt   |   Bttrfly  |   Rd/Wrt
    Buffer    |            |  Register  |            |  Register
              |____________|____________|____________|
                                 |
                                 |
                            Interconnect
                               Paths

   The use of "in-place" computation permits one to use only one set of 
   registers realized by an array of complex number structures.  To describe
   the coefficients for each butterfly I am using a two dimensional array
   (stage, butterfly) of complex numbers.  The predetermined stage connections
   will be described in a two dimensional array of indicies.  These indicies 
   will be used to determine the order of reading at each stage of the    
   computation.  
*/


/*****************************************************************************/
/* INCLUDE FILES                                                             */
/*****************************************************************************/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

    /* the following is needed only to declare pd_fft() as exportable in MSW */
#include "m_pd.h"

/* some basic definitions */
#ifndef BOOL
#define        BOOL                    int
#define        TRUE                    1
#define        FALSE                   0
#endif

#define        SAMPLE t_float         /* data type used in calculation */

#define        SHORT_SIZE              sizeof(short)
#define        INT_SIZE                sizeof(int)
#define        SAMPLE_SIZE             sizeof(SAMPLE)
#define        PNTR_SIZE               sizeof(char *)

#define        PI                      3.1415927
#define        TWO_PI                  6.2831854

/* type definitions for I/O buffers */
#define        REAL                    0          /* real only          */
#define        IMAG                    2          /* imaginary only     */
#define        RECT                    8          /* real and imaginary */
#define        MAG                     16         /* magnitude only     */
#define        PHASE                   32         /* phase only         */
#define        POLAR                   64         /* magnitude and phase*/

/* scale definitions for I/O buffers */
#define        LINEAR                  0
#define        DB                      1          /* 20log10            */

/* transform direction definition */
#define        FORWARD                 1          /* Forward FFT        */
#define        INVERSE                 2          /* Inverse FFT        */

/* window type definitions */
#define        HANNING                 1
#define        RECTANGULAR             0



/* network structure definition */

typedef struct Tfft_net {
        int             n;
        int             stages;
        int             bps;
        int             direction;
        int             window_type;
        int             *load_index;
        SAMPLE          *window, *inv_window;
        SAMPLE          *regr;
        SAMPLE          *regi;
        SAMPLE          **indexpr;
        SAMPLE          **indexpi;
        SAMPLE          **indexqr;
        SAMPLE          **indexqi;
        SAMPLE          *coeffr, *inv_coeffr;
        SAMPLE          *coeffi, *inv_coeffi;
        struct Tfft_net *next;  
} FFT_NET;


void cfft(int trnsfrm_dir, int npnt, int window,
    SAMPLE *source_buf, int source_form, int source_scale,
    SAMPLE *result_buf, int result_form, int result_scale, int debug);


/*****************************************************************************/
/* GLOBAL DECLARATIONS                                                       */
/*****************************************************************************/

static FFT_NET  *firstnet;

/* prototypes */

void net_alloc(FFT_NET *fft_net);
void net_dealloc(FFT_NET *fft_net);
int power_of_two(int n);
void create_hanning(SAMPLE *window, int n, SAMPLE scale);
void create_rectangular(SAMPLE *window, int n, SAMPLE scale);
void short_to_float(short *short_buf, SAMPLE *float_buf, int n);
void load_registers(FFT_NET *fft_net, SAMPLE *buf, int buf_form,
    int buf_scale, int trnsfrm_dir);
void compute_fft(FFT_NET  *fft_net);
void store_registers(FFT_NET    *fft_net, SAMPLE *buf, int buf_form,
    int buf_scale, int debug);
void build_fft_network(FFT_NET *fft_net, int n, int window_type);

/*****************************************************************************/
/* GENERALIZED FAST FOURIER TRANSFORM MODULE                                 */
/*****************************************************************************/

void cfft(int trnsfrm_dir, int npnt, int window,
    SAMPLE *source_buf, int source_form, int source_scale,
    SAMPLE *result_buf, int result_form, int result_scale, int debug)

/* modifies: result_buf
   effects:  Computes npnt FFT specified by form, scale, and dir parameters.  
         Source samples (single precision float) are taken from soure_buf and 
         the transfrmd representation is stored in result_buf (single precision
         float).  The parameters are defined as follows:
        
         trnsfrm_dir = FORWARD | INVERSE
         npnt        = 2^k for some any positive integer k
         window      = HANNING | RECTANGULAR
         (RECT = real and imag parts, POLAR = magnitude and phase)
         source_form = REAL | IMAG | RECT | POLAR  
         result_form = REAL | IMAG | RECT | MAG | PHASE | POLAR
         xxxxxx_scale= LINEAR | DB ( 20log10 |mag| )
         
         The input/output buffers are stored in a form appropriate to the type.
         For example: REAL  => {real, real, real ...}, 
                      MAG   => {mag, mag, mag, ... },
                      RECT  => {real, imag, real, imag, ... },
                      POLAR => {mag, phase, mag, phase, ... }.

         To look at the magnitude (in db) of a 1024 point FFT of a real time 
         signal we have:

         fft(FORWARD, 1024, RECTANGULAR, input, REAL, LINEAR, output, MAG, DB)

         All possible input and output combinations are possible given the 
         choice of type and scale parameters.
*/

{
         FFT_NET         *thisnet = (FFT_NET *)0;
         FFT_NET         *lastnet = (FFT_NET *)0;
         
         /* A linked list of fft networks of different sizes is maintained to
            avoid building with every call.  The network is built on the first
            call but reused for subsequent calls requesting the same size 
            transformation.
         */
   
         thisnet=firstnet;
         while (thisnet) {
             if (!(thisnet->n == npnt) || !(thisnet->window_type == window)) { 
               /* current net doesn't match size or window type */
               lastnet=thisnet;
               thisnet=thisnet->next;
               continue;                  /* keep looking */
             }

             else {                       /* network matches desired size */
               load_registers(thisnet, source_buf, source_form, source_scale, 
                              trnsfrm_dir);
               compute_fft(thisnet);      /* do transformation */
               store_registers(thisnet, result_buf, result_form, result_scale,debug);
               return;
             }
         }           

         /* none of existing networks match required size*/

         if (lastnet) {                 /* add new network to end of list */
           thisnet = (FFT_NET *)malloc(sizeof(FFT_NET));      /* allocate */
           thisnet->next = 0;
           lastnet->next = thisnet;     /* add to end of list             */
         }
         else {                         /* first network to be created    */
           thisnet=firstnet=(FFT_NET *)malloc(sizeof(FFT_NET)); /* alloc. */
           thisnet->next = 0;
         }

         /* build new network and compute transformation */
         build_fft_network(thisnet, npnt, window);
         load_registers(thisnet, source_buf, source_form, source_scale, 
                        trnsfrm_dir);
         compute_fft(thisnet);
         store_registers(thisnet, result_buf, result_form, result_scale,debug);
         return;
}

void fft_clear(void)

/* effects: Deallocates all preserved FFT networks.  Should be used when 
         finished with all computations.
*/

{
         FFT_NET      *thisnet, *nextnet;

         if (firstnet) {
           thisnet=firstnet;
           do {
             nextnet = thisnet->next;
             net_dealloc(thisnet);
             free((char *)thisnet);
           } while (thisnet = nextnet);
         }
}
           

/*****************************************************************************/
/* NETWORK CONSTRUCTION                                                      */
/*****************************************************************************/

void build_fft_network(FFT_NET *fft_net, int n, int window_type)


/* modifies:fft_net
   effects: Constructs the fft network as described in fft.h.  Butterfly
         coefficients, read/write indicies, bit reversed load indicies,
         and array allocations are computed.
*/

{
         int cntr, i, j, s; 
         int       stages, bps;
         int       **p, **q, *pp, *qp;
         SAMPLE     two_pi_div_n = TWO_PI / n;


         /* network definition */
         fft_net->n   = n;
         fft_net->bps = bps = n/2;
         for (i = 0, j = n; j > 1; j >>= 1, i++);
         fft_net->stages = stages = i;
         fft_net->direction = FORWARD;
         fft_net->window_type = window_type;
         fft_net->next = (FFT_NET *)0;

         /* allocate registers, index, coefficient arrays */
         net_alloc(fft_net);


         /* create appropriate windows */
         if (window_type==HANNING)   {
                  create_hanning(fft_net->window, n, 1.);
                  create_hanning(fft_net->inv_window, n, 1./n);
         }
         else {
                  create_rectangular(fft_net->window, n, 1.);
                  create_rectangular(fft_net->inv_window, n, 1./n);
         }


         /* calculate butterfly coefficients */ {
                  
                  int       num_diff_coeffs, power_inc, power;
                  SAMPLE *coeffpr     = fft_net->coeffr;
                  SAMPLE *coeffpi     = fft_net->coeffi;
                  SAMPLE *inv_coeffpr = fft_net->inv_coeffr;
                  SAMPLE *inv_coeffpi = fft_net->inv_coeffi;
                  
                  /* stage one coeffs are 1 + 0j */
                  for (i = 0; i < bps; i++) {
                           *coeffpr = *inv_coeffpr = 1.;
                           *coeffpi = *inv_coeffpi = 0.;
                           coeffpr++; inv_coeffpr++;
                           coeffpi++; inv_coeffpi++;
                  }

                  /* stage 2 to last stage coeffs need calculation */
                  /* (1<<r <=> 2^r */
                  for (s = 2; s <= stages; s++) {
                           
                           num_diff_coeffs = n / (1 << (stages - s + 1)); 
                           power_inc       = 1 << (stages -s);
                           cntr            = 0;

                           for (i = bps/num_diff_coeffs; i > 0; i--) {

                              power  = 0;

                              for (j = num_diff_coeffs; j > 0; j--) {
                                 *coeffpr     = cos(two_pi_div_n*power);
                                 *inv_coeffpr = cos(two_pi_div_n*power);
/* AAA change these signs */     *coeffpi     = -sin(two_pi_div_n*power);
/* change back */                *inv_coeffpi = sin(two_pi_div_n*power);
                                 power += power_inc;
                                 coeffpr++; inv_coeffpr++;
                                 coeffpi++; inv_coeffpi++;
                              }
                           }
                  }
         }

         /* calculate network indicies:  stage exchange indicies are 
            calculated and then used as offset values from the base
            register locations.  The final addresses are then stored in
            fft_net.
         */ {

                  int       index, inc;
                  SAMPLE **indexpr = fft_net->indexpr;
                  SAMPLE **indexpi = fft_net->indexpi;
                  SAMPLE **indexqr = fft_net->indexqr;
                  SAMPLE **indexqi = fft_net->indexqi;
                  SAMPLE *regr     = fft_net->regr;
                  SAMPLE *regi     = fft_net->regi; 


                  /* allocate temporary 2d stage exchange index, 1d temp 
                     load index */
                  p = (int **)malloc(stages * PNTR_SIZE);
                  q = (int **)malloc(stages * PNTR_SIZE);

                  for (s = 0; s < stages; s++) {
                           p[s] = (int *)malloc(bps * INT_SIZE);
                           q[s] = (int *)malloc(bps * INT_SIZE);
                  }

                  /* calculate stage exchange indicies: */
                  for (s = 0; s < stages; s++) {
                           pp = p[s];
                           qp = q[s];
                           inc    = 1 << s;
                           cntr   = 1 << (stages-s-1);
                           i      = j = index = 0;

                           do {
                                    do {
                                             qp[i]   = index + inc;
                                             pp[i++] = index++;
                                    }  while (++j < inc);
                                    index = qp[i-1] + 1;
                                    j = 0;
                           }        while (--cntr);
                  }

                  /* compute actual address values using indicies as offsets */
                  for (s = 0; s < stages; s++) {
                           for (i = 0; i < bps; i++) {
                                    *indexpr++ = regr + p[s][i];
                                    *indexpi++ = regi + p[s][i];
                                    *indexqr++ = regr + q[s][i];
                                    *indexqi++ = regi + q[s][i];
                           }
                  }
         }


         /* calculate load indicies (bit reverse ordering) */
         /* bit reverse ordering achieved by passing normal
            order indicies backwards through the network */
                  
         /* init to normal order indicies */ {
                  int *load_index,*load_indexp;
                  int *temp_indexp, *temp_index;
                  temp_index=temp_indexp=(int *)malloc(n * INT_SIZE);
                           
                  i = 0; j = n;
                  load_index = load_indexp = fft_net->load_index;
                           
                  while (j--)
                    *load_indexp++ = i++;

        /* pass indicies backwards through net */
                  for (s = stages - 1; s > 0; s--) {
                           pp = p[s];
                           qp = q[s];

                           for (i = 0; i < bps; i++) {
                                    temp_index[pp[i]]=load_index[2*i];
                                    temp_index[qp[i]]=load_index[2*i+1];
                           }
                           j = n;
                           load_indexp = load_index;
                           temp_indexp = temp_index;
                           while (j--) 
                             *load_indexp++ = *temp_indexp++;
                  }
                  
                  /* free all temporary arrays */
                  free((char *)temp_index);
                  for (s = 0; s < stages; s++) {
                           free((char *)p[s]);free((char *)q[s]);
                  }
                  free((char *)p);free((char *)q);
         }
}



/*****************************************************************************/
/* REGISTER LOAD AND STORE                                                   */
/*****************************************************************************/

void load_registers(FFT_NET *fft_net, SAMPLE *buf, int buf_form,
    int buf_scale, int trnsfrm_dir)

/* effects:  Multiplies the input buffer with the appropriate window and
         stores the resulting values in the initial registers of the
         network.  Input buffer must contain values appropriate to form.  
         For RECT, the buffer contains real num. followed by imag num, 
         and for POLAR, it contains magnitude followed by phase.  Pure
         inputs are listed normally.  Both LINEAR and DB scales are 
         interpreted.
*/

{
         int      *load_index = fft_net->load_index;
         SAMPLE *window;
         int index, i = 0, n = fft_net->n;

         if      (trnsfrm_dir==FORWARD)   window = fft_net->window;
         else if (trnsfrm_dir==INVERSE)   window = fft_net->inv_window;
         else {
                  fprintf(stderr, "load_registers:illegal transform direction\n"); 
                  exit(0);
         }
         fft_net->direction = trnsfrm_dir;

         switch(buf_scale) {
         case LINEAR: {

           switch (buf_form) {
           case REAL: {                    /* pure REAL */
             while (i < fft_net->n) {  
               index = load_index[i];
               fft_net->regr[i]=(SAMPLE)buf[index] * window[index];
               fft_net->regi[i]=0.;
               i++;                                            
             }
           } break;

           case IMAG: {                    /* pure IMAGinary */
             while (i < fft_net->n) {  
               index = load_index[i];
               fft_net->regr[i]=0;
               fft_net->regi[i]=(SAMPLE)buf[index] * window[index];
               i++;                            
             }                
           } break;

           case RECT: {                    /* both REAL and IMAGinary */
             while (i < fft_net->n) {
               index = load_index[i];
               fft_net->regr[i]=(SAMPLE)buf[index*2]   * window[index];
               fft_net->regi[i]=(SAMPLE)buf[index*2+1] * window[index];
               i++;
             }
           } break;      
         
           case POLAR: {                   /* magnitude followed by phase */
             while (i < fft_net->n) {
               index = load_index[i];
               fft_net->regr[i]=(SAMPLE)(buf[index*2] * cos(buf[index*2+1])) 
                                                      * window[index];
               fft_net->regi[i]=(SAMPLE)(buf[index*2] * sin(buf[index*2+1])) 
                                                      * window[index];
               i++;                            
             }                
           } break;

           default: {
             fprintf(stderr, "load_registers:illegal input form\n"); 
             exit(0);
           } break;
           }
         } break;

         case DB: {
          
           switch (buf_form) {
           case REAL: {                     /* log pure REAL */
             while (i < fft_net->n) {  
               index = load_index[i];
               fft_net->regr[i]=(SAMPLE)pow(10., (1./20.)*buf[index]) 
                 * window[index];    /* window scaling after linearization */
               fft_net->regi[i]=0.;
               i++;                                            
             }
           } break;

           case IMAG: {                     /* log pure IMAGinary */
             while (i < fft_net->n) {  
               index = load_index[i];
               fft_net->regr[i]=0.;
               fft_net->regi[i]=(SAMPLE)pow(10., (1./20.)*buf[index])
                    * window[index];
               i++;                            
            }                
           } break;

           case RECT: {                     /* log REAL and log IMAGinary */
             while (i < fft_net->n) {
               index = load_index[i];
               fft_net->regr[i]=(SAMPLE)pow(10., (1./20.)*buf[index*2])
                 * window[index];
               fft_net->regi[i]=(SAMPLE)pow(10., (1./20.)*buf[index*2+1]) 
                 * window[index];
               i++;
             }
           } break;      
         
           case POLAR: {                    /* log mag followed by phase */
             while (i < fft_net->n) {
               index = load_index[i];
               fft_net->regr[i]=(SAMPLE)(pow(10., (1./20.)*buf[index*2])
                             * cos(buf[index*2+1])) * window[index];
               fft_net->regi[i]=(SAMPLE)(pow(10., (1./20.)*buf[index*2])
                             * sin(buf[index*2+1])) * window[index];
               i++;                            
             }                
           } break;

           default: {
             fprintf(stderr, "load_registers:illegal input form\n"); 
             exit(0);
           } break;
           }
         } break;

         default: {
           fprintf(stderr, "load_registers:illegal input scale\n"); 
           exit(0);
         } break;
         }
}


void store_registers(FFT_NET    *fft_net, SAMPLE *buf, int buf_form,
    int buf_scale, int debug)

/* modifies: buf
   effects:  Writes the final contents of the network registers into buf in 
         either linear or db scale, polar or rectangular form.  If any of 
         the pure forms(REAL, IMAG, MAG, or PHASE) are used then only the 
         corresponding part of the registers is stored in buf.
*/

{
         int        i;
         SAMPLE     real, imag, mag, phase;
         int        n;

         i = 0;
         n = fft_net->n;

         switch (buf_scale) {
         case LINEAR: {

           switch (buf_form) {
           case REAL: {                        /* pure REAL */
             do {
               *buf++ = (SAMPLE)fft_net->regr[i];
             } while (++i < n);  
           } break;

           case IMAG: {                        /* pure IMAGinary */
             do {
               *buf++ = (SAMPLE)fft_net->regi[i];
             } while (++i < n);  
           } break;

           case RECT: {                        /* both REAL and IMAGinary */   
             do {
               *buf++ = (SAMPLE)fft_net->regr[i];
               *buf++ = (SAMPLE)fft_net->regi[i];
             } while (++i < n);  
           } break;

           case MAG: {                         /* magnitude only */
             do {
               real  = fft_net->regr[i];
               imag  = fft_net->regi[i];
               *buf++ = (SAMPLE)sqrt(real*real+imag*imag);
             } while (++i < n);
           } break;

           case PHASE: {                       /* phase only */
             do {
               real  = fft_net->regr[i];
               imag  = fft_net->regi[i];
               if (real > .00001) 
                 *buf++ = (SAMPLE)atan2(imag, real);
               else {                          /* deal with bad case */
                 if (imag > 0){      *buf++ = PI / 2.;
                     if(debug) fprintf(stderr,"real=0 and imag > 0\n");}
                 else if (imag < 0){ *buf++ = -PI / 2.;
                     if(debug) fprintf(stderr,"real=0 and imag < 0\n");}
                 else {              *buf++ = 0;
                     if(debug) fprintf(stderr,"real=0 and imag=0\n");}
               }
             } while (++i < n);
           } break;

           case POLAR: {                       /* magnitude and phase */
             do {
               real    = fft_net->regr[i];
               imag    = fft_net->regi[i];
               *buf++  = (SAMPLE)sqrt(real*real+imag*imag);
               if (real)                       /* a hack to avoid div by zero */
                 *buf++ = (SAMPLE)atan2(imag, real);
               else {                          /* deal with bad case */
                 if (imag > 0)      *buf++ = PI / 2.;
                 else if (imag < 0) *buf++ = -PI / 2.;
                 else               *buf++ = 0;
               }
             } while (++i < n);
           } break;

           default: {
             fprintf(stderr, "store_registers:illegal output form\n");
             exit(0);
           } break;
           }
         } break;
                
         case DB: {

           switch (buf_form) {
           case REAL: {                        /* real only */
             do {
               *buf++ = (SAMPLE)20.*log10(fft_net->regr[i]);
             } while (++i < n);
           } break;

           case IMAG: {                        /* imag only */
             do {
               *buf++ = (SAMPLE)20.*log10(fft_net->regi[i]);
             } while (++i < n);
           } break;

           case RECT: {                        /* real and imag */
             do {
               *buf++ = (SAMPLE)20.*log10(fft_net->regr[i]);
               *buf++ = (SAMPLE)20.*log10(fft_net->regi[i]);
             } while (++i < n);  
           } break;

           case MAG: {                         /* magnitude only  */
             do {
               real  = fft_net->regr[i];
               imag  = fft_net->regi[i];
               *buf++ = (SAMPLE)20.*log10(sqrt(real*real+imag*imag));  
             } while (++i < n);
           } break;

           case PHASE: {                       /* phase only */
             do {
               real  = fft_net->regr[i];
               imag  = fft_net->regi[i];
               if (real) 
                 *buf++ = (SAMPLE)atan2(imag, real);
               else {                          /* deal with bad case */
                 if (imag > 0)      *buf++ = PI / 2.;
                 else if (imag < 0) *buf++ = -PI / 2.;
                 else               *buf++ = 0;
               }
             } while (++i < n);
           } break;

           case POLAR: {                       /* magnitude and phase */
             do {
               real  = fft_net->regr[i];
               imag  = fft_net->regi[i];
               *buf++ = (SAMPLE)20.*log10(sqrt(real*real+imag*imag));           
               if (real) 
                 *buf++ = (SAMPLE)atan2(imag, real);
               else {                          /* deal with bad case */
                 if (imag > 0)      *buf++ = PI / 2.;
                 else if (imag < 0) *buf++ = -PI / 2.;
                 else               *buf++ = 0;
               }
             } while (++i < n);
           } break;

           default: {
             fprintf(stderr, "store_registers:illegal output form\n");
             exit(0);
           } break;
           } 
         } break;

         default: {
           fprintf(stderr, "store_registers:illegal output scale\n");
           exit(0);
         } break;
         }
}



/*****************************************************************************/
/* COMPUTE TRANSFORMATION                                                    */
/*****************************************************************************/

void compute_fft(FFT_NET  *fft_net)


/* modifies: fft_net
   effects: Passes the values (already loaded) in the registers through
         the network, multiplying with appropriate coefficients at each 
         stage.  The fft result will be in the registers at the end of
         the computation.  The direction of the transformation is indicated
         by the network flag 'direction'.  The form of the computation is:

         X(pn) = X(p) + C*X(q)
         X(qn) = X(p) - C*X(q)

         where X(pn,qn) represents the output of the registers at each stage.  
         The calculations are actually done in place.  Register pointers are 
         used to speed up the calculations.

         Register and coefficient addresses involved in the calculations 
         are stored sequentially and are accessed as such. fft_net->indexp,
         indexq contain pointers to the relevant addresses, and fft_net->coeffs, 
         inv_coeffs points to the appropriate coefficients at each stage of the 
         computation.
*/

{
         SAMPLE     **xpr, **xpi, **xqr, **xqi, *cr, *ci;
         int        i;
         SAMPLE     tpr, tpi, tqr, tqi;
         int        bps = fft_net->bps;
         int        cnt = bps * (fft_net->stages - 1);

         /* predetermined register addresses and coefficients */
         xpr = fft_net->indexpr;              
         xpi = fft_net->indexpi;              
         xqr = fft_net->indexqr;
         xqi = fft_net->indexqi;

         if (fft_net->direction==FORWARD) {     /* FORWARD FFT coefficients */
                  cr  = fft_net->coeffr;
                  ci  = fft_net->coeffi;
         }
         else {                                 /* INVERSE FFT coefficients */
                  cr = fft_net->inv_coeffr;
                  ci = fft_net->inv_coeffi;
         }

         /* stage one coefficients are 1 + 0j so C*X(q)=X(q)  */
         /* bps mults can be avoided                          */

         for (i = 0; i < bps; i++) {

                  /* add X(p) and X(q) */
                  tpr = **xpr + **xqr;
                  tpi = **xpi + **xqi;
                  tqr = **xpr - **xqr;
                  tqi = **xpi - **xqi;
                  
                  /* exchange register with temp */
                  **xpr = tpr;
                  **xpi = tpi;
                  **xqr = tqr;
                  **xqi = tqi;

                  /* next set of register for calculations: */
                  xpr++; xpi++; xqr++; xqi++; cr++; ci++;

         }

         for (i = 0; i < cnt; i++) {
                  
                  /* mult X(q) by coeff C */
                  tqr = **xqr * *cr - **xqi * *ci;
                  tqi = **xqr * *ci + **xqi * *cr;

                  /* exchange register with temp */
                  **xqr = tqr;
                  **xqi = tqi;

                  /* add X(p) and X(q) */
                  tpr = **xpr + **xqr;
                  tpi = **xpi + **xqi;
                  tqr = **xpr - **xqr;
                  tqi = **xpi - **xqi;
                  
                  /* exchange register with temp */
                  **xpr = tpr;
                  **xpi = tpi;
                  **xqr = tqr;
                  **xqi = tqi;
                  /* next set of register for calculations: */
                  xpr++; xpi++; xqr++; xqi++; cr++; ci++;
         }
}


/****************************************************************************/
/* SUPPORT MODULES                                                          */
/****************************************************************************/

void net_alloc(FFT_NET *fft_net)


/* effects: Allocates appropriate two dimensional arrays and assigns
           correct internal pointers.
*/

{

         int      stages, bps, n;

         n      = fft_net->n;
         stages = fft_net->stages;
         bps    = fft_net->bps;


         /* two dimensional arrays with elements stored sequentially */

         fft_net->load_index  = (int *)malloc(n * INT_SIZE);
         fft_net->regr        = (SAMPLE *)malloc(n * SAMPLE_SIZE);
         fft_net->regi        = (SAMPLE *)malloc(n * SAMPLE_SIZE);
         fft_net->coeffr      = (SAMPLE *)malloc(stages*bps*SAMPLE_SIZE);
         fft_net->coeffi      = (SAMPLE *)malloc(stages*bps*SAMPLE_SIZE);
         fft_net->inv_coeffr  = (SAMPLE *)malloc(stages*bps*SAMPLE_SIZE);
         fft_net->inv_coeffi  = (SAMPLE *)malloc(stages*bps*SAMPLE_SIZE);
         fft_net->indexpr     = (SAMPLE **)malloc(stages * bps * PNTR_SIZE);
         fft_net->indexpi     = (SAMPLE **)malloc(stages * bps * PNTR_SIZE);
         fft_net->indexqr     = (SAMPLE **)malloc(stages * bps * PNTR_SIZE);
         fft_net->indexqi     = (SAMPLE **)malloc(stages * bps * PNTR_SIZE);

         /* one dimensional load window */
         fft_net->window      = (SAMPLE *)malloc(n * SAMPLE_SIZE);
         fft_net->inv_window  = (SAMPLE *)malloc(n * SAMPLE_SIZE);
}

void net_dealloc(FFT_NET *fft_net)


/* effects: Deallocates given FFT network.
*/

{

         free((char *)fft_net->load_index);  
         free((char *)fft_net->regr);        
         free((char *)fft_net->regi);        
         free((char *)fft_net->coeffr);      
         free((char *)fft_net->coeffi);      
         free((char *)fft_net->inv_coeffr);  
         free((char *)fft_net->inv_coeffi);  
         free((char *)fft_net->indexpr);     
         free((char *)fft_net->indexpi);     
         free((char *)fft_net->indexqr);   
         free((char *)fft_net->indexqi);   
         free((char *)fft_net->window);
         free((char *)fft_net->inv_window);
}


BOOL power_of_two(n)

int               n;

/* effects: Returns TRUE if n is a power of two, otherwise FALSE.
*/

{
         int      i;

         for (i = n; i > 1; i >>= 1) 
                  if (i & 1) return FALSE;        /* more than one bit high */
         return TRUE;
}


void create_hanning(SAMPLE *window, int n, SAMPLE scale)

/* effects: Fills the buffer window with a hanning window of the appropriate
         size scaled by scale.
*/

{
         SAMPLE     a, pi_div_n = PI/n;
         int        k;

         for (k=1; k <= n; k++) {
                  a = sin(k * pi_div_n);
                  *window++ = scale * a * a;
         }
}


void create_rectangular(SAMPLE *window, int n, SAMPLE scale)

/* effects: Fills the buffer window with a rectangular window of the
   appropriate size of height scale.
*/

{
         while (n--)
           *window++ = scale;
}


void short_to_float(short *short_buf, SAMPLE *float_buf, int n)

/* effects; Converts short_buf to floats and stores them in float_buf.
*/

{
         while (n--) {
                  *float_buf++ = (SAMPLE)*short_buf++;
         }
}


/* here's the meat: */

void pd_fft(t_float *buf, int npoints, int inverse)
{
  double renorm;
  SAMPLE *fp, *fp2;
  int i;
  renorm = (inverse ? npoints : 1.);
  cfft((inverse ? INVERSE : FORWARD), npoints, RECTANGULAR, 
       buf, RECT, LINEAR, buf, RECT, LINEAR, 0);
  for (i = npoints << 1, fp = buf; i--; fp++) *fp *= renorm;
}
