/*
 * Copyright (c) 2013 Dan Wilcox (danomatika@gmail.com)
 * Copyright (c) 2013-2021 libpd team
 *
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 *
 * See https://github.com/libpd/libpd/wiki for documentation
 *
 */

#ifndef __Z_HOOKS_H__
#define __Z_HOOKS_H__

#include "z_libpd.h"
#include "s_stuff.h"

// internal hooks, etc
// do *not* include this file in a user-facing header

/* hooks */

typedef struct _libpdhooks {

  // messages
  // no h_printhook as libpd_set_printhook() sets internal STUFF->st_printhook
  t_libpd_banghook h_banghook;
  t_libpd_floathook h_floathook;
  t_libpd_doublehook h_doublehook;
  t_libpd_symbolhook h_symbolhook;
  t_libpd_listhook h_listhook;
  t_libpd_messagehook h_messagehook;

  // MIDI
  t_libpd_noteonhook h_noteonhook;
  t_libpd_controlchangehook h_controlchangehook;
  t_libpd_programchangehook h_programchangehook;
  t_libpd_pitchbendhook h_pitchbendhook;
  t_libpd_aftertouchhook h_aftertouchhook;
  t_libpd_polyaftertouchhook h_polyaftertouchhook;
  t_libpd_midibytehook h_midibytehook;
} t_libpdhooks;

/* instance */

/// libpd per-instance implementation data
typedef struct _libpdimp {
  t_libpdhooks i_hooks; /* event hooks */
  void *i_queued;       /* queued data, default NULL */
  void *i_print_util;   /* print util data, default NULL */
  void *i_data;         /* user data, default NULL */
  t_libpd_freehook i_queued_freehook; /* i_queued free, default NULL */
  t_libpd_freehook i_data_freehook;   /* i_data free, default NULL */
} t_libpdimp;

/// main instance implementation data, always valid
extern t_libpdimp libpd_mainimp;

/// alloc new instance implementation data
t_libpdimp* libpdimp_new(void);

/// free instance implementation data
/// does nothing if imp is libpd_mainimp
void libpdimp_free(t_libpdimp *imp);

/// get current instance implementation data
#ifdef PDINSTANCE
  #define LIBPDSTUFF ((t_libpdimp *)(STUFF->st_impdata))
#else
  #define LIBPDSTUFF ((t_libpdimp *)&libpd_mainimp)
#endif

#endif
