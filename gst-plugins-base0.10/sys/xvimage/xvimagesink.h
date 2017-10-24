/* GStreamer
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
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

#ifndef __GST_XVIMAGESINK_H__
#define __GST_XVIMAGESINK_H__

#include <gst/video/gstvideosink.h>

#ifdef HAVE_XSHM
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif /* HAVE_XSHM */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_XSHM
#include <X11/extensions/XShm.h>
#endif /* HAVE_XSHM */

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#ifdef GST_EXT_XV_ENHANCEMENT
#include <X11/Xatom.h>
#include <stdio.h>
#endif

//#include <avoc_avsink.h>		// New requirement,  related with avoc_set_resolution

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <avoc.h>
#include <sys/playermixer/player_mixer.h> 


#ifdef USE_TBM_SDK
  #define DISPLAY_BUFFER_NUM			1			 // Display Buffer Num: 0->0->0
#else
  #define DISPLAY_BUFFER_NUM			3			// Display Buffer Num: 0->1->2->0
#endif
#define PRE_BUFFER_NUM				1			//(1 or 2) Rotate Buffer num:  90 or 180 or 270
#define PRE_DISPLAY_BUFFER_NUM	3			//(3 or 2) DP only has 4 buffer, Display Buffer Num: 0->1->2->0
#define ENABLE_PERFORMANCE_CHECKING		0
#define ENABLE_VF_ROTATE        		1   	 		// Line:  HW Frame-> AP(1920x1080)->MP->Xvideo, should def ENABLE_TBM
#define ENABLE_RT_DISPLAY			1			// For Cloud Game(SW Path) and MHEG future(HW Path)
#define ENABLE_RT_SEAMLESS_HW_SCALER	0
#define ENABLE_RT_SEAMLESS_GA_SCALER		1
#define ENABLE_HW_TBM_SCALING_CHECKING	0

//#define DUMP_SW_DATA
//#define DUMP_HW_DATA
//#define DUMP_ROTATE_HW_DATA
//#define DUMP_ROTATE_DATA
//#define DUMP_HW_DISCONTINUS_DATA

#include <tbm_bufmgr.h>
#if ENABLE_VF_ROTATE
#include "videoframerotate.h"
typedef struct VideoFrameRotateContext VFRotateContext;
typedef struct VIDEO_FRAME VFame;
#define MAX_SUPPORTED_ROTATED_WIDTH  606
#define MAX_SUPPORTED_ROTATED_HEIGHT 1080
#define MAX_SUPORTTED_RESOLUTION_W 1920
#define MAX_SUPORTTED_RESOLUTION_H 1080
#define MAX_SUPPORTED_ROTATED_DISPLAY_X 657		//(MAX_SUPORTTED_RESOLUTION_W - MAX_SUPPORTED_ROTATED_WIDTH) / 2
#define MAX_SUPPORTED_ROTATED_DISPLAY_Y 0
#endif

G_BEGIN_DECLS

#define GST_TYPE_XVIMAGESINK \
  (gst_xvimagesink_get_type())
#define GST_XVIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_XVIMAGESINK, GstXvImageSink))
#define GST_XVIMAGESINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_XVIMAGESINK, GstXvImageSinkClass))
#define GST_IS_XVIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_XVIMAGESINK))
#define GST_IS_XVIMAGESINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_XVIMAGESINK))

#ifdef GST_EXT_XV_ENHANCEMENT
#define XV_SCREEN_SIZE_WIDTH 4096
#define XV_SCREEN_SIZE_HEIGHT 4096

#define MAX_PIXMAP_NUM 10
typedef uint (*get_pixmap_callback)(void *user_data);
typedef struct _GstXPixmap GstXPixmap;
#endif /* GST_EXT_XV_ENHANCEMENT */

typedef struct _GstXContext GstXContext;
typedef struct _GstXWindow GstXWindow;
typedef struct _GstXvImageFormat GstXvImageFormat;
typedef struct _GstXvImageBuffer GstXvImageBuffer;
typedef struct _GstXvImageBufferClass GstXvImageBufferClass;

typedef struct _GstXvImageSink GstXvImageSink;
typedef struct _GstXvImageSinkClass GstXvImageSinkClass;

