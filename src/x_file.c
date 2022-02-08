/* Copyright (c) 2021 IOhannes m zmölnig.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* The "file" object. */
#define _XOPEN_SOURCE 600

#include "m_pd.h"
#include "g_canvas.h"
#include "s_utf8.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#ifdef _WIN32
# include <windows.h>
# include <io.h>
#else
# include <glob.h>
# include <ftw.h>
#endif

#ifdef _MSC_VER
# include <BaseTsd.h>
typedef unsigned int mode_t;
typedef SSIZE_T ssize_t;
# define wstat _wstat
# define snprintf _snprintf
#endif

#ifdef _WIN32
# include <malloc.h> /* MSVC or mingw on windows */
#elif defined(__linux__) || defined(__APPLE__) || defined(HAVE_ALLOCA_H)
# include <alloca.h> /* linux, mac, mingw, cygwin */
#else
# include <stdlib.h> /* BSDs for example */
#endif

#ifndef S_ISREG
  #define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
# define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifdef _WIN32
static int do_delete_ucs2(wchar_t*pathname) {
    struct stat sb;
    if (!wstat(pathname, &sb) && (S_ISDIR(sb.st_mode))) {
            /* a directory */
        return !(RemoveDirectoryW(pathname));
    } else {
            /* probably a file */
        return !(DeleteFileW(pathname));
    }
}

static int sys_stat(const char *pathname, struct stat *statbuf) {
    int16_t ucs2buf[MAX_PATH];
    u8_utf8toucs2(ucs2buf, MAX_PATH, pathname, strlen(pathname));
    return wstat(ucs2buf, statbuf);
}

static int sys_rename(const char *oldpath, const char *newpath) {
    int16_t src[MAX_PATH], dst[MAX_PATH];
    u8_utf8toucs2(src, MAX_PATH, oldpath, MAX_PATH);
    u8_utf8toucs2(dst, MAX_PATH, newpath, MAX_PATH);
    return _wrename(src, dst);
}
static int sys_mkdir(const char *pathname, mode_t mode) {
    uint16_t ucs2name[MAX_PATH];
    u8_utf8toucs2(ucs2name, MAX_PATH, pathname, MAX_PATH);
    return !(CreateDirectoryW(ucs2name, 0));
}
static int sys_remove(const char *pathname) {
    uint16_t ucs2buf[MAXPDSTRING];
    u8_utf8toucs2(ucs2buf, MAXPDSTRING, pathname, MAXPDSTRING);
    return do_delete_ucs2(ucs2buf);
}
#else
static int sys_stat(const char *pathname, struct stat *statbuf) {
    return stat(pathname, statbuf);
}

static int sys_rename(const char *oldpath, const char *newpath) {
    return rename(oldpath, newpath);
}
static int sys_mkdir(const char *pathname, mode_t mode) {
    return mkdir(pathname, mode);
}
static int sys_remove(const char *pathname) {
    return remove(pathname);
}
#endif

#ifndef HAVE_ALLOCA     /* can work without alloca() but we never need it */
# define HAVE_ALLOCA 1
#endif
#ifdef ALLOCA
# undef ALLOCA
#endif
#ifdef FREEA
# undef FREEA
#endif

#if HAVE_ALLOCA
# define ALLOCA(t, x, n, max) ((x) = (t *)((n) < (max) ?            \
            alloca((n) * sizeof(t)) : getbytes((n) * sizeof(t))))
# define FREEA(t, x, n, max) (                                  \
        ((n) < (max) || (freebytes((x), (n) * sizeof(t)), 0)))
#else
# define ALLOCA(t, x, n, max) ((x) = (t *)getbytes((n) * sizeof(t)))
# define FREEA(t, x, n, max) (freebytes((x), (n) * sizeof(t)))
#endif

    /* expand env vars and ~ at the beginning of a path and make a copy to return */
static char*do_expandpath(const char *from, char *to, int bufsize)
{
    if ((strlen(from) == 1 && from[0] == '~') || (strncmp(from,"~/", 2) == 0))
    {
#ifdef _WIN32
        const char *home = getenv("USERPROFILE");
#else
        const char *home = getenv("HOME");
#endif
        if (home)
        {
            strncpy(to, home, bufsize);
            to[bufsize-1] = 0;
            strncpy(to + strlen(to), from + 1, bufsize - strlen(to));
            to[bufsize-1] = 0;
        }
        else *to = 0;
    }
    else
    {
        strncpy(to, from, bufsize);
        to[bufsize-1] = 0;
    }
#ifdef _WIN32
    {
        char *buf = alloca(bufsize);
        ExpandEnvironmentStrings(to, buf, bufsize-1);
        buf[bufsize-1] = 0;
        strncpy(to, buf, bufsize);
        to[bufsize-1] = 0;
    }
#endif
    return to;
}

    /* unbash '\' to '/', and drop duplicate '/' */
static char*do_pathnormalize(const char *from, char *to) {
    const char *rp;
    char *wp, c;
    sys_unbashfilename(from, to);
    rp=wp=to;
    while((*wp++=c=*rp++)) {
        if('/' == c) {
            while('/' == *rp++);
            rp--;
        }
    }
    return to;
}

static char*do_expandunbash(const char *from, char *to, int bufsize) {
    do_expandpath(from, to, bufsize);
    to[bufsize-1]=0;
    sys_unbashfilename(to, to);
    to[bufsize-1]=0;
    return to;
}

static int str_endswith(char* str, char* end){
    size_t strsize = strlen(str), endsize = strlen(end);
    if(strsize<endsize) return 0;
    return strcmp(str + strsize - endsize, end) == 0;
}

#ifdef _WIN32
static const char*do_errmsg(char*buffer, size_t bufsize) {
    char errcode[10];
    char*s;
    wchar_t wbuf[MAXPDSTRING];
    DWORD err = GetLastError();
    DWORD count = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        0, err, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), wbuf, MAXPDSTRING, NULL);
    if (!count || !WideCharToMultiByte(CP_UTF8, 0, wbuf, count+1, buffer, bufsize, 0, 0))
        *buffer = '\0';
    s=buffer + strlen(buffer)-1;
    while(('\r' == *s || '\n' == *s) && s>buffer)
        *s--=0;
    snprintf(errcode, sizeof(errcode), " [%ld]", err);
    errcode[sizeof(errcode)-1] = 0;
    strcat(buffer, errcode);
    return buffer;
}
#else /* !_WIN32 */
static const char*do_errmsg(char*buffer, size_t bufsize) {
    (void)buffer; (void)bufsize;
    return strerror(errno);
}
#endif /* !_WIN32 */

