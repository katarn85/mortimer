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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/gstutils.h>
#include <glib-object.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h> /* sched_yield() */

#include "gstcamerasrc.h"
#include "gstcamerasrccontrol.h"
#include "gstcamerasrccolorbalance.h"


/******************************************************************************
 * Definitions
 *******************************************************************************/
GST_DEBUG_CATEGORY (camerasrc_debug);
#define GST_CAT_DEFAULT camerasrc_debug

#define USE_FRAME_SAFETY_MARGIN

#if defined (USE_FRAME_SAFETY_MARGIN)
#define FRAME_SAFETY_MARGIN 4096
#else
#define FRAME_SAFETY_MARGIN 0
#endif

#ifndef YUV422_SIZE
#define YUV422_SIZE(width,height) ( ((width)*(height)) << 1 )
#endif

#ifndef YUV420_SIZE
#define YUV420_SIZE(width,height) ( ((width)*(height)*3) >> 1 )
#endif

#define SCMN_CS_YUV420              1 /* Y:U:V 4:2:0 */
#define SCMN_CS_I420                SCMN_CS_YUV420 /* Y:U:V */
#define SCMN_CS_NV12                6
#define SCMN_CS_NV12_T64X32         11 /* 64x32 Tiled NV12 type */
#define SCMN_CS_UYVY                100
#define SCMN_CS_YUYV                101
#define SCMN_CS_YUY2                SCMN_CS_YUYV

/* max channel count *********************************************************/
#define SCMN_IMGB_MAX_PLANE         (4)

/* image buffer definition ***************************************************

    +------------------------------------------+ ---
    |                                          |  ^
    |     a[], p[]                             |  |
    |     +---------------------------+ ---    |  |
    |     |                           |  ^     |  |
    |     |<---------- w[] ---------->|  |     |  |
    |     |                           |  |     |  |
    |     |                           |        |
    |     |                           |  h[]   |  e[]
    |     |                           |        |
    |     |                           |  |     |  |
    |     |                           |  |     |  |
    |     |                           |  v     |  |
    |     +---------------------------+ ---    |  |
    |                                          |  v
    +------------------------------------------+ ---

    |<----------------- s[] ------------------>|
*/
#if 0
typedef struct
{
	/* width of each image plane */
	int w[SCMN_IMGB_MAX_PLANE];
	/* height of each image plane */
	int h[SCMN_IMGB_MAX_PLANE];
	/* stride of each image plane */
	int s[SCMN_IMGB_MAX_PLANE];
	/* elevation of each image plane */
	int e[SCMN_IMGB_MAX_PLANE];
	/* user space address of each image plane */
	void *a[SCMN_IMGB_MAX_PLANE];
	/* physical address of each image plane, if needs */
	void *p[SCMN_IMGB_MAX_PLANE];
	/* color space type of image */
	int cs;
	/* left postion, if needs */
	int x;
	/* top position, if needs */
	int y;
	/* to align memory */
	int __dummy2;
	/* arbitrary data */
	int data[16];
	int dma_buf_fd[SCMN_IMGB_MAX_PLANE];
	int buf_share_method;
} SCMN_IMGB;
#endif

#if !defined (PAGE_SHIFT)
    #define PAGE_SHIFT sysconf(_SC_PAGESIZE)
#endif
#if !defined (PAGE_SIZE)
    #define PAGE_SIZE (1UL << PAGE_SHIFT)
#endif
#if !defined (PAGE_MASK)
    #define PAGE_MASK (~(PAGE_SIZE-1))
#endif
#if !defined (PAGE_ALIGN)
    #define PAGE_ALIGN(addr)    (((addr)+PAGE_SIZE-1)&PAGE_MASK)
#endif

#define ALIGN_SIZE_I420                 (1024<<2)
#define ALIGN_SIZE_NV12                 (1024<<6)
#define CAMERASRC_ALIGN(addr,size)      (((addr)+((size)-1))&(~((size)-1)))

#if !defined (CLEAR)
    #define CLEAR(x)                    memset(&(x), 0, sizeof(x))
#endif


/* Enables */
#define _ENABLE_CAMERASRC_DEBUG                 0

/* Local definitions */
#define _DEFAULT_WIDTH                          320
#define _DEFAULT_HEIGHT                         240
#define _DEFAULT_FPS                            30
#define _DEFAULT_HIGH_SPEED_FPS                 0
#define _DEFAULT_FPS_AUTO                       FALSE
#define _DEFAULT_PIX_FORMAT                     CAMERASRC_PIX_MJPEG
#define _DEFAULT_FOURCC                         GST_MAKE_FOURCC('M','J','P','G')
#define _DEFAULT_COLORSPACE                     CAMERASRC_COL_JPEG
#define _DEFAULT_CAMERA_ID                      CAMERASRC_DEV_ID_PRIMARY

/* mmap/pad-alloc related definition */
#define _DEFAULT_NUM_LIVE_BUFFER                0
#define _DEFAULT_BUFFER_COUNT                   0
#define _ALLOWED_GAP_BTWN_BUF                   5       /* Allowed gap between circulation count of each buffer and that of average buffer */
#define _DEFAULT_BUFFER_RUNNING                 FALSE
#define _DEFAULT_DEQUE_WAITINGTIME              400     /* msec */
#define _MINIMUM_REMAINING_V4L2_QBUF            1       /* minimum number of buffer that should remain in the v4l2 driver */

#define _FD_DEFAULT     (-1)
#define _FD_MIN         (-1)
#define _FD_MAX         (1<<15) /* 2^15 == 32768 */

#define _DEFAULT_CAP_QUALITY                    GST_CAMERASRC_QUALITY_HIGH
#define _DEFAULT_CAP_JPG_QUALITY                95
#define _DEFAULT_CAP_WIDTH                      640
#define _DEFAULT_CAP_HEIGHT                     480
#define _DEFAULT_CAP_COUNT                      1
#define _DEFAULT_CAP_INTERVAL                   0
#define _DEFAULT_CAP_PROVIDE_EXIF               FALSE
#define _DEFAULT_ENABLE_ZSL_MODE                FALSE
#define _DEFAULT_DO_FACE_DETECT                 FALSE
#define _DEFAULT_DO_AF                          FALSE
#define _DEFAULT_SIGNAL_AF                      FALSE
#define _DEFAULT_SIGNAL_STILL_CAPTURE           FALSE
#define _DEFAULT_PREVIEW_WIDTH                  _DEFAULT_WIDTH
#define _DEFAULT_PREVIEW_HEIGHT                 _DEFAULT_HEIGHT
#define _DEFAULT_KEEPING_BUFFER                 0
#define _DEFAULT_USE_PAD_ALLOC                  FALSE
#define _DEFAULT_NUM_ALLOC_BUF                  6
#define _DEFAULT_SCRNL_FOURCC                   GST_MAKE_FOURCC('N','V','1','2')
#define _MAX_NUM_ALLOC_BUF                      100
#define _MAX_TRIAL_WAIT_FRAME                   25
#define _PAD_ALLOC_RETRY_PERIOD                 25
#define _CONTINUOUS_SHOT_MARGIN                 50      /* msec */
#define _PREVIEW_BUFFER_WAIT_TIMEOUT            4000000 /* usec */

/*FIXME*/
#define _THUMBNAIL_WIDTH                        320
#define _THUMBNAIL_HEIGHT                       240

#define GST_TYPE_CAMERASRC_BUFFER               (gst_camerasrc_buffer_get_type())
#define GST_IS_CAMERASRC_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CAMERASRC_BUFFER))
#define GST_CAMERASRC_BUFFER(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CAMERASRC_BUFFER, GstCameraBuffer))
#define GST_TYPE_CAMERASRC_QUALITY              (gst_camerasrc_quality_get_type())


/* Enumerations */
enum {
	/*signal*/
	SIGNAL_STILL_CAPTURE,
	SIGNAL_NEGO_COMPLETE,
	/*SIGNAL_REGISTER_TROUBLE,*/
	LAST_SIGNAL
};

enum {
	ARG_0,
	/* camera */
	ARG_CAMERA_HIGH_SPEED_FPS,
	ARG_CAMERA_AUTO_FPS,
	ARG_CAMERA_ID,
	ARG_CAMERA_EXT_VIDEO_FD,

	/* capture */
	ARG_CAMERA_CAPTURE_FOURCC,
	ARG_CAMERA_CAPTURE_QUALITY,
	ARG_CAMERA_CAPTURE_WIDTH,
	ARG_CAMERA_CAPTURE_HEIGHT,
	ARG_CAMERA_CAPTURE_INTERVAL,
	ARG_CAMERA_CAPTURE_COUNT,
	ARG_CAMERA_CAPTURE_JPG_QUALITY,

	/* signal */
	ARG_SIGNAL_STILLCAPTURE,
	ARG_REQ_NEGOTIATION,

	ARG_USE_PAD_ALLOC,
	ARG_NUM_ALLOC_BUF,
	ARG_OPERATION_STATUS,
/*	ARG_FAKE_ESD, */
#ifdef SUPPORT_CAMERA_SENSOR_MODE
	ARG_SENSOR_MODE,
#endif	
	ARG_VFLIP,
	ARG_HFLIP,
	ARG_NUM,
};

enum {
	VIDEO_IN_MODE_UNKNOWN,
	VIDEO_IN_MODE_PREVIEW,
	VIDEO_IN_MODE_VIDEO,
	VIDEO_IN_MODE_CAPTURE,
};

/* Structures */
typedef struct {
    gint index;
    gint buffer_data_index;
    GstCameraSrc *camerasrc;
} GST_CAMERASRC_BUFFER_DATA;

static void gst_camerasrc_uri_handler_init (gpointer g_iface, gpointer iface_data);

/* Local variables */
static GstBufferClass *camera_buffer_parent_class = NULL;

static guint gst_camerasrc_signals[LAST_SIGNAL] = { 0 };

/* For pad_alloc architecture */
static camerasrc_usr_buf_t g_present_buf;

#if _ENABLE_CAMERASRC_DEBUG
static unsigned int g_preview_frame_cnt;
#endif

