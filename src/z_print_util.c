/*
 * Copyright (c) 2013 Dan Wilcox (danomatika@gmail.com) &
 *                    Peter Brinkmann (peter.brinkmann@gmail.com)
 * Copyright (c) 2022 libpd team
 *
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 *
 * See https://github.com/libpd/libpd/wiki for documentation
 *
 */

#include "z_print_util.h"

#include <stdlib.h>
#include <string.h>

#include "z_hooks.h"

#define PRINT_LINE_SIZE 2048

typedef struct _print_util {
  t_libpd_printhook concat_printhook;
  char concat_buf[PRINT_LINE_SIZE]; /* line buffer */
  int concat_len;                   /* current line len */
} print_util;

void libpd_set_concatenated_printhook(const t_libpd_printhook hook) {
  t_libpdimp *imp = LIBPDSTUFF;
  if (hook) {
    if (!imp->i_print_util) {
      imp->i_print_util = calloc(1, sizeof(print_util));
    }
    ((print_util *)imp->i_print_util)->concat_printhook = hook;
  }
  else {
    if (imp->i_print_util) {
      free(imp->i_print_util);
      imp->i_print_util = NULL;
    }
  }
}

void libpd_print_concatenator(const char *s) {
  print_util *util = (print_util *)LIBPDSTUFF->i_print_util;
  if (!util) return;

  util->concat_buf[util->concat_len] = '\0';

  int len = (int)strlen(s);
  while (util->concat_len + len >= PRINT_LINE_SIZE) {
    int d = PRINT_LINE_SIZE - 1 - util->concat_len;
    strncat(util->concat_buf, s, d);
    util->concat_printhook(util->concat_buf);
    s += d;
    len -= d;
    util->concat_len = 0;
    util->concat_buf[0] = '\0';
  }

  strncat(util->concat_buf, s, len);
  util->concat_len += len;

  if (util->concat_len > 0 && util->concat_buf[util->concat_len - 1] == '\n') {
    util->concat_buf[util->concat_len - 1] = '\0';
    util->concat_printhook(util->concat_buf);
    util->concat_len = 0;
  }
}
