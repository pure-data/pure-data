/* Copyright (c) 1997-1999 Miller Puckette. Updated 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* ref: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html */

#include "d_soundfile.h"

/* the WAVE header structure;  All Wave files are little endian.  We assume
the "fmt" chunk comes first which is usually the case but perhaps not always. */

/* TODO: handle extensible format PCM data */
/* TODO: add support for BWF and/or RF64 (file id "RF64" and "DS64" chunk) */

typedef struct _wave
{
    char w_fileid[4];               /* chunk id "RIFF"            */
    uint32_t w_chunksize;           /* chunk size                 */
    char w_waveid[4];               /* wave chunk id "WAVE"       */
    char w_fmtid[4];                /* format chunk id "fmt "     */
    uint32_t w_fmtchunksize;        /* format chunk size          */
    uint16_t w_fmttag;              /* format tag, see below      */
    uint16_t w_nchannels;           /* number of channels         */
    uint32_t w_samplerate;          /* sample rate in hz          */
    uint32_t w_navgbytespersec;     /* data rate in bytes per sec */
    uint16_t w_nblockalign;         /* number of bytes per frame  */
    uint16_t w_nbitspersample;      /* number of bits in a sample */
    char w_datachunkid[4];          /* data chunk id 'data'       */
    uint32_t w_datachunksize;       /* length of data chunk       */
} t_wave;

typedef struct _fmt         /* format chunk */
{
    uint16_t f_fmttag;              /* format tag, see below      */
    uint16_t f_nchannels;           /* number of channels         */
    uint32_t f_samplerate;          /* sample rate in hz          */
    uint32_t f_navgbytespersec;     /* average bytes per second   */
    uint16_t f_nblockalign;         /* number of bytes per frame  */
    uint16_t f_nbitspersample;      /* number of bits in a sample */
} t_fmt;

typedef struct _wavechunk           /* ... and the last two items */
{
    char wc_id[4];                  /* data chunk id: "data" "fmt " or "fact" */
    uint32_t wc_size;               /* length of data chunk       */
} t_wavechunk;

#define WAV_FORMAT_PCM   0x0001 /* 16 or 24 bit int */
#define WAV_FORMAT_FLOAT 0x0003 /* 32 bit float */
#define WAV_FORMAT_EXT   0xfffe /* extensible, format in fmt chunk subformat */

int soundfile_wave_headersize()
{
    return sizeof(t_wave);
}

int soundfile_wave_isheader(const char *buf, long size)
{
    if (size < 4) return 0;
    return !strncmp(buf, "RIFF", 4);
}

