/* Copyright (c) 1999 Guenter Geiger and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*
 * This file implements the loader for linux, which includes
 * a little bit of path handling.
 *
 * Generalized by MSP to provide an open_via_path function
 * and lists of files for all purposes.
 */ 

/* #define DEBUG(x) x */
#define DEBUG(x)

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

#ifdef HAVE_ALLOCA_H        /* ifdef nonsense to find include for alloca() */
# include <alloca.h>        /* linux, mac, mingw, cygwin */
#elif defined _MSC_VER
# include <malloc.h>        /* MSVC */
#else
# include <stddef.h>        /* BSDs for example */
#endif                      /* end alloca() ifdef nonsense */

#include <string.h>
#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include "s_utf8.h"
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#ifdef _LARGEFILE64_SOURCE
# define open  open64
# define lseek lseek64
# define fstat fstat64
# define stat  stat64
#endif

t_namelist *sys_externlist;
t_namelist *sys_searchpath;
t_namelist *sys_staticpath;
t_namelist *sys_helppath;

    /* change '/' characters to the system's native file separator */
void sys_bashfilename(const char *from, char *to)
{
    char c;
    while (c = *from++)
    {
#ifdef _WIN32
        if (c == '/') c = '\\';
#endif
        *to++ = c;
    }
    *to = 0;
}

    /* change the system's native file separator to '/' characters  */
void sys_unbashfilename(const char *from, char *to)
{
    char c;
    while (c = *from++)
    {
#ifdef _WIN32
        if (c == '\\') c = '/';
#endif
        *to++ = c;
    }
    *to = 0;
}

/* test if path is absolute or relative, based on leading /, env vars, ~, etc */
int sys_isabsolutepath(const char *dir)
{
    if (dir[0] == '/' || dir[0] == '~'
#ifdef _WIN32
        || dir[0] == '%' || (dir[1] == ':' && dir[2] == '/')
#endif
        )
    {
        return 1;
    }
    else
    {
        return 0;            
    }
}

/* expand env vars and ~ at the beginning of a path and make a copy to return */
static void sys_expandpath(const char *from, char *to, int bufsize)
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
}

/*******************  Utility functions used below ******************/

/*!
 * \brief copy until delimiter
 * 
 * \arg to destination buffer
 * \arg to_len destination buffer length
 * \arg from source buffer
 * \arg delim string delimiter to stop copying on
 *
 * \return position after delimiter in string.  If it was the last
 *         substring, return NULL.
 */
static const char *strtokcpy(char *to, size_t to_len, const char *from, char delim)
{
    unsigned int i = 0;

        for (; i < (to_len - 1) && from[i] && from[i] != delim; i++)
                to[i] = from[i];
        to[i] = '\0';

        if (i && from[i] != '\0')
                return from + i + 1;

        return NULL;
}

/* add a single item to a namelist.  If "allowdup" is true, duplicates
may be added; othewise they're dropped.  */

t_namelist *namelist_append(t_namelist *listwas, const char *s, int allowdup)
{
    t_namelist *nl, *nl2;
    nl2 = (t_namelist *)(getbytes(sizeof(*nl)));
    nl2->nl_next = 0;
    nl2->nl_string = (char *)getbytes(strlen(s) + 1);
    strcpy(nl2->nl_string, s);
    sys_unbashfilename(nl2->nl_string, nl2->nl_string);
    if (!listwas)
        return (nl2);
    else
    {
        for (nl = listwas; ;)
        {
            if (!allowdup && !strcmp(nl->nl_string, s))
                return (listwas);
            if (!nl->nl_next)
                break;
            nl = nl->nl_next;
        }
        nl->nl_next = nl2;
    }
    return (listwas);
}

/* add a colon-separated list of names to a namelist */

#ifdef _WIN32
#define SEPARATOR ';'   /* in MSW the natural separator is semicolon instead */
#else
#define SEPARATOR ':'
#endif

