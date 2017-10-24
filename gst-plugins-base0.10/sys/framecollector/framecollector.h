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

#ifndef __GST_FRAMECOLLECTOR_H__
#define __GST_FRAMECOLLECTOR_H__


#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>

#include <string.h>
#include <math.h>
#include <stdlib.h>

G_BEGIN_DECLS

#define GST_TYPE_FRAMECOLLECTOR \
  (gst_framecollector_get_type())
#define GST_FRAMECOLLECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_FRAMECOLLECTOR, GstFrameCollector))
#define GST_FRAMECOLLECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_FRAMECOLLECTOR, GstFrameCollectorClass))
#define GST_IS_FRAMECOLLECTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_FRAMECOLLECTOR))
#define GST_IS_FRAMECOLLECTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_FRAMECOLLECTOR))

typedef struct _GstFrameCollector GstFrameCollector;
typedef struct _GstFrameCollectorClass GstFrameCollectorClass;

/**
 * GstFrameCollector:
 *
 * The #GstFrameCollector data structure.
 */
struct _GstFrameCollector {
  GstElement element;
  GstCollectPads *collect;
  GstPadEventFunction  collect_event;
  GstPad* srcpad;
};

struct _GstFrameCollectorClass {
  GstElementClass parent_class;
};

GType gst_framecollector_get_type(void);

G_END_DECLS

#endif /* __GST_FRAMECOLLECTOR_H__ */
