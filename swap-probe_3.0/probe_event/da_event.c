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

#include <appfw/app.h>
#include <runtime_info.h>

#include <Ecore.h>
#include <Eina.h>
#include <Ecore_X.h>

#include "daprobe.h"
#include "dahelper.h"
#include "da_event.h"

#include "binproto.h"
#include "real_functions.h"

static int external_angle = 0;
static int internal_angle = 0;

// ===================================================================
// orientation related functions
// ===================================================================

static int convert_angle(int angle)
{
	int os = _OS_NONE;

	switch(angle)
	{
	case 0:
		os = _OS_PORTRAIT;
		break;
	case 90:
		os = _OS_LANDSCAPE_REVERSE;
		break;
	case 180:
		os = _OS_PORTRAIT_REVERSE;
		break;
	case 270:
		os = _OS_LANDSCAPE;
		break;
	default:
		break;
	}

	return os;
}

void on_orientation_changed(int angle, bool capi)
{
	probeInfo_t	probeInfo;

	probeBlockStart();

	internal_angle = angle;
	external_angle = internal_angle;

	if(isOptionEnabled(OPT_EVENT))
	{
		setProbePoint(&probeInfo);

		PREPARE_LOCAL_BUF();
		PACK_COMMON_BEGIN(MSG_PROBE_UIEVENT,
				  API_ID_on_orientation_changed,
				  "dd", angle, (uint32_t)capi);
		PACK_COMMON_END('v', 0, 0, 0);
		PACK_UIEVENT(_EVENT_ORIENTATION, 0, 0, 0, "", convert_angle(external_angle));
		FLUSH_LOCAL_BUF();
	}

	SCREENSHOT_SET();
//	if(!capi)
//	{
//		SCREENSHOT_DONE();
//	}

	probeBlockEnd();
}

int getOrientation()
{
	return external_angle;
}

// ====================================================================
// initialize and finalize event
// ====================================================================

int initialize_event()
{
	external_angle = internal_angle = app_get_device_orientation();
	return 0;
}

int finalize_event()
{
	return 0;
}


