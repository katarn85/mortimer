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

#include "daprobe.h"
#include "dahelper.h"
#include "probeinfo.h"
#include "tizen_probe.h"
#include "dacollection.h"
#include "da_event.h"
#include "gesture.h"
#include "binproto.h"

#define PACK_GESTURE_EVENT(API_ID, _GESTURETYPE, _X, _Y, _INFO1, _INFO2, _ARGDETECTOR) \
	do {																		\
		char info1_str[16];														\
		setProbePoint(&probeInfo);												\
		PREPARE_LOCAL_BUF();													\
		PACK_COMMON_BEGIN(MSG_PROBE_UIEVENT, API_ID, "p", voidp_to_uint64(_ARGDETECTOR));	\
		PACK_COMMON_END('v', 0, 0, 0);												\
		sprintf(info1_str, "%d", _INFO1);										\
		PACK_UIEVENT(_EVENT_GESTURE, _GESTURETYPE, _X, _Y, info1_str, _INFO2);	\
		FLUSH_LOCAL_BUF();														\
	} while (0)

GestureEventListener GestureEventListener::rInstance;

GestureEventListener::GestureEventListener()
{

}

GestureEventListener::~GestureEventListener()
{

}

void GestureEventListener::OnCustomGestureCanceled (TouchGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnCustomGestureCanceled__TouchGestureDetector__gestureDetector_,
				   _GESTURE_CUSTOM, 0, 0, 0, 0, &gestureDetector);
		probeBlockEnd();
	}
}

void GestureEventListener::OnCustomGestureChanged (TouchGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnCustomGestureChanged__TouchGestureDetector__gestureDetector_,
				   _GESTURE_CUSTOM, 0, 0, 0, 0, &gestureDetector);
		probeBlockEnd();
	}
}

void GestureEventListener::OnCustomGestureFinished (TouchGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnCustomGestureFinished__TouchGestureDetector__gestureDetector_,
				   _GESTURE_CUSTOM, 0, 0, 0, 0, &gestureDetector);
		probeBlockEnd();
	}
}

void GestureEventListener::OnCustomGestureStarted (TouchGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnCustomGestureStarted__TouchGestureDetector__gestureDetector_,
				   _GESTURE_CUSTOM, 0, 0, 0, 0, &gestureDetector);
		probeBlockEnd();
	}
}

