/* Copyright (c) 2021 Christof Ressi.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* The task API is a safe and user-friendly way to run tasks in the background
 * without blocking the audio thread. This is intended for CPU intensive or
 * otherwise non-real-time safe operations, such as reading data from a file.
 *
 * See taskobj.c in doc/6.externs for a complete code example.
 *
 * ================================================================================
 * Documentation:
 * ================================================================================
 * For Pd external authors:
 * --------------------------------------------------------------------------------
 *
 * t_task *task_start(t_pd *owner, void *data, task_workfn workfn, task_callback cb)
 *
 * description: schedule a new task.
 *
 * arguments:
 *
 * 1) (t_pd *) owner: the Pd object that owns the given task.
 *    NB: this argument can be NULL in case there is no valid owner;
 *    for example, this can be used for scheduling a task from within the
 *    callback of a cancelled task (where 'owner' will be NULL) to avoid
 *    cleaning up resources on the audio thread.
 *
 * 2) (void *) data: a user-specific dynamically allocated struct.
 *    This struct contains the input data for the worker function, e.g. a filename,
 *    and also the output data, e.g. a memory buffer. Try to avoid any references
 *    to shared state. Instead, treat the data object as a message that is exchanged
 *    between your Pd object and the task system.
 *    NOTE: In the rare case where you do need to access shared state, make sure
 *    to call task_stop() with sync=1! For an example, see the "progress" method
 *    in "doc/6.externs/taskobj.c".
 *
 * 3) (task_workfn) workfn: a function to be called in the background.
 *    You would typically read from 'data', perform some operations and store
 *    the result back to 'data'.
 *    ***IMPORTANT***: do not call any Pd API functions, except for task_check(),
 *    task_suspend() and task_resume(). In particular, do not call sys_lock()
 *    or sys_unlock(), as this can cause deadlocks!
 *
 * 4) (task_callback) cb: (optional) a function to be called after the task has completed.
 *    This is where the result can be safely moved from 'data' to the owning object.
 *    If the task has been cancelled, 'owner' is NULL; in this case, do not forget
 *    to clean up any resources in 'data'.
 *    Finally, here you can also safely free the 'data' object itself!
 *    You may pass NULL if you do not need a callback.
 *
 * returns: a handle to the new task.
 *    Store the handle in your object, so that you are able to cancel the task
 *    later if needed, see task_stop().
 *    ***IMPORTANT***: do not try to use the task handle after the task has finished;
 *    typically you would unset the task handle in the callback function.
 *
 * NOTE: the order in which tasks are executed is undefined! Implementations might
 * use multiple worker threads which execute tasks concurrently.
 *
 * NOTE: If there are no worker threads, 'workfn' is executed synchronously and
 * 'cb' will be called at the end of the scheduler tick.
 *
 * --------------------------------------------------------------------------------
 *
 * t_task *task_spawn(t_pd *owner, void *data, task_workfn workfn, task_callback cb)
 *
 * description: spawn a new task
 *    This function behaves just like task_start(), except that it runs the worker
 *    function in its own thread, so that other tasks can run concurrently.
 *    Use this if a task might take a long time - let's say more than 1 second -
 *    and you cannot use task_yield().
 *
 * NOTE: if there are no worker threads, task_spawn() behaves the same as task_start().
 *
 * ---------------------------------------------------------------------------------
 *
 * int task_stop(t_task *task, int sync)
 *
 * description: stop a given task.
 *     You always have to call this in the object destructor for any pending tasks(s);
 *     this makes sure that the task callback function won't try to access an object
 *     that has already been deleted!
 *     In addition, you can use this to support cancellation by the user. The task
 *     might be currently executing or it might have completed already. Either way,
 *     the task handle will be invalidated and the callback will eventually be
 *     called with 'owner' set to NULL.
 *
 * arguments:
 *
 * 1) (t_task *) task: the task that shall be cancelled.
 *
 * 2) (int) sync: synchronize with worker thread(s).
 *     true (1): check if the task is currently being executed and if yes, wait for its
 *               completion. Typically, this is only necessary if the task data contains
 *               references to shared state - which you should avoid in the first place!
 *     false (0): return immediately without blocking.
 *
 * returns: 1 if the task could be stopped, 0 on failure.
 *
 * ----------------------------------------------------------------------------------
 *
 * int task_check(t_task *task)
 *
 * description: check the task state
 *     In the worker function you can regularly call task_check() and return early if
 *     the task has been cancelled. This makes the overall task system more responsive.
 *
 * returns: 1 if running, 0 if cancelled
 *
 * ----------------------------------------------------------------------------------
 *
 * int task_yield(t_task *task, t_task_workfn workfn)
 *
 * description: yield back to the task system
 *     In the worker function you can regularly call task_yield() to yield control back
 *     to the task system and allow other tasks to run. The task will continue with the
 *     given work function (unless it has been cancelled). This makes the overall task
 *     system more responsive. Use this if you have a long running task that can be
 *     broken up into several chunks of work.
 *
 * returns: 1 if running, 0 if cancelled
 *
 * ----------------------------------------------------------------------------------
 *
 * void task_notify(t_task *task, void *data, t_task_callback fn)
 *
 * description: notify the Pd scheduler
 *    This sends a message from the current thread back to the Pd scheduler.
 *    It may be called from a worker function or any other thread.
 *    This can be used, for example, to communicate the progress of an operation
 *    or for piece-wise data transfer (e.g. streaming a file from the internet)
 *
 * arguments:
 *
 * 1) (t_task *) task: the task handle
 *
 * 2) (void *) data: dynamically allocated data that will be passed to the function.
 *
 * 3) (t_task_callback) fn: the function that shall be called with 'data' by the Pd
 *    scheduler. At the end of the function you have to free the 'data' object.
 *    If the associated task has been cancelled, 'owner' will be NULL.
 *
 * ----------------------------------------------------------------------------------
 *
 * void task_suspend(t_task *task)
 *
 * description: suspend current execution
 *     Call this function after deferring to another thread, typically as the last
 *     statement before returning from the worker function. The task will be suspended
 *     until the other thread finally calls task_resume(), allowing other tasks to run
 *     concurrently. This is useful for interfacing with event loops or other asynchronous
 *     library calls.
 *
 * ----------------------------------------------------------------------------------
 *
 * void task_resume(t_task *task, t_task_workfn workfn)
 *
 * description: resume execution
 *     This resumes a task that has been suspended with task_suspend().
 *     It is always called from another thread.
 *
 * arguments:
 *
 * 1) (t_task *) task: the task handle
 *
 * 2) (t_task_workfn) workfn: (optional) worker function
 *     The task will continue with the provided worker function;
 *     if 'workfn' is NULL, the task is considered completed.
 *     Generally, you should spend as little time as necessary in the external thread
 *     and continue processing the result in a new worker function.
 *
 * ==================================================================================
 * For libpd clients:
 * ----------------------------------------------------------------------------------
 *
 * void sys_tasksystem_start(int threads)
 *
 * description: start the task system
 *
 * arguments:
 *
 * 1) (int) threads: the number of (internal) worker threads, or one of the following
 *     constants:
 *     TASKQUEUE_DEFAULT: use default number of threads;
 *     TASKQUEUE_EXTERNAL: use external worker threads
 *
 * NOTE: if sys_tasksystem_start() is not called, the tasks are executed synchronously
 * on the audio/scheduler thread. This is useful for batch/offline processing, for example.
 *
 * ----------------------------------------------------------------------------------
 *
 * void sys_tasksystem_stop(void)
 *
 * description: stop the task system
 *     After this call, a libpd client can safely join any external worker threads.
 *
 * ----------------------------------------------------------------------------------
 *
 * int sys_tasksystem_perform(t_pdinstance *pd, int nonblocking)
 *
 * description: execute pending tasks.
 *     This function is called from the external worker thread(s).
 *     It can be safely called from multiple threads at the same time!
 *
 * arguments:
 *
 * 1) (t_pdinstance *) pd: the Pd instance
 *
 * 2) (int) nonblocking: call in non-blocking mode (true/false)
 *     true (1): execute a *single* task, if available, otherwise return immediately.
 *     false (0): block until the task system is stopped with sys_tasksystem_stop().
 *
 * returns: 1 if it did something or 0 if there was nothing to do.
 *
 * ----------------------------------------------------------------------------------
 *
 * int sys_tasksystem_running(void)
 *
 * description: check if the task system is running.
 *
 * returns: true (1) or false (0)
 *
 * ==================================================================================
 */

