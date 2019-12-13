/*
 * Copyright (c) 2012 Peter Brinkmann (peter.brinkmann@gmail.com)
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

#include "z_ringbuffer.h"

t_libpd_printhook libpd_queued_printhook = NULL;
t_libpd_banghook libpd_queued_banghook = NULL;
t_libpd_floathook libpd_queued_floathook = NULL;
t_libpd_symbolhook libpd_queued_symbolhook = NULL;
t_libpd_listhook libpd_queued_listhook = NULL;
t_libpd_messagehook libpd_queued_messagehook = NULL;

t_libpd_noteonhook libpd_queued_noteonhook = NULL;
t_libpd_controlchangehook libpd_queued_controlchangehook = NULL;
t_libpd_programchangehook libpd_queued_programchangehook = NULL;
t_libpd_pitchbendhook libpd_queued_pitchbendhook = NULL;
t_libpd_aftertouchhook libpd_queued_aftertouchhook = NULL;
t_libpd_polyaftertouchhook libpd_queued_polyaftertouchhook = NULL;
t_libpd_midibytehook libpd_queued_midibytehook = NULL;

typedef struct _pd_params {
  enum {
    LIBPD_PRINT, LIBPD_BANG, LIBPD_FLOAT,
    LIBPD_SYMBOL, LIBPD_LIST, LIBPD_MESSAGE,
  } type;
  const char *src;
  float x;
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

#define BUFFER_SIZE 16384
#define S_PD_PARAMS sizeof(pd_params)
#define S_MIDI_PARAMS sizeof(midi_params)
#define S_ATOM sizeof(t_atom)

static ring_buffer *pd_receive_buffer = NULL;
static ring_buffer *midi_receive_buffer = NULL;

static void receive_print(pd_params *p, char **buffer) {
  if (libpd_queued_printhook) {
    libpd_queued_printhook(*buffer);
  }
  *buffer += p->argc;
}

static void receive_bang(pd_params *p, char **buffer) {
  if (libpd_queued_banghook) {
    libpd_queued_banghook(p->src);
  }
}

static void receive_float(pd_params *p, char **buffer) {
  if (libpd_queued_floathook) {
    libpd_queued_floathook(p->src, p->x);
  }
}

static void receive_symbol(pd_params *p, char **buffer) {
  if (libpd_queued_symbolhook) {
    libpd_queued_symbolhook(p->src, p->sym);
  }
}

static void receive_list(pd_params *p, char **buffer) {
  if (libpd_queued_listhook) {
    libpd_queued_listhook(p->src, p->argc, (t_atom *) *buffer);
  }
  *buffer += p->argc * S_ATOM;
}

static void receive_message(pd_params *p, char **buffer) {
  if (libpd_queued_messagehook) {
    libpd_queued_messagehook(p->src, p->sym, p->argc, (t_atom *) *buffer);
  }
  *buffer += p->argc * S_ATOM;
}

#define LIBPD_WORD_ALIGN 8

static void internal_printhook(const char *s) {
  static char padding[LIBPD_WORD_ALIGN];
  int len = (int) strlen(s) + 1; // remember terminating null char
  int rest = len % LIBPD_WORD_ALIGN;
  if (rest) rest = LIBPD_WORD_ALIGN - rest;
  int total = len + rest;
  if (rb_available_to_write(pd_receive_buffer) >= S_PD_PARAMS + total) {
    pd_params p = {LIBPD_PRINT, NULL, 0.0f, NULL, total};
    rb_write_to_buffer(pd_receive_buffer, 3,
        (const char *)&p, S_PD_PARAMS, s, len, padding, rest);
  }
}

static void internal_banghook(const char *src) {
  if (rb_available_to_write(pd_receive_buffer) >= S_PD_PARAMS) {
    pd_params p = {LIBPD_BANG, src, 0.0f, NULL, 0};
    rb_write_to_buffer(pd_receive_buffer, 1, (const char *)&p, S_PD_PARAMS);
  }
}

static void internal_floathook(const char *src, float x) {
  if (rb_available_to_write(pd_receive_buffer) >= S_PD_PARAMS) {
    pd_params p = {LIBPD_FLOAT, src, x, NULL, 0};
    rb_write_to_buffer(pd_receive_buffer, 1, (const char *)&p, S_PD_PARAMS);
  }
}

static void internal_symbolhook(const char *src, const char *sym) {
  if (rb_available_to_write(pd_receive_buffer) >= S_PD_PARAMS) {
    pd_params p = {LIBPD_SYMBOL, src, 0.0f, sym, 0};
    rb_write_to_buffer(pd_receive_buffer, 1, (const char *)&p, S_PD_PARAMS);
  }
}

static void internal_listhook(const char *src, int argc, t_atom *argv) {
  int n = argc * S_ATOM;
  if (rb_available_to_write(pd_receive_buffer) >= S_PD_PARAMS + n) {
    pd_params p = {LIBPD_LIST, src, 0.0f, NULL, argc};
    rb_write_to_buffer(pd_receive_buffer, 2,
        (const char *)&p, S_PD_PARAMS, (const char *)argv, n);
  }
}

static void internal_messagehook(const char *src, const char* sym,
    int argc, t_atom *argv) {
  int n = argc * S_ATOM;
  if (rb_available_to_write(pd_receive_buffer) >= S_PD_PARAMS + n) {
    pd_params p = {LIBPD_MESSAGE, src, 0.0f, sym, argc};
    rb_write_to_buffer(pd_receive_buffer, 2,
        (const char *)&p, S_PD_PARAMS, (const char *)argv, n);
  }
}

static void receive_noteon(midi_params *p, char **buffer) {
  if (libpd_queued_noteonhook) {
    libpd_queued_noteonhook(p->midi1, p->midi2, p->midi3);
  }
}

static void receive_controlchange(midi_params *p, char **buffer) {
  if (libpd_queued_controlchangehook) {
    libpd_queued_controlchangehook(p->midi1, p->midi2, p->midi3);
  }
}

static void receive_programchange(midi_params *p, char **buffer) {
  if (libpd_queued_programchangehook) {
    libpd_queued_programchangehook(p->midi1, p->midi2);
  }
}

static void receive_pitchbend(midi_params *p, char **buffer) {
  if (libpd_queued_pitchbendhook) {
    libpd_queued_pitchbendhook(p->midi1, p->midi2);
  }
}

static void receive_aftertouch(midi_params *p, char **buffer) {
  if (libpd_queued_aftertouchhook) {
    libpd_queued_aftertouchhook(p->midi1, p->midi2);
  }
}

static void receive_polyaftertouch(midi_params *p, char **buffer) {
  if (libpd_queued_polyaftertouchhook) {
    libpd_queued_polyaftertouchhook(p->midi1, p->midi2, p->midi3);
  }
}

static void receive_midibyte(midi_params *p, char **buffer) {
  if (libpd_queued_midibytehook) {
    libpd_queued_midibytehook(p->midi1, p->midi2);
  }
}

static void internal_noteonhook(int channel, int pitch, int velocity) {
  if (rb_available_to_write(midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_NOTEON, channel, pitch, velocity};
    rb_write_to_buffer(midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_controlchangehook(int channel, int controller, int value) {
  if (rb_available_to_write(midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_CONTROLCHANGE, channel, controller, value};
    rb_write_to_buffer(midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_programchangehook(int channel, int value) {
  if (rb_available_to_write(midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_PROGRAMCHANGE, channel, value, 0};
    rb_write_to_buffer(midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_pitchbendhook(int channel, int value) {
  if (rb_available_to_write(midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_PITCHBEND, channel, value, 0};
    rb_write_to_buffer(midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_aftertouchhook(int channel, int value) {
  if (rb_available_to_write(midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_AFTERTOUCH, channel, value, 0};
    rb_write_to_buffer(midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_polyaftertouchhook(int channel, int pitch, int value) {
  if (rb_available_to_write(midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_POLYAFTERTOUCH, channel, pitch, value};
    rb_write_to_buffer(midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

static void internal_midibytehook(int port, int byte) {
  if (rb_available_to_write(midi_receive_buffer) >= S_MIDI_PARAMS) {
    midi_params p = {LIBPD_MIDIBYTE, port, byte, 0};
    rb_write_to_buffer(midi_receive_buffer, 1, (const char *)&p, S_MIDI_PARAMS);
  }
}

void libpd_set_queued_printhook(const t_libpd_printhook hook) {
  libpd_queued_printhook = hook;
}

void libpd_set_queued_banghook(const t_libpd_banghook hook) {
  libpd_queued_banghook = hook;
}

void libpd_set_queued_floathook(const t_libpd_floathook hook) {
  libpd_queued_floathook = hook;
}

void libpd_set_queued_symbolhook(const t_libpd_symbolhook hook) {
  libpd_queued_symbolhook = hook;
}

void libpd_set_queued_listhook(const t_libpd_listhook hook) {
  libpd_queued_listhook = hook;
}

void libpd_set_queued_messagehook(const t_libpd_messagehook hook) {
  libpd_queued_messagehook = hook;
}

void libpd_set_queued_noteonhook(const t_libpd_noteonhook hook) {
  libpd_queued_noteonhook = hook;
}

void libpd_set_queued_controlchangehook(const t_libpd_controlchangehook hook) {
  libpd_queued_controlchangehook = hook;
}

void libpd_set_queued_programchangehook(const t_libpd_programchangehook hook) {
  libpd_queued_programchangehook = hook;
}

void libpd_set_queued_pitchbendhook(const t_libpd_pitchbendhook hook) {
  libpd_queued_pitchbendhook = hook;
}

void libpd_set_queued_aftertouchhook(const t_libpd_aftertouchhook hook) {
  libpd_queued_aftertouchhook = hook;
}

void libpd_set_queued_polyaftertouchhook(const t_libpd_polyaftertouchhook hook) {
  libpd_queued_polyaftertouchhook = hook;
}

void libpd_set_queued_midibytehook(const t_libpd_midibytehook hook) {
  libpd_queued_midibytehook = hook;
}

int libpd_queued_init() {
  if (!pd_receive_buffer) {
    pd_receive_buffer = rb_create(BUFFER_SIZE);
    if (!pd_receive_buffer) return -2;
  }
  if (!midi_receive_buffer) {
    midi_receive_buffer = rb_create(BUFFER_SIZE);
    if (!midi_receive_buffer) return -2;
  }

  libpd_set_printhook(internal_printhook);
  libpd_set_banghook(internal_banghook);
  libpd_set_floathook(internal_floathook);
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

  return libpd_init();
}

void libpd_queued_release() {
  if (pd_receive_buffer) {
    rb_free(pd_receive_buffer);
    pd_receive_buffer = NULL;
  }
  if (midi_receive_buffer) {
    rb_free(midi_receive_buffer);
    midi_receive_buffer = NULL;
  }
}

void libpd_queued_receive_pd_messages() {
  size_t available = rb_available_to_read(pd_receive_buffer);
  if (!available) return;
  static char temp_buffer[BUFFER_SIZE];
  rb_read_from_buffer(pd_receive_buffer, temp_buffer, (int) available);
  char *end = temp_buffer + available;
  char *buffer = temp_buffer;
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

void libpd_queued_receive_midi_messages() {
  size_t available = rb_available_to_read(midi_receive_buffer);
  if (!available) return;
  static char temp_buffer[BUFFER_SIZE];
  rb_read_from_buffer(midi_receive_buffer, temp_buffer, (int) available);
  char *end = temp_buffer + available;
  char *buffer = temp_buffer;
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
