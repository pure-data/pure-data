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

#include "z_queued.h"

#include <stdlib.h>
#include <string.h>

#include "z_hooks.h"
#include "z_ringbuffer.h"

#define BUFFER_SIZE 16384

typedef struct _queued_stuff {
  t_libpdhooks hooks;
  t_libpd_printhook printhook;
  ring_buffer *pd_receive_buffer;
  ring_buffer *midi_receive_buffer;
  char temp_buffer[BUFFER_SIZE];
} queued_stuff;

#define QUEUEDSTUFF ((queued_stuff *)(LIBPDSTUFF->i_queued))

typedef struct _pd_params {
  enum {
    LIBPD_PRINT, LIBPD_BANG, LIBPD_FLOAT,
    LIBPD_SYMBOL, LIBPD_LIST, LIBPD_MESSAGE,
  } type;
  const char *src;
  t_float x;
  const char *sym;
  int argc;
} pd_params;

typedef struct _midi_params {
  enum {
    LIBPD_NOTEON, LIBPD_CONTROLCHANGE, LIBPD_PROGRAMCHANGE, LIBPD_PITCHBEND,
    LIBPD_AFTERTOUCH, LIBPD_POLYAFTERTOUCH, LIBPD_MIDIBYTE
  } type;
  int midi1;
  int midi2;
  int midi3;
} midi_params;

#define S_PD_PARAMS sizeof(pd_params)
#define S_MIDI_PARAMS sizeof(midi_params)
#define S_ATOM sizeof(t_atom)

static void receive_print(pd_params *p, char **buffer) {
  if (QUEUEDSTUFF->printhook) {
    QUEUEDSTUFF->printhook(*buffer);
  }
  *buffer += p->argc;
}

static void receive_bang(pd_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_banghook) {
    QUEUEDSTUFF->hooks.h_banghook(p->src);
  }
}

static void receive_float(pd_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_floathook) {
    QUEUEDSTUFF->hooks.h_floathook(p->src, (float)p->x);
  }
  else if (QUEUEDSTUFF->hooks.h_doublehook) {
    QUEUEDSTUFF->hooks.h_doublehook(p->src, (double)p->x);
  }
}

static void receive_symbol(pd_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_symbolhook) {
    QUEUEDSTUFF->hooks.h_symbolhook(p->src, p->sym);
  }
}

static void receive_list(pd_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_listhook) {
    QUEUEDSTUFF->hooks.h_listhook(p->src, p->argc, (t_atom *) *buffer);
  }
  *buffer += p->argc * S_ATOM;
}

static void receive_message(pd_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_messagehook) {
    QUEUEDSTUFF->hooks.h_messagehook(p->src, p->sym, p->argc, (t_atom *) *buffer);
  }
  *buffer += p->argc * S_ATOM;
}

#define LIBPD_WORD_ALIGN 8

static void internal_printhook(const char *s) {
  static char padding[LIBPD_WORD_ALIGN];
  queued_stuff *queued = QUEUEDSTUFF;
  int len = (int) strlen(s) + 1; // remember terminating null char
  int rest = len % LIBPD_WORD_ALIGN;
  if (rest) rest = LIBPD_WORD_ALIGN - rest;
  int total = len + rest;
  if (rb_available_to_write(queued->pd_receive_buffer) >= S_PD_PARAMS + total) {
    pd_params p = {LIBPD_PRINT, NULL, 0.0f, NULL, total};
    rb_write_to_buffer(queued->pd_receive_buffer, 3,
        (const char *)&p, S_PD_PARAMS, s, len, padding, rest);
  }
}

static void internal_banghook(const char *src) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->pd_receive_buffer) >= S_PD_PARAMS) {
    pd_params p = {LIBPD_BANG, src, 0.0f, NULL, 0};
    rb_write_to_buffer(queued->pd_receive_buffer, 1, (const char *)&p, S_PD_PARAMS);
  }
}

static void internal_floathook(const char *src, float x) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->pd_receive_buffer) >= S_PD_PARAMS) {
    pd_params p = {LIBPD_FLOAT, src, x, NULL, 0};
    rb_write_to_buffer(queued->pd_receive_buffer, 1, (const char *)&p, S_PD_PARAMS);
  }
}

static void internal_doublehook(const char *src, double x) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->pd_receive_buffer) >= S_PD_PARAMS) {
    pd_params p = {LIBPD_FLOAT, src, (t_float)x, NULL, 0};
    rb_write_to_buffer(queued->pd_receive_buffer, 1, (const char *)&p, S_PD_PARAMS);
  }
}

static void internal_symbolhook(const char *src, const char *sym) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->pd_receive_buffer) >= S_PD_PARAMS) {
    pd_params p = {LIBPD_SYMBOL, src, 0.0f, sym, 0};
    rb_write_to_buffer(queued->pd_receive_buffer, 1, (const char *)&p, S_PD_PARAMS);
  }
}

