/*  GStreamer remote audio sink class
 *  Copyright (C) <2014> Liu Yang(yang010.liu@samsung.com)
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

/* FIXME 0.11: turn this into a proper base class */

#ifndef __GST_REMOTE_AUDIO_SINK_H__
#define __GST_REMOTE_AUDIO_SINK_H__

#include "gstremotesink.h"

G_BEGIN_DECLS

#define GST_TYPE_REMOTE_AUDIO_SINK (gst_remote_audio_sink_get_type())
#define GST_REMOTE_AUDIO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_REMOTE_AUDIO_SINK, GstRemoteAudioSink))
#define GST_REMOTE_AUDIO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_REMOTE_AUDIO_SINK, GstRemoteAudioSinkClass))
#define GST_IS_REMOTE_AUDIO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_REMOTE_AUDIO_SINK))
#define GST_IS_REMOTE_AUDIO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_REMOTE_AUDIO_SINK))
#define GST_REMOTE_AUDIO_SINK_GET_CLASS(klass) \
  (G_TYPE_INSTANCE_GET_CLASS ((klass), GST_TYPE_REMOTE_AUDIO_SINK, GstRemoteAudioSinkClass))

/**
 * GST_REMOTE_AUDIO_SINK_CAST:
 * @obj: a #GstRemoteAudioSink or derived object
 *
 * Cast @obj to a #GstRemoteAudioSink without runtime type check.
 *
 * Since: 0.10.12
 */
#define GST_REMOTE_AUDIO_SINK_CAST(obj)  ((GstRemoteAudioSink *) (obj))

/**
 * GST_REMOTE_AUDIO_SINK_PAD:
 * @obj: a #GstRemoteAudioSink
 *
 * Get the sink #GstPad of @obj.
 */
#define GST_REMOTE_AUDIO_SINK_PAD(obj) GST_REMOTE_SINK_PAD(obj)

typedef struct _GstRemoteAudioSink GstRemoteAudioSink;
typedef struct _GstRemoteAudioSinkClass GstRemoteAudioSinkClass;
typedef struct _GstRemoteAudioSinkPrivate GstRemoteAudioSinkPrivate;

/**
 * GstRemoteAudioSink:
 *
 * The remote audio sink instance structure. 
 */
struct _GstRemoteAudioSink {
  GstRemoteSink element;
  GstRemoteAudioSinkPrivate *priv;

  gpointer _gst_reserved[GST_PADDING - 1];
};

/**
 * GstRemoteAudioSinkClass:
 * @parent_class: the parent class structure
 */
struct _GstRemoteAudioSinkClass {
  GstRemoteSinkClass parent_class;
  
  /*< private >*/
  gpointer _gst_reserved[GST_PADDING - 1];
};

GType gst_remote_audio_sink_get_type (void);


G_END_DECLS

#endif  /* __GST_REMOTE_AUDIO_SINK_H__ */

