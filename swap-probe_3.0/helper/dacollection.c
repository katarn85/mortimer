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

#include <assert.h>		// for assert
#include <stdlib.h>		// for malloc, free
#include <string.h>		// for strlen, strcpy
#include <pthread.h>	// for pthread_mutex_t

#include "dahelper.h"
#include "daerror.h"
#include "dacollection.h"

// hash table variable
__hashInfo _hashinfo =
{
	NULL,						// khash_t(symbol)*	symHash
	PTHREAD_MUTEX_INITIALIZER,	// pthread_mutex_t symHashMutex
	NULL,						// khash_t(allocmap)*	memHash
	PTHREAD_MUTEX_INITIALIZER,	// pthread_mutex_t memHashMutex
	NULL,						// khash_t(uiobject)*	uiobjHash
	PTHREAD_MUTEX_INITIALIZER,	// pthread_mutex_t	uiobjHashMutex
	NULL,						// khash_t(object)*	objHash
	PTHREAD_MUTEX_INITIALIZER,	// pthread_mutex_t objHashMutex
	NULL,						// khash_t(detector)*	dttHash
	PTHREAD_MUTEX_INITIALIZER	// pthread_mutex_t dttHashMutex
};

// glist typedef and variable
struct _element
{
	int		keylen;
	char*	keystr;
	void*	dataptr;
	struct _element*	next;
	struct _element*	prev;
};

typedef struct _element element_t;

element_t* gsymbol_list = NULL;
pthread_mutex_t glist_mutex = PTHREAD_MUTEX_INITIALIZER;

// *********************************************************************************************
// hash table related implemenation
// *********************************************************************************************

int initialize_hash_table()
{
	SYMBOLHASH_LOCK;
	SYMBOLHASH = kh_init(symbol);
	SYMBOLHASH_UNLOCK;

	MEMORYHASH_LOCK;
	MEMORYHASH = kh_init(allocmap);
	MEMORYHASH_UNLOCK;

	UIOBJECTHASH_LOCK;
	UIOBJECTHASH = kh_init(uiobject);
	UIOBJECTHASH_UNLOCK;

	OBJECTHASH_LOCK;
	OBJECTHASH = kh_init(object);
	OBJECTHASH_UNLOCK;

	DETECTORHASH_LOCK;
	DETECTORHASH = kh_init(detector);
	DETECTORHASH_UNLOCK;

	return 0;
}

int finalize_hash_table()
{
	if (SYMBOLHASH)
	{
		khiter_t k;
		char* val;

		SYMBOLHASH_LOCK;
		for(k = kh_begin(SYMBOLHASH); k != kh_end(SYMBOLHASH); k++)
		{
			if (kh_exist(SYMBOLHASH, k))
			{
				val = kh_value(SYMBOLHASH, k);
				free(val);
			}
		}
		kh_destroy(symbol, SYMBOLHASH);
		SYMBOLHASH = NULL;
		SYMBOLHASH_UNLOCK;
	}

	if (MEMORYHASH)
	{
		MEMORYHASH_LOCK;
		kh_destroy(allocmap, MEMORYHASH);
		MEMORYHASH = NULL;
		MEMORYHASH_UNLOCK;
	}

	if (UIOBJECTHASH)
	{
		khiter_t k;
		_uiobjectinfo* val;

		UIOBJECTHASH_LOCK;
		for(k = kh_begin(UIOBJECTHASH); k != kh_end(UIOBJECTHASH); k++)
		{
			if (kh_exist(UIOBJECTHASH, k))
			{
				val = kh_value(UIOBJECTHASH, k);
				if (likely(val->type != 0)) free(val->type);
				if (likely(val->name != 0)) free(val->name);
				free(val);
			}
		}
		kh_destroy(uiobject, UIOBJECTHASH);
		UIOBJECTHASH = NULL;
		UIOBJECTHASH_UNLOCK;
	}

	if (OBJECTHASH)
	{
		OBJECTHASH_LOCK;
		kh_destroy(object, OBJECTHASH);
		OBJECTHASH = NULL;
		OBJECTHASH_UNLOCK;
	}

	if (DETECTORHASH)
	{
		DETECTORHASH_LOCK;
		kh_destroy(detector, DETECTORHASH);
		DETECTORHASH = NULL;
		DETECTORHASH_UNLOCK;
	}

	return 0;
}

