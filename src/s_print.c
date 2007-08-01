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

t_printhook sys_printhook;
int sys_printtostderr;

static void dopost(const char *s)
{
    if (sys_printhook)
        (*sys_printhook)(s);
    else if (sys_printtostderr)
        fprintf(stderr, "%s", s);
    else
    {
        char upbuf[MAXPDSTRING];
        int ptin = 0, ptout = 0, len = strlen(s);
        static int heldcr = 0;
        if (heldcr)
            upbuf[ptout++] = '\n', heldcr = 0;
        for (; ptin < len && ptout < MAXPDSTRING-3;
            ptin++, ptout++)
        {
            int c = s[ptin];
            if (c == '\\' || c == '{' || c == '}' || c == ';')
                upbuf[ptout++] = '\\';
            upbuf[ptout] = s[ptin];
        }
        if (ptout && upbuf[ptout-1] == '\n')
            upbuf[--ptout] = 0, heldcr = 1;
        upbuf[ptout] = 0;
        sys_vgui("pdtk_post {%s}\n", upbuf);
    }
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

void postatom(int argc, t_atom *argv)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        char buf[80];
        atom_string(argv+i, buf, 80);
        poststring(buf);
    }
}

void postfloat(float f)
{
    char buf[80];
    t_atom a;
    SETFLOAT(&a, f);
    postatom(1, &a);
}

void endpost(void)
{
    dopost("\n");
}

void error(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    dopost("error: ");
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");
    dopost(buf);
}

void verbose(int level, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    if(level>sys_verbose)return;
    dopost("verbose(");
    postfloat((float)level);
    dopost("):");
    
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");
    dopost(buf);
}

    /* here's the good way to log errors -- keep a pointer to the
    offending or offended object around so the user can search for it
    later. */

static void *error_object;
static char error_string[256];
void canvas_finderror(void *object);

void pd_error(void *object, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    static int saidit;
    dopost("error: ");
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");
    dopost(buf);
    error_object = object;
    if (!saidit)
    {
        post("... you might be able to track this down from the Find menu.");
        saidit = 1;
    }
}

void glob_finderror(t_pd *dummy)
{
    if (!error_object)
        post("no findable error yet.");
    else
    {
        post("last trackable error:");
        post("%s", error_string);
        canvas_finderror(error_object);
    }
}

void bug(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    t_int arg[8];
    int i;
    dopost("consistency check failed: ");
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");
    dopost(buf);
}

    /* this isn't worked out yet. */
static const char *errobject;
static const char *errstring;

void sys_logerror(const char *object, const char *s)
{
    errobject = object;
    errstring = s;
}

void sys_unixerror(const char *object)
{
    errobject = object;
    errstring = strerror(errno);
}

void sys_ouch(void)
{
    if (*errobject) error("%s: %s", errobject, errstring);
    else error("%s", errstring);
}
