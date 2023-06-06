/* Copyright (c) 1997-1999 Miller Puckette. Updated 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* this file contains, first, a collection of soundfile access routines, a
sort of soundfile library.  Second, the "soundfiler" object is defined which
uses the routines to read or write soundfiles, synchronously, from garrays.
These operations are not to be done in "real time" as they may have to wait
for disk accesses (even the write routine.)  Finally, the realtime objects
readsf~ and writesf~ are defined which confine disk operations to a separate
thread so that they can be used in real time.  The readsf~ and writesf~
objects use Posix-like threads. */

#include "d_soundfile.h"
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>

/* Supported sample formats: LPCM (16 or 24 bit int) & 32 bit float */

#define MAXSFCHANS 64

/* GLIBC large file support */
#ifdef _LARGEFILE64_SOURCE
#define open open64
#endif

/* MSVC uses different naming for these */
#ifdef _MSC_VER
#define O_CREAT  _O_CREAT
#define O_TRUNC  _O_TRUNC
#define O_WRONLY _O_WRONLY
#endif

#define SCALE (1. / (1024. * 1024. * 1024. * 2.))

    /* float sample conversion wrapper */
typedef union _floatuint
{
  float f;
  uint32_t ui;
} t_floatuint;

/* ----- soundfile ----- */

void soundfile_clear(t_soundfile *sf)
{
    memset(sf, 0, sizeof(t_soundfile));
    sf->sf_fd = -1;
    sf->sf_type = NULL;
    sf->sf_bytelimit = SFMAXBYTES;
}

void soundfile_copy(t_soundfile *dst, const t_soundfile *src)
{
    memcpy((char *)dst, (char *)src, sizeof(t_soundfile));
}

int soundfile_needsbyteswap(const t_soundfile *sf)
{
    return sf->sf_bigendian != sys_isbigendian();
}

const char* soundfile_strerror(int errnum)
{
    switch (errnum)
    {
        case SOUNDFILE_ERRUNKNOWN:
            return "unknown header format";
        case SOUNDFILE_ERRMALFORMED:
            return "bad header format";
        case SOUNDFILE_ERRVERSION:
            return "unsupported header format version";
        case SOUNDFILE_ERRSAMPLEFMT:
            return "unsupported sample format";
        default: /* C/POSIX error */
            return strerror(errnum);
    }
}

    /** output soundfile format info as a list */
static void outlet_soundfileinfo(t_outlet *out, t_soundfile *sf)
{
    t_atom info_list[5];
    SETFLOAT((t_atom *)info_list, (t_float)sf->sf_samplerate);
    SETFLOAT((t_atom *)info_list+1,
        (t_float)(sf->sf_headersize < 0 ? 0 : sf->sf_headersize));
    SETFLOAT((t_atom *)info_list+2, (t_float)sf->sf_nchannels);
    SETFLOAT((t_atom *)info_list+3, (t_float)sf->sf_bytespersample);
    SETSYMBOL((t_atom *)info_list+4, gensym((sf->sf_bigendian ? "b" : "l")));
    outlet_list(out, &s_list, 5, (t_atom *)info_list);
}

    /* post soundfile error, try to print type name */
static void object_sferror(const void *x, const char *header,
    const char *filename, int errnum, const t_soundfile *sf)
{
    if (sf && sf->sf_type)
        pd_error(x, "%s %s: %s: %s", header, sf->sf_type->t_name, filename,
            soundfile_strerror(errnum));
    else
        pd_error(x, "%s: %s: %s", header, filename, soundfile_strerror(errnum));
}

/* ----- soundfile type ----- */

#define SFMAXTYPES 4

/* should these globals be PERTHREAD? */

    /** supported type implementations */
static t_soundfile_type *sf_types[SFMAXTYPES] = {0};

    /** number of types */
static size_t sf_numtypes = 0;

    /** min required header size, largest among the current types */
static size_t sf_minheadersize = 0;

    /** printable type argument list,
       dash prepended and separated by spaces */
static char sf_typeargs[MAXPDSTRING] = {0};

    /* built-in type implementations */
void soundfile_wave_setup(void);
void soundfile_aiff_setup(void);
void soundfile_caf_setup(void);
void soundfile_next_setup(void);

    /** set up built-in types */
void soundfile_type_setup(void)
{
    soundfile_wave_setup(); /* default first */
    soundfile_aiff_setup();
    soundfile_caf_setup();
    soundfile_next_setup();
}

int soundfile_addtype(const t_soundfile_type *type)
{
    int i;
    if (sf_numtypes == SFMAXTYPES)
    {
        pd_error(0, "soundfile: max number of type implementations reached");
        return 0;
    }
    sf_types[sf_numtypes] = (t_soundfile_type *)type;
    sf_numtypes++;
    if (type->t_minheadersize > sf_minheadersize)
        sf_minheadersize = type->t_minheadersize;
    strcat(sf_typeargs, (sf_numtypes > 1 ? " -" : "-"));
    strcat(sf_typeargs, type->t_name);
    return 1;
}

    /** return type list head pointer */
static t_soundfile_type **soundfile_firsttype(void)
{
    return &sf_types[0];
}

    /** return next type list pointer or NULL if at the end */
static t_soundfile_type **soundfile_nexttype(t_soundfile_type **t)
{
    return (t == &sf_types[sf_numtypes-1] ? NULL : ++t);
}

    /** find type by name, returns NULL if not found */
static t_soundfile_type *soundfile_findtype(const char *name)
{
    t_soundfile_type **t = soundfile_firsttype();
    while (t)
    {
        if (!strcmp(name, (*t)->t_name))
            break;
        t = soundfile_nexttype(t);
    }
    return (t ? *t : NULL);
}

/* ----- ASCII ----- */

    /** compound ascii read/write args  */
typedef struct _asciiargs
{
    ssize_t aa_onsetframe;  /* start frame */
    ssize_t aa_nframes;     /* nframes to read/write */
    int aa_nchannels;       /* number of channels to read/write */
    t_word **aa_vectors;    /* vectors to read into/write out of */
    t_garray **aa_garrays;  /* read: arrays to resize & read into */
    int aa_resize;          /* read: resize when reading? */
    size_t aa_maxsize;      /* read: max size to read/resize to */
    t_sample aa_normfactor; /* write: normalization factor */
} t_asciiargs;

static int ascii_hasextension(const char *filename, size_t size)
{
    int len = strnlen(filename, size);
    if (len >= 5 && !strncmp(filename + (len - 4), ".txt", 4))
        return 1;
    return 0;
}

static int ascii_addextension(char *filename, size_t size)
{
    size_t len = strnlen(filename, size);
    if (len + 4 >= size)
        return 0;
    strcpy(filename + len, ".txt");
    return 1;
}

/* ----- read write ----- */

ssize_t fd_read(int fd, off_t offset, void *dst, size_t size)
{
    if (lseek(fd, offset, SEEK_SET) != offset)
        return -1;
    return read(fd, dst, size);
}

ssize_t fd_write(int fd, off_t offset, const void *src, size_t size)
{
    if (lseek(fd, offset, SEEK_SET) != offset)
        return -1;
    return write(fd, src, size);
}

/* ----- byte swappers ----- */

int sys_isbigendian(void)
{
    unsigned short s = 1;
    unsigned char c = *(char *)(&s);
    return (c == 0);
}

uint64_t swap8(uint64_t n, int doit)
{
    if (doit)
        return (((n >> 56) & 0x00000000000000ffULL) |
                ((n >> 40) & 0x000000000000ff00ULL) |
                ((n >> 24) & 0x0000000000ff0000ULL) |
                ((n >>  8) & 0x00000000ff000000ULL) |
                ((n <<  8) & 0x000000ff00000000ULL) |
                ((n << 24) & 0x0000ff0000000000ULL) |
                ((n << 40) & 0x00ff000000000000ULL) |
                ((n << 56) & 0xff00000000000000ULL));
    return n;
}

int64_t swap8s(int64_t n, int doit)
{
    if (doit)
    {
        n = ((n <<  8) & 0xff00ff00ff00ff00ULL) |
            ((n >>  8) & 0x00ff00ff00ff00ffULL);
        n = ((n << 16) & 0xffff0000ffff0000ULL) |
            ((n >> 16) & 0x0000ffff0000ffffULL );
        return (n << 32) | ((n >> 32) & 0xffffffffULL);
    }
    return n;
}

uint32_t swap4(uint32_t n, int doit)
{
    if (doit)
        return (((n & 0x0000ff) << 24) | ((n & 0x0000ff00) <<  8) |
                ((n & 0xff0000) >>  8) | ((n & 0xff000000) >> 24));
    return n;
}

int32_t swap4s(int32_t n, int doit)
{
    if (doit)
    {
        n = ((n << 8) & 0xff00ff00) | ((n >> 8) & 0xff00ff);
        return (n << 16) | ((n >> 16) & 0xffff);
    }
    return n;
}

uint16_t swap2(uint16_t n, int doit)
{
    if (doit)
        return (((n & 0x00ff) << 8) | ((n & 0xff00) >> 8));
    return n;
}

void swapstring4(char *foo, int doit)
{
    if (doit)
    {
        char a = foo[0], b = foo[1], c = foo[2], d = foo[3];
        foo[0] = d; foo[1] = c; foo[2] = b; foo[3] = a;
    }
}

void swapstring8(char *foo, int doit)
{
    if (doit)
    {
        char a = foo[0], b = foo[1], c = foo[2], d = foo[3],
        e = foo[4], f = foo[5], g = foo[6], h = foo[7];
        foo[0] = h; foo[1] = g; foo[2] = f; foo[3] = e;
        foo[4] = d; foo[5] = c; foo[6] = b; foo[7] = a;
    }
}

/* ----------------------- soundfile access routines ----------------------- */

    /** This routine opens a file, looks for a supported file format
        header, seeks to end of it, and fills in the soundfile header info
        values. Only 2- and 3-byte fixed-point samples and 4-byte floating point
        samples are supported.  If sf->sf_headersize is nonzero, the caller
        should supply the number of channels, endinanness, and bytes per sample;
        the header is ignored.  If sf->sf_type is non-NULL, the given type
        implementation is used. Otherwise, the routine tries to read the header
        and fill in the properties. Fills sf struct on success, closes fd on
        failure. */
int open_soundfile_via_fd(int fd, t_soundfile *sf, size_t skipframes)
{
    off_t offset;
    errno = 0;
    if (sf->sf_headersize >= 0) /* header detection overridden */
    {
            /* interpret data size from file size */
        ssize_t bytelimit = lseek(fd, 0, SEEK_END);
        if (bytelimit < 0)
            goto badheader;
        if (bytelimit > SFMAXBYTES || bytelimit < 0)
            bytelimit = SFMAXBYTES;
        sf->sf_bytelimit = bytelimit;
        sf->sf_fd = fd;
    }
    else
    {
        char buf[SFHDRBUFSIZE];
        ssize_t bytesread = read(fd, buf, sf_minheadersize);

        if (!sf->sf_type)
        {
                /* check header for type */
            t_soundfile_type **t = soundfile_firsttype();
            while (t)
            {
                if ((*t)->t_isheaderfn(buf, bytesread))
                    break;
                t = soundfile_nexttype(t);
            }
            if (!t) /* not recognized */
            {
                errno = SOUNDFILE_ERRUNKNOWN;
                goto badheader;
            }
            sf->sf_type = *t;
        }
        else
        {
                /* check header using given type */
            if (!sf->sf_type->t_isheaderfn(buf, bytesread))
            {
                errno = SOUNDFILE_ERRUNKNOWN;
                goto badheader;
            }
        }
        sf->sf_fd = fd;

            /* rewind and read header */
        if (lseek(sf->sf_fd, 0, SEEK_SET) < 0)
            goto badheader;
        if (!sf->sf_type->t_readheaderfn(sf))
            goto badheader;
    }

        /* seek past header and any sample frames to skip */
    offset = sf->sf_headersize + (skipframes * sf->sf_bytesperframe);
    if (lseek(sf->sf_fd, offset, 0) < offset)
        goto badheader;
    sf->sf_bytelimit -= skipframes * sf->sf_bytesperframe;
    if (sf->sf_bytelimit < 0)
        sf->sf_bytelimit = 0;

        /* copy sample format back to caller */
    return fd;

badheader:
        /* the header wasn't recognized.  We're threadable here so let's not
        print out the error... */
    if (!errno)
        errno = SOUNDFILE_ERRMALFORMED;
    sf->sf_fd = -1;
    if (fd >= 0)
        sys_close(fd);
    return -1;
}

    /** open a soundfile, using open_via_path().  This is used by readsf~ in
        a not-perfectly-threadsafe way.  LATER replace with a thread-hardened
        version of open_soundfile_via_canvas().
        returns number of frames in the soundfile */