typedef struct _file_handler {
    int fh_fd;
    int fh_mode; /* 0..read, 1..write */
} t_file_handler;
#define x_fd x_fhptr->fh_fd
#define x_mode x_fhptr->fh_mode
typedef struct _file_handle {
    t_object x_obj;
    t_file_handler x_fh;
    t_file_handler*x_fhptr;
    t_symbol*x_fcname; /* multiple [file handle] object can refer to the same [file define] */

    mode_t x_creationmode; /* default: 0666, 0777 */
    int x_verbose; /* default: 0 */

    t_canvas*x_canvas;
    t_outlet*x_dataout;
    t_outlet*x_infoout;

} t_file_handle;

static int do_parse_creationmode(t_atom*ap) {
    const char*s;
    if(A_FLOAT==ap->a_type)
        return atom_getfloat(ap);
    if(A_SYMBOL!=ap->a_type)
        return -1; /* oopsie */
    s = atom_getsymbol(ap)->s_name;
    if(!strncmp(s, "0o", 2)) {
            /* octal mode */
        char*endptr;
        long mode = strtol(s+2, &endptr, 8);
        return (*endptr)?-1:(int)mode;
    } else if(!strncmp(s, "0x", 2)) {
            /* hex mode: nobody sane uses this... */
        char*endptr;
        long mode = strtol(s+2, &endptr, 16);
        return (*endptr)?-1:(int)mode;
    } else {
            /* free form mode: a+rwx,go-w */
            /* not supported yet */
        return -1;
    }
    return -1;
}

static void do_parse_args(t_file_handle*x, int argc, t_atom*argv) {
        /*
         * -q: quiet mode
         * -v: verbose mode
         * -m <mode>: creation mode
         */
    t_symbol*flag_m = gensym("-m");
    t_symbol*flag_q = gensym("-q");
    t_symbol*flag_v = gensym("-v");
    x->x_fcname = 0;
    while(argc--) {
        const t_symbol*flag = atom_getsymbol(argv);
        if (0);
        else if (flag == flag_q) {
            x->x_verbose--;
        } else if (flag == flag_v) {
            x->x_verbose++;
        } else if (flag == flag_m) {
            int mode;
            if(!argc) {
                pd_error(x, "'-m' requires an argument");
                break;
            }
            argc--;
            argv++;
            mode = do_parse_creationmode(argv);
            if(mode<0) {
                char buf[MAXPDSTRING];
                atom_string(argv, buf, MAXPDSTRING);
                pd_error(x, "invalid creation mode '%s'", buf);
                break;
            } else {
                x->x_creationmode = mode;
            }
        } else {
            int filearg = (!argc);
            if(filearg) {
                x->x_fcname = (t_symbol*)flag;
            } else {
                pd_error(x, "unknown flag %s", flag->s_name);
            }
            break;
        }
        argv++;
    }
    x->x_verbose = x->x_verbose > 0;
}

static t_file_handle* do_file_handle_new(t_class*cls, t_symbol*s, int argc, t_atom*argv, int verbose, mode_t creationmode) {
    t_file_handle*x = (t_file_handle*)pd_new(cls);
    (void)s;
    x->x_fhptr = &x->x_fh;
    x->x_fd = -1;
    x->x_canvas = canvas_getcurrent();
    x->x_creationmode = creationmode;
    x->x_verbose = verbose;

    x->x_dataout = outlet_new(&x->x_obj, 0);
    x->x_infoout = outlet_new(&x->x_obj, 0);
    do_parse_args(x, argc, argv);
    return x;
}

static int do_file_open(t_file_handle*x, const char* filename, int mode) {
    char expandbuf[MAXPDSTRING+1];
    int fd = sys_open(do_expandpath(filename, expandbuf, MAXPDSTRING), mode, x?x->x_creationmode:0666);
    if(x) {
        x->x_fd = fd;
        if(fd<0) {
            if(x->x_verbose)
                pd_error(x, "unable to open '%s': %s", filename, strerror(errno));
            if(x->x_infoout)
                outlet_bang(x->x_infoout);
        }
    }
    return fd;
}


static void file_set_verbosity(t_file_handle*x, t_float f) {
    x->x_verbose = (f>0.5);
}
static void file_set_creationmode(t_file_handle*x, t_symbol*s, int argc, t_atom*argv) {
    if(argc!=1) {
        pd_error(x, "usage: '%s <mode>'", s->s_name);
        return;
    }
    x->x_creationmode = do_parse_creationmode(argv);
}

    /* ================ [file handle] ====================== */
t_class *file_define_class;
static int file_handle_getdefine(t_file_handle*x) {
    if(x->x_fcname) {
        t_file_handle *y = (t_file_handle *)pd_findbyclass(x->x_fcname,
                                                           file_define_class);
        if(y) {
            x->x_fhptr=&y->x_fh;
            return 1;
        }
        return 0;
    }
    x->x_fhptr = &x->x_fh;
    return 1;
}