#include "m_pd.h"
/* for atomic_int */
#include "m_private_utils.h"
#include "s_stuff.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif /* _WIN32 */

/* If PD_WORKERTHREADS is 1, tasks are pushed to a queue. One ore more worker threads
 * pop tasks from the queue and execute the 'task_workfn' function. On completion,
 * tasks are pushed to another queue that is regularly polled by the scheduler.
 * This is were the 'task_callback' function is called.
 *
 * If PD_WORKERTHREADS is 0, or tasksystem_start() is not called, tasks are executed
 * on the audio/scheduler thread. This is intended to support single-threaded systems
 * and use cases where synchronous execution is desired (e.g. batch processing).
 * Note that although the work function is executed synchronously, the callback is
 * still deferred to the end to the current scheduler tick. This is done to maintain
 * the same semantics as the threaded implementation, i.e. the callback is guaranteed
 * to run AFTER task_start() has returned.
 */

#ifndef PD_WORKERTHREADS
#define PD_WORKERTHREADS 1
#endif

#if PD_WORKERTHREADS
#include <pthread.h>
#endif

    /* see task_waitforresume() */
#define TASK_RESUME_POLL_INTERVAL 1

struct _task
{
    struct _task *t_next;
    struct _task *t_nextpending;
#ifdef PDINSTANCE
        /* for task_resume() and task_notify() */
    t_pdinstance *t_instance;
#endif
    t_pd *t_owner;
    void *t_data;
    t_task_workfn t_workfn;
    t_task_callback t_cb;
    atomic_int t_cancel;
    atomic_int t_suspend;
};

