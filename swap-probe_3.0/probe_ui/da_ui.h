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
 *
 */

#ifndef __DA_SNAPSHOT_H__
#define __DA_SNAPSHOT_H__

#include "daprobe.h"
#include "dahelper.h"

/********************************************************************
 * SNAPSHOT PROBE MACRO
 *
 * log format
 *		SeqNumber`,ApiName`,Time`,Pid`,Tid`,InputParm`,Return`,PCAddr`,Error`,Size`,FUNCTYPE`,\n
 * 		callstack_start`,callstack`,callstack_end
 *
 * FUNCTYPE
 *		SNAPSHOT_API_TYPE_PAGER,
 *		SNAPSHOT_API_TYPE_CONTROLBAR,
 *		SNAPSHOT_API_TYPE_NAVIFRAME,
 *		SNAPSHOT_API_TYPE_OTHER
 *
 **********************************************************************/

#define BEFORE_ORIGINAL_SNAPSHOT(FUNCNAME, LIBNAME)		\
	Evas_Object* ret;									\
	GET_REAL_FUNC(FUNCNAME, LIBNAME)

#define AFTER_ORIGINAL_SNAPSHOT(EVASOBJECT)							\
	do {															\
		probeBlockStart();											\
		evas_event_callback_add(evas_object_evas_get(EVASOBJECT),	\
			EVAS_CALLBACK_RENDER_FLUSH_POST,						\
			_cb_render_post, NULL);									\
		probeBlockEnd();											\
	} while(0)

#endif // __DA_SNAPSHOT_H__
