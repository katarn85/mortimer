/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Jaewon Lim <jaewon81.lim@samsung.com>
 * Woojin Jung <woojin2.jung@samsung.com>
 * Juyoung Kim <j0.kim@samsung.com>
 * Anastasia Lyupa <a.lyupa@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Contributors:
 * - S-Core Co., Ltd
 * - Samsung RnD Institute Russia
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include "daprobe.h"
#include "probeinfo.h"
#include "dautil.h"
#include "dahelper.h"
#include "da_io.h"

#include "binproto.h"
#include "real_functions.h"

static enum DaOptions _sopt = OPT_FILE;

static inline char *get_abs_path(FILE *file, const char *fname,
				 char *buf, size_t bufsiz)
{
	char *path = file ? real_abs_path(fileno(file), buf, bufsiz): NULL;

	if (!path)
		path = absolutize_filepath(fname, buf, bufsiz);

	return path;
}

FILE* fopen(const char* filename, const char* mode)
{
	static FILE* (*fopenp)(const char* filename, const char* mode);
	char buffer[PATH_MAX];
	FILE* fret;

	BEFORE_ORIGINAL_FILE(fopen, LIBC);

	fret = fopenp(filename, mode);

	_filepath = get_abs_path(fret, filename, buffer, PATH_MAX);

	AFTER_PACK_ORIGINAL_FILEP(API_ID_fopen,
				  'p', fret, 0, fret, FD_API_OPEN, "s4s",
				  filename, mode);

	return fret;
}

FILE* freopen(const char * filename, const char * mode, FILE * stream)
{
	static FILE* (*freopenp)(const char *filename, const char *mode,
				 FILE *stream);
	char buffer[PATH_MAX];
 	FILE* fret;

	BEFORE_ORIGINAL_FILE(freopen, LIBC);

	fret = freopenp(filename, mode, stream);

	_filepath = get_abs_path(fret, filename, buffer, PATH_MAX);

	AFTER_PACK_ORIGINAL_FILEP(API_ID_freopen, 'p', fret, 0, fret, FD_API_OPEN,
				  "s4sp", filename, mode,
				  voidp_to_uint64(stream));

	return fret;
}

FILE* fdopen(int fildes, const char *mode)
{
	static FILE* (*fdopenp)(int fildes, const char *mode);
 	FILE* fret;

	BEFORE_ORIGINAL_FILE(fdopen, LIBC);

	fret = fdopenp(fildes, mode);

	AFTER_PACK_ORIGINAL_FILEP(API_ID_fdopen,
				  'p', fret, 0, fret, FD_API_OPEN, "ds", fildes, mode);

	return fret;
}

int fflush(FILE* stream)
{
	static int (*fflushp)(FILE* stream);

	BEFORE_ORIGINAL_FILE(fflush, LIBC);
	ret = fflushp(stream);
	AFTER_PACK_ORIGINAL_FILEP(API_ID_fflush,
				  'd', ret, 0, stream, FD_API_OTHER, "p",
				  voidp_to_uint64(stream));
	return ret;
}

int fclose(FILE* stream)
{
	static int (*fclosep)(FILE* stream);
	DECLARE_VARIABLE_FD;

	GET_REAL_FUNC(fclose, LIBC);

	PRE_PROBEBLOCK_BEGIN();
	GET_FD_FROM_FILEP(stream);
	if(_fd != -1) {
		_fstatret = fstat(_fd, &_statbuf);
	}
	PRE_PROBEBLOCK_END();

	ret = fclosep(stream);

	POST_PACK_PROBEBLOCK_BEGIN();
	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE,
			  API_ID_fclose,
			  "p", voidp_to_uint64(stream));
	PACK_COMMON_END('d', ret, newerrno, blockresult);
	POST_PACK_PROBEBLOCK_MIDDLE_FD(0, _fd, FD_API_CLOSE);
	POST_PACK_PROBEBLOCK_END();

	return ret;
}

