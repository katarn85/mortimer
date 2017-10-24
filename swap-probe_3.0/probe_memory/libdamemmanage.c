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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <memory.h>
#include <errno.h>
#include "daprobe.h"
#include "probeinfo.h"
#include "dautil.h"
#include "da_memory.h"
#include "binproto.h"
#include "common_probe_init.h"
#include "real_functions.h"

static enum DaOptions _sopt = OPT_ALLOC;

void *memset(void *memblock, int c, size_t n)
{
	static void *(*memsetp)(void *,int,size_t);
	DECLARE_VARIABLE_STANDARD;
	void *pret;

	GET_REAL_FUNC_RTLD_NEXT(memset);

	PRE_PROBEBLOCK();

	pret = memsetp(memblock, c, n);

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_memset,
			  "pdx", voidp_to_uint64(memblock), c,
			  (uint64_t)(n));
	PACK_COMMON_END('p', pret, newerrno, blockresult);
	PACK_MEMORY(n, MEMORY_API_MANAGE, pret);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();

	return pret;
}

int memcmp(const void * ptr1, const void * ptr2, size_t num)
{
	static int(*memcmpp)(const void *,const void *,size_t);
	DECLARE_VARIABLE_STANDARD;

	GET_REAL_FUNC_RTLD_NEXT(memcmp);

	PRE_PROBEBLOCK();

	ret = memcmpp(ptr1, ptr2, num);

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_memcmp,
			  "ppx", voidp_to_uint64(ptr1), voidp_to_uint64(ptr2),
			  (uint64_t)(num));
	PACK_COMMON_END('d', ret, newerrno, blockresult);
	PACK_MEMORY(num, MEMORY_API_MANAGE, ret);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();

	return ret;
}

void *memcpy(void * destination, const void * source, size_t num )
{
	static void *(*memcpyp)(void *,const void *,size_t);
	DECLARE_VARIABLE_STANDARD;
	void *pret;

	GET_REAL_FUNC_RTLD_NEXT(memcpy);

	PRE_PROBEBLOCK();

	pret = memcpyp(destination, source, num);

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_memcpy,
			  "ppx", voidp_to_uint64(destination),
			  voidp_to_uint64(source),
			  (uint64_t)(num));
	PACK_COMMON_END('p', pret, newerrno, blockresult);
	PACK_MEMORY(num, MEMORY_API_MANAGE, pret);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();

	return pret;
}

