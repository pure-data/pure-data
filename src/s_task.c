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
 * t_task *task_sched(t_pd *owner, void *data, task_workfn workfn, task_callback cb)
 *
 * description: schedule a new task.
 *
 * arguments:
 * 1) (t_pd *) owner: the Pd object that owns the given task.
 *    NB: this argument can be NULL in case there is no valid owner;
 *    for example, this can be used for scheduling a task from within the
 *    callback of a cancelled task (where 'owner' will be NULL) to avoid
 *    cleaning up resources on the audio thread.
 * 2) (void *) data: a user-specific dynamically allocated struct.
 *    This struct contains the input data for the worker function, e.g. a filename,
 *    and also the output data, e.g. a memory buffer. Try to avoid any references
 *    to shared state. Instead, treat the data object as a message that is exchanged
 *    between your Pd object and the task system.
 *    NOTE: In the rare case where you do need to access shared state, make sure
 *    to call task_cancel() with sync=1! For an example, see the "progress" method
 *    in "doc/6.externs/taskobj.c".
 * 3) (task_workfn) workfn: a function to be called in the background.
 *    You would typically read from 'data', perform some operations and store
 *    the result back to 'data'.
 *    ***IMPORTANT***: do not call any Pd API functions, except for task_check(),
 *    task_suspend() and task_resume(). In particular, do not call sys_lock()
 *    or sys_unlock(), as this can cause deadlocks!
 * 4) (task_callback) cb: a function to be called after the task has completed.
 *    This is where the result can be safely moved from 'data' to the owning object.
 *    If the task has been cancelled, 'owner' is NULL; in this case, do not forget
 *    to clean up any resources in 'data'.
 *    Finally, here you can also safely free the 'data' object itself!
 *
 * returns: a handle to the new task.
 *    Store the handle in your object, so that you are able to cancel the task
 *    later if needed, see task_cancel().
 *    ***IMPORTANT***: do not try to use the task handle after the task has finished;
 *    typically you would unset the task handle in the callback function.
 *
 * NOTE: the order in which tasks are executed is undefined! Implementations might
 * use multiple worker threads which execute task concurrently.
 *
 * --------------------------------------------------------------------------------
 *
 * t_task *task_spawn(t_pd *owner, void *data, task_workfn workfn, task_callback cb)
 *
 * description: spawn a new task
 *    This function behaves just like task_sched(), except it runs the worker
 *    function in its own thread, so that other tasks can run concurrently.
 *    Use this if a task might take a long time - let's say more than 1 second
 *    - and you cannot use asynchronous I/O with task_suspend() + task_resume().
 *
 * ---------------------------------------------------------------------------------
 *
 * int task_cancel(t_task *task, int sync)
 *
 * description: cancel a given task.
 *     You always have to call this in the object destructor to cancel any pending
 *     tasks; this makes sure that the task callback function won't try to access
 *     an object that has already been deleted!
 *     It might also be useful in cases where the user is calling an asynchronous
 *     method while a task is still running (another option would be to simply forbid
 *     the call until the pending task has finished).
 *     Note that the task might be currently executing or it might have completed
 *     already. Either way, the task handle will be invalidated and the callback
 *     will eventually be called with 'owner' set to NULL.
 *
 * arguments:
 * 1) (t_task *) task: the task that shall be cancelled.
 * 2) (int) sync: synchronize with worker thread(s).
 *     true (1): check if the task is currently being executed and if yes, wait for its
 *               completion. Typically, this is only necessary if the task data contains
 *               references to shared state - which you should avoid in the first place!
 *     false (0): return immediately without blocking.
 *
 * returns: always 0.
 *     In the future, this may return additional information.
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
 * void task_suspend(t_task *task)
 *
 * description: suspend current execution
 *     Call this function AFTER deferring to another thread, typically as the last
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
 * 1) (t_task *) task: the task handle
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
 * void sys_taskqueue_start(int threads)
 *
 * description: start the task queue system
 *
 * arguments:
 * 1) (int) threads: the number of (internal) worker threads, or one of the following
 *     constants:
 *     TASKQUEUE_DEFAULT: use default number of threads;
 *     TASKQUEUE_EXTERNAL: use external worker threads
 *
 * NOTE: if sys_taskqueue_start() is not called, the tasks are executed synchronously
 * on the audio/scheduler thread. This is useful for batch/offline processing, for example.
 *
 * ----------------------------------------------------------------------------------
 *
 * void sys_taskqueue_stop(void)
 *
 * description: stop the task queue system
 *     After this call, a libpd client can safely join any external worker threads.
 *
 * ----------------------------------------------------------------------------------
 *
 * int sys_taskqueue_perform(t_pdinstance *pd, int nonblocking)
 *
 * description: execute pending tasks.
 *     This function is called from the external worker thread(s).
 *     It can be safely called from multiple threads at the same time!
 *
 * arguments:
 * 1) (t_pdinstance *) pd: the Pd instance
 * 2) (int) nonblocking: call in non-blocking mode (true/false)
 *     true (1): execute a *single* task, if available, otherwise return immediately.
 *     false (0): block until the task queue is stopped with sys_taskqueue_stop().
 *
 * returns: 1 if it did something or 0 if there was nothing to do.
 *
 * ----------------------------------------------------------------------------------
 *
 * int sys_taskqueue_running(void)
 *
 * description: check if the task system is running.
 *
 * returns: true (1) or false (0)
 *
 * ==================================================================================
 */

