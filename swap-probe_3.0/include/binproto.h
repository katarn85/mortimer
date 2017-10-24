/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Nikita Kalyazin <n.kalyazin@samsung.com>
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
 * - Samsung RnD Institute Russia
 *
 */

#ifndef __BIN_PROTO_H__
#define __BIN_PROTO_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sys/time.h>

#include <sys/socket.h>
#include "dahelper.h"
#include "api_id_mapping.h"

#define MSG_PROBE_MEMORY 0x0101
#define MSG_PROBE_UICONTROL 0x0102
#define MSG_PROBE_UIEVENT 0x0103
#define MSG_PROBE_RESOURCE 0x0104
#define MSG_PROBE_LIFECYCLE 0x0105
#define MSG_PROBE_SCREENSHOT 0x0106
#define MSG_PROBE_SCENE 0x0107
#define MSG_PROBE_THREAD 0x0108
#define MSG_PROBE_CUSTOM 0x0109
#define MSG_PROBE_SYNC 0x0110
#define MSG_PROBE_NETWORK 0x0111
#define MSG_PROBE_GL 0x0112

#define SCREENSHOT_DIRECTORY	"/tmp/da"

#define MAX_PACK_FILENAME_LEN (4 * 1024)
#define MAX_SHADER_LEN (4 * 1024)
#define ADD_LOCAL_BUF_SIZE (1024)
#define MAX_GL_CONTEXT_VALUE_SIZE (1024)
#define MAX_LOCAL_BUF_SIZE (MAX_SHADER_LEN + ADD_LOCAL_BUF_SIZE)
#define LOCAL_BUF msg_buf

// TODO: remove this copy-paste
#define CALLER_ADDRESS							\
	((void*) __builtin_extract_return_addr(__builtin_return_address(0)))

static inline uint64_t voidp_to_uint64(const void *p)
{
	return (uint64_t)(uintptr_t)p;
}

// COMMON
static inline char *pack_int32(char *to, uint32_t val)
{
	*(uint32_t *)to = val;
	return to + sizeof(uint32_t);
}

static inline char *pack_int64(char *to, uint64_t val)
{
	*(uint64_t *)to = val;
	return to + sizeof(uint64_t);
}

static char __attribute__((used)) *pack_string(char *to, const char *str)
{
	if (!str) {
		*to = '\0';
		return to + 1;
	} else {
		size_t len = strlen(str) + 1;
		strncpy(to, str, len);
		return to + len;
	}
}

static char __attribute__((used)) *pack_bin(char *to, const char *from,
					    uint32_t len)
{
	memcpy(to, from, len);
	return to + len;
}

static inline char *pack_double(char *to, double val)
{
	*(double *)to = val;
	return to + sizeof(double);
}

static inline char *pack_timestamp(char *to)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	to = pack_int32(to, tv.tv_sec);
	to = pack_int32(to, tv.tv_usec * 1000);

	return to;
}

///////////////////////////////////////////////////////////////////
// function: pack_array
///////////////////////////////////////////////////////////////////
// info:
//  Pack array param to buffer
// params:
//  char *to              - destination buffer
//  va_list *args         - array size and array (pointer)
//  uint32_t element_size - size of array element
// return:
//  char *                - destination pointer end after pack
static char *pack_array(char *to, va_list *args, uint32_t element_size)
{
	uint32_t d;
	uint64_t p;

	// get array elements count
	d = va_arg(*args, uint32_t);
	// get array pointer
	p = (unsigned long)(uintptr_t)va_arg(*args, uint64_t);

	// pack original pointer
	memcpy(to, &p, sizeof(p));
	to += sizeof(p);
	// pack array elements count
	memcpy(to, &d, sizeof(d));
	to += sizeof(d);

	// pack array elements
	element_size *= d;
	memcpy(to, (void *)(long)p, element_size);
	to += element_size;

	return to;
}

