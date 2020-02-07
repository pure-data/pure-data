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
#include <errno.h>
#include <math.h>

/* Supported sample formats: linear PCM, 16 or 24 bit int, 32 bit float */

/* TODO: add support for 32 bit int samples? */

#define MAXSFCHANS 64

/* GLIBC */
#ifdef _LARGEFILE64_SOURCE
#define open open64
#endif

/* Microsoft Visual Studio does not define these... arg */
#ifdef _MSC_VER
#define O_CREAT   _O_CREAT
#define O_TRUNC   _O_TRUNC
#define O_WRONLY  _O_WRONLY
#endif

#define SCALE (1./(1024. * 1024. * 1024. * 2.))

typedef union _samplelong {
  t_sample f;
  long     l;
} t_sampleuint;

    /* supported file types */
typedef enum _soundfile_filetype
{
    FILETYPE_UNKNOWN = -1,
    FILETYPE_WAVE    =  0,
    FILETYPE_AIFF    =  1,
    FILETYPE_NEXT    =  2
} t_soundfile_filetype;

/* ----- soundfile helpers ----- */

    /** returns min buffer size between the various soundfile types
        TODO: this could probably be a define, but is designed to be
              dynamic for future changes */
static int soundfile_min_headersize()
{
    int size = soundfile_wave_headersize();
    if (size < soundfile_aiff_headersize())
        size = soundfile_aiff_headersize();
    if (size < soundfile_next_headersize())
        size = soundfile_next_headersize();
    return size;
}

/* ----- soundfile info ----- */

void soundfile_info_clear(t_soundfile_info *info)
{
    info->i_samplerate = 0;
    info->i_nchannels = 0;
    info->i_bytespersample = 0;
    info->i_headersize = 0;
    info->i_bigendian = 0;
    info->i_bytelimit = SFMAXBYTES;
    info->i_bytesperframe = 0;
}

void soundfile_info_copy(t_soundfile_info *dst, const t_soundfile_info *src)
{
    dst->i_samplerate = src->i_samplerate;
    dst->i_nchannels = src->i_nchannels;
    dst->i_bytespersample = src->i_bytespersample;
    dst->i_headersize = src->i_headersize;
    dst->i_bigendian = src->i_bigendian;
    dst->i_bytelimit = src->i_bytelimit;
    dst->i_bytesperframe = src->i_bytesperframe;
}

void soundfile_info_print(const t_soundfile_info *info)
{
    printf("%d %d %d %ld %s %ld %d\n", info->i_samplerate, info->i_nchannels,
        info->i_bytespersample, info->i_headersize,
        (info->i_bigendian ? "b" : "l"), info->i_bytelimit,
        info->i_bytesperframe);
}

int soundfile_info_swap(const t_soundfile_info *src)
{
    return src->i_bigendian != sys_isbigendian();
}

static void outlet_soundfile_info(t_outlet *out, t_soundfile_info *info)
{
    t_atom info_list[5];
    SETFLOAT((t_atom *)info_list, (t_float)info->i_samplerate);
    SETFLOAT((t_atom *)info_list+1,
        (t_float)(info->i_headersize < 0 ? 0 : info->i_headersize));
    SETFLOAT((t_atom *)info_list+2, (t_float)info->i_nchannels);
    SETFLOAT((t_atom *)info_list+3, (t_float)info->i_bytespersample);
    SETSYMBOL((t_atom *)info_list+4, gensym((info->i_bigendian ? "b" : "l")));
    outlet_list(out, &s_list, 5, (t_atom *)info_list);
}

/* ----- read write ----- */

ssize_t soundfile_readbytes(int fd, off_t offset, char *dst, size_t size) {
    off_t seekout = lseek(fd, offset, SEEK_SET);
    if (seekout != offset)
        return -1;
    return read(fd, dst, size);
}

ssize_t soundfile_writebytes(int fd, off_t offset, const char *src, size_t size) {
    off_t seekout = lseek(fd, offset, SEEK_SET);
    if (seekout != offset)
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

uint32_t swap4(uint32_t n, int doit)
{
    if (doit)
        return (((n & 0xff) << 24) | ((n & 0xff00) << 8) |
            ((n & 0xff0000) >> 8) | ((n & 0xff000000) >> 24));
    else return n;
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

uint16_t swap2(uint32_t n, int doit)
{
    if (doit)
        return (((n & 0xff) << 8) | ((n & 0xff00) >> 8));
    else return n;
}

void swapstring(char *foo, int doit)
{
    if (doit)
    {
        char a = foo[0], b = foo[1], c = foo[2], d = foo[3];
        foo[0] = d; foo[1] = c; foo[2] = b; foo[3] = a;
    }
}

/* ----------------------- soundfile access routines ----------------------- */

    /** This routine opens a file, looks for either a supported file format
        header, seeks to end of it, and fills in the soundfile header info
        values. Only 2- and 3-byte fixed-point samples and 4-byte floating point
        samples are supported.  If info->i_headersize is nonzero, the caller
        should supply the number of channels, endinanness, and bytes per sample;
        the header is ignored.  Otherwise, the routine tries to read the header
        and fill in the properties. */
int open_soundfile_via_fd(int fd, t_soundfile_info *info, size_t skipframes)
{
    t_soundfile_info i;
    off_t offset, seekout;
    errno = 0;
    if (info->i_headersize >= 0) /* header detection overridden */
        soundfile_info_copy(&i, info);
    else
    {
        char buf[SFHDRBUFSIZE];
        ssize_t bytesread = read(fd, buf, soundfile_min_headersize());
        t_soundfile_filetype filetype = FILETYPE_UNKNOWN;

            /* check header for filetype */
        if (soundfile_wave_isheader(buf, bytesread))
            filetype = FILETYPE_WAVE;
        else if (soundfile_aiff_isheader(buf, bytesread))
            filetype = FILETYPE_AIFF;
        else if (soundfile_next_isheader(buf, bytesread))
            filetype = FILETYPE_NEXT;
        else goto badheader;

            /* rewind and read header */
        if (lseek(fd, 0, SEEK_SET) < 0)
            return -1;
        soundfile_info_clear(&i);
        switch (filetype)
        {
            case FILETYPE_WAVE:
                if (!soundfile_wave_readheader(fd, &i)) goto badheader;
                break;
            case FILETYPE_AIFF:
                if (!soundfile_aiff_readheader(fd, &i)) goto badheader;
                break;
            case FILETYPE_NEXT:
                if (!soundfile_next_readheader(fd, &i)) goto badheader;
                break;
            default: goto badheader;
        }
    }

        /* seek past header and any sample frames to skip */
    offset = i.i_headersize + (i.i_bytesperframe * skipframes);
    seekout = lseek(fd, offset, 0);
    if (seekout != offset)
        return -1;
    i.i_bytelimit -= i.i_bytesperframe * skipframes;
    if (i.i_bytelimit < 0)
        i.i_bytelimit = 0;

        /* copy sample format back to caller */
    soundfile_info_copy(info, &i);
    return fd;

badheader:
        /* the header wasn't recognized.  We're threadable here so let's not
        print out the error... */
    errno = EIO;
    return -1;
}

    /** open a soundfile, using open_via_path().  This is used by readsf~ in
        a not-perfectly-threadsafe way.  LATER replace with a thread-hardened
        version of open_soundfile_via_canvas() */
int open_soundfile(const char *dirname, const char *filename,
    t_soundfile_info *info, size_t skipframes)
{
    char buf[SFHDRBUFSIZE], *bufptr;
    int fd, sf_fd;
    fd = open_via_path(dirname, filename, "", buf, &bufptr, SFHDRBUFSIZE, 1);
    if (fd < 0)
        return -1;
    sf_fd = open_soundfile_via_fd(fd, info, skipframes);
    if (sf_fd < 0)
        sys_close(fd);
    return sf_fd;
}

    /** open a soundfile, using open_via_canvas().  This is used by readsf~ in
        a not-perfectly-threadsafe way.  LATER replace with a thread-hardened
        version of open_soundfile_via_canvas() */
int open_soundfile_via_canvas(t_canvas *canvas, const char *filename,
    t_soundfile_info *info, size_t skipframes)
{
    char buf[SFHDRBUFSIZE], *bufptr;
    int fd, sf_fd;
    fd = canvas_open(canvas, filename, "", buf, &bufptr, SFHDRBUFSIZE, 1);
    if (fd < 0)
        return -1;
    sf_fd = open_soundfile_via_fd(fd, info, skipframes);
    if (sf_fd < 0)
        sys_close(fd);
    return sf_fd;
}

static void soundfile_xferin_sample(const t_soundfile_info *info, int nvecs,
    t_sample **vecs, size_t framesread, unsigned char *buf, size_t nframes)
{
    int nchannels = (info->i_nchannels < nvecs ? info->i_nchannels : nvecs), i;
    size_t j;
    unsigned char *sp, *sp2;
    t_sample *fp;
    for (i = 0, sp = buf; i < nchannels; i++, sp += info->i_bytespersample)
    {
        if (info->i_bytespersample == 2)
        {
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
                        *fp = SCALE * ((sp2[0] << 24) | (sp2[1] << 16));
            }
            else
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
                        *fp = SCALE * ((sp2[1] << 24) | (sp2[0] << 16));
            }
        }
        else if (info->i_bytespersample == 3)
        {
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
                        *fp = SCALE * ((sp2[0] << 24) | (sp2[1] << 16)
                            | (sp2[2] << 8));
            }
            else
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
                        *fp = SCALE * ((sp2[2] << 24) | (sp2[1] << 16)
                            | (sp2[0] << 8));
            }
        }
        else if (info->i_bytespersample == 4)
        {
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
                        *(long *)fp = ((sp2[0] << 24) | (sp2[1] << 16)
                            | (sp2[2] << 8) | sp2[3]);
            }
            else
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
                        *(long *)fp = ((sp2[3] << 24) | (sp2[2] << 16)
                            | (sp2[1] << 8) | sp2[0]);
            }
        }
    }
        /* zero out other outputs */
    for (i = info->i_nchannels; i < nvecs; i++)
        for (j = nframes, fp = vecs[i]; j--;)
            *fp++ = 0;

}

