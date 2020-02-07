/* Copyright (c) 1997-1999 Miller Puckette. Updated 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* refs: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
         http://soundfile.sapp.org/doc/WaveFormat */

#include "d_soundfile.h"
#include "s_stuff.h" /* for sys_verbose */

/* WAVE header (Waveform Audio File Format)

  * RIFF variant with sections split into data "chunks"
  * chunk and sound data are little endian
  * format and sound data chunks are required
  * format tags:
    - PCM   linear PCM
    - FLOAT 32-bit float
    - EXT   extensible format -> described in subformat fields
    - the rest are not relevant to Pd...
  * extensible format must be used when:
    - PCM data has more than 16 bits per sample
    - number of channels is more than 2
    - mapping of channels to speakers is required (not relevant to Pd)
  * a fact chunk is required when the format is non-PCM
  * limited to ~4 GB files as sizes are unsigned 32 bit ints
  * there are variants with 64-bit sizes (W64 and RF64) as well as extension
    formats which can split sound data across multiple files (BWF)

  this implementation:

  * supports basic and extended format chunks (WAVE Rev. 3)
  * implements chunks: format, fact, sound data
  * ignores chunks: info, cset, cue, playlist, associated data, instrument,
                    sample, display, junk, pad, time code, digitization time,
  * assumes format chunk is always before sound data chunk
  * assumes there is only 1 sound data chunk
  * ignores speaker channel mapping
  * does not support 64-bit variants or BWF file-splitting
  * sample format: 16 and 24 bit lpcm, 32 bit float, no 32 bit lpcm

*/

/* TODO: handle extensible format info */

    /* explicit byte sizes, sizeof(struct) may return alignment-padded values */
#define WAVECHUNKSIZE   8 /**< chunk header only */
#define WAVEHEADSIZE   12 /**< chunk header and file format only */
#define WAVEFORMATSIZE 24 /**< chunk header and data */
#define WAVEFACTSIZE   12 /**< chunk header and data */

#define WAVE_FORMAT_PCM   0x0001 /* 16 or 24 bit int */
#define WAVE_FORMAT_FLOAT 0x0003 /* 32 bit float */
#define WAVE_FORMAT_EXT   0xfffe /* extensible, see format chunk subformat */

    /** basic chunk header, 8 bytes */
typedef struct _chunk
{
    char c_id[4];                  /**< data chunk id                 */
    uint32_t c_size;               /**< length of data chunk          */
} t_chunk;

    /** file header, 12 bytes */
typedef struct _head
{
    char h_id[4];                   /**< chunk id "RIFF"              */
    uint32_t h_size;                /**< chunk data length            */
    char h_formtype[4];             /**< format: "WAVE"               */
} t_head;

    /** format chunk, 24 (basic) or ?? (extensible) */
typedef struct _formatchunk
{
    char fc_id[4];                   /**< chunk id "fmt "             */
    uint32_t fc_size;                /**< chunk data length           */
    uint16_t fc_fmttag;              /**< format tag                  */
    uint16_t fc_nchannels;           /**< number of channels          */
    uint32_t fc_samplerate;          /**< sample rate in hz           */
    uint32_t fc_bytespersecond;      /**< average bytes per second    */
    uint16_t fc_blockalign;          /**< number of bytes per frame   */
    uint16_t fc_bitspersample;       /**< number of bits in a sample  */
} t_formatchunk;

    /** fact chunk, 12 bytes */
typedef struct _factchunk
{
    char fc_id[4];                   /**< chunk id "fact"             */
    uint32_t fc_size;                /**< chunk data length           */
    uint32_t fc_samplelength;        /**< number of samples           */
} t_factchunk;

/* ----- helpers ----- */

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
        default:
            post("  format %d (unsupported)", formattag);
            break;
    }
    post("  channels %d", swap2(format->fc_nchannels, swap));
    post("  sample rate %d", swap4(format->fc_samplerate, swap));
    post("  bytes per sec %d", swap4(format->fc_bytespersecond, swap));
    post("  block align %d", swap2(format->fc_blockalign, swap));
    post("  bits per sample %u", swap2(format->fc_bitspersample, swap));
}

/* ------------------------- WAVE ------------------------- */

int soundfile_wave_headersize()
{
    return WAVEHEADSIZE + WAVEFORMATSIZE + WAVECHUNKSIZE;
}

int soundfile_wave_isheader(const char *buf, size_t size)
{
    if (size < 4) return 0;
    return !strncmp(buf, "RIFF", 4);
}