int open_soundfile_via_path(const char *dirname, const char *filename,
    t_soundfile *sf, size_t skipframes)
{
    char buf[MAXPDSTRING], *dummy;
    int fd, sf_fd;
    fd = open_via_path(dirname, filename, "", buf, &dummy, MAXPDSTRING, 1);
    if (fd < 0)
        return -1;
    sf_fd = open_soundfile_via_fd(fd, sf, skipframes);
    return sf_fd;
}

    /** open a soundfile, using open_via_canvas().  This is used by readsf~ in
        a not-perfectly-threadsafe way.  LATER replace with a thread-hardened
        version of open_soundfile_via_canvas().
        returns number of frames in the soundfile */
int open_soundfile_via_canvas(t_canvas *canvas, const char *filename,
    t_soundfile *sf, size_t skipframes)
{
    char buf[MAXPDSTRING], *dummy;
    int fd, sf_fd;
    fd = canvas_open(canvas, filename, "", buf, &dummy, MAXPDSTRING, 1);
    if (fd < 0)
        return -1;
    sf_fd = open_soundfile_via_fd(fd, sf, skipframes);
    return sf_fd;
}

static void soundfile_xferin_sample(const t_soundfile *sf, int nvecs,
    t_sample **vecs, size_t framesread, unsigned char *buf, size_t nframes)
{
    int nchannels = (sf->sf_nchannels < nvecs ? sf->sf_nchannels : nvecs), i;
    size_t j;
    unsigned char *sp, *sp2;
    t_sample *fp;
    for (i = 0, sp = buf; i < nchannels; i++, sp += sf->sf_bytespersample)
    {
        if (sf->sf_bytespersample == 2)
        {
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                        *fp = SCALE * ((sp2[0] << 24) | (sp2[1] << 16));
            }
            else
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                        *fp = SCALE * ((sp2[1] << 24) | (sp2[0] << 16));
            }
        }
        else if (sf->sf_bytespersample == 3)
        {
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                        *fp = SCALE * ((sp2[0] << 24) | (sp2[1] << 16) |
                                       (sp2[2] << 8));
            }
            else
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                        *fp = SCALE * ((sp2[2] << 24) | (sp2[1] << 16) |
                                       (sp2[0] << 8));
            }
        }
        else if (sf->sf_bytespersample == 4)
        {
            t_floatuint alias;
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                {
                    alias.ui = ((sp2[0] << 24) | (sp2[1] << 16) |
                                (sp2[2] << 8)  |  sp2[3]);
                    *fp = (t_sample)alias.f;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                {
                    alias.ui = ((sp2[3] << 24) | (sp2[2] << 16) |
                                (sp2[1] << 8)  |  sp2[0]);
                    *fp = (t_sample)alias.f;
                }
            }
        }
    }
        /* zero out other outputs */
    for (i = sf->sf_nchannels; i < nvecs; i++)
        for (j = nframes, fp = vecs[i]; j--;)
            *fp++ = 0;
}

static void soundfile_xferin_words(const t_soundfile *sf, int nvecs,
    t_word **vecs, size_t framesread, unsigned char *buf, size_t nframes)
{
    unsigned char *sp, *sp2;
    t_word *wp;
    int nchannels = (sf->sf_nchannels < nvecs ? sf->sf_nchannels : nvecs), i;
    size_t j;
    for (i = 0, sp = buf; i < nchannels; i++, sp += sf->sf_bytespersample)
    {
        if (sf->sf_bytespersample == 2)
        {
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                        wp->w_float = SCALE * ((sp2[0] << 24) | (sp2[1] << 16));
            }
            else
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                        wp->w_float = SCALE * ((sp2[1] << 24) | (sp2[0] << 16));
            }
        }
        else if (sf->sf_bytespersample == 3)
        {
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                        wp->w_float = SCALE * ((sp2[0] << 24) | (sp2[1] << 16) |
                                               (sp2[2] << 8));
            }
            else
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                        wp->w_float = SCALE * ((sp2[2] << 24) | (sp2[1] << 16) |
                                               (sp2[0] << 8));
            }
        }
        else if (sf->sf_bytespersample == 4)
        {
            t_floatuint alias;
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                {
                    alias.ui = ((sp2[0] << 24) | (sp2[1] << 16) |
                                (sp2[2] << 8)  |  sp2[3]);
                    wp->w_float = (t_float)alias.f;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                {
                    alias.ui = ((sp2[3] << 24) | (sp2[2] << 16) |
                                (sp2[1] << 8)  |  sp2[0]);
                    wp->w_float = (t_float)alias.f;
                }
            }
        }
    }
        /* zero out other outputs */
    for (i = sf->sf_nchannels; i < nvecs; i++)
        for (j = nframes, wp = vecs[i]; j--;)
            (wp++)->w_float = 0;
}

    /* soundfiler_write ...

       usage: write [flags] filename table ...
       flags:
         -nframes <frames>
         -skip <frames>
         -bytes <bytespersample>
         -rate / -r <samplerate>
         -normalize
         -wave
         -aiff
         -caf
         -next / -nextstep
         -ascii
         -big
         -little
    */

    /** parsed write arguments */
typedef struct _soundfiler_writeargs
{
    t_symbol *wa_filesym;             /* file path symbol */
    t_soundfile_type *wa_type;        /* type implementation */
    int wa_samplerate;                /* sample rate */
    int wa_bytespersample;            /* number of bytes per sample */
    int wa_bigendian;                 /* is sample data bigendian? */
    size_t wa_nframes;                /* number of sample frames to write */
    size_t wa_onsetframes;            /* sample frame onset when writing */
    int wa_normalize;                 /* normalize samples? */
    int wa_ascii;                     /* write ascii? */
} t_soundfiler_writeargs;


/* the routine which actually does the work should LATER also be called
from garray_write16. */

    /** Parse arguments for writing.  The "obj" argument is only for flagging
        errors.  For streaming to a file the "normalize" and "nframes"
        arguments shouldn't be set but the calling routine flags this. */
static int soundfiler_parsewriteargs(void *obj, int *p_argc, t_atom **p_argv,
    t_soundfiler_writeargs *wa)
{
    int argc = *p_argc;
    t_atom *argv = *p_argv;
    int samplerate = -1, bytespersample = 2, bigendian = 0, endianness = -1;
    size_t nframes = SFMAXFRAMES, onsetframes = 0;
    int normalize = 0, ascii = 0;
    t_symbol *filesym;
    t_soundfile_type *type = NULL;

    while (argc > 0 && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        const char *flag = argv->a_w.w_symbol->s_name + 1;
        if (!strcmp(flag, "skip"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                (argv[1].a_w.w_float) < 0)
                    return -1;
            onsetframes = argv[1].a_w.w_float;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(flag, "nframes"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                argv[1].a_w.w_float < 0)
                    return -1;
            nframes = argv[1].a_w.w_float;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(flag, "bytes"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                ((bytespersample = argv[1].a_w.w_float) < 2) ||
                    bytespersample > 4)
                        return -1;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(flag, "normalize"))
        {
            normalize = 1;
            argc -= 1; argv += 1;
        }
        else if (!strcmp(flag, "big"))
        {
            endianness = 1;
            argc -= 1; argv += 1;
        }
        else if (!strcmp(flag, "little"))
        {
            endianness = 0;
            argc -= 1; argv += 1;
        }
        else if (!strcmp(flag, "rate") || !strcmp(flag, "r"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                ((samplerate = argv[1].a_w.w_float) <= 0))
                    return -1;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(flag, "ascii"))
        {
            ascii = 1;
            argc -= 1; argv += 1;
        }
        else if (!strcmp(flag, "nextstep"))
        {
                /* handle old "-nextstep" alias */
            type = soundfile_findtype("next");
            argc -= 1; argv += 1;
        }
        else
        {
                /* check for type by name */
            if (!(type = soundfile_findtype(flag)))
                return -1; /* unknown flag */
            ascii = 0; /* replaced */
            argc -= 1; argv += 1;
        }
    }
    if (!argc || argv->a_type != A_SYMBOL)
        return -1;
    filesym = argv->a_w.w_symbol;

        /* deduce from filename extension? */
    if (!type)
    {
        t_soundfile_type **t = soundfile_firsttype();
        while (t)
        {
            if ((*t)->t_hasextensionfn(filesym->s_name, MAXPDSTRING))
                break;
            t = soundfile_nexttype(t);
        }
        if (!t)
        {
            if (!ascii)
                ascii = ascii_hasextension(filesym->s_name, MAXPDSTRING);
            t = soundfile_firsttype(); /* default if unknown */
        }
        type = *t;
    }

        /* check requested endianness */
    bigendian = type->t_endiannessfn(endianness);
    if (endianness != -1 && endianness != bigendian)
    {
        post("%s: forced to %s endian", type->t_name,
            (bigendian ? "big" : "little"));
    }

        /* return to caller */
    argc--; argv++;
    *p_argc = argc;
    *p_argv = argv;
    wa->wa_filesym = filesym;
    wa->wa_type = type;
    wa->wa_samplerate = samplerate;
    wa->wa_bytespersample = bytespersample;
    wa->wa_bigendian = bigendian;
    wa->wa_nframes = nframes;
    wa->wa_onsetframes = onsetframes;
    wa->wa_normalize = normalize;
    wa->wa_ascii = ascii;
    return 0;
}

    /** sets sf fd & headerisze on success and returns fd or -1 on failure */