static void soundfile_xferin_words(const t_soundfile_info *info, int nvecs,
    t_word **vecs, size_t framesread, unsigned char *buf, size_t nframes)
{
    unsigned char *sp, *sp2;
    t_word *wp;
    int nchannels = (info->i_nchannels < nvecs ? info->i_nchannels : nvecs), i;
    size_t j;
    union
    {
        uint32_t long32;
        float float32;
    } word32;
    for (i = 0, sp = buf; i < nchannels; i++, sp += info->i_bytespersample)
    {
        if (info->i_bytespersample == 2)
        {
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
                        wp->w_float = SCALE * ((sp2[0] << 24) | (sp2[1] << 16));
            }
            else
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
                        wp->w_float = SCALE * ((sp2[1] << 24) | (sp2[0] << 16));
            }
        }
        else if (info->i_bytespersample == 3)
        {
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
                        wp->w_float = SCALE * ((sp2[0] << 24) | (sp2[1] << 16)
                            | (sp2[2] << 8));
            }
            else
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
                        wp->w_float = SCALE * ((sp2[2] << 24) | (sp2[1] << 16)
                            | (sp2[0] << 8));
            }
        }
        else if (info->i_bytespersample == 4)
        {
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
                {
                    word32.long32 = (sp2[0] << 24) |
                            (sp2[1] << 16) | (sp2[2] << 8) | sp2[3];
                    wp->w_float = word32.float32;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + framesread;
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
                {
                    word32.long32 = (sp2[3] << 24) |
                        (sp2[2] << 16) | (sp2[1] << 8) | sp2[0];
                    wp->w_float = word32.float32;
                }
            }
        }
    }
        /* zero out other outputs */
    for (i = info->i_nchannels; i < nvecs; i++)
        for (j = nframes, wp = vecs[i]; j--;)
            (wp++)->w_float = 0;

}

    /* soundfiler_write ...

       usage: write [flags] filename table ...
       flags:
         -nframes <frames>
         -skip <frames>
         -bytes <bytes per sample>
         -r / -rate <samplerate>
         -normalize
         -wave
         -aiff
         -nextstep
         -big
         -little
    */

typedef struct _soundfiler_writeargs
{
    t_symbol *wa_filesym;             /* file path symbol */
    t_soundfile_filetype wa_filetype; /* file type */
    int wa_samplerate;                /* sample rate */
    int wa_bytespersample;            /* number of bytes per sample */
    int wa_bigendian;                 /* is sample data bigendian? */
    size_t wa_nframes;                /* number of sample frames to write */
    size_t wa_onsetframes;            /* sample frame onset when writing */
    int wa_normalize;                 /* normalize samples? */
} t_soundfiler_writeargs;

/* the routine which actually does the work should LATER also be called
from garray_write16. */

    /** Parse arguments for writing.  The "obj" argument is only for flagging
        errors.  For streaming to a file the "normalize", "onset" and "nframes"
        arguments shouldn't be set but the calling routine flags this. */
