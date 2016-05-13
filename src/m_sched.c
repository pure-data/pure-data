/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  scheduling stuff  */

#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#ifdef _WIN32
#include <windows.h>
#endif

    /* LATER consider making this variable.  It's now the LCM of all sample
    rates we expect to see: 32000, 44100, 48000, 88200, 96000. */
#define TIMEUNITPERMSEC (32. * 441.)
#define TIMEUNITPERSECOND (TIMEUNITPERMSEC * 1000.)

#ifndef THREAD_LOCKING
#define THREAD_LOCKING 1
#endif

#if THREAD_LOCKING
#include "pthread.h"
#endif

#define SYS_QUIT_QUIT 1
#define SYS_QUIT_RESTART 2
static int sys_quit;

int sys_schedblocksize = DEFDACBLKSIZE;
int sys_usecsincelastsleep(void);
int sys_sleepgrain;

typedef void (*t_clockmethod)(void *client);

struct _clock
{
    double c_settime;       /* in TIMEUNITS; <0 if unset */
    void *c_owner;
    t_clockmethod c_fn;
    struct _clock *c_next;
    t_float c_unit;         /* >0 if in TIMEUNITS; <0 if in samples */
};

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

t_clock *clock_new(void *owner, t_method fn)
{
    t_clock *x = (t_clock *)getbytes(sizeof *x);
    x->c_settime = -1;
    x->c_owner = owner;
    x->c_fn = (t_clockmethod)fn;
    x->c_next = 0;
    x->c_unit = TIMEUNITPERMSEC;
    return (x);
}

void clock_unset(t_clock *x)
{
    if (x->c_settime >= 0)
    {
        if (x == pd_this->pd_clock_setlist)
            pd_this->pd_clock_setlist = x->c_next;
        else
        {
            t_clock *x2 = pd_this->pd_clock_setlist;
            while (x2->c_next != x) x2 = x2->c_next;
            x2->c_next = x->c_next;
        }
        x->c_settime = -1;
    }
}

    /* set the clock to call back at an absolute system time */
void clock_set(t_clock *x, double setticks)
{
    if (setticks < pd_this->pd_systime) setticks = pd_this->pd_systime;
    clock_unset(x);
    x->c_settime = setticks;
    if (pd_this->pd_clock_setlist &&
        pd_this->pd_clock_setlist->c_settime <= setticks)
    {
        t_clock *cbefore, *cafter;
        for (cbefore = pd_this->pd_clock_setlist,
            cafter = pd_this->pd_clock_setlist->c_next;
                cbefore; cbefore = cafter, cafter = cbefore->c_next)
        {
            if (!cafter || cafter->c_settime > setticks)
            {
                cbefore->c_next = x;
                x->c_next = cafter;
                return;
            }
        }
    }
    else x->c_next = pd_this->pd_clock_setlist, pd_this->pd_clock_setlist = x;
}

    /* set the clock to call back after a delay in msec */
void clock_delay(t_clock *x, double delaytime)
{
    clock_set(x, (x->c_unit > 0 ?
        pd_this->pd_systime + x->c_unit * delaytime :
            pd_this->pd_systime - (x->c_unit*(TIMEUNITPERSECOND/sys_dacsr)) * delaytime));
}

    /* set the time unit in msec or (if 'samps' is set) in samples.  This
    is flagged by setting c_unit negative.  If the clock is currently set,
    recalculate the delay based on the new unit and reschedule */
void clock_setunit(t_clock *x, double timeunit, int sampflag)
{
    double timeleft;
    if (timeunit <= 0)
        timeunit = 1;
    /* if no change, return to avoid truncation errors recalculating delay */
    if ((sampflag && (timeunit == -x->c_unit)) ||
        (!sampflag && (timeunit == x->c_unit * TIMEUNITPERMSEC)))
            return;

        /* figure out time left in the units we were in */
    timeleft = (x->c_settime < 0 ? -1 :
        (x->c_settime - pd_this->pd_systime)/((x->c_unit > 0)? x->c_unit :
            (x->c_unit*(TIMEUNITPERSECOND/sys_dacsr))));
    if (sampflag)
        x->c_unit = -timeunit;  /* negate to flag sample-based */
    else x->c_unit = timeunit * TIMEUNITPERMSEC;
    if (timeleft >= 0)  /* reschedule if already set */
        clock_delay(x, timeleft);
}

    /* get current logical time.  We don't specify what units this is in;
    use clock_gettimesince() to measure intervals from time of this call. */
