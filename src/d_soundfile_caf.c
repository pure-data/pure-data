/* Copyright (c) 2020 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* ref: https://developer.apple.com/library/archive/documentation/MusicAudio/Reference/CAFSpec/CAF_spec/CAF_spec.html */

#include "d_soundfile.h"

#ifndef _MSC_VER
#include <inttypes.h>
#endif

/* CAF (Core Audio Format)

  * RIFF variant with sections split into data "chunks"
  * chunk sizes do not include the chunk id or size (- 12)
  * chunk data is big-endian
  * sound data can be big or little endian (set in desc fmtflags)
  * header is immediately followed by description chunk
  * data chunk:
     - size > 0: can be anywhere after the description chunk
     - size -1: last chunk in the file,
                size = file size - (data chunk pos + 12)

  this implementation:

  * supports CAF version 1 only
  * implements chunks: description, data
  * ignores chunks: packet table, magic cookie, strings, marker, region,
                    instrument, MIDI, overview, peak, edit comments,
                    information, unique material identifier, user-defined
  * ignores any chunks after finding the data chunk
  * sample format: 16 and 24 bit lpcm, 32 bit float, no 32 bit lpcm

  Pd versions < 0.51 did *not* read or write CAF files.

*/

    /* explicit byte sizes, sizeof(struct) can return alignment padded values */
#define CAFHEADSIZE     8 /**< chunk header only */
#define CAFCHUNKSIZE   12 /**< chunk header and format */
#define CAFDESCSIZE    44 /**< chunk header and data */
#define CAFDATASIZE    16 /**< chunk header and edit count data */

#define CAFMAXBYTES SFMAXBYTES /** max signed 64 bit size */

#define CAF_UNKNOWN_SIZE 0xffffffffffffffff /** aka -1 */

    /** Apple-style description format flags */
enum {
    kCAFLinearPCMFormatFlagIsFloat        = (1L << 0),
    kCAFLinearPCMFormatFlagIsLittleEndian = (1L << 1)
};

    /** basic chunk header, 12 bytes
        note: size is split to avoid struct alignment padding */
typedef struct _chunk {
    char c_id[4];                /**< chunk id                        */
    uint8_t c_size[8];           /**< chunk data length, int64_t      */
} t_chunk;

    /** file head container chunk, 8 bytes */
typedef struct _head {
    char h_id[4];                /**< file id "caff"                  */
    uint16_t h_version;          /**< file version, probably 1        */
    uint16_t h_flags;            /**< format flags, 0 for CAF v1      */
} t_head;

    /** description chunk, 44 bytes
        note: size and samplerate are split to avoid struct alignment padding */
typedef struct _descchunk {
    char ds_id[4];               /**< chunk id "desc"                 */
    uint8_t ds_size[8];          /**< chunk data length, int64_t      */
    uint8_t ds_samplerate[8];    /**< sample rate, double             */
    char ds_fmtid[4];            /**< format id, 4 letter char code   */
    uint32_t ds_fmtflags;        /**< format flags, set 0 for "none"  */
    uint32_t ds_bytesperpacket;  /**< bytes per packet                */
    uint32_t ds_framesperpacket; /**< uncompressed: 1 pckt = 1 frame  */
    uint32_t ds_nchannels;       /**< number of channels              */
    uint32_t ds_bitsperchannel;  /**< bits per chan in 1 sample frame */
} t_descchunk;

    /** data chunk, min 16 bytes
        note: size is split to avoid struct alignment padding */
typedef struct _datachunk {
    char dc_id[4];               /**< chunk id "data"                 */
    uint8_t dc_size[8];          /**< chunk data length, int64_t      */
    uint32_t dc_editcount;       /**< edit count, set 0 for none      */
} t_datachunk;

/* ----- helpers ----- */

static int64_t caf_getchunksize(const t_chunk *chunk, int swap)
{
    int64_t size = 0;
    memcpy(&size, chunk->c_size, 8);
    return swap8s(size, swap);
}

static void caf_setchunksize(t_chunk *chunk, int64_t size, int swap)
{
    size = swap8s(size, swap);
    memcpy(chunk->c_size, &size, 8);
}