#include "m_pd.h"
#include "s_stuff.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* If PD_WORKERTHREADS is 1, tasks are pushed to a queue. A worker thread pops
 * tasks from the queue and executes the 'task_workfn' function. On completion,
 * tasks are pushed to another queue that is regularly polled by the scheduler.
 * This is were the 'task_callback' function is called.
 *
 * If PD_WORKERTHREADS is 0, tasks are executed on the audio/scheduler thread.
 * This is intended to support single threaded systems. To maintain the same
 * semantics as the threaded implementation (the callback is guaranteed to run
 * after task_sched() has returned), we can't just execute tasks and its callbacks
 * synchronously; instead we have to put them on a queue where they are popped and
 * dispatch with the next scheduler tick. */

#ifndef PD_WORKERTHREADS
#define PD_WORKERTHREADS 1
#endif

#if PD_WORKERTHREADS

#include <pthread.h>

/* NB: atomic_increment() and atomic_decrement() return the NEW value */
#if defined(_MSC_VER) /* MSVC */
#define atomic_int volatile LONG
#define atomic_increment(x) _InterlockedIncrement(x)
#define atomic_decrement(x) _InterlockedDecrement(x)
#elif defined(__GNUC__) /* GCC, clang, ICC */
#define atomic_int volatile int
#define atomic_increment(x) __atomic_add_fetch((x), 1, __ATOMIC_SEQ_CST)
#define atomic_decrement(x) __atomic_sub_fetch((x), 1, __ATOMIC_SEQ_CST)
#else
#error "compiler not supported, please build with -DPD_WORKERTHREADS=0"
#endif

#else /* PD_WORKERTHREADS */

#define atomic_int volatile int

#endif

struct _task
{
    struct _task *t_next;
    t_pd *t_owner;
    void *t_data;
    t_task_workfn t_workfn;
    t_task_callback t_cb;
    atomic_int t_cancel;
    atomic_int t_suspend;
};

typedef struct _tasklist
{
    t_task *tl_head;
    t_task *tl_tail;
#if PD_WORKERTHREADS
    pthread_mutex_t tl_mutex;
    pthread_cond_t tl_condition;
#endif
} t_tasklist;

static void tasklist_init(t_tasklist *list)
{
    list->tl_head = list->tl_tail = 0;
#if PD_WORKERTHREADS
    pthread_mutex_init(&list->tl_mutex, 0);
    pthread_cond_init(&list->tl_condition, 0);
#endif
}

static void tasklist_flush(t_tasklist *list)
{
    t_task *task = list->tl_head;
    while (task)
    {
        t_task *next = task->t_next;
            /* the callback will free the data */
        task->t_cb(0, task->t_data);
        freebytes(task, sizeof(t_task));
        task = next;
    }
    list->tl_head = list->tl_tail = 0;
}

static void tasklist_free(t_tasklist *list)
{
    tasklist_flush(list);
#if PD_WORKERTHREADS
    pthread_mutex_destroy(&list->tl_mutex);
    pthread_cond_destroy(&list->tl_condition);
#endif
}

