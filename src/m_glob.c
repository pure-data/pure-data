/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include "m_imp.h"

t_class *glob_pdobject;
static t_class *maxclass;

int sys_perf;   /* true if we should query user on close and quit */
int pd_compatibilitylevel = 100000;  /* e.g., 43 for pd 0.43 compatibility */

/* These "glob" routines, which implement messages to Pd, are from all
over.  Some others are prototyped in m_imp.h as well. */

void glob_menunew(void *dummy, t_symbol *name, t_symbol *dir);
void glob_verifyquit(void *dummy, t_floatarg f);
void glob_dsp(void *dummy, t_symbol *s, int argc, t_atom *argv);
void glob_meters(void *dummy, t_floatarg f);
void glob_key(void *dummy, t_symbol *s, int ac, t_atom *av);
void glob_audiostatus(void *dummy);
void glob_finderror(t_pd *dummy);
void glob_findinstance(t_pd *dummy, t_symbol*s);
void glob_audio_properties(t_pd *dummy, t_floatarg flongform);
void glob_audio_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv);
void glob_audio_setapi(t_pd *dummy, t_floatarg f);
void glob_midi_properties(t_pd *dummy, t_floatarg flongform);
void glob_midi_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv);
void glob_midi_setapi(t_pd *dummy, t_floatarg f);
void glob_start_path_dialog(t_pd *dummy, t_floatarg flongform);
void glob_path_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv);
void glob_start_startup_dialog(t_pd *dummy, t_floatarg flongform);
void glob_startup_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv);
void glob_ping(t_pd *dummy);
void glob_watchdog(t_pd *dummy);
void glob_savepreferences(t_pd *dummy);

static void glob_compatibility(t_pd *dummy, t_floatarg level)
{
    int dspwas = canvas_suspend_dsp();
    pd_compatibilitylevel = 0.5 + 100. * level;
    canvas_resume_dsp(dspwas);
}

#ifdef _WIN32
void glob_audio(void *dummy, t_floatarg adc, t_floatarg dac);
#endif

/* a method you add for debugging printout */
void glob_foo(void *dummy, t_symbol *s, int argc, t_atom *argv);

#if 1
void glob_foo(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    post("foo 1");
    printf("barbarbar 2\n");
    post("foo 3");
}
#endif

static void glob_version(t_pd *dummy, float f)
{
    if (f > (PD_MAJOR_VERSION + 0.01*PD_MINOR_VERSION + 0.001))
    {
        static int warned;
        if (warned < 1)
            post("warning: file format (%g) newer than this version (%g) of Pd",
                f, PD_MAJOR_VERSION + 0.01*PD_MINOR_VERSION);
        else if (warned < 2)
            post("(... more file format messages suppressed)");
        warned++;
    }
}

static void glob_perf(t_pd *dummy, float f)
{
    sys_perf = (f != 0);
}

void max_default(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    char str[80];
    startpost("%s: unknown message %s ", class_getname(pd_class(x)),
        s->s_name);
    for (i = 0; i < argc; i++)
    {
        atom_string(argv+i, str, 80);
        poststring(str);
    }
    endpost();
}

void glob_init(void)
{
    maxclass = class_new(gensym("max"), 0, 0, sizeof(t_pd),
        CLASS_DEFAULT, A_NULL);
    class_addanything(maxclass, max_default);
    pd_bind(&maxclass, gensym("max"));

    glob_pdobject = class_new(gensym("pd"), 0, 0, sizeof(t_pd),
        CLASS_DEFAULT, A_NULL);
    class_addmethod(glob_pdobject, (t_method)glob_initfromgui, gensym("init"),
        A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_menunew, gensym("menunew"),
        A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(glob_pdobject, (t_method)glob_evalfile, gensym("open"),
        A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(glob_pdobject, (t_method)glob_quit, gensym("quit"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_verifyquit,
        gensym("verifyquit"), A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_foo, gensym("foo"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_dsp, gensym("dsp"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_meters, gensym("meters"),
        A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_key, gensym("key"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_audiostatus,
        gensym("audiostatus"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_finderror,
        gensym("finderror"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_findinstance,
        gensym("findinstance"), A_SYMBOL, 0);
    class_addmethod(glob_pdobject, (t_method)glob_audio_properties,
        gensym("audio-properties"), A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_audio_dialog,
        gensym("audio-dialog"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_audio_setapi,
        gensym("audio-setapi"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_midi_setapi,
        gensym("midi-setapi"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_midi_properties,
        gensym("midi-properties"), A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_midi_dialog,
        gensym("midi-dialog"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_start_path_dialog,
        gensym("start-path-dialog"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_path_dialog,
        gensym("path-dialog"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_start_startup_dialog,
        gensym("start-startup-dialog"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_startup_dialog,
        gensym("startup-dialog"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_ping, gensym("ping"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_savepreferences,
        gensym("save-preferences"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_version,
        gensym("version"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_perf,
        gensym("perf"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_compatibility,
        gensym("compatibility"), A_FLOAT, 0);
#if defined(__linux__) || defined(__FreeBSD_kernel__)
    class_addmethod(glob_pdobject, (t_method)glob_watchdog,
        gensym("watchdog"), 0);
#endif
    class_addanything(glob_pdobject, max_default);
    pd_bind(&glob_pdobject, gensym("pd"));
}

    /* function to return version number at run time.  Any of the
    calling pointers may be zero in case you don't need all of them. */
void sys_getversion(int *major, int *minor, int *bugfix)
{
    if (major)
        *major = PD_MAJOR_VERSION;
    if (minor)
        *minor = PD_MINOR_VERSION;
    if (bugfix)
        *bugfix = PD_BUGFIX_VERSION;
}

