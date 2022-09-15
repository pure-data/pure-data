/* Copyright (c) 1997-1999 Miller Puckette. Updated 2020 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* ref: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/AIFF/AIFF.html */

#include "d_soundfile.h"
#include <math.h>

/* AIFF (Audio Interchange File Format) and AIFF-C (AIFF Compressed)

  * RIFF variant with sections split into data "chunks"
  * chunk sizes do not include the chunk id or size (- 8)
  * the file header's size is the total size of all subsequent chunks (- 8)
  * chunk data is big endian
  * sound data is big endian, unless AIFF-C "sowt" compression type
  * the sound data chunk can be omitted if common chunk sample frames is 0
  * chunks start on even byte offsets
  * chunk string values are pascal strings:
    - first byte is string length, limited to 255 chars
    - total length must be even, pad with a '\0' byte, max 256
    - padding is not added to string length byte value
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
  * implements chunks: common, data, version (AIFF-C)
  * ignores chunks: marker, instrument, comment, name, author, copyright,
                    annotation, audio recording, MIDI data, application, ID3
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

#define AIFFCVER1    0xA2805140 /**< 2726318400 decimal */
#define AIFFMAXBYTES 0x7fffffff /**< max signed 32 bit size */

     /* compression string defines */
#define AIFF_NONE_STR "not compressed"
#define AIFF_FL32_STR "32-bit floating point"

#define AIFF_NONE_LEN 16 /**< 1 len byte + 15 bytes + 1 \0 pad byte */
#define AIFF_FL32_LEN 22 /**< 1 len byte + 22 bytes, no pad byte */

    /** basic chunk header, 8 bytes */
typedef struct _chunk
{
    char c_id[4];                    /**< chunk id                     */
    int32_t c_size;                  /**< chunk data length            */
} t_chunk;

    /** file head container chunk, 12 bytes */
typedef struct _head
{
    char h_id[4];                    /**< chunk id "FORM"              */
    int32_t h_size;                  /**< chunk data length            */
    char h_formtype[4];              /**< format: "AIFF" or "AIFC"     */
} t_head;

    /** common chunk, 26 (AIFF) or 30+ (AIFC) bytes
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
    char cc_compname[256];           /**< compression name, pascal str */
} t_commchunk;

    /** sound data chunk, min 16 bytes before data */
typedef struct _datachunk
{
    char dc_id[4];                  /**< chunk id "SSND"               */
    int32_t dc_size;                /**< chunk data length             */
    uint32_t dc_offset;             /**< additional offset in bytes    */
    uint32_t dc_block;              /**< block size                    */
} t_datachunk;

    /** AIFF-C format version chunk, 12 bytes */
typedef struct _verchunk
{
    char vc_id[4];                  /**< chunk id "FVER"               */
    int32_t vc_size;                /**< chunk data length, 4 bytes    */
    uint32_t vc_timestamp;          /**< AIFF-C version timestamp      */
} t_verchunk;

/* ----- helpers ----- */

    /** returns 1 if format requires AIFF-C */
static int aiff_isaiffc(const t_soundfile *sf)
{
    return (!sf->sf_bigendian || sf->sf_bytespersample == 4);
}

    /** pascal string to c string, max size 256, returns *total* pstring size */
static int aiff_getpstring(const char* pstring, char *cstring)
{
    uint8_t len = (uint8_t)pstring[0];
    memcpy(cstring, pstring + 1, len);
    cstring[len] = '\0';
    len++; /* length byte */
    if (len & 1) len++; /* pad byte */
    return len;
}

    /** c string to pascal string, max size 256, returns *total* pstring size */
static int aiff_setpstring(char *pstring, const char *cstring)
{
    uint8_t len = strlen(cstring);
    pstring[0] = len;
    memcpy(pstring + 1, cstring, len);
    len++; /* length byte */
    if (len & 1)
    {
        pstring[len] = '\0'; /* pad byte */
        len++;
    }
    return len;
}

    /** read byte buffer to unsigned 32 bit int */
static uint32_t aiff_get4(const uint8_t *src, int swap)
{
    uint32_t uinttmp = 0;
    memcpy(&uinttmp, src, 4);
    return swap4(uinttmp, swap);
}

    /** write unsigned 32 bit int to byte buffer */
static void aiff_set4(uint8_t* dst, uint32_t ui, int swap)
{
    ui = swap4(ui, swap);
    memcpy(dst, &ui, 4);
}

    /** read sample rate from comm chunk 80-bit AIFF-compatible number */