static char __attribute__((used)) *pack_string_to_file(char *to, const char *st,
						       uint32_t data_len,
						       uint32_t max_len)
{
	if (data_len < max_len) {
		/* pack string to buffer */
		to = pack_string(to, st);
	} else {
		/* pack string to file */
		char template_name[] = SCREENSHOT_DIRECTORY "/swap_XXXXXX";
		char dst_path_pack[MAX_PATH_LENGTH];
		FILE *file;
		mktemp(template_name);
		file = fopen(template_name, "w");
		if (file != NULL) {
			fwrite(st, data_len, 1, file);
			fclose(file);
		}
		snprintf(dst_path_pack, sizeof(dst_path_pack), "FILE:%s", template_name);
		to = pack_string(to, dst_path_pack);
	}
	return to;
}

static char __attribute__((used)) *pack_value_by_type(char *to, const char **t, va_list *args)
{
	uint8_t c;
	uint8_t cs;
	uint32_t d;
	uint64_t x;
	uint64_t p;
	float f;
	double w;
	char *s;
	int n;

	*to++ = **t;

	switch (**t) {
	case 'b':
	case 'c':
		c = (uint8_t)va_arg(*args, uint32_t);
		memcpy(to, &c, sizeof(c));
		to += sizeof(c);
		break;
	case 'd':
		d = va_arg(*args, uint32_t);
		memcpy(to, &d, sizeof(d));
		to += sizeof(d);
		break;
	case 'x':
		x = 0; // real value may be less then uint64_t
		x = (unsigned long)(uint64_t)va_arg(*args, uint64_t);
		memcpy(to, &x, sizeof(x));
		to += sizeof(x);
		break;
	case 'p':
		p = 0; // real value may be less then uint64_t
		p = (unsigned long)(uintptr_t)va_arg(*args, uint64_t);
		memcpy(to, &p, sizeof(p));
		to += sizeof(p);
		break;
	case 'f':
		f = (float)va_arg(*args, double);
		memcpy(to, &f, sizeof(f));
		to += sizeof(f);
		break;
	case 'w':
		w = va_arg(*args, double);
		memcpy(to, &w, sizeof(w));
		to += sizeof(w);
		break;
	case 's':
		cs = *((*t)+1);
		s = va_arg(*args, char *);

		switch (cs) {
			case '4': //"s4" - pack 4K or to file
				(*t)++;
				to = pack_string_to_file(to, s, strlen(s),
							 MAX_PACK_FILENAME_LEN);
				break;
			case '0': //"s0" - pack 256 bytes
				(*t)++;
				n = strlen(s) + 1;
				if (n >= 255)
					n = 255;
				strncpy(to, s, n);
				to[n-1] = '\0';
				to += n;
				break;
			default: //"s" - pack all string
				n = strlen(s) + 1;
				strncpy(to, s, n);
				to += n;
				break;
		}

		break;
	case 'v':
	case 'n':
		break;
	/* pack arrays params */
	case 'D':
		/* array of 'd' uint32_t */
		to = pack_array(to, args, sizeof(d));
		break;
	case 'F':
		/* array of 'f' float */
		to = pack_array(to, args, sizeof(f));
		break;
	case 'W':
		/* array of 'w' double */
		to = pack_array(to, args, sizeof(w));
		break;
	default: {
		PRINTERR("ERROR PACK #%d '%c'!!!", **t, **t);
		to--;
		break;
		}
	}

	(*t)++;
	return to;
}

static char __attribute__((used)) *pack_args(char *to, const char *fmt, ...)
{
	va_list args;
	uint32_t num = 0;
	const char *t = fmt;
	char *size_p = to;

	//put dummy num value
	memcpy(to, &num, sizeof(num));
	to += sizeof(num);

	va_start(args, fmt);

	for (t = fmt; *t != '\0';) {
		to = pack_value_by_type(to, &t, &args);
		num++;
	}

	//set real num value
	memcpy(size_p, &num, sizeof(num));

	va_end(args);

	return to;
}

