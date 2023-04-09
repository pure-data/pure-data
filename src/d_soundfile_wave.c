/* Copyright (c) 1997-1999 Miller Puckette. Updated 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* ref: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html */

#include "d_soundfile.h"

/* WAVE (Waveform Audio File Format)

  * RIFF variant with sections split into data "chunks"
  * chunk sizes do not include the chunk id or size (- 8)
  * chunk and sound data are little endian
  * format and sound data chunks are required
  * format tags:
    - PCM   linear PCM
    - FLOAT 32-bit float
    - EXT   extended format -> described in subformat fields
    - the rest are not relevant to Pd...
  * extended format must be used when:
    - PCM data has more than 16 bits per sample
    - number of channels is more than 2
    - mapping of channels to speakers is required (not relevant to Pd)
  * a fact chunk is required when the format is non-PCM
  * if sound data length is odd, a 0 pad byte should appended
  * limited to ~4 GB files as sizes are unsigned 32 bit ints
  * there are variants with 64-bit sizes (W64 and RF64) as well as extension
    formats which can split sound data across multiple files (BWF)

  this implementation:

  * supports basic and extended format chunks (WAVE Rev. 3)
  * implicitly writes extended format for 32 bit float (see below)
  * implements chunks: format, fact, sound data
  * ignores chunks: info, cset, cue, playlist, associated data, instrument,
                    sample, display, junk, pad, time code, digitization time
  * assumes format chunk is always before sound data chunk
  * assumes there is only 1 sound data chunk
  * does not support 64-bit variants or BWF file-splitting
  * sample format: 16 and 24 bit lpcm, 32 bit float, no 32 bit lpcm

  Pd versions < 0.51 did *not* read or write extended format explicitly, but
  ignored the format chunk format tag and interpreted the sample type based on
  the bits per sample: 2 : int 16, 3 : int 24, 4 : float 32. This means files
  created by newer Pd versions are currently more or less compatible with older
  Pd versions. This may change if 32 bit int support is introduced.

*/

    /* explicit byte sizes, sizeof(struct) may return alignment-padded values */
#define WAVECHUNKSIZE   8 /**< chunk header only */
#define WAVEHEADSIZE   12 /**< chunk header and file format only */
#define WAVEFORMATSIZE 24 /**< chunk header and data */
#define WAVEFACTSIZE   12 /**< chunk header and data */

#define WAVEMAXBYTES 0xffffffff /**< max unsigned 32 bit size */

#define WAVE_FORMAT_PCM   0x0001 /**< 16 or 24 bit int */
#define WAVE_FORMAT_FLOAT 0x0003 /**< 32 bit float */
#define WAVE_FORMAT_EXT   0xfffe /**< extended, see format chunk subformat */

/** extended format header and data length */
#define WAVE_EXT_SIZE 24

/** 14 byte extended subformat GUID */
#define WAVE_EXT_GUID "\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71"

    /** basic chunk header, 8 bytes */
typedef struct _chunk
{
    char c_id[4];                  /**< data chunk id                   */
    uint32_t c_size;               /**< length of data chunk            */
} t_chunk;

    /** file head container chunk, 12 bytes */
typedef struct _head
{
    char h_id[4];                   /**< chunk id "RIFF"                */
    uint32_t h_size;                /**< chunk data length              */
    char h_formtype[4];             /**< format: "WAVE"                 */
} t_head;

    /** format chunk, 24 (basic) or 48 (extended) */
typedef struct _formatchunk
{
    char fc_id[4];                   /**< chunk id "fmt "               */
    uint32_t fc_size;                /**< chunk data length             */
    uint16_t fc_fmttag;              /**< format tag                    */
    uint16_t fc_nchannels;           /**< number of channels            */
    uint32_t fc_samplerate;          /**< sample rate in hz             */
    uint32_t fc_bytespersecond;      /**< average bytes per second      */
    uint16_t fc_blockalign;          /**< number of bytes per frame     */
    uint16_t fc_bitspersample;       /**< number of bits in a sample    */
    /* extended format */
    uint16_t fc_extsize;             /**< extended format info length   */
    uint16_t fc_validbitspersample;  /**< number of valid bits          */
    uint32_t fc_channelmask;         /**< speaker pos channel mask      */
    char fc_subformat[16];           /**< format tag is bytes 0 & 1     */
} t_formatchunk;

    /** fact chunk, 12 bytes */
typedef struct _factchunk
{
    char fc_id[4];                   /**< chunk id "fact"               */
    uint32_t fc_size;                /**< chunk data length             */
    uint32_t fc_samplelength;        /**< number of samples per channel */
} t_factchunk;

/* ----- helpers ----- */

    /** returns 1 if format requires extended format and fact chunk */
