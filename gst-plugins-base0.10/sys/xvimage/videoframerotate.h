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

#ifndef __VIDEOFRAMEROTATE_H__
#define __VIDEOFRAMEROTATE_H__

#include <glib.h>
#include <gst/gst.h>

//#define ENABLE_ROTATE_TEST
#define ENABLE_TBM

//#define ENABLE_LOCAL_ROTATE_BUFFER	//If enable this macro, the data flow will be,  HW (memory heap) -> mem heap(Rotate) -> AP -> MP

#ifdef ENABLE_TBM
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <tbm_bufmgr.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <libdrm/drm.h>
extern Bool DRI2Connect(Display * display, XID window, char **driverName, char **deviceName);
extern Bool DRI2Authenticate(Display * display, XID window, unsigned int magic);

enum VIDEO_DATA_FORMAT
{
	VIDEO_DATA_FORMAT_YCBCR,
};

enum MD_PIXEL_FORMAT
{
	MD_COLOR_FORMAT_YCbCr420,
	MD_COLOR_FORMAT_YCbCr422,
};

struct VIDEO_FRAME
{
	int iVideoFormat;
	unsigned char* pData0;         ///< Y or YCbCr or Ytop or R or RGB
	unsigned char* pData1;        ///< Cb or CbCr or Ybottom or G
	unsigned char* pData2;        ///< Cr or Ctop or B
	unsigned char* pData3;        ///< Cbottom
	unsigned int lineSize0;     ///< width + padded bytes size for pData0
	unsigned int lineSize1;     ///< width + padded bytes size for pData1
	unsigned int lineSize2;     ///< width + padded bytes size for pData2
	unsigned int lineSize3;     ///< width + padded bytes size for pData3
	unsigned int width;           ///< Frame Width
	unsigned int height;         ///< Frame Height
	enum MD_PIXEL_FORMAT eColorFormat; ///< Frame Color Format

	int iKeyFrame; ///< Key Frame Flag
	int iFrameDimension;
    unsigned int decodingStartLine; ///< JPEG only. when partial decoding, decoded start line.
	unsigned int decodingEndLine; ///< JPEG only. when partial decoding, decoded end line
	enum VIDEO_DATA_FORMAT eVideoDataFormat;
	gboolean bResolutionChanged;
	gboolean bIsRotationChanged;
	int iVideoDecodingMode;	// MediaCommon::VIDEO_DECODING_MODE
};

