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

#include <pthread.h>
#include <signal.h>			// for signal
#include <unistd.h>			// for ualarm, sleep
#include <sys/timerfd.h>	// for timerfd
#include <stdio.h>
#include <errno.h>			// for errno
#include <string.h>			// for memset
#include <stdint.h>			// for uint64_t

#include "daprobe.h"
#include "dahelper.h"
#include "probeinfo.h"
#define _USE_DA_
#include "da_chart.h"

#include "binproto.h"
#include "real_functions.h"

#define ERR_THREAD_CREATE_FAIL	-2001	// thread creation fail

#define MAX_TITLE_LENGTH		16
#define MAX_CHART_HANDLE		10		// cannot be exceeded 10
#define MAX_SERIES_PER_CHART	4		// cannot be exceeded 10
// chart handle is  from 1 to 9
// series handle is from 11 to 99
// series handle 32 means that the series is 2nd series in 3rd chart.
// zero value handle means error

// =====================================================================
// global variable
// =====================================================================

typedef struct _chart_interval_callback
{
	da_handle chart_handle;
	da_handle series_handle;
	da_user_data_2_chart_data callback;
	void* user_data;
	struct _chart_interval_callback* next;
} chart_interval_callback;

typedef struct
{
	int			timerfd;
	pthread_t	thread_handle;
	chart_interval_callback*	callback_list;
	pthread_mutex_t		list_mutex;
} interval_manager;

typedef struct
{
	int			chart_handle_index;
	int			series_handle_index[MAX_CHART_HANDLE];
	int			interval_for_series[MAX_CHART_HANDLE][MAX_SERIES_PER_CHART];
	interval_manager	interval_10ms;
	interval_manager	interval_100ms;
	interval_manager	interval_1s;
} chart_handle_maintainer;

chart_handle_maintainer chm;

__thread pid_t cur_thread = -1;


// =====================================================================
// internal macro definition
// =====================================================================

#define DECLARE_CHART_VARIABLE					\
	da_handle __attribute__((unused)) ret = 0;	\
	probeInfo_t probeInfo;

// =====================================================================
// timer thread routine for callback
// =====================================================================

void* _chart_timerThread(void* data)
{
	DECLARE_CHART_VARIABLE;

	uint64_t exp;
	ssize_t readsize;
	chart_interval_callback* cur;
	float value;
	sigset_t profsigmask;
	interval_manager* pmanager = (interval_manager*)data;

	probeBlockStart();

	sigemptyset(&profsigmask);
	sigaddset(&profsigmask, SIGPROF);
	pthread_sigmask(SIG_BLOCK, &profsigmask, NULL);

	while((readsize = read(pmanager->timerfd, &exp, sizeof(uint64_t))) > 0)
	{
		pthread_mutex_lock(&pmanager->list_mutex);

		cur = pmanager->callback_list;
		while(cur != NULL)
		{
			value = cur->callback(cur->user_data);

			setProbePoint(&probeInfo);

			PREPARE_LOCAL_BUF();
			PACK_COMMON_BEGIN(MSG_PROBE_CUSTOM,
					  API_ID__chart_timerThread,
					  "", 0);
			PACK_COMMON_END('p', 0, 0, 2);
			PACK_CUSTOM(cur->series_handle, 0, "", 0, value);
			FLUSH_LOCAL_BUF();

			cur = cur->next;
		}

		pthread_mutex_unlock(&pmanager->list_mutex);

		sleep(0);
	}

	probeBlockEnd();

	return NULL;
}

