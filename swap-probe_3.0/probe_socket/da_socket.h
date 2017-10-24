/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Hyunjong Park <phjwithyou.park@samsung.com>
 * Juyoung Kim <j0.kim@samsung.com>
 * Anastasia Lyupa <a.lyupa@samsung.com
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

#ifndef __DA_SOCKET_H__
#define __DA_SOCKET_H__

#include "daprobe.h"


#define SOCKET_SEND_SIZE 1024
#define NO_TOTAL_INFO (NULL)
#define POST_PROBEBLOCK_MIDDLE_LIBC_SOCK(OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO)				\
	do {							\
		BUF_PTR = pack_int64(BUF_PTR, (uintptr_t)OBJECTPTR);		\
		BUF_PTR = pack_int64(BUF_PTR, FDVALUE);		\
		BUF_PTR = pack_int32(BUF_PTR, APITYPE);		\
		struct sockaddr *tmp_dest = ((struct sockaddr *)TOTAL_INFO.sock); \
		if ( tmp_dest != NO_TOTAL_INFO) { \
			switch (tmp_dest->sa_family) { \
				case AF_INET:\
					BUF_PTR = pack_int32(BUF_PTR, ((struct sockaddr_in *)tmp_dest)->sin_addr.s_addr ); \
					BUF_PTR = pack_int32(BUF_PTR, ntohs(((struct sockaddr_in *)tmp_dest)->sin_port ) ); \
					break;\
				/*case AF_INET6:\
					BUF_PTR = pack_int32(BUF_PTR, ((struct sockaddr_in6 *)tmp_dest)->sin6_addr.s6_addr32 ); \
					BUF_PTR = pack_int32(BUF_PTR, ((struct sockaddr_in6 *)tmp_dest)->sin6_port ); \
					break;*/ \
				default:\
					BUF_PTR = pack_int32(BUF_PTR, (uint32_t) 0);	\
					BUF_PTR = pack_int32(BUF_PTR, (uint32_t) 0);	\
					break; \
			} \
		} else { \
			BUF_PTR = pack_int32(BUF_PTR, (uint32_t) 0);	\
			BUF_PTR = pack_int32(BUF_PTR, (uint32_t) 0);	\
		}; \
		BUF_PTR = pack_int64(BUF_PTR, TOTAL_INFO.msg_total_size);		\
		BUF_PTR = pack_bin(BUF_PTR, TOTAL_INFO.msg_buf, TOTAL_INFO.msg_pack_size);		\
	} while (0)


//lib Common Function
#define AFTER_ORIGINAL_LIBC_SOCK(RTYPE, RVAL, OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO, INPUTFORMAT, ...)      \
		POST_PROBEBLOCK_BEGIN(LC_SOCKET, RTYPE, RVAL, INPUTFORMAT, __VA_ARGS__);		\
		POST_PROBEBLOCK_MIDDLE_LIBC_SOCK(OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO );								\
		POST_PROBEBLOCK_END();

//lib START Function
#define AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START(RTYPE, RVAL,OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO, INPUTFORMAT, ...)	\
	POST_PROBEBLOCK_FUNC_START_BEGIN(LC_SOCKET, RTYPE, RVAL, INPUTFORMAT, __VA_ARGS__);				\
	POST_PROBEBLOCK_MIDDLE_LIBC_SOCK(OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO );					\
	FLUSH_LOCAL_BUF();												\
	PRE_PROBEBLOCK_END()