static void tasklist_push(t_tasklist *list, t_task *task)
{
    if (list->tl_tail)
        list->tl_tail = (list->tl_tail->t_next = task);
    else
        list->tl_head = list->tl_tail = task;
}

static t_task *tasklist_pop(t_tasklist *list)
{
    if (list->tl_head)
    {
        t_task *task = list->tl_head;
        list->tl_head = task->t_next;
        if (!list->tl_head) /* list is now empty */
            list->tl_tail = 0;
        return task;
    }
    else
        return 0;
}

#if PD_WORKERTHREADS

static void tasklist_lock(t_tasklist *list)
{
    pthread_mutex_lock(&list->tl_mutex);
}

static void tasklist_unlock(t_tasklist *list)
{
    pthread_mutex_unlock(&list->tl_mutex);
}

static void tasklist_notify(t_tasklist *list)
{
    pthread_cond_signal(&list->tl_condition);
}

static void tasklist_notify_all(t_tasklist *list)
{
    pthread_cond_broadcast(&list->tl_condition);
}

static void tasklist_wait(t_tasklist *list)
{
    pthread_cond_wait(&list->tl_condition, &list->tl_mutex);
}

#endif

struct _taskqueue
{
#if PD_WORKERTHREADS
    t_tasklist tq_fromsched;
    t_tasklist tq_tosched;
    pthread_t *tq_threads; /* internal worker threads */
    int tq_numthreads; /* number of internal worker threads */
    int tq_running; /* thread(s) are running? */
    int tq_external; /* use external worker threads */
    atomic_int tq_pending; /* number of pending tasks */
#else
    t_tasklist tq_list;
#endif
    t_clock *tq_clock;
};

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

    sys_taskqueue_perform((t_pdinstance *)x, 0);

    return 0;
}

int sys_taskqueue_perform(t_pdinstance *pd, int nonblocking)
{
    t_taskqueue *queue = pd->pd_stuff->st_taskqueue;
    t_tasklist *fromsched = &queue->tq_fromsched;
    t_tasklist *tosched = &queue->tq_tosched;
    int didsomething = 0;

#ifdef PDINSTANCE
    pd_setinstance(pd);
#endif

    tasklist_lock(fromsched);
    while (queue->tq_running)
    {
            /* try to pop item from list */
        t_task *task = tasklist_pop(fromsched);
        if (task)
        {
        do_task:
                /* increment with mutex locked, see task_cancel() */
            atomic_increment(&queue->tq_pending);
            tasklist_unlock(fromsched);

                /* execute task (without lock!) */
            if (!task->t_cancel)
            {
                task->t_workfn(task, task->t_data);
                didsomething = 1;
            }

            tasklist_lock(tosched);
            if (task->t_suspend > 0) /* suspended */
            {
                    /* NB: it is unlikely, but entirely possible, that the task
                    is already fully resumed at this point, see task_resume() */
                if (--task->t_suspend == 0) /* fully resumed */
                {
                        /* decrement with mutex locked, see task_cancel() */
                    if (atomic_decrement(&queue->tq_pending) < 0)
                        fprintf(stderr, "bug: negative pending task count\n");
                    if (task->t_workfn && !task->t_cancel) /* continue */
                    {
                        tasklist_unlock(tosched);
                        tasklist_lock(fromsched);
                        goto do_task;
                    }
                        /* move task back to scheduler */
                    tasklist_push(tosched, task);
                    tasklist_notify(tosched);
                }
            }
            else /* not suspended */
            {
                if (task->t_suspend < 0)
                    fprintf(stderr, "bug: negative task suspend count\n");
                    /* decrement with mutex locked, see task_cancel() */
                if (atomic_decrement(&queue->tq_pending) < 0)
                    fprintf(stderr, "bug: negative pending task count\n");
                    /* move task back to scheduler */
                tasklist_push(tosched, task);
                    /* notify the scheduler thread because it might
                    be waiting in task_cancel() */
                tasklist_notify(tosched);
            }
            tasklist_unlock(tosched);

            tasklist_lock(fromsched);
        }
        else
        {
            if (nonblocking)
                break;
            else /* wait for new task */
                tasklist_wait(fromsched);
        }
    }
    tasklist_unlock(fromsched);

    return didsomething;
}