FILE * tmpfile ( void )
{
	static FILE* (*tmpfilep) ( void );
	FILE* fret;

	BEFORE_ORIGINAL_FILE(tmpfile, LIBC);
	_filepath = "<temp file>";
	fret = tmpfilep();
	AFTER_PACK_ORIGINAL_FILEP(API_ID_tmpfile,
				  'p', fret, 0, fret, FD_API_OPEN, "s", "");
	return fret;
}

int fgetpos(FILE* stream, fpos_t* position)
{
	static int (*fgetposp)(FILE* stream, fpos_t* position);

	BEFORE_ORIGINAL_FILE(fgetpos, LIBC);
	ret = fgetposp(stream, position);
	AFTER_PACK_ORIGINAL_FILEP(API_ID_fgetpos,
				  'd', ret, 0, stream, FD_API_OTHER, "pp",
				  voidp_to_uint64(stream),
				  voidp_to_uint64(position));
	return ret;
}

int fseek(FILE* stream, long int offset, int origin)
{
	static int (*fseekp)(FILE* stream, long int offset, int origin);

	BEFORE_ORIGINAL_FILE(fseek, LIBC);
	ret = fseekp(stream, offset, origin);
	AFTER_PACK_ORIGINAL_FILEP(API_ID_fseek,
				  'd', ret, (unsigned int)offset, stream, FD_API_OTHER,
				  "pxd", voidp_to_uint64(stream),
				  (uint64_t)(offset), origin);
	return ret;
}

int fsetpos(FILE* stream, const fpos_t* pos)
{
	static int (*fsetposp)(FILE* stream, const fpos_t* pos);

	BEFORE_ORIGINAL_FILE(fsetpos, LIBC);
	ret = fsetposp(stream, pos);
	AFTER_PACK_ORIGINAL_FILEP(API_ID_fsetpos,
				  'd', ret, 0, stream, FD_API_OTHER, "pp",
				  voidp_to_uint64(stream), voidp_to_uint64(pos));
	return ret;
}

long int ftell(FILE* stream)
{
	static long int (*ftellp)(FILE* stream);
	long int lret;

	BEFORE_ORIGINAL_FILE(ftell, LIBC);

	lret = ftellp(stream);

	AFTER_PACK_ORIGINAL_FILEP(API_ID_ftell,
				  'x', lret, 0, stream, FD_API_OTHER, "p",
				  voidp_to_uint64(stream));

	return lret;
}

void rewind(FILE* stream)
{
	static void (*rewindp)(FILE* stream);

	BEFORE_ORIGINAL_FILE(rewind, LIBC);

	rewindp(stream);

	AFTER_PACK_ORIGINAL_FILEP(API_ID_rewind,
				  'v', 0, 0, stream, FD_API_OTHER, "p",
				  voidp_to_uint64(stream));
}

void clearerr(FILE* stream)
{
	static void (*clearerrp)(FILE* stream);

	BEFORE_ORIGINAL_FILE(clearerr, LIBC);

	clearerrp(stream);

	AFTER_PACK_ORIGINAL_FILEP(API_ID_clearerr,
				  'v', 0, 0, stream, FD_API_OTHER, "p",
				  voidp_to_uint64(stream));
}

int feof(FILE* stream)
{
	static int (*feofp)(FILE* stream);

	BEFORE_ORIGINAL_FILE(feof, LIBC);
	ret = feofp(stream);
	AFTER_PACK_ORIGINAL_FILEP(API_ID_feof,
				  'd', ret, 0, stream, FD_API_OTHER, "p",
				  voidp_to_uint64(stream));
	return ret;
}

int ferror(FILE* stream)
{
	static int (*ferrorp)(FILE* stream);

	BEFORE_ORIGINAL_FILE(ferror, LIBC);
	ret = ferrorp(stream);
	AFTER_PACK_ORIGINAL_FILEP(API_ID_ferror,
				  'd', ret, 0, stream, FD_API_OTHER, "p",
				  voidp_to_uint64(stream));
	return ret;
}

int fileno(FILE* stream)
{
	static int (*filenop)(FILE* stream);

	BEFORE_ORIGINAL_FILE(fileno, LIBC);
	ret = filenop(stream);
	AFTER_PACK_ORIGINAL_FILEP(API_ID_fileno,
				  'd', ret, 0, stream, FD_API_OTHER, "p",
				  voidp_to_uint64(stream));
	return ret;
}



