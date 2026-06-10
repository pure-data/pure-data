/* This object shows how to run asynchronous tasks with the task API.
 *
 * The "read" method reads the content of the given file into memory.
 * It outputs the file size on success or -1 on failure.
 *
 * With the "get" method you can get a single byte of the file.
 *
 * The "download" method uses the "curl" command to download a file from
 * the internet to the specified location. It demonstrates how to use
 * task_spawn() to execute a long-running task in its own thread,
 * allowing other tasks to run concurrently.
 *
 * The "progress" method fakes some work and communicates the progress
 * to the user. For this purpose we continuously update a (volatile)
 * member in our object which in turn is polled regularly with a clock.
 * Generally, you should avoid references to shared state, as it can easily
 * introduce data races, but here it is the only way to achieve our goal.
 * Note that we have to call task_stop() with sync=1!
 */

#include "m_pd.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <errno.h>
#endif

#define TASKOBJ_CANCEL 0

static t_class *taskobj_class;

typedef struct _taskobj
{
    t_object x_obj;
    t_outlet *x_statusout;
    t_outlet *x_byteout;
    char *x_data;
    int x_size;
    t_task *x_task;
} t_taskobj;

static void taskobj_cancel(t_taskobj *x);

/* ------------------- taskobj_read --------------------- */

typedef struct _read_data
{
    char filepath[256];
    char *data;
    int size;
    int err;
    float ms;
} t_read_data;

static void taskobj_message(t_taskobj *x, char *msg)
{
    if (x)
        logpost(x, PD_NORMAL, "%s", msg);
    free(msg);
}

    /* For demonstration purposes we will send messages back to Pd.
     * Here the data is just a string, but it can really be anything.
     * Use this whenever you need to send data or notify your object
     * while the worker function is still in progress. */
static void taskobj_send_message(t_task *task, const char *msg)
{
    int size = strlen(msg) + 1;
    char *data = (char *)malloc(size);
    memcpy(data, msg, size);
    task_notify(task, data, (t_task_callback)taskobj_message);
}

    /* this function is executed on a worker thread;
     * do not call any Pd API functions! */
static void taskobj_read_workfn(t_task *task, t_read_data *x)
{
        /* free old file data */
    if (x->data)
        free(x->data);
    x->data = 0;
    x->size = 0;
        /* try to read new file data */
    FILE *fp = fopen(x->filepath, "rb");
    if (fp)
    {
        char msgbuf[MAXPDSTRING];
        char *data;
        size_t size;

        snprintf(msgbuf, sizeof(msgbuf), "successfully opened '%s'", x->filepath);
        taskobj_send_message(task, msgbuf);
            /* get file size */
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
            /* allocate buffer */
        data = malloc(size);
        if (data)
        {
                /* read file into buffer */
            size_t result = fread(data, 1, size, fp);
            if (result == size) /* success */
            {
                const float delta = 100;
                float remaining = x->ms;
                x->data = data;
                x->size = size;
                    /* send another message */
                snprintf(msgbuf, sizeof(msgbuf), "read %d bytes", (int)size);
                taskobj_send_message(task, msgbuf);
                    /* sleep to simulate work; return early if the
                     * task has been cancelled in the meantime */
                while (remaining > 0 && task_check(task))
                {
                #ifdef _WIN32
                    Sleep(delta);
                #else
                    usleep(delta * 1000.0);
                #endif
                    remaining -= delta;
                }
            }
            else /* fail */
            {
                x->err = errno;
                free(data);
            }
        }
            /* close file */
        fclose(fp);
    }
    else /* catch error */
        x->err = errno;
}

    /* this function is called when the task
     * has been completed or cancelled */