t_namelist *namelist_append_files(t_namelist *listwas, const char *s)
{
    const char *npos;
    char temp[MAXPDSTRING];
    t_namelist *nl = listwas, *rtn = listwas;
    
    npos = s;
    do
    {
        npos = strtokcpy(temp, sizeof(temp), npos, SEPARATOR);
        if (! *temp) continue;
        nl = namelist_append(nl, temp, 0);
    }
        while (npos);
    return (nl);
}

void namelist_free(t_namelist *listwas)
{
    t_namelist *nl, *nl2;
    for (nl = listwas; nl; nl = nl2)
    {
        nl2 = nl->nl_next;
        t_freebytes(nl->nl_string, strlen(nl->nl_string) + 1);
        t_freebytes(nl, sizeof(*nl));
    }
}

char *namelist_get(t_namelist *namelist, int n)
{
    int i;
    t_namelist *nl;
    for (i = 0, nl = namelist; i < n && nl; i++, nl = nl->nl_next)
        ;
    return (nl ? nl->nl_string : 0);
}

int sys_usestdpath = 1;

void sys_setextrapath(const char *p)
{
    char pathbuf[MAXPDSTRING];
    namelist_free(sys_staticpath);
    /* add standard place for users to install stuff first */
#ifdef __gnu_linux__
    sys_expandpath("~/pd-externals", pathbuf, MAXPDSTRING);
    sys_staticpath = namelist_append(0, pathbuf, 0);
    sys_staticpath = namelist_append(sys_staticpath, "/usr/local/lib/pd-externals", 0);
#endif

#ifdef __APPLE__
    sys_expandpath("~/Library/Pd", pathbuf, MAXPDSTRING);
    sys_staticpath = namelist_append(0, pathbuf, 0);
    sys_staticpath = namelist_append(sys_staticpath, "/Library/Pd", 0);
#endif

#ifdef _WIN32
    sys_expandpath("%ProgramFiles%/Common Files/Pd", pathbuf, MAXPDSTRING);
    sys_staticpath = namelist_append(0, pathbuf, 0);
    sys_expandpath("%UserProfile%/Application Data/Pd", pathbuf, MAXPDSTRING);
    sys_staticpath = namelist_append(sys_staticpath, pathbuf, 0);
#endif
    /* add built-in "extra" path last so its checked last */
    sys_staticpath = namelist_append(sys_staticpath, p, 0);
}

    /* try to open a file in the directory "dir", named "name""ext",
    for reading.  "Name" may have slashes.  The directory is copied to
    "dirresult" which must be at least "size" bytes.  "nameresult" is set
    to point to the filename (copied elsewhere into the same buffer). 
    The "bin" flag requests opening for binary (which only makes a difference
    on Windows). */

int sys_trytoopenone(const char *dir, const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin)
{
    int fd;
    char buf[MAXPDSTRING];
    if (strlen(dir) + strlen(name) + strlen(ext) + 4 > size)
        return (-1);
    sys_expandpath(dir, buf, MAXPDSTRING);
    strcpy(dirresult, buf);
    if (*dirresult && dirresult[strlen(dirresult)-1] != '/')
        strcat(dirresult, "/");
    strcat(dirresult, name);
    strcat(dirresult, ext);

    DEBUG(post("looking for %s",dirresult));
        /* see if we can open the file for reading */
    if ((fd=sys_open(dirresult, O_RDONLY)) >= 0)
    {
            /* in unix, further check that it's not a directory */
#ifdef HAVE_UNISTD_H
        struct stat statbuf;
        int ok =  ((fstat(fd, &statbuf) >= 0) &&
            !S_ISDIR(statbuf.st_mode));
        if (!ok)
        {
            if (sys_verbose) post("tried %s; stat failed or directory",
                dirresult);
            close (fd);
            fd = -1;
        }
        else
#endif
        {
            char *slash;
            if (sys_verbose) post("tried %s and succeeded", dirresult);
            sys_unbashfilename(dirresult, dirresult);
            slash = strrchr(dirresult, '/');
            if (slash)
            {
                *slash = 0;
                *nameresult = slash + 1;
            }
            else *nameresult = dirresult;

            return (fd);  
        }
    }
    else
    {
        if (sys_verbose) post("tried %s and failed", dirresult);
    }
    return (-1);
}

    /* check if we were given an absolute pathname, if so try to open it
    and return 1 to signal the caller to cancel any path searches */
