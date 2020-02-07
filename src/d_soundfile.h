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
#define off_t ssize_t
#endif

    /** should be large enough for all file type min sizes */
#define SFHDRBUFSIZE 128

    /** default max sample frames */
#define SFMAXFRAMES SIZE_MAX

    /** default max sample bytes */
#define SFMAXBYTES SIZE_MAX

/* ----- soundfile format info ----- */

    /** soundfile format info */
typedef struct _soundfile_info
{
    int i_samplerate;     /**< read: file sr, write: pd sr               */
    int i_nchannels;      /**< number of channels                        */
    int i_bytespersample; /**< 2: 16 bit, 3: 24 bit, 4: 32 bit           */
    ssize_t i_headersize; /**< header size in bytes, -1 for unknown size */
    int i_bigendian;      /**< sample endianness 1 : big or 0 : little   */
    size_t i_bytelimit;   /**< max number of data bytes to read/write    */
    int i_bytesperframe;  /**< number of bytes per sample frame          */
} t_soundfile_info;

    /** clear soundfile info struct to defaults */
void soundfile_info_clear(t_soundfile_info *info);

    /** copy src soundfile info into dst */
void soundfile_info_copy(t_soundfile_info *dst, const t_soundfile_info *src);

    /** print info struct */
void soundfile_info_print(const t_soundfile_info *info);

    /** returns 1 if bytes need to be swapped due to endianess, otherwise 0 */
int soundfile_info_swap(const t_soundfile_info *src);

/* ----- read write ----- */

    /** seek to offset in file fd and read size bytes into dst,
        returns bytes written on success or -1 on failure */
ssize_t soundfile_readbytes(int fd, off_t offset, char *dst, size_t size);

    /** seek to offset in file fd and write size bytes from dst,
        returns number of bytes written on success or -1 if seek or write
        failed */
ssize_t soundfile_writebytes(int fd, off_t offset, const char *src, size_t size);

/* ----- byte swappers ----- */

    /** returns 1 is system is bigendian
        FIXME: should this be renamed or moved? */
int sys_isbigendian(void);

    /** swap 4 bytes and return if doit = 1, otherwise return n */
uint32_t swap4(uint32_t n, int doit);

    /** swap a 32 bit signed int and return if do it = 1, otherwise return n */
int32_t swap4s(int32_t n, int doit);

    /** swap 2 bytes and return if doit = 1, otherwise return n */
uint16_t swap2(uint32_t n, int doit);

    /** swap a 4 byte string in place if do it = 1, otherewise do nothing */
void swapstring(char *foo, int doit);

/* ------------------------- WAVE ------------------------- */

    /** returns min WAVE header size in bytes */
int soundfile_wave_headersize();

    /** returns 1 if buffer is the beginning of a WAVE header */
int soundfile_wave_isheader(const char *buf, size_t size);

    /** read WAVE header from a file into info,
        returns 1 on success or 0 on error */
int soundfile_wave_readheader(int fd, t_soundfile_info *info);

    /** write header to beginning of an open file from an info struct
        returns header bytes written or -1 on error */
int soundfile_wave_writeheader(int fd, const t_soundfile_info *info,
    size_t nframes);

    /** update file header data size,
        returns 1 on success or 0 on error */
int soundfile_wave_updateheader(int fd, const t_soundfile_info *info,
    size_t nframes);

    /** returns 1 if the filename has a WAVE extension
        (.wav, .wave, .WAV, or .WAVE) otherwise 0 */
int soundfile_wave_hasextension(const char *filename, size_t size);

/* ------------------------- AIFF ------------------------- */

    /** returns min AIFF header size in bytes */
int soundfile_aiff_headersize();

    /** returns 1 if buffer is the beginning of an AIFF header */
int soundfile_aiff_isheader(const char *buf, size_t size);

    /** read AIFF header from a file into info,
        returns 1 on success or 0 on error */
int soundfile_aiff_readheader(int fd, t_soundfile_info *info);

    /** write header to beginning of an open file from an info struct
        returns header bytes written or -1 on error */
int soundfile_aiff_writeheader(int fd, const t_soundfile_info *info,
    size_t nframes);

    /** update file header data size,
        returns 1 on success or 0 on error */
int soundfile_aiff_updateheader(int fd, const t_soundfile_info *info,
    size_t nframes);

    /** returns 1 if the filename has an AIFF extension
        (.aif, .aiff, .aifc, .AIF, .AIFF, or .AIFC) otherwise 0 */
int soundfile_aiff_hasextension(const char *filename, size_t size);

/* ------------------------- NEXT ------------------------- */

    /** returns min NEXT header size in bytes */
int soundfile_next_headersize();

    /** returns 1 if buffer is the beginning of a NEXT header */
int soundfile_next_isheader(const char *buf, size_t size);

    /** read NEXT header from a file into info,
        returns 1 on success or 0 on error */
int soundfile_next_readheader(int fd, t_soundfile_info *info);

    /** write header to beginning of an open file from an info struct
        returns header bytes written or -1 on error */
int soundfile_next_writeheader(int fd, const t_soundfile_info *info,
    size_t nframes);

    /** update file header data size,
        returns 1 on success or 0 on error */
int soundfile_next_updateheader(int fd, const t_soundfile_info *info,
    size_t nframes);

    /** returns 1 if the filename has a NEXT extension
        (.au, .snd, .AU, or .SND) otherwise 0 */
int soundfile_next_hasextension(const char *filename, size_t size);
