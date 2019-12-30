/* Copyright (c) 1997-1999 Miller Puckette. Updated 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/AU/AU.html */

#include "d_soundfile.h"

#ifdef _LARGEFILE64_SOURCE
# define lseek lseek64
#endif

/* the NeXTStep/Sun sound header structure; can be big or little endian  */

typedef struct _nextstep
{
    char ns_fileid[4];      /* magic number; ".snd" (big endian) or "dns." */
    uint32_t ns_onset;      /* byte offset of first sample */
    uint32_t ns_length;     /* length of sound in bytes */
    uint32_t ns_format;     /* data format code; see below */
    uint32_t ns_samplerate; /* sample rate */
    uint32_t ns_nchannels;  /* number of channels */
    char ns_info[4];        /* comment string, minimum 4 bytes up to onset */
} t_nextstep;

#define NS_FORMAT_LINEAR_16 3 /* 16 bit int */
#define NS_FORMAT_LINEAR_24 4 /* 24 bit int */
#define NS_FORMAT_FLOAT     6 /* 32 bit float */

int soundfile_next_headersize()
{
    return sizeof(t_nextstep);
}

int soundfile_next_isheader(const char *buf, long size)
{
    if (size < 4) return 0;
    if (!strncmp(buf, ".snd", 4) || !strncmp(buf, "dns.", 4))
        return 1;
    return 0;
}

int soundfile_next_readheader(int fd, t_soundfile_info *info)
{
    int format, bytespersample, bigendian = 0, headersize = 24, swap;
    long bytesread = 0;
    union
    {
        char b_c[SFHDRBUFSIZE];
        t_nextstep b_nextstep;
    } buf = {0};
    t_nextstep *nsbuf = &buf.b_nextstep;

    bytesread = read(fd, buf.b_c, sizeof(t_nextstep));
    if (bytesread < sizeof(t_nextstep))
        return 0;

    if(!strncmp(nsbuf->ns_fileid, ".snd", 4))
        bigendian = 1;
    else if(!strncmp(nsbuf->ns_fileid, "dns.", 4))
        bigendian = 0;
    else
        return 0;
    swap = (bigendian != sys_isbigendian());

    headersize = swap4(nsbuf->ns_onset, swap);
    if (headersize < 24) /* min valid header size */
        return 0;

    format = swap4(nsbuf->ns_format, swap);
    switch (format)
    {
        case NS_FORMAT_LINEAR_16: bytespersample = 2; break;
        case NS_FORMAT_LINEAR_24: bytespersample = 3; break;
        case NS_FORMAT_FLOAT:     bytespersample = 4; break;
        default: error("NEXT unsupported format %d", format); return 0;
    }

        /* copy sample format back to caller */
    info->i_samplerate = swap4(nsbuf->ns_samplerate, swap);
    info->i_nchannels = swap4(nsbuf->ns_nchannels, swap);
    info->i_bytespersample = bytespersample;
    info->i_headersize = headersize;
    info->i_bytelimit = SFMAXBYTES;
    info->i_bigendian = bigendian;

    return 1;
}

int soundfile_next_writeheader(int fd, const t_soundfile_info *info,
    long nframes)
{
    char headerbuf[SFHDRBUFSIZE];
    t_nextstep *nexthdr = (t_nextstep *)headerbuf;
    int byteswritten = 0, headersize = sizeof(t_nextstep);
    int swap = soundfile_info_swap(info);
    long datasize = nframes * soundfile_info_bytesperframe(info);

    strncpy(nexthdr->ns_fileid, (info->i_bigendian ? ".snd" : "dns."), 4);
    nexthdr->ns_onset = swap4(headersize, swap);
    nexthdr->ns_length = 0;
    switch(info->i_bytespersample)
    {
        case 2:
            nexthdr->ns_format = swap4(NS_FORMAT_LINEAR_16, swap);
            break;
        case 3:
            nexthdr->ns_format = swap4(NS_FORMAT_LINEAR_24, swap);
            break;
        case 4:
            nexthdr->ns_format = swap4(NS_FORMAT_FLOAT, swap);
            break;
        default: /* unsupported format */
            return 0;
    }
    nexthdr->ns_samplerate = swap4(info->i_samplerate, swap);
    nexthdr->ns_nchannels = swap4(info->i_nchannels, swap);
    strcpy(nexthdr->ns_info, "Pd ");
    swapstring(nexthdr->ns_info, swap);

    byteswritten = (int)write(fd, headerbuf, headersize);
    return (byteswritten < headersize ? -1 : byteswritten);
}

/* the data size is limited to 4 bytes, so if the size is too large,
do it the lazy way: just set the size field to "unknown size" */
int soundfile_next_updateheader(int fd, const t_soundfile_info *info,
    long nframes)
{
    uint32_t nextsize = 0xffffffff; /* unknown size */
    if (nframes * soundfile_info_bytesperframe(info) < nextsize)
        nextsize = nframes * soundfile_info_bytesperframe(info);
    
    if (lseek(fd, 8, SEEK_SET) == 0)
        return 0;
    if (write(fd, &nextsize, 4) < 4)
        return 0;
    return 1;
}

int soundfile_next_hasextension(const char *filename, long size)
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
