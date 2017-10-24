/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
 */


#ifndef __GST_PIFFDEMUX_H__
#define __GST_PIFFDEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include "piffcommon.h"

#ifdef DRM_ENABLE
#include <drm_client.h>
#include <drm_trusted_client.h>
#include <drm_client_types.h>
#include <drm_trusted_client_types.h>
#endif

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN (piffdemux_debug);
#define GST_CAT_DEFAULT piffdemux_debug

#define GST_TYPE_PIFFDEMUX \
  (gst_piffdemux_get_type())
#define GST_PIFFDEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PIFFDEMUX,GstPiffDemux))
#define GST_PIFFDEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PIFFDEMUX,GstPiffDemuxClass))
#define GST_IS_PIFFDEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PIFFDEMUX))
#define GST_IS_PIFFDEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PIFFDEMUX))

#define GST_PIFFDEMUX_CAST(obj) ((GstPiffDemux *)(obj))

/* piffdemux produces these for atoms it cannot parse */
#define GST_PIFF_DEMUX_PRIVATE_TAG "private-piff-tag"
#define GST_PIFF_DEMUX_CLASSIFICATION_TAG "classification"

#define GST_PIFFDEMUX_MAX_STREAMS         8

typedef struct _GstPiffDemux GstPiffDemux;
typedef struct _GstPiffDemuxClass GstPiffDemuxClass;
typedef struct _PiffDemuxStream PiffDemuxStream;

struct _GstPiffDemux {
  GstElement element;

  /* pads */
  GstPad *sinkpad;
  GstPad *srcpad;

  PiffDemuxStream *stream;

  guint32 timescale;
  gint64 duration;

  gboolean fragmented;
  guint64 moof_offset;
  gint state;
  gboolean posted_redirect;

  /* push based variables */
  guint neededbytes;
  guint todrop;
  GstAdapter *adapter;
  GstBuffer *mdatbuffer;
  guint64 mdatleft;

  /* offset of the media data (i.e.: Size of header) */
  guint64 offset;
  /* offset of the mdat atom */
  guint64 mdatoffset;
  guint64 first_mdat;

  GstTagList *tag_list;

  /* configured playback region */
  GstSegment segment;
  gboolean segment_running;
  GstEvent *pending_newsegment;

  /* gst index support */
  GstIndex *element_index;
  gint index_id;

  gint64 requested_seek_time;
  guint64 seek_offset;
  gboolean moof_rcvd;

  /* live specific params */
  piff_live_param_t *param;
  guint lookahead_cnt;
  gboolean is_live;

  gboolean encrypt_content;
  gboolean decrypt_init;
#ifdef DRM_ENABLE
  DRM_DECRYPT_HANDLE pr_handle;
#endif
  GstBuffer *protection_header;
};

struct _GstPiffDemuxClass {
  GstElementClass parent_class;
  void (*live_param)   (GstPiffDemux *piff, const piff_live_param_t *param);
};

GType gst_piffdemux_get_type (void);

/* prepares video caps based on input params */
void gst_piffdemux_set_video_params (GstPiffDemux * piffdemux, guint fourcc, guint width, guint height,
                                     guint fps_n, guint fps_d, unsigned char *codec_data, unsigned int codec_data_len);
/* prepares audio caps based on input params */
void gst_piffdemux_set_audio_params (GstPiffDemux * piffdemux, guint fourcc, guint sampling_rate, guint bps,
                                     guint channels, unsigned char *codec_data, unsigned int codec_data_len);
G_END_DECLS

#endif /* __GST_PIFFDEMUX_H__ */