static double caf_getsamplerate(const t_descchunk *desc, int swap)
{
    double sr = 0;
    memcpy(&sr, desc->ds_samplerate, 8);
    swapstring8((char *)&sr, swap);
    return sr;
}

static void caf_setsamplerate(t_descchunk *desc, double sr, int swap)
{
    memcpy(desc->ds_samplerate, &sr, 8);
    swapstring8((char *)desc->ds_samplerate, swap);
}

    /** read first chunk, returns filled chunk and offset on success or -1 */
static off_t caf_firstchunk(const t_soundfile *sf, t_chunk *chunk)
{
    if (fd_read(sf->sf_fd, CAFHEADSIZE, (char *)chunk,
        CAFCHUNKSIZE) < CAFCHUNKSIZE)
        return -1;
    return CAFHEADSIZE;
}

    /** read next chunk, chunk should be filled when calling
        returns fills chunk offset on success or -1 */
static off_t caf_nextchunk(const t_soundfile *sf, off_t offset, t_chunk *chunk)
{
    int64_t chunksize = caf_getchunksize(chunk, !sys_isbigendian());
    off_t seekto = offset + CAFCHUNKSIZE + chunksize;
    if (seekto & 1) /* pad up to even number of bytes */
        seekto++;
    if (fd_read(sf->sf_fd, seekto, (char *)chunk,
        CAFCHUNKSIZE) < CAFCHUNKSIZE)
        return -1;
    return seekto;
}

#ifdef DEBUG_SOUNDFILE

    /** post head info for debugging */
static void caf_posthead(const t_head *head, int swap)
{
    post("%.4s", head->h_id);
    post("  version %d", swap2(head->h_version, swap));
    post("  flags %d", swap2(head->h_flags, swap));
}

    /** post chunk info for debugging */
static void caf_postchunk(const t_chunk *chunk, int swap)
{
    post("%.4s %" PRId64, chunk->c_id, caf_getchunksize(chunk, swap));
}

    /** post desc info for debugging */
static void caf_postdesc(const t_descchunk *desc, int swap)
{
    uint32_t fmtflags = swap4(desc->ds_fmtflags, swap);
    caf_postchunk((t_chunk *)desc, swap);
    post("  sample rate %g", caf_getsamplerate(desc, swap));
    post("  fmt id %.4s", desc->ds_fmtid);
    post("  fmt flags %d (%s %s)", fmtflags,
        (fmtflags & kCAFLinearPCMFormatFlagIsLittleEndian ? "little" : "big"),
        (fmtflags & kCAFLinearPCMFormatFlagIsFloat ? "float" : "int"));
    post("  bytes per packet %d", swap4(desc->ds_bytesperpacket, swap));
    post("  frames per packet %d", swap4(desc->ds_framesperpacket, swap));
    post("  channels %d", swap4(desc->ds_nchannels, swap));
    post("  bits per channel %d", swap4(desc->ds_bitsperchannel, swap));
}

    /** post data header info for debugging, currently edit count only */
static void caf_postdata(const t_datachunk *data, int swap)
{
    caf_postchunk((t_chunk *)data, swap);
    post("  edit count %d", swap4(data->dc_editcount, swap));
}

#endif /* DEBUG_SOUNDFILE */

/* ------------------------- CAF -------------------------- */

static int caf_isheader(const char *buf, size_t size)
{
    if (size < 4) return 0;
    return !strncmp(buf, "caff", 4);
}

