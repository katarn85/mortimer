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

#define _USE_DA_
#include "da_chart.h"

// =====================================================================
// api definition
// =====================================================================
#define __unused __attribute__((unused))
void da_mark(chart_color __unused color, char __unused * mark_text)
{

}

da_handle da_create_chart(char __unused * chart_name)
{
	return 0;
}

da_handle da_create_series(da_handle __unused charthandle,
			   char __unused * seriesname,
			   series_type __unused type,
			   chart_color __unused color)
{
	return 0;
}

da_handle da_create_default_series(da_handle __unused charthandle,
				   char __unused * seriesname)
{
	return 0;
}

int da_set_callback(da_handle __unused series_handle,
		    da_user_data_2_chart_data __unused callback,
		    void __unused * data_addr,
		    chart_interval __unused interval)
{
	return 0;
}

void da_log(da_handle __unused series_handle, float __unused uservalue)
{
}
