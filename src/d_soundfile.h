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

/* GLIBC large file support */
#ifdef _LARGEFILE64_SOURCE
#define lseek lseek64
#define off_t __off64_t
#endif

/* MSVC doesn't define or uses different naming for these Posix types */
#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#define off_t ssize_t
/* choose appropriate size if SSIZE_MAX is not defined */
#ifndef SSIZE_MAX
#ifdef _WIN64
#define SSIZE_MAX _I64_MAX
#else /* _WIN32 */
#define SSIZE_MAX INT_MAX
#endif
#endif /* SSIZE_MAX */
#endif /* _MSC_VER */

    /** should be large enough for all file type min sizes */
#define SFHDRBUFSIZE 128

#define SFMAXFRAMES SIZE_MAX  /**< default max sample frames, unsigned */
#define SFMAXBYTES  SSIZE_MAX /**< default max sample bytes, signed */

/* ----- soundfile format info ----- */

    /** soundfile format info
        note: headersize and bytelimit are signed as they are used for < 0
              comparisons, hopefully ssize_t is large enough */
typedef struct _soundfile_info
{
    int i_samplerate;     /**< read: file sr, write: pd sr               */
    int i_nchannels;      /**< number of channels                        */
    int i_bytespersample; /**< 2: 16 bit, 3: 24 bit, 4: 32 bit           */
    ssize_t i_headersize; /**< header size in bytes, -1 for unknown size */
    int i_bigendian;      /**< sample endianness 1 : big or 0 : little   */
    ssize_t i_bytelimit;  /**< max number of data bytes to read/write    */
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

/* ----- soundfile file type ----- */

    /** returns 1 if buffer is the beginning of a file type header */
typedef int (*t_sf_isheaderfn)(const char *buf, size_t size);

    /** read header from a file into info,
        returns 1 on success or 0 on error */
typedef int (*t_sf_readheaderfn)(int fd, t_soundfile_info *info);

    /** write header to beginning of an open file from an info struct
        returns header bytes written or -1 on error */
typedef int (*t_sf_writeheaderfn)(int fd, const t_soundfile_info *info,
    size_t nframes);

    /** update file header data size, returns 1 on success or 0 on error */
typedef int (*t_sf_updateheaderfn)(int fd, const t_soundfile_info *info,
    size_t nframes);

    /** returns 1 if the filename has an extension for the file type,
        otherwise 0 */
typedef int (*t_sf_hasextensionfn)(const char *filename, size_t size);

    /** appends the default file type extension, returns 1 on success */
typedef int (*t_sf_addextensionfn)(char *filename, size_t size);

    /** returns 1 if file type prefers big endian based on
        requested endianness (0 little, 1 big, -1 unspecified),
        otherwise 0 for little endian */
typedef int (*t_sf_endiannessfn)(int endianness);

    /* file type specific implementation */
typedef struct _soundfile_filetype
{
    t_symbol *ft_name;                     /**< file type name               */
    size_t ft_minheadersize;               /**< minimum valid header size    */
    t_sf_isheaderfn ft_isheaderfn;         /**< header check function        */
    t_sf_readheaderfn ft_readheaderfn;     /**< header read function         */
    t_sf_writeheaderfn ft_writeheaderfn;   /**< header write function        */
    t_sf_updateheaderfn ft_updateheaderfn; /**< header update function       */
    t_sf_hasextensionfn ft_hasextensionfn; /**< filename ext check function  */
    t_sf_addextensionfn ft_addextensionfn; /**< filename ext add function    */
    t_sf_endiannessfn ft_endiannessfn;     /**< file type endianness func    */
    void *ft_data;                         /**< implementation specific data */
} t_soundfile_filetype;

    /** add a new file type, makes a deep copy of ft
        returns 1 on success or 0 if max file types has been reached */
int soundfile_addfiletype(t_soundfile_filetype *ft);

/* ----- read write ----- */

    /** seek to offset in file fd and read size bytes into dst,
        returns bytes written on success or -1 on failure */
ssize_t soundfile_readbytes(int fd, off_t offset, char *dst, size_t size);

    /** seek to offset in file fd and write size bytes from dst,
        returns number of bytes written on success or -1 if seek or write
        failed */
ssize_t soundfile_writebytes(int fd, off_t offset, const char *src,
    size_t size);

/* ----- byte swappers ----- */

    /** returns 1 is system is bigendian */
int sys_isbigendian(void);

    /** swap 8 bytes and return if doit = 1, otherwise return n */
uint64_t swap8(uint64_t n, int doit);

    /** swap a 64 bit signed int and return if do it = 1, otherwise return n */
int64_t swap8s(int64_t n, int doit);

    /** swap 4 bytes and return if doit = 1, otherwise return n */
uint32_t swap4(uint32_t n, int doit);

    /** swap a 32 bit signed int and return if do it = 1, otherwise return n */
int32_t swap4s(int32_t n, int doit);

    /** swap 2 bytes and return if doit = 1, otherwise return n */
uint16_t swap2(uint16_t n, int doit);

    /** swap a 4 byte string in place if do it = 1, otherewise do nothing */
void swapstring4(char *foo, int doit);

    /** swap an 8 byte string in place if do it = 1, otherwise do nothing */
void swapstring8(char *foo, int doit);