static char __attribute__((used)) *pack_ret(char *to, char ret_type, ...)
{
	va_list args;
	char fmt[] = {ret_type, 0};
	const char *fmtp = (const char *)fmt;

	va_start(args, ret_type);
	to = pack_value_by_type(to, &fmtp, &args);
	va_end(args);

	return to;
}

#define BUF_PTR p
#define RET_PTR ret_p
#define PACK_INT32(val)				\
	BUF_PTR = pack_int32(BUF_PTR, val);
#define PACK_INT64(val)				\
	BUF_PTR = pack_int64(BUF_PTR, val);
#define PACK_STRING(str)			\
	BUF_PTR = pack_string(BUF_PTR, str);

#define PACK_COMMON_BEGIN(msg_id, api_id, fmt, ...)		\
	do {	/* PACK_COMMON_BEGIN*/				\
		BUF_PTR = pack_int32(BUF_PTR, msg_id);		/* msg id */	\
		BUF_PTR = pack_int32(BUF_PTR, 0);		/* sequence */	\
		BUF_PTR = pack_timestamp(BUF_PTR);		/* timestamp */	\
		BUF_PTR = pack_int32(BUF_PTR, 0);		/* length */	\
		BUF_PTR = pack_int32(BUF_PTR, api_id);		/* API id */	\
		BUF_PTR = pack_int32(BUF_PTR, _getpid());	/* PID */	\
		BUF_PTR = pack_int32(BUF_PTR, _gettid());	/* call pc*/\
		BUF_PTR = pack_args(BUF_PTR, fmt, __VA_ARGS__);	/* args */	\
		RET_PTR = BUF_PTR;		\
	} while (0)

#define PACK_COMMON_END(ret_type, ret, errn, intern_call)			\
	do {	/* PACK_COMMON_END */						\
		BUF_PTR = pack_ret(RET_PTR, ret_type, (uintptr_t)ret); /* return val */ \
		BUF_PTR = pack_int64(BUF_PTR, (uint64_t)errn);	/* errno */	\
		BUF_PTR = pack_int32(BUF_PTR, (uint32_t)intern_call);	/* internal call*/	\
		BUF_PTR = pack_int64(BUF_PTR, (uintptr_t)CALLER_ADDRESS); /*caller addr*/\
		BUF_PTR = pack_int32(BUF_PTR, 0);	/* reserved */		\
		BUF_PTR = pack_int32(BUF_PTR, 0);	/* reserved */		\
	} while (0)

#define PACK_RETURN_END(ret_type, ret)						\
		RET_PTR = pack_ret(RET_PTR, ret_type, (uintptr_t)ret); /* return val */

#define PACK_MEMORY(size, memory_api_type, addr)				\
	do {	/* PACK_MEMORY */						\
		BUF_PTR = pack_int64(BUF_PTR, size);	/* alloc size */	\
		BUF_PTR = pack_int32(BUF_PTR, memory_api_type);	/* alloc type */\
		BUF_PTR = pack_int64(BUF_PTR, (uintptr_t)addr);	/* alloc addr */\
	} while (0)

#define PACK_UICONTROL(control)						\
	do {								\
		if (unlikely(control == NULL)) {			\
			BUF_PTR = pack_string(BUF_PTR, "");		\
			BUF_PTR = pack_string(BUF_PTR, "");		\
			BUF_PTR = pack_int64(BUF_PTR, 0);		\
		} else {						\
			char *type = NULL, *name = NULL;		\
			if (find_uiobject_hash((void*)(control),	\
					     &type, &name) == 1) {	\
				BUF_PTR = pack_string(BUF_PTR, type);	\
				BUF_PTR = pack_string(BUF_PTR, name);	\
			} else {					\
				BUF_PTR = pack_string(BUF_PTR, "");	\
				BUF_PTR = pack_string(BUF_PTR, "");	\
			}						\
			BUF_PTR = pack_int64(BUF_PTR, (uintptr_t)(control)); \
		}							\
	} while(0)

