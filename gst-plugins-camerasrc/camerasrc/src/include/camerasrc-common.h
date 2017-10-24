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

#ifndef __CAMERASRC_COMMON_H__
#define __CAMERASRC_COMMON_H__

#include <stdio.h>
#include <malloc.h>
#include <pthread.h>
#include <errno.h>      /*EXXX*/
#include <sys/ioctl.h>  /*ioctl*/
#include <string.h>     /*memcpy*/

#include <sys/types.h>  /*open*/
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>     /*mmap*/
#include <sys/mman.h>   /*alloc series, free..*/
#include <sys/time.h>   /*gettimeofday*/
#include <math.h>       /*log2*/
#include <gst/gst.h>

#undef __ASM_ARM_TYPES_H
#undef __ASSEMBLY_
#undef _I386_TYPES_H

#include <asm/types.h>
#include <linux/videodev2.h>    /* V4L2 APIs */
#ifdef SUPPORT_EXYNOS
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#endif

#include "camerasrc.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Memory utility definitions
 */
#if !defined (PAGE_SHIFT)
    #define PAGE_SHIFT sysconf(_SC_PAGESIZE)
#endif
#if !defined (PAGE_SIZE)
    #define PAGE_SIZE (1UL << PAGE_SHIFT)
#endif
#if !defined (PAGE_MASK)
    #define PAGE_MASK (~(PAGE_SIZE-1))
#endif

#define PAGE_ALIGN(addr)    (((addr)+PAGE_SIZE-1)&PAGE_MASK)
#define CLEAR(x)            memset (&(x), 0, sizeof (x))

#define CAMERASRC_MAX_WIDTH                     2560
#define CAMERASRC_MAX_HEIGHT                    1920
#define CAMERASRC_CID_NOT_SUPPORT               -1
#define CAMERASRC_USRPTR_MAX_BUFFER_NUM         12
#define CAMERASRC_MAX_FILENAME_LEN              255
#define CAMERASRC_DEV_NODE_PREFIX               "/dev/video"
#define CAMERASRC_DEV_FD_INIT                   -1
#define CAMERASRC_DEV_ON_ACCESS_STR             "ONACCESS"
#define CAMERASRC_DEV_RELEASED_STR              "RELEASED"
#define CAMERASRC_OPENED_CHK_FILENAME           "/tmp/.dev_chk"
#define CAMERASRC_THREAD_KILL                   -999
#define CAMERASRC_DEV_FD_EMERGENCY_CLOSED       -999
#define CAMERASRC_ERRMSG_MAX_LEN                128
#define CAMERASRC_FRAME_DUMP_PATH                   "/tmp/"
#define CAMERASRC_DBG_SCRIPT_PATH               "/mnt/mmc/cam_dbg_script"
#define CAMERASRC_PRIMARY_BASIC_INFO_PATH       "/tmp/.camprimarybasicinfo"
#define CAMERASRC_PRIMARY_MISC_INFO_PATH        "/tmp/.camprimarymiscinfo"
#define CAMERASRC_PRIMARY_EXTRA_INFO_PATH        "/tmp/.camprimaryextrainfo"
#define CAMERASRC_SECONDARY_BASIC_INFO_PATH     "/tmp/.camsecondarybasicinfo"
#define CAMERASRC_SECONDARY_MISC_INFO_PATH      "/tmp/.camsecondarymiscinfo"
#define CAMERASRC_SECONDARY_EXTRA_INFO_PATH      "/tmp/.camsecondaryextrainfo"
#define CAMERASRC_MAX_IMAGE_BUFFER_PLANES	3

#define USE_ROTATION_MODE			0

/*#define USE_SKIP_FRAME*/                        /*< Skip frame toggle */
/*#define USE_IOCTL_DEBUG*/                       /*< For debugging ioctl name, argument, address, etc */
/*#define USE_FRAME_COPY_BOUNDARY_CHECK*/         /*< Copy boundary checks occurs seg fault when overrun */
/*#define USE_SKIP_FRAME_AT_RAW_FRAME*/           /*< In pumping raw frame, initial 2-3 frames are darker. so skip it */
/*#define USE_CAMERASRC_FRAME_DUMP*/              /*< Debug system annoying me. Use printf!!!! */
/*#define USE_USERPTR_DEBUG*/
/*#define ENABLE_Q_ERROR*/

