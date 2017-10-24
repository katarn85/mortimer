/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Woojin Jung <woojin2.jung@samsung.com>
 * Jaewon Lim <jaewon81.lim@samsung.com>
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
#include <errno.h>
#include <new>
#include "daprobe.h"
#include "probeinfo.h"
#include "dacollection.h"
#include "da_memory.h"
#include "binproto.h"

static enum DaOptions _sopt = OPT_ALLOC;

void *operator new(std::size_t size) throw (std::bad_alloc)
{
	static void*(*newp)(std::size_t size);
	DECLARE_VARIABLE_STANDARD;
	void *pret;

	GET_REAL_FUNCP_RTLD_NEXT_CPP(_Znwj,newp);

	bfiltering = INTERNAL_NEW_FILTERING;
	PRE_PROBEBLOCK();

	pret = newp(size);

	if(pret != NULL && gProbeBlockCount == 0)
	{
		add_memory_hash(pret, size, MEMTYPE_NEW, blockresult ? MEM_EXTERNAL : MEM_INTERNAL);
	}

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_void__operator_new_std__size_t_size__throw__std__bad_alloc_,
			  "x", (uint64_t)(size));
	PACK_COMMON_END('p', pret, newerrno, blockresult);
	PACK_MEMORY(size, MEMORY_API_ALLOC, pret);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();

	return pret;
}

void *operator new[](std::size_t size) throw (std::bad_alloc)
{
	static void*(*newp)(std::size_t size);
	DECLARE_VARIABLE_STANDARD;
	void *pret;

	GET_REAL_FUNCP_RTLD_NEXT_CPP(_Znaj, newp);

	bfiltering = INTERNAL_NEW_FILTERING;
	PRE_PROBEBLOCK();

	pret = newp(size);

	if(pret != NULL && gProbeBlockCount == 0)
	{
		add_memory_hash(pret, size, MEMTYPE_NEW, blockresult ? MEM_EXTERNAL : MEM_INTERNAL);
	}

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_void__operator_new___std__size_t_size__throw__std__bad_alloc_,
			  "x", (uint64_t)(size));
	PACK_COMMON_END('p', pret, newerrno, blockresult);
	PACK_MEMORY(size, MEMORY_API_ALLOC, pret);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();

	return pret;
}

void operator delete(void *ptr) throw()
{
	static void (*deletep)(void *ptr);
	DECLARE_VARIABLE_STANDARD;
	unsigned short caller;

	GET_REAL_FUNCP_RTLD_NEXT_CPP(_ZdlPv, deletep);

	bfiltering = INTERNAL_DELETE_FILTERING;
	PRE_PROBEBLOCK();

	if(ptr != NULL && gProbeBlockCount == 0)
	{
		probeBlockStart();
		ret = del_memory_hash(ptr, MEMTYPE_DELETE, &caller);
		if(blockresult == 0 && ret == 0 && caller == MEM_EXTERNAL)
		{
			setProbePoint(&probeInfo);
			blockresult = 2;
			probingStart();
		}
		probeBlockEnd();
	}

	deletep(ptr);

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_void_operator_delete_void__ptr__throw__,
			  "p", voidp_to_uint64(ptr));
	PACK_COMMON_END('v', 0, newerrno, blockresult);
	PACK_MEMORY(0, MEMORY_API_FREE, ptr);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();
}

void operator delete[](void *ptr) throw()
{
	static void (*deletep)(void *ptr);
	DECLARE_VARIABLE_STANDARD;
	unsigned short caller;

	GET_REAL_FUNCP_RTLD_NEXT_CPP(_ZdaPv, deletep);

	bfiltering = INTERNAL_DELETE_FILTERING;
	PRE_PROBEBLOCK();

	if(ptr != NULL && gProbeBlockCount == 0)
	{
		probeBlockStart();
		ret = del_memory_hash(ptr, MEMTYPE_DELETE, &caller);
		if(blockresult == 0 && ret == 0 && caller == MEM_EXTERNAL)
		{
			setProbePoint(&probeInfo);
			blockresult = 2;
			probingStart();
		}
		probeBlockEnd();
	}

	deletep(ptr);

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_void_operator_delete___void__ptr__throw__,
			  "p", voidp_to_uint64(ptr));
	PACK_COMMON_END('v', 0, newerrno, blockresult);
	PACK_MEMORY(0, MEMORY_API_FREE, ptr);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();
}

