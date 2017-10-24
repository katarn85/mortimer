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

#ifndef __GST_EVASIMAGESINK_H__
#define __GST_EVASIMAGESINK_H__

//#define USE_NATIVE_BUFFER
#define USE_FIMCC

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <Evas.h>
#include <Ecore.h>
#ifdef USE_NATIVE_BUFFER
#include <native-buffer.h>
#include <tbm_bufmgr.h>
#endif
G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_EVASIMAGESINK \
  (gst_evas_image_sink_get_type())
#define GST_EVASIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_EVASIMAGESINK,GstEvasImageSink))
#define GST_EVASIMAGESINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_EVASIMAGESINK,GstEvasImageSinkClass))
#define GST_IS_EVASIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_EVASIMAGESINK))
#define GST_IS_EVASIMAGESINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_EVASIMAGESINK))

typedef struct _GstEvasImageSink      GstEvasImageSink;
typedef struct _GstEvasImageSinkClass GstEvasImageSinkClass;
#ifdef USE_NATIVE_BUFFER
typedef struct _GstEvasImageDisplayingBuffer GstEvasImageDisplayingBuffer;
typedef struct _GstEvasImageFlushBuffer GstEvasImageFlushBuffer;
typedef enum {
	BUF_SHARE_METHOD_NONE = -1,
	BUF_SHARE_METHOD_PADDR = 0,
	BUF_SHARE_METHOD_FD,
	BUF_SHARE_METHOD_TIZEN_BUFFER,
	BUF_SHARE_METHOD_FLUSH_BUFFER
} buf_share_method_t;

enum {
    DEGREE_0 = 0,
    DEGREE_90,
    DEGREE_180,
    DEGREE_270,
    DEGREE_NUM,
};

enum {
    DISP_GEO_METHOD_LETTER_BOX = 0,
    DISP_GEO_METHOD_ORIGIN_SIZE,
    DISP_GEO_METHOD_FULL_SCREEN,
    DISP_GEO_METHOD_CROPPED_FULL_SCREEN,
    DISP_GEO_METHOD_ORIGIN_SIZE_OR_LETTER_BOX,
    DISP_GEO_METHOD_CUSTOM_DST_ROI,
    DISP_GEO_METHOD_NUM,
};

enum {
	FLIP_NONE = 0,
	FLIP_HORIZONTAL,
	FLIP_VERTICAL,
	FLIP_BOTH,
	FLIP_NUM
};

struct buffer_info {
	uint64_t usr_addr;
	uint64_t size;
	void *bo;
	native_buffer_t *n_buffer;
	native_buffer_provider_t *n_provider;
};

/* _GstEvasDisplayingBuffer
 *
 * buffer		: manage ref count by got index through comparison
 * bo			: compare with buffer from codec for getting index
 * n_buffer		: compare with buffer from evas for getting index
 * ref_count	: decide whether it unref buffer or not in gst_evas_image_sink_fini/reset
 */
struct _GstEvasImageDisplayingBuffer {
	GstBuffer *buffer;
	void *bo;
	native_buffer_t *n_buffer;
	int ref_count;
};

struct _GstEvasImageFlushBuffer {
	void *bo;
	native_buffer_t *n_buffer;
	native_buffer_provider_t *n_provider;
};
#define NATIVE_BUFFER_NUM 20
#define SOURCE_BUFFER_NUM 8
#endif
struct _GstEvasImageSink
{
	GstVideoSink element;

	Evas_Object *eo;
	Ecore_Pipe *epipe;
	Evas_Coord w;
	Evas_Coord h;
	gboolean object_show;
	gchar update_visibility;
	gboolean gl_zerocopy;

	GstBuffer *oldbuf;

	gboolean is_evas_object_size_set;
	guint present_data_addr;
#ifdef USE_NATIVE_BUFFER
	GMutex *display_buffer_lock;
	GMutex *flow_lock;
	GstEvasImageDisplayingBuffer displaying_buffer[NATIVE_BUFFER_NUM];
	GstVideoRectangle eo_size;
	gboolean use_ratio;
	guint rotate_angle;
	guint display_geometry_method;
	guint flip;
	native_buffer_provider_t *n_provider;
	GstBuffer *prev_buf;
	gint prev_index;
	gint cur_index;
	gboolean need_flush;
	gboolean enable_flush_buffer;
	GstEvasImageFlushBuffer *flush_buffer;
	gint sent_buffer_cnt;
	gint debuglog_cnt_showFrame;
	gint debuglog_cnt_ecoreCbPipe;

	native_buffer_format_t native_buffer_format;
	struct buffer_info src_buffer_info[NATIVE_BUFFER_NUM];
	guint src_buf_idx;
	gboolean is_buffer_allocated;
#endif
};

struct _GstEvasImageSinkClass
{
	GstVideoSinkClass parent_class;
};

GType gst_evas_image_sink_get_type (void);

G_END_DECLS

#endif /* __GST_EVASIMAGESINK_H__ */
