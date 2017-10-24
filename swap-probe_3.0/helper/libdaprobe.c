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
 * Nikita Kalyazin <n.kalyazin@samsung.com>
 * Vitaliy Cherepanov <v.cherepanov@samsung.com>
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

#include <stdio.h>			// for sprintf
#include <stdlib.h>			// for getenv
#include <string.h>			// for strstr
#include <stdbool.h>		// for bool
#include <stdint.h>			// fot uint32_t,uint64_t
#include <stdarg.h>			// for va_list, va_arg(__appendTypeLog)
#include <execinfo.h>		// for backtrace, backtrace_symbols
#include <unistd.h>			// for write, alarm function, syscall
#include <pthread.h>		// for pthread_mutex_lock
#include <signal.h>
#include <stdint.h>

#include <sys/syscall.h>	// for syscall
#include <sys/time.h>		// for gettimeofday
#include <sys/socket.h>		// for socket, connect
#include <sys/un.h>			// for sockaddr_un
#include <sys/timerfd.h>	// for timerfd

#include "probeinfo.h"
#include "dautil.h"
#include "dahelper.h"
#include "dacollection.h"
#include "da_sync.h"
#include "daprobe.h"

#include "binproto.h"
#include "daforkexec.h"
#include "damaps.h"
#include "dastdout.h"
#include "common_probe_init.h"
#include "real_functions.h"

#define APP_INSTALL_PATH		"/opt/apps"
#define TISEN_APP_POSTFIX			".exe"
#define UDS_NAME				"/tmp/da.socket"
#define TIMERFD_INTERVAL		100000000		// 0.1 sec

__thread int		gProbeBlockCount = 0;
__thread int		gProbeDepth = 0;
pid_t gPid = -1;
__thread pid_t		gTid = -1;

int			g_timerfd = 0;
long		g_total_alloc_size = 0;
pthread_t	g_recvthread_id;

int log_fd = 0;

int getExecutableMappingAddress();

bool printLog(log_t* log, int msgType);

/******************************************************************************
 * internal functions
   (this means that these functions do not need to set enter/exit flag)
 ******************************************************************************/

// runtime configure the probe option
static void _configure(char* configstr)
{
	gTraceInfo.optionflag = atoll(configstr);

	if isOptionEnabled(OPT_SNAPSHOT)
		SCREENSHOT_SET();
	else
		SCREENSHOT_UNSET();
	PRINTMSG("configure in probe : %s, %llx\n", configstr, gTraceInfo.optionflag);
}

void application_exit()
{
	pid_t gpid;
	FILE *f = NULL;
	char buf[MAX_PATH_LENGTH];
	const char manager_name[] = "da_manager";

	/* TODO think of another way for correct app termination */

	gpid = getpgrp();
	/* check for parent */
	snprintf(buf, sizeof(buf), "/proc/%d/cmdline", gpid);
	f = fopen(buf, "r");
	if (f != NULL) {
		fscanf(f, "%s", buf);
		fclose(f);
		if (strlen(buf) == strlen(manager_name) &&
		    strncmp(buf, manager_name, sizeof(manager_name)) == 0) {
			/* Luke, I am your father
			 * da_manager is our parent
			 * looks like we are common applicaton
			 */
			PRINTMSG("App termination: EXIT(0)");
			exit(0);
		}
	}

	/* da_manager is not our father
	 * we are native app or already launched.
	 * Will be troubles up there if we are common already launched app!!!
	 * Kill yourself!!
	 */
	PRINTMSG("App termination: kill all process group");
	killpg(gpid, SIGKILL);
}

