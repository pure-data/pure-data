/* Copyright (c) 2020 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* ref: https://developer.apple.com/library/archive/documentation/MusicAudio/Reference/CAFSpec/CAF_spec/CAF_spec.html */

#include "d_soundfile.h"
#include "s_stuff.h" /* for sys_verbose */
#include <math.h>
#include <inttypes.h>

/* the CAFF header (Core Audio Format File)

  * chunk data is big-endian
  * audio data can be big or little endian (set in desc fmt_flags)
  * header is immediately followed by audio description chunk
  * audio data chunk:
     - size > 0: can be anywhere after audio description chunk
     - size -1: last chunk in the file,
                size = file size - (audio data chunk pos + 12)

  this implementation:

  * supports CAFF version 1 only
  * implements chunks: audio description, audio data 
  * ignores chunks: packet table, magic cookie, strings, marker, region,
                    instrument, MIDI, overview, peak, edit comments,
                    information, unique material identifier, user-defined
  * ignores any chunks after finding the audio data chunk
  * lpcm samples only, no 32 bit int samples

*/

/* file header, 8 bytes */
typedef struct _caff {
    char c_type[4];                /* file type "caff"                */
    uint16_t c_version;            /* file version, probably 1        */
    uint16_t c_flags;              /* format flags, 0 for CAF v1      */
} t_caff;

/* chunk, 12 bytes */
typedef struct _caff_chunk {
    char cc_type[4];               /* chunk type, 4 letter char code  */
    int64_t cc_size;               /* length of chunk data in bytes   */
} t_caff_chunk;

/* audio description chunk data section, 32 bytes */
typedef struct _caff_desc {
    double cd_samplerate;          /* sample rate, 64 bit float       */
    char cd_fmtid[4];              /* format id, 4 letter char code   */
    uint32_t cd_fmtflags;          /* format flags, set 0 for "none"  */
    uint32_t cd_bytesperpacket;    /* bytes per packet                */
    uint32_t cd_framesperpacket;   /* for lpcm, 1 packet = 1 frame    */
    uint32_t cd_nchannels;         /* number of channels              */
    uint32_t cd_bitsperchannel;    /* bits per chan in 1 sample frame */
} t_caff_desc;

/* audio data chunk data section, min 4 bytes */
typedef struct _caff_data {
    uint32_t cd_editcount;         /* edit count, set 0 for none      */
    uint8_t cd_data[0];            /* audio data starts here          */
} t_caff_data;

/* explicit byte sizes as sizeof(struct) can return alignment padded values */
#define CAFFHDRSIZE      8
#define CAFFCHUNKSIZE   12 /* sizeof(t_caff) returns 16 on 64 systems... */
#define CAFFDESCSIZE    32
#define CAFFDATASIZEMIN  4 /* edit count only */

/* audio description format flags, Apple-style */
enum {
    kCAFLinearPCMFormatFlagIsFloat        = (1L << 0),
    kCAFLinearPCMFormatFlagIsLittleEndian = (1L << 1)
};

/* helpers */

/// read raw byte buf into head
static void caff_readhead(const uint8_t *buf, t_caff *head, int swap);

/// write head into raw byte buf
static void caff_writehead(uint8_t *buf, const t_caff *head, int swap);

/// post head info for debugging
static void caff_posthead(const t_caff *head);

/// read raw byte buf into chunk
static void caff_readchunk(const uint8_t *buf, t_caff_chunk *chunk, int swap);

/// write chunk into raw byte buf
static void caff_writechunk(uint8_t *buf, const t_caff_chunk *chunk, int swap);

/// post chunk info for debugging
static void caff_postchunk(const t_caff_chunk *chunk);

/// read raw byte buf into desc
static void caff_readdesc(const uint8_t *buf, t_caff_desc *desc, int swap);

/// write desc into raw byte buf
static void caff_writedesc(uint8_t *buf, const t_caff_desc *desc, int swap);

/// post desc info for debugging
static void caff_postdesc(const t_caff_desc *desc);

/// read raw byte buf into data header, currently edit count only
static void caff_readdata(const uint8_t *buf, t_caff_data *data, int swap);