// *******************************************************************
// File read / write APIs
// *******************************************************************

int vfprintf(FILE* stream, const char* format, va_list arg)
{
	static int (*vfprintfp)(FILE* stream, const char* format, va_list arg);

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_vfprintf, 'd', vfprintf, LIBC, stream,
					FD_API_WRITE_START, "ps",
					voidp_to_uint64(stream), format);

	ret = vfprintfp(stream, format, arg);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_vfprintf, 'd', ret, ret, stream,
				       FD_API_WRITE_END, "ps",
				       voidp_to_uint64(stream), format);

	return ret;
}

int vfscanf(FILE* stream, const char* format, va_list arg)
{
	static int (*vfscanfp)(FILE* stream, const char* format, va_list arg);

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_vfscanf, 'd', vfscanf, LIBC, stream,
					FD_API_READ_START, "ps",
					voidp_to_uint64(stream), format);

	ret = vfscanfp(stream, format, arg);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_vfscanf, 'd', ret, ret, stream,
				       FD_API_READ_END, "ps",
				       voidp_to_uint64(stream), format);

	return ret;
}

int fgetc(FILE* stream)
{
	static int (*fgetcp)(FILE* stream);

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_fgetc, 'd', fgetc, LIBC, stream,
					FD_API_READ_START, "p",
					voidp_to_uint64(stream));

	ret = fgetcp(stream);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_fgetc, 'd', ret, (ret != EOF), stream,
				       FD_API_READ_END, "p",
				       voidp_to_uint64(stream));

	return ret;
}

#if 0	// why is this commented?
char* fgets(char* str, int size, FILE* stream)
{
	static char* (*fgetsp)(char* str, int num, FILE* stream);
	char* cret;

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_fgets, 'p', FD_API_READ_START,
				fgets, LIBC, stream, "sdp", str, size, stream);

	cret = fgetsp(str, size, stream);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_fgets, 'p', cret, (ret == NULL ? 0 : strlen(cret)),
			stream, FD_API_READ_END, "sdp", str, size, stream);

	return cret;
}
#endif

int fputc(int character, FILE* stream)
{
	static int (*fputcp)(int character, FILE* stream);

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_fputc, 'd', fputc, LIBC, stream,
					FD_API_WRITE_START, "dp", character,
					voidp_to_uint64(stream));

	ret = fputcp(character, stream);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_fputc, 'd', ret, (ret == EOF ? 0 : 1),
				       stream, FD_API_WRITE_END, "dp",
				       character, voidp_to_uint64(stream));

	return ret;
}

int fputs(const char* str, FILE* stream)
{
	static int (*fputsp)(const char* str, FILE* stream);

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_fputs, 'd', fputs, LIBC, stream,
					FD_API_WRITE_START, "sp", str,
					voidp_to_uint64(stream));

	ret = fputsp(str, stream);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_fputs, 'd', ret, ret, stream,
				       FD_API_WRITE_END, "sp", str,
				       voidp_to_uint64(stream));

	return ret;
}

int getc(FILE* stream)
{
	static int (*getcp)(FILE* stream);

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_getc, 'd', getc, LIBC, stream,
					FD_API_READ_START, "p",
					voidp_to_uint64(stream));

	ret = getcp(stream);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_getc, 'd', ret, (ret == EOF ? 0 : 1), stream,
				       FD_API_READ_END, "p",
				       voidp_to_uint64(stream));

	return ret;
}

int putc(int character, FILE* stream)
{
	static int (*putcp)(int character, FILE* stream);

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_putc, 'd', putc, LIBC, stream,
					FD_API_WRITE_START, "dp", character,
					voidp_to_uint64(stream));

	ret = putcp(character, stream);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_putc, 'd', ret, (ret == EOF ? 0 : 1),
				       stream, FD_API_WRITE_END, "dp",
				       character, voidp_to_uint64(stream));

	return ret;
}