static void taskobj_read_callback(t_taskobj *x, t_read_data *y)
{
    if (x) /* completed */
    {
            /* move file data to object */
        x->x_data = y->data;
        x->x_size = y->size;
            /* the task handle is now invalid, so we have to unset it!
             * Let's do this BEFORE sending the message, so the user
             * can call the "read" method recursively. */
        x->x_task = 0;
            /* notify */
        if (x->x_data) /* success */
            outlet_float(x->x_statusout, x->x_size);
        else /* fail */
        {
            pd_error(x, "could not read file '%s': %s (%d)",
                y->filepath, strerror(y->err), y->err);
            outlet_float(x->x_statusout, -1);
        }
    }
    else /* cancelled */
    {
        post("taskobj: task has been cancelled");
            /* free any file data to avoid a memory leak!
             * NB: we could use task_start() to defer resource cleanup
             * to the worker thread, but this is omitted for brevity. */
        if (y->data)
            free(y->data);
    }
        /* finally free the task data object */
    freebytes(y, sizeof(t_read_data));
}

static void taskobj_read(t_taskobj *x, t_symbol *file, t_floatarg ms)
{
    t_read_data *y;
        /* check if a task is still running; typically it does not
         * make sense to run several tasks concurrently */
    if (x->x_task)
    {
    #if TASKOBJ_CANCEL
        taskobj_cancel(x);
    #else
            /* treat as error and return */
        pd_error(x, "taskobj: task still running");
        return;
    #endif
    }
        /* allocate task data */
    y = (t_read_data *)getbytes(sizeof(t_read_data));
    y->err = 0;
    y->ms = ms;
        /* move old file data, to be deleted in the worker thread */
    y->data = x->x_data;
    y->size = x->x_size;
    x->x_data = 0;
    x->x_size = 0;
        /* store file name */
    snprintf(y->filepath, sizeof(y->filepath), "%s", file->s_name);
        /* schedule task and store task handle */
    x->x_task = task_start((t_pd *)x, y, (t_task_workfn)taskobj_read_workfn,
        (t_task_callback)taskobj_read_callback);
}

/* ------------------- taskobj_download --------------------- */

typedef struct _download_data
{
    char url[256];
    char filepath[256];
    int success;
} t_download_data;

static void taskobj_download_workfn(t_task *task, t_download_data *x)
{
    char cmd[MAXPDSTRING];
    snprintf(cmd, sizeof(cmd), "curl %s --output %s", x->url, x->filepath);
        /* we're lazy and just call the 'curl' command with 'system()'.
         * In reality, you should probably use libcurl instead :-) */
    if (system(cmd) == EXIT_SUCCESS)
        x->success = 1;
    else
    {
        fprintf(stderr, "command '%s' failed\n", cmd);
        x->success = 0;
    }
}

static void taskobj_download_callback(t_taskobj *x, t_download_data *y)
{
    if (x) /* completed */
    {
        x->x_task = 0;
        if (!y->success)
            pd_error(x, "could not download %s to %s", y->url, y->filepath);
        outlet_float(x->x_statusout, y->success);
    }
        /* finally free the task data object */
    freebytes(y, sizeof(t_download_data));
}

static void taskobj_download(t_taskobj *x, t_symbol *url, t_symbol *file)
{
    t_download_data *y;
        /* check if a task is still running */
    if (x->x_task)
    {
    #if TASKOBJ_CANCEL
        taskobj_cancel(x);
    #else
            /* treat as error and return */
        pd_error(x, "taskobj: task still running");
        return;
    #endif
    }
    y = (t_download_data *)getbytes(sizeof(t_download_data));
    y->success = 0;
    snprintf(y->url, sizeof(y->url), "%s", url->s_name);
    snprintf(y->filepath, sizeof(y->filepath), "%s", file->s_name);
        /* spawn task as new thread and store task handle */
    x->x_task = task_spawn((t_pd *)x, y, (t_task_workfn)taskobj_download_workfn,
        (t_task_callback)taskobj_download_callback);
}

/* ------------------- taskobj_progress --------------------- */

#define POLL_INTERVAL 30

typedef struct _progress_data
{
    float duration;
    float elapsed;
} t_progress_data;

static void taskobj_progress_notify(t_taskobj *x, void *y)
{
    if (x) /* check if valid! */
    {
        t_float progress = (t_int)y * 0.01;
        outlet_float(x->x_byteout, progress);
    }
        /* nothing to free! */
}

