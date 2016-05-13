/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#if defined(HAVE_LIBDL) || defined(__FreeBSD__)
#include <dlfcn.h>
#endif
#ifdef HAVE_UNISTD_H
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#endif
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <string.h>
#include "m_pd.h"
#include "s_stuff.h"
#include <stdio.h>
#include <sys/stat.h>
#ifdef _MSC_VER  /* This is only for Microsoft's compiler, not cygwin, e.g. */
#define snprintf sprintf_s
#define stat _stat
#endif

typedef void (*t_xxx)(void);

/* naming convention for externs.  The names are kept distinct for those
who wich to make "fat" externs compiled for many platforms.  Less specific
fallbacks are provided, primarily for back-compatibility; these suffice if
you are building a package which will run with a single set of compiled
objects.  The specific name is the letter b, l, d, or m for  BSD, linux,
darwin, or microsoft, followed by a more specific string, either "fat" for
a fat binary or an indication of the instruction set. */

#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__GNU__) || defined(__FreeBSD__)
static char sys_dllextent2[] = ".pd_linux";
# ifdef __x86_64__
static char sys_dllextent[] = ".l_ia64"; // this should be .l_x86_64 or .l_amd64
# elif defined(__i386__) || defined(_M_IX86)
static char sys_dllextent[] = ".l_i386";
# elif defined(__arm__)
static char sys_dllextent[] = ".l_arm";
# else
static char sys_dllextent[] = ".so";
# endif
#elif defined(__APPLE__)
# ifndef MACOSX3
static char sys_dllextent[] = ".d_fat", sys_dllextent2[] = ".pd_darwin";
# else
static char sys_dllextent[] = ".d_ppc", sys_dllextent2[] = ".pd_darwin";
# endif
#elif defined(_WIN32) || defined(__CYGWIN__)
static char sys_dllextent[] = ".m_i386", sys_dllextent2[] = ".dll";
#else
static char sys_dllextent[] = ".so", sys_dllextent2[] = ".so";
#endif

    /* maintain list of loaded modules to avoid repeating loads */
typedef struct _loadedlist
{
    struct _loadedlist *ll_next;
    t_symbol *ll_name;
} t_loadlist;

static t_loadlist *sys_loaded;
int sys_onloadlist(const char *classname) /* return true if already loaded */
{
    t_symbol *s = gensym(classname);
    t_loadlist *ll;
    for (ll = sys_loaded; ll; ll = ll->ll_next)
        if (ll->ll_name == s)
            return (1);
    return (0);
}

     /* add to list of loaded modules */
void sys_putonloadlist(const char *classname)
{
    t_loadlist *ll = (t_loadlist *)getbytes(sizeof(*ll));
    ll->ll_name = gensym(classname);
    ll->ll_next = sys_loaded;
    sys_loaded = ll;
    /* post("put on list %s", classname); */
}

void class_set_extern_dir(t_symbol *s);

static int sys_do_load_abs(t_canvas *canvas, const char *objectname,
    const char *path);
