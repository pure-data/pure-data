/* Copyright (c) 1997-1999 Miller Puckette. Updated 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* ref: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/AIFF/AIFF.html */

#include "d_soundfile.h"

#include <math.h>

/* the AIFF header;  All AIFF files are big endian.  We assume the "COMM"
chunk comes first which is usually the case but perhaps not always. */

/*
TODO: add support for 32 bit float in d_soundfile.c
TODO: add related AIFC support for uncompressed PCM compression types in the
COMM chunk: "NONE" (big endian int PCM), "sowt" (little endian int PCM),
"fl32" (big endian 32 bit float)
*/

typedef struct _datachunk
{
    char dc_id[4];                  /* data chunk id "SSND"       */
    uint32_t dc_size;               /* length of data chunk       */
    uint32_t dc_offset;             /* additional offset in bytes */
    uint32_t dc_block;              /* block size                 */
} t_datachunk;

typedef struct _comm
{
    uint16_t c_nchannels;           /* number of channels         */
    uint16_t c_nframeshi;           /* # of sample frames (hi)    */
    uint16_t c_nframeslo;           /* # of sample frames (lo)    */
    uint16_t c_bitspersample;       /* bits per sample            */
    unsigned char c_samplerate[10]; /* sample rate, 80-bit float! */
} t_comm;

    /* this version is more convenient for writing them out: */
typedef struct _aiff
{
    char a_fileid[4];               /* chunk id "FORM"            */
    uint32_t a_chunksize;           /* chunk size                 */
    char a_aiffid[4];               /* aiff chunk id "AIFF"       */
    char a_fmtid[4];                /* format chunk id "COMM"     */
    uint32_t a_fmtchunksize;        /* format chunk size, 18      */
    uint16_t a_nchannels;           /* number of channels         */
    uint16_t a_nframeshi;           /* # of sample frames (hi)    */
    uint16_t a_nframeslo;           /* # of sample frames (lo)    */
    uint16_t a_bitspersample;       /* bits per sample            */
    unsigned char a_samplerate[10]; /* sample rate, 80-bit float! */
} t_aiff;

#define AIFFHDRSIZE 38 /* sizeof(t_aiff) may return 40 from alignment padding */
#define AIFFPLUS (AIFFHDRSIZE + 16)  /* header size including SSND chunk hdr */

    /* read sample rate from an 80-bit AIFF-compatible number */