static int start_callback_thread(chart_interval interval)
{
	int* timerfd = NULL;
	pthread_t* thread_handle = NULL;
	interval_manager* pman = NULL;
	struct itimerspec timevalue;

	if(interval == CHART_INTERVAL_10MSEC)
	{
		timerfd = &(chm.interval_10ms.timerfd);
		thread_handle = &(chm.interval_10ms.thread_handle);
		pman = &(chm.interval_10ms);
		timevalue.it_value.tv_sec = 0;
		timevalue.it_value.tv_nsec = 10000000L;
		timevalue.it_interval.tv_sec = 0;
		timevalue.it_interval.tv_nsec = 10000000L;
	}
	else if(interval == CHART_INTERVAL_100MSEC)
	{
		timerfd = &(chm.interval_100ms.timerfd);
		thread_handle = &(chm.interval_100ms.thread_handle);
		pman = &(chm.interval_100ms);
		timevalue.it_value.tv_sec = 0;
		timevalue.it_value.tv_nsec = 100000000L;
		timevalue.it_interval.tv_sec = 0;
		timevalue.it_interval.tv_nsec = 100000000L;
	}
	else if(interval == CHART_INTERVAL_1SEC)
	{
		timerfd = &(chm.interval_1s.timerfd);
		thread_handle = &(chm.interval_1s.thread_handle);
		pman = &(chm.interval_1s);
		timevalue.it_value.tv_sec = 1;
		timevalue.it_value.tv_nsec = 0;
		timevalue.it_interval.tv_sec = 1;
		timevalue.it_interval.tv_nsec = 0;
	}
	else
	{
		return ERR_WRONG_PARAMETER;
	}

	/**
	 * FIXME: Type of pthread_t is undefined.
	 * Comparing it with -1 is *very* bad
	 */
	if (*timerfd != -1 || *thread_handle != (pthread_t) -1)
		return 0;		// already thread exist

	*timerfd = timerfd_create(CLOCK_REALTIME, 0);
	if (*timerfd == -1)
		return errno;

	if (timerfd_settime(*timerfd, 0, &timevalue, NULL) == -1)
		return errno;

	if (pthread_create(thread_handle, NULL, _chart_timerThread, pman) < 0)
		return ERR_THREAD_CREATE_FAIL;

	return 0;
}


// =====================================================================
// internal utility function
// =====================================================================

static void add_to_callback_list(chart_interval interval, da_handle charthandle, da_handle series_handle,
		da_user_data_2_chart_data callback, void* user_data)
{
	chart_interval_callback* newelem;

	newelem = (chart_interval_callback*)real_malloc(sizeof(chart_interval_callback));
	newelem->chart_handle = charthandle;
	newelem->series_handle = series_handle;
	newelem->callback = callback;
	newelem->user_data = user_data;

	chm.interval_for_series[charthandle][series_handle % 10] = interval;

	switch(interval)
	{
	case CHART_INTERVAL_10MSEC:
		pthread_mutex_lock(&chm.interval_10ms.list_mutex);
		newelem->next = chm.interval_10ms.callback_list;
		chm.interval_10ms.callback_list = newelem;
		pthread_mutex_unlock(&chm.interval_10ms.list_mutex);
		__sync_add_and_fetch(&(gTraceInfo.custom_chart_callback_count), 1);
		break;
	case CHART_INTERVAL_100MSEC:
		pthread_mutex_lock(&chm.interval_100ms.list_mutex);
		newelem->next = chm.interval_100ms.callback_list;
		chm.interval_100ms.callback_list = newelem;
		pthread_mutex_unlock(&chm.interval_100ms.list_mutex);
		__sync_add_and_fetch(&(gTraceInfo.custom_chart_callback_count), 1);
		break;
	case CHART_INTERVAL_1SEC:
		pthread_mutex_lock(&chm.interval_1s.list_mutex);
		newelem->next = chm.interval_1s.callback_list;
		chm.interval_1s.callback_list = newelem;
		pthread_mutex_unlock(&chm.interval_1s.list_mutex);
		__sync_add_and_fetch(&(gTraceInfo.custom_chart_callback_count), 1);
		break;
	default:
		free(newelem);
		break;
	}
}

static void remove_all_callback_list()
{
	chart_interval_callback* cur;

	pthread_mutex_lock(&chm.interval_10ms.list_mutex);
	while(chm.interval_10ms.callback_list != NULL)
	{
		cur = chm.interval_10ms.callback_list;
		chm.interval_10ms.callback_list = cur->next;
		free(cur);
	}
	pthread_mutex_unlock(&chm.interval_10ms.list_mutex);

	pthread_mutex_lock(&chm.interval_100ms.list_mutex);
	while(chm.interval_100ms.callback_list != NULL)
	{
		cur = chm.interval_100ms.callback_list;
		chm.interval_100ms.callback_list = cur->next;
		free(cur);
	}
	pthread_mutex_unlock(&chm.interval_100ms.list_mutex);

	pthread_mutex_lock(&chm.interval_1s.list_mutex);
	while(chm.interval_1s.callback_list != NULL)
	{
		cur = chm.interval_1s.callback_list;
		chm.interval_1s.callback_list = cur->next;
		free(cur);
	}
	pthread_mutex_unlock(&chm.interval_1s.list_mutex);

	memset(&chm.interval_for_series, 0, sizeof(chm.interval_for_series));
	__sync_and_and_fetch(&(gTraceInfo.custom_chart_callback_count), 0);
}

