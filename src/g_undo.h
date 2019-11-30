
#ifndef __g_undo_h_
#define __g_undo_h_

/*
Infinite undo by Ivica Ico Bukvic <ico@vt.edu> Dec. 2011
Modified for Pd-vanilla by IOhannes m zmölnig <zmoelnig@iem.at> Jun. 2018

This is the home of infinite undo queue. Each root canvas has one of these.
Only canvas that is root will instantiate the pointer to t_undo_action struct.
All sub-canvases (including abstractions) will be linked to the root window.
Once the root window is destroyed, so is its undo queue. Each canvas (t_glist)
defined in g_canvas.h now also has information on t_undo_action queue and a
pointer to the last item in the doubly-linked list (queue).

First initialized undo is never filled (it is of a type 0). First (new) action
creates another t_undo_action and saves its contents there and updates "last"
pointer inside the t_canvas (t_glist).

t_undo_action requires canvas information in case we've deleted a canvas and
then undoed its deletion which will in effect change its pointer (memory
location). Then we need to call check_canvas_pointers to update all of the old
(stale) undo actions to corresepond with the new memory location.

What about abstractions? Once they are recreated  (e.g. after delete, followed
by an undo) all undo actions (except for its deletion in the parent window should
be purged since abstraction's state will now default to its original (saved) state.

Types of undo data:
0  - init data (start of the queue)
1  - connect
2  - disconnect
3  - cut, clear & typing into objects
4  - motion, inculding "tidy up" and stretching
5  - paste & duplicate
6  - apply
7  - arrange (to front/back)
8  - canvas apply
9  - create
10 - recreate
*/

#include "m_pd.h"

typedef enum
{
    UNDO_INIT = 0,
    UNDO_CONNECT,      /* 1: OK */
    UNDO_DISCONNECT,   /* 2: OK */
    UNDO_CUT,          /* 3: OK */
    UNDO_MOTION,       /* 4: OK */
    UNDO_PASTE,        /* 5: OK */
    UNDO_APPLY,        /* 6: how to test? */
    UNDO_ARRANGE,      /* 7: FIXME: skipped */
    UNDO_CANVAS_APPLY, /* 8: OK */
    UNDO_CREATE,       /* 9: OK */
    UNDO_RECREATE,     /* 10: OK */
    UNDO_FONT,         /* 11: OK */
    UNDO_SEQUENCE_START, /* 12 start an atomic sequence of undo actions*/
    UNDO_SEQUENCE_END,   /* 13 end an atomic sequence of undo actions */

    UNDO_LAST
} t_undo_type;

struct _undo_action
{
	t_canvas *x;				/* canvas undo is associated with */
	t_undo_type type;			/* defines what kind of data container it is */
	void *data;					/* each action will have a different data container */
	char *name;					/* name of current action */
	struct _undo_action *prev;	/* previous undo action */
	struct _undo_action *next;	/* next undo action */
};

#ifndef t_undo_action
#define t_undo_action struct _undo_action
#endif

struct _undo
{
    t_undo_action *u_queue;
    t_undo_action *u_last;
    void *u_cleanstate; /* pointer to non-dirty state */
    int u_doing; /* currently undoing */
};
#define t_undo struct _undo


EXTERN t_undo*canvas_undo_get(t_canvas*x);

EXTERN void canvas_undo_cleardirty(t_canvas *x);

EXTERN t_undo_action *canvas_undo_init(t_canvas *x);
EXTERN t_undo_action *canvas_undo_add(t_canvas *x,
	t_undo_type type, const char *name, void *data);
EXTERN void canvas_undo_undo(t_canvas *x);
EXTERN void canvas_undo_redo(t_canvas *x);
EXTERN void canvas_undo_rebranch(t_canvas *x);
EXTERN void canvas_undo_check_canvas_pointers(t_canvas *x);
EXTERN void canvas_undo_purge_abstraction_actions(t_canvas *x);
EXTERN void canvas_undo_free(t_canvas *x);

/* --------- 1. connect ---------- */

EXTERN void *canvas_undo_set_connect(t_canvas *x,
    int index1, int outno, int index2, int inno);
EXTERN int canvas_undo_connect(t_canvas *x, void *z, int action);

/* --------- 2. disconnect ------- */

EXTERN void *canvas_undo_set_disconnect(t_canvas *x,
    int index1, int outno, int index2, int inno);
EXTERN int canvas_undo_disconnect(t_canvas *x, void *z, int action);

/* --------- 3. cut -------------- */

EXTERN void *canvas_undo_set_cut(t_canvas *x,
    int mode);
EXTERN int canvas_undo_cut(t_canvas *x, void *z, int action);

/* --------- 4. move ------------- */

EXTERN void *canvas_undo_set_move(t_canvas *x, int selected);
EXTERN int canvas_undo_move(t_canvas *x, void *z, int action);

/* --------- 5. paste ------------ */

EXTERN void *canvas_undo_set_paste(t_canvas *x,
    int offset, int duplicate, int d_offset);
EXTERN int canvas_undo_paste(t_canvas *x, void *z, int action);

/* --------- 6. apply ------------ */

EXTERN void *canvas_undo_set_apply(t_canvas *x,
    int n);
EXTERN int canvas_undo_apply(t_canvas *x, void *z, int action);

/* --------- 7. arrange (currently unused) ---------- */

EXTERN void *canvas_undo_set_arrange(t_canvas *x,
    t_gobj *obj, int newindex);
EXTERN int canvas_undo_arrange(t_canvas *x, void *z, int action);

/* --------- 8. canvas apply (currently unused) ----- */

EXTERN void *canvas_undo_set_canvas(t_canvas *x);
EXTERN int canvas_undo_canvas_apply(t_canvas *x, void *z, int action);

/* --------- 9. create ----------- */

EXTERN void *canvas_undo_set_create(t_canvas *x);
EXTERN int canvas_undo_create(t_canvas *x, void *z, int action);

/* --------- 10. recreate -------- */

EXTERN void *canvas_undo_set_recreate(t_canvas *x,
    t_gobj *y, int old_pos);
EXTERN int canvas_undo_recreate(t_canvas *x, void *z, int action);

/* --------- 11. font (currently unused) ------------ */

EXTERN void *canvas_undo_set_font(t_canvas *x,
    int font, t_float realresize, int whichresize);
EXTERN int canvas_undo_font(t_canvas *x, void *z, int action);

/* ------------------------------- */

#endif /* __g_undo_h_ */