/*
 * GstXContext:
 * @disp: the X11 Display of this context
 * @screen: the default Screen of Display @disp
 * @screen_num: the Screen number of @screen
 * @visual: the default Visual of Screen @screen
 * @root: the root Window of Display @disp
 * @white: the value of a white pixel on Screen @screen
 * @black: the value of a black pixel on Screen @screen
 * @depth: the color depth of Display @disp
 * @bpp: the number of bits per pixel on Display @disp
 * @endianness: the endianness of image bytes on Display @disp
 * @width: the width in pixels of Display @disp
 * @height: the height in pixels of Display @disp
 * @widthmm: the width in millimeters of Display @disp
 * @heightmm: the height in millimeters of Display @disp
 * @par: the pixel aspect ratio calculated from @width, @widthmm and @height,
 * @heightmm ratio
 * @use_xshm: used to known wether of not XShm extension is usable or not even
 * if the Extension is present
 * @xv_port_id: the XVideo port ID
 * @im_format: used to store at least a valid format for XShm calls checks
 * @formats_list: list of supported image formats on @xv_port_id
 * @channels_list: list of #GstColorBalanceChannels
 * @caps: the #GstCaps that Display @disp can accept
 *
 * Structure used to store various informations collected/calculated for a
 * Display.
 */
struct _GstXContext {
  Display *disp;

  Screen *screen;
  gint screen_num;

  Visual *visual;

  Window root;

  gulong white, black;

  gint depth;
  gint bpp;
  gint endianness;

  gint width, height;
  gint widthmm, heightmm;
  GValue *par;                  /* calculated pixel aspect ratio */

  gboolean use_xshm;

  XvPortID xv_port_id;
  guint nb_adaptors;
  gchar ** adaptors;
  gint im_format;

  GList *formats_list;
  GList *channels_list;

  GstCaps *caps;

  /* Optimisation storage for buffer_alloc return */
  GstCaps *last_caps;
  gint last_format;
  gint last_width;
  gint last_height;
};

/*
 * GstXWindow:
 * @win: the Window ID of this X11 window
 * @width: the width in pixels of Window @win
 * @height: the height in pixels of Window @win
 * @internal: used to remember if Window @win was created internally or passed
 * through the #GstXOverlay interface
 * @gc: the Graphical Context of Window @win
 *
 * Structure used to store informations about a Window.
 */
struct _GstXWindow {
  Window win;
#ifdef GST_EXT_XV_ENHANCEMENT
  gint x, y;
#endif
  gint width, height;
  gboolean internal;
  GC gc;
};

#ifdef GST_EXT_XV_ENHANCEMENT
struct _GstXPixmap {
	Window pixmap;
	gint x, y;
	gint width, height;
	GC gc;
};
#endif

/**
 * GstXvImageFormat:
 * @format: the image format
 * @caps: generated #GstCaps for this image format
 *
 * Structure storing image format to #GstCaps association.
 */
struct _GstXvImageFormat {
  gint format;
  GstCaps *caps;
};

/**
 * GstXImageBuffer:
 * @xvimagesink: a reference to our #GstXvImageSink
 * @xvimage: the XvImage of this buffer
 * @width: the width in pixels of XvImage @xvimage
 * @height: the height in pixels of XvImage @xvimage
 * @im_format: the image format of XvImage @xvimage
 * @size: the size in bytes of XvImage @xvimage
 *
 * Subclass of #GstBuffer containing additional information about an XvImage.
 */
struct _GstXvImageBuffer {
  GstBuffer   buffer;

  /* Reference to the xvimagesink we belong to */
  GstXvImageSink *xvimagesink;

  XvImage *xvimage;

#ifdef HAVE_XSHM
  XShmSegmentInfo SHMInfo;
#endif /* HAVE_XSHM */

  gint width, height, im_format;
  size_t size;
  void *pTBMinfo[DISPLAY_BUFFER_NUM];
  gint dsp_idx;
  void *pTBMinfo_web;
};

#ifndef USE_TBM_SDK
//For HDR setting
typedef struct
{
	double mcv_r_x;
	double mcv_r_y;
	double mcv_g_x;
	double mcv_g_y;
	double mcv_b_x;
	double mcv_b_y;
	double mcv_w_x;
	double mcv_w_y;
	double mcv_l_min;
	double mcv_l_max;
	int lightlevel_contentmax;
	int lightlevel_frameaveragemax;
}HDRMetadate; 
#endif