static void file_handle_close(t_file_handle*x) {
    if (x->x_fd>=0)
        sys_close(x->x_fd);
    x->x_fd = -1;
}
static int file_handle_checkopen(t_file_handle*x, const char*cmd) {
    if(x->x_fcname) {
        if(!file_handle_getdefine(x)) {
            pd_error(x, "file handle: couldn't find file-define '%s'", x->x_fcname->s_name);
            return 0;
        }
    }
    if(x->x_fd<0) {
        if(!cmd)cmd=(x->x_mode)?"write":"read";
        pd_error(x, "'%s' without prior 'open'", cmd);
        return 0;
    }
    return 1;
}
static void file_handle_do_read(t_file_handle*x, t_float f) {
    t_atom*outv;
    unsigned char*buf;
    ssize_t n, len, outc=f;
    if(outc<1) {
        pd_error(x, "cannot read %d bytes", (int)outc);
        return;
    }
    ALLOCA(unsigned char, buf, outc, 100);
    ALLOCA(t_atom, outv, outc, 100);
    if(buf && outv) {
        len = read(x->x_fd, buf, outc);
        for(n=0; n<len; n++) {
            SETFLOAT(outv+n, (t_float)buf[n]);
        }
        if(len>0) {
            outlet_list(x->x_dataout, gensym("list"), len, outv);
        } else if (!len) {
            file_handle_close(x);
            outlet_bang(x->x_infoout);
        } else {
            if(x->x_verbose)
                pd_error(x, "read failed: %s", strerror(errno));
            file_handle_close(x);
            outlet_bang(x->x_infoout);
        }
    } else {
        pd_error(x, "couldn't allocate buffer for %d bytes", (int)outc);
    }
    FREEA(unsigned char, buf, outc, 100);
    FREEA(t_atom, outv, outc, 100);
}
static void file_handle_do_write(t_file_handle*x, int argc, t_atom*argv) {
    unsigned char*buf;
    size_t len = (argc>0)?argc:0;
    ALLOCA(unsigned char, buf, argc, 100);
    if(buf) {
        ssize_t n;
        for(n=0; n<argc; n++) {
            buf[n] = atom_getfloat(argv+n);
        }
        n = write(x->x_fd, buf, len);
        if(n >= 0 && (size_t)n < len) {
            n = write(x->x_fd, buf+n, len-n);
        }
        if (n<0) {
            pd_error(x, "write failed: %s", strerror(errno));
            file_handle_close(x);
            outlet_bang(x->x_infoout);
        }
    } else {
        pd_error(x, "could not allocate %d bytes for writing", argc);
    }
    FREEA(unsigned char, buf, argc, 100);
}
static void file_handle_list(t_file_handle*x, t_symbol*s, int argc, t_atom*argv) {
    (void)s;
    if(!file_handle_checkopen(x, 0))
        return;
    if(x->x_mode) {
            /* write_mode */
        file_handle_do_write(x, argc, argv);
        } else {
            /* read mode */
        if(1==argc && A_FLOAT==argv->a_type) {
            file_handle_do_read(x, atom_getfloat(argv));
        } else {
            pd_error(x, "no way to handle 'list' messages while reading file");
        }
    }
}
static void file_handle_set(t_file_handle*x, t_symbol*s) {
    if (gensym("") == s)
        s=0;
    if (s && x->x_fhptr == &x->x_fh && x->x_fh.fh_fd >= 0) {
            /* trying to set a name, even though we have an fd open... */
        pd_error(x, "file handle: shadowing local file descriptor with '%s'", s->s_name);
    } else if (!s && x->x_fhptr != &x->x_fh && x->x_fh.fh_fd >= 0) {
        logpost(x, 3, "file handle: unshadowing local file descriptor");
    }
    x->x_fcname = s;
    file_handle_getdefine(x);
}
static void file_handle_seek(t_file_handle*x, t_symbol*s, int argc, t_atom*argv) {
    off_t offset=0;
    int whence = SEEK_SET;
    t_atom a[1];
    switch(argc) {
    case 0:
            /* just output the current position */
        whence=SEEK_CUR;
        break;
    case 2: {
        if (A_SYMBOL!=argv[1].a_type)
            goto usage;
        s=atom_getsymbol(argv+1);
        switch(*s->s_name) {
        case 'S': case 's': case 0:
            whence = SEEK_SET;
            break;
        case 'E': case 'e':
            whence = SEEK_END;
            break;
        case 'C': case 'c': case 'R': case 'r':
            whence = SEEK_CUR;
            break;
        default:
            pd_error(x, "seek mode must be 'set', 'end' or 'current' (resp. 'relative')");
            return;
        }
    }
            /* falls through */
    case 1:
        if (A_FLOAT!=argv[0].a_type)
            goto usage;
        offset = (int)atom_getfloat(argv);
        break;
    }

    if(!file_handle_checkopen(x, "seek"))
        return;
    offset = lseek(x->x_fd, offset, whence);
    SETFLOAT(a, offset);
    outlet_anything(x->x_infoout, gensym("seek"), 1, a);
    return;
 usage:
    pd_error(x, "usage: seek [<int:offset> [<symbol:mode>]]");
}
static void file_handle_open(t_file_handle*x, t_symbol*file, t_symbol*smode) {
    int mode = O_RDONLY;
    if (x->x_fd>=0) {
        pd_error(x, "'open' without prior 'close'");
        return;
    }
    if(!file_handle_getdefine(x)) {
        pd_error(x, "file handle: couldn't find file-define '%s'", x->x_fcname->s_name);
        return;
    }
    if(smode && smode!=&s_) {
        switch(smode->s_name[0]) {
        case 'r': /* read */
            mode = O_RDONLY;
            break;
        case 'w': /* write */
            mode = O_WRONLY;
            break;
        case 'a': /* append */
            mode = O_WRONLY | O_APPEND;
            break;
        case 'c': /* create */
            mode = O_WRONLY | O_TRUNC;
            break;
        }
    }
    if(mode & O_WRONLY) {
        mode |= O_CREAT;
    }
    if(do_file_open(x, file->s_name, mode)>=0) {
            /* check if we haven't accidentally opened a directory */
        struct stat sb;
        if(fstat(x->x_fd, &sb)) {
            file_handle_close(x);
            if(x->x_verbose)
                pd_error(x, "unable to stat '%s': %s", file->s_name, strerror(errno));
            outlet_bang(x->x_infoout);
            return;
        }
        if(S_ISDIR(sb.st_mode)) {
            file_handle_close(x);
            if(x->x_verbose)
                pd_error(x, "unable to open directory '%s' as file", file->s_name);
            outlet_bang(x->x_infoout);
            return;
        }
        x->x_mode = (mode&O_WRONLY)?1:0;
    }
}

static void file_handle_free(t_file_handle*x) {
        /* close our own file handle (if any) */
    x->x_fhptr = &x->x_fh;
    file_handle_close(x);
}

    /* ================ [file stat] ====================== */
