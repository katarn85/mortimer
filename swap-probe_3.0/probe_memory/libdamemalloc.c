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
 * Dmitry Bogatov <d.bogatov@samsung.com>
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
#include <malloc.h>
#include "daprobe.h"
#include "probeinfo.h"
#include "dacollection.h"
#include "dautil.h"
#include "da_memory.h"
#include "binproto.h"

static enum DaOptions _sopt = OPT_ALLOC;

void *malloc(size_t size)
{
	static void* (*mallocp)(size_t);
	DECLARE_VARIABLE_STANDARD;
	void *pret;

	rtdl_next_current_set_once(mallocp);

	PRE_PROBEBLOCK();

	pret = (*mallocp)(size);

	if(pret != NULL && gProbeBlockCount == 0)
		add_memory_hash(pret, size, MEMTYPE_ALLOC,
				blockresult ? MEM_EXTERNAL : MEM_INTERNAL);


	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY, API_ID_malloc,
			  "x", (int64_t) size);
	PACK_COMMON_END('p', pret, newerrno, blockresult);
	PACK_MEMORY(size, MEMORY_API_ALLOC, pret);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();

	return pret;
}

void free(void *ptr)
{
	static void (*freep)(void *);
	DECLARE_VARIABLE_STANDARD;

	rtdl_next_current_set_once(freep);

	PRE_PROBEBLOCK();

	if(ptr != NULL && gProbeBlockCount == 0)
		del_memory_hash(ptr, MEMTYPE_FREE, NULL);

	(*freep)(ptr);

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY, API_ID_free, 
			  "p", (int64_t)(int) ptr);
	PACK_COMMON_END('v', 0, newerrno, blockresult);
	PACK_MEMORY(0, MEMORY_API_FREE, ptr);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();
}

void *realloc(void *memblock, size_t size)
{
	static void* (*reallocp)(void*, size_t);
	DECLARE_VARIABLE_STANDARD;
	void *pret;

	rtdl_next_current_set_once(reallocp);
	PRE_PROBEBLOCK();

	if(memblock != NULL && gProbeBlockCount == 0)
		del_memory_hash(memblock, MEMTYPE_FREE, NULL);

	pret = (*reallocp)(memblock, size);

	if(pret != NULL && gProbeBlockCount == 0)
		add_memory_hash(pret, size, MEMTYPE_ALLOC,
				blockresult ? MEM_EXTERNAL : MEM_INTERNAL);

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY, API_ID_realloc,
			  "px", voidp_to_uint64(memblock), (uint64_t) size);
	PACK_COMMON_END('p', pret, newerrno, blockresult);
	PACK_MEMORY(size, MEMORY_API_ALLOC, pret);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();

	return pret;
}


void *temp_calloc(size_t nelem, size_t elsize)
{
	/* Magic number, but somewhy it is sufficent */
	static char extra_mem[20] = {0};
	return (nelem * elsize > sizeof(extra_mem))
		? extra_mem
		: NULL;
}

void *calloc(size_t nelem, size_t elsize)
{
	static void* (*callocp)(size_t, size_t);
	DECLARE_VARIABLE_STANDARD;
	void *pret;

	if (!callocp) {
		/**
		 * Calloc is called by `dlsym`, so we provide small amount
		 * of static memory via `temp_calloc`.
		 */
		callocp = temp_calloc;
		callocp = rtdl_next(__func__);
	}

	PRE_PROBEBLOCK();

	pret = (*callocp)(nelem, elsize);

	if(pret != NULL && gProbeBlockCount == 0)
		add_memory_hash(pret, nelem * elsize, MEMTYPE_ALLOC,
				blockresult ? MEM_EXTERNAL : MEM_INTERNAL);

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY, API_ID_calloc,
			  "xx", (uint64_t)nelem, (uint64_t)elsize);
	PACK_COMMON_END('p', pret, newerrno, blockresult);
	PACK_MEMORY(nelem * elsize, MEMORY_API_ALLOC, pret);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();


	return pret;
}