/// write data header into raw byte buf, currently edit count only
static void caff_writedata(uint8_t *buf, const t_caff_data *data, int swap);

/// post data header info for debugging, currently edit count only
static void caff_postdata(const t_caff_data *data);

/* main functions */

int soundfile_caff_headersize()
{
    return CAFFHDRSIZE +                    /* file header */
           CAFFCHUNKSIZE + CAFFDESCSIZE +   /* audio desc chunk */
           CAFFCHUNKSIZE + CAFFDATASIZEMIN; /* audio data chunk */
}

int soundfile_caff_isheader(const char *buf, long size)
{
    if (size < 4) return 0;
    return !strncmp(buf, "caff", 4);
}

int soundfile_caff_readheader(int fd, t_soundfile_info *info)
{
    int nchannels = 1, bytespersample = 2, samplerate = 44100, bigendian = 1,
        headersize = CAFFHDRSIZE, swap = !sys_isbigendian();
    long bytesread = 0, bytelimit = SFMAXBYTES;
    unsigned char buf[SFHDRBUFSIZE];
    t_caff head = {0};
    t_caff_chunk chunk = {0};
    t_caff_desc desc = {0};

        /* file header */
    if (read(fd, buf, soundfile_caff_headersize()) < headersize)
        return 0;
    caff_readhead(buf, &head, swap);
    if (strncmp(head.c_type, "caff", 4))
        return 0;
    if (head.c_version != 1)
    {
        error("CAFF version %d unsupported", head.c_version);
        return 0;
    }
    if (head.c_version == 1 && head.c_flags != 0)
    {
        error("CAFF version 1 format flags invalid");
        return 0;
    }
    if (sys_verbose)
        caff_posthead(&head);  

        /* first chunk must be audio description */
    caff_readchunk(buf + headersize, &chunk, swap);
    if (sys_verbose)
        caff_postchunk(&chunk);
    if (strncmp(chunk.cc_type, "desc", 4))
        return 0;
    headersize += CAFFCHUNKSIZE;
    caff_readdesc(buf + headersize, &desc, swap);
    if (sys_verbose)
        caff_postdesc(&desc);
    headersize += CAFFDESCSIZE;
    if (strncmp(desc.cd_fmtid, "lpcm", 4)) {
        error("CAFF format \"%.4s\" not supported", desc.cd_fmtid);
        return 0;
    }
    nchannels = desc.cd_nchannels;
    switch(desc.cd_bitsperchannel)
    {
        case 16: bytespersample = 2; break;
        case 24: bytespersample = 3; break;
        case 32: bytespersample = 4; break;
        default: return 0;
    }
    if (bytespersample == 4 &&
        !(desc.cd_fmtflags & kCAFLinearPCMFormatFlagIsFloat)) {
        error("CAFF 32 bit int not supported");
        return 0;
    }
    bigendian = !(desc.cd_fmtflags & kCAFLinearPCMFormatFlagIsLittleEndian);
    samplerate = desc.cd_samplerate;

        /* read chunks in loop until we get to the audio data */
    while (1)
    {
        long long seekto = headersize, seekout;
        if (seekto & 1) /* pad up to even number of bytes */
            seekto++;
        seekout = (long)lseek(fd, seekto, SEEK_SET);
        if (seekout != seekto)
            return 0;
        if (read(fd, buf, CAFFCHUNKSIZE) < CAFFCHUNKSIZE)
            return 0;
        caff_readchunk(buf, &chunk, swap);
        if (chunk.cc_size == 0)
            return 0;
        if (sys_verbose)
            caff_postchunk(&chunk);
        if (!strncmp(chunk.cc_type, "data", 4))
        {
            // audio data chunk data section starts with 4 byte edit count
            if (sys_verbose)
            {
                t_caff_data data;
                caff_readdata(buf + CAFFCHUNKSIZE, &data, swap);
                caff_postdata(&data);
            }
            bytelimit = chunk.cc_size - CAFFDATASIZEMIN;
            headersize += CAFFCHUNKSIZE + CAFFDATASIZEMIN;
            break;
        }
        else
            headersize += CAFFCHUNKSIZE + chunk.cc_size;
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
    long nframes)
{
    int byteswritten = 0, headersize = CAFFHDRSIZE, swap = !sys_isbigendian();
    int64_t datasize = nframes * info->i_bytesperframe;
    uint8_t buf[SFHDRBUFSIZE] = {0};

    t_caff head = { "caff", 1, 0 };
    t_caff_chunk chunk = { "desc", (int64_t)CAFFDESCSIZE };
    t_caff_desc desc = {
        (double)info->i_samplerate,          /* sample rate */
        "lpcm",                              /* format id */
        0,                                   /* format flags */
        (uint32_t)info->i_bytesperframe,     /* bytes per packet */
        1,                                   /* frames per packet */
        (uint32_t)info->i_nchannels,         /* channels */
        (uint32_t)info->i_bytespersample * 8 /* bits per channel */
    };
    t_caff_data data = {0};

        /* file header */
    caff_writehead(buf, &head, swap);
    if (sys_verbose)
        caff_posthead(&head);

        /* audio description chunk */
    caff_writechunk(buf + headersize, &chunk, swap);
    if (sys_verbose)
        caff_postchunk(&chunk);
    headersize += CAFFCHUNKSIZE;
    if (info->i_bytespersample == 4)
        desc.cd_fmtflags |= kCAFLinearPCMFormatFlagIsFloat;
    if (!info->i_bigendian)
        desc.cd_fmtflags |= kCAFLinearPCMFormatFlagIsLittleEndian;
    caff_writedesc(buf + headersize, &desc, swap);
    if (sys_verbose)
        caff_postdesc(&desc);
    headersize += CAFFDESCSIZE;

        /* audio data chunk */
    strncpy(chunk.cc_type, "data", 4);
    chunk.cc_size = CAFFDATASIZEMIN + datasize;
    caff_writechunk(buf + headersize, &chunk, swap);
    if (sys_verbose)
        caff_postchunk(&chunk);
    headersize += CAFFCHUNKSIZE;
    caff_writedata(buf + headersize, &data, swap);
    if (sys_verbose)
        caff_postdata(&data);
    headersize += CAFFDATASIZEMIN;

    byteswritten = (int)write(fd, buf, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

int soundfile_caff_updateheader(int fd, const t_soundfile_info *info,
    long nframes)
{
    int swap = !sys_isbigendian();
    int64_t datasize = (nframes *info->i_bytesperframe) + CAFFDATASIZEMIN;
    off_t seekto = CAFFHDRSIZE + CAFFCHUNKSIZE + CAFFDESCSIZE + 4; /* cc_type */

        /* audio data chunk data size */
    if (lseek(fd, seekto, SEEK_SET) == 0)
        return 0;
    datasize = swap8s(datasize, swap);
    if (write(fd, (char *)&datasize, 8) < 8)
        return 0;

    return 1;
}

int soundfile_caff_hasextension(const char *filename, long size)
{
    int len = strnlen(filename, size);
    if (len >= 5 &&
        (!strncmp(filename + (len - 4), ".caf", 4) ||
         !strncmp(filename + (len - 4), ".CAF", 4)))
        return 1;
    return 0;
}

/* helpers */

static void caff_readhead(const uint8_t *buf, t_caff *head, int swap)
{
    memcpy(&head->c_type,    buf,     4);
    memcpy(&head->c_version, buf + 4, 2);
    memcpy(&head->c_flags,   buf + 6, 2);
    if (swap)
    {
        head->c_version = swap2(head->c_version, 1);
        head->c_flags = swap2(head->c_flags, 1);
    }
}

static void caff_writehead(uint8_t *buf, const t_caff *head, int swap)
{
    uint16_t inttmp;
    memcpy(buf, &head->c_type, 4);
    
    inttmp = swap2(head->c_version, swap);
    memcpy(buf + 4, &inttmp, 2);

    inttmp = swap2(head->c_flags, swap);
    memcpy(buf + 6, &inttmp, 2);
}

static void caff_posthead(const t_caff *head)
{
    post("%.4s", head->c_type);
    post("  version %d", head->c_version);
    post("  flags %d", head->c_flags);
}

static void caff_readchunk(const uint8_t *buf, t_caff_chunk *chunk, int swap)
{
    memcpy(&chunk->cc_type, buf,     4);
    memcpy(&chunk->cc_size, buf + 4, 8);
    chunk->cc_size = swap8s(chunk->cc_size, swap);
}

static void caff_writechunk(uint8_t *buf, const t_caff_chunk *chunk, int swap)
{
    int64_t longtmp;

    memcpy(buf, &chunk->cc_type, 4);

    longtmp = swap8s(chunk->cc_size, swap);
    memcpy(buf + 4, &longtmp, 8);
}

static void caff_postchunk(const t_caff_chunk *chunk)
{
    post("%.4s %" PRId64, chunk->cc_type, chunk->cc_size);
}

static void caff_readdesc(const uint8_t *buf, t_caff_desc *desc, int swap)
{
    memcpy(&desc->cd_samplerate,      buf,      8);
    memcpy(&desc->cd_fmtid,           buf +  8, 4);
    memcpy(&desc->cd_fmtflags,        buf + 12, 4);
    memcpy(&desc->cd_bytesperpacket,  buf + 16, 4);
    memcpy(&desc->cd_framesperpacket, buf + 20, 4);
    memcpy(&desc->cd_nchannels,       buf + 24, 4);
    memcpy(&desc->cd_bitsperchannel,  buf + 28, 4);
    if (swap) 
    {
        desc->cd_samplerate = swapdouble(desc->cd_samplerate, 1);
        desc->cd_fmtflags = swap4(desc->cd_fmtflags, 1);
        desc->cd_bytesperpacket = swap4(desc->cd_bytesperpacket, 1);
        desc->cd_framesperpacket = swap4(desc->cd_framesperpacket, 1);
        desc->cd_nchannels = swap4(desc->cd_nchannels, 1);
        desc->cd_bitsperchannel = swap4(desc->cd_bitsperchannel, 1);
    }
}

static void caff_writedesc(uint8_t *buf, const t_caff_desc *desc, int swap)
{
    double doubletmp;
    uint32_t inttmp;

    doubletmp = swapdouble(desc->cd_samplerate, swap);
    memcpy(buf, &doubletmp, 8);

    memcpy(buf + 8, &desc->cd_fmtid, 4);

    inttmp = swap4(desc->cd_fmtflags, swap);
    memcpy(buf + 12, &inttmp, 4);

    inttmp = swap4(desc->cd_bytesperpacket, swap);
    memcpy(buf + 16, &inttmp, 4);

    inttmp = swap4(desc->cd_framesperpacket, swap);
    memcpy(buf + 20, &inttmp, 4);

    inttmp = swap4(desc->cd_nchannels, swap);
    memcpy(buf + 24, &inttmp, 4);

    inttmp = swap4(desc->cd_bitsperchannel, swap);
    memcpy(buf + 28, &inttmp, 4);
}

static void caff_postdesc(const t_caff_desc *desc)
{
    post("  sample rate %g", desc->cd_samplerate);
    post("  fmt id %.4s", desc->cd_fmtid);
    post("  fmt flags %u", desc->cd_fmtflags);
    post("  bytes per packet %u", desc->cd_bytesperpacket);
    post("  frames per packet %u", desc->cd_framesperpacket);
    post("  channels %u", desc->cd_nchannels);
    post("  bits per channel %u", desc->cd_bitsperchannel);
}

static void caff_readdata(const uint8_t *buf, t_caff_data *data, int swap) {
    memcpy(&data->cd_editcount, buf, 4);
    if (swap)
        data->cd_editcount = swap4(data->cd_editcount, 1);
}

static void caff_writedata(uint8_t *buf, const t_caff_data *data, int swap) {
    uint32_t inttmp;

    inttmp = swap4(data->cd_editcount, swap);
    memcpy(buf, &inttmp, 4);
}

static void caff_postdata(const t_caff_data *data)
{
    post("  edit count %u", data->cd_editcount);
}