static int sys_do_load_lib(t_canvas *canvas, const char *objectname,
    const char *path)
{
    char symname[MAXPDSTRING], filename[MAXPDSTRING], dirbuf[MAXPDSTRING],
        *nameptr, altsymname[MAXPDSTRING];
    const char *classname, *cnameptr;
    void *dlobj;
    t_xxx makeout = NULL;
    int i, hexmunge = 0, fd;
#ifdef _WIN32
    HINSTANCE ntdll;
#endif
        /* NULL-path is only used as a last resort,
           but we have already tried all paths */
    if(!path)return (0);

    if ((classname = strrchr(objectname, '/')))
        classname++;
    else classname = objectname;
    for (i = 0, cnameptr = classname; i < MAXPDSTRING-7 && *cnameptr;
        cnameptr++)
    {
        char c = *cnameptr;
        if ((c>='0' && c<='9') || (c>='A' && c<='Z')||
           (c>='a' && c<='z' )|| c == '_')
        {
            symname[i] = c;
            i++;
        }
            /* trailing tilde becomes "_tilde" */
        else if (c == '~' && cnameptr[1] == 0)
        {
            strcpy(symname+i, "_tilde");
            i += strlen(symname+i);
        }
        else /* anything you can't put in a C symbol is sprintf'ed in hex */
        {
            sprintf(symname+i, "0x%02x", c);
            i += strlen(symname+i);
            hexmunge = 1;
        }
    }
    symname[i] = 0;
    if (hexmunge)
    {
        memmove(symname+6, symname, strlen(symname)+1);
        strncpy(symname, "setup_", 6);
    }
    else strcat(symname, "_setup");

#if 0
    fprintf(stderr, "lib: %s\n", classname);
#endif
        /* try looking in the path for (objectname).(sys_dllextent) ... */
    if ((fd = sys_trytoopenone(path, objectname, sys_dllextent,
        dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            goto gotone;
        /* same, with the more generic sys_dllextent2 */
    if ((fd = sys_trytoopenone(path, objectname, sys_dllextent2,
        dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            goto gotone;
        /* next try (objectname)/(classname).(sys_dllextent) ... */
    strncpy(filename, objectname, MAXPDSTRING);
    filename[MAXPDSTRING-2] = 0;
    strcat(filename, "/");
    strncat(filename, classname, MAXPDSTRING-strlen(filename));
    filename[MAXPDSTRING-1] = 0;
    if ((fd = sys_trytoopenone(path, filename, sys_dllextent,
        dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            goto gotone;
    if ((fd = sys_trytoopenone(path, filename, sys_dllextent2,
        dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            goto gotone;
#ifdef ANDROID
    /* Android libs have a 'lib' prefix, '.so' suffix and don't allow ~ */
    char libname[MAXPDSTRING] = "lib";
    strncat(libname, objectname, MAXPDSTRING - 4);
    int len = strlen(libname);
    if (libname[len-1] == '~' && len < MAXPDSTRING - 6) {
        strcpy(libname+len-1, "_tilde");
    }
    if ((fd = sys_trytoopenone(path, libname, ".so",
        dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            goto gotone;
#endif
    return (0);
gotone:
    close(fd);
    class_set_extern_dir(gensym(dirbuf));

        /* rebuild the absolute pathname */
    strncpy(filename, dirbuf, MAXPDSTRING);
    filename[MAXPDSTRING-2] = 0;
    strcat(filename, "/");
    strncat(filename, nameptr, MAXPDSTRING-strlen(filename));
    filename[MAXPDSTRING-1] = 0;

#ifdef _WIN32
    {
        char dirname[MAXPDSTRING], *s, *basename;
        sys_bashfilename(filename, filename);
        /* set the dirname as DllDirectory, meaning in the path for
           loading other DLLs so that dependent libraries can be included
           in the same folder as the external. SetDllDirectory() needs a
           minimum supported version of Windows XP SP1 for
           SetDllDirectory, so WINVER must be 0x0502 */
        strncpy(dirname, filename, MAXPDSTRING);
        s = strrchr(dirname, '\\');
        basename = s;
        if (s && *s)
          *s = '\0';
        if (!SetDllDirectory(dirname))
           error("Could not set '%s' as DllDirectory(), '%s' might not load.",
                 dirname, basename);
        /* now load the DLL for the external */
        ntdll = LoadLibrary(filename);
        if (!ntdll)
        {
            verbose(1, "%s: couldn't load", filename);
            class_set_extern_dir(&s_);
            return (0);
        }
        makeout = (t_xxx)GetProcAddress(ntdll, symname);
        if (!makeout)
             makeout = (t_xxx)GetProcAddress(ntdll, "setup");
        SetDllDirectory(NULL); /* reset DLL dir to nothing */
    }
#elif defined(HAVE_LIBDL) || defined(__FreeBSD__)
    dlobj = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
    if (!dlobj)
    {
        verbose(1, "%s: %s", filename, dlerror());
        class_set_extern_dir(&s_);
        return (0);
    }
    makeout = (t_xxx)dlsym(dlobj,  symname);
    if(!makeout)
        makeout = (t_xxx)dlsym(dlobj,  "setup");
#else
#warning "No dynamic loading mechanism specified, \
    libdl or WIN32 required for loading externals!"
#endif

    if (!makeout)
    {
        verbose(1, "load_object: Symbol \"%s\" not found", symname);
        class_set_extern_dir(&s_);
        return 0;
    }
    (*makeout)();
    class_set_extern_dir(&s_);
    return (1);
}


/* linked list of loaders */
typedef struct loader_queue {
    loader_t loader;
    struct loader_queue *next;
} loader_queue_t;

static loader_queue_t loaders = {sys_do_load_lib, NULL};

/* register class loader function */
void sys_register_loader(loader_t loader)
{
    loader_queue_t *q = &loaders;
    while (1)
    {
        if (q->loader == loader)    /* already loaded - nothing to do */
            return;
        else if (q->next)
            q = q->next;
        else
        {
            q->next = (loader_queue_t *)getbytes(sizeof(loader_queue_t));
            q->next->loader = loader;
            q->next->next = NULL;
            break;
        }
    }
}

#include "g_canvas.h"

/* the data passed to the iter-function */
struct _loadlib_data
{
    t_canvas *canvas;
    const char *classname;
    int ok;
};

int sys_loadlib_iter(const char *path, struct _loadlib_data *data)
{
    int ok = 0;
    loader_queue_t *q;
    for(q = &loaders; q; q = q->next)
        if ((ok = q->loader(data->canvas, data->classname, path)))
            break;
    /* if all loaders failed, try to load as abstraction */
    if (!ok)
        ok = sys_do_load_abs(data->canvas, data->classname, path);
    data->ok = ok;
    return (ok == 0);
}

int sys_load_lib(t_canvas *canvas, const char *classname)
{
    int dspstate = canvas_suspend_dsp();
    struct _loadlib_data data;
    data.canvas = canvas;
    data.ok = 0;

    if (sys_onloadlist(classname))
    {
        verbose(1, "%s: already loaded", classname);
        return (1);
    }
        /* if classname is absolute, try this first */
    if (sys_isabsolutepath(classname))
    {
            /* this is just copied from sys_open_absolute()
               LATER avoid code duplication */
        char dirbuf[MAXPDSTRING], *z = strrchr(classname, '/');
        int dirlen;
        if (!z)
            return (0);
        dirlen = z - classname;
        if (dirlen > MAXPDSTRING-1)
            dirlen = MAXPDSTRING-1;
        strncpy(dirbuf, classname, dirlen);
        dirbuf[dirlen] = 0;
        data.classname=classname+(dirlen+1);
        sys_loadlib_iter(dirbuf, &data);
    }
    data.classname = classname;
    if(!data.ok)
        canvas_path_iterate(canvas, (t_canvas_path_iterator)sys_loadlib_iter,
            &data);

    /* if loaders failed so far, we try a last time without a PATH
     * let the loaders search wherever they want */
    if (!data.ok)
        sys_loadlib_iter(0, &data);

    if(data.ok)
      sys_putonloadlist(classname);


    canvas_resume_dsp(dspstate);
    return data.ok;
}

int sys_run_scheduler(const char *externalschedlibname,
    const char *sys_extraflagsstring)
{
    typedef int (*t_externalschedlibmain)(const char *);
    t_externalschedlibmain externalmainfunc;
    char filename[MAXPDSTRING];
    struct stat statbuf;
    snprintf(filename, sizeof(filename), "%s%s", externalschedlibname,
        sys_dllextent);
    sys_bashfilename(filename, filename);
        /* if first-choice file extent can't 'stat', go for second */
    if (stat(filename, &statbuf) < 0)
    {
        snprintf(filename, sizeof(filename), "%s%s", externalschedlibname,
            sys_dllextent2);
        sys_bashfilename(filename, filename);
    }
#ifdef _WIN32
    {
        HINSTANCE ntdll = LoadLibrary(filename);
        if (!ntdll)
        {
            fprintf(stderr, "%s: couldn't load external scheduler\n", filename);
            error("%s: couldn't load external scheduler", filename);
            return (1);
        }
        externalmainfunc =
            (t_externalschedlibmain)GetProcAddress(ntdll, "pd_extern_sched");
        if (!externalmainfunc)
            externalmainfunc =
                (t_externalschedlibmain)GetProcAddress(ntdll, "main");
    }
#elif defined HAVE_LIBDL
    {
        void *dlobj;
        dlobj = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
        if (!dlobj)
        {
            error("%s: %s", filename, dlerror());
            fprintf(stderr, "dlopen failed for %s: %s\n", filename, dlerror());
            return (1);
        }
        externalmainfunc = (t_externalschedlibmain)dlsym(dlobj,
            "pd_extern_sched");
    }
#else
    return (0);
#endif
    if (externalmainfunc)
        return((*externalmainfunc)(sys_extraflagsstring));
    else
    {
        fprintf(stderr, "%s: couldn't find pd_extern_sched() or main()\n",
            filename);
        return (0);
    }
}


/* abstraction loading */
void canvas_popabstraction(t_canvas *x);
int pd_setloadingabstraction(t_symbol *sym);
extern t_pd *newest;

static t_pd *do_create_abstraction(t_symbol*s, int argc, t_atom *argv)
{
    /*
     * TODO: check if the there is a binbuf cached for <canvas::symbol>
        and use that instead.  We'll have to invalidate the cache once we
        are done (either with a clock_delay(0) or something else)
     */
    if (!pd_setloadingabstraction(s))
    {
        const char *objectname = s->s_name;
        char dirbuf[MAXPDSTRING], classslashclass[MAXPDSTRING], *nameptr;
        t_glist *glist = (t_glist *)canvas_getcurrent();
        t_canvas *canvas = (t_canvas*)glist_getcanvas(glist);
        int fd = -1;

        t_pd *was = s__X.s_thing;
        snprintf(classslashclass, MAXPDSTRING, "%s/%s", objectname, objectname);
        if ((fd = canvas_open(canvas, objectname, ".pd",
                  dirbuf, &nameptr, MAXPDSTRING, 0)) >= 0 ||
            (fd = canvas_open(canvas, objectname, ".pat",
                  dirbuf, &nameptr, MAXPDSTRING, 0)) >= 0 ||
            (fd = canvas_open(canvas, classslashclass, ".pd",
                  dirbuf, &nameptr, MAXPDSTRING, 0)) >= 0)
        {
            close(fd);
            canvas_setargs(argc, argv);

            binbuf_evalfile(gensym(nameptr), gensym(dirbuf));
            if (s__X.s_thing && was != s__X.s_thing)
                canvas_popabstraction((t_canvas *)(s__X.s_thing));
            else s__X.s_thing = was;
            canvas_setargs(0, 0);
            return (newest);
        }
            /* otherwise we couldn't do it; just return 0 */
    }
    else error("%s: can't load abstraction within itself\n", s->s_name);
    newest = 0;
    return (0);
}

/* search for abstraction; register a creator if found */
static int sys_do_load_abs(t_canvas *canvas, const char *objectname,
    const char *path)
{
    int fd;
    static t_gobj*abstraction_classes = 0;
    char dirbuf[MAXPDSTRING], classslashclass[MAXPDSTRING], *nameptr;
        /* NULL-path is only used as a last resort,
           but we have already tried all paths */
    if (!path) return (0);

    snprintf(classslashclass, MAXPDSTRING, "%s/%s", objectname, objectname);
    if ((fd = sys_trytoopenone(path, objectname, ".pd",
              dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0 ||
        (fd = sys_trytoopenone(path, objectname, ".pat",
              dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0 ||
        (fd = sys_trytoopenone(path, classslashclass, ".pd",
              dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
    {
        t_class*c=0;
        close(fd);
            /* found an abstraction, now register it as a new pseudo-class */
        class_set_extern_dir(gensym(dirbuf));
        if((c=class_new(gensym(objectname),
                        (t_newmethod)do_create_abstraction, 0,
                        0, 0, A_GIMME, 0)))
        {
                /* store away the newly created class, maybe we will need it one day */
            t_gobj*absclass=0;
            absclass=t_getbytes(sizeof(*absclass));
            absclass->g_pd=c;
            absclass->g_next=abstraction_classes;
            abstraction_classes=absclass;
        }
        class_set_extern_dir(&s_);

        return (1);
    }
    return (0);
}
