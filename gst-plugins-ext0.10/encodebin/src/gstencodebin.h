/*
 * GStreamer encodebin
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>
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

#ifndef __GST_ENCODE_BIN_H__
#define __GST_ENCODE_BIN_H__

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include <gst/riff/riff-ids.h>

G_BEGIN_DECLS

#define GST_CAT_DEFAULT gst_encode_bin_debug

#define GST_TYPE_ENCODE_BIN_PROFILE (gst_encode_bin_profile_get_type())
GType gst_encode_bin_profile_get_type (void);

#define GST_TYPE_ENCODE_BIN             (gst_encode_bin_get_type())
#define GST_ENCODE_BIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ENCODE_BIN,GstEncodeBin))
#define GST_ENCODE_BIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ENCODE_BIN,GstEncodeBinClass))
#define GST_IS_ENCODE_BIN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ENCODE_BIN))
#define GST_IS_ENCODE_BIN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ENCODE_BIN))

#define GST_ENCODE_BIN_GET_LOCK(encodebin) (((GstEncodeBin*)encodebin)->mutex)
#define GST_ENCODE_BIN_LOCK(encodebin)     g_mutex_lock (GST_ENCODE_BIN_GET_LOCK(encodebin))
#define GST_ENCODE_BIN_UNLOCK(encodebin)   g_mutex_unlock (GST_ENCODE_BIN_GET_LOCK(encodebin))

/* Signal enable */
#define GST_ENCODE_BIN_SIGNAL_ENABLE

typedef struct _GstEncodeBinPad {
  GstCollectData *collect;

  gboolean is_video;
  gboolean connected;

  gchar *tag;

  gst_riff_strh hdr;
} GstEncodeBinPad;

typedef struct _GstEncodeBin GstEncodeBin;
typedef struct _GstEncodeBinClass GstEncodeBinClass;

struct _GstEncodeBin
{
  GstBin bin;                   /* we extend GstBin */

  GMutex *mutex;

  /* pads */
  GstPad *srcpad;
  GstPad *video_sinkpad;
  GstPad *audio_sinkpad;
  GstPad *image_sinkpad;
  GstPad *mux_audio_sinkpad;
  GstPad *mux_video_sinkpad;

  /* sinkpads, video first */
  GSList *sinkpads;

  /* video restricted to 1 pad */
  guint video_pads, audio_pads;

  gint profile;
  gint fps;
  gint high_speed_fps;
  gint multiple;
  gchar *venc_name;
  gchar *aenc_name;
  gchar *ienc_name;
  gchar *mux_name;
  gchar *vconv_name;

  GstCaps *vcaps;
  GstCaps *acaps;
  GstCaps *icaps;

  gboolean auto_audio_convert;
  gboolean auto_audio_resample;
  gboolean auto_color_space;
  gboolean block;
  gboolean pause;
  gboolean use_video_toggle;
  gboolean use_venc_queue;
  gboolean use_aenc_queue;

  GstElement *audio_queue;
  GstElement *video_queue;
  GstElement *video_encode_queue;
  GstElement *audio_encode_queue;
  GstElement *image_queue;

  GstElement *audio_encode;
  GstElement *video_encode;
  GstElement *image_encode;

  GstElement *vcapsfilter;
  GstElement *acapsfilter;
  GstElement *icapsfilter;

  GstElement *video_toggle;
  GstElement *image_toggle;
  GstElement *color_space;
  GstElement *audio_conv;
  GstElement *audio_sample;

  GstElement *mux;

  /* pause/resume variables */
  GstClockTime paused_time;             /* pipeline time when pausing */
  GstClockTime total_offset_time;       /* delayed time which is due to pause */
  gulong vsink_probeid;
  gulong vsink_hs_probeid;
  gulong asink_probeid;
  gulong veque_sig_id;
  gulong aeque_sig_id;

  /* mux enable flag */
  gboolean enable_mux;
};

struct _GstEncodeBinClass
{
  GstBinClass parent_class;

#ifdef GST_ENCODE_BIN_SIGNAL_ENABLE
  /* signal we fire when stream block/pause function called */
  void (*stream_block) (GstElement * element, gboolean result);  
  void (*stream_unblock) (GstElement * element, gboolean result);  
  void (*stream_pause) (GstElement * element, gboolean result);
  void (*stream_resume) (GstElement * element, gboolean result);
#endif

};

GType gst_encode_bin_get_type (void);

G_END_DECLS


#endif /* __GST_ENCODE_BIN_H__ */