int ungetc(int character, FILE* stream)
{
	static int (*ungetcp)(int character, FILE* stream);

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_putc, 'd', ungetc, LIBC, stream,
					FD_API_WRITE_START, "dp", character,
					voidp_to_uint64(stream));

	ret = ungetcp(character, stream);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_ungetc, 'd', ret, 0, stream,
				       FD_API_OTHER, "dp", character,
				       voidp_to_uint64(stream));

	return ret;
}

size_t fread(void* ptr, size_t size, size_t count, FILE* stream)
{
	static size_t (*freadp)(void* ptr, size_t size, size_t count, FILE* stream);
	size_t tret;

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_fread, 'x', fread, LIBC, stream,
					FD_API_READ_START, "pxxp",
					voidp_to_uint64(ptr),
					(uint64_t)(size),
					(uint64_t)(count),
					voidp_to_uint64(stream));

	tret = freadp(ptr, size, count, stream);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_fread, 'x', tret, tret*size, stream,
				       FD_API_READ_END, "pxxp",
				       voidp_to_uint64(ptr),
				       (uint64_t)(size),
				       (uint64_t)(count),
				       voidp_to_uint64(stream));

	return tret;
}

size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream)
{
	static size_t (*fwritep)(const void* ptr, size_t size, size_t count, FILE* stream);
	size_t tret;

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_fwrite, 'x', fwrite, LIBC, stream,
					FD_API_WRITE_START, "pxxp",
					voidp_to_uint64(ptr),
					(uint64_t)(size),
					(uint64_t)(count),
					voidp_to_uint64(stream));

	tret = fwritep(ptr, size, count, stream);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_fwrite, 'x', tret, tret*size, stream,
				       FD_API_WRITE_END, "pxxp",
				       voidp_to_uint64(ptr),
				       (uint64_t)(size),
				       (uint64_t)(count),
				       voidp_to_uint64(stream));

	return tret;
}

// *********************************************************
// variable parameter function
// *********************************************************
int fprintf(FILE* stream, const char* format, ...)
{
	static int (*vfprintfp)(FILE* stream, const char* format, ...);

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_fprintf, 'd', vfprintf, LIBC, stream,
					FD_API_WRITE_START, "ps",
					voidp_to_uint64(stream), format);

	va_list arg;
	va_start(arg, format);

	ret = vfprintfp(stream, format, arg);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_fprintf, 'd', ret, ret, stream,
				       FD_API_WRITE_END, "ps",
				       voidp_to_uint64(stream), format);

	va_end(arg);

	return ret;
}

int fscanf(FILE* stream, const char* format, ...)
{
	static int (*vfscanfp)(FILE* stream, const char* format, ...);

	BEFORE_ORIGINAL_START_END_FILEP(API_ID_fscanf, 'd', vfscanf, LIBC, stream,
					FD_API_READ_START, "ps",
					voidp_to_uint64(stream), format);

	va_list arg;
	va_start(arg, format);

	ret = vfscanfp(stream, format, arg);

	AFTER_ORIGINAL_START_END_FILEP(API_ID_fscanf, 'd', ret, ret, stream,
				       FD_API_READ_END, "ps",
				       voidp_to_uint64(stream), format);

	va_end(arg);

	return ret;
}

#if !defined(DA_DEBUG_LOG) && !defined(PRINT_STDOUT)
int printf(const char* format, ...)
{
	static int (*vprintfp)(const char* format, ...);

	BEFORE_ORIGINAL_START_END_NOFD(API_ID_printf, 'd', vprintf, LIBC,
				FD_API_WRITE_START, "s", format);

	va_list arg;
	va_start(arg, format);
	ret = vprintfp(format, arg);

	AFTER_ORIGINAL_START_END_NOFD(API_ID_printf, 'd', ret, ret,
				FD_API_WRITE_END, "s", format);

	va_end(arg);

	return ret;
}
#endif

int scanf(const char* format, ...)
{
	static int (*vscanfp)(const char* format, ...);

	BEFORE_ORIGINAL_START_END_NOFD(API_ID_scanf, 'd', vscanf, LIBC,
				FD_API_READ_START, "s", format);

	va_list arg;
	va_start(arg, format);
	ret = vscanfp(format, arg);

	AFTER_ORIGINAL_START_END_NOFD(API_ID_scanf, 'd', ret, ret,
				 FD_API_READ_END, "s", format);

	va_end(arg);

	return ret;
}