static int create_soundfile(t_canvas *canvas, const char *filename,
    t_soundfile *sf, size_t nframes)
{
    char filenamebuf[MAXPDSTRING], pathbuf[MAXPDSTRING];
    ssize_t headersize = -1;
    int fd;

        /* create file */
    strncpy(filenamebuf, filename, MAXPDSTRING);
    if (!sf->sf_type->t_hasextensionfn(filenamebuf, MAXPDSTRING-10))
        if (!sf->sf_type->t_addextensionfn(filenamebuf, MAXPDSTRING-10))
            return -1;
    filenamebuf[MAXPDSTRING-10] = 0; /* FIXME: what is the 10 for? */
    canvas_makefilename(canvas, filenamebuf, pathbuf, MAXPDSTRING);
    if ((fd = sys_open(pathbuf, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        return -1;
    sf->sf_fd = fd;

        /* write header */
    headersize = sf->sf_type->t_writeheaderfn(sf, nframes);
    if (headersize < 0)
        goto badcreate;
    sf->sf_headersize = headersize;
    return fd;

badcreate:
    sf->sf_fd = -1;
    if (fd >= 0)
        sys_close(fd);
    return -1;
}

static void soundfile_finishwrite(void *obj, const char *filename,
    t_soundfile *sf, size_t nframes, size_t frameswritten)
{
    if (frameswritten >= nframes) return;
    if (nframes < SFMAXFRAMES)
        pd_error(obj, "soundfiler write: %ld out of %ld frames written",
            (long)frameswritten, (long)nframes);
    if (sf->sf_type->t_updateheaderfn(sf, frameswritten))
        return;
    object_sferror(obj, "soundfiler write", filename, errno, sf);
}

static void soundfile_xferout_sample(const t_soundfile *sf,
    t_sample **vecs, unsigned char *buf, size_t nframes, size_t onsetframes,
    t_sample normalfactor)
{
    int i;
    size_t j;
    unsigned char *sp, *sp2;
    t_sample *fp;
    for (i = 0, sp = buf; i < sf->sf_nchannels; i++,
        sp += sf->sf_bytespersample)
    {
        if (sf->sf_bytespersample == 2)
        {
            t_sample ff = normalfactor * 32768.;
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                {
                    int xx = 32768. + (*fp * ff);
                    xx -= 32768;
                    if (xx < -32767)
                        xx = -32767;
                    if (xx > 32767)
                        xx = 32767;
                    sp2[0] = (xx >> 8);
                    sp2[1] = xx;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                {
                    int xx = 32768. + (*fp * ff);
                    xx -= 32768;
                    if (xx < -32767)
                        xx = -32767;
                    if (xx > 32767)
                        xx = 32767;
                    sp2[1] = (xx >> 8);
                    sp2[0] = xx;
                }
            }
        }
        else if (sf->sf_bytespersample == 3)
        {
            t_sample ff = normalfactor * 8388608.;
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                {
                    int xx = 8388608. + (*fp * ff);
                    xx -= 8388608;
                    if (xx < -8388607)
                        xx = -8388607;
                    if (xx > 8388607)
                        xx = 8388607;
                    sp2[0] = (xx >> 16);
                    sp2[1] = (xx >> 8);
                    sp2[2] = xx;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                {
                    int xx = 8388608. + (*fp * ff);
                    xx -= 8388608;
                    if (xx < -8388607)
                        xx = -8388607;
                    if (xx > 8388607)
                        xx = 8388607;
                    sp2[2] = (xx >> 16);
                    sp2[1] = (xx >> 8);
                    sp2[0] = xx;
                }
            }
        }
        else if (sf->sf_bytespersample == 4)
        {
            t_floatuint f2;
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                {
                    f2.f = *fp * normalfactor;
                    sp2[0] = (f2.ui >> 24); sp2[1] = (f2.ui >> 16);
                    sp2[2] = (f2.ui >> 8);  sp2[3] = f2.ui;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, fp++)
                {
                    f2.f = *fp * normalfactor;
                    sp2[3] = (f2.ui >> 24); sp2[2] = (f2.ui >> 16);
                    sp2[1] = (f2.ui >> 8);  sp2[0] = f2.ui;
                }
            }
        }
    }
}

static void soundfile_xferout_words(const t_soundfile *sf, t_word **vecs,
    unsigned char *buf, size_t nframes, size_t onsetframes,
    t_sample normalfactor)
{
    int i;
    size_t j;
    unsigned char *sp, *sp2;
    t_word *wp;
    for (i = 0, sp = buf; i < sf->sf_nchannels;
         i++, sp += sf->sf_bytespersample)
    {
        if (sf->sf_bytespersample == 2)
        {
            t_sample ff = normalfactor * 32768.;
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                {
                    int xx = 32768. + (wp->w_float * ff);
                    xx -= 32768;
                    if (xx < -32767)
                        xx = -32767;
                    if (xx > 32767)
                        xx = 32767;
                    sp2[0] = (xx >> 8);
                    sp2[1] = xx;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                {
                    int xx = 32768. + (wp->w_float * ff);
                    xx -= 32768;
                    if (xx < -32767)
                        xx = -32767;
                    if (xx > 32767)
                        xx = 32767;
                    sp2[1] = (xx >> 8);
                    sp2[0] = xx;
                }
            }
        }
        else if (sf->sf_bytespersample == 3)
        {
            t_sample ff = normalfactor * 8388608.;
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                {
                    int xx = 8388608. + (wp->w_float * ff);
                    xx -= 8388608;
                    if (xx < -8388607)
                        xx = -8388607;
                    if (xx > 8388607)
                        xx = 8388607;
                    sp2[0] = (xx >> 16);
                    sp2[1] = (xx >> 8);
                    sp2[2] = xx;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                {
                    int xx = 8388608. + (wp->w_float * ff);
                    xx -= 8388608;
                    if (xx < -8388607)
                        xx = -8388607;
                    if (xx > 8388607)
                        xx = 8388607;
                    sp2[2] = (xx >> 16);
                    sp2[1] = (xx >> 8);
                    sp2[0] = xx;
                }
            }
        }
        else if (sf->sf_bytespersample == 4)
        {
            t_floatuint f2;
            if (sf->sf_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                {
                    f2.f = wp->w_float * normalfactor;
                    sp2[0] = (f2.ui >> 24); sp2[1] = (f2.ui >> 16);
                    sp2[2] = (f2.ui >> 8);  sp2[3] = f2.ui;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += sf->sf_bytesperframe, wp++)
                {
                    f2.f = wp->w_float * normalfactor;
                    sp2[3] = (f2.ui >> 24); sp2[2] = (f2.ui >> 16);
                    sp2[1] = (f2.ui >> 8);  sp2[0] = f2.ui;
                }
            }
        }
    }
}

/* ----- soundfiler - reads and writes soundfiles to/from "garrays" ----- */

#define SAMPBUFSIZE 1024

static t_class *soundfiler_class;

typedef struct _soundfiler
{
    t_object x_obj;
    t_outlet *x_out2;
    t_canvas *x_canvas;
} t_soundfiler;

static t_soundfiler *soundfiler_new(void)
{
    t_soundfiler *x = (t_soundfiler *)pd_new(soundfiler_class);
    x->x_canvas = canvas_getcurrent();
    outlet_new(&x->x_obj, &s_float);
    x->x_out2 = outlet_new(&x->x_obj, &s_float);
    return x;
}

static int soundfiler_readascii(t_soundfiler *x, const char *filename,
    t_asciiargs *a)
{
    t_binbuf *b = binbuf_new();
    int i, j, vecsize;
    ssize_t framesinfile, nframes = a->aa_nframes;
    t_atom *atoms, *ap;
    if (binbuf_read_via_canvas(b, filename, x->x_canvas, 0))
    {
        nframes = 0;
        goto done;
    }
    atoms = binbuf_getvec(b);
    framesinfile = binbuf_getnatom(b) / a->aa_nchannels;
#ifdef DEBUG_SOUNDFILE
    post("ascii: read 1 natoms %d frames %d onset %d channels %d",
        binbuf_getnatom(b), nframes, a->aa_onsetframe, a->aa_nchannels);
#endif
    if (framesinfile < 1)
    {
        pd_error(x, "soundfiler read: %s: empty or very short ascii file",
            filename);
        nframes = 0;
        goto done;
    }
    if (a->aa_resize)
    {
        if ((size_t)framesinfile > a->aa_maxsize)
        {
            pd_error(x, "soundfiler read: truncated to %ld elements",
                (long)a->aa_maxsize);
            framesinfile = a->aa_maxsize;
        }
        nframes = framesinfile - a->aa_onsetframe;
        for (i = 0; i < a->aa_nchannels; i++)
        {
            garray_resize_long(a->aa_garrays[i], nframes);
            garray_setsaveit(a->aa_garrays[i], 0);
            if (!garray_getfloatwords(a->aa_garrays[i], &vecsize,
                &a->aa_vectors[i]) || (vecsize != nframes))
            {
                pd_error(x, "soundfiler read: resize failed");
                nframes = 0;
                goto done;
            }
        }
    }
    else if (nframes > framesinfile)
        nframes = framesinfile - a->aa_onsetframe;
#ifdef DEBUG_SOUNDFILE
    post("ascii: read 2 frames %d", nframes);
#endif
    if (a->aa_onsetframe > 0)
        atoms += a->aa_onsetframe * a->aa_nchannels;
    for (j = 0, ap = atoms; j < nframes; j++)
        for (i = 0; i < a->aa_nchannels; i++)
            a->aa_vectors[i][j].w_float = atom_getfloat(ap++);
        /* zero out remaining elements of vectors */
    for (i = 0; i < a->aa_nchannels; i++)
    {
        if (garray_getfloatwords(a->aa_garrays[i], &vecsize, &a->aa_vectors[i]))
            for (j = nframes; j < vecsize; j++)
                a->aa_vectors[i][j].w_float = 0;
    }
    for (i = 0; i < a->aa_nchannels; i++)
        garray_redraw(a->aa_garrays[i]);
#ifdef DEBUG_SOUNDFILE
    post("ascii: read 3");
#endif
done:
    binbuf_free(b);
    return nframes;
}

    /* soundfiler_read ...

       usage: read [flags] filename [tablename] ...
       flags:
           -skip <frames> ... frames to skip in file
           -raw <headersize channels bytespersample endian>
           -resize
           -maxsize <maxsize>
           -wave
           -aiff
           -caf
           -next
           -ascii
    */

static void soundfiler_read(t_soundfiler *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_soundfile sf = {0};
    int fd = -1, resize = 0, ascii = 0, raw = 0, i;
    size_t skipframes = 0, finalsize = 0, maxsize = SFMAXFRAMES,
           framesread = 0, bufframes, j;
    ssize_t nframes, framesinfile;
    char endianness;
    const char *filename;
    t_garray *garrays[MAXSFCHANS];
    t_word *vecs[MAXSFCHANS];
    char sampbuf[SAMPBUFSIZE];

    soundfile_clear(&sf);
    sf.sf_headersize = -1;
    while (argc > 0 && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        const char *flag = argv->a_w.w_symbol->s_name + 1;
        if (!strcmp(flag, "skip"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                (argv[1].a_w.w_float) < 0)
                    goto usage;
            skipframes = argv[1].a_w.w_float;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(flag, "ascii"))
        {
            if (sf.sf_headersize >= 0)
                post("'-ascii' overridden by '-raw'");
            ascii = 1;
            argc--; argv++;
        }
        else if (!strcmp(flag, "raw"))
        {
            if (ascii)
                post("'-ascii' overridden by '-raw'");
            else if (sf.sf_type)
                post("'-%s' overridden by '-raw'", sf.sf_type->t_name);
            if (argc < 5 ||
                argv[1].a_type != A_FLOAT ||
                ((sf.sf_headersize = argv[1].a_w.w_float) < 0) ||
                argv[2].a_type != A_FLOAT ||
                ((sf.sf_nchannels = argv[2].a_w.w_float) < 1) ||
                (sf.sf_nchannels > MAXSFCHANS) ||
                argv[3].a_type != A_FLOAT ||
                ((sf.sf_bytespersample = argv[3].a_w.w_float) < 2) ||
                    (sf.sf_bytespersample > 4) ||
                argv[4].a_type != A_SYMBOL ||
                    ((endianness = argv[4].a_w.w_symbol->s_name[0]) != 'b'
                    && endianness != 'l' && endianness != 'n'))
                        goto usage;
            if (endianness == 'b')
                sf.sf_bigendian = 1;
            else if (endianness == 'l')
                sf.sf_bigendian = 0;
            else
                sf.sf_bigendian = sys_isbigendian();
            sf.sf_samplerate = sys_getsr();
            sf.sf_bytesperframe = sf.sf_nchannels * sf.sf_bytespersample;
            raw = 1;
            argc -= 5; argv += 5;
        }
        else if (!strcmp(flag, "resize"))
        {
            resize = 1;
            argc -= 1; argv += 1;
        }
        else if (!strcmp(flag, "maxsize"))
        {
            ssize_t tmp;
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                ((tmp = (argv[1].a_w.w_float > SFMAXFRAMES ?
                SFMAXFRAMES : argv[1].a_w.w_float)) < 0))
                    goto usage;
            maxsize = (size_t)tmp;
            resize = 1;     /* maxsize implies resize */
            argc -= 2; argv += 2;
        }
        else
        {
                /* check for type by name */
            if (!(sf.sf_type = soundfile_findtype(flag)))
                goto usage; /* unknown flag */
            ascii = 0; /* replaced */
            if (sf.sf_headersize >= 0)
                post("'-%s' overridden by '-raw'", sf.sf_type->t_name);
            argc -= 1; argv += 1;
        }
    }
    if (sf.sf_headersize >= 0)
    {
        ascii = 0;
        sf.sf_type = NULL;
    }
    if (argc < 1 ||                           /* no filename or tables */
        argc > MAXSFCHANS + 1 ||              /* too many tables */
        argv[0].a_type != A_SYMBOL)           /* bad filename */
            goto usage;
    filename = argv[0].a_w.w_symbol->s_name;
    argc--; argv++;

        /* check for implicit ascii */
    if (!ascii && !sf.sf_type && sf.sf_headersize < 0)
        ascii = ascii_hasextension(filename, MAXPDSTRING);

    for (i = 0; i < argc; i++)
    {
        int vecsize;
        if (argv[i].a_type != A_SYMBOL)
            goto usage;
        if (!(garrays[i] =
            (t_garray *)pd_findbyclass(argv[i].a_w.w_symbol, garray_class)))
        {
            pd_error(x, "soundfiler read: %s: no such table",
                argv[i].a_w.w_symbol->s_name);
            goto done;
        }
        else if (!garray_getfloatwords(garrays[i], &vecsize,
                &vecs[i]))
            pd_error(x, "soundfiler read: %s: bad template for tabwrite",
                argv[i].a_w.w_symbol->s_name);
        if (finalsize && finalsize != (size_t)vecsize && !resize)
        {
            post("arrays have different lengths, resizing...");
            resize = 1;
        }
        finalsize = vecsize;
    }
    if (ascii)
    {
        t_asciiargs a =
            {skipframes, finalsize, argc, vecs, garrays, resize, maxsize, 0};
        if (!argc)
        {
            pd_error(x, "soundfiler read: "
                        "'-ascii' requires at least one table");
            goto done;
        }
        if ((framesread = soundfiler_readascii(x, filename, &a)) == 0)
            goto done;
            /* fill in for info outlet */
        sf.sf_samplerate = sys_getsr();
        sf.sf_headersize = 0;
        sf.sf_nchannels = argc;
        sf.sf_bytespersample = 4;
        sf.sf_bigendian = sys_isbigendian();
        goto done;
    }

    fd = open_soundfile_via_canvas(x->x_canvas, filename, &sf, skipframes);
    if (fd < 0)
    {
        object_sferror(x, "soundfiler read", filename, errno, &sf);
        goto done;
    }
    framesinfile = sf.sf_bytelimit / sf.sf_bytesperframe;

    if (resize)
    {
            /* figure out what to resize to using header info */
        if ((size_t)framesinfile > maxsize)
        {
            pd_error(x, "soundfiler read: truncated to %ld elements",
                (long)maxsize);
            framesinfile = maxsize;
        }
        finalsize = framesinfile;
        for (i = 0; i < argc; i++)
        {
            int vecsize;
            garray_resize_long(garrays[i], finalsize);
                /* for sanity's sake let's clear the save-in-patch flag here */
            garray_setsaveit(garrays[i], 0);
            if (!garray_getfloatwords(garrays[i], &vecsize, &vecs[i])
                /* if the resize failed, garray_resize reported the error */
                || (vecsize != framesinfile))
            {
                pd_error(x, "soundfiler read: resize failed");
                goto done;
            }
        }
    }

    if (!finalsize) finalsize = SFMAXFRAMES;
    if (framesinfile >= 0 && finalsize > (size_t)framesinfile)
        finalsize = framesinfile;

        /* no tablenames, try to use header info instead of reading */
    if (argc == 0 &&
        !(raw ||                     /* read if raw */
          finalsize == SFMAXFRAMES)) /* read if unknown size */
    {
        framesread = finalsize;
#ifdef DEBUG_SOUNDFILE
    post("skipped reading frames");
#endif
        goto done;
    }

        /* read */
#ifdef DEBUG_SOUNDFILE
    post("reading frames");
#endif
    bufframes = SAMPBUFSIZE / sf.sf_bytesperframe;
    for (framesread = 0; framesread < finalsize;)
    {
        size_t thisread = finalsize - framesread;
        thisread = (thisread > bufframes ? bufframes : thisread);
        nframes = read(sf.sf_fd, sampbuf,
            thisread * sf.sf_bytesperframe) / sf.sf_bytesperframe;
        if (nframes <= 0) break;
        soundfile_xferin_words(&sf, argc, vecs, framesread,
            (unsigned char *)sampbuf, nframes);
        framesread += nframes;
    }
        /* warn if a file's bad size field is gobbling memory */
    if (resize && framesread < (size_t)finalsize)
    {
        post("warning: soundfile %s header promised \
%ld points but file was truncated to %ld",
            filename, (long)finalsize, (long)framesread);
    }
        /* zero out remaining elements of vectors */
    for (i = 0; i < argc; i++)
    {
        int vecsize;
        if (garray_getfloatwords(garrays[i], &vecsize, &vecs[i]))
            for (j = framesread; j < (size_t)vecsize; j++)
                vecs[i][j].w_float = 0;
    }
        /* zero out vectors in excess of number of channels */
    for (i = sf.sf_nchannels; i < argc; i++)
    {
        int vecsize;
        t_word *foo;
        if (garray_getfloatwords(garrays[i], &vecsize, &foo))
            for (j = 0; j < (size_t)vecsize; j++)
                foo[j].w_float = 0;
    }
        /* do all graphics updates */
    for (i = 0; i < argc; i++)
        garray_redraw(garrays[i]);
    goto done;
usage:
    pd_error(x, "usage: read [flags] filename [tablename]...");
    post("flags: -skip <n> -resize -maxsize <n> %s -ascii ...", sf_typeargs);
    post("-raw <headerbytes> <channels> <bytespersample> "
         "<endian (b, l, or n)>");
done:
    sf.sf_fd = -1;
    if (fd >= 0)
        sys_close(fd);
    outlet_soundfileinfo(x->x_out2, &sf);
    outlet_float(x->x_obj.ob_outlet, (t_float)framesread);
}

    /** write to an ascii text file, channels (nvecs) are interleaved,
        assumes all vectors are at least nframes in length
        adapted from garray_savecontentsto()
        returns frames written on success or 0 on error */
int soundfiler_writeascii(t_soundfiler *x, const char *filename, t_asciiargs *a)
{
    char path[MAXPDSTRING];
    t_binbuf *b = binbuf_new();
    int i, j, frameswritten = 0, ret = 1;
#ifdef DEBUG_SOUNDFILE
    post("ascii write: frames %d onset %d channels %d",
        a->aa_nframes, a->aa_onsetframe, a->aa_nchannels);
#endif
    canvas_makefilename(x->x_canvas, filename, path, MAXPDSTRING);
    if (a->aa_nframes > 200000)
        post("warning: writing %d table points to ascii file!");
    for (i = a->aa_onsetframe; frameswritten < a->aa_nframes; ++i)
    {
        for (j = 0; j < a->aa_nchannels; ++j)
            binbuf_addv(b, "f", a->aa_vectors[j][i].w_float * a->aa_normfactor);
        frameswritten++;
    }
    binbuf_addv(b, ";");
    ret = binbuf_write(b, path, "", 1); /* convert semis to cr */
    binbuf_free(b);
    return (ret == 0 ? frameswritten : 0);
}

    /** this is broken out from soundfiler_write below so garray_write can
        call it too... not done yet though. */
size_t soundfiler_dowrite(void *obj, t_canvas *canvas,
    int argc, t_atom *argv, t_soundfile *sf)
{
    t_soundfiler_writeargs wa = {0};
    int fd = -1, i;
    size_t bufframes, frameswritten = 0, j;
    t_garray *garrays[MAXSFCHANS];
    t_word *vectors[MAXSFCHANS];
    char sampbuf[SAMPBUFSIZE];
    t_sample normfactor = 1, biggest = 0;

    soundfile_clear(sf);
    if (soundfiler_parsewriteargs(obj, &argc, &argv, &wa))
        goto usage;
    sf->sf_type = (wa.wa_ascii ? NULL : wa.wa_type);
    sf->sf_nchannels = argc;
    sf->sf_samplerate = wa.wa_samplerate;
    sf->sf_bytespersample = wa.wa_bytespersample;
    sf->sf_bigendian = wa.wa_bigendian;
    sf->sf_bytesperframe = argc * wa.wa_bytespersample;
    if (sf->sf_nchannels < 1 || sf->sf_nchannels > MAXSFCHANS)
        goto usage;
    if (sf->sf_samplerate <= 0)
        sf->sf_samplerate = sys_getsr();
    for (i = 0; i < sf->sf_nchannels; i++)
    {
        int vecsize;
        if (argv[i].a_type != A_SYMBOL)
            goto usage;
        if (!(garrays[i] =
            (t_garray *)pd_findbyclass(argv[i].a_w.w_symbol, garray_class)))
        {
            pd_error(obj, "soundfiler write: %s: no such table",
                argv[i].a_w.w_symbol->s_name);
            goto fail;
        }
        else if (!garray_getfloatwords(garrays[i], &vecsize, &vectors[i]))
            pd_error(obj, "soundfiler write: %s: bad template for tabwrite",
                argv[i].a_w.w_symbol->s_name);
        if (wa.wa_nframes > vecsize - wa.wa_onsetframes)
            wa.wa_nframes = vecsize - wa.wa_onsetframes;
    }
    if (wa.wa_nframes <= 0)
    {
        pd_error(obj, "soundfiler write: no samples at onset %ld",
            (long)wa.wa_onsetframes);
        goto fail;
    }

        /* find biggest sample for normalizing */
    for (i = 0; i < sf->sf_nchannels; i++)
    {
        for (j = wa.wa_onsetframes; j < wa.wa_nframes + wa.wa_onsetframes; j++)
        {
            if (vectors[i][j].w_float > biggest)
                biggest = vectors[i][j].w_float;
            else if (-vectors[i][j].w_float > biggest)
                biggest = -vectors[i][j].w_float;
        }
    }

        /* write to ascii text file? */
    if (wa.wa_ascii)
    {
        char filenamebuf[MAXPDSTRING];
        t_asciiargs a =
            {wa.wa_onsetframes, wa.wa_nframes, sf->sf_nchannels, vectors, 0,
                0, 0, 0};
        strcpy(filenamebuf, wa.wa_filesym->s_name);
        if (!ascii_hasextension(filenamebuf, MAXPDSTRING))
            ascii_addextension(filenamebuf, MAXPDSTRING);
        if (wa.wa_normalize)
            a.aa_normfactor = (biggest > 0 ? 32767./(32768. * biggest) : 1);
        else
            a.aa_normfactor = 1;
        if ((frameswritten = soundfiler_writeascii(obj, filenamebuf, &a)) == 0)
        {
            pd_error(obj, "soundfiler write: writing ascii failed");
            goto fail;
        }
            /* fill in for info outlet */
        sf->sf_headersize = 0;
        sf->sf_bytespersample = 4;
        sf->sf_bigendian = sys_isbigendian();
        return frameswritten;
    }

        /* create file and detect if int samples should be normalized */
    if ((fd = create_soundfile(canvas, wa.wa_filesym->s_name,
        sf, wa.wa_nframes)) < 0)
    {
        object_sferror(obj, "soundfiler write",
            wa.wa_filesym->s_name, errno, sf);
        goto fail;
    }
    if (!wa.wa_normalize)
    {
        if (sf->sf_bytespersample != 4 && biggest > 1)
        {
            post("%s: reducing max amplitude %f to 1",
                wa.wa_filesym->s_name, biggest);
            wa.wa_normalize = 1;
        }
        else post("%s: biggest amplitude = %f",
            wa.wa_filesym->s_name, biggest);
    }
    if (wa.wa_normalize)
        normfactor = (biggest > 0 ? 32767./(32768. * biggest) : 1);

        /* write samples */
    bufframes = SAMPBUFSIZE / sf->sf_bytesperframe;
    for (frameswritten = 0; frameswritten < wa.wa_nframes;)
    {
        size_t thiswrite = wa.wa_nframes - frameswritten,
               datasize;
        ssize_t byteswritten;
        thiswrite = (thiswrite > bufframes ? bufframes : thiswrite);
        datasize = sf->sf_bytesperframe * thiswrite;
        soundfile_xferout_words(sf, vectors, (unsigned char *)sampbuf,
            thiswrite, wa.wa_onsetframes, normfactor);
        byteswritten = write(sf->sf_fd, sampbuf, datasize);
        if (byteswritten < 0 || (size_t)byteswritten < datasize)
        {
            object_sferror(obj, "soundfiler write",
                wa.wa_filesym->s_name, errno, sf);
            if (byteswritten > 0)
                frameswritten += byteswritten / sf->sf_bytesperframe;
            break;
        }
        frameswritten += thiswrite;
        wa.wa_onsetframes += thiswrite;
    }
        /* update header frame size */
    if (fd >= 0)
    {
        soundfile_finishwrite(obj, wa.wa_filesym->s_name, sf,
                wa.wa_nframes, frameswritten);
        sys_close(fd);
    }
    sf->sf_fd = -1;
    return frameswritten;
usage:
    pd_error(obj, "usage: write [flags] filename tablename...");
    post("flags: -skip <n> -nframes <n> -bytes <n> %s ...", sf_typeargs);
    post("-ascii -big -little -normalize");
    post("(defaults to a 16 bit wave file)");
fail:
    soundfile_clear(sf); /* clear any bad data */
    if (fd >= 0)
        sys_close(fd);
    return 0;
}

static void soundfiler_write(t_soundfiler *x, t_symbol *s,
    int argc, t_atom *argv)
{
    size_t frameswritten;
    t_soundfile sf = {0};
    frameswritten = soundfiler_dowrite(x, x->x_canvas, argc, argv, &sf);
    outlet_soundfileinfo(x->x_out2, &sf);
    outlet_float(x->x_obj.ob_outlet, (t_float)frameswritten);
}

static void soundfiler_setup(void)
{
    soundfiler_class = class_new(gensym("soundfiler"),
        (t_newmethod)soundfiler_new, 0,
        sizeof(t_soundfiler), 0, 0);
    class_addmethod(soundfiler_class, (t_method)soundfiler_read,
        gensym("read"), A_GIMME, 0);
    class_addmethod(soundfiler_class, (t_method)soundfiler_write,
        gensym("write"), A_GIMME, 0);
}

/* ------------------------- readsf object ------------------------- */

/* READSF uses the Posix threads package; for the moment we're Linux
only although this should be portable to the other platforms.

Each instance of readsf~ owns a "child" thread for doing the Posix file reading.
The parent thread signals the child each time:
    (1) a file wants opening or closing;
    (2) we've eaten another 1/16 of the shared buffer (so that the
        child thread should check if it's time to read some more.)
The child signals the parent whenever a read has completed.  Signaling
is done by setting "conditions" and putting data in mutex-controlled common
areas.
*/

#define MAXVECSIZE 128

#define READSIZE 65536
#define WRITESIZE 65536
#define DEFBUFPERCHAN 262144
#define MINBUFSIZE (4 * READSIZE)
#define MAXBUFSIZE 16777216     /* arbitrary; just don't want to hang malloc */

    /* read/write thread request type */
typedef enum _soundfile_request
{
    REQUEST_NOTHING = 0,
    REQUEST_OPEN    = 1,
    REQUEST_CLOSE   = 2,
    REQUEST_QUIT    = 3,
    REQUEST_BUSY    = 4
} t_soundfile_request;

    /* read/write thread state */
typedef enum _soundfile_state
{
    STATE_IDLE    = 0,
    STATE_STARTUP = 1,
    STATE_STREAM  = 2
} t_soundfile_state;

static t_class *readsf_class;

typedef struct _readsf
{
    t_object x_obj;
    t_canvas *x_canvas;
    t_clock *x_clock;
    char *x_buf;                      /**< soundfile buffer */
    int x_bufsize;                    /**< buffer size in bytes */
    int x_noutlets;                   /**< number of audio outlets */
    t_sample *(x_outvec[MAXSFCHANS]); /**< audio vectors */
    int x_vecsize;                    /**< vector size for transfers */
    t_outlet *x_bangout;              /**< bang-on-done outlet */
    t_soundfile_state x_state;        /**< opened, running, or idle */
    t_float x_insamplerate;           /**< input signal sample rate, if known */
        /* parameters to communicate with subthread */
    t_soundfile_request x_requestcode; /**< pending request to I/O thread */
    const char *x_filename;   /**< file to open (string permanently allocated) */
    int x_fileerror;          /**< slot for "errno" return */
    t_soundfile x_sf;         /**< soundfile fd, type, and format info */
    size_t x_onsetframes;     /**< number of sample frames to skip */
    int x_fifosize;           /**< buffer size appropriately rounded down */
    int x_fifohead;           /**< index of next byte to get from file */
    int x_fifotail;           /**< index of next byte the ugen will read */
    int x_eof;                /**< true if fifohead has stopped changing */
    int x_sigcountdown;       /**< counter for signaling child for more data */
    int x_sigperiod;          /**< number of ticks per signal */
    size_t x_frameswritten;   /**< writesf~ only; frames written */
    t_float x_f;              /**< writesf~ only; scalar for signal inlet */
    pthread_mutex_t x_mutex;
    pthread_cond_t x_requestcondition;
    pthread_cond_t x_answercondition;
    pthread_t x_childthread;
#ifdef PDINSTANCE
    t_pdinstance *x_pd_this;  /**< pointer to the owner pd instance */
#endif
} t_readsf;

/* ----- the child thread which performs file I/O ----- */

    /** thread state debug prints to stderr */
//#define DEBUG_SOUNDFILE_THREADS

#if 1
#define sfread_cond_wait pthread_cond_wait
#define sfread_cond_signal pthread_cond_signal
#else
#include <sys/time.h>    /* debugging version... */
#include <sys/types.h>
static void readsf_fakewait(pthread_mutex_t *b)
{
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000000;
    pthread_mutex_unlock(b);
    select(0, 0, 0, 0, &timeout);
    pthread_mutex_lock(b);
}

#define sfread_cond_wait(a,b) readsf_fakewait(b)
#define sfread_cond_signal(a)
#endif

static void *readsf_child_main(void *zz)
{
    t_readsf *x = zz;
    t_soundfile sf = {0};
    soundfile_clear(&sf);
#ifdef PDINSTANCE
    pd_this = x->x_pd_this;
#endif
#ifdef DEBUG_SOUNDFILE_THREADS
    fprintf(stderr, "readsf~: 1\n");
#endif
    pthread_mutex_lock(&x->x_mutex);
    while (1)
    {
        int fifohead;
        char *buf;
#ifdef DEBUG_SOUNDFILE_THREADS
        fprintf(stderr, "readsf~: 0\n");
#endif
        if (x->x_requestcode == REQUEST_NOTHING)
        {
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "readsf~: wait 2\n");
#endif
            sfread_cond_signal(&x->x_answercondition);
            sfread_cond_wait(&x->x_requestcondition, &x->x_mutex);
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "readsf~: 3\n");
#endif
        }
        else if (x->x_requestcode == REQUEST_OPEN)
        {
            ssize_t bytesread;
            size_t wantbytes;

                /* copy file stuff out of the data structure so we can
                relinquish the mutex while we're in open_soundfile_via_path() */
            size_t onsetframes = x->x_onsetframes;
            const char *filename = x->x_filename;
            const char *dirname = canvas_getdir(x->x_canvas)->s_name;

#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "readsf~: 4\n");
#endif
                /* alter the request code so that an ensuing "open" will get
                noticed. */
            x->x_requestcode = REQUEST_BUSY;
            x->x_fileerror = 0;

                /* if there's already a file open, close it */
            if (sf.sf_fd >= 0)
            {
                pthread_mutex_unlock(&x->x_mutex);
                sys_close(sf.sf_fd);
                sf.sf_fd = -1;
                pthread_mutex_lock(&x->x_mutex);
                x->x_sf.sf_fd = -1;
                if (x->x_requestcode != REQUEST_BUSY)
                    goto lost;
            }
                /* cache sf *after* closing as x->sf's type
                    may have changed in readsf_open() */
            soundfile_copy(&sf, &x->x_sf);

                /* open the soundfile with the mutex unlocked */
            pthread_mutex_unlock(&x->x_mutex);
            open_soundfile_via_path(dirname, filename, &sf, onsetframes);
            pthread_mutex_lock(&x->x_mutex);

#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "readsf~: 5\n");
#endif
            if (sf.sf_fd < 0)
            {
                x->x_fileerror = errno;
                x->x_eof = 1;
#ifdef DEBUG_SOUNDFILE_THREADS
                fprintf(stderr, "readsf~: open failed %s %s\n",
                    filename, dirname);
#endif
                goto lost;
            }
                /* copy back into the instance structure. */
            soundfile_copy(&x->x_sf, &sf);
                /* check if another request has been made; if so, field it */
            if (x->x_requestcode != REQUEST_BUSY)
                goto lost;
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "readsf~: 6\n");
#endif
            x->x_fifohead = 0;
                    /* set fifosize from bufsize.  fifosize must be a
                    multiple of the number of bytes eaten for each DSP
                    tick.  We pessimistically assume MAXVECSIZE samples
                    per tick since that could change.  There could be a
                    problem here if the vector size increases while a
                    soundfile is being played...  */
            x->x_fifosize = x->x_bufsize - (x->x_bufsize %
                (sf.sf_bytesperframe * MAXVECSIZE));
                    /* arrange for the "request" condition to be signaled 16
                    times per buffer */
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "readsf~: fifosize %d\n", x->x_fifosize);
#endif
            x->x_sigcountdown = x->x_sigperiod = (x->x_fifosize /
                (16 * sf.sf_bytesperframe * x->x_vecsize));
                /* in a loop, wait for the fifo to get hungry and feed it */

            while (x->x_requestcode == REQUEST_BUSY)
            {
                int fifosize = x->x_fifosize;
#ifdef DEBUG_SOUNDFILE_THREADS
                fprintf(stderr, "readsf~: 77\n");
#endif
                if (x->x_eof)
                    break;
                if (x->x_fifohead >= x->x_fifotail)
                {
                        /* if the head is >= the tail, we can immediately read
                        to the end of the fifo.  Unless, that is, we would
                        read all the way to the end of the buffer and the
                        "tail" is zero; this would fill the buffer completely
                        which isn't allowed because you can't tell a completely
                        full buffer from an empty one. */
                    if (x->x_fifotail || (fifosize - x->x_fifohead > READSIZE))
                    {
                        wantbytes = fifosize - x->x_fifohead;
                        if (wantbytes > READSIZE)
                            wantbytes = READSIZE;
                        if (sf.sf_bytelimit >= 0 && wantbytes > (size_t)sf.sf_bytelimit)
                            wantbytes = sf.sf_bytelimit;
#ifdef DEBUG_SOUNDFILE_THREADS
                        fprintf(stderr, "readsf~: head %d, tail %d, size %ld\n",
                            x->x_fifohead, x->x_fifotail, wantbytes);
#endif
                    }
                    else
                    {
#ifdef DEBUG_SOUNDFILE_THREADS
                        fprintf(stderr, "readsf~: wait 7a...\n");
#endif
                        sfread_cond_signal(&x->x_answercondition);
#ifdef DEBUG_SOUNDFILE_THREADS
                        fprintf(stderr, "readsf~: signaled...\n");
#endif
                        sfread_cond_wait(&x->x_requestcondition, &x->x_mutex);
#ifdef DEBUG_SOUNDFILE_THREADS
                        fprintf(stderr, "readsf~: 7a ... done\n");
#endif
                        continue;
                    }
                }
                else
                {
                        /* otherwise check if there are at least READSIZE
                        bytes to read.  If not, wait and loop back. */
                    wantbytes =  x->x_fifotail - x->x_fifohead - 1;
                    if (wantbytes < READSIZE)
                    {
#ifdef DEBUG_SOUNDFILE_THREADS
                        fprintf(stderr, "readsf~: wait 7...\n");
#endif
                        sfread_cond_signal(&x->x_answercondition);
                        sfread_cond_wait(&x->x_requestcondition, &x->x_mutex);
#ifdef DEBUG_SOUNDFILE_THREADS
                        fprintf(stderr, "readsf~: 7 ... done\n");
#endif
                        continue;
                    }
                    else wantbytes = READSIZE;
                    if (sf.sf_bytelimit >= 0 && wantbytes > (size_t)sf.sf_bytelimit)
                        wantbytes = sf.sf_bytelimit;
                }
#ifdef DEBUG_SOUNDFILE_THREADS
                fprintf(stderr, "readsf~: 8\n");
#endif
                buf = x->x_buf;
                fifohead = x->x_fifohead;
                pthread_mutex_unlock(&x->x_mutex);
                bytesread = read(sf.sf_fd, buf + fifohead, wantbytes);
                pthread_mutex_lock(&x->x_mutex);
                if (x->x_requestcode != REQUEST_BUSY)
                    break;
                if (bytesread < 0)
                {
#ifdef DEBUG_SOUNDFILE_THREADS
                    fprintf(stderr, "readsf~: fileerror %d\n", errno);
#endif
                    x->x_fileerror = errno;
                    break;
                }
                else if (bytesread == 0)
                {
                    x->x_eof = 1;
                    break;
                }
                else
                {
                    x->x_fifohead += bytesread;
                    sf.sf_bytelimit -= bytesread;
                    if (x->x_fifohead == fifosize)
                        x->x_fifohead = 0;
                    if (sf.sf_bytelimit <= 0)
                    {
                        x->x_eof = 1;
                        break;
                    }
                }
#ifdef DEBUG_SOUNDFILE_THREADS
                fprintf(stderr, "readsf~: after, head %d tail %d\n",
                    x->x_fifohead, x->x_fifotail);
#endif
                    /* signal parent in case it's waiting for data */
                sfread_cond_signal(&x->x_answercondition);
            }

        lost:
            if (x->x_requestcode == REQUEST_BUSY)
                x->x_requestcode = REQUEST_NOTHING;