static int caf_readheader(t_soundfile *sf)
{
    int nchannels = 1, bytespersample = 2, samplerate = 44100, bigendian = 1,
        fmtflags, swap = !sys_isbigendian();
    off_t headersize = CAFHEADSIZE + CAFDESCSIZE;
    ssize_t bytelimit = CAFMAXBYTES;
    union
    {
        char b_c[SFHDRBUFSIZE];
        t_head b_head;
        t_chunk b_chunk;
        t_descchunk b_descchunk;
        t_datachunk b_datachunk;
    } buf = {0};
    t_head *head = &buf.b_head;
    t_chunk *chunk = &buf.b_chunk;
    t_descchunk *desc = &buf.b_descchunk;

        /* file header */
    if (fd_read(sf->sf_fd, 0, buf.b_c, headersize) < headersize)
        return 0;
    if (strncmp(head->h_id, "caff", 4))
        return 0;
    if (swap2(head->h_version, swap) != 1)
    {
        errno = SOUNDFILE_ERRVERSION;
        return 0;
    }
    if (swap2(head->h_flags, swap) != 0)
    {
            /* current spec says these should be empty */
        errno = SOUNDFILE_ERRVERSION;
        return 0;
    }
#ifdef DEBUG_SOUNDFILE
    caf_posthead(head, swap);
#endif

        /* copy the first chunk header to beginning of buffer,
           use memmove as src & dst are the same */
    memmove(buf.b_c, buf.b_c + CAFHEADSIZE, CAFDESCSIZE);
    headersize = CAFHEADSIZE;

        /* first chunk must be description */
    if (strncmp(desc->ds_id, "desc", 4))
        return 0;
#ifdef DEBUG_SOUNDFILE
    caf_postdesc(desc, swap);
#endif
    if (strncmp(desc->ds_fmtid, "lpcm", 4))
    {
        errno = SOUNDFILE_ERRSAMPLEFMT;
        return 0;
    }
    nchannels = swap4(desc->ds_nchannels, swap);
    fmtflags = swap4(desc->ds_fmtflags, swap);
    bytespersample = swap4(desc->ds_bitsperchannel, swap) / 8;
    switch (bytespersample)
    {
        case 2: case 3: case 4: break;
        default:
            errno = SOUNDFILE_ERRSAMPLEFMT;
            return 0;
    }
    if (bytespersample == 4 && !(fmtflags & kCAFLinearPCMFormatFlagIsFloat))
    {
        errno = SOUNDFILE_ERRSAMPLEFMT;
        return 0;
    }
    bigendian = !(fmtflags & kCAFLinearPCMFormatFlagIsLittleEndian);
    samplerate = caf_getsamplerate(desc, swap);

        /* read chunks in loop until we find the sound data chunk */
    if ((headersize = caf_nextchunk(sf, headersize, chunk)) == -1)
        return 0;
    while (1)
    {
        int64_t chunksize = caf_getchunksize(chunk, swap);
        off_t seekto = headersize + CAFCHUNKSIZE + chunksize, seekout;
        if (seekto & 1) /* pad up to even number of bytes */
            seekto++;
        /* post("chunk %.4s seek %d", chunk->c_id, seekto); */
        if (!strncmp(chunk->c_id, "data", 4))
        {
                /* sound data chunk */
#ifdef DEBUG_SOUNDFILE
            t_datachunk *data = &buf.b_datachunk;
            if (fd_read(sf->sf_fd, headersize + CAFCHUNKSIZE,
                    buf.b_c + CAFCHUNKSIZE, 4) < 4)
                return 0;
            caf_postdata(data, swap);
#endif
            headersize += CAFDATASIZE;
            if (chunksize == CAF_UNKNOWN_SIZE)
            {
                    /* interpret data size from file size */
                bytelimit = lseek(sf->sf_fd, 0, SEEK_END) - headersize;
                if (bytelimit > CAFMAXBYTES || bytelimit < 0)
                    bytelimit = CAFMAXBYTES;
            }
            else
                bytelimit = chunksize - 4; /* - edit count */
            break;
        }
#ifdef DEBUG_SOUNDFILE
        else
        {
                /* everything else */
            caf_postchunk(chunk, swap);
        }
#endif
        if ((headersize = caf_nextchunk(sf, headersize, chunk)) == -1)
            return 0;
    }

        /* copy sample format back to caller */
    sf->sf_samplerate = samplerate;
    sf->sf_nchannels = nchannels;
    sf->sf_bytespersample = bytespersample;
    sf->sf_headersize = headersize;
    sf->sf_bytelimit = bytelimit;
    sf->sf_bigendian = bigendian;
    sf->sf_bytesperframe = nchannels * bytespersample;

    return 1;
}