double clock_getlogicaltime( void)
{
    return (pd_this->pd_systime);
}

    /* OBSOLETE (misleading) function name kept for compatibility */
double clock_getsystime( void) { return (pd_this->pd_systime); }

    /* elapsed time in milliseconds since the given system time */
double clock_gettimesince(double prevsystime)
{
    return ((pd_this->pd_systime - prevsystime)/TIMEUNITPERMSEC);
}

    /* elapsed time in units, ala clock_setunit(), since given system time */
double clock_gettimesincewithunits(double prevsystime,
    double units, int sampflag)
{
            /* If in samples, divide TIMEUNITPERSECOND/sys_dacsr first (at
            cost of an extra division) since it's probably an integer and if
            units == 1 and (sys_time - prevsystime) is an integer number of
            DSP ticks, the result will be exact. */
    if (sampflag)
        return ((pd_this->pd_systime - prevsystime)/
            ((TIMEUNITPERSECOND/sys_dacsr)*units));
    else return ((pd_this->pd_systime - prevsystime)/(TIMEUNITPERMSEC*units));
}

    /* what value the system clock will have after a delay */
double clock_getsystimeafter(double delaytime)
{
    return (pd_this->pd_systime + TIMEUNITPERMSEC * delaytime);
}

void clock_free(t_clock *x)
{
    clock_unset(x);
    freebytes(x, sizeof *x);
}

/* the following routines maintain a real-execution-time histogram of the
various phases of real-time execution. */

static int sys_bin[] = {0, 2, 5, 10, 20, 30, 50, 100, 1000};
#define NBIN (sizeof(sys_bin)/sizeof(*sys_bin))
#define NHIST 10
static int sys_histogram[NHIST][NBIN];
static double sys_histtime;
static int sched_diddsp, sched_didpoll, sched_didnothing;

void sys_clearhist( void)
{
    unsigned int i, j;
    for (i = 0; i < NHIST; i++)
        for (j = 0; j < NBIN; j++) sys_histogram[i][j] = 0;
    sys_histtime = sys_getrealtime();
    sched_diddsp = sched_didpoll = sched_didnothing = 0;
}

void sys_printhist( void)
{
    unsigned int i, j;
    for (i = 0; i < NHIST; i++)
    {
        int doit = 0;
        for (j = 0; j < NBIN; j++) if (sys_histogram[i][j]) doit = 1;
        if (doit)
        {
            post("%2d %8d %8d %8d %8d %8d %8d %8d %8d", i,
                sys_histogram[i][0],
                sys_histogram[i][1],
                sys_histogram[i][2],
                sys_histogram[i][3],
                sys_histogram[i][4],
                sys_histogram[i][5],
                sys_histogram[i][6],
                sys_histogram[i][7]);
        }
    }
    post("dsp %d, pollgui %d, nothing %d",
        sched_diddsp, sched_didpoll, sched_didnothing);
}

static int sys_histphase;

int sys_addhist(int phase)
{
    int i, j, phasewas = sys_histphase;
    double newtime = sys_getrealtime();
    int msec = (newtime - sys_histtime) * 1000.;
    for (j = NBIN-1; j >= 0; j--)
    {
        if (msec >= sys_bin[j])
        {
            sys_histogram[phasewas][j]++;
            break;
        }
    }
    sys_histtime = newtime;
    sys_histphase = phase;
    return (phasewas);
}

#define NRESYNC 20

typedef struct _resync
{
    int r_ntick;
    int r_error;
} t_resync;

static int oss_resyncphase = 0;
static int oss_nresync = 0;
static t_resync oss_resync[NRESYNC];


static char *(oss_errornames[]) = {
"unknown",
"ADC blocked",
"DAC blocked",
"A/D/A sync",
"data late"
};