typedef struct _taskqueue
{
    t_task *tq_head;
    t_task *tq_tail;
#if PD_WORKERTHREADS
    pthread_mutex_t tq_mutex;
    pthread_cond_t tq_condition;
#endif
} t_taskqueue;

static void taskqueue_init(t_taskqueue *x)
{
    x->tq_head = x->tq_tail = 0;
#if PD_WORKERTHREADS
    pthread_mutex_init(&x->tq_mutex, 0);
    pthread_cond_init(&x->tq_condition, 0);
#endif
}

static void taskqueue_free(t_taskqueue *x)
{
    t_task *task = x->tq_head;
    while (task)
    {
        t_task *next = task->t_next;
            /* the callback will free the data */
        task->t_cb(0, task->t_data);
        freebytes(task, sizeof(t_task));
        task = next;
    }
    x->tq_head = x->tq_tail = 0;
#if PD_WORKERTHREADS
    pthread_mutex_destroy(&x->tq_mutex);
    pthread_cond_destroy(&x->tq_condition);
#endif
}

static void taskqueue_push(t_taskqueue *x, t_task *task)
{
    if (x->tq_tail)
        x->tq_tail = (x->tq_tail->t_next = task);
    else
        x->tq_head = x->tq_tail = task;
}

static void taskqueue_push_sync(t_taskqueue *x, t_task *task, int notify)
{
#if PD_WORKERTHREADS
    pthread_mutex_lock(&x->tq_mutex);
#endif
    taskqueue_push(x, task);
#if PD_WORKERTHREADS
    pthread_mutex_unlock(&x->tq_mutex);
    if (notify)
        pthread_cond_signal(&x->tq_condition);
#endif
}

static t_task *taskqueue_pop(t_taskqueue *x)
{
    if (x->tq_head)
    {
        t_task *task = x->tq_head;
        x->tq_head = task->t_next;
        if (!x->tq_head) /* queue is now empty */
            x->tq_tail = 0;
        task->t_next = 0;
        return task;
    }
    else
        return 0;
}

static t_task *taskqueue_popall(t_taskqueue *x)
{
    t_task *task = x->tq_head;
    x->tq_head = x->tq_tail = 0;
    return task;
}

static int taskqueue_empty(t_taskqueue *x)
{
    return x->tq_head == 0;
}

