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
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

#ifdef _WIN32
# include <malloc.h> /* MSVC or mingw on windows */
#elif defined(__linux__) || defined(__APPLE__)
# include <alloca.h> /* linux, mac, mingw, cygwin */
#else
# include <stdlib.h> /* BSDs for example */
#endif

#include <string.h>
#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include "s_utf8.h"
#include <ctype.h>

    /* change '/' characters to the system's native file separator */
void sys_bashfilename(const char *from, char *to)
{
    char c;
    while ((c = *from++))
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
    while ((c = *from++))
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
            {
                freebytes(nl2->nl_string, strlen(nl2->nl_string) + 1);
                return (listwas);
            }
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
    t_namelist *nl = listwas;

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

const char *namelist_get(const t_namelist *namelist, int n)
{
    int i;
    const t_namelist *nl;
    for (i = 0, nl = namelist; i < n && nl; i++, nl = nl->nl_next)
        ;
    return (nl ? nl->nl_string : 0);
}

int sys_usestdpath = 1;

void sys_setextrapath(const char *p)
{
    char pathbuf[MAXPDSTRING];
    namelist_free(STUFF->st_staticpath);
    /* add standard place for users to install stuff first */
#ifdef __gnu_linux__
    sys_expandpath("~/.local/lib/pd/extra/", pathbuf, MAXPDSTRING);
    STUFF->st_staticpath = namelist_append(0, pathbuf, 0);
    sys_expandpath("~/pd-externals", pathbuf, MAXPDSTRING);
    STUFF->st_staticpath = namelist_append(STUFF->st_staticpath, pathbuf, 0);
    STUFF->st_staticpath = namelist_append(STUFF->st_staticpath,
        "/usr/local/lib/pd-externals", 0);
#endif

#ifdef __APPLE__
    sys_expandpath("~/Library/Pd", pathbuf, MAXPDSTRING);
    STUFF->st_staticpath = namelist_append(0, pathbuf, 0);
    STUFF->st_staticpath = namelist_append(STUFF->st_staticpath, "/Library/Pd", 0);
#endif

#ifdef _WIN32
    sys_expandpath("%AppData%/Pd", pathbuf, MAXPDSTRING);
    STUFF->st_staticpath = namelist_append(0, pathbuf, 0);
    sys_expandpath("%CommonProgramFiles%/Pd", pathbuf, MAXPDSTRING);
    STUFF->st_staticpath = namelist_append(STUFF->st_staticpath, pathbuf, 0);
#endif
    /* add built-in "extra" path last so its checked last */
    STUFF->st_staticpath = namelist_append(STUFF->st_staticpath, p, 0);
}

    /* try to open a file in the directory "dir", named "name""ext",
    for reading, return true on success.  "Name" may have slashes.  The directory is copied to
    "dirresult" which must be at least "size" bytes.  "nameresult" is set
    to point to the filename (copied elsewhere into the same buffer). Filehandle written to "File".
    The "bin" flag requests opening for binary (which only makes a difference
    on Windows). */

bool sys_trytoopenone(const char *dir, const char *name, const char* ext,
    char *dirresult, char **nameresult, t_fileops_handle *file, unsigned int size, int bin)
{
    t_fileops_handle fd;
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
    if (sys_fileops.open(dirresult, FILEOPS_READ, &fd))
    {
            /* in unix, further check that it's not a directory */
        t_fileops_stat statbuf;
        bool ok =  sys_fileops.stat(fd, &statbuf) &&
            !(statbuf.isdir_known && statbuf.isdir);
        if (!ok)
        {
            if (sys_verbose) post("tried %s; stat failed or directory",
                dirresult);
            sys_fileops.close(fd);
        }
        else
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

            *file = fd;
            return true;
        }
    }
    else
    {
        if (sys_verbose) post("tried %s and failed", dirresult);
    }
    return false;
}

    /* check if we were given an absolute pathname, if so try to open it
    and return true to signal the caller to cancel any path searches */
