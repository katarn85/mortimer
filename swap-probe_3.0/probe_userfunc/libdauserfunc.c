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
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <execinfo.h>
#include <unistd.h>
#include <pthread.h>
#include "daprobe.h"
#include "probeinfo.h"
#include "dautil.h"
#include "dahelper.h"
#include "da_sync.h"

//#define USING_BACKTRACE

#define RETURN_ADDRESS(nr) \
	__builtin_extract_return_addr (__builtin_return_address(nr))

#if defined(__i386__)
#define GET_PC(ctx)	((void *) ctx.eip)
#elif defined (__arm__)
#define GET_PC(ctx)	((void *) (ctx)->uc_mcontext.arm_pc)
#endif

#define USERFUNCTION_MAX_DEPTH				32
#define PROFIL_LOG_SENDING_INTERVAL_USEC	100000 // 100 milliseconds
#define SAMPLE_MAX_IN_INTERVAL				50
#define	PROFIL_TRIM_STACK_DEPTH				2

#define IS_FULL_SAMPLE_ARRAY	((((sample_write_index + 1) % SAMPLE_MAX_IN_INTERVAL) == sample_read_index) ? 1 : 0)
#define IS_EMPTY_SAMPLE_ARRAY	((sample_write_index == sample_read_index) ? 1 : 0)

#define CUSTOM_CB_FUNC_NAME		"_chart_timerThread"

void *custom_cb_addr = (void*)-1;

int profil_option = 0;
volatile int profil_turned_on = 0;
volatile int profil_thread_on = 0;
u_long low_pc, high_pc;

typedef struct elapsed_time_t {
	struct timeval startTime;
	struct timeval endTime;
	struct timeval resultTime;
	void *self;
} elapsed_time;

__thread elapsed_time elapsed_time_array[USERFUNCTION_MAX_DEPTH];
__thread int elapsed_time_index;

pthread_t profil_log_thread;
pthread_mutex_t profil_log_mutex;

typedef struct sample_info_t {
	unsigned long time;
	void *pc;
	size_t bt_size;
	void *bt_array[MAX_STACK_DEPTH];
} sample_info;

int sample_seq = 0;
int sample_read_index = 0;
int sample_write_index = 0;
sample_info sample_info_array[SAMPLE_MAX_IN_INTERVAL];

int __profile_frequency();

/*
static unsigned long long getElapsedTime(struct timeval eTime)
{
	return ((unsigned long long) eTime.tv_sec) * 1000000 + eTime.tv_usec;
}
*/

static unsigned long getTime()
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	return (unsigned long)(ts.tv_sec * 10000 + (ts.tv_nsec/100000));
}

/* TODO refactor close equal code in ifdef/else section */
/* TODO remove code. it seems unused. */
#ifdef USING_BACKTRACE
int profil_backtrace_symbols(log_t *log, int bufsize, int index)
{
	char **strings = NULL;
	size_t i;
	int initsize;
	int curlen;
	int stringlen;

	if(log == NULL)
		return 0;

	initsize = log->length;
	curlen = initsize;
	log->data[curlen] = '\0';	// is this necessary ?
	if(likely(sample_info_array[index].bt_size > PROFIL_TRIM_STACK_DEPTH))
	{
		strings = BACKTRACE_SYMBOLS(sample_info_array[index].bt_array + PROFIL_TRIM_STACK_DEPTH,
				sample_info_array[index].bt_size - PROFIL_TRIM_STACK_DEPTH);

		if(likely(strings != NULL))
		{
			for(i = PROFIL_TRIM_STACK_DEPTH; i < sample_info_array[index].bt_size; i++)
			{
				stringlen = strlen(strings[i - PROFIL_TRIM_STACK_DEPTH]) + 14;
				if(curlen + stringlen >= bufsize + initsize)
					break;

				curlen += snprintf(log->data + curlen, bufsize - curlen, "%010u`,%s`,",
						(unsigned int)(sample_info_array[index].bt_array[i]),
						strings[i - PROFIL_TRIM_STACK_DEPTH]);
			}
			curlen -= 2;
			log->data[curlen] = '\0';
			log->length = curlen;
			free(strings);
		}
		else
		{
			for(i = PROFIL_TRIM_STACK_DEPTH; i < sample_info_array[index].bt_size; i++)
			{
				stringlen = 23;
				if(curlen + stringlen >= bufsize + initsize)
					break;

				curlen += snprintf(log->data + curlen, bufsize - curlen, "%010u`,(unknown)`,",
						(unsigned int)(sample_info_array[index].bt_array[i]));
			}
			curlen -= 2;
			log->data[curlen] = '\0';
			log->length = curlen;
		}
		return (int)(sample_info_array[index].bt_size - PROFIL_TRIM_STACK_DEPTH);
	}
	else
	{
		return 0;
	}
}
#else
int profil_backtrace_symbols(log_t *log, int bufsize, int index)
{
	char **strings = NULL;
	size_t i;
	int initsize;
	int curlen;
	int stringlen;

	if(log == NULL)
		return 0;

	initsize = log->length;
	curlen = initsize;
	log->data[curlen] = '\0';	// is this necessary ?
	strings = BACKTRACE_SYMBOLS(sample_info_array[index].bt_array,
			sample_info_array[index].bt_size);

	if(likely(strings != NULL))
	{
		for(i = 0; i < sample_info_array[index].bt_size; i++)
		{
			stringlen = strlen(strings[i]) + 14;
			if(curlen + stringlen >= bufsize + initsize)
				break;

			curlen += snprintf(log->data + curlen, bufsize - curlen, "%010u`,%s`,",
					(unsigned int)(sample_info_array[index].bt_array[i]),
					strings[i]);
		}
		curlen -= 2;
		log->data[curlen] = '\0';
		log->length = curlen;
		free(strings);
	}
	else
	{
		for(i = 0; i < sample_info_array[index].bt_size; i++)
		{
			stringlen = 23;
			if(curlen + stringlen >= bufsize + initsize)
				break;

			curlen += snprintf(log->data + curlen, bufsize - curlen, "%010u`,(unknown)`,",
					(unsigned int)(sample_info_array[index].bt_array[i]));
		}
		curlen -= 2;
		log->data[curlen] = '\0';
		log->length = curlen;
	}
	return (int)(sample_info_array[index].bt_size);
}
#endif