static int caf_writeheader(t_soundfile *sf, size_t nframes)
{
    int swap = !sys_isbigendian();
    size_t datasize =
        (nframes > 0 ? nframes * sf->sf_bytesperframe : CAF_UNKNOWN_SIZE);
    off_t headersize = 0;
    ssize_t byteswritten = 0;
    uint32_t uinttmp = 0;
    char buf[SFHDRBUFSIZE] = {0};
    t_head head = {"caff", swap2(1, swap), 0};
    t_descchunk desc = {"desc", {0}, {0}};
    t_datachunk data = {"data", {0}, 0};

        /* file header */
    memcpy(buf + headersize, &head, CAFHEADSIZE);
    headersize += CAFHEADSIZE;

        /* description chunk */
    caf_setchunksize((t_chunk *)&desc, CAFDESCSIZE - CAFCHUNKSIZE, swap);
    caf_setsamplerate(&desc, sf->sf_samplerate, swap);
    strncpy(desc.ds_fmtid, "lpcm", 4);
    if (sf->sf_bytespersample == 4)
        uinttmp |= kCAFLinearPCMFormatFlagIsFloat;
    if (!sf->sf_bigendian)
        uinttmp |= kCAFLinearPCMFormatFlagIsLittleEndian;
    desc.ds_fmtflags = swap4(uinttmp, swap);
    desc.ds_bytesperpacket = swap4(sf->sf_bytesperframe, swap);
    desc.ds_framesperpacket = swap4(1, swap);
    desc.ds_nchannels = swap4(sf->sf_nchannels, swap);
    desc.ds_bitsperchannel = swap4(sf->sf_bytespersample * 8, swap);
    memcpy(buf + headersize, &desc, CAFDESCSIZE);
    headersize += CAFDESCSIZE;

        /* data chunk (+ edit count) */
    caf_setchunksize((t_chunk *)&data, datasize + 4, swap);
    memcpy(buf + headersize, &data, CAFDATASIZE);
    headersize += CAFDATASIZE;

#ifdef DEBUG_SOUNDFILE
    caf_posthead(&head, swap);
    caf_postdesc(&desc, swap);
    caf_postdata(&data, swap);
#endif

    byteswritten = fd_write(sf->sf_fd, 0, buf, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

    /** assumes chunk order: head desc data */
static int caf_updateheader(t_soundfile *sf, size_t nframes)
{
    int swap = !sys_isbigendian();
    int64_t datasize = swap8s((nframes * sf->sf_bytesperframe) + 4, swap);

        /* data chunk size (+ edit count) */
    if (fd_write(sf->sf_fd, CAFHEADSIZE + CAFDESCSIZE + 4, &datasize, 8) < 8)
        return 0;

#ifdef DEBUG_SOUNDFILE
    post("caff");
    post("data %" PRId64, swap8s(datasize, swap));
#endif

    return 1;
}

static int caf_hasextension(const char *filename, size_t size)
{
    int len = strnlen(filename, size);
    if (len >= 5 &&
        (!strncmp(filename + (len - 4), ".caf", 4) ||
         !strncmp(filename + (len - 4), ".CAF", 4)))
        return 1;
    return 0;
}

static int caf_addextension(char *filename, size_t size)
{
    int len = strnlen(filename, size);
    if (len + 4 >= size)
        return 0;
    strcpy(filename + len, ".caf");
    return 1;
}

    /* default to big endian if not specified */
static int caf_endianness(int endianness)
{
    if (endianness == -1)
        return 1;
    return endianness;
}

/* ------------------------- setup routine ------------------------ */

static const t_soundfile_type caf = {
    "caf",
    CAFHEADSIZE + CAFDESCSIZE + CAFDATASIZE,
    caf_isheader,
    caf_readheader,
    caf_writeheader,
    caf_updateheader,
    caf_hasextension,
    caf_addextension,
    caf_endianness
};

void soundfile_caf_setup( void)
{
    soundfile_addtype(&caf);
}
