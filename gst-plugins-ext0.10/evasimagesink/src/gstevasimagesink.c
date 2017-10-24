/*
 * evasimagesink
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Sangchul Lee <sc11.lee@samsung.com>
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

/**
 * SECTION:element-evasimagesink
 * Gstreamer Evas Video Sink - draw video on the given Evas Image Object
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include <Evas.h>
#include <Ecore.h>
#include <Ecore_X.h>

#include "gstevasimagesink.h"

#define CAP_WIDTH "width"
#define CAP_HEIGHT "height"

GST_DEBUG_CATEGORY_STATIC (gst_evas_image_sink_debug);
#define GST_CAT_DEFAULT gst_evas_image_sink_debug

/* Enumerations */
enum
{
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_EVAS_OBJECT,
	PROP_EVAS_OBJECT_SHOW,
#ifdef USE_NATIVE_BUFFER
	PROP_ROTATE_ANGLE,
	PROP_DISPLAY_GEOMETRY_METHOD,
	PROP_ENABLE_FLUSH_BUFFER,
	PROP_FLIP
#endif
};

enum
{
	UPDATE_FALSE,
	UPDATE_TRUE
};

#define COLOR_DEPTH 4
#define GL_X11_ENGINE "gl_x11"
#define DO_RENDER_FROM_FIMC 1
#define SIZE_FOR_UPDATE_VISIBILITY sizeof(gchar)
#ifdef USE_NATIVE_BUFFER
#define MAX_ECOREPIPE_BUFFER_CNT 4
#define DEBUGLOG_DEFAULT_COUNT 8
#define SIZE_FOR_NATIVE_INDEX sizeof(gint)

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
	/* dma buf fd */
	int fd[SCMN_IMGB_MAX_PLANE];
	/* buffer share method */
	int buf_share_method;
	/* Y plane size in case of ST12 */
	int y_size;
	/* UV plane size in case of ST12 */
	int uv_size;
	/* Tizen buffer object */
	void *bo[SCMN_IMGB_MAX_PLANE];
	/* JPEG data */
	void *jpeg_data;
	/* JPEG size */
	int jpeg_size;
	/* TZ memory buffer */
	int tz_enable;
} SCMN_IMGB;
#endif

#define EVASIMAGESINK_SET_EVAS_OBJECT_EVENT_CALLBACK( x_evas_image_object, x_usr_data ) \
do \
{ \
	if (x_evas_image_object) { \
		GST_LOG("object callback add"); \
		evas_object_event_callback_add (x_evas_image_object, EVAS_CALLBACK_DEL, evas_callback_del_event, x_usr_data); \
		evas_object_event_callback_add (x_evas_image_object, EVAS_CALLBACK_RESIZE, evas_callback_resize_event, x_usr_data); \
	} \
}while(0)

#define EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK( x_evas_image_object ) \
do \
{ \
	if (x_evas_image_object) { \
		GST_LOG("object callback del"); \
		evas_object_event_callback_del (x_evas_image_object, EVAS_CALLBACK_DEL, evas_callback_del_event); \
		evas_object_event_callback_del (x_evas_image_object, EVAS_CALLBACK_RESIZE, evas_callback_resize_event); \
	} \
}while(0)

#ifdef USE_NATIVE_BUFFER
#define EVASIMAGESINK_SET_EVAS_EVENT_CALLBACK( x_evas, x_usr_data ) \
do \
{ \
	if (x_evas) { \
		GST_DEBUG("callback add... evas_callback_render_pre.. evas : %p esink : %p", x_evas, x_usr_data); \
		evas_event_callback_add (x_evas, EVAS_CALLBACK_RENDER_PRE, evas_callback_render_pre, x_usr_data); \
	} \
}while(0)

#define EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK( x_evas ) \
do \
{ \
	if (x_evas) { \
		GST_DEBUG("callback del... evas_callback_render_pre"); \
		evas_event_callback_del (x_evas, EVAS_CALLBACK_RENDER_PRE, evas_callback_render_pre); \
	} \
}while(0)
#endif

GMutex *instance_lock;
guint instance_lock_count;

static inline gboolean
is_evas_image_object (Evas_Object *obj)
{
	const char *type;
	if (!obj) {
		return FALSE;
	}
	type = evas_object_type_get (obj);
	if (!type) {
		return FALSE;
	}
	if (strcmp (type, "image") == 0) {
		return TRUE;
	}
	return FALSE;
}

#ifdef USE_NATIVE_BUFFER
gint
gst_evas_image_sink_ref_count (GstBuffer * buf)
{
	return GST_OBJECT_REFCOUNT_VALUE(GST_BUFFER_CAST(buf));
}
#endif

