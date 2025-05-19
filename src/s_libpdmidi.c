/* Copyright (c) 1997-2010 Miller Puckette and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include "z_libpd.h"
#include "z_hooks.h"

#define CLAMP(x, low, high) ((x > high) ? high : ((x < low) ? low : x))
#define CLAMP4BIT(x) CLAMP(x, 0, 0x0f)
#define CLAMP7BIT(x) CLAMP(x, 0, 0x7f)
#define CLAMP8BIT(x) CLAMP(x, 0, 0xff)
#define CLAMP12BIT(x) CLAMP(x, 0, 0x0fff)
#define CLAMP14BIT(x) CLAMP(x, 0, 0x3fff)

#define CHANNEL ((CLAMP12BIT(port) << 4) | CLAMP4BIT(channel))

void outmidi_noteon(int port, int channel, int pitch, int velo) {
  t_libpdimp *imp = LIBPDSTUFF;
  if (imp && imp->i_hooks.h_noteonhook)
    imp->i_hooks.h_noteonhook(CHANNEL, CLAMP7BIT(pitch), CLAMP7BIT(velo));
}

void outmidi_controlchange(int port, int channel, int ctl, int value) {
  t_libpdimp *imp = LIBPDSTUFF;
  if (imp && imp->i_hooks.h_controlchangehook)
    imp->i_hooks.h_controlchangehook(CHANNEL, CLAMP7BIT(ctl), CLAMP7BIT(value));
}

void outmidi_programchange(int port, int channel, int value) {
  t_libpdimp *imp = LIBPDSTUFF;
  if (imp && imp->i_hooks.h_programchangehook)
    imp->i_hooks.h_programchangehook(CHANNEL, CLAMP7BIT(value));
}

void outmidi_pitchbend(int port, int channel, int value) {
  t_libpdimp *imp = LIBPDSTUFF;
  if (imp && imp->i_hooks.h_pitchbendhook)
    imp->i_hooks.h_pitchbendhook(CHANNEL, CLAMP14BIT(value) - 8192); // remove offset
}

void outmidi_aftertouch(int port, int channel, int value) {
  t_libpdimp *imp = LIBPDSTUFF;
  if (imp && imp->i_hooks.h_aftertouchhook)
    imp->i_hooks.h_aftertouchhook(CHANNEL, CLAMP7BIT(value));
}

void outmidi_polyaftertouch(int port, int channel, int pitch, int value) {
  t_libpdimp *imp = LIBPDSTUFF;
  if (imp && imp->i_hooks.h_polyaftertouchhook)
    imp->i_hooks.h_polyaftertouchhook(CHANNEL, CLAMP7BIT(pitch), CLAMP7BIT(value));
}

void outmidi_byte(int port, int value) {
  t_libpdimp *imp = LIBPDSTUFF;
  if (imp && imp->i_hooks.h_midibytehook)
    imp->i_hooks.h_midibytehook(CLAMP12BIT(port), CLAMP8BIT(value));
}

/* tell Pd GUI that our list of MIDI APIs is empty */
#include <string.h>
void sys_get_midi_apis(char *buf) {strcpy(buf, "{}");}

// the rest is not relevant to libpd
void sys_listmididevs(void) {}
void sys_get_midi_params(int *pnmidiindev, int *pmidiindev,
    int *pnmidioutdev, int *pmidioutdev) {*pnmidiindev = *pnmidioutdev = 0;}
void sys_open_midi(int nmidiindev, int *midiindev,
    int nmidioutdev, int *midioutdev, int enable) {}
void sys_close_midi() {}
void sys_reopen_midi(void) {}
void sys_initmidiqueue(void) {}
void sys_pollmidiqueue(void) {}
void sys_setmiditimediff(double inbuftime, double outbuftime) {}
void glob_midi_setapi(void *dummy, t_floatarg f) {}
void glob_midi_properties(t_pd *dummy, t_floatarg flongform) {}
void glob_midi_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv) {}
int sys_mididevnametonumber(int output, const char *name) { return 0; }
void sys_mididevnumbertoname(int output, int devno, char *name, int namesize) {}
void sys_set_midi_api(int api) {}
void sys_gui_midipreferences(void) {}
int sys_midiapi;