static void remove_from_callback_list(da_handle charthandle, da_handle series_handle)
{
	chart_interval_callback *prev, *cur;
	chart_interval interval;

	interval = chm.interval_for_series[charthandle][series_handle % 10];
	chm.interval_for_series[charthandle][series_handle % 10] = 0;

	switch(interval)
	{
	case CHART_INTERVAL_10MSEC:
		pthread_mutex_lock(&chm.interval_10ms.list_mutex);

		prev = NULL;
		cur = chm.interval_10ms.callback_list;
		while(cur != NULL)
		{
			if(cur->chart_handle == charthandle && cur->series_handle == series_handle)
			{
				if(prev)
					prev->next = cur->next;
				else
					chm.interval_10ms.callback_list = cur->next;

				__sync_sub_and_fetch(&(gTraceInfo.custom_chart_callback_count), 1);
				free(cur);
				break;
			}

			prev = cur;
			cur = cur->next;
		}

		pthread_mutex_unlock(&chm.interval_10ms.list_mutex);
		break;
	case CHART_INTERVAL_100MSEC:
		pthread_mutex_lock(&chm.interval_100ms.list_mutex);

		prev = NULL;
		cur = chm.interval_100ms.callback_list;
		while(cur != NULL)
		{
			if(cur->chart_handle == charthandle && cur->series_handle == series_handle)
			{
				if(prev)
					prev->next = cur->next;
				else
					chm.interval_100ms.callback_list = cur->next;

				__sync_sub_and_fetch(&(gTraceInfo.custom_chart_callback_count), 1);
				free(cur);
				break;
			}

			prev = cur;
			cur = cur->next;
		}

		pthread_mutex_unlock(&chm.interval_100ms.list_mutex);
		break;
	case CHART_INTERVAL_1SEC:
		pthread_mutex_lock(&chm.interval_1s.list_mutex);

		prev = NULL;
		cur = chm.interval_1s.callback_list;
		while(cur != NULL)
		{
			if(cur->chart_handle == charthandle && cur->series_handle == series_handle)
			{
				if(prev)
					prev->next = cur->next;
				else
					chm.interval_1s.callback_list = cur->next;

				__sync_sub_and_fetch(&(gTraceInfo.custom_chart_callback_count), 1);
				free(cur);
				break;
			}

			prev = cur;
			cur = cur->next;
		}

		pthread_mutex_unlock(&chm.interval_1s.list_mutex);
		break;
	default:
		break;
	}
}


// =====================================================================
// constructor and destructor functions
// =====================================================================

void __attribute__((constructor)) _init_lib()
{
	probeBlockStart();

	memset(&chm, 0, sizeof(chm));
	chm.interval_10ms.timerfd = -1;
	chm.interval_100ms.timerfd = -1;
	chm.interval_1s.timerfd = -1;
	chm.interval_10ms.thread_handle = -1;
	chm.interval_100ms.thread_handle = -1;
	chm.interval_1s.thread_handle = -1;
	pthread_mutex_init(&chm.interval_10ms.list_mutex, NULL);
	pthread_mutex_init(&chm.interval_100ms.list_mutex, NULL);
	pthread_mutex_init(&chm.interval_1s.list_mutex, NULL);

	probeBlockEnd();
}

void __attribute__((destructor)) _fini_lib()
{
	probeBlockStart();

	remove_all_callback_list();
	if (chm.interval_10ms.timerfd != -1)
		close(chm.interval_10ms.timerfd);
	if (chm.interval_100ms.timerfd != -1)
		close(chm.interval_100ms.timerfd);
	if (chm.interval_1s.timerfd != -1)
		close(chm.interval_1s.timerfd);
	/*! Bad. Ugly. Unportable */
	if (chm.interval_10ms.thread_handle != (pthread_t) -1)
		pthread_join(chm.interval_10ms.thread_handle, NULL);
	if (chm.interval_100ms.thread_handle != (pthread_t) -1)
		pthread_join(chm.interval_100ms.thread_handle, NULL);
	if (chm.interval_1s.thread_handle != (pthread_t) -1)
		pthread_join(chm.interval_1s.thread_handle, NULL);

	probeBlockEnd();
}


// =====================================================================
// api definition
// =====================================================================

