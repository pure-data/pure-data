/*
 * Copyright (c) 2012 Peter Brinkmann (peter.brinkmann@gmail.com)
 *
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 *
 * See https://github.com/libpd/libpd/wiki for documentation
 *
 */

#include "z_ringbuffer.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

ring_buffer *rb_create(int size) {
  if (size & 0xff) return NULL;  // size must be a multiple of 256
  ring_buffer *buffer = malloc(sizeof(ring_buffer));
  if (!buffer) return NULL;
  buffer->buf_ptr = calloc(size, sizeof(char));
  if (!buffer->buf_ptr) {
    free(buffer);
    return NULL;
  }
  buffer->size = size;
  buffer->write_idx = 0;
  buffer->read_idx = 0;
  return buffer;
}

void rb_free(ring_buffer *buffer) {
  free(buffer->buf_ptr);
  free(buffer);
}

int rb_available_to_write(ring_buffer *buffer) {
  if (buffer) {
    // note: the largest possible result is buffer->size - 1 because
    // we adopt the convention that read_idx == write_idx means that the
    // buffer is empty
    int read_idx = atomic_int_load(&buffer->read_idx);
    int write_idx = atomic_int_load(&buffer->write_idx);
    return (buffer->size + read_idx - write_idx - 1) % buffer->size;
  } else {
    return 0;
  }
}

int rb_available_to_read(ring_buffer *buffer) {
  if (buffer) {
    int read_idx = atomic_int_load(&buffer->read_idx);
    int write_idx = atomic_int_load(&buffer->write_idx);
    return (buffer->size + write_idx - read_idx) % buffer->size;
  } else {
    return 0;
  }
}

int rb_write_to_buffer(ring_buffer *buffer, int n, ...) {
  if (!buffer) return -1;
  int write_idx = buffer->write_idx;  // no need for sync in writer thread
  int available = rb_available_to_write(buffer);
  va_list args;
  va_start(args, n);
  int i;
  for (i = 0; i < n; ++i) {
    const char* src = va_arg(args, const char*);
    int len = va_arg(args, int);
    available -= len;
    if (len < 0 || available < 0) return -1;
    if (write_idx + len <= buffer->size) {
      memcpy(buffer->buf_ptr + write_idx, src, len);
    } else {
      int d = buffer->size - write_idx;
      memcpy(buffer->buf_ptr + write_idx, src, d);
      memcpy(buffer->buf_ptr, src + d, len - d);
    }
    write_idx = (write_idx + len) % buffer->size;
  }
  va_end(args);
  atomic_int_store(&buffer->write_idx, write_idx); // includes memory barrier
  return 0;
}

int rb_write_value_to_buffer(ring_buffer *buffer, int value, int n) {
  if (!buffer) return -1;
  int write_idx = buffer->write_idx;  // No need for sync in writer thread.
  int available = rb_available_to_write(buffer);
  available -= n;
  if (n < 0 || available < 0) return -1;
  if (write_idx + n <= buffer->size) {
    memset(buffer->buf_ptr + write_idx, value, n);
  } else {
    int d = buffer->size - write_idx;
    memset(buffer->buf_ptr + write_idx, value, d);
    memset(buffer->buf_ptr, value, n - d);
  }
  write_idx = (write_idx + n) % buffer->size;
  atomic_int_store(&buffer->write_idx, write_idx); // includes memory barrier
  return 0;
}

int rb_read_from_buffer(ring_buffer *buffer, char *dest, int len) {
  if (len == 0) return 0;
  if (!buffer || len < 0 || len > rb_available_to_read(buffer)) return -1;
  // note that rb_available_to_read also serves as a memory barrier, and so any
  // writes to buffer->buf_ptr that precede the update of buffer->write_idx are
  // visible to us now
  int read_idx = buffer->read_idx;  // no need for sync in reader thread
  if (read_idx + len <= buffer->size) {
    memcpy(dest, buffer->buf_ptr + read_idx, len);
  } else {
    int d = buffer->size - read_idx;
    memcpy(dest, buffer->buf_ptr + read_idx, d);
    memcpy(dest + d, buffer->buf_ptr, len - d);
  }
  // includes memory barrier
  atomic_int_store(&buffer->read_idx, (read_idx + len) % buffer->size);
  return 0;
}

// simply reset the indices
void rb_clear_buffer(ring_buffer *buffer) {
  if (buffer) {
    atomic_int_store(&buffer->read_idx, 0);
    atomic_int_store(&buffer->write_idx, 0);
  }
}