#ifdef DEBUG_SOUNDFILE_THREADS
                fprintf(stderr, "readsf~: lost\n");
#endif
                /* fell out of read loop: close file if necessary,
                set EOF and signal once more */
            if (sf.sf_fd >= 0)
            {
                    /* only set EOF if there is no pending "open" request!
                    Otherwise, we might accidentally set EOF after it has been
                    unset in readsf_open() and the stream would fail silently. */
                if (x->x_requestcode != REQUEST_OPEN)
                    x->x_eof = 1;
                x->x_sf.sf_fd = -1;
                    /* use cached sf */
                pthread_mutex_unlock(&x->x_mutex);
                sys_close(sf.sf_fd);
                sf.sf_fd = -1;
                pthread_mutex_lock(&x->x_mutex);
            }
            sfread_cond_signal(&x->x_answercondition);
        }
        else if (x->x_requestcode == REQUEST_CLOSE)
        {
            if (sf.sf_fd >= 0)
            {
                x->x_sf.sf_fd = -1;
                    /* use cached sf */
                pthread_mutex_unlock(&x->x_mutex);
                sys_close(sf.sf_fd);
                sf.sf_fd = -1;
                pthread_mutex_lock(&x->x_mutex);
            }
            if (x->x_requestcode == REQUEST_CLOSE)
                x->x_requestcode = REQUEST_NOTHING;
            sfread_cond_signal(&x->x_answercondition);
        }
        else if (x->x_requestcode == REQUEST_QUIT)
        {
            if (sf.sf_fd >= 0)
            {
                x->x_sf.sf_fd = -1;
                    /* use cached sf */
                pthread_mutex_unlock(&x->x_mutex);
                sys_close(sf.sf_fd);
                sf.sf_fd = -1;
                pthread_mutex_lock(&x->x_mutex);
            }
            x->x_requestcode = REQUEST_NOTHING;
            sfread_cond_signal(&x->x_answercondition);
            break;
        }
        else
        {
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "readsf~: 13\n");
#endif
        }
    }