void glob_audiostatus(void)
{
    int dev, nresync, nresyncphase, i;
    nresync = (oss_nresync >= NRESYNC ? NRESYNC : oss_nresync);
    nresyncphase = oss_resyncphase - 1;
    post("audio I/O error history:");
    post("seconds ago\terror type");
    for (i = 0; i < nresync; i++)
    {
        int errtype;
        if (nresyncphase < 0)
            nresyncphase += NRESYNC;
        errtype = oss_resync[nresyncphase].r_error;
        if (errtype < 0 || errtype > 4)
            errtype = 0;

        post("%9.2f\t%s",
            (sched_diddsp - oss_resync[nresyncphase].r_ntick)
                * ((double)sys_schedblocksize) / sys_dacsr,
            oss_errornames[errtype]);
        nresyncphase--;
    }
}

static int sched_diored;
static int sched_dioredtime;
static int sched_meterson;

void sys_log_error(int type)
{
    oss_resync[oss_resyncphase].r_ntick = sched_diddsp;
    oss_resync[oss_resyncphase].r_error = type;
    oss_nresync++;
    if (++oss_resyncphase == NRESYNC) oss_resyncphase = 0;
    if (type != ERR_NOTHING && !sched_diored &&
        (sched_diddsp >= sched_dioredtime))
    {
        sys_vgui("pdtk_pd_dio 1\n");
        sched_diored = 1;
    }
    sched_dioredtime =
        sched_diddsp + (int)(sys_dacsr /(double)sys_schedblocksize);
}

static int sched_lastinclip, sched_lastoutclip,
    sched_lastindb, sched_lastoutdb;

void glob_watchdog(t_pd *dummy);

static void sched_pollformeters( void)
{
    int inclip, outclip, indb, outdb;
    static int sched_nextmeterpolltime, sched_nextpingtime;

        /* if there's no GUI but we're running in "realtime", here is
        where we arrange to ping the watchdog every 2 seconds. */
#if defined(__linux__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__GNU__)
    if (sys_nogui && sys_hipriority && (sched_diddsp - sched_nextpingtime > 0))
    {
        glob_watchdog(0);
            /* ping every 2 seconds */
        sched_nextpingtime = sched_diddsp +
            2 * (int)(sys_dacsr /(double)sys_schedblocksize);
    }
#endif

    if (sched_diddsp - sched_nextmeterpolltime < 0)
        return;
    if (sched_diored && (sched_diddsp - sched_dioredtime > 0))
    {
        sys_vgui("pdtk_pd_dio 0\n");
        sched_diored = 0;
    }
    if (sched_meterson)
    {
        t_sample inmax, outmax;
        sys_getmeters(&inmax, &outmax);
        indb = 0.5 + rmstodb(inmax);
        outdb = 0.5 + rmstodb(outmax);
        inclip = (inmax > 0.999);
        outclip = (outmax >= 1.0);
    }
    else
    {
        indb = outdb = 0;
        inclip = outclip = 0;
    }
    if (inclip != sched_lastinclip || outclip != sched_lastoutclip
        || indb != sched_lastindb || outdb != sched_lastoutdb)
    {
        sys_vgui("pdtk_pd_meters %d %d %d %d\n", indb, outdb, inclip, outclip);
        sched_lastinclip = inclip;
        sched_lastoutclip = outclip;
        sched_lastindb = indb;
        sched_lastoutdb = outdb;
    }
    sched_nextmeterpolltime =
        sched_diddsp + (int)(sys_dacsr /(double)sys_schedblocksize);
}

void glob_meters(void *dummy, t_float f)
{
    if (f == 0)
        sys_getmeters(0, 0);
    sched_meterson = (f != 0);
    sched_lastinclip = sched_lastoutclip = sched_lastindb = sched_lastoutdb =
        -1;
}

#if 0
void glob_foo(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    if (argc) sys_clearhist();
    else sys_printhist();
}
#endif

void dsp_tick(void);

static int sched_useaudio = SCHED_AUDIO_NONE;
static double sched_referencerealtime, sched_referencelogicaltime;
double sys_time_per_dsp_tick;

