/*
 *  DA probe
 *
 * Copyright 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
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

#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "daprobe.h"
#include "probeinfo.h"
#include "dahelper.h"
#include "da_sync.h"

#include "binproto.h"
#include "real_functions.h"

static enum DaOptions _sopt = OPT_THREAD;

int pthread_mutex_init(pthread_mutex_t *mutex,
		const pthread_mutexattr_t *attr) {
	static int (*pthread_mutex_initp)(pthread_mutex_t *mutex,
			const pthread_mutexattr_t *attr);

	BEFORE_ORIGINAL_SYNC(pthread_mutex_init, LIBPTHREAD);

	ret = pthread_mutex_initp(mutex, attr);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutex_init,
				 'd', ret, mutex, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "pp",
				 voidp_to_uint64(mutex), voidp_to_uint64(attr));

	return ret;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
	static int (*pthread_mutex_destroyp)(pthread_mutex_t *mutex);

	BEFORE_ORIGINAL_SYNC(pthread_mutex_destroy, LIBPTHREAD);

	ret = pthread_mutex_destroyp(mutex);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutex_destroy,
				 'd', ret, mutex, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "p", voidp_to_uint64(mutex));

	return ret;
}

int real_pthread_mutex_lock(pthread_mutex_t *mutex) {
	static int (*pthread_mutex_lockp)(pthread_mutex_t *mutex);

	GET_REAL_FUNC(pthread_mutex_lock, LIBPTHREAD);

	return pthread_mutex_lockp(mutex);
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
	static int (*pthread_mutex_lockp)(pthread_mutex_t *mutex);

	DECLARE_VARIABLE_STANDARD;
	GET_REAL_FUNC(pthread_mutex_lock, LIBPTHREAD);

	PRE_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_SYNC,
			  API_ID_pthread_mutex_lock,
			  "p", voidp_to_uint64(mutex));
	PACK_COMMON_END('d', 0, 0, blockresult);
	PACK_SYNC(mutex, SYNC_PTHREAD_MUTEX, SYNC_API_ACQUIRE_WAIT_START);
	FLUSH_LOCAL_BUF();

	PRE_PROBEBLOCK_END();

	ret = pthread_mutex_lockp(mutex);

	// send WAIT_END log
	newerrno = errno;
	if(postBlockBegin(blockresult)) {
		setProbePoint(&probeInfo);

		PREPARE_LOCAL_BUF();
		PACK_COMMON_BEGIN(MSG_PROBE_SYNC,
				  API_ID_pthread_mutex_lock,
				  "p", voidp_to_uint64(mutex));
		PACK_COMMON_END('d', ret, errno, blockresult);
		PACK_SYNC(mutex, SYNC_PTHREAD_MUTEX, SYNC_API_ACQUIRE_WAIT_END);
		FLUSH_LOCAL_BUF();

		postBlockEnd();
	}
        errno = (newerrno != 0) ? newerrno : olderrno;

	return ret;
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex,
		const struct timespec *abs_timeout) {
	static int (*pthread_mutex_timedlockp)(pthread_mutex_t *mutex,
			const struct timespec *abs_timeout);

	DECLARE_VARIABLE_STANDARD;
	GET_REAL_FUNC(pthread_mutex_timedlock, LIBPTHREAD);

	PRE_PROBEBLOCK_BEGIN();

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_SYNC,
			  API_ID_pthread_mutex_timedlock,
			  "pp", voidp_to_uint64(mutex),
			  voidp_to_uint64(abs_timeout));
	PACK_COMMON_END('d', 0, 0, blockresult);
	PACK_SYNC(mutex, SYNC_PTHREAD_MUTEX, SYNC_API_ACQUIRE_WAIT_START);
	FLUSH_LOCAL_BUF();

	PRE_PROBEBLOCK_END();

	ret = pthread_mutex_timedlockp(mutex, abs_timeout);

	// send WAIT_END log
	newerrno = errno;
	if(postBlockBegin(blockresult)) {
		setProbePoint(&probeInfo);

		PREPARE_LOCAL_BUF();
		PACK_COMMON_BEGIN(MSG_PROBE_SYNC,
				  API_ID_pthread_mutex_timedlock,
				  "pp", voidp_to_uint64(mutex),
				  voidp_to_uint64(abs_timeout));
		PACK_COMMON_END('d', ret, errno, blockresult);
		PACK_SYNC(mutex, SYNC_PTHREAD_MUTEX, SYNC_API_ACQUIRE_WAIT_END);
		FLUSH_LOCAL_BUF();

		postBlockEnd();
	}

    errno = (newerrno != 0) ? newerrno : olderrno;

	return ret;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
	static int (*pthread_mutex_trylockp)(pthread_mutex_t *mutex);

	BEFORE_ORIGINAL_SYNC(pthread_mutex_trylock, LIBPTHREAD);

	ret = pthread_mutex_trylockp(mutex);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutex_trylock,
				 'd', ret, mutex, SYNC_PTHREAD_MUTEX,
				 SYNC_API_TRY_ACQUIRE, "p",
				 voidp_to_uint64(mutex));

	return ret;
}

int real_pthread_mutex_unlock(pthread_mutex_t *mutex) {
	static int (*pthread_mutex_unlockp)(pthread_mutex_t *mutex);

	GET_REAL_FUNC(pthread_mutex_unlock, LIBPTHREAD);

	return pthread_mutex_unlockp(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	static int (*pthread_mutex_unlockp)(pthread_mutex_t *mutex);

	BEFORE_ORIGINAL_SYNC(pthread_mutex_unlock, LIBPTHREAD);

	ret = pthread_mutex_unlockp(mutex);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutex_unlock,
				 'd', ret, mutex, SYNC_PTHREAD_MUTEX,
				 SYNC_API_RELEASE, "p",
				 voidp_to_uint64(mutex));

	return ret;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
	static int (*pthread_mutexattr_initp)(pthread_mutexattr_t *attr);

	BEFORE_ORIGINAL_SYNC(pthread_mutexattr_init, LIBPTHREAD);

	ret = pthread_mutexattr_initp(attr);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutexattr_init,
				 'd', ret, 0, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "p",
				 voidp_to_uint64(attr));

	return ret;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
	static int (*pthread_mutexattr_destroyp)(pthread_mutexattr_t *attr);

	BEFORE_ORIGINAL_SYNC(pthread_mutexattr_destroy, LIBPTHREAD);

	ret = pthread_mutexattr_destroyp(attr);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutexattr_destroy,
				 'd', ret, 0, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "p",
				 voidp_to_uint64(attr));

	return ret;
}

int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr,
		int *prioceiling) {
	static int (*pthread_mutexattr_getprioceilingp)(
			const pthread_mutexattr_t *attr, int *prioceiling);

	BEFORE_ORIGINAL_SYNC(pthread_mutexattr_getprioceiling, LIBPTHREAD);

	ret = pthread_mutexattr_getprioceilingp(attr, prioceiling);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutexattr_getprioceiling,
				 'd', ret, 0, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "pp",
				 voidp_to_uint64(attr),
				 voidp_to_uint64(prioceiling));

	return ret;
}

int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr,
		int prioceiling) {
	static int (*pthread_mutexattr_setprioceilingp)(
			pthread_mutexattr_t *attr, int prioceiling);

	BEFORE_ORIGINAL_SYNC(pthread_mutexattr_setprioceiling, LIBPTHREAD);

	ret = pthread_mutexattr_setprioceilingp(attr, prioceiling);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutexattr_setprioceiling,
				 'd', ret, 0, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "pd", voidp_to_uint64(attr),
				 prioceiling);

	return ret;
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr,
		int *protocol) {
	static int (*pthread_mutexattr_getprotocolp)(
			const pthread_mutexattr_t *attr, int *protocol);

	BEFORE_ORIGINAL_SYNC(pthread_mutexattr_getprotocol, LIBPTHREAD);

	ret = pthread_mutexattr_getprotocolp(attr, protocol);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutexattr_getprotocol,
				 'd', ret, 0, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "pp",
				 voidp_to_uint64(attr),
				 voidp_to_uint64(protocol));

	return ret;
}

int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr,
		int protocol) {
	static int (*pthread_mutexattr_setprotocolp)(
			pthread_mutexattr_t *attr, int protocol);

	BEFORE_ORIGINAL_SYNC(pthread_mutexattr_setprotocol, LIBPTHREAD);

	ret = pthread_mutexattr_setprotocolp(attr, protocol);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutexattr_setprotocol,
				 'd', ret, 0, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "pd", voidp_to_uint64(attr),
				 protocol);

	return ret;
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr,
		int *pshared) {
	static int (*pthread_mutexattr_getpsharedp)(
			const pthread_mutexattr_t *attr, int *pshared);

	BEFORE_ORIGINAL_SYNC(pthread_mutexattr_getpshared, LIBPTHREAD);

	ret = pthread_mutexattr_getpsharedp(attr, pshared);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutexattr_getpshared,
				 'd', ret, 0, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "pp",
				 voidp_to_uint64(attr),
				 voidp_to_uint64(pshared));

	return ret;
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr,
		int pshared) {
	static int (*pthread_mutexattr_setpsharedp)(
			pthread_mutexattr_t *attr, int pshared);

	BEFORE_ORIGINAL_SYNC(pthread_mutexattr_setpshared, LIBPTHREAD);

	ret = pthread_mutexattr_setpsharedp(attr, pshared);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutexattr_setpshared,
				 'd', ret, 0, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "pd", voidp_to_uint64(attr),
				 pshared);

	return ret;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type) {
	static int (*pthread_mutexattr_gettypep)(
			const pthread_mutexattr_t *attr, int *type);

	BEFORE_ORIGINAL_SYNC(pthread_mutexattr_gettype, LIBPTHREAD);

	ret = pthread_mutexattr_gettypep(attr, type);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutexattr_gettype,
				 'd', ret, 0, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "pp",
				 voidp_to_uint64(attr),
				 voidp_to_uint64(type));

	return ret;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type) {
	static int (*pthread_mutexattr_settypep)(
			pthread_mutexattr_t *attr, int type);

	BEFORE_ORIGINAL_SYNC(pthread_mutexattr_settype, LIBPTHREAD);

	ret = pthread_mutexattr_settypep(attr, type);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_mutexattr_settype,
				 'd', ret, 0, SYNC_PTHREAD_MUTEX,
				 SYNC_API_OTHER, "pd",
				 voidp_to_uint64(attr),
				 type);

	return ret;
}

/*
int pthread_mutex_getprioceiling(const pthread_mutex_t *mutex,
		int *prioceiling);
int pthread_mutex_setprioceiling(pthread_mutex_t *mutex,
		int prioceiling, int *old_ceiling);
*/

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
	static int (*pthread_cond_initp)(pthread_cond_t *cond,
			const pthread_condattr_t *attr);

	BEFORE_ORIGINAL_SYNC(pthread_cond_init, LIBPTHREAD);

	ret = pthread_cond_initp(cond, attr);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_cond_init,
				 'd', ret, cond, SYNC_PTHREAD_COND_VARIABLE,
				 SYNC_API_OTHER, "pp",
				 voidp_to_uint64(cond),
				 voidp_to_uint64(attr));

	return ret;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
	static int (*pthread_cond_destroyp)(pthread_cond_t *cond);

	BEFORE_ORIGINAL_SYNC(pthread_cond_destroy, LIBPTHREAD);

	ret = pthread_cond_destroyp(cond);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_cond_destroy,
				 'd', ret, cond, SYNC_PTHREAD_COND_VARIABLE,
				 SYNC_API_OTHER, "p",
				 voidp_to_uint64(cond));

	return ret;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
	static int (*pthread_cond_waitp)(pthread_cond_t *cond,
			pthread_mutex_t *mutex);

	DECLARE_VARIABLE_STANDARD;
	GET_REAL_FUNC(pthread_cond_wait, LIBPTHREAD);

	PRE_PROBEBLOCK_BEGIN();
	// send WAIT_START log

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_SYNC,
			  API_ID_pthread_cond_wait,
			  "pp",
			  voidp_to_uint64(cond),
			  voidp_to_uint64(mutex));
	PACK_COMMON_END('d', 0, 0, blockresult);
	PACK_SYNC(cond, SYNC_PTHREAD_COND_VARIABLE, SYNC_API_COND_WAIT_START);
	FLUSH_LOCAL_BUF();

	PRE_PROBEBLOCK_END();

	ret = pthread_cond_waitp(cond, mutex);

	// send WAIT_END log
	newerrno = errno;
	if(postBlockBegin(blockresult)) {
		setProbePoint(&probeInfo);

		PREPARE_LOCAL_BUF();
		PACK_COMMON_BEGIN(MSG_PROBE_SYNC,
				  API_ID_pthread_cond_wait,
				  "pp",
				  voidp_to_uint64(cond),
				  voidp_to_uint64(mutex));
		PACK_COMMON_END('d', ret, errno, blockresult);
		PACK_SYNC(cond, SYNC_PTHREAD_COND_VARIABLE, SYNC_API_COND_WAIT_END);
		FLUSH_LOCAL_BUF();

		postBlockEnd();
	}
    errno = (newerrno != 0) ? newerrno : olderrno;

	return ret;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		const struct timespec *abstime) {
	static int (*pthread_cond_timedwaitp)(pthread_cond_t *cond,
				pthread_mutex_t *mutex, const struct timespec *abstime);

	DECLARE_VARIABLE_STANDARD;
	GET_REAL_FUNC(pthread_cond_timedwait, LIBPTHREAD);

	PRE_PROBEBLOCK_BEGIN();
	// send WAIT_START log

	PREPARE_LOCAL_BUF();
	PACK_COMMON_BEGIN(MSG_PROBE_SYNC,
			  API_ID_pthread_cond_timedwait,
			  "ppp",
			  voidp_to_uint64(cond),
			  voidp_to_uint64(mutex),
			  voidp_to_uint64(abstime));
	PACK_COMMON_END('d', 0, 0, blockresult);
	PACK_SYNC(cond, SYNC_PTHREAD_COND_VARIABLE, SYNC_API_COND_WAIT_START);
	FLUSH_LOCAL_BUF();

	PRE_PROBEBLOCK_END();

	ret = pthread_cond_timedwaitp(cond, mutex, abstime);

	// send WAIT_END log
	newerrno = errno;
	if(postBlockBegin(blockresult)) {
		setProbePoint(&probeInfo);

		PREPARE_LOCAL_BUF();
		PACK_COMMON_BEGIN(MSG_PROBE_SYNC,
				  API_ID_pthread_cond_timedwait,
				  "ppp",
				  voidp_to_uint64(cond),
				  voidp_to_uint64(mutex),
				  voidp_to_uint64(abstime));
		PACK_COMMON_END('d', ret, errno, blockresult);
		PACK_SYNC(cond, SYNC_PTHREAD_COND_VARIABLE, SYNC_API_COND_WAIT_END);
		FLUSH_LOCAL_BUF();

		postBlockEnd();
	}

    errno = (newerrno != 0) ? newerrno : olderrno;

	return ret;
}

int pthread_cond_signal(pthread_cond_t *cond) {
	static int (*pthread_cond_signalp)(pthread_cond_t *cond);

	BEFORE_ORIGINAL_SYNC(pthread_cond_signal, LIBPTHREAD);

	ret = pthread_cond_signalp(cond);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_cond_signal,
				 'd', ret, cond, SYNC_PTHREAD_COND_VARIABLE,
				 SYNC_API_NOTIFY, "p", voidp_to_uint64(cond));

	return ret;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
	static int (*pthread_cond_broadcastp)(pthread_cond_t *cond);

	BEFORE_ORIGINAL_SYNC(pthread_cond_broadcast, LIBPTHREAD);

	ret = pthread_cond_broadcastp(cond);

	AFTER_PACK_ORIGINAL_SYNC(API_ID_pthread_cond_broadcast,
				 'd', ret, cond, SYNC_PTHREAD_COND_VARIABLE,
				 SYNC_API_NOTIFY_ALL, "p", voidp_to_uint64(cond));

	return ret;
}