void sys_taskqueue_start(int threads)
{
    int i, err;
    t_taskqueue *queue = STUFF->st_taskqueue;
    if (queue->tq_running)
    {
        pd_error(0, "cannot start taskqueue: already running");
        return;
    }
    if (threads == TASKQUEUE_EXTERNAL) /* external worker threads */
    {
        queue->tq_external = 1;
        queue->tq_running = 1;
    }
    else /* internal worker threads */
    {
        if (threads == TASKQUEUE_DEFAULT)
            threads = DEFNUMWORKERTHREADS;
        if (threads <= 0)
        {
            bug("sys_taskqueue_start");
            return;
        }
        queue->tq_external = 0;
        queue->tq_running = 1;
        queue->tq_numthreads = threads;
        queue->tq_threads = getbytes(threads * sizeof(pthread_t));
        for (i = 0; i < threads; i++)
        {
            if ((err = pthread_create(&queue->tq_threads[i], 0, task_threadfn, pd_this)))
                pd_error(0, "could not create worker thread (%d)", err);
        }
    }
}

void sys_taskqueue_stop(void)
{
    t_taskqueue *queue = STUFF->st_taskqueue;
    t_tasklist *fromsched = &queue->tq_fromsched;
    t_tasklist *tosched = &queue->tq_tosched;
    if (!queue->tq_running)
        return;
        /* quit and notify worker thread(s) */
    tasklist_lock(fromsched);
    queue->tq_running = 0;
    tasklist_unlock(fromsched);
    tasklist_notify_all(fromsched);
    if (!queue->tq_external)
    {
            /* join internal worker thread(s) */
        int i;
        for (i = 0; i < queue->tq_numthreads; i++)
            pthread_join(queue->tq_threads[i], 0);
        freebytes(queue->tq_threads, queue->tq_numthreads * sizeof(pthread_t));
        queue->tq_threads = 0;
        queue->tq_numthreads = 0;
    }
    queue->tq_external = 0;
        /* wait for any pending tasks */
    tasklist_lock(tosched);
    while (queue->tq_pending > 0)
    {
        fprintf(stderr, "waiting for %d pending tasks...", queue->tq_pending);
        fflush(stderr);
        tasklist_wait(tosched);
    }
    tasklist_unlock(tosched);
        /* finally flush task lists */
    tasklist_flush(fromsched);
    tasklist_flush(tosched);
}

#else

int sys_taskqueue_perform(t_pdinstance *pd, int nonblocking)
{
    fprintf(stderr, "warning: sys_taskqueue_perform() called without PD_WORKERTHREADS!\n");
    return 0;
}

void sys_taskqueue_start(int threads)
{
    if (threads == TASKQUEUE_EXTERNAL)
        fprintf(stderr, "warning: trying to use external worker threads without PD_WORKERTHREADS!\n");
}

void sys_taskqueue_stop(void)
{
    tasklist_flush(&STUFF->st_taskqueue->tq_list);
}

#endif /* PD_WORKERTHREADS */

static void taskqueue_dopoll(t_taskqueue *queue);

void s_task_newpdinstance(void)
{
    t_taskqueue *queue =
        STUFF->st_taskqueue = getbytes(sizeof(t_taskqueue));

#if PD_WORKERTHREADS
    tasklist_init(&queue->tq_tosched);
    tasklist_init(&queue->tq_fromsched);
    queue->tq_threads = 0;
    queue->tq_numthreads = 0;
    queue->tq_external = 0;
    queue->tq_running = 0;
    queue->tq_pending = 0;
#else
    tasklist_init(&queue->tq_list);
#endif
        /* the clock is used to defer the callback to the next
        clock timeout in case there is no worker thread. */
    queue->tq_clock = clock_new(queue, (t_method)taskqueue_dopoll);
}

void s_task_freepdinstance(void)
{
    t_taskqueue *queue = STUFF->st_taskqueue;
#if PD_WORKERTHREADS
    sys_taskqueue_stop();
    tasklist_free(&queue->tq_fromsched);
    tasklist_free(&queue->tq_tosched);
#else
    tasklist_free(&queue->tq_list);
#endif
    clock_free(queue->tq_clock);

    freebytes(queue, sizeof(t_taskqueue));
}