int sys_open_absolute(const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin, int *fdp)
{
    if (sys_isabsolutepath(name))
    {
        char dirbuf[MAXPDSTRING], *z = strrchr(name, '/');
        int dirlen;
        if (!z)
            return (0);
        dirlen = z - name;
        if (dirlen > MAXPDSTRING-1) 
            dirlen = MAXPDSTRING-1;
        strncpy(dirbuf, name, dirlen);
        dirbuf[dirlen] = 0;
        *fdp = sys_trytoopenone(dirbuf, name+(dirlen+1), ext,
            dirresult, nameresult, size, bin);
        return (1);
    }
    else return (0);
}

/* search for a file in a specified directory, then along the globally
defined search path, using ext as filename extension.  The
fd is returned, the directory ends up in the "dirresult" which must be at
least "size" bytes.  "nameresult" is set to point to the filename, which
ends up in the same buffer as dirresult.  Exception:
if the 'name' starts with a slash or a letter, colon, and slash in MSW,
there is no search and instead we just try to open the file literally.  */

/* see also canvas_open() which, in addition, searches down the
canvas-specific path. */

static int do_open_via_path(const char *dir, const char *name,
    const char *ext, char *dirresult, char **nameresult, unsigned int size,
    int bin, t_namelist *searchpath)
{
    t_namelist *nl;
    int fd = -1;

        /* first check if "name" is absolute (and if so, try to open) */
    if (sys_open_absolute(name, ext, dirresult, nameresult, size, bin, &fd))
        return (fd);
    
        /* otherwise "name" is relative; try the directory "dir" first. */
    if ((fd = sys_trytoopenone(dir, name, ext,
        dirresult, nameresult, size, bin)) >= 0)
            return (fd);

        /* next go through the search path */
    for (nl = searchpath; nl; nl = nl->nl_next)
        if ((fd = sys_trytoopenone(nl->nl_string, name, ext,
            dirresult, nameresult, size, bin)) >= 0)
                return (fd);

        /* next look in built-in paths like "extra" */
    if (sys_usestdpath)
        for (nl = sys_staticpath; nl; nl = nl->nl_next)
            if ((fd = sys_trytoopenone(nl->nl_string, name, ext,
                dirresult, nameresult, size, bin)) >= 0)
                    return (fd);

    *dirresult = 0;
    *nameresult = dirresult;
    return (-1);
}

    /* open via path, using the global search path. */
int open_via_path(const char *dir, const char *name, const char *ext,
    char *dirresult, char **nameresult, unsigned int size, int bin)
{
    return (do_open_via_path(dir, name, ext, dirresult, nameresult,
        size, bin, sys_searchpath));
}

    /* open a file with a UTF-8 filename
    This is needed because WIN32 does not support UTF-8 filenames, only UCS2.
    Having this function prevents lots of #ifdefs all over the place.
    */
#ifdef _WIN32
int sys_open(const char *path, int oflag, ...)
{
    int i, fd;
    char pathbuf[MAXPDSTRING];
    wchar_t ucs2path[MAXPDSTRING];
    sys_bashfilename(path, pathbuf);
    u8_utf8toucs2(ucs2path, MAXPDSTRING, pathbuf, MAXPDSTRING-1);
    /* For the create mode, Win32 does not have the same possibilities,
     * so we ignore the argument and just hard-code read/write. */
    if (oflag & O_CREAT)
        fd = _wopen(ucs2path, oflag, _S_IREAD | _S_IWRITE);
    else
        fd = _wopen(ucs2path, oflag);
    return fd;
}

