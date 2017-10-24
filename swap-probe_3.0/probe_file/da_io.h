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

#ifndef __DA_IO_H__
#define __DA_IO_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#include "daprobe.h"

/********************************************************************
 * RESOURCE PROBE MACRO
 *
 * log format
 * 		SeqNumber`,ApiName`,Time`,Pid`,Tid`,InputParm`,Return`,PCAddr`,Error`,
 *		Size`,FDValue`,FDType`,FDApiType`,					\n
 *		callstack_start`,callstack`,callstack_end
 *
 * fd type
 *		FD_TYPE_FILE, FD_TYPE_SOCKET
 *
 * fd api type
 *		FD_API_TYPE_OPEN, FD_API_TYPE_CLOSE, FD_API_TYPE_READ, FD_API_TYPE_WRITE,
 *		FD_API_TYPE_DIRECTORY, FD_API_TYPE_PERMISSION, FD_API_TYPE_OTHER,
 *		FD_API_TYPE_SEND, FD_API_TYPE_RECEIVE, FD_API_TYPE_OPTION, FD_API_TYPE_MANAGE
 *
 ************************************************************************/

// ===================================================================
// macro for file probe
// ===================================================================

#define GET_FD_FROM_FILEP(FILEP)				\
	do {							\
		_fd = (FILEP != NULL) ? fileno(FILEP) : -1;	\
	} while (0);

#define DECLARE_VARIABLE_FD					\
		DECLARE_VARIABLE_STANDARD;			\
		const char *_filepath = "";			\
		int __attribute((unused)) _fd = -1;		\
		ssize_t __attribute((unused)) _filesize = 0;	\
		int __attribute((unused)) _fstatret = -1;	\
		struct stat __attribute((unused)) _statbuf

// =================================================================
// POST_PROBEBLOCK_MIDDLE for file
// =================================================================

#define POST_PACK_PROBEBLOCK_MIDDLE_FD(SIZE, FD, APITYPE)				\
	do {										\
		PACK_RESOURCE(SIZE, FD, APITYPE,					\
			      (_fstatret == 0 ? _statbuf.st_size : 0), _filepath);	\
		FLUSH_LOCAL_BUF();							\
	} while (0)

#define POST_PACK_PROBEBLOCK_MIDDLE_NOFD(SIZE, APITYPE)		\
	do {							\
		PACK_RESOURCE(SIZE, 0, APITYPE, 0, _filepath);	\
		FLUSH_LOCAL_BUF();				\
	} while (0)

// ==================================================================
// BEFORE_ORIGINAL macro for file
// ==================================================================

#define BEFORE_ORIGINAL_FILE(FUNCNAME, LIBNAME)		\
	DECLARE_VARIABLE_FD;				\
	GET_REAL_FUNC(FUNCNAME, LIBNAME);		\
	PRE_PROBEBLOCK();

#define BEFORE_ORIGINAL_FILE_NOFILTER(FUNCNAME, LIBNAME)	\
	DECLARE_VARIABLE_FD;					\
	GET_REAL_FUNC(FUNCNAME, LIBNAME);			\
	bfiltering = false;					\
	PRE_PROBEBLOCK()

// ==================================================================
// macro for filesize
// ==================================================================

#define DEFINE_FILESIZE_FD(fd) _fd = (fd); _filesize = get_fd_filesize(_fd);
#define DEFINE_FILESIZE_FP(fp) _fd = checked_fileno(fp); _filesize = get_fd_filesize(_fd);
#define DEFINE_FILESIZE_0() _fd = _filesize = 0;

/*!
 * Macro AFTER_PACK_ORIGINAL_FD is used in and only in functions, which should report
 * only about regular files or sockets, so this logic implemented in macro.
 * Watch out when reusing it somewhere else
 */
#define AFTER_PACK_ORIGINAL_FD(API_ID, RTYPE, RVAL, SIZE, FD, APITYPE, INPUTFORMAT, ...)	\
do {												\
	POST_PACK_PROBEBLOCK_BEGIN();								\
	_fstatret = fstat(FD, &_statbuf);							\
	if (stat_regular_or_socket_p(&_statbuf)) {						\
		PREPARE_LOCAL_BUF();								\
		PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE, API_ID, INPUTFORMAT, __VA_ARGS__);	\
		PACK_COMMON_END(RTYPE, RVAL, newerrno, blockresult);				\
		POST_PACK_PROBEBLOCK_MIDDLE_FD(SIZE, FD, APITYPE);				\
	}											\
	POST_PACK_PROBEBLOCK_END();								\
} while(0)

