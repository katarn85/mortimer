/*  GStreamer remote sink class
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

#ifndef __GST_REMOTE_SINK_H__
#define __GST_REMOTE_SINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

#define GST_TYPE_REMOTE_SINK (gst_remote_sink_get_type())
#define GST_REMOTE_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_REMOTE_SINK, GstRemoteSink))
#define GST_REMOTE_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_REMOTE_SINK, GstRemoteSinkClass))
#define GST_IS_REMOTE_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_REMOTE_SINK))
#define GST_IS_REMOTE_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_REMOTE_SINK))
#define GST_REMOTE_SINK_GET_CLASS(klass) \
  (G_TYPE_INSTANCE_GET_CLASS ((klass), GST_TYPE_REMOTE_SINK, GstRemoteSinkClass))

/**
 * GST_REMOTE_SINK_CAST:
 * @obj: a #GstRemoteSink or derived object
 *
 * Cast @obj to a #GstRemoteSink without runtime type check.
 *
 * Since: 0.10.12
 */
#define GST_REMOTE_SINK_CAST(obj)  ((GstRemoteSink *) (obj))

/**
 * GST_REMOTE_SINK_PAD:
 * @obj: a #GstRemoteSink
 *
 * Get the sink #GstPad of @obj.
 */
#define GST_REMOTE_SINK_PAD(obj) GST_BASE_SINK_PAD(obj)

typedef struct _GstRemoteSink GstRemoteSink;
typedef struct _GstRemoteSinkClass GstRemoteSinkClass;
typedef struct _GstRemoteSinkPrivate GstRemoteSinkPrivate;

/**
 * GstRemoteSink:
 *
 * The remote sink instance structure. 
 */
struct _GstRemoteSink {
  GstBaseSink element;
  GstRemoteSinkPrivate *priv;

  gpointer _gst_reserved[GST_PADDING - 1];
};

/**
 * GstRemoteSinkClass:
 * @parent_class: the parent class structure
 * @remote_render: remote render GstBuffer. Maps to #GstBaseSinkClass.render() and
 *     #GstBaseSinkClass.preroll() vfuncs.
 *
 * The remote sink class structure. Derived classes should override the
 * @remote_render virtual function.
 */
struct _GstRemoteSinkClass {
  GstBaseSinkClass parent_class;

  GstFlowReturn  (*remote_render) (GstRemoteSink *remote_sink, GstBuffer *buf);
  gboolean  (*remote_flush) (GstRemoteSink *remote_sink);
  gboolean  (*remote_mute) (GstRemoteSink *remote_sink);
  gboolean  (*remote_unmute) (GstRemoteSink *remote_sink);
  /*< private >*/
  gpointer _gst_reserved[GST_PADDING - 1];
};

typedef enum _GstRemoteSinkResponseResult {
	GST_REMOTE_SINK_RESPONSE_RESULT_SUCCESS,
	GST_REMOTE_SINK_RESPONSE_RESULT_ERROR,
	GST_REMOTE_SINK_RESPONSE_RESULT_UNKNOWN
}GstRemoteSinkResponseResult;

GType gst_remote_sink_get_type (void);

gboolean gst_remote_sink_remote_push(GstRemoteSink *remote_sink, gpointer buffer, gint push_size, gint *real_size);
gboolean gst_remote_sink_remote_pull(GstRemoteSink *remote_sink, gpointer buffer, gint pull_size, gint *real_size);

G_END_DECLS

#endif  /* __GST_REMOTE_SINK_H__ */
