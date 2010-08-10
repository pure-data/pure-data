/*
 * Copyright (c) 2010 Peter Brinkmann (peter.brinkmann@gmail.com)
 *
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#ifdef USEAPI_DUMMY

#include <stdio.h>

int dummy_open_audio(int nin, int nout, int sr) {
  return 0;
}

int dummy_close_audio() {
  return 0;
}

int dummy_send_dacs() {
  return 0;
}

void dummy_getdevs(char *indevlist, int *nindevs, char *outdevlist,
    int *noutdevs, int *canmulti, int maxndev, int devdescsize) {
  sprintf(indevlist, "NONE");
  sprintf(outdevlist, "NONE");
  *nindevs = *noutdevs = 1;
  *canmulti = 0;
}

void dummy_listdevs() {
  // do nothing
}

#endif