static int do_file_stat(t_file_handle*x, const char*filename, struct stat*sb, int*is_symlink) {
    int result = -1;
    int fd = -1;
    char buf[MAXPDSTRING+1];
    do_expandpath(filename, buf, MAXPDSTRING);

    if(is_symlink) {
        *is_symlink=0;
#ifdef S_IFLNK
        if(!lstat(buf, sb)) {
            *is_symlink = !!(S_ISLNK(sb->st_mode));
        }
#endif
    }
    result = sys_stat(buf, sb);
    if(!result)
        return result;

    fd = do_file_open(0, filename, 0);

    if(fd >= 0) {
        result = fstat(fd, sb);
        sys_close(fd);
    } else
        result = -1;

    if(x) {
        x->x_fd = -1;
        if(result && x->x_verbose) {
            pd_error(x, "could not stat on '%s': %s", filename, strerror(errno));
        }
    }
    return result;
}
static void do_dataout_symbol(t_file_handle*x, const char*selector, t_symbol*s) {
    t_atom ap[1];
    SETSYMBOL(ap, s);
    outlet_anything(x->x_dataout, gensym(selector), 1, ap);
}
static void do_dataout_float(t_file_handle*x, const char*selector, t_float f) {
    t_atom ap[1];
    SETFLOAT(ap, f);
    outlet_anything(x->x_dataout, gensym(selector), 1, ap);
}
static void do_dataout_time(t_file_handle*x, const char*selector, time_t t) {
    t_atom ap[7];
    struct tm *ts = localtime(&t);
    if(!ts) {
        pd_error(x, "unable to convert timestamp %ld", (long int)t);
    }
    SETFLOAT(ap+0, ts->tm_year + 1900);
    SETFLOAT(ap+1, ts->tm_mon + 1);
    SETFLOAT(ap+2, ts->tm_mday);
    SETFLOAT(ap+3, ts->tm_hour);
    SETFLOAT(ap+4, ts->tm_min);
    SETFLOAT(ap+5, ts->tm_sec);
    SETFLOAT(ap+6, ts->tm_isdst);
    outlet_anything(x->x_dataout, gensym(selector), 7, ap);
}
static void file_stat_symbol(t_file_handle*x, t_symbol*filename) {
        /* get all the info for the given file */
    struct stat sb;
    t_symbol*s;
    int is_symlink=0;
    int readable=0, writable=0, executable=0, owned=-1;
    char buf[MAXPDSTRING+1];

    if(do_file_stat(x, filename->s_name, &sb, &is_symlink) < 0) {
        outlet_bang(x->x_infoout);
        return;
    }

        /* this is wrong: readable/writable/executable are supposed to report
         * on the *current* user, not the *owner*
         */
    readable = !!(sb.st_mode & 0400);
    writable = !!(sb.st_mode & 0200);
    executable = !!(sb.st_mode & 0100);
#ifdef HAVE_UNISTD_H
        /* this is the right way */
    do_expandpath(filename->s_name, buf, MAXPDSTRING);

    readable = !(access(buf, R_OK));
    writable = !(access(buf, W_OK));
    executable = !(access(buf, X_OK));
#ifndef _WIN32
    owned = (geteuid() == sb.st_uid);
#endif
#endif

    switch (sb.st_mode & S_IFMT) {
    case S_IFREG:
#ifdef S_IFLNK
    case S_IFLNK:
#endif
        do_dataout_float(x, "size", (int)(sb.st_size));
        break;
    case S_IFDIR:
        do_dataout_float(x, "size", 0);
        break;
    default:
        do_dataout_float(x, "size", -1);
        break;
    }
    do_dataout_float(x, "readable", readable);
    do_dataout_float(x, "writable", writable);
    do_dataout_float(x, "executable", executable);
    do_dataout_float(x, "owned", owned);

    do_dataout_float(x, "isfile", !!(S_ISREG(sb.st_mode)));
    do_dataout_float(x, "isdirectory", !!(S_ISDIR(sb.st_mode)));
    do_dataout_float(x, "issymlink", is_symlink);

    do_dataout_float(x, "uid", (int)(sb.st_uid));
    do_dataout_float(x, "gid", (int)(sb.st_gid));
    do_dataout_float(x, "permissions", (int)(sb.st_mode & 0777));
    switch (sb.st_mode & S_IFMT) {
    case S_IFREG:  s = gensym("file");            break;
    case S_IFDIR:  s = gensym("directory");       break;
#ifdef S_IFBLK
    case S_IFBLK:  s = gensym("blockdevice");     break;
#endif
#ifdef S_IFCHR
    case S_IFCHR:  s = gensym("characterdevice"); break;
#endif
#ifdef S_IFIFO
    case S_IFIFO:  s = gensym("pipe");            break;
#endif
#ifdef S_IFLNK
    case S_IFLNK:  s = gensym("symlink");         break;
#endif
#ifdef S_IFSOCK
    case S_IFSOCK: s = gensym("socket");          break;
#endif
    default:       s = 0;                         break;
    }
    if(s)
        do_dataout_symbol(x, "type", s);
    else
        do_dataout_symbol(x, "type", gensym("unknown"));

    do_dataout_time(x, "atime", sb.st_atime);
    do_dataout_time(x, "mtime", sb.st_mtime);
}

static void file_size_symbol(t_file_handle*x, t_symbol*filename) {
    struct stat sb;
    if(do_file_stat(x, filename->s_name, &sb, 0) < 0) {
        outlet_bang(x->x_infoout);
    } else {
        switch (sb.st_mode & S_IFMT) {
        case S_IFREG:
#ifdef S_IFLNK
        case S_IFLNK:
#endif
            outlet_float(x->x_dataout, (int)(sb.st_size));
            break;
        case S_IFDIR:
            outlet_float(x->x_dataout, 0);
            break;
        default:
            outlet_float(x->x_dataout, -1);
            break;
        }
    }
}
static void file_isfile_symbol(t_file_handle*x, t_symbol*filename) {
    struct stat sb;
    if(do_file_stat(x, filename->s_name, &sb, 0) < 0) {
        outlet_bang(x->x_infoout);
    } else {
        outlet_float(x->x_dataout, !!(S_ISREG(sb.st_mode)));
    }
}
static void file_isdirectory_symbol(t_file_handle*x, t_symbol*filename) {
    struct stat sb;
    if(do_file_stat(x, filename->s_name, &sb, 0) < 0) {
        outlet_bang(x->x_infoout);
    } else {
        outlet_float(x->x_dataout, !!(S_ISDIR(sb.st_mode)));
    }
}

    /* ================ [file glob] ====================== */
#ifdef _WIN32
/* idiosyncrasies:
 * - cases are ignored ('a*' matches 'A.txt' and 'a.txt'), even with wine on ext4
 * - only the filename component is returned (must prefix path separately)
 * - non-ASCII needs special handling
 * - '*?' seems to be illegal (e.g. 'f*?.txt'); '?*' seems to be fine though
 * - "*" matches files starting with '.' (including '.', '..', but also .gitignore)
 * - if the pattern includes '*.', it matches a trailing '~'
 * - wildcards do not apply to directory-components (e.g. 'foo/ * /' (without the spaces, they are just due to C-comments constraints))
 *
 * plan:
 * - concat the path and the filename
 * - convert to utf16 (and back again)
 * - replace '*?' with '*' in the pattern
 * - manually filter out:
 *   - matches starting with '.' if the pattern does not start with '.'
 *   - matches ending in '~' if the pattern does not end with '[*?~]'
 * - only (officially) support wildcards in the filename component (not in the paths)
 * - if the pattern ends with '/', strip it, but return only directories
 */
