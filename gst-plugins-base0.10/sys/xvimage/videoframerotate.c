/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Younghyun Kim <yh_46.kim.kim@samsung.com>, Yunsu Kim <ystoto.kim@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifdef	ENABLE_ROTATE_TEST
#include "libavutil/avstring.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libavcodec/opt.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#endif

#include "videoframerotate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>

#define TILE_W_SIZE	16
#define TILE_H_SIZE	32
#define PANEL_WIDTH 1920
#define PANEL_HEIGHT 1080
#define VIDEO_ROTATION_THREAD_COUNT 4

#define MAX_SUPPORTED_SCALED_WIDTH  606
#define MAX_SUPPORTED_SCALED_HEIGHT 1080
#define MAX_SUPPORTED_FPS 60

#if 0
#define LOW_RESOLUTION_WIDTH 720
#define LOW_RESOLUTION_HEIGHT 404
#else
#define LOW_RESOLUTION_WIDTH 720
#define LOW_RESOLUTION_HEIGHT 480
#endif

#define MAX_SKIP_LENGTH 2

/* Max size of rotation support.  
     Considering of the rotation performance, we only support the file size smaller than FHD.
     These values must be divisible by eight.
*/
#define MAX_ROTATE_SUPPORT_WIDTH 1920	
#define MAX_ROTATE_SUPPORT_HEIGHT 1088

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))

#ifdef  ENABLE_ROTATE_TEST
#define YUV420_YCbCrInterleaved
#define YUV420_YUV420

struct options
{
	int streamId;
	int frames;
	int nodec;
	int degree;
	int thread_count;
	int64_t lstart;
	char finput[256];
	char foutput1[256];
	char foutput2[256];
	char foutput3[256];
	char foutput4[256];
};

int parse_options(struct options *opts, int argc, char** argv)
{
    int optidx;
    char *optstr;

    if (argc < 2) return -1;

    opts->streamId = -1;
    opts->lstart = -1;
    opts->frames = -1;
    opts->foutput1[0] = 0;
    opts->foutput2[0] = 0;
    opts->foutput3[0] = 0;
    opts->foutput4[0] = 0;
    opts->nodec = 0;
    opts->degree = 0;
    opts->thread_count = 0;
    strcpy(opts->finput, argv[1]);

    optidx = 2;
    while (optidx < argc)
    {
        optstr = argv[optidx++];
        if (*optstr++ != '-') return -1;
        switch (*optstr++)
        {
        case 's':  //< stream id
            opts->streamId = atoi(optstr);
            break;
        case 'f':  //< frames
            opts->frames = atoi(optstr);
            break;
        case 'k':  //< skipped
            opts->lstart = atoll(optstr);
            break;
        case 'o':  //< output
            strcpy(opts->foutput1, optstr);
            strcat(opts->foutput1, ".raw");
            strcpy(opts->foutput2, optstr);
            strcat(opts->foutput2, ".yuv");
			strcpy(opts->foutput3, optstr);
			strcat(opts->foutput3, "_fr.yuv");
			strcpy(opts->foutput4, optstr);
			strcat(opts->foutput4, "_interlace.yuv");
            break;
        case 'n': //decoding and output options
            if (strcmp("dec", optstr) == 0)
                opts->nodec = 1;
            break;
        case 'd':
            opts->degree = atoi(optstr);
            break;
        case 't':
            opts->thread_count = atoi(optstr);
            break;
        default:
            return -1;
        }
    }

    return 0;
}

void show_help(char* program)
{
    GST_LOG("Simple test program ");
    GST_LOG("Usage: %s inputfile [-sstreamid [-fframes] [-kskipped] [-ooutput_filename(without extension)] [-ddegree(90/180/270)] [-tthread_count]] ",
           program);
    return;
}

static void log_callback(void* ptr, int level, const char* fmt, va_list vl)
{
	if(level>48)
       return;
	vfprintf(stdout, fmt, vl);
}

#endif

struct PerThreadContext
{
	int thread_count;
	int id;
	int starty;
	int endy;
	int starty_c;
	int endy_c;
	int srcw;
	int srch;
	int srcLine;
	int dstw;
	int dsth;
	int degree;
	void* context;
	int update;
	int finish;
	pthread_mutex_t *context_mutex;
	pthread_mutex_t update_mutex;
	pthread_cond_t update_cond;
	pthread_mutex_t finish_mutex;
	pthread_cond_t finish_cond;
	int* stop;
	unsigned char* srcY;
	unsigned char* srcCbCr;
	unsigned char* dstY;
	unsigned char* dstCbCr;
	unsigned char* targetYUYV;
	unsigned char*targetGAbuffer;
	/*90 degrees*/
	int* Ty1;
	int* Ty2;
	int* Ty3;
	int* Tuv1;
	int* Tuv2;
	int* Tuv3;
	/*270 degrees*/
	int* Ty4;
	int* Ty5;
	int* Ty6;
	int* Tuv4;
	int* Tuv5;
	int* Tuv6;
	unsigned long long rot_time; /*debug*/
	unsigned long long rot_time1;/*debug*/
	/* 90 degrees totated tiled*/
	int * TiledRotatedTableY90Deg;
	int * TiledRotatedTableUV90Deg;
	/* 270 degrees totated tiled*/
	int * TiledRotatedTableY270Deg;
	int * TiledRotatedTableUV270Deg;
	//Rotated tables for 180
	int * TiledRotatedTable180Deg;
	int RotatedTableOffset_90_Y;
	int RotatedTableOffset_90_UV;
	int RotatedTableOffset_270_Y;
	int RotatedTableOffset_270_UV;
	int ConvHeight;
	int ConvWidth;
	int RotatedTableOffsetIncrement_90;
	int RotatedTableOffsetIncrement_270;
};

struct AllThreadContext
{
	pthread_t* workers;
	struct PerThreadContext* thread;
	int thread_count;
	int context;
	int stop;
	int update;
	int degree;
	unsigned char* srcY;
	unsigned char* srcCbCr;
	unsigned char* dstY;
	unsigned char* dstCbCr;
	//back buffer
	unsigned char* targetYUYV;
	//GA Buffer for 180 Degree
	unsigned char*targetGAbuffer;
	pthread_mutex_t context_mutex;
};


/* private internal function declarations */
static unsigned long long get_sw_time();

static void* t_RotateThreadWorker(void *arg);
static gboolean t_CreateRotateThread(void *(*start_routine)(void*), int iPriority, void* pParam, pthread_t* pThreadId, gboolean detachedMode);

static gboolean t_IsHighFPSFrameToSkipRotate(struct VideoFrameRotateContext* context);
static void t_UpdateThreadParams(struct VideoFrameRotateContext* context);
static gboolean t_CalculateTargetDimentionForVeryLowResolution(struct VideoFrameRotateContext* context);
static gboolean t_CalculateTargetDimention(struct VideoFrameRotateContext* context);
static void t_ConvertYUYV(struct VideoFrameRotateContext* context);
static void t_FlushCache(struct VideoFrameRotateContext* context);

static gboolean t_Rotate_0_Thread(struct VideoFrameRotateContext* context, struct PerThreadContext* p);
static gboolean t_Rotate_90_270_Thread(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *ty1, int *ty2, int* ty3, int *tuv1, int *tuv2, int *tuv3);
static gboolean t_Rotate_90_270_Thread_for_interscan(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *ty1, int *ty2, int* ty3, int *tuv1, int *tuv2, int *tuv3);
static gboolean t_Rotate_180_Thread_Opt(struct VideoFrameRotateContext* context, struct PerThreadContext* p);
static gboolean t_Rotate_180_Thread(struct VideoFrameRotateContext* context, struct PerThreadContext* p);
static gboolean t_Rotate_180_Thread_for_interscan(struct PerThreadContext* p);
static gboolean t_Rotate_90_270_Tiled_Thread(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *table_tiled_y, int *table_tiled_c);
static gboolean t_Rotate_90_Tiled_Thread_Opt(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *table_tiled_y, int *table_tiled_c);
static gboolean t_Rotate_270_Tiled_Thread_Opt(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *table_tiled_y, int *table_tiled_c);
static gboolean t_Rotate_180_Tiled_Thread(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *table_tiled_180);
static gboolean t_Rotate_180_Tiled_Thread_for_interlace(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *table_tiled_180);
static gboolean t_InitVideoRotationContext(struct VideoFrameRotateContext* context, int linesize_open);
static void t_DeInitVideoRotationContext(struct VideoFrameRotateContext* context);
static void t_FreeThreadsForRotation(struct VideoFrameRotateContext* context);
static void t_FreeThreadTablesForRotation(struct VideoFrameRotateContext* context, struct PerThreadContext* p);
static void t_FreeThreadTiledTablesForRotation(struct VideoFrameRotateContext* context, struct PerThreadContext* p);
static void t_FreeRotationTiledTables(struct VideoFrameRotateContext* context);
static void t_FreeBackBufferMemory(struct VideoFrameRotateContext* context);
static void t_FreeGAMemory(struct VideoFrameRotateContext* context);
static void t_FreeGAScaledY_CbCrBuffers(struct VideoFrameRotateContext* context);
static void t_DecideThreadCountForRotation(struct VideoFrameRotateContext* context);
static void t_DecideLineSizeForTiledFormat(struct VideoFrameRotateContext* context,int linesize_open);
static gboolean t_InitRotationTables(struct VideoFrameRotateContext* context, int srcw, int srch, int srcLine, int dstw, int dsth, int rx, int ry);
static gboolean t_InitTiledToLinearTables(struct VideoFrameRotateContext* context);
static gboolean t_initRotatedTiledTables(struct VideoFrameRotateContext* context, int dstw, int dsth);
static void t_FreeRotationTables(struct VideoFrameRotateContext* context);
static void t_FreeTiledToLinearTables(struct VideoFrameRotateContext* context);
static gboolean t_CreateGAMemory(struct VideoFrameRotateContext* context);
static gboolean t_CreateBackBufferMemory(struct VideoFrameRotateContext* context);
static gboolean t_AssignGAMemoryForScaling(struct VideoFrameRotateContext* context);
static void t_DisableCache(struct VideoFrameRotateContext* context);
static gboolean t_AssignThreadsForRotation(struct VideoFrameRotateContext* context);
static gboolean t_InitThreadTiledTables(struct VideoFrameRotateContext* context, struct PerThreadContext* p);
static gboolean t_InitThreadTables(struct VideoFrameRotateContext* context, struct PerThreadContext* p);
static gboolean t_HandleLeftOverTiledData(int nLeftOverData, unsigned long long *srcY64bitPtr0, unsigned long long *srcY64bitPtr1, unsigned long long *srcUV64bitPtr, unsigned char* dstyuyv0, unsigned char* dstyuyv1);
static gboolean t_CreateRotationTables(struct VideoFrameRotateContext* context, int linesize_open);
static gboolean allocate_rotate_tbm_buffer(struct VideoFrameRotateContext* context);
static gboolean free_rotate_tbm_buffer(struct VideoFrameRotateContext* context);
#ifdef ENABLE_LOCAL_ROTATE_BUFFER
static gboolean allocate_rotate_local_buffer(struct VideoFrameRotateContext* context);
static gboolean free_rotate_local_buffer(struct VideoFrameRotateContext* context);
#endif

/* implementations for the public functions */

struct VideoFrameRotateContext* videoframe_rotate_create()
{
	struct VideoFrameRotateContext* context = NULL;
	context = (struct VideoFrameRotateContext*)g_malloc(sizeof(struct VideoFrameRotateContext));

	context->m_bIsVideoRotationEnabled = FALSE;
	context->m_iRotationDegree = 0;
	context->m_bIsRotationTableInit = FALSE;
	context->m_bIsRotationThreadsInit = FALSE;
	context->m_bIsRotateBufferAllocated = FALSE;
	context->Ty1 = NULL;
	context->Ty2 = NULL;
	context->Ty3 = NULL;
	context->Tuv1 = NULL;
	context->Tuv2 = NULL;
	context->Tuv3 = NULL;
	context->Ty4 = NULL;
	context->Ty5 = NULL;
	context->Ty6 = NULL;
	context->Tuv4 = NULL;
	context->Tuv5 = NULL;
	context->Tuv6 = NULL;
	context->TiledToLinearTableY = NULL;
	context->TiledToLinearTableUV = NULL;
	context->TiledRotatedTableY90Deg = NULL;
	context->TiledRotatedTableUV90Deg = NULL;
	context->TiledRotatedTableY270Deg = NULL;
	context->TiledRotatedTableUV270Deg = NULL;
	context->TiledRotatedTable180Deg = NULL;
	context->m_pGAScaledYBuffer =  NULL;
	context->m_pGAScaledCbCrBuffer =  NULL;
	context->m_pBackBuffer = NULL;
	context->m_pGARotateBuffer = NULL;
	context->m_bIsScaleBufferParmsSet = FALSE;
	context->m_bIsRotateAngleChanged = FALSE;
	context->m_bIsRotationEnabledForTiledFormat = FALSE;
	context->m_bIsApplyRotationForOptimizations = FALSE;
	context->m_IsEnableGAMemoryForTiledRotation = FALSE;
	context->m_iTargetScaledWidth = 0;
	context->m_iTargetScaledHeight = 0;
	context->m_iOriginalWidth = 0;
	context->m_iOriginalHeight = 0;
	context->m_iScaledWidth = 0;
	context->m_iScaledHeight = 0;
	context->m_iRotationThreadCount = VIDEO_ROTATION_THREAD_COUNT;
	//To Use BackBuffer of GFX1 :1 , Allocated GA Memory : 0
	context->m_iRotationGAScalledBuffer_GFX1 = 0;
	// To minimize cache misses and to change read and write directions
	context->m_iRotationXY = 0;
	// For FOXB 270 Deg performance Enhancement
	context->m_iRotationYRaise = 0;
	// To Enable/Disable VideoRotation Prints
	context->m_bIsRotationPrintsEnabled = FALSE;
	// for thread affinity
	context->m_imask = 0;
	context->m_bIsTheadAffinity = FALSE;
	context->m_iCpuCoreCount = 0;
	// Enable CacheMiss related Optimizations to reduce CPU Usage
	context->m_bIsCacheMissOptEnabled = FALSE;
	// System Memory to avoid GA memory cache misses
	context->m_pSysOutputYBuffer = NULL;
	context->m_pSysOutputCbCrBuffer = NULL;
	// Buffer Pointers to choose either GA Memory or System Memory
	context->m_pOutputYBufPtr = NULL;
	context->m_pOutputCbCrBufPtr = NULL;
	// For Cache Flush
	context->m_ihResolution = 0;
	context->m_ivResolution = 0;
	// For FOXP 180 Deg Optimized
	context->m_bIs180DegOptimized = FALSE;
	context->m_bEnable180DegOpti = FALSE;
	// For FOXB JAVA0 memory usage for GA
	context->m_bIsJava0MemoryUsedForGA = FALSE;
	// FOXP/FOXB, For SkipOptimizations, 3 : skip 1 row/coloum for every 2 rows/column
	// m_iSkipFactor 2 : skip every alternate row/column
	context->m_iSkipFactor = 0;
	// Decide Interlaced scan type
	context->m_bIsInterlacedScanType = FALSE;
	// FOXP/FOXB 90/270 Deg skip Optimized
	context->m_bIsOptimized = FALSE;
	context->m_hUmpHandle = NULL;
	context->m_bIsLowResolution = FALSE;
	context->m_bIsCallInterScanFunc = FALSE;
	//Very low resolution quality improvement < 176x144
	context->m_IsVeryLowResolutionOpt = FALSE;
	context->m_eVideoDataFormat = -1;
	context->m_eColorFormat = -1;
	context->m_iTableWidth_90_270 = 0;
	context->m_iTableHeight_90_270 = 0;
	// To Handle FPS > 30
	context->m_iFramesPerSec = 0;
	context->m_bIsFPSMoreThanSupported = FALSE;
	context->m_FPSRatio = 0.0f;
	context->m_FPSAccumated = 0.0f;
	// Source LineSize
	context->m_iLineSizeConvertedFrame = 0;

	context->g_frames = 0;
	context->g_total_time = 0;
	context->g_rot_time = 0;

#ifdef  ENABLE_TBM
	context->bufmgr_AP = NULL;
	context->bo_AP = NULL;
	context->drm_fd = -1;
	context->boAP_key = 0;
	context->disp = NULL;
	#ifdef ENABLE_LOCAL_ROTATE_BUFFER
		context->pLocalRotateBuffer_Y = NULL;
		context->pLocalRotateBuffer_CbCr = NULL;
	#endif
#endif

	return context;
}

void videoframe_rotate_destroy(struct VideoFrameRotateContext* context)
{
	if(context)
	{
		g_free(context);
		context = NULL;
	}
}

gboolean videoframe_rotate_open(struct VideoFrameRotateContext* context, gboolean bIsVideoRotationEnabled, int iRotationDegree, int iOriginalWidth, int iOriginalHeight, int iCodecId, gboolean bIsInterlacedScanType, int linesize_open)
{
	if(context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("VideoRotate: Open ++++++++++ ");
	}

	gboolean RetStatus;
	context->m_bIsVideoRotationEnabled = bIsVideoRotationEnabled;
	context->m_iRotationDegree = iRotationDegree;

	context->m_iOriginalWidth  = iOriginalWidth;
	context->m_iOriginalHeight = iOriginalHeight;

	// Rrestricting maximum supporting height is 1080, even height is 1088
	if (context->m_iOriginalHeight > PANEL_HEIGHT)
	{
		context->m_iOriginalHeight = PANEL_HEIGHT;
	}

	context->m_iCodecId = iCodecId;

	if (bIsInterlacedScanType == TRUE)
	{
		context->m_bIsInterlacedScanType = TRUE;
		GST_LOG("VideoRotate: SET VIDEOROTATION FOR INTERLACED SCAN TYPE ");
		//Interlaced scan type low resolution
		if (context->m_iOriginalWidth<1280 && context->m_iOriginalHeight<720)
		{
			context->m_bIsLowResolution = TRUE;
		}
		if ((context->m_bIsInterlacedScanType == TRUE) && (context->m_bIsLowResolution == TRUE))
		{
			context->m_bIsCallInterScanFunc = TRUE;
		}
	}

	// Init entire Video Rotation context: threads, tables, etc
	RetStatus = t_InitVideoRotationContext(context,linesize_open);
	if(RetStatus)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("VideoRotate: VideoRotate Open SUCCESS RetStatus[%d] ",RetStatus);
		}
	}
	else
	{
		GST_LOG("VideoRotate: VideoRotate Open FAILED RetStatus[%d] ",RetStatus);
	}

	context->g_frames = 0;
	context->g_total_time = 0;
	context->g_rot_time = 0;

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("VideoRotate: VideoRotate Open ---------- ");
	}
	return RetStatus;

}

void videoframe_rotate_close(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG(" VideoRotate Close ++++++++++ ");
	}

	t_DeInitVideoRotationContext(context);

	//Reset Rotation Params
	context->m_bIsVideoRotationEnabled = FALSE;
	context->m_iRotationDegree = 0;

	context->g_frames = 0;
	context->g_total_time = 0;
	context->g_rot_time = 0;
	context->m_imask = 0;
	context->m_bIsTheadAffinity = FALSE;

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG(" VideoRotate Close ---------- ");
	}
}

gboolean videoframe_rotate_apply(struct VideoFrameRotateContext* context, int HwDecoderHandle, struct VIDEO_FRAME* pOutputFrame, int RotationDegree, int *piFrameDone)
{
	if (context->m_pAllThreadContextRotate == NULL)
	{
		GST_LOG("!!!!!!!!!!!m_pAllThreadContextRotate : NULL !!!!!!!!!! ");
		return FALSE;
	}
	context->g_frames++;

	// To handle FPX > 30, need to skip frame for not render
	if (context->m_bIsFPSMoreThanSupported == TRUE)
	{
		if ( t_IsHighFPSFrameToSkipRotate(context) == TRUE)
		{
			*piFrameDone = 0;
			return TRUE;
		}
	}

	gboolean bRet = FALSE;
	unsigned long long tt;

	// TODO: needs to replace VIDEO_FRAME with v4l2_drm structure.
	memset(&context->tOutVideoFrameData, 0x0, sizeof(struct VIDEO_FRAME));

	unsigned long long totaltime = get_sw_time();

	// convert to common structure
	// TODO: needs to replace ConvertVideoFrame with the new api supported on tizen platform.
	//bRet = FALSE; //CPlatformInterface::GetInstance()->ConvertVideoFrame(HwDecoderHandle, pOutputFrame, &tOutVideoFrameData);
	/*Simulate Convert Video Frame*/
	if(pOutputFrame){
		memcpy(&context->tOutVideoFrameData, pOutputFrame, sizeof(struct VIDEO_FRAME));
		bRet = TRUE;
	}

	if(bRet == FALSE )
	{
		//LOG_ERROR("Convert video frame failed");
		return FALSE;
	}

	tt = get_sw_time();

	/*Multi thread scale and rotate algorithm using tables*/
	struct PerThreadContext *p1;
	int j;
	context->m_pAllThreadContextRotate->context = 0;

	for(j=0; j<context->m_pAllThreadContextRotate->thread_count; j++)
	{
		p1 = &context->m_pAllThreadContextRotate->thread[j];
		p1->srcY =  context->tOutVideoFrameData.pData0;
		p1->srcCbCr = context->tOutVideoFrameData.pData1;
		p1->dstw = context->m_iScaledWidth;
		p1->degree = context->m_iRotationDegree;
		pthread_mutex_lock(&p1->update_mutex);
		p1->update=1;
		p1->finish=0;
		pthread_cond_signal(&p1->update_cond);
		pthread_mutex_unlock(&p1->update_mutex);
	}

	for(j=0; j<context->m_pAllThreadContextRotate->thread_count; j++)
	{
		p1 = &context->m_pAllThreadContextRotate->thread[j];
		pthread_mutex_lock(&p1->finish_mutex);
		while (!p1->finish)
		{
			pthread_cond_wait(&p1->finish_cond, &p1->finish_mutex);
			break;
		}
		pthread_mutex_unlock(&p1->finish_mutex);

		pthread_mutex_lock(&p1->update_mutex);
		p1->update=0;
		pthread_mutex_unlock(&p1->update_mutex);
	}

	// For Echo-P, etc : apply Cache Flush for every frame
	if (context->m_bIsRotationEnabledForTiledFormat == FALSE)
	{
		t_FlushCache(context);
	}
	else // For mstar-X10Pc : apply Cache Flush when Rotation angle changed, because CacheFlush takes 5 msecs per frame on X10P
	{
		if (context->m_bIsRotateAngleChanged == TRUE)
		{
			t_FlushCache(context);
		}
	}

	context->g_rot_time += get_sw_time() - tt;

	//YUV422 Conversion - mstar-X10P
	if ((context->m_bIsRotationEnabledForTiledFormat == TRUE) && (context->m_bIsApplyRotationForOptimizations == FALSE))
	{
		GST_LOG("Calling format conversion for non Full HD Rotation ");
		if (context->m_iRotationDegree != 180)
		{
			t_ConvertYUYV(context);
		}
	}

	// TODO: needs to replace both api calls with the proper api calss supported on tizen platform.
	// free converted video data on common structure
	//bRet = FALSE;//CPlatformInterface::GetInstance()->FreeConvertedVideoFrame(HwDecoderHandle, &context->tOutVideoFrameData);
	/*Simulate Free Convert Video Frame*/
	{
		context->tOutVideoFrameData.pData0 = NULL;
		context->tOutVideoFrameData.pData1 = NULL;
		context->tOutVideoFrameData.pData2 = NULL;
		context->tOutVideoFrameData.pData3 = NULL;
		bRet = TRUE;
	}
	// free platform data which from hw decoder
	//CPlatformInterface::GetInstance()->freeVideoDecoderResources(HwDecoderHandle, pOutputFrame);
	/*Simulate Free Video Decoder Resources*/
	{
		memset(pOutputFrame, 0x0, sizeof(struct VIDEO_FRAME));
	}

	// TODO: needs to replace VIDEO_FRAME with v4l2_drm structure.
	memcpy(pOutputFrame, &context->tOutVideoFrameData, sizeof(struct VIDEO_FRAME));

	// TODO: needs to replace VIDEO_DATA_FORMAT_YCBCR with the proper enum for tizen platform.
	if(context->m_eVideoDataFormat == VIDEO_DATA_FORMAT_YCBCR)
	{
		GST_LOG("LINE:  %d  VideoDataFormat %d  ", __LINE__, context->m_eVideoDataFormat );
		if ((context->m_iRotationDegree == 180) && (context->m_IsEnableGAMemoryForTiledRotation ==TRUE))
		{
			//GA Buffer
			pOutputFrame->pData0 = context->m_pGARotateBuffer;
		}
		else
		{
			//Back Buffer
			pOutputFrame->pData0 = context->m_pBackBuffer;
		}

		GST_LOG("LINE:  %d    m_iTargetScaledWidth[%d] m_iScaledHeight[%d]  ", __LINE__, context->m_iTargetScaledWidth, context->m_iScaledHeight);
		pOutputFrame->pData1 = NULL;
		pOutputFrame->pData2 = NULL;
		pOutputFrame->pData3 = NULL;

		pOutputFrame->lineSize0 = context->m_iTargetScaledWidth*context->m_iScaledHeight*2;
		pOutputFrame->lineSize1 =0;
		pOutputFrame->lineSize2 =0;
		pOutputFrame->lineSize3 =0;
		pOutputFrame->width= context->m_iTargetScaledWidth;
		pOutputFrame->height= context->m_iScaledHeight;

		if (pOutputFrame->width % TILE_W_SIZE)
		{
			pOutputFrame->width += TILE_W_SIZE -(pOutputFrame->width % TILE_W_SIZE);
			pOutputFrame->lineSize0 = pOutputFrame->width * context->m_iScaledHeight * 2;
		}

		// TODO: needs to replace MD_COLOR_FORMAT_YCbCr422 and VIDEO_DATA_FORMAT_YCBCR with the proper enums for tizen platform.
		pOutputFrame->eColorFormat= MD_COLOR_FORMAT_YCbCr422;
		pOutputFrame->eVideoDataFormat = VIDEO_DATA_FORMAT_YCBCR;
		GST_LOG(" Degree[%d] lineSize0[ %d] m_iScaledWidth[%d] m_iScaledHeight[%d]  ",context->m_iRotationDegree, pOutputFrame->lineSize0,context->m_iScaledWidth,context->m_iScaledHeight);
	}
	else
	{
		//GST_LOG("LINE:  %d  VideoDataFormat %d \n", __LINE__, context->m_eVideoDataFormat );
		if (context->m_bIsCacheMissOptEnabled == TRUE)
		{
			// Copy output data from system memory to GA memory
			memcpy(context->m_pGAScaledYBuffer, context->m_pSysOutputYBuffer, context->m_iScaledWidth*context->m_iScaledHeight);
			memcpy(context->m_pGAScaledCbCrBuffer, context->m_pSysOutputCbCrBuffer, context->m_iScaledWidth*(context->m_iScaledHeight/2));
		}
		else
		{
			// No need to copy, already output data in GA memory
		}

		pOutputFrame->pData0 = context->m_pGAScaledYBuffer;
		pOutputFrame->pData1 = context->m_pGAScaledCbCrBuffer;

		if (context->m_bEnable180DegOpti == TRUE)
		{
			// Only FOXP, 180 Deg case
			pOutputFrame->lineSize0 = context->m_iTargetScaledWidth;
			pOutputFrame->lineSize1 = context->m_iTargetScaledWidth;
			pOutputFrame->width= context->m_iTargetScaledWidth;
			pOutputFrame->height= context->m_iTargetScaledHeight;
		}
		else
		{
			pOutputFrame->lineSize0 = context->m_iScaledWidth;
			pOutputFrame->lineSize1 = context->m_iScaledWidth;
			pOutputFrame->width = context->m_iScaledWidth;
			pOutputFrame->height = context->m_iScaledHeight;
		}
	}

	*piFrameDone = 1;

	context->g_total_time += get_sw_time() - totaltime;
	if (context->g_frames%40==1)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("----- FrameNum[%lld] AvgTotalTime[%lld] RotationTime[%lld] ----- ", context->g_frames,context->g_total_time/context->g_frames,context->g_rot_time/context->g_frames);
		}
	}
	return TRUE;

}

void videoframe_rotate_set_degree(struct VideoFrameRotateContext* context, int RotationDegree)
{
	if(context == NULL)
	{
		return;
	}

	if(context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("VideoRotate Angle[%d] ", RotationDegree);
	}

	if(RotationDegree != context->m_iRotationDegree)
	{
		context->m_bIsRotateAngleChanged = TRUE;
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("Rotate Angle changed ");
		}

		if(context->m_bIsFPSMoreThanSupported == TRUE)
		{
			context->m_FPSAccumated = 0.0f;
		}
	}

	if(RotationDegree == 90 || RotationDegree == 270 || RotationDegree == 180)
	{
		context->m_iRotationDegree = RotationDegree;
	}
	else
	{
	 	context->m_iRotationDegree = 0;
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("Rotate angle is set to [0] degrees ");
		}
	}

	if(context->m_IsVeryLowResolutionOpt == TRUE)
	{
	 	// only or very low resolution files like <176x144 to improve rotation quality, just swap width and height
	 	t_CalculateTargetDimentionForVeryLowResolution(context);
	}
	else
	{
		t_CalculateTargetDimention(context);
	}

	// update threads with modified width and height info
	t_UpdateThreadParams(context);
}

int videoframe_rotate_get_scaled_width(struct VideoFrameRotateContext* context)
{
	if(context)
	{
		if(context->m_bEnable180DegOpti)
		{
			GST_LOG("Get Scaled W/H %d/%d , m_bEnable180DegOpti[%d] ", context->m_iScaledWidth, context->m_iScaledHeight, context->m_bEnable180DegOpti);
			return context->m_iTargetScaledWidth;
		}
		else
		{
			GST_LOG("Get Scaled W/H %d/%d , m_bEnable180DegOpti[%d] ", context->m_iScaledWidth, context->m_iScaledHeight, context->m_bEnable180DegOpti);
			return context->m_iScaledWidth;
		}
	}

	return -1;
}

int videoframe_rotate_get_scaled_height(struct VideoFrameRotateContext* context)
{
	if(context)
	{
		if(context->m_bEnable180DegOpti)
		{
			return context->m_iTargetScaledHeight;
		}
		else
		{
			return context->m_iScaledHeight;
		}
	}

	return -1;

}

void videoframe_rotate_update_rotate_angle_change_state(struct VideoFrameRotateContext* context, gboolean bIsRotateAngleChanged)
{
	if(context)
	{
		context->m_bIsRotateAngleChanged = bIsRotateAngleChanged;
	}
}

gboolean videoframe_rotate_is_interlaced_scan_type(struct VideoFrameRotateContext* context)
{
	if(context)
	{
		return context->m_bIsInterlacedScanType;
	}

	return FALSE;
}

gboolean videoframe_rotate_can_support(struct VideoFrameRotateContext* context, int iFramesPerSec, int frm_height, int frm_width)
{
    if(context)
	{
    	context->m_iFramesPerSec = iFramesPerSec;
    	if (context->m_iFramesPerSec > MAX_SUPPORTED_FPS)
    	{
    		context->m_bIsFPSMoreThanSupported = TRUE;
    		context->m_FPSRatio = MAX_SUPPORTED_FPS/(float)context->m_iFramesPerSec;
    		GST_LOG("VideoRotate: SET VIDEOROTATION FOR FPS ABOVE 30 : FPS[%d] m_bIsFPSMoreThanSupported[%d] m_FPSRatio[%5.2f] ",context->m_iFramesPerSec,context->m_bIsFPSMoreThanSupported,context->m_FPSRatio);
    	}
    	if(frm_height > MAX_ROTATE_SUPPORT_HEIGHT || frm_width > MAX_ROTATE_SUPPORT_WIDTH)
    	{
    		context->m_bIsSIZEMoreThanSupported = TRUE;
    		GST_LOG("VideoRotate: Video frame size is out of 1920x1088 ");
    		return FALSE;
    	}
	
		GST_LOG("VideoRotate: Can support rotate !!! ");
		return TRUE;
	}

	return FALSE;
}

/* implementations for the private functions */