/* Element template variables */
#if defined (USE_SAMSUNG_FORMAT)
static GstStaticPadTemplate src_factory =
	GST_STATIC_PAD_TEMPLATE("src",
	                        GST_PAD_SRC,
	                        GST_PAD_ALWAYS,
	                        GST_STATIC_CAPS("video/x-raw-yuv,"
	                                        "format = (fourcc) { UYVY }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { YU12 }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { I420 }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { NV12 }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { SN12 }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { ST12 }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { YUY2 }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { YUYV }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { SUYV }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { SUY2 }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { SYVY }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { S420 }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-rgb,"
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]"));
#else
static GstStaticPadTemplate src_factory =
	GST_STATIC_PAD_TEMPLATE("src",
	                        GST_PAD_SRC,
	                        GST_PAD_ALWAYS,
	                        GST_STATIC_CAPS("video/x-jpeg,"
	                                        "format = (fourcc) { MJPG }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]; "
	                                        "video/x-raw-yuv,"
	                                        "format = (fourcc) { YUYV }, "
	                                        "width = (int) [ 1, 4096 ], "
	                                        "height = (int) [ 1, 4096 ]"));
#endif
static GstElementDetails camerasrc_details = {
	"Camera Source GStreamer Plug-in",
	"Src/Video",
	"camera src for videosrc based GStreamer Plug-in",
	""
};



/* Local static functions */
static void gst_camerasrc_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_camerasrc_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static gboolean gst_camerasrc_src_start(GstBaseSrc *src);
static gboolean gst_camerasrc_src_stop(GstBaseSrc *src);
static gboolean gst_camerasrc_start(GstCameraSrc *camerasrc);

static GstFlowReturn gst_camerasrc_src_create(GstPushSrc *src, GstBuffer **buffer);
static GstFlowReturn gst_camerasrc_read_capture(GstCameraSrc *camerasrc, GstBuffer **buffer, int command);
static GstFlowReturn gst_camerasrc_read_preview_mmap(GstCameraSrc *camerasrc, GstBuffer **buffer);
static GstFlowReturn gst_camerasrc_read_preview_pad_alloc(GstCameraSrc *camerasrc, GstBuffer **buffer);

static GstStateChangeReturn gst_camerasrc_change_state(GstElement *element, GstStateChange transition);
static GstCaps *gst_camerasrc_get_caps(GstBaseSrc *src);
static gboolean gst_camerasrc_set_caps(GstBaseSrc *src, GstCaps *caps);
static gboolean gst_camerasrc_get_caps_info(GstCameraSrc *camerasrc, GstCaps *caps, guint *size);
static gboolean gst_camerasrc_fill_ctrl_list(GstCameraSrc *camerasrc);
static gboolean gst_camerasrc_empty_ctrl_list(GstCameraSrc *camerasrc);
static void gst_camerasrc_finalize(GObject *object);

static gboolean gst_camerasrc_device_is_open(GstCameraSrc *camerasrc);
static gboolean gst_camerasrc_get_timeinfo(GstCameraSrc *camerasrc, GstBuffer *buffer);
static int get_proper_index(GstCameraSrc *camerasrc, GstBuffer *pad_alloc_buffer);

static gboolean gst_camerasrc_capture_start(GstCameraSrc *camerasrc);
static gboolean gst_camerasrc_capture_stop(GstCameraSrc *camerasrc);

static GstCameraBuffer *gst_camerasrc_buffer_new(GstCameraSrc *camerasrc);
static GType gst_camerasrc_buffer_get_type(void);
static void gst_camerasrc_buffer_class_init(gpointer g_class, gpointer class_data);
static void gst_camerasrc_buffer_finalize(GstCameraBuffer *buffer);
static void gst_camerasrc_buffer_free(gpointer data);
static void gst_camerasrc_buffer_trace(GstCameraSrc *camerasrc);
static void gst_camerasrc_error_handler(GstCameraSrc *camerasrc, int ret);

/* Util functions */
static unsigned long gst_get_current_time(void);
static gboolean _gst_camerasrc_get_frame_size(int fourcc, int width, int height, unsigned int *outsize);
static gboolean _gst_camerasrc_get_raw_pixel_info(int fourcc, int *pix_format, int *colorspace);
#if _ENABLE_CAMERASRC_DEBUG
static int __util_write_file(char *filename, void *data, int size);
#endif /* _ENABLE_CAMERASRC_DEBUG */
static gboolean gst_camerasrc_negotiate (GstBaseSrc * basesrc);

GST_IMPLEMENT_CAMERASRC_COLOR_BALANCE_METHODS(GstCameraSrc, gst_camera_src);
GST_IMPLEMENT_CAMERASRC_CONTROL_METHODS(GstCameraSrc, gst_camera_src);


/******************************************************************************
 * Implementations
 *******************************************************************************/
static void gst_camerasrc_error_handler(GstCameraSrc *camerasrc, int ret)
{
	switch (ret) {
	case CAMERASRC_SUCCESS:
		break;
	case CAMERASRC_ERR_IO_CONTROL:
		GST_ELEMENT_ERROR(camerasrc, RESOURCE, FAILED, ("IO control error"), GST_ERROR_SYSTEM);
		break;
	case CAMERASRC_ERR_DEVICE_OPEN:
		GST_ELEMENT_ERROR(camerasrc, RESOURCE, OPEN_READ_WRITE, ("camera open failed"), GST_ERROR_SYSTEM);
		break;
	case CAMERASRC_ERR_DEVICE_BUSY:
		GST_ELEMENT_ERROR(camerasrc, RESOURCE, BUSY, ("camera device busy"), GST_ERROR_SYSTEM);
		break;
	case CAMERASRC_ERR_DEVICE_NOT_FOUND:
		GST_ELEMENT_ERROR(camerasrc, RESOURCE, NOT_FOUND, ("camera device not found"), GST_ERROR_SYSTEM);
		break;
	case CAMERASRC_ERR_DEVICE_UNAVAILABLE:
		GST_ELEMENT_ERROR(camerasrc, RESOURCE, OPEN_READ, ("camera device unavailable"), GST_ERROR_SYSTEM);
		break;
	case CAMERASRC_ERR_DEVICE_WAIT_TIMEOUT:
		gst_camerasrc_buffer_trace(camerasrc);
		GST_ELEMENT_ERROR(camerasrc, RESOURCE, TOO_LAZY, (("Timeout[live_buffers=%d]"), camerasrc->num_live_buffers), GST_ERROR_SYSTEM);
		break;
	case CAMERASRC_ERR_DEVICE_NOT_SUPPORT:
		GST_ELEMENT_ERROR(camerasrc, RESOURCE, SETTINGS, ("Not supported"), GST_ERROR_SYSTEM);
		break;
	case CAMERASRC_ERR_ALLOCATION:
		GST_ELEMENT_ERROR(camerasrc, RESOURCE, SETTINGS, ("memory allocation failed"), GST_ERROR_SYSTEM);
		break;
	case CAMERASRC_ERR_SECURITY_SERVICE:
		GST_ELEMENT_ERROR(camerasrc, RESOURCE, FAILED, ("Security service failed"), GST_ERROR_SYSTEM);
		break;
	default:
		GST_ELEMENT_ERROR(camerasrc, RESOURCE, SEEK, (("General video device error[ret=%x]"), ret), GST_ERROR_SYSTEM);
		break;
	}

	return;
}

static gboolean gst_camerasrc_iface_supported(GstImplementsInterface *iface, GType iface_type)
{
	g_assert(iface_type == GST_TYPE_CAMERA_CONTROL ||
	         iface_type == GST_TYPE_COLOR_BALANCE);

	return TRUE;
}


static void gst_camerasrc_interface_init(GstImplementsInterfaceClass *klass)
{
	/*
	 * default virtual functions
	 */
	klass->supported = gst_camerasrc_iface_supported;
}


void gst_camerasrc_init_interfaces(GType type)
{
	static const GInterfaceInfo urihandler_info = {
		gst_camerasrc_uri_handler_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo cameraiface_info = {
		(GInterfaceInitFunc)gst_camerasrc_interface_init,
		NULL,
		NULL,
	};

	static const GInterfaceInfo camerasrc_control_info = {
		(GInterfaceInitFunc)gst_camera_src_control_interface_init,
		NULL,
		NULL,
	};

	static const GInterfaceInfo camerasrc_color_balance_info = {
		(GInterfaceInitFunc)gst_camera_src_color_balance_interface_init,
		NULL,
		NULL,
	};

	g_type_add_interface_static (type, GST_TYPE_URI_HANDLER, &urihandler_info);
	g_type_add_interface_static(type, GST_TYPE_IMPLEMENTS_INTERFACE, &cameraiface_info);
	g_type_add_interface_static(type, GST_TYPE_CAMERA_CONTROL, &camerasrc_control_info);
	g_type_add_interface_static(type, GST_TYPE_COLOR_BALANCE, &camerasrc_color_balance_info);
}


static GType gst_camerasrc_quality_get_type(void)
{
	static GType camerasrc_quality_type = 0;
	static const GEnumValue quality_types[] = {
		{GST_CAMERASRC_QUALITY_LOW, "Low quality", "low"},
		{GST_CAMERASRC_QUALITY_HIGH, "High quality", "high"},
		{0, NULL, NULL}
	};

	if (!camerasrc_quality_type) {
		camerasrc_quality_type = g_enum_register_static ("GstCameraSrcQuality", quality_types);
	}
	return camerasrc_quality_type;
}

/* VOID:OBJECT,OBJECT (generated by 'glib-genmarshal') */
#define g_marshal_value_peek_object(v)   (v)->data[0].v_pointer
void gst_camerasrc_VOID__OBJECT_OBJECT(GClosure *closure,
                                       GValue *return_value,
                                       guint n_param_values,
                                       const GValue *param_values,
                                       gpointer invocation_hint,
                                       gpointer marshal_data)
{
	typedef void (*GMarshalFunc_VOID__OBJECT_OBJECT)(gpointer data1,
	                                                 gpointer arg_1,
	                                                 gpointer arg_2,
	                                                 gpointer arg_3,
	                                                 gpointer data2);
	register GMarshalFunc_VOID__OBJECT_OBJECT callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2;

	g_return_if_fail (n_param_values == 4);

	if (G_CCLOSURE_SWAP_DATA(closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	} else {
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	callback = (GMarshalFunc_VOID__OBJECT_OBJECT)(marshal_data ? marshal_data : cc->callback);

	callback(data1,
	         g_marshal_value_peek_object(param_values + 1),
	         g_marshal_value_peek_object(param_values + 2),
	         g_marshal_value_peek_object(param_values + 3),
	         data2);
}


/* use following BOILERPLATE MACRO as _get_type entry */
GST_BOILERPLATE_FULL(GstCameraSrc, gst_camerasrc, GstPushSrc, GST_TYPE_PUSH_SRC, gst_camerasrc_init_interfaces);


static gboolean gst_camerasrc_create(GstCameraSrc *camerasrc)
{
	int ret = 0;

	/*create handle*/
	__ta__("        camerasrc_create",
	ret = camerasrc_create(&(camerasrc->v4l2_handle));
	);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT(camerasrc, "camerasrc_create() failed. errcode = 0x%08X", ret);
		goto _ERROR;
	}

	GST_INFO_OBJECT (camerasrc, "camerasrc_create() done");

	if (camerasrc->external_videofd != _FD_DEFAULT) {
		__ta__("            camerasrc_set_videofd",
		camerasrc_set_videofd(camerasrc->v4l2_handle,camerasrc->external_videofd);
		);
	}

	/*CAMERASRC CAM: realize*/
	__ta__("            camerasrc_realize",  
	ret = camerasrc_realize(camerasrc->v4l2_handle, camerasrc->camera_id);
	);
	if (ret != CAMERASRC_SUCCESS) {
		goto _ERROR;
	}

	GST_INFO("camerasrc_realize() done.");

	camerasrc_get_buffer_share_method(camerasrc->v4l2_handle, &camerasrc->buf_share_method);
	GST_INFO("buf_share_method %d", camerasrc->buf_share_method);

	/*CAMERASRC CAM: start*/
	__ta__("                camerasrc_start",  
	ret = camerasrc_start(camerasrc->v4l2_handle);
	);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT(camerasrc, "camerasrc_start() failed. errcode = 0x%x", ret);
		goto _ERROR;
	}

	/*CAMERASRC CAM: set camera device id*/
	/**@note if realized, iput device can't change!*/
	__ta__("            camerasrc_set_input",
	ret = camerasrc_set_input(camerasrc->v4l2_handle, camerasrc->camera_id);
	);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT(camerasrc, "camerasrc_set_input() failed. errcode = 0x%x", ret);
		goto _ERROR;
	}

	if (!gst_camerasrc_fill_ctrl_list(camerasrc)) {
		GST_WARNING_OBJECT(camerasrc, "Can't fill v4l2 control list.");
	}

	return TRUE;

 _ERROR:
	gst_camerasrc_error_handler(camerasrc, ret);

	return FALSE;
}


static gboolean gst_camerasrc_destroy(GstCameraSrc *camerasrc)
{
	GST_INFO_OBJECT (camerasrc, "ENTERED");

	if (camerasrc->v4l2_handle) {
		/*Empty control list */
		gst_camerasrc_empty_ctrl_list(camerasrc);

		/*CAMERASRC CAM: stop stream*/
		/*CAMERASRC CAM: unrealize*/
		GST_INFO_OBJECT(camerasrc, "camerasrc_unrealize() calling...");
		camerasrc_unrealize(camerasrc->v4l2_handle);

		/*CAMERASRC CAM: destroy*/
		GST_INFO_OBJECT(camerasrc, "camerasrc_destroy() calling...");
		camerasrc_destroy(camerasrc->v4l2_handle);
		camerasrc->v4l2_handle = NULL;
		GST_INFO_OBJECT(camerasrc, "AV cam destroyed.");
		camerasrc->mode = VIDEO_IN_MODE_UNKNOWN;
	}

	GST_INFO_OBJECT(camerasrc, "LEAVED");

	return TRUE;
}


static gboolean gst_camerasrc_fill_ctrl_list(GstCameraSrc *camerasrc)
{
	int n = 0;
	camerasrc_ctrl_info_t ctrl_info;

	g_return_val_if_fail(camerasrc, FALSE);
	g_return_val_if_fail(camerasrc->v4l2_handle, FALSE);

	GST_DEBUG_OBJECT(camerasrc, "ENTERED");

	for (n = CAMERASRC_CTRL_BRIGHTNESS ; n < CAMERASRC_CTRL_NUM ; n++) {
		GstCameraSrcColorBalanceChannel *camerasrc_color_channel = NULL;
		GstColorBalanceChannel *color_channel = NULL;

		GstCamerasrcControlChannel *camerasrc_control_channel = NULL;
		GstCameraControlChannel *control_channel = NULL;

		gint channel_type;

		memset(&ctrl_info, 0x0, sizeof(camerasrc_ctrl_info_t));

		if (camerasrc_query_control(camerasrc->v4l2_handle, n, &ctrl_info) != CAMERASRC_SUCCESS) {
			/* TODO: */
		}

		switch (n) {
			case CAMERASRC_CTRL_BRIGHTNESS:
			case CAMERASRC_CTRL_CONTRAST:
			case CAMERASRC_CTRL_COLOR_TONE:
			case CAMERASRC_CTRL_SATURATION:
			case CAMERASRC_CTRL_SHARPNESS:
				channel_type = INTERFACE_COLOR_BALANCE;
				break;
			case CAMERASRC_CTRL_FLIP:
				channel_type = INTERFACE_CAMERA_CONTROL;
				break;
			default:
				channel_type = INTERFACE_NONE;
				continue;
		}

		if (channel_type == INTERFACE_COLOR_BALANCE) {
			camerasrc_color_channel = g_object_new(GST_TYPE_CAMERASRC_COLOR_BALANCE_CHANNEL, NULL);
			color_channel = GST_COLOR_BALANCE_CHANNEL(camerasrc_color_channel);

			color_channel->label = g_strdup((const gchar *)camerasrc_ctrl_label[n]);
			camerasrc_color_channel->id = n;
			color_channel->min_value = ctrl_info.min;
			color_channel->max_value = ctrl_info.max;

			camerasrc->colors = g_list_append(camerasrc->colors, (gpointer)color_channel);
			GST_INFO_OBJECT(camerasrc, "Adding Color Balance Channel %s (%x)",
			                           color_channel->label, camerasrc_color_channel->id);
		} else { /* if( channel_type == INTERFACE_CAMERA_CONTROL ) */
			camerasrc_control_channel = g_object_new(GST_TYPE_CAMERASRC_CONTROL_CHANNEL, NULL);
			control_channel = GST_CAMERA_CONTROL_CHANNEL(camerasrc_control_channel);

			control_channel->label = g_strdup((const gchar *)camerasrc_ctrl_label[n]);
			camerasrc_control_channel->id = n;
			control_channel->min_value = ctrl_info.min;
			control_channel->max_value = ctrl_info.max;

			camerasrc->camera_controls = g_list_append(camerasrc->camera_controls, (gpointer)control_channel);
			GST_INFO_OBJECT(camerasrc, "Adding Camera Control Channel %s (%x)",
			                           control_channel->label, camerasrc_control_channel->id);
		}
	}

	GST_DEBUG_OBJECT(camerasrc, "LEAVED");

	return TRUE;
}


static gboolean gst_camerasrc_empty_ctrl_list(GstCameraSrc *camerasrc)
{
	g_return_val_if_fail(camerasrc, FALSE);

	GST_DEBUG_OBJECT (camerasrc, "ENTERED");

	g_list_foreach(camerasrc->colors, (GFunc)g_object_unref, NULL);
	g_list_free(camerasrc->colors);
	camerasrc->colors = NULL;

	g_list_foreach(camerasrc->camera_controls, (GFunc)g_object_unref, NULL);
	g_list_free(camerasrc->camera_controls);
	camerasrc->camera_controls = NULL;

	GST_DEBUG_OBJECT(camerasrc, "LEAVED");

	return TRUE;
}


static GstFlowReturn gst_camerasrc_prepare_preview(GstCameraSrc *camerasrc, int buf_num)
{
	int page_size = getpagesize();
	int buffer_size = 0;
	int i = 0;
	int ret = 0;
	unsigned int main_buf_sz = 0;
	unsigned int thumb_buf_sz = 0;
	GstCaps *negotiated_caps = gst_pad_get_negotiated_caps(GST_BASE_SRC_PAD(camerasrc));

	g_return_val_if_fail(buf_num <= MAX_USR_BUFFER_NUM, GST_FLOW_ERROR);

	camerasrc_query_img_buf_size(camerasrc->v4l2_handle, &main_buf_sz, &thumb_buf_sz);

	buffer_size = (main_buf_sz + page_size - 1) & ~(page_size - 1);

	g_present_buf.present_buffer = calloc(buf_num, sizeof(camerasrc_buffer_t));

	for (i = 0 ; i < buf_num ; i++) {
		GST_INFO_OBJECT (camerasrc,"pad_alloc called");
		ret = gst_pad_alloc_buffer(GST_BASE_SRC_PAD (camerasrc),
		                           0,
		                           main_buf_sz,
		                           negotiated_caps,
		                           &(camerasrc->usr_buffer[i]));

		if (!GST_IS_BUFFER(camerasrc->usr_buffer[i])) {
			GST_INFO_OBJECT (camerasrc, "[%d] NOT BUFFER!!?", i);
		}
		if (ret != GST_FLOW_OK) {
			GST_ERROR_OBJECT (camerasrc, "gst_pad_alloc_buffer failed. [%d]", ret);
			return ret;
		}

		GST_INFO_OBJECT(camerasrc, "Alloced Size %d, Alloced address %p",
		                           GST_BUFFER_SIZE(camerasrc->usr_buffer[i]),
		                           GST_BUFFER_DATA(camerasrc->usr_buffer[i]));

		g_present_buf.present_buffer[i].start = GST_BUFFER_DATA(camerasrc->usr_buffer[i]);
		g_present_buf.present_buffer[i].length = buffer_size;
	}

	g_present_buf.num_buffer = buf_num;

	gst_caps_unref(negotiated_caps);
	negotiated_caps = NULL;

	ret = camerasrc_present_usr_buffer(camerasrc->v4l2_handle, &g_present_buf, CAMERASRC_IO_METHOD_USRPTR);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT(camerasrc, "camerasrc_present_usr_buffer failed ret = %x", ret);
	} else {
		GST_INFO_OBJECT(camerasrc, "present_buffer success");
	}

	/*CAMERASRC CAM: start video preview*/
	ret = camerasrc_start_preview_stream(camerasrc->v4l2_handle);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT (camerasrc, "camerasrc_start_preview_stream() failed. errcode = 0x%08X", ret);
		gst_camerasrc_error_handler(camerasrc, ret);

		return GST_FLOW_ERROR;
	}

	GST_INFO_OBJECT(camerasrc, "camerasrc_start_preview_stream() done");

	camerasrc->buffer_running = TRUE;
	camerasrc->buffer_count = buf_num;
	camerasrc->first_invokation = TRUE;

	return GST_FLOW_OK;
}


static gboolean gst_camerasrc_start(GstCameraSrc *camerasrc)
{
	int ret = 0;

	camerasrc_format_t fmt;
	camerasrc_frac_t frac;

	GST_DEBUG_OBJECT(camerasrc, "ENTERED");

#ifdef _SPEED_UP_RAW_CAPTURE
	/* check if from no stream change capture */
	if (camerasrc->mode == VIDEO_IN_MODE_CAPTURE &&
	    camerasrc->cap_stream_diff == FALSE) {
		camerasrc->buffer_running = TRUE;
		goto _READY_DONE;
	}

	camerasrc->cap_stream_diff = FALSE;
#endif /* _SPEED_UP_RAW_CAPTURE */

	/**@note do not setting at camer starting time*/
	/*CAMERASRC CAM: set FILTER (by default value)*/
	CLEAR(fmt);
	CLEAR(frac);

	/*CAMERASRC CAM: format*/
	fmt.pix_format = camerasrc->pix_format;
	fmt.colorspace = camerasrc->colorspace;
	fmt.is_highquality_mode = 0;	/*Fixed. if preview mode, set 0*/
	fmt.rotation = camerasrc->rotate;

	/*CAMERASRC CAM: set resolution - Do not care about rotation */
	CAMERASRC_SET_SIZE_BY_DIMENSION(fmt, camerasrc->width, camerasrc->height);

	/*CAMERASRC CAM: set format*/
	GST_INFO_OBJECT(camerasrc, "pix [%dx%d] fps %d, format %d, colorspace %d, rotation %d",
	                           camerasrc->width, camerasrc->height, camerasrc->fps,
	                           fmt.pix_format, fmt.colorspace, fmt.rotation);
	ret = camerasrc_set_format(camerasrc->v4l2_handle, &fmt);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT (camerasrc, "camerasrc_set_format() failed. errcode = 0x%08X", ret);
		goto _ERROR;
	}

	/*CAMERASRC CAM: set fps*/
	if (camerasrc->fps_auto) {
		/*if fps is zero, auto fps mode*/
		frac.numerator = 0;
		frac.denominator = 1;
		GST_INFO_OBJECT (camerasrc, "FPS auto(%d)", camerasrc->fps_auto);
	} else if (camerasrc->high_speed_fps <= 0) {
		if (camerasrc->fps <= 0) {
			/*if fps is zero, auto fps mode*/
			frac.numerator   = 0;
			frac.denominator = 1;
		} else {
			frac.numerator   = 1;
			frac.denominator = camerasrc->fps;
		}
	} else {
		GST_INFO_OBJECT(camerasrc, "high speed recording(%d)", camerasrc->high_speed_fps);
		frac.numerator = 1;
		frac.denominator = camerasrc->high_speed_fps;
	}

	ret = camerasrc_set_timeperframe(camerasrc->v4l2_handle, &frac);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT(camerasrc, "camerasrc_set_timeperframe() failed. errcode = 0x%x", ret);
		goto _ERROR;
	}
	GST_INFO_OBJECT (camerasrc, "camerasrc_set_timeperframe() done");

	camerasrc->buffer_running = TRUE;

	if ((camerasrc->use_pad_alloc == FALSE)) {
#ifdef SUPPORT_CAMERA_SENSOR_MODE	
		/*CAMERASRC CAM: Set Sensor mode*/
		camerasrc_set_sensor_mode(camerasrc->v4l2_handle, camerasrc->sensor_mode);
#endif
		/*CAMERASRC CAM: Set flip*/
		camerasrc_set_vflip(camerasrc->v4l2_handle, camerasrc->vflip);
		camerasrc_set_hflip(camerasrc->v4l2_handle, camerasrc->hflip);

		/*CAMERASRC CAM: start video preview*/
		__ta__("                camerasrc_start_preview_stream",
		ret = camerasrc_start_preview_stream(camerasrc->v4l2_handle);
		);
		if (ret != CAMERASRC_SUCCESS) {
			GST_ERROR_OBJECT(camerasrc, "camerasrc_start_preview_stream() failed. errcode = 0x%x", ret);
			camerasrc->buffer_running = FALSE;
			goto _ERROR;
		}

		GST_INFO_OBJECT(camerasrc, "camerasrc_start_preview_stream() done");

		ret = camerasrc_get_num_buffer(camerasrc->v4l2_handle, &(camerasrc->buffer_count));
		if (ret != CAMERASRC_SUCCESS) {
			GST_ERROR_OBJECT(camerasrc, "camerasrc_get_num_buffer() failed. errcode = 0x%x", ret);
			goto _ERROR;
		}

		GST_INFO_OBJECT(camerasrc, "buffer number %d", camerasrc->buffer_count);
#ifdef _SPEED_UP_RAW_CAPTURE
		camerasrc->cap_stream_diff = FALSE;
#endif /* _SPEED_UP_RAW_CAPTURE */
	}

