#pragma once

/* Copyright (c) 2020 Andi McClure.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

typedef intptr_t t_fileops_handle;

typedef enum {
	FILEOPS_NONE = 0,
	FILEOPS_READ = 1,
	FILEOPS_WRITE = 2,
	FILEOPS_CREAT = 4,
	FILEOPS_TRUNC = 8,
	FILEOPS_SEEK_SET = 0x10,
	FILEOPS_SEEK_CUR = 0x20,
	FILEOPS_SEEK_END = 0x40,
} t_fileops_flags;

typedef struct _fileops_stat {
	bool isdir_known; // If false, isdir value is undefined
	bool isdir;
} t_fileops_stat;

typedef struct _t_fileops /* All function pointers return true on success */
{
	bool (*open)(const char *path, t_fileops_flags flags, t_fileops_handle *handle);
	bool (*close)(t_fileops_handle handle);
	bool (*stat)(t_fileops_handle handle, t_fileops_stat *stat);
	int64_t (*seek)(t_fileops_handle handle, int64_t offset, t_fileops_flags flags);
	ssize_t (*read)(t_fileops_handle handle, void *buf, size_t nbyte);
	ssize_t (*write)(t_fileops_handle handle, const void *buf, size_t nbyte);
	ssize_t (*scanf)(t_fileops_handle handle, const char * restrict format, ...);
	ssize_t (*vscanf)(t_fileops_handle handle, const char * restrict format, va_list ap);
	ssize_t (*printf)(t_fileops_handle handle, const char * restrict format, ...);
	ssize_t (*vprintf)(t_fileops_handle handle, const char * restrict format, va_list ap);
	bool (*flush)(t_fileops_handle handle);
} t_fileops;

// Also search: fstat, sys_fopen

extern t_fileops sys_fileops;
