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
 * --------------------------------------------------------------------------------
 *
 * t_task *task_spawn(t_pd *owner, void *data, task_workfn workfn, task_callback cb)
 *
 * description: spawn a new task
 *    This function behaves just like task_start(), except that it runs the worker
 *    function in its own thread, so that other tasks can run concurrently.
 *    Use this if a task might take a long time - let's say more than 1 second.
 *
 * ---------------------------------------------------------------------------------
 *
 * int task_stop(t_task *task, int sync)
 *
 * description: cancel a given task.
 *     You always have to call this in the object destructor for any pending tasks(s);
 *     this makes sure that the task callback function won't try to access an object
 *     that has already been deleted!
 *     It might also be useful in cases where the user is calling an asynchronous
 *     method while it is still running. (Another option would be to reject such
 *     method calls until the pending task has finished.)
 *     Note that the task might be currently executing or it might have completed
 *     already. Either way, the task handle will be invalidated and the callback
 *     will eventually be called with 'owner' set to NULL.
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
 * void sys_taskqueue_start(int threads)
 *
 * description: start the task queue system
 *
 * arguments:
 *
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
 *
 * 1) (t_pdinstance *) pd: the Pd instance
 *
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
#endif /* _WIN32 */
#include <pthread.h>

/* If PD_WORKERTHREADS is 1, tasks are pushed to a queue. A worker thread pops
 * tasks from the queue and executes the 'task_workfn' function. On completion,
 * tasks are pushed to another queue that is regularly polled by the scheduler.
 * This is were the 'task_callback' function is called.
 *
 * If PD_WORKERTHREADS is 0, tasks are executed on the audio/scheduler thread.
 * This is intended to support single threaded systems. To maintain the same
 * semantics as the threaded implementation (the callback is guaranteed to run
 * AFTER task_start() has returned), we can't just execute tasks and its callbacks
 * synchronously; instead we have to put them on a queue from where they are popped
 * and dispatched at the next scheduler tick. */

#ifndef PD_WORKERTHREADS
#define PD_WORKERTHREADS 1
#endif

#if PD_WORKERTHREADS

/* NB: atomic_increment() and atomic_decrement() return the NEW value */
#if defined(_MSC_VER) /* MSVC */
#define atomic_int volatile LONG
#define atomic_increment(x) _InterlockedIncrement(x)
#define atomic_decrement(x) _InterlockedDecrement(x)
#define atomic_add(x, i) _InterlockedAdd((x), i)
#elif defined(__GNUC__) /* GCC, clang, ICC */
#define atomic_int volatile int
#define atomic_increment(x) __atomic_add_fetch((x), 1, __ATOMIC_SEQ_CST)
#define atomic_decrement(x) __atomic_sub_fetch((x), 1, __ATOMIC_SEQ_CST)
#define atomic_add(x, i) __atomic_add_fetch((x), i, __ATOMIC_SEQ_CST)
#else
#error "compiler not supported, please build with -DPD_WORKERTHREADS=0"
#endif

#else /* PD_WORKERTHREADS */

#define atomic_int volatile int

#endif