void GestureEventListener::OnFlickGestureCanceled (TouchFlickGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int x = 0, y = 0, dur = 0;
			FlickDirection direction = FLICK_DIRECTION_NONE;

			gestureDetector.GetDistance(x, y);
			dur = gestureDetector.GetDuration();
			direction = gestureDetector.GetDirection();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnFlickGestureCanceled__TouchFlickGestureDetector__gestureDetector_,
					   _GESTURE_FLICK, x, y, dur, (int)direction, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnFlickGestureDetected (TouchFlickGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int x = 0, y = 0, dur = 0;
			FlickDirection direction = FLICK_DIRECTION_NONE;

			gestureDetector.GetDistance(x, y);
			dur = gestureDetector.GetDuration();
			direction = gestureDetector.GetDirection();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnFlickGestureDetected__TouchFlickGestureDetector__gestureDetector_,
					   _GESTURE_FLICK, x, y, dur, (int)direction, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnLongPressGestureCanceled (TouchLongPressGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int moveallow = 0, tcount = 0, dur = 0;

			moveallow = gestureDetector.GetMoveAllowance();
			dur = gestureDetector.GetDuration();
			tcount = gestureDetector.GetTouchCount();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnLongPressGestureCanceled__TouchLongPressGestureDetector__gestureDetector_,
					   _GESTURE_LONGPRESS, moveallow, 0, dur, tcount, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnLongPressGestureDetected (TouchLongPressGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int moveallow = 0, tcount = 0, dur = 0;

			moveallow = gestureDetector.GetMoveAllowance();
			dur = gestureDetector.GetDuration();
			tcount = gestureDetector.GetTouchCount();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnLongPressGestureDetected__TouchLongPressGestureDetector__gestureDetector_,
					   _GESTURE_LONGPRESS, moveallow, 0, dur, tcount, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnPanningGestureCanceled (TouchPanningGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int tcount = 0;

			tcount = gestureDetector.GetTouchCount();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnPanningGestureCanceled__TouchPanningGestureDetector__gestureDetector_,
					   _GESTURE_PANNING, 0, 0, 0, tcount, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnPanningGestureChanged (TouchPanningGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int tcount = 0;

			tcount = gestureDetector.GetTouchCount();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnPanningGestureChanged__TouchPanningGestureDetector__gestureDetector_,
					   _GESTURE_PANNING, 0, 0, 0, tcount, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnPanningGestureFinished (TouchPanningGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int tcount = 0;

			tcount = gestureDetector.GetTouchCount();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnPanningGestureFinished__TouchPanningGestureDetector__gestureDetector_,
					   _GESTURE_PANNING, 0, 0, 0, tcount, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnPanningGestureStarted (TouchPanningGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int tcount = 0;

			tcount = gestureDetector.GetTouchCount();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnPanningGestureStarted__TouchPanningGestureDetector__gestureDetector_,
					   _GESTURE_PANNING, 0, 0, 0, tcount, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnPinchGestureCanceled (TouchPinchGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			Tizen::Graphics::Point point;
			int scale = 0;

			point = gestureDetector.GetCenterPoint();
			scale = gestureDetector.GetScale();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnPinchGestureCanceled__TouchPinchGestureDetector__gestureDetector_,
					   _GESTURE_PINCH, point.x, point.y, scale, 0, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnPinchGestureChanged (TouchPinchGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			Tizen::Graphics::Point point;
			int scale = 0;

			point = gestureDetector.GetCenterPoint();
			scale = gestureDetector.GetScale();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnPinchGestureChanged__TouchPinchGestureDetector__gestureDetector_,
					   _GESTURE_PINCH, point.x, point.y, scale, 0, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnPinchGestureFinished (TouchPinchGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			Tizen::Graphics::Point point;
			int scale = 0;

			point = gestureDetector.GetCenterPoint();
			scale = gestureDetector.GetScale();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnPinchGestureFinished__TouchPinchGestureDetector__gestureDetector_,
					   _GESTURE_PINCH, point.x, point.y, scale, 0, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnPinchGestureStarted (TouchPinchGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			Tizen::Graphics::Point point;
			int scale = 0;

			point = gestureDetector.GetCenterPoint();
			scale = gestureDetector.GetScale();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnPinchGestureStarted__TouchPinchGestureDetector__gestureDetector_,
					   _GESTURE_PINCH, point.x, point.y, scale, 0, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnRotationGestureCanceled (TouchRotationGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int distance = 0;
			float angle = 0.0f;

			distance = gestureDetector.GetDistance();
			angle = gestureDetector.GetAngle();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnRotationGestureCanceled__TouchRotationGestureDetector__gestureDetector_,
					   _GESTURE_ROTATION, 0, 0, distance, static_cast<int>(angle), &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnRotationGestureChanged (TouchRotationGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int distance = 0;
			float angle = 0.0f;

			distance = gestureDetector.GetDistance();
			angle = gestureDetector.GetAngle();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnRotationGestureChanged__TouchRotationGestureDetector__gestureDetector_,
					   _GESTURE_ROTATION, 0, 0, distance, static_cast<int>(angle), &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnRotationGestureFinished (TouchRotationGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int distance = 0;
			float angle = 0.0f;

			distance = gestureDetector.GetDistance();
			angle = gestureDetector.GetAngle();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnRotationGestureFinished__TouchRotationGestureDetector__gestureDetector_,
					   _GESTURE_ROTATION, 0, 0, distance, static_cast<int>(angle), &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnRotationGestureStarted (TouchRotationGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int distance = 0;
			float angle = 0.0f;

			distance = gestureDetector.GetDistance();
			angle = gestureDetector.GetAngle();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnRotationGestureStarted__TouchRotationGestureDetector__gestureDetector_,
					   _GESTURE_ROTATION, 0, 0, distance, static_cast<int>(angle), &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnTapGestureCanceled (TouchTapGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int move = 0, tapcount = 0, touchcount = 0, interval = 0;

			move = gestureDetector.GetMoveAllowance();
			tapcount = gestureDetector.GetTapCount();
			interval = gestureDetector.GetTapInterval();
			touchcount = gestureDetector.GetTouchCount();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnTapGestureCanceled__TouchTapGestureDetector__gestureDetector_,
					   _GESTURE_TAP, move, tapcount, interval, touchcount, &gestureDetector);
		}
		probeBlockEnd();
	}
}

void GestureEventListener::OnTapGestureDetected (TouchTapGestureDetector &gestureDetector)
{
	probeInfo_t	probeInfo;

	if(isOptionEnabled(OPT_EVENT))
	{
		probeBlockStart();
		{
			int move = 0, tapcount = 0, touchcount = 0, interval = 0;

			move = gestureDetector.GetMoveAllowance();
			tapcount = gestureDetector.GetTapCount();
			interval = gestureDetector.GetTapInterval();
			touchcount = gestureDetector.GetTouchCount();
			PACK_GESTURE_EVENT(API_ID_void_GestureEventListener__OnTapGestureDetected__TouchTapGestureDetector__gestureDetector_,
					   _GESTURE_TAP, move, tapcount, interval, touchcount, &gestureDetector);
		}
		probeBlockEnd();
	}
}

GestureEventListener& GestureEventListener::GetInstance()
{
	return rInstance;
}
