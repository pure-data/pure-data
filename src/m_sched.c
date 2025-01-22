/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  scheduling stuff  */

#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#ifdef _WIN32
#include <windows.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif
#include <errno.h>
#include <pthread.h>

    /* LATER consider making this variable.  It's now the LCM of all sample
    rates we expect to see: 32000, 44100, 48000, 88200, 96000. */
#define TIMEUNITPERMSEC (32. * 441.)
#define TIMEUNITPERSECOND (TIMEUNITPERMSEC * 1000.)
#define SYSTIMEPERTICK \
    ((STUFF->st_schedblocksize/STUFF->st_dacsr) * TIMEUNITPERSECOND)
#define APPROXTICKSPERSEC \
    ((int)(STUFF->st_dacsr /(double)STUFF->st_schedblocksize))

#define SYS_QUIT_QUIT 1
#define SYS_QUIT_REOPEN 2
#define SYS_QUIT_CLOSE 3
#define SYS_QUIT_RESTART 4
static int sys_quit;
static pthread_t mainthread_id;
static pthread_cond_t sched_cond;
static pthread_mutex_t sched_mutex;
static int sched_useaudio = SCHED_AUDIO_NONE;
static double sched_referencerealtime, sched_referencelogicaltime;

static int sys_exitcode;
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

void glob_watchdog(void *dummy);

static float sched_fastforward;

void glob_fastforward(void *dummy, t_floatarg f)
{
    if (sched_useaudio == SCHED_AUDIO_CALLBACK)
        pd_error(0, "'fast-forward' does not work with 'callbacks' (yet)");
    else
        sched_fastforward = TIMEUNITPERMSEC * f;
}

void sched_init(void)
{
    pthread_mutex_init(&sched_mutex, 0);
    pthread_cond_init(&sched_cond, 0);
    mainthread_id = pthread_self();
}

void sched_term(void)
{
    pthread_mutex_destroy(&sched_mutex);
    pthread_cond_destroy(&sched_cond);
}

int sys_ismainthread(void)
{
    return pthread_equal(pthread_self(), mainthread_id);
}

void dsp_tick(void);

    /* ask the scheduler to quit; this is thread-safe, so it
    can be safely called from within the audio callback. */
void sys_exit(int status)
{
    pthread_mutex_lock(&sched_mutex);
    if (SYS_QUIT_QUIT != sys_quit) {
        sys_exitcode = status;
    } else {
        pd_error(0, "quit already called with exit code %d", sys_exitcode);
    }
    sys_quit = SYS_QUIT_QUIT;
    pthread_cond_signal(&sched_cond);
    pthread_mutex_unlock(&sched_mutex);
}

void sys_do_reopen_audio(void);
void sys_do_close_audio(void);

/* Implementation notes about (re)opening and closing the audio device:

If we are in the main thread, we can safely close and (re)open the audio
device *synchronously*. This makes sure that objects immediately see the
actual samplerate. For example, the sample rate might have been changed
by the user in the audio settings, or the actual rate might differ from
the initial (default) rate.

If we are in the audio thread, however, we can't do this because it would
deadlock; instead we send a request to main thread. However, we cannot
do this immediately because we don't want the main thread to interrupt
us halfway and accidentally send us back to polling scheduler, just
because we temporarily turned off DSP. Instead we capture all requests
to open and close the device in `sched_request`. Only at the end of the
audio callback we send the most recent request to the main thread, which
in turn will either close or (re)open the device. This allows users to
toggle the DSP state multiple times within a single scheduler tick.
See m_callbackscheduler() and sched_audio_callbackfn().

When we start audio with callbacks we need to break from the polling
scheduler. This is done by setting `sys_quit` to SYS_QUIT_RESTART.
NOTE: in this case it is NOT possible to further (re)open or close the
audio device in that same scheduler tick because it would deadlock.
For now, we just post an error message, but LATER we might want to fix this.
In practice, there is hardly a reason why a user would want to start audio
and then immediately stop or (re)open it. Note, however, that it's always ok
to toggle DSP states while audio is already running (which has its use cases).

If Pd is built with -DUSEAPI_DUMMY=1, sys_reopen_audio() and sys_close_audio()
are simply no-ops since there are no audio devices that can be opened or closed.
This also avoids potential issues with libpd. */

