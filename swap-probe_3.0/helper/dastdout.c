/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Cherepanov Vitaliy <v.cherepanov@samsung.com>
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include "dahelper.h"

static inline int __redirect(int old, int new)
{
	int ret = 0;

	if (dup2(old, new) == -1) {
		/* fail to print up there. there is no sdterr */
		/* TODO inform about problem */
		fprintf(stderr, "dup2 fail\n");
		ret = -1;
	}

	return ret;
}

int __redirect_std(void)
{
#ifdef STDTOFILE
	char STDOUT[MAX_PATH_LENGTH];
	snprintf(STDOUT,sizeof(STDOUT), "/tmp/da_preloaded_%d_%d.log", getppid(), getpid());
#else
	#define STDOUT		"/dev/null"
#endif

	int ret = 0;
	int fd = open(STDOUT, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	if (fd != -1) {
		if (__redirect(fd, 1) != 0 ||
		    __redirect(fd, 2) != 0) {
			/* fail to print up there. there is no sdterr */
			/* TODO inform about problem */
			/* fprintf(sdterr, "duplicate fd fail\n"); */
			ret = -1;
		}

		close(fd);
	} else {
		/* TODO inform about problem */
		close(1);
		close(2);
	}

	close(0);
	return ret;
}