void *profil_log_func(void __unused * data)
{
	probeBlockStart();

	sigset_t profsigmask;
	sigemptyset(&profsigmask);
	sigaddset(&profsigmask, SIGPROF);
	pthread_sigmask(SIG_BLOCK, &profsigmask, NULL);

	while(profil_thread_on)
	{
		while(!IS_EMPTY_SAMPLE_ARRAY)
		{
			sample_read_index = (sample_read_index + 1) % SAMPLE_MAX_IN_INTERVAL;
		}
		usleep(PROFIL_LOG_SENDING_INTERVAL_USEC);
	}
	probeBlockEnd();

	return NULL;
}

void __cyg_profile_func_enter(void *this, void *callsite)
{
	probeInfo_t probeInfo;

	sigset_t profsigmask, oldsigmask;
	sigemptyset(&profsigmask);
	sigaddset(&profsigmask, SIGPROF);
	pthread_sigmask(SIG_BLOCK, &profsigmask, &oldsigmask);

	probeBlockStart();
	do {
		// remove custom chart callback function
		if(gTraceInfo.custom_chart_callback_count > 0)
		{
			if(custom_cb_addr == (void*)-1)
			{
				char **strings = BACKTRACE_SYMBOLS(&callsite, 1);
				if(likely(strings != NULL))
				{
					if(strstr(strings[0], CUSTOM_CB_FUNC_NAME) != NULL)
					{
						custom_cb_addr = callsite;
						free(strings);
						break;
					}
					free(strings);
				}
			}
			else
			{
				if(callsite == custom_cb_addr)
					break;
			}
		}

		setProbePoint(&probeInfo);

		gettimeofday(&(elapsed_time_array[elapsed_time_index].startTime), NULL);
		elapsed_time_array[elapsed_time_index].self = this;
		elapsed_time_index++;

	} while(0);
	probeBlockEnd();

	pthread_sigmask(SIG_SETMASK, &oldsigmask, NULL);
	return;
}

void __cyg_profile_func_exit(void *this, void *callsite)
{
	probeInfo_t probeInfo;

	sigset_t profsigmask, oldsigmask;
	sigemptyset(&profsigmask);
	sigaddset(&profsigmask, SIGPROF);
	pthread_sigmask(SIG_BLOCK, &profsigmask, &oldsigmask);

	probeBlockStart();
	do {
		// remove custom chart callback function
		if(gTraceInfo.custom_chart_callback_count > 0)
		{
			if(custom_cb_addr == (void*)-1)
			{
				char **strings = BACKTRACE_SYMBOLS(&callsite, 1);
				if(likely(strings != NULL))
				{
					if(strstr(strings[0], CUSTOM_CB_FUNC_NAME) != NULL)
					{
						custom_cb_addr = callsite;
						free(strings);
						break;
					}
					free(strings);
				}
			}
			else
			{
				if(callsite == custom_cb_addr)
					break;
			}
		}

		elapsed_time_index--;

		if(this != elapsed_time_array[elapsed_time_index].self)
		{ // this should never happen
			;// function exit: enter/exit function matching failed!!
		}

		gettimeofday(&(elapsed_time_array[elapsed_time_index].endTime), NULL);
		timersub(&(elapsed_time_array[elapsed_time_index].endTime),
				&(elapsed_time_array[elapsed_time_index].startTime),
				&(elapsed_time_array[elapsed_time_index].resultTime));

		setProbePoint(&probeInfo);
	} while(0);
	probeBlockEnd();

	pthread_sigmask(SIG_SETMASK, &oldsigmask, NULL);
	return;
}

#if defined(__i386__)
void mcount(void)
{
	return;
}

#elif defined(__arm__)
void mcount_internal(u_long __unused frompc, u_long __unused selfpc)
{
	return;
}