static void taskqueue_dopoll(t_taskqueue *queue)
{
#if PD_WORKERTHREADS
    t_tasklist *list = &queue->tq_tosched;
#else
    t_tasklist *list = &queue->tq_list;
#endif
    t_task *task;
    while (1)
    {
            /* try to pop task from list */
    #if PD_WORKERTHREADS
        tasklist_lock(list);
    #endif
        task = tasklist_pop(list);
    #if PD_WORKERTHREADS
        tasklist_unlock(list);
    #endif
        if (task)
        {
                /* run callback (without lock!)
                if the task has been cancelled, 'owner' is set to 0! */
            task->t_cb(task->t_cancel ? 0 : task->t_owner, task->t_data);
            freebytes(task, sizeof(t_task));
        }
        else
            break;
    }
}

    /* called in sched_tick() */
void taskqueue_poll(void)
{
#if PD_WORKERTHREADS
    t_taskqueue *queue = STUFF->st_taskqueue;
    if (queue->tq_running)
        taskqueue_dopoll(queue);
#endif
}

t_task *task_sched(t_pd *owner, void *data,
    t_task_workfn workfn, t_task_callback cb)
{
    t_taskqueue *queue = STUFF->st_taskqueue;
#if PD_WORKERTHREADS
    t_tasklist *fromsched = &queue->tq_fromsched;
#else
    t_tasklist *list = &STUFF->st_taskqueue->tq_list;
#endif

    t_task *task = getbytes(sizeof(t_task));
    task->t_next = 0;
    task->t_owner = owner;
    task->t_data = data;
    task->t_workfn = workfn;
    task->t_cb = cb;
    task->t_cancel = 0;
    task->t_suspend = 0;

#if PD_WORKERTHREADS
    if (queue->tq_running)
    {
            /* push task to list and notify one worker thread */
        tasklist_lock(fromsched);
        tasklist_push(fromsched, task);
        tasklist_unlock(fromsched);
        tasklist_notify(fromsched);
    }
    else
    {
            /* execute work function synchronously */
        task->t_workfn(task, task->t_data);
            /* push to list and defer to next clock timeout */
        tasklist_push(&queue->tq_tosched, task);
        clock_delay(queue->tq_clock, 0);
    }
#else
        /* execute work function synchronously */
    task->t_workfn(task, task->t_data);
        /* push to list and defer to next clock timeout */
    tasklist_push(list, task);
    clock_delay(queue->tq_clock, 0);
#endif
    return task;
}

#if PD_WORKERTHREADS

typedef struct _spawn_data
{
    t_task *task;
    t_task_workfn workfn;
    t_task_callback cb;
    void *data;
} t_spawn_data;

static void *task_spawn_threadfn(void *x)
{
    t_spawn_data *y = (t_spawn_data *)x;
        /* in case pthread_attr_setinheritsched() did not work (seems to be the case on Windows...) */
    lower_thread_priority();
    y->workfn(y->task, y->data);
    task_resume(y->task, 0);
    return 0;
}

static void task_spawn_workfn(t_task *task, t_spawn_data *data)
{
    pthread_t tid;
    pthread_attr_t attr;
    int err;
    data->task = task;
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
    {
        fprintf(stderr, "error: pthread_create() failed (%d)\n", err);
        task->t_cancel = 1;
    }
    pthread_attr_destroy(&attr);
}

static void task_spawn_callback(t_pd *x, t_spawn_data *y)
{
    y->cb(x, y->data);
    freebytes(y, sizeof(t_spawn_data));
}

#endif /* PD_WORKERTHREADS */

t_task *task_spawn(t_pd *owner, void *data,
    t_task_workfn workfn, t_task_callback cb)
{
#if PD_WORKERTHREADS
    if (STUFF->st_taskqueue->tq_running)
    {
        t_spawn_data *d = (t_spawn_data *)getbytes(sizeof(t_spawn_data));
        d->workfn = workfn;
        d->cb = cb;
        d->data = data;
        return task_sched(owner, d, (t_task_workfn)task_spawn_workfn,
            (t_task_callback)task_spawn_callback);
    }
#endif /* PD_WORKERTHREADS */
        /* just run synchronously */
    return task_sched(owner, data, workfn, cb);
}

