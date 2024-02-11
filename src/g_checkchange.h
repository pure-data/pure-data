
#ifndef __g_checkchange_h_
#define __g_checkchange_h_

/*
    checkchange is privately used by the editor. It asks the user to accept
    to discard every changes in a set of selected canvases before calling the
    the configured callback (which will probably delete all these canvases).
*/

#include "m_pd.h"
#include "g_canvas.h"
#include <stdarg.h>

#define CHECKCHANGE_CB_MAXARGS 6
typedef void (*checkchange_cb)(void **c_args);

typedef struct _checkchange
{
    checkchange_cb c_cb;                    /* the callback */
    void *c_args[CHECKCHANGE_CB_MAXARGS];   /* its arguments, converted to (void*) */
    t_selection *c_selection;               /* the selected dirty canvases */
} t_checkchange;

    /* Initial setup, called by g_editor_setup() */
void checkchange_setup(void);

    /* Initialize a call of the checkchange dialog. The first argument cb is the callback
       to call if no modified abstraction is contained by the tested canvas, or if the user
       accepts to discard any change. The following arguments will be given to the callback,
       and should be casted to (void*) before (and casted back by the callback). */
void checkchange_init(checkchange_cb cb, ...);

    /* Select this canvas if dirty, plus every dirty canvases inside of it */
void checkchange_find_dirty(t_canvas *x);

    /* Select dirty canvases in the canvas's selection */
void checkchange_find_dirty_in_selection(t_canvas *x);

    /* For every selected dirty canvas, ask the user to accept the loss of modifications */
void checkchange_start(void);

#endif /* __g_checkchange_h_ */
