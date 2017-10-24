/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Vitaliy Cherepanov <v.cherepanov@samsung.com>
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
 * - Samsung RnD Institute Russia
 *
 */
#ifndef __DAFORKEXEC_H_
#define __DAFORKEXEC_H_

#define INIT_FUNC_EXEC(FUNCNAME)					\
	if(!FUNCNAME##_p) {						\
		void *tmpPtr = dlsym(RTLD_NEXT , #FUNCNAME);	\
		if (tmpPtr == NULL || dlerror() != NULL) {		\
			perror("dlsym failed : " #FUNCNAME);		\
			exit(0);					\
		}							\
		memcpy(&FUNCNAME##_p, &tmpPtr, sizeof(tmpPtr));		\
	}

#define DECLARE(RET, FUNCTION, ...)			\
	typedef RET (* FUNCTION ## _p_t)(__VA_ARGS__);	\
	static FUNCTION ## _p_t FUNCTION ## _p = 0

void init_exec_fork();

#endif /* __DAFORKEXEC_H_ */
