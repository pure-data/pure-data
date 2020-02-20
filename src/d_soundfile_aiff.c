/* Copyright (c) 1997-1999 Miller Puckette. Updated 2020 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* ref: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/AIFF/AIFF.html */

#include "d_soundfile.h"
#include "s_stuff.h" /* for sys_verbose */
#include <math.h>

/* AIFF (Audio Interchange File Format) and AIFF-C (AIFF Compressed)

  * RIFF variant with sections split into data "chunks"
  * chunk sizes do not include the chunk id or size (- 8)
  * the file header's size is the total size of all subsequent chunks (- 8)
  * chunk data is big endian
  * sound data is big endian, unless AIFF-C "sowt" compression type
  * the sound data chunk can be omitted if common chunk sample frames is 0
  * chunks start on even byte offsets
  * some chunk string values are pascal strings:
    - first byte is string length, limited to 255 chars
    - total length must be even, pad with a '\0' byte, max 256
    - padding is not added to string length byte value
  * other chunk string values have variable length denoted by the chunk size
    ie. text chunks, there is no c string \0 end byte
  * AIFF-C compression 4 char types & name (pascal) strings:
    - NONE "not compressed" big endian
    - sowt "not compressed" little endian
    - fl32 "32-bit floating point" big endian
    - FL32 "Float 32" big endian
    - the rest are not relevant to Pd...
  * limited to ~2 GB files as sizes are signed 32 bit ints

  this implementation:

  * supports AIFF and AIFF-C
  * implicitly writes AIFF-C header for 32 bit float (see below)
  * implements chunks: common, data, version (AIFF-C), name, author, copyright,
                    annotation
  * ignores chunks: marker, instrument, comment, audio recording, MIDI data,
                    application, ID3
  * assumes common chunk is always before sound data chunk
  * ignores any chunks after finding the sound data chunk
  * assumes there is always a sound data chunk
  * does not block align sound data
  * sample format: 16 and 24 bit lpcm, 32 bit float, no 32 bit lpcm

  Pd versions < 0.51 did *not* read or write AIFF files with a 32 bit float
  sample format.

*/

    /* explicit byte sizes, sizeof(struct) may return alignment-padded values */
#define AIFFCHUNKSIZE  8 /**< chunk header only */
#define AIFFHEADSIZE  12 /**< chunk header and file format only */
#define AIFFVERSIZE   12 /**< chunk header and data */
#define AIFFCOMMSIZE  26 /**< chunk header and data */
#define AIFFDATASIZE  16 /**< chunk header, offset, and block size data */
#define AIFFMARKERSIZE 10

#define AIFFCVER1    0xA2805140 /**< 2726318400 decimal */
#define AIFFMAXBYTES 0x7fffffff /**< max signed 32 bit size */

    /* pascal string defines */
#define AIFF_NONE_PSTR "\x0E""not compressed"
#define AIFF_FL32_PSTR "\x16""32-bit floating point"

#define AIFF_NONE_LEN 16 /**< 1 len byte + 14 bytes + 1 \0 pad byte */
#define AIFF_FL32_LEN 22 /**< 1 len byte + 21 bytes, no pad byte */

    /** basic chunk header, 8 bytes */
typedef struct _chunk
{
    char c_id[4];                    /**< chunk id                     */
    int32_t c_size;                  /**< chunk data length            */
} t_chunk;

    /** file header, 12 bytes */
typedef struct _head
{
    char h_id[4];                    /**< chunk id "FORM"              */
    int32_t h_size;                  /**< chunk data length            */
    char h_formtype[4];              /**< format: "AIFF" or "AIFC"     */
} t_head;

    /** commmon chunk, 26 (AIFF) or 30+ (AIFC) bytes
        note: sample frames is split to avoid struct alignment padding */