// create socket to daemon and connect
#define MSG_CONFIG_RECV 0x01
#define MSG_MAPS_INST_LIST_RECV 0x02
static int createSocket(void)
{
	char strerr_buf[MAX_PATH_LENGTH];
	ssize_t recvlen;
	int clientLen, ret = 0;
	struct sockaddr_un clientAddr;
	log_t log;

	gTraceInfo.socket.daemonSock =
	  socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (gTraceInfo.socket.daemonSock != -1)	{
		memset(&clientAddr, '\0', sizeof(clientAddr));
		clientAddr.sun_family = AF_UNIX;
		snprintf(clientAddr.sun_path, sizeof(UDS_NAME), "%s", UDS_NAME);

		clientLen = sizeof(clientAddr);
		if (connect(gTraceInfo.socket.daemonSock,
			    (struct sockaddr *)&clientAddr, clientLen) >= 0)
		{
			char buf[64];
			int recved = 0;

			/* send pid and ppid to manager */
			snprintf(buf, sizeof(buf), "%d %d", getpid(), getppid());
			print_log_str(MSG_PID, buf);

			/* we need recv this messages right now! */
			while (((recved & MSG_CONFIG_RECV) == 0) ||
			       ((recved & MSG_MAPS_INST_LIST_RECV) == 0))
			{
				PRINTMSG("wait incoming message %d\n",
					 gTraceInfo.socket.daemonSock);
				/* recv header */
				recvlen = recv(gTraceInfo.socket.daemonSock, &log,
					       sizeof(log.type) + sizeof(log.length),
					       MSG_WAITALL);

				fprintf(stderr, "recv %d\n", recvlen);
				if (recvlen > 0) {/* recv succeed */
					char *data_buf = NULL;

					data_buf = real_malloc(log.length);

					if (data_buf == NULL) {
						PRINTERR("cannot allocate buf to recv msg");
						break;
					}

					recvlen = recv(gTraceInfo.socket.daemonSock, data_buf,
						       log.length, MSG_WAITALL);

					if (recvlen != log.length) {
						PRINTERR("Can not get data from daemon sock\n");
						goto free_data_buf;
					}

					if (log.type == MSG_CONFIG) {
						PRINTMSG("MSG_CONFIG");
						_configure(data_buf);
						recved |= MSG_CONFIG_RECV;
					} else if(log.type ==  MSG_MAPS_INST_LIST) {
						PRINTMSG("MSG_MAPS_INST_LIST <%u>", *((uint32_t *)data_buf));
						set_map_inst_list(&data_buf, *((uint32_t *)data_buf));
						recved |= MSG_MAPS_INST_LIST_RECV;
					} else {
						// unexpected case
						PRINTERR("unknown message! %d", log.type);
					}

free_data_buf:
					if (data_buf != NULL)
						free(data_buf);

				} else if (recvlen < 0) {
					close(gTraceInfo.socket.daemonSock);
					gTraceInfo.socket.daemonSock = -1;
					PRINTERR("recv failed with error(%d)\n",
						 recvlen);
					ret = -1;
					application_exit();
					break;
				} else {
					/* closed by other peer */
					close(gTraceInfo.socket.daemonSock);
					gTraceInfo.socket.daemonSock = -1;
					PRINTERR("closed by other peer\n");
					ret = -1;
					application_exit();
					break;
				}

			}

			PRINTMSG("createSocket connect() success\n");
		} else {
			close(gTraceInfo.socket.daemonSock);
			gTraceInfo.socket.daemonSock = -1;
			strerror_r(errno, strerr_buf, sizeof(strerr_buf));
			PRINTERR("cannot connect to da_manager. err <%s>\n",
				 strerr_buf);
			ret = -1;
		}
	} else {
		strerror_r(errno, strerr_buf, sizeof(strerr_buf));
		PRINTERR("cannot create socket. err <%s>\n", strerr_buf);
		ret = -1;
	}

	PRINTMSG("socket create done with result = %d", ret);
	return ret;
}


// parse backtrace string and find out the caller of probed api function
// return 0 if caller is user binary, otherwise return 1
static int determineCaller(char* tracestring)
{
	char *substr;

	// determine whether saveptr (caller binary name) is user binary or not
	substr = strstr(tracestring, APP_INSTALL_PATH);

	if(substr == NULL)	// not user binary
	{
		return 1;
	}
	else				// user binary
	{
#ifdef TISENAPP
		substr = strstr(tracestring, TISEN_APP_POSTFIX);
		if(substr == NULL)
			return 1;
#endif
		return 0;
	}
}

void reset_pid_tid()
{
	gPid = -1;
	gTid = -1;
}

// return current process id
pid_t _getpid()
{
	if (gPid == -1)
		gPid = getpid();
	return gPid;
}

// return current thread id
pid_t _gettid()
{
	if(gTid == -1)
		gTid = syscall(__NR_gettid);	// syscall is very expensive
	return gTid;
}

