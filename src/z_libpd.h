/*
 * Copyright (c) 2010 Peter Brinkmann (peter.brinkmann@gmail.com)
 *
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 *
 * See https://github.com/libpd/libpd/wiki for documentation
 *
 */

#ifndef __Z_LIBPD_H__
#define __Z_LIBPD_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "m_pd.h"

EXTERN int libpd_init(void);
EXTERN void libpd_clear_search_path(void);
EXTERN void libpd_add_to_search_path(const char *sym);

EXTERN void *libpd_openfile(const char *basename, const char *dirname);
EXTERN void libpd_closefile(void *p);
EXTERN int libpd_getdollarzero(void *p);

EXTERN int libpd_blocksize(void);
EXTERN int libpd_init_audio(int inChans, int outChans, int sampleRate);
EXTERN int libpd_process_raw(const float *inBuffer, float *outBuffer);
EXTERN int libpd_process_short(const int ticks,
    const short *inBuffer, short *outBuffer);
EXTERN int libpd_process_float(const int ticks,
    const float *inBuffer, float *outBuffer);
EXTERN int libpd_process_double(const int ticks,
    const double *inBuffer, double *outBuffer);

EXTERN int libpd_arraysize(const char *name);
// The parameters of the next two functions are inspired by memcpy.
EXTERN int libpd_read_array(float *dest, const char *src, int offset, int n);
EXTERN int libpd_write_array(const char *dest, int offset, const float *src, int n);

EXTERN int libpd_bang(const char *recv);
EXTERN int libpd_float(const char *recv, float x);
EXTERN int libpd_symbol(const char *recv, const char *sym);

EXTERN void libpd_set_float(t_atom *v, float x);
EXTERN void libpd_set_symbol(t_atom *v, const char *sym);
EXTERN int libpd_list(const char *recv, int argc, t_atom *argv);
EXTERN int libpd_message(const char *recv, const char *msg, int argc, t_atom *argv);

EXTERN int libpd_start_message(int max_length);
EXTERN void libpd_add_float(float x);
EXTERN void libpd_add_symbol(const char *sym);
EXTERN int libpd_finish_list(const char *recv);
EXTERN int libpd_finish_message(const char *recv, const char *msg);

EXTERN int libpd_exists(const char *sym);
EXTERN void *libpd_bind(const char *sym);
EXTERN void libpd_unbind(void *p);

EXTERN int libpd_is_float(t_atom *a);
EXTERN int libpd_is_symbol(t_atom *a);
EXTERN float libpd_get_float(t_atom *a);
EXTERN const char *libpd_get_symbol(t_atom *a);
EXTERN t_atom *libpd_next_atom(t_atom *a);

typedef void (*t_libpd_printhook)(const char *recv);
typedef void (*t_libpd_banghook)(const char *recv);
typedef void (*t_libpd_floathook)(const char *recv, float x);
typedef void (*t_libpd_symbolhook)(const char *recv, const char *sym);
typedef void (*t_libpd_listhook)(const char *recv, int argc, t_atom *argv);
typedef void (*t_libpd_messagehook)(const char *recv, const char *msg,
    int argc, t_atom *argv);

EXTERN void libpd_set_printhook(const t_libpd_printhook hook);
EXTERN void libpd_set_banghook(const t_libpd_banghook hook);
EXTERN void libpd_set_floathook(const t_libpd_floathook hook);
EXTERN void libpd_set_symbolhook(const t_libpd_symbolhook hook);
EXTERN void libpd_set_listhook(const t_libpd_listhook hook);
EXTERN void libpd_set_messagehook(const t_libpd_messagehook hook);

EXTERN int libpd_noteon(int channel, int pitch, int velocity);
EXTERN int libpd_controlchange(int channel, int controller, int value);
EXTERN int libpd_programchange(int channel, int value);
EXTERN int libpd_pitchbend(int channel, int value);
EXTERN int libpd_aftertouch(int channel, int value);
EXTERN int libpd_polyaftertouch(int channel, int pitch, int value);
EXTERN int libpd_midibyte(int port, int byte);
EXTERN int libpd_sysex(int port, int byte);
EXTERN int libpd_sysrealtime(int port, int byte);

typedef void (*t_libpd_noteonhook)(int channel, int pitch, int velocity);
typedef void (*t_libpd_controlchangehook)(int channel,
    int controller, int value);
typedef void (*t_libpd_programchangehook)(int channel, int value);
typedef void (*t_libpd_pitchbendhook)(int channel, int value);
typedef void (*t_libpd_aftertouchhook)(int channel, int value);
typedef void (*t_libpd_polyaftertouchhook)(int channel, int pitch, int value);
typedef void (*t_libpd_midibytehook)(int port, int byte);

EXTERN void libpd_set_noteonhook(const t_libpd_noteonhook hook);
EXTERN void libpd_set_controlchangehook(const t_libpd_controlchangehook hook);
EXTERN void libpd_set_programchangehook(const t_libpd_programchangehook hook);
EXTERN void libpd_set_pitchbendhook(const t_libpd_pitchbendhook hook);
EXTERN void libpd_set_aftertouchhook(const t_libpd_aftertouchhook hook);
EXTERN void libpd_set_polyaftertouchhook(const t_libpd_polyaftertouchhook hook);
EXTERN void libpd_set_midibytehook(const t_libpd_midibytehook hook);

/// \section GUI

/// open the current patches within a Pd vanilla GUI
/// requires the path to Pd's main folder that contains bin/, tcl/, etc
/// for a macOS .app bundle: /path/to/Pd-#.#-#.app/Contents/Resources
/// returns 0 on success
EXTERN int libpd_start_gui(char *path);

/// stop the Pd vanilla GUI
EXTERN void libpd_stop_gui(void);

/// manually update and handle any GUI messages
/// this is called automatically when using a libpd_process* function,
/// also facilitates network message processing, etc so it can be useful to
/// call repeatedly when idle for more throughput
EXTERN void libpd_poll_gui(void);

/// \section Multiple Instances

/// create a new pd instance
/// returns 0 when libpd is not compiled with PDINSTANCE
EXTERN t_pdinstance *libpd_new_instance(void);

/// set the current pd instance,
/// subsequent libpd calls will affect this instance only
/// does nothing when libpd is not compiled with PDINSTANCE
EXTERN void libpd_set_instance(t_pdinstance *x);

/// free a pd instance
/// does nothing when libpd is not compiled with PDINSTANCE
EXTERN void libpd_free_instance(t_pdinstance *x);

/// get the current pd instance
EXTERN t_pdinstance *libpd_this_instance(void);

/// get a pd instance by index
/// returns 0 if index is out of bounds
/// returns "this" instance when libpd is not compiled with PDINSTANCE
EXTERN t_pdinstance *libpd_get_instance(int index);

/// get the number of pd instances
/// returns 1 when libpd is not compiled with PDINSTANCE
EXTERN int libpd_num_instances(void);

/// \section Log Level

/// set verbose print state: 0 or 1
EXTERN void libpd_set_verbose(int verbose);

/// get the verbose print state: 0 or 1
EXTERN int libpd_get_verbose(void);

#ifdef __cplusplus
}
#endif

#endif
