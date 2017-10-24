/* GStreamer
 * Copyright (C) 2008 Wim Taymans <wim.taymans at gmail.com>

* Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
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

* 1. Applied Miracast WFD Server function
*/


#ifndef __RTSP_FUNNEL_H__
#define __RTSP_FUNNEL_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define RTSP_TYPE_FUNNEL \
  (rtsp_funnel_get_type ())
#define RTSP_FUNNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),RTSP_TYPE_FUNNEL,RTSPFunnel))
#define RTSP_FUNNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),RTSP_TYPE_FUNNEL,RTSPFunnelClass))
#define RTSP_IS_FUNNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),RTSP_TYPE_FUNNEL))
#define RTSP_IS_FUNNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),RTSP_TYPE_FUNNEL))

typedef struct _RTSPFunnel          RTSPFunnel;
typedef struct _RTSPFunnelClass     RTSPFunnelClass;

/**
 * RTSPFunnel:
 *
 * Opaque #RTSPFunnel data structure.
 */
struct _RTSPFunnel {
  GstElement      element;

  /*< private >*/
  GstPad         *srcpad;

  gboolean has_segment;
};

struct _RTSPFunnelClass {
  GstElementClass parent_class;
};

GType   rtsp_funnel_get_type        (void);

G_END_DECLS

#endif /* __RTSP_FUNNEL_H__ */