#define AFTER_PACK_ORIGINAL_FD_MIDDLE(API_ID, RTYPE, RVAL, SIZE, FD, APITYPE, INPUTFORMAT, ...)	\
do {												\
	POST_PACK_PROBEBLOCK_BEGIN();								\
	_fstatret = fstat(FD, &_statbuf);							\
	if (stat_regular_or_socket_p(&_statbuf)) {						\
		PREPARE_LOCAL_BUF();								\
		PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE, API_ID, INPUTFORMAT, __VA_ARGS__);	\
		PACK_COMMON_END(RTYPE, RVAL, newerrno, blockresult);				\
		POST_PACK_PROBEBLOCK_MIDDLE_FD(SIZE, FD, APITYPE);				\
	}											\
	POST_PACK_PROBEBLOCK_ADD_END();								\
} while(0)

#define AFTER_PACK_ORIGINAL_NOFD(API_ID, RTYPE, RVAL, SIZE, APITYPE, INPUTFORMAT, ...)	\
do {											\
	POST_PACK_PROBEBLOCK_BEGIN();							\
	PREPARE_LOCAL_BUF();								\
	PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE, API_ID, INPUTFORMAT, __VA_ARGS__);	\
	PACK_COMMON_END(RTYPE, RVAL, newerrno, blockresult);				\
	POST_PACK_PROBEBLOCK_MIDDLE_NOFD(SIZE, APITYPE);				\
	POST_PACK_PROBEBLOCK_END();							\
} while(0)

#define AFTER_PACK_ORIGINAL_FILEP(API_ID, RTYPE, RVAL, SIZE, FILEP, APITYPE, INPUTFORMAT, ...) \
do {											\
	POST_PACK_PROBEBLOCK_BEGIN();							\
	GET_FD_FROM_FILEP(FILEP);							\
	if(_fd != -1) { 								\
		_fstatret = fstat(_fd, &_statbuf);					\
	}										\
	PREPARE_LOCAL_BUF();								\
	PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE, API_ID, INPUTFORMAT, __VA_ARGS__);	\
	PACK_COMMON_END(RTYPE, RVAL, newerrno, blockresult);				\
	POST_PACK_PROBEBLOCK_MIDDLE_FD(SIZE, _fd, APITYPE);				\
	POST_PACK_PROBEBLOCK_END();							\
} while(0)

/*!
 * Macros {BEFORE,AFTER}_ORIGINAL_START_END_FD only used in {p,v,}{read,write}
 * in which we should log only about files and sockets. Given this lucky
 * coincidence we can implement this login in macros. As such, they should not
 * be used in functions for which this logic do not apply.
 */
static inline bool stat_regular_or_socket_p(struct stat *buf)
{
  return S_ISREG(buf->st_mode) || S_ISSOCK(buf->st_mode);
}
#define BEFORE_ORIGINAL_START_END_FD(API_ID, RTYPE, FUNCNAME, LIBNAME, FD, APITYPE, INPUTFORMAT, ...)	\
	DECLARE_VARIABLE_FD;									\
	GET_REAL_FUNC(FUNCNAME, LIBNAME);							\
	PRE_PROBEBLOCK_BEGIN();									\
	_fstatret = fstat(FD, &_statbuf);							\
	if (stat_regular_or_socket_p(&_statbuf)) {						\
		DEFINE_FILESIZE_FD(fd);								\
		PREPARE_LOCAL_BUF();								\
		PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE, API_ID, INPUTFORMAT, __VA_ARGS__);	\
		PACK_COMMON_END(RTYPE, 0, 0, blockresult);					\
		PACK_RESOURCE(0, FD, APITYPE, _filesize, _filepath);				\
		FLUSH_LOCAL_BUF();								\
	}								\
	PRE_PROBEBLOCK_END();								\

#define AFTER_ORIGINAL_START_END_FD(API_ID, RTYPE, RVAL, SIZE, FD, APITYPE, INPUTFORMAT, ...)		\
do {									\
	POST_PACK_PROBEBLOCK_BEGIN();					\
	setProbePoint(&probeInfo);					\
	_fstatret = fstat(FD, &_statbuf);				\
	if (stat_regular_or_socket_p(&_statbuf)) {			\
		PREPARE_LOCAL_BUF();								\
		PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE, API_ID, INPUTFORMAT, __VA_ARGS__); \
		PACK_COMMON_END(RTYPE, RVAL, newerrno, blockresult);		\
		PACK_RESOURCE(SIZE, FD, APITYPE, _filesize, _filepath);	\
		FLUSH_LOCAL_BUF();								\
	}								\
	POST_PACK_PROBEBLOCK_END();								\
} while(0)