#define PACK_UIEVENT(event_type, detail_type, x, y, info1, info2)	\
	do {	/* PACK_UIEVENT */					\
		BUF_PTR = pack_int32(BUF_PTR, event_type);		\
		BUF_PTR = pack_int32(BUF_PTR, detail_type);		\
		BUF_PTR = pack_int32(BUF_PTR, x);			\
		BUF_PTR = pack_int32(BUF_PTR, y);			\
		BUF_PTR = pack_string(BUF_PTR, info1);			\
		BUF_PTR = pack_int32(BUF_PTR, info2);			\
	} while (0)

#define PACK_RESOURCE(size, fd_value, fd_api_type, file_size,	\
		      file_path)					\
	do {	/* PACK_RESOURCE */					\
		BUF_PTR = pack_int64(BUF_PTR, size);			\
		BUF_PTR = pack_int64(BUF_PTR, fd_value);		\
		BUF_PTR = pack_int32(BUF_PTR, fd_api_type);		\
		BUF_PTR = pack_int64(BUF_PTR, file_size);		\
		BUF_PTR = pack_string_to_file(BUF_PTR, file_path,	\
					      strlen(file_path),	\
					      MAX_PACK_FILENAME_LEN);	\
	} while (0)

#define PACK_SCREENSHOT(image_file_path, orientation)				\
		do {	/* PACK_SCREENSHOT */					\
			  BUF_PTR = pack_string(BUF_PTR, image_file_path); 	\
			  BUF_PTR = pack_int32(BUF_PTR, orientation);		\
		} while (0)

#define PACK_SCENE(scene_name, formid, pform, panelid, ppanel, transition, user)	\
	do {										\
		BUF_PTR = pack_string(BUF_PTR,  scene_name);				\
		if (unlikely(pform == NULL)) {						\
			BUF_PTR = pack_string(BUF_PTR, "");				\
			BUF_PTR = pack_int64(BUF_PTR, 0);				\
		} else {								\
			char *type = NULL, *name = NULL;				\
			if (find_uiobject_hash((void*)(pform), &type, &name) == 1) {	\
				BUF_PTR = pack_string(BUF_PTR, name);			\
			} else {							\
				BUF_PTR = pack_string(BUF_PTR, "");			\
			}								\
			BUF_PTR = pack_int64(BUF_PTR, (uintptr_t)pform); 		\
		}									\
		if (unlikely(ppanel == NULL)) {						\
			BUF_PTR = pack_string(BUF_PTR, "");				\
			BUF_PTR = pack_int64(BUF_PTR, 0);				\
		} else {								\
			char *type = NULL, *name = NULL;				\
			if (find_uiobject_hash((void*)(ppanel), &type, &name) == 1) {	\
				BUF_PTR = pack_string(BUF_PTR, name);			\
			} else {							\
				BUF_PTR = pack_string(BUF_PTR, "");			\
			}								\
			BUF_PTR = pack_int64(BUF_PTR, (uintptr_t)(ppanel)); 		\
		}									\
		BUF_PTR = pack_int64(BUF_PTR, transition);				\
		BUF_PTR = pack_int64(BUF_PTR, user);					\
	} while(0)

#define PACK_THREAD(thread_id, thread_type, api_type, class_name)	\
	 do {								\
	if(thread_type == THREAD_PTHREAD) {	                        \
	    BUF_PTR = pack_int64(BUF_PTR, thread_id);			\
	    BUF_PTR = pack_int64(BUF_PTR, 0);				\
	} else if(thread_type == THREAD_TIZENTHREAD_WORKER		\
		    || thread_type == THREAD_TIZENTHREAD_EVENTDRIVEN) {   \
	    BUF_PTR = pack_int64(BUF_PTR, 0);				\
	    BUF_PTR = pack_int64(BUF_PTR, thread_id);			\
	} else {							\
	    BUF_PTR = pack_int64(BUF_PTR, 0);				\
	    BUF_PTR = pack_int64(BUF_PTR, 0);				\
	}				                                \
	BUF_PTR = pack_int32(BUF_PTR, thread_type);	                \
	BUF_PTR = pack_int32(BUF_PTR, api_type);		        \
	BUF_PTR = pack_string(BUF_PTR, class_name);			\
	} while (0)

