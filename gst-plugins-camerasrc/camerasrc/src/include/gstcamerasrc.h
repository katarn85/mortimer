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

#ifndef __GSTCAMERASRC_H__
#define __GSTCAMERASRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gst/interfaces/colorbalance.h>
#include <gst/interfaces/cameracontrol.h>

#include "camerasrc.h"

#ifndef _SPEED_UP_RAW_CAPTURE
#define _SPEED_UP_RAW_CAPTURE
#endif /* _SPEED_UP_RAW_CAPTURE */

G_BEGIN_DECLS
#define GST_TYPE_CAMERA_SRC             (gst_camerasrc_get_type())
#define GST_CAMERA_SRC(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CAMERA_SRC,GstCameraSrc))
#define GST_CAMERA_SRC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CAMERA_SRC,GstCameraSrcClass))
#define GST_IS_CAMERA_SRC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CAMERA_SRC))
#define GST_IS_CAMERA_SRC_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CAMERA_SRC))


/**
 * Quality.
 */
typedef enum {
	GST_CAMERASRC_QUALITY_LOW,   /**< Low quality */
	GST_CAMERASRC_QUALITY_HIGH,  /**< High quality */
} GstCameraSrcQuality;

#define MAX_USR_BUFFER_NUM              12

typedef struct _GstCameraSrc GstCameraSrc;
typedef struct _GstCameraSrcClass GstCameraSrcClass;
typedef struct _GstCameraBuffer GstCameraBuffer;

/* global info */
struct _GstCameraBuffer {
	GstBuffer buffer;
	int v4l2_buffer_index;
	GstCameraSrc *camerasrc;
	gboolean is_alloc_data;
};

struct _GstCameraSrc
{
	GstPushSrc element;

	/*private*/
	void *v4l2_handle;                      /**< video4linux2 handle */
	int mode;
	int sensor_mode;                        /**< sensor mode - camera or movie(camcorder) */
	gboolean vflip;                         /**< flip camera input vertically */
	gboolean hflip;                         /**< flip camera input horizontally */
	gboolean firsttime;

	int main_buf_sz;
	int cap_count_current;                  /**< current capture count */
	int cap_count_reverse;                  /**< current capture count (reverse counting) */
	unsigned long cap_next_time;            /**< next shot time for capture */
	GQueue *command_list;                   /**< Command list(Start capture, Stop capture) queue */

	GCond *cond;
	GMutex *mutex;

	/*camera property*/
	int width;                              /**< Width */
	int height;                             /**< Height */
	int fps;                                /**< Video source fps */

	guint32 fourcc;                         /**< Four CC */
	int pix_format;                         /**< Image format of video source */
	int colorspace;                         /**< Colorspace of video source */
	int high_speed_fps;                     /**< Video source fps for high speed recording */
	gboolean fps_auto;                      /**< Auto Video source fps */

	gboolean req_negotiation;               /**< Video source fps for high speed recording */
	int camera_id;
	int rotate;                             /**< Video source rotate */
	gboolean use_rotate_caps;               /**< Use or not rotate value in caps */
	int external_videofd;                   /**< video FD from external module */

	GCond *buffer_cond;
	GMutex *buffer_lock;
	gboolean buffer_running;                /* with lock */
	gint num_live_buffers;                  /* with lock */
	guint buffer_count;
	gboolean bfirst;                        /* temp */
#ifdef _SPEED_UP_RAW_CAPTURE
	gboolean cap_stream_diff;               /* whether preview and capture streams are different each other */
#endif
	GQueue *pad_alloc_list;
	GMutex *pad_alloc_mutex;
	gint current_buffer_data_index;
	gboolean first_invokation;

	GstBuffer *usr_buffer[MAX_USR_BUFFER_NUM];
	gboolean use_pad_alloc;
	guint num_alloc_buf;

	/* Colorbalance , CameraControl interface */
	GList *colors;
	GList *camera_controls;


	/*capture property*/
	guint32 cap_fourcc;                     /**< gstreamer fourcc value(GST_MAKE_FOURCC format) for raw capturing */
	GstCameraSrcQuality cap_quality;        /**< Capture quality */
	int cap_width;                          /**< Capture width */
	int cap_height;                         /**< Capture height */
	int cap_interval;                       /**< Capture interval */
	int cap_count;                          /**< Capture count */
	int cap_jpg_quality;                    /**< Capture quality for jpg compress ratio */
	gboolean cap_provide_exif;              /**< Is exif provided? */
	buf_share_method_t buf_share_method;    /**< buffer share method */

	/*etc property*/
	gboolean signal_still_capture;          /**< enable still capture signal */
	gboolean create_jpeg;
	GMutex *jpg_mutex;
};

struct _GstCameraSrcClass {
	GstPushSrcClass parent_class;
	/* signals */
	void (*still_capture) (GstElement *element, GstBuffer *main/*, GstBuffer *sub, GstBuffer *scrnl*/);
	void (*nego_complete) (GstElement *element);
	void (*register_trouble) (GstElement *element);
};

typedef enum {
	INTERFACE_NONE,
	INTERFACE_COLOR_BALANCE,
	INTERFACE_CAMERA_CONTROL,
} GstInterfaceType;


void gst_camerasrc_set_capture_command(GstCameraSrc* camerasrc, GstCameraControlCaptureCommand cmd);


GType gst_camerasrc_get_type(void);

G_END_DECLS

#endif /* __GSTCAMERASRC_H__ */