FILE *sys_fopen(const char *filename, const char *mode)
{
    char namebuf[MAXPDSTRING];
    wchar_t ucs2buf[MAXPDSTRING];
    wchar_t ucs2mode[MAXPDSTRING];
    sys_bashfilename(filename, namebuf);
    u8_utf8toucs2(ucs2buf, MAXPDSTRING, namebuf, MAXPDSTRING-1);
    /* mode only uses ASCII, so no need for a full conversion, just copy it */
    mbstowcs(ucs2mode, mode, MAXPDSTRING);
    return (_wfopen(ucs2buf, ucs2mode));
}
#else
#include <stdarg.h>
int sys_open(const char *path, int oflag, ...)
{
    int i, fd;
    char pathbuf[MAXPDSTRING];
    sys_bashfilename(path, pathbuf);
    if (oflag & O_CREAT)
    {
        mode_t mode;
        int imode;
        va_list ap;
        va_start(ap, oflag);

        /* Mac compiler complains if we just set mode = va_arg ... so, even
        though we all know it's just an int, we explicitly va_arg to an int
        and then convert.
           -> http://www.mail-archive.com/bug-gnulib@gnu.org/msg14212.html
           -> http://bugs.debian.org/647345
        */
        
        imode = va_arg (ap, int);
        mode = (mode_t)imode;
        va_end(ap);
        fd = open(pathbuf, oflag, mode);
    }
    else
        fd = open(pathbuf, oflag);
    return fd;
}

FILE *sys_fopen(const char *filename, const char *mode)
{
  char namebuf[MAXPDSTRING];
  sys_bashfilename(filename, namebuf);
  return fopen(namebuf, mode);
}
#endif /* _WIN32 */

   /* close a previously opened file
   this is needed on platforms where you cannot open/close resources
   across dll-boundaries, but we provide it for other platforms as well */
int sys_close(int fd)
{
#ifdef _WIN32
    return _close(fd);  /* Bill Gates is a big fat hen */
#else
    return close(fd);
#endif
}

int sys_fclose(FILE *stream)
{
    return fclose(stream);
}


    /* Open a help file using the help search path.  We expect the ".pd"
    suffix here, even though we have to tear it back off for one of the
    search attempts. */
void open_via_helppath(const char *name, const char *dir)
{
    char realname[MAXPDSTRING], dirbuf[MAXPDSTRING], *basename;
        /* make up a silly "dir" if none is supplied */
    const char *usedir = (*dir ? dir : "./");
    int fd;

        /* 1. "objectname-help.pd" */
    strncpy(realname, name, MAXPDSTRING-10);
    realname[MAXPDSTRING-10] = 0;
    if (strlen(realname) > 3 && !strcmp(realname+strlen(realname)-3, ".pd"))
        realname[strlen(realname)-3] = 0;
    strcat(realname, "-help.pd");
    if ((fd = do_open_via_path(usedir, realname, "", dirbuf, &basename, 
        MAXPDSTRING, 0, sys_helppath)) >= 0)
            goto gotone;

        /* 2. "help-objectname.pd" */
    strcpy(realname, "help-");
    strncat(realname, name, MAXPDSTRING-10);
    realname[MAXPDSTRING-1] = 0;
    if ((fd = do_open_via_path(usedir, realname, "", dirbuf, &basename, 
        MAXPDSTRING, 0, sys_helppath)) >= 0)
            goto gotone;

        /* 3. "objectname.pd" */
    if ((fd = do_open_via_path(usedir, name, "", dirbuf, &basename, 
        MAXPDSTRING, 0, sys_helppath)) >= 0)
            goto gotone;
    post("sorry, couldn't find help patch for \"%s\"", name);
    return;
gotone:
    close (fd);
    glob_evalfile(0, gensym((char*)basename), gensym(dirbuf));
}