static void *recvThread(void __unused * data)
{
	fd_set readfds, workfds;
	int maxfd = 0, rc;
	uint64_t xtime;
	uint32_t tmp;
	ssize_t recvlen;
	log_t log;
	char *data_buf = NULL;
	sigset_t profsigmask;

	if(gTraceInfo.socket.daemonSock == -1)
		return NULL;

	probeBlockStart();

	sigemptyset(&profsigmask);
	sigaddset(&profsigmask, SIGPROF);
	pthread_sigmask(SIG_BLOCK, &profsigmask, NULL);

	FD_ZERO(&readfds);
	if(g_timerfd > 0)
	{
		maxfd = g_timerfd;
		FD_SET(g_timerfd, &readfds);
	}
	if(maxfd < gTraceInfo.socket.daemonSock)
		maxfd = gTraceInfo.socket.daemonSock;
	FD_SET(gTraceInfo.socket.daemonSock, &readfds);

	while(1)
	{
		workfds = readfds;
		rc = select(maxfd + 1, &workfds, NULL, NULL, NULL);
		if(rc < 0)
		{
			continue;
		}

		if(g_timerfd > 0 && FD_ISSET(g_timerfd, &workfds))
		{
			recvlen = read(g_timerfd, &xtime, sizeof(xtime));
			if(recvlen > 0)
			{
				log.length = snprintf(log.data, sizeof(log.data), "%ld", g_total_alloc_size) + 1;
				printLog(&log, MSG_ALLOC);
			}
			else
			{
				// read failed
			}
			continue;
		}
		else if(FD_ISSET(gTraceInfo.socket.daemonSock, &workfds))
		{
			recvlen = recv(gTraceInfo.socket.daemonSock, &log,
					sizeof(log.type) + sizeof(log.length), MSG_WAITALL);

			if(recvlen > 0)	// recv succeed
			{

				if(log.length > 0) {
					data_buf = real_malloc(log.length);
					if (data_buf == NULL) {
						PRINTERR("cannot allocate buf to recv msg");
						break;
					}
					recvlen = recv(gTraceInfo.socket.daemonSock, data_buf,
						log.length, MSG_WAITALL);
					if (recvlen != log.length) {
						PRINTERR("Can not recv data from\n");
						goto free_data_buf;
					}
				}

				if (log.type == MSG_CAPTURE_SCREEN) {
					captureScreen();
				} else if (log.type == MSG_CONFIG) {
					_configure(data_buf);
				} else if(log.type == MSG_STOP) {
					PRINTMSG("MSG_STOP");
					if (data_buf) {
						free(data_buf);
						data_buf = NULL;
					}
					application_exit();
					break;
				} else if(log.type == MSG_MAPS_INST_LIST) {
					if(log.length > 0) {
						tmp = *((uint32_t *)data_buf);
						PRINTMSG("MSG_MAPS_INST_LIST <%u>", tmp);
						set_map_inst_list(&data_buf, tmp);
						continue;
					} else {
						PRINTERR("WRONG MSG_MAPS_INST_LIST");
					}
				} else {
					PRINTERR("recv unknown message. id = (%d)", log.type);
				}

free_data_buf:
				if (data_buf) {
					free(data_buf);
					data_buf = NULL;
				}

			}
			else if(recvlen == 0)	// closed by other peer
			{
				close(gTraceInfo.socket.daemonSock);
				gTraceInfo.socket.daemonSock = -1;
				break;
			}
			else	// recv error
			{
				PRINTERR("recv failed in recv thread with error(%d)", recvlen);
				continue;
			}
		}
		else	// unknown case
		{
			continue;
		}
	}

	/* release buffer */
	if (data_buf)
		free(data_buf);

	probeBlockEnd();
	return NULL;
}

/*****************************************************************************
 * initialize / finalize function
 *****************************************************************************/
static int init_timerfd(void)
{
	const struct timespec interval = {
		.tv_sec = 0,
		.tv_nsec = TIMERFD_INTERVAL
	};
	const struct itimerspec ctime = {
		.it_interval = interval,
		.it_value = interval
	};

	int timer = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC);

	if (timer == -1) {
		PRINTMSG("failed to create timerdf\n");
		return -1;
	}

	if (timerfd_settime(timer, 0, &ctime, NULL) != 0) {
		PRINTMSG("failed to set timerfd\n");
		close(timer);
		return -1;
	}

	return timer;
}

static uint64_t get_app_start_time(void)
{
	enum {nsecs_in_sec = 1000 * 1000};
	struct timeval time;

	gettimeofday(&time, NULL);
	return nsecs_in_sec * (uint64_t) time.tv_sec + time.tv_usec;
}

static int create_recv_thread()
{
	int err = pthread_create(&g_recvthread_id, NULL, recvThread, NULL);

	if (err)
		PRINTMSG("failed to crate recv thread\n");

	return err;
}