static unsigned long long get_sw_time()
{
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC, &tv);
	return ((unsigned long long)(tv.tv_sec)*1000000 + tv.tv_nsec/1000);
}

static gboolean t_IsHighFPSFrameToSkipRotate(struct VideoFrameRotateContext* context)
{
	context->m_FPSAccumated += context->m_FPSRatio;
	int iAccumlate = (int)context->m_FPSAccumated;
	if (iAccumlate >= 1)
	{
		context->m_FPSAccumated -= 1.0f;
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

static void t_UpdateThreadParams(struct VideoFrameRotateContext* context)
{
	int j, thread_cnt;
	struct PerThreadContext *p1 = NULL;

	if (context->m_pAllThreadContextRotate == NULL)
	{
		GST_LOG("!!!!!!!m_pAllThreadContextRotate is NULL!!!!!!!!! ");
		return;
	}

	for(j=0; j<context->m_pAllThreadContextRotate->thread_count; j++)
	{
		p1 = &context->m_pAllThreadContextRotate->thread[j];
		pthread_mutex_lock(&p1->finish_mutex);
		while (!p1->finish)
		{
			pthread_cond_wait(&p1->finish_cond, &p1->finish_mutex);
			break;
		}
		pthread_mutex_unlock(&p1->finish_mutex);

		pthread_mutex_lock(&p1->update_mutex);
		p1->update=0;
		pthread_mutex_unlock(&p1->update_mutex);
	}

	thread_cnt = context->m_pAllThreadContextRotate->thread_count;

	for(j=0; j<context->m_pAllThreadContextRotate->thread_count; j++)
	{
		p1 = &context->m_pAllThreadContextRotate->thread[j];
		p1->dstw = context->m_iScaledWidth;
		p1->dsth = context->m_iScaledHeight;
		p1->starty   = context->m_iScaledHeight * j / thread_cnt;
		p1->endy     = MIN((context->m_iScaledHeight * (j+1) / thread_cnt) - 1, context->m_iScaledHeight - 1);
		p1->starty_c = (context->m_iScaledHeight>>1) * j / thread_cnt;
		p1->endy_c   = MIN(((context->m_iScaledHeight>>1) * (j+1) / thread_cnt) - 1, (context->m_iScaledHeight>>1) - 1);
	}
}

static gboolean t_CalculateTargetDimentionForVeryLowResolution(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_CalculateTargetDimentionForVeryLowResolution +++++++ ");
	}
	gboolean RetStatus =  FALSE;

	if ( (context->m_iOriginalWidth > PANEL_WIDTH || context->m_iOriginalWidth ==0) || (context->m_iOriginalHeight > PANEL_HEIGHT || context->m_iOriginalHeight == 0))
	{
		GST_LOG("    !!!! ERROR : Unable To Proceed t_CalculateTargetDimentionForVeryLowResolution : m_iOriginalWidth[%d] m_iOriginalHeight[%d] !!! ", context->m_iOriginalWidth,context->m_iOriginalHeight);
		context->m_bIsVideoRotationEnabled = FALSE;
		GST_LOG("t_CalculateTargetDimentionForVeryLowResolution m_bIsVideoRotationEnabled[%d]------- ",context->m_bIsVideoRotationEnabled);
		return RetStatus;
	}

	if( (context->m_iRotationDegree == 180)||(context->m_iRotationDegree == 0))
	{
		context->m_iScaledWidth = context->m_iOriginalWidth;
		context->m_iScaledHeight = context->m_iOriginalHeight;
		context->m_iScaledWidth -= (context->m_iScaledWidth&0x1);
		context->m_iScaledHeight -= (context->m_iScaledHeight&0x1);
	}
	else if ((context->m_iRotationDegree == 90) ||(context->m_iRotationDegree == 270))
	{
		// swap width and height
		context->m_iScaledWidth = context->m_iOriginalHeight;
		context->m_iScaledHeight = context->m_iOriginalWidth;
		context->m_iScaledWidth -= (context->m_iScaledWidth&0x1);
		context->m_iScaledHeight -= (context->m_iScaledHeight&0x1);

		// X12 side line video garbage issue for 90/270
		if ((context->m_bIsRotationEnabledForTiledFormat == TRUE) && (context->m_iScaledWidth>240))
		{
			context->m_iScaledWidth -= MAX_SKIP_LENGTH;
		}
	}
	RetStatus = TRUE;

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG(" m_iRotationDegree[%d] m_iOriginalWidth[%d] m_iOriginalHeight[%d] m_iScaledWidth[%d] m_iScaledHeight[%d] ",context->m_iRotationDegree,context->m_iOriginalWidth,context->m_iOriginalHeight,context->m_iScaledWidth,context->m_iScaledHeight);
		GST_LOG("t_CalculateTargetDimentionForVeryLowResolution ------- ");
	}
	return RetStatus;
}

static gboolean t_CalculateTargetDimention(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_CalculateTargetDimention +++++++ ");
	}
	int X=0;
	int Y=0;
	gboolean RetStatus =  FALSE;

	if ( (context->m_iOriginalWidth > PANEL_WIDTH || context->m_iOriginalWidth ==0) || (context->m_iOriginalHeight > PANEL_HEIGHT || context->m_iOriginalHeight == 0))
	{
		GST_LOG("    !!!! ERROR : Unable To Proceed t_CalculateTargetDimention : m_iOriginalWidth[%d] m_iOriginalHeight[%d] !!!", context->m_iOriginalWidth,context->m_iOriginalHeight);
		context->m_bIsVideoRotationEnabled = FALSE;
		GST_LOG("t_CalculateTargetDimention m_bIsVideoRotationEnabled[%d]------- ",context->m_bIsVideoRotationEnabled);
		return RetStatus;
	}

	// this is required to change rotation degree from menu, otherwise 180->270 gives corrupted video due to m_bEnable180DegOpti is already TRUE
	if (context->m_bEnable180DegOpti == TRUE)
	{
		context->m_bEnable180DegOpti = FALSE;
	}

	if( (context->m_iRotationDegree == 180)||(context->m_iRotationDegree == 0))
	{
		context->m_iScaledWidth = context->m_iOriginalWidth;
		context->m_iScaledHeight = context->m_iOriginalHeight;

		context->m_iScaledWidth -= (context->m_iScaledWidth&0x1);
		context->m_iScaledHeight -= (context->m_iScaledHeight&0x1);

		if ((context->m_bIs180DegOptimized == TRUE) && (context->m_iRotationDegree == 180) && (context->m_iScaledWidth == 1920) && (context->m_iScaledHeight == 1080))
		{
			context->m_bEnable180DegOpti = TRUE;
		}
		else
		{
			context->m_bEnable180DegOpti = FALSE;
		}
	}
	else if ((context->m_iRotationDegree == 90) ||(context->m_iRotationDegree == 270))
	{
		X = MAX(context->m_iOriginalWidth, context->m_iOriginalHeight);
		Y = MIN(context->m_iOriginalWidth, context->m_iOriginalHeight);

		context->m_iScaledWidth = context->m_iOriginalHeight * Y / X;
		context->m_iScaledHeight = context->m_iOriginalWidth * Y / X;
		context->m_iScaledWidth -= (context->m_iScaledWidth&0x1);
		context->m_iScaledHeight -= (context->m_iScaledHeight&0x1);

		if ((context->m_bIsOptimized == TRUE) && (context->m_iScaledWidth == 606) && (context->m_iScaledHeight == 1080))
		{
			// Only FOXP
			context->m_iScaledWidth = 404;
			context->m_iScaledHeight = 720;
		}
		//To handle resolution 1280x1080, 1440x1080 -> scaled resolution will be 910x1080, 810x1080
		if ((context->m_iScaledWidth*context->m_iScaledHeight) > (MAX_SUPPORTED_SCALED_WIDTH*MAX_SUPPORTED_SCALED_HEIGHT))
		{
			X = X*3;
			Y = Y*2;
			context->m_iScaledWidth = context->m_iOriginalHeight * Y / X;
			context->m_iScaledHeight = context->m_iOriginalWidth * Y / X;
			context->m_iScaledWidth -= (context->m_iScaledWidth&0x1);
			context->m_iScaledHeight -= (context->m_iScaledHeight&0x1);
		}
	}
	RetStatus = TRUE;

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG(" context->m_iRotationDegree[%d] X[%d] Y[%d] context->m_iOriginalWidth[%d] context->m_iOriginalHeight[%d] context->m_iScaledWidth[%d] context->m_iScaledHeight[%d]  ",context->m_iRotationDegree,X,Y,context->m_iOriginalWidth,context->m_iOriginalHeight,context->m_iScaledWidth,context->m_iScaledHeight);
		GST_LOG("t_CalculateTargetDimention ------- ");
	}
	return RetStatus;
}


static void t_ConvertYUYV(struct VideoFrameRotateContext* context)
{
	unsigned char *yPtr0,*yPtr1,*uvPtr,*out0,*out1;
	int nWidth = 0;
	int width = 0;
	int height = 0;
	int h = 0;
	int w = 0;

	width = context->m_iTargetScaledWidth; //context->m_iScaledWidth;
	height = context->m_iScaledHeight;

	yPtr0 = context->m_pGAScaledYBuffer;
	yPtr1 = context->m_pGAScaledYBuffer + width;
	uvPtr = context->m_pGAScaledCbCrBuffer;

	out0 = context->m_pBackBuffer;
	if (width % TILE_W_SIZE)
	{
		out1 = context->m_pBackBuffer + width*2 + 2*(TILE_W_SIZE - (width % TILE_W_SIZE));
		nWidth = (width*2) + 4*(TILE_W_SIZE - (width % TILE_W_SIZE));
	}
	else
	{
		out1 = context->m_pBackBuffer + width*2;
		nWidth = width*2;
	}

	for (h=0; h<height/2; h++)
	{
		for (w=0; w<width/2; w++)
		{
			*out0++ = *yPtr0++;  //Y1
			*out0++ = *uvPtr;      //U

			*out1++ = *yPtr1++; //Y3
			*out1++ = *uvPtr++; //U

			*out0++ = *yPtr0++; //Y2
			*out0++ = *uvPtr;   //V

			*out1++ = *yPtr1++;  //4//Y4
			*out1++ = *uvPtr++;  //V
		}

		yPtr0 += width;
		yPtr1 += width;

		out0 += nWidth;
		out1 += nWidth;
	}

}

static void t_FlushCache(struct VideoFrameRotateContext* context)
{
#ifdef SUPPORT_SW_VIDEO_DECODER

	if (context->m_iRotationGAScalledBuffer_GFX1 == 1)
	{
		uint32_t SdRet = SD_OK;
		SdGfx_Status_t Status;
		SdGfx_Settings_t Settings;
		SdRect_t Rect;
		memset(&Status, 0x0, sizeof(SdGfx_Status_t));
		memset(&Rect, 0x0, sizeof(SdRect_t));
		memset(&Settings, 0x0, sizeof(Settings));

		SdRet = SdGfx_GetStatus(SD_GFX1, SD_GFX_STATUS_PLANE_INFO, &Status);
		if(SdRet != SD_OK)
		{
			GST_LOG("SdGfx_GetStatus failed ");
			return;
		}

		SdRet = SdGfx_Get(SD_GFX1, &Settings);
		if(SdRet != SD_OK)
		{
			GST_LOG("SdGfx_Get failed ");
			return;
		}

		Rect.x = 0;
		Rect.y = 0;
		Rect.w = Settings.sScaleData.hResolution;
		Rect.h = Settings.sScaleData.vResolution;

		SdRet = SdGfx_CacheFlush(Status.GfxInfo.sPlane.pBackBuffer[0], &Rect);
		if(SdRet != SD_OK)
		{
			GST_LOG("SdGfx_CacheFlush failed ");
			return;
		}
	}
	else
	{
		uint32_t SdRet = SD_OK;
		SdRect_t Rect;
		memset(&Rect, 0x0, sizeof(SdRect_t));

		Rect.x = 0;
		Rect.y = 0;
		Rect.w = context->m_ihResolution;
		Rect.h = context->m_ivResolution;

		SdRet = SdGfx_CacheFlush((unsigned int *)((void *)context->m_pGAMemory), &Rect);
		if(SdRet != SD_OK)
		{
			GST_LOG("SdGfx_CacheFlush failed ");
			return;
		}
	}

#endif //SUPPORT_SW_VIDEO_DECODER
}

static gboolean t_Rotate_90_270_Thread(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *ty1, int *ty2, int* ty3, int *tuv1, int *tuv2, int *tuv3)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* dstScaleYBuffer = p->dstY;
	unsigned char* dstScaleCbCrBuffer = p->dstCbCr;
	int dstScaledWidth = p->dstw;
	int dstWc = dstScaledWidth/2;
	int sy = p->starty;
	int ey = p->endy;
	int syc = p->starty_c;
	int eyc = p->endy_c;

	int x = 0;
	int y = 0;
	int dstY = 0;
	int srcY = 0;
	unsigned short *srcCbCr = (unsigned short *)((void *)srcCbCrBuffer);
	unsigned short *dstCbCr = (unsigned short *)((void *)dstScaleCbCrBuffer);

	if(!ty1 || !ty2 || !ty3 || !tuv1 || !tuv2 || !tuv3 )
	{
		GST_LOG("!!!!!!!!!!!!!!!!!  TABLES ARE NOT INITIALZED !!!!!!!!!!!!!!! ");
		return FALSE;
	}
	ey++;
	eyc++;

	// Y
	if(context->m_iRotationXY)
	{
		for(y=sy; y<ey; y++)
		{
			dstY =ty1[y];
			srcY = ty3[y];
			for(x=0; x<dstScaledWidth; x++)
			{
				dstScaleYBuffer[dstY + x] = srcYBuffer[ty2[x] + srcY];
			}
		}
	}
	else if(context->m_iRotationYRaise)
	{
		for(x=0; x<dstScaledWidth; x++)
		{
			dstY= ty2[x];
			for(y=sy; y<ey; y++)
			{
				dstScaleYBuffer[ty1[y] + x] = srcYBuffer[dstY+ ty3[y]];
			}
		}
	}
	else //context->m_iRotationYRaise = 0
	{
		sy--;
		ey--;
		for(x=0; x<dstScaledWidth; x++)
		{
			dstY= ty2[x];
			for(y=ey; y>sy; y--)
			{
				dstScaleYBuffer[ty1[y] + x] = srcYBuffer[dstY+ ty3[y]];
			}
		}
	}

	// UV
	if(context->m_iRotationXY)
	{
		for(y=syc; y<eyc; y++)
		{
			dstY = tuv1[y];
			srcY = tuv3[y];
			for(x=0; x<(dstScaledWidth>>1); x++)
			{
				dstCbCr[dstY + x] = srcCbCr[tuv2[x] + srcY];
			}
		}
	}
	else if(context->m_iRotationYRaise)
	{
		for(x=0; x<dstWc; x++)
		{
			dstY = tuv2[x];
			for(y=syc; y<eyc; y++)
			{
				dstCbCr[tuv1[y] + x] = srcCbCr[dstY+tuv3[y]];
			}
		}
	}
       else //context->m_iRotationYRaise = 0
	{
	    syc--;
		eyc--;
		for(x=0; x<dstWc; x++)
		{
			dstY = tuv2[x];
			for(y=eyc; y>syc; y--)
			{
				dstCbCr[tuv1[y] + x] = srcCbCr[dstY+tuv3[y]];
			}
		}
	}
	return TRUE;
}

//For Interlaced scan type low resolution
static gboolean t_Rotate_90_270_Thread_for_interscan(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *ty1, int *ty2, int* ty3, int *tuv1, int *tuv2, int *tuv3)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* dstScaleYBuffer = p->dstY;
	unsigned char* dstScaleCbCrBuffer = p->dstCbCr;
	int dstScaledWidth = p->dstw;
	int dstWc = dstScaledWidth/2;
	int sy = p->starty;
	int ey = p->endy;
	int syc = p->starty_c;
	int eyc = p->endy_c;

	int x = 0;
	int y = 0;
	int dstY = 0;
	int dstY1 = 0;
	int srcY = 0;
	unsigned short *srcCbCr = (unsigned short *)((void *)srcCbCrBuffer);
	unsigned short *dstCbCr = (unsigned short *)((void *)dstScaleCbCrBuffer);

	int srcLine = p->srcLine;
	int srcLineC = p->srcLine>>1;
	int maxdstY1 ;

	unsigned short tmpCb1, tmpCr1, tmpCb2, tmpCr2;

	if(!ty1 || !ty2 || !ty3 || !tuv1 || !tuv2 || !tuv3 )
	{
		GST_LOG("!!!!!!!!!!!!!!!!!  TABLES ARE NOT INITIALZED !!!!!!!!!!!!!!! ");
		return FALSE;
	}
	ey++;
	eyc++;

	maxdstY1 = srcLine * (p->srch-1);
	
	// Y
	if(context->m_iRotationXY)
	{
		for(y=sy; y<ey; y++)
		{
			dstY =ty1[y];
			srcY = ty3[y];
			for(x=0; x<dstScaledWidth; x++)
			{
				dstScaleYBuffer[dstY + x] = srcYBuffer[ty2[x] + srcY] ;
			}
		}
	}
	else if(context->m_iRotationYRaise)
	{
		for(x=0; x<dstScaledWidth; x++)
		{
			dstY= ty2[x];
			dstY1 = dstY + srcLine;
			dstY1 = FFMAX(0, FFMIN(dstY1, maxdstY1));
			for(y=sy; y<ey; y++)
			{
				dstScaleYBuffer[ty1[y] + x] = (srcYBuffer[dstY+ ty3[y]] + srcYBuffer[dstY1 + ty3[y]])>>1;
			}
		}
	}
	else //context->m_iRotationYRaise = 0
	{
		sy--;
		ey--;
		for(x=0; x<dstScaledWidth; x++)
		{
			dstY= ty2[x];
			dstY1 = dstY + srcLine;
			dstY1 = FFMAX(0, FFMIN(dstY1, maxdstY1));
			for(y=ey; y>sy; y--)
			{
				dstScaleYBuffer[ty1[y] + x] = (srcYBuffer[dstY+ ty3[y]] + srcYBuffer[dstY1 + ty3[y]])>>1;
			}
		}
	}

	maxdstY1 = srcLineC * ((p->srch>>1)-1);

	// UV
	if(context->m_iRotationXY)
	{
		for(y=syc; y<eyc; y++)
		{
			dstY = tuv1[y];
			srcY = tuv3[y];
			for(x=0; x<(dstScaledWidth>>1); x++)
			{
				dstCbCr[dstY + x] = srcCbCr[tuv2[x] + srcY];
			}
		}
	}
	else if(context->m_iRotationYRaise)
	{
		for(x=0; x<dstWc; x++)
		{
			dstY = tuv2[x];
			dstY1 = dstY + srcLineC;
			dstY1 = FFMAX(0, FFMIN(dstY1, maxdstY1));
			for(y=syc; y<eyc; y++)
			{
				tmpCb1 = srcCbCr[dstY+tuv3[y]]&0xff;
				tmpCr1 = (srcCbCr[dstY+tuv3[y]]>>8)&0xff;
				tmpCb2 = srcCbCr[dstY1+tuv3[y]]&0xff;
				tmpCr2 = (srcCbCr[dstY1+tuv3[y]]>>8)&0xff;
				tmpCb1 = (tmpCb1+tmpCb2)>>1;
				tmpCr1 = (tmpCr1+tmpCr2)>>1;
				dstCbCr[tuv1[y] + x] = (tmpCr1<<8) + tmpCb1;
			}
		}
	}
    else //context->m_iRotationYRaise = 0
	{
		syc--;
		eyc--;
		for(x=0; x<dstWc; x++)
		{
			dstY = tuv2[x];
			dstY1 = dstY + srcLineC;
			dstY1 = FFMAX(0, FFMIN(dstY1, maxdstY1));
			for(y=eyc; y>syc; y--)
			{
				tmpCb1 = srcCbCr[dstY+tuv3[y]]&0xff;
				tmpCr1 = (srcCbCr[dstY+tuv3[y]]>>8)&0xff;
				tmpCb2 = srcCbCr[dstY1+tuv3[y]]&0xff;
				tmpCr2 = (srcCbCr[dstY1+tuv3[y]]>>8)&0xff;
				tmpCb1 = (tmpCb1+tmpCb2)>>1;
				tmpCr1 = (tmpCr1+tmpCr2)>>1;
				dstCbCr[tuv1[y] + x] = (tmpCr1<<8) + tmpCb1;
			}
		}
	}
	return TRUE;
}

static gboolean t_Rotate_180_Thread_Opt(struct VideoFrameRotateContext* context, struct PerThreadContext* p)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* dstScaleYBuffer = p->dstY;
	unsigned char* dstScaleCbCrBuffer = p->dstCbCr;
	int dstScaledWidth = p->dstw;
	int dstScaledHeight = p->dsth;
	int srcLine = p->srcLine;
	int sy = p->starty;
	int ey = p->endy;
	int syc = p->starty_c;
	int eyc = p->endy_c;

	int iDenom = 0;
	if (context->m_iSkipFactor == 3)
	{
		iDenom = 3;
	}
	else if (context->m_iSkipFactor == 2)
	{
		iDenom = 4;
	}
	else
		iDenom = 1;

 /*Y and CbCr are copied in separate threads without tables*/

	int x, y;
	int dstuvw, dstuvh;
	unsigned char* srcy = NULL;
	unsigned char* dsty = NULL;
	unsigned short *srcCbCr = NULL;
	unsigned short *dstCbCr = NULL;

	ey++;
	eyc++;

	//Y
	srcy = srcYBuffer+sy*srcLine;
	//dsty = dstScaleYBuffer+(dstScaledHeight-sy)*dstScaledWidth-1;
	dsty = dstScaleYBuffer+(context->m_iTargetScaledHeight-(sy*2/iDenom))*context->m_iTargetScaledWidth-1;

	if (context->m_iSkipFactor == 3)
	{
		for(y=sy; y<ey; y+=3)
		{
			for(x=0; x<dstScaledWidth; x+=3)
			{
				*dsty--= srcy[x];
				*dsty--= srcy[x+1];
				//skip
				//*dsty--= srcy[x+2];
			}
			srcy+=srcLine;
			for(x=0; x<dstScaledWidth; x+=3)
			{
				*dsty--= srcy[x];
				*dsty--= srcy[x+1];
				//skip
				//*dsty--= srcy[x+2];
			}
			srcy+=srcLine;
			// Skip Row
			srcy+=srcLine;
		}
	}
	else if (context->m_iSkipFactor == 2)
	{
		for(y=sy; y<ey; y+=2)
		{
			for(x=0; x<dstScaledWidth; x+=2)
			{
				*dsty--= srcy[x];
			}
			srcy+=srcLine;
			// Skip Row
			srcy+=srcLine;
		}
	}

	// UV
	dstuvw = dstScaledWidth/2;
	dstuvh = dstScaledHeight/2;

	srcCbCrBuffer += srcLine * syc;
	//dstScaleCbCrBuffer += dstScaledWidth * (dstuvh-syc)-2;
	dstScaleCbCrBuffer += context->m_iTargetScaledWidth * ((context->m_iTargetScaledHeight/2)-(syc*2/iDenom))-2;
	srcCbCr = (unsigned short*)((void *)srcCbCrBuffer);
	dstCbCr = (unsigned short*)((void *)dstScaleCbCrBuffer);

	if (context->m_iSkipFactor == 3)
	{
		for(y=syc; y<eyc; y+=3)
		{
			for(x=0; x<dstuvw; x+=3)
			{
				*dstCbCr--= srcCbCr[x];
				*dstCbCr--= srcCbCr[x+1];
				//skip
				//*dstCbCr--= srcCbCr[x+2];
			}
			srcCbCr+=srcLine/2;
			for(x=0; x<dstuvw; x+=3)
			{
				*dstCbCr--= srcCbCr[x];
				*dstCbCr--= srcCbCr[x+1];
				//skip
				//*dstCbCr--= srcCbCr[x+2];
			}
			srcCbCr+=srcLine/2;
			// Skip Row
			srcCbCr+=srcLine/2;
		}
	}
	else if(context->m_iSkipFactor == 2)
	{
		for(y=syc; y<eyc; y+=2)
		{
			for(x=0; x<dstuvw; x+=2)
			{
				*dstCbCr--= srcCbCr[x];
			}
			srcCbCr+=srcLine/2;
			// Skip Row
			srcCbCr+=srcLine/2;
		}
	}

	return TRUE;
}


static gboolean t_Rotate_180_Thread(struct VideoFrameRotateContext* context, struct PerThreadContext* p)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* dstScaleYBuffer = p->dstY;
	unsigned char* dstScaleCbCrBuffer = p->dstCbCr;
	int dstScaledWidth = p->dstw;
	int dstScaledHeight = p->dsth;
	int srcLine = p->srcLine;
	int sy = p->starty;
	int ey = p->endy;
	int syc = p->starty_c;
	int eyc = p->endy_c;

	/*
	GST_LOG("t_Rotate_180_Thread: m_iOriginalWidth/m_iOriginalHeight: [%d]/[%d], m_iScaledWidth/m_iScaledHeight: [%d]/[%d], m_iScaledWidth_90_270/m_iScaledHeight_90_270: [%d]/[%d], m_iTableWidth_90_270/m_iTableHeight_90_270: [%d]/[%d], m_iTargetScaledWidth/m_iTargetScaledHeight: [%d]/[%d] , dstScaledWidth/dstScaledHeight: [%d]/[%d], srcYBuffer[0x%x], srcCbCrBuffer[0x%x], dstScaleYBuffer[0x%x], dstScaleCbCrBuffer[0x%x] ",
		context->m_iOriginalWidth, context->m_iOriginalHeight,
		context->m_iScaledWidth, context->m_iScaledHeight,
		context->m_iScaledWidth_90_270, context->m_iScaledHeight_90_270,
		context->m_iTableWidth_90_270, context->m_iTableHeight_90_270,
		context->m_iTargetScaledWidth, context->m_iTargetScaledHeight,
		dstScaledWidth, dstScaledHeight,
		srcYBuffer, srcCbCrBuffer, dstScaleYBuffer, dstScaleCbCrBuffer);
	*/

 /*Y and CbCr are copied in separate threads without tables*/

	int x, y;
	int dstuvw, dstuvh;
	unsigned char* srcy = NULL;
	unsigned char* dsty = NULL;
	unsigned short *srcCbCr = NULL;
	unsigned short *dstCbCr = NULL;

	ey++;
	eyc++;

	//Y
	srcy = srcYBuffer+sy*srcLine;
	dsty = dstScaleYBuffer+(dstScaledHeight-sy)*dstScaledWidth-1;

	if (context->m_bIsInterlacedScanType == TRUE)
	{
		if(sy&0x1){
			srcy -= srcLine;
			for(x=0; x<dstScaledWidth; x++)
			{
				*dsty--= srcy[x];
			}
			srcy += 2*srcLine;
		}
		sy = sy + (sy&0x1);
		for(y=sy; y<ey-2; y+=2)
		{
			for(x=0; x<dstScaledWidth; x++)
			{
				*dsty--= srcy[x];
			}
			for(x=0; x<dstScaledWidth; x++)
			{
				*dsty--= srcy[x];
			}
			srcy += 2*srcLine;
		}
		if(y+1<ey)
		{
			for(x=0; x<dstScaledWidth; x++)
			{
				*dsty--= srcy[x];
			}
			for(x=0; x<dstScaledWidth; x++)
			{
				*dsty--= srcy[x];
			}
		}else{
			for(x=0; x<dstScaledWidth; x++)
			{
				*dsty--= srcy[x];
			}
		}
	}
	else
	{
		for(y=sy; y<ey; y++)
		{
			for(x=0; x<dstScaledWidth; x++)
			{
				*dsty--= srcy[x];
			}
			srcy+=srcLine;
		}
	}

	// UV
	dstuvw = dstScaledWidth>>1;
	dstuvh = dstScaledHeight>>1;

	srcCbCrBuffer += srcLine * syc;
	dstScaleCbCrBuffer += dstScaledWidth * (dstuvh-syc)-2;
	srcCbCr = (unsigned short*)((void *)srcCbCrBuffer);
	dstCbCr = (unsigned short*)((void *)dstScaleCbCrBuffer);

	if (context->m_bIsInterlacedScanType == TRUE)
	{
		if(syc&0x1){
			srcCbCr -= srcLine/2;
			for(x=0; x<dstuvw; x++)
			{
				*dstCbCr--= srcCbCr[x];
			}
			srcCbCr+=srcLine;
		}
		syc = syc + (syc&0x1);
		for(y=syc; y<eyc-2; y+=2)
		{
			for(x=0; x<dstuvw; x++)
			{
				*dstCbCr--= srcCbCr[x];
			}
			for(x=0; x<dstuvw; x++)
			{
				*dstCbCr--= srcCbCr[x];
			}
			srcCbCr+=srcLine;
		}
		if(y+1<eyc)
		{
			for(x=0; x<dstuvw; x++)
			{
				*dstCbCr--= srcCbCr[x];
			}
			for(x=0; x<dstuvw; x++)
			{
				*dstCbCr--= srcCbCr[x];
			}
		}else{
			for(x=0; x<dstuvw; x++)
			{
				*dstCbCr--= srcCbCr[x];
			}
		}
	}
	else
	{
		for(y=syc; y<eyc; y++)
		{
			for(x=0; x<dstuvw; x++)
			{
				*dstCbCr--= srcCbCr[x];
			}
			srcCbCr+=srcLine/2;
		}
	}

	return TRUE;
}

static gboolean t_Rotate_0_Thread(struct VideoFrameRotateContext* context, struct PerThreadContext* p)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* dstScaleYBuffer = p->dstY;
	unsigned char* dstScaleCbCrBuffer = p->dstCbCr;
	int dstScaledWidth = p->dstw;
	int dstScaledHeight = p->dsth;
	int srcLine = p->srcLine;
	int sy = p->starty;
	int ey = p->endy;
	int syc = p->starty_c;
	int eyc = p->endy_c;

	/*
	GST_LOG("t_Rotate_180_Thread: m_iOriginalWidth/m_iOriginalHeight: [%d]/[%d], m_iScaledWidth/m_iScaledHeight: [%d]/[%d], m_iScaledWidth_90_270/m_iScaledHeight_90_270: [%d]/[%d], m_iTableWidth_90_270/m_iTableHeight_90_270: [%d]/[%d], m_iTargetScaledWidth/m_iTargetScaledHeight: [%d]/[%d] , dstScaledWidth/dstScaledHeight: [%d]/[%d], srcYBuffer[0x%x], srcCbCrBuffer[0x%x], dstScaleYBuffer[0x%x], dstScaleCbCrBuffer[0x%x] ",
		context->m_iOriginalWidth, context->m_iOriginalHeight,
		context->m_iScaledWidth, context->m_iScaledHeight,
		context->m_iScaledWidth_90_270, context->m_iScaledHeight_90_270,
		context->m_iTableWidth_90_270, context->m_iTableHeight_90_270,
		context->m_iTargetScaledWidth, context->m_iTargetScaledHeight,
		dstScaledWidth, dstScaledHeight,
		srcYBuffer, srcCbCrBuffer, dstScaleYBuffer, dstScaleCbCrBuffer);
	*/

 /*Y and CbCr are copied in separate threads without tables*/

	int x, y;
	int dstuvw, dstuvh;
	unsigned char* srcy = NULL;
	unsigned char* dsty = NULL;
	unsigned short *srcCbCr = NULL;
	unsigned short *dstCbCr = NULL;

	ey++;
	eyc++;

	//Y
	srcy = srcYBuffer+sy*srcLine;
	dsty = dstScaleYBuffer + sy*dstScaledWidth;

	for(y=sy; y<ey; y++)
	{
		for(x=0; x<dstScaledWidth; x++)
		{
			*dsty++ = srcy[x];
		}
		srcy+=srcLine;
	}

	// UV
	dstuvw = dstScaledWidth>>1;
	dstuvh = dstScaledHeight>>1;

	srcCbCrBuffer += srcLine * syc;
	dstScaleCbCrBuffer += syc*dstScaledWidth;
	srcCbCr = (unsigned short*)((void *)srcCbCrBuffer);
	dstCbCr = (unsigned short*)((void *)dstScaleCbCrBuffer);

	for(y=syc; y<eyc; y++)
	{
		for(x=0; x<dstuvw; x++)
		{
			*dstCbCr++= srcCbCr[x];
		}
		srcCbCr+=srcLine/2;
	}

	return TRUE;
}


