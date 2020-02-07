/* Copyright (c) 1997-1999 Miller Puckette. Updated 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* refs: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/AU/AU.html
         https://pubs.opengroup.org/external/auformat.html
         http://sox.sourceforge.net/AudioFormats-11.html#ss11.2
         http://soundfile.sapp.org/doc/NextFormat/ */

#include "d_soundfile.h"
#include "s_stuff.h" /* for sys_verbose */

/* NeXTStep/Sun sound header structure

    * header and sound data can be big or little endian, depending on id:
      - ".snd" big endian
      - "dns." little endian
    * sound data length can be set to an "unknown size", in which case the
      actual length is file size - sound data offset
    * info string after 24 byte header is optional
      - null terminated
      - string len is sound data len - 24, min len is 4
    * can be headerless, in which case is 8 bit u-law at 8000 Hz

  this implementation:

    * does not support headerless files
    * supports big and little endian, system endianess used by default
    * sets a default info string: "Pd "
    * tries to set sound data length, otherwise falls back to "unknown size"
    * sample format: 16 and 24 bit lpcm, 32 bit float, no 32 bit lpcm

*/

/* TODO: support 32 bit int format? */

    /* explicit byte sizes, sizeof(struct) may return alignment padded values */
#define NEXTHDRSIZE 28 /**< min valid header size + info string */

#define NEXT_FORMAT_LINEAR_16 3 /**< 16 bit int */
#define NEXT_FORMAT_LINEAR_24 4 /**< 24 bit int */
#define NEXT_FORMAT_FLOAT     6 /**< 32 bit float */

#define NEXT_UNKNOWN_SIZE 0xffffffff /** aka -1 */

typedef struct _nextstep
{
    char ns_id[4];          /**< magic number; ".snd" (big endian) or "dns." */
    uint32_t ns_onset;      /**< offset to sound data in bytes */
    uint32_t ns_length;     /**< length of sound data in bytes */
    uint32_t ns_format;     /**< sound data format code; see below */
    uint32_t ns_samplerate; /**< sample rate */
    uint32_t ns_nchannels;  /**< number of channels */
    char ns_info[4];        /**< info string, minimum 4 bytes up to onset */
} t_nextstep;

/* ----- helpers ----- */

    /** returns 1 if big endian, 0 if little endian, or -1 for bad header */
static int next_isbigendian(const t_nextstep *next)
{
    if (!strncmp(next->ns_id, ".snd", 4))
        return 1;
    else if(!strncmp(next->ns_id, "dns.", 4))
        return 0;
    return -1;
}

    /** returns the number of bytes per sample for a given format code
        or 0 if the format is not supported */
static int next_bytespersample(int format)
{
    switch (format)
    {
        case NEXT_FORMAT_LINEAR_16: return 2;
        case NEXT_FORMAT_LINEAR_24: return 3;
        case NEXT_FORMAT_FLOAT:     return 4;
        default: return 0;
    }
}

    /** post head info for debugging */
static void next_posthead(const t_nextstep *next, int swap)
{
    uint32_t onset = swap4(next->ns_onset, swap),
             format = swap4(next->ns_format, swap),
             datasize = swap4(next->ns_length, swap),
             channels = swap4(next->ns_nchannels, swap),
             frames = (channels < 1 ? 0 : datasize / channels);
    int bytespersample = next_bytespersample(format);
    frames = (bytespersample < 1 ? 0 : frames / bytespersample);
    post("NEXT %.4s (%s)", next->ns_id,
        (next_isbigendian(next) ? "big" : "little"));
    post("  data onset %d", onset);
    post("  data length %d (frames %ld)", datasize,
        (datasize == NEXT_UNKNOWN_SIZE ? -1 : frames));
    switch (format)
    {
        case NEXT_FORMAT_LINEAR_16:
            post("  format %d (16 bit int)", format);
            break;
        case NEXT_FORMAT_LINEAR_24:
            post("  format %d (24 bit int)", format);
            break;
        case NEXT_FORMAT_FLOAT:
            post("  format %d (32 bit float)", format);
            break;
        default:
            post("  format %d (unsupported)", format);
            break;
    }
    post("  sample rate %d", swap4(next->ns_samplerate, swap));
    post("  channels %d", channels);
    post("  info \"%.4s%s\"",
        next->ns_info, (onset > NEXTHDRSIZE ? "..." : ""));
}

/* ------------------------- NEXT ------------------------- */

int soundfile_next_headersize()
{
    return NEXTHDRSIZE - 4; /* without info string */
}