static int sched_request;

    /* always called with Pd locked */
void sys_reopen_audio(void)
{
#ifndef USEAPI_DUMMY
    if (sys_ismainthread())
    {
        if (sched_useaudio != SCHED_AUDIO_CALLBACK)
        {
            sys_do_reopen_audio(); /* (re)open synchronously */
                /* if we have started the callback scheduler,
                break from the polling scheduler! */
            if (sched_useaudio == SCHED_AUDIO_CALLBACK)
            {
                pthread_mutex_lock(&sched_mutex);
                if (sys_quit != SYS_QUIT_QUIT)
                    sys_quit = SYS_QUIT_RESTART;
                pthread_mutex_unlock(&sched_mutex);
            }
        }
        else /* See comments above sys_reopen_audio() */
            pd_error(0, "Cannot reopen audio: would deadlock");
    }
    else /* called from the audio callback, see sched_audio_callbackfn() */
        sched_request = SYS_QUIT_REOPEN;
#endif
}

    /* always called with Pd locked */
void sys_close_audio(void)
{
#ifndef USEAPI_DUMMY
    if (sys_ismainthread())
    {
        if (sched_useaudio != SCHED_AUDIO_CALLBACK)
            sys_do_close_audio(); /* close synchronously */
        else  /* See comments above sys_reopen_audio() */
            pd_error(0, "Cannot close audio: would deadlock");
    }
    else /* called from the audio callback, see sched_audio_callbackfn() */
        sched_request = SYS_QUIT_CLOSE;
#endif
}

    /* called by sys_do_reopen_audio() and sys_do_close_audio() */
void sched_set_using_audio(int flag)
{
    sched_useaudio = flag;
    if (flag == SCHED_AUDIO_NONE)
    {
        sched_referencerealtime = sys_getrealtime();
        sched_referencelogicaltime = clock_getlogicaltime();
    }

    pdgui_vmess("pdtk_pd_audio", "r", flag ? "on" : "off");
}

int sched_get_using_audio(void)
{
    return sched_useaudio;
}

void messqueue_dispatch();

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
            /* ignore SYS_QUIT_REOPEN and SYS_QUIT_CLOSE! */
        if (sys_quit == SYS_QUIT_QUIT)
            return;
    }
    pd_this->pd_systime = next_sys_time;
    messqueue_dispatch();
    dsp_tick();
    sched_counter++;
}

int sched_get_sleepgrain(void)
{
    if (sys_sleepgrain > 0)
        return sys_sleepgrain;
    else if (sched_useaudio == SCHED_AUDIO_POLL)
    {
        int sleepgrain = sys_schedadvance / 4;
        if (sleepgrain > 5000)
            sleepgrain = 5000;
        else if (sleepgrain < 100)
            sleepgrain = 100;
        return sleepgrain;
    }
    else return 1000; /* default */
}

    /* old stuff for extern binary compatibility -- remove someday */
