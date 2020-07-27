/* Copyright (c) 2020 Andi McClure.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "s_fileops.h"

static bool null_open(const char *path, t_fileops_flags flags, t_fileops_handle *handle) {
    return false;
}
static bool null_close(t_fileops_handle handle) {
    return false;
}
static bool null_stat(t_fileops_handle handle, t_fileops_stat *stat) {
    return false;
}
static int64_t null_seek(t_fileops_handle handle, int64_t offset, t_fileops_flags flags) {
    return 0;
}
static ssize_t null_read(t_fileops_handle handle, void *buf, size_t nbyte) {
    return 0;
}
static ssize_t null_write(t_fileops_handle handle, const void *buf, size_t nbyte) {
    return 0;
}
static ssize_t null_scanf(t_fileops_handle handle, const char * restrict format, ...) {
    return 0;
}
static ssize_t null_vscanf(t_fileops_handle handle, const char * restrict format, va_list ap) {
    return 0;
}
static ssize_t null_printf(t_fileops_handle handle, const char * restrict format, ...) {
    return 0;
}
static ssize_t null_vprintf(t_fileops_handle handle, const char * restrict format, va_list ap) {
    return 0;
}
static bool null_flush(t_fileops_handle handle) {
    return false;
}

#define FILEOPS_NULL_VALUES {null_open, null_close, null_stat, null_seek, null_read, null_write, null_scanf, null_vscanf, null_printf, null_vprintf}

const t_fileops sys_fileops_null = FILEOPS_NULL_VALUES;

#ifndef _PD_METAFILE_NO_DEFAULT

#include <fcntl.h>
#include <sys/stat.h>

#define SYS_TOHANDLE(x) ((intptr_t)(void *)(x))
#define SYS_FROMHANDLE(x) ((FILE *)(void *)(x))

#ifdef _LARGEFILE64_SOURCE
# define open  open64
# define lseek lseek64
# define fstat fstat64
# define stat  stat64
#error TODO
#endif

static void std_flags_to_fopen_string(char *mode, t_fileops_flags flags, bool *doreopen) {
    int midx = 0;
    if (!(flags & (FILEOPS_WRITE | FILEOPS_READ)))
        flags = flags | FILEOPS_WRITE | FILEOPS_READ;
    // CREAT does not mesh precisely with the fopen concept.
    if ((flags & FILEOPS_WRITE) && !(flags & FILEOPS_CREAT)) {
        mode[0] = 'r';
        mode[1] = '+';
        midx = 2;
        if (doreopen)
            *doreopen = true;
    } else {
        if (flags & FILEOPS_READ) {
            mode[midx] = 'r';
            midx++;
        }
        if (flags & FILEOPS_WRITE) {
            mode[midx] = midx>0?'+':'w';
            midx++;
        }
        if (doreopen)
            *doreopen = false;
    }
    mode[midx++] = '\0';
}

// FIXME: This doesn't correctly recreate the O_CREAT behavior; a better way to do this would be fdopen/_fdopen
static bool std_open(const char *path, t_fileops_flags flags, t_fileops_handle *handle) {
    char mode[3];
    bool doreopen;

    std_flags_to_fopen_string(mode, flags, &doreopen);

// IF BODY PD CODE
#ifdef _WIN32
    char namebuf[MAXPDSTRING];
    wchar_t ucs2buf[MAXPDSTRING];
    wchar_t ucs2mode[MAXPDSTRING];
    sys_bashfilename(path, namebuf);
    u8_utf8toucs2(ucs2buf, MAXPDSTRING, namebuf, MAXPDSTRING-1);
    /* mode only uses ASCII, so no need for a full conversion, just copy it */
    mbstowcs(ucs2mode, mode, MAXPDSTRING);
    FILE * f = _wfopen(ucs2buf, ucs2mode);
#else
  char namebuf[MAXPDSTRING];
  sys_bashfilename(path, namebuf);
  FILE *f = fopen(namebuf, mode);
#endif

  if (f && doreopen) {
    std_flags_to_fopen_string(mode, flags & ~(FILEOPS_CREAT), NULL);
    f = freopen(path, mode, f);
  }

  *handle = (intptr_t)(void *)f;
  return tobool(f);
}
static bool std_close(t_fileops_handle handle) {
    FILE *f = (FILE *)(void *)handle;
    int result = fclose(f);
    return 0 == result;
}

static bool std_stat(t_fileops_handle handle, t_fileops_stat *stat) {
#ifdef _WIN32
    // On Windows fopen fails on directories, so we always know the answer by this point is no.
    stat->isdir = false;
    stat->isdir_known = true;
#else
    FILE *f = SYS_FROMHANDLE(handle);
    int fd = fileno(f);
    struct stat data;
    if (fstat(fd, &data))
        return false;
    stat->isdir = tobool(data.st_mode & S_IFDIR);
    stat->isdir_known = true;
#endif
    return true;
}
static int64_t std_seek(t_fileops_handle handle, int64_t offset, t_fileops_flags flags) {
    FILE *f = SYS_FROMHANDLE(handle);
    int whence = 0;
    if (flags & FILEOPS_SEEK_SET)
        whence = SEEK_SET;
    else if (flags & FILEOPS_SEEK_CUR)
        whence = SEEK_CUR;
    else if (flags & FILEOPS_SEEK_END)
        whence = SEEK_END;
    int64_t result = fseek(f, offset, whence);
    if (result<0)
        return -1;
    return ftell(f);
}
static ssize_t std_read(t_fileops_handle handle, void *buf, size_t nbyte) {
    FILE *f = SYS_FROMHANDLE(handle);
    return fread(buf, 1, nbyte, f);
}
static ssize_t std_write(t_fileops_handle handle, const void *buf, size_t nbyte) {
    FILE *f = SYS_FROMHANDLE(handle);
    return fwrite(buf, 1, nbyte, f);
}
static ssize_t std_scanf(t_fileops_handle handle, const char * restrict format, ...) {
    FILE *f = SYS_FROMHANDLE(handle);
    va_list ap;
    int result;

    va_start(ap, format);
    result = vfscanf(f, format, ap);
    va_end(ap);

    return result;
}
static ssize_t std_vscanf(t_fileops_handle handle, const char * restrict format, va_list ap) {
    FILE *f = SYS_FROMHANDLE(handle);
    return vfscanf(f, format, ap);
}
static ssize_t std_printf(t_fileops_handle handle, const char * restrict format, ...) {
    FILE *f = SYS_FROMHANDLE(handle);
    va_list ap;
    int result;

    va_start(ap, format);
    result = vfprintf(f, format, ap);
    va_end(ap);

    return result;
}
static ssize_t std_vprintf(t_fileops_handle handle, const char * restrict format, va_list ap) {
    FILE *f = SYS_FROMHANDLE(handle);
    return vfprintf(f, format, ap);
}
static bool std_flush(t_fileops_handle handle) {
    FILE *f = SYS_FROMHANDLE(handle);
    return !fflush(f);
}

#define FILEOPS_STD_VALUES {std_open, std_close, std_stat, std_seek, std_read, std_write, std_scanf, std_vscanf, std_printf, std_vprintf}

t_fileops sys_fileops_standard = FILEOPS_STD_VALUES;

t_fileops sys_fileops = FILEOPS_STD_VALUES;

#else

t_fileops sys_fileops = FILEOPS_NULL_VALUES;

#endif