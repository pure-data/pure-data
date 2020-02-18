/* Copyright (c) 1997-1999 Miller Puckette. Updated 2020 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "d_soundfile.h"
#include "s_stuff.h" /* for sys_verbose */

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

    if (sys_verbose)
        post("raw %ld", bytelimit);

    return 1;
}

void soundfile_raw_setup(t_soundfile_filetype *ft)
{
    t_soundfile_filetype raw = {
        gensym("raw"),
        0,
        NULL, /* isheaderfn */
        soundfile_filetype_open,
        soundfile_filetype_close,
        raw_readheader,
        NULL, /* writeheaderfn */
        NULL, /* updateheaderfn */
        NULL, /* hasextensionfn */
        NULL, /* addextensionfn */
        NULL, /* endiannessfn */
        soundfile_filetype_seektoframe,
        soundfile_filetype_readsamples,
        NULL, /* writesamplesfn */
        NULL
    };
    memcpy(ft, &raw, sizeof(t_soundfile_filetype));
}