#ifdef GST_EXT_XV_ENHANCEMENT
#define MAX_PLANE_NUM          4
#endif /* GST_EXT_XV_ENHANCEMENT */

/**
 * GstXvImageSink:
 * @display_name: the name of the Display we want to render to
 * @xcontext: our instance's #GstXContext
 * @xwindow: the #GstXWindow we are rendering to
 * @xvimage: internal #GstXvImage used to store incoming buffers and render when
 * not using the buffer_alloc optimization mechanism
 * @cur_image: a reference to the last #GstXvImage that was put to @xwindow. It
 * is used when Expose events are received to redraw the latest video frame
 * @event_thread: a thread listening for events on @xwindow and handling them
 * @running: used to inform @event_thread if it should run/shutdown
 * @fps_n: the framerate fraction numerator
 * @fps_d: the framerate fraction denominator
 * @x_lock: used to protect X calls as we are not using the XLib in threaded
 * mode
 * @flow_lock: used to protect data flow routines from external calls such as
 * events from @event_thread or methods from the #GstXOverlay interface
 * @par: used to override calculated pixel aspect ratio from @xcontext
 * @pool_lock: used to protect the buffer pool
 * @image_pool: a list of #GstXvImageBuffer that could be reused at next buffer
 * allocation call
 * @synchronous: used to store if XSynchronous should be used or not (for
 * debugging purpose only)
 * @keep_aspect: used to remember if reverse negotiation scaling should respect
 * aspect ratio
 * @handle_events: used to know if we should handle select XEvents or not
 * @brightness: used to store the user settings for color balance brightness
 * @contrast: used to store the user settings for color balance contrast
 * @hue: used to store the user settings for color balance hue
 * @saturation: used to store the user settings for color balance saturation
 * @cb_changed: used to store if the color balance settings where changed
 * @video_width: the width of incoming video frames in pixels
 * @video_height: the height of incoming video frames in pixels
 *
 * The #GstXvImageSink data structure.
 */
struct _GstXvImageSink {
  /* Our element stuff */
  GstVideoSink videosink;

  char *display_name;
  guint adaptor_no;

  GstXContext *xcontext;
  GstXWindow *xwindow;
  GstXvImageBuffer *xvimage;
  GstXvImageBuffer *cur_image;

  GThread *event_thread;
  gboolean running;

  gint fps_n;
  gint fps_d;

  GMutex *x_lock;
  GMutex *flow_lock;

  /* object-set pixel aspect ratio */
  GValue *par;

  GMutex *pool_lock;
  gboolean pool_invalid;
  GSList *image_pool;

  gboolean synchronous;
  gboolean double_buffer;
  gboolean keep_aspect;
  gboolean redraw_border;
  gboolean handle_events;
  gboolean handle_expose;

  gint brightness;
  gint contrast;
  gint hue;
  gint saturation;
  gboolean cb_changed;

  /* size of incoming video, used as the size for XvImage */
  guint video_width, video_height;

  /* max size of incoming video */
  guint max_video_width, max_video_height;

  /* display sizes, used for clipping the image */
  gint disp_x, disp_y;
  gint disp_width, disp_height;
  gfloat disp_x_ratio, disp_y_ratio;
  gfloat disp_width_ratio, disp_height_ratio;  
  gboolean crop_flag; /*video cropping*/

  /* port attributes */
  gboolean autopaint_colorkey;
  gint colorkey;
  
  gboolean draw_borders;
  
  /* port features */
  gboolean have_autopaint_colorkey;
  gboolean have_colorkey;
  gboolean have_double_buffer;
  
  /* stream metadata */
  gchar *media_title;

  /* target video rectangle */
  GstVideoRectangle render_rect;
  gboolean have_render_rect;

#ifdef GST_EXT_XV_ENHANCEMENT
  /* display mode */
  gint pre_bitrate; //add for updata realtime bitrate
  gint new_bitrate;

  guint sei_metadata_size;
  guint sei_metadata_alloc_size;
  guchar* sei_metadata;

  gboolean sei_mdcv_filled;
  guchar* sei_mdcv;

  gint cur_scantype; //add for updata realtime scantype

