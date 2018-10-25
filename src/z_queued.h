/*
 * Copyright (c) 2012 Peter Brinkmann (peter.brinkmann@gmail.com)
 *
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 *
 * See https://github.com/libpd/libpd/wiki for documentation
 *
 */

#ifndef __Z_QUEUED_H__
#define __Z_QUEUED_H__

#include "z_libpd.h"

#ifdef __cplusplus
extern "C"
{
#endif

EXTERN void libpd_set_queued_printhook(const t_libpd_printhook hook);
EXTERN void libpd_set_queued_banghook(const t_libpd_banghook hook);
EXTERN void libpd_set_queued_floathook(const t_libpd_floathook hook);
EXTERN void libpd_set_queued_symbolhook(const t_libpd_symbolhook hook);
EXTERN void libpd_set_queued_listhook(const t_libpd_listhook hook);
EXTERN void libpd_set_queued_messagehook(const t_libpd_messagehook hook);

EXTERN void libpd_set_queued_noteonhook(const t_libpd_noteonhook hook);
EXTERN void libpd_set_queued_controlchangehook(const t_libpd_controlchangehook hook);
EXTERN void libpd_set_queued_programchangehook(const t_libpd_programchangehook hook);
EXTERN void libpd_set_queued_pitchbendhook(const t_libpd_pitchbendhook hook);
EXTERN void libpd_set_queued_aftertouchhook(const t_libpd_aftertouchhook hook);
EXTERN void libpd_set_queued_polyaftertouchhook(const t_libpd_polyaftertouchhook hook);
EXTERN void libpd_set_queued_midibytehook(const t_libpd_midibytehook hook);

EXTERN int libpd_queued_init();
EXTERN void libpd_queued_release();
EXTERN void libpd_queued_receive_pd_messages();
EXTERN void libpd_queued_receive_midi_messages();

#ifdef __cplusplus
}
#endif

#endif
