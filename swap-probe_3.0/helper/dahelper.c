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

#include <wchar.h>			// for wcslen
#include <unistd.h>			// for unlink
#include <sys/types.h>
#include <dirent.h>			// for opendir, readdir
#include <assert.h>
#include "dahelper.h"

int app_efl_main_flg = 0;

const char *lib_string[NUM_ORIGINAL_LIBRARY] = {
	"libc.so.6",				//0
	"libpthread.so.0",			//1
	"libelementary.so.1",			//2
	"libecore_input_evas.so.1",		//3
	"libdaemon.so.0",			//4
	"libcapi-appfw-application.so.0",	//5
	"libGLESv2.so",				//6
	"libEGL.so",				//7
	SELF_LIB_NAME				//8
};
void *lib_handle[NUM_ORIGINAL_LIBRARY];

/* trace info global variable */
__traceInfo gTraceInfo =
{
	{
		1,							// int eventIndex
		PTHREAD_MUTEX_INITIALIZER	// pthread_mutex_t eventMutex
	},		// __indexInfo
	{
		-1,							// int daemonSock
		PTHREAD_MUTEX_INITIALIZER	// ptrhread_mutex_t sockMutex
	},		// __socketInfo
	{
		{0, },						// char appName[128]
		0							// unsigned int startTime
	},		// __appInfo
	{
		0,							// int state
		PTHREAD_MUTEX_INITIALIZER	// ptrhread_mutex_t ssMutex
	},		// __screenshotInfo
	{
		NULL,						// map_start
		NULL						// map_end
	},		// __mapInfo
	-1,								// int stateTouch
	0,								// int init_complete
	0,								// int custom_chart_callback_count
	0								// unsigned long optionflag
};

void WcharToChar(char* pstrDest, const wchar_t* pwstrSrc)
{
	int nLen=(int)wcslen(pwstrSrc);
	wcstombs(pstrDest, pwstrSrc, nLen+1);
}

// return 0 if succeed
// return -1 if error occured
int remove_indir(const char *dirname)
{
	DIR *dir;
	struct dirent *entry;
	char path[MAX_PATH_LENGTH];
	static char dirent_buffer[ sizeof(struct dirent) + PATH_MAX + 1 ] = {0,};
	static struct dirent *dirent_r = (struct dirent *)dirent_buffer;


	dir = opendir(dirname);
	if(dir == NULL)
	{
		return -1;
	}

	while ((readdir_r(dir, dirent_r, &entry) == 0) && entry) {
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
		{
			snprintf(path, (size_t) MAX_PATH_LENGTH, "%s/%s", dirname, entry->d_name);
			if (entry->d_type != DT_DIR)	// file
			{
				unlink(path);
			}
			else { }	// directory
		}
	}
	closedir(dir);

	return 0;
}

static int absolute_filepath_p(const char *fname)
{
	return fname[0] == '/';
}

char *absolutize_filepath(const char *fname, char *buffer, size_t bufsiz)
{
	char cwd[PATH_MAX];

	assert(fname && "Filename, passed to stdc function is NULL.");
	if (absolute_filepath_p(fname) || getcwd(cwd, sizeof(cwd)) == NULL)
		snprintf(buffer, bufsiz, "%s", fname);
	else
		snprintf(buffer, bufsiz, "%s/%s", cwd, fname);
	return buffer;
}

char *real_abs_path(int fd, char *buffer, size_t bufsiz)
{
	static const char *PROC_FD = "/proc/self/fd/%d";
	char proc_path[sizeof(PROC_FD) + 16]; /* PATH_MAX not needed here */
	ssize_t ret = 0;

	if (fd < 0)
		return NULL;

	snprintf(proc_path, sizeof(proc_path), PROC_FD, fd);

	ret = readlink(proc_path, buffer, bufsiz);
	if (ret < 0) /* some error occured */
		return NULL;
	buffer[ret] = '\0';

	return buffer;
}
