/* Copyright (c) 1997-1999 Miller Puckette. Updated 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* refs: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/AU/AU.html
         https://pubs.opengroup.org/external/auformat.html
         http://sox.sourceforge.net/AudioFormats-11.html#ss11.2
         http://soundfile.sapp.org/doc/NextFormat */

#include "d_soundfile.h"

/* NeXTStep/Sun sound

  * simple header followed by sound data
  * header and sound data can be big or little endian, depending on id:
    - ".snd" big endian
    - "dns." little endian
  * sound data length can be set to an "unknown size", in which case the
    actual length is file size - sound data offset
  * info string after 24 byte header is optional
    - null terminated
    - string len is sound data len - 24, min len is 4
  * can be headerless, in which case is 8 bit u-law at 8000 Hz
  * limited to ~4 GB files as sizes are unsigned 32 bit ints

  this implementation:

    * does not support headerless files
    * supports big and little endian, system endianness used by default
    * sets a default info string: "Pd "
    * tries to set sound data length, otherwise falls back to "unknown size"
    * sample format: 16 and 24 bit lpcm, 32 bit float, no 32 bit lpcm

    Pd versions < 0.51 did *not* write the actual data chunk size when updating
    the header, but set "unknown size" instead.

*/

    /* explicit byte sizes, sizeof(struct) may return alignment padded values */
#define NEXTHEADSIZE 28 /**< min valid header size + info string */

#define NEXTMAXBYTES 0xffffffff /**< max unsigned 32 bit size */

#define NEXT_FORMAT_LINEAR_16 3 /**< 16 bit int */
#define NEXT_FORMAT_LINEAR_24 4 /**< 24 bit int */
#define NEXT_FORMAT_FLOAT     6 /**< 32 bit float */

#define NEXT_UNKNOWN_SIZE 0xffffffff /** aka -1 */