static void file_glob_symbol(t_file_handle*x, t_symbol*spattern) {
    WIN32_FIND_DATAW FindFileData;
    HANDLE hFind;
    uint16_t ucs2pattern[MAXPDSTRING];
    char pattern[MAXPDSTRING];
    int nostartdot=0, noendtilde=0, onlydirs=0;
    char *filepattern, *strin, *strout;
    int pathpatternlength=0;
    int matchdot=0;

    do_expandunbash(spattern->s_name, pattern, MAXPDSTRING);

        /* '.' and '..' should only match if the pattern exquisitely asked for them */
    if(!strcmp(".", pattern) || !strcmp("./", pattern)
        || str_endswith(pattern, "/.") || str_endswith(pattern, "/./"))
        matchdot=1;
    else if(!strcmp("..", pattern) || !strcmp("../", pattern)
        || str_endswith(pattern, "/..") || str_endswith(pattern, "/../"))
        matchdot=2;

    if (matchdot) {
            /* windows FindFile would return the actual path rather than '.'
             * (which would confuse our full-path construction)
             * so we just return the result directly
             */
        struct stat sb;
        if (!do_file_stat(0, pattern, &sb, 0)) {
            t_atom outv[2];
            size_t end = strlen(pattern);
                /* get rid of trailing slash */
            if('/' == pattern[end-1])
                pattern[end-1]=0;
            SETSYMBOL(outv+0, gensym(pattern));
            SETFLOAT(outv+1, S_ISDIR(sb.st_mode));
            outlet_list(x->x_dataout, gensym("list"), 2, outv);
        } else {
                // this gets triggered if there is no match...
            outlet_bang(x->x_infoout);
        }
        return;
    }


    filepattern=strrchr(pattern, '/');
    if(filepattern && !filepattern[1]) {
            /* patterns ends with slashes: filter for dirs, and bash the trailing slashes */
        onlydirs=1;
        while('/' == *filepattern && filepattern>pattern) {
            *filepattern--=0;
        }
        filepattern=strrchr(pattern, '/');
    }
    if(!filepattern)
        filepattern=pattern;
    else {
        filepattern++;
        pathpatternlength=filepattern-pattern;
    }
    nostartdot=('.' != *filepattern);
    strin=filepattern;
    strout=filepattern;
    while(*strin) {
        char c = *strin++;
        *strout++ = c;
        if('*' == c) {
            while('?' == *strin || '*' == *strin)
                strin++;
        }
    }
    *strout=0;
    if (strout>pattern) {
        switch(strout[-1]) {
        case '~':
        case '*':
        case '?':
            noendtilde=0;
            break;
        default:
            noendtilde=1;
        }
    }
    u8_utf8toucs2(ucs2pattern, MAXPDSTRING, pattern, MAXPDSTRING);

    hFind = FindFirstFileW(ucs2pattern, &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
            // this gets triggered if there is no match...
        outlet_bang(x->x_infoout);
        return;
    }
    do {
        t_symbol*s;
        t_atom outv[2];
        int len = 0;
        int isdir = !!(FILE_ATTRIBUTE_DIRECTORY & FindFileData.dwFileAttributes);
        if (matchdot!=1 && !wcscmp(L"." , FindFileData.cFileName))
            continue;
        if (matchdot!=2 && !wcscmp(L".." , FindFileData.cFileName))
            continue;
        u8_ucs2toutf8(filepattern, MAXPDSTRING-pathpatternlength, FindFileData.cFileName, MAX_PATH);
        len = strlen(filepattern);

        if(onlydirs && !isdir)
            continue;
        if(nostartdot && '.' == filepattern[0])
            continue;
        if(noendtilde && '~' == filepattern[len-1])
            continue;

        s = gensym(pattern);
        SETSYMBOL(outv+0, s);
        SETFLOAT(outv+1, isdir);
        outlet_list(x->x_dataout, gensym("list"), 2, outv);
    } while (FindNextFileW(hFind, &FindFileData) != 0);
    FindClose(hFind);
}
#else /* !_WIN32 */
static void file_glob_symbol(t_file_handle*x, t_symbol*spattern) {
    t_atom outv[2];
    glob_t gg;
    int flags = 0;
    int matchdot=0;
    char pattern[MAXPDSTRING];
    size_t patternlen;
    int onlydirs;
    do_expandpath(spattern->s_name, pattern, MAXPDSTRING);
    patternlen=strlen(pattern);
    onlydirs = ('/' == pattern[patternlen-1]);
    if(!strcmp(".", pattern) || !strcmp("./", pattern)
        || str_endswith(pattern, "/.") || str_endswith(pattern, "/./"))
        matchdot=1;
    else if(!strcmp("..", pattern) || !strcmp("../", pattern)
        || str_endswith(pattern, "/..") || str_endswith(pattern, "/../"))
        matchdot=2;
    if(glob(pattern, flags, NULL, &gg)) {
            // this gets triggered if there is no match...
        outlet_bang(x->x_infoout);
    } else {
        size_t i;
        for(i=0; i<gg.gl_pathc; i++) {
            t_symbol *s;
            char*path = gg.gl_pathv[i];
            int isdir = 0;
            struct stat sb;
            int end;
            if(!do_file_stat(0, path, &sb, 0)) {
                isdir = S_ISDIR(sb.st_mode);
            }
            if(onlydirs && !isdir)
                continue;
            end=strlen(path);
            if('/' == path[end-1]) {
                path[end-1]=0;
            }
            if (matchdot!=1 && (!strcmp(path, ".") || str_endswith(path, "/.")))
                continue;
            if (matchdot!=2 && (!strcmp(path, "..") || str_endswith(path, "/..")))
                continue;

            s = gensym(path);
            SETSYMBOL(outv+0, s);
            SETFLOAT(outv+1, isdir);
            outlet_list(x->x_dataout, gensym("list"), 2, outv);
        }
    }
    globfree(&gg);
}
#endif /* _WIN32 */


    /* ================ [file which] ====================== */

static void file_which_symbol(t_file_handle*x, t_symbol*s) {
        /* LATER we might output directories as well,... */
    int isdir=0;
    t_atom outv[2];
    char dirresult[MAXPDSTRING], *nameresult;
    int fd = canvas_open(x->x_canvas,
        s->s_name, "",
        dirresult, &nameresult, MAXPDSTRING,
        1);
    if(fd>=0) {
        sys_close(fd);
        if(nameresult>dirresult)
            nameresult[-1]='/';
        SETSYMBOL(outv+0, gensym(dirresult));
        SETFLOAT(outv+1, isdir);
        outlet_list(x->x_dataout, gensym("list"), 2, outv);
    } else {
        outlet_symbol(x->x_infoout, s);
    }
}


    /* ================ [file mkdir] ====================== */
static void file_mkdir_symbol(t_file_handle*x, t_symbol*dir) {
    char pathname[MAXPDSTRING], *path=pathname, *str;
    struct stat sb;

    do_expandpath(dir->s_name, pathname, MAXPDSTRING);
    pathname[MAXPDSTRING-1]=0;
    do_pathnormalize(pathname, pathname);

    if(sys_isabsolutepath(pathname)) {
        str=strchr(path, '/');
        if(str)
            path=str;
    }
    path++;
    while(*path) {
            /* get to the next path separator */
        str=strchr(path, '/');
        if(str) {
            *str=0;
        }
        if(!sys_stat(pathname, &sb) && (S_ISDIR(sb.st_mode))) {
                // directory exists, skip...
        } else {
            if(sys_mkdir(pathname, x?x->x_creationmode:0777)) {
                char buf[MAXPDSTRING];
                pd_error(x, "failed to create '%s': %s", pathname, do_errmsg(buf, MAXPDSTRING));
                outlet_bang(x->x_infoout);
                return;
            }
        }

        if(str) {
            *str='/';
            path=str+1;
        }
        else
            break;
    }
    outlet_symbol(x->x_dataout, gensym(pathname));
}

    /* ================ [file delete] ====================== */
#ifdef _WIN32
static int file_do_delete_recursive_ucs2(uint16_t*path) {
    WIN32_FIND_DATAW FindFileData;
    HANDLE hFind;
    uint16_t pattern[MAX_PATH];
    swprintf(pattern, MAX_PATH, L"%ls/*", path);
    hFind = FindFirstFileW(pattern, &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 1;
    }
    do {
        int isdir = !!(FILE_ATTRIBUTE_DIRECTORY & FindFileData.dwFileAttributes);
        swprintf(pattern, MAX_PATH, L"%ls/%ls", path, FindFileData.cFileName);
        if(!isdir) {
            DeleteFileW(pattern);
        } else {
                /* skip self and parent */
            if(!wcscmp(L".", FindFileData.cFileName))continue;
            if(!wcscmp(L"..", FindFileData.cFileName))continue;
            file_do_delete_recursive_ucs2(pattern);
        }
    } while (FindNextFileW(hFind, &FindFileData) != 0);
    FindClose(hFind);
    return do_delete_ucs2(path);
}

