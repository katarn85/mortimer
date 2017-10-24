/*
 * audiotp
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

#ifndef __GST_AUDIOTP_H__
#define __GST_AUDIOTP_H__

#include <gst/gst.h>
#include <stdlib.h>
#include <string.h>

G_BEGIN_DECLS

#define GST_TYPE_AUDIOTP             (gst_audiotp_get_type())
#define GST_AUDIOTP(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIOTP,Gstaudiotp))
#define GST_AUDIOTP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIOTP,GstaudiotpClass))
#define GST_AUDIOTP_GET_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS((klass),GST_TYPE_AUDIOTP,GstaudiotpClass))
#define GST_IS_AUDIOTP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIOTP))
#define GST_IS_AUDIOTP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUDIOTP))
#define GST_AUDIOTP_CAST(obj)        ((Gstaudiotp *)(obj))

typedef struct _Gstaudiotp Gstaudiotp;
typedef struct _GstaudiotpClass GstaudiotpClass;

struct _Gstaudiotp
{
  GstElement element;
  GstPad *sinkpad;
  GstPad *srcpad;
  GQueue *reverse; /* used in reverse trickplay */
  GstSegment segment;

  /* Flag to indicate the new buffer recieved is discountinued in
  its time-stamp */
  gboolean discont;
  gboolean is_reversed;
  GstClockTime head_prev;
  GstClockTime tail_prev;
};

struct _GstaudiotpClass
{
  GstElementClass parent_class;
};

GType gst_audiotp_get_type (void);

G_END_DECLS

#endif /* __GST_AUDIOTP_H__ */