/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */
#if !defined (HAVE_LIBDL) && HAVE_DLOPEN
# define HAVE_LIBDL 1
#endif

#if HAVE_LIBDL
# include <dlfcn.h>
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
#include "m_private_utils.h"
#ifdef _MSC_VER  /* This is only for Microsoft's compiler, not cygwin, e.g. */
#define stat _stat
#endif

typedef void (*t_xxx)(void);

/* naming convention for externs.  The names are kept distinct for those
who wish to make "fat" externs compiled for many platforms.  Less specific
fallbacks are provided, primarily for back-compatibility; these suffice if
you are building a package which will run with a single set of compiled
objects.  The specific name is the letter b, l, d, or m for  BSD, linux,
darwin, or microsoft, followed by a more specific string, either "fat" for
a fat binary or an indication of the instruction set. */

#ifdef __APPLE__
# define FAT_BINARIES 1
#endif

#define STR(s) #s
#define STRINGIFY(s) STR(s)


#if defined(__x86_64__) || defined(_M_X64)
# define ARCHEXT "amd64"
#elif defined(__i386__) || defined(_M_IX86)
# define ARCHEXT "i386"
#elif defined(__arm__)
# define ARCHEXT "arm"
#elif defined(__aarch64__)
# define ARCHEXT "arm64"
#elif defined(__ppc__)
# define ARCHEXT "ppc"
#endif

#ifdef ARCHEXT
#define ARCHDLLEXT(prefix) prefix ARCHEXT ,
#else
#define ARCHDLLEXT(prefix)
#endif


#if defined(_WIN32) || defined(__CYGWIN__)
# define SYSTEMEXT ".dll"
#else
# define SYSTEMEXT ".so"
#endif

static const char*sys_dllextent_base[] = {
#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__GNU__)
    ARCHDLLEXT(".l_")
# if defined(__x86_64__) || defined(_M_X64)
    ".l_ia64",      /* incorrect but probably in wide use */
# endif
    ".pd_linux",
#elif defined(__APPLE__)
    ARCHDLLEXT(".d_")
    ".d_fat",
    ".pd_darwin",
#elif defined(_WIN32) || defined(__CYGWIN__)
    ARCHDLLEXT(".m_")
    SYSTEMEXT,
#endif
    0
    };

static const char**sys_dllextent = 0;
static size_t num_dllextents = 0;
static void add_dllextension(const char*ext) {
    const char**extensions;
    if(ext) {
        /* prevent duplicate entries */
        int i;
        for(i=0; i<num_dllextents; i++) {
            if(!strcmp(ext, sys_dllextent[i]))
                return;
        }
    }
    extensions = resizebytes(sys_dllextent
        , sizeof(*sys_dllextent) * num_dllextents
        , sizeof(*sys_dllextent) * (num_dllextents+1)
        );
    if(!extensions)
        return;
    sys_dllextent = extensions;
    sys_dllextent[num_dllextents] = ext;
    num_dllextents++;
}

const char*sys_deken_specifier(char*buf, size_t bufsize, int include_floatsize, int cpu);

static char*add_deken_extension(const char*systemext, int float_agnostic, int cpu)
{
    char extbuf[MAXPDSTRING];
    char*ext = 0;

    if(!(sys_deken_specifier(extbuf, sizeof(extbuf), float_agnostic, cpu)))
        return 0;

    ext = getbytes(MAXPDSTRING);
    if(!ext)
        return 0;
    ext[MAXPDSTRING-1] = 0;

    if(pd_snprintf(ext, MAXPDSTRING-1, ".%s%s", extbuf, systemext) > 0)
        add_dllextension(ext);
    else
    {
        freebytes(ext, MAXPDSTRING);
        ext = 0;
    }
    return ext;
}

    /* get an array of dll-extensions */