static void internal_listhook(const char *src, int argc, t_atom *argv) {
  queued_stuff *queued = QUEUEDSTUFF;
  int n = argc * S_ATOM;
  if (rb_available_to_write(queued->pd_receive_buffer) >= S_PD_PARAMS + n) {
    pd_params p = {LIBPD_LIST, src, 0.0f, NULL, argc};
    rb_write_to_buffer(queued->pd_receive_buffer, 2,
        (const char *)&p, S_PD_PARAMS, (const char *)argv, n);
  }
}

static void internal_messagehook(const char *src, const char* sym,
  int argc, t_atom *argv) {
  queued_stuff *queued = QUEUEDSTUFF;
  int n = argc * S_ATOM;
  if (rb_available_to_write(queued->pd_receive_buffer) >= S_PD_PARAMS + n) {
    pd_params p = {LIBPD_MESSAGE, src, 0.0f, sym, argc};
    rb_write_to_buffer(queued->pd_receive_buffer, 2,
        (const char *)&p, S_PD_PARAMS, (const char *)argv, n);
  }
}

static void receive_noteon(midi_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_noteonhook) {
    QUEUEDSTUFF->hooks.h_noteonhook(p->midi1, p->midi2, p->midi3);
  }
}

static void receive_controlchange(midi_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_controlchangehook) {
    QUEUEDSTUFF->hooks.h_controlchangehook(p->midi1, p->midi2, p->midi3);
  }
}

static void receive_programchange(midi_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_programchangehook) {
    QUEUEDSTUFF->hooks.h_programchangehook(p->midi1, p->midi2);
  }
}

static void receive_pitchbend(midi_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_pitchbendhook) {
    QUEUEDSTUFF->hooks.h_pitchbendhook(p->midi1, p->midi2);
  }
}

static void receive_aftertouch(midi_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_aftertouchhook) {
    QUEUEDSTUFF->hooks.h_aftertouchhook(p->midi1, p->midi2);
  }
}

static void receive_polyaftertouch(midi_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_polyaftertouchhook) {
    QUEUEDSTUFF->hooks.h_polyaftertouchhook(p->midi1, p->midi2, p->midi3);
  }
}

static void receive_midibyte(midi_params *p, char **buffer) {
  if (QUEUEDSTUFF->hooks.h_midibytehook) {
    QUEUEDSTUFF->hooks.h_midibytehook(p->midi1, p->midi2);
  }
}