static int wave_isextended(const t_soundfile *sf)
{
    return sf->sf_bytespersample == 4;
}

    /** read first chunk, returns filled chunk and offset on success or -1 */
static off_t wave_firstchunk(const t_soundfile *sf, t_chunk *chunk)
{
    if (fd_read(sf->sf_fd, WAVEHEADSIZE, (char *)chunk,
        WAVECHUNKSIZE) < WAVECHUNKSIZE)
        return -1;
    return WAVEHEADSIZE;
}

    /** read next chunk, chunk should be filled when calling
        returns fills chunk offset on success or -1 */
static off_t wave_nextchunk(const t_soundfile *sf, off_t offset, t_chunk *chunk)
{
    uint32_t chunksize = swap4(chunk->c_size, sys_isbigendian());
    off_t seekto = offset + WAVECHUNKSIZE + chunksize;
    if (seekto & 1) /* pad up to even number of bytes */
        seekto++;
    if (fd_read(sf->sf_fd, seekto, (char *)chunk,
        WAVECHUNKSIZE) < WAVECHUNKSIZE)
        return -1;
    return seekto;
}

#ifdef DEBUG_SOUNDFILE

    /** post chunk info for debugging */
static void wave_postchunk(const t_chunk *chunk, int swap)
{
    post("%.4s %d", chunk->c_id, swap4s(chunk->c_size, swap));
}

    /** post head info for debugging */
static void wave_posthead(const t_head *head, int swap)
{
    wave_postchunk((const t_chunk *)head, swap);
    post("  %.4s", head->h_formtype);
}

    /** post format info for debugging */
static void wave_postformat(const t_formatchunk *format, int swap)
{
    uint16_t formattag = swap2(format->fc_fmttag, swap);
    wave_postchunk((const t_chunk *)format, swap);
    switch (formattag)
    {
        case WAVE_FORMAT_PCM:
            post("  format %d (PCM)", formattag);
            break;
        case WAVE_FORMAT_FLOAT:
            post("  format %d (FLOAT)", formattag);
            break;
        case WAVE_FORMAT_EXT:
            post("  format %d (EXT)", formattag);
            break;
        default:
            post("  format %d (unsupported)", formattag);
            break;
    }
    post("  channels %d", swap2(format->fc_nchannels, swap));
    post("  sample rate %d", swap4(format->fc_samplerate, swap));
    post("  bytes per sec %d", swap4(format->fc_bytespersecond, swap));
    post("  block align %d", swap2(format->fc_blockalign, swap));
    post("  bits per sample %u", swap2(format->fc_bitspersample, swap));
    if (formattag == WAVE_FORMAT_EXT)
    {
        formattag = swap2(*(uint16_t *)&format->fc_subformat, swap);
        post("  ext size %d", swap2(format->fc_extsize, swap));
        post("  ext valid bits per sample %d",
            swap2(format->fc_validbitspersample, swap));
        post("  ext channel mask 0x%04x", swap4(format->fc_channelmask, swap));
        switch (formattag)
        {
            case WAVE_FORMAT_PCM:
                post("  ext format %d (PCM)", formattag);
                break;
            case WAVE_FORMAT_FLOAT:
                post("  ext format %d (FLOAT)", formattag);
                break;
            default:
                post("  ext format %d (unsupported)", formattag);
                break;
        }
    }
}

    /** post fact info for debugging */
static void wave_postfact(const t_factchunk *fact, int swap)
{
    wave_postchunk((const t_chunk *)fact, swap);
    post("  sample length %d", swap4(fact->fc_samplelength, swap));
}

#endif /* DEBUG_SOUNDFILE */

/* ------------------------- WAVE ------------------------- */

static int wave_isheader(const char *buf, size_t size)
{
    if (size < 4) return 0;
    return !strncmp(buf, "RIFF", 4);
}