#ifdef DEBUG_SOUNDFILE_THREADS
    fprintf(stderr, "readsf~: thread exit\n");
#endif
    pthread_mutex_unlock(&x->x_mutex);
    return 0;
}

/* ----- the object proper runs in the calling (parent) thread ----- */

static void readsf_tick(t_readsf *x);

static void *readsf_new(t_floatarg fnchannels, t_floatarg fbufsize)
{
    t_readsf *x;
    int nchannels = fnchannels, bufsize = fbufsize, i;
    char *buf;

    if (nchannels < 1)
        nchannels = 1;
    else if (nchannels > MAXSFCHANS)
        nchannels = MAXSFCHANS;
    if (bufsize <= 0) bufsize = DEFBUFPERCHAN * nchannels;
    else if (bufsize < MINBUFSIZE)
        bufsize = MINBUFSIZE;
    else if (bufsize > MAXBUFSIZE)
        bufsize = MAXBUFSIZE;
    buf = getbytes(bufsize);
    if (!buf) return 0;

    x = (t_readsf *)pd_new(readsf_class);

    for (i = 0; i < nchannels; i++)
        outlet_new(&x->x_obj, gensym("signal"));
    x->x_noutlets = nchannels;
    x->x_bangout = outlet_new(&x->x_obj, &s_bang);
    pthread_mutex_init(&x->x_mutex, 0);
    pthread_cond_init(&x->x_requestcondition, 0);
    pthread_cond_init(&x->x_answercondition, 0);
    x->x_vecsize = MAXVECSIZE;
    x->x_state = STATE_IDLE;
    x->x_clock = clock_new(x, (t_method)readsf_tick);
    x->x_canvas = canvas_getcurrent();
    soundfile_clear(&x->x_sf);
    x->x_sf.sf_bytespersample = 2;
    x->x_sf.sf_nchannels = 1;
    x->x_sf.sf_bytesperframe = 2;
    x->x_buf = buf;
    x->x_bufsize = bufsize;
    x->x_fifosize = x->x_fifohead = x->x_fifotail = x->x_requestcode = 0;
#ifdef PDINSTANCE
    x->x_pd_this = pd_this;
#endif
    pthread_create(&x->x_childthread, 0, readsf_child_main, x);
    return x;
}

