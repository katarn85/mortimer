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

#ifndef __DA_EVENT_H__
#define __DA_EVENT_H__

#include "daprobe.h"

/*******************************************************************
 * Type definition
 *******************************************************************/

enum __event_type
{
	_EVENT_KEY = 1,
	_EVENT_TOUCH = 2,
	_EVENT_GESTURE = 3,
	_EVENT_ORIENTATION = 4,
	_EVENT_LISTENER = 5
} EVENT_TYPE;

enum __key_status
{
	_KEY_PRESSED = 0,
	_KEY_RELEASED = 1
} KEY_STATUS;

enum __touch_status
{
	_TOUCH_PRESSED = 0,
	_TOUCH_LONG_PRESSED = 1,
	_TOUCH_RELEASED = 2,
	_TOUCH_MOVED = 3,
	_TOUCH_DOUBLE_PRESSED = 4,
	_TOUCH_FOCUS_IN = 5,
	_TOUCH_FOCUS_OUT = 6,
	_TOUCH_CANCELED = 7
} _TOUCH_STATUS;

enum __gesture_type
{
	_GESTURE_FLICK = 0,
	_GESTURE_LONGPRESS = 1,
	_GESTURE_PANNING = 2,
	_GESTURE_PINCH = 3,
	_GESTURE_ROTATION = 4,
	_GESTURE_TAP = 5,
	_GESTURE_CUSTOM = 6
} GESTURE_TYPE;

enum _orientation_status
{
	_OS_NONE = 0,
	_OS_PORTRAIT = 1,
	_OS_LANDSCAPE = 2,
	_OS_PORTRAIT_REVERSE = 3,
	_OS_LANDSCAPE_REVERSE = 4
} ORIENTATION_STATUS;


/*******************************************************************
 * macros for event probe
 *
 * log format:
 * 		SeqNumber`,ApiName`,Time`,Pid`,Tid`,InputParm`,Return`,PCAddr`,Error`,x`,y`,\n
 *		callstack_start`,callstack`,callstack_end
 *
 *******************************************************************/

#define DECLARE_VARIABLE_EVENT			\
	probeInfo_t	probeInfo;				\
	static unsigned int oldtime = 0;	\
	int blockresult = 0

#define PRE_PROBEBLOCK_BEGIN_EVENT(FUNCNAME, LIBNAME, EVENTTYPE)	\
	GET_REAL_FUNC(FUNCNAME, LIBNAME);								\
	do {															\
		if(oldtime != timestamp && (EVENTTYPE != EVENT_TYPE_MOVE ||	\
			getTouchState() == EVENT_TYPE_DOWN ||					\
			getTouchState() == EVENT_TYPE_MOVE))					\
		{															\
			blockresult = 1;										\
			detectTouchEvent(EVENTTYPE);							\
			PRE_UNCONDITIONAL_BLOCK_BEGIN()

#define PRE_PROBEBLOCK_END_EVENT()			\
			PRE_UNCONDITIONAL_BLOCK_END();	\
		}									\
	} while(0)

#define BEFORE_ORIGINAL_EVENT(FUNCNAME, LIBNAME, EVENTTYPE)		\
	DECLARE_VARIABLE_EVENT;										\
	PRE_PROBEBLOCK_BEGIN_EVENT(FUNCNAME, LIBNAME, EVENTTYPE);	\
	PRE_PROBEBLOCK_END_EVENT()

#define AFTER_ORIGINAL_EVENT(EVENTTYPE, INPUTFORMAT, ...)		\
	do {														\
		if(blockresult == 1) {									\
			POST_UNCONDITIONAL_BLOCK_BEGIN(LC_UIEVENT);			\
			APPEND_LOG_INPUT(INPUTFORMAT, __VA_ARGS__);			\
			log.length += sprintf(log.data + log.length,		\
					"`,`,`,`,1`,`,%d`,%d`,%d", x, y, EVENTTYPE);\
			APPEND_LOG_NULL_CALLSTACK();						\
			POST_UNCONDITIONAL_BLOCK_END();						\
			oldtime = timestamp;								\
		}														\
	} while(0)


#endif // __DA_EVENT_H__