#ifdef _SPEED_UP_RAW_CAPTURE
_READY_DONE:
#endif /* _SPEED_UP_RAW_CAPTURE */
	camerasrc->mode = VIDEO_IN_MODE_PREVIEW;
	camerasrc->current_buffer_data_index = (camerasrc->current_buffer_data_index + 1)%10;

	GST_DEBUG_OBJECT(camerasrc, "LEAVED");

	return TRUE;

_ERROR:
	gst_camerasrc_error_handler(camerasrc, ret);

	/* Stop stream */
	camerasrc_stop_stream(camerasrc->v4l2_handle);

	GST_DEBUG_OBJECT(camerasrc, "LEAVED");

	return FALSE;
}


static gboolean gst_camerasrc_stop(GstCameraSrc *camerasrc)
{
	camerasrc_io_method_t io_method;

	GST_DEBUG_OBJECT (camerasrc, "ENTERED");

	if (camerasrc->v4l2_handle) {
		/* CAMERASRC CAM: stop stream */
		/* To guarantee buffers are valid before finishing */
		GMutex *lock_mutex = NULL;

		if (camerasrc->use_pad_alloc == FALSE) {
			lock_mutex = camerasrc->buffer_lock;
		} else {
			lock_mutex = camerasrc->pad_alloc_mutex;
		}

		g_mutex_lock(lock_mutex);
		while (camerasrc->num_live_buffers > _DEFAULT_KEEPING_BUFFER) {
			GTimeVal abstimeout;
			GST_INFO_OBJECT(camerasrc, "Wait until all live buffers are relased. (Tot=%d, Live=%d)",
			                           camerasrc->buffer_count, camerasrc->num_live_buffers);

			g_get_current_time(&abstimeout);
			g_time_val_add(&abstimeout, _PREVIEW_BUFFER_WAIT_TIMEOUT);

			if (!g_cond_timed_wait(camerasrc->buffer_cond, lock_mutex, &abstimeout)) {
				GST_ERROR_OBJECT(camerasrc, "Buffer wait timeout[%d usec].(Live=%d) Skip waiting...",
				                            _PREVIEW_BUFFER_WAIT_TIMEOUT, camerasrc->num_live_buffers);
				gst_camerasrc_buffer_trace(camerasrc);
				break;
			} else {
				GST_INFO_OBJECT(camerasrc, "Signal received.");
			}
		}
		g_mutex_unlock(lock_mutex);
		GST_INFO_OBJECT(camerasrc, "Waiting free buffer finished. (Live=%d)", camerasrc->num_live_buffers);

		camerasrc_stop_stream(camerasrc->v4l2_handle);

		camerasrc->buffer_running = FALSE;
		camerasrc->mode = VIDEO_IN_MODE_UNKNOWN;

		/* If I/O method is Usrptr, it uses usr_buffer that is member of camerasrc structure */
		/* It must be unreffed when it stops */

		camerasrc_get_io_method(camerasrc->v4l2_handle, &io_method);
		switch (io_method) {
		case CAMERASRC_IO_METHOD_MMAP:
			break;
		case CAMERASRC_IO_METHOD_USRPTR:
			if (g_present_buf.present_buffer) {
				free(g_present_buf.present_buffer);
				g_present_buf.present_buffer = NULL;
			}
			break;
		case CAMERASRC_IO_METHOD_READ:
		default:
			break;
		}
	}

	GST_DEBUG_OBJECT(camerasrc, "LEAVED");

	return TRUE;
}


static gboolean gst_camerasrc_capture_start(GstCameraSrc *camerasrc)
{
	/*CAMERASRC CAM*/
	int ret = 0;
	char *pfourcc = NULL;
	camerasrc_format_t fmt;
	camerasrc_frac_t frac;

	GST_INFO_OBJECT(camerasrc, "ENTERED");

	if (camerasrc->mode == VIDEO_IN_MODE_PREVIEW) {
		/* To guarantee buffers are valid before finishing. */
		__ta__( "            wait for buffer in gst_camerasrc_capture_start",
		if (camerasrc->use_pad_alloc == FALSE) {
			g_mutex_lock(camerasrc->buffer_lock);
			while (camerasrc->num_live_buffers > _DEFAULT_KEEPING_BUFFER) {
				GTimeVal abstimeout;
				GST_INFO_OBJECT(camerasrc, "Wait until all live buffers are relased. (Tot=%d, Live=%d)",
				                           camerasrc->buffer_count, camerasrc->num_live_buffers);

				g_get_current_time(&abstimeout);
				g_time_val_add(&abstimeout, _PREVIEW_BUFFER_WAIT_TIMEOUT);

				if (!g_cond_timed_wait(camerasrc->buffer_cond, camerasrc->buffer_lock, &abstimeout)) {
					GST_ERROR_OBJECT(camerasrc, "Buffer wait timeout[%d usec].(Live=%d) Skip waiting...",
					                            _PREVIEW_BUFFER_WAIT_TIMEOUT, camerasrc->num_live_buffers);
					gst_camerasrc_buffer_trace(camerasrc);
					break;
				} else {
					GST_INFO_OBJECT(camerasrc, "Signal received.");
				}
			}
			g_mutex_unlock(camerasrc->buffer_lock);
			GST_INFO_OBJECT(camerasrc, "Waiting free buffer is finished. (Live=%d)", camerasrc->num_live_buffers);
		}
		);

#ifdef _SPEED_UP_RAW_CAPTURE
		/* Skip restart stream if format/width/height are all same */
		if (camerasrc->fourcc == camerasrc->cap_fourcc &&
		    camerasrc->width == camerasrc->cap_width &&
		    camerasrc->height == camerasrc->cap_height) {
			GST_INFO_OBJECT(camerasrc, "fourcc, width and height is same. Skip restarting stream.");
			goto _CAPTURE_READY_DONE;
		}

		camerasrc->cap_stream_diff = TRUE;
#endif

		/*CAMERASRC CAM: stop stream*/
		__ta__( "            camerasrc_stop_stream in gst_camerasrc_capture_start",
		camerasrc_stop_stream(camerasrc->v4l2_handle);
		);

		GST_INFO_OBJECT (camerasrc, "camerasrc_stop_stream() done");
		camerasrc->buffer_running = FALSE;

		pfourcc = (char*)&camerasrc->cap_fourcc;
		GST_INFO_OBJECT(camerasrc, "CAPTURE: Size[%dx%d], fourcc(%c%c%c%c) quality[%d] interval[%d] count[%d]",
		                           camerasrc->cap_width, camerasrc->cap_height,
		                           pfourcc[0], pfourcc[1], pfourcc[2], pfourcc[3],
		                           camerasrc->cap_quality, camerasrc->cap_interval, camerasrc->cap_count);

		/*START STILL CAPTURE*/

		/*set current video info*/

		memset(&fmt, 0x00, sizeof (camerasrc_format_t));

		/*CAMERASRC CAM: set format*/
		CAMERASRC_SET_SIZE_BY_DIMENSION(fmt, camerasrc->cap_width, camerasrc->cap_height);

		_gst_camerasrc_get_raw_pixel_info(camerasrc->cap_fourcc, &(fmt.pix_format), &(fmt.colorspace));
		fmt.rotation = 0;

		if (camerasrc->cap_fourcc == GST_MAKE_FOURCC('J', 'P', 'E', 'G') ||
		    camerasrc->cap_fourcc == GST_MAKE_FOURCC('j', 'p', 'e', 'g') ||
		    camerasrc->cap_fourcc == GST_MAKE_FOURCC('M', 'J', 'P', 'G') ||
		    camerasrc->cap_fourcc == GST_MAKE_FOURCC('m', 'j', 'p', 'g')) {
			fmt.quality = camerasrc->cap_jpg_quality;
			fmt.is_highquality_mode = camerasrc->cap_quality;	/*must be 1*/
		} else {
			fmt.is_highquality_mode = camerasrc->cap_quality;	/*0 or 1 (default: 0)*/
		}

		/*CAMERASRC CAM: format*/
		ret = camerasrc_set_format(camerasrc->v4l2_handle, &fmt);
		if (ret != CAMERASRC_SUCCESS) {
			GST_ERROR_OBJECT(camerasrc, "camerasrc_set_format() failed. errcode = 0x%x", ret);
			goto _ERROR;
		}
		GST_INFO_OBJECT(camerasrc, "camerasrc_set_format() done");

		if (camerasrc->fps_auto || camerasrc->fps <= 0) {
			/*if fps is zero, auto fps mode*/
			frac.numerator   = 0;
			frac.denominator = 1;
			GST_INFO_OBJECT (camerasrc, "FPS auto");
		} else {
			frac.numerator   = 1;
			frac.denominator = camerasrc->fps;
			GST_INFO_OBJECT (camerasrc, "FPS (%d)", camerasrc->fps);
		}

		ret = camerasrc_set_timeperframe(camerasrc->v4l2_handle, &frac);
		if (ret != CAMERASRC_SUCCESS) {
			GST_ERROR_OBJECT(camerasrc, "camerasrc_set_timeperframe() failed. errcode = 0x%x", ret);
			goto _ERROR;
		}
		GST_INFO_OBJECT (camerasrc, "camerasrc_set_timeperframe() done");

		/*CAMERASRC CAM: start stream*/
		__ta__( "            camerasrc_start_still_stream",
		ret = camerasrc_start_still_stream(camerasrc->v4l2_handle);
		);
		if (ret != CAMERASRC_SUCCESS) {
			GST_ERROR_OBJECT(camerasrc, "camerasrc_start_still_stream() failed. errcode = 0x%x", ret);
			goto _ERROR;
		}
		GST_INFO_OBJECT(camerasrc, "camerasrc_start_still_stream() done");

		camerasrc->buffer_running = TRUE;

		/*CAMERASRC CAM: set fps*/
		/*TODO: maybe do not works!*/

#ifdef _SPEED_UP_RAW_CAPTURE
_CAPTURE_READY_DONE:
#endif

		g_mutex_lock(camerasrc->jpg_mutex);
		camerasrc->cap_next_time = 0UL;
		g_mutex_unlock(camerasrc->jpg_mutex);
		camerasrc->cap_count_current = 0;

		/* end change to capture mode*/
		camerasrc->mode = VIDEO_IN_MODE_CAPTURE;

		GST_INFO_OBJECT(camerasrc, "CAPTURE STARTED!");
	} else {
		GST_WARNING_OBJECT(camerasrc, "Wrong state[%d]!", camerasrc->mode);
	}

	GST_DEBUG_OBJECT(camerasrc, "LEAVED");

	return TRUE;

_ERROR:
	gst_camerasrc_error_handler(camerasrc, ret);

	return FALSE;
}


static gboolean gst_camerasrc_capture_stop(GstCameraSrc *camerasrc)
{
	GST_DEBUG_OBJECT(camerasrc, "ENTERED");

	if (camerasrc->mode == VIDEO_IN_MODE_CAPTURE) {
#ifdef _SPEED_UP_RAW_CAPTURE
		if (camerasrc->cap_stream_diff) {
			/*CAMERASRC CAM: stop stream*/
			camerasrc_stop_stream(camerasrc->v4l2_handle);
			camerasrc->buffer_running = FALSE;

			GST_INFO_OBJECT(camerasrc, "camerasrc_stop_stream() done");
		} else {
			GST_INFO_OBJECT(camerasrc, "no need to stop stream(capture format==preview format)");
		}
#else
		/*CAMERASRC CAM: stop stream*/
		camerasrc_stop_stream(camerasrc->v4l2_handle);
		camerasrc->buffer_running = FALSE;

		GST_INFO_OBJECT(camerasrc, "camerasrc_stop_stream() done");
#endif
		GST_INFO_OBJECT(camerasrc, "CAPTURE STOPPED!");
	}

	GST_DEBUG_OBJECT (camerasrc, "LEAVED");

	return TRUE;
}

#if 0
#include "camerasrc-common.h"

#define WRITE_UNIT  4096
static int seq_no = 0;