/* Startup file reading for linux and __APPLE__.  As of 0.38 this will be
deprecated in favor of the "settings" mechanism */

int sys_argparse(int argc, char **argv);

#ifndef _WIN32

#define STARTUPNAME ".pdrc"
#define NUMARGS 1000

int sys_rcfile(void)
{
    FILE* file;
    int i;
    int k;
    int rcargc;
    char* rcargv[NUMARGS];
    char* buffer;
    char  fname[MAXPDSTRING], buf[1000], *home = getenv("HOME");
    int retval = 1; /* that's what we will return at the end; for now, let's think it'll be an error */
 
    /* initialize rc-arg-array so we can safely clean up at the end */
    for (i = 1; i < NUMARGS-1; i++)
      rcargv[i]=0;


    /* parse a startup file */
    
    *fname = '\0'; 

    strncat(fname, home? home : ".", MAXPDSTRING-10);
    strcat(fname, "/");

    strcat(fname, STARTUPNAME);

    if (!(file = fopen(fname, "r")))
        return 1;

    post("reading startup file: %s", fname);

    rcargv[0] = ".";    /* this no longer matters to sys_argparse() */

    for (i = 1; i < NUMARGS-1; i++)
    {
        if (fscanf(file, "%998s", buf) < 0)
            break;
        buf[999] = 0;
        if (!(rcargv[i] = malloc(strlen(buf) + 1)))
            goto cleanup;
        strcpy(rcargv[i], buf);
    }
    if (i >= NUMARGS-1)
        fprintf(stderr, "startup file too long; extra args dropped\n");
    rcargv[i] = 0;

    rcargc = i;

    /* parse the options */

    fclose(file);
    if (sys_verbose)
    {
        if (rcargc)
        {
            post("startup args from RC file:");
            for (i = 1; i < rcargc; i++)
                post("%s", rcargv[i]);
        }
        else post("no RC file arguments found");
    }
    if (sys_argparse(rcargc-1, rcargv+1))
    {
        error("error parsing RC arguments");
        goto cleanup;
    }

    retval=0; /* we made it without an error */


 cleanup: /* prevent memleak */
    for (i = 1; i < NUMARGS-1; i++)
      if(rcargv[i])free(rcargv[i]);
    
    return(retval);
}
#endif /* _WIN32 */

void sys_doflags( void)
{
    int i, beginstring = 0, state = 0, len = strlen(sys_flags->s_name);
    int rcargc = 0;
    char *rcargv[MAXPDSTRING];
    if (len > MAXPDSTRING)
    {
        error("flags: %s: too long", sys_flags->s_name);
        return;
    }
    for (i = 0; i < len+1; i++)
    {
        int c = sys_flags->s_name[i];
        if (state == 0)
        {
            if (c && !isspace(c))
            {
                beginstring = i;
                state = 1;
            }
        }
        else
        {
            if (!c || isspace(c))
            {
                char *foo = malloc(i - beginstring + 1);
                if (!foo)
                    return;
                strncpy(foo, sys_flags->s_name + beginstring, i - beginstring);
                foo[i - beginstring] = 0;
                rcargv[rcargc] = foo;
                rcargc++;
                if (rcargc >= MAXPDSTRING)
                    break;
                state = 0;
            }
        }
    }
    if (sys_argparse(rcargc, rcargv))
        error("error parsing startup arguments");
}

/* undo pdtl_encodedialog.  This allows dialogs to send spaces, commas,
    dollars, and semis down here. */
