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

#ifndef __GST_MIXERSINK_H__
#define __GST_MIXERSINK_H__

#define INHERIT_GSTBASESINK

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>


#include <string.h>
#include <math.h>
#include <stdlib.h>

G_BEGIN_DECLS

#define GST_TYPE_MIXERSINK \
  (gst_mixersink_get_type())
#define GST_MIXERSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_MIXERSINK, GstMixerSink))
#define GST_MIXERSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_MIXERSINK, GstMixerSinkClass))
#define GST_IS_MIXERSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_MIXERSINK))
#define GST_IS_MIXERSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_MIXERSINK))

typedef struct _GstMixerSink GstMixerSink;
typedef struct _GstMixerSinkClass GstMixerSinkClass;

/**
 * GstMIXERSINK:
 *
 * The #GstMIXERSINK data structure.
 */
struct _GstMixerSink {
  GstBaseSink sink;
  int device_fd;
  int mixerhandle;
  int mixerdisplayhandle;
};

struct _GstMixerSinkClass {
  GstBaseSinkClass parent_class;
};

GType gst_mixersink_get_type(void);

G_END_DECLS

#endif /* __GST_MIXERSINK_H__ */

