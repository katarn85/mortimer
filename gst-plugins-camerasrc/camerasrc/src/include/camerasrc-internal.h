/*
 * camerasrc
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jeongmo Yang <jm80.yang@samsung.com>
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
 */

#ifndef __CAMERASRC_INTERNAL_H__
#define __CAMERASRC_INTERNAL_H__

#include "camerasrc-common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Video 4 linux control ID definitions (Extended by kernel team)
 * extended pixel format for V4l2
 */


/**
 * Miscellaneous camera-dependent definitions
 */
#define CAMERASRC_DEV_INDEX_MAX                 2
#define CAMERASRC_PREVIEW_BUFFER_NUM            7
#define CAMERASRC_VIDEO_BUFFER_NUM              7
#define CAMERASRC_STILL_BUFFER_NUM              1
#define CAMERASRC_REGISTER_SET_RETRY_NO         200
#define CAMERASRC_TIMEOUT_CRITICAL_VALUE        5000
#define CAMERASRC_V4L2_PREVIEW_PIX_FMT_DEFAULT          V4L2_PIX_FMT_YUYV
#define CAMERASRC_V4L2_JPEG_CAPTURE_PIX_FMT_DEFAULT     V4L2_PIX_FMT_JPEG
#define CAMERASRC_V4L2_JPG_YUV_CAPTURE_PIX_FMT_DEFAULT  V4L2_PIX_FMT_MJPEG
#define CAMERASRC_V4L2_RGB_CAPTURE_PIX_FMT_DEFAULT      V4L2_PIX_FMT_RGB565
#define CAMERASRC_SUPPORT_JPEG_CAMERA

#define CAMERASRC_SYNC_KEY_PATH                 "USB_WEBCAM"
#define CAMERASRC_SYNC_KEY_PREFIX               0x55

/*
 * |------------------------------------------------------------|
 * |             | Primary   | Secondary | Extension| Unknown   |
 * |------------------------------------------------------------|
 * |   Index     |    2      |     1     |    0     |    -1     |
 * |------------------------------------------------------------|
 *
 * |------------------------------------------------------------|
 * |             |    0      |     1     |    2     |     3     |
 * |------------------------------------------------------------|
 * |   ID        | Unknown   | Secondary | Primary  | Extension |
 * |------------------------------------------------------------|
 */

static int _camerasrc_dev_index[CAMERASRC_DEV_ID_NUM][CAMERASRC_DEV_RECOG_NUM] =
{
    {CAMERASRC_DEV_ID_PRIMARY,      0},
    {CAMERASRC_DEV_ID_SECONDARY, 0},
    {CAMERASRC_DEV_ID_EXTENSION, 2},
    {CAMERASRC_DEV_ID_UNKNOWN, -1},
};


/**
 * preset size index
 * |-------------------------------------------------------------------------|
 * |             |             |  YUV422P  |  YUV420P |  SRGGB8   | SRGGB10  |
 * |-------------------------------------------------------------------------|
 * |             | Highquality |     O     |    X     |     X     |     X    |
 * |    RAW      |-----------------------------------------------------------|
 * |             | Normal      |     O     |    X     |     X     |     X    |
 * |-------------------------------------------------------------------------|
 * |             | Highquality |     X     |    X     |     O     |     X    |
 * |    JPEG     |-----------------------------------------------------------|
 * |             | Normal      |     X     |    X     |     X     |     X    |
 * |-------------------------------------------------------------------------|
 */