#if PD_WORKERTHREADS

static void taskqueue_lock(t_taskqueue *x)
{
    pthread_mutex_lock(&x->tq_mutex);
}

static void taskqueue_unlock(t_taskqueue *x)
{
    pthread_mutex_unlock(&x->tq_mutex);
}

static void taskqueue_notify(t_taskqueue *x)
{
    pthread_cond_signal(&x->tq_condition);
}

static void taskqueue_notify_all(t_taskqueue *x)
{
    pthread_cond_broadcast(&x->tq_condition);
}

static void taskqueue_wait(t_taskqueue *x)
{
    pthread_cond_wait(&x->tq_condition, &x->tq_mutex);
}

#endif /* PD_WORKERTHREADS */

typedef struct _message
{
    struct _message *m_next;
    t_task *m_task;
    void *m_data;
    t_task_callback m_fn;
} t_message;

typedef struct _messqueue
{
    t_message *mq_head;
    t_message *mq_tail;
} t_messqueue;

static void messqueue_init(t_messqueue *x)
{
    x->mq_head = x->mq_tail = 0;
}

static void messqueue_push(t_messqueue *x, t_message *m)
{
    if (x->mq_tail)
        x->mq_tail = (x->mq_tail->m_next = m);
    else
        x->mq_head = x->mq_tail = m;
}

static t_message *messqueue_popall(t_messqueue *x)
{
    t_message *m = x->mq_head;
    x->mq_head = x->mq_tail = 0;
    return m;
}

static void messqueue_free(t_messqueue *x)
{
    t_message *m = x->mq_head;
    while (m)
    {
        t_message *next = m->m_next;
            /* the callback will free the data */
        m->m_fn(0, m->m_data);
        freebytes(m, sizeof(*m));
        m = next;
    }
    x->mq_head = x->mq_tail = 0;
}

struct _tasksystem
{
    t_task *ts_pending; /* list of pending tasks */
    t_taskqueue ts_fromsched; /* scheduled tasks */
    t_taskqueue ts_tosched; /* finished tasks */
    t_messqueue ts_messages; /* messages sent to the scheduler;
                             protected by the same mutex as ts_tosched */
#if PD_WORKERTHREADS
    pthread_t *ts_threads; /* internal worker threads */
    int ts_numthreads; /* number of internal worker threads */
    int ts_running; /* thread(s) are running? */
    int ts_external; /* use external worker threads */
#endif
};

static int task_iscancelled(t_task *task)
{
    return atomic_int_load(&task->t_cancel) != 0;
}

static int task_issuspended(t_task *task)
{
    return atomic_int_load(&task->t_suspend) != 0;
}

static void task_waitforresume(t_task *task)
{
    /* poll until task_resume() has been called from the other thread */
    while (atomic_int_load(&task->t_suspend) > 1)
    {
#ifdef _WIN32
        Sleep(TASK_RESUME_POLL_INTERVAL);
#else
        usleep(TASK_RESUME_POLL_INTERVAL * 1000);
#endif
    }
}

static void task_doresume(t_tasksystem *sys, t_task *task)
{
        /* The one who decrements the suspend count to zero gets to
         * reschedule the task. */
    if (atomic_int_fetch_sub(&task->t_suspend, 1) - 1 == 0)
    {
        if (task->t_workfn && !task_iscancelled(task))
                /* reschedule task to continue with new work function */
            taskqueue_push_sync(&sys->ts_fromsched, task, 1);
        else
                /* move task back to scheduler and notify the main thread,
                 * which might be waiting in task_stop(). */
            taskqueue_push_sync(&sys->ts_tosched, task, 1);
    }
}

void tasksystem_poll(void);

#if PD_WORKERTHREADS

static void lower_thread_priority(void)
{
#ifdef _WIN32
    if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST))
        fprintf(stderr, "warning: could not set low priority for worker thread\n");
#else
    struct sched_param param;
    param.sched_priority = 0;
    if (pthread_setschedparam(pthread_self(), SCHED_OTHER, &param) != 0)
        fprintf(stderr, "warning: could not set low priority for worker thread\n");
#endif
}