bool sys_open_absolute(const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin, t_fileops_handle *fdp)
{
    if (sys_isabsolutepath(name))
    {
        char dirbuf[MAXPDSTRING], *z = strrchr(name, '/');
        int dirlen;
        if (!z)
            return (0);
        dirlen = (int)(z - name);
        if (dirlen > MAXPDSTRING-1)
            dirlen = MAXPDSTRING-1;
        strncpy(dirbuf, name, dirlen);
        dirbuf[dirlen] = 0;
        return sys_trytoopenone(dirbuf, name+(dirlen+1), ext,
            dirresult, nameresult, fdp, size, bin);
    }
    else return false;
}

/* search for a file in a specified directory, then along the globally
defined search path, using ext as filename extension.  True-for-success
is returned, the directory ends up in the "dirresult" which must be at
least "size" bytes.  "nameresult" is set to point to the filename, which
ends up in the same buffer as dirresult, filehandle saved in *file.  Exception:
if the 'name' starts with a slash or a letter, colon, and slash in MSW,
there is no search and instead we just try to open the file literally.  */

/* see also canvas_open() which, in addition, searches down the
canvas-specific path. */

static bool do_open_via_path(const char *dir, const char *name,
    const char *ext, char *dirresult, char **nameresult, t_fileops_handle *file,
    unsigned int size, int bin, t_namelist *searchpath)
{
    t_namelist *nl;

        /* first check if "name" is absolute (and if so, try to open) */
    if (sys_open_absolute(name, ext, dirresult, nameresult, size, bin, file))
        return true;

        /* otherwise "name" is relative; try the directory "dir" first. */
    if (sys_trytoopenone(dir, name, ext,
        dirresult, nameresult, file, size, bin))
            return true;

        /* next go through the search path */
    for (nl = searchpath; nl; nl = nl->nl_next)
        if (sys_trytoopenone(nl->nl_string, name, ext,
            dirresult, nameresult, file, size, bin))
                return true;
        /* next go through the temp paths from the commandline */
    for (nl = STUFF->st_temppath; nl; nl = nl->nl_next)
        if (sys_trytoopenone(nl->nl_string, name, ext,
            dirresult, nameresult, file, size, bin))
                return true;
        /* next look in built-in paths like "extra" */
    if (sys_usestdpath)
        for (nl = STUFF->st_staticpath; nl; nl = nl->nl_next)
            if (sys_trytoopenone(nl->nl_string, name, ext,
                dirresult, nameresult, file, size, bin))
                    return true;

    *dirresult = 0;
    *nameresult = dirresult;
    return false;
}

    /* open via path, using the global search path.
       returns success on true, file saved to *file */
bool open_via_path(const char *dir, const char *name, const char *ext,
    char *dirresult, char **nameresult, t_fileops_handle *file, unsigned int size, int bin)
{
    return (do_open_via_path(dir, name, ext, dirresult, nameresult, file,
        size, bin, STUFF->st_searchpath));
}

    /* Open a help file using the help search path.  We expect the ".pd"
    suffix here, even though we have to tear it back off for one of the
    search attempts. */
void open_via_helppath(const char *name, const char *dir)
{
    char realname[MAXPDSTRING], dirbuf[MAXPDSTRING], *basename;
        /* make up a silly "dir" if none is supplied */
    const char *usedir = (*dir ? dir : "./");
    t_fileops_handle fd;

        /* 1. "objectname-help.pd" */
    strncpy(realname, name, MAXPDSTRING-10);
    realname[MAXPDSTRING-10] = 0;
    if (strlen(realname) > 3 && !strcmp(realname+strlen(realname)-3, ".pd"))
        realname[strlen(realname)-3] = 0;
    strcat(realname, "-help.pd");
    if (do_open_via_path(usedir, realname, "", dirbuf, &basename, &fd,
        MAXPDSTRING, 0, STUFF->st_helppath))
            goto gotone;

        /* 2. "help-objectname.pd" */
    strcpy(realname, "help-");
    strncat(realname, name, MAXPDSTRING-10);
    realname[MAXPDSTRING-1] = 0;
    if (do_open_via_path(usedir, realname, "", dirbuf, &basename, &fd,
        MAXPDSTRING, 0, STUFF->st_helppath))
            goto gotone;

    post("sorry, couldn't find help patch for \"%s\"", name);
    return;
gotone:
    sys_fileops.close (fd);
    glob_evalfile(0, gensym((char*)basename), gensym(dirbuf));
}