const char**sys_get_dllextensions(void)
{
    if(!sys_dllextent) {
        const char *extraext = 0;
#if defined EXTERNAL_EXTENSION
        do {
            /* the EXTERNAL_EXTENSION might be surrounded by single-quotes
             * to prevent macro-expansion within the macro
             * if so, get rid of them
             */
            unsigned int i,j;
            static char extern_extension[MAXPDSTRING];
            strcpy(extern_extension, STRINGIFY(EXTERNAL_EXTENSION));
            extern_extension[MAXPDSTRING-1] = 0;
            for(i=0,j=0; i<MAXPDSTRING; i++) {
                if(!extern_extension[i]) {
                    extern_extension[j] = 0;
                    break;
                }
                switch(extern_extension[i]) {
                case '\'': break;
                default:
                    extern_extension[j++] = extern_extension[i];
                }
            }
            extraext = extern_extension;
        } while (0);
#endif

        int i, cpu;

            /* create deken-based extensions */
        for(cpu=0; ; cpu++)
        {
            /* iterate over compatible CPUs  */
            if (!add_deken_extension(SYSTEMEXT, 0, cpu))
                break;
            if (!add_deken_extension(SYSTEMEXT, 1, cpu))
                break;
        }
#if FAT_BINARIES
        add_deken_extension(SYSTEMEXT, 0, -1);
        add_deken_extension(SYSTEMEXT, 1, -1);
#endif

        if(extraext) {
            /* check if the extra-extension is part of sys_dllextent_base
             * and if so drop it as redundant.
             */
            for(i=0; i<sizeof(sys_dllextent_base)/sizeof(*sys_dllextent_base); i++) {
                if(!sys_dllextent_base[i])
                    continue;
                if(!strcmp(sys_dllextent_base[i], extraext)) {
                    extraext=0;
                    break;
                }
            }
        }
        if(extraext)
            add_dllextension(extraext);

#if PD_FLOATSIZE == 32
            /* and add the legacy extensions */
        for(i=0; i<sizeof(sys_dllextent_base)/sizeof(*sys_dllextent_base); i++)
        {
            if(sys_dllextent_base[i])
                add_dllextension(sys_dllextent_base[i]);
        }
#endif
            /* 0-terminate the extension list */
        add_dllextension(0);
    }
    return sys_dllextent;
}

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


static int sys_do_load_lib_from_file(int fd,
    const char*objectname,
    const char*dirbuf,
    const char*nameptr,
    const char*symname) {
    char filename[MAXPDSTRING];
    t_xxx makeout = NULL;
#ifdef _WIN32
    HINSTANCE dlobj;
#else
    void*dlobj = NULL;
#endif
        /* close dangling filedescriptor */
    close(fd);

        /* attempt to open the library and call the setup function */


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
           pd_error(0, "could not set '%s' as DllDirectory(), '%s' might not load.",
                 dirname, basename);
        /* now load the DLL for the external */
        dlobj = LoadLibrary(filename);
        if (!dlobj)
        {
            wchar_t wbuf[MAXPDSTRING];
            char buf[MAXPDSTRING];
            DWORD count, err = GetLastError();
            count = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                0, err, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), wbuf, MAXPDSTRING, NULL);
            if (!count || !WideCharToMultiByte(CP_UTF8, 0, wbuf, count+1, buf, MAXPDSTRING, 0, 0))
                *buf = '\0';
            pd_error(0, "%s: %s (%d)", filename, buf, err);
        } else {
            makeout = (t_xxx)GetProcAddress(dlobj, symname);
            if (!makeout)
                makeout = (t_xxx)GetProcAddress(dlobj, "setup");
        }
        SetDllDirectory(NULL); /* reset DLL dir to nothing */
    }
#elif defined(HAVE_LIBDL)
    {
        dlobj = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
        if (!dlobj)
        {
            pd_error(0, "%s:%s", filename, dlerror());
        } else {
            makeout = (t_xxx)dlsym(dlobj,  symname);
            if(!makeout)
                makeout = (t_xxx)dlsym(dlobj,  "setup");
        }
    }
#else
#warning "No dynamic loading mechanism specified, \
libdl or WIN32 required for loading externals!"
#endif

    if(makeout)
        (*makeout)();
    else if (dlobj)
        pd_error(0, "load_object: Symbol \"%s\" not found in \"%s\"", symname, filename);

    class_set_extern_dir(&s_);

    return (makeout)?1:0;
}