#define POST_PROBEBLOCK_FUNC_START_BEGIN(LCTYPE, RTYPE, RETVALUE, INPUTFORMAT, ...)	\
		newerrno = errno;														\
			if(postBlockBegin(blockresult)) {										\
				PREPARE_LOCAL_BUF(); \
				PACK_COMMON_BEGIN(MSG_PROBE_NETWORK, vAPI_ID, INPUTFORMAT, __VA_ARGS__);\
				PACK_COMMON_END(RTYPE, RETVALUE, errno, blockresult)


//lib END Function
#define AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END(RTYPE, RVAL, OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO, INPUTFORMAT, ...)	\
	POST_PROBEBLOCK_FUNC_END_BEGIN(LC_SOCKET, RTYPE, RVAL,	INPUTFORMAT, __VA_ARGS__);		\
	POST_PROBEBLOCK_MIDDLE_LIBC_SOCK(OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO );								\
	POST_PROBEBLOCK_END()

#define POST_PROBEBLOCK_FUNC_END_BEGIN(LCTYPE, RTYPE, RETVALUE, INPUTFORMAT, ...)	\
	newerrno = errno;														\
	if(postBlockBegin(blockresult)) {										\
		setProbePoint(&probeInfo);										\
		PREPARE_LOCAL_BUF(); \
		PACK_COMMON_BEGIN(MSG_PROBE_NETWORK, vAPI_ID, INPUTFORMAT, __VA_ARGS__);\
		PACK_COMMON_END(RTYPE, RETVALUE, errno, blockresult)

#define POST_PROBEBLOCK_MIDDLE_TIZEN_SOCK(OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO )				\
	do {									\
		BUF_PTR = pack_int64(BUF_PTR, (uintptr_t)OBJECTPTR);		\
		BUF_PTR = pack_int64(BUF_PTR, FDVALUE);				\
		BUF_PTR = pack_int32(BUF_PTR, APITYPE);				\
		BUF_PTR = pack_int32(BUF_PTR, TOTAL_INFO.host_ip);		\
		BUF_PTR = pack_int32(BUF_PTR, TOTAL_INFO.host_port);		\
		BUF_PTR = pack_int64(BUF_PTR, TOTAL_INFO.msg_total_size);	\
		BUF_PTR = pack_bin(BUF_PTR, TOTAL_INFO.msg_buf,			\
				   TOTAL_INFO.msg_pack_size);			\
	} while (0)



//Tizen Common Function
#define AFTER_ORIGINAL_TIZEN_SOCK(APINAME, RTYPE, RVAL, OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO, INPUTFORMAT, ...)	\
	POST_PROBEBLOCK_TIZEN_BEGIN(APINAME, LC_SOCKET, RTYPE, RVAL, INPUTFORMAT, __VA_ARGS__);		\
	POST_PROBEBLOCK_MIDDLE_TIZEN_SOCK(OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO );								\
	POST_PROBEBLOCK_END()

// Tizen Start Function
#define AFTER_ORIGINAL_TIZEN_SOCK_WAIT_FUNC_START(APINAME, RTYPE, RVAL,OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO, INPUTFORMAT, ...)	\
	POST_PROBEBLOCK_TIZEN_FUNC_START_BEGIN(APINAME,LC_SOCKET, RTYPE, RVAL, INPUTFORMAT, __VA_ARGS__);		\
	POST_PROBEBLOCK_MIDDLE_TIZEN_SOCK(OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO );								\
	FLUSH_LOCAL_BUF();						\
	PRE_PROBEBLOCK_END();

//Tizen END Function
#define AFTER_ORIGINAL_TIZEN_SOCK_WAIT_FUNC_END(APINAME,RTYPE, RVAL,OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO, INPUTFORMAT, ...)	\
	POST_PROBEBLOCK_TIZEN_FUNC_END_BEGIN(APINAME,LC_SOCKET, RTYPE, RVAL, INPUTFORMAT, __VA_ARGS__);		\
	POST_PROBEBLOCK_MIDDLE_TIZEN_SOCK(OBJECTPTR, FDVALUE, APITYPE, TOTAL_INFO );								\
	POST_PROBEBLOCK_END()

#define POST_PROBEBLOCK_TIZEN_BEGIN(APINAME, LCTYPE, RTYPE, RETVALUE, INPUTFORMAT, ...)	\
	if(postBlockBegin(blockresult)) {										\
		PREPARE_LOCAL_BUF(); \
		PACK_COMMON_BEGIN(MSG_PROBE_NETWORK, vAPI_ID, INPUTFORMAT, __VA_ARGS__);\
		PACK_COMMON_END(RTYPE, RETVALUE, errno, blockresult)

#define APPEND_NETWORK_LOG_BASIC(LCTYPE, APINAME)								\
	log.length = sprintf(log.data, "%d`,%d`,%s`,%lu`,%d`,%d",	\
			LCTYPE, probeInfo.eventIndex, APINAME,				\
			probeInfo.currentTime, probeInfo.pID, probeInfo.tID)

#define POST_PROBEBLOCK_TIZEN_FUNC_START_BEGIN(APINAME, LCTYPE, RTYPE, RETVALUE, INPUTFORMAT, ...)	\
			if(postBlockBegin(blockresult)) {										\
				PREPARE_LOCAL_BUF(); \
				PACK_COMMON_BEGIN(MSG_PROBE_NETWORK, vAPI_ID, INPUTFORMAT, __VA_ARGS__);\
				PACK_COMMON_END(RTYPE, RETVALUE, errno, blockresult)

#define POST_PROBEBLOCK_TIZEN_FUNC_END_BEGIN(APINAME, LCTYPE, RTYPE, RETVALUE, INPUTFORMAT, ...)	\
	if(postBlockBegin(blockresult)) {										\
		setProbePoint(&probeInfo);										\
		PREPARE_LOCAL_BUF(); \
		PACK_COMMON_BEGIN(MSG_PROBE_NETWORK, vAPI_ID, INPUTFORMAT, __VA_ARGS__);\
		PACK_COMMON_END(RTYPE, RETVALUE, errno, blockresult)

#define BEFORE_ORIGINAL_SOCK(FUNCNAME, LIBNAME)				\
	DECLARE_VARIABLE_STANDARD;					\
	info_t info;							\
	INIT_INFO;							\
	int32_t __attribute__((unused)) vAPI_ID = API_ID_ ## FUNCNAME;	\
	GET_REAL_FUNC(FUNCNAME, LIBNAME);				\
	PRE_PROBEBLOCK()

#endif // __DA_SOCKET_H__
