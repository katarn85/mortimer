/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Woojin Jung <woojin2.jung@samsung.com>
 * Jaewon Lim <jaewon81.lim@samsung.com>
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

#ifndef __DA_SYNC_H__
#define __DA_SYNC_H__

#include "daprobe.h"

#ifdef __cplusplus
extern "C"{
#endif

int real_pthread_mutex_lock(pthread_mutex_t *mutex);
int real_pthread_mutex_unlock(pthread_mutex_t *mutex);
int real_pthread_setcancelstate(int state, int *oldstate);

#define BEFORE_ORIGINAL_SYNC(FUNCNAME, LIBNAME) 	\
	DECLARE_VARIABLE_STANDARD;						\
	GET_REAL_FUNC(FUNCNAME, LIBNAME);				\
	PRE_PROBEBLOCK()

#define AFTER_PACK_ORIGINAL_SYNC(API_ID, RTYPE, RVAL, SYNCVAL, SYNCTYPE, APITYPE, INPUTFORMAT, ...) 	\
	POST_PACK_PROBEBLOCK_BEGIN();									\
	PREPARE_LOCAL_BUF();										\
	PACK_COMMON_BEGIN(MSG_PROBE_SYNC, API_ID, INPUTFORMAT, __VA_ARGS__);				\
	PACK_COMMON_END(RTYPE, RVAL, errno, blockresult);						\
	PACK_SYNC(SYNCVAL, SYNCTYPE, APITYPE);								\
	FLUSH_LOCAL_BUF();										\
	POST_PACK_PROBEBLOCK_END()

#ifdef __cplusplus
}
#endif

#endif // __DA_SYNC_H__