typedef struct _commchunk
{
    char cc_id[4];                   /**< chunk id "COMM"              */
    int32_t cc_size;                 /**< chunk data length            */
    uint16_t cc_nchannels;           /**< number of channels           */
    uint8_t cc_nframes[4];           /**< # of sample frames, uint32_t */
    uint16_t cc_bitspersample;       /**< bits per sample              */
    uint8_t cc_samplerate[10];       /**< sample rate, 80-bit float!   */
    /* AIFF-C */
    char cc_comptype[4];             /**< compression type             */
    char cc_compname[AIFF_FL32_LEN]; /**< compression name, pascal str */
} t_commchunk;

    /** sound data chunk, min 16 bytes before data */
typedef struct _datachunk
{
    char dc_id[4];                   /**< chunk id "SSND"              */
    int32_t dc_size;                 /**< chunk data length            */
    uint32_t dc_offset;              /**< additional offset in bytes   */
    uint32_t dc_block;               /**< block size                   */
} t_datachunk;

    /** AIFF-C format version chunk, 12 bytes */
typedef struct _verchunk
{
    char vc_id[4];                   /**< chunk id "FVER"              */
    int32_t vc_size;                 /**< chunk data length, 4 bytes   */
    uint32_t vc_timestamp;           /**< AIFF-C version timestamp     */
} t_verchunk;

    /** marker, min 6 bytes */
typedef struct _marker {
       int8_t m_id;                  /**< marker id, must be > 0       */
       uint32_t m_frame;             /**< sample frame position        */
       char m_name;                  /**< name, pascal str             */
} t_marker;

    /** marker chunk, min 10 bytes before marker list */
typedef struct _markerchunk {
    char mc_id[4];                   /**< chunk id "MARK"              */
    int32_t mc_size;                 /**< chunk data length, 4 bytes   */
    uint16_t mc_nmarkers;
} t_markerchunk;

    /** instrument chunk loop, 4 bytes  */
typedef struct _loop {
    int16_t l_playmode; /**< loop playback mode, 0 : no loop, 1 : forward,
                                                 2: forward backward */
    int8_t l_begin;     /**< begin marker id */
    int8_t l_end;       /**< end marker id   */
} t_loop;

    /** instrument chunk, 24 bytes */
typedef struct _instchunk
{
    char ic_id[4];                  /**< chunk id "INST"               */
    int32_t ic_size;                /**< chunk data length, 4 bytes    */
    int8_t ic_basenote;             /**< original pitch, MIDI 0 to 127 */
    int8_t ic_detune;               /**< detune cents, -50 to 50       */
    int8_t ic_lownote;              /**< playback low, MIDI 0 to 127   */
    int8_t ic_highnote;             /**< playback high, MIDI 0 to 127  */
    int8_t ic_lowvelocity;          /**< noteon low, MIDI 1 to 127     */
    int8_t ic_highvelocity;         /**< noteon high, MIDI 1 to 127    */
    uint16_t ic_gain;               /**< gain adjustment, +/- dB       */
    t_loop   ic_sustainloop;        /**< sustain loop points           */
    t_loop   ic_releaseloop;        /**< release loop points           */
} t_instchunk;

/* ----- helpers ----- */

    /** pascal string to c string, max size 256 */
static void aiff_getpstring(const char* pstring, char *cstring, size_t size)
{
    uint8_t len = (uint8_t)pstring[0];
    if (len > size - 1) len = size - 1;
    memcpy(cstring, pstring + 1, len);
    cstring[len+1] = '\0';
}
    /** c string to pascal string, max size 256 */
static void aiff_setpstring(char *pstring, const char *cstring, size_t size)
{
    uint8_t len = strlen(cstring);
    if (len > 255) len = 255;
    if (len > size) len = size;
    pstring[0] = len;
    memcpy(pstring + 1, cstring, len);
    if (!(len & 1)) pstring[len] = '\0'; /* pad byte */
}

static uint64_t aiff_getframes(const t_commchunk *comm, int swap)
{
    uint32_t nframes = 0;
    memcpy(&nframes, comm->cc_nframes, 4);
    return swap4(nframes, swap);
}

static void aiff_setframes(t_commchunk *comm, uint32_t nframes, int swap)
{
    nframes = swap4(nframes, swap);
    memcpy(comm->cc_nframes, &nframes, 4);
}

    /** read sample rate from comm chunk 80-bit AIFF-compatible number */
