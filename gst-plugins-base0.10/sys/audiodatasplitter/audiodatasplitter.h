/* GStreamer
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
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
 */

#ifndef __GST_AUDIODATA_SPLITTER_H__
#define __GST_AUDIODATA_SPLITTER_H__


#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_AUDIODATA_SPLITTER \
  (gst_audiodata_splitter_get_type())
#define GST_AUDIODATA_SPLITTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AUDIODATA_SPLITTER, GstAudioDataSplitter))
#define GST_AUDIODATA_SPLITTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AUDIODATA_SPLITTER, GstAudioDataSplitterClass))
#define GST_IS_AUDIODATA_SPLITTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AUDIODATA_SPLITTER))
#define GST_IS_AUDIODATA_SPLITTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AUDIODATA_SPLITTER))

typedef struct _GstAudioDataSplitter GstAudioDataSplitter;
typedef struct _GstAudioDataSplitterClass GstAudioDataSplitterClass;

/**
 * GstAudioDataSplitter:
 *
 * The #GstAudioDataSplitter data structure.
 */
struct _GstAudioDataSplitter {
  GstElement element;
  GstPad* sinkpad;
  GList* srcpads;
  GstPad *main_pcm_pad;
  GstPad *sub_pcm_pad;
  GstPad *spdif_pcm_pad;
  gboolean flush_event_sub;
  gboolean flush_event_spdif;

  gboolean newseg_update;
  gdouble newseg_rate;
  gdouble newseg_applied_rate;
  GstFormat newseg_format;
  gint64 newseg_start;
  gint64 newseg_stop;
  gint64 newseg_position;
  gboolean send_newseg_sub_pcm;
  gboolean send_newseg_spdif;
};

struct _GstAudioDataSplitterClass {
  GstElementClass parent_class;
};

GType gst_audiodata_splitter_get_type(void);

G_END_DECLS

#endif /* __GST_AUDIODATA_SPLITTER_H__ */
