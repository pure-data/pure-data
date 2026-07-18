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

#include "z_hooks.h"
#include <stdlib.h>

/* instance */

t_libpdimp libpd_mainimp = {NULL};

t_libpdimp* libpdimp_new(void) {
  t_libpdimp *imp = calloc(1, sizeof(t_libpdimp));
  return imp;
}

void libpdimp_free(t_libpdimp *imp) {
  if (imp == &libpd_mainimp) return;
  if (imp->i_queued) imp->i_queued_freehook(imp->i_queued);
  if (imp->i_print_util) free(imp->i_print_util);
  if (imp->i_data && imp->i_data_freehook) imp->i_data_freehook(imp->i_data);
  free(imp);
}