static void write_buffer_into_path(GstCameraSrc *camerasrc, camerasrc_buffer_t *buffer, char *path, char *name)
{
	int fd = -1;
	int write_len = 0;
	char temp_fn[255];
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	sprintf(temp_fn, "%s/%s", path, name);

	fd = open(temp_fn, O_CREAT | O_WRONLY | O_SYNC, S_IRUSR|S_IWUSR);

	if(fd == -1) {
		strerror_r(errno, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		GST_LOG_OBJECT(camerasrc, "OPEN ERR!!!! : %s", err_msg);
		camsrc_assert(0);
	} else {
		GST_LOG_OBJECT(camerasrc, "Open success, FD = %d, seq = %d", fd, seq_no);
	}

	int write_remain = buffer->length;
	int offset = 0;

	while (write_remain > 0) {
		write_len = write(fd, buffer->start + offset, write_remain<WRITE_UNIT?write_remain:WRITE_UNIT);
		if (write_len < 0) {
			strerror_r(errno, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			GST_LOG_OBJECT(camerasrc, "WRITE ERR!!!! : %s", err_msg);
			camsrc_assert(0);
		} else {
			write_remain -= write_len;
			offset+= write_len;
			GST_LOG_OBJECT(camerasrc, "%d written, %d left", write_len, write_remain);
		}
	}

	close(fd);
}
#endif

static GstFlowReturn gst_camerasrc_read_preview_mmap(GstCameraSrc *camerasrc, GstBuffer **buffer)
{
	int ret = 0;
	int v4l2_buffer_index = 0;
	guint i = 0;
	unsigned int isize = 0;
	unsigned char *pData = NULL;
	void *buf = NULL;
	camerasrc_buffer_t main_buf;
	GstCameraBuffer *vid_buf = NULL;

	/*alloc main buffer*/
	vid_buf = gst_camerasrc_buffer_new(camerasrc);
	buf = (GstCameraBuffer *)vid_buf;

	g_mutex_lock(camerasrc->buffer_lock);
	GST_LOG_OBJECT(camerasrc, "After lock(buffer live %d, total %d)",
	                          camerasrc->num_live_buffers, camerasrc->buffer_count);

	for (i = 0 ; i < _MAX_TRIAL_WAIT_FRAME ; i++) {
		/* Wait frame */
		__ta__( "                camerasrc_wait_frame_available",
		ret = camerasrc_wait_frame_available(camerasrc->v4l2_handle, _DEFAULT_DEQUE_WAITINGTIME);
		);
		if (ret != CAMERASRC_SUCCESS) {
			if (ret == CAMERASRC_ERR_DEVICE_WAIT_TIMEOUT && i < (_MAX_TRIAL_WAIT_FRAME - 1)) {
				GST_ERROR_OBJECT(camerasrc, "SELECT TIMEOUT!!! Retry..(pad_alloc %d, live %d)",
				                            camerasrc->use_pad_alloc, camerasrc->num_live_buffers);
				continue;
			}

			if (ret == CAMERASRC_ERR_DEVICE_UNAVAILABLE) {
				GST_ERROR_OBJECT(camerasrc,  "register trouble error!! [%x]", ret);
				/*g_signal_emit (G_OBJECT (camerasrc), gst_camerasrc_signals[SIGNAL_REGISTER_TROUBLE], (GQuark)NULL);*/
				g_mutex_unlock(camerasrc->buffer_lock);
				gst_camerasrc_error_handler(camerasrc, ret);

				return GST_FLOW_ERROR;
			} else if (ret == CAMERASRC_ERR_INVALID_STATE && (i < _MAX_TRIAL_WAIT_FRAME - 1)) {
				GST_WARNING_OBJECT(camerasrc, "try again...");
			} else {
				GST_ERROR_OBJECT(camerasrc, "Frame waiting error[%x]", ret);
				g_mutex_unlock(camerasrc->buffer_lock);
				gst_camerasrc_error_handler(camerasrc, ret);

				return GST_FLOW_ERROR;
			}
		} else {
			GST_LOG_OBJECT(camerasrc, "select success, do DQBUF");
			break;
		}
	}

	/* Buffer DQ */
	__ta__( "                camerasrc_dequeue_buffer",
	ret = camerasrc_dequeue_buffer(camerasrc->v4l2_handle, &v4l2_buffer_index, &main_buf);
	);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT(camerasrc, "Dequeue frame error[%x]", ret);
		g_mutex_unlock(camerasrc->buffer_lock);
		gst_camerasrc_error_handler(camerasrc, ret);

		return GST_FLOW_ERROR;
	}

	camerasrc->num_live_buffers++;
	GST_LOG_OBJECT (camerasrc, "after : DQBUF (index %d, live bufs %d)",
	                           v4l2_buffer_index, camerasrc->num_live_buffers);

	g_mutex_unlock (camerasrc->buffer_lock);

	pData = main_buf.start;
	isize = main_buf.length;

	GST_BUFFER_DATA(vid_buf) = pData;
	GST_BUFFER_SIZE(vid_buf) = isize;
	vid_buf->v4l2_buffer_index = v4l2_buffer_index;

#if defined(USE_CAMERASRC_FRAME_DUMP)
	GST_ERROR_OBJECT(camerasrc, "******************in buf len = %d", main_buf.length);
	write_buffer_into_path(camerasrc, &main_buf, CAMERASRC_FRAME_DUMP_PATH, "main.yuv");
#endif	

	if (camerasrc->firsttime) {
		/** Because of basesrc negotiation , "framerate" field is needed. */
		int fps_nu = 0;
		int fps_de = 0;
		gchar *caps_string = NULL;
		GstCaps *caps = NULL;

		if (camerasrc->fps <= 0) {
			/*if fps is zero, auto fps mode*/
			fps_nu = 0;
			fps_de = 1;
		} else {
			fps_nu = 1;
			fps_de = camerasrc->fps;
		}

		GST_INFO_OBJECT(camerasrc, "FPS auto[%d], FPS[%d], High speed FPS[%d]",
		                           camerasrc->fps_auto, camerasrc->fps, camerasrc->high_speed_fps);

		if (camerasrc->pix_format == CAMERASRC_PIX_MJPEG) {
			caps = gst_caps_new_simple("video/x-jpeg",
			                           "format", GST_TYPE_FOURCC, camerasrc->fourcc,
			                           "width", G_TYPE_INT, camerasrc->width,
			                           "height", G_TYPE_INT, camerasrc->height,
			                           "framerate", GST_TYPE_FRACTION, fps_de, fps_nu,
			                           NULL);
		} else {
			caps = gst_caps_new_simple("video/x-raw-yuv",
			                           "format", GST_TYPE_FOURCC, camerasrc->fourcc,
			                           "width", G_TYPE_INT, camerasrc->width,
			                           "height", G_TYPE_INT, camerasrc->height,
			                           "framerate", GST_TYPE_FRACTION, fps_de, fps_nu,
			                           NULL);
              }
		if (caps == NULL) {
			GST_ERROR_OBJECT(camerasrc, "failed to alloc caps");
			gst_buffer_unref((GstBuffer *)vid_buf);
			vid_buf = NULL;
			buf = NULL;
			return GST_FLOW_ERROR;
		}

		if (camerasrc->use_rotate_caps) {
			gst_caps_set_simple(caps, "rotate", G_TYPE_INT, camerasrc->rotate, NULL);
		}

		GST_BUFFER_CAPS(buf) = caps;

		caps_string = gst_caps_to_string(caps);
		GST_INFO_OBJECT(camerasrc, "PREVIEW MODE first time [%dx%d], rotate[%d], caps[%s]",
		                           camerasrc->width, camerasrc->height, camerasrc->rotate, caps_string);
		if (caps_string) {
			g_free(caps_string);
			caps_string = NULL;
		}

		camerasrc->firsttime = FALSE;
	}

	gst_camerasrc_get_timeinfo(camerasrc, (GstBuffer*)vid_buf);

	*buffer = (GstBuffer*)vid_buf;

	/*GST_DEBUG_OBJECT(camerasrc, "refcount: %d", GST_OBJECT_REFCOUNT(*buffer));*/

#if _ENABLE_CAMERASRC_DEBUG
	g_preview_frame_cnt++;
#endif

	return GST_FLOW_OK;
}


static GstFlowReturn gst_camerasrc_read_preview_pad_alloc(GstCameraSrc *camerasrc, GstBuffer **buffer)
{
	int ret = 0;
	int v4l2_buffer_index = 0;
	void *buf = NULL;
	camerasrc_buffer_t main_buf;
	GstBuffer *gst_buf = NULL;
	GST_CAMERASRC_BUFFER_DATA *buffer_data = NULL;
	int is_jpeg = FALSE;

	/*alloc main buffer*/
	gst_buf = *buffer;
	buf = (GstBuffer *)gst_buf;

	g_mutex_lock(camerasrc->buffer_lock);
	GST_LOG_OBJECT(camerasrc, "After lock(lvn %d, buf cnt %d)", camerasrc->num_live_buffers, camerasrc->buffer_count);

	g_mutex_lock (camerasrc->pad_alloc_mutex);
	while (!g_queue_is_empty(camerasrc->pad_alloc_list) && !camerasrc->first_invokation) {
		int pad_alloc_index = -1;
		int proper_index = -1;
		camerasrc_buffer_t camerasrc_buffer;
		GstBuffer *pad_alloc_buffer;
		GstCaps *negotiated_caps = NULL;

		g_mutex_unlock (camerasrc->pad_alloc_mutex);

		camerasrc_buffer.start = NULL;
		camerasrc_buffer.length = 0;

		g_mutex_lock (camerasrc->pad_alloc_mutex);
		pad_alloc_index = (int)g_queue_pop_head(camerasrc->pad_alloc_list);
		g_mutex_unlock (camerasrc->pad_alloc_mutex);
		sched_yield();
		GST_LOG_OBJECT (camerasrc, "Index queue have item. pad_alloc_index = %d", pad_alloc_index);

		negotiated_caps = gst_pad_get_negotiated_caps (GST_BASE_SRC_PAD (camerasrc));

		/* pad allocation from sink(or any where) */
		ret = gst_pad_alloc_buffer(GST_BASE_SRC_PAD (camerasrc),
		                           0,
		                           camerasrc->main_buf_sz,
		                           negotiated_caps,
		                           &pad_alloc_buffer);
		if (ret != GST_FLOW_OK) {
			GST_ERROR_OBJECT(camerasrc, "Pad alloc fail, ret = [%d]", ret);
			g_cond_signal(camerasrc->buffer_cond);
			g_mutex_unlock(camerasrc->buffer_lock);
			gst_caps_unref(negotiated_caps);

			return ret;
		}

		proper_index = get_proper_index(camerasrc, pad_alloc_buffer);
		if (proper_index == -1) {
			GST_INFO_OBJECT(camerasrc, "Proper index doesn't exist");
			g_mutex_unlock(camerasrc->buffer_lock);

			return GST_FLOW_ERROR;
		}

		if (proper_index != pad_alloc_index) {
			GST_LOG_OBJECT(camerasrc, "Proper index different from pad_alloc_index, proper_index = %d, pad_alloc_index = %d",
			                          proper_index, pad_alloc_index);
		}

		GST_LOG_OBJECT(camerasrc, "gst_pad_alloc_buffer called. index = %d, GstBuffer address = %p",
		                          proper_index, &(camerasrc->usr_buffer[pad_alloc_index]));

		camerasrc->usr_buffer[proper_index] = pad_alloc_buffer;

		camerasrc_buffer.start = GST_BUFFER_DATA(GST_BUFFER(camerasrc->usr_buffer[proper_index]));
		camerasrc_buffer.length = GST_BUFFER_SIZE(GST_BUFFER(camerasrc->usr_buffer[proper_index]));

		camerasrc_buffer.length = PAGE_ALIGN(camerasrc_buffer.length);

		if (!camerasrc_buffer.start) {
			GST_ERROR_OBJECT(camerasrc, "Data for queueing is not available one, data = %p", camerasrc_buffer.start);
			g_cond_signal(camerasrc->buffer_cond);
			g_mutex_unlock(camerasrc->buffer_lock);

			return GST_FLOW_ERROR;
		}

		if (camerasrc_buffer.length <= 0) {
			GST_ERROR_OBJECT(camerasrc, "Length for queueing is not available one, data = %d", camerasrc_buffer.length);
			g_cond_signal(camerasrc->buffer_cond);
			g_mutex_unlock(camerasrc->buffer_lock);

			return GST_FLOW_ERROR;
		}

		ret = camerasrc_queue_buffer(camerasrc->v4l2_handle, proper_index, &camerasrc_buffer);
		if (ret != CAMERASRC_SUCCESS) {
			GST_ERROR_OBJECT(camerasrc, "Queue frame error, [%x]", ret);
			g_cond_signal(camerasrc->buffer_cond);
			g_mutex_unlock(camerasrc->buffer_lock);
			gst_caps_unref(negotiated_caps);

			return GST_FLOW_ERROR;
		}

		gst_caps_unref(negotiated_caps);

		g_cond_signal(camerasrc->buffer_cond);
		GST_LOG_OBJECT(camerasrc, "QBUF : [idx=%d, lvn=%d]", proper_index, camerasrc->num_live_buffers);
		g_mutex_lock(camerasrc->pad_alloc_mutex);
	}

	g_mutex_unlock(camerasrc->pad_alloc_mutex);

	ret = camerasrc_wait_frame_available(camerasrc->v4l2_handle, _PAD_ALLOC_RETRY_PERIOD);
	if (ret == CAMERASRC_ERR_DEVICE_WAIT_TIMEOUT) {
		GST_LOG_OBJECT(camerasrc, "timeout");
//		g_mutex_unlock(camerasrc->buffer_lock);

		gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_DEVICE_WAIT_TIMEOUT);

		return GST_FLOW_WRONG_STATE;
		
	} else if (ret != CAMERASRC_SUCCESS) {
		g_mutex_unlock(camerasrc->buffer_lock);
		GST_ERROR_OBJECT(camerasrc, "camerasrc_wait_frame_available error [%x]", ret);

		return GST_FLOW_ERROR;
	}

	/* Buffer DQ */
	__ta__( "            camerasrc_dequeue_buffer",
	ret = camerasrc_dequeue_buffer(camerasrc->v4l2_handle, &v4l2_buffer_index, &main_buf);
	);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT(camerasrc, "Dequeue frame error, [%x]", ret);
		g_mutex_unlock(camerasrc->buffer_lock);
		gst_camerasrc_error_handler(camerasrc, ret);

		return GST_FLOW_ERROR;
	}

	camerasrc->num_live_buffers++;
	GST_LOG_OBJECT(camerasrc, "after : DQBUF (index %d, live number %d)",
	                          v4l2_buffer_index, camerasrc->num_live_buffers);
	g_mutex_unlock(camerasrc->buffer_lock);

	/* For camera state stopped */
	gst_buf = camerasrc->usr_buffer[v4l2_buffer_index];
	if (gst_buf == NULL) {
		GST_ERROR_OBJECT(camerasrc, "camerasrc->usr_buffer[v4l2_buffer_index] NULL");
		return GST_FLOW_WRONG_STATE;
	}

	GST_LOG_OBJECT(camerasrc, "Start to set up buffer free structure");

	buffer_data = g_malloc(sizeof(GST_CAMERASRC_BUFFER_DATA));
	if (buffer_data == NULL) {
		GST_ERROR_OBJECT(camerasrc, "buffer_data NULL");
		return GST_FLOW_WRONG_STATE;
	}

	buffer_data->camerasrc = camerasrc;
	buffer_data->index = v4l2_buffer_index;
	buffer_data->buffer_data_index = camerasrc->current_buffer_data_index;

	GST_BUFFER_DATA(gst_buf) = main_buf.start;
	GST_BUFFER_SIZE(gst_buf) = main_buf.length;
	GST_BUFFER_MALLOCDATA(gst_buf) = (guint8 *)buffer_data;
	GST_BUFFER_FREE_FUNC(gst_buf) = gst_camerasrc_buffer_free;

	GST_LOG_OBJECT(camerasrc, "End to set up buffer free structure");


	if (camerasrc->firsttime) {
		/* Because of basesrc negotiation , "framerate" field is needed. */
		int fps_nu = 0;
		int fps_de = 0;
		gchar *caps_string = NULL;
		GstCaps *caps = NULL;

		if (camerasrc->fps <= 0) {
			/*if fps is zero, auto fps mode*/
			fps_nu = 0;
			fps_de = 1;
		} else {
			fps_nu = 1;
			fps_de = camerasrc->fps;
		}

		GST_INFO_OBJECT(camerasrc, "FPS auto[%d], FPS[%d], High speed FPS[%d]",
		                           camerasrc->fps_auto, camerasrc->fps, camerasrc->high_speed_fps);

		if (camerasrc->cap_fourcc == GST_MAKE_FOURCC('J','P','E','G') ||
		    camerasrc->cap_fourcc == GST_MAKE_FOURCC('j','p','e','g') || 
		    camerasrc->cap_fourcc == GST_MAKE_FOURCC('M','J','P','G') ||
		    camerasrc->cap_fourcc == GST_MAKE_FOURCC('m','j','p','g')) {
			is_jpeg = TRUE;
		} else {
			is_jpeg = FALSE;
		}

		if (is_jpeg) {
			caps = gst_caps_new_simple("video/x-jpeg",
			                           "format", GST_TYPE_FOURCC, camerasrc->fourcc,
			                           "width", G_TYPE_INT, camerasrc->width,
			                           "height", G_TYPE_INT, camerasrc->height,
			                           "framerate", GST_TYPE_FRACTION, fps_de, fps_nu,
			                           NULL);
		} else {
			caps = gst_caps_new_simple("video/x-raw-yuv",
			                           "format", GST_TYPE_FOURCC, camerasrc->fourcc,
			                           "width", G_TYPE_INT, camerasrc->width,
			                           "height", G_TYPE_INT, camerasrc->height,
			                           "framerate", GST_TYPE_FRACTION, fps_de, fps_nu,
			                           NULL);
              }
		if (caps == NULL) {
			GST_ERROR_OBJECT(camerasrc, "failed to alloc caps");
			return GST_FLOW_ERROR;
		}

		if (camerasrc->use_rotate_caps) {
			gst_caps_set_simple(caps, "rotate", G_TYPE_INT, camerasrc->rotate, NULL);
		}

		GST_BUFFER_CAPS(gst_buf) = caps;

		caps_string = gst_caps_to_string(caps);
		GST_INFO_OBJECT (camerasrc, "PREVIEW MODE first time [%dx%d], rotate[%d], caps[%s]",
		                            camerasrc->width, camerasrc->height, camerasrc->rotate, caps_string);
		if (caps_string) {
			g_free(caps_string);
			caps_string = NULL;
		}

		camerasrc->firsttime = FALSE;
	}

	gst_camerasrc_get_timeinfo(camerasrc, gst_buf);

	*buffer = gst_buf;

	g_mutex_lock (camerasrc->pad_alloc_mutex);
	if (camerasrc->first_invokation) {
		GST_INFO_OBJECT(camerasrc, "[DEBUG] Check something in pad_alloc_list");
		g_mutex_unlock (camerasrc->pad_alloc_mutex);
		g_mutex_lock (camerasrc->pad_alloc_mutex);

		while (!g_queue_is_empty(camerasrc->pad_alloc_list)) {
			g_mutex_unlock(camerasrc->pad_alloc_mutex);
			g_mutex_lock(camerasrc->pad_alloc_mutex);

			/* Remove all item */

			g_queue_pop_head(camerasrc->pad_alloc_list);
			GST_INFO_OBJECT(camerasrc, "[DEBUG] Something is in pad_alloc_list before first frame. remove it!");

			g_mutex_unlock(camerasrc->pad_alloc_mutex);
			g_mutex_lock(camerasrc->pad_alloc_mutex);
		}

		g_mutex_unlock(camerasrc->pad_alloc_mutex);
		g_mutex_lock(camerasrc->pad_alloc_mutex);
		camerasrc->first_invokation = FALSE;
	}
	g_mutex_unlock (camerasrc->pad_alloc_mutex);

#if _ENABLE_CAMERASRC_DEBUG
	g_preview_frame_cnt++;
#endif

	return GST_FLOW_OK;
}