static gboolean t_Rotate_180_Thread_for_interscan(struct PerThreadContext* p)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* dstScaleYBuffer = p->dstY;
	unsigned char* dstScaleCbCrBuffer = p->dstCbCr;
	int dstScaledWidth = p->dstw;
	int dstScaledHeight = p->dsth;
	int srcLine = p->srcLine;
	int srcLineC = (srcLine>>1);
	int sy = p->starty;
	int ey = p->endy;
	int syc = p->starty_c;
	int eyc = p->endy_c;

 /*Y and CbCr are copied in separate threads without tables*/

	int x, y;
	int dstuvw, dstuvh;
	int maxdstY1, dstY, dstY1;
	unsigned char* dsty = NULL;
	unsigned short *srcCbCr = NULL;
	unsigned short *dstCbCr = NULL;

	unsigned short tmpCb1, tmpCr1, tmpCb2, tmpCr2;

	ey++;
	eyc++;

	//Y
	dsty = dstScaleYBuffer+(dstScaledHeight-sy)*dstScaledWidth-1;

	maxdstY1 = srcLine * (p->srch-1);
	dstY = sy*srcLine;

	for(y=sy; y<ey; y++)
	{
		dstY1 = dstY + srcLine;
		dstY1 = FFMAX(0, FFMIN(dstY1, maxdstY1));
		for(x=0; x<dstScaledWidth; x++)
		{
			*dsty--= (srcYBuffer[dstY + x] + srcYBuffer[dstY1 + x] + 1)>>1;
		}
		dstY+=srcLine;
	}


	// UV
	dstuvw = dstScaledWidth>>1;
	dstuvh = dstScaledHeight>>1;

	dstScaleCbCrBuffer += dstScaledWidth * (dstuvh-syc)-2;
	srcCbCr = (unsigned short*)((void *)srcCbCrBuffer);
	dstCbCr = (unsigned short*)((void *)dstScaleCbCrBuffer);

	maxdstY1 = srcLineC * ((p->srch>>1)-1);
	dstY = syc*srcLineC;

	for(y=syc; y<eyc; y++)
	{
		dstY1 = dstY + srcLineC;
		dstY1 = FFMAX(0, FFMIN(dstY1, maxdstY1));
		for(x=0; x<dstuvw; x++)
		{
			tmpCb1 = srcCbCr[dstY+x]&0xff;
			tmpCr1 = (srcCbCr[dstY+x]>>8)&0xff;
			tmpCb2 = srcCbCr[dstY1+x]&0xff;
			tmpCr2 = (srcCbCr[dstY1+x]>>8)&0xff;
			tmpCb1 = (tmpCb1+tmpCb2)>>1;
			tmpCr1 = (tmpCr1+tmpCr2)>>1;
			*dstCbCr-- = (tmpCr1<<8) + tmpCb1;
		}
		dstY+=srcLineC;
	}

	return TRUE;
}

static gboolean t_Rotate_90_270_Tiled_Thread(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *table_tiled_y, int *table_tiled_c)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* dstScaleYBuffer = p->dstY;
	unsigned char* dstScaleCbCrBuffer = p->dstCbCr;

	int dstScaledWidth = p->dstw;
	int dstWc = dstScaledWidth>>1;
	int sy = p->starty;
	int ey = p->endy;
	int syc = p->starty_c;
	int eyc = p->endy_c;

	context->m_iTargetScaledWidth = dstScaledWidth;

	int dstScaledHeight = p->dsth;
	int x = 0;
	int y = 0;
	int pos = 0;
	int idx_tmp = 0;
	int prev_pos = 0;
	int pos1 = 0;
	unsigned long long tmp64BitReg = 0;

	unsigned short *srcCbCr = (unsigned short *)((void *)srcCbCrBuffer);
	unsigned short *dstCbCr = (unsigned short *)((void *)dstScaleCbCrBuffer);

	if(!table_tiled_y || !table_tiled_c )
	{
		GST_LOG("!!!!!!!!!!!!!!!!!  TABLES ARE NOT INITIALZED !!!!!!!!!!!!!!! ");
		return FALSE;
	}
	ey++;
	eyc++;


	// Y
	unsigned long long *srcY64bitPtr = (unsigned long long *)((void *)srcYBuffer);
	int *pTailedYPtr = NULL;
	unsigned char *outY;

	pTailedYPtr = table_tiled_y + sy;

	for(x=0; x<dstScaledWidth; x++)
	{
		outY = dstScaleYBuffer + x;

		for(y=sy; y<ey; y++)
		{
			pos1 = *pTailedYPtr++;
			pos = (pos1 & 0x1FFFF8)>>3;
			idx_tmp = (pos1 % 8)<<3; // left shit by 3 is to right shifted bytes idx_tmp<<3 from 64bit tmp64BitReg

			if(prev_pos != pos || (pos == 0 && prev_pos == 0))
			{
				tmp64BitReg = srcY64bitPtr[pos];
			}

			if (idx_tmp == 0)
			{
				outY[y*dstScaledWidth] = (unsigned char)(tmp64BitReg);
			}
			else
			{
				outY[y*dstScaledWidth] = (unsigned char)(tmp64BitReg>>idx_tmp);
			}

			prev_pos = pos;
		}

		pTailedYPtr += dstScaledHeight-(ey-sy);
	}

	// UV
	pos = 0;
	pos1 = 0;
	idx_tmp = 0;
	prev_pos = 0;
	int *pTailedCbCrPtr = NULL;
	unsigned long long *srcCbCr64bitPtr = (unsigned long long *)((void *)srcCbCr);
	unsigned short *outCbCr;

	pTailedCbCrPtr = table_tiled_c + syc;

	for(x=0; x<dstWc; x++)
	{
		outCbCr = dstCbCr + x;

		for(y=syc; y<eyc; y++)
		{
			pos1 = *pTailedCbCrPtr++;
			pos = (pos1 & 0x1FFFF8)>>3;
			idx_tmp = (pos1 % 8)<<3; 

			if((prev_pos != pos || (pos == 0 && prev_pos == 0)))
			{
				tmp64BitReg = srcCbCr64bitPtr[pos];
			}

			if (idx_tmp == 0)
				outCbCr[y*dstWc] = (unsigned short)(tmp64BitReg);
			else
				outCbCr[y*dstWc] = (unsigned short)(tmp64BitReg>>idx_tmp);

			prev_pos = pos;
		}

		pTailedCbCrPtr += (dstScaledHeight/2)-(eyc-syc);

	}

	
	return TRUE;
}

static gboolean t_Rotate_90_Tiled_Thread_Opt(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *table_tiled_y, int *table_tiled_c)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* targetYUYVBuffer = p->targetYUYV;
	int dstScaledWidth = p->dstw;
	int sy = p->starty;
	int ey = p->endy;
	int eyc = p->endy_c;

	int table_offset = p->RotatedTableOffset_90_Y;//(sy*240)/1080;  // Need to generalize for other resolutions
	int table_offset_UV = p->RotatedTableOffset_90_UV;
	int table_offset_Increment = p->RotatedTableOffsetIncrement_90;

	dstScaledWidth -= TILE_W_SIZE;
	context->m_iTargetScaledWidth = dstScaledWidth;
	if (dstScaledWidth%TILE_W_SIZE)
	{
		dstScaledWidth = (dstScaledWidth/TILE_W_SIZE)*TILE_W_SIZE;
		context->m_iTargetScaledWidth = dstScaledWidth;
	}

	int x = 0;
	int y = 0;
	unsigned short *srcCbCr = (unsigned short *)((void *)srcCbCrBuffer);

	if(!table_tiled_y || !table_tiled_c )
	{
		GST_LOG("!!!!!!!!!!!!!!!!!  TABLES ARE NOT INITIALZED !!!!!!!!!!!!!!! ");
		return FALSE;
	}
	ey++;
	eyc++;

	// Y
	unsigned char *outY = NULL;
	int *pTailedYPtr1 = NULL;
	int *pTailedYPtr2 = NULL;
	int *pTailedCbCrPtr = NULL;
	unsigned long long tmp64BitRegY1 = 0;
	unsigned long long tmp64BitRegY2 = 0;
	unsigned long long tmp64BitRegCbCr = 0;
	int posY1 = 0;
	int posY2 = 0;
	int posCbCr = 0;

	pTailedYPtr1 = table_tiled_y + table_offset;
	pTailedYPtr2 = pTailedYPtr1 + table_offset_Increment;
	pTailedCbCrPtr = table_tiled_c + table_offset_UV;

	unsigned long long *srcY64bitPtr = (unsigned long long *)((void *)srcYBuffer);
	unsigned long long *srcCbCr64bitPtr = (unsigned long long *)((void *)srcCbCr);

	int nWidth = dstScaledWidth*2;
	if (dstScaledWidth % TILE_W_SIZE)
	{
		nWidth += 2*(TILE_W_SIZE -(dstScaledWidth % TILE_W_SIZE));
	}

	int delta =ey-sy;

	for(x=0; x< dstScaledWidth/2; x++)
	{
		outY = targetYUYVBuffer + (x*4);
		unsigned char *dst = outY + sy*nWidth;

		for(y=0; y<delta; y+=18)
		{
			///1st part : Y1  : 1st Read
			posY1 = *pTailedYPtr1++;
			tmp64BitRegY1 = srcY64bitPtr[posY1];
			unsigned int lsbY1 = (unsigned int)tmp64BitRegY1;
			unsigned int msbY1 = (unsigned int)(tmp64BitRegY1>>32);
			// UV : 1st Read
			posCbCr = *pTailedCbCrPtr++;
			tmp64BitRegCbCr = srcCbCr64bitPtr[posCbCr];
			unsigned int lsbCbCr = (unsigned int)tmp64BitRegCbCr;
			unsigned int msbCbCr = (unsigned int)(tmp64BitRegCbCr>>32);
			//Y2
			posY2 = *pTailedYPtr2++;
			tmp64BitRegY2 = srcY64bitPtr[posY2];
			unsigned int lsbY2 = (unsigned int)tmp64BitRegY2;
			unsigned int msbY2 = (unsigned int)(tmp64BitRegY2>>32);

			//0-Y1 0-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1);      //Y1
			dst[1] = (unsigned char)(lsbCbCr);    //U
			dst[2] = (unsigned char)(lsbY2);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8); //V
			dst += nWidth;
			///1-Y1 1-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1>>8);    //Y1
			dst[1] = (unsigned char)(lsbCbCr);     //U
			dst[2] = (unsigned char)(lsbY2>>8);    //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8);  //V
			dst += nWidth;
			///3-Y1 3-Y2 2-U 2-V
			dst[0] = (unsigned char)(lsbY1>>24);      //Y1
			dst[1] = (unsigned char)(lsbCbCr>>16);    //U
			dst[2] = (unsigned char)(lsbY2>>24);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>24);    //V
			dst += nWidth;
			//5-Y1 5-Y2 2-U 2-V
			dst[0] = (unsigned char)(msbY1>>8);      //Y1
			dst[1] = (unsigned char)(lsbCbCr>>16);    //U
			dst[2] = (unsigned char)(msbY2>>8);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>24);    //V
			dst += nWidth;
			//7-Y1 7-Y2 6-U 6-V
			dst[0] = (unsigned char)(msbY1>>24);      //Y1
			dst[1] = (unsigned char)(msbCbCr>>16);    //U
			dst[2] = (unsigned char)(msbY2>>24);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>24);    //V
			dst += nWidth;

			///1st part : Y1  : 2nd Read
			posY1 = *pTailedYPtr1++;
			tmp64BitRegY1 = srcY64bitPtr[posY1];
			lsbY1 = (unsigned int)tmp64BitRegY1;
			msbY1 = (unsigned int)(tmp64BitRegY1>>32);
			//Y2
			posY2 = *pTailedYPtr2++;
			tmp64BitRegY2 = srcY64bitPtr[posY2];
			lsbY2 = (unsigned int)tmp64BitRegY2;
			msbY2 = (unsigned int)(tmp64BitRegY2>>32);

			//0-Y1 0-Y2 6-U 6-V
			dst[0] = (unsigned char)(lsbY1);          //Y1
			dst[1] = (unsigned char)(msbCbCr>>16);    //U
			dst[2] = (unsigned char)(lsbY2);          //Y2
			dst[3] = (unsigned char)(msbCbCr>>24);    //V
			dst += nWidth;

			// UV : 2nd Read
			posCbCr = *pTailedCbCrPtr++;
			tmp64BitRegCbCr = srcCbCr64bitPtr[posCbCr];
			lsbCbCr = (unsigned int)tmp64BitRegCbCr;
			msbCbCr = (unsigned int)(tmp64BitRegCbCr>>32);

			///2-Y1 2-Y2 2-U 2-V
			dst[0] = (unsigned char)(lsbY1>>16);      //Y1
			dst[1] = (unsigned char)(lsbCbCr>>16);    //U
			dst[2] = (unsigned char)(lsbY2>>16);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>24);    //V
			dst += nWidth;
			///4-Y1 4-Y2 2-U 2-V
			dst[0] = (unsigned char)(msbY1);          //Y1
			dst[1] = (unsigned char)(lsbCbCr>>16);    //U
			dst[2] = (unsigned char)(msbY2);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>24);    //V
			dst += nWidth;
			//6-Y1 6-Y2 6-U 6-V
			dst[0] = (unsigned char)(msbY1>>16);       //Y1
			dst[1] = (unsigned char)(msbCbCr>>16);    //U
			dst[2] = (unsigned char)(msbY2>>16);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>24);    //V
			dst += nWidth;

			///2nd part : Y1  : 1st Read
			posY1 = *pTailedYPtr1++;
			tmp64BitRegY1 = srcY64bitPtr[posY1];
			lsbY1 = (unsigned int)tmp64BitRegY1;
			msbY1 = (unsigned int)(tmp64BitRegY1>>32);
			//Y2
			posY2 = *pTailedYPtr2++;
			tmp64BitRegY2 = srcY64bitPtr[posY2];
			lsbY2 = (unsigned int)tmp64BitRegY2;
			msbY2 = (unsigned int)(tmp64BitRegY2>>32);

			//0-Y1 0-Y2 6-U 6-V
			dst[0] = (unsigned char)(lsbY1);      //Y1
			dst[1] = (unsigned char)(msbCbCr>>16);    //U
			dst[2] = (unsigned char)(lsbY2);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>24); //V
			dst += nWidth;

			// UV : 3rd Read
			posCbCr = *pTailedCbCrPtr++;
			tmp64BitRegCbCr = srcCbCr64bitPtr[posCbCr];
			lsbCbCr = (unsigned int)tmp64BitRegCbCr;
			msbCbCr = (unsigned int)(tmp64BitRegCbCr>>32);

			///1-Y1 1-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1>>8);    //Y1
			dst[1] = (unsigned char)(lsbCbCr);     //U
			dst[2] = (unsigned char)(lsbY2>>8);    //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8);  //V
			dst += nWidth;
			///3-Y1 3-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1>>24);      //Y1
			dst[1] = (unsigned char)(lsbCbCr);        //U
			dst[2] = (unsigned char)(lsbY2>>24);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8);     //V
			dst += nWidth;
			//5-Y1 5-Y2 4-U 4-V
			dst[0] = (unsigned char)(msbY1>>8);      //Y1
			dst[1] = (unsigned char)(msbCbCr);       //U
			dst[2] = (unsigned char)(msbY2>>8);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>8);    //V
			dst += nWidth;
			//7-Y1 7-Y2 4-U 4-V
			dst[0] = (unsigned char)(msbY1>>24);      //Y1
			dst[1] = (unsigned char)(msbCbCr);        //U
			dst[2] = (unsigned char)(msbY2>>24);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>8);     //V
			dst += nWidth;

			///2nd part : Y1  : 2nd Read
			posY1 = *pTailedYPtr1++;
			tmp64BitRegY1 = srcY64bitPtr[posY1];
			lsbY1 = (unsigned int)tmp64BitRegY1;
			msbY1 = (unsigned int)(tmp64BitRegY1>>32);
			// UV : 4th Read
			posCbCr = *pTailedCbCrPtr++;
			tmp64BitRegCbCr = srcCbCr64bitPtr[posCbCr];
			lsbCbCr = (unsigned int)tmp64BitRegCbCr;
			msbCbCr = (unsigned int)(tmp64BitRegCbCr>>32);
			//Y2
			posY2 = *pTailedYPtr2++;
			tmp64BitRegY2 = srcY64bitPtr[posY2];
			lsbY2 = (unsigned int)tmp64BitRegY2;
			msbY2 = (unsigned int)(tmp64BitRegY2>>32);

			//0-Y1 0-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1);      //Y1
			dst[1] = (unsigned char)(lsbCbCr);    //U
			dst[2] = (unsigned char)(lsbY2);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8); //V
			dst += nWidth;
			///2-Y1 2-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1>>16);  //Y1
			dst[1] = (unsigned char)(lsbCbCr);    //U
			dst[2] = (unsigned char)(lsbY2>>16);  //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8); //V
			dst += nWidth;
			///4-Y1 4-Y2 4-U 4-V
			dst[0] = (unsigned char)(msbY1);          //Y1
			dst[1] = (unsigned char)(msbCbCr);        //U
			dst[2] = (unsigned char)(msbY2);          //Y2
			dst[3] = (unsigned char)(msbCbCr>>8);    //V
			dst += nWidth;
			///6-Y1 6-Y2 4-U 4-V
			dst[0] = (unsigned char)(msbY1>>16);       //Y1
			dst[1] = (unsigned char)(msbCbCr);         //U
			dst[2] = (unsigned char)(msbY2>>16);       //Y2
			dst[3] = (unsigned char)(msbCbCr>>8);      //V
			dst += nWidth;
		}

		pTailedYPtr1 += table_offset_Increment;  //60 for 4 threads, 40 for 6 threads
		pTailedYPtr2 += table_offset_Increment;  //60 for 4 threads, 40 for 6 threads

	}

	return TRUE;
}

static gboolean t_Rotate_270_Tiled_Thread_Opt(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *table_tiled_y, int *table_tiled_c)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* targetYUYVBuffer = p->targetYUYV;
	int dstScaledWidth = p->dstw;
	int sy = p->starty;
	int ey = p->endy;
	int eyc = p->endy_c;

	int table_offset = p->RotatedTableOffset_270_Y;//(sy*240)/1080;  // Need to generalize for other resolutions
	int table_offset_UV = p->RotatedTableOffset_270_UV;
	int table_offset_Increment = p->RotatedTableOffsetIncrement_270;

	dstScaledWidth -= TILE_W_SIZE;
	context->m_iTargetScaledWidth = dstScaledWidth;
	if (dstScaledWidth%TILE_W_SIZE)
	{
		dstScaledWidth = (dstScaledWidth/TILE_W_SIZE)*TILE_W_SIZE;
		context->m_iTargetScaledWidth = dstScaledWidth;
	}

	int x = 0;
	int y = 0;
	unsigned short *srcCbCr = (unsigned short *)((void *)srcCbCrBuffer);

	GST_LOG("270 Degree YTable[%d] : %p UVTable[%d] : %p  ",p->id,table_tiled_y,p->id, table_tiled_c);

	if(!table_tiled_y || !table_tiled_c )
	{
		GST_LOG("!!!!!!!!!!!!!!!!!  TABLES ARE NOT INITIALZED !!!!!!!!!!!!!!! ");
		return FALSE;
	}
	ey++;
	eyc++;

	// Y
	unsigned char *outY = NULL;
	int *pTailedYPtr1 = NULL;
	int *pTailedYPtr2 = NULL;
	int *pTailedCbCrPtr = NULL;
	unsigned long long tmp64BitRegY1 = 0;
	unsigned long long tmp64BitRegY2 = 0;
	unsigned long long tmp64BitRegCbCr = 0;
	int posY1 = 0;
	int posY2 = 0;
	int posCbCr = 0;

	pTailedYPtr1 = table_tiled_y + table_offset;
	pTailedYPtr2 = pTailedYPtr1 + table_offset_Increment;
	pTailedCbCrPtr = table_tiled_c + table_offset_UV;

	unsigned long long *srcY64bitPtr = (unsigned long long *)((void *)srcYBuffer);
	unsigned long long *srcCbCr64bitPtr = (unsigned long long *)((void *)srcCbCr);

	int nWidth = dstScaledWidth*2;
	if (dstScaledWidth % TILE_W_SIZE)
	{
		nWidth += 2*(TILE_W_SIZE -(dstScaledWidth % TILE_W_SIZE));
	}
	int delta =ey-sy;

	for(x=0; x< dstScaledWidth/2; x++)
	{
		outY = targetYUYVBuffer + (x*4);
		unsigned char *dst = outY + sy*nWidth;

		for(y=0; y<delta; y+=18)
		{
			///1st part : Y1  : 1st Read
			posY1 = *pTailedYPtr1++;
			tmp64BitRegY1 = srcY64bitPtr[posY1];
			unsigned int lsbY1 = (unsigned int)tmp64BitRegY1;
			unsigned int msbY1 = (unsigned int)(tmp64BitRegY1>>32);
			// UV : 1st Read
			posCbCr = *pTailedCbCrPtr++;
			tmp64BitRegCbCr = srcCbCr64bitPtr[posCbCr];
			unsigned int lsbCbCr = (unsigned int)tmp64BitRegCbCr;
			unsigned int msbCbCr = (unsigned int)(tmp64BitRegCbCr>>32);
			//Y2
			posY2 = *pTailedYPtr2++;
			tmp64BitRegY2 = srcY64bitPtr[posY2];
			unsigned int lsbY2 = (unsigned int)tmp64BitRegY2;
			unsigned int msbY2 = (unsigned int)(tmp64BitRegY2>>32);

			//6-Y1 6-Y2 4-U 4-V
			dst[0] = (unsigned char)(msbY1>>16);      //Y1
			dst[1] = (unsigned char)(msbCbCr);    //U
			dst[2] = (unsigned char)(msbY2>>16);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>8); //V
			dst += nWidth;
			///4-Y1 4-Y2 4-U 4-V
			dst[0] = (unsigned char)(msbY1);      //Y1
			dst[1] = (unsigned char)(msbCbCr);    //U
			dst[2] = (unsigned char)(msbY2);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>8); //V
			dst += nWidth;
			///2-Y1 2-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1>>16);      //Y1
			dst[1] = (unsigned char)(lsbCbCr);    //U
			dst[2] = (unsigned char)(lsbY2>>16);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8); //V
			dst += nWidth;
			//0-Y1 0-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1);      //Y1
			dst[1] = (unsigned char)(lsbCbCr);    //U
			dst[2] = (unsigned char)(lsbY2);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8); //V
			dst += nWidth;

			///1st part : Y1  : 2nd Read
			posY1 = *pTailedYPtr1++;
			tmp64BitRegY1 = srcY64bitPtr[posY1];
			lsbY1 = (unsigned int)tmp64BitRegY1;
			msbY1 = (unsigned int)(tmp64BitRegY1>>32);
			// UV : 2nd Read
			posCbCr = *pTailedCbCrPtr++;
			tmp64BitRegCbCr = srcCbCr64bitPtr[posCbCr];
			lsbCbCr = (unsigned int)tmp64BitRegCbCr;
			msbCbCr = (unsigned int)(tmp64BitRegCbCr>>32);
			//Y2
			posY2 = *pTailedYPtr2++;
			tmp64BitRegY2 = srcY64bitPtr[posY2];
			lsbY2 = (unsigned int)tmp64BitRegY2;
			msbY2 = (unsigned int)(tmp64BitRegY2>>32);

			//7-Y1 7-Y2 4-U 4-V
			dst[0] = (unsigned char)(msbY1>>24);      //Y1
			dst[1] = (unsigned char)(msbCbCr);    //U
			dst[2] = (unsigned char)(msbY2>>24);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>8); //V
			dst += nWidth;

			//5-Y1 5-Y2 4-U 4-V
			dst[0] = (unsigned char)(msbY1>>8);      //Y1
			dst[1] = (unsigned char)(msbCbCr);    //U
			dst[2] = (unsigned char)(msbY2>>8);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>8); //V
			dst += nWidth;

			///3-Y1 3-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1>>24);      //Y1
			dst[1] = (unsigned char)(lsbCbCr);    //U
			dst[2] = (unsigned char)(lsbY2>>24);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8); //V
			dst += nWidth;

			///1-Y1 1-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1>>8);      //Y1
			dst[1] = (unsigned char)(lsbCbCr);    //U
			dst[2] = (unsigned char)(lsbY2>>8);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8); //V
			dst += nWidth;

			// UV : 3rd Read
			posCbCr = *pTailedCbCrPtr++;
			tmp64BitRegCbCr = srcCbCr64bitPtr[posCbCr];
			lsbCbCr = (unsigned int)tmp64BitRegCbCr;
			msbCbCr = (unsigned int)(tmp64BitRegCbCr>>32);

			//0-Y1 0-Y2 6-U 6-V
			dst[0] = (unsigned char)(lsbY1);      //Y1
			dst[1] = (unsigned char)(msbCbCr>>16);    //U
			dst[2] = (unsigned char)(lsbY2);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>24); //V
			dst += nWidth;

			///2nd part : Y1  : 1st Read
			posY1 = *pTailedYPtr1++;
			tmp64BitRegY1 = srcY64bitPtr[posY1];
			lsbY1 = (unsigned int)tmp64BitRegY1;
			msbY1 = (unsigned int)(tmp64BitRegY1>>32);
			//Y2
			posY2 = *pTailedYPtr2++;
			tmp64BitRegY2 = srcY64bitPtr[posY2];
			lsbY2 = (unsigned int)tmp64BitRegY2;
			msbY2 = (unsigned int)(tmp64BitRegY2>>32);

			//6-Y1 6-Y2 6-U 6-V
			dst[0] = (unsigned char)(msbY1>>16);      //Y1
			dst[1] = (unsigned char)(msbCbCr>>16);    //U
			dst[2] = (unsigned char)(msbY2>>16);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>24); //V
			dst += nWidth;

			///4-Y1 4-Y2 2-U 2-V
			dst[0] = (unsigned char)(msbY1);      //Y1
			dst[1] = (unsigned char)(lsbCbCr>>16);    //U
			dst[2] = (unsigned char)(msbY2);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>24); //V
			dst += nWidth;

			///2-Y1 2-Y2 2-U 2-V
			dst[0] = (unsigned char)(lsbY1>>16);      //Y1
			dst[1] = (unsigned char)(lsbCbCr>>16);    //U
			dst[2] = (unsigned char)(lsbY2>>16);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>24); //V
			dst += nWidth;

			// UV : 4th Read
			posCbCr = *pTailedCbCrPtr++;
			tmp64BitRegCbCr = srcCbCr64bitPtr[posCbCr];
			lsbCbCr = (unsigned int)tmp64BitRegCbCr;
			msbCbCr = (unsigned int)(tmp64BitRegCbCr>>32);

			//0-Y1 0-Y2 6-U 6-V
			dst[0] = (unsigned char)(lsbY1);      //Y1
			dst[1] = (unsigned char)(msbCbCr>>16);    //U
			dst[2] = (unsigned char)(lsbY2);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>24); //V
			dst += nWidth;

			///2nd part : Y1  : 2nd Read
			posY1 = *pTailedYPtr1++;
			tmp64BitRegY1 = srcY64bitPtr[posY1];
			lsbY1 = (unsigned int)tmp64BitRegY1;
			msbY1 = (unsigned int)(tmp64BitRegY1>>32);
			//Y2
			posY2 = *pTailedYPtr2++;
			tmp64BitRegY2 = srcY64bitPtr[posY2];
			lsbY2 = (unsigned int)tmp64BitRegY2;
			msbY2 = (unsigned int)(tmp64BitRegY2>>32);

			//7-Y1 7-Y2 6-U 6-V
			dst[0] = (unsigned char)(msbY1>>24);      //Y1
			dst[1] = (unsigned char)(msbCbCr>>16);    //U
			dst[2] = (unsigned char)(msbY2>>24);      //Y2
			dst[3] = (unsigned char)(msbCbCr>>24); //V
			dst += nWidth;

			//5-Y1 5-Y2 2-U 2-V
			dst[0] = (unsigned char)(msbY1>>8);      //Y1
			dst[1] = (unsigned char)(lsbCbCr>>16);    //U
			dst[2] = (unsigned char)(msbY2>>8);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>24); //V
			dst += nWidth;

			///3-Y1 3-Y2 2-U 2-V
			dst[0] = (unsigned char)(lsbY1>>24);      //Y1
			dst[1] = (unsigned char)(lsbCbCr>>16);    //U
			dst[2] = (unsigned char)(lsbY2>>24);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>24); //V
			dst += nWidth;

			///1-Y1 1-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1>>8);      //Y1
			dst[1] = (unsigned char)(lsbCbCr);    //U
			dst[2] = (unsigned char)(lsbY2>>8);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8); //V
			dst += nWidth;

			//0-Y1 0-Y2 0-U 0-V
			dst[0] = (unsigned char)(lsbY1);      //Y1
			dst[1] = (unsigned char)(lsbCbCr);    //U
			dst[2] = (unsigned char)(lsbY2);      //Y2
			dst[3] = (unsigned char)(lsbCbCr>>8); //V
			dst += nWidth;

		}

		pTailedYPtr1 += table_offset_Increment;  //60 for 4 threads, 40 for 6 threads
		pTailedYPtr2 += table_offset_Increment;  //60 for 4 threads, 40 for 6 threads

	}

	return TRUE;
}