int _init_(void)
{
	int res = 0;

	probeBlockStart();

	/* redirect stderr and stdout.*/
	/* if there is no std - print call will crash app */
	__redirect_std();

	init_exec_fork();
	initialize_hash_table();

	initialize_screencapture();

	initialize_event();

	getExecutableMappingAddress();

	gTraceInfo.app.startTime = get_app_start_time();

	// create socket for communication with da_daemon
	if (createSocket() == 0) {
		g_timerfd = init_timerfd();
		create_recv_thread();
		update_heap_memory_size(true, 0);
	}


	PRINTMSG("dynamic analyzer probe helper so loading... pid[%d]\n",
		 getpid());

	gTraceInfo.init_complete = 1;
	if (maps_make() != 0) {
		PRINTERR("maps make failed\n");
		res = -1;
		goto unlock_exit;
	}

unlock_exit:
	probeBlockEnd();

	return res;
}

void __attribute__((constructor)) _init_probe()
{

	if (_init_real_functions()) {
		PRINTERR("cannot init real functions\n");
		exit(1);
	}
	rtdl_next_set_once(real_malloc, "malloc");

	 /* init maps */
	if (maps_init()!=0){
		perror("cannot init readers semaphores\n");
		exit(0);
	};

	/* init library */
	_init_();

	/* init gl functions */
	__init_gl_functions__();

	PRINTMSG("<-lib construnctor");
}

void _uninit_(void)
{
	int i;
	probeBlockStart();

	gTraceInfo.init_complete = -1;
	PRINTMSG("dynamic analyzer probe helper so unloading... pid[%d]\n",
		 getpid());

	remove_all_glist();

	// close timerfd
	if(g_timerfd > 0)
		close(g_timerfd);

	// close socket
	if(gTraceInfo.socket.daemonSock != -1)
	{
		print_log_str(MSG_TERMINATE, NULL);
		close(gTraceInfo.socket.daemonSock);
		gTraceInfo.socket.daemonSock = -1;
	}

	finalize_event();

	finalize_screencapture();

	finalize_hash_table();

	for(i = 0; i < NUM_ORIGINAL_LIBRARY; i++)
	{
		if(lib_handle[i] != NULL)
		{
			dlclose(lib_handle[i]);
		}
	}

	probeBlockEnd();
}

void __attribute__((destructor)) _fini_probe()
{
	PRINTMSG("->lib destructor. pid[%d]\n", getpid());
	_uninit_();
}


/**************************************************************************
 * Helper APIs
 **************************************************************************/

/************************************************************************
 * manipulate and print log functions
 ************************************************************************/
const char *msg_code_to_srt(enum MessageType type)
{
	switch (type) {
		case MSG_MSG:
			return "[INF]";
		case MSG_ERROR:
			return "[ERR]";
		case MSG_WARNING:
			return "[WRN]";
		default:
			return "[???]";
	}
}

bool printLog(log_t *log, int msgType)
{
	ssize_t res, len;
	if(unlikely(gTraceInfo.socket.daemonSock == -1))
		return false;

	if(unlikely(log == NULL))
		return false;

	probeBlockStart();
	log->type = msgType;
	len = sizeof(log->type) + sizeof(log->length) + log->length;

	real_pthread_mutex_lock(&(gTraceInfo.socket.sockMutex));
	res = send(gTraceInfo.socket.daemonSock, log, len, 0);
	real_pthread_mutex_unlock(&(gTraceInfo.socket.sockMutex));
	probeBlockEnd();

	return (res == len);
}

/*
 * Debug function for ld preloaded library
 * msgType - message type (MSG_TERMINATE, MSG_PID)
 * str     - string send to host. may be NULL
 * return  - true on success. false on fail.
 */

bool print_log_str(int msgType, char *str)
{
	log_t log;
	ssize_t res, len;

	/* Check connection status */
	if(unlikely(gTraceInfo.socket.daemonSock == -1))
		return false;

	probeBlockStart();

	/* fill message */
	log.type = msgType;
	if (str != NULL)
		log.length = snprintf(log.data, sizeof(log.data), str);
	else
		log.length = 0;

	/* calculate message length */
	len = sizeof(log.type) + sizeof(log.length) + log.length;

	/* lock socket and send */
	real_pthread_mutex_lock(&(gTraceInfo.socket.sockMutex));
	res = send(gTraceInfo.socket.daemonSock, &log, len, 0);
	real_pthread_mutex_unlock(&(gTraceInfo.socket.sockMutex));
	probeBlockEnd();

	return (res == len);
}

