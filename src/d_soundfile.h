/* Copyright (c) 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* soundfile formats and helper functions */

#pragma once

#include "m_pd.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif
#include <string.h>
#include <limits.h>
#include <errno.h>

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

    /** sound file read/write debug posts */
//#define DEBUG_SOUNDFILE

/* ----- soundfile ----- */

    /** soundfile file descriptor, backend type, and format info
        note: headersize and bytelimit are signed as they are used for < 0
              comparisons, hopefully ssize_t is large enough
        "headersize" can also be thought of as the audio data byte offset */
typedef struct _soundfile
{
    int sf_fd;             /**< file descriptor, >= 0 : open, -1 : closed */
    struct _soundfile_type *sf_type; /**< type implementation             */
    /* format info */
    int sf_samplerate;     /**< read: file sr, write: pd sr               */
    int sf_nchannels;      /**< number of channels                        */
    int sf_bytespersample; /**< bit rate, 2: 16 bit, 3: 24 bit, 4: 32 bit */
    ssize_t sf_headersize; /**< header size in bytes, -1 for unknown size */
    int sf_bigendian;      /**< sample endianness, 1 : big or 0 : little  */
    int sf_bytesperframe;  /**< number of bytes per sample frame          */
    ssize_t sf_bytelimit;  /**< number of sound data bytes to read/write  */
} t_soundfile;

    /** clear soundfile struct to defaults, does not close or free */
void soundfile_clear(t_soundfile *sf);

    /** copy src soundfile info into dst */
void soundfile_copy(t_soundfile *dst, const t_soundfile *src);

    /** returns 1 if bytes need to be swapped due to endianness, otherwise 0 */
int soundfile_needsbyteswap(const t_soundfile *sf);

    /** generic soundfile errors */
typedef enum _soundfile_errno
{
    SOUNDFILE_ERRUNKNOWN   = -1000, /* unknown header */
    SOUNDFILE_ERRMALFORMED = -1001, /* bad header */
    SOUNDFILE_ERRVERSION   = -1002, /* header ok, unsupported version */
    SOUNDFILE_ERRSAMPLEFMT = -1003  /* header ok, unsupported sample format */
} t_soundfile_errno;

    /** returns a soundfile error string, otherwise calls C strerror */
const char* soundfile_strerror(int errnum);

/* ----- soundfile type ----- */

    /** returns 1 if buffer is the beginning of a supported file header,
        size will be at least minheadersize
        this may be called in a background thread */
typedef int (*t_soundfile_isheaderfn)(const char *buf, size_t size);

    /** read format info from soundfile header,
        returns 1 on success or 0 on error
        note: set sf_bytelimit = sound data size, optionally set errno
        this may be called in a background thread */
typedef int (*t_soundfile_readheaderfn)(t_soundfile *sf);

    /** write header to beginning of an open file from an info struct
        returns header bytes written or < 0 on error
        note: optionally set errno
        this may be called in a background thread */
typedef int (*t_soundfile_writeheaderfn)(t_soundfile *sf, size_t nframes);

    /** update file header data size, returns 1 on success or 0 on error
        this may be called in a background thread */
typedef int (*t_soundfile_updateheaderfn)(t_soundfile *sf, size_t nframes);

    /** returns 1 if the filename has a supported file extension, otherwise 0
        this may be called in a background thread */
typedef int (*t_soundfile_hasextensionfn)(const char *filename, size_t size);

    /** appends the default file extension, returns 1 on success
        this may be called in a background thread */
typedef int (*t_soundfile_addextensionfn)(char *filename, size_t size);

    /** returns the type's preferred sample endianness based on the
        requested endianness (0 little, 1 big, -1 unspecified)
        returns 1 for big endian, 0 for little endian */
typedef int (*t_soundfile_endiannessfn)(int endianness);

    /* type implementation for a single file format */
typedef struct _soundfile_type
{
    char *t_name;           /**< type name, unique & w/o white spaces       */
    size_t t_minheadersize; /**< minimum valid header size                  */
    t_soundfile_isheaderfn t_isheaderfn;         /**< must be non-NULL      */
    t_soundfile_readheaderfn t_readheaderfn;     /**< must be non-NULL      */
    t_soundfile_writeheaderfn t_writeheaderfn;   /**< must be non-NULL      */
    t_soundfile_updateheaderfn t_updateheaderfn; /**< must be non-NULL      */
    t_soundfile_hasextensionfn t_hasextensionfn; /**< must be non-NULL      */
    t_soundfile_addextensionfn t_addextensionfn; /**< must be non-NULL      */
    t_soundfile_endiannessfn t_endiannessfn;     /**< must be non-NULL      */
} t_soundfile_type;

    /** add a new type implementation
        returns 1 on success or 0 if max types has been reached */
int soundfile_addtype(const t_soundfile_type *t);

/* ----- read/write helpers ----- */

    /** seek to offset in file fd and read size bytes into dst,
        returns bytes written on success or -1 on failure */
ssize_t fd_read(int fd, off_t offset, void *dst, size_t size);

    /** seek to offset in file fd and write size bytes from dst,
        returns number of bytes written on success or -1 if seek or write
        failed */
ssize_t fd_write(int fd, off_t offset, const void *src, size_t size);

/* ----- byte swappers ----- */

    /** returns 1 if system is bigendian */
int sys_isbigendian(void);

    /** swap 8 bytes and return if doit = 1, otherwise return n */
uint64_t swap8(uint64_t n, int doit);

    /** swap a 64 bit signed int and return if doit = 1, otherwise return n */
int64_t swap8s(int64_t n, int doit);

    /** swap 4 bytes and return if doit = 1, otherwise return n */
uint32_t swap4(uint32_t n, int doit);

    /** swap a 32 bit signed int and return if doit = 1, otherwise return n */
int32_t swap4s(int32_t n, int doit);

    /** swap 2 bytes and return if doit = 1, otherwise return n */
uint16_t swap2(uint16_t n, int doit);

    /** swap a 4 byte string in place if doit = 1, otherwise do nothing */
void swapstring4(char *foo, int doit);

    /** swap an 8 byte string in place if doit = 1, otherwise do nothing */
void swapstring8(char *foo, int doit);