void __attribute__((__naked__)) __gnu_mcount_nc(void)
{
	asm(
			"push {r0, r1, r2, r3, lr}\n\t"
			"bic r1, lr, #1\n\t"
			"ldr r0, [sp, #20]\n\t"
			"bl mcount_internal\n\t"
			"pop {r0, r1, r2, r3, ip, lr}\n\t"
			"bx ip");
}
#endif

static inline void profil_count(void *pc)
{
#if defined(__arm__)
	/* This is a hack to prevent profil_count function from being called
	   inside of __gnu_mcount_nc. DO NOT put any other function between
	   __gnu_mcount_nc and profil_count. */
	if((pc >= (void*)(&profil_count - 0x18)) && (pc <= (void*)(&profil_count)))
		return;
#endif
	if(gProbeBlockCount != 0)
		return;

	probeBlockStart();

	real_pthread_mutex_lock(&profil_log_mutex);
	do {
		if(IS_FULL_SAMPLE_ARRAY)
		{	// when this happens, array size should be increased
			// profil log: sample info array is full
			break;
		}
		sample_info_array[sample_write_index].time = getTime();
		sample_info_array[sample_write_index].pc = pc;
#ifdef USING_BACKTRACE
		sample_info_array[sample_write_index].bt_size
			= backtrace(sample_info_array[sample_write_index].bt_array, MAX_STACK_DEPTH);
//#if defined(__i386__)
// restore frame clobbered by signal handler
		sample_info_array[sample_write_index].bt_array[2] = pc;
//#endif
#else
		sample_info_array[sample_write_index].bt_array[0] = pc;
		{
			int i;
			for(i = 0; i < elapsed_time_index; i++)
			{
				sample_info_array[sample_write_index].bt_array[i + 1]
					= elapsed_time_array[i].self;
			}
			sample_info_array[sample_write_index].bt_size = elapsed_time_index + 1;
		}
#endif
		sample_write_index = (sample_write_index + 1) % SAMPLE_MAX_IN_INTERVAL;
	} while(0);
	real_pthread_mutex_unlock(&profil_log_mutex);

	probeBlockEnd();
}

#if defined(__i386__)
static void profil_counter(int __unused signo, const struct sigcontext scp)
{
	profil_count((void *) GET_PC(scp));

	/* This is a hack to prevent the compiler from implementing the
	   above function call as a sibcall. The sibcall would overwrite
	   the signal context */
	asm volatile("");
}
#elif defined(__arm__)
static void profil_counter(int __unused signr, siginfo_t __unused * si,
			   struct ucontext *uctx)
{
	profil_count((void *) GET_PC(uctx));

	/* This is a hack to prevent the compiler from implementing the
	   above function call as a sibcall. The sibcall would overwrite
	   the signal context */
	asm volatile("");
}
#endif

#if 0
// this function can cause floating point exception.
int __profile_frequency(void)
{
	/*
	 * Discover the tick frequency of the machine if something goes wrong,
	 * we return 0, an impossible hertz.
	 */
	struct itimerval tim;

	tim.it_interval.tv_sec = 0;
	tim.it_interval.tv_usec = 1;
	tim.it_value.tv_sec = 0;
	tim.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &tim, 0);
	setitimer(ITIMER_REAL, 0, &tim);
	if (tim.it_interval.tv_usec < 2)
		return 0;
	return (1000000 / tim.it_interval.tv_usec);
}
#endif

int __profil(int mode)
{
	struct sigaction act;
	struct itimerval timer;

	static struct sigaction oact;
	static struct itimerval otimer;

	if(profil_thread_on != 1)
	{
		profil_option = mode;
		return 0;
	}

	if(mode == 0)
	{
		if(profil_turned_on == 0)
		{
			return 0;
		}

		if(setitimer(ITIMER_PROF, &otimer, NULL) < 0)
			return -1;
		profil_turned_on = 0;
		return sigaction(SIGPROF, &oact, NULL);
	}

	if(profil_turned_on == 1)
	{
		if(setitimer(ITIMER_PROF, &otimer, NULL) < 0
				|| sigaction(SIGPROF, &oact, NULL) < 0)
		{
			return -1;
		}
	}
	profil_turned_on = 1;

	act.sa_handler = (sighandler_t) &profil_counter;
#if defined(__i386__)
	act.sa_flags = SA_RESTART;
#elif defined(__arm__)
	act.sa_flags = SA_RESTART | SA_SIGINFO;
#endif
	sigfillset(&act.sa_mask);
	if(sigaction(SIGPROF, &act, &oact) < 0)
		return -1;

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 1000000 / __profile_frequency();
	timer.it_interval = timer.it_value;
	return setitimer(ITIMER_PROF, &timer, &otimer);
}

void __monstartup(u_long lowpc, u_long highpc)
{
	low_pc = lowpc;
	high_pc = highpc;

	pthread_mutex_init(&profil_log_mutex, NULL);
	probeBlockStart();
	profil_thread_on = 1;
	__profil(profil_option);
	if(pthread_create(&profil_log_thread, NULL, &profil_log_func, NULL) < 0)
	{
		perror("Fail to create profil_log thread");
	}
	probeBlockEnd();
	return;
}

void _mcleanup(void)
{
	__profil(0);
	profil_thread_on = 0;
	return;
}