int soundfile_wave_readheader(int fd, t_soundfile_info *info)
{
    int nchannels = 1, bytespersample = 2, samplerate = 44100, headersize = 12,
        bigendian = 0, swap = (bigendian != sys_isbigendian());
    long bytesread = 0, bytelimit = SFMAXBYTES;
    union
    {
        char b_c[SFHDRBUFSIZE];
        t_fmt b_fmt;
        t_wavechunk b_wavechunk;
    } buf = {0};
    t_wavechunk *wavechunk = &buf.b_wavechunk;

    /*  This is awful.  You have to skip over chunks,
   except that if one happens to be a "fmt" chunk, you want to
   find out the format from that one.  The case where the
   "fmt" chunk comes after the audio isn't handled. */
    bytesread = read(fd, buf.b_c, sizeof(t_wave));
    if (bytesread < 20 || strncmp(buf.b_c + 8, "WAVE", 4))
        return 0;

        /* First we guess a number of channels, etc., in case there's
        no "fmt" chunk to follow. */

        /* copy the first chunk header to beginnning of buffer. */
    memcpy(buf.b_c, buf.b_c + headersize, sizeof(t_wavechunk));
    /* post("chunk %c %c %c %c",
            wavechunk->wc_id[0],
            wavechunk->wc_id[1],
            wavechunk->wc_id[2],
            wavechunk->wc_id[3]); */
        /* read chunks in loop until we get to the data chunk */
    while (strncmp(wavechunk->wc_id, "data", 4))
    {
        long chunksize = swap4(wavechunk->wc_size, swap);
        long seekto = headersize + chunksize + 8, seekout;
        if (seekto & 1) /* pad up to even number of bytes */
            seekto++;
        if (!strncmp(wavechunk->wc_id, "fmt ", 4))
        {
            int format;
            long commblockonset = headersize + 8;
            seekout = (long)lseek(fd, commblockonset, SEEK_SET);
            if (seekout != commblockonset)
                return 0;
            if (read(fd, buf.b_c, sizeof(t_fmt)) < (int) sizeof(t_fmt))
                return 0;
            nchannels = swap2(buf.b_fmt.f_nchannels, swap);
            samplerate = swap4(buf.b_fmt.f_samplerate, swap);
            format = swap2(buf.b_fmt.f_fmttag, swap);
            switch(format)
            {
                case WAV_FORMAT_PCM: case WAV_FORMAT_FLOAT: break;
                case WAV_FORMAT_EXT: default:
                    error("WAVE unsupported format 0x%x", format); return 0;
            }
            bytespersample = swap2(buf.b_fmt.f_nbitspersample, swap) / 8;
            if (bytespersample == 4 && format != WAV_FORMAT_FLOAT)
            {
                error("WAVE 32 bit int not supported");
                return 0;
            }
        }
        seekout = (long)lseek(fd, seekto, SEEK_SET);
        if (seekout != seekto)
            return 0;
        if (read(fd, buf.b_c, sizeof(t_wavechunk)) < (int)sizeof(t_wavechunk))
            return 0;
        /* post("new chunk %c %c %c %c at %d",
            wavechunk->wc_id[0],
            wavechunk->wc_id[1],
            wavechunk->wc_id[2],
            wavechunk->wc_id[3], seekto); */
        headersize = (int)seekto;
    }
    bytelimit = swap4(wavechunk->wc_size, swap);
    headersize += sizeof(t_wavechunk);

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
    long nframes)
{
    char headerbuf[SFHDRBUFSIZE];
    t_wave *wavehdr = (t_wave *)headerbuf;
    int byteswritten = 0, headersize = sizeof(t_wave);
    int swap = soundfile_info_swap(info);
    long datasize = nframes * info->i_bytesperframe;
    
    strncpy(wavehdr->w_fileid, "RIFF", 4);
    wavehdr->w_chunksize = swap4((uint32_t)(datasize + headersize - 8), swap);
    strncpy(wavehdr->w_waveid, "WAVE", 4);
    strncpy(wavehdr->w_fmtid, "fmt ", 4);
    wavehdr->w_fmtchunksize = swap4(16, swap);
    switch(info->i_bytespersample) {
        case 4:  wavehdr->w_fmttag = swap2(WAV_FORMAT_FLOAT, swap); break;
        default: wavehdr->w_fmttag = swap2(WAV_FORMAT_PCM, swap); break;
    }
    wavehdr->w_nchannels = swap2(info->i_nchannels, swap);
    wavehdr->w_samplerate = swap4(info->i_samplerate, swap);
    wavehdr->w_navgbytespersec =
        swap4(info->i_samplerate * info->i_bytesperframe, swap);
    wavehdr->w_nblockalign = swap2(info->i_bytesperframe, swap);
    wavehdr->w_nbitspersample = swap2(8 * info->i_bytespersample, swap);
    strncpy(wavehdr->w_datachunkid, "data", 4);
    wavehdr->w_datachunksize = swap4((uint32_t)datasize, swap);

    byteswritten = (int)write(fd, headerbuf, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

int soundfile_wave_updateheader(int fd, const t_soundfile_info *info,
    long nframes)
{
    int swap = soundfile_info_swap(info);
    long longtmp, datasize = nframes * info->i_bytesperframe;

    // total chunk size
    if (lseek(fd,
        ((char *)(&((t_wave *)0)->w_chunksize)) - (char *)0, SEEK_SET) == 0)
        return 0;
    longtmp = swap4((uint32_t)(datasize + sizeof(t_wave) - 8), swap);
    if (write(fd, (char *)(&longtmp), 4) < 4)
        return 0;

    // data size
    if (lseek(fd,
        ((char *)(&((t_wave *)0)->w_datachunksize)) - (char *)0, SEEK_SET) == 0)
        return 0;
    longtmp = swap4((uint32_t)datasize, swap);
    if (write(fd, (char *)(&longtmp), 4) < 4)
        return 0;

    return 1;
}

// TODO: length-safe version?
int soundfile_wave_hasextension(const char *filename, long size)
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