static GstFlowReturn gst_camerasrc_read_capture(GstCameraSrc *camerasrc, GstBuffer **buffer, int command)
{
	int ret;
	int buffer_index = 0;
	unsigned long cur_time;
	gboolean is_jpeg = FALSE;

	static gboolean get_stop_command = FALSE;
	static gboolean get_stop_multi_command = FALSE;

	GstCameraBuffer *buf = NULL;            /*output buffer for preview*/
	GstBuffer *buf_cap_signal1 = NULL;      /*output main buffer for capture signal*/

	camerasrc_buffer_t main_buf = {0, NULL, 0};       /*original size buffer*/

	GST_DEBUG_OBJECT(camerasrc, "ENTERED. Command[%d]", command);

	GST_INFO_OBJECT(camerasrc, "src size[%dx%d], capture size[%dx%d]",
	                           camerasrc->width, camerasrc->height,
	                           camerasrc->cap_width, camerasrc->cap_height );

	if (command == GST_CAMERA_CONTROL_CAPTURE_COMMAND_STOP) {
		get_stop_command = TRUE;
	} else if (command == GST_CAMERA_CONTROL_CAPTURE_COMMAND_STOP_MULTISHOT) {
		get_stop_multi_command = TRUE;
	}

	GST_INFO_OBJECT(camerasrc, "cnt current:%d, reverse:%d, stop cmd:%d, multi stop cmd:%d",
	                           camerasrc->cap_count_reverse, camerasrc->cap_count_current,
	                           get_stop_command, get_stop_multi_command);

	while (TRUE) {
		if (camerasrc->cap_count_reverse == 0 ||
		    ((get_stop_command || get_stop_multi_command) &&
		     camerasrc->cap_count_current != 0 )) {
			g_mutex_lock(camerasrc->mutex);

			GST_INFO_OBJECT(camerasrc, "Capture finished.");

			__ta__( "        capture: gst_camerasrc_capture_stop",
			gst_camerasrc_capture_stop(camerasrc);
			);
			if (get_stop_command == FALSE) {
				if (!g_queue_is_empty(camerasrc->command_list)) {
					command = (int)g_queue_pop_head(camerasrc->command_list);
					GST_INFO_OBJECT(camerasrc, "Pop command [%d]", command);
					if (command == GST_CAMERA_CONTROL_CAPTURE_COMMAND_STOP) {
						get_stop_command = TRUE;
					}
				}

				if (get_stop_command == FALSE) {
					GST_INFO_OBJECT(camerasrc, "Start : Wait for Capture stop signal");
					__ta__( "            capture: wait for cond after image capture",
					g_cond_wait(camerasrc->cond, camerasrc->mutex);
					);
					GST_INFO_OBJECT(camerasrc, "End   : Wait for Capture stop signal");
				}
			}

			__ta__("        capture: gst_camerasrc_start",
			gst_camerasrc_start(camerasrc);
			);

			__ta__("        capture: one gst_camerasrc_read_preview_mmap",
			ret = gst_camerasrc_read_preview_mmap(camerasrc, buffer);
			);

			get_stop_command = FALSE;
			get_stop_multi_command = FALSE;

			g_mutex_unlock(camerasrc->mutex);

			MMTA_ACUM_ITEM_END( "    Shot to Shot in gstcamerasrc", FALSE);

			return ret;
		}

		if (camerasrc->cap_fourcc == GST_MAKE_FOURCC('J','P','E','G') ||
		    camerasrc->cap_fourcc == GST_MAKE_FOURCC('j','p','e','g') || 
		    camerasrc->cap_fourcc == GST_MAKE_FOURCC('M','J','P','G') ||
		    camerasrc->cap_fourcc == GST_MAKE_FOURCC('m','j','p','g')) {
			is_jpeg = TRUE;
		} else {
			is_jpeg = FALSE;
		}

		/**
		 *@important JPEG still: always same image generated by camerasrc_read_frame.
		 *				if you want to multi shot, set YUV format !
		 */
		__ta__("            camerasrc_read_frame:select,DQ,Copy,Q",
		ret = camerasrc_read_frame(camerasrc->v4l2_handle, &main_buf, &buffer_index);
		);
		if (ret != CAMERASRC_SUCCESS) {
			if (ret == CAMERASRC_ERR_DEVICE_UNAVAILABLE) {
				GST_ERROR_OBJECT (camerasrc, "Video src device return register trouble error!! [%x]", ret);
				/*g_signal_emit (G_OBJECT (camerasrc), gst_camerasrc_signals[SIGNAL_REGISTER_TROUBLE], (GQuark)NULL);*/
				gst_camerasrc_error_handler(camerasrc, ret);
				return GST_FLOW_ERROR;
			} else {
				GST_ERROR_OBJECT (camerasrc, "camerasrc_read_frame() failed. [ret = 0x%08X]", ret);
				GST_ERROR_OBJECT (camerasrc, "return GST_FLOW_ERROR");
				/* should stop capture; */
				/*retrun EOS*/
				*buffer = NULL;
				gst_camerasrc_error_handler(camerasrc, ret);

				return GST_FLOW_ERROR;
			}
		}

CHECK_CAPTURE_INTERVAL:
		/* get shot time */
		cur_time = gst_get_current_time();

		if (camerasrc->cap_next_time == 0UL) {
			camerasrc->cap_next_time = cur_time;
		}

		if (camerasrc->cap_count_reverse > 0 && camerasrc->cap_next_time <= cur_time) {
			GST_INFO_OBJECT(camerasrc, "CHECK: reverse capture count: %d, next time:%lu current time:%lu",
			                           camerasrc->cap_count_reverse, camerasrc->cap_next_time, cur_time);

			camerasrc->cap_next_time = cur_time + camerasrc->cap_interval;
			camerasrc->cap_count_reverse--;
			camerasrc->cap_count_current++;

			/* alloc buffer for capture callback */
			buf_cap_signal1 = gst_buffer_new ();

			/* make buffers for capture callback and display(raw format) */
			if (is_jpeg) {
				GST_INFO_OBJECT (camerasrc, "JPEG CAPTURE MODE");

				GST_BUFFER_DATA(buf_cap_signal1) = main_buf.start;
				GST_BUFFER_SIZE(buf_cap_signal1) = main_buf.length;
				GST_BUFFER_CAPS(buf_cap_signal1) = gst_caps_new_simple("video/x-jpeg",
				                                   "width", G_TYPE_INT, camerasrc->cap_width,
				                                   "height", G_TYPE_INT, camerasrc->cap_height,
				                                   NULL);

				*buffer = NULL;
			} else {

				GST_INFO_OBJECT (camerasrc, "RAW CAPTURE MODE");

				/*alloc main buffer*/
				buf = gst_camerasrc_buffer_new(camerasrc);;
				if (buf == NULL) {
					GST_ERROR_OBJECT(camerasrc, "Buffer alloc failed.");
					*buffer = NULL;
					gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_ALLOCATION);
					return GST_FLOW_ERROR;
				}

				buf->v4l2_buffer_index = buffer_index;

				GST_BUFFER_DATA(buf) = main_buf.start;
				GST_BUFFER_SIZE(buf) = main_buf.length;

				*buffer = (GstBuffer *)buf;
				GST_INFO_OBJECT(camerasrc, "BUF for PREVIEW: addr %p size %d",
				                           GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf));

				GST_BUFFER_DATA(buf_cap_signal1) = GST_BUFFER_DATA(buf);
				GST_BUFFER_SIZE(buf_cap_signal1) = GST_BUFFER_SIZE(buf);
				GST_BUFFER_CAPS(buf_cap_signal1) = gst_caps_new_simple("video/x-raw-yuv",
				                                   "format", GST_TYPE_FOURCC, camerasrc->cap_fourcc,
				                                   "width", G_TYPE_INT, camerasrc->cap_width,
				                                   "height", G_TYPE_INT, camerasrc->cap_height,
				                                   NULL);
			}

			/*call signal*/
			GST_INFO_OBJECT (camerasrc, "CALL: capture callback");

			g_signal_emit( G_OBJECT (camerasrc),
			               gst_camerasrc_signals[SIGNAL_STILL_CAPTURE],
			               0,
			               buf_cap_signal1,
			               NULL,
			               NULL);

			GST_INFO_OBJECT (camerasrc, "RETURN: capture callback");

			if (is_jpeg ||
			    (!is_jpeg && (camerasrc->fourcc != camerasrc->cap_fourcc ||
			                  camerasrc->width != camerasrc->cap_width ||
			                  camerasrc->height != camerasrc->cap_height))) {
				/* Queue buffer */
				camerasrc_queue_buffer(camerasrc->v4l2_handle, buffer_index, &main_buf);

				/* release buffer */
				if (*buffer) {
					gst_buffer_unref(*buffer);
					*buffer = NULL;
				}
			} else {
				camerasrc->num_live_buffers++;
				/*escape loop for passing buffer to videosink*/
				break;
			}
		} else {
			if (camerasrc->cap_next_time < cur_time + _CONTINUOUS_SHOT_MARGIN) {
				GST_DEBUG_OBJECT(camerasrc, "check again time");
				usleep((camerasrc->cap_next_time - cur_time) * 1000);
				goto CHECK_CAPTURE_INTERVAL;
			}

			/* RAW capture buffer should be reach here */
			camerasrc_queue_buffer(camerasrc->v4l2_handle, buffer_index, &main_buf);

			/* Skip passing this buffer */
			*buffer = NULL;

			GST_DEBUG_OBJECT(camerasrc, "Skip pssing this buffer");
			break;
		}
	}

	GST_DEBUG_OBJECT (camerasrc, "LEAVED");

	return GST_FLOW_OK;
}


static GstFlowReturn gst_camerasrc_read(GstCameraSrc *camerasrc, GstBuffer **buffer)
{
	int err = 0;
	int command = GST_CAMERA_CONTROL_CAPTURE_COMMAND_NONE;
	GstFlowReturn ret = GST_FLOW_OK;
	camerasrc_state_t state = CAMERASRC_STATE_NONE;

	g_mutex_lock(camerasrc->mutex);

	if (!g_queue_is_empty(camerasrc->command_list)) {
		command = (int)g_queue_pop_head(camerasrc->command_list);
		GST_INFO_OBJECT(camerasrc, "popped cmd : %d", command);
	}

	/* Normal Capture Routine */
	if (command == GST_CAMERA_CONTROL_CAPTURE_COMMAND_START) {
		MMTA_ACUM_ITEM_BEGIN("    Shot to Shot in gstcamerasrc", FALSE);
		__ta__("        gst_camerasrc_capture_start",
		gst_camerasrc_capture_start(camerasrc);
		);
	}

	g_mutex_unlock(camerasrc->mutex);

	switch (camerasrc->mode) {
	case VIDEO_IN_MODE_PREVIEW:
	case VIDEO_IN_MODE_VIDEO:
		if (camerasrc->use_pad_alloc == TRUE) {
			err = camerasrc_get_state(camerasrc->v4l2_handle, &state);

			if (state == CAMERASRC_STATE_READY) {
				GST_INFO_OBJECT (camerasrc,"Prepare buffer");
				ret = gst_camerasrc_prepare_preview(camerasrc, camerasrc->num_alloc_buf);
			}
		}

		if (camerasrc->use_pad_alloc == TRUE) {
			__ta__("            gst_camerasrc_read_preview_pad_alloc",
			ret = gst_camerasrc_read_preview_pad_alloc(camerasrc, buffer);
			);
		} else {
			__ta__("            gst_camerasrc_read_preview_mmap",
			ret = gst_camerasrc_read_preview_mmap(camerasrc, buffer);
			)
		}
		break;
	case VIDEO_IN_MODE_CAPTURE:
		__ta__( "        gst_camerasrc_read_capture",
		ret = gst_camerasrc_read_capture(camerasrc, buffer, command);
		);
		break;
	case VIDEO_IN_MODE_UNKNOWN:
	default:
		ret = GST_FLOW_ERROR;
		GST_ERROR_OBJECT (camerasrc, "can't reach statement.[camerasrc->mode=%d]", camerasrc->mode);
		break;
	}

	if (!buffer || !(*buffer) || !GST_IS_BUFFER(*buffer)) {
		/* To avoid seg fault, make dummy buffer. */
		GST_WARNING_OBJECT (camerasrc, "Make a dummy buffer");
		*buffer = gst_buffer_new();
		GST_BUFFER_DATA(*buffer) = NULL;
		GST_BUFFER_SIZE(*buffer) = 0;
	}

	return ret;
}


/* Buffer related functions */
static void gst_camerasrc_buffer_pad_alloc_qbuf(GstCameraBuffer *buffer)
{
	int ret = 0;
	gint index = 0;
	GstCaps *negotiated_caps = NULL;
	GstBuffer *queue_buf = NULL;
	GstCameraSrc *camerasrc = NULL;
	camerasrc_buffer_t camerasrc_buffer;

	if (buffer) {
		camerasrc = buffer->camerasrc;
	} else {
		GST_ERROR("buffer is NULL");
		return;
	}

	if (camerasrc) {
		negotiated_caps = gst_pad_get_negotiated_caps(GST_BASE_SRC_PAD(camerasrc));
	} else {
		GST_ERROR("camerasrc is NULL");
		return;
	}

	index = buffer->v4l2_buffer_index;

	GST_LOG_OBJECT(camerasrc, "pad alloc qbuf");

        ret = gst_pad_alloc_buffer(GST_BASE_SRC_PAD (camerasrc),
                                   0,
                                   camerasrc->main_buf_sz,
                                   negotiated_caps,
                                   &queue_buf);
	if (!GST_IS_BUFFER(queue_buf)) {
		GST_INFO_OBJECT (camerasrc, "[DEBUG] NOT BUFFER!!?");
	}

	if (ret != GST_FLOW_OK) {
		GST_ERROR_OBJECT(camerasrc, "gst_pad_alloc_buffer failed. [%d]", ret);
		return;
	} else {
		GST_LOG_OBJECT(camerasrc, "Alloced Size = %dAlloced address : %p",
		                          GST_BUFFER_SIZE(GST_BUFFER(queue_buf)), GST_BUFFER_DATA(GST_BUFFER(queue_buf)));
	}

	gst_caps_unref(negotiated_caps);
	negotiated_caps = NULL;

	camerasrc_buffer.start = GST_BUFFER_DATA(GST_BUFFER(queue_buf));
	camerasrc_buffer.length = GST_BUFFER_SIZE(GST_BUFFER(queue_buf));
	ret = camerasrc_queue_buffer(camerasrc->v4l2_handle, index, &camerasrc_buffer);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT(camerasrc, "QBUF error, [%x]", ret);
		return;
	} else {
		GST_LOG_OBJECT (camerasrc, "QBUF : [idx=%d]", index);
	}

	return;
}


static void gst_camerasrc_buffer_mmap_qbuf(GstCameraBuffer *buffer)
{
	int ret = 0;
	gint index = 0;
	GstCameraSrc *camerasrc = NULL;
	camerasrc_buffer_t camerasrc_buffer;

	camerasrc = buffer->camerasrc;
	index = buffer->v4l2_buffer_index;

	ret = camerasrc_queue_buffer(camerasrc->v4l2_handle, index, &camerasrc_buffer);
	if (ret != CAMERASRC_SUCCESS) {
		GST_ERROR_OBJECT(camerasrc, "QBUF error, [%x]", ret);
	} else {
		GST_LOG_OBJECT(camerasrc, "QBUF : [idx=%d]", index);
	}

	return;
}

static void gst_camerasrc_buffer_finalize(GstCameraBuffer *buffer)
{
	gint index = 0;
	GstCameraSrc *camerasrc = NULL;

	camerasrc = buffer->camerasrc;
	index = buffer->v4l2_buffer_index;

	GST_LOG_OBJECT(camerasrc, "finalizing buffer %p, %d [%"
	               GST_TIME_FORMAT " dur %" GST_TIME_FORMAT "]", buffer, index,
	               GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)),
	               GST_TIME_ARGS(GST_BUFFER_DURATION(buffer)));

	if (camerasrc->buffer_running) {
		/* Buffer Q again */
		if (camerasrc->use_pad_alloc == TRUE){
			gst_camerasrc_buffer_pad_alloc_qbuf(buffer);
		} else {
			gst_camerasrc_buffer_mmap_qbuf(buffer);
		}
	} else {
		GST_INFO_OBJECT(camerasrc, "It is not running. skip QBUF");
	}

	camerasrc->num_live_buffers--;
	g_cond_signal(camerasrc->buffer_cond);
	GST_LOG_OBJECT(camerasrc, "QBUF : [idx=%d, lvn=%d]", index, camerasrc->num_live_buffers);

	if (buffer->is_alloc_data) {
		GST_DEBUG("free allocated data %p", GST_BUFFER_DATA(buffer));
		if (GST_BUFFER_DATA(buffer)) {
			free(GST_BUFFER_DATA(buffer));
			GST_BUFFER_DATA(buffer) = NULL;
		}

		buffer->is_alloc_data = FALSE;
	}

	if (GST_BUFFER_MALLOCDATA(buffer)) {
		free(GST_BUFFER_MALLOCDATA(buffer));
		GST_BUFFER_MALLOCDATA(buffer) = NULL;
	}

	gst_object_unref(camerasrc);

	if (GST_MINI_OBJECT_CLASS (camera_buffer_parent_class)->finalize) {
		GST_MINI_OBJECT_CLASS (camera_buffer_parent_class)->finalize (GST_MINI_OBJECT(buffer));
	}

	return;
}


static void gst_camerasrc_buffer_class_init(gpointer g_class, gpointer class_data)
{
	GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS(g_class);

	camera_buffer_parent_class = g_type_class_peek_parent(g_class);
	mini_object_class->finalize = (GstMiniObjectFinalizeFunction)gst_camerasrc_buffer_finalize;
}


static GType gst_camerasrc_buffer_get_type(void)
{
	static GType _gst_camerasrc_buffer_type;

	if (G_UNLIKELY(_gst_camerasrc_buffer_type == 0)) {
		static const GTypeInfo camera_buffer_info = {
			sizeof (GstBufferClass),
			NULL,
			NULL,
			gst_camerasrc_buffer_class_init,
			NULL,
			NULL,
			sizeof (GstCameraBuffer),
			0,
			NULL,
			NULL
		};

		_gst_camerasrc_buffer_type = g_type_register_static(GST_TYPE_BUFFER,
		                                                    "GstCameraBuffer",
		                                                    &camera_buffer_info, 0);
	}

	return _gst_camerasrc_buffer_type;
}


