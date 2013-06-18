/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#ifdef HAVE_LIBDL
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

#ifdef __FreeBSD__
static char sys_dllextent[] = ".b_i386", sys_dllextent2[] = ".pd_freebsd";
#elif defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__GNU__)
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
int sys_onloadlist(char *classname) /* return true if already loaded */
{
    t_symbol *s = gensym(classname);
    t_loadlist *ll;
    for (ll = sys_loaded; ll; ll = ll->ll_next)
        if (ll->ll_name == s)
            return (1);
    return (0);
}

void sys_putonloadlist(char *classname) /* add to list of loaded modules */
{
    t_loadlist *ll = (t_loadlist *)getbytes(sizeof(*ll));
    ll->ll_name = gensym(classname);
    ll->ll_next = sys_loaded;
    sys_loaded = ll;
    /* post("put on list %s", classname); */
}

void class_set_extern_dir(t_symbol *s);

static int sys_do_load_lib(t_canvas *canvas, char *objectname)
{
    char symname[MAXPDSTRING], filename[MAXPDSTRING], dirbuf[MAXPDSTRING],
        *classname, *nameptr, altsymname[MAXPDSTRING];
    void *dlobj;
    t_xxx makeout = NULL;
    int i, hexmunge = 0, fd;
#ifdef _WIN32
    HINSTANCE ntdll;
#endif
    if (classname = strrchr(objectname, '/'))
        classname++;
    else classname = objectname;
    if (sys_onloadlist(objectname))
    {
        post("%s: already loaded", objectname);
        return (1);
    }
    for (i = 0, nameptr = classname; i < MAXPDSTRING-7 && *nameptr; nameptr++)
    {
        char c = *nameptr;
        if ((c>='0' && c<='9') || (c>='A' && c<='Z')||
           (c>='a' && c<='z' )|| c == '_')
        {
            symname[i] = c;
            i++;
        }
            /* trailing tilde becomes "_tilde" */
        else if (c == '~' && nameptr[1] == 0)
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
    if ((fd = canvas_open(canvas, objectname, sys_dllextent,
        dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            goto gotone;
        /* same, with the more generic sys_dllextent2 */
    if ((fd = canvas_open(canvas, objectname, sys_dllextent2,
        dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            goto gotone;
        /* next try (objectname)/(classname).(sys_dllextent) ... */
    strncpy(filename, objectname, MAXPDSTRING);
    filename[MAXPDSTRING-2] = 0;
    strcat(filename, "/");
    strncat(filename, classname, MAXPDSTRING-strlen(filename));
    filename[MAXPDSTRING-1] = 0;
    if ((fd = canvas_open(canvas, filename, sys_dllextent,
        dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            goto gotone;
    if ((fd = canvas_open(canvas, filename, sys_dllextent2,
        dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            goto gotone;
#ifdef ANDROID
    /* Android libs always have a 'lib' prefix, '.so' suffix and don't allow ~ */
    char libname[MAXPDSTRING] = "lib";
    strncat(libname, objectname, MAXPDSTRING - 4);
    int len = strlen(libname);
    if (libname[len-1] == '~' && len < MAXPDSTRING - 6) {
        strcpy(libname+len-1, "_tilde");
    }
    if ((fd = canvas_open(canvas, libname, ".so",
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
            post("%s: couldn't load", filename);
            class_set_extern_dir(&s_);
            return (0);
        }
        makeout = (t_xxx)GetProcAddress(ntdll, symname);  
        if (!makeout)
             makeout = (t_xxx)GetProcAddress(ntdll, "setup");
        SetDllDirectory(NULL); /* reset DLL dir to nothing */
    }
#elif defined HAVE_LIBDL
    dlobj = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
    if (!dlobj)
    {
        post("%s: %s", filename, dlerror());
        class_set_extern_dir(&s_);
        return (0);
    }
    makeout = (t_xxx)dlsym(dlobj,  symname);
    if(!makeout)
        makeout = (t_xxx)dlsym(dlobj,  "setup");
#else
#warning "No dynamic loading mechanism specified, libdl or WIN32 required for loading externals!"
#endif

    if (!makeout)
    {
        post("load_object: Symbol \"%s\" not found", symname);
        class_set_extern_dir(&s_);
        return 0;
    }
    (*makeout)();
    class_set_extern_dir(&s_);
    sys_putonloadlist(objectname);
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

int sys_load_lib(t_canvas *canvas, char *classname)
{
    int dspstate = canvas_suspend_dsp();
    int ok = 0;
    loader_queue_t *q;
    for(q = &loaders; q; q = q->next)
        if (ok = q->loader(canvas, classname)) break;
    canvas_resume_dsp(dspstate);
    return ok;
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
            post("%s: couldn't load external scheduler", filename);
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
            post("%s: %s", filename, dlerror());
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
