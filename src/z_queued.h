/*
 * Copyright (c) 2012 Peter Brinkmann (peter.brinkmann@gmail.com)
 * Copyright (c) 2022 libpd team
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

/// set the queued print receiver hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_printhook(const t_libpd_printhook hook);

/// set the queued bang receiver hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_banghook(const t_libpd_banghook hook);

/// set the queued float receiver hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
/// note: you can either have a queued float receiver hook, or a queued
///       double receiver hook (see below), but not both.
///       calling this, will automatically unset the queued double receiver
///       hook
EXTERN void libpd_set_queued_floathook(const t_libpd_floathook hook);

/// set the queued double receiver hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
/// note: you can either have a queued double receiver hook, or a queued
///       float receiver hook (see above), but not both.
///       calling this, will automatically unset the queued float receiver
///       hook
EXTERN void libpd_set_queued_doublehook(const t_libpd_doublehook hook);

/// set the queued symbol receiver hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_symbolhook(const t_libpd_symbolhook hook);

/// set the queued list receiver hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_listhook(const t_libpd_listhook hook);

/// set the queued typed message receiver hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_messagehook(const t_libpd_messagehook hook);

/// set the queued MIDI note on hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_noteonhook(const t_libpd_noteonhook hook);

/// set the queued MIDI control change hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_controlchangehook(const t_libpd_controlchangehook hook);

/// set the queued MIDI program change hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_programchangehook(const t_libpd_programchangehook hook);

/// set the queued MIDI pitch bend hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_pitchbendhook(const t_libpd_pitchbendhook hook);

/// set the queued MIDI after touch hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_aftertouchhook(const t_libpd_aftertouchhook hook);

/// set the queued MIDI poly after touch hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_polyaftertouchhook(const t_libpd_polyaftertouchhook hook);

/// set the queued raw MIDI byte hook, NULL by default
/// note: do not call this before libpd_queued_init() or while DSP is running
EXTERN void libpd_set_queued_midibytehook(const t_libpd_midibytehook hook);

/// initialize libpd and the queued ringbuffers, safe to call more than once
/// returns 0 on success, -1 if libpd was already initialized, or -2 if ring
/// buffer allocation failed
///
/// with a single instance, use in place of libpd_init()
///
/// with multiple instances, call once for each instance *before* setting hooks:
///    t_pdinstance *pd1 = libpd_new_instance();
///    libpd_set_instance(pd1);
///    libpd_queued_init();
///    libpd_set_queued_printhook(pdprint);
///    ...
///
EXTERN int libpd_queued_init(void);

/// free the queued ringbuffers
/// with multiple instances, call before freeing each instance:
///     libpd_set_instance(pd1);
///     libpd_queued_release();
///     libpd_free_instance(pd1);
EXTERN void libpd_queued_release(void);

/// process and dispatch received messages in message ringbuffer
EXTERN void libpd_queued_receive_pd_messages(void);

/// process and dispatch receive midi messages in MIDI message ringbuffer
EXTERN void libpd_queued_receive_midi_messages(void);

#ifdef __cplusplus
}
#endif

#endif
