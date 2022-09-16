/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "s_stuff.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

t_printhook sys_printhook = NULL;
int sys_printtostderr;

/* escape characters for tcl/tk */
char* pdgui_strnescape(char *dst, size_t dstlen, const char *src, size_t srclen)
{
    unsigned ptin = 0, ptout = 0;
    if(!dst || !src)return 0;
    while(1)
    {
        int c = src[ptin];
        if (c == '\\' || c == '{' || c == '}' || c == '[' || c == ']') {
            dst[ptout++] = '\\';
            if (dstlen && ptout >= dstlen){
                dst[ptout-1] = 0;
                break;
            }
        }
        dst[ptout] = c;
        ptin++;
        ptout++;
        if (c==0) break;
        if (srclen && ptin  >= srclen) break;
        if (dstlen && ptout >= dstlen) break;
    }

    if(!dstlen || ptout < dstlen)
        dst[ptout]=0;
    else
        dst[dstlen-1]=0;

    return dst;
}

static void dopost(const char *s)
{
    if (STUFF->st_printhook)
        (*STUFF->st_printhook)(s);
    else if (sys_printtostderr || !sys_havegui())
    {
#ifdef _WIN32
    #ifdef _MSC_VER
        fwprintf(stderr, L"%S", s);
    #else
        fwprintf(stderr, L"%s", s);
    #endif
        fflush(stderr);
#else
        fprintf(stderr, "%s", s);
#endif
    }
    else
    {
        pdgui_vmess("::pdwindow::post", "s", s);
    }
}

static void doerror(const void *object, const char *s)
{
    char upbuf[MAXPDSTRING];
    upbuf[MAXPDSTRING-1]=0;

    // what about sys_printhook_error ?
    if (STUFF->st_printhook)
    {
        snprintf(upbuf, MAXPDSTRING-1, "error: %s", s);
        (*STUFF->st_printhook)(upbuf);
    }
    else if (sys_printtostderr)
    {
#ifdef _WIN32
    #ifdef _MSC_VER
        fwprintf(stderr, L"error: %S", s);
    #else
        fwprintf(stderr, L"error: %s", s);
    #endif
        fflush(stderr);
#else
        fprintf(stderr, "error: %s", s);
#endif
    }
    else
        pdgui_vmess("::pdwindow::logpost", "ois",
                  object, 1, s);
}

static void dologpost(const void *object, const int level, const char *s)
{
    char upbuf[MAXPDSTRING];
    upbuf[MAXPDSTRING-1]=0;
        /* if it's a verbose message and we aren't set to 'verbose' just do
            nothing */
    if (level >= PD_VERBOSE && !sys_verbose)
        return;
    // what about sys_printhook_verbose ?
    if (STUFF->st_printhook)
    {
        snprintf(upbuf, MAXPDSTRING-1, "verbose(%d): %s", level, s);
        (*STUFF->st_printhook)(upbuf);
    }
    else if (sys_printtostderr)
    {
#ifdef _WIN32
    #ifdef _MSC_VER
        fwprintf(stderr, L"verbose(%d): %S", level, s);
    #else
        fwprintf(stderr, L"verbose(%d): %s", level, s);
    #endif
        fflush(stderr);
#else
        fprintf(stderr, "verbose(%d): %s", level, s);
#endif
    }
    else
        pdgui_vmess("::pdwindow::logpost", "ois",
                  object, level, s);
}

void logpost(const void *object, int level, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    if (level > PD_DEBUG && !sys_verbose) return;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

    dologpost(object, level, buf);
}

void startlogpost(const void *object, const int level, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    if (level > PD_DEBUG && !sys_verbose) return;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);

    dologpost(object, level, buf);
}


void post(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

    dopost(buf);
}

void startpost(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);

    dopost(buf);
}

void poststring(const char *s)
{
    dopost(" ");

    dopost(s);
}

void postatom(int argc, const t_atom *argv)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        char buf[MAXPDSTRING];
        atom_string(argv+i, buf, MAXPDSTRING);
        poststring(buf);
    }
}

void postfloat(t_float f)
{
    char buf[80];
    t_atom a;
    SETFLOAT(&a, f);

    postatom(1, &a);
}

void endpost(void)
{
    if (STUFF->st_printhook)
        (*STUFF->st_printhook)("\n");
    else if (sys_printtostderr)
        fprintf(stderr, "\n");
    else post("");
}

  /* keep this in the Pd app for binary extern compatibility but don't
  include in libpd because it conflicts with the posix pd_error(0, ) function. */
#ifdef PD_INTERNAL
EXTERN void error(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;

    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

    doerror(NULL, buf);
}
#endif

/* deprecated in favor of logpost() */
void verbose(int level, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;

    if (level > sys_verbose) return;

    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

        /* log levels for verbose() traditionally start at -3,
        so we have to adjust it before passing it on to dologpost() */
    dologpost(NULL, level + 3, buf);
}

    /* here's the good way to log errors -- keep a pointer to the
    offending or offended object around so the user can search for it
    later. */

static const void *error_object;
static char error_string[256];
void canvas_finderror(const void *object);

void pd_error(const void *object, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    static int saidit;

    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

    doerror(object, buf);

    error_object = object;
    strncpy(error_string, buf, 256);
    error_string[255] = 0;

    if (!saidit)
    {
        logpost(NULL, 4,
                "... you might be able to track this down from the Find menu.");
        saidit = 1;
    }
}

void glob_finderror(t_pd *dummy)
{
    if (!error_object)
        post("no findable error yet");
    else
    {
        post("last trackable error:");
        post("%s", error_string);
        canvas_finderror(error_object);
    }
}

void glob_findinstance(t_pd *dummy, t_symbol*s)
{
    // revert s to (potential) pointer to object
    PD_LONGINTTYPE obj = 0;
    const char*addr;
    if(!s || !s->s_name)
        return;
    addr = s->s_name;
    if (('.' != addr[0]) && ('0' != addr[0]))
        return;
    if (!sscanf(addr+1, "x%lx", &obj))
        return;

    if(!obj)
        return;

    canvas_finderror((void *)obj);
}

void bug(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);

    pd_error(0, "consistency check failed: %s", buf);
}

    /* don't use these.  They're included for binary compatibility with
    old externs but never worked and now do nothing. */
void sys_logerror(const char *object, const char *s) {}
void sys_unixerror(const char *object) {}
void sys_ouch(void) {}