static gboolean t_Rotate_180_Tiled_Thread(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *table_tiled_180)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* targetYUYVBuffer = NULL;
	int dstScaledWidth = p->dstw;
	int dstScaledHeight = p->dsth;
	int syc = p->starty_c;
	int eyc = p->endy_c;
	int m_OriginalWidth = p->srcw;

	int iTileSize = TILE_W_SIZE*TILE_H_SIZE;
	int iTilesPerLine = m_OriginalWidth/TILE_W_SIZE;

	context->m_iTargetScaledWidth = dstScaledWidth;

	/*Y and CbCr are copied in separate threads without tables*/
	int x, z;
	eyc++;

	unsigned int lsb32Y0, lsb32Y1, msb32Y0, msb32Y1, lsb32UV, msb32UV;
	unsigned long long tmp64BitReg = 0;
	unsigned char* srcy = NULL;
	unsigned char* srcuv = NULL;
	unsigned long long *srcY64bitPtr0 = NULL;
	unsigned long long *srcY64bitPtr1 = NULL;
	unsigned long long 	*srcUV64bitPtr = NULL;
	unsigned char* dstyuyv0 = NULL;
	unsigned char* dstyuyv1 = NULL;
	int *pTableTiled180Ptr = NULL;
	int nWidth = 0;
	int nLeftOverData = 0;

	if (context->m_IsEnableGAMemoryForTiledRotation == TRUE)
	{
		// Choose GA Buffer for 180
		targetYUYVBuffer = p->targetGAbuffer;
	}
	else
	{
		// Choose Back Buffer for 180
		targetYUYVBuffer = p->targetYUYV;
	}

	nWidth = dstScaledWidth*2;

	if (dstScaledWidth % TILE_W_SIZE)
	{
		int Delta = TILE_W_SIZE - (dstScaledWidth % TILE_W_SIZE);
		int newDstWidth = 2*(dstScaledWidth+Delta);
		unsigned char* dstyuyv = targetYUYVBuffer+ ((dstScaledHeight-2*syc)*newDstWidth) -1;
		dstyuyv0 = dstyuyv - 2*Delta;
		dstyuyv1 = dstyuyv0 - newDstWidth;
		nWidth +=  4*Delta; 
		nLeftOverData = dstScaledWidth % TILE_W_SIZE;
	}
	else
	{
		dstyuyv0 = targetYUYVBuffer+((dstScaledHeight-2*syc)*2*dstScaledWidth)-1;
		dstyuyv1 = dstyuyv0 - 2*dstScaledWidth;
		nLeftOverData = 0;
	}

	pTableTiled180Ptr = table_tiled_180;

	for(x=syc; x < eyc; x++)
	{
		srcy = srcYBuffer + pTableTiled180Ptr[2*x];
		srcuv = srcCbCrBuffer + pTableTiled180Ptr[x];

		srcY64bitPtr0 = (unsigned long long *)((void *)(srcy));
		srcY64bitPtr1 = srcY64bitPtr0+(TILE_W_SIZE>>3);
		srcUV64bitPtr = (unsigned long long *)((void *)(srcuv));

		for(z=0; z<iTilesPerLine; z++)
		{
			tmp64BitReg = srcY64bitPtr0[0];
			lsb32Y0 = (unsigned int)tmp64BitReg;
			msb32Y0 = (unsigned int)(tmp64BitReg>>32);
			tmp64BitReg = srcY64bitPtr1[0];
			lsb32Y1 = (unsigned int)tmp64BitReg;
			msb32Y1 = (unsigned int)(tmp64BitReg>>32);
			tmp64BitReg = srcUV64bitPtr[0];
			lsb32UV = (unsigned int)tmp64BitReg;
			msb32UV = (unsigned int)(tmp64BitReg>>32);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>8);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>8);
			*dstyuyv0-- = (unsigned char)(lsb32Y0);
			*dstyuyv1-- = (unsigned char)(lsb32Y1);

			*dstyuyv0-- = (unsigned char)(lsb32UV);
			*dstyuyv1-- = (unsigned char)(lsb32UV);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>8);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>8);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>24);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>24);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>16);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>16);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>16);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>16);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>24);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>24);

			*dstyuyv0-- = (unsigned char)(msb32UV>>8);
			*dstyuyv1-- = (unsigned char)(msb32UV>>8);
			*dstyuyv0-- = (unsigned char)(msb32Y0);
			*dstyuyv1-- = (unsigned char)(msb32Y1);

			*dstyuyv0-- = (unsigned char)(msb32UV);
			*dstyuyv1-- = (unsigned char)(msb32UV);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>8);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>8);

			*dstyuyv0-- = (unsigned char)(msb32UV>>24);
			*dstyuyv1-- = (unsigned char)(msb32UV>>24);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>16);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>16);

			*dstyuyv0-- = (unsigned char)(msb32UV>>16);
			*dstyuyv1-- = (unsigned char)(msb32UV>>16);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>24);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>24);

			tmp64BitReg = srcY64bitPtr0[1];
			lsb32Y0 = (unsigned int)tmp64BitReg;
			msb32Y0 = (unsigned int)(tmp64BitReg>>32);
			tmp64BitReg = srcY64bitPtr1[1];
			lsb32Y1 = (unsigned int)tmp64BitReg;
			msb32Y1 = (unsigned int)(tmp64BitReg>>32);
			tmp64BitReg = srcUV64bitPtr[1];
			lsb32UV = (unsigned int)tmp64BitReg;
			msb32UV = (unsigned int)(tmp64BitReg>>32);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>8);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>8);
			*dstyuyv0-- = (unsigned char)(lsb32Y0);
			*dstyuyv1-- = (unsigned char)(lsb32Y1);

			*dstyuyv0-- = (unsigned char)(lsb32UV);
			*dstyuyv1-- = (unsigned char)(lsb32UV);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>8);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>8);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>24);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>24);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>16);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>16);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>16);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>16);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>24);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>24);

			*dstyuyv0-- = (unsigned char)(msb32UV>>8);
			*dstyuyv1-- = (unsigned char)(msb32UV>>8);
			*dstyuyv0-- = (unsigned char)(msb32Y0);
			*dstyuyv1-- = (unsigned char)(msb32Y1);

			*dstyuyv0-- = (unsigned char)(msb32UV);
			*dstyuyv1-- = (unsigned char)(msb32UV);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>8);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>8);

			*dstyuyv0-- = (unsigned char)(msb32UV>>24);
			*dstyuyv1-- = (unsigned char)(msb32UV>>24);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>16);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>16);

			*dstyuyv0-- = (unsigned char)(msb32UV>>16);
			*dstyuyv1-- = (unsigned char)(msb32UV>>16);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>24);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>24);

			srcY64bitPtr0 = srcY64bitPtr0 + (iTileSize>>3);
			srcY64bitPtr1 = srcY64bitPtr1 + (iTileSize>>3);
			srcUV64bitPtr = srcUV64bitPtr + (iTileSize>>3);
		}

		//Process Remaining Tiled Data
		if ((nLeftOverData > 0) && (nLeftOverData < TILE_W_SIZE))
		{
			// Remove this function after adjusting process correctly the remaining tiled and adjusting offsets accordingly to avoid alignment issues
			gboolean Ret = t_HandleLeftOverTiledData(nLeftOverData, srcY64bitPtr0, srcY64bitPtr1, srcUV64bitPtr, dstyuyv0, dstyuyv1);
			if (Ret == TRUE)
			{
				dstyuyv0 -= 2*nLeftOverData;
				dstyuyv1 -= 2*nLeftOverData;
			}
		}

		dstyuyv0 -= nWidth;
		dstyuyv1 -= nWidth;

	}

	return TRUE;
}

static gboolean t_Rotate_180_Tiled_Thread_for_interlace(struct VideoFrameRotateContext* context, struct PerThreadContext* p, int *table_tiled_180)
{
	unsigned char* srcYBuffer = p->srcY;
	unsigned char* srcCbCrBuffer =  p->srcCbCr;
	unsigned char* targetYUYVBuffer = NULL;
	int dstScaledWidth = p->dstw;
	int dstScaledHeight = p->dsth;
	int syc = p->starty_c;
	int eyc = p->endy_c;
	int m_OriginalWidth = p->srcw;

	int iTileSize = TILE_W_SIZE*TILE_H_SIZE;
	int iTilesPerLine = m_OriginalWidth/TILE_W_SIZE;

	context->m_iTargetScaledWidth = dstScaledWidth;

	/*Y and CbCr are copied in separate threads without tables*/
	int x, z;
	eyc++;

	unsigned int lsb32Y0, lsb32Y1, msb32Y0, msb32Y1, lsb32UV, msb32UV;
	unsigned long long tmp64BitReg = 0;
	unsigned char* srcy = NULL;
	unsigned char* srcuv = NULL;
	unsigned long long *srcY64bitPtr0 = NULL;
	unsigned long long *srcY64bitPtr1 = NULL;
	unsigned long long 	*srcUV64bitPtr = NULL;
	unsigned char* dstyuyv0 = NULL;
	unsigned char* dstyuyv1 = NULL;
	int *pTableTiled180Ptr = NULL;
	int nWidth = 0;
	int nLeftOverData = 0;

	if (context->m_IsEnableGAMemoryForTiledRotation == TRUE)
	{
		// Choose GA Buffer for 180
		targetYUYVBuffer = p->targetGAbuffer;
	}
	else
	{
		// Choose Back Buffer for 180
		targetYUYVBuffer = p->targetYUYV;
	}

	nWidth = dstScaledWidth*2;

	if (dstScaledWidth % TILE_W_SIZE)
	{
		int Delta = TILE_W_SIZE - (dstScaledWidth % TILE_W_SIZE);
		int newDstWidth = 2*(dstScaledWidth+Delta);
		unsigned char* dstyuyv = targetYUYVBuffer+ ((dstScaledHeight-2*syc)*newDstWidth) -1;
		dstyuyv0 = dstyuyv - 2*Delta;
		dstyuyv1 = dstyuyv0 - newDstWidth;
		nWidth +=  4*Delta;
		nLeftOverData = dstScaledWidth % TILE_W_SIZE;
	}
	else
	{
		dstyuyv0 = targetYUYVBuffer+((dstScaledHeight-2*syc)*2*dstScaledWidth)-1;
		dstyuyv1 = dstyuyv0 - 2*dstScaledWidth;
		nLeftOverData = 0;
	}

	pTableTiled180Ptr = table_tiled_180;

	for(x=syc; x < eyc; x++)
	{
		srcy = srcYBuffer + pTableTiled180Ptr[2*x];
		srcuv = srcCbCrBuffer + pTableTiled180Ptr[x];

		srcY64bitPtr0 = (unsigned long long *)((void *)(srcy));
		srcY64bitPtr1 = srcY64bitPtr0;
		srcUV64bitPtr = (unsigned long long *)((void *)(srcuv));

		for(z=0; z<iTilesPerLine; z++)
		{
			tmp64BitReg = srcY64bitPtr0[0];
			lsb32Y0 = (unsigned int)tmp64BitReg;
			msb32Y0 = (unsigned int)(tmp64BitReg>>32);
			tmp64BitReg = srcY64bitPtr1[0];
			lsb32Y1 = (unsigned int)tmp64BitReg;
			msb32Y1 = (unsigned int)(tmp64BitReg>>32);
			tmp64BitReg = srcUV64bitPtr[0];
			lsb32UV = (unsigned int)tmp64BitReg;
			msb32UV = (unsigned int)(tmp64BitReg>>32);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>8);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>8);
			*dstyuyv0-- = (unsigned char)(lsb32Y0);
			*dstyuyv1-- = (unsigned char)(lsb32Y1);

			*dstyuyv0-- = (unsigned char)(lsb32UV);
			*dstyuyv1-- = (unsigned char)(lsb32UV);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>8);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>8);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>24);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>24);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>16);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>16);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>16);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>16);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>24);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>24);

			*dstyuyv0-- = (unsigned char)(msb32UV>>8);
			*dstyuyv1-- = (unsigned char)(msb32UV>>8);
			*dstyuyv0-- = (unsigned char)(msb32Y0);
			*dstyuyv1-- = (unsigned char)(msb32Y1);

			*dstyuyv0-- = (unsigned char)(msb32UV);
			*dstyuyv1-- = (unsigned char)(msb32UV);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>8);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>8);

			*dstyuyv0-- = (unsigned char)(msb32UV>>24);
			*dstyuyv1-- = (unsigned char)(msb32UV>>24);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>16);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>16);

			*dstyuyv0-- = (unsigned char)(msb32UV>>16);
			*dstyuyv1-- = (unsigned char)(msb32UV>>16);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>24);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>24);

			tmp64BitReg = srcY64bitPtr0[1];
			lsb32Y0 = (unsigned int)tmp64BitReg;
			msb32Y0 = (unsigned int)(tmp64BitReg>>32);
			tmp64BitReg = srcY64bitPtr1[1];
			lsb32Y1 = (unsigned int)tmp64BitReg;
			msb32Y1 = (unsigned int)(tmp64BitReg>>32);
			tmp64BitReg = srcUV64bitPtr[1];
			lsb32UV = (unsigned int)tmp64BitReg;
			msb32UV = (unsigned int)(tmp64BitReg>>32);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>8);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>8);
			*dstyuyv0-- = (unsigned char)(lsb32Y0);
			*dstyuyv1-- = (unsigned char)(lsb32Y1);

			*dstyuyv0-- = (unsigned char)(lsb32UV);
			*dstyuyv1-- = (unsigned char)(lsb32UV);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>8);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>8);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>24);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>24);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>16);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>16);

			*dstyuyv0-- = (unsigned char)(lsb32UV>>16);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>16);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>24);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>24);

			*dstyuyv0-- = (unsigned char)(msb32UV>>8);
			*dstyuyv1-- = (unsigned char)(msb32UV>>8);
			*dstyuyv0-- = (unsigned char)(msb32Y0);
			*dstyuyv1-- = (unsigned char)(msb32Y1);

			*dstyuyv0-- = (unsigned char)(msb32UV);
			*dstyuyv1-- = (unsigned char)(msb32UV);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>8);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>8);

			*dstyuyv0-- = (unsigned char)(msb32UV>>24);
			*dstyuyv1-- = (unsigned char)(msb32UV>>24);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>16);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>16);

			*dstyuyv0-- = (unsigned char)(msb32UV>>16);
			*dstyuyv1-- = (unsigned char)(msb32UV>>16);
			*dstyuyv0-- = (unsigned char)(msb32Y0>>24);
			*dstyuyv1-- = (unsigned char)(msb32Y1>>24);

			srcY64bitPtr0 = srcY64bitPtr0 + (iTileSize>>3);
			srcY64bitPtr1 = srcY64bitPtr1 + (iTileSize>>3);
			srcUV64bitPtr = srcUV64bitPtr + (iTileSize>>3);
		}

		//Process Remaining Tiled Data
		if ((nLeftOverData > 0) && (nLeftOverData < TILE_W_SIZE))
		{
			// Remove this function after adjusting process correctly the remaining tiled and adjusting offsets accordingly to avoid alignment issues
			gboolean Ret = t_HandleLeftOverTiledData(nLeftOverData, srcY64bitPtr0, srcY64bitPtr1, srcUV64bitPtr, dstyuyv0, dstyuyv1);
			if (Ret == TRUE)
			{
				dstyuyv0 -= 2*nLeftOverData;
				dstyuyv1 -= 2*nLeftOverData;
			}
		}

		dstyuyv0 -= nWidth;
		dstyuyv1 -= nWidth;

	}

	return TRUE;
}

static gboolean t_InitVideoRotationContext(struct VideoFrameRotateContext* context, int linesize_open)
{
	gboolean RetStatus = TRUE;
	gboolean RetVal = TRUE;
	if (context->m_bIsVideoRotationEnabled == TRUE)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("t_InitVideoRotationContext ++++++++++ ");
		}

	#ifdef TILED_FRAME_16X32_VIDEOROTATION
		context->m_bIsRotationEnabledForTiledFormat = TRUE;

		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG(" ==========================================");
			GST_LOG("    ROTATION ENABLED FOR : MSTATR - X12/X10P \n");
			GST_LOG(" ==========================================");
		}

		context->m_eColorFormat = MD_COLOR_FORMAT_YCbCr422;
		context->m_eVideoDataFormat = VIDEO_DATA_FORMAT_YCBCR;
		//Remove this code after adding low resolution interlaced support for X12
		context->m_bIsLowResolution = FALSE;
		context->m_bIsCallInterScanFunc = FALSE;

		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("context->m_eColorFormat  : %d context->m_eVideoDataFormat : %d ",context->m_eColorFormat,context->m_eVideoDataFormat);
		}
	#endif

		// To decide Number of threads and optimizations for x10p mstar rotation
		t_DecideThreadCountForRotation(context);
		//To Calculate Target dimentions
		if (context->m_IsVeryLowResolutionOpt == TRUE)
		{
			// only or very low resolution files like <176x144 to improve rotation quality, just swap width and height
	 		RetVal = t_CalculateTargetDimentionForVeryLowResolution(context);
		}
		else
		{
			RetVal = t_CalculateTargetDimention(context);
		}
		if (RetVal == TRUE)
		{
			GST_LOG("t_CalculateTargetDimention SUCCEED! ");
			//Create Rotation Tables
			if (t_CreateRotationTables(context,linesize_open) == TRUE)
			{
				GST_LOG("t_CreateRotationTables SUCCEED! ");
				// Creating Y and CbCr GA memory for Rotation and Scaling Buffers
				if (t_AssignGAMemoryForScaling(context)== TRUE)
				{
					GST_LOG("t_AssignGAMemoryForScaling SUCCEED! ");
					//Create threads for Rotation and create rotation table for each thread
					if (t_AssignThreadsForRotation(context) == TRUE)
					{
						GST_LOG("t_AssignThreadsForRotation SUCCEED! ");
						//Success
					}
					else
					{
						//free tables, GA memory, threads, etc
						t_DeInitVideoRotationContext(context);	
						RetStatus = FALSE;
						context->m_bIsVideoRotationEnabled = FALSE;
						GST_LOG("t_AssignThreadsForRotation FAILED !!!!!!!! ");
					}
				}
				else
				{
					// free if any tables are created
					if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
					{
						t_FreeRotationTiledTables(context);
					}
					else
					{
						t_FreeRotationTables(context);
					}
					// free if GA memory created
					t_FreeGAScaledY_CbCrBuffers(context);
					RetStatus = FALSE;
					context->m_bIsVideoRotationEnabled = FALSE;
					GST_LOG("t_AssignGAMemoryForScaling FAILED !!!!!!!! ");
				}
			}
			else
			{
				// free if any tables are created
				if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
				{
					t_FreeRotationTiledTables(context);
				}
				else
				{
					t_FreeRotationTables(context);
				}
				RetStatus = FALSE;
				context->m_bIsVideoRotationEnabled = FALSE;
				GST_LOG("t_CreateRotationTables FAILED!!!!!!!! ");
			}
		}
		else
		{
			 RetStatus = FALSE;
			 context->m_bIsVideoRotationEnabled = FALSE;
			 GST_LOG("t_CalculateTargetDimention FAILED ");

		}
		GST_LOG("t_InitVideoRotationContext: Ret[%d] -> ScalledW %d, ScalledH %d, OriginalW %d, OriginalH %d ", RetStatus, context->m_iScaledWidth, context->m_iScaledHeight, context->m_iOriginalWidth, context->m_iOriginalHeight);
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("ScalledW %d, ScalledH %d, OriginalW %d, OriginalH %d ",context->m_iScaledWidth, context->m_iScaledHeight, context->m_iOriginalWidth, context->m_iOriginalHeight);
			GST_LOG("t_InitVideoRotationContext ---------- ");
		}
	}
	else
	{
		 // Do nothing if rotation is not enabled
	}
	return RetStatus;
}

static void t_DeInitVideoRotationContext(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsVideoRotationEnabled == TRUE)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("t_DeInitVideoRotationContext ++++++++++ ");
		}
		t_FreeGAScaledY_CbCrBuffers(context);

		//Free mstar-x10p : rotation tiled tables
		if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
		{
			t_FreeRotationTiledTables(context);
		}
		else // ECHOP & Other Platforms
		{
			t_FreeRotationTables(context);
		}

		t_FreeThreadsForRotation(context);
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("t_DeInitVideoRotationContext ---------- ");
		}
	}
	else
	{
		//NULL
	}
}

static void t_FreeThreadsForRotation(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeThreadsForRotation +++++++ ");
	}

	struct PerThreadContext *p;
	int i;

	if (context->m_pAllThreadContextRotate != NULL)
	{
		context->m_pAllThreadContextRotate->stop = 1;
		for(i=0; i<context->m_pAllThreadContextRotate->thread_count; i++)
		{
			p = &context->m_pAllThreadContextRotate->thread[i];
			pthread_mutex_lock(&p->update_mutex);
			p->update=1;
			pthread_cond_signal(&p->update_cond);
			pthread_mutex_unlock(&p->update_mutex);
		}

		//if(context->m_pAllThreadContextRotate->thread_count > 1) // remove this condition as we already using thread count > 1
		{
			for(i=0; i<context->m_pAllThreadContextRotate->thread_count; i++)
			{
				p = &context->m_pAllThreadContextRotate->thread[i];

				if(context->m_pAllThreadContextRotate->workers[i])
				{
					pthread_join(context->m_pAllThreadContextRotate->workers[i], NULL);
				}

				//MSTAR-X10P Platform
				if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
				{
					t_FreeThreadTiledTablesForRotation(context, p);
				}
				else // ECHOP and Other Platforms
				{
					t_FreeThreadTablesForRotation(context, p);
				}

				pthread_mutex_destroy(&p->update_mutex);
				pthread_cond_destroy(&p->update_cond);
				pthread_mutex_destroy(&p->finish_mutex);
				pthread_cond_destroy(&p->finish_cond);
			}

			pthread_mutex_destroy(&context->m_pAllThreadContextRotate->context_mutex);

			if (context->m_pAllThreadContextRotate->thread)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("  Free context->m_pAllThreadContextRotate->thread Success : %p ",context->m_pAllThreadContextRotate->thread);
				}
				g_free(context->m_pAllThreadContextRotate->thread);
				context->m_pAllThreadContextRotate->thread = NULL;
			}
			if (context->m_pAllThreadContextRotate->workers)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("  Free context->m_pAllThreadContextRotate->workers Success : %p ",context->m_pAllThreadContextRotate->workers);
				}
				g_free(context->m_pAllThreadContextRotate->workers);
				context->m_pAllThreadContextRotate->workers = NULL;
			}
			if (context->m_pAllThreadContextRotate)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("  Free context->m_pAllThreadContextRotate Success : %p ",context->m_pAllThreadContextRotate);
				}
				g_free(context->m_pAllThreadContextRotate);
				context->m_pAllThreadContextRotate = NULL;
			}
		}
	}
	else
	{
		//LOG_ERROR("context->m_pAllThreadContextRotate == NULL");
	}
	context->m_bIsRotationThreadsInit = FALSE;

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeThreadsForRotation ------- ");
	}
}

static void t_FreeThreadTablesForRotation(struct VideoFrameRotateContext* context, struct PerThreadContext* p)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeThreadTablesForRotation [%d] +++++++ ",p->id);
	}
	if(!p)
		return;

	if(p->Ty1)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Ty1[%d] Success : p->Ty1 : %p ",p->id,p->Ty1);
		}
		g_free(p->Ty1);
		p->Ty1 = NULL;
	}
	if(p->Ty2)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Ty2[%d] Success : p->Ty2 : %p ",p->id,p->Ty2);
		}
		g_free(p->Ty2);
		p->Ty2 = NULL;
	}
	if(p->Ty3)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Ty3[%d] Success : p->Ty3 : %p ",p->id,p->Ty3);
		}
		g_free(p->Ty3);
		p->Ty3 = NULL;
	}
	if(p->Tuv1)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Tuv1[%d] Success : p->Tuv1 : %p ",p->id,p->Tuv1);
		}
		g_free(p->Tuv1);
		p->Tuv1 = NULL;
	}
	if(p->Tuv2)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Tuv2[%d] Success : p->Tuv2 : %p ",p->id,p->Tuv2);
		}
		g_free(p->Tuv2);
		p->Tuv2 = NULL;
	}
	if(p->Tuv3)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Tuv3[%d] Success : p->Tuv3 : %p ",p->id,p->Tuv3);
		}
		g_free(p->Tuv3);
		p->Tuv3 = NULL;
	}
	if(p->Ty4)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Ty4[%d] Success : p->Ty4 : %p ",p->id,p->Ty4);
		}
		g_free(p->Ty4);
		p->Ty4 = NULL;
	}
	if(p->Ty5)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Ty5[%d] Success : p->Ty5 : %p ",p->id,p->Ty5);
		}
		g_free(p->Ty5);
		p->Ty5 = NULL;
	}
	if(p->Ty6)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Ty6[%d] Success : p->Ty6 : %p ",p->id,p->Ty6);
		}
		g_free(p->Ty6);
		p->Ty6 = NULL;
	}
	if(p->Tuv4)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Tuv4[%d] Success : p->Tuv4 : %p ",p->id,p->Tuv4);
		}
		g_free(p->Tuv4);
		p->Tuv4 = NULL;
	}
	if(p->Tuv5)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Tuv5[%d] Success : p->Tuv5 : %p ",p->id,p->Tuv5);
		}
		g_free(p->Tuv5);
		p->Tuv5 = NULL;
	}
	if(p->Tuv6)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free p->Tuv6[%d] Success : p->Tuv6 : %p ",p->id,p->Tuv6);
		}
		g_free(p->Tuv6);
		p->Tuv6 = NULL;
	}
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeThreadTablesForRotation [%d] ------- ",p->id);
	}
}

static void t_FreeThreadTiledTablesForRotation(struct VideoFrameRotateContext* context, struct PerThreadContext* p)
{
	if(!p)
		return;

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeThreadTiledTablesForRotation [%d] +++++++",p->id);
	}

	if(p->TiledRotatedTableY90Deg)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free 90 Degree Y Table Success : TiledRotatedTableY90Deg[%d] : %p ",p->id, p->TiledRotatedTableY90Deg);
		}
		g_free(p->TiledRotatedTableY90Deg);
		p->TiledRotatedTableY90Deg = NULL;
	}
	if(p->TiledRotatedTableUV90Deg)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free 90 Degree UV Table Success : TiledRotatedTableUV90Deg[%d] : %p ",p->id, p->TiledRotatedTableUV90Deg);
		}
		g_free(p->TiledRotatedTableUV90Deg);
		p->TiledRotatedTableUV90Deg = NULL;
	}

	//270 Degree
	if(p->TiledRotatedTableY270Deg)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free 90 Degree Y Table Success : TiledRotatedTableY270Deg[%d] : %p ",p->id, p->TiledRotatedTableY270Deg);
		}
		g_free(p->TiledRotatedTableY270Deg);
		p->TiledRotatedTableY270Deg = NULL;
	}
	if(p->TiledRotatedTableUV270Deg)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free 90 Degree UV Table Success : TiledRotatedTableUV270Deg[%d] : %p ",p->id, p->TiledRotatedTableUV270Deg);
		}
		g_free(p->TiledRotatedTableUV270Deg);
		p->TiledRotatedTableUV270Deg = NULL;
	}

	//180 Degree
	if(p->TiledRotatedTable180Deg)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free 90 Degree Y Table Success : TiledRotatedTable180Deg[%d] : %p ",p->id, p->TiledRotatedTable180Deg);
		}
		g_free(p->TiledRotatedTable180Deg);
		p->TiledRotatedTable180Deg = NULL;
	}

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeThreadTiledTablesForRotation [%d] ------- ",p->id);
	}
}


static void t_FreeRotationTiledTables(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeRotationTiledTables +++++++ ");
	}
	//Free totated 90 degrees tables
	if(context->TiledRotatedTableY90Deg)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free 90 Degree Y Table Success : TiledRotatedTableY90Deg : %p",context->TiledRotatedTableY90Deg);
		}
		g_free(context->TiledRotatedTableY90Deg);
		context->TiledRotatedTableY90Deg = NULL;

	}
	if(context->TiledRotatedTableUV90Deg)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free 90 Degree UV Table Success : TiledRotatedTableUV90Deg : %p",context->TiledRotatedTableUV90Deg);
		}
		g_free(context->TiledRotatedTableUV90Deg);
		context->TiledRotatedTableUV90Deg = NULL;
	}

	//Free totated 270 degrees tables
	if(context->TiledRotatedTableY270Deg)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free 270 Degree Y Table Success : TiledRotatedTableY270Deg : %p",context->TiledRotatedTableY270Deg);
		}
		g_free(context->TiledRotatedTableY270Deg);
		context->TiledRotatedTableY270Deg = NULL;
	}
	if(context->TiledRotatedTableUV270Deg)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free 270 Degree UV Table Success : TiledRotatedTableUV270Deg : %p",context->TiledRotatedTableUV270Deg);
		}
		g_free(context->TiledRotatedTableUV270Deg);
		context->TiledRotatedTableUV270Deg = NULL;
	}

	//Free totated 180 degrees tables
	if(context->TiledRotatedTable180Deg)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free 180 Degree Table Success : TiledRotatedTable180Deg : %p",context->TiledRotatedTable180Deg);
		}
		g_free(context->TiledRotatedTable180Deg);
		context->TiledRotatedTable180Deg = NULL;
	}

	context->m_bIsRotationTableInit = FALSE;
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeRotationTiledTables -------");
	}
}

static void t_FreeBackBufferMemory(struct VideoFrameRotateContext* context)
{
	// turnOff the cache  for backbuffer GFX1
	t_DisableCache(context);

	if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
	{
		if (context->m_pBackBuffer)
		{
			context->m_pBackBuffer = NULL;
			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("Free context->m_pBackBuffer Success :  %p",context->m_pBackBuffer);
			}
		}

		if (context->m_IsEnableGAMemoryForTiledRotation == TRUE)
		{
			gboolean RetVal = FALSE;//CPlatformInterface::GetInstance()->FreeGAVideoRotationMemory(context->m_pGARotateBuffer, context->m_pGAMemHandle_RotateBuf);
			if (RetVal == TRUE)
			{
				context->m_pGARotateBuffer = NULL;
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG(" FREE context->m_pGABufferToRotate SUCCESS : %p",context->m_pGARotateBuffer);
				}
			}
			else
			{
				GST_LOG(" ERROR: FAILED TO FREE context->m_pGARotateBuffer");
			}
		}
	}
}

static void t_FreeGAMemory(struct VideoFrameRotateContext* context)
{
	//Free GA memory 4MB
	if (context->m_pGAMemory)
	{
		gboolean RetVal = TRUE;
		// FOXB - Clear JAVA0 memory
		if (context->m_bIsJava0MemoryUsedForGA == TRUE)
		{
			RetVal = FALSE;//CPlatformInterface::GetInstance()->FreeBDCachebleGAMemoory(context->m_pGAMemory, context->m_hUmpHandle);
			if (RetVal == TRUE)
			{
				context->m_pGAMemory = NULL;
				if (context->m_bIsRotationPrintsEnabled)
				{
					//LOG_ERROR(" FREE JAVA0 BDCachebleGAMemoory SUCCESS");
				}
			}
			else
			{
				//LOG_ERROR(" ERROR: FAILED TO FREE JAVA0 BDCachebleGAMemoory");
			}
		}
		else  // FOXP/X12
		{
			RetVal = FALSE;//CPlatformInterface::GetInstance()->FreeGAVideoRotationMemory(context->m_pGAMemory, context->m_pGAMemoryHandle);
			if (RetVal == TRUE)
			{
				context->m_pGAMemory = NULL;
				if (context->m_bIsRotationPrintsEnabled)
				{
					//LOG_ERROR(" FREE context->m_pGAMemory SUCCESS");
				}
			}
			else
			{
				//LOG_ERROR(" ERROR: FAILED TO FREE context->m_pGAMemory");
			}
		}
	}

#ifdef  ENABLE_TBM
	#ifdef ENABLE_LOCAL_ROTATE_BUFFER
		free_rotate_local_buffer(context);
	#endif
		free_rotate_tbm_buffer(context);
 #endif

	//Free System Memory
	if (context->m_bIsCacheMissOptEnabled == TRUE)
	{
		//Free System Output Y Buffer
		if (context->m_pSysOutputYBuffer)
		{
			g_free(context->m_pSysOutputYBuffer);
			context->m_pSysOutputYBuffer = NULL;
			if (context->m_bIsRotationPrintsEnabled)
			{
				//LOG_ERROR(" FREE context->m_pSysOutputYBuffer SUCCESS");
			}
		}
		else
		{
			//LOG_ERROR(" ERROR: FAILED TO FREE context->m_pSysOutputYBuffer");
		}
		// Free System Output CbCr Buffer
		if (context->m_pSysOutputCbCrBuffer)
		{
			g_free(context->m_pSysOutputCbCrBuffer);
			context->m_pSysOutputCbCrBuffer = NULL;
			if (context->m_bIsRotationPrintsEnabled)
			{
				//LOG_ERROR(" FREE context->m_pSysOutputCbCrBuffer SUCCESS");
			}
		}
		else
		{
			//LOG_ERROR(" ERROR: FAILED TO FREE context->m_pSysOutputCbCrBuffer");
		}
	}
}

static void t_FreeGAScaledY_CbCrBuffers(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeGAScaledY_CbCrBuffers +++++++");
	}

	// Free BackBuffer Memory
	if (context->m_iRotationGAScalledBuffer_GFX1 == 1)
	{
		t_FreeBackBufferMemory(context);
	}
	else  // Free GA Memory
	{
		t_FreeGAMemory(context);
	}

	context->m_pGAScaledYBuffer = NULL;
	context->m_pGAScaledCbCrBuffer = NULL;
	context->m_pBackBuffer = NULL;
	context->m_pGARotateBuffer = NULL;

	context->m_bIsRotateBufferAllocated = FALSE;

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeGAScaledY_CbCrBuffers ------- ");
	}
}

