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

/// set the queued print receiver hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_printhook(const t_libpd_printhook hook);

/// set the queued bang receiver hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_banghook(const t_libpd_banghook hook);

/// set the queued float receiver hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_floathook(const t_libpd_floathook hook);

/// set the queued symbol receiver hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_symbolhook(const t_libpd_symbolhook hook);

/// set the queued list receiver hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_listhook(const t_libpd_listhook hook);

/// set the queued typed message receiver hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_messagehook(const t_libpd_messagehook hook);

/// set the queued MIDI note on hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_noteonhook(const t_libpd_noteonhook hook);

/// set the queued MIDI control change hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_controlchangehook(const t_libpd_controlchangehook hook);

/// set the queued MIDI program change hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_programchangehook(const t_libpd_programchangehook hook);

/// set the queued MIDI pitch bend hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_pitchbendhook(const t_libpd_pitchbendhook hook);

/// set the queued MIDI after touch hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_aftertouchhook(const t_libpd_aftertouchhook hook);

/// set the queued MIDI poly after touch hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_polyaftertouchhook(const t_libpd_polyaftertouchhook hook);

/// set the queued raw MIDI byte hook, NULL by default
/// note: do not call this while DSP is running
EXTERN void libpd_set_queued_midibytehook(const t_libpd_midibytehook hook);

/// initialize libpd and the queued ringbuffers, use in place of libpd_init()
/// this is safe to call more than once
/// returns 0 on success, -1 if libpd was already initialized, or -2 if ring
/// buffer allocation failed
EXTERN int libpd_queued_init();

/// free the queued ringbuffers
EXTERN void libpd_queued_release();

/// process and dispatch received messages in message ringbuffer
EXTERN void libpd_queued_receive_pd_messages();

/// process and dispatch receive midi messages in MIDI message ringbuffer
EXTERN void libpd_queued_receive_midi_messages();

#ifdef __cplusplus
}
#endif

#endif