static int file_do_delete_recursive(const char*path) {
    uint16_t ucs2path[MAXPDSTRING];
    u8_utf8toucs2(ucs2path, MAXPDSTRING, path, MAXPDSTRING);
    return file_do_delete_recursive_ucs2(ucs2path);
}
#else /* !_WIN32 */
static int nftw_cb(const char *path, const struct stat *s, int flag, struct FTW *f) {
    (void)s;
    (void)f;
    (void)flag;
    return remove(path);
}

static int file_do_delete_recursive(const char*pathname) {
    return nftw(pathname, nftw_cb, 128, FTW_MOUNT|FTW_PHYS|FTW_DEPTH);
}
#endif /* !_WIN32 */


static void file_delete_symbol(t_file_handle*x, t_symbol*path) {
    char pathname[MAXPDSTRING];

    do_expandunbash(path->s_name, pathname, MAXPDSTRING);

    if(sys_remove(pathname)) {
        char buf[MAXPDSTRING];
        if(x && x->x_verbose)
            pd_error(x, "unable to delete '%s': %s", pathname, do_errmsg(buf, MAXPDSTRING));
        outlet_bang(x->x_infoout);
    } else {
        outlet_symbol(x->x_dataout, gensym(pathname));
    }
}

static void file_delete_recursive(t_file_handle*x, t_symbol*path) {
    char pathname[MAXPDSTRING];

    do_expandunbash(path->s_name, pathname, MAXPDSTRING);

    if(file_do_delete_recursive(pathname)) {
        if(x->x_verbose) {
            char buf[MAXPDSTRING];
            pd_error(x, "unable to recursively delete '%s': %s", pathname, do_errmsg(buf, MAXPDSTRING));
        }
        outlet_bang(x->x_infoout);
    } else {
        outlet_symbol(x->x_dataout, gensym(pathname));
    }
}


    /* ================ [file copy]/[file move] ====================== */
static int file_do_copy(const char*source, const char*destination, int mode) {
    int result = 0;
    ssize_t len;
    char buf[1024];
    int src, dst;
    int wflags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef _WIN32
    wflags |= O_BINARY;
#endif
    src = sys_open(source, O_RDONLY);
    if(src<0)
        return 1;
    dst = sys_open(destination, wflags, mode);
    if(dst<0) {
        struct stat sb;
            /* check if destination is a directory: if so calculate the new filename */
        if(!do_file_stat(0, destination, &sb, 0) && (S_ISDIR(sb.st_mode))) {
            char destfile[MAXPDSTRING];
            const char*filename=strrchr(source, '/');
            if(!filename)
                filename=source;
            else
                filename++;
            snprintf(destfile, MAXPDSTRING, "%s/%s", destination, filename);
            dst = sys_open(destfile, O_WRONLY | O_CREAT | O_TRUNC, mode);
        }
    }
    if(dst<0)
        return 1;

    while((len=read(src, buf, sizeof(buf)))>0) {
        ssize_t wlen=write(dst, buf, len);
            /* TODO: cater for partial writes */
        if (wlen<1) {
            result = 1 ;
        }
    }
    sys_close(src);
    sys_close(dst);

    return (result);
}
static int file_do_move(const char*source, const char*destination, int mode) {
    int olderrno=0;
    int result = sys_rename(source, destination);
    (void)mode;
    if(result) {
        struct stat sb;
        int isfile=0;
        int isdir=0;

        olderrno=errno;
            /* check whether we are trying to move a file to a directory */
        if(do_file_stat(0, source, &sb, 0) < 0)
            goto done; /* source is not statable, that's a serious error */
        isfile = !(S_ISDIR(sb.st_mode));
        if(do_file_stat(0, destination, &sb, 0) < 0)
            goto done;  /* destination is not statable (so it doesn't exist and is not a directory either */
        isdir = (S_ISDIR(sb.st_mode));
        if(isfile && isdir) {
            char destfile[MAXPDSTRING];
            const char*filename=strrchr(source, '/');
            if(!filename)
                filename=source;
            else
                filename++;
            snprintf(destfile, MAXPDSTRING, "%s/%s", destination, filename);
            result = sys_rename(source, destfile);
            olderrno = errno;
        }
    }

    if(result && EXDEV == errno) {
            /* need to manually copy the file to the another filesystem */
        result = file_do_copy(source, destination, mode);
        if(!result) {
            olderrno=0;
                /* copy succeeded, now get rid of the source file */
            if(sys_remove(source)) {
                    /* oops, couldn't delete the source-file...
                     * we still report this as SUCCESS, as the file has been duplicated.
                     * LATER we might try to unlink() the file first.
                     * set the olderrno to print some error in verbose-mode
                     */
                olderrno=errno;
            }
        }
    }
 done:
    errno=olderrno;
    return result;
}
static void file_do_copymove(t_file_handle*x,
    const char*verb, int (*fun)(const char*,const char*,int),
    t_symbol*s, int argc, t_atom*argv) {
    struct stat sb;
    char src[MAXPDSTRING], dst[MAXPDSTRING];
    if(argc != 2 || A_SYMBOL != argv[0].a_type || A_SYMBOL != argv[1].a_type) {
        pd_error(x, "bad arguments for [file %s] - should be 'source:symbol destination:symbol'", verb);
        return;
    }
    do_expandunbash(atom_getsymbol(argv+0)->s_name, src, MAXPDSTRING);
    do_expandunbash(atom_getsymbol(argv+1)->s_name, dst, MAXPDSTRING);

    if(!sys_stat(src, &sb)) {
        if(S_ISDIR(sb.st_mode)) {
            if(x->x_verbose) {
                pd_error(x, "failed to %s '%s': %s", verb, src, strerror(EISDIR));
            }
            outlet_bang(x->x_infoout);
            return;
        }
    }

    errno = 0;
    if(fun(src, dst, x->x_creationmode?x->x_creationmode:sb.st_mode)) {
        if(x->x_verbose) {
            char buf[MAXPDSTRING];
            pd_error(x, "failed to %s '%s' to '%s': %s", verb, src, dst, do_errmsg(buf, MAXPDSTRING));
        }
        outlet_bang(x->x_infoout);
    } else {
        if(errno && x->x_verbose) {
            char buf[MAXPDSTRING];
            pd_error(x, "troubles (but overall success) to %s '%s' to '%s': %s", verb, src, dst, do_errmsg(buf, MAXPDSTRING));
        }
        outlet_list(x->x_dataout, s, argc, argv);
    }
}

