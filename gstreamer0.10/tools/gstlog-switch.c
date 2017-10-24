
#include <pthread.h>
#include <sched.h>
#include <syscall.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


static unsigned char bStopReadpipe = 0;
static int fdReadPipe = -1;

int CreateThread(void *(*start_routine)(void*), void* pParam)//, int* pThreadId)
{
	int err;
	pthread_t	 t_id = 0;
	pthread_attr_t tattr;

	err = pthread_attr_init(&tattr);
	if (err == 0)
	{
		err = pthread_attr_setinheritsched(&tattr, PTHREAD_EXPLICIT_SCHED);
	}

	if (err == 0)
	{
		err = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	}

	if (err == 0)
	{
	        err = pthread_create(&t_id, &tattr, start_routine, pParam);
	}
	if (err == 0)
	{
		err = pthread_attr_destroy(&tattr);
	}
#if 0
	if(pThreadId != 0)
	{
		*pThreadId = (int)t_id;
	}
#endif
	return err;
}

static void* readPipeAndPrintout(void* param)
{
	char* pipename = param;
	fdReadPipe = open(pipename, O_RDONLY|O_NONBLOCK);
	if (fdReadPipe != -1)
	{
		printf("readPipe[ %s ] Opened, fdReadPipe[ %d ]\n", pipename, fdReadPipe);
		while(bStopReadpipe == 0)
		{
			char str[1024] = {0,};
			char num = 0;
			int len = read(fdReadPipe, str, 1024);
			if (len > 0)
				printf("%s", str);
			else
				usleep(5000);
		}

		if (fdReadPipe != -1)
		{
			close(fdReadPipe);
			fdReadPipe = -1;
			printf("readPipe[ %s ] Closed\n", pipename);
		}
	}
	else
		printf("cannot open readPipe[ %s ]\n", pipename);

	return;
}


int main()
{
#define INPIPE_PREFIX "/tmp/.gstIn"
#define OUTPIPE_PREFIX "/tmp/.gstOut"

	int n=0;
	char pid[128] = {0,};
	char inPipeName[256] = INPIPE_PREFIX;
	char outPipeName[256] = OUTPIPE_PREFIX;
	int fd = 0;
	printf("Select PID : ");
	fgets(pid, 128, stdin);
	memcpy(inPipeName + strlen(INPIPE_PREFIX), pid, strlen(pid));
	memcpy(outPipeName + strlen(OUTPIPE_PREFIX), pid, strlen(pid));
	inPipeName[strlen(inPipeName)-1] = '\0';
	outPipeName[strlen(outPipeName)-1] = '\0';
	printf("inpipe : %s \n", inPipeName);
	printf("outpipe : %s \n", outPipeName);
	CreateThread(readPipeAndPrintout, (void*)outPipeName);
	fd = open(inPipeName, O_WRONLY);
	if (fd != -1)
	{
		printf("fd[ %d ] opened\n", fd);
		while(1)
		{
			char tmp[10]={0,};
			fgets(tmp, 10, stdin);
			unsigned int tmpnum = atoi(tmp);
			if (tmpnum <= 0xff)
			{
				unsigned char num = (unsigned char)tmpnum;
				//printf("num[ %d,  %c,  %x]\n", num, num, num);
				write(fd, &num, 1);
				if (num == 99)
				{
					bStopReadpipe = 1;
					if (fdReadPipe != -1)
					{
						close(fdReadPipe);
						fdReadPipe = -1;
					}
					printf("fd[ %d ] closed\n", fd);
					close(fd);
					return 0;
				}
			}
			else
				printf("No item[ %d ]\n", tmpnum);
		}
	}
	else
		printf("fail to open [ %s ][ %d ], errno[ %d ]\n", inPipeName, strlen(inPipeName), errno);

	return 0;
}