static void taskobj_progress_workfn(t_task *task, t_progress_data *data)
{
    const float delta = 20;
    t_int progress;
loop:
    /* sleep to simulate work */
#ifdef _WIN32
    Sleep(delta);
#else
    usleep(delta * 1000.0);
#endif
    data->elapsed += delta;
    if (data->elapsed < data->duration)
    {
            /* let's communicate the progress to the user. Normally, we would pass
             * a heap-allocated object to task_notify(), but if the data is just a
             * single integer, we can pass it directly as a t_int. */
        progress = data->elapsed / data->duration * 100;
        task_notify(task, (void*)progress, (t_task_callback)taskobj_progress_notify);
            /* yield control to the task system to allow other tasks to run.
             * We will continue with the same work function (unless the task has
             * been cancelled by the user). */
        // goto loop;
        task_yield(task, (t_task_workfn)taskobj_progress_workfn);
    }
        /* if we are finished, just return from the work function */
}

static void taskobj_progress_callback(t_taskobj *x, t_progress_data *data)
{
    if (x) /* completed */
    {
        x->x_task = 0;
        outlet_float(x->x_byteout, 1.);
        outlet_bang(x->x_statusout);
    }
        /* finally free the task data object */
    freebytes(data, sizeof(*data));
}

static void taskobj_progress(t_taskobj *x, t_floatarg ms)
{
    t_progress_data *data;
        /* check if a task is still running */
    if (x->x_task)
    {
    #if TASKOBJ_CANCEL
        taskobj_cancel(x);
    #else
            /* treat as error and return */
        pd_error(x, "taskobj: task still running");
        return;
    #endif
    }
    data = (t_progress_data *)getbytes(sizeof(*data));
    data->duration = ms;
    data->elapsed = 0;
        /* start task and store task handle */
    x->x_task = task_start((t_pd *)x, data, (t_task_workfn)taskobj_progress_workfn,
        (t_task_callback)taskobj_progress_callback);
}

/* --------------------------------------------------------------------- */

static void taskobj_get(t_taskobj *x, t_floatarg f)
{
    if (x->x_data)
    {
        int i = f;
        if (i >= 0 && i < x->x_size)
            outlet_float(x->x_byteout, x->x_data[i]);
        else
            pd_error(x, "index %d out of range!", i);
    }
    else
        pd_error(x, "no file loaded!");
}

static void taskobj_cancel(t_taskobj *x)
{
    if (x->x_task)
    {
        task_stop(x->x_task, 0);
            /* don't forget to unset the task handle! */
        x->x_task = 0;
    }
    else
        pd_error(x, "taskobj: no task");
}

static void *taskobj_new(void)
{
    t_taskobj *x = (t_taskobj *)pd_new(taskobj_class);
    x->x_data = 0;
    x->x_size = 0;
    x->x_task = 0;
    x->x_statusout = outlet_new(&x->x_obj, 0);
    x->x_byteout = outlet_new(&x->x_obj, 0);
    return x;
}

static void taskobj_free(t_taskobj *x)
{
        /* cancel pending task! */
    if (x->x_task)
        taskobj_cancel(x);
        /* cleanup */
    if (x->x_data)
        free(x->x_data);
}

void taskobj_setup(void)
{
    taskobj_class = class_new(gensym("taskobj"),
        (t_newmethod)taskobj_new, (t_method)taskobj_free,
        sizeof(t_taskobj), 0, 0);
    class_addmethod(taskobj_class, (t_method)taskobj_read,
        gensym("read"), A_SYMBOL, A_DEFFLOAT, 0);
    class_addmethod(taskobj_class, (t_method)taskobj_get,
        gensym("get"), A_FLOAT, 0);
    class_addmethod(taskobj_class, (t_method)taskobj_cancel,
        gensym("cancel"), 0);
    class_addmethod(taskobj_class, (t_method)taskobj_download,
        gensym("download"), A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(taskobj_class, (t_method)taskobj_progress,
        gensym("progress"), A_FLOAT, 0);
}