static double aiff_getsamplerate(const t_commchunk *comm, int swap)
{
    unsigned char temp[10], *src = temp, exp;
    unsigned long mantissa, last = 0;

    memcpy(temp, (char *)comm->cc_samplerate, 10);
    swapstring4((char *)src + 2, swap);

    mantissa = (unsigned long) *((unsigned long *)(src + 2));
    exp = 30 - *(src + 1);
    while (exp--)
    {
        last = mantissa;
        mantissa >>= 1;
    }
    if (last & 0x00000001) mantissa++;
    return mantissa;
}

    /** write a sample rate to comm chunk 80-bit AIFF-compatible number */
static void aiff_setsamplerate(t_commchunk *comm, double sr)
{
    unsigned char *dst = (unsigned char *)comm->cc_samplerate;
    int exponent;
    double mantissa = frexp(sr, &exponent);
    unsigned long fixmantissa = ldexp(mantissa, 32);
    dst[0] = (exponent + 16382) >> 8;
    dst[1] = exponent + 16382;
    dst[2] = fixmantissa >> 24;
    dst[3] = fixmantissa >> 16;
    dst[4] = fixmantissa >> 8;
    dst[5] = fixmantissa;
    dst[6] = dst[7] = dst[8] = dst[9] = 0;
}

    /** post chunk info for debugging */
static void aiff_postchunk(const t_chunk *chunk, int swap)
{
    post("%.4s %d", chunk->c_id, swap4s(chunk->c_size, swap));
}

    /** post head info for debugging */
static void aiff_posthead(const t_head *head, int swap)
{
    aiff_postchunk((const t_chunk *)head, swap);
    post("  %.4s", head->h_formtype);
}

    /** post comm info for debugging */
static void aiff_postcomm(const t_commchunk *comm, int isaiffc, int swap)
{
    aiff_postchunk((const t_chunk *)comm, swap);
    post("  channels %u", swap2(comm->cc_nchannels, swap));
    post("  frames %u", aiff_getframes(comm, swap));
    post("  bits per sample %u", swap2(comm->cc_bitspersample, swap));
    post("  sample rate %g", aiff_getsamplerate(comm, swap));
    if (isaiffc)
    {
            /* handle pascal string */
        char name[256];
        aiff_getpstring(comm->cc_compname, name, 256);
        post("  comp type %.4s", comm->cc_comptype);
        post("  comp name \"%s\"", name);
    }
}

    /** post sata info for debugging */
static void aiff_postdata(const t_datachunk *data, int swap)
{
    aiff_postchunk((const t_chunk *)data, swap);
    post("  offset %u", swap4(data->dc_offset, swap));
    post("  block size %u", swap4(data->dc_block, swap));
}

    /** returns 1 if format requires AIFF-C */
static int aiff_isaiffc(const t_soundfile *sf)
{
    return (!sf->sf_bigendian || sf->sf_bytespersample == 4);
}

    /** read first chunk, returns filled chunk and offset on success or -1 */
static off_t aiff_firstchunk(const t_soundfile *sf, t_chunk *chunk)
{
    if (soundfile_readbytes(sf->sf_fd, AIFFHEADSIZE, (char *)chunk,
                                AIFFCHUNKSIZE) < AIFFCHUNKSIZE)
        return -1;
    return AIFFHEADSIZE;
}

    /** read next chunk, chunk should be filled when calling
        returns fills chunk offset on success or -1 */
static off_t aiff_nextchunk(const t_soundfile *sf, off_t offset, t_chunk *chunk)
{
    int32_t chunksize = swap4s(chunk->c_size, !sys_isbigendian());
    off_t seekto = offset + AIFFCHUNKSIZE + chunksize;
    if (seekto & 1) /* pad up to even number of bytes */
        seekto++;
    if (soundfile_readbytes(sf->sf_fd, seekto, (char *)chunk,
                                AIFFCHUNKSIZE) < AIFFCHUNKSIZE)
        return -1;
    return seekto;
}

    /** look for a chunk by name starting at an offset,
        chunk should be filled if offset > 0
        fills chunk and returns offset on success or -1 */