void *operator new(std::size_t size, const std::nothrow_t& nothrow) throw()
{
	static void*(*newp)(std::size_t size, const std::nothrow_t& nothrow);
	DECLARE_VARIABLE_STANDARD;
	void *pret;

	GET_REAL_FUNCP_RTLD_NEXT_CPP(_ZnwjRKSt9nothrow_t, newp);

	bfiltering = INTERNAL_NEW_FILTERING;
	PRE_PROBEBLOCK();

	pret = newp(size, nothrow);

	if(pret != NULL && gProbeBlockCount == 0)
	{
		add_memory_hash(pret, size, MEMTYPE_NEW, blockresult ? MEM_EXTERNAL : MEM_INTERNAL);
	}

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_void__operator_new_std__size_t_size__const_std__nothrow_t__nothrow__throw__,
			  "xp", (uint64_t)(size), voidp_to_uint64(&nothrow));
	PACK_COMMON_END('p', pret, newerrno, blockresult);
	PACK_MEMORY(size, MEMORY_API_ALLOC, pret);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();

	return pret;
}

void *operator new[](std::size_t size, const std::nothrow_t& nothrow) throw()
{
	static void*(*newp)(std::size_t size, const std::nothrow_t& nothrow);
	DECLARE_VARIABLE_STANDARD;
	void *pret;

	GET_REAL_FUNCP_RTLD_NEXT_CPP(_ZnajRKSt9nothrow_t, newp);

	bfiltering = INTERNAL_NEW_FILTERING;
	PRE_PROBEBLOCK();

	pret = newp(size, nothrow);

	if(pret != NULL && gProbeBlockCount == 0)
	{
		add_memory_hash(pret, size, MEMTYPE_NEW, blockresult ? MEM_EXTERNAL : MEM_INTERNAL);
	}

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_void__operator_new___std__size_t_size__const_std__nothrow_t__nothrow__throw__,
			  "xp", (uint64_t)(size), voidp_to_uint64(&nothrow));
	PACK_COMMON_END('p', pret, newerrno, blockresult);
	PACK_MEMORY(size, MEMORY_API_ALLOC, pret);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();

	return pret;
}

void operator delete(void *ptr, const std::nothrow_t& nothrow) throw()
{
	static void (*deletep)(void *ptr, const std::nothrow_t& nothrow);
	DECLARE_VARIABLE_STANDARD;
	unsigned short caller;

	GET_REAL_FUNCP_RTLD_NEXT_CPP(_ZdlPvRKSt9nothrow_t, deletep);

	bfiltering = INTERNAL_DELETE_FILTERING;
	PRE_PROBEBLOCK();

	if(ptr != NULL && gProbeBlockCount == 0)
	{
		probeBlockStart();
		ret = del_memory_hash(ptr, MEMTYPE_DELETE, &caller);
		if(blockresult == 0 && ret == 0 && caller == MEM_EXTERNAL)
		{
			setProbePoint(&probeInfo);
			blockresult = 2;
			probingStart();
		}
		probeBlockEnd();
	}

	deletep(ptr, nothrow);

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_void_operator_delete_void__ptr__const_std__nothrow_t__nothrow__throw__,
			  "pp", voidp_to_uint64(ptr), voidp_to_uint64(&nothrow));
	PACK_COMMON_END('v', 0, newerrno, blockresult);
	PACK_MEMORY(0, MEMORY_API_FREE, ptr);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();
}

void operator delete[](void *ptr, const std::nothrow_t& nothrow) throw()
{
	static void (*deletep)(void *ptr, const std::nothrow_t& nothrow);
	DECLARE_VARIABLE_STANDARD;
	unsigned short caller;

	GET_REAL_FUNCP_RTLD_NEXT_CPP(_ZdaPvRKSt9nothrow_t, deletep);

	bfiltering = INTERNAL_DELETE_FILTERING;
	PRE_PROBEBLOCK();

	if(ptr != NULL && gProbeBlockCount == 0)
	{
		probeBlockStart();
		ret = del_memory_hash(ptr, MEMTYPE_DELETE, &caller);
		if(blockresult == 0 && ret == 0 && caller == MEM_EXTERNAL)
		{
			setProbePoint(&probeInfo);
			blockresult = 2;
			probingStart();
		}
		probeBlockEnd();
	}

	deletep(ptr, nothrow);

	POST_PACK_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_MEMORY,
			  API_ID_void_operator_delete___void__ptr__const_std__nothrow_t__nothrow__throw__,
			  "pp", voidp_to_uint64(ptr), voidp_to_uint64(&nothrow));
	PACK_COMMON_END('v', 0, newerrno, blockresult);
	PACK_MEMORY(0, MEMORY_API_FREE, ptr);
	FLUSH_LOCAL_BUF();

	POST_PACK_PROBEBLOCK_END();
}