static void internal_noteonhook(int channel, int pitch, int velocity) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_NOTEON, channel, pitch, velocity};
    rb_write_to_buffer(queued->midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_controlchangehook(int channel, int controller, int value) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_CONTROLCHANGE, channel, controller, value};
    rb_write_to_buffer(queued->midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_programchangehook(int channel, int value) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_PROGRAMCHANGE, channel, value, 0};
    rb_write_to_buffer(queued->midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_pitchbendhook(int channel, int value) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_PITCHBEND, channel, value, 0};
    rb_write_to_buffer(queued->midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_aftertouchhook(int channel, int value) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_AFTERTOUCH, channel, value, 0};
    rb_write_to_buffer(queued->midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_polyaftertouchhook(int channel, int pitch, int value) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_POLYAFTERTOUCH, channel, pitch, value};
    rb_write_to_buffer(queued->midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_midibytehook(int port, int byte) {
  queued_stuff *queued = QUEUEDSTUFF;
  if (rb_available_to_write(queued->midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_MIDIBYTE, port, byte, 0};
    rb_write_to_buffer(queued->midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

void libpd_set_queued_printhook(const t_libpd_printhook hook) {
  QUEUEDSTUFF->printhook = hook;
}

void libpd_set_queued_banghook(const t_libpd_banghook hook) {
  QUEUEDSTUFF->hooks.h_banghook = hook;
}

void libpd_set_queued_floathook(const t_libpd_floathook hook) {
  QUEUEDSTUFF->hooks.h_floathook = hook;
  QUEUEDSTUFF->hooks.h_doublehook = NULL;
}

void libpd_set_queued_doublehook(const t_libpd_doublehook hook) {
  QUEUEDSTUFF->hooks.h_floathook = NULL;
  QUEUEDSTUFF->hooks.h_doublehook = hook;
}

void libpd_set_queued_symbolhook(const t_libpd_symbolhook hook) {
  QUEUEDSTUFF->hooks.h_symbolhook = hook;
}

void libpd_set_queued_listhook(const t_libpd_listhook hook) {
  QUEUEDSTUFF->hooks.h_listhook = hook;
}

void libpd_set_queued_messagehook(const t_libpd_messagehook hook) {
  QUEUEDSTUFF->hooks.h_messagehook = hook;
}

void libpd_set_queued_noteonhook(const t_libpd_noteonhook hook) {
  QUEUEDSTUFF->hooks.h_noteonhook = hook;
}

void libpd_set_queued_controlchangehook(const t_libpd_controlchangehook hook) {
  QUEUEDSTUFF->hooks.h_controlchangehook = hook;
}

void libpd_set_queued_programchangehook(const t_libpd_programchangehook hook) {
  QUEUEDSTUFF->hooks.h_programchangehook = hook;
}

void libpd_set_queued_pitchbendhook(const t_libpd_pitchbendhook hook) {
  QUEUEDSTUFF->hooks.h_pitchbendhook = hook;
}

void libpd_set_queued_aftertouchhook(const t_libpd_aftertouchhook hook) {
  QUEUEDSTUFF->hooks.h_aftertouchhook = hook;
}

void libpd_set_queued_polyaftertouchhook(const t_libpd_polyaftertouchhook hook) {
  QUEUEDSTUFF->hooks.h_polyaftertouchhook = hook;
}

void libpd_set_queued_midibytehook(const t_libpd_midibytehook hook) {
  QUEUEDSTUFF->hooks.h_midibytehook = hook;
}

static void queued_stuff_free(void *p) {
  queued_stuff *queued = (queued_stuff *)p;
  if (queued->pd_receive_buffer) rb_free(queued->pd_receive_buffer);
  if (queued->midi_receive_buffer) rb_free(queued->midi_receive_buffer);
}

int libpd_queued_init(void) {
  int ret = libpd_init();

  libpd_set_printhook(internal_printhook);
  libpd_set_banghook(internal_banghook);
  libpd_set_doublehook(internal_doublehook);
  libpd_set_symbolhook(internal_symbolhook);
  libpd_set_listhook(internal_listhook);
  libpd_set_messagehook(internal_messagehook);

  libpd_set_noteonhook(internal_noteonhook);
  libpd_set_controlchangehook(internal_controlchangehook);
  libpd_set_programchangehook(internal_programchangehook);
  libpd_set_pitchbendhook(internal_pitchbendhook);
  libpd_set_aftertouchhook(internal_aftertouchhook);
  libpd_set_polyaftertouchhook(internal_polyaftertouchhook);
  libpd_set_midibytehook(internal_midibytehook);

  t_libpdimp *imp = LIBPDSTUFF;
  if (!imp->i_queued) {
    queued_stuff *queued = (queued_stuff *)calloc(1, sizeof(queued_stuff));
    if(!queued) goto cleanup;
    queued->pd_receive_buffer = rb_create(BUFFER_SIZE);
    if (!queued->pd_receive_buffer) goto cleanup;
    queued->midi_receive_buffer = rb_create(BUFFER_SIZE);
    if (!queued->midi_receive_buffer) goto cleanup;
    imp->i_queued = (void *)queued;
    imp->i_queued_freehook = queued_stuff_free;
  }
  return ret;
cleanup:
  libpd_queued_release();
  return -2;
}

void libpd_queued_release(void) {
  t_libpdimp *imp = LIBPDSTUFF;
  if (imp->i_queued) {
    queued_stuff_free(imp->i_queued);
    imp->i_queued = NULL;
    imp->i_queued_freehook = NULL;
  }
}

void libpd_queued_receive_pd_messages(void) {
  queued_stuff *queued = QUEUEDSTUFF;
  size_t available = rb_available_to_read(queued->pd_receive_buffer);
  if (!available) return;
  rb_read_from_buffer(queued->pd_receive_buffer, queued->temp_buffer, (int)available);
  char *end = queued->temp_buffer + available;
  char *buffer = queued->temp_buffer;
  while (buffer < end) {
    pd_params *p = (pd_params *)buffer;
    buffer += S_PD_PARAMS;
    switch (p->type) {
      case LIBPD_PRINT: {
        receive_print(p, &buffer);
        break;
      }
      case LIBPD_BANG: {
        receive_bang(p, &buffer);
        break;
      }
      case LIBPD_FLOAT: {
        receive_float(p, &buffer);
        break;
      }
      case LIBPD_SYMBOL: {
        receive_symbol(p, &buffer);
        break;
      }
      case LIBPD_LIST: {
        receive_list(p, &buffer);
        break;
      }
      case LIBPD_MESSAGE: {
        receive_message(p, &buffer);
        break;
      }
      default:
        break;
    }
  }
}

void libpd_queued_receive_midi_messages(void) {
  queued_stuff *queued = QUEUEDSTUFF;
  size_t available = rb_available_to_read(queued->midi_receive_buffer);
  if (!available) return;
  rb_read_from_buffer(queued->midi_receive_buffer, queued->temp_buffer, (int)available);
  char *end = queued->temp_buffer + available;
  char *buffer = queued->temp_buffer;
  while (buffer < end) {
    midi_params *p = (midi_params *)buffer;
    buffer += S_MIDI_PARAMS;
    switch (p->type) {
      case LIBPD_NOTEON: {
        receive_noteon(p, &buffer);
        break;
      }
      case LIBPD_CONTROLCHANGE: {
        receive_controlchange(p, &buffer);
        break;
      }
      case LIBPD_PROGRAMCHANGE: {
        receive_programchange(p, &buffer);
        break;
      }
      case LIBPD_PITCHBEND: {
        receive_pitchbend(p, &buffer);
        break;
      }
      case LIBPD_AFTERTOUCH: {
        receive_aftertouch(p, &buffer);
        break;
      }
      case LIBPD_POLYAFTERTOUCH: {
        receive_polyaftertouch(p, &buffer);
        break;
      }
      case LIBPD_MIDIBYTE: {
        receive_midibyte(p, &buffer);
        break;
      }
      default:
        break;
    }
  }
}