#ifndef GST_CAT_DEFAULT
GST_DEBUG_CATEGORY_EXTERN(camerasrc_debug);
#define GST_CAT_DEFAULT camerasrc_debug
#endif /* GST_CAT_DEFAULT */


#define camsrc_info(msg, args...)          GST_INFO(msg, ##args)
#define camsrc_warning(msg, args...)       GST_WARNING(msg, ##args)
#define camsrc_error(msg, args...)         GST_ERROR(msg, ##args)
#define camsrc_critical(msg, args...)      GST_ERROR(msg, ##args)
#define camsrc_assert(condition) { \
	if (!(condition)) { \
		GST_ERROR("failed [%s]", #condition); \
	} \
}

/*
 * Values for internal
 */
enum camerasrc_op_mode_t {
    CAMERASRC_OP_PREVIEW = 0,
    CAMERASRC_OP_CAPTURE,
    CAMERASRC_OP_VIDEO,
    CAMERASRC_OP_REGISTER_VALUE,
    CAMERASRC_OP_NUM,
};

/*
 * Values for internal
 */
enum camerasrc_ctrl_property_t{
    CAMERASRC_CTRL_SUPPORT = 0,
    CAMERASRC_CTRL_MAX_VALUE,
    CAMERASRC_CTRL_MIN_VALUE,
    CAMERASRC_CTRL_CID_VALUE,
    CAMERASRC_CTRL_CURRENT_VALUE,
    CAMERASRC_CTRL_PROPERTY_NUM,
};

/*
 * Values for internal
 */
enum camerasrc_quality_t{
    CAMERASRC_QUALITY_NORMAL = 0,
    CAMERASRC_QUALITY_HIGH,
    CAMERASRC_QUALITY_NUM,
};

enum camerasrc_dev_recog_t{
    CAMERASRC_DEV_RECOG_ID = 0,
    CAMERASRC_DEV_RECOG_INDEX,
    CAMERASRC_DEV_RECOG_NUM,
};

/**
 * Phase, camerasrc consist of two phase, running and non-running.
 */
typedef enum {
    CAMERASRC_PHASE_RUNNING = 0,
    CAMERASRC_PHASE_NON_RUNNING,
    CAMERASRC_PHASE_NUM,
} _camerasrc_phase_t;

typedef enum {
    CAMERASRC_MISC_STILL_SIGNAL = 0,
    CAMERASRC_MISC_SKIP_FRAME,
    CAMERASRC_MISC_FUNC_NUM,
} _camerasrc_misc_func_t;

typedef enum{
    _CAMERASRC_CMD_SUPPORT_JPEG_ENCODING,
    _CAMERASRC_CMD_JPEG_COMPRESS_RATIO,
    _CAMERASRC_CMD_CTRL,
#ifdef SUPPORT_CAMERA_SENSOR_MODE
    _CAMERASRC_CMD_SENSOR_MODE,
#endif
    _CAMERASRC_CMD_VFLIP,
    _CAMERASRC_CMD_HFLIP,
    _CAMERASRC_CMD_NUM,
}_camsrc_cmd_t;

typedef struct{
    int cid;
    int value;
} _camerasrc_ctrl_t;


// U T I L I T Y   D E F I N I T I O N
/**
 * Mapping device index - Device ID
 */
#define _CAMERASRC_GET_DEV_INDEX(dev_id)        _camerasrc_dev_index[dev_id][CAMERASRC_DEV_RECOG_INDEX]
#define _CAMERASRC_GET_DEV_ID(dev_idx)          _camerasrc_dev_index[dev_idx][CAMERASRC_DEV_RECOG_ID]

/**
 * For colorspace - pixel format combinability check
 */
#define _CAMERASRC_MATCH_COL_TO_PIX(dev_id, colorspace, pixel_fmt, quality)     _camerasrc_match_col_to_pix[dev_id][colorspace][pixel_fmt][quality]

/**
 * For control capability check
 */