int soundfile_wave_readheader(int fd, t_soundfile_info *info)
{
    int nchannels = 1, bytespersample = 2, samplerate = 44100, bigendian = 0,
        swap = (bigendian != sys_isbigendian()), formatfound = 1;
    off_t headersize = WAVEHEADSIZE + WAVECHUNKSIZE;
    size_t bytelimit = SFMAXBYTES;
    union
    {
        char b_c[SFHDRBUFSIZE];
        t_head b_head;
        t_chunk b_chunk;
        t_formatchunk b_formatchunk;
    } buf = {0};
    t_chunk *chunk = &buf.b_chunk;

        /* file header */
    if (soundfile_readbytes(fd, 0, buf.b_c, headersize) < headersize)
        return 0;
    if (strncmp(buf.b_c + 8, "WAVE", 4))
        return 0;
    if (sys_verbose)
        wave_posthead(&buf.b_head, swap);

        /* copy the first chunk header to beginnning of buffer */
    memcpy(buf.b_c, buf.b_c + WAVEHEADSIZE, WAVECHUNKSIZE);
    headersize = WAVEHEADSIZE;

        /* read chunks in loop until we find the sound data chunk */
    while (1)
    {
        uint32_t chunksize = swap4(chunk->c_size, swap);
        long seekto = headersize + chunksize + 8, seekout;
        if (seekto & 1) /* pad up to even number of bytes */
            seekto++;
        /* post("chunk %.4s seek %d", chunk->c_id, seekto); */
        if (!strncmp(chunk->c_id, "fmt ", 4))
        {
                /* format chunk */
            int formattag;
            t_formatchunk *format = &buf.b_formatchunk;
            if (soundfile_readbytes(fd, headersize + 8,
                                    buf.b_c + 8, chunksize) < chunksize)
                return 0;
            if (sys_verbose)
                wave_postformat(format, swap);
            nchannels = swap2(format->fc_nchannels, swap);
            samplerate = swap4(format->fc_samplerate, swap);
            formattag = swap2(format->fc_fmttag, swap);
            switch(formattag)
            {
                case WAVE_FORMAT_PCM: case WAVE_FORMAT_FLOAT: break;
                case WAVE_FORMAT_EXT: default:
                    error("WAVE unsupported format %d", formattag); return 0;
            }
            bytespersample = swap2(format->fc_bitspersample, swap) / 8;
            if (bytespersample == 4 && formattag != WAVE_FORMAT_FLOAT)
            {
                error("WAVE 32 bit int not supported");
                return 0;
            }
            formatfound = 1;
        }
        else if(!strncmp(chunk->c_id, "data", 4))
        {
                /* sound data chunk */
            bytelimit = swap4(chunk->c_size, swap);
            headersize += WAVECHUNKSIZE;
            if (sys_verbose)
                wave_postchunk(chunk, swap);
            break;
        }
        else
        {
                /* everything else */
            if (sys_verbose)
                wave_postchunk(chunk, swap);
        }
        if (soundfile_readbytes(fd, seekto, buf.b_c,
                                WAVECHUNKSIZE) < WAVECHUNKSIZE)
            return 0;
        headersize = seekto;
    }
    if (!formatfound) {
        error("WAVE format chunk not found");
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

int soundfile_wave_writeheader(int fd, const t_soundfile_info *info,
    size_t nframes)
{
    int swap = soundfile_info_swap(info);
    size_t formatsize = WAVEFORMATSIZE,
           datasize = nframes * info->i_bytesperframe;
    off_t headersize = 0;
    ssize_t byteswritten = 0;
    char buf[SFHDRBUFSIZE];
    t_head head = {"RIFF", 0, "WAVE"};
    t_formatchunk format = {
        "fmt ", swap4(16, swap),
        WAVE_FORMAT_PCM,                                   /* format tag      */
        swap2((uint16_t)info->i_nchannels, swap),          /* channels        */
        swap4((uint32_t)info->i_samplerate, swap),         /* sample rate     */
        swap4((uint32_t)(info->i_samplerate *              /* bytes per sec   */
                         info->i_bytesperframe), swap),
        swap2((uint16_t)info->i_bytesperframe, swap),      /* block align     */
        swap2((uint16_t)info->i_bytespersample * 8, swap), /* bits per sample */
    };
    t_chunk data = {"data", swap4((uint32_t)(datasize + 8), swap)};

        /* file header */
    memcpy(buf + headersize, &head, WAVEHEADSIZE);
    headersize += WAVEHEADSIZE;

        /* format chunk */
    if (info->i_bytespersample == 4)
        format.fc_fmttag = swap2(WAVE_FORMAT_FLOAT, swap);
    format.fc_size = swap4(formatsize - 8, swap);
    memcpy(buf + headersize, &format, formatsize);
    headersize += formatsize;

        /* data chunk */
    memcpy(buf + headersize, &data, WAVECHUNKSIZE);
    headersize += WAVECHUNKSIZE;

    byteswritten = soundfile_writebytes(fd, 0, buf, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

    /** assumes chunk order: head format data */
int soundfile_wave_updateheader(int fd, const t_soundfile_info *info,
    size_t nframes)
{
    int swap = soundfile_info_swap(info);
    size_t datasize = nframes * info->i_bytesperframe,
           headersize = WAVEHEADSIZE + WAVEFORMATSIZE;
    uint32_t uinttmp;

        /* sound data chunk size */
    uinttmp = swap4((uint32_t)datasize, swap);
    if (soundfile_writebytes(fd, headersize + 4, (char *)&uinttmp, 4) < 4)
        return 0;
    headersize += WAVECHUNKSIZE;

        /* file header chunk size */
    uinttmp = swap4((uint32_t)(headersize + datasize - 8), swap);
    if (soundfile_writebytes(fd, 4, (char *)&uinttmp, 4) < 4)
        return 0;

    return 1;
}

int soundfile_wave_hasextension(const char *filename, size_t size)
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
