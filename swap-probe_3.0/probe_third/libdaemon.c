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

#include <string.h>	// for memcpy
#include <errno.h>	// for errno

#include "daprobe.h"
#include "dahelper.h"

#include "real_functions.h"

int daemon_close_allv(const int except_fds[])
{
	static int (*daemon_close_allvp)(const int except_fds[]);
	int i, saved_errno, ret;
	int* fds;

	GET_REAL_FUNCP(daemon_close_allv, LIBDAEMON, daemon_close_allvp);

	probeBlockStart();
	// get number of fds
	for(i = 0; ; i++)
	{
		if(except_fds[i] == -1)
			break;
	}

	// allocate memory for new except fds
	fds = (int*)real_malloc((i + 2) * sizeof(int));

	// copy fds
	if(fds)
	{
		if(i > 0)
			memcpy(fds, except_fds, i * sizeof(int));
		fds[i] = gTraceInfo.socket.daemonSock;
		fds[i + 1] = -1;
	}
	else
	{
		// do nothing
	}
	probeBlockEnd();

	// call original function
	if(fds)
	{
		ret = daemon_close_allvp(fds);
	}
	else
	{
		ret = daemon_close_allvp(except_fds);
	}

	probeBlockStart();
	saved_errno = errno;
	free(fds);
	errno = saved_errno;
	probeBlockEnd();

	return ret;
}