static void t_DecideThreadCountForRotation(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_DecideThreadCountForRotation +++++++ ");
	}
	// For mstar-x10p Tiled format
	if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
	{
		// X10P/X12
		//Disable Backbuffer usage as it give screen noise issue, , use GA memory
		context->m_iRotationGAScalledBuffer_GFX1 = 0;
		context->m_bIsTheadAffinity = TRUE;
		context->m_iCpuCoreCount = 2;
		// Enable/Disable CacheMiss related Optimizations to reduce CPU Usage
		// This required extra memcpy (takes 2 msecs/frame on FOXP)
		context->m_bIsCacheMissOptEnabled = TRUE;  // Default Enable for X10P/X12 except 1080P & 720P
		//For Cache Flush Params
		context->m_ihResolution = 1280;
		context->m_ivResolution = 720;
		context->m_bIs180DegOptimized = FALSE; // FOXP only

		// reduce to HD quality due to divix certification issues for all
		context->m_bIsOptimized = TRUE;
		/*
		// To enable skip optimization for X12 Interlaced Files and H264 : need to removed this patch
		if ((context->m_bIsInterlacedScanType == TRUE) && (context->m_iCodecId == CODEC_ID_H264) && (context->m_iOriginalWidth == 1920 && context->m_iOriginalHeight == 1080))
		{
			context->m_bIsOptimized = TRUE;
		}*/

		if (context->m_iOriginalWidth == 1920 && context->m_iOriginalHeight == 1080)
		{
			context->m_iRotationThreadCount = 4;
			//context->m_bIsApplyRotationForOptimizations = TRUE;
			//context->m_bIsCacheMissOptEnabled = FALSE;
			context->m_bIsApplyRotationForOptimizations = FALSE;
		}
		else if (context->m_iOriginalWidth == 1280 && context->m_iOriginalHeight == 720)
		{
			context->m_iRotationThreadCount = 4;
			//context->m_bIsApplyRotationForOptimizations = TRUE;
			//context->m_bIsCacheMissOptEnabled = FALSE;
			context->m_bIsApplyRotationForOptimizations = FALSE;
		}
		else if (context->m_iOriginalWidth == 1440 && context->m_iOriginalHeight == 1080)
		{
			context->m_iRotationThreadCount = 4;
			context->m_bIsApplyRotationForOptimizations = FALSE;
		}
		else if (context->m_iOriginalWidth == 800 && context->m_iOriginalHeight == 600)
		{
			context->m_iRotationThreadCount = 4;
			context->m_bIsApplyRotationForOptimizations = FALSE;
		}
		else if (context->m_iOriginalWidth == 720 && context->m_iOriginalHeight == 480)
		{
			context->m_iRotationThreadCount = 4;
			context->m_bIsApplyRotationForOptimizations = FALSE;
		}
		else if (context->m_iOriginalWidth == 640 && context->m_iOriginalHeight == 480)
		{
			context->m_iRotationThreadCount = 4;
			context->m_bIsApplyRotationForOptimizations = FALSE;
		}
		else if ((context->m_iOriginalWidth == 640 && context->m_iOriginalHeight == 480) || (context->m_iOriginalWidth == 320 && context->m_iOriginalHeight == 240))
		{
			context->m_iRotationThreadCount = 4;
			context->m_bIsApplyRotationForOptimizations = FALSE;
		}
		else
		{
			context->m_iRotationThreadCount = 2;
			context->m_bIsApplyRotationForOptimizations = FALSE;
		}

		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("OrgWidth[%d] OrgHeight[%d] ThreadCount [%d] context->m_bIsRotationEnabledForTiledFormat [%d] context->m_bIsApplyRotationForOptimizations[%d]",context->m_iOriginalWidth,context->m_iOriginalHeight,context->m_iRotationThreadCount,context->m_bIsRotationEnabledForTiledFormat,context->m_bIsApplyRotationForOptimizations);
		}

		//For DIVIX Certification : Low resolution quality improvement < 720x400
		if (context->m_iOriginalWidth <= LOW_RESOLUTION_WIDTH && context->m_iOriginalHeight <= LOW_RESOLUTION_HEIGHT)
		{
			context->m_IsVeryLowResolutionOpt = TRUE;
		}

	}
	else // FOXP/FOXB/ECHOP and others
	{
			#ifdef BD_VIDEOROTATION_LINESIZE
				context->m_iRotationThreadCount = 2; // FOXB, ECHOP
				//Disable Backbuffer usage as it give screen noise issue, , use GA memory
				context->m_iRotationGAScalledBuffer_GFX1 = 0;
				context->m_bIsTheadAffinity = TRUE;
				context->m_iCpuCoreCount = 2;
				// Enable/Disable CacheMiss related Optimizations to reduce CPU Usage
				// This required extra memcpy (takes 2 msecs/frame on FOXP, too much on FOXB)
				context->m_bIsCacheMissOptEnabled = FALSE;
				//For Cache Flush Params
				context->m_ihResolution = 1280;
				context->m_ivResolution = 720;
				// For  180 Deg Optimized : FOXP only, try for FOXB also
				context->m_bIs180DegOptimized = FALSE;	
				// FOXP/FOXB, For SkipOptimizations, 3 : skip 1 row/coloum for every 2 rows/column
				// context->m_iSkipFactor 2 : skip every alternate row/column
				context->m_iSkipFactor = 2;
				// For FOXB JAVA0 memory usage for GA, use this when Backbuffer is disabled on FOXB
				if (context->m_iRotationGAScalledBuffer_GFX1 == 0)
				{
					context->m_bIsJava0MemoryUsedForGA = TRUE;
				}
				// To enable skip optimization for FOXP/FOXB
				context->m_bIsOptimized = TRUE;
			#else
				context->m_iRotationThreadCount = 2; // FOXP
				//Disable Backbuffer usage as it give screen noise issue, use GA memory
				context->m_iRotationGAScalledBuffer_GFX1 = 0;
				context->m_bIsTheadAffinity = TRUE;
				context->m_iCpuCoreCount = 4;
				// Enable/Disable CacheMiss related Optimizations to reduce CPU Usage
				// This required extra memcpy (takes 2 msecs/frame on FOXP)
				context->m_bIsCacheMissOptEnabled = FALSE;
				//For Cache Flush Params
				context->m_ihResolution = 1920;
				context->m_ivResolution = 1080;
				// For FOXP 180 Deg Optimized : FOXP only
				context->m_bIs180DegOptimized = TRUE; 	// for TIZEN platform,  enable the 1920x1080 video frame rotation scaling 
				// FOXP/FOXB, For SkipOptimizations, 3 : skip 1 row/coloum for every 2 rows/column
				// context->m_iSkipFactor 2 : skip every alternate row/column
				context->m_iSkipFactor = 3;
				if (context->m_bIsInterlacedScanType == TRUE)
				{
					context->m_iSkipFactor = 2;
				}
				// To enable skip optimization for FOXP/FOXB
				context->m_bIsOptimized = TRUE;				// for TIZEN platform, also enable this 90/270 degreen FHD optimizate.
			#endif

			if (context->m_bIs180DegOptimized == TRUE)
			{
				if (context->m_iSkipFactor == 3)
				{
					context->m_iTargetScaledWidth = 1280;
					context->m_iTargetScaledHeight = 720;
				}
				else if (context->m_iSkipFactor == 2)
				{
					context->m_iTargetScaledWidth = 960;
					context->m_iTargetScaledHeight = 540;
				}
			}

			//For DIVIX Certification : Low resolution quality improvement < 720x400
			if (context->m_iOriginalWidth <= LOW_RESOLUTION_WIDTH && context->m_iOriginalHeight <= LOW_RESOLUTION_HEIGHT)
			{
				context->m_IsVeryLowResolutionOpt = TRUE;
			}

			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("VideoRotation ThreadCount[%d]",context->m_iRotationThreadCount);
			}
	}
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_DecideThreadCountForRotation ------- ");
	}
}

static void t_DecideLineSizeForTiledFormat(struct VideoFrameRotateContext* context, int linesize_open)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_DecideLineSizeForTiledFormat +++++++");
	}
#if 0
	// To Support MSTAR -X10P Tiled Format
	if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
	{
		int codec_id = context->m_iCodecId;

		if ((codec_id == CODEC_ID_H264) ||
		     (codec_id == CODEC_ID_RV10) ||
		     (codec_id == CODEC_ID_RV20) ||
		     (codec_id == CODEC_ID_RV30) ||
		     (codec_id == CODEC_ID_RV40) ||
		     (codec_id == CODEC_ID_VP8))
		{
			if(context->m_iOriginalWidth%TILE_W_SIZE)
			{
				context->m_iLineSizeConvertedFrame = (context->m_iOriginalWidth/TILE_W_SIZE)*TILE_W_SIZE + TILE_W_SIZE;
			}
			else
			{
				context->m_iLineSizeConvertedFrame = context->m_iOriginalWidth;
			}
			/*
			//X12, Tiled disable 90/270 optimzations due to tear image W0000169344
			if (((context->m_iOriginalWidth == 1280) && (context->m_iOriginalHeight == 720)) || (context->m_bIsInterlacedScanType == TRUE))
			{
				context->m_bIsApplyRotationForOptimizations = FALSE;
				context->m_bIsCacheMissOptEnabled = TRUE;
			}*/
			if (context->m_bIsRotationPrintsEnabled)
			{
				//GST_LOG("   codec_id[%d : H264 Or RMVB]  context->m_iLineSizeConvertedFrame[%d]",codec_id, context->m_iLineSizeConvertedFrame);
			}
		}
		else if ((codec_id == CODEC_ID_VC1)   ||
			     (codec_id == CODEC_ID_WMV3) ||
			     (codec_id == CODEC_ID_MPEG1VIDEO) ||
			     (codec_id == CODEC_ID_MPEG2VIDEO) ||
			     (codec_id == CODEC_ID_MPEG4) ||
			     (codec_id == CODEC_ID_H263) ||
			     (codec_id == CODEC_ID_MSMPEG4V3))
		{
			context->m_iLineSizeConvertedFrame = 1920;
			if (context->m_bIsRotationPrintsEnabled)
			{
				//GST_LOG("   codec_id[%d : VC1 Or MPEG1/2/4 Or H263 Or MSMPEG4V3]  context->m_iLineSizeConvertedFrame[%d]",codec_id, context->m_iLineSizeConvertedFrame);
			}
		}
	}
	else // for FOXP/FOXB and Others
	{
		if( (context->m_iOriginalWidth > 1024)&&(context->m_iOriginalWidth<=2048) )
		{
			context->m_iLineSizeConvertedFrame = 2048;
		}
		else
		{
			#ifdef BD_VIDEOROTATION_LINESIZE
				context->m_iLineSizeConvertedFrame = 1024; // FOXB, ECHOP
			#else
				context->m_iLineSizeConvertedFrame = 1024;//(((context->m_iOriginalWidth + 127)>>7)<<7); // FOXP
			#endif
		}
	}
	if (context->m_bIsRotationPrintsEnabled)
	{
		//GST_LOG("t_DecideLineSizeForTiledFormat -------\n\n");
	}
#else
	// For Tizen FOXP Board
	context->m_iLineSizeConvertedFrame = linesize_open;
#if 0
	if( (context->m_iOriginalWidth > 1024)&&(context->m_iOriginalWidth<=2048))
	{
		context->m_iLineSizeConvertedFrame = 2048;
	}
	else
	{
		context->m_iLineSizeConvertedFrame = 1024;
	}
#endif
#endif
}

static gboolean t_InitRotationTables(struct VideoFrameRotateContext* context, int srcw, int srch, int srcLine, int dstw, int dsth, int rx, int ry)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_InitRotationTables +++++++");
	}

	int x, y;
	int dstwc = dstw/2;
	int dsthc = dsth/2;
	int srcLinec = srcLine/2;
	//int srchc = srch/2;
	// 90 Degree Tables - Start
	if (context->m_bIsRotationPrintsEnabled)
	{
	    GST_LOG("srcLine[%d] srcLinec[%d]",srcLine,srcLinec);
	    GST_LOG("##### creating %dx%d-->%dx%d rotation tables(90 degree) #####", srcw, srch, dstw, dsth);
	}

	if (!context->Ty1)
	{
		context->Ty1 = (int*)g_malloc(dsth * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Ty1 Success : Ty1 : %p",context->Ty1);
		}
	}
	if (!context->Ty2)
	{
		context->Ty2 = (int*)g_malloc(dstw * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Ty2 Success : Ty2 : %p",context->Ty2);
		}
	}
	if (!context->Ty3)
	{
		context->Ty3 = (int*)g_malloc(dsth * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Ty3 Success : Ty3 : %p",context->Ty3);
		}
	}
	if (!context->Tuv1)
	{
		context->Tuv1 = (int*)g_malloc(dsthc * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Tuv1 Success : Tuv1 : %p",context->Tuv1);
		}
	}
	if (!context->Tuv2)
	{
		context->Tuv2 = (int*)g_malloc(dstwc * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Tuv2 Success : Tuv2 : %p",context->Tuv2);
		}
	}
	if (!context->Tuv3)
	{
		context->Tuv3 = (int*)g_malloc(dsthc * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Tuv3 Success : Tuv3 : %p",context->Tuv3);
		}
	}

	if (!context->Ty1 || !context->Ty2 || !context->Ty3 || !context->Tuv1 || !context->Tuv2 || !context->Tuv3)
	{
		 GST_LOG("!!!!!!!!ERROR : Unable to Proceed to Calculate t_InitRotationTables 90 Degree Tables !!!!!!!!");
		 return FALSE;
	}


	for(y=0; y<dsth; y++)
	{
	    context->Ty1[y] = y * dstw;
	    context->Ty3[y] = FFMAX(0, FFMIN(y * rx / ry, context->m_iOriginalWidth-1));
	}
	// For Interlaced scan type
	//modified for Interlaced scan type low resolution
	if ((context->m_bIsInterlacedScanType == TRUE) && (context->m_bIsLowResolution == FALSE))
	{
		for(x=0; x<dstw; x++)
		{
			context->Ty2[x] = ((dstw - 1 - x) * rx  / ry + 1)/2;
			context->Ty2[x] = FFMAX(0, FFMIN(context->Ty2[x], context->m_iOriginalHeight/2-1));
			context->Ty2[x] *= 2*srcLine;
		}
	}
	else // For Progressive scan type
	{
		for(x=0; x<dstw; x++)
		{
		    context->Ty2[x] = (dstw - 1 - x) * rx  / ry ;
		    context->Ty2[x] = FFMAX(0, FFMIN(context->Ty2[x], context->m_iOriginalHeight-1));
		    context->Ty2[x] *= srcLine;
		}
	}

	for(y=0; y<dsthc; y++)
	{
	    context->Tuv1[y] = y * dstwc;
	    context->Tuv3[y] = FFMAX(0, FFMIN(y * rx / ry, (context->m_iOriginalWidth/2)-1));
	}
	// For Interlaced scan type
	//modified for Interlaced scan type low resolution
	if ((context->m_bIsInterlacedScanType == TRUE) && (context->m_bIsLowResolution == FALSE))
	{
		for(x=0; x<dstwc; x++)
		{
			context->Tuv2[x] = ((dstwc - 1 - x) * rx / ry + 1)/2;
			context->Tuv2[x] = FFMAX(0, FFMIN(context->Tuv2[x], (context->m_iOriginalHeight/4)-1));
			context->Tuv2[x] *= 2*srcLinec;
		}
	}
	else // For Progressive scan type
	{
		for(x=0; x<dstwc; x++)
		{
		    context->Tuv2[x] = (dstwc - 1 - x) * rx / ry;
		    context->Tuv2[x] = FFMAX(0, FFMIN(context->Tuv2[x], (context->m_iOriginalHeight/2)-1));
		    context->Tuv2[x] *= srcLinec;
		}
	}
	// 90 Degree Tables - Done

	// 270 Degree Tables - Start
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("##### creating %dx%d-->%dx%d rotation tables(270 degree) #####", srcw, srch, dstw, dsth);
	}

	if (!context->Ty4)
	{
		context->Ty4 = (int*)g_malloc(dsth * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Ty4 Success : Ty4 : %p",context->Ty4);
		}
	}
	if (!context->Ty5)
	{
		context->Ty5 = (int*)g_malloc(dstw * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Ty5 Success : Ty5 : %p",context->Ty5);
		}
	}
	if (!context->Ty6)
	{
		context->Ty6 = (int*)g_malloc(dsth * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Ty6 Success : Ty6 : %p",context->Ty6);
		}
	}
	if (!context->Tuv4)
	{
		context->Tuv4 = (int*)g_malloc(dsthc * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Tuv4 Success : Tuv4 : %p",context->Tuv4);
		}
	}
	if (!context->Tuv5)
	{
		context->Tuv5 = (int*)g_malloc(dstwc * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Tuv5 Success : Tuv5 : %p",context->Tuv5);
		}
	}
	if (!context->Tuv6)
	{
		context->Tuv6 = (int*)g_malloc(dsthc * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc Tuv6 Success : Tuv6 : %p",context->Tuv6);
		}
	}

	if (!context->Ty4 || !context->Ty5 || !context->Ty6 || !context->Tuv4 || !context->Tuv5 || !context->Tuv6)
	{
		 GST_LOG("!!!!!!!!ERROR : Unable to Proceed to Calculate t_InitRotationTables 270 Degree Tables !!!!!! ");
		 return FALSE;
	}

	for(y=0; y<dsth; y++)
	{
	    context->Ty4[y] = y * dstw;
	    context->Ty6[y] = (dsth - 1 - y) * rx / ry;
	}
	// For Interlaced scan type
	//modified for Interlaced scan type low resolution
	if ((context->m_bIsInterlacedScanType == TRUE) && (context->m_bIsLowResolution == FALSE))
	{
		for(x=0; x<dstw; x++)
		{
			context->Ty5[x] = (x * rx  / ry + 1)/2;
			context->Ty5[x] *= 2*srcLine;
		}
	}
	else // For Progressive scan type
	{
		for(x=0; x<dstw; x++)
		{
		    context->Ty5[x] = x * rx  / ry ;
		    context->Ty5[x] *= srcLine;
		}
	}

	for(y=0; y<dsthc; y++)
	{
	    context->Tuv4[y] = y * dstwc;
	    context->Tuv6[y] = (dsthc - 1 - y) * rx / ry;
	}
	// For Interlaced scan type
	//modified for Interlaced scan type low resolution
	if ((context->m_bIsInterlacedScanType == TRUE) && (context->m_bIsLowResolution == FALSE))
	{
		for(x=0; x<dstwc; x++)
		{
			context->Tuv5[x] = (x * rx  / ry + 1)/2 ;
			context->Tuv5[x] *= 2*srcLinec;
		}

	}
	else // For Progressive scan type
	{
		for(x=0; x<dstwc; x++)
		{
		    context->Tuv5[x] = x * rx  / ry ;
		    context->Tuv5[x] *= srcLinec;
		}
	}

	// 270 Degree Tables - Done
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_InitRotationTables -------");
	}

	return TRUE;
}

static gboolean t_InitTiledToLinearTables(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_InitTiledToLinearTables +++++++");
	}

	int iPaddingByteOnRight = 0;
	int iPaddingByteOnBottom = 0;
	int iTotalNumOfTilesOnRow = 0;
	int iTotalNumOfTilesOnColumn= 0;
	int ValidHeightOfTile = 0;
	int ValidWidthOfTile = 0;
	int iTileWidth = TILE_W_SIZE;
	int iTileHeight = TILE_H_SIZE;
	int t1, t2;
	int dOffset = 0;
	int sOffset = 0;
	int src_width = context->m_iLineSizeConvertedFrame;
	int src_height = context->m_iOriginalHeight;

	if(!context->TiledToLinearTableY)
	{
		context->TiledToLinearTableY = (int*)g_malloc(context->m_iLineSizeConvertedFrame* src_height* sizeof(int));
		if(context->TiledToLinearTableY)
		{
			memset(context->TiledToLinearTableY,0,context->m_iLineSizeConvertedFrame* src_height* sizeof(int));	
			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("TiledToLinearTableY :  %p",context->TiledToLinearTableY);
			}
		}
		else
		{
			GST_LOG("Failed To Create TiledToLinearTableY");
			return FALSE;
		}
	}
	if(!context->TiledToLinearTableUV)
	{
		context->TiledToLinearTableUV = (int*)g_malloc(context->m_iLineSizeConvertedFrame* (src_height/2) * sizeof(int));
		if(context->TiledToLinearTableUV)
		{
			memset(context->TiledToLinearTableUV,0,context->m_iLineSizeConvertedFrame* (src_height/2)* sizeof(int));
			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("TiledToLinearTableUV :  %p",context->TiledToLinearTableUV);
			}
		}
		else
		{
			//LOG_ERROR("Failed To Create TiledToLinearTableUV");
			return FALSE;
		}
	}

	iPaddingByteOnRight = ((src_width % iTileWidth) > 0) ? (iTileWidth - (src_width % iTileWidth)) : 0;
	iPaddingByteOnBottom = 0;
	iTotalNumOfTilesOnRow = src_width / iTileWidth;
	iTotalNumOfTilesOnColumn = 0;

	// Check boundary (Whether padding used or not..)
	t1 = src_height % iTileHeight;
	t2 = src_height/iTileHeight;

	if (t1 > 0)
	{
		iPaddingByteOnBottom = iTileHeight - t1;
		iTotalNumOfTilesOnColumn = t2 + 1;
	}
	else
	{
		iPaddingByteOnBottom = 0;
		iTotalNumOfTilesOnColumn = t2;
	}

	GST_LOG("Y : srcW[%d]  srcH[%d]  col[%d] row[%d]  B[%d] R[%d]", src_width, src_height, iTotalNumOfTilesOnColumn, iTotalNumOfTilesOnRow, iPaddingByteOnBottom, iPaddingByteOnRight);

	//Y
	int i, j, k, x;
	for(i=0; i<iTotalNumOfTilesOnColumn; i++)
	{
		for(j=0; j<iTotalNumOfTilesOnRow; j++)
		{
			ValidHeightOfTile = iTileHeight;
			if ((i == iTotalNumOfTilesOnColumn-1) && (iPaddingByteOnBottom > 0))
			{
				ValidHeightOfTile = iTileHeight - iPaddingByteOnBottom;
			}

			ValidWidthOfTile = iTileWidth;
			if ((j == iTotalNumOfTilesOnRow-1) && (iPaddingByteOnRight>0))	
			{
				ValidWidthOfTile = iTileWidth - iPaddingByteOnRight;
			}

			dOffset = (j * iTileWidth ) + (i * iTileHeight * src_width);
			sOffset = ((j + (i * iTotalNumOfTilesOnRow)) * iTileWidth * iTileHeight);

			for(k=0; k<ValidHeightOfTile;k++)
			{
				for(x=0; x<ValidWidthOfTile;x++)
				{
					context->TiledToLinearTableY[dOffset+x] = sOffset+x;
				}

				sOffset += iTileWidth;
				dOffset += src_width;
			}
		}
	}

	//UV
	src_width = context->m_iLineSizeConvertedFrame/2;
	src_height = context->m_iOriginalHeight/2;
	iTileWidth = TILE_W_SIZE/2;
	iTileHeight = TILE_H_SIZE;

	iPaddingByteOnRight = ((src_width % iTileWidth) > 0) ? (iTileWidth - (src_width % iTileWidth)) : 0;
	iPaddingByteOnBottom = 0;
	iTotalNumOfTilesOnRow = src_width/iTileWidth;
	iTotalNumOfTilesOnColumn = 0;

	// Check boundary (Whether padding used or not..)
	t1 = src_height % iTileHeight;
	t2 = src_height/iTileHeight;

	if (t1 > 0)
	{
		iPaddingByteOnBottom = iTileHeight - t1;
		iTotalNumOfTilesOnColumn = t2 + 1;
	}
	else
	{
		iPaddingByteOnBottom = 0;
		iTotalNumOfTilesOnColumn = t2;
	}

	GST_LOG("UV: srcW[%d]  srcH[%d]  col[%d] row[%d]  B[%d] R[%d]", src_width, src_height, iTotalNumOfTilesOnColumn, iTotalNumOfTilesOnRow, iPaddingByteOnBottom, iPaddingByteOnRight);

	for(i=0; i<iTotalNumOfTilesOnColumn; i++)
	{
		for(j=0; j<iTotalNumOfTilesOnRow; j++)
		{
			ValidHeightOfTile = iTileHeight;
			if ((i == iTotalNumOfTilesOnColumn-1) && (iPaddingByteOnBottom > 0))
			{
				ValidHeightOfTile = iTileHeight - iPaddingByteOnBottom;
			}

			ValidWidthOfTile = iTileWidth;
			if ((j == iTotalNumOfTilesOnRow-1) && (iPaddingByteOnRight>0))
			{
				ValidWidthOfTile = iTileWidth - iPaddingByteOnRight;
			}

			dOffset = (j * iTileWidth ) + (i * iTileHeight * src_width);
			sOffset = ((j + (i * iTotalNumOfTilesOnRow)) * iTileWidth * iTileHeight);

			for(k=0; k<ValidHeightOfTile;k++)
			{
				for(x=0; x<ValidWidthOfTile;x++)
				{
					context->TiledToLinearTableUV[dOffset+x] = (sOffset+x)*2;
				}

				sOffset += iTileWidth;
				dOffset += src_width;
			}

		}
	}

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_InitTiledToLinearTables -------");
	}
	return TRUE;
}