static char _camerasrc_match_col_to_pix[CAMERASRC_DEV_ID_EXTENSION][CAMERASRC_COL_NUM][CAMERASRC_PIX_NUM][CAMERASRC_QUALITY_NUM] =
{
    {/*SECONDARY*/
        /*422P,     420P,      SRGGB8,    SRGGB10*/
        {{0x1,0x0}, {0x0,0x0}, {0x0,0x0}, {0x0,0x0}},   /**< RAW */
        {{0x0,0x0}, {0x0,0x0}, {0x0,0x0}, {0x0,0x0}},   /**< JPG */
    },
    {/*PRIMARY*/
        /*422P,     420P,      SRGGB8,    SRGGB10 */
        {{0x1,0x0}, {0x0,0x0}, {0x0,0x0}, {0x0,0x0}},   /**< RAW */
        {{0x0,0x0}, {0x0,0x0}, {0x0,0x1}, {0x0,0x1}},   /**< JPG */
    },
}; /* {Normal quality, High quality} */

/*
 * |--------------------------------------------------------------------------------|
 * |               | Support   | MAX value | MIN value |   CID                      |
 * |--------------------------------------------------------------------------------|
 * | Brightness    |    1      |     8     |    0      | V4L2_CID_BRIGHTNESS        |
 * |--------------------------------------------------------------------------------|
 * | Contrast      |    1      |     9     |    0      |    -1                      |
 * |--------------------------------------------------------------------------------|
 * | Digital zoom  |    1      |    100    |    0      |  V4L2_CID_BASE+29          |
 * |--------------------------------------------------------------------------------|
 * | Optical zoom  |    0      |    -1     |   -1      |    -1                      |
 * |--------------------------------------------------------------------------------|
 * | White balance |    1      |     9     |    0      |  V4L2_CID_DO_WHITE_BALANCE |
 * |--------------------------------------------------------------------------------|
 * | Color tone    |    1      |    14     |    0      |  V4L2_CID_BASE+28          |
 * |--------------------------------------------------------------------------------|
 * | Program mode  |    0      |    -1     |   -1      |    -1                      |
 * |--------------------------------------------------------------------------------|
 * | Flip          |    1      |     1     |    0      |  V4L2_CID_FLIP             |
 * |--------------------------------------------------------------------------------|
 * | Flash         |    1      |     2     |    0      |  V4L2_CID_BASE+32          |
 * |--------------------------------------------------------------------------------|
 */