/* the capabilities of the inputs.
 *
 * BGRx format
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
#ifdef USE_FIMCC
		GST_STATIC_CAPS (GST_VIDEO_CAPS_BGRx));
#endif
#ifdef USE_NATIVE_BUFFER
		GST_STATIC_CAPS(GST_VIDEO_CAPS_YUV ("I420") ";"
						GST_VIDEO_CAPS_YUV ("NV12") ";"
						GST_VIDEO_CAPS_YUV ("SN12"))
);
#endif
GST_BOILERPLATE (GstEvasImageSink, gst_evas_image_sink, GstVideoSink, GST_TYPE_VIDEO_SINK);

static void gst_evas_image_sink_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_evas_image_sink_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean gst_evas_image_sink_set_caps (GstBaseSink *base_sink, GstCaps *caps);
static GstFlowReturn gst_evas_image_sink_show_frame (GstVideoSink *video_sink, GstBuffer *buf);
static gboolean gst_evas_image_sink_event (GstBaseSink *sink, GstEvent *event);
static GstStateChangeReturn gst_evas_image_sink_change_state (GstElement *element, GstStateChange transition);
static void evas_callback_del_event (void *data, Evas *e, Evas_Object *obj, void *event_info);
static void evas_callback_resize_event (void *data, Evas *e, Evas_Object *obj, void *event_info);
#ifdef USE_NATIVE_BUFFER
static void evas_callback_render_pre (void *data, Evas *e, void *event_info);
static GstFlowReturn gst_esink_epipe_reset(GstEvasImageSink *esink);
static gboolean gst_esink_make_flush_buffer(GstEvasImageSink *esink);
static void _release_flush_buffer(GstEvasImageSink *esink);
static void gst_evas_image_sink_update_geometry (GstEvasImageSink *esink, GstVideoRectangle *result);
static void gst_evas_image_sink_apply_geometry (GstEvasImageSink *esink);
#endif
static void
gst_evas_image_sink_base_init (gpointer gclass)
{
	GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

	gst_element_class_set_details_simple (element_class,
		"EvasImageSink",
		"VideoSink",
		"Video sink element for evas image object",
		"Samsung Electronics <www.samsung.com>");

	gst_element_class_add_pad_template (element_class,
	gst_static_pad_template_get (&sink_factory));
}

static void
gst_evas_image_sink_class_init (GstEvasImageSinkClass *klass)
{
	GObjectClass *gobject_class;
	GstBaseSinkClass *gstbasesink_class;
	GstVideoSinkClass *gstvideosink_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstbasesink_class = GST_BASE_SINK_CLASS (klass);
	gstvideosink_class = GST_VIDEO_SINK_CLASS (klass);
	gstelement_class = (GstElementClass *) klass;

	gobject_class->set_property = gst_evas_image_sink_set_property;
	gobject_class->get_property = gst_evas_image_sink_get_property;

	g_object_class_install_property (gobject_class, PROP_EVAS_OBJECT,
		g_param_spec_pointer ("evas-object", "Destination Evas Object",	"Destination evas image object", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_EVAS_OBJECT_SHOW,
		g_param_spec_boolean ("visible", "Show Evas Object", "When disabled, evas object does not show", TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#ifdef USE_NATIVE_BUFFER
	g_object_class_install_property(gobject_class, PROP_ROTATE_ANGLE,
		g_param_spec_int("rotate", "Rotate angle", "Rotate angle of display output", DEGREE_0, DEGREE_NUM, DEGREE_0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property(gobject_class, PROP_DISPLAY_GEOMETRY_METHOD,
		g_param_spec_int("display-geometry-method", "Display geometry method",
			"Geometrical method for display", DISP_GEO_METHOD_LETTER_BOX, DISP_GEO_METHOD_NUM, DISP_GEO_METHOD_LETTER_BOX, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_ENABLE_FLUSH_BUFFER,
		g_param_spec_boolean("enable-flush-buffer", "Enable flush buffer mechanism",
			"Enable flush buffer mechanism when state change(PAUSED_TO_READY)",
			TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property(gobject_class, PROP_FLIP,
		g_param_spec_int("flip", "Display flip",
			"Flip for display", FLIP_NONE, FLIP_NUM, FLIP_NONE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif
	gstvideosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_evas_image_sink_show_frame);
	gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_evas_image_sink_set_caps);
	gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_evas_image_sink_event);
	gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_evas_image_sink_change_state);
}

static void
gst_evas_image_sink_fini (gpointer data, GObject *obj)
{
	GST_DEBUG ("[ENTER]");

	GstEvasImageSink *esink = GST_EVASIMAGESINK (obj);
	if (!esink) {
		return;
	}
#ifdef USE_FIMCC
	if (esink->oldbuf) {
		gst_buffer_unref (esink->oldbuf);
	}
#endif

	if (esink->eo) {
#ifdef USE_NATIVE_BUFFER
		EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK (evas_object_evas_get(esink->eo));
		GST_DEBUG("unset EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK esink : %p, esink->eo : %x", esink, esink->eo);
#endif
		EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK (esink->eo);
		GST_DEBUG("unset EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK esink : %p, esink->eo : %x", esink, esink->eo);
		evas_object_image_data_set(esink->eo, NULL);
	}
#ifdef USE_NATIVE_BUFFER
	if (esink->display_buffer_lock) {
		g_mutex_free (esink->display_buffer_lock);
		esink->display_buffer_lock = NULL;
	}
	if (esink->flow_lock) {
		g_mutex_free (esink->flow_lock);
		esink->flow_lock = NULL;
	}
	if(esink->n_provider)
		native_buffer_provider_destroy(esink->n_provider);
	esink->eo = NULL;
	esink->epipe = NULL;
#endif
	g_mutex_lock (instance_lock);
	instance_lock_count--;
	g_mutex_unlock (instance_lock);
	if (instance_lock_count == 0) {
		g_mutex_free (instance_lock);
		instance_lock = NULL;
	}

	GST_DEBUG ("[LEAVE]");
}

#ifdef USE_NATIVE_BUFFER
static void
gst_evas_image_sink_reset (GstEvasImageSink *esink)
{
	int i = 0;
	tbm_bo_handle bo_handle;

	GST_DEBUG("gst_evas_image_sink_reset start");

	g_mutex_lock(esink->display_buffer_lock);

	for(i=0; i<NATIVE_BUFFER_NUM; i++)
	{
		if(esink->displaying_buffer[i].n_buffer)
		{
			native_buffer_destroy(esink->displaying_buffer[i].n_buffer);
			esink->displaying_buffer[i].n_buffer = NULL;
		}
		if(esink->displaying_buffer[i].buffer)
		{
			if(esink->displaying_buffer[i].ref_count)
			{
				if(esink->displaying_buffer[i].bo) {
					bo_handle = tbm_bo_map(esink->displaying_buffer[i].bo, TBM_DEVICE_CPU, TBM_OPTION_READ|TBM_OPTION_WRITE);
					if (!bo_handle.ptr) {
						GST_WARNING("failed to map bo [%p]", esink->displaying_buffer[i].bo);
					}
					else
						tbm_bo_unmap(esink->displaying_buffer[i].bo);
				}
				else
					GST_WARNING("there is no bo information. so skip to map bo");

				GST_WARNING("[reset] unreffing gst %p", esink->displaying_buffer[i].buffer);

				while(esink->displaying_buffer[i].ref_count)
				{
					GST_WARNING("index[%d]'s buffer ref count=%d",i,gst_evas_image_sink_ref_count (esink->displaying_buffer[i].buffer));
					esink->displaying_buffer[i].ref_count--;
					gst_buffer_unref(esink->displaying_buffer[i].buffer);
				}
			}
			esink->displaying_buffer[i].buffer = NULL;
		}
		if(esink->displaying_buffer[i].bo)
			esink->displaying_buffer[i].bo = NULL;
	}
	esink->prev_buf = NULL;
	esink->prev_index = -1;
	esink->cur_index = -1;

	esink->eo_size.x = esink->eo_size.y =
	esink->eo_size.w = esink->eo_size.h = 0;
	esink->use_ratio = FALSE;
	esink->sent_buffer_cnt = 0;

	g_mutex_unlock(esink->display_buffer_lock);
}
#endif

#ifdef USE_FIMCC
static void
evas_image_sink_cb_pipe (void *data, void *buffer, unsigned int nbyte)
#endif
#ifdef USE_NATIVE_BUFFER
static void
evas_image_sink_cb_pipe (void *data, int *buffer_index, unsigned int nbyte)
#endif
{
	GstEvasImageSink *esink = data;
#ifdef USE_FIMCC
	GstBuffer *buf;
	void *img_data;
#endif
	GST_DEBUG ("[ENTER]");
	if (!esink || !esink->eo) {
		GST_WARNING ("esink : %p, or eo is NULL returning", esink);
		return;
	}
	GST_LOG("esink : %p, esink->eo : %x", esink, esink->eo);
	if (nbyte == SIZE_FOR_UPDATE_VISIBILITY) {
		if(!esink->object_show) {
			evas_object_hide(esink->eo);
			GST_INFO ("object hide..");
		} else {
			evas_object_show(esink->eo);
			GST_INFO ("object show..");
		}
		GST_DEBUG ("[LEAVE]");
		return;
	}
#ifdef USE_FIMCC
	if (!buffer || nbyte != sizeof (GstBuffer *)) {
		GST_WARNING ("buffer %p, nbyte : %d, sizeof(GstBuffer *) : %d", buffer, nbyte, sizeof (GstBuffer *));
		return;
	}
#endif
#ifdef USE_NATIVE_BUFFER
	int index = 0;
	index = *buffer_index;
	if ((index<0 || index>=NATIVE_BUFFER_NUM) || nbyte != SIZE_FOR_NATIVE_INDEX)	{
		GST_WARNING ("index : %d, nbyte : %d", index, nbyte);
		return;
	}
#endif
	if (GST_STATE(esink) < GST_STATE_PAUSED) {
		GST_WARNING ("WRONG-STATE(%d) for rendering, skip this frame", GST_STATE(esink));
		return;
	}

#ifdef USE_FIMCC
	memcpy (&buf, buffer, sizeof (GstBuffer *));
	if (!buf) {
		GST_ERROR ("There is no buffer");
		return;
	}
	if (esink->present_data_addr == -1) {
		/* if present_data_addr is -1, we don't use this member variable */
	} else if (esink->present_data_addr != DO_RENDER_FROM_FIMC) {
		GST_WARNING ("skip rendering this buffer, present_data_addr:%d, DO_RENDER_FROM_FIMC:%d", esink->present_data_addr, DO_RENDER_FROM_FIMC);
		return;
	}