static int sys_do_load_lib(t_canvas *canvas, const char *objectname,
    const char *path)
{
    char symname[MAXPDSTRING], filename[MAXPDSTRING], dirbuf[MAXPDSTRING],
        *nameptr;
    const char**dllextent;
    const char *classname, *cnameptr;
    void *dlobj;
    t_xxx makeout = NULL;
    int i, hexmunge = 0, fd;
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
    for(dllextent=sys_get_dllextensions(); *dllextent; dllextent++)
    {
        if ((fd = sys_trytoopenone(path, objectname, *dllextent,
            dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            if(sys_do_load_lib_from_file(fd, objectname, dirbuf, nameptr, symname))
                return 1;
    }
        /* next try (objectname)/(classname).(sys_dllextent) ... */
    strncpy(filename, objectname, MAXPDSTRING);
    filename[MAXPDSTRING-2] = 0;
    strcat(filename, "/");
    strncat(filename, classname, MAXPDSTRING-strlen(filename));
    filename[MAXPDSTRING-1] = 0;
    for(dllextent=sys_get_dllextensions(); *dllextent; dllextent++)
    {
        if ((fd = sys_trytoopenone(path, filename, *dllextent,
            dirbuf, &nameptr, MAXPDSTRING, 1)) >= 0)
            if(sys_do_load_lib_from_file(fd, objectname, dirbuf, nameptr, symname))
                return 1;
    }
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
            if(sys_do_load_lib_from_file(fd, objectname, dirbuf, nameptr, symname))
                return 1;
#endif
    return (0);
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
        return (1); /* if lib is already loaded, dismiss. */

        /* if classname is absolute, try this first */
    if (sys_isabsolutepath(classname))
    {
            /* this is just copied from sys_open_absolute()
               LATER avoid code duplication */
        char dirbuf[MAXPDSTRING], *z = strrchr(classname, '/');
        int dirlen;
        if (!z)
            return (0);
        dirlen = (int)(z - classname);
        if (dirlen > MAXPDSTRING-1)
            dirlen = MAXPDSTRING-1;
        strncpy(dirbuf, classname, dirlen);
        dirbuf[dirlen] = 0;
        data.classname=classname+(dirlen+1);
        sys_loadlib_iter(dirbuf, &data);
    }
    data.classname = classname;
    if(!data.ok && !sys_isabsolutepath(classname)) /* don't iterate if classname is absolute */
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
    const char**dllextent;
    for(dllextent=sys_get_dllextensions(); *dllextent; dllextent++)
    {
        struct stat statbuf;
        pd_snprintf(filename, sizeof(filename), "%s%s", externalschedlibname,
            *dllextent);
        sys_bashfilename(filename, filename);
        if(!stat(filename, &statbuf))
            break;
    }

#ifdef _WIN32
    {
        HINSTANCE ntdll = LoadLibrary(filename);
        if (!ntdll)
        {
            fprintf(stderr, "%s: couldn't load external scheduler\n", filename);
            pd_error(0, "%s: couldn't load external scheduler", filename);
            return (1);
        }
        externalmainfunc =
            (t_externalschedlibmain)GetProcAddress(ntdll, "pd_extern_sched");
        if (!externalmainfunc)
            externalmainfunc =
                (t_externalschedlibmain)GetProcAddress(ntdll, "main");
    }
#elif HAVE_LIBDL
    {
        void *dlobj;
        dlobj = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
        if (!dlobj)
        {
            pd_error(0, "%s: %s", filename, dlerror());
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
        pd_snprintf(classslashclass, MAXPDSTRING, "%s/%s", objectname, objectname);
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
            return (pd_this->pd_newest);
        }
            /* otherwise we couldn't do it; just return 0 */
    }
    else pd_error(0, "%s: can't load abstraction within itself\n", s->s_name);
    pd_this->pd_newest = 0;
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

    pd_snprintf(classslashclass, MAXPDSTRING, "%s/%s", objectname, objectname);
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

t_method sys_getfunbyname(const char *name)
{
#ifdef _WIN32
    HMODULE module;
        /* Get a handle to the actual module that contains the Pd API functions.
        For this we just have to pass *any* Pd API function to GetModuleHandleEx().
        NB: GetModuleHandle(NULL) wouldn't work because it would return a handle
        to the main executable and GetProcAddress(), unlike dlsym(), does not
        reach into its dependencies. */
    if (GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&pd_typedmess, &module))
            return (t_method)GetProcAddress(module, name);
    else
    {
        fprintf(stderr, "GetModuleHandleEx() failed with error code %d\n",
            GetLastError());
        return NULL;
    }
#else
    return (t_method)dlsym(dlopen(NULL, RTLD_NOW), name);
#endif
}