/***********************************************************
 * symbol hash related functions
 ***********************************************************/
// return 0 if there is no entry in hash
// return 1 if there is entry in hash
// return negative value if error occurred
int find_symbol_hash(void* ptr, char** psymbol)
{
	khiter_t k;
	int ret = 0;

	if (unlikely(SYMBOLHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	if (unlikely(psymbol == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();

	SYMBOLHASH_LOCK;
	k = kh_get(symbol, SYMBOLHASH, (uint32_t)ptr);
	if (k == kh_end(SYMBOLHASH))		// there is no entry for key
	{
		ret = 0;
	}
	else
	{
		*psymbol = kh_value(SYMBOLHASH, k);
		ret = 1;
	}
	SYMBOLHASH_UNLOCK;
	probeBlockEnd();
	return ret;
}

// return 0 if succeed
// return 1 if there is already exist in hash
// return negative value if other error occurred
int add_symbol_hash(void* ptr, const char* str, int strlen)
{
	khiter_t k;
	int rethash, ret = 0;

	if (unlikely(SYMBOLHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	if (unlikely(str == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();

	SYMBOLHASH_LOCK;
	k = kh_put(symbol, SYMBOLHASH, (uint32_t)ptr, &rethash);
	if (likely(rethash != 0))	// succeed to add in hash table
	{
		char* tlast = (char*)real_malloc(strlen);
		if (likely(tlast != NULL))
		{
			memcpy(tlast, str, strlen);
			kh_value(SYMBOLHASH, k) = tlast;
		}
		else
		{
			kh_del(symbol, SYMBOLHASH, k);
			ret = ERR_OUTOFMEMORY;
		}
	}
	else
	{
		// TODO : error handling
		ret = 1;
	}
	SYMBOLHASH_UNLOCK;
	probeBlockEnd();
	return ret;
}

/***********************************************************
 * memory hash related functions
 ***********************************************************/
// return 0 if succeed
// return 1 if key is already exist in hash table
// return negative value if other error occurred
int add_memory_hash(void* ptr, size_t size, unsigned short type, unsigned short caller)
{
	khiter_t k;
	int rethash, ret = 0;
	size_t memsize;
	uint64_t meminfo;

	if (unlikely(MEMORYHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();
	MEMORYHASH_LOCK;
	k = kh_put(allocmap, MEMORYHASH, (uint32_t)ptr, &rethash);
	if (likely(rethash != 0))	// succeed to add in hash table
	{
		kh_value(MEMORYHASH, k) = MAKE_MEMINFO(caller, type, size);
		update_heap_memory_size(true, size);
	}
	else
	{
		// key is already exist in hash
		// update memory info
		meminfo = kh_value(MEMORYHASH, k);
		memsize = GET_MEMSIZE(meminfo);
		if (memsize == size)
		{
			kh_value(MEMORYHASH, k) = MAKE_MEMINFO(caller, type, size);
		}
		ret = 1;
	}
	MEMORYHASH_UNLOCK;
	probeBlockEnd();
	return ret;
}

// return 0 if succeed
// return 1 if key is not in hash table
// return negative if other error occurred
int del_memory_hash(void* ptr, unsigned short type, unsigned short* caller)
{
	khiter_t k;
	int ret = 0;
	uint32_t size;
	uint64_t meminfo;

	if (unlikely(MEMORYHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();
	MEMORYHASH_LOCK;
	k = kh_get(allocmap, MEMORYHASH, (uint32_t)ptr);
	if (likely(k != kh_end(MEMORYHASH)))
	{				// there is entry in hash table
		meminfo = kh_value(MEMORYHASH, k);
		if (unlikely(type != GET_MEMTYPE(meminfo)))
		{
			ret = -1;
		}
		else
		{
			size = GET_MEMSIZE(meminfo);
			if (caller != NULL)
				*caller = GET_MEMCALLER(meminfo);
			update_heap_memory_size(false, size);
			kh_del(allocmap, MEMORYHASH, k);
		}
	}
	else
	{
		ret = 1;	// there is not entry in hash table
	}
	MEMORYHASH_UNLOCK;
	probeBlockEnd();

	return ret;
}

/***********************************************************
 * uiobject hash related functions
 ***********************************************************/
// return 0 if there is no entry in hash
// return 1 if there is entry in hash
// return negative value if error occurred
int find_uiobject_hash(void* ptr, char** type, char** classname)
{
	khiter_t k;
	int ret = 0;

	if (unlikely(UIOBJECTHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	if (unlikely(type == NULL))
		return ERR_WRONGPARAMETER;

	if (unlikely(classname == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();

	UIOBJECTHASH_LOCK;
	k = kh_get(uiobject, UIOBJECTHASH, (uint32_t)ptr);
	if (unlikely(k == kh_end(UIOBJECTHASH)))		// there is no entry for key
	{
		ret = 0;
	}
	else
	{
		*classname = kh_value(UIOBJECTHASH, k)->name;
		*type = kh_value(UIOBJECTHASH, k)->type;
		ret = 1;
	}
	UIOBJECTHASH_UNLOCK;
	probeBlockEnd();
	return ret;
}

// return 0 if succeed
// return 1 if there is no entry in hash
// return negative value if other error occurred
int add_uiobject_hash_class(void* ptr, const char* classname)
{
	int str_len;
	khiter_t k;
	int rethash, ret = 0;

	if (unlikely(UIOBJECTHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	if (unlikely(classname == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();

	str_len = strlen(classname) + 1;

	UIOBJECTHASH_LOCK;
	k = kh_put(uiobject, UIOBJECTHASH, (uint32_t)ptr, &rethash);
	if (likely(rethash == 0))	// entry is already in hash table
	{
		if (likely(kh_value(UIOBJECTHASH, k) != NULL))
		{
			if (kh_value(UIOBJECTHASH, k)->name == NULL)
			{
				char* tlast = (char*)real_malloc(str_len);
				if (likely(tlast != NULL))
				{
					memcpy(tlast, classname, str_len);
					kh_value(UIOBJECTHASH, k)->name = tlast;
				}
				else
				{
					kh_value(UIOBJECTHASH, k)->name = NULL;
					ret = ERR_OUTOFMEMORY;	// out of memory
				}
			}
			else
				ret = ERR_ALREADYEXIST;
		}
		else	// not possible
			ret = ERR_NOTEXIST;	// there is entry but there is no value
	}
	else	// error
		ret = 1;	// there is no entry

	UIOBJECTHASH_UNLOCK;
	probeBlockEnd();
	return ret;
}

// return 0 if succeed
// return 1 if there is already exist in hash
// return negative value if other error occurred
int add_uiobject_hash_type(void* ptr, const char* type)
{
	int str_len;
	khiter_t k;
	int rethash, ret = 0;

	if (unlikely(UIOBJECTHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	if (unlikely(type == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();

	str_len = strlen(type) + 1;

	UIOBJECTHASH_LOCK;
	k = kh_put(uiobject, UIOBJECTHASH, (uint32_t)ptr, &rethash);
	if (likely(rethash != 0))	// succeed to add in hash table
	{
		char* tlast;
		_uiobjectinfo* newentry;

		newentry = (_uiobjectinfo*)calloc(1, sizeof(_uiobjectinfo));
		if (likely(newentry != NULL))
		{
			kh_value(UIOBJECTHASH, k) = newentry;

			tlast = (char*)real_malloc(str_len);
			if (likely(tlast != NULL))
			{
				memcpy(tlast, type, str_len);
				kh_value(UIOBJECTHASH, k)->type = tlast;
			}
			else
			{
				kh_value(UIOBJECTHASH, k)->type = NULL;
				ret = ERR_OUTOFMEMORY;
			}
		}
		else
		{
			kh_del(uiobject, UIOBJECTHASH, k);
			ret = ERR_OUTOFMEMORY;
		}
	}
	else
		ret = 1;

	UIOBJECTHASH_UNLOCK;
	probeBlockEnd();
	return ret;
}

// return 0 if succeed
// return 1 if key is not in hash table
// return negative value if other error occurred
int del_uiobject_hash(void* ptr)
{
	khiter_t k;
	_uiobjectinfo* val;
	int ret = 0;

	if (unlikely(UIOBJECTHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();
	UIOBJECTHASH_LOCK;
	k = kh_get(uiobject, UIOBJECTHASH, (uint32_t)ptr);
	if (likely(k != kh_end(UIOBJECTHASH)))		// there is entry in hash table
	{
		val = kh_value(UIOBJECTHASH, k);
		kh_del(uiobject, UIOBJECTHASH, k);
		if (likely(val->type != NULL)) free(val->type);
		if (likely(val->name != NULL)) free(val->name);
		free(val);
	}
	else
	{
		ret = 1;
	}
	UIOBJECTHASH_UNLOCK;
	probeBlockEnd();

	return ret;
}

/***********************************************************
 * object hash related functions
 ***********************************************************/
// return 0 if there is no entry in hash
// return 1 if there is entry in hash
// return negative value if error occurred
int find_object_hash(void* ptr, unsigned short *caller)
{
	khiter_t k;
	int ret = 0;

	if (unlikely(OBJECTHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	if (unlikely(caller == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();

	OBJECTHASH_LOCK;
	k = kh_get(object, OBJECTHASH, (uint32_t)ptr);
	if (unlikely(k == kh_end(OBJECTHASH)))		// there is no entry for key
	{
		ret = 0;
	}
	else
	{
		*caller = kh_value(OBJECTHASH, k);
		ret = 1;
	}
	OBJECTHASH_UNLOCK;
	probeBlockEnd();
	return ret;
}

// return 0 if succeed
// return 1 if there is no entry in hash
// return negative value if other error occurred
int add_object_hash(void* ptr, unsigned short caller)
{
	khiter_t k;
	int rethash, ret = 0;

	if (unlikely(OBJECTHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();

	OBJECTHASH_LOCK;
	k = kh_put(object, OBJECTHASH, (uint32_t)ptr, &rethash);
	if (likely(rethash != 0))	// entry is already in hash table
	{
		kh_value(OBJECTHASH, k) = caller;
	}
	else
	{
		// TODO : error handling
		ret = 1;
	}

	OBJECTHASH_UNLOCK;
	probeBlockEnd();
	return ret;
}

// return 0 if succeed
// return 1 if key is not in hash table
// return negative value if other error occurred
int del_object_hash(void* ptr, unsigned short *caller)
{
	khiter_t k;
	int ret = 0;

	if (unlikely(OBJECTHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	if (unlikely(caller == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();

	OBJECTHASH_LOCK;
	k = kh_get(object, OBJECTHASH, (uint32_t)ptr);
	if (likely(k != kh_end(OBJECTHASH)))		// there is entry in hash table
	{
		*caller = kh_value(OBJECTHASH, k);
		kh_del(object, OBJECTHASH, k);
	}
	else
	{
		ret = 1;
	}
	OBJECTHASH_UNLOCK;
	probeBlockEnd();

	return ret;
}

/***********************************************************
 * detector hash related functions
 ***********************************************************/
// return 0 if succeed
// return 1 if there is already exist in hash
// return negative value if other error occurred
int add_detector_hash(void* ptr, void* listener)
{
	khiter_t k;
	int rethash, ret = 0;

	if (unlikely(DETECTORHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	if (unlikely(listener == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();

	DETECTORHASH_LOCK;
	k = kh_put(detector, DETECTORHASH, (uint32_t)ptr, &rethash);
	if (likely(rethash != 0))	// succeed to add in hash table
	{
		kh_value(DETECTORHASH, k) = listener;
	}
	else
	{
		// TODO : error handling
		ret = 1;
	}
	DETECTORHASH_UNLOCK;
	probeBlockEnd();
	return ret;
}

// return 0 if succeed
// return 1 if key is not in hash table
// return negative value if other error occurred
int del_detector_hash(void* ptr)
{
	khiter_t k;
	int ret = 0;

	if (unlikely(DETECTORHASH == 0))
		return ERR_NOTINITIALIZED;

	if (unlikely(ptr == NULL))
		return ERR_WRONGPARAMETER;

	probeBlockStart();
	DETECTORHASH_LOCK;
	k = kh_get(detector, DETECTORHASH, (uint32_t)ptr);
	if (likely(k != kh_end(DETECTORHASH)))		// there is entry in hash table
	{
		kh_del(detector, DETECTORHASH, k);
	}
	else
	{
		ret = 1;
	}
	DETECTORHASH_UNLOCK;
	probeBlockEnd();

	return ret;
}


// *********************************************************************************************
// glist implemenation
// *********************************************************************************************

// all function to call this function have to use mutex_lock
static element_t* find_element(char* key)
{
	element_t* res;
	int keylen;

	if (unlikely(key == NULL))
		return NULL;

	keylen = strlen(key);

//	pthread_mutex_lock(&glist_mutex);	// this is commented because of static function
	res = gsymbol_list;
	while(res != NULL)
	{
		if (keylen == res->keylen && (strcmp(key, res->keystr) == 0))
			break;
		res = res->next;
	}
//	pthread_mutex_unlock(&glist_mutex);	// this is commented because of static function

	return res;
}

int add_to_glist(char* key, void* data)
{
	element_t* elm;
	int ret = 0;

	if (unlikely(key == NULL || data == NULL))
		return 0;

	pthread_mutex_lock(&glist_mutex);
	elm = find_element(key);
	if (elm == NULL)
	{
		elm = (element_t*)real_malloc(sizeof(element_t));
		if (likely(elm != NULL))
		{
			elm->keylen = strlen(key);
			elm->keystr = (char*)real_malloc(elm->keylen + 1);
			if (likely(elm->keystr != NULL))
			{
				memcpy(elm->keystr, key, elm->keylen + 1);
				elm->dataptr = data;
				elm->next = gsymbol_list;
				elm->prev = NULL;
				if (gsymbol_list)
					gsymbol_list->prev = elm;
				gsymbol_list = elm;
				ret = 1;
			}
			else
			{
				free(elm);
			}
		}
	}
	pthread_mutex_unlock(&glist_mutex);

	return ret;
}

int remove_from_glist(char* key)
{
	element_t* elm;
	int ret = 0;

	pthread_mutex_lock(&glist_mutex);
	elm = find_element(key);
	if (elm != NULL)
	{
		if (elm->prev)
			elm->prev->next = elm->next;
		if (elm->next)
			elm->next->prev = elm->prev;
		if (gsymbol_list == elm)
			gsymbol_list = gsymbol_list->next;
	}
	pthread_mutex_unlock(&glist_mutex);

	if (elm != NULL)
	{
		free(elm->dataptr);
		free(elm->keystr);
		free(elm);
		ret = 1;
	}

	return ret;
}

int remove_all_glist()
{
	element_t* elm;

	pthread_mutex_lock(&glist_mutex);
	while(gsymbol_list != NULL)
	{
		elm = gsymbol_list;
		gsymbol_list = elm->next;
		free(elm->dataptr);
		free(elm->keystr);
		free(elm);
	}
	pthread_mutex_unlock(&glist_mutex);

	return 1;
}

void* find_glist(char* key)
{
	element_t* elm;

	pthread_mutex_lock(&glist_mutex);
	elm = find_element(key);
	pthread_mutex_unlock(&glist_mutex);

	if (elm)
		return elm->dataptr;
	else
		return NULL;
}

