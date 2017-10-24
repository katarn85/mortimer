/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
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

#include <Ecore.h>
#include <Ecore_X.h>

#include "daprobe.h"
#include "dahelper.h"
#include "common_probe_init.h"


Ecore_Event_Handler *register_orientation_event_listener();
void unregister_orientation_event_listener(Ecore_Event_Handler *handler);

Ecore_Event_Handler *handler = NULL;


Eina_Bool _da_onclientmessagereceived(void __unused *pData, int __unused type,
				      void *pEvent)
{
	Ecore_X_Event_Client_Message *pClientEvent;

	probeBlockStart();
	pClientEvent = (Ecore_X_Event_Client_Message*)pEvent;

	if (pClientEvent != NULL) {

		//This code from ecore_x
		//So I don't know what 32 does mean
		if (pClientEvent->format != 32) {
			probeBlockEnd();
			return ECORE_CALLBACK_PASS_ON;
		}

		if (pClientEvent->message_type ==
		    ECORE_X_ATOM_E_WINDOW_ROTATION_CHANGE_PREPARE) {
			int orientation = (int)pClientEvent->data.l[1];
			on_orientation_changed(orientation, false);
		}
	}
	probeBlockEnd();

	return ECORE_CALLBACK_RENEW;
}

Ecore_Event_Handler *register_orientation_event_listener()
{
	Ecore_Event_Handler *handler;
	probeBlockStart();

	handler = ecore_event_handler_add(ECORE_X_EVENT_CLIENT_MESSAGE,
					  _da_onclientmessagereceived, NULL);

	probeBlockEnd();

	return handler;
}

void unregister_orientation_event_listener(Ecore_Event_Handler *handler)
{
	probeBlockStart();

	if (handler)
		ecore_event_handler_del(handler);

	probeBlockEnd();
}

EAPI int ecore_x_init(const char *name)
{
	static Ecore_Event_Handler *event_handler = NULL;

	int res = 0;
	static int (*ecore_x_initp)(const char *name);
	PRINTMSG("(%s)", name);

	GET_REAL_FUNC_RTLD_NEXT(ecore_x_init);
	res = ecore_x_initp(name);

	if (event_handler == NULL) {
		event_handler = register_orientation_event_listener();
		if (event_handler == NULL)
			PRINTERR("Fail to init event listener");
	}
	return res;
}