static void readsf_tick(t_readsf *x)
{
    outlet_bang(x->x_bangout);
}

static t_int *readsf_perform(t_int *w)
{
    t_readsf *x = (t_readsf *)(w[1]);
    int vecsize = x->x_vecsize, noutlets = x->x_noutlets, i;
    size_t j;
    t_sample *fp;
    if (x->x_state == STATE_STREAM)
    {
        int wantbytes;
        t_soundfile sf = {0};
        pthread_mutex_lock(&x->x_mutex);
            /* copy with mutex locked! */
        soundfile_copy(&sf, &x->x_sf);
        wantbytes = vecsize * sf.sf_bytesperframe;
        while (!x->x_eof && x->x_fifohead >= x->x_fifotail &&
                x->x_fifohead < x->x_fifotail + wantbytes-1)
        {
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "readsf~: wait...\n");
#endif
            sfread_cond_signal(&x->x_requestcondition);
            sfread_cond_wait(&x->x_answercondition, &x->x_mutex);
                /* resync local variables -- bug fix thanks to Shahrokh */
            vecsize = x->x_vecsize;
            soundfile_copy(&sf, &x->x_sf);
            wantbytes = vecsize * sf.sf_bytesperframe;
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "readsf~: ... done\n");
#endif
        }
        if (x->x_eof && x->x_fifohead >= x->x_fifotail &&
            x->x_fifohead < x->x_fifotail + wantbytes-1)
        {
            int xfersize;
            if (x->x_fileerror)
                object_sferror(x, "readsf~", x->x_filename,
                    x->x_fileerror, &x->x_sf);
                /* if there's a partial buffer left, copy it out */
            xfersize = (x->x_fifohead - x->x_fifotail + 1) /
                       sf.sf_bytesperframe;
            if (xfersize)
            {
                soundfile_xferin_sample(&sf, noutlets, x->x_outvec, 0,
                    (unsigned char *)(x->x_buf + x->x_fifotail), xfersize);
                vecsize -= xfersize;
            }
            pthread_mutex_unlock(&x->x_mutex);
                /* send bang and zero out the (rest of the) output */
            clock_delay(x->x_clock, 0);
            x->x_state = STATE_IDLE;
            for (i = 0; i < noutlets; i++)
                for (j = vecsize, fp = x->x_outvec[i] + xfersize; j--;)
                    *fp++ = 0;
            return w + 2;
        }

        soundfile_xferin_sample(&sf, noutlets, x->x_outvec, 0,
            (unsigned char *)(x->x_buf + x->x_fifotail), vecsize);

        x->x_fifotail += wantbytes;
        if (x->x_fifotail >= x->x_fifosize)
            x->x_fifotail = 0;
        if ((--x->x_sigcountdown) <= 0)
        {
            sfread_cond_signal(&x->x_requestcondition);
            x->x_sigcountdown = x->x_sigperiod;
        }
        pthread_mutex_unlock(&x->x_mutex);
    }
    else
    {
        for (i = 0; i < noutlets; i++)
            for (j = vecsize, fp = x->x_outvec[i]; j--;)
                *fp++ = 0;
    }
    return w + 2;
}

    /** start making output.  If we're in the "startup" state change
        to the "running" state. */
static void readsf_start(t_readsf *x)
{
    if (x->x_state == STATE_STARTUP)
        x->x_state = STATE_STREAM;
    else pd_error(x, "readsf~: start requested with no prior 'open'");
}

    /** LATER rethink whether you need the mutex just to set a variable? */
static void readsf_stop(t_readsf *x)
{
    pthread_mutex_lock(&x->x_mutex);
    x->x_state = STATE_IDLE;
    x->x_requestcode = REQUEST_CLOSE;
    sfread_cond_signal(&x->x_requestcondition);
    pthread_mutex_unlock(&x->x_mutex);
}

static void readsf_float(t_readsf *x, t_floatarg f)
{
    if (f != 0)
        readsf_start(x);
    else readsf_stop(x);
}

    /** open method.  Called as:
        open [flags] filename [onsetframes headersize channels bytes endianness]
        (if headersize is zero, header is taken to be automatically detected;
        thus, use the special "-1" to mean a truly headerless file.)
        if type implementation is set, pass this to open unless headersize is -1 */
