/* Copyright (c) 2006 Guenter Geiger,
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#ifdef USEAPI_ESD

#include <stdio.h>
#include <string.h> /* memset */
#include <esd.h>
#include "m_pd.h"
#include "s_stuff.h"
#include "m_fixed.h"

/* exported variables */

float sys_dacsr;


/* private data */

static int esd_socket_out;
static int esd_channels_out;

static int esd_socket_in;
static int esd_channels_in;


int esd_open_audio(int nindev,  int *indev,  int nchin,  int *chin,
    int noutdev, int *outdev, int nchout, int *chout, int rate)
{

    esd_format_t format = ESD_BITS16 | ESD_STEREO | ESD_STREAM | ESD_PLAY;

    indev[0] = 0;
    nindev = 1;

    outdev[0] = 0;
    noutdev = 1;

    rate = sys_dacsr = ESD_DEFAULT_RATE;

    if (*chout == 2) {
      esd_socket_out = esd_play_stream_fallback(format, ESD_DEFAULT_RATE, NULL, "pd");
      if (esd_socket_out <= 0) {
        fprintf (stderr, "Error at esd open: %d\n", esd_socket_out);
        esd_channels_out = *chout = 0;
        return 0;
      }
      esd_channels_out = *chout = 2;
    }

    if (*chin == 2) {
      esd_socket_in = esd_record_stream_fallback(format, ESD_DEFAULT_RATE, NULL, "pd-in");
      if (esd_socket_in <= 0) {
        fprintf (stderr, "Error at esd open: %d\n", esd_socket_in);
        esd_channels_in = *chin = 0;
        return 0;
      }
      esd_channels_in = *chin = 2;
    }
    return (0);
}

void esd_close_audio( void)
{
  close(esd_socket_out);
  close(esd_socket_in);
}


#define DEFDACBLKSIZE 64
#define MAXCHANS 2

static short buf[DEFDACBLKSIZE*MAXCHANS];

int esd_send_dacs(void)
{
  t_sample* fp1,*fp2;
  short* sp;
  int i,j;

  /* Do input */

  if (esd_channels_in) {
    read(esd_socket_in,buf,DEFDACBLKSIZE*esd_channels_out*2);
    for (i = DEFDACBLKSIZE,  fp1 = sys_soundin,
                 sp = (short *)buf; i--; fp1++, sp += esd_channels_in) {
      for (j=0, fp2 = fp1; j<esd_channels_in; j++, fp2 += DEFDACBLKSIZE)
        {
          int s = INVSCALE16(sp[j]);
          *fp2 = s;
        }
    }
  }

  /* Do output */

  if (esd_channels_out) {
    for (i = DEFDACBLKSIZE,  fp1 = sys_soundout,
         sp = (short *)buf; i--; fp1++, sp += esd_channels_out) {
      for (j=0, fp2 = fp1; j<esd_channels_out; j++, fp2 += DEFDACBLKSIZE)
        {
          int s = SCALE16(*fp2);
          if (s > 32767) s = 32767;
          else if (s < -32767) s = -32767;
          sp[j] = s;
        }
    }

    write(esd_socket_out,buf,DEFDACBLKSIZE*esd_channels_out*2);
  }

  memset(sys_soundin,0,DEFDACBLKSIZE*esd_channels_out*2);
  memset(sys_soundout,0,DEFDACBLKSIZE*esd_channels_out*4);

  return (SENDDACS_YES);
}

void esd_listdevs( void)
{
    post("device listing not implemented for ESD yet\n");
}

void esd_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti,
        int maxndev, int devdescsize)
{
    int i, ndev;
    *canmulti = 1;  /* supports multiple devices */
    sprintf(indevlist, "ESD device");
    sprintf(outdevlist, "ESD device");
    *nindevs = *noutdevs = 1;
}

#endif