static void *task_threadfn(void *x)
{
    lower_thread_priority();

    sys_tasksystem_perform((t_pdinstance *)x, 0);

    return 0;
}

int sys_tasksystem_perform(t_pdinstance *pd, int nonblocking)
{
    t_tasksystem *sys = pd->pd_stuff->st_tasksystem;
    t_taskqueue *fromsched = &sys->ts_fromsched;
    t_taskqueue *tosched = &sys->ts_tosched;
    int didsomething = 0;

#ifdef PDINSTANCE
    pd_setinstance(pd);
#endif

    taskqueue_lock(fromsched);
    while (sys->ts_running)
    {
            /* try to pop item from queue */
        t_task *task = taskqueue_pop(fromsched);
        if (task)
        {
            taskqueue_unlock(fromsched);
                /* execute work function (without lock!) */
            if (!task_iscancelled(task))
            {
                task->t_workfn(task, task->t_data);
                didsomething = 1;
                if (task_issuspended(task))
                {
                        /* task has been suspended/yielded in the work function */
                    task_doresume(sys, task);
                    taskqueue_lock(fromsched);
                    continue;
                }
            }
                /* move task back to scheduler and notify the main thread,
                 * which might be waiting in task_stop(). */
            taskqueue_push_sync(tosched, task, 1);
            taskqueue_lock(fromsched);
        }
        else
        {
            if (nonblocking)
                break;
            else /* wait for new task */
                taskqueue_wait(fromsched);
        }
    }
    taskqueue_unlock(fromsched);

    return didsomething;
}

void sys_tasksystem_start(int threads)
{
    int i, err;
    t_tasksystem *sys = STUFF->st_tasksystem;
    if (sys->ts_running)
    {
        pd_error(0, "cannot start task system: already running");
        return;
    }
    if (threads == TASKSYSTEM_EXTERNAL) /* external worker threads */
    {
        sys->ts_external = 1;
        sys->ts_running = 1;
    }
    else /* internal worker threads */
    {
        if (threads == TASKSYSTEM_DEFAULT)
            threads = DEFNUMWORKERTHREADS;
        if (threads <= 0)
        {
            bug("sys_tasksystem_start");
            return;
        }
        sys->ts_external = 0;
        sys->ts_running = 1;
        sys->ts_numthreads = threads;
        sys->ts_threads = getbytes(threads * sizeof(pthread_t));
        for (i = 0; i < threads; i++)
        {
            if ((err = pthread_create(&sys->ts_threads[i], 0, task_threadfn, pd_this)))
                pd_error(0, "could not create worker thread (%d)", err);
        }
    }
}

void sys_tasksystem_stop(void)
{
    t_tasksystem *sys = STUFF->st_tasksystem;
    t_taskqueue *fromsched = &sys->ts_fromsched;
    t_taskqueue *tosched = &sys->ts_tosched;
        /* wait until all pending tasks have finished. (This will also flush all queues.) */
    if (sys->ts_pending)
    {
        fprintf(stderr, "waiting for pending tasks...\n");
        if (sys->ts_running)
        {
                /* wait for the worker thread(s) and poll until all tasks have completed. */
            do
            {
                taskqueue_lock(tosched);
                while (taskqueue_empty(tosched))
                    taskqueue_wait(tosched);
                taskqueue_unlock(tosched);
                tasksystem_poll();
            } while (sys->ts_pending);
        }
        else
        {
                /* wait for all suspended tasks */
            t_task *t;
            for (t = sys->ts_pending; t; t = t->t_nextpending)
            {
                if (task_issuspended(t))
                    task_waitforresume(t);
            }
                /* poll one last time to resume tasks and flush all queues */
            tasksystem_poll();
        }
    }
        /* quit and notify worker thread(s) */
    if (sys->ts_running)
    {
        taskqueue_lock(fromsched);
        sys->ts_running = 0;
        taskqueue_unlock(fromsched);
        taskqueue_notify_all(fromsched);
        if (!sys->ts_external)
        {
                /* join internal worker thread(s) */
            int i;
            for (i = 0; i < sys->ts_numthreads; i++)
                pthread_join(sys->ts_threads[i], 0);
            freebytes(sys->ts_threads, sys->ts_numthreads * sizeof(pthread_t));
            sys->ts_threads = 0;
            sys->ts_numthreads = 0;
        }
        sys->ts_external = 0;
    }
}