static int soundfiler_writeargs_parse(void *obj, int *p_argc, t_atom **p_argv,
    t_soundfiler_writeargs *wa)
{
    int argc = *p_argc;
    t_atom *argv = *p_argv;
    int samplerate = -1, bytespersample = 2, bigendian = 0, endianness = -1;
    size_t nframes = SFMAXFRAMES, onsetframes = 0;
    int normalize = 0;
    t_symbol *filesym;
    t_soundfile_filetype filetype = FILETYPE_UNKNOWN;

    while (argc > 0 && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        const char *flag = argv->a_w.w_symbol->s_name + 1;
        if (!strcmp(flag, "skip"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                ((onsetframes = argv[1].a_w.w_float) < 0))
                    goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(flag, "nframes"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                ((nframes = argv[1].a_w.w_float) < 0))
                    goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(flag, "bytes"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                ((bytespersample = argv[1].a_w.w_float) < 2) ||
                    bytespersample > 4)
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(flag, "normalize"))
        {
            normalize = 1;
            argc -= 1; argv += 1;
        }
        else if (!strcmp(flag, "wave"))
        {
            filetype = FILETYPE_WAVE;
            argc -= 1; argv += 1;
        }
        else if (!strcmp(flag, "nextstep"))
        {
            filetype = FILETYPE_NEXT;
            argc -= 1; argv += 1;
        }
        else if (!strcmp(flag, "aiff"))
        {
            filetype = FILETYPE_AIFF;
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
        else if (!strcmp(flag, "r") || !strcmp(flag, "rate"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                ((samplerate = argv[1].a_w.w_float) <= 0))
                    goto usage;
            argc -= 2; argv += 2;
        }
        else goto usage;
    }
    if (!argc || argv->a_type != A_SYMBOL)
        goto usage;
    filesym = argv->a_w.w_symbol;

        /* check if format not specified and fill in */
    if (filetype == FILETYPE_UNKNOWN)
    {
        if (soundfile_wave_hasextension(filesym->s_name, MAXPDSTRING))
            filetype = FILETYPE_WAVE;
        else if(soundfile_aiff_hasextension(filesym->s_name, MAXPDSTRING))
            filetype = FILETYPE_AIFF;
        else if(soundfile_next_hasextension(filesym->s_name, MAXPDSTRING))
            filetype = FILETYPE_NEXT;
        else
            filetype = FILETYPE_WAVE; /* default */
    }

        /* for WAVE force little endian; for nextstep use machine native */
    if (filetype == FILETYPE_WAVE)
    {
        bigendian = 0;
        if (endianness == 1)
            pd_error(obj, "WAVE file forced to little endian");
    }
    else if (filetype == FILETYPE_AIFF)
    {
        bigendian = 1;
        if (endianness == 0)
            bigendian = 0;
    }
    else if (endianness == -1)
    {
        bigendian = sys_isbigendian();
    }
    else bigendian = endianness;

    argc--; argv++;

        /* return to caller */
    *p_argc = argc;
    *p_argv = argv;
    wa->wa_filesym = filesym;
    wa->wa_filetype = filetype;
    wa->wa_samplerate = samplerate;
    wa->wa_bytespersample = bytespersample;
    wa->wa_bigendian = bigendian;
    wa->wa_nframes = nframes;
    wa->wa_onsetframes = onsetframes;
    wa->wa_normalize = normalize;
    return 0;
usage:
    return -1;
}

    /** return fd and sets info->i_headersize on success or -1 on failure */
static int create_soundfile(t_canvas *canvas, const char *filename,
    int filetype, size_t nframes, t_soundfile_info *info)
{
    char filenamebuf[MAXPDSTRING], buf2[MAXPDSTRING];
    ssize_t headersize = -1;
    int fd;

        /* create file */
    strncpy(filenamebuf, filename, MAXPDSTRING);
    switch (filetype)
    {
        case FILETYPE_WAVE:
            if (!soundfile_wave_hasextension(filenamebuf, MAXPDSTRING))
                strcat(filenamebuf, ".wav");
            break;
        case FILETYPE_AIFF:
            if (!soundfile_aiff_hasextension(filenamebuf, MAXPDSTRING))
                strcat(filenamebuf, ".aif");
            break;
        case FILETYPE_NEXT:
            if (!soundfile_next_hasextension(filenamebuf, MAXPDSTRING))
                strcat(filenamebuf, ".snd");
            break;
        default: break;
    }
    filenamebuf[MAXPDSTRING-10] = 0;
    canvas_makefilename(canvas, filenamebuf, buf2, MAXPDSTRING);
    if ((fd = sys_open(buf2, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        return -1;

        /* write header */
    switch (filetype)
    {
        case FILETYPE_WAVE:
            headersize = soundfile_wave_writeheader(fd, info, nframes); break;
        case FILETYPE_AIFF:
            headersize = soundfile_aiff_writeheader(fd, info, nframes); break;
        case FILETYPE_NEXT:
            headersize = soundfile_next_writeheader(fd, info, nframes); break;
        default: break;
    }

        /* cleanup */
    if (headersize < 0)
    {
        close(fd);
        return -1;
    }
    info->i_headersize = headersize;
    return fd;
}

static void soundfile_finishwrite(void *obj, const char *filename, int fd,
    int filetype, size_t nframes, size_t frameswritten,
    const t_soundfile_info *info)
{
    if (frameswritten >= nframes) return;
    if (nframes < SFMAXFRAMES)
        pd_error(obj, "soundfiler_write: %ld out of %ld frames written",
            frameswritten, nframes);
    switch (filetype)
    {
        case FILETYPE_WAVE:
            if (soundfile_wave_updateheader(fd, info, frameswritten)) return;
            break;
        case FILETYPE_AIFF:
            if (soundfile_aiff_updateheader(fd, info, frameswritten)) return;
            break;
        case FILETYPE_NEXT:
            if (soundfile_next_updateheader(fd, info, frameswritten)) return;
            break;
        default: return;
    }
    pd_error(obj, "soundfiler_write: %s: %s", filename, strerror(errno));
}

static void soundfile_xferout_sample(const t_soundfile_info *info,
    t_sample **vecs, unsigned char *buf, size_t nframes, size_t onsetframes,
    t_sample normalfactor)
{
    int i;
    size_t j;
    unsigned char *sp, *sp2;
    t_sample *fp;
    for (i = 0, sp = buf; i < info->i_nchannels; i++,
        sp += info->i_bytespersample)
    {
        if (info->i_bytespersample == 2)
        {
            t_sample ff = normalfactor * 32768.;
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
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
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
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
        else if (info->i_bytespersample == 3)
        {
            t_sample ff = normalfactor * 8388608.;
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
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
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
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
        else if (info->i_bytespersample == 4)
        {
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
                {
                    t_sampleuint f2;
                    f2.f = *fp * normalfactor;
                    sp2[0] = (f2.l >> 24); sp2[1] = (f2.l >> 16);
                    sp2[2] = (f2.l >> 8); sp2[3] = f2.l;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, fp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += info->i_bytesperframe, fp++)
                {
                    t_sampleuint f2;
                    f2.f = *fp * normalfactor;
                    sp2[3] = (f2.l >> 24); sp2[2] = (f2.l >> 16);
                    sp2[1] = (f2.l >> 8); sp2[0] = f2.l;
                }
            }
        }
    }
}

static void soundfile_xferout_words(const t_soundfile_info *info, t_word **vecs,
    unsigned char *buf, size_t nframes, size_t onsetframes,
    t_sample normalfactor)
{
    int i;
    size_t j;
    unsigned char *sp, *sp2;
    t_word *wp;
    for (i = 0, sp = buf; i < info->i_nchannels;
         i++, sp += info->i_bytespersample)
    {
        if (info->i_bytespersample == 2)
        {
            t_sample ff = normalfactor * 32768.;
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
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
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
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
        else if (info->i_bytespersample == 3)
        {
            t_sample ff = normalfactor * 8388608.;
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
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
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
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
        else if (info->i_bytespersample == 4)
        {
            if (info->i_bigendian)
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
                {
                    t_sampleuint f2;
                    f2.f = wp->w_float * normalfactor;
                    sp2[0] = (f2.l >> 24); sp2[1] = (f2.l >> 16);
                    sp2[2] = (f2.l >> 8); sp2[3] = f2.l;
                }
            }
            else
            {
                for (j = 0, sp2 = sp, wp = vecs[i] + onsetframes;
                    j < nframes; j++, sp2 += info->i_bytesperframe, wp++)
                {
                    t_sampleuint f2;
                    f2.f = wp->w_float * normalfactor;
                    sp2[3] = (f2.l >> 24); sp2[2] = (f2.l >> 16);
                    sp2[1] = (f2.l >> 8); sp2[0] = f2.l;
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

static void soundfiler_readascii(t_soundfiler *x, const char *filename,
    int narray, t_garray **garrays, t_word **vecs, int resize, int finalsize)
{
    t_binbuf *b = binbuf_new();
    int n, i, j, nframes, vecsize;
    t_atom *atoms, *ap;
    if (binbuf_read_via_canvas(b, filename, x->x_canvas, 0))
        return;
    n = binbuf_getnatom(b);
    atoms = binbuf_getvec(b);
    nframes = n / narray;
#ifdef DEBUG_SOUNDFILE
    post("read 1 %d", n);
#endif
    if (nframes < 1)
    {
        pd_error(x, "soundfiler_read: %s: empty or very short file", filename);
        return;
    }
    if (resize)
    {
        for (i = 0; i < narray; i++)
        {
            garray_resize_long(garrays[i], nframes);
            garray_getfloatwords(garrays[i], &vecsize, &vecs[i]);
        }
    }
    else if (finalsize < nframes)
        nframes = finalsize;
#ifdef DEBUG_SOUNDFILE
    post("read 2");
#endif
    for (j = 0, ap = atoms; j < nframes; j++)
        for (i = 0; i < narray; i++)
            vecs[i][j].w_float = atom_getfloat(ap++);
        /* zero out remaining elements of vectors */
    for (i = 0; i < narray; i++)
    {
        int vecsize;
        if (garray_getfloatwords(garrays[i], &vecsize, &vecs[i]))
            for (j = nframes; j < vecsize; j++)
                vecs[i][j].w_float = 0;
    }
    for (i = 0; i < narray; i++)
        garray_redraw(garrays[i]);
#ifdef DEBUG_SOUNDFILE
    post("read 3");
#endif
    
}

    /* soundfiler_read ...

       usage: read [flags] filename table ...
       flags:
           -skip <frames> ... frames to skip in file
           -onset <frames> ... onset in table to read into (NOT DONE YET)
           -raw <headersize channels bytes endian>
           -resize
           -maxsize <max-size>
    */

static void soundfiler_read(t_soundfiler *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_soundfile_info info;
    int fd = -1, resize = 0, ascii = 0, i;
    size_t skipframes = 0, finalsize = 0, maxsize = SFMAXFRAMES,
           framesread = 0, nframes, bufframes, j;
    char endianness;
    const char *filename;
    t_garray *garrays[MAXSFCHANS];
    t_word *vecs[MAXSFCHANS];
    char sampbuf[SAMPBUFSIZE];
    FILE *fp;

    soundfile_info_clear(&info);
    info.i_headersize = -1;
    while (argc > 0 && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        const char *flag = argv->a_w.w_symbol->s_name + 1;
        if (!strcmp(flag, "skip"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                ((skipframes = argv[1].a_w.w_float) < 0))
                    goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(flag, "ascii"))
        {
            if (info.i_headersize >= 0)
                post("soundfiler_read: '-raw' overridden by '-ascii'");
            ascii = 1;
            argc--; argv++;
        }
        else if (!strcmp(flag, "raw"))
        {
            if (ascii)
                post("soundfiler_read: '-raw' overridden by '-ascii'");
            if (argc < 5 ||
                argv[1].a_type != A_FLOAT ||
                ((info.i_headersize = argv[1].a_w.w_float) < 0) ||
                argv[2].a_type != A_FLOAT ||
                ((info.i_nchannels = argv[2].a_w.w_float) < 1) ||
                (info.i_nchannels > MAXSFCHANS) ||
                argv[3].a_type != A_FLOAT ||
                ((info.i_bytespersample = argv[3].a_w.w_float) < 2) ||
                    (info.i_bytespersample > 4) ||
                argv[4].a_type != A_SYMBOL ||
                    ((endianness = argv[4].a_w.w_symbol->s_name[0]) != 'b'
                    && endianness != 'l' && endianness != 'n'))
                        goto usage;
            if (endianness == 'b')
                info.i_bigendian = 1;
            else if (endianness == 'l')
                info.i_bigendian = 0;
            else
                info.i_bigendian = sys_isbigendian();
            info.i_samplerate = sys_getsr();
            info.i_bytesperframe = info.i_nchannels * info.i_bytespersample;
            argc -= 5; argv += 5;
        }
        else if (!strcmp(flag, "resize"))
        {
            resize = 1;
            argc -= 1; argv += 1;
        }
        else if (!strcmp(flag, "maxsize"))
        {
            if (argc < 2 || argv[1].a_type != A_FLOAT ||
                ((maxsize = (argv[1].a_w.w_float > SFMAXFRAMES ?
                SFMAXFRAMES : argv[1].a_w.w_float)) < 0))
                    goto usage;
            resize = 1;     /* maxsize implies resize. */
            argc -= 2; argv += 2;
        }
        else goto usage;
    }
    if (argc < 1 ||                           /* no filename or tables */
        argc > MAXSFCHANS + 1 ||              /* too many tables */
        argv[0].a_type != A_SYMBOL)           /* bad filename */
            goto usage;
    filename = argv[0].a_w.w_symbol->s_name;
    argc--; argv++;

    for (i = 0; i < argc; i++)
    {
        int vecsize;
        if (argv[i].a_type != A_SYMBOL)
            goto usage;
        if (!(garrays[i] =
            (t_garray *)pd_findbyclass(argv[i].a_w.w_symbol, garray_class)))
        {
            pd_error(x, "%s: no such table", argv[i].a_w.w_symbol->s_name);
            goto done;
        }
        else if (!garray_getfloatwords(garrays[i], &vecsize,
                &vecs[i]))
            error("%s: bad template for tabwrite",
                argv[i].a_w.w_symbol->s_name);
        if (finalsize && finalsize != vecsize && !resize)
        {
            post("soundfiler_read: arrays have different lengths; resizing...");
            resize = 1;
        }
        finalsize = vecsize;
    }
    if (ascii)
    {
        soundfiler_readascii(x, filename,
            argc, garrays, vecs, resize, finalsize);
        return;
    }
    fd = open_soundfile_via_canvas(x->x_canvas, filename, &info, skipframes);

    if (fd < 0)
    {
        pd_error(x, "soundfiler_read: %s: %s", filename, (errno == EIO ?
            "unknown or bad header format" : strerror(errno)));
        goto done;
    }

    if (resize)
    {
            /* figure out what to resize to */
        off_t poswas, eofis, framesinfile;
        poswas = lseek(fd, 0, SEEK_CUR);
        eofis = lseek(fd, 0, SEEK_END);
        if (poswas < 0 || eofis < 0 || eofis < poswas)
        {
            pd_error(x, "soundfiler_read: lseek failed");
            goto done;
        }
        lseek(fd, poswas, SEEK_SET);
        framesinfile = (eofis - poswas) / info.i_bytesperframe;
        if (framesinfile > maxsize)
        {
            pd_error(x, "soundfiler_read: truncated to %ld elements", maxsize);
            framesinfile = maxsize;
        }
        if (framesinfile > info.i_bytelimit / info.i_bytesperframe)
            framesinfile = info.i_bytelimit / info.i_bytesperframe;
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
                pd_error(x, "resize failed");
                goto done;
            }
        }
    }
    if (!finalsize) finalsize = SFMAXFRAMES;
    if (finalsize > info.i_bytelimit / info.i_bytesperframe)
        finalsize = info.i_bytelimit / info.i_bytesperframe;
    fp = fdopen(fd, "rb");
    bufframes = SAMPBUFSIZE / info.i_bytesperframe;

    for (framesread = 0; framesread < finalsize; )
    {
        size_t thisread = finalsize - framesread;
        thisread = (thisread > bufframes ? bufframes : thisread);
        nframes = fread(sampbuf, info.i_bytesperframe, thisread, fp);
        if (nframes == 0) break;
        soundfile_xferin_words(&info, argc, vecs, framesread,
            (unsigned char *)sampbuf, nframes);
        framesread += nframes;
    }
        /* zero out remaining elements of vectors */
    for (i = 0; i < argc; i++)
    {
        int vecsize;
        if (garray_getfloatwords(garrays[i], &vecsize, &vecs[i]))
            for (j = framesread; j < vecsize; j++)
                vecs[i][j].w_float = 0;
    }
        /* zero out vectors in excess of number of channels */
    for (i = info.i_nchannels; i < argc; i++)
    {
        int vecsize;
        t_word *foo;
        if (garray_getfloatwords(garrays[i], &vecsize, &foo))
            for (j = 0; j < vecsize; j++)
                foo[j].w_float = 0;
    }
        /* do all graphics updates */
    for (i = 0; i < argc; i++)
        garray_redraw(garrays[i]);
    fclose(fp);
    fd = -1;
    goto done;
usage:
    pd_error(x, "usage: read [flags] filename tablename...");
    post("flags: -skip <n> -resize -maxsize <n> ...");
    post("-raw <headerbytes> <channels> <bytespersample> "
         "<endian (b, l, or n)>");
done:
    if (fd >= 0)
        close(fd);
    outlet_soundfile_info(x->x_out2, &info);
    outlet_float(x->x_obj.ob_outlet, (t_float)framesread);
}

    /** this is broken out from soundfiler_write below so garray_write can
        call it too... not done yet though. */
long soundfiler_dowrite(void *obj, t_canvas *canvas,
    int argc, t_atom *argv, t_soundfile_info *info)
{
    t_soundfiler_writeargs wa = {NULL, FILETYPE_UNKNOWN, 0};
    int fd = -1, i;
    size_t bufframes, frameswritten = 0, j;
    t_garray *garrays[MAXSFCHANS];
    t_word *vectors[MAXSFCHANS];
    char sampbuf[SAMPBUFSIZE];
    t_sample normfactor, biggest = 0;

    if (soundfiler_writeargs_parse(obj, &argc, &argv, &wa))
        goto usage;
    info->i_nchannels = argc;
    info->i_samplerate = wa.wa_samplerate;
    info->i_bytespersample = wa.wa_bytespersample;
    info->i_bigendian = wa.wa_bigendian;
    info->i_bytesperframe = argc * wa.wa_bytespersample;
    if (info->i_nchannels < 1 || info->i_nchannels > MAXSFCHANS)
        goto usage;
    if (info->i_samplerate <= 0)
        info->i_samplerate = sys_getsr();
    for (i = 0; i < info->i_nchannels; i++)
    {
        int vecsize;
        if (argv[i].a_type != A_SYMBOL)
            goto usage;
        if (!(garrays[i] =
            (t_garray *)pd_findbyclass(argv[i].a_w.w_symbol, garray_class)))
        {
            pd_error(obj, "%s: no such table", argv[i].a_w.w_symbol->s_name);
            goto fail;
        }
        else if (!garray_getfloatwords(garrays[i], &vecsize, &vectors[i]))
            error("%s: bad template for tabwrite",
                argv[i].a_w.w_symbol->s_name);
        if (wa.wa_nframes > vecsize - wa.wa_onsetframes)
            wa.wa_nframes = vecsize - wa.wa_onsetframes;
    }
    if (wa.wa_nframes <= 0)
    {
        pd_error(obj, "soundfiler_write: no samples at onset %ld",
            wa.wa_onsetframes);
        goto fail;
    }
    /* find biggest sample for normalizing */
    for (i = 0; i < info->i_nchannels; i++)
    {
        for (j = wa.wa_onsetframes; j < wa.wa_nframes + wa.wa_onsetframes; j++)
        {
            if (vectors[i][j].w_float > biggest)
                biggest = vectors[i][j].w_float;
            else if (-vectors[i][j].w_float > biggest)
                biggest = -vectors[i][j].w_float;
        }
    }
    if ((fd = create_soundfile(canvas, wa.wa_filesym->s_name, wa.wa_filetype,
        wa.wa_nframes, info)) < 0)
    {
        post("%s: %s\n", wa.wa_filesym->s_name, strerror(errno));
        goto fail;
    }
    if (!wa.wa_normalize)
    {
        if ((info->i_bytespersample != 4) && (biggest > 1))
        {
            post("%s: reducing max amplitude %f to 1",
                wa.wa_filesym->s_name, biggest);
            wa.wa_normalize = 1;
        }
        else post("%s: biggest amplitude = %f", wa.wa_filesym->s_name, biggest);
    }
    if (wa.wa_normalize)
        normfactor = (biggest > 0 ? 32767./(32768. * biggest) : 1);
    else normfactor = 1;

    bufframes = SAMPBUFSIZE / info->i_bytesperframe;

    for (frameswritten = 0; frameswritten < wa.wa_nframes;)
    {
        size_t thiswrite = wa.wa_nframes - frameswritten,
               byteswritten, datasize;
        thiswrite = (thiswrite > bufframes ? bufframes : thiswrite);
        datasize = info->i_bytesperframe * thiswrite;
        soundfile_xferout_words(info, vectors, (unsigned char *)sampbuf,
            thiswrite, wa.wa_onsetframes, normfactor);
        byteswritten = write(fd, sampbuf, datasize);
        if (byteswritten < datasize)
        {
            post("%s: %s", wa.wa_filesym->s_name, strerror(errno));
            if (byteswritten > 0)
                frameswritten += byteswritten / info->i_bytesperframe;
            break;
        }
        frameswritten += thiswrite;
        wa.wa_onsetframes += thiswrite;
    }
    if (fd >= 0)
    {
        soundfile_finishwrite(obj, wa.wa_filesym->s_name, fd, wa.wa_filetype,
            wa.wa_nframes, frameswritten, info);
        close(fd);
    }
    return frameswritten;
usage:
    pd_error(obj, "usage: write [flags] filename tablename...");
    post("flags: -skip <n> -nframes <n> -bytes <n> -wave -aiff -nextstep ...");
    post("-big -little -normalize");
    post("(defaults to a 16-bit wave file)");
fail:
    if (fd >= 0)
        close(fd);
    return 0;
}

static void soundfiler_write(t_soundfiler *x, t_symbol *s,
    int argc, t_atom *argv)
{
    long frameswritten;
    t_soundfile_info info;
    soundfile_info_clear(&info);
    frameswritten = soundfiler_dowrite(x, x->x_canvas, argc, argv, &info);
    if (frameswritten == 0)
        soundfile_info_clear(&info); /* clear any bad data */
    outlet_soundfile_info(x->x_out2, &info);
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

#define MAXBYTESPERSAMPLE 4
#define MAXVECSIZE 128

#define READSIZE 65536
#define WRITESIZE 65536
#define DEFBUFPERCHAN 262144
#define MINBUFSIZE (4 * READSIZE)
#define MAXBUFSIZE 16777216     /* arbitrary; just don't want to hang malloc */

#define REQUEST_NOTHING 0
#define REQUEST_OPEN 1
#define REQUEST_CLOSE 2
#define REQUEST_QUIT 3
#define REQUEST_BUSY 4

#define STATE_IDLE 0
#define STATE_STARTUP 1
#define STATE_STREAM 2

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
    int x_state;                      /**< opened, running, or idle */
    t_float x_insamplerate;           /**< input signal sample rate, if known */
        /* parameters to communicate with subthread */
    int x_requestcode;        /**< pending request from parent to I/O thread */
    const char *x_filename;   /**< file to open (string permanently alloced) */
    int x_fileerror;          /**< slot for "errno" return */
    t_soundfile_info x_info;  /**< soundfile format info */
    size_t x_onsetframes;     /**< number of sample frames to skip */
    int x_fd;                 /**< filedesc */
    int x_fifosize;           /**< buffer size appropriately rounded down */
    int x_fifohead;           /**< index of next byte to get from file */
    int x_fifotail;           /**< index of next byte the ugen will read */
    int x_eof;                /**< true if fifohead has stopped changing */
    int x_sigcountdown;       /**< counter for signaling child for more data */
    int x_sigperiod;          /**< number of ticks per signal */
    int x_filetype;           /**< writesf~ only; type of file to create */
    size_t x_frameswritten;   /**< writesf~ only; frames written */
    t_float x_f;              /**< writesf~ only; scalar for signal inlet */
    pthread_mutex_t x_mutex;
    pthread_cond_t x_requestcondition;
    pthread_cond_t x_answercondition;
    pthread_t x_childthread;
} t_readsf;

/* ----- the child thread which performs file I/O ----- */

#if 0
static void pute(const char *s)   /* debug routine */
{
    write(2, s, strlen(s));
}
#define DEBUG_SOUNDFILE
#endif

#if 1
#define sfread_cond_wait pthread_cond_wait
#define sfread_cond_signal pthread_cond_signal
#else
#include <sys/time.h>    /* debugging version... */
#include <sys/types.h>
static void readsf_fakewait(pthread_mutex_t *b)
{
    struct timeval timout;
    timout.tv_sec = 0;
    timout.tv_usec = 1000000;
    pthread_mutex_unlock(b);
    select(0, 0, 0, 0, &timout);
    pthread_mutex_lock(b);
}

#define sfread_cond_wait(a,b) readsf_fakewait(b)
#define sfread_cond_signal(a)
#endif

static void *readsf_child_main(void *zz)
{
    t_readsf *x = zz;
#ifdef DEBUG_SOUNDFILE
    pute("1\n");
#endif
    pthread_mutex_lock(&x->x_mutex);
    while (1)
    {
        int fd, fifohead;
        char *buf;
#ifdef DEBUG_SOUNDFILE
        pute("0\n");
#endif
        if (x->x_requestcode == REQUEST_NOTHING)
        {
#ifdef DEBUG_SOUNDFILE
            pute("wait 2\n");
#endif
            sfread_cond_signal(&x->x_answercondition);
            sfread_cond_wait(&x->x_requestcondition, &x->x_mutex);
#ifdef DEBUG_SOUNDFILE
            pute("3\n");
#endif
        }
        else if (x->x_requestcode == REQUEST_OPEN)
        {
            char boo[80];
            ssize_t bytesread;
            size_t wantbytes;

                /* copy file stuff out of the data structure so we can
                relinquish the mutex while we're in open_soundfile(). */
            t_soundfile_info info;
            size_t onsetframes = x->x_onsetframes;
            const char *filename = x->x_filename;
            const char *dirname = canvas_getdir(x->x_canvas)->s_name;
            soundfile_info_clear(&info);
            soundfile_info_copy(&info, &x->x_info);
#ifdef DEBUG_SOUNDFILE
            pute("4\n");
#endif
                /* alter the request code so that an ensuing "open" will get
                noticed. */
            x->x_requestcode = REQUEST_BUSY;
            x->x_fileerror = 0;

                /* if there's already a file open, close it */
            if (x->x_fd >= 0)
            {
                fd = x->x_fd;
                pthread_mutex_unlock(&x->x_mutex);
                close(fd);
                pthread_mutex_lock(&x->x_mutex);
                x->x_fd = -1;
                if (x->x_requestcode != REQUEST_BUSY)
                    goto lost;
            }
                /* open the soundfile with the mutex unlocked */
            pthread_mutex_unlock(&x->x_mutex);
            fd = open_soundfile(dirname, filename, &info, onsetframes);
            pthread_mutex_lock(&x->x_mutex);

#ifdef DEBUG_SOUNDFILE
            pute("5\n");
#endif
                /* copy back into the instance structure. */
            soundfile_info_copy(&x->x_info, &info);
            x->x_fd = fd;
            if (fd < 0)
            {
                x->x_fileerror = errno;
                x->x_eof = 1;
#ifdef DEBUG_SOUNDFILE
                pute("open failed\n");
                pute(filename);
                pute(dirname);
#endif
                goto lost;
            }
                /* check if another request has been made; if so, field it */
            if (x->x_requestcode != REQUEST_BUSY)
                goto lost;
#ifdef DEBUG_SOUNDFILE
            pute("6\n");
#endif
            x->x_fifohead = 0;
                    /* set fifosize from bufsize.  fifosize must be a
                    multiple of the number of bytes eaten for each DSP
                    tick.  We pessimistically assume MAXVECSIZE samples
                    per tick since that could change.  There could be a
                    problem here if the vector size increases while a
                    soundfile is being played...  */
            x->x_fifosize = x->x_bufsize - (x->x_bufsize %
                (x->x_info.i_bytesperframe * MAXVECSIZE));
                    /* arrange for the "request" condition to be signaled 16
                    times per buffer */
#ifdef DEBUG_SOUNDFILE
            sprintf(boo, "fifosize %d\n",
                x->x_fifosize);
            pute(boo);
#endif
            x->x_sigcountdown = x->x_sigperiod = (x->x_fifosize /
                (16 * x->x_info.i_bytesperframe * x->x_vecsize));
                /* in a loop, wait for the fifo to get hungry and feed it */

            while (x->x_requestcode == REQUEST_BUSY)
            {
                int fifosize = x->x_fifosize;
#ifdef DEBUG_SOUNDFILE
                pute("77\n");
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
                        if (wantbytes > x->x_info.i_bytelimit)
                            wantbytes = x->x_info.i_bytelimit;
#ifdef DEBUG_SOUNDFILE
                        sprintf(boo, "head %d, tail %d, size %ld\n",
                            x->x_fifohead, x->x_fifotail, wantbytes);
                        pute(boo);
#endif
                    }
                    else
                    {
#ifdef DEBUG_SOUNDFILE
                        pute("wait 7a ...\n");
#endif
                        sfread_cond_signal(&x->x_answercondition);
#ifdef DEBUG_SOUNDFILE
                        pute("signaled\n");
#endif
                        sfread_cond_wait(&x->x_requestcondition,
                            &x->x_mutex);
#ifdef DEBUG_SOUNDFILE
                        pute("7a done\n");
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
#ifdef DEBUG_SOUNDFILE
                        pute("wait 7...\n");
#endif
                        sfread_cond_signal(&x->x_answercondition);
                        sfread_cond_wait(&x->x_requestcondition,
                            &x->x_mutex);
#ifdef DEBUG_SOUNDFILE
                        pute("7 done\n");
#endif
                        continue;
                    }
                    else wantbytes = READSIZE;
                    if (wantbytes > x->x_info.i_bytelimit)
                        wantbytes = x->x_info.i_bytelimit;
                }
#ifdef DEBUG_SOUNDFILE
                pute("8\n");
#endif
                fd = x->x_fd;
                buf = x->x_buf;
                fifohead = x->x_fifohead;
                pthread_mutex_unlock(&x->x_mutex);
                bytesread = read(fd, buf + fifohead, wantbytes);
                pthread_mutex_lock(&x->x_mutex);
                if (x->x_requestcode != REQUEST_BUSY)
                    break;
                if (bytesread < 0)
                {
#ifdef DEBUG_SOUNDFILE
                    pute("fileerror\n");
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
                    x->x_info.i_bytelimit -= bytesread;
                    if (x->x_fifohead == fifosize)
                        x->x_fifohead = 0;
                    if (x->x_info.i_bytelimit <= 0)
                    {
                        x->x_eof = 1;
                        break;
                    }
                }
#ifdef DEBUG_SOUNDFILE
                sprintf(boo, "after: head %d, tail %d\n",
                    x->x_fifohead, x->x_fifotail);
                pute(boo);
#endif
                    /* signal parent in case it's waiting for data */
                sfread_cond_signal(&x->x_answercondition);
            }
        lost:

            if (x->x_requestcode == REQUEST_BUSY)
                x->x_requestcode = REQUEST_NOTHING;
                /* fell out of read loop: close file if necessary,
                set EOF and signal once more */
            if (x->x_fd >= 0)
            {
                fd = x->x_fd;
                pthread_mutex_unlock(&x->x_mutex);
                close(fd);
                pthread_mutex_lock(&x->x_mutex);
                x->x_fd = -1;
            }
            sfread_cond_signal(&x->x_answercondition);

        }
        else if (x->x_requestcode == REQUEST_CLOSE)
        {
            if (x->x_fd >= 0)
            {
                fd = x->x_fd;
                pthread_mutex_unlock(&x->x_mutex);
                close(fd);
                pthread_mutex_lock(&x->x_mutex);
                x->x_fd = -1;
            }
            if (x->x_requestcode == REQUEST_CLOSE)
                x->x_requestcode = REQUEST_NOTHING;
            sfread_cond_signal(&x->x_answercondition);
        }
        else if (x->x_requestcode == REQUEST_QUIT)
        {
            if (x->x_fd >= 0)
            {
                fd = x->x_fd;
                pthread_mutex_unlock(&x->x_mutex);
                close(fd);
                pthread_mutex_lock(&x->x_mutex);
                x->x_fd = -1;
            }
            x->x_requestcode = REQUEST_NOTHING;
            sfread_cond_signal(&x->x_answercondition);
            break;
        }
        else
        {
#ifdef DEBUG_SOUNDFILE
            pute("13\n");
#endif
        }
    }
#ifdef DEBUG_SOUNDFILE
    pute("thread exit\n");
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
    soundfile_info_clear(&x->x_info);
    x->x_info.i_bytespersample = 2;
    x->x_info.i_nchannels = 1;
    x->x_info.i_bytesperframe = 2;
    x->x_fd = -1;
    x->x_buf = buf;
    x->x_bufsize = bufsize;
    x->x_fifosize = x->x_fifohead = x->x_fifotail = x->x_requestcode = 0;
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
    t_soundfile_info info = {0};
    int vecsize = x->x_vecsize, noutlets = x->x_noutlets, i;
    size_t j;
    t_sample *fp;
    soundfile_info_copy(&info, &x->x_info);
    if (x->x_state == STATE_STREAM)
    {
        int wantbytes;
        pthread_mutex_lock(&x->x_mutex);
        wantbytes = vecsize * info.i_bytesperframe;
        while (
            !x->x_eof && x->x_fifohead >= x->x_fifotail &&
                x->x_fifohead < x->x_fifotail + wantbytes-1)
        {
#ifdef DEBUG_SOUNDFILE
            pute("wait...\n");
#endif
            sfread_cond_signal(&x->x_requestcondition);
            sfread_cond_wait(&x->x_answercondition, &x->x_mutex);
                /* resync local variables -- bug fix thanks to Shahrokh */
            vecsize = x->x_vecsize;
            soundfile_info_copy(&info, &x->x_info);
            wantbytes = vecsize * info.i_bytesperframe;
#ifdef DEBUG_SOUNDFILE
            pute("done\n");
#endif
        }
        if (x->x_eof && x->x_fifohead >= x->x_fifotail &&
            x->x_fifohead < x->x_fifotail + wantbytes-1)
        {
            int xfersize;
            if (x->x_fileerror)
            {
                pd_error(x, "readsf: %s: %s", x->x_filename,
                    (x->x_fileerror == EIO ?
                        "unknown or bad header format" :
                            strerror(x->x_fileerror)));
            }
            clock_delay(x->x_clock, 0);
            x->x_state = STATE_IDLE;

                /* if there's a partial buffer left, copy it out. */
            xfersize = (x->x_fifohead - x->x_fifotail + 1) /
                       info.i_bytesperframe;
            if (xfersize)
            {
                soundfile_xferin_sample(&info, noutlets, x->x_outvec, 0,
                    (unsigned char *)(x->x_buf + x->x_fifotail), xfersize);
                vecsize -= xfersize;
            }
                /* then zero out the (rest of the) output */
            for (i = 0; i < noutlets; i++)
                for (j = vecsize, fp = x->x_outvec[i] + xfersize; j--;)
                    *fp++ = 0;

            sfread_cond_signal(&x->x_requestcondition);
            pthread_mutex_unlock(&x->x_mutex);
            return w + 2;
        }

        soundfile_xferin_sample(&info, noutlets, x->x_outvec, 0,
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
    else pd_error(x, "readsf: start requested with no prior 'open'");
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
        open filename [skipframes headersize channels bytespersamp endianness]
        (if headersize is zero, header is taken to be automatically detected;
        thus, use the special "-1" to mean a truly headerless file.) */
static void readsf_open(t_readsf *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *filesym = atom_getsymbolarg(0, argc, argv);
    t_float onsetframes = atom_getfloatarg(1, argc, argv);
    t_float headersize = atom_getfloatarg(2, argc, argv);
    t_float nchannels = atom_getfloatarg(3, argc, argv);
    t_float bytespersample = atom_getfloatarg(4, argc, argv);
    t_symbol *endian = atom_getsymbolarg(5, argc, argv);
    if (!*filesym->s_name)
        return;
    pthread_mutex_lock(&x->x_mutex);
    soundfile_info_clear(&x->x_info);
    x->x_requestcode = REQUEST_OPEN;
    x->x_filename = filesym->s_name;
    x->x_fifotail = 0;
    x->x_fifohead = 0;
    if (*endian->s_name == 'b')
         x->x_info.i_bigendian = 1;
    else if (*endian->s_name == 'l')
         x->x_info.i_bigendian = 0;
    else if (*endian->s_name)
        pd_error(x, "endianness neither 'b' nor 'l'");
    else x->x_info.i_bigendian = sys_isbigendian();
    x->x_onsetframes = (onsetframes > 0 ? onsetframes : 0);
    x->x_info.i_headersize = (headersize > 0 ? headersize :
        (headersize == 0 ? -1 : 0));
    x->x_info.i_nchannels = (nchannels >= 1 ? nchannels : 1);
    x->x_info.i_bytespersample = (bytespersample > 2 ? bytespersample : 2);
    x->x_info.i_bytesperframe = x->x_info.i_nchannels *
                                x->x_info.i_bytespersample;
    x->x_eof = 0;
    x->x_fileerror = 0;
    x->x_state = STATE_STARTUP;
    sfread_cond_signal(&x->x_requestcondition);
    pthread_mutex_unlock(&x->x_mutex);
}

static void readsf_dsp(t_readsf *x, t_signal **sp)
{
    int i, noutlets = x->x_noutlets;
    pthread_mutex_lock(&x->x_mutex);
    x->x_vecsize = sp[0]->s_n;
    x->x_sigperiod = x->x_fifosize / (x->x_info.i_bytesperframe * x->x_vecsize);
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
    post("fd %d", x->x_fd);
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
        error("readsf_free: join failed");

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

#define t_writesf t_readsf    /* just re-use the structure */

/* ----- the child thread which performs file I/O ----- */

static void *writesf_child_main(void *zz)
{
    t_writesf *x = zz;
#ifdef DEBUG_SOUNDFILE
    pute("1\n");
#endif
    pthread_mutex_lock(&x->x_mutex);
    while (1)
    {
#ifdef DEBUG_SOUNDFILE
        pute("0\n");
#endif
        if (x->x_requestcode == REQUEST_NOTHING)
        {
#ifdef DEBUG_SOUNDFILE
            pute("wait 2\n");
#endif
            sfread_cond_signal(&x->x_answercondition);
            sfread_cond_wait(&x->x_requestcondition, &x->x_mutex);
#ifdef DEBUG_SOUNDFILE
            pute("3\n");
#endif
        }
        else if (x->x_requestcode == REQUEST_OPEN)
        {
            char boo[80];
            int fd;
            ssize_t byteswritten;
            size_t writebytes;

                /* copy file stuff out of the data structure so we can
                relinquish the mutex while we're in open_soundfile(). */
            t_soundfile_info info;

            int filetype = x->x_filetype;
            const char *filename = x->x_filename;
            t_canvas *canvas = x->x_canvas;

            soundfile_info_copy(&info, &x->x_info);
                /* alter the request code so that an ensuing "open" will get
                noticed. */
#ifdef DEBUG_SOUNDFILE
            pute("4\n");
#endif
            x->x_requestcode = REQUEST_BUSY;
            x->x_fileerror = 0;

                /* if there's already a file open, close it.  This
                should never happen since writesf_open() calls stop if
                needed and then waits until we're idle. */
            if (x->x_fd >= 0)
            {
                t_soundfile_info info;
                const char *filename = x->x_filename;
                int fd = x->x_fd;
                int filetype = x->x_filetype;
                size_t frameswritten = x->x_frameswritten;

                soundfile_info_copy(&info, &x->x_info);

                pthread_mutex_unlock(&x->x_mutex);

                soundfile_finishwrite(x, filename, fd,
                    filetype, SFMAXFRAMES, frameswritten, &info);
                close(fd);

                pthread_mutex_lock(&x->x_mutex);
                x->x_fd = -1;
#ifdef DEBUG_SOUNDFILE
                {
                    char s[1000];
                    sprintf(s, "bug??? ditched %d\n", frameswritten);
                    pute(s);
                }
#endif
                if (x->x_requestcode != REQUEST_BUSY)
                    continue;
            }

                /* open the soundfile with the mutex unlocked */
            pthread_mutex_unlock(&x->x_mutex);

            soundfile_info_copy(&info, &x->x_info);

            fd = create_soundfile(canvas, filename, filetype, 0, &info);
            pthread_mutex_lock(&x->x_mutex);
#ifdef DEBUG_SOUNDFILE
            pute("5\n");
#endif

            if (fd < 0)
            {
                x->x_fd = -1;
                x->x_eof = 1;
                x->x_fileerror = errno;
#ifdef DEBUG_SOUNDFILE
                pute("open failed\n");
                pute(filename);
#endif
                x->x_requestcode = REQUEST_NOTHING;
                continue;
            }
                /* check if another request has been made; if so, field it */
            if (x->x_requestcode != REQUEST_BUSY)
                continue;
#ifdef DEBUG_SOUNDFILE
            pute("6\n");
#endif
            x->x_fd = fd;
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
#ifdef DEBUG_SOUNDFILE
                pute("77\n");
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
#ifdef DEBUG_SOUNDFILE
                    pute("wait 7a ...\n");
#endif
                    sfread_cond_signal(&x->x_answercondition);
#ifdef DEBUG_SOUNDFILE
                    pute("signaled\n");
#endif
                    sfread_cond_wait(&x->x_requestcondition,
                        &x->x_mutex);
#ifdef DEBUG_SOUNDFILE
                    pute("7a done\n");
#endif
                    continue;
                }
#ifdef DEBUG_SOUNDFILE
                pute("8\n");
#endif
                fifotail = x->x_fifotail;
                fd = x->x_fd;
                pthread_mutex_unlock(&x->x_mutex);
                byteswritten = write(fd, buf + fifotail, writebytes);
                pthread_mutex_lock(&x->x_mutex);
                if (x->x_requestcode != REQUEST_BUSY &&
                    x->x_requestcode != REQUEST_CLOSE)
                        break;
                if (byteswritten < writebytes)
                {
#ifdef DEBUG_SOUNDFILE
                    pute("fileerror\n");
#endif
                    x->x_fileerror = errno;
                    break;
                }
                else
                {
                    x->x_fifotail += byteswritten;
                    if (x->x_fifotail == fifosize)
                        x->x_fifotail = 0;
                }
                x->x_frameswritten += byteswritten / x->x_info.i_bytesperframe;
#ifdef DEBUG_SOUNDFILE
                sprintf(boo, "after: head %d, tail %d written %d\n",
                    x->x_fifohead, x->x_fifotail, x->x_frameswritten);
                pute(boo);
#endif
                    /* signal parent in case it's waiting for data */
                sfread_cond_signal(&x->x_answercondition);
            }
        }
        else if (x->x_requestcode == REQUEST_CLOSE ||
            x->x_requestcode == REQUEST_QUIT)
        {
            int quit = (x->x_requestcode == REQUEST_QUIT);
            if (x->x_fd >= 0)
            {
                t_soundfile_info info;
                const char *filename = x->x_filename;
                int fd = x->x_fd;
                int filetype = x->x_filetype;
                size_t frameswritten = x->x_frameswritten;

                fd = x->x_fd;
                soundfile_info_copy(&info, &x->x_info);

                pthread_mutex_unlock(&x->x_mutex);

                soundfile_finishwrite(x, filename, fd,
                    filetype, SFMAXFRAMES, frameswritten, &info);
                close(fd);

                pthread_mutex_lock(&x->x_mutex);
                x->x_fd = -1;
            }
            x->x_requestcode = REQUEST_NOTHING;
            sfread_cond_signal(&x->x_answercondition);
            if (quit)
                break;
        }
        else
        {
#ifdef DEBUG_SOUNDFILE
            pute("13\n");
#endif
        }
    }
#ifdef DEBUG_SOUNDFILE
    pute("thread exit\n");
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
    soundfile_info_clear(&x->x_info);
    x->x_info.i_nchannels = nchannels;
    x->x_info.i_bytespersample = 2;
    x->x_info.i_bytesperframe = nchannels * 2;
    x->x_fd = -1;
    x->x_buf = buf;
    x->x_bufsize = bufsize;
    x->x_fifosize = x->x_fifohead = x->x_fifotail = x->x_requestcode = 0;
    pthread_create(&x->x_childthread, 0, writesf_child_main, x);
    return x;
}

static t_int *writesf_perform(t_int *w)
{
    t_writesf *x = (t_writesf *)(w[1]);
    t_soundfile_info info = {0};
    int vecsize = x->x_vecsize;
    soundfile_info_copy(&info, &x->x_info);
    if (x->x_state == STATE_STREAM)
    {
        int roominfifo;
        size_t wantbytes;
        pthread_mutex_lock(&x->x_mutex);
        wantbytes = vecsize * info.i_bytesperframe;
        roominfifo = x->x_fifotail - x->x_fifohead;
        if (roominfifo <= 0)
            roominfifo += x->x_fifosize;
        while (roominfifo < wantbytes + 1)
        {
            fprintf(stderr, "writesf waiting for disk write..\n");
            fprintf(stderr, "(head %d, tail %d, room %d, want %ld)\n",
                x->x_fifohead, x->x_fifotail, roominfifo, wantbytes);
            sfread_cond_signal(&x->x_requestcondition);
            sfread_cond_wait(&x->x_answercondition, &x->x_mutex);
            fprintf(stderr, "... done waiting.\n");
            roominfifo = x->x_fifotail - x->x_fifohead;
            if (roominfifo <= 0)
                roominfifo += x->x_fifosize;
        }

        soundfile_xferout_sample(&info, x->x_outvec,
            (unsigned char *)(x->x_buf + x->x_fifohead), vecsize, 0, 1.);

        x->x_fifohead += wantbytes;
        if (x->x_fifohead >= x->x_fifosize)
            x->x_fifohead = 0;
        if ((--x->x_sigcountdown) <= 0)
        {
#ifdef DEBUG_SOUNDFILE
            pute("signal 1\n");
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
        pd_error(x, "writesf: start requested with no prior 'open'");
}

    /** LATER rethink whether you need the mutex just to set a variable? */
static void writesf_stop(t_writesf *x)
{
    pthread_mutex_lock(&x->x_mutex);
    x->x_state = STATE_IDLE;
    x->x_requestcode = REQUEST_CLOSE;
#ifdef DEBUG_SOUNDFILE
    pute("signal 2\n");
#endif
    sfread_cond_signal(&x->x_requestcondition);
    pthread_mutex_unlock(&x->x_mutex);
}

    /** open method.  Called as: open [args] filename with args as in
        soundfiler_writeargs_parse(). */
static void writesf_open(t_writesf *x, t_symbol *s, int argc, t_atom *argv)
{
    t_soundfiler_writeargs wa = {NULL, FILETYPE_UNKNOWN, 0};
    if (x->x_state != STATE_IDLE)
    {
        writesf_stop(x);
    }
    if (soundfiler_writeargs_parse(x, &argc, &argv, &wa))
    {
        pd_error(x, "usage: open [flags] filename...");
        post("flags: -bytes <n> -wave -nextstep -aiff ...");
        post("-big -little -rate <n>");
        return;
    }
    if (wa.wa_normalize || wa.wa_onsetframes || (wa.wa_nframes != SFMAXFRAMES))
        pd_error(x, "normalize/onset/nframes argument to writesf~ ignored");
    if (argc)
        pd_error(x, "extra argument(s) to writesf~ ignored");
    pthread_mutex_lock(&x->x_mutex);
    while (x->x_requestcode != REQUEST_NOTHING)
    {
        sfread_cond_signal(&x->x_requestcondition);
        sfread_cond_wait(&x->x_answercondition, &x->x_mutex);
    }
    x->x_filename = wa.wa_filesym->s_name;
    x->x_filetype = (int)wa.wa_filetype;
    if (wa.wa_samplerate > 0)
        x->x_info.i_samplerate = wa.wa_samplerate;
    else if (x->x_insamplerate > 0)
        x->x_info.i_samplerate = x->x_insamplerate;
    else x->x_info.i_samplerate = sys_getsr();
    x->x_info.i_bytespersample =
        (wa.wa_bytespersample > 2 ? wa.wa_bytespersample : 2);
    x->x_info.i_bigendian = wa.wa_bigendian;
    x->x_info.i_bytesperframe = x->x_info.i_nchannels *
                                x->x_info.i_bytespersample;
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
        (x->x_info.i_bytesperframe * MAXVECSIZE));
        /* arrange for the "request" condition to be signaled 16
            times per buffer */
    x->x_sigcountdown = x->x_sigperiod = (x->x_fifosize /
            (16 * (x->x_info.i_bytesperframe * x->x_vecsize)));
    sfread_cond_signal(&x->x_requestcondition);
    pthread_mutex_unlock(&x->x_mutex);
}

static void writesf_dsp(t_writesf *x, t_signal **sp)
{
    int i, ninlets = x->x_info.i_nchannels;
    pthread_mutex_lock(&x->x_mutex);
    x->x_vecsize = sp[0]->s_n;
    x->x_sigperiod = (x->x_fifosize /
            (16 * x->x_info.i_bytesperframe * x->x_vecsize));
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
    post("fd %d", x->x_fd);
    post("eof %d", x->x_eof);
}

    /** request QUIT and wait for acknowledge */
static void writesf_free(t_writesf *x)
{
    void *threadrtn;
    pthread_mutex_lock(&x->x_mutex);
    x->x_requestcode = REQUEST_QUIT;
#ifdef DEBUG_SOUNDFILE
    post("stopping writesf thread...");
#endif
    sfread_cond_signal(&x->x_requestcondition);
    while (x->x_requestcode != REQUEST_NOTHING)
    {
#ifdef DEBUG_SOUNDFILE
        post("signaling...");
#endif
        sfread_cond_signal(&x->x_requestcondition);
        sfread_cond_wait(&x->x_answercondition, &x->x_mutex);
    }
    pthread_mutex_unlock(&x->x_mutex);
    if (pthread_join(x->x_childthread, &threadrtn))
        error("writesf_free: join failed");
#ifdef DEBUG_SOUNDFILE
    post("... done.");
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
    soundfiler_setup();
    readsf_setup();
    writesf_setup();
}