void da_mark(chart_color color, char* mark_text)
{
	DECLARE_CHART_VARIABLE;

	probeBlockStart();

	setProbePoint(&probeInfo);

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_CUSTOM,
			  API_ID_da_mark,
			  "dp", color, voidp_to_uint64(mark_text));
	PACK_COMMON_END('v', 0, 0, 2);
	PACK_CUSTOM(0, 0, mark_text, color, 0.0f);
	FLUSH_LOCAL_BUF();

	probeBlockEnd();
}

da_handle da_create_chart(char* chart_name)
{
	DECLARE_CHART_VARIABLE;

	// check if there is available chart handle slot
	if(chm.chart_handle_index + 1 >= MAX_CHART_HANDLE)
		return ERR_MAX_CHART_NUMBER;

	// check if chart_name is null
	if(chart_name == NULL)
		return ERR_WRONG_PARAMETER;

	probeBlockStart();
	ret = ++(chm.chart_handle_index);

	setProbePoint(&probeInfo);

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_CUSTOM,
			  API_ID_da_create_chart,
			  "p", voidp_to_uint64(chart_name));
	PACK_COMMON_END('d', ret, 0, 2);
	PACK_CUSTOM(0, 0, chart_name, 0, 0.0f);
	FLUSH_LOCAL_BUF();

	probeBlockEnd();

	return ret;
}

da_handle da_create_series(da_handle charthandle, char* seriesname,
		series_type type, chart_color color)
{
	DECLARE_CHART_VARIABLE;

	// check if charthandle is valid handle or not
	if(charthandle <= 0 || charthandle > chm.chart_handle_index)
		return ERR_WRONG_HANDLE;

	// chech if extra parameter is valid
	if(seriesname == NULL)
		return ERR_WRONG_PARAMETER;

	// check if there is available series spot
	if(chm.series_handle_index[(int)charthandle] + 1 >= MAX_SERIES_PER_CHART)
		return ERR_MAX_CHART_NUMBER;

	probeBlockStart();
	ret = ++(chm.series_handle_index[charthandle]);
	ret += (10 * charthandle);

	setProbePoint(&probeInfo);

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_CUSTOM,
			  API_ID_da_create_series,
			  "dpdd",  charthandle, voidp_to_uint64(seriesname),
			  type, color);
	PACK_COMMON_END('d', ret, 0, 2);
	PACK_CUSTOM(charthandle, type, seriesname, color, 0.0f);
	FLUSH_LOCAL_BUF();

	probeBlockEnd();

	return ret;
}

da_handle da_create_default_series(da_handle charthandle, char* seriesname)
{
	return da_create_series(charthandle, seriesname,
			CHART_TYPE_AUTO, CHART_COLOR_AUTO);
}

int da_set_callback(da_handle series_handle, da_user_data_2_chart_data callback,
		void* data_addr, chart_interval interval)
{
	int cindex, sindex;
	cindex = (int)(series_handle / 10);
	sindex = series_handle % 10;

	// check series handle
	if(cindex <= 0 || cindex > chm.chart_handle_index)
		return ERR_WRONG_HANDLE;

	if(sindex > chm.series_handle_index[(int)cindex])
		return ERR_WRONG_HANDLE;

	// check rest parameters
	if(interval == CHART_NO_CYCLE && callback != NULL)
		return ERR_WRONG_PARAMETER;

	probeBlockStart();

	// remove previously registered callback
	remove_from_callback_list(cindex, series_handle);

	// register new callback
	if(callback != NULL)
	{
		int re;
		add_to_callback_list(interval, cindex, series_handle, callback, data_addr);
		re = start_callback_thread(interval);
		PRINTMSG("start callback thread return %d\n", re);
	}
	probeBlockEnd();

	return 0;
}

void da_log(da_handle series_handle, float uservalue)
{
	DECLARE_CHART_VARIABLE;

	// chech if series handle is valid
	int cindex, sindex;
	cindex = (int)(series_handle / 10);
	sindex = series_handle % 10;

	if(cindex <= 0 || cindex > chm.chart_handle_index)
		return;

	if(sindex > chm.series_handle_index[(int)cindex])
		return;

	probeBlockStart();

	setProbePoint(&probeInfo);

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_CUSTOM,
			  API_ID_da_log,
			  "df", series_handle, uservalue);
	PACK_COMMON_END('v', 0, 0, 2);
	PACK_CUSTOM(series_handle, 0, "", 0, uservalue);
	FLUSH_LOCAL_BUF();

	probeBlockEnd();
}