static off_t aiff_findchunk(const t_soundfile *sf, const char *name,
    off_t offset, t_chunk *chunk)
{
    if (offset <= 0)
        offset = aiff_firstchunk(sf, chunk);
    else
        offset = aiff_nextchunk(sf, offset, chunk);
    if (offset == -1)
        return -1;
    while (1)
    {
        if (!strncmp(chunk->c_id, name, 4))
            return offset;
        if((offset = aiff_nextchunk(sf, offset, chunk)) == -1)
            return -1;
    }
}

    /* sets the total chunk size in the file header,
       assumes the file header has already been written to sf->sf_fd */
static int aiff_setheadchunksize(const t_soundfile *sf, int32_t size, int swap)
{
    size = swap4s(size, swap);
    if (soundfile_writebytes(sf->sf_fd, 4, (char *)&size, 4) < 4)
        return 0;
    return 1;
}

/* ------------------------- AIFF ------------------------- */

static int aiff_isheader(const char *buf, size_t size)
{
    if (size < 4) return 0;
    return !strncmp(buf, "FORM", 4);
}

    /** loop through chunks to find comm and data */
static int aiff_readheader(t_soundfile *sf)
{
    int nchannels = 1, bytespersample = 2, samplerate = 44100, bigendian = 1,
        swap = !sys_isbigendian(), isaiffc = 0, commfound = 0;
    off_t headersize = AIFFHEADSIZE;
    size_t bytelimit = AIFFMAXBYTES;
    union
    {
        char b_c[SFHDRBUFSIZE];
        t_head b_head;
        t_chunk b_chunk;
        t_commchunk b_commchunk;
        t_verchunk b_verchunk;
        t_datachunk b_datachunk;
    } buf = {0};
    t_head *head = &buf.b_head;
    t_chunk *chunk = &buf.b_chunk;

        /* file header */
    if (soundfile_readbytes(sf->sf_fd, 0, buf.b_c, headersize) < headersize)
        return 0;
    if (strncmp(head->h_formtype, "AIFF", 4))
    {
        if (strncmp(head->h_formtype, "AIFC", 4))
            return 0;
        isaiffc = 1;
    }
    if (sys_verbose)
        aiff_posthead(head, swap);

        /* read chunks in loop until we find the sound data chunk */
    if ((headersize = aiff_firstchunk(sf, chunk)) == -1)
        return 0;
    while (1)
    {
        int32_t chunksize = swap4s(chunk->c_size, swap);
        /* post("chunk %.4s seek %d", chunk->c_id, seekto); */
        if (!strncmp(chunk->c_id, "FVER", 4))
        {
                /* AIFF-C format version chunk */
            if (!isaiffc)
            {
                error("aiff: found FVER chunk, but not AIFF-C");
                return 0;
            }
            if (soundfile_readbytes(sf->sf_fd, headersize + AIFFCHUNKSIZE,
                                    buf.b_c + AIFFCHUNKSIZE,
                                    chunksize) < chunksize)
                return 0;
            if (swap4(buf.b_verchunk.vc_timestamp, swap) != AIFFCVER1)
            {
                error("aiff: unsupported AIFF-C version");
                return 0;
            }
        }
        else if (!strncmp(chunk->c_id, "COMM", 4))
        {
                /* common chunk */
            int bitspersample, isfloat = 0;
            t_commchunk *comm = &buf.b_commchunk;
            if (soundfile_readbytes(sf->sf_fd, headersize + AIFFCHUNKSIZE,
                                    buf.b_c + AIFFCHUNKSIZE,
                                    chunksize) < chunksize)
                return 0;
            if (sys_verbose)
                aiff_postcomm(comm, isaiffc, swap);
            nchannels = swap2(comm->cc_nchannels, swap);
            bitspersample = swap2(comm->cc_bitspersample, swap);
            switch (bitspersample)
            {
                case 16: bytespersample = 2; break;
                case 24: bytespersample = 3; break;
                case 32: bytespersample = 4; break;
                default:
                    error("aiff: %d bit samples not supported", bitspersample);
                    return 0;
            }
            samplerate = aiff_getsamplerate(comm, swap);
            if (isaiffc)
            {
                if (!strncmp(comm->cc_comptype, "NONE", 4))
                {
                        /* big endian */
                }
                else if (!strncmp(comm->cc_comptype, "sowt", 4))
                {
                        /* little endian */
                    bigendian = 0;
                }
                else if (!strncmp(comm->cc_comptype, "fl32", 4) ||
                         !strncmp(comm->cc_comptype, "FL32", 4))
                {
                        /* big endian float */
                    if (bytespersample != 4)
                    {
                        error("aiff: wrong byte size for format %.4s",
                              comm->cc_comptype);
                        return 0;
                    }
                    isfloat = 1;
                }
                else
                {
                    error("aiff: format \"%.4s\" not supported",
                          comm->cc_comptype);
                    return 0;
                }
            }
            if (bytespersample == 4 && !isfloat)
            {
                error("aiff: 32 bit int not supported");
                return 0;
            }
            commfound = 1;
        }
        else if (!strncmp(chunk->c_id, "SSND", 4))
        {
                /* uncompressed sound data chunk */
            if (sys_verbose)
            {
                t_datachunk *data = &buf.b_datachunk;
                if (soundfile_readbytes(sf->sf_fd, headersize + AIFFCHUNKSIZE,
                                        buf.b_c + AIFFCHUNKSIZE, 8) < 8)
                    return 0;
                if (sys_verbose)
                    aiff_postdata(data, swap);
            }
            bytelimit = chunksize - 8; /* subtract offset and block */
            headersize += AIFFDATASIZE;
            break;
        }
        else if (!strncmp(chunk->c_id, "CSND", 4))
        {
                /* AIFF-C compressed sound data chunk */
            error("aiff: compressed format not support");
            return 0;
        }
        else
        {
                /* everything else */
            if (sys_verbose)
                aiff_postchunk(chunk, swap);
        }
        if((headersize = aiff_nextchunk(sf, headersize, chunk)) == -1)
            return 0;
    }
    if (!commfound)
    {
        error("aiff: common chunk not found");
        return 0;
    }

        /* interpret data size from file size? this is not supported by the
           AIFF spec, but let's do it just in case */
    if (bytelimit == AIFFMAXBYTES)
    {
        bytelimit = lseek(sf->sf_fd, 0, SEEK_END) - headersize;
        if (bytelimit > AIFFMAXBYTES || bytelimit < 0)
            bytelimit = AIFFMAXBYTES;
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

    /** write basic header with order: head [ver] comm data */
static int aiff_writeheader(t_soundfile *sf, size_t nframes)
{
    int isaiffc = aiff_isaiffc(sf), swap = !sys_isbigendian();
    size_t commsize = AIFFCOMMSIZE, datasize = nframes * sf->sf_bytesperframe;
    off_t headersize = 0;
    ssize_t byteswritten = 0;
    char buf[SFHDRBUFSIZE] = {0};
    t_head head = {"FORM", 0, "AIFF"};
    t_commchunk comm = {
        "COMM", swap4s(18, swap),
        swap2(sf->sf_nchannels, swap),          /* channels         */
        0,                                      /* sample frames    */
        swap2(sf->sf_bytespersample / 8, swap), /* bits per sample  */
        0,                                      /* sample rate      */
        0, 0                                    /* comp info        */
    };
    t_datachunk data = {"SSND", swap4s(8, swap), 0, 0};

        /* file header */
    if (isaiffc)
        strncpy(head.h_formtype, "AIFC", 4);
    memcpy(buf + headersize, &head, AIFFHEADSIZE);
    headersize += AIFFHEADSIZE;

        /* format ver chunk */
    if (isaiffc)
    {
        t_verchunk ver = {
            "FVER", swap4s(4, swap),
            swap4(AIFFCVER1, swap) /* version timestamp */
        };
        memcpy(buf + headersize, &ver, AIFFVERSIZE);
        headersize += AIFFVERSIZE;
    }

        /* comm chunk */
    comm.cc_nchannels = swap2(sf->sf_nchannels, swap);
    aiff_setframes(&comm, nframes, swap);
    comm.cc_bitspersample = swap2(8 * sf->sf_bytespersample, swap);
    aiff_setsamplerate(&comm, sf->sf_samplerate);
    if (isaiffc)
    {
            /* AIFF-C compression info */
        if (sf->sf_bytespersample == 4)
        {
            strncpy(comm.cc_comptype, "fl32", 4);
            strncpy(comm.cc_compname, AIFF_FL32_PSTR, AIFF_FL32_LEN);
            commsize += 4 + AIFF_FL32_LEN;
        }
        else
        {
            strncpy(comm.cc_comptype, (sf->sf_bigendian ? "NONE" : "sowt"), 4);
            strncpy(comm.cc_compname, AIFF_NONE_PSTR, AIFF_NONE_LEN);
            commsize += 4 + AIFF_NONE_LEN;
        }
    }
    comm.cc_size = swap4(commsize - AIFFCHUNKSIZE, swap);
    memcpy(buf + headersize, &comm, commsize);
    headersize += commsize;

        /* data chunk */
    data.dc_size = swap4((uint32_t)(datasize + 8), swap);
    memcpy(buf + headersize, &data, AIFFDATASIZE);
    headersize += AIFFDATASIZE;

        /* update total chunk size */
    head.h_size = swap4s((int32_t)(headersize + datasize - 8), swap);
    memcpy(buf + 4, (char *)&head.h_size, 4);

    if (sys_verbose)
    {
        aiff_posthead(&head, swap);
        aiff_postcomm(&comm, isaiffc, swap);
        aiff_postdata(&data, swap);
    }

    byteswritten = soundfile_writebytes(sf->sf_fd, 0, buf, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

    /** loop through chunks to update comm and data,
        assumes order: head [ver] comm [meta...] data */
static int aiff_updateheader(t_soundfile *sf, size_t nframes)
{
    int swap = !sys_isbigendian(), datafound = 0;
    size_t datasize = nframes * sf->sf_bytesperframe;
    off_t headersize;
    t_chunk chunk = {0};

    if ((headersize = aiff_firstchunk(sf, &chunk)) == -1)
        return 0;
    while (1)
    {
        if (!strncmp(chunk.c_id, "COMM", 4))
        {
                /* num frames */
            uint32_t uinttmp = swap4((uint32_t)nframes, swap);
            if (soundfile_writebytes(sf->sf_fd, headersize + 10,
                                     (char *)&uinttmp, 4) < 4)
                return 0;
        }
        else if(!strncmp(chunk.c_id, "SSND", 4))
        {
                /* data chunk size */
            int32_t inttmp = swap4s((int32_t)datasize + 8, swap);
            if (soundfile_writebytes(sf->sf_fd, headersize + 4,
                                     (char *)&inttmp, 4) < 4)
                return 0;
            datafound = 1;
            break;
        }
        if((headersize = aiff_nextchunk(sf, headersize, &chunk)) == -1)
            return 0;
    }
    if (!datafound)
        return 0; /* shouldn't happen */

        /* file header chunk size */
    if (!aiff_setheadchunksize(sf, headersize + datasize - 8, swap))
        return 0;

    if (sys_verbose)
    {
        post("FORM %d", headersize + datasize - 8);
        post("  %s", (aiff_isaiffc(sf) ? "AIFC" : "AIFF"));
        post("COMM");
        post("  frames %d", nframes);
        post("SSND %d", datasize + 8);
    }

    return 1;
}

static int aiff_hasextension(const char *filename, size_t size)
{
    int len = strnlen(filename, size);
    if (len >= 5 &&
        (!strncmp(filename + (len - 4), ".aif", 4) ||
         !strncmp(filename + (len - 4), ".AIF", 4)))
        return 1;
    if (len >= 6 &&
        (!strncmp(filename + (len - 5), ".aiff", 5) ||
         !strncmp(filename + (len - 5), ".aifc", 5) ||
         !strncmp(filename + (len - 5), ".AIFF", 5) ||
         !strncmp(filename + (len - 5), ".AIFC", 5)))
        return 1;
    return 0;
}

static int aiff_addextension(char *filename, size_t size)
{
    int len = strnlen(filename, size);
    if (len + 4 > size)
        return 0;
    strncat(filename, ".aif", 4);
    return 1;
}

    /* default to big endian unless overridden */
static int aiff_endianness(int endianness)
{
    if (endianness == 0)
        return 0;
    return 1;
}

    /** read relevant meta chunk info and outputs lists to out,
        assumes order: head [ver] comm [meta...] data */
static int aiff_readmeta(t_soundfile *sf, t_outlet *out) {
    int swap = !sys_isbigendian();
    off_t headersize;
    t_chunk chunk;

    if ((headersize = aiff_firstchunk(sf, &chunk)) == -1)
        return 0;
    while (1)
    {
        int32_t chunksize = swap4s(chunk.c_size, swap);
        if (!strncmp(chunk.c_id, "NAME", 4) ||
            !strncmp(chunk.c_id, "AUTH", 4) ||
            !strncmp(chunk.c_id, "(c) ", 4) ||
            !strncmp(chunk.c_id, "ANNO", 4))
        {
                /* text chunk */
            t_atom list[2];
            t_symbol *name;
            char str[MAXPDSTRING];
            int len = chunksize;
            if (len > MAXPDSTRING-1) len = MAXPDSTRING-1;
            if (soundfile_readbytes(sf->sf_fd,
                                    headersize + AIFFCHUNKSIZE,
                                    str, len) < len) return 0;
            str[len] = '\0';
            if (sys_verbose)
            {
                aiff_postchunk(&chunk, swap);
                post("  \"%s\"", str);
            }
            if (!strncmp(chunk.c_id, "NAME", 4))
                name = gensym("name");
            else if (!strncmp(chunk.c_id, "AUTH", 4))
                name = gensym("author");
            else if (!strncmp(chunk.c_id, "(c) ", 4))
                name = gensym("copyright");
            else /* ANNO */
                name = gensym("annotation");
            SETSYMBOL((t_atom *)list, name);
            SETSYMBOL((t_atom *)list+1, gensym(str));
            outlet_list(out, &s_list, 2, (t_atom *)list);
        }
        if (!strncmp(chunk.c_id, "SSND", 4))
            break; /* done */
        if((headersize = aiff_nextchunk(sf, headersize, &chunk)) == -1)
            return 0;
    }

    return 1;
}

    /** write meta data chunks by moving existing chunks to make room,
        keeps order: head [ver] comm [meta...] data */
static int aiff_writemeta(t_soundfile *sf, int argc, t_atom *argv)
{
    int swap = !sys_isbigendian();
    off_t headersize = sf->sf_headersize, filesize, datapos;
    int32_t datasize;
    char *meta;
    t_chunk chunk = {0};

    if (argc < 2 || argv->a_type != A_SYMBOL)
        goto usage;
    meta = (char *)argv->a_w.w_symbol->s_name;
    argc--; argv++;

    if ((filesize = lseek(sf->sf_fd, 0, SEEK_END)) == -1)
        return 0;

        /* find data chunk */
    if ((datapos = aiff_findchunk(sf, "SSND", 0, &chunk)) == -1)
        return 0;
    datasize = swap4s(chunk.c_size, swap);

        /* add new chunk */
    if (!strcmp(meta, "name") || !strcmp(meta, "author") ||
        !strcmp(meta, "copyright") || !strcmp(meta, "annotation"))
    {
            /* text chunk */
        const char *text = argv->a_w.w_symbol->s_name;
        char *textid;
        size_t textlen = 0, movesize = 0;
        off_t textpos, movefrom, moveto;

        if (argv->a_type != A_SYMBOL)
            goto usage;
        textlen = strlen(text);
        if (textlen == 0)
        {
            post("aiff: meta %s ignored, symbol is empty", meta);
            return 1;
        }

        if (!strcmp(meta, "annotation"))
        {
                /* there can be multiple ANNO chunks, move data forward */
            textid = "ANNO";
            movefrom = datapos;
            moveto = datapos + AIFFCHUNKSIZE + textlen;
            movesize = AIFFDATASIZE;
            textpos = datapos;
            headersize = moveto;
        }
        else
        {
                /* there can only be 1 of these chunks */
            if (!strcmp(meta, "name"))
                textid = "NAME";
            else if (!strcmp(meta, "author"))
                textid = "AUTH";
            else /* copyright */
                textid = "(c) ";

                /* look for existing chunk */
            if ((textpos = aiff_findchunk(sf, textid, 0, &chunk)) == -1)
            {
                    /* not found, move data forward */
                movefrom = datapos;
                moveto = datapos + AIFFCHUNKSIZE + textlen;
                movesize = AIFFDATASIZE;
                textpos = datapos;
                headersize = moveto;
            }
            else
            {
                    /* found */
                int32_t oldtextlen = swap4s(chunk.c_size, swap),
                        diff = textlen - oldtextlen;
                if (diff != 0)
                {
                        /* shrink or grow */
                    movefrom = textpos + AIFFCHUNKSIZE + oldtextlen;
                    if (oldtextlen & 1) movefrom++; /* dont cp prev pad byte */
                    moveto = textpos + AIFFCHUNKSIZE + textlen;
                    movesize = filesize - movefrom;
                    headersize += diff;
                }
                    /* else same size */
            }
        }

            /* make room? */
        if (movesize > 0)
        {
            if (moveto & 1) /* pad up to even number of bytes */
            {
               moveto++;
               headersize++;
            }
            if (soundfile_movebytes(sf->sf_fd, moveto, movefrom,
                                    movesize) < movesize)
                return 0;
        }

            /* write new/updated chunk */
        strncpy(chunk.c_id, textid, 4);
        chunk.c_size = swap4s(textlen, swap);
        if (soundfile_writebytes(sf->sf_fd, textpos, (char *)&chunk,
                                 AIFFCHUNKSIZE) < AIFFCHUNKSIZE)
            return 0;
        if (soundfile_writebytes(sf->sf_fd, textpos + AIFFCHUNKSIZE,
                                 text, textlen) < textlen)
            return 0;

        if (sys_verbose)
        {
            post("FORM %d", headersize - 8);
            post("  %s", (aiff_isaiffc(sf) ? "AIFC" : "AIFF"));
            aiff_postchunk(&chunk, swap);
            post("  %s", text);
        }
    }

        /* update file header chunk size */
    if (!aiff_setheadchunksize(sf, headersize + datasize, swap))
        return 0;

        /* update headersize and seek to end for subsequent sample writes */
    sf->sf_headersize = headersize;
    lseek(sf->sf_fd, 0, SEEK_END);

    return 1;

usage:
    error("usage: meta <type>...");
    post("type: name <symbol>, author <symbol>, copyright <symbol>...");
    post("annotation <symbol>");
    return 0;
}

void soundfile_aiff_setup()
{
    t_soundfile_filetype aiff = {
        gensym("aiff"),
        AIFFHEADSIZE + AIFFCOMMSIZE + AIFFDATASIZE,
        aiff_isheader,
        soundfile_filetype_open,
        soundfile_filetype_close,
        aiff_readheader,
        aiff_writeheader,
        aiff_updateheader,
        aiff_hasextension,
        aiff_addextension,
        aiff_endianness,
        soundfile_filetype_seektoframe,
        soundfile_filetype_readsamples,
        soundfile_filetype_writesamples,
        aiff_readmeta,
        aiff_writemeta,
        NULL /* data */
    };
    soundfile_addfiletype(&aiff);
}
