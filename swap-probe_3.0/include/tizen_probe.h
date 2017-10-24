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

#ifndef __BADA_PROBE_H__
#define __BADA_PROBE_H__

#include <string.h>
#include <dlfcn.h>

#ifdef __cplusplus
extern "C" {
#endif


#define GET_REAL_FUNC_TIZEN(FUNCNAME, LIBNAME, FUNCTIONPOINTER)						\
	do {												\
		if(!FUNCTIONPOINTER) {									\
			probeBlockStart();								\
			if(lib_handle[LIBNAME] == NULL) {						\
				lib_handle[LIBNAME] = dlopen(lib_string[LIBNAME], RTLD_LAZY);		\
				if(lib_handle[LIBNAME] == NULL) {					\
					char perror_msg[PERROR_MSG_MAX];				\
					sprintf(perror_msg, "dlopen failed : %s", lib_string[LIBNAME]);	\
					perror(perror_msg);						\
					exit(0);							\
				}									\
			}										\
			void* funcp = dlsym(lib_handle[LIBNAME], #FUNCNAME);				\
			if(funcp == NULL || dlerror() != NULL) {					\
				perror("dlsym failed : " #FUNCNAME);					\
				exit(0);								\
			}										\
			memcpy(&FUNCTIONPOINTER, &funcp, sizeof(void*));				\
			probeBlockEnd();								\
		}											\
	} while(0)

#define PRE_PROBEBLOCK_TIZEN(FILTER)												\
	do {																		\
		if((blockresult = preBlockBegin(CALLER_ADDRESS, FILTER, _sopt)) != 0) {	\
			setProbePoint(&probeInfo);					\
			preBlockEnd();							\
		}									\
	} while(0)

extern int SceneManagerUsed;

#ifdef __cplusplus
}
#endif

#endif