int *get_sys_sleepgrain(void)
{
    return(&sys_sleepgrain);
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

void sys_pollmidiqueue(void);
void sys_initmidiqueue(void);

 /* sys_idlehook is a hook the user can fill in to grab idle time.  Return
nonzero if you actually used the time; otherwise we're really really idle and
will now sleep. */
int (*sys_idlehook)(void);

    /* when audio is idle, see to GUI and other stuff */
int sched_idletask(void)
{
    static int sched_nextmeterpolltime, sched_nextpingtime;
    int rtn = 0;
    sys_lock();
    if (sys_pollgui())
        rtn = 1;
    sys_unlock();

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
        /* NB: we don't need to lock the scheduler mutex because sys_quit
        will only be modified from this thread */
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
            /* do at least one GUI update per DSP tick, so that Pd stays responsive
             * if the scheduler can't keep up with the audio callback */
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
            else
                timeforward = sys_send_dacs();
            sys_addhist(3);
                /* test for idle; if so, do graphics updates. */
            if (timeforward != SENDDACS_YES && !sched_idletask())
            {
                /* if even that had nothing to do, sleep. */
                sys_addhist(4);
                if (!sys_nosleep && timeforward != SENDDACS_SLEPT)
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

static volatile int callback_inprogress;

void sched_audio_callbackfn(void)
{
        /* do not process once we have asked
        to leave the callback scheduler! */
    if (sys_quit) return;

    callback_inprogress = 1;
    sched_request = 0;
    sys_lock();
    sys_addhist(0);
    sched_tick();
    sys_addhist(1);
    sys_pollmidiqueue();
    sys_addhist(2);
    sys_unlock();
    (void)sched_idletask();
    sys_addhist(3);
    if (sched_request)
    {
            /* notify main thread! */
        pthread_mutex_lock(&sched_mutex);
        if (sys_quit != SYS_QUIT_QUIT)
            sys_quit = sched_request;
        pthread_cond_signal(&sched_cond);
        pthread_mutex_unlock(&sched_mutex);
    }
    callback_inprogress = 0;
}

    /* callback scheduler timeout in seconds */
#define CALLBACK_TIMEOUT 2.0

int sys_try_reopen_audio(void);

static void m_callbackscheduler(void)
{
        /* wait in a loop until the audio callback asks us to quit. */
    pthread_mutex_lock(&sched_mutex);
    while (!sys_quit)
    {
        int wasinprogress;
            /* get current system time and add timeout */
        double timewas, timeout = CALLBACK_TIMEOUT;
        struct timespec ts;
    #ifdef _WIN32
        struct __timeb64 tb;
        _ftime64(&tb);
            /* add fractional part to timeout */
        timeout += tb.millitm * 0.001;
        ts.tv_sec = tb.time + (time_t)timeout;
        ts.tv_nsec = (timeout - (time_t)timeout) * 1000000000;
    #else
        struct timeval now;
        gettimeofday(&now, 0);
            /* add fractional part to timeout */
        timeout += now.tv_usec * 0.000001;
        ts.tv_sec = now.tv_sec + (time_t)timeout;
        ts.tv_nsec = (timeout - (time_t)timeout) * 1000000000;
    #endif
            /* sleep on condition variable (with timeout) */
        timewas = pd_this->pd_systime;
        wasinprogress = callback_inprogress;
        if (pthread_cond_timedwait(&sched_cond, &sched_mutex, &ts) == ETIMEDOUT)
        {
                /* check if the schedular has advanced since the last time
                we checked (while it was not in progress) */
            if (!sys_quit && !wasinprogress && (pd_this->pd_systime == timewas))
            {
                pthread_mutex_unlock(&sched_mutex);
                    /* if the scheduler has not advanced, but the callback is
                    still in progress, it just blocks on some Pd message.
                    Otherwise, the audio device got stuck or disconnected. */
                if (!callback_inprogress && !sys_try_reopen_audio())
                    return;
                pthread_mutex_lock(&sched_mutex);
            }
        }
    }
    pthread_mutex_unlock(&sched_mutex);
}

int m_mainloop(void)
{
        /* open audio and MIDI */
    sys_reopen_midi();
    if (audio_shouldkeepopen() && !audio_isopen())
    {
        sys_lock();
        sys_do_reopen_audio();
        sys_unlock();
    }

        /* run the scheduler until it quits. */
    while (sys_quit != SYS_QUIT_QUIT)
    {
        sys_lock();
            /* check if we should close/reopen the audio device. */
        if (sys_quit == SYS_QUIT_REOPEN)
        {
            sys_do_close_audio();
            sys_do_reopen_audio();
        }
        else if (sys_quit == SYS_QUIT_CLOSE)
            sys_do_close_audio();
        sys_quit = 0;
        sys_initmidiqueue();
        sys_unlock();
        if (sched_useaudio == SCHED_AUDIO_CALLBACK)
            m_callbackscheduler();
        else
            m_pollingscheduler();
    }

    sys_do_close_audio();
    sys_close_midi();

    return (sys_exitcode);
}

int m_batchmain(void)
{
    while (sys_quit != SYS_QUIT_QUIT)
        sched_tick();
    return (sys_exitcode);
}