static double aiff_getsamplerate(const uint8_t *src, int swap)
{
    unsigned char temp[10], *p = temp, exponent;
    unsigned long mantissa, last = 0;

    memcpy(temp, src, 10);
    swapstring4((char *)p + 2, swap);

    mantissa = (uint32_t) *((uint32_t *)(p + 2));
    exponent = 30 - *(p + 1);
    while (exponent--)
    {
        last = mantissa;
        mantissa >>= 1;
    }
    if (last & 0x00000001) mantissa++;
    return mantissa;
}

    /** write a sample rate to comm chunk 80-bit AIFF-compatible number */
static void aiff_setsamplerate(uint8_t *dst, double sr)
{
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

    /** read first chunk, returns filled chunk and offset on success or -1 */
static off_t aiff_firstchunk(const t_soundfile *sf, t_chunk *chunk)
{
    if (fd_read(sf->sf_fd, AIFFHEADSIZE, (char *)chunk,
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
    if (fd_read(sf->sf_fd, seekto, (char *)chunk,
        AIFFCHUNKSIZE) < AIFFCHUNKSIZE)
        return -1;
    return seekto;
}

#ifdef DEBUG_SOUNDFILE

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
    post("  frames %u", aiff_get4(comm->cc_nframes, swap));
    post("  bits per sample %u", swap2(comm->cc_bitspersample, swap));
    post("  sample rate %g", aiff_getsamplerate(comm->cc_samplerate, swap));
    if (isaiffc)
    {
            /* handle pascal string */
        char name[256];
        aiff_getpstring(comm->cc_compname, name);
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

#endif /* DEBUG_SOUNDFILE */

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
    if (fd_read(sf->sf_fd, 0, buf.b_c, headersize) < headersize)
        return 0;
    if (strncmp(head->h_formtype, "AIFF", 4))
    {
        if (strncmp(head->h_formtype, "AIFC", 4))
            return 0;
        isaiffc = 1;
    }
#ifdef DEBUG_SOUNDFILE
    aiff_posthead(head, swap);
#endif

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
                errno = SOUNDFILE_ERRMALFORMED;
                return 0;
            }
            if (fd_read(sf->sf_fd, headersize + AIFFCHUNKSIZE,
                    buf.b_c + AIFFCHUNKSIZE, chunksize) < chunksize)
                return 0;
            if (swap4(buf.b_verchunk.vc_timestamp, swap) != AIFFCVER1)
            {
                errno = SOUNDFILE_ERRVERSION;
                return 0;
            }
        }
        else if (!strncmp(chunk->c_id, "COMM", 4))
        {
                /* common chunk */
            int bitspersample, isfloat = 0;
            t_commchunk *comm = &buf.b_commchunk;
            if (fd_read(sf->sf_fd, headersize + AIFFCHUNKSIZE,
                    buf.b_c + AIFFCHUNKSIZE, chunksize) < chunksize)
                return 0;
#ifdef DEBUG_SOUNDFILE
            aiff_postcomm(comm, isaiffc, swap);
#endif
            nchannels = swap2(comm->cc_nchannels, swap);
            bitspersample = swap2(comm->cc_bitspersample, swap);
            switch (bitspersample)
            {
                case 16: bytespersample = 2; break;
                case 24: bytespersample = 3; break;
                case 32: bytespersample = 4; break;
                default:
                {
                    errno = SOUNDFILE_ERRSAMPLEFMT;
                    return 0;
                }
            }
            samplerate = aiff_getsamplerate(comm->cc_samplerate, swap);
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
                    if (bytespersample != 4)
                    {
                        errno = SOUNDFILE_ERRMALFORMED;
                        return 0;
                    }
                    isfloat = 1;
                }
                else
                {
                    errno = SOUNDFILE_ERRSAMPLEFMT;
                    return 0;
                }
            }
            if (bytespersample == 4 && !isfloat)
            {
                    /* 32 bit int */
                errno = SOUNDFILE_ERRSAMPLEFMT;
                return 0;
            }
            commfound = 1;
        }
        else if (!strncmp(chunk->c_id, "SSND", 4))
        {
                /* uncompressed sound data chunk */
#ifdef DEBUG_SOUNDFILE
            t_datachunk *data = &buf.b_datachunk;
            if (fd_read(sf->sf_fd, headersize + AIFFCHUNKSIZE,
                    buf.b_c + AIFFCHUNKSIZE, 8) < 8)
                return 0;
            aiff_postdata(data, swap);
#endif
            bytelimit = chunksize - 8; /* - offset and block */
            headersize += AIFFDATASIZE;
            break;
        }
        else if (!strncmp(chunk->c_id, "CSND", 4))
        {
                /* AIFF-C compressed sound data chunk */
            errno = SOUNDFILE_ERRSAMPLEFMT;
            return 0;
        }
#ifdef DEBUG_SOUNDFILE
        else
        {
                /* everything else */
            aiff_postchunk(chunk, swap);
        }
