/* Copyright (c) 2020 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* ref: https://developer.apple.com/library/archive/documentation/MusicAudio/Reference/CAFSpec/CAF_spec/CAF_spec.html */

#include "d_soundfile.h"
#include "s_stuff.h" /* for sys_verbose */
#include <math.h>
#include <inttypes.h>

/* CAFF header (Core Audio File Format)

  * RIFF variant with sections split into data "chunks"
  * chunk data is big-endian
  * sound data can be big or little endian (set in desc fmtflags)
  * header is immediately followed by description chunk
  * data chunk:
     - size > 0: can be anywhere after the description chunk
     - size -1: last chunk in the file,
                size = file size - (data chunk pos + 12)

  this implementation:

  * supports CAFF version 1 only
  * implements chunks: description, data
  * ignores chunks: packet table, magic cookie, strings, marker, region,
                    instrument, MIDI, overview, peak, edit comments,
                    information, unique material identifier, user-defined
  * ignores any chunks after finding the data chunk
  * sample format: 16 and 24 bit lpcm, 32 bit float, no 32 bit lpcm

*/

    /* explicit byte sizes, sizeof(struct) can return alignment padded values */
#define CAFFHEADSIZE     8 /**< chunk header only */
#define CAFFCHUNKSIZE   12 /**< chunk header and format */
#define CAFFDESCSIZE    44 /**< chunk header and data */
#define CAFFDATASIZE    16 /**< chunk header and edit count data */

#define CAFFMAXBYTES SFMAXBYTES /** max signed 64 bit size */

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

    /** file header, 8 bytes */
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
    uint32_t ds_framesperpacket; /**< for lpcm, 1 packet = 1 frame    */
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

static int64_t caff_getchunksize(const t_chunk *chunk, int swap)
{
    int64_t size = 0;
    memcpy(&size, chunk->c_size, 8);
    return swap8s(size, swap);
}

static void caff_setchunksize(t_chunk *chunk, int64_t size, int swap)
{
    size = swap8s(size, swap);
    memcpy(chunk->c_size, &size, 8);
}

static double caff_getsamplerate(const t_descchunk *desc, int swap)
{
    double sr = 0;
    memcpy(&sr, desc->ds_samplerate, 8);
    swapstring8((char *)&sr, swap);
    return sr;
}

static void caff_setsamplerate(t_descchunk *desc, double sr, int swap)
{
    memcpy(desc->ds_samplerate, &sr, 8);
    swapstring8((char *)desc->ds_samplerate, swap);
}

    /** post head info for debugging */
static void caff_posthead(const t_head *head, int swap)
{
    post("%.4s", head->h_id);
    post("  version %d", swap2(head->h_version, swap));
    post("  flags %d", swap2(head->h_flags, swap));
}

    /** post chunk info for debugging */
static void caff_postchunk(const t_chunk *chunk, int swap)
{
    post("%.4s %" PRId64, chunk->c_id, caff_getchunksize(chunk, swap));
}

    /** post desc info for debugging */