typedef struct _nextstep
{
    char ns_id[4];          /**< magic number; ".snd" (big endian) or "dns." */
    uint32_t ns_onset;      /**< offset to sound data in bytes               */
    uint32_t ns_length;     /**< length of sound data in bytes               */
    uint32_t ns_format;     /**< sound data format code; see below           */
    uint32_t ns_samplerate; /**< sample rate                                 */
    uint32_t ns_nchannels;  /**< number of channels                          */
    char ns_info[4];        /**< info string, minimum 4 bytes up to onset    */
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

#ifdef DEBUG_SOUNDFILE

    /** post head info for debugging */
static void next_posthead(const t_nextstep *next, int swap)
{
    uint32_t onset = swap4(next->ns_onset, swap),
             format = swap4(next->ns_format, swap),
             datasize = swap4(next->ns_length, swap);
    post("next %.4s (%s)", next->ns_id,
        (next_isbigendian(next) ? "big" : "little"));
    post("  data onset %d", onset);
    if (datasize == NEXT_UNKNOWN_SIZE)
        post("  data length -1");
    else
        post("  data length %d", datasize);
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
    post("  channels %d", swap4(next->ns_nchannels, swap));
    post("  info \"%.4s%s\"",
        next->ns_info, (onset > NEXTHEADSIZE ? "..." : ""));
}

#endif /* DEBUG_SOUNDFILE */

/* ------------------------- NEXT ------------------------- */

static int next_isheader(const char *buf, size_t size)
{
    if (size < 4) return 0;
    if (!strncmp(buf, ".snd", 4) || !strncmp(buf, "dns.", 4))
        return 1;
    return 0;
}

static int next_readheader(t_soundfile *sf)
{
    int format, bytespersample, bigendian = 1, swap = 0;
    off_t headersize = NEXTHEADSIZE;
    size_t bytelimit = NEXTMAXBYTES;
    union
    {
        char b_c[SFHDRBUFSIZE];
        t_nextstep b_nextstep;
    } buf = {0};
    t_nextstep *next = &buf.b_nextstep;

    if (fd_read(sf->sf_fd, 0, buf.b_c, headersize) < headersize)
        return 0;

    bigendian = next_isbigendian(next);
    if (bigendian < 0)
        return 0;
    swap = (bigendian != sys_isbigendian());

    headersize = swap4(next->ns_onset, swap);
    if (headersize < NEXTHEADSIZE - 4) /* min valid header w/o info string */
        return 0;

    bytelimit = swap4(next->ns_length, swap);
    if (bytelimit == NEXT_UNKNOWN_SIZE)
    {
            /* interpret data size from file size */
        bytelimit = lseek(sf->sf_fd, 0, SEEK_END) - headersize;
        if (bytelimit > NEXTMAXBYTES || bytelimit < 0)
            bytelimit = NEXTMAXBYTES;
    }

    format = swap4(next->ns_format, swap);
    switch (format)
    {
        case NEXT_FORMAT_LINEAR_16: bytespersample = 2; break;
        case NEXT_FORMAT_LINEAR_24: bytespersample = 3; break;
        case NEXT_FORMAT_FLOAT:     bytespersample = 4; break;
        default:
            errno = SOUNDFILE_ERRSAMPLEFMT;
            return 0;
    }

#ifdef DEBUG_SOUNDFILE
    next_posthead(next, swap);
#endif

        /* copy sample format back to caller */
    sf->sf_samplerate = swap4(next->ns_samplerate, swap);
    sf->sf_nchannels = swap4(next->ns_nchannels, swap);
    sf->sf_bytespersample = bytespersample;
    sf->sf_headersize = headersize;
    sf->sf_bytelimit = bytelimit;
    sf->sf_bigendian = bigendian;
    sf->sf_bytesperframe = sf->sf_nchannels * bytespersample;

    return 1;
}

static int next_writeheader(t_soundfile *sf, size_t nframes)
{
    int swap = soundfile_needsbyteswap(sf);
    size_t datasize =
        (nframes > 0 ? nframes * sf->sf_bytesperframe : NEXT_UNKNOWN_SIZE);
    off_t headersize = NEXTHEADSIZE;
    ssize_t byteswritten = 0;
    t_nextstep next = {
        ".snd",                                   /* id          */
        swap4((uint32_t)headersize, swap),        /* data onset  */
        swap4((uint32_t)datasize, swap),          /* data length */
        0,                                        /* format      */
        swap4((uint32_t)sf->sf_samplerate, swap), /* sample rate */
        swap4((uint32_t)sf->sf_nchannels, swap),  /* channels    */
        "Pd "                                     /* info string */
    };

    if (!sf->sf_bigendian)
        swapstring4(next.ns_id, 1);
    switch (sf->sf_bytespersample)
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

#ifdef DEBUG_SOUNDFILE
    next_posthead(&next, swap);
#endif

    byteswritten = fd_write(sf->sf_fd, 0, &next, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

    /** the data length is limited to 4 bytes, so if the size is too large,
        do it the lazy way: just set the size field to "unknown size" */
static int next_updateheader(t_soundfile *sf, size_t nframes)
{
    int swap = soundfile_needsbyteswap(sf);
    size_t datasize = nframes * sf->sf_bytesperframe;

    if (datasize > NEXTMAXBYTES)
        datasize = NEXT_UNKNOWN_SIZE;
    datasize = swap4(datasize, swap);
    if (fd_write(sf->sf_fd, 8, &datasize, 4) < 4)
        return 0;

#ifdef DEBUG_SOUNDFILE
    datasize = swap4(datasize, swap);
    post("next %.4s (%s)", (sf->sf_bigendian ? ".snd" : "dns."),
        (sf->sf_bigendian ? "big" : "little"));
    if (datasize == NEXT_UNKNOWN_SIZE)
        post("  data length -1");
    else
        post("  data length %d", datasize);
#endif

    return 1;
}

static int next_hasextension(const char *filename, size_t size)
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

static int next_addextension(char *filename, size_t size)
{
    int len = strnlen(filename, size);
    if (len + 4 >= size)
        return 0;
    strcpy(filename + len, ".snd");
    return 1;
}

    /* machine native if not specified */
static int next_endianness(int endianness)
{
    if (endianness == -1)
        return sys_isbigendian();
    return endianness;
}

/* ------------------------- setup routine ------------------------ */

t_soundfile_type next = {
    "next",
    NEXTHEADSIZE - 4, /* - info string */
    next_isheader,
    next_readheader,
    next_writeheader,
    next_updateheader,
    next_hasextension,
    next_addextension,
    next_endianness
};

void soundfile_next_setup( void)
{
    soundfile_addtype(&next);
}