static void readsf_open(t_readsf *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *filesym, *endian;
    t_float onsetframes, headersize, nchannels, bytespersample;
    t_soundfile_type *type = NULL;

    while (argc > 0 && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
            /* check for type by name */
        const char *flag = argv->a_w.w_symbol->s_name + 1;
        if (!(type = soundfile_findtype(flag)))
            goto usage; /* unknown flag */
        argc -= 1; argv += 1;
    }
    filesym = atom_getsymbolarg(0, argc, argv);
    onsetframes = atom_getfloatarg(1, argc, argv);
    headersize = atom_getfloatarg(2, argc, argv);
    nchannels = atom_getfloatarg(3, argc, argv);
    bytespersample = atom_getfloatarg(4, argc, argv);
    endian = atom_getsymbolarg(5, argc, argv);
    if (!*filesym->s_name)
        return; /* no filename */

    pthread_mutex_lock(&x->x_mutex);
    soundfile_clear(&x->x_sf);
    x->x_requestcode = REQUEST_OPEN;
    x->x_filename = filesym->s_name;
    x->x_fifotail = 0;
    x->x_fifohead = 0;
    if (*endian->s_name == 'b')
         x->x_sf.sf_bigendian = 1;
    else if (*endian->s_name == 'l')
         x->x_sf.sf_bigendian = 0;
    else if (*endian->s_name)
        pd_error(x, "readsf~ open: endianness neither 'b' nor 'l'");
    else x->x_sf.sf_bigendian = sys_isbigendian();
    x->x_onsetframes = (onsetframes > 0 ? onsetframes : 0);
    x->x_sf.sf_headersize = (headersize > 0 ? headersize :
        (headersize == 0 ? -1 : 0));
    x->x_sf.sf_nchannels = (nchannels >= 1 ? nchannels : 1);
    x->x_sf.sf_bytespersample = (bytespersample > 2 ? bytespersample : 2);
    x->x_sf.sf_bytesperframe = x->x_sf.sf_nchannels * x->x_sf.sf_bytespersample;
    if (type && x->x_sf.sf_headersize >= 0)
    {
        post("'-%s' overridden by headersize", type->t_name);
        x->x_sf.sf_type = NULL;
    }
    else
        x->x_sf.sf_type = type;
    x->x_eof = 0;
    x->x_fileerror = 0;
    x->x_state = STATE_STARTUP;
    sfread_cond_signal(&x->x_requestcondition);
    pthread_mutex_unlock(&x->x_mutex);
    return;
usage:
    pd_error(x, "usage: open [flags] filename [onset] [headersize]...");
    pd_error(0, "[nchannels] [bytespersample] [endian (b or l)]");
    post("flags: %s", sf_typeargs);
}

static void readsf_dsp(t_readsf *x, t_signal **sp)
{
    int i, noutlets = x->x_noutlets;
    pthread_mutex_lock(&x->x_mutex);
    x->x_vecsize = sp[0]->s_n;
    x->x_sigperiod = x->x_fifosize / (x->x_sf.sf_bytesperframe * x->x_vecsize);
    for (i = 0; i < noutlets; i++)
        x->x_outvec[i] = sp[i]->s_vec;
    pthread_mutex_unlock(&x->x_mutex);
    dsp_add(readsf_perform, 1, x);
}

static void readsf_print(t_readsf *x)
{
    post("state %d", x->x_state);
    post("fifo head %d", x->x_fifohead);
    post("fifo tail %d", x->x_fifotail);
    post("fifo size %d", x->x_fifosize);
    post("fd %d", x->x_sf.sf_fd);
    post("eof %d", x->x_eof);
}

    /** request QUIT and wait for acknowledge */
static void readsf_free(t_readsf *x)
{
    void *threadrtn;
    pthread_mutex_lock(&x->x_mutex);
    x->x_requestcode = REQUEST_QUIT;
    sfread_cond_signal(&x->x_requestcondition);
    while (x->x_requestcode != REQUEST_NOTHING)
    {
        sfread_cond_signal(&x->x_requestcondition);
        sfread_cond_wait(&x->x_answercondition, &x->x_mutex);
    }
    pthread_mutex_unlock(&x->x_mutex);
    if (pthread_join(x->x_childthread, &threadrtn))
        pd_error(x, "readsf_free: join failed");

    pthread_cond_destroy(&x->x_requestcondition);
    pthread_cond_destroy(&x->x_answercondition);
    pthread_mutex_destroy(&x->x_mutex);
    freebytes(x->x_buf, x->x_bufsize);
    clock_free(x->x_clock);
}

static void readsf_setup(void)
{
    readsf_class = class_new(gensym("readsf~"),
        (t_newmethod)readsf_new, (t_method)readsf_free,
        sizeof(t_readsf), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addfloat(readsf_class, (t_method)readsf_float);
    class_addmethod(readsf_class, (t_method)readsf_start, gensym("start"), 0);
    class_addmethod(readsf_class, (t_method)readsf_stop, gensym("stop"), 0);
    class_addmethod(readsf_class, (t_method)readsf_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(readsf_class, (t_method)readsf_open,
        gensym("open"), A_GIMME, 0);
    class_addmethod(readsf_class, (t_method)readsf_print, gensym("print"), 0);
}

/* ------------------------- writesf ------------------------- */

static t_class *writesf_class;

typedef t_readsf t_writesf; /* just re-use the structure */

/* ----- the child thread which performs file I/O ----- */

static void *writesf_child_main(void *zz)
{
    t_writesf *x = zz;
    t_soundfile sf = {0};
    soundfile_clear(&sf);
#ifdef PDINSTANCE
    pd_this = x->x_pd_this;
#endif
#ifdef DEBUG_SOUNDFILE_THREADS
    fprintf(stderr, "writesf~: 1\n");
#endif
    pthread_mutex_lock(&x->x_mutex);
    while (1)
    {
#ifdef DEBUG_SOUNDFILE_THREADS
        fprintf(stderr, "writesf~: 0\n");
#endif
        if (x->x_requestcode == REQUEST_NOTHING)
        {
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "writesf~: wait 2\n");
#endif
            sfread_cond_signal(&x->x_answercondition);
            sfread_cond_wait(&x->x_requestcondition, &x->x_mutex);
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "writesf~: 3\n");
#endif
        }
        else if (x->x_requestcode == REQUEST_OPEN)
        {
            ssize_t byteswritten;
            size_t writebytes;

                /* copy file stuff out of the data structure so we can
                relinquish the mutex while we're in open_soundfile_via_path() */
            const char *filename = x->x_filename;
            t_canvas *canvas = x->x_canvas;
            soundfile_copy(&sf, &x->x_sf);

                /* alter the request code so that an ensuing "open" will get
                noticed. */
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "writesf~: 4\n");
#endif
            x->x_requestcode = REQUEST_BUSY;
            x->x_fileerror = 0;

                /* if there's already a file open, close it.  This
                should never happen since writesf_open() calls stop if
                needed and then waits until we're idle. */
            if (sf.sf_fd >= 0)
            {
                size_t frameswritten = x->x_frameswritten;

                pthread_mutex_unlock(&x->x_mutex);
                soundfile_finishwrite(x, filename, &sf,
                    SFMAXFRAMES, frameswritten);
                sys_close(sf.sf_fd);
                sf.sf_fd = -1;
                pthread_mutex_lock(&x->x_mutex);
                x->x_sf.sf_fd = -1;
#ifdef DEBUG_SOUNDFILE_THREADS
                fprintf(stderr, "writesf~: bug? ditched %ld\n", frameswritten);
#endif
                if (x->x_requestcode != REQUEST_BUSY)
                    continue;
            }
                /* cache sf *after* closing as x->sf's type
                    may have changed in writesf_open() */
            soundfile_copy(&sf, &x->x_sf);

                /* open the soundfile with the mutex unlocked */
            pthread_mutex_unlock(&x->x_mutex);
            create_soundfile(canvas, filename, &sf, 0);
            pthread_mutex_lock(&x->x_mutex);

#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "writesf~: 5\n");
#endif

            if (sf.sf_fd < 0)
            {
                x->x_sf.sf_fd = -1;
                x->x_eof = 1;
                x->x_fileerror = errno;
#ifdef DEBUG_SOUNDFILE_THREADS
                fprintf(stderr, "writesf~: open failed %s\n", filename);
#endif
                goto bail;
            }
                /* check if another request has been made; if so, field it */
            if (x->x_requestcode != REQUEST_BUSY)
                continue;
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "writesf~: 6\n");
#endif
                /* copy back into the instance structure. */
            soundfile_copy(&x->x_sf, &sf);
            x->x_fifotail = 0;
            x->x_frameswritten = 0;
                /* in a loop, wait for the fifo to have data and write it
                    to disk */
            while (x->x_requestcode == REQUEST_BUSY ||
                (x->x_requestcode == REQUEST_CLOSE &&
                    x->x_fifohead != x->x_fifotail))
            {
                int fifosize = x->x_fifosize, fifotail;
                char *buf = x->x_buf;
#ifdef DEBUG_SOUNDFILE_THREADS
                fprintf(stderr, "writesf~: 77\n");
#endif
                    /* if the head is < the tail, we can immediately write
                    from tail to end of fifo to disk; otherwise we hold off
                    writing until there are at least WRITESIZE bytes in the
                    buffer */
                if (x->x_fifohead < x->x_fifotail ||
                    x->x_fifohead >= x->x_fifotail + WRITESIZE
                    || (x->x_requestcode == REQUEST_CLOSE &&
                        x->x_fifohead != x->x_fifotail))
                {
                    writebytes = (x->x_fifohead < x->x_fifotail ?
                        fifosize : x->x_fifohead) - x->x_fifotail;
                    if (writebytes > READSIZE)
                        writebytes = READSIZE;
                }
                else
                {
#ifdef DEBUG_SOUNDFILE_THREADS
                    fprintf(stderr, "writesf~: wait 7a...\n");
#endif
                    sfread_cond_signal(&x->x_answercondition);
#ifdef DEBUG_SOUNDFILE_THREADS
                    fprintf(stderr, "writesf~: signaled...\n");
#endif
                    sfread_cond_wait(&x->x_requestcondition,
                        &x->x_mutex);
#ifdef DEBUG_SOUNDFILE_THREADS
                    fprintf(stderr, "writesf~: 7a ... done\n");
#endif
                    continue;
                }
#ifdef DEBUG_SOUNDFILE_THREADS
                fprintf(stderr, "writesf~: 8\n");
#endif
                fifotail = x->x_fifotail;
                soundfile_copy(&sf, &x->x_sf);
                pthread_mutex_unlock(&x->x_mutex);
                byteswritten = write(sf.sf_fd, buf + fifotail, writebytes);
                pthread_mutex_lock(&x->x_mutex);
                if (x->x_requestcode != REQUEST_BUSY &&
                    x->x_requestcode != REQUEST_CLOSE)
                        break;
                if (byteswritten < 0 || (size_t)byteswritten < writebytes)
                {
#ifdef DEBUG_SOUNDFILE_THREADS
                    fprintf(stderr, "writesf~: fileerror %d\n", errno);
#endif
                    x->x_fileerror = errno;
                    goto bail;
                }
                else
                {
                    x->x_fifotail += byteswritten;
                    if (x->x_fifotail == fifosize)
                        x->x_fifotail = 0;
                }
                x->x_frameswritten += byteswritten / sf.sf_bytesperframe;
#ifdef DEBUG_SOUNDFILE_THREADS
                fprintf(stderr, "writesf~: after head %d tail %d written %ld\n",
                    x->x_fifohead, x->x_fifotail, x->x_frameswritten);
#endif
                    /* signal parent in case it's waiting for data */
                sfread_cond_signal(&x->x_answercondition);
                continue;

             bail:
                 if (x->x_requestcode == REQUEST_BUSY)
                     x->x_requestcode = REQUEST_NOTHING;
                     /* hit an error; close file if necessary,
                     set EOF and signal once more */
                 if (sf.sf_fd >= 0)
                 {
                     pthread_mutex_unlock(&x->x_mutex);
                     sys_close(sf.sf_fd);
                     sf.sf_fd = -1;
                     pthread_mutex_lock(&x->x_mutex);
                     x->x_eof = 1;
                     x->x_sf.sf_fd = -1;
                 }
                 sfread_cond_signal(&x->x_answercondition);
            }
        }
        else if (x->x_requestcode == REQUEST_CLOSE ||
            x->x_requestcode == REQUEST_QUIT)
        {
            int quit = (x->x_requestcode == REQUEST_QUIT);
            if (sf.sf_fd >= 0)
            {
                const char *filename = x->x_filename;
                size_t frameswritten = x->x_frameswritten;
                soundfile_copy(&sf, &x->x_sf);
                pthread_mutex_unlock(&x->x_mutex);
                soundfile_finishwrite(x, filename, &sf,
                    SFMAXFRAMES, frameswritten);
                sys_close(sf.sf_fd);
                sf.sf_fd = -1;
                pthread_mutex_lock(&x->x_mutex);
                x->x_sf.sf_fd = -1;
            }
            x->x_requestcode = REQUEST_NOTHING;
            sfread_cond_signal(&x->x_answercondition);
            if (quit)
                break;
        }
        else
        {
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "writesf~: 13\n");
#endif
        }
    }