static double readaiffsamprate(char * src, int swap)
{
   unsigned long mantissa, last = 0;
   unsigned char exp;

   swapstring(src + 2, swap);

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

    /* write a sample rate as an 80-bit AIFF-compatible number,
    assumes dst is a minimum of 10 bytes in length */
static void makeaiffsamprate(double sr, unsigned char *dst)
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

int soundfile_aiff_headersize()
{
    return AIFFPLUS; 
}

int soundfile_aiff_isheader(const char *buf, long size)
{
    if (size < 4) return 0;
    return !strncmp(buf, "FORM", 4);
}

int soundfile_aiff_readheader(int fd, t_soundfile_info *info)
{
    int nchannels = 1, bytespersample = 2, samplerate = 44100, headersize = 12,
        bigendian = 1, swap = (bigendian != sys_isbigendian());
    long bytesread = 0, bytelimit = SFMAXBYTES;
    union
    {
        char b_c[SFHDRBUFSIZE];
        t_datachunk b_datachunk;
        t_comm b_commchunk;
    } buf = {0};
    t_datachunk *datachunk = &buf.b_datachunk;

    /* AIFF.  same as WAVE; actually predates it.  Disgusting. */
    bytesread = read(fd, buf.b_c, AIFFPLUS);
    if (bytesread < 20 || strncmp(buf.b_c + 8, "AIFF", 4))
        return 0;

        /* First we guess a number of channels, etc., in case there's
        no COMM block to follow. */

        /* copy the first chunk header to beginning of buffer. */
    memcpy(buf.b_c, buf.b_c + headersize, sizeof(t_datachunk));
        /* read chunks in loop until we get to the data chunk */
    while (strncmp(datachunk->dc_id, "SSND", 4))
    {
        long chunksize = swap4(datachunk->dc_size,
            swap), seekto = headersize + chunksize + 8, seekout;
        if (seekto & 1)     /* pad up to even number of bytes */
            seekto++;
        /* post("chunk %c %c %c %c seek %d",
            datachunk->dc_id[0],
            datachunk->dc_id[1],
            datachunk->dc_id[2],
            datachunk->dc_id[3], seekto); */
        if (!strncmp(datachunk->dc_id, "COMM", 4))
        {
            int format;
            long commblockonset = headersize + 8;
            t_comm *commchunk;
            seekout = (long)lseek(fd, commblockonset, SEEK_SET);
            if (seekout != commblockonset)
                return 0;
            if (read(fd, buf.b_c, sizeof(t_comm)) < (int)sizeof(t_comm))
                return 0;
            commchunk = &buf.b_commchunk;
            nchannels = swap2(commchunk->c_nchannels, swap);
            format = swap2(commchunk->c_bitspersample, swap);
            switch(format)
            {
                case 16: bytespersample = 2; break;
                case 24: bytespersample = 3; break;
                case 32: error("AIFF 32 bit float not supported");
                default: return 0;
            }
            samplerate =
                readaiffsamprate((char *)commchunk->c_samplerate, swap);
        }
        seekout = (long)lseek(fd, seekto, SEEK_SET);
        if (seekout != seekto)
            return 0;
        if (read(fd, buf.b_c, sizeof(t_datachunk)) < (int)sizeof(t_datachunk))
            return 0;
        headersize = (int)seekto;
    }
    bytelimit = swap4(datachunk->dc_size, swap) - 8;
    headersize += sizeof(t_datachunk);

        /* copy sample format back to caller */
    info->i_samplerate = samplerate;
    info->i_nchannels = nchannels;
    info->i_bytespersample = bytespersample;
    info->i_headersize = headersize;
    info->i_bytelimit = bytelimit;
    info->i_bigendian = 1; /* default */

    return 1;
}

int soundfile_aiff_writeheader(int fd, const t_soundfile_info *info,
    long nframes)
{
    char headerbuf[SFHDRBUFSIZE];
    t_aiff *aiffhdr = (t_aiff *)headerbuf;
    int byteswritten = 0, headersize = AIFFPLUS;
    int swap = soundfile_info_swap(info);
    long longtmp, datasize = nframes * soundfile_info_bytesperframe(info);

    strncpy(aiffhdr->a_fileid, "FORM", 4);
    aiffhdr->a_chunksize =
        swap4((uint32_t)(datasize + sizeof(*aiffhdr) + 4), swap);
    strncpy(aiffhdr->a_aiffid, "AIFF", 4);
    strncpy(aiffhdr->a_fmtid, "COMM", 4);
    aiffhdr->a_fmtchunksize = swap4(18, swap);
    aiffhdr->a_nchannels = swap2(info->i_nchannels, swap);
    
    longtmp = swap4((uint32_t)nframes, swap);
    memcpy(&aiffhdr->a_nframeshi, &longtmp, 4);
    
    aiffhdr->a_bitspersample = swap2(8 * info->i_bytespersample, swap);
    
    makeaiffsamprate(info->i_samplerate, aiffhdr->a_samplerate);
    strncpy(((char *)(&aiffhdr->a_samplerate)) + 10, "SSND", 4);
    
    longtmp = swap4((uint32_t)(datasize + 8), swap);
    memcpy(((char *)(&aiffhdr->a_samplerate)) + 14, &longtmp, 4);
    memset(((char *)(&aiffhdr->a_samplerate)) + 18, 0, 8);

    byteswritten = (int)write(fd, headerbuf, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

int soundfile_aiff_updateheader(int fd, const t_soundfile_info *info,
    long nframes)
{
    int swap = soundfile_info_swap(info);
    long longtmp, datasize = nframes * soundfile_info_bytesperframe(info);

    // num frames
    if (lseek(fd,
        ((char *)(&((t_aiff *)0)->a_nframeshi)) - (char *)0, SEEK_SET) == 0)
        return 0;
    longtmp = swap4((uint32_t)nframes, swap);
    if (write(fd, (char *)(&longtmp), 4) < 4)
        return 0;

    // total chunk size
    if (lseek(fd,
        ((char *)(&((t_aiff *)0)->a_chunksize)) - (char *)0, SEEK_SET) == 0)
        return 0;
    longtmp = swap4((uint32_t)(datasize + AIFFHDRSIZE), swap);
    if (write(fd, (char *)(&longtmp), 4) < 4)
        return 0;

    // data size
    if (lseek(fd, (AIFFHDRSIZE + 4), SEEK_SET) == 0)
        return 0;
    longtmp = swap4((uint32_t)(datasize), swap);
    if (write(fd, (char *)(&longtmp), 4) < 4)
        return 0;

    return 1;
}

int soundfile_aiff_hasextension(const char *filename, long size)
{
    int len = strnlen(filename, size);
    if (len >= 5 &&
        (!strncmp(filename + (len - 4), ".aif", 4) ||
         !strncmp(filename + (len - 4), ".AIF", 4)))
        return 1;
    if (len >= 6 &&
        (!strncmp(filename + (len - 5), ".aiff", 5) ||
         !strncmp(filename + (len - 5), ".AIFF", 5)))
        return 1;
    return 0;
}