int task_cancel(t_task *task, int sync)
{
    t_taskqueue *queue = STUFF->st_taskqueue;
    task->t_cancel = 1;
#if PD_WORKERTHREADS
    if (sync && queue->tq_running)
    {
        t_tasklist *fromsched = &queue->tq_fromsched;
        t_tasklist *tosched = &queue->tq_tosched;
        t_task *t;
            /* check if the task is still in the 'fromsched' queue */
        tasklist_lock(fromsched);
        for (t = fromsched->tl_head; t; t = t->t_next)
        {
            if (t == task)
            {
                tasklist_unlock(fromsched);
                return 0;
            }
        }
            /* check if the task is already in the 'tosched' queue;
            continue until the pending count has reached 0. Keep the
            'fromsched' list locked so that the count can only go down. */
        tasklist_lock(tosched);
        while (1)
        {
            for (t = tosched->tl_head; t; t = t->t_next)
            {
                if (t == task)
                {
                    tasklist_unlock(tosched);
                    tasklist_unlock(fromsched);
                    return 0;
                }
            }
            if (queue->tq_pending > 0)
                tasklist_wait(tosched);
            else
            {
                pd_error(0, "task_cancel: could not find task");
                break;
            }
        }
        tasklist_unlock(tosched);
        tasklist_unlock(fromsched);
    }
#endif
    return 0;
}

int task_check(t_task *task)
{
    return task->t_cancel == 0;
}

#define POLL_INTERVAL 1

static int taskqueue_havethreads(void)
{
#if PD_WORKERTHREADS
    return (STUFF->st_taskqueue->tq_numthreads > 0)
        || STUFF->st_taskqueue->tq_external;
#else
    return 0;
#endif
}

void task_suspend(t_task *task)
{
#if PD_WORKERTHREADS
    t_taskqueue *queue = STUFF->st_taskqueue;
        /* NB: tq_running might have been unset in sys_taskqueue_stop()! */
    if (taskqueue_havethreads())
    {
        t_tasklist *tosched = &queue->tq_tosched;
        tasklist_lock(tosched);
            /* must be decremented both in taskqueue_perform() and task_resume() */
        task->t_suspend += 2;
        tasklist_unlock(tosched);
        return;
    }
#endif /* PD_WORKERTHREADS */
        /* poll until task_resume() has been called from the other thread */
    while (task->t_suspend == 0)
    {
  #ifdef _WIN32
        Sleep(POLL_INTERVAL);
  #else
        usleep(POLL_INTERVAL * 1000);
  #endif
    }
    task->t_suspend = 0;
}

void task_resume(t_task *task, t_task_workfn workfn)
{
#if PD_WORKERTHREADS
    t_taskqueue *queue = STUFF->st_taskqueue;
        /* NB: tq_running might have been unset in sys_taskqueue_stop()! */
    if (taskqueue_havethreads())
    {
        t_tasklist *fromsched = &queue->tq_fromsched;
        t_tasklist *tosched = &queue->tq_tosched;

        tasklist_lock(tosched);
        task->t_workfn = workfn;
            /* NB: it is unlikely, but entirely possible, that
            task_resume() is called before task_suspend() or
            before the task is handled in taskqueue_perform().
            In this case we do not want to handle the task yet;
            instead, it will be handled later in taskqueue_perform(). */
        if (--task->t_suspend == 0) /* fully resumed */
        {
                /* decrement with mutex locked, see task_cancel() */
            if (atomic_decrement(&queue->tq_pending) < 0)
                fprintf(stderr, "bug: negative pending task count\n");
                /* NB: don't reschedule if the taskqueue has been stopped! */
            if (workfn && queue->tq_running && !task->t_cancel) /* continue */
            {
                tasklist_unlock(tosched);
                    /* reschedule task */
                tasklist_lock(fromsched);
                tasklist_push(fromsched, task);
                tasklist_unlock(fromsched);
                tasklist_notify(fromsched);
            }
            else /* completed/cancelled */
            {
                tasklist_push(tosched, task);
                tasklist_unlock(tosched);
                tasklist_notify(tosched);
            }
        }
        else
            tasklist_unlock(tosched); /* ! */
        return;
    }
#endif /* PD_WORKERTHREADS */
    task->t_suspend = 1;
}

int sys_taskqueue_running(void)
{
#if PD_WORKERTHREADS
    return STUFF->st_taskqueue->tq_running;
#else
    return 0;
#endif
}
