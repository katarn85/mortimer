/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
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

#include <app.h>
#include <Ecore.h>
#include "damaps.h"
#include "daprobe.h"
#include "dahelper.h"
#include "probeinfo.h"
#include "binproto.h"
#include "real_functions.h"
#include "common_probe_init.h"
#include "api_config.h"

Ecore_Event_Handler *register_orientation_event_listener();
void unregister_orientation_event_listener(Ecore_Event_Handler *handler);

app_event_callback_s gAppCallback;
ui_app_lifecycle_callback_s uiAppCallback;

#define PACK_ORIGINAL_APPFWCYCLE(API_ID, RTYPE, RVAL, INPUTFORMAT, ...)		\
	newerrno = errno;							\
	do {									\
		PREPARE_LOCAL_BUF();					\
		PACK_COMMON_BEGIN(MSG_PROBE_LIFECYCLE, API_ID, INPUTFORMAT, __VA_ARGS__);	\
		PACK_COMMON_END(RTYPE, RVAL, newerrno, blockresult);	\
		FLUSH_LOCAL_BUF();					\
	} while(0);								\
	errno = (newerrno != 0) ? newerrno : olderrno

static bool _dalc_app_create(void *user_data)
{
	bool bret = false;
	DECLARE_ERRNO_VARS;
	int blockresult = 1;

	bret = gAppCallback.create(user_data);

	PACK_ORIGINAL_APPFWCYCLE(API_ID__dalc_app_create, 'b', bret, "p",
				 voidp_to_uint64(user_data));

	return bret;
}

static void _dalc_app_terminate(void *user_data)
{
	DECLARE_ERRNO_VARS;
	int blockresult = 1;

	gAppCallback.terminate(user_data);

	PACK_ORIGINAL_APPFWCYCLE(API_ID__dalc_app_terminate, 'v', 0, "p",
				 voidp_to_uint64(user_data));
}

static void _dalc_app_pause(void *user_data)
{
	DECLARE_ERRNO_VARS;
	int blockresult = 1;

	gAppCallback.pause(user_data);

	PACK_ORIGINAL_APPFWCYCLE(API_ID__dalc_app_pause, 'v', 0, "p",
				 voidp_to_uint64(user_data));
}

static void _dalc_app_resume(void *user_data)
{
	DECLARE_ERRNO_VARS;
	int blockresult = 1;

	gAppCallback.resume(user_data);

	PACK_ORIGINAL_APPFWCYCLE(API_ID__dalc_app_resume, 'v', 0, "p",
				 voidp_to_uint64(user_data));
}

#ifdef PRIVATE_CAPI_APPFW
static void _dalc_app_control(app_control_h handle, void *user_data)
#else /* !PRIVATE_CAPI_APPFW */
static void _dalc_app_service(service_h handle, void *user_data)
#endif /* PRIVATE_CAPI_APPFW */
{
	DECLARE_ERRNO_VARS;
	int blockresult = 1;

#ifdef PRIVATE_CAPI_APPFW
	gAppCallback.app_control(handle, user_data);
#else /* !PRIVATE_CAPI_APPFW */
	gAppCallback.service(handle, user_data);
#endif /* PRIVATE_CAPI_APPFW */

	PACK_ORIGINAL_APPFWCYCLE(API_ID__dalc_app_service, 'v', 0, "dp",
				 (unsigned int)handle,
				 voidp_to_uint64(user_data));
}

static void _dalc_app_deviceorientationchanged(app_device_orientation_e orientation, void *user_data)
{
	on_orientation_changed((int)orientation, true);

	if (gAppCallback.device_orientation)
		gAppCallback.device_orientation(orientation, user_data);
}