static gboolean t_initRotatedTiledTables(struct VideoFrameRotateContext* context, int dstw, int dsth)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_initRotatedTiledTables +++++++");
	}
	int x,y;
	int dstY = 0;

	// 90 Degree Rotation Tiled Rotation Tables - Starts
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_initRotatedTiledTables_90 +++++++");
	}
	if (context->m_bIsApplyRotationForOptimizations == TRUE)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("   t_initRotatedTiledTables_90 : Optimized");
		}

		int pos = 0;
		int pos1 = 0;
		int prev_pos = 0;
		int RotateTableReads = 0;

		for(x=0; x<dstw; x++)
		{
			dstY= context->Ty2[x];
			for(y=0; y<dsth; y++)
			{
				pos1 = context->TiledToLinearTableY[dstY+ context->Ty3[y]];
				pos = (pos1 & 0x1FFFF8)>>3;
				if(prev_pos != pos || (pos == 0 && prev_pos == 0))
				{
					RotateTableReads++;
				}
				prev_pos = pos;
			}

			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("   context->m_iRotateTableReadCountSize_90 :  %d",RotateTableReads);
			}
			
			break;
		}

	    // need to read 8 bytes every time, hence maximum reads can go to OrigWidth/8
	     if (RotateTableReads > context->m_iOriginalWidth/8)
	     {
	           RotateTableReads = context->m_iOriginalWidth/8;

		    if (context->m_bIsRotationPrintsEnabled)
		    {
	          	 GST_LOG("   Read Size Crossed Limit : Modied context->m_iRotateTableReadCountSize_90 :  %d",RotateTableReads);
		    }
	     }

	    context->m_iRotateTableReadCountSize_90 = RotateTableReads;

	    if(!context->TiledRotatedTableY90Deg)
	    {
	    		context->TiledRotatedTableY90Deg = (int*)g_malloc(dstw* context->m_iRotateTableReadCountSize_90* sizeof(int));
			if (context->TiledRotatedTableY90Deg)
			{
				memset(context->TiledRotatedTableY90Deg,0,dstw* context->m_iRotateTableReadCountSize_90* sizeof(int));
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("   TiledRotatedTableY90Deg :  %p",context->TiledRotatedTableY90Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR TiledRotatedTableY90Deg  FAILED:  %p",context->TiledRotatedTableY90Deg);
				return FALSE;
			}
	    }
	    if(!context->TiledRotatedTableUV90Deg)
	    {
	    	context->TiledRotatedTableUV90Deg = (int*)g_malloc(dstw* context->m_iRotateTableReadCountSize_90 * sizeof(int));
			if (context->TiledRotatedTableUV90Deg)
			{
				memset(context->TiledRotatedTableUV90Deg,0,dstw* context->m_iRotateTableReadCountSize_90* sizeof(int));
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("   TiledRotatedTableUV90Deg :  %p",context->TiledRotatedTableUV90Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR TiledRotatedTableUV90Deg FAILED:  %p",context->TiledRotatedTableUV90Deg);
				return FALSE;
			}
	    }

		dstY = 0;
	   	pos = 0;
		pos1 = 0;
		prev_pos = 0;
		int t1 = 0;
		int idx_tmp = 0;
		int count = 0;

		//Y
		int *ptr1 = context->TiledRotatedTableY90Deg;
		for(x=0; x<dstw; x++)
		{
			dstY= context->Ty2[x];
			count = 0;

			for(y=0; y<dsth; y++)
			{
				pos1 = context->TiledToLinearTableY[dstY+ context->Ty3[y]];
				pos = (pos1 & 0x1FFFF8)>>3;
				idx_tmp = (pos1 % 8)<<3;

				if(prev_pos != pos || (pos == 0 && prev_pos == 0))
				{
					if(count >= context->m_iRotateTableReadCountSize_90)
					{
						GST_LOG("count[%d] idx_tmp[%d] ",count,idx_tmp);
					}
					else
					{
						*ptr1++ = pos;
						count++;
					}
				}
				else
				{
					t1 = idx_tmp;
				}

				prev_pos = pos;
			}

		}

		//UV
		int *ptr2 = context->TiledRotatedTableUV90Deg;
		for(x=0; x<(dstw/2); x++)
		{
			dstY = context->Tuv2[x];
			count = 0;

			for(y=0; y<dsth/2; y++)
			{
				pos1 = context->TiledToLinearTableUV[dstY+context->Tuv3[y]];
				pos = (pos1 & 0x1FFFF8)>>3;
				idx_tmp = (pos1 % 8)<<3;

				if((prev_pos != pos || (pos == 0 && prev_pos == 0)))
				{
					if(count >= context->m_iRotateTableReadCountSize_90)
					{
						GST_LOG("count[%d] idx_tmp[%d] ",count,idx_tmp);
					}
					else
					{
						*ptr2++ = pos;
						count++;
					}
				}
				else
				{
					t1 = idx_tmp;
				}

				prev_pos = pos;
			}
		}
	}
	else
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("   t_initRotatedTiledTables_90 : Other Resolutions");
		}
		
		if(!context->TiledRotatedTableY90Deg)	
		{
			context->TiledRotatedTableY90Deg = (int*)g_malloc(dstw* dsth* sizeof(int));
			if (context->TiledRotatedTableY90Deg)
			{
				memset(context->TiledRotatedTableY90Deg,0,dstw* dsth* sizeof(int));
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("   TiledRotatedTableY90Deg :  %p",context->TiledRotatedTableY90Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR TiledRotatedTableY90Deg FAILED:  %p",context->TiledRotatedTableY90Deg);
				return FALSE;
			}
		}

		if(!context->TiledRotatedTableUV90Deg)
		{
			context->TiledRotatedTableUV90Deg = (int*)g_malloc(dstw* dsth/2 * sizeof(int));
			if (context->TiledRotatedTableUV90Deg)
			{
				memset(context->TiledRotatedTableUV90Deg,0,dstw* (dsth/2)* sizeof(int));
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("   TiledRotatedTableUV90Deg :  %p",context->TiledRotatedTableUV90Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR TiledRotatedTableUV90Deg FAILED:  %p",context->TiledRotatedTableUV90Deg);
				return FALSE;
			}
		}

		//Y
		int *ptr1 = context->TiledRotatedTableY90Deg;
		for(x=0; x<dstw; x++)
		{
			dstY= context->Ty2[x];
			for(y=0; y<dsth; y++)
			{
				*ptr1++= context->TiledToLinearTableY[dstY+ context->Ty3[y]];
			}
		}

		//UV
		int *ptr2 = context->TiledRotatedTableUV90Deg;
		for(x=0; x<(dstw/2); x++)
		{
			dstY = context->Tuv2[x];
			for(y=0; y<dsth/2; y++)
			{
				*ptr2++ = context->TiledToLinearTableUV[dstY+context->Tuv3[y]]; 
			}
		}

	}

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_initRotatedTiledTables_90 -------");
	}
	// 90 Degree Rotation Tiled Rotation Tables - Done


	// 270 Degree Rotation Tiled Rotation Tables - Starts
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_initRotatedTiledTables_270 +++++++");
	}

	if (context->m_bIsApplyRotationForOptimizations == TRUE)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("   t_initRotatedTiledTables_270 : Optimized");
		}
		
		int pos = 0;
		int pos1 = 0;
		int prev_pos = 0;
		int RotateTableReads = 0;

		for(x=0; x<dstw; x++)
		{
			dstY= context->Ty5[x];
			for(y=0; y<dsth; y++)
			{
				pos1 = context->TiledToLinearTableY[dstY+ context->Ty6[y]];
				pos = (pos1 & 0x1FFFF8)>>3;
				if(prev_pos != pos || (pos == 0 && prev_pos == 0))
				{
					RotateTableReads++;
				}
				prev_pos = pos;
			}

			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("   context->m_iRotateTableReadCountSize_270 :  %d",RotateTableReads);
			}

			break;
		}
		// need to read 8 bytes every time, hence maximum reads can go to OrigWidth/8
		if (RotateTableReads > context->m_iOriginalWidth/8)
		{
			RotateTableReads = context->m_iOriginalWidth/8;

			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("   Read Size Crossed Limit : Modied context->m_iRotateTableReadCountSize_270 :  %d",RotateTableReads);
			}
		}

		context->m_iRotateTableReadCountSize_270 = RotateTableReads;

		if(!context->TiledRotatedTableY270Deg)	
		{
			context->TiledRotatedTableY270Deg = (int*)g_malloc(dstw* context->m_iRotateTableReadCountSize_270* sizeof(int));
			if (context->TiledRotatedTableY270Deg)
			{
				memset(context->TiledRotatedTableY270Deg,0,dstw* context->m_iRotateTableReadCountSize_270* sizeof(int));
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("   TiledRotatedTableY270Deg :  %p",context->TiledRotatedTableY270Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR TiledRotatedTableY270Deg FAILED :  %p",context->TiledRotatedTableY270Deg);
				return FALSE;
			}
		}
		if(!context->TiledRotatedTableUV270Deg)
		{
			context->TiledRotatedTableUV270Deg = (int*)g_malloc(dstw* context->m_iRotateTableReadCountSize_270 * sizeof(int));
			if (context->TiledRotatedTableUV270Deg)
			{
				memset(context->TiledRotatedTableUV270Deg,0,dstw* context->m_iRotateTableReadCountSize_270* sizeof(int));
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("   TiledRotatedTableUV270Deg :  %p",context->TiledRotatedTableUV270Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR TiledRotatedTableUV270Deg FAILED:  %p",context->TiledRotatedTableUV270Deg);
				return FALSE;
			}
		}

		dstY = 0;
	   	pos = 0;
		pos1 = 0;
		prev_pos = 0;
		int t1 = 0;
		int idx_tmp = 0;
		int count = 0;

		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("   Start 270 Y Tables");
		}
		//Y
		int *ptr1 = context->TiledRotatedTableY270Deg;
		for(x=0; x<dstw; x++)
		{
			dstY= context->Ty5[x];
			count = 0;

			for(y=0; y<dsth; y++)
			{
				pos1 = context->TiledToLinearTableY[dstY+ context->Ty6[y]];
				pos = (pos1 & 0x1FFFF8)>>3;
				idx_tmp = (pos1 % 8)<<3;

				if(prev_pos != pos || (pos == 0 && prev_pos == 0))
				{
					if(count >= context->m_iRotateTableReadCountSize_270)
					{
						GST_LOG("count[%d] idx_tmp[%d] ",count,idx_tmp);
					}
					else
					{
						*ptr1++ = pos;
						count++;
					}
				}
				else
				{
					t1 = idx_tmp;
				}

				prev_pos = pos;
			}
		}

		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("   END 270 Y Tables");
		}

		dstY = 0;
		t1 = 0;
		prev_pos = 0;
		pos = 0;
		pos1 = 0;
		idx_tmp = 0;

		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("   Start 270 UV Tables");
		}
		//UV
		int *ptr2 = context->TiledRotatedTableUV270Deg;
		for(x=0; x<(dstw/2); x++)
		{
			dstY = context->Tuv5[x];
			count = 0;

			for(y=0; y<dsth/2; y++)
			{
				pos1 = context->TiledToLinearTableUV[dstY+context->Tuv6[y]];
				pos = (pos1 & 0x1FFFF8)>>3;
				idx_tmp = (pos1 % 8)<<3;

				if((prev_pos != pos || (pos == 0 && prev_pos == 0)))
				{
					if(count >= context->m_iRotateTableReadCountSize_270)
					{
						GST_LOG("count[%d] idx_tmp[%d]\n",count,idx_tmp);
					}
					else
					{
						*ptr2++ = pos;
						count++;
					}
				}
				else
				{
					t1 = idx_tmp;
				}
				prev_pos = pos;
			}
		}
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("   END 270 UV Tables");
		}
	}
	else
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("   t_initRotatedTiledTables_270 : Other Resolutions");
		}
		if(!context->TiledRotatedTableY270Deg)	
		{
			context->TiledRotatedTableY270Deg = (int*)g_malloc(dstw* dsth* sizeof(int));
			if (context->TiledRotatedTableY270Deg)
			{
				memset(context->TiledRotatedTableY270Deg,0,dstw* dsth* sizeof(int));
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("   TiledRotatedTableY270Deg :  %p",context->TiledRotatedTableY270Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR TiledRotatedTableY270Deg FAILED:  %p",context->TiledRotatedTableY270Deg);
				return FALSE;
			}
		}

		if(!context->TiledRotatedTableUV270Deg)
		{
			context->TiledRotatedTableUV270Deg = (int*)g_malloc(dstw* dsth/2 * sizeof(int));
			if (context->TiledRotatedTableUV270Deg)
			{
				memset(context->TiledRotatedTableUV270Deg,0,dstw* (dsth/2)* sizeof(int));
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("   TiledRotatedTableUV270Deg :  %p",context->TiledRotatedTableUV270Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR TiledRotatedTableUV270Deg FAILED:  %p",context->TiledRotatedTableUV270Deg);
				return FALSE;
			}
		}

		//Y
		int *ptr1 = context->TiledRotatedTableY270Deg;
		for(x=0; x<dstw; x++)
		{
			dstY= context->Ty5[x];
			for(y=0; y<dsth; y++)
			{
				*ptr1++= context->TiledToLinearTableY[dstY+ context->Ty6[y]];
			}
		}

		//UV
		int *ptr2 = context->TiledRotatedTableUV270Deg;
		for(x=0; x<(dstw/2); x++)
		{
			dstY = context->Tuv5[x];
			for(y=0; y<dsth/2; y++)
			{
				*ptr2++ = context->TiledToLinearTableUV[dstY+context->Tuv6[y]]; 
			}
		}

	}
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_initRotatedTiledTables_270 -------");
	}
	// 270 Degree Rotation Tiled Rotation Tables - Done

	// 180 Degree Rotation Tiled Rotation Tables - Starts
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_initRotatedTiledTables_180 +++++++");
	}
	if(!context->TiledRotatedTable180Deg)
	{
		context->TiledRotatedTable180Deg = (int*)g_malloc(context->m_iOriginalHeight * sizeof(int));
		if (context->TiledRotatedTable180Deg)
		{
			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("  Alloc TiledRotatedTable180Deg Success : TiledRotatedTable180Deg : %p",context->TiledRotatedTable180Deg);
			}
		}
		else
		{
			GST_LOG("  Alloc TiledRotatedTable180Deg FAILED : TiledRotatedTable180Deg : %p",context->TiledRotatedTable180Deg);
			// Do Error handling
			return FALSE;
		}
	}

	if (context->TiledRotatedTable180Deg)
	{
		int* tempYUV = context->TiledRotatedTable180Deg;
		int tileOffset = 0;

		if (context->m_bIsInterlacedScanType == FALSE)
		{
			for(y=0; y<(context->m_iOriginalHeight/TILE_H_SIZE); y++)
			{
				for(x=0;x<TILE_H_SIZE;x++)
				{
					*tempYUV++ = x * TILE_W_SIZE + tileOffset;
				}

				tileOffset += TILE_H_SIZE*context->m_iLineSizeConvertedFrame;
			}

			if(context->m_iOriginalHeight%TILE_H_SIZE)
			{
				for(x=0;x<(context->m_iOriginalHeight%TILE_H_SIZE);x++)
				{
					*tempYUV++ = x * TILE_W_SIZE + tileOffset;
				}
			}
		}
		else
		{
			for(y=0; y<(context->m_iOriginalHeight/TILE_H_SIZE); y++)
			{
				for(x=0;x<TILE_H_SIZE;x+=2)
				{
					*tempYUV++ = x * TILE_W_SIZE + tileOffset;
					*tempYUV++ = x * TILE_W_SIZE + tileOffset;
				}

				tileOffset += TILE_H_SIZE*context->m_iLineSizeConvertedFrame;
			}
			if(context->m_iOriginalHeight%TILE_H_SIZE)
			{
				for(x=0;x<((context->m_iOriginalHeight%TILE_H_SIZE)-2);x+=2)
				{
					*tempYUV++ = x * TILE_W_SIZE + tileOffset;
					*tempYUV++ = x * TILE_W_SIZE + tileOffset;
				}
				if(x+1<(context->m_iOriginalHeight%TILE_H_SIZE))
				{
					*tempYUV++ = x * TILE_W_SIZE + tileOffset;
					*tempYUV++ = x * TILE_W_SIZE + tileOffset;
				}else{
					*tempYUV++ = x * TILE_W_SIZE + tileOffset;
				}
			}
		}
	}
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_initRotatedTiledTables_180 -------");
	}
	// 180 Degree Rotation Tiled Rotation Tables - Done
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_initRotatedTiledTables -------");
	}
	return TRUE;
}

static void t_FreeRotationTables(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeRotationTables +++++++");
	}

	if(context->Ty1)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Ty1 Success : Ty1 : %p",context->Ty1);
		}
		g_free(context->Ty1);
		context->Ty1 = NULL;
	}
	if(context->Ty2)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Ty2 Success : Ty2 : %p",context->Ty2);
		}
		g_free(context->Ty2);
		context->Ty2 = NULL;
	}
	if(context->Ty3)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Ty3 Success : Ty3 : %p",context->Ty3);
		}
		g_free(context->Ty3);
		context->Ty3 = NULL;
	}
	if(context->Tuv1)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Tuv1 Success : Tuv1 : %p",context->Tuv1);
		}
		g_free(context->Tuv1);
		context->Tuv1 = NULL;
	}
	if(context->Tuv2)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Tuv2 Success : Tuv2 : %p",context->Tuv2);
		}
		g_free(context->Tuv2);
		context->Tuv2 = NULL;
	}
	if(context->Tuv3)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Tuv3 Success : Tuv3 : %p",context->Tuv3);
		}
		g_free(context->Tuv3);
		context->Tuv3 = NULL;
	}
	if(context->Ty4)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Ty4 Success : Ty4 : %p",context->Ty4);
		}
		g_free(context->Ty4);
		context->Ty4 = NULL;
	}
	if(context->Ty5)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Ty5 Success : Ty5 : %p",context->Ty5);
		}
		g_free(context->Ty5);
		context->Ty5 = NULL;
	}
	if(context->Ty6)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Ty6 Success : Ty6 : %p",context->Ty6);
		}
		g_free(context->Ty6);
		context->Ty6 = NULL;
	}
	if(context->Tuv4)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Tuv4 Success : Tuv4 : %p",context->Tuv4);
		}
		g_free(context->Tuv4);
		context->Tuv4 = NULL;
	}
	if(context->Tuv5)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Tuv5 Success : Tuv5 : %p",context->Tuv5);
		}
		g_free(context->Tuv5);
		context->Tuv5 = NULL;
	}
	if(context->Tuv6)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Tuv6 Success : Tuv6 : %p",context->Tuv6);
		}
		g_free(context->Tuv6);
		context->Tuv6 = NULL;
	}

	context->m_bIsRotationTableInit = FALSE;
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeRotationTables -------");
	}
}

static void t_FreeTiledToLinearTables(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeTiledToLinearTables +++++++");
	}

	if(context->TiledToLinearTableY)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Tiled Y Table Success : TiledToLinearTableY : %p",context->TiledToLinearTableY);
		}
		g_free(context->TiledToLinearTableY);
		context->TiledToLinearTableY = NULL;
	}
	if(context->TiledToLinearTableUV)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free Tiled UV Table Success : TiledToLinearTableUV : %p",context->TiledToLinearTableUV);
		}
		g_free(context->TiledToLinearTableUV);
		context->TiledToLinearTableUV = NULL;
	}

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_FreeTiledToLinearTables -------");
	}
}

static gboolean t_CreateRotationTables(struct VideoFrameRotateContext* context, int linesize_open)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_CreateRotationTables +++++++");
	}

	gboolean RetStatus = TRUE;
	int X, Y;
	int iTargetRotationWidth = 0;
	int iTargetRotationHeight = 0;

	X = MAX(context->m_iOriginalWidth, context->m_iOriginalHeight);
	Y = MIN(context->m_iOriginalWidth, context->m_iOriginalHeight);

	iTargetRotationWidth = context->m_iOriginalHeight * Y / X;
	iTargetRotationHeight = context->m_iOriginalWidth * Y / X;
	iTargetRotationWidth -= (iTargetRotationWidth&0x1);
	iTargetRotationHeight -= (iTargetRotationHeight&0x1);

	//FOXP 90/270 Deg skip Optimized
	if(context->m_bIsOptimized == TRUE && context->m_iOriginalWidth == 1920 && context->m_iOriginalHeight == 1080)
	{
		iTargetRotationWidth = 404;
		iTargetRotationHeight = 720;
		X = X*3;
		Y = Y*2;
	}

	//To handle resolution 1280x1080, 1440x1080 -> scaled resolution will be 910x1080, 810x1080
	if ((iTargetRotationWidth*iTargetRotationHeight) > (MAX_SUPPORTED_SCALED_WIDTH*MAX_SUPPORTED_SCALED_HEIGHT))
	{
		X = X*3;
		Y = Y*2;
		iTargetRotationWidth = context->m_iOriginalHeight * Y / X;
		iTargetRotationHeight = context->m_iOriginalWidth * Y / X;
		iTargetRotationWidth -= (iTargetRotationWidth&0x1);
		iTargetRotationHeight -= (iTargetRotationHeight&0x1);
	}
	// only or very low resolution files like <176x144 to improve rotation quality, just swap width and height
	if (context->m_IsVeryLowResolutionOpt == TRUE)
	{
		X = 1;
		Y = 1;
		iTargetRotationWidth = context->m_iOriginalHeight;
		iTargetRotationHeight = context->m_iOriginalWidth;
		iTargetRotationWidth -= (iTargetRotationWidth&0x1);
		iTargetRotationHeight -= (iTargetRotationHeight&0x1);
		
		// X12 side line video garbage issue for 90/270
		if ((context->m_bIsRotationEnabledForTiledFormat == TRUE) && (iTargetRotationWidth>240))
		{
			iTargetRotationWidth -= MAX_SKIP_LENGTH;
		}
	}

	context->m_iTableWidth_90_270 = iTargetRotationWidth;
	context->m_iTableHeight_90_270 = iTargetRotationHeight;

	// Calculate Line Size for X10P or Echop/Others
	t_DecideLineSizeForTiledFormat(context,linesize_open); // got m_iLineSizeConvertedFrame
	
	if(context->m_iLineSizeConvertedFrame == 0){
		RetStatus = FALSE;
		GST_LOG("!!!!!context->m_iLineSizeConvertedFrame is ZERO !!!!!!!! ");
		return RetStatus;
	}

	if (!context->m_bIsRotationTableInit)
	{
		RetStatus = t_InitRotationTables(context, context->m_iOriginalWidth, context->m_iOriginalHeight, context->m_iLineSizeConvertedFrame, iTargetRotationWidth, iTargetRotationHeight, X, Y);
		if (RetStatus == TRUE)
		{
			//To Support MSTAR -X10P Tiled Format : Init & Free original Tiled Tables which are not required any more
			if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
			{
				context->m_iScaledWidth_90_270 = iTargetRotationWidth;
				context->m_iScaledHeight_90_270 = iTargetRotationHeight;

				// Create Tiled To Linear Tables
				if (t_InitTiledToLinearTables(context) == TRUE)
				{
					// Create Rotated Tiled Tables for 90 Degree
					if (t_initRotatedTiledTables(context, iTargetRotationWidth, iTargetRotationHeight) == FALSE)
					{
						GST_LOG("!!!!!ERROR t_initRotatedTiledTables  FAILED !!!!!!!!");
						RetStatus = FALSE;
					}
				}
				else
				{
					GST_LOG("!!!!!ERROR t_InitTiledToLinearTables  FAILED !!!!!!!!");
					RetStatus = FALSE;
				}
				//Free Rotation Tables Like Ty1, Ty2, ... which are not required for MSTAR-X10P Platform
				t_FreeRotationTables(context);
				//Free Tiled To Linear Tables
				t_FreeTiledToLinearTables(context);
			}
			context->m_bIsRotationTableInit = TRUE;
		}
		else
		{
			GST_LOG("!!!!!t_CreateRotationTables  FAILED !!!!!!!!");
			return RetStatus;
		}
	}

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_CreateRotationTables -------");
	}
	return RetStatus;
}

static gboolean t_CreateGAMemory(struct VideoFrameRotateContext* context)
{
	gboolean RetVal = TRUE;
	unsigned int nVideoMemorySize = 0;

	// FOXB
	if (context->m_bIsJava0MemoryUsedForGA == TRUE)
	{
		// This function gets 15MB of GA Cacheble memory from JAVA0 region
		// Use 4 MB for rotation from 15 MB
		// TODO: needs to replace this function call with the proper function for tizen platform.
		RetVal = FALSE;//CPlatformInterface::GetInstance()->GetBDCachebleGAMemory(&context->m_pGAMemory, &context->m_hUmpHandle);
		if (RetVal == TRUE)
		{
			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("GetBDCachebleGAMemory:Success : context->m_pGAMemory[%p]", context->m_pGAMemory);
			}
			nVideoMemorySize = 4*1024*1024;
			if (context->m_pGAMemory)
			{
				memset(context->m_pGAMemory, 0, nVideoMemorySize);
			}
		}
		else
		{
			GST_LOG(" GetBDCachebleGAMemory : FAILED  context->m_pGAMemory[%p]", context->m_pGAMemory);
			context->m_pGAMemory = NULL;
			return FALSE;
		}
	}
	else // FOXP/X12
	{
		//Create GA memory 4MB for Scaled Y and CbCr Buffers
		context->m_iBitDepth = 2;
		//RetVal = FALSE;//CPlatformInterface::GetInstance()->GetGAVideoRotationMemory(&context->m_pGAMemory, &context->m_pGAMemoryHandle, PANEL_WIDTH, PANEL_HEIGHT, context->m_iBitDepth);
#ifdef  ENABLE_TBM
		/* get buffer manager */
		#ifdef ENABLE_LOCAL_ROTATE_BUFFER
			RetVal = allocate_rotate_local_buffer(context);
		#endif
			RetVal = allocate_rotate_tbm_buffer(context); // alloc AP&MP.
		
#else
		context->m_pGAMemory = (unsigned char *)g_malloc(PANEL_WIDTH*PANEL_HEIGHT*context->m_iBitDepth*sizeof(unsigned char));
		RetVal = (context->m_pGAMemory)?TRUE:FALSE;
#endif

		if (RetVal == TRUE)
		{
			nVideoMemorySize = PANEL_WIDTH*PANEL_HEIGHT*context->m_iBitDepth;
			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("SdGfx_AllocBuffer:Success : context->m_pGAMemory : %p Size:%d", context->m_pGAMemory, nVideoMemorySize);
			}
#ifdef  ENABLE_TBM
		#ifdef ENABLE_LOCAL_ROTATE_BUFFER
			if(context->pLocalRotateBuffer_Y && context->pLocalRotateBuffer_CbCr){
				memset (context->pLocalRotateBuffer_Y, 0x0, PANEL_WIDTH*PANEL_HEIGHT);
				memset (context->pLocalRotateBuffer_CbCr, 0x0, PANEL_WIDTH*PANEL_HEIGHT);
			}
		#endif
			if(context->bo_handle_AP.ptr && context->bo_handle_AP_CbCr.ptr)
			{
				GST_LOG("VF: TBM alloc AP memory success & memset 0 !  ");
				/* memset 0x0 */
				memset (context->bo_handle_AP.ptr, 0x0, PANEL_WIDTH*PANEL_HEIGHT);
				memset (context->bo_handle_AP_CbCr.ptr, 0x0, PANEL_WIDTH*PANEL_HEIGHT);
			}
#else
			if (context->m_pGAMemory)
			{
				memset(context->m_pGAMemory, 0, nVideoMemorySize);
			}
#endif
		}
		else
		{
			GST_LOG(" SdGfx_AllocBuffer : FAILED  context->m_pGAMemory : %p", context->m_pGAMemory);
#ifdef ENABLE_TBM
		#ifdef ENABLE_LOCAL_ROTATE_BUFFER
			context->pLocalRotateBuffer_Y = NULL;
			context->pLocalRotateBuffer_CbCr = NULL;
		#endif
			context->bo_AP = NULL;
			context->bo_AP_CbCr = NULL;
#else
			context->m_pGAMemory = NULL;
#endif
			return FALSE;
		 }
	}

	// Create System Memory
	if (context->m_bIsCacheMissOptEnabled == TRUE)
	{
		// Create System Output Y Buffer
		context->m_pSysOutputYBuffer = (unsigned char*)g_malloc(PANEL_WIDTH*PANEL_HEIGHT*sizeof(unsigned char));
		if (context->m_pSysOutputYBuffer != NULL)
		{
			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG(" ALLOC Success : context->m_pSysOutputYBuffer[%p]", context->m_pSysOutputYBuffer);
			}
			memset(context->m_pSysOutputYBuffer, 0, PANEL_WIDTH*PANEL_HEIGHT*sizeof(unsigned char));
		}
		else
		{
			GST_LOG(" ALLOC FAILED  context->m_pSysOutputYBuffer[%p]", context->m_pSysOutputYBuffer);
			return FALSE;
		}
		// Create System Output CbCr Buffer
		context->m_pSysOutputCbCrBuffer = (unsigned char*)g_malloc(PANEL_WIDTH*(PANEL_HEIGHT/2)*sizeof(unsigned char));
		if (context->m_pSysOutputCbCrBuffer != NULL)
		{
			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG(" ALLOC Success : context->m_pSysOutputCbCrBuffer[%p]", context->m_pSysOutputCbCrBuffer);
			}
			memset(context->m_pSysOutputCbCrBuffer, 0, PANEL_WIDTH*(PANEL_HEIGHT/2)*sizeof(unsigned char));
		}
		else
		{
			GST_LOG(" ALLOC FAILED  context->m_pSysOutputCbCrBuffer[%p]", context->m_pSysOutputCbCrBuffer);
			return FALSE;
		}
	}

	// For MSTAR-X10P/X12
	if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
	{
		context->m_IsEnableGAMemoryForTiledRotation = TRUE;
		//For Full HD - Inplace YUYV conversion
		if (context->m_bIsApplyRotationForOptimizations == TRUE)
		{
			// Use GA memory to write final YUYV output
		 	context->m_pBackBuffer = context->m_pGAMemory;
			context->m_pGARotateBuffer = context->m_pGAMemory;

			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("BackBuffer context->m_pBackBuffer : %p",context->m_pBackBuffer);
			}
		}
		else // For Other Resolutions - No Inplace & extrenal YUYV conversion
		{
			 // Use local system memory to write rotation output due to performance reasons.
			context->m_pGAScaledYBuffer = context->m_pSysOutputYBuffer;
			context->m_pGAScaledCbCrBuffer = context->m_pSysOutputCbCrBuffer;
			// Use GA memory to write final YUYV output
			context->m_pBackBuffer = context->m_pGAMemory;
			context->m_pGARotateBuffer = context->m_pGAMemory;

			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("context->m_pGAScaledYBuffer[%p]   context->m_pGAScaledCbCrBuffer[%p]   context->m_pBackBuffer[%p]",context->m_pGAScaledYBuffer, context->m_pGAScaledCbCrBuffer, context->m_pBackBuffer);
			}
		}
	}
	else  //FOXP/FOXB/ECHOP
	{
#ifdef  ENABLE_TBM
		#ifdef ENABLE_LOCAL_ROTATE_BUFFER
			context->m_pGAScaledYBuffer = context->pLocalRotateBuffer_Y;
			context->m_pGAScaledCbCrBuffer = context->pLocalRotateBuffer_CbCr;
		#else
			context->m_pGAScaledYBuffer = context->bo_handle_AP.ptr;
			context->m_pGAScaledCbCrBuffer = context->bo_handle_AP_CbCr.ptr;
		#endif
#else
		context->m_pGAScaledYBuffer = context->m_pGAMemory;
		context->m_pGAScaledCbCrBuffer = context->m_pGAScaledYBuffer + (PANEL_WIDTH*PANEL_HEIGHT);
#endif
	}

	// Assign to Local pointers
	//X10P/X12
	if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
	{
		// These pointers can have either GA memory or Local System Memory based on above judgement
		context->m_pOutputYBufPtr = context->m_pGAScaledYBuffer;
		context->m_pOutputCbCrBufPtr = context->m_pGAScaledCbCrBuffer;
	}
	else // FOXP/FOXB/ECHOP
	{
		if (context->m_bIsCacheMissOptEnabled == TRUE)
		{
			// Use local system memory to write rotation output due to performance reasons.
			context->m_pOutputYBufPtr = context->m_pSysOutputYBuffer;
			context->m_pOutputCbCrBufPtr = context->m_pSysOutputCbCrBuffer;
		}
		else
		{
			// These pointers can have either GA memory or Local System Memory based on above judgement
			context->m_pOutputYBufPtr = context->m_pGAScaledYBuffer;
			context->m_pOutputCbCrBufPtr = context->m_pGAScaledCbCrBuffer;
		}
	}

	context->m_bIsRotateBufferAllocated = TRUE;
	return TRUE;
}

static gboolean t_CreateBackBufferMemory(struct VideoFrameRotateContext* context)
{
	#ifdef SUPPORT_SW_VIDEO_DECODER
		uint32_t SdRet = SD_OK;
		SdGfx_Status_t Status;
		SdGfx_Settings_t Settings;
		memset(&Settings, 0x0, sizeof(SdGfx_Settings_t));
		memset(&Status, 0x0, sizeof(SdGfx_Status_t));

		SdRet = SdGfx_Get(SD_GFX1, &Settings);
		if(SdRet != SD_OK)
		{
			//LOG_INFO("SdGfx_Get Error : %d ",SdRet);
		}

		Settings.bCachedOn = TRUE;
		SdRet = SdGfx_Set(SD_GFX1, &Settings);
		if(SdRet != SD_OK)
		{
			//LOG_INFO("SdGfx_Set Error : %d ",SdRet);
		}

		SdRet = SdGfx_GetStatus(SD_GFX1, SD_GFX_STATUS_PLANE_INFO, &Status);
		if(SdRet != SD_OK)
		{
			//LOG_INFO("SdGfx_GetStatus Error!!!");
			return FALSE;
		}

		// For MSTAR-X10P
		if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
		{
			//For Full HD - Inplace YUYV conversion
			if (context->m_bIsApplyRotationForOptimizations == TRUE)
			{
			 	context->m_pBackBuffer = (unsigned char*)Status.GfxInfo.sPlane.pBackBuffer[0];

				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("BackBuffer context->m_pBackBuffer : %p",context->m_pBackBuffer);
				}
			}
			else // For Other Resolutions - No Inplace & extrenal YUYV conversion
			{
				context->m_pGAScaledYBuffer = (unsigned char*)Status.GfxInfo.sPlane.pBackBuffer[0];
				context->m_pGAScaledCbCrBuffer = context->m_pGAScaledYBuffer + (context->m_iScaledWidth*context->m_iScaledHeight);
				context->m_pBackBuffer = context->m_pGAScaledCbCrBuffer+ (context->m_iScaledWidth*context->m_iScaledHeight/2);

				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("context->m_pGAScaledYBuffer[%p]   context->m_pGAScaledCbCrBuffer[%p]   context->m_pBackBuffer[%p]",context->m_pGAScaledYBuffer, context->m_pGAScaledCbCrBuffer, context->m_pBackBuffer);
				}
			}

			context->m_IsEnableGAMemoryForTiledRotation = TRUE;

			if (context->m_IsEnableGAMemoryForTiledRotation == TRUE)
			{
				gboolean RetVal = TRUE;
				context->m_iBitDepth = 2;
				//Create GA memory for 180 Degree Rotation
				RetVal = CPlatformInterface::GetInstance()->GetGAVideoRotationMemory(&context->m_pGARotateBuffer, &context->m_pGAMemHandle_RotateBuf, PANEL_WIDTH, PANEL_HEIGHT, context->m_iBitDepth);
				if (RetVal == TRUE)
				{
					int nVideoMemorySize = PANEL_WIDTH*PANEL_HEIGHT*context->m_iBitDepth;

					if (context->m_bIsRotationPrintsEnabled)
					{
						GST_LOG("SdGfx_AllocBuffer:Success : context->m_pGABufferToRotate : %p Size:%d", context->m_pGARotateBuffer, nVideoMemorySize);
					}

					if (context->m_pGARotateBuffer)
					{
						memset(context->m_pGARotateBuffer, 0, nVideoMemorySize);
					}
				}
				else
				{
					GST_LOG(" SdGfx_AllocBuffer : FAILED  context->m_pGARotateBuffer : %p", context->m_pGARotateBuffer);
					context->m_pGARotateBuffer = NULL;
					return FALSE;
				}
			}

		}
		else // For Other Platforms FOXP/FOXB/ECHOP
	 	{
			context->m_pGAScaledYBuffer = (unsigned char*)Status.GfxInfo.sPlane.pBackBuffer[0];
			context->m_pGAScaledCbCrBuffer = context->m_pGAScaledYBuffer + (PANEL_WIDTH*PANEL_HEIGHT);

			if ((!context->m_pGAScaledYBuffer) || (!context->m_pGAScaledCbCrBuffer))
			{
				GST_LOG(" !!!!!! BACK BUFFER FAILED context->m_pGAScaledYBuffer[%p] context->m_pGAScaledCbCrBuffer[%p]",context->m_pGAScaledYBuffer,context->m_pGAScaledCbCrBuffer);
				return FALSE;
			}
			
	 	}

		// These pointers can have either GA memory or Local System Memory based on above judgement
		context->m_pOutputYBufPtr = context->m_pGAScaledYBuffer;
		context->m_pOutputCbCrBufPtr = context->m_pGAScaledCbCrBuffer;

	#endif

	return TRUE;
}

static gboolean t_AssignGAMemoryForScaling(struct VideoFrameRotateContext* context)
{
	gboolean RetVal = TRUE;

	// Choose BackBuffer
	if (context->m_iRotationGAScalledBuffer_GFX1 == 1)
	{

		RetVal = t_CreateBackBufferMemory(context);
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("!!!!!PROCESS VIDEOROTATION WITH BACKBUFFER MEMORY!!!!!");
		}
	}
	else  // Choose GA memory
	{
		RetVal = t_CreateGAMemory(context);
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("!!!!!PROCESS VIDEOROTATION WITH GA MEMORY!!!!!");
		}
	}

	return RetVal;
}

static void t_DisableCache(struct VideoFrameRotateContext* context)
{
#ifdef SUPPORT_SW_VIDEO_DECODER
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_DisableCache +++++++");
	}

	uint32_t SdRet = SD_OK;
	SdGfx_Settings_t Settings;
	memset(&Settings, 0x0, sizeof(SdGfx_Settings_t));

	SdRet = SdGfx_Get(SD_GFX1, &Settings);
	if(SdRet != SD_OK)
	{
		//LOG_INFO("SdGfx_Get Error : %d ",SdRet);
	}

	Settings.bCachedOn = FALSE;
	SdRet = SdGfx_Set(SD_GFX1, &Settings);
	if(SdRet != SD_OK)
	{
		//LOG_INFO("SdGfx_Set Error : %d ",SdRet);
	}

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_DisableCache -------");
	}
#endif
}