void sched_reopenmeplease(void)   /* request from s_audio for deferred reopen */
{
    sys_quit = SYS_QUIT_RESTART;
}

void sched_set_using_audio(int flag)
{
    sched_useaudio = flag;
    if (flag == SCHED_AUDIO_NONE)
    {
        sched_referencerealtime = sys_getrealtime();
        sched_referencelogicaltime = clock_getlogicaltime();
    }
        if (flag == SCHED_AUDIO_CALLBACK &&
            sched_useaudio != SCHED_AUDIO_CALLBACK)
                sys_quit = SYS_QUIT_RESTART;
        if (flag != SCHED_AUDIO_CALLBACK &&
            sched_useaudio == SCHED_AUDIO_CALLBACK)
                post("sorry, can't turn off callbacks yet; restart Pd");
                    /* not right yet! */

    sys_time_per_dsp_tick = (TIMEUNITPERSECOND) *
        ((double)sys_schedblocksize) / sys_dacsr;
    sys_vgui("pdtk_pd_audio %s\n", flag ? "on" : "off");
}

    /* take the scheduler forward one DSP tick, also handling clock timeouts */
void sched_tick( void)
{
    double next_sys_time = pd_this->pd_systime + sys_time_per_dsp_tick;
    int countdown = 5000;
    while (pd_this->pd_clock_setlist &&
        pd_this->pd_clock_setlist->c_settime < next_sys_time)
    {
        t_clock *c = pd_this->pd_clock_setlist;
        pd_this->pd_systime = c->c_settime;
        clock_unset(pd_this->pd_clock_setlist);
        outlet_setstacklim();
        (*c->c_fn)(c->c_owner);
        if (!countdown--)
        {
            countdown = 5000;
            sys_pollgui();
        }
        if (sys_quit)
            return;
    }
    pd_this->pd_systime = next_sys_time;
    dsp_tick();
    sched_diddsp++;
}

/*
Here is Pd's "main loop."  This routine dispatches clock timeouts and DSP
"ticks" deterministically, and polls for input from MIDI and the GUI.  If
we're left idle we also poll for graphics updates; but these are considered
lower priority than the rest.

The time source is normally the audio I/O subsystem via the "sys_send_dacs()"
call.  This call returns true if samples were transferred; false means that
the audio I/O system is still busy with previous transfers.
*/

void sys_pollmidiqueue( void);
void sys_initmidiqueue( void);

 /* sys_idlehook is a hook the user can fill in to grab idle time.  Return
nonzero if you actually used the time; otherwise we're really really idle and
will now sleep. */
int (*sys_idlehook)(void);