static GstCameraBuffer *gst_camerasrc_buffer_new(GstCameraSrc *camerasrc)
{
	GstCameraBuffer *ret = NULL;

	ret = (GstCameraBuffer *)gst_mini_object_new(GST_TYPE_CAMERASRC_BUFFER);

	GST_LOG_OBJECT(camerasrc, "creating buffer : %p", ret);

	ret->camerasrc = gst_object_ref(GST_OBJECT(camerasrc));
	ret->is_alloc_data = FALSE;

	return ret;
}


static void gst_camerasrc_buffer_free(gpointer data)
{
	GST_CAMERASRC_BUFFER_DATA *buffer_data = (GST_CAMERASRC_BUFFER_DATA *)data;

	GST_LOG_OBJECT(buffer_data->camerasrc, "freeing buffer with %p %d",
	                                       buffer_data->camerasrc, buffer_data->index);

	g_mutex_lock(buffer_data->camerasrc->pad_alloc_mutex);

	if (buffer_data->buffer_data_index == buffer_data->camerasrc->current_buffer_data_index) {
		GST_LOG_OBJECT(buffer_data->camerasrc, "Push index [%d] to pad_alloc_list", buffer_data->index);
		g_queue_push_tail(buffer_data->camerasrc->pad_alloc_list, (gpointer)buffer_data->index);
	} else {
		GST_WARNING_OBJECT(buffer_data->camerasrc, "Skip Pushing index [%d] to pad_alloc_list.", buffer_data->index );
		GST_WARNING_OBJECT(buffer_data->camerasrc, "buffer_data_index [%d], current_buffer_data_index [%d]",
		                                           buffer_data->buffer_data_index,
		                                           buffer_data->camerasrc->current_buffer_data_index);
	}

	buffer_data->camerasrc->num_live_buffers--;

	g_mutex_unlock(buffer_data->camerasrc->pad_alloc_mutex);
	g_cond_signal(buffer_data->camerasrc->buffer_cond);

	buffer_data->camerasrc = NULL;
	buffer_data->index = -1;

	/* Free data argument */
	g_free(data);

	return;
}


static void gst_camerasrc_buffer_trace(GstCameraSrc *camerasrc)
{
	int i = 0;
	GstBuffer *buf = NULL;

	if (!camerasrc) {
		GST_ERROR_OBJECT(camerasrc, "Element is NULL");
		return;
	}

	for (i = 0 ; i < camerasrc->num_alloc_buf ; i ++) {
		buf = camerasrc->usr_buffer[i];
		if (GST_IS_BUFFER(buf)) {
			GST_ELEMENT_WARNING(camerasrc, RESOURCE, FAILED,
			                    NULL, (("Chainfunc of buf[%d]=%s()"), i,
		                            GST_DEBUG_FUNCPTR_NAME(buf->_gst_reserved[0])));
		} else {
			GST_ELEMENT_WARNING(camerasrc, RESOURCE, FAILED,
			                    NULL, (("buf[%d] is not GstBuffer"), i));
		}
	}

	return;
}

static gboolean gst_camerasrc_device_is_open(GstCameraSrc *camerasrc)
{
	return camerasrc_device_is_open((camsrc_handle_t)camerasrc->v4l2_handle);
}


static gboolean gst_camerasrc_get_timeinfo(GstCameraSrc *camerasrc, GstBuffer  *buffer)
{
	int fps_nu = 0;
	int fps_de = 0;
	GstClock *clock = NULL;
	GstClockTime timestamp = GST_CLOCK_TIME_NONE;
	GstClockTime duration = GST_CLOCK_TIME_NONE;

	if (!camerasrc || !buffer) {
		GST_WARNING_OBJECT (camerasrc, "Invalid pointer [hadle:%p, buffer:%p]", camerasrc, buffer);
		return FALSE;
	}

	/* timestamps, LOCK to get clock and base time. */
	clock = GST_ELEMENT_CLOCK(camerasrc);
	if (clock) {
		/* the time now is the time of the clock minus the base time */
		gst_object_ref(clock);
		timestamp = gst_clock_get_time(clock) - GST_ELEMENT(camerasrc)->base_time;
		gst_object_unref(clock);

		/* if we have a framerate adjust timestamp for frame latency */
		if (camerasrc->fps_auto) {
			/*if fps is zero, auto fps mode*/
			duration = GST_CLOCK_TIME_NONE;
		} else {
			if (camerasrc->fps <= 0) {
				/*if fps is zero, auto fps mode*/
				fps_nu = 0;
				fps_de = 1;
			} else {
				fps_nu = 1;
				fps_de = camerasrc->fps;
			}

			if (fps_nu > 0 && fps_de > 0) {
				GstClockTime latency;

				latency = gst_util_uint64_scale_int(GST_SECOND, fps_nu, fps_de);
				duration = latency;
			}
		}
	} else {
		/* no clock, can't set timestamps */
		timestamp = GST_CLOCK_TIME_NONE;
	}

	GST_BUFFER_TIMESTAMP(buffer) = timestamp;
	GST_BUFFER_DURATION(buffer) = duration;
/*
	GST_INFO_OBJECT(camerasrc, "[%"GST_TIME_FORMAT" dur %" GST_TIME_FORMAT "]",
	                           GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)),
	                           GST_TIME_ARGS(GST_BUFFER_DURATION(buffer)));
*/

	return TRUE;
}


static int get_proper_index(GstCameraSrc *camerasrc, GstBuffer *pad_alloc_buffer)
{
	int temp_idx = 0;

	while (GST_BUFFER_DATA(GST_BUFFER(pad_alloc_buffer)) != g_present_buf.present_buffer[temp_idx++].start) {
		if (temp_idx >= camerasrc->num_alloc_buf) {
			return -1;
		}
	}

	return temp_idx - 1; /* -1 is caused that it already increased in ++ */
}


/* Gstreamer general functions */
static gboolean gst_camerasrc_src_start(GstBaseSrc *src)
{
	int ret = TRUE;
	GstCameraSrc *camerasrc = GST_CAMERA_SRC (src);

	GST_DEBUG_OBJECT(camerasrc, "ENTERED");

#if _ENABLE_CAMERASRC_DEBUG
	g_preview_frame_cnt = 0;
#endif

	camerasrc->firsttime = TRUE;
	/* 'gst_camerasrc_set_caps' will call gst_camerasrc_start(). So skip to call it. */
	/*ret = gst_camerasrc_start(camerasrc);*/

	GST_DEBUG_OBJECT(camerasrc, "LEAVED");

	return ret;
}


static gboolean gst_camerasrc_src_stop(GstBaseSrc *src)
{
	int ret = 0;
	GstCameraSrc *camerasrc = GST_CAMERA_SRC(src);

	GST_DEBUG_OBJECT (camerasrc, "ENTERED");

	ret = gst_camerasrc_stop(camerasrc);

	GST_DEBUG_OBJECT(camerasrc, "LEAVED");

	return TRUE;
}


static GstFlowReturn gst_camerasrc_src_create(GstPushSrc *src, GstBuffer **buffer)
{
	GstCameraSrc *camerasrc = GST_CAMERA_SRC (src);
	GstFlowReturn ret;

	/*GST_DEBUG_OBJECT(camerasrc, "ENTERED");*/

	if (camerasrc->req_negotiation) {
		GST_INFO_OBJECT(camerasrc, "negotiation start");

		GST_BASE_SRC_GET_CLASS(camerasrc)->negotiate(GST_BASE_SRC(camerasrc));
		camerasrc->req_negotiation = FALSE;
		g_signal_emit(G_OBJECT(camerasrc), gst_camerasrc_signals[SIGNAL_NEGO_COMPLETE], (GQuark)NULL);

		GST_INFO_OBJECT (camerasrc, "negotiation stop");
	}

	__ta__("            gst_camerasrc_read",  
	ret = gst_camerasrc_read(camerasrc, buffer);
	)
	/*GST_DEBUG_OBJECT (camerasrc, "LEAVED");*/

	return ret;
}


static void gst_camerasrc_set_property(GObject *object, guint prop_id,
                                       const GValue *value, GParamSpec *pspec)
{
	int tmp = 0;
	GstCameraSrc *camerasrc = NULL;

	g_return_if_fail(GST_IS_CAMERA_SRC(object));
	camerasrc = GST_CAMERA_SRC(object);

	switch (prop_id) {
	case ARG_REQ_NEGOTIATION:
		camerasrc->req_negotiation = g_value_get_boolean(value);
		break;
	case ARG_CAMERA_HIGH_SPEED_FPS:
		tmp = g_value_get_int(value);
		camerasrc->high_speed_fps = tmp;
		break;
	case ARG_CAMERA_AUTO_FPS:
		camerasrc->fps_auto = g_value_get_boolean(value);
		GST_DEBUG_OBJECT(camerasrc, "Set AUTO_FPS:%d", camerasrc->fps_auto);
		break;
	case ARG_CAMERA_ID:
		camerasrc->camera_id = g_value_get_int(value);
		break;
	case ARG_CAMERA_EXT_VIDEO_FD:
		camerasrc->external_videofd = g_value_get_int(value);
		break;
	case ARG_CAMERA_CAPTURE_FOURCC:
		camerasrc->cap_fourcc = g_value_get_uint(value);
		break;
	case ARG_CAMERA_CAPTURE_QUALITY:
		camerasrc->cap_quality = g_value_get_enum(value);
		break;
	case ARG_CAMERA_CAPTURE_WIDTH:
		camerasrc->cap_width = g_value_get_int(value);
		break;
	case ARG_CAMERA_CAPTURE_HEIGHT:
		camerasrc->cap_height = g_value_get_int(value);
		break;
	case ARG_CAMERA_CAPTURE_INTERVAL:
		camerasrc->cap_interval = g_value_get_int(value);
		break;
	case ARG_CAMERA_CAPTURE_COUNT:
		tmp = g_value_get_int(value);
		camerasrc->cap_count = tmp;
		g_mutex_lock(camerasrc->jpg_mutex);
		camerasrc->cap_count_reverse = tmp;
		g_mutex_unlock(camerasrc->jpg_mutex);
		GST_INFO("SET reverse capture count: %d", camerasrc->cap_count_reverse);
		break;
	case ARG_CAMERA_CAPTURE_JPG_QUALITY:
		camerasrc->cap_jpg_quality = g_value_get_int(value);
		GST_INFO("SET jpeg compress ratio : %d", camerasrc->cap_jpg_quality);
		break;
	case ARG_SIGNAL_STILLCAPTURE:
		camerasrc->signal_still_capture = g_value_get_boolean(value);
		break;
	case ARG_USE_PAD_ALLOC:
		camerasrc->use_pad_alloc = g_value_get_boolean(value);
		break;
	case ARG_NUM_ALLOC_BUF:
		camerasrc->num_alloc_buf = g_value_get_int(value);
		break;
#ifdef SUPPORT_CAMERA_SENSOR_MODE		
	case ARG_SENSOR_MODE:
		camerasrc->sensor_mode = g_value_get_int(value);
		break;
#endif		
	case ARG_VFLIP:
		camerasrc->vflip = g_value_get_boolean(value);
		break;
	case ARG_HFLIP:
		camerasrc->hflip = g_value_get_boolean(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}


static void gst_camerasrc_get_property(GObject *object, guint prop_id,
                                       GValue *value, GParamSpec *pspec)
{
	GstCameraSrc *camerasrc;

	g_return_if_fail(GST_IS_CAMERA_SRC(object));
	camerasrc = GST_CAMERA_SRC(object);

	switch (prop_id) {
	case ARG_REQ_NEGOTIATION:
		g_value_set_boolean(value, camerasrc->req_negotiation);
		break;
	case ARG_CAMERA_HIGH_SPEED_FPS:
		g_value_set_int(value, camerasrc->high_speed_fps);
		break;
	case ARG_CAMERA_AUTO_FPS:
		g_value_set_boolean(value, camerasrc->fps_auto);
		break;
	case ARG_CAMERA_ID:
		g_value_set_int(value, camerasrc->camera_id);
		break;
	case ARG_CAMERA_EXT_VIDEO_FD:
		g_value_set_int(value, camerasrc->external_videofd);
		break;
	case ARG_CAMERA_CAPTURE_FOURCC:
		g_value_set_uint(value, camerasrc->cap_fourcc);
		break;
	case ARG_CAMERA_CAPTURE_QUALITY:
		g_value_set_enum(value, camerasrc->cap_quality);
		break;
	case ARG_CAMERA_CAPTURE_WIDTH:
		g_value_set_int(value, camerasrc->cap_width);
		break;
	case ARG_CAMERA_CAPTURE_HEIGHT:
		g_value_set_int(value, camerasrc->cap_height);
		break;
	case ARG_CAMERA_CAPTURE_INTERVAL:
		g_value_set_int(value, camerasrc->cap_interval);
		break;
	case ARG_CAMERA_CAPTURE_COUNT:
		g_value_set_int(value, camerasrc->cap_count);
		break;
	case ARG_CAMERA_CAPTURE_JPG_QUALITY:
		g_value_set_int(value, camerasrc->cap_jpg_quality);
		GST_INFO("GET jpeg compress ratio : %d", camerasrc->cap_jpg_quality);
		break;
	case ARG_SIGNAL_STILLCAPTURE:
		g_value_set_boolean(value, camerasrc->signal_still_capture);
		break;
	case ARG_USE_PAD_ALLOC:
		g_value_set_boolean(value, camerasrc->use_pad_alloc);
		break;
	case ARG_NUM_ALLOC_BUF:
		g_value_set_int(value, camerasrc->num_alloc_buf);
		break;
	case ARG_OPERATION_STATUS:
		g_value_set_int(value, 0);
		gst_camerasrc_buffer_trace(camerasrc);
		break;
#ifdef SUPPORT_CAMERA_SENSOR_MODE		
	case ARG_SENSOR_MODE:
		g_value_set_int(value, camerasrc->sensor_mode);
		break;
#endif		
	case ARG_VFLIP:
		g_value_set_boolean(value, camerasrc->vflip);
		break;
	case ARG_HFLIP:
		g_value_set_boolean(value, camerasrc->hflip);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}


static GstStateChangeReturn gst_camerasrc_change_state(GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstCameraSrc *camerasrc;
	camerasrc = GST_CAMERA_SRC (element);

	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_INFO_OBJECT(camerasrc, "GST CAMERA SRC: NULL -> READY");
		__ta__("    gst_camerasrc_create",
		if (!gst_camerasrc_create(camerasrc)){
			goto statechange_failed;
		}
		);
		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_INFO_OBJECT(camerasrc, "GST CAMERA SRC: READY -> PAUSED");
		ret = GST_STATE_CHANGE_NO_PREROLL;
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		GST_INFO_OBJECT(camerasrc, "GST CAMERA SRC: PAUSED -> PLAYING");
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
	if (ret == GST_STATE_CHANGE_FAILURE){
		return ret;
	}

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		GST_INFO_OBJECT(camerasrc, "GST CAMERA SRC: PLAYING -> PAUSED");
		ret = GST_STATE_CHANGE_NO_PREROLL;
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_INFO_OBJECT(camerasrc, "GST CAMERA SRC: PAUSED -> READY");
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_INFO_OBJECT(camerasrc, "GST CAMERA SRC: READY -> NULL");
		__ta__("    gst_camerasrc_destroy",
		if (!gst_camerasrc_destroy(camerasrc)){
			goto statechange_failed;
		}
		);
		break;
	default:
		break;
	}

	return ret;

 statechange_failed:
	/* subclass must post a meaningfull error message */
	GST_ERROR_OBJECT(camerasrc, "state change failed");

	return GST_STATE_CHANGE_FAILURE;
}


static void gst_camerasrc_finalize(GObject *object)
{
	GstCameraSrc *camerasrc = GST_CAMERA_SRC(object);

#if _ENABLE_CAMERASRC_DEBUG
	GST_INFO_OBJECT(camerasrc, "total [%u] preview frame(s) outputed.",g_preview_frame_cnt);
#endif

	g_mutex_free(camerasrc->jpg_mutex);
	g_mutex_free(camerasrc->mutex);
	g_cond_free(camerasrc->cond);
	g_mutex_free(camerasrc->buffer_lock);
	g_cond_broadcast(camerasrc->buffer_cond);
	g_cond_free(camerasrc->buffer_cond);

	if (camerasrc->command_list != NULL) {
		g_queue_free(camerasrc->command_list);
		camerasrc->command_list = NULL;
	}

	G_OBJECT_CLASS(parent_class)->finalize(object);
}


void gst_camerasrc_set_capture_command(GstCameraSrc *camerasrc, GstCameraControlCaptureCommand cmd)
{
	if (camerasrc == NULL) {
		GST_ERROR_OBJECT(camerasrc, "camerasrc is NULL");
		return;
	}

	GST_INFO_OBJECT(camerasrc, "ENTERED");

	g_mutex_lock(camerasrc->mutex);

	g_queue_push_tail(camerasrc->command_list, (gpointer)cmd);
	GST_INFO_OBJECT(camerasrc, "ACTION: Push capture command [%d] finished.", cmd);

	if (cmd == GST_CAMERA_CONTROL_CAPTURE_COMMAND_STOP) {
		g_cond_signal(camerasrc->cond);
		GST_INFO_OBJECT(camerasrc, "Send signal for CAPTURE STOP");
	}

	g_mutex_unlock(camerasrc->mutex);

	return;
}


static gboolean gst_camerasrc_negotiate (GstBaseSrc * basesrc)
{
	GstCaps *thiscaps;
	GstCaps *caps = NULL;
	GstCaps *peercaps = NULL;
	gboolean result = FALSE;
	GstStructure *s;
	GstCameraSrc *camerasrc = GST_CAMERA_SRC(basesrc);

	GST_INFO_OBJECT(camerasrc, "ENTERED");
	/* first see what is possible on our source pad */
	thiscaps = gst_pad_get_caps (GST_BASE_SRC_PAD (basesrc));
	GST_DEBUG_OBJECT (basesrc, "caps of src: %" GST_PTR_FORMAT, thiscaps);

	/* nothing or anything is allowed, we're done */
	if (thiscaps == NULL || gst_caps_is_any (thiscaps))
		goto no_nego_needed;

	/* get the peer caps */
	peercaps = gst_pad_peer_get_caps (GST_BASE_SRC_PAD (basesrc));
	GST_DEBUG_OBJECT (basesrc, "caps of peer: %" GST_PTR_FORMAT, peercaps);
	//LOG_CAPS (basesrc, peercaps);
	if (peercaps && !gst_caps_is_any (peercaps)) {
		GstCaps *icaps = NULL;
		int i;

		/* Prefer the first caps we are compatible with that the peer proposed */
		for (i = 0; i < gst_caps_get_size (peercaps); i++) {
			/* get intersection */
			GstCaps *ipcaps = gst_caps_copy_nth (peercaps, i);

			icaps = gst_caps_intersect (thiscaps, ipcaps);
			gst_caps_unref (ipcaps);

			if (!gst_caps_is_empty (icaps))
				break;

			gst_caps_unref (icaps);
			icaps = NULL;
		}

		GST_DEBUG_OBJECT (basesrc, "intersect: %" GST_PTR_FORMAT, icaps);
		if (icaps) {
			/* If there are multiple intersections pick the one with the smallest
			* resolution strictly bigger than the first peer caps */
			if (gst_caps_get_size (icaps) > 1) {
				s = gst_caps_get_structure (peercaps, 0);
				int best = 0;
				int twidth, theight;
				int width = G_MAXINT, height = G_MAXINT;

				if (gst_structure_get_int (s, "width", &twidth)
					&& gst_structure_get_int (s, "height", &theight)) {

					/* Walk the structure backwards to get the first entry of the
					* smallest resolution bigger (or equal to) the preferred resolution)
					*/
					for (i = gst_caps_get_size (icaps) - 1; i >= 0; i--) {
						GstStructure *is = gst_caps_get_structure (icaps, i);
						int w, h;

						if (gst_structure_get_int (is, "width", &w)
							&& gst_structure_get_int (is, "height", &h)) {
							if (w >= twidth && w <= width && h >= theight && h <= height) {
								width = w;
								height = h;
								best = i;
							}
						}
					}
				}

				caps = gst_caps_copy_nth (icaps, best);
				gst_caps_unref (icaps);
			} else {
				caps = icaps;
			}
		}
		gst_caps_unref (thiscaps);
		gst_caps_unref (peercaps);
	} else {
		/* no peer or peer has ANY caps, work with our own caps then */
		caps = thiscaps;
	}
	if (caps) {
		caps = gst_caps_make_writable (caps);
		gst_caps_truncate (caps);

		/* now fixate */
		if (!gst_caps_is_empty (caps)) {
			gst_pad_fixate_caps (GST_BASE_SRC_PAD (basesrc), caps);
			GST_DEBUG_OBJECT (basesrc, "fixated to: %" GST_PTR_FORMAT, caps);

			if (gst_caps_is_any (caps)) {
				/* hmm, still anything, so element can do anything and
				* nego is not needed */
				result = TRUE;
			} else if (gst_caps_is_fixed (caps)) {
				/* yay, fixed caps, use those then */
				if (gst_pad_set_caps (GST_BASE_SRC_PAD (basesrc), caps))
				result = TRUE;
			}
		}
		gst_caps_unref (caps);
	}
	return result;

no_nego_needed:
	{
		GST_DEBUG_OBJECT (basesrc, "no negotiation needed");
		if (thiscaps)
			gst_caps_unref (thiscaps);
		return TRUE;
	}
}


static GstCaps *gst_camerasrc_get_caps(GstBaseSrc *src)
{
	GstCameraSrc *camerasrc = GST_CAMERA_SRC(src);
	GstCaps *ret = NULL;

	GST_DEBUG_OBJECT(camerasrc, "ENTERED");

	if (camerasrc->mode == VIDEO_IN_MODE_UNKNOWN) {
		GST_INFO_OBJECT(camerasrc, "Unknown mode. Just return template caps.");
		GST_DEBUG_OBJECT(camerasrc, "LEAVED");

		return gst_caps_copy(gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(camerasrc)));
	}

	/*FIXME: Using "VIDIOC_ENUM_FMT".*/
	ret = gst_caps_copy(gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(camerasrc)));

	GST_INFO_OBJECT(camerasrc, "probed caps: %x", ret);
	GST_DEBUG_OBJECT(camerasrc, "LEAVED");

	return ret;
}


