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
#define SYSTIMEPERTICK \
    ((STUFF->st_schedblocksize/STUFF->st_dacsr) * TIMEUNITPERSECOND)
#define APPROXTICKSPERSEC \
    ((int)(STUFF->st_dacsr /(double)STUFF->st_schedblocksize))

#define SYS_QUIT_QUIT 1
#define SYS_QUIT_RESTART 2
static int sys_quit;
extern int sys_nosleep;

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
            pd_this->pd_systime -
                (x->c_unit*(TIMEUNITPERSECOND/STUFF->st_dacsr)) * delaytime));
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
            (x->c_unit*(TIMEUNITPERSECOND/STUFF->st_dacsr))));
    if (sampflag)
        x->c_unit = -timeunit;  /* negate to flag sample-based */
    else x->c_unit = timeunit * TIMEUNITPERMSEC;
    if (timeleft >= 0)  /* reschedule if already set */
        clock_delay(x, timeleft);
}

    /* get current logical time.  We don't specify what units this is in;
    use clock_gettimesince() to measure intervals from time of this call. */
double clock_getlogicaltime(void)
{
    return (pd_this->pd_systime);
}

    /* OBSOLETE (misleading) function name kept for compatibility */
double clock_getsystime(void) { return (pd_this->pd_systime); }

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
            ((TIMEUNITPERSECOND/STUFF->st_dacsr)*units));
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


void glob_audiostatus(void)
{
    /* rewrite me */
}

static int sched_diored;
static int sched_dioredtime;
static int sched_meterson;
static int sched_counter;

static void sys_addhist(int n) {}   /* maybe revive this later for profiling */
static void sys_clearhist(void) {}

void sys_log_error(int type)
{
    if (type != ERR_NOTHING && !sched_diored &&
        (sched_counter >= sched_dioredtime))
    {
        pdgui_vmess("pdtk_pd_dio", "i", 1);
        sched_diored = 1;
    }
    sched_dioredtime = sched_counter + APPROXTICKSPERSEC;
}

static int sched_lastinclip, sched_lastoutclip,
    sched_lastindb, sched_lastoutdb;

void glob_watchdog(t_pd *dummy);

static float sched_fastforward;

void glob_fastforward(void *dummy, t_floatarg f)
{
    sched_fastforward = TIMEUNITPERMSEC * f;
}

void dsp_tick(void);

static int sched_useaudio = SCHED_AUDIO_NONE;
static double sched_referencerealtime, sched_referencelogicaltime;

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

    pdgui_vmess("pdtk_pd_audio", "r", flag ? "on" : "off");
}

    /* take the scheduler forward one DSP tick, also handling clock timeouts */
void sched_tick(void)
{
    double next_sys_time = pd_this->pd_systime + SYSTIMEPERTICK;
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
            (void)sys_pollgui();
        }
        if (sys_quit)
            return;
    }
    pd_this->pd_systime = next_sys_time;
    dsp_tick();
    sched_counter++;
}

int sched_get_sleepgrain( void)
{
    return (sys_sleepgrain > 0 ? sys_sleepgrain :
        (sys_schedadvance/4 > 5000 ? 5000 : (sys_schedadvance/4 < 100 ? 100 :
            sys_schedadvance/4)));
}

    /* old stuff for extern binary compatibility -- remove someday */
int *get_sys_sleepgrain(void) {return(&sys_sleepgrain);}

/*
Here is Pd's "main loop."  This routine dispatches clock timeouts and DSP
"ticks" deterministically, and polls for input from MIDI and the GUI.  If
we're left idle we also poll for graphics updates; but these are considered
lower priority than the rest.

The time source is normally the audio I/O subsystem via the "sys_send_dacs()"
call.  This call returns true if samples were transferred; false means that
the audio I/O system is still busy with previous transfers.
*/

