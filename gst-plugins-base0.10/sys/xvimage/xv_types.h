/**************************************************************************

Copyright 2010 - 2013 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: Boram Park <boram1288.park@samsung.com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/


#ifndef __XV_TYPES_H__
#define __XV_TYPES_H__

#include "linux/videodev2.h"
#include "drm/sdp_frc.h"
#define XV_PUTIMAGE_HEADER	0xDEADCD01
#define XV_PUTIMAGE_VERSION	0x00010001

/* Return Values */
#define XV_OK 0
#define XV_HEADER_ERROR -1
#define XV_VERSION_MISMATCH -2

/* Video Mode */
#define DISPLAY_MODE_DEFAULT                                      0
#define DISPLAY_MODE_PRI_VIDEO_ON_AND_SEC_VIDEO_FULL_SCREEN       1
#define DISPLAY_MODE_PRI_VIDEO_OFF_AND_SEC_VIDEO_FULL_SCREEN      2

/* Buffer Type */
#define XV_BUF_TYPE_DMABUF  0
#define XV_BUF_TYPE_LEGACY  1

/* Data structure for XvPutImage / XvShmPutImage */
typedef struct {
	unsigned int _header; /* for internal use only */
	unsigned int _version; /* for internal use only */

	unsigned int YBuf;
	unsigned int CbBuf;
	unsigned int CrBuf;
	unsigned int BufType;
} XV_PUTIMAGE_DATA, * XV_PUTIMAGE_DATA_PTR;
 
typedef enum
{
    XV_DRM_COLORFORMAT_RGB444 = V4L2_DRM_COLORFORMAT_RGB444,	/**< RGB 444 */
    XV_DRM_COLORFORMAT_YUV444 = V4L2_DRM_COLORFORMAT_YUV444,	/**< YUV 444 */
    XV_DRM_COLORFORMAT_YUV422 = V4L2_DRM_COLORFORMAT_YUV422,	/**< YUV 422 */
    XV_DRM_COLORFORMAT_YUV420 = V4L2_DRM_COLORFORMAT_YUV420,	/**< YUV 420 */
    XV_DRM_COLORFORMAT_YC = V4L2_DRM_COLORFORMAT_YC,		/**< YC */ 
} FOXPXvDrmColorFormatType;

typedef struct _FOXPXvDecInfo
{
    unsigned int YBuf;
    unsigned int CbBuf;
    unsigned int CrBuf;

    unsigned int framerate;
    unsigned int scantype;
    unsigned int display_index;
    FOXPXvDrmColorFormatType colorformat;
}FOXPXvDecInfo;

typedef enum 
{
    XV_3DMODE_2D = CD_3D_MODE_OFF,
    XV_3DMODE_SIDEBYSIDE = CD_3D_MODE_SIDE_BY_SIDE,
    XV_3DMODE_TOP_BOTTOM = CD_3D_MODE_TOP_BOTTOM,
    XV_3DMODE_FRAME_SEQUENTIAL = CD_3D_MODE_FRAME_SEQUENTIAL,
    XV_3DMODE_FRAME_PACKING = CD_3D_MODE_FRAME_PACKING,
    XV_3DMODE_2D_3D_CONVERSION = CD_3D_MODE_2D_3D_CONVERSION,
    XV_3DMODE_CHECKER_BOARD = CD_3D_MODE_CHECKER_BOARD,
    XV_3DMODE_LINEBYLINE = CD_3D_MODE_LINE_BY_LINE,
    XV_3DMODE_VERTICAL_STRIPE = CD_3D_MODE_VERTICAL_STRIPE,
    XV_3DMODE_FRAME_DUAL = CD_3D_MODE_FRAME_DUAL,
    XV_3DMODE_INIT = CD_3D_MODE_INIT,
    XV_3DMODE_MAX,
}FOXPXv3dMode;

typedef enum 
{
    XV_3D_FORMAT_MPO = CD_3D_FORMAT_MPO,       // MPO(Photo) 형식 3D Auto Detection 을 위한 포맷 구분 Enum
    XV_3D_FORMAT_SVAF = CD_3D_FORMAT_SVAF,      // SVAF(Movie) 형식 3D Auto Detection 을 위한 포맷 구분 Enum
    XV_3D_FORMAT_M2TS = CD_3D_FORMAT_M2TS,      // M2TS(Photo) 형식 3D Auto Detection 을 위한 포맷 구분 Enum
    XV_3D_FORMAT_FILENAME = CD_3D_FORMAT_FILENAME,      // File Name 기준 3D Auto Detection 을 위한 포맷 구분 Enum
    XV_3D_FORMAT_MVC = CD_3D_FORMAT_MVC,       //Multi Video Codec
    XV_3D_FORMAT_MVC_SEAMLESS = CD_3D_FORMAT_MVC_SEAMLESS,  // Multi Video Codec - for seamless
    XV_3D_FORMAT_DUAL3DCH = CD_3D_FORMAT_DUAL3DCH,      //KR3D/Dual stream
    XV_3D_FORMAT_GRAPHIC = CD_3D_FORMAT_GRAPHIC,       // 3D Graphic 지원. SmartHub 등에서 3D Game 지원 포맷.
#if 1 //SUPPORT_NTV
    XV_3D_FORMAT_DUAL_FRAME = CD_3D_FORMAT_DUAL_FRAME,    // Dual Watch ( NTV ) - 2개 Source를 3D 영상으로 만들어 각각 보는 방식.
    XV_3D_FORMAT_DUAL_PLAY = CD_3D_FORMAT_DUAL_PLAY,     // Enjoy Game ( NTV) - 1개 Source의 3D 영상을 L/R 로 분리하여 각각 보는 방식.
#endif
    XV_3D_FORMAT_SCREEN_4DIVISION = CD_3D_FORMAT_SCREEN_4DIVISION,  // UDTV - 4분할 화면 대응
    XV_3D_FORMAT_MAX = CD_3D_FORMAT_MAX,
}FOXPXv3dFormat;

typedef struct _FOXPXvVideo3dInfo
{
    unsigned int Freq;
    unsigned int bIsComponent;
    FOXPXv3dMode e3dMode;
    FOXPXv3dFormat e3dFormat;
}FOXPXvVideo3dInfo;
static void XV_PUTIMAGE_INIT_DATA(XV_PUTIMAGE_DATA_PTR data)
{
	data->_header = XV_PUTIMAGE_HEADER;
	data->_version = XV_PUTIMAGE_VERSION;
}

#endif /* __XV_TYPES_H__ */
