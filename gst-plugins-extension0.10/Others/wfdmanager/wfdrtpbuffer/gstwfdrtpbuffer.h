/*
 * Farsight Voice+Video library
 *
 *  Copyright 2007 Collabora Ltd, 
 *  Copyright 2007 Nokia Corporation
 *   @author: Philippe Kalaf <philippe.kalaf@collabora.co.uk>.
 *  Copyright 2007 Wim Taymans <wim.taymans@gmail.com>
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
 */

#ifndef __GST_WFD_RTP_BUFFER_H__
#define __GST_WFD_RTP_BUFFER_H__

#include <gst/gst.h>
#include "gstwfdrtpbuffer.h"

G_BEGIN_DECLS

/* #define's don't like whitespacey bits */
#define GST_TYPE_WFD_RTP_BUFFER \
  (gst_wfd_rtp_buffer_get_type())
#define GST_WFD_RTP_BUFFER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  GST_TYPE_WFD_RTP_BUFFER,GstWfdRTPBuffer))
#define GST_WFD_RTP_BUFFER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
  GST_TYPE_WFD_RTP_BUFFER,GstWfdRTPBufferClass))
#define GST_IS_WFD_RTP_BUFFER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WFD_RTP_BUFFER))
#define GST_IS_WFD_RTP_BUFFER_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WFD_RTP_BUFFER))

typedef struct _GstWfdRTPBuffer GstWfdRTPBuffer;
typedef struct _GstWfdRTPBufferClass GstWfdRTPBufferClass;
typedef struct _GstWfdRTPBufferPrivate GstWfdRTPBufferPrivate;

/**
 * GstWfdRTPBuffer:
 *
 * Opaque wfdrtpbuffer structure.
 */
struct _GstWfdRTPBuffer
{
  GstElement parent;

  /*< private >*/
  GstWfdRTPBufferPrivate *priv; /* FIXME: remove? */
};

struct _GstWfdRTPBufferClass
{
  GstElementClass parent_class;

  /* signals */
  GstCaps* (*request_pt_map)   (GstWfdRTPBuffer *buffer, guint pt);

  void     (*handle_sync)      (GstWfdRTPBuffer *buffer, GstStructure *s);
  void     (*on_npt_stop)      (GstWfdRTPBuffer *buffer);

  /* actions */
  void     (*clear_pt_map)   (GstWfdRTPBuffer *buffer);

  GstClockTime (*set_active)     (GstWfdRTPBuffer *buffer, gboolean active, guint64 elapsed);
};

GType gst_wfd_rtp_buffer_get_type (void);

G_END_DECLS

#endif /* __GST_WFD_RTP_BUFFER_H__ */