  gboolean xid_updated;
  gint orientation;
  guint display_mode;
  guint display_geometry_method;
  guint flip;
  gboolean support_rotation;
  guint rotate_angle;
  guint rotate_setting_angle;
  gboolean visible;
  gfloat zoom;
  guint rotation;
  guint rotate_cnt;
  GstVideoRectangle dst_roi;
  XImage* xim_transparenter;
  guint scr_w, scr_h;
  gboolean stop_video;
  gboolean is_hided;
  /* needed if fourcc is one if S series */
  guint aligned_width;
  guint aligned_height;
  gint drm_fd;
  unsigned int gem_handle[MAX_PLANE_NUM];
  /* for using multiple pixmaps */
  GstXPixmap *xpixmap[MAX_PIXMAP_NUM];
  gint current_pixmap_idx;
  get_pixmap_callback get_pixmap_cb;
  void* get_pixmap_cb_user_data;
  void*  old_get_pixmap_cb;
#endif /* GST_EXT_XV_ENHANCEMENT */
  guint scaler_id;

  gboolean user_enable_rotation;
  gboolean is_uhd_fit;
#if ENABLE_VF_ROTATE
  VFRotateContext *frctx;
  gboolean is_rotate_opened;
  gint vf_rotate_degree;
  gint vf_current_degree;
  gint vf_rotate_setting_degree;
  gint vf_iFrameDone;
  gboolean  vf_bIsVideoRotationEnabled;
  gint vf_iOriginalWidth;
  gint vf_iOriginalHeight;
  gint vf_iCodecId;
  gboolean vf_bIsInterlacedScanType;
  gint vf_iFramesPerSec;
  gint vf_iCurrentWidth;
  gint vf_iCurrentHeight;
  gint vf_iScaledWidth;
  gint vf_iScaledHeight;
  VFame *vf_sOutputFrame;
  tbm_bufmgr vf_display_bufmgr;
  tbm_bo vf_display_boY, vf_display_boCbCr;
  tbm_bo_handle vf_display_bo_hnd_Y, vf_display_bo_hnd_CbCr;
  gint vf_display_idx;
  tbm_bo vf_pre_display_boY[PRE_DISPLAY_BUFFER_NUM][PRE_BUFFER_NUM], vf_pre_display_boCbCr[PRE_DISPLAY_BUFFER_NUM][PRE_BUFFER_NUM];		// 0: 90 / 270, 1: 180 / 0
  tbm_bo_handle vf_pre_display_bo_hnd_Y[PRE_DISPLAY_BUFFER_NUM][PRE_BUFFER_NUM], vf_pre_display_bo_hnd_CbCr[PRE_DISPLAY_BUFFER_NUM][PRE_BUFFER_NUM];
  gint vf_pre_display_idx_n;
  gint vf_degree_idx;
  gint vf_pre_display_idx[PRE_DISPLAY_BUFFER_NUM][PRE_BUFFER_NUM];
  gint vf_pre_allocate_done;
  gint vf_display_drm_fd;
  gint vf_display_channel;		// current display channel, same with butype 0: HW path 1: SW path
  gint ARdegree;	//auto rotation degree from mov tkhd box, if 0: enable the user rotate, else disable the user rotate.
  gint CANRotate; // CANRotate 0: not support, 1: support,   for judgment video codec format rotation
  gint vf_need_update_display_idx;
  gint vf_force_unmute;	// for skip rotate frame case, video unmute failed.
#endif
#if ENABLE_RT_DISPLAY
  gint rt_display_vaule;	// 0, HW path, 1, SW path
  gint rt_display_avoc_done;
  gint rt_display_drm_fd;
  tbm_bufmgr rt_display_bufmgr;
  tbm_bo rt_display_boY[DISPLAY_BUFFER_NUM], rt_display_boCbCr[DISPLAY_BUFFER_NUM];
  tbm_bo_handle rt_display_bo_hnd_Y[DISPLAY_BUFFER_NUM], rt_display_bo_hnd_CbCr[DISPLAY_BUFFER_NUM];
  gint rt_display_idx_n;
  gint rt_display_idx[DISPLAY_BUFFER_NUM];
  gint rt_allocate_done;
  gint rt_channel_setting_done;
  gint rt_resetinfo_done;
#endif
  gint device_id;
  gboolean is_first_putimage;
  gboolean avoc_video_started;
  gint dp_linesize;
  guint mute_flag;
  gint custom_x;
  gint custom_y;
  gint custom_w;
  gint custom_h;
  gint zoom_x;
  gint zoom_y;
  gint zoom_w;
  gint zoom_h;
  RESResolution_k resolution;
  GstVideoRectangle src_input;
  GstVideoRectangle dest_Rect;
  gboolean seamless_resolution_change;
  gint video_quality_mode;
  gint avoc_source;