static void caff_postdesc(const t_descchunk *desc, int swap)
{
    uint32_t fmtflags = swap4(desc->ds_fmtflags, swap);
    caff_postchunk((t_chunk *)desc, swap);
    post("  sample rate %g", caff_getsamplerate(desc, swap));
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
static void caff_postdata(const t_datachunk *data, int swap)
{
    caff_postchunk((t_chunk *)data, swap);
    post("  edit count %d", swap4(data->dc_editcount, swap));
}

/* ------------------------- CAFF ------------------------- */

int soundfile_caff_headersize()
{
    return CAFFHEADSIZE + CAFFDESCSIZE + CAFFDATASIZE;
}

int soundfile_caff_isheader(const char *buf, size_t size)
{
    if (size < 4) return 0;
    return !strncmp(buf, "caff", 4);
}

int soundfile_caff_readheader(int fd, t_soundfile_info *info)
{
    int nchannels = 1, bytespersample = 2, bigendian = 1,
        fmtflags, bitspersample, swap = !sys_isbigendian();
    double samplerate = 44100;
    off_t headersize = CAFFHEADSIZE + CAFFDESCSIZE;
    ssize_t bytelimit = CAFFMAXBYTES;
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
    if (soundfile_readbytes(fd, 0, buf.b_c, headersize) < headersize)
        return 0;
    if (strncmp(head->h_id, "caff", 4))
        return 0;
    if (swap2(head->h_version, swap) != 1)
    {
        error("CAFF version %d unsupported", swap2(head->h_version, swap));
        return 0;
    }
    if (swap2(head->h_flags, swap) != 0)
    {
            /* current spec says these should be empty */
        error("CAFF version 1 format flags invalid");
        return 0;
    }
    if (sys_verbose)
        caff_posthead(head, swap);

        /* copy the first chunk header to beginning of buffer */
    memcpy(buf.b_c, buf.b_c + CAFFHEADSIZE, CAFFDESCSIZE);
    headersize = CAFFHEADSIZE;

        /* first chunk must be description */
    if (strncmp(desc->ds_id, "desc", 4))
        return 0;
    if (sys_verbose)
        caff_postdesc(desc, swap);
    if (strncmp(desc->ds_fmtid, "lpcm", 4))
    {
        error("CAFF format \"%.4s\" not supported", desc->ds_fmtid);
        return 0;
    }
    nchannels = swap4(desc->ds_nchannels, swap);
    fmtflags = swap4(desc->ds_fmtflags, swap);
    bitspersample = swap4(desc->ds_bitsperchannel, swap);
    switch (bitspersample)
    {
        case 16: bytespersample = 2; break;
        case 24: bytespersample = 3; break;
        case 32: bytespersample = 4; break;
        default:
            error("CAFF %d bit samples not supported ", bitspersample);
            return 0;
    }
    if (bytespersample == 4 && !(fmtflags & kCAFLinearPCMFormatFlagIsFloat))
    {
        error("CAFF 32 bit int not supported");
        return 0;
    }
    bigendian = !(fmtflags & kCAFLinearPCMFormatFlagIsLittleEndian);
    samplerate = caff_getsamplerate(desc, swap);
    headersize += CAFFDESCSIZE;

        /* prepare second chunk */
    if (soundfile_readbytes(fd, headersize, buf.b_c,
                            CAFFCHUNKSIZE) < CAFFCHUNKSIZE)
        return 0;

        /* read chunks in loop until we find the sound data chunk */
    while (1)
    {
        int64_t chunksize = caff_getchunksize(chunk, swap);
        off_t seekto = headersize + chunksize, seekout;
        if (seekto & 1) /* pad up to even number of bytes */
            seekto++;
         post("chunk %.4s seek %d", chunk->c_id, seekto); 
        if (!strncmp(chunk->c_id, "data", 4))
        {
                /* sound data chunk */
            if (sys_verbose)
            {
                t_datachunk *data = &buf.b_datachunk;
                if (soundfile_readbytes(fd, headersize + CAFFCHUNKSIZE,
                                        buf.b_c + CAFFCHUNKSIZE, 4) < 4)
                    return 0;
                caff_postdata(data, swap);
            }
            bytelimit = chunksize - 4; /* subtract edit count */
            headersize += CAFFDATASIZE;
            break;
        }
        else
        {
                /* everything else */
            if (sys_verbose)
                caff_postchunk(chunk, swap);
        }
        headersize = seekto;
        if (soundfile_readbytes(fd, seekto, buf.b_c,
                                CAFFCHUNKSIZE) < CAFFCHUNKSIZE)
            return 0;
    }

        /* copy sample format back to caller */
    info->i_samplerate = samplerate;
    info->i_nchannels = nchannels;
    info->i_bytespersample = bytespersample;
    info->i_headersize = headersize;
    info->i_bytelimit = bytelimit;
    info->i_bigendian = bigendian;
    info->i_bytesperframe = nchannels * bytespersample;

    return 1;
}

int soundfile_caff_writeheader(int fd, const t_soundfile_info *info,
    size_t nframes)
{
    int swap = !sys_isbigendian();
    size_t datasize = nframes * info->i_bytesperframe;
    off_t headersize = 0;
    ssize_t byteswritten = 0;
    uint32_t uinttmp = 0;
    char buf[SFHDRBUFSIZE] = {0};
    t_head head = {"caff", swap2(1, swap), 0};
    t_descchunk desc = {"desc", 0, 0};
    t_datachunk data = {"data", 0, 0};

        /* file header */
    memcpy(buf + headersize, &head, CAFFHEADSIZE);
    headersize += CAFFHEADSIZE;

        /* description chunk */
    caff_setchunksize((t_chunk *)&desc, CAFFDESCSIZE, swap);
    caff_setsamplerate(&desc, info->i_samplerate, swap);
    strncpy(desc.ds_fmtid, "lpcm", 4);
    if (info->i_bytespersample == 4)
        uinttmp |= kCAFLinearPCMFormatFlagIsFloat;
    if (!info->i_bigendian)
        uinttmp |= kCAFLinearPCMFormatFlagIsLittleEndian;
    desc.ds_fmtflags = swap4(uinttmp, swap);
    desc.ds_bytesperpacket = swap4(info->i_bytesperframe, swap);
    desc.ds_framesperpacket = swap4(1, swap);
    desc.ds_nchannels = swap4(info->i_nchannels, swap);
    desc.ds_bitsperchannel = swap4(info->i_bytespersample * 8, swap);
    memcpy(buf + headersize, &desc, CAFFDESCSIZE);
    headersize += CAFFDESCSIZE;

        /* data chunk */
    caff_setchunksize((t_chunk *)&data, datasize + 4, swap);
    memcpy(buf + headersize, &data, CAFFDATASIZE);
    headersize += CAFFDATASIZE;

    if (sys_verbose)
    {
        caff_posthead(&head, swap);
        caff_postdesc(&desc, swap);
        caff_postdata(&data, swap);
    }

    byteswritten = soundfile_writebytes(fd, 0, buf, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

    /** assumes chunk order: head desc data */
int soundfile_caff_updateheader(int fd, const t_soundfile_info *info,
    size_t nframes)
{
    int swap = !sys_isbigendian();
    int64_t datasize = swap8s((nframes * info->i_bytesperframe) + 4, swap);

        /* data chunk size */
    if (soundfile_writebytes(fd, CAFFHEADSIZE + CAFFDESCSIZE + 4, /* dc_id */
                             (char *)&datasize, 8) < 8)
        return 0;

    if (sys_verbose)
    {
        post("caff");
        post("data %" PRId64, swap8s(datasize, swap));
    }

    return 1;
}

int soundfile_caff_hasextension(const char *filename, size_t size)
{
    int len = strnlen(filename, size);
    if (len >= 5 &&
        (!strncmp(filename + (len - 4), ".caf", 4) ||
         !strncmp(filename + (len - 4), ".CAF", 4)))
        return 1;
    return 0;
}