/* CUSTOM V4L2 CONTROL ID DEFINITIONS (END) */
static int _camerasrc_ctrl_list[CAMERASRC_CTRL_FLIP][CAMERASRC_CTRL_NUM][CAMERASRC_CTRL_PROPERTY_NUM] =
{       /* { SUPPORT, MAX_VALUE, MIN_VALUE, CID, CURRENT_VALUE } */
#if 0
    {   /* Primary camera */
        {-1, 4, -4, V4L2_CID_EXPOSURE, 0},              /* Brightness */
        {-1, 3, -3, V4L2_CID_CONTRAST, 0},              /* Contrast */
        {0,  -1, -1, -1, -1},           /* Digital zoom */
        {0,  -1, -1, -1, -1},                           /* Optical zoom */
        {-1, CAMERASRC_WHITEBALANCE_TUNGSTEN, CAMERASRC_WHITEBALANCE_AUTO, V4L2_CID_DO_WHITE_BALANCE, CAMERASRC_WHITEBALANCE_AUTO},          /* White balance */
        {-1, CAMERASRC_COLORTONE_SET_CBCR, CAMERASRC_COLORTONE_NONE, V4L2_CID_COLORFX, CAMERASRC_COLORTONE_NONE},                                /* Colortone */
        {0,  -1, -1, -1, -1},      /* program mode */
        {0,  -1, -1, -1, -1},                           /* Flip. V4L2_CID_VFLIP/HFLIP */
        {0,  -1, -1, -1, -1},                           /* PARTCOLOR_SRC */
        {0,  -1, -1, -1, -1},                           /* PARTCOLOR_DST */
        {0,  -1, -1, -1, -1},                           /* PARTCOLOR_MODE */
        {0,  -1, -1, -1, -1},          /* ANTI_HANDSHAKE */
        {0,  -1, -1, -1, -1},                 /* WIDE_DYNAMIC_RANGE */
        {-1, 3, -3, V4L2_CID_SATURATION, 0},            /* SATURATION */
        {-1, 3, -3, V4L2_CID_SHARPNESS, 0},             /* SHARPNESS */
        {0,  -1, -1, -1, -1},         /* ISO */
        {0,  -1, -1, -1, -1},    /* PHOTOMETRY */
    },
    {   /* Secondary camera */
        {0,  -1, -1, V4L2_CID_BRIGHTNESS, -1},   /* Brightness */
        {0,  -1, -1, V4L2_CID_CONTRAST, -1},            /* Contrast */
        {0,  -1, -1, V4L2_CID_ZOOM_ABSOLUTE, -1},       /* Digital zoom */
        {0,  -1, -1, -1, -1},                           /* Optical zoom */
        {0,  -1, -1, V4L2_CID_DO_WHITE_BALANCE, -1},/* White balance */
        {0,  -1, -1, V4L2_CID_COLORFX, -1},             /* Colortone */
        {0,  -1, -1, -1, -1},   /* program mode */
        {0,  -1, -1, -1, -1},                           /* Flip */
        {0,  -1, -1, -1, -1},                           /* PARTCOLOR_SRC */
        {0,  -1, -1, -1, -1},                           /* PARTCOLOR_DST */
        {0,  -1, -1, -1, -1},                           /* PARTCOLOR_MODE */
        {0,  -1, -1, -1, -1},       /* ANTI_HANDSHAKE */
        {0,  -1, -1, -1, -1},              /* WIDE_DYNAMIC_RANGE */
        {0,  -1, -1, V4L2_CID_SATURATION, -1},          /* SATURATION */
        {0,  -1, -1, V4L2_CID_SHARPNESS, -1},           /* SHARPNESS */
        {0,  -1, -1, -1, -1},          /* ISO */
        {0,  -1, -1, -1, -1},     /* PHOTOMETRY */
    },
#else
    {   /* Primary camera */
        {0,  -1, -1, -1, -1},              /* Brightness */
        {0,  -1, -1, -1, -1},              /* Contrast */
        {0,  -1, -1, -1, -1},            /* Colortone */
        {0,  -1, -1, -1, -1},            /* SATURATION */
        {0,  -1, -1, -1, -1},             /* SHARPNESS */
        {0,  -1, -1, -1, -1},            /* Flip. V4L2_CID_VFLIP/HFLIP */        
    },
    {   /* Secondary camera */
	{0,  -1, -1, -1, -1},			   /* Brightness */
	{0,  -1, -1, -1, -1},			   /* Contrast */
	{0,  -1, -1, -1, -1},			 /* Colortone */
	{0,  -1, -1, -1, -1},			 /* SATURATION */
	{0,  -1, -1, -1, -1},			  /* SHARPNESS */
	{0,  -1, -1, -1, -1},			 /* Flip. V4L2_CID_VFLIP/HFLIP */	  
    },
#endif
};

static char _camerasrc_misc_func_list[CAMERASRC_DEV_ID_EXTENSION][CAMERASRC_OP_REGISTER_VALUE][CAMERASRC_COL_NUM][CAMERASRC_MISC_FUNC_NUM] =
{
    {/*Primary*/
        /*Raw,       JPG*/
        {{0x0,0x0}, {0x0,0x0}}, /*Preview*/
        {{0x0,0x0}, {0x0,0x0}}, /*Capture*/
        {{0x0,0x0}, {0x0,0x0}}, /*Video*/
    },
    {/*Secondary*/
        {{0x0,0x0}, {0x0,0x0}}, /*Preview*/
        {{0x0,0x0}, {0x0,0x0}}, /*Capture*/
        {{0x0,0x0}, {0x0,0x0}}, /*Video*/
    }/*{{Signal,Skip}, {Signal,Skip}}*/
};

#ifdef __cplusplus
}
#endif

#endif /*__CAMERASRC_INTERNAL_H__*/