int soundfile_next_isheader(const char *buf, size_t size)
{
    if (size < 4) return 0;
    if (!strncmp(buf, ".snd", 4) || !strncmp(buf, "dns.", 4))
        return 1;
    return 0;
}

int soundfile_next_readheader(int fd, t_soundfile_info *info)
{
    int format, bytespersample, bigendian = 1, swap = 0;
    off_t headersize = NEXTHDRSIZE;
    size_t bytelimit;
    union
    {
        char b_c[SFHDRBUFSIZE];
        t_nextstep b_nextstep;
    } buf = {0};
    t_nextstep *next = &buf.b_nextstep;

    if (soundfile_readbytes(fd, 0, buf.b_c, headersize) < headersize)
        return 0;

    bigendian = next_isbigendian(next);
    if (bigendian < 0)
        return 0;
    swap = (bigendian != sys_isbigendian());

    headersize = swap4(next->ns_onset, swap);
    if (headersize < NEXTHDRSIZE - 4) /* min valid header size */
        return 0;

    bytelimit = swap4(next->ns_length, swap);
    if (bytelimit == NEXT_UNKNOWN_SIZE)
        bytelimit = SFMAXBYTES;

    format = swap4(next->ns_format, swap);
    bytespersample = next_bytespersample(format);
    if (!bytespersample)
    {
        error("NEXT unsupported format %d", format);
        return 0;
    }

    if (sys_verbose)
        next_posthead(next, swap);

        /* copy sample format back to caller */
    info->i_samplerate = swap4(next->ns_samplerate, swap);
    info->i_nchannels = swap4(next->ns_nchannels, swap);
    info->i_bytespersample = bytespersample;
    info->i_headersize = headersize;
    info->i_bytelimit = bytelimit;
    info->i_bigendian = bigendian;
    info->i_bytesperframe = info->i_nchannels * bytespersample;

    return 1;
}

int soundfile_next_writeheader(int fd, const t_soundfile_info *info,
    size_t nframes)
{
    int swap = soundfile_info_swap(info);
    size_t datasize = nframes * info->i_bytesperframe;
    off_t headersize = NEXTHDRSIZE;
    ssize_t byteswritten = 0;
    t_nextstep next = {
        ".snd",                                    /* id          */
        swap4((uint32_t)headersize, swap),         /* data onset  */
        swap4((uint32_t)datasize, swap),           /* data length */
        0,                                         /* format      */
        swap4((uint32_t)info->i_samplerate, swap), /* sample rate */
        swap4((uint32_t)info->i_nchannels, swap),  /* channels    */
        "Pd "                                      /* info string */
    };

    if (!info->i_bigendian)
        swapstring(next.ns_id, 1);
    switch (info->i_bytespersample)
    {
        case 2:
            next.ns_format = swap4(NEXT_FORMAT_LINEAR_16, swap);
            break;
        case 3:
            next.ns_format = swap4(NEXT_FORMAT_LINEAR_24, swap);
            break;
        case 4:
            next.ns_format = swap4(NEXT_FORMAT_FLOAT, swap);
            break;
        default: /* unsupported format */
            return 0;
    }

    if (sys_verbose)
        next_posthead(&next, swap);

    byteswritten = soundfile_writebytes(fd, 0, (char *)&next, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

    /** the data length is limited to 4 bytes, so if the size is too large,
        do it the lazy way: just set the size field to "unknown size" */
int soundfile_next_updateheader(int fd, const t_soundfile_info *info,
    size_t nframes)
{
    int swap = soundfile_info_swap(info);
    size_t datasize = nframes * info->i_bytesperframe;

    if (datasize > UINT_MAX)
        datasize = NEXT_UNKNOWN_SIZE;
    datasize = swap4(datasize, swap);
    if (soundfile_writebytes(fd, 8, (char *)&datasize, 4) < 4)
        return 0;

    if (sys_verbose)
    {
        datasize = swap4(datasize, swap);
        post("NEXT %.4s (%s)", (info->i_bigendian ? ".snd" : "dns."),
            (info->i_bigendian ? "big" : "little"));
        post("  data length %d (frames %ld)", datasize, nframes);
    }

    return 1;
}

int soundfile_next_hasextension(const char *filename, size_t size)
{
    int len = strnlen(filename, size);
    if (len >= 4 &&
        (!strncmp(filename + (len - 3), ".au", 3) ||
         !strncmp(filename + (len - 3), ".AU", 3)))
        return 1;
    if (len >= 5 &&
        (!strncmp(filename + (len - 4), ".snd", 4) ||
         !strncmp(filename + (len - 4), ".SND", 4)))
        return 1;
    return 0;
}