static void file_copy_list(t_file_handle*x, t_symbol*s, int argc, t_atom*argv) {
    file_do_copymove(x, "copy", file_do_copy, s, argc, argv);
}
static void file_move_list(t_file_handle*x, t_symbol*s, int argc, t_atom*argv) {
    file_do_copymove(x, "move", file_do_move, s, argc, argv);
}

/* ================ file path operations ====================== */
static void file_split_symbol(t_file_handle*x, t_symbol*path) {
    t_symbol*slashsym = gensym("/");
    t_atom*outv;
    int outc=0, outsize=1;
    char buffer[MAXPDSTRING], *pathname=buffer;
    sys_unbashfilename(path->s_name, buffer);
    buffer[MAXPDSTRING-1] = 0;

        /* first count the number of path components */
    while(*pathname)
        outsize += ('/'==*pathname++);
    pathname=buffer;
    ALLOCA(t_atom, outv, outsize, 100);

    if('/' == *pathname)
        SETSYMBOL(outv+outc, slashsym), outc++;

    while(*pathname) {
        char*pathsep;
        while('/' == *pathname)
            pathname++;
        pathsep=strchr(pathname, '/');
        if(!pathsep) {
            if(*pathname)
                SETSYMBOL(outv+outc, gensym(pathname)), outc++;
            break;
        }
        *pathsep=0;
        SETSYMBOL(outv+outc, gensym(pathname)), outc++;
        pathname=pathsep+1;
    }
    if (*pathname)
        outlet_bang(x->x_infoout);
    else
        outlet_symbol(x->x_infoout, slashsym);

    outlet_list(x->x_dataout, gensym("list"), outc, outv);

    FREEA(t_atom, outv, outsize, 100);
}

static void file_join_list(t_file_handle*x, t_symbol*s, int argc, t_atom*argv) {
        /* luckily for us, the path-separator in Pd is always '/' */
    size_t bufsize = 1;
    char*buffer = getbytes(bufsize);
    (void)s;
    while(argc--) {
        size_t newsize;
        int needsep=(argc>0);
        char abuf[MAXPDSTRING], *newbuffer;
        size_t alen;
        atom_string(argv++, abuf, MAXPDSTRING);
        alen = strlen(abuf);
        if(!alen) continue;
        if('/' == abuf[alen-1])
            needsep = 0;
        newsize = bufsize + alen + needsep;
        if (!(newbuffer = resizebytes(buffer, bufsize, newsize))) break;
        buffer = newbuffer;
        strcpy(buffer+bufsize-1, abuf);
        if(needsep)
            buffer[newsize-2]='/';
        bufsize = newsize;
    }
    buffer[bufsize-1] = 0;
    outlet_symbol(x->x_dataout, gensym(do_pathnormalize(buffer, buffer)));
    freebytes(buffer, bufsize);
}

static void file_splitext_symbol(t_file_handle*x, t_symbol*path) {
    char pathname[MAXPDSTRING];
    t_atom outv[2];
    char*str;
    sys_unbashfilename(path->s_name, pathname);
    pathname[MAXPDSTRING-1]=0;
    str=pathname + strlen(pathname)-1;
    if(str < pathname || '.' != *str)
    {
        while(str>=pathname) {
            char c = *str;
            switch(c) {
            case '.':
                str[0]=0;
                SETSYMBOL(outv+0, gensym(pathname));
                SETSYMBOL(outv+1, gensym(str+1));
                outlet_list(x->x_dataout, gensym("list"), 2, outv);
                return;
            case '/':
                str = pathname;
                break;
            default:
                break;
            }
            str--;
        }
    }
    outlet_symbol(x->x_infoout, gensym(pathname));
}
static void file_splitname_symbol(t_file_handle*x, t_symbol*path) {
    char pathname[MAXPDSTRING];
    char*str;
    sys_unbashfilename(path->s_name, pathname);
    pathname[MAXPDSTRING-1]=0;
    str=strrchr(pathname, '/');
    if(str>pathname) {
        t_symbol*s;
        *str++=0;
        s = gensym(pathname);
        if(*str) {
            t_atom outv[2];
            SETSYMBOL(outv+0, s);
            SETSYMBOL(outv+1, gensym(str));
            outlet_list(x->x_dataout, gensym("list"), 2, outv);
        } else {
            outlet_symbol(x->x_dataout, s);
        }
    } else {
        outlet_symbol(x->x_infoout, gensym(pathname));
    }
}


    /* overall creator for "file" objects - dispatch to "file handle" etc */
t_class *file_handle_class, *file_which_class, *file_glob_class;
t_class *file_stat_class, *file_size_class, *file_isfile_class, *file_isdirectory_class;
t_class *file_mkdir_class, *file_delete_class, *file_copy_class, *file_move_class;
t_class *file_split_class,*file_join_class,*file_splitext_class, *file_splitname_class;

#define FILE_PD_NEW(verb, verbose, creationmode) static t_file_handle* file_##verb##_new(t_symbol*s, int argc, t_atom*argv) \
    {                                                                   \
        return do_file_handle_new(file_##verb##_class, s, argc, argv, verbose, creationmode); \
    }

static void file_define_ignore(t_file_handle*x, t_symbol*s, int argc, t_atom*argv) {
        /* this is a noop (so the object does not bail out if somebody 'send's something to its label) */
    (void)s;
    (void)argc;
    (void)argv;
}
static t_file_handle*file_define_new(t_symbol*s, int argc, t_atom*argv) {
   t_file_handle*x = (t_file_handle*)pd_new(file_define_class);
   x->x_fhptr = &x->x_fh;
   x->x_fh.fh_fd = -1;
   x->x_canvas = canvas_getcurrent();
   x->x_creationmode = 0666;
   x->x_verbose = 0;
   if(1 == argc && A_SYMBOL == argv->a_type) {
       x->x_fcname = atom_getsymbol(argv);
       pd_bind(&x->x_obj.ob_pd, x->x_fcname);
   } else {
       pd_error(x, "%s requires an argument: handle name", s->s_name);
   }
   return x;
}
static void file_define_free(t_file_handle*x) {
    file_handle_close(x);
    if(x->x_fcname)
        pd_unbind(&x->x_obj.ob_pd, x->x_fcname);
}

static t_file_handle*file_handle_new(t_symbol*s, int argc, t_atom*argv) {
    t_file_handle*x=do_file_handle_new(file_handle_class, s, argc, argv, 1, 0666);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("set"));
    return x;
}

FILE_PD_NEW(which, 0, 0);
FILE_PD_NEW(glob, 0, 0);

FILE_PD_NEW(stat, 0, 0);
FILE_PD_NEW(size, 0, 0);
FILE_PD_NEW(isfile, 0, 0);
FILE_PD_NEW(isdirectory, 0, 0);