struct VideoFrameRotateContext
{
	gboolean bIsFirstFrame;
	gboolean m_bIsVideoRotationEnabled;
	gboolean m_bIsScaleBufferParmsSet;
	int  m_iLineSizeConvertedFrame;
	int m_iRotationDegree;
	int m_iOriginalWidth;
	int m_iOriginalHeight;
	int m_iScaledWidth;
	int m_iScaledHeight;
	gboolean m_bIsRotationTableInit;
	gboolean m_bIsRotationThreadsInit;
	gboolean m_bIsRotateBufferAllocated;
	gboolean m_bIsRotateAngleChanged;
	int* Ty1;
	int* Ty2;
	int* Ty3;
	int* Tuv1;
	int* Tuv2;
	int* Tuv3;
	int* Ty4;
	int* Ty5;
	int* Ty6;
	int* Tuv4;
	int* Tuv5;
	int* Tuv6;
	unsigned char *m_pGAScaledYBuffer;
	unsigned char *m_pGAScaledCbCrBuffer;
	unsigned char *m_pBackBuffer;
	unsigned char *m_pGARotateBuffer;
	unsigned char *m_pGAMemory;
	void * m_pGAMemHandle_ScaledY;
	void * m_pGAMemHandle_ScaledCbCr;
	void * m_pGAMemHandle_RotateBuf;
	void * m_pGAMemoryHandle;
	struct AllThreadContext* m_pAllThreadContextRotate;
	struct VIDEO_FRAME tOutVideoFrameData;
	enum VIDEO_DATA_FORMAT m_eVideoDataFormat;
	enum MD_PIXEL_FORMAT m_eColorFormat;
	int m_iRotateTableReadCountSize_90;
	int m_iRotateTableReadCountSize_270;
	// for tiled tables 90 degree
	int* TiledToLinearTableY;
	int* TiledToLinearTableUV;
	//Rotated tables for 90
	int * TiledRotatedTableY90Deg;
	int * TiledRotatedTableUV90Deg;
	//Rotated tables for 270
	int* TiledRotatedTableY270Deg;
	int* TiledRotatedTableUV270Deg;
	//Rotated tables for 180
	int* TiledRotatedTable180Deg;
	int m_iScaledWidth_90_270;
	int m_iScaledHeight_90_270;
	//Enable Rotation For MSTAR-X10P Platform
	gboolean m_bIsRotationEnabledForTiledFormat;
	gboolean m_bIsApplyRotationForOptimizations;
	gboolean m_IsEnableGAMemoryForTiledRotation;
	int m_iTargetScaledWidth;
	int m_iTargetScaledHeight;
	int m_iRotationThreadCount;
	int m_iRotationGAScalledBuffer_GFX1;
	int m_iRotationXY;
	// For FOXB 270 Deg performance Enhancement
	int m_iRotationYRaise;
	unsigned long long g_frames;
	unsigned long long g_total_time;
	unsigned long long g_rot_time;
	int m_iCodecId;
	unsigned int m_iBitDepth;
	// Enable/Disable Rotation prints
	gboolean m_bIsRotationPrintsEnabled;
	// for thread affinity
	unsigned int m_imask;
	gboolean m_bIsTheadAffinity;
	int m_iCpuCoreCount;
	// Enable CacheMiss related Optimizations to reduce CPU Usage
	gboolean m_bIsCacheMissOptEnabled;
	// System Memory to avoid GA memory cache misses
	unsigned char *m_pSysOutputYBuffer;
	unsigned char *m_pSysOutputCbCrBuffer;
	// Buffer Pointers to choose either GA Memory or System Memory
	unsigned char *m_pOutputYBufPtr;
	unsigned char *m_pOutputCbCrBufPtr;
	// For Cache Flush
	unsigned int m_ihResolution;
	unsigned int m_ivResolution;
	// For FOXP 180 Deg Optimized
	gboolean m_bIs180DegOptimized;
	gboolean m_bEnable180DegOpti;
	// For FOXB JAVA0 memory usage for GA
	gboolean m_bIsJava0MemoryUsedForGA;
	// FOXP/FOXB, For SkipOptimizations
	int m_iSkipFactor;
	// Decide Interlaced scan type
	gboolean m_bIsInterlacedScanType;
	// FOXP/FOXB 90/270 Deg skp Optimized
	gboolean m_bIsOptimized;
	void* m_hUmpHandle;
	gboolean m_bIsLowResolution;
	gboolean m_bIsCallInterScanFunc;
	//Very low resolution quality improvement < 176x144
	gboolean m_IsVeryLowResolutionOpt;
	int m_iTableWidth_90_270;
	int m_iTableHeight_90_270;
	// To handle frame size > 1920*1088
	gboolean m_bIsSIZEMoreThanSupported;
	// To handle FPS > 30
	int m_iFramesPerSec;
	gboolean m_bIsFPSMoreThanSupported;
	float m_FPSRatio;
	float m_FPSAccumated;
#ifdef  ENABLE_TBM
	tbm_bufmgr bufmgr_AP;
	tbm_bo bo_AP, bo_AP_CbCr;
	tbm_bo_handle bo_handle_AP, bo_handle_AP_CbCr;
	int drm_fd;
	int boAP_key, boAP_CbCr_key;
	Display *disp;
#endif	//ENABLE_TBM
#ifdef ENABLE_LOCAL_ROTATE_BUFFER
	unsigned char * pLocalRotateBuffer_Y;
	unsigned char * pLocalRotateBuffer_CbCr;
#endif	//ENABLE_LOCAL_ROTATE_BUFFER
};

struct VideoFrameRotateContext* videoframe_rotate_create();
void videoframe_rotate_destroy(struct VideoFrameRotateContext* context);
gboolean videoframe_rotate_open(
	struct VideoFrameRotateContext* context,
	gboolean bIsVideoRotationEnabled,
	int iRotationDegree,
	int iOriginalWidth,
	int iOriginalHeight,
	int iCodecId,
	gboolean bIsInterlacedScanType,
	int linesize_open);

void videoframe_rotate_close(struct VideoFrameRotateContext* context);
gboolean videoframe_rotate_apply(struct VideoFrameRotateContext* context, int HwDecoderHandle, struct VIDEO_FRAME* pOutputFrame, int RotationDegree, int *piFrameDone);
void videoframe_rotate_set_degree(struct VideoFrameRotateContext* context, int RotationDegree);
int  videoframe_rotate_get_scaled_width(struct VideoFrameRotateContext* context);
int  videoframe_rotate_get_scaled_height(struct VideoFrameRotateContext* context);
void videoframe_rotate_update_rotate_angle_change_state(struct VideoFrameRotateContext* context, gboolean bIsRotateAngleChanged);
gboolean videoframe_rotate_is_interlaced_scan_type(struct VideoFrameRotateContext* context);
gboolean videoframe_rotate_can_support(struct VideoFrameRotateContext* context, int iFramesPerSec, int frm_height, int frm_width);


#endif // __VIDEOFRAMEROTATE_H__