static void m_pollingscheduler( void)
{
    int idlecount = 0;
    sys_time_per_dsp_tick = (TIMEUNITPERSECOND) *
        ((double)sys_schedblocksize) / sys_dacsr;

#if THREAD_LOCKING
        sys_lock();
#endif

    sys_clearhist();
    if (sys_sleepgrain < 100)
        sys_sleepgrain = sys_schedadvance/4;
    if (sys_sleepgrain < 100)
        sys_sleepgrain = 100;
    else if (sys_sleepgrain > 5000)
        sys_sleepgrain = 5000;
    sys_initmidiqueue();
    while (!sys_quit)
    {
        int didsomething = 0;
        int timeforward;

        sys_addhist(0);
    waitfortick:
        if (sched_useaudio != SCHED_AUDIO_NONE)
        {
#if THREAD_LOCKING
            /* T.Grill - send_dacs may sleep ->
                unlock thread lock make that time available
                - could messaging do any harm while sys_send_dacs is running?
            */
            sys_unlock();
#endif
            timeforward = sys_send_dacs();
#if THREAD_LOCKING
            /* T.Grill - done */
            sys_lock();
#endif
                /* if dacs remain "idle" for 1 sec, they're hung up. */
            if (timeforward != 0)
                idlecount = 0;
            else
            {
                idlecount++;
                if (!(idlecount & 31))
                {
                    static double idletime;
                    if (sched_useaudio != SCHED_AUDIO_POLL)
                    {
                            bug("m_pollingscheduler\n");
                            return;
                    }
                        /* on 32nd idle, start a clock watch;  every
                        32 ensuing idles, check it */
                    if (idlecount == 32)
                        idletime = sys_getrealtime();
                    else if (sys_getrealtime() - idletime > 1.)
                    {
                        error("audio I/O stuck... closing audio\n");
                        sys_close_audio();
                        sched_set_using_audio(SCHED_AUDIO_NONE);
                        goto waitfortick;
                    }
                }
            }
        }
        else
        {
            if (1000. * (sys_getrealtime() - sched_referencerealtime)
                > clock_gettimesince(sched_referencelogicaltime))
                    timeforward = SENDDACS_YES;
            else timeforward = SENDDACS_NO;
        }
        sys_setmiditimediff(0, 1e-6 * sys_schedadvance);
        sys_addhist(1);
        if (timeforward != SENDDACS_NO)
            sched_tick();
        if (timeforward == SENDDACS_YES)
            didsomething = 1;

        sys_addhist(2);
        sys_pollmidiqueue();
        if (sys_pollgui())
        {
            if (!didsomething)
                sched_didpoll++;
            didsomething = 1;
        }
        sys_addhist(3);
            /* test for idle; if so, do graphics updates. */
        if (!didsomething)
        {
            sched_pollformeters();
            sys_reportidle();
#if THREAD_LOCKING
            sys_unlock();   /* unlock while we idle */
#endif
                /* call externally installed idle function if any. */
            if (!sys_idlehook || !sys_idlehook())
            {
                    /* if even that had nothing to do, sleep. */
                if (timeforward != SENDDACS_SLEPT)
                    sys_microsleep(sys_sleepgrain);
            }
#if THREAD_LOCKING
            sys_lock();
#endif
            sys_addhist(5);
            sched_didnothing++;
        }
    }

#if THREAD_LOCKING
    sys_unlock();
#endif
}

void sched_audio_callbackfn(void)
{
    sys_lock();
    sys_setmiditimediff(0, 1e-6 * sys_schedadvance);
    sys_addhist(1);
    sched_tick();
    sys_addhist(2);
    sys_pollmidiqueue();
    sys_addhist(3);
    sys_pollgui();
    sys_addhist(5);
    sched_pollformeters();
    sys_addhist(0);
    sys_unlock();
}

static void m_callbackscheduler(void)
{
    sys_initmidiqueue();
    while (!sys_quit)
    {
        double timewas = pd_this->pd_systime;
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
        if (pd_this->pd_systime == timewas)
        {
            sys_lock();
            sys_pollgui();
            sched_tick();
            sys_unlock();
        }
        if (sys_idlehook)
            sys_idlehook();
    }
}

int m_mainloop(void)
{
    while (sys_quit != SYS_QUIT_QUIT)
    {
        if (sched_useaudio == SCHED_AUDIO_CALLBACK)
            m_callbackscheduler();
        else m_pollingscheduler();
        if (sys_quit == SYS_QUIT_RESTART)
        {
            sys_quit = 0;
            if (audio_isopen())
            {
                sys_close_audio();
                sys_reopen_audio();
            }
        }
    }
    return (0);
}

int m_batchmain(void)
{
    sys_time_per_dsp_tick = (TIMEUNITPERSECOND) *
        ((double)sys_schedblocksize) / sys_dacsr;
    while (sys_quit != SYS_QUIT_QUIT)
        sched_tick();
    return (0);
}

/* ------------ thread locking ------------------- */

#if THREAD_LOCKING
static pthread_mutex_t sys_mutex = PTHREAD_MUTEX_INITIALIZER;

void sys_lock(void)
{
    pthread_mutex_lock(&sys_mutex);
}

void sys_unlock(void)
{
    pthread_mutex_unlock(&sys_mutex);
}

int sys_trylock(void)
{
    return pthread_mutex_trylock(&sys_mutex);
}

#else

void sys_lock(void) {}
void sys_unlock(void) {}
int sys_trylock(void) {return (1);}

#endif

void sys_exit(void)
{
    sys_quit = SYS_QUIT_QUIT;
}