int getchar()
{
	static int (*getcharp)();

	BEFORE_ORIGINAL_START_END_NOFD(API_ID_getchar, 'd', getchar, LIBC,
				FD_API_READ_START, "", 0);

	ret = getcharp();

	AFTER_ORIGINAL_START_END_NOFD(API_ID_getchar, 'd', ret, (ret == EOF ? 0 : 1),
				FD_API_READ_END, "", 0);

	return ret;
}

int putchar(int c)
{
	static int (*putcharp)(int c);

	BEFORE_ORIGINAL_START_END_NOFD(API_ID_putchar, 'd', putchar, LIBC,
				FD_API_WRITE_START, "d", c);

	ret = putcharp(c);

	AFTER_ORIGINAL_START_END_NOFD(API_ID_putchar, 'd', ret, (ret == EOF ? 0 : 1),
				FD_API_WRITE_END, "d", c);

	return ret;
}

char* gets(char* str)
{
	static char* (*getsp)(char* str);
	char* cret;

	BEFORE_ORIGINAL_START_END_NOFD(API_ID_gets, 'p', gets, LIBC,
				FD_API_READ_START, "s", str);

	cret = getsp(str);

	AFTER_ORIGINAL_START_END_NOFD(API_ID_gets, 'p', cret, strlen(cret),
				FD_API_READ_END, "s", str);

	return cret;
}

#if !defined(DA_DEBUG_LOG) && !defined(PRINT_STDOUT)
int puts(const char* str)
{
	static int (*putsp)(const char* str);

	BEFORE_ORIGINAL_START_END_NOFD(API_ID_puts, 'd', puts, LIBC,
				FD_API_WRITE_START, "s", str);

	ret = putsp(str);

	AFTER_ORIGINAL_START_END_NOFD(API_ID_puts, 'd', ret, ret,
				FD_API_WRITE_END, "s", str);

	return ret;
}
#endif



void setbuf(FILE* stream, char* buf)
{
	static void (*setbufp)(FILE* stream, char* buf);

	BEFORE_ORIGINAL_FILE(setbuf, LIBC);

	setbufp(stream, buf);

	AFTER_PACK_ORIGINAL_FILEP(API_ID_setbuf,
				  'v', 0, 0, stream, FD_API_OTHER, "pp",
				  voidp_to_uint64(stream),
				  voidp_to_uint64(buf));
}

void setbuffer(FILE* stream, char* buf, size_t size)
{
	static void (*setbufferp)(FILE* stream, char* buf, size_t size);

	BEFORE_ORIGINAL_FILE(setbuffer, LIBC);

	setbufferp(stream, buf, size);

	AFTER_PACK_ORIGINAL_FILEP(API_ID_setbuffer,
				  'v', 0, size, stream, FD_API_OTHER,
				  "ppx", voidp_to_uint64(stream),
				  voidp_to_uint64(buf), (uint64_t)(size));
}

void setlinebuf(FILE* stream)
{
	static int (*setlinebufp)(FILE* stream);

	BEFORE_ORIGINAL_FILE(setlinebuf, LIBC);

	setlinebufp(stream);

	AFTER_PACK_ORIGINAL_FILEP(API_ID_setlinebuf,
				  'v', 0, 0, stream, FD_API_OTHER, "p",
				  voidp_to_uint64(stream));
}

int setvbuf(FILE* stream, char* buf, int mode, size_t size)
{
	static int (*setvbufp)(FILE* stream, char* buf, int mode, size_t size);

	BEFORE_ORIGINAL_FILE(setvbuf, LIBC);
	ret = setvbufp(stream,buf,mode,size);
	AFTER_PACK_ORIGINAL_FILEP(API_ID_setvbuf,
				  'd', ret, size, stream, FD_API_OTHER,
				  "ppdx",
				  voidp_to_uint64(stream),
				  voidp_to_uint64(buf), mode,
				  (uint64_t)(size));
	return ret;
}