int sys_tasksystem_running(void)
{
    return STUFF->st_tasksystem->ts_running;
}

#else

int sys_tasksystem_perform(t_pdinstance *pd, int nonblocking)
{
    fprintf(stderr, "warning: sys_tasksystem_perform() called without PD_WORKERTHREADS!\n");
    return 0;
}

void sys_tasksystem_start(int threads)
{
    if (threads == TASKSYSTEM_EXTERNAL)
        fprintf(stderr, "warning: trying to use external worker threads without PD_WORKERTHREADS!\n");
}

void sys_tasksystem_stop(void)
{
    t_tasksystem *sys = STUFF->st_tasksystem;
    if (sys->ts_pending)
    {
        t_task *t;
        fprintf(stderr, "waiting for pending tasks...\n");
            /* wait for all suspended tasks */
        for (t = sys->ts_pending; t; t = t->t_nextpending)
        {
            if (task_issuspended(t))
                task_waitforresume(t);
        }
            /* poll one final time to resume tasks and flush all queues */
        tasksystem_poll();
    }
}

int sys_tasksystem_running(void)
{
    return 0;
}

#endif /* PD_WORKERTHREADS */

void s_task_newpdinstance(void)
{
    t_tasksystem *sys = STUFF->st_tasksystem = getbytes(sizeof(t_tasksystem));

    sys->ts_pending = 0;
    taskqueue_init(&sys->ts_tosched);
#if PD_WORKERTHREADS
    taskqueue_init(&sys->ts_fromsched);
    sys->ts_threads = 0;
    sys->ts_numthreads = 0;
    sys->ts_external = 0;
    sys->ts_running = 0;
#endif
    messqueue_init(&sys->ts_messages);
}

void s_task_freepdinstance(void)
{
    t_tasksystem *sys = STUFF->st_tasksystem;
    sys_tasksystem_stop();
    taskqueue_free(&sys->ts_tosched);
#if PD_WORKERTHREADS
    taskqueue_free(&sys->ts_fromsched);
#endif
    messqueue_free(&sys->ts_messages);

    freebytes(sys, sizeof(t_tasksystem));
}

    /* called in sched_tick() */
void tasksystem_poll(void)
{
    t_tasksystem *sys = STUFF->st_tasksystem;
    t_taskqueue *tosched;
    t_task *task;
    t_message *msg;
    if (!sys->ts_pending)
        return;

#if PD_WORKERTHREADS
    if (!sys->ts_running)
#endif
    {
        t_taskqueue *fromsched = &sys->ts_fromsched;
            /* if we have no worker threads, we must poll suspended tasks */
        for (task = sys->ts_pending; task; task = task->t_nextpending)
        {
                /* only resume task if task_resume() has been called!
                 * This makes sure we only push tasks from the main thread. */
            if (atomic_int_load(&task->t_suspend) == 1)
                task_doresume(sys, task);
        }
            /* execute scheduled tasks. NOTE: tasks may be rescheduled, see task_yield(). */
        while (!taskqueue_empty(fromsched))
        {
            for (task = taskqueue_popall(fromsched); task; task = task->t_next)
            {
                if (!task_iscancelled(task))
                {
                        /* execute work function synchronously */
                    task->t_workfn(task, task->t_data);
                    if (task_issuspended(task))
                    {
                            /* task has been suspended/yielded in the work function */
                        if (atomic_int_load(&task->t_suspend) == 1)
                            task_doresume(sys, task);
                        continue;
                    }
                }
                    /* schedule callback (see below). */
                taskqueue_push(&sys->ts_tosched, task);
            }
        }
    }

        /* atomically pop all messages and finished tasks */
    tosched = &sys->ts_tosched;
#if PD_WORKERTHREADS
    taskqueue_lock(tosched);
#endif
    msg = messqueue_popall(&sys->ts_messages);
    task = taskqueue_popall(tosched);
#if PD_WORKERTHREADS
    taskqueue_unlock(tosched);
#endif
        /* first dispatch all messages because they are associated with tasks
         * and always scheduled before the respective task has finished. */
    while (msg)
    {
        t_message *next = msg->m_next;
        int cancelled = task_iscancelled(msg->m_task);
            /* If the associated task has been cancelled, set 'owner' to NULL! */
        msg->m_fn(cancelled ? 0 : msg->m_task->t_owner, msg->m_data);
        freebytes(msg, sizeof(*msg));
        msg = next;
    }
        /* now dispatch all tasks. */
    while (task)
    {
        t_task *t, *next = task->t_next;
        if (atomic_int_load(&task->t_suspend) != 0)
            bug("tasksystem_poll: non-zero task suspend count");
            /* run callback. If the task has been cancelled, set 'owner' to NULL! */
        if (task->t_cb)
            task->t_cb(task_iscancelled(task) ? 0 : task->t_owner, task->t_data);
            /* remove from pending list */
        if (task == sys->ts_pending)
            sys->ts_pending = task->t_nextpending;
        else
        {
            int found = 0;
            for (t = sys->ts_pending; t; t = t->t_nextpending)
            {
                if (t->t_nextpending == task)
                {
                    t->t_nextpending = task->t_nextpending;
                    found = 1;
                    break;
                }
            }
            if (!found)
                bug("tasksystem_poll: task not in pending list");
        }
        freebytes(task, sizeof(*task));
        task = next;
    }
}