#define BEFORE_ORIGINAL_START_END_NOFD(API_ID, RTYPE, FUNCNAME, LIBNAME, APITYPE, INPUTFORMAT, ...)	\
	DECLARE_VARIABLE_FD;								\
	GET_REAL_FUNC(FUNCNAME, LIBNAME);						\
	PRE_PROBEBLOCK_BEGIN();								\
	DEFINE_FILESIZE_0();								\
	PREPARE_LOCAL_BUF();								\
	PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE, API_ID, INPUTFORMAT, __VA_ARGS__);	\
	PACK_COMMON_END(RTYPE, 0, 0, blockresult);					\
	POST_PACK_PROBEBLOCK_MIDDLE_NOFD(0, APITYPE);					\
	PRE_PROBEBLOCK_END()

#define BEFORE_ORIGINAL_START_END_FILEP(API_ID, RTYPE, FUNCNAME, LIBNAME, FILEP, APITYPE, INPUTFORMAT, ...)	\
	DECLARE_VARIABLE_FD;									\
	GET_REAL_FUNC(FUNCNAME, LIBNAME);							\
	PRE_PROBEBLOCK_BEGIN();									\
	GET_FD_FROM_FILEP(FILEP);								\
	if(_fd != -1) { 									\
		_fstatret = fstat(_fd, &_statbuf);						\
	}											\
	PREPARE_LOCAL_BUF();									\
	PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE, API_ID, INPUTFORMAT, __VA_ARGS__);		\
	PACK_COMMON_END(RTYPE, 0, 0, blockresult);						\
	POST_PACK_PROBEBLOCK_MIDDLE_FD(0, _fd, APITYPE);					\
	PRE_PROBEBLOCK_END()

#define AFTER_ORIGINAL_START_END_NOFD(API_ID, RTYPE, RVAL, SIZE, APITYPE, INPUTFORMAT, ...)	\
do {												\
	POST_PACK_PROBEBLOCK_BEGIN();								\
	setProbePoint(&probeInfo);								\
	PREPARE_LOCAL_BUF();									\
	PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE, API_ID, INPUTFORMAT, __VA_ARGS__);		\
	PACK_COMMON_END(RTYPE, RVAL, newerrno, blockresult);					\
	POST_PACK_PROBEBLOCK_MIDDLE_NOFD(SIZE, APITYPE);					\
	POST_PACK_PROBEBLOCK_END();								\
} while(0)

#define AFTER_ORIGINAL_START_END_FILEP(API_ID, RTYPE, RVAL, SIZE, FILEP, APITYPE, INPUTFORMAT, ...)	\
do {												\
	POST_PACK_PROBEBLOCK_BEGIN();								\
	setProbePoint(&probeInfo);								\
	GET_FD_FROM_FILEP(FILEP);								\
	if(_fd != -1) { 									\
		_fstatret = fstat(_fd, &_statbuf);						\
	}											\
	PREPARE_LOCAL_BUF();									\
	PACK_COMMON_BEGIN(MSG_PROBE_RESOURCE, API_ID, INPUTFORMAT, __VA_ARGS__);		\
	PACK_COMMON_END(RTYPE, RVAL, newerrno, blockresult);					\
	POST_PACK_PROBEBLOCK_MIDDLE_FD(SIZE, _fd, APITYPE);					\
	POST_PACK_PROBEBLOCK_END();								\
} while(0)


static inline ssize_t get_fd_filesize(int fd)
{
	/**
	 * Calling library function on invalid file descriptiors is okay,
	 * for such cases we assume size == 0
	 * FIXME: Separate empty files and invalid descriptors.
	 **/
	struct stat buf;
	int err = fstat(fd, &buf);
	if (err)
		return 0;
	return buf.st_size ?: 0;
}

static inline int checked_fileno(FILE *fp)
{
	assert((fp != NULL) &&
	       "This function (checked_fileno) is only called from probes\n"
	       "on library functions. Passing NULL instead of file pointer\n"
	       "to standart C functions is undefined behavior\n"
	       "and as such fatal error.\n");
	return fileno(fp);
}


#endif	// __DA_IO_H__