int app_efl_main(int *argc, char ***argv, app_event_callback_s *callback, void *user_data)
{
	static int (*app_efl_mainp)(int *argc, char ***argv, app_event_callback_s *callback, void *user_data);
	Ecore_Event_Handler* handler;
	int ret;

	GET_REAL_FUNCP_RTLD_NEXT(app_efl_main, app_efl_mainp);

	probeBlockStart();
	handler = register_orientation_event_listener();
	gAppCallback.create = callback->create;
	gAppCallback.terminate = callback->terminate;
	gAppCallback.pause = callback->pause;
	gAppCallback.resume = callback->resume;
#ifdef PRIVATE_CAPI_APPFW
	gAppCallback.app_control = callback->app_control;
#else /* !PRIVATE_CAPI_APPFW */
	gAppCallback.service = callback->service;
#endif /* PRIVATE_CAPI_APPFW */
	gAppCallback.device_orientation = callback->device_orientation;

	if (callback->create)
		callback->create = _dalc_app_create;
	if (callback->terminate)
		callback->terminate = _dalc_app_terminate;
	if (callback->pause)
		callback->pause = _dalc_app_pause;
	if (callback->resume)
		callback->resume = _dalc_app_resume;
#ifdef PRIVATE_CAPI_APPFW
	if (callback->app_control)
		callback->app_control = _dalc_app_control;
#else /* !PRIVATE_CAPI_APPFW */
	if (callback->service)
		callback->service = _dalc_app_service;
#endif /* PRIVATE_CAPI_APPFW */
	callback->device_orientation = _dalc_app_deviceorientationchanged;
	probeBlockEnd();

	ret = app_efl_mainp(argc, argv, callback, user_data);

	probeBlockStart();
	unregister_orientation_event_listener(handler);
	callback->create = gAppCallback.create;
	callback->terminate = gAppCallback.terminate;
	callback->pause = gAppCallback.pause;
	callback->resume = gAppCallback.resume;
#ifdef PRIVATE_CAPI_APPFW
	callback->app_control = gAppCallback.app_control;
#else /* !PRIVATE_CAPI_APPFW */
	callback->service = gAppCallback.service;
#endif /* PRIVATE_CAPI_APPFW */
	callback->device_orientation = gAppCallback.device_orientation;
	probeBlockEnd();

	return ret;
}


/************************************ UI APP ******************************************/
static bool _ui_dalc_app_create(void *user_data)
{
	bool bret = false;
	DECLARE_ERRNO_VARS;
	int blockresult = 1;

	bret = uiAppCallback.create(user_data);

	PACK_ORIGINAL_APPFWCYCLE(API_ID__dalc_app_create, 'b', bret, "p",
				 voidp_to_uint64(user_data));

	return bret;
}

static void _ui_dalc_app_terminate(void *user_data)
{
	DECLARE_ERRNO_VARS;
	int blockresult = 1;

	uiAppCallback.terminate(user_data);

	PACK_ORIGINAL_APPFWCYCLE(API_ID__dalc_app_terminate, 'v', 0, "p",
				 voidp_to_uint64(user_data));
}

static void _ui_dalc_app_pause(void *user_data)
{
	DECLARE_ERRNO_VARS;
	int blockresult = 1;

	uiAppCallback.pause(user_data);

	PACK_ORIGINAL_APPFWCYCLE(API_ID__dalc_app_pause, 'v', 0, "p",
				 voidp_to_uint64(user_data));
}

static void _ui_dalc_app_resume(void *user_data)
{
	DECLARE_ERRNO_VARS;
	int blockresult = 1;

	uiAppCallback.resume(user_data);

	PACK_ORIGINAL_APPFWCYCLE(API_ID__dalc_app_resume, 'v', 0, "p",
				 voidp_to_uint64(user_data));
}

static void _ui_dalc_app_control(app_control_h handle, void *user_data)
{
	DECLARE_ERRNO_VARS;
	int blockresult = 1;

	uiAppCallback.app_control(handle, user_data);

	PACK_ORIGINAL_APPFWCYCLE(API_ID__dalc_app_service, 'v', 0, "dp",
				 (unsigned int)handle,
				 voidp_to_uint64(user_data));
}

int ui_app_main(int argc, char **argv, ui_app_lifecycle_callback_s *callback, void *user_data)
{
	static int (*ui_app_mainp)(int argc, char **argv, ui_app_lifecycle_callback_s *callback, void *user_data);
	Ecore_Event_Handler* handler;
	int ret;

	GET_REAL_FUNCP_RTLD_NEXT(ui_app_main, ui_app_mainp);

	probeBlockStart();
	handler = register_orientation_event_listener();
	uiAppCallback.create = callback->create;
	uiAppCallback.terminate = callback->terminate;
	uiAppCallback.pause = callback->pause;
	uiAppCallback.resume = callback->resume;

	uiAppCallback.app_control = callback->app_control;

	if (callback->create)
		callback->create = _ui_dalc_app_create;
	if (callback->terminate)
		callback->terminate = _ui_dalc_app_terminate;
	if (callback->pause)
		callback->pause = _ui_dalc_app_pause;
	if (callback->resume)
		callback->resume = _ui_dalc_app_resume;

	if (callback->app_control)
		callback->app_control = _ui_dalc_app_control;

	probeBlockEnd();

	ret = ui_app_mainp(argc, argv, callback, user_data);

	probeBlockStart();
	unregister_orientation_event_listener(handler);

	callback->create = uiAppCallback.create;
	callback->terminate = uiAppCallback.terminate;
	callback->pause = uiAppCallback.pause;
	callback->resume = uiAppCallback.resume;
	callback->app_control = uiAppCallback.app_control;

	probeBlockEnd();

	return ret;
}