static gboolean _gst_camerasrc_get_raw_pixel_info(int fourcc, int *pix_format, int *colorspace)
{
	switch (fourcc) {
	case GST_MAKE_FOURCC('I','4','2','0'):	/* V4L2_PIX_FMT_YUV420 */
	case GST_MAKE_FOURCC('I','Y','U','V'):
	case GST_MAKE_FOURCC('Y','U','1','2'):
	case GST_MAKE_FOURCC('S','4','2','0'):
		*pix_format = CAMERASRC_PIX_YUV420P;
		*colorspace = CAMERASRC_COL_RAW;
		break;
	case GST_MAKE_FOURCC('Y','U','Y','V'):	/* V4L2_PIX_FMT_YUYV */
	case GST_MAKE_FOURCC('Y','U','Y','2'):	/* V4L2_PIX_FMT_YUYV */
	case GST_MAKE_FOURCC('S','U','Y','V'):
	case GST_MAKE_FOURCC('S','U','Y','2'):
		*pix_format = CAMERASRC_PIX_YUY2;
		*colorspace = CAMERASRC_COL_RAW;
		break;
	case GST_MAKE_FOURCC('U','Y','V','Y'):	/* V4L2_PIX_FMT_UYVY */
	case GST_MAKE_FOURCC('S','Y','V','Y'):	/* V4L2_PIX_FMT_UYVY */
		*pix_format = CAMERASRC_PIX_UYVY;
		*colorspace = CAMERASRC_COL_RAW;
		break;
	case GST_MAKE_FOURCC('4','2','2','P'):	/* V4L2_PIX_FMT_YUV422P */
	case GST_MAKE_FOURCC('Y','4','2','B'):	/* V4L2_PIX_FMT_YUV422P */
		*pix_format = CAMERASRC_PIX_YUV422P;
		*colorspace = CAMERASRC_COL_RAW;
		break;
	case GST_MAKE_FOURCC('N','V','1','2'):	/* V4L2_PIX_FMT_NV12 */
		*pix_format = CAMERASRC_PIX_NV12;
		*colorspace = CAMERASRC_COL_RAW;
		break;
	case GST_MAKE_FOURCC('S','N','1','2'):	/* V4L2_PIX_FMT_NV12 non-linear */
		*pix_format = CAMERASRC_PIX_SN12;
		*colorspace = CAMERASRC_COL_RAW;
		break;
	case GST_MAKE_FOURCC('S','T','1','2'):	/* V4L2_PIX_FMT_NV12 tiled non-linear */
		*pix_format = CAMERASRC_PIX_ST12;
		*colorspace = CAMERASRC_COL_RAW;
		break;
	case GST_MAKE_FOURCC('J','P','E','G'):
	case GST_MAKE_FOURCC('j','p','e','g'):
		*pix_format = CAMERASRC_PIX_RGGB8;
		*colorspace = CAMERASRC_COL_JPEG;
		break;
	case GST_MAKE_FOURCC('M','J','P','G'):
	case GST_MAKE_FOURCC('m','j','p','g'):
		*pix_format = CAMERASRC_PIX_MJPEG;
		*colorspace = CAMERASRC_COL_JPEG;
		break;
	case GST_MAKE_FOURCC('Y','4','1','P'):	/* V4L2_PIX_FMT_Y41P */
	case GST_MAKE_FOURCC('N','V','2','1'):	/* V4L2_PIX_FMT_NV21 */
	case GST_MAKE_FOURCC('Y','4','1','B'):	/* V4L2_PIX_FMT_YUV411P */
	case GST_MAKE_FOURCC('Y','V','1','2'):	/* V4L2_PIX_FMT_YVU420 */
	default:
		/* ERROR */
		*pix_format = CAMERASRC_PIX_NONE;
		*colorspace = CAMERASRC_COL_NONE;
		break;
	}

	return TRUE;
}


static gboolean _gst_camerasrc_get_frame_size(int fourcc, int width, int height, unsigned int *outsize)
{
	switch (fourcc) {
	case GST_MAKE_FOURCC('I','4','2','0'):	/* V4L2_PIX_FMT_YUV420 */
	case GST_MAKE_FOURCC('I','Y','U','V'):
	case GST_MAKE_FOURCC('Y','U','1','2'):
	case GST_MAKE_FOURCC('Y','V','1','2'):
	case GST_MAKE_FOURCC('S','4','2','0'):	/* V4L2_PIX_FMT_NV12 tiled non-linear */
		*outsize = GST_ROUND_UP_4 (width) * GST_ROUND_UP_2 (height);
		*outsize += 2 * ((GST_ROUND_UP_8 (width) / 2) * (GST_ROUND_UP_2 (height) / 2));
		break;
	case GST_MAKE_FOURCC('Y','U','Y','V'):	/* V4L2_PIX_FMT_YUYV */
	case GST_MAKE_FOURCC('Y','U','Y','2'):	/* V4L2_PIX_FMT_YUYV */
	case GST_MAKE_FOURCC('S','U','Y','V'):
	case GST_MAKE_FOURCC('S','U','Y','2'):
	case GST_MAKE_FOURCC('U','Y','V','Y'):	/* V4L2_PIX_FMT_UYVY */
	case GST_MAKE_FOURCC('S','Y','V','Y'):	/* V4L2_PIX_FMT_UYVY */
	case GST_MAKE_FOURCC('4','2','2','P'):	/* V4L2_PIX_FMT_YUV422P */
	case GST_MAKE_FOURCC('Y','4','2','B'):	/* V4L2_PIX_FMT_YUV422P */
	case GST_MAKE_FOURCC('Y','4','1','P'):	/* V4L2_PIX_FMT_Y41P */
		*outsize = (GST_ROUND_UP_2 (width) * 2) * height;
		break;
	case GST_MAKE_FOURCC('Y','4','1','B'):	/* V4L2_PIX_FMT_YUV411P */
		*outsize = GST_ROUND_UP_4 (width) * height;
		*outsize += 2 * ((GST_ROUND_UP_8 (width) / 4) * height);
		break;
	case GST_MAKE_FOURCC('N','V','1','2'):	/* V4L2_PIX_FMT_NV12 */
	case GST_MAKE_FOURCC('N','V','2','1'):	/* V4L2_PIX_FMT_NV21 */
	case GST_MAKE_FOURCC('S','N','1','2'):	/* V4L2_PIX_FMT_NV12 non-linear */
	case GST_MAKE_FOURCC('S','T','1','2'):	/* V4L2_PIX_FMT_NV12 tiled non-linear */
		*outsize = GST_ROUND_UP_4 (width) * GST_ROUND_UP_2 (height);
		*outsize += (GST_ROUND_UP_4 (width) * height) / 2;
		break;
	case GST_MAKE_FOURCC('J','P','E','G'):
	case GST_MAKE_FOURCC('j','p','e','g'):
		/* jpeg size can't be calculated here. */
		*outsize = 0;
		break;
	default:
		/* unkown format!! */
		*outsize = 0;
		break;
	}

	return TRUE;
}

static gboolean gst_camerasrc_get_caps_info(GstCameraSrc *camerasrc, GstCaps *caps, guint *size)
{
	gint fps_n = 0;
	gint fps_d = 0;
	gint w = 0;
	gint h = 0;
	gint rot = 0;
	gchar *caps_string = NULL;
	const gchar *mimetype;
	const GValue *framerate;
	GstStructure *structure = NULL;

	GST_INFO_OBJECT(camerasrc, "ENTERED Collect data for given caps.(caps:%x)", caps);

	structure = gst_caps_get_structure(caps, 0);

	if (!gst_structure_get_int(structure, "width", &w)) {
		goto _caps_info_failed;
	}

	if (!gst_structure_get_int(structure, "height", &h)) {
		goto _caps_info_failed;
	}

	if (!gst_structure_get_int(structure, "rotate", &rot)) {
		GST_WARNING_OBJECT(camerasrc, "Failed to get rotate info in caps. set default 0.");
		camerasrc->use_rotate_caps = FALSE;
	} else {
		GST_INFO_OBJECT(camerasrc, "Succeed to get rotate[%d] info in caps", rot);
		camerasrc->use_rotate_caps = TRUE;
	}

	/* set default size if there is no capsfilter */
	if (w == 1) {
		w = _DEFAULT_WIDTH * 2;
	}

	if (h == 1) {
		h = _DEFAULT_HEIGHT * 2;
	}

	camerasrc->width = w;
	camerasrc->height = h;
	camerasrc->rotate = rot;

	framerate = gst_structure_get_value(structure, "framerate");
	if (!framerate) {
		GST_INFO("Set FPS as default");

		/* set default fps if framerate is not existed in caps */
		fps_n = _DEFAULT_FPS;
		fps_d = 1;
	} else {
		fps_n = gst_value_get_fraction_numerator(framerate);
		fps_d = gst_value_get_fraction_denominator(framerate);

		/* numerator and denominator should be bigger than zero */
		if (fps_n <= 0) {
			GST_WARNING("numerator of FPS is %d. make it default(15).", fps_n);
			fps_n = _DEFAULT_FPS;
		}

		if (fps_d <= 0) {
			GST_WARNING("denominator of FPS is %d. make it 1.", fps_d);
			fps_d = 1;
		}
	}

	camerasrc->fps = (int)((float)fps_n / (float)fps_d);

	mimetype = gst_structure_get_name (structure);

	*size = 0;

	if (!strcmp(mimetype, "video/x-raw-yuv")) {
		gst_structure_get_fourcc(structure, "format", &camerasrc->fourcc);
		if (camerasrc->fourcc == 0) {
			GST_INFO_OBJECT(camerasrc, "Getting fourcc is zero.");
			goto _caps_info_failed;
		}

		_gst_camerasrc_get_frame_size(camerasrc->fourcc, w, h, size);
		_gst_camerasrc_get_raw_pixel_info(camerasrc->fourcc, &(camerasrc->pix_format), &(camerasrc->colorspace));
	} else if (!strcmp (mimetype, "video/x-raw-rgb")) {
		gint depth = 0;
		gint endianness = 0;
		gint r_mask = 0;

		gst_structure_get_int(structure, "depth", &depth);
		gst_structure_get_int(structure, "endianness", &endianness);
		gst_structure_get_int(structure, "red_mask", &r_mask);

		switch (depth) {
		case 8:  /* V4L2_PIX_FMT_RGB332 */
			camerasrc->pix_format = CAMERASRC_PIX_RGGB8;
			camerasrc->colorspace = CAMERASRC_COL_RAW;
			break;
		case 15: /* V4L2_PIX_FMT_RGB555 : V4L2_PIX_FMT_RGB555X */
			camerasrc->pix_format = CAMERASRC_PIX_NONE;
			camerasrc->colorspace = CAMERASRC_COL_NONE;
			break;
		case 16: /* V4L2_PIX_FMT_RGB565 : V4L2_PIX_FMT_RGB565X */
			camerasrc->pix_format = CAMERASRC_PIX_NONE;
			camerasrc->colorspace = CAMERASRC_COL_NONE;
			break;
		case 24: /* V4L2_PIX_FMT_BGR24 : V4L2_PIX_FMT_RGB24 */
			camerasrc->pix_format = CAMERASRC_PIX_NONE;
			camerasrc->colorspace = CAMERASRC_COL_NONE;
			break;
		case 32: /* V4L2_PIX_FMT_BGR32 : V4L2_PIX_FMT_RGB32 */
			camerasrc->pix_format = CAMERASRC_PIX_NONE;
			camerasrc->colorspace = CAMERASRC_COL_NONE;
			break;
		}
	} else if (strcmp(mimetype, "video/x-dv") == 0) { /* V4L2_PIX_FMT_DV */
		camerasrc->pix_format = CAMERASRC_PIX_NONE;
		camerasrc->colorspace = CAMERASRC_COL_NONE;
	} else if (strcmp(mimetype, "video/x-jpeg") == 0) { /* V4L2_PIX_FMT_JPEG */
		camerasrc->pix_format = CAMERASRC_PIX_MJPEG; /* default */		
		camerasrc->colorspace = CAMERASRC_COL_JPEG;
	} else if (strcmp(mimetype, "image/jpeg") == 0) { /* V4L2_PIX_FMT_JPEG */
		camerasrc->pix_format = CAMERASRC_PIX_MJPEG; /* default */
		camerasrc->colorspace = CAMERASRC_COL_JPEG;
	}

	caps_string = gst_caps_to_string(caps);
	GST_INFO_OBJECT(camerasrc, "pixformat %d, colorspace %d, size %d, caps : [%s]",
	                           camerasrc->pix_format, camerasrc->colorspace, *size, caps_string);
	if (caps_string) {
		g_free(caps_string);
		caps_string = NULL;
	}

	return TRUE;

_caps_info_failed:
	GST_INFO_OBJECT(camerasrc, "Failed to get caps info.");
	GST_DEBUG_OBJECT(camerasrc, "LEAVED");
	return FALSE;
}


