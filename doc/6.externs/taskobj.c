/* This object shows how to run asynchronous tasks with the task API.
 *
 * The "read" method reads the content of the given file into memory.
 * It outputs the file size on success or -1 on failure.
 *
 * With the "get" method you can get a single byte of the file.
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
        char *data;
        size_t size;
        // get file size
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        // allocate buffer
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
             * NB: we could use task_sched() to defer resource cleanup
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
    x->x_task = task_sched((t_pd *)x, y, (t_task_workfn)taskobj_read_workfn,
        (t_task_callback)taskobj_read_callback);
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
            /* NB: our task data does not contain any references
             * to our Pd object, so we don't have to synchronize! */
        task_cancel(x->x_task, 0);
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
}