#define _CAMERASRC_CHK_SUPPORT_CONTROL(ctrl_id, dev_id)         _camerasrc_ctrl_list[dev_id][ctrl_id][CAMERASRC_CTRL_SUPPORT]
#define _CAMERASRC_MAX_VALUE(ctrl_id, dev_id)                   _camerasrc_ctrl_list[dev_id][ctrl_id][CAMERASRC_CTRL_MAX_VALUE]
#define _CAMERASRC_MIN_VALUE(ctrl_id, dev_id)                   _camerasrc_ctrl_list[dev_id][ctrl_id][CAMERASRC_CTRL_MIN_VALUE]
#define _CAMERASRC_GET_CID(ctrl_id, dev_id)                     _camerasrc_ctrl_list[dev_id][ctrl_id][CAMERASRC_CTRL_CID_VALUE]
#define _CAMERASRC_GET_CURRENT_VALUE(ctrl_id, dev_id)           _camerasrc_ctrl_list[dev_id][ctrl_id][CAMERASRC_CTRL_CURRENT_VALUE]
#define _CAMERASRC_SET_CURRENT_VALUE(ctrl_id, dev_id, value)    _camerasrc_ctrl_list[dev_id][ctrl_id][CAMERASRC_CTRL_CURRENT_VALUE] = value

/**
 * Need miscellaneous function on operation?
 */
#define _CAMERASRC_NEED_MISC_FUNCTION(dev_id, operation, colorspace, misc_func)         _camerasrc_misc_func_list[dev_id][operation][colorspace][misc_func]

/**
 * Utility definitions
 */
#define CAMERASRC_SET_STATE(handle, state) { \
	handle->prev_state = handle->cur_state; \
	handle->cur_state = state; \
	camsrc_info("Set state [%d] -> [%d]", handle->prev_state, handle->cur_state); \
}
#define CAMERASRC_SET_PHASE(handle, phase)      handle->cur_phase = phase;
#define CAMERASRC_STATE(handle)                 (handle->cur_state)
#define CAMERASRC_PREV_STREAM_STATE(handle)     -1
#define CAMERASRC_PHASE(handle)                 (handle->cur_phase)
#define CAMERASRC_HANDLE(handle)                ((camerasrc_handle_t*) handle)
#define CAMERASRC_CURRENT_DEV_ID(handle)        (handle->dev_id)
#define CAMERASRC_IS_DEV_CLOSED(p) (p->dev_fd == -1 || p->dev_fd == CAMERASRC_DEV_FD_EMERGENCY_CLOSED)

#define YUV422_SIZE(handle) ((handle->format.img_size.dim.height * handle->format.img_size.dim.width) << 1)
#define YUV420_SIZE(handle) ((handle->format.img_size.dim.height * handle->format.img_size.dim.width * 3) >> 1)
#define RGB565_SIZE(handle) ((handle->format.img_size.dim.height * handle->format.img_size.dim.width) << 1)

