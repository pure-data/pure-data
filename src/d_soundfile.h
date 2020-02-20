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

/* ----- soundfile ----- */

typedef struct _soundfile_filetype t_soundfile_filetype;

    /** soundfile file descriptor, backend type, and format info
        note: headersize and bytelimit are signed as they are used for < 0
              comparisons, hopefully ssize_t is large enough
        "headersize" can also be thought of as the audio data byte offset */
typedef struct _soundfile
{
    int sf_fd;             /**< file descriptor, >= 0 : open, -1 : closed */
    /* format info */
    int sf_samplerate;     /**< read: file sr, write: pd sr               */
    int sf_nchannels;      /**< number of channels                        */
    int sf_bytespersample; /**< bit rate, 2: 16 bit, 3: 24 bit, 4: 32 bit */
    ssize_t sf_headersize; /**< header size in bytes, -1 for unknown size */
    int sf_bigendian;      /**< sample endianness, 1 : big or 0 : little  */
    int sf_bytesperframe;  /**< number of bytes per sample frame          */
    ssize_t sf_bytelimit;  /**< number of sound data bytes to read/write  */
    /* implementation */
    t_soundfile_filetype *sf_filetype; /**< implementation type           */
    void *sf_data;         /**< implementation-specific data              */
} t_soundfile;

    /** clear soundfile struct to defaults, does not close or free */
void soundfile_clear(t_soundfile *sf);

    /** clear soundfile format info to defaults; leaves fd, filetype, & data */
void soundfile_clearinfo(t_soundfile *sf);

    /** copy src soundfile info into dst */
void soundfile_copy(t_soundfile *dst, const t_soundfile *src);

    /** print soundfile format info */
void soundfile_printinfo(const t_soundfile *sf);

    /** returns 1 if bytes need to be swapped due to endianess, otherwise 0 */
int soundfile_needsbyteswap(const t_soundfile *sf);

/* ----- soundfile file type ----- */

    /** returns 1 if buffer is the beginning of a file type header,
        size will be at least minheadersize
        this may be called in a background thread */
typedef int (*t_soundfile_isheaderfn)(const char *buf, size_t size);

    /** open a sound file with a file descriptor and allocate sf_data,
        returns 1 on success and 0 on failure
        note: fd is already valid and open when this function is called,
              so set sf->sf_fd here
        this may be called in a background thread */
typedef int (*t_soundfile_openfn)(t_soundfile *sf, int fd);

    /** close a soundfile and free sf_data,
        returns 1 on success and 0 on failure
        note: close sf_fd here, set sf_fd = -1 & sf_data = NULL
        this may be called in a background thread */
typedef int (*t_soundfile_closefn)(t_soundfile *sf);

    /** read format info from soundfile header,
        returns 1 on success or 0 on error
        note: set sf_bytelimit = sound data size
        this may be called in a background thread, but
        sf_metaout is only set when called on main thread */
typedef int (*t_soundfile_readheaderfn)(t_soundfile *sf);

    /** write header to beginning of an open file from an info struct
        returns header bytes written or -1 on error
        this may be called in a background thread */
typedef int (*t_soundfile_writeheaderfn)(t_soundfile *sf, size_t nframes);

    /** update file header data size, returns 1 on success or 0 on error
        this may be called in a background thread */
typedef int (*t_soundfile_updateheaderfn)(t_soundfile *sf, size_t nframes);

    /** returns 1 if the filename has a file type extension, otherwise 0
        this may be called in a background thread */
typedef int (*t_soundfile_hasextensionfn)(const char *filename, size_t size);

    /** appends the default file type extension, returns 1 on success
        this may be called in a background thread */
typedef int (*t_soundfile_addextensionfn)(char *filename, size_t size);

    /** returns 1 if file type prefers big endian samples based on
        requested endianness (0 little, 1 big, -1 unspecified),
        otherwise 0 for little endian */
typedef int (*t_soundfile_endiannessfn)(int endianness);

    /** seek to a specified sample frame in an open file,
        returns 1 on success or 0 on failure
        this may be called in a background thread */