  gboolean need_update_color_info;
  avoc_video_data_format_e prev_color_info_format;
  avoc_color_space_info prev_color_info;
  int par_x; //aspect ratio
  int par_y;
  
  gint netflix_display_par_x;
  gint netflix_display_par_y;
  gint dps_display_width;
  gint dps_display_height;
  gboolean set_pixmap;

  gint mode_3d;

#ifndef USE_TBM_SDK
  HDRMetadate hdr_xml_metadata;  //For HDR setting
#endif

#ifdef USE_TBM_SDK
void *p_sdk_TBMinfo[DISPLAY_BUFFER_NUM];
gint sdk_dsp_idx;
#endif
#ifdef DUMP_SW_DATA
  int is_check_sw_dump_filename_done;
  char dump_sw_filename[1024];
#endif
#ifdef DUMP_HW_DATA
  int is_check_hw_dump_filename_done;
  char dump_hw_filename[1024];
  char dump_hw_org_filename[1024];
#endif

  gint set_hw_rotate_degree; // rotation degree to set for hw video rotation from application
  gint get_hw_rotate_degree; // get rotation degree from driver
  gboolean can_support_hw_rotate;
  gboolean enable_hw_rotate_support;
  gint hw_rotate_scaled_width;
  gint hw_rotate_scaled_height;
  gint hw_rotate_degree; // mapped to enum values
  gint curr_hw_rotate_degree;
  gint prev_hw_rotate_degree;
  gboolean is_hw_rotate_degree_changed;
  gboolean is_unmute_req_for_hw_rotate;
  gboolean is_hw_rotate_on_mixed_frame;
  gboolean is_player_mixer_support_enabled;

  int video_still;
};

#ifdef GST_EXT_XV_ENHANCEMENT
/* max plane count *********************************************************/
#define MPLANE_IMGB_MAX_COUNT         (4)

/* image buffer definition ***************************************************

    +------------------------------------------+ ---
    |                                          |  ^
    |     uaddr[], index[]                     |  |
    |     +---------------------------+ ---    |  |
    |     |                           |  ^     |  |
    |     |<-------- width[] -------->|  |     |  |
    |     |                           |  |     |  |
    |     |                           |        |
    |     |                           |height[]|elevation[]
    |     |                           |        |
    |     |                           |  |     |  |
    |     |                           |  |     |  |
    |     |                           |  v     |  |
    |     +---------------------------+ ---    |  |
    |                                          |  v
    +------------------------------------------+ ---

    |<----------------- stride[] ------------------>|
*/
typedef struct _GstMultiPlaneImageBuffer GstMultiPlaneImageBuffer;
struct _GstMultiPlaneImageBuffer
{
    GstBuffer buffer;

    /* width of each image plane */
    gint      width[MPLANE_IMGB_MAX_COUNT];
    /* height of each image plane */
    gint      height[MPLANE_IMGB_MAX_COUNT];
    /* stride of each image plane */
    gint      stride[MPLANE_IMGB_MAX_COUNT];
    /* elevation of each image plane */
    gint      elevation[MPLANE_IMGB_MAX_COUNT];
    /* user space address of each image plane */
    gpointer uaddr[MPLANE_IMGB_MAX_COUNT];
    /* Index of real address of each image plane, if needs */
    gpointer index[MPLANE_IMGB_MAX_COUNT];
    /* left postion, if needs */
    gint      x;
    /* top position, if needs */
    gint      y;
    /* to align memory */
    gint      __dummy2;
    /* arbitrary data */
    gint      data[16];
    gboolean   set_pixmap;
};
#endif /* GST_EXT_XV_ENHANCEMENT */

struct _GstXvImageSinkClass {
  GstVideoSinkClass parent_class;
};

GType gst_xvimagesink_get_type(void);

G_END_DECLS

#endif /* __GST_XVIMAGESINK_H__ */