void sys_pollmidiqueue(void);
void sys_initmidiqueue(void);

 /* sys_idlehook is a hook the user can fill in to grab idle time.  Return
nonzero if you actually used the time; otherwise we're really really idle and
will now sleep. */
int (*sys_idlehook)(void);

    /* when audio is idle, see to GUI and other stuff */
static int sched_idletask( void)
{
    static int sched_nextmeterpolltime, sched_nextpingtime;
    int rtn = 0;
    sys_lock();
    if (sys_pollgui())
        rtn = 1;
    sys_unlock();

#if defined(__linux__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)\
                || defined(__GNU__)
        /* if there's no GUI but we're running in "realtime", here is
        where we arrange to ping the watchdog every 2 seconds.  (If there's
        a GUI, it initiates the ping instead to be sure there's communication
        back and forth.) */
    if (!sys_havegui() && sys_hipriority && sched_counter > sched_nextpingtime)
    {
        glob_watchdog(0);
            /* ping every 2 seconds */
        sched_nextpingtime = sched_counter + 2 * APPROXTICKSPERSEC;
    }
#endif

        /* clear the "DIO error" warning 1 sec after it flashes */
    if (sched_counter > sched_nextmeterpolltime)
    {
        if (sched_diored && (sched_counter - sched_dioredtime > 0))
        {
            pdgui_vmess("pdtk_pd_dio", "i", 0);
            sched_diored = 0;
        }
        sched_nextmeterpolltime = sched_counter + APPROXTICKSPERSEC;
    }
    return (rtn || sys_idlehook && sys_idlehook());
}

static void m_pollingscheduler(void)
{
    sys_lock();
    sys_initmidiqueue();
    while (!sys_quit)   /* outer loop runs once per tick */
    {
        sys_addhist(0);
        sched_tick();
        sys_addhist(1);

            /* fast forward, in which the scheduler advances without waiting
            for real time; for patches that alternate between interactive
            and batch-like computations. */
        if (sched_fastforward > 0)
        {
            sched_fastforward -= SYSTIMEPERTICK;
            sched_referencerealtime = sys_getrealtime();
            sched_referencelogicaltime = pd_this->pd_systime;
            continue;
        }
        sys_pollgui();
        sys_pollmidiqueue();
        sys_addhist(2);
        while (!sys_quit)   /* inner loop runs until it can transfer audio */
        {
            int timeforward; /* SENDDACS_YES if audio was transferred, SENDDACS_NO if not,
                                or SENDDACS_SLEPT if yes but time elapsed during xfer */
            sys_unlock();
            if (sched_useaudio == SCHED_AUDIO_NONE)
            {
                    /* no audio; use system clock */
                double lateness = 1000. *
                    (sys_getrealtime() - sched_referencerealtime) -
                        clock_gettimesince(sched_referencelogicaltime);
                if (lateness > 20000)   /* if 20" late, don't try to catch up */
                {
                    sched_referencerealtime = sys_getrealtime();
                    sched_referencelogicaltime = pd_this->pd_systime;
                }
                timeforward = (lateness > 0 ? SENDDACS_YES : SENDDACS_NO);
            }
            else timeforward = sys_send_dacs();
            sys_addhist(3);
                /* test for idle; if so, do graphics updates. */
            if (timeforward != SENDDACS_YES && !sched_idletask() && !sys_nosleep)
            {
                /* if even that had nothing to do, sleep. */
                sys_addhist(4);
                sys_microsleep();
            }
            sys_addhist(5);
            sys_lock();
            if (timeforward != SENDDACS_NO)
                break;
        }
    }
    sys_unlock();
}

void sched_audio_callbackfn(void)
{
    sys_lock();
    sys_addhist(0);
    sched_tick();
    sys_addhist(1);
    sys_pollmidiqueue();
    sys_addhist(2);
    sys_unlock();
    (void)sched_idletask();
    sys_addhist(3);
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
            (void)sys_pollgui();
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
    while (sys_quit != SYS_QUIT_QUIT)
        sched_tick();
    return (0);
}

void sys_exit(void)
{
    sys_quit = SYS_QUIT_QUIT;
}