typedef int (*t_soundfile_seektoframefn)(t_soundfile *sf, size_t frame);

    /** read samples from the soundfile into the dst buffer,
        dst is interleaved and is signed int (16 or 24 bit) or 32 bit float
        returns bytes read or < 0 on failure
        this may be called in a background thread */
typedef ssize_t (*t_soundfile_readsamplesfn)(t_soundfile *sf,
    unsigned char *dst, size_t size);

    /** write samples from the src buffer into the soundfile,
        src is interleaved and is signed int (16 or 24 bit) or 32 bit float
        returns bytes written or < 0 on failure
        this may be called in a background thread */
typedef ssize_t (*t_soundfile_writesamplesfn)(t_soundfile *sf,
    const unsigned char *src, size_t size);

    /** read meta data from the soundfile header to the given outlet
        returns 1 on success or 0 on failure */
typedef int (*t_soundfile_readmetafn)(t_soundfile *sf, t_outlet *out);

    /** write meta data to the soundfile header and updates headersize
        returns 1 on success or 0 on failure */
typedef int (*t_soundfile_writemetafn)(t_soundfile *sf, int argc, t_atom *argv);

    /* file type specific implementation */
typedef struct _soundfile_filetype
{
    t_symbol *ft_name;       /**< file type name               */
    size_t ft_minheadersize; /**< minimum valid header size    */
    t_soundfile_isheaderfn ft_isheaderfn;         /**< must be non-NULL */
    t_soundfile_openfn ft_openfn;                 /**< must be non-NULL */
    t_soundfile_closefn ft_closefn;               /**< must be non-NULL */
    t_soundfile_readheaderfn ft_readheaderfn;     /**< must be non-NULL */
    t_soundfile_writeheaderfn ft_writeheaderfn;   /**< must be non-NULL */
    t_soundfile_updateheaderfn ft_updateheaderfn; /**< must be non-NULL */
    t_soundfile_hasextensionfn ft_hasextensionfn; /**< must be non-NULL */
    t_soundfile_addextensionfn ft_addextensionfn; /**< must be non-NULL */
    t_soundfile_endiannessfn ft_endiannessfn;     /**< must be non-NULL */
    t_soundfile_seektoframefn ft_seektoframefn;   /**< must be non-NULL */
    t_soundfile_readsamplesfn ft_readsamplesfn;   /**< must be non-NULL */
    t_soundfile_writesamplesfn ft_writesamplesfn; /**< must be non-NULL */
    t_soundfile_readmetafn ft_readmetafn;         /**< NULL if not supported */
    t_soundfile_writemetafn ft_writemetafn;       /**< NULL if not supported */
    void *ft_data; /**< implementation specific data */
} t_soundfile_filetype;

    /** add a new file type, makes a deep copy
        returns 1 on success or 0 if max file types has been reached */
int soundfile_addfiletype(t_soundfile_filetype *ft);

/* ----- default implementations ----- */

    /** default t_soundfile_openfn implementation */
int soundfile_filetype_open(t_soundfile *sf, int fd);

    /** default t_soundfile_closefn implementation */
int soundfile_filetype_close(t_soundfile *sf);

    /** default t_soundfile_seektoframefn implementation */
int soundfile_filetype_seektoframe(t_soundfile *sf, size_t frame);

    /** default t_soundfile_readsamplesfn implementation */
ssize_t soundfile_filetype_readsamples(t_soundfile *sf,
    unsigned char *buf, size_t size);

    /** default t_soundfile_writesamplesfn implementation */
ssize_t soundfile_filetype_writesamples(t_soundfile *sf,
    const unsigned char *buf, size_t size);

/* ----- read/write helpers ----- */

    /** seek to offset in file fd and read size bytes into dst,
        returns bytes written on success or -1 on failure */
ssize_t soundfile_readbytes(int fd, off_t offset,
    char *dst, size_t size);

    /** seek to offset in file fd and write size bytes from dst,
        returns number of bytes written on success or -1 if seek or write
        failed */
ssize_t soundfile_writebytes(int fd, off_t offset,
    const char *src, size_t size);

    /** move size bytes from src location to dst location, assumes fd is O_RDWR
        returns bytes moved on success or -1 on failure */
ssize_t soundfile_movebytes(int fd, off_t dst, off_t src, size_t size);

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
