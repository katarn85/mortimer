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

#ifndef __PROBEINFO_H__
#define __PROBEINFO_H__

#ifdef __cplusplus
extern "C"{
#endif

#define		VT_INT			'd'
#define		VT_UINT			'd'
#define		VT_LONG			'x'
#define		VT_ULONG		'x'
#define		VT_STR			's'
#define		VT_CHAR			'c'
#define		VT_PTR			'p'
#define		VT_NULL			'x'
#define		VT_OFF_T		'd'
#define		VT_SIZE_T		'x'
#define		VT_SSIZE_T		'x'
#define		VT_SOCKLEN_T		'd'
#define		VT_UINT16_T		'd'
#define		VT_UINT32_T		'd'
#define		VT_UINT64_T		'x'
#define		VT_MODE_T		'd'
#define		VT_DEV_T		'd'

/* #define		LC_MEMORY		1 */
/* #define		LC_UICREATE		2 */
/* #define		LC_UIEVENT		3 */
/* #define		LC_USERFUNC		4 */
/* #define		LC_RESOURCE		5 */
/* #define		LC_LIFECYCLE		6 */
/* #define		LC_SNAPSHOT		7 */
/* #define		LC_SCENE		8 */
/* #define		LC_DEVICE		9 */
/* #define		LC_ALLOCMEM		10 */
/* #define		LC_SAMPLE		11 */
/* #define		LC_THREAD		12 */
/* #define		LC_CUSTOM		13 */
/* #define		LC_SYNC			14 */
/* #define		LC_SOCKET		15 */
/* #define		LC_GLES20		16 */


#define		FD_API_OPEN			0
#define		FD_API_CLOSE			1
#define		FD_API_READ_START		2
#define		FD_API_READ_END 		3
#define		FD_API_WRITE_START		4
#define		FD_API_WRITE_END		5
#define		FD_API_DIRECTORY		6
#define		FD_API_PERMISSION		7
#define		FD_API_OTHER			8
#define		FD_API_SEND			9
#define		FD_API_RECEIVE			10
#define		FD_API_OPTION			11
#define		FD_API_MANAGE			12
#define		FD_API_LOCK_START		14
#define		FD_API_LOCK_END			15
#define		FD_API_LOCK_RELEASE		16


#define		SOCKET_API_FD_OPEN		0
#define		SOCKET_API_FD_CLOSE		1
#define		SOCKET_API_RECV_START		2
#define		SOCKET_API_RECV_END		3
#define		SOCKET_API_SEND_START		4
#define		SOCKET_API_SEND_END		5
#define		SOCKET_API_BIND			6
#define		SOCKET_API_LISTEN		7
#define		SOCKET_API_CONNECT		8
#define		SOCKET_API_ACCEPT_START		9
#define		SOCKET_API_ACCEPT_END		10
#define		SOCKET_API_EVENT_START		11
#define		SOCKET_API_EVENT_END		12
#define		SOCKET_API_OTHER		13
#define		HTTP_API_SESSION_CONSTRUCT	14
#define		HTTP_API_SESSION_CLOSE		15
#define		HTTP_API_TRANSACTION_OPEN	16
#define		HTTP_API_TRANSACTION_CLOSE	17
#define		HTTP_API_ALLOCATION		18
#define		HTTP_API_SUBMIT			19
#define		HTTP_API_REQUEST		20
#define		HTTP_API_RESPONSE		21
#define		HTTP_API_OTHER			22
#define		HTTP_API_CLOSE			23


#define		MEMORY_API_ALLOC	0
#define		MEMORY_API_FREE		1
#define		MEMORY_API_MANAGE	2

//#define		SNAPSHOT_API_WIN		0
//#define		SNAPSHOT_API_CONTROLBAR		1
//#define		SNAPSHOT_API_NAVIFRAME		2
//#define		SNAPSHOT_API_PAGER		3

#define 	EVENT_TYPE_DOWN		0
#define 	EVENT_TYPE_UP		1
#define 	EVENT_TYPE_MOVE		2

#define		USERFUNC_ENTER		0
#define		USERFUNC_EXIT		1

#define		THREAD_PTHREAD			0
#define		THREAD_TIZENTHREAD_WORKER		1
#define		THREAD_TIZENTHREAD_EVENTDRIVEN	2

#define THREAD_CLASS_BLANK ""

#define		THREAD_API_NEW				0
#define		THREAD_API_START			1
#define		THREAD_API_STOP				2
#define		THREAD_API_EXIT				3
#define		THREAD_API_WAIT_START			4
#define		THREAD_API_WAIT_END			5
#define		THREAD_API_INTERNAL_START		6
#define		THREAD_API_INTERNAL_STOP		7
#define		THREAD_API_OTHER			8

#define 	SYNC_TIZEN_MUTEX				0
#define		SYNC_TIZEN_MONITOR			1
#define		SYNC_TIZEN_SEMAPHORE			2
#define		SYNC_PTHREAD_MUTEX			3
#define		SYNC_PTHREAD_COND_VARIABLE		4
#define		SYNC_PTHREAD_RWLOCK			5
#define		SYNC_PTHREAD_SPINLOCK			6
#define		SYNC_PTHREAD_BARRIER			7

#define		SYNC_API_NEW				0
#define		SYNC_API_ACQUIRE_WAIT_START		1
#define		SYNC_API_ACQUIRE_WAIT_END		2
#define		SYNC_API_RELEASE			3
#define		SYNC_API_TRY_ACQUIRE			4
#define		SYNC_API_COND_WAIT_START		5
#define		SYNC_API_COND_WAIT_END			6
#define		SYNC_API_NOTIFY				7
#define		SYNC_API_NOTIFY_ALL			8
#define		SYNC_API_OTHER				9

enum MessageType
{
	MSG_DEVICE = 1,
	MSG_TIME,
	MSG_SAMPLE,
	MSG_LOG = 5,
	MSG_IMAGE = 6,
	MSG_TERMINATE = 7,
	MSG_PID = 8,
	MSG_MSG = 9,
	MSG_ALLOC = 10,
	MSG_ERROR = 11,
	MSG_WARNING = 12,
	MSG_STOP = 101,
	MSG_CONFIG = 103,
	MSG_CAPTURE_SCREEN= 108,
	MSG_MAPS_INST_LIST= 109
};

enum DaOptions
{
	OPT_ALWAYSOFF		=	0x000000000,
	OPT_ALLOC		=	0x000000008,
	OPT_FILE		=	0x000000010,
	OPT_THREAD		=	0x000000020,
	OPT_UI			=	0x000000040,
	OPT_SNAPSHOT		=	0x000000080,
	OPT_EVENT		=	0x000000100,
	OPT_RECORD		=	0x000000200,
	OPT_NETWORK		=	0x000020000,
	OPT_GLES		=	0x000040000,
	OPT_ALWAYSON		=	0xfffffffff,
	OPT_ALLOC_ALWAYS	=	0x010000000,
	OPT_FILE_ALWAYS		=	0x020000000,
	OPT_THREAD_ALWAYS	=	0x040000000,
	OPT_UI_ALWAYS		=	0x080000000,
	OPT_NETWORK_ALWAYS	=	0x100000000ULL,
	OPT_GLES_ALWAYS		=	0x200000000ULL
};


#ifdef __cplusplus
}
#endif

#endif
