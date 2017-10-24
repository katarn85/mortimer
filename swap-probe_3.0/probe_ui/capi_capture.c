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

#include <Evas.h>
#include <Elementary.h>

#include "daprobe.h"
#include "probeinfo.h"
#include "da_ui.h"

Evas_Object *elm_win_add(Evas_Object *parent, const char* name, Elm_Win_Type type)
{
	static Evas_Object * (*elm_win_addp)(Evas_Object *parent, const char* name, Elm_Win_Type type);

	BEFORE_ORIGINAL_SNAPSHOT(elm_win_add, LIBELEMENTARY);
	ret = elm_win_addp(parent, name, type);
	AFTER_ORIGINAL_SNAPSHOT(ret);

	return ret;
}

Evas_Object *elm_controlbar_add (Evas_Object *parent)
{
	static Evas_Object * (*elm_controlbar_addp)(Evas_Object *parent);

	BEFORE_ORIGINAL_SNAPSHOT(elm_controlbar_add, LIBELEMENTARY);
	ret = elm_controlbar_addp(parent);
	AFTER_ORIGINAL_SNAPSHOT(ret);

	return ret;
}

Evas_Object *elm_naviframe_add(Evas_Object *parent)
{
	static Evas_Object * (*elm_naviframe_addp)(Evas_Object *parent);

	BEFORE_ORIGINAL_SNAPSHOT(elm_naviframe_add, LIBELEMENTARY);
	ret = elm_naviframe_addp(parent);
	AFTER_ORIGINAL_SNAPSHOT(ret);

	return ret;
}

Evas_Object *elm_pager_add(Evas_Object *parent)
{
	static Evas_Object * (*elm_pager_addp)(Evas_Object *parent);

	BEFORE_ORIGINAL_SNAPSHOT(elm_pager_add, LIBELEMENTARY);
	ret = elm_pager_addp(parent);
	AFTER_ORIGINAL_SNAPSHOT(ret);

	return ret;
}
