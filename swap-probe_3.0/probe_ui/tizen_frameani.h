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

#ifndef __TIZEN_FRAMEANI_H__
#define __TIZEN_FRAMEANI_H__

#include <FUi.h>

using namespace Tizen::Ui::Animations;
using namespace Tizen::Ui::Controls;

class DAFrameAnimationEventListener :
		public IFrameAnimatorEventListener
{
public:
	DAFrameAnimationEventListener();
	virtual ~DAFrameAnimationEventListener();

	virtual void 	OnFormTransitionAnimationFinished (FrameAnimator &source, Frame &frame, Form &form1, Form &form2);
	virtual void 	OnFormTransitionAnimationStarted (FrameAnimator &source, Frame &frame, Form &form1, Form &form2);
	virtual void 	OnFormTransitionAnimationStopped (FrameAnimator &source, Frame &frame, Form &form1, Form &form2);

	static DAFrameAnimationEventListener& GetInstance(void);

private:
	static DAFrameAnimationEventListener rInstance;
};


#endif	// __TIZEN_FRAMEANI_H__

