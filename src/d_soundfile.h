/* Copyright (c) 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* soundfile formats and helper functions */

#pragma once

#include "m_pd.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <limits.h>

/* GLIBC */
#ifdef _LARGEFILE64_SOURCE
#define lseek lseek64
#define off_t __off64_t
#endif

#ifdef _MSC_VER
#define off_t long
#endif

// should be large enough for all file type min sizes
#define SFHDRBUFSIZE 128

// default max sample frames
#define SFMAXFRAMES LONG_MAX

// default max sample bytes
#define SFMAXBYTES LONG_MAX

/* soundfile format info */

/// soundfile format info
typedef struct _soundfile_info
{
    int i_samplerate;     ///< read: file sr, write: pd sr
    int i_nchannels;      ///< number of channels
    int i_bytespersample; ///< 2: 16 bit, 3: 24 bit, 4: 32 bit
    int i_headersize;     ///< header size in bytes
    int i_bigendian;      ///< 1: big : 1, 0: little
    long i_bytelimit;     ///< max number of data bytes to read/write
} t_soundfile_info;

/// clear soundfile info struct to defaults
void soundfile_info_clear(t_soundfile_info *info);

/// copy src soundfile info into dst
void soundfile_info_copy(t_soundfile_info *dst, const t_soundfile_info *src);

/// print info struct
void soundfile_info_print(const t_soundfile_info *info);

/// compute number of bytes per frame from src info struct
int soundfile_info_bytesperframe(const t_soundfile_info *src);

/// returns 1 if bytes need to be swapped due to endianess, otherwise 0
int soundfile_info_swap(const t_soundfile_info *src);

/* byte swappers */

/// returns 1 is system is bigendian
/// FIXME: should this be renamed or moved?
int sys_isbigendian(void);

/// swap 4 bytes and return if doit = 1, otherwise return n
uint32_t swap4(uint32_t n, int doit);

/// swap 2 bytes and return if doit = 1, otherwise return n
uint16_t swap2(uint32_t n, int doit);

/// swap a 4 byte string in place if do it = 1, otherewise do nothing
void swapstring(char *foo, int doit);

/* WAVE */

/// returns min WAVE header size in bytes
int soundfile_wave_headersize();

/// returns 1 if buffer is the beginning of a WAVE header
int soundfile_wave_isheader(const char *buf, long size);

/// read WAVE header from a file into info, assumes fd is at the beginning
/// returns 1 on success or 0 on error
int soundfile_wave_readheader(int fd, t_soundfile_info *info);

/// write header to beginning of an open file from an info struct
/// returns header bytes written or -1 on error
int soundfile_wave_writeheader(int fd, const t_soundfile_info *info,
    long nframes);

/// update file header data size, assumes fd is at the beginning
/// returns 1 on success or 0 on error
int soundfile_wave_updateheader(int fd, const t_soundfile_info *info,
    long nframes);

/// returns 1 if the filename has a WAVE extension (.wav, .wave, .WAV, or .WAVE)
/// otherwise 0
int soundfile_wave_hasextension(const char *filename, long size);

/* AIFF */

/// returns min AIFF header size in bytes
int soundfile_aiff_headersize();

/// returns 1 if buffer is the beginning of an AIFF header
int soundfile_aiff_isheader(const char *buf, long size);

/// read AIFF header from a file into info, assumes fd is at the beginning
/// returns 1 on success or 0 on error
int soundfile_aiff_readheader(int fd, t_soundfile_info *info);

/// write header to beginning of an open file from an info struct
/// returns header bytes written or -1 on error
int soundfile_aiff_writeheader(int fd, const t_soundfile_info *info,
    long nframes);

/// update file header data size, assumes fd is at the beginning
/// returns 1 on success or 0 on error
int soundfile_aiff_updateheader(int fd, const t_soundfile_info *info,
    long nframes);

/// returns 1 if the filename has an AIFF extension (.aif, .aiff, .AIF, or
/// .AIFF) otherwise 0
int soundfile_aiff_hasextension(const char *filename, long size);

/* NEXT */

/// returns min NEXT header size in bytes
int soundfile_next_headersize();

/// returns 1 if buffer is the beginning of a NEXT header
int soundfile_next_isheader(const char *buf, long size);

/// read NEXT header from a file into info, assumes fd is at the beginning
/// returns 1 on success or 0 on error
int soundfile_next_readheader(int fd, t_soundfile_info *info);

/// write header to beginning of an open file from an info struct
/// returns header bytes written or -1 on error
int soundfile_next_writeheader(int fd, const t_soundfile_info *info,
    long nframes);

/// update file header data size, assumes fd is at the beginning
/// returns 1 on success or 0 on error
int soundfile_next_updateheader(int fd, const t_soundfile_info *info,
    long nframes);

/// returns 1 if the filename has a NEXT extension (.au, .snd, .AU, or .SND)
/// otherwise 0
int soundfile_next_hasextension(const char *filename, long size);