#define PACK_CUSTOM(handle, type, name, color, value)		\
	do {						    	\
		BUF_PTR = pack_int32(BUF_PTR, handle);		\
		BUF_PTR = pack_int32(BUF_PTR, type);		\
		BUF_PTR = pack_string(BUF_PTR, name);		\
		BUF_PTR = pack_int32(BUF_PTR, color);		\
		BUF_PTR = pack_double(BUF_PTR, value);		\
	} while (0)

#define PACK_SYNC(sync_val, sync_type, api_type)		     \
	do {						     	     \
		BUF_PTR = pack_int64(BUF_PTR, (uintptr_t)sync_val);  \
		BUF_PTR = pack_int32(BUF_PTR, sync_type);	     \
		BUF_PTR = pack_int32(BUF_PTR, api_type);	     \
	} while (0)

/* TODO maloc/free for each event turns out expensive: think of buffer
 * allocator implementation */
#define PREPARE_LOCAL_BUF()			\
		char *LOCAL_BUF = (char *)(*real_malloc)(MAX_LOCAL_BUF_SIZE);	\
		char *BUF_PTR = LOCAL_BUF;			\
		char *RET_PTR = NULL

#define PREPARE_LOCAL_BUF_THOUGH(BUFF)				\
		char *LOCAL_BUF = BUFF;				\
		char *BUF_PTR = LOCAL_BUF;			\
		char *RET_PTR = NULL

#define MSG_LEN_OFFSET 16
#define MSG_HDR_LEN 20

#define SET_MSG_LEN()							\
		*(uint32_t *)(msg_buf + MSG_LEN_OFFSET) = (p - msg_buf) - MSG_HDR_LEN;

#define GET_MSG_LEN()							\
		(p - msg_buf) - MSG_HDR_LEN

#define FLUSH_LOCAL_BUF()						\
		SET_MSG_LEN();						\
		send(gTraceInfo.socket.daemonSock, msg_buf, (p - msg_buf), 0); \
		free(LOCAL_BUF);					\
		LOCAL_BUF = NULL

// =========================== post block macro ===========================

#define POST_PACK_PROBEBLOCK_BEGIN()					\
	newerrno = errno;						\
	if(postBlockBegin(blockresult)) {

#define POST_PACK_PROBEBLOCK_END() 					\
		postBlockEnd();						\
	}								\
	errno = (newerrno != 0) ? newerrno : olderrno

#define POST_PACK_PROBEBLOCK_ADD_END()					\
		preBlockEnd();						\
	}								\
	errno = (newerrno != 0) ? newerrno : olderrno


/* int main(int argc, char **argv) */
/* { */
/* 	char buf[1024]; */
/* 	char *p = buf; */

/* 	p = PACK_COMMON_BEGIN(p, 42, "cdxpfws", 'a', 10, (uint64_t)80, */
/* 			      (uint64_t)0, 0.19, 0.33, "hello!"); */
/* 	p = PACK_COMMON_END('p', p, 0); */

/* 	int fd = creat("out.bin", 0644); */
/* 	if (fd == -1) { */
/* 		printf("error\n"); */
/* 		exit(1); */
/* 	} */

/* 	write(fd, buf, p - buf); */
/* 	close(fd); */

/* 	return 0; */
/* } */

extern int _init_(void);
extern void _uninit_(void);

#endif /* __BIN_PROTO_H__ */
