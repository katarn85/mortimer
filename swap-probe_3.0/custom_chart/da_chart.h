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

#ifndef __DA_CHART_H__
#define __DA_CHART_H__

#ifdef __cplusplus
extern "C" {
#endif

// Error code
#define ERR_MAX_CHART_NUMBER	-1001	// no more chart or series can be added
#define ERR_WRONG_HANDLE		-1002	// chart or series handle is wrong handle
#define ERR_WRONG_PARAMETER		-1003	// wrong parameter

typedef enum
{
	CHART_TYPE_AUTO = 0,
	CHART_TYPE_LINE = 1,
	CHART_TYPE_BAR = 2,
	CHART_TYPE_AREA = 3,
	CHART_TYPE_STEP = 4
} series_type;

typedef enum
{
	CHART_COLOR_AUTO = 0,
	CHART_COLOR_BLUE = 1,
	CHART_COLOR_GREEN = 2,
	CHART_COLOR_RED = 3,
	CHART_COLOR_BROWN = 4,
	CHART_COLOR_PURPLE = 5,
	CHART_COLOR_NAVY = 6,
	CHART_COLOR_CHOCOLATE = 7,
	CHART_COLOR_INDIGO = 8,
	CHART_COLOR_MAGENTA = 9,
	CHART_COLOR_TEAL = 10
} chart_color;

typedef enum
{
	CHART_NO_CYCLE = 0,
	CHART_INTERVAL_10MSEC = 1,
	CHART_INTERVAL_100MSEC = 2,
	CHART_INTERVAL_1SEC = 3
} chart_interval;

typedef int da_handle;
typedef float (* da_user_data_2_chart_data) (void* data_addr);


// api definition
#ifdef _USE_DA_

extern da_handle da_create_chart (char* chart_name);
extern da_handle da_create_series (da_handle charthandle, char* seriesname,
		series_type type, chart_color color);
extern da_handle da_create_default_series (da_handle charthandle, char* seriesname);
extern int da_set_callback (da_handle series_handle,	da_user_data_2_chart_data callback,
		void* data_addr, chart_interval interval);
extern void da_log (da_handle series_handle, float uservalue);


extern void da_mark (chart_color color, char* mark_text);

#else // _USE_DA_ (do not use da custom chart)

#define da_create_chart(a)						0
#define da_create_series(a, b, c, d)			0
#define da_create_default_series(a, b)			0
#define da_set_callback(a, b, c, d)				0
#define da_log(a, b)


#define da_mark(color, text)

#endif	// _USE_DA_

#ifdef __cplusplus
}
#endif

#endif // __DA_CHART_H__
