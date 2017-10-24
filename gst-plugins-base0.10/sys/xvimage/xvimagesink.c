/* GStreamer
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
 *               <2009>,<2010> Stefan Kost <stefan.kost@nokia.com>
 * Copyright (C) 2012, 2013 Samsung Electronics Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * * Modifications by Samsung Electronics Co., Ltd.
 * 1. Add display related properties
 * 2. Support samsung extension format to improve performance
 * 3. Support video texture overlay of OSP layer
 */

/**
 * SECTION:element-xvimagesink
 *
 * XvImageSink renders video frames to a drawable (XWindow) on a local display
 * using the XVideo extension. Rendering to a remote display is theoretically
 * possible but i doubt that the XVideo extension is actually available when
 * connecting to a remote display. This element can receive a Window ID from the
 * application through the XOverlay interface and will then render video frames
 * in this drawable. If no Window ID was provided by the application, the
 * element will create its own internal window and render into it.
 *
 * <refsect2>
 * <title>Scaling</title>
 * <para>
 * The XVideo extension, when it's available, handles hardware accelerated
 * scaling of video frames. This means that the element will just accept
 * incoming video frames no matter their geometry and will then put them to the
 * drawable scaling them on the fly. Using the #GstXvImageSink:force-aspect-ratio
 * property it is possible to enforce scaling with a constant aspect ratio,
 * which means drawing black borders around the video frame.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Events</title>
 * <para>
 * XvImageSink creates a thread to handle events coming from the drawable. There
 * are several kind of events that can be grouped in 2 big categories: input
 * events and window state related events. Input events will be translated to
 * navigation events and pushed upstream for other elements to react on them.
 * This includes events such as pointer moves, key press/release, clicks etc...
 * Other events are used to handle the drawable appearance even when the data
 * is not flowing (GST_STATE_PAUSED). That means that even when the element is
 * paused, it will receive expose events from the drawable and draw the latest
 * frame with correct borders/aspect-ratio.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Pixel aspect ratio</title>
 * <para>
 * When changing state to GST_STATE_READY, XvImageSink will open a connection to
 * the display specified in the #GstXvImageSink:display property or the
 * default display if nothing specified. Once this connection is open it will
 * inspect the display configuration including the physical display geometry and
 * then calculate the pixel aspect ratio. When receiving video frames with a
 * different pixel aspect ratio, XvImageSink will use hardware scaling to
 * display the video frames correctly on display's pixel aspect ratio.
 * Sometimes the calculated pixel aspect ratio can be wrong, it is
 * then possible to enforce a specific pixel aspect ratio using the
 * #GstXvImageSink:pixel-aspect-ratio property.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Examples</title>
 * |[
 * gst-launch -v videotestsrc ! xvimagesink
 * ]| A pipeline to test hardware scaling.
 * When the test video signal appears you can resize the window and see that
 * video frames are scaled through hardware (no extra CPU cost).
 * |[
 * gst-launch -v videotestsrc ! xvimagesink force-aspect-ratio=true
 * ]| Same pipeline with #GstXvImageSink:force-aspect-ratio property set to true
 * You can observe the borders drawn around the scaled image respecting aspect
 * ratio.
 * |[
 * gst-launch -v videotestsrc ! navigationtest ! xvimagesink
 * ]| A pipeline to test navigation events.
 * While moving the mouse pointer over the test signal you will see a black box
 * following the mouse pointer. If you press the mouse button somewhere on the
 * video and release it somewhere else a green box will appear where you pressed
 * the button and a red one where you released it. (The navigationtest element
 * is part of gst-plugins-good.) You can observe here that even if the images
 * are scaled through hardware the pointer coordinates are converted back to the
 * original video frame geometry so that the box can be drawn to the correct
 * position. This also handles borders correctly, limiting coordinates to the
 * image area
 * |[
 * gst-launch -v videotestsrc ! video/x-raw-yuv, pixel-aspect-ratio=(fraction)4/3 ! xvimagesink
 * ]| This is faking a 4/3 pixel aspect ratio caps on video frames produced by
 * videotestsrc, in most cases the pixel aspect ratio of the display will be
 * 1/1. This means that XvImageSink will have to do the scaling to convert
 * incoming frames to a size that will match the display pixel aspect ratio
 * (from 320x240 to 320x180 in this case). Note that you might have to escape
 * some characters for your shell like '\(fraction\)'.
 * |[
 * gst-launch -v videotestsrc ! xvimagesink hue=100 saturation=-100 brightness=100
 * ]| Demonstrates how to use the colorbalance interface.
 * </refsect2>
 */

/* for developers: there are two useful tools : xvinfo and xvattr */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define __USE_GNU
#include <sched.h>

#include "linux/videodev2.h"

/* Our interfaces */
#include <gst/interfaces/navigation.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/interfaces/colorbalance.h>
#include <gst/interfaces/propertyprobe.h>
/* Helper functions */
#include <gst/video/video.h>
#include <resolution_util.h>

/* Object header */
#include "xvimagesink.h"

#ifdef GST_EXT_XV_ENHANCEMENT
/* Samsung extension headers */
/* For xv extension header for buffer transfer (output) */
#include "xv_types.h"

/* headers for drm */
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <X11/Xmd.h>
//#include <dri2/dri2.h>
//#include <libdrm/drm.h>
#include <drm/sdp_drm.h>

#include <audio-session-manager.h>

#include <avoc.h>
#include <avoc_avsink.h>
typedef enum {
	BUF_SHARE_METHOD_PADDR = 0,
	BUF_SHARE_METHOD_FD
} buf_share_method_t;

#ifndef GST_TIME_FORMAT5
#define GST_TIME_FORMAT5 "u:%02u:%02u.%03u"
#endif

#ifndef GST_TIME_ARGS5
#define GST_TIME_ARGS5(t) \
        GST_CLOCK_TIME_IS_VALID (t) ? \
        (guint) (((GstClockTime)(t)) / (GST_SECOND * 60 * 60)) : 99, \
        GST_CLOCK_TIME_IS_VALID (t) ? \
        (guint) ((((GstClockTime)(t)) / (GST_SECOND * 60)) % 60) : 99, \
        GST_CLOCK_TIME_IS_VALID (t) ? \
        (guint) ((((GstClockTime)(t)) / GST_SECOND) % 60) : 99, \
        GST_CLOCK_TIME_IS_VALID (t) ? \
        (guint) (((((GstClockTime)(t)) % GST_SECOND))/GST_MSECOND) : 999
#endif

#define MIN_LATENCY 5000
#define ENABLE_MASK(x, y) ( (x) = ((x) | (y)) )
#define DISABLE_MASK(x, y) ( (x) = ((x) & (~(y))) )
#define FIND_MASK(x, y) ( (((x) & (y)) == (y)) ? true : false)

#define PSIZE_RESET 0
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

typedef struct
{
	/* width of each image plane */
	int      w[SCMN_IMGB_MAX_PLANE];
	/* height of each image plane */
	int      h[SCMN_IMGB_MAX_PLANE];
	/* stride of each image plane */
	int      s[SCMN_IMGB_MAX_PLANE];
	/* elevation of each image plane */
	int      e[SCMN_IMGB_MAX_PLANE];
	/* user space address of each image plane */
	void   * a[SCMN_IMGB_MAX_PLANE];
	/* physical address of each image plane, if needs */
	void   * p[SCMN_IMGB_MAX_PLANE];
	/* color space type of image */
	int      cs;
	/* left postion, if needs */
	int      x;
	/* top position, if needs */
	int      y;
	/* to align memory */
	int      __dummy2;
	/* arbitrary data */
	int      data[16];
	/* dma buf fd */
	int dmabuf_fd[SCMN_IMGB_MAX_PLANE];
	/* buffer share method */
	int buf_share_method;
} SCMN_IMGB;

#endif /* GST_EXT_XV_ENHANCEMENT */

typedef struct
{
        int x;
        int y;
        int width;
        int height;
} ResRect_t;

static unsigned long long get_time()
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return ((ts.tv_sec * GST_SECOND + ts.tv_nsec * GST_NSECOND)/1000);
}

#if ENABLE_PERFORMANCE_CHECKING

static int64_t pre_time = 0;
static int64_t get_time_diff(void)
{
	int64_t now_time, time_diff;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    now_time = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
	if (pre_time == 0)
		pre_time = now_time;
	time_diff = now_time - pre_time;
	pre_time = now_time;
	return time_diff;
}
static int show_frame_count = 0;
static int xvimage_put_count = 0;
static int tbm_aptmp_count = 0;
static unsigned long long total_show_frame_time = 0;
static unsigned long long total_xvimage_put_time = 0;
static unsigned long long total_tbm_aptmp_time = 0;
unsigned long long show_frame_time_tmp = 0;
unsigned long long xvimage_put_time_tmp = 0;
unsigned long long tbm_aptmp_time_tmp = 0;
static int rotate_frame_count = 0;
static unsigned long long rotate_hw_into_AP_time = 0;
static unsigned long long rotate_AP_inot_MP_time = 0;
unsigned long long rotate_time_tmp = 0;
static unsigned long long rotate_total_time = 0;
unsigned long long rotate_total_time_tmp = 0;
#endif

static const gint DefaultDeviceId = -1;

/* Thread color conversion defines */
#define CONV_THREADS 4
#define CONV_ALIGNMENT 16

#define USE_TBM
#ifdef USE_TBM
#include <tbm_bufmgr.h>
#ifdef USE_TBM_FOXP
//#include <foxp/tbm_bufmgr_foxp.h>
#define TBM_BO_FOXP_DP_MEM (1<<16)
#define DP_MEM_FLAGS_SFT 20
#define DP_FB_Y (2<< DP_MEM_FLAGS_SFT)
#define DP_FB_C (3<< DP_MEM_FLAGS_SFT)
#define DP_FB_MAIN (0x0<<3)
#define DP_FB_SUB (0x1<<3)
#define DP_FB_IDX(idx) ((idx) << 24)
#endif

// #define USE_AVOC_DAEMON

static unsigned int get_dp_fb_type(GstXvImageSink *xvimagesink)
{
  return ((xvimagesink->scaler_id == 0)?DP_FB_MAIN:DP_FB_SUB);
}

//#define DUMP_HW_DATA
//#define DUMP_ROTATE_HW_DATA
//#define DUMP_ROTATE_DATA
//#define DUMP_HW_DISCONTINUS_DATA

#define ENABLE_MID_BUFFER		/// For soc virual ptr get not continuous data,  cache miss problem(90, 270)

#ifdef 	ENABLE_MID_BUFFER
unsigned char *Y_buffer = NULL;
unsigned char *UV_buffer = NULL;
#endif

#define FHD_DISPLAY_PANEL_W	1920
#define FHD_DISPLAY_PANEL_H	1080
#define HD_DISPLAY_PANEL_W	1280
#define HD_DISPLAY_PANEL_H		720


#ifdef GST_EXT_XV_ENHANCEMENT
#define XV_WEBKIT_PIXMAP_SUPPORT
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/extensions/Xdamage.h>
#include <dri2.h>
//#include <xf86drm.h>
#include "xv_pixmap_utils.h"
#endif

typedef struct
{
	int drm_fd;
	tbm_bufmgr bufmgr;
	tbm_bo boY, boCbCr;
	tbm_bo_handle bo_hnd_Y, bo_hnd_CbCr;
	unsigned char *pY, *pCbCr;
	gint idx;
#ifdef XV_WEBKIT_PIXMAP_SUPPORT
	tbm_bo boPixmap;
	tbm_bo_handle bo_hnd_Pixmap;
	unsigned char *pScaledY, *pScaledCbCr;
	XserverRegion region;
#endif
} TBMinfo, *TBMinfoPTR;
#endif

typedef struct {
	gushort display_primaries_x[3];
	gushort display_primaries_y[3];
	gushort white_point_x;
	gushort white_point_y;
	guint max_display_mastering_luminance;
	guint min_display_mastering_luminance;
} VIDEO_MASTERING_DISPLAY_COLOUR_VOLUME;

/* Debugging category */
#include <gst/gstinfo.h>

#include "gst/glib-compat-private.h"

GST_DEBUG_CATEGORY_STATIC (gst_debug_xvimagesink);
#define GST_CAT_DEFAULT gst_debug_xvimagesink
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

#ifdef GST_EXT_XV_ENHANCEMENT
#define GST_TYPE_XVIMAGESINK_DISPLAY_MODE (gst_xvimagesink_display_mode_get_type())

static GType
gst_xvimagesink_display_mode_get_type(void)
{
	static GType xvimagesink_display_mode_type = 0;
	static const GEnumValue display_mode_type[] = {
		{ 0, "Default mode", "DEFAULT"},
		{ 1, "Primary video ON and Secondary video FULL SCREEN mode", "PRI_VIDEO_ON_AND_SEC_VIDEO_FULL_SCREEN"},
		{ 2, "Primary video OFF and Secondary video FULL SCREEN mode", "PRI_VIDEO_OFF_AND_SEC_VIDEO_FULL_SCREEN"},
		{ 3, NULL, NULL},
	};

	if (!xvimagesink_display_mode_type) {
		xvimagesink_display_mode_type = g_enum_register_static("GstXVImageSinkDisplayModeType", display_mode_type);
	}

	return xvimagesink_display_mode_type;
}

enum {
    DEGREE_0,
    DEGREE_90,
    DEGREE_180,
    DEGREE_270,
    DEGREE_NUM,
};

#define GST_TYPE_XVIMAGESINK_ROTATE_ANGLE (gst_xvimagesink_rotate_angle_get_type())

static GType
gst_xvimagesink_rotate_angle_get_type(void)
{
	static GType xvimagesink_rotate_angle_type = 0;
	static const GEnumValue rotate_angle_type[] = {
		{ 0, "No rotate", "DEGREE_0"},
		{ 1, "Rotate 90 degree", "DEGREE_90"},
		{ 2, "Rotate 180 degree", "DEGREE_180"},
		{ 3, "Rotate 270 degree", "DEGREE_270"},
		{ 4, NULL, NULL},
	};

	if (!xvimagesink_rotate_angle_type) {
		xvimagesink_rotate_angle_type = g_enum_register_static("GstXVImageSinkRotateAngleType", rotate_angle_type);
	}

	return xvimagesink_rotate_angle_type;
}

enum {
    DISP_GEO_METHOD_LETTER_BOX = 0,
    DISP_GEO_METHOD_ORIGIN_SIZE,
    DISP_GEO_METHOD_FULL_SCREEN,
    DISP_GEO_METHOD_CROPPED_FULL_SCREEN,
    DISP_GET_METHOD_ZOOM_HALF,
    DISP_GET_METHOD_ZOOM_THREE_QUARTERS,
    DISP_GEO_METHOD_ORIGIN_SIZE_OR_LETTER_BOX,
    DISP_GEO_METHOD_CUSTOM_ROI,
    DISP_GET_METHOD_ZOOM_16X9,
    DISP_GET_METHOD_ZOOM,
    DISP_GET_METHOD_CUSTOM,
    DISP_GET_METHOD_ZOOM_NETFLIX_16X9,
    DISP_GET_METHOD_ZOOM_NETFLIX_4X3,
    DISP_GET_METHOD_DPS,
    DISP_GEO_METHOD_NUM,
};
#define DEF_DISPLAY_GEOMETRY_METHOD DISP_GEO_METHOD_LETTER_BOX

enum {
    FLIP_NONE = 0,
    FLIP_HORIZONTAL,
    FLIP_VERTICAL,
    FLIP_BOTH,
    FLIP_NUM,
};
#define DEF_DISPLAY_FLIP            FLIP_NONE

#define GST_TYPE_XVIMAGESINK_DISPLAY_GEOMETRY_METHOD (gst_xvimagesink_display_geometry_method_get_type())
#define GST_TYPE_XVIMAGESINK_FLIP                    (gst_xvimagesink_flip_get_type())

static GType
gst_xvimagesink_display_geometry_method_get_type(void)
{
	static GType xvimagesink_display_geometry_method_type = 0;
	static const GEnumValue display_geometry_method_type[] = {
		{ 0, "Letter box", "LETTER_BOX"},
		{ 1, "Origin size", "ORIGIN_SIZE"},
		{ 2, "Full-screen", "FULL_SCREEN"},
		{ 3, "Cropped full-screen", "CROPPED_FULL_SCREEN"},
		{ 4, "Zoom-half", "ZOOM_HALF"},
		{ 5, "Zoom_Three_Quaters", "ZOOM_THREE_QUARTERS"},
		{ 6, "Origin size(if screen size is larger than video size(width/height)) or Letter box(if video size(width/height) is larger than screen size)", "ORIGIN_SIZE_OR_LETTER_BOX"},
		{ 7, "Explicitly described destination ROI", "CUSTOM_ROI"},
		{ 8, "ZOOM_16x9", "ZOOM_16x9"},
		{ 9, "ZOOM_W", "ZOOM_W"},
		{ 10, "ZOOM_CUSTOM", "ZOOM_CUSTOM"},
		{ 11, "NETFLIX 16x9", "NETFLIX DISPLAY AS 16x9 RATIO, NEED USR TO SET PAR PROPERTY[display-netflix-par-x, display-netflix-par-y]"},
		{ 12, "NETFLIX 4x3", "NETFLIX DISPLAY AS 4x3 RATIO, NEED USR TO SET PAR PROPERTY[display-netflix-par-x, display-netflix-par-y]"},
		{ 13, "DIVX","DIVX DISPLAY, NEED USR TO SET PROPERTY [display-dps-width,display-dps-height]"}, 
		{ 14, NULL, NULL},
	};

	if (!xvimagesink_display_geometry_method_type) {
		xvimagesink_display_geometry_method_type = g_enum_register_static("GstXVImageSinkDisplayGeometryMethodType", display_geometry_method_type);
	}

	return xvimagesink_display_geometry_method_type;
}

static GType
gst_xvimagesink_flip_get_type(void)
{
	static GType xvimagesink_flip_type = 0;
	static const GEnumValue flip_type[] = {
		{ FLIP_NONE,       "Flip NONE", "FLIP_NONE"},
		{ FLIP_HORIZONTAL, "Flip HORIZONTAL", "FLIP_HORIZONTAL"},
		{ FLIP_VERTICAL,   "Flip VERTICAL", "FLIP_VERTICAL"},
		{ FLIP_BOTH,       "Flip BOTH", "FLIP_BOTH"},
		{ FLIP_NUM, NULL, NULL},
	};

	if (!xvimagesink_flip_type) {
		xvimagesink_flip_type = g_enum_register_static("GstXVImageSinkFlipType", flip_type);
	}

	return xvimagesink_flip_type;
}

#define g_marshal_value_peek_pointer(v)  (v)->data[0].v_pointer
void
gst_xvimagesink_BOOLEAN__POINTER (GClosure         *closure,
                                     GValue         *return_value G_GNUC_UNUSED,
                                     guint          n_param_values,
                                     const GValue   *param_values,
                                     gpointer       invocation_hint G_GNUC_UNUSED,
                                     gpointer       marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__POINTER) (gpointer     data1,
                                                     gpointer     arg_1,
                                                     gpointer     data2);
  register GMarshalFunc_BOOLEAN__POINTER callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  gboolean v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure)) {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
  } else {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
  }
  callback = (GMarshalFunc_BOOLEAN__POINTER) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                      g_marshal_value_peek_pointer (param_values + 1),
                      data2);

  g_value_set_boolean (return_value, v_return);
}

enum
{
    SIGNAL_FRAME_RENDER_ERROR,
    SIGNAL_SEIMETADATA_CHANGED,
    LAST_SIGNAL
};
static guint gst_xvimagesink_signals[LAST_SIGNAL] = { 0 };

#endif /* GST_EXT_XV_ENHANCEMENT */

typedef struct
{
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long input_mode;
  unsigned long status;
}
MotifWmHints, MwmHints;

#define MWM_HINTS_DECORATIONS   (1L << 1)

static void gst_xvimagesink_reset (GstXvImageSink * xvimagesink);
static void gst_xvimagesink_free_outside_buf (GstXvImageSink * xvimagesink);

static GstBufferClass *xvimage_buffer_parent_class = NULL;
static void gst_xvimage_buffer_finalize (GstXvImageBuffer * xvimage);

static void gst_xvimagesink_xwindow_update_geometry (GstXvImageSink *
    xvimagesink);
static gint gst_xvimagesink_get_format_from_caps (GstXvImageSink * xvimagesink,
    GstCaps * caps);
static void gst_xvimagesink_expose (GstXOverlay * overlay);

static void gst_xvimagesink_avoc_stop(GstXvImageSink *xvimagesink);
static gboolean gst_xvimagesink_avoc_set_resolution(GstXvImageSink *xvimagesink, GstCaps *newcaps, gboolean async, gboolean seamless_on, gboolean is_pre_cb);
static gboolean gst_xvimagesink_set_xv_port_attribute(GstXvImageSink *xvimagesink, const gchar* attribute_name, gint value);
static avoc_video_data_format_e gst_xvimagesink_set_videocodec_info(GstXvImageSink * xvimagesink, GstStructure* structure);
#ifndef USE_TBM_SDK
static gboolean gst_xvimagesink_avoc_set_attribute(GstXvImageSink * xvimagesink); //For HDR setting
#endif

#ifdef GST_EXT_XV_ENHANCEMENT
static XImage *make_transparent_image(Display *d, Window win, int w, int h);
static gboolean set_display_mode(GstXContext *xcontext, int set_mode);
//static gboolean set_input_mode(GstXContext *xcontext, guint set_mode);
static void drm_close_gem(GstXvImageSink *xvimagesink, unsigned int *gem_handle);
static void gst_xvimagesink_set_pixmap_handle (GstXOverlay * overlay, guintptr id);
#endif /* GST_EXT_XV_ENHANCEMENT */

#ifdef XV_WEBKIT_PIXMAP_SUPPORT
static gboolean check_x_extension (GstXvImageSink * xvimagesink);
static tbm_bo get_pixmap_bo (GstXvImageSink * xvimagesink, TBMinfoPTR tbmptr);
static XserverRegion get_pixmap_region (GstXvImageSink * xvimagesink);
static GstCaps *get_pixmap_support (GstXvImageSink * xvimagesink, GstXContext * xcontext);
static gboolean prepare_pixmap_tbm_buffers(GstXvImageBuffer * xvimage);
static void unprepare_pixmap_tbm_buffers(GstXvImageBuffer * xvimage);
static tbm_bo get_decoded_frame_bo(GstXvImageBuffer * xvimage, GstBuffer * buf);
static gboolean do_sw_tbm_scaling(GstXvImageBuffer * xvimage, tbm_bo ap_bo);
static gboolean do_hw_tbm_scaling(GstXvImageBuffer * xvimage, GstBuffer *buf);
static gboolean do_colorspace_conversion (GstXvImageBuffer * xvimage);
#endif/* XV_WEBKIT_PIXMAP_SUPPORT */

/* Default template - initiated with class struct to allow gst-register to work
   without X running */
static GstStaticPadTemplate gst_xvimagesink_sink_template_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-rgb, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], "
        "height = (int) [ 1, MAX ]; "
        "video/x-raw-yuv, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

enum MUTE_STATE
{
  UNMUTE_STATE = 0x00,	// Do not use directly
  MUTE_DEFAULT = 0x01,  // Mute By previous resource user,  and it'll be unmute after first putimage.
  MUTE_RESOLUTION_CHANGE = 0x02,  // mute the period ( from avoc_set_resolution ~ to next putimage )
  MUTE_ROTATION_CHANGE = 0x04,
  MUTE_FLIP_MODE_CHANGE = 0x08,
  MUTE_VISIBLE = 0x10,
  MUTE_VIDEO_QUALITY_SET = 0x20, // mute the period ( from avoc_set_resolution ~ to the async_set_done if it is async mode  avoc_set_resolution. )
  MUTE_STATE_HIDE = 0x40, //  About Visibility in xwindow mapped state. (The Visibility is meaningful only in mapped state.)
  MUTE_STATE_RT_CHANGE = 0x80,
  MUTE_INVALID_WINSIZE = 0x100,
  MUTE_UNMAPPED = 0x200, // xwindow show : mapped,   hide : unmapped	
};

enum VIDEO_QUALITY_MODE  // Do not change the given values, because element owner cannot use this enum value.
{
	VQ_MODE_DEFAULT = 0x00,
	VQ_MODE_ASYNCHRONOUS_SET = 0x01,
	VQ_MODE_PC_MODE = 0x02,
	VQ_MODE_GAME_MODE = 0x04,
	VQ_MODE_EXTERNAL_AVOC_SET = 0x08,  // ScreenMirroring use this.  (avoc_set_color_space_info, avoc_set_resolution) would be called by application.
	// Add here.
	VQ_MODE_VIDEO_PACK = 0x1000,
	VQ_MODE_MAX,
};

enum
{
  PROP_0,
  PROP_CONTRAST,
  PROP_BRIGHTNESS,
  PROP_HUE,
  PROP_SATURATION,
  PROP_DISPLAY,
  PROP_SYNCHRONOUS,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_FORCE_ASPECT_RATIO,
  PROP_HANDLE_EVENTS,
  PROP_DEVICE,
  PROP_DEVICE_NAME,
  PROP_DEVICE_ATTR_SCALER,
  PROP_HANDLE_EXPOSE,
  PROP_DOUBLE_BUFFER,
  PROP_AUTOPAINT_COLORKEY,
  PROP_COLORKEY,
  PROP_DRAW_BORDERS,
  PROP_WINDOW_WIDTH,
  PROP_WINDOW_HEIGHT,
  PROP_DISPLAY_SRC_X,/*video cropping*/
  PROP_DISPLAY_SRC_Y,
  PROP_DISPLAY_SRC_W,
  PROP_DISPLAY_SRC_H, 
  PROP_DISPLAY_SRC_X_RATIO,
  PROP_DISPLAY_SRC_Y_RATIO,
  PROP_DISPLAY_SRC_W_RATIO,
  PROP_DISPLAY_SRC_H_RATIO, 
  PROP_DISPLAY_ZOOM_Y, /*ZOOM*/
  PROP_DISPLAY_ZOOM_H,
  PROP_DISPLAY_CUSTOM_X, /*CUSTOM*/
  PROP_DISPLAY_CUSTOM_Y,
  PROP_DISPLAY_CUSTOM_W,
  PROP_DISPLAY_CUSTOM_H,
  PROP_DISPLAY_NTEFLIX_PAR_X,
  PROP_DISPLAY_NTEFLIX_PAR_Y,
  PROP_DISPLAY_UHD_FIT,
#ifdef GST_EXT_XV_ENHANCEMENT
  PROP_ORIENTATION,
  PROP_DISPLAY_MODE,
  PROP_ROTATE_ANGLE,
  PROP_SUPPORT_ROTATION,
  PROP_ENABEL_ROTATION,
  PROP_FLIP,
  PROP_DISPLAY_GEOMETRY_METHOD,
  PROP_VISIBLE,
  PROP_ZOOM,
  PROP_DST_ROI_X,
  PROP_DST_ROI_Y,
  PROP_DST_ROI_W,
  PROP_DST_ROI_H,
  PROP_STOP_VIDEO,
  PROP_PIXMAP_CB,
  PROP_PIXMAP_CB_USER_DATA,
#endif /* GST_EXT_XV_ENHANCEMENT */
#if ENABLE_RT_DISPLAY
  PROP_ES_DISPLAY,
#endif
  PROP_SEAMLESS_RESOLUTION_CHANGE,
  PROP_VIDEO_QUALITY_MODE,
  PROP_VIDEO_AVOC_SOURCE,
  PROP_DEVICE_ID,
  PROP_3D_MODE,
  PROP_DISPLAY_DPS_WIDTH,
  PROP_DISPLAY_DPS_HEIGHT,
  PROP_CAN_SUPPORT_HW_ROTATE,
  PROP_HW_ROTATE_DEGREE,
  PROP_ENABLE_HW_ROTATION,
  PROP_MIXER_ROTATE_DEGREE,
  PROP_MIXER_POSITION,
};

static void gst_xvimagesink_init_interfaces (GType type);

GST_BOILERPLATE_FULL (GstXvImageSink, gst_xvimagesink, GstVideoSink,
    GST_TYPE_VIDEO_SINK, gst_xvimagesink_init_interfaces);

static void gst_video_disp_adjust(GstXvImageSink * xvimagesink );  /*video cropping*/
static void gst_FitDimension(GstXvImageSink * xvimagesink,int SrcWidth, int SrcHeight, int * DstX, int * DstY, int * DstWidth, int * DstHeight);

static void gst_xvimagesink_xwindow_clear (GstXvImageSink * xvimagesink, GstXWindow * xwindow);
static gboolean gst_xvimagesink_xvimage_put (GstXvImageSink * xvimagesink, GstXvImageBuffer * xvimage, GstCaps* caps, int debug_info);


/* ============================================================= */
/*                                                               */
/*                       Private Methods                         */
/*                                                               */
/* ============================================================= */

/* xvimage buffers */

#define GST_TYPE_XVIMAGE_BUFFER (gst_xvimage_buffer_get_type())

#define GST_IS_XVIMAGE_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_XVIMAGE_BUFFER))
#define GST_XVIMAGE_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_XVIMAGE_BUFFER, GstXvImageBuffer))
#define GST_XVIMAGE_BUFFER_CAST(obj) ((GstXvImageBuffer *)(obj))


#define HW_DP_LINESIZE	1920		// FHD: 1920, UHD: 3840
gboolean set_display_dp_linesize(GstXvImageSink *xvimagesink)
{
	gint val = 0;
	gint i = 0, count = 0;
	Atom atom_dp = None;
	if(xvimagesink->xcontext){
		GstXContext* xcontext = xvimagesink->xcontext;
		XvAttribute *const attr = XvQueryPortAttributes(xcontext->disp, xcontext->xv_port_id, &count);
		if (attr) {
		  for (i = 0 ; i < count ; i++) {
			if (!strcmp(attr[i].name, "_USER_WM_PORT_ATTRIBUTE_Y_FB_BYTEPERLINE")) {
				atom_dp= XInternAtom(xcontext->disp, "_USER_WM_PORT_ATTRIBUTE_Y_FB_BYTEPERLINE", FALSE);
				XvGetPortAttribute (xcontext->disp, xcontext->xv_port_id, atom_dp, &val);
				xvimagesink->dp_linesize = val;
				break;
			}
		  }
		  XFree(attr);
		} else {
		  GST_WARNING_OBJECT(xvimagesink, "DP: Failed XvQueryPortAttributes -> disp:%d, port_id:%d ", xcontext->disp, xcontext->xv_port_id);
		}
	}

	if(xvimagesink->dp_linesize == 0){
		xvimagesink->dp_linesize = HW_DP_LINESIZE;
		GST_WARNING_OBJECT(xvimagesink, "DP: force to set display linesize[%d] !!!", xvimagesink->dp_linesize);
	}else {
		GST_INFO_OBJECT(xvimagesink, "DP: set video display linesize[%d] !!!", xvimagesink->dp_linesize);
	}
	
	return TRUE;
}

gboolean init_tbm_bufmgr(gint *pFD, tbm_bufmgr *pMgr, Display* pDisp)
{
	if (pFD && pMgr && pDisp)
	{
		char *driverName = NULL;
		char *deviceName = NULL;
		if (*pFD != -1 || *pMgr)
		{
			GST_ERROR("Alread opened *pFD[%d], *pMgr[0x%x]", *pFD, *pMgr);
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


void deinit_tbm_bufmgr(gint *pFD, tbm_bufmgr *pMgr)
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

#ifdef USE_TBM_SDK
gboolean allocate_tbm_buffer_sdk(GstXvImageSink *xvimagesink)
{
#ifdef USE_TBM
  int m, n;
  if (xvimagesink &&xvimagesink->xcontext && xvimagesink->xcontext->disp &&
  	(xvimagesink->p_sdk_TBMinfo[0] == NULL))
  {
  	for(m = 0; m < DISPLAY_BUFFER_NUM; m++){
		xvimagesink->p_sdk_TBMinfo[m] = g_malloc (sizeof(TBMinfo));
		if(!(xvimagesink->p_sdk_TBMinfo[m])){
			GST_ERROR_OBJECT(xvimagesink, "allocate_tbm_buffer_sdk : TBMinfo buffer [%d] allcate failed !!!", m);
			return FALSE;
		}
		memset(xvimagesink->p_sdk_TBMinfo[m], 0, sizeof(TBMinfo));
	}
	if (xvimagesink)
	{
		TBMinfoPTR tbmptr[DISPLAY_BUFFER_NUM];
		for(n = 0; n < DISPLAY_BUFFER_NUM; n++){
			tbmptr[n] = (TBMinfoPTR)(xvimagesink->p_sdk_TBMinfo[n]);
		}
		tbmptr[0]->drm_fd = -1;
		if (init_tbm_bufmgr(&tbmptr[0]->drm_fd, &tbmptr[0]->bufmgr, xvimagesink->xcontext->disp)) {
			for(n = 1; n < DISPLAY_BUFFER_NUM; n++){
				memcpy(tbmptr[n], tbmptr[0], sizeof(TBMinfo));
			}
			int i=0; 
#if 0
			guint w = xvimagesink->video_width;
			guint h = xvimagesink->video_height;
			GST_ERROR_OBJECT(xvimagesink, "Allocate TBM buffer size %dx%d", w, h);
#else
			guint w = 1920;
			guint h = 1080;
			GST_ERROR_OBJECT(xvimagesink, "Allocate TBM buffer size 1920x1080 !!!");
#endif
			for(i=0; i<DISPLAY_BUFFER_NUM; i++)
			{
#ifdef USE_TBM_FOXP
          			tbmptr[i]->boY = tbm_bo_alloc(tbmptr[i]->bufmgr, w*h, TBM_BO_FOXP_DP_MEM | DP_FB_Y | DP_FB_IDX(((get_dp_fb_type(xvimagesink)) | i)));
#else
          			tbmptr[i]->boY = tbm_bo_alloc(tbmptr[i]->bufmgr, w*h, TBM_BO_DEFAULT);
#endif
          			if (!(tbmptr[i]->boY)) {
            				GST_ERROR_OBJECT(xvimagesink, "tbmptr[%d]->boY is NULL", i);
            				goto FAIL_TO_ALLOC_TBM;
          			}
				tbmptr[i]->bo_hnd_Y = tbm_bo_map(tbmptr[i]->boY, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
				if (!(tbmptr[i]->bo_hnd_Y.ptr)) {
					GST_ERROR_OBJECT(xvimagesink, "tbmptr[%d]->bo_hnd_Y.ptr is NULL", i);
					goto FAIL_TO_ALLOC_TBM;
				}
#ifdef USE_TBM_FOXP
          			tbmptr[i]->boCbCr = tbm_bo_alloc(tbmptr[i]->bufmgr, w*h/2, TBM_BO_FOXP_DP_MEM | DP_FB_C | DP_FB_IDX(((get_dp_fb_type(xvimagesink)) | i)));
#else
          			tbmptr[i]->boCbCr = tbm_bo_alloc(tbmptr[i]->bufmgr, w*h/2, TBM_BO_DEFAULT);
#endif
          			if (!(tbmptr[i]->boCbCr)) {
            				GST_ERROR_OBJECT(xvimagesink, "tbmptr->boCbCr[%d] is NULL", i);
            				goto FAIL_TO_ALLOC_TBM;
          			}
          			tbmptr[i]->bo_hnd_CbCr = tbm_bo_map(tbmptr[i]->boCbCr, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
          			if (!(tbmptr[i]->bo_hnd_CbCr.ptr)) {
            				GST_ERROR_OBJECT(xvimagesink, "tbmptr->bo_hnd_CbCr.ptr is NULL");
            				goto FAIL_TO_ALLOC_TBM;
          			}
          			tbmptr[i]->idx = i;
          			tbmptr[i]->pY = tbmptr[i]->bo_hnd_Y.ptr;
          			tbmptr[i]->pCbCr = tbmptr[i]->bo_hnd_CbCr.ptr;

          			memset(tbmptr[i]->pY, 0, w*h);
          			memset(tbmptr[i]->pCbCr, 0, w*h/2);
				GST_DEBUG_OBJECT(xvimagesink, "Succeeded to allocate the DP[%d]: idx[ %d ] bufmgr[ %p ], size[ %d x %d ] , Y[ bo:%p, vaddr:%p, size:%d ] C[ bo:%p, vaddr:%p, size:%d ]",
					i, tbmptr[i]->idx, tbmptr[i]->bufmgr, w,h,tbmptr[i]->boY, tbmptr[i]->pY, tbm_bo_size(tbmptr[i]->boY), tbmptr[i]->boCbCr, tbmptr[i]->pCbCr, tbm_bo_size(tbmptr[i]->boCbCr));

				continue;
FAIL_TO_ALLOC_TBM:
          			GST_WARNING_OBJECT(xvimagesink, "Failed to allocate DP memoery idx[ %d ] !!!", i);
				int j = 0;
				for(j=0; j<DISPLAY_BUFFER_NUM; j++){
	          			if (tbmptr[j]->bo_hnd_Y.ptr)  tbm_bo_unmap(tbmptr[j]->boY);
	          			if (tbmptr[j]->boY)  tbm_bo_unref(tbmptr[j]->boY);
	          			if (tbmptr[j]->bo_hnd_CbCr.ptr)  tbm_bo_unmap(tbmptr[j]->boCbCr);
	          			if (tbmptr[j]->boCbCr)  tbm_bo_unref(tbmptr[j]->boCbCr);
					if(j == DISPLAY_BUFFER_NUM -1)
						deinit_tbm_bufmgr(&tbmptr[j]->drm_fd, &tbmptr[j]->bufmgr);
					memset(xvimagesink->p_sdk_TBMinfo[j], 0, sizeof(TBMinfo));
				}
				GST_ERROR_OBJECT(xvimagesink, "can not find available index ");
          			return FALSE;
        		}
		}else{
        		GST_ERROR_OBJECT(xvimagesink, "Failed to init_tbm_bufmgr !!!");
			return FALSE;
		}
	}else {
		GST_ERROR_OBJECT(xvimagesink, "NO xvimagesink[ %p ]", xvimagesink);
		return FALSE;
	}
  }else {
  	GST_WARNING("xvimagesink is NULL?? or allocate already done ~ ");
	return FALSE;
  }
#endif
  return TRUE;
}

gboolean free_tbm_buffer_sdk(GstXvImageSink *xvimagesink)
{
  GST_LOG_OBJECT(xvimagesink, "Free tbm buffer Called");
#ifdef USE_TBM
  int i;
  if (xvimagesink && (xvimagesink->p_sdk_TBMinfo[0]))
  {
  	TBMinfoPTR tbmptr[DISPLAY_BUFFER_NUM];
	for(i = 0; i < DISPLAY_BUFFER_NUM; i++){
		tbmptr[i] = (TBMinfoPTR)(xvimagesink->p_sdk_TBMinfo[i]);
		if(tbmptr[i]){			
			if (tbmptr[i]->bo_hnd_Y.ptr)  tbm_bo_unmap(tbmptr[i]->boY);
			if (tbmptr[i]->boY)  tbm_bo_unref(tbmptr[i]->boY);
			if (tbmptr[i]->bo_hnd_CbCr.ptr)  tbm_bo_unmap(tbmptr[i]->boCbCr);
			if (tbmptr[i]->boCbCr)	tbm_bo_unref(tbmptr[i]->boCbCr);

			//Only deinit once
			if(i == DISPLAY_BUFFER_NUM-1){
				deinit_tbm_bufmgr(&tbmptr[i]->drm_fd, &tbmptr[i]->bufmgr);
			}
		}
	}
	for(i = 0; i < DISPLAY_BUFFER_NUM; i++){
		g_free (xvimagesink->p_sdk_TBMinfo[i]);
		xvimagesink->p_sdk_TBMinfo[i] = NULL;
	}
	xvimagesink->sdk_dsp_idx = 0;
	GST_DEBUG_OBJECT(xvimagesink, "Free tbm buffer finished");
	return TRUE;
  }
#endif
  return FALSE;
}
#else
gboolean allocate_tbm_buffer(GstXvImageBuffer * xvimage)
{
#ifdef USE_TBM
  int m, n;
  if (xvimage && xvimage->xvimagesink && xvimage->xvimagesink->xcontext && xvimage->xvimagesink->xcontext->disp &&
  	(xvimage->pTBMinfo[0] == NULL))
  {
  	for(m = 0; m < DISPLAY_BUFFER_NUM; m++){
		xvimage->pTBMinfo[m] = g_malloc (sizeof(TBMinfo));
		if(!(xvimage->pTBMinfo[m])){
			GST_ERROR_OBJECT(xvimage->xvimagesink, "allocate_tbm_buffer : TBMinfo buffer [%d] allcate failed !!!", m);
			return FALSE;
		}
		memset(xvimage->pTBMinfo[m], 0, sizeof(TBMinfo));
	}
	if (xvimage->xvimagesink)
	{
		TBMinfoPTR tbmptr[DISPLAY_BUFFER_NUM];
		for(n = 0; n < DISPLAY_BUFFER_NUM; n++){
			tbmptr[n] = (TBMinfoPTR)(xvimage->pTBMinfo[n]);
		}
		tbmptr[0]->drm_fd = -1;
		if (init_tbm_bufmgr(&tbmptr[0]->drm_fd, &tbmptr[0]->bufmgr, xvimage->xvimagesink->xcontext->disp)) {
			for(n = 1; n < DISPLAY_BUFFER_NUM; n++){
				memcpy(tbmptr[n], tbmptr[0], sizeof(TBMinfo));
			}
			int i=0; 
#if 1
			guint w = xvimage->xvimagesink->video_width;
			guint h = xvimage->xvimagesink->video_height;
			GST_ERROR_OBJECT(xvimage->xvimagesink, "Allocate TBM buffer size %dx%d", w, h);
#else
			guint w = 1920;
			guint h = 1080;
			GST_ERROR_OBJECT(xvimage->xvimagesink, "Allocate TBM buffer size 1920x1080 !!!");
#endif
			for(i=0; i<DISPLAY_BUFFER_NUM; i++)
			{
#ifdef USE_TBM_FOXP
          			tbmptr[i]->boY = tbm_bo_alloc(tbmptr[i]->bufmgr, w*h, TBM_BO_FOXP_DP_MEM | DP_FB_Y | DP_FB_IDX(((get_dp_fb_type(xvimage->xvimagesink)) | i)));
#else
          			tbmptr[i]->boY = tbm_bo_alloc(tbmptr[i]->bufmgr, w*h, TBM_BO_DEFAULT);
#endif
          			if (!(tbmptr[i]->boY)) {
            				GST_ERROR_OBJECT(xvimage->xvimagesink, "tbmptr[%d]->boY is NULL", i);
            				goto FAIL_TO_ALLOC_TBM;
          			}
				tbmptr[i]->bo_hnd_Y = tbm_bo_map(tbmptr[i]->boY, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
				if (!(tbmptr[i]->bo_hnd_Y.ptr)) {
					GST_ERROR_OBJECT(xvimage->xvimagesink, "tbmptr[%d]->bo_hnd_Y.ptr is NULL", i);
					goto FAIL_TO_ALLOC_TBM;
				}
#ifdef USE_TBM_FOXP
          			tbmptr[i]->boCbCr = tbm_bo_alloc(tbmptr[i]->bufmgr, w*h/2, TBM_BO_FOXP_DP_MEM | DP_FB_C | DP_FB_IDX(((get_dp_fb_type(xvimage->xvimagesink)) | i)));
#else
          			tbmptr[i]->boCbCr = tbm_bo_alloc(tbmptr[i]->bufmgr, w*h/2, TBM_BO_DEFAULT);
#endif
          			if (!(tbmptr[i]->boCbCr)) {
            				GST_ERROR_OBJECT(xvimage->xvimagesink, "tbmptr->boCbCr[%d] is NULL", i);
            				goto FAIL_TO_ALLOC_TBM;
          			}
          			tbmptr[i]->bo_hnd_CbCr = tbm_bo_map(tbmptr[i]->boCbCr, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
          			if (!(tbmptr[i]->bo_hnd_CbCr.ptr)) {
            				GST_ERROR_OBJECT(xvimage->xvimagesink, "tbmptr->bo_hnd_CbCr.ptr is NULL");
            				goto FAIL_TO_ALLOC_TBM;
          			}
          			tbmptr[i]->idx = i;
          			tbmptr[i]->pY = tbmptr[i]->bo_hnd_Y.ptr;
          			tbmptr[i]->pCbCr = tbmptr[i]->bo_hnd_CbCr.ptr;

          			memset(tbmptr[i]->pY, 0, w*h);
          			memset(tbmptr[i]->pCbCr, 0, w*h/2);
				GST_DEBUG_OBJECT(xvimage->xvimagesink, "Succeeded to allocate the DP[%d]: idx[ %d ] bufmgr[ %p ], size[ %d x %d ] , Y[ bo:%p, vaddr:%p, size:%d ] C[ bo:%p, vaddr:%p, size:%d ]",
					i, tbmptr[i]->idx, tbmptr[i]->bufmgr, w,h,tbmptr[i]->boY, tbmptr[i]->pY, tbm_bo_size(tbmptr[i]->boY), tbmptr[i]->boCbCr, tbmptr[i]->pCbCr, tbm_bo_size(tbmptr[i]->boCbCr));

				continue;
FAIL_TO_ALLOC_TBM:
          			GST_WARNING_OBJECT(xvimage->xvimagesink, "Failed to allocate DP memoery idx[ %d ] !!!", i);
				int j = 0;
				for(j=0; j<DISPLAY_BUFFER_NUM; j++){
	          			if (tbmptr[j]->bo_hnd_Y.ptr)  tbm_bo_unmap(tbmptr[j]->boY);
	          			if (tbmptr[j]->boY)  tbm_bo_unref(tbmptr[j]->boY);
	          			if (tbmptr[j]->bo_hnd_CbCr.ptr)  tbm_bo_unmap(tbmptr[j]->boCbCr);
	          			if (tbmptr[j]->boCbCr)  tbm_bo_unref(tbmptr[j]->boCbCr);
					if(j == DISPLAY_BUFFER_NUM -1)
						deinit_tbm_bufmgr(&tbmptr[j]->drm_fd, &tbmptr[j]->bufmgr);
					memset(xvimage->pTBMinfo[j], 0, sizeof(TBMinfo));
				}
				GST_ERROR_OBJECT(xvimage->xvimagesink, "can not find available index ");
          			return FALSE;
        		}
		}else{
        		GST_ERROR_OBJECT(xvimage->xvimagesink, "Failed to init_tbm_bufmgr !!!");
			return FALSE;
		}
	}else {
		GST_ERROR_OBJECT(xvimage->xvimagesink, "NO xvimagesink[ %p ]", xvimage->xvimagesink);
		return FALSE;
	}
  }else {
  	GST_WARNING("xvimage is NULL?? or..");
	return FALSE;
  }
#endif
  return TRUE;
}

gboolean free_tbm_buffer(GstXvImageBuffer * xvimage)
{
  GST_LOG_OBJECT(xvimage->xvimagesink, "Free tbm buffer Called");
#ifdef USE_TBM
  int i;
  if (xvimage && (xvimage->pTBMinfo[0]))
  {
  	TBMinfoPTR tbmptr[DISPLAY_BUFFER_NUM];
	for(i = 0; i < DISPLAY_BUFFER_NUM; i++){
		tbmptr[i] = (TBMinfoPTR)(xvimage->pTBMinfo[i]);
		if(tbmptr[i]){			
			if (tbmptr[i]->bo_hnd_Y.ptr)  tbm_bo_unmap(tbmptr[i]->boY);
			if (tbmptr[i]->boY)  tbm_bo_unref(tbmptr[i]->boY);
			if (tbmptr[i]->bo_hnd_CbCr.ptr)  tbm_bo_unmap(tbmptr[i]->boCbCr);
			if (tbmptr[i]->boCbCr)	tbm_bo_unref(tbmptr[i]->boCbCr);

			//Only deinit once
			if(i == DISPLAY_BUFFER_NUM-1){
				deinit_tbm_bufmgr(&tbmptr[i]->drm_fd, &tbmptr[i]->bufmgr);
			}
		}
	}
	for(i = 0; i < DISPLAY_BUFFER_NUM; i++){
		g_free (xvimage->pTBMinfo[i]);
		xvimage->pTBMinfo[i] = NULL;
	}
	xvimage->dsp_idx = 0;
	GST_DEBUG_OBJECT(xvimage->xvimagesink, "Free tbm buffer finished");
	return TRUE;
  }
#endif
  return FALSE;
}
#endif	//USE_TBM_SDK

#if ENABLE_VF_ROTATE
static 
gboolean set_rotate_video_display_channel(GstXvImageSink *xvimagesink, gint currdpychannel, gint butype)
{
  gint i = 0, count = 0;
  gint val = 0;
  if(xvimagesink->xcontext){
	if(butype != currdpychannel){
		GstXContext* xcontext = xvimagesink->xcontext;
		XvAttribute *const attr = XvQueryPortAttributes(xcontext->disp, xcontext->xv_port_id, &count);
		if (attr) {
			for (i = 0 ; i < count ; i++) {
				if (!strcmp(attr[i].name, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE")) {
					Atom atom_butype = XInternAtom(xcontext->disp, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE", FALSE);
					gint ret = XvSetPortAttribute(xcontext->disp, xcontext->xv_port_id, atom_butype, butype); // 0:bypass hw decoded frame(default)  1:index of MP framebuffer.
					if (ret != Success){
						GST_ERROR_OBJECT(xvimagesink, "Rotate: Failed	_USER_WM_PORT_ATTRIBUTE_BUFTYPE[index %d] -> found ret[ %d ], xvimage[ %p ]", i, ret, xvimagesink->xvimage);
						return FALSE;
					}else{
						GST_DEBUG_OBJECT(xvimagesink, "Rotate: Success _USER_WM_PORT_ATTRIBUTE_BUFTYPE[index %d] -> xvimage[ %p ]", i, xvimagesink->xvimage);
						xvimagesink->vf_display_channel = butype;
						/*  If need butype setting sync, 
						while(1){  //To keep set mute operate in Xserver is sync
							XvGetPortAttribute (xcontext->disp, xcontext->xv_port_id, atom_butype, &val);
							if(val & 0x01){
								break;
							}
							g_usleep (G_USEC_PER_SEC /100);
							GST_INFO_OBJECT(xvimagesink, "Rotate: waiting to get _USER_WM_PORT_ATTRIBUTE_BUFTYPE");
						}
						*/
						XSync (xvimagesink->xcontext->disp, FALSE);
						GST_INFO_OBJECT(xvimagesink, "Rotate: Set video display channel[%d] success !!!", xvimagesink->vf_display_channel);
					}
					break;
				}
			}
			XFree(attr);
		} else {
			GST_ERROR_OBJECT(xvimagesink, "Rotate: Failed XvQueryPortAttributes disp:%d, port_id:%d ", xcontext->disp, xcontext->xv_port_id);
			return FALSE;
		}
	}
  }
  
  return TRUE;
}

static gboolean gst_xvimagesink_set_xv_port_attribute(GstXvImageSink *xvimagesink, const gchar* attribute_name, gint value)
{
	gint i = 0, count = 0;
	Atom atom = None;
	gboolean result = FALSE;
	g_return_val_if_fail(xvimagesink, FALSE);
	g_return_val_if_fail(xvimagesink->xcontext, FALSE);
	g_return_val_if_fail(xvimagesink->xcontext->xv_port_id != 0, FALSE);
	g_return_val_if_fail(xvimagesink->xcontext->disp != NULL, FALSE);

	GstXContext* xcontext = xvimagesink->xcontext;
	XvAttribute *const attr = XvQueryPortAttributes(xcontext->disp, xcontext->xv_port_id, &count);
	g_return_val_if_fail(attr, FALSE);

	/* Find index of this attribute */
	for (i = 0 ; i < count ; i++) {
		if (!strcmp(attr[i].name, attribute_name)) {
			atom= XInternAtom(xcontext->disp, attribute_name, FALSE);
			break;
		}
	}

	if (atom == None)
	{
		GST_ERROR_OBJECT(xvimagesink, "can not find  [ %s ] attribute", attribute_name);
		goto FINISH;
	}

	/* Set given value to this attribute */
	guint64 T1 = get_time();
	gint ret = XvSetPortAttribute(xcontext->disp, xcontext->xv_port_id, atom, value);
	guint64 T2 = get_time();
	if (ret != Success){
		GST_ERROR_OBJECT(xvimagesink, "FAIL to set attr[ %d : %s ] value[ %d ] ret[ %d ], xvimage[ %p ]", i, attribute_name, value, ret, xvimagesink->xvimage);
	}else{
		GST_DEBUG_OBJECT(xvimagesink, "SUCCESS, attr[%d.%s] value[ %d ] ret[ %d ], xvimage[ %p ]", i, attribute_name, value, ret, xvimagesink->xvimage);
		result = TRUE;
	} 
	XSync (xvimagesink->xcontext->disp, FALSE);
	guint64 T3 = get_time();
	if (T3-T1 > MIN_LATENCY)
		GST_ERROR_OBJECT(xvimagesink,"XvSetPortAttribute[ %lld ms ], XSync[ %lld ms ]  makes a latency for video rendering", (T2-T1)/1000, (T3-T2)/1000);

FINISH:
	XFree(attr);
	return result;
}


static 
gboolean mute_video_display(GstXvImageSink *xvimagesink, gboolean bMuteOn, guint mask)
{
#if 0
	GST_ERROR_OBJECT (xvimagesink, "Mute Test Always On!");
	gst_xvimagesink_set_xv_port_attribute(xvimagesink, "_USER_WM_PORT_ATTRIBUTE_MUTE", 1);
	return FALSE;
#endif

	gboolean ret = FALSE;
	guint prev_mute_flag = xvimagesink->mute_flag;


	if (bMuteOn)
		ENABLE_MASK(xvimagesink->mute_flag, mask);
	else
		DISABLE_MASK(xvimagesink->mute_flag, mask);

	if (bMuteOn == FALSE && prev_mute_flag != UNMUTE_STATE &&  xvimagesink->mute_flag == UNMUTE_STATE)
	{
		// MUTE -> UNMUTE
		ret = gst_xvimagesink_set_xv_port_attribute(xvimagesink, "_USER_WM_PORT_ATTRIBUTE_MUTE", (gint)bMuteOn);
		GST_ERROR_OBJECT (xvimagesink, "NO-ERROR! JUST.. VIDEO UNMUTE, result[ %s ], mask[ %04x ], flag[ %04x -> %04x ]", ret?"SUCCESS":"FAIL", mask, prev_mute_flag, xvimagesink->mute_flag);
	}
	else if (bMuteOn == TRUE && prev_mute_flag == UNMUTE_STATE &&  xvimagesink->mute_flag != UNMUTE_STATE)
	{
		// UNMUTE -> MUTE
		ret = gst_xvimagesink_set_xv_port_attribute(xvimagesink, "_USER_WM_PORT_ATTRIBUTE_MUTE", (gint)bMuteOn);
		GST_ERROR_OBJECT (xvimagesink, "NO-ERROR! JUST.. VIDEO MUTE, result[ %s ], mask[ %04x ], flag[ %04x -> %04x ]", ret?"SUCCESS":"FAIL", mask, prev_mute_flag, xvimagesink->mute_flag);
	}
	else
		GST_ERROR_OBJECT(xvimagesink, "bMuteOn[ %d ], mask[ %04x ], flag[ %04x -> %04x ]", bMuteOn, mask, prev_mute_flag, xvimagesink->mute_flag );
	return ret;
}

gboolean pre_adjust_rotate_index(GstXvImageSink *xvimagesink){
	if(xvimagesink){
		if(PRE_BUFFER_NUM == 1){
			xvimagesink->vf_pre_allocate_done = 0;  // rotate degree changed,  so free & allocate again.
			xvimagesink->vf_degree_idx = 0;
		}else {
			if(xvimagesink->vf_rotate_degree == 90 || xvimagesink->vf_rotate_degree == 270){		  //only for the PRE_BUFFER_NUM = 2
				xvimagesink->vf_degree_idx = 0;
			}else if(xvimagesink->vf_rotate_degree == 180 || xvimagesink->vf_rotate_degree == 0){
				xvimagesink->vf_degree_idx = 1;
			}else {
				xvimagesink->vf_degree_idx = 1;
			}
		}
		GST_DEBUG_OBJECT(xvimagesink,"Rotate: set vf_rotate_degree = %d  !!!", xvimagesink->vf_degree_idx);
	}else {
		GST_ERROR("Xvimagesink is NULL, or ...");
		return FALSE;
	}

	return TRUE;
}

gboolean pre_adjust_display_index(GstXvImageSink *xvimagesink){
  if(xvimagesink){
	xvimagesink->vf_pre_display_idx_n++;
	if(xvimagesink->vf_pre_display_idx_n >= PRE_DISPLAY_BUFFER_NUM)
		xvimagesink->vf_pre_display_idx_n = 0;
	xvimagesink->vf_need_update_display_idx = 0;
	GST_DEBUG_OBJECT(xvimagesink,"Rotate: set vf_pre_display_idx_n = %d  update[%d]!!!", xvimagesink->vf_pre_display_idx_n, xvimagesink->vf_need_update_display_idx);
  }else {
	GST_ERROR("Xvimagesink is NULL, or ...");
	return FALSE;
  }
  return TRUE;
}

gboolean pre_allocate_rotate_tbm_display_buffer(GstXvImageSink *xvimagesink)
{
  GST_DEBUG_OBJECT (xvimagesink, "Rotate: pre allocate tbm display buffer begin");
  gint m, n, j;
  guint w, h;
  gint i = 0;
  gint degree;

  for(m = 0; m < PRE_DISPLAY_BUFFER_NUM; m++){
	  for(n = 0; n < PRE_BUFFER_NUM; n++){
	    memset (&xvimagesink->vf_pre_display_bo_hnd_Y[m][n], 0x0, sizeof(tbm_bo_handle));
	    memset (&xvimagesink->vf_pre_display_bo_hnd_CbCr[m][n], 0x0, sizeof(tbm_bo_handle));
	  }
  }

  if(xvimagesink->frctx->drm_fd == -1){
    if(!init_tbm_bufmgr(&xvimagesink->vf_display_drm_fd, &xvimagesink->vf_display_bufmgr, xvimagesink->xcontext->disp)){
      GST_ERROR_OBJECT (xvimagesink, "Rotate: pre init tbm bufmgr failed!");
      return FALSE;
    }
  }else{
    xvimagesink->vf_display_drm_fd = xvimagesink->frctx->drm_fd;
    xvimagesink->vf_display_bufmgr = tbm_bufmgr_init(xvimagesink->vf_display_drm_fd);
    if(!xvimagesink->vf_display_bufmgr){
      GST_ERROR_OBJECT (xvimagesink, "Rotate: pre init tbm bufmgr(fd from rotation module) failed!");
      return FALSE;
    }
  }
  if(xvimagesink->vf_display_drm_fd != -1)
 {
 	for(m = 0; m < PRE_DISPLAY_BUFFER_NUM; m++){
		for(n=0; n<PRE_BUFFER_NUM; n++){
			// caculate the rotation degree
		  if(PRE_BUFFER_NUM == 1){
			w= xvimagesink->vf_iScaledWidth;
			h = xvimagesink->vf_iScaledHeight;
			GST_DEBUG_OBJECT(xvimagesink, "Rotate: -> w/h [%d]/[%d] !!!", w, h);
		  }else {
			if(xvimagesink->frctx){
				if(n == 0){
					degree = 90;		// 90 or 270
				}else if(n == 1){
					degree = 180;		// 0 or 180
				}else {
					degree = 0;
				}
				videoframe_rotate_set_degree(xvimagesink->frctx, degree);
				w = videoframe_rotate_get_scaled_width(xvimagesink->frctx);
				h = videoframe_rotate_get_scaled_height(xvimagesink->frctx);
				GST_DEBUG_OBJECT(xvimagesink, "Rotate: degree[%d] -> w/h [%d]/[%d] ", degree, w, h);
			}
		  }
#ifdef USE_TBM_FOXP
			xvimagesink->vf_pre_display_boY[m][n] = tbm_bo_alloc(xvimagesink->vf_display_bufmgr, w*h, TBM_BO_FOXP_DP_MEM | DP_FB_Y | DP_FB_IDX(((get_dp_fb_type(xvimagesink)) | (m*PRE_BUFFER_NUM + n))));
#else
			xvimagesink->vf_pre_display_boY[m][n] = tbm_bo_alloc(xvimagesink->vf_display_bufmgr, w*h, TBM_BO_DEFAULT);
#endif
			if (!xvimagesink->vf_pre_display_boY[m][n]) {
			       GST_ERROR_OBJECT(xvimagesink, "Rotate: pre allocate tbm display Y buffer failed");
				goto FAIL_TO_PRE_ALLOC_TBM;
			}
			xvimagesink->vf_pre_display_bo_hnd_Y[m][n] = tbm_bo_map(xvimagesink->vf_pre_display_boY[m][n], TBM_DEVICE_CPU, TBM_OPTION_WRITE);
			if (!xvimagesink->vf_pre_display_bo_hnd_Y[m][n].ptr) {
			    GST_ERROR_OBJECT(xvimagesink, "Rotate: pre map tbm display Y buffer failed");
			    goto FAIL_TO_PRE_ALLOC_TBM;
			}
#ifdef USE_TBM_FOXP
		       xvimagesink->vf_pre_display_boCbCr[m][n] = tbm_bo_alloc(xvimagesink->vf_display_bufmgr, w*h/2, TBM_BO_FOXP_DP_MEM | DP_FB_C | DP_FB_IDX(((get_dp_fb_type(xvimagesink)) | (m*PRE_BUFFER_NUM + n))));
#else
		       xvimagesink->vf_pre_display_boCbCr[m][n] = tbm_bo_alloc(xvimagesink->vf_display_bufmgr, w*h/2, TBM_BO_DEFAULT);
#endif
			if (!xvimagesink->vf_pre_display_boCbCr[m][n]) {
			       GST_ERROR_OBJECT(xvimagesink, "Rotate: pre allocate tbm display CbCr buffer failed");
				goto FAIL_TO_PRE_ALLOC_TBM;
			}
			xvimagesink->vf_pre_display_bo_hnd_CbCr[m][n] = tbm_bo_map(xvimagesink->vf_pre_display_boCbCr[m][n], TBM_DEVICE_CPU, TBM_OPTION_WRITE);
			if (!xvimagesink->vf_pre_display_bo_hnd_CbCr[m][n].ptr) {
			      GST_ERROR_OBJECT(xvimagesink, "Rotate: pre map tbm display CbCr buffer failed");
				goto FAIL_TO_PRE_ALLOC_TBM;
			}
			xvimagesink->vf_pre_display_idx[m][n] = m*PRE_BUFFER_NUM + n;
			memset(xvimagesink->vf_pre_display_bo_hnd_Y[m][n].ptr, 0, w*h);
			memset(xvimagesink->vf_pre_display_bo_hnd_CbCr[m][n].ptr, 0, w*h/2);
			GST_DEBUG_OBJECT(xvimagesink, "Rotate:  Succeed to pre allocate display buffer %d -> idx[ %d ] bufmgr[ %p ], size[ %d x %d ] , Y[ bo:%p, vaddr:%p, size:%d ] C[ bo:%p, vaddr:%p, size:%d ]",
			       n, xvimagesink->vf_pre_display_idx[m][n], xvimagesink->vf_display_bufmgr, w,h,xvimagesink->vf_pre_display_boY[m][n], xvimagesink->vf_pre_display_bo_hnd_Y[m][n].ptr, tbm_bo_size(xvimagesink->vf_pre_display_boY[m][n]), xvimagesink->vf_pre_display_boCbCr[m][n], xvimagesink->vf_pre_display_bo_hnd_CbCr[m][n].ptr, tbm_bo_size(xvimagesink->vf_pre_display_boCbCr[m][n]));

			continue;

FAIL_TO_PRE_ALLOC_TBM:
			GST_WARNING_OBJECT(xvimagesink, "Rotate: Failed to pre allocate display buffer -> idx[ %d ], try next idx", i);
			for(i=0; i<PRE_DISPLAY_BUFFER_NUM; i++){
				for(j=0; j<PRE_BUFFER_NUM; j++){
					if (xvimagesink->vf_pre_display_bo_hnd_Y[i][j].ptr)  tbm_bo_unmap(xvimagesink->vf_pre_display_boY[i][j]);
					if (xvimagesink->vf_pre_display_boY[i][j])  tbm_bo_unref(xvimagesink->vf_pre_display_boY[i][j]);
					if (xvimagesink->vf_pre_display_bo_hnd_CbCr[i][j].ptr)  tbm_bo_unmap(xvimagesink->vf_pre_display_boCbCr[i][j]);
					if (xvimagesink->vf_pre_display_boCbCr[i][j])  tbm_bo_unref(xvimagesink->vf_pre_display_boCbCr[i][j]);
				}
			}
			GST_ERROR_OBJECT(xvimagesink, "Rotate: Failed to pre allocate display buffer done !!!");
			return FALSE;
		}
  	}
  }else {
	GST_ERROR_OBJECT(xvimagesink, "Rotate:pre init tbm bufmgr failed -> drm_fd[ %d ], err[ %d ]", xvimagesink->vf_display_drm_fd, errno);
	return FALSE;
  }
  xvimagesink->vf_pre_allocate_done = 1;
  return TRUE;
}

gboolean pre_free_rotate_tbm_display_buffer(GstXvImageSink *xvimagesink){
  gint m, n;
  if(xvimagesink){
    for(m = 0; m < PRE_DISPLAY_BUFFER_NUM; m++){
	    for(n = 0; n < PRE_BUFFER_NUM; n++){
		    if(xvimagesink->vf_pre_display_bo_hnd_Y[m][n].ptr){
		      tbm_bo_unmap(xvimagesink->vf_pre_display_boY[m][n]);
		      GST_DEBUG_OBJECT(xvimagesink, "Rotate: %d %d pre unmap tbm display Y buffer success", m, n);
		    }
		    if(xvimagesink->vf_pre_display_boY[m][n]){
		      tbm_bo_unref(xvimagesink->vf_pre_display_boY[m][n]);
		      xvimagesink->vf_pre_display_boY[m][n] = NULL;
		      GST_DEBUG_OBJECT(xvimagesink, "Rotate: %d %d pre unref tbm display Y buffer success", m, n);
		    }
		    if(xvimagesink->vf_pre_display_bo_hnd_CbCr[m][n].ptr){
		      tbm_bo_unmap(xvimagesink->vf_pre_display_boCbCr[m][n]);
		      GST_DEBUG_OBJECT(xvimagesink, "Rotate: %d %d pre unmap tbm display CbCr buffer success", m, n);
		    }
		    if(xvimagesink->vf_pre_display_boCbCr[m][n]){
		      tbm_bo_unref(xvimagesink->vf_pre_display_boCbCr[m][n]);
		      xvimagesink->vf_pre_display_boCbCr[m][n] = NULL;
		      GST_DEBUG_OBJECT(xvimagesink, "Rotate: %d %d pre unref tbm display CbCr buffer success", m, n);
		    }
		    memset (&xvimagesink->vf_pre_display_boY[m][n], 0x0, sizeof(tbm_bo));
		    memset (&xvimagesink->vf_pre_display_boCbCr[m][n], 0x0, sizeof(tbm_bo));
		    memset (&xvimagesink->vf_pre_display_bo_hnd_Y[m][n], 0x0, sizeof(tbm_bo_handle));
		    memset (&xvimagesink->vf_pre_display_bo_hnd_CbCr[m][n], 0x0, sizeof(tbm_bo_handle));
		    GST_DEBUG_OBJECT(xvimagesink, "Rotate: pre free tbm display buffer success!");
	      }
      }
	
	if(xvimagesink->frctx->drm_fd == -1){
	  deinit_tbm_bufmgr(&xvimagesink->vf_display_drm_fd, &xvimagesink->vf_display_bufmgr);
	  GST_DEBUG_OBJECT(xvimagesink, "Rotate: pre deinit tbm display bufmgr success");
	}else {
	  if (xvimagesink->vf_display_bufmgr){
		tbm_bufmgr_deinit(xvimagesink->vf_display_bufmgr);
		xvimagesink->vf_display_bufmgr = NULL;
		xvimagesink->vf_display_drm_fd = -1;
		GST_DEBUG_OBJECT(xvimagesink, "Rotate: pre deinit tbm display bufmgr(from the rotation module) success");
	  }
	}
	
	xvimagesink->vf_pre_allocate_done = 0;
  }
  return TRUE;
}
#endif	//ENABLE_VF_ROTATE

/* This function destroys a GstXvImage handling XShm availability */
static void
gst_xvimage_buffer_destroy (GstXvImageBuffer * xvimage)
{
  GstXvImageSink *xvimagesink;

  GST_DEBUG_OBJECT (xvimage, "Destroying buffer");

  xvimagesink = xvimage->xvimagesink;
  if (G_UNLIKELY (xvimagesink == NULL))
    goto no_sink;

  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  GST_OBJECT_LOCK (xvimagesink);

  /* If the destroyed image is the current one we destroy our reference too */
  if (xvimagesink->cur_image == xvimage)
    xvimagesink->cur_image = NULL;

  /* We might have some buffers destroyed after changing state to NULL */
  if (xvimagesink->xcontext == NULL) {
    GST_DEBUG_OBJECT (xvimagesink, "Destroying XvImage after Xcontext");
#ifdef HAVE_XSHM
    /* Need to free the shared memory segment even if the x context
     * was already cleaned up */
    if (xvimage->SHMInfo.shmaddr != ((void *) -1)) {
      shmdt (xvimage->SHMInfo.shmaddr);
    }
#endif
    goto beach;
  }

  g_mutex_lock (xvimagesink->x_lock);

#ifdef HAVE_XSHM
  if (xvimagesink->xcontext->use_xshm) {
    if (xvimage->SHMInfo.shmaddr != ((void *) -1)) {
      GST_DEBUG_OBJECT (xvimagesink, "XServer ShmDetaching from 0x%x id 0x%lx",
          xvimage->SHMInfo.shmid, xvimage->SHMInfo.shmseg);
      XShmDetach (xvimagesink->xcontext->disp, &xvimage->SHMInfo);
      XSync (xvimagesink->xcontext->disp, FALSE);

      shmdt (xvimage->SHMInfo.shmaddr);
    }
    if (xvimage->xvimage)
      XFree (xvimage->xvimage);
  } else
#endif /* HAVE_XSHM */
  {
    if (xvimage->xvimage) {
      if (xvimage->xvimage->data) {
        g_free (xvimage->xvimage->data);
      }
      XFree (xvimage->xvimage);
    }
  }
#ifndef USE_TBM_SDK
  free_tbm_buffer(xvimage);
#endif
#ifdef XV_WEBKIT_PIXMAP_SUPPORT
  unprepare_pixmap_tbm_buffers(xvimage);
#endif
  XSync (xvimagesink->xcontext->disp, FALSE);

  g_mutex_unlock (xvimagesink->x_lock);

beach:
  GST_OBJECT_UNLOCK (xvimagesink);
  xvimage->xvimagesink = NULL;
  gst_object_unref (xvimagesink);

  GST_MINI_OBJECT_CLASS (xvimage_buffer_parent_class)->finalize (GST_MINI_OBJECT
      (xvimage));

  return;

no_sink:
  {
    GST_WARNING ("no sink found");
    return;
  }
}

static void
gst_xvimage_buffer_finalize (GstXvImageBuffer * xvimage)
{
  GstXvImageSink *xvimagesink;
  gboolean running;

  xvimagesink = xvimage->xvimagesink;
  if (G_UNLIKELY (xvimagesink == NULL))
    goto no_sink;

  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  GST_OBJECT_LOCK (xvimagesink);
  running = xvimagesink->running;
  GST_OBJECT_UNLOCK (xvimagesink);

  /* If our geometry changed we can't reuse that image. */
  if (running == FALSE) {
    GST_LOG_OBJECT (xvimage, "destroy image as sink is shutting down");
    gst_xvimage_buffer_destroy (xvimage);
  } else if ((xvimage->width != xvimagesink->video_width) ||
      (xvimage->height != xvimagesink->video_height)) {
    GST_LOG_OBJECT (xvimage,
        "destroy image as its size changed %dx%d vs current %dx%d",
        xvimage->width, xvimage->height,
        xvimagesink->video_width, xvimagesink->video_height);
    gst_xvimage_buffer_destroy (xvimage);
  } else {
    /* In that case we can reuse the image and add it to our image pool. */
    GST_LOG_OBJECT (xvimage, "recycling image in pool");
    /* need to increment the refcount again to recycle */
    gst_buffer_ref (GST_BUFFER_CAST (xvimage));
    g_mutex_lock (xvimagesink->pool_lock);
    xvimagesink->image_pool = g_slist_prepend (xvimagesink->image_pool,
        xvimage);
    g_mutex_unlock (xvimagesink->pool_lock);
  }
  return;

no_sink:
  {
    GST_WARNING ("no sink found");
    return;
  }
}

static void
gst_xvimage_buffer_free (GstXvImageBuffer * xvimage)
{
  /* make sure it is not recycled */
  xvimage->width = -1;
  xvimage->height = -1;
#ifndef USE_TBM_SDK
  free_tbm_buffer(xvimage);
#endif
  gst_buffer_unref (GST_BUFFER (xvimage));
}

static void
gst_xvimage_buffer_init (GstXvImageBuffer * xvimage, gpointer g_class)
{
  int i;
#ifdef HAVE_XSHM
  xvimage->SHMInfo.shmaddr = ((void *) -1);
  xvimage->SHMInfo.shmid = -1;
#endif
  for(i = 0; i < DISPLAY_BUFFER_NUM; i++){
  	xvimage->pTBMinfo[i] = NULL;
	 GST_LOG_OBJECT(xvimage->xvimagesink, "xvimage buffer init, reset pTBMinfo to NULL !!!");
  }
  xvimage->dsp_idx = 0;
  xvimage->pTBMinfo_web = NULL;
}

static void
gst_xvimage_buffer_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  xvimage_buffer_parent_class = g_type_class_peek_parent (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      gst_xvimage_buffer_finalize;
}

static GType
gst_xvimage_buffer_get_type (void)
{
  static GType _gst_xvimage_buffer_type;

  if (G_UNLIKELY (_gst_xvimage_buffer_type == 0)) {
    static const GTypeInfo xvimage_buffer_info = {
      sizeof (GstBufferClass),
      NULL,
      NULL,
      gst_xvimage_buffer_class_init,
      NULL,
      NULL,
      sizeof (GstXvImageBuffer),
      0,
      (GInstanceInitFunc) gst_xvimage_buffer_init,
      NULL
    };
    _gst_xvimage_buffer_type = g_type_register_static (GST_TYPE_BUFFER,
        "GstXvImageBuffer", &xvimage_buffer_info, 0);
  }
  return _gst_xvimage_buffer_type;
}

/* X11 stuff */

static gboolean error_caught = FALSE;

static int
gst_xvimagesink_handle_xerror (Display * display, XErrorEvent * xevent)
{
  char error_msg[1024];

  XGetErrorText (display, xevent->error_code, error_msg, 1024);
  GST_ERROR ("xvimagesink triggered an XError. error_msg: %s, xevent->error_code: %d, xevent->request_code: %d, xevent->minor_code: %d",
  	error_msg,xevent->error_code,xevent->request_code,xevent->minor_code);
  error_caught = TRUE;
  return 0;
}

#ifdef HAVE_XSHM
/* This function checks that it is actually really possible to create an image
   using XShm */
static gboolean
gst_xvimagesink_check_xshm_calls (GstXContext * xcontext)
{
  XvImage *xvimage;
  XShmSegmentInfo SHMInfo;
  gint size;
  int (*handler) (Display *, XErrorEvent *);
  gboolean result = FALSE;
  gboolean did_attach = FALSE;

  g_return_val_if_fail (xcontext != NULL, FALSE);

  /* Sync to ensure any older errors are already processed */
  XSync (xcontext->disp, FALSE);

  /* Set defaults so we don't free these later unnecessarily */
  SHMInfo.shmaddr = ((void *) -1);
  SHMInfo.shmid = -1;

  /* Setting an error handler to catch failure */
  error_caught = FALSE;
  handler = XSetErrorHandler (gst_xvimagesink_handle_xerror);

  /* Trying to create a 1x1 picture */
  xvimage = XvShmCreateImage (xcontext->disp, xcontext->xv_port_id,
      xcontext->im_format, NULL, 1, 1, &SHMInfo);

  /* Might cause an error, sync to ensure it is noticed */
  XSync (xcontext->disp, FALSE);
  if (!xvimage || error_caught) {
    GST_WARNING ("could not XvShmCreateImage a 1x1 image");
    goto beach;
  }
  size = xvimage->data_size;

  SHMInfo.shmid = shmget (IPC_PRIVATE, size, IPC_CREAT | 0777);
  if (SHMInfo.shmid == -1) {
    GST_WARNING ("could not get shared memory of %d bytes", size);
    goto beach;
  }

  SHMInfo.shmaddr = shmat (SHMInfo.shmid, NULL, 0);
  if (SHMInfo.shmaddr == ((void *) -1)) {
    GST_WARNING ("Failed to shmat: %s", g_strerror (errno));
    /* Clean up the shared memory segment */
    shmctl (SHMInfo.shmid, IPC_RMID, NULL);
    goto beach;
  }

  xvimage->data = SHMInfo.shmaddr;
  SHMInfo.readOnly = FALSE;

  if (XShmAttach (xcontext->disp, &SHMInfo) == 0) {
    GST_WARNING ("Failed to XShmAttach");
    /* Clean up the shared memory segment */
    shmctl (SHMInfo.shmid, IPC_RMID, NULL);
    goto beach;
  }

  /* Sync to ensure we see any errors we caused */
  XSync (xcontext->disp, FALSE);

  /* Delete the shared memory segment as soon as everyone is attached.
   * This way, it will be deleted as soon as we detach later, and not
   * leaked if we crash. */
  shmctl (SHMInfo.shmid, IPC_RMID, NULL);

  if (!error_caught) {
    GST_DEBUG ("XServer ShmAttached to 0x%x, id 0x%lx", SHMInfo.shmid,
        SHMInfo.shmseg);

    did_attach = TRUE;
    /* store whether we succeeded in result */
    result = TRUE;
  } else {
    GST_WARNING ("MIT-SHM extension check failed at XShmAttach. "
        "Not using shared memory.");
  }

beach:
  /* Sync to ensure we swallow any errors we caused and reset error_caught */
  XSync (xcontext->disp, FALSE);

  error_caught = FALSE;
  XSetErrorHandler (handler);

  if (did_attach) {
    GST_DEBUG ("XServer ShmDetaching from 0x%x id 0x%lx",
        SHMInfo.shmid, SHMInfo.shmseg);
    XShmDetach (xcontext->disp, &SHMInfo);
    XSync (xcontext->disp, FALSE);
  }
  if (SHMInfo.shmaddr != ((void *) -1))
    shmdt (SHMInfo.shmaddr);
  if (xvimage)
    XFree (xvimage);
  return result;
}
#endif /* HAVE_XSHM */

/* This function handles GstXvImage creation depending on XShm availability */
static GstXvImageBuffer *
gst_xvimagesink_xvimage_new (GstXvImageSink * xvimagesink, GstCaps * caps)
{
  GstXvImageBuffer *xvimage = NULL;
  GstStructure *structure = NULL;
  gboolean succeeded = FALSE;
  int (*handler) (Display *, XErrorEvent *);

  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), NULL);

  if (caps == NULL)
    return NULL;

  xvimage = (GstXvImageBuffer *) gst_mini_object_new (GST_TYPE_XVIMAGE_BUFFER);
  GST_DEBUG_OBJECT (xvimagesink, "Creating new XvImageBuffer");

  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "width", &xvimage->width) ||
      !gst_structure_get_int (structure, "height", &xvimage->height)) {
    GST_WARNING ("failed getting geometry from caps %" GST_PTR_FORMAT, caps);
  }

  GST_LOG_OBJECT (xvimagesink, "creating %dx%d", xvimage->width,
      xvimage->height);
#ifdef GST_EXT_XV_ENHANCEMENT
  GST_LOG_OBJECT(xvimagesink, "aligned size %dx%d",
                               xvimagesink->aligned_width, xvimagesink->aligned_height);
  if (xvimagesink->aligned_width == 0 || xvimagesink->aligned_height == 0) {
    GST_INFO_OBJECT(xvimagesink, "aligned size is zero. set size of caps.");
    xvimagesink->aligned_width = xvimage->width;
    xvimagesink->aligned_height = xvimage->height;
  }
#endif /* GST_EXT_XV_ENHANCEMENT */

  xvimage->im_format = gst_xvimagesink_get_format_from_caps (xvimagesink, caps);
  if (xvimage->im_format == -1) {
    GST_WARNING_OBJECT (xvimagesink, "failed to get format from caps %"
        GST_PTR_FORMAT, caps);
    GST_ELEMENT_ERROR (xvimagesink, RESOURCE, WRITE,
        ("Failed to create output image buffer of %dx%d pixels",
            xvimage->width, xvimage->height), ("Invalid input caps"));
    goto beach_unlocked;
  }
  xvimage->xvimagesink = gst_object_ref (xvimagesink);

#ifdef XV_WEBKIT_PIXMAP_SUPPORT
  if (xvimagesink->get_pixmap_cb &&
    (xvimage->im_format == GST_MAKE_FOURCC ('S', 'T', 'V', '1') || xvimage->im_format == GST_MAKE_FOURCC ('S', 'T', 'V', '0'))
    ) {
    if(!prepare_pixmap_tbm_buffers(xvimage)) {
      gst_xvimage_buffer_free (xvimage);
      xvimage = NULL;
    }
    /* since we do not really create a xvimage, we do not need release it as well */
#ifdef HAVE_XSHM
    if(xvimage){
      xvimage->SHMInfo.shmaddr = -1;
    }
#endif
    GST_DEBUG_OBJECT (xvimagesink, "for webkit pixmap support, we do not really create a complete xvimage!");
    return xvimage;
  }
#endif

  g_mutex_lock (xvimagesink->x_lock);

#ifdef GST_EXT_XV_ENHANCEMENT
  XSync (xvimagesink->xcontext->disp, FALSE);
#endif /* GST_EXT_XV_ENHANCEMENT */
  /* Setting an error handler to catch failure */
  error_caught = FALSE;
  handler = XSetErrorHandler (gst_xvimagesink_handle_xerror);

#ifdef HAVE_XSHM
  if (xvimagesink->xcontext->use_xshm) {
    int expected_size;

    xvimage->xvimage = XvShmCreateImage (xvimagesink->xcontext->disp,
        xvimagesink->xcontext->xv_port_id, xvimage->im_format, NULL,
#ifdef GST_EXT_XV_ENHANCEMENT
        xvimagesink->aligned_width, xvimagesink->aligned_height, &xvimage->SHMInfo);
#else /* GST_EXT_XV_ENHANCEMENT */
        xvimage->width, xvimage->height, &xvimage->SHMInfo);
#endif /* GST_EXT_XV_ENHANCEMENT */
    if (!xvimage->xvimage || error_caught) {
      g_mutex_unlock (xvimagesink->x_lock);

      /* Reset error flag */
      error_caught = FALSE;

      /* Push a warning */
      GST_ELEMENT_WARNING (xvimagesink, RESOURCE, WRITE,
          ("Failed to create output image buffer of %dx%d pixels",
              xvimage->width, xvimage->height),
          ("could not XvShmCreateImage a %dx%d image",
              xvimage->width, xvimage->height));

#ifdef GST_EXT_XV_ENHANCEMENT
      goto beach_unlocked;
#else /* GST_EXT_XV_ENHANCEMENT */
      /* Retry without XShm */
      xvimagesink->xcontext->use_xshm = FALSE;

      /* Hold X mutex again to try without XShm */
      g_mutex_lock (xvimagesink->x_lock);
      goto no_xshm;
#endif /* GST_EXT_XV_ENHANCEMENT */
    }

    /* we have to use the returned data_size for our shm size */
    xvimage->size = xvimage->xvimage->data_size;
    GST_LOG_OBJECT (xvimagesink, "XShm image size is %" G_GSIZE_FORMAT,
        xvimage->size);

    /* calculate the expected size.  This is only for sanity checking the
     * number we get from X. */
    switch (xvimage->im_format) {
      case GST_MAKE_FOURCC ('S', 'T', 'V', '1'):
#ifdef USE_TBM_SDK
	allocate_tbm_buffer_sdk(xvimage->xvimagesink);
#else
	allocate_tbm_buffer(xvimage);
#endif
      case GST_MAKE_FOURCC ('I', '4', '2', '0'):
      case GST_MAKE_FOURCC ('S', 'T', 'V', '0'):
      case GST_MAKE_FOURCC ('Y', 'V', '1', '2'):
      {
        gint pitches[3];
        gint offsets[3];
        guint plane;

        offsets[0] = 0;
        pitches[0] = GST_ROUND_UP_4 (xvimage->width);
        offsets[1] = offsets[0] + pitches[0] * GST_ROUND_UP_2 (xvimage->height);
        pitches[1] = GST_ROUND_UP_8 (xvimage->width) / 2;
        offsets[2] =
            offsets[1] + pitches[1] * GST_ROUND_UP_2 (xvimage->height) / 2;
        pitches[2] = GST_ROUND_UP_8 (pitches[0]) / 2;

        expected_size =
            offsets[2] + pitches[2] * GST_ROUND_UP_2 (xvimage->height) / 2;

        for (plane = 0; plane < xvimage->xvimage->num_planes; plane++) {
          GST_DEBUG_OBJECT (xvimagesink,
              "Plane %u has a expected pitch of %d bytes, " "offset of %d",
              plane, pitches[plane], offsets[plane]);
        }
        break;
      }
      case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
      case GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'):
        expected_size = xvimage->height * GST_ROUND_UP_4 (xvimage->width * 2);
        break;

#ifdef GST_EXT_XV_ENHANCEMENT
      case GST_MAKE_FOURCC ('S', 'T', '1', '2'):
      case GST_MAKE_FOURCC ('S', 'N', '1', '2'):
      case GST_MAKE_FOURCC ('S', 'N', '2', '1'):
      case GST_MAKE_FOURCC ('S', 'U', 'Y', 'V'):
      case GST_MAKE_FOURCC ('S', 'U', 'Y', '2'):
      case GST_MAKE_FOURCC ('S', '4', '2', '0'):
      case GST_MAKE_FOURCC ('S', 'Y', 'V', 'Y'):
        expected_size = sizeof(SCMN_IMGB);
        break;
#endif /* GST_EXT_XV_ENHANCEMENT */
      default:
        expected_size = 0;
        break;
    }
    if (expected_size != 0 && xvimage->size != expected_size) {
      GST_WARNING_OBJECT (xvimagesink,
          "unexpected XShm image size (got %" G_GSIZE_FORMAT ", expected %d)",
          xvimage->size, expected_size);
    }

    /* Be verbose about our XvImage stride */
    {
      guint plane;

      for (plane = 0; plane < xvimage->xvimage->num_planes; plane++) {
        GST_DEBUG_OBJECT (xvimagesink, "Plane %u has a pitch of %d bytes, "
            "offset of %d", plane, xvimage->xvimage->pitches[plane],
            xvimage->xvimage->offsets[plane]);
      }
    }

    xvimage->SHMInfo.shmid = shmget (IPC_PRIVATE, xvimage->size,
        IPC_CREAT | 0777);
    if (xvimage->SHMInfo.shmid == -1) {
      g_mutex_unlock (xvimagesink->x_lock);
      GST_ELEMENT_ERROR (xvimagesink, RESOURCE, WRITE,
          ("Failed to create output image buffer of %dx%d pixels",
              xvimage->width, xvimage->height),
          ("could not get shared memory of %" G_GSIZE_FORMAT " bytes",
              xvimage->size));
      goto beach_unlocked;
    }

    xvimage->SHMInfo.shmaddr = shmat (xvimage->SHMInfo.shmid, NULL, 0);
    if (xvimage->SHMInfo.shmaddr == ((void *) -1)) {
      g_mutex_unlock (xvimagesink->x_lock);
      GST_ELEMENT_ERROR (xvimagesink, RESOURCE, WRITE,
          ("Failed to create output image buffer of %dx%d pixels",
              xvimage->width, xvimage->height),
          ("Failed to shmat: %s", g_strerror (errno)));
      /* Clean up the shared memory segment */
      shmctl (xvimage->SHMInfo.shmid, IPC_RMID, NULL);
      goto beach_unlocked;
    }

    xvimage->xvimage->data = xvimage->SHMInfo.shmaddr;
    xvimage->SHMInfo.readOnly = FALSE;

    if (XShmAttach (xvimagesink->xcontext->disp, &xvimage->SHMInfo) == 0) {
      /* Clean up the shared memory segment */
      shmctl (xvimage->SHMInfo.shmid, IPC_RMID, NULL);

      g_mutex_unlock (xvimagesink->x_lock);
      GST_ELEMENT_ERROR (xvimagesink, RESOURCE, WRITE,
          ("Failed to create output image buffer of %dx%d pixels",
              xvimage->width, xvimage->height), ("Failed to XShmAttach"));
      goto beach_unlocked;
    }

    XSync (xvimagesink->xcontext->disp, FALSE);

    /* Delete the shared memory segment as soon as we everyone is attached.
     * This way, it will be deleted as soon as we detach later, and not
     * leaked if we crash. */
    shmctl (xvimage->SHMInfo.shmid, IPC_RMID, NULL);

    GST_DEBUG_OBJECT (xvimagesink, "XServer ShmAttached to 0x%x, id 0x%lx",
        xvimage->SHMInfo.shmid, xvimage->SHMInfo.shmseg);
  } else
  no_xshm:
#endif /* HAVE_XSHM */
  {
    xvimage->xvimage = XvCreateImage (xvimagesink->xcontext->disp,
        xvimagesink->xcontext->xv_port_id,
#ifdef GST_EXT_XV_ENHANCEMENT
        xvimage->im_format, NULL, xvimagesink->aligned_width, xvimagesink->aligned_height);
#else /* GST_EXT_XV_ENHANCEMENT */
        xvimage->im_format, NULL, xvimage->width, xvimage->height);
#endif /* GST_EXT_XV_ENHANCEMENT */
    if (!xvimage->xvimage || error_caught) {
      g_mutex_unlock (xvimagesink->x_lock);
      /* Reset error handler */
      error_caught = FALSE;
      XSetErrorHandler (handler);
      /* Push an error */
      GST_ELEMENT_ERROR (xvimagesink, RESOURCE, WRITE,
          ("Failed to create outputimage buffer of %dx%d pixels",
              xvimage->width, xvimage->height),
          ("could not XvCreateImage a %dx%d image",
              xvimage->width, xvimage->height));
      goto beach_unlocked;
    }

    /* we have to use the returned data_size for our image size */
    xvimage->size = xvimage->xvimage->data_size;
    xvimage->xvimage->data = g_malloc (xvimage->size);

    XSync (xvimagesink->xcontext->disp, FALSE);
  }
  /* Reset error handler */
  error_caught = FALSE;
  XSetErrorHandler (handler);

  succeeded = TRUE;

  GST_BUFFER_DATA (xvimage) = (guchar *) xvimage->xvimage->data;
  GST_BUFFER_SIZE (xvimage) = xvimage->size;

  g_mutex_unlock (xvimagesink->x_lock);

beach_unlocked:
  if (!succeeded) {
    gst_xvimage_buffer_free (xvimage);
    xvimage = NULL;
  }

  return xvimage;
}

/* We are called with the x_lock taken */
static void
gst_xvimagesink_xwindow_draw_borders (GstXvImageSink * xvimagesink,
    GstXWindow * xwindow, GstVideoRectangle rect)
{
  gint t1, t2;

  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));
  g_return_if_fail (xwindow != NULL);

  XSetForeground (xvimagesink->xcontext->disp, xwindow->gc,
      xvimagesink->xcontext->black);

  /* Left border */
  if (rect.x > xvimagesink->render_rect.x) {
    XFillRectangle (xvimagesink->xcontext->disp, xwindow->win, xwindow->gc,
        xvimagesink->render_rect.x, xvimagesink->render_rect.y,
        rect.x - xvimagesink->render_rect.x, xvimagesink->render_rect.h);
  }

  /* Right border */
  t1 = rect.x + rect.w;
  t2 = xvimagesink->render_rect.x + xvimagesink->render_rect.w;
  if (t1 < t2) {
    XFillRectangle (xvimagesink->xcontext->disp, xwindow->win, xwindow->gc,
        t1, xvimagesink->render_rect.y, t2 - t1, xvimagesink->render_rect.h);
  }

  /* Top border */
  if (rect.y > xvimagesink->render_rect.y) {
    XFillRectangle (xvimagesink->xcontext->disp, xwindow->win, xwindow->gc,
        xvimagesink->render_rect.x, xvimagesink->render_rect.y,
        xvimagesink->render_rect.w, rect.y - xvimagesink->render_rect.y);
  }

  /* Bottom border */
  t1 = rect.y + rect.h;
  t2 = xvimagesink->render_rect.y + xvimagesink->render_rect.h;
  if (t1 < t2) {
    XFillRectangle (xvimagesink->xcontext->disp, xwindow->win, xwindow->gc,
        xvimagesink->render_rect.x, t1, xvimagesink->render_rect.w, t2 - t1);
  }
}

/*video cropping*/
/*ajust the validity of xvimagesink->disp_x*/
static void
gst_video_disp_adjust(GstXvImageSink * xvimagesink )
{
  /*if disp_x is abnormal, set original x,y,width and heigth */
  if ( xvimagesink->disp_x < 0 || xvimagesink->disp_x > xvimagesink->video_width){
    xvimagesink->disp_x = 0;
  }
  if ( xvimagesink->disp_y < 0 || xvimagesink->disp_y > xvimagesink->video_height){
    xvimagesink->disp_y = 0;
  }
  if ( xvimagesink->disp_width > xvimagesink->video_width || xvimagesink->disp_width <= 0){
    xvimagesink->disp_width = xvimagesink->video_width;
  }
  if ( xvimagesink->disp_height > xvimagesink->video_height || xvimagesink->disp_height <= 0){
    xvimagesink->disp_height = xvimagesink->video_height;
  }
  GST_LOG_OBJECT(xvimagesink,
                 "xvimagesink->disp_x: %d,xvimagesink->disp_y : %d,xvimagesink->disp_width: %d, xvimagesink->disp_height: %d, video_width[%d], video_height[%d]",
                 xvimagesink->disp_x, xvimagesink->disp_y,
                 xvimagesink->disp_width, xvimagesink->disp_height,
                 xvimagesink->video_width, xvimagesink->video_height);
  return;
}
static void gst_getRectSize(guint display_geometry_method, GstXvImageSink * xvimagesink)
{
  
  if(((display_geometry_method != DISP_GET_METHOD_ZOOM_THREE_QUARTERS) 
  	&& (display_geometry_method < DISP_GET_METHOD_ZOOM_16X9))
  	|| display_geometry_method == DISP_GET_METHOD_ZOOM_NETFLIX_16X9
  	|| display_geometry_method == DISP_GET_METHOD_ZOOM_NETFLIX_4X3
  	|| display_geometry_method == DISP_GET_METHOD_DPS){
    GST_LOG_OBJECT(xvimagesink,"input display_geometry_method:%d,return", display_geometry_method);
    return;
  }
  
	ResRect_t pInputVideo;		// it's the orginal video src rect
	ResRect_t pInputVideo_tmp;	// it's the tmp video src rect for calculate the dst rect
	pInputVideo.x = 0;
	pInputVideo.y = 0;
	pInputVideo_tmp.x = 0;
	pInputVideo_tmp.y = 0;	
	
#if ENABLE_VF_ROTATE
	GstVideoRectangle src_input_tmp;
	src_input_tmp.x = xvimagesink->src_input.x = xvimagesink->disp_x;
	src_input_tmp.y = xvimagesink->src_input.y = xvimagesink->disp_y;
	src_input_tmp.w = xvimagesink->src_input.w = xvimagesink->vf_iScaledWidth;
	src_input_tmp.h = xvimagesink->src_input.h = xvimagesink->vf_iScaledHeight;

	pInputVideo.width = xvimagesink->vf_iScaledWidth;
	pInputVideo.height = xvimagesink->vf_iScaledHeight;

	if(xvimagesink->vf_rotate_degree == 90 ||xvimagesink->vf_rotate_degree == 270){
		pInputVideo_tmp.width = pInputVideo.width*xvimagesink->par_y;		// for contents self aspect ratio, default par_y is 1
		pInputVideo_tmp.height = pInputVideo.height*xvimagesink->par_x; 	// for contents self aspect ratio, default par_x is 1
	}else {
		pInputVideo_tmp.width = pInputVideo.width*xvimagesink->par_x;		// for contents self aspect ratio, default par_x is 1
		pInputVideo_tmp.height = pInputVideo.height*xvimagesink->par_y; 		// for contents self aspect ratio, default par_y is 1
	}
	
	if(pInputVideo.width == 4096 && pInputVideo.height == 2160){
		if(xvimagesink->is_uhd_fit){
			//Not corped the orginal content
			GST_ERROR_OBJECT(xvimagesink,"4K Fit On: pInputVideo_tmp [%d]/[%d]/[%d]/[%d]",pInputVideo_tmp.x, pInputVideo_tmp.y, pInputVideo_tmp.width, pInputVideo_tmp.height);
			GST_ERROR_OBJECT(xvimagesink,"4K Fit On: src_input_tmp [%d]/[%d]/[%d]/[%d]",src_input_tmp.x, src_input_tmp.y, src_input_tmp.w, src_input_tmp.h);
		}else {
			pInputVideo_tmp.width = pInputVideo.height *16/9;	// Keep the TV panel aspect ratio
			src_input_tmp.w = pInputVideo_tmp.width;
			src_input_tmp.h =  pInputVideo_tmp.height;
			GST_ERROR_OBJECT(xvimagesink,"4K Fit Off: pInputVideo_tmp [%d]/[%d]/[%d]/[%d]",pInputVideo_tmp.x, pInputVideo_tmp.y, pInputVideo_tmp.width, pInputVideo_tmp.height);
			GST_ERROR_OBJECT(xvimagesink,"4K Fit Off: src_input_tmp [%d]/[%d]/[%d]/[%d]",src_input_tmp.x, src_input_tmp.y, src_input_tmp.w, src_input_tmp.h);
		}
	}
#else
	xvimagesink->src_input.x = xvimagesink->disp_x;
	xvimagesink->src_input.y = xvimagesink->disp_y;
	xvimagesink->src_input.w = xvimagesink->disp_width;
	xvimagesink->src_input.h = xvimagesink->disp_height;

	pInputVideo.width = xvimagesink->video_width;
	pInputVideo.height = xvimagesink->video_height;

	pInputVideo_tmp.width = pInputVideo.width;
	pInputVideo_tmp.height = pInputVideo.height;
#endif

	switch (xvimagesink->display_geometry_method)	{
		case DISP_GET_METHOD_ZOOM_THREE_QUARTERS:
		{ 
			gst_FitDimension(xvimagesink,  pInputVideo_tmp.width, pInputVideo_tmp.height, &xvimagesink->dest_Rect.x, &xvimagesink->dest_Rect.y,&xvimagesink->dest_Rect.w,&xvimagesink->dest_Rect.h);
			xvimagesink->dest_Rect.x = xvimagesink->dest_Rect.x + xvimagesink->dest_Rect.w*1/8;
			xvimagesink->dest_Rect.w = xvimagesink->dest_Rect.w * 3/4;					
		}
		break;
		case DISP_GET_METHOD_ZOOM_16X9:
		{
			gst_FitDimension(xvimagesink,  pInputVideo_tmp.width, pInputVideo_tmp.height, &xvimagesink->dest_Rect.x, &xvimagesink->dest_Rect.y,&xvimagesink->dest_Rect.w,&xvimagesink->dest_Rect.h);		
		}

		if(xvimagesink->video_width == 4096 && xvimagesink->video_height == 2160){
			if(xvimagesink->is_uhd_fit){
				xvimagesink->dest_Rect.x = xvimagesink->dest_Rect.y = 0;
				xvimagesink->dest_Rect.w = xvimagesink->xwindow->width;
				xvimagesink->dest_Rect.h = xvimagesink->xwindow->height;
			}else {
				xvimagesink->src_input.x = (pInputVideo.width - pInputVideo_tmp.width) / 2;
				xvimagesink->src_input.y = 0;
				xvimagesink->src_input.w = pInputVideo_tmp.width;
				xvimagesink->src_input.h = pInputVideo_tmp.height;
			}
		}
		break;
		case DISP_GET_METHOD_ZOOM:
		{
			GST_ERROR_OBJECT(xvimagesink,"Input OFFSET zoom x[%d], zoom y[%d], zoom width[%d], zoom height[%d]",xvimagesink->zoom_x,xvimagesink->zoom_y,xvimagesink->zoom_w, xvimagesink->zoom_h);      		
			if(xvimagesink->zoom_h>10)
				xvimagesink->zoom_h = 10;
			if(xvimagesink->zoom_h<-10)
				xvimagesink->zoom_h = -10;
			if(xvimagesink->zoom_y> 15 + xvimagesink->zoom_h)
				xvimagesink->zoom_y = 15 + xvimagesink->zoom_h;
			if(xvimagesink->zoom_y<-15 -xvimagesink->zoom_h)
				xvimagesink->zoom_y = -15 -xvimagesink->zoom_h;

			gst_FitDimension(xvimagesink,  pInputVideo_tmp.width, pInputVideo_tmp.height, &xvimagesink->dest_Rect.x, &xvimagesink->dest_Rect.y,&xvimagesink->dest_Rect.w,&xvimagesink->dest_Rect.h);
			GST_INFO_OBJECT(xvimagesink,"Fit dest_Rect.x[%d], dest_Rect.y[%d], dest_Rect.width[%d], dest_Rect.height[%d]",xvimagesink->dest_Rect.x,xvimagesink->dest_Rect.y,xvimagesink->dest_Rect.w, xvimagesink->dest_Rect.h);  	
			if(xvimagesink->dest_Rect.h*(1.3+0.02*xvimagesink->zoom_h)>xvimagesink->xwindow->height)
			{
				xvimagesink->dest_Rect.h = xvimagesink->xwindow->height;
				xvimagesink->dest_Rect.y = 0;
				src_input_tmp.y = (src_input_tmp.h -1/(1.3+0.02*xvimagesink->zoom_h)*src_input_tmp.h)*1/2; 
				src_input_tmp.y = src_input_tmp.y + xvimagesink->zoom_y*src_input_tmp.y/(xvimagesink->zoom_h + 15);
				src_input_tmp.h =  1/(1.3+0.02*xvimagesink->zoom_h)*src_input_tmp.h;
			}
			else
			{
					xvimagesink->dest_Rect.h = xvimagesink->dest_Rect.h*(1.3+0.02*xvimagesink->zoom_h);
					xvimagesink->dest_Rect.y = (xvimagesink->xwindow->height - xvimagesink->dest_Rect.h)/2;
			}

			if(xvimagesink->video_width == 4096 && xvimagesink->video_height == 2160){
				if(xvimagesink->is_uhd_fit){
					xvimagesink->src_input.x = src_input_tmp.x;
					xvimagesink->src_input.y = src_input_tmp.y;
					xvimagesink->src_input.w = src_input_tmp.w;
					xvimagesink->src_input.h = src_input_tmp.h;
					xvimagesink->dest_Rect.x = xvimagesink->dest_Rect.y = 0;
					xvimagesink->dest_Rect.w = xvimagesink->xwindow->width;
					xvimagesink->dest_Rect.h = xvimagesink->xwindow->height;
				}else {
					xvimagesink->src_input.x = (pInputVideo.width - pInputVideo_tmp.width) / 2 + src_input_tmp.x;
					xvimagesink->src_input.y = src_input_tmp.y;
					xvimagesink->src_input.w = src_input_tmp.w;
					xvimagesink->src_input.h = src_input_tmp.h;
				}
			}else {
				xvimagesink->src_input.x = src_input_tmp.x;
				xvimagesink->src_input.y = src_input_tmp.y;
				xvimagesink->src_input.w = src_input_tmp.w;
				xvimagesink->src_input.h = src_input_tmp.h;
			}
		}
		break;
		case DISP_GET_METHOD_CUSTOM:
		{
			GST_ERROR_OBJECT(xvimagesink,"Input OFSET custom.x[%d], custom.y[%d], custom.width[%d], custom.height[%d]",xvimagesink->custom_x,xvimagesink->custom_y,xvimagesink->custom_w, xvimagesink->custom_h);
			if(xvimagesink->custom_h>25)
				xvimagesink->custom_h = 25;
			if(xvimagesink->custom_h< 0)
				xvimagesink->custom_h = 0;
			if(xvimagesink->custom_w>17)
				xvimagesink->custom_w = 17;
			if(xvimagesink->custom_w< 0)
				xvimagesink->custom_w = 0;
			if(xvimagesink->custom_y> 25 + xvimagesink->custom_h)
				xvimagesink->custom_y = 25 + xvimagesink->custom_h;
			if(xvimagesink->custom_y<-25 -xvimagesink->custom_h)
				xvimagesink->custom_y = -25 -xvimagesink->custom_h;
			if(xvimagesink->custom_x> 17 + xvimagesink->custom_w)
				xvimagesink->custom_x = 17 + xvimagesink->custom_w;
			if(xvimagesink->custom_x<-17 -xvimagesink->custom_w)
				xvimagesink->custom_x = -17 -xvimagesink->custom_w;

			gst_FitDimension(xvimagesink,  pInputVideo_tmp.width, pInputVideo_tmp.height, &xvimagesink->dest_Rect.x, &xvimagesink->dest_Rect.y,&xvimagesink->dest_Rect.w,&xvimagesink->dest_Rect.h);
			GST_INFO_OBJECT(xvimagesink,"Fit dest_Rect.x[%d], dest_Rect.y[%d], dest_Rect.width[%d], dest_Rect.height[%d]",xvimagesink->dest_Rect.x,xvimagesink->dest_Rect.y,xvimagesink->dest_Rect.w, xvimagesink->dest_Rect.h);  	
			if(xvimagesink->dest_Rect.h*(1+0.02*xvimagesink->custom_h)>xvimagesink->xwindow->height)
			{
				xvimagesink->dest_Rect.h = xvimagesink->xwindow->height;
				xvimagesink->dest_Rect.y = 0;
				src_input_tmp.y = (src_input_tmp.h -1/(1+0.02*xvimagesink->custom_h)*src_input_tmp.h)*1/2;   
				src_input_tmp.y = src_input_tmp.y + xvimagesink->custom_y*src_input_tmp.y/(xvimagesink->custom_h + 25);
				src_input_tmp.h =  1/(1+0.02*xvimagesink->custom_h)*src_input_tmp.h;
			}
			else
			{
					xvimagesink->dest_Rect.h = xvimagesink->dest_Rect.h*(1+0.02*xvimagesink->custom_h);
					xvimagesink->dest_Rect.y = (xvimagesink->xwindow->height - xvimagesink->dest_Rect.h)/2;
			}
			GST_INFO_OBJECT(xvimagesink,"Calculated  src_input.x[%d], src_input.y[%d], src_input.width[%d], src_input.height[%d]", xvimagesink->src_input.x,xvimagesink->src_input.y,xvimagesink->src_input.w, xvimagesink->src_input.h);
			if(xvimagesink->dest_Rect.w*(1+0.02*xvimagesink->custom_w)>xvimagesink->xwindow->width)
			{
				xvimagesink->dest_Rect.w = xvimagesink->xwindow->width;
				xvimagesink->dest_Rect.x = 0;
				src_input_tmp.x = (src_input_tmp.w -1/(1+0.02*xvimagesink->custom_w)*src_input_tmp.w)*1/2;  
				src_input_tmp.x = src_input_tmp.x + xvimagesink->custom_x*src_input_tmp.x/(xvimagesink->custom_w + 17);
				src_input_tmp.w =  1/(1+0.02*xvimagesink->custom_w)*src_input_tmp.w;
			}
			else
			{
					xvimagesink->dest_Rect.w = xvimagesink->dest_Rect.w*(1+0.02*xvimagesink->custom_w);
					xvimagesink->dest_Rect.x = (xvimagesink->xwindow->width - xvimagesink->dest_Rect.w)/2;
			}
			
			if(xvimagesink->video_width == 4096 && xvimagesink->video_height == 2160){
				if(xvimagesink->is_uhd_fit){
					xvimagesink->src_input.x = src_input_tmp.x;
					xvimagesink->src_input.y = src_input_tmp.y;
					xvimagesink->src_input.w = src_input_tmp.w;
					xvimagesink->src_input.h = src_input_tmp.h;
					xvimagesink->dest_Rect.x = xvimagesink->dest_Rect.y = 0;
					xvimagesink->dest_Rect.w = xvimagesink->xwindow->width;
					xvimagesink->dest_Rect.h = xvimagesink->xwindow->height;
				}else {
					xvimagesink->src_input.x = (pInputVideo.width - pInputVideo_tmp.width) / 2 + src_input_tmp.x;
					xvimagesink->src_input.y = src_input_tmp.y;
					xvimagesink->src_input.w = src_input_tmp.w;
					xvimagesink->src_input.h = src_input_tmp.h;
				}
			}else {
				xvimagesink->src_input.x = src_input_tmp.x;
				xvimagesink->src_input.y = src_input_tmp.y;
				xvimagesink->src_input.w = src_input_tmp.w;
				xvimagesink->src_input.h = src_input_tmp.h;
			}
		}
		break;
		default:
		{
			GST_ERROR_OBJECT(xvimagesink,"DEFAULT");
		 }
		break;
	}
	
	GST_ERROR_OBJECT(xvimagesink,"Calculated  src_input_tmp.x[%d], src_input_tmp.y[%d], src_input_tmp.width[%d], src_input_tmp.height[%d]", src_input_tmp.x,src_input_tmp.y,src_input_tmp.w, src_input_tmp.h);
	GST_ERROR_OBJECT(xvimagesink,"Calculated  src_input.x[%d], src_input.y[%d], src_input.width[%d], src_input.height[%d]", xvimagesink->src_input.x,xvimagesink->src_input.y,xvimagesink->src_input.w, xvimagesink->src_input.h);
	GST_ERROR_OBJECT(xvimagesink,"Calculated  dest_Rect.x[%d], dest_Rect.y[%d], dest_Rect.width[%d], dest_Rect.height[%d]",xvimagesink->dest_Rect.x,xvimagesink->dest_Rect.y,xvimagesink->dest_Rect.w, xvimagesink->dest_Rect.h);
	return;
}

static void
gst_FitDimension(GstXvImageSink * xvimagesink,int SrcWidth, int SrcHeight, int * DstX, int * DstY, int * DstWidth, int * DstHeight)
{
  int FixedWidth, pPanelWidth;
  int FixedHeight, pPanelHeight;
  float i, j;

  FixedWidth = xvimagesink->xwindow->width;
  FixedHeight = xvimagesink->xwindow->height;

  if(SrcWidth == 0 || SrcHeight == 0)
  {
    return;
  }

  i = FixedWidth / (float)SrcWidth;
  j = FixedHeight / (float)SrcHeight;
  if(i < j)
  {
    *DstWidth = FixedWidth;
    *DstHeight = (int)(i * SrcHeight);
    *DstX = 0;
    *DstY = (FixedHeight - (*DstHeight)) / 2;
  }
  else
  {
    *DstWidth = (int)(j * SrcWidth);
    *DstHeight = FixedHeight;
    *DstX = (FixedWidth - (*DstWidth))/2;
    *DstY = 0;
  }
  return;
}

static void calculate_netflix_display_coordinates_nine_sixteen(GstXvImageSink * xvimagesink, int par_x, int par_y, int org_w, int org_h, int * dstW, int * dstH){
	int par_x_t = 0;
	int par_y_t = 0;
	//16:9 Display Aspect Ratio (1.78)
	if(par_x == 0 || par_y == 0){
		if(org_w == 1920 && org_h == 1080){
			par_x_t = 1;
			par_y_t = 1;
		}else if(org_w == 1280 && org_h == 720){
			par_x_t = 1;
			par_y_t = 1;
		}else if(org_w == 720 && org_h == 480){
			par_x_t = 32;
			par_y_t = 27;
		}else if(org_w == 640 && org_h == 480){
			par_x_t = 4;
			par_y_t = 3;
		}else if(org_w == 512 && org_h == 384){
			par_x_t = 4;
			par_y_t = 3;
		}else if(org_w == 384 && org_h == 288){
			par_x_t = 4;
			par_y_t = 3;
		}else if(org_w == 320 && org_h == 240){
			par_x_t = 4;
			par_y_t = 3;
		}else {
			par_x_t = 1;
			par_y_t = 1;
			GST_WARNING_OBJECT(xvimagesink, "NETFLIX: 16x9, the app not set par, and the file resolution is not standard, w[%d]/h[%d]", org_w, org_h);
		}
	}else {
		par_x_t = par_x;
		par_y_t = par_y;
	}

	*dstW = org_w*par_x_t;
	*dstH = org_h*par_y_t ;

	GST_DEBUG_OBJECT(xvimagesink, "NETFLIX: 16x9 the user par[%d, %d], final par[%d, %d], Tmp W/H[%d]/[%d]",
		par_x, par_y, par_x_t, par_y_t, *dstW, *dstH);
	return;
}

static void calculate_netflix_display_coordinates_three_quarter(GstXvImageSink * xvimagesink,  int par_x, int par_y, int org_w, int org_h, int * dstW, int * dstH){
	int par_x_t = 0;
	int par_y_t = 0;
	//4:3 Display Aspect Ratio (1.33)
	if(par_x == 0 || par_y == 0){
		if(org_w == 1440 && org_h == 1080){
			par_x_t = 1;
			par_y_t = 1;
		}else if(org_w == 960 && org_h == 720){
			par_x_t = 1;
			par_y_t = 1;
		}else if(org_w == 720 && org_h == 480){
			par_x_t = 8;
			par_y_t = 9;
		}else if(org_w == 640 && org_h == 480){
			par_x_t = 1;
			par_y_t = 1;
		}else if(org_w == 512 && org_h == 384){
			par_x_t = 1;
			par_y_t = 1;
		}else if(org_w == 384 && org_h == 288){
			par_x_t = 1;
			par_y_t = 1;
		}else if(org_w == 320 && org_h == 240){
			par_x_t = 1;
			par_y_t = 1;
		}else {
			par_x_t = 1;
			par_y_t = 1;
			GST_WARNING_OBJECT(xvimagesink, "NETFLIX: 4x3, the app not set par, and the file resolution is not standard, w[%d]/h[%d]", org_w, org_h);
		}
	}else {
		par_x_t = par_x;
		par_y_t = par_y;
	}

	*dstW = org_w*par_x_t;
	*dstH = org_h*par_y_t ;

	GST_DEBUG_OBJECT(xvimagesink, "NETFLIX: 4x3 the app par[%d, %d], final par[%d, %d], Tmp W/H[%d]/[%d]",
		par_x, par_y, par_x_t, par_y_t, *dstW, *dstH);
	return;

}

/* This function puts a GstXvImage on a GstXvImageSink's window. Returns FALSE
 * if no window was available  */
static gboolean
gst_xvimagesink_xvimage_put (GstXvImageSink * xvimagesink,
    GstXvImageBuffer * xvimage, GstCaps* caps, int debug_info)
{
  GstVideoRectangle result;
  result.x = result.y = 0;
  result.w = result.h = 0;
  gboolean draw_border = FALSE;
  gboolean need_avoc_post_set = FALSE;
  gboolean is_4k_UHD = FALSE;
//  RESPictureSizeType_k eSize;

#if ENABLE_PERFORMANCE_CHECKING
  xvimage_put_time_tmp = get_time();
#endif

  if(xvimage && xvimage->width == 3840 && xvimage->height == 2160)
  {
	is_4k_UHD = TRUE;
  }

#ifdef GST_EXT_XV_ENHANCEMENT
  static Atom atom_rotation = None;
  GstVideoRectangle src_origin = { 0, 0, 0, 0};
  GstVideoRectangle src_input  = { 0, 0, 0, 0};
  GstVideoRectangle src = { 0, 0, 0, 0};
  GstVideoRectangle dst = { 0, 0, 0, 0};
  gint tmp_w_h = 0; /* add for w/h adjust */
  
  int rotate        = 0;
  int ret           = 0;
  int idx           = 0;
  int (*handler) (Display *, XErrorEvent *) = NULL;
  gboolean res = FALSE;
  int w_new = 0;
  int h_new = 0;
	
#endif /* GST_EXT_XV_ENHANCEMENT */
  /*video cropping*/
  gst_video_disp_adjust(xvimagesink);


  /* We take the flow_lock. If expose is in there we don't want to run
     concurrently from the data flow thread */
  g_mutex_lock (xvimagesink->flow_lock);

#ifdef GST_EXT_XV_ENHANCEMENT
  if (xvimagesink->xid_updated) {
    if (xvimage && xvimagesink->xvimage == NULL) {
      GST_WARNING_OBJECT (xvimagesink, "set xvimage to NULL, new xid was set right after creation of new xvimage");
      xvimage = NULL;
    }
    xvimagesink->xid_updated = FALSE;
  }
#endif /* GST_EXT_XV_ENHANCEMENT */

  if (G_UNLIKELY (xvimagesink->xwindow == NULL)) {
#ifdef GST_EXT_XV_ENHANCEMENT
    if (xvimagesink->get_pixmap_cb) {
      GST_INFO_OBJECT( xvimagesink, "xwindow is NULL, but it has get_pixmap_cb(0x%x), keep going..",xvimagesink->get_pixmap_cb );
    } else {
      GST_INFO_OBJECT( xvimagesink, "xwindow is NULL. Skip xvimage_put." );
#endif /* GST_EXT_XV_ENHANCEMENT */
    g_mutex_unlock (xvimagesink->flow_lock);
    return FALSE;
#ifdef GST_EXT_XV_ENHANCEMENT
    }
#endif /* GST_EXT_XV_ENHANCEMENT */
  }

#if 0//def GST_EXT_XV_ENHANCEMENT
  if (xvimagesink->visible == FALSE && is_4k_UHD == FALSE) {
    GST_INFO_OBJECT(xvimagesink, "visible is FALSE. Skip xvimage_put.");
    g_mutex_unlock(xvimagesink->flow_lock);
    return TRUE;
  }
#endif /* GST_EXT_XV_ENHANCEMENT */

  /* Draw borders when displaying the first frame. After this
     draw borders only on expose event or after a size change. */
  if (!xvimagesink->cur_image || xvimagesink->redraw_border) {
    draw_border = TRUE;
  }

  /* Store a reference to the last image we put, lose the previous one */
  if (xvimage && xvimagesink->cur_image != xvimage) {
    if (xvimagesink->cur_image) {
      GST_LOG_OBJECT (xvimagesink, "unreffing %p", xvimagesink->cur_image);
      gst_buffer_unref (GST_BUFFER_CAST (xvimagesink->cur_image));
    }
    GST_LOG_OBJECT (xvimagesink, "reffing %p as our current image", xvimage);
    xvimagesink->cur_image =
        GST_XVIMAGE_BUFFER_CAST (gst_buffer_ref (GST_BUFFER_CAST (xvimage)));
  }

  /* Expose sends a NULL image, we take the latest frame */
  if (!xvimage) {
    if (xvimagesink->cur_image) {
      draw_border = TRUE;
      xvimage = xvimagesink->cur_image;
    } else {
#ifdef GST_EXT_XV_ENHANCEMENT
      GST_INFO_OBJECT(xvimagesink, "cur_image is NULL. Skip xvimage_put.");
#endif /* GST_EXT_XV_ENHANCEMENT */
      g_mutex_unlock (xvimagesink->flow_lock);
      return TRUE;
    }
  }

#ifdef GST_EXT_XV_ENHANCEMENT
  if (!xvimagesink->get_pixmap_cb) {
    //gst_xvimagesink_xwindow_update_geometry( xvimagesink );
  } else {
    /* for multi-pixmap usage for the video texture */
    gst_xvimagesink_set_pixmap_handle ((GstXOverlay *)xvimagesink, xvimagesink->get_pixmap_cb(xvimagesink->get_pixmap_cb_user_data));
    idx = xvimagesink->current_pixmap_idx;
    if (idx == -1) {
      g_mutex_unlock (xvimagesink->flow_lock);
      return FALSE;
    } else if (idx == -2) {
      GST_WARNING_OBJECT(xvimagesink, "Skip putImage().");
      g_mutex_unlock (xvimagesink->flow_lock);
      return TRUE;
    }
  }
  
  /*video cropping*/
  src.x = src_origin.x = src_input.x = xvimagesink->disp_x;
  src.y = src_origin.y = src_input.y = xvimagesink->disp_y;
  src_origin.w = src_input.w = xvimagesink->disp_width;
  src_origin.h = src_input.h = xvimagesink->disp_height;	
  #if 0
  src.x = src.y = 0;
  src_origin.x = src_origin.y = src_input.x = src_input.y = 0;

  src_input.w = src_origin.w = xvimagesink->video_width;
  src_input.h = src_origin.h = xvimagesink->video_height;
  #endif
  if (xvimagesink->rotate_angle == DEGREE_0) {
    src.w = src_origin.w;
    src.h = src_origin.h;
  } else {
#if ENABLE_VF_ROTATE
    src.w = src_input.w = xvimagesink->vf_iScaledWidth;
    src.h = src_input.h = xvimagesink->vf_iScaledHeight;
#else
    src.w = src_origin.h;
    src.h = src_origin.w;
#endif
  }

   //HW Rotate   
  if(xvimagesink->enable_hw_rotate_support)
  {	
	// To calculate target geometry
	src.w = src_input.w = xvimagesink->hw_rotate_scaled_width;
	src.h = src_input.h = xvimagesink->hw_rotate_scaled_height;

	// To update the Crop setttings, source should be original dimention
	src_input.w = src_origin.w;
	src_input.h = src_origin.h;
  }
  //HW Rotate    

  dst.w = xvimagesink->render_rect.w;
  dst.h = xvimagesink->render_rect.h;

#if ENABLE_RT_SEAMLESS_GA_SCALER
  if(xvimagesink->rt_display_vaule == 1){
  	src.x = src_input.x = 0;
	src.y = src_input.y = 0;
	src.w = src_input.w = 1920;
	src.h = src_input.h = 1080;
 }
#endif

  switch (xvimagesink->display_geometry_method)
  {
    case DISP_GEO_METHOD_LETTER_BOX:
      gst_video_sink_center_rect (src, dst, &result, TRUE);
      result.x += xvimagesink->render_rect.x;
      result.y += xvimagesink->render_rect.y;
      break;

    case DISP_GEO_METHOD_ORIGIN_SIZE_OR_LETTER_BOX:
      GST_WARNING_OBJECT(xvimagesink, "not supported API, set ORIGIN_SIZE mode");
    case DISP_GEO_METHOD_ORIGIN_SIZE:
      gst_video_sink_center_rect (src, dst, &result, FALSE);
#if ENABLE_VF_ROTATE
	//Don't changed the src_inpt's coordinates
#else
      gst_video_sink_center_rect (dst, src, &src_input, FALSE);

      if (xvimagesink->rotate_angle == DEGREE_90 ||
          xvimagesink->rotate_angle == DEGREE_270) {
        src_input.x = src_input.x ^ src_input.y;
        src_input.y = src_input.x ^ src_input.y;
        src_input.x = src_input.x ^ src_input.y;

        src_input.w = src_input.w ^ src_input.h;
        src_input.h = src_input.w ^ src_input.h;
        src_input.w = src_input.w ^ src_input.h;
      }
#endif
      break;

    case DISP_GEO_METHOD_FULL_SCREEN:
      result.x = result.y = 0;
      if (!xvimagesink->get_pixmap_cb) {
#if ENABLE_VF_ROTATE
	if(xvimagesink->rotate_angle == DEGREE_90 ||
		xvimagesink->rotate_angle == DEGREE_270){
		GstVideoRectangle src_t = {MAX_SUPPORTED_ROTATED_DISPLAY_X, MAX_SUPPORTED_ROTATED_DISPLAY_Y, MAX_SUPPORTED_ROTATED_WIDTH, MAX_SUPPORTED_ROTATED_HEIGHT};
		GstVideoRectangle dst_t = {0, 0, xvimagesink->xwindow->width, xvimagesink->xwindow->height};
		gst_video_sink_center_rect (src_t, dst_t, &result, TRUE);
	}else {
		result.w = xvimagesink->xwindow->width;
		result.h = xvimagesink->xwindow->height;
	}
#else
	result.w = xvimagesink->xwindow->width;
	result.h = xvimagesink->xwindow->height;
#endif
      } else {
        result.w = xvimagesink->xpixmap[idx]->width;
        result.h = xvimagesink->xpixmap[idx]->height;
      }
      break;

    case DISP_GEO_METHOD_CROPPED_FULL_SCREEN:
#if ENABLE_VF_ROTATE
	result.x = result.y = 0;
	if(xvimagesink->rotate_angle == DEGREE_90 ||
		xvimagesink->rotate_angle == DEGREE_270){
		GstVideoRectangle src_t = {MAX_SUPPORTED_ROTATED_DISPLAY_X, MAX_SUPPORTED_ROTATED_DISPLAY_Y, MAX_SUPPORTED_ROTATED_WIDTH, MAX_SUPPORTED_ROTATED_HEIGHT};
		GstVideoRectangle dst_t = {0, 0, xvimagesink->xwindow->width, xvimagesink->xwindow->height};
		gst_video_sink_center_rect (src_t, dst_t, &result, TRUE);
	}else {
		result.w = xvimagesink->xwindow->width;
		result.h = xvimagesink->xwindow->height;
	}
#else
	gst_video_sink_center_rect(dst, src, &src_input, TRUE);

	result.x = result.y = 0;
	result.w = dst.w;
	result.h = dst.h;

	if (xvimagesink->rotate_angle == DEGREE_90 ||
	  xvimagesink->rotate_angle == DEGREE_270) {
	  src_input.x = src_input.x ^ src_input.y;
	  src_input.y = src_input.x ^ src_input.y;
	  src_input.x = src_input.x ^ src_input.y;

	  src_input.w = src_input.w ^ src_input.h;
	  src_input.h = src_input.w ^ src_input.h;
	  src_input.w = src_input.w ^ src_input.h;
	}
#endif

      break;
    case DISP_GET_METHOD_ZOOM_HALF:
      if(src.w * 9 >= 16 * src.h){
        w_new = 960;
        h_new = src.h * 960 / src.w;
      }else{
        w_new = src.w * 540 / src.h;
        h_new = 540;
      }
      if(w_new > xvimagesink->xwindow->width || h_new > xvimagesink->xwindow->height){
        gst_FitDimension(xvimagesink,w_new,h_new,&result.x,&result.y,&result.w,&result.h);
      }else{
        result.x = (xvimagesink->xwindow->width - w_new) / 2;
        result.y = (xvimagesink->xwindow->height - h_new) / 2;
        result.w = w_new;
        result.h = h_new;
      }
      break;
    case DISP_GET_METHOD_ZOOM_NETFLIX_16X9:
	calculate_netflix_display_coordinates_nine_sixteen(xvimagesink, xvimagesink->netflix_display_par_x, xvimagesink->netflix_display_par_y, src.w, src.h, &w_new, &h_new);
	gst_FitDimension(xvimagesink,w_new,h_new,&result.x,&result.y,&result.w,&result.h);
	GST_DEBUG_OBJECT(xvimagesink, "Netflix: 16x9 method, result (%d, %d, %d, %d) ", result.x, result.y, result.w, result.h);
	break;
    case DISP_GET_METHOD_ZOOM_NETFLIX_4X3:
	calculate_netflix_display_coordinates_three_quarter(xvimagesink, xvimagesink->netflix_display_par_x, xvimagesink->netflix_display_par_y, src.w, src.h, &w_new, &h_new);
	gst_FitDimension(xvimagesink,w_new,h_new,&result.x,&result.y,&result.w,&result.h);
	GST_DEBUG_OBJECT(xvimagesink, "Netflix: 4x3 display method, result (%d, %d, %d, %d) ", result.x, result.y, result.w, result.h);
	break;
	case DISP_GET_METHOD_DPS:
	//Added for DivX	
	w_new = xvimagesink->dps_display_width;
	h_new = xvimagesink->dps_display_height;
	gst_FitDimension(xvimagesink,w_new,h_new,&result.x,&result.y,&result.w,&result.h);
	GST_DEBUG_OBJECT(xvimagesink, "DivX: display method, result (%d, %d, %d, %d) ", result.x, result.y, result.w, result.h);

	break;
    case DISP_GET_METHOD_ZOOM_THREE_QUARTERS:
      memcpy(&src_input,&(xvimagesink->src_input),sizeof(GstVideoRectangle));
      memcpy(&result,&(xvimagesink->dest_Rect),sizeof(GstVideoRectangle));
	GST_DEBUG_OBJECT(xvimagesink, "method [3x4]: getRectSize src_input(%d,%d,%d,%d) result(%d,%d,%d,%d) !", 
	  	src_input.x, src_input.y, src_input.w, src_input.h, result.x, result.y, result.w, result.h);

      break;
    case DISP_GET_METHOD_ZOOM_16X9:
      memcpy(&src_input,&(xvimagesink->src_input),sizeof(GstVideoRectangle));
      memcpy(&result,&(xvimagesink->dest_Rect),sizeof(GstVideoRectangle));
	GST_DEBUG_OBJECT(xvimagesink, "method [16x9]: getRectSize src_input(%d,%d,%d,%d) result(%d,%d,%d,%d) !", 
	  	src_input.x, src_input.y, src_input.w, src_input.h, result.x, result.y, result.w, result.h);
      break;
    case DISP_GET_METHOD_ZOOM:
      memcpy(&src_input,&(xvimagesink->src_input),sizeof(GstVideoRectangle));
      memcpy(&result,&(xvimagesink->dest_Rect),sizeof(GstVideoRectangle));
      break;
    case DISP_GET_METHOD_CUSTOM:
      memcpy(&src_input,&(xvimagesink->src_input),sizeof(GstVideoRectangle));
      memcpy(&result,&(xvimagesink->dest_Rect),sizeof(GstVideoRectangle));
	  GST_DEBUG_OBJECT(xvimagesink, "method [custom]: getRectSize src_input(%d,%d,%d,%d) result(%d,%d,%d,%d) !", 
	  	src_input.x, src_input.y, src_input.w, src_input.h, result.x, result.y, result.w, result.h);

      break;
    case DISP_GEO_METHOD_CUSTOM_ROI:
#ifdef GST_EXT_XV_ENHANCEMENT_ROI_MODE
      switch (xvimagesink->rotate_angle) {
      case DEGREE_90:
        result.w = xvimagesink->dst_roi.h;
        result.h = xvimagesink->dst_roi.w;

        result.x = xvimagesink->dst_roi.y;
        if (!xvimagesink->get_pixmap_cb) {
          result.y = xvimagesink->xwindow->height - xvimagesink->dst_roi.x - xvimagesink->dst_roi.w;
        } else {
          result.y = xvimagesink->xpixmap[idx]->height - xvimagesink->dst_roi.x - xvimagesink->dst_roi.w;
        }
        break;
      case DEGREE_180:
        result.w = xvimagesink->dst_roi.w;
        result.h = xvimagesink->dst_roi.h;

        if (!xvimagesink->get_pixmap_cb) {
          result.x = xvimagesink->xwindow->width - result.w - xvimagesink->dst_roi.x;
          result.y = xvimagesink->xwindow->height - result.h - xvimagesink->dst_roi.y;
        } else {
          result.x = xvimagesink->xpixmap[idx]->width - result.w - xvimagesink->dst_roi.x;
          result.y = xvimagesink->xpixmap[idx]->height - result.h - xvimagesink->dst_roi.y;
        }
        break;
      case DEGREE_270:
        result.w = xvimagesink->dst_roi.h;
        result.h = xvimagesink->dst_roi.w;

        if (!xvimagesink->get_pixmap_cb) {
          result.x = xvimagesink->xwindow->width - xvimagesink->dst_roi.y - xvimagesink->dst_roi.h;
        } else {
          result.x = xvimagesink->xpixmap[idx]->width - xvimagesink->dst_roi.y - xvimagesink->dst_roi.h;
        }
        result.y = xvimagesink->dst_roi.x;
        break;
      default:
        result.x = xvimagesink->dst_roi.x;
        result.y = xvimagesink->dst_roi.y;
        result.w = xvimagesink->dst_roi.w;
        result.h = xvimagesink->dst_roi.h;
        break;
      }

      GST_LOG_OBJECT(xvimagesink, "rotate[%d], ROI input[%d,%d,%dx%d] > result[%d,%d,%dx%d]",
                     xvimagesink->rotate_angle,
                     xvimagesink->dst_roi.x, xvimagesink->dst_roi.y, xvimagesink->dst_roi.w, xvimagesink->dst_roi.h,
                     result.x, result.y, result.w, result.h);
#else /* GST_EXT_XV_ENHANCEMENT_ROI_MODE */
      result.x = xvimagesink->dst_roi.x;
      result.y = xvimagesink->dst_roi.y;
      result.w = xvimagesink->dst_roi.w;
      result.h = xvimagesink->dst_roi.h;

      if (xvimagesink->rotate_angle == DEGREE_90 ||
          xvimagesink->rotate_angle == DEGREE_270) {
        result.w = xvimagesink->dst_roi.h;
        result.h = xvimagesink->dst_roi.w;
      }
#endif /* GST_EXT_XV_ENHANCEMENT_ROI_MODE */
      break;

    default:
      break;
  }

  if (xvimagesink->zoom >= 0.1 && xvimagesink->zoom < 1) {
    src_input.x += (src_input.w-(gint)(src_input.w*xvimagesink->zoom))>>1;
    src_input.y += (src_input.h-(gint)(src_input.h*xvimagesink->zoom))>>1;
    src_input.w *= xvimagesink->zoom;
    src_input.h *= xvimagesink->zoom;
  }

#if (ENABLE_RT_SEAMLESS_GA_SCALER || ENABLE_RT_SEAMLESS_HW_SCALER)
  if(xvimagesink->rt_display_vaule == 1){
    result.x	= 0;
    result.y	= 0;
    result.w = 1920;
    result.h = 1080;	   
  }
#endif

#else /* GST_EXT_XV_ENHANCEMENT */
  if (xvimagesink->keep_aspect) {
    GstVideoRectangle src, dst;

    /* We use the calculated geometry from _setcaps as a source to respect
       source and screen pixel aspect ratios. */
	   
    /*video cropping*/
    //src.w = GST_VIDEO_SINK_WIDTH (xvimagesink);
    //src.h = GST_VIDEO_SINK_HEIGHT (xvimagesink);
	src.w = xvimagesink->disp_width;
	src.h = xvimagesink->disp_height;
    dst.w = xvimagesink->render_rect.w;
    dst.h = xvimagesink->render_rect.h;

    gst_video_sink_center_rect (src, dst, &result, TRUE);
    result.x += xvimagesink->render_rect.x;
    result.y += xvimagesink->render_rect.y;
  } else {
    memcpy (&result, &xvimagesink->render_rect, sizeof (GstVideoRectangle));
  }
#endif /* GST_EXT_XV_ENHANCEMENT */

  g_mutex_lock (xvimagesink->x_lock);

#ifdef GST_EXT_XV_ENHANCEMENT
  if (draw_border && xvimagesink->draw_borders && !xvimagesink->get_pixmap_cb) {
#else
  if (draw_border && xvimagesink->draw_borders) {
#endif /* GST_EXT_XV_ENHANCEMENT */
    gst_xvimagesink_xwindow_draw_borders (xvimagesink, xvimagesink->xwindow,
        result);
    xvimagesink->redraw_border = FALSE;
  }

  // HW Rotate
  //Call Driver Api for Set HW Video Rotation with rotate_degree  
  if (xvimagesink->enable_hw_rotate_support || xvimagesink->is_hw_rotate_on_mixed_frame)
  {  	   	 
	  ret = gst_xvimagesink_set_xv_port_attribute(xvimagesink, "_USER_WM_PORT_ATTRIBUTE_ROTATION", xvimagesink->hw_rotate_degree);		 
  }

  /* We scale to the window's geometry */
  guint64 T1 = 0;//get_time();
#ifdef HAVE_XSHM
  if (xvimagesink->xcontext->use_xshm) {
    GST_LOG_OBJECT (xvimagesink,
        "XvShmPutImage with image %dx%d and window %dx%d, from xvimage %"
        GST_PTR_FORMAT,
        xvimage->width, xvimage->height,
        xvimagesink->render_rect.w, xvimagesink->render_rect.h, xvimage);

#ifdef GST_EXT_XV_ENHANCEMENT
    switch( xvimagesink->rotate_angle )
    {
      /* There's slightly weired code (CCW? CW?) */
      case DEGREE_0:
        break;
      case DEGREE_90:
        rotate = 90;
        break;
      case DEGREE_180:
        rotate = 180;
        break;
      case DEGREE_270:
        rotate = 270;
        break;
      default:
        GST_WARNING_OBJECT( xvimagesink, "Unsupported rotation [%d]... set DEGREE 0.",
          xvimagesink->rotate_angle );
        break;
    }

    /* Trim as proper size */
    if (src_input.w % 2 == 1) {
        src_input.w -= 1;
    }
    if (src_input.h % 2 == 1) {
        src_input.h -= 1;
    }

    if (!xvimagesink->get_pixmap_cb) {
      GST_LOG_OBJECT( xvimagesink, "screen[%dx%d],window[%d,%d,%dx%d],method[%d],rotate[%d],zoom[%f],dp_mode[%d],src[%dx%d],dst[%d,%d,%dx%d],input[%d,%d,%dx%d],result[%d,%d,%dx%d]",
        xvimagesink->scr_w, xvimagesink->scr_h,
        xvimagesink->xwindow->x, xvimagesink->xwindow->y, xvimagesink->xwindow->width, xvimagesink->xwindow->height,
        xvimagesink->display_geometry_method, rotate, xvimagesink->zoom, xvimagesink->display_mode,
        src_origin.w, src_origin.h,
        dst.x, dst.y, dst.w, dst.h,
        src_input.x, src_input.y, src_input.w, src_input.h,
        result.x, result.y, result.w, result.h );
    } else {
      GST_LOG_OBJECT( xvimagesink, "pixmap[%d,%d,%dx%d],method[%d],rotate[%d],zoom[%f],dp_mode[%d],src[%dx%d],dst[%d,%d,%dx%d],input[%d,%d,%dx%d],result[%d,%d,%dx%d]",
      xvimagesink->xpixmap[idx]->x, xvimagesink->xpixmap[idx]->y, xvimagesink->xpixmap[idx]->width, xvimagesink->xpixmap[idx]->height,
      xvimagesink->display_geometry_method, rotate, xvimagesink->zoom, xvimagesink->display_mode,
      src_origin.w, src_origin.h,
      dst.x, dst.y, dst.w, dst.h,
      src_input.x, src_input.y, src_input.w, src_input.h,
      result.x, result.y, result.w, result.h );
    }

#if ENABLE_VF_ROTATE
   // Not set _USER_WM_PORT_ATTRIBUTE_ROTATION attribute here.
#else
    /* set display rotation */
    if (atom_rotation == None) {
      atom_rotation = XInternAtom(xvimagesink->xcontext->disp,
                                  "_USER_WM_PORT_ATTRIBUTE_ROTATION", False);
    }

    ret = XvSetPortAttribute(xvimagesink->xcontext->disp, xvimagesink->xcontext->xv_port_id, atom_rotation, rotate);
    if (ret != Success) {
      GST_ERROR_OBJECT( xvimagesink, "XvSetPortAttribute failed[%d]. disp[%x],xv_port_id[%d],atom[%x],rotate[%d]",
        ret, xvimagesink->xcontext->disp, xvimagesink->xcontext->xv_port_id, atom_rotation, rotate );
      return FALSE;
    }
#endif

    /* set error handler */
    error_caught = FALSE;
    handler = XSetErrorHandler(gst_xvimagesink_handle_xerror);

    /* src input indicates the status when degree is 0 */
    /* dst input indicates the area that src will be shown regardless of rotate */

    if ((/*xvimagesink->visible &&*/ !xvimagesink->is_hided) || (is_4k_UHD == TRUE)) {
      if (xvimagesink->xim_transparenter) {
        GST_LOG_OBJECT( xvimagesink, "Transparent related issue." );
        XPutImage(xvimagesink->xcontext->disp,
          xvimagesink->xwindow->win,
          xvimagesink->xwindow->gc,
          xvimagesink->xim_transparenter,
          0, 0,
          result.x, result.y, result.w, result.h);
      }

      if (xvimagesink->get_pixmap_cb) {
        gint idx = xvimagesink->current_pixmap_idx;
        ret = XvShmPutImage (xvimagesink->xcontext->disp,
          xvimagesink->xcontext->xv_port_id,
          xvimagesink->xpixmap[idx]->pixmap,
          xvimagesink->xpixmap[idx]->gc, xvimage->xvimage,
          src_input.x, src_input.y, src_input.w, src_input.h,
          result.x, result.y, result.w, result.h, FALSE);
        GST_LOG_OBJECT(xvimagesink, "pixmap[%d]->pixmap = %d", idx, xvimagesink->xpixmap[idx]->pixmap);
      } else {
	  GST_LOG_OBJECT( xvimagesink, "XvShmPutImage set xcontext(Display:[0x%x] Screen:[0x%x] screen_num[%d] Visual[0x%x] Window[0x%x] white/black [%d]/[%d] depth[%d] bpp[%d] endianness[%d] width/height [%d]/[%d] widthmm/heightmm [%d]/[%d] GValue[0x%x] use_xshm[%d] XvPortID[%d] nb_adaptors[%d] im_format[%d])", 
			xvimagesink->xcontext->disp, xvimagesink->xcontext->screen, xvimagesink->xcontext->screen_num, xvimagesink->xcontext->visual, 
			&xvimagesink->xcontext->root, xvimagesink->xcontext->white, xvimagesink->xcontext->black, xvimagesink->xcontext->depth, xvimagesink->xcontext->bpp, xvimagesink->xcontext->endianness,
			xvimagesink->xcontext->width, xvimagesink->xcontext->height, xvimagesink->xcontext->widthmm, xvimagesink->xcontext->heightmm, xvimagesink->xcontext->par, 
			xvimagesink->xcontext->use_xshm, xvimagesink->xcontext->xv_port_id, xvimagesink->xcontext->nb_adaptors, xvimagesink->xcontext->im_format);
	  GST_LOG_OBJECT( xvimagesink, "XvShmPutImage set xwindow(Window[0x%x] x/y [%d]/[%d] width/height [%d]/[%d] internal[%d] GC)",
	 		&xvimagesink->xwindow->win, xvimagesink->xwindow->x, xvimagesink->xwindow->y, xvimagesink->xwindow->width, xvimagesink->xwindow->height, 
	 		xvimagesink->xwindow->internal, xvimagesink->xwindow->gc);
	  if (debug_info>0x50 && GST_BASE_SINK_CAST(xvimagesink)->debugCategory)
	  {
	    GST_CAT_INFO_OBJECT(GST_BASE_SINK_CAST(xvimagesink)->debugCategory, xvimagesink, "pixel[ %02x, %s] base=%"GST_TIME_FORMAT5" curr=%"GST_TIME_FORMAT5" ts=%"GST_TIME_FORMAT5" set src_input [%d][%d][%d][%d] -> result[%d][%d][%d][%d]",
		  	debug_info, debug_info>0x50?"-sync-":" ", GST_TIME_ARGS5(GST_ELEMENT_CAST (xvimagesink)->base_time), 
		  	GST_TIME_ARGS5(gst_clock_get_time (GST_ELEMENT_CLOCK (xvimagesink))),
	 		GST_TIME_ARGS5(GST_BUFFER_TIMESTAMP (GST_BUFFER_CAST(xvimage))), src_input.x, src_input.y, src_input.w, src_input.h,
	 		result.x, result.y, result.w, result.h);
	  }
	  GST_INFO_OBJECT( xvimagesink, " base=%"GST_TIME_FORMAT5" curr=%"GST_TIME_FORMAT5" ts=%"GST_TIME_FORMAT5" set src_input [%d][%d][%d][%d] -> result[%d][%d][%d][%d]",
		  	GST_TIME_ARGS5(GST_ELEMENT_CAST (xvimagesink)->base_time), 
		  	GST_TIME_ARGS5(gst_clock_get_time (GST_ELEMENT_CLOCK (xvimagesink))),
	 		GST_TIME_ARGS5(GST_BUFFER_TIMESTAMP (GST_BUFFER_CAST(xvimage))), src_input.x, src_input.y, src_input.w, src_input.h,
	 		result.x, result.y, result.w, result.h);
#if ENABLE_VF_ROTATE
	  /// adjust the display index before XvShmPutImage function, to avoid any else condition.
	  if(xvimagesink->vf_need_update_display_idx){
	  	pre_adjust_display_index(xvimagesink);
	  }
#endif
	  
        T1 = get_time();
        ret = XvShmPutImage (xvimagesink->xcontext->disp,
          xvimagesink->xcontext->xv_port_id,
          xvimagesink->xwindow->win,
          xvimagesink->xwindow->gc, xvimage->xvimage,
          src_input.x, src_input.y, src_input.w, src_input.h,
          result.x, result.y, result.w, result.h, FALSE);
        if (ret==0 && xvimagesink->is_first_putimage)  // No need to avoc setting for pixmap case.
        {
          need_avoc_post_set = TRUE;
        }
        if (ret == 0 && caps && FIND_MASK(xvimagesink->mute_flag, MUTE_RESOLUTION_CHANGE))
        {
          mute_video_display(xvimagesink, FALSE, MUTE_RESOLUTION_CHANGE);
        }
        if(ret == 11 /*Bad Alloc (on MLS Senario UD&UD not available resource)*/) 
        {
        	  GST_ELEMENT_ERROR (xvimagesink, RESOURCE, NOT_FOUND,
              ("on MLS Senario, UD&UD not available resource "), (NULL));
        }
      }
    } else {
      GST_LOG_OBJECT( xvimagesink, "visible is FALSE. skip this image..." );
    }
#else /* GST_EXT_XV_ENHANCEMENT */
    GST_INFO_OBJECT( xvimagesink, "XvShmPutImage ts=%"GST_TIME_FORMAT" disp: [%d][%d][%d][%d], result[%d][%d][%d][%d]",
        GST_TIME_ARGS(GST_BUFFER_TIMESTAMP (GST_BUFFER_CAST(xvimage))), xvimagesink->disp_x, xvimagesink->disp_y, xvimagesink->disp_width, xvimagesink->disp_height,
        result.x, result.y, result.w, result.h);
    XvShmPutImage (xvimagesink->xcontext->disp,
        xvimagesink->xcontext->xv_port_id,
        xvimagesink->xwindow->win,
        xvimagesink->xwindow->gc, xvimage->xvimage,
        xvimagesink->disp_x, xvimagesink->disp_y,
        xvimagesink->disp_width, xvimagesink->disp_height,
        result.x, result.y, result.w, result.h, FALSE);
#endif /* GST_EXT_XV_ENHANCEMENT */
  } else
#endif /* HAVE_XSHM */
  {
    GST_INFO_OBJECT( xvimagesink, "XvPutImage ts=%"GST_TIME_FORMAT" disp: [%d][%d][%d][%d], result[%d][%d][%d][%d]",
        GST_TIME_ARGS(GST_BUFFER_TIMESTAMP (GST_BUFFER_CAST(xvimage))), xvimagesink->disp_x, xvimagesink->disp_y, xvimagesink->disp_width, xvimagesink->disp_height,
        result.x, result.y, result.w, result.h);
    XvPutImage (xvimagesink->xcontext->disp,
        xvimagesink->xcontext->xv_port_id,
        xvimagesink->xwindow->win,
        xvimagesink->xwindow->gc, xvimage->xvimage,
        xvimagesink->disp_x, xvimagesink->disp_y,
        xvimagesink->disp_width, xvimagesink->disp_height,
        result.x, result.y, result.w, result.h);
  }
  guint64 T2 = get_time();
  XSync (xvimagesink->xcontext->disp, FALSE);
  guint64 T3 = get_time();
  if (T1!=0 && T3-T1 > (MIN_LATENCY*2))
    GST_ERROR_OBJECT(xvimagesink,"XvShmPutImage[ %lld ms ], XSync[ %lld ms ]  makes a latency for video rendering", (T2-T1)/1000, (T3-T2)/1000);

  if (need_avoc_post_set && caps)
  {
  /* 1. Beginning of playback : Sync, No seamless, Set 2 times(Pre/Post) will be called after first putimage */
    gst_xvimagesink_avoc_set_resolution(xvimagesink, caps, (xvimagesink->video_quality_mode & VQ_MODE_ASYNCHRONOUS_SET)/* SYNC */, FALSE/*Non-Seamless*/, FALSE/*POST SET*/);
    xvimagesink->is_first_putimage = FALSE; // MUST BE SET AFTER gst_xvimagesink_avoc_set_resolution(), becuase this function consider this value.
    /* Scaler Unmute */
    mute_video_display(xvimagesink, FALSE, MUTE_DEFAULT);
  }
  
#ifdef HAVE_XSHM
#ifdef GST_EXT_XV_ENHANCEMENT
  if (xvimagesink->get_pixmap_cb) {
    if (error_caught) {
      g_signal_emit (G_OBJECT (xvimagesink),
                     gst_xvimagesink_signals[SIGNAL_FRAME_RENDER_ERROR],
                     0,
                     &xvimagesink->xpixmap[idx]->pixmap,
                     &res);
      GST_WARNING_OBJECT( xvimagesink, "pixmap cb case, putimage error_caught" );
    }
  }
  else
  {
	if (error_caught) {
		GST_ERROR_OBJECT(xvimagesink,"triggered an XError. Returned.");	
		/* Reset error handler */
		error_caught = FALSE;
		XSetErrorHandler (handler);
		g_mutex_unlock (xvimagesink->x_lock);
		g_mutex_unlock (xvimagesink->flow_lock);
		return FALSE;
	}
  }
  /* Reset error handler */
  error_caught = FALSE;
  XSetErrorHandler (handler);
#endif /* GST_EXT_XV_ENHANCEMENT */
#endif /* HAVE_XSHM */

  g_mutex_unlock (xvimagesink->x_lock);

  g_mutex_unlock (xvimagesink->flow_lock);

#if ENABLE_PERFORMANCE_CHECKING
  xvimage_put_count++;
  total_xvimage_put_time += get_time() - xvimage_put_time_tmp;
  if(xvimage_put_count == 10 || xvimage_put_count == 300 || xvimage_put_count == 800 || xvimage_put_count == 1000 || xvimage_put_count == 2000 || xvimage_put_count == 3000){
	GST_ERROR_OBJECT(xvimagesink, "***************************xvimage put performance test: *****************************************");
	GST_ERROR_OBJECT(xvimagesink, "xvimage put=%d, total_xvimage_put_time =%lldus, average xvimage put time=%lldus", xvimage_put_count, total_xvimage_put_time, total_xvimage_put_time/xvimage_put_count);
  }
#endif

  return TRUE;
}

static gboolean
gst_xvimagesink_xwindow_decorate (GstXvImageSink * xvimagesink,
    GstXWindow * window)
{
  Atom hints_atom = None;
  MotifWmHints *hints;

  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), FALSE);
  g_return_val_if_fail (window != NULL, FALSE);

  g_mutex_lock (xvimagesink->x_lock);

  hints_atom = XInternAtom (xvimagesink->xcontext->disp, "_MOTIF_WM_HINTS",
      True);
  if (hints_atom == None) {
    g_mutex_unlock (xvimagesink->x_lock);
    return FALSE;
  }

  hints = g_malloc0 (sizeof (MotifWmHints));

  hints->flags |= MWM_HINTS_DECORATIONS;
  hints->decorations = 1 << 0;

  XChangeProperty (xvimagesink->xcontext->disp, window->win,
      hints_atom, hints_atom, 32, PropModeReplace,
      (guchar *) hints, sizeof (MotifWmHints) / sizeof (long));

  XSync (xvimagesink->xcontext->disp, FALSE);

  g_mutex_unlock (xvimagesink->x_lock);

  g_free (hints);

  return TRUE;
}

static void
gst_xvimagesink_xwindow_set_title (GstXvImageSink * xvimagesink,
    GstXWindow * xwindow, const gchar * media_title)
{
  if (media_title) {
    g_free (xvimagesink->media_title);
    xvimagesink->media_title = g_strdup (media_title);
  }
  if (xwindow) {
    /* we have a window */
    if (xwindow->internal) {
      XTextProperty xproperty;
      const gchar *app_name;
      const gchar *title = NULL;
      gchar *title_mem = NULL;

      /* set application name as a title */
      app_name = g_get_application_name ();

      if (app_name && xvimagesink->media_title) {
        title = title_mem = g_strconcat (xvimagesink->media_title, " : ",
            app_name, NULL);
      } else if (app_name) {
        title = app_name;
      } else if (xvimagesink->media_title) {
        title = xvimagesink->media_title;
      }

      if (title) {
        if ((XStringListToTextProperty (((char **) &title), 1,
                    &xproperty)) != 0) {
          XSetWMName (xvimagesink->xcontext->disp, xwindow->win, &xproperty);
          XFree (xproperty.value);
        }

        g_free (title_mem);
      }
    }
  }
}

#ifdef GST_EXT_XV_ENHANCEMENT
static XImage *make_transparent_image(Display *d, Window win, int w, int h)
{
  XImage *xim;

  /* create a normal ximage */
  xim = XCreateImage(d, DefaultVisualOfScreen(DefaultScreenOfDisplay(d)),  24, ZPixmap, 0, NULL, w, h, 32, 0);

  GST_INFO("ximage %p", xim);

  /* allocate data for it */
  if (xim) {
    xim->data = (char *)malloc(xim->bytes_per_line * xim->height);
    if (xim->data) {
      memset(xim->data, 0x00, xim->bytes_per_line * xim->height);
      return xim;
    } else {
      GST_ERROR("failed to alloc data - size %d", xim->bytes_per_line * xim->height);
    }

    XDestroyImage(xim);
  }

  GST_ERROR("failed to create Ximage");

  return NULL;
}
#if 0
static gboolean set_input_mode(GstXContext *xcontext, guint set_mode)
{
  int ret = 0;
  static gboolean is_exist = FALSE;
  static XvPortID current_port_id = -1;
  Atom atom_output = None;

  if (xcontext == NULL) {
    GST_WARNING("xcontext is NULL");
    return FALSE;
  }

  /* check once per one xv_port_id */
  if (current_port_id != xcontext->xv_port_id) {
    /* check whether _USER_WM_PORT_ATTRIBUTE_OUTPUT attribute is existed */
    int i = 0;
    int count = 0;
    XvAttribute *const attr = XvQueryPortAttributes(xcontext->disp,
                                                    xcontext->xv_port_id, &count);
    if (attr) {
      current_port_id = xcontext->xv_port_id;
      for (i = 0 ; i < count ; i++) {
        if (!strcmp(attr[i].name, "_USER_WM_PORT_ATTRIBUTE_INPUT")) {
          is_exist = TRUE;
          GST_INFO("_USER_WM_PORT_ATTRIBUTE_INPUT[index %d] found", i);
          break;
        }
      }
      XFree(attr);
    } else {
      GST_WARNING("XvQueryPortAttributes disp:%d, port_id:%d failed",
                  xcontext->disp, xcontext->xv_port_id);
    }
  }

  if (is_exist) {
    GST_LOG("set input mode %d", set_mode);
    atom_output = XInternAtom(xcontext->disp,
                              "_USER_WM_PORT_ATTRIBUTE_INPUT", False);
    ret = XvSetPortAttribute(xcontext->disp, xcontext->xv_port_id,
                             atom_output, set_mode);
    if (ret == Success) {
      return TRUE;
    } else {
      GST_ERROR("display mode[%d] set failed.", set_mode);
    }
  } else {
    GST_WARNING("_USER_WM_PORT_ATTRIBUTE_INPUT is not existed");
  }

  return FALSE;
}
#endif

/*
  set_flip_mode uses flow_lock, x_lock internally.
*/
static gboolean set_flip_mode(GstXvImageSink *xvimagesink, guint flip)
{
  g_return_val_if_fail(xvimagesink, FALSE);
  g_return_val_if_fail(xvimagesink->xcontext, FALSE);
  g_return_val_if_fail(xvimagesink->xcontext->disp, FALSE);

  static Atom atom_hflip = None;
  static Atom atom_vflip = None;
  if (atom_hflip == None) {
    atom_hflip = XInternAtom(xvimagesink->xcontext->disp,
                             "_USER_WM_PORT_ATTRIBUTE_HFLIP", False);
  }
  if (atom_vflip == None) {
    atom_vflip = XInternAtom(xvimagesink->xcontext->disp,
                             "_USER_WM_PORT_ATTRIBUTE_VFLIP", False);
  }

  gboolean set_hflip = FALSE;
  gboolean set_vflip = FALSE;
  switch (flip) {
  case FLIP_HORIZONTAL:
    set_hflip = TRUE;
    set_vflip = FALSE;
    break;
  case FLIP_VERTICAL:
    set_hflip = FALSE;
    set_vflip = TRUE;
    break;
  case FLIP_BOTH:
    set_hflip = TRUE;
    set_vflip = TRUE;
    break;
  case FLIP_NONE:
  default:
    set_hflip = FALSE;
    set_vflip = FALSE;
    break;
  }
  g_mutex_lock (xvimagesink->flow_lock);
  g_mutex_lock (xvimagesink->x_lock);
  mute_video_display(xvimagesink, TRUE, MUTE_FLIP_MODE_CHANGE);
  GST_INFO_OBJECT(xvimagesink, "HFLIP[ %s ], VFLIP[ %s ],  flip[ %d ]", set_hflip?"ON":"OFF", set_vflip?"ON":"OFF", flip );
  int ret = XvSetPortAttribute(xvimagesink->xcontext->disp, xvimagesink->xcontext->xv_port_id, atom_hflip, set_hflip);
  if (ret != Success) {
    GST_WARNING_OBJECT(xvimagesink, "set HFLIP failed[%d]. disp[%x],xv_port_id[%d],atom[%x],hflip[%d]",
                ret, xvimagesink->xcontext->disp, xvimagesink->xcontext->xv_port_id, atom_hflip, set_hflip);
  }
  ret = XvSetPortAttribute(xvimagesink->xcontext->disp, xvimagesink->xcontext->xv_port_id, atom_vflip, set_vflip);
  if (ret != Success) {
    GST_WARNING_OBJECT(xvimagesink, "set VFLIP failed[%d]. disp[%x],xv_port_id[%d],atom[%x],vflip[%d]",
                ret, xvimagesink->xcontext->disp, xvimagesink->xcontext->xv_port_id, atom_vflip, set_vflip);
  }
  mute_video_display(xvimagesink, FALSE, MUTE_FLIP_MODE_CHANGE);
  g_mutex_unlock (xvimagesink->x_lock);
  g_mutex_unlock(xvimagesink->flow_lock);
  return ((ret == Success) ? TRUE : FALSE);
}

static gboolean set_display_mode(GstXContext *xcontext, int set_mode)
{
  int ret = 0;
  static gboolean is_exist = FALSE;
  static XvPortID current_port_id = -1;
  Atom atom_output = None;

  if (xcontext == NULL) {
    GST_WARNING("xcontext is NULL");
    return FALSE;
  }

  /* check once per one xv_port_id */
  if (current_port_id != xcontext->xv_port_id) {
    /* check whether _USER_WM_PORT_ATTRIBUTE_OUTPUT attribute is existed */
    int i = 0;
    int count = 0;
    XvAttribute *const attr = XvQueryPortAttributes(xcontext->disp,
                                                    xcontext->xv_port_id, &count);
    if (attr) {
      current_port_id = xcontext->xv_port_id;
      for (i = 0 ; i < count ; i++) {
        if (!strcmp(attr[i].name, "_USER_WM_PORT_ATTRIBUTE_OUTPUT")) {
          is_exist = TRUE;
          GST_INFO("_USER_WM_PORT_ATTRIBUTE_OUTPUT[index %d] found", i);
          break;
        }
      }
      XFree(attr);
    } else {
      GST_WARNING("XvQueryPortAttributes disp:%d, port_id:%d failed",
                  xcontext->disp, xcontext->xv_port_id);
    }
  }

  if (is_exist) {
    GST_INFO("set display mode %d", set_mode);
    atom_output = XInternAtom(xcontext->disp,
                              "_USER_WM_PORT_ATTRIBUTE_OUTPUT", False);
    ret = XvSetPortAttribute(xcontext->disp, xcontext->xv_port_id,
                             atom_output, set_mode);
    if (ret == Success) {
      return TRUE;
    } else {
      GST_WARNING("display mode[%d] set failed.", set_mode);
    }
  } else {
    GST_WARNING("_USER_WM_PORT_ATTRIBUTE_OUTPUT is not existed");
  }

  return FALSE;
}

static void drm_init(GstXvImageSink *xvimagesink)
{
#if 0
	Display *dpy;
	int i = 0;
	int eventBase = 0;
	int errorBase = 0;
	int dri2Major = 0;
	int dri2Minor = 0;
	char *driverName = NULL;
	char *deviceName = NULL;
	struct drm_auth auth_arg = {0};

	xvimagesink->drm_fd = -1;

	dpy = XOpenDisplay(0);
	if (!dpy) {
		GST_ERROR("XOpenDisplay failed errno:%d", errno);
		return;
	}

	GST_INFO("START");

	/* DRI2 */
	if (!DRI2QueryExtension(dpy, &eventBase, &errorBase)) {
		GST_ERROR("DRI2QueryExtension !!");
		goto DRM_INIT_ERROR;
	}

	if (!DRI2QueryVersion(dpy, &dri2Major, &dri2Minor)) {
		GST_ERROR("DRI2QueryVersion !!");
		goto DRM_INIT_ERROR;
	}

	if (!DRI2Connect(dpy, RootWindow(dpy, DefaultScreen(dpy)), &driverName, &deviceName)) {
		GST_ERROR("DRI2Connect !!");
		goto DRM_INIT_ERROR;
	}

	if (!driverName || !deviceName) {
		GST_ERROR("driverName or deviceName is not valid");
		goto DRM_INIT_ERROR;
	}

	GST_INFO("Open drm device : %s", deviceName);

	/* get the drm_fd though opening the deviceName */
	xvimagesink->drm_fd = open(deviceName, O_RDWR);
	if (xvimagesink->drm_fd < 0) {
		GST_ERROR("cannot open drm device (%s)", deviceName);
		goto DRM_INIT_ERROR;
	}

	/* get magic from drm to authentication */
	if (ioctl(xvimagesink->drm_fd, DRM_IOCTL_GET_MAGIC, &auth_arg)) {
		GST_ERROR("cannot get drm auth magic");
		goto DRM_INIT_ERROR;
	}

	if (!DRI2Authenticate(dpy, RootWindow(dpy, DefaultScreen(dpy)), auth_arg.magic)) {
		GST_ERROR("cannot get drm authentication from X");
		goto DRM_INIT_ERROR;
	}

	/* init gem handle */
	for (i = 0 ; i < MAX_PLANE_NUM ; i++) {
		xvimagesink->gem_handle[i] = 0;
	}

	XCloseDisplay(dpy);
	free(driverName);
	free(deviceName);

	GST_INFO("DONE");

	return;

DRM_INIT_ERROR:
	if (xvimagesink->drm_fd >= 0) {
		close(xvimagesink->drm_fd);
		xvimagesink->drm_fd = -1;
	}
	if (dpy) {
		XCloseDisplay(dpy);
	}
	if (driverName) {
		free(driverName);
	}
	if (deviceName) {
		free(deviceName);
	}
#endif
	return;
}

static void drm_fini(GstXvImageSink *xvimagesink)
{
#if 0
	GST_INFO("START");

	if (xvimagesink->drm_fd >= 0) {
		GST_INFO("close drm_fd %d", xvimagesink->drm_fd);
		close(xvimagesink->drm_fd);
		xvimagesink->drm_fd = -1;
	} else {
		GST_INFO("DRM device is NOT opened");
	}

	GST_INFO("DONE");
#endif
}

static unsigned int drm_convert_dmabuf_gemname(GstXvImageSink *xvimagesink, int dmabuf_fd, int *gem_handle)
{
#if 0
	int i = 0;
	int ret = 0;

	struct drm_prime_handle prime_arg = {0,};
	struct drm_gem_flink flink_arg = {0,};

	if (!xvimagesink || !gem_handle) {
		GST_ERROR("handle[%p,%p] is NULL", xvimagesink, gem_handle);
		return 0;
	}

	GST_LOG("START - fd : %d", dmabuf_fd);

	if (xvimagesink->drm_fd < 0) {
		GST_ERROR("DRM is not opened");
		return 0;
	}

	if (dmabuf_fd <= 0) {
		GST_LOG("Ignore wrong dmabuf fd(%d)", dmabuf_fd);
		return 0;
	}

	prime_arg.fd = dmabuf_fd;
	ret = ioctl(xvimagesink->drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_arg);
	if (ret) {
		GST_ERROR("DRM_IOCTL_PRIME_FD_TO_HANDLE failed. ret %d, dmabuf fd : %d", ret, dmabuf_fd);
		return 0;
	}

	GST_LOG("gem handle %u", prime_arg.handle);

	*gem_handle = prime_arg.handle;
	flink_arg.handle = prime_arg.handle;
	ret = ioctl(xvimagesink->drm_fd, DRM_IOCTL_GEM_FLINK, &flink_arg);
	if (ret) {
		GST_ERROR("DRM_IOCTL_GEM_FLINK failed. ret %d, gem_handle %u, gem_name %u", ret, *gem_handle, flink_arg.name);
		return 0;
	}

	GST_LOG("DONE converted GEM name %u", flink_arg.name);

	return flink_arg.name;
#else
	return 0;
#endif
}

static void drm_close_gem(GstXvImageSink *xvimagesink, unsigned int *gem_handle)
{
#if 0
	struct drm_gem_close close_arg = {0,};

	if (xvimagesink->drm_fd < 0 || !gem_handle) {
		GST_ERROR("DRM is not opened");
		return;
	}

	if (*gem_handle <= 0) {
		GST_DEBUG("invalid gem handle %d", *gem_handle);
		return;
	}

	GST_DEBUG("Call DRM_IOCTL_GEM_CLOSE");

	close_arg.handle = *gem_handle;
	if (ioctl(xvimagesink->drm_fd, DRM_IOCTL_GEM_CLOSE, &close_arg)) {
		GST_ERROR("cannot close drm gem handle %d", gem_handle);
		return;
	}

	GST_DEBUG("close gem handle %d done", *gem_handle);

	*gem_handle = 0;
#endif
	return;
}
#endif /* GST_EXT_XV_ENHANCEMENT */

/* This function handles a GstXWindow creation
 * The width and height are the actual pixel size on the display */
static GstXWindow *
gst_xvimagesink_xwindow_new (GstXvImageSink * xvimagesink,
    gint width, gint height)
{
  GstXWindow *xwindow = NULL;
  XGCValues values;
#ifdef GST_EXT_XV_ENHANCEMENT
  XSetWindowAttributes win_attr;
  XWindowAttributes root_attr;
#endif /* GST_EXT_XV_ENHANCEMENT */

  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), NULL);

  xwindow = g_new0 (GstXWindow, 1);

  xvimagesink->render_rect.x = xvimagesink->render_rect.y = 0;
#ifdef GST_EXT_XV_ENHANCEMENT
  /* 0 or 180 */
  if (xvimagesink->rotate_angle == 0 || xvimagesink->rotate_angle == 2) {
    xvimagesink->render_rect.w = xwindow->width = width;
    xvimagesink->render_rect.h = xwindow->height = height;
  /* 90 or 270 */
  } else {
    xvimagesink->render_rect.w = xwindow->width = height;
    xvimagesink->render_rect.h = xwindow->height = width;
  }

  XGetWindowAttributes(xvimagesink->xcontext->disp, xvimagesink->xcontext->root, &root_attr);

  if (xwindow->width > root_attr.width) {
    GST_INFO_OBJECT(xvimagesink, "Width[%d] is bigger than Max width. Set Max[%d].",
                                 xwindow->width, root_attr.width);
    xvimagesink->render_rect.w = xwindow->width = root_attr.width;
  }
  if (xwindow->height > root_attr.height) {
    GST_INFO_OBJECT(xvimagesink, "Height[%d] is bigger than Max Height. Set Max[%d].",
                                 xwindow->height, root_attr.height);
    xvimagesink->render_rect.h = xwindow->height = root_attr.height;
  }
  xwindow->internal = TRUE;

  g_mutex_lock (xvimagesink->x_lock);

  GST_DEBUG_OBJECT( xvimagesink, "window create [%dx%d]", xwindow->width, xwindow->height );

  xwindow->win = XCreateSimpleWindow(xvimagesink->xcontext->disp,
                                     xvimagesink->xcontext->root,
                                     0, 0, xwindow->width, xwindow->height,
                                     0, 0, 0);

  xvimagesink->xim_transparenter = make_transparent_image(xvimagesink->xcontext->disp,
                                                          xvimagesink->xcontext->root,
                                                          xwindow->width, xwindow->height);

  /* Make window manager not to change window size as Full screen */
  win_attr.override_redirect = True;
  XChangeWindowAttributes(xvimagesink->xcontext->disp, xwindow->win, CWOverrideRedirect, &win_attr);
#else /* GST_EXT_XV_ENHANCEMENT */
  xvimagesink->render_rect.w = width;
  xvimagesink->render_rect.h = height;

  xwindow->width = width;
  xwindow->height = height;
  xwindow->internal = TRUE;

  g_mutex_lock (xvimagesink->x_lock);

  xwindow->win = XCreateSimpleWindow (xvimagesink->xcontext->disp,
      xvimagesink->xcontext->root,
      0, 0, width, height, 0, 0, xvimagesink->xcontext->black);
#endif /* GST_EXT_XV_ENHANCEMENT */

  /* We have to do that to prevent X from redrawing the background on
   * ConfigureNotify. This takes away flickering of video when resizing. */
  XSetWindowBackgroundPixmap (xvimagesink->xcontext->disp, xwindow->win, None);

  /* set application name as a title */
  gst_xvimagesink_xwindow_set_title (xvimagesink, xwindow, NULL);

  if (xvimagesink->handle_events) {
    Atom wm_delete;

    XSelectInput (xvimagesink->xcontext->disp, xwindow->win, ExposureMask |
        StructureNotifyMask | PointerMotionMask | KeyPressMask |
        KeyReleaseMask | ButtonPressMask | ButtonReleaseMask);

    /* Tell the window manager we'd like delete client messages instead of
     * being killed */
    wm_delete = XInternAtom (xvimagesink->xcontext->disp,
        "WM_DELETE_WINDOW", True);
    if (wm_delete != None) {
      (void) XSetWMProtocols (xvimagesink->xcontext->disp, xwindow->win,
          &wm_delete, 1);
    }
  }

  xwindow->gc = XCreateGC (xvimagesink->xcontext->disp,
      xwindow->win, 0, &values);

  XMapRaised (xvimagesink->xcontext->disp, xwindow->win);

  XSync (xvimagesink->xcontext->disp, FALSE);

  g_mutex_unlock (xvimagesink->x_lock);

  gst_xvimagesink_xwindow_decorate (xvimagesink, xwindow);

  gst_x_overlay_got_window_handle (GST_X_OVERLAY (xvimagesink), xwindow->win);

  return xwindow;
}

/* This function destroys a GstXWindow */
static void
gst_xvimagesink_xwindow_destroy (GstXvImageSink * xvimagesink,
    GstXWindow * xwindow)
{
  g_return_if_fail (xwindow != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  g_mutex_lock (xvimagesink->x_lock);

  /* If we did not create that window we just free the GC and let it live */
  if (xwindow->internal) {
    XDestroyWindow (xvimagesink->xcontext->disp, xwindow->win);
    if (xvimagesink->xim_transparenter) {
      XDestroyImage(xvimagesink->xim_transparenter);
      xvimagesink->xim_transparenter = NULL;
    }
  } else {
    XSelectInput (xvimagesink->xcontext->disp, xwindow->win, 0);
  }

  XFreeGC (xvimagesink->xcontext->disp, xwindow->gc);

  XSync (xvimagesink->xcontext->disp, FALSE);

  g_mutex_unlock (xvimagesink->x_lock);

  g_free (xwindow);
}

#ifdef GST_EXT_XV_ENHANCEMENT
/* This function destroys a GstXWindow */
static void
gst_xvimagesink_xpixmap_destroy (GstXvImageSink * xvimagesink,
    GstXPixmap * xpixmap)
{
  g_return_if_fail (xpixmap != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  g_mutex_lock (xvimagesink->x_lock);

  XSelectInput (xvimagesink->xcontext->disp, xpixmap->pixmap, 0);

  XFreeGC (xvimagesink->xcontext->disp, xpixmap->gc);

  XSync (xvimagesink->xcontext->disp, FALSE);

  g_mutex_unlock (xvimagesink->x_lock);

  g_free (xpixmap);
}
#endif /* GST_EXT_XV_ENHANCEMENT */

static void
gst_xvimagesink_xwindow_update_geometry (GstXvImageSink * xvimagesink)
{
#ifdef GST_EXT_XV_ENHANCEMENT
  Window root_window, child_window;
  XWindowAttributes root_attr;

  int cur_win_x = 0;
  int cur_win_y = 0;
  unsigned int cur_win_width = 0;
  unsigned int cur_win_height = 0;
  unsigned int cur_win_border_width = 0;
  unsigned int cur_win_depth = 0;
#else /* GST_EXT_XV_ENHANCEMENT */
  XWindowAttributes attr;
#endif /* GST_EXT_XV_ENHANCEMENT */

  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  /* Update the window geometry */
  g_mutex_lock (xvimagesink->x_lock);
  if (G_UNLIKELY (xvimagesink->xwindow == NULL)) {
    g_mutex_unlock (xvimagesink->x_lock);
    return;
  }

#ifdef GST_EXT_XV_ENHANCEMENT
  /* Get root window and size of current window */
  XGetGeometry( xvimagesink->xcontext->disp, xvimagesink->xwindow->win, &root_window,
    &cur_win_x, &cur_win_y, /* relative x, y */
    &cur_win_width, &cur_win_height,
    &cur_win_border_width, &cur_win_depth );

  xvimagesink->xwindow->width = cur_win_width;
  xvimagesink->xwindow->height = cur_win_height;

  /* Get absolute coordinates of current window */
  XTranslateCoordinates( xvimagesink->xcontext->disp,
    xvimagesink->xwindow->win,
    root_window,
    0, 0,
    &cur_win_x, &cur_win_y, // relative x, y to root window == absolute x, y
    &child_window );

  xvimagesink->xwindow->x = cur_win_x;
  xvimagesink->xwindow->y = cur_win_y;

  /* Get size of root window == size of screen */
  XGetWindowAttributes(xvimagesink->xcontext->disp, root_window, &root_attr);

  xvimagesink->scr_w = root_attr.width;
  xvimagesink->scr_h = root_attr.height;

  if (!xvimagesink->have_render_rect) {
    xvimagesink->render_rect.x = xvimagesink->render_rect.y = 0;
    xvimagesink->render_rect.w = cur_win_width;
    xvimagesink->render_rect.h = cur_win_height;
  }

  GST_LOG_OBJECT(xvimagesink, "screen size %dx%d, current window geometry %d,%d,%dx%d, render_rect %d,%d,%dx%d",
    xvimagesink->scr_w, xvimagesink->scr_h,
    xvimagesink->xwindow->x, xvimagesink->xwindow->y,
    xvimagesink->xwindow->width, xvimagesink->xwindow->height,
    xvimagesink->render_rect.x, xvimagesink->render_rect.y,
    xvimagesink->render_rect.w, xvimagesink->render_rect.h);
#else /* GST_EXT_XV_ENHANCEMENT */
  XGetWindowAttributes (xvimagesink->xcontext->disp,
      xvimagesink->xwindow->win, &attr);

  xvimagesink->xwindow->width = attr.width;
  xvimagesink->xwindow->height = attr.height;

  if (!xvimagesink->have_render_rect) {
    xvimagesink->render_rect.x = xvimagesink->render_rect.y = 0;
    xvimagesink->render_rect.w = attr.width;
    xvimagesink->render_rect.h = attr.height;
  }
#endif /* GST_EXT_XV_ENHANCEMENT */

  g_mutex_unlock (xvimagesink->x_lock);
}

static void
gst_xvimagesink_xwindow_clear (GstXvImageSink * xvimagesink,
    GstXWindow * xwindow)
{
  g_return_if_fail (xwindow != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  g_mutex_lock (xvimagesink->x_lock);

  if (xvimagesink->get_pixmap_cb == NULL) // No need to avoc setting for pixmap case.
  {
    mute_video_display(xvimagesink, TRUE, MUTE_DEFAULT);
    gst_xvimagesink_avoc_stop(xvimagesink);
  }

  XvStopVideo (xvimagesink->xcontext->disp, xvimagesink->xcontext->xv_port_id,
      xwindow->win);
  XSync (xvimagesink->xcontext->disp, FALSE);

  g_mutex_unlock (xvimagesink->x_lock);
}

/* This function commits our internal colorbalance settings to our grabbed Xv
   port. If the xcontext is not initialized yet it simply returns */
static void
gst_xvimagesink_update_colorbalance (GstXvImageSink * xvimagesink)
{
  GList *channels = NULL;

  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  /* If we haven't initialized the X context we can't update anything */
  if (xvimagesink->xcontext == NULL)
    return;

  /* Don't set the attributes if they haven't been changed, to avoid
   * rounding errors changing the values */
  if (!xvimagesink->cb_changed)
    return;

  /* For each channel of the colorbalance we calculate the correct value
     doing range conversion and then set the Xv port attribute to match our
     values. */
  channels = xvimagesink->xcontext->channels_list;

  while (channels) {
    if (channels->data && GST_IS_COLOR_BALANCE_CHANNEL (channels->data)) {
      GstColorBalanceChannel *channel = NULL;
      Atom prop_atom;
      gint value = 0;
      gdouble convert_coef;

      channel = GST_COLOR_BALANCE_CHANNEL (channels->data);
      g_object_ref (channel);

      /* Our range conversion coef */
      convert_coef = (channel->max_value - channel->min_value) / 2000.0;

      if (g_ascii_strcasecmp (channel->label, "XV_HUE") == 0) {
        value = xvimagesink->hue;
      } else if (g_ascii_strcasecmp (channel->label, "XV_SATURATION") == 0) {
        value = xvimagesink->saturation;
      } else if (g_ascii_strcasecmp (channel->label, "XV_CONTRAST") == 0) {
        value = xvimagesink->contrast;
      } else if (g_ascii_strcasecmp (channel->label, "XV_BRIGHTNESS") == 0) {
        value = xvimagesink->brightness;
      } else {
        g_warning ("got an unknown channel %s", channel->label);
        g_object_unref (channel);
        return;
      }

      /* Committing to Xv port */
      g_mutex_lock (xvimagesink->x_lock);
      prop_atom =
          XInternAtom (xvimagesink->xcontext->disp, channel->label, True);
      if (prop_atom != None) {
        int xv_value;
        xv_value =
            floor (0.5 + (value + 1000) * convert_coef + channel->min_value);
        XvSetPortAttribute (xvimagesink->xcontext->disp,
            xvimagesink->xcontext->xv_port_id, prop_atom, xv_value);
      }
      g_mutex_unlock (xvimagesink->x_lock);

      g_object_unref (channel);
    }
    channels = g_list_next (channels);
  }
}

/* This function handles XEvents that might be in the queue. It generates
   GstEvent that will be sent upstream in the pipeline to handle interactivity
   and navigation. It will also listen for configure events on the window to
   trigger caps renegotiation so on the fly software scaling can work. */
static void
gst_xvimagesink_handle_xevents (GstXvImageSink * xvimagesink)
{
  XEvent e;
  guint pointer_x = 0, pointer_y = 0;
  gboolean pointer_moved = FALSE;
  gboolean exposed = FALSE, configured = FALSE;

  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  /* Handle Interaction, produces navigation events */

  /* We get all pointer motion events, only the last position is
     interesting. */
  g_mutex_lock (xvimagesink->flow_lock);
  g_mutex_lock (xvimagesink->x_lock);
  while (XCheckWindowEvent (xvimagesink->xcontext->disp,
          xvimagesink->xwindow->win, PointerMotionMask, &e)) {
    g_mutex_unlock (xvimagesink->x_lock);
    g_mutex_unlock (xvimagesink->flow_lock);

    switch (e.type) {
      case MotionNotify:
        pointer_x = e.xmotion.x;
        pointer_y = e.xmotion.y;
        pointer_moved = TRUE;
        break;
      default:
        break;
    }
    g_mutex_lock (xvimagesink->flow_lock);
    g_mutex_lock (xvimagesink->x_lock);
  }
  if (pointer_moved) {
    g_mutex_unlock (xvimagesink->x_lock);
    g_mutex_unlock (xvimagesink->flow_lock);

    GST_DEBUG ("xvimagesink pointer moved over window at %d,%d",
        pointer_x, pointer_y);
    gst_navigation_send_mouse_event (GST_NAVIGATION (xvimagesink),
        "mouse-move", 0, e.xbutton.x, e.xbutton.y);

    g_mutex_lock (xvimagesink->flow_lock);
    g_mutex_lock (xvimagesink->x_lock);
  }

  /* We get all events on our window to throw them upstream */
  while (XCheckWindowEvent (xvimagesink->xcontext->disp,
          xvimagesink->xwindow->win,
          KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask,
          &e)) {
    KeySym keysym;

    /* We lock only for the X function call */
    g_mutex_unlock (xvimagesink->x_lock);
    g_mutex_unlock (xvimagesink->flow_lock);

    switch (e.type) {
      case ButtonPress:
        /* Mouse button pressed over our window. We send upstream
           events for interactivity/navigation */
        GST_DEBUG ("xvimagesink button %d pressed over window at %d,%d",
            e.xbutton.button, e.xbutton.x, e.xbutton.y);
        gst_navigation_send_mouse_event (GST_NAVIGATION (xvimagesink),
            "mouse-button-press", e.xbutton.button, e.xbutton.x, e.xbutton.y);
        break;
      case ButtonRelease:
        /* Mouse button released over our window. We send upstream
           events for interactivity/navigation */
        GST_DEBUG ("xvimagesink button %d released over window at %d,%d",
            e.xbutton.button, e.xbutton.x, e.xbutton.y);
        gst_navigation_send_mouse_event (GST_NAVIGATION (xvimagesink),
            "mouse-button-release", e.xbutton.button, e.xbutton.x, e.xbutton.y);
        break;
      case KeyPress:
      case KeyRelease:
        /* Key pressed/released over our window. We send upstream
           events for interactivity/navigation */
        GST_DEBUG ("xvimagesink key %d pressed over window at %d,%d",
            e.xkey.keycode, e.xkey.x, e.xkey.y);
        g_mutex_lock (xvimagesink->x_lock);
        keysym = XKeycodeToKeysym (xvimagesink->xcontext->disp,
            e.xkey.keycode, 0);
        g_mutex_unlock (xvimagesink->x_lock);
        if (keysym != NoSymbol) {
          char *key_str = NULL;

          g_mutex_lock (xvimagesink->x_lock);
          key_str = XKeysymToString (keysym);
          g_mutex_unlock (xvimagesink->x_lock);
          gst_navigation_send_key_event (GST_NAVIGATION (xvimagesink),
              e.type == KeyPress ? "key-press" : "key-release", key_str);
        } else {
          gst_navigation_send_key_event (GST_NAVIGATION (xvimagesink),
              e.type == KeyPress ? "key-press" : "key-release", "unknown");
        }
        break;
      default:
        GST_DEBUG ("xvimagesink unhandled X event (%d)", e.type);
    }
    g_mutex_lock (xvimagesink->flow_lock);
    g_mutex_lock (xvimagesink->x_lock);
  }

  /* Handle Expose */
  while (XCheckWindowEvent (xvimagesink->xcontext->disp,
          xvimagesink->xwindow->win, ExposureMask | StructureNotifyMask, &e)) {
    switch (e.type) {
      case Expose:
        exposed = TRUE;
        break;
      case ConfigureNotify:
        g_mutex_unlock (xvimagesink->x_lock);
#ifdef GST_EXT_XV_ENHANCEMENT
        GST_INFO_OBJECT (xvimagesink, "Call gst_xvimagesink_xwindow_update_geometry!");
#endif /* GST_EXT_XV_ENHANCEMENT */
        gst_xvimagesink_xwindow_update_geometry (xvimagesink);
        g_mutex_lock (xvimagesink->x_lock);
        configured = TRUE;
        break;
      default:
        break;
    }
  }

  if (xvimagesink->handle_expose && (exposed || configured)) {
    g_mutex_unlock (xvimagesink->x_lock);
    g_mutex_unlock (xvimagesink->flow_lock);
    gst_xvimagesink_expose (GST_X_OVERLAY (xvimagesink));
	if(xvimagesink->xwindow)
	{
		if((xvimagesink->xwindow->width > 1) && (xvimagesink->xwindow->height > 1))
		{
		  mute_video_display(xvimagesink, FALSE, MUTE_INVALID_WINSIZE);
		}
		else
		{
		  mute_video_display(xvimagesink, TRUE, MUTE_INVALID_WINSIZE);
		}
	}
	exposed = FALSE;
	configured = FALSE;
    g_mutex_lock (xvimagesink->flow_lock);
    g_mutex_lock (xvimagesink->x_lock);
  }

  /* Handle Display events */
  int num_event =0;
  while (XPending (xvimagesink->xcontext->disp)) {
    XNextEvent (xvimagesink->xcontext->disp, &e);
    GST_DEBUG_OBJECT(xvimagesink, "num of event[ %d ], type[ %d ] [ %d, %d ]", ++num_event, e.type, ClientMessage, VisibilityNotify );

    switch (e.type) {
      case ClientMessage:{
        Atom wm_delete;

        wm_delete = XInternAtom (xvimagesink->xcontext->disp,
            "WM_DELETE_WINDOW", True);
        if (wm_delete != None && wm_delete == (Atom) e.xclient.data.l[0]) {
          /* Handle window deletion by posting an error on the bus */
          GST_ELEMENT_ERROR (xvimagesink, RESOURCE, NOT_FOUND,
              ("Output window was closed"), (NULL));

          g_mutex_unlock (xvimagesink->x_lock);
          gst_xvimagesink_xwindow_destroy (xvimagesink, xvimagesink->xwindow);
          xvimagesink->xwindow = NULL;
          g_mutex_lock (xvimagesink->x_lock);
        }
        break;
      }
#ifdef GST_EXT_XV_ENHANCEMENT
      case VisibilityNotify:
        if (e.xvisibility.window == xvimagesink->xwindow->win) {
          if (e.xvisibility.state == VisibilityFullyObscured) {
            Atom atom_stream;

            GST_INFO_OBJECT(xvimagesink, "current window is FULLY HIDED");

            xvimagesink->is_hided = TRUE;
			mute_video_display(xvimagesink, TRUE, MUTE_STATE_HIDE);
            atom_stream = XInternAtom(xvimagesink->xcontext->disp,
                                      "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF", False);
            if (atom_stream != None) {
              if (XvSetPortAttribute(xvimagesink->xcontext->disp,
                                     xvimagesink->xcontext->xv_port_id,
                                     atom_stream, 0) != Success) {
                GST_WARNING_OBJECT(xvimagesink, "STREAM OFF failed");
              }
              XSync(xvimagesink->xcontext->disp, FALSE);
            } else {
              GST_WARNING_OBJECT(xvimagesink, "atom_stream is NONE");
            }
          } else {
            GST_INFO_OBJECT(xvimagesink, "current window is SHOWN");

            if (xvimagesink->is_hided) {
              g_mutex_unlock(xvimagesink->x_lock);
              g_mutex_unlock(xvimagesink->flow_lock);

              xvimagesink->is_hided = FALSE;
			  mute_video_display(xvimagesink, FALSE, MUTE_STATE_HIDE);
              gst_xvimagesink_expose(GST_X_OVERLAY(xvimagesink));

              g_mutex_lock(xvimagesink->flow_lock);
              g_mutex_lock(xvimagesink->x_lock);
            } else {
              GST_INFO_OBJECT(xvimagesink, "current window is not HIDED, skip this event");
            }
          }
        }
        break;
#endif /* GST_EXT_XV_ENHANCEMENT */
		case Expose:
		  exposed = TRUE;
		  break;
		case ConfigureNotify:
		  g_mutex_unlock (xvimagesink->x_lock);
		#ifdef GST_EXT_XV_ENHANCEMENT
		  GST_ERROR_OBJECT (xvimagesink, "Call gst_xvimagesink_xwindow_update_geometry !");
		#endif /* GST_EXT_XV_ENHANCEMENT */
		  gst_xvimagesink_xwindow_update_geometry (xvimagesink);
		  g_mutex_lock (xvimagesink->x_lock);
		  configured = TRUE;
		  break;
      default:
        break;
    }
  }

  g_mutex_unlock (xvimagesink->x_lock);
  g_mutex_unlock (xvimagesink->flow_lock);

  if (xvimagesink->handle_expose && (exposed || configured)) {
    gst_xvimagesink_expose (GST_X_OVERLAY (xvimagesink));
	if(xvimagesink->xwindow)
	{
		if((xvimagesink->xwindow->width > 1) && (xvimagesink->xwindow->height > 1))
		{
		  mute_video_display(xvimagesink, FALSE, MUTE_INVALID_WINSIZE);
		}
		else
		{
		  mute_video_display(xvimagesink, TRUE, MUTE_INVALID_WINSIZE);
		}
	}
  }
}

#ifdef XV_WEBKIT_PIXMAP_SUPPORT
static gboolean
check_x_extension (GstXvImageSink * xvimagesink)
{
  gint base, err_base;
  gint major, minor;
  Display *dpy;

  g_return_val_if_fail (xvimagesink != NULL, FALSE);
  g_return_val_if_fail ((xvimagesink->xcontext != NULL && xvimagesink->xcontext->disp != NULL), FALSE);

  dpy = xvimagesink->xcontext->disp;
  if (!DRI2QueryExtension (dpy, &base, &err_base)) {
    GST_ERROR_OBJECT (xvimagesink, "error: no DRI2 extension!");
    return FALSE;
  }
  if (!DRI2QueryVersion (dpy, &major, &minor)) {
    GST_ERROR_OBJECT (xvimagesink, "error: DRI2QueryVersion failed!");
    return FALSE;
  }
  if (!XFixesQueryExtension (dpy, &base, &err_base)) {
    GST_ERROR_OBJECT (xvimagesink, "error: no XFixes extension!");
    return FALSE;
  }
  if (!XFixesQueryVersion (dpy, &major, &minor)) {
    GST_ERROR_OBJECT (xvimagesink, "error: DRI2QueryVersion failed!");
    return FALSE;
  }

  return TRUE;
}

/* To get the buffer object of a pixmap */
static tbm_bo
get_pixmap_bo (GstXvImageSink * xvimagesink, TBMinfoPTR tbmptr)
{
  guint attachments[1];
  gint dri2_count, dri2_out_count;
  gint dri2_width, dri2_height, dri2_stride;
  DRI2Buffer *dri2_buffers = NULL;
  tbm_bo bo = NULL;
  Display *dpy = NULL;
  Pixmap pixmap;

  g_return_val_if_fail (xvimagesink != NULL, NULL);
  g_return_val_if_fail (tbmptr != NULL, NULL);
  g_return_val_if_fail ((xvimagesink->xcontext != NULL && xvimagesink->xcontext->disp != NULL), NULL);
  g_return_val_if_fail ((xvimagesink->xpixmap[xvimagesink->current_pixmap_idx] && xvimagesink->set_pixmap == TRUE), NULL);

  dpy = xvimagesink->xcontext->disp;
  pixmap = xvimagesink->xpixmap[xvimagesink->current_pixmap_idx]->pixmap;

  DRI2CreateDrawable (dpy, pixmap);

  attachments[0] = DRI2BufferFrontLeft;
  dri2_count = 1;
  dri2_buffers = DRI2GetBuffers (dpy, pixmap, &dri2_width, &dri2_height, attachments, dri2_count, &dri2_out_count);
  fprintf(stderr, "get_pixmap_bo: %x\n", dri2_buffers->flags);
  if (!dri2_buffers || dri2_buffers[0].name <= 0){
    GST_ERROR_OBJECT (xvimagesink, "error: DRI2GetBuffers failed!");
    goto fail_get;
  }

  /* real buffer of pixmap */
  bo = tbm_bo_import (tbmptr->bufmgr, dri2_buffers[0].name);
  if (!bo){
    GST_ERROR_OBJECT (xvimagesink, "error: tbm_bo_import failed!");
    goto fail_get;
  }
  dri2_stride = dri2_buffers[0].pitch;

  GST_DEBUG_OBJECT (xvimagesink, "pixmap: w(%d) h(%d) pitch(%d) cpp(%d) flink_id(%d)\n",
    dri2_width, dri2_height,
    dri2_buffers[0].pitch,
    dri2_buffers[0].cpp,
    dri2_buffers[0].name);

  free (dri2_buffers);
  return bo;

fail_get:
  if (dri2_buffers)
    free (dri2_buffers);
  return NULL;
}

/* To get the region of a pixmap */
static XserverRegion
get_pixmap_region (GstXvImageSink * xvimagesink)
{
  Window root;
  gint x, y;
  guint width, height, border, depth;
  XRectangle rect;
  Display *dpy;
  Pixmap pixmap;

  g_return_val_if_fail (xvimagesink != NULL, 0);
  g_return_val_if_fail ((xvimagesink->xcontext != NULL && xvimagesink->xcontext->disp != NULL), 0);
  g_return_val_if_fail ((xvimagesink->xpixmap[xvimagesink->current_pixmap_idx] && xvimagesink->set_pixmap == TRUE), NULL);

  dpy = xvimagesink->xcontext->disp;
  pixmap = xvimagesink->xpixmap[xvimagesink->current_pixmap_idx]->pixmap;

  if (!XGetGeometry (dpy, pixmap, &root, &x, &y, &width, &height, &border, &depth))
    return 0;

  rect.x = 0;
  rect.y = 0;
  rect.width = width;
  rect.height = height;
  return XFixesCreateRegion (dpy, &rect, 1);
}

/* This function generates a caps for all supported format by the internal
   color convert routine, which converts the incoming color formats to what
   pixmap supports. We store each one of the supported formats in a
   format list and append the format to a newly created caps that we return
*/
static GstCaps *
get_pixmap_support (GstXvImageSink * xvimagesink,
    GstXContext * xcontext)
{
  GstCaps *pixmap_caps = NULL, *pixmap_hw_caps = NULL, *pixmap_sw_caps = NULL;
  GstXvImageFormat *format = NULL;

  g_return_val_if_fail (xvimagesink != NULL, NULL);
  g_return_val_if_fail (xcontext != NULL, NULL);

  if (!xvimagesink->get_pixmap_cb) {
    GST_WARNING_OBJECT (xvimagesink, "warning: No pixmap callback pointer set, so no extra pixmap support !");
    return NULL;
  }

  GST_DEBUG_OBJECT (xvimagesink, "Now create frame formats caps that webkit pixmap supports !");

  /* add pixmap supported hw caps. */
  pixmap_hw_caps = gst_caps_new_simple ("video/x-raw-yuv",
    "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('S', 'T', 'V', '0'),
    "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
    "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
    "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
  if (pixmap_hw_caps) {
    GST_DEBUG_OBJECT (xvimagesink, "Generated the pixmap supported caps(hw): %" GST_PTR_FORMAT, pixmap_hw_caps);
    format = g_new0 (GstXvImageFormat, 1);
    if (format) {
      format->format = GST_MAKE_FOURCC('S', 'T', 'V', '0');
      format->caps = gst_caps_copy (pixmap_hw_caps);
      xcontext->formats_list = g_list_append (xcontext->formats_list, format);
    }
    /* for pixmap_caps always NULL, remove DEADLINE for prevent, w.you, DF150113-00797, 2015/1/15
	if (!pixmap_caps)
      pixmap_caps = pixmap_hw_caps;
    else
      gst_caps_append (pixmap_caps, pixmap_hw_caps);
    */
    pixmap_caps = pixmap_hw_caps; /* for pixmap_caps always NULL, remove DEADLINE and add this line for prevent, w.you, DF150113-00797, 2015/1/15 */
  }

  /* add pixmap supported sw caps. */
  pixmap_sw_caps = gst_caps_new_simple ("video/x-raw-yuv",
    "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('S', 'T', 'V', '1'),
    "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
    "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
    "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
  if (pixmap_sw_caps) {
    GST_DEBUG_OBJECT (xvimagesink, "Generated the pixmap supported caps(sw): %" GST_PTR_FORMAT, pixmap_sw_caps);
    format = g_new0 (GstXvImageFormat, 1);
    if (format) {
      format->format = GST_MAKE_FOURCC('S', 'T', 'V', '1');
      format->caps = gst_caps_copy (pixmap_sw_caps);
      xcontext->formats_list = g_list_append (xcontext->formats_list, format);
    }
	if (!pixmap_caps)
      pixmap_caps = pixmap_sw_caps;
    else
      gst_caps_append (pixmap_caps, pixmap_sw_caps);
  }

  GST_DEBUG_OBJECT (xvimagesink, "pixmap supported caps(sw): %" GST_PTR_FORMAT, pixmap_caps);

  if (gst_caps_is_empty (pixmap_caps)) {
    gst_caps_unref (pixmap_caps);
    GST_ELEMENT_ERROR (xvimagesink, STREAM, WRONG_TYPE, (NULL), ("No supported format found"));
    return NULL;
  }

  return pixmap_caps;
}

static gboolean
prepare_pixmap_tbm_buffers(GstXvImageBuffer * xvimage)
{
  GstXvImageSink * xvimagesink = NULL;
  TBMinfoPTR tbmptr = NULL;
  guint w, h;
  GstXPixmap *pm = NULL;

  g_return_val_if_fail (xvimage != NULL, FALSE);
  g_return_val_if_fail (xvimage->xvimagesink != NULL, FALSE);
  xvimagesink = xvimage->xvimagesink;
  g_return_val_if_fail ((xvimagesink->xpixmap[xvimagesink->current_pixmap_idx] && xvimagesink->set_pixmap == TRUE), FALSE);

  pm = xvimagesink->xpixmap[xvimagesink->current_pixmap_idx];

  if (!xvimagesink->get_pixmap_cb) {
    GST_WARNING_OBJECT (xvimagesink,"warning: No pixmap callback pointer set, so no extra pixmap support !");
    return FALSE;
  }
  if (!xvimage->pTBMinfo_web) {
    GST_LOG_OBJECT (xvimagesink,"TBMinfo didn't created yet, so we allocate it now !");
    xvimage->pTBMinfo_web = g_malloc (sizeof(TBMinfo));
    if (!xvimage->pTBMinfo_web)
      return FALSE;
    memset(xvimage->pTBMinfo_web, 0, sizeof(TBMinfo));
  }
  tbmptr = (TBMinfoPTR)(xvimage->pTBMinfo_web);

  if (tbmptr->drm_fd > 0 && tbmptr->bufmgr) {
    GST_LOG_OBJECT (xvimagesink,"TBM scaling buffer has been prepared already!");
    return TRUE;
  }
  tbmptr->drm_fd = -1;

  if (!check_x_extension (xvimagesink)) {
    GST_ERROR_OBJECT (xvimagesink, "error: check x extension failed!");
    return FALSE;
  }
  if (!init_tbm_bufmgr(&tbmptr->drm_fd, &tbmptr->bufmgr, xvimage->xvimagesink->xcontext->disp)) {
    GST_ERROR_OBJECT (xvimagesink, "error: init drm failed!");
    return FALSE;
  }

  GST_DEBUG_OBJECT (xvimagesink, "now, start to prepare the TBM APs for Y, CbCr and RGB respectively!");

  /* allocate and map AP memory by TBM */
  /* the width and height are that the target pixmap.*/
  tbmptr->idx = 0;
  w = pm->width;
  h = pm->height;

  GST_DEBUG_OBJECT (xvimagesink,"the width(%d) and height(%d) for the pixmap.", w, h);
  tbmptr->boY = tbm_bo_alloc(tbmptr->bufmgr, w*h, TBM_BO_SCANOUT);
  if (!tbmptr->boY)
	goto tbm_prepare_failed;
  tbmptr->bo_hnd_Y = tbm_bo_map(tbmptr->boY, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
  if (!tbmptr->bo_hnd_Y.ptr)
    goto tbm_prepare_failed; 
  tbmptr->pScaledY = (unsigned char *) memalign(4096, w*h);
  GST_DEBUG_OBJECT (xvimagesink,"the ScaledY buffer(%p) allocated!.", tbmptr->pScaledY);

  tbmptr->boPixmap = get_pixmap_bo(xvimagesink, tbmptr);
  if (!tbmptr->boPixmap)
	goto tbm_prepare_failed;
  tbmptr->region = get_pixmap_region (xvimagesink);
  if (tbmptr->region == 0)
	goto tbm_prepare_failed;

  /*if the value of width is odd number.*/
  w = (w & 0x1) ? ((w >> 1) + 1) : (w >> 1);
  tbmptr->boCbCr = tbm_bo_alloc(tbmptr->bufmgr, w*h /*YUV420 : w*h/2*/, TBM_BO_SCANOUT);
  if (!tbmptr->boCbCr)
	goto tbm_prepare_failed;
  tbmptr->bo_hnd_CbCr = tbm_bo_map(tbmptr->boCbCr, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
  if (!tbmptr->bo_hnd_CbCr.ptr)
    goto tbm_prepare_failed;

  tbmptr->pScaledCbCr = (unsigned char *) memalign(4096, w*h);
  GST_DEBUG_OBJECT (xvimagesink,"the ScaledCbCr buffer(%p) allocated!.", tbmptr->pScaledCbCr);

  tbmptr->idx = -1;
  tbmptr->pY = (unsigned char*)tbmptr->bo_hnd_Y.ptr;
  tbmptr->pCbCr = (unsigned char*)tbmptr->bo_hnd_CbCr.ptr;

  return TRUE;

tbm_prepare_failed:
  GST_ERROR_OBJECT (xvimagesink,"prepare TBM scaling buffer failed!");
  if (tbmptr->bo_hnd_Y.ptr) {
    tbm_bo_unmap(tbmptr->boY);
    tbmptr->bo_hnd_Y.ptr = NULL;
  }
  if (tbmptr->boY) {
    tbm_bo_unref(tbmptr->boY);
    tbmptr->boY = NULL;
  }
  if (tbmptr->bo_hnd_CbCr.ptr) {
    tbm_bo_unmap(tbmptr->boCbCr);
    tbmptr->bo_hnd_CbCr.ptr = NULL;
  }
  if (tbmptr->boCbCr) {
    tbm_bo_unref(tbmptr->boCbCr);
    tbmptr->boCbCr = NULL;
  }
  if (tbmptr->boPixmap) {
    tbm_bo_unref(tbmptr->boPixmap);
    tbmptr->boPixmap = NULL;
  }

  if (tbmptr->bufmgr)
    tbm_bufmgr_deinit(tbmptr->bufmgr);
  tbmptr->bufmgr = NULL;
  if (tbmptr->drm_fd > 0)
    close(tbmptr->drm_fd);
  tbmptr->drm_fd = -1;

  return FALSE;
}

static void
unprepare_pixmap_tbm_buffers(GstXvImageBuffer * xvimage)
{
  TBMinfoPTR tbmptr = NULL;

  g_return_if_fail (xvimage != NULL);
  g_return_if_fail (xvimage->xvimagesink != NULL);
  g_return_if_fail ((xvimage->xvimagesink->xcontext != NULL && xvimage->xvimagesink->xcontext->disp != NULL));

  GST_DEBUG_OBJECT (xvimage->xvimagesink, "unprepare the TBM APs for Y, CbCr and RGB respectively! pTBMinfo_web[%p]", xvimage->pTBMinfo_web);
  if (!xvimage->pTBMinfo_web)
    return;
  tbmptr = (TBMinfoPTR)(xvimage->pTBMinfo_web);
  if (tbmptr->bo_hnd_Y.ptr)
    tbm_bo_unmap(tbmptr->boY);
  if (tbmptr->boY)
    tbm_bo_unref(tbmptr->boY);
  if (tbmptr->bo_hnd_CbCr.ptr)
    tbm_bo_unmap(tbmptr->boCbCr);
  if (tbmptr->boCbCr)
    tbm_bo_unref(tbmptr->boCbCr);
  if (tbmptr->boPixmap)
    tbm_bo_unref(tbmptr->boPixmap);
  if (tbmptr->region)
    XFixesDestroyRegion (xvimage->xvimagesink->xcontext->disp, tbmptr->region);
  if (tbmptr->pScaledY)
    g_free(tbmptr->pScaledY);
  if (tbmptr->pScaledCbCr)
    g_free(tbmptr->pScaledCbCr);

  if (tbmptr->bufmgr)
    tbm_bufmgr_deinit(tbmptr->bufmgr);

  if (tbmptr->drm_fd > 0)
    close(tbmptr->drm_fd);

  g_free (xvimage->pTBMinfo_web);
  xvimage->pTBMinfo_web = NULL;
}

 static tbm_bo
 get_decoded_frame_bo(GstXvImageBuffer * xvimage, GstBuffer * buf)
{
  gint format;
  tbm_bo boAP = NULL;
  tbm_bo_handle boAP_hd = {0};
  GstXvImageSink * xvimagesink = NULL;
  TBMinfoPTR tbmptr = NULL;
  const GstStructure* structure = NULL;
  guint boAP_key = 0;

  g_return_val_if_fail (xvimage != NULL, NULL);
  g_return_val_if_fail (xvimage->xvimagesink != NULL, NULL);
  g_return_val_if_fail (buf != NULL, NULL);

  xvimagesink = xvimage->xvimagesink;

  if (!xvimagesink->get_pixmap_cb) {
    GST_WARNING_OBJECT (xvimagesink,"warning: No pixmap callback pointer set, so no extra pixmap support !");
    return NULL;
  }
  if (!xvimage->pTBMinfo_web) {
    GST_ERROR_OBJECT(xvimagesink, "error: TBM scaling buffer is not prepared yet");
    return NULL;
  }
  tbmptr = (TBMinfoPTR)(xvimage->pTBMinfo_web);

  if (!tbmptr->drm_fd || !tbmptr->bufmgr) {
    GST_ERROR_OBJECT(xvimagesink, "error: TBM scaling buffer is not prepared yet");
    return NULL;
  }

  format = gst_xvimagesink_get_format_from_caps(xvimagesink, GST_BUFFER_CAPS(buf));
  if (xvimage->im_format != format || format != GST_MAKE_FOURCC('S', 'T', 'V', '1')) {
    GST_ERROR_OBJECT(xvimagesink, "error: the format of the buffer is not 'STV1'!");
    return NULL;
  }

  GST_DEBUG_OBJECT (xvimagesink, "get the bo from buffer qdata!");
  structure = gst_buffer_get_qdata(buf, g_quark_from_string("tbm_bo"));
  if (!structure || !gst_structure_get_uint(structure, "tbm_bo_key", &boAP_key)) {
    GST_ERROR_OBJECT(xvimagesink, "error: get bo from buffer qdata failed!");
    return NULL;
  }
  boAP = tbm_bo_import (tbmptr->bufmgr, boAP_key);
  if (!boAP) {
    GST_ERROR_OBJECT(xvimagesink, "error: the bo is NULL!");
    return NULL;
  }

  boAP_hd = tbm_bo_get_handle(boAP, TBM_DEVICE_CPU);
  if(!boAP_hd.ptr) {
    GST_ERROR_OBJECT (xvimagesink, "Fail to get the boAP_hd.ptr 0x%x !!! ", boAP_hd.ptr);
    return NULL;
  }
  GST_LOG_OBJECT (xvimagesink, "got the bo key(%u), bo(%p), bo_handle(%x) and bo_handle.ptr(%p)!",
    boAP_key, boAP, boAP_hd, boAP_hd.ptr);

  return boAP;
}

static gboolean
do_sw_tbm_scaling(GstXvImageBuffer * xvimage, tbm_bo ap_bo)
{
  GstXvImageSink * xvimagesink = NULL;
  TBMinfoPTR tbmptr = NULL;
  tbm_ga_scale_wrap scale_wrap;
  gint ret;
  GstXPixmap *pm = NULL;

  g_return_val_if_fail (xvimage != NULL, FALSE);
  g_return_val_if_fail (xvimage->xvimagesink != NULL, FALSE);
  g_return_val_if_fail (ap_bo != NULL, FALSE);
  xvimagesink = xvimage->xvimagesink;
  g_return_val_if_fail ((xvimagesink->xpixmap[xvimagesink->current_pixmap_idx] && xvimagesink->set_pixmap == TRUE), FALSE);

  pm = xvimagesink->xpixmap[xvimagesink->current_pixmap_idx];

  if (!xvimagesink->get_pixmap_cb) {
    GST_WARNING_OBJECT (xvimagesink,"warning: No pixmap callback pointer set, so no extra pixmap support !");
    return FALSE;
  }
  if (!xvimage->pTBMinfo_web) {
    GST_ERROR_OBJECT(xvimagesink, "error: TBM scaling buffer is not prepared yet");
    return FALSE;
  }
  tbmptr = (TBMinfoPTR)(xvimage->pTBMinfo_web);

  if (!tbmptr->drm_fd || !tbmptr->bufmgr) {
    GST_ERROR_OBJECT(xvimagesink, "error: TBM scaling buffer is not prepared yet");
    return FALSE;
  }
  if(xvimage->im_format != (GST_MAKE_FOURCC('S','T','V','1'))) {
    GST_ERROR_OBJECT(xvimagesink, "error: video frame is not 'STV1' format!");
    return FALSE;
  }

  memset(&scale_wrap, 0, sizeof(tbm_ga_scale_wrap));
  scale_wrap.bufmgr = tbmptr->bufmgr;

  GST_LOG_OBJECT(xvimagesink, "start to scale Y!xvimage width(%d), height(%d), xvimagesink width(%d), height(%d)",
    xvimage->width, xvimage->height, xvimagesink->video_width, xvimagesink->video_height);

  /* scale Y */
  scale_wrap.src_bo = ap_bo;
  scale_wrap.dst_bo = tbmptr->boY;
  scale_wrap.src_paddr = NULL;
  scale_wrap.dst_paddr = NULL;
  scale_wrap.scale.color_mode = TBM_GA_FORMAT_8BPP;
  scale_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
  scale_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
  scale_wrap.scale.src_hbyte_size = xvimage->width * TBM_BPP8; /* Y line size */
  scale_wrap.scale.src_rect.x = 0;
  scale_wrap.scale.src_rect.y = 0; /* Y: Top of buffer (w*h) */
  scale_wrap.scale.src_rect.w = xvimage->width;
  scale_wrap.scale.src_rect.h = xvimage->height;
  scale_wrap.scale.dst_hbyte_size = pm->width * TBM_BPP8;
  scale_wrap.scale.dst_rect.x = 0;
  scale_wrap.scale.dst_rect.y = 0;
  scale_wrap.scale.dst_rect.w = pm->width; /* the width and height of the target pixmap.*/
  scale_wrap.scale.dst_rect.h = pm->height;
  scale_wrap.scale.rop_ca_value = 0;
  scale_wrap.scale.src_key = 0;
  scale_wrap.scale.rop_on_off = 0;
  ret = tbm_bo_ga_scale(&scale_wrap);
  if (!ret) {
    GST_ERROR_OBJECT (xvimagesink,"scaling Y failed! ret(%d)", ret);
    return ret;
  }

  /* scale CbCr */
  scale_wrap.src_bo = ap_bo;
  scale_wrap.dst_bo = tbmptr->boCbCr;
  scale_wrap.src_paddr = NULL;
  scale_wrap.dst_paddr = NULL;
  scale_wrap.scale.color_mode = TBM_GA_FORMAT_16BPP; /* Because of CbCr Interleaved */
  scale_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
  scale_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
  scale_wrap.scale.src_hbyte_size = xvimage->width * TBM_BPP8; /* for YUV420 interleaved case*/
  scale_wrap.scale.src_rect.x = 0;
  scale_wrap.scale.src_rect.y = xvimage->height; /* CbCr : Bottom of buffer (w*h/2) */
  scale_wrap.scale.src_rect.w = xvimage->width/2;
  scale_wrap.scale.src_rect.h = xvimage->height/2;
  scale_wrap.scale.dst_hbyte_size = pm->width * TBM_BPP8;
  scale_wrap.scale.dst_rect.x = 0;
  scale_wrap.scale.dst_rect.y = 0;
  scale_wrap.scale.dst_rect.w = pm->width/2; /* the width and height of the target pixmap.*/
  scale_wrap.scale.dst_rect.h = pm->height/2;
  scale_wrap.scale.rop_ca_value = 0;
  scale_wrap.scale.src_key = 0;
  scale_wrap.scale.rop_on_off = 0;
  ret = tbm_bo_ga_scale(&scale_wrap);
  if (!ret) {
    GST_ERROR_OBJECT (xvimagesink,"scaling CbCrfailed! ret(%d)", ret);
  }
  return ret;
}

static gboolean
do_hw_tbm_scaling(GstXvImageBuffer * xvimage, GstBuffer *buf)
{
  GstXvImageSink * xvimagesink = NULL;
  TBMinfoPTR tbmptr = NULL;
  tbm_ga_scale_wrap scale_wrap;
  gint ret;
  struct v4l2_drm *outinfo;
  struct v4l2_drm_dec_info *decinfo;
  struct v4l2_private_frame_info *frminfo;
  GstXPixmap *pm = NULL;

  g_return_val_if_fail (xvimage != NULL, FALSE);
  g_return_val_if_fail (xvimage->xvimagesink != NULL, FALSE);
  g_return_val_if_fail (buf != NULL, FALSE);
  xvimagesink = xvimage->xvimagesink;
  g_return_val_if_fail ((xvimagesink->xpixmap[xvimagesink->current_pixmap_idx] && xvimagesink->set_pixmap == TRUE), FALSE);

  pm = xvimagesink->xpixmap[xvimagesink->current_pixmap_idx];
  if (!xvimagesink->get_pixmap_cb) {
    GST_WARNING_OBJECT (xvimagesink,"warning: No pixmap callback pointer set, so no extra pixmap support !");
    return FALSE;
  }
  if (!xvimage->pTBMinfo_web) {
    GST_ERROR_OBJECT(xvimagesink, "error: TBM scaling buffer is not prepared yet");
    return FALSE;
  }
  tbmptr = (TBMinfoPTR)(xvimage->pTBMinfo_web);

  if (!tbmptr->drm_fd || !tbmptr->bufmgr) {
    GST_ERROR_OBJECT(xvimagesink, "error: TBM scaling buffer is not prepared yet");
    return FALSE;
  }
  if(xvimage->im_format != (GST_MAKE_FOURCC('S','T','V','0'))) {
    GST_ERROR_OBJECT(xvimagesink, "error: video frame is not 'STV0' format!");
    return FALSE;
  }

#if ENABLE_HW_TBM_SCALING_CHECKING
  FILE *outf = fopen("/tmp/xv.log", "a");
  struct timeval ts, te;
  gettimeofday(&ts, NULL);
#endif

  memset(&scale_wrap, 0, sizeof(tbm_ga_scale_wrap));
  scale_wrap.bufmgr = tbmptr->bufmgr;

#if ENABLE_PERFORMANCE_CHECKING
  GST_ERROR_OBJECT (xvimagesink, "the time before 'do_hw_tbm_scaling', cosumed %llu microseconds", get_time_diff());
#endif
  outinfo = (struct v4l2_drm*)(GST_BUFFER_MALLOCDATA(buf));
  decinfo = &(outinfo->u.dec_info);
  frminfo = &(outinfo->u.dec_info.pFrame[0]);

  GST_LOG_OBJECT(xvimagesink, "Use v4l2_drm w/h[ %d x %d ] instead of  caps,  start to scale Y!xvimage width(%d), height(%d), xvimagesink width(%d), height(%d) colorformat=[%d]",
    frminfo->width, frminfo->height, xvimage->width, xvimage->height, xvimagesink->video_width, xvimagesink->video_height, frminfo->colorformat);
  
  if ( frminfo->y_linesize ==0 || frminfo->u_linesize == 0 || frminfo->y_phyaddr == NULL || frminfo->u_phyaddr == NULL)
  {
    GST_WARNING_OBJECT (xvimagesink, "warning: the line size of hw frame is invalid: y_linesize(%d), u_linesize(%d) y_phyaddr[ %x ], u_phyaddr[ %x ]!",
      frminfo->y_linesize, frminfo->u_linesize, frminfo->y_phyaddr, frminfo->u_phyaddr);
  }

  /* convert 422 to 420 for mjpeg camera */
  if (frminfo->colorformat == V4L2_DRM_COLORFORMAT_YUV422)
  {
	  int i,j;
	  unsigned char *puvir = (unsigned char *)frminfo->u_viraddr;
  
	  for (i = 0, j = 1; j < frminfo->height; j += 2, i += frminfo->u_linesize) {
		  memcpy(puvir + i, puvir+(j * frminfo->u_linesize), frminfo->u_linesize);
	  }
  }

  /* scale Y */
  scale_wrap.src_bo = NULL;
  scale_wrap.dst_bo = tbmptr->boY;
  scale_wrap.src_paddr = frminfo->y_phyaddr;
  scale_wrap.dst_paddr = NULL;
  scale_wrap.scale.color_mode = TBM_GA_FORMAT_8BPP;
  scale_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
  scale_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
  scale_wrap.scale.src_hbyte_size = frminfo->y_linesize * TBM_BPP8; /* Y line size */
  scale_wrap.scale.src_rect.x = 0;
  scale_wrap.scale.src_rect.y = 0; /* Y: Top of buffer (w*h) */
  scale_wrap.scale.src_rect.w = frminfo->width;
  scale_wrap.scale.src_rect.h = frminfo->height;
  scale_wrap.scale.dst_hbyte_size = pm->width * TBM_BPP8;
  scale_wrap.scale.dst_rect.x = 0;
  scale_wrap.scale.dst_rect.y = 0;
  scale_wrap.scale.dst_rect.w = pm->width; /* the width and height of the target pixmap.*/
  scale_wrap.scale.dst_rect.h = pm->height;
  scale_wrap.scale.rop_ca_value = 0;
  scale_wrap.scale.src_key = 0;
  scale_wrap.scale.rop_on_off = 0;
  ret = tbm_bo_ga_scale(&scale_wrap);
  if (!ret) {
    GST_ERROR_OBJECT (xvimagesink,"scaling Y failed! ret(%d)", ret);
    return ret;
  }

  /* scale CbCr */
  scale_wrap.src_bo = NULL;
  scale_wrap.dst_bo = tbmptr->boCbCr;
  scale_wrap.src_paddr = frminfo->u_phyaddr;
  scale_wrap.dst_paddr = NULL;
  scale_wrap.scale.color_mode = TBM_GA_FORMAT_16BPP; /* Because of CbCr Interleaved */
  scale_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
  scale_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
  scale_wrap.scale.src_hbyte_size = frminfo->u_linesize * TBM_BPP8; /* for YUV420 interleaved case*/
  scale_wrap.scale.src_rect.x = 0;
  scale_wrap.scale.src_rect.y = 0;
  scale_wrap.scale.src_rect.w = frminfo->width/2;
  scale_wrap.scale.src_rect.h = frminfo->height/2;
  scale_wrap.scale.dst_hbyte_size = pm->width * TBM_BPP8;
  scale_wrap.scale.dst_rect.x = 0;
  scale_wrap.scale.dst_rect.y = 0;
  scale_wrap.scale.dst_rect.w = pm->width/2; /* the width and height of the target pixmap.*/
  scale_wrap.scale.dst_rect.h = pm->height/2;
  scale_wrap.scale.rop_ca_value = 0;
  scale_wrap.scale.src_key = 0;
  scale_wrap.scale.rop_on_off = 0;
  ret = tbm_bo_ga_scale(&scale_wrap);
  if (!ret) {
    GST_ERROR_OBJECT (xvimagesink,"scaling CbCr failed! ret(%d)", ret);
  }
#if ENABLE_PERFORMANCE_CHECKING
  GST_ERROR_OBJECT (xvimagesink, "the time after 'do_hw_tbm_scaling', cosumed %llu microseconds", get_time_diff());
#endif

#if ENABLE_HW_TBM_SCALING_CHECKING
  gettimeofday(&te, NULL);
  if (outf) {
	  flock(fileno(outf), LOCK_EX);
	  fprintf(outf, "scale: %u ms\n",
			  (te.tv_sec - ts.tv_sec) * 1000 +
			  (te.tv_usec - ts.tv_usec) / 1000);
	  flock(fileno(outf), LOCK_UN);
	  fclose(outf);
  }
#endif

  return ret;
}

/* Structure for passing arguments for color conversion via void *ptr to threads */
struct nv21_rgba_args {
	unsigned char *y;
	unsigned char *CbCr;
	int yLineSize;
	int width;
	int height;
	unsigned char *rgbaOut;
};

/* NEON version of conversion thread */
static void *nv21_neon_rgba_thread(void *arg) {
	struct nv21_rgba_args *nv21_args = (struct nv21_rgba_args *) arg;

	NV21_RGBA(
				nv21_args->y,
				nv21_args->CbCr,
				nv21_args->yLineSize,
				nv21_args->width,
				nv21_args->height,
				nv21_args->rgbaOut
				);
	return 0;
}

/* We assume some problems could occur due to memory alignment issues.
 * So real number of threads depends on successful alignment of input
 * and output buffers
 * */
//#define COLORSPACE_DEBUG_TO_STDERR
#ifdef COLORSPACE_DEBUG_TO_STDERR
#define DBGLOG(argv...) fprintf(stderr, ##argv)
#else
#define DBGLOG(argv...)
#endif

static bool do_colorspace_neon_conversion_in_threads(
		int threads, int alignment,
		guchar *y, guchar *cbcr,
		int yline, int width, int height,
		guchar *rgbaout)
{
	int i;
	bool convert_success = false, split_success;

	guchar *curr_y, *curr_cbcr, *curr_rgba;
	int delta_y, delta_cbcr, delta_h, delta_rgba;
	int height_even;

	struct nv21_rgba_args thargs[threads];
	pthread_t thr[threads];

	if (height & 0x1) //can't properly handle odd-height images
		return false;

	if (yline % alignment) //can't properly handle images with yline not aligned
		return false;

	DBGLOG("in args <threads: %d, alignment: %d>, "
			"<y: %p, cbcr: %p, yline: %d, width: %d, height: %d, rgbaout, %p\n",
			threads, alignment,
			y, cbcr, yline, width, height, rgbaout);

	do {
		curr_y = y;
		curr_cbcr = cbcr;
		curr_rgba = rgbaout;
		height_even = height / threads & (~1);
		delta_y = width * height_even;
		delta_cbcr = width * height_even / 2;
		delta_h = (height / threads) & (~1);
		delta_rgba = width * height_even * 4;

		DBGLOG("Trying to prepare split to %d threads\n", threads);
		DBGLOG("height_even: %d, delta_y: %d, delta_cbcr: %d, delta_h: %d, delta_rgba: %d\n",
				height_even, delta_y, delta_cbcr, delta_h, delta_rgba);

		split_success = true;
		for (i = 0; i < threads; i++) {
			thargs[i].y = curr_y + delta_y * i;
			thargs[i].CbCr = curr_cbcr + delta_cbcr * i;
			thargs[i].yLineSize = width;
			thargs[i].width = width;
			thargs[i].rgbaOut = curr_rgba + delta_rgba * i;					

			if (i != (threads-1)) {
				thargs[i].height = delta_h;
			} else {
				thargs[i].height = height - (delta_h * (threads-1));
			}

			DBGLOG("y: %p, cbcr: %p, linesize: %d, width: %d, height: %d, rgbaout: %p\n",
					thargs[i].y,
					thargs[i].CbCr,
					thargs[i].yLineSize,
					thargs[i].width,
					thargs[i].height,
					thargs[i].rgbaOut
					);

			/* Skip this split, we have not aligned addresses */
			if ((guint) thargs[i].y % alignment ||
				(guint) thargs[i].CbCr % alignment ||
				(guint) thargs[i].rgbaOut % alignment) {
				DBGLOG("Alignment %d for threads %d not match\n", alignment, threads);
				split_success = false;
				break;
			}
		}

		/* TODO: exiting threads if one of them can't be started */

		if (split_success) {
			DBGLOG("We've found valid split, threads: %d\n", threads);

			for (i = 0; i < threads; i++) {
				pthread_create(&thr[i], NULL, nv21_neon_rgba_thread, &thargs[i]);
			}
			for (i = 0; i < threads; i++) {
				pthread_join(thr[i], NULL);
			}

			convert_success = true;
			break;
		}

		threads--;
	} while (!convert_success && threads);

	return convert_success;
}

/* Software version (not NEON) of conversion thread */
static void *nv21_software_rgba_thread(void *arg) {
	struct nv21_rgba_args *nv21_args = (struct nv21_rgba_args *) arg;

	convert_yuv420_interleaved_to_argb(
				nv21_args->y,
				nv21_args->CbCr,
				nv21_args->yLineSize,
				nv21_args->width,
				nv21_args->width,
				nv21_args->height,
				nv21_args->rgbaOut
				);

	return 0;
}

static bool do_colorspace_software_conversion_in_threads(
		int threads, int alignment,
		guchar *y, guchar *cbcr,
		int yline, int width, int height,
		guchar *rgbaout)
{
	int i;
	guchar *curr_y, *curr_cbcr, *curr_rgba;
	int delta_y, delta_cbcr, delta_h, delta_rgba;
	int height_even;

	struct nv21_rgba_args thargs[threads];
	pthread_t thr[threads];

	if (height & 0x1) //can't properly handle odd-height images
		return false;

	curr_y = y;
	curr_cbcr = cbcr;
	curr_rgba = rgbaout;
	height_even = height / threads & (~1);
	delta_y = width * height_even;
	delta_cbcr = width * height_even / 2;
	delta_h = (height / threads) & (~1);
	delta_rgba = width * height_even * 4;

	for (i = 0; i < threads; i++) {
		thargs[i].y = curr_y + delta_y * i;
		thargs[i].CbCr = curr_cbcr + delta_cbcr * i;
		thargs[i].yLineSize = width;
		thargs[i].width = width;
		thargs[i].rgbaOut = curr_rgba + delta_rgba * i;

		if (i != (threads-1)) {
			thargs[i].height = delta_h;
		} else {
			thargs[i].height = height - (delta_h * (threads-1));
		}
	}

	for (i = 0; i < threads; i++) {
		pthread_create(&thr[i], NULL, nv21_software_rgba_thread, &thargs[i]);
	}
	for (i = 0; i < threads; i++) {
		pthread_join(thr[i], NULL);
	}

	return true;
}



static gboolean
do_colorspace_conversion (GstXvImageBuffer * xvimage)
{
  GstXvImageSink *xvimagesink = NULL;
  TBMinfoPTR tbmptr = NULL;
  guchar *scaled_boY_addr = NULL;
  guchar *scaled_boCbCr_addr = NULL;
  guchar *boPixmap_addr = NULL;
  Display *dpy = NULL;
  GstXPixmap *pm = NULL;
  guint w = 0;
  guint h = 0;
  static int64_t total_convert_time = 0, count = 0;

#if ENABLE_PERFORMANCE_CHECKING
  GST_ERROR_OBJECT (xvimagesink, "the time step in 'do_colorspace_conversion', cosumed %llu microseconds", get_time_diff());
#endif
  g_return_val_if_fail (xvimage != NULL, FALSE);
  g_return_val_if_fail (xvimage->xvimagesink != NULL, FALSE);
  xvimagesink = xvimage->xvimagesink;
  g_return_val_if_fail ((xvimagesink->xcontext != NULL && xvimagesink->xcontext->disp != NULL), FALSE);
  g_return_val_if_fail ((xvimagesink->xpixmap[xvimagesink->current_pixmap_idx] && xvimagesink->set_pixmap == TRUE), FALSE);

  pm = xvimagesink->xpixmap[xvimagesink->current_pixmap_idx];
  dpy = xvimagesink->xcontext->disp;

  GST_DEBUG_OBJECT (xvimagesink, "current_pixmap_idx: (%d), xpixmap(%p)!",
    xvimagesink->current_pixmap_idx, xvimagesink->xpixmap[xvimagesink->current_pixmap_idx]);

  if (!xvimagesink->get_pixmap_cb) {
    GST_WARNING_OBJECT (xvimagesink,"warning: No pixmap callback pointer set, so no extra TBM scaling support !");
    return FALSE;
  }
  if (!xvimage->pTBMinfo_web) {
    GST_ERROR_OBJECT(xvimagesink, "error: TBM scaling buffer is not prepared yet");
    return FALSE;
  }
  tbmptr = (TBMinfoPTR)(xvimage->pTBMinfo_web);
  if (!tbmptr->drm_fd || !tbmptr->bufmgr) {
    GST_ERROR_OBJECT(xvimagesink, "error: TBM scaling buffer is not prepared yet");
    return FALSE;
  }
  if ( !tbmptr->pScaledY || !tbmptr->pScaledCbCr) {
    GST_ERROR_OBJECT(xvimagesink, "error: the pScaledY or  pScaledCbCr buffer is null!");
    return FALSE;
  }

  tbmptr->bo_hnd_Pixmap = tbm_bo_map (tbmptr->boPixmap, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
  if (!tbmptr->bo_hnd_Pixmap.ptr) {
    GST_ERROR_OBJECT(xvimagesink, "error: tbm_bo_map Pixmap bo failed! ");
    return FALSE;
  }

  w = pm->width;
  h = pm->height;

  boPixmap_addr = (guchar *)tbmptr->bo_hnd_Pixmap.ptr;

#if ENABLE_PERFORMANCE_CHECKING
  GST_ERROR_OBJECT (xvimagesink, "the time after 'copy pScaledY and  pScaledCbCr to memory', cosumed %llu microseconds", get_time_diff());
#endif
  if (1) {
	bool is_converted = do_colorspace_neon_conversion_in_threads(CONV_THREADS,
																 CONV_ALIGNMENT,
																 (guchar *)tbmptr->bo_hnd_Y.ptr,
																 (guchar *)tbmptr->bo_hnd_CbCr.ptr,
																 w, w, h & (~1), boPixmap_addr);

	if (!is_converted) { //fallback, width is not modulo 16
		memcpy(tbmptr->pScaledY, ((guchar *)tbmptr->bo_hnd_Y.ptr), w * h);
		memcpy(tbmptr->pScaledCbCr, ((guchar *)tbmptr->bo_hnd_CbCr.ptr), (((w & 0x1) ? ((w >> 1) + 1) : (w >> 1)) * h));
		do_colorspace_software_conversion_in_threads(CONV_THREADS,
													 CONV_ALIGNMENT,
													 (guchar *)tbmptr->pScaledY,
													 (guchar *)tbmptr->pScaledCbCr,
													 w, w, h & (~1), boPixmap_addr);
	}
  } else {
    convert_yuv420_interleaved_to_argb_factor4(tbmptr->pScaledY, tbmptr->pScaledCbCr, w, w, w, h, boPixmap_addr);
  }
#if ENABLE_PERFORMANCE_CHECKING
  GST_ERROR_OBJECT (xvimagesink, "the time after 'convert yuv420 to argb8888', cosumed %llu microseconds", get_time_diff());
#endif
  tbm_bo_unmap (tbmptr->boPixmap);

#if ENABLE_PERFORMANCE_CHECKING
  GST_ERROR_OBJECT (xvimagesink, "the time after 'tbm_bo_unmap', cosumed %llu microseconds", get_time_diff());
#endif
  /* Send a damage event */
  XDamageAdd (dpy, pm->pixmap, tbmptr->region);
  XSync (dpy, 0);
#if ENABLE_PERFORMANCE_CHECKING
  GST_ERROR_OBJECT (xvimagesink, "the time after 'XDamageAdd', cosumed %llu microseconds", get_time_diff());
#endif
  return TRUE;
}
#endif //XV_WEBKIT_PIXMAP_SUPPORT

#if ENABLE_RT_DISPLAY
static gboolean rt_set_display_channel(GstXvImageSink *xvimagesink, gint dpychannel)
{
  GST_DEBUG_OBJECT(xvimagesink, "RT: set display channel !!!");

  gint i = 0, count = 0;
  gint val = 0;
  if(xvimagesink->xcontext){
	GstXContext* xcontext = xvimagesink->xcontext;
	XvAttribute *const attr = XvQueryPortAttributes(xcontext->disp, xcontext->xv_port_id, &count);
	if (attr) {
		for (i = 0 ; i < count ; i++) {
			if (!strcmp(attr[i].name, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE")) {
				Atom atom_butype = XInternAtom(xcontext->disp, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE", FALSE);
				gint ret = XvSetPortAttribute(xcontext->disp, xcontext->xv_port_id, atom_butype, dpychannel); // 0:bypass hw decoded frame(default)  1:index of MP framebuffer.
				if (ret != Success){
					GST_ERROR_OBJECT(xvimagesink, "RT: Failed	_USER_WM_PORT_ATTRIBUTE_BUFTYPE[index %d] -> found ret[ %d ], xvimage[ %p ]", i, ret, xvimagesink->xvimage);
					xvimagesink->rt_channel_setting_done = 0;
					return FALSE;
				}else{
					GST_DEBUG_OBJECT(xvimagesink, "RT: Success _USER_WM_PORT_ATTRIBUTE_BUFTYPE[index %d] -> xvimage[ %p ]", i, xvimagesink->xvimage);
				}
				break;
			}
		}
		XFree(attr);
	} else {
		GST_ERROR_OBJECT(xvimagesink, "RT: Failed XvQueryPortAttributes disp:%d, port_id:%d ", xcontext->disp, xcontext->xv_port_id);
		xvimagesink->rt_channel_setting_done = 0;
		return FALSE;
	}
	xvimagesink->rt_channel_setting_done = 1;
  }
  
  return TRUE;
}

static void rt_adjust_display_index(GstXvImageSink *xvimagesink)
{
  GST_DEBUG_OBJECT(xvimagesink, "RT: adjust display index !!!");

  if(xvimagesink){
	if(DISPLAY_BUFFER_NUM == 3){
		switch(xvimagesink->rt_display_idx_n){
			case 0:
				xvimagesink->rt_display_idx_n = 1;
				break;
			case 1:
				xvimagesink->rt_display_idx_n = 2;
				break;
			case 2:
				xvimagesink->rt_display_idx_n = 0;
				break;
			default:
				xvimagesink->rt_display_idx_n = 0;
				break;	
		}
		GST_DEBUG_OBJECT(xvimagesink,"RT: set rt_display_idx_n = %d ", xvimagesink->rt_display_idx_n);
	}else {
		xvimagesink->rt_display_idx_n = 0;
		GST_WARNING_OBJECT(xvimagesink,"RT: DISPLAY_BUFFER_NUM != 3, set default display idx = 0 ");
	}
  }
}

static gboolean rt_free_tbm_display_buffer(GstXvImageSink *xvimagesink)
{
	GST_DEBUG_OBJECT(xvimagesink, "RT: free tbm display buffer !!!");

	gint m;
	if(xvimagesink){
	  for(m = 0; m < DISPLAY_BUFFER_NUM; m++){
		  if(xvimagesink->rt_display_bo_hnd_Y[m].ptr){
			tbm_bo_unmap(xvimagesink->rt_display_boY[m]);
			GST_DEBUG_OBJECT(xvimagesink, "RT: %d unmap tbm display Y buffer success", m);
		  }
		  if(xvimagesink->rt_display_boY[m]){
			tbm_bo_unref(xvimagesink->rt_display_boY[m]);
			xvimagesink->rt_display_boY[m] = NULL;
			GST_DEBUG_OBJECT(xvimagesink, "RT: %d pre unref tbm display Y buffer success", m);
		  }
		  if(xvimagesink->rt_display_bo_hnd_CbCr[m].ptr){
			tbm_bo_unmap(xvimagesink->rt_display_boCbCr[m]);
			GST_DEBUG_OBJECT(xvimagesink, "RT: %d pre unmap tbm display CbCr buffer success", m);
		  }
		  if(xvimagesink->rt_display_boCbCr[m]){
			tbm_bo_unref(xvimagesink->rt_display_boCbCr[m]);
			xvimagesink->rt_display_boCbCr[m] = NULL;
			GST_DEBUG_OBJECT(xvimagesink, "RT: %d pre unref tbm display CbCr buffer success", m);
		  }
		  memset(&xvimagesink->rt_display_boY[m], 0x0, sizeof(tbm_bo));
		  memset(&xvimagesink->rt_display_boCbCr[m], 0x0, sizeof(tbm_bo));
		  memset(&xvimagesink->rt_display_bo_hnd_Y[m], 0x0, sizeof(tbm_bo_handle));
		  memset(&xvimagesink->rt_display_bo_hnd_CbCr[m], 0x0, sizeof(tbm_bo_handle));
		  GST_DEBUG_OBJECT(xvimagesink, "RT: pre free tbm display buffer success!");
	}
	if(xvimagesink->rt_display_bufmgr){
		deinit_tbm_bufmgr(&xvimagesink->rt_display_drm_fd, &xvimagesink->rt_display_bufmgr);
		GST_DEBUG_OBJECT(xvimagesink, "RT: deinit tbm display bufmgr success");
	}  
  	xvimagesink->rt_allocate_done = 0;
	xvimagesink->rt_display_drm_fd = -1;
}

	return TRUE;
}

static gboolean rt_allocate_tbm_display_buffer(GstXvImageSink *xvimagesink)
{
	GST_DEBUG_OBJECT(xvimagesink, "RT: allocate tbm display buffer !!!");

	gint m, i;
	guint w, h;
	gint degree;

	for(m = 0; m < DISPLAY_BUFFER_NUM; m++){
		memset (&xvimagesink->rt_display_bo_hnd_Y[m], 0x0, sizeof(tbm_bo_handle));
		memset (&xvimagesink->rt_display_bo_hnd_CbCr[m], 0x0, sizeof(tbm_bo_handle));
	}

	if(xvimagesink->rt_display_drm_fd == -1){
		if(!init_tbm_bufmgr(&xvimagesink->rt_display_drm_fd, &xvimagesink->rt_display_bufmgr, xvimagesink->xcontext->disp)){
			GST_ERROR_OBJECT (xvimagesink, "RT: init tbm bufmgr failed!");
			return FALSE;
		}
	}

	for(m = 0; m < DISPLAY_BUFFER_NUM; m++){
#if (ENABLE_RT_SEAMLESS_HW_SCALER || ENABLE_RT_SEAMLESS_GA_SCALER)
	w = 1920;
	h = 1080;
#else
	w = xvimagesink->xvimage->width;
	h = xvimagesink->xvimage->height;
#endif

#ifdef USE_TBM_FOXP
		xvimagesink->rt_display_boY[m] = tbm_bo_alloc(xvimagesink->rt_display_bufmgr, w*h, TBM_BO_FOXP_DP_MEM | DP_FB_Y | DP_FB_IDX(((get_dp_fb_type(xvimagesink)) | m)));
#else
		xvimagesink->rt_display_boY[m] = tbm_bo_alloc(xvimagesink->rt_display_bufmgr, w*h, TBM_BO_DEFAULT);
#endif
		if (!xvimagesink->rt_display_boY[m]) {
			GST_ERROR_OBJECT(xvimagesink, "RT: allocate tbm display Y buffer failed");
			goto RT_FAIL_TO_PRE_ALLOC_TBM;
		}
		xvimagesink->rt_display_bo_hnd_Y[m] = tbm_bo_map(xvimagesink->rt_display_boY[m], TBM_DEVICE_CPU, TBM_OPTION_WRITE);
		if (!xvimagesink->rt_display_bo_hnd_Y[m].ptr) {
			GST_ERROR_OBJECT(xvimagesink, "RT: map tbm display Y buffer failed");
			goto RT_FAIL_TO_PRE_ALLOC_TBM;
		}
#ifdef USE_TBM_FOXP
		xvimagesink->rt_display_boCbCr[m] = tbm_bo_alloc(xvimagesink->rt_display_bufmgr, w*h/2, TBM_BO_FOXP_DP_MEM | DP_FB_C | DP_FB_IDX(((get_dp_fb_type(xvimagesink)) | m)));
#else
		xvimagesink->rt_display_boCbCr[m] = tbm_bo_alloc(xvimagesink->rt_display_bufmgr, w*h/2, TBM_BO_DEFAULT);
#endif
		if (!xvimagesink->rt_display_boCbCr[m]) {
			GST_ERROR_OBJECT(xvimagesink, "RT: allocate tbm display CbCr buffer failed");
			goto RT_FAIL_TO_PRE_ALLOC_TBM;
		}
		xvimagesink->rt_display_bo_hnd_CbCr[m] = tbm_bo_map(xvimagesink->rt_display_boCbCr[m], TBM_DEVICE_CPU, TBM_OPTION_WRITE);
		if (!xvimagesink->rt_display_bo_hnd_CbCr[m].ptr) {
			GST_ERROR_OBJECT(xvimagesink, "RT: map tbm display CbCr buffer failed");
			goto RT_FAIL_TO_PRE_ALLOC_TBM;
		}

		xvimagesink->rt_display_idx[m] = m;
		memset(xvimagesink->rt_display_bo_hnd_Y[m].ptr, 0, w*h);
		memset(xvimagesink->rt_display_bo_hnd_CbCr[m].ptr, 0, w*h/2);
		GST_DEBUG_OBJECT(xvimagesink, "RT:  Succeed to allocate DP buffer %d -> idx[ %d ] bufmgr[ %p ], size[ %d x %d ] , Y[ bo:%p, vaddr:%p, size:%d ] C[ bo:%p, vaddr:%p, size:%d ]",
			m, xvimagesink->rt_display_idx[m], xvimagesink->rt_display_bufmgr, w,h,xvimagesink->rt_display_boY[m], xvimagesink->rt_display_bo_hnd_Y[m].ptr, tbm_bo_size(xvimagesink->rt_display_boY[m]), xvimagesink->rt_display_boCbCr[m], xvimagesink->rt_display_bo_hnd_CbCr[m].ptr, tbm_bo_size(xvimagesink->rt_display_boCbCr[m]));
		continue;
	
RT_FAIL_TO_PRE_ALLOC_TBM:
		GST_ERROR_OBJECT(xvimagesink, "RT: Failed to allocate display buffer -> idx[ %d ]", m);
		for(i = 0; i < DISPLAY_BUFFER_NUM; i++){
			if (xvimagesink->rt_display_bo_hnd_Y[i].ptr)	tbm_bo_unmap(xvimagesink->rt_display_boY[i]);
			if (xvimagesink->rt_display_boY[i])  tbm_bo_unref(xvimagesink->rt_display_boY[i]);
			if (xvimagesink->rt_display_bo_hnd_CbCr[i].ptr)  tbm_bo_unmap(xvimagesink->rt_display_boCbCr[i]);
			if (xvimagesink->rt_display_boCbCr[i])  tbm_bo_unref(xvimagesink->rt_display_boCbCr[i]);
			GST_ERROR_OBJECT(xvimagesink, "RT: free DP buffer %d ", i);
		}
		deinit_tbm_bufmgr(&xvimagesink->rt_display_drm_fd, &xvimagesink->rt_display_bufmgr);
		/* xvimagesink->rt_display_drm_fd == -1; */
        	xvimagesink->rt_display_drm_fd = -1;  /* w.you modified at 2015/1/15 for prevent, DF150113-00549*/
		GST_ERROR_OBJECT(xvimagesink, "RT: deinit tbm buffer manager done !!! ");
		return FALSE;
	}

	xvimagesink->rt_allocate_done = 1;
	GST_DEBUG_OBJECT(xvimagesink, "RT: allocate DP buffer done !!!");
	return TRUE;
}

static gboolean rt_do_display(GstXvImageBuffer * xvimage, GstBuffer *buf)
{
  	gint ret = 0;
	gint ret1 = 0, ret2 = 0;
	struct v4l2_drm *outinfo;
	struct v4l2_drm_dec_info *decinfo;
	struct v4l2_private_frame_info *frminfo;
	GstXvImageSink * xvimagesink = NULL;
	g_return_val_if_fail (xvimage != NULL, FALSE);
	g_return_val_if_fail (xvimage->xvimagesink != NULL, FALSE);
	g_return_val_if_fail (buf != NULL, FALSE);
	xvimagesink = xvimage->xvimagesink;

	GST_DEBUG_OBJECT(xvimagesink, "RT: do display !!!");

	if(!xvimagesink->rt_channel_setting_done){
		ret = rt_set_display_channel(xvimagesink, 1);
		if(!ret){
			GST_ERROR_OBJECT(xvimagesink,"RT: Failed to set display path !!!");
			return FALSE;
		}
	}

	if(!xvimagesink->rt_allocate_done){
		rt_free_tbm_display_buffer(xvimagesink);
		ret = rt_allocate_tbm_display_buffer(xvimagesink);
		if(ret) {
			GST_DEBUG_OBJECT(xvimagesink,"RT: Success to allocate tbm display buffer");
		}else {
			GST_ERROR_OBJECT(xvimagesink,"RT: Failed to allocate tbm display buffer");
			return FALSE;
		}
	}

	rt_adjust_display_index(xvimagesink);

	outinfo = (struct v4l2_drm*)(GST_BUFFER_MALLOCDATA(buf));
	decinfo = &(outinfo->u.dec_info);
	frminfo = &(outinfo->u.dec_info.pFrame[0]);
	GST_DEBUG_OBJECT(xvimagesink, "RT: Get HW Frame Data info: y_viraddr[0x%x] u_viraddr[0x%x] y_linesize[%d] u_linesize[%d]  W[%d]/H[%d]  y_phyaddr[0x%x] u_phyaddr[0x%x] v_phyaddr[0x%x] !!!",
			  frminfo->y_viraddr, frminfo->u_viraddr, frminfo->y_linesize, frminfo->u_linesize, frminfo->width, frminfo->height,  frminfo->y_phyaddr, frminfo->u_phyaddr, frminfo->v_phyaddr);
#if ENABLE_RT_SEAMLESS_GA_SCALER
	tbm_ga_scale_wrap sm_wrap;	//Can support Seamless Resolution
	memset(&sm_wrap, 0, sizeof(tbm_ga_scale_wrap));
	sm_wrap.bufmgr = xvimagesink->rt_display_bufmgr;
	// COPY Y
	{
		  sm_wrap.src_bo = NULL;
		  sm_wrap.dst_bo = xvimagesink->rt_display_boY[xvimagesink->rt_display_idx_n];
		  sm_wrap.src_paddr = frminfo->y_phyaddr;
		  sm_wrap.dst_paddr = NULL;
		  sm_wrap.scale.color_mode = TBM_GA_FORMAT_8BPP;
		  sm_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
		  sm_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
		  sm_wrap.scale.src_hbyte_size = frminfo->y_linesize * TBM_BPP8; /* Y line size */
		  sm_wrap.scale.src_rect.x = 0;
		  sm_wrap.scale.src_rect.y = 0; /* Y: Top of buffer (w*h) */
		  sm_wrap.scale.src_rect.w = frminfo->width;
		  sm_wrap.scale.src_rect.h = frminfo->height;
		  sm_wrap.scale.dst_hbyte_size = xvimagesink->dp_linesize * TBM_BPP8;
		  sm_wrap.scale.dst_rect.x = 0;
		  sm_wrap.scale.dst_rect.y = 0;
		  sm_wrap.scale.dst_rect.w = 1920;
		  sm_wrap.scale.dst_rect.h = 1080;
		  sm_wrap.scale.rop_ca_value = 0;
		  sm_wrap.scale.src_key = 0;
		  sm_wrap.scale.rop_on_off = 0;
		  ret1 = tbm_bo_ga_scale(&sm_wrap);
	}
	// COPY CbCr
	{
		  sm_wrap.src_bo = NULL;
		  sm_wrap.dst_bo = xvimagesink->rt_display_boCbCr[xvimagesink->rt_display_idx_n];
		  sm_wrap.src_paddr = frminfo->u_phyaddr;
		  sm_wrap.dst_paddr = NULL;
		  sm_wrap.scale.color_mode = TBM_GA_FORMAT_16BPP;
		  sm_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
		  sm_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
		  sm_wrap.scale.src_hbyte_size = frminfo->u_linesize * TBM_BPP8; /* for YUV420 interleaved case */
		  sm_wrap.scale.src_rect.x = 0;
		  sm_wrap.scale.src_rect.y = 0;
		  sm_wrap.scale.src_rect.w = frminfo->width/2;
		  sm_wrap.scale.src_rect.h = frminfo->height/2;
		  sm_wrap.scale.dst_hbyte_size = xvimagesink->dp_linesize * TBM_BPP8;
		  sm_wrap.scale.dst_rect.x = 0;
		  sm_wrap.scale.dst_rect.y = 0;
		  sm_wrap.scale.dst_rect.w = 960;
		  sm_wrap.scale.dst_rect.h = 540;
		  sm_wrap.scale.rop_ca_value = 0;
		  sm_wrap.scale.src_key = 0;
		  sm_wrap.scale.rop_on_off = 0;
		  ret2 = tbm_bo_ga_scale(&sm_wrap);
	}
#else
  	tbm_ga_bitblt_wrap bb_wrap;
	memset(&bb_wrap, 0, sizeof(tbm_ga_bitblt_wrap));
	bb_wrap.bufmgr = xvimagesink->rt_display_bufmgr;
	// COPY Y 
	{
		bb_wrap.src_bo = NULL;
		bb_wrap.dst_bo = xvimagesink->rt_display_boY[xvimagesink->rt_display_idx_n];
		bb_wrap.src_paddr = frminfo->y_phyaddr;
		bb_wrap.dst_paddr = NULL;
		bb_wrap.bitblt.color_mode = TBM_GA_FORMAT_8BPP;
		bb_wrap.bitblt.ga_mode = TBM_GA_BITBLT_MODE_NORMAL;
		bb_wrap.bitblt.src1_byte_size = frminfo->y_linesize * TBM_BPP8;
		bb_wrap.bitblt.src1_rect.x = 0;
		bb_wrap.bitblt.src1_rect.y = 0;	// Y: Top of buffer (w*h)
		bb_wrap.bitblt.src1_rect.w = xvimage->width;
		bb_wrap.bitblt.src1_rect.h = xvimage->height;
		bb_wrap.bitblt.dst_byte_size = xvimagesink->dp_linesize * TBM_BPP8; // --> YUV420 : 1920,	 YUV444 : 3846, Fixed linesize of destination MP FrameBuffer
		bb_wrap.bitblt.dst_x = 0;
		bb_wrap.bitblt.dst_y = 0;
		ret1 = tbm_bo_ga_copy(&bb_wrap);
	}
	// COPY CbCr
	{
		bb_wrap.src_bo = NULL;
		bb_wrap.dst_bo = xvimagesink->rt_display_boCbCr[xvimagesink->rt_display_idx_n];
		bb_wrap.src_paddr = frminfo->u_phyaddr;
		bb_wrap.dst_paddr = NULL;
		bb_wrap.bitblt.color_mode = TBM_GA_FORMAT_16BPP; // Because of CbCr Interleaved
		bb_wrap.bitblt.ga_mode = TBM_GA_BITBLT_MODE_NORMAL;
		bb_wrap.bitblt.src1_byte_size = frminfo->u_linesize * TBM_BPP8;
		bb_wrap.bitblt.src1_rect.x = 0;
		bb_wrap.bitblt.src1_rect.y = 0;
		bb_wrap.bitblt.src1_rect.w = xvimage->width/2;
		bb_wrap.bitblt.src1_rect.h = xvimage->height/2;
		bb_wrap.bitblt.dst_byte_size = xvimagesink->dp_linesize * TBM_BPP8; // YUV420 : 1920,   YUV444 : 3846, Fixed linesize of destination MP FrameBuffer
		bb_wrap.bitblt.dst_x = 0;
		bb_wrap.bitblt.dst_y = 0;
		ret2 = tbm_bo_ga_copy(&bb_wrap);
	}
#endif
	if (ret1 != 1 || ret2 != 1){
		GST_ERROR_OBJECT(xvimagesink, "RT: Failed ga_copy HW -> MP, [ retY:%d  retC:%d ]", ret1, ret2);
		return FALSE;
	}else {
		GST_DEBUG_OBJECT(xvimagesink,"RT: Succeed ga_copy HW -> MP !!!");
	}

	FOXPXvDecInfo *pVideoFrame = (FOXPXvDecInfo*) xvimagesink->xvimage->xvimage->data;
	if(pVideoFrame){
		memset(pVideoFrame, 0x0, sizeof(FOXPXvDecInfo));
	}else{
	    GST_DEBUG_OBJECT(xvimagesink, "pVideoFrame is NULL  !!!");
	    return FALSE;
    }
	if (xvimagesink->fps_d && xvimagesink->fps_n){
		pVideoFrame->framerate = xvimagesink->fps_n * 100 / xvimagesink->fps_d;
	}else{
		pVideoFrame->framerate = 6000;
		GST_WARNING_OBJECT(xvimagesink, "RT: wrong framerate [ %d / %d ] --> 6000", xvimagesink->fps_n, xvimagesink->fps_d);
	}
	pVideoFrame->colorformat = XV_DRM_COLORFORMAT_YUV420;
	pVideoFrame->scantype = 1; // always progressive
	pVideoFrame->display_index = xvimagesink->rt_display_idx[xvimagesink->rt_display_idx_n];
	GST_DEBUG_OBJECT (xvimagesink, "RT: Finish Xvideo display setting in MP channel display_index[%d]  !!! ", pVideoFrame->display_index);

	return TRUE;
}
#endif

static void
gst_lookup_xv_port_from_adaptor (GstXContext * xcontext,
    XvAdaptorInfo * adaptors, int adaptor_no)
{
  gint j;
  gint res;

  /* Do we support XvImageMask ? */
  if (!(adaptors[adaptor_no].type & XvImageMask)) {
    GST_DEBUG ("XV Adaptor %s has no support for XvImageMask",
        adaptors[adaptor_no].name);
    return;
  }

  /* We found such an adaptor, looking for an available port */
  for (j = 0; j < adaptors[adaptor_no].num_ports && !xcontext->xv_port_id; j++) {
    /* We try to grab the port */
    res = XvGrabPort (xcontext->disp, adaptors[adaptor_no].base_id + j, 0);
    if (Success == res) {
      xcontext->xv_port_id = adaptors[adaptor_no].base_id + j;
      GST_DEBUG ("XV Adaptor %s with %ld ports", adaptors[adaptor_no].name,
          adaptors[adaptor_no].num_ports);
    } else {
      GST_DEBUG ("GrabPort %d for XV Adaptor %s failed: %d", j,
          adaptors[adaptor_no].name, res);
    }
  }
}

/* This function generates a caps with all supported format by the first
   Xv grabable port we find. We store each one of the supported formats in a
   format list and append the format to a newly created caps that we return
   If this function does not return NULL because of an error, it also grabs
   the port via XvGrabPort */
static GstCaps *
gst_xvimagesink_get_xv_support (GstXvImageSink * xvimagesink,
    GstXContext * xcontext)
{
  gint i;
  XvAdaptorInfo *adaptors;
  gint nb_formats;
  XvImageFormatValues *formats = NULL;
  guint nb_encodings;
  XvEncodingInfo *encodings = NULL;
  gulong max_w = G_MAXINT, max_h = G_MAXINT;
  GstCaps *caps = NULL;
  GstCaps *rgb_caps = NULL;

  g_return_val_if_fail (xcontext != NULL, NULL);

  /* First let's check that XVideo extension is available */
  if (!XQueryExtension (xcontext->disp, "XVideo", &i, &i, &i)) {
    GST_ELEMENT_ERROR (xvimagesink, RESOURCE, SETTINGS,
        ("Could not initialise Xv output"),
        ("XVideo extension is not available"));
    return NULL;
  }

  /* Then we get adaptors list */
  if (Success != XvQueryAdaptors (xcontext->disp, xcontext->root,
          &xcontext->nb_adaptors, &adaptors)) {
    GST_ELEMENT_ERROR (xvimagesink, RESOURCE, SETTINGS,
        ("Could not initialise Xv output"),
        ("Failed getting XV adaptors list"));
    return NULL;
  }

  xcontext->xv_port_id = 0;

  GST_DEBUG ("Found %u XV adaptor(s)", xcontext->nb_adaptors);

  xcontext->adaptors =
      (gchar **) g_malloc0 (xcontext->nb_adaptors * sizeof (gchar *));

  /* Now fill up our adaptor name array */
  for (i = 0; i < xcontext->nb_adaptors; i++) {
    xcontext->adaptors[i] = g_strdup (adaptors[i].name);
    GST_DEBUG ("Name of adaptor #[%d] = %s", i, adaptors[i].name);
  }

  if (xvimagesink->adaptor_no < xcontext->nb_adaptors) {
    /* Find xv port from user defined adaptor */
    gst_lookup_xv_port_from_adaptor (xcontext, adaptors,
        xvimagesink->adaptor_no);
  }

  if (!xcontext->xv_port_id) {
    /* Now search for an adaptor that supports XvImageMask */
    for (i = 0; i < xcontext->nb_adaptors && !xcontext->xv_port_id; i++) {
      gst_lookup_xv_port_from_adaptor (xcontext, adaptors, i);
      xvimagesink->adaptor_no = i;
    }
  }

  XvFreeAdaptorInfo (adaptors);

  if (!xcontext->xv_port_id) {
    xvimagesink->adaptor_no = -1;
    GST_ELEMENT_ERROR (xvimagesink, RESOURCE, BUSY,
        ("Could not initialise Xv output"), ("No port available"));
    return NULL;
  }

  /* Set XV_AUTOPAINT_COLORKEY and XV_DOUBLE_BUFFER and XV_COLORKEY */
  {
    int count, todo = 3;
    XvAttribute *const attr = XvQueryPortAttributes (xcontext->disp,
        xcontext->xv_port_id, &count);
    static const char autopaint[] = "XV_AUTOPAINT_COLORKEY";
    static const char dbl_buffer[] = "XV_DOUBLE_BUFFER";
    static const char colorkey[] = "XV_COLORKEY";

    GST_DEBUG_OBJECT (xvimagesink, "Checking %d Xv port attributes", count);

    xvimagesink->have_autopaint_colorkey = FALSE;
    xvimagesink->have_double_buffer = FALSE;
    xvimagesink->have_colorkey = FALSE;

    for (i = 0; ((i < count) && todo); i++)
      if (!strcmp (attr[i].name, autopaint)) {
        const Atom atom = XInternAtom (xcontext->disp, autopaint, False);

        /* turn on autopaint colorkey */
        XvSetPortAttribute (xcontext->disp, xcontext->xv_port_id, atom,
            (xvimagesink->autopaint_colorkey ? 1 : 0));
        todo--;
        xvimagesink->have_autopaint_colorkey = TRUE;
      } else if (!strcmp (attr[i].name, dbl_buffer)) {
        const Atom atom = XInternAtom (xcontext->disp, dbl_buffer, False);

        XvSetPortAttribute (xcontext->disp, xcontext->xv_port_id, atom,
            (xvimagesink->double_buffer ? 1 : 0));
        todo--;
        xvimagesink->have_double_buffer = TRUE;
      } else if (!strcmp (attr[i].name, colorkey)) {
        /* Set the colorkey, default is something that is dark but hopefully
         * won't randomly appear on the screen elsewhere (ie not black or greys)
         * can be overridden by setting "colorkey" property
         */
        const Atom atom = XInternAtom (xcontext->disp, colorkey, False);
        guint32 ckey = 0;
        gboolean set_attr = TRUE;
        guint cr, cg, cb;

        /* set a colorkey in the right format RGB565/RGB888
         * We only handle these 2 cases, because they're the only types of
         * devices we've encountered. If we don't recognise it, leave it alone
         */
        cr = (xvimagesink->colorkey >> 16);
        cg = (xvimagesink->colorkey >> 8) & 0xFF;
        cb = (xvimagesink->colorkey) & 0xFF;
        switch (xcontext->depth) {
          case 16:             /* RGB 565 */
            cr >>= 3;
            cg >>= 2;
            cb >>= 3;
            ckey = (cr << 11) | (cg << 5) | cb;
            break;
          case 24:
          case 32:             /* RGB 888 / ARGB 8888 */
            ckey = (cr << 16) | (cg << 8) | cb;
            break;
          default:
            GST_DEBUG_OBJECT (xvimagesink,
                "Unknown bit depth %d for Xv Colorkey - not adjusting",
                xcontext->depth);
            set_attr = FALSE;
            break;
        }

        if (set_attr) {
          ckey = CLAMP (ckey, (guint32) attr[i].min_value,
              (guint32) attr[i].max_value);
          GST_LOG_OBJECT (xvimagesink,
              "Setting color key for display depth %d to 0x%x",
              xcontext->depth, ckey);

          XvSetPortAttribute (xcontext->disp, xcontext->xv_port_id, atom,
              (gint) ckey);
        }
        todo--;
        xvimagesink->have_colorkey = TRUE;
      }

    XFree (attr);
  }

  /* Get the list of encodings supported by the adapter and look for the
   * XV_IMAGE encoding so we can determine the maximum width and height
   * supported */
  XvQueryEncodings (xcontext->disp, xcontext->xv_port_id, &nb_encodings,
      &encodings);

  for (i = 0; i < nb_encodings; i++) {
    GST_LOG_OBJECT (xvimagesink,
        "Encoding %d, name %s, max wxh %lux%lu rate %d/%d",
        i, encodings[i].name, encodings[i].width, encodings[i].height,
        encodings[i].rate.numerator, encodings[i].rate.denominator);
    if (strcmp (encodings[i].name, "XV_IMAGE") == 0) {
      max_w = encodings[i].width;
      max_h = encodings[i].height;
#ifdef GST_EXT_XV_ENHANCEMENT
      xvimagesink->scr_w = max_w;
      xvimagesink->scr_h = max_h;
#endif /* GST_EXT_XV_ENHANCEMENT */
    }
  }

  XvFreeEncodingInfo (encodings);

  /* We get all image formats supported by our port */
  formats = XvListImageFormats (xcontext->disp,
      xcontext->xv_port_id, &nb_formats);
  caps = gst_caps_new_empty ();
  for (i = 0; i < nb_formats; i++) {
    GstCaps *format_caps = NULL;
    gboolean is_rgb_format = FALSE;

    /* We set the image format of the xcontext to an existing one. This
       is just some valid image format for making our xshm calls check before
       caps negotiation really happens. */
    xcontext->im_format = formats[i].id;
    //GST_DEBUG_OBJECT(xvimagesink, "idx[ %d / %d ] ID[%" GST_FOURCC_FORMAT "], TYPE[ %d ]!!!", i, nb_formats, GST_FOURCC_ARGS(formats[i].id), formats[i].type);
    switch (formats[i].type) {
      case XvRGB:
      {
        XvImageFormatValues *fmt = &(formats[i]);
        gint endianness = G_BIG_ENDIAN;

        if (fmt->byte_order == LSBFirst) {
          /* our caps system handles 24/32bpp RGB as big-endian. */
          if (fmt->bits_per_pixel == 24 || fmt->bits_per_pixel == 32) {
            fmt->red_mask = GUINT32_TO_BE (fmt->red_mask);
            fmt->green_mask = GUINT32_TO_BE (fmt->green_mask);
            fmt->blue_mask = GUINT32_TO_BE (fmt->blue_mask);

            if (fmt->bits_per_pixel == 24) {
              fmt->red_mask >>= 8;
              fmt->green_mask >>= 8;
              fmt->blue_mask >>= 8;
            }
          } else
            endianness = G_LITTLE_ENDIAN;
        }

        format_caps = gst_caps_new_simple ("video/x-raw-rgb",
            "endianness", G_TYPE_INT, endianness,
            "depth", G_TYPE_INT, fmt->depth,
            "bpp", G_TYPE_INT, fmt->bits_per_pixel,
            "red_mask", G_TYPE_INT, fmt->red_mask,
            "green_mask", G_TYPE_INT, fmt->green_mask,
            "blue_mask", G_TYPE_INT, fmt->blue_mask,
            "width", GST_TYPE_INT_RANGE, 1, max_w,
            "height", GST_TYPE_INT_RANGE, 1, max_h,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

        is_rgb_format = TRUE;
        break;
      }
      case XvYUV:
        format_caps = gst_caps_new_simple ("video/x-raw-yuv",
            "format", GST_TYPE_FOURCC, formats[i].id,
            "width", GST_TYPE_INT_RANGE, 1, max_w,
            "height", GST_TYPE_INT_RANGE, 1, max_h,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    if (format_caps) {
      GstXvImageFormat *format = NULL;

      format = g_new0 (GstXvImageFormat, 1);
      if (format) {
        format->format = formats[i].id;
        format->caps = gst_caps_copy (format_caps);
        xcontext->formats_list = g_list_append (xcontext->formats_list, format);
      }

      if (is_rgb_format) {
        if (rgb_caps == NULL)
          rgb_caps = format_caps;
        else
          gst_caps_append (rgb_caps, format_caps);
      } else
        gst_caps_append (caps, format_caps);
    }
  }

  /* Collected all caps into either the caps or rgb_caps structures.
   * Append rgb_caps on the end of YUV, so that YUV is always preferred */
  if (rgb_caps)
    gst_caps_append (caps, rgb_caps);
  if (formats)
    XFree (formats);

  GST_DEBUG ("Generated the following caps: %" GST_PTR_FORMAT, caps);

  if (gst_caps_is_empty (caps)) {
    gst_caps_unref (caps);
    XvUngrabPort (xcontext->disp, xcontext->xv_port_id, 0);
    GST_ELEMENT_ERROR (xvimagesink, STREAM, WRONG_TYPE, (NULL),
        ("No supported format found"));
    return NULL;
  }

  return caps;
}

inline void
gst_xvimagesink_update_video_quality(GstXvImageSink * xvimagesink)
{
	guint64 T1=0, T2=0, T3=0, T4=0, T5=0, T6=0, T7=0, T8=0;
	// Bitrate Check
	if(xvimagesink->pre_bitrate != xvimagesink->new_bitrate)
	{
		T1 = get_time(); 
		avoc_set_bitrate_level_async(xvimagesink->new_bitrate);
		T2 = get_time();
		xvimagesink->pre_bitrate = xvimagesink->new_bitrate;
	}

	// SEI Metadata
	guint debug_sei_data_size = 0;
	if (xvimagesink->sei_metadata_size > 0 && xvimagesink->sei_metadata)
	{
		T3 = get_time(); 
		avoc_pass_sei_metadata_async(xvimagesink->sei_metadata, xvimagesink->sei_metadata_size);
		debug_sei_data_size = xvimagesink->sei_metadata_size;
		xvimagesink->sei_metadata_size = 0;
		T4 = get_time();
	}

	// SEI mvcd data
	if (xvimagesink->sei_mdcv_filled && xvimagesink->sei_mdcv)
	{
		T5 = get_time(); 
		avoc_pass_mdcv_metadata_async(xvimagesink->sei_mdcv, sizeof(VIDEO_MASTERING_DISPLAY_COLOUR_VOLUME));
		xvimagesink->sei_mdcv_filled = FALSE;
		T6 = get_time();
	}

	// Color Inforation
	if (xvimagesink->need_update_color_info == TRUE)
	{
		T7 = get_time();
		avoc_set_color_space_info_async(xvimagesink->prev_color_info_format, &xvimagesink->prev_color_info);	// must be set between  pre_avoc_set_resolutionand post.
		T8 = get_time();
		xvimagesink->need_update_color_info = FALSE;
	}

	if (T1 || T3 || T5 || T7) {
		if ((T2-T1 > MIN_LATENCY*4) || (T4-T3 > MIN_LATENCY*4) || (T6-T5 > MIN_LATENCY*4) || (T8-T7 > MIN_LATENCY*4)) {
			GST_ERROR_OBJECT(xvimagesink, "bitrate[ %d, %lld ms ],  sei[ size %d, %lld ms ], mvcd[ %lld ms ], colorinfo[ %lld ms ]"
								,xvimagesink->pre_bitrate, (T2-T1)/1000, debug_sei_data_size, (T4-T3)/1000, (T6-T5)/1000, (T8-T7)/1000);
		} else {
			GST_LOG_OBJECT(xvimagesink, "bitrate[ %d, %lld ms ],  sei[ size %d, %lld ms ], mvcd[ %lld ms ], colorinfo[ %lld ms ]"
								,xvimagesink->pre_bitrate, (T2-T1)/1000, debug_sei_data_size, (T4-T3)/1000, (T6-T5)/1000, (T8-T7)/1000);
		}
	}
}
	

static gpointer
gst_xvimagesink_event_thread (GstXvImageSink * xvimagesink)
{
  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), NULL);

  GST_OBJECT_LOCK (xvimagesink);
	XWindowAttributes attr;
	if(xvimagesink->xwindow)
	{
		 XGetWindowAttributes (xvimagesink->xcontext->disp, xvimagesink->xwindow->win, &attr);
		if((attr.map_state != 2) && (xvimagesink->is_hided == FALSE))
		{
		  // set hide mask
		  xvimagesink->is_hided = TRUE;
		  mute_video_display(xvimagesink, TRUE, MUTE_STATE_HIDE);
		}else if((attr.map_state == 2) && (xvimagesink->is_hided == TRUE))
		{
		  // set show mask
		  xvimagesink->is_hided = FALSE;
		  mute_video_display(xvimagesink, FALSE, MUTE_STATE_HIDE);
		}

		if((xvimagesink->xwindow->width > 1) && (xvimagesink->xwindow->height > 1))
		{
		  mute_video_display(xvimagesink, FALSE, MUTE_INVALID_WINSIZE);
		}
		else
		{
		  mute_video_display(xvimagesink, TRUE, MUTE_INVALID_WINSIZE);
		}
	}
  while (xvimagesink->running) {
    GST_OBJECT_UNLOCK (xvimagesink);
    if(xvimagesink){
	    if (xvimagesink->xwindow) {
	      guint64 T1 = get_time();
	      gst_xvimagesink_handle_xevents (xvimagesink);
	      guint64 T2 = get_time();
	      if (T2-T1 > MIN_LATENCY*4)
	        GST_ERROR_OBJECT(xvimagesink, "handle_xevent[ %lld ms ]", (T2-T1)/1000);
#ifdef USE_AVOC_DAEMON	        
	      gst_xvimagesink_update_video_quality(xvimagesink);
#endif 
	    }
	    /* FIXME: do we want to align this with the framerate or anything else? */
	    g_usleep (G_USEC_PER_SEC / 20);

	    GST_OBJECT_LOCK (xvimagesink);
    }else {
	GST_WARNING_OBJECT(xvimagesink, "Xvimagesink already destory by APP, so ignore this event thread !");
    }
  }
  if(xvimagesink){
  	GST_OBJECT_UNLOCK (xvimagesink);
  }else {
	GST_WARNING_OBJECT(xvimagesink, "Xvimagesink already destory by APP, so ignore this event thread !!");
  }

  return NULL;
}

static void
gst_xvimagesink_manage_event_thread (GstXvImageSink * xvimagesink)
{
  GThread *thread = NULL;

  /* don't start the thread too early */
  if (xvimagesink->xcontext == NULL) {
    return;
  }

  GST_OBJECT_LOCK (xvimagesink);
  if (xvimagesink->handle_expose || xvimagesink->handle_events) {
    if (!xvimagesink->event_thread) {
      /* Setup our event listening thread */
      GST_DEBUG_OBJECT (xvimagesink, "run xevent thread, expose %d, events %d",
          xvimagesink->handle_expose, xvimagesink->handle_events);
      xvimagesink->running = TRUE;
#if !GLIB_CHECK_VERSION (2, 31, 0)
      xvimagesink->event_thread = g_thread_create (
          (GThreadFunc) gst_xvimagesink_event_thread, xvimagesink, TRUE, NULL);
#else
      xvimagesink->event_thread = g_thread_try_new ("xvimagesink-events",
          (GThreadFunc) gst_xvimagesink_event_thread, xvimagesink, NULL);
#endif
    }
  } else {
    if (xvimagesink->event_thread) {
      GST_DEBUG_OBJECT (xvimagesink, "stop xevent thread, expose %d, events %d",
          xvimagesink->handle_expose, xvimagesink->handle_events);
      xvimagesink->running = FALSE;
      /* grab thread and mark it as NULL */
      thread = xvimagesink->event_thread;
      xvimagesink->event_thread = NULL;
    }
  }
  GST_OBJECT_UNLOCK (xvimagesink);

  /* Wait for our event thread to finish */
  if (thread)
    g_thread_join (thread);

}


#ifdef GST_EXT_XV_ENHANCEMENT
/**
 * gst_xvimagesink_prepare_xid:
 * @overlay: a #GstXOverlay which does not yet have an XWindow or XPixmap.
 *
 * This will post a "prepare-xid" element message with video size and display size on the bus
 * to give applications an opportunity to call
 * gst_x_overlay_set_xwindow_id() before a plugin creates its own
 * window or pixmap.
 *
 * This function should only be used by video overlay plugin developers.
 */
static void
gst_xvimagesink_prepare_xid (GstXOverlay * overlay)
{
  GstStructure *s;
  GstMessage *msg;

  g_return_if_fail (overlay != NULL);
  g_return_if_fail (GST_IS_X_OVERLAY (overlay));

  GstXvImageSink *xvimagesink;
  xvimagesink = GST_XVIMAGESINK (GST_OBJECT (overlay));

  GST_DEBUG ("post \"prepare-xid\" element message with video-width(%d), video-height(%d), display-width(%d), display-height(%d)",
        GST_VIDEO_SINK_WIDTH (xvimagesink), GST_VIDEO_SINK_HEIGHT (xvimagesink), xvimagesink->xcontext->width, xvimagesink->xcontext->height);

  GST_LOG_OBJECT (GST_OBJECT (overlay), "prepare xid");
  s = gst_structure_new ("prepare-xid",
        "video-width", G_TYPE_INT, GST_VIDEO_SINK_WIDTH (xvimagesink),
        "video-height", G_TYPE_INT, GST_VIDEO_SINK_HEIGHT (xvimagesink),
        "display-width", G_TYPE_INT, xvimagesink->xcontext->width,
        "display-height", G_TYPE_INT, xvimagesink->xcontext->height,
        NULL);
  msg = gst_message_new_element (GST_OBJECT (overlay), s);
  gst_element_post_message (GST_ELEMENT (overlay), msg);
}
#endif /* GST_EXT_XV_ENHANCEMENT */


/* This function calculates the pixel aspect ratio based on the properties
 * in the xcontext structure and stores it there. */
static void
gst_xvimagesink_calculate_pixel_aspect_ratio (GstXContext * xcontext)
{
  static const gint par[][2] = {
    {1, 1},                     /* regular screen */
    {16, 15},                   /* PAL TV */
    {11, 10},                   /* 525 line Rec.601 video */
    {54, 59},                   /* 625 line Rec.601 video */
    {64, 45},                   /* 1280x1024 on 16:9 display */
    {5, 3},                     /* 1280x1024 on 4:3 display */
    {4, 3}                      /*  800x600 on 16:9 display */
  };
  gint i;
  gint index;
  gdouble ratio;
  gdouble delta;

#define DELTA(idx) (ABS (ratio - ((gdouble) par[idx][0] / par[idx][1])))

  /* first calculate the "real" ratio based on the X values;
   * which is the "physical" w/h divided by the w/h in pixels of the display */
  ratio = (gdouble) (xcontext->widthmm * xcontext->height)
      / (xcontext->heightmm * xcontext->width);

  /* DirectFB's X in 720x576 reports the physical dimensions wrong, so
   * override here */
  if (xcontext->width == 720 && xcontext->height == 576) {
    ratio = 4.0 * 576 / (3.0 * 720);
  }
  GST_DEBUG ("calculated pixel aspect ratio: %f", ratio);
  /* now find the one from par[][2] with the lowest delta to the real one */
  delta = DELTA (0);
  index = 0;

  for (i = 1; i < sizeof (par) / (sizeof (gint) * 2); ++i) {
    gdouble this_delta = DELTA (i);

    if (this_delta < delta) {
      index = i;
      delta = this_delta;
    }
  }

  GST_DEBUG ("Decided on index %d (%d/%d)", index,
      par[index][0], par[index][1]);

  g_free (xcontext->par);
  xcontext->par = g_new0 (GValue, 1);
  g_value_init (xcontext->par, GST_TYPE_FRACTION);
  gst_value_set_fraction (xcontext->par, par[index][0], par[index][1]);
  GST_DEBUG ("set xcontext PAR to %d/%d",
      gst_value_get_fraction_numerator (xcontext->par),
      gst_value_get_fraction_denominator (xcontext->par));
}

/* This function gets the X Display and global info about it. Everything is
   stored in our object and will be cleaned when the object is disposed. Note
   here that caps for supported format are generated without any window or
   image creation */
static GstXContext *
gst_xvimagesink_xcontext_get (GstXvImageSink * xvimagesink)
{
  GstXContext *xcontext = NULL;
  XPixmapFormatValues *px_formats = NULL;
  gint nb_formats = 0, i, j, N_attr;
  XvAttribute *xv_attr;
  Atom prop_atom;
  const char *channels[4] = { "XV_HUE", "XV_SATURATION",
    "XV_BRIGHTNESS", "XV_CONTRAST"
  };

  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), NULL);

  xcontext = g_new0 (GstXContext, 1);
  xcontext->im_format = 0;

  g_mutex_lock (xvimagesink->x_lock);

  if (!XInitThreads()){
	GST_ERROR_OBJECT (xvimagesink, "XInitThreads() failed");
	return NULL;
  }
  
  xcontext->disp = XOpenDisplay (xvimagesink->display_name);

  if (!xcontext->disp) {
    g_mutex_unlock (xvimagesink->x_lock);
    g_free (xcontext);
    GST_ELEMENT_ERROR (xvimagesink, RESOURCE, WRITE,
        ("Could not initialise Xv output"), ("Could not open display"));
    return NULL;
  }

  xcontext->screen = DefaultScreenOfDisplay (xcontext->disp);
  xcontext->screen_num = DefaultScreen (xcontext->disp);
  xcontext->visual = DefaultVisual (xcontext->disp, xcontext->screen_num);
  xcontext->root = DefaultRootWindow (xcontext->disp);
  xcontext->white = XWhitePixel (xcontext->disp, xcontext->screen_num);
  xcontext->black = XBlackPixel (xcontext->disp, xcontext->screen_num);
  xcontext->depth = DefaultDepthOfScreen (xcontext->screen);

  xcontext->width = DisplayWidth (xcontext->disp, xcontext->screen_num);
  xcontext->height = DisplayHeight (xcontext->disp, xcontext->screen_num);
  xcontext->widthmm = DisplayWidthMM (xcontext->disp, xcontext->screen_num);
  xcontext->heightmm = DisplayHeightMM (xcontext->disp, xcontext->screen_num);

  GST_DEBUG_OBJECT (xvimagesink, "X reports %dx%d pixels and %d mm x %d mm",
      xcontext->width, xcontext->height, xcontext->widthmm, xcontext->heightmm);

  gst_xvimagesink_calculate_pixel_aspect_ratio (xcontext);
  /* We get supported pixmap formats at supported depth */
  px_formats = XListPixmapFormats (xcontext->disp, &nb_formats);

  if (!px_formats) {
    XCloseDisplay (xcontext->disp);
    g_mutex_unlock (xvimagesink->x_lock);
    g_free (xcontext->par);
    g_free (xcontext);
    GST_ELEMENT_ERROR (xvimagesink, RESOURCE, SETTINGS,
        ("Could not initialise Xv output"), ("Could not get pixel formats"));
    return NULL;
  }

  /* We get bpp value corresponding to our running depth */
  for (i = 0; i < nb_formats; i++) {
    if (px_formats[i].depth == xcontext->depth)
      xcontext->bpp = px_formats[i].bits_per_pixel;
  }

  XFree (px_formats);

  xcontext->endianness =
      (ImageByteOrder (xcontext->disp) ==
      LSBFirst) ? G_LITTLE_ENDIAN : G_BIG_ENDIAN;

  /* our caps system handles 24/32bpp RGB as big-endian. */
  if ((xcontext->bpp == 24 || xcontext->bpp == 32) &&
      xcontext->endianness == G_LITTLE_ENDIAN) {
    xcontext->endianness = G_BIG_ENDIAN;
    xcontext->visual->red_mask = GUINT32_TO_BE (xcontext->visual->red_mask);
    xcontext->visual->green_mask = GUINT32_TO_BE (xcontext->visual->green_mask);
    xcontext->visual->blue_mask = GUINT32_TO_BE (xcontext->visual->blue_mask);
    if (xcontext->bpp == 24) {
      xcontext->visual->red_mask >>= 8;
      xcontext->visual->green_mask >>= 8;
      xcontext->visual->blue_mask >>= 8;
    }
  }

  xcontext->caps = gst_xvimagesink_get_xv_support (xvimagesink, xcontext);

  if (!xcontext->caps) {
    XCloseDisplay (xcontext->disp);
    g_mutex_unlock (xvimagesink->x_lock);
    g_free (xcontext->par);
    g_free (xcontext);
    /* GST_ELEMENT_ERROR is thrown by gst_xvimagesink_get_xv_support */
    return NULL;
  }
#ifdef HAVE_XSHM
  /* Search for XShm extension support */
  if (XShmQueryExtension (xcontext->disp) &&
      gst_xvimagesink_check_xshm_calls (xcontext)) {
    xcontext->use_xshm = TRUE;
    GST_DEBUG ("xvimagesink is using XShm extension");
  } else
#endif /* HAVE_XSHM */
  {
    xcontext->use_xshm = FALSE;
    GST_DEBUG ("xvimagesink is not using XShm extension");
  }

  xv_attr = XvQueryPortAttributes (xcontext->disp,
      xcontext->xv_port_id, &N_attr);


  /* Generate the channels list */
  for (i = 0; i < (sizeof (channels) / sizeof (char *)); i++) {
    XvAttribute *matching_attr = NULL;

    /* Retrieve the property atom if it exists. If it doesn't exist,
     * the attribute itself must not either, so we can skip */
    prop_atom = XInternAtom (xcontext->disp, channels[i], True);
    if (prop_atom == None)
      continue;

    if (xv_attr != NULL) {
      for (j = 0; j < N_attr && matching_attr == NULL; ++j)
        if (!g_ascii_strcasecmp (channels[i], xv_attr[j].name))
          matching_attr = xv_attr + j;
    }

    if (matching_attr) {
      GstColorBalanceChannel *channel;

      channel = g_object_new (GST_TYPE_COLOR_BALANCE_CHANNEL, NULL);
      channel->label = g_strdup (channels[i]);
      channel->min_value = matching_attr->min_value;
      channel->max_value = matching_attr->max_value;

      xcontext->channels_list = g_list_append (xcontext->channels_list,
          channel);

      /* If the colorbalance settings have not been touched we get Xv values
         as defaults and update our internal variables */
      if (!xvimagesink->cb_changed) {
        gint val;

        XvGetPortAttribute (xcontext->disp, xcontext->xv_port_id,
            prop_atom, &val);
        /* Normalize val to [-1000, 1000] */
        val = floor (0.5 + -1000 + 2000 * (val - channel->min_value) /
            (double) (channel->max_value - channel->min_value));

        if (!g_ascii_strcasecmp (channels[i], "XV_HUE"))
          xvimagesink->hue = val;
        else if (!g_ascii_strcasecmp (channels[i], "XV_SATURATION"))
          xvimagesink->saturation = val;
        else if (!g_ascii_strcasecmp (channels[i], "XV_BRIGHTNESS"))
          xvimagesink->brightness = val;
        else if (!g_ascii_strcasecmp (channels[i], "XV_CONTRAST"))
          xvimagesink->contrast = val;
      }
    }
  }

  if (xv_attr) { /* To select specific scaler*/

    if (xvimagesink->device_id == DefaultDeviceId) {
      GST_WARNING_OBJECT(xvimagesink, "The actual device id has not been assigned");
    } else {
      GST_LOG_OBJECT(xvimagesink, "device-id[%d]", xvimagesink->device_id);
    }

    if (i == N_attr)
      GST_ERROR_OBJECT(xvimagesink, "There is no attribute <_USER_WM_PORT_ATTRIBUTE_SCALER>");
  }

  if (xv_attr)
    XFree (xv_attr);

#ifdef GST_EXT_XV_ENHANCEMENT
  set_display_mode(xcontext, xvimagesink->display_mode);
#endif /* GST_EXT_XV_ENHANCEMENT */

  g_mutex_unlock (xvimagesink->x_lock);

  return xcontext;
}

/* This function cleans the X context. Closing the Display, releasing the XV
   port and unrefing the caps for supported formats. */
static void
gst_xvimagesink_xcontext_clear (GstXvImageSink * xvimagesink)
{
  GList *formats_list, *channels_list;
  GstXContext *xcontext;
  gint i = 0;

  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  GST_OBJECT_LOCK (xvimagesink);
  if (xvimagesink->xcontext == NULL) {
    GST_OBJECT_UNLOCK (xvimagesink);
    return;
  }

  /* Take the XContext from the sink and clean it up */
  xcontext = xvimagesink->xcontext;
  xvimagesink->xcontext = NULL;

  GST_OBJECT_UNLOCK (xvimagesink);


  formats_list = xcontext->formats_list;

  while (formats_list) {
    GstXvImageFormat *format = formats_list->data;

    gst_caps_unref (format->caps);
    g_free (format);
    formats_list = g_list_next (formats_list);
  }

  if (xcontext->formats_list)
    g_list_free (xcontext->formats_list);

  channels_list = xcontext->channels_list;

  while (channels_list) {
    GstColorBalanceChannel *channel = channels_list->data;

    g_object_unref (channel);
    channels_list = g_list_next (channels_list);
  }

  if (xcontext->channels_list)
    g_list_free (xcontext->channels_list);

  gst_caps_unref (xcontext->caps);
  if (xcontext->last_caps)
    gst_caps_replace (&xcontext->last_caps, NULL);

  for (i = 0; i < xcontext->nb_adaptors; i++) {
    g_free (xcontext->adaptors[i]);
  }

  g_free (xcontext->adaptors);

  g_free (xcontext->par);

  g_mutex_lock (xvimagesink->x_lock);

  GST_DEBUG_OBJECT (xvimagesink, "Closing display and freeing X Context");

  XvUngrabPort (xcontext->disp, xcontext->xv_port_id, 0);

  XCloseDisplay (xcontext->disp);

  g_mutex_unlock (xvimagesink->x_lock);

  g_free (xcontext);
}

static void
gst_xvimagesink_imagepool_clear (GstXvImageSink * xvimagesink)
{
  g_mutex_lock (xvimagesink->pool_lock);

  while (xvimagesink->image_pool) {
    GstXvImageBuffer *xvimage = xvimagesink->image_pool->data;

    xvimagesink->image_pool = g_slist_delete_link (xvimagesink->image_pool,
        xvimagesink->image_pool);
    gst_xvimage_buffer_free (xvimage);
  }

  g_mutex_unlock (xvimagesink->pool_lock);
}

/* Element stuff */

/* This function tries to get a format matching with a given caps in the
   supported list of formats we generated in gst_xvimagesink_get_xv_support */
static gint
gst_xvimagesink_get_format_from_caps (GstXvImageSink * xvimagesink,
    GstCaps * caps)
{
  GList *list = NULL;

  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), 0);

  list = xvimagesink->xcontext->formats_list;

  while (list) {
    GstXvImageFormat *format = list->data;

    if (format) {
      if (gst_caps_can_intersect (caps, format->caps)) {
        return format->format;
      }
    }
    list = g_list_next (list);
  }

  return -1;
}

static GstCaps *
gst_xvimagesink_getcaps (GstBaseSink * bsink)
{
  GstXvImageSink *xvimagesink;

  xvimagesink = GST_XVIMAGESINK (bsink);

  if (xvimagesink->xcontext)
    return gst_caps_ref (xvimagesink->xcontext->caps);

  return
      gst_caps_copy (gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD
          (xvimagesink)));
}

static gboolean get_color_info(GstXvImageSink *xvimagesink, GstStructure* structure, avoc_color_space_info* color_info)
{
	g_return_val_if_fail(structure, FALSE);
	g_return_val_if_fail(color_info, FALSE);

	int iColorPrimaries = 0;
	int iTransferCharacteristics = 0;
	int iMatrixCoefficients = 0;
	gboolean result = TRUE;
	/* Value not used now */
	color_info->uiBitsDepthLuma =  0;
	color_info->uiBitsDepthChroma =  0;
	color_info->iFullRange = 0;

	gboolean ret = gst_structure_get_int (structure, "iColorPrimaries", &iColorPrimaries);
	result &= ret;
	if (ret)
	{
		color_info->iColorPrimaries = iColorPrimaries;
	}
	else
	{
		color_info->iColorPrimaries = 0;
	}

	ret = gst_structure_get_int (structure, "iTransferCharacteristics", &iTransferCharacteristics);
	result &= ret;
	if (ret)
	{
		color_info->iTransferCharacteristics = iTransferCharacteristics;
	}
	else
	{
		color_info->iTransferCharacteristics = 0;
	}

	ret = gst_structure_get_int (structure, "iMatrixCoefficients", &iMatrixCoefficients);
	result &= ret;
	if (ret)
	{
		color_info->iMatrixCoefficients = iMatrixCoefficients;
	}
	else
	{
		color_info->iMatrixCoefficients = 0;
	}

	if(iColorPrimaries > 0 || iTransferCharacteristics > 0 || iMatrixCoefficients > 0)
	{
		if(result)
		{
			color_info->bValid = 1;
		}
		else
		{
			color_info->bValid = 0;
		}
	}
	else
	{
		color_info->bValid = 0;
	}

	GST_INFO_OBJECT(xvimagesink, "result[ %s ], bValid[ %d ], uiBitsDepthLuma[ %d ], uiBitsDepthChroma[ %d ], iColorPrimaries[ %d ], iTransferCharacteristics[ %d ], iMatrixCoefficients[ %d ], iFullRange[ %d ]",
		result?"Success":"Fail to find", color_info->bValid, color_info->uiBitsDepthLuma, color_info->uiBitsDepthChroma, color_info->iColorPrimaries, color_info->iTransferCharacteristics, color_info->iMatrixCoefficients, color_info->iFullRange);
	return result;
}

static gboolean is_different_color_info(avoc_color_space_info* color_info1, avoc_color_space_info* color_info2)
{
	if (color_info1->bValid == color_info2->bValid && 
		color_info1->uiBitsDepthLuma == color_info2->uiBitsDepthLuma && 
		color_info1->uiBitsDepthChroma == color_info2->uiBitsDepthChroma && 
		color_info1->iColorPrimaries == color_info2->iColorPrimaries && 
		color_info1->iTransferCharacteristics == color_info2->iTransferCharacteristics && 
		color_info1->iMatrixCoefficients == color_info2->iMatrixCoefficients && 
		color_info1->iFullRange == color_info2->iFullRange && 
		color_info1->bValid == color_info2->bValid)
	{
		return FALSE;
	}

	return TRUE;	
}

static void
gst_xvimagesink_update_avoc_color_space_info(GstXvImageSink *xvimagesink, GstStructure *structure, gboolean sync)
{
	if (FIND_MASK(xvimagesink->video_quality_mode ,VQ_MODE_EXTERNAL_AVOC_SET))
	{
		GST_LOG_OBJECT(xvimagesink, "no need to set colorspace info for VQ_MODE_EXTERNAL_AVOC_SET, video_quality_mode[ 0x%x ]", xvimagesink->video_quality_mode);
		return;
	}

	guint64 T1=0, T2=0;
	avoc_color_space_info new_color_info;
	memset(&new_color_info, 0, sizeof(avoc_color_space_info));
	gboolean need_to_set = FALSE;
	avoc_error_e ret = AVOC_EXIT_SUCCESS;
 	if (get_color_info(xvimagesink, structure, &new_color_info))
 	{
 		// If the color info is updated during the playback..
 		need_to_set = is_different_color_info(&xvimagesink->prev_color_info, &new_color_info);
 		GST_LOG_OBJECT(xvimagesink, "result of comparison[ %d ]", need_to_set);
 	}
 	else
 	{
 		if (xvimagesink->is_first_putimage)  // Set once before 1st putimage, if it is default case.
 		{
 		  GST_INFO_OBJECT(xvimagesink, "no response for the color_info query");
 		  need_to_set = TRUE; // need to set vValid = FALSE  even if there is no meaningful value at the beginning of playback,
 		  new_color_info.bValid = FALSE;
 		}
 	}

    if (need_to_set)
    {
      if (xvimagesink->need_update_color_info == TRUE)
      {
        GST_ERROR_OBJECT(xvimagesink, "previous color-info is not set yet by gst_xvimagesink_update_video_quality().");
        return;
      }

      xvimagesink->prev_color_info_format = gst_xvimagesink_set_videocodec_info(xvimagesink, structure);
      xvimagesink->prev_color_info = new_color_info;
      if (sync)
      {
        guint64 T1 = get_time();
        ret = avoc_set_color_space_info(xvimagesink->prev_color_info_format, &xvimagesink->prev_color_info);	// must be set between  pre_avoc_set_resolutionand post.
        guint64 T2 = get_time();
        if (ret != AVOC_EXIT_SUCCESS)
          GST_ERROR_OBJECT(xvimagesink, "Fail to avoc_set_color_space_info[ %lld ms ], ret[ %d ]", (T2-T1)/1000, ret);
      }
      else
      {
        /* will be updated by gst_xvimagesink_update_video_quality() in another thread */
        xvimagesink->need_update_color_info = TRUE;
      }

      GST_ERROR_OBJECT(xvimagesink,  "avoc_set_color_space_info ret[ %d | %lld ms ], color_info_valid[ %d ] uiBitsDepthLuma[ %d ] uiBitsDepthChroma[ %d ] iColorPrimaries[ %d ]"
        "iTransferCharacteristics[ %d ] iMatrixCoefficients[ %d ] iFullRange[ %d ]"
        , ret, (T2-T1)/1000, new_color_info.bValid, new_color_info.uiBitsDepthLuma, new_color_info.uiBitsDepthChroma, new_color_info.iColorPrimaries
        , new_color_info.iTransferCharacteristics, new_color_info.iMatrixCoefficients, new_color_info.iFullRange);
    }
    return;
}

static gboolean
gst_xvimagesink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstXvImageSink *xvimagesink = NULL;
  GstStructure *structure = NULL;
  GstStructure *oldStructure = NULL;
  guint32 im_format = 0;
  gboolean ret;
  gint video_width, video_height;
  gint disp_x, disp_y;
  gint disp_width, disp_height;
  gint video_par_n, video_par_d;        /* video's PAR */
  gint display_par_n, display_par_d;    /* display's PAR */
  const GValue *caps_par;
  const GValue *caps_disp_reg;
  const GValue *fps;
  guint num, den;
#ifdef GST_EXT_XV_ENHANCEMENT
  gboolean enable_last_buffer;
#endif /* #ifdef GST_EXT_XV_ENHANCEMENT */

  xvimagesink = GST_XVIMAGESINK (bsink);

  GST_DEBUG_OBJECT (xvimagesink, "In setcaps. Possible caps %" GST_PTR_FORMAT, xvimagesink->xcontext->caps);
  GST_DEBUG_OBJECT (xvimagesink, "New setting caps %" GST_PTR_FORMAT, caps);

  if (!gst_caps_can_intersect (xvimagesink->xcontext->caps, caps))
    goto incompatible_caps;
  GstElement *prevcaps = GST_PAD_CAPS(bsink->sinkpad);
  structure = gst_caps_get_structure (caps, 0);
  oldStructure = gst_caps_get_structure (prevcaps, 0);
  ret = gst_structure_get_int (structure, "width", &video_width);
  ret &= gst_structure_get_int (structure, "height", &video_height);
  fps = gst_structure_get_value (structure, "framerate");
  ret &= (fps != NULL);
  if (!ret)
    goto incomplete_caps;

#ifdef GST_EXT_XV_ENHANCEMENT
  xvimagesink->aligned_width = video_width;
  xvimagesink->aligned_height = video_height;

  /* get enable-last-buffer */
  g_object_get(G_OBJECT(xvimagesink), "enable-last-buffer", &enable_last_buffer, NULL);
  GST_INFO_OBJECT(xvimagesink, "current enable-last-buffer : %d", enable_last_buffer);

  /* flush if enable-last-buffer is TRUE */
  if (enable_last_buffer) {
    GST_INFO_OBJECT(xvimagesink, "flush last-buffer");
    g_object_set(G_OBJECT(xvimagesink), "enable-last-buffer", FALSE, NULL);
    g_object_set(G_OBJECT(xvimagesink), "enable-last-buffer", TRUE, NULL);
  }
#endif /* GST_EXT_XV_ENHANCEMENT */

  xvimagesink->fps_n = gst_value_get_fraction_numerator (fps);
  xvimagesink->fps_d = gst_value_get_fraction_denominator (fps);
  
#if ENABLE_VF_ROTATE /* add for picture size init*/
  xvimagesink->vf_iScaledWidth = xvimagesink->video_width = video_width;
  xvimagesink->vf_iScaledHeight = xvimagesink->video_height = video_height;
#else
  xvimagesink->video_width = video_width;
  xvimagesink->video_height = video_height;
#endif

  /* Get max W/H ,  If there is no max W/H,  then use current w/h. */
  if (!gst_structure_get_int (structure, "maxwidth", &xvimagesink->max_video_width))
  	xvimagesink->max_video_width = xvimagesink->video_width;
  if (xvimagesink->max_video_width < xvimagesink->video_width) // To guarantee that the max_w is higher than src resolution
	xvimagesink->max_video_width = xvimagesink->video_width;
  if (!gst_structure_get_int (structure, "maxheight", &xvimagesink->max_video_height))
  	xvimagesink->max_video_height = xvimagesink->video_height;
  if (xvimagesink->max_video_height < xvimagesink->video_height) // To guarantee that the max_h is higher than src resolution
  	xvimagesink->max_video_height = xvimagesink->video_height;

  im_format = gst_xvimagesink_get_format_from_caps (xvimagesink, caps);
  if (im_format == -1)
    goto invalid_format;

  /* Fix the V/P display buftype flag conflict issue,  2014-08-09*/
#if ENABLE_RT_DISPLAY
	if(xvimagesink->rt_display_vaule == 1){
		if(!xvimagesink->rt_display_avoc_done){	// To control reset caps not change to display channel again
			GST_ERROR_OBJECT(xvimagesink, "RT: Resolution Init: width/height = %d/%d ", xvimagesink->video_width, xvimagesink->video_height);
			if(xvimagesink->xcontext){
			  GstXContext* xcontext = xvimagesink->xcontext;
			  int i = 0, count = 0;
			  XvAttribute *const attr = XvQueryPortAttributes(xcontext->disp, xcontext->xv_port_id, &count);
			  if (attr) {
				for (i = 0 ; i < count ; i++) {
				  if (!strcmp(attr[i].name, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE")) {
				  Atom atom_butype = XInternAtom(xcontext->disp, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE", FALSE);
				  gint ret = XvSetPortAttribute(xcontext->disp, xcontext->xv_port_id, atom_butype, 0); // 0:bypass hw decoded frame(default)  1:index of MP framebuffer.
				  if (ret != Success){
					  GST_ERROR_OBJECT(xvimagesink, "Default: Failed    _USER_WM_PORT_ATTRIBUTE_BUFTYPE[index %d]  -> found ret[ %d ], xvimage[ %p ]", i, ret, xvimagesink->xvimage);
				       }else{
					  GST_DEBUG_OBJECT(xvimagesink, "Default: Success _USER_WM_PORT_ATTRIBUTE_BUFTYPE[index %d] -> xvimage[ %p ]", i, xvimagesink->xvimage);
					}
					break;
				  }
				}
				XFree(attr);
			  } else {
				GST_WARNING_OBJECT(xvimagesink, "Default: Failed XvQueryPortAttributes -> disp:%d, port_id:%d ", xcontext->disp, xcontext->xv_port_id);
			  }
  		    }
		}else {
			GST_ERROR_OBJECT(xvimagesink, "RT: Resolution Changed: width/height = %d/%d ", xvimagesink->video_width, xvimagesink->video_height);
		}
	}else
#endif	
	{
		if(xvimagesink->xcontext){
			  GstXContext* xcontext = xvimagesink->xcontext;
			  int i = 0, count = 0;
			  XvAttribute *const attr = XvQueryPortAttributes(xcontext->disp, xcontext->xv_port_id, &count);
			  if (attr) {
				for (i = 0 ; i < count ; i++) {
				  if (!strcmp(attr[i].name, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE")) {
				  Atom atom_butype = XInternAtom(xcontext->disp, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE", FALSE);
				  gint ret = XvSetPortAttribute(xcontext->disp, xcontext->xv_port_id, atom_butype, 0); // 0:bypass hw decoded frame(default)  1:index of MP framebuffer.
				  if (ret != Success){
					  GST_ERROR_OBJECT(xvimagesink, "Default: Failed    _USER_WM_PORT_ATTRIBUTE_BUFTYPE[index %d]  -> found ret[ %d ], xvimage[ %p ]", i, ret, xvimagesink->xvimage);
				       }else{
					  GST_DEBUG_OBJECT(xvimagesink, "Default: Success _USER_WM_PORT_ATTRIBUTE_BUFTYPE[index %d] -> xvimage[ %p ]", i, xvimagesink->xvimage);
					}
					break;
				  }
				}
				XFree(attr);
			  } else {
				GST_WARNING_OBJECT(xvimagesink, "Default: Failed XvQueryPortAttributes -> disp:%d, port_id:%d ", xcontext->disp, xcontext->xv_port_id);
			  }
  		}

#ifdef USE_TBM
		  if ((im_format == GST_MAKE_FOURCC('S', 'T', 'V', '1')) && xvimagesink->xcontext) {
			GstXContext* xcontext = xvimagesink->xcontext;
			int i = 0, count = 0;
			XvAttribute *const attr = XvQueryPortAttributes(xcontext->disp, xcontext->xv_port_id, &count);
			if (attr) {
			  for (i = 0 ; i < count ; i++) {
				if (!strcmp(attr[i].name, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE")) {
				Atom atom_butype = XInternAtom(xcontext->disp, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE", FALSE);
				gint ret = XvSetPortAttribute(xcontext->disp, xcontext->xv_port_id, atom_butype, 1); // 0:bypass hw decoded frame(default)	1:index of MP framebuffer.
				if (ret != Success)
					GST_ERROR_OBJECT(xvimagesink, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE[index %d] found ret[ %d ], xvimage[ %p ]", i, ret, xvimagesink->xvimage);
				  else
					GST_WARNING_OBJECT(xvimagesink, "_USER_WM_PORT_ATTRIBUTE_BUFTYPE[index %d], xvimage[ %p ]", i, xvimagesink->xvimage);
				  break;
				}
			  }
			  XFree(attr);
			} else {
			  GST_WARNING_OBJECT(xvimagesink, "XvQueryPortAttributes disp:%d, port_id:%d failed", xcontext->disp, xcontext->xv_port_id);
			}
		  }
#endif

#if ENABLE_VF_ROTATE
	  if((im_format == GST_MAKE_FOURCC('S', 'T', 'V', '0'))&& xvimagesink->xcontext){
	
			if(!set_rotate_video_display_channel(xvimagesink, xvimagesink->vf_display_channel, 0)){
				  GST_WARNING_OBJECT (xvimagesink, "Rotate: set video display channel failed !!!");
			}
			 /* For judgment video codec format rotation,
				 CANRotate 0: not support, 1: support, 2014-09-02 */
			if(gst_structure_get_int (structure, "CANRotate", &xvimagesink->CANRotate)){
				GST_INFO_OBJECT(xvimagesink, "Get CANRotate  %d ", xvimagesink->CANRotate);
			}else {
				GST_INFO_OBJECT(xvimagesink, "Fail to get CANRotate !!! ");
			}
			/*3D video not support rotate*/
			const gchar* str3Dformat = gst_structure_get_string(structure, "3Dformat");
			if(str3Dformat){
				GST_INFO_OBJECT(xvimagesink, "This is 3D video !!! ");
				xvimagesink->CANRotate = 0;
			}
	
			/* for auto change degree from mov container, 2014-06-13 */
			if(gst_structure_get_int (structure, "ARdegree", &xvimagesink->ARdegree)){
				GST_INFO_OBJECT(xvimagesink, "Get MOV ARdegree	%d ", xvimagesink->ARdegree);
			}else {
				GST_INFO_OBJECT(xvimagesink, "Fail to get MOV ARdegree !!! ");
			}
		 }
#endif
	}

  	set_display_dp_linesize(xvimagesink);
/*
#if ENABLE_RT_DISPLAY
	if(xvimagesink->rt_display_vaule == 1){
		xvimagesink->rt_resetinfo_done = 0;
		//mute_video_display(xvimagesink,TRUE,MUTE_STATE_RT_CHANGE);
	}
#endif
*/

  /* get aspect ratio from caps if it's present, and
   * convert video width and height to a display width and height
   * using wd / hd = wv / hv * PARv / PARd */

  /* get video's PAR */
  caps_par = gst_structure_get_value (structure, "pixel-aspect-ratio");
  if (caps_par) {
    video_par_n = gst_value_get_fraction_numerator (caps_par);
    video_par_d = gst_value_get_fraction_denominator (caps_par);
  } else {
    video_par_n = 1;
    video_par_d = 1;
  }
  /* get display's PAR */
  if (xvimagesink->par) {
    display_par_n = gst_value_get_fraction_numerator (xvimagesink->par);
    display_par_d = gst_value_get_fraction_denominator (xvimagesink->par);
  } else {
    display_par_n = 1;
    display_par_d = 1;
  }

/*video cropping,if  xvimagesink->disp_* is not set,keep video_width and video_height*/
if( !xvimagesink->crop_flag ){
	  /* get the display region */
	  caps_disp_reg = gst_structure_get_value (structure, "display-region");
	  if (caps_disp_reg) {
	    disp_x = g_value_get_int (gst_value_array_get_value (caps_disp_reg, 0));
	    disp_y = g_value_get_int (gst_value_array_get_value (caps_disp_reg, 1));
	    disp_width = g_value_get_int (gst_value_array_get_value (caps_disp_reg, 2));
	    disp_height =
	        g_value_get_int (gst_value_array_get_value (caps_disp_reg, 3));
		GST_DEBUG_OBJECT (xvimagesink,
			"if(caps_disp_reg),disp_x: %d, disp_y: %d,disp_width :%d,disp_height:%d\n",
			disp_x, disp_y, disp_width, disp_height);

	  } else {
	    disp_x = video_width;
	    disp_y = video_height;
	    disp_width = video_width;
	    disp_height = video_height;
		GST_DEBUG_OBJECT (xvimagesink,
		"if(caps_disp_reg),else : disp_x: %d, disp_y: %d,disp_width :%d,disp_height:%d\n",
		disp_x, disp_y, disp_width, disp_height);
	  }
	  xvimagesink->disp_x = (gint)(disp_x * xvimagesink->disp_x_ratio);
	  xvimagesink->disp_y = (gint)(disp_y * xvimagesink->disp_y_ratio);
	  xvimagesink->disp_width = (gint)(disp_width * xvimagesink->disp_width_ratio);
	  xvimagesink->disp_height = (gint)(disp_height * xvimagesink->disp_height_ratio);
}

  if (!gst_video_calculate_display_ratio (&num, &den, video_width,
          video_height, video_par_n, video_par_d, display_par_n, display_par_d))
    goto no_disp_ratio;


  GST_DEBUG_OBJECT (xvimagesink,
	  "xvimagesink->disp_x : %d, xvimagesink->disp_y: %d,xvimagesink->disp_width :%d,xvimagesink->disp_height:%d\n",
	  xvimagesink->disp_x, xvimagesink->disp_y, xvimagesink->disp_width, xvimagesink->disp_height);

  GST_DEBUG_OBJECT (xvimagesink,
      "video width/height: %dx%d, calculated display ratio: %d/%d",
      video_width, video_height, num, den);

  /* now find a width x height that respects this display ratio.
   * prefer those that have one of w/h the same as the incoming video
   * using wd / hd = num / den */

  /* start with same height, because of interlaced video */
  /* check hd / den is an integer scale factor, and scale wd with the PAR */
  if (video_height % den == 0) {
    GST_DEBUG_OBJECT (xvimagesink, "keeping video height");
    GST_VIDEO_SINK_WIDTH (xvimagesink) = (guint)
        gst_util_uint64_scale_int (video_height, num, den);
    GST_VIDEO_SINK_HEIGHT (xvimagesink) = video_height;
  } else if (video_width % num == 0) {
    GST_DEBUG_OBJECT (xvimagesink, "keeping video width");
    GST_VIDEO_SINK_WIDTH (xvimagesink) = video_width;
    GST_VIDEO_SINK_HEIGHT (xvimagesink) = (guint)
        gst_util_uint64_scale_int (video_width, den, num);
  } else {
    GST_DEBUG_OBJECT (xvimagesink, "approximating while keeping video height");
    GST_VIDEO_SINK_WIDTH (xvimagesink) = (guint)
        gst_util_uint64_scale_int (video_height, num, den);
    GST_VIDEO_SINK_HEIGHT (xvimagesink) = video_height;
  }
  GST_DEBUG_OBJECT (xvimagesink, "scaling to %dx%d",
      GST_VIDEO_SINK_WIDTH (xvimagesink), GST_VIDEO_SINK_HEIGHT (xvimagesink));

  /* Notify application to set xwindow id now */
  g_mutex_lock (xvimagesink->flow_lock);
#ifdef GST_EXT_XV_ENHANCEMENT
  if (!xvimagesink->xwindow && !xvimagesink->get_pixmap_cb) {
    g_mutex_unlock (xvimagesink->flow_lock);
    gst_xvimagesink_prepare_xid (GST_X_OVERLAY (xvimagesink));
#else
  if (!xvimagesink->xwindow) {
    g_mutex_unlock (xvimagesink->flow_lock);
    gst_x_overlay_prepare_xwindow_id (GST_X_OVERLAY (xvimagesink));
#endif
  } else {
    g_mutex_unlock (xvimagesink->flow_lock);
  }

  /* Creating our window and our image with the display size in pixels */
  if (GST_VIDEO_SINK_WIDTH (xvimagesink) <= 0 ||
      GST_VIDEO_SINK_HEIGHT (xvimagesink) <= 0)
    goto no_display_size;

#if (ENABLE_RT_SEAMLESS_HW_SCALER || ENABLE_RT_SEAMLESS_GA_SCALER)
    if(xvimagesink->rt_display_vaule == 1){
	  if(xvimagesink->video_width > 1920 || xvimagesink->video_height > 1080){
	  	  GST_ERROR_OBJECT(xvimagesink, "RT Display not support resolution [%d]/[%d]", 
		  	xvimagesink->video_width, xvimagesink->video_height);
		  goto no_display_size;
	  }
   }
#endif

  g_mutex_lock (xvimagesink->flow_lock);
#ifdef GST_EXT_XV_ENHANCEMENT
  if (!xvimagesink->xwindow && !xvimagesink->get_pixmap_cb) {
    GST_DEBUG_OBJECT (xvimagesink, "xwindow is null and not multi-pixmaps usage case");
#else
  if (!xvimagesink->xwindow) {
#endif
    xvimagesink->xwindow = gst_xvimagesink_xwindow_new (xvimagesink,
        GST_VIDEO_SINK_WIDTH (xvimagesink),
        GST_VIDEO_SINK_HEIGHT (xvimagesink));
  }

  /* After a resize, we want to redraw the borders in case the new frame size
   * doesn't cover the same area */
#if ENABLE_RT_DISPLAY
    if(xvimagesink->rt_display_vaule == 1){
        if(!xvimagesink->rt_display_avoc_done){ // To control reset caps not change to display channel again
		xvimagesink->redraw_border = TRUE;
	}
    }else {
	xvimagesink->redraw_border = TRUE;
    }
#else
	xvimagesink->redraw_border = TRUE;
#endif
  

  /* We renew our xvimage only if size or format changed;
   * the xvimage is the same size as the video pixel size */
  if ((xvimagesink->xvimage) &&
      ((im_format != xvimagesink->xvimage->im_format) ||
          (video_width != xvimagesink->xvimage->width) ||
          (video_height != xvimagesink->xvimage->height))) {
    GST_DEBUG_OBJECT (xvimagesink,
        "old format %" GST_FOURCC_FORMAT ", new format %" GST_FOURCC_FORMAT,
        GST_FOURCC_ARGS (xvimagesink->xvimage->im_format),
        GST_FOURCC_ARGS (im_format));
#if 0	//ENABLE_RT_DISPLAY
   if(xvimagesink->rt_display_vaule == 1){
	GST_DEBUG_OBJECT (xvimagesink, "RT: rt display mode, resolution changed not renew xvimage again !!! ");
   }else 
#endif
   {
   	GST_DEBUG_OBJECT (xvimagesink, "renewing xvimage");
    	gst_buffer_unref (GST_BUFFER (xvimagesink->xvimage));
    	xvimagesink->xvimage = NULL;
   }
  }

#if ENABLE_RT_DISPLAY
    if(xvimagesink->rt_display_avoc_done == 0){
#endif	//ENABLE_RT_DISPLAY	
	  if (xvimagesink->get_pixmap_cb == NULL) // No need to avoc setting for pixmap case.
	  {
	    /* Notification of changed resolution, if it is new */
	    gint prevVideo_width = 0, prevVideo_height = 0;
	    ret = gst_structure_get_int (oldStructure, "width", &prevVideo_width);
	    ret &= gst_structure_get_int (oldStructure, "height", &prevVideo_height);
	    if ((prevVideo_width != video_width) || (prevVideo_height != video_height))
	    {
	      /* 1. Beginning of playback : Sync, No seamless, Set 2 times(Pre/Post). Post will be called after first putimage */
	      if (xvimagesink->is_first_putimage == TRUE)
	      {
	        gst_xvimagesink_avoc_set_resolution(xvimagesink, caps, FALSE/* SYNC */, FALSE/*Non-Seamless*/, TRUE/*PRE SET*/);
	      }
	      else /* 2. During the playback : Async, Seamless, Set Once */
	      {
	        if (xvimagesink->seamless_resolution_change == FALSE)
		  	mute_video_display(xvimagesink, TRUE, MUTE_RESOLUTION_CHANGE);
	        gst_xvimagesink_avoc_set_resolution(xvimagesink, caps, TRUE/* ASYNC */, xvimagesink->seamless_resolution_change/*Seamless*/, FALSE/*ONCE*/);
	      }
	      
	      GST_DEBUG_OBJECT( xvimagesink, "signal for resolution-changed information, ret[ %d ]", ret);
	    }

	    /* Notification of updated ColorSpace info, if it is new */
	    gst_xvimagesink_update_avoc_color_space_info(xvimagesink, structure, xvimagesink->is_first_putimage/*use sync only before first putimage*/);

	    GstStructure *query_structure = gst_structure_new("request_aspect_ratio", NULL);
	    GstQuery *query = gst_query_new_application (GST_QUERY_CUSTOM, query_structure);
	    gboolean ret_query = gst_pad_peer_query(bsink->sinkpad, query);
	    ret = gst_structure_get_int (query_structure, "sample_aspect_ratio_num", &xvimagesink->par_x);
	    if(!ret)
	    	GST_ERROR_OBJECT(xvimagesink, "Fail to set par_x");
	    ret = gst_structure_get_int (query_structure, "sample_aspect_ratio_den", &xvimagesink->par_y);
	    if(!ret)
	    	GST_ERROR_OBJECT(xvimagesink, "Fail to set par_y");
	    if (xvimagesink->par_x == 0)
			{
				xvimagesink->par_x = 1;
			}
			if (xvimagesink->par_y == 0)
			{
				xvimagesink->par_y = 1;
			}
	    GST_ERROR_OBJECT(xvimagesink, "par_x [%d] par_y [%d]",xvimagesink->par_x,xvimagesink->par_y);
	    if (query)
	      gst_query_unref(query);
	  }
#if ENABLE_RT_DISPLAY
	  if(xvimagesink->rt_display_vaule == 1){// need to make sure this play mode is RT case, to avoid other resolution changed scenarios.
	  	xvimagesink->rt_display_avoc_done = 1;
	  }
  }
#endif	//ENABLE_RT_DISPLAY
  g_mutex_unlock (xvimagesink->flow_lock);

  return TRUE;

  /* ERRORS */
incompatible_caps:
  {
    GST_ERROR_OBJECT (xvimagesink, "caps incompatible");
    return FALSE;
  }
incomplete_caps:
  {
    GST_DEBUG_OBJECT (xvimagesink, "Failed to retrieve either width, "
        "height or framerate from intersected caps");
    return FALSE;
  }
invalid_format:
  {
    GST_DEBUG_OBJECT (xvimagesink,
        "Could not locate image format from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
no_disp_ratio:
  {
    GST_ELEMENT_ERROR (xvimagesink, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output display ratio of the video."));
    return FALSE;
  }
no_display_size:
  {
    GST_ELEMENT_ERROR (xvimagesink, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output display ratio of the video."));
    return FALSE;
  }
}

static GstStateChangeReturn
gst_xvimagesink_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstXvImageSink *xvimagesink;
  GstXContext *xcontext = NULL;
#ifdef GST_EXT_XV_ENHANCEMENT
  Atom atom_preemption = None;
#endif /* GST_EXT_XV_ENHANCEMENT */

  xvimagesink = GST_XVIMAGESINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
	GST_DEBUG_OBJECT( xvimagesink, " State  NULL -> READY" );
#ifdef XV_WEBKIT_PIXMAP_SUPPORT
	GST_DEBUG_OBJECT (xvimagesink, "init xv color conversion!");
	xv_colorconversion_init();
#endif // XV_WEBKIT_PIXMAP_SUPPORT
      /* Initializing the XContext */
      if (xvimagesink->xcontext == NULL) {
        xcontext = gst_xvimagesink_xcontext_get (xvimagesink);
        if (xcontext == NULL)
          return GST_STATE_CHANGE_FAILURE;
        GST_OBJECT_LOCK (xvimagesink);
        if (xcontext)
          xvimagesink->xcontext = xcontext;
        GST_OBJECT_UNLOCK (xvimagesink);
      }

      int i;
     	gint N_attr = 0;
			XvAttribute *xv_attr;
  		xv_attr = XvQueryPortAttributes (xvimagesink->xcontext->disp,
      xvimagesink->xcontext->xv_port_id, &N_attr);
  
    	for (i = 0 ; i < N_attr ; i++) {
      if (!strcmp(xv_attr[i].name, "_USER_WM_PORT_ATTRIBUTE_SCALER")) {
        Atom atom_output = XInternAtom(xvimagesink->xcontext->disp, "_USER_WM_PORT_ATTRIBUTE_SCALER", False);
        gint ret = XvSetPortAttribute(xvimagesink->xcontext->disp, xvimagesink->xcontext->xv_port_id, atom_output, xvimagesink->scaler_id);
        if (ret != Success)
          GST_ERROR_OBJECT(xvimagesink, "Fail to set scaler id[ %d ], ret[ %d ]", xvimagesink->scaler_id, ret);
        GST_DEBUG_OBJECT(xvimagesink, "[%d]_USER_WM_PORT_ATTRIBUTE_SCALER found, and set scaler id to [ %d ]", i, xvimagesink->scaler_id);
        break;
      }
    }
      /* update object's par with calculated one if not set yet */
      if (!xvimagesink->par) {
        xvimagesink->par = g_new0 (GValue, 1);
        gst_value_init_and_copy (xvimagesink->par, xvimagesink->xcontext->par);
        GST_DEBUG_OBJECT (xvimagesink, "set calculated PAR on object's PAR");
      }
      /* call XSynchronize with the current value of synchronous */
      GST_DEBUG_OBJECT (xvimagesink, "XSynchronize called with %s",
          xvimagesink->synchronous ? "TRUE" : "FALSE");
      XSynchronize (xvimagesink->xcontext->disp, xvimagesink->synchronous);
      gst_xvimagesink_update_colorbalance (xvimagesink);
      gst_xvimagesink_manage_event_thread (xvimagesink);
#if 0  //disable Temporarily
	  #ifndef USE_TBM_SDK
      gst_xvimagesink_avoc_set_attribute  (xvimagesink); //For HDR setting
      #endif
#endif
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
	GST_DEBUG_OBJECT( xvimagesink, " State  READY -> PAUSE" );
      set_flip_mode(xvimagesink, xvimagesink->flip);
      g_mutex_lock (xvimagesink->pool_lock);
      xvimagesink->pool_invalid = FALSE;
      g_mutex_unlock (xvimagesink->pool_lock);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
	GST_DEBUG_OBJECT( xvimagesink, "  State  PAUSE -> PLAYING" );
#ifdef GST_EXT_XV_ENHANCEMENT
      g_mutex_lock (xvimagesink->x_lock);
//      set_input_mode(xvimagesink->xcontext, 1); /*DRM_SDP_DP_INPORT_MM = 1*/
	gst_xvimagesink_set_xv_port_attribute(xvimagesink, "_USER_WM_PORT_ATTRIBUTE_PREEMPTION", 1);
      g_mutex_unlock (xvimagesink->x_lock);
#endif /* GST_EXT_XV_ENHANCEMENT */
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
	GST_DEBUG_OBJECT( xvimagesink, "  State  PAUSED -> PAUSE" );
      g_mutex_lock (xvimagesink->pool_lock);
      xvimagesink->pool_invalid = TRUE;
      g_mutex_unlock (xvimagesink->pool_lock);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
	GST_DEBUG_OBJECT( xvimagesink, "  State PLAYING -> PAUSE" );
#ifdef GST_EXT_XV_ENHANCEMENT
      g_mutex_lock (xvimagesink->x_lock);
      gst_xvimagesink_set_xv_port_attribute(xvimagesink, "_USER_WM_PORT_ATTRIBUTE_PREEMPTION", 0);
      g_mutex_unlock (xvimagesink->x_lock);
#endif /* GST_EXT_XV_ENHANCEMENT */
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
	GST_DEBUG_OBJECT( xvimagesink, " State PAUSE -> READY" );
      xvimagesink->fps_n = 0;
      xvimagesink->fps_d = 1;
      GST_VIDEO_SINK_WIDTH (xvimagesink) = 0;
      GST_VIDEO_SINK_HEIGHT (xvimagesink) = 0;
#ifdef GST_EXT_XV_ENHANCEMENT
      /* close drm */
      drm_fini(xvimagesink);
#endif /* GST_EXT_XV_ENHANCEMENT */
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
	GST_DEBUG_OBJECT( xvimagesink, " State READY -> NULL" );
      gst_xvimagesink_reset (xvimagesink);
      gst_xvimagesink_free_outside_buf(xvimagesink);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_xvimagesink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstXvImageSink *xvimagesink;

  xvimagesink = GST_XVIMAGESINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf)) {
      *end = *start + GST_BUFFER_DURATION (buf);
    } else {
      if (xvimagesink->fps_n > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND, xvimagesink->fps_d,
            xvimagesink->fps_n);
      }
    }
  }
}

#ifndef uint32_t
typedef unsigned int  uint32_t;
#endif

typedef struct
{
    unsigned int _header; /* for internal use only */
    unsigned int _version; /* for internal use only */

    unsigned int YBuf;
    unsigned int CbBuf;
    unsigned int CrBuf;

    unsigned int BufType;
} XV_DATA, * XV_DATA_PTR;

static void invalidate_hw_decoded_frame_cache(GstPad* pad, gboolean invalidate)
{
  gboolean ret = FALSE;
  GstStructure* structure = gst_structure_new("cache_control", "onoff", G_TYPE_BOOLEAN, invalidate, NULL);
  GstEvent* event = gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, structure);
  ret = gst_pad_push_event(pad, event);
  GST_ERROR_OBJECT(pad, "[ %s ] to invalidate[ %d ]", ret?"Success":"Fail to", invalidate);
  return;
}

inline void  update_sei_metadata(GstXvImageSink * xvimagesink, GstBuffer * buf)
{
	g_return_if_fail(xvimagesink);
	g_return_if_fail(buf);

	GstStructure * seimetadata_size_structure = gst_buffer_get_qdata(buf, g_quark_from_string("seimetadata-size"));
	GstStructure * seimetadata_structure = gst_buffer_get_qdata(buf, g_quark_from_string("seimetadata"));

	if (seimetadata_size_structure && seimetadata_structure)
	{
		guint size = 0;
		guchar* seidata = NULL;

		gst_structure_get_int(seimetadata_size_structure, "seimetadata-size", &size);
		gst_structure_get(seimetadata_structure, "seimetadata", G_TYPE_POINTER, &seidata, NULL);
		
		if(size > 0 && seidata != NULL)
		{
			if (xvimagesink->sei_metadata_size == 0)
			{
				// check allocated size;
				if (xvimagesink->sei_metadata_alloc_size < size)
				{
					if (xvimagesink->sei_metadata)
					{
						g_free(xvimagesink->sei_metadata);
						xvimagesink->sei_metadata = NULL;
						xvimagesink->sei_metadata_alloc_size = 0;
					}
				}

				if (xvimagesink->sei_metadata == NULL)
				{
					xvimagesink->sei_metadata = g_new0 (guchar, size);
					if (xvimagesink->sei_metadata == NULL)
					{
						GST_ERROR_OBJECT(xvimagesink, "can not allocate sei_metadata[ %d ]bytes", size);
						return;
					}
					xvimagesink->sei_metadata_alloc_size = size;
				}

				memcpy(xvimagesink->sei_metadata, seidata, size);
				xvimagesink->sei_metadata_size = size;
				GST_INFO_OBJECT(xvimagesink, "size[ %d ], data[ 0x%02x ~ 0x%02x ]", size, xvimagesink->sei_metadata[0], xvimagesink->sei_metadata[size-1]);
#if 0
				GST_ERROR_OBJECT(xvimagesink, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", xvimagesink->sei_metadata[0], xvimagesink->sei_metadata[1]
					, xvimagesink->sei_metadata[2], xvimagesink->sei_metadata[3], xvimagesink->sei_metadata[4], xvimagesink->sei_metadata[5]
					, xvimagesink->sei_metadata[6], xvimagesink->sei_metadata[7], xvimagesink->sei_metadata[8], xvimagesink->sei_metadata[9]);
				GST_ERROR_OBJECT(xvimagesink, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", xvimagesink->sei_metadata[size-11+0], xvimagesink->sei_metadata[size-11+1]
					, xvimagesink->sei_metadata[size-11+2], xvimagesink->sei_metadata[size-11+3], xvimagesink->sei_metadata[size-11+4], xvimagesink->sei_metadata[size-11+5]
					, xvimagesink->sei_metadata[size-11+6], xvimagesink->sei_metadata[size-11+7], xvimagesink->sei_metadata[size-11+8], xvimagesink->sei_metadata[size-11+9]);
#endif
			}
			else
			{
				GST_ERROR_OBJECT(xvimagesink, "prev_sei[ %d ]is not processed, new_sei[ %d ]", xvimagesink->sei_metadata_size, size);
			}
		}
	}

	return;
}

inline void  update_sei_mdcv_data(GstXvImageSink * xvimagesink, GstBuffer * buf)
{
	g_return_if_fail(xvimagesink);
	g_return_if_fail(buf);

	GstStructure * sei_mdcv_structure = gst_buffer_get_qdata(buf, g_quark_from_string("sei-mdcv"));
	if (sei_mdcv_structure)
	{
		if (xvimagesink->sei_mdcv_filled)
		{
			GST_WARNING_OBJECT(xvimagesink, "Previous sei_mdcv is not updated");
		}

		if (xvimagesink->sei_mdcv == NULL)
		{
			xvimagesink->sei_mdcv = g_new0 (guchar, sizeof(VIDEO_MASTERING_DISPLAY_COLOUR_VOLUME));
			if (xvimagesink->sei_mdcv == NULL)
			{
				GST_ERROR_OBJECT(xvimagesink, "can not allocate sei_mdcv[ %d ]bytes", sizeof(VIDEO_MASTERING_DISPLAY_COLOUR_VOLUME));
				return;
			}
		}

		VIDEO_MASTERING_DISPLAY_COLOUR_VOLUME* mdcv_structure = (VIDEO_MASTERING_DISPLAY_COLOUR_VOLUME*)xvimagesink->sei_mdcv;
		guint value = 0;
		gst_structure_get(sei_mdcv_structure, "display_primaries_x0", G_TYPE_UINT, &value, NULL);
		mdcv_structure->display_primaries_x[0] = value;
		gst_structure_get(sei_mdcv_structure, "display_primaries_x1", G_TYPE_UINT, &value, NULL);
		mdcv_structure->display_primaries_x[1] = value;
		gst_structure_get(sei_mdcv_structure, "display_primaries_x2", G_TYPE_UINT, &value, NULL);
		mdcv_structure->display_primaries_x[2] = value;

		gst_structure_get(sei_mdcv_structure, "display_primaries_y0", G_TYPE_UINT, &value, NULL);
		mdcv_structure->display_primaries_y[0] = value;
		gst_structure_get(sei_mdcv_structure, "display_primaries_y1", G_TYPE_UINT, &value, NULL);
		mdcv_structure->display_primaries_y[1] = value;
		gst_structure_get(sei_mdcv_structure, "display_primaries_y2", G_TYPE_UINT, &value, NULL);
		mdcv_structure->display_primaries_y[2] = value;

		gst_structure_get(sei_mdcv_structure, "white_point_x", G_TYPE_UINT, &value, NULL);
		mdcv_structure->white_point_x = value;
		gst_structure_get(sei_mdcv_structure, "white_point_y", G_TYPE_UINT, &value, NULL);
		mdcv_structure->white_point_y = value;
		gst_structure_get(sei_mdcv_structure, "max_display_mastering_luminance", G_TYPE_UINT, &value, NULL);
		mdcv_structure->max_display_mastering_luminance = value;
		gst_structure_get(sei_mdcv_structure, "min_display_mastering_luminance", G_TYPE_UINT, &value, NULL);
		mdcv_structure->min_display_mastering_luminance = value;
		GST_DEBUG_OBJECT(xvimagesink, "primaries_x[ %d %d %d ], y[ %d %d %d ], white_point[ %d, %d ], lum[ %d, %d ]", 
				mdcv_structure->display_primaries_x[0], mdcv_structure->display_primaries_x[1], mdcv_structure->display_primaries_x[2],
				mdcv_structure->display_primaries_y[0], mdcv_structure->display_primaries_y[1], mdcv_structure->display_primaries_y[2],
				mdcv_structure->white_point_x, mdcv_structure->white_point_y, mdcv_structure->max_display_mastering_luminance,mdcv_structure->min_display_mastering_luminance);

		xvimagesink->sei_mdcv_filled = TRUE;
	}

	return;
}

// HW Rotate Modules Start
static gboolean gst_hw_rotate_apply_mute(GstXvImageSink * xvimagesink)
{
	mute_video_display(xvimagesink, TRUE, MUTE_ROTATION_CHANGE);  
	xvimagesink->is_unmute_req_for_hw_rotate = TRUE;		
	GST_DEBUG_OBJECT(xvimagesink, "MUTE DONE FOR HW ROTATION PrevDegree[%d] CurrentDegree[%d] RotateChangedStatus[%d]",xvimagesink->prev_hw_rotate_degree,xvimagesink->curr_hw_rotate_degree,xvimagesink->is_hw_rotate_degree_changed);
	return TRUE;
}

static gboolean gst_hw_rotate_apply_unmute(GstXvImageSink * xvimagesink)
{
	mute_video_display(xvimagesink, FALSE, MUTE_ROTATION_CHANGE);
	xvimagesink->is_unmute_req_for_hw_rotate = FALSE;
	xvimagesink->is_hw_rotate_degree_changed = FALSE;
	xvimagesink->prev_hw_rotate_degree = xvimagesink->curr_hw_rotate_degree;
	GST_DEBUG_OBJECT(xvimagesink, "UNMUTE DONE FOR HW ROTATION");
	return TRUE;
}

static gboolean gst_reset_hw_rotate_context(GstXvImageSink * xvimagesink)
{
    xvimagesink->set_hw_rotate_degree = 0;
	xvimagesink->get_hw_rotate_degree = 0;
	xvimagesink->can_support_hw_rotate = FALSE;
	xvimagesink->enable_hw_rotate_support = FALSE;
	xvimagesink->hw_rotate_scaled_width = 0;
	xvimagesink->hw_rotate_scaled_height = 0;
	xvimagesink->hw_rotate_degree = DEGREE_0;
	xvimagesink->curr_hw_rotate_degree = DEGREE_0;
	xvimagesink->prev_hw_rotate_degree = DEGREE_0;
	xvimagesink->is_hw_rotate_degree_changed = FALSE;
	xvimagesink->is_unmute_req_for_hw_rotate = FALSE;
	xvimagesink->is_hw_rotate_on_mixed_frame = FALSE;
	GST_DEBUG_OBJECT(xvimagesink, "HW Rotation Context Reset Is Completed");
	return TRUE;
}

static gboolean gst_can_support_hw_video_rotate(GstXvImageSink * xvimagesink)
{
	gboolean can_support = FALSE; 
	GST_DEBUG_OBJECT(xvimagesink, "Starts");
	
	//Call Driver Api for Set HW Video Rotation with rotate_degree	
	can_support = TRUE; // Set TRUE until driver API is ready
	xvimagesink->can_support_hw_rotate = can_support;

	GST_DEBUG_OBJECT(xvimagesink, "Ends Can_Support_Hw_Rotation[%d]",xvimagesink->can_support_hw_rotate);
	return TRUE;
}

static gboolean gst_calculate_target_dimention_hw_rotate(GstXvImageSink * xvimagesink)
{
	if (xvimagesink->enable_hw_rotate_support)
	{
		#define MAXSIZE(a, b) ((a)>(b)?(a):(b))
		#define MINSIZE(a, b) ((a)<(b)?(a):(b))	
		
		gint X = 0,Y = 0;
		gint scaled_width = 0, scaled_height = 0;
		gint hw_rotate_degree = xvimagesink->set_hw_rotate_degree;
		gint original_width = xvimagesink->video_width;
		gint original_height = xvimagesink->video_height;		

		GST_ERROR_OBJECT(xvimagesink, "hw_rotate_degree (%d) original_width (%d) original_height (%d)", hw_rotate_degree, original_width, original_height);

		switch(hw_rotate_degree)
		{
			case DEGREE_0:
			case DEGREE_180:
			default: 
				scaled_width = original_width;
				scaled_width -= (original_width&0x1);
				scaled_height = original_height;		
				scaled_height -= (original_height&0x1); 		
				break;
			case DEGREE_90:
			case DEGREE_270:
				X = MAXSIZE(original_width, original_height);
				Y = MINSIZE(original_width, original_height);		
				
				if (X != 0)
				{
					scaled_width = original_height * Y / X;
					scaled_height = original_width * Y / X; 		  
					scaled_width -= (scaled_width&0x1);
					scaled_height -= (scaled_height&0x1);
					GST_ERROR_OBJECT(xvimagesink, "scaled_width (%d) scaled_height (%d)", scaled_width, scaled_height);
				}

				break;
		}

		xvimagesink->hw_rotate_scaled_width = scaled_width;
		xvimagesink->hw_rotate_scaled_height = scaled_height;
	}
	else
	{
		GST_DEBUG_OBJECT(xvimagesink, "HW ROTATION IS NOT ENABLED[%d]",xvimagesink->enable_hw_rotate_support);
	}

	return TRUE;
}

static gboolean gst_set_hw_video_rotate_degree(GstXvImageSink * xvimagesink)
{
	gboolean ret = FALSE;
	if (xvimagesink->enable_hw_rotate_support)
	{		
		GST_DEBUG_OBJECT(xvimagesink, "Starts Set_Hw_Rotation_Degree[%d]",xvimagesink->set_hw_rotate_degree);	

		gst_calculate_target_dimention_hw_rotate(xvimagesink);
		GST_DEBUG_OBJECT(xvimagesink, "Degree[%d] OrginalDimention[W,H][%d, %d] ScaledDimention[W,H][%d, %d]",xvimagesink->set_hw_rotate_degree,xvimagesink->video_width,xvimagesink->video_height,xvimagesink->hw_rotate_scaled_width,xvimagesink->hw_rotate_scaled_height);	
		xvimagesink->curr_hw_rotate_degree = xvimagesink->set_hw_rotate_degree;

		if (xvimagesink->curr_hw_rotate_degree != xvimagesink->prev_hw_rotate_degree)
		{
			xvimagesink->is_hw_rotate_degree_changed = TRUE;
			GST_DEBUG_OBJECT(xvimagesink, "PrevDegree[%d] CurrentDegree[%d] RotateChangedStatus[%d]",xvimagesink->prev_hw_rotate_degree,xvimagesink->curr_hw_rotate_degree,xvimagesink->is_hw_rotate_degree_changed);
		}
		else
		{
			xvimagesink->is_hw_rotate_degree_changed = FALSE;
		}

#if 0
		switch(xvimagesink->curr_hw_rotate_degree)
		{
			case 0:
				xvimagesink->hw_rotate_degree = DEGREE_0;
			break;
			case 90:
				xvimagesink->hw_rotate_degree = DEGREE_90;
				break;
			case 180:
				xvimagesink->hw_rotate_degree = DEGREE_180;
				break;
			case 270:
				xvimagesink->hw_rotate_degree = DEGREE_270;
				break;
			default:
				xvimagesink->hw_rotate_degree = DEGREE_0;
			break;
		}
#else
		xvimagesink->hw_rotate_degree = xvimagesink->curr_hw_rotate_degree;
#endif
		GST_DEBUG_OBJECT(xvimagesink, "Ends Set_Hw_Rotation_Degree[%d] ret[%d]",xvimagesink->set_hw_rotate_degree,ret);
	}
	else
	{
		GST_DEBUG_OBJECT(xvimagesink, "HW ROTATION IS NOT ENABLED[%d]",xvimagesink->enable_hw_rotate_support);
	}
	return ret;
}

static gboolean gst_set_hw_video_rotate_degree_on_mixedframe(GstXvImageSink * xvimagesink)
{
	gboolean ret = FALSE;
	if (xvimagesink->is_hw_rotate_on_mixed_frame)
	{
		GST_DEBUG_OBJECT(xvimagesink, "Starts Set_Hw_Rotation_Degree_On_MixedFrame[%d]",xvimagesink->set_hw_rotate_degree);	

		// Calculate Target Dimention is not required for mixed frame
		
		GST_DEBUG_OBJECT(xvimagesink, "Degree[%d] OrginalDimention[W,H][%d, %d] ScaledDimention[W,H][%d, %d]",xvimagesink->set_hw_rotate_degree,xvimagesink->video_width,xvimagesink->video_height,xvimagesink->hw_rotate_scaled_width,xvimagesink->hw_rotate_scaled_height);	
		xvimagesink->curr_hw_rotate_degree = xvimagesink->set_hw_rotate_degree;

		if (xvimagesink->curr_hw_rotate_degree != xvimagesink->prev_hw_rotate_degree)
		{
			xvimagesink->is_hw_rotate_degree_changed = TRUE;
			GST_DEBUG_OBJECT(xvimagesink, "PrevDegree[%d] CurrentDegree[%d] RotateChangedStatus[%d]",xvimagesink->prev_hw_rotate_degree,xvimagesink->curr_hw_rotate_degree,xvimagesink->is_hw_rotate_degree_changed);
		}
		else
		{
			xvimagesink->is_hw_rotate_degree_changed = FALSE;
		}

		switch(xvimagesink->curr_hw_rotate_degree)
		{
			case 0:
				xvimagesink->hw_rotate_degree = DEGREE_0;
			break;
			case 90:
				xvimagesink->hw_rotate_degree = DEGREE_90;
				break;
			case 180:
				xvimagesink->hw_rotate_degree = DEGREE_180;
				break;
			case 270:
				xvimagesink->hw_rotate_degree = DEGREE_270;
				break;
			default:
				xvimagesink->hw_rotate_degree = DEGREE_0;
			break;
		}
		
		GST_DEBUG_OBJECT(xvimagesink, "Ends Set_Hw_Rotation_Degree_On_MixedFrame[%d] ret[%d]",xvimagesink->set_hw_rotate_degree,ret);		
	}
	else
	{
		GST_DEBUG_OBJECT(xvimagesink, "HW ROTATION ON MIXED FRAME IS NOT ENABLED[%d]",xvimagesink->is_hw_rotate_on_mixed_frame);
	}

	return ret;
}


static gboolean gst_xvimagesink_get_xv_port_hw_rotate_attribute(GstXvImageSink * xvimagesink, gint *degree)
{
	gint val = 0;
	gint i = 0, count = 0;
	Atom atom_dp = None;
	if(xvimagesink->xcontext){
		GstXContext* xcontext = xvimagesink->xcontext;
		XvAttribute *const attr = XvQueryPortAttributes(xcontext->disp, xcontext->xv_port_id, &count);
		if (attr) {
		  for (i = 0 ; i < count ; i++) {
			if (!strcmp(attr[i].name, "_USER_WM_PORT_ATTRIBUTE_ROTATION")) {
				atom_dp= XInternAtom(xcontext->disp, "_USER_WM_PORT_ATTRIBUTE_ROTATION", FALSE);
				XvGetPortAttribute (xcontext->disp, xcontext->xv_port_id, atom_dp, &val);
				*degree = val;
				break;
			}
		  }
		  XFree(attr);
		} else {
		  GST_WARNING_OBJECT(xvimagesink, "DP: Failed XvQueryPortAttributes -> disp:%d, port_id:%d ", xcontext->disp, xcontext->xv_port_id);
		}
	}
	
	return TRUE;
}

static gboolean gst_get_hw_video_rotate_degree(GstXvImageSink * xvimagesink)
{
	gboolean ret = FALSE;
	if (xvimagesink->enable_hw_rotate_support)
	{	
		gint degree = 0;
		GST_DEBUG_OBJECT(xvimagesink, "Starts");
		
		//Call Driver Api to Get HW Video Rotation with rotate_degree
		ret = gst_xvimagesink_get_xv_port_hw_rotate_attribute(xvimagesink, &degree);
		
		xvimagesink->get_hw_rotate_degree = degree;
		
		GST_DEBUG_OBJECT(xvimagesink, "Ends Get_Hw_Rotation_Degree[%d] ret[%d]",xvimagesink->get_hw_rotate_degree,ret);
	}
	else
	{
		GST_DEBUG_OBJECT(xvimagesink, "HW ROTATION IS NOT ENABLED[%d]",xvimagesink->enable_hw_rotate_support);
	}
	return ret;
}
// HW Rotate Modules End
static GstFlowReturn
gst_xvimagesink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstXvImageSink *xvimagesink;
  gint need_unmute_after_rotation_putimage = 0;
  int debug_info = 0;
#if ENABLE_PERFORMANCE_CHECKING
  show_frame_time_tmp = get_time();
#endif

#ifdef GST_EXT_XV_ENHANCEMENT
  XV_PUTIMAGE_DATA_PTR img_data = NULL;
  SCMN_IMGB *scmn_imgb = NULL;
  gint format = 0;
  gboolean ret = FALSE;
#endif /* GST_EXT_XV_ENHANCEMENT */

  xvimagesink = GST_XVIMAGESINK (vsink);

#ifdef GST_EXT_XV_ENHANCEMENT
  if (xvimagesink->stop_video) {
    GST_INFO( "Stop video is TRUE. so skip show frame..." );
    return GST_FLOW_OK;
  }
#endif /* GST_EXT_XV_ENHANCEMENT */

#if 1
// cpoy qdata
  GstStructure* structure_bitrate = gst_buffer_get_qdata(buf, g_quark_from_string("realtime_bitrate"));
	if (structure_bitrate)
	{
		gst_structure_get_int(structure_bitrate, "realtime_bitrate", &xvimagesink->new_bitrate);
	}
#endif
	
	GstStructure* structure_scantype = gst_buffer_get_qdata(buf, g_quark_from_string("scantype"));
	if (structure_scantype)
	{
		gint scantype = 0;
		if (gst_structure_get_int(structure_scantype, "scantype", &scantype))
		{
			  xvimagesink->cur_scantype =  scantype;
			  GST_WARNING_OBJECT(xvimagesink,"scantype changed = %d", scantype);
		}
	}
	update_sei_metadata(xvimagesink, buf);
	update_sei_mdcv_data(xvimagesink, buf);

#ifdef XV_WEBKIT_PIXMAP_SUPPORT
  if (xvimagesink->get_pixmap_cb != xvimagesink->old_get_pixmap_cb) {
  	/* Clear the xvimage and the image pool */
  	GST_DEBUG_OBJECT (xvimagesink, "pixmap callback changed, we need to release all xvimages! old callback: %p, new callback:%p",
  		xvimagesink->old_get_pixmap_cb, xvimagesink->get_pixmap_cb);
    g_mutex_lock (xvimagesink->flow_lock);
  	gst_xvimagesink_imagepool_clear (xvimagesink);
  	if (xvimagesink->xvimage) {
  		gst_xvimage_buffer_free (xvimagesink->xvimage);
  		xvimagesink->xvimage = NULL;
  	}
    g_mutex_unlock (xvimagesink->flow_lock);
  	xvimagesink->old_get_pixmap_cb = xvimagesink->get_pixmap_cb;
  }	
#if ENABLE_PERFORMANCE_CHECKING
  if (xvimagesink->get_pixmap_cb) {
    pre_time = 0;
    GST_ERROR_OBJECT (xvimagesink, "the time enter 'show_frame', %llu microseconds", get_time());
    GST_ERROR_OBJECT (xvimagesink, "the time enter 'show_frame', cosumed %llu microseconds", get_time_diff());
  }
#endif
#endif
  /* If this buffer has been allocated using our buffer management we simply
     put the ximage which is in the PRIVATE pointer */
  if (GST_IS_XVIMAGE_BUFFER (buf)) {
    GST_LOG_OBJECT (xvimagesink, "fast put of bufferpool buffer %p", buf);
#ifdef GST_EXT_XV_ENHANCEMENT
    xvimagesink->xid_updated = FALSE;
#endif /* GST_EXT_XV_ENHANCEMENT */

    if (!gst_xvimagesink_xvimage_put (xvimagesink,
            GST_XVIMAGE_BUFFER_CAST (buf), GST_BUFFER_CAPS (buf), 0))
      goto no_window;
  } else {

    GST_CAT_LOG_OBJECT (GST_CAT_PERFORMANCE, xvimagesink,
        "slow copy into bufferpool buffer %p", buf);
    /* Else we have to copy the data into our private image, */
    /* if we have one... */
#ifdef GST_EXT_XV_ENHANCEMENT
    g_mutex_lock (xvimagesink->flow_lock);
#endif /* GST_EXT_XV_ENHANCEMENT */

    if (!xvimagesink->xvimage) {
      GST_DEBUG_OBJECT (xvimagesink, "creating our xvimage");

#ifdef GST_EXT_XV_ENHANCEMENT
      format = gst_xvimagesink_get_format_from_caps(xvimagesink, GST_BUFFER_CAPS(buf));
      switch (format) {
        case GST_MAKE_FOURCC('S', 'T', '1', '2'):
        case GST_MAKE_FOURCC('S', 'N', '1', '2'):
        case GST_MAKE_FOURCC('S', 'N', '2', '1'):
        case GST_MAKE_FOURCC('S', '4', '2', '0'):
        case GST_MAKE_FOURCC('S', 'U', 'Y', '2'):
        case GST_MAKE_FOURCC('S', 'U', 'Y', 'V'):
        case GST_MAKE_FOURCC('S', 'Y', 'V', 'Y'):
          scmn_imgb = (SCMN_IMGB *)GST_BUFFER_MALLOCDATA(buf);
          if(scmn_imgb == NULL) {
            GST_DEBUG_OBJECT( xvimagesink, "scmn_imgb is NULL. Skip xvimage put..." );
            g_mutex_unlock (xvimagesink->flow_lock);


            return GST_FLOW_OK;
          }

          /* skip buffer if aligned size is smaller than size of caps */
          if (scmn_imgb->s[0] < xvimagesink->video_width ||
              scmn_imgb->e[0] < xvimagesink->video_height) {
            GST_WARNING_OBJECT(xvimagesink, "invalid size[caps:%dx%d,aligned:%dx%d]. Skip this buffer...",
                                            xvimagesink->video_width, xvimagesink->video_height,
                                            scmn_imgb->s[0], scmn_imgb->e[0]);
            g_mutex_unlock (xvimagesink->flow_lock);


            return GST_FLOW_OK;
          }

          xvimagesink->aligned_width = scmn_imgb->s[0];
          xvimagesink->aligned_height = scmn_imgb->e[0];
          GST_INFO_OBJECT(xvimagesink, "Use aligned width,height[%dx%d]",
                                       xvimagesink->aligned_width, xvimagesink->aligned_height);
          break;
        default:
          GST_INFO_OBJECT(xvimagesink, "Use original width,height of caps");
          break;
      }
#endif /* GST_EXT_XV_ENHANCEMENT */
#ifdef XV_WEBKIT_PIXMAP_SUPPORT
      if (xvimagesink->get_pixmap_cb) {
      	gst_xvimagesink_set_pixmap_handle ((GstXOverlay *)xvimagesink, xvimagesink->get_pixmap_cb(xvimagesink->get_pixmap_cb_user_data));
      	if (xvimagesink->current_pixmap_idx < 0) {
      		GST_DEBUG_OBJECT (xvimagesink, "get pixmap ID failed!");
      		return GST_FLOW_ERROR;
      	}
      }
#endif
      xvimagesink->xvimage = gst_xvimagesink_xvimage_new (xvimagesink,
          GST_BUFFER_CAPS (buf));
	  //sometimes xvimagesink->src_input and xvimagesink->dest_Rect is 0. so (0,0,0,0) size was set to putimage.
	  //need to re arrange gst_getRectSize call position later. 
	  gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
      if (!xvimagesink->xvimage)
        /* The create method should have posted an informative error */
        goto no_image;

      if (format != GST_MAKE_FOURCC('S', 'T', 'V', '1')
#ifdef XV_WEBKIT_PIXMAP_SUPPORT
        && format != GST_MAKE_FOURCC('S', 'T', 'V', '0')
#endif
        ) {
        if (xvimagesink->xvimage->size < GST_BUFFER_SIZE (buf)) {
          GST_ELEMENT_ERROR (xvimagesink, RESOURCE, WRITE,
              ("Failed to create output image buffer of %dx%d pixels",
                  xvimagesink->xvimage->width, xvimagesink->xvimage->height),
              ("XServer allocated buffer size did not match input buffer"));

          gst_xvimage_buffer_destroy (xvimagesink->xvimage);
          xvimagesink->xvimage = NULL;

          goto no_image;
        }
      }
    }

#ifdef GST_EXT_XV_ENHANCEMENT
    switch (xvimagesink->xvimage->im_format) {
      /* Cases for specified formats of extension */
      case GST_MAKE_FOURCC('S', 'T', '1', '2'):
      case GST_MAKE_FOURCC('S', 'N', '1', '2'):
      case GST_MAKE_FOURCC('S', 'N', '2', '1'):
      case GST_MAKE_FOURCC('S', '4', '2', '0'):
      case GST_MAKE_FOURCC('S', 'U', 'Y', '2'):
      case GST_MAKE_FOURCC('S', 'U', 'Y', 'V'):
      case GST_MAKE_FOURCC('S', 'Y', 'V', 'Y'):
      case GST_MAKE_FOURCC('I', 'T', 'L', 'V'):
      {
        GST_LOG("EXT format - fourcc:%c%c%c%c, display mode:%d, Rotate angle:%d",
                xvimagesink->xvimage->im_format, xvimagesink->xvimage->im_format>>8,
                xvimagesink->xvimage->im_format>>16, xvimagesink->xvimage->im_format>>24,
                xvimagesink->display_mode, xvimagesink->rotate_angle);

        if (xvimagesink->xvimage->xvimage->data) {
          img_data = (XV_PUTIMAGE_DATA_PTR) xvimagesink->xvimage->xvimage->data;
          memset(img_data, 0x0, sizeof(XV_PUTIMAGE_DATA));
          XV_PUTIMAGE_INIT_DATA(img_data);

          scmn_imgb = (SCMN_IMGB *)GST_BUFFER_MALLOCDATA(buf);
          if (scmn_imgb == NULL) {
            GST_DEBUG_OBJECT( xvimagesink, "scmn_imgb is NULL. Skip xvimage put..." );
            g_mutex_unlock (xvimagesink->flow_lock);


            return GST_FLOW_OK;
          }

          if (scmn_imgb->buf_share_method == BUF_SHARE_METHOD_PADDR) {
            img_data->YBuf = (unsigned int)scmn_imgb->p[0];
            img_data->CbBuf = (unsigned int)scmn_imgb->p[1];
            img_data->CrBuf = (unsigned int)scmn_imgb->p[2];
            img_data->BufType = XV_BUF_TYPE_LEGACY;
          } else if (scmn_imgb->buf_share_method == BUF_SHARE_METHOD_FD) {
            /* open drm to use gem */
            if (xvimagesink->drm_fd < 0) {
              // drm_init(xvimagesink);
            }

            /* convert dma-buf fd into drm gem name */
//            img_data->YBuf = drm_convert_dmabuf_gemname(xvimagesink, (int)scmn_imgb->dmabuf_fd[0], &xvimagesink->gem_handle[0]);
//            img_data->CbBuf = drm_convert_dmabuf_gemname(xvimagesink, (int)scmn_imgb->dmabuf_fd[1], &xvimagesink->gem_handle[1]);
//            img_data->CrBuf = drm_convert_dmabuf_gemname(xvimagesink, (int)scmn_imgb->dmabuf_fd[2], &xvimagesink->gem_handle[2]);
            img_data->BufType = XV_BUF_TYPE_DMABUF;
          } else {
            GST_WARNING("unknown buf_share_method type [%d]. skip xvimage put...",
                        scmn_imgb->buf_share_method);
            g_mutex_unlock (xvimagesink->flow_lock);


            return GST_FLOW_OK;
          }

          GST_LOG_OBJECT(xvimagesink, "YBuf[%d], CbBuf[%d], CrBuf[%d]",
                                      img_data->YBuf, img_data->CbBuf, img_data->CrBuf );
        } else {
          GST_WARNING_OBJECT( xvimagesink, "xvimage->data is NULL. skip xvimage put..." );
          g_mutex_unlock (xvimagesink->flow_lock);


          return GST_FLOW_OK;
        }
        break;
      }
#ifdef USE_TBM
      case GST_MAKE_FOURCC('S', 'T', 'V', '1'): // MP memory allocated by TBM (only the index will be sent)
#ifdef XV_WEBKIT_PIXMAP_SUPPORT
        if (xvimagesink->get_pixmap_cb) {
          tbm_bo in_bo = NULL;
          GST_DEBUG_OBJECT (xvimagesink, "the incoming decoded frame comes from a sw decoder, and format is 'STV1'!");
          in_bo = get_decoded_frame_bo(xvimagesink->xvimage, buf);
          if (!in_bo)
            goto pixmap_stv1_support_error;
          if(!do_sw_tbm_scaling(xvimagesink->xvimage, in_bo))
            goto pixmap_stv1_support_error;
          if (!do_colorspace_conversion(xvimagesink->xvimage))
            goto pixmap_stv1_support_error;
          tbm_bo_unmap(in_bo);
          tbm_bo_unref(in_bo);
          g_mutex_unlock (xvimagesink->flow_lock);
          return GST_FLOW_OK;
      pixmap_stv1_support_error:
          GST_DEBUG_OBJECT (xvimagesink, "process sw 'STV1' frame failed!");
          if (in_bo) {
            tbm_bo_unmap(in_bo);
            tbm_bo_unref(in_bo);
          }
          g_mutex_unlock (xvimagesink->flow_lock);
          return GST_FLOW_ERROR;
        } else
#endif //XV_WEBKIT_PIXMAP_SUPPORT
      {
#if ENABLE_PERFORMANCE_CHECKING
	tbm_aptmp_time_tmp = get_time();
#endif
	   GST_LOG_OBJECT(xvimagesink, "STV1 display");
        // If no MP mem,  allocate 2 MP memory, and double buffer..
        FOXPXvDecInfo *pVideoFrame = (FOXPXvDecInfo*) xvimagesink->xvimage->xvimage->data;
#ifdef USE_TBM_SDK
        if (pVideoFrame && xvimagesink->p_sdk_TBMinfo[0])
	 {
          TBMinfoPTR tbmptr = (TBMinfoPTR)(xvimagesink->p_sdk_TBMinfo[xvimagesink->sdk_dsp_idx]);
	   GST_DEBUG_OBJECT(xvimagesink, "SW dsp_idx [%d]  idx[%d] !!!", xvimagesink->sdk_dsp_idx, tbmptr->idx);
#else
	 if (pVideoFrame && xvimagesink->xvimage->pTBMinfo[0])
	 {
          TBMinfoPTR tbmptr = (TBMinfoPTR)(xvimagesink->xvimage->pTBMinfo[xvimagesink->xvimage->dsp_idx]);
	   GST_DEBUG_OBJECT(xvimagesink, "SW dsp_idx [%d]  idx[%d] !!!", xvimagesink->xvimage->dsp_idx, tbmptr->idx);
#endif
       
          memset(pVideoFrame, 0x0, sizeof(FOXPXvDecInfo));
#if 1
          // 1. Get tbm_bo and tbm_bo_handle of AP from GstBuffer.priv.qdata
          // 2. generate tbm_ga_bitblt structure;
          // 3. COPY AP->MP by tbm_bo_ga_copy 
          tbm_ga_bitblt_wrap data;
          tbm_bo boAP = NULL;
          memset(&data, 0, sizeof(tbm_ga_bitblt_wrap));
          GstStructure* structure = gst_buffer_get_qdata(buf, g_quark_from_string("tbm_bo"));

          if (structure)
          {
            guint boAP_key = 0;
            if (gst_structure_get_uint(structure, "tbm_bo_key", &boAP_key))
              boAP = tbm_bo_import (tbmptr->bufmgr, boAP_key);
            else
              GST_ERROR_OBJECT(xvimagesink, "No tbm_bo_key");
#ifdef DUMP_SW_DATA
		GST_ERROR_OBJECT(xvimagesink, "Begin to dump Bo data, ");
		tbm_bo_handle boAP_hd = {0};
		boAP_hd = tbm_bo_get_handle(boAP, TBM_DEVICE_CPU);
		if(!boAP_hd.ptr) {
		  GST_ERROR_OBJECT (xvimagesink, "Fail to get the boAP_hd.ptr 0x%x !!! ", boAP_hd.ptr);
		}else {
			   int height_frm, width_frm;
			   int y_linesize, u_linesize,v_linesize;
			   height_frm = xvimagesink->video_height;
			   width_frm = xvimagesink->video_width;
			   y_linesize = xvimagesink->video_width;
			   u_linesize = xvimagesink->video_width;
			   v_linesize = xvimagesink->video_width;
				
			   int m_hw, n_hw, hw_h_hw, hw_w_hw, hw_linesize_hw;
			   unsigned char * pBuf_hw;
			   FILE * fpo_hw = NULL;
			   int fileid = 0;
			   if(!xvimagesink->is_check_sw_dump_filename_done){
				   do{
					sprintf(xvimagesink->dump_sw_filename, "/mnt/usb/sw_%d.yuv", fileid);
					fileid++;
				   }while(access( xvimagesink->dump_sw_filename, 0) == 0);
				   xvimagesink->is_check_sw_dump_filename_done = 1;
				   GST_ERROR_OBJECT(xvimagesink, "Final sw dump filename is [%s]", xvimagesink->dump_sw_filename);
			   }
			   fpo_hw = fopen(xvimagesink->dump_sw_filename, "a+");
			   if(!fpo_hw){
				GST_ERROR_OBJECT (xvimagesink, "SW frame data open file failed, need to mount /mnt/usb !!! ");
			   }else {
				//Y
				hw_h_hw = xvimagesink->video_height;
				hw_w_hw = xvimagesink->video_width;
				hw_linesize_hw = xvimagesink->video_width;
				pBuf_hw = boAP_hd.ptr;
				// no padding
				for(m_hw=0; m_hw<hw_h_hw; m_hw++){
					fwrite(pBuf_hw, hw_w_hw, 1, fpo_hw);
					pBuf_hw += hw_linesize_hw;
				}
				// CbCr
				hw_linesize_hw = xvimagesink->video_width;
				pBuf_hw = boAP_hd.ptr + (xvimagesink->video_height * xvimagesink->video_width)/2;
				if(0){	// Enable to dump CbCr data 
					if(0){
						// CbCr & no padding
						for(m_hw=0; m_hw<hw_h_hw/2; m_hw++){
							fwrite(pBuf_hw, hw_w_hw, 1, fpo_hw);
							pBuf_hw += hw_linesize_hw;
						}
					}else {
						// Cb & no padding
						pBuf_hw = boAP_hd.ptr + (xvimagesink->video_height * xvimagesink->video_width)/2;
						for(m_hw=0; m_hw<hw_h_hw/2; m_hw++){
							for (n_hw=0;n_hw<hw_w_hw;){
								fwrite(pBuf_hw, 1, 1, fpo_hw);
								pBuf_hw += 2;
								n_hw +=2;
							}
							pBuf_hw = boAP_hd.ptr + (xvimagesink->video_height * xvimagesink->video_width)/2 + (m_hw+1)*hw_linesize_hw;
						}
						// Cr & no padding
						pBuf_hw = boAP_hd.ptr + (xvimagesink->video_height * xvimagesink->video_width)/2;
						for(m_hw=0; m_hw<hw_h_hw/2; m_hw++){
							for (n_hw=0;n_hw<hw_w_hw;){
								fwrite(pBuf_hw, 1, 1, fpo_hw);
								pBuf_hw += 2;
								n_hw +=2;
							}
							pBuf_hw = boAP_hd.ptr + (xvimagesink->video_height * xvimagesink->video_width)/2 + 1 + (m_hw+1)*hw_linesize_hw;
						}
					  }
				  }
				  GST_DEBUG_OBJECT (xvimagesink, "SW frame data write file done !!! ");
				  fflush(fpo_hw);
				  fclose(fpo_hw);
		 	    }
		}
#endif  //DUMP_SW_DATA
            GST_LOG_OBJECT(xvimagesink, "boAP_key[ %d ] boAP[ %p ]", boAP_key, boAP);
            if (boAP)
            {
              gint ret1 = 0;
              gint ret2 = 0;
#if 1
              data.bufmgr = tbmptr->bufmgr;
              // Copy Y;
              data.src_bo = boAP;
              data.dst_bo = tbmptr->boY;
              data.src_paddr = NULL; // TODO: If the data from hw decoder, this should be set.
              data.dst_paddr = NULL;
              data.bitblt.color_mode = TBM_GA_FORMAT_8BPP;
              data.bitblt.ga_mode = TBM_GA_BITBLT_MODE_NORMAL;
              data.bitblt.src1_byte_size = xvimagesink->video_width * TBM_BPP8;
              data.bitblt.src1_rect.x = 0;
              data.bitblt.src1_rect.y = 0;  // Y: Top of buffer (w*h)
              data.bitblt.src1_rect.w = xvimagesink->video_width;
              data.bitblt.src1_rect.h = xvimagesink->video_height;
              data.bitblt.dst_byte_size = xvimagesink->dp_linesize * TBM_BPP8; // YUV420 : 1920,   YUV444 : 3846, Fixed linesize of destination MP FrameBuffer
              data.bitblt.dst_x = 0;
              data.bitblt.dst_y = 0;
              ret1 = tbm_bo_ga_copy(&data);

              // Copy CbCr;
              data.src_bo = boAP;
		 data.dst_bo = tbmptr->boCbCr;
              data.src_paddr = NULL; // TODO: If the data from hw decoder, this should be set.
              data.dst_paddr = NULL;
              data.bitblt.color_mode = TBM_GA_FORMAT_16BPP; // Because of CbCr Interleaved
              data.bitblt.ga_mode = TBM_GA_BITBLT_MODE_NORMAL;
              data.bitblt.src1_byte_size = xvimagesink->video_width * TBM_BPP8;
              data.bitblt.src1_rect.x = 0;
              data.bitblt.src1_rect.y = xvimagesink->video_height; // CbCr : Bottom of buffer (w*h/2)
              data.bitblt.src1_rect.w = xvimagesink->video_width/2;
              data.bitblt.src1_rect.h = xvimagesink->video_height/2;
              data.bitblt.dst_byte_size = xvimagesink->dp_linesize * TBM_BPP8;  // YUV420 : 1920,   YUV444 : 3846, Fixed linesize of destination MP FrameBuffer
              data.bitblt.dst_x = 0;
              data.bitblt.dst_y = 0;
              ret2 = tbm_bo_ga_copy(&data);
              tbm_bo_unref(boAP);
              boAP = NULL;
#else
              tbm_ga_scale_wrap data;
              memset(&data, 0, sizeof(tbm_ga_scale_wrap));

              data.bufmgr = tbmptr->bufmgr;
              // Copy Y;
              data.src_bo = boAP;
              data.dst_bo = tbmptr->boY;
              data.src_paddr = NULL; // TODO: If the data from hw decoder, this should be set.
              data.dst_paddr = NULL;
              data.scale.color_mode = TBM_GA_FORMAT_8BPP;
              data.scale.src_hbyte_size = xvimagesink->video_width * TBM_BPP8;
              data.scale.src_rect.x = 0;
              data.scale.src_rect.y = 0;  // Y: Top of buffer (w*h)
              data.scale.src_rect.w = xvimagesink->video_width;
              data.scale.src_rect.h = xvimagesink->video_height;
              data.scale.dst_hbyte_size = xvimagesink->video_width * TBM_BPP8; // YUV420 : 1920,   YUV444 : 3846, Fixed linesize of destination MP FrameBuffer
              data.scale.dst_rect.x = 0;
              data.scale.dst_rect.y = 0;
              data.scale.dst_rect.w = xvimagesink->video_width/2;  // 2:1 down scale
              data.scale.dst_rect.h = xvimagesink->video_height/2;  // 2:1 down scale
              data.scale.rop_mode = TBM_GA_ROP_COPY;
              data.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
              data.scale.rop_ca_value = 0;
              data.scale.src_key = 0;
              data.scale.rop_on_off = 0;
              ret1 = tbm_bo_ga_scale(&data);

              // Copy CbCr;
              data.src_bo = boAP;
              data.dst_bo = tbmptr->boCbCr;
              data.src_paddr = NULL; // TODO: If the data from hw decoder, this should be set.
              data.dst_paddr = NULL;
              data.scale.color_mode = TBM_GA_FORMAT_16BPP; // Because of CbCr Interleaved
              data.scale.src_hbyte_size = xvimagesink->video_width/2 * TBM_BPP16;
              data.scale.src_rect.x = 0;
              data.scale.src_rect.y = xvimagesink->video_height;  // CbCr : Bottom of buffer (w*h/2)
              data.scale.src_rect.w = xvimagesink->video_width/2;
              data.scale.src_rect.h = xvimagesink->video_height/2;
              data.scale.dst_hbyte_size = xvimagesink->video_width/2 * TBM_BPP16; // YUV420 : 1920,   YUV444 : 3846, Fixed linesize of destination MP FrameBuffer
              data.scale.dst_rect.x = 0;
              data.scale.dst_rect.y = 0;
              data.scale.dst_rect.w = xvimagesink->video_width/4; // 2:1 down scale
              data.scale.dst_rect.h = xvimagesink->video_height/4; // 2:1 down scale
              data.scale.rop_mode = TBM_GA_ROP_COPY;
              data.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
              data.scale.rop_ca_value = 0;
              data.scale.src_key = 0;
              data.scale.rop_on_off = 0;
              ret2 = tbm_bo_ga_scale(&data);
#endif
              if (ret1 != 1 || ret2 != 1)
                GST_ERROR_OBJECT(xvimagesink, "result of ga_copy [ Y:%d  C:%d ]", ret1, ret2);
            }
            else
            {
              GST_ERROR_OBJECT(xvimagesink, "No tbm_bo in this GstBuffer,  boAP_key[ %d ] boAP[ NULL ]", boAP_key);
              goto no_image;
            }
          }
          else
          {
            GST_ERROR_OBJECT(xvimagesink, "No tbm_bo in this GstBuffer");
            goto no_image;
          }
#else
          // memcpy
          // buf->data is pointer to Y/UV420 Planar.
          unsigned char* pY = buf->data;
          unsigned char* pCbCr = &(buf->data[xvimagesink->video_width*xvimagesink->video_height]);
          memcpy(tbmptr->bo_hnd_Y.ptr, pY, xvimagesink->video_width*xvimagesink->video_height);
          memcpy(tbmptr->bo_hnd_CbCr.ptr, pCbCr, xvimagesink->video_width*xvimagesink->video_height/2);
#endif
          if (xvimagesink->fps_d && xvimagesink->fps_n)
            pVideoFrame->framerate = xvimagesink->fps_n * 100 / xvimagesink->fps_d;
          else
          {
            pVideoFrame->framerate = 6000;
            GST_WARNING_OBJECT(xvimagesink, "wrong framerate [ %d / %d ] --> 6000", xvimagesink->fps_n, xvimagesink->fps_d);
          }
          pVideoFrame->colorformat = XV_DRM_COLORFORMAT_YUV420;
          pVideoFrame->scantype = 1; // always progressive
          pVideoFrame->display_index = tbmptr->idx; // 3. Set Index of MP
#ifdef USE_TBM_SDK          
          xvimagesink->sdk_dsp_idx++;  //  DP buffer 0 -> 1 -> 2 -> 0
	   if(xvimagesink->sdk_dsp_idx >= DISPLAY_BUFFER_NUM){
	  	xvimagesink->sdk_dsp_idx = 0;
	  }
#else
	   xvimagesink->xvimage->dsp_idx++;  //  DP buffer 0 -> 1 -> 2 -> 0
	   if(xvimagesink->xvimage->dsp_idx >= DISPLAY_BUFFER_NUM){
	 	xvimagesink->xvimage->dsp_idx = 0;
	  }
#endif
        }
        else
        {
#ifdef USE_TBM_SDK          
	  GST_ERROR_OBJECT( xvimagesink, "xvimage->data[ %p ], p_sdk_TBMinfo[ %p ]  skip xvimage put...", 
	  	img_data, xvimagesink->p_sdk_TBMinfo[0]);
#else
          GST_ERROR_OBJECT( xvimagesink, "xvimage->data[ %p ], pTBMinfo[ %p ]  skip xvimage put...", 
		img_data, xvimagesink->xvimage->pTBMinfo[0]);
#endif
          goto no_image;
        }
#if ENABLE_PERFORMANCE_CHECKING
		  tbm_aptmp_count++;
		  total_tbm_aptmp_time += get_time() - tbm_aptmp_time_tmp;
		  if(tbm_aptmp_count == 10 || tbm_aptmp_count == 300 || tbm_aptmp_count == 800 || tbm_aptmp_count == 1000 || tbm_aptmp_count == 2000 || tbm_aptmp_count == 3000){
			GST_ERROR_OBJECT(xvimagesink, "***************************TBM AP copy into MP performance test: *****************************************");
			GST_ERROR_OBJECT(xvimagesink, "TBM: tbm_aptmp_count =%d, total_tbm_aptmp_time =%lldus, average tbm aptmp time=%lldus", tbm_aptmp_count, total_tbm_aptmp_time, total_tbm_aptmp_time/tbm_aptmp_count);
		}
#endif
        break;
      }
#endif
      case GST_MAKE_FOURCC('S', 'T', 'V', '0'): // TV HW Decoded frame (v4l2_drm structure)
#ifdef XV_WEBKIT_PIXMAP_SUPPORT
      if (xvimagesink->get_pixmap_cb) {
		TBMinfoPTR tbmptr = (TBMinfoPTR)(xvimagesink->xvimage->pTBMinfo_web);

        GST_DEBUG_OBJECT (xvimagesink, "the incoming decoded frame comes from a hw decoder, and format is 'STV0'!");


        if(!do_hw_tbm_scaling(xvimagesink->xvimage, buf))
        	goto pixmap_i420_support_error;
        if (!do_colorspace_conversion(xvimagesink->xvimage))
        	goto pixmap_i420_support_error;
        GST_BUFFER_FLAG_SET(buf, GST_BUFFER_FLAG_LAST); // To inform decoder
        GST_DEBUG_OBJECT(xvimagesink, "flags [ %x ], buf[ %x ]", GST_BUFFER_FLAGS(buf), buf);
        g_mutex_unlock (xvimagesink->flow_lock);
        return GST_FLOW_OK;
      pixmap_i420_support_error:
        GST_DEBUG_OBJECT (xvimagesink, "process hw 'STV0' frame failed!");
        g_mutex_unlock (xvimagesink->flow_lock);
        return GST_FLOW_ERROR;
      } else
#endif
#if ENABLE_RT_DISPLAY
	//if(g_getenv("ES_DISPLAY")){
	if(xvimagesink->rt_display_vaule == 1){
		GST_DEBUG_OBJECT (xvimagesink, "RT: Handle to ES data flow !!!");
		if(!rt_do_display(xvimagesink->xvimage, buf))
			goto rt_display_error;
		g_mutex_unlock (xvimagesink->flow_lock);
		if(!gst_xvimagesink_xvimage_put(xvimagesink, xvimagesink->xvimage, GST_BUFFER_CAPS (buf), 0)){
			goto no_window;
		}
		if(!xvimagesink->rt_resetinfo_done){
			//mute_video_display(xvimagesink,FALSE,MUTE_STATE_RT_CHANGE);
			xvimagesink->rt_resetinfo_done = 1;
		}
		return GST_FLOW_OK;
	rt_display_error:
		GST_ERROR_OBJECT (xvimagesink, "RT: Display Error !!! ");
		g_mutex_unlock (xvimagesink->flow_lock);
		return GST_FLOW_ERROR;
	} else
#endif
      {
#if ENABLE_VF_ROTATE
	if(xvimagesink->user_enable_rotation){
		if(!xvimagesink->is_rotate_opened)
		{
			gint m, n;
			xvimagesink->frctx = videoframe_rotate_create();
			if(xvimagesink->frctx){
				xvimagesink->vf_sOutputFrame = (VFame *)g_malloc(sizeof(VFame));
				if(xvimagesink->vf_sOutputFrame){
					memset(xvimagesink->vf_sOutputFrame, 0, sizeof(struct VIDEO_FRAME));
				}else {
					GST_ERROR_OBJECT(xvimagesink, "Rotate: failed to malloc vf_sOutputFrame !!!");
				}
				xvimagesink->vf_display_bufmgr = NULL;
				xvimagesink->vf_display_drm_fd = -1;
				for(m = 0; m < PRE_DISPLAY_BUFFER_NUM; m++){
					for(n = 0; n < PRE_BUFFER_NUM; n++){
						memset (&xvimagesink->vf_pre_display_boY[m][n], 0x0, sizeof(tbm_bo));
						memset (&xvimagesink->vf_pre_display_boCbCr[m][n], 0x0, sizeof(tbm_bo));
						memset (&xvimagesink->vf_pre_display_bo_hnd_Y[m][n], 0x0, sizeof(tbm_bo_handle));
						memset (&xvimagesink->vf_pre_display_bo_hnd_CbCr[m][n], 0x0, sizeof(tbm_bo_handle));
					}
				}
	  		}else {
	    			GST_ERROR_OBJECT(xvimagesink, "Rotate: failed to create video rotation context !!!");
				goto display_channel_error;
	  		}
		
		  struct v4l2_drm *outinfo_open;
		  struct v4l2_private_frame_info *frminfo_open;
		  int linesize_open,height_frm_open,width_frm_open, scan_type_open;
		  outinfo_open = (struct v4l2_drm *)(GST_BUFFER_MALLOCDATA(buf));  /// get the v4l2_drm ptr
		  frminfo_open = &(outinfo_open->u.dec_info.pFrame[0]);		   /// get the v4l2_private_frame_info
		  linesize_open = frminfo_open->y_linesize;
		  height_frm_open= frminfo_open->height;
		  width_frm_open= frminfo_open->width;
		  scan_type_open = outinfo_open->u.dec_info.scantype;			// 1:  progressive, 0: interlaced
		  
		  xvimagesink->vf_bIsVideoRotationEnabled = TRUE;
		  xvimagesink->vf_current_degree = 0;   // Original degree
		  xvimagesink->vf_iOriginalWidth = xvimagesink->vf_iCurrentWidth = xvimagesink->vf_iScaledWidth = xvimagesink->video_width;
		  xvimagesink->vf_iOriginalHeight =  xvimagesink->vf_iCurrentHeight = xvimagesink->vf_iScaledHeight = xvimagesink->video_height;
		  xvimagesink->vf_iCodecId = -1;	// Default
		  xvimagesink->vf_pre_display_idx_n = 0;
		  if(scan_type_open == 0)
		  	xvimagesink->vf_bIsInterlacedScanType = TRUE;	  // set scan type from hw decinfo, interlaced
		  else
		  	xvimagesink->vf_bIsInterlacedScanType = FALSE;	  // Get scan type from hw decinfo, progressive
		  if(xvimagesink->xcontext->disp != NULL){
			  xvimagesink->frctx->disp = xvimagesink->xcontext->disp;
			  GST_DEBUG_OBJECT(xvimagesink, "Rotate: set display handle into rotation module(disp: tbm bufmgr init) !!!");
		  }else {
			  xvimagesink->frctx->disp = NULL;
			  GST_ERROR_OBJECT(xvimagesink, "Rotate: Failed to get the display handle !!!");
		  }
		  if (xvimagesink->fps_d && xvimagesink->fps_n){
			  xvimagesink->vf_iFramesPerSec =  xvimagesink->fps_n/xvimagesink->fps_d;
		  }else {
			  xvimagesink->vf_iFramesPerSec =  60;
		  }

		  if(!videoframe_rotate_can_support(xvimagesink->frctx,xvimagesink->vf_iFramesPerSec,height_frm_open,width_frm_open)){
			GST_WARNING_OBJECT(xvimagesink, "Rotate: Video rotation can't support this fps[%d] W[%d]H[%d]!!!", 
				xvimagesink->frctx->m_iFramesPerSec, height_frm_open,width_frm_open);
			xvimagesink->vf_bIsVideoRotationEnabled = FALSE;
		  }

		  if(xvimagesink->vf_bIsVideoRotationEnabled){		
			  //Open video frame rotate
			  GST_DEBUG_OBJECT(xvimagesink, "Rotate: open video rotation rotate, enable[%d], degree[%d], W/H [%d]/[%d], codecId[%d], scan_type[%d] !!!",
			  	xvimagesink->vf_bIsVideoRotationEnabled, xvimagesink->vf_current_degree, xvimagesink->vf_iOriginalWidth, xvimagesink->vf_iOriginalHeight,
			  	xvimagesink->vf_iCodecId, xvimagesink->vf_bIsInterlacedScanType);
			  ret = videoframe_rotate_open(xvimagesink->frctx, xvimagesink->vf_bIsVideoRotationEnabled, xvimagesink->vf_current_degree, xvimagesink->vf_iCurrentWidth, xvimagesink->vf_iCurrentHeight, xvimagesink->vf_iCodecId, xvimagesink->vf_bIsInterlacedScanType, linesize_open);
			  if(!ret){
				  GST_WARNING_OBJECT(xvimagesink, "Rotate: Failed to open video rotation !!!");
			  }else {
				  xvimagesink->vf_bIsVideoRotationEnabled = xvimagesink->frctx->m_bIsVideoRotationEnabled;
				  GST_DEBUG_OBJECT(xvimagesink, "Rotate: Succeed to open video rotation -> vf_bIsVideoRotationEnabled[%d] !!!", xvimagesink->vf_bIsVideoRotationEnabled);
			  }
		  }

		  if(frminfo_open->y_viraddr == NULL || frminfo_open->u_viraddr == NULL ){
			GST_WARNING_OBJECT(xvimagesink, "Rotate: HardWare viraddr is NULL, can not support this!!");
			xvimagesink->vf_bIsVideoRotationEnabled = FALSE;
		  }

		  // update support video rotation & set the auto rotation degree into current rotation mechanism.
		  if(xvimagesink->vf_bIsVideoRotationEnabled){
			  if(xvimagesink->CANRotate){
			  	if(xvimagesink->ARdegree == 0){
					xvimagesink->support_rotation = TRUE;	// same with the current rotation mechanism.
				}else {
					xvimagesink->support_rotation = FALSE; // same with orsay, if auto rotation degree is not 0, disable the user rotate.
					xvimagesink->vf_rotate_degree = xvimagesink->ARdegree;	
					xvimagesink->vf_rotate_setting_degree = xvimagesink->ARdegree;
					
					if(xvimagesink->vf_rotate_degree == 90)
						xvimagesink->rotate_angle = DEGREE_90;
					if(xvimagesink->vf_rotate_degree == 180)
						xvimagesink->rotate_angle = DEGREE_180;
					if(xvimagesink->vf_rotate_degree == 270)
						xvimagesink->rotate_angle = DEGREE_270;
					gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
				}
			  }else {
				xvimagesink->support_rotation = FALSE;
			  }
		  }else {
			  xvimagesink->support_rotation = FALSE;
		  }
		  xvimagesink->is_rotate_opened = TRUE;
		  
		  GST_INFO_OBJECT(xvimagesink, "Rotate: Set Video Rotation Support = %d ", xvimagesink->support_rotation);
		  GST_INFO_OBJECT(xvimagesink, "Rotate: open video rotation rotate end");
		}
	}else {
		xvimagesink->vf_bIsVideoRotationEnabled = FALSE;
		xvimagesink->support_rotation = FALSE;
		/* GST_DEBUG_OBJECT(xvimagesink, "Rotate: user not enable video rotation !!!"); */
	}
	if(xvimagesink->vf_rotate_setting_degree != xvimagesink->vf_rotate_degree)
	{
		GST_DEBUG_OBJECT(xvimagesink,"update rotate degree from [%d] to [%d]",xvimagesink->vf_rotate_degree,xvimagesink->vf_rotate_setting_degree);
		// Should update the rotate degree before get new display coordinates
		xvimagesink->vf_rotate_degree = xvimagesink->vf_rotate_setting_degree;
		xvimagesink->rotate_angle = xvimagesink->rotate_setting_angle;

		if(xvimagesink->vf_rotate_setting_degree == 0)
		{
			xvimagesink->vf_iScaledWidth = xvimagesink->video_width;
			xvimagesink->vf_iScaledHeight = xvimagesink->video_height;
			gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
		}		
	}

	if(xvimagesink->vf_bIsVideoRotationEnabled&&(xvimagesink->vf_rotate_degree != 0)){
#if ENABLE_PERFORMANCE_CHECKING
		rotate_total_time_tmp = get_time();
#endif

		  ///Apply video frame rotation
		  //1. set rotate degree
		  videoframe_rotate_set_degree(xvimagesink->frctx, xvimagesink->vf_rotate_degree);
		  if(xvimagesink->frctx->m_bIsRotateAngleChanged){
			   GST_DEBUG_OBJECT(xvimagesink, "Rotate: rotation degree changed & set vf_rotate_degree[%d] !!!", xvimagesink->vf_rotate_degree);
			   xvimagesink->vf_iScaledWidth = videoframe_rotate_get_scaled_width(xvimagesink->frctx);
			   xvimagesink->vf_iScaledHeight = videoframe_rotate_get_scaled_height(xvimagesink->frctx);
			   gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
		   }
		  
		  if((xvimagesink->vf_iScaledWidth !=xvimagesink->vf_iCurrentWidth) ||
			 (xvimagesink->vf_iScaledHeight !=xvimagesink->vf_iCurrentHeight)){
				pre_adjust_rotate_index(xvimagesink);
				if(xvimagesink->is_first_putimage == FALSE)
				{
					mute_video_display(xvimagesink, TRUE, MUTE_ROTATION_CHANGE);  // switch 90/180/270 degree, we need to mute
					need_unmute_after_rotation_putimage = 1;
				}
				GST_DEBUG_OBJECT(xvimagesink,"Rotate: reset display and set vf_degree_idx = %d ", xvimagesink->vf_degree_idx);
		  }

		   //2. Get the HW Decoder Frame Data
		   struct v4l2_drm *outinfo;
		   struct v4l2_drm_dec_info * decinfo;
		   struct v4l2_private_frame_info *frminfo;
		   int height_dec, width_dec;
		   int height_frm, width_frm;
		   int y_linesize, u_linesize,v_linesize;
		   int display_index;

		   outinfo = (struct v4l2_drm *)(GST_BUFFER_MALLOCDATA(buf));  /// get the v4l2_drm ptr
		   decinfo = &(outinfo->u.dec_info);				   /// get the v4l2_drm_dec_info
		   frminfo = &(outinfo->u.dec_info.pFrame[0]);		   /// get the v4l2_private_frame_info
		   height_dec = decinfo->hres;					   /// get the Source horizontal size
		   width_dec = decinfo->vres;					   /// get the Source vertical size
		   height_frm = frminfo->height;
		   width_frm = frminfo->width;
		   y_linesize = frminfo->y_linesize;
		   u_linesize = frminfo->u_linesize;
		   v_linesize = frminfo->v_linesize;
		   display_index = frminfo->display_index;
		   GST_DEBUG_OBJECT(xvimagesink, "Rotate: Get HW Frame Data info: y_viraddr[0x%x] u_viraddr[0x%x] y_linesize[%d] u_linesize[%d]  W[%d]/H[%d]  y_phyaddr[0x%x] u_phyaddr[0x%x] v_phyaddr[0x%x] !!!",
			  frminfo->y_viraddr, frminfo->u_viraddr, frminfo->y_linesize, frminfo->u_linesize, frminfo->width, frminfo->height,  frminfo->y_phyaddr, frminfo->u_phyaddr, frminfo->v_phyaddr);

#ifdef DUMP_HW_DISCONTINUS_DATA
		{
			int hw_h_hw_disc, hw_w_hw_disc, hw_linesize_hw_disc;
			int i_disc, k_disc, tt_disc;
			unsigned char * pBuf_hw_disc;
			FILE * fpo_hw_disc_org_disc = NULL;
			FILE * fpo_hw_disc = NULL;
			fpo_hw_disc_org_disc = fopen("/mnt/usb/hw_org_disc.yuv", "a+");
			fpo_hw_disc = fopen("/mnt/usb/hw_disc.yuv", "a+");
			if(!fpo_hw_disc || !fpo_hw_disc_org_disc){
				GST_ERROR_OBJECT (xvimagesink, "HW frame data open file failed, need to mount /mnt/usb !!! ");
			}else {
				//Y
				hw_h_hw_disc = frminfo->height;
				hw_w_hw_disc = frminfo->width;
					// orginal
				hw_linesize_hw_disc = frminfo->y_linesize;
				pBuf_hw_disc = frminfo->y_viraddr;
				for(k_disc = 0; k_disc < hw_linesize_hw_disc; k_disc++){	// 0 ~ linesize-1
					for(i_disc = 0; i_disc < hw_h_hw_disc; i_disc++){		// 0 ~ height-1
						fwrite(pBuf_hw_disc, 1, 1, fpo_hw_disc_org_disc);
						pBuf_hw_disc += hw_linesize_hw_disc;	
					}
					pBuf_hw_disc = frminfo->y_viraddr + k_disc;	// jump to next column
				}
					// no padding
				hw_linesize_hw_disc = frminfo->y_linesize;
				pBuf_hw_disc = frminfo->y_viraddr;
				for(k_disc = 0; k_disc < hw_w_hw_disc; k_disc++){			// 0 ~ width-1
					for(i_disc = 0; i_disc < hw_h_hw_disc; i_disc++){		// 0 ~ height-1
						fwrite(pBuf_hw_disc, 1, 1, fpo_hw_disc);
						pBuf_hw_disc += hw_linesize_hw_disc;
					}
					pBuf_hw_disc = frminfo->y_viraddr + k_disc;	// jump to next column
				}
				GST_DEBUG_OBJECT (xvimagesink, "HW frame data write file done !!! ");
				fflush(fpo_hw_disc_org_disc);
				fclose(fpo_hw_disc_org_disc);
				fflush(fpo_hw_disc);
				fclose(fpo_hw_disc);
			}
		}
#endif	//DUMP_HW_DISCONTINUS_DATA

#ifdef DUMP_ROTATE_HW_DATA
		 {
			int m_hw, n_hw, hw_h_hw, hw_w_hw, hw_linesize_hw;
			unsigned char * pBuf_hw;
			FILE * fpo_hw_org = NULL;
			FILE * fpo_hw = NULL;
			fpo_hw_org = fopen("/mnt/usb/hw_org.yuv", "a+");
			fpo_hw = fopen("/mnt/usb/hw.yuv", "a+");
			if(!fpo_hw || !fpo_hw_org){
				GST_ERROR_OBJECT (xvimagesink, "HW frame data open file failed, need to mount /mnt/usb !!! ");
			}else {
				//Y
				hw_h_hw = frminfo->height;
				hw_w_hw = frminfo->width;
				hw_linesize_hw = frminfo->y_linesize;
				pBuf_hw = frminfo->y_viraddr;
					// orginal
				fwrite(pBuf_hw, hw_linesize_hw, hw_h_hw, fpo_hw_org);
					// no padding
				for(m_hw=0; m_hw<hw_h_hw; m_hw++){
					fwrite(pBuf_hw, hw_w_hw, 1, fpo_hw);
					pBuf_hw += hw_linesize_hw;
				}
				// CbCr
				hw_linesize_hw = frminfo->u_linesize;
				pBuf_hw = frminfo->u_viraddr;
					// orginal
					fwrite(pBuf_hw, hw_linesize_hw, hw_h_hw/2, fpo_hw_org);
				if(0){
					// CbCr & no padding
					for(m_hw=0; m_hw<hw_h_hw/2; m_hw++){
						fwrite(pBuf_hw, hw_w_hw, 1, fpo_hw);
						pBuf_hw += hw_linesize_hw;
					}
				}else {
					// Cb & no padding
					pBuf_hw = frminfo->u_viraddr;
					for(m_hw=0; m_hw<hw_h_hw/2; m_hw++){
						for (n_hw=0;n_hw<hw_w_hw;){
							fwrite(pBuf_hw, 1, 1, fpo_hw);
							pBuf_hw += 2;
							n_hw +=2;
						}
						pBuf_hw = frminfo->u_viraddr + (m_hw+1)*hw_linesize_hw;
					}
					// Cr & no padding
					pBuf_hw = frminfo->u_viraddr + 1;
					for(m_hw=0; m_hw<hw_h_hw/2; m_hw++){
						for (n_hw=0;n_hw<hw_w_hw;){
							fwrite(pBuf_hw, 1, 1, fpo_hw);
							pBuf_hw += 2;
							n_hw +=2;
						}
						pBuf_hw = frminfo->u_viraddr + 1 + (m_hw+1)*hw_linesize_hw;
					}
				}
				GST_DEBUG_OBJECT (xvimagesink, "HW frame data write file done !!! ");
				fflush(fpo_hw_org);
				fclose(fpo_hw_org);
				fflush(fpo_hw);
				fclose(fpo_hw);
			}
		}
#endif	//DUMP_ROTATE_HW_DATA

		   //3. Init Output Frame Params
		   memset(xvimagesink->vf_sOutputFrame, 0, sizeof(struct VIDEO_FRAME));
		   xvimagesink->vf_sOutputFrame->pData0 = frminfo->y_viraddr;		  //Y: frminfo->y_viraddr
		   xvimagesink->vf_sOutputFrame->pData1 = frminfo->u_viraddr;		  //UV: frminfo->u_viraddr
		   xvimagesink->vf_sOutputFrame->pData2 = NULL;
		   xvimagesink->vf_sOutputFrame->pData3 = NULL;
		   xvimagesink->vf_sOutputFrame->lineSize0 = frminfo->y_linesize;
		   xvimagesink->vf_sOutputFrame->lineSize1 = frminfo->u_linesize;
		   xvimagesink->vf_sOutputFrame->lineSize2 = 0;
		   xvimagesink->vf_sOutputFrame->lineSize3 = 0;
		   xvimagesink->vf_sOutputFrame->width = frminfo->width;
		   xvimagesink->vf_sOutputFrame->height = frminfo->height;
		   xvimagesink->vf_sOutputFrame->eColorFormat = -1;
		   xvimagesink->vf_sOutputFrame->eVideoDataFormat = -1;
		   xvimagesink->vf_sOutputFrame->iFrameDimension = -1;
		   xvimagesink->vf_sOutputFrame->bIsRotationChanged = -1;
		   xvimagesink->vf_sOutputFrame->bResolutionChanged = -1;
		   xvimagesink->vf_sOutputFrame->iKeyFrame = -1;
		   xvimagesink->vf_sOutputFrame->iVideoDecodingMode = -1;
		   xvimagesink->vf_sOutputFrame->iVideoFormat = -1;
		   
#ifdef ENABLE_MID_BUFFER
		if(xvimagesink->video_width < 1280 || xvimagesink->video_height < 720){
			gint copy_size = 0;

			copy_size = frminfo->y_linesize*frminfo->height;
			Y_buffer = g_malloc(copy_size*sizeof(unsigned char));
			if(Y_buffer){
				memcpy(Y_buffer, frminfo->y_viraddr, copy_size);
			}else{
				GST_WARNING_OBJECT (xvimagesink, "Mid Buffer: Y_buffer size[%d] allcate failed !!! ", copy_size);
			}
	   		copy_size = frminfo->u_linesize*frminfo->height/2;
	   		UV_buffer = g_malloc(copy_size*sizeof(unsigned char));
			if(UV_buffer){
	   			memcpy(UV_buffer, frminfo->u_viraddr, copy_size);
			}else {
				GST_WARNING_OBJECT (xvimagesink, "Mid Buffer: UV_buffer size[%d] allcate failed !!! ", copy_size);
			}
	   		xvimagesink->vf_sOutputFrame->pData0 = Y_buffer;
	   		xvimagesink->vf_sOutputFrame->pData1 = UV_buffer;
	   	}
#endif	// ENABLE_MID_BUFFER

		   //3. Apply the video frame rotate
		   if(xvimagesink->vf_display_channel == 0) {
			invalidate_hw_decoded_frame_cache(GST_BASE_SINK_CAST(xvimagesink)->sinkpad, TRUE);
		   }
		   if(!set_rotate_video_display_channel(xvimagesink, xvimagesink->vf_display_channel, 1)){
			goto display_channel_error;
		   }

		  GST_DEBUG_OBJECT(xvimagesink, "Rotate: ready to apply the video rotation: W/H[%d][%d]->Scale_W/Scale_H[%d][%d] Degree[%d]->[%d] ",
		   xvimagesink->vf_iCurrentWidth, xvimagesink->vf_iCurrentHeight, xvimagesink->vf_iScaledWidth, xvimagesink->vf_iScaledHeight, xvimagesink->vf_current_degree, xvimagesink->vf_rotate_degree);
#if ENABLE_PERFORMANCE_CHECKING
		rotate_time_tmp = get_time();
#endif
		videoframe_rotate_update_rotate_angle_change_state(xvimagesink->frctx,1);
		ret = videoframe_rotate_apply(xvimagesink->frctx, 0, xvimagesink->vf_sOutputFrame, xvimagesink->vf_rotate_degree, &xvimagesink->vf_iFrameDone);
#if ENABLE_PERFORMANCE_CHECKING
		rotate_hw_into_AP_time = get_time() - rotate_time_tmp;
#endif

#ifdef ENABLE_MID_BUFFER
		if(xvimagesink->video_width < 1280 || xvimagesink->video_height < 720){
			if(Y_buffer)
				g_free(Y_buffer);
			if(UV_buffer)
				g_free(UV_buffer);
			Y_buffer = NULL;
			UV_buffer = NULL;
		}
#endif	//ENABLE_MID_BUFFER

		if(xvimagesink->vf_iFrameDone && ret){
			GST_DEBUG_OBJECT(xvimagesink,"Rotate: video rotation FrameDone[%d] ret[%d] ", xvimagesink->vf_iFrameDone, ret);
#ifdef DUMP_ROTATE_DATA
/* if(xvimagesink->vf_current_degree == 180) */
			{
				   int m_rotate, n_rotate, tbm_h_rotate, tbm_w_rotate, tbm_linesize_rotate;
				   unsigned char * pBuf_tbm_rotate;
				   FILE * fpo_rotate = NULL;
				   fpo_rotate = fopen("/mnt/usb/rotate.yuv", "a+");
				   if(!fpo_rotate){
					   GST_ERROR_OBJECT (xvimagesink, "Rotate: rotate_tbm open file failed !!! ");
				   }else {
					   tbm_h_rotate = xvimagesink->vf_sOutputFrame->height;
					   tbm_w_rotate = xvimagesink->vf_sOutputFrame->width;
					   tbm_linesize_rotate = xvimagesink->vf_sOutputFrame->lineSize0;
					   pBuf_tbm_rotate = xvimagesink->vf_sOutputFrame->pData0;
					   GST_DEBUG_OBJECT (xvimagesink, "Rotate: output frame_Y W[%d]/H[%d]/Linesize[%d]/PTR[0x%x] !!! ", tbm_w_rotate, tbm_h_rotate, tbm_linesize_rotate, pBuf_tbm_rotate);
					   for(m_rotate=0; m_rotate<tbm_h_rotate; m_rotate++){
						fwrite(pBuf_tbm_rotate, tbm_w_rotate, 1, fpo_rotate);
						pBuf_tbm_rotate += tbm_linesize_rotate;
					   }
					   tbm_linesize_rotate = xvimagesink->vf_sOutputFrame->lineSize1;
					   pBuf_tbm_rotate = xvimagesink->vf_sOutputFrame->pData1;
					   GST_DEBUG_OBJECT (xvimagesink, "Rotate: output frame_CbCr W[%d]/H[%d]/Linesize[%d]/PTR[0x%x] !!! ", tbm_w_rotate, tbm_h_rotate, tbm_linesize_rotate, pBuf_tbm_rotate);
				  	   if(0){
						   pBuf_tbm_rotate = xvimagesink->vf_sOutputFrame->pData1;
						   for(m_rotate=0; m_rotate<tbm_h_rotate/2; m_rotate++){
								fwrite(pBuf_tbm_rotate, tbm_w_rotate, 1, fpo_rotate);
								pBuf_tbm_rotate += tbm_linesize_rotate;
					         }
					   }else {
					   // Cb
						pBuf_tbm_rotate = xvimagesink->vf_sOutputFrame->pData1;
						for(m_rotate=0; m_rotate<tbm_h_rotate/2; m_rotate++){
							for (n_rotate=0;n_rotate<tbm_w_rotate;){
								fwrite(pBuf_tbm_rotate, 1, 1, fpo_rotate);
								pBuf_tbm_rotate += 2;
								n_rotate +=2;
							}
							pBuf_tbm_rotate = xvimagesink->vf_sOutputFrame->pData1 + (m_rotate+1)*tbm_linesize_rotate;
						}
					   // Cr
						pBuf_tbm_rotate = xvimagesink->vf_sOutputFrame->pData1 + 1;
						for(m_rotate=0; m_rotate<tbm_h_rotate/2; m_rotate++){
							for (n_rotate=0;n_rotate<tbm_w_rotate;){
								fwrite(pBuf_tbm_rotate, 1, 1, fpo_rotate);
								pBuf_tbm_rotate += 2;
								n_rotate +=2;
							}
							pBuf_tbm_rotate = xvimagesink->vf_sOutputFrame->pData1 + 1 + (m_rotate+1)*tbm_linesize_rotate;
						}
					}
					GST_DEBUG_OBJECT (xvimagesink, "Rotate: rotate_tbm write file done !!! ");
					fflush(fpo_rotate);
					fclose(fpo_rotate);
				}
			}
#endif	// DUMP_ROTATE_DATA
		if(!xvimagesink->vf_pre_allocate_done){
			ret = pre_free_rotate_tbm_display_buffer(xvimagesink);
			ret = pre_allocate_rotate_tbm_display_buffer(xvimagesink);
			videoframe_rotate_set_degree(xvimagesink->frctx, xvimagesink->vf_rotate_degree);	// reset to the orginal video rotation setting
			if(ret) {
				GST_DEBUG_OBJECT(xvimagesink,"Rotate: Success to pre allocate tbm display buffer");
			}else {
				GST_ERROR_OBJECT(xvimagesink,"Rotate: Failed to pre allocate tbm display buffer");
				return GST_FLOW_ERROR;
			}
		}

		//2. GA copy rotate buffer into MP
#if ENABLE_PERFORMANCE_CHECKING
		rotate_time_tmp = get_time();
#endif
		//2.1 Copy Local Buffer into AP.
	#ifdef ENABLE_LOCAL_ROTATE_BUFFER
		memcpy(xvimagesink->frctx->bo_handle_AP.ptr, xvimagesink->frctx->pLocalRotateBuffer_Y, 1920*1080*sizeof(unsigned char));
		memcpy(xvimagesink->frctx->bo_handle_AP_CbCr.ptr, xvimagesink->frctx->pLocalRotateBuffer_CbCr, 1920*1080*sizeof(unsigned char));
	#endif
		//2.2 copy AP into MP
		tbm_ga_bitblt_wrap data;
		memset(&data, 0, sizeof(tbm_ga_bitblt_wrap));
		gint ret1 = 0, ret2 = 0;
		/*
		tbm_bo boAP = NULL;
		tbm_bo boAP_CbCr = NULL;
		boAP = tbm_bo_import (xvimagesink->vf_display_bufmgr, xvimagesink->frctx->boAP_key);
		boAP_CbCr = tbm_bo_import (xvimagesink->vf_display_bufmgr, xvimagesink->frctx->boAP_CbCr_key);
		if (boAP)
		*/
		if(xvimagesink->frctx->bo_AP)
		{
			data.bufmgr = xvimagesink->vf_display_bufmgr;
			// Copy Y;
			//data.src_bo = boAP;
			data.src_bo = xvimagesink->frctx->bo_AP;
			data.dst_bo = xvimagesink->vf_pre_display_boY[xvimagesink->vf_pre_display_idx_n][xvimagesink->vf_degree_idx];
			data.src_paddr = NULL; // TODO: If the data from hw decoder, this should be set.
			data.dst_paddr = NULL;
			data.bitblt.color_mode = TBM_GA_FORMAT_8BPP;
			data.bitblt.ga_mode = TBM_GA_BITBLT_MODE_NORMAL;
			data.bitblt.src1_byte_size = xvimagesink->vf_iScaledWidth* TBM_BPP8;	// vf_iScaledWidth == linesize
			data.bitblt.src1_rect.x = 0;
			data.bitblt.src1_rect.y = 0;	// Y: Top of buffer (w*h)
			data.bitblt.src1_rect.w = xvimagesink->vf_iScaledWidth;
			data.bitblt.src1_rect.h = xvimagesink->vf_iScaledHeight;
			data.bitblt.dst_byte_size = xvimagesink->dp_linesize * TBM_BPP8; // --> YUV420 : 1920,   YUV444 : 3846, Fixed linesize of destination MP FrameBuffer
			data.bitblt.dst_x = 0;
			data.bitblt.dst_y = 0;
			ret1 = tbm_bo_ga_copy(&data);
		}
		if(xvimagesink->frctx->bo_AP_CbCr){
			// Copy CbCr;
			//data.src_bo = boAP_CbCr;
			data.src_bo = xvimagesink->frctx->bo_AP_CbCr;
			data.dst_bo = xvimagesink->vf_pre_display_boCbCr[xvimagesink->vf_pre_display_idx_n][xvimagesink->vf_degree_idx];
			data.src_paddr = NULL; // TODO: If the data from hw decoder, this should be set.
			data.dst_paddr = NULL;
			data.bitblt.color_mode = TBM_GA_FORMAT_16BPP; // Because of CbCr Interleaved
			data.bitblt.ga_mode = TBM_GA_BITBLT_MODE_NORMAL;
			data.bitblt.src1_byte_size = xvimagesink->vf_iScaledWidth * TBM_BPP8;	// vf_iScaledWidth == linesize
			data.bitblt.src1_rect.x = 0;
			data.bitblt.src1_rect.y = 0;
			data.bitblt.src1_rect.w = xvimagesink->vf_iScaledWidth/2;
			data.bitblt.src1_rect.h = xvimagesink->vf_iScaledHeight/2;
			data.bitblt.dst_byte_size = xvimagesink->dp_linesize * TBM_BPP8;	  // YUV420 : 1920,   YUV444 : 3846, Fixed linesize of destination MP FrameBuffer
			data.bitblt.dst_x = 0;
			data.bitblt.dst_y = 0;
			ret2 = tbm_bo_ga_copy(&data);
		}
		if (ret1 != 1 || ret2 != 1){
			GST_ERROR_OBJECT(xvimagesink, "Rotate: Failed GA Copy AP into MP -> ga_copy [ Y:%d  C:%d ]", ret1, ret2);
		}else {
			GST_DEBUG_OBJECT(xvimagesink,"Rotate: Succeed video rotation FrameDone[%d] ret[%d] \n", xvimagesink->vf_iFrameDone, ret);
			videoframe_rotate_update_rotate_angle_change_state(xvimagesink->frctx,0);
			xvimagesink->vf_current_degree = xvimagesink->vf_rotate_degree;
			xvimagesink->vf_iCurrentWidth = xvimagesink->vf_iScaledWidth;
			xvimagesink->vf_iCurrentHeight = xvimagesink->vf_iScaledHeight;
			xvimagesink->orientation = xvimagesink->vf_current_degree/90;		// Same with the New CoreAPI which added by SWC(eunhae1.choi@samsung.com), 2014-03-18
		}
#if ENABLE_PERFORMANCE_CHECKING
		rotate_AP_inot_MP_time = get_time() - rotate_time_tmp;
#endif
#if ENABLE_PERFORMANCE_CHECKING
		rotate_total_time = get_time() - rotate_total_time_tmp;
		rotate_frame_count++;
#endif
		  xvimagesink->vf_bIsVideoRotationEnabled = xvimagesink->frctx->m_bIsVideoRotationEnabled;
		  GST_DEBUG_OBJECT(xvimagesink, "Rotate: update vf_bIsVideoRotationEnabled[%d] !!!", xvimagesink->vf_bIsVideoRotationEnabled);
		  ///Set pVideoFrame params
		  FOXPXvDecInfo *pVideoFrame = (FOXPXvDecInfo*) xvimagesink->xvimage->xvimage->data;
		  if(pVideoFrame){
			memset(pVideoFrame, 0x0, sizeof(FOXPXvDecInfo));
		  }
		  if (xvimagesink->fps_d && xvimagesink->fps_n){
		    pVideoFrame->framerate = xvimagesink->fps_n * 100 / xvimagesink->fps_d;
		  }else{
		    pVideoFrame->framerate = 6000;
			GST_WARNING_OBJECT(xvimagesink, "Rotate: wrong framerate [ %d / %d ] --> 6000", xvimagesink->fps_n, xvimagesink->fps_d);
		  }
		  pVideoFrame->colorformat = XV_DRM_COLORFORMAT_YUV420;
		  pVideoFrame->scantype = 1; // always progressive
               pVideoFrame->display_index = xvimagesink->vf_pre_display_idx[xvimagesink->vf_pre_display_idx_n][xvimagesink->vf_degree_idx]; // 3. Set Index of MP
		  xvimagesink->vf_need_update_display_idx = 1;
		  GST_DEBUG_OBJECT(xvimagesink, "Rotate: Finish Xvideo display setting in MP channel  display_index[%d] , dsp[%d]degree[%d] update[%d]!!! ", 
		  	pVideoFrame->display_index, xvimagesink->vf_pre_display_idx_n, xvimagesink->vf_degree_idx, xvimagesink->vf_need_update_display_idx);
		  break;
		}else {
			//Skip this rotation frame
			goto skip_frame;
		}
		break;
	}else {
#ifdef DUMP_HW_DATA
	{
	   struct v4l2_drm *outinfo_hw;
	   struct v4l2_drm_dec_info * decinfo_hw;
	   struct v4l2_private_frame_info *frminfo_hw;
	   int height_dec, width_dec;
	   int height_frm, width_frm;
	   int y_linesize, u_linesize,v_linesize;
	   int display_index;
	
	   outinfo_hw = (struct v4l2_drm *)(GST_BUFFER_MALLOCDATA(buf));  /// get the v4l2_drm ptr
	   decinfo_hw = &(outinfo_hw->u.dec_info);				   /// get the v4l2_drm_dec_info
	   frminfo_hw = &(outinfo_hw->u.dec_info.pFrame[0]);		   /// get the v4l2_private_frame_info
	   height_dec = decinfo_hw->hres;					   /// get the Source horizontal size
	   width_dec = decinfo_hw->vres;					   /// get the Source vertical size
	   height_frm = frminfo_hw->height;
	   width_frm = frminfo_hw->width;
	   y_linesize = frminfo_hw->y_linesize;
	   u_linesize = frminfo_hw->u_linesize;
	   v_linesize = frminfo_hw->v_linesize;
	   display_index = frminfo_hw->display_index;
		
	   int m_hw, n_hw, hw_h_hw, hw_w_hw, hw_linesize_hw;
	   unsigned char * pBuf_hw;
	   FILE * fpo_hw_org = NULL;
	   FILE * fpo_hw = NULL;
	   fpo_hw_org = fopen("/mnt/usb/hw_org.yuv", "a+");
	   fpo_hw = fopen("/mnt/usb/hw.yuv", "a+");
	   if(!fpo_hw || !fpo_hw_org){
		GST_ERROR_OBJECT (xvimagesink, "HW frame data open file failed, need to mount /mnt/usb !!! ");
	   }else {
		//Y
		hw_h_hw = frminfo_hw->height;
		hw_w_hw = frminfo_hw->width;
		hw_linesize_hw = frminfo_hw->y_linesize;
		pBuf_hw = frminfo_hw->y_viraddr;
		// orginal
		fwrite(pBuf_hw, hw_linesize_hw, hw_h_hw, fpo_hw_org);
		// no padding
		for(m_hw=0; m_hw<hw_h_hw; m_hw++){
			fwrite(pBuf_hw, hw_w_hw, 1, fpo_hw);
			pBuf_hw += hw_linesize_hw;
		}
		// CbCr
		hw_linesize_hw = frminfo_hw->u_linesize;
		pBuf_hw = frminfo_hw->u_viraddr;
		// orginal
		fwrite(pBuf_hw, hw_linesize_hw, hw_h_hw/2, fpo_hw_org);
		if(0){
			// CbCr & no padding
			for(m_hw=0; m_hw<hw_h_hw/2; m_hw++){
				fwrite(pBuf_hw, hw_w_hw, 1, fpo_hw);
				pBuf_hw += hw_linesize_hw;
			}
		}else {
			// Cb & no padding
			pBuf_hw = frminfo_hw->u_viraddr;
			for(m_hw=0; m_hw<hw_h_hw/2; m_hw++){
				for (n_hw=0;n_hw<hw_w_hw;){
					fwrite(pBuf_hw, 1, 1, fpo_hw);
					pBuf_hw += 2;
					n_hw +=2;
				}
				pBuf_hw = frminfo_hw->u_viraddr + (m_hw+1)*hw_linesize_hw;
			}
			// Cr & no padding
			pBuf_hw = frminfo_hw->u_viraddr + 1;
			for(m_hw=0; m_hw<hw_h_hw/2; m_hw++){
				for (n_hw=0;n_hw<hw_w_hw;){
					fwrite(pBuf_hw, 1, 1, fpo_hw);
					pBuf_hw += 2;
					n_hw +=2;
				}
				pBuf_hw = frminfo_hw->u_viraddr + 1 + (m_hw+1)*hw_linesize_hw;
			}
		  }
		  GST_DEBUG_OBJECT (xvimagesink, "HW frame data write file done !!! ");
		  fflush(fpo_hw_org);
		  fclose(fpo_hw_org);
		  fflush(fpo_hw);
		  fclose(fpo_hw);
 	    }
      }
#endif	//DUMP_HW_DATA
	
		// Reset the display module, and use the HW display.
		if(xvimagesink->vf_display_channel == 1) {
			invalidate_hw_decoded_frame_cache(GST_BASE_SINK_CAST(xvimagesink)->sinkpad, FALSE);
			mute_video_display(xvimagesink, TRUE, MUTE_ROTATION_CHANGE);  // Enter 0 degree, we need to mute, first
			need_unmute_after_rotation_putimage = 1;
		}
		if(!set_rotate_video_display_channel(xvimagesink, xvimagesink->vf_display_channel, 0)){
			goto display_channel_error;
		}

		//Reset the width/height & rotate angle context
		xvimagesink->vf_iOriginalWidth = xvimagesink->vf_iCurrentWidth = xvimagesink->vf_iScaledWidth = xvimagesink->video_width;
		xvimagesink->vf_iOriginalHeight =  xvimagesink->vf_iCurrentHeight = xvimagesink->vf_iScaledHeight = xvimagesink->video_height;
		xvimagesink->vf_rotate_setting_degree = xvimagesink->vf_rotate_degree = xvimagesink->vf_current_degree = 0;
		if(xvimagesink->frctx)
			xvimagesink->frctx->m_iRotationDegree = 0;
		xvimagesink->rotate_setting_angle = xvimagesink->rotate_angle = DEGREE_0;

		struct v4l2_drm *outinfo = (struct v4l2_drm *)(GST_BUFFER_MALLOCDATA(buf));  /// get the v4l2_drm ptr
		if (!outinfo)
			goto no_image;
		struct v4l2_private_frame_info *frminfo_t = &(outinfo->u.dec_info.pFrame[0]);		   /// get the v4l2_private_frame_info;

		if(outinfo){
			outinfo->stereoscopic_info = V4L2_DRM_INPUT_STEREOSCOPIC_2D;
		}
		if (xvimagesink->seamless_resolution_change == TRUE)
		{
			if(outinfo){
				outinfo->u.dec_info.next_res.max_hres = xvimagesink->max_video_width;
				outinfo->u.dec_info.next_res.max_vres = xvimagesink->max_video_height;
			}
		}
		else
		{
			// According to the sclaer driver's requirement, if this is not seamless-resolution-change, set 0 to max w/h.
			if(outinfo){
				outinfo->u.dec_info.next_res.max_hres = 0;
				outinfo->u.dec_info.next_res.max_vres = 0;
			}
		}
		
		if(outinfo){
			outinfo->u.dec_info.next_res.crop_x = (gint)(xvimagesink->video_width * xvimagesink->disp_x_ratio);
			outinfo->u.dec_info.next_res.crop_y = (gint)(xvimagesink->video_height * xvimagesink->disp_y_ratio);
			outinfo->u.dec_info.next_res.crop_w = (gint)(xvimagesink->video_width * xvimagesink->disp_width_ratio);
			outinfo->u.dec_info.next_res.crop_h = (gint)(xvimagesink->video_height * xvimagesink->disp_height_ratio);
			if (outinfo->u.dec_info.pFrame[0].y_viraddr)
			{
			  gchar* ptr = outinfo->u.dec_info.pFrame[0].y_viraddr + ( outinfo->u.dec_info.pFrame[0].y_linesize * outinfo->u.dec_info.pFrame[0].height/2) + outinfo->u.dec_info.pFrame[0].width/2;
			  if(ptr){
			  	debug_info = *ptr;
			  }else {
			  	GST_WARNING_OBJECT (xvimagesink, "Get white value prt is NULL !!! ");
				debug_info = -1;
			  }
			}else{
			  debug_info = -1;
			}
		}

		const gchar* str3Dformat = gst_structure_get_string(gst_caps_get_structure( GST_BUFFER_CAPS(buf), 0 ), "3Dformat");
		if (str3Dformat)
		{
			if (g_str_has_prefix(str3Dformat, "MVC")){
				if(outinfo){
					outinfo->stereoscopic_info = V4L2_DRM_INPUT_STEREOSCOPIC_3D_MVC_B;
				}
			}else if (g_str_has_prefix(str3Dformat, "SVAF_2ES")){
				if(outinfo){
					outinfo->stereoscopic_info = V4L2_DRM_INPUT_STEREOSCOPIC_3D_SVAF;
				}
			}else if (g_str_has_prefix(str3Dformat, "FileName") || g_str_has_prefix(str3Dformat, "SVAF")){
				const gchar* str3Dmode = gst_structure_get_string(gst_caps_get_structure( GST_BUFFER_CAPS(buf), 0 ), "3Dmode");
				if (g_str_has_prefix(str3Dmode, "SideBySide")){
					if(outinfo){
						outinfo->stereoscopic_info = V4L2_DRM_INPUT_STEREOSCOPIC_SBS;
					}
				}else if (g_str_has_prefix(str3Dmode, "TopAndBottom")){
					if(outinfo){
						outinfo->stereoscopic_info = V4L2_DRM_INPUT_STEREOSCOPIC_TNB;
					}
				}
			}
		}
		if(xvimagesink->mode_3d)
		{
			if(xvimagesink->mode_3d == 1/*MM_PLAYER_3D_TYPE_SBS*/){
				if(outinfo){
					outinfo->stereoscopic_info = V4L2_DRM_INPUT_STEREOSCOPIC_SBS;
				}
			}
			if(xvimagesink->mode_3d == 2/*MM_PLAYER_3D_TYPE_TNB*/){
				if(outinfo){
					outinfo->stereoscopic_info = V4L2_DRM_INPUT_STEREOSCOPIC_TNB;
				}
			}
		}
		// Copy the HW Struecture info into Xvideo.
		memcpy (xvimagesink->xvimage->xvimage->data, GST_BUFFER_DATA (buf),
		                MIN (GST_BUFFER_SIZE (buf), xvimagesink->xvimage->size));

		if(outinfo){
			GST_LOG_OBJECT(xvimagesink, "Reset the display channel in HW display -> buf_size[%d], xvimage_size[%d]  3dtype[ %d ], max[%dx%d] display_idx[%d]",
				GST_BUFFER_SIZE (buf), xvimagesink->xvimage->size, outinfo->stereoscopic_info, outinfo->u.dec_info.next_res.max_hres, outinfo->u.dec_info.next_res.max_vres, frminfo_t->display_index);
		}else {
			GST_WARNING_OBJECT(xvimagesink, "v4l2_drm is NULL !!!! ");
		}
		
		break;
	}
#endif
  }
      default:
      {
        GST_DEBUG("Normal format activated. fourcc = %d", xvimagesink->xvimage->im_format);
        memcpy (xvimagesink->xvimage->xvimage->data,
        GST_BUFFER_DATA (buf),
        MIN (GST_BUFFER_SIZE (buf), xvimagesink->xvimage->size));
        break;
      }
    }

	//Apply Mute for HW Rotation 		
	if((xvimagesink->enable_hw_rotate_support && xvimagesink->is_hw_rotate_degree_changed) || (xvimagesink->is_hw_rotate_on_mixed_frame && xvimagesink->is_hw_rotate_degree_changed))
	{		
		gst_hw_rotate_apply_mute(xvimagesink);		
	}	

    GST_BUFFER_TIMESTAMP(xvimagesink->xvimage) = GST_BUFFER_TIMESTAMP(buf); // update for debugging
    g_mutex_unlock (xvimagesink->flow_lock);

    ret = gst_xvimagesink_xvimage_put(xvimagesink, xvimagesink->xvimage, GST_BUFFER_CAPS (buf), debug_info);
    if (need_unmute_after_rotation_putimage || xvimagesink->vf_force_unmute){
         mute_video_display(xvimagesink, FALSE, MUTE_ROTATION_CHANGE);
	  xvimagesink->vf_force_unmute = 0;
    }

	//Apply UnMute for HW Rotation 	
	if ((xvimagesink->enable_hw_rotate_support && xvimagesink->is_unmute_req_for_hw_rotate)||(xvimagesink->is_hw_rotate_on_mixed_frame && xvimagesink->is_unmute_req_for_hw_rotate))
	{
		gst_hw_rotate_apply_unmute(xvimagesink);		
	}

    /* close gem handle */
    if (xvimagesink->drm_fd >= 0 ) {
//      drm_close_gem(xvimagesink, &xvimagesink->gem_handle[0]);
//      drm_close_gem(xvimagesink, &xvimagesink->gem_handle[1]);
//      drm_close_gem(xvimagesink, &xvimagesink->gem_handle[2]);
    }

    if (!ret) {
      goto no_window;
    }
#else /* GST_EXT_XV_ENHANCEMENT */
    memcpy (xvimagesink->xvimage->xvimage->data,
        GST_BUFFER_DATA (buf),
        MIN (GST_BUFFER_SIZE (buf), xvimagesink->xvimage->size));


    if (!gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage, GST_BUFFER_CAPS (buf), 0))
      goto no_window;
#endif /* GST_EXT_XV_ENHANCEMENT */
  }

#if ENABLE_PERFORMANCE_CHECKING
  show_frame_count++;
  total_show_frame_time += get_time() - show_frame_time_tmp;
  if(show_frame_count == 10 || show_frame_count == 300 || show_frame_count == 800 || show_frame_count == 1000 || show_frame_count == 2000 || show_frame_count == 3000){
	GST_ERROR_OBJECT(xvimagesink, "***************************show frame performance test: *****************************************");
	GST_ERROR_OBJECT(xvimagesink, "show frame=%d, total_show_frame_time =%lldus, average show frame time=%lldus", show_frame_count, total_show_frame_time, total_show_frame_time/show_frame_count);
  }
  if(rotate_frame_count ==10 || rotate_frame_count ==300 || rotate_frame_count ==800 || rotate_frame_count ==1000 || rotate_frame_count == 3000){
	GST_ERROR_OBJECT(xvimagesink, "***************************rotateframe performance test: *****************************************");
	GST_ERROR_OBJECT(xvimagesink, "HW->AP: %lldus AP->MP: %lldus Total rotate time: %lldus ", rotate_hw_into_AP_time, rotate_AP_inot_MP_time, rotate_total_time);
  }
#endif

  return GST_FLOW_OK;

/*ROTATIONS*/
#if ENABLE_VF_ROTATE
display_channel_error:
  {
	GST_ERROR_OBJECT (xvimagesink, "Rotate: set video display channel failed !!!");
#ifdef GST_EXT_XV_ENHANCEMENT		
	g_mutex_unlock (xvimagesink->flow_lock);
#endif
	return GST_FLOW_ERROR;
  }
skip_frame:
  {
  	GST_WARNING_OBJECT (xvimagesink, "Rotate: video rotation failed, skip this frame !!!");
#ifdef GST_EXT_XV_ENHANCEMENT	
	g_mutex_unlock (xvimagesink->flow_lock);
#endif
	return GST_FLOW_OK;
  }
#endif
  /* ERRORS */
no_image:
  {
    /* No image available. That's very bad ! */
    GST_WARNING_OBJECT (xvimagesink, "could not create image");
#ifdef GST_EXT_XV_ENHANCEMENT
    g_mutex_unlock (xvimagesink->flow_lock);
#endif


    return GST_FLOW_ERROR;
  }
no_window:
  {
    /* No Window available to put our image into */
    GST_WARNING_OBJECT (xvimagesink, "could not output image - no window");


    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_xvimagesink_event (GstBaseSink * sink, GstEvent * event)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (sink);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:{
      GstTagList *l;
      gchar *title = NULL;

      gst_event_parse_tag (event, &l);
      gst_tag_list_get_string (l, GST_TAG_TITLE, &title);

      if (title) {
#ifdef GST_EXT_XV_ENHANCEMENT
        if (!xvimagesink->get_pixmap_cb) {
#endif
        GST_DEBUG_OBJECT (xvimagesink, "got tags, title='%s'", title);
        gst_xvimagesink_xwindow_set_title (xvimagesink, xvimagesink->xwindow,
            title);

        g_free (title);
#ifdef GST_EXT_XV_ENHANCEMENT
        }
#endif
      }
      break;
    }
    default:
      break;
  }
  if (GST_BASE_SINK_CLASS (parent_class)->event)
    return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
  else
    return TRUE;
}

/* Buffer management */

static GstCaps *
gst_xvimage_sink_different_size_suggestion (GstXvImageSink * xvimagesink,
    GstCaps * caps)
{
  GstCaps *intersection;
  GstCaps *new_caps;
  GstStructure *s;
  gint width, height;
  gint par_n = 1, par_d = 1;
  gint dar_n, dar_d;
  gint w, h;

  new_caps = gst_caps_copy (caps);

  s = gst_caps_get_structure (new_caps, 0);

  gst_structure_get_int (s, "width", &width);
  gst_structure_get_int (s, "height", &height);
  gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d);

  gst_structure_remove_field (s, "width");
  gst_structure_remove_field (s, "height");
  gst_structure_remove_field (s, "pixel-aspect-ratio");

  intersection = gst_caps_intersect (xvimagesink->xcontext->caps, new_caps);
  gst_caps_unref (new_caps);

  if (gst_caps_is_empty (intersection))
    return intersection;

  s = gst_caps_get_structure (intersection, 0);

  gst_util_fraction_multiply (width, height, par_n, par_d, &dar_n, &dar_d);

  /* xvimagesink supports all PARs */

  gst_structure_fixate_field_nearest_int (s, "width", width);
  gst_structure_fixate_field_nearest_int (s, "height", height);
  gst_structure_get_int (s, "width", &w);
  gst_structure_get_int (s, "height", &h);

  gst_util_fraction_multiply (h, w, dar_n, dar_d, &par_n, &par_d);
  gst_structure_set (s, "pixel-aspect-ratio", GST_TYPE_FRACTION, par_n, par_d,
      NULL);

  return intersection;
}

static GstFlowReturn
gst_xvimagesink_buffer_alloc (GstBaseSink * bsink, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstXvImageSink *xvimagesink;
  GstXvImageBuffer *xvimage = NULL;
  GstCaps *intersection = NULL;
  GstStructure *structure = NULL;
  gint width, height, image_format;

  xvimagesink = GST_XVIMAGESINK (bsink);

  if (G_UNLIKELY (!caps))
    goto no_caps;

  g_mutex_lock (xvimagesink->pool_lock);
  if (G_UNLIKELY (xvimagesink->pool_invalid))
    goto invalid;

  if (G_LIKELY (xvimagesink->xcontext->last_caps &&
          gst_caps_is_equal (caps, xvimagesink->xcontext->last_caps))) {
    GST_LOG_OBJECT (xvimagesink,
        "buffer alloc for same last_caps, reusing caps");
    intersection = gst_caps_ref (caps);
    image_format = xvimagesink->xcontext->last_format;
    width = xvimagesink->xcontext->last_width;
    height = xvimagesink->xcontext->last_height;

    goto reuse_last_caps;
  }

  GST_DEBUG_OBJECT (xvimagesink, "buffer alloc requested size %d with caps %"
      GST_PTR_FORMAT ", intersecting with our caps %" GST_PTR_FORMAT, size,
      caps, xvimagesink->xcontext->caps);

  /* Check the caps against our xcontext */
  intersection = gst_caps_intersect (xvimagesink->xcontext->caps, caps);

  GST_DEBUG_OBJECT (xvimagesink, "intersection in buffer alloc returned %"
      GST_PTR_FORMAT, intersection);

  if (gst_caps_is_empty (intersection)) {
    GstCaps *new_caps;

    gst_caps_unref (intersection);

    /* So we don't support this kind of buffer, let's define one we'd like */
    new_caps = gst_caps_copy (caps);

    structure = gst_caps_get_structure (new_caps, 0);
    if (!gst_structure_has_field (structure, "width") ||
        !gst_structure_has_field (structure, "height")) {
      gst_caps_unref (new_caps);
      goto invalid;
    }

    /* Try different dimensions */
    intersection =
        gst_xvimage_sink_different_size_suggestion (xvimagesink, new_caps);

    if (gst_caps_is_empty (intersection)) {
      /* Try with different YUV formats first */
      gst_structure_set_name (structure, "video/x-raw-yuv");

      /* Remove format specific fields */
      gst_structure_remove_field (structure, "format");
      gst_structure_remove_field (structure, "endianness");
      gst_structure_remove_field (structure, "depth");
      gst_structure_remove_field (structure, "bpp");
      gst_structure_remove_field (structure, "red_mask");
      gst_structure_remove_field (structure, "green_mask");
      gst_structure_remove_field (structure, "blue_mask");
      gst_structure_remove_field (structure, "alpha_mask");

      /* Reuse intersection with Xcontext */
      intersection = gst_caps_intersect (xvimagesink->xcontext->caps, new_caps);
    }

    if (gst_caps_is_empty (intersection)) {
      /* Try with different dimensions and YUV formats */
      intersection =
          gst_xvimage_sink_different_size_suggestion (xvimagesink, new_caps);
    }

    if (gst_caps_is_empty (intersection)) {
      /* Now try with RGB */
      gst_structure_set_name (structure, "video/x-raw-rgb");
      /* And interset again */
      gst_caps_unref (intersection);
      intersection = gst_caps_intersect (xvimagesink->xcontext->caps, new_caps);
    }

    if (gst_caps_is_empty (intersection)) {
      /* Try with different dimensions and RGB formats */
      intersection =
          gst_xvimage_sink_different_size_suggestion (xvimagesink, new_caps);
    }

    /* Clean this copy */
    gst_caps_unref (new_caps);

    if (gst_caps_is_empty (intersection))
      goto incompatible;
  }

  /* Ensure the returned caps are fixed */
  gst_caps_truncate (intersection);

  GST_DEBUG_OBJECT (xvimagesink, "allocating a buffer with caps %"
      GST_PTR_FORMAT, intersection);
  if (gst_caps_is_equal (intersection, caps)) {
    /* Things work better if we return a buffer with the same caps ptr
     * as was asked for when we can */
    gst_caps_replace (&intersection, caps);
  }

  /* Get image format from caps */
  image_format = gst_xvimagesink_get_format_from_caps (xvimagesink,
      intersection);

  /* Get geometry from caps */
  structure = gst_caps_get_structure (intersection, 0);
  if (!gst_structure_get_int (structure, "width", &width) ||
      !gst_structure_get_int (structure, "height", &height) ||
      image_format == -1)
    goto invalid_caps;

  /* Store our caps and format as the last_caps to avoid expensive
   * caps intersection next time */
  gst_caps_replace (&xvimagesink->xcontext->last_caps, intersection);
  xvimagesink->xcontext->last_format = image_format;
  xvimagesink->xcontext->last_width = width;
  xvimagesink->xcontext->last_height = height;

reuse_last_caps:

  /* Walking through the pool cleaning unusable images and searching for a
     suitable one */
  while (xvimagesink->image_pool) {
    xvimage = xvimagesink->image_pool->data;

    if (xvimage) {
      /* Removing from the pool */
      xvimagesink->image_pool = g_slist_delete_link (xvimagesink->image_pool,
          xvimagesink->image_pool);

      /* We check for geometry or image format changes */
      if ((xvimage->width != width) ||
          (xvimage->height != height) || (xvimage->im_format != image_format)) {
        /* This image is unusable. Destroying... */
        gst_xvimage_buffer_free (xvimage);
        xvimage = NULL;
      } else {
        /* We found a suitable image */
        GST_LOG_OBJECT (xvimagesink, "found usable image in pool");
        break;
      }
    }
  }

  if (!xvimage) {
    /* We found no suitable image in the pool. Creating... */
    GST_DEBUG_OBJECT (xvimagesink, "no usable image in pool, creating xvimage");
    xvimage = gst_xvimagesink_xvimage_new (xvimagesink, intersection);
  }
  g_mutex_unlock (xvimagesink->pool_lock);

  if (xvimage) {
    /* Make sure the buffer is cleared of any previously used flags */
    GST_MINI_OBJECT_CAST (xvimage)->flags = 0;
    gst_buffer_set_caps (GST_BUFFER_CAST (xvimage), intersection);
  }

  *buf = GST_BUFFER_CAST (xvimage);

beach:
  if (intersection) {
    gst_caps_unref (intersection);
  }

  return ret;

  /* ERRORS */
invalid:
  {
    GST_DEBUG_OBJECT (xvimagesink, "the pool is flushing");
    ret = GST_FLOW_WRONG_STATE;
    g_mutex_unlock (xvimagesink->pool_lock);
    goto beach;
  }
incompatible:
  {
    GST_WARNING_OBJECT (xvimagesink, "we were requested a buffer with "
        "caps %" GST_PTR_FORMAT ", but our xcontext caps %" GST_PTR_FORMAT
        " are completely incompatible with those caps", caps,
        xvimagesink->xcontext->caps);
    ret = GST_FLOW_NOT_NEGOTIATED;
    g_mutex_unlock (xvimagesink->pool_lock);
    goto beach;
  }
invalid_caps:
  {
    GST_WARNING_OBJECT (xvimagesink, "invalid caps for buffer allocation %"
        GST_PTR_FORMAT, intersection);
    ret = GST_FLOW_NOT_NEGOTIATED;
    g_mutex_unlock (xvimagesink->pool_lock);
    goto beach;
  }
no_caps:
  {
    GST_WARNING_OBJECT (xvimagesink, "have no caps, doing fallback allocation");
    *buf = NULL;
    ret = GST_FLOW_OK;
    goto beach;
  }
}

/* Interfaces stuff */

static gboolean
gst_xvimagesink_interface_supported (GstImplementsInterface * iface, GType type)
{
  if (type == GST_TYPE_NAVIGATION || type == GST_TYPE_X_OVERLAY ||
      type == GST_TYPE_COLOR_BALANCE || type == GST_TYPE_PROPERTY_PROBE)
    return TRUE;
  else
    return FALSE;
}

static void
gst_xvimagesink_interface_init (GstImplementsInterfaceClass * klass)
{
  klass->supported = gst_xvimagesink_interface_supported;
}

static void
gst_xvimagesink_navigation_send_event (GstNavigation * navigation,
    GstStructure * structure)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (navigation);
  GstPad *peer;

  if ((peer = gst_pad_get_peer (GST_VIDEO_SINK_PAD (xvimagesink)))) {
    GstEvent *event;
   // GstVideoRectangle src, dst, result;
    GstVideoRectangle src =  { 0, 0, 0, 0};
    GstVideoRectangle dst =  { 0, 0, 0, 0};
    GstVideoRectangle result = { 0, 0, 0, 0};
    gdouble x, y, xscale = 1.0, yscale = 1.0;

    event = gst_event_new_navigation (structure);

    /* We take the flow_lock while we look at the window */
    g_mutex_lock (xvimagesink->flow_lock);

    if (!xvimagesink->xwindow) {
      g_mutex_unlock (xvimagesink->flow_lock);
      return;
    }

    if (xvimagesink->keep_aspect) {
      /* We get the frame position using the calculated geometry from _setcaps
         that respect pixel aspect ratios */
      src.w = GST_VIDEO_SINK_WIDTH (xvimagesink);
      src.h = GST_VIDEO_SINK_HEIGHT (xvimagesink);
      dst.w = xvimagesink->render_rect.w;
      dst.h = xvimagesink->render_rect.h;

      gst_video_sink_center_rect (src, dst, &result, TRUE);
      result.x += xvimagesink->render_rect.x;
      result.y += xvimagesink->render_rect.y;
    } else {
      memcpy (&result, &xvimagesink->render_rect, sizeof (GstVideoRectangle));
    }

    g_mutex_unlock (xvimagesink->flow_lock);

    /* We calculate scaling using the original video frames geometry to include
       pixel aspect ratio scaling. */
    xscale = (gdouble) xvimagesink->video_width / result.w;
    yscale = (gdouble) xvimagesink->video_height / result.h;

    /* Converting pointer coordinates to the non scaled geometry */
    if (gst_structure_get_double (structure, "pointer_x", &x)) {
      x = MIN (x, result.x + result.w);
      x = MAX (x - result.x, 0);
      gst_structure_set (structure, "pointer_x", G_TYPE_DOUBLE,
          (gdouble) x * xscale, NULL);
    }
    if (gst_structure_get_double (structure, "pointer_y", &y)) {
      y = MIN (y, result.y + result.h);
      y = MAX (y - result.y, 0);
      gst_structure_set (structure, "pointer_y", G_TYPE_DOUBLE,
          (gdouble) y * yscale, NULL);
    }

    gst_pad_send_event (peer, event);
    gst_object_unref (peer);
  }
}

static void
gst_xvimagesink_navigation_init (GstNavigationInterface * iface)
{
  iface->send_event = gst_xvimagesink_navigation_send_event;
}

#ifdef GST_EXT_XV_ENHANCEMENT
static void
gst_xvimagesink_set_pixmap_handle (GstXOverlay * overlay, guintptr id)
{
  XID pixmap_id = id;
  int i = 0;
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (overlay);
  GstXPixmap *xpixmap = NULL;

  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  /* If the element has not initialized the X11 context try to do so */
  if (!xvimagesink->xcontext && !(xvimagesink->xcontext = gst_xvimagesink_xcontext_get (xvimagesink))) {
    /* we have thrown a GST_ELEMENT_ERROR now */
    return;
  }

  gst_xvimagesink_update_colorbalance (xvimagesink);

  GST_DEBUG_OBJECT( xvimagesink, "pixmap id : %d", pixmap_id );

  /* if the returned pixmap_id is 0, set current pixmap index to -2 to skip putImage() */
  if (pixmap_id == 0) {
    xvimagesink->current_pixmap_idx = -2;
    return;
  }

  g_mutex_lock (xvimagesink->x_lock);

  for (i = 0; i < MAX_PIXMAP_NUM; i++) {
    if (!xvimagesink->xpixmap[i]) {
      Window root_window;
      int cur_win_x = 0;
      int cur_win_y = 0;
      unsigned int cur_win_width = 0;
      unsigned int cur_win_height = 0;
      unsigned int cur_win_border_width = 0;
      unsigned int cur_win_depth = 0;

      GST_INFO_OBJECT( xvimagesink, "xpixmap[%d] is empty, create it with pixmap_id(%d)", i, pixmap_id );

      xpixmap = g_new0 (GstXPixmap, 1);
      if (xpixmap) {
        xpixmap->pixmap = pixmap_id;
        xvimagesink->set_pixmap = TRUE;

        /* Get root window and size of current window */
        XGetGeometry(xvimagesink->xcontext->disp, xpixmap->pixmap, &root_window,
                     &cur_win_x, &cur_win_y, /* relative x, y */
                     &cur_win_width, &cur_win_height,
                     &cur_win_border_width, &cur_win_depth);
        GST_INFO_OBJECT( xvimagesink, "cur_win_width(%d), cur_win_height(%d).", cur_win_width, cur_win_height );
        if (!cur_win_width || !cur_win_height) {
          GST_INFO_OBJECT( xvimagesink, "cur_win_width(%d) or cur_win_height(%d) is null..", cur_win_width, cur_win_height );
          g_mutex_unlock (xvimagesink->x_lock);
          return;
        }
        xpixmap->width = cur_win_width;
        xpixmap->height = cur_win_height;
				
        if (!xvimagesink->render_rect.w)
          xvimagesink->render_rect.w = cur_win_width;
        if (!xvimagesink->render_rect.h)
          xvimagesink->render_rect.h = cur_win_height;
        /* Create a GC */
        xpixmap->gc = XCreateGC (xvimagesink->xcontext->disp, xpixmap->pixmap, 0, NULL);

        xvimagesink->xpixmap[i] = xpixmap;
        xvimagesink->current_pixmap_idx = i;
      } else {
        GST_ERROR("failed to create xpixmap errno: %d", errno);
      }

      g_mutex_unlock (xvimagesink->x_lock);
      return;

    } else if (xvimagesink->xpixmap[i]->pixmap == pixmap_id) {
      GST_DEBUG_OBJECT( xvimagesink, "found xpixmap[%d]->pixmap : %d", i, pixmap_id );
      xvimagesink->current_pixmap_idx = i;

      g_mutex_unlock (xvimagesink->x_lock);
      return;

    } else {
      continue;
    }
  }

  GST_ERROR_OBJECT( xvimagesink, "could not find the pixmap id(%d) in xpixmap array", pixmap_id );
  xvimagesink->current_pixmap_idx = -1;

  g_mutex_unlock (xvimagesink->x_lock);
  return;
}
#endif /* GST_EXT_XV_ENHANCEMENT */

static void
gst_xvimagesink_set_window_handle (GstXOverlay * overlay, guintptr id)
{
  XID xwindow_id = id;
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (overlay);
  GstXWindow *xwindow = NULL;

  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));

  g_mutex_lock (xvimagesink->flow_lock);

#ifdef GST_EXT_XV_ENHANCEMENT
  GST_INFO_OBJECT( xvimagesink, "ENTER, id : %d", xwindow_id );
#endif /* GST_EXT_XV_ENHANCEMENT */

  /* If we already use that window return */
  if (xvimagesink->xwindow && (xwindow_id == xvimagesink->xwindow->win)) {
    g_mutex_unlock (xvimagesink->flow_lock);
    return;
  }

  /* If the element has not initialized the X11 context try to do so */
  if (!xvimagesink->xcontext &&
      !(xvimagesink->xcontext = gst_xvimagesink_xcontext_get (xvimagesink))) {
    g_mutex_unlock (xvimagesink->flow_lock);
    /* we have thrown a GST_ELEMENT_ERROR now */
    return;
  }

  gst_xvimagesink_update_colorbalance (xvimagesink);

  /* Clear image pool as the images are unusable anyway */
  gst_xvimagesink_imagepool_clear (xvimagesink);

  /* Clear the xvimage */
  if (xvimagesink->xvimage) {
    gst_xvimage_buffer_free (xvimagesink->xvimage);
    xvimagesink->xvimage = NULL;
  }

  /* If a window is there already we destroy it */
  if (xvimagesink->xwindow) {
    GST_ERROR_OBJECT(xvimagesink, "xwindow destory !!!"); 
    gst_xvimagesink_xwindow_destroy (xvimagesink, xvimagesink->xwindow);
    xvimagesink->xwindow = NULL;
    GST_ERROR_OBJECT(xvimagesink, "xwindow destory done !!!"); 
  }

  /* If the xid is 0 we go back to an internal window */
  if (xwindow_id == 0) {
    /* If no width/height caps nego did not happen window will be created
       during caps nego then */
#ifdef GST_EXT_XV_ENHANCEMENT
  GST_INFO_OBJECT( xvimagesink, "xid is 0. create window[%dx%d]",
    GST_VIDEO_SINK_WIDTH (xvimagesink), GST_VIDEO_SINK_HEIGHT (xvimagesink) );
#endif /* GST_EXT_XV_ENHANCEMENT */
    if (GST_VIDEO_SINK_WIDTH (xvimagesink)
        && GST_VIDEO_SINK_HEIGHT (xvimagesink)) {
      xwindow =
          gst_xvimagesink_xwindow_new (xvimagesink,
          GST_VIDEO_SINK_WIDTH (xvimagesink),
          GST_VIDEO_SINK_HEIGHT (xvimagesink));
    }
  } else {
    XWindowAttributes attr;

    xwindow = g_new0 (GstXWindow, 1);
    xwindow->win = xwindow_id;

    /* Set the event we want to receive and create a GC */
    g_mutex_lock (xvimagesink->x_lock);

    XGetWindowAttributes (xvimagesink->xcontext->disp, xwindow->win, &attr);

    xwindow->width = attr.width;
    xwindow->height = attr.height;
    xwindow->internal = FALSE;
    GST_ERROR_OBJECT(xvimagesink, "Get Window Attributes From APP, W[%d]/H[%d] ", xwindow->width,  xwindow->height);
    if (!xvimagesink->have_render_rect) {
      xvimagesink->render_rect.x = xvimagesink->render_rect.y = 0;
      xvimagesink->render_rect.w = attr.width;
      xvimagesink->render_rect.h = attr.height;
    }
    if (xvimagesink->handle_events) {
      XSelectInput (xvimagesink->xcontext->disp, xwindow->win, ExposureMask |
          StructureNotifyMask | PointerMotionMask | KeyPressMask |
          KeyReleaseMask);
      XSync(xvimagesink->xcontext->disp, FALSE);
    }

    xwindow->gc = XCreateGC (xvimagesink->xcontext->disp,
        xwindow->win, 0, NULL);
    g_mutex_unlock (xvimagesink->x_lock);
	/*
	** attr.map_state **
	IsUnmapped		0
	IsUnviewable		1
	IsViewable		2
	*/
	if((attr.map_state != 2) && (xvimagesink->is_hided == FALSE))
	{
		// set hide mask
		xvimagesink->is_hided = TRUE;
		mute_video_display(xvimagesink, TRUE, MUTE_STATE_HIDE);
	}else if((attr.map_state == 2) && (xvimagesink->is_hided == TRUE))
	{
		// set show mask
		xvimagesink->is_hided = FALSE;
		mute_video_display(xvimagesink, FALSE, MUTE_STATE_HIDE);
	}
  }

  if (xwindow)
    xvimagesink->xwindow = xwindow;

	if(xvimagesink->xwindow)
	{
		if((xvimagesink->xwindow->width > 1) && (xvimagesink->xwindow->height > 1))
		{
		  mute_video_display(xvimagesink, FALSE, MUTE_INVALID_WINSIZE);
		}
		else
		{
		  mute_video_display(xvimagesink, TRUE, MUTE_INVALID_WINSIZE);
		}
	}
#ifdef GST_EXT_XV_ENHANCEMENT
  xvimagesink->xid_updated = TRUE;
#endif /* GST_EXT_XV_ENHANCEMENT */

  g_mutex_unlock (xvimagesink->flow_lock);
  
  GstState current;
  gst_element_get_state (xvimagesink, &current, NULL, 0);
			
  if(current == GST_STATE_PAUSED)
  {
	gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage, NULL, 0);
  }
}

static void
gst_xvimagesink_expose (GstXOverlay * overlay)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (overlay);

  gst_xvimagesink_xwindow_update_geometry (xvimagesink);
#ifdef GST_EXT_XV_ENHANCEMENT
  GST_INFO_OBJECT( xvimagesink, "Overlay window exposed. update it");
  gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage, NULL, 0);
#else /* GST_EXT_XV_ENHANCEMENT */
  gst_xvimagesink_xvimage_put (xvimagesink, NULL, NULL, 0);
#endif /* GST_EXT_XV_ENHANCEMENT */
}

static void
gst_xvimagesink_set_event_handling (GstXOverlay * overlay,
    gboolean handle_events)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (overlay);

  xvimagesink->handle_events = handle_events;

  g_mutex_lock (xvimagesink->flow_lock);

  if (G_UNLIKELY (!xvimagesink->xwindow)) {
    g_mutex_unlock (xvimagesink->flow_lock);
    return;
  }

  g_mutex_lock (xvimagesink->x_lock);

  if (handle_events) {
    if (xvimagesink->xwindow->internal) {
      XSelectInput (xvimagesink->xcontext->disp, xvimagesink->xwindow->win,
#ifdef GST_EXT_XV_ENHANCEMENT
          ExposureMask | StructureNotifyMask | PointerMotionMask | VisibilityChangeMask |
#else /* GST_EXT_XV_ENHANCEMENT */
          ExposureMask | StructureNotifyMask | PointerMotionMask |
#endif /* GST_EXT_XV_ENHANCEMENT */
          KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask);
    } else {
      XSelectInput (xvimagesink->xcontext->disp, xvimagesink->xwindow->win,
#ifdef GST_EXT_XV_ENHANCEMENT
          ExposureMask | StructureNotifyMask | PointerMotionMask | VisibilityChangeMask |
#else /* GST_EXT_XV_ENHANCEMENT */
          ExposureMask | StructureNotifyMask | PointerMotionMask |
#endif /* GST_EXT_XV_ENHANCEMENT */
          KeyPressMask | KeyReleaseMask);
    }
  } else {
    XSelectInput (xvimagesink->xcontext->disp, xvimagesink->xwindow->win, 0);
  }

  g_mutex_unlock (xvimagesink->x_lock);

  g_mutex_unlock (xvimagesink->flow_lock);
}

static void
gst_xvimagesink_set_render_rectangle (GstXOverlay * overlay, gint x, gint y,
    gint width, gint height)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (overlay);

  /* FIXME: how about some locking? */
  if (width >= 0 && height >= 0) {
    xvimagesink->render_rect.x = x;
    xvimagesink->render_rect.y = y;
    xvimagesink->render_rect.w = width;
    xvimagesink->render_rect.h = height;
    xvimagesink->have_render_rect = TRUE;
  } else {
    xvimagesink->render_rect.x = 0;
    xvimagesink->render_rect.y = 0;
    xvimagesink->render_rect.w = xvimagesink->xwindow->width;
    xvimagesink->render_rect.h = xvimagesink->xwindow->height;
    xvimagesink->have_render_rect = FALSE;
  }
}

static void
gst_xvimagesink_xoverlay_init (GstXOverlayClass * iface)
{
  iface->set_window_handle = gst_xvimagesink_set_window_handle;
  iface->expose = gst_xvimagesink_expose;
  iface->handle_events = gst_xvimagesink_set_event_handling;
  iface->set_render_rectangle = gst_xvimagesink_set_render_rectangle;
}

static const GList *
gst_xvimagesink_colorbalance_list_channels (GstColorBalance * balance)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (balance);

  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), NULL);

  if (xvimagesink->xcontext)
    return xvimagesink->xcontext->channels_list;
  else
    return NULL;
}

static void
gst_xvimagesink_colorbalance_set_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel, gint value)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (balance);

  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));
  g_return_if_fail (channel->label != NULL);

  xvimagesink->cb_changed = TRUE;

  /* Normalize val to [-1000, 1000] */
  value = floor (0.5 + -1000 + 2000 * (value - channel->min_value) /
      (double) (channel->max_value - channel->min_value));

  if (g_ascii_strcasecmp (channel->label, "XV_HUE") == 0) {
    xvimagesink->hue = value;
  } else if (g_ascii_strcasecmp (channel->label, "XV_SATURATION") == 0) {
    xvimagesink->saturation = value;
  } else if (g_ascii_strcasecmp (channel->label, "XV_CONTRAST") == 0) {
    xvimagesink->contrast = value;
  } else if (g_ascii_strcasecmp (channel->label, "XV_BRIGHTNESS") == 0) {
    xvimagesink->brightness = value;
  } else {
    g_warning ("got an unknown channel %s", channel->label);
    return;
  }

  gst_xvimagesink_update_colorbalance (xvimagesink);
}

static gint
gst_xvimagesink_colorbalance_get_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (balance);
  gint value = 0;

  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), 0);
  g_return_val_if_fail (channel->label != NULL, 0);

  if (g_ascii_strcasecmp (channel->label, "XV_HUE") == 0) {
    value = xvimagesink->hue;
  } else if (g_ascii_strcasecmp (channel->label, "XV_SATURATION") == 0) {
    value = xvimagesink->saturation;
  } else if (g_ascii_strcasecmp (channel->label, "XV_CONTRAST") == 0) {
    value = xvimagesink->contrast;
  } else if (g_ascii_strcasecmp (channel->label, "XV_BRIGHTNESS") == 0) {
    value = xvimagesink->brightness;
  } else {
    g_warning ("got an unknown channel %s", channel->label);
  }

  /* Normalize val to [channel->min_value, channel->max_value] */
  value = channel->min_value + (channel->max_value - channel->min_value) *
      (value + 1000) / 2000;

  return value;
}

static void
gst_xvimagesink_colorbalance_init (GstColorBalanceClass * iface)
{
  GST_COLOR_BALANCE_TYPE (iface) = GST_COLOR_BALANCE_HARDWARE;
  iface->list_channels = gst_xvimagesink_colorbalance_list_channels;
  iface->set_value = gst_xvimagesink_colorbalance_set_value;
  iface->get_value = gst_xvimagesink_colorbalance_get_value;
}

static const GList *
gst_xvimagesink_probe_get_properties (GstPropertyProbe * probe)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (probe);
  static GList *list = NULL;

  if (!list) {
    list = g_list_append (NULL, g_object_class_find_property (klass, "device"));
    list =
        g_list_append (list, g_object_class_find_property (klass,
            "autopaint-colorkey"));
    list =
        g_list_append (list, g_object_class_find_property (klass,
            "double-buffer"));
    list =
        g_list_append (list, g_object_class_find_property (klass, "colorkey"));
  }

  return list;
}

static void
gst_xvimagesink_probe_probe_property (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (probe);

  switch (prop_id) {
    case PROP_DEVICE:
    case PROP_AUTOPAINT_COLORKEY:
    case PROP_DOUBLE_BUFFER:
    case PROP_COLORKEY:
      GST_DEBUG_OBJECT (xvimagesink,
          "probing device list and get capabilities");
      if (!xvimagesink->xcontext) {
        GST_DEBUG_OBJECT (xvimagesink, "generating xcontext");
        xvimagesink->xcontext = gst_xvimagesink_xcontext_get (xvimagesink);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }
}

static gboolean
gst_xvimagesink_probe_needs_probe (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (probe);
  gboolean ret = FALSE;

  switch (prop_id) {
    case PROP_DEVICE:
    case PROP_AUTOPAINT_COLORKEY:
    case PROP_DOUBLE_BUFFER:
    case PROP_COLORKEY:
      if (xvimagesink->xcontext != NULL) {
        ret = FALSE;
      } else {
        ret = TRUE;
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

  return ret;
}

static GValueArray *
gst_xvimagesink_probe_get_values (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (probe);
  GValueArray *array = NULL;

  if (G_UNLIKELY (!xvimagesink->xcontext)) {
    GST_WARNING_OBJECT (xvimagesink, "we don't have any xcontext, can't "
        "get values");
    goto beach;
  }

  switch (prop_id) {
    case PROP_DEVICE:
    {
      guint i;
      GValue value = { 0 };

      array = g_value_array_new (xvimagesink->xcontext->nb_adaptors);
      g_value_init (&value, G_TYPE_STRING);

      for (i = 0; i < xvimagesink->xcontext->nb_adaptors; i++) {
        gchar *adaptor_id_s = g_strdup_printf ("%u", i);

        g_value_set_string (&value, adaptor_id_s);
        g_value_array_append (array, &value);
        g_free (adaptor_id_s);
      }
      g_value_unset (&value);
      break;
    }
    case PROP_AUTOPAINT_COLORKEY:
      if (xvimagesink->have_autopaint_colorkey) {
        GValue value = { 0 };

        array = g_value_array_new (2);
        g_value_init (&value, G_TYPE_BOOLEAN);
        g_value_set_boolean (&value, FALSE);
        g_value_array_append (array, &value);
        g_value_set_boolean (&value, TRUE);
        g_value_array_append (array, &value);
        g_value_unset (&value);
      }
      break;
    case PROP_DOUBLE_BUFFER:
      if (xvimagesink->have_double_buffer) {
        GValue value = { 0 };

        array = g_value_array_new (2);
        g_value_init (&value, G_TYPE_BOOLEAN);
        g_value_set_boolean (&value, FALSE);
        g_value_array_append (array, &value);
        g_value_set_boolean (&value, TRUE);
        g_value_array_append (array, &value);
        g_value_unset (&value);
      }
      break;
    case PROP_COLORKEY:
      if (xvimagesink->have_colorkey) {
        GValue value = { 0 };

        array = g_value_array_new (1);
        g_value_init (&value, GST_TYPE_INT_RANGE);
        gst_value_set_int_range (&value, 0, 0xffffff);
        g_value_array_append (array, &value);
        g_value_unset (&value);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

beach:
  return array;
}

static void
gst_xvimagesink_property_probe_interface_init (GstPropertyProbeInterface *
    iface)
{
  iface->get_properties = gst_xvimagesink_probe_get_properties;
  iface->probe_property = gst_xvimagesink_probe_probe_property;
  iface->needs_probe = gst_xvimagesink_probe_needs_probe;
  iface->get_values = gst_xvimagesink_probe_get_values;
}

/* =========================================== */
/*                                             */
/*              Init & Class init              */
/*                                             */
/* =========================================== */

static void
gst_xvimagesink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstXvImageSink *xvimagesink;

  g_return_if_fail (GST_IS_XVIMAGESINK (object));

  xvimagesink = GST_XVIMAGESINK (object);

  switch (prop_id) {
    case PROP_HUE:
      xvimagesink->hue = g_value_get_int (value);
      xvimagesink->cb_changed = TRUE;
      gst_xvimagesink_update_colorbalance (xvimagesink);
      break;
    case PROP_CONTRAST:
      xvimagesink->contrast = g_value_get_int (value);
      xvimagesink->cb_changed = TRUE;
      gst_xvimagesink_update_colorbalance (xvimagesink);
      break;
    case PROP_BRIGHTNESS:
      xvimagesink->brightness = g_value_get_int (value);
      xvimagesink->cb_changed = TRUE;
      gst_xvimagesink_update_colorbalance (xvimagesink);
      break;
    case PROP_SATURATION:
      xvimagesink->saturation = g_value_get_int (value);
      xvimagesink->cb_changed = TRUE;
      gst_xvimagesink_update_colorbalance (xvimagesink);
      break;
    case PROP_DISPLAY:
      xvimagesink->display_name = g_strdup (g_value_get_string (value));
      break;
    case PROP_SYNCHRONOUS:
      xvimagesink->synchronous = g_value_get_boolean (value);
      if (xvimagesink->xcontext) {
        XSynchronize (xvimagesink->xcontext->disp, xvimagesink->synchronous);
        GST_DEBUG_OBJECT (xvimagesink, "XSynchronize called with %s",
            xvimagesink->synchronous ? "TRUE" : "FALSE");
      }
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      g_free (xvimagesink->par);
      xvimagesink->par = g_new0 (GValue, 1);
      g_value_init (xvimagesink->par, GST_TYPE_FRACTION);
      if (!g_value_transform (value, xvimagesink->par)) {
        g_warning ("Could not transform string to aspect ratio");
        gst_value_set_fraction (xvimagesink->par, 1, 1);
      }
      GST_DEBUG_OBJECT (xvimagesink, "set PAR to %d/%d",
          gst_value_get_fraction_numerator (xvimagesink->par),
          gst_value_get_fraction_denominator (xvimagesink->par));
      break;
    case PROP_FORCE_ASPECT_RATIO:
      xvimagesink->keep_aspect = g_value_get_boolean (value);
      break;
    case PROP_HANDLE_EVENTS:
      gst_xvimagesink_set_event_handling (GST_X_OVERLAY (xvimagesink),
          g_value_get_boolean (value));
      gst_xvimagesink_manage_event_thread (xvimagesink);
      break;
    case PROP_DEVICE:
      xvimagesink->adaptor_no = atoi (g_value_get_string (value));
      break;
    case PROP_DEVICE_ATTR_SCALER:
      xvimagesink->scaler_id = g_value_get_int(value);
      break;
    case PROP_HANDLE_EXPOSE:
      xvimagesink->handle_expose = g_value_get_boolean (value);
      gst_xvimagesink_manage_event_thread (xvimagesink);
      break;
    case PROP_DOUBLE_BUFFER:
      xvimagesink->double_buffer = g_value_get_boolean (value);
      break;
    case PROP_AUTOPAINT_COLORKEY:
      xvimagesink->autopaint_colorkey = g_value_get_boolean (value);
      break;
    case PROP_COLORKEY:
      xvimagesink->colorkey = g_value_get_int (value);
      break;
    case PROP_DRAW_BORDERS:
      xvimagesink->draw_borders = g_value_get_boolean (value);
      break;
	  
      /*video cropping*/
    case PROP_DISPLAY_SRC_X:
      xvimagesink->disp_x = g_value_get_int (value);
      break;
    case PROP_DISPLAY_SRC_Y:
      xvimagesink->disp_y = g_value_get_int (value);	 
      break;
    case PROP_DISPLAY_SRC_W:
      xvimagesink->disp_width = g_value_get_int (value);	  
      break;
    case PROP_DISPLAY_SRC_H:
      xvimagesink->disp_height = g_value_get_int (value);
      if((xvimagesink->disp_x == 0) &&
		(xvimagesink->disp_y == 0) &&
		(xvimagesink->video_width <= xvimagesink->disp_width) &&
		(xvimagesink->video_height <= xvimagesink->disp_height))
      {
		xvimagesink->crop_flag = FALSE;
		GST_ERROR_OBJECT(xvimagesink, "crop_flag FALSE");
      }
      else
      {
		xvimagesink->crop_flag = TRUE;
		GST_ERROR_OBJECT(xvimagesink, "crop_flag TRUE");
      }
      break;
	  
    case PROP_DISPLAY_SRC_X_RATIO:
      xvimagesink->disp_x_ratio= g_value_get_float (value);
      xvimagesink->disp_x = (gint)(xvimagesink->video_width * xvimagesink->disp_x_ratio);
	  if(xvimagesink->disp_x_ratio > 0)
	  {
	  	  xvimagesink->display_geometry_method = DISP_GEO_METHOD_CROPPED_FULL_SCREEN;
	  }
      break;
    case PROP_DISPLAY_SRC_Y_RATIO:
      xvimagesink->disp_y_ratio= g_value_get_float (value);
      xvimagesink->disp_y = (gint)(xvimagesink->video_height * xvimagesink->disp_y_ratio);
	  if(xvimagesink->disp_y_ratio > 0)
	  {
	  	  xvimagesink->display_geometry_method = DISP_GEO_METHOD_CROPPED_FULL_SCREEN;
	  }
      break;
    case PROP_DISPLAY_SRC_W_RATIO:
      xvimagesink->disp_width_ratio= g_value_get_float (value);
      xvimagesink->disp_width = (gint)(xvimagesink->video_width * xvimagesink->disp_width_ratio);
	  if(xvimagesink->disp_width_ratio < 1)
	  {
	  	  xvimagesink->display_geometry_method = DISP_GEO_METHOD_CROPPED_FULL_SCREEN;
	  }
      break;
    case PROP_DISPLAY_SRC_H_RATIO:
      xvimagesink->disp_height_ratio= g_value_get_float (value);
      xvimagesink->disp_height = (gint)(xvimagesink->video_height * xvimagesink->disp_height_ratio);
	  if(xvimagesink->disp_height_ratio < 1)
	  {
	  	  xvimagesink->display_geometry_method = DISP_GEO_METHOD_CROPPED_FULL_SCREEN;
	  }
      break;

#ifdef GST_EXT_XV_ENHANCEMENT
    case PROP_ORIENTATION:
    {
      xvimagesink->orientation = g_value_get_int (value);
      break;
    }
    case PROP_DISPLAY_MODE:
    {
      int set_mode = g_value_get_enum (value);

      g_mutex_lock(xvimagesink->flow_lock);
      g_mutex_lock(xvimagesink->x_lock);

      if (xvimagesink->display_mode != set_mode) {
        if (xvimagesink->xcontext) {
          /* set display mode */
          if (set_display_mode(xvimagesink->xcontext, set_mode)) {
            xvimagesink->display_mode = set_mode;
          } else {
            GST_WARNING_OBJECT(xvimagesink, "display mode[%d] set failed.", set_mode);
          }
        } else {
          /* "xcontext" is not created yet. It will be applied when xcontext is created. */
          GST_INFO_OBJECT(xvimagesink, "xcontext is NULL. display-mode will be set later.");
          xvimagesink->display_mode = set_mode;
        }
      } else {
        GST_INFO_OBJECT(xvimagesink, "skip display mode %d, because current mode is same", set_mode);
      }

      g_mutex_unlock(xvimagesink->x_lock);
      g_mutex_unlock(xvimagesink->flow_lock);
    }
      break;
    case PROP_DISPLAY_GEOMETRY_METHOD:
      xvimagesink->display_geometry_method = g_value_get_enum (value);
      GST_INFO_OBJECT(xvimagesink,"Overlay geometry[ %d ] changed. update it", xvimagesink->display_geometry_method);
      if(xvimagesink->xvimage){	  	
        GST_INFO_OBJECT(xvimagesink,"Overlay geometry[ %d ] changed.call gst_getRectSize",xvimagesink->display_geometry_method);
        gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
        GST_INFO_OBJECT(xvimagesink,"Overlay geometry[ %d ] changed. update it now", xvimagesink->display_geometry_method);
        gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage,GST_BUFFER_CAPS(&(xvimagesink->xvimage->buffer)), 0);
      }
      break;
    case PROP_FLIP:
      xvimagesink->flip = g_value_get_enum(value);
      set_flip_mode(xvimagesink, xvimagesink->flip);
      break;
    case PROP_SUPPORT_ROTATION:
      xvimagesink->support_rotation = g_value_get_boolean(value);
      break;
    case PROP_ROTATE_ANGLE:
	if(xvimagesink->support_rotation){
	      xvimagesink->rotate_setting_angle = g_value_get_enum (value);
#if ENABLE_VF_ROTATE
		switch(xvimagesink->rotate_setting_angle){
			case DEGREE_0:
				xvimagesink->vf_rotate_setting_degree = 0;
				break;
			case DEGREE_90:
				xvimagesink->vf_rotate_setting_degree = 90;
				break;
			case DEGREE_180:
				xvimagesink->vf_rotate_setting_degree = 180;
				break;
			case DEGREE_270:
				xvimagesink->vf_rotate_setting_degree = 270;
				break;
			case DEGREE_NUM:
				xvimagesink->vf_rotate_setting_degree = xvimagesink->rotate_setting_angle;
				break;
			default:
				xvimagesink->vf_rotate_setting_degree = 0;
				break;
		}
		GST_DEBUG_OBJECT(xvimagesink, "Rotate: received the vf_rotate_degree[%d] , vf_current_degree[%d]  !!!", xvimagesink->vf_rotate_setting_degree, xvimagesink->vf_current_degree);
#endif
	}
      break;
    case PROP_ENABEL_ROTATION:
	xvimagesink->user_enable_rotation = g_value_get_boolean(value);
	GST_WARNING_OBJECT(xvimagesink, "Rotate: APP already enable the video rotate !!!", xvimagesink->user_enable_rotation);
	break;
    case PROP_VISIBLE:
      g_mutex_lock( xvimagesink->flow_lock );
      g_mutex_lock( xvimagesink->x_lock );
      if (xvimagesink->visible && (g_value_get_boolean(value) == FALSE)) {
        xvimagesink->visible = g_value_get_boolean (value);
        mute_video_display(xvimagesink, TRUE, MUTE_VISIBLE);
      } else if (!xvimagesink->visible && (g_value_get_boolean(value) == TRUE)) {
        /* No need to do putimage again,  now the visible property will be executed by mute control. */
        xvimagesink->visible = g_value_get_boolean (value);
        mute_video_display(xvimagesink, FALSE, MUTE_VISIBLE);
        GST_INFO_OBJECT( xvimagesink, "Set visible as TRUE. Update it." );
      }
      g_mutex_unlock( xvimagesink->x_lock );
      g_mutex_unlock( xvimagesink->flow_lock );
      break;
    case PROP_ZOOM:
      xvimagesink->zoom = g_value_get_float (value);
      break;
    case PROP_DST_ROI_X:
      xvimagesink->dst_roi.x = g_value_get_int (value);
      break;
    case PROP_DST_ROI_Y:
      xvimagesink->dst_roi.y = g_value_get_int (value);
      break;
    case PROP_DST_ROI_W:
      xvimagesink->dst_roi.w = g_value_get_int (value);
      break;
    case PROP_DST_ROI_H:
      xvimagesink->dst_roi.h = g_value_get_int (value);
      break;
    case PROP_STOP_VIDEO:
      xvimagesink->stop_video = g_value_get_int (value);
      g_mutex_lock( xvimagesink->flow_lock );
      GST_ERROR_OBJECT( xvimagesink, "why the user set this property? xvimagesink->stop_video[ %d ]", xvimagesink->stop_video);
      if( xvimagesink->stop_video )
      {
        if ( xvimagesink->get_pixmap_cb ) {
          if (xvimagesink->xpixmap[0] && xvimagesink->xpixmap[0]->pixmap) {
            g_mutex_lock (xvimagesink->x_lock);
            GST_INFO_OBJECT( xvimagesink, "calling XvStopVideo()" );
            XvStopVideo (xvimagesink->xcontext->disp, xvimagesink->xcontext->xv_port_id, xvimagesink->xpixmap[0]->pixmap);
            g_mutex_unlock (xvimagesink->x_lock);
          }
        } else {
          GST_INFO_OBJECT( xvimagesink, "Xwindow CLEAR when set video-stop property" );
          gst_xvimagesink_xwindow_clear (xvimagesink, xvimagesink->xwindow);
        }
      }

      g_mutex_unlock( xvimagesink->flow_lock );
      break;
    case PROP_PIXMAP_CB:
    {
      void *cb_func;
      cb_func = g_value_get_pointer(value);
#ifdef XV_WEBKIT_PIXMAP_SUPPORT
      GST_INFO_OBJECT (xvimagesink, "Set callback for getting pixmap id, old callback: (0x%x), new callback: (0x%x)",
        xvimagesink->get_pixmap_cb, cb_func);
      xvimagesink->old_get_pixmap_cb = xvimagesink->get_pixmap_cb;
      xvimagesink->get_pixmap_cb = cb_func;
#else
      if (cb_func) {
        xvimagesink->get_pixmap_cb = cb_func;
        GST_INFO_OBJECT (xvimagesink, "Set callback(0x%x) for getting pixmap id", xvimagesink->get_pixmap_cb);
      }
#endif
      break;
    }
    case PROP_PIXMAP_CB_USER_DATA:
    {
      void *user_data;
      user_data = g_value_get_pointer(value);
      if (user_data) {
        xvimagesink->get_pixmap_cb_user_data = user_data;
        GST_INFO_OBJECT (xvimagesink, "Set user data(0x%x) for getting pixmap id", xvimagesink->get_pixmap_cb_user_data);
      }
      break;
    }
#endif /* GST_EXT_XV_ENHANCEMENT */
#if ENABLE_RT_DISPLAY
    case PROP_ES_DISPLAY:
	xvimagesink->rt_display_vaule = g_value_get_int(value);
	GST_LOG_OBJECT(xvimagesink, "RT: ES Display Value[%d] ", xvimagesink->rt_display_vaule);
	break;
#endif	// ENABLE_RT_DISPLAY
    case PROP_DEVICE_ID:
      xvimagesink->device_id = g_value_get_int(value);
      GST_LOG_OBJECT(xvimagesink, "device-id[%d]", xvimagesink->device_id);
      break;
    case PROP_DISPLAY_ZOOM_Y:
    	GST_ERROR_OBJECT(xvimagesink, "zoom_y[%d] value[%d]", xvimagesink->zoom_y, g_value_get_int(value));
    	 if(g_value_get_int(value) == PSIZE_RESET){
    		xvimagesink->zoom_y = PSIZE_RESET;
    	}
    	else{
    		xvimagesink->zoom_y = xvimagesink->zoom_y + g_value_get_int(value);
    	}
    		GST_INFO_OBJECT(xvimagesink, "zoom_y[%d]", xvimagesink->zoom_y);
    		if(xvimagesink->xvimage){
    			gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
    			gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage,GST_BUFFER_CAPS(&(xvimagesink->xvimage->buffer)), 0);
	      }
      break;
    case PROP_DISPLAY_ZOOM_H:
    	  GST_ERROR_OBJECT(xvimagesink, "zoom_h[%d] value[%d]", xvimagesink->zoom_h, g_value_get_int(value));
    	 if(g_value_get_int(value) == PSIZE_RESET){
    		xvimagesink->zoom_h = PSIZE_RESET;
    	}
    	else{
    		xvimagesink->zoom_h = xvimagesink->zoom_h + g_value_get_int(value);
    	}
    		GST_INFO_OBJECT(xvimagesink, "zoom_h[%d]", xvimagesink->zoom_h);
    		if(xvimagesink->xvimage){
    			gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
    			gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage,GST_BUFFER_CAPS(&(xvimagesink->xvimage->buffer)), 0);
	      }
      break;
    case PROP_DISPLAY_CUSTOM_X:
    	GST_ERROR_OBJECT(xvimagesink, "custom_x[%d] value[%d]", xvimagesink->custom_x, g_value_get_int(value));
    	 if(g_value_get_int(value) == PSIZE_RESET){
    		xvimagesink->custom_x = PSIZE_RESET;
    	}
    	else{
    		xvimagesink->custom_x = xvimagesink->custom_x + g_value_get_int(value);
    	}
    		GST_INFO_OBJECT(xvimagesink, "custom_x[%d]", xvimagesink->custom_x);
    		if(xvimagesink->xvimage){
    			gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
    			gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage,GST_BUFFER_CAPS(&(xvimagesink->xvimage->buffer)), 0);
	      }
      break;
    case PROP_DISPLAY_CUSTOM_Y:
    	GST_ERROR_OBJECT(xvimagesink, "custom_y[%d] value[%d]", xvimagesink->custom_y, g_value_get_int(value));
    	 if(g_value_get_int(value) == PSIZE_RESET){
    		xvimagesink->custom_y = PSIZE_RESET;
    	}
    	else{
    		xvimagesink->custom_y = xvimagesink->custom_y + g_value_get_int(value);
    	}
    		GST_INFO_OBJECT(xvimagesink, "custom_y[%d]", xvimagesink->custom_y);
    		if(xvimagesink->xvimage){
    			gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
    			gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage,GST_BUFFER_CAPS(&(xvimagesink->xvimage->buffer)), 0);
	      }
      break;
    case PROP_DISPLAY_CUSTOM_W:
    	GST_ERROR_OBJECT(xvimagesink, "custom_w[%d] value[%d]", xvimagesink->custom_w, g_value_get_int(value));
    	 if(g_value_get_int(value) == PSIZE_RESET){
    		xvimagesink->custom_w = PSIZE_RESET;
    	}
    	else{
    		xvimagesink->custom_w = xvimagesink->custom_w + g_value_get_int(value);
    	}
    		GST_INFO_OBJECT(xvimagesink, "custom_w[%d]", xvimagesink->custom_w);
    		if(xvimagesink->xvimage){
    			gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
    			gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage,GST_BUFFER_CAPS(&(xvimagesink->xvimage->buffer)), 0);
	      }
      break;
    case PROP_DISPLAY_CUSTOM_H:
    	GST_ERROR_OBJECT(xvimagesink, "custom_h[%d] value[%d]", xvimagesink->custom_h, g_value_get_int(value));
    	if(g_value_get_int(value) == PSIZE_RESET)	{
    		xvimagesink->custom_h = PSIZE_RESET;
    	}
    	else{
    		xvimagesink->custom_h = xvimagesink->custom_h + g_value_get_int(value);
    	}
    		GST_INFO_OBJECT(xvimagesink, "custom_h[%d]", xvimagesink->custom_h);
    		if(xvimagesink->xvimage){
    			gst_getRectSize(xvimagesink->display_geometry_method,xvimagesink);
    			gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage,GST_BUFFER_CAPS(&(xvimagesink->xvimage->buffer)), 0);
	      }
      break;
    case PROP_DISPLAY_NTEFLIX_PAR_X:
	xvimagesink->netflix_display_par_x = g_value_get_int(value);
	GST_DEBUG_OBJECT(xvimagesink, "Netflix: Get display par x [%d]", xvimagesink->netflix_display_par_x);
	break;
    case PROP_DISPLAY_NTEFLIX_PAR_Y:
		xvimagesink->netflix_display_par_y = g_value_get_int(value);
		GST_DEBUG_OBJECT(xvimagesink, "Netflix: Get display par y [%d]", xvimagesink->netflix_display_par_y);
		break;
	case PROP_DISPLAY_DPS_WIDTH:
		xvimagesink->dps_display_width = g_value_get_int(value);
		GST_DEBUG_OBJECT(xvimagesink, "Divx: Get display Width [%d]", xvimagesink->dps_display_width);
		break;
	case PROP_DISPLAY_DPS_HEIGHT:
		xvimagesink->dps_display_height = g_value_get_int(value);
		GST_DEBUG_OBJECT(xvimagesink, "Divx: Get display Height [%d]", xvimagesink->dps_display_height);
		break;
    case PROP_DISPLAY_UHD_FIT:
		xvimagesink->is_uhd_fit = g_value_get_boolean(value);
		GST_DEBUG_OBJECT(xvimagesink, "4K: Set UHD Fit [%d]", xvimagesink->is_uhd_fit);
		if(xvimagesink->is_uhd_fit){
			if((xvimagesink->video_width != 4096) || (xvimagesink->video_height != 2160)){
				GST_WARNING_OBJECT(xvimagesink, "4K: Set UHD Fit On failed W/H = [%d]/[%d]", xvimagesink->video_width, xvimagesink->video_height);
				xvimagesink->is_uhd_fit = FALSE;
			}
		}
		break;
    case PROP_SEAMLESS_RESOLUTION_CHANGE:
      if (GST_STATE(xvimagesink) == GST_STATE_NULL || GST_STATE(xvimagesink) == GST_STATE_READY)
      {
        xvimagesink->seamless_resolution_change = g_value_get_boolean(value);
      }
      else
      {
	  GST_ERROR_OBJECT(xvimagesink, "can not set[ %d -> %d ] now, because of the state[ %d ]",
	      xvimagesink->seamless_resolution_change, g_value_get_boolean(value), GST_STATE(xvimagesink));
      }
      break;
    case PROP_VIDEO_QUALITY_MODE:
      if (GST_STATE(xvimagesink) == GST_STATE_NULL || GST_STATE(xvimagesink) == GST_STATE_READY)
      {
        xvimagesink->video_quality_mode = g_value_get_int(value);
      }
      else
      {
	  GST_ERROR_OBJECT(xvimagesink, "can not set[ %d -> %d ] now, because of the state[ %d ]",
	      xvimagesink->video_quality_mode, g_value_get_int(value), GST_STATE(xvimagesink));
      }
      break;
    case PROP_VIDEO_AVOC_SOURCE:
      {
        gint prev_source = xvimagesink->avoc_source;
        xvimagesink->avoc_source = g_value_get_int(value);
        GST_ERROR_OBJECT(xvimagesink, "avoc_source[ old: 0x%x  ->  new: 0x%x ] ",prev_source, xvimagesink->avoc_source);
      }
      break;
    case PROP_3D_MODE:
			GST_ERROR_OBJECT(xvimagesink, "3d mode[ %d ]",g_value_get_int(value));
			xvimagesink->mode_3d = g_value_get_int(value);
			break;
	case PROP_HW_ROTATE_DEGREE: 
	{
		xvimagesink->set_hw_rotate_degree = g_value_get_int(value); 	
		GST_DEBUG_OBJECT(xvimagesink, "Set_Hw_Rotation_Degree[%d]",xvimagesink->set_hw_rotate_degree);		
		gst_set_hw_video_rotate_degree(xvimagesink);
	}
	break;
	case PROP_ENABLE_HW_ROTATION:
	{
		xvimagesink->enable_hw_rotate_support = g_value_get_boolean(value);
		GST_DEBUG_OBJECT(xvimagesink, "Enable_Hw_Rotation_Support[%d]",xvimagesink->enable_hw_rotate_support);	
		if (xvimagesink->enable_hw_rotate_support == FALSE)
		{
			gst_reset_hw_rotate_context(xvimagesink);
		}
	}
	break;
	case PROP_MIXER_ROTATE_DEGREE:
	{
		if (xvimagesink->is_player_mixer_support_enabled == TRUE)
		{
			xvimagesink->is_hw_rotate_on_mixed_frame = TRUE;
			xvimagesink->set_hw_rotate_degree = g_value_get_int(value); 
			GST_DEBUG_OBJECT(xvimagesink, "Set_Rotation_Degree_On_MixedFrame[%d]",xvimagesink->set_hw_rotate_degree);					
			gst_set_hw_video_rotate_degree_on_mixedframe(xvimagesink);
		}
		else
		{
			GST_DEBUG_OBJECT(xvimagesink, "!!!!!! ERROR SetPositionOnMixedFrame Is NOT CALLED  is_player_mixer_support_enabled[%d] !!!!!!",xvimagesink->is_player_mixer_support_enabled);	
		}
	}
	break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_xvimagesink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstXvImageSink *xvimagesink;

  g_return_if_fail (GST_IS_XVIMAGESINK (object));

  xvimagesink = GST_XVIMAGESINK (object);

  switch (prop_id) {
    case PROP_HUE:
      g_value_set_int (value, xvimagesink->hue);
      break;
    case PROP_CONTRAST:
      g_value_set_int (value, xvimagesink->contrast);
      break;
    case PROP_BRIGHTNESS:
      g_value_set_int (value, xvimagesink->brightness);
      break;
    case PROP_SATURATION:
      g_value_set_int (value, xvimagesink->saturation);
      break;
    case PROP_DISPLAY:
      g_value_set_string (value, xvimagesink->display_name);
      break;
    case PROP_SYNCHRONOUS:
      g_value_set_boolean (value, xvimagesink->synchronous);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      if (xvimagesink->par)
        g_value_transform (xvimagesink->par, value);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, xvimagesink->keep_aspect);
      break;
    case PROP_HANDLE_EVENTS:
      g_value_set_boolean (value, xvimagesink->handle_events);
      break;
    case PROP_DEVICE:
    {
      char *adaptor_no_s = g_strdup_printf ("%u", xvimagesink->adaptor_no);

      g_value_set_string (value, adaptor_no_s);
      g_free (adaptor_no_s);
      break;
    }
    case PROP_DEVICE_NAME:
      if (xvimagesink->xcontext && xvimagesink->xcontext->adaptors) {
        g_value_set_string (value,
            xvimagesink->xcontext->adaptors[xvimagesink->adaptor_no]);
      } else {
        g_value_set_string (value, NULL);
      }
      break;
    case PROP_DEVICE_ATTR_SCALER:
      g_value_set_int (value, xvimagesink->scaler_id);
      break;
    case PROP_HANDLE_EXPOSE:
      g_value_set_boolean (value, xvimagesink->handle_expose);
      break;
    case PROP_DOUBLE_BUFFER:
      g_value_set_boolean (value, xvimagesink->double_buffer);
      break;
    case PROP_AUTOPAINT_COLORKEY:
      g_value_set_boolean (value, xvimagesink->autopaint_colorkey);
      break;
    case PROP_COLORKEY:
      g_value_set_int (value, xvimagesink->colorkey);
      break;
    case PROP_DRAW_BORDERS:
      g_value_set_boolean (value, xvimagesink->draw_borders);
      break;
    case PROP_WINDOW_WIDTH:
      if (xvimagesink->xwindow)
        g_value_set_int (value, xvimagesink->xwindow->width);
      else
        g_value_set_int (value, 0);
      break;
    case PROP_WINDOW_HEIGHT:
      if (xvimagesink->xwindow)
        g_value_set_int (value, xvimagesink->xwindow->height);
      else
        g_value_set_int (value, 0);
      break;
#ifdef GST_EXT_XV_ENHANCEMENT
    case PROP_ORIENTATION:
      g_value_set_int (value, xvimagesink->orientation);
      break;
    case PROP_DISPLAY_MODE:
      g_value_set_enum (value, xvimagesink->display_mode);
      break;
    case PROP_DISPLAY_GEOMETRY_METHOD:
      g_value_set_enum (value, xvimagesink->display_geometry_method);
      break;
    case PROP_FLIP:
      g_value_set_enum(value, xvimagesink->flip);
      break;
    case PROP_SUPPORT_ROTATION:
      g_value_set_boolean (value, xvimagesink->support_rotation);
      break;
    case PROP_ROTATE_ANGLE:
      g_value_set_enum (value, xvimagesink->rotate_angle);
      break;
    case PROP_ENABEL_ROTATION:
	g_value_set_enum (value, xvimagesink->user_enable_rotation);
	break;	  
    case PROP_VISIBLE:
      g_value_set_boolean (value, xvimagesink->visible);
      break;
    case PROP_ZOOM:
      g_value_set_float (value, xvimagesink->zoom);
      break;
    case PROP_DST_ROI_X:
      g_value_set_int (value, xvimagesink->dst_roi.x);
      break;
    case PROP_DST_ROI_Y:
      g_value_set_int (value, xvimagesink->dst_roi.y);
      break;
    case PROP_DST_ROI_W:
      g_value_set_int (value, xvimagesink->dst_roi.w);
      break;
    case PROP_DST_ROI_H:
      g_value_set_int (value, xvimagesink->dst_roi.h);
      break;
    case PROP_STOP_VIDEO:
      g_value_set_int (value, xvimagesink->stop_video);
      break;
    case PROP_PIXMAP_CB:
      g_value_set_pointer (value, xvimagesink->get_pixmap_cb);
      break;
    case PROP_PIXMAP_CB_USER_DATA:
      g_value_set_pointer (value, xvimagesink->get_pixmap_cb_user_data);
      break;
    case PROP_DISPLAY_SRC_X:
      g_value_set_int (value, xvimagesink->disp_x);
      break;
    case PROP_DISPLAY_SRC_Y:
      g_value_set_int (value, xvimagesink->disp_y);
      break;
    case PROP_DISPLAY_SRC_W:
      g_value_set_int (value, xvimagesink->disp_width);
      break;
    case PROP_DISPLAY_SRC_H:
      g_value_set_int (value, xvimagesink->disp_height);
      break;
    case PROP_DISPLAY_SRC_X_RATIO:
      g_value_set_float (value, xvimagesink->disp_x_ratio);
      break;
    case PROP_DISPLAY_SRC_Y_RATIO:
      g_value_set_float (value, xvimagesink->disp_y_ratio);
      break;
    case PROP_DISPLAY_SRC_W_RATIO:
      g_value_set_float (value, xvimagesink->disp_width_ratio);
      break;
    case PROP_DISPLAY_SRC_H_RATIO:
      g_value_set_float (value, xvimagesink->disp_height_ratio);
      break;
#endif /* GST_EXT_XV_ENHANCEMENT */
#if ENABLE_RT_DISPLAY
    case PROP_ES_DISPLAY:
	g_value_set_int (value, xvimagesink->rt_display_vaule);
	break;
#endif	// ENABLE_RT_DISPLAY
    case PROP_DISPLAY_NTEFLIX_PAR_X:
	g_value_set_int (value, xvimagesink->netflix_display_par_x);
	break;
    case PROP_DISPLAY_NTEFLIX_PAR_Y:
	g_value_set_int (value, xvimagesink->netflix_display_par_y);
	break;
	case PROP_DISPLAY_DPS_WIDTH:
	  g_value_set_int (value, xvimagesink->dps_display_width);
	  break;
	case PROP_DISPLAY_DPS_HEIGHT:
	  g_value_set_int (value, xvimagesink->dps_display_height);
	  break;
    case PROP_DEVICE_ID:
      g_value_set_int(value, xvimagesink->device_id);
      break;
 	  case PROP_SEAMLESS_RESOLUTION_CHANGE:
      g_value_set_boolean (value, xvimagesink->seamless_resolution_change);
      break;
    case PROP_DISPLAY_UHD_FIT:
	g_value_set_boolean (value, xvimagesink->is_uhd_fit);
	break;
    case PROP_VIDEO_QUALITY_MODE:
      g_value_set_int (value, xvimagesink->video_quality_mode);
      break;
    case PROP_VIDEO_AVOC_SOURCE:
      g_value_set_int (value, xvimagesink->avoc_source);
      break;
	case PROP_CAN_SUPPORT_HW_ROTATE:	 
	{
	  gst_can_support_hw_video_rotate(xvimagesink);
	  g_value_set_boolean (value, xvimagesink->can_support_hw_rotate);
	}
	break;
	case PROP_HW_ROTATE_DEGREE:
	{
	  gst_get_hw_video_rotate_degree(xvimagesink);
      g_value_set_int (value, xvimagesink->get_hw_rotate_degree);
	}
    break;	  
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_xvimagesink_free_outside_buf (GstXvImageSink * xvimagesink)
{
  g_return_if_fail (xvimagesink);
  
  #if ENABLE_VF_ROTATE
    xvimagesink->is_rotate_opened = FALSE;
	if(xvimagesink->user_enable_rotation){
		if(xvimagesink->frctx){
		  	pre_free_rotate_tbm_display_buffer(xvimagesink);
		  	videoframe_rotate_close(xvimagesink->frctx);
			GST_DEBUG_OBJECT(xvimagesink, "Rotate: Success to free cma buffer & close video rotation !!!");
			videoframe_rotate_destroy(xvimagesink->frctx);
			xvimagesink->frctx = NULL;
			if(xvimagesink->vf_sOutputFrame){
				g_free(xvimagesink->vf_sOutputFrame);
				xvimagesink->vf_sOutputFrame = NULL;
			}
			GST_DEBUG_OBJECT(xvimagesink, "Rotate: Success to free destory & output frame !!!");
	  	}
	}
#endif
#if ENABLE_RT_DISPLAY
	if(xvimagesink->rt_display_vaule == 1){
	  if(xvimagesink->rt_allocate_done){
		  rt_free_tbm_display_buffer(xvimagesink);
	  }
	}
#endif
#ifdef XV_WEBKIT_PIXMAP_SUPPORT
	GST_DEBUG_OBJECT (xvimagesink, "deinit xv color conversion!");
	xv_colorconversion_deinit();
#endif /* XV_WEBKIT_PIXMAP_SUPPORT */
#ifdef USE_TBM_SDK
  free_tbm_buffer_sdk(xvimagesink);
#endif
}


static void
gst_xvimagesink_reset (GstXvImageSink * xvimagesink)
{
  GThread *thread;

  GST_OBJECT_LOCK (xvimagesink);
  xvimagesink->running = FALSE;
  /* grab thread and mark it as NULL */
  thread = xvimagesink->event_thread;
  xvimagesink->event_thread = NULL;
  GST_OBJECT_UNLOCK (xvimagesink);

  /* invalidate the pool, current allocations continue, new buffer_alloc fails
   * with wrong_state */
  g_mutex_lock (xvimagesink->pool_lock);
  xvimagesink->pool_invalid = TRUE;
  g_mutex_unlock (xvimagesink->pool_lock);

  /* Wait for our event thread to finish before we clean up our stuff. */
  if (thread)
    g_thread_join (thread);

  if (xvimagesink->cur_image) {
    gst_buffer_unref (GST_BUFFER_CAST (xvimagesink->cur_image));
    xvimagesink->cur_image = NULL;
  }
  if (xvimagesink->xvimage) {
    gst_buffer_unref (GST_BUFFER_CAST (xvimagesink->xvimage));
    xvimagesink->xvimage = NULL;
  }

  gst_xvimagesink_imagepool_clear (xvimagesink);

  g_mutex_lock (xvimagesink->flow_lock);
  if (xvimagesink->xwindow) {
    GST_ERROR_OBJECT(xvimagesink, "xwindow clear & destory !!!"); 
    gst_xvimagesink_xwindow_clear (xvimagesink, xvimagesink->xwindow);
    gst_xvimagesink_xwindow_destroy (xvimagesink, xvimagesink->xwindow);
    xvimagesink->xwindow = NULL;
    GST_ERROR_OBJECT(xvimagesink, "xwindow clear & destory done !!!"); 
  }
  g_mutex_unlock (xvimagesink->flow_lock);

#ifdef GST_EXT_XV_ENHANCEMENT
  if (xvimagesink->get_pixmap_cb) {
    int i = 0;
    if (xvimagesink->xpixmap[0] && xvimagesink->xpixmap[0]->pixmap) {
      g_mutex_lock (xvimagesink->x_lock);
      GST_INFO_OBJECT( xvimagesink, "calling XvStopVideo()" );
      XvStopVideo (xvimagesink->xcontext->disp, xvimagesink->xcontext->xv_port_id, xvimagesink->xpixmap[0]->pixmap);
      g_mutex_unlock (xvimagesink->x_lock);
    }
    for (i = 0; i < MAX_PIXMAP_NUM; i++) {
      if (xvimagesink->xpixmap[i]) {
        gst_xvimagesink_xpixmap_destroy (xvimagesink, xvimagesink->xpixmap[i]);
        xvimagesink->xpixmap[i] = NULL;
      }
    }
    xvimagesink->get_pixmap_cb = NULL;
    xvimagesink->get_pixmap_cb_user_data = NULL;
  }
#endif /* GST_EXT_XV_ENHANCEMENT */

  xvimagesink->render_rect.x = xvimagesink->render_rect.y =
      xvimagesink->render_rect.w = xvimagesink->render_rect.h = 0;
  xvimagesink->have_render_rect = FALSE;

  gst_xvimagesink_xcontext_clear (xvimagesink);

  xvimagesink->pre_bitrate = 0;
  xvimagesink->new_bitrate = 0; //add for updata realtime bitrate
  xvimagesink->sei_metadata_size = 0;
  xvimagesink->sei_mdcv_filled = FALSE;
  xvimagesink->need_update_color_info = FALSE;
  xvimagesink->prev_color_info_format = AVOC_VIDEO_FORMAT_NONE;
}

/* Finalize is called only once, dispose can be called multiple times.
 * We use mutexes and don't reset stuff to NULL here so let's register
 * as a finalize. */
static void
gst_xvimagesink_finalize (GObject * object)
{
  GstXvImageSink *xvimagesink;

  xvimagesink = GST_XVIMAGESINK (object);
  gst_xvimagesink_reset (xvimagesink);

  if (xvimagesink->sei_metadata) {
    g_free (xvimagesink->sei_metadata);
    xvimagesink->sei_metadata = NULL;
    xvimagesink->sei_metadata_alloc_size = 0;
  }

  if (xvimagesink->sei_mdcv) {
    g_free (xvimagesink->sei_mdcv);
    xvimagesink->sei_mdcv = NULL;
  }

  if (xvimagesink->display_name) {
    g_free (xvimagesink->display_name);
    xvimagesink->display_name = NULL;
  }

  if (xvimagesink->par) {
    g_free (xvimagesink->par);
    xvimagesink->par = NULL;
  }
  if (xvimagesink->x_lock) {
    g_mutex_free (xvimagesink->x_lock);
    xvimagesink->x_lock = NULL;
  }
  if (xvimagesink->flow_lock) {
    g_mutex_free (xvimagesink->flow_lock);
    xvimagesink->flow_lock = NULL;
  }
  if (xvimagesink->pool_lock) {
    g_mutex_free (xvimagesink->pool_lock);
    xvimagesink->pool_lock = NULL;
  }

  g_free (xvimagesink->media_title);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GST_DEBUG_CATEGORY_EXTERN(GST_CAT_VREN);

static void
gst_xvimagesink_init (GstXvImageSink * xvimagesink,
    GstXvImageSinkClass * xvimagesinkclass)
{
  GstBaseSink *basesink = NULL;
  
  xvimagesink->display_name = NULL;
  xvimagesink->adaptor_no = 0;
  xvimagesink->xcontext = NULL;
  xvimagesink->xwindow = NULL;
  xvimagesink->xvimage = NULL;
  xvimagesink->cur_image = NULL;

  xvimagesink->hue = xvimagesink->saturation = 0;
  xvimagesink->contrast = xvimagesink->brightness = 0;
  xvimagesink->cb_changed = FALSE;

  xvimagesink->fps_n = 0;
  xvimagesink->fps_d = 0;
  xvimagesink->video_width = xvimagesink->max_video_width = 0;
  xvimagesink->video_height = xvimagesink->max_video_height = 0;
  
  /*video cropping*/
  xvimagesink->disp_x = 0;
  xvimagesink->disp_y = 0;
  xvimagesink->disp_width = 0;
  xvimagesink->disp_height = 0;
  xvimagesink->disp_x_ratio = 0;
  xvimagesink->disp_y_ratio = 0;
  xvimagesink->disp_width_ratio = 1;
  xvimagesink->disp_height_ratio = 1;

  xvimagesink->x_lock = g_mutex_new ();
  xvimagesink->flow_lock = g_mutex_new ();

  xvimagesink->image_pool = NULL;
  xvimagesink->pool_lock = g_mutex_new ();

  xvimagesink->synchronous = FALSE;
  xvimagesink->double_buffer = TRUE;
  xvimagesink->running = FALSE;
  xvimagesink->keep_aspect = FALSE;
  xvimagesink->handle_events = TRUE;
  xvimagesink->par = NULL;
  xvimagesink->handle_expose = TRUE;
  xvimagesink->autopaint_colorkey = TRUE;

  /* on 16bit displays this becomes r,g,b = 1,2,3
   * on 24bit displays this becomes r,g,b = 8,8,16
   * as a port atom value
   */
  xvimagesink->colorkey = (8 << 16) | (8 << 8) | 16;
  xvimagesink->draw_borders = TRUE;

#ifdef GST_EXT_XV_ENHANCEMENT
  xvimagesink->xid_updated = FALSE;
  xvimagesink->display_mode = DISPLAY_MODE_DEFAULT;
  xvimagesink->display_geometry_method = DEF_DISPLAY_GEOMETRY_METHOD;
  xvimagesink->flip = DEF_DISPLAY_FLIP;
  xvimagesink->rotate_angle = DEGREE_0; /* DEGREE_270 IS MOBILE VERSION */
  xvimagesink->rotate_setting_angle = DEGREE_0;
  xvimagesink->visible = TRUE;
  xvimagesink->zoom = 1;
  xvimagesink->rotation = -1;
  xvimagesink->dst_roi.x = 0;
  xvimagesink->dst_roi.y = 0;
  xvimagesink->dst_roi.w = 0;
  xvimagesink->dst_roi.h = 0;
  xvimagesink->xim_transparenter = NULL;
  xvimagesink->scr_w = 0;
  xvimagesink->scr_h = 0;
  xvimagesink->aligned_width = 0;
  xvimagesink->aligned_height = 0;
  xvimagesink->stop_video = FALSE;
  xvimagesink->is_hided = FALSE;
  xvimagesink->drm_fd = -1;
  xvimagesink->current_pixmap_idx = -1;
  xvimagesink->get_pixmap_cb = NULL;
  xvimagesink->get_pixmap_cb_user_data = NULL;
  xvimagesink->pre_bitrate = 0; //add for updata realtime bitrate
  xvimagesink->new_bitrate = 0;
  xvimagesink->cur_scantype = 1;
#endif /* GST_EXT_XV_ENHANCEMENT */

#ifdef XV_WEBKIT_PIXMAP_SUPPORT
  xvimagesink->old_get_pixmap_cb = NULL;
#endif

  basesink = GST_BASE_SINK(xvimagesink);
  basesink->debugCategory = GST_CAT_VREN;

  xvimagesink->scaler_id = 0;
  xvimagesink->crop_flag = FALSE; /*video cropping*/
  xvimagesink->is_first_putimage = TRUE;

#if ENABLE_VF_ROTATE
  xvimagesink->vf_bIsVideoRotationEnabled = FALSE;
  xvimagesink->user_enable_rotation = FALSE;
  xvimagesink->is_rotate_opened = FALSE;
  xvimagesink->ARdegree = 0;
  xvimagesink->CANRotate = 0;
  xvimagesink->support_rotation = FALSE;
  xvimagesink->vf_display_channel = 0; 
  xvimagesink->vf_need_update_display_idx = 0;
  xvimagesink->vf_force_unmute = 0;
#endif

#if ENABLE_RT_DISPLAY
	xvimagesink->rt_display_vaule = 0;
	xvimagesink->rt_display_drm_fd = -1;
	xvimagesink->rt_allocate_done = 0;
	xvimagesink->rt_channel_setting_done = 0;
	xvimagesink->rt_display_avoc_done = 0;
	gint k;
	for(k = 0; k < DISPLAY_BUFFER_NUM; k++){
		memset (&xvimagesink->rt_display_boY[k], 0x0, sizeof(tbm_bo));
		memset (&xvimagesink->rt_display_boCbCr[k], 0x0, sizeof(tbm_bo));
		memset (&xvimagesink->rt_display_bo_hnd_Y[k], 0x0, sizeof(tbm_bo_handle));
		memset (&xvimagesink->rt_display_bo_hnd_CbCr[k], 0x0, sizeof(tbm_bo_handle));
	}
#endif
  xvimagesink->custom_x = 0;/*init custom offset*/
  xvimagesink->custom_y = 0;
  xvimagesink->custom_w = 0;
  xvimagesink->custom_h = 0;
  xvimagesink->zoom_x = 0; /*init zoom offset*/
  xvimagesink->zoom_y = 0;
  xvimagesink->zoom_w = 0;
  xvimagesink->zoom_h = 0;
  xvimagesink->netflix_display_par_x = 0; /*init netflix par*/
  xvimagesink->netflix_display_par_y = 0;
  xvimagesink->dps_display_width = 0; /*init divx par*/
  xvimagesink->dps_display_height = 0;
  xvimagesink->dp_linesize = 0;
  xvimagesink->device_id = DefaultDeviceId;
  xvimagesink->mute_flag = MUTE_DEFAULT; // It's MUTE STATE AT THE BEGINNING.
  xvimagesink->seamless_resolution_change = TRUE;
  xvimagesink->video_quality_mode = 0;
  xvimagesink->avoc_source = AVOC_SOURCE_UNIPLAYER;
  xvimagesink->need_update_color_info = FALSE;
  xvimagesink->prev_color_info_format = AVOC_VIDEO_FORMAT_NONE;
  memset(&xvimagesink->prev_color_info, 0, sizeof(avoc_color_space_info));
  xvimagesink->par_x = 0;
  xvimagesink->par_y = 0;  
  xvimagesink->set_pixmap = FALSE;
  xvimagesink->mode_3d = 0;

  xvimagesink->sei_metadata_size = 0;
  xvimagesink->sei_metadata_alloc_size = 0;
  xvimagesink->sei_metadata = NULL;

  xvimagesink->sei_mdcv_filled = FALSE;
  xvimagesink->sei_mdcv = NULL;
  xvimagesink->is_uhd_fit = FALSE;

#ifdef DUMP_SW_DATA
  xvimagesink->is_check_sw_dump_filename_done = 0;
#endif
#ifdef DUMP_HW_DATA
  xvimagesink->is_check_hw_dump_filename_done = 0;
#endif

#ifndef USE_TBM_SDK
  memset(&xvimagesink->hdr_xml_metadata, 0, sizeof(HDRMetadate)); //For HDR setting
#endif

#ifdef USE_TBM_SDK
	int i = 0;
	for(i = 0; i < DISPLAY_BUFFER_NUM; i++){
	  xvimagesink->p_sdk_TBMinfo[i] = NULL;
	   GST_LOG_OBJECT(xvimagesink, "xvimagesink buffer init, reset p_sdk_TBMinfo to NULL !!!");
	}
	xvimagesink->sdk_dsp_idx = 0;
#endif
  //HW Rotate Context
  xvimagesink->set_hw_rotate_degree = 0;
  xvimagesink->get_hw_rotate_degree = 0;
  xvimagesink->can_support_hw_rotate = FALSE;
  xvimagesink->enable_hw_rotate_support = FALSE;
  xvimagesink->hw_rotate_scaled_width = 0;
  xvimagesink->hw_rotate_scaled_height = 0;
  xvimagesink->hw_rotate_degree = DEGREE_0;
  xvimagesink->curr_hw_rotate_degree = DEGREE_0;
  xvimagesink->prev_hw_rotate_degree = DEGREE_0;
  xvimagesink->is_hw_rotate_degree_changed = FALSE;
  xvimagesink->is_unmute_req_for_hw_rotate = FALSE;

  // Video Mixer Context
  xvimagesink->is_player_mixer_support_enabled = FALSE;
  xvimagesink->is_hw_rotate_on_mixed_frame = FALSE;  
}

static void
gst_xvimagesink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class,
      "Video sink", "Sink/Video",
      "A Xv based videosink", "Julien Moutte <julien@moutte.net>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_xvimagesink_sink_template_factory);
}

static gboolean
gst_xvimagesink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstStructure* structure = NULL;
  GstXvImageSink* xvimagesink = GST_XVIMAGESINK(bsink);
  gboolean ret = FALSE;
  if (xvimagesink == NULL || query == NULL)
  	return FALSE;

  GST_LOG_OBJECT(xvimagesink, "query received[%d]", GST_QUERY_TYPE (query));

  switch (query->type)
  {
    case GST_QUERY_CUSTOM:
    {
	  structure = gst_query_get_structure (query);
	  if(gst_structure_has_name(structure, "tbm_auth") == FALSE)
	  {
	  	return FALSE;
	  }
	  
      if (xvimagesink->xcontext && xvimagesink->xcontext->disp)
      {
        gst_structure_set(structure,"TBMauthentication",G_TYPE_POINTER,xvimagesink->xcontext->disp,NULL);
        ret = TRUE;
      }
      break;
    }
    case GST_QUERY_RESOURCE:
      GST_INFO_OBJECT(xvimagesink, "RESOURCE QUERY - RESOURCE CATEGORY[ ASM_RESOURCE_SCALER ]");
      gst_query_add_resource(query, ASM_RESOURCE_SCALER);
      ret = TRUE;
      break;
    default:
      ret = GST_ELEMENT_CLASS (parent_class)->query(GST_ELEMENT(bsink), query);
      break;
  }
  GST_LOG_OBJECT(xvimagesink,"query type [ %d ] ret[ %d ]",query->type, ret);
  return ret;
}

static void
gst_xvimagesink_class_init (GstXvImageSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *videosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  videosink_class = (GstVideoSinkClass *) klass;

  gobject_class->set_property = gst_xvimagesink_set_property;
  gobject_class->get_property = gst_xvimagesink_get_property;
  gstelement_class->query = gst_xvimagesink_query;

  g_object_class_install_property (gobject_class, PROP_CONTRAST,
      g_param_spec_int ("contrast", "Contrast", "The contrast of the video",
          -1000, 1000, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BRIGHTNESS,
      g_param_spec_int ("brightness", "Brightness",
          "The brightness of the video", -1000, 1000, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HUE,
      g_param_spec_int ("hue", "Hue", "The hue of the video", -1000, 1000, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SATURATION,
      g_param_spec_int ("saturation", "Saturation",
          "The saturation of the video", -1000, 1000, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_string ("display", "Display", "X Display name", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SYNCHRONOUS,
      g_param_spec_boolean ("synchronous", "Synchronous",
          "When enabled, runs the X display in synchronous mode. "
          "(unrelated to A/V sync, used only for debugging)", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
      g_param_spec_string ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", "1/1",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio", "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HANDLE_EVENTS,
      g_param_spec_boolean ("handle-events", "Handle XEvents",
          "When enabled, XEvents will be selected and handled", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Adaptor number",
          "The number of the video adaptor", "0",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Adaptor name",
          "The name of the video adaptor", NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE_ATTR_SCALER,
      g_param_spec_int ("device-scaler", "Scaler ID",
          "To select specific scaler(0:Main, 1:Sub, 2:BG", 0, 2, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEVICE_ID,
      g_param_spec_int ("device-id", "Device ID",
            "device id for the actual hw resource",
            DefaultDeviceId, INT_MAX, DefaultDeviceId,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:handle-expose
   *
   * When enabled, the current frame will always be drawn in response to X
   * Expose.
   *
   * Since: 0.10.14
   */
  g_object_class_install_property (gobject_class, PROP_HANDLE_EXPOSE,
      g_param_spec_boolean ("handle-expose", "Handle expose",
          "When enabled, "
          "the current frame will always be drawn in response to X Expose "
          "events", TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:double-buffer
   *
   * Whether to double-buffer the output.
   *
   * Since: 0.10.14
   */
  g_object_class_install_property (gobject_class, PROP_DOUBLE_BUFFER,
      g_param_spec_boolean ("double-buffer", "Double-buffer",
          "Whether to double-buffer the output", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstXvImageSink:autopaint-colorkey
   *
   * Whether to autofill overlay with colorkey
   *
   * Since: 0.10.21
   */
  g_object_class_install_property (gobject_class, PROP_AUTOPAINT_COLORKEY,
      g_param_spec_boolean ("autopaint-colorkey", "Autofill with colorkey",
          "Whether to autofill overlay with colorkey", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstXvImageSink:colorkey
   *
   * Color to use for the overlay mask.
   *
   * Since: 0.10.21
   */
  g_object_class_install_property (gobject_class, PROP_COLORKEY,
      g_param_spec_int ("colorkey", "Colorkey",
          "Color to use for the overlay mask", G_MININT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:draw-borders
   *
   * Draw black borders when using GstXvImageSink:force-aspect-ratio to fill
   * unused parts of the video area.
   *
   * Since: 0.10.21
   */
  g_object_class_install_property (gobject_class, PROP_DRAW_BORDERS,
      g_param_spec_boolean ("draw-borders", "Colorkey",
          "Draw black borders to fill unused area in force-aspect-ratio mode",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:window-width
   *
   * Actual width of the video window.
   *
   * Since: 0.10.32
   */
  g_object_class_install_property (gobject_class, PROP_WINDOW_WIDTH,
      g_param_spec_int ("window-width", "window-width",
          "Width of the window", 0, G_MAXINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:window-height
   *
   * Actual height of the video window.
   *
   * Since: 0.10.32
   */
  g_object_class_install_property (gobject_class, PROP_WINDOW_HEIGHT,
      g_param_spec_int ("window-height", "window-height",
          "Height of the window", 0, G_MAXINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  


/*video cropping*/
  g_object_class_install_property (gobject_class, PROP_DISPLAY_SRC_X,
      g_param_spec_int ("display-src-x", "src_x of video cropping",
          "src_x of video cropping", 0, G_MAXINT, 0,
          G_PARAM_READWRITE ));

  g_object_class_install_property (gobject_class, PROP_DISPLAY_SRC_Y,
      g_param_spec_int ("display-src-y", "src_y of video cropping",
          "src_y of video cropping", 0, G_MAXINT, 0,
          G_PARAM_READWRITE ));
  g_object_class_install_property (gobject_class, PROP_DISPLAY_SRC_W,
      g_param_spec_int ("display-src-w", "src_w of video cropping",
          "src_w of video cropping", 0, G_MAXINT, 0,
          G_PARAM_READWRITE ));
  g_object_class_install_property (gobject_class, PROP_DISPLAY_SRC_H,
      g_param_spec_int ("display-src-h", "src_h of video cropping",
          "src_h of video cropping", 0, G_MAXINT, 0,
          G_PARAM_READWRITE ));

  g_object_class_install_property (gobject_class, PROP_DISPLAY_SRC_X_RATIO,
      g_param_spec_float ("display-src-x-ratio", "src_x ratio of video cropping",
          "src_x ratio of video cropping", 0, 1, 0,
          G_PARAM_READWRITE ));

  g_object_class_install_property (gobject_class, PROP_DISPLAY_SRC_Y_RATIO,
      g_param_spec_float ("display-src-y-ratio", "src_y ratio of video cropping",
          "src_y ratio of video cropping", 0, 1, 0,
          G_PARAM_READWRITE ));
  g_object_class_install_property (gobject_class, PROP_DISPLAY_SRC_W_RATIO,
      g_param_spec_float ("display-src-w-ratio", "src_w ratio of video cropping",
          "src_w ratio of video cropping", 0, 1, 0,
          G_PARAM_READWRITE ));
  g_object_class_install_property (gobject_class, PROP_DISPLAY_SRC_H_RATIO,
      g_param_spec_float ("display-src-h-ratio", "src_h ratio of video cropping",
          "src_h ratio of video cropping", 0, 1, 0,
          G_PARAM_READWRITE ));


/*zoom*/
g_object_class_install_property (gobject_class, PROP_DISPLAY_ZOOM_Y,
	g_param_spec_int ("display-zoom-y", "dy of zoom offset",
		"dy of zoom offset", -G_MAXINT, G_MAXINT, 0,
		G_PARAM_READWRITE ));
g_object_class_install_property (gobject_class, PROP_DISPLAY_ZOOM_H,
	g_param_spec_int ("display-zoom-h", "height of zoom offset",
		"zoom height", -G_MAXINT, G_MAXINT, 0,
		G_PARAM_READWRITE ));

/*custom*/
g_object_class_install_property (gobject_class, PROP_DISPLAY_CUSTOM_X,
	g_param_spec_int ("display-custom-x", "dx of zoom offset",
		"dx of custom offset", -G_MAXINT, G_MAXINT, 0,
		G_PARAM_READWRITE ));

g_object_class_install_property (gobject_class, PROP_DISPLAY_CUSTOM_Y,
	g_param_spec_int ("display-custom-y", "dy of custom offset",
		"dy of custom offset", -G_MAXINT, G_MAXINT, 0,
		G_PARAM_READWRITE ));
g_object_class_install_property (gobject_class, PROP_DISPLAY_CUSTOM_W,
	g_param_spec_int ("display-custom-w", "width of custom offset",
		"custom width", -G_MAXINT, G_MAXINT, 0,
		G_PARAM_READWRITE ));
g_object_class_install_property (gobject_class, PROP_DISPLAY_CUSTOM_H,
	g_param_spec_int ("display-custom-h", "height of custom offset",
		"custom height", -G_MAXINT, G_MAXINT, 0,
		G_PARAM_READWRITE ));

/*Netflix*/
	g_object_class_install_property (gobject_class, PROP_DISPLAY_NTEFLIX_PAR_X,
		g_param_spec_int ("display-netflix-par-x", "netflix content display apsect ratio x",
			"par x", -G_MAXINT, G_MAXINT, 0,
			G_PARAM_READWRITE ));
	g_object_class_install_property (gobject_class, PROP_DISPLAY_NTEFLIX_PAR_Y,
		g_param_spec_int ("display-netflix-par-y", "netflix content display apsect ratio y",
			"par y", -G_MAXINT, G_MAXINT, 0,
			G_PARAM_READWRITE ));
/*DivX*/
	g_object_class_install_property (gobject_class,PROP_DISPLAY_DPS_WIDTH,
		g_param_spec_int ("display-dps-width", "divx content display width",
			"divx width", -G_MAXINT, G_MAXINT, 0,
			G_PARAM_READWRITE ));
	g_object_class_install_property (gobject_class,PROP_DISPLAY_DPS_HEIGHT,
		g_param_spec_int ("display-dps-height", "divx content display height",
			"divx height", -G_MAXINT, G_MAXINT, 0,
			G_PARAM_READWRITE ));
#if ENABLE_RT_DISPLAY
	g_object_class_install_property (gobject_class, PROP_ES_DISPLAY,
	  g_param_spec_int("use-mp-buffer", "use-mp-buffer","Set ES Player Display Value, 0: HW Path	1: SW Path", 0, 1, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

#ifdef GST_EXT_XV_ENHANCEMENT
  /**
   * GstXvImageSink:display-orientation
   *
   * select display orientation
   */
   g_object_class_install_property(gobject_class, PROP_ORIENTATION,
    g_param_spec_int("orientation", "Display Orientation",
      "Display orientation setting: 0 degree, 90 degree, 180 degree, 270 degree ", 0, 3, 0,
      G_PARAM_READWRITE));

  /**
   * GstXvImageSink:display-mode
   *
   * select display mode
   */
  g_object_class_install_property(gobject_class, PROP_DISPLAY_MODE,
    g_param_spec_enum("display-mode", "Display Mode",
      "Display device setting",
      GST_TYPE_XVIMAGESINK_DISPLAY_MODE, DISPLAY_MODE_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:display-geometry-method
   *
   * Display geometrical method setting
   */
  g_object_class_install_property(gobject_class, PROP_DISPLAY_GEOMETRY_METHOD,
    g_param_spec_enum("display-geometry-method", "Display geometry method",
      "Geometrical method for display",
      GST_TYPE_XVIMAGESINK_DISPLAY_GEOMETRY_METHOD, DEF_DISPLAY_GEOMETRY_METHOD,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:display-flip
   *
   * Display flip setting
   */
  g_object_class_install_property(gobject_class, PROP_FLIP,
    g_param_spec_enum("flip", "Display flip",
      "Flip for display",
      GST_TYPE_XVIMAGESINK_FLIP, DEF_DISPLAY_FLIP,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:support-rotation
   *
   * Support rotation setting
   */
  g_object_class_install_property(gobject_class, PROP_SUPPORT_ROTATION,
    g_param_spec_boolean("support-rotation", "Support video rotation",
      "If support video rotation or not", TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:rotate
   *
   * Draw rotation angle setting
   */
  g_object_class_install_property(gobject_class, PROP_ROTATE_ANGLE,
    g_param_spec_enum("rotate", "Rotate angle",
      "Rotate angle of display output",
      GST_TYPE_XVIMAGESINK_ROTATE_ANGLE, DEGREE_0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:enable-rotation
   *
   * Enable rotation setting
   */
  g_object_class_install_property(gobject_class, PROP_ENABEL_ROTATION,
    g_param_spec_boolean("enable-rotation", "Enable video rotation",
      "If app want to support video rotation or not", FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:visible
   *
   * Whether reserve original src size or not
   */
  g_object_class_install_property (gobject_class, PROP_VISIBLE,
      g_param_spec_boolean ("visible", "Visible",
          "Draws screen or blacks out, true means visible, false blacks out",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:zoom
   *
   * Scale small area of screen to 2X, 3X, ... , 9X
   */
  g_object_class_install_property (gobject_class, PROP_ZOOM,
      g_param_spec_float ("zoom", "Zoom",
          "Zooms screen as nX",0.1,1,1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:dst-roi-x
   *
   * X value of Destination ROI
   */
  g_object_class_install_property (gobject_class, PROP_DST_ROI_X,
      g_param_spec_int ("dst-roi-x", "Dst-ROI-X",
          "X value of Destination ROI(only effective \"CUSTOM_ROI\")", 0, XV_SCREEN_SIZE_WIDTH, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:dst-roi-y
   *
   * Y value of Destination ROI
   */
  g_object_class_install_property (gobject_class, PROP_DST_ROI_Y,
      g_param_spec_int ("dst-roi-y", "Dst-ROI-Y",
          "Y value of Destination ROI(only effective \"CUSTOM_ROI\")", 0, XV_SCREEN_SIZE_HEIGHT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:dst-roi-w
   *
   * W value of Destination ROI
   */
  g_object_class_install_property (gobject_class, PROP_DST_ROI_W,
      g_param_spec_int ("dst-roi-w", "Dst-ROI-W",
          "W value of Destination ROI(only effective \"CUSTOM_ROI\")", 0, XV_SCREEN_SIZE_WIDTH, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:dst-roi-h
   *
   * H value of Destination ROI
   */
  g_object_class_install_property (gobject_class, PROP_DST_ROI_H,
      g_param_spec_int ("dst-roi-h", "Dst-ROI-H",
          "H value of Destination ROI(only effective \"CUSTOM_ROI\")", 0, XV_SCREEN_SIZE_HEIGHT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:uhd fit
   *
   * UHD whether fit screen or not
   */
  g_object_class_install_property (gobject_class, PROP_DISPLAY_UHD_FIT,
      g_param_spec_boolean ("display-uhd-fit", "Fit Screen",
          "UHD 4096x2160, Fit screen on / off",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:stop-video
   *
   * Stop video for releasing video source buffer
   */
  g_object_class_install_property (gobject_class, PROP_STOP_VIDEO,
      g_param_spec_int ("stop-video", "Stop-Video",
          "Stop video for releasing video source buffer", 0, 1, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PIXMAP_CB,
      g_param_spec_pointer("pixmap-id-callback", "Pixmap-Id-Callback",
          "pointer of callback function for getting pixmap id", G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PIXMAP_CB_USER_DATA,
      g_param_spec_pointer("pixmap-id-callback-userdata", "Pixmap-Id-Callback-Userdata",
          "pointer of user data of callback function for getting pixmap id", G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_VIDEO_QUALITY_MODE,
      g_param_spec_int ("video-quality-mode", "Video-Quality-Mode",
          "Set VideoQualitySetting Mode.(use bit-mask, only available on NULL/READY STATE)\n\
                        0x00 : Default - Sync setting.\n\
                        0x01 : Async setting (data consumming will be happened during this video-quality setting),\n\
                        0x02 : Enable PC Mode\n\
                        0x04 : Enable Game Mode(can not be used with PCMode)\n\
                        0x08 : Disable whole AVOC setting(Start/Resolution/Stop)\n\
                        0x1000 : videopack mode", 0, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VIDEO_AVOC_SOURCE,
      g_param_spec_int ("avoc-source", "avoc-source",
          "Select particular avoc-source (please refer to the avoc_source_e in avoc_def.h)\
                        AVOC_SOURCE_UNIPLAYER(Default)\n\
                        AVOC_SOURCE_RVU\n\
                        AVOC_SOURCE_IPTV\n", -G_MAXINT, G_MAXINT, (gint)AVOC_SOURCE_UNIPLAYER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	

  g_object_class_install_property (gobject_class, PROP_SEAMLESS_RESOLUTION_CHANGE,
      g_param_spec_boolean("seamless-resolution-change", "Seamless Resolution Change",
          "As default 'FALSE', resolution-change will be happened with flicker(mute).\n\
                        If you set TRUE, resolution can be changed without any flicker. (Seamless)\n\
                        But, you should know there is some restriction for this seamless mode, please contact HQ driver team.\n\
                        * Only available on NULL/READY STATE.\n\
                        * This is only available when you use xvimagesink with omx decoder.\n\
                        * and of course, if you set TRUE for this property, you must also set TRUE to the 'seamless' property of omx video decoder.",
                        FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_3D_MODE,
      g_param_spec_int("mode-3d", "Mode-3D",
          "To set 3d mode",0,4,0,
          G_PARAM_READWRITE| G_PARAM_STATIC_STRINGS));

  /**
   * GstXvImageSink:Set HW Rotate Degree
   *
   * To set the degree for HW Video Rotation
   */     
  g_object_class_install_property (gobject_class, PROP_CAN_SUPPORT_HW_ROTATE,
      g_param_spec_boolean("can-support-hw-rotate-feature", "Can-Support-Hw-Rotate-Feature",
          "Check If HW Rotation is supported or not",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)); 
  g_object_class_install_property (gobject_class, PROP_HW_ROTATE_DEGREE,
      g_param_spec_int ("hardware-rotate", "Hardware-Rotate",
          "Rotate Degree for HW Video Rotation", 0, 270, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ENABLE_HW_ROTATION,
      g_param_spec_boolean("enable-hw-rotate", "Enable-Hw-Rotate-Support",
          "Enable HW Rotation is supported or not",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MIXER_ROTATE_DEGREE,
      g_param_spec_int ("mixer-set-rotate-degree", "MixerSetRotateDegree",
          "To Set Rotation Degree on Mixed Frame", 0, 270, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MIXER_POSITION,
      g_param_spec_int ("video-mixer-position", "VideoMixerSetPosition",
          "To Set Video Mixer Buffer Set Position", -G_MAXINT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));  
  /**
   * GstXvImageSink::frame-render-error
   */
  gst_xvimagesink_signals[SIGNAL_FRAME_RENDER_ERROR] = g_signal_new (
          "frame-render-error",
          G_TYPE_FROM_CLASS (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL,
          NULL,
          gst_xvimagesink_BOOLEAN__POINTER,
          G_TYPE_BOOLEAN,
          1,
          G_TYPE_POINTER);

	gst_xvimagesink_signals[SIGNAL_SEIMETADATA_CHANGED] = g_signal_new (
          "seimetadata-changed",
          G_TYPE_FROM_CLASS (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL,
          NULL,
          gst_xvimagesink_BOOLEAN__POINTER,
          G_TYPE_BOOLEAN,
          1,
          G_TYPE_POINTER);

#endif /* GST_EXT_XV_ENHANCEMENT */

  gobject_class->finalize = gst_xvimagesink_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_xvimagesink_change_state);

  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_xvimagesink_getcaps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_xvimagesink_setcaps);
  gstbasesink_class->buffer_alloc =
      GST_DEBUG_FUNCPTR (gst_xvimagesink_buffer_alloc);
  gstbasesink_class->get_times = GST_DEBUG_FUNCPTR (gst_xvimagesink_get_times);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_xvimagesink_event);
  gstbasesink_class->query = GST_DEBUG_FUNCPTR (gst_xvimagesink_query);

  videosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_xvimagesink_show_frame);
}

/* ============================================================= */
/*                                                               */
/*                       Public Methods                          */
/*                                                               */
/* ============================================================= */

/* =========================================== */
/*                                             */
/*          Object typing & Creation           */
/*                                             */
/* =========================================== */
static void
gst_xvimagesink_init_interfaces (GType type)
{
  static const GInterfaceInfo iface_info = {
    (GInterfaceInitFunc) gst_xvimagesink_interface_init,
    NULL,
    NULL,
  };
  static const GInterfaceInfo navigation_info = {
    (GInterfaceInitFunc) gst_xvimagesink_navigation_init,
    NULL,
    NULL,
  };
  static const GInterfaceInfo overlay_info = {
    (GInterfaceInitFunc) gst_xvimagesink_xoverlay_init,
    NULL,
    NULL,
  };
  static const GInterfaceInfo colorbalance_info = {
    (GInterfaceInitFunc) gst_xvimagesink_colorbalance_init,
    NULL,
    NULL,
  };
  static const GInterfaceInfo propertyprobe_info = {
    (GInterfaceInitFunc) gst_xvimagesink_property_probe_interface_init,
    NULL,
    NULL,
  };

  g_type_add_interface_static (type,
      GST_TYPE_IMPLEMENTS_INTERFACE, &iface_info);
  g_type_add_interface_static (type, GST_TYPE_NAVIGATION, &navigation_info);
  g_type_add_interface_static (type, GST_TYPE_X_OVERLAY, &overlay_info);
  g_type_add_interface_static (type, GST_TYPE_COLOR_BALANCE,
      &colorbalance_info);
  g_type_add_interface_static (type, GST_TYPE_PROPERTY_PROBE,
      &propertyprobe_info);

  /* register type and create class in a more safe place instead of at
   * runtime since the type registration and class creation is not
   * threadsafe. */
  g_type_class_ref (gst_xvimage_buffer_get_type ());
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "xvimagesink",
          GST_RANK_PRIMARY, GST_TYPE_XVIMAGESINK))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (gst_debug_xvimagesink, "xvimagesink", 0,
      "xvimagesink element");
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");

  return TRUE;
}
 
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "xvimagesink",
    "XFree86 video output plugin using Xv extension",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)


typedef struct {
  const guint enumVal;
  const char* str;
} item_3d_info;

const static item_3d_info list3Dmodes[] = {
    {AVOC_3D_EFFECT_OFF,  "Default"},// 2D default
    {AVOC_3D_SIDE_BY_SIDE, "SideBySide"},
    {AVOC_3D_TOP_BOTTOM, "TopAndBottom"},
    {AVOC_3D_FRAME_SEQUENCE, "FrameSequence"},
    {AVOC_3D_FRAME_PACKING, "FramePacking"},
    {AVOC_3D_EFFECT_OFF, NULL}
};

const static item_3d_info list3Dformats[] = {
    {AVOC_3D_FORMAT_MAX,  "Default"},// 2D default
    {AVOC_3D_FORMAT_SVAF,  "SVAF"},
    {AVOC_3D_FORMAT_M2TS,  "SVAF_2ES_Right"},
    {AVOC_3D_FORMAT_M2TS,  "SVAF_2ES_Left"},
    {AVOC_3D_FORMAT_M2TS,  "MVC_base"},
    {AVOC_3D_FORMAT_M2TS,  "MVC_dependent"},
    {AVOC_3D_FORMAT_FILENAME,  "FileName"},
    {AVOC_3D_FORMAT_MVC_SEAMLESS,  "MVC_seamless"},
    {AVOC_3D_FORMAT_MAX, NULL},
};
static GHashTable *table3Dformat = NULL;
static GHashTable *table3Dmode = NULL;

static gboolean
gst_xvimagesink_set_3d_info(GstXvImageSink *xvimagesink, avoc_tpt_resolution_s* avoc_data, GstStructure *in_structure)
{
	const gchar* format = NULL;
	const gchar* mode = NULL;
	const item_3d_info* item = NULL;
	g_return_val_if_fail (avoc_data, FALSE);
	g_return_val_if_fail (in_structure, FALSE);

	/* Initialize the 3D information table once */
	if (table3Dformat == NULL)
	{
		item = list3Dformats;
		table3Dformat = g_hash_table_new (g_str_hash, g_str_equal);
		while(item && item->str)
		{
			g_hash_table_insert (table3Dformat, (gpointer)item->str, (gpointer)item);
			item++;
		}
		item = NULL;
	}
	if (table3Dmode == NULL)
	{
		item = list3Dmodes;
		table3Dmode = g_hash_table_new (g_str_hash, g_str_equal);
		while(item && item->str)
		{
			g_hash_table_insert (table3Dmode, (gpointer)item->str, (gpointer)item);
			item++;
		}
		item = NULL;
	}

	/* Find available 3d information from given structure */
	format = gst_structure_get_string(in_structure, "3Dformat");
	if (format == NULL)
	{
		format = "Default";
	}

	item = g_hash_table_lookup (table3Dformat, format);
	if (item)
	{
		avoc_data->format_3d = item->enumVal;
	}

	mode = gst_structure_get_string(in_structure, "3Dmode");
	if (mode == NULL)
	{
		mode = "Default";
	}

	item = g_hash_table_lookup (table3Dmode, mode);
	if (item)
	{
		avoc_data->video_3d_mode = item->enumVal;
		if(xvimagesink->mode_3d)
		{
			if(xvimagesink->mode_3d == 1/*MM_PLAYER_3D_TYPE_SBS*/)
				avoc_data->video_3d_mode = AVOC_3D_SIDE_BY_SIDE;
			if(xvimagesink->mode_3d == 2/*MM_PLAYER_3D_TYPE_TNB*/)
				avoc_data->video_3d_mode = AVOC_3D_TOP_BOTTOM;
		}
			GST_ERROR("mode [%d]",xvimagesink->mode_3d);
	}

	if (xvimagesink->seamless_resolution_change && (avoc_data->format_3d == AVOC_3D_FORMAT_M2TS) && (g_str_has_prefix(format, "MVC_")))
	{
		avoc_data->format_3d = AVOC_3D_FORMAT_MVC_SEAMLESS;
	}

	return TRUE;
}

static gboolean
gst_xvimagesink_set_resolution_type(avoc_tpt_resolution_s* avoc_data)
{
	g_return_val_if_fail (avoc_data, FALSE);
	struct v4l2_sdpmfc_videoinfo src;
	struct v4l2_sdpmfc_videoinfo dest;
	memset( &src, 0, sizeof(struct v4l2_sdpmfc_videoinfo) );
	memset( &dest, 0, sizeof(struct v4l2_sdpmfc_videoinfo) );
	RESResolution_k res = RES_RESOLUTION_MAX;
	
	src.width = avoc_data->h_resolution;
	src.height = avoc_data->v_resolution;
	src.framerate = avoc_data->v_frequency; //5000, 6000, xxx
	src.progressive = avoc_data->progress_scan; // set scan type 
	res_lookup_convert_dtv_resolution(V4L2_DRM_INPUTPORT_TYPE_MM, &src, &dest, &res);		//guided by sh1223.kim and dest is no mean
	avoc_data->resolution = res;

	return TRUE;
}

static avoc_video_data_format_e
gst_xvimagesink_set_videocodec_info(GstXvImageSink * xvimagesink, GstStructure* structure)
{
	g_return_val_if_fail (structure, FALSE);//Fix for RTSP crash

	const gchar* video_codec = gst_structure_get_string(structure, "prevmimetype");
	const gchar* str3dformat = gst_structure_get_string(structure, "3Dformat");
	const gint version = 0;
	gst_structure_get_int(structure, "codec_version", &version);

	if (g_str_has_prefix(video_codec, "video/x-h264"))
	{
		if (g_str_has_prefix(str3dformat, "MVC"))
			return AVOC_VIDEO_FORMAT_MVC;
		else
			return AVOC_VIDEO_FORMAT_H264;
	}
	else if (g_str_has_prefix(video_codec, "video/x-h265"))
		return AVOC_VIDEO_FORMAT_H264;
	else if (g_str_has_prefix(video_codec, "video/mpeg"))
	{
		if (version == 1)
			return AVOC_VIDEO_FORMAT_MPEG1; // MPEG-1 video
		else if (version == 2)
			return AVOC_VIDEO_FORMAT_MPEG2; // MPEG-2 video
		else
			return AVOC_VIDEO_FORMAT_MPEG4; // MPEG-4 video
	}		
	else if (g_str_has_prefix(video_codec, "video/x-msmpeg"))
		return AVOC_VIDEO_FORMAT_MSMPEG4;
	else if (g_str_has_prefix(video_codec, "video/x-wmv"))
	{
		if (version == 3)
			return AVOC_VIDEO_FORMAT_VC1;  // VC1 / Windows Media Video 9
		else
			return AVOC_VIDEO_FORMAT_WMV;  // Windows Media Video
	}
	else if (g_str_has_prefix(video_codec, "video/x-jpeg"))
		return AVOC_VIDEO_FORMAT_MJPEG;
	else if (g_str_has_prefix(video_codec, "video/x-h263"))
		return AVOC_VIDEO_FORMAT_H263;
	else if (g_str_has_prefix(video_codec, "video/x-pn-realvideo"))
		return AVOC_VIDEO_FORMAT_REALVIDEO; // RealVideo 4.0
	else if (g_str_has_prefix(video_codec, "video/x-vp8"))
		return AVOC_VIDEO_FORMAT_ON2; // On2 VP8
	else if (g_str_has_prefix(video_codec, "video/x-vp6"))
		return AVOC_VIDEO_FORMAT_VP6;
	else if ((g_str_has_prefix(video_codec, "video/x-divx")) || (g_str_has_prefix(video_codec, "video/x-xvid")) || (g_str_has_prefix(video_codec, "video/x-3ivx")))
		return AVOC_VIDEO_FORMAT_MPEG4; // MPEG-4 part 2
	else if ((g_str_has_prefix(video_codec, "video/x-avs")) || (g_str_has_prefix(video_codec, "video/x-avs+")))
		return AVOC_VIDEO_FORMAT_AVS; // AVS (Audio Video Standard) video
	else
		return AVOC_VIDEO_FORMAT_H264;

	GST_ERROR_OBJECT(xvimagesink, "Can not find  proper codec info, set default - AVOC_VIDEO_FORMAT_H264");
	return AVOC_VIDEO_FORMAT_H264;
}

static int
gst_xvimagesink_avoc_preset_async_done_cb(int return_value)
{
	GST_ERROR("avoc_set_pre_resolution_async  done");
	return 0;
}

static GstXvImageSink *xvimagesink_for_main_scaler = NULL;  // must be kept before avoc_set_resolution_async() call. Because avoc interfaces have no Userdata.
static int
gst_xvimagesink_avoc_postset_async_done_cb(int return_value)
{
	if (xvimagesink_for_main_scaler) {
	  mute_video_display(xvimagesink_for_main_scaler, FALSE, MUTE_VIDEO_QUALITY_SET);
	  xvimagesink_for_main_scaler = NULL;
	}
	GST_ERROR("avoc_set_resolution_async done");
	return 0;
}

static void
gst_xvimagesink_avoc_stop(GstXvImageSink *xvimagesink)
{
	g_return_if_fail (xvimagesink);
	if (xvimagesink->avoc_video_started == FALSE)
	{
		GST_ERROR_OBJECT(xvimagesink, "avoc_video_start was not called before.");
		return;
	}

	xvimagesink->avoc_video_started = FALSE;
	GST_LOG_OBJECT(xvimagesink, "avoc_video_stop called");
	xvimagesink_for_main_scaler = NULL;
#ifdef USE_AVOC_DAEMON	
	int ret = avoc_video_stop( 0 /*desktop id*/, xvimagesink->avoc_source /* can be changed for scaler */, AVOC_SCALER_MAIN);
	if (ret != AVOC_EXIT_SUCCESS)
	{
		GST_ERROR_OBJECT(xvimagesink, "avoc_video_stop failed, ret[ %d ]", ret);
	}
#endif
	GST_ERROR_OBJECT(xvimagesink, "avoc_video_stop done");
}


static gboolean
gst_xvimagesink_avoc_set_resolution(GstXvImageSink *xvimagesink, GstCaps *newcaps, gboolean async, gboolean seamless_on, gboolean is_pre_cb)
{
	g_return_val_if_fail (xvimagesink, FALSE);
	g_return_val_if_fail (newcaps, FALSE);
	g_return_val_if_fail (xvimagesink->scaler_id==0, FALSE); // This set_resolution is only for main scaler (0)

	GstStructure *structure = gst_caps_get_structure(newcaps, 0);
	g_return_val_if_fail (structure, FALSE);
	if(FIND_MASK(xvimagesink->video_quality_mode, VQ_MODE_EXTERNAL_AVOC_SET))
	{
		if (xvimagesink->is_first_putimage)
		{
			GST_ERROR_OBJECT(xvimagesink,"VQ_MODE_EXTERNAL_AVOC_SET");
			return TRUE;
		}
		else
			GST_ERROR_OBJECT(xvimagesink,"VQ_MODE_EXTERNAL_AVOC_SET, but, This is resolution-change case");
	}
	int ret = 0;
	avoc_tpt_resolution_s res_data;
	guint num=0, den=0;
	memset( &res_data, 0, sizeof(avoc_tpt_resolution_s) );
	//_mmplayer_update_content_attrs(player,ATTR_DURATION|ATTR_BITRATE|ATTR_VIDEO);
	if (xvimagesink->avoc_video_started == FALSE)
	{
		/* BEGINNING OF PLAYBACK */
		xvimagesink->avoc_video_started = TRUE;
		GST_LOG_OBJECT(xvimagesink, "avoc_video_start called");
#ifdef USE_AVOC_DAEMON 
		ret = avoc_video_start( 0 /*desktop id*/, xvimagesink->avoc_source /* can be changed for scaler */, AVOC_SCALER_MAIN);
		if (ret != AVOC_EXIT_SUCCESS)
		{
			GST_ERROR_OBJECT(xvimagesink, "avoc_video_start failed, ret[ %d ]", ret);
			return FALSE;
		}
#endif
		GST_INFO_OBJECT(xvimagesink, "avoc_video_start done");
	}
	else
	{
		/* DURING THE PLAYBACK */
		GST_LOG_OBJECT(xvimagesink, "dynamic resolution change !");
	}
	gst_structure_get_int(structure, "width", (gint *)&res_data.h_resolution);
	gst_structure_get_int(structure, "height", (gint *)&res_data.v_resolution);
	gst_structure_get_fraction(structure, "framerate", (gint *)&num, (gint *)&den);
	if (den == 0)
	{
		GST_ERROR_OBJECT(xvimagesink, "num[ %d ], den[ %d ]--> forced set den to 1", num, den);
		den = 1;
	}

	guint64 ullFreq = num;
	ullFreq = ullFreq * 100 / den;
	if (ullFreq >= G_MAXUINT32)
		GST_ERROR_OBJECT(xvimagesink, "the framerate is wrong,  num[ %d ] / den[ %d ],  ullFreq[ %lld ]", num, den, ullFreq);
	res_data.v_frequency = (unsigned int)ullFreq;
	res_data.bit_rate = 9000 /* TODO: Now can't get bitrate from attr, */;
	res_data.seamless_play_enabled = seamless_on;
	res_data.v_window_size = xvimagesink->xwindow->height;
	res_data.h_window_size = xvimagesink->xwindow->width;
	res_data.progress_scan =  xvimagesink->cur_scantype;
	res_data.video_format = gst_xvimagesink_set_videocodec_info( xvimagesink, structure);
	res_data.res_pc_mode = (FIND_MASK(xvimagesink->video_quality_mode, VQ_MODE_PC_MODE) ? 1:0);
	res_data.game_mode = (FIND_MASK(xvimagesink->video_quality_mode, VQ_MODE_GAME_MODE) ? 1:0);
	res_data.video_pack = (FIND_MASK(xvimagesink->video_quality_mode, VQ_MODE_VIDEO_PACK) ? 1:0);
	gst_xvimagesink_set_resolution_type(&res_data);
	gst_xvimagesink_set_3d_info(xvimagesink, &res_data, structure);
	xvimagesink->resolution = res_data.resolution;

	GST_ERROR_OBJECT(xvimagesink, "source[ %d ], h_res[ %d ], v_res[ %d ], v_freq[ %d ], bitrate[ %d ], codec_level[ %d ] res[ %d ], pc[ %d ], game[ %d ], 3Dmode[ %d ], format_3d[ %d ], vfmt[ %s : %d ], async[ %d ] seamless[ %d ]  is_pre_cb[ %d ]",
		xvimagesink->avoc_source, res_data.h_resolution, res_data.v_resolution, res_data.v_frequency, res_data.bit_rate, res_data.codec_level_type, res_data.resolution, res_data.res_pc_mode, res_data.game_mode,
		res_data.video_3d_mode, res_data.format_3d, gst_structure_get_string(structure, "prevmimetype"), res_data.video_format, async, res_data.seamless_play_enabled, is_pre_cb);
	guint64 T1 = get_time();
	if (is_pre_cb)
	{
#ifdef USE_AVOC_DAEMON 	
		if (async)
			ret = avoc_set_pre_resolution_async( 0 /*desktop id*/, xvimagesink->avoc_source /* can be changed for scaler */, res_data, AVOC_SCALER_MAIN, gst_xvimagesink_avoc_preset_async_done_cb);
		else
			ret = avoc_set_pre_resolution( 0 /*desktop id*/, xvimagesink->avoc_source /* can be changed for scaler */, res_data, AVOC_SCALER_MAIN);
#endif
	}
	else
	{
		if (seamless_on == FALSE)
			mute_video_display(xvimagesink, TRUE, MUTE_VIDEO_QUALITY_SET); // Will be unmuted by gst_xvimagesink_avoc_postset_async_done_cb() for async mode.
		if (async)
		{
			xvimagesink_for_main_scaler = xvimagesink;
#ifdef USE_AVOC_DAEMON 
			ret = avoc_set_resolution_async( 0 /*desktop id*/, xvimagesink->avoc_source /* can be changed for scaler */, res_data, AVOC_SCALER_MAIN, gst_xvimagesink_avoc_postset_async_done_cb);
#endif
		}
		else
		{
#ifdef USE_AVOC_DAEMON		
			ret = avoc_set_resolution( 0 /*desktop id*/, xvimagesink->avoc_source /* can be changed for scaler */, res_data, AVOC_SCALER_MAIN);
#endif
			if (seamless_on == FALSE)
				mute_video_display(xvimagesink, FALSE, MUTE_VIDEO_QUALITY_SET);
		}
	}
	guint64 T2 = get_time();
	if (ret != AVOC_EXIT_SUCCESS)
	{
		GST_ERROR_OBJECT(xvimagesink, "avoc_set_resolution%s" "failed, ret[ %d | %lld ms ]", async?"_async ":" ", ret,  (T2-T1)/1000);
		return FALSE;
	}
	GST_ERROR_OBJECT(xvimagesink, "avoc_set_resolution%s" "done[ %lld ms ]", async?"_async ":" ", (T2-T1)/1000);
	return TRUE;
}

//For HDR setting
#ifndef USE_TBM_SDK
static gboolean gst_xvimagesink_avoc_set_attribute(GstXvImageSink * xvimagesink)
{
    GST_INFO_OBJECT(xvimagesink, "SCSA HDR Input gst_xvimagesink_avoc_set_attribute");
    gboolean ret = FALSE;
    HDRMetadate scsa_xml_metadata;
    memset( &scsa_xml_metadata, 0, sizeof(HDRMetadate));
	
    GstStructure *query_structure = gst_structure_new("scsa_metadata", NULL);
    GstQuery *query = gst_query_new_application (GST_QUERY_CUSTOM, query_structure);
    gboolean ret_query = gst_pad_peer_query(GST_BASE_SINK_CAST(xvimagesink)->sinkpad, query);
    ret = gst_structure_get_double (query_structure, "mcv-r-x", &scsa_xml_metadata.mcv_r_x);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set mcv-r-x");
	ret = gst_structure_get_double (query_structure, "mcv-r-y", &scsa_xml_metadata.mcv_r_y);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set mcv-r-y");
	ret = gst_structure_get_double (query_structure, "mcv-g-x", &scsa_xml_metadata.mcv_g_x);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set mcv-g-x");
	ret = gst_structure_get_double (query_structure, "mcv-g-y", &scsa_xml_metadata.mcv_g_y);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set mcv-g-y");
	ret = gst_structure_get_double (query_structure, "mcv-b-x", &scsa_xml_metadata.mcv_b_x);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set mcv-b-x");
	ret = gst_structure_get_double (query_structure, "mcv-b-y", &scsa_xml_metadata.mcv_b_y);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set mcv-b-y");
	ret = gst_structure_get_double (query_structure, "mcv-w-x", &scsa_xml_metadata.mcv_w_x);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set mcv-w-x");
	ret = gst_structure_get_double (query_structure, "mcv-w-y", &scsa_xml_metadata.mcv_w_y);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set mcv-w-y");
	ret = gst_structure_get_double (query_structure, "mcv-l-min", &scsa_xml_metadata.mcv_l_min);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set mcv-l-min");
	ret = gst_structure_get_double (query_structure, "mcv-l-max", &scsa_xml_metadata.mcv_l_max);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set mcv-l-max");
	ret = gst_structure_get_int (query_structure, "lightlevel-contentmax", &scsa_xml_metadata.lightlevel_contentmax);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set lightlevel-contentmax");
	ret = gst_structure_get_int (query_structure, "lightlevel-frameaveragemax", &scsa_xml_metadata.lightlevel_frameaveragemax);
    if(!ret)
    	GST_ERROR_OBJECT(xvimagesink, "Fail to set lightlevel-frameaveragemax");

    xvimagesink->hdr_xml_metadata = scsa_xml_metadata;

	ret = avoc_pass_sei_xml_metadata(&xvimagesink->hdr_xml_metadata, sizeof(scsa_xml_metadata));
	if (ret != AVOC_EXIT_SUCCESS)
	{
		GST_ERROR_OBJECT(xvimagesink, "avoc_set_xml_metadata_for_HDR failed, ret[ %d ]", ret);
		return FALSE;
	}

    if (query)
      gst_query_unref(query);

	return ret;
}
#endif
