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

#ifndef __GESTURE_H__
#define __GESTURE_H__

#include <FUi.h>

using namespace Tizen::Ui;

class GestureEventListener :
		public ITouchCustomGestureEventListener,
		public ITouchFlickGestureEventListener,
		public ITouchLongPressGestureEventListener,
		public ITouchPanningGestureEventListener,
		public ITouchPinchGestureEventListener,
		public ITouchRotationGestureEventListener,
		public ITouchTapGestureEventListener
{
public:
	GestureEventListener();
	virtual ~GestureEventListener();

	virtual void 	OnCustomGestureStarted (TouchGestureDetector &gestureDetector);
	virtual void 	OnCustomGestureChanged (TouchGestureDetector &gestureDetector);
	virtual void 	OnCustomGestureFinished (TouchGestureDetector &gestureDetector);
	virtual void 	OnCustomGestureCanceled (TouchGestureDetector &gestureDetector);

	virtual void 	OnFlickGestureDetected (TouchFlickGestureDetector &gestureDetector);
	virtual void 	OnFlickGestureCanceled (TouchFlickGestureDetector &gestureDetector);

	virtual void 	OnLongPressGestureDetected (TouchLongPressGestureDetector &gestureDetector);
	virtual void 	OnLongPressGestureCanceled (TouchLongPressGestureDetector &gestureDetector);

	virtual void 	OnPanningGestureStarted (TouchPanningGestureDetector &gestureDetector);
	virtual void 	OnPanningGestureChanged (TouchPanningGestureDetector &gestureDetector);
	virtual void 	OnPanningGestureFinished (TouchPanningGestureDetector &gestureDetector);
	virtual void 	OnPanningGestureCanceled (TouchPanningGestureDetector &gestureDetector);

	virtual void 	OnPinchGestureStarted (TouchPinchGestureDetector &gestureDetector);
	virtual void 	OnPinchGestureChanged (TouchPinchGestureDetector &gestureDetector);
	virtual void 	OnPinchGestureFinished (TouchPinchGestureDetector &gestureDetector);
	virtual void 	OnPinchGestureCanceled (TouchPinchGestureDetector &gestureDetector);

	virtual void 	OnRotationGestureStarted (TouchRotationGestureDetector &gestureDetector);
	virtual void 	OnRotationGestureChanged (TouchRotationGestureDetector &gestureDetector);
	virtual void 	OnRotationGestureFinished (TouchRotationGestureDetector &gestureDetector);
	virtual void 	OnRotationGestureCanceled (TouchRotationGestureDetector &gestureDetector);

	virtual void 	OnTapGestureDetected (TouchTapGestureDetector &gestureDetector);
	virtual void 	OnTapGestureCanceled (TouchTapGestureDetector &gestureDetector);

	static GestureEventListener& GetInstance(void);

private:
	static GestureEventListener rInstance;
};

#endif