struct _task
{
    struct _task *t_next;
    struct _task *t_nextpending;
#ifdef PDINSTANCE
    t_pdinstance *t_instance;
#endif
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

static void tasklist_push_sync(t_tasklist *list, t_task *task, int notify)
{
#if PD_WORKERTHREADS
    pthread_mutex_lock(&list->tl_mutex);
#endif
    tasklist_push(list, task);
#if PD_WORKERTHREADS
    pthread_mutex_unlock(&list->tl_mutex);
    if (notify)
        pthread_cond_signal(&list->tl_condition);
#endif
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

static t_task *tasklist_popall(t_tasklist *list)
{
    t_task *task = list->tl_head;
    list->tl_head = list->tl_tail = 0;
    return task;
}

static int tasklist_empty(t_tasklist *list)
{
    return list->tl_head == 0;
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

typedef struct _message
{
    struct _message *m_next;
    t_task *m_task;
    void *m_data;
    t_task_callback m_fn;
} t_message;

typedef struct _messagelist
{
    t_message *ml_head;
    t_message *ml_tail;
} t_messagelist;

static void messagelist_init(t_messagelist *list)
{
    list->ml_head = list->ml_tail = 0;
}

static void messagelist_push(t_messagelist *list, t_message *m)
{
    if (list->ml_tail)
        list->ml_tail = (list->ml_tail->m_next = m);
    else
        list->ml_head = list->ml_tail = m;
}

static t_message *messagelist_popall(t_messagelist *list)
{
    t_message *m = list->ml_head;
    list->ml_head = list->ml_tail = 0;
    return m;
}

static void messagelist_flush(t_messagelist *list)
{
    t_message *m = list->ml_head;
    while (m)
    {
        t_message *next = m->m_next;
        m->m_fn(0, m->m_data);
        freebytes(m, sizeof(*m));
        m = next;
    }
    list->ml_head = list->ml_tail = 0;
}

struct _taskqueue
{
    t_task *tq_pending; /* list of pending tasks */
    t_tasklist tq_tosched; /* finished tasks */
#if PD_WORKERTHREADS
    t_tasklist tq_fromsched; /* scheduled tasks */
    pthread_t *tq_threads; /* internal worker threads */
    int tq_numthreads; /* number of internal worker threads */
    int tq_running; /* thread(s) are running? */
    int tq_external; /* use external worker threads */
#endif
    t_messagelist tq_messages; /* messages sent to the scheduler */
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
            tasklist_unlock(fromsched);
        do_task:
                /* execute task (without lock!) */
            if (!task->t_cancel)
            {
                task->t_workfn(task, task->t_data);
                didsomething = 1;
            }

            if (task->t_suspend > 0) /* suspended */
            {
                    /* NB: it is unlikely, but entirely possible, that the task
                    is already fully resumed at this point, see task_resume() */
                if (atomic_decrement(&task->t_suspend) == 0) /* fully resumed */
                {
                    if (task->t_workfn && !task->t_cancel) /* continue */
                        goto do_task;
                    else
                        /* move task back to scheduler and notify the main thread
                         * since it might be waiting in task_stop() */
                        tasklist_push_sync(tosched, task, 1);
                }
            }
            else /* not suspended */
            {
                    /* move task back to scheduler and notify the main thread
                     * since it might be waiting in task_stop() */
                tasklist_push_sync(tosched, task, 1);
            }

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

static void taskqueue_dopoll(t_taskqueue *queue);

void sys_taskqueue_stop(void)
{
    t_taskqueue *queue = STUFF->st_taskqueue;
    t_tasklist *fromsched = &queue->tq_fromsched;
    t_tasklist *tosched = &queue->tq_tosched;
    if (!queue->tq_running)
    {
            /* just flush queues and return */
        tasklist_flush(&queue->tq_fromsched);
        tasklist_flush(&queue->tq_tosched);
        messagelist_flush(&queue->tq_messages);
        queue->tq_pending = 0;
        return;
    }
        /* wait until all pending tasks have finished.
         * (This will also flush all queues.) */
    while (queue->tq_pending)
    {
        taskqueue_dopoll(queue);
        if (queue->tq_pending)
        {
                /* if there are still pending tasks after taskqueue_dopoll()
                 * and 'tosched' is empty, we need to wait for the thread(s)
                 * to push the remaining tasks. */
            fprintf(stderr, "waiting for pending tasks...\n");
            tasklist_lock(tosched);
            while (tasklist_empty(tosched))
                tasklist_wait(tosched);
            tasklist_unlock(tosched);
        }
    }
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
    tasklist_flush(&STUFF->st_taskqueue->tq_tosched);
    messagelist_flush(&STUFF->st_taskqueue->tq_messages);
    STUFF->st_taskqueue->tq_pending = 0;
}

#endif /* PD_WORKERTHREADS */

static void taskqueue_dopoll(t_taskqueue *queue);

void s_task_newpdinstance(void)
{
    t_taskqueue *queue =
        STUFF->st_taskqueue = getbytes(sizeof(t_taskqueue));

    queue->tq_pending = 0;
    tasklist_init(&queue->tq_tosched);
#if PD_WORKERTHREADS
    tasklist_init(&queue->tq_fromsched);
    queue->tq_threads = 0;
    queue->tq_numthreads = 0;
    queue->tq_external = 0;
    queue->tq_running = 0;
#endif
    messagelist_init(&queue->tq_messages);
        /* the clock is used to defer the callback to the next
        clock timeout in case there is no worker thread. */
    queue->tq_clock = clock_new(queue, (t_method)taskqueue_dopoll);
}

void s_task_freepdinstance(void)
{
    t_taskqueue *queue = STUFF->st_taskqueue;
    sys_taskqueue_stop();
    tasklist_free(&queue->tq_tosched);
#if PD_WORKERTHREADS
    tasklist_free(&queue->tq_fromsched);
#endif
    messagelist_flush(&queue->tq_messages);
    clock_free(queue->tq_clock);

    freebytes(queue, sizeof(t_taskqueue));
}

static void taskqueue_dopoll(t_taskqueue *queue)
{
    t_tasklist *tosched = &queue->tq_tosched;
    t_task *task;
    t_message *msg;
        /* atomically pop all messages and finished tasks */
#if PD_WORKERTHREADS
    tasklist_lock(tosched);
#endif
    msg = messagelist_popall(&queue->tq_messages);
    task = tasklist_popall(tosched);
#if PD_WORKERTHREADS
    tasklist_unlock(tosched);
#endif
        /* first dispatch all messages because they are associated with tasks
         * and always scheduled before the respective task has finished. */
    while (msg)
    {
        t_message *next = msg->m_next;
            /* If the associated task has been cancelled, set 'owner' to NULL! */
        msg->m_fn(msg->m_task->t_cancel ? 0 : msg->m_task->t_owner, msg->m_data);
        freebytes(msg, sizeof(*msg));
        msg = next;
    }
        /* now dispatch all tasks. */
    while (task)
    {
        t_task *t, *next = task->t_next;
        if (task->t_suspend < 0)
            bug("taskqueue_dopoll: negative task suspend count");
            /* Run callback. If the task has been cancelled, set 'owner' to NULL! */
        if (task->t_cb)
            task->t_cb(task->t_cancel ? 0 : task->t_owner, task->t_data);
            /* remove from pending list */
        if (task == queue->tq_pending)
            queue->tq_pending = task->t_nextpending;
        else
        {
            int found = 0;
            for (t = queue->tq_pending; t; t = t->t_nextpending)
            {
                if (t->t_nextpending == task)
                {
                    t->t_nextpending = task->t_nextpending;
                    found = 1;
                    break;
                }
            }
            if (!found)
                bug("taskqueue_dopoll: task not in pending list");
        }
        freebytes(task, sizeof(*task));
        task = next;
    }
}

    /* called in sched_tick() */
void taskqueue_poll(void)
{
    t_taskqueue *queue = STUFF->st_taskqueue;
    if (queue->tq_pending)
        taskqueue_dopoll(queue);
}

t_task *task_start(t_pd *owner, void *data,
    t_task_workfn workfn, t_task_callback cb)
{
    t_taskqueue *queue = STUFF->st_taskqueue;
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
    task->t_nextpending = queue->tq_pending;
    queue->tq_pending = task;

#if PD_WORKERTHREADS
    if (queue->tq_running)
    {
            /* push task to 'fromsched' list and notify one worker thread */
        tasklist_push_sync(&queue->tq_fromsched, task, 1);
        return task;
    }
#endif
        /* execute work function synchronously */
    task->t_workfn(task, task->t_data);
        /* push to 'tosched' list and defer to next clock timeout */
    tasklist_push(&queue->tq_tosched, task);
    clock_delay(queue->tq_clock, 0);
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
#ifdef PDINSTANCE
    pd_setinstance(y->task->t_instance);
#endif
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

t_task *task_spawn(t_pd *owner, void *data,
    t_task_workfn workfn, t_task_callback cb)
{
    if (STUFF->st_taskqueue->tq_running)
    {
        t_spawn_data *d = (t_spawn_data *)getbytes(sizeof(t_spawn_data));
        d->workfn = workfn;
        d->cb = cb;
        d->data = data;
        return task_start(owner, d, (t_task_workfn)task_spawn_workfn,
            (t_task_callback)task_spawn_callback);
    }
        /* just run synchronously */
    return task_start(owner, data, workfn, cb);
}

#else

t_task *task_spawn(t_pd *owner, void *data,
    t_task_workfn workfn, t_task_callback cb)
{
        /* just run synchronously */
    return task_start(owner, data, workfn, cb);
}

#endif /* PD_WORKERTHREADS */

int task_stop(t_task *task, int sync)
{
    t_taskqueue *queue = STUFF->st_taskqueue;
    t_task *t;
#if 1
        /* check if the task is really pending */
    int foundit = 0;
    for (t = queue->tq_pending; t; t = t->t_nextpending)
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
    task->t_cancel = 1;

#if PD_WORKERTHREADS
    if (sync && queue->tq_running)
    {
        t_tasklist *fromsched = &queue->tq_fromsched;
        t_tasklist *tosched = &queue->tq_tosched;
            /* check if the task is still in the 'fromsched' queue;
             * if yes, we don't need to do anything since the worker
             * function will never be called. */
        tasklist_lock(fromsched);
        for (t = fromsched->tl_head; t; t = t->t_next)
        {
            if (t == task)
            {
                tasklist_unlock(fromsched);
                return 0;
            }
        }
        tasklist_unlock(fromsched);
            /* wait until the task is in the 'tosched' queue */
        tasklist_lock(tosched);
        while (1)
        {
            for (t = tosched->tl_head; t; t = t->t_next)
            {
                if (t == task)
                {
                    tasklist_unlock(tosched);
                    return 1;
                }
            }
            tasklist_wait(tosched);
        }
    }
#endif
    return 1;
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
    if (queue->tq_running)
    {
            /* must be decremented both in taskqueue_perform() and task_resume() */
        atomic_add(&task->t_suspend, 2);
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
#ifdef PDINSTANCE
    t_taskqueue *queue = task->t_instance->pd_stuff->st_taskqueue;
#else
    t_taskqueue *queue = STUFF->st_taskqueue;
#endif
    if (queue->tq_running)
    {
        task->t_workfn = workfn;
            /* NB: it is unlikely, but entirely possible, that
             * task_resume() is called before task_suspend() or
             * before the task is handled in taskqueue_perform().
             * In this case we do not want to handle the task yet;
             * instead, it will be handled later in taskqueue_perform(). */
        if (atomic_decrement(&task->t_suspend) == 0) /* fully resumed */
        {
            if (workfn && !task->t_cancel) /* continue */
            {
                t_tasklist *fromsched = &queue->tq_fromsched;
                    /* reschedule task and notify worker thread */
                tasklist_push_sync(fromsched, task, 1);
            }
            else /* completed/cancelled */
            {
                t_tasklist *tosched = &queue->tq_tosched;
                    /* move task back to scheduler and notify the main thread
                     * since it might be waiting in task_stop() */
                tasklist_push_sync(tosched, task, 1);
            }
        }
        return;
    }
#endif /* PD_WORKERTHREADS */
    task->t_suspend = 1;
}

void task_notify(t_task *task, void *data, t_task_callback fn)
{
#ifdef PDINSTANCE
    t_taskqueue *queue = task->t_instance->pd_stuff->st_taskqueue;
#else
    t_taskqueue *queue = STUFF->st_taskqueue;
#endif
    t_message *msg = (t_message *)getbytes(sizeof(t_message));
    msg->m_next = 0;
    msg->m_task = task;
    msg->m_data = data;
    msg->m_fn = fn;
#if PD_WORKERTHREADS
    tasklist_lock(&queue->tq_tosched);
#endif
    messagelist_push(&queue->tq_messages, msg);
#if PD_WORKERTHREADS
    tasklist_unlock(&queue->tq_tosched);
#endif
}

int sys_taskqueue_running(void)
{
#if PD_WORKERTHREADS
    return STUFF->st_taskqueue->tq_running;
#else
    return 0;
#endif
}
