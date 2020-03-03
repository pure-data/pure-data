/* Copyright (c) 1997-1999 Miller Puckette. Updated 2020 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "d_soundfile.h"

/* read raw lpcm (16 or 24 bit int) or 32 bit float samples without header */

/* ------------------------- RAW ------------------------- */

static int raw_readheader(t_soundfile *sf)
{
        /* interpret data size from file size */
    ssize_t bytelimit = lseek(sf->sf_fd, 0, SEEK_END);
    if (bytelimit > SFMAXBYTES || bytelimit < 0)
        bytelimit = SFMAXBYTES;

        /* copy sample format back to caller */
    sf->sf_bytelimit = bytelimit;

#ifdef DEBUG_SOUNDFILE
    post("raw %ld", bytelimit);
#endif

    return 1;
}

void soundfile_raw_setup(t_soundfile_type *type)
{
    t_soundfile_type raw = {
        gensym("raw"),
        0,
        NULL, /* data */
        soundfile_type_open,
        soundfile_type_close,
        raw_readheader,
        NULL, /* writeheaderfn */
        NULL, /* updateheaderfn */
        NULL, /* hasextensionfn */
        NULL, /* addextensionfn */
        soundfile_type_seektoframe,
        soundfile_type_readsamples,
        NULL, /* writesamplesfn */
        NULL, /* isheaderfn */
        NULL, /* endiannessfn */
        NULL, /* readmetafn */
        NULL, /* writemetafn */
        NULL  /* strerrorfn */
    };
    memcpy(type, &raw, sizeof(t_soundfile_type));
}