FILE_PD_NEW(mkdir, 0, 0777);
FILE_PD_NEW(delete, 0, 0);
FILE_PD_NEW(copy, 0, 0);
FILE_PD_NEW(move, 0, 0);

FILE_PD_NEW(split, 0, 0);
FILE_PD_NEW(join, 0, 0);
FILE_PD_NEW(splitext, 0, 0);
FILE_PD_NEW(splitname, 0, 0);

static t_pd *fileobj_new(t_symbol *s, int argc, t_atom*argv)
{
    t_file_handle*x = 0;
    const char*verb=0;
    if(gensym("file") == s) {
        if (argc && A_SYMBOL == argv->a_type) {
            verb = atom_getsymbol(argv)->s_name;
            argc--;
            argv++;
        } else {
            verb = "handle";
        }
    } else if (strlen(s->s_name)>5) {
        verb = s->s_name + 5;
    }
    if (!verb || !*verb)
        x = do_file_handle_new(file_handle_class, gensym("file handle"), argc, argv, 1, 0666);
    else {
#define ELIF_FILE_PD_NEW(name, verbose, creationmode) else              \
            if (!strcmp(verb, #name))                                   \
                x = do_file_handle_new(file_##name##_class, gensym("file "#name), argc, argv, verbose, creationmode)

        if (!strcmp(verb, "define"))
            x = file_define_new(gensym("file define"), argc, argv);
        else if (!strcmp(verb, "handle"))
            x = file_handle_new(gensym("file handle"), argc, argv);
        ELIF_FILE_PD_NEW(handle, 1, 0666);
        ELIF_FILE_PD_NEW(which, 0, 0);
        ELIF_FILE_PD_NEW(glob, 0, 0);
        ELIF_FILE_PD_NEW(stat, 0, 0);
        ELIF_FILE_PD_NEW(size, 0, 0);
        ELIF_FILE_PD_NEW(isfile, 0, 0);
        ELIF_FILE_PD_NEW(isdirectory, 0, 0);
        ELIF_FILE_PD_NEW(mkdir, 0, 0777);
        ELIF_FILE_PD_NEW(delete, 0, 0);
        ELIF_FILE_PD_NEW(copy, 0, 0);
        ELIF_FILE_PD_NEW(move, 0, 0);
        ELIF_FILE_PD_NEW(split, 0, 0);
        ELIF_FILE_PD_NEW(join, 0, 0);
        ELIF_FILE_PD_NEW(splitext, 0, 0);
        ELIF_FILE_PD_NEW(splitname, 0, 0);
        else {
            pd_error(0, "file %s: unknown function", verb);
        }
    }
    return (t_pd*)x;
}

typedef enum _filenew_flag {
    DEFAULT = 0,
    VERBOSE = 1<<0,
    MODE = 1<<1
} t_filenew_flag;

static t_class*file_class_new(const char*name
    , t_file_handle* (*ctor)(t_symbol*,int,t_atom*), void (*dtor)(t_file_handle*)
    , void (*symfun)(t_file_handle*, t_symbol*)
    , t_filenew_flag flag
    ) {
    t_class*cls = class_new(gensym(name),
        (t_newmethod)ctor, (t_method)dtor,
        sizeof(t_file_handle), 0, A_GIMME, 0);
    if (flag & VERBOSE)
        class_addmethod(cls, (t_method)file_set_verbosity, gensym("verbose"), A_FLOAT, 0);
    if (flag & MODE)
        class_addmethod(cls, (t_method)file_set_creationmode, gensym("creationmode"), A_GIMME, 0);
    if(symfun)
        class_addsymbol(cls, (t_method)symfun);

    class_sethelpsymbol(cls, gensym("file"));
    return cls;
}

    /* ---------------- global setup function -------------------- */
void x_file_setup(void)
{
    class_addcreator((t_newmethod)fileobj_new, gensym("file"), A_GIMME, 0);

        /* [file define] */
    file_define_class = class_new(gensym("file define"),
                                  (t_newmethod)file_define_new, (t_method)file_define_free,
                                  sizeof(t_file_handle), CLASS_NOINLET, A_GIMME, 0);
    class_addanything(file_define_class, file_define_ignore);
    class_sethelpsymbol(file_define_class, gensym("file"));

        /* [file handle] */
    file_handle_class = file_class_new("file handle", file_handle_new, file_handle_free, 0, MODE|VERBOSE);
    class_addmethod(file_handle_class, (t_method)file_handle_open,
        gensym("open"), A_SYMBOL, A_DEFSYM, 0);
    class_addmethod(file_handle_class, (t_method)file_handle_close,
        gensym("close"), 0);
    class_addmethod(file_handle_class, (t_method)file_handle_seek,
        gensym("seek"), A_GIMME, 0);
    class_addmethod(file_handle_class, (t_method)file_handle_set,
        gensym("set"), A_DEFSYMBOL, 0);
    class_addlist(file_handle_class, file_handle_list);

        /* [file which] */
    file_which_class = file_class_new("file which", file_which_new, 0, file_which_symbol, VERBOSE);

        /* [file glob] */
    file_glob_class = file_class_new("file glob", file_glob_new, 0, file_glob_symbol, VERBOSE);

        /* [file stat] */
    file_stat_class = file_class_new("file stat", file_stat_new, 0, file_stat_symbol, VERBOSE);
    file_size_class = file_class_new("file size", file_size_new, 0, file_size_symbol, VERBOSE);
    file_isfile_class = file_class_new("file isfile", file_isfile_new, 0, file_isfile_symbol, VERBOSE);
    file_isdirectory_class = file_class_new("file isdirectory", file_isdirectory_new, 0, file_isdirectory_symbol, VERBOSE);

        /* [file mkdir] */
    file_mkdir_class = file_class_new("file mkdir", file_mkdir_new, 0, file_mkdir_symbol, MODE|VERBOSE);

        /* [file delete] */
    file_delete_class = file_class_new("file delete", file_delete_new, 0, file_delete_symbol, VERBOSE);
    class_addmethod(file_delete_class, (t_method)file_delete_recursive,
        gensym("recursive"), A_SYMBOL, 0);

        /* [file copy] */
    file_copy_class = file_class_new("file copy", file_copy_new, 0, 0, MODE|VERBOSE);
    class_addlist(file_copy_class, (t_method)file_copy_list);

        /* [file move] */
    file_move_class = file_class_new("file move", file_move_new, 0, 0, MODE|VERBOSE);
    class_addlist(file_move_class, (t_method)file_move_list);

        /* file path objects */
    file_split_class = file_class_new("file split", file_split_new, 0, file_split_symbol, DEFAULT);
    file_join_class = file_class_new("file join", file_join_new, 0, 0, DEFAULT);
    class_addlist(file_join_class, (t_method)file_join_list);
    file_splitext_class = file_class_new("file splitext", file_splitext_new, 0, file_splitext_symbol, DEFAULT);
    file_splitname_class = file_class_new("file splitname", file_splitname_new, 0, file_splitname_symbol, DEFAULT);
}
