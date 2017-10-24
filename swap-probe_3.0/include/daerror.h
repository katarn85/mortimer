/*  DA probe
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

#ifndef DAERROR_H_
#define DAERROR_H_

#ifdef __cplusplus
extern "C"{
#endif

#define ERR_SUCCESS				0
#define ERR_OUTOFMEMORY			-11
#define ERR_WRONGPARAMETER		-12
#define ERR_NOTINITIALIZED		-13
#define ERR_NOTEXIST			-21
#define ERR_ALREADYEXIST		-22
#define ERR_OTHER				-30
#define ERR_DLOPEN				-101
#define ERR_DLSYM				-102
#define ERR_USER				-10000


#ifdef __cplusplus
}
#endif


#endif /* DAERROR_H_ */