static int wave_readheader(t_soundfile *sf)
{
    int nchannels = 1, bytespersample = 2, samplerate = 44100, bigendian = 0,
        swap = (bigendian != sys_isbigendian()), formatfound = 1;
    off_t headersize = WAVEHEADSIZE;
    size_t bytelimit = WAVEMAXBYTES;
    union
    {
        char b_c[SFHDRBUFSIZE];
        t_head b_head;
        t_chunk b_chunk;
        t_formatchunk b_formatchunk;
        t_factchunk b_factchunk;
    } buf = {0};
    t_chunk *chunk = &buf.b_chunk;

        /* file header */
    if (fd_read(sf->sf_fd, 0, buf.b_c, headersize) < headersize)
        return 0;
    if (strncmp(buf.b_c + 8, "WAVE", 4))
        return 0;
#ifdef DEBUG_SOUNDFILE
        wave_posthead(&buf.b_head, swap);
#endif

        /* read chunks in loop until we find the sound data chunk */
    if ((headersize = wave_firstchunk(sf, chunk)) == -1)
        return 0;
    while (1)
    {
        uint32_t chunksize = swap4(chunk->c_size, swap);
        /* post("chunk %.4s seek %d", chunk->c_id, seekto); */
        if (!strncmp(chunk->c_id, "fmt ", 4))
        {
                /* format chunk */
            int formattag;
            t_formatchunk *format = &buf.b_formatchunk;
            if (fd_read(sf->sf_fd, headersize + 8,
                    buf.b_c + 8, chunksize) < chunksize)
                return 0;
#ifdef DEBUG_SOUNDFILE
            wave_postformat(format, swap);
#endif
            nchannels = swap2(format->fc_nchannels, swap);
            samplerate = swap4(format->fc_samplerate, swap);
            formattag = swap2(format->fc_fmttag, swap);
            if (formattag == WAVE_FORMAT_EXT && chunksize == WAVEFORMATSIZE)
            {
                errno = SOUNDFILE_ERRSAMPLEFMT;
                return 0;
            }
            switch (formattag)
            {
                case WAVE_FORMAT_PCM: case WAVE_FORMAT_FLOAT: break;
                case WAVE_FORMAT_EXT:
                    formattag = swap2(*(uint16_t *)&format->fc_subformat, swap);
                    if (formattag == WAVE_FORMAT_PCM ||
                        formattag == WAVE_FORMAT_FLOAT)
                            break;
                default:
                {
                    errno = SOUNDFILE_ERRSAMPLEFMT;
                    return 0;
                }
            }
            bytespersample = swap2(format->fc_bitspersample, swap) / 8;
            switch (bytespersample)
            {
                case 2: case 3: break;
                case 4:
                    if (formattag == WAVE_FORMAT_FLOAT) /* 32 bit int? */
                        break;
                default:
                    errno = SOUNDFILE_ERRSAMPLEFMT;
                    return 0;
            }

            formatfound = 1;
        }
        else if(!strncmp(chunk->c_id, "data", 4))
        {
                /* sound data chunk */
            bytelimit = swap4(chunk->c_size, swap);
            headersize += WAVECHUNKSIZE;
#ifdef DEBUG_SOUNDFILE
            wave_postchunk(chunk, swap);
#endif
            break;
        }
#ifdef DEBUG_SOUNDFILE
        else if (!strncmp(chunk->c_id, "fact", 4))
        {
                /* extended format fact chunk */
            wave_postfact(&buf.b_factchunk, swap);
        }
        else
        {
                /* everything else */
            wave_postchunk(chunk, swap);
        }
#endif
        if ((headersize = wave_nextchunk(sf, headersize, chunk)) == -1)
            return 0;
    }
    if (!formatfound)
    {
        errno = SOUNDFILE_ERRMALFORMED;
        return 0;
    }

        /* interpret data size from file size? */
    if (bytelimit == WAVEMAXBYTES)
    {
        bytelimit = lseek(sf->sf_fd, 0, SEEK_END) - headersize;
        if (bytelimit > WAVEMAXBYTES || bytelimit < 0)
            bytelimit = WAVEMAXBYTES;
    } else if (bytelimit & 1) {
            /* the actual data chunk size is always even */
        bytelimit++;
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

static int wave_writeheader(t_soundfile *sf, size_t nframes)
{
    int isextended = wave_isextended(sf), swap = soundfile_needsbyteswap(sf);
    size_t formatsize = WAVEFORMATSIZE,
           datasize = nframes * sf->sf_bytesperframe;
    off_t headersize = 0;
    ssize_t byteswritten = 0;
    char buf[SFHDRBUFSIZE] = {0};
    t_head head = {"RIFF", 0, "WAVE"};
    t_formatchunk format = {
        "fmt ", swap4(16, swap),
        WAVE_FORMAT_PCM,                                  /* format tag      */
        swap2((uint16_t)sf->sf_nchannels, swap),          /* channels        */
        swap4((uint32_t)sf->sf_samplerate, swap),         /* sample rate     */
        swap4((uint32_t)(sf->sf_samplerate *              /* bytes per sec   */
                         sf->sf_bytesperframe), swap),
        swap2((uint16_t)sf->sf_bytesperframe, swap),      /* block align     */
        swap2((uint16_t)sf->sf_bytespersample * 8, swap), /* bits per sample */
        0                                                 /* extended format */
    };
    t_chunk data = {"data", swap4((uint32_t)datasize, swap)};

        /* file header */
    memcpy(buf + headersize, &head, WAVEHEADSIZE);
    headersize += WAVEHEADSIZE;

        /* format chunk */
    if (sf->sf_bytespersample == 4)
        format.fc_fmttag = swap2(WAVE_FORMAT_FLOAT, swap);
    if (isextended)
    {
        format.fc_extsize = swap2(WAVE_EXT_SIZE - 2, swap); /* - fmttag */
        format.fc_validbitspersample = format.fc_bitspersample;
        format.fc_channelmask = 0; /* no specific speaker positions */
        memcpy(format.fc_subformat, &format.fc_fmttag, 2); /* move tag */
        memcpy(format.fc_subformat + 2, WAVE_EXT_GUID, 14);
        format.fc_fmttag = swap2(WAVE_FORMAT_EXT, swap);
        formatsize += WAVE_EXT_SIZE;
    }
    format.fc_size = swap4(formatsize - 8, swap);
    memcpy(buf + headersize, &format, formatsize);
    headersize += formatsize;

        /* fact chunk */
    if (isextended)
    {
        t_factchunk fact = {
            "fact", swap4(4, swap),
            swap4((uint32_t)(sf->sf_nchannels * nframes), swap)
        };
        memcpy(buf + headersize, &fact, WAVEFACTSIZE);
        headersize += WAVEFACTSIZE;
    }

        /* data chunk */
    if (datasize & 1)
    {
            /* add pad byte */
        data.c_size = swap4((uint32_t)datasize + 1, swap);
    }
    memcpy(buf + headersize, &data, WAVECHUNKSIZE);
    headersize += WAVECHUNKSIZE;

        /* update file header chunk size (- chunk header) */
    head.h_size = swap4s((int32_t)(datasize + headersize - 8), swap);
    memcpy(buf + 4, &head.h_size, 4);

#ifdef DEBUG_SOUNDFILE
    wave_posthead(&head, swap);
    wave_postformat(&format, swap);
    wave_postchunk(&data, swap);
#endif

    byteswritten = fd_write(sf->sf_fd, 0, buf, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

    /** assumes chunk order:
        * basic:    head format data
        * extended: head format+ext fact data */
static int wave_updateheader(t_soundfile *sf, size_t nframes)
{
    int isextended = wave_isextended(sf), swap = soundfile_needsbyteswap(sf);
    size_t datasize = nframes * sf->sf_bytesperframe,
           headersize = WAVEHEADSIZE + WAVEFORMATSIZE;
    int padbyte = (datasize & 1);
    uint32_t uinttmp;

    if (isextended)
    {
        headersize += WAVE_EXT_SIZE;

            /* fact chunk sample length */
        uinttmp = swap4((uint32_t)(nframes * sf->sf_nchannels), swap);
        if (fd_write(sf->sf_fd, headersize + 8, &uinttmp, 4) < 4)
            return 0;
        headersize += WAVEFACTSIZE;
    }

        /* sound data chunk size */
    datasize += padbyte;
    uinttmp = swap4((uint32_t)datasize, swap);
    if (fd_write(sf->sf_fd, headersize + 4, &uinttmp, 4) < 4)
        return 0;
    headersize += WAVECHUNKSIZE;

        /* add pad byte */
    if (padbyte)
    {
        uinttmp = 0;
        if (fd_write(sf->sf_fd, headersize + datasize - 1, &uinttmp, 1) < 1)
            return 0;
    }

        /* file header chunk size (- chunk header) */
    uinttmp = swap4((uint32_t)(headersize + datasize - 8), swap);
    if (fd_write(sf->sf_fd, 4, &uinttmp, 4) < 4)
        return 0;

#ifdef DEBUG_SOUNDFILE
        post("RIFF %d", headersize + datasize - 8);
        post("  WAVE");
        post("data %d", datasize);
#endif

    return 1;
}

static int wave_hasextension(const char *filename, size_t size)
{
    int len = strnlen(filename, size);
    if (len >= 5 &&
        (!strncmp(filename + (len - 4), ".wav", 4) ||
         !strncmp(filename + (len - 4), ".WAV", 4)))
        return 1;
    if (len >= 6 &&
        (!strncmp(filename + (len - 5), ".wave", 5) ||
         !strncmp(filename + (len - 5), ".WAVE", 5)))
        return 1;
    return 0;
}

static int wave_addextension(char *filename, size_t size)
{
    int len = strnlen(filename, size);
    if (len + 4 >= size)
        return 0;
    strcpy(filename + len, ".wav");
    return 1;
}

    /* force little endian */
static int wave_endianness(int endianness)
{
    return 0;
}

/* ------------------------- setup routine ------------------------ */

t_soundfile_type wave = {
    "wave",
    WAVEHEADSIZE + WAVEFORMATSIZE + WAVECHUNKSIZE,
    wave_isheader,
    wave_readheader,
    wave_writeheader,
    wave_updateheader,
    wave_hasextension,
    wave_addextension,
    wave_endianness
};

void soundfile_wave_setup( void)
{
    soundfile_addtype(&wave);
}