#ifdef DEBUG_SOUNDFILE_THREADS
    fprintf(stderr, "writesf~: thread exit\n");
#endif
    pthread_mutex_unlock(&x->x_mutex);
    return 0;
}

/* ----- the object proper runs in the calling (parent) thread ----- */

static void writesf_tick(t_writesf *x);

static void *writesf_new(t_floatarg fnchannels, t_floatarg fbufsize)
{
    t_writesf *x;
    int nchannels = fnchannels, bufsize = fbufsize, i;
    char *buf;

    if (nchannels < 1)
        nchannels = 1;
    else if (nchannels > MAXSFCHANS)
        nchannels = MAXSFCHANS;
    if (bufsize <= 0) bufsize = DEFBUFPERCHAN * nchannels;
    else if (bufsize < MINBUFSIZE)
        bufsize = MINBUFSIZE;
    else if (bufsize > MAXBUFSIZE)
        bufsize = MAXBUFSIZE;
    buf = getbytes(bufsize);
    if (!buf) return 0;

    x = (t_writesf *)pd_new(writesf_class);

    for (i = 1; i < nchannels; i++)
        inlet_new(&x->x_obj,  &x->x_obj.ob_pd, &s_signal, &s_signal);

    x->x_f = 0;
    pthread_mutex_init(&x->x_mutex, 0);
    pthread_cond_init(&x->x_requestcondition, 0);
    pthread_cond_init(&x->x_answercondition, 0);
    x->x_vecsize = MAXVECSIZE;
    x->x_insamplerate = 0;
    x->x_state = STATE_IDLE;
    x->x_clock = 0;     /* no callback needed here */
    x->x_canvas = canvas_getcurrent();
    soundfile_clear(&x->x_sf);
    x->x_sf.sf_nchannels = nchannels;
    x->x_sf.sf_bytespersample = 2;
    x->x_sf.sf_bytesperframe = nchannels * 2;
    x->x_buf = buf;
    x->x_bufsize = bufsize;
    x->x_fifosize = x->x_fifohead = x->x_fifotail = x->x_requestcode = 0;
#ifdef PDINSTANCE
    x->x_pd_this = pd_this;
#endif
    pthread_create(&x->x_childthread, 0, writesf_child_main, x);
    return x;
}

static t_int *writesf_perform(t_int *w)
{
    t_writesf *x = (t_writesf *)(w[1]);
    if (x->x_state == STATE_STREAM)
    {
        size_t roominfifo;
        size_t wantbytes;
        int vecsize = x->x_vecsize;
        t_soundfile sf = {0};
        pthread_mutex_lock(&x->x_mutex);
            /* copy with mutex locked! */
        soundfile_copy(&sf, &x->x_sf);
        wantbytes = vecsize * sf.sf_bytesperframe;
        roominfifo = x->x_fifotail - x->x_fifohead;
        if (roominfifo <= 0)
            roominfifo += x->x_fifosize;
        while (!x->x_eof && roominfifo < wantbytes + 1)
        {
            fprintf(stderr, "writesf waiting for disk write..\n");
            fprintf(stderr, "(head %d, tail %d, room %d, want %ld)\n",
                (int)x->x_fifohead, (int)x->x_fifotail,
                (int)roominfifo, (long)wantbytes);
            sfread_cond_signal(&x->x_requestcondition);
            sfread_cond_wait(&x->x_answercondition, &x->x_mutex);
            fprintf(stderr, "... done waiting.\n");
            roominfifo = x->x_fifotail - x->x_fifohead;
            if (roominfifo <= 0)
                roominfifo += x->x_fifosize;
        }
        if (x->x_eof)
        {
            if (x->x_fileerror)
                object_sferror(x, "writesf~", x->x_filename,
                    x->x_fileerror, &x->x_sf);
            x->x_state = STATE_IDLE;
            sfread_cond_signal(&x->x_requestcondition);
            pthread_mutex_unlock(&x->x_mutex);
            return w + 2;
        }

        soundfile_xferout_sample(&sf, x->x_outvec,
            (unsigned char *)(x->x_buf + x->x_fifohead), vecsize, 0, 1.);

        x->x_fifohead += wantbytes;
        if (x->x_fifohead >= x->x_fifosize)
            x->x_fifohead = 0;
        if ((--x->x_sigcountdown) <= 0)
        {
#ifdef DEBUG_SOUNDFILE_THREADS
            fprintf(stderr, "writesf~: signal 1\n");
#endif
            sfread_cond_signal(&x->x_requestcondition);
            x->x_sigcountdown = x->x_sigperiod;
        }
        pthread_mutex_unlock(&x->x_mutex);
    }
    return w + 2;
}

    /** start making output.  If we're in the "startup" state change
        to the "running" state. */
static void writesf_start(t_writesf *x)
{
    if (x->x_state == STATE_STARTUP)
        x->x_state = STATE_STREAM;
    else
        pd_error(x, "writesf~: start requested with no prior 'open'");
}

    /** LATER rethink whether you need the mutex just to set a variable? */
static void writesf_stop(t_writesf *x)
{
    pthread_mutex_lock(&x->x_mutex);
    x->x_state = STATE_IDLE;
    x->x_requestcode = REQUEST_CLOSE;
#ifdef DEBUG_SOUNDFILE_THREADS
    fprintf(stderr, "writesf~: signal 2\n");
#endif
    sfread_cond_signal(&x->x_requestcondition);
    pthread_mutex_unlock(&x->x_mutex);
}

    /** open method.  Called as: open [flags] filename with args as in
        soundfiler_parsewriteargs(). */
static void writesf_open(t_writesf *x, t_symbol *s, int argc, t_atom *argv)
{
    t_soundfiler_writeargs wa = {0};
    if (x->x_state != STATE_IDLE)
        writesf_stop(x);
    if (soundfiler_parsewriteargs(x, &argc, &argv, &wa) || wa.wa_ascii)
    {
        pd_error(x, "usage: open [flags] filename...");
        post("flags: -bytes <n> %s -big -little -rate <n>", sf_typeargs);
        return;
    }
    if (wa.wa_normalize || wa.wa_onsetframes || (wa.wa_nframes != SFMAXFRAMES))
        pd_error(x, "writesf~ open: normalize/onset/nframes argument ignored");
    if (argc)
        pd_error(x, "writesf~ open: extra argument(s) ignored");
    pthread_mutex_lock(&x->x_mutex);
        /* make sure that the child thread has finished writing */
    while (x->x_requestcode != REQUEST_NOTHING)
    {
        sfread_cond_signal(&x->x_requestcondition);
        sfread_cond_wait(&x->x_answercondition, &x->x_mutex);
    }
    x->x_filename = wa.wa_filesym->s_name;
    x->x_sf.sf_type = wa.wa_type;
    if (wa.wa_samplerate > 0)
        x->x_sf.sf_samplerate = wa.wa_samplerate;
    else if (x->x_insamplerate > 0)
        x->x_sf.sf_samplerate = x->x_insamplerate;
    else x->x_sf.sf_samplerate = sys_getsr();
    x->x_sf.sf_bytespersample =
        (wa.wa_bytespersample > 2 ? wa.wa_bytespersample : 2);
    x->x_sf.sf_bigendian = wa.wa_bigendian;
    x->x_sf.sf_bytesperframe = x->x_sf.sf_nchannels * x->x_sf.sf_bytespersample;
    x->x_frameswritten = 0;
    x->x_requestcode = REQUEST_OPEN;
    x->x_fifotail = 0;
    x->x_fifohead = 0;
    x->x_eof = 0;
    x->x_fileerror = 0;
    x->x_state = STATE_STARTUP;
        /* set fifosize from bufsize.  fifosize must be a
        multiple of the number of bytes eaten for each DSP
        tick.  */
    x->x_fifosize = x->x_bufsize - (x->x_bufsize %
        (x->x_sf.sf_bytesperframe * MAXVECSIZE));
        /* arrange for the "request" condition to be signaled 16
            times per buffer */
    x->x_sigcountdown = x->x_sigperiod = (x->x_fifosize /
            (16 * (x->x_sf.sf_bytesperframe * x->x_vecsize)));
    sfread_cond_signal(&x->x_requestcondition);
    pthread_mutex_unlock(&x->x_mutex);
}

static void writesf_dsp(t_writesf *x, t_signal **sp)
{
    int i, ninlets = x->x_sf.sf_nchannels;
    pthread_mutex_lock(&x->x_mutex);
    x->x_vecsize = sp[0]->s_n;
    x->x_sigperiod = (x->x_fifosize /
            (16 * x->x_sf.sf_bytesperframe * x->x_vecsize));
    for (i = 0; i < ninlets; i++)
        x->x_outvec[i] = sp[i]->s_vec;
    x->x_insamplerate = sp[0]->s_sr;
    pthread_mutex_unlock(&x->x_mutex);
    dsp_add(writesf_perform, 1, x);
}

static void writesf_print(t_writesf *x)
{
    post("state %d", x->x_state);
    post("fifo head %d", x->x_fifohead);
    post("fifo tail %d", x->x_fifotail);
    post("fifo size %d", x->x_fifosize);
    post("fd %d", x->x_sf.sf_fd);
    post("eof %d", x->x_eof);
}

    /** request QUIT and wait for acknowledge */
static void writesf_free(t_writesf *x)
{
    void *threadrtn;
    pthread_mutex_lock(&x->x_mutex);
    x->x_requestcode = REQUEST_QUIT;
#ifdef DEBUG_SOUNDFILE_THREADS
    fprintf(stderr, "writesf~: stopping thread...\n");
#endif
    sfread_cond_signal(&x->x_requestcondition);
    while (x->x_requestcode != REQUEST_NOTHING)
    {
#ifdef DEBUG_SOUNDFILE_THREADS
        fprintf(stderr, "writesf~: signaling...\n");
#endif
        sfread_cond_signal(&x->x_requestcondition);
        sfread_cond_wait(&x->x_answercondition, &x->x_mutex);
    }
    pthread_mutex_unlock(&x->x_mutex);
    if (pthread_join(x->x_childthread, &threadrtn))
        pd_error(x, "writesf_free: join failed");
#ifdef DEBUG_SOUNDFILE_THREADS
    fprintf(stderr, "writesf~: ... done\n");
#endif

    pthread_cond_destroy(&x->x_requestcondition);
    pthread_cond_destroy(&x->x_answercondition);
    pthread_mutex_destroy(&x->x_mutex);
    freebytes(x->x_buf, x->x_bufsize);
}

static void writesf_setup(void)
{
    writesf_class = class_new(gensym("writesf~"),
        (t_newmethod)writesf_new, (t_method)writesf_free,
        sizeof(t_writesf), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(writesf_class, (t_method)writesf_start, gensym("start"), 0);
    class_addmethod(writesf_class, (t_method)writesf_stop, gensym("stop"), 0);
    class_addmethod(writesf_class, (t_method)writesf_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(writesf_class, (t_method)writesf_open,
        gensym("open"), A_GIMME, 0);
    class_addmethod(writesf_class, (t_method)writesf_print, gensym("print"), 0);
    CLASS_MAINSIGNALIN(writesf_class, t_writesf, x_f);
}

/* ------------------------- global setup routine ------------------------ */

void d_soundfile_setup(void)
{
    soundfile_type_setup();
    soundfiler_setup();
    readsf_setup();
    writesf_setup();
}