#define ISO_APPROXIMATE_VALUE(iso_in, iso_approximated) { \
	if(iso_in > 8.909 && iso_in <= 11.22) iso_approximated = 10; \
	else if(iso_in > 11.22 && iso_in <= 14.14) iso_approximated = 12; \
	else if(iso_in > 14.14 && iso_in <= 17.82) iso_approximated = 16; \
	else if(iso_in > 17.82 && iso_in <= 22.45) iso_approximated = 20; \
	else if(iso_in > 22.45 && iso_in <= 28.28) iso_approximated = 25; \
	else if(iso_in > 28.28 && iso_in <= 35.64) iso_approximated = 32; \
	else if(iso_in > 35.64 && iso_in <= 44.90) iso_approximated = 40; \
	else if(iso_in > 44.90 && iso_in <= 56.57) iso_approximated = 50; \
	else if(iso_in > 56.57 && iso_in <= 71.27) iso_approximated = 64; \
	else if(iso_in > 71.27 && iso_in <= 89.09) iso_approximated = 80; \
	else if(iso_in > 89.09 && iso_in <= 112.2) iso_approximated = 100; \
	else if(iso_in > 112.2 && iso_in <= 141.4) iso_approximated = 125; \
	else if(iso_in > 141.4 && iso_in <= 178.2) iso_approximated = 160; \
	else if(iso_in > 178.2 && iso_in <= 224.5) iso_approximated = 200; \
	else if(iso_in > 224.5 && iso_in <= 282.8) iso_approximated = 250; \
	else if(iso_in > 282.8 && iso_in <= 356.4) iso_approximated = 320; \
	else if(iso_in > 356.4 && iso_in <= 449.0) iso_approximated = 400; \
	else if(iso_in > 449.0 && iso_in <= 565.7) iso_approximated = 500; \
	else if(iso_in > 565.7 && iso_in <= 712.7) iso_approximated = 640; \
	else if(iso_in > 712.7 && iso_in <= 890.9) iso_approximated = 800; \
	else if(iso_in > 890.9 && iso_in <= 1122) iso_approximated = 1000; \
	else if(iso_in > 1122 && iso_in <= 1414) iso_approximated = 1250; \
	else if(iso_in > 1414 && iso_in <= 1782) iso_approximated = 1600; \
	else if(iso_in > 1782 && iso_in <= 2245) iso_approximated = 2000; \
	else if(iso_in > 2245 && iso_in <= 2828) iso_approximated = 2500; \
	else if(iso_in > 2828 && iso_in <= 3564) iso_approximated = 3200; \
	else if(iso_in > 3564 && iso_in <= 4490) iso_approximated = 4000; \
	else if(iso_in > 4490 && iso_in <= 5657) iso_approximated = 5000; \
	else if(iso_in > 5657 && iso_in <= 7127) iso_approximated = 6400; \
	else if(iso_in > 7127 && iso_in <= 8909) iso_approximated = 8000; \
	else { \
		camsrc_warning("Invalid parameter(Maybe kernel failure).. give default value, 100");\
		iso_approximated = 100;\
	}\
}

typedef void *(*camerasrc_signal_func_t) (camsrc_handle_t handle);
typedef int (*camerasrc_skip_frame_func_t) (camsrc_handle_t handle, long int timeout, int skip_frame);

typedef struct _camerasrc_handle_t {
	int is_async_open;

	/* device information */
	int dev_fd;
	buf_share_method_t buf_share_method; /* buffer share method */
	int cur_dev_id;
	int errnum;
	int lens_rotation;	/* physical rotation of lens */

	/* state information */
	int prev_stream_state;
	int prev_state;
	int cur_state;
	int cur_phase;

	/* image format information */
	int is_highquality;
	int is_preset;
	camerasrc_resol_name_t resolution;
	camerasrc_format_t format;

	/* buffer information */
	camerasrc_io_method_t io_method;
	camerasrc_usr_buf_t *present_buf;
	int buffer_idx;
	int num_buffers;
	int queued_buffer_count;
	int first_frame;
	struct v4l2_buffer queued_buf_list[CAMERASRC_USRPTR_MAX_BUFFER_NUM];
	camerasrc_buffer_t *buffer;

	/* Jpg Still information */
	camerasrc_skip_frame_func_t skip_frame_func;

	/* fps */
	camerasrc_frac_t timeperframe;

#ifdef SUPPORT_CAMERA_SENSOR_MODE
	/* sensor mode */
	camerasrc_sensor_mode_t sensor_mode;
#endif	

	/* flip */
	int vflip;
	int hflip;

	/* thread safe mechanism */
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} camerasrc_handle_t;

typedef struct {
    int (*_ioctl) (camerasrc_handle_t *handle, int request, void *arg);
    int (*_ioctl_once) (camerasrc_handle_t *handle, int request, void *arg);
    int (*_skip_frame) (camerasrc_handle_t *handle, long int timeout, int skip_frame);
    int (*_copy_frame) (camerasrc_handle_t *handle, camerasrc_buffer_t *src_buffer, camerasrc_buffer_t *dst_buffer, int isThumbnail);
    int (*_set_cmd) (camerasrc_handle_t *handle, _camsrc_cmd_t cmd, void *value);
    int (*_get_cmd) (camerasrc_handle_t *handle, _camsrc_cmd_t cmd, void *value);
} CAMERASRC_DEV_DEPENDENT_MISC_FUNC;

#ifdef __cplusplus
}
#endif

#endif /*__CAMERASRC_COMMON_H__*/