static gboolean gst_camerasrc_set_caps(GstBaseSrc *src, GstCaps *caps)
{
	guint size;
	GstCameraSrc *camerasrc = NULL;

	camerasrc = GST_CAMERA_SRC(src);

	GST_INFO_OBJECT(camerasrc, "ENTERED");

	if (camerasrc->mode == VIDEO_IN_MODE_PREVIEW ||
	    camerasrc->mode == VIDEO_IN_MODE_VIDEO) {
		GST_INFO_OBJECT(camerasrc, "Proceed set_caps");
		__ta__("            gst_camerasrc_stop",
		if (!gst_camerasrc_stop(camerasrc)) {
			GST_INFO_OBJECT(camerasrc, "Cam sensor stop failed.");
		}
		);
	} else if (camerasrc->mode == VIDEO_IN_MODE_CAPTURE) {
		GST_ERROR_OBJECT(camerasrc, "A mode of avsystem camera is capture. Not to proceed set_caps.");
		GST_DEBUG_OBJECT(camerasrc, "LEAVED");
		return FALSE;
	} else {
		GST_INFO_OBJECT(camerasrc, "A mode of avsystem camera is unknown[%d]. Proceed set_caps.", camerasrc->mode);
	}

	/* we want our own v4l2 type of fourcc codes */
	if (!gst_camerasrc_get_caps_info(camerasrc, caps, &size)) {
		GST_INFO_OBJECT(camerasrc, "can't get capture information from caps %x", caps);
		return FALSE;
	}

	__ta__("            gst_camerasrc_start",
	if (!gst_camerasrc_start(camerasrc)) {
		GST_INFO_OBJECT (camerasrc,  "Cam sensor start failed.");
	}
	);

	GST_INFO_OBJECT (camerasrc, "LEAVED");

	return TRUE;
}


static void gst_camerasrc_base_init(gpointer klass)
{
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

	GST_DEBUG_CATEGORY_INIT(camerasrc_debug, "camerasrc", 0, "camerasrc element");

	GST_INFO("ENTERED");

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get (&src_factory));
	gst_element_class_set_details(element_class, &camerasrc_details);

	GST_INFO("LEAVED");
}


static void gst_camerasrc_class_init(GstCameraSrcClass *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *element_class;
	GstBaseSrcClass *basesrc_class;
	GstPushSrcClass *pushsrc_class;

	GST_DEBUG("ENTERED");

	gobject_class = G_OBJECT_CLASS(klass);
	element_class = GST_ELEMENT_CLASS(klass);
	basesrc_class = GST_BASE_SRC_CLASS(klass);
	pushsrc_class = GST_PUSH_SRC_CLASS(klass);

	gobject_class->set_property = gst_camerasrc_set_property;
	gobject_class->get_property = gst_camerasrc_get_property;
	gobject_class->finalize = gst_camerasrc_finalize;
	element_class->change_state = gst_camerasrc_change_state;
	basesrc_class->start = gst_camerasrc_src_start;
	basesrc_class->stop = gst_camerasrc_src_stop;
	basesrc_class->get_caps = gst_camerasrc_get_caps;
	basesrc_class->set_caps = gst_camerasrc_set_caps;
	basesrc_class->negotiate = gst_camerasrc_negotiate;
	pushsrc_class->create = gst_camerasrc_src_create;

	g_object_class_install_property(gobject_class, ARG_REQ_NEGOTIATION,
	                                g_param_spec_boolean("req-negotiation", "Request re-negotiation",
	                                                     "Request to negotiate while on playing",
	                                                     FALSE,
	                                                     G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, ARG_CAMERA_HIGH_SPEED_FPS,
	                                g_param_spec_int("high-speed-fps", "Fps for high speed recording",
	                                                 "If this value is 0, the element doesn't activate high speed recording.",
	                                                 0, G_MAXINT, _DEFAULT_HIGH_SPEED_FPS,
	                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_CAMERA_AUTO_FPS,
	                                g_param_spec_boolean("fps-auto", "FPS Auto",
	                                                     "Field for auto fps setting",
	                                                     _DEFAULT_FPS_AUTO,
	                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_CAMERA_EXT_VIDEO_FD,
	                                g_param_spec_int("extern_videofd", "External video file descriptor",
	                                                 "External video file descriptor",
	                                                 _FD_MIN, _FD_MAX, _FD_MIN,
	                                                 G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, ARG_CAMERA_ID,
	                                g_param_spec_int("camera-id", "index number of camera to activate",
	                                                 "index number of camera to activate",
	                                                 _FD_MIN, _FD_MAX, 0,
	                                                 G_PARAM_READWRITE));

	/*Capture*/
	g_object_class_install_property(gobject_class, ARG_CAMERA_CAPTURE_FOURCC,
	                                g_param_spec_uint("capture-fourcc", "Capture format",
	                                                  "Fourcc value for capture format",
	                                                  0, G_MAXUINT, 0,
	                                                  G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, ARG_CAMERA_CAPTURE_QUALITY,
	                                g_param_spec_enum("capture-quality", "Capture quality",
	                                                  "Quality of capture image (JPEG: 'high', RAW: 'high' or 'low')",
	                                                  GST_TYPE_CAMERASRC_QUALITY, _DEFAULT_CAP_QUALITY,
	                                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_CAMERA_CAPTURE_WIDTH,
	                                g_param_spec_int("capture-width", "Capture width",
	                                                 "Width for camera size to capture",
	                                                 0, G_MAXINT, _DEFAULT_CAP_WIDTH,
	                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_CAMERA_CAPTURE_HEIGHT,
	                                g_param_spec_int("capture-height", "Capture height",
	                                                 "Height for camera size to capture",
	                                                 0, G_MAXINT, _DEFAULT_CAP_HEIGHT,
	                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_CAMERA_CAPTURE_INTERVAL,
	                                g_param_spec_int("capture-interval", "Capture interval",
	                                                 "Interval time to capture (millisecond)",
	                                                 0, G_MAXINT, _DEFAULT_CAP_INTERVAL,
	                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_CAMERA_CAPTURE_COUNT,
	                                g_param_spec_int("capture-count", "Capture count",
	                                                 "Capture conut for multishot",
	                                                 1, G_MAXINT, _DEFAULT_CAP_COUNT,
	                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_CAMERA_CAPTURE_JPG_QUALITY,
	                                g_param_spec_int("capture-jpg-quality", "JPEG Capture compress ratio",
	                                                 "Quality of capture image compress ratio",
	                                                 1, 100, _DEFAULT_CAP_JPG_QUALITY,
	                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_SIGNAL_STILLCAPTURE,
	                                g_param_spec_boolean("signal-still-capture", "Signal still capture",
	                                                     "Send a signal before pushing the buffer",
	                                                     _DEFAULT_SIGNAL_STILL_CAPTURE,
	                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_USE_PAD_ALLOC,
	                                g_param_spec_boolean("use-pad-alloc", "Use pad alloc",
	                                                     "Use gst_pad_alloc_buffer() for image frame buffer",
	                                                     _DEFAULT_USE_PAD_ALLOC,
	                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_NUM_ALLOC_BUF,
	                                g_param_spec_int("num-alloc-buf", "Number alloced buffer",
	                                                 "Number of buffer to alloc using gst_pad_alloc_buffer()",
	                                                 1, _MAX_NUM_ALLOC_BUF, _DEFAULT_NUM_ALLOC_BUF,
	                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_OPERATION_STATUS,
	                                g_param_spec_int("operation-status", "Check operation status of camerasrc",
	                                                 "This value has bit flags that describe current operation status.",
	                                                 G_MININT, G_MAXINT, 0,
	                                                 G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

#ifdef SUPPORT_CAMERA_SENSOR_MODE
	g_object_class_install_property(gobject_class, ARG_SENSOR_MODE,
	                                g_param_spec_int("sensor-mode", "Sensor mode",
	                                                 "Set sensor mode as CAMERA or MOVIE(camcorder)",
	                                                 CAMERASRC_SENSOR_MODE_CAMERA, CAMERASRC_SENSOR_MODE_MOVIE, CAMERASRC_SENSOR_MODE_CAMERA,
	                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

	g_object_class_install_property(gobject_class, ARG_VFLIP,
	                                g_param_spec_boolean("vflip", "Flip vertically",
	                                                     "Flip camera input vertically",
	                                                     0,
	                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_class, ARG_HFLIP,
	                                g_param_spec_boolean("hflip", "Flip horizontally",
	                                                     "Flip camera input horizontally",
	                                                     0,
	                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstCameraSrc::still-capture:
	* @camerasrc: the camerasrc instance
	* @buffer: the buffer that will be pushed - Main
	*
	* This signal gets emitted before sending the buffer.
	*/
	gst_camerasrc_signals[SIGNAL_STILL_CAPTURE] =
		g_signal_new("still-capture",
		             G_TYPE_FROM_CLASS(klass),
		             G_SIGNAL_RUN_LAST,
		             G_STRUCT_OFFSET(GstCameraSrcClass, still_capture),
		             NULL,
		             NULL,
		             gst_camerasrc_VOID__OBJECT_OBJECT,
		             G_TYPE_NONE,
		             3, /* Number of parameter */
		             GST_TYPE_BUFFER,  /* Main image buffer */
		             GST_TYPE_BUFFER,  /* Thumbnail image buffer */
		             GST_TYPE_BUFFER); /* Screennail image buffer */

	/* wh01.cho:req-negotiation:+:To notify user of camerasrc, after changing resolution. */
	/**
	* GstCameraSrc::nego-complete:
	* @camerasrc: the camerasrc instance
	* @start: when re-negotiation is finished.
	*
	*/
	gst_camerasrc_signals[SIGNAL_NEGO_COMPLETE] =
		g_signal_new("nego-complete",
		             G_TYPE_FROM_CLASS (klass),
		             G_SIGNAL_RUN_LAST,
		             G_STRUCT_OFFSET(GstCameraSrcClass, nego_complete),
		             NULL,
		             NULL,
		             gst_marshal_VOID__VOID,
		             G_TYPE_NONE, 0);

	GST_DEBUG("LEAVED");
}


static void gst_camerasrc_init(GstCameraSrc *camerasrc, GstCameraSrcClass *klass)
{
	GST_DEBUG_OBJECT (camerasrc, "ENTERED");

	camerasrc->v4l2_handle = NULL;
	camerasrc->mode = VIDEO_IN_MODE_UNKNOWN;
	camerasrc->firsttime = TRUE;
	camerasrc->main_buf_sz = 0;
	camerasrc->cap_count_current = -1;
	camerasrc->cap_count_reverse = _DEFAULT_CAP_COUNT;
	camerasrc->cap_next_time = 0UL;
	camerasrc->command_list = g_queue_new ();
	camerasrc->cond = g_cond_new ();
	camerasrc->mutex = g_mutex_new ();

	/*camera*/
	camerasrc->width = _DEFAULT_WIDTH;
	camerasrc->height = _DEFAULT_HEIGHT;
	camerasrc->fps = _DEFAULT_FPS;
	camerasrc->rotate = 0;
	camerasrc->use_rotate_caps = FALSE;
	camerasrc->high_speed_fps = _DEFAULT_HIGH_SPEED_FPS;
	camerasrc->fps_auto = _DEFAULT_FPS_AUTO;
	camerasrc->pix_format = _DEFAULT_PIX_FORMAT;
	camerasrc->colorspace = _DEFAULT_COLORSPACE;
	camerasrc->fourcc = _DEFAULT_FOURCC;
	camerasrc->req_negotiation = FALSE;
	camerasrc->num_live_buffers = _DEFAULT_NUM_LIVE_BUFFER;
	camerasrc->buffer_count = _DEFAULT_BUFFER_COUNT;
	camerasrc->buffer_running = _DEFAULT_BUFFER_RUNNING;
	camerasrc->buffer_lock = g_mutex_new ();
	camerasrc->buffer_cond = g_cond_new ();
	camerasrc->bfirst = TRUE;
	camerasrc->pad_alloc_list = g_queue_new ();
	camerasrc->pad_alloc_mutex = g_mutex_new ();
	camerasrc->current_buffer_data_index = 0;
#ifdef SUPPORT_CAMERA_SENSOR_MODE	
	camerasrc->sensor_mode = CAMERASRC_SENSOR_MODE_CAMERA;
#endif	
	camerasrc->vflip = 0;
	camerasrc->hflip = 0;

	/* Initialize usr buffer to be alloced by sink to NULL */
	{
		int i = 0;
		for(i = 0; i < MAX_USR_BUFFER_NUM; i++) {
			camerasrc->usr_buffer[i] = NULL;
		}
	}
	camerasrc->use_pad_alloc = FALSE;
	camerasrc->num_alloc_buf = 0;
	camerasrc->camera_id = _DEFAULT_CAMERA_ID;

	/*capture*/
	camerasrc->cap_fourcc = _DEFAULT_FOURCC;
	camerasrc->cap_quality = _DEFAULT_CAP_QUALITY;
	camerasrc->cap_width = _DEFAULT_CAP_WIDTH;
	camerasrc->cap_height = _DEFAULT_CAP_HEIGHT;
	camerasrc->cap_interval = _DEFAULT_CAP_INTERVAL;
	camerasrc->cap_count = _DEFAULT_CAP_COUNT;
	camerasrc->cap_jpg_quality = _DEFAULT_CAP_JPG_QUALITY;
	camerasrc->cap_provide_exif = _DEFAULT_CAP_PROVIDE_EXIF;
	camerasrc->signal_still_capture = _DEFAULT_SIGNAL_STILL_CAPTURE;
	camerasrc->create_jpeg = FALSE;
	camerasrc->jpg_mutex = g_mutex_new();
	camerasrc->first_invokation = TRUE;
	camerasrc->external_videofd = _FD_DEFAULT;
	camerasrc->buf_share_method = BUF_SHARE_METHOD_PADDR;

#ifdef _SPEED_UP_RAW_CAPTURE
	camerasrc->cap_stream_diff = FALSE;
#endif /* _SPEED_UP_RAW_CAPTURE */

	/* we operate in time */
	gst_base_src_set_format(GST_BASE_SRC(camerasrc), GST_FORMAT_TIME);
	gst_base_src_set_live(GST_BASE_SRC(camerasrc), TRUE);
	gst_base_src_set_do_timestamp(GST_BASE_SRC(camerasrc), TRUE);

	GST_DEBUG("LEAVED");
}

static unsigned long gst_get_current_time(void)
{
	struct timeval lc_time;

	gettimeofday(&lc_time, NULL);

	return ((unsigned long)(lc_time.tv_sec * 1000L) + (unsigned long)(lc_time.tv_usec / 1000L));
}


#if _ENABLE_CAMERASRC_DEBUG
#include <stdio.h>
static int __util_write_file(char *filename, void *data, int size)
{
	FILE *fp = NULL;

	fp = fopen(filename, "wb");
	if (!fp) {
		return FALSE;
	}

	fwrite(data, 1, size, fp);
	fclose(fp);

	return TRUE;
}
#endif


static gboolean plugin_init(GstPlugin *plugin)
{
	gboolean error;

	error = gst_element_register(plugin, "camerasrc", GST_RANK_PRIMARY + 100, GST_TYPE_CAMERA_SRC);

	return error;
}


GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  "camerasrc",
                  "Camera source plug-in",
                  plugin_init,
                  PACKAGE_VERSION,
                  "LGPL",
                  "Samsung Electronics Co",
                  "http://www.samsung.com")

/* GstURIHandler interface */
static GstURIType gst_camerasrc_uri_get_type (void)
{
	return GST_URI_SRC;
}

static gchar ** gst_camerasrc_uri_get_protocols (void)
{
	static gchar *protocols[] = { (char *) "camera", NULL };
	return protocols;
}

static const gchar * gst_camerasrc_uri_get_uri (GstURIHandler * handler)
{
	//GstCameraSrc *camerasrc = GST_CAMERA_SRC (handler);

	gchar uri[256];
	g_snprintf (uri, sizeof (uri), "camera://0");
	return g_intern_string (uri);
}

static gboolean gst_camerasrc_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
	GstCameraSrc *camerasrc = GST_CAMERA_SRC (handler);
	const gchar *device = "0";
	if (strcmp (uri, "camera://") != 0) {
		device = uri + 9;
	}
	g_object_set (camerasrc, "camera-id", atoi(device), NULL);

	return TRUE;
}


static void gst_camerasrc_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
	GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

	iface->get_type = gst_camerasrc_uri_get_type;
	iface->get_protocols = gst_camerasrc_uri_get_protocols;
	iface->get_uri = gst_camerasrc_uri_get_uri;
	iface->set_uri = gst_camerasrc_uri_set_uri;
}
/* EOF */