t_task *task_start(t_pd *owner, void *data,
    t_task_workfn workfn, t_task_callback cb)
{
    t_tasksystem *sys = STUFF->st_tasksystem;
    t_task *task = getbytes(sizeof(t_task));
    task->t_next = 0;
#ifdef PDINSTANCE
    task->t_instance = pd_this;
#endif
    task->t_owner = owner;
    task->t_data = data;
    task->t_workfn = workfn;
    task->t_cb = cb;
    task->t_cancel = 0;
    task->t_suspend = 0;
        /* put on pending list */
    task->t_nextpending = sys->ts_pending;
    sys->ts_pending = task;
        /* push task to 'fromsched' queue and notify one worker thread */
    taskqueue_push_sync(&sys->ts_fromsched, task, 1);
    return task;
}

#if PD_WORKERTHREADS

typedef struct _spawn_data
{
    t_task *x_task;
    t_task_workfn x_workfn;
    t_task_callback x_cb;
    void *x_data;
} t_spawn_data;

static void *task_spawn_threadfn(void *x)
{
    t_spawn_data *y = (t_spawn_data *)x;
#ifdef PDINSTANCE
    pd_setinstance(y->x_task->t_instance);
#endif
        /* in case pthread_attr_setinheritsched() did not work
         * (seems to be the case on Windows...) */
    lower_thread_priority();
    y->x_workfn(y->x_task, y->x_data);
    task_resume(y->x_task, 0);
    return 0;
}

static void task_spawn_workfn(t_task *task, t_spawn_data *data)
{
    pthread_t tid;
    pthread_attr_t attr;
    int err;
    data->x_task = task;
        /* inherit thread priority and detach; this is done so that the thread already
        starts with a low priority, instead of changing the priorty after the fact. */
    pthread_attr_init(&attr);
    if ((err = pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED)))
        fprintf(stderr, "warning: pthread_attr_setinheritedsched() failed (%d)\n", err);
    if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)))
        fprintf(stderr, "warning: pthread_attr_setdetachstate() failed (%d)\n", err);
        /* create thread and suspend task */
    if ((err = pthread_create(&tid, &attr, task_spawn_threadfn, data)) == 0)
        task_suspend(task);
    else
        fprintf(stderr, "error: pthread_create() failed (%d)\n", err);
    pthread_attr_destroy(&attr);
}

static void task_spawn_callback(t_pd *x, t_spawn_data *y)
{
    y->x_cb(x, y->x_data);
    freebytes(y, sizeof(t_spawn_data));
}

#endif /* PD_WORKERTHREADS */