static gboolean t_CreateRotateThread(void *(*start_routine)(void*), int iPriority, void* pParam, pthread_t* pThreadId, gboolean detachedMode)
{
	int err, attr_inited;
	pthread_t t_id = 0;
	pthread_attr_t tattr;
	#define PLAYER_THREAD_PRIORITY_OTHER 80

	attr_inited = 0;
	detachedMode = TRUE;
	if (pThreadId != NULL)
	{
		*pThreadId = 0;
	}

	err = pthread_attr_init(&tattr);
	GST_LOG("pthread_attr_init ret = %d ", err);
	if (err == 0)
	{
		attr_inited = 1;
		err = pthread_attr_setinheritsched(&tattr, PTHREAD_EXPLICIT_SCHED);
		GST_LOG("pthread_attr_setinheritsched ret = %d ", err);
	}

	if (err == 0)
	{
		if (iPriority == PLAYER_THREAD_PRIORITY_OTHER)
		{
			err = pthread_attr_setschedpolicy(&tattr, SCHED_OTHER);
			GST_LOG("pthread_attr_setinheritsched[PLAYER_THREAD_PRIORITY_OTHER(%d)] ret = %d ",PLAYER_THREAD_PRIORITY_OTHER, err);
		}
		else
		{
			err = pthread_attr_setschedpolicy(&tattr, SCHED_FIFO);
			GST_LOG("pthread_attr_setinheritsched ret = %d ", err);
			if (err == 0)
			{
				struct sched_param param;
				param.sched_priority = iPriority;
				err = pthread_attr_setschedparam(&tattr, &param);
				GST_LOG("pthread_attr_setschedparam ret = %d ", err);
			}
		}
	}

	if ((err == 0) && detachedMode)
	{
		err = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
		GST_LOG("pthread_attr_setdetachstate ret = %d ", err);
	}

	if (err == 0)
	{
		err = pthread_create(&t_id, &tattr, start_routine, (void *)pParam);
		GST_LOG("pthread_create  ret = %d ", err);
		if (err)
		{
			t_id = 0;
		}
	}

	if (attr_inited)
	{
		err |= pthread_attr_destroy(&tattr);
		GST_LOG("pthread_attr_destroy ret = %d ", err);
	}

	if(pThreadId != 0)
	{
		*pThreadId = t_id;
	}

	if (err != 0)
	{
		return FALSE;
	}
	else{
		return TRUE;
	}
	//return err;
}

static void* t_RotateThreadWorker(void *arg)
{
	prctl(PR_SET_NAME, "UP-VideoRotateThread", 0, 0, 0); 

	struct PerThreadContext* p = (struct PerThreadContext*)arg;
	struct VideoFrameRotateContext* pThis = (struct VideoFrameRotateContext*)p->context;

	if (pThis->m_bIsTheadAffinity == TRUE)
	{
		pThis->m_imask++;
		if(pThis->m_imask == (unsigned int)(pThis->m_iCpuCoreCount+1))
		{
			pThis->m_imask=1;
		}

		if(pthread_setaffinity_np(pthread_self(),sizeof(pThis->m_imask),(const cpu_set_t *)&pThis->m_imask)<0)
		{
			GST_LOG("SET CPU ERROR %d", pThis->m_imask);
			return NULL;
		}
	}

	for(;;)
	{
		if(*(p->stop))
			return NULL;
		pthread_mutex_lock(&p->update_mutex);
		while (!p->update)
		{
			pthread_cond_wait(&p->update_cond, &p->update_mutex);
			if(*(p->stop))
			{
				pthread_mutex_unlock(&p->update_mutex);
				return NULL;
			}
			break;
		}
		pthread_mutex_unlock(&p->update_mutex);
		unsigned long long tt = get_sw_time();
		//For MSTAR-X10P Rotation

		if (pThis->m_bIsRotationEnabledForTiledFormat == TRUE)
		{
			if (pThis->m_bIsApplyRotationForOptimizations == TRUE)
			{
				if(p->degree == 90) 
					t_Rotate_90_Tiled_Thread_Opt(pThis, p, p->TiledRotatedTableY90Deg, p->TiledRotatedTableUV90Deg);
				else if(p->degree == 270)
					t_Rotate_270_Tiled_Thread_Opt(pThis, p, p->TiledRotatedTableY270Deg, p->TiledRotatedTableUV270Deg); 
				else if(p->degree == 180)
				{
					if(pThis->m_bIsInterlacedScanType == TRUE)
					{
						t_Rotate_180_Tiled_Thread_for_interlace(pThis, p, p->TiledRotatedTable180Deg);
					}
					else
					{
						t_Rotate_180_Tiled_Thread(pThis, p, p->TiledRotatedTable180Deg);
					}
				}
			}
			else
			{
				if(p->degree == 90) 
					t_Rotate_90_270_Tiled_Thread(pThis, p, p->TiledRotatedTableY90Deg, p->TiledRotatedTableUV90Deg);
				else if(p->degree == 270)
					t_Rotate_90_270_Tiled_Thread(pThis, p, p->TiledRotatedTableY270Deg, p->TiledRotatedTableUV270Deg); 
				else if(p->degree == 180)
				{
					if(pThis->m_bIsInterlacedScanType == TRUE)
					{
						t_Rotate_180_Tiled_Thread_for_interlace(pThis, p, p->TiledRotatedTable180Deg);
					}
					else
					{
						t_Rotate_180_Tiled_Thread(pThis, p, p->TiledRotatedTable180Deg);
					}
				}
			}
		}
		else // For Other Platforms ECHOP, etc
		{
			if (p->degree == 90)
			{
				pThis->m_iRotationYRaise = 1;
				if(pThis->m_bIsCallInterScanFunc == FALSE)
				{
					t_Rotate_90_270_Thread(pThis, p, p->Ty1, p->Ty2, p->Ty3, p->Tuv1, p->Tuv2, p->Tuv3);
				}
				else
				{
					//interlace scan mode low resolution
					t_Rotate_90_270_Thread_for_interscan(pThis, p, p->Ty1, p->Ty2, p->Ty3, p->Tuv1, p->Tuv2, p->Tuv3);
				}
			}
			else if (p->degree == 270)
			{
				pThis->m_iRotationYRaise = 0;
				if(pThis->m_bIsCallInterScanFunc == FALSE)
				{
					t_Rotate_90_270_Thread(pThis, p, p->Ty4, p->Ty5, p->Ty6, p->Tuv4, p->Tuv5, p->Tuv6); 
				}
				else
				{
					//interlace scan mode low resolution
					t_Rotate_90_270_Thread_for_interscan(pThis, p, p->Ty4, p->Ty5, p->Ty6, p->Tuv4, p->Tuv5, p->Tuv6); 
				}
			}
			else if (p->degree == 180) //180
			{
				if (pThis->m_bEnable180DegOpti == TRUE)
					t_Rotate_180_Thread_Opt(pThis, p);   // FOXP only
				else if(pThis->m_bIsCallInterScanFunc == TRUE)
					t_Rotate_180_Thread_for_interscan(p);
				else
					t_Rotate_180_Thread(pThis, p);
			}
			else // 0
			{
				t_Rotate_0_Thread(pThis, p);
			}
		}

		pthread_mutex_lock(&p->update_mutex);
		p->update = 0;
		pthread_mutex_unlock(&p->update_mutex);

		p->rot_time += get_sw_time() - tt;

		pthread_mutex_lock(&p->finish_mutex);
		p->finish = 1;
		pthread_cond_signal(&p->finish_cond);

		pthread_mutex_unlock(&p->finish_mutex);

		p->rot_time1 += get_sw_time() - tt;
		/*if(g_frames % 20 == 1)
			GST_LOG("Thread[%d] frame[%lld] rot time[%lld] avg time[%lld] time1[%lld] curr rottime[%lld] ", 
					p->id, g_frames, get_sw_time() - tt, p->rot_time/g_frames,p->rot_time1/g_frames,get_sw_time() - tt); */
		//usleep(5000);
	}
	return NULL;
}


static gboolean t_AssignThreadsForRotation(struct VideoFrameRotateContext* context)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_AssignThreadsForRotation +++++++ ");
	}

	#define PLAYER_THREAD_PRIORITY_NORMAL 80
	gboolean RetStatus = TRUE;
	struct PerThreadContext *pPerThreadContext;
	context->m_pAllThreadContextRotate = (struct AllThreadContext*)g_malloc(sizeof(struct AllThreadContext));
	if (context->m_pAllThreadContextRotate == NULL)
	{
		GST_LOG("!!!!!!!!!!!context->m_pAllThreadContextRotate FAILED !!!!!!!!!! ");
		return FALSE;
	}
	else
	{
		 if (context->m_bIsRotationPrintsEnabled)
		 {
			GST_LOG("  Alloc context->m_pAllThreadContextRotate Success : context->m_pAllThreadContextRotate : %p ",context->m_pAllThreadContextRotate);
		 }
	}
	memset(context->m_pAllThreadContextRotate, 0, sizeof(struct AllThreadContext));
	int thread_cnt = context->m_pAllThreadContextRotate->thread_count = context->m_iRotationThreadCount;
	context->m_pAllThreadContextRotate->dstY = context->m_pOutputYBufPtr;//context->m_pGAScaledYBuffer;
	context->m_pAllThreadContextRotate->dstCbCr = context->m_pOutputCbCrBufPtr;//context->m_pGAScaledCbCrBuffer;
	context->m_pAllThreadContextRotate->targetYUYV= context->m_pBackBuffer;
	context->m_pAllThreadContextRotate->targetGAbuffer= context->m_pGARotateBuffer;
	context->m_pAllThreadContextRotate->workers = (pthread_t*)g_malloc(thread_cnt * sizeof(pthread_t));
	if (context->m_pAllThreadContextRotate->workers == NULL)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free context->m_pAllThreadContextRotate Success : %p ",context->m_pAllThreadContextRotate);
		}
		g_free(context->m_pAllThreadContextRotate);
		context->m_pAllThreadContextRotate = NULL;

		GST_LOG("!!!!!!!!!!!context->m_pAllThreadContextRotate->workers FAILED !!!!!!!!!! ");
		return FALSE;
	}
	else
	{
		 if (context->m_bIsRotationPrintsEnabled)
		 {
			GST_LOG("  Alloc context->m_pAllThreadContextRotate->workers Success : context->m_pAllThreadContextRotate->workers : %p ",context->m_pAllThreadContextRotate->workers);
		 }
	}
	context->m_pAllThreadContextRotate->thread = (struct PerThreadContext*)g_malloc(thread_cnt * sizeof(struct PerThreadContext));
	if (context->m_pAllThreadContextRotate->thread == NULL)
	{
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free context->m_pAllThreadContextRotate->workers Success : %p ",context->m_pAllThreadContextRotate->workers);
		}
		g_free(context->m_pAllThreadContextRotate->workers);
		context->m_pAllThreadContextRotate->workers = NULL;

		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Free context->m_pAllThreadContextRotate Success : %p ",context->m_pAllThreadContextRotate);
		}
		g_free(context->m_pAllThreadContextRotate);
		context->m_pAllThreadContextRotate = NULL;

		GST_LOG("!!!!!!!!!!!context->m_pAllThreadContextRotate->thread FAILED !!!!!!!!!! ");
		return FALSE;
	}
	else
	{
		 if (context->m_bIsRotationPrintsEnabled)
		 {
			GST_LOG("  Alloc context->m_pAllThreadContextRotate->thread Success : context->m_pAllThreadContextRotate->thread : %p ",context->m_pAllThreadContextRotate->thread);
		 }
	}

	pthread_mutex_init(&context->m_pAllThreadContextRotate->context_mutex, NULL);

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("Assign threads for rotation ");
	}

	if(context->m_pAllThreadContextRotate->thread_count > 1)
	{
		int i;
		for(i=0; i<context->m_pAllThreadContextRotate->thread_count; i++)
		{
			pPerThreadContext = &context->m_pAllThreadContextRotate->thread[i];
			memset(pPerThreadContext, 0, sizeof(struct PerThreadContext));
			pPerThreadContext->thread_count = thread_cnt;
			pPerThreadContext->context =(void*) context;
			pPerThreadContext->context_mutex = &context->m_pAllThreadContextRotate->context_mutex;
			pPerThreadContext->srcw = context->m_iOriginalWidth;
			pPerThreadContext->srch = context->m_iOriginalHeight;
			pPerThreadContext->srcLine= context->m_iLineSizeConvertedFrame;
			pPerThreadContext->dstw = context->m_iScaledWidth;
			pPerThreadContext->dsth = context->m_iScaledHeight;
			pPerThreadContext->dstY = context->m_pAllThreadContextRotate->dstY;
			pPerThreadContext->dstCbCr = context->m_pAllThreadContextRotate->dstCbCr;
			pPerThreadContext->targetYUYV = context->m_pAllThreadContextRotate->targetYUYV;
			pPerThreadContext->targetGAbuffer = context->m_pAllThreadContextRotate->targetGAbuffer;
			pPerThreadContext->stop = &context->m_pAllThreadContextRotate->stop;
			pPerThreadContext->id = i;
			//FOXP 90/270 Deg skip Optimized
			pPerThreadContext->update = 0;
			pPerThreadContext->finish = 1;

			pthread_mutex_init(&pPerThreadContext->update_mutex,NULL);
			pthread_cond_init(&pPerThreadContext->update_cond, NULL);
			pthread_mutex_init(&pPerThreadContext->finish_mutex,NULL);
			pthread_cond_init(&pPerThreadContext->finish_cond, NULL);

			pPerThreadContext->starty   = context->m_iScaledHeight * i / thread_cnt;
			pPerThreadContext->endy     = MIN((context->m_iScaledHeight * (i+1) / thread_cnt) - 1, context->m_iScaledHeight - 1);
			pPerThreadContext->starty_c = (context->m_iScaledHeight>>1) * i / thread_cnt;
			pPerThreadContext->endy_c   = MIN(((context->m_iScaledHeight>>1) * (i+1) / thread_cnt) - 1, (context->m_iScaledHeight>>1) - 1);

			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("##### Thread[%d] starty[%d] endy[%d] (scaled width[%d] height[%d]) #####",
					i,pPerThreadContext->starty,pPerThreadContext->endy,context->m_iScaledWidth,context->m_iScaledHeight);
			}

			pPerThreadContext->degree = context->m_iRotationDegree;

			if(context->m_bIsRotationTableInit)
			{
				// MSTAR - X10P Platform
				if (context->m_bIsRotationEnabledForTiledFormat == TRUE)
				{
					if (t_InitThreadTiledTables(context, pPerThreadContext) == FALSE)
					{
						GST_LOG("!!!!!!!!!!!t_InitThreadTiledTables[%d] FAILED !!!!!!!!!!i  ",i);
						RetStatus = FALSE;
					}
				}
				else // ECHOP & Other platforms
				{
					if (t_InitThreadTables(context, pPerThreadContext) == FALSE)
					{
						GST_LOG("!!!!!!!!!!!t_InitThreadTables[%d] FAILED !!!!!!!!!!  ",i);
						RetStatus = FALSE;
					}
				}
			}

			//pthread_create(&context->m_pAllThreadContextRotate->workers[i], NULL, t_RotateThreadWorker, (void*)pPerThreadContext);
			if (t_CreateRotateThread(t_RotateThreadWorker, PLAYER_THREAD_PRIORITY_NORMAL, (void*)pPerThreadContext, 
				&context->m_pAllThreadContextRotate->workers[i], TRUE) == FALSE)
			{
				GST_LOG("!!!!!!!!!!!t_CreateRotateThread [%d] FAILED !!!!!!!!!! ",i);
				RetStatus = FALSE;
			}

			//Failue Case: Error handling
			if (RetStatus == FALSE)
			{
				context->m_pAllThreadContextRotate->thread_count = i+1;
				pPerThreadContext->thread_count = context->m_pAllThreadContextRotate->thread_count;
				GST_LOG(" ERROR IN ASSIGN THREADS FOR ROTATION : TERMINATING THREAD_COUNT[%d] ",context->m_pAllThreadContextRotate->thread_count);
				return FALSE;
			}
		}
		context->m_bIsRotationThreadsInit = TRUE;
	}

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_AssignThreadsForRotation -------");
	}
	return TRUE;
}

static gboolean t_InitThreadTiledTables(struct VideoFrameRotateContext* context, struct PerThreadContext* p)
{
	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_InitThreadTiledTables [%d] +++++++",p->id);
	}

	// 90 Degree Thread Tables - Start
	if (context->m_bIsApplyRotationForOptimizations == TRUE)
	{
		int RotateTableReadCountSize = context->m_iRotateTableReadCountSize_90/p->thread_count;  // 240/p->thread_count

		if(!p->TiledRotatedTableY90Deg)
		{
			p->TiledRotatedTableY90Deg = (int*)g_malloc(p->dstw* RotateTableReadCountSize* sizeof(int));
			if (p->TiledRotatedTableY90Deg)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("  Thread TiledRotatedTableY90Deg[%d] : %p",p->id, p->TiledRotatedTableY90Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR Thread TiledRotatedTableY90Deg[%d]  FAILED:  %p",p->id,context->TiledRotatedTableY90Deg);
				return FALSE;
			}
		}
		if(!p->TiledRotatedTableUV90Deg)
		{
			p->TiledRotatedTableUV90Deg = (int*)g_malloc(p->dstw* RotateTableReadCountSize * sizeof(int));
			if (p->TiledRotatedTableUV90Deg)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("  Thread TiledRotatedTableUV90Deg[%d] : %p",p->id, p->TiledRotatedTableUV90Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR Thread TiledRotatedTableUV90Deg[%d]   FAILED:  %p",p->id,context->TiledRotatedTableUV90Deg);
				return FALSE;
			}
		}

		int sy = p->starty;
		int ey = p->endy;
		int syc = p->starty_c;
		int eyc = p->endy_c;
		int dstWc = context->m_iScaledWidth_90_270/2;
		ey++;
		eyc++;
		
		int table_offset = (sy*context->m_iRotateTableReadCountSize_90)/context->m_iScaledHeight_90_270; //(sy*240)/1080; //// Need to generalize for other resolutions
		int offset = ((context->m_iScaledHeight_90_270 - (ey-sy)) *context->m_iRotateTableReadCountSize_90)/context->m_iScaledHeight_90_270; //((dstScaledHeight-(ey-sy)) *240)/1080;

		p->RotatedTableOffset_90_Y = table_offset;
		p->RotatedTableOffsetIncrement_90 = RotateTableReadCountSize;

		int *pTailedSrcYPtr = context->TiledRotatedTableY90Deg + table_offset;
		int *pTailedDstPerThreadYPtr = p->TiledRotatedTableY90Deg;

		//Y
		int x, y;
		for(x=0; x<context->m_iScaledWidth_90_270; x++)
		{
			for(y=sy; y<ey; y+=9)
			{
				*pTailedDstPerThreadYPtr++ = *pTailedSrcYPtr++;
				*pTailedDstPerThreadYPtr++ = *pTailedSrcYPtr++;
			}
			pTailedSrcYPtr += offset;
		}

		//UV
		table_offset = (syc*context->m_iRotateTableReadCountSize_90)/(context->m_iScaledHeight_90_270/2);
		offset = (((context->m_iScaledHeight_90_270/2) - (eyc-syc)) *context->m_iRotateTableReadCountSize_90)/(context->m_iScaledHeight_90_270/2); //(((dstScaledHeight/2)-(eyc-syc)) *240)/540;

		p->RotatedTableOffset_90_UV= table_offset;

		int *pTailedSrcCbCrPtr = context->TiledRotatedTableUV90Deg + table_offset;
		int *pTailedDstPerThreadCbCrPtr = p->TiledRotatedTableUV90Deg;

		for(x=0; x<dstWc; x++)
		{
			for(y=syc; y<eyc; y+=9)
			{
				*pTailedDstPerThreadCbCrPtr++ = *pTailedSrcCbCrPtr++;
				*pTailedDstPerThreadCbCrPtr++ = *pTailedSrcCbCrPtr++;
				*pTailedDstPerThreadCbCrPtr++ = *pTailedSrcCbCrPtr++;
				*pTailedDstPerThreadCbCrPtr++ = *pTailedSrcCbCrPtr++;
			}

			pTailedSrcCbCrPtr += offset;
		}
	}
	else
	{
		if (!p->TiledRotatedTableY90Deg)
		{
			p->TiledRotatedTableY90Deg = (int*)g_malloc(context->m_iScaledWidth_90_270* context->m_iScaledHeight_90_270* sizeof(int));
			if (p->TiledRotatedTableY90Deg)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("   TiledRotatedTableY270Deg :  %p",context->TiledRotatedTableY270Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR TiledRotatedTableY270Deg  FAILED:  %p",context->TiledRotatedTableY270Deg);
				// Do Error Handling
				return FALSE;
			}
		}
		if(!p->TiledRotatedTableUV90Deg)
		{
			p->TiledRotatedTableUV90Deg = (int*)g_malloc(context->m_iScaledWidth_90_270* (context->m_iScaledHeight_90_270/2) * sizeof(int));
			if (p->TiledRotatedTableUV90Deg)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("   TiledRotatedTableUV90Deg :  %p",context->TiledRotatedTableUV90Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR TiledRotatedTableUV90Deg  FAILED:  %p",context->TiledRotatedTableUV90Deg);
				// Do Error Handling
				return FALSE;
			}
		}

		memcpy(p->TiledRotatedTableY90Deg, context->TiledRotatedTableY90Deg, (context->m_iScaledWidth_90_270* context->m_iScaledHeight_90_270* sizeof(int)));
		memcpy(p->TiledRotatedTableUV90Deg, context->TiledRotatedTableUV90Deg, (context->m_iScaledWidth_90_270* (context->m_iScaledHeight_90_270/2) * sizeof(int)));
	}
	// 90 Degree Thread Tables - End

	// 270 Degree Thread Tables - Start
	if (context->m_bIsApplyRotationForOptimizations == TRUE)
	{
		int RotateTableReadCountSize = context->m_iRotateTableReadCountSize_270/p->thread_count;  // 240/p->thread_count
	
		if(!p->TiledRotatedTableY270Deg)
		{
			p->TiledRotatedTableY270Deg = (int*)g_malloc(p->dstw* RotateTableReadCountSize* sizeof(int));
			if (p->TiledRotatedTableY270Deg)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("  Thread TiledRotatedTableY270Deg[%d] : %p",p->id, p->TiledRotatedTableY270Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR Thread TiledRotatedTableY270Deg[%d]  FAILED:  %p",p->id,p->TiledRotatedTableY270Deg);
				return FALSE;
			}
		}
		if(!p->TiledRotatedTableUV270Deg)
		{
			p->TiledRotatedTableUV270Deg = (int*)g_malloc(p->dstw* RotateTableReadCountSize * sizeof(int));
			if (p->TiledRotatedTableUV270Deg)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("  Thread TiledRotatedTableUV270Deg[%d] : %p",p->id, p->TiledRotatedTableUV270Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR Thread TiledRotatedTableUV270Deg[%d]  FAILED:  %p",p->id,p->TiledRotatedTableUV270Deg);
				return FALSE;
			}
		}

		int sy = p->starty;
		int ey = p->endy;
		int syc = p->starty_c;
		int eyc = p->endy_c;
		int dstWc = context->m_iScaledWidth_90_270/2;

		ey++;
		eyc++;
		int table_offset = (sy*context->m_iRotateTableReadCountSize_270)/context->m_iScaledHeight_90_270; //(sy*240)/1080;	 //// Need to generalize for other resolutions
		int offset = ((context->m_iScaledHeight_90_270 - (ey-sy)) *context->m_iRotateTableReadCountSize_270)/context->m_iScaledHeight_90_270; //((dstScaledHeight-(ey-sy)) *240)/1080;

		p->RotatedTableOffset_270_Y = table_offset;
		p->RotatedTableOffsetIncrement_270 = RotateTableReadCountSize;

		int *pTailedSrcYPtr = context->TiledRotatedTableY270Deg + table_offset;
		int *pTailedDstPerThreadYPtr = p->TiledRotatedTableY270Deg;

		//Y
		int x, y;
		for(x=0; x<context->m_iScaledWidth_90_270; x++)
		{
			for(y=sy; y<ey; y+=9)
			{
				*pTailedDstPerThreadYPtr++ = *pTailedSrcYPtr++;
				*pTailedDstPerThreadYPtr++ = *pTailedSrcYPtr++;
			}
			pTailedSrcYPtr += offset;
		}

		//UV
		table_offset = (syc*context->m_iRotateTableReadCountSize_270)/(context->m_iScaledHeight_90_270/2);
		offset = (((context->m_iScaledHeight_90_270/2) - (eyc-syc)) *context->m_iRotateTableReadCountSize_270)/(context->m_iScaledHeight_90_270/2); //(((dstScaledHeight/2)-(eyc-syc)) *240)/1080;

		p->RotatedTableOffset_270_UV = table_offset;
		
		int *pTailedSrcCbCrPtr = context->TiledRotatedTableUV270Deg + table_offset;
		int *pTailedDstPerThreadCbCrPtr = p->TiledRotatedTableUV270Deg;

		for(x=0; x<dstWc; x++)
		{
			for(y=syc; y<eyc; y+=9)
			{
				*pTailedDstPerThreadCbCrPtr++ = *pTailedSrcCbCrPtr++;
				*pTailedDstPerThreadCbCrPtr++ = *pTailedSrcCbCrPtr++;
				*pTailedDstPerThreadCbCrPtr++ = *pTailedSrcCbCrPtr++;
				*pTailedDstPerThreadCbCrPtr++ = *pTailedSrcCbCrPtr++;
			}

			pTailedSrcCbCrPtr += offset;
		}
	}
	else
	{
		if (!p->TiledRotatedTableY270Deg)
		{
			p->TiledRotatedTableY270Deg = (int*)g_malloc(context->m_iScaledWidth_90_270* context->m_iScaledHeight_90_270* sizeof(int));
			if (p->TiledRotatedTableY270Deg)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("  Thread TiledRotatedTableY270Deg[%d] : %p",p->id, p->TiledRotatedTableY270Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR Thread TiledRotatedTableY270Deg[%d]  FAILED:  %p",p->id,p->TiledRotatedTableY270Deg);
				// Do Error Handling
				return FALSE;
			}
		}
		if(!p->TiledRotatedTableUV270Deg)
		{
			p->TiledRotatedTableUV270Deg = (int*)g_malloc(context->m_iScaledWidth_90_270* (context->m_iScaledHeight_90_270/2) * sizeof(int));
			if (p->TiledRotatedTableUV270Deg)
			{
				if (context->m_bIsRotationPrintsEnabled)
				{
					GST_LOG("  Thread TiledRotatedTableUV270Deg[%d] : %p",p->id, p->TiledRotatedTableUV270Deg);
				}
			}
			else
			{
				GST_LOG("   ERROR Thread TiledRotatedTableUV270Deg[%d]  FAILED:  %p",p->id,p->TiledRotatedTableUV270Deg);
				// Do Error Handling
		 		return FALSE;
			}
		}

		memcpy(p->TiledRotatedTableY270Deg, context->TiledRotatedTableY270Deg, (context->m_iScaledWidth_90_270* context->m_iScaledHeight_90_270* sizeof(int)));
		memcpy(p->TiledRotatedTableUV270Deg, context->TiledRotatedTableUV270Deg, (context->m_iScaledWidth_90_270* (context->m_iScaledHeight_90_270/2) * sizeof(int)));
	}
	// 270 Degree Thread Tables - End


	// 180 Degree Thread Tables - Start
	if (!p->TiledRotatedTable180Deg)
	{
		p->TiledRotatedTable180Deg = (int*)g_malloc(context->m_iOriginalHeight * sizeof(int));
		if (p->TiledRotatedTable180Deg)
		{
			if (context->m_bIsRotationPrintsEnabled)
			{
				GST_LOG("  Thread TiledRotatedTable180Deg[%d] : %p",p->id, p->TiledRotatedTable180Deg);
			}
		}
		else
		{
			GST_LOG("   ERROR Thread TiledRotatedTable180Deg[%d]  FAILED:  %p",p->id,p->TiledRotatedTable180Deg);
		 	return FALSE;
		}
	}
	memcpy(p->TiledRotatedTable180Deg, context->TiledRotatedTable180Deg, context->m_iOriginalHeight * sizeof(int));
	// 180 Degree Thread Tables - End

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("t_InitThreadTiledTables [%d] -------",p->id);
	}
	return TRUE;
}

static gboolean t_InitThreadTables(struct VideoFrameRotateContext* context, struct PerThreadContext* p)
{
	if(!p)
	{
		return FALSE;
	}

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("Init Thread tables 90 degrees ");
	}

	int dstw = 0;
	int dsth = 0;

	GST_LOG("t_InitThreadTables: IsVeryLowResolutionOpt %d, Low_W/H  %d/%d   W/H  %d/%d  ", context->m_IsVeryLowResolutionOpt, context->m_iTableWidth_90_270, context->m_iTableHeight_90_270, p->dstw, p->dsth);
	dstw = context->m_iTableWidth_90_270;
	dsth = context->m_iTableHeight_90_270;

	if ((dstw == 0) ||(dsth== 0))
	{
		GST_LOG("!!!!!!!!ERROR : Unable to Proceed to Calculate t_InitThreadTables  !!!!!!!!");
		return FALSE;
	}

	if (!p->Ty1)
	{
		p->Ty1 = (int*)g_malloc(dsth * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Ty1[%d] Success : p->Ty1 : %p",p->id,p->Ty1);
		}
	}
	if (!p->Ty2)
	{
		p->Ty2 = (int*)g_malloc(dstw * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Ty2[%d] Success : p->Ty2 : %p",p->id,p->Ty2);
		}
	}
	if (!p->Ty3)
	{
		p->Ty3 = (int*)g_malloc(dsth * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Ty3[%d] Success : p->Ty3 : %p",p->id,p->Ty3);
		}
	}

	if (!p->Tuv1)
	{
		p->Tuv1 = (int*)g_malloc((dsth/2) * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Tuv1[%d] Success : p->Tuv1 : %p",p->id,p->Tuv1);
		}
	}
	if (!p->Tuv2)
	{
		p->Tuv2 = (int*)g_malloc((dstw/2) * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Tuv2[%d] Success : p->Tuv2 : %p",p->id,p->Tuv2);
		}
	}
	if (!p->Tuv3)
	{
		p->Tuv3 = (int*)g_malloc((dsth/2) * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Tuv3[%d] Success : p->Tuv3 : %p",p->id,p->Tuv3);
		}
	}

	if(!p->Ty1 || !p->Ty2 || !p->Ty3 || !p->Tuv1 || !p->Tuv2 || !p->Tuv3)
	{
		 GST_LOG("!!!!!!!!ERROR : Unable to Proceed to Calculate t_InitThreadTables 90 Degree Tables !!!!!!!!");
		 return FALSE;
	}

	memcpy(p->Ty1, context->Ty1, dsth * sizeof(int));
	memcpy(p->Ty2, context->Ty2, dstw * sizeof(int));
	memcpy(p->Ty3, context->Ty3, dsth * sizeof(int));
	memcpy(p->Tuv1, context->Tuv1, (dsth/2) * sizeof(int));
	memcpy(p->Tuv2, context->Tuv2, (dstw/2) * sizeof(int));
	memcpy(p->Tuv3, context->Tuv3, (dsth/2) * sizeof(int));

	if (context->m_bIsRotationPrintsEnabled)
	{
		GST_LOG("Init Thread tables 270 degrees ");
	}

	if (!p->Ty4)
	{
		p->Ty4 = (int*)g_malloc(dsth * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Ty4[%d] Success : p->Ty4 : %p",p->id,p->Ty4);
		}
	}
	if (!p->Ty5)
	{
		p->Ty5 = (int*)g_malloc(dstw * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Ty5[%d] Success : p->Ty5 : %p",p->id,p->Ty5);
		}
	}
	if (!p->Ty6)
	{
		p->Ty6 = (int*)g_malloc(dsth * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Ty6[%d] Success : p->Ty6 : %p",p->id,p->Ty6);
		}
	}

	if (!p->Tuv4)
	{
		p->Tuv4 = (int*)g_malloc((dsth/2) * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Tuv4[%d] Success : p->Tuv4 : %p",p->id,p->Tuv4);
		}
	}
	if (!p->Tuv5)
	{
		p->Tuv5 = (int*)g_malloc((dstw/2) * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Tuv5[%d] Success : p->Tuv5 : %p",p->id,p->Tuv5);
		}
	}
	if (!p->Tuv6)
	{
		p->Tuv6 = (int*)g_malloc((dsth/2) * sizeof(int));
		if (context->m_bIsRotationPrintsEnabled)
		{
			GST_LOG("  Alloc p->Tuv6[%d] Success : p->Tuv6 : %p",p->id,p->Tuv6);
		}
	}

	if(!p->Ty4 || !p->Ty5 || !p->Ty6 || !p->Tuv4 || !p->Tuv5 || !p->Tuv6)
	{
		 GST_LOG("!!!!!!!!ERROR : Unable to Proceed to Calculate t_InitThreadTables 270 Degree Tables !!!!!!!!");
		 return FALSE;
	}

	memcpy(p->Ty4, context->Ty4, dsth * sizeof(int));
	memcpy(p->Ty5, context->Ty5, dstw * sizeof(int));
	memcpy(p->Ty6, context->Ty6, dsth * sizeof(int));
	memcpy(p->Tuv4, context->Tuv4, (dsth/2) * sizeof(int));
	memcpy(p->Tuv5, context->Tuv5, (dstw/2) * sizeof(int));
	memcpy(p->Tuv6, context->Tuv6, (dsth/2) * sizeof(int));

	// add TileTables for 270 Degree
	return TRUE;
}

