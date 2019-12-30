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

t_printhook sys_printhook;
int sys_printtostderr;

/* escape characters for tcl/tk */
char* pdgui_strnescape(char *dst, size_t dstlen, const char *src, size_t srclen)
{
    unsigned ptin = 0, ptout = 0;
    if(!dst || !src)return 0;
    while(1)
    {
        int c = src[ptin];
        if (c == '\\' || c == '{' || c == '}') {
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

static char* strnpointerid(char *dest, const void *pointer, size_t len)
{
    *dest=0;
    if (pointer)
        snprintf(dest, len, ".x%lx", (unsigned long)pointer);
    return dest;
}

static void dopost(const char *s)
{
    if (sys_printhook)
        (*sys_printhook)(s);
    else if (sys_printtostderr || !sys_havegui())
#ifdef _WIN32
        fwprintf(stderr, L"%S", s);
#else
        fprintf(stderr, "%s", s);
#endif
    else
    {
        char upbuf[MAXPDSTRING];
        sys_vgui("::pdwindow::post {%s}\n", pdgui_strnescape(upbuf, MAXPDSTRING, s, 0));
    }
}

static void doerror(const void *object, const char *s)
{
    char upbuf[MAXPDSTRING];
    upbuf[MAXPDSTRING-1]=0;

    // what about sys_printhook_error ?
    if (sys_printhook)
    {
        snprintf(upbuf, MAXPDSTRING-1, "error: %s", s);
        (*sys_printhook)(upbuf);
    }
    else if (sys_printtostderr)
        fprintf(stderr, "error: %s", s);
    else
    {
        char obuf[MAXPDSTRING];
        sys_vgui("::pdwindow::logpost {%s} 1 {%s}\n",
                 strnpointerid(obuf, object, MAXPDSTRING),
                 pdgui_strnescape(upbuf, MAXPDSTRING, s, 0));
    }
}

static void dologpost(const void *object, const int level, const char *s)
{
    char upbuf[MAXPDSTRING];
    upbuf[MAXPDSTRING-1]=0;

    // what about sys_printhook_verbose ?
    if (sys_printhook)
    {
        snprintf(upbuf, MAXPDSTRING-1, "verbose(%d): %s", level, s);
        (*sys_printhook)(upbuf);
    }
    else if (sys_printtostderr)
    {
        fprintf(stderr, "verbose(%d): %s", level, s);
    }
    else
    {
        char obuf[MAXPDSTRING];
        sys_vgui("::pdwindow::logpost {%s} %d {%s}\n",
                 strnpointerid(obuf, object, MAXPDSTRING),
                 level, pdgui_strnescape(upbuf, MAXPDSTRING, s, 0));
    }
}

void logpost(const void *object, const int level, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

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
    if (sys_printhook)
        (*sys_printhook)("\n");
    else if (sys_printtostderr)
        fprintf(stderr, "\n");
    else post("");
}

void error(const char *fmt, ...)
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

void verbose(int level, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    int loglevel=level+3;

    if(level>sys_verbose)return;

    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

    dologpost(NULL, loglevel, buf);
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
    if (sscanf(s->s_name, ".x%lx", &obj))
    {
        if (obj)
        {
            canvas_finderror((void *)obj);
        }
    }
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

    error("consistency check failed: %s", buf);
}

    /* don't use these.  They're included for binary compatibility with
    old externs but never worked and now do nothing. */
void sys_logerror(const char *object, const char *s) {}
void sys_unixerror(const char *object) {}
void sys_ouch(void) {}