t_task *task_spawn(t_pd *owner, void *data,
    t_task_workfn workfn, t_task_callback cb)
{
#if PD_WORKERTHREADS
    if (STUFF->st_tasksystem->ts_running)
    {
        t_spawn_data *d = (t_spawn_data *)getbytes(sizeof(t_spawn_data));
        d->x_workfn = workfn;
        d->x_cb = cb;
        d->x_data = data;
        return task_start(owner, d, (t_task_workfn)task_spawn_workfn,
            (t_task_callback)task_spawn_callback);
    }
#endif
        /* just run synchronously */
    return task_start(owner, data, workfn, cb);
}

int task_stop(t_task *task, int sync)
{
    t_tasksystem *sys = STUFF->st_tasksystem;
    t_task *t;
#if 1
        /* check if the task is really pending */
    int foundit = 0;
    for (t = sys->ts_pending; t; t = t->t_nextpending)
    {
        if (t == task)
        {
            foundit = 1;
            break;
        }
    }
    if (!foundit)
    {
        pd_error(task->t_owner, "task_stop: task not found!");
        return 0;
    }
#endif
        /* now we can safely cancel it */
    atomic_int_store(&task->t_cancel, 1);

        /* finally, synchronize with worker threads resp. external threads if necessary */
    if (sync)
    {
    #if PD_WORKERTHREADS
        if (sys->ts_running)
        {
            t_taskqueue *fromsched = &sys->ts_fromsched;
            t_taskqueue *tosched = &sys->ts_tosched;
                /* check if the task is still in the 'fromsched' queue;
                 * if yes, we don't need to do anything since the worker
                 * function will never be called. */
            taskqueue_lock(fromsched);
            for (t = fromsched->tq_head; t; t = t->t_next)
            {
                if (t == task)
                {
                    taskqueue_unlock(fromsched);
                    return 0;
                }
            }
            taskqueue_unlock(fromsched);
                /* wait until the task is in the 'tosched' queue */
            taskqueue_lock(tosched);
            while (1)
            {
                for (t = tosched->tq_head; t; t = t->t_next)
                {
                    if (t == task)
                    {
                        taskqueue_unlock(tosched);
                        return 1;
                    }
                }
                taskqueue_wait(tosched);
            }
        }
        else
    #endif
            /* if the task is suspended, wait until it has been resumed
             * by the external thread.
             * NB: task_doresume() will be called in tasksystem_poll(). */
            if (task_issuspended(task))
                task_waitforresume(task);
    }
    return 1;
}

int task_check(t_task *task)
{
    return atomic_int_load(&task->t_cancel) == 0;
}

void task_yield(t_task *task, t_task_workfn workfn)
{
    t_tasksystem *sys = STUFF->st_tasksystem;
    task->t_workfn = workfn;
        /* tell the task system that we have yielded so it can reschedule the task */
    atomic_int_fetch_add(&task->t_suspend, 1);
}

void task_suspend(t_task *task)
{
        /* must be decremented both in sys_tasksystem_perform() and task_resume()
         * resp. tasksystem_poll() and task_resume() */
    atomic_int_fetch_add(&task->t_suspend, 2);
}

void task_resume(t_task *task, t_task_workfn workfn)
{
#ifdef PDINSTANCE
        /* NOTE: the Pd instance might not be set! */
    t_tasksystem *sys = task->t_instance->pd_stuff->st_tasksystem;
#else
    t_tasksystem *sys = STUFF->st_tasksystem;
#endif
    task->t_workfn = workfn;
    task_doresume(sys, task);
}

void task_notify(t_task *task, void *data, t_task_callback fn)
{
#ifdef PDINSTANCE
        /* NOTE: the Pd instance might not be set! */
    t_tasksystem *sys = task->t_instance->pd_stuff->st_tasksystem;
#else
    t_tasksystem *sys = STUFF->st_tasksystem;
#endif
    t_message *msg = (t_message *)getbytes(sizeof(t_message));
    msg->m_next = 0;
    msg->m_task = task;
    msg->m_data = data;
    msg->m_fn = fn;
#if PD_WORKERTHREADS
    taskqueue_lock(&sys->ts_tosched);
#endif
    messqueue_push(&sys->ts_messages, msg);
#if PD_WORKERTHREADS
    taskqueue_unlock(&sys->ts_tosched);
#endif
}