/*
 * Additional debug function for ld preloaded library can be used in defines
 * msgType   - message type (MSG_TERMINATE, MSG_PID)
 * func_name - call function name
 * line      - call line number
 * ...       - format and arguments like for sprintf function
 * return    - true on success. false on fail.
 */
bool print_log_fmt(int msgType, const char *func_name, int line, ...)
{
	ssize_t res = 0, len = 0;
	char *fmt, *p;
	int n;
	log_t log;
	va_list ap;

	/* Check connection status */

	probeBlockStart();

	/* fill message */
	log.type = msgType;

	p = log.data;
	n = snprintf(p, sizeof(log.data), "[%05d:%05d]%s:%d)", _getpid(), _gettid(),  func_name, line);
	p += n;

	/* extract params */
	va_start(ap, line);
	fmt = va_arg(ap, char *);
	if (fmt != NULL) {
		if (strchr(fmt, '%') == NULL)
			n += snprintf(p, sizeof(log.data) - n, "%s", fmt);
		else
			n += vsnprintf(p, sizeof(log.data) - n, fmt, ap);
	}
	va_end(ap);

	/* check printed symbols count */
	if (n > -1 && n < (int)sizeof(log.data)) {
		log.length = n;
	} else {
		fprintf(stderr, "Log pack error\n");
		log.length = 0;
	}

	/* calculate message length */
	len = sizeof(log.type) + sizeof(log.length) + log.length;

	/* lock socket and send */
	real_pthread_mutex_lock(&(gTraceInfo.socket.sockMutex));

	if(unlikely(gTraceInfo.socket.daemonSock != -1)) {
		res = send(gTraceInfo.socket.daemonSock, &log, len, MSG_DONTWAIT);
	} else {
		/* if socket is not connected, out to stderr */
		fprintf(stderr, "%s %s\n", msg_code_to_srt(msgType), log.data);
		fflush(stderr);
	}

	real_pthread_mutex_unlock(&(gTraceInfo.socket.sockMutex));

	probeBlockEnd();

	/* return result */
	return (res == len);
}

// get backtrace string
// return stack depth if succeed, otherwise return 0
// parameter 'log' cannot be null
/* TODO remove unused code (getBacktraceString)*/
int getBacktraceString(log_t* log, int bufsize)
{
	void* array[MAX_STACK_DEPTH];
	char** strings = NULL;
	size_t i, size;
	int initsize;
	int curlen;
	int stringlen;

	if(log == NULL)
		return 0;

	probeBlockStart();

	initsize = log->length;
	curlen = initsize;
	log->data[curlen] = '\0';	// is this necessary ?
	size = backtrace(array, MAX_STACK_DEPTH);
	if(likely(size > TRIM_STACK_DEPTH))
	{
		strings = BACKTRACE_SYMBOLS(array + TRIM_STACK_DEPTH, size - TRIM_STACK_DEPTH);

		if(likely(strings != NULL))
		{
			for(i = TRIM_STACK_DEPTH; i < size; i++)
			{
				stringlen = strlen(strings[i - TRIM_STACK_DEPTH]) + 14;
				if(curlen + stringlen >= bufsize + initsize)
					break;

				curlen += snprintf(log->data + curlen, bufsize - curlen, "%010u`,%s`,", (unsigned int)(array[i]), strings[i - TRIM_STACK_DEPTH]);
			}
			curlen -= 2;
			log->data[curlen] = '\0';
			log->length = curlen;
			free(strings);
		}
		else	// failed to get backtrace symbols
		{
			// just print trace address
			for(i = TRIM_STACK_DEPTH; i < size; i++)
			{
				stringlen = 23;
				if(curlen + stringlen >= bufsize + initsize)
					break;

				curlen += snprintf(log->data + curlen, bufsize - curlen, "%010u`,(unknown)`,", (unsigned int)(array[i]));
			}
			curlen -= 2;
			log->data[curlen] = '\0';
			log->length = curlen;
		}

		probeBlockEnd();
		return (int)(size - TRIM_STACK_DEPTH);
	}
	else
	{
		probeBlockEnd();
		return 0;
	}
}

/*************************************************************************
 * probe block control functions
 *************************************************************************/
