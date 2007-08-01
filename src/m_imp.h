/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* This file contains function prototypes and data types used to implement
Pd, but not shared with Pd objects. */

/* NOTE: this file describes Pd implementation details which may change
in future releases.  The public (stable) API is in m_pd.h. */  

/* LATER consider whether to use 'char' for method arg types to save space */
#ifndef __m_imp_h_

/* the structure for a method handler ala Max */
typedef struct _methodentry
{
    t_symbol *me_name;
    t_gotfn me_fun;
    t_atomtype me_arg[MAXPDARG+1];
} t_methodentry;

EXTERN_STRUCT _widgetbehavior;

typedef void (*t_bangmethod)(t_pd *x);
typedef void (*t_pointermethod)(t_pd *x, t_gpointer *gp);
typedef void (*t_floatmethod)(t_pd *x, t_float f);
typedef void (*t_symbolmethod)(t_pd *x, t_symbol *s);
typedef void (*t_listmethod)(t_pd *x, t_symbol *s, int argc, t_atom *argv);
typedef void (*t_anymethod)(t_pd *x, t_symbol *s, int argc, t_atom *argv);

struct _class
{
    t_symbol *c_name;                   /* name (mostly for error reporting) */
    t_symbol *c_helpname;               /* name of help file */
    t_symbol *c_externdir;              /* directory extern was loaded from */
    size_t c_size;                      /* size of an instance */
    t_methodentry *c_methods;           /* methods other than bang, etc below */
    int c_nmethod;                      /* number of methods */
    t_method c_freemethod;              /* function to call before freeing */
    t_bangmethod c_bangmethod;          /* common methods */
    t_pointermethod c_pointermethod;
    t_floatmethod c_floatmethod;
    t_symbolmethod c_symbolmethod;
    t_listmethod c_listmethod;
    t_anymethod c_anymethod;
    struct _widgetbehavior *c_wb;       /* "gobjs" only */
    struct _parentwidgetbehavior *c_pwb;/* widget behavior in parent */
    t_savefn c_savefn;                  /* function to call when saving */
    t_propertiesfn c_propertiesfn;      /* function to start prop dialog */
    int c_floatsignalin;                /* onset to float for signal input */
    char c_gobj;                        /* true if is a gobj */
    char c_patchable;                   /* true if we have a t_object header */
    char c_firstin;                 /* if patchable, true if draw first inlet */
    char c_drawcommand;             /* a drawing command for a template */
};


/* m_obj.c */
EXTERN int obj_noutlets(t_object *x);
EXTERN int obj_ninlets(t_object *x);
EXTERN t_outconnect *obj_starttraverseoutlet(t_object *x, t_outlet **op,
    int nout);
EXTERN t_outconnect *obj_nexttraverseoutlet(t_outconnect *lastconnect,
    t_object **destp, t_inlet **inletp, int *whichp);
EXTERN t_outconnect *obj_connect(t_object *source, int outno,
    t_object *sink, int inno);
EXTERN void obj_disconnect(t_object *source, int outno, t_object *sink,
    int inno);
EXTERN void outlet_setstacklim(void);
EXTERN int obj_issignalinlet(t_object *x, int m);
EXTERN int obj_issignaloutlet(t_object *x, int m);
EXTERN int obj_nsiginlets(t_object *x);
EXTERN int obj_nsigoutlets(t_object *x);
EXTERN int obj_siginletindex(t_object *x, int m);
EXTERN int obj_sigoutletindex(t_object *x, int m);

/* misc */
EXTERN void glob_evalfile(t_pd *ignore, t_symbol *name, t_symbol *dir);
EXTERN void glob_initfromgui(void *dummy, t_symbol *s, int argc, t_atom *argv);
EXTERN void glob_quit(void *dummy);
#define __m_imp_h_
#endif /* __m_imp_h_ */