#endif
#ifdef USE_NATIVE_BUFFER
	g_mutex_lock(esink->display_buffer_lock);
	if(esink->native_buffer_format == NATIVE_BUFFER_FORMAT_NV12) {
		if (!esink->displaying_buffer[index].n_buffer) {
			GST_ERROR("the index's nbuffer was already NULL, so return");
			g_mutex_unlock(esink->display_buffer_lock);
			return;
		}
		GST_LOG("received (bo %p, gst %p) index num : %d", esink->displaying_buffer[index].bo, esink->displaying_buffer[index].buffer, index);
	} else if(esink->native_buffer_format == NATIVE_BUFFER_FORMAT_I420) {
		GST_LOG("received (bo %p) index num : %d", esink->displaying_buffer[index].bo, index);
	}

	Evas_Native_Surface surf;
	surf.type = EVAS_NATIVE_SURFACE_TIZEN;
	surf.version = EVAS_NATIVE_SURFACE_VERSION;
	surf.data.tizen.buffer = esink->displaying_buffer[index].n_buffer;
	surf.data.tizen.rot = esink->rotate_angle;
	surf.data.tizen.flip = esink->flip;

	GST_LOG("received (bo %p, gst %p) index num : %d", esink->displaying_buffer[index].bo, esink->displaying_buffer[index].buffer, index);

	GstVideoRectangle result = {0};

	evas_object_geometry_get(esink->eo, &esink->eo_size.x, &esink->eo_size.y, &esink->eo_size.w, &esink->eo_size.h);
	if (!esink->eo_size.w || !esink->eo_size.h) {
		GST_ERROR ("there is no information for evas object size");
		g_mutex_unlock(esink->display_buffer_lock);
		return;
	}
	gst_evas_image_sink_update_geometry(esink, &result);
	if(!result.w || !result.h)
	{
		GST_ERROR("no information about geometry (%d, %d)", result.w, result.h);
		g_mutex_unlock(esink->display_buffer_lock);
		return;
	}

	if(esink->use_ratio)
	{
		surf.data.tizen.ratio = (float) esink->w / esink->h;
		GST_LOG("set ratio for letter mode");
	}
	evas_object_size_hint_align_set(esink->eo, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_size_hint_weight_set(esink->eo, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	if ( !esink->is_evas_object_size_set && esink->w > 0 && esink->h > 0) {
		evas_object_image_size_set(esink->eo, esink->w, esink->h);
		esink->is_evas_object_size_set = TRUE;
	}
    evas_object_image_native_surface_set(esink->eo, &surf);
	GST_DEBUG("native surface set finish");

	if(result.x || result.y)
		GST_LOG("coordinate x, y (%d, %d) for locating video to center", result.x, result.y);
	evas_object_image_fill_set(esink->eo, result.x, result.y, result.w, result.h);

	evas_object_image_pixels_dirty_set(esink->eo, EINA_TRUE);
	GST_DEBUG_OBJECT (esink, "GEO_METHOD : src(%dx%d), dst(%dx%d), dst_x(%d), dst_y(%d), rotate(%d), flip(%d)",
		esink->w, esink->h, esink->eo_size.w, esink->eo_size.h, esink->eo_size.x, esink->eo_size.y, esink->rotate_angle, esink->flip);

	/* unref previous buffer */
	if(esink->native_buffer_format == NATIVE_BUFFER_FORMAT_NV12) {
		if(esink->prev_buf && esink->displaying_buffer[esink->prev_index].ref_count)
		{
			GST_DEBUG("before index %d's ref_count =%d, gst_buf %p",esink->prev_index,esink->displaying_buffer[esink->prev_index].ref_count, esink->prev_buf);
			esink->displaying_buffer[esink->prev_index].ref_count--;
			GST_DEBUG("after index %d's ref_count =%d, gst_buf %p",esink->prev_index,esink->displaying_buffer[esink->prev_index].ref_count, esink->prev_buf);
			/* Print debug log for 8 frame */
			if(esink->debuglog_cnt_ecoreCbPipe > 0)
			{
				GST_WARNING("(%d) ecore_cb_pipe unref index[%d] .. gst_buf %p", DEBUGLOG_DEFAULT_COUNT-(esink->debuglog_cnt_ecoreCbPipe), esink->prev_index, esink->prev_buf);
				esink->debuglog_cnt_ecoreCbPipe--;
			}
			if (esink->sent_buffer_cnt == MAX_ECOREPIPE_BUFFER_CNT)
				GST_WARNING("sent buffer cnt 4->3 so skip will be stop");

			esink->sent_buffer_cnt--;
			GST_DEBUG("prev gst_buffer %p's unref Start!!", esink->prev_buf);
			gst_buffer_unref(esink->prev_buf);
			GST_DEBUG("prev gst_buffer %p's unref End!!", esink->prev_buf);
		} else {
			GST_DEBUG("ref_count=%d  unref prev gst_buffer %p", esink->displaying_buffer[esink->prev_index].ref_count,esink->prev_buf);
		}

		GST_DEBUG("Current gst_buf %p and index=%d is overwrited to Prev gst_buf %p & index %d",
			esink->displaying_buffer[index].buffer, index, esink->prev_buf, esink->prev_index );
		esink->prev_buf = esink->displaying_buffer[index].buffer;
		esink->prev_index = index;
	}else if(esink->native_buffer_format == NATIVE_BUFFER_FORMAT_I420) {
		/* Print debug log for 8 frame */
		if(esink->debuglog_cnt_ecoreCbPipe > 0)
		{
			GST_WARNING("(%d) ecore_cb_pipe set native surface [%d]  n_buffer[%p]",
				DEBUGLOG_DEFAULT_COUNT-(esink->debuglog_cnt_ecoreCbPipe), index, esink->displaying_buffer[index].n_buffer);
			esink->debuglog_cnt_ecoreCbPipe--;
		}
		if (esink->sent_buffer_cnt == MAX_ECOREPIPE_BUFFER_CNT)
			GST_WARNING("sent buffer cnt 4->3 so skip will be stop");

		esink->sent_buffer_cnt--;
	}
#endif
#ifdef USE_FIMCC
	if ( !esink->is_evas_object_size_set && esink->w > 0 && esink->h > 0) {
			evas_object_image_size_set (esink->eo, esink->w, esink->h);
			GST_DEBUG("evas_object_image_size_set(), width(%d),height(%d)", esink->w, esink->h);
			esink->is_evas_object_size_set = TRUE;
	}
	if (esink->gl_zerocopy) {
		img_data = evas_object_image_data_get (esink->eo, EINA_TRUE);
		if (!img_data || !GST_BUFFER_DATA(buf)) {
			GST_WARNING ("Cannot get image data from evas object or cannot get gstbuffer data");
			evas_object_image_data_set(esink->eo, img_data);
		} else {
			GST_DEBUG ("img_data(%x), GST_BUFFER_DATA(buf):%x, esink->w(%d),esink->h(%d), esink->eo(%x)",img_data,GST_BUFFER_DATA(buf),esink->w,esink->h,esink->eo);
			memcpy (img_data, GST_BUFFER_DATA (buf), esink->w * esink->h * COLOR_DEPTH);
			evas_object_image_pixels_dirty_set (esink->eo, 1);
			evas_object_image_data_set(esink->eo, img_data);
		}
		gst_buffer_unref (buf);
	} else {
		GST_DEBUG ("GST_BUFFER_DATA(buf):%x, esink->eo(%x)",GST_BUFFER_DATA(buf),esink->eo);
		evas_object_image_data_set (esink->eo, GST_BUFFER_DATA (buf));
		evas_object_image_pixels_dirty_set (esink->eo, 1);
		if (esink->oldbuf) {
			gst_buffer_unref(esink->oldbuf);
		}
		esink->oldbuf = buf;
	}
#endif
#ifdef USE_NATIVE_BUFFER
	g_mutex_unlock(esink->display_buffer_lock);
#endif
	GST_DEBUG ("[LEAVE]");
}
#ifdef USE_NATIVE_BUFFER
static void
gst_evas_image_sink_update_geometry (GstEvasImageSink *esink, GstVideoRectangle *result)
{
	if (!esink || !esink->eo) {
		GST_WARNING("there is no esink");
		return;
	}

	result->x = 0;
	result->y = 0;

	switch (esink->display_geometry_method)
	{
		case DISP_GEO_METHOD_LETTER_BOX:	// 0
			/* set black padding for letter box mode */
			GST_DEBUG("letter box mode");
			esink->use_ratio = TRUE;
			result->w = esink->eo_size.w;
			result->h = esink->eo_size.h;
			break;
		case DISP_GEO_METHOD_ORIGIN_SIZE:	// 1
			GST_DEBUG("origin size mode");
			esink->use_ratio = FALSE;
			/* set coordinate for each case */
			result->x = (esink->eo_size.w-esink->w) / 2;
			result->y = (esink->eo_size.h-esink->h) / 2;
			result->w = esink->w;
			result->h = esink->h;
			break;
		case DISP_GEO_METHOD_FULL_SCREEN:	// 2
			GST_DEBUG("full screen mode");
			esink->use_ratio = FALSE;
			result->w = esink->eo_size.w;
			result->h = esink->eo_size.h;
			break;
		case DISP_GEO_METHOD_CROPPED_FULL_SCREEN:       // 3
			GST_DEBUG("cropped full screen mode");
			esink->use_ratio = FALSE;
			/* compare evas object's ratio with video's */
			if((esink->eo_size.w/esink->eo_size.h) > (esink->w/esink->h))
			{
				result->w = esink->eo_size.w;
				result->h = esink->eo_size.w * esink->h / esink->w;
				result->y = -(result->h-esink->eo_size.h) / 2;
			}
			else
			{
				result->w = esink->eo_size.h * esink->w / esink->h;
				result->h = esink->eo_size.h;
				result->x = -(result->w-esink->eo_size.w) / 2;
			}
			break;
		case DISP_GEO_METHOD_ORIGIN_SIZE_OR_LETTER_BOX:	// 4
			GST_DEBUG("origin size or letter box mode");
			/* if video size is smaller than evas object's, it will be set to origin size mode */
			if((esink->eo_size.w > esink->w) && (esink->eo_size.h > esink->h))
			{
				GST_DEBUG("origin size mode");
				esink->use_ratio = FALSE;
				/* set coordinate for each case */
				result->x = (esink->eo_size.w-esink->w) / 2;
				result->y = (esink->eo_size.h-esink->h) / 2;
				result->w = esink->w;
				result->h = esink->h;
			}
			else
			{
				GST_DEBUG("letter box mode");
				esink->use_ratio = TRUE;
				result->w = esink->eo_size.w;
				result->h = esink->eo_size.h;
			}
			break;
		default:
			GST_WARNING("unsupported mode.");
			break;
	}
	GST_DEBUG("geometry result [%d, %d, %d, %d]", result->x, result->y, result->w, result->h);
}
static void
gst_evas_image_sink_apply_geometry (GstEvasImageSink *esink)
{
	if (!esink || !esink->eo) {
		GST_WARNING("there is no esink");
		return;
	}

	Evas_Native_Surface *surf = evas_object_image_native_surface_get(esink->eo);
	GstVideoRectangle result = {0};

	if(surf)
	{
		GST_DEBUG("native surface exists");
		surf->data.tizen.rot = esink->rotate_angle;
		surf->data.tizen.flip = esink->flip;
		evas_object_image_native_surface_set(esink->eo, surf);

		gst_evas_image_sink_update_geometry(esink, &result);

		if(esink->use_ratio)
		{
			surf->data.tizen.ratio = (float) esink->w / esink->h;
			GST_LOG("set ratio for letter mode");
		}

		if(result.x || result.y)
			GST_LOG("coordinate x, y (%d, %d) for locating video to center", result.x, result.y);

		evas_object_image_fill_set(esink->eo, result.x, result.y, result.w, result.h);
	}
	else
		GST_WARNING("there is no surf");
}
#endif

static void
gst_evas_image_sink_init (GstEvasImageSink *esink, GstEvasImageSinkClass *gclass)
{
	GST_DEBUG ("[ENTER]");

	esink->eo = NULL;
	esink->epipe = NULL;
	esink->object_show = FALSE;
	esink->update_visibility = UPDATE_FALSE;
	esink->is_evas_object_size_set = FALSE;
#ifdef USE_FIMCC
	esink->gl_zerocopy = FALSE;
	esink->present_data_addr = -1;
#endif
#ifdef USE_NATIVE_BUFFER
	esink->display_buffer_lock = g_mutex_new ();
	esink->flow_lock = g_mutex_new ();
	esink->n_provider = native_buffer_provider_create (NATIVE_BUFFER_PROVIDER_CORE);
	int i = 0;
	for (i=0; i<NATIVE_BUFFER_NUM; i++)
	{
		esink->displaying_buffer[i].n_buffer = NULL;
		esink->displaying_buffer[i].buffer = NULL;
		esink->displaying_buffer[i].bo = NULL;
		esink->displaying_buffer[i].ref_count = 0;
	}
	esink->prev_buf = NULL;
	esink->prev_index = -1;
	esink->cur_index = -1;
	esink->enable_flush_buffer = TRUE;
	esink->need_flush = FALSE;
	esink->display_geometry_method = DISP_GEO_METHOD_LETTER_BOX;
	esink->flip = FLIP_NONE;
	esink->eo_size.x = esink->eo_size.y =
	esink->eo_size.w = esink->eo_size.h = 0;
	esink->use_ratio = FALSE;
	esink->sent_buffer_cnt = 0;
	esink->debuglog_cnt_showFrame = DEBUGLOG_DEFAULT_COUNT;
	esink->debuglog_cnt_ecoreCbPipe = DEBUGLOG_DEFAULT_COUNT;

	esink->src_buf_idx = 0;
	esink->is_buffer_allocated = FALSE;
#endif
	if(!instance_lock) {
		instance_lock = g_mutex_new();
	}
	g_mutex_lock (instance_lock);
	instance_lock_count++;
	g_mutex_unlock (instance_lock);

	g_object_weak_ref (G_OBJECT (esink), gst_evas_image_sink_fini, NULL);

	GST_DEBUG ("[LEAVE]");
}

static void
evas_callback_del_event (void *data, Evas *e, Evas_Object *obj, void *event_info)
{
	GST_DEBUG ("[ENTER]");

	GstEvasImageSink *esink = data;
	if (!esink) {
		return;
	}

#ifdef USE_FIMCC
	if (esink->oldbuf) {
		gst_buffer_unref (esink->oldbuf);
		esink->oldbuf = NULL;
	}
#endif
	if (esink->eo) {
#ifdef USE_NATIVE_BUFFER
		EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK (evas_object_evas_get(esink->eo));
		GST_DEBUG("unset EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK esink : %p, esink->eo : %x", esink, esink->eo);
#endif
		EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK (esink->eo);
		GST_DEBUG("unset EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK esink : %p, esink->eo : %x", esink, esink->eo);

		evas_object_image_data_set(esink->eo, NULL);
		esink->eo = NULL;
	}

	GST_DEBUG ("[LEAVE]");
}

static void
evas_callback_resize_event (void *data, Evas *e, Evas_Object *obj, void *event_info)
{
	int x, y, w, h;
	x = y = w = h = 0;

	GST_DEBUG ("[ENTER]");

	GstEvasImageSink *esink = data;
	if (!esink || !esink->eo) {
		return;
	}

	evas_object_geometry_get(esink->eo, &x, &y, &w, &h);
	if (!w || !h) {
		GST_WARNING ("evas object size (w:%d,h:%d) was not set", w, h);
	} else {
#ifdef USE_NATIVE_BUFFER
		esink->eo_size.x = x;
		esink->eo_size.y = y;
		esink->eo_size.w = w;
		esink->eo_size.h = h;
		GST_WARNING ("resize (x:%d, y:%d, w:%d, h:%d)", x, y, w, h);
		gst_evas_image_sink_apply_geometry(esink);
#endif
#ifdef USE_FIMCC
		evas_object_image_fill_set(esink->eo, 0, 0, w, h);
		GST_DEBUG ("evas object fill set (w:%d,h:%d)", w, h);
#endif
	}

	GST_DEBUG ("[LEAVE]");
}
#ifdef USE_NATIVE_BUFFER
static void
evas_callback_render_pre (void *data, Evas *e, void *event_info)
{
	GstEvasImageSink *esink = data;
	if (!esink || !esink->eo) {
		GST_WARNING("there is no esink info.... esink : %p, or eo is NULL returning", esink);
		return;
	}
	if(esink->need_flush && esink->flush_buffer)
	{
		g_mutex_lock(esink->display_buffer_lock);
		Evas_Native_Surface surf;
		GstVideoRectangle result = {0};

		evas_object_geometry_get(esink->eo, &esink->eo_size.x, &esink->eo_size.y, &esink->eo_size.w, &esink->eo_size.h);
		if (!esink->eo_size.w || !esink->eo_size.h) {
			GST_ERROR ("there is no information for evas object size");
			return;
		}
		gst_evas_image_sink_update_geometry(esink, &result);
		if(!result.w || !result.h)
		{
			GST_ERROR("no information about geometry (%d, %d)", result.w, result.h);
			return;
		}

		if(esink->use_ratio)
		{
			surf.data.tizen.ratio = (float) esink->w / esink->h;
			GST_LOG("set ratio for letter mode");
		}
		evas_object_size_hint_align_set(esink->eo, EVAS_HINT_FILL, EVAS_HINT_FILL);
		evas_object_size_hint_weight_set(esink->eo, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		if ( !esink->is_evas_object_size_set && esink->w > 0 && esink->h > 0) {
			evas_object_image_size_set(esink->eo, esink->w, esink->h);
			esink->is_evas_object_size_set = TRUE;
		}

		if(result.x || result.y)
			GST_LOG("coordinate x, y (%d, %d) for locating video to center", result.x, result.y);

		evas_object_image_fill_set(esink->eo, result.x, result.y, result.w, result.h);

		surf.type = EVAS_NATIVE_SURFACE_TIZEN;
		surf.version = EVAS_NATIVE_SURFACE_VERSION;
		surf.data.tizen.buffer = esink->flush_buffer->n_buffer;
		surf.data.tizen.rot = esink->rotate_angle;
		surf.data.tizen.flip = esink->flip;
		GST_DEBUG("use esink->flush buffer->n_buffer (%p), rotate(%d), flip(%d)",
			esink->flush_buffer->n_buffer, esink->rotate_angle, esink->flip);
		evas_object_image_native_surface_set(esink->eo, &surf);
		g_mutex_unlock(esink->display_buffer_lock);
		esink->need_flush = FALSE;
	}
	evas_object_image_pixels_dirty_set (esink->eo, EINA_TRUE);
	GST_LOG("dirty set finish");
}

static gboolean
gst_evas_image_sink_release_source_buffer(GstEvasImageSink *esink)
{
	int i = 0;
	tbm_bo_handle bo_handle;
	g_mutex_lock(esink->display_buffer_lock);

	for(i = 0; i < NATIVE_BUFFER_NUM; i++) {
		GST_WARNING("[reset] reset gst %p", esink->displaying_buffer[i].buffer);
		esink->displaying_buffer[i].bo = NULL;
		esink->displaying_buffer[i].n_buffer = NULL;
		esink->displaying_buffer[i].ref_count = 0;
	}

	for(i = 0; i < SOURCE_BUFFER_NUM; i++) {
		if(esink->src_buffer_info[i].bo) {
			tbm_bo_unmap(esink->src_buffer_info[i].bo);
			esink->src_buffer_info[i].bo = NULL;
		}

		if (esink->src_buffer_info[i].n_buffer) {
			native_buffer_destroy(esink->src_buffer_info[i].n_buffer);
			esink->src_buffer_info[i].n_buffer = NULL;
		}
	}

	if(esink->n_provider) {
		native_buffer_provider_destroy(esink->n_provider);
		esink->n_provider = NULL;
	}
	esink->is_buffer_allocated = FALSE;
	esink->src_buf_idx = 0;
	esink->prev_buf = NULL;
	esink->prev_index = -1;
	esink->cur_index = -1;

	esink->eo_size.x = esink->eo_size.y =
	esink->eo_size.w = esink->eo_size.h = 0;
	esink->use_ratio = FALSE;
	esink->sent_buffer_cnt = 0;

	g_mutex_unlock(esink->display_buffer_lock);

	return TRUE;
}

static gboolean
gst_evas_image_sink_allocate_source_buffer(GstEvasImageSink *esink)
{
	int size = 0;
	int idx = 0;
	tbm_bo bo;
	tbm_bo_handle vaddr;
	GstFlowReturn ret=GST_FLOW_OK;

	if (esink == NULL) {
		GST_ERROR("handle is NULL");
		return FALSE;
	}

	/* create native buffer provider for making native buffer */
	if(esink->n_provider) {
		GST_INFO("Native buffer provider already exists. Skipping creating one.");
	} else {
		esink->n_provider = native_buffer_provider_create (NATIVE_BUFFER_PROVIDER_CORE);
		if(!esink->n_provider) {
			GST_ERROR("n_provider is NULL!!");
			goto ALLOC_FAILED;
		}
	}

	for(idx=0; idx < SOURCE_BUFFER_NUM; idx++) {
		if(!esink->src_buffer_info[idx].n_buffer) {
			/* create native buffer */
			esink->src_buffer_info[idx].n_buffer = native_buffer_create(esink->n_provider, esink->w, esink->h, 0, esink->native_buffer_format,
				NATIVE_BUFFER_USAGE_CPU|NATIVE_BUFFER_USAGE_3D_TEXTURE);
		}
		if(!esink->src_buffer_info[idx].n_buffer)
		{
			GST_ERROR("n_buffer is NULL!!");
			goto ALLOC_FAILED;
		}

		/* get bo and size */
		bo = native_buffer_get_bo(esink->src_buffer_info[idx].n_buffer);
		size = tbm_bo_size(bo);
		if(!bo || !size)
		{
			GST_ERROR("bo(%p), size(%d)", bo, size);
			goto ALLOC_FAILED;
		}
		esink->src_buffer_info[idx].bo = bo;

		vaddr = tbm_bo_map(bo, TBM_DEVICE_CPU, TBM_OPTION_READ|TBM_OPTION_WRITE);
		if (!vaddr.ptr) {
			GST_WARNING_OBJECT(esink, "get vaddr failed pointer = %p", vaddr.ptr);
			tbm_bo_unmap(bo);
			goto ALLOC_FAILED;
		} else {
			memset (vaddr.ptr, 0x0, size);
			GST_LOG_OBJECT (esink, "tbm_bo_map(VADDR) finished, bo(%p), vaddr(%p)", bo, vaddr.ptr);
		}

		esink->src_buffer_info[idx].usr_addr = vaddr.ptr;

		GST_WARNING_OBJECT(esink, "src buffer index:%d , native buffer : %p", idx, esink->src_buffer_info[idx].n_buffer);
	}

	return TRUE;

ALLOC_FAILED:
	gst_evas_image_sink_release_source_buffer(esink);
	return FALSE;
}
#endif

static int
evas_image_sink_get_size_from_caps (GstCaps *caps, int *w, int *h)
{
	gboolean r;
	int width, height;
	GstStructure *s;

	if (!caps || !w || !h) {
		return -1;
	}
	s = gst_caps_get_structure (caps, 0);
	if (!s) {
		return -1;
	}

	r = gst_structure_get_int (s, CAP_WIDTH, &width);
	if (r == FALSE) {
		GST_DEBUG("fail to get width from caps");
		return -1;
	}

	r = gst_structure_get_int (s, CAP_HEIGHT, &height);
	if (r == FALSE) {
		GST_DEBUG("fail to get height from caps");
		return -1;
	}

	*w = width;
	*h = height;
	GST_DEBUG ("size w(%d), h(%d)", width, height);

	return 0;
}
#ifdef USE_FIMCC
static gboolean
is_zerocopy_supported (Evas *e)
{
	Eina_List *engines, *l;
	int cur_id;
	int id;
	char *name;

	if (!e) {
		return FALSE;
	}

	engines = evas_render_method_list ();
	if (!engines) {
		return FALSE;
	}

	cur_id = evas_output_method_get (e);

	EINA_LIST_FOREACH (engines, l, name) {
		id = evas_render_method_lookup (name);
		if (name && id == cur_id) {
			if (!strcmp (name, GL_X11_ENGINE)) {
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

static int
evas_image_sink_event_parse_data (GstEvasImageSink *esink, GstEvent *event)
{
	const GstStructure *st;
	guint st_data_addr = 0;
	gint st_data_width = 0;
	gint st_data_height = 0;

	g_return_val_if_fail (event != NULL, FALSE);
	g_return_val_if_fail (esink != NULL, FALSE);

	if (GST_EVENT_TYPE (event) != GST_EVENT_CUSTOM_DOWNSTREAM_OOB) {
		GST_WARNING ("it's not a custom downstream oob event");
		return -1;
	}
	st = gst_event_get_structure (event);
	if (st == NULL || !gst_structure_has_name (st, "GstStructureForCustomEvent")) {
		GST_WARNING ("structure in a given event is not proper");
		return -1;
	}
	if (!gst_structure_get_uint (st, "data-addr", &st_data_addr)) {
		GST_WARNING ("parsing data-addr failed");
		return -1;
	}
	esink->present_data_addr = st_data_addr;

	return 0;
}
#endif
static gboolean
gst_evas_image_sink_event (GstBaseSink *sink, GstEvent *event)
{
#ifdef USE_FIMCC
	GstEvasImageSink *esink = GST_EVASIMAGESINK (sink);
	GstMessage *msg;
	gchar *str;
#endif
	switch (GST_EVENT_TYPE (event)) {
		case GST_EVENT_FLUSH_START:
			GST_DEBUG ("GST_EVENT_FLUSH_START");
			break;
		case GST_EVENT_FLUSH_STOP:
			GST_DEBUG ("GST_EVENT_FLUSH_STOP");
			break;
		case GST_EVENT_EOS:
			GST_DEBUG ("GST_EVENT_EOS");
			break;
#ifdef USE_FIMCC
		case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
			if(!evas_image_sink_event_parse_data(esink, event)) {
				GST_DEBUG ("GST_EVENT_CUSTOM_DOWNSTREAM_OOB, present_data_addr:%x",esink->present_data_addr);
			} else {
				GST_ERROR ("evas_image_sink_event_parse_data() failed");
			}
			break;
#endif
		default:
			break;
	}
	if (GST_BASE_SINK_CLASS (parent_class)->event) {
		return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
	} else {
		return TRUE;
	}
}

static GstStateChangeReturn
gst_evas_image_sink_change_state (GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret_state = GST_STATE_CHANGE_SUCCESS;
	GstEvasImageSink *esink = NULL;
	GstFlowReturn ret=GST_FLOW_OK;
	esink = GST_EVASIMAGESINK(element);

	if(!esink) {
		GST_ERROR("can not get evasimagesink from element");
		return GST_STATE_CHANGE_FAILURE;
	}

	switch (transition) {
		case GST_STATE_CHANGE_NULL_TO_READY:
			GST_WARNING ("*** STATE_CHANGE_NULL_TO_READY ***");
#ifdef USE_NATIVE_BUFFER
			g_mutex_lock (esink->flow_lock);
			if (!is_evas_image_object (esink->eo)) {
				GST_ERROR_OBJECT (esink, "There is no evas image object..");
				g_mutex_unlock (esink->flow_lock);
				return GST_STATE_CHANGE_FAILURE;
			}
			g_mutex_unlock (esink->flow_lock);
#endif
			break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			GST_WARNING ("*** STATE_CHANGE_READY_TO_PAUSED ***");
			break;
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
#ifdef USE_NATIVE_BUFFER
			/* Print debug log for 8 frame */
			esink->debuglog_cnt_showFrame = DEBUGLOG_DEFAULT_COUNT;
			esink->debuglog_cnt_ecoreCbPipe = DEBUGLOG_DEFAULT_COUNT;
#endif
			GST_WARNING ("*** STATE_CHANGE_PAUSED_TO_PLAYING ***");
			break;
		default:
			break;
	}

	ret_state = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	switch (transition) {
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
			GST_WARNING ("*** STATE_CHANGE_PLAYING_TO_PAUSED ***");
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			GST_WARNING ("*** STATE_CHANGE_PAUSED_TO_READY ***");
#ifdef USE_NATIVE_BUFFER
			Eina_Bool r;
			/* flush buffer, we will copy last buffer to keep image data and reset buffer list */
			GST_WARNING("esink->enable_flush_buffer : %d", esink->enable_flush_buffer);
			if(esink->enable_flush_buffer && esink->native_buffer_format == NATIVE_BUFFER_FORMAT_NV12)
			{
				 if (gst_esink_make_flush_buffer(esink) == FALSE) {
					ret = gst_esink_epipe_reset(esink);
					if(ret) {
						GST_ERROR_OBJECT(esink, "evas epipe reset ret=%d, need to check",ret);
						return GST_STATE_CHANGE_FAILURE;
					}
					gst_evas_image_sink_reset(esink);
				}
			}
			else {
				ret = gst_esink_epipe_reset(esink);
				if(ret) {
					GST_ERROR_OBJECT(esink, "evas epipe reset ret=%d, need to check",ret);
					return GST_STATE_CHANGE_FAILURE;
				}
				if(esink->native_buffer_format == NATIVE_BUFFER_FORMAT_NV12) {
					gst_evas_image_sink_reset(esink);
				} else if(esink->native_buffer_format == NATIVE_BUFFER_FORMAT_I420) {
					gst_evas_image_sink_release_source_buffer(esink);
				}
			}
#endif
			break;
		case GST_STATE_CHANGE_READY_TO_NULL:
			GST_WARNING ("*** STATE_CHANGE_READY_TO_NULL ***");
#ifdef USE_NATIVE_BUFFER
			if(esink->flush_buffer)
				_release_flush_buffer(esink);
			EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK (evas_object_evas_get(esink->eo));
#endif
			EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK (esink->eo);
			if (esink->epipe) {
				GST_DEBUG("ecore-pipe will delete");
				ecore_pipe_del (esink->epipe);
				esink->epipe = NULL;
			}
			break;
		default:
			break;
	}

	return ret_state;
}

static void
gst_evas_image_sink_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GstEvasImageSink *esink = GST_EVASIMAGESINK (object);
	Evas_Object *eo;

	g_mutex_lock (instance_lock);

	switch (prop_id) {
	case PROP_EVAS_OBJECT:
		eo = g_value_get_pointer (value);
		if (is_evas_image_object (eo)) {
			if (eo != esink->eo) {
				Eina_Bool r;
#ifdef USE_NATIVE_BUFFER
				EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK (evas_object_evas_get(esink->eo));
#endif
				/* delete evas object callbacks registrated on a previous evas image object */
				EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK (esink->eo);
				esink->eo = eo;
				/* add evas object callbacks on a new evas image object */
				EVASIMAGESINK_SET_EVAS_OBJECT_EVENT_CALLBACK (esink->eo, esink);
#ifdef USE_NATIVE_BUFFER
				GST_WARNING("register render callback [esink : %p, esink->eo : %x]", esink, esink->eo);
				EVASIMAGESINK_SET_EVAS_EVENT_CALLBACK (evas_object_evas_get(esink->eo), esink);
				evas_object_geometry_get(esink->eo, &esink->eo_size.x, &esink->eo_size.y, &esink->eo_size.w, &esink->eo_size.h);
				GST_WARNING ("evas object size (x:%d, y:%d, w:%d, h:%d)", esink->eo_size.x, esink->eo_size.y, esink->eo_size.w, esink->eo_size.h);
#endif
#ifdef USE_FIMCC
				esink->gl_zerocopy = is_zerocopy_supported (evas_object_evas_get (eo));
				if (esink->gl_zerocopy) {
					evas_object_image_content_hint_set (esink->eo, EVAS_IMAGE_CONTENT_HINT_DYNAMIC);
					GST_DEBUG("Enable gl zerocopy");
				}
#endif
				GST_DEBUG("Evas Image Object(%x) is set", esink->eo);
				esink->is_evas_object_size_set = FALSE;
				esink->object_show = TRUE;
				esink->update_visibility = UPDATE_TRUE;
				if (esink->epipe) {
					r = ecore_pipe_write (esink->epipe, &esink->update_visibility, SIZE_FOR_UPDATE_VISIBILITY);
					if (r == EINA_FALSE)  {
						GST_WARNING ("Failed to ecore_pipe_write() for updating visibility\n");
					}
				}
			}
		} else {
			GST_ERROR ("Cannot set evas-object property: value is not an evas image object");
		}
		break;

	case PROP_EVAS_OBJECT_SHOW:
	{
		Eina_Bool r;
		esink->object_show = g_value_get_boolean (value);
		if( !is_evas_image_object(esink->eo) ) {
			GST_WARNING ("Cannot apply visible(show-object) property: cannot get an evas object\n");
			break;
		}
		esink->update_visibility = UPDATE_TRUE;
		GST_LOG("esink->update_visibility : %d", esink->update_visibility);
		if (esink->epipe) {
			r = ecore_pipe_write (esink->epipe, &esink->update_visibility, SIZE_FOR_UPDATE_VISIBILITY);
			if (r == EINA_FALSE)  {
				GST_WARNING ("Failed to ecore_pipe_write() for updating visibility)\n");
			}
		}
		break;
	}
#ifdef USE_NATIVE_BUFFER
	case PROP_ROTATE_ANGLE:
	{
		int rotate = 0;
		rotate = g_value_get_int (value);
		switch(rotate)
		{
			case DEGREE_0:
				esink->rotate_angle = 0;
				break;
			case DEGREE_90:
				esink->rotate_angle = 90;
				break;
			case DEGREE_180:
				esink->rotate_angle = 180;
				break;
			case DEGREE_270:
				esink->rotate_angle = 270;
				break;
			default:
				break;
		}
		GST_INFO("update rotate_angle : %d", esink->rotate_angle);
		break;
	}
	case PROP_DISPLAY_GEOMETRY_METHOD:
	{
		Eina_Bool r;
		guint geometry = g_value_get_int (value);
		if (esink->display_geometry_method != geometry) {
			esink->display_geometry_method = geometry;
			GST_INFO_OBJECT (esink, "Overlay geometry method update, display_geometry_method(%d)", esink->display_geometry_method);
		}

		g_mutex_lock(esink->display_buffer_lock);

		if(esink->cur_index!=-1 && esink->epipe)
		{
			GST_WARNING("apply property esink->cur_index =%d",esink->cur_index);
			esink->displaying_buffer[esink->cur_index].ref_count++;
			gst_buffer_ref(esink->displaying_buffer[esink->cur_index].buffer);
			esink->sent_buffer_cnt++;
			r = ecore_pipe_write (esink->epipe, &esink->cur_index, SIZE_FOR_NATIVE_INDEX);

			if (r == EINA_FALSE)  {
                              GST_WARNING("ecore_pipe_write fail");
				  esink->displaying_buffer[esink->cur_index].ref_count--;
				  gst_buffer_unref(esink->displaying_buffer[esink->cur_index].buffer);
				  esink->sent_buffer_cnt--;
                     }
		}
		g_mutex_unlock(esink->display_buffer_lock);
		break;
	}
	case PROP_ENABLE_FLUSH_BUFFER:
		esink->enable_flush_buffer = g_value_get_boolean(value);
		break;
	case PROP_FLIP:
	{
		esink->flip = g_value_get_int(value);
		GST_INFO("update flip : %d", esink->flip);
		break;
	}
#endif
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	g_mutex_unlock (instance_lock);
}

static void
gst_evas_image_sink_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GstEvasImageSink *esink = GST_EVASIMAGESINK (object);

	switch (prop_id) {
	case PROP_EVAS_OBJECT:
		g_value_set_pointer (value, esink->eo);
		break;
	case PROP_EVAS_OBJECT_SHOW:
		g_value_set_boolean (value, esink->object_show);
		break;
#ifdef USE_NATIVE_BUFFER
	case PROP_ROTATE_ANGLE:
		g_value_set_int (value, esink->rotate_angle);
		break;
	case PROP_DISPLAY_GEOMETRY_METHOD:
		g_value_set_int (value, esink->display_geometry_method);
		break;
	case PROP_ENABLE_FLUSH_BUFFER:
		g_value_set_boolean(value, esink->enable_flush_buffer);
		break;
	case PROP_FLIP:
		g_value_set_int(value, esink->flip);
		break;
#endif
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
gst_evas_image_sink_set_caps (GstBaseSink *base_sink, GstCaps *caps)
{
	int r;
	int w, h;
	GstEvasImageSink *esink = GST_EVASIMAGESINK (base_sink);
	GstStructure *structure = NULL;
	guint32 format = 0;

	esink->is_evas_object_size_set = FALSE;
	r = evas_image_sink_get_size_from_caps (caps, &w, &h);
	if (!r) {
		esink->w = w;
		esink->h = h;
		GST_DEBUG ("set size w(%d), h(%d)", w, h);
	}

	structure = gst_caps_get_structure (caps, 0);
	if (!structure) {
		return FALSE;
	}

	if (!gst_structure_get_fourcc (structure, "format", &format)) {
		GST_DEBUG ("format(dst) is not set...it may be rgb series");
	}

	GST_DEBUG_OBJECT(esink, "source color format is %" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (format));

#ifdef USE_NATIVE_BUFFER
	if (format == GST_MAKE_FOURCC('S','N','1','2') || format == GST_MAKE_FOURCC('N','V','1','2') ) {
		esink->native_buffer_format = NATIVE_BUFFER_FORMAT_NV12;
	} else if (format == GST_MAKE_FOURCC('I','4','2','0')){
		esink->native_buffer_format = NATIVE_BUFFER_FORMAT_I420;
	} else {
		GST_ERROR("cannot parse fourcc format from caps.");
		return FALSE;
	}
#endif

	return TRUE;
}

static GstFlowReturn
gst_evas_image_sink_show_frame (GstVideoSink *video_sink, GstBuffer *buf)
{
	GstEvasImageSink *esink = GST_EVASIMAGESINK (video_sink);
	Eina_Bool r;

	GST_LOG("[ENTER] show frame");
#ifdef USE_NATIVE_BUFFER
	if (!gst_evas_image_sink_ref_count (buf))
	{
		GST_WARNING("ref count is 0.. skip show frame");
		return GST_FLOW_OK;
	}
#endif
	g_mutex_lock (instance_lock);
#ifdef USE_FIMCC
	if (esink->present_data_addr == -1) {
		/* if present_data_addr is -1, we don't use this member variable */
	} else if (esink->present_data_addr != DO_RENDER_FROM_FIMC) {
		GST_WARNING ("skip rendering this buffer, present_data_addr:%d, DO_RENDER_FROM_FIMC:%d", esink->present_data_addr, DO_RENDER_FROM_FIMC);
		g_mutex_unlock (instance_lock);
		return GST_FLOW_OK;
	}
#endif
	if (!esink->epipe) {
		esink->epipe = ecore_pipe_add ((Ecore_Pipe_Cb)evas_image_sink_cb_pipe, esink);
		if (!esink->epipe) {
			GST_ERROR ("ecore-pipe create failed");
			g_mutex_unlock (instance_lock);
			return GST_FLOW_ERROR;
		}
		GST_DEBUG("ecore-pipe create success");
	}
#ifdef USE_NATIVE_BUFFER
	int i = 0;
	int index = -1;
	SCMN_IMGB *scmn_imgb = NULL;
	gboolean exist_bo = FALSE;

	GST_LOG ("BUF: gst_buf= %p ts= %" GST_TIME_FORMAT " size= %lu,  mallocdata= %p",
		buf, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP (buf)), GST_BUFFER_SIZE (buf), GST_BUFFER_MALLOCDATA(buf));

	if(esink->native_buffer_format == NATIVE_BUFFER_FORMAT_NV12) {
		/* get received buffer informations */
		scmn_imgb = (SCMN_IMGB *)GST_BUFFER_MALLOCDATA(buf);
		if (!scmn_imgb) {
			GST_WARNING("scmn_imgb is NULL. Skip..." );
			g_mutex_unlock (instance_lock);
			return GST_FLOW_OK;
		}
		if (scmn_imgb->buf_share_method == BUF_SHARE_METHOD_TIZEN_BUFFER)
		{
			/* check whether bo is new or not */
			for(i=0; i<NATIVE_BUFFER_NUM; i++)
			{
				if(esink->displaying_buffer[i].bo==scmn_imgb->bo[0])
				{
					GST_DEBUG("it is already saved bo %p (index num : %d)", esink->displaying_buffer[i].bo, i);
					index = i;
					exist_bo = TRUE;
					break;
				}
				else
					exist_bo = FALSE;
			}
			/* keep bo */
			if(!exist_bo)
			{
				/* find empty buffer space for indexing */
				for(i=0; i<NATIVE_BUFFER_NUM; i++)
				{
					if(!esink->displaying_buffer[i].bo)
					{
						index = i;
						break;
					}
				}
				if(index!=-1)
				{
					/* keep informations */
					esink->displaying_buffer[index].buffer = buf;
					esink->displaying_buffer[index].bo = scmn_imgb->bo[0];
					GST_WARNING("TBM bo %p / gst_buf %p", esink->displaying_buffer[index].bo, esink->displaying_buffer[index].buffer);

					/* create new native buffer */
					if(!esink->n_provider)
					{
						GST_WARNING("native buffer provide don't exist... create it");
						esink->n_provider = native_buffer_provider_create (NATIVE_BUFFER_PROVIDER_CORE);
					}
					esink->displaying_buffer[index].n_buffer = native_buffer_create_for_tbm(esink->n_provider, esink->displaying_buffer[index].bo, esink->w, esink->h, 0,
						NATIVE_BUFFER_FORMAT_NV12);
					if(!esink->displaying_buffer[index].n_buffer)
					{
						GST_WARNING("there is no native buffer.. bo : %p,  gst_buf : %p", esink->displaying_buffer[index].bo, esink->displaying_buffer[index].buffer);
						g_mutex_unlock (instance_lock);
						return GST_FLOW_OK;
					}

					GST_WARNING("create native buffer : %p", esink->displaying_buffer[index].n_buffer);
				}
			}
			/* because it has same bo, use existing native buffer */
			else
			{
				if(index!=-1)
				{
					esink->displaying_buffer[index].buffer = buf;
					GST_DEBUG("existing native buffer %p.. bo %p,  gst_buf %p", esink->displaying_buffer[index].n_buffer, esink->displaying_buffer[index].bo, esink->displaying_buffer[index].buffer);
					exist_bo = FALSE;
				}
			}

			/* if it couldn't find proper index */
			if(index == -1)
				GST_WARNING("all spaces are using!!!");
			else
				GST_DEBUG("selected buffer index %d", index);

		}
		else if (scmn_imgb->buf_share_method == BUF_SHARE_METHOD_FLUSH_BUFFER)
		{
			/* flush buffer, we will copy last buffer to keep image data and reset buffer list */
			GST_WARNING_OBJECT(esink, "FLUSH_BUFFER: gst_buf= %p ts= %" GST_TIME_FORMAT " size= %lu,  mallocdata= %p",
			buf, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP (buf)), GST_BUFFER_SIZE (buf), GST_BUFFER_MALLOCDATA(buf));
			gst_buffer_ref (buf);
			if (gst_esink_make_flush_buffer(esink))
				GST_WARNING("made flush buffer");
			gst_buffer_unref (buf);
		}
		else
		{
			GST_ERROR("it is not TBM buffer.. %d", scmn_imgb->buf_share_method);
		}
	} else if(esink->native_buffer_format == NATIVE_BUFFER_FORMAT_I420) {
		static int skip_count_i420=0;
		if(esink->sent_buffer_cnt >= MAX_ECOREPIPE_BUFFER_CNT) {
			if(!(skip_count_i420++%20)) {
				GST_WARNING("[%d]EA buffer was already sent to ecore pipe, and %d frame skipped", esink->sent_buffer_cnt,skip_count_i420);
			}
			g_mutex_unlock (instance_lock);
			return GST_FLOW_OK;
		}

		if(!esink->is_buffer_allocated) {
			/* Allocate TBM buffer for non-zero copy case */
			if(!gst_evas_image_sink_allocate_source_buffer(esink)) {
				GST_ERROR_OBJECT(esink, "Buffer allocation failed");
				g_mutex_unlock (instance_lock);
				return GST_FLOW_ERROR;
			}
			esink->is_buffer_allocated = TRUE;
		}

		skip_count_i420 = 0; //for skip log in I420

		/* check whether bo is new or not */
		for(i=0; i<NATIVE_BUFFER_NUM; i++) {
			if(esink->displaying_buffer[i].bo == esink->src_buffer_info[esink->src_buf_idx].bo) {
				GST_DEBUG_OBJECT(esink, "it is already saved bo %p (index num : %d)", esink->displaying_buffer[i].bo, i);
				index = i;
				exist_bo = TRUE;
				break;
			}
			else
				exist_bo = FALSE;
		}

		/* keep bo */
		if(!exist_bo) {
			/* find empty buffer space for indexing */
			for(i=0; i<NATIVE_BUFFER_NUM; i++) {
				if(!esink->displaying_buffer[i].bo) {
					index = i;
					break;
				}
			}

			if(index!=-1) {
				/* keep informations */
				esink->displaying_buffer[index].bo = esink->src_buffer_info[esink->src_buf_idx].bo;
				esink->displaying_buffer[index].n_buffer = esink->src_buffer_info[esink->src_buf_idx].n_buffer;

				GST_DEBUG_OBJECT(esink, "gst_buf %p, TBM bo %p.. gst_buf vaddr %p .. src data size(%lu)",
					buf, esink->displaying_buffer[index].bo, GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf));

				memcpy(esink->src_buffer_info[esink->src_buf_idx].usr_addr, GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf));
			}
		} else {
			/* because it has same bo, use existing native buffer */
			if(index!=-1) {
				memcpy(esink->src_buffer_info[esink->src_buf_idx].usr_addr, GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf));

				GST_DEBUG_OBJECT(esink, "existing native buffer %p.. gst_buf %p, bo %p,  gst_buf vaddr %p",
					esink->displaying_buffer[index].n_buffer, buf, esink->displaying_buffer[index].bo, GST_BUFFER_DATA(buf));
				exist_bo = FALSE;
			}
		}

		/* if it couldn't find proper index */
		if(index == -1)
			GST_WARNING_OBJECT(esink, "all spaces are being used!!!");
		else
			GST_DEBUG_OBJECT(esink, "selected buffer index %d", index);

	} else {
		GST_ERROR_OBJECT(esink, "unsupported color format");
		g_mutex_unlock (instance_lock);
		return GST_FLOW_ERROR;
	}
#endif
	if (esink->object_show && index != -1) {
#ifdef USE_FIMCC
		gst_buffer_ref (buf);
		r = ecore_pipe_write (esink->epipe, &buf, sizeof (GstBuffer *));
		if (r == EINA_FALSE)  {
			//add error handling
			GST_WARNING("ecore_pipe_write fail");
		}
#endif
#ifdef USE_NATIVE_BUFFER
		int old_curidx=esink->cur_index ;
		static int skip_count=0;
		g_mutex_lock(esink->display_buffer_lock);

		if(esink->native_buffer_format == NATIVE_BUFFER_FORMAT_NV12 && scmn_imgb->buf_share_method != BUF_SHARE_METHOD_FLUSH_BUFFER) {
			if (esink->sent_buffer_cnt < MAX_ECOREPIPE_BUFFER_CNT) {
				GST_LOG("[show_frame] before refcount : %d .. gst_buf : %p", gst_evas_image_sink_ref_count (buf), buf);
				gst_buffer_ref (buf);
				esink->displaying_buffer[index].ref_count++;
				esink->cur_index = index;
				GST_LOG("index %d set refcount increase as %d", index,esink->displaying_buffer[index].ref_count);
				GST_LOG("[show_frame] after refcount : %d .. gst_buf : %p", gst_evas_image_sink_ref_count (buf), buf);

				/* Print debug log for 8 frame */
				if(esink->debuglog_cnt_showFrame > 0)
				{
					GST_WARNING("(%d) ecore_pipe_write index[%d]  gst_buf : %p", DEBUGLOG_DEFAULT_COUNT-(esink->debuglog_cnt_showFrame),
						esink->cur_index, esink->displaying_buffer[esink->cur_index].buffer);
					esink->debuglog_cnt_showFrame--;
				}

				esink->sent_buffer_cnt++;
				skip_count =0 ;

				r = ecore_pipe_write (esink->epipe, &esink->cur_index , SIZE_FOR_NATIVE_INDEX);

				if (r == EINA_FALSE)  {
					GST_LOG("[show_frame] before refcount : %d .. gst_buf : %p", gst_evas_image_sink_ref_count (buf), buf);
					esink->cur_index = old_curidx;
					if(esink->displaying_buffer[index].ref_count)
					{
						esink->displaying_buffer[index].ref_count--;
						esink->sent_buffer_cnt--;
						gst_buffer_unref(buf);
						GST_ERROR("finish unreffing");
					}
				}
			} else {
				/* If buffer count which is sent to ecore pipe, is upper 3, Print Error log */
				if(!(skip_count++%20)) {
					GST_WARNING("[%d]EA buffer was already sent to ecore pipe, and %d frame skipped", esink->sent_buffer_cnt,skip_count);
				}
			}
		} else if (esink->native_buffer_format == NATIVE_BUFFER_FORMAT_I420) {
			esink->cur_index = index;
			GST_LOG("index %d", index);
			GST_LOG("[show_frame] gst_buf vaddr %p", esink->src_buffer_info[esink->src_buf_idx].usr_addr);

			/* Print debug log for 8 frame */
			if(esink->debuglog_cnt_showFrame > 0)
			{
				GST_WARNING("(%d) ecore_pipe_write :  index[%d], bo[%p], n_buffer[%p], gst_buf[%p], gst_buf vaddr [%p]", DEBUGLOG_DEFAULT_COUNT-(esink->debuglog_cnt_showFrame),
					esink->cur_index, esink->displaying_buffer[index].bo, esink->displaying_buffer[index].n_buffer, buf, esink->src_buffer_info[esink->src_buf_idx].usr_addr);
				esink->debuglog_cnt_showFrame--;
			}

			esink->sent_buffer_cnt++;

			esink->src_buf_idx++;
		    r = ecore_pipe_write (esink->epipe, &esink->cur_index , SIZE_FOR_NATIVE_INDEX);

			if (r == EINA_FALSE)  {
				esink->cur_index = old_curidx;
				esink->sent_buffer_cnt--;
				esink->src_buf_idx--;
				GST_ERROR("ecore_pipe_write is failed. index[%d], bo[%p], n_buffer[%p], gst_buf vaddr [%p]",
					index, esink->displaying_buffer[index].bo, esink->displaying_buffer[index].n_buffer, esink->src_buffer_info[esink->src_buf_idx].usr_addr);
			}
			esink->src_buf_idx = esink->src_buf_idx % SOURCE_BUFFER_NUM;
		}

		g_mutex_unlock(esink->display_buffer_lock);
#endif
		GST_DEBUG ("ecore_pipe_write() was called with gst_buf= %p..  gst_buf Vaddr=%p, mallocdata= %p", buf, GST_BUFFER_DATA(buf), GST_BUFFER_MALLOCDATA(buf));
	} else {
		GST_WARNING ("skip ecore_pipe_write(). becuase of esink->object_show(%d) index(%d)", esink->object_show, index);
	}
	g_mutex_unlock (instance_lock);
	GST_DEBUG ("Leave");
	return GST_FLOW_OK;
}
#ifdef USE_NATIVE_BUFFER

static GstFlowReturn
gst_esink_epipe_reset(GstEvasImageSink *esink)
{
	if (esink == NULL) {
		GST_ERROR("handle is NULL");
		return GST_FLOW_ERROR;
	}

	if (esink->epipe) {
		GST_DEBUG("ecore-pipe will delete");
		ecore_pipe_del (esink->epipe);
		esink->epipe = NULL;
	}

	if (!esink->epipe) {
		esink->epipe = ecore_pipe_add ((Ecore_Pipe_Cb)evas_image_sink_cb_pipe, esink);
		if (!esink->epipe) {
			GST_ERROR ("ecore-pipe create failed");
			return GST_FLOW_ERROR;
		}
		GST_DEBUG("ecore-pipe create success");
	}

	return GST_FLOW_OK;
}


static gboolean gst_esink_make_flush_buffer(GstEvasImageSink *esink)
{
	GstEvasImageFlushBuffer *flush_buffer = NULL;
	GstEvasImageDisplayingBuffer *display_buffer = NULL;
	int size = 0;
	tbm_bo bo;
	tbm_bo_handle vaddr_src;
	tbm_bo_handle vaddr_dst;
	GstFlowReturn ret=GST_FLOW_OK;
	int src_size = 0;

	if (esink == NULL) {
		GST_ERROR("handle is NULL");
		return FALSE;
	}

	if (esink->cur_index == -1) {
		GST_WARNING_OBJECT(esink, "there is no remained buffer");
		return FALSE;
	}

	if(esink->flush_buffer)
		_release_flush_buffer(esink);

	/* malloc buffer */
	flush_buffer = (GstEvasImageFlushBuffer *)malloc(sizeof(GstEvasImageFlushBuffer));
	if (flush_buffer == NULL) {
		GST_ERROR_OBJECT(esink, "GstEvasImageFlushBuffer alloc failed");
		return FALSE;
	}

	memset(flush_buffer, 0x0, sizeof(GstEvasImageFlushBuffer));

	display_buffer = &(esink->displaying_buffer[esink->cur_index]);
	GST_WARNING_OBJECT(esink, "cur_index [%d]",
								  esink->cur_index);
	if(!display_buffer || !display_buffer->bo)
	{
		GST_WARNING("display_buffer(%p) or bo (%p) is NULL!!", display_buffer, display_buffer->bo);
		goto FLUSH_BUFFER_FAILED;
	}

	/* create native buffer provider for making native buffer */
	flush_buffer->n_provider = native_buffer_provider_create (NATIVE_BUFFER_PROVIDER_CORE);

	if(!flush_buffer->n_provider)
	{
		GST_ERROR("n_provider is NULL!!");
		goto FLUSH_BUFFER_FAILED;
	}

	/* create native buffer */
	flush_buffer->n_buffer = native_buffer_create(flush_buffer->n_provider, esink->w, esink->h, 0, NATIVE_BUFFER_FORMAT_NV12,
		NATIVE_BUFFER_USAGE_CPU|NATIVE_BUFFER_USAGE_3D_TEXTURE);
	if(!flush_buffer->n_buffer)
	{
		GST_ERROR("n_buffer is NULL!!");
		goto FLUSH_BUFFER_FAILED;
	}

	/* get bo and size */
	bo = native_buffer_get_bo(flush_buffer->n_buffer);
	size = tbm_bo_size(bo);
	if(!bo || !size)
	{
		GST_ERROR("bo(%p), size(%d)", bo, size);
		goto FLUSH_BUFFER_FAILED;
	}
	flush_buffer->bo = bo;

	src_size = display_buffer->buffer->size;

	vaddr_src = tbm_bo_map(display_buffer->bo, TBM_DEVICE_CPU, TBM_OPTION_READ|TBM_OPTION_WRITE);
	vaddr_dst = tbm_bo_map(bo, TBM_DEVICE_CPU, TBM_OPTION_READ|TBM_OPTION_WRITE);
	if (!vaddr_src.ptr || !vaddr_dst.ptr) {
		GST_WARNING_OBJECT(esink, "get vaddr failed src %p, dst %p", vaddr_src.ptr, vaddr_dst.ptr);
		if (vaddr_src.ptr) {
			tbm_bo_unmap(display_buffer->bo);
		}
		if (vaddr_dst.ptr) {
			tbm_bo_unmap(bo);
		}
		goto FLUSH_BUFFER_FAILED;
	} else {
		memset (vaddr_dst.ptr, 0x0, size);
		GST_WARNING_OBJECT (esink, "tbm_bo_map(VADDR) finished, bo(%p), vaddr(%p)", bo, vaddr_dst.ptr);
	}

	/* copy buffer */
	memcpy(vaddr_dst.ptr, vaddr_src.ptr, src_size);

	tbm_bo_unmap(display_buffer->bo);
	tbm_bo_unmap(bo);

	GST_WARNING_OBJECT(esink, "copy done.. native buffer : %p src_size : %d", flush_buffer->n_buffer, src_size);

	esink->flush_buffer = flush_buffer;

	/* initialize buffer list */
       if (esink->object_show)
               esink->need_flush = TRUE;

       ret = gst_esink_epipe_reset(esink);
       if(ret) {
              GST_ERROR_OBJECT(esink, "evas epipe reset ret=%d, need to check",ret);
              return FALSE;
       } else {
              GST_ERROR_OBJECT(esink, "evas epipe reset success");
       }

       gst_evas_image_sink_reset(esink);


	return TRUE;

FLUSH_BUFFER_FAILED:
	if (flush_buffer) {
		if(flush_buffer->n_buffer)
		{
			native_buffer_destroy(flush_buffer->n_buffer);
			flush_buffer->n_buffer = NULL;
		}
		if(flush_buffer->n_provider)
		{
			native_buffer_provider_destroy(flush_buffer->n_provider);
		}
		free(flush_buffer);
		flush_buffer = NULL;
	}
	return FALSE;
}

 static void _release_flush_buffer(GstEvasImageSink *esink)
{
	if (esink == NULL ||
	    esink->flush_buffer == NULL) {
		GST_WARNING("handle is NULL");
		return;
	}

	GST_WARNING_OBJECT(esink, "release FLUSH BUFFER start");
	if (esink->flush_buffer->bo) {
		esink->flush_buffer->bo = NULL;
	}
	if(esink->flush_buffer->n_buffer) {
		native_buffer_destroy(esink->flush_buffer->n_buffer);
		esink->flush_buffer->n_buffer = NULL;
	}
	if(esink->flush_buffer->n_provider) {
		native_buffer_provider_destroy(esink->flush_buffer->n_provider);
	}

	GST_WARNING_OBJECT(esink, "release FLUSH BUFFER done");

	free(esink->flush_buffer);
	esink->flush_buffer = NULL;

	return;
}

#endif
static gboolean
evas_image_sink_init (GstPlugin *evasimagesink)
{
	GST_DEBUG_CATEGORY_INIT (gst_evas_image_sink_debug, "evasimagesink", 0, "Evas image object based videosink");

	return gst_element_register (evasimagesink, "evasimagesink", GST_RANK_NONE, GST_TYPE_EVASIMAGESINK);
}

#ifndef PACKAGE
#define PACKAGE "gstevasimagesink-plugin-package"
#endif

GST_PLUGIN_DEFINE (
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	"evasimagesink",
	"Evas image object based videosink",
	evas_image_sink_init,
	VERSION,
	"LGPL",
	"Samsung Electronics Co",
	"http://www.samsung.com/"
)