static inline bool isNoFiltOptionEnabled(enum DaOptions option)
{
	return
		((option == OPT_ALLOC)
		 && isOptionEnabled(OPT_ALLOC_ALWAYS))
		|| ((option == OPT_FILE)
		    && isOptionEnabled(OPT_FILE_ALWAYS))
		|| ((option == OPT_THREAD)
		    && isOptionEnabled(OPT_THREAD_ALWAYS))
		|| ((option == OPT_UI)
		    && isOptionEnabled(OPT_UI_ALWAYS))
		|| ((option == OPT_NETWORK)
		    && isOptionEnabled(OPT_NETWORK_ALWAYS))
		|| ((option == OPT_GLES)
		    && isOptionEnabled(OPT_GLES_ALWAYS));
}

static inline bool is_user_call(const void *caller)
{
	bool user = false;
	char **strings;

	if (gTraceInfo.exec_map.map_start != NULL) {
		if (caller >= gTraceInfo.exec_map.map_start &&
		    caller <= gTraceInfo.exec_map.map_end)
			user = true;
	} else {
		strings = BACKTRACE_SYMBOLS((void * const *)caller, 1);
		if (strings != NULL) {
			if (determineCaller(strings[0]) == 0)
				user = true;
			free(strings);
		}
	}

	return user;
}

int preBlockBegin(const void *caller, bool bFiltering, enum DaOptions option)
{
	bool opt_nofilt;

	if (gTraceInfo.socket.daemonSock == -1)
		return 0;

	if(gProbeBlockCount != 0 || gProbeDepth != 0)
		return 0;

	if(gTraceInfo.init_complete <= 0)
		return 0;

	opt_nofilt = isNoFiltOptionEnabled(option);

	/* Actually we are considering 3 variables here:
	    - regular option is enabled
	    - non-filtering (always) option is enabled
	    - per-probe filtering
	*/
	if (!isOptionEnabled(option) && !opt_nofilt)
		return 0;
	else if (opt_nofilt)
		bFiltering = false;

	probeBlockStart();

	if (maps_is_instrument_section_by_addr(caller) == 1) {
		probingStart();
		return 2; /* user call */
	} else {
		if (bFiltering) {
			probeBlockEnd();
			return 0; /* not probing */
		} else {
			probingStart();
			return 1; /* internal call */
		}
	}
}

int postBlockBegin(int preresult)
{
	if(preresult)
	{
		probeBlockStart();
	}

	return preresult;
}

void preBlockEnd()
{
	probeBlockEnd();
}

void postBlockEnd()
{
	probingEnd();
	probeBlockEnd();
}

/*************************************************************************
 * helper info getter functions
 *************************************************************************/
// return current time in 1/10000 sec unit
unsigned long getCurrentTime()
{
	struct timeval cTime;

	gettimeofday(&cTime, NULL);

	return (unsigned long)((cTime.tv_sec * 10000 + (cTime.tv_usec/100)));
}

unsigned int getCurrentEventIndex()
{
	return gTraceInfo.index.eventIndex;
}

uint64_t get_current_nsec(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}

/************************************************************************
 * probe functions
 ************************************************************************/
bool setProbePoint(probeInfo_t* iProbe)
{
	if(unlikely(iProbe == NULL))
	{
		return false;
	}

	probeBlockStart();

	// atomic operaion(gcc builtin) is more expensive then pthread_mutex
	real_pthread_mutex_lock(&(gTraceInfo.index.eventMutex));
	iProbe->eventIndex = gTraceInfo.index.eventIndex++;
	real_pthread_mutex_unlock(&(gTraceInfo.index.eventMutex));

	iProbe->currentTime = getCurrentTime();
	iProbe->pID = _getpid();
	iProbe->tID = _gettid();
	iProbe->callDepth = gProbeDepth;

	probeBlockEnd();
	return true;
}

// update heap memory size through socket
// return 0 if size is updated through socket
// return 1 if size is updated into global variable
int update_heap_memory_size(bool isAdd, size_t size)
{
	if(isAdd)
	{
		__sync_add_and_fetch(&g_total_alloc_size, (long)size);
	}
	else
	{
		__sync_sub_and_fetch(&g_total_alloc_size, (long)size);
	}

	return 0;
}

/* rtdl_next */
void *rtdl_next(const char *symname)
{
	void *symbol;

	probeBlockStart();

	symbol = dlsym(RTLD_NEXT, symname);
	if (symbol == NULL || dlerror() != NULL) {
		PRINTERR("dlsym failed <%s>\n", symname);
		exit(41);
	}

	probeBlockEnd();

	return symbol;
}