#endif
        if ((headersize = aiff_nextchunk(sf, headersize, chunk)) == -1)
            return 0;
    }
    if (!commfound)
    {
        errno = SOUNDFILE_ERRMALFORMED;
        return 0;
    }

        /* interpret data size from file size? this is not supported by the
            AIFF spec, but let's do it just in case */
    if (bytelimit == AIFFMAXBYTES)
    {
        bytelimit = lseek(sf->sf_fd, 0, SEEK_END) - headersize;
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
        {0},                                      /* sample frames    */
        swap2(sf->sf_bytespersample / 8, swap), /* bits per sample  */
        {0},                                      /* sample rate      */
        {0}, {0}                                    /* comp info        */
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
    aiff_set4(comm.cc_nframes, nframes, swap);
    comm.cc_bitspersample = swap2(8 * sf->sf_bytespersample, swap);
    aiff_setsamplerate(comm.cc_samplerate, sf->sf_samplerate);
    if (isaiffc)
    {
            /* AIFF-C compression info */
        if (sf->sf_bytespersample == 4)
        {
            strncpy(comm.cc_comptype, "fl32", 4);
            commsize += 4 + aiff_setpstring(comm.cc_compname, AIFF_FL32_STR);
        }
        else
        {
            strncpy(comm.cc_comptype, (sf->sf_bigendian ? "NONE" : "sowt"), 4);
            commsize += 4 + aiff_setpstring(comm.cc_compname, AIFF_NONE_STR);
        }
    }
    comm.cc_size = swap4(commsize - AIFFCHUNKSIZE, swap);
    memcpy(buf + headersize, &comm, commsize);
    headersize += commsize;

        /* data chunk (+ offset & block) */
    data.dc_size = swap4((uint32_t)(datasize + 8), swap);
    memcpy(buf + headersize, &data, AIFFDATASIZE);
    headersize += AIFFDATASIZE;

        /* update file header chunk size (- chunk header) */
    head.h_size = swap4s((int32_t)(headersize + datasize - 8), swap);
    memcpy(buf + 4, &head.h_size, 4);

#ifdef DEBUG_SOUNDFILE
    aiff_posthead(&head, swap);
    aiff_postcomm(&comm, isaiffc, swap);
    aiff_postdata(&data, swap);
#endif

    byteswritten = fd_write(sf->sf_fd, 0, buf, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

    /** assumes chunk order:
        * AIFF  : head comm data
        * AIFF-C: head version comm data, comm chunk size variable due to str */
static int aiff_updateheader(t_soundfile *sf, size_t nframes)
{
    int isaiffc = aiff_isaiffc(sf), swap = !sys_isbigendian();
    size_t datasize = nframes * sf->sf_bytesperframe,
           headersize = AIFFHEADSIZE, commsize = AIFFCOMMSIZE;
    uint32_t uinttmp;
    int32_t inttmp;

    if (isaiffc)
    {
            /* AIFF-C compression info */
        if (sf->sf_bytespersample == 4)
            commsize += 4 + AIFF_FL32_LEN;
        else
            commsize += 4 + AIFF_NONE_LEN;
        headersize += AIFFVERSIZE;
    }

        /* num frames */
    uinttmp = swap4((uint32_t)nframes, swap);
    if (fd_write(sf->sf_fd, headersize + 10, &uinttmp, 4) < 4)
        return 0;
    headersize += commsize;

        /* data chunk size (+ offset & block) */
    inttmp = swap4s((int32_t)datasize + 8, swap);
    if (fd_write(sf->sf_fd, headersize + 4, &inttmp, 4) < 4)
        return 0;
    headersize += AIFFDATASIZE;

        /* file header chunk size (- chunk header) */
    inttmp = swap4s((int32_t)(headersize + datasize - 8), swap);
    if (fd_write(sf->sf_fd, 4, &inttmp, 4) < 4)
        return 0;

#ifdef DEBUG_SOUNDFILE
    post("FORM %d", headersize + datasize - 8);
    post("  %s", (aiff_isaiffc(sf) ? "AIFC" : "AIFF"));
    post("COMM %d", commsize);
    post("  frames %d", nframes);
    post("SSND %d", datasize + 8);
#endif

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
    if (len + 4 >= size)
        return 0;
    strcpy(filename + len, ".aif");
    return 1;
}

    /* default to big endian unless overridden */
static int aiff_endianness(int endianness)
{
    if (endianness == 0)
        return 0;
    return 1;
}

/* ------------------------- setup routine ------------------------ */

static const t_soundfile_type aiff = {
    "aiff",
    AIFFHEADSIZE + AIFFCOMMSIZE + AIFFDATASIZE,
    aiff_isheader,
    aiff_readheader,
    aiff_writeheader,
    aiff_updateheader,
    aiff_hasextension,
    aiff_addextension,
    aiff_endianness
};

void soundfile_aiff_setup( void)
{
    soundfile_addtype(&aiff);
}