int sys_argparse(int argc, char **argv);
void sys_doflags(void)
{
    int i, beginstring = 0, state = 0, len;
    int rcargc = 0;
    char *rcargv[MAXPDSTRING];
    if (!sys_flags)
        sys_flags = &s_;
    len = (int)strlen(sys_flags->s_name);
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
    char buf[MAXPDSTRING];
    const char *sp = s->s_name;
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
void sys_set_searchpath(void)
{
    int i;
    t_namelist *nl;

    sys_gui("set ::tmp_path {}\n");
    for (nl = STUFF->st_searchpath, i = 0; nl; nl = nl->nl_next, i++)
        sys_vgui("lappend ::tmp_path {%s}\n", nl->nl_string);
    sys_gui("set ::sys_searchpath $::tmp_path\n");
}

    /* send the temp paths from the commandline to pd-gui */
void sys_set_temppath(void)
{
    int i;
    t_namelist *nl;

    sys_gui("set ::tmp_path {}\n");
    for (nl = STUFF->st_temppath, i = 0; nl; nl = nl->nl_next, i++)
        sys_vgui("lappend ::tmp_path {%s}\n", nl->nl_string);
    sys_gui("set ::sys_temppath $::tmp_path\n");
}

    /* send the hard-coded search path to pd-gui */
void sys_set_extrapath(void)
{
    int i;
    t_namelist *nl;

    sys_gui("set ::tmp_path {}\n");
    for (nl = STUFF->st_staticpath, i = 0; nl; nl = nl->nl_next, i++)
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
    namelist_free(STUFF->st_searchpath);
    STUFF->st_searchpath = 0;
    sys_usestdpath = atom_getfloatarg(0, argc, argv);
    sys_verbose = atom_getfloatarg(1, argc, argv);
    for (i = 0; i < argc-2; i++)
    {
        t_symbol *s = sys_decodedialog(atom_getsymbolarg(i+2, argc, argv));
        if (*s->s_name)
            STUFF->st_searchpath =
                namelist_append_files(STUFF->st_searchpath, s->s_name);
    }
}

    /* add one item to search path (intended for use by Deken plugin).
    if "saveit" is set, also save all settings.  */
void glob_addtopath(t_pd *dummy, t_symbol *path, t_float saveit)
{
    t_symbol *s = sys_decodedialog(path);
    if (*s->s_name)
    {
        STUFF->st_searchpath =
            namelist_append_files(STUFF->st_searchpath, s->s_name);
        if (saveit != 0)
            sys_savepreferences(0);
    }
}

    /* set the global list vars for startup libraries and flags */
void sys_set_startup(void)
{
    int i;
    t_namelist *nl;

    sys_vgui("set ::startup_flags {%s}\n",
        (sys_flags? sys_flags->s_name : ""));
    sys_gui("set ::startup_libraries {}\n");
    for (nl = STUFF->st_externlist, i = 0; nl; nl = nl->nl_next, i++)
        sys_vgui("lappend ::startup_libraries {%s}\n", nl->nl_string);
}

    /* start a startup dialog window */
void glob_start_startup_dialog(t_pd *dummy)
{
    char buf[MAXPDSTRING];

    sys_set_startup();
    sprintf(buf, "pdtk_startup_dialog %%s %d \"%s\"\n", sys_defeatrt,
        (sys_flags? sys_flags->s_name : ""));
    gfxstub_new(&glob_pdobject, (void *)glob_start_startup_dialog, buf);
}

    /* new values from dialog window */
void glob_startup_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    namelist_free(STUFF->st_externlist);
    STUFF->st_externlist = 0;
    sys_defeatrt = atom_getfloatarg(0, argc, argv);
    sys_flags = sys_decodedialog(atom_getsymbolarg(1, argc, argv));
    for (i = 0; i < argc-2; i++)
    {
        t_symbol *s = sys_decodedialog(atom_getsymbolarg(i+2, argc, argv));
        if (*s->s_name)
            STUFF->st_externlist =
                namelist_append_files(STUFF->st_externlist, s->s_name);
    }
}

// FIXME: This is bonkers, but I don't want to try to add my .c file to all these buildfiles until my patch is accepted :P -- Andi
#include "s_fileops.c"