t_symbol *sys_decodedialog(t_symbol *s)
{
    char buf[MAXPDSTRING], *sp = s->s_name;
    int i;
    if (*sp != '+')
        bug("sys_decodedialog: %s", sp);
    else sp++;
    for (i = 0; i < MAXPDSTRING-1; i++, sp++)
    {
        if (!sp[0])
            break;
        if (sp[0] == '+')
        {
            if (sp[1] == '_')
                buf[i] = ' ', sp++;
            else if (sp[1] == '+')
                buf[i] = '+', sp++;
            else if (sp[1] == 'c')
                buf[i] = ',', sp++;
            else if (sp[1] == 's')
                buf[i] = ';', sp++;
            else if (sp[1] == 'd')
                buf[i] = '$', sp++;
            else buf[i] = sp[0];
        }
        else buf[i] = sp[0];
    }
    buf[i] = 0;
    return (gensym(buf));
}

    /* send the user-specified search path to pd-gui */
void sys_set_searchpath( void)
{
    int i;
    t_namelist *nl;

    sys_gui("set ::tmp_path {}\n");
    for (nl = sys_searchpath, i = 0; nl; nl = nl->nl_next, i++)
        sys_vgui("lappend ::tmp_path {%s}\n", nl->nl_string);
    sys_gui("set ::sys_searchpath $::tmp_path\n");
}

    /* send the hard-coded search path to pd-gui */
void sys_set_extrapath( void)
{
    int i;
    t_namelist *nl;

    sys_gui("set ::tmp_path {}\n");
    for (nl = sys_staticpath, i = 0; nl; nl = nl->nl_next, i++)
        sys_vgui("lappend ::tmp_path {%s}\n", nl->nl_string);
    sys_gui("set ::sys_staticpath $::tmp_path\n");
}

    /* start a search path dialog window */
void glob_start_path_dialog(t_pd *dummy)
{
     char buf[MAXPDSTRING];

    sys_set_searchpath();
    sprintf(buf, "pdtk_path_dialog %%s %d %d\n", sys_usestdpath, sys_verbose);
    gfxstub_new(&glob_pdobject, (void *)glob_start_path_dialog, buf);
}

    /* new values from dialog window */
void glob_path_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    namelist_free(sys_searchpath);
    sys_searchpath = 0;
    sys_usestdpath = atom_getintarg(0, argc, argv);
    sys_verbose = atom_getintarg(1, argc, argv);
    for (i = 0; i < argc-2; i++)
    {
        t_symbol *s = sys_decodedialog(atom_getsymbolarg(i+2, argc, argv));
        if (*s->s_name)
            sys_searchpath = namelist_append_files(sys_searchpath, s->s_name);
    }
}

    /* set the global list vars for startup libraries and flags */
void sys_set_startup( void)
{
    int i;
    t_namelist *nl;

    sys_vgui("set ::startup_flags {%s}\n", sys_flags->s_name);
    sys_gui("set ::startup_libraries {}\n");
    for (nl = sys_externlist, i = 0; nl; nl = nl->nl_next, i++)
        sys_vgui("lappend ::startup_libraries {%s}\n", nl->nl_string);
}

    /* start a startup dialog window */
void glob_start_startup_dialog(t_pd *dummy)
{
    char buf[MAXPDSTRING];

    sys_set_startup();
    sprintf(buf, "pdtk_startup_dialog %%s %d \"%s\"\n", sys_defeatrt,
        sys_flags->s_name);
    gfxstub_new(&glob_pdobject, (void *)glob_start_startup_dialog, buf);
}

    /* new values from dialog window */
void glob_startup_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    namelist_free(sys_externlist);
    sys_externlist = 0;
    sys_defeatrt = atom_getintarg(0, argc, argv);
    sys_flags = sys_decodedialog(atom_getsymbolarg(1, argc, argv));
    for (i = 0; i < argc-2; i++)
    {
        t_symbol *s = sys_decodedialog(atom_getsymbolarg(i+2, argc, argv));
        if (*s->s_name)
            sys_externlist = namelist_append_files(sys_externlist, s->s_name);
    }
}