static gboolean t_HandleLeftOverTiledData(int nLeftOverData, unsigned long long *srcY64bitPtr0, unsigned long long *srcY64bitPtr1, unsigned long long *srcUV64bitPtr, unsigned char* dstyuyv0, unsigned char* dstyuyv1)
{
	unsigned int lsb32Y0, lsb32Y1, msb32Y0, msb32Y1, lsb32UV, msb32UV;
	unsigned long long tmp64BitReg = 0;
	int nStillRemained = nLeftOverData;

	if ((nLeftOverData<=0) || (srcY64bitPtr0==NULL) || (srcY64bitPtr1==NULL) || (srcUV64bitPtr==NULL) || (dstyuyv0==NULL) ||(dstyuyv1==NULL))
	{
		return FALSE;
	}

	tmp64BitReg = srcY64bitPtr0[0];
	lsb32Y0 = (unsigned int)tmp64BitReg;
	msb32Y0 = (unsigned int)(tmp64BitReg>>32);
	tmp64BitReg = srcY64bitPtr1[0];
	lsb32Y1 = (unsigned int)tmp64BitReg;
	msb32Y1 = (unsigned int)(tmp64BitReg>>32);
	tmp64BitReg = srcUV64bitPtr[0];
	lsb32UV = (unsigned int)tmp64BitReg;
	msb32UV = (unsigned int)(tmp64BitReg>>32);
	///1
	*dstyuyv0-- = (unsigned char)(lsb32UV>>8);
	*dstyuyv1-- = (unsigned char)(lsb32UV>>8);
	*dstyuyv0-- = (unsigned char)(lsb32Y0);
	*dstyuyv1-- = (unsigned char)(lsb32Y1);
	nStillRemained--;
	if (nStillRemained)
	{     ///2
		*dstyuyv0-- = (unsigned char)(lsb32UV);
		*dstyuyv1-- = (unsigned char)(lsb32UV);
		*dstyuyv0-- = (unsigned char)(lsb32Y0>>8);
		*dstyuyv1-- = (unsigned char)(lsb32Y1>>8);
		nStillRemained--;
		if (nStillRemained)
		{	///3
			*dstyuyv0-- = (unsigned char)(lsb32UV>>24);
			*dstyuyv1-- = (unsigned char)(lsb32UV>>24);
			*dstyuyv0-- = (unsigned char)(lsb32Y0>>16);
			*dstyuyv1-- = (unsigned char)(lsb32Y1>>16);
			nStillRemained--;
			if (nStillRemained)
			{	///4
				*dstyuyv0-- = (unsigned char)(lsb32UV>>16);
				*dstyuyv1-- = (unsigned char)(lsb32UV>>16);
				*dstyuyv0-- = (unsigned char)(lsb32Y0>>24);
				*dstyuyv1-- = (unsigned char)(lsb32Y1>>24);
				nStillRemained--;
				if (nStillRemained)
				{	///5
					*dstyuyv0-- = (unsigned char)(msb32UV>>8);
					*dstyuyv1-- = (unsigned char)(msb32UV>>8);
					*dstyuyv0-- = (unsigned char)(msb32Y0);
					*dstyuyv1-- = (unsigned char)(msb32Y1);
					nStillRemained--;
					if (nStillRemained)
					{     ///6
						*dstyuyv0-- = (unsigned char)(msb32UV);
						*dstyuyv1-- = (unsigned char)(msb32UV);
						*dstyuyv0-- = (unsigned char)(msb32Y0>>8);
						*dstyuyv1-- = (unsigned char)(msb32Y1>>8);
						nStillRemained--;
						if (nStillRemained)
						{	///7
							*dstyuyv0-- = (unsigned char)(msb32UV>>24);
							*dstyuyv1-- = (unsigned char)(msb32UV>>24);
							*dstyuyv0-- = (unsigned char)(msb32Y0>>16);
							*dstyuyv1-- = (unsigned char)(msb32Y1>>16);
							nStillRemained--;
							if (nStillRemained)
							{	///8
								*dstyuyv0-- = (unsigned char)(msb32UV>>16);
								*dstyuyv1-- = (unsigned char)(msb32UV>>16);
								*dstyuyv0-- = (unsigned char)(msb32Y0>>24);
								*dstyuyv1-- = (unsigned char)(msb32Y1>>24);
								nStillRemained--;
								if (nStillRemained)
								{
									tmp64BitReg = srcY64bitPtr0[1];
									lsb32Y0 = (unsigned int)tmp64BitReg;
									msb32Y0 = (unsigned int)(tmp64BitReg>>32);
									tmp64BitReg = srcY64bitPtr1[1];
									lsb32Y1 = (unsigned int)tmp64BitReg;
									msb32Y1 = (unsigned int)(tmp64BitReg>>32);
									tmp64BitReg = srcUV64bitPtr[1];
									lsb32UV = (unsigned int)tmp64BitReg;
									msb32UV = (unsigned int)(tmp64BitReg>>32);
									///9 
									*dstyuyv0-- = (unsigned char)(lsb32UV>>8);
									*dstyuyv1-- = (unsigned char)(lsb32UV>>8);
									*dstyuyv0-- = (unsigned char)(lsb32Y0);
									*dstyuyv1-- = (unsigned char)(lsb32Y1);
									nStillRemained--;
									if (nStillRemained)
									{	///10
										*dstyuyv0-- = (unsigned char)(lsb32UV);
										*dstyuyv1-- = (unsigned char)(lsb32UV);
										*dstyuyv0-- = (unsigned char)(lsb32Y0>>8);
										*dstyuyv1-- = (unsigned char)(lsb32Y1>>8);
										nStillRemained--;
										if (nStillRemained)
										{	///11
											*dstyuyv0-- = (unsigned char)(lsb32UV>>24);
											*dstyuyv1-- = (unsigned char)(lsb32UV>>24);
											*dstyuyv0-- = (unsigned char)(lsb32Y0>>16);
											*dstyuyv1-- = (unsigned char)(lsb32Y1>>16);
											nStillRemained--;
											if (nStillRemained)
											{	///12
												*dstyuyv0-- = (unsigned char)(lsb32UV>>16);
												*dstyuyv1-- = (unsigned char)(lsb32UV>>16);
												*dstyuyv0-- = (unsigned char)(lsb32Y0>>24);
												*dstyuyv1-- = (unsigned char)(lsb32Y1>>24);
												nStillRemained--;
												if (nStillRemained)
												{	///13
													*dstyuyv0-- = (unsigned char)(msb32UV>>8);
													*dstyuyv1-- = (unsigned char)(msb32UV>>8);
													*dstyuyv0-- = (unsigned char)(msb32Y0);
													*dstyuyv1-- = (unsigned char)(msb32Y1);
													nStillRemained--;
													if (nStillRemained)
													{	///14
														*dstyuyv0-- = (unsigned char)(msb32UV);
														*dstyuyv1-- = (unsigned char)(msb32UV);
														*dstyuyv0-- = (unsigned char)(msb32Y0>>8);
														*dstyuyv1-- = (unsigned char)(msb32Y1>>8);
														nStillRemained--;
														if (nStillRemained)
														{	///15
															*dstyuyv0-- = (unsigned char)(msb32UV>>24);
															*dstyuyv1-- = (unsigned char)(msb32UV>>24);
															*dstyuyv0-- = (unsigned char)(msb32Y0>>16);
															*dstyuyv1-- = (unsigned char)(msb32Y1>>16);
															nStillRemained--;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return TRUE;
}

#ifdef  ENABLE_TBM
gboolean init_rotate_tbm_bufmgr(gint *pFD, tbm_bufmgr *pMgr, Display* pDisp)
{
	if (pFD && pMgr && pDisp)
	{
		char *driverName = NULL;
		char *deviceName = NULL;
		if (*pFD != -1 || *pMgr)
		{
			GST_ERROR("Alread opened");
			return FALSE;
		}

		if (!DRI2Connect(pDisp, DefaultRootWindow(pDisp), &driverName, &deviceName))
		{
			GST_ERROR("DRI2Connect !!");
			return FALSE;
		}

		if (!deviceName)
		{
			GST_ERROR("deviceName is NULL");
			return FALSE;
		}

		GST_LOG("driverName[ %s ], deviceName[ %s ]", driverName, deviceName);
		*pFD = open(deviceName, O_RDWR | O_CLOEXEC);
		if (*pFD)
		{
			/* authentication */                                                         
			unsigned int magic = 0;
			if(drmGetMagic(*pFD, &magic))
			{
				GST_ERROR("Can't get magic key from drm");
				goto FAIL_TO_INIT;
			}

			if(False == DRI2Authenticate(pDisp, DefaultRootWindow(pDisp), magic))
			{                                                                            
				GST_ERROR("Can't get the permission");
				goto FAIL_TO_INIT;
			}

			*pMgr = tbm_bufmgr_init(*pFD);
			if (*pMgr == NULL)
			{
				GST_ERROR("tbm_bufmgr_init failed");
				goto FAIL_TO_INIT;
			}

			return TRUE;
		}
	}

FAIL_TO_INIT:
	if (pFD && *pFD != -1)
	{
		close(*pFD);
		*pFD = -1;
	}
	return FALSE;
}

void deinit_rotate_tbm_bufmgr(gint *pFD, tbm_bufmgr *pMgr)
{
	if (pFD && pMgr)
	{
		if (*pMgr)
		{
			tbm_bufmgr_deinit(*pMgr);
			*pMgr = NULL;
		}
		
		if (*pFD != -1)
		{
			close(*pFD);
			*pFD = -1;
		}
	}
}

static gboolean allocate_rotate_tbm_buffer(struct VideoFrameRotateContext* context)
{
  gboolean ret = FALSE;
  memset (&context->bo_AP, 0x0, sizeof(tbm_bo));
  memset (&context->bo_AP_CbCr, 0x0, sizeof(tbm_bo));
  memset (&context->bo_handle_AP, 0x0, sizeof(tbm_bo_handle));
  memset (&context->bo_handle_AP_CbCr, 0x0, sizeof(tbm_bo_handle));
  if(context){
	if (init_rotate_tbm_bufmgr(&context->drm_fd, &context->bufmgr_AP, context->disp) == FALSE) {
		GST_ERROR("VF: failed to init tbm bufmgr -> drm_fd[%d], disp[0x%x], err[%d] ", context->drm_fd, context->disp, errno);
		ret = FALSE;
		return ret;
	}else {
		ret = TRUE;
	}
	if (ret) {
		guint w = PANEL_WIDTH;  //default alloc one AP&MP Buffer
		guint h = PANEL_HEIGHT;  //default alloc one AP&MP Buffer
		/*alloc the roate tbm AP buffer*/
		context->bo_AP = tbm_bo_alloc(context->bufmgr_AP, w*h, TBM_BO_SCANOUT);
		if(!context->bo_AP){
			GST_ERROR("VF: failed to allocate tbm bo_AP !");
			goto FAIL_TO_ALLOC_TBM;
		}
		context->bo_handle_AP = tbm_bo_map(context->bo_AP, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
		if(!context->bo_handle_AP.ptr){
			GST_ERROR("VF: failed to map tbm bo_AP !");
			goto FAIL_TO_ALLOC_TBM;
		}
		context->bo_AP_CbCr = tbm_bo_alloc(context->bufmgr_AP, w*h, TBM_BO_SCANOUT);
		if(!context->bo_AP_CbCr){
			GST_ERROR("VF: failed to allocate tbm  bo_AP_CbCr ! ");
			goto FAIL_TO_ALLOC_TBM;
		}
		context->bo_handle_AP_CbCr = tbm_bo_map(context->bo_AP_CbCr, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
		if(!context->bo_handle_AP_CbCr.ptr){
			GST_ERROR("VF: failed to map tbm bo_AP_CbCr ! ");
			goto FAIL_TO_ALLOC_TBM;
		}
		context->boAP_key = tbm_bo_export(context->bo_AP);
		context->boAP_CbCr_key = tbm_bo_export(context->bo_AP_CbCr);
		GST_ERROR("VF: Success to allocate(allocate_rotate_tbm_buffer) -> Y[0x%x] boAP_key [%d] , CbCr[0x%x] boAP_CbCr_key[%d] !", 
			context->bo_handle_AP.ptr, context->boAP_key, context->bo_handle_AP_CbCr.ptr, context->boAP_CbCr_key);
    		return TRUE;
FAIL_TO_ALLOC_TBM:
	   	if (context->bo_handle_AP.ptr){
			GST_ERROR("VF: unmap bo_AP ptr[0x%x] ", context->bo_handle_AP.ptr);
			tbm_bo_unmap(context->bo_AP);
			GST_ERROR("VF: unmap bo_AP(end) ptr[0x%x] ", context->bo_handle_AP.ptr);
	   	}
		if (context->bo_AP) {
			GST_ERROR("VF: unref bo_AP ptr[0x%x] ", context->bo_AP);
			tbm_bo_unref(context->bo_AP);
			GST_ERROR("VF: unref bo_AP(end) ptr[0x%x] ", context->bo_AP);
		}
		if (context->bo_handle_AP_CbCr.ptr){
			GST_ERROR("VF: unmap bo_AP_CbCr ptr[0x%x] ", context->bo_handle_AP_CbCr.ptr);
			tbm_bo_unmap(context->bo_AP_CbCr);
			GST_ERROR("VF: unmap bo_AP_CbCr(end) ptr[0x%x] ", context->bo_handle_AP_CbCr.ptr);
		}
		if (context->bo_AP_CbCr)  {
			GST_ERROR("VF: unref bo_AP_CbCr ptr[0x%x] ", context->bo_AP_CbCr);
			tbm_bo_unref(context->bo_AP_CbCr);
			GST_ERROR("VF: unref bo_AP_CbCr(end) ptr[0x%x] ", context->bo_AP_CbCr);
		}
	   	GST_ERROR("VF: Failed to allocate video rotation AP -> ptr[0x%x]/[0x%x] ", context->bo_handle_AP.ptr, context->bo_handle_AP_CbCr.ptr);
		return FALSE;
	}else {
	        GST_ERROR("VF: failed to open the drm_fd[ %d ], err[ %d ] ", context->drm_fd, errno);
	}
  }else {
	GST_ERROR("VF: VideoFrameRotateContext[0x%x] is NULL ", context);
  }
  return FALSE;
}
static gboolean free_rotate_tbm_buffer(struct VideoFrameRotateContext* context)
{
	if(context){
		if(context->bo_handle_AP.ptr){
			tbm_bo_unmap(context->bo_AP);
			GST_LOG("VF: unmap tbm Y buffer success!");
		}
		if(context->bo_AP){
			tbm_bo_unref(context->bo_AP);
			context->bo_AP = NULL;
			GST_LOG("VF: unref tbm Y buffer success!");
		}
		if(context->bo_handle_AP_CbCr.ptr){
			tbm_bo_unmap(context->bo_AP_CbCr);
			GST_LOG("VF: unmap tbm CbCr buffer success!");
		}
		if(context->bo_AP_CbCr){
			tbm_bo_unref(context->bo_AP_CbCr);
			context->bo_AP_CbCr = NULL;
			GST_LOG("VF: unref tbm CbCr buffer success!");
		}
		if (context->bufmgr_AP){
			deinit_rotate_tbm_bufmgr(&context->drm_fd, &context->bufmgr_AP);
			GST_LOG("VF: deinit rotate tbm_bufmgr success!");
		}
		memset (&context->bo_handle_AP, 0x0, sizeof(tbm_bo_handle));
		memset (&context->bo_handle_AP_CbCr, 0x0, sizeof(tbm_bo_handle));
		memset (&context->bo_AP, 0x0, sizeof(tbm_bo));
		memset (&context->bo_AP_CbCr, 0x0, sizeof(tbm_bo));
		GST_ERROR("VF: Success to free (free_rotate_tbm_buffer) -> Y[0x%x], CbCr[0x%x]  !", 
			context->bo_handle_AP.ptr, context->bo_handle_AP_CbCr.ptr);
	}
  return TRUE;
}

	#ifdef ENABLE_LOCAL_ROTATE_BUFFER
	static gboolean allocate_rotate_local_buffer(struct VideoFrameRotateContext* context){
		if(context){
			context->pLocalRotateBuffer_Y =  (unsigned char*)g_malloc(PANEL_WIDTH*PANEL_HEIGHT*sizeof(unsigned char));
			context->pLocalRotateBuffer_CbCr =  (unsigned char*)g_malloc(PANEL_WIDTH*PANEL_HEIGHT*sizeof(unsigned char));
			if(!context->pLocalRotateBuffer_Y || !context->pLocalRotateBuffer_CbCr){
				GST_ERROR("VF: allocate rotate local buffer failed !!!");
				if(context->pLocalRotateBuffer_Y)
					g_free(context->pLocalRotateBuffer_Y);
				if(context->pLocalRotateBuffer_CbCr)
					g_free(context->pLocalRotateBuffer_CbCr);
				return FALSE;
			}
		}else {
			GST_ERROR("VF: allocate rotate local buffer failed, context is NULL !!!");
			return FALSE;
		}
		GST_ERROR("VF: allocate rotate local buffer success !!!");
		return TRUE;
	}

	static gboolean free_rotate_local_buffer(struct VideoFrameRotateContext* context){
		if(context){
			if(context->pLocalRotateBuffer_Y)
				g_free(context->pLocalRotateBuffer_Y);
			if(context->pLocalRotateBuffer_CbCr)
				g_free(context->pLocalRotateBuffer_CbCr);
		}else{
			GST_ERROR("VF: free rotate local buffer, context is NULL !!!");
		}
		GST_ERROR("VF: free rotate local buffer success !!!");
		return TRUE;
	}
	#endif	//ENABLE_LOCAL_ROTATE_BUFFER
#endif	//ENABLE_TBM

#ifdef ENABLE_ROTATE_TEST
int main(int argc, char ** argv){
	int i,j;
	int ret = 0;
	int bIsVideoRotationEnabled = 0;
	int iRotationDegree = 0;
	int iOriginalWidth = 0;
	int iOriginalHeight = 0;
	int iCodecId = 0;
	int bIsInterlacedScanType = 0;
	int iFramesPerSec = 0;
	int iRotationDegree_s = 0;
	int iScaledWidth = 0;
	int iScaledHeight = 0;
	int iFrameDone = 0;
	struct VideoFrameRotateContext *frctx = NULL;
	struct VIDEO_FRAME outputFrame;

	AVFormatContext* pCtx = NULL;
	AVCodecContext *pCodecCtx = NULL;
	AVCodec *pCodec = 0;
	AVPacket packet;
	AVFrame *pFrame = 0;
	FILE *fpo1 = NULL;
	FILE *fpo2 = NULL;
	FILE *fpo3 = NULL;
	struct options opt;
       int nframe;
	int got_picture;
	int picwidth, picheight, linesize;
	unsigned char *pBuf;
	int decoded = 0;
	int can_seek = 0;
	int usefo = 0;
	int len = 0;
#ifdef YUV420_YCbCrInterleaved	
	FILE *fpo4 = NULL;
	unsigned char *pGAmem = NULL;
#endif

	GST_LOG("*****ENABLE VIDEO ROTATE TESTING***** \n");

	av_register_all();

	av_log_set_callback(log_callback);
	av_log_set_level(30);

	if (parse_options(&opt, argc, argv) < 0 || (strlen(opt.finput) == 0)){
			show_help(argv[0]);
			return 0;
			}

	ret = av_open_input_file(&pCtx, opt.finput, 0, 0, 0);
		if (ret < 0){
		GST_LOG("\n->(av_open_input_file)\tERROR:\t%d\n", ret);
			goto fail;
		}

	ret = av_find_stream_info(pCtx);
	if (ret < 0){
			GST_LOG("\n->(av_find_stream_info)\tERROR:\t%d\n", ret);
			goto fail;
		}

	if (opt.streamId < 0){
		dump_format(pCtx, 0, pCtx->filename, 0);
		goto fail;
	}else{
		GST_LOG("\n extra data in Stream %d  codec_id[%d]  (%dB):", opt.streamId, pCtx->streams[opt.streamId]->codec->codec_id, pCtx->streams[opt.streamId]->codec->extradata_size);
		for (i = 0; i < pCtx->streams[opt.streamId]->codec->extradata_size; i++){
			if (i%16 == 0) GST_LOG("\n");
				GST_LOG("%2x  ", pCtx->streams[opt.streamId]->codec->extradata[i]);
			}
		}
		
	can_seek = av_seek_available(pCtx);
	GST_LOG("CAN SEEK: %s\n", can_seek == 0?"Yes" : "No");

	/* try opening output files */
	if (strlen(opt.foutput1) && strlen(opt.foutput2)&&strlen(opt.foutput3) ){
        fpo1 = fopen(opt.foutput1, "wb");
        fpo2 = fopen(opt.foutput2, "wb");
	 fpo3 = fopen(opt.foutput3, "wb");
	if (!fpo1 || !fpo2 || !fpo3){
		GST_LOG("\n->error opening output files\n");
		goto fail;
	}
	usefo = 1;
#ifdef YUV420_YCbCrInterleaved
	if(strlen(opt.foutput4)){
		fpo4 = fopen(opt.foutput4, "wb");
		if (!fpo4){
			GST_LOG("\n->error opening output files\n");
			goto fail;
		}
	}
#endif
      }else{
        usefo = 0;
      }

	if (opt.streamId >= pCtx->nb_streams){
		GST_LOG("\n->StreamId\tERROR\n");
		goto fail;
	}

	if (opt.lstart > 0){
		ret = av_seek_frame(pCtx, opt.streamId, opt.lstart, AVSEEK_FLAG_ANY);
		if (ret < 0){
			GST_LOG("\n->(av_seek_frame)\tERROR:\t%d\n", ret);
			goto fail;
		}
	}

	/* for decoder configuration */
	if (!opt.nodec){
		/* prepare codec */
		pCodecCtx = pCtx->streams[opt.streamId]->codec;
		if (opt.thread_count <= 16 && opt.thread_count > 0 ){
			pCodecCtx->thread_count = opt.thread_count;
			pCodecCtx->thread_type = FF_THREAD_FRAME;
		}
		pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
		if (!pCodec){
			GST_LOG("\n->can not find codec!\n");
			goto fail;
		}
		GST_LOG("avcodec_open   pix_fmt [%d]  PIX_FMT_YUV420P16LE[%d] PIX_FMT_YUV420P10LE[%d] \n", pCodecCtx->pix_fmt, PIX_FMT_YUV420P16LE, PIX_FMT_YUV420P10LE);
		ret = avcodec_open(pCodecCtx, pCodec);
		if (ret < 0){
			GST_LOG("\n->(avcodec_open)\tERROR:\t%d\n", ret);
			goto fail;
		}
		pFrame = avcodec_alloc_frame(); 
#ifdef YUV420_YCbCrInterleaved
		pGAmem = av_malloc(1936*1096*4);
		if(!pGAmem){
			GST_LOG("pGAmem malloc failed!\n");
		}
#endif
	}

	iRotationDegree_s = opt.degree;
	bIsVideoRotationEnabled = 1;
	iRotationDegree = 0; //Original degree
	iOriginalWidth = pCodecCtx->width;
	iOriginalHeight = pCodecCtx->height;
	iCodecId = pCodecCtx->codec_id;
	bIsInterlacedScanType = 0;
	iFramesPerSec = pCtx->streams[opt.streamId]->avg_frame_rate.num/pCtx->streams[opt.streamId]->avg_frame_rate.den;

	frctx = videoframe_rotate_create();
	if(!frctx){
		GST_LOG("ERROR: CREATE  VIDEO FRAME ROTATE CONTEXT FAILED! ");
		goto fail;
	}

	GST_LOG("DEBUG: ROTATE OPEN  vr_enable[%d] degree[%d] org_w[%d] org_h[%d] CodecID[%d] ScanType[%d] fps[%d] ",
			bIsVideoRotationEnabled,iRotationDegree, iOriginalWidth, iOriginalHeight, iCodecId, bIsInterlacedScanType, iFramesPerSec);
	ret = videoframe_rotate_open(frctx, bIsVideoRotationEnabled, iRotationDegree, iOriginalWidth, iOriginalHeight, iCodecId, bIsInterlacedScanType, iFramesPerSec);
	if(!ret){
		GST_LOG("ERROR: VIDEO FRAME OPEN FAILED!");
		goto fail;
	}

	videoframe_rotate_set_degree(frctx, iRotationDegree_s);
	if(iRotationDegree_s == iRotationDegree){
		GST_LOG("Rotation Degree Not Changed! ");
		goto fail;
	}

	iScaledWidth = videoframe_rotate_get_scaled_width(frctx);
	iScaledHeight = videoframe_rotate_get_scaled_height(frctx);

	GST_LOG("Start Decode");
	nframe = 0;
	while(nframe < opt.frames || opt.frames == -1){
		ret = av_read_frame(pCtx, &packet);
		if (ret < 0){
			GST_LOG("\n->(av_read_frame)\tERROR:\t%d ", ret);
			break;
		}
		nframe++;
		if (packet.stream_index == opt.streamId){
			if (usefo){
				fwrite(packet.data, packet.size, 1, fpo1);
				fflush(fpo1);
			}

			if (pCtx->streams[opt.streamId]->codec->codec_type == CODEC_TYPE_VIDEO && !opt.nodec){
				 picheight = pCtx->streams[opt.streamId]->codec->height;
				 picwidth = pCtx->streams[opt.streamId]->codec->width;
#ifdef YUV420_YCbCrInterleaved
	#ifdef YUV420_YUV420
				pFrame->data[3] = pGAmem;
				if(pCtx->streams[opt.streamId]->r_frame_rate.den)
					pFrame->frames_per_sec = pCtx->streams[opt.streamId]->r_frame_rate.num / pCtx->streams[opt.streamId]->r_frame_rate.den;				
				pFrame->format_conv_type = 1;
				pCodecCtx->enable_frame_drops = 0;
				pCodecCtx->disable_color_conversion = 0;
				pCodecCtx->enable_color_conv_logs = 0;
				pCodecCtx->Thread_count_ColorConv = 0;
	#endif
#else
				 pFrame->data[3] = NULL;
#endif
				 av_dup_packet(&packet);
				 len = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, &packet);
				  if (got_picture){
					decoded++;
					GST_LOG("\nFrame No %5d stream#%d\tsize %6dB, dts:%6lld, pts:%6lld, pos:%6lld, duration:%6lld   W/H %d/%d ", decoded, packet.stream_index, packet.size, packet.dts, packet.pts, packet.pos, packet.duration, picwidth, picheight);
					/*simulate after decoder, and already get the YUV420 buffer*/
					memset(&outputFrame, 0, sizeof(struct VIDEO_FRAME));
					outputFrame.pData0 = pFrame->data[3];
					outputFrame.pData1 = pFrame->data[3] + picwidth*picheight;
					outputFrame.pData2 = NULL;
					outputFrame.pData3 = NULL;
					outputFrame.lineSize0 = pFrame->linesize[0];
					outputFrame.lineSize1 = pFrame->linesize[1];
					outputFrame.lineSize2 = 0;
					outputFrame.lineSize3 = 0;
					outputFrame.width = picwidth;
					outputFrame.height= picheight;
					outputFrame.eColorFormat = -1;
					outputFrame.eVideoDataFormat = -1;
					outputFrame.iFrameDimension = -1;
					outputFrame.bIsRotationChanged = -1;
					outputFrame.bResolutionChanged = -1;
					outputFrame.iKeyFrame = -1;
					outputFrame.iVideoDecodingMode = -1;
					outputFrame.iVideoFormat = -1;

					if(videoframe_rotate_can_support(frctx,iFramesPerSec,picheight,picwidth)){
						GST_LOG("CAN SUPPORT VIDEO ROTATE: W/H[%d][%d]->Scale_W/Scale_H[%d][%d] Degree[%d]->[%d] ", iOriginalWidth, iOriginalHeight, iScaledWidth, iScaledHeight, iRotationDegree, iRotationDegree_s);
						ret = videoframe_rotate_apply(frctx, 0, &outputFrame, iRotationDegree_s, &iFrameDone);
						if(iFrameDone && ret){
							GST_LOG("VIDEO ROTATE SUCCEED FrameDone[%d] ret[%d] ", iFrameDone, ret);
							videoframe_rotate_update_rotate_angle_change_state(frctx,1);
						}else{
							GST_LOG("VIDEO ROTATE FAILED FrameDone[%d] ret[%d] ", iFrameDone, ret);
							videoframe_rotate_update_rotate_angle_change_state(frctx,0);
						}
					}else {
						GST_LOG("VIDEO FRAME CAN NOT SUPPORT ROTATE! ");
						goto fail;
					}

					if (pCtx->streams[opt.streamId]->codec->pix_fmt == PIX_FMT_YUV420P 
						|| pCtx->streams[opt.streamId]->codec->pix_fmt == PIX_FMT_YUVJ420P)
					{
						if(usefo){
							 GST_LOG("Write Orginal Data:  W/H %d/%d Linesize %d %d %d ", picwidth, picheight, pFrame->linesize[0], pFrame->linesize[1], pFrame->linesize[2]);
							 linesize = pFrame->linesize[0];
							 pBuf = pFrame->data[0];
						   	 for (i = 0; i < picheight; i++){
								fwrite(pBuf, picwidth, 1, fpo2);
								pBuf += linesize;
							 }
							 linesize = pFrame->linesize[1];
							 pBuf = pFrame->data[1];
							  for (i = 0; i < picheight/2; i++){
								fwrite(pBuf, picwidth/2, 1, fpo2);
								pBuf += linesize;
							  }
							  linesize = pFrame->linesize[2];
							  pBuf = pFrame->data[2];
							  for (i = 0; i < picheight/2; i++){
								fwrite(pBuf, picwidth/2, 1, fpo2);
								pBuf += linesize;
							  }
							  fflush(fpo2);

							  GST_LOG("Write after rotate frame  Data:  W/H %d/%d Linesize %d %d %d ", outputFrame.width, outputFrame.height, outputFrame.lineSize0, outputFrame.lineSize1, outputFrame.lineSize2);
							  picwidth = outputFrame.width;
							  picheight = outputFrame.height;
							  linesize = outputFrame.lineSize0;
							  pBuf = outputFrame.pData0;
							  for (i = 0; i < picheight; i++){
								fwrite(pBuf, picwidth, 1, fpo3);
								pBuf += linesize;
							  }
							  linesize = outputFrame.lineSize1;
							  pBuf = outputFrame.pData1;
							  for (i = 0; i < picheight/2; i++){
								fwrite(pBuf, picwidth, 1, fpo3);
								pBuf += linesize;
							  }
							  fflush(fpo3);
#ifdef YUV420_YCbCrInterleaved
	#ifdef YUV420_YUV420
						  int len = pCodecCtx->width*pCodecCtx->height*3/2;
						  pBuf= pFrame->data[3];
						  fwrite(pBuf, len, 1, fpo4);
						  fflush(fpo4);
	#endif
#endif
						}
					}
				}
				av_free_packet(&packet);
			}else {
				av_free_packet(&packet);
				GST_LOG("Please choose the video stream num !");
				goto fail;
			}
			 av_free_packet(&packet);
		 }
		 av_free_packet(&packet);
	  }

	GST_LOG("\n%d frames parsed ", nframe);
       GST_LOG("%d frames decoded ", decoded);

fail:
	if(frctx){
		videoframe_rotate_close(frctx);
		videoframe_rotate_destroy(frctx);
	}
	if(pCodecCtx){
		avcodec_close(pCodecCtx);
		pCodecCtx = NULL;
	}
	if (pCtx){
		av_close_input_file(pCtx);
	}
	if (fpo1){
		fclose(fpo1);
	}
	if (fpo2){
		fclose(fpo2);
	}
	if (fpo3){
		fclose(fpo3);
	}
#ifdef YUV420_YCbCrInterleaved
	if (fpo4)
	{
		fclose(fpo4);
	}
	if(pGAmem)
	{
		av_free(pGAmem);
		pGAmem = NULL;
	}
#endif
	GST_LOG("*****VIDEO ROTATE COMPLETED*****");
	return ret;
}
#endif

